# LubanCat board — running kernel config

The running kernel's config was read from `/proc/config.gz` on the LubanCat-5IO
(kernel `6.1.99-rk3588`) over the serial console (`/dev/ttyUSB0`, 1500000 8N1, root).
Read-only: nothing on the board was changed and no modules were loaded. The full
config was **not** committed; it is saved on the host only.
The SDK side is `/home/tom/LubanCat/kernel/.config` (5.10.160, a **different kernel**),
used here only for comparison.

## Command log

```console
# ls -l /proc/config.gz
-r--r--r-- 1 root root 48772 Jun 18 04:44 /proc/config.gz

# zcat /proc/config.gz | md5sum ; zcat /proc/config.gz | wc -l
931026800151912fc38c37fce658e5a9  -
8460
```

`CONFIG_IKCONFIG_PROC` is therefore enabled. The md5 is the same as the board's
`/boot/config-6.1.99-rk3588` (`931026800151912fc38c37fce658e5a9`, read earlier, see
[board-runtime.md](board-runtime.md)).

## Transfer to host

- Method: `zcat /proc/config.gz | sed -n 'A,Bp'` in 40-line chunks (212 chunks). Each
  chunk was accepted only when its md5 on the host matched `… | md5sum` run on the board.
  Lines 7521–7560 could not be taken as plain text without corruption, so they were
  transferred as `… | base64 -w0` and decoded, with the same md5 check. 154 chunk
  retries in total.
- Host copy: **`/home/tom/LubanCat-board/config-6.1.99-rk3588`**
- Verification on host: `md5sum` → `931026800151912fc38c37fce658e5a9`, `wc -l` → `8460`.
  **This matches the board.**

## Filtered options

The pattern as given (`^CONFIG_(…|DRM$|…)`) can't match `# CONFIG_X is not set` lines,
and `DRM$` can't match `CONFIG_DRM=y`. So it was run as `^(# )?CONFIG_(…)(=| is not set)`,
with `DRM` in place of `DRM$`. The trailing `(=| is not set)` makes each alternative
match the whole option name; for example `LOCALVERSION` does not also match
`LOCALVERSION_AUTO`. Exact command (the same pattern was applied to the SDK `.config`):

