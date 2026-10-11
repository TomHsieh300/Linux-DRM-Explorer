# LubanCat-5IO device tree (running kernel source)

Read-only survey taken 2026-10-11 for study-plan item L0.3. No builds, no git
checkout, nothing changed on the board, no DTS/DTB copied into this repo.

- **Tree:** `/home/tom/LubanCat-6.1/kernel-6.1` (`LubanCat/kernel` @ `cb5bebb5c445`),
  the source of the running `6.1.99-rk3588 #11`
  (see [board-kernel-source.md](board-kernel-source.md)). All paths below are relative to
  `arch/arm64/boot/dts/rockchip/` in that tree unless prefixed `SDK 5.10:`.
- **Board:** serial console `/dev/ttyUSB0` @ 1500000. Every board command was md5-verified.
  The board was reflashed on 2026-10-11, so everything here was re-read and nothing
  is reused from `board-runtime.md`.

## Summary

- In the 6.1 tree, **`rk3588-lubancat-5io.dts` is a standalone board file** (1744 lines)
  with `model = "Embedfire LubanCat-5IO"`. **It does not include or derive from
  `rk3588-lubancat-5iob.dts`**; that file doesn't exist in the 6.1 tree.
  (In the SDK 5.10 tree it does: `#include "rk3588-lubancat-5iob.dts"`.)
- The board's `/boot/rk-kernel.dtb` → `dtb/rk3588-lubancat-5io.dtb`. It is the only DTB in
  `/boot/dtb` whose model is `Embedfire LubanCat-5IO`, and its md5
  (`2a60a3440be10364767ede61dc163457`) equals the tree's built `rk3588-lubancat-5io.dtb`.
  The running `/proc/device-tree/model` is `Embedfire LubanCat-5IO`. U-Boot's actual
  file choice isn't logged anywhere I read, so it's **unknown** for certain. The model
  string is only in that one DTB.
- **Display:** the base DTS enables **no** display controller. `dsi0/dsi1/hdmi0/hdmi1/dp0`
  stay `disabled` from the SoC dtsi (confirmed in the built DTB). The board enables
  HDMI0→VP0, DP0→VP1 and HDMI1→VP2 through U-Boot overlays listed in `uEnv.txt`.
  DSI0/DSI1 come only from the auto-loaded `*-dsi{0,1}-*-generic` overlays, which also
  add the panel EEPROM `eeprom@51` on I²C2 / I²C5. The base DTS sets `i2c2` and `i2c5`
  to `okay` and has no `eeprom@51` node.

## 1. `rk3588-lubancat-5io*` files

```console
$ wc -l rk3588-lubancat-5io*.dts*
    79 rk3588-lubancat-5io-cam0.dtsi
    79 rk3588-lubancat-5io-cam1.dtsi
    79 rk3588-lubancat-5io-cam2.dtsi
    79 rk3588-lubancat-5io-cam3.dtsi
    79 rk3588-lubancat-5io-cam4.dtsi
    79 rk3588-lubancat-5io-cam5.dtsi
  2269 rk3588-lubancat-5io-csi.dtsi
    37 rk3588-lubancat-5io-dp0-vp1.dtsi
   126 rk3588-lubancat-5io-dsi0-vp2-1080x1920-5.5inch-ebf410125.dtsi
   126 rk3588-lubancat-5io-dsi1-vp3-1080x1920-5.5inch-ebf410125.dtsi
  1744 rk3588-lubancat-5io.dts
    43 rk3588-lubancat-5io-hdmi0-vp0.dtsi
    45 rk3588-lubancat-5io-hdmi1-vp0.dtsi
    45 rk3588-lubancat-5io-hdmi1-vp2.dtsi
    29 rk3588-lubancat-5io-hdmirx.dtsi
    17 rk3588-lubancat-5ioi.dts
  4955 total
$ ls rk3588-lubancat-5iob*
ls: cannot access 'rk3588-lubancat-5iob*': No such file or directory
```

