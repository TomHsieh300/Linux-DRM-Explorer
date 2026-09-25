#!/bin/sh
# collect-results.sh - gather the on-hardware evidence for the TODO(on-hardware)
# placeholders in docs/ into one directory (and a .tar.gz of it).
#
# Default mode is read-only: it queries KMS state, debugfs, the device tree and
# runs the listing modes of the demos. Nothing is modeset.
# With --active it additionally runs drm-frame-timing (which takes over the
# display for about a minute and then restores it).
#
# Everything that needs eyes on the panel (tearing, blending, colour bars) is
# NOT automated; see docs/HARDWARE_VALIDATION.md.

# The inner `sh -c '...'` snippets expand their variables in the child shell
# on purpose, so single quotes are intended.
# shellcheck disable=SC2016

set -u

usage() {
	cat <<'EOF'
Usage: sudo tools/collect-results.sh [--active] [-o <outdir>] [-d <card index>]

  --active       also run the frame-timing measurements (takes over the
                 display for about a minute, restores it afterwards)
  -o <outdir>    output directory (default ./hw-results-<date>-<time>)
  -d <index>     DRM card index, /dev/dri/card<index> (default 0)
  -h, --help     show this help

Run from the repository root after `make`, on a text console with no
compositor. Send back the generated .tar.gz (or the directory).
EOF
}

ACTIVE=0
OUT=""
CARD=0
while [ $# -gt 0 ]; do
	case "$1" in
	--active) ACTIVE=1 ;;
	-o) [ $# -ge 2 ] || { usage; exit 2; }; OUT=$2; shift ;;
	-d) [ $# -ge 2 ] || { usage; exit 2; }; CARD=$2; shift ;;
	-h|--help) usage; exit 0 ;;
	*) echo "unknown option: $1" >&2; usage; exit 2 ;;
	esac
	shift
done

case "$CARD" in
''|*[!0-9]*) echo "-d expects a number" >&2; exit 2 ;;
esac

if [ "$(id -u)" -ne 0 ]; then
	echo "must be run as root (debugfs and DRM master access)" >&2
	exit 1
fi

if [ ! -f Makefile ] || [ ! -d src ]; then
	echo "run from the repository root" >&2
	exit 1
fi

[ -n "$OUT" ] || OUT="./hw-results-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT" || exit 1

DEV="/dev/dri/card$CARD"
DBG="/sys/kernel/debug/dri/$CARD"
SUMMARY="$OUT/00-summary.txt"
: > "$SUMMARY"

# run <file> <command...>: record the command, its output and exit code
run() {
	f="$OUT/$1"
	shift
	{
		echo "# \$ $*"
		echo "# date: $(date -Iseconds)"
		"$@" 2>&1
		rc=$?
		echo "# exit code: $rc"
	} > "$f"
	printf '%-45s %s\n' "$f" "$*" >> "$SUMMARY"
}

# run a demo binary only if it has been built
run_bin() {
	f=$1
	bin=$2
	shift 2
	if [ -x "$bin" ]; then
		run "$f" timeout 120 "$bin" "$@"
	else
		echo "# $bin not built (run make first)" > "$OUT/$f"
		printf '%-45s %s\n' "$OUT/$f" "SKIPPED: $bin not built" >> "$SUMMARY"
	fi
}

have() { command -v "$1" >/dev/null 2>&1; }

if [ ! -d /sys/kernel/debug/dri ]; then
	mount -t debugfs none /sys/kernel/debug 2>/dev/null
fi

echo "Collecting into $OUT ..."

# ---- Test Environment table (README) -------------------------------------
run 01-uname.txt uname -a
run 01-os-release.txt cat /etc/os-release
run 01-cmdline.txt cat /proc/cmdline
run 01-libdrm.txt sh -c 'pkg-config --modversion libdrm; dpkg -l "libdrm*" 2>/dev/null | grep "^ii"'
run 01-dri-name.txt cat "$DBG/name"
run 01-dri-debugfs-ls.txt ls -la "$DBG/"
if [ -r /proc/device-tree/model ]; then
	run 01-dt-model.txt sh -c 'tr "\0" "\n" < /proc/device-tree/model; echo; tr "\0" "\n" < /proc/device-tree/compatible'
fi

# ---- Experiments 01, 02, 05: modetest listings (read-only) ---------------
if have modetest; then
	run 02-modetest-connectors.txt modetest -M rockchip -c
	run 02-modetest-encoders.txt modetest -M rockchip -e
	run 02-modetest-planes.txt modetest -M rockchip -p
	run 02-modetest-help.txt sh -c 'modetest -h 2>&1'
else
	echo "modetest not installed (apt install libdrm-tests)" > "$OUT/02-modetest-MISSING.txt"
fi

# ---- Experiment 03: timing registers and device tree ---------------------
if [ -r "$DBG/regs" ]; then
	run 03-regs-full.txt cat "$DBG/regs"
	run 03-regs-vp3-f40-f50.txt sh -c "grep -iE 'fdd90f4|fdd90f5' '$DBG/regs'"
elif [ -r "$DBG/vop2/regs" ]; then
	run 03-regs-full.txt cat "$DBG/vop2/regs"
