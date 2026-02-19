#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
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

#define MAX_BUFFERS 2

/*
 * animation_state - Tracks the position and direction of the moving bar.
 *
 * The moving bar is the key ingredient for visible tearing: a static image
 * would never reveal the artifact because there is nothing to mis-align
 * between the two halves of a frame.
 */
struct animation_state {
	int bar_x;       /* Current X position of the leading edge */
	int bar_width;   /* Width of the bar in pixels              */
	int direction;   /* +1 = moving right, -1 = moving left     */
	int frame_count; /* Total frames rendered so far            */
};

struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;  /* Row stride in bytes (may include padding) */
	uint32_t handle; /* GEM object handle returned by the kernel  */
	uint32_t size;   /* Total buffer size in bytes                */
	uint8_t  *vaddr; /* CPU-accessible mapping via mmap()         */
	uint32_t fb_id;  /* DRM framebuffer ID registered with KMS    */
};

/*
 * flip_pending - Shared state between the main loop and the page-flip callback.
 *
 * The main loop sets 'waiting = true' before calling drmModePageFlip().
 * The kernel delivers a DRM_EVENT_FLIP_COMPLETE event on the next vblank,
 * which drives page_flip_handler() via drmHandleEvent(), clearing the flag.
 */
struct flip_pending {
	bool waiting;
};

/* ============================================================
 * draw_moving_bar - Renders a white vertical bar on a dark background.
 * @bo:   The buffer object to draw into.
 * @anim: Current animation state describing bar position.
 *
 * When tearing occurs, the scanout hardware is reading from this buffer
 * mid-update.  The horizontal discontinuity in the bar's position between
 * the upper and lower halves of the screen is what makes tearing visible.
 * ============================================================ */
static void draw_moving_bar(struct buffer_object *bo,
			    struct animation_state *anim)
{
	uint32_t *pixel    = (uint32_t *)bo->vaddr;
	uint32_t bg_color  = 0x202020; /* Dark grey background */
	uint32_t bar_color = 0xffffff; /* White moving bar     */

	for (uint32_t y = 0; y < bo->height; y++) {
		for (uint32_t x = 0; x < bo->width; x++) {
			uint32_t offset = y * (bo->pitch / 4) + x;
			if ((int)x >= anim->bar_x &&
			    (int)x <  anim->bar_x + anim->bar_width)
				pixel[offset] = bar_color;
			else
				pixel[offset] = bg_color;
		}
	}
}

/* ============================================================
 * update_animation - Advances the bar position by one frame.
 * @anim:         Current animation state (modified in-place).
 * @screen_width: Horizontal resolution used for bounce detection.
 * ============================================================ */
static void update_animation(struct animation_state *anim, int screen_width)
{
	anim->bar_x += anim->direction * 8; /* 8-pixel step per frame */

	if (anim->bar_x + anim->bar_width >= screen_width) {
		anim->bar_x = screen_width - anim->bar_width;
		anim->direction = -1;
	} else if (anim->bar_x <= 0) {
		anim->bar_x = 0;
		anim->direction = 1;
	}
	anim->frame_count++;
}

static int modeset_create_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_create_dumb create = {
		.width  = bo->width,
		.height = bo->height,
		.bpp    = 32,
	};
	struct drm_mode_map_dumb map = {0};

	/*
	 * DRM_IOCTL_MODE_CREATE_DUMB allocates a GEM buffer object in the
	 * kernel.  The driver fills in pitch and size based on alignment
	 * requirements (e.g., VOP2 on RK3588 requires 64-byte row alignment).
	 */
	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0)
		return -1;

	bo->pitch  = create.pitch;
	bo->size   = create.size;
	bo->handle = create.handle;

	/*
	 * drmModeAddFB() registers the GEM buffer as a KMS framebuffer,
	 * associating pixel format metadata so the display engine knows
	 * how to interpret the raw memory during scanout.
	 */
	if (drmModeAddFB(fd, bo->width, bo->height, 24, 32,
			 bo->pitch, bo->handle, &bo->fb_id))
		return -1;

	/*
	 * DRM_IOCTL_MODE_MAP_DUMB returns a fake offset suitable for mmap().
	 * The kernel sets up a CPU mapping through the GEM subsystem; on
	 * discrete GPUs this involves cache-coherent GART, on RK3588 the
	 * buffer lives in regular DRAM shared with the display controller.
	 */
	map.handle = bo->handle;
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0)
		return -1;

	bo->vaddr = mmap(0, bo->size, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, map.offset);
	if (bo->vaddr == MAP_FAILED)
		return -1;

	return 0;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = { .handle = bo->handle };
	drmModeRmFB(fd, bo->fb_id);
	munmap(bo->vaddr, bo->size);
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

