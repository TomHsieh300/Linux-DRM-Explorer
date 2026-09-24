#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

/* ============================================================
 * DRM Plane Properties & Hardware Blending Demo
 *
 * Experiment 05 described hardware composition in words: several planes
 * are stacked by the display controller and blended on the fly, without a
 * GPU pass.  This program turns that into code.  The knobs the kernel
 * gives userspace for composition are plain KMS properties on the plane
 * object:
 *
 *   zpos               -- stacking order (range)
 *   alpha              -- plane-wide opacity, 0 .. 0xffff (range)
 *   pixel blend mode   -- how per-pixel alpha is interpreted (enum)
 *   rotation           -- rotate / reflect step (bitmask)
 *   (scaling)          -- no property: SRC_W/H != CRTC_W/H, and only
 *                         an atomic TEST_ONLY commit can tell you
 *                         whether the hardware accepts it
 *
 * None of these is guaranteed to exist.  Each one is optional and created
 * by the driver (drm_blend.c helpers), so everything here is discovered
 * at runtime by *name* and every change is validated with
 * DRM_MODE_ATOMIC_TEST_ONLY before it is committed.
 *
 * Progression from previous demos:
 *   modeset-double-buffer.c    -- GEM / KMS basics
 *   drm-vblank-sync-demo.c     -- vblank sync, tearing
 *   drm-atomic-demo.c          -- atomic commit, properties, planes
 *   drm-dmabuf-fence.c         -- DMA-BUF sharing, implicit/explicit fence
 *   drm-plane-props.c          -- composition properties, HW blending
 *
 * Runnable modes:
 *   --list  (default)  For every plane usable on the chosen CRTC, print
 *                      type, possible_crtcs and the composition
 *                      properties (kind, range / enum / bitmask names,
 *                      immutable flag, current value), then probe
 *                      scaling with TEST_ONLY commits.
 *   --demo             Atomic modeset + full-screen primary + one
 *                      semi-transparent overlay, animated:
 *                        phase 1: alpha sweep 0 -> max
 *                        phase 2: zpos swap (overlay above / below)
 *                        phase 3: overlay scaling (0.5x .. 1.5x)
 *                        phase 4: cycle "pixel blend mode" values
 *                      Rejected features are reported and skipped.
 *                      The original KMS state is restored on exit.
 *
 * Target: RK3588 / VOP2 with Ubuntu Lite (no compositor).
 * Not yet verified on hardware.
 * ============================================================ */

/* ============================================================
 * Portability fallbacks
 *
 * The libdrm version on the target board is unknown.  The property type
 * encoding below is kernel UAPI (include/uapi/drm/drm_mode.h) and has
 * been stable for a long time, but older libdrm copies of drm_mode.h
 * may lack some of the names, so define them if missing.  We avoid the
 * newer libdrm inline helper drmModeGetPropertyType() for the same
 * reason and decode the flags ourselves.
 * ============================================================ */
#ifndef DRM_MODE_PROP_EXTENDED_TYPE
#define DRM_MODE_PROP_EXTENDED_TYPE 0x0000ffc0
#endif
#ifndef DRM_MODE_PROP_TYPE
#define DRM_MODE_PROP_TYPE(n) ((n) << 6)
#endif
#ifndef DRM_MODE_PROP_OBJECT
#define DRM_MODE_PROP_OBJECT DRM_MODE_PROP_TYPE(1)
#endif
#ifndef DRM_MODE_PROP_SIGNED_RANGE
#define DRM_MODE_PROP_SIGNED_RANGE DRM_MODE_PROP_TYPE(2)
#endif
#ifndef DRM_MODE_PROP_ATOMIC
#define DRM_MODE_PROP_ATOMIC 0x80000000
#endif
#ifndef DRM_MODE_PROP_LEGACY_TYPE
#define DRM_MODE_PROP_LEGACY_TYPE (DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ENUM | \
				   DRM_MODE_PROP_BLOB | DRM_MODE_PROP_BITMASK)
#endif
#ifndef DRM_PLANE_TYPE_OVERLAY
#define DRM_PLANE_TYPE_OVERLAY 0
#define DRM_PLANE_TYPE_PRIMARY 1
#define DRM_PLANE_TYPE_CURSOR  2
#endif

#define MAX_PLANES     64
#define MAX_ENUMS      16
#define PROBE_SIZE     64      /* source size used by the scaling probe */
#define FLIP_TIMEOUT_S 1

/* ============================================================
 * Feature bits used by the demo's "test, and drop what fails" logic.
 * Scaling has no property of its own -- it is simply a CRTC_W/H that
 * differs from SRC_W/H -- but from the point of view of TEST_ONLY it is
 * one more thing the driver may refuse, so it gets a bit too.
 * ============================================================ */
enum feature {
	F_ALPHA = 1u << 0,
	F_ZPOS  = 1u << 1,
	F_SCALE = 1u << 2,
	F_BLEND = 1u << 3,
};

static const struct {
	unsigned bit;
	const char *name;
} feature_names[] = {
	{ F_ALPHA, "alpha" },
	{ F_ZPOS,  "zpos" },
	{ F_SCALE, "scaling" },
	{ F_BLEND, "pixel blend mode" },
};

static const char *feature_name(unsigned bit)
{
	for (size_t i = 0; i < sizeof(feature_names) / sizeof(feature_names[0]); i++)
		if (feature_names[i].bit == bit)
			return feature_names[i].name;
	return "?";
}

/* ============================================================
 * Property bookkeeping
 *
 * A struct prop records one property as found on one object.  id == 0
 * means "this object does not expose the property" -- the normal case
 * for optional properties, not an error.  'value' is the value read at
 * startup; the demo uses it to restore the original state on exit.
 * ============================================================ */
struct prop {
	uint32_t id;
	uint32_t flags;
	uint64_t value;
	uint64_t min, max;  /* only meaningful for (signed) range kinds */
};

struct enum_entry {
	uint64_t value;
	char name[DRM_PROP_NAME_LEN + 1];
};

struct plane_info {
	uint32_t id;
	uint64_t type;           /* DRM_PLANE_TYPE_* from the "type" enum */
	bool has_type;
	uint32_t possible_crtcs;
	uint32_t n_formats;
	bool has_argb8888, has_xrgb8888;

	/* Standard atomic geometry (only visible with DRM_CLIENT_CAP_ATOMIC) */
	struct prop fb_id, crtc_id;
	struct prop crtc_x, crtc_y, crtc_w, crtc_h;
	struct prop src_x, src_y, src_w, src_h;

	/* Optional composition properties (drm_blend.c) */
	struct prop zpos, alpha, blend, rotation;

	struct enum_entry blend_enums[MAX_ENUMS];
	int n_blend_enums;
	bool has_rot0;           /* "rotate-0" present in the bitmask */
	uint64_t rot0_value;     /* 1 << bit index of "rotate-0" */
};

static uint32_t prop_kind(uint32_t flags)
{
	return flags & (DRM_MODE_PROP_LEGACY_TYPE | DRM_MODE_PROP_EXTENDED_TYPE);
}

static bool kind_is_range(uint32_t flags)
{
	uint32_t k = prop_kind(flags);
	return k == DRM_MODE_PROP_RANGE || k == DRM_MODE_PROP_SIGNED_RANGE;
}

static const char *kind_name(uint32_t flags)
{
	switch (prop_kind(flags)) {
	case DRM_MODE_PROP_RANGE:        return "range";
	case DRM_MODE_PROP_SIGNED_RANGE: return "signed range";
	case DRM_MODE_PROP_ENUM:         return "enum";
	case DRM_MODE_PROP_BITMASK:      return "bitmask";
	case DRM_MODE_PROP_BLOB:         return "blob";
	case DRM_MODE_PROP_OBJECT:       return "object";
	default:                         return "unknown";
	}
}

static bool prop_present(const struct prop *p)
{
	return p->id != 0;
}

/* A property can be written only if it exists and is not IMMUTABLE.  The
 * kernel rejects any attempt to set an immutable property with -EINVAL
 * (drm_property_change_valid_get()), so never put one in a request. */
static bool prop_mutable(const struct prop *p)
{
	return p->id != 0 && !(p->flags & DRM_MODE_PROP_IMMUTABLE);
}

/* Fill a struct prop from a drmModePropertyRes + its current value. */
static void fill_prop(struct prop *dst, const drmModePropertyRes *pr,
		      uint64_t value)
{
	dst->id    = pr->prop_id;
	dst->flags = pr->flags;
	dst->value = value;
	if (kind_is_range(pr->flags) && pr->count_values >= 2) {
		dst->min = pr->values[0];
		dst->max = pr->values[1];
	}
}

/* ============================================================
 * find_prop - look up one property by name on any KMS object.
 * Returns true and fills *out if found; false otherwise.
 * ============================================================ */
static bool find_prop(int fd, uint32_t obj_id, uint32_t obj_type,
		      const char *name, struct prop *out)
{
	bool found = false;
	drmModeObjectProperties *op;

	memset(out, 0, sizeof(*out));
	op = drmModeObjectGetProperties(fd, obj_id, obj_type);
	if (!op)
		return false;

	for (uint32_t i = 0; i < op->count_props && !found; i++) {
		drmModePropertyRes *pr = drmModeGetProperty(fd, op->props[i]);
		if (!pr)
			continue;
		if (strcmp(pr->name, name) == 0) {
			fill_prop(out, pr, op->prop_values[i]);
			found = true;
		}
		drmModeFreeProperty(pr);
	}
	drmModeFreeObjectProperties(op);
	return found;
}