fi
if have dtc; then
	run 03-live-dt.dts dtc -I fs -O dts /proc/device-tree
else
	echo "dtc not installed (apt install device-tree-compiler)" > "$OUT/03-live-dt-MISSING.txt"
fi

# ---- Experiments 04, 06, 14: ownership, IRQ, vblank, debugfs ------------
run 04-clients.txt cat "$DBG/clients"
run 06-vblankoffdelay.txt cat /sys/module/drm/parameters/vblankoffdelay
run 06-interrupts-vop.txt sh -c 'grep -i vop /proc/interrupts'
for f in state framebuffer summary; do
	[ -r "$DBG/$f" ] && run "14-debugfs-$f.txt" cat "$DBG/$f"
done
run 14-drm-debug-param.txt cat /sys/module/drm/parameters/debug
run 14-tracefs-drm-events.txt sh -c 'ls /sys/kernel/tracing/events/drm/ 2>/dev/null || ls /sys/kernel/debug/tracing/events/drm/'

# ---- Experiments 10, 12, 13, 16: listing modes of the demos --------------
run_bin 10-atomic-property-discovery.txt ./src/drm-atomic-demo
run_bin 12-plane-props-list.txt ./src/drm-plane-props --list -d "$DEV"
run_bin 13-formats-modifiers-list.txt ./src/drm-formats-modifiers --list -d "$DEV"
run_bin 16-hotplug-topology.txt ./src/drm-hotplug-monitor --topology -d "$DEV"

# ---- Experiment 18: GPU driver stack (read-only) --------------------------
run 18-dev-nodes.txt sh -c 'ls -la /dev/mali* /dev/dri/ /dev/dma_heap/ 2>&1'
run 18-gpu-modules.txt sh -c 'ls /sys/module | grep -iE "mali|kbase|bifrost|panfrost|panthor"; echo "--- lsmod"; lsmod 2>/dev/null | grep -iE "mali|kbase|bifrost|panfrost|panthor"'
run 18-gpu-dt-node.txt sh -c 'for n in /proc/device-tree/gpu@* /proc/device-tree/*/gpu@*; do [ -d "$n" ] || continue; echo "== $n"; printf "compatible: "; tr "\0" " " < "$n/compatible"; echo; [ -r "$n/status" ] && { printf "status: "; tr -d "\0" < "$n/status"; echo; }; done'
run 18-gpu-driver-binding.txt sh -c 'for d in /sys/bus/platform/devices/*.gpu; do [ -e "$d" ] || continue; echo "$d -> $(readlink "$d/driver")"; done'
run 18-gpuinfo.txt sh -c 'for f in $(find /sys/devices -maxdepth 6 -name gpuinfo 2>/dev/null); do echo "== $f"; cat "$f"; done'
run 18-mali-debugfs-ls.txt sh -c 'ls -la /sys/kernel/debug/mali0/ 2>&1'
run 18-render-node-names.txt sh -c 'for d in /sys/kernel/debug/dri/*; do [ -r "$d/name" ] && echo "$d: $(cat "$d/name")"; done'
run 18-gpu-devfreq.txt sh -c 'for d in /sys/class/devfreq/*; do echo "== $d"; cat "$d/name" "$d/cur_freq" "$d/available_frequencies" 2>/dev/null; done'
run 18-gpu-libs.txt sh -c 'ldconfig -p | grep -iE "mali|libEGL|libGLESv2|libgbm|libvulkan"; echo "--- files"; ls -la /usr/lib/*/libmali* /usr/lib/libmali* 2>&1; echo "--- packages"; dpkg-query -W -f "\${Package}\t\${Version}\n" 2>/dev/null | grep -iE "^(libmali|mali|mesa|libegl|libgles|libgbm|libglvnd|libgl1-mesa)"'
if have eglinfo; then
	run 18-eglinfo.txt timeout 60 eglinfo
else
	echo "eglinfo not installed (apt install mesa-utils)" > "$OUT/18-eglinfo-MISSING.txt"
fi
if have vulkaninfo; then
	run 18-vulkaninfo.txt timeout 60 vulkaninfo --summary
fi
run 18-dmesg-gpu.txt sh -c 'dmesg | grep -iE "mali|kbase|panthor|panfrost|gpu" | tail -200'

# ---- Experiment 17: frame timing (only with --active) --------------------
if [ "$ACTIVE" -eq 1 ]; then
	echo "Running frame-timing measurements (display will be taken over) ..."
	run_bin 17-frame-timing-baseline.txt ./src/drm-frame-timing -d "$DEV" -n 600 \
		--csv "$OUT/17-frame-timing-baseline.csv"
	for l in 10 15 20 35; do
		run_bin "17-frame-timing-load$l.txt" ./src/drm-frame-timing -d "$DEV" -n 300 \
			--load-ms "$l" --csv "$OUT/17-frame-timing-load$l.csv"
	done
fi

run 99-dmesg-drm.txt sh -c 'dmesg | grep -iE "drm|vop|dsi|panel|rockchip" | tail -300'

tar czf "$OUT.tar.gz" -C "$(dirname "$OUT")" "$(basename "$OUT")" 2>/dev/null &&
	echo "Done: $OUT.tar.gz" || echo "Done: $OUT (tar failed, send the directory)"
