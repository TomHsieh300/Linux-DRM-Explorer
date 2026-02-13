# Experiment 06: Synchronization via VBlank and Page Flip

## 1. Objective
To understand how the DRM subsystem synchronizes framebuffer updates with the display's vertical refresh cycle to prevent screen tearing.

## 2. Methodology: Using `modetest -v`
By executing `modetest` with the `-v` flag, the tool initiates a continuous **Page Flip** loop. This forces the CRTC to switch between two framebuffers during each Vertical Blanking Interval (VBlank).

## 3. Kernel Level Observation
While the page flip is active, I monitored the hardware interrupts via `/proc/interrupts`:
* **Command**: `watch -n 1 "cat /proc/interrupts | grep vop"`
* **Actual Output from LubanCat 5**:
    ```text
    61:       6044          0          0          0   GICv3 188 Level     fdd97e00.iommu, fdd90000.vop
    ```
* **Result**: The interrupt count for the VOP increases by exactly 60 per second, aligning perfectly with the 60Hz refresh rate of the DSI panel.

## 4. Engineering Insight
This experiment confirms the integrity of the **Interrupt Handling Path**. In a professional Bring-up scenario, if the screen is on but the IRQ count isn't increasing, it usually indicates a "frozen" hardware state or a misconfigured Clock/Power domain.
