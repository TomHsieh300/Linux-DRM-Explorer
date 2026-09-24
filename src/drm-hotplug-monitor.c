#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <linux/netlink.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* ============================================================
 * DRM Multi-Display Topology & Hotplug Monitor
 *
 * This program makes the KMS "routing graph" visible and watches it
 * change when a display is plugged or unplugged.  It is READ-ONLY:
 * it never performs a modeset, never creates a framebuffer and never
 * touches a CRTC.  It only queries objects and listens for uevents.
 *
 * Progression from previous demos:
 *   01_Hardware_Inventory      -- mentioned the possible_crtcs mask
 *   drm-atomic-demo.c          -- picked "the first CRTC that fits"
 *   drm-dmabuf-fence.c         -- one pipeline, one display
 *   drm-hotplug-monitor.c      -- the whole routing matrix for ALL
 *                                 displays + reacting to hotplug
 *
 * Runnable modes:
 *   --topology   (default) Print connectors -> encoders -> CRTCs and
 *                planes -> CRTCs, then compute one valid
 *                connector-to-CRTC assignment (multi-display bring-up)
 *   --monitor    Listen for kernel hotplug uevents on a raw
 *                NETLINK_KOBJECT_UEVENT socket (no libudev), re-probe
 *                the connectors and print what changed
 *
 * Target: RK3588 / VOP2 with Ubuntu Lite (no compositor).
 * Not yet verified on hardware.
 * ============================================================ */

#define MAX_CRTCS   32  /* possible_crtcs is a 32-bit mask            */
#define MAX_OBJS    64  /* connectors / encoders / planes we track     */
#define MAX_CONN_ENCODERS 16
#define UEVENT_RX_BUF 8192 /* kernel env buffer is 2048 bytes (kobject.h) */

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

/* ============================================================
 * Name tables
 *
 * The strings mirror the kernel's drm_connector_enum_list[] and
 * drm_encoder_enum_list[] so that "HDMI-A-1" printed here matches the
 * name the kernel uses in dmesg and in /sys/class/drm/card0-HDMI-A-1.
 * libdrm has drmModeGetConnectorTypeName(), but it is a recent
 * addition, so we keep our own table for portability.
 * ============================================================ */
static const char *connector_type_name(uint32_t type)
{
	switch (type) {
	case DRM_MODE_CONNECTOR_VGA:         return "VGA";
	case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
	case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
	case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
	case DRM_MODE_CONNECTOR_Composite:   return "Composite";
	case DRM_MODE_CONNECTOR_SVIDEO:      return "SVIDEO";
	case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
	case DRM_MODE_CONNECTOR_Component:   return "Component";
	case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
	case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
	case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
	case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
	case DRM_MODE_CONNECTOR_TV:          return "TV";
	case DRM_MODE_CONNECTOR_eDP:         return "eDP";
	case DRM_MODE_CONNECTOR_VIRTUAL:     return "Virtual";
	case DRM_MODE_CONNECTOR_DSI:         return "DSI";
#ifdef DRM_MODE_CONNECTOR_DPI
	case DRM_MODE_CONNECTOR_DPI:         return "DPI";
#endif
#ifdef DRM_MODE_CONNECTOR_WRITEBACK
	case DRM_MODE_CONNECTOR_WRITEBACK:   return "Writeback";
#endif
#ifdef DRM_MODE_CONNECTOR_SPI
	case DRM_MODE_CONNECTOR_SPI:         return "SPI";
#endif
#ifdef DRM_MODE_CONNECTOR_USB
	case DRM_MODE_CONNECTOR_USB:         return "USB";
#endif
	default:                             return "Unknown";
	}
}

static const char *encoder_type_name(uint32_t type)
{
	switch (type) {
	case DRM_MODE_ENCODER_NONE:    return "None";
	case DRM_MODE_ENCODER_DAC:     return "DAC";
	case DRM_MODE_ENCODER_TMDS:    return "TMDS";
	case DRM_MODE_ENCODER_LVDS:    return "LVDS";
	case DRM_MODE_ENCODER_TVDAC:   return "TV";
	case DRM_MODE_ENCODER_VIRTUAL: return "Virtual";
	case DRM_MODE_ENCODER_DSI:     return "DSI";
	case DRM_MODE_ENCODER_DPMST:   return "DP MST";
#ifdef DRM_MODE_ENCODER_DPI
	case DRM_MODE_ENCODER_DPI:     return "DPI";
#endif
	default:                       return "Unknown";
	}
}

static const char *connection_name(drmModeConnection c)
{
	switch (c) {
	case DRM_MODE_CONNECTED:         return "connected";
	case DRM_MODE_DISCONNECTED:      return "disconnected";
	case DRM_MODE_UNKNOWNCONNECTION: return "unknown";
	default:                         return "?";
	}
}

/* ============================================================
 * Topology snapshot
 *
 * We copy everything we need out of the libdrm structs into plain
 * arrays.  Two reasons:
 *   1. The backtracking solver and the matrix printer work on indices,
 *      not on libdrm pointers that must be freed.
 *   2. --monitor keeps an "old" snapshot and diffs it against a "new"
 *      one after each hotplug uevent.
 * ============================================================ */
struct mode_brief {
	bool     valid;
	bool     preferred; /* true if DRM_MODE_TYPE_PREFERRED was set */
	uint16_t hdisplay;
	uint16_t vdisplay;
	uint32_t vrefresh;
	char     name[DRM_DISPLAY_MODE_LEN];
};

struct conn_info {
	uint32_t id;
	uint32_t type;
	uint32_t type_id;
	drmModeConnection connection;
	uint32_t mm_width;
	uint32_t mm_height;
	int      count_modes;
	struct mode_brief pref;
	uint32_t cur_encoder_id;  /* 0 = not bound */
	uint32_t cur_crtc_id;     /* via cur_encoder -> crtc_id, 0 = none */
	int      count_encoders;
	uint32_t encoders[MAX_CONN_ENCODERS];
	uint32_t crtc_mask;       /* OR of possible_crtcs of all encoders */
};

