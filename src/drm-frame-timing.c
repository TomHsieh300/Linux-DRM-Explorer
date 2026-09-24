#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* ============================================================
 * DRM Frame Timing Measurement (Quantifying VBlank Sync)
 *
 * Experiment 09 showed *qualitatively* that vblank-synchronised page
 * flips do not tear.  This demo turns that statement into numbers:
 * every atomic page flip returns a DRM_EVENT_FLIP_COMPLETE event that
 * carries the kernel's vblank sequence number and a timestamp.  By
 * recording those values for N flips we can measure
 *
 *   - the real refresh period (flip-to-flip interval) and its jitter,
 *   - how many vblanks were missed (sequence gaps > 1),
 *   - how long a commit takes to reach the screen (commit -> event),
 *   - how late userspace wakes up after the vblank (vblank -> read()).
 *
 * Progression from previous demos:
 *   drm-vblank-sync-demo.c     -- legacy PageFlip, tearing vs. vsync
 *   drm-atomic-demo.c          -- atomic commit, properties, planes
 *   drm-dmabuf-fence.c         -- DMA-BUF sharing, implicit/explicit fence
 *   drm-frame-timing.c         -- measuring the flip events themselves
 *
 * Runnable modes:
 *   (default)          600 back-to-back atomic flips, then a report
 *   -n <frames>        number of flips to measure
 *   --load-ms <ms>     simulate <ms> of CPU "render" work per frame
 *   --csv <file>       dump every raw sample for offline plotting
 *   Ctrl+C             stop early; the summary so far is printed and
 *                      the previous CRTC configuration is restored
 *
 * Target: RK3588 / VOP2 with Ubuntu Lite (no compositor).
 * Not yet verified on hardware.
 * ============================================================ */

#define NUM_BUFFERS       2
#define DEFAULT_FRAMES    600
#define MAX_FRAMES        1000000
#define MAX_SAVED_CONNS   8
#define EBUSY_MAX_RETRY   100
#define STRIPE_WIDTH      16
#define STRIPE_STEP       8

/* ============================================================
 * Property ID cache -- same pattern as drm-atomic-demo.c
 * ============================================================ */
struct plane_props {
	uint32_t fb_id;
	uint32_t crtc_id;
	uint32_t crtc_x, crtc_y, crtc_w, crtc_h;
	uint32_t src_x,  src_y,  src_w,  src_h;
};

struct crtc_props {
	uint32_t active;
	uint32_t mode_id;
};

struct connector_props {
	uint32_t crtc_id;
};

struct kms_state {
	int fd;

	uint32_t conn_id;
	uint32_t crtc_id;
	uint32_t crtc_idx;
	uint32_t plane_id;

	drmModeModeInfo mode;
	uint32_t mode_blob_id;

	struct connector_props conn_props;
	struct crtc_props      crtc_props;
	struct plane_props     primary_props;

	/*
	 * What was on the CRTC before we took it over.  drmModeGetCrtc()
	 * returns the legacy view (fb, x/y, mode); we also remember which
	 * connectors were routed to the CRTC so that restoring does not
	 * accidentally move a different output.
	 */
	drmModeCrtc *saved_crtc;
	uint32_t     saved_conns[MAX_SAVED_CONNS];
	int          saved_conn_count;
	bool         modeset_done;
};

struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint64_t size;
	uint8_t  *vaddr;
	uint32_t fb_id;
	int      stripe_x;   /* last stripe drawn into this buffer, -1 = none */
};

/* ============================================================
 * One sample per page flip
 *
 * Two clocks appear here and they must not be confused:
 *
 *   vbl_us     -- written by the KERNEL into struct drm_event_vblank
 *                 (tv_sec/tv_usec).  It is the timestamp of the vblank
 *                 in which the flip completed.
 *   *_ns       -- read by US with clock_gettime(CLOCK_MONOTONIC).
 *
 * Subtracting one from the other is only meaningful if the kernel
 * timestamp is also CLOCK_MONOTONIC -> DRM_CAP_TIMESTAMP_MONOTONIC.
 * ============================================================ */
struct sample {
	uint32_t seq;         /* drm_event_vblank.sequence                 */
	uint32_t seq_delta;   /* seq - previous seq (0 for the first flip) */
	uint32_t crtc_id;     /* drm_event_vblank.crtc_id (0 if unknown)   */
	uint32_t ebusy;       /* -EBUSY retries needed for this commit     */
	uint64_t vbl_us;      /* kernel vblank timestamp, microseconds     */
	uint64_t submit_ns;   /* CLOCK_MONOTONIC just before the commit    */
	uint64_t ioctl_ns;    /* duration of the nonblocking commit ioctl  */
	uint64_t recv_ns;     /* CLOCK_MONOTONIC when the event was read   */
	uint64_t work_ns;     /* previous event -> this submit ("render")  */
};

struct flip_ctx {
	bool           pending;
	struct sample *cur;
};

struct options {
	const char *device;
	const char *csv_path;
	size_t      frames;
	double      load_ms;
};

/*
 * Nominal timing derived from the mode, plus what the kernel told us
 * about its timestamps.
 */
struct timing_info {
	double period_us;     /* from clock / htotal / vtotal            */
	double refresh_hz;
	bool   monotonic;     /* DRM_CAP_TIMESTAMP_MONOTONIC == 1         */
	bool   crtc_in_event; /* DRM_CAP_CRTC_IN_VBLANK_EVENT == 1        */
};

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static uint64_t now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ============================================================
 * Small statistics helpers
 *
 * The Makefile links only -ldrm, so we avoid <math.h> (sqrt would
 * need -lm on some toolchains) and use a Newton iteration instead.
 * ============================================================ */
struct stat_summary {
	size_t n;
	double mean, min, max, sd;
};

static double newton_sqrt(double x)
{
	if (x <= 0.0)
		return 0.0;
	double r = x > 1.0 ? x : 1.0;
	for (int i = 0; i < 100; i++) {
		double next = 0.5 * (r + x / r);
		if (next == r)
			break;
		r = next;
	}
	return r;
}

static struct stat_summary summarize(const double *v, size_t n)
{
	struct stat_summary s = { .n = n };

	if (n == 0)
		return s;

	s.min = s.max = v[0];
	double sum = 0.0;
	for (size_t i = 0; i < n; i++) {
		sum += v[i];
		if (v[i] < s.min) s.min = v[i];
		if (v[i] > s.max) s.max = v[i];
	}
	s.mean = sum / (double)n;

