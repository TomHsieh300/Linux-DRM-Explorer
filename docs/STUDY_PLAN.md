# Study Plan: From Experiment Notes to a DRM Textbook and Lab Course

Status: **design proposal**. Nothing below is implemented yet.

> **Scope note.** This plan is based on the contents of this repository only (README, `docs/01`–`18`, `HARDWARE_VALIDATION.md`, `src/`, `tools/`, `Makefile`, CI, git history).
> Facts about the LubanCat SDK (`/home/tom/LubanCat`) come from [`notes/lubancat-inventory.md`](./notes/lubancat-inventory.md), a read-only survey taken on the host on 2026-10-10. That survey describes the **SDK build**, not the running board: whether the board runs this exact kernel and rootfs has **not** been confirmed (Phase 0, L0.1).
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
| Target | Single board: LubanCat 5 (RK3588, VOP2), one 1024x600 DSI panel on VP3 (per README) |
| SDK (from the inventory) | `LubanCat_Linux_rk3588_SDK_20260421`: BSP kernel **5.10.160** (`LubanCat/kernel` @ `ca43ccf`), defconfig `lubancat_linux_rk3588_defconfig`, Ubuntu **20.04.6** lite rootfs, libdrm **2.4.107**, libmali `valhall-g610-g13p0-x11-gbm` 1.9-1, MPP 1.5.0, librga 2.2.0, gcc 10.3 cross toolchain. No mainline kernel tree, no TRM, no source for libmali/MPP/RGA |

### Conflicts between the SDK and the existing docs
The inventory contradicts assumptions in several docs. These must be resolved before new content builds on them:

| Doc(s) | Doc currently says | SDK inventory says | Action |
| :--- | :--- | :--- | :--- |
| 03, 18, `HARDWARE_VALIDATION.md` | Vendor kernel branch `lbc-develop-6.1` (doc 03 also cites `rockchip-linux/kernel` `develop-6.1`) | Kernel `make kernelversion` = **5.10.160**, detached HEAD `ca43ccf` | Re-check every BSP citation (overlay timings, `regs` debugfs, kbase files, defconfig) against `kernel/` at `ca43ccf`, or state that the 6.1 branch was used and why |
| 01, 02, 07–12 | API/option details checked against libdrm **2.4.125** | Rootfs has libdrm **2.4.107** | Run `modetest -h` on the board; check that every libdrm call used in `src/` exists in 2.4.107 |
| 18 | `eglinfo` from Ubuntu 24.04 `mesa-utils` 9.0.0 | Rootfs is Ubuntu 20.04; no Mesa GL/EGL; EGL/GBM come from libmali | Find out whether `eglinfo` is installable on 20.04 (not verified); otherwise write a small EGL query program |
| 18 | GPU driver unknown, two candidate stacks | Tree has kbase (`bifrost/`, `g18p0-01eac0`, `CONFIG_MALI_CSF_SUPPORT=y`), **no** `panthor/`; rootfs has libmali g13p0 | Experiment 18 can now be narrowed to the BSP stack; confirm on the board (kbase version vs libmali version mismatch, if any, must be checked at runtime, not assumed) |
| README | Cross build on Ubuntu 24.04 multiarch (`libdrm-dev:arm64`, newer headers) | Target libdrm is 2.4.107; SDK ships its own gcc 10.3 toolchain | Decide whether to build against the board's libdrm (sysroot from `ubuntu/binary`) so binaries cannot depend on newer symbols |
| 03 | Panel = 7" ebf410173 overlay on DSI1/VP3, not confirmed | That overlay exists (`clock-frequency` 51668640, hfp/hsync/hbp 160/10/160, vfp/vsync/vbp 12/1/23); a DSI0/VP2 version exists too | Confirm which overlay is loaded (L0.3) |

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
* **L0.1** Fill the README Test Environment table (OS, kernel, tree/commit, libdrm, driver).
* **L0.2** Run `tools/collect-results.sh` (+ `--active`) and the manual Part B list; fill all `TODO(on-hardware)` in docs 01–18.
* **L0.3** Resolve the open questions in `HARDWARE_VALIDATION.md` (live DT vs documented timings, which DSI overlay is loaded, source of the VP0 "8K" claim). The SDK builds `rk3588-lubancat-generic.dts`; how that maps to the LubanCat 5 board DTS at boot is unknown and should be answered here.
* **L0.4** Resolve every row of "Conflicts between the SDK and the existing docs" (§1).
* *Exit criterion:* no `TODO(on-hardware)` remaining in docs 01–18, or each remaining one has a stated reason.

### Phase 5 — GPU → Display (already planned)
* **L19 GPU render → dma-buf → KMS.** On this SDK the stack is kbase + libmali (`x11-gbm` build, which ships `libgbm.so`); whether its EGL exposes the dma-buf import / native-fence extensions is still unknown until queried on the board. Path chosen from Experiment 18's EGL extension results (GBM scanout / dma-buf import / native-fence → `IN_FENCE_FD`). Baseline: `glReadPixels` copy; measure what zero-copy saves with the L17 timing tool.
* **L20 End-to-end trace.** One frame from GL call to scanout: ioctl boundary, fences, atomic commit, vblank event; produced with the L14 capture script.