struct enc_info {
	uint32_t id;
	uint32_t type;
	uint32_t cur_crtc_id;
	uint32_t possible_crtcs;  /* bit i == res->crtcs[i]           */
	uint32_t possible_clones; /* bit j == res->encoders[j]        */
};

enum plane_kind { PLANE_UNKNOWN = -1, PLANE_OVERLAY = 0,
		  PLANE_PRIMARY = 1, PLANE_CURSOR = 2 };

struct plane_info {
	uint32_t id;
	int      kind;
	uint32_t possible_crtcs;
	uint32_t cur_crtc_id;
	uint32_t cur_fb_id;
};

struct crtc_info {
	uint32_t id;
	bool     mode_valid;
	struct mode_brief mode;
};

struct topology {
	int n_crtcs;
	struct crtc_info crtc[MAX_CRTCS];
	int n_enc;
	struct enc_info enc[MAX_OBJS];
	int n_conn;
	struct conn_info conn[MAX_OBJS];
	int n_planes;
	struct plane_info plane[MAX_OBJS];
};

static void conn_label(const struct conn_info *c, char *buf, size_t len)
{
	snprintf(buf, len, "%s-%u", connector_type_name(c->type), c->type_id);
}

static void mode_to_brief(const drmModeModeInfo *m, struct mode_brief *b)
{
	b->valid     = true;
	b->preferred = (m->type & DRM_MODE_TYPE_PREFERRED) != 0;
	b->hdisplay  = m->hdisplay;
	b->vdisplay  = m->vdisplay;
	b->vrefresh  = m->vrefresh;
	memcpy(b->name, m->name, sizeof(b->name));
	b->name[sizeof(b->name) - 1] = '\0';
}

/* CRTC ID -> CRTC index (position in drmModeRes->crtcs), -1 if absent */
static int crtc_index_of(const struct topology *t, uint32_t crtc_id)
{
	for (int i = 0; i < t->n_crtcs; i++)
		if (t->crtc[i].id == crtc_id)
			return i;
	return -1;
}

static int encoder_index_of(const struct topology *t, uint32_t enc_id)
{
	for (int i = 0; i < t->n_enc; i++)
		if (t->enc[i].id == enc_id)
			return i;
	return -1;
}

/* Print a CRTC mask as "0x5 -> idx{0,2} = CRTC{88,168}" */
static void print_crtc_mask(const struct topology *t, uint32_t mask)
{
	printf("0x%" PRIx32 " -> idx{", mask);
	bool first = true;
	for (int i = 0; i < t->n_crtcs; i++) {
		if (!(mask & (1u << i)))
			continue;
		printf("%s%d", first ? "" : ",", i);
		first = false;
	}
	printf("} = CRTC{");
	first = true;
	for (int i = 0; i < t->n_crtcs; i++) {
		if (!(mask & (1u << i)))
			continue;
		printf("%s%" PRIu32, first ? "" : ",", t->crtc[i].id);
		first = false;
	}
	printf("}");
	/* Bits beyond count_crtcs would be a driver bug worth seeing */
	if (t->n_crtcs < 32 && (mask >> t->n_crtcs))
		printf(" [!] bits beyond count_crtcs set");
}

/* ============================================================
 * query_connector - fetch one connector, with or without a probe.
 *
 * drmModeGetConnector()        -> GETCONNECTOR with count_modes = 0.
 *   If this fd is the current DRM master, the kernel calls the
 *   connector's ->fill_modes() hook: it re-runs ->detect(), re-reads
 *   the EDID over DDC and rebuilds the mode list.  This is a *forced
 *   probe*: it can block for a while and, per the UAPI docs, may even
 *   cause flicker.  A non-master fd is silently demoted to a read-only
 *   query (kernel >= v5.15, drm_mode_getconnector()).
 *
 * drmModeGetConnectorCurrent() -> GETCONNECTOR with count_modes = 1.
 *   The kernel skips ->fill_modes() and just reports what it already
 *   knows (status, mode list, current encoder).  Cheap, never blocks
 *   on DDC.
 * ============================================================ */
static drmModeConnector *query_connector(int fd, uint32_t id, bool probe)
{
	return probe ? drmModeGetConnector(fd, id)
		     : drmModeGetConnectorCurrent(fd, id);
}

static int plane_kind_of(int fd, uint32_t plane_id)
{
	int kind = PLANE_UNKNOWN;
	drmModeObjectProperties *props =
		drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
	if (!props)
		return kind;

	for (uint32_t i = 0; i < props->count_props; i++) {
		drmModePropertyRes *p = drmModeGetProperty(fd, props->props[i]);
		if (!p)
			continue;
		if (strcmp(p->name, "type") == 0)
			kind = (int)props->prop_values[i];
		drmModeFreeProperty(p);
		if (kind != PLANE_UNKNOWN)
			break;
	}
	drmModeFreeObjectProperties(props);
	return kind;
}

/* ============================================================
 * collect_topology - snapshot every KMS object that takes part in
 * routing.  Returns 0 on success, -1 on a fatal error.
 *
 * with_planes: planes are only needed for --topology; the monitor
 * loop skips them to keep each re-scan cheap.
 * ============================================================ */
static int collect_topology(int fd, bool probe, bool with_planes,
			    struct topology *t)
{
	memset(t, 0, sizeof(*t));

	drmModeRes *res = drmModeGetResources(fd);
	if (!res) {
		fprintf(stderr, "drmModeGetResources failed: %s\n",
			strerror(errno));
		return -1;
	}

	/*
	 * CRTCs: the ORDER of res->crtcs[] is the index space that every
	 * possible_crtcs mask refers to.  Keep it exactly as returned.
	 */
	if (res->count_crtcs > MAX_CRTCS)
		fprintf(stderr, "warning: %d CRTCs, only %d tracked\n",
			res->count_crtcs, MAX_CRTCS);
	t->n_crtcs = res->count_crtcs < MAX_CRTCS ? res->count_crtcs
						   : MAX_CRTCS;
	for (int i = 0; i < t->n_crtcs; i++) {
		t->crtc[i].id = res->crtcs[i];
		drmModeCrtc *c = drmModeGetCrtc(fd, res->crtcs[i]);
		if (!c) {
			fprintf(stderr, "drmModeGetCrtc(%u) failed: %s\n",
				res->crtcs[i], strerror(errno));
			continue;
		}
		t->crtc[i].mode_valid = c->mode_valid;
		if (c->mode_valid)
			mode_to_brief(&c->mode, &t->crtc[i].mode);
		drmModeFreeCrtc(c);
	}

