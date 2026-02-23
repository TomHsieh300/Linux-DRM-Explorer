#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* ============================================================
 * DRM Atomic KMS Demo
 *
 * This program demonstrates the modern KMS atomic API and the
 * property-based object model that underlies it.
 *
 * Progression from the previous demo:
 *   drm-pageflip-vs-tearing.c  -- legacy SetCrtc / PageFlip API
 *   drm-atomic-demo.c          -- atomic commit, properties, planes
 *
 * Three runnable modes:
 *   (default)     Print all KMS object properties and exit
 *   --atomic      Atomic modesetting + non-blocking page flip
 *   --multiplane  Primary plane animation + static overlay plane
 *
 * Tested on RK3588 / VOP2 with Ubuntu Lite (no compositor).
 * ============================================================ */

#define MAX_BUFFERS 2

/* ============================================================
 * Property ID cache
 *
 * The atomic API operates entirely through property IDs.  Instead of
 * passing (crtc_id, fb_id, x, y) as separate ioctl arguments like the
 * legacy API does, you build a property list:
 *
 *   object_id  |  property_id  |  value
 *   -----------+---------------+--------
 *   plane_id   |  FB_ID        |  fb_id
 *   plane_id   |  CRTC_ID      |  crtc_id
 *   plane_id   |  CRTC_X       |  0
 *   plane_id   |  CRTC_Y       |  0
 *   ...
 *
 * Caching IDs at startup avoids repeated string lookups at runtime.
 * ============================================================ */
struct plane_props {
	uint32_t fb_id;
	uint32_t crtc_id;
	uint32_t crtc_x;
	uint32_t crtc_y;
	uint32_t crtc_w;
	uint32_t crtc_h;
	uint32_t src_x;
	uint32_t src_y;
	uint32_t src_w;
	uint32_t src_h;
	uint32_t type;   /* PRIMARY / OVERLAY / CURSOR */
};

struct crtc_props {
	uint32_t active;
	uint32_t mode_id;  /* Blob property: encodes drmModeModeInfo */
};

struct connector_props {
	uint32_t crtc_id;
};

/* Top-level KMS pipeline state */
struct kms_state {
	int fd;

	uint32_t conn_id;
	uint32_t crtc_id;
	uint32_t crtc_idx;   /* Index into res->crtcs[], needed for vblank */
	uint32_t plane_id;   /* Primary plane */
	uint32_t overlay_id; /* Overlay plane (multiplane mode) */

	drmModeModeInfo mode;
	uint32_t mode_blob_id; /* Kernel-managed blob for mode data */

	struct connector_props conn_props;
	struct crtc_props       crtc_props;
	struct plane_props      primary_props;
	struct plane_props      overlay_props;
};

struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint8_t  *vaddr;
	uint32_t fb_id;
};

struct animation_state {
	int bar_x;
	int bar_width;
	int direction;
	int frame_count;
};

struct flip_pending {
	bool waiting;
};

/* ============================================================
 * get_property_id - Look up a property ID by name on a DRM object.
 * @fd:      DRM file descriptor.
 * @props:   Property list returned by drmModeObjectGetProperties().
 * @name:    Property name string to search for.
 * @id_out:  Receives the property ID if found.
 *
 * Returns 0 on success, -1 if the property is not found.
 *
 * Every KMS object (connector, CRTC, plane) exposes a set of named
 * properties.  The property IDs are assigned dynamically by the kernel
 * at driver load time, so they must be discovered at runtime rather
 * than hard-coded.  This helper performs that discovery.
 * ============================================================ */
static int get_property_id(int fd,
			   drmModeObjectProperties *props,
			   const char *name,
			   uint32_t *id_out)
{
	for (uint32_t i = 0; i < props->count_props; i++) {
		drmModePropertyRes *prop =
			drmModeGetProperty(fd, props->props[i]);
		if (!prop)
			continue;

		if (strcmp(prop->name, name) == 0) {
			*id_out = prop->prop_id;
			drmModeFreeProperty(prop);
			return 0;
		}
		drmModeFreeProperty(prop);
	}
	return -1;
}

/* ============================================================
 * print_object_properties - Dump all properties of a KMS object.
 * @fd:      DRM file descriptor.
 * @obj_id:  The KMS object ID (connector, CRTC, or plane ID).
 * @obj_type: DRM_MODE_OBJECT_* constant.
 * @label:   Human-readable label for the log output.
 *
 * This is the core of the property discovery mode.  For each property
 * it prints the ID, name, flags, and current value.  Property flags
 * indicate immutability (DRM_MODE_PROP_IMMUTABLE), whether the value
 * is an enum, a bitmask, a blob, or a plain integer range.
 * ============================================================ */