```console
$ grep -E '^(# )?CONFIG_(LOCALVERSION|IKCONFIG|MODULES|MODVERSIONS|MODULE_SIG|DRM|DRM_.*|ROCKCHIP_.*VOP.*|ROCKCHIP_.*DSI.*|ROCKCHIP_DRM_.*|DRM_ROCKCHIP.*|DRM_PANEL.*|DRM_VKMS|DRM_PANTHOR|DRM_PANFROST|MALI_.*|DMABUF.*|DMA_SHARED_BUFFER|SYNC_FILE|SW_SYNC|DEBUG_FS|DYNAMIC_DEBUG|FTRACE|FUNCTION_TRACER|TRACING|KPROBES|ROCKCHIP_RKNPU.*|ROCKCHIP_RGA.*|ROCKCHIP_MPP.*|VIDEO_ROCKCHIP.*)(=| is not set)' /home/tom/LubanCat-board/config-6.1.99-rk3588
CONFIG_LOCALVERSION="-rk3588"
CONFIG_IKCONFIG=y
# CONFIG_KPROBES is not set
CONFIG_MODULES=y
# CONFIG_MODVERSIONS is not set
# CONFIG_MODULE_SIG is not set
# CONFIG_MALI_BASE_MODULES is not set
CONFIG_DMA_SHARED_BUFFER=y
CONFIG_VIDEO_ROCKCHIP_CIF=y
CONFIG_VIDEO_ROCKCHIP_ISP=y
CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V30=y
CONFIG_VIDEO_ROCKCHIP_ISPP=y
# CONFIG_VIDEO_ROCKCHIP_ISPP_FEC is not set
CONFIG_VIDEO_ROCKCHIP_ISPP_VERSION_V20=y
CONFIG_VIDEO_ROCKCHIP_HDMIRX_CLASS=y
CONFIG_VIDEO_ROCKCHIP_HDMIRX=y
CONFIG_VIDEO_ROCKCHIP_RGA=y
# CONFIG_VIDEO_ROCKCHIP_ISP1 is not set
# CONFIG_VIDEO_ROCKCHIP_PREISP is not set
CONFIG_DRM=y
CONFIG_DRM_EDID=y
CONFIG_DRM_IGNORE_IOTCL_PERMIT=y
CONFIG_DRM_MIPI_DSI=y
# CONFIG_DRM_DEBUG_MM is not set
CONFIG_DRM_KMS_HELPER=y
# CONFIG_DRM_DEBUG_DP_MST_TOPOLOGY_REFS is not set
# CONFIG_DRM_DEBUG_MODESET_LOCK is not set
CONFIG_DRM_FBDEV_EMULATION=y
CONFIG_DRM_FBDEV_OVERALLOC=100
# CONFIG_DRM_FBDEV_LEAK_PHYS_SMEM is not set
CONFIG_DRM_LOAD_EDID_FIRMWARE=y
CONFIG_DRM_DP=y
CONFIG_DRM_DISPLAY_HELPER=y
CONFIG_DRM_DISPLAY_DP_HELPER=y
CONFIG_DRM_DISPLAY_HDCP_HELPER=y
CONFIG_DRM_DISPLAY_HDMI_HELPER=y
CONFIG_DRM_DP_AUX_CHARDEV=y
# CONFIG_DRM_DP_CEC is not set
CONFIG_DRM_GEM_DMA_HELPER=y
# CONFIG_DRM_I2C_CH7006 is not set
# CONFIG_DRM_I2C_SIL164 is not set
# CONFIG_DRM_I2C_NXP_TDA998X is not set
# CONFIG_DRM_I2C_NXP_TDA9950 is not set
# CONFIG_DRM_HDLCD is not set
# CONFIG_DRM_MALI_DISPLAY is not set
# CONFIG_DRM_KOMEDA is not set
# CONFIG_DRM_RADEON is not set
# CONFIG_DRM_AMDGPU is not set
# CONFIG_DRM_NOUVEAU is not set
# CONFIG_DRM_VGEM is not set
# CONFIG_DRM_VKMS is not set
CONFIG_DRM_ROCKCHIP=y
CONFIG_ROCKCHIP_DRM_DEBUG=y
# CONFIG_ROCKCHIP_DRM_DIRECT_SHOW is not set
CONFIG_ROCKCHIP_VOP2=y
CONFIG_ROCKCHIP_DRM_TVE=y
CONFIG_ROCKCHIP_DW_MIPI_DSI=y
CONFIG_ROCKCHIP_DW_MIPI_DSI2=y
# CONFIG_DRM_ROCKCHIP_VVOP is not set
# CONFIG_DRM_VMWGFX is not set
# CONFIG_DRM_UDL is not set
# CONFIG_DRM_AST is not set
# CONFIG_DRM_MGAG200 is not set
# CONFIG_DRM_RCAR_DW_HDMI is not set
# CONFIG_DRM_RCAR_USE_LVDS is not set
# CONFIG_DRM_RCAR_USE_MIPI_DSI is not set
# CONFIG_DRM_QXL is not set
# CONFIG_DRM_VIRTIO_GPU is not set
CONFIG_DRM_PANEL=y
# CONFIG_DRM_PANEL_ABT_Y030XX067A is not set
# CONFIG_DRM_PANEL_ARM_VERSATILE is not set
# CONFIG_DRM_PANEL_ASUS_Z00T_TM5P5_NT35596 is not set
# CONFIG_DRM_PANEL_BOE_BF060Y8M_AJ0 is not set
# CONFIG_DRM_PANEL_BOE_HIMAX8279D is not set
# CONFIG_DRM_PANEL_BOE_TV101WUM_NL6 is not set
# CONFIG_DRM_PANEL_DSI_CM is not set
# CONFIG_DRM_PANEL_LVDS is not set
CONFIG_DRM_PANEL_SIMPLE=y
# CONFIG_DRM_PANEL_EDP is not set
# CONFIG_DRM_PANEL_EBBG_FT8719 is not set
# CONFIG_DRM_PANEL_ELIDA_KD35T133 is not set
# CONFIG_DRM_PANEL_FEIXIN_K101_IM2BA02 is not set
# CONFIG_DRM_PANEL_FEIYANG_FY07024DI26A30D is not set
# CONFIG_DRM_PANEL_ILITEK_IL9322 is not set
# CONFIG_DRM_PANEL_ILITEK_ILI9341 is not set
# CONFIG_DRM_PANEL_ILITEK_ILI9881C is not set
# CONFIG_DRM_PANEL_INNOLUX_EJ030NA is not set
# CONFIG_DRM_PANEL_INNOLUX_P079ZCA is not set
# CONFIG_DRM_PANEL_JDI_LT070ME05000 is not set
# CONFIG_DRM_PANEL_JDI_R63452 is not set
# CONFIG_DRM_PANEL_KHADAS_TS050 is not set
# CONFIG_DRM_PANEL_KINGDISPLAY_KD097D04 is not set
# CONFIG_DRM_PANEL_LEADTEK_LTK050H3146W is not set
# CONFIG_DRM_PANEL_LEADTEK_LTK500HD1829 is not set
# CONFIG_DRM_PANEL_SAMSUNG_LD9040 is not set
# CONFIG_DRM_PANEL_LG_LB035Q02 is not set
# CONFIG_DRM_PANEL_LG_LG4573 is not set
# CONFIG_DRM_PANEL_NEC_NL8048HL11 is not set
# CONFIG_DRM_PANEL_NEWVISION_NV3052C is not set
# CONFIG_DRM_PANEL_NOVATEK_NT35510 is not set
# CONFIG_DRM_PANEL_NOVATEK_NT35560 is not set
# CONFIG_DRM_PANEL_NOVATEK_NT35950 is not set
# CONFIG_DRM_PANEL_NOVATEK_NT36672A is not set
# CONFIG_DRM_PANEL_NOVATEK_NT39016 is not set
# CONFIG_DRM_PANEL_MANTIX_MLAF057WE51 is not set
# CONFIG_DRM_PANEL_MAXIM_MAX96752F is not set
# CONFIG_DRM_PANEL_MAXIM_MAX96772 is not set
# CONFIG_DRM_PANEL_OLIMEX_LCD_OLINUXINO is not set
# CONFIG_DRM_PANEL_ORISETECH_OTM8009A is not set
# CONFIG_DRM_PANEL_OSD_OSD101T2587_53TS is not set
# CONFIG_DRM_PANEL_PANASONIC_VVX10F034N00 is not set
# CONFIG_DRM_PANEL_RASPBERRYPI_TOUCHSCREEN is not set
# CONFIG_DRM_PANEL_RAYDIUM_RM67191 is not set
# CONFIG_DRM_PANEL_RAYDIUM_RM68200 is not set
# CONFIG_DRM_PANEL_RONBO_RB070D30 is not set
# CONFIG_DRM_PANEL_SAMSUNG_ATNA33XC20 is not set
# CONFIG_DRM_PANEL_SAMSUNG_DB7430 is not set
# CONFIG_DRM_PANEL_SAMSUNG_S6D16D0 is not set
# CONFIG_DRM_PANEL_SAMSUNG_S6D27A1 is not set
# CONFIG_DRM_PANEL_SAMSUNG_S6E3HA2 is not set
# CONFIG_DRM_PANEL_SAMSUNG_S6E63J0X03 is not set
# CONFIG_DRM_PANEL_SAMSUNG_S6E63M0 is not set
# CONFIG_DRM_PANEL_SAMSUNG_S6E88A0_AMS452EF01 is not set
# CONFIG_DRM_PANEL_SAMSUNG_S6E8AA0 is not set
# CONFIG_DRM_PANEL_SAMSUNG_SOFEF00 is not set
# CONFIG_DRM_PANEL_SEIKO_43WVF1G is not set
# CONFIG_DRM_PANEL_SHARP_LQ101R1SX01 is not set
# CONFIG_DRM_PANEL_SHARP_LS037V7DW01 is not set
# CONFIG_DRM_PANEL_SHARP_LS043T1LE01 is not set
# CONFIG_DRM_PANEL_SHARP_LS060T1SX01 is not set
# CONFIG_DRM_PANEL_SITRONIX_ST7701 is not set
# CONFIG_DRM_PANEL_SITRONIX_ST7703 is not set
# CONFIG_DRM_PANEL_SITRONIX_ST7789V is not set
# CONFIG_DRM_PANEL_SONY_ACX565AKM is not set
# CONFIG_DRM_PANEL_SONY_TULIP_TRULY_NT35521 is not set
# CONFIG_DRM_PANEL_TDO_TL070WSH30 is not set
# CONFIG_DRM_PANEL_TPO_TD028TTEC1 is not set
# CONFIG_DRM_PANEL_TPO_TD043MTEA1 is not set
# CONFIG_DRM_PANEL_TPO_TPG110 is not set
# CONFIG_DRM_PANEL_TRULY_NT35597_WQXGA is not set
# CONFIG_DRM_PANEL_VISIONOX_RM69299 is not set
# CONFIG_DRM_PANEL_WIDECHIPS_WS2401 is not set
# CONFIG_DRM_PANEL_XINPENG_XPP055C272 is not set
CONFIG_DRM_BRIDGE=y
CONFIG_DRM_PANEL_BRIDGE=y
# CONFIG_DRM_CDNS_DSI is not set
# CONFIG_DRM_CHIPONE_ICN6211 is not set
# CONFIG_DRM_CHRONTEL_CH7033 is not set
CONFIG_DRM_DISPLAY_CONNECTOR=y
# CONFIG_DRM_ITE_IT6161 is not set
# CONFIG_DRM_ITE_IT6505 is not set
# CONFIG_DRM_LONTIUM_LT8912B is not set
# CONFIG_DRM_LONTIUM_LT9211 is not set
# CONFIG_DRM_LONTIUM_LT9611 is not set
# CONFIG_DRM_LONTIUM_LT9611UXC is not set
# CONFIG_DRM_ITE_IT66121 is not set
# CONFIG_DRM_LVDS_CODEC is not set
# CONFIG_DRM_MAXIM_MAX96745 is not set
# CONFIG_DRM_MAXIM_MAX96755F is not set
# CONFIG_DRM_MEGACHIPS_STDPXXXX_GE_B850V3_FW is not set
# CONFIG_DRM_NWL_MIPI_DSI is not set
# CONFIG_DRM_NXP_PTN3460 is not set
# CONFIG_DRM_PARADE_PS8622 is not set
# CONFIG_DRM_PARADE_PS8640 is not set
# CONFIG_DRM_RK1000_TVE is not set
# CONFIG_DRM_ROHM_BU18XL82 is not set
# CONFIG_DRM_SIL_SII8620 is not set
CONFIG_DRM_SII902X=y
# CONFIG_DRM_SII9234 is not set
# CONFIG_DRM_SIMPLE_BRIDGE is not set
# CONFIG_DRM_THINE_THC63LVD1024 is not set
# CONFIG_DRM_TOSHIBA_TC358762 is not set
# CONFIG_DRM_TOSHIBA_TC358764 is not set
# CONFIG_DRM_TOSHIBA_TC358767 is not set
# CONFIG_DRM_TOSHIBA_TC358768 is not set
# CONFIG_DRM_TOSHIBA_TC358775 is not set
# CONFIG_DRM_TI_DLPC3433 is not set
# CONFIG_DRM_TI_TFP410 is not set
# CONFIG_DRM_TI_SN65DSI83 is not set
# CONFIG_DRM_TI_SN65DSI86 is not set
# CONFIG_DRM_TI_TPD12S015 is not set
# CONFIG_DRM_ANALOGIX_ANX6345 is not set
# CONFIG_DRM_ANALOGIX_ANX78XX is not set
CONFIG_DRM_ANALOGIX_DP=y
# CONFIG_DRM_ANALOGIX_ANX7625 is not set
# CONFIG_DRM_I2C_ADV7511 is not set
# CONFIG_DRM_CDNS_MHDP8546 is not set
CONFIG_DRM_DW_HDMI=y
# CONFIG_DRM_DW_HDMI_AHB_AUDIO is not set
CONFIG_DRM_DW_HDMI_I2S_AUDIO=y
# CONFIG_DRM_DW_HDMI_GP_AUDIO is not set
CONFIG_DRM_DW_HDMI_CEC=y
CONFIG_DRM_DW_MIPI_DSI=y
# CONFIG_DRM_ETNAVIV is not set
# CONFIG_DRM_HISI_HIBMC is not set
# CONFIG_DRM_HISI_KIRIN is not set
# CONFIG_DRM_LOGICVC is not set
# CONFIG_DRM_ARCPGU is not set
# CONFIG_DRM_BOCHS is not set
# CONFIG_DRM_CIRRUS_QEMU is not set
# CONFIG_DRM_GM12U320 is not set
# CONFIG_DRM_PANEL_MIPI_DBI is not set
# CONFIG_DRM_SIMPLEDRM is not set
# CONFIG_DRM_PL111 is not set
# CONFIG_DRM_LIMA is not set
# CONFIG_DRM_PANFROST is not set
# CONFIG_DRM_TIDSS is not set
# CONFIG_DRM_GUD is not set
# CONFIG_DRM_SSD130X is not set
# CONFIG_DRM_LEGACY is not set
CONFIG_DRM_PANEL_ORIENTATION_QUIRKS=y
CONFIG_DRM_NOMODESET=y
CONFIG_MALI_DMA_BUF_MAP_ON_ATTACH=y
CONFIG_MALI_SHARED_INTERRUPTS=y
# CONFIG_MALI_PMU_PARALLEL_POWER_UP is not set
CONFIG_MALI_DT=y
CONFIG_MALI_DEVFREQ=y
# CONFIG_MALI_QUIET is not set
CONFIG_MALI_MIDGARD=y
# CONFIG_MALI_GATOR_SUPPORT is not set
# CONFIG_MALI_MIDGARD_ENABLE_TRACE is not set
# CONFIG_MALI_DMA_FENCE is not set
CONFIG_MALI_EXPERT=y
# CONFIG_MALI_CORESTACK is not set
# CONFIG_MALI_PRFCNT_SET_SECONDARY is not set
# CONFIG_MALI_PLATFORM_FAKE is not set
# CONFIG_MALI_PLATFORM_DEVICETREE is not set
CONFIG_MALI_PLATFORM_THIRDPARTY=y
CONFIG_MALI_PLATFORM_THIRDPARTY_NAME="rk"
CONFIG_MALI_DEBUG=y
CONFIG_MALI_FENCE_DEBUG=y
# CONFIG_MALI_NO_MALI is not set
# CONFIG_MALI_TRACE_TIMELINE is not set
# CONFIG_MALI_SYSTEM_TRACE is not set
# CONFIG_MALI_GPU_MMU_AARCH64 is not set
CONFIG_MALI_PWRSOFT_765=y
# CONFIG_MALI_KUTF is not set
CONFIG_MALI_BIFROST=y
CONFIG_MALI_PLATFORM_NAME="rk"
CONFIG_MALI_REAL_HW=y
# CONFIG_MALI_BIFROST_NO_MALI is not set
# CONFIG_MALI_IS_FPGA is not set
CONFIG_MALI_CSF_SUPPORT=y
CONFIG_MALI_BIFROST_DEVFREQ=y
CONFIG_MALI_BIFROST_GATOR_SUPPORT=y
CONFIG_MALI_BIFROST_ENABLE_TRACE=y
# CONFIG_MALI_ARBITER_SUPPORT is not set
# CONFIG_MALI_DMA_BUF_MAP_ON_DEMAND is not set
# CONFIG_MALI_DMA_BUF_LEGACY_COMPAT is not set
# CONFIG_MALI_CORESIGHT is not set
CONFIG_MALI_BIFROST_EXPERT=y
CONFIG_MALI_BIFROST_DEBUG=y
CONFIG_MALI_BIFROST_FENCE_DEBUG=y
CONFIG_MALI_BIFROST_SYSTEM_TRACE=y
CONFIG_MALI_PRFCNT_SET_PRIMARY=y
# CONFIG_MALI_BIFROST_PRFCNT_SET_SECONDARY is not set
# CONFIG_MALI_PRFCNT_SET_TERTIARY is not set
# CONFIG_MALI_JOB_DUMP is not set
# CONFIG_MALI_HW_ERRATA_1485982_NOT_AFFECTED is not set
# CONFIG_MALI_HW_ERRATA_1485982_USE_CLOCK_ALTERNATIVE is not set
# CONFIG_MALI_ARBITRATION is not set
CONFIG_MALI_TRACE_POWER_GPU_WORK_PERIOD=y
CONFIG_MALI_CSF_INCLUDE_FW=y
# CONFIG_ROCKCHIP_RGA is not set
CONFIG_ROCKCHIP_RGA_ASYNC=y
CONFIG_ROCKCHIP_RGA_PROC_FS=y
CONFIG_ROCKCHIP_RGA_DEBUG_FS=y
CONFIG_ROCKCHIP_RGA_DEBUGGER=y
CONFIG_ROCKCHIP_MPP_SERVICE=y
CONFIG_ROCKCHIP_MPP_PROC_FS=y
CONFIG_ROCKCHIP_MPP_RKVDEC=y
CONFIG_ROCKCHIP_MPP_RKVDEC2=y
CONFIG_ROCKCHIP_MPP_RKVENC=y
CONFIG_ROCKCHIP_MPP_RKVENC2=y
CONFIG_ROCKCHIP_MPP_VDPU1=y
CONFIG_ROCKCHIP_MPP_VEPU1=y
CONFIG_ROCKCHIP_MPP_VDPU2=y
CONFIG_ROCKCHIP_MPP_VEPU2=y
CONFIG_ROCKCHIP_MPP_IEP2=y
CONFIG_ROCKCHIP_MPP_JPGDEC=y
CONFIG_ROCKCHIP_MPP_JPGENC=y
CONFIG_ROCKCHIP_MPP_AV1DEC=y
# CONFIG_ROCKCHIP_MPP_VDPP is not set
CONFIG_DMABUF_CACHE=y
# CONFIG_DMABUF_PARTIAL is not set
CONFIG_SYNC_FILE=y
CONFIG_SW_SYNC=y
# CONFIG_DMABUF_MOVE_NOTIFY is not set
# CONFIG_DMABUF_DEBUG is not set
# CONFIG_DMABUF_SELFTESTS is not set
CONFIG_DMABUF_HEAPS=y
CONFIG_DMABUF_SYSFS_STATS=y
CONFIG_DMABUF_HEAPS_SYSTEM=y
CONFIG_DMABUF_HEAPS_CMA=y
# CONFIG_DMABUF_HEAPS_SRAM is not set
# CONFIG_DMABUF_HEAPS_ROCKCHIP is not set
CONFIG_ROCKCHIP_RKNPU=y
CONFIG_ROCKCHIP_RKNPU_DEBUG_FS=y
# CONFIG_ROCKCHIP_RKNPU_PROC_FS is not set
# CONFIG_ROCKCHIP_RKNPU_FENCE is not set
# CONFIG_ROCKCHIP_RKNPU_SRAM is not set
CONFIG_ROCKCHIP_RKNPU_DRM_GEM=y
CONFIG_DYNAMIC_DEBUG=y
CONFIG_DEBUG_FS=y
CONFIG_TRACING=y
CONFIG_FTRACE=y
CONFIG_FUNCTION_TRACER=y
```