	/* Encoders: possible_crtcs / possible_clones live here */
	for (int i = 0; i < res->count_encoders && t->n_enc < MAX_OBJS; i++) {
		drmModeEncoder *e = drmModeGetEncoder(fd, res->encoders[i]);
		if (!e) {
			fprintf(stderr, "drmModeGetEncoder(%u) failed: %s\n",
				res->encoders[i], strerror(errno));
			continue;
		}
		struct enc_info *ei = &t->enc[t->n_enc++];
		ei->id              = e->encoder_id;
		ei->type            = e->encoder_type;
		ei->cur_crtc_id     = e->crtc_id;
		ei->possible_crtcs  = e->possible_crtcs;
		ei->possible_clones = e->possible_clones;
		drmModeFreeEncoder(e);
	}

	/* Connectors */
	for (int i = 0; i < res->count_connectors && t->n_conn < MAX_OBJS; i++) {
		drmModeConnector *c = query_connector(fd, res->connectors[i],
						      probe);
		if (!c) {
			fprintf(stderr, "GetConnector(%u) failed: %s\n",
				res->connectors[i], strerror(errno));
			continue;
		}
		struct conn_info *ci = &t->conn[t->n_conn++];
		ci->id          = c->connector_id;
		ci->type        = c->connector_type;
		ci->type_id     = c->connector_type_id;
		ci->connection  = c->connection;
		ci->mm_width    = c->mmWidth;
		ci->mm_height   = c->mmHeight;
		ci->count_modes = c->count_modes;
		ci->cur_encoder_id = c->encoder_id;

		/* Preferred mode: the one flagged PREFERRED, else modes[0] */
		for (int m = 0; m < c->count_modes; m++) {
			if (c->modes[m].type & DRM_MODE_TYPE_PREFERRED) {
				mode_to_brief(&c->modes[m], &ci->pref);
				break;
			}
		}
		if (!ci->pref.valid && c->count_modes > 0)
			mode_to_brief(&c->modes[0], &ci->pref);

		ci->count_encoders = c->count_encoders < MAX_CONN_ENCODERS ?
				     c->count_encoders : MAX_CONN_ENCODERS;
		for (int e = 0; e < ci->count_encoders; e++) {
			ci->encoders[e] = c->encoders[e];
			int ei = encoder_index_of(t, c->encoders[e]);
			if (ei >= 0)
				ci->crtc_mask |= t->enc[ei].possible_crtcs;
		}
		if (ci->cur_encoder_id) {
			int ei = encoder_index_of(t, ci->cur_encoder_id);
			if (ei >= 0)
				ci->cur_crtc_id = t->enc[ei].cur_crtc_id;
		}
		drmModeFreeConnector(c);
	}

	drmModeFreeResources(res);

	if (!with_planes)
		return 0;

	/* Planes: need DRM_CLIENT_CAP_UNIVERSAL_PLANES to see PRIMARY/CURSOR */
	drmModePlaneRes *pres = drmModeGetPlaneResources(fd);
	if (!pres) {
		fprintf(stderr, "drmModeGetPlaneResources failed: %s\n",
			strerror(errno));
		return 0; /* not fatal for the connector view */
	}
	for (uint32_t i = 0; i < pres->count_planes && t->n_planes < MAX_OBJS; i++) {
		drmModePlane *p = drmModeGetPlane(fd, pres->planes[i]);
		if (!p) {
			fprintf(stderr, "drmModeGetPlane(%u) failed: %s\n",
				pres->planes[i], strerror(errno));
			continue;
		}
		struct plane_info *pi = &t->plane[t->n_planes++];
		pi->id             = p->plane_id;
		pi->possible_crtcs = p->possible_crtcs;
		pi->cur_crtc_id    = p->crtc_id;
		pi->cur_fb_id      = p->fb_id;
		pi->kind           = plane_kind_of(fd, p->plane_id);
		drmModeFreePlane(p);
	}
	drmModeFreePlaneResources(pres);
	return 0;
}

static const char *plane_kind_name(int kind)
{
	switch (kind) {
	case PLANE_PRIMARY: return "PRIMARY";
	case PLANE_OVERLAY: return "OVERLAY";
	case PLANE_CURSOR:  return "CURSOR";
	default:            return "?";
	}
}

/* ============================================================
 * Multi-display assignment (small backtracking search)
 *
 * The core bring-up question: "given N connected connectors, can each
 * get its OWN CRTC at the same time, and which?"  Constraints:
 *
 *   - a connector can only use one of its encoders
 *     (drmModeConnector->encoders[])
 *   - an encoder can only feed CRTCs in its possible_crtcs mask
 *   - one encoder serves one connector here, and one CRTC drives one
 *     encoder (we solve the "extended desktop" case, not cloning)
 *
 * This is bipartite matching; with <= 32 CRTCs and a handful of
 * connectors, plain backtracking is instant and easy to read.  We try
 * the connector's *current* encoder/CRTC first so the answer changes
 * as little as possible (a compositor would do the same to avoid a
 * full modeset).  If not everything fits we keep the best partial
 * answer.
 *
 * NOTE: this only checks the static routing masks.  Bandwidth, clocks
 * and per-VP resolution limits are only checked by the driver's
 * atomic_check -- i.e. a DRM_MODE_ATOMIC_TEST_ONLY commit.
 * ============================================================ */
struct assign_state {
	const struct topology *t;
	int      order[MAX_OBJS];   /* connector indices to place */
	int      n;
	int      cur_enc[MAX_OBJS]; /* per order[k]: encoder index or -1 */
	int      cur_crtc[MAX_OBJS];
	int      best_enc[MAX_OBJS];
	int      best_crtc[MAX_OBJS];
	int      best_count;
	uint32_t used_crtcs;
	uint64_t used_encs;
};

