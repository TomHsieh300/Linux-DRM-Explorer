# Linux-DRM-Explorer

A repository dedicated to exploring and mastering the Linux **DRM/KMS (Direct Rendering Manager / Kernel Mode Setting)** subsystem. This project documents technical insights, debugging methodologies, and hardware bring-up experiences on ARM-based embedded platforms.

---

## Technical Objectives
* **Deep Dive into KMS Pipeline**: Understanding the abstraction of Planes, CRTCs, Encoders, and Connectors.
* **Hardware Bring-up Methodology**: Documenting the process of driving display panels from low-level kernel configurations (DTS, Clocks, Timings).
* **Advanced Diagnostics**: Mastering system-level debugging using tools like `modetest`, `drm_info`, and `debugfs`.
* **Subsystem Comparison**: Leveraging experience from the Linux **ASoC** (ALSA SoC) subsystem to build a comparative mental model for multimedia drivers.

---

## Cross-Subsystem Mapping: ASoC vs. DRM
To accelerate the learning curve, I utilize a comparative approach by mapping display logic to audio logic:

| Dimension | ASoC (Audio) | DRM (Display) |
| :--- | :--- | :--- |
| **Hardware Enumeration** | `cat /proc/asound/cards` | `ls -l /dev/dri/` |
| **Path/Routing** | DAPM Widgets / Routes | KMS Pipeline (Plane -> CRTC -> Connector) |
| **Control & Testing** | `tinymix` / `speaker-test` | `modetest` / `kmscube` |
| **Low-level Snapshots** | `/sys/kernel/debug/asoc/` | `/sys/kernel/debug/dri/` |

---

## Initial Analysis: Rockchip VOP (RK3588)
My first experiment involves analyzing an active display pipeline on a **LubanCat 5 (RK3588)** platform.

### Snapshot Highlights (`modetest -p` Analysis)
* **Platform**: Rockchip VOP2 (Video Output Processor)
* **Active Mode**: 1024x600 @ 60.00 Hz
* **Pipeline Status**: 
    * **CRTC ID 208** is currently active and bound to **Framebuffer 262**.
    * **Primary Plane (ID 176)** is feeding the CRTC.
* **Key Observations**:
    * The hardware exhibits a multi-window architecture with support for `Cluster`, `Esmart`, and `Smart` planes.
    * Support for **AFBC** (Arm Frame Buffer Compression) is detected on specific planes (e.g., Plane 81), which is critical for bandwidth optimization in high-resolution scenarios.

---

## Learning Roadmap
- [x] Master `modetest` for resource enumeration and basic modesetting.
- [ ] Analyze Atomic State snapshots in `/sys/kernel/debug/dri/0/state`.
- [ ] Explore DSI/HDMI bridge driver initialization and DTS bindings.
- [ ] Implement automated health-check scripts for display pipelines.