/* Map a plane property name to its slot in struct plane_info. */
static struct prop *plane_prop_slot(struct plane_info *pi, const char *name)
{
	static const struct {
		const char *name;
		size_t off;
	} tbl[] = {
		{ "FB_ID",            offsetof(struct plane_info, fb_id) },
		{ "CRTC_ID",          offsetof(struct plane_info, crtc_id) },
		{ "CRTC_X",           offsetof(struct plane_info, crtc_x) },
		{ "CRTC_Y",           offsetof(struct plane_info, crtc_y) },
		{ "CRTC_W",           offsetof(struct plane_info, crtc_w) },
		{ "CRTC_H",           offsetof(struct plane_info, crtc_h) },
		{ "SRC_X",            offsetof(struct plane_info, src_x) },
		{ "SRC_Y",            offsetof(struct plane_info, src_y) },
		{ "SRC_W",            offsetof(struct plane_info, src_w) },
		{ "SRC_H",            offsetof(struct plane_info, src_h) },
		{ "zpos",             offsetof(struct plane_info, zpos) },
		{ "alpha",            offsetof(struct plane_info, alpha) },
		{ "pixel blend mode", offsetof(struct plane_info, blend) },
		{ "rotation",         offsetof(struct plane_info, rotation) },
	};

	for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
		if (strcmp(tbl[i].name, name) == 0)
			return (struct prop *)((char *)pi + tbl[i].off);
	return NULL;
}

/* ============================================================
 * load_plane - snapshot one plane: formats, type and all properties
 * we care about, including their *current* values.
 * ============================================================ */
static int load_plane(int fd, uint32_t plane_id, struct plane_info *pi)
{
	drmModePlane *pl;
	drmModeObjectProperties *op;

	memset(pi, 0, sizeof(*pi));
	pi->id = plane_id;

	pl = drmModeGetPlane(fd, plane_id);
	if (!pl) {
		fprintf(stderr, "drmModeGetPlane(%u): %s\n", plane_id,
			strerror(errno));
		return -1;
	}
	pi->possible_crtcs = pl->possible_crtcs;
	pi->n_formats = pl->count_formats;
	for (uint32_t i = 0; i < pl->count_formats; i++) {
		if (pl->formats[i] == DRM_FORMAT_ARGB8888)
			pi->has_argb8888 = true;
		if (pl->formats[i] == DRM_FORMAT_XRGB8888)
			pi->has_xrgb8888 = true;
	}
	drmModeFreePlane(pl);

	op = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
	if (!op) {
		fprintf(stderr, "drmModeObjectGetProperties(plane %u): %s\n",
			plane_id, strerror(errno));
		return -1;
	}

	for (uint32_t i = 0; i < op->count_props; i++) {
		drmModePropertyRes *pr = drmModeGetProperty(fd, op->props[i]);
		struct prop *slot;

		if (!pr)
			continue;

		if (strcmp(pr->name, "type") == 0) {
			pi->type = op->prop_values[i];
			pi->has_type = true;
		}

		slot = plane_prop_slot(pi, pr->name);
		if (slot)
			fill_prop(slot, pr, op->prop_values[i]);

		/*
		 * Enum values are looked up by NAME, never assumed.  The
		 * numeric values behind "Pre-multiplied"/"Coverage"/"None"
		 * are kernel-internal constants (include/drm/drm_blend.h,
		 * not UAPI); the enum list is the contract.
		 */
		if (strcmp(pr->name, "pixel blend mode") == 0 &&
		    prop_kind(pr->flags) == DRM_MODE_PROP_ENUM) {
			for (int e = 0; e < pr->count_enums &&
			     pi->n_blend_enums < MAX_ENUMS; e++) {
				struct enum_entry *ee =
					&pi->blend_enums[pi->n_blend_enums++];
				ee->value = pr->enums[e].value;
				snprintf(ee->name, sizeof(ee->name), "%s",
					 pr->enums[e].name);
			}
		}

		/*
		 * For BITMASK properties the enum "value" is a bit INDEX, not
		 * a mask (the kernel validates with 1ULL << values[i]).  So
		 * "rotate-0" is usually value 0, meaning mask 0x1.
		 */
		if (strcmp(pr->name, "rotation") == 0 &&
		    prop_kind(pr->flags) == DRM_MODE_PROP_BITMASK) {
			for (int e = 0; e < pr->count_enums; e++) {
				if (strcmp(pr->enums[e].name, "rotate-0") == 0 &&
				    pr->enums[e].value < 64) {
					pi->has_rot0 = true;
					pi->rot0_value = 1ULL << pr->enums[e].value;
				}
			}
		}

		drmModeFreeProperty(pr);
	}
	drmModeFreeObjectProperties(op);
	return 0;
}

static const char *plane_type_name(const struct plane_info *pi)
{
	if (!pi->has_type)
		return "?";
	switch (pi->type) {
	case DRM_PLANE_TYPE_PRIMARY: return "Primary";
	case DRM_PLANE_TYPE_OVERLAY: return "Overlay";
	case DRM_PLANE_TYPE_CURSOR:  return "Cursor";
	default:                     return "?";
	}
}

static bool plane_has_atomic_geometry(const struct plane_info *pi)
{
	return prop_present(&pi->fb_id)  && prop_present(&pi->crtc_id) &&
	       prop_present(&pi->crtc_x) && prop_present(&pi->crtc_y) &&
	       prop_present(&pi->crtc_w) && prop_present(&pi->crtc_h) &&
	       prop_present(&pi->src_x)  && prop_present(&pi->src_y) &&
	       prop_present(&pi->src_w)  && prop_present(&pi->src_h);
}

/* ============================================================
 * Small pure helpers (exercised by --selftest)
 * ============================================================ */

/* SRC_* properties are 16.16 fixed point; CRTC_* are integer pixels. */
static uint64_t to_fp16(uint32_t px)
{
	return (uint64_t)px << 16;
}

static int parse_u32(const char *s, uint32_t *out)
{
	char *end;
	unsigned long long v;

	if (!s || !*s)
		return -1;
	errno = 0;
	v = strtoull(s, &end, 0);
	if (errno || *end || v > UINT32_MAX)
		return -1;
	*out = (uint32_t)v;
	return 0;
}

/*
 * The only portable way to learn about errors from drmModeAtomicCommit():
 * libdrm 2.4.125 returns -errno, but we also look at errno itself so the
 * code does not depend on that detail of a particular libdrm version.
 */
static int atomic_commit_err(int fd, drmModeAtomicReq *req, uint32_t flags,
			     void *user_data)
{
	int ret;

	errno = 0;
	ret = drmModeAtomicCommit(fd, req, flags, user_data);
	if (ret == 0)
		return 0;
	if (errno)
		return errno;
	return ret < 0 ? -ret : EIO;
}

/*
 * zpos arrangement.
 *
 * Two orders are needed for the swap: overlay above primary, and
 * overlay below primary.  With mutable ranges [pmin..pmax] (primary)
 * and [omin..omax] (overlay):
 *
 *   overlay on top :  primary = pmin, overlay = omax   (needs omax > pmin)
 *   overlay below  :  primary = pmax, overlay = omin   (needs pmax > omin)
 *
 * An immutable zpos is a range with min == max == its fixed value.
 * Equal zpos values are legal but the order between them is undefined
 * (drm_blend.c), so we insist on strict inequality.
 */
struct zrange {
	bool present;
	uint64_t min, max;
};

static bool plan_zpos(const struct zrange *p, const struct zrange *o,
		      uint64_t top[2], uint64_t bottom[2])
{
	if (!p->present || !o->present)
		return false;
	if (!(o->max > p->min) || !(p->max > o->min))
		return false;
	top[0] = p->min;    top[1] = o->max;
	bottom[0] = p->max; bottom[1] = o->min;
	return true;
}

static struct zrange zrange_of(const struct prop *z)
{
	struct zrange r = { .present = prop_present(z) };

	if (!r.present)
		return r;
	if (z->flags & DRM_MODE_PROP_IMMUTABLE) {
		r.min = r.max = z->value;
	} else {
		r.min = z->min;
		r.max = z->max;
	}
	return r;
}

/* ============================================================
 * Frame planner (pure): what should the overlay look like at frame N?
 * ============================================================ */
enum phase { PH_ALPHA, PH_ZPOS, PH_SCALE, PH_BLEND, PH_COUNT };

static const unsigned phase_feature[PH_COUNT] = {
	[PH_ALPHA] = F_ALPHA,
	[PH_ZPOS]  = F_ZPOS,
	[PH_SCALE] = F_SCALE,
	[PH_BLEND] = F_BLEND,
};

static const char *phase_title[PH_COUNT] = {
	[PH_ALPHA] = "alpha sweep: plane alpha 0 -> max",
	[PH_ZPOS]  = "zpos swap: overlay above / below the primary every second",
	[PH_SCALE] = "scaling: overlay CRTC size 0.5x .. 1.5x of its SRC size",
	[PH_BLEND] = "pixel blend mode: cycle through the advertised enum values",
};

struct plan_params {
	uint64_t alpha_max;
	uint32_t phase_len;      /* frames per phase */
	uint32_t toggle_len;     /* frames between zpos toggles */
	uint32_t src_w, src_h;   /* overlay framebuffer size */
	uint64_t blend_vals[MAX_ENUMS];
	int n_blend;
	uint64_t blend_default;
};

struct frame {
	int phase;               /* -1 = static (no feature enabled) */
	uint64_t alpha;
	bool overlay_top;
	int blend_idx;
	uint64_t blend;
	uint32_t dst_w, dst_h;
};