static void print_object_properties(int fd, uint32_t obj_id,
				    uint32_t obj_type, const char *label)
{
	drmModeObjectProperties *props =
		drmModeObjectGetProperties(fd, obj_id, obj_type);
	if (!props) {
		fprintf(stderr, "  Failed to get properties for %s\n", label);
		return;
	}

	printf("\n  [%s] id=%u  (%u properties)\n",
	       label, obj_id, props->count_props);
	printf("  %-6s  %-32s  %-12s  %s\n",
	       "PropID", "Name", "Flags", "Current Value");
	printf("  %s\n", "------  --------------------------------"
			  "  ------------  -------------");

	for (uint32_t i = 0; i < props->count_props; i++) {
		drmModePropertyRes *prop =
			drmModeGetProperty(fd, props->props[i]);
		if (!prop)
			continue;

		/* Decode property type flags into a readable string */
		char flags[64] = {0};
		if (prop->flags & DRM_MODE_PROP_IMMUTABLE)
			strcat(flags, "IMMUTABLE ");
		if (prop->flags & DRM_MODE_PROP_ENUM)
			strcat(flags, "ENUM ");
		if (prop->flags & DRM_MODE_PROP_BITMASK)
			strcat(flags, "BITMASK ");
		if (prop->flags & DRM_MODE_PROP_BLOB)
			strcat(flags, "BLOB ");
		if (prop->flags & DRM_MODE_PROP_RANGE)
			strcat(flags, "RANGE ");
		if (prop->flags & DRM_MODE_PROP_OBJECT)
			strcat(flags, "OBJECT ");
		if (strlen(flags) == 0)
			strcat(flags, "SIGNED_RANGE");

		printf("  %-6u  %-32s  %-12s  %" PRIu64 "\n",
		       prop->prop_id, prop->name, flags,
		       props->prop_values[i]);

		/*
		 * For ENUM properties, print the valid enum values.
		 * Example: "type" plane property has values
		 *   0=Overlay, 1=Primary, 2=Cursor
		 */
		if (prop->flags & DRM_MODE_PROP_ENUM) {
			for (int e = 0; e < prop->count_enums; e++) {
				printf("  %6s  %-32s  %-12s  value=%" PRIu64 "\n",
				       "", prop->enums[e].name, "(enum)",
				       (uint64_t)prop->enums[e].value);
			}
		}

		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props);
}

/* ============================================================
 * run_property_discovery - Enumerate and print all KMS object properties.
 *
 * This mode answers the question "what can I control via atomic commit?"
 * for the connected display hardware.  The output lists every property
 * name and ID that can appear in a drmModeAtomicAddProperty() call.
 * ============================================================ */
static void run_property_discovery(int fd, drmModeRes *res,
				   struct kms_state *kms)
{
	printf("============================================================\n");
	printf(" KMS Object Property Discovery\n");
	printf(" Connector / CRTC / Plane property IDs and current values\n");
	printf("============================================================\n");

	/* --- Connectors --- */
	printf("\n=== CONNECTORS (%d) ===", res->count_connectors);
	for (int i = 0; i < res->count_connectors; i++) {
		drmModeConnector *conn =
			drmModeGetConnector(fd, res->connectors[i]);
		if (!conn) continue;

		char label[64];
		const char *type_name;
		switch (conn->connector_type) {
		case DRM_MODE_CONNECTOR_HDMIA:  type_name = "HDMI-A"; break;
		case DRM_MODE_CONNECTOR_DSI:    type_name = "DSI";    break;
		case DRM_MODE_CONNECTOR_eDP:    type_name = "eDP";    break;
		case DRM_MODE_CONNECTOR_DisplayPort: type_name = "DP"; break;
		default: type_name = "Unknown"; break;
		}

		snprintf(label, sizeof(label), "Connector %s-%d [%s]",
			 type_name, conn->connector_type_id,
			 conn->connection == DRM_MODE_CONNECTED ?
			 "connected" : "disconnected");

		print_object_properties(fd, conn->connector_id,
					DRM_MODE_OBJECT_CONNECTOR, label);
		drmModeFreeConnector(conn);
	}

	/* --- CRTCs --- */
	printf("\n=== CRTCs (%d) ===", res->count_crtcs);
	for (int i = 0; i < res->count_crtcs; i++) {
		char label[32];
		snprintf(label, sizeof(label), "CRTC[%d]", i);
		print_object_properties(fd, res->crtcs[i],
					DRM_MODE_OBJECT_CRTC, label);
	}

	/* --- Planes --- */
	drmModePlaneRes *plane_res = drmModeGetPlaneResources(fd);
	if (!plane_res) {
		fprintf(stderr, "drmModeGetPlaneResources failed\n");
		return;
	}

