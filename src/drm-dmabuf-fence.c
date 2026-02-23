#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <linux/dma-buf.h>

/* ============================================================
 * DRM DMA-BUF and Fence Synchronization Demo
 *
 * Progression from previous demos:
 *   modeset-double-buffer.c    -- GEM / KMS basics
 *   drm-pageflip-vs-tearing.c  -- vblank sync, tearing
 *   drm-atomic-demo.c          -- atomic commit, properties
 *   drm-dmabuf-fence.c         -- DMA-BUF sharing, implicit/explicit fence
 *
 * Core concept: DMA-BUF is a kernel mechanism that lets different devices
 * share the same physical memory without copying.  A buffer exported from
 * one device fd can be imported by another device fd; both get independent
 * GEM handles that map to the same underlying pages.
 *
 * This demo uses two fds to /dev/dri/card0 to model a producer/consumer
 * pipeline -- in real BSP work this would be GPU fd + display fd, or
 * ISP fd + display fd.  The fd separation is what matters conceptually.
 *
 * Three runnable modes:
 *   (default)      DMA-BUF export/import, verify shared memory, display
 *   --nosync       Write to active scanout buffer with no fence (artifacts)
 *   --fence        Explicit fence via IN_FENCE_FD plane property
 *
 * Tested on RK3588 / VOP2 with Ubuntu Lite (no compositor).
 * ============================================================ */

#define MAX_BUFFERS 2

/* ============================================================
 * KMS pipeline state -- same pattern as drm-atomic-demo.c
 * ============================================================ */
struct plane_props {
	uint32_t fb_id;
	uint32_t crtc_id;
	uint32_t crtc_x, crtc_y, crtc_w, crtc_h;
	uint32_t src_x,  src_y,  src_w,  src_h;
	uint32_t in_fence_fd; /* Explicit fence input property */
};

struct crtc_props {
	uint32_t active;
	uint32_t mode_id;
	uint32_t out_fence_ptr; /* Explicit fence output property */
};

struct connector_props {
	uint32_t crtc_id;
};

struct kms_state {
	int display_fd;  /* fd for display/KMS operations (/dev/dri/card0) */

	uint32_t conn_id;
	uint32_t crtc_id;
	uint32_t crtc_idx;
	uint32_t plane_id;

	drmModeModeInfo  mode;
	uint32_t         mode_blob_id;

	struct connector_props conn_props;
	struct crtc_props      crtc_props;
	struct plane_props     primary_props;
};

/* ============================================================
 * dmabuf_buffer - Represents a GEM buffer exported as a DMA-BUF.
 *
 * The same physical memory is accessible through two different handles:
 *   producer_handle -- GEM handle on fd_producer (the writer)
 *   display_handle  -- GEM handle on display_fd  (the display engine)
 *   dmabuf_fd       -- the DMA-BUF file descriptor that bridges them
 *
 * This models the real-world case where a GPU or ISP produces a frame
 * and the display controller consumes it without any memory copy.
 * ============================================================ */
struct dmabuf_buffer {
	/* Producer side (models GPU / ISP / camera) */
	int      fd_producer;    /* Second open() of /dev/dri/card0 */
	uint32_t producer_handle;
	uint32_t producer_pitch;
	uint32_t producer_size;
	uint8_t  *producer_vaddr;

	/* DMA-BUF bridge */
	int      dmabuf_fd;      /* Exported via DRM_IOCTL_PRIME_HANDLE_TO_FD */

	/* Display side */
	uint32_t display_handle; /* Imported via DRM_IOCTL_PRIME_FD_TO_HANDLE */
	uint32_t fb_id;          /* Registered with drmModeAddFB() */

