# Study Plan: From Experiment Notes to a DRM Textbook and Lab Course

Status: **design proposal**. Nothing below is implemented yet.

> **Scope note.** This plan is based on the contents of this repository only (README, `docs/01`–`18`, `HARDWARE_VALIDATION.md`, `src/`, `tools/`, `Makefile`, CI, git history).
> The LubanCat working directory (`/home/tom/LubanCat`) was **not accessible** when this was written, so nothing here has been checked against its kernel tree, rootfs, images or notes. Items that depend on it are marked `[needs LubanCat tree]`.

---

## 1. Current state (facts from the repo)

| Area | State |
| :--- | :--- |
| Experiments | 18 docs (`docs/01`–`18`), 9 C programs in `src/` (~9,700 lines), 2 scripts in `tools/` |
| Hardware validation | Experiments 12–17 are compile-only; README says their Results are placeholders. Every doc `01`–`18` still contains at least one `TODO(on-hardware)` (`grep -c` shows 1–10 per file). The README Test Environment table is all `TODO` (OS image, kernel, kernel tree/commit, driver, libdrm). |
| Planned, unwritten | Experiments 19 (GPU render → dma-buf → KMS) and 20 (tracing userspace → kernel), per README Phase 5 |
| Build / CI | `make` builds every `src/*.c`; GitHub Actions builds native and aarch64 with `-Werror`. No automated tests that run anything |
| Structure | Chronological lab notes, each with Objective → Environment → Background → Steps → Results → Analysis → Takeaways → References |
| Target | Single board: LubanCat 5 (RK3588, VOP2), one 1024x600 DSI panel; kernel is probably the LubanCat BSP 6.1 tree (to be confirmed, see `HARDWARE_VALIDATION.md`) |

### What is strong
* Source-cited kernel explanations and an explicit "not measured unless it was" rule.
* Good coverage of the userspace KMS API (legacy → atomic), planes, formats, sync, tracing.

### Gaps
1. **It is a lab diary, not a textbook.** Concepts are introduced where the experiment needs them; there is no concept-first path, glossary, or index, and prerequisites between docs are implicit.
2. **Unvalidated.** The biggest risk to credibility is that most Results sections are empty. New content built on unmeasured results compounds the problem.
3. **Userspace-only.** Everything is a libdrm client. There is no lab where the reader *writes or modifies kernel code*, though doc 15 reads the driver.
4. **Single platform.** No mainline vs BSP comparison is carried through the labs; no way to practise without the board.
5. **No self-assessment.** No exercises, expected outputs, or pass/fail criteria per experiment.
6. **Missing topics** (see §4): GPU path (19/20 planned), color pipeline, video/camera buffers, compositor-level use of KMS, power, panel/backlight, DRM from the kernel side.

---

## 2. Design principles

1. **Validate before extending.** Phase 0 below is a hard gate for anything that depends on measured numbers.
2. **Two artefacts, one source of truth.** The *textbook* explains concepts and cites kernel source; the *labs* are hands-on and link into the textbook. A concept is written once, in the textbook.
3. **Every lab is checkable.** Each lab states observable pass criteria (a printed value, a file, a tracepoint) so the reader can tell whether it worked without a reference answer.
4. **No fabricated results.** Keep the existing rule: unmeasured → `TODO(on-hardware)`; a claim about the kernel cites file and function, and says which tree/tag it was checked on.
5. **Board-optional where possible.** Where a concept does not need Rockchip hardware, provide a variant that runs on any Linux machine (VKMS, `drm_info`, tracepoints) so the course is usable without the board.

---

## 3. Proposed textbook structure

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

## 4. Proposed lab series

Numbering continues the existing sequence. Labs 19–20 are already promised by the README.

### Phase 0 — Validate what exists (gate)
* **L0.1** Fill the README Test Environment table (OS, kernel, tree/commit, libdrm, driver).
* **L0.2** Run `tools/collect-results.sh` (+ `--active`) and the manual Part B list; fill all `TODO(on-hardware)` in docs 01–18.
* **L0.3** Resolve the open questions in `HARDWARE_VALIDATION.md` (BSP vs mainline kernel, live DT vs documented timings, source of the VP0 "8K" claim).
* *Exit criterion:* no `TODO(on-hardware)` remaining in docs 01–18, or each remaining one has a stated reason.