	double var = 0.0;
	for (size_t i = 0; i < n; i++) {
		double d = v[i] - s.mean;
		var += d * d;
	}
	/* Population standard deviation: we describe this run, not a model */
	s.sd = newton_sqrt(var / (double)n);
	return s;
}

static int cmp_double(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;
	return (x > y) - (x < y);
}

/* Nearest-rank percentile on an already sorted array */
static double percentile_sorted(const double *sorted, size_t n, double pct)
{
	if (n == 0)
		return 0.0;
	size_t idx = (size_t)(pct / 100.0 * (double)(n - 1) + 0.5);
	if (idx >= n)
		idx = n - 1;
	return sorted[idx];
}

static void print_stats_line(const char *label, const double *v, size_t n,
			     const char *unit, bool with_percentiles)
{
	if (n == 0) {
		printf("  %-28s (no samples)\n", label);
		return;
	}

	struct stat_summary s = summarize(v, n);
	printf("  %-28s n=%zu  mean=%.3f  min=%.3f  max=%.3f  stddev=%.3f %s\n",
	       label, s.n, s.mean, s.min, s.max, s.sd, unit);

	if (!with_percentiles)
		return;

	double *sorted = malloc(n * sizeof(*sorted));
	if (!sorted)
		return;
	memcpy(sorted, v, n * sizeof(*sorted));
	qsort(sorted, n, sizeof(*sorted), cmp_double);
	printf("  %-28s p50=%.3f  p90=%.3f  p99=%.3f %s\n", "",
	       percentile_sorted(sorted, n, 50.0),
	       percentile_sorted(sorted, n, 90.0),
	       percentile_sorted(sorted, n, 99.0), unit);
	free(sorted);
}

/* ============================================================
 * print_histogram - text histogram with under/overflow bins
 * @lo:     lower edge of the first bin
 * @width:  bin width (same unit as the values)
 * @nbins:  number of regular bins
 * @scale:  divide bin edges by this for display (e.g. 1000 -> ms)
 * ============================================================ */
static void print_histogram(const char *title, const double *v, size_t n,
			    double lo, double width, int nbins,
			    double scale, const char *unit)
{
	if (n == 0 || nbins <= 0 || width <= 0.0)
		return;

	size_t *bins = calloc((size_t)nbins + 2, sizeof(*bins));
	if (!bins)
		return;

	/* bins[0] = underflow, bins[1..nbins] = regular, bins[nbins+1] = overflow */
	for (size_t i = 0; i < n; i++) {
		double pos = (v[i] - lo) / width;
		if (pos < 0.0)
			bins[0]++;
		else if (pos >= (double)nbins)
			bins[nbins + 1]++;
		else
			bins[1 + (int)pos]++;
	}

	size_t peak = 0;
	int first = nbins + 1, last = 0;
	for (int b = 0; b < nbins + 2; b++) {
		if (bins[b] > peak)
			peak = bins[b];
		if (bins[b] && b < first)
			first = b;
		if (bins[b])
			last = b;
	}

	/* Print only from the first to the last non-empty bin */
	printf("\n  %s\n", title);
	for (int b = first; b <= last; b++) {
		char range[64];

		if (b == 0) {
			if (!bins[b]) continue;
			snprintf(range, sizeof(range), "        < %9.3f",
				 lo / scale);
		} else if (b == nbins + 1) {
			if (!bins[b]) continue;
			snprintf(range, sizeof(range), "       >= %9.3f",
				 (lo + width * nbins) / scale);
		} else {
			snprintf(range, sizeof(range), "%9.3f .. %9.3f",
				 (lo + width * (b - 1)) / scale,
				 (lo + width * b) / scale);
		}

		int bar = peak ? (int)((bins[b] * 50 + peak - 1) / peak) : 0;
		printf("  %s %-2s |%-50.*s| %zu\n", range, unit, bar,
		       "##################################################",
		       bins[b]);
	}
	free(bins);
}

/* ============================================================
 * nominal_period_us - refresh period implied by the mode timings
 *
 * Same arithmetic as Experiment 03, turned around:
 *
 *   refresh = PCLK / (H_total * V_total)
 *   period  = (H_total * V_total) / PCLK
 *
 * drmModeModeInfo.clock is in kHz.  The interlace / doublescan /
 * vscan adjustments mirror drm_mode_vrefresh() in drm_modes.c so the
 * result corresponds to one vblank (one field for interlaced modes).
 *
 * Note that mode.vrefresh is an integer rounded by the kernel
 * (DIV_ROUND_CLOSEST), so it hides small offsets from e.g. 60 Hz.
 * ============================================================ */
static double nominal_period_us(const drmModeModeInfo *m)
{
	double num = (double)m->clock;               /* kHz */
	double den = (double)m->htotal * (double)m->vtotal;

	if (num <= 0.0 || den <= 0.0)
		return 0.0;
	if (m->flags & DRM_MODE_FLAG_INTERLACE)
		num *= 2.0;
	if (m->flags & DRM_MODE_FLAG_DBLSCAN)
		den *= 2.0;
	if (m->vscan > 1)
		den *= (double)m->vscan;

	/* den pixels / (num * 1000 pixels/s) seconds -> * 1e6 for us */
	return den * 1000.0 / num;
}

/* ============================================================
 * Report
 * ============================================================ */