static void plan_frame(const struct plan_params *pp, unsigned enabled,
		       unsigned long frame, struct frame *f)
{
	int active[PH_COUNT];
	int n_active = 0;
	uint32_t t;

	f->phase       = -1;
	f->alpha       = pp->alpha_max;
	f->overlay_top = true;
	f->blend_idx   = -1;
	f->blend       = pp->blend_default;
	f->dst_w       = pp->src_w;
	f->dst_h       = pp->src_h;

	for (int p = 0; p < PH_COUNT; p++)
		if (enabled & phase_feature[p])
			active[n_active++] = p;
	if (n_active == 0 || pp->phase_len < 2)
		return;

	f->phase = active[(frame / pp->phase_len) % (unsigned long)n_active];
	t = (uint32_t)(frame % pp->phase_len);

	switch (f->phase) {
	case PH_ALPHA:
		f->alpha = pp->alpha_max * t / (pp->phase_len - 1);
		break;
	case PH_ZPOS:
		f->overlay_top = ((t / (pp->toggle_len ? pp->toggle_len : 1)) % 2) == 0;
		break;
	case PH_SCALE: {
		/* Triangle wave 0..1000..0 -> scale factor 0.5 .. 1.5 */
		uint32_t half = pp->phase_len / 2;
		uint64_t tri = t < half ? (uint64_t)t * 1000 / half
					: (uint64_t)(pp->phase_len - t) * 1000 /
					  (pp->phase_len - half);
		uint64_t s = 500 + (tri > 1000 ? 1000 : tri);
		uint32_t w = (uint32_t)(pp->src_w * s / 1000) & ~1u;
		uint32_t h = (uint32_t)(pp->src_h * s / 1000) & ~1u;
		f->dst_w = w < 4 ? 4 : w;
		f->dst_h = h < 4 ? 4 : h;
		break;
	}
	case PH_BLEND:
		if (pp->n_blend > 0) {
			f->blend_idx = (int)((uint64_t)t * (uint64_t)pp->n_blend /
					     pp->phase_len);
			f->blend = pp->blend_vals[f->blend_idx];
		}
		break;
	default:
		break;
	}
}

/* ============================================================
 * Dumb framebuffers
 *
 * Same GEM "dumb buffer" technique as the earlier demos, but registered
 * with drmModeAddFB2() so we can pick the fourcc explicitly: ARGB8888 for
 * the overlay (it carries per-pixel alpha), XRGB8888 for the primary.
 * ============================================================ */
struct dumb_fb {
	uint32_t w, h, pitch, handle, fb_id, format;
	uint64_t size;
	void *map;
};

static void dumb_fb_destroy(int fd, struct dumb_fb *fb)
{
	if (fb->map && fb->map != MAP_FAILED)
		munmap(fb->map, fb->size);
	if (fb->fb_id)
		drmModeRmFB(fd, fb->fb_id);
	if (fb->handle) {
		struct drm_mode_destroy_dumb d = { .handle = fb->handle };
		drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
	}
	memset(fb, 0, sizeof(*fb));
}

static int dumb_fb_create(int fd, uint32_t w, uint32_t h, uint32_t format,
			  struct dumb_fb *fb)
{
	struct drm_mode_create_dumb create = { .width = w, .height = h, .bpp = 32 };
	struct drm_mode_map_dumb map = { 0 };
	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };

	memset(fb, 0, sizeof(*fb));
	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
		fprintf(stderr, "CREATE_DUMB %ux%u: %s\n", w, h, strerror(errno));
		return -1;
	}
	fb->w = w;
	fb->h = h;
	fb->pitch = create.pitch;
	fb->size = create.size;
	fb->handle = create.handle;
	fb->format = format;

	handles[0] = fb->handle;
	pitches[0] = fb->pitch;
	if (drmModeAddFB2(fd, w, h, format, handles, pitches, offsets,
			  &fb->fb_id, 0)) {
		fprintf(stderr, "drmModeAddFB2 %ux%u: %s\n", w, h, strerror(errno));
		fb->fb_id = 0;
		goto fail;
	}

	map.handle = fb->handle;
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
		fprintf(stderr, "MAP_DUMB: %s\n", strerror(errno));
		goto fail;
	}
	fb->map = mmap(NULL, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED,
		       fd, map.offset);
	if (fb->map == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		fb->map = NULL;
		goto fail;
	}
	memset(fb->map, 0, fb->size);
	return 0;

fail:
	dumb_fb_destroy(fd, fb);
	return -1;
}

static inline void put_px(struct dumb_fb *fb, uint32_t x, uint32_t y, uint32_t v)
{
	uint32_t *row = (uint32_t *)((uint8_t *)fb->map + (size_t)y * fb->pitch);
	row[x] = v;
}

/*
 * Primary: vertical colour bars in the top two thirds and a grey
 * checkerboard below.  Both make transparency easy to see: whatever
 * shows through the overlay is obviously "background".
 */
static void draw_primary(struct dumb_fb *fb)
{
	static const uint32_t bars[8] = {
		0xffffff, 0xffff00, 0x00ffff, 0x00ff00,
		0xff00ff, 0xff0000, 0x0000ff, 0x000000,
	};
	uint32_t split = fb->h * 2 / 3;

	for (uint32_t y = 0; y < fb->h; y++) {
		for (uint32_t x = 0; x < fb->w; x++) {
			uint32_t c;
			if (y < split)
				c = bars[(x * 8) / fb->w];
			else
				c = (((x / 32) + (y / 32)) & 1) ? 0x808080 : 0x404040;
			put_px(fb, x, y, 0xff000000u | c);
		}
	}
}

/*
 * Overlay: orange rectangle whose per-pixel alpha ramps from 0x40 (left)
 * to 0xff (right), with a 6-pixel opaque white border.
 *
 * The colour is stored PRE-MULTIPLIED (rgb already scaled by alpha),
 * matching the kernel's default blend mode "Pre-multiplied".  When the
 * demo later switches to "Coverage" or "None", the same bytes are
 * interpreted differently -- that is the point of the blend phase.
 *
 * For an XRGB8888 overlay (no alpha channel) the colour is written
 * unscaled; plane alpha is then the only transparency available.
 */
static void draw_overlay(struct dumb_fb *fb, bool has_alpha)
{
	const uint32_t R = 0xff, G = 0x80, B = 0x00, border = 6;

	for (uint32_t y = 0; y < fb->h; y++) {
		for (uint32_t x = 0; x < fb->w; x++) {
			uint32_t a, r, g, b;

			if (x < border || y < border ||
			    x >= fb->w - border || y >= fb->h - border) {
				put_px(fb, x, y, 0xffffffffu);
				continue;
			}
			a = has_alpha ? 0x40 + (0xff - 0x40) * x / (fb->w - 1) : 0xff;
			r = R * a / 0xff;
			g = G * a / 0xff;
			b = B * a / 0xff;
			put_px(fb, x, y, (a << 24) | (r << 16) | (g << 8) | b);
		}
	}
}

/* ============================================================
 * Choosing the CRTC
 *
 * possible_crtcs (on encoders and planes) is a bitmask of CRTC *indices*
 * into drmModeRes.crtcs[], not of CRTC object IDs.
 * ============================================================ */
static int crtc_index(const drmModeRes *res, uint32_t crtc_id)
{
	for (int i = 0; i < res->count_crtcs; i++)
		if (res->crtcs[i] == crtc_id)
			return i;
	return -1;
}

static uint32_t connector_crtc_mask(int fd, const drmModeConnector *conn)
{
	uint32_t mask = 0;

	for (int i = 0; i < conn->count_encoders; i++) {
		drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[i]);
		if (!enc)
			continue;
		mask |= enc->possible_crtcs;
		drmModeFreeEncoder(enc);
	}
	return mask;
}

static uint32_t connector_current_crtc(int fd, const drmModeConnector *conn)
{
	uint32_t crtc = 0;
	drmModeEncoder *enc;

	if (!conn->encoder_id)
		return 0;
	enc = drmModeGetEncoder(fd, conn->encoder_id);
	if (enc) {
		crtc = enc->crtc_id;
		drmModeFreeEncoder(enc);
	}
	return crtc;
}

/* For --list: override, else the CRTC of the first connected connector. */
static int choose_list_crtc(int fd, const drmModeRes *res, uint32_t override,
			    uint32_t *crtc_id, const char **why)
{
	if (override) {
		if (crtc_index(res, override) < 0) {
			fprintf(stderr, "CRTC %u does not exist. Available:", override);
			for (int i = 0; i < res->count_crtcs; i++)
				fprintf(stderr, " %u", res->crtcs[i]);
			fprintf(stderr, "\n");
			return -1;
		}
		*crtc_id = override;
		*why = "selected with -c";
		return 0;
	}

	for (int i = 0; i < res->count_connectors; i++) {
		drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
		uint32_t c = 0, mask;

		if (!conn)
			continue;
		if (conn->connection == DRM_MODE_CONNECTED) {
			c = connector_current_crtc(fd, conn);
			if (c) {
				*why = "currently drives a connected connector";
			} else {
				mask = connector_crtc_mask(fd, conn);
				for (int k = 0; k < res->count_crtcs; k++) {
					if (mask & (1u << k)) {
						c = res->crtcs[k];
						*why = "first CRTC usable by a connected connector";
						break;
					}
				}
			}
		}
		drmModeFreeConnector(conn);
		if (c) {
			*crtc_id = c;
			return 0;
		}
	}

	if (res->count_crtcs > 0) {
		*crtc_id = res->crtcs[0];
		*why = "no connected connector found, using the first CRTC";
		return 0;
	}
	fprintf(stderr, "No CRTCs\n");
	return -1;
}

/* ============================================================
 * --list : property report
 * ============================================================ */
static void print_possible_crtcs(const drmModeRes *res, uint32_t mask)
{
	bool first = true;

	printf("possible_crtcs=0x%x (CRTC", mask);
	for (int i = 0; i < res->count_crtcs && i < 32; i++) {
		if (mask & (1u << i)) {
			printf("%s%u", first ? " " : ", ", res->crtcs[i]);
			first = false;
		}
	}
	printf("%s)", first ? " none" : "");
}

/*
 * Print one composition property generically: whatever kind the driver
 * chose, print its limits / names, whether it is immutable, and the
 * current value decoded against those names.
 */
