# Experiment 09: VBlank Synchronization vs. Screen Tearing

## 1. Objective
Analyze the visual and architectural differences between **Synchronous Page Flipping** and **Asynchronous Buffer Updates**. This experiment demonstrates how screen tearing occurs and how the DRM subsystem leverages VBlank interrupts to achieve fluid, tear-free animation.

---

## 2. Environment
See the [Test Environment](../README.md#test-environment) section.

---

## 3. Deep Dive: The Mechanics of Tearing

### The Scanout Race
Screen tearing is a **data race** between the Display Controller (Reader) and the CPU (Writer).

* **The Reader**: The VOP2 (Display Engine) reads the framebuffer line-by-line from top to bottom (Scanout). For a 60Hz display, one full scan takes ~16.6ms.
* **The Writer**: The CPU updates pixel data via `mmap` or buffer swaps.
* **The Conflict**: If the CPU finishes a new frame while the VOP2 is in the middle of scanning the screen (e.g., at line 300 of this panel's 600 active lines), the upper part of the screen shows the **old frame**, while the lower part shows the **new frame**.

#### Timing diagram (numbers for this board's 1024x600 panel, from [Experiment 03](./03_DSI_Panel_Bringup.md))
```text
 one frame = 636 lines x 1354 clocks / 51.668 MHz  ~= 16.667 ms
|<---------------- active scanout: lines 0..599 (~15.72 ms) ---------------->|<- VBlank: 36 lines (~0.94 ms) ->|

Unsynchronized swap (tearing):
 scanout line:  0 ........ 300 ...................................... 599 | blank |
 FB address:    [ frame N  ][ frame N+1 ---------------------------------- ]         <- swapped mid-scan
 on screen:     top half = frame N, bottom half = frame N+1   -> visible tear line at ~line 300

VBlank-synchronized flip (page flip / atomic commit):
 scanout line:  0 ....................................................... 599 | blank |  0 .......
 FB address:    [ frame N --------------------------------------------------- ]|latch |[ frame N+1 ...
 on screen:     every scanned frame comes from a single buffer             -> no tear
```
The only safe moment to change the scanout address is the ~0.94 ms VBlank window, which is why the flip is latched by hardware at VBlank rather than applied immediately by software.



### The Solution: VBlank & Shadow Registers
To prevent this, we must only swap buffers during the **Vertical Blanking Interval (VBlank)**—the brief pause when the controller is not reading any pixel data.

* **Shadow Registers**: Modern SoCs like the RK3588 use "shadow registers" for the framebuffer address. When we call `PageFlip`, the hardware stores the new address in a buffer and only applies it to the active register during the next VBlank.

---

## 4. High-Level Logic Flow (Pseudocode)

To visualize the architectural differences, here is the logic for the three modes implemented in `src/drm-vblank-sync-demo.c`:

### Mode A: Single Buffer (Guaranteed Tearing)
The CPU and DMA read/write to the **same** memory space concurrently.
```c
while (true) {
    // CPU overwrites the active scanout buffer as fast as possible
    draw_moving_bar(active_buffer); 
    update_animation_state();
    // RESULT: Immediate tearing as VOP2 reads mid-write.
}
```

### Mode B: Legacy SetCrtc (Manual Double Buffering)
Uses two buffers, but the "switch" instruction is sent without synchronization.
```c
while (true) {
    draw_moving_bar(back_buffer);
    // Legacy API: Tells the kernel to switch FB_ID immediately
    drmModeSetCrtc(fd, crtc_id, back_buffer_fb_id, ...);
    
    usleep(2000); // Heavy race condition with the scanout pointer
}
```

### Mode C: Page Flip (VBlank Synchronized)
The professional event-driven approach using Linux DRM event notification.
```c
void page_flip_handler(...) {
    waiting = false; // Triggered by the Kernel when VBlank IRQ occurs
}

while (true) {
    draw_moving_bar(back_buffer);
    
    // 1. Request an atomic swap on the NEXT VBlank
    drmModePageFlip(fd, crtc_id, back_buffer_fb_id, DRM_MODE_PAGE_FLIP_EVENT, &waiting);
    waiting = true;

    // 2. Block until the hardware interrupt confirms completion
    while (waiting) {
        select(fd + 1, ...);         // Yield CPU, wait for DRM event
        drmHandleEvent(fd, &ev_ctx); // Dispatches to page_flip_handler()
    }
    
    // Swap buffer roles for the next frame
    swap(front_buffer, back_buffer);
}
```

## 5. Comparison Table

| Mode | API Used | Sync Method | Visual Result | CPU Load |
| :--- | :--- | :--- | :--- | :--- |
| **Single Buffer** | `mmap` direct write | None (Race Condition) | Severe horizontal "fractures" | **High** (Spinning) |
| **Tearing Demo** | `drmModeSetCrtc` | `usleep()` (Manual) | Frequent flickering/tearing | **Medium** |
| **Page Flip** | `drmModePageFlip` | **VBlank IRQ (Hardware)** | **Perfectly smooth** | **Low** (Event-driven) |

## 6. Execution & Observation

```bash
# 1. Observe the "Moving Tear Line" (CPU vs DMA race)
sudo ./src/drm-vblank-sync-demo --singlebuf

# 2. Observe the "Address Swap Race" (Shadow register race)
sudo ./src/drm-vblank-sync-demo

# 3. Observe Professional Grade Animation (Sync'd)
sudo ./src/drm-vblank-sync-demo --pageflip
```

## 7. Results
> `TODO(on-hardware)`: record the exact command lines, program output and observations from the LubanCat 5 for this experiment (kernel version as listed in the README's Test Environment).

## 8. References
* libdrm 2.4.125 `xf86drmMode.c`: `drmModePageFlip`, `drmHandleEvent`
* Kernel source: `drivers/gpu/drm/drm_plane.c` (`drm_mode_page_flip_ioctl`), `drivers/gpu/drm/drm_vblank.c`
* Kernel documentation: [`Documentation/gpu/drm-kms.rst`](https://github.com/torvalds/linux/blob/master/Documentation/gpu/drm-kms.rst) (Vertical Blanking)