static void print_report(const struct sample *s, size_t n,
			 const struct timing_info *ti,
			 const drmModeModeInfo *mode, double load_ms)
{
	printf("\n============================================================\n");
	printf(" Frame timing summary: %zu flips measured\n", n);
	printf("============================================================\n");
	printf("Mode %ux%u: clock=%u kHz  htotal=%u  vtotal=%u  (mode.vrefresh=%u)\n",
	       mode->hdisplay, mode->vdisplay, mode->clock,
	       mode->htotal, mode->vtotal, mode->vrefresh);
	printf("Nominal period from mode: %.3f us  (%.5f Hz)\n",
	       ti->period_us, ti->refresh_hz);
	printf("Simulated render load   : %.3f ms per frame\n", load_ms);
	printf("Kernel timestamps       : %s\n",
	       ti->monotonic ? "CLOCK_MONOTONIC (comparable with userspace)"
			     : "CLOCK_REALTIME (cross-clock latencies disabled)");

	if (n < 2) {
		printf("\nNeed at least 2 flips for interval statistics.\n");
		return;
	}

	size_t m = n - 1;
	double *interval   = malloc(m * sizeof(double));
	double *per_vbl    = malloc(m * sizeof(double));
	double *jitter     = malloc(m * sizeof(double));
	double *commit_evt = malloc(n * sizeof(double));
	double *commit_vbl = malloc(n * sizeof(double));
	double *vbl_wake   = malloc(n * sizeof(double));
	double *work       = malloc(n * sizeof(double));
	double *ioctl_t    = malloc(n * sizeof(double));

	if (!interval || !per_vbl || !jitter || !commit_evt || !commit_vbl ||
	    !vbl_wake || !work || !ioctl_t) {
		fprintf(stderr, "report: out of memory\n");
		goto out;
	}

	/* ---------- Flip-to-flip intervals (kernel timestamps) ---------- */
	uint64_t missed = 0, gaps = 0, backwards = 0;
	uint64_t delta_hist[5] = {0};   /* 0,1,2,3,>=4 */
	size_t nper = 0;

	for (size_t i = 1; i < n; i++) {
		uint32_t d = s[i].seq_delta;
		double iv = (double)(int64_t)(s[i].vbl_us - s[i - 1].vbl_us);

		interval[i - 1] = iv;
		delta_hist[d < 4 ? d : 4]++;
		if (d > 1) {
			missed += d - 1;
			gaps++;
		}
		if (d == 0 || iv <= 0.0) {
			backwards++;
			continue;
		}
		per_vbl[nper] = iv / (double)d;
		jitter[nper]  = iv - (double)d * ti->period_us;
		nper++;
	}

	printf("\n[1] Flip-to-flip interval (kernel vblank timestamps)\n");
	print_stats_line("interval", interval, m, "us", true);
	{
		struct stat_summary is = summarize(interval, m);
		double span_us = (double)(s[n - 1].vbl_us - s[0].vbl_us);
		if (is.mean > 0.0)
			printf("  effective flip rate          %.4f Hz (1e6 / mean interval)\n",
			       1e6 / is.mean);
		if (span_us > 0.0)
			printf("  span first->last flip        %.3f ms for %zu intervals\n",
			       span_us / 1000.0, m);
	}

	/*
	 * Bins are 1/4 period wide and start at period/8, so every whole
	 * multiple of the period sits in the *centre* of a bin instead of on
	 * a bin edge (where tiny jitter would split one peak into two).
	 */
	print_histogram("Interval histogram (bin = 1/4 nominal period, centred on multiples), ms:",
			interval, m, ti->period_us / 8.0, ti->period_us / 4.0,
			18, 1000.0, "ms");

	printf("\n[2] VBlank sequence analysis (drm_event_vblank.sequence)\n");
	printf("  first seq=%u  last seq=%u  vblanks elapsed=%u  flips=%zu\n",
	       s[0].seq, s[n - 1].seq, s[n - 1].seq - s[0].seq, n);
	printf("  seq delta = 1 : %" PRIu64 "  (flip landed on the very next vblank)\n",
	       delta_hist[1]);
	printf("  seq delta = 2 : %" PRIu64 "\n", delta_hist[2]);
	printf("  seq delta = 3 : %" PRIu64 "\n", delta_hist[3]);
	printf("  seq delta >= 4: %" PRIu64 "\n", delta_hist[4]);
	if (delta_hist[0])
		printf("  seq delta = 0 : %" PRIu64 "  (unexpected: two events for one vblank?)\n",
		       delta_hist[0]);
	printf("  missed vblanks (sum of delta-1): %" PRIu64 "  in %" PRIu64
	       " late flips (%.2f%% of intervals)\n",
	       missed, gaps, 100.0 * (double)gaps / (double)m);
	if (backwards)
		printf("  WARNING: %" PRIu64 " intervals had delta 0 or a non-positive "
		       "timestamp step; excluded from per-vblank stats\n", backwards);

	printf("\n[3] Per-vblank period = interval / seq delta\n");
	print_stats_line("period", per_vbl, nper, "us", false);
	if (nper && ti->period_us > 0.0) {
		struct stat_summary ps = summarize(per_vbl, nper);
		printf("  deviation of mean from nominal: %+.3f us (%+.1f ppm)\n",
		       ps.mean - ti->period_us,
		       (ps.mean - ti->period_us) / ti->period_us * 1e6);
	}
	print_histogram("Jitter histogram: interval - delta * nominal period, us:",
			jitter, nper, -100.0, 10.0, 20, 1.0, "us");

	/* ---------- Latencies ---------- */
	size_t nl = 0;
	for (size_t i = 0; i < n; i++) {
		commit_evt[i] = (double)(s[i].recv_ns - s[i].submit_ns) / 1000.0;
		work[i]       = (double)s[i].work_ns / 1000.0;
		ioctl_t[i]    = (double)s[i].ioctl_ns / 1000.0;
		if (ti->monotonic) {
			double vbl_ns = (double)s[i].vbl_us * 1000.0;
			commit_vbl[nl] = (vbl_ns - (double)s[i].submit_ns) / 1000.0;
			vbl_wake[nl]   = ((double)s[i].recv_ns - vbl_ns) / 1000.0;
			nl++;
		}
	}

	printf("\n[4] Latency (userspace CLOCK_MONOTONIC)\n");
	print_stats_line("commit -> event received", commit_evt, n, "us", true);
	print_histogram("Commit -> event histogram (bin = 1 ms):",
			commit_evt, n, 0.0, 1000.0,
			(int)(ti->period_us * 3.0 / 1000.0) + 1, 1000.0, "ms");
	if (nl) {
		print_stats_line("commit -> vblank timestamp", commit_vbl, nl, "us", true);
		print_stats_line("vblank ts -> event received", vbl_wake, nl, "us", true);
	} else {
		printf("  (kernel timestamps are not CLOCK_MONOTONIC: skipping\n"
		       "   commit->vblank and vblank->wakeup, they would mix clocks)\n");
	}
	print_stats_line("frame work (event->submit)", work, n, "us", false);
	print_stats_line("nonblocking commit ioctl", ioctl_t, n, "us", false);

	uint64_t ebusy = 0;
	for (size_t i = 0; i < n; i++)
		ebusy += s[i].ebusy;
	printf("  EBUSY retries                %" PRIu64 "\n", ebusy);

out:
	free(interval); free(per_vbl); free(jitter);
	free(commit_evt); free(commit_vbl); free(vbl_wake);
	free(work); free(ioctl_t);
}