/* ============================================================
 * run_tearing_demo - Deliberately induces screen tearing.
 *
 * drmModeSetCrtc() is called in a tight loop without waiting for vblank.
 * The CRTC's scanout pointer is updated while the display engine is
 * actively reading pixels, causing the hardware to fetch the upper rows
 * from the old framebuffer and the lower rows from the new one.
 *
 * The horizontal discontinuity in the white bar's position is the
 * classic tearing artifact this demo is designed to expose.
 * ============================================================ */
static void run_tearing_demo(int fd, uint32_t crtc_id, uint32_t conn_id,
			     drmModeModeInfo *mode,
			     struct buffer_object bufs[MAX_BUFFERS])
{
	struct animation_state anim = {
		.bar_x      = 0,
		.bar_width  = 80,
		.direction  = 1,
		.frame_count = 0,
	};
	int cur = 0;

	printf("\n[TEARING MODE] Running without vblank sync - Ctrl+C to stop\n");
	printf("Watch the white bar for a horizontal split/offset (the tear line)\n\n");

	while (1) {
		int back = 1 - cur;

		draw_moving_bar(&bufs[back], &anim);
		update_animation(&anim, (int)bufs[back].width);

		/*
		 * SetCrtc reconfigures the CRTC immediately with no regard for
		 * the current scanout position.  If the electron beam (or LCD
		 * scan) is partway through the screen, the lower portion will
		 * show the new buffer while the upper portion already showed
		 * the old one -- the definition of a torn frame.
		 */
		if (drmModeSetCrtc(fd, crtc_id, bufs[back].fb_id,
				   0, 0, &conn_id, 1, mode)) {
			perror("drmModeSetCrtc");
			break;
		}

		cur = back;

		/*
		 * Sleep for 2 ms -- much shorter than a 60 Hz vblank interval
		 * (~16.67 ms) -- so buffer swaps frequently race the scanout.
		 */
		usleep(2000);
	}
}

/* ============================================================
 * page_flip_handler - Callback invoked when a queued flip completes.
 * @fd:        The DRM file descriptor (unused here).
 * @sequence:  The vblank counter value at completion time.
 * @tv_sec:    Timestamp seconds of the completed flip.
 * @tv_usec:   Timestamp microseconds of the completed flip.
 * @user_data: Pointer to the flip_pending struct owned by the main loop.
 *
 * This function executes inside drmHandleEvent(), which parses the
 * DRM_EVENT_FLIP_COMPLETE event delivered by the kernel after the
 * hardware atomically swaps the scanout pointer during vblank.
 * ============================================================ */
static void page_flip_handler(int fd, unsigned int sequence,
			      unsigned int tv_sec, unsigned int tv_usec,
			      void *user_data)
{
	struct flip_pending *pending = user_data;
	pending->waiting = false;

	/*
	 * 'sequence' increments once per vblank.  Comparing it across calls
	 * lets you detect dropped frames (sequence delta > 1 means a vblank
	 * was missed and the frame rate has fallen below the refresh rate).
	 */
	(void)fd;
	(void)sequence;
	(void)tv_sec;
	(void)tv_usec;
}

/* ============================================================
 * run_pageflip_demo - Correct double-buffering with vblank synchronization.
 *
 * drmModePageFlip() submits a flip request to the kernel's DRM core.
 * The kernel queues it and defers the actual register write until the
 * CRTC's vblank interrupt fires, at which point the display controller
 * is between frames and not reading any pixel data.  The swap is therefore
 * invisible to the viewer -- no tearing is possible.
 *
 * Kernel path (simplified):
 *   drmModePageFlip()
 *     -> drm_mode_page_flip_ioctl()
 *        -> crtc->funcs->page_flip()         (driver callback)
 *           -> vblank IRQ fires
 *              -> drm_crtc_handle_vblank()
 *                 -> plane scanout address updated in hardware
 *                    -> DRM_EVENT_FLIP_COMPLETE sent to fd
 * ============================================================ */