	printf("\n=== PLANES (%d) ===", plane_res->count_planes);
	for (uint32_t i = 0; i < plane_res->count_planes; i++) {
		drmModePlane *plane =
			drmModeGetPlane(fd, plane_res->planes[i]);
		if (!plane) continue;

		/*
		 * Determine plane type by reading the "type" property.
		 * Plane types:
		 *   PRIMARY  -- must always be active when CRTC is active
		 *   OVERLAY  -- optional, can be layered on top
		 *   CURSOR   -- small cursor sprite, usually 64x64 max
		 */
		drmModeObjectProperties *oprops =
			drmModeObjectGetProperties(fd, plane->plane_id,
						   DRM_MODE_OBJECT_PLANE);
		const char *type_str = "Unknown";
		if (oprops) {
			for (uint32_t p = 0; p < oprops->count_props; p++) {
				drmModePropertyRes *pr =
					drmModeGetProperty(fd,
							   oprops->props[p]);
				if (pr && strcmp(pr->name, "type") == 0) {
					uint64_t val = oprops->prop_values[p];
					if (val == DRM_PLANE_TYPE_PRIMARY)
						type_str = "PRIMARY";
					else if (val == DRM_PLANE_TYPE_OVERLAY)
						type_str = "OVERLAY";
					else if (val == DRM_PLANE_TYPE_CURSOR)
						type_str = "CURSOR";
					drmModeFreeProperty(pr);
					break;
				}
				drmModeFreeProperty(pr);
			}
			drmModeFreeObjectProperties(oprops);
		}

		char label[64];
		snprintf(label, sizeof(label),
			 "Plane[%u] id=%u type=%s possible_crtcs=0x%x",
			 i, plane->plane_id, type_str,
			 plane->possible_crtcs);
		print_object_properties(fd, plane->plane_id,
					DRM_MODE_OBJECT_PLANE, label);
		drmModeFreePlane(plane);
	}
	drmModeFreePlaneResources(plane_res);
}

/* ============================================================
 * cache_plane_props - Read and cache property IDs for a plane.
 * @fd:      DRM file descriptor.
 * @plane_id: Target plane object ID.
 * @p:       Output struct to populate.
 *
 * Must be called before any atomic commit that involves this plane.
 * The cached IDs are passed to drmModeAtomicAddProperty() at runtime.
 * ============================================================ */
static int cache_plane_props(int fd, uint32_t plane_id, struct plane_props *p)
{
	drmModeObjectProperties *props =
		drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
	if (!props) return -1;

	int ret = 0;
	ret |= get_property_id(fd, props, "FB_ID",   &p->fb_id);
	ret |= get_property_id(fd, props, "CRTC_ID", &p->crtc_id);
	ret |= get_property_id(fd, props, "CRTC_X",  &p->crtc_x);
	ret |= get_property_id(fd, props, "CRTC_Y",  &p->crtc_y);
	ret |= get_property_id(fd, props, "CRTC_W",  &p->crtc_w);
	ret |= get_property_id(fd, props, "CRTC_H",  &p->crtc_h);
	ret |= get_property_id(fd, props, "SRC_X",   &p->src_x);
	ret |= get_property_id(fd, props, "SRC_Y",   &p->src_y);
	ret |= get_property_id(fd, props, "SRC_W",   &p->src_w);
	ret |= get_property_id(fd, props, "SRC_H",   &p->src_h);
	ret |= get_property_id(fd, props, "type",    &p->type);

	drmModeFreeObjectProperties(props);
	return ret;
}

static int cache_crtc_props(int fd, uint32_t crtc_id, struct crtc_props *p)
{
	drmModeObjectProperties *props =
		drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
	if (!props) return -1;

	int ret = 0;
	ret |= get_property_id(fd, props, "ACTIVE",  &p->active);
	ret |= get_property_id(fd, props, "MODE_ID", &p->mode_id);

	drmModeFreeObjectProperties(props);
	return ret;
}

static int cache_connector_props(int fd, uint32_t conn_id,
				 struct connector_props *p)
{
	drmModeObjectProperties *props =
		drmModeObjectGetProperties(fd, conn_id,
					   DRM_MODE_OBJECT_CONNECTOR);
	if (!props) return -1;

	int ret = get_property_id(fd, props, "CRTC_ID", &p->crtc_id);

	drmModeFreeObjectProperties(props);
	return ret;
}

/* ============================================================
 * Framebuffer helpers (unchanged from previous demo)
 * ============================================================ */
