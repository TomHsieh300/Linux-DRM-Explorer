# Hardware Validation Checklist

Experiments 12–17, and the Results sections of 01–11, contain `TODO(on-hardware)` placeholders. This page lists everything that still has to be run or observed on the LubanCat 5, in two parts:

* **Part A — automated**: one script collects all read-only output (and, optionally, the frame-timing numbers).
* **Part B — manual**: things that need someone looking at the panel.

Run everything from a text console with **no compositor**, after `make`, as root.

---

## Part A — Automated collection

```bash
make
sudo tools/collect-results.sh            # read-only, never modesets
sudo tools/collect-results.sh --active   # also runs Experiment 17 (takes over the display for ~1 min)
```

The script writes `hw-results-<date>-<time>/` plus a `.tar.gz` of it. Every file starts with the exact command and ends with its exit code; `00-summary.txt` lists all of them. A missing tool (e.g. `modetest`, `dtc`) is noted instead of aborting.

| Output file(s) | Fills in |
| :--- | :--- |
| `01-*.txt` | README → Test Environment table (kernel, BSP vs mainline, libdrm, driver name, debugfs layout) |
| `02-modetest-{connectors,encoders,planes}.txt` | [01](./01_Hardware_Inventory.md) §5, [02](./02_Modetest_Mastery.md) §5, [05](./05_Composition_and_Sync.md) §5 |
| `03-regs-*.txt`, `03-live-dt.dts` | [03](./03_DSI_Panel_Bringup.md) §5 (full register words, loaded panel timing), [15](./15_Device_Tree_and_Driver_Walkthrough.md) §5 |
| `04-clients.txt` | [04](./04_DRM_Master_and_Race_Condition.md) §6 (case: console only) |
| `06-*.txt` | [06](./06_VBlank_and_PageFlip.md) §5 (IRQ line, `vblankoffdelay`) |
| `10-atomic-property-discovery.txt` | [10](./10_Atomic_KMS_Implementation.md) Results |
| `12-plane-props-list.txt` | [12](./12_Plane_Properties_and_Blending.md) §5.1 |
| `13-formats-modifiers-list.txt` | [13](./13_Pixel_Formats_and_Modifiers.md) §5.2 |
| `14-*.txt` | [14](./14_Debugging_and_Tracing.md) §5 (debugfs files present, tracepoints available) |
| `16-hotplug-topology.txt` | [16](./16_Multi_Display_and_Hotplug.md) §5 (DSI only; repeat with HDMI connected, see Part B) |
| `17-frame-timing-*.txt/.csv` (`--active` only) | [17](./17_Frame_Timing_Measurement.md) §5.1–5.3 |
| `18-*.txt` | [18](./18_GPU_Stack_Discovery.md) §5 (GPU driver, device node, libmali/Mesa, EGL extensions). Install `mesa-utils` first so `eglinfo` is available. |
| `99-dmesg-drm.txt` | Context for any failure above |

---

## Part B — Manual observations

For each item, note what you **saw** and any terminal output. `<conn>` / `<plane>` IDs come from `02-modetest-*.txt`; `208` is the CRTC ID recorded in Experiment 01.

### Environment and open questions
- [ ] Is the running kernel the LubanCat BSP (`lbc-develop-6.1`) or mainline? (`01-uname.txt`)
- [ ] Is the 1024x600 VP3/DSI1 overlay the one loaded? Compare `hactive`/porches in `03-live-dt.dts` with [Experiment 03](./03_DSI_Panel_Bringup.md) §5.1.
- [ ] Source of the "VP0 supports 8K" statement in [Experiment 01](./01_Hardware_Inventory.md) (datasheet/TRM page, or BSP code).

### 02 · modetest
- [ ] `sudo modetest -M rockchip -s <conn>@208:1024x600` — test pattern appears? (photo)
- [ ] Same with `-a` (atomic) — same result?

### 04 · DRM Master
- [ ] `sudo modetest -M rockchip -w <conn>:DPMS:3` — panel turns off and comes back when modetest exits?
- [ ] While `sudo ./src/drm-atomic-demo --atomic` runs, from a second shell (SSH): `sudo cat /sys/kernel/debug/dri/0/clients` — which process shows `master = y`?

### 05 · Planes
- [ ] `sudo modetest -M rockchip -s <conn>@208:1024x600 -P <overlay_plane>@208:400x300+100+100` — two stacked patterns?

### 06 · VBlank IRQ
- [ ] With `modetest ... -v` running, `watch -n 1 "grep vop /proc/interrupts"` increases by ~60/s?
- [ ] ~10 s after stopping it, the counter stops increasing (default `vblankoffdelay` = 5000 ms)?

### 07 / 08 · Legacy KMS
- [ ] `sudo ./src/modeset-single-buffer` and `sudo ./src/modeset-double-buffer` — describe what is shown.

### 09 · Tearing
- [ ] `--singlebuf`, default, `--pageflip` modes of `sudo ./src/drm-vblank-sync-demo` — where/how often does a tear line appear in each?

### 10 · Atomic
- [ ] `sudo ./src/drm-atomic-demo --atomic` and `--multiplane` — smooth animation? overlay visible?

### 11 · DMA-BUF & fences
- [ ] `sudo ./src/drm-dmabuf-fence`, `--nosync`, `--fence` — any visible difference between the three?

### 12 · Plane properties
- [ ] `sudo ./src/drm-plane-props --demo` — for each phase (alpha, zpos, scaling, blend modes): shown as expected, or reported as rejected by TEST_ONLY?
- [ ] Blend-mode phase: does `None` look like `Coverage`? (§5.2 of the doc)
- [ ] After Ctrl+C: `Original KMS state restored.` printed and console back?

### 13 · Formats
- [ ] `sudo ./src/drm-formats-modifiers --nv12` — which plane was chosen; do the bars look correct or colour-shifted?
- [ ] `sudo ./src/drm-formats-modifiers --nv12 --bt709` — which of the two looks correct?
- [ ] Bottom luma ramp: are the left and right ends flat black/white?

### 14 · Debug capture
- [ ] `sudo tools/drm-debug-capture.sh -m 0x14 -- ./src/drm-atomic-demo --atomic` — send the output directory.
- [ ] `sudo tools/drm-debug-capture.sh -m 0x20 -T -t 3` while an animation runs — send the output directory.

### 16 · Hotplug (needs an HDMI monitor)
- [ ] `sudo ./src/drm-hotplug-monitor --topology` with HDMI connected.
- [ ] `sudo ./src/drm-hotplug-monitor --monitor`, then unplug and replug HDMI — paste the output; is `CONNECTOR=` present in the uevent?

### 17 · Frame timing
- [ ] Covered by `--active` in Part A. Additionally: press Ctrl+C during `sudo ./src/drm-frame-timing -n 3000` — partial summary printed and console back?

---

## Sending results back
Send the `hw-results-*.tar.gz`, the capture directories from item 14, and your notes for Part B (a text list following the headings above is enough). Photos help for 02, 09, 12 and 13.
