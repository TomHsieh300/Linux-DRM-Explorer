# Experiment 09: VBlank Synchronization vs. Screen Tearing

## 1. Objective
Analyze the visual and architectural differences between **Synchronous Page Flipping** and **Asynchronous Buffer Updates**. This experiment demonstrates how screen tearing occurs and how the DRM subsystem uses VBlank events to achieve fluid animation.

## 2. Theoretical Background: What is Tearing?
Tearing occurs when the CPU updates the memory that the display controller is actively reading (Scanning out). If the update happens mid-frame, the top half of the screen shows the "old" frame while the bottom half shows the "new" frame.



## 3. Key Mechanisms in this Experiment
* **Non-blocking Event Loop**: Uses `select()` to sleep the process until the kernel sends a `DRM_EVENT_FLIP_COMPLETE` event. This is much more CPU-efficient than polling.
* **Kernel Callback (`page_flip_handler`)**: Demonstrates how userspace applications "subscribe" to hardware interrupts.
* **Shadow Register Logic**: Note that in RK3588 VOP2, address registers are shadowed. `drmModePageFlip` ensures the address is swapped only during the Vertical Blanking Interval.

## 4. Comparison Table

| Mode | API Used | Sync Method | Visual Result |
| :--- | :--- | :--- | :--- |
| **Single Buffer** | `mmap` write | None (Race) | Severe tearing; moving bar appears fragmented. |
| **Tearing Demo** | `drmModeSetCrtc` | usleep (Fixed) | High tearing; buffer swap races against DMA read. |
| **Page Flip** | `drmModePageFlip` | VBlank IRQ | Perfectly smooth; no horizontal discontinuities. |

## 5. Execution
```bash
# To observe the tearing artifact:
sudo ./src/drm-vblank-sync-demo --singlebuf

# To observe smooth, sync'd animation:
sudo ./src/drm-vblank-sync-demo --pageflip
```