	uint32_t width;
	uint32_t height;
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
 * get_property_id / cache helpers -- identical to drm-atomic-demo.c
 * ============================================================ */
static int get_property_id(int fd, drmModeObjectProperties *props,
			   const char *name, uint32_t *id_out)
{
	for (uint32_t i = 0; i < props->count_props; i++) {
		drmModePropertyRes *prop =
			drmModeGetProperty(fd, props->props[i]);
		if (!prop) continue;
		if (strcmp(prop->name, name) == 0) {
			*id_out = prop->prop_id;
			drmModeFreeProperty(prop);
			return 0;
		}
		drmModeFreeProperty(prop);
	}
	return -1;
}

static int cache_plane_props(int fd, uint32_t plane_id, struct plane_props *p)
{
	drmModeObjectProperties *props =
		drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
	if (!props) return -1;

	int ret = 0;
	ret |= get_property_id(fd, props, "FB_ID",       &p->fb_id);
	ret |= get_property_id(fd, props, "CRTC_ID",     &p->crtc_id);
	ret |= get_property_id(fd, props, "CRTC_X",      &p->crtc_x);
	ret |= get_property_id(fd, props, "CRTC_Y",      &p->crtc_y);
	ret |= get_property_id(fd, props, "CRTC_W",      &p->crtc_w);
	ret |= get_property_id(fd, props, "CRTC_H",      &p->crtc_h);
	ret |= get_property_id(fd, props, "SRC_X",       &p->src_x);
	ret |= get_property_id(fd, props, "SRC_Y",       &p->src_y);
	ret |= get_property_id(fd, props, "SRC_W",       &p->src_w);
	ret |= get_property_id(fd, props, "SRC_H",       &p->src_h);

	/*
	 * IN_FENCE_FD is the explicit fence input property on a plane.
	 * Setting it to a sync_file fd tells VOP2: "wait for this fence
	 * to signal before scanning out this framebuffer."
	 * Not all drivers expose this property; failure here is non-fatal.
	 */
	if (get_property_id(fd, props, "IN_FENCE_FD", &p->in_fence_fd) < 0) {
		p->in_fence_fd = 0;
		printf("  Note: IN_FENCE_FD property not available on this plane\n");
	}

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

	/*
	 * OUT_FENCE_PTR is a CRTC property that accepts a userspace pointer.
	 * The kernel writes a sync_file fd into it after each atomic commit,
	 * representing a fence that signals when the committed frame is
	 * actually being scanned out.  This is the "display done" signal
	 * that a compositor uses to know when a buffer is safe to recycle.
	 */
	if (get_property_id(fd, props, "OUT_FENCE_PTR", &p->out_fence_ptr) < 0)
		p->out_fence_ptr = 0;

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
 * dmabuf_create - Allocate a GEM buffer on fd_producer and export
 *                 it as a DMA-BUF, then import it on display_fd.
 *
 * After this function returns, buf->producer_vaddr is writable by
 * the CPU (simulating a GPU/ISP write), and buf->fb_id is registered
 * with the display engine for scanout -- all pointing to the same
 * physical pages.
 *
 * Kernel path:
 *   DRM_IOCTL_MODE_CREATE_DUMB    allocate GEM object on producer fd
 *   DRM_IOCTL_PRIME_HANDLE_TO_FD  export GEM handle as DMA-BUF fd
 *                                  (increments buffer's reference count)
 *   DRM_IOCTL_PRIME_FD_TO_HANDLE  import DMA-BUF fd on display fd
 *                                  (driver creates a new local GEM handle
 *                                   pointing to the same physical pages)
 *   drmModeAddFB()                register as KMS framebuffer
 * ============================================================ */
static int dmabuf_create(struct dmabuf_buffer *buf, int display_fd,
			 int fd_producer, uint32_t width, uint32_t height)
{
	buf->fd_producer = fd_producer;
	buf->width       = width;
	buf->height      = height;

	/* Step 1: Allocate GEM buffer on the producer fd */
	struct drm_mode_create_dumb create = {
		.width  = width,
		.height = height,
		.bpp    = 32,
	};
	if (drmIoctl(fd_producer, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
		perror("CREATE_DUMB on producer");
		return -1;
	}
	buf->producer_handle = create.handle;
	buf->producer_pitch  = create.pitch;
	buf->producer_size   = create.size;

	/* Step 2: Map producer buffer for CPU writes */
	struct drm_mode_map_dumb map = { .handle = create.handle };
	if (drmIoctl(fd_producer, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
		perror("MAP_DUMB on producer");
		return -1;
	}
	buf->producer_vaddr = mmap(0, create.size,
				   PROT_READ | PROT_WRITE, MAP_SHARED,
				   fd_producer, map.offset);
	if (buf->producer_vaddr == MAP_FAILED) {
		perror("mmap producer");
		return -1;
	}

	/*
	 * Step 3: Export the GEM buffer as a DMA-BUF file descriptor.
	 *
	 * DRM_CLOEXEC ensures the fd is not inherited across exec().
	 * DRM_RDWR allows both read and write access for the importer.
	 *
	 * The returned dmabuf_fd is a regular file descriptor that
	 * represents a reference to the underlying memory.  It can be
	 * passed to any process or driver that understands DMA-BUF.
	 */
	if (drmPrimeHandleToFD(fd_producer, buf->producer_handle,
			       DRM_CLOEXEC | DRM_RDWR,
			       &buf->dmabuf_fd) < 0) {
		perror("PRIME_HANDLE_TO_FD (export)");
		return -1;
	}
	printf("  DMA-BUF exported: producer GEM handle=%u -> dmabuf_fd=%d\n",
	       buf->producer_handle, buf->dmabuf_fd);

	/*
	 * Step 4: Import the DMA-BUF on the display fd.
	 *
	 * The kernel looks up the physical pages backing dmabuf_fd and
	 * creates a new GEM handle on display_fd that maps to them.
	 * No memory allocation or copy occurs -- this is zero-copy sharing.
	 */
	if (drmPrimeFDToHandle(display_fd, buf->dmabuf_fd,
			       &buf->display_handle) < 0) {
		perror("PRIME_FD_TO_HANDLE (import)");
		return -1;
	}
	printf("  DMA-BUF imported: dmabuf_fd=%d -> display GEM handle=%u\n",
	       buf->dmabuf_fd, buf->display_handle);

	/*
	 * Step 5: Register the imported GEM handle as a KMS framebuffer.
	 * The display engine now knows this buffer's dimensions and format
	 * and can use it as a scanout source.
	 */
	if (drmModeAddFB(display_fd, width, height, 24, 32,
			 buf->producer_pitch, buf->display_handle,
			 &buf->fb_id) < 0) {
		perror("drmModeAddFB on imported buffer");
		return -1;
	}
	printf("  Framebuffer registered: fb_id=%u  (width=%u height=%u)\n",
	       buf->fb_id, width, height);

	return 0;
}

static void dmabuf_destroy(struct dmabuf_buffer *buf, int display_fd)
{
	drmModeRmFB(display_fd, buf->fb_id);
	close(buf->dmabuf_fd);

	/* Release the display-side GEM handle */
	struct drm_gem_close close_display = { .handle = buf->display_handle };
	drmIoctl(display_fd, DRM_IOCTL_GEM_CLOSE, &close_display);

	/* Release the producer-side GEM object */
	munmap(buf->producer_vaddr, buf->producer_size);
	struct drm_mode_destroy_dumb destroy = { .handle = buf->producer_handle };
	drmIoctl(buf->fd_producer, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

/* ============================================================
 * draw_frame - CPU writes to the producer-side mapping.
 *
 * In real hardware this would be a GPU command buffer submission
 * or an ISP DMA write.  The write goes directly to the physical
 * pages shared with the display engine -- no copy involved.
 *
 * The DMA_BUF_IOCTL_SYNC calls implement implicit fence semantics:
 *   SYNC_START: inform the kernel the CPU is about to access the buffer
 *               (waits for any pending GPU/display fence to signal)
 *   SYNC_END:   inform the kernel the CPU access is complete
 *               (allows the display engine to begin reading)
 *
 * On a unified-memory SoC like RK3588 these translate to cache
 * maintenance operations (clean/invalidate) rather than actual waits,
 * but the ordering guarantees are the same as on discrete hardware.
 * ============================================================ */
static void draw_frame(struct dmabuf_buffer *buf,
		       struct animation_state *anim,
		       uint32_t bar_color)
{
	/*
	 * Implicit fence: acquire write access.
	 * On RK3588 this issues a cache invalidate so the CPU sees
	 * any previous display DMA writes to this buffer.
	 */
	struct dma_buf_sync sync_start = {
		.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE,
	};
	ioctl(buf->dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync_start);

	uint32_t *pixel   = (uint32_t *)buf->producer_vaddr;
	uint32_t bg_color = 0x202020;

	for (uint32_t y = 0; y < buf->height; y++) {
		for (uint32_t x = 0; x < buf->width; x++) {
			uint32_t offset = y * (buf->producer_pitch / 4) + x;
			if ((int)x >= anim->bar_x &&
			    (int)x <  anim->bar_x + anim->bar_width)
				pixel[offset] = bar_color;
			else
				pixel[offset] = bg_color;
		}
	}

	/*
	 * Implicit fence: release write access.
	 * On RK3588 this issues a cache clean so the display DMA
	 * sees the pixel data written above.  On discrete GPU hardware
	 * this would signal the fence that the display engine is waiting on.
	 */
	struct dma_buf_sync sync_end = {
		.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE,
	};
	ioctl(buf->dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync_end);
}

static void draw_frame_nosync(struct dmabuf_buffer *buf,
			      struct animation_state *anim,
			      uint32_t bar_color)
{
	/*
	 * Write directly without DMA_BUF_IOCTL_SYNC.
	 * On discrete hardware this causes visible corruption because
	 * the CPU cache is not flushed before display DMA reads the buffer.
	 * On RK3588 the effect is subtler but the race condition is real.
	 */
	uint32_t *pixel   = (uint32_t *)buf->producer_vaddr;
	uint32_t bg_color = 0x202020;

	for (uint32_t y = 0; y < buf->height; y++) {
		for (uint32_t x = 0; x < buf->width; x++) {
			uint32_t offset = y * (buf->producer_pitch / 4) + x;
			if ((int)x >= anim->bar_x &&
			    (int)x <  anim->bar_x + anim->bar_width)
				pixel[offset] = bar_color;
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
 * atomic_modeset - same pattern as drm-atomic-demo.c
 * ============================================================ */
static int atomic_modeset(struct kms_state *kms, uint32_t fb_id)
{
	int ret = drmModeCreatePropertyBlob(kms->display_fd, &kms->mode,
					    sizeof(kms->mode),
					    &kms->mode_blob_id);
	if (ret) { perror("CreatePropertyBlob"); return ret; }

	drmModeAtomicReq *req = drmModeAtomicAlloc();
	if (!req) return -ENOMEM;

	drmModeAtomicAddProperty(req, kms->conn_id,
				 kms->conn_props.crtc_id,  kms->crtc_id);
	drmModeAtomicAddProperty(req, kms->crtc_id,
				 kms->crtc_props.active,   1);
	drmModeAtomicAddProperty(req, kms->crtc_id,
				 kms->crtc_props.mode_id,  kms->mode_blob_id);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.fb_id,   fb_id);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.crtc_id, kms->crtc_id);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.crtc_x,  0);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.crtc_y,  0);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.crtc_w,  kms->mode.hdisplay);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.crtc_h,  kms->mode.vdisplay);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.src_x,   0);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.src_y,   0);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.src_w,
				 (uint64_t)kms->mode.hdisplay << 16);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.src_h,
				 (uint64_t)kms->mode.vdisplay << 16);

