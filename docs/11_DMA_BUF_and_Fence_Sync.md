# Experiment 11: DMA-BUF (PRIME) & Fence Synchronization

## 1. Objective
Explore the mechanisms of **Zero-copy memory sharing** and **Hardware Synchronization**. This experiment demonstrates how different hardware modules share the same physical memory via **DMA-BUF (PRIME)** and how **Fences** ensure data integrity between a producer (e.g., GPU/ISP) and a consumer (Display Engine).

---

## 2. Environment
See the [Test Environment](../README.md#test-environment) section.

---

## 3. Key Implementation Details

### The "Dual-FD" Simulation Strategy
* **Driver Context**: On the current platform (RK3588), the proprietary Mali driver does not fully integrate with the standard DRM-PRIME cross-device pipeline. 
* **GEM Namespace**: Since GEM handles are local to a file descriptor, this experiment opens **two independent FDs** to `/dev/dri/card0` to simulate two distinct hardware devices. 
* **The Bridge**: By exporting a GEM handle from `fd_producer` to a `dmabuf_fd` and importing it into `display_fd`, we effectively demonstrate the **PRIME** mechanism used in production-grade multimedia pipelines (e.g., Camera to Display).



### Synchronization Mechanisms
* **Implicit Fence (SYNC IOCTL)**: Used for CPU-side writes. `DMA_BUF_IOCTL_SYNC` (START/END) performs necessary cache maintenance (Clean/Invalidate) to ensure the VOP2 sees the data written by the CPU.

> **What `DMA_BUF_IOCTL_SYNC` actually does (verified against kernel `v6.1` and master)**
> * **Waits for implicit fences**: `DMA_BUF_SYNC_START` ends up in `__dma_buf_begin_cpu_access()` (`drivers/dma-buf/dma-buf.c`), which first waits on the fences stored in the buffer's reservation object (`dma_resv_wait_timeout()`, comment: *"Wait on any implicit rendering fences"*).
> * **Cache maintenance depends on the exporter**: after the wait, the exporter's `begin_cpu_access` / `end_cpu_access` callbacks run *if it provides them*. The generic DRM PRIME ops (`drm_gem_prime_dmabuf_ops` in `drivers/gpu/drm/drm_prime.c`) do not define these callbacks, and in mainline the Rockchip GEM code (`drivers/gpu/drm/rockchip/rockchip_drm_gem.c`) maps buffers to userspace as **write-combined** (`pgprot_writecombine`), i.e. not through the CPU cache. So on a mainline kernel there is no Clean/Invalidate to perform for these buffers; the Rockchip BSP kernel may implement this differently—check its `rockchip_drm_gem.c`.
> * **It is not a lock**: the UAPI documentation of `struct dma_buf_sync` (`include/uapi/linux/dma-buf.h`) states that it *"only provides cache coherency"* and *"does not prevent other processes or devices from accessing the memory at the same time"*.
* **Explicit Fence (Sync Files)**: 
    * **`IN_FENCE_FD`**: A plane property that tells the kernel "wait for this fence before scanout."
    * **`OUT_FENCE_PTR`**: A CRTC property where the kernel writes an FD signaling "display finished."

---

## 4. High-Level Logic Flow (C-Style Pseudocode)

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

## 5. Comparison Table

| Method | Control | Use Case | Hardware Behavior (RK3588) |
| :--- | :--- | :--- | :--- |
| **Implicit** | Kernel | Legacy / CPU Access | Wait on the buffer's reservation-object fences; cache maintenance only if the exporter implements it (see note in section 3) |
| **Explicit** | Userspace | Wayland / Vulkan | Atomic wait for Sync File signal |
| **No Sync** | None | Testing | Race condition / Potential corruption |

---

## 6. Execution
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

## 7. Results
> `TODO(on-hardware)`: record the exact command lines, program output and observations from the LubanCat 5 for this experiment (kernel version as listed in the README's Test Environment).

## 8. References
* libdrm 2.4.125 `xf86drm.c`: `drmPrimeHandleToFD`, `drmPrimeFDToHandle`
* Kernel UAPI: `include/uapi/linux/dma-buf.h` (`struct dma_buf_sync`, `DMA_BUF_IOCTL_SYNC`)
* Kernel source: `drivers/dma-buf/dma-buf.c` (`__dma_buf_begin_cpu_access`), `drivers/gpu/drm/drm_prime.c` (`drm_gem_prime_dmabuf_ops`), `drivers/gpu/drm/rockchip/rockchip_drm_gem.c`
* Kernel source: `drivers/gpu/drm/drm_atomic_uapi.c` (documentation of `IN_FENCE_FD` / `OUT_FENCE_PTR`)
* Kernel documentation: [`Documentation/driver-api/dma-buf.rst`](https://github.com/torvalds/linux/blob/master/Documentation/driver-api/dma-buf.rst), [`Documentation/driver-api/sync_file.rst`](https://github.com/torvalds/linux/blob/master/Documentation/driver-api/sync_file.rst)