static void try_pair(struct assign_state *s, int k, int count,
		     int enc_idx, int crtc_idx);

static void assign_search(struct assign_state *s, int k, int count)
{
	if (s->best_count == s->n)
		return;                           /* already perfect */
	if (count + (s->n - k) <= s->best_count)
		return;                           /* cannot beat best */
	if (k == s->n) {
		s->best_count = count;
		memcpy(s->best_enc,  s->cur_enc,  sizeof(s->best_enc));
		memcpy(s->best_crtc, s->cur_crtc, sizeof(s->best_crtc));
		return;
	}

	const struct topology *t = s->t;
	const struct conn_info *c = &t->conn[s->order[k]];

	/* 1) Current binding first (keeps the existing routing stable) */
	int cur_e = c->cur_encoder_id ? encoder_index_of(t, c->cur_encoder_id) : -1;
	int cur_x = c->cur_crtc_id ? crtc_index_of(t, c->cur_crtc_id) : -1;
	if (cur_e >= 0 && cur_x >= 0)
		try_pair(s, k, count, cur_e, cur_x);

	/* 2) Every other (encoder, CRTC) pair the masks allow */
	for (int e = 0; e < c->count_encoders; e++) {
		int ei = encoder_index_of(t, c->encoders[e]);
		if (ei < 0)
			continue;
		for (int x = 0; x < t->n_crtcs; x++) {
			if (ei == cur_e && x == cur_x)
				continue;         /* already tried */
			try_pair(s, k, count, ei, x);
		}
	}

	/* 3) Leave this connector dark and see if the others fit */
	s->cur_enc[k]  = -1;
	s->cur_crtc[k] = -1;
	assign_search(s, k + 1, count);
}

static void try_pair(struct assign_state *s, int k, int count,
		     int enc_idx, int crtc_idx)
{
	const struct enc_info *e = &s->t->enc[enc_idx];

	if (!(e->possible_crtcs & (1u << crtc_idx)))
		return;
	if (s->used_crtcs & (1u << crtc_idx))
		return;
	if (s->used_encs & (1ull << enc_idx))
		return;

	s->used_crtcs |= 1u << crtc_idx;
	s->used_encs  |= 1ull << enc_idx;
	s->cur_enc[k]  = enc_idx;
	s->cur_crtc[k] = crtc_idx;

	assign_search(s, k + 1, count + 1);

	s->used_crtcs &= ~(1u << crtc_idx);
	s->used_encs  &= ~(1ull << enc_idx);
}

/* Returns the number of connectors that received a CRTC */
static int solve_assignment(const struct topology *t, struct assign_state *s)
{
	memset(s, 0, sizeof(*s));
	s->t = t;
	for (int i = 0; i < t->n_conn; i++)
		if (t->conn[i].connection == DRM_MODE_CONNECTED)
			s->order[s->n++] = i;
	for (int k = 0; k < MAX_OBJS; k++) {
		s->cur_enc[k] = s->cur_crtc[k] = -1;
		s->best_enc[k] = s->best_crtc[k] = -1;
	}
	s->best_count = -1;
	assign_search(s, 0, 0);
	if (s->best_count < 0)
		s->best_count = 0;
	return s->best_count;
}

/* First PRIMARY plane that may be attached to CRTC index idx */
static int primary_plane_for(const struct topology *t, int idx)
{
	for (int i = 0; i < t->n_planes; i++)
		if (t->plane[i].kind == PLANE_PRIMARY &&
		    (t->plane[i].possible_crtcs & (1u << idx)))
			return i;
	return -1;
}

/* ============================================================
 * print_topology - the routing matrix Experiment 01 only described
 * ============================================================ */