static void run_pageflip_demo(int fd, uint32_t crtc_id, uint32_t conn_id,
			      drmModeModeInfo *mode,
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
	 * Register our callback with the DRM event dispatch table.
	 * drmHandleEvent() reads from the DRM fd and routes each event
	 * to the appropriate handler based on the event type field.
	 */
	drmEventContext ev_ctx = {
		.version           = DRM_EVENT_CONTEXT_VERSION,
		.page_flip_handler = page_flip_handler,
	};

	/* Display the first buffer before entering the flip loop. */
	if (drmModeSetCrtc(fd, crtc_id, bufs[cur].fb_id,
			   0, 0, &conn_id, 1, mode)) {
		perror("initial drmModeSetCrtc");
		return;
	}

	printf("\n[PAGE FLIP MODE] vblank-synchronized - Ctrl+C to stop\n");
	printf("The white bar should move perfectly smoothly with no visible tear\n\n");

	while (1) {
		int back = 1 - cur;

		/* Render the next frame into the back buffer while the
		 * front buffer is safely being scanned out by hardware. */
		draw_moving_bar(&bufs[back], &anim);
		update_animation(&anim, (int)bufs[back].width);

		/*
		 * Queue the flip.  DRM_MODE_PAGE_FLIP_EVENT requests a
		 * DRM_EVENT_FLIP_COMPLETE notification so we know exactly
		 * when the swap occurred and can prepare the next frame.
		 *
		 * The kernel will reject a second flip request while one is
		 * already pending, so we must wait for the event before
		 * calling this again.
		 */
		if (drmModePageFlip(fd, crtc_id, bufs[back].fb_id,
				    DRM_MODE_PAGE_FLIP_EVENT, &pending)) {
			perror("drmModePageFlip");
			break;
		}
		pending.waiting = true;

		/*
		 * Block on select() until the DRM fd becomes readable,
		 * which happens when the kernel posts the flip-complete event.
		 * This yields the CPU during the vblank wait instead of
		 * spinning, keeping system load low.
		 */
		while (pending.waiting) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
			int ret = select(fd + 1, &fds, NULL, NULL, &timeout);

			if (ret < 0) {
				perror("select");
				return;
			}
			if (ret == 0) {
				fprintf(stderr,
					"Timeout: no vblank event within 1s\n");
				return;
			}

			/* Parse the event and invoke page_flip_handler(). */
			drmHandleEvent(fd, &ev_ctx);
		}

		cur = back;
	}
}

/* ============================================================
 * run_single_buffer_tearing - True tearing via concurrent CPU write
 * and hardware scanout on the same framebuffer.
 *
 * No buffer switch occurs.  The CPU continuously overwrites the
 * active scanout buffer while VOP2 is reading it.  Because there
 * is no address change, VOP2's shadow-register protection is
 * irrelevant: the data race is between the CPU store and the
 * display DMA read, which operates line by line top to bottom.
 *
 * Expected artifact: the bar appears split at the scanline that
 * the display engine had reached when the CPU finished writing
 * the new frame -- a classic horizontal tear line.
 * ============================================================ */
static void run_single_buffer_tearing(int fd, uint32_t crtc_id,
                                       uint32_t conn_id,
                                       drmModeModeInfo *mode,
                                       struct buffer_object *bo)
{
    struct animation_state anim = {
        .bar_x      = 0,
        .bar_width  = 80,
        .direction  = 1,
        .frame_count = 0,
    };

    /* Bind the single buffer to the CRTC once; never change it again. */
    if (drmModeSetCrtc(fd, crtc_id, bo->fb_id, 0, 0, &conn_id, 1, mode)) {
        perror("drmModeSetCrtc");
        return;
    }

    printf("\n[SINGLE BUFFER TEARING] Writing to active scanout buffer\n");
    printf("The tear line moves with the race between CPU write and DMA read\n\n");

