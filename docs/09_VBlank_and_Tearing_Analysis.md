# Experiment 09: VBlank Synchronization vs. Screen Tearing

## 1. Objective
Analyze the visual and architectural differences between **Synchronous Page Flipping** and **Asynchronous Buffer Updates**. This experiment demonstrates how screen tearing occurs and how the DRM subsystem leverages VBlank interrupts to achieve fluid, tear-free animation.

---

## 2. Deep Dive: The Mechanics of Tearing

### The Scanout Race
Screen tearing is a **data race** between the Display Controller (Reader) and the CPU (Writer).

* **The Reader**: The VOP2 (Display Engine) reads the framebuffer line-by-line from top to bottom (Scanout). For a 60Hz display, one full scan takes ~16.6ms.
* **The Writer**: The CPU updates pixel data via `mmap` or buffer swaps.
* **The Conflict**: If the CPU finishes a new frame while the VOP2 is in the middle of scanning the screen (e.g., at line 500 of 1080), the upper part of the screen shows the **old frame**, while the lower part shows the **new frame**.



### The Solution: VBlank & Shadow Registers
To prevent this, we must only swap buffers during the **Vertical Blanking Interval (VBlank)**¡Xthe brief pause when the controller is not reading any pixel data.

* **Shadow Registers**: Modern SoCs like the RK3588 use "shadow registers" for the framebuffer address. When we call `PageFlip`, the hardware stores the new address in a buffer and only applies it to the active register during the next VBlank.

---

## 3. High-Level Logic Flow (Pseudocode)

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

## 4. Comparison Table

| Mode | API Used | Sync Method | Visual Result | CPU Load |
| :--- | :--- | :--- | :--- | :--- |
| **Single Buffer** | `mmap` direct write | None (Race Condition) | Severe horizontal "fractures" | **High** (Spinning) |
| **Tearing Demo** | `drmModeSetCrtc` | `usleep()` (Manual) | Frequent flickering/tearing | **Medium** |
| **Page Flip** | `drmModePageFlip` | **VBlank IRQ (Hardware)** | **Perfectly smooth** | **Low** (Event-driven) |

## 5. Execution & Observation

```bash
# 1. Observe the "Moving Tear Line" (CPU vs DMA race)
sudo ./src/drm-vblank-sync-demo --singlebuf

# 2. Observe the "Address Swap Race" (Shadow register race)
sudo ./src/drm-vblank-sync-demo

# 3. Observe Professional Grade Animation (Sync'd)
sudo ./src/drm-vblank-sync-demo --pageflip
```
