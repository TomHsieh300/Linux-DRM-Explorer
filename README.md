# Linux-DRM-Explorer

A systematic and deep-dive exploration of the **Linux DRM/KMS subsystem** on the **Rockchip RK3588 (VOP2)** platform. This project documents a professional display bring-up journey, transitioning from low-level register verification to modern, high-performance atomic synchronization.

## Project Highlights
* **Architectural Mastery**: Bridging the mental model between **Linux ASoC** (Audio) and **DRM** (Display), treating pixels as a specialized DMA stream.
* **Modern API Transition**: Complete migration from **Legacy KMS** (SetCrtc) to **Atomic KMS** (Property-based commits).
* **Zero-Copy Pipelines**: Implementation of **DMA-BUF (PRIME)** for efficient cross-device memory sharing without CPU intervention.
* **Performance Optimization**: Inner-loop **branchless rendering** and **Fence-based hardware synchronization** to eliminate UI jitter and tearing.

## The 11-Stage Learning Roadmap

I have structured the bring-up process into 11 progressive experiments:

### Phase 1: Hardware & Subsystem Basics
1. [**KMS Pipeline Mapping**](./docs/01_Hardware_Inventory.md): Analyzing internal VOP2 resources (VP0-VP3) and Plane constraints.
2. [**Modetest Mastery**](./docs/02_Modetest_Mastery.md): Hands-on with atomic modesetting and troubleshooting object IDs.
3. [**DSI Panel Bring-up**](./docs/03_DSI_Panel_Bringup.md): Calculating Video Timings and verifying PCLK/DPHY registers.
4. [**DRM Master Concepts**](./docs/04_DRM_Master_and_Race_Condition.md): Navigating ownership conflicts between `fbcon` and userspace clients.

### Phase 2: Building the Software Pipeline
5. [**Composition & Sync**](./docs/05_Composition_and_Sync.md): Understanding hardware layering (Z-order) and GEM/Fence roles.
6. [**VBlank & Page Flip**](./docs/06_VBlank_and_PageFlip.md): Real-time monitoring of VOP interrupts to verify the display "heartbeat."
7. [**Userspace KMS C-API**](./docs/07_Userspace_KMS_Implementation.md): Implementing the first framebuffer renderer using `libdrm`.
8. [**Double Buffering & Branchless**](./docs/08_Double_Buffering_and_Optimization.md): Scaling to multi-buffer architectures and CPU pipeline optimization.

### Phase 3: Advanced Atomic & Synchronization
9. [**VBlank Sync vs. Tearing**](./docs/09_VBlank_and_Tearing_Analysis.md): Analyzing the scanout race condition and the physics of screen tearing.
10. [**Atomic KMS Mastery**](./docs/10_Atomic_KMS_Implementation.md): Fully migrating to the **Atomic property model** and `TEST_ONLY` validation.
11. [**DMA-BUF & Fence Sync**](./docs/11_DMA_BUF_and_Fence_Sync.md): Simulating cross-device pipelines with **PRIME** and explicit fences (`IN_FENCE_FD`).

## Tools & Environment
* **Target Hardware**: LubanCat 5 (Rockchip RK3588, VOP2)
* **Software Stack**: Ubuntu Lite (Minimal CLI), `libdrm`, `linux-libc-dev`.
* **Analysis Tools**: `modetest`, `debugfs` (KMS status), `GICv3` interrupt analysis.

---

## Technical Insights for IC Design & BSP Teams
* **Unified Memory Coherency**: Deep understanding of `DMA_BUF_IOCTL_SYNC` for cache maintenance on ARM SoCs.
* **Proprietary Driver Strategies**: Successfully simulated cross-namespace GEM sharing on platforms with proprietary Mali stacks.
* **Fixed-Point Precision**: Handling 16.16 fixed-point source coordinates required by modern display hardware.

---
*Signed-off-by: TomHsieh300 <hungen3108@gmail.com>*