### Phase 6 — Kernel-side (new)
* **L21 VKMS as a sandbox** `[board-optional]`. Two variants: (a) on a PC with a distro kernel; (b) on the board, building `drivers/gpu/drm/vkms` (present in the SDK tree, 1697 lines incl. `vkms_writeback.c`) as a module from `kernel/`, since `CONFIG_DRM_VKMS` is not set but `CONFIG_MODULES=y`. Whether it builds and loads against the 5.10 BSP tree is not verified. Run the existing atomic/plane tools against it and compare with the VOP2 output of L01/L16.
* **L22 Minimal KMS driver.** Out-of-tree module that registers one plane + CRTC + connector with a fake vblank timer, driven by `src/drm-atomic-demo`. Goal: the reader implements `atomic_check`/`atomic_flush` themselves. Target kernel: the SDK's 5.10.160 (`kernel/` at `ca43ccf`), built with the SDK gcc 10.3 toolchain and `CONFIG_MODULES=y`. Choose the helper API by reading that tree's `include/drm/` (upstream has been phasing out the simple-pipe helpers, so a 5.10 design will not port unchanged to mainline — note the differences for L32).
* **L23 Add a property.** Add a custom plane/CRTC property to L22 and read/write it from userspace; ties back to L10/L12.
* **L24 Panel driver exercise.** All LubanCat 5 DSI overlays use `compatible = "simple-panel-dsi"`, matched in `drivers/gpu/drm/panel/panel-simple.c` (5458 lines in the SDK tree); DSI host is `dw-mipi-dsi2-rockchip.c` / `dw-mipi-dsi-rockchip.c` + `bridge/synopsys/dw-mipi-dsi.c`. Trace how the overlay's `display-timings` and init sequence reach the panel and DSI host; then change one timing value in a copy of the overlay and observe via L03/L17.

### Phase 7 — Color, media and 2D (new; each needs a capability check first)
* **L25 Color pipeline.** Discover whether CRTC `GAMMA_LUT` / `CTM` and plane `COLOR_ENCODING`/`COLOR_RANGE` exist on this VOP2 driver, then use them. Extends L13's BT.601/709 observation.
* **L26 Video → plane.** Decoded NV12 buffer → dma-buf → overlay plane. The rootfs provides Rockchip MPP 1.5.0 (`librockchip-mpp1`, `-dev`, `rockchip-mpp-demos`) and `gstreamer1.0-rockchip1` 1.14-4; prebuilt only, no source in the SDK. Start from the MPP API and export the decoder's output buffer as a dma-buf (mechanism to be verified in the MPP headers).
* **L27 2D engine.** Rockchip RGA for scale/convert into a scanout buffer, using the rootfs `librga2` / `librga-dev` 2.2.0 (prebuilt; no source in the SDK). Which RGA kernel driver the 5.10 tree builds is not yet checked.

### Phase 8 — System level (new)
* **L28 Compositor behaviour.** Run a Wayland compositor on KMS; use L14 tracing and L12 plane-props to see which layers become hardware planes vs GPU composition.
* **L29 Direct scanout & latency.** Fullscreen client → direct scanout; compare flip latency to the L17 baseline.
* **L30 Power & robustness.** DPMS, runtime PM, hotplug during commit, restoring state on `SIGINT` (extends L16/L12 cleanup patterns).

### Phase 9 — Expert track (new, driven by the goal in §2)
* **L31 Source-reading drills.** For each earlier lab, trace one ioctl from `drm_ioctl()` to the driver callback in `kernel/` at `ca43ccf` (5.10.160; VOP2 is `rockchip_drm_vop2.c` 11801 lines + `rockchip_vop2_reg.c` 4006 + `rockchip_vop2_clk.c` 332); write a one-page call-chain note. Self-check: draw it from memory.
* **L32 BSP vs mainline diff study.** Compare the Rockchip BSP display driver and the mainline `rockchip_drm_vop2.c` for the same feature (e.g. plane `zpos`, AFBC, interrupt handling). Output: a table of differences with file/function references. The SDK has only the 5.10 BSP tree; a **mainline checkout must be added** outside the repo (RK3588 VOP2 data is upstream from v6.8, panthor from v6.10, per docs 12 and 18). The gap is large (5.10 BSP vs ≥ v6.8), so compare per feature, not per file.
* **L33 Fix or extend a driver.** Pick a real defect or missing feature found during Phase 0–8 (e.g. a property the VOP2 driver does not expose), implement it in the board's tree, and verify with the earlier lab tools.
* **L34 Upstream-style patch.** Prepare the L33 change as a mailing-list-quality patch (checkpatch, commit message, test evidence) against current mainline; submission itself is optional.
* **L35 Hardware reference.** Read the RK3588 TRM sections for VOP2 / DSI and annotate register-level behaviour against traces from L14. **Blocked:** no TRM or datasheet was found on the host (inventory §8). Until one is obtained, the register source is the driver's own tables (`rockchip_vop2_reg.c`) and `Documentation/devicetree/bindings/display/rockchip/` in the SDK tree.

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
| 5 | L21–L23 (kernel side, VKMS first so it is board-optional) | none for L21; kernel tree for L22/L23 |
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
1. Does the board run the SDK build (kernel `5.10.160-18` deb, Ubuntu 20.04.6 lite rootfs)? Compare `uname -a` / `/etc/os-release` on the board with the inventory.
2. Which DSI overlay is loaded (DSI1/VP3 7" ebf410173 is the candidate from doc 03)?
3. Is it acceptable to risk the display when loading test modules (L21b, L22)? `CONFIG_MODULES=y` in the SDK config, so it is technically possible.
4. Is an HDMI monitor available (L16, multi-display labs)?
5. Can a RK3588 TRM be obtained (L35)?
6. Where should the mainline kernel checkout for L32 live (outside the repo, e.g. next to `LUBANCAT_DIR`)?
