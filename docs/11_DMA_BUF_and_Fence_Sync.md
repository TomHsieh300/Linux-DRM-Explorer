

bjective
Explore the mechanisms of **Zero-copy memory sharing** and **Hardware Synchronization**. This experiment demonstrates how different hardware modules share the same physical memory via **DMA-BUF (PRIME)** and how **Fences** ensure data integrity between a producer (e.g., GPU/ISP) and a consumer (Display Engine).

---

## 2. Key Implementation Details

### The "Dual-FD" Simulation Strategy
* **Driver Context**: On the current platform (RK3588), the proprietary Mali driver does not fully integrate with the standard DRM-PRIME cross-device pipeline. 
* **GEM Namespace**: Since GEM handles are local to a file descriptor, this experiment opens **two independent FDs** to `/dev/dri/card0` to simulate two distinct hardware devices. 
* **The Bridge**: By exporting a GEM handle from `fd_producer` to a `dmabuf_fd` and importing it into `display_fd`, we effectively demonstrate the **PRIME** mechanism used in production-grade multimedia pipelines (e.g., Camera to Display).



### Synchronization Mechanisms
* **Implicit Fence (SYNC IOCTL)**: Used for CPU-side writes. `DMA_BUF_IOCTL_SYNC` (START/END) performs necessary cache maintenance (Clean/Invalidate) to ensure the VOP2 sees the data written by the CPU.
* **Explicit Fence (Sync Files)**: 
    * **`IN_FENCE_FD`**: A plane property that tells the kernel "wait for this fence before scanout."
    * **`OUT_FENCE_PTR`**: A CRTC property where the kernel writes an FD signaling "display finished."

---

## 3. High-Level Logic Flow (C-Style Pseudocode)

### The DMA-BUF Lifecycle (PRIME)
```c
// 1. Export from Producer (Simulating GPU/ISP)
int dmabuf_fd;
drmPrimeHandleToFD(fd_producer, producer_handle, DRM_CLOEXEC | DRM_RDWR, &dmabuf_fd);

// 2. Import to Display (VOP2)
uint32_t display_handle;
drmPrimeFDToHandle(display_fd, dmabuf_fd, &display_handle);

// 3. Register for Scanout
drmModeAddFB(display_fd, width, height, ..., display_handle, &fb_id);
```

### Explicit Fence Pipeline
```c
// Frame N: Request an out-fence
drmModeAtomicAddProperty(req, crtc_id, out_fence_ptr, &new_out_fence_fd);
drmModeAtomicCommit(display_fd, req, ...);

// Frame N+1: Use that fence as an in-fence
// Ensures VOP2 doesn't read the new buffer until Frame N is off-screen
drmModeAtomicAddProperty(req, plane_id, in_fence_fd, new_out_fence_fd);
drmModeAtomicCommit(display_fd, req, ...);
```

## 4. Comparison Table

| Method | Control | Use Case | Hardware Behavior (RK3588) |
| :--- | :--- | :--- | :--- |
| **Implicit** | Kernel | Legacy / CPU Access | CPU Cache Flush/Invalidate |
| **Explicit** | Userspace | Wayland / Vulkan | Atomic wait for Sync File signal |
| **No Sync** | None | Testing | Race condition / Potential corruption |

---

## 5. Execution
```bash
# Build the project
make

# Mode 1: DMA-BUF sharing with implicit sync (Default)
sudo ./src/drm-dmabuf-fence

# Mode 2: Observe race conditions without sync
sudo ./src/drm-dmabuf-fence --nosync

# Mode 3: Modern explicit fence synchronization
sudo ./src/drm-dmabuf-fence --fence
```