static void print_topology(const struct topology *t, bool probed)
{
	char label[32];

	printf("============================================================\n");
	printf(" KMS Routing Topology  (connector probe: %s)\n",
	       probed ? "forced (drmModeGetConnector)"
		      : "none (drmModeGetConnectorCurrent)");
	printf("============================================================\n");

	/* --- CRTC index <-> ID --- */
	printf("\n=== CRTCs: index <-> object ID (%d) ===\n", t->n_crtcs);
	printf("  Every possible_crtcs mask is indexed by the POSITION in\n"
	       "  drmModeRes->crtcs[], NOT by the CRTC object ID.\n");
	for (int i = 0; i < t->n_crtcs; i++) {
		printf("  idx %-2d -> CRTC %-4" PRIu32 " (mask bit 0x%08x)  ",
		       i, t->crtc[i].id, 1u << i);
		if (t->crtc[i].mode_valid)
			printf("active %ux%u@%u\n", t->crtc[i].mode.hdisplay,
			       t->crtc[i].mode.vdisplay,
			       t->crtc[i].mode.vrefresh);
		else
			printf("inactive\n");
	}

	/* --- Encoders --- */
	printf("\n=== Encoders (%d) ===\n", t->n_enc);
	for (int i = 0; i < t->n_enc; i++) {
		const struct enc_info *e = &t->enc[i];
		printf("  ENC %-4" PRIu32 " [enc idx %d] type=%-7s cur CRTC=%-4" PRIu32 "\n",
		       e->id, i, encoder_type_name(e->type), e->cur_crtc_id);
		printf("      possible_crtcs  = ");
		print_crtc_mask(t, e->possible_crtcs);
		printf("\n      possible_clones = 0x%" PRIx32 " -> ENC{",
		       e->possible_clones);
		bool first = true;
		for (int j = 0; j < t->n_enc && j < 32; j++) {
			if (!(e->possible_clones & (1u << j)))
				continue;
			printf("%s%" PRIu32, first ? "" : ",", t->enc[j].id);
			first = false;
		}
		printf("}%s\n", e->possible_clones == (1u << i) ?
		       "  (only itself: no cloning)" : "");
	}

	/* --- Connectors --- */
	printf("\n=== Connectors (%d) ===\n", t->n_conn);
	for (int i = 0; i < t->n_conn; i++) {
		const struct conn_info *c = &t->conn[i];
		conn_label(c, label, sizeof(label));
		printf("  CONN %-4" PRIu32 " %-12s %-12s %ux%umm  modes=%d",
		       c->id, label, connection_name(c->connection),
		       c->mm_width, c->mm_height, c->count_modes);
		if (c->pref.valid)
			printf("  %s=%ux%u@%u",
			       c->pref.preferred ? "preferred" : "first",
			       c->pref.hdisplay, c->pref.vdisplay,
			       c->pref.vrefresh);
		printf("\n      possible encoders: ");
		for (int e = 0; e < c->count_encoders; e++)
			printf("%s%" PRIu32, e ? "," : "", c->encoders[e]);
		if (!c->count_encoders)
			printf("(none)");
		printf("\n      reachable CRTCs  : ");
		print_crtc_mask(t, c->crtc_mask);
		printf("\n      current route    : ");
		if (c->cur_encoder_id) {
			int x = crtc_index_of(t, c->cur_crtc_id);
			printf("ENC %" PRIu32 " -> CRTC %" PRIu32, c->cur_encoder_id,
			       c->cur_crtc_id);
			if (x >= 0)
				printf(" (idx %d)", x);
			printf("\n");
		} else {
			printf("(not bound)\n");
		}
	}

	/* --- Planes --- */
	printf("\n=== Planes (%d) ===\n", t->n_planes);
	for (int i = 0; i < t->n_planes; i++) {
		const struct plane_info *p = &t->plane[i];
		printf("  PLANE %-4" PRIu32 " %-7s cur CRTC=%-4" PRIu32 " FB=%-4" PRIu32
		       " possible_crtcs = ",
		       p->id, plane_kind_name(p->kind), p->cur_crtc_id,
		       p->cur_fb_id);
		print_crtc_mask(t, p->possible_crtcs);
		printf("\n");
	}

	/* --- Matrix --- */
	printf("\n=== Routing matrix  (X = allowed, * = currently in use) ===\n");
	printf("  %-26s", "CRTC idx ->");
	for (int x = 0; x < t->n_crtcs; x++)
		printf(" %5d", x);
	printf("\n  %-26s", "CRTC id  ->");
	for (int x = 0; x < t->n_crtcs; x++)
		printf(" %5" PRIu32, t->crtc[x].id);
	printf("\n");

	for (int i = 0; i < t->n_conn; i++) {
		const struct conn_info *c = &t->conn[i];
		char row[64];
		conn_label(c, label, sizeof(label));
		snprintf(row, sizeof(row), "CONN %s%s", label,
			 c->connection == DRM_MODE_CONNECTED ? " (C)" : "");
		printf("  %-26s", row);
		for (int x = 0; x < t->n_crtcs; x++) {
			char m = '.';
			if (c->crtc_mask & (1u << x))
				m = (c->cur_crtc_id == t->crtc[x].id) ? '*' : 'X';
			printf(" %5c", m);
		}
		printf("\n");
	}
	for (int i = 0; i < t->n_enc; i++) {
		const struct enc_info *e = &t->enc[i];
		char row[64];
		snprintf(row, sizeof(row), "ENC %" PRIu32 " (%s)", e->id,
			 encoder_type_name(e->type));
		printf("  %-26s", row);
		for (int x = 0; x < t->n_crtcs; x++) {
			char m = '.';
			if (e->possible_crtcs & (1u << x))
				m = (e->cur_crtc_id == t->crtc[x].id) ? '*' : 'X';
			printf(" %5c", m);
		}
		printf("\n");
	}
	for (int i = 0; i < t->n_planes; i++) {
		const struct plane_info *p = &t->plane[i];
		char row[64];
		snprintf(row, sizeof(row), "PLANE %" PRIu32 " (%s)", p->id,
			 plane_kind_name(p->kind));
		printf("  %-26s", row);
		for (int x = 0; x < t->n_crtcs; x++) {
			char m = '.';
			if (p->possible_crtcs & (1u << x))
				m = (p->cur_crtc_id == t->crtc[x].id) ? '*' : 'X';
			printf(" %5c", m);
		}
		printf("\n");
	}
	printf("  (C) = connected\n");

	/* --- Assignment --- */
	struct assign_state s;
	int placed = solve_assignment(t, &s);

	printf("\n=== Multi-display assignment (static masks only) ===\n");
	if (s.n == 0) {
		printf("  No connected connectors -- nothing to assign.\n");
		return;
	}
	for (int k = 0; k < s.n; k++) {
		const struct conn_info *c = &t->conn[s.order[k]];
		conn_label(c, label, sizeof(label));
		if (s.best_crtc[k] < 0) {
			printf("  %-12s -> (no free CRTC/encoder left)\n", label);
			continue;
		}
		int x  = s.best_crtc[k];
		int pp = primary_plane_for(t, x);
		printf("  %-12s -> ENC %-4" PRIu32 " -> CRTC idx %d (id %" PRIu32 ")",
		       label, t->enc[s.best_enc[k]].id, x, t->crtc[x].id);
		if (pp >= 0)
			printf("  primary PLANE %" PRIu32, t->plane[pp].id);
		else
			printf("  [!] no PRIMARY plane for this CRTC");
		if (c->cur_crtc_id == t->crtc[x].id)
			printf("  (unchanged)");
		printf("\n");
	}
	printf("  => %d of %d connected display(s) can be lit simultaneously.\n",
	       placed, s.n);
	printf("  Next step on real hardware: build this as one atomic request\n"
	       "  and validate it with DRM_MODE_ATOMIC_TEST_ONLY -- only the\n"
	       "  driver knows about bandwidth / clock / per-VP size limits.\n");
}

/* ============================================================
 * Uevent parsing
 *
 * A kernel uevent datagram on NETLINK_KOBJECT_UEVENT looks like:
 *
 *   "change@/devices/platform/display-subsystem/drm/card0\0"   header
 *   "ACTION=change\0"
 *   "DEVPATH=/devices/platform/display-subsystem/drm/card0\0"
 *   "SUBSYSTEM=drm\0"
 *   "MAJOR=226\0" "MINOR=0\0" "DEVNAME=dri/card0\0" ...
 *   "HOTPLUG=1\0" ["CONNECTOR=<id>\0"] ["PROPERTY=<id>\0"]
 *   "SEQNUM=<n>\0"
 *
 * (The path above is only an illustration of the format.)
 * The header is "<action>@<devpath>", then NUL-separated KEY=VALUE
 * strings (lib/kobject_uevent.c).  udev's re-broadcast messages start
 * with "libudev\0" instead and travel on a different multicast group,
 * so we never see them on group 1.
 * ============================================================ */