	ret = drmModeAtomicCommit(kms->display_fd, req,
				  DRM_MODE_ATOMIC_TEST_ONLY |
				  DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	if (ret) {
		fprintf(stderr, "TEST_ONLY failed: %s\n", strerror(-ret));
		drmModeAtomicFree(req);
		return ret;
	}

	ret = drmModeAtomicCommit(kms->display_fd, req,
				  DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	if (ret) perror("atomic modeset commit");

	drmModeAtomicFree(req);
	return ret;
}

static void flip_handler(int fd, unsigned int seq, unsigned int tv_sec,
			 unsigned int tv_usec, unsigned int crtc_id,
			 void *user_data)
{
	struct flip_pending *p = user_data;
	p->waiting = false;
	(void)fd; (void)seq; (void)tv_sec; (void)tv_usec; (void)crtc_id;
}

/* ============================================================
 * run_dmabuf_demo - Basic DMA-BUF sharing with implicit fence (SYNC ioctl).
 *
 * Each frame:
 *   1. CPU writes to producer_vaddr (with SYNC_START / SYNC_END)
 *   2. Atomic commit flips to the display-side fb_id
 *   3. Wait for vblank event before next frame
 *
 * The producer and display sides share physical memory; the SYNC ioctls
 * provide the implicit fence ordering guarantee.
 * ============================================================ */
static void run_dmabuf_demo(struct kms_state *kms,
			    struct dmabuf_buffer bufs[MAX_BUFFERS])
{
	struct animation_state anim = {
		.bar_x = 0, .bar_width = 80, .direction = 1
	};
	uint32_t colors[MAX_BUFFERS] = { 0xffffff, 0x00ff88 };
	int cur = 0;
	struct flip_pending pending = { .waiting = false };
	drmEventContext ev_ctx = {
		.version            = 3,
		.page_flip_handler2 = flip_handler,
	};

	printf("\n[DMA-BUF IMPLICIT FENCE] SYNC_START/SYNC_END around CPU writes\n");
	printf("White/green bar alternates between two shared buffers -- Ctrl+C to stop\n\n");

	while (1) {
		int back = 1 - cur;

		/*
		 * Producer writes to the back buffer.
		 * draw_frame() brackets the write with DMA_BUF_IOCTL_SYNC,
		 * ensuring cache coherency between CPU write and display DMA read.
		 */
		draw_frame(&bufs[back], &anim, colors[back]);
		update_animation(&anim, (int)bufs[back].width);

		drmModeAtomicReq *req = drmModeAtomicAlloc();
		if (!req) break;

		drmModeAtomicAddProperty(req, kms->plane_id,
					 kms->primary_props.fb_id,
					 bufs[back].fb_id);
		drmModeAtomicAddProperty(req, kms->plane_id,
					 kms->primary_props.crtc_id,
					 kms->crtc_id);

		int ret = drmModeAtomicCommit(kms->display_fd, req,
					      DRM_MODE_ATOMIC_NONBLOCK |
					      DRM_MODE_PAGE_FLIP_EVENT,
					      &pending);
		drmModeAtomicFree(req);
		if (ret) { perror("atomic flip"); break; }
		pending.waiting = true;

		while (pending.waiting) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(kms->display_fd, &fds);
			struct timeval timeout = { .tv_sec = 1 };
			int s = select(kms->display_fd + 1, &fds,
				       NULL, NULL, &timeout);
			if (s <= 0) { fprintf(stderr, "vblank timeout\n"); return; }
			drmHandleEvent(kms->display_fd, &ev_ctx);
		}

		cur = back;
	}
}

/* ============================================================
 * run_nosync_demo - Write without DMA_BUF_IOCTL_SYNC (no fence).
 *
 * This is the DMA-BUF equivalent of the single-buffer tearing demo.
 * The CPU writes to the active scanout buffer without the SYNC ioctl,
 * so no cache maintenance is performed.  On discrete hardware this
 * causes visible corruption.  On RK3588's unified memory the CPU
 * cache line writeback may hide the artifact, but the race is real.
 *
 * This mode exists to demonstrate WHY DMA_BUF_IOCTL_SYNC matters.
 * ============================================================ */
static void run_nosync_demo(struct kms_state *kms,
			    struct dmabuf_buffer *buf)
{
	struct animation_state anim = {
		.bar_x = 0, .bar_width = 80, .direction = 1
	};