307 lines (104 set, 203 "is not set"). The same grep on the SDK `.config` gives 267 lines.

## Comparison with SDK `kernel/.config`

Summary: **same 246**, **differs 5**, **board only 56**, **SDK only 16**.
"board only" / "SDK only" means the symbol isn't in the other file at all, usually because it doesn't exist in that kernel version.

### Key options

| Option | Board (6.1.99) | SDK (5.10.160) | Result |
|---|---|---|---|
| `DRM` | `y` | `y` | same |
| `DRM_ROCKCHIP` | `y` | `y` | same |
| `ROCKCHIP_VOP2` | `y` | `y` | same |
| `ROCKCHIP_DW_MIPI_DSI` | `y` | `y` | same |
| `ROCKCHIP_DW_MIPI_DSI2` | `y` | — | board only |
| `DRM_PANEL_SIMPLE` | `y` | `y` | same |
| `DRM_VKMS` | `is not set` | `is not set` | same |
| `DRM_PANTHOR` | — | — | absent in both (no such symbol in either config; no panthor driver in either tree) |
| `DRM_PANFROST` | `is not set` | `is not set` | same |
| `MALI_BIFROST` | `y` | `y` | same |
| `MALI_CSF_SUPPORT` | `y` | `y` | same |
| `DMABUF_HEAPS` | `y` | `y` | same |
| `DMABUF_HEAPS_SYSTEM` | `y` | `y` | same |
| `DMABUF_HEAPS_CMA` | `y` | `y` | same |
| `SYNC_FILE` | `y` | `y` | same |
| `SW_SYNC` | `y` | `y` | same |
| `DEBUG_FS` | `y` | `y` | same |
| `DYNAMIC_DEBUG` | `y` | `y` | same |
| `FTRACE` | `y` | `y` | same |
| `FUNCTION_TRACER` | `y` | `y` | same |
| `KPROBES` | `is not set` | `is not set` | same |
| `MODULES` | `y` | `y` | same |
| `MODULE_SIG` | `is not set` | `is not set` | same |
| `MODVERSIONS` | `is not set` | `is not set` | same |
| `IKCONFIG` | `y` | `y` | same |
| `IKCONFIG_PROC` | `y` | `y` | same (not matched by the pattern, which matches `IKCONFIG` exactly; values from `grep` of the full files) |
| `LOCALVERSION` | `"-rk3588"` | `""` | differs |
| `ROCKCHIP_RKNPU` | `y` | `y` | same |
| `ROCKCHIP_RGA` | `is not set` | `is not set` | same |
| `ROCKCHIP_MPP_SERVICE` | `y` | `y` | same |
### differs (5)