static void print_comp_prop(int fd, const drmModeObjectProperties *op,
			    const char *name)
{
	drmModePropertyRes *pr = NULL;
	uint64_t cur = 0;
	uint32_t k;

	for (uint32_t i = 0; i < op->count_props; i++) {
		drmModePropertyRes *p = drmModeGetProperty(fd, op->props[i]);
		if (!p)
			continue;
		if (strcmp(p->name, name) == 0) {
			pr = p;
			cur = op->prop_values[i];
			break;
		}
		drmModeFreeProperty(p);
	}

	printf("    %-17s: ", name);
	if (!pr) {
		printf("not exposed by this driver/plane\n");
		return;
	}

	k = prop_kind(pr->flags);
	printf("%s", kind_name(pr->flags));

	if (k == DRM_MODE_PROP_RANGE && pr->count_values >= 2) {
		printf(" [%" PRIu64 " .. %" PRIu64 "]", pr->values[0], pr->values[1]);
		if (pr->values[1] > 0xff)
			printf(" (max 0x%" PRIx64 ")", pr->values[1]);
	} else if (k == DRM_MODE_PROP_SIGNED_RANGE && pr->count_values >= 2) {
		printf(" [%" PRId64 " .. %" PRId64 "]",
		       (int64_t)pr->values[0], (int64_t)pr->values[1]);
	} else if (k == DRM_MODE_PROP_ENUM) {
		printf(" {");
		for (int e = 0; e < pr->count_enums; e++)
			printf("%s\"%s\"=%" PRIu64, e ? ", " : "",
			       pr->enums[e].name, (uint64_t)pr->enums[e].value);
		printf("}");
	} else if (k == DRM_MODE_PROP_BITMASK) {
		printf(" {");
		for (int e = 0; e < pr->count_enums; e++)
			printf("%s%s=bit%" PRIu64, e ? ", " : "",
			       pr->enums[e].name, (uint64_t)pr->enums[e].value);
		printf("}");
	}

	printf(", %s", (pr->flags & DRM_MODE_PROP_IMMUTABLE) ? "IMMUTABLE" : "mutable");

	/* Decode the current value */
	if (k == DRM_MODE_PROP_ENUM) {
		const char *n = "?";
		for (int e = 0; e < pr->count_enums; e++)
			if (pr->enums[e].value == cur)
				n = pr->enums[e].name;
		printf(", current=%" PRIu64 " (\"%s\")\n", cur, n);
	} else if (k == DRM_MODE_PROP_BITMASK) {
		bool first = true;
		printf(", current=0x%" PRIx64 " [", cur);
		for (int e = 0; e < pr->count_enums; e++) {
			if (pr->enums[e].value < 64 &&
			    (cur & (1ULL << pr->enums[e].value))) {
				printf("%s%s", first ? "" : "|", pr->enums[e].name);
				first = false;
			}
		}
		printf("]\n");
	} else if (k == DRM_MODE_PROP_SIGNED_RANGE) {
		printf(", current=%" PRId64 "\n", (int64_t)cur);
	} else {
		printf(", current=%" PRIu64, cur);
		if (strcmp(name, "alpha") == 0 && pr->count_values >= 2 &&
		    pr->values[1] > 0)
			printf(" (%.1f%% opaque)",
			       100.0 * (double)cur / (double)pr->values[1]);
		printf("\n");
	}
	drmModeFreeProperty(pr);
}

static void print_other_props(int fd, const drmModeObjectProperties *op)
{
	static const char *const shown[] = {
		"zpos", "alpha", "pixel blend mode", "rotation",
	};
	int col = 0;

	printf("    other properties : ");
	for (uint32_t i = 0; i < op->count_props; i++) {
		drmModePropertyRes *p = drmModeGetProperty(fd, op->props[i]);
		bool skip = false;

		if (!p)
			continue;
		for (size_t s = 0; s < sizeof(shown) / sizeof(shown[0]); s++)
			if (strcmp(p->name, shown[s]) == 0)
				skip = true;
		if (!skip) {
			if (col && col % 6 == 0)
				printf(",\n                       ");
			else if (col)
				printf(", ");
			printf("%s", p->name);
			col++;
		}
		drmModeFreeProperty(p);
	}
	printf("%s\n", col ? "" : "(none)");
}

/* One TEST_ONLY commit that places 'pi' on 'crtc_id' with SRC sw x sh
 * scaled to CRTC dw x dh.  Returns 0 if accepted, else a positive errno. */
static int test_plane_rect(int fd, const struct plane_info *pi, uint32_t crtc_id,
			   uint32_t fb_id, uint32_t sw, uint32_t sh,
			   uint32_t dw, uint32_t dh)
{
	drmModeAtomicReq *req = drmModeAtomicAlloc();
	int err;

	if (!req)
		return ENOMEM;
	drmModeAtomicAddProperty(req, pi->id, pi->fb_id.id,   fb_id);
	drmModeAtomicAddProperty(req, pi->id, pi->crtc_id.id, crtc_id);
	drmModeAtomicAddProperty(req, pi->id, pi->crtc_x.id,  0);
	drmModeAtomicAddProperty(req, pi->id, pi->crtc_y.id,  0);
	drmModeAtomicAddProperty(req, pi->id, pi->crtc_w.id,  dw);
	drmModeAtomicAddProperty(req, pi->id, pi->crtc_h.id,  dh);
	drmModeAtomicAddProperty(req, pi->id, pi->src_x.id,   0);
	drmModeAtomicAddProperty(req, pi->id, pi->src_y.id,   0);
	drmModeAtomicAddProperty(req, pi->id, pi->src_w.id,   to_fp16(sw));
	drmModeAtomicAddProperty(req, pi->id, pi->src_h.id,   to_fp16(sh));
	err = atomic_commit_err(fd, req, DRM_MODE_ATOMIC_TEST_ONLY, NULL);
	drmModeAtomicFree(req);
	return err;
}

struct probe_ctx {
	int fd;
	bool atomic_ok;
	bool crtc_active;
	uint32_t crtc_id;
	struct dumb_fb xrgb, argb;   /* created lazily */
};

/*
 * Scaling has no property: the only way to know whether a plane can
 * scale is to ask.  We do three TEST_ONLY commits with a 64x64 source:
 * 1:1 (baseline), 64->128 (2x up) and 64->32 (2x down).  Nothing is
 * applied to the hardware.  TEST_ONLY still goes through the atomic
 * ioctl, which requires DRM master.
 */
static void probe_scaling(struct probe_ctx *pc, const struct plane_info *pi)
{
	struct dumb_fb *fb = NULL;
	int base, up, down;

	printf("    scaling (probe)  : ");
	if (!pc->atomic_ok) {
		printf("not probed (atomic not available)\n");
		return;
	}
	if (!plane_has_atomic_geometry(pi)) {
		printf("not probed (plane lacks atomic SRC_*/CRTC_* properties)\n");
		return;
	}
	if (!pc->crtc_active) {
		printf("not probed (CRTC %u is off; --demo lights it and reports scaling)\n",
		       pc->crtc_id);
		return;
	}
	if (pi->crtc_id.value && pi->crtc_id.value != pc->crtc_id) {
		printf("not probed (plane is in use on CRTC %" PRIu64 ")\n",
		       pi->crtc_id.value);
		return;
	}

	if (pi->has_xrgb8888) {
		if (!pc->xrgb.fb_id &&
		    dumb_fb_create(pc->fd, PROBE_SIZE, PROBE_SIZE,
				   DRM_FORMAT_XRGB8888, &pc->xrgb) < 0) {
			printf("not probed (cannot allocate test buffer)\n");
			return;
		}
		fb = &pc->xrgb;
	} else if (pi->has_argb8888) {
		if (!pc->argb.fb_id &&
		    dumb_fb_create(pc->fd, PROBE_SIZE, PROBE_SIZE,
				   DRM_FORMAT_ARGB8888, &pc->argb) < 0) {
			printf("not probed (cannot allocate test buffer)\n");
			return;
		}
		fb = &pc->argb;
	} else {
		printf("not probed (plane supports neither XRGB8888 nor ARGB8888)\n");
		return;
	}

	base = test_plane_rect(pc->fd, pi, pc->crtc_id, fb->fb_id,
			       PROBE_SIZE, PROBE_SIZE, PROBE_SIZE, PROBE_SIZE);
	if (base == EACCES || base == EPERM) {
		printf("not probed (%s: atomic ioctl needs DRM master -- "
		       "is another KMS client running?)\n", strerror(base));
		return;
	}
	if (base) {
		printf("inconclusive (even 1:1 %ux%u placement rejected: %s)\n",
		       PROBE_SIZE, PROBE_SIZE, strerror(base));
		return;
	}
	up   = test_plane_rect(pc->fd, pi, pc->crtc_id, fb->fb_id,
			       PROBE_SIZE, PROBE_SIZE, PROBE_SIZE * 2, PROBE_SIZE * 2);
	down = test_plane_rect(pc->fd, pi, pc->crtc_id, fb->fb_id,
			       PROBE_SIZE, PROBE_SIZE, PROBE_SIZE / 2, PROBE_SIZE / 2);
	printf("1:1 ok, up 2x %s%s%s, down 1/2 %s%s%s\n",
	       up ? "REJECTED (" : "ok", up ? strerror(up) : "", up ? ")" : "",
	       down ? "REJECTED (" : "ok", down ? strerror(down) : "", down ? ")" : "");
}