static int create_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_create_dumb create = {
		.width  = bo->width,
		.height = bo->height,
		.bpp    = 32,
	};
	struct drm_mode_map_dumb map = {0};

	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0)
		return -1;

	bo->pitch  = create.pitch;
	bo->size   = create.size;
	bo->handle = create.handle;

	if (drmModeAddFB(fd, bo->width, bo->height, 24, 32,
			 bo->pitch, bo->handle, &bo->fb_id))
		return -1;

	map.handle = bo->handle;
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0)
		return -1;

	bo->vaddr = mmap(0, bo->size, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, map.offset);
	if (bo->vaddr == MAP_FAILED)
		return -1;

	return 0;
}

static void destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = { .handle = bo->handle };
	drmModeRmFB(fd, bo->fb_id);
	munmap(bo->vaddr, bo->size);
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

static void draw_moving_bar(struct buffer_object *bo,
			    struct animation_state *anim,
			    uint32_t color)
{
	uint32_t *pixel   = (uint32_t *)bo->vaddr;
	uint32_t bg_color = 0x202020;

	for (uint32_t y = 0; y < bo->height; y++) {
		for (uint32_t x = 0; x < bo->width; x++) {
			uint32_t offset = y * (bo->pitch / 4) + x;
			if ((int)x >= anim->bar_x &&
			    (int)x <  anim->bar_x + anim->bar_width)
				pixel[offset] = color;
			else
				pixel[offset] = bg_color;
		}
	}
}

static void update_animation(struct animation_state *anim, int screen_width)
{
	anim->bar_x += anim->direction * 8;
	if (anim->bar_x + anim->bar_width >= screen_width) {
		anim->bar_x = screen_width - anim->bar_width;
		anim->direction = -1;
	} else if (anim->bar_x <= 0) {
		anim->bar_x = 0;
		anim->direction = 1;
	}
	anim->frame_count++;
}

/* ============================================================
 * atomic_modeset - Perform initial display configuration atomically.
 * @kms:    KMS pipeline state with cached property IDs.
 * @fb_id:  Initial framebuffer to display.
 *
 * This replaces the legacy drmModeSetCrtc().  The key differences:
 *
 * 1. All changes are expressed as (object_id, property_id, value) tuples.
 * 2. DRM_MODE_ATOMIC_TEST_ONLY validates the request without applying it.
 * 3. DRM_MODE_ATOMIC_ALLOW_MODESET permits mode changes (DPMS, resolution).
 *
 * The mode is passed as a blob property rather than an inline struct.
 * The kernel creates the blob object and returns an ID; subsequent
 * atomic commits reference this ID rather than copying the mode data.
 * ============================================================ */
static int atomic_modeset(struct kms_state *kms, uint32_t fb_id)
{
	int ret;

	/*
	 * Create a mode blob.  The kernel copies the drmModeModeInfo struct
	 * into a managed blob object and returns a blob ID.  This ID is
	 * used as the value of the CRTC "MODE_ID" property.
	 */
	ret = drmModeCreatePropertyBlob(kms->fd, &kms->mode,
					sizeof(kms->mode),
					&kms->mode_blob_id);
	if (ret) {
		perror("drmModeCreatePropertyBlob");
		return ret;
	}
	printf("Created mode blob id=%u for %dx%d@%dHz\n",
	       kms->mode_blob_id,
	       kms->mode.hdisplay, kms->mode.vdisplay, kms->mode.vrefresh);

	drmModeAtomicReq *req = drmModeAtomicAlloc();
	if (!req) return -ENOMEM;

	/*
	 * Build the atomic request: set properties on three object types.
	 *
	 * Connector: tell it which CRTC drives it.
	 * CRTC:      activate it and assign a display mode.
	 * Plane:     point it at our framebuffer and define source/dest rects.
	 *
	 * SRC_* coordinates are in 16.16 fixed-point format (shifted left
	 * by 16 bits).  This allows sub-pixel source crop for scaled planes.
	 * CRTC_* coordinates are in plain pixels (destination on screen).
	 */

	/* Connector properties */
	drmModeAtomicAddProperty(req, kms->conn_id,
			     kms->conn_props.crtc_id, kms->crtc_id);

	/* CRTC properties */
	drmModeAtomicAddProperty(req, kms->crtc_id,
			     kms->crtc_props.active,  1);
	drmModeAtomicAddProperty(req, kms->crtc_id,
			     kms->crtc_props.mode_id, kms->mode_blob_id);

	/* Primary plane properties */
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.fb_id,   fb_id);
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.crtc_id, kms->crtc_id);
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.crtc_x, 0);
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.crtc_y, 0);
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.crtc_w, kms->mode.hdisplay);
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.crtc_h, kms->mode.vdisplay);
	/* SRC_* in 16.16 fixed-point */
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.src_x, 0);
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.src_y, 0);
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.src_w,
			     (uint64_t)kms->mode.hdisplay << 16);
	drmModeAtomicAddProperty(req, kms->plane_id,
			     kms->primary_props.src_h,
			     (uint64_t)kms->mode.vdisplay << 16);

	/*
	 * TEST_ONLY: ask the kernel to validate the request without
	 * touching any hardware registers.  Returns -EINVAL if the
	 * combination of properties is rejected by the driver.
	 * This is unique to atomic -- legacy API had no dry-run path.
	 */
	ret = drmModeAtomicCommit(kms->fd, req,
				  DRM_MODE_ATOMIC_TEST_ONLY |
				  DRM_MODE_ATOMIC_ALLOW_MODESET,
				  NULL);
	if (ret) {
		fprintf(stderr, "Atomic TEST_ONLY failed: %s\n",
			strerror(-ret));
		drmModeAtomicFree(req);
		return ret;
	}
	printf("Atomic TEST_ONLY passed -- committing for real\n");

	/* Commit for real, with ALLOW_MODESET to permit clock/PLL changes */
	ret = drmModeAtomicCommit(kms->fd, req,
				  DRM_MODE_ATOMIC_ALLOW_MODESET,
				  NULL);
	if (ret)
		perror("drmModeAtomicCommit (modeset)");

	drmModeAtomicFree(req);
	return ret;
}