| Option | Board | SDK |
|---|---|---|
| `DMABUF_PARTIAL` | `is not set` | `y` |
| `LOCALVERSION` | `"-rk3588"` | `""` |
| `ROCKCHIP_DRM_DEBUG` | `y` | `is not set` |
| `ROCKCHIP_DRM_TVE` | `y` | `is not set` |
| `ROCKCHIP_RGA_PROC_FS` | `y` | `is not set` |

### board only (56)

| Option | Board | SDK |
|---|---|---|
| `DMABUF_DEBUG` | `is not set` | — |
| `DRM_ANALOGIX_ANX7625` | `is not set` | — |
| `DRM_CHIPONE_ICN6211` | `is not set` | — |
| `DRM_DEBUG_MODESET_LOCK` | `is not set` | — |
| `DRM_DISPLAY_DP_HELPER` | `y` | — |
| `DRM_DISPLAY_HDCP_HELPER` | `y` | — |
| `DRM_DISPLAY_HDMI_HELPER` | `y` | — |
| `DRM_DISPLAY_HELPER` | `y` | — |
| `DRM_DW_HDMI_GP_AUDIO` | `is not set` | — |
| `DRM_GEM_DMA_HELPER` | `y` | — |
| `DRM_GUD` | `is not set` | — |
| `DRM_ITE_IT6505` | `is not set` | — |
| `DRM_ITE_IT66121` | `is not set` | — |
| `DRM_LOGICVC` | `is not set` | — |
| `DRM_LONTIUM_LT8912B` | `is not set` | — |
| `DRM_LONTIUM_LT9211` | `is not set` | — |
| `DRM_LONTIUM_LT9611UXC` | `is not set` | — |
| `DRM_NOMODESET` | `y` | — |
| `DRM_PANEL_ABT_Y030XX067A` | `is not set` | — |
| `DRM_PANEL_BOE_BF060Y8M_AJ0` | `is not set` | — |
| `DRM_PANEL_DSI_CM` | `is not set` | — |
| `DRM_PANEL_EBBG_FT8719` | `is not set` | — |
| `DRM_PANEL_EDP` | `is not set` | — |
| `DRM_PANEL_ILITEK_ILI9341` | `is not set` | — |
| `DRM_PANEL_INNOLUX_EJ030NA` | `is not set` | — |
| `DRM_PANEL_JDI_R63452` | `is not set` | — |
| `DRM_PANEL_KHADAS_TS050` | `is not set` | — |
| `DRM_PANEL_MAXIM_MAX96772` | `is not set` | — |
| `DRM_PANEL_MIPI_DBI` | `is not set` | — |
| `DRM_PANEL_NEWVISION_NV3052C` | `is not set` | — |
| `DRM_PANEL_NOVATEK_NT35560` | `is not set` | — |
| `DRM_PANEL_NOVATEK_NT35950` | `is not set` | — |
| `DRM_PANEL_NOVATEK_NT36672A` | `is not set` | — |
| `DRM_PANEL_SAMSUNG_ATNA33XC20` | `is not set` | — |
| `DRM_PANEL_SAMSUNG_DB7430` | `is not set` | — |
| `DRM_PANEL_SAMSUNG_S6D27A1` | `is not set` | — |
| `DRM_PANEL_SAMSUNG_SOFEF00` | `is not set` | — |
| `DRM_PANEL_SHARP_LS060T1SX01` | `is not set` | — |
| `DRM_PANEL_SONY_TULIP_TRULY_NT35521` | `is not set` | — |
| `DRM_PANEL_TDO_TL070WSH30` | `is not set` | — |
| `DRM_PANEL_WIDECHIPS_WS2401` | `is not set` | — |
| `DRM_RCAR_USE_LVDS` | `is not set` | — |
| `DRM_RCAR_USE_MIPI_DSI` | `is not set` | — |
| `DRM_SIMPLEDRM` | `is not set` | — |
| `DRM_SSD130X` | `is not set` | — |
| `DRM_TI_DLPC3433` | `is not set` | — |
| `DRM_TI_SN65DSI83` | `is not set` | — |
| `DRM_VIRTIO_GPU` | `is not set` | — |
| `DRM_VMWGFX` | `is not set` | — |
| `MALI_ARBITER_SUPPORT` | `is not set` | — |
| `MALI_CSF_INCLUDE_FW` | `y` | — |
| `MALI_IS_FPGA` | `is not set` | — |
| `MALI_TRACE_POWER_GPU_WORK_PERIOD` | `y` | — |
| `ROCKCHIP_DW_MIPI_DSI2` | `y` | — |
| `ROCKCHIP_MPP_JPGENC` | `y` | — |
| `VIDEO_ROCKCHIP_ISP1` | `is not set` | — |

