# Experiment 02: Modetest Mastery & Practical Troubleshooting

## 1. Core Command Syntax
* **Set Mode (-s)**: `modetest -M rockchip -s <Connector_ID>@<CRTC_ID>:<Mode>`
* **Write Property (-w)**: `modetest -M rockchip -w <Object_ID>:<Prop>:<Value>`
* **Set Plane (-P)**: `modetest -M rockchip -P <Plane_ID>@<CRTC_ID>:<w>x<h>+<x>+<y>`

## 2. Troubleshooting: "Object ID Not Found"
While attempting to modify the `DPMS` property, an `Object not found` error occurred.
* **Root Cause**: Mistaking the `Encoder ID` for the `Connector ID`.
* **Solution**: Properties like DPMS are attached to the **Connector**. Always use the ID from the leftmost column in the `modetest -c` list.

## 3. Engineering Insights
`modetest` acts as both a register configurator and a "Graphic Producer." Its built-in color bar pattern is the fastest way to verify the integrity of the display link (from Framebuffer to physical screen) without a full GUI stack.