static int write_csv(const char *path, const struct sample *s, size_t n)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "fopen %s: %s\n", path, strerror(errno));
		return -1;
	}

	fprintf(f, "index,sequence,seq_delta,crtc_id,vblank_ts_us,"
		   "submit_mono_ns,recv_mono_ns,interval_us,"
		   "submit_to_recv_us,work_us,ioctl_us,ebusy_retries\n");
	for (size_t i = 0; i < n; i++) {
		long long iv = i ? (long long)(s[i].vbl_us - s[i - 1].vbl_us) : 0;
		fprintf(f, "%zu,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64
			   ",%lld,%.3f,%.3f,%.3f,%u\n",
			i, s[i].seq, s[i].seq_delta, s[i].crtc_id, s[i].vbl_us,
			s[i].submit_ns, s[i].recv_ns, iv,
			(double)(s[i].recv_ns - s[i].submit_ns) / 1000.0,
			(double)s[i].work_ns / 1000.0,
			(double)s[i].ioctl_ns / 1000.0, s[i].ebusy);
	}

	if (fclose(f)) {
		fprintf(stderr, "fclose %s: %s\n", path, strerror(errno));
		return -1;
	}
	printf("Wrote %zu samples to %s\n", n, path);
	return 0;
}

/* ============================================================
 * Property discovery helpers (see drm-atomic-demo.c for the lesson)
 * ============================================================ */
static int get_property_id(int fd, drmModeObjectProperties *props,
			   const char *name, uint32_t *id_out)
{
	for (uint32_t i = 0; i < props->count_props; i++) {
		drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
		if (!prop)
			continue;
		if (strcmp(prop->name, name) == 0) {
			*id_out = prop->prop_id;
			drmModeFreeProperty(prop);
			return 0;
		}
		drmModeFreeProperty(prop);
	}
	fprintf(stderr, "Property \"%s\" not found\n", name);
	return -1;
}

static int cache_props(struct kms_state *kms)
{
	drmModeObjectProperties *p;
	int ret = 0;

	p = drmModeObjectGetProperties(kms->fd, kms->conn_id,
				       DRM_MODE_OBJECT_CONNECTOR);
	if (!p) return -1;
	ret |= get_property_id(kms->fd, p, "CRTC_ID", &kms->conn_props.crtc_id);
	drmModeFreeObjectProperties(p);

	p = drmModeObjectGetProperties(kms->fd, kms->crtc_id, DRM_MODE_OBJECT_CRTC);
	if (!p) return -1;
	ret |= get_property_id(kms->fd, p, "ACTIVE",  &kms->crtc_props.active);
	ret |= get_property_id(kms->fd, p, "MODE_ID", &kms->crtc_props.mode_id);
	drmModeFreeObjectProperties(p);

	p = drmModeObjectGetProperties(kms->fd, kms->plane_id, DRM_MODE_OBJECT_PLANE);
	if (!p) return -1;
	struct plane_props *pp = &kms->primary_props;
	ret |= get_property_id(kms->fd, p, "FB_ID",   &pp->fb_id);
	ret |= get_property_id(kms->fd, p, "CRTC_ID", &pp->crtc_id);
	ret |= get_property_id(kms->fd, p, "CRTC_X",  &pp->crtc_x);
	ret |= get_property_id(kms->fd, p, "CRTC_Y",  &pp->crtc_y);
	ret |= get_property_id(kms->fd, p, "CRTC_W",  &pp->crtc_w);
	ret |= get_property_id(kms->fd, p, "CRTC_H",  &pp->crtc_h);
	ret |= get_property_id(kms->fd, p, "SRC_X",   &pp->src_x);
	ret |= get_property_id(kms->fd, p, "SRC_Y",   &pp->src_y);
	ret |= get_property_id(kms->fd, p, "SRC_W",   &pp->src_w);
	ret |= get_property_id(kms->fd, p, "SRC_H",   &pp->src_h);
	drmModeFreeObjectProperties(p);

	return ret ? -1 : 0;
}

/* ============================================================
 * Pipeline discovery: connector -> CRTC -> primary plane
 *
 * Prefer the CRTC that already drives the connector (it keeps the
 * current routing, e.g. VP3 for the DSI panel) and the primary plane
 * already attached to it.  Nothing here is hard-coded.
 * ============================================================ */
static int find_pipeline(struct kms_state *kms, drmModeRes *res)
{
	drmModeConnector *conn = NULL;

	for (int i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(kms->fd, res->connectors[i]);
		if (conn && conn->connection == DRM_MODE_CONNECTED &&
		    conn->count_modes > 0)
			break;
		drmModeFreeConnector(conn);
		conn = NULL;
	}
	if (!conn) {
		fprintf(stderr, "No connected connector with modes\n");
		return -1;
	}

	kms->conn_id = conn->connector_id;
	kms->mode = conn->modes[0];
	for (int i = 0; i < conn->count_modes; i++) {
		if (conn->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
			kms->mode = conn->modes[i];
			break;
		}
	}

	/* 1) CRTC currently routed to this connector, if any */
	uint32_t want_crtc = 0;
	if (conn->encoder_id) {
		drmModeEncoder *enc = drmModeGetEncoder(kms->fd, conn->encoder_id);
		if (enc) {
			want_crtc = enc->crtc_id;
			drmModeFreeEncoder(enc);
		}
	}
	for (int i = 0; want_crtc && i < res->count_crtcs; i++) {
		if (res->crtcs[i] == want_crtc) {
			kms->crtc_id  = want_crtc;
			kms->crtc_idx = (uint32_t)i;
		}
	}

	/* 2) otherwise the first CRTC any of its encoders can reach */
	for (int e = 0; !kms->crtc_id && e < conn->count_encoders; e++) {
		drmModeEncoder *enc = drmModeGetEncoder(kms->fd, conn->encoders[e]);
		if (!enc)
			continue;
		for (int i = 0; i < res->count_crtcs; i++) {
			if (enc->possible_crtcs & (1u << i)) {
				kms->crtc_id  = res->crtcs[i];
				kms->crtc_idx = (uint32_t)i;
				break;
			}
		}
		drmModeFreeEncoder(enc);
	}
	drmModeFreeConnector(conn);

	if (!kms->crtc_id) {
		fprintf(stderr, "No usable CRTC for connector %u\n", kms->conn_id);
		return -1;
	}

	/* Primary plane for that CRTC */
	drmModePlaneRes *pres = drmModeGetPlaneResources(kms->fd);
	if (!pres) {
		fprintf(stderr, "drmModeGetPlaneResources: %s\n", strerror(errno));
		return -1;
	}
	for (uint32_t i = 0; i < pres->count_planes; i++) {
		drmModePlane *pl = drmModeGetPlane(kms->fd, pres->planes[i]);
		if (!pl)
			continue;
		if (!(pl->possible_crtcs & (1u << kms->crtc_idx))) {
			drmModeFreePlane(pl);
			continue;
		}

		drmModeObjectProperties *p =
			drmModeObjectGetProperties(kms->fd, pl->plane_id,
						   DRM_MODE_OBJECT_PLANE);
		uint64_t type = DRM_PLANE_TYPE_OVERLAY;
		for (uint32_t k = 0; p && k < p->count_props; k++) {
			drmModePropertyRes *pr = drmModeGetProperty(kms->fd, p->props[k]);
			if (!pr)
				continue;
			if (strcmp(pr->name, "type") == 0)
				type = p->prop_values[k];
			drmModeFreeProperty(pr);
		}
		if (p)
			drmModeFreeObjectProperties(p);

		if (type == DRM_PLANE_TYPE_PRIMARY) {
			if (pl->crtc_id == kms->crtc_id || !kms->plane_id)
				kms->plane_id = pl->plane_id;
		}
		drmModeFreePlane(pl);
	}
	drmModeFreePlaneResources(pres);

	if (!kms->plane_id) {
		fprintf(stderr, "No primary plane for CRTC %u\n", kms->crtc_id);
		return -1;
	}
	return 0;
}

