# Experiment 07: Implementing Userspace KMS using C and libdrm

## 1. Objective
Transition from command-line tools to custom C code to gain full control over the display pipeline. This experiment implements the "Legacy KMS" path to set up a basic framebuffer and scan it out to a DSI panel.

## 2. Key Implementation Details
* **Buffer Management**: Utilized `DRM_IOCTL_MODE_CREATE_DUMB` to allocate raw memory and `drmModeAddFB` to register it as a scanout-capable Framebuffer.
* **Hardware-Aware CRTC Selection**: Implemented a search algorithm using the `possible_crtcs` bitmask from the encoder.
    * *Lesson Learned*: Hardcoding `res->crtcs[0]` (VP0) fails on RK3588 because the DSI panel is physically wired to VP3 (CRTC 208).
* **Pointer Arithmetic**: Used the `pitch` (stride) value provided by the kernel instead of `width` to ensure proper memory alignment during pixel writing.

## 3. Compilation
To compile this program on the LubanCat 5, ensure `libdrm-dev` is installed and use `pkg-config` to handle header paths:
```bash
gcc -o src/modeset-single-buffer src/modeset-single-buffer.c $(pkg-config --cflags --libs libdrm)

## 4. High-Level Logic Flow (C-Style Pseudocode)

The following logic outlines the sequence of operations performed in `src/modeset-single-buffer.c`. This serves as a blueprint for the Legacy KMS display pipeline:

```c
// 1. Initialization
int fd = open("/dev/dri/card0", O_RDWR);
drmModeRes *res = drmModeGetResources(fd);

// 2. Resource Discovery & Pipeline Matching
for (int i = 0; i < res->count_connectors; i++) {
    conn = drmModeGetConnector(fd, res->connectors[i]);
    if (conn->connection == DRM_MODE_CONNECTED) {
        // Find a CRTC compatible with the connector's encoder
        encoder = drmModeGetEncoder(fd, conn->encoder_id);
        crtc_id = find_compatible_crtc(encoder, res);
        break;
    }
}

// 3. Buffer Management (Dumb Buffer)
struct drm_mode_create_dumb creq = { .width = w, .height = h, .bpp = 32 };
drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
drmModeAddFB(fd, w, h, 24, 32, creq.pitch, creq.handle, &fb_id);

// 4. Memory Mapping & Rendering
struct drm_mode_map_dumb mreq = { .handle = creq.handle };
drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
uint32_t *map = mmap(..., mreq.offset);

memset(map, 0xFF, size); // Fill with test pattern

// 5. Scanout Commit (The Legacy KMS call)
drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &conn_id, 1, &mode);

// 6. Cleanup on exit
getchar(); // Wait for user
modeset_destroy_fb(fd, &buf);
close(fd);
```
