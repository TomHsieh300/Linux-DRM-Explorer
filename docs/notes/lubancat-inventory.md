# LubanCat RK3588 SDK — local inventory

Read-only survey of `/home/tom/LubanCat` on the host, taken 2026-10-10 for
`docs/STUDY_PLAN.md`. No SDK files were copied. Every value below came from a
command run on the host; anything not checked is marked **unknown**. Values
marked *derived* are arithmetic on verified numbers.

## 1. Top-level layout

SDK = `LubanCat_Linux_rk3588_SDK_20260421` (repo manifest `rk3588_linux_release.xml`).

| Entry | Type | `du -sh` |
|---|---|---|
| `.repo/` | repo metadata | 2.3G |
| `LubanCat_Linux_rk3588_SDK_20260421.tgz` | SDK tarball | 2.3G |
| `kernel/` | git | 7.8G (includes build output) |
| `ubuntu/` | git (rootfs builder + built rootfs) | 3.3G |
| `rockdev/` | flash images | 1.7G |
| `debian/` | git (`LubanCat/debian11`) | 1.1G |
| `prebuilts/` | toolchain | 699M |
| `tools/` | git | 502M |
| `u-boot/` | git | 297M |
| `device/` | contains git `device/rockchip` | 242M |
| `linux-image-5.10.160-dbg_5.10.160-18_arm64.deb` | | 184M |
| `rkbin/` | git | 55M |
| `linux-image-5.10.160_5.10.160-18_arm64.deb` | | 20M |
| `linux-headers-5.10.160_5.10.160-18_arm64.deb` | | 7.6M |
| `linux-libc-dev_5.10.160-18_arm64.deb` | | 1.2M |
| `linux-5.10.160_5.10.160-18_arm64.{buildinfo,changes}` | | 8K / 4K |
| `.rootfs/` | symlink `rootfs.ext4 -> ../ubuntu/ubuntu-lite-rootfs.img` | 4K |
| `build.sh`, `mkfirmware.sh`, `rkflash.sh` | symlinks to `device/rockchip/common/` | 0 |

## 2. Git repositories

Every repo is on a **detached HEAD** (repo-tool checkout, no branch).

| Path | Remote | HEAD commit | Date | Subject |
|---|---|---|---|---|
| `kernel` | `https://github.com/LubanCat/kernel` | `ca43ccfcb96ca8d650ab5f26bd92b5f3a22d09b6` | 2026-07-29 | version rk3588 update KBUILD_BUILD_VERSION=16 |
| `u-boot` | `https://github.com/LubanCat/u-boot` | `9b8922d299b2da3de8d96d1693ab7d2eeade5eb0` | 2024-04-08 | configs rk3588 添加i2c和grepenv工具编译 |
| `ubuntu` | `https://github.com/LubanCat/ubuntu` | `c40d7e89787b993680ce1b94cd41df01d3a3b8a6` | 2026-07-29 | base 添加apt-proxy代理功能 |
| `debian` | `https://github.com/LubanCat/debian11` | `b967bdd422849993bea42c1594d9344d822e0e65` | 2026-07-29 | ubuntu-build-service 安装auto-apt-proxy和bash-completion |
| `device/rockchip` | `https://github.com/LubanCat/device_rockchip` | `aa605529daab64df339d9592f347a5d656d47f8a` | 2024-12-05 | rk356x/rk3588 修改默认通用设备树名称 |
| `rkbin` | `https://github.com/LubanCat/rkbin` | `f358db70f6b7983a777026c5798fc7055e0a2153` | 2024-10-13 | rk3588: ddr: modify the lp4 frequency to 1848Mhz |
| `tools` | `https://github.com/LubanCat/tools` | `210be81d659a6bc4e7a648744ae77837f394be0f` | 2024-01-02 | linux: Linux_Pack_Firmware: add support rk3588 and rk356x ubuntu |

There is one kernel tree, `kernel/`. `make kernelversion` reports **5.10.160**
(a Rockchip BSP tree, not mainline). The only local modification is `.version`.

## 3. Display / GPU stack in `kernel/`