`rk3588-lubancat-5io.dts` is 1744 lines, so here are only its `#include` lines, model and compatible:

```console
$ grep -nE '#include|model|compatible = "embedfire|^\s*compatible = "rockchip,rk3588' rk3588-lubancat-5io.dts
9:#include <dt-bindings/usb/pd.h>
10:#include <dt-bindings/gpio/gpio.h>
11:#include <dt-bindings/pwm/pwm.h>
12:#include <dt-bindings/pinctrl/rockchip.h>
13://#include <dt-bindings/input/rk-input.h>
14:#include <dt-bindings/input/linux-event-codes.h>
15:#include <dt-bindings/display/drm_mipi_dsi.h>
16:#include <dt-bindings/display/rockchip_vop.h>
17:#include <dt-bindings/sensor-dev.h>
18:#include <dt-bindings/phy/jlsemi-dt-phy.h>
19:#include "rk3588.dtsi"
20:#include "rk3588-linux.dtsi"
23:// #include "rk3588-lubancat-5io-hdmi0-vp0.dtsi"
24:// #include "rk3588-lubancat-5io-hdmi1-vp0.dtsi"
27:// #include "rk3588-lubancat-5-dp0-vp1.dtsi"
30:// #include "rk3588-lubancat-5io-hdmi1-vp2.dtsi"
31:// #include "rk3588-lubancat-5io-dsi0-vp2-1080x1920-5.5inch-ebf410125.dtsi"
34:// #include "rk3588-lubancat-5io-dsi1-vp3-1080x1920-5.5inch-ebf410125.dtsi"
37:// #include "rk3588-lubancat-5io-hdmirx.dtsi"
40:#include "rk3588-lubancat-5io-csi.dtsi"
41:// #include "rk3588-lubancat-5io-cam0.dtsi"
42:// #include "rk3588-lubancat-5io-cam1.dtsi"
43:// #include "rk3588-lubancat-5io-cam2.dtsi"
44:// #include "rk3588-lubancat-5io-cam3.dtsi"
45:// #include "rk3588-lubancat-5io-cam4.dtsi"
46:// #include "rk3588-lubancat-5io-cam5.dtsi"
49:	model = "Embedfire LubanCat-5IO";
50:	compatible = "embedfire,rk3588-lubancat-5io", "rockchip,rk3588";
```

Every display `.dtsi` include is commented out. Only `rk3588-lubancat-5io-csi.dtsi` (camera) is active.

## 2. `#include` chain (quoted includes only)

Root-level (`/ { … }`, one tab) `model` / `compatible` lines per file. `grep -nP '^\t(model|compatible) = '`
also matched `rk3588-lubancat-5io.dts:1228` (`compatible = "rockchip,remotectl-pwm"`), but that
is inside a `&pwm…` override, not the root node, so it is left out.

```
rk3588-lubancat-5io.dts (1744 lines)          49: model = "Embedfire LubanCat-5IO";
                                              50: compatible = "embedfire,rk3588-lubancat-5io", "rockchip,rk3588";
├─ :19 rk3588.dtsi (922)                      (no root model/compatible)
│   ├─ :7 rk3588s.dtsi (6958)                 19: compatible = "rockchip,rk3588";
│   │   └─ :6958 rk3588s-pinctrl.dtsi (3481)  (none)
│   │       └─ :7 rk3588s-pinconf.dtsi (260)  (none)
│   └─ :8 rk3588-vccio3-pinctrl.dtsi (525)    (none)
│       └─ :7 rockchip-pinconf.dtsi (415)     (none)
├─ :20 rk3588-linux.dtsi (136)                (none)
└─ :40 rk3588-lubancat-5io-csi.dtsi (2269)    (none)
    └─ :8 rk3588-lubancat-cam.dtsi (74)       (none)
```

**Does `rk3588-lubancat-5io.dts` include or derive from `rk3588-lubancat-5iob.dts`? No.**
There is no `5iob` line in it (`grep -n '5iob' rk3588-lubancat-5io.dts` prints nothing), and
no `rk3588-lubancat-5iob*` file exists in this tree. It includes `rk3588.dtsi` directly
(line 19, shown above).

