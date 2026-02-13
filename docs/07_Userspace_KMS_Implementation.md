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