static int run_list(int fd, bool atomic_ok, uint32_t crtc_override)
{
	drmModeRes *res = drmModeGetResources(fd);
	drmModePlaneRes *pres = NULL;
	drmModeCrtc *crtc = NULL;
	struct probe_ctx pc = { .fd = fd, .atomic_ok = atomic_ok };
	const char *why = "";
	uint32_t crtc_id;
	int idx, shown = 0, ret = -1;

	if (!res) {
		fprintf(stderr, "drmModeGetResources: %s\n", strerror(errno));
		return -1;
	}
	if (choose_list_crtc(fd, res, crtc_override, &crtc_id, &why) < 0)
		goto out;
	idx = crtc_index(res, crtc_id);
	if (idx < 0 || idx >= 32) {
		fprintf(stderr, "CRTC %u has no usable index\n", crtc_id);
		goto out;
	}
	pc.crtc_id = crtc_id;

	crtc = drmModeGetCrtc(fd, crtc_id);
	pc.crtc_active = crtc && crtc->mode_valid;

	printf("============================================================\n");
	printf(" Plane composition properties\n");
	printf("============================================================\n");
	printf("CRTC %u (index %d, %s): %s", crtc_id, idx, why,
	       pc.crtc_active ? "active" : "off");
	if (pc.crtc_active)
		printf(", mode \"%s\" (%ux%u@%u)", crtc->mode.name,
		       crtc->mode.hdisplay, crtc->mode.vdisplay, crtc->mode.vrefresh);
	printf("\nAtomic client cap: %s\n", atomic_ok ? "enabled" :
	       "NOT available (SRC_*/CRTC_*/FB_ID are hidden, no probes)");

	pres = drmModeGetPlaneResources(fd);
	if (!pres) {
		fprintf(stderr, "drmModeGetPlaneResources: %s\n", strerror(errno));
		goto out;
	}

	for (uint32_t i = 0; i < pres->count_planes; i++) {
		struct plane_info pi;
		drmModeObjectProperties *op;

		if (load_plane(fd, pres->planes[i], &pi) < 0)
			continue;
		if (!(pi.possible_crtcs & (1u << idx)))
			continue;
		shown++;

		printf("\nPlane %u  type=%s  ", pi.id, plane_type_name(&pi));
		print_possible_crtcs(res, pi.possible_crtcs);
		printf("\n");
		if (prop_present(&pi.crtc_id))
			printf("    bound to         : CRTC %" PRIu64 ", FB %" PRIu64 "\n",
			       pi.crtc_id.value, pi.fb_id.value);
		printf("    formats          : %u (ARGB8888 %s, XRGB8888 %s)\n",
		       pi.n_formats, pi.has_argb8888 ? "yes" : "no",
		       pi.has_xrgb8888 ? "yes" : "no");

		op = drmModeObjectGetProperties(fd, pi.id, DRM_MODE_OBJECT_PLANE);
		if (!op) {
			printf("    (cannot read properties: %s)\n", strerror(errno));
			continue;
		}
		print_comp_prop(fd, op, "zpos");
		print_comp_prop(fd, op, "alpha");
		print_comp_prop(fd, op, "pixel blend mode");
		print_comp_prop(fd, op, "rotation");
		probe_scaling(&pc, &pi);
		print_other_props(fd, op);
		drmModeFreeObjectProperties(op);
	}
	printf("\n%d plane(s) usable on CRTC %u.\n", shown, crtc_id);
	ret = 0;

out:
	dumb_fb_destroy(fd, &pc.xrgb);
	dumb_fb_destroy(fd, &pc.argb);
	if (crtc)
		drmModeFreeCrtc(crtc);
	if (pres)
		drmModeFreePlaneResources(pres);
	drmModeFreeResources(res);
	return ret;
}

/* ============================================================
 * --demo
 * ============================================================ */
static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

struct demo {
	int fd;
	uint32_t conn_id, crtc_id;
	int crtc_idx;
	drmModeModeInfo mode;
	uint32_t mode_blob;

	/* Saved (original) state for restore */
	struct prop conn_crtc;          /* connector CRTC_ID */
	struct prop crtc_active, crtc_mode_id;
	drmModeModeInfo saved_mode;
	bool saved_mode_valid;

	struct plane_info planes[MAX_PLANES];
	bool touched[MAX_PLANES];
	int n_planes;
	int primary, overlay;           /* indices into planes[], -1 = none */

	struct dumb_fb pfb, ofb;
	bool overlay_has_alpha;         /* ARGB8888 overlay buffer */

	unsigned enabled;               /* F_* still accepted by the driver */
	uint64_t z_top[2], z_bottom[2]; /* [0]=primary, [1]=overlay */
	struct plan_params pp;

	bool modeset_done;
	bool flip_pending;
};

static void flip_handler(int fd, unsigned int seq, unsigned int sec,
			 unsigned int usec, void *data)
{
	struct demo *d = data;

	(void)fd; (void)seq; (void)sec; (void)usec;
	d->flip_pending = false;
}

/* Wait for the page-flip event of the last NONBLOCK commit.
 * If 'honour_stop' is set, a signal ends the wait early (we come back
 * later with honour_stop=false to drain the event before restoring). */
static int wait_flip(struct demo *d, bool honour_stop)
{
	drmEventContext ev = {
		.version = 2,
		.page_flip_handler = flip_handler,
	};

	while (d->flip_pending) {
		fd_set fds;
		struct timeval tv = { .tv_sec = FLIP_TIMEOUT_S, .tv_usec = 0 };
		int s;

		if (honour_stop && g_stop)
			return 1;
		FD_ZERO(&fds);
		FD_SET(d->fd, &fds);
		s = select(d->fd + 1, &fds, NULL, NULL, &tv);
		if (s < 0) {
			if (errno == EINTR)
				continue;
			perror("select");
			d->flip_pending = false;
			return -1;
		}
		if (s == 0) {
			fprintf(stderr, "Timed out waiting for flip event\n");
			d->flip_pending = false;
			return -1;
		}
		if (drmHandleEvent(d->fd, &ev) != 0) {
			fprintf(stderr, "drmHandleEvent failed\n");
			d->flip_pending = false;
			return -1;
		}
	}
	return 0;
}

static struct plane_info *pri(struct demo *d)
{
	return &d->planes[d->primary];
}

static struct plane_info *ovl(struct demo *d)
{
	return &d->planes[d->overlay];
}

/* Add the full-screen primary configuration to a request. */
static void add_primary(struct demo *d, drmModeAtomicReq *req)
{
	struct plane_info *p = pri(d);
	uint32_t w = d->mode.hdisplay, h = d->mode.vdisplay;

	drmModeAtomicAddProperty(req, p->id, p->fb_id.id,   d->pfb.fb_id);
	drmModeAtomicAddProperty(req, p->id, p->crtc_id.id, d->crtc_id);
	drmModeAtomicAddProperty(req, p->id, p->crtc_x.id,  0);
	drmModeAtomicAddProperty(req, p->id, p->crtc_y.id,  0);
	drmModeAtomicAddProperty(req, p->id, p->crtc_w.id,  w);
	drmModeAtomicAddProperty(req, p->id, p->crtc_h.id,  h);
	drmModeAtomicAddProperty(req, p->id, p->src_x.id,   0);
	drmModeAtomicAddProperty(req, p->id, p->src_y.id,   0);
	drmModeAtomicAddProperty(req, p->id, p->src_w.id,   to_fp16(w));
	drmModeAtomicAddProperty(req, p->id, p->src_h.id,   to_fp16(h));
}

/*
 * Add the overlay for one frame.  The base (FB, position, size) is
 * always present; each composition property is added only if its
 * feature bit is in 'mask'.  A property left out of a request keeps its
 * previously committed value -- atomic state is persistent.
 */
static void add_overlay(struct demo *d, drmModeAtomicReq *req,
			const struct frame *f, unsigned mask)
{
	struct plane_info *o = ovl(d), *p = pri(d);
	uint32_t sw = d->ofb.w, sh = d->ofb.h;
	uint32_t dw = (mask & F_SCALE) ? f->dst_w : sw;
	uint32_t dh = (mask & F_SCALE) ? f->dst_h : sh;
	int32_t x = ((int32_t)d->mode.hdisplay - (int32_t)dw) / 2;
	int32_t y = ((int32_t)d->mode.vdisplay - (int32_t)dh) / 2;

	drmModeAtomicAddProperty(req, o->id, o->fb_id.id,   d->ofb.fb_id);
	drmModeAtomicAddProperty(req, o->id, o->crtc_id.id, d->crtc_id);
	/* CRTC_X/Y are signed: pass the two's-complement 64-bit value */
	drmModeAtomicAddProperty(req, o->id, o->crtc_x.id,  (uint64_t)(int64_t)x);
	drmModeAtomicAddProperty(req, o->id, o->crtc_y.id,  (uint64_t)(int64_t)y);
	drmModeAtomicAddProperty(req, o->id, o->crtc_w.id,  dw);
	drmModeAtomicAddProperty(req, o->id, o->crtc_h.id,  dh);
	drmModeAtomicAddProperty(req, o->id, o->src_x.id,   0);
	drmModeAtomicAddProperty(req, o->id, o->src_y.id,   0);
	drmModeAtomicAddProperty(req, o->id, o->src_w.id,   to_fp16(sw));
	drmModeAtomicAddProperty(req, o->id, o->src_h.id,   to_fp16(sh));

	if (mask & F_ALPHA)
		drmModeAtomicAddProperty(req, o->id, o->alpha.id, f->alpha);
	if (mask & F_BLEND)
		drmModeAtomicAddProperty(req, o->id, o->blend.id, f->blend);
	if (mask & F_ZPOS) {
		const uint64_t *z = f->overlay_top ? d->z_top : d->z_bottom;
		if (prop_mutable(&p->zpos))
			drmModeAtomicAddProperty(req, p->id, p->zpos.id, z[0]);
		if (prop_mutable(&o->zpos))
			drmModeAtomicAddProperty(req, o->id, o->zpos.id, z[1]);
	}
}

static int test_overlay(struct demo *d, const struct frame *f, unsigned mask)
{
	drmModeAtomicReq *req = drmModeAtomicAlloc();
	int err;

	if (!req)
		return ENOMEM;
	add_overlay(d, req, f, mask);
	err = atomic_commit_err(d->fd, req, DRM_MODE_ATOMIC_TEST_ONLY, NULL);
	drmModeAtomicFree(req);
	return err;
}

static void describe_frame_value(const struct demo *d, const struct frame *f,
				 unsigned bit, char *buf, size_t n)
{
	switch (bit) {
	case F_ALPHA:
		snprintf(buf, n, "alpha=0x%" PRIx64, f->alpha);
		break;
	case F_ZPOS: {
		const uint64_t *z = f->overlay_top ? d->z_top : d->z_bottom;
		snprintf(buf, n, "overlay %s primary (zpos primary=%" PRIu64
			 ", overlay=%" PRIu64 ")",
			 f->overlay_top ? "above" : "below", z[0], z[1]);
		break;
	}
	case F_SCALE:
		snprintf(buf, n, "SRC %ux%u -> CRTC %ux%u", d->ofb.w, d->ofb.h,
			 f->dst_w, f->dst_h);
		break;
	case F_BLEND:
		snprintf(buf, n, "blend enum value %" PRIu64, f->blend);
		break;
	default:
		snprintf(buf, n, "?");
	}
}