/* ============================================================
 * Page flip callback for atomic non-blocking commits.
 *
 * Identical in purpose to the legacy page_flip_handler, but note that
 * atomic commits can update multiple planes simultaneously.  A single
 * DRM_EVENT_FLIP_COMPLETE event is delivered for the entire commit,
 * not one per plane -- this is the "atomic" guarantee.
 * ============================================================ */
static void atomic_flip_handler(int fd, unsigned int sequence,
				unsigned int tv_sec, unsigned int tv_usec,
				unsigned int crtc_id, void *user_data)
{
	struct flip_pending *pending = user_data;
	pending->waiting = false;

	(void)fd; (void)sequence; (void)tv_sec;
	(void)tv_usec; (void)crtc_id;
}

/* ============================================================
 * run_atomic_pageflip - Non-blocking atomic page flip animation.
 *
 * Each frame builds a minimal atomic request that updates only the
 * FB_ID property of the primary plane.  No modeset flags are needed
 * because the CRTC and connector configuration is unchanged.
 *
 * DRM_MODE_ATOMIC_NONBLOCK returns immediately after queuing the flip.
 * The kernel schedules the actual register update for the next vblank,
 * then delivers DRM_EVENT_FLIP_COMPLETE via the DRM fd.
 *
 * Compared to legacy drmModePageFlip():
 *   - Can update multiple planes in one atomic operation
 *   - Can change plane position/size alongside the FB swap
 *   - All changes appear simultaneously on screen (true atomicity)
 * ============================================================ */
static void run_atomic_pageflip(struct kms_state *kms,
				struct buffer_object bufs[MAX_BUFFERS])
{
	struct animation_state anim = {
		.bar_x      = 0,
		.bar_width  = 80,
		.direction  = 1,
		.frame_count = 0,
	};
	int cur = 0;
	struct flip_pending pending = { .waiting = false };

	/*
	 * Use the v2 event context which includes crtc_id in the callback.
	 * This matters when driving multiple displays from the same fd.
	 */
	drmEventContext ev_ctx = {
		.version                 = 3,
		.page_flip_handler2      = atomic_flip_handler,
	};

	printf("\n[ATOMIC PAGE FLIP] Non-blocking vblank-synced animation\n");
	printf("Primary plane only -- Ctrl+C to stop\n\n");

	while (1) {
		int back = 1 - cur;

		draw_moving_bar(&bufs[back], &anim, 0xffffff);
		update_animation(&anim, (int)bufs[back].width);

		drmModeAtomicReq *req = drmModeAtomicAlloc();
		if (!req) break;

		/*
		 * Minimal flip request: only FB_ID changes.
		 * The kernel diffing engine compares this against the
		 * current committed state and only updates what changed.
		 */
		drmModeAtomicAddProperty(req, kms->plane_id,
				     kms->primary_props.fb_id,
				     bufs[back].fb_id);
		drmModeAtomicAddProperty(req, kms->plane_id,
				     kms->primary_props.crtc_id,
				     kms->crtc_id);

		int ret = drmModeAtomicCommit(kms->fd, req,
					      DRM_MODE_ATOMIC_NONBLOCK |
					      DRM_MODE_PAGE_FLIP_EVENT,
					      &pending);
		drmModeAtomicFree(req);

		if (ret) {
			perror("drmModeAtomicCommit (flip)");
			break;
		}
		pending.waiting = true;

		/* Wait for the vblank event before preparing the next frame */
		while (pending.waiting) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(kms->fd, &fds);

			struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
			int s = select(kms->fd + 1, &fds, NULL, NULL, &timeout);
			if (s < 0) { perror("select"); return; }
			if (s == 0) {
				fprintf(stderr, "Vblank timeout\n");
				return;
			}
			drmHandleEvent(kms->fd, &ev_ctx);
		}