/* Remember which connectors currently feed from our CRTC (for restore) */
static void save_crtc_state(struct kms_state *kms, drmModeRes *res)
{
	kms->saved_crtc = drmModeGetCrtc(kms->fd, kms->crtc_id);
	kms->saved_conn_count = 0;

	for (int i = 0; i < res->count_connectors; i++) {
		drmModeConnector *c = drmModeGetConnector(kms->fd, res->connectors[i]);
		if (!c)
			continue;
		if (c->encoder_id) {
			drmModeEncoder *enc = drmModeGetEncoder(kms->fd, c->encoder_id);
			if (enc && enc->crtc_id == kms->crtc_id &&
			    kms->saved_conn_count < MAX_SAVED_CONNS)
				kms->saved_conns[kms->saved_conn_count++] = c->connector_id;
			if (enc)
				drmModeFreeEncoder(enc);
		}
		drmModeFreeConnector(c);
	}
}

/* ============================================================
 * Dumb buffers (unchanged idea from previous demos)
 * ============================================================ */
static void destroy_fb(int fd, struct buffer_object *bo)
{
	if (bo->vaddr && bo->vaddr != MAP_FAILED)
		munmap(bo->vaddr, bo->size);
	if (bo->fb_id)
		drmModeRmFB(fd, bo->fb_id);
	if (bo->handle) {
		struct drm_mode_destroy_dumb d = { .handle = bo->handle };
		drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
	}
	memset(bo, 0, sizeof(*bo));
}

static int create_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_create_dumb create = {
		.width  = bo->width,
		.height = bo->height,
		.bpp    = 32,
	};
	struct drm_mode_map_dumb map = {0};

	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
		fprintf(stderr, "CREATE_DUMB: %s\n", strerror(errno));
		return -1;
	}
	bo->pitch  = create.pitch;
	bo->size   = create.size;
	bo->handle = create.handle;

	int ret = drmModeAddFB(fd, bo->width, bo->height, 24, 32,
			       bo->pitch, bo->handle, &bo->fb_id);
	if (ret) {
		fprintf(stderr, "drmModeAddFB: %s\n", strerror(-ret));
		bo->fb_id = 0;
		goto fail;
	}

	map.handle = bo->handle;
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
		fprintf(stderr, "MAP_DUMB: %s\n", strerror(errno));
		goto fail;
	}
	bo->vaddr = mmap(NULL, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED,
			 fd, map.offset);
	if (bo->vaddr == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		bo->vaddr = NULL;
		goto fail;
	}

	/* Dark grey background, written once; per-frame drawing is tiny */
	uint32_t *px = (uint32_t *)bo->vaddr;
	for (uint32_t y = 0; y < bo->height; y++)
		for (uint32_t x = 0; x < bo->width; x++)
			px[y * (bo->pitch / 4) + x] = 0x202020;
	bo->stripe_x = -1;
	return 0;

fail:
	destroy_fb(fd, bo);
	return -1;
}

static void fill_column(struct buffer_object *bo, int x0, uint32_t color)
{
	uint32_t *px = (uint32_t *)bo->vaddr;
	uint32_t stride = bo->pitch / 4;

	for (uint32_t y = 0; y < bo->height; y++)
		for (int x = x0; x < x0 + STRIPE_WIDTH && x < (int)bo->width; x++)
			px[y * stride + (uint32_t)x] = color;
}

/*
 * Deliberately cheap "rendering": erase the stripe this buffer showed
 * two frames ago and draw a new one.  Repainting the whole 1024x600
 * buffer every frame would add CPU time we are not trying to measure;
 * --load-ms is the explicit, controllable knob for render cost.
 * A moving stripe also makes dropped frames visible as stutter.
 */
static void draw_frame(struct buffer_object *bo, size_t frame)
{
	int span = (int)bo->width - STRIPE_WIDTH;
	if (span <= 0)
		return;

	if (bo->stripe_x >= 0)
		fill_column(bo, bo->stripe_x, 0x202020);

	/* Triangle wave across the screen */
	int pos = (int)((frame * STRIPE_STEP) % (size_t)(2 * span));
	if (pos > span)
		pos = 2 * span - pos;

	fill_column(bo, pos, 0xffffff);
	bo->stripe_x = pos;
}

/* ============================================================
 * atomic_modeset - initial configuration (blocking, TEST_ONLY first)
 * ============================================================ */