For comparison, **SDK 5.10** `rk3588-lubancat-5io.dts`, verbatim except blank lines (7 lines):

```
// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 * Copyright (c) 2025 EmbedFire <embedfire@embedfire.com>
 */
/dts-v1/;
#include "rk3588-lubancat-5iob.dts"
```

## 3. Built DTBs in the tree (read-only)

```console
$ fdtget <dtb> / model ; fdtget <dtb> / compatible ; md5sum <dtb>
rk3588-lubancat-5io.dtb:     model=Embedfire LubanCat-5IO     compatible=embedfire,rk3588-lubancat-5io rockchip,rk3588  md5=2a60a3440be10364767ede61dc163457  (2026-10-11 11:34)
rk3588-lubancat-5iob.dtb:    not present
rk3588-lubancat-5ioi.dtb:    model=Embedfire LubanCat-5IOI    compatible=rockchip,rk3588-lubancat-5ioi rockchip,rk3588  md5=8c8c4b65e13385aec0796512dcda113b
rk3588-lubancat-generic.dtb: model=Embedfire LubanCat-RK3588  compatible=rockchip,rk3588-lubancat rockchip,rk3588      md5=e946bd01a7c003e34002720c13e43996
```

## 4. Board

```console
# cat /proc/device-tree/model; echo
Embedfire LubanCat-5IO 
# tr '\0' '\n' < /proc/device-tree/compatible
embedfire,rk3588-lubancat-5io
rockchip,rk3588
# ls -l /boot/rk-kernel.dtb
lrwxrwxrwx 1 root root 27 Jun 18  2024 /boot/rk-kernel.dtb -> dtb/rk3588-lubancat-5io.dtb
# md5sum /boot/rk-kernel.dtb /boot/dtb/*.dtb
2a60a3440be10364767ede61dc163457  /boot/rk-kernel.dtb
0b48ff162ad073731d027c8bd7ed7954  /boot/dtb/rk3588-lubancat-5-v2.dtb
ed68b0adc9ad19f11529be1509cecc31  /boot/dtb/rk3588-lubancat-5.dtb
2a60a3440be10364767ede61dc163457  /boot/dtb/rk3588-lubancat-5io.dtb
8c8c4b65e13385aec0796512dcda113b  /boot/dtb/rk3588-lubancat-5ioi.dtb
e946bd01a7c003e34002720c13e43996  /boot/dtb/rk3588-lubancat-generic.dtb
340d946199920e0346628af6870567f6  /boot/dtb/rk3588s-lubancat-4-v1.dtb
a3b27d972d4e511e888a89203e0e33d1  /boot/dtb/rk3588s-lubancat-4.dtb
90c7d5e0403e9c7598291cb67d353d4d  /boot/dtb/rk3588s-lubancat-4io.dtb
# which fdtget
/usr/bin/fdtget
# fdtget /boot/rk-kernel.dtb / model
Embedfire LubanCat-5IO
# for f in /boot/dtb/*.dtb; do echo "$f: $(fdtget $f / model)"; done
/boot/dtb/rk3588-lubancat-5-v2.dtb: Embedfire LubanCat-5 V2
/boot/dtb/rk3588-lubancat-5.dtb: Embedfire LubanCat-5
/boot/dtb/rk3588-lubancat-5io.dtb: Embedfire LubanCat-5IO
/boot/dtb/rk3588-lubancat-5ioi.dtb: Embedfire LubanCat-5IOI
/boot/dtb/rk3588-lubancat-generic.dtb: Embedfire LubanCat-RK3588
/boot/dtb/rk3588s-lubancat-4-v1.dtb: Embedfire LubanCat-4 V1
/boot/dtb/rk3588s-lubancat-4.dtb: Embedfire LubanCat-4
/boot/dtb/rk3588s-lubancat-4io.dtb: Embedfire LubanCat-4IO
# grep -nvE '^(#|$)' /boot/uEnv/uEnv.txt ; readlink /boot/uEnv/uEnv.txt
1:uname_r=6.1.99-rk3588
2:initrd=initrd-6.1
3:size=0x1000000
4:cmdline="earlyprintk console=ttyFIQ0 console=tty1 consoleblank=0 loglevel=7 rootwait rw rootfstype=ext4"
6:enable_uboot_overlays=1
26:dtoverlay=/dtb/overlay/rk3588-lubancat-5io-hdmi0-vp0-overlay.dtbo
28:dtoverlay=/dtb/overlay/rk3588-lubancat-dp0-vp1-overlay.dtbo
30:dtoverlay=/dtb/overlay/rk3588-lubancat-5io-hdmi1-vp2-overlay.dtbo
99:enable_gsdt_auto_load=1
100:eeprom_addr=0x51
101:i2c_devs=2 5
102:gsdt_plugin0=/dtb/overlay/rk3588-lubancat-5io-dsi0-vp2-generic-overlay.dtbo
103:gsdt_plugin1=/dtb/overlay/rk3588-lubancat-5io-dsi1-vp3-generic-overlay.dtbo
uEnvLubanCat5IO.txt
```