| Component | Present | Files (lines) |
|---|---|---|
| VOP2 display controller | yes | `drivers/gpu/drm/rockchip/rockchip_drm_vop2.c` (11801), `rockchip_vop2_reg.c` (4006), `rockchip_vop2_clk.c` (332); 16139 total |
| MIPI DSI | yes | `drivers/gpu/drm/rockchip/dw-mipi-dsi2-rockchip.c` (1957), `dw-mipi-dsi-rockchip.c` (1488), `drivers/gpu/drm/bridge/synopsys/dw-mipi-dsi.c` (1391) |
| Panel driver referenced by board DTS | yes | every LubanCat 5 DSI overlay uses `compatible = "simple-panel-dsi"`, which is matched in `drivers/gpu/drm/panel/panel-simple.c` (5458 lines, match at line 5162). The touch controller is `goodix,gt911` |
| `drivers/gpu/arm/bifrost` (Mali kbase) | yes | 159977 lines of `*.[ch]`; `MALI_RELEASE_NAME ?= "g18p0-01eac0"`. Siblings `mali400/` and `midgard/` are also present |
| `drivers/gpu/drm/panthor` | **no** | directory does not exist |
| `drivers/gpu/drm/vkms` | yes | 8 source files, 1697 lines total (includes `vkms_writeback.c`) |

GPU DT node in `rk3588s.dtsi`: `compatible = "arm,mali-bifrost"`. The
kbase driver is built with `CONFIG_MALI_CSF_SUPPORT=y`, which is the CSF path
used by the Valhall G610.

## 4. Board config

- The active board config is `device/rockchip/.BoardConfig.mk -> rk3588/BoardConfig-LubanCat-3588-ubuntu-lite.mk`
  - `RK_KERNEL_DEFCONFIG=lubancat_linux_rk3588_defconfig` (no fragment)
  - `RK_KERNEL_DTS=rk3588-lubancat-generic`
  - `RK_UBOOT_DEFCONFIG=rk3588`
  - `RK_ROOTFS_SYSTEM=ubuntu`, `RK_ROOTFS_TARGET=lite`, `RK_ROOTFS_TYPE=ext4`
- The other LubanCat configs (`ubuntu-gnome/xfce`, `debian-lite/gnome/xfce`) use the same defconfig and DTS.
- Values below are from `kernel/.config`, which is the config the kernel was actually built with (`CC_VERSION_TEXT` = the gcc 10.3 toolchain in section 9):

| Option | `.config` | In defconfig? |
|---|---|---|
| `CONFIG_DRM_ROCKCHIP` | `y` | `y` |
| `CONFIG_ROCKCHIP_VOP2` (BSP name; no `ROCKCHIP_DRM_VOP2` exists) | `y` | not listed (default/selected) |
| `CONFIG_ROCKCHIP_DW_MIPI_DSI` | `y` | — |
| `CONFIG_DRM_PANEL_SIMPLE` | `y` | — |
| `CONFIG_MALI_BIFROST` | `y` (built-in) | `y` |
| `CONFIG_MALI_CSF_SUPPORT` | `y` | — |
| `CONFIG_DRM_PANTHOR` | absent (driver not in tree) | — |
| `CONFIG_DRM_VKMS` | **not set** | not listed |
| `CONFIG_DMABUF_HEAPS` | `y` | `y` |
| `CONFIG_DMABUF_HEAPS_SYSTEM` | `y` | |
| `CONFIG_DMABUF_HEAPS_CMA` | `y` | |
| `CONFIG_DMABUF_HEAPS_PAGE_POOL` | `y` | |
| `CONFIG_DMABUF_HEAPS_DEFERRED_FREE` | `y` | |
| `CONFIG_DMABUF_HEAPS_SRAM` | not set | |
| `CONFIG_DMABUF_HEAPS_ROCKCHIP` | not set | |
| `CONFIG_MODULES` | `y` | |

`ROCKCHIP_VOP2` is defined at `drivers/gpu/drm/rockchip/Kconfig:55`. The
Makefile builds `rockchip_drm_vop2.o rockchip_vop2_reg.o rockchip_post_csc.o`
into `rockchipdrm` when it is set.

## 5. Device tree (`arch/arm64/boot/dts/rockchip/`)

- The DTS the build uses is `rk3588-lubancat-generic.dts`
  (`model = "Embedfire LubanCat-RK3588"`; includes `rk3588s.dtsi`, `rk3588-linux.dtsi`).
  How the generic DTB maps to the exact board at boot is **unknown**.
