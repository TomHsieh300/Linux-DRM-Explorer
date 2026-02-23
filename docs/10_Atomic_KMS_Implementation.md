# Experiment 10: Transitioning to modern Atomic KMS

## 1. Objective
Migrate the entire display pipeline from Legacy APIs (`SetCrtc`, `PageFlip`) to the **Atomic KMS API**. This experiment explores the "Property-based" object model, ensuring synchronized updates across multiple planes and leveraging the "all-or-nothing" atomic guarantee for flicker-free display transitions.

---

## 2. Key Implementation Details
* **The Atomic Property Model**: Unlike legacy APIs with fixed arguments, Atomic KMS treats everything (FB_ID, CRTC_ID, coordinates) as **Properties**. This implementation uses a caching mechanism to store Property IDs at startup, avoiding expensive string lookups during the animation loop.
* **Blob Management**: Display modes (`drmModeModeInfo`) are no longer passed as structures but as **Blobs**. The kernel manages these memory blobs, and we reference them via a `MODE_ID` property.
* **16.16 Fixed-Point Math**: Atomic KMS requires source coordinates (`SRC_X`, `SRC_W`, etc.) in **16.16 fixed-point format**. This allows for sub-pixel precision during hardware scaling¡Xa detail crucial for high-end SoCs like the RK3588.
* **Atomic Test-Only**: Utilized `DRM_MODE_ATOMIC_TEST_ONLY` to validate the hardware configuration before committing. This "dry-run" capability is a major safety feature of the Atomic API, preventing invalid configurations from reaching the hardware.

---

## 3. Compilation
This project utilizes the universal Makefile. Ensure `libdrm-dev` is installed on your LubanCat 5.

```bash
# Build all experiments including atomic-demo
make

# Run property discovery mode (List all IDs)
sudo ./src/drm-atomic-demo

# Run atomic page-flip animation
sudo ./src/drm-atomic-demo --atomic
```

## 4. High-Level Logic Flow (C-Style Pseudocode)
The atomic workflow is fundamentally different: you build a request, test it, and then commit it.

``` c
// 1. Enable Capabilities (Mandatory for Atomic)
drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);

// 2. Build the Atomic Request
drmModeAtomicReq *req = drmModeAtomicAlloc();

// Atomic is all about (Object_ID, Property_ID, Value) tuples
// Set up the Primary Plane
drmModeAtomicAddProperty(req, plane_id, props.fb_id,   fb_id);
drmModeAtomicAddProperty(req, plane_id, props.crtc_id, crtc_id);
// SRC coordinates must be 16.16 fixed point
drmModeAtomicAddProperty(req, plane_id, props.src_w,   (uint64_t)width << 16); 

// Set up the CRTC
drmModeAtomicAddProperty(req, crtc_id,  props.active,  1);
drmModeAtomicAddProperty(req, crtc_id,  props.mode_id, mode_blob_id);

// 3. The "Dry Run" (Validation)
int ret = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_TEST_ONLY, NULL);

if (ret == 0) {
    // 4. Real Commit (All changes happen on the same VBlank)
    drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, &user_data);
}

drmModeAtomicFree(req);
```

## 5. Legacy vs. Atomic: Why the change?
The transition from Legacy to Atomic KMS represents a shift from "command-based" display updates to "state-based" transactions. In legacy KMS, changing multiple properties (like moving a plane and switching a framebuffer) required multiple IOCTLs, which often resulted in visible glitches because the hardware might catch a frame "in-between" those commands.

Atomic KMS solves this by bundling all desired changes into a single "request" that the kernel applies in one hardware transaction, guaranteed to take effect during the same VBlank interval.



| Feature | Legacy KMS | Atomic KMS |
| :--- | :--- | :--- |
| **Atomicity** | Multiple IOCTLs (Potential glitches) | Single IOCTL (Synchronized) |
| **Validation** | Fail at runtime | `TEST_ONLY` (Pre-validated) |
| **Flexibility** | Fixed Arguments | Property-based (Extensible) |
| **Multi-plane** | Hard to synchronize | Native simultaneous updates |
| **Hardware State** | Incremental / Fragile | Transactional / Robust |