`ls -1 /boot/dtb /boot/dtb/overlay | head -100` was also run (md5-verified). It shows the
9 DTBs above and the first 90 overlay `.dtbo` names (cam*, dsi*, edp, hdmi*, xl9535, …);
the list is cut off at 100 lines by `head`.

**Which board DTB has model "Embedfire LubanCat-5IO":** `/boot/dtb/rk3588-lubancat-5io.dtb`
(via the symlink `/boot/rk-kernel.dtb`). Its md5 is identical to the tree's built
`rk3588-lubancat-5io.dtb`, and `5ioi` / `generic` match their tree builds too. U-Boot's
actual choice: **unknown** from logs, though the running model string exists only in this DTB.

## 5. Display / I²C nodes in the 5io chain

SoC defaults (all `disabled`):

```
rk3588s.dtsi:4927: 	dsi0: dsi@fde20000 {          rk3588s.dtsi:4941: 		status = "disabled";
rk3588s.dtsi:4967: 	dsi1: dsi@fde30000 {          rk3588s.dtsi:4981: 		status = "disabled";
rk3588s.dtsi:5093: 	hdmi0: hdmi@fde80000 {        rk3588s.dtsi:5134: 		status = "disabled";
rk3588.dtsi:363: 	hdmi1: hdmi@fdea0000 {         rk3588.dtsi:404: 		status = "disabled";
rk3588s.dtsi:5024: 	dp0: dp@fde50000 {            rk3588s.dtsi:5038: 		status = "disabled";
rk3588s.dtsi:6075: 	i2c2: i2c@feaa0000 {          rk3588s.dtsi:6087: 		status = "disabled";
rk3588s.dtsi:6120: 	i2c5: i2c@fead0000 {          rk3588s.dtsi:6132: 		status = "disabled";
```

Board-chain overrides (`&node {` blocks and their `status` lines):

```
rk3588-lubancat-5io-csi.dtsi:466: &i2c2 {         467: 	status = "disabled";   (+9 camera child nodes, all "disabled")
rk3588-lubancat-5io-csi.dtsi:697: &i2c5 {         698: 	status = "disabled";   (+9 camera child nodes, all "disabled")
rk3588-lubancat-5io.dts:702: &i2c2 {              703: 	status = "okay";   pinctrl-0 = <&i2c2m4_xfer>;
rk3588-lubancat-5io.dts:708: &i2c5 {              711: 	status = "okay";   pinctrl-0 = <&i2c5m3_xfer>;
rk3588-lubancat-5io.dts:1658: &hdptxphy_hdmi0 {   1659: 	status = "okay";
rk3588-lubancat-5io.dts:1662: &hdptxphy_hdmi1 {   1663: 	status = "okay";
```