struct uevent {
	const char *action;
	const char *devpath;
	const char *subsystem;
	const char *devname;
	const char *major;
	const char *minor;
	const char *hotplug;
	const char *connector;
	const char *property;
	const char *lease;
	const char *seqnum;
};

/*
 * parse_uevent - split a raw uevent datagram in place.
 * @buf: datagram, must be NUL-terminated at buf[len] by the caller.
 * @len: number of bytes received.
 * Returns 0 on success, -1 if this is not a kernel uevent.
 */
static int parse_uevent(const char *buf, size_t len, struct uevent *ev)
{
	memset(ev, 0, sizeof(*ev));
	if (len == 0)
		return -1;

	size_t hlen = strnlen(buf, len);
	if (hlen == len || !memchr(buf, '@', hlen))
		return -1;              /* no "<action>@<devpath>" header */

	const char *p   = buf + hlen + 1;
	const char *end = buf + len;
	while (p < end) {
		size_t l = strnlen(p, (size_t)(end - p));
		if (l == 0) {
			p++;
			continue;
		}
		const char *eq = memchr(p, '=', l);
		if (eq) {
			size_t klen = (size_t)(eq - p);
			const char *val = eq + 1;
#define UEV_KEY(name, field) \
			if (klen == sizeof(name) - 1 && !memcmp(p, name, klen)) ev->field = val;
			UEV_KEY("ACTION",    action)
			UEV_KEY("DEVPATH",   devpath)
			UEV_KEY("SUBSYSTEM", subsystem)
			UEV_KEY("DEVNAME",   devname)
			UEV_KEY("MAJOR",     major)
			UEV_KEY("MINOR",     minor)
			UEV_KEY("HOTPLUG",   hotplug)
			UEV_KEY("CONNECTOR", connector)
			UEV_KEY("PROPERTY",  property)
			UEV_KEY("LEASE",     lease)
			UEV_KEY("SEQNUM",    seqnum)
#undef UEV_KEY
		}
		p += l + 1;
	}
	return 0;
}

/*
 * uevent_is_for_device - does this event belong to the DRM node we
 * opened?  We compare MAJOR/MINOR with fstat() of our fd, which also
 * works when -d points at a /dev/dri/by-path/ symlink.
 */
static bool uevent_is_for_device(const struct uevent *ev, dev_t rdev)
{
	if (!ev->major || !ev->minor)
		return false;
	char *e1, *e2;
	unsigned long maj = strtoul(ev->major, &e1, 10);
	unsigned long min = strtoul(ev->minor, &e2, 10);
	if (*e1 || *e2)
		return false;
	return maj == major(rdev) && min == minor(rdev);
}

/* ============================================================
 * open_uevent_socket - raw kernel uevent listener, no libudev.
 *
 * AF_NETLINK / NETLINK_KOBJECT_UEVENT, multicast group 1: the kernel
 * broadcasts every kobject uevent to group 1 (netlink_broadcast(...,
 * group 1) in lib/kobject_uevent.c).  The kernel socket is created with
 * NL_CFG_F_NONROOT_RECV, so even non-root processes may listen.
 * nl_pid = 0 lets the kernel pick a unique port ID for us.
 * ============================================================ */
static int open_uevent_socket(void)
{
	int s = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC,
		       NETLINK_KOBJECT_UEVENT);
	if (s < 0) {
		fprintf(stderr, "socket(NETLINK_KOBJECT_UEVENT): %s\n",
			strerror(errno));
		return -1;
	}

	/*
	 * Uevent storms (e.g. module load) can overflow the default receive
	 * buffer; the kernel then drops messages and recv() reports ENOBUFS.
	 * A bigger buffer makes that less likely.  Failure is non-fatal.
	 */
	int rcvbuf = 1 << 20;
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0)
		fprintf(stderr, "warning: SO_RCVBUF: %s\n", strerror(errno));

	struct sockaddr_nl addr;
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid    = 0;   /* kernel assigns */
	addr.nl_groups = 1;   /* group 1 = kernel uevents */
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "bind(netlink group 1): %s\n", strerror(errno));
		close(s);
		return -1;
	}
	return s;
}

/* ============================================================
 * Connector diff for --monitor
 * ============================================================ */
static const struct conn_info *find_conn(const struct topology *t, uint32_t id)
{
	for (int i = 0; i < t->n_conn; i++)
		if (t->conn[i].id == id)
			return &t->conn[i];
	return NULL;
}

static void fmt_mode(const struct mode_brief *m, char *buf, size_t len)
{
	if (m->valid)
		snprintf(buf, len, "%ux%u@%u", m->hdisplay, m->vdisplay,
			 m->vrefresh);
	else
		snprintf(buf, len, "none");
}

