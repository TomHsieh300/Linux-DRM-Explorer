# Experiment 05: Hardware Plane Composition & Sync (GEM/Fence)

## 1. Hardware Compositor
RK3588 VOP2 features multiple Plane types (Cluster, Esmart, Smart).
* **Advantage**: By utilizing hardware planes for overlays, the system can skip GPU-based software composition, significantly reducing power consumption.

## 2. Memory & Sync (GEM/Fence)
* **GEM (Graphics Execution Manager)**: Handles memory allocation for framebuffers.
* **dma-fence**: Solves the synchronization between the producer (GPU) and consumer (VOP). It ensures the VOP scans the buffer only after the GPU has finished rendering, preventing screen tearing.

## 3. Summary
Successful hardware bring-up requires not just the physical link, but a synchronized dance between memory management, GPU rendering, and display scanout.
