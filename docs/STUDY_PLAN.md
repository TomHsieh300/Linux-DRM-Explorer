# Study Plan: From Experiment Notes to a DRM Textbook and Lab Course

Status: **design proposal**. Nothing below is implemented yet.

> **Scope note.** This plan is based on the contents of this repository only (README, `docs/01`–`18`, `HARDWARE_VALIDATION.md`, `src/`, `tools/`, `Makefile`, CI, git history).
> Three read-only surveys feed this plan:
> * [`notes/board-kernel-config.md`](./notes/board-kernel-config.md) — the running kernel's `/proc/config.gz` (8460 lines, md5 `931026800151912fc38c37fce658e5a9`, identical to `/boot/config-6.1.99-rk3588`). The full file is kept on the host only, at `/home/tom/LubanCat-board/config-6.1.99-rk3588`.
> * [`notes/board-runtime.md`](./notes/board-runtime.md) — the **running board** (serial console, 2026-10-10). This is the reference for everything that runs.
> * [`notes/lubancat-inventory.md`](./notes/lubancat-inventory.md) — the SDK in `/home/tom/LubanCat`. **The board does not run this SDK's kernel or image**, so its kernel source, Kconfig and DT facts describe the SDK only; its userspace package facts still need a per-item check against the board.
>
> **Policy: the LubanCat directory is never committed or pushed to this repository** (kernel trees, images and toolchains are too large). It stays outside the repo; labs refer to it through an environment variable (`LUBANCAT_DIR`, default `/home/tom/LubanCat`) and the repo contains only small scripts, notes and results.

---

## 1. Current state (facts from the repo)

| Area | State |
| :--- | :--- |
| Experiments | 18 docs (`docs/01`–`18`), 9 C programs in `src/` (~9,700 lines), 2 scripts in `tools/` |
| Hardware validation | Experiments 12–17 are compile-only; README says their Results are placeholders. Every doc `01`–`18` still contains at least one `TODO(on-hardware)` (`grep -c` shows 1–10 per file). The README Test Environment table is all `TODO` (OS image, kernel, kernel tree/commit, driver, libdrm). |
| Planned, unwritten | Experiments 19 (GPU render → dma-buf → KMS) and 20 (tracing userspace → kernel), per README Phase 5 |
| Build / CI | `make` builds every `src/*.c`; GitHub Actions builds native and aarch64 with `-Werror`. No automated tests that run anything |
| Structure | Chronological lab notes, each with Objective → Environment → Background → Steps → Results → Analysis → Takeaways → References |
| Board (running) | **LubanCat-5IO** (`embedfire,rk3588-lubancat-5io`), kernel **6.1.99-rk3588** `#7` built 2026-02-13 on `tom-B550M-AORUS-ELITE` with the Arm gcc 10.3-2021.07 toolchain (source tree/commit **unknown**, no `linux-image` package), Ubuntu 20.04.6 (image built 2026-02-13), libdrm 2.4.107 (+`libdrm-dev`), `modetest` from `rktoolkit` |
| Kernel config (running) | `CONFIG_LOCALVERSION="-rk3588"`. **Modules:** `MODULES=y`, `MODVERSIONS` and `MODULE_SIG` not set. **Display:** `DRM_ROCKCHIP`, `ROCKCHIP_VOP2`, `ROCKCHIP_DW_MIPI_DSI`, `ROCKCHIP_DW_MIPI_DSI2`, `DRM_PANEL_SIMPLE`, `ROCKCHIP_DRM_DEBUG` all `y`; `DRM_VKMS` not set. **GPU:** `MALI_BIFROST=y`, `MALI_CSF_SUPPORT=y`, plus `MALI_BIFROST_DEBUG`, `_FENCE_DEBUG`, `_ENABLE_TRACE`, `_SYSTEM_TRACE`; no panthor, `DRM_PANFROST` not set. **Sync/buffers:** `SYNC_FILE`, `SW_SYNC`, `DMABUF_HEAPS{,_SYSTEM,_CMA}` = `y`. **Debug:** `DEBUG_FS`, `DYNAMIC_DEBUG`, `TRACING`, `FTRACE`, `FUNCTION_TRACER` = `y`; `KPROBES` **not set**. **Media:** `ROCKCHIP_MPP_SERVICE` + RKVDEC/RKVDEC2/AV1DEC/… = `y`; `VIDEO_ROCKCHIP_RGA=y` (V4L2), `ROCKCHIP_RGA` not set but `ROCKCHIP_RGA_ASYNC/PROC_FS/DEBUG_FS/DEBUGGER=y`; `VIDEO_ROCKCHIP_HDMIRX=y`. Source tree/commit still unknown |
| DRM devices (running) | `card0` = `rockchip-drm` (`display-subsystem`), `card1`/`renderD129` = `rknpu` (NPU); `renderD128` belongs to card0. `/dev/mali0` present (kbase); dma-heaps `system`, `system-uncached`, `cma` |
| Outputs (running) | card0 connectors: DP-1, HDMI-A-1, HDMI-A-2 (all disconnected at capture), **Writeback-1**. **No DSI connector**: DSI overlays are commented out in `uEnv`; `5io-dsi{0,1}-generic` plugins are auto-loaded only when an EEPROM is detected at 0x51 on I²C 2/5. Active overlays: `5io-hdmi0-vp0`, `dp0-vp1`, `5io-hdmi1-vp2` |
| SDK on host | `LubanCat_Linux_rk3588_SDK_20260421`: kernel 5.10.160 @ `ca43ccf`, Ubuntu 20.04.6 lite rootfs, libmali `valhall-g610-g13p0-x11-gbm` 1.9-1, MPP 1.5.0, librga 2.2.0, gcc 10.3 toolchain. No mainline tree, no TRM, no source for libmali/MPP/RGA. **Not what the board runs** |