### Phase 5 — GPU → Display (already planned)
* **L19 GPU render → dma-buf → KMS.** Path chosen from Experiment 18's EGL extension results (GBM scanout / dma-buf import / native-fence → `IN_FENCE_FD`). Baseline: `glReadPixels` copy; measure what zero-copy saves with the L17 timing tool.
* **L20 End-to-end trace.** One frame from GL call to scanout: ioctl boundary, fences, atomic commit, vblank event; produced with the L14 capture script.

### Phase 6 — Kernel-side (new)
* **L21 VKMS as a sandbox** `[board-optional]`. Load the mainline `vkms` module on a PC, run the existing atomic/plane tools against it, compare object model with the VOP2 output of L01/L16.
* **L22 Minimal KMS driver.** Out-of-tree module that registers one plane + CRTC + connector with a fake vblank timer, driven by `src/drm-atomic-demo`. Goal: the reader implements `atomic_check`/`atomic_flush` themselves. Which helper API to build on must be chosen against the kernel version actually used `[needs LubanCat tree]` (some simple-pipe helpers are being phased out upstream; verify before writing).
* **L23 Add a property.** Add a custom plane/CRTC property to L22 and read/write it from userspace; ties back to L10/L12.
* **L24 Panel driver exercise.** Trace the panel's init sequence and timing path from the DT `[needs LubanCat tree]`; modify one timing value in an overlay and observe via L03/L17.

### Phase 7 — Color, media and 2D (new; each needs a capability check first)
* **L25 Color pipeline.** Discover whether CRTC `GAMMA_LUT` / `CTM` and plane `COLOR_ENCODING`/`COLOR_RANGE` exist on this VOP2 driver, then use them. Extends L13's BT.601/709 observation.
* **L26 Video → plane.** Decoded NV12 buffer from the vendor/V4L2 stack → dma-buf → overlay plane. *Which decoder stack the image provides is unknown* `[needs LubanCat tree]`.
* **L27 2D engine.** Rockchip RGA for scale/convert into a scanout buffer; exists as both a BSP driver and a mainline V4L2 driver, availability on the image to be verified.

### Phase 8 — System level (new)
* **L28 Compositor behaviour.** Run a Wayland compositor on KMS; use L14 tracing and L12 plane-props to see which layers become hardware planes vs GPU composition.
* **L29 Direct scanout & latency.** Fullscreen client → direct scanout; compare flip latency to the L17 baseline.
* **L30 Power & robustness.** DPMS, runtime PM, hotplug during commit, restoring state on `SIGINT` (extends L16/L12 cleanup patterns).

### Lab template (add to every lab)
```
Goal · Prerequisites (textbook chapter, earlier labs) · Setup
Task (what to build / run) · Expected observations
Pass criteria (checkable) · Self-check questions (3–5)
Stretch goals · Results (measured, with kernel/tree stated) · References
```

---

## 5. Delivery roadmap

| Step | Deliverable | Depends on |
| :--- | :--- | :--- |
| 1 | Phase 0 results merged; README environment table filled | Board access |
| 2 | `docs/textbook/` skeleton: chapter stubs + glossary; each links to existing experiment | none |
| 3 | Retrofit labs 01–18 with *Pass criteria* and *Self-check questions* | Step 1 |
| 4 | L19, L20 | Step 1 (Exp 18 results) |
| 5 | L21–L23 (kernel side, VKMS first so it is board-optional) | none for L21; kernel tree for L22/L23 |
| 6 | Textbook chapters 1–11 written from existing docs | Step 2 |
| 7 | Phases 7–8, chapters 12–17 | Steps 1, 4 |
| 8 | CI: add smoke tests that need no hardware (e.g. run tools against VKMS in a container if the CI runner allows loading it — **unverified**) | Step 5 |

---

## 6. Open questions for the author

1. What is in `/home/tom/LubanCat` (kernel tree? rootfs? images? notes)? Which kernel branch and commit does the board actually run?
2. Is the audience the author alone, a team (BSP/IC design, per the README), or public readers? This decides tone and how much prerequisite knowledge to assume.
3. Is a kernel-module lab (L22) acceptable, i.e. can the board build and load out-of-tree modules, and is it safe to risk the display on it?
4. Is an HDMI monitor available (needed for L16 hotplug and any multi-display lab)?
5. Which media sources are available for L26 (hardware-decoded video stack on the image)?
6. Preferred language for the textbook: the repo is in English; the user conversation was in Chinese.
