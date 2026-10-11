# LubanCat-5IO — source tree of the running kernel

Survey taken 2026-10-11 on the host and over the board's serial console
(`/dev/ttyUSB0`, 1500000 8N1). Everything was read-only: no builds, no git
checkout/clean, and nothing was copied into this repo. Board reads were
md5-verified, as in [board-runtime.md](board-runtime.md).

## Important: the board was reflashed on 2026-10-11

The facts in [board-runtime.md](board-runtime.md) and
[board-kernel-config.md](board-kernel-config.md) describe the kernel that was on
the board **before** 2026-10-11:

```
Linux version 6.1.99-rk3588 (tom@tom-B550M-AORUS-ELITE) (...) #7 SMP Fri Feb 13 11:34:08 CST 2026
```

On 2026-10-11 the user rebuilt the image on this host from the LubanCat generic
SDK (manifest `lubancat_linux/lubancat_linux_generic_20260114.xml`, SDK
defconfig `LubanCat_rk3588_ubuntu_linux6.1_lite_defconfig`) and flashed it. The
board now runs build **#11** from this host. That `#7` kernel no longer exists
on the board, so its `System.map` and `.version` can't be checked against any tree.
This document therefore answers two questions:

1. **Source of the kernel running now (#11):** found and confirmed. It is
   `/home/tom/LubanCat-6.1/kernel-6.1`.
2. **Source of the earlier #7 kernel:** **not found on this host**. It was most
   likely built on the other machine (`tom-B550M-AORUS-ELITE`). See "About the
   #7 kernel" below.

The running kernel's **config is unchanged**: `/proc/config.gz` md5 is still
`931026800151912fc38c37fce658e5a9` (8460 lines), the same value as for #7. So
everything in `board-kernel-config.md` still applies.

## 1. Host

```console
$ hostname
tom-VM
```

**This host is not `tom-B550M-AORUS-ELITE`.** It is a VMware guest. The only
disk is `sda` (500G, `/` on `sda2`). The VMware shared folder
`/mnt/hgfs/vm_share` (18G, from the VM's host PC) was searched too. It holds
`Alpha_imx6ull/`, `frdm-imx95/` and `lubancat/` (LubanCat hardware files and
Rockchip PDF documentation), but no kernel tree: no `Makefile` with
`SUBLEVEL = 99`, and no `kernel.release` or `System.map`. `/mnt/host` and
`/media/tom` are empty.

## 2. Candidate trees

Search: every `Makefile` under `/` (single filesystem; `/proc`, `/sys`, `.git`,
`.repo` and snap pruned) that has `VERSION = 6`, `PATCHLEVEL = 1`,
`SUBLEVEL = 99` and a `NAME` line:

```
/home/tom/LubanCat-6.1/kernel-6.1
/home/tom/LubanCat-6.1/kernel-6.1/debian/linux-headers/usr/src/linux-headers-6.1.99-rk3588
/home/tom/LubanCat-6.1/ubuntu20.04/binary/usr/src/linux-headers-6.1.99-rk3588
```

The two `linux-headers-6.1.99-rk3588` hits are packaged header trees produced
by the build (inside the kernel's `debian/` staging dir and inside the rootfs),
not source trees. **The only candidate is `/home/tom/LubanCat-6.1/kernel-6.1`.**

`include/config/kernel.release` files found (same search):

```
/home/tom/LubanCat-6.1/kernel-6.1/include/config/kernel.release: 6.1.99-rk3588
/home/tom/LubanCat-6.1/kernel-6.1/debian/linux-headers/usr/src/linux-headers-6.1.99-rk3588/include/config/kernel.release: 6.1.99-rk3588
/home/tom/LubanCat-6.1/ubuntu20.04/binary/usr/src/linux-headers-6.1.99-rk3588/include/config/kernel.release: 6.1.99-rk3588
/home/tom/LubanCat/kernel/include/config/kernel.release: 5.10.160
/home/tom/LubanCat/kernel/debian/linux-headers/usr/src/linux-headers-5.10.160/include/config/kernel.release: 5.10.160
/home/tom/LubanCat/ubuntu/binary/usr/src/linux-headers-5.10.160/include/config/kernel.release: 5.10.160
/home/tom/Documents/corechip_bsp_sdk/linux-6.18/include/config/kernel.release: 6.18.15-g74b7ee0b708c
/home/tom/Documents/imx93-dev/output-uboot-nxp/build/linux-custom/include/config/kernel.release: 6.12.3
/home/tom/Documents/corechip_qemu_sdk/linux-6.12.28/include/config/kernel.release: 6.12.28
/usr/src/linux-headers-7.0.0-34-generic/include/config/kernel.release: 7.0.0-34-generic
/usr/src/linux-headers-7.0.0-31-generic/include/config/kernel.release: 7.0.0-31-generic
```

## 3. Evidence for `/home/tom/LubanCat-6.1/kernel-6.1`

Board side (serial, md5-verified):

```console
# cat /proc/version
Linux version 6.1.99-rk3588 (tom@tom-VM) (aarch64-none-linux-gnu-gcc (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 10.3.1 20210621, GNU ld (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 2.36.1.20210621) #11 SMP Sun Oct 11 14:22:33 CST 2026

# md5sum /boot/System.map-6.1.99-rk3588
9294ce891a33b637101fbde16fe2e72d  /boot/System.map-6.1.99-rk3588
```

| Check | Tree | vs running board (#11) | vs earlier #7 |
|---|---|---|---|
| a. `.config` md5 | `931026800151912fc38c37fce658e5a9` | **match** | **match** (same config) |
| b. `Linux version` in `arch/arm64/boot/Image` | `… (tom@tom-VM) … #11 SMP Sun Oct 11 14:22:33 CST 2026` | **exact match** | no match (`#7`, `tom-B550M-AORUS-ELITE`, Feb 13) |
| c. `System.map` md5 | `9294ce891a33b637101fbde16fe2e72d` (also `extboot/System.map-6.1.99-rk3588`) | **match** | unknown (board's #7 System.map is gone) |
| d. `.version` | `12` | — (running build is `#11`) | no match (expected 7) |

Commands and verbatim output for (b):

```console
$ strings arch/arm64/boot/Image | grep -E '#11 SMP Sun Oct 11 14:22:33 CST 2026' | head -2
Linux version 6.1.99-rk3588 (tom@tom-VM) (aarch64-none-linux-gnu-gcc (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 10.3.1 20210621, GNU ld (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 2.36.1.20210621) #11 SMP Sun Oct 11 14:22:33 CST 2026
#11 SMP Sun Oct 11 14:22:33 CST 2026
```

The *first* `Linux version` string in the Image ends in `# SMP ` with no number
or date (a template in 6.1); the second one is the full banner above.
`vmlinux` shows the same first string. Why `.version` reads `12` while the
running build is `#11` is **unknown**; the tree was built more than once on
2026-10-11.

**Verdict: confirmed source of the running kernel (#11)**, by (b) and (c).
For the earlier #7 kernel it is **candidate, not confirmed**: same upstream
repo, release and config, but (b) does not match and (c)/(d) can't be checked.

## 4. Confirmed tree details

```console
$ cd /home/tom/LubanCat-6.1/kernel-6.1
$ git remote -v
origin	https://github.com/LubanCat/kernel (fetch)
origin	https://github.com/LubanCat/kernel (push)
$ git branch --show-current          # (empty: detached HEAD)
$ git rev-parse --abbrev-ref HEAD
HEAD
$ git log -1 --format='%H %cd %s'
cb5bebb5c4452106bbcbf0951b01c89834fd6ec1 Thu Feb 26 07:47:06 2026 +0000 dts LubanCat-2 修复dsi0点不亮新5.5寸屏幕,注释掉mipi-dsi追加节点
$ git status --short | head -50
 M .version
$ git status --short | wc -l
1
$ git diff .version
-6
+12
$ git describe --tags --always
cb5bebb5c445
$ git rev-parse --is-shallow-repository
true
$ du -sh .
4.0G	.
```

- **Detached HEAD at `cb5bebb5c445`** (`lbc-develop-6.1`), pinned by the SDK manifest
  `lubancat_linux/lubancat_linux_generic_20260114.xml` (repo `clone-depth="1"`,
  so the history is shallow and `git describe` has no tags to use).
- **No uncommitted source changes:** the only modified tracked file is `.version`,
  the build counter. The running kernel is upstream `cb5bebb5c445` + this config.
- `du` includes the build output (objects, `vmlinux`, debs staging).

Board `/boot/kerneldeb` (list only) is empty and records no source:

```console
# ls -la /boot/kerneldeb
total 8
drwxrwxr-x 2 root root 4096 Jun 18  2024 .
drwxr-xr-x 7 root root 4096 Jun 18  2024 ..
```

## About the #7 kernel (not confirmed)

- The board string names `tom-B550M-AORUS-ELITE`, and that machine is not reachable
  from here. Its source tree, if it still exists, is on that machine: **unknown**.
- Upstream `LubanCat/kernel` commits `.version` = `6` (see the `git diff` above),
  so a single build from a clean checkout is numbered `#7`. That fits a one-time
  build from a fresh `LubanCat/kernel` 6.1 checkout. *(Inference, not verified.)*
- From the GitHub history: the newest `lbc-develop-6.1` commit before the #7 build
  time (2026-02-13 11:34 CST) is `c18b7037b594` (2026-02-11), the parent of
  `cb5bebb5c445`. The one commit between them touches only
  `arch/arm64/boot/dts/rockchip/rk3568-lubancat-2-v2.dts`. Whether #7 was built
  from `c18b7037b594` is **unknown**. The release string can't tell: the #11
  build here was made from a git checkout and also shows plain `6.1.99-rk3588`
  with no `-g<hash>` suffix.