/*
 * Test -> (diagnose) -> commit one frame.
 *
 * 1. TEST_ONLY with every still-enabled feature.
 * 2. On failure, find the culprit: base alone, then base + each feature
 *    on its own.  A feature that fails on its own is disabled for the
 *    rest of the run and reported.
 * 3. Commit for real (NONBLOCK + PAGE_FLIP_EVENT).
 *
 * Returns 0 on success, -1 if even the base overlay is refused or the
 * real commit fails.
 */
static int commit_frame(struct demo *d, const struct frame *f)
{
	drmModeAtomicReq *req;
	int err;

	err = test_overlay(d, f, d->enabled);
	if (err) {
		int base = test_overlay(d, f, 0);
		if (base) {
			fprintf(stderr, "TEST_ONLY: base overlay configuration "
				"rejected (%s) -- stopping demo\n", strerror(base));
			return -1;
		}
		for (size_t i = 0; i < sizeof(feature_names) / sizeof(feature_names[0]); i++) {
			unsigned bit = feature_names[i].bit;
			int e;
			char what[96];

			if (!(d->enabled & bit))
				continue;
			e = test_overlay(d, f, bit);
			if (!e)
				continue;
			describe_frame_value(d, f, bit, what, sizeof(what));
			printf("  !! TEST_ONLY rejected feature \"%s\" (%s: %s) "
			       "-- continuing without it\n",
			       feature_name(bit), what, strerror(e));
			d->enabled &= ~bit;
		}
		if (d->enabled && test_overlay(d, f, d->enabled)) {
			printf("  !! features accepted one by one but not together "
			       "-- continuing with base overlay only\n");
			d->enabled = 0;
		}
	}

	req = drmModeAtomicAlloc();
	if (!req)
		return -1;
	add_overlay(d, req, f, d->enabled);
	err = atomic_commit_err(d->fd, req,
				DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, d);
	drmModeAtomicFree(req);
	if (err) {
		fprintf(stderr, "atomic commit: %s\n", strerror(err));
		return -1;
	}
	d->flip_pending = true;
	return 0;
}

/*
 * Build the initial modeset request.
 *
 * level 2: connector + CRTC (MODE_ID, ACTIVE) + primary + overlay + reset
 *          of composition props (rotation=rotate-0, primary alpha opaque,
 *          primary blend = Pre-multiplied) + overlay features
 * level 1: same, but without any composition property
 * level 0: no overlay at all
 * Every other plane that is currently on this CRTC is switched off, so
 * only our two planes take part in blending.
 */
static drmModeAtomicReq *build_modeset(struct demo *d, int level,
				       const struct frame *f0)
{
	drmModeAtomicReq *req = drmModeAtomicAlloc();
	struct plane_info *p = pri(d);

	if (!req)
		return NULL;

	drmModeAtomicAddProperty(req, d->conn_id, d->conn_crtc.id, d->crtc_id);
	drmModeAtomicAddProperty(req, d->crtc_id, d->crtc_mode_id.id, d->mode_blob);
	drmModeAtomicAddProperty(req, d->crtc_id, d->crtc_active.id, 1);
	add_primary(d, req);

	for (int i = 0; i < d->n_planes; i++) {
		struct plane_info *q = &d->planes[i];
		if (i == d->primary || (level > 0 && i == d->overlay))
			continue;
		if (q->crtc_id.value == d->crtc_id) {
			drmModeAtomicAddProperty(req, q->id, q->fb_id.id, 0);
			drmModeAtomicAddProperty(req, q->id, q->crtc_id.id, 0);
		}
	}

	if (level >= 1)
		add_overlay(d, req, f0, level >= 2 ? d->enabled : 0);

	if (level >= 2) {
		if (prop_mutable(&p->rotation) && p->has_rot0)
			drmModeAtomicAddProperty(req, p->id, p->rotation.id, p->rot0_value);
		if (prop_mutable(&p->alpha))
			drmModeAtomicAddProperty(req, p->id, p->alpha.id, p->alpha.max);
		for (int e = 0; e < p->n_blend_enums; e++)
			if (prop_mutable(&p->blend) &&
			    strcmp(p->blend_enums[e].name, "Pre-multiplied") == 0)
				drmModeAtomicAddProperty(req, p->id, p->blend.id,
							 p->blend_enums[e].value);
		if (d->overlay >= 0 && prop_mutable(&ovl(d)->rotation) && ovl(d)->has_rot0)
			drmModeAtomicAddProperty(req, ovl(d)->id, ovl(d)->rotation.id,
						 ovl(d)->rot0_value);
	}
	return req;
}

