# Experiment 02: Modetest Mastery & Practical Troubleshooting

## 1. Objective
Use `modetest` (the KMS test tool shipped with libdrm) to set modes, attach planes and write properties by hand, before writing any C code.

## 2. Environment
See the [Test Environment](../README.md#test-environment) section. The option syntax below was checked against `modetest -h` from **libdrm 2.4.125**; older libdrm releases may lack some options, so run `modetest -h` on the board to confirm.

## 3. Core Command Syntax
| Option | Syntax (from `modetest -h`) | Purpose |
| :--- | :--- | :--- |
| `-s` | `<connector_id>[,<connector_id>][@<crtc_id>]:mode[@<format>]` | Set a mode and show a test pattern |
| `-P` | `<plane_id>@<crtc_id>:<w>x<h>[+<x>+<y>][*<scale>][@<format>]` | Put a test pattern on a plane |
| `-w` | `<obj_id>:<prop_name>:<value>` | Write a property on any KMS object |
| `-v` | — | Page flip test synchronized to VBlank (used in [Experiment 06](./06_VBlank_and_PageFlip.md)) |
| `-a` | — | Use the atomic API instead of the legacy ioctls |
| `-M` / `-D` | `-M <module>` / `-D <device>` | Select the driver by name / device node |

`mode` can be given as `<hdisp>x<vdisp>[-<vrefresh>]`, as a full custom timing, or as `#<mode index>` (index into the connector's mode list printed by `modetest -c`).

Examples for this board (IDs from [Experiment 01](./01_Hardware_Inventory.md); yours may differ):
```bash
# Light up the DSI panel on VP3 with the default test pattern
sudo modetest -M rockchip -s <connector_id>@208:1024x600

# Same, using the atomic API
sudo modetest -M rockchip -a -s <connector_id>@208:1024x600

# Turn the panel off through the connector's DPMS property (see Experiment 04)
sudo modetest -M rockchip -w <connector_id>:DPMS:3
```

## 4. Troubleshooting: "Object ID Not Found"
While attempting to modify the `DPMS` property, an `Object not found` error occurred.
* **Root Cause**: Mistaking the `Encoder ID` for the `Connector ID`.
* **Solution**: Properties like DPMS are attached to the **Connector**. Always use the ID from the leftmost column in the `modetest -c` list.

### Checklist for other common failures
| Symptom | Where to look |
| :--- | :--- |
| Property write rejected | Is the property attached to this object type? `modetest -c` / `-p` prints properties under each object. |
| Mode set fails for a plane/CRTC pair | Check the plane's `possible_crtcs` mask ([Experiment 01](./01_Hardware_Inventory.md)). |
| Change reverts as soon as `modetest` exits | Last-close restore by the in-kernel console client ([Experiment 04](./04_DRM_Master_and_Race_Condition.md)). |
| Atomic `-a` commit rejected with `EINVAL` | Enable `drm.debug` to see which check failed ([Experiment 14](./14_Debugging_and_Tracing.md)). |

## 5. Results
> `TODO(on-hardware)`: record the exact commands used on the LubanCat 5 (with real connector/plane IDs), the `modetest` version (`dpkg -l libdrm-tests`), and a photo of the test pattern on the panel.

## 6. Engineering Insights
`modetest` acts as both a register configurator and a "Graphic Producer." Its built-in color bar pattern is the fastest way to verify the integrity of the display link (from Framebuffer to physical screen) without a full GUI stack.

## 7. Key Takeaways
* Properties belong to specific object types (connector, CRTC, plane); the ID must match the object that owns the property.
* `modetest -h` on the target is the authority for option syntax—it changes between libdrm releases.

## 8. References
* `modetest -h` output, libdrm 2.4.125 (`tests/modetest/modetest.c` in the libdrm source tree)