`5io-csi.dtsi` is included at line 40, before lines 702/708, so the final value is `okay`.
No `&dsi0`, `&dsi1`, `&hdmi0`, `&hdmi1` or `&dp0` override exists in the chain, and there is
**no `eeprom@51` node** (`grep -n eeprom@` over the chain finds nothing).

Built tree DTB confirms it:

```console
$ fdtget rk3588-lubancat-5io.dtb /<node> status
/dsi@fde20000 status=disabled
/dsi@fde30000 status=disabled
/hdmi@fde80000 status=disabled
/hdmi@fdea0000 status=disabled
/dp@fde50000 status=disabled
/i2c@feaa0000 status=okay
/i2c@fead0000 status=okay
```

Overlays (tree `overlay/` sources for the files `uEnv.txt` references):

| Overlay | Targets set to `okay` (file:line) |
|---|---|
| `rk3588-lubancat-5io-hdmi0-vp0-overlay.dts` | `&hdmi0` :11/16, `&route_hdmi0` :21, `&hdmi0_in_vp0` :30, `&hdptxphy_hdmi0` :38; `hdmi0_in_vp1/vp2` disabled :46/:54 |
| `rk3588-lubancat-dp0-vp1-overlay.dts` | `&dp0` :6/9, `&route_dp0` :14, `&dp0_in_vp1` :31; `dp0_in_vp0/vp2` disabled :23/:39 |
| `rk3588-lubancat-5io-hdmi1-vp2-overlay.dts` | `&hdmi1` :11/18, `&route_hdmi1` :23, `&hdmi1_in_vp2` :32, `&hdptxphy_hdmi1` :40; `hdmi1_in_vp0/vp1` disabled :48/:56 |
| `rk3588-lubancat-5io-dsi0-vp2-generic-overlay.dts` (gsdt_plugin0) | `&mipi_dcphy0` :11, `&route_dsi0` :19, `&dsi0_in_vp2` :28, `&dsi0` :37/40, `&i2c2` :96/99 with `gt911@5d` and `eeprom0: eeprom@51` (`atmel,24c64`, `reg = <0x51>`) :120–122 |
| `rk3588-lubancat-5io-dsi1-vp3-generic-overlay.dts` (gsdt_plugin1) | `&mipi_dcphy1` :11, `&route_dsi1` :19, `&dsi1_in_vp3` :28, `&dsi1` :36/39, `&i2c5` :95/98 with `eeprom1: eeprom@51` (`atmel,24c64`, `reg = <0x51>`) :119–121 |

When the gsdt plugins are actually applied by U-Boot: **unknown**. At the last runtime check,
`/sys/class/drm` had no DSI connector.

### SDK 5.10 comparison (brief)

- `SDK 5.10: rk3588-lubancat-5io.dts` is only `#include "rk3588-lubancat-5iob.dts"`. Its DTB
  reports `Embedfire LubanCat-5IOB` / `embedfire,rk3588-lubancat-5iob` (see
  [board-runtime.md](board-runtime.md)).
- `SDK 5.10: rk3588-lubancat-5iob.dts` (1756 lines) has the same include structure (`rk3588.dtsi`
  :19, `rk3588-linux.dtsi` :20, `rk3588-lubancat-5io-csi.dtsi` :40) and the same I²C setup:
  `&i2c2` :696 `status = "okay"` (:697), `&i2c5` :702 `status = "okay"` (:705). No
  display-controller override is in the base file there either.
- `diff` of the 6.1 `5io.dts` vs the 5.10 `5iob.dts` (headers skipped) shows 52 changed lines:
  model/compatible, several `regulator-always-on/boot-on` and regulator voltages (e.g. a
  `vdd_0v75_s0` NLDO_REG5 regulator only in 5.10), I²S `trcm` properties, and an extra
  `&rkvtunnel` node in 6.1. 5.10 uses `&hdptxphy_hdmi_clk0/1` where 6.1 has
  `&hdptxphy_hdmi0/1`. Whether the 5IOB and 5IO boards are electrically identical: **unknown**.
