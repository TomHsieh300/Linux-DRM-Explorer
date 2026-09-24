# Experiment 05: Hardware Plane Composition & Sync (GEM/Fence)

## 1. Hardware Compositor
RK3588 VOP2 features multiple Plane types (Cluster, Esmart, Smart).
* **Advantage**: By utilizing hardware planes for overlays, the system can skip GPU-based software composition, significantly reducing power consumption.

## 2. Memory & Sync (GEM/Fence)
* **GEM (Graphics Execution Manager)**: Handles memory allocation for framebuffers.
* **dma-fence**: Solves the synchronization between the producer (GPU) and consumer (VOP). It ensures the VOP scans the buffer only after the GPU has finished rendering, preventing a **partially rendered frame** from reaching the screen.

> **Fences vs. tearing — two different problems**
> * **Incomplete frame** (fence problem): the display starts scanning a buffer the GPU is still writing to. A dma-fence makes the commit wait until rendering is complete.
> * **Tearing** (timing problem): the buffer address is switched *while* the display is in the middle of scanning out a frame, so the top and bottom of the screen come from different frames. This is solved by latching the new buffer only during **VBlank** (page flip / atomic commit), not by fences. See [Experiment 09](./09_VBlank_and_Tearing_Analysis.md).
>
> A correct pipeline needs both: the fence says *"the content is ready"*, VBlank says *"now is a safe moment to switch"*.

## 3. Summary
Successful hardware bring-up requires not just the physical link, but a synchronized dance between memory management, GPU rendering, and display scanout.