/* Returns the number of changes printed */
static int diff_topology(const struct topology *old, const struct topology *cur)
{
	int changes = 0;
	char label[32], a[48], b[48];

	for (int i = 0; i < cur->n_conn; i++) {
		const struct conn_info *n = &cur->conn[i];
		const struct conn_info *o = find_conn(old, n->id);
		conn_label(n, label, sizeof(label));

		if (!o) {
			/* e.g. a DP-MST branch device appearing */
			printf("  + CONN %" PRIu32 " %s appeared (%s)\n", n->id,
			       label, connection_name(n->connection));
			changes++;
			continue;
		}
		if (o->connection != n->connection) {
			printf("  ~ CONN %" PRIu32 " %s: %s -> %s\n", n->id, label,
			       connection_name(o->connection),
			       connection_name(n->connection));
			changes++;
		}
		if (o->count_modes != n->count_modes) {
			printf("  ~ CONN %" PRIu32 " %s: modes %d -> %d\n", n->id,
			       label, o->count_modes, n->count_modes);
			changes++;
		}
		fmt_mode(&o->pref, a, sizeof(a));
		fmt_mode(&n->pref, b, sizeof(b));
		if (strcmp(a, b) != 0) {
			printf("  ~ CONN %" PRIu32 " %s: preferred %s -> %s\n",
			       n->id, label, a, b);
			changes++;
		}
		if (o->mm_width != n->mm_width || o->mm_height != n->mm_height) {
			printf("  ~ CONN %" PRIu32 " %s: size %ux%umm -> %ux%umm\n",
			       n->id, label, o->mm_width, o->mm_height,
			       n->mm_width, n->mm_height);
			changes++;
		}
		if (o->cur_encoder_id != n->cur_encoder_id ||
		    o->cur_crtc_id != n->cur_crtc_id) {
			printf("  ~ CONN %" PRIu32 " %s: route ENC %" PRIu32 "/CRTC %" PRIu32
			       " -> ENC %" PRIu32 "/CRTC %" PRIu32 "\n", n->id, label,
			       o->cur_encoder_id, o->cur_crtc_id,
			       n->cur_encoder_id, n->cur_crtc_id);
			changes++;
		}
	}
	for (int i = 0; i < old->n_conn; i++) {
		const struct conn_info *o = &old->conn[i];
		if (!find_conn(cur, o->id)) {
			conn_label(o, label, sizeof(label));
			printf("  - CONN %" PRIu32 " %s disappeared\n", o->id, label);
			changes++;
		}
	}
	return changes;
}

static void print_connector_summary(const struct topology *t)
{
	char label[32], m[48];
	for (int i = 0; i < t->n_conn; i++) {
		const struct conn_info *c = &t->conn[i];
		conn_label(c, label, sizeof(label));
		fmt_mode(&c->pref, m, sizeof(m));
		printf("  CONN %-4" PRIu32 " %-12s %-12s modes=%-3d pref=%s\n",
		       c->id, label, connection_name(c->connection),
		       c->count_modes, m);
	}
}

/*
 * print_connector_property - resolve PROPERTY=<id> from a connector
 * property event (e.g. "Content Protection") into name and value.
 */
static void print_connector_property(int fd, uint32_t conn_id, uint32_t prop_id)
{
	drmModePropertyRes *p = drmModeGetProperty(fd, prop_id);
	if (!p) {
		printf("    property %" PRIu32 ": lookup failed: %s\n", prop_id,
		       strerror(errno));
		return;
	}
	printf("    property %" PRIu32 " = \"%s\"", prop_id, p->name);

	drmModeObjectProperties *props =
		drmModeObjectGetProperties(fd, conn_id, DRM_MODE_OBJECT_CONNECTOR);
	if (props) {
		for (uint32_t i = 0; i < props->count_props; i++) {
			if (props->props[i] != prop_id)
				continue;
			uint64_t v = props->prop_values[i];
			printf(", value %" PRIu64, v);
			if (p->flags & DRM_MODE_PROP_ENUM) {
				for (int e = 0; e < p->count_enums; e++)
					if (p->enums[e].value == v)
						printf(" (%s)", p->enums[e].name);
			}
			break;
		}
		drmModeFreeObjectProperties(props);
	}
	printf("\n");
	drmModeFreeProperty(p);
}

/* ============================================================
 * is_drm_master - same trick libdrm's drmIsMaster() uses.
 *
 * DRM_IOCTL_AUTH_MAGIC requires master.  Magic 0 is never valid, so a
 * master fd fails with EINVAL while a non-master fd fails with EACCES.
 * We re-implement it with drmAuthMagic() because drmIsMaster() only
 * exists in newer libdrm releases.
 * ============================================================ */
static bool is_drm_master(int fd)
{
	return drmAuthMagic(fd, 0) != -EACCES;
}

/* ============================================================
 * run_monitor - the hotplug event loop
 * ============================================================ */