	printf("\n[DMA-BUF NO SYNC] Writing to active scanout buffer without fence\n");
	printf("On discrete GPU hardware this causes visible corruption\n");
	printf("On RK3588 unified memory the cache race may be subtle -- Ctrl+C to stop\n\n");

	/* Display this single buffer once and never change the CRTC */
	drmModeAtomicReq *req = drmModeAtomicAlloc();
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.fb_id,   buf->fb_id);
	drmModeAtomicAddProperty(req, kms->plane_id,
				 kms->primary_props.crtc_id, kms->crtc_id);
	drmModeAtomicCommit(kms->display_fd, req,
			    DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	drmModeAtomicFree(req);

	while (1) {
		draw_frame_nosync(buf, &anim, 0xff4400);
		update_animation(&anim, (int)buf->width);
		/* No sleep, no sync -- maximum race condition exposure */
	}
}

/* ============================================================
 * run_explicit_fence_demo - Demonstrate OUT_FENCE_PTR + IN_FENCE_FD.
 *
 * Explicit fences give userspace precise control over the
 * producer/consumer synchronization timeline:
 *
 *   OUT_FENCE_PTR (CRTC property):
 *     A userspace pointer.  After each atomic commit the kernel writes
 *     a sync_file fd into it.  This fd represents a fence that signals
 *     when the committed frame is actually being scanned out (i.e., the
 *     previous buffer is no longer in use and can be recycled).
 *
 *   IN_FENCE_FD (plane property):
 *     A sync_file fd passed to the kernel with an atomic commit.
 *     VOP2 will not begin reading the new framebuffer until this fence
 *     signals.  This is how a GPU tells the display engine "wait until
 *     I have finished rendering before you start scanning out."
 *
 * In this demo we use the out-fence from frame N as the in-fence for
 * frame N+1, creating a strict pipeline ordering.
 * ============================================================ */
static void run_explicit_fence_demo(struct kms_state *kms,
				    struct dmabuf_buffer bufs[MAX_BUFFERS])
{
	if (!kms->primary_props.in_fence_fd || !kms->crtc_props.out_fence_ptr) {
		fprintf(stderr,
			"IN_FENCE_FD or OUT_FENCE_PTR not available on this hardware\n"
			"Falling back to implicit fence demo\n");
		run_dmabuf_demo(kms, bufs);
		return;
	}