static int atomic_modeset(struct kms_state *kms, uint32_t fb_id)
{
	int ret = drmModeCreatePropertyBlob(kms->fd, &kms->mode, sizeof(kms->mode),
					    &kms->mode_blob_id);
	if (ret) {
		fprintf(stderr, "drmModeCreatePropertyBlob: %s\n", strerror(-ret));
		kms->mode_blob_id = 0;
		return -1;
	}

	drmModeAtomicReq *req = drmModeAtomicAlloc();
	if (!req)
		return -1;

	const struct plane_props *pp = &kms->primary_props;
	uint32_t w = kms->mode.hdisplay, h = kms->mode.vdisplay;

	drmModeAtomicAddProperty(req, kms->conn_id, kms->conn_props.crtc_id, kms->crtc_id);
	drmModeAtomicAddProperty(req, kms->crtc_id, kms->crtc_props.active, 1);
	drmModeAtomicAddProperty(req, kms->crtc_id, kms->crtc_props.mode_id, kms->mode_blob_id);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->fb_id,   fb_id);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->crtc_id, kms->crtc_id);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->crtc_x, 0);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->crtc_y, 0);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->crtc_w, w);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->crtc_h, h);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->src_x, 0);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->src_y, 0);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->src_w, (uint64_t)w << 16);
	drmModeAtomicAddProperty(req, kms->plane_id, pp->src_h, (uint64_t)h << 16);

	ret = drmModeAtomicCommit(kms->fd, req,
				  DRM_MODE_ATOMIC_TEST_ONLY |
				  DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	if (ret) {
		fprintf(stderr, "Atomic TEST_ONLY (modeset) failed: %s\n", strerror(-ret));
		drmModeAtomicFree(req);
		return -1;
	}

	/* Blocking commit: returns once the new state is on screen */
	ret = drmModeAtomicCommit(kms->fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	drmModeAtomicFree(req);
	if (ret) {
		fprintf(stderr, "Atomic modeset failed: %s\n", strerror(-ret));
		return -1;
	}
	kms->modeset_done = true;
	return 0;
}

/* ============================================================
 * Page flip event handlers
 *
 * drmHandleEvent() read()s struct drm_event_vblank records from the
 * fd and dispatches DRM_EVENT_FLIP_COMPLETE to:
 *   - page_flip_handler2 if evctx->version >= 3 (gets crtc_id), else
 *   - page_flip_handler  if evctx->version >= 2.
 * page_flip_handler2 / version 3 only exist in newer libdrm headers,
 * so fall back at compile time when the header is older.
 *
 * We take the userspace timestamp first thing in the handler; the
 * interesting delay (vblank IRQ -> process wakes up -> read()) has
 * already happened by then, which is exactly what we want to measure.
 * ============================================================ */
static void record_flip(unsigned int sequence, unsigned int tv_sec,
			unsigned int tv_usec, unsigned int crtc_id,
			void *user_data)
{
	uint64_t t = now_ns();
	struct flip_ctx *ctx = user_data;

	if (!ctx || !ctx->pending || !ctx->cur)
		return;

	ctx->cur->recv_ns = t;
	ctx->cur->seq     = sequence;
	ctx->cur->vbl_us  = (uint64_t)tv_sec * 1000000ull + tv_usec;
	ctx->cur->crtc_id = crtc_id;
	ctx->pending = false;
}

#if defined(DRM_EVENT_CONTEXT_VERSION) && DRM_EVENT_CONTEXT_VERSION >= 3
static void flip_handler2(int fd, unsigned int sequence, unsigned int tv_sec,
			  unsigned int tv_usec, unsigned int crtc_id,
			  void *user_data)
{
	(void)fd;
	record_flip(sequence, tv_sec, tv_usec, crtc_id, user_data);
}
#else
static void flip_handler(int fd, unsigned int sequence, unsigned int tv_sec,
			 unsigned int tv_usec, void *user_data)
{
	(void)fd;
	record_flip(sequence, tv_sec, tv_usec, 0, user_data);
}
#endif

static void init_event_context(drmEventContext *ev)
{
	memset(ev, 0, sizeof(*ev));
#if defined(DRM_EVENT_CONTEXT_VERSION) && DRM_EVENT_CONTEXT_VERSION >= 3
	/*
	 * Ask for version 3 explicitly rather than DRM_EVENT_CONTEXT_VERSION:
	 * we only fill the fields that exist up to v3.
	 */
	ev->version = 3;
	ev->page_flip_handler2 = flip_handler2;
#else
	ev->version = 2;
	ev->page_flip_handler = flip_handler;
#endif
}

/* ============================================================
 * submit_flip - one nonblocking atomic page flip with an event
 *
 * DRM_MODE_ATOMIC_NONBLOCK: the ioctl returns as soon as the commit is
 *   queued; the hardware update happens at the next vblank.
 * DRM_MODE_PAGE_FLIP_EVENT: the kernel sends DRM_EVENT_FLIP_COMPLETE
 *   when the new FB is actually being scanned out.
 *
 * -EBUSY: with the atomic helpers, a nonblocking commit is refused
 * while the previous commit on the same CRTC/plane has not signalled
 * flip_done ("Userspace is not allowed to get ahead of the previous
 * commit with nonblocking ones", drm_atomic_helper.c:stall_checks()).
 * Our loop waits for each event before submitting, so EBUSY should
 * not happen; if it does we count it and retry after 1 ms.
 * ============================================================ */
static drmModeAtomicReq *build_flip_req(struct kms_state *kms, uint32_t fb_id)
{
	drmModeAtomicReq *req = drmModeAtomicAlloc();
	if (!req)
		return NULL;

	/* Minimal flip: only the primary plane's FB changes */
	drmModeAtomicAddProperty(req, kms->plane_id, kms->primary_props.fb_id, fb_id);
	drmModeAtomicAddProperty(req, kms->plane_id, kms->primary_props.crtc_id,
				 kms->crtc_id);
	return req;
}

/*
 * Validate the flip request once before the loop.  Note that
 * DRM_MODE_PAGE_FLIP_EVENT must NOT be combined with TEST_ONLY: the
 * kernel rejects that combination with -EINVAL (drm_mode_atomic_ioctl).
 */
static int test_flip(struct kms_state *kms, uint32_t fb_id)
{
	drmModeAtomicReq *req = build_flip_req(kms, fb_id);
	if (!req)
		return -ENOMEM;

	int ret = drmModeAtomicCommit(kms->fd, req,
				      DRM_MODE_ATOMIC_TEST_ONLY |
				      DRM_MODE_ATOMIC_NONBLOCK, NULL);
	drmModeAtomicFree(req);
	if (ret)
		fprintf(stderr, "Atomic TEST_ONLY (flip) failed: %s\n", strerror(-ret));
	return ret;
}

static int submit_flip(struct kms_state *kms, uint32_t fb_id,
		       struct flip_ctx *ctx, struct sample *s)
{
	drmModeAtomicReq *req = build_flip_req(kms, fb_id);
	if (!req)
		return -ENOMEM;

	int ret;
	for (;;) {
		s->submit_ns = now_ns();
		ret = drmModeAtomicCommit(kms->fd, req,
					  DRM_MODE_ATOMIC_NONBLOCK |
					  DRM_MODE_PAGE_FLIP_EVENT, ctx);
		s->ioctl_ns = now_ns() - s->submit_ns;

		if (ret != -EBUSY || s->ebusy >= EBUSY_MAX_RETRY)
			break;
		s->ebusy++;
		struct timespec ms = { .tv_sec = 0, .tv_nsec = 1000000 };
		nanosleep(&ms, NULL);
	}
	drmModeAtomicFree(req);

	if (ret)
		fprintf(stderr, "Nonblocking atomic flip failed: %s\n", strerror(-ret));
	return ret;
}

/*
 * Block until the pending flip's event has been handled.  A signal
 * (Ctrl+C) interrupts select() with EINTR; we keep waiting because a
 * flip is in flight and must complete before we tear things down.
 */
static int wait_for_flip(int fd, drmEventContext *ev, struct flip_ctx *ctx)
{
	while (ctx->pending) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

		int r = select(fd + 1, &fds, NULL, NULL, &tv);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "select: %s\n", strerror(errno));
			return -1;
		}
		if (r == 0) {
			fprintf(stderr, "Timed out (1 s) waiting for flip event\n");
			return -1;
		}
		if (drmHandleEvent(fd, ev) != 0) {
			fprintf(stderr, "drmHandleEvent failed\n");
			return -1;
		}
	}
	return 0;
}

