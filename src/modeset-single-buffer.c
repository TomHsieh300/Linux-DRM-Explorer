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

/**
 * struct buffer_object - Scaffolding for DRM dumb buffer management
 * @width:  Width in pixels
 * @height: Height in pixels
 * @pitch:  Number of bytes between two consecutive scanlines
 * @handle: GEM handle returned by the kernel
 * @size:   Total buffer size in bytes
 * @vaddr:  Userspace virtual address (mmap'ed)
 * @fb_id:  Framebuffer ID registered with the DRM subsystem
 */
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
 * modeset_create_fb - Allocate and prepare a dumb buffer for scanout
 * * This function performs the standard 4-step buffer initialization:
 * 1. Create a dumb buffer (raw memory in VRAM/DDR)
 * 2. Register the buffer as a Framebuffer (FB) for the display engine
 * 3. Map the buffer for CPU access
 * 4. Fill the buffer with a test pattern (RGB Bars)
 */
static int modeset_create_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_create_dumb create = { .width = bo->width, .height = bo->height, .bpp = 32 };
	struct drm_mode_map_dumb map = {0};

	/* 1. Allocate memory on the device for the dumb buffer */
	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) return -1;

	bo->pitch = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;

	/* 2. Create a Framebuffer (FB) object that references our dumb buffer */
	if (drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch, bo->handle, &bo->fb_id)) return -1;

	/* 3. Prepare the buffer for memory mapping */
	map.handle = bo->handle;
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) return -1;

	/* 4. Map the buffer into the process's virtual address space */
	bo->vaddr = mmap(0, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);
	if (bo->vaddr == MAP_FAILED) return -1;

	/* 5. Draw RGB vertical bars. Note: pitch is used for proper memory alignment */
	uint32_t *pixel = (uint32_t *)bo->vaddr;
	for (uint32_t y = 0; y < bo->height; y++) {
		for (uint32_t x = 0; x < bo->width; x++) {
			uint32_t offset = y * (bo->pitch / 4) + x;
			if (x < bo->width / 3)
				pixel[offset] = 0xff0000; // Red
			else if (x < (bo->width * 2) / 3)
				pixel[offset] = 0x00ff00; // Green
			else
				pixel[offset] = 0x0000ff; // Blue
		}
	}
	return 0;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = { .handle = bo->handle };

	/* Clean up FB and memory mapping */
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
	struct buffer_object buf = {0};

	/* Open the primary DRM device */
	fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("Failed to open /dev/dri/card0");
		return -1;
	}

	/* Retrieve display resources */
	res = drmModeGetResources(fd);
	if (!res) return -1;

	/* Enumerate connectors to find an active display output */
	for (int i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(fd, res->connectors[i]);
		if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
			printf("Connected display found: ID %d\n", conn->connector_id);
			break;
		}
		drmModeFreeConnector(conn);
		conn = NULL;
	}

	if (!conn) {
		fprintf(stderr, "No connected display found!\n");
		return -1;
	}

	/* Basic pipeline setup: Select the first available CRTC and primary mode */
	/* Find the encoder used by this connector */
	drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
	if (!enc) {
    	/* If no encoder is currently attached, try the first one supported by the connector */
    		enc = drmModeGetEncoder(fd, conn->encoders[0]);
	}

	/* Find a compatible CRTC using the possible_crtcs bitmask */
	crtc_id = 0;
	for (int i = 0; i < res->count_crtcs; i++) {
    		if (enc->possible_crtcs & (1 << i)) {
        		crtc_id = res->crtcs[i];
        		printf("Selected compatible CRTC ID: %d (index %d)\n", crtc_id, i);
        		break;
    		}
	}

	drmModeFreeEncoder(enc);

	if (crtc_id == 0) {
    		fprintf(stderr, "Could not find a compatible CRTC for this connector\n");
    		return -1;
	}	
	conn_id = conn->connector_id;
	buf.width = conn->modes[0].hdisplay;
	buf.height = conn->modes[0].vdisplay;

	printf("Targeting Resolution: %dx%d @ %dHz\n", buf.width, buf.height, conn->modes[0].vrefresh);

	/* Prepare the framebuffer */
	if (modeset_create_fb(fd, &buf) < 0) {
		fprintf(stderr, "Failed to create framebuffer\n");
		return -1;
	}

	/* Perform Legacy Modesetting (Atomic set internally by modern drivers) */
	if (drmModeSetCrtc(fd, crtc_id, buf.fb_id, 0, 0, &conn_id, 1, &conn->modes[0])) {
		perror("drmModeSetCrtc failed");
	}

	printf("RGB Bars should be visible on screen. Press Enter to clean up and exit.\n");
	getchar();

	/* Cleanup resources */
	modeset_destroy_fb(fd, &buf);
	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	close(fd);

	return 0;
}