		cur = back;
	}
}

/* ============================================================
 * run_multiplane - Animate primary plane while overlay stays static.
 *
 * This demonstrates the core advantage of atomic KMS: updating two
 * planes in a single commit guarantees they change on the same vblank.
 * With legacy API you would need two separate ioctls; the display
 * engine could scan out a frame showing plane A updated but plane B
 * not yet -- atomic eliminates this class of glitch entirely.
 *
 * Layout:
 *   Primary plane:  full-screen moving white bar (dark background)
 *   Overlay plane:  small red rectangle fixed at top-left corner
 *                   (if hardware overlay is available on this CRTC)
 * ============================================================ */
static void run_multiplane(struct kms_state *kms,
			   struct buffer_object primary_bufs[MAX_BUFFERS],
			   struct buffer_object *overlay_buf)
{
	struct animation_state anim = {
		.bar_x      = 0,
		.bar_width  = 80,
		.direction  = 1,
		.frame_count = 0,
	};
	int cur = 0;
	struct flip_pending pending = { .waiting = false };

	drmEventContext ev_ctx = {
		.version            = 3,
		.page_flip_handler2 = atomic_flip_handler,
	};

	/* Fill the overlay buffer with a solid red rectangle */
	memset(overlay_buf->vaddr, 0, overlay_buf->size);
	uint32_t *pix = (uint32_t *)overlay_buf->vaddr;
	for (uint32_t y = 0; y < overlay_buf->height; y++)
		for (uint32_t x = 0; x < overlay_buf->width; x++)
			pix[y * (overlay_buf->pitch / 4) + x] = 0xff0000;

	/*
	 * Commit both planes in a single atomic request.
	 * The overlay plane is configured once here at a fixed position;
	 * subsequent flips only update the primary plane FB_ID.
	 *
	 * Overlay SRC_W/SRC_H must match the overlay framebuffer dimensions.
	 * CRTC_W/CRTC_H can differ -- VOP2 will scale the overlay if needed.
	 */
	{
		uint32_t ow = overlay_buf->width;
		uint32_t oh = overlay_buf->height;

		drmModeAtomicReq *req = drmModeAtomicAlloc();

		/* Primary plane */
		drmModeAtomicAddProperty(req, kms->plane_id,
				     kms->primary_props.fb_id,
				     primary_bufs[cur].fb_id);
		drmModeAtomicAddProperty(req, kms->plane_id,
				     kms->primary_props.crtc_id, kms->crtc_id);

		/* Overlay plane */
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.fb_id,
				     overlay_buf->fb_id);
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.crtc_id, kms->crtc_id);
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.crtc_x, 50);
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.crtc_y, 50);
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.crtc_w, ow);
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.crtc_h, oh);
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.src_x, 0);
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.src_y, 0);
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.src_w,
				     (uint64_t)ow << 16);
		drmModeAtomicAddProperty(req, kms->overlay_id,
				     kms->overlay_props.src_h,
				     (uint64_t)oh << 16);

		int ret = drmModeAtomicCommit(kms->fd, req,
					      DRM_MODE_ATOMIC_ALLOW_MODESET,
					      NULL);
		drmModeAtomicFree(req);
		if (ret) {
			fprintf(stderr,
				"Overlay initial commit failed: %s\n"
				"Hardware may not support overlay plane "
				"on this CRTC -- falling back to primary only\n",
				strerror(-ret));
			kms->overlay_id = 0;
		}
	}

	printf("\n[MULTI-PLANE ATOMIC] Primary (animated) + Overlay (static red)\n");
	printf("Both planes update on the same vblank -- Ctrl+C to stop\n\n");

	while (1) {
		int back = 1 - cur;

		draw_moving_bar(&primary_bufs[back], &anim, 0xffffff);
		update_animation(&anim, (int)primary_bufs[back].width);

		drmModeAtomicReq *req = drmModeAtomicAlloc();
		if (!req) break;

		/*
		 * Only FB_ID of the primary plane changes each frame.
		 * The overlay stays at the same framebuffer and position,
		 * so it does not need to be included in every flip commit.
		 * The kernel's state machine retains the overlay configuration
		 * from the initial commit above.
		 */
		drmModeAtomicAddProperty(req, kms->plane_id,
				     kms->primary_props.fb_id,
				     primary_bufs[back].fb_id);
		drmModeAtomicAddProperty(req, kms->plane_id,
				     kms->primary_props.crtc_id, kms->crtc_id);

		int ret = drmModeAtomicCommit(kms->fd, req,
					      DRM_MODE_ATOMIC_NONBLOCK |
					      DRM_MODE_PAGE_FLIP_EVENT,
					      &pending);
		drmModeAtomicFree(req);

		if (ret) { perror("atomic flip"); break; }
		pending.waiting = true;

		while (pending.waiting) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(kms->fd, &fds);
			struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
			int s = select(kms->fd + 1, &fds, NULL, NULL, &timeout);
			if (s <= 0) return;
			drmHandleEvent(kms->fd, &ev_ctx);
		}

		cur = back;
	}
}