	struct animation_state anim = {
		.bar_x = 0, .bar_width = 80, .direction = 1
	};
	int cur       = 0;
	int out_fence = -1; /* sync_file fd received from the kernel */
	struct flip_pending pending = { .waiting = false };
	drmEventContext ev_ctx = {
		.version            = 3,
		.page_flip_handler2 = flip_handler,
	};

	printf("\n[EXPLICIT FENCE] OUT_FENCE_PTR -> IN_FENCE_FD pipeline\n");
	printf("Each frame's out-fence becomes the next frame's in-fence\n");
	printf("This is how Wayland compositors synchronize GPU and display -- Ctrl+C to stop\n\n");

	while (1) {
		int back = 1 - cur;

		draw_frame(&bufs[back], &anim, 0x4488ff);
		update_animation(&anim, (int)bufs[back].width);

		drmModeAtomicReq *req = drmModeAtomicAlloc();
		if (!req) break;

		/*
		 * OUT_FENCE_PTR: pass a pointer to out_fence.
		 * After this commit the kernel will write a new sync_file fd
		 * into out_fence representing "frame back is on screen."
		 */
		int new_out_fence = -1;
		drmModeAtomicAddProperty(req, kms->crtc_id,
					 kms->crtc_props.out_fence_ptr,
					 (uint64_t)(uintptr_t)&new_out_fence);

		drmModeAtomicAddProperty(req, kms->plane_id,
					 kms->primary_props.fb_id,
					 bufs[back].fb_id);
		drmModeAtomicAddProperty(req, kms->plane_id,
					 kms->primary_props.crtc_id,
					 kms->crtc_id);

		/*
		 * IN_FENCE_FD: pass the out-fence from the previous frame.
		 * VOP2 will not start reading bufs[back] until this fence
		 * signals, guaranteeing the display engine has finished with
		 * the buffer we are about to overwrite.
		 *
		 * Value -1 means "no fence, proceed immediately" which is
		 * correct for the very first frame.
		 */
		if (kms->primary_props.in_fence_fd) {
			drmModeAtomicAddProperty(req, kms->plane_id,
						 kms->primary_props.in_fence_fd,
						 (uint64_t)(int64_t)out_fence);
		}

		int ret = drmModeAtomicCommit(kms->display_fd, req,
					      DRM_MODE_ATOMIC_NONBLOCK |
					      DRM_MODE_PAGE_FLIP_EVENT,
					      &pending);
		drmModeAtomicFree(req);

		/* Close the fence fd we just consumed as IN_FENCE_FD */
		if (out_fence >= 0) {
			close(out_fence);
			out_fence = -1;
		}

		if (ret) { perror("atomic flip (explicit fence)"); break; }

		/*
		 * new_out_fence now holds the kernel-written sync_file fd.
		 * Save it to use as IN_FENCE_FD for the next frame.
		 */
		out_fence = new_out_fence;
		if (out_fence >= 0)
			printf("  Frame %d: out_fence_fd=%d\n",
			       anim.frame_count, out_fence);

		pending.waiting = true;
		while (pending.waiting) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(kms->display_fd, &fds);
			struct timeval timeout = { .tv_sec = 1 };
			int s = select(kms->display_fd + 1, &fds,
				       NULL, NULL, &timeout);
			if (s <= 0) { fprintf(stderr, "vblank timeout\n"); goto out; }
			drmHandleEvent(kms->display_fd, &ev_ctx);
		}