static void busy_wait_until(uint64_t deadline_ns)
{
	/* Pure CPU spin: models a render loop that owns the core */
	while (now_ns() < deadline_ns && !g_stop)
		;
}

/* ============================================================
 * run_measurement - the flip loop
 *
 *   modeset shows bufs[0]
 *   loop:
 *     t0 = time the previous flip event was received
 *     draw into back buffer, spin until t0 + load
 *     submit nonblocking flip (+event), note submit time
 *     wait for FLIP_COMPLETE, note seq / kernel ts / receive time
 *     swap front/back
 *
 * With --load-ms 0 each flip should land on the next vblank
 * (seq delta 1).  When the work no longer fits in one period the
 * commit misses that vblank and lands on a later one: seq delta 2,
 * interval ~2 periods, i.e. the frame rate halves.
 * ============================================================ */
static size_t run_measurement(struct kms_state *kms,
			      struct buffer_object bufs[NUM_BUFFERS],
			      const struct options *o, struct sample *samples)
{
	struct flip_ctx ctx = {0};
	drmEventContext ev;
	int front = 0;
	size_t n = 0;
	uint64_t load_ns = (uint64_t)(o->load_ms * 1e6);
	uint64_t work_start = now_ns();

	init_event_context(&ev);
	if (test_flip(kms, bufs[1].fb_id))
		return 0;

	printf("\nMeasuring %zu flips (load %.3f ms/frame) -- Ctrl+C to stop early\n",
	       o->frames, o->load_ms);

	while (n < o->frames && !g_stop) {
		int back = 1 - front;
		struct sample *s = &samples[n];

		memset(s, 0, sizeof(*s));
		draw_frame(&bufs[back], n);
		if (load_ns)
			busy_wait_until(work_start + load_ns);
		if (g_stop)
			break;
		s->work_ns = now_ns() - work_start;

		ctx.cur = s;
		ctx.pending = true;
		if (submit_flip(kms, bufs[back].fb_id, &ctx, s)) {
			ctx.pending = false;
			break;
		}
		if (wait_for_flip(kms->fd, &ev, &ctx))
			break;

		if (n > 0)
			s->seq_delta = s->seq - samples[n - 1].seq; /* u32 wrap-safe */
		work_start = s->recv_ns;
		front = back;
		n++;

		if (n % 120 == 0)
			printf("  %zu flips, last seq=%u\n", n, s->seq);
	}

	if (g_stop)
		printf("\nInterrupted after %zu flips\n", n);
	return n;
}

/* ============================================================
 * restore_crtc - put back what was on the CRTC before we started
 *
 * If the CRTC was active (e.g. fbcon), re-apply its fb/mode with the
 * legacy SetCrtc (atomic drivers implement it via
 * drm_atomic_helper_set_config).  If it was off, disable it again
 * with an atomic commit.
 * ============================================================ */