static int run_monitor(int fd, bool probe)
{
	struct stat st;
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "fstat: %s\n", strerror(errno));
		return -1;
	}

	int nl = open_uevent_socket();
	if (nl < 0)
		return -1;

	/*
	 * Order matters: open the socket BEFORE the baseline scan, so a
	 * plug event that races with the scan is queued, not lost.
	 */
	static struct topology snap[2];
	int cur = 0;
	if (collect_topology(fd, probe, false, &snap[cur]) < 0) {
		close(nl);
		return -1;
	}

	printf("Monitoring hotplug uevents for DRM device %u:%u "
	       "(Ctrl+C to stop)\n", major(st.st_rdev), minor(st.st_rdev));
	printf("Re-probe method: %s\n", probe ?
	       "drmModeGetConnector (forced probe if DRM master)" :
	       "drmModeGetConnectorCurrent (no probe)");
	printf("Baseline:\n");
	print_connector_summary(&snap[cur]);
	fflush(stdout);

	char buf[UEVENT_RX_BUF + 1];
	unsigned long n_events = 0, n_hotplug = 0;

	while (!g_stop) {
		struct pollfd pfd = { .fd = nl, .events = POLLIN };
		int r = poll(&pfd, 1, -1);
		if (r < 0) {
			if (errno == EINTR)
				continue;       /* g_stop re-checked by loop */
			fprintf(stderr, "poll: %s\n", strerror(errno));
			break;
		}
		if (!(pfd.revents & POLLIN))
			continue;

		struct sockaddr_nl src;
		struct iovec iov = { .iov_base = buf, .iov_len = UEVENT_RX_BUF };
		struct msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_name    = &src;
		msg.msg_namelen = sizeof(src);
		msg.msg_iov     = &iov;
		msg.msg_iovlen  = 1;

		ssize_t len = recvmsg(nl, &msg, 0);
		bool rescan_anyway = false;
		if (len < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			if (errno == ENOBUFS) {
				/* Receive queue overflowed: events were lost.
				 * The only safe recovery is a full re-scan. */
				printf("[!] uevent queue overflow (ENOBUFS), "
				       "re-scanning everything\n");
				rescan_anyway = true;
			} else {
				fprintf(stderr, "recvmsg: %s\n", strerror(errno));
				break;
			}
		}

		struct uevent ev;
		memset(&ev, 0, sizeof(ev));
		if (!rescan_anyway) {
			if (msg.msg_flags & MSG_TRUNC) {
				printf("[!] truncated uevent ignored\n");
				continue;
			}
			/* Only trust messages sent by the kernel (port 0) */
			if (src.nl_pid != 0)
				continue;
			buf[len] = '\0';
			if (parse_uevent(buf, (size_t)len, &ev) < 0)
				continue;
			n_events++;

			if (!ev.subsystem || strcmp(ev.subsystem, "drm") != 0)
				continue;       /* usb, block, ... not ours */
			if (!uevent_is_for_device(&ev, st.st_rdev)) {
				printf("[drm] event for another DRM node "
				       "(DEVNAME=%s), ignored\n",
				       ev.devname ? ev.devname : "?");
				continue;
			}

			printf("\n[drm] SEQNUM=%s ACTION=%s DEVNAME=%s\n",
			       ev.seqnum ? ev.seqnum : "?",
			       ev.action ? ev.action : "?",
			       ev.devname ? ev.devname : "?");
			if (ev.lease)
				printf("  LEASE=%s (lease change, not a hotplug)\n",
				       ev.lease);
			if (!ev.hotplug || strcmp(ev.hotplug, "1") != 0) {
				if (!ev.lease)
					printf("  (no HOTPLUG=1 -- not a hotplug event)\n");
				continue;
			}
			n_hotplug++;
			printf("  HOTPLUG=1");
			if (ev.connector)
				printf("  CONNECTOR=%s", ev.connector);
			if (ev.property)
				printf("  PROPERTY=%s", ev.property);
			if (!ev.connector)
				printf("  (device-wide: re-probe all connectors)");
			printf("\n");
		}

		int next = cur ^ 1;
		if (collect_topology(fd, probe, false, &snap[next]) < 0)
			continue;

		if (ev.connector && ev.property) {
			uint32_t cid = (uint32_t)strtoul(ev.connector, NULL, 10);
			uint32_t pid = (uint32_t)strtoul(ev.property, NULL, 10);
			print_connector_property(fd, cid, pid);
		}

		int changes = diff_topology(&snap[cur], &snap[next]);
		if (changes == 0)
			printf("  (no connector change visible to this client)\n");
		cur = next;
		fflush(stdout);
	}

	printf("\nStopped. %lu uevent(s) seen, %lu DRM hotplug event(s).\n",
	       n_events, n_hotplug);
	close(nl);
	return 0;
}

/* ============================================================
 * main
 * ============================================================ */
static void usage(const char *prog)
{
	printf("Usage: %s [options]\n"
	       "\n"
	       "Read-only KMS topology inspector and hotplug monitor.\n"
	       "It never performs a modeset.\n"
	       "\n"
	       "Modes:\n"
	       "  --topology      Print connector/encoder/CRTC/plane routing\n"
	       "                  and a valid multi-display assignment (default)\n"
	       "  --monitor       Listen for DRM hotplug uevents (raw netlink,\n"
	       "                  no libudev) and print connector changes\n"
	       "\n"
	       "Options:\n"
	       "  -d <device>     DRM device node (default /dev/dri/card0)\n"
	       "  --no-probe      Use drmModeGetConnectorCurrent() instead of\n"
	       "                  drmModeGetConnector(): no forced probe / EDID read\n"
	       "  --drop-master   Give up DRM master right after open, so another\n"
	       "                  KMS client can run meanwhile (forced probes are\n"
	       "                  then demoted to read-only queries by the kernel)\n"
	       "  -h, --help      Show this help\n",
	       prog);
}

int main(int argc, char **argv)
{
	const char *dev_path = "/dev/dri/card0";
	bool monitor = false, probe = true, drop_master = false;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--topology")) {
			monitor = false;
		} else if (!strcmp(argv[i], "--monitor")) {
			monitor = true;
		} else if (!strcmp(argv[i], "--no-probe")) {
			probe = false;
		} else if (!strcmp(argv[i], "--drop-master")) {
			drop_master = true;
		} else if (!strcmp(argv[i], "-d")) {
			if (i + 1 >= argc) {
				fprintf(stderr, "-d needs a device path\n");
				usage(argv[0]);
				return 1;
			}
			dev_path = argv[++i];
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage(argv[0]);
			return 0;
		} else {
			fprintf(stderr, "Unknown argument: %s\n", argv[i]);
			usage(argv[0]);
			return 1;
		}
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;   /* no SA_RESTART: poll() must see EINTR */
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	int fd = open(dev_path, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", dev_path, strerror(errno));
		return 1;
	}

	/*
	 * The first opener of a card node with no current master becomes
	 * master automatically (drm_master_open()).  That matters here:
	 * only the master's GETCONNECTOR triggers a forced probe.
	 */
	if (drop_master) {
		if (drmDropMaster(fd) != 0)
			fprintf(stderr, "drmDropMaster: %s (were we master?)\n",
				strerror(errno));
	}
	bool master = is_drm_master(fd);
	printf("Device %s: this fd %s DRM master\n", dev_path,
	       master ? "IS" : "is NOT");
	if (probe && !master)
		printf("  note: drmModeGetConnector() will NOT force a probe "
		       "for a non-master fd (kernel >= v5.15)\n");

	/*
	 * Universal planes: without this cap drmModeGetPlaneResources()
	 * hides PRIMARY and CURSOR planes.  It only changes what we can
	 * see; it does not modify any hardware state.
	 */
	if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0)
		fprintf(stderr, "warning: DRM_CLIENT_CAP_UNIVERSAL_PLANES: %s\n",
			strerror(errno));

	int ret = 0;
	if (monitor) {
		ret = run_monitor(fd, probe) < 0 ? 1 : 0;
	} else {
		static struct topology topo;
		if (collect_topology(fd, probe, true, &topo) < 0)
			ret = 1;
		else
			print_topology(&topo, probe);
	}

	close(fd);
	return ret;
}