### SDK only (16)

| Option | Board | SDK |
|---|---|---|
| `DMABUF_HEAPS_DEFERRED_FREE` | — | `y` |
| `DMABUF_HEAPS_PAGE_POOL` | — | `y` |
| `DRM_DEBUG_SELFTEST` | — | `is not set` |
| `DRM_GEM_CMA_HELPER` | — | `y` |
| `DRM_KMS_FB_HELPER` | — | `y` |
| `DRM_MXSFB` | — | `is not set` |
| `DRM_PANEL_SONY_ACX424AKP` | — | `is not set` |
| `DRM_RCAR_LVDS` | — | `is not set` |
| `MALI_MEMORY_FULLY_BACKED` | — | `is not set` |
| `MALI_PRFCNT_SET_SELECT_VIA_DEBUG_FS` | — | `is not set` |
| `ROCKCHIP_VOP` | — | `y` |
| `VIDEO_ROCKCHIP_ISPP_VERSION_V10` | — | `is not set` |
| `VIDEO_ROCKCHIP_ISP_VERSION_V1X` | — | `y` |
| `VIDEO_ROCKCHIP_ISP_VERSION_V20` | — | `is not set` |
| `VIDEO_ROCKCHIP_ISP_VERSION_V21` | — | `y` |
| `VIDEO_ROCKCHIP_ISP_VERSION_V32` | — | `is not set` |

### same (246)

<details><summary>show</summary>

