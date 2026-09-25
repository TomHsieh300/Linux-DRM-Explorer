# Linux-DRM-Explorer

A systematic and deep-dive exploration of the **Linux DRM/KMS subsystem** on the **Rockchip RK3588 (VOP2)** platform. This project documents a professional display bring-up journey, transitioning from low-level register verification to modern, high-performance atomic synchronization.

## Project Highlights
* **Architectural Mastery**: Bridging the mental model between **Linux ASoC** (Audio) and **DRM** (Display), treating pixels as a specialized DMA stream.
* **Modern API Transition**: Complete migration from **Legacy KMS** (SetCrtc) to **Atomic KMS** (Property-based commits).
* **Zero-Copy Pipelines**: Implementation of **DMA-BUF (PRIME)** for efficient cross-device memory sharing without CPU intervention.
* **Performance Optimization**: Inner-loop **branchless rendering** and **Fence-based hardware synchronization** to eliminate UI jitter and tearing.

## The Learning Roadmap

I have structured the bring-up process into progressive experiments:

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

### Phase 4: Composition, Topology & Observability
> Experiments 12–17 are implemented and compile cleanly, but have **not yet been run on the board**; their Results sections are placeholders. The [Hardware Validation Checklist](./docs/HARDWARE_VALIDATION.md) lists what to run and observe, and `tools/collect-results.sh` gathers the read-only output in one go.

12. [**Plane Properties & Blending**](./docs/12_Plane_Properties_and_Blending.md): Driving `zpos`, `alpha`, `pixel blend mode`, rotation and scaling through atomic commits.
13. [**Pixel Formats & Modifiers**](./docs/13_Pixel_Formats_and_Modifiers.md): Decoding the `IN_FORMATS` blob (incl. AFBC) and scanning out an NV12 buffer.
14. [**Debugging & Tracing**](./docs/14_Debugging_and_Tracing.md): `drm.debug` categories, debugfs, DRM/dma-fence tracepoints and a capture script.
15. [**Device Tree & Driver Walkthrough**](./docs/15_Device_Tree_and_Driver_Walkthrough.md): Following the DSI panel from the OF graph through the VOP2/DSI driver bind flow.
16. [**Multi-Display & Hotplug**](./docs/16_Multi_Display_and_Hotplug.md): Routing matrix (`possible_crtcs`), CRTC assignment and netlink hotplug monitoring.
17. [**Frame Timing Measurement**](./docs/17_Frame_Timing_Measurement.md): Quantifying flip intervals, latency and missed VBlanks.

### Phase 5: GPU → Display Pipeline
> In progress. Experiment 18 identifies the GPU driver stack on the board; experiments 19 (GPU render → dma-buf → KMS, zero-copy) and 20 (tracing the path from userspace to kernel) will be written from its results.

18. [**GPU Stack Discovery**](./docs/18_GPU_Stack_Discovery.md): BSP kbase + libmali vs. mainline panthor + Mesa, and the EGL extensions that decide how a zero-copy GPU → KMS path can be built.

## Tools & Environment
* **Target Hardware**: LubanCat 5 (Rockchip RK3588, VOP2)
* **Software Stack**: Ubuntu Lite (Minimal CLI), `libdrm`, `linux-libc-dev`.
* **Analysis Tools**: `modetest`, `debugfs` (KMS status), `GICv3` interrupt analysis, `drm.debug` / ftrace ([Experiment 14](./docs/14_Debugging_and_Tracing.md)).

### Test Environment
Register addresses, IRQ numbers and object IDs quoted in the experiments depend on the exact kernel and board configuration. Record the versions used so that results can be reproduced:

| Item | Value | How to obtain |
| :--- | :--- | :--- |
| Board | LubanCat 5 (RK3588) | — |
| OS image | `TODO` | `cat /etc/os-release` |
| Kernel version | `TODO` | `uname -a` |
| Kernel source (BSP tree / branch / commit) | `TODO` | from the image vendor |
| DRM driver | `TODO` | `cat /sys/kernel/debug/dri/0/name` |
| libdrm | `TODO` | `pkg-config --modversion libdrm` |
| modetest | `TODO` | `dpkg -l libdrm-tests` |
| Display | 1024x600 MIPI-DSI panel on VP3 | [Experiment 03](./docs/03_DSI_Panel_Bringup.md) |

> Kernel-side explanations in the docs cite upstream `torvalds/linux` source files and tags (mainly `v6.1` and `master`; RK3588 VOP2 support is upstream since `v6.8`). The Rockchip BSP kernel carries vendor patches, so behaviour may differ—each doc says where this matters.

### Building
```bash
# Native build on the board
sudo apt install build-essential pkg-config libdrm-dev
make

# Cross build on an x86_64 Ubuntu 24.04 host (multiarch)
sudo dpkg --add-architecture arm64      # arm64 packages come from ports.ubuntu.com
sudo apt install gcc-aarch64-linux-gnu libdrm-dev:arm64
make CROSS_COMPILE=aarch64-linux-gnu- PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig
```
Every `src/*.c` file becomes one binary next to its source. Helper scripts live in [`tools/`](./tools/) (e.g. `tools/drm-debug-capture.sh` from Experiment 14). CI ([`.github/workflows/build.yml`](./.github/workflows/build.yml)) builds all experiments natively and for aarch64 with `-Werror`.

### Documentation Conventions
Each experiment follows the same outline: **Objective → Environment → Background → Steps / Implementation → Results → Analysis → Key Takeaways → References**.
* Results that have not been captured on the board yet are marked `TODO(on-hardware)`—nothing in the docs is presented as measured unless it was.
* Kernel and libdrm statements cite the file and function they were checked against.

---

## Technical Insights for IC Design & BSP Teams
* **Unified Memory Coherency**: Deep understanding of `DMA_BUF_IOCTL_SYNC` for cache maintenance on ARM SoCs.
* **Proprietary Driver Strategies**: Successfully simulated cross-namespace GEM sharing on platforms with proprietary Mali stacks.
* **Fixed-Point Precision**: Handling 16.16 fixed-point source coordinates required by modern display hardware.

---
*Signed-off-by: TomHsieh300 <hungen3108@gmail.com>*