/* ============================================================
 * find_primary_and_overlay - Walk plane list for this CRTC.
 * @fd:          DRM file descriptor.
 * @crtc_idx:    Index into resource crtc array (used in bitmask check).
 * @primary_out: Receives the primary plane ID.
 * @overlay_out: Receives the first overlay plane ID (0 if none).
 * ============================================================ */
static int find_planes(int fd, uint32_t crtc_id, uint32_t crtc_idx,
                       uint32_t *primary_out, uint32_t *overlay_out)
{
        drmModePlaneRes *plane_res = drmModeGetPlaneResources(fd);
        if (!plane_res) return -1;

        *primary_out = 0;
        *overlay_out = 0;

        for (uint32_t i = 0; i < plane_res->count_planes; i++) {
                drmModePlane *plane =
                        drmModeGetPlane(fd, plane_res->planes[i]);
                if (!plane) continue;

                if (!(plane->possible_crtcs & (1 << crtc_idx))) {
                        drmModeFreePlane(plane);
                        continue;
                }

                drmModeObjectProperties *props =
                        drmModeObjectGetProperties(fd, plane->plane_id,
                                                   DRM_MODE_OBJECT_PLANE);
                if (!props) { drmModeFreePlane(plane); continue; }

                uint64_t type     = 0;
                uint64_t curr_crtc = 0;

                for (uint32_t p = 0; p < props->count_props; p++) {
                        drmModePropertyRes *pr =
                                drmModeGetProperty(fd, props->props[p]);
                        if (!pr) continue;
                        if (strcmp(pr->name, "type") == 0)
                                type = props->prop_values[p];
                        if (strcmp(pr->name, "CRTC_ID") == 0)
                                curr_crtc = props->prop_values[p];
                        drmModeFreeProperty(pr);
                }

                drmModeFreeObjectProperties(props);

                if (type == DRM_PLANE_TYPE_PRIMARY) {
                        /*
                         * Prefer the plane already active on this CRTC.
                         * If none is active yet, fall back to the first
                         * compatible PRIMARY plane found.
                         */
                        if (curr_crtc == crtc_id)
                                *primary_out = plane->plane_id; /* best match */
                        else if (!*primary_out)
                                *primary_out = plane->plane_id; /* fallback */
                } else if (type == DRM_PLANE_TYPE_OVERLAY && !*overlay_out) {
                        *overlay_out = plane->plane_id;
                }

                drmModeFreePlane(plane);
        }

        drmModeFreePlaneResources(plane_res);
        return (*primary_out) ? 0 : -1;
}

/* ============================================================
 * main
 * ============================================================ */