- The LubanCat 5 board DTS is `rk3588-lubancat-5.dts` (includes `rk3588.dtsi`, `rk3588-linux.dtsi`).
  - Variants in the Makefile: `-5-v2`, `-5io`, `-5ioi`, `-5iob`, `-5iof`, `-5iobi`, `-5iob-test`.
  - In `rk3588-lubancat-5.dts` every display `#include` is commented out:
    `hdmi0-vp0`, `hdmi1-vp0`, `dp0-vp1`, `dp1-vp2-edp-port`, `hdmi1-vp2`,
    `dsi0-vp2-1080x1920-5.5inch-ebf410125`, `dsi1-vp3-1080x1920-5.5inch-ebf410125`.
    Only `rk3588-lubancat-5-csi.dtsi` is active. Displays are expected to come from overlays.
- In-tree DSI `.dtsi` files: `rk3588-lubancat-5-dsi0-vp2-1080x1920-5.5inch-ebf410125.dtsi`,
  `rk3588-lubancat-5-dsi1-vp3-1080x1920-5.5inch-ebf410125.dtsi`
- DSI overlays for LubanCat 5 in `overlay/` (each one exists as `.dts` and `.dtbo`). The DSI0 versions are:
  - `rk3588-lubancat-5-dsi0-vp2-1080x1920-5.5inch-ebf410125-overlay`
  - `rk3588-lubancat-5-dsi0-vp2-1024x600-7inch-ebf410173-overlay`
  - `rk3588-lubancat-5-dsi0-vp2-800x1280-10.1inch-ebf410177-overlay`
  - `rk3588-lubancat-5-dsi0-vp2-800x1280-10.1inch-s8001280b1060b-overlay`
  - `rk3588-lubancat-5-dsi0-vp2-generic-overlay` (no timing node)
  - The same five exist as `dsi1-vp3-*`, and for the `-5-v2`, `-5io` and `-5io-test` variants.
- Every panel uses `simple-panel-dsi` with `dsi,lanes = <4>`. The 5.5" `.dtsi` also has
  `dsi,format = MIPI_DSI_FMT_RGB888` and
  `dsi,flags = VIDEO | VIDEO_BURST | LPM | EOT_PACKET`.

Panel timings (`display-timings` node, from the `dsi0-vp2` overlays; refresh is *derived*):

| Panel | clock-frequency | hactive | hfp | hsync | hbp | vactive | vfp | vsync | vbp | refresh |
|---|---|---|---|---|---|---|---|---|---|---|
| 5.5" ebf410125 | 131376000 | 1080 | 10 | 10 | 20 | 1920 | 10 | 5 | 20 | 60.00 Hz |
| 7" ebf410173 | 51668640 | 1024 | 160 | 10 | 160 | 600 | 12 | 1 | 23 | 60.00 Hz |
| 10.1" ebf410177 | 67000000 | 800 | 12 | 24 | 24 | 1280 | 7 | 2 | 9 | 60.02 Hz |
| 10.1" s8001280b1060b | 69801600 | 800 | 40 | 20 | 20 | 1280 | 30 | 4 | 8 | 60.00 Hz |

Which panel is physically attached to the user's board is **unknown** from the SDK alone.

## 6. Images / rootfs

| File | Size (bytes) | Notes |
|---|---|---|
| `rockdev/update.img` | 1800331850 | full flash image |
| `rockdev/boot.img -> kernel/extboot.img` | 134217728 | |
| `rockdev/uboot.img -> u-boot/uboot.img` | 8388608 | |
| `rockdev/MiniLoaderAll.bin -> u-boot/rk3588_spl_loader_v1.17.113.bin` | 483776 | |
| `rockdev/parameter.txt -> device/rockchip/rk3588/parameter-ubuntu-fit.txt` | | partition table |
| `rockdev/rootfs.{img,ext4} -> ubuntu/ubuntu-lite-rootfs.img` | 1656750080 | ext4, UUID `aee60886-7521-416f-b511-5e999969b856` |
| `ubuntu/ubuntu-base-lite-arm64-20261005.tar.gz` | 320448176 | |
| `ubuntu/ubuntu-base-20.04.5-base-arm64.tar.gz` | 26257800 | |

Distro: **Ubuntu 20.04.6 LTS (Focal Fossa)**, read from
`ubuntu/binary/etc/os-release`. `ubuntu/binary/` is the unpacked rootfs
staging directory, owned by root but readable, so no mount was needed.

