# Experiment 06: Synchronization via VBlank and Page Flip

## 1. Objective
To understand how the DRM subsystem synchronizes framebuffer updates with the display's vertical refresh cycle to prevent screen tearing.

## 2. Environment
See the [Test Environment](../README.md#test-environment) section.

## 3. Background
* **VBlank** is the vertical blanking interval between the last active line of one frame and the first active line of the next (36 lines on this panel, see [Experiment 03](./03_DSI_Panel_Bringup.md)). The display controller raises an interrupt around this point.
* A **page flip** asks the kernel to switch the scanout buffer at the next VBlank and to notify userspace with an event once it happened.
* **VBlank interrupts are reference counted.** The DRM core only keeps the VBlank IRQ enabled while someone needs it (a pending flip, a `drmWaitVBlank()` call, …). When the last user goes away, it disables the interrupt after a delay set by the `drm.vblankoffdelay` module parameter, default **5000 ms** (`drm_vblank_offdelay` in `drivers/gpu/drm/drm_vblank.c`, same default in `v6.1` and master). A driver can opt into disabling immediately via `vblank_disable_immediate`; the mainline Rockchip DRM driver files (`rockchip_drm_drv.c`, `rockchip_drm_vop.c`, `rockchip_drm_vop2.c`) do not set it (checked on master).

## 4. Methodology: Using `modetest -v`
By executing `modetest` with the `-v` flag ("test vsynced page flipping" in `modetest -h`), the tool initiates a continuous **Page Flip** loop. This forces the CRTC to switch between two framebuffers during each Vertical Blanking Interval (VBlank).

```bash
# Terminal 1: continuous page flipping on the DSI panel (VP3)
sudo modetest -M rockchip -s <connector_id>@208:1024x600 -v

# Terminal 2: watch the VOP interrupt counter
watch -n 1 "cat /proc/interrupts | grep vop"

# Current value of the auto-disable delay (milliseconds)
sudo cat /sys/module/drm/parameters/vblankoffdelay   # mode 0600: root only
```

## 5. Results
While the page flip is active, I monitored the hardware interrupts via `/proc/interrupts`:
* **Command**: `watch -n 1 "cat /proc/interrupts | grep vop"`
* **Actual Output from LubanCat 5**:
    ```text
    61:       6044          0          0          0   GICv3 188 Level     fdd97e00.iommu, fdd90000.vop
    ```
* **Result**: The interrupt count for the VOP increases by exactly 60 per second, aligning perfectly with the 60Hz refresh rate of the DSI panel.

Reading the line from left to right: Linux IRQ number `61`, one count column per CPU shown, the interrupt controller (`GICv3`) and its hardware interrupt number `188`, trigger type `Level`, and the devices that registered a handler on this line.

> **Note:** the line is **shared** by `fdd97e00.iommu` and `fdd90000.vop`, so the counter includes every interrupt from both devices (and any VOP interrupt source, not only VBlank). A rate of exactly 60/s means the other sources were quiet during the measurement.

> `TODO(on-hardware)`: repeat the measurement (a) while `modetest -v` runs and (b) about 10 s after it stops. With the default `vblankoffdelay`, the counter is expected to stop increasing roughly 5 s after the last VBlank user disappears—confirm this.

## 6. Engineering Insight
This experiment confirms the integrity of the **Interrupt Handling Path**. In a professional Bring-up scenario, if the screen is on but the IRQ count isn't increasing *while a page-flip loop is running*, it usually indicates a "frozen" hardware state or a misconfigured Clock/Power domain. A static counter on an idle system, however, is normal: nobody requested VBlank events, so the core disabled the interrupt.

For a precise measurement of flip intervals and missed frames, see [Experiment 17](./17_Frame_Timing_Measurement.md).

## 7. Key Takeaways
* One VBlank interrupt per frame is the display's "heartbeat"—60/s on this panel.
* VBlank interrupts are enabled on demand; an idle counter is not necessarily a fault.
* Check whether the IRQ line is shared before interpreting the raw counter.

## 8. References
* Kernel source: `drivers/gpu/drm/drm_vblank.c` (`drm_vblank_offdelay`, module parameter `vblankoffdelay`) — checked on `v6.1` and master
* Kernel documentation: [`Documentation/gpu/drm-kms.rst`](https://github.com/torvalds/linux/blob/master/Documentation/gpu/drm-kms.rst) (Vertical Blanking section)