int main(int argc, char **argv)
{
	int mode_choice = 0; /* 0=discovery, 1=atomic flip, 2=multiplane */

	if (argc > 1 && strcmp(argv[1], "--atomic")      == 0) mode_choice = 1;
	if (argc > 1 && strcmp(argv[1], "--multiplane")  == 0) mode_choice = 2;

	printf("DRM Atomic KMS Demo\n");
	printf("  %s                -> property discovery (print and exit)\n",
	       argv[0]);
	printf("  %s --atomic       -> atomic page flip animation\n", argv[0]);
	printf("  %s --multiplane   -> primary + overlay plane demo\n\n",
	       argv[0]);

	struct kms_state kms = {0};

	kms.fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (kms.fd < 0) { perror("open /dev/dri/card0"); return -1; }

	/*
	 * Universal planes must be explicitly enabled to gain access to
	 * PRIMARY and CURSOR planes via the plane API.  Without this call
	 * drmModeGetPlaneResources() returns only OVERLAY planes.
	 */
	if (drmSetClientCap(kms.fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
		fprintf(stderr, "DRM_CLIENT_CAP_UNIVERSAL_PLANES not supported\n");
		return -1;
	}

	/*
	 * Enable atomic modesetting capability.  The kernel rejects atomic
	 * ioctls unless this cap is set, ensuring backwards compatibility
	 * with legacy userspace that doesn't understand atomic semantics.
	 */
	if (drmSetClientCap(kms.fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
		fprintf(stderr, "DRM_CLIENT_CAP_ATOMIC not supported\n");
		return -1;
	}

	drmModeRes *res = drmModeGetResources(kms.fd);
	if (!res) return -1;

	/* Property discovery mode needs no further setup */
	if (mode_choice == 0) {
		run_property_discovery(kms.fd, res, &kms);
		drmModeFreeResources(res);
		close(kms.fd);
		return 0;
	}

	/* Find a connected connector */
	drmModeConnector *conn = NULL;
	for (int i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(kms.fd, res->connectors[i]);
		if (conn && conn->connection == DRM_MODE_CONNECTED &&
		    conn->count_modes > 0)
			break;
		drmModeFreeConnector(conn);
		conn = NULL;
	}
	if (!conn) { fprintf(stderr, "No connected display\n"); return -1; }

	/* Find a compatible CRTC */
	drmModeEncoder *enc = NULL;
	if (conn->encoder_id)
		enc = drmModeGetEncoder(kms.fd, conn->encoder_id);
	if (!enc && conn->count_encoders > 0)
		enc = drmModeGetEncoder(kms.fd, conn->encoders[0]);

	kms.crtc_id  = 0;
	kms.crtc_idx = 0;
	for (int i = 0; i < res->count_crtcs; i++) {
		if (enc && (enc->possible_crtcs & (1 << i))) {
			kms.crtc_id  = res->crtcs[i];
			kms.crtc_idx = (uint32_t)i;
			break;
		}
	}
	if (enc) drmModeFreeEncoder(enc);
	if (!kms.crtc_id) { fprintf(stderr, "No usable CRTC\n"); return -1; }

	kms.conn_id = conn->connector_id;
	kms.mode    = conn->modes[0];

	printf("Display: %dx%d @ %u Hz  (CRTC id=%u idx=%u)\n",
	       kms.mode.hdisplay, kms.mode.vdisplay, kms.mode.vrefresh,
	       kms.crtc_id, kms.crtc_idx);

	/* Find primary and overlay planes for this CRTC */
	if (find_planes(kms.fd, kms.crtc_id, kms.crtc_idx,
			&kms.plane_id, &kms.overlay_id) < 0) {
		fprintf(stderr, "No primary plane found for CRTC\n");
		return -1;
	}
	printf("Primary plane id=%u  Overlay plane id=%u%s\n",
	       kms.plane_id, kms.overlay_id,
	       kms.overlay_id ? "" : " (none available)");

	/* Cache all property IDs */
	if (cache_connector_props(kms.fd, kms.conn_id,  &kms.conn_props)  ||
	    cache_crtc_props     (kms.fd, kms.crtc_id,  &kms.crtc_props)  ||
	    cache_plane_props    (kms.fd, kms.plane_id,  &kms.primary_props)) {
		fprintf(stderr, "Failed to cache required property IDs\n");
		return -1;
	}
	if (kms.overlay_id)
		cache_plane_props(kms.fd, kms.overlay_id, &kms.overlay_props);

	/* Allocate primary plane framebuffers */
	struct buffer_object primary_bufs[MAX_BUFFERS] = {0};
	for (int i = 0; i < MAX_BUFFERS; i++) {
		primary_bufs[i].width  = kms.mode.hdisplay;
		primary_bufs[i].height = kms.mode.vdisplay;
		if (create_fb(kms.fd, &primary_bufs[i]) < 0) {
			fprintf(stderr, "Failed to create primary fb %d\n", i);
			return -1;
		}
		memset(primary_bufs[i].vaddr, 0x20, primary_bufs[i].size);
	}

	/* Perform atomic modeset with the first framebuffer */
	if (atomic_modeset(&kms, primary_bufs[0].fb_id) < 0)
		return -1;

	if (mode_choice == 1) {
		run_atomic_pageflip(&kms, primary_bufs);
	} else {
		/* Allocate a small overlay buffer (256x256) */
		struct buffer_object overlay_buf = {
			.width  = 256,
			.height = 256,
		};
		if (kms.overlay_id && create_fb(kms.fd, &overlay_buf) == 0) {
			run_multiplane(&kms, primary_bufs, &overlay_buf);
			destroy_fb(kms.fd, &overlay_buf);
		} else {
			fprintf(stderr,
				"No overlay plane available, "
				"falling back to atomic flip\n");
			run_atomic_pageflip(&kms, primary_bufs);
		}
	}

	/* Cleanup */
	if (kms.mode_blob_id)
		drmModeDestroyPropertyBlob(kms.fd, kms.mode_blob_id);

	for (int i = 0; i < MAX_BUFFERS; i++)
		destroy_fb(kms.fd, &primary_bufs[i]);

	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	close(kms.fd);
	return 0;
}
