# Experiment 01: Hardware Resource Inventory & KMS Mapping

## 1. Objective
Enumerate the internal display resources of the Rockchip RK3588 (VOP2) and establish a mental model by comparing it with the **ASoC** (ALSA SoC) subsystem.

## 2. Cross-Subsystem Mapping (ASoC vs. DRM)
| Dimension | ASoC (Audio) | DRM (Display) | Description |
| :--- | :--- | :--- | :--- |
| **Hardware Interface** | `cat /proc/asound/cards` | `modetest -c` | Verifying physical connectivity. |
| **Path/Routing** | DAPM Widgets / Routes | KMS Pipeline | Plane -> CRTC -> Connector path. |
| **Data Scanout** | DMA Engine | CRTC (VOP) | Moving data from memory to hardware. |

## 3. RK3588 VOP2 Resource Analysis
Based on `modetest -p` output, the system features **4 CRTCs**, corresponding to the 4 Video Ports (VP) in the VOP2 architecture:
* **VP0 (ID 88)**: Supports up to 8K output.
* **VP1 (ID 128)** / **VP2 (ID 168)**: Support up to 4K output.
* **VP3 (ID 208)**: Supports 2K output, currently driving the DSI panel.

## 4. Key Takeaways
* **Object ID**: IDs in DRM are globally unique handles assigned by the kernel.
* **Possible CRTCs Mask**: A bitmask used to identify which Planes can be attached to which CRTCs¡Xa critical constraint during multi-display bring-up.