## 7. Userspace (installed in `ubuntu/binary`, versions from `var/lib/dpkg/status`)

| Area | Installed | Version |
|---|---|---|
| libmali | `libmali-valhall-g610-g13p0-x11-gbm` (+dbgsym) | 1.9-1 |
| | files `libmali-valhall-g610-g13p0-x11-gbm.so`, `libmali.so.1.9.0`, `libmali_hook.so.1.9.0`, `mali/{libEGL,libGLESv1_CM,libGLESv2,libgbm}.so*`, `mali/libMaliOpenCL.so*`, `pkgconfig/mali.pc` | |
| GPU firmware | `usr/lib/firmware/mali_csffw.bin` | |
| Mesa | only `mesa-va-drivers` and `mesa-vdpau-drivers`; no Mesa GL/EGL/GBM packages; `usr/lib/aarch64-linux-gnu/dri/` is empty | 21.2.6-0ubuntu0.1~20.04.2 |
| libdrm | `libdrm2`, `-common`, `-amdgpu1`, `-nouveau2`, `-radeon1` | 2.4.107-8ubuntu1~20.04.2 |
| MPP | `librockchip-mpp1`, `librockchip-mpp-dev`, `rockchip-mpp-demos` | 1.5.0-1 |
| RGA | `librga2`, `librga-dev` (`librga.so.2.1.0` on disk) | 2.2.0-1 |
| GStreamer | `gstreamer1.0-rockchip1` (`libgstrockchipmpp.so`, `libgstrkximage.so`) | 1.14-4 |
| Other | `camera-engine-rkaiq`, `rktoolkit` | |

There are prebuilt `.deb` packages in `ubuntu/packages/arm64/` (116 total), including:
- `libmali/`: the valhall-g610, bifrost-g31/g52, midgard-t86x and utgard-450 builds, each in x11-gbm and wayland-gbm flavours, version 1.9-1
- `mpp/` and `gst-rkmpp/` (MPP 1.5.0-1, gstreamer1.0-rockchip1 1.14-4, librockchip-vpu0)
- `rga2/` (2.2.0-1) and `rga/` (librga2 2.1.0-1)
- `libdrm/libkms1_2.4.104-1`
- `libdrm-cursor/` 1.4.1-1
- `xserver/` (xserver-xorg-core 1.20.11)
- `gstreamer/` 1.18.5-1

`debian/packages/{arm64,armhf}/{libmali,mpp}` hold the equivalents for Debian.

**Source trees** for mpp, librga, gstreamer-rockchip, Mesa or libmali: none
found at depth ≤ 4 outside `.repo`. There are only prebuilt packages.

## 8. Documentation

No RK3588 TRM or datasheet PDF was found: no PDF name matching `rk3588|trm|datasheet`
under the SDK, `~/Documents`, `~/Downloads` or `~/Desktop`. The Rockchip PDFs
that do exist are tool manuals, e.g.:
- `tools/linux/Linux_DDR_Bandwidth_Tool/Rockchip_Introduction_DDR_Bandwidth_Tool_CN.pdf`
- `tools/linux/Linux_SecurityAVB/Rockchip_User_Manual_Linux_AVB_EN.pdf`
- `tools/windows/RKDevInfoWriteTool/Rockchip_User_Guide_RKDevInfoWriteTool_CN.pdf`
- `tools/windows/Rockchip_USB_SQ_Tool/Rockchip_Introduction_USB_SQ_Tool_CN_V1.4.0.pdf`

The kernel tree's own `Documentation/gpu/` and
`Documentation/devicetree/bindings/display/rockchip/` are present.

## 9. Toolchain

- Path: `prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc`
- `--version`: `aarch64-none-linux-gnu-gcc (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 10.3.1 20210621`

## Implications for the study plan

- The GPU kernel driver is vendor **kbase** (`bifrost/`, CSF, g18p0), used with the
  closed **libmali g13p0**. Panthor/Panfrost labs would need a mainline kernel and Mesa,
  and neither is in this SDK.
- VKMS source is in the tree but disabled. Enabling it means setting `CONFIG_DRM_VKMS=m` and rebuilding.
- The display path for the DSI labs is VOP2 VP2/VP3 → `dw-mipi-dsi2` → `panel-simple` (`simple-panel-dsi`), with timings set in the DT overlays above.