static int do_modeset(struct demo *d, const struct frame *f0)
{
	static const char *level_desc[] = {
		"primary only (overlay rejected)",
		"primary + overlay, composition properties left as found",
		"primary + overlay + composition properties",
	};
	int start = d->overlay >= 0 ? 2 : 0;

	if (drmModeCreatePropertyBlob(d->fd, &d->mode, sizeof(d->mode),
				      &d->mode_blob)) {
		fprintf(stderr, "drmModeCreatePropertyBlob: %s\n", strerror(errno));
		d->mode_blob = 0;
		return -1;
	}

	for (int level = start; level >= 0; level--) {
		drmModeAtomicReq *req = build_modeset(d, level, f0);
		int err;

		if (!req)
			return -1;
		err = atomic_commit_err(d->fd, req, DRM_MODE_ATOMIC_TEST_ONLY |
					DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
		if (err) {
			printf("  TEST_ONLY modeset [%s]: rejected (%s)\n",
			       level_desc[level], strerror(err));
			drmModeAtomicFree(req);
			if (err == EACCES || err == EPERM) {
				fprintf(stderr, "Not DRM master -- stop other KMS "
					"clients (X, Wayland, another demo) first\n");
				return -1;
			}
			continue;
		}
		err = atomic_commit_err(d->fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
		drmModeAtomicFree(req);
		if (err) {
			fprintf(stderr, "Modeset commit failed after TEST_ONLY "
				"passed: %s\n", strerror(err));
			return -1;
		}
		d->modeset_done = true;
		printf("Modeset committed: %s\n", level_desc[level]);
		if (level == 0) {
			d->overlay = -1;
			d->enabled = 0;
		} else if (level == 1) {
			printf("  (features will be tested one by one in the loop)\n");
		}
		return 0;
	}
	fprintf(stderr, "Every modeset variant was rejected\n");
	return -1;
}

/*
 * Restore what was on screen before we started.
 *
 * The saved MODE_ID blob may no longer exist (blobs die with their last
 * reference), so a fresh blob is created from the mode reported by
 * drmModeGetCrtc() at startup.  Everything else is written back from
 * the property values read at startup.
 */
static void restore_state(struct demo *d)
{
	drmModeAtomicReq *req;
	uint32_t blob = 0;
	int err;

	if (!d->modeset_done)
		return;
	wait_flip(d, false);

	req = drmModeAtomicAlloc();
	if (!req)
		return;

	if (d->saved_mode_valid &&
	    drmModeCreatePropertyBlob(d->fd, &d->saved_mode,
				      sizeof(d->saved_mode), &blob)) {
		fprintf(stderr, "restore: cannot create mode blob: %s\n",
			strerror(errno));
		blob = 0;
	}
	drmModeAtomicAddProperty(req, d->conn_id, d->conn_crtc.id, d->conn_crtc.value);
	drmModeAtomicAddProperty(req, d->crtc_id, d->crtc_mode_id.id, blob);
	drmModeAtomicAddProperty(req, d->crtc_id, d->crtc_active.id,
				 blob ? d->crtc_active.value : 0);

	for (int i = 0; i < d->n_planes; i++) {
		struct plane_info *q = &d->planes[i];
		const struct prop *all[] = {
			&q->fb_id, &q->crtc_id, &q->crtc_x, &q->crtc_y,
			&q->crtc_w, &q->crtc_h, &q->src_x, &q->src_y,
			&q->src_w, &q->src_h, &q->zpos, &q->alpha,
			&q->blend, &q->rotation,
		};
		if (!d->touched[i])
			continue;
		for (size_t k = 0; k < sizeof(all) / sizeof(all[0]); k++)
			if (prop_mutable(all[k]))
				drmModeAtomicAddProperty(req, q->id, all[k]->id,
							 all[k]->value);
	}

	err = atomic_commit_err(d->fd, req, DRM_MODE_ATOMIC_TEST_ONLY |
				DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	if (!err)
		err = atomic_commit_err(d->fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	if (err)
		fprintf(stderr, "Restore of the original state failed (%s).\n"
			"If an fbdev console was active, the kernel restores it "
			"when the last DRM client closes the device.\n",
			strerror(err));
	else
		printf("Original KMS state restored.\n");

	drmModeAtomicFree(req);
	if (blob)
		drmModeDestroyPropertyBlob(d->fd, blob);
}

/* Find the connector (+ CRTC) for --demo. */
static int demo_pick_output(struct demo *d, const drmModeRes *res,
			    uint32_t override, drmModeConnector **conn_out)
{
	int oidx = override ? crtc_index(res, override) : -1;

	if (override && oidx < 0) {
		fprintf(stderr, "CRTC %u does not exist\n", override);
		return -1;
	}

	for (int i = 0; i < res->count_connectors; i++) {
		drmModeConnector *c = drmModeGetConnector(d->fd, res->connectors[i]);
		uint32_t mask, cur, pick = 0;

		if (!c)
			continue;
		if (c->connection != DRM_MODE_CONNECTED || c->count_modes == 0) {
			drmModeFreeConnector(c);
			continue;
		}
		mask = connector_crtc_mask(d->fd, c);
		cur = connector_current_crtc(d->fd, c);

		if (override) {
			if (!(mask & (1u << oidx))) {
				drmModeFreeConnector(c);
				continue;
			}
			if (cur && cur != override) {
				fprintf(stderr, "Connector %u is currently driven by "
					"CRTC %u; moving it to CRTC %u is not "
					"supported by this demo (restore would need "
					"to touch two CRTCs).\n",
					c->connector_id, cur, override);
				drmModeFreeConnector(c);
				return -1;
			}
			pick = override;
		} else if (cur) {
			pick = cur;
		} else {
			for (int k = 0; k < res->count_crtcs; k++)
				if (mask & (1u << k)) {
					pick = res->crtcs[k];
					break;
				}
		}
		if (pick) {
			d->conn_id = c->connector_id;
			d->crtc_id = pick;
			d->crtc_idx = crtc_index(res, pick);
			*conn_out = c;
			return 0;
		}
		drmModeFreeConnector(c);
	}
	fprintf(stderr, "No connected connector usable%s\n",
		override ? " with that CRTC" : "");
	return -1;
}

/* Refuse to steal a CRTC that is driving a different connector. */
static int demo_check_crtc_free(struct demo *d, const drmModeRes *res)
{
	for (int i = 0; i < res->count_connectors; i++) {
		struct prop cp;
		if (res->connectors[i] == d->conn_id)
			continue;
		if (find_prop(d->fd, res->connectors[i], DRM_MODE_OBJECT_CONNECTOR,
			      "CRTC_ID", &cp) && cp.value == d->crtc_id) {
			fprintf(stderr, "CRTC %u already drives connector %u; "
				"pick another CRTC with -c\n", d->crtc_id,
				res->connectors[i]);
			return -1;
		}
	}
	return 0;
}

static void print_feature_summary(struct demo *d)
{
	struct plane_info *o = ovl(d);

	printf("Overlay feature check (plane %u):\n", o->id);
	printf("  alpha            : %s\n",
	       !prop_present(&o->alpha) ? "not exposed -> phase skipped" :
	       !prop_mutable(&o->alpha) ? "immutable -> phase skipped" :
	       (d->enabled & F_ALPHA) ? "mutable -> will sweep" : "unusable range");
	printf("  zpos             : %s\n",
	       (d->enabled & F_ZPOS) ? "both orders expressible -> will swap" :
	       "cannot express overlay above AND below primary -> phase skipped");
	printf("  pixel blend mode : %s\n",
	       (d->enabled & F_BLEND) ? "mutable enum -> will cycle" :
	       !prop_present(&o->blend) ? "not exposed -> phase skipped" :
	       !d->overlay_has_alpha ? "overlay buffer has no alpha channel -> "
				       "all modes are equivalent, phase skipped" :
	       "immutable or single value -> phase skipped");
	printf("  scaling          : no property; decided by TEST_ONLY\n");
}

static int run_demo(int fd, uint32_t crtc_override)
{
	struct demo d;
	drmModeRes *res = NULL;
	drmModePlaneRes *pres = NULL;
	drmModeConnector *conn = NULL;
	drmModeCrtc *crtc = NULL;
	struct frame f, prev = { .phase = -2 };
	struct sigaction sa;
	unsigned long frame = 0;
	int best_score = -1, ret = -1;
	uint32_t refresh;

	memset(&d, 0, sizeof(d));
	d.fd = fd;
	d.primary = d.overlay = -1;

	res = drmModeGetResources(fd);
	if (!res) {
		fprintf(stderr, "drmModeGetResources: %s\n", strerror(errno));
		return -1;
	}
	if (demo_pick_output(&d, res, crtc_override, &conn) < 0)
		goto out;
	if (d.crtc_idx < 0 || d.crtc_idx >= 32) {
		fprintf(stderr, "CRTC %u has no usable index\n", d.crtc_id);
		goto out;
	}
	if (demo_check_crtc_free(&d, res) < 0)
		goto out;

	/* Preferred mode if the connector marks one, else the first. */
	d.mode = conn->modes[0];
	for (int i = 0; i < conn->count_modes; i++)
		if (conn->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
			d.mode = conn->modes[i];
			break;
		}
	refresh = d.mode.vrefresh ? d.mode.vrefresh : 60;
	printf("Connector %u -> CRTC %u (index %d), mode \"%s\" (%ux%u@%u)\n",
	       d.conn_id, d.crtc_id, d.crtc_idx, d.mode.name,
	       d.mode.hdisplay, d.mode.vdisplay, d.mode.vrefresh);

	/* ---- Save the original CRTC / connector state ---- */
	if (!find_prop(fd, d.conn_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID", &d.conn_crtc) ||
	    !find_prop(fd, d.crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE", &d.crtc_active) ||
	    !find_prop(fd, d.crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID", &d.crtc_mode_id)) {
		fprintf(stderr, "Missing atomic connector/CRTC properties\n");
		goto out;
	}
	crtc = drmModeGetCrtc(fd, d.crtc_id);
	if (crtc && crtc->mode_valid) {
		d.saved_mode = crtc->mode;
		d.saved_mode_valid = true;
	}

	/* ---- Snapshot every plane usable on this CRTC ---- */
	pres = drmModeGetPlaneResources(fd);
	if (!pres) {
		fprintf(stderr, "drmModeGetPlaneResources: %s\n", strerror(errno));
		goto out;
	}
	for (uint32_t i = 0; i < pres->count_planes && d.n_planes < MAX_PLANES; i++) {
		struct plane_info *pi = &d.planes[d.n_planes];
		if (load_plane(fd, pres->planes[i], pi) < 0)
			continue;
		if (!(pi->possible_crtcs & (1u << d.crtc_idx)) ||
		    !plane_has_atomic_geometry(pi))
			continue;
		d.n_planes++;
	}

	/*
	 * Primary: a PRIMARY plane, preferably the one already on this CRTC.
	 * Overlay: an OVERLAY plane not used by another CRTC, scored by
	 * ARGB8888 support (+4), mutable alpha (+2), mutable zpos (+1).
	 */
	for (int i = 0; i < d.n_planes; i++) {
		struct plane_info *pi = &d.planes[i];
		bool free_here = pi->crtc_id.value == 0 || pi->crtc_id.value == d.crtc_id;

		if (pi->type == DRM_PLANE_TYPE_PRIMARY && free_here) {
			if (d.primary < 0 || pi->crtc_id.value == d.crtc_id)
				if (pi->has_xrgb8888 || pi->has_argb8888)
					d.primary = i;
		} else if (pi->type == DRM_PLANE_TYPE_OVERLAY && free_here) {
			int score = (pi->has_argb8888 ? 4 : 0) +
				    (prop_mutable(&pi->alpha) ? 2 : 0) +
				    (prop_mutable(&pi->zpos) ? 1 : 0);
			if ((pi->has_argb8888 || pi->has_xrgb8888) && score > best_score) {
				best_score = score;
				d.overlay = i;
			}
		}
	}
	if (d.primary < 0) {
		fprintf(stderr, "No usable primary plane for CRTC %u\n", d.crtc_id);
		goto out;
	}
	printf("Primary plane %u, overlay plane %s", pri(&d)->id,
	       d.overlay >= 0 ? "" : "none (no free OVERLAY plane)\n");
	if (d.overlay >= 0)
		printf("%u\n", ovl(&d)->id);

	/* Every plane we may modify is saved for restore. */
	for (int i = 0; i < d.n_planes; i++)
		if (i == d.primary || i == d.overlay ||
		    d.planes[i].crtc_id.value == d.crtc_id)
			d.touched[i] = true;

	/* ---- Framebuffers ---- */
	if (dumb_fb_create(fd, d.mode.hdisplay, d.mode.vdisplay,
			   pri(&d)->has_xrgb8888 ? DRM_FORMAT_XRGB8888 : DRM_FORMAT_ARGB8888,
			   &d.pfb) < 0)
		goto out;
	draw_primary(&d.pfb);

	if (d.overlay >= 0) {
		uint32_t ow = (d.mode.hdisplay / 2) & ~1u;
		uint32_t oh = (d.mode.vdisplay / 2) & ~1u;
		struct plane_info *o = ovl(&d);
		struct zrange zp = zrange_of(&pri(&d)->zpos), zo = zrange_of(&o->zpos);

		d.overlay_has_alpha = o->has_argb8888;
		if (ow < 8 || oh < 8 ||
		    dumb_fb_create(fd, ow, oh, d.overlay_has_alpha ?
				   DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888,
				   &d.ofb) < 0) {
			fprintf(stderr, "Overlay buffer unavailable -- primary only\n");
			d.overlay = -1;
		} else {
			draw_overlay(&d.ofb, d.overlay_has_alpha);
			printf("Overlay buffer %ux%u %s\n", ow, oh,
			       d.overlay_has_alpha ? "ARGB8888 (per-pixel alpha, premultiplied)" :
			       "XRGB8888 (plane does not list ARGB8888: no per-pixel alpha)");

			/* ---- Decide candidate features ---- */
			d.enabled = F_SCALE;
			if (prop_mutable(&o->alpha) && kind_is_range(o->alpha.flags) &&
			    o->alpha.max > 0)
				d.enabled |= F_ALPHA;
			if (plan_zpos(&zp, &zo, d.z_top, d.z_bottom))
				d.enabled |= F_ZPOS;

			d.pp.alpha_max  = (d.enabled & F_ALPHA) ? o->alpha.max : 0;
			d.pp.phase_len  = 4 * refresh;
			d.pp.toggle_len = refresh;
			d.pp.src_w = ow;
			d.pp.src_h = oh;
			/* Default = "Pre-multiplied" (matches our buffer), else
			 * whatever the driver lists first. */
			if (o->n_blend_enums > 0)
				d.pp.blend_default = o->blend_enums[0].value;
			for (int e = 0; e < o->n_blend_enums; e++) {
				d.pp.blend_vals[d.pp.n_blend++] = o->blend_enums[e].value;
				if (strcmp(o->blend_enums[e].name, "Pre-multiplied") == 0)
					d.pp.blend_default = o->blend_enums[e].value;
			}
			if (prop_mutable(&o->blend) && d.pp.n_blend >= 2 &&
			    d.overlay_has_alpha)
				d.enabled |= F_BLEND;
			print_feature_summary(&d);
		}
	}

	/* ---- Signals: stop the loop, then restore ---- */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;    /* no SA_RESTART: let select() return EINTR */
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* ---- Initial atomic modeset ---- */
	plan_frame(&d.pp, d.enabled, 0, &f);
	if (do_modeset(&d, &f) < 0)
		goto out;

	if (d.overlay < 0) {
		printf("No overlay on screen -- nothing to animate. "
		       "Press Ctrl+C to restore and exit.\n");
		while (!g_stop)
			pause();
		ret = 0;
		goto out;
	}

	printf("\nAnimating (%u frames per phase). Ctrl+C to stop.\n", d.pp.phase_len);

	/* ---- Animation loop ---- */
	while (!g_stop) {
		plan_frame(&d.pp, d.enabled, frame, &f);

		if (f.phase != prev.phase) {
			if (f.phase >= 0)
				printf("\n[phase] %s\n", phase_title[f.phase]);
			else
				printf("\n[phase] static overlay (no feature left to animate)\n");
		}
		if (f.phase == PH_BLEND && f.blend_idx >= 0 &&
		    (f.blend_idx != prev.blend_idx || prev.phase != PH_BLEND)) {
			const char *n = ovl(&d)->blend_enums[f.blend_idx].name;
			const char *hint =
				!strcmp(n, "Pre-multiplied") ? "buffer matches this mode: smooth fade to the right" :
				!strcmp(n, "Coverage") ? "colour multiplied by alpha twice: left side darker" :
				!strcmp(n, "None") ? "pixel alpha ignored: opaque, dark-to-bright ramp" :
				"driver-specific mode";
			printf("  blend mode \"%s\" (%s)\n", n, hint);
		}
		if (f.phase == PH_ZPOS && (f.overlay_top != prev.overlay_top ||
					    prev.phase != PH_ZPOS))
			printf("  overlay %s primary\n", f.overlay_top ? "ABOVE" : "BELOW");

		if (commit_frame(&d, &f) < 0)
			break;
		if (wait_flip(&d, true) < 0)
			break;
		prev = f;
		frame++;
	}
	ret = 0;

out:
	restore_state(&d);
	dumb_fb_destroy(fd, &d.ofb);
	dumb_fb_destroy(fd, &d.pfb);
	if (d.mode_blob)
		drmModeDestroyPropertyBlob(fd, d.mode_blob);
	if (crtc)
		drmModeFreeCrtc(crtc);
	if (pres)
		drmModeFreePlaneResources(pres);
	if (conn)
		drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	return ret;
}

/* ============================================================
 * --selftest : checks of the pure helpers (no DRM device needed)
 * ============================================================ */
static int run_selftest(void)
{
	int fails = 0;
	uint32_t u;
	uint64_t top[2], bot[2];
	struct frame f;
	struct plan_params pp = {
		.alpha_max = 0xffff, .phase_len = 240, .toggle_len = 60,
		.src_w = 512, .src_h = 300, .n_blend = 3,
		.blend_vals = { 0, 1, 2 }, .blend_default = 0,
	};
	uint32_t minw = UINT32_MAX, maxw = 0;

#define CHECK(cond) do { \
	if (cond) { printf("  PASS  %s\n", #cond); } \
	else { printf("  FAIL  %s\n", #cond); fails++; } } while (0)

	printf("selftest:\n");
	CHECK(to_fp16(1024) == 0x04000000ULL);
	CHECK(parse_u32("208", &u) == 0 && u == 208);
	CHECK(parse_u32("0xd0", &u) == 0 && u == 208);
	CHECK(parse_u32("12x", &u) < 0);
	CHECK(parse_u32("", &u) < 0);
	CHECK(parse_u32("4294967296", &u) < 0);

	/* VOP2-like: both planes mutable 0..7 */
	{
		struct zrange a = { true, 0, 7 }, b = { true, 0, 7 };
		CHECK(plan_zpos(&a, &b, top, bot) && top[0] == 0 && top[1] == 7 &&
		      bot[0] == 7 && bot[1] == 0);
	}
	/* both immutable: no swap possible */
	{
		struct zrange a = { true, 0, 0 }, b = { true, 3, 3 };
		CHECK(!plan_zpos(&a, &b, top, bot));
	}
	/* primary pinned at bottom (immutable 0), overlay 1..7: cannot go below */
	{
		struct zrange a = { true, 0, 0 }, b = { true, 1, 7 };
		CHECK(!plan_zpos(&a, &b, top, bot));
	}
	/* missing zpos on one plane */
	{
		struct zrange a = { false, 0, 0 }, b = { true, 0, 7 };
		CHECK(!plan_zpos(&a, &b, top, bot));
	}

	/* Alpha sweep endpoints (phase 0 with all features enabled) */
	plan_frame(&pp, F_ALPHA | F_ZPOS | F_SCALE | F_BLEND, 0, &f);
	CHECK(f.phase == PH_ALPHA && f.alpha == 0);
	plan_frame(&pp, F_ALPHA | F_ZPOS | F_SCALE | F_BLEND, 239, &f);
	CHECK(f.phase == PH_ALPHA && f.alpha == 0xffff);

	/* zpos toggles once per toggle_len */
	plan_frame(&pp, F_ALPHA | F_ZPOS, 240, &f);
	CHECK(f.phase == PH_ZPOS && f.overlay_top);
	plan_frame(&pp, F_ALPHA | F_ZPOS, 300, &f);
	CHECK(f.phase == PH_ZPOS && !f.overlay_top);

	/* Scale stays within 0.5x .. 1.5x and even */
	for (unsigned long fr = 0; fr < 240; fr++) {
		plan_frame(&pp, F_SCALE, fr, &f);
		if (f.dst_w < minw) minw = f.dst_w;
		if (f.dst_w > maxw) maxw = f.dst_w;
		if (f.dst_w & 1) fails++;
	}
	CHECK(minw == 256 && maxw == 768);

	/* Blend phase visits every value */
	plan_frame(&pp, F_BLEND, 0, &f);
	CHECK(f.phase == PH_BLEND && f.blend == 0);
	plan_frame(&pp, F_BLEND, 239, &f);
	CHECK(f.blend == 2);

	/* Disabled phases are skipped */
	plan_frame(&pp, F_SCALE, 0, &f);
	CHECK(f.phase == PH_SCALE && f.alpha == 0xffff);
	plan_frame(&pp, 0, 123, &f);
	CHECK(f.phase == -1 && f.dst_w == 512);
#undef CHECK

	printf("%s (%d failure%s)\n", fails ? "FAILED" : "OK", fails,
	       fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

/* ============================================================
 * main
 * ============================================================ */
static void usage(const char *prog)
{
	printf("Usage: %s [--list | --demo] [-d <device>] [-c <crtc_id>]\n"
	       "\n"
	       "  --list          (default) report zpos / alpha / pixel blend mode /\n"
	       "                  rotation for every plane usable on the CRTC and\n"
	       "                  probe scaling with TEST_ONLY commits\n"
	       "  --demo          modeset + primary + semi-transparent overlay;\n"
	       "                  animate alpha, zpos, scaling and blend mode\n"
	       "                  (Ctrl+C restores the original state)\n"
	       "  -d <device>     DRM device (default /dev/dri/card0)\n"
	       "  -c <crtc_id>    use this CRTC object ID instead of auto-selecting\n"
	       "  --selftest      check the pure helper logic (no device needed)\n"
	       "  -h, --help      this text\n", prog);
}

int main(int argc, char **argv)
{
	static const struct option longopts[] = {
		{ "list",     no_argument,       NULL, 'L' },
		{ "demo",     no_argument,       NULL, 'D' },
		{ "selftest", no_argument,       NULL, 'S' },
		{ "help",     no_argument,       NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	const char *dev = "/dev/dri/card0";
	uint32_t crtc_override = 0;
	bool demo = false, atomic_ok;
	int opt, fd, ret, atomic_errno;

	/* Keep stdout/stderr ordered when the output is piped into a file */
	setvbuf(stdout, NULL, _IOLBF, 0);

	while ((opt = getopt_long(argc, argv, "d:c:h", longopts, NULL)) != -1) {
		switch (opt) {
		case 'L': demo = false; break;
		case 'D': demo = true;  break;
		case 'S': return run_selftest();
		case 'd': dev = optarg; break;
		case 'c':
			if (parse_u32(optarg, &crtc_override) < 0 || !crtc_override) {
				fprintf(stderr, "Invalid CRTC id '%s'\n", optarg);
				return 2;
			}
			break;
		case 'h': usage(argv[0]); return 0;
		default:  usage(argv[0]); return 2;
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Unexpected argument '%s'\n", argv[optind]);
		usage(argv[0]);
		return 2;
	}

	fd = open(dev, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
		return 1;
	}

	/*
	 * UNIVERSAL_PLANES exposes primary and cursor planes through the
	 * plane API.  ATOMIC additionally exposes the atomic-only properties
	 * (FB_ID, CRTC_ID, SRC_*, CRTC_* on planes; MODE_ID/ACTIVE on CRTCs)
	 * and unlocks the atomic ioctl.  zpos/alpha/blend/rotation are *not*
	 * atomic-only, so --list still reports them without it.
	 */
	if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
		fprintf(stderr, "DRM_CLIENT_CAP_UNIVERSAL_PLANES: %s\n", strerror(errno));
		close(fd);
		return 1;
	}
	atomic_ok = drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0;
	atomic_errno = atomic_ok ? 0 : errno;

	if (demo) {
		if (!atomic_ok) {
			fprintf(stderr, "--demo needs DRM_CLIENT_CAP_ATOMIC: %s\n",
				strerror(atomic_errno));
			close(fd);
			return 1;
		}
		ret = run_demo(fd, crtc_override);
	} else {
		ret = run_list(fd, atomic_ok, crtc_override);
	}

	close(fd);
	return ret ? 1 : 0;
}