static void restore_crtc(struct kms_state *kms)
{
	if (!kms->modeset_done)
		return;

	drmModeCrtc *sc = kms->saved_crtc;
	if (sc && sc->mode_valid && sc->buffer_id && kms->saved_conn_count > 0) {
		int ret = drmModeSetCrtc(kms->fd, sc->crtc_id, sc->buffer_id,
					 sc->x, sc->y, kms->saved_conns,
					 kms->saved_conn_count, &sc->mode);
		if (ret)
			fprintf(stderr, "Restoring CRTC %u failed: %s\n",
				sc->crtc_id, strerror(-ret));
		else
			printf("Restored previous configuration of CRTC %u (fb %u)\n",
			       sc->crtc_id, sc->buffer_id);
		return;
	}

	drmModeAtomicReq *req = drmModeAtomicAlloc();
	if (!req)
		return;
	drmModeAtomicAddProperty(req, kms->plane_id, kms->primary_props.fb_id, 0);
	drmModeAtomicAddProperty(req, kms->plane_id, kms->primary_props.crtc_id, 0);
	drmModeAtomicAddProperty(req, kms->conn_id, kms->conn_props.crtc_id, 0);
	drmModeAtomicAddProperty(req, kms->crtc_id, kms->crtc_props.mode_id, 0);
	drmModeAtomicAddProperty(req, kms->crtc_id, kms->crtc_props.active, 0);
	int ret = drmModeAtomicCommit(kms->fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	drmModeAtomicFree(req);
	if (ret)
		fprintf(stderr, "Disabling CRTC %u failed: %s\n",
			kms->crtc_id, strerror(-ret));
	else
		printf("CRTC %u was off before the test; disabled it again\n",
		       kms->crtc_id);
}

/* ============================================================
 * Command line
 * ============================================================ */
static void usage(const char *prog)
{
	printf("Usage: %s [options]\n"
	       "\n"
	       "Measure atomic page-flip timing: vblank sequence gaps, flip-to-flip\n"
	       "interval vs. the nominal refresh period, and commit->event latency.\n"
	       "\n"
	       "Options:\n"
	       "  -d, --device <path>   DRM device (default /dev/dri/card0)\n"
	       "  -n, --frames <N>      number of flips to measure (default %d, max %d)\n"
	       "      --load-ms <ms>    busy-loop this long per frame to simulate\n"
	       "                        render work (default 0; fractions allowed)\n"
	       "      --csv <file>      write every raw sample to <file>\n"
	       "  -h, --help            show this help\n"
	       "\n"
	       "Ctrl+C prints the summary collected so far and restores the CRTC.\n"
	       "Needs DRM master: run from a VT with no compositor, usually via sudo.\n",
	       prog, DEFAULT_FRAMES, MAX_FRAMES);
}

/* Returns 0 = run, 1 = exit success (--help), -1 = usage error */
static int parse_args(int argc, char **argv, struct options *o)
{
	static const struct option longopts[] = {
		{ "device",  required_argument, NULL, 'd' },
		{ "frames",  required_argument, NULL, 'n' },
		{ "load-ms", required_argument, NULL, 'L' },
		{ "csv",     required_argument, NULL, 'C' },
		{ "help",    no_argument,       NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};

	o->device   = "/dev/dri/card0";
	o->csv_path = NULL;
	o->frames   = DEFAULT_FRAMES;
	o->load_ms  = 0.0;

	int c;
	while ((c = getopt_long(argc, argv, "d:n:h", longopts, NULL)) != -1) {
		char *end = NULL;

		switch (c) {
		case 'd':
			o->device = optarg;
			break;
		case 'n': {
			errno = 0;
			unsigned long long v = strtoull(optarg, &end, 10);
			if (errno || !end || *end || optarg[0] == '-' ||
			    v < 1 || v > MAX_FRAMES) {
				fprintf(stderr, "Invalid frame count '%s' (1..%d)\n",
					optarg, MAX_FRAMES);
				return -1;
			}
			o->frames = (size_t)v;
			break;
		}
		case 'L':
			errno = 0;
			o->load_ms = strtod(optarg, &end);
			if (errno || !end || *end || !(o->load_ms >= 0.0) ||
			    o->load_ms > 10000.0) {
				fprintf(stderr, "Invalid --load-ms '%s' (0..10000)\n", optarg);
				return -1;
			}
			break;
		case 'C':
			o->csv_path = optarg;
			break;
		case 'h':
			usage(argv[0]);
			return 1;
		default:
			usage(argv[0]);
			return -1;
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Unexpected argument '%s'\n", argv[optind]);
		usage(argv[0]);
		return -1;
	}
	return 0;
}

/* ============================================================
 * main
 * ============================================================ */
int main(int argc, char **argv)
{
	struct options opt;
	int pr = parse_args(argc, argv, &opt);
	if (pr > 0)
		return 0;
	if (pr < 0)
		return 2;

	printf("DRM Frame Timing Measurement\n");

	int status = 1;
	struct kms_state kms = { .fd = -1 };
	struct buffer_object bufs[NUM_BUFFERS] = {0};
	struct sample *samples = NULL;
	drmModeRes *res = NULL;
	size_t n = 0;

	kms.fd = open(opt.device, O_RDWR | O_CLOEXEC);
	if (kms.fd < 0) {
		fprintf(stderr, "open %s: %s\n", opt.device, strerror(errno));
		return 1;
	}

	if (drmSetClientCap(kms.fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) ||
	    drmSetClientCap(kms.fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
		fprintf(stderr, "Atomic / universal planes not supported: %s\n",
			strerror(errno));
		goto out;
	}

	/*
	 * DRM_CAP_TIMESTAMP_MONOTONIC tells us which clock the kernel uses
	 * for drm_event_vblank.tv_sec/tv_usec.  Only if it is
	 * CLOCK_MONOTONIC can we subtract our own clock_gettime() values.
	 * Per the UAPI header the cap is always 1 since Linux 4.15, but
	 * checking costs nothing and documents the assumption.
	 */
	struct timing_info ti = {0};
	uint64_t cap = 0;
	if (drmGetCap(kms.fd, DRM_CAP_TIMESTAMP_MONOTONIC, &cap) == 0)
		ti.monotonic = (cap == 1);
	else
		fprintf(stderr, "drmGetCap(TIMESTAMP_MONOTONIC) failed: %s\n",
			strerror(errno));
	cap = 0;
	if (drmGetCap(kms.fd, DRM_CAP_CRTC_IN_VBLANK_EVENT, &cap) == 0)
		ti.crtc_in_event = (cap == 1);
	printf("DRM_CAP_TIMESTAMP_MONOTONIC=%d  DRM_CAP_CRTC_IN_VBLANK_EVENT=%d\n",
	       ti.monotonic, ti.crtc_in_event);
#if defined(DRM_EVENT_CONTEXT_VERSION) && DRM_EVENT_CONTEXT_VERSION >= 3
	printf("libdrm event context: version 3 (page_flip_handler2)\n");
#else
	printf("libdrm event context: version 2 (page_flip_handler, no crtc_id)\n");
#endif

	res = drmModeGetResources(kms.fd);
	if (!res) {
		fprintf(stderr, "drmModeGetResources: %s\n", strerror(errno));
		goto out;
	}
	if (find_pipeline(&kms, res) || cache_props(&kms))
		goto out;

	ti.period_us  = nominal_period_us(&kms.mode);
	ti.refresh_hz = ti.period_us > 0.0 ? 1e6 / ti.period_us : 0.0;
	printf("Connector %u -> CRTC %u (index %u) -> primary plane %u\n",
	       kms.conn_id, kms.crtc_id, kms.crtc_idx, kms.plane_id);
	printf("Mode %s: %ux%u  clock=%u kHz  htotal=%u  vtotal=%u\n",
	       kms.mode.name, kms.mode.hdisplay, kms.mode.vdisplay,
	       kms.mode.clock, kms.mode.htotal, kms.mode.vtotal);
	printf("Nominal period = htotal*vtotal/clock = %.3f us -> %.5f Hz "
	       "(mode.vrefresh=%u)\n", ti.period_us, ti.refresh_hz,
	       kms.mode.vrefresh);

	samples = calloc(opt.frames, sizeof(*samples));
	if (!samples) {
		fprintf(stderr, "Cannot allocate %zu samples\n", opt.frames);
		goto out;
	}

	for (int i = 0; i < NUM_BUFFERS; i++) {
		bufs[i].width  = kms.mode.hdisplay;
		bufs[i].height = kms.mode.vdisplay;
		if (create_fb(kms.fd, &bufs[i]))
			goto out;
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;   /* no SA_RESTART: let select() return */
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	save_crtc_state(&kms, res);
	if (atomic_modeset(&kms, bufs[0].fb_id))
		goto out;

	n = run_measurement(&kms, bufs, &opt, samples);

	print_report(samples, n, &ti, &kms.mode, opt.load_ms);
	if (opt.csv_path && n > 0 && write_csv(opt.csv_path, samples, n))
		goto out;
	status = 0;

out:
	restore_crtc(&kms);
	for (int i = 0; i < NUM_BUFFERS; i++)
		destroy_fb(kms.fd, &bufs[i]);
	if (kms.mode_blob_id)
		drmModeDestroyPropertyBlob(kms.fd, kms.mode_blob_id);
	if (kms.saved_crtc)
		drmModeFreeCrtc(kms.saved_crtc);
	if (res)
		drmModeFreeResources(res);
	free(samples);
	close(kms.fd);
	return status;
}