| Option | Board | SDK |
|---|---|---|
| `DEBUG_FS` | `y` | `y` |
| `DMABUF_CACHE` | `y` | `y` |
| `DMABUF_HEAPS` | `y` | `y` |
| `DMABUF_HEAPS_CMA` | `y` | `y` |
| `DMABUF_HEAPS_ROCKCHIP` | `is not set` | `is not set` |
| `DMABUF_HEAPS_SRAM` | `is not set` | `is not set` |
| `DMABUF_HEAPS_SYSTEM` | `y` | `y` |
| `DMABUF_MOVE_NOTIFY` | `is not set` | `is not set` |
| `DMABUF_SELFTESTS` | `is not set` | `is not set` |
| `DMABUF_SYSFS_STATS` | `y` | `y` |
| `DMA_SHARED_BUFFER` | `y` | `y` |
| `DRM` | `y` | `y` |
| `DRM_AMDGPU` | `is not set` | `is not set` |
| `DRM_ANALOGIX_ANX6345` | `is not set` | `is not set` |
| `DRM_ANALOGIX_ANX78XX` | `is not set` | `is not set` |
| `DRM_ANALOGIX_DP` | `y` | `y` |
| `DRM_ARCPGU` | `is not set` | `is not set` |
| `DRM_AST` | `is not set` | `is not set` |
| `DRM_BOCHS` | `is not set` | `is not set` |
| `DRM_BRIDGE` | `y` | `y` |
| `DRM_CDNS_DSI` | `is not set` | `is not set` |
| `DRM_CDNS_MHDP8546` | `is not set` | `is not set` |
| `DRM_CHRONTEL_CH7033` | `is not set` | `is not set` |
| `DRM_CIRRUS_QEMU` | `is not set` | `is not set` |
| `DRM_DEBUG_DP_MST_TOPOLOGY_REFS` | `is not set` | `is not set` |
| `DRM_DEBUG_MM` | `is not set` | `is not set` |
| `DRM_DISPLAY_CONNECTOR` | `y` | `y` |
| `DRM_DP` | `y` | `y` |
| `DRM_DP_AUX_CHARDEV` | `y` | `y` |
| `DRM_DP_CEC` | `is not set` | `is not set` |
| `DRM_DW_HDMI` | `y` | `y` |
| `DRM_DW_HDMI_AHB_AUDIO` | `is not set` | `is not set` |
| `DRM_DW_HDMI_CEC` | `y` | `y` |
| `DRM_DW_HDMI_I2S_AUDIO` | `y` | `y` |
| `DRM_DW_MIPI_DSI` | `y` | `y` |
| `DRM_EDID` | `y` | `y` |
| `DRM_ETNAVIV` | `is not set` | `is not set` |
| `DRM_FBDEV_EMULATION` | `y` | `y` |
| `DRM_FBDEV_LEAK_PHYS_SMEM` | `is not set` | `is not set` |
| `DRM_FBDEV_OVERALLOC` | `100` | `100` |
| `DRM_GM12U320` | `is not set` | `is not set` |
| `DRM_HDLCD` | `is not set` | `is not set` |
| `DRM_HISI_HIBMC` | `is not set` | `is not set` |
| `DRM_HISI_KIRIN` | `is not set` | `is not set` |
| `DRM_I2C_ADV7511` | `is not set` | `is not set` |
| `DRM_I2C_CH7006` | `is not set` | `is not set` |
| `DRM_I2C_NXP_TDA9950` | `is not set` | `is not set` |
| `DRM_I2C_NXP_TDA998X` | `is not set` | `is not set` |
| `DRM_I2C_SIL164` | `is not set` | `is not set` |
| `DRM_IGNORE_IOTCL_PERMIT` | `y` | `y` |
| `DRM_ITE_IT6161` | `is not set` | `is not set` |
| `DRM_KMS_HELPER` | `y` | `y` |
| `DRM_KOMEDA` | `is not set` | `is not set` |
| `DRM_LEGACY` | `is not set` | `is not set` |
| `DRM_LIMA` | `is not set` | `is not set` |
| `DRM_LOAD_EDID_FIRMWARE` | `y` | `y` |
| `DRM_LONTIUM_LT9611` | `is not set` | `is not set` |
| `DRM_LVDS_CODEC` | `is not set` | `is not set` |
| `DRM_MALI_DISPLAY` | `is not set` | `is not set` |
| `DRM_MAXIM_MAX96745` | `is not set` | `is not set` |
| `DRM_MAXIM_MAX96755F` | `is not set` | `is not set` |
| `DRM_MEGACHIPS_STDPXXXX_GE_B850V3_FW` | `is not set` | `is not set` |
| `DRM_MGAG200` | `is not set` | `is not set` |
| `DRM_MIPI_DSI` | `y` | `y` |
| `DRM_NOUVEAU` | `is not set` | `is not set` |
| `DRM_NWL_MIPI_DSI` | `is not set` | `is not set` |
| `DRM_NXP_PTN3460` | `is not set` | `is not set` |
| `DRM_PANEL` | `y` | `y` |
| `DRM_PANEL_ARM_VERSATILE` | `is not set` | `is not set` |
| `DRM_PANEL_ASUS_Z00T_TM5P5_NT35596` | `is not set` | `is not set` |
| `DRM_PANEL_BOE_HIMAX8279D` | `is not set` | `is not set` |
| `DRM_PANEL_BOE_TV101WUM_NL6` | `is not set` | `is not set` |
| `DRM_PANEL_BRIDGE` | `y` | `y` |
| `DRM_PANEL_ELIDA_KD35T133` | `is not set` | `is not set` |
| `DRM_PANEL_FEIXIN_K101_IM2BA02` | `is not set` | `is not set` |
| `DRM_PANEL_FEIYANG_FY07024DI26A30D` | `is not set` | `is not set` |
| `DRM_PANEL_ILITEK_IL9322` | `is not set` | `is not set` |
| `DRM_PANEL_ILITEK_ILI9881C` | `is not set` | `is not set` |
| `DRM_PANEL_INNOLUX_P079ZCA` | `is not set` | `is not set` |
| `DRM_PANEL_JDI_LT070ME05000` | `is not set` | `is not set` |
| `DRM_PANEL_KINGDISPLAY_KD097D04` | `is not set` | `is not set` |
| `DRM_PANEL_LEADTEK_LTK050H3146W` | `is not set` | `is not set` |
| `DRM_PANEL_LEADTEK_LTK500HD1829` | `is not set` | `is not set` |
| `DRM_PANEL_LG_LB035Q02` | `is not set` | `is not set` |
| `DRM_PANEL_LG_LG4573` | `is not set` | `is not set` |
| `DRM_PANEL_LVDS` | `is not set` | `is not set` |
| `DRM_PANEL_MANTIX_MLAF057WE51` | `is not set` | `is not set` |
| `DRM_PANEL_MAXIM_MAX96752F` | `is not set` | `is not set` |
| `DRM_PANEL_NEC_NL8048HL11` | `is not set` | `is not set` |
| `DRM_PANEL_NOVATEK_NT35510` | `is not set` | `is not set` |
| `DRM_PANEL_NOVATEK_NT39016` | `is not set` | `is not set` |
| `DRM_PANEL_OLIMEX_LCD_OLINUXINO` | `is not set` | `is not set` |
| `DRM_PANEL_ORIENTATION_QUIRKS` | `y` | `y` |
| `DRM_PANEL_ORISETECH_OTM8009A` | `is not set` | `is not set` |
| `DRM_PANEL_OSD_OSD101T2587_53TS` | `is not set` | `is not set` |
| `DRM_PANEL_PANASONIC_VVX10F034N00` | `is not set` | `is not set` |
| `DRM_PANEL_RASPBERRYPI_TOUCHSCREEN` | `is not set` | `is not set` |
| `DRM_PANEL_RAYDIUM_RM67191` | `is not set` | `is not set` |
| `DRM_PANEL_RAYDIUM_RM68200` | `is not set` | `is not set` |
| `DRM_PANEL_RONBO_RB070D30` | `is not set` | `is not set` |
| `DRM_PANEL_SAMSUNG_LD9040` | `is not set` | `is not set` |
| `DRM_PANEL_SAMSUNG_S6D16D0` | `is not set` | `is not set` |
| `DRM_PANEL_SAMSUNG_S6E3HA2` | `is not set` | `is not set` |
| `DRM_PANEL_SAMSUNG_S6E63J0X03` | `is not set` | `is not set` |
| `DRM_PANEL_SAMSUNG_S6E63M0` | `is not set` | `is not set` |
| `DRM_PANEL_SAMSUNG_S6E88A0_AMS452EF01` | `is not set` | `is not set` |
| `DRM_PANEL_SAMSUNG_S6E8AA0` | `is not set` | `is not set` |
| `DRM_PANEL_SEIKO_43WVF1G` | `is not set` | `is not set` |
| `DRM_PANEL_SHARP_LQ101R1SX01` | `is not set` | `is not set` |
| `DRM_PANEL_SHARP_LS037V7DW01` | `is not set` | `is not set` |
| `DRM_PANEL_SHARP_LS043T1LE01` | `is not set` | `is not set` |
| `DRM_PANEL_SIMPLE` | `y` | `y` |
| `DRM_PANEL_SITRONIX_ST7701` | `is not set` | `is not set` |
| `DRM_PANEL_SITRONIX_ST7703` | `is not set` | `is not set` |
| `DRM_PANEL_SITRONIX_ST7789V` | `is not set` | `is not set` |
| `DRM_PANEL_SONY_ACX565AKM` | `is not set` | `is not set` |
| `DRM_PANEL_TPO_TD028TTEC1` | `is not set` | `is not set` |
| `DRM_PANEL_TPO_TD043MTEA1` | `is not set` | `is not set` |
| `DRM_PANEL_TPO_TPG110` | `is not set` | `is not set` |
| `DRM_PANEL_TRULY_NT35597_WQXGA` | `is not set` | `is not set` |
| `DRM_PANEL_VISIONOX_RM69299` | `is not set` | `is not set` |
| `DRM_PANEL_XINPENG_XPP055C272` | `is not set` | `is not set` |
| `DRM_PANFROST` | `is not set` | `is not set` |
| `DRM_PARADE_PS8622` | `is not set` | `is not set` |
| `DRM_PARADE_PS8640` | `is not set` | `is not set` |
| `DRM_PL111` | `is not set` | `is not set` |
| `DRM_QXL` | `is not set` | `is not set` |
| `DRM_RADEON` | `is not set` | `is not set` |
| `DRM_RCAR_DW_HDMI` | `is not set` | `is not set` |
| `DRM_RK1000_TVE` | `is not set` | `is not set` |
| `DRM_ROCKCHIP` | `y` | `y` |
| `DRM_ROCKCHIP_VVOP` | `is not set` | `is not set` |
| `DRM_ROHM_BU18XL82` | `is not set` | `is not set` |
| `DRM_SII902X` | `y` | `y` |
| `DRM_SII9234` | `is not set` | `is not set` |
| `DRM_SIL_SII8620` | `is not set` | `is not set` |
| `DRM_SIMPLE_BRIDGE` | `is not set` | `is not set` |
| `DRM_THINE_THC63LVD1024` | `is not set` | `is not set` |
| `DRM_TIDSS` | `is not set` | `is not set` |
| `DRM_TI_SN65DSI86` | `is not set` | `is not set` |
| `DRM_TI_TFP410` | `is not set` | `is not set` |
| `DRM_TI_TPD12S015` | `is not set` | `is not set` |
| `DRM_TOSHIBA_TC358762` | `is not set` | `is not set` |
| `DRM_TOSHIBA_TC358764` | `is not set` | `is not set` |
| `DRM_TOSHIBA_TC358767` | `is not set` | `is not set` |
| `DRM_TOSHIBA_TC358768` | `is not set` | `is not set` |
| `DRM_TOSHIBA_TC358775` | `is not set` | `is not set` |
| `DRM_UDL` | `is not set` | `is not set` |
| `DRM_VGEM` | `is not set` | `is not set` |
| `DRM_VKMS` | `is not set` | `is not set` |
| `DYNAMIC_DEBUG` | `y` | `y` |
| `FTRACE` | `y` | `y` |
| `FUNCTION_TRACER` | `y` | `y` |
| `IKCONFIG` | `y` | `y` |
| `KPROBES` | `is not set` | `is not set` |
| `MALI_ARBITRATION` | `is not set` | `is not set` |
| `MALI_BASE_MODULES` | `is not set` | `is not set` |
| `MALI_BIFROST` | `y` | `y` |
| `MALI_BIFROST_DEBUG` | `y` | `y` |
| `MALI_BIFROST_DEVFREQ` | `y` | `y` |
| `MALI_BIFROST_ENABLE_TRACE` | `y` | `y` |
| `MALI_BIFROST_EXPERT` | `y` | `y` |
| `MALI_BIFROST_FENCE_DEBUG` | `y` | `y` |
| `MALI_BIFROST_GATOR_SUPPORT` | `y` | `y` |
| `MALI_BIFROST_NO_MALI` | `is not set` | `is not set` |
| `MALI_BIFROST_PRFCNT_SET_SECONDARY` | `is not set` | `is not set` |
| `MALI_BIFROST_SYSTEM_TRACE` | `y` | `y` |
| `MALI_CORESIGHT` | `is not set` | `is not set` |
| `MALI_CORESTACK` | `is not set` | `is not set` |
| `MALI_CSF_SUPPORT` | `y` | `y` |
| `MALI_DEBUG` | `y` | `y` |
| `MALI_DEVFREQ` | `y` | `y` |
| `MALI_DMA_BUF_LEGACY_COMPAT` | `is not set` | `is not set` |
| `MALI_DMA_BUF_MAP_ON_ATTACH` | `y` | `y` |
| `MALI_DMA_BUF_MAP_ON_DEMAND` | `is not set` | `is not set` |
| `MALI_DMA_FENCE` | `is not set` | `is not set` |
| `MALI_DT` | `y` | `y` |
| `MALI_EXPERT` | `y` | `y` |
| `MALI_FENCE_DEBUG` | `y` | `y` |
| `MALI_GATOR_SUPPORT` | `is not set` | `is not set` |
| `MALI_GPU_MMU_AARCH64` | `is not set` | `is not set` |
| `MALI_HW_ERRATA_1485982_NOT_AFFECTED` | `is not set` | `is not set` |
| `MALI_HW_ERRATA_1485982_USE_CLOCK_ALTERNATIVE` | `is not set` | `is not set` |
| `MALI_JOB_DUMP` | `is not set` | `is not set` |
| `MALI_KUTF` | `is not set` | `is not set` |
| `MALI_MIDGARD` | `y` | `y` |
| `MALI_MIDGARD_ENABLE_TRACE` | `is not set` | `is not set` |
| `MALI_NO_MALI` | `is not set` | `is not set` |
| `MALI_PLATFORM_DEVICETREE` | `is not set` | `is not set` |
| `MALI_PLATFORM_FAKE` | `is not set` | `is not set` |
| `MALI_PLATFORM_NAME` | `"rk"` | `"rk"` |
| `MALI_PLATFORM_THIRDPARTY` | `y` | `y` |
| `MALI_PLATFORM_THIRDPARTY_NAME` | `"rk"` | `"rk"` |
| `MALI_PMU_PARALLEL_POWER_UP` | `is not set` | `is not set` |
| `MALI_PRFCNT_SET_PRIMARY` | `y` | `y` |
| `MALI_PRFCNT_SET_SECONDARY` | `is not set` | `is not set` |
| `MALI_PRFCNT_SET_TERTIARY` | `is not set` | `is not set` |
| `MALI_PWRSOFT_765` | `y` | `y` |
| `MALI_QUIET` | `is not set` | `is not set` |
| `MALI_REAL_HW` | `y` | `y` |
| `MALI_SHARED_INTERRUPTS` | `y` | `y` |
| `MALI_SYSTEM_TRACE` | `is not set` | `is not set` |
| `MALI_TRACE_TIMELINE` | `is not set` | `is not set` |
| `MODULES` | `y` | `y` |
| `MODULE_SIG` | `is not set` | `is not set` |
| `MODVERSIONS` | `is not set` | `is not set` |
| `ROCKCHIP_DRM_DIRECT_SHOW` | `is not set` | `is not set` |
| `ROCKCHIP_DW_MIPI_DSI` | `y` | `y` |
| `ROCKCHIP_MPP_AV1DEC` | `y` | `y` |
| `ROCKCHIP_MPP_IEP2` | `y` | `y` |
| `ROCKCHIP_MPP_JPGDEC` | `y` | `y` |
| `ROCKCHIP_MPP_PROC_FS` | `y` | `y` |
| `ROCKCHIP_MPP_RKVDEC` | `y` | `y` |
| `ROCKCHIP_MPP_RKVDEC2` | `y` | `y` |
| `ROCKCHIP_MPP_RKVENC` | `y` | `y` |
| `ROCKCHIP_MPP_RKVENC2` | `y` | `y` |
| `ROCKCHIP_MPP_SERVICE` | `y` | `y` |
| `ROCKCHIP_MPP_VDPP` | `is not set` | `is not set` |
| `ROCKCHIP_MPP_VDPU1` | `y` | `y` |
| `ROCKCHIP_MPP_VDPU2` | `y` | `y` |
| `ROCKCHIP_MPP_VEPU1` | `y` | `y` |
| `ROCKCHIP_MPP_VEPU2` | `y` | `y` |
| `ROCKCHIP_RGA` | `is not set` | `is not set` |
| `ROCKCHIP_RGA_ASYNC` | `y` | `y` |
| `ROCKCHIP_RGA_DEBUGGER` | `y` | `y` |
| `ROCKCHIP_RGA_DEBUG_FS` | `y` | `y` |
| `ROCKCHIP_RKNPU` | `y` | `y` |
| `ROCKCHIP_RKNPU_DEBUG_FS` | `y` | `y` |
| `ROCKCHIP_RKNPU_DRM_GEM` | `y` | `y` |
| `ROCKCHIP_RKNPU_FENCE` | `is not set` | `is not set` |
| `ROCKCHIP_RKNPU_PROC_FS` | `is not set` | `is not set` |
| `ROCKCHIP_RKNPU_SRAM` | `is not set` | `is not set` |
| `ROCKCHIP_VOP2` | `y` | `y` |
| `SW_SYNC` | `y` | `y` |
| `SYNC_FILE` | `y` | `y` |
| `TRACING` | `y` | `y` |
| `VIDEO_ROCKCHIP_CIF` | `y` | `y` |
| `VIDEO_ROCKCHIP_HDMIRX` | `y` | `y` |
| `VIDEO_ROCKCHIP_HDMIRX_CLASS` | `y` | `y` |
| `VIDEO_ROCKCHIP_ISP` | `y` | `y` |
| `VIDEO_ROCKCHIP_ISPP` | `y` | `y` |
| `VIDEO_ROCKCHIP_ISPP_FEC` | `is not set` | `is not set` |
| `VIDEO_ROCKCHIP_ISPP_VERSION_V20` | `y` | `y` |
| `VIDEO_ROCKCHIP_ISP_VERSION_V30` | `y` | `y` |
| `VIDEO_ROCKCHIP_PREISP` | `is not set` | `is not set` |
| `VIDEO_ROCKCHIP_RGA` | `y` | `y` |

</details>