		cur = back;
	}
out:
	if (out_fence >= 0) close(out_fence);
}

/* ============================================================
 * find_active_primary_plane - same logic as drm-atomic-demo.c fix
 * ============================================================ */
static int find_active_primary_plane(int fd, uint32_t crtc_id,
				     uint32_t crtc_idx, uint32_t *plane_out)
{
	drmModePlaneRes *plane_res = drmModeGetPlaneResources(fd);
	if (!plane_res) return -1;

	*plane_out = 0;

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

		uint64_t type = 0, curr_crtc = 0;
		for (uint32_t p = 0; p < props->count_props; p++) {
			drmModePropertyRes *pr =
				drmModeGetProperty(fd, props->props[p]);
			if (!pr) continue;
			if (strcmp(pr->name, "type")    == 0) type      = props->prop_values[p];
			if (strcmp(pr->name, "CRTC_ID") == 0) curr_crtc = props->prop_values[p];
			drmModeFreeProperty(pr);
		}
		drmModeFreeObjectProperties(props);

		if (type == DRM_PLANE_TYPE_PRIMARY) {
			if (curr_crtc == crtc_id)
				*plane_out = plane->plane_id;
			else if (!*plane_out)
				*plane_out = plane->plane_id;
		}

		drmModeFreePlane(plane);
	}

	drmModeFreePlaneResources(plane_res);
	return (*plane_out) ? 0 : -1;
}