### Conflicts between the running board and the existing docs
These must be resolved before new content builds on them (Phase 0, L0.4):

| Doc(s) | Doc currently says | Board says | Action |
| :--- | :--- | :--- | :--- |
| README, 01, 03, 15, `HARDWARE_VALIDATION.md` | Board is a **LubanCat 5**; display is a 1024x600 **DSI panel on VP3**, CRTC ID 208 | Board is a **LubanCat-5IO**; **no DSI connector** at this boot; active outputs are HDMI0/VP0, DP0/VP1, HDMI1/VP2 | Decide the reference display setup (see open question 1). Either reconnect the DSI panel and enable the matching `5io-dsi*` overlay, or move the experiments to HDMI/DP. Results in 01/03 recorded with the DSI panel must state which board/kernel/overlay they came from |
| 03, 18 | BSP citations from `LubanCat/kernel` `lbc-develop-6.1` / `rockchip-linux/kernel` `develop-6.1` | Running kernel is 6.1.99-rk3588, but its source commit is unknown | Identify the running kernel's source tree (open question 2), then re-check the citations against it. Do **not** use the SDK 5.10 tree for this |
| 01, 02, 07–12 | API/option details checked against libdrm **2.4.125** | libdrm **2.4.107** | Run `modetest -h` (rktoolkit build) on the board; check every libdrm call in `src/` exists in 2.4.107. The board has `libdrm-dev`, so a native build on the board is the direct test |
| 18 | `eglinfo` via Ubuntu 24.04 `mesa-utils` 9.0.0; GPU stack unknown | Ubuntu 20.04; `/dev/mali0` present → kbase. libmali package on the board **not yet checked** (the SDK's g13p0 build is SDK-only evidence) | Check `dpkg -l 'libmali*'` on the board; find out whether `eglinfo` is available on 20.04 (not verified) or write a small EGL query program |
| `src/*.c`, `tools/` | Hard-code `/dev/dri/card0` as the display device | card0 is the display **at this boot**; card1 is the NPU. Probe order between `rockchip-drm` and `rknpu` is not guaranteed by anything checked so far | Keep card0 as default but select the device by driver name (`drmGetVersion()->name == "rockchip"`) or document `-d` everywhere |
| README build | Cross build on Ubuntu 24.04 multiarch (newer libdrm headers) | Target libdrm is 2.4.107; board has `libdrm-dev` and the running kernel was built with the Arm gcc 10.3 toolchain | Either build natively on the board, or cross-build against a sysroot of the board's rootfs |

New possibilities the board offers, not covered by any doc yet: a **Writeback connector** (KMS writeback → capture the composed frame into a buffer), **two HDMI + one DP** outputs (multi-display labs if monitors are available), and an **NPU DRM device** (a second DRM driver on the same SoC to compare against `rockchip-drm`).

### What is strong
* Source-cited kernel explanations and an explicit "not measured unless it was" rule.
* Good coverage of the userspace KMS API (legacy → atomic), planes, formats, sync, tracing.

### Gaps
1. **It is a lab diary, not a textbook.** Concepts are introduced where the experiment needs them; there is no concept-first path, glossary, or index, and prerequisites between docs are implicit.
2. **Unvalidated.** The biggest risk to credibility is that most Results sections are empty. New content built on unmeasured results compounds the problem.
3. **Userspace-only.** Everything is a libdrm client. There is no lab where the reader *writes or modifies kernel code*, though doc 15 reads the driver.
4. **Single platform.** No mainline vs BSP comparison is carried through the labs; no way to practise without the board.
5. **No self-assessment.** No exercises, expected outputs, or pass/fail criteria per experiment.
6. **Missing topics** (see §5): GPU path (19/20 planned), color pipeline, video/camera buffers, compositor-level use of KMS, power, panel/backlight, DRM from the kernel side.

---

## 2. Audience and goal

The only reader is the author, whose goal is to become a **domain expert in Linux DRM/KMS and the GPU-to-display stack** (BSP / driver level), not to produce a polished public book. That changes the design:

* Depth over polish: kernel source reading, driver writing and debugging take priority over prose.
* The standard for "done" with a topic is: can explain it from the kernel source, reproduce it on the board, and modify the driver to change its behaviour.
* Public-quality editing is optional and comes last.
* Language: **English throughout** (docs, code comments, lab text).

## 3. Design principles

1. **Validate before extending.** Phase 0 below is a hard gate for anything that depends on measured numbers.
2. **Two artefacts, one source of truth.** The *textbook* explains concepts and cites kernel source; the *labs* are hands-on and link into the textbook. A concept is written once, in the textbook.
3. **Every lab is checkable.** Each lab states observable pass criteria (a printed value, a file, a tracepoint) so the reader can tell whether it worked without a reference answer.
4. **No fabricated results.** Keep the existing rule: unmeasured → `TODO(on-hardware)`; a claim about the kernel cites file and function, and says which tree/tag it was checked on.
5. **Board-optional where possible.** Where a concept does not need Rockchip hardware, provide a variant that runs on any Linux machine (VKMS, `drm_info`, tracepoints) so the course is usable without the board.

---

## 4. Proposed textbook structure

Working title: *Linux DRM/KMS and the GPU-to-Display Pipeline, on RK3588*.
Each chapter = concept text (new) + pointer to the existing experiment doc(s) as the worked example.

| Part | Chapter | Content | Existing material to reuse |
| :--- | :--- | :--- | :--- |
| **I. Foundations** | 1. Display pipeline in one page | Scanout, framebuffer, timing, why a display controller exists | 01, 03 background |
| | 2. DRM architecture | `drm_device`, card/render nodes, DRM master, file ops, ioctl dispatch | 04, 14 |
| | 3. KMS object model | Framebuffer, plane, CRTC, encoder, connector, properties, `possible_crtcs` | 01, 02 |
| **II. Memory & sync** | 4. Buffers | GEM, dumb buffers, dma-buf/PRIME, heaps, cache coherency | 07, 11 |
| | 5. Synchronization | VBlank, page flip, tearing, dma-fence, sync_file, implicit vs explicit | 06, 09, 11 |
| | 6. Formats & modifiers | fourcc, tiling/AFBC, YUV | 13 |
| **III. Atomic KMS** | 7. Atomic model | Property commits, `TEST_ONLY`, flags, nonblocking, state objects | 10 |
| | 8. Planes & composition | zpos, alpha, blend, scaling, rotation, plane assignment | 05, 12 |
| | 9. Topology | Multiple CRTCs/connectors, hotplug, EDID | 16 |
| **IV. Hardware bring-up** | 10. Panel & DSI | Timings, DPHY, device tree, panel drivers | 03, 15 |
| | 11. VOP2 driver walkthrough | Bind flow, atomic check/flush, interrupts | 15 |
| **V. GPU & media** | 12. GPU driver stacks | kbase + libmali vs panthor + Mesa | 18 |
| | 13. GPU → KMS zero copy | EGL/GBM/dma-buf import, native fence | **new (Exp 19)** |
| | 14. Video & 2D engines | decoder → dma-buf → NV12 plane, 2D blitter | **new** |
| **VI. Engineering** | 15. Measuring | Frame timing, latency, missed vblanks | 17 |
| | 16. Debugging & tracing | drm.debug, debugfs, tracepoints, end-to-end trace | 14, **new (Exp 20)** |
| | 17. Compositors | How a compositor drives KMS (plane offload, direct scanout) | **new** |
| **Appendices** | A. Cheat sheet of ioctls/properties · B. Glossary · C. BSP vs mainline differences · D. Troubleshooting matrix |

---

## 5. Proposed lab series

Numbering continues the existing sequence. Labs 19–20 are already promised by the README.

### Phase 0 — Validate what exists (gate)
* **L0.1** Fill the README Test Environment table from `notes/board-runtime.md` (done for what was collected); `/proc/config.gz` collected (`notes/board-kernel-config.md`); still missing: running kernel source/commit, loaded DTB, libmali package.
* **L0.2** Choose the reference display setup (open question 1), then run `tools/collect-results.sh` (+ `--active`) and the manual Part B list; fill all `TODO(on-hardware)` in docs 01–18 with results from **this** board.
* **L0.3** Resolve the open questions in `HARDWARE_VALIDATION.md`: which DTB U-Boot loads (`/boot/rk-kernel.dtb` exists; extlinux uses `devicetreedir /`), why no DSI panel probed, source of the VP0 "8K" claim (the board's `uEnv` comment says 8K output occupies VP0+VP1 and needs the `hdmi-8k` overlay — a vendor comment, not a hardware reference).
* **L0.4** Resolve every row of "Conflicts between the running board and the existing docs" (§1).
* *Exit criterion:* no `TODO(on-hardware)` remaining in docs 01–18, or each remaining one has a stated reason.

### Phase 5 — GPU → Display (already planned)
* **L19 GPU render → dma-buf → KMS.** On the board the kernel side is kbase (`/dev/mali0`); the userspace library on the board is not yet checked (the SDK rootfs has libmali `x11-gbm`, which ships `libgbm.so`). Whether its EGL exposes the dma-buf import / native-fence extensions is still unknown until queried on the board. Path chosen from Experiment 18's EGL extension results (GBM scanout / dma-buf import / native-fence → `IN_FENCE_FD`). Baseline: `glReadPixels` copy; measure what zero-copy saves with the L17 timing tool.
* **L20 End-to-end trace.** One frame from GL call to scanout: ioctl boundary, fences, atomic commit, vblank event; produced with the L14 capture script. Available on the running kernel: ftrace with `FUNCTION_TRACER`, `DYNAMIC_DEBUG`, debugfs, and the kbase trace options (`MALI_BIFROST_ENABLE_TRACE`, `_SYSTEM_TRACE`, `_FENCE_DEBUG`). **`KPROBES` is not set**, so kprobe events (and kprobe-based tools such as bpftrace probes on kernel functions) cannot be used; stick to tracepoints and the function tracer, or rebuild the kernel with `KPROBES=y`. Which kbase tracepoints these options actually create must be read in the running kernel's source.

### Phase 6 — Kernel-side (new)
* **L21 VKMS as a sandbox** `[board-optional]`. Two variants: (a) on a PC with a distro kernel; (b) on the board: `DRM_VKMS` is not set in the running kernel, so build `vkms` as a module. The exact `.config` is now on the host; the matching **source** is still unknown. With `MODVERSIONS` and `MODULE_SIG` off, the loader checks the vermagic string, not symbol CRCs, so a module built from a different 6.1.99 tree may load but can still crash if struct layouts differ — build only from the tree the kernel was built from. Run the existing atomic/plane tools against it and compare with the VOP2 output of L01/L16.
* **L22 Minimal KMS driver.** Out-of-tree module that registers one plane + CRTC + connector with a fake vblank timer, driven by `src/drm-atomic-demo`. Goal: the reader implements `atomic_check`/`atomic_flush` themselves. Target kernel: the running **6.1.99-rk3588** (`LOCALVERSION="-rk3588"`, `MODULES=y`, no module signing or modversions). Build with the host copy of the running `.config` and the same Arm gcc 10.3-2021.07 toolchain, from the kernel's own source tree (still to be located, open question 2). Choose the helper API by reading that tree's `include/drm/` (upstream has been phasing out the simple-pipe helpers — note differences for L32).
* **L23 Add a property.** Add a custom plane/CRTC property to L22 and read/write it from userspace; ties back to L10/L12.
* **L24 Panel driver exercise.** Needs a DSI panel attached and a `5io-dsi*` overlay enabled (no DSI connector at the captured boot). In the SDK tree the LubanCat DSI overlays use `compatible = "simple-panel-dsi"` (`drivers/gpu/drm/panel/panel-simple.c`) with the Synopsys DSI host drivers; re-check this in the running 6.1.99 tree. Trace how the overlay's `display-timings` and init sequence reach the panel and DSI host; then change one timing value in a copy of the overlay and observe via L03/L17.

### Phase 7 — Color, media and 2D (new; each needs a capability check first)
* **L25 Color pipeline.** Discover whether CRTC `GAMMA_LUT` / `CTM` and plane `COLOR_ENCODING`/`COLOR_RANGE` exist on this VOP2 driver, then use them. Extends L13's BT.601/709 observation.
* **L26 Video → plane.** Decoded NV12 buffer → dma-buf → overlay plane. The SDK rootfs provides Rockchip MPP 1.5.0 (`librockchip-mpp1`, `-dev`, `rockchip-mpp-demos`) and `gstreamer1.0-rockchip1` 1.14-4; prebuilt only, no source in the SDK. Kernel side on the board: `ROCKCHIP_MPP_SERVICE=y` with RKVDEC, RKVDEC2, AV1DEC, VDPU1/2, JPGDEC and others. Whether the MPP user libraries are installed on the board is not yet checked. Start from the MPP API and export the decoder's output buffer as a dma-buf (mechanism to be verified in the MPP headers).
* **L25b Writeback.** The board exposes `card0-Writeback-1`. Use a writeback connector (`WRITEBACK_FB_ID`, `WRITEBACK_OUT_FENCE_PTR`) to capture the composed output of a CRTC into a buffer and compare it with what was committed — a way to verify plane/blend results (L12, L13) without a camera. Which CRTCs it can attach to must be read from its `possible_crtcs` on the board.
* **L27b HDMI input → display.** `VIDEO_ROCKCHIP_HDMIRX=y` and the board's `uEnv` has a (commented-out) `5io-hdmi-rx` overlay. Capture HDMI input via V4L2, import the buffer as a dma-buf and scan it out on a plane: a real external producer for the L11/L13 zero-copy path. Needs an HDMI source and enabling that overlay.
* **L27 2D engine.** Rockchip RGA for scale/convert into a scanout buffer, using the SDK rootfs's `librga2` / `librga-dev` 2.2.0 (prebuilt; no source in the SDK). Running kernel: `VIDEO_ROCKCHIP_RGA=y` (the V4L2 mem2mem driver) and `ROCKCHIP_RGA_ASYNC/PROC_FS/DEBUG_FS/DEBUGGER=y`, while `ROCKCHIP_RGA` itself is not set; which option enables the driver that `librga` talks to was not matched by the config filter and must be read from the source Kconfig. Whether `librga` is installed on the board is not yet checked.

### Phase 8 — System level (new)
* **L28 Compositor behaviour.** Run a Wayland compositor on KMS; use L14 tracing and L12 plane-props to see which layers become hardware planes vs GPU composition.
* **L29 Direct scanout & latency.** Fullscreen client → direct scanout; compare flip latency to the L17 baseline.
* **L30 Power & robustness.** DPMS, runtime PM, hotplug during commit, restoring state on `SIGINT` (extends L16/L12 cleanup patterns).

### Phase 9 — Expert track (new, driven by the goal in §2)
* **L31 Source-reading drills.** For each earlier lab, trace one ioctl from `drm_ioctl()` to the driver callback in the source tree of the running 6.1.99-rk3588 kernel (to be located; the SDK's 5.10 tree at `ca43ccf` is a different kernel and only useful for a 5.10 → 6.1 comparison); write a one-page call-chain note. Self-check: draw it from memory.
* **L32 BSP vs mainline diff study.** Compare the Rockchip BSP display driver and the mainline `rockchip_drm_vop2.c` for the same feature (e.g. plane `zpos`, AFBC, interrupt handling). Output: a table of differences with file/function references. BSP side: the running 6.1.99 tree. Mainline side: a checkout to be added outside the repo (RK3588 VOP2 data is upstream from v6.8, panthor from v6.10, per docs 12 and 18). Compare per feature, not per file. Optional third column: the SDK's 5.10.160 tree, to see how the BSP driver evolved.
* **L33 Fix or extend a driver.** Pick a real defect or missing feature found during Phase 0–8 (e.g. a property the VOP2 driver does not expose), implement it in the board's tree, and verify with the earlier lab tools.
* **L34 Upstream-style patch.** Prepare the L33 change as a mailing-list-quality patch (checkpatch, commit message, test evidence) against current mainline; submission itself is optional.
* **L35 Hardware reference.** Read the RK3588 TRM sections for VOP2 / DSI and annotate register-level behaviour against traces from L14. **Blocked:** no TRM or datasheet was found on the host (inventory §8). Until one is obtained, the register source is the driver's own tables (`rockchip_vop2_reg.c`) and `Documentation/devicetree/bindings/display/rockchip/` in the kernel tree.

### Lab template (add to every lab)
```
Goal · Prerequisites (textbook chapter, earlier labs) · Setup
Task (what to build / run) · Expected observations
Pass criteria (checkable) · Self-check questions (3–5)
Stretch goals · Results (measured, with kernel/tree stated) · References
```

---

## 6. Delivery roadmap

| Step | Deliverable | Depends on |
| :--- | :--- | :--- |
| 1 | Phase 0 results merged; README environment table filled | Board access |
| 2 | `docs/textbook/` skeleton: chapter stubs + glossary; each links to existing experiment | none |
| 3 | Retrofit labs 01–18 with *Pass criteria* and *Self-check questions* | Step 1 |
| 4 | L19, L20 | Step 1 (Exp 18 results) |
| 5 | L21–L23 (kernel side, VKMS first so it is board-optional) | none for L21a; running kernel's source + config for L21b/L22/L23 |
| 6 | Textbook chapters 1–11 written from existing docs | Step 2 |
| 7 | Phases 7–8, chapters 12–17 | Steps 1, 4 |
| 8 | Phase 9 expert track, interleaved with 4–7 (do L31 alongside each lab rather than at the end) | `LUBANCAT_DIR` |
| 9 | CI: add smoke tests that need no hardware (e.g. run tools against VKMS in a container if the CI runner allows loading it — **unverified**) | Step 5 |

---

## 7. Handling the external LubanCat directory

* Never `git add` it. Add `/LubanCat/` and `LUBANCAT_DIR` mounts to `.gitignore` if it is ever symlinked into the repo.
* Scripts take the path from `LUBANCAT_DIR` (default `/home/tom/LubanCat`); they must not copy files out of it.
* Notes derived from it (call chains, diffs, register annotations) are committed as small text files with the exact tree path, branch and commit they were checked against.
* Cloud sessions cannot see it. Anything that needs it is done locally, or the relevant small pieces are provided explicitly.

---

## 8. Open questions
1. **Reference display setup.** Reconnect the 1024x600 DSI panel (and which DSI port / `5io-dsi*` overlay), or move the experiments to HDMI/DP? Which monitors are available?
2. **Running kernel source.** The 6.1.99-rk3588 kernel was built on `tom-B550M-AORUS-ELITE` on 2026-02-13. Its `.config` is now known; where is the source tree, and which branch/commit? This is needed for L0.4, L21b, L22, L23, L31, L32.
3. Is it acceptable to risk the display when loading test modules (L21b, L22)? The running kernel allows it technically (`MODULES=y`, no signing).
4. Should the kernel be rebuilt with `KPROBES=y` for the tracing labs (L14, L20), or should they stay within tracepoints and the function tracer?
5. Can a RK3588 TRM be obtained (L35)?
6. Where should the mainline kernel checkout for L32 live (outside the repo)?
