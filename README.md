# Linux-DRM-Explorer

A systematic exploration of the **Linux DRM/KMS subsystem** on the **Rockchip RK3588 (VOP2)** platform. This project documents the complete display bring-up process, from low-level register verification to high-level atomic synchronization.

## Key Project Highlights
* **Cross-Subsystem Mental Model**: Leveraging experience from **Linux ASoC** to master the DRM pipeline (Mapping DMA/Mixers to CRTC/Planes).
* **Hardware-Level Debugging**: Direct verification of Video Timing and Pixel Clocks via `debugfs` and hardware registers.
* **Sync & Performance**: Deep dive into `dma-fence` mechanisms and **VBlank IRQ** monitoring for smooth page-flipping.

## Technical Experiments (Step-by-Step)
I have documented the bring-up process in 6 structured experiments:

1. [**KMS Pipeline Mapping**](./docs/01_Hardware_Inventory.md): Analyzing internal resources (VP0-VP3) and Plane constraints.
2. [**Modetest mastery**](./docs/02_Modetest_Mastery.md): Hands-on with atomic modesetting and troubleshooting object IDs.
3. [**DSI Panel Bring-up**](./docs/03_DSI_Panel_Bringup.md): Calculating Video Timings and verifying PCLK registers.
4. [**DRM Master Concepts**](./docs/04_DRM_Master_and_Race_Condition.md): Understanding ownership conflicts between `fbcon` and userspace clients.
5. [**Composition & Sync**](./docs/05_Composition_and_Sync.md): Hardware layering (Z-order) and the role of GEM/Fence.
6. [**VBlank & Page Flip**](./docs/06_VBlank_and_PageFlip.md): Real-time monitoring of VOP interrupts to verify display "heartbeat."
7. [**User Space Example**](./docs/07_Userspace_KMS_Implementation.md): Implementing Userspace KMS using C and libdrm

## Tools & Environment
* **Hardware**: LubanCat 5 (Rockchip RK3588)
* **OS**: Ubuntu Lite (Minimal environment for cleaner DRM debugging)
* **Kernel Tools**: `modetest`, `debugfs`, `procfs`, `GICv3` interrupt controller analysis.

---
*Signed-off-by: TomHsieh300 <hungen3108@gmail.com>*
