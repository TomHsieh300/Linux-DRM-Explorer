#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define MAX_BUFFERS 2
#define PATTERN_RGB 0
#define PATTERN_GBR 1

struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint8_t *vaddr;
	uint32_t fb_id;
};

/**
 * draw_test_pattern - Renders a vertical bar pattern to the buffer.
 * @bo: The buffer object to draw into.
 * @pattern_type: The color sequence to use (PATTERN_RGB or PATTERN_GBR).
 * * This function calculates the color for each pixel without using nested 
 * conditional branches inside the inner loop, optimizing for CPU pipeline 
 * efficiency. It divides the screen into three vertical segments.
 */
static void draw_test_pattern(struct buffer_object *bo, int pattern_type)
{
	uint32_t *pixel = (uint32_t *)bo->vaddr;
	
	/* Pre-define the color sequence to avoid branching inside loops */
	uint32_t colors[2][3] = {
		{0xff0000, 0x00ff00, 0x0000ff}, // PATTERN_RGB: Red, Green, Blue
		{0x00ff00, 0x0000ff, 0xff0000}  // PATTERN_GBR: Green, Blue, Red
	};

	for (uint32_t y = 0; y < bo->height; y++) {
		for (uint32_t x = 0; x < bo->width; x++) {
			/* Select color segment (0, 1, or 2) */
			int segment = x * 3 / bo->width;
			if (segment > 2) segment = 2; // Boundary safety

			uint32_t offset = y * (bo->pitch / 4) + x;
			pixel[offset] = colors[pattern_type][segment];
		}
	}
}

static int modeset_create_fb(int fd, struct buffer_object *bo, int pattern_type)
{
	struct drm_mode_create_dumb create = { .width = bo->width, .height = bo->height, .bpp = 32 };
	struct drm_mode_map_dumb map = {0};

	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) return -1;

	bo->pitch = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;

	if (drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch, bo->handle, &bo->fb_id)) return -1;

	map.handle = bo->handle;
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) return -1;

	bo->vaddr = mmap(0, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);
	if (bo->vaddr == MAP_FAILED) return -1;

	draw_test_pattern(bo, pattern_type);
	return 0;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = { .handle = bo->handle };
	drmModeRmFB(fd, bo->fb_id);
	munmap(bo->vaddr, bo->size);
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

int main(int argc, char **argv)
{
	int fd;
	drmModeRes *res;
	drmModeConnector *conn = NULL;
	uint32_t conn_id, crtc_id;
	struct buffer_object bufs[MAX_BUFFERS] = {0};

	fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("Failed to open /dev/dri/card0");
		return -1;
	}

	res = drmModeGetResources(fd);
	if (!res) return -1;

	/* Resource discovery */
	for (int i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(fd, res->connectors[i]);
		if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
			break;
		}
		drmModeFreeConnector(conn);
		conn = NULL;
	}

	if (!conn) return -1;

	/* Find compatible CRTC using possible_crtcs bitmask */
	drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
	if (!enc) enc = drmModeGetEncoder(fd, conn->encoders[0]);
	
	crtc_id = 0;
	for (int i = 0; i < res->count_crtcs; i++) {
		if (enc->possible_crtcs & (1 << i)) {
			crtc_id = res->crtcs[i];
			break;
		}
	}
	drmModeFreeEncoder(enc);

	conn_id = conn->connector_id;

	/* Initialize all buffers in a loop */
	for (int i = 0; i < MAX_BUFFERS; i++) {
		bufs[i].width = conn->modes[0].hdisplay;
		bufs[i].height = conn->modes[0].vdisplay;
		
		/* Use i as pattern index (0 for RGB, 1 for GBR) */
		if (modeset_create_fb(fd, &bufs[i], i % 2) < 0) {
			fprintf(stderr, "Failed to create buffer %d\n", i);
			return -1;
		}
	}

	/* Double buffering demo: switch between buffers */
	for (int i = 0; i < MAX_BUFFERS; i++) {
		printf("Displaying Buffer [%d] with pattern %s. Press Enter to switch...\n", 
		        i, (i == PATTERN_RGB) ? "RGB" : "GBR");
		
		if (drmModeSetCrtc(fd, crtc_id, bufs[i].fb_id, 0, 0, &conn_id, 1, &conn->modes[0])) {
			perror("drmModeSetCrtc failed");
		}
		getchar();
	}

	/* Cleanup */
	for (int i = 0; i < MAX_BUFFERS; i++) {
		modeset_destroy_fb(fd, &bufs[i]);
	}

	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	close(fd);

	return 0;
}