    while (1) {
        /*
         * Draw directly into the buffer currently being scanned out.
         * VOP2 reads this memory top-to-bottom at ~60 lines/ms (1080p60).
         * The CPU write races against that DMA read with no synchronisation,
         * guaranteeing that some scanlines see the old bar position and
         * others see the new one within the same displayed frame.
         */
        draw_moving_bar(bo, &anim);
        update_animation(&anim, (int)bo->width);

        /*
         * No sleep here -- maximum write rate keeps the race condition
         * active and makes the tear line clearly visible.
         * Optionally add usleep(500) if the artifact moves too fast.
         */
    }
}

int main(int argc, char **argv)
{
	int fd;
	drmModeRes       *res;
	drmModeConnector *conn = NULL;
	drmModeEncoder   *enc  = NULL;
	uint32_t conn_id, crtc_id;
	struct buffer_object bufs[MAX_BUFFERS] = {0};
	int mode_choice = 0; /* 0 = tearing demo, 1 = page-flip demo */

	if (argc > 1 && strcmp(argv[1], "--pageflip")  == 0) mode_choice = 1;
	if (argc > 1 && strcmp(argv[1], "--singlebuf") == 0) mode_choice = 2;

	printf("DRM Tearing vs Page-Flip Experiment\n");
	printf("Usage: %s            -> tearing mode (no vblank sync)\n",
	       argv[0]);
	printf("Usage: %s --pageflip -> correct vblank-synchronized mode\n\n",
	       argv[0]);

	fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("open /dev/dri/card0");
		return -1;
	}

	res = drmModeGetResources(fd);
	if (!res) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return -1;
	}

	/* Walk the connector list and pick the first connected display. */
	for (int i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(fd, res->connectors[i]);
		if (conn && conn->connection == DRM_MODE_CONNECTED &&
		    conn->count_modes > 0)
			break;
		drmModeFreeConnector(conn);
		conn = NULL;
	}
	if (!conn) {
		fprintf(stderr, "No connected display found\n");
		return -1;
	}

	/*
	 * Resolve a CRTC compatible with this connector.  The encoder's
	 * possible_crtcs bitmask tells us which CRTCs the hardware signal
	 * path can reach from this connector.
	 */
	if (conn->encoder_id)
		enc = drmModeGetEncoder(fd, conn->encoder_id);
	if (!enc && conn->count_encoders > 0)
		enc = drmModeGetEncoder(fd, conn->encoders[0]);

	crtc_id = 0;
	for (int i = 0; i < res->count_crtcs; i++) {
		if (enc && (enc->possible_crtcs & (1 << i))) {
			crtc_id = res->crtcs[i];
			break;
		}
	}
	if (enc) drmModeFreeEncoder(enc);
	if (!crtc_id) {
		fprintf(stderr, "No usable CRTC found\n");
		return -1;
	}

	conn_id = conn->connector_id;
	drmModeModeInfo mode = conn->modes[0]; /* Use the preferred mode */

	printf("Display: %dx%d @ %u Hz\n",
	       mode.hdisplay, mode.vdisplay, mode.vrefresh);
	printf("Vblank interval: ~%.2f ms\n\n", 1000.0 / mode.vrefresh);

	/* Allocate two framebuffers for double-buffering. */
	for (int i = 0; i < MAX_BUFFERS; i++) {
		bufs[i].width  = mode.hdisplay;
		bufs[i].height = mode.vdisplay;
		if (modeset_create_fb(fd, &bufs[i]) < 0) {
			fprintf(stderr, "Failed to create framebuffer %d\n", i);
			return -1;
		}
		memset(bufs[i].vaddr, 0x20, bufs[i].size); /* Fill dark grey */
	}

	if (mode_choice == 0)
		run_tearing_demo(fd, crtc_id, conn_id, &mode, bufs);
	else if (mode_choice == 1)
		run_pageflip_demo(fd, crtc_id, conn_id, &mode, bufs);
	else
		/* Single buffer only needs bufs[0] */
    		run_single_buffer_tearing(fd, crtc_id, conn_id, &mode, &bufs[0]);

	/* Release all DRM resources in reverse allocation order. */
	for (int i = 0; i < MAX_BUFFERS; i++)
		modeset_destroy_fb(fd, &bufs[i]);

	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	close(fd);
	return 0;
}