/* ============================================================
 * main
 * ============================================================ */
int main(int argc, char **argv)
{
	int mode = 0; /* 0=dmabuf+implicit fence, 1=nosync, 2=explicit fence */

	if (argc > 1 && strcmp(argv[1], "--nosync") == 0) mode = 1;
	if (argc > 1 && strcmp(argv[1], "--fence")  == 0) mode = 2;

	printf("DRM DMA-BUF and Fence Synchronization Demo\n");
	printf("  %s            -> DMA-BUF sharing + implicit fence (SYNC ioctl)\n",
	       argv[0]);
	printf("  %s --nosync   -> No fence (demonstrates why SYNC matters)\n",
	       argv[0]);
	printf("  %s --fence    -> Explicit fence via IN_FENCE_FD / OUT_FENCE_PTR\n\n",
	       argv[0]);

	struct kms_state kms = {0};

	/*
	 * Open two independent file descriptors to the same DRM device.
	 * display_fd owns the KMS/atomic pipeline.
	 * fd_producer models the buffer producer (GPU, ISP, camera, etc).
	 * In real BSP work these would be fds to different device nodes.
	 */
	kms.display_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (kms.display_fd < 0) { perror("open card0 (display)"); return -1; }

	int fd_producer = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd_producer < 0) { perror("open card0 (producer)"); return -1; }

	printf("Opened two fds to /dev/dri/card0:\n");
	printf("  display_fd=%d  (KMS / atomic commit)\n", kms.display_fd);
	printf("  fd_producer=%d (buffer producer / writer)\n\n", fd_producer);

	if (drmSetClientCap(kms.display_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) ||
	    drmSetClientCap(kms.display_fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
		fprintf(stderr, "Atomic/universal planes not supported\n");
		return -1;
	}

	drmModeRes *res = drmModeGetResources(kms.display_fd);
	if (!res) return -1;

	/* Find connected connector */
	drmModeConnector *conn = NULL;
	for (int i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(kms.display_fd, res->connectors[i]);
		if (conn && conn->connection == DRM_MODE_CONNECTED &&
		    conn->count_modes > 0)
			break;
		drmModeFreeConnector(conn);
		conn = NULL;
	}
	if (!conn) { fprintf(stderr, "No connected display\n"); return -1; }

	/* Find CRTC */
	drmModeEncoder *enc = NULL;
	if (conn->encoder_id)
		enc = drmModeGetEncoder(kms.display_fd, conn->encoder_id);
	if (!enc && conn->count_encoders > 0)
		enc = drmModeGetEncoder(kms.display_fd, conn->encoders[0]);

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

	printf("Display: %dx%d @ %uHz  CRTC id=%u\n",
	       kms.mode.hdisplay, kms.mode.vdisplay,
	       kms.mode.vrefresh, kms.crtc_id);

	if (find_active_primary_plane(kms.display_fd, kms.crtc_id, kms.crtc_idx,
				      &kms.plane_id) < 0) {
		fprintf(stderr, "No primary plane found\n");
		return -1;
	}
	printf("Primary plane id=%u\n\n", kms.plane_id);

	/* Cache property IDs */
	if (cache_connector_props(kms.display_fd, kms.conn_id, &kms.conn_props) ||
	    cache_crtc_props(kms.display_fd, kms.crtc_id, &kms.crtc_props)      ||
	    cache_plane_props(kms.display_fd, kms.plane_id, &kms.primary_props)) {
		fprintf(stderr, "Failed to cache property IDs\n");
		return -1;
	}

	/*
	 * Allocate DMA-BUF backed framebuffers.
	 * Each buffer is created on fd_producer and imported on display_fd,
	 * demonstrating the full zero-copy sharing path.
	 */
	printf("=== DMA-BUF Buffer Allocation ===\n");
	struct dmabuf_buffer bufs[MAX_BUFFERS] = {0};
	for (int i = 0; i < MAX_BUFFERS; i++) {
		printf("Buffer [%d]:\n", i);
		if (dmabuf_create(&bufs[i], kms.display_fd, fd_producer,
				  kms.mode.hdisplay, kms.mode.vdisplay) < 0) {
			fprintf(stderr, "Failed to create DMA-BUF buffer %d\n", i);
			return -1;
		}
		memset(bufs[i].producer_vaddr, 0x20, bufs[i].producer_size);
	}

	printf("\n=== Memory Sharing Verification ===\n");
	printf("Buffer [0]: producer_handle=%u  dmabuf_fd=%d  display_handle=%u  fb_id=%u\n",
	       bufs[0].producer_handle, bufs[0].dmabuf_fd,
	       bufs[0].display_handle,  bufs[0].fb_id);
	printf("Buffer [1]: producer_handle=%u  dmabuf_fd=%d  display_handle=%u  fb_id=%u\n",
	       bufs[1].producer_handle, bufs[1].dmabuf_fd,
	       bufs[1].display_handle,  bufs[1].fb_id);
	printf("producer_handle != display_handle : different GEM namespaces\n");
	printf("dmabuf_fd bridges them            : same physical pages\n\n");

	/* Initial modeset */
	if (atomic_modeset(&kms, bufs[0].fb_id) < 0)
		return -1;

	/* Run selected mode */
	if (mode == 0)
		run_dmabuf_demo(&kms, bufs);
	else if (mode == 1)
		run_nosync_demo(&kms, &bufs[0]);
	else
		run_explicit_fence_demo(&kms, bufs);

	/* Cleanup */
	if (kms.mode_blob_id)
		drmModeDestroyPropertyBlob(kms.display_fd, kms.mode_blob_id);
	for (int i = 0; i < MAX_BUFFERS; i++)
		dmabuf_destroy(&bufs[i], kms.display_fd);
	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	close(fd_producer);
	close(kms.display_fd);
	return 0;
}
