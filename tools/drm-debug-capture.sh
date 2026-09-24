#!/bin/sh
# =============================================================================
# drm-debug-capture.sh - one-shot DRM/KMS debug evidence collector
#
# Companion tool for Experiment 14 (docs/14_Debugging_and_Tracing.md).
#
# What it does, in order:
#   1. Saves the current drm.debug mask (/sys/module/drm/parameters/debug)
#      and installs a trap that ALWAYS restores it on exit / Ctrl-C.
#   2. Snapshots the generic DRM debugfs files of /sys/kernel/debug/dri/<N>/
#      (name, clients, internal_clients, gem_names, framebuffer, state) plus
#      Rockchip-specific files if they exist (BSP: summary, active_regs, regs,
#      mm_dump; mainline >= v6.14: vop2/summary, vop2/active_regs), the
#      per-CRTC crc/control files and /sys/kernel/debug/dma_buf/bufinfo.
#      Missing files are skipped, never treated as fatal.
#   3. Writes a marker into the kernel log via /dev/kmsg, sets the requested
#      drm.debug mask and (optionally) enables the drm:* vblank tracepoints
#      and/or dma_fence:* tracepoints in a PRIVATE ftrace instance, so the
#      global trace buffer and any other tracing session are left untouched.
#   4. Waits <seconds>, or runs the command given after "--" (e.g. one of the
#      demos in src/), then snapshots debugfs again.
#   5. Restores drm.debug, removes the trace instance, and saves the kernel
#      log lines emitted after the marker (the "dmesg delta").
#
# Nothing here writes to KMS state: it only reads debugfs/sysfs files and
# toggles logging/tracing knobs. It deliberately never opens crc/data,
# because opening that file starts CRC generation (drm_debugfs_crc.c,
# crtc_crc_open -> set_crc_source) and may trigger a commit.
#
# Target: RK3588 / VOP2 with Ubuntu Lite (no compositor). Not yet verified on
# hardware. Only syntax, --help and the "path missing" code paths have been
# exercised (on a machine without DRM hardware and with a fake sysfs/debugfs
# tree supplied through the DRM_CAPTURE_* override variables below).
#
# Test-only overrides (normally leave unset):
#   DRM_CAPTURE_PARAM    path of the drm.debug parameter file
#   DRM_CAPTURE_DEBUGFS  debugfs mount point           (default /sys/kernel/debug)
#   DRM_CAPTURE_TRACEFS  tracefs directory             (default: auto-detect)
#   DRM_CAPTURE_KMSG     kernel log injection device   (default /dev/kmsg)
# =============================================================================

set -u

PROG=$(basename "$0")

# ---- defaults ---------------------------------------------------------------
MASK="0x16"            # DRIVER | KMS | ATOMIC (see docs/14 section 3.1)
SECONDS_WIN=5
OUTDIR=""
CARD=0
TRACE_VBLANK=0
TRACE_FENCE=0
BUF_KB=4096

PARAM=${DRM_CAPTURE_PARAM:-/sys/module/drm/parameters/debug}
DEBUGFS=${DRM_CAPTURE_DEBUGFS:-/sys/kernel/debug}
TRACEFS=${DRM_CAPTURE_TRACEFS:-}
KMSG=${DRM_CAPTURE_KMSG:-/dev/kmsg}

# ---- state used by the cleanup trap ------------------------------------------
ORIG_MASK=""
MASK_CHANGED=0
TRACE_INST=""
CLEANED=0
MARKER=""

usage() {
	cat <<EOF
Usage: sudo $PROG [options] [-- command [args...]]

Collect DRM debug evidence (drm.debug log, debugfs snapshots, optional
ftrace events) around a time window or around one command.

Options:
  -m <mask>     drm.debug mask to use during the capture (hex like 0x16 or
                decimal). "keep" leaves drm.debug unchanged. Default: $MASK
                  0x01 CORE   0x02 DRIVER  0x04 KMS    0x08 PRIME
                  0x10 ATOMIC 0x20 VBL     0x40 STATE  0x80 LEASE
                  0x100 DP    0x200 DRMRES
  -t <seconds>  capture window when no command is given. Default: $SECONDS_WIN
  -o <outdir>   output directory. Default: ./drm-capture-<date>-<time>
  -c <index>    DRM minor index, i.e. /sys/kernel/debug/dri/<index>
                (0 for /dev/dri/card0). Default: $CARD
  -T            also record the drm:drm_vblank_event* tracepoints
  -F            also record the dma_fence:* tracepoints
  -b <kb>       per-CPU buffer size of the private trace instance. Default: $BUF_KB
  -h            show this help and exit

Examples:
  sudo $PROG -m 0x14 -- ./src/drm-atomic-demo --atomic   # atomic check failures
  sudo $PROG -m 0x20 -T -t 3                              # vblank heartbeat
  sudo $PROG -m 0x18 -F -- ./src/drm-dmabuf-fence --fence  # PRIME + fences

The original drm.debug value is restored on exit, including Ctrl-C.
Tracing uses a private ftrace instance; the global trace buffer is untouched.
EOF
}

log() {
	printf '%s\n' "$*"
	if [ -n "$OUTDIR" ] && [ -d "$OUTDIR" ]; then
		printf '%s\n' "$*" >>"$OUTDIR/capture.log"
	fi
}

warn() {
	log "WARNING: $*"
}

die() {
	printf '%s: error: %s\n' "$PROG" "$*" >&2
	exit 1
}

# -----------------------------------------------------------------------------
# cleanup: runs exactly once, from the EXIT trap. Restores every knob we
# touched. Each step is independent so one failure does not skip the rest.
# -----------------------------------------------------------------------------
cleanup() {
	[ "$CLEANED" -eq 1 ] && return
	CLEANED=1

	if [ "$MASK_CHANGED" -eq 1 ]; then
		if printf '%s\n' "$ORIG_MASK" >"$PARAM" 2>/dev/null; then
			log "restored drm.debug to $ORIG_MASK"
		else
			printf '%s: could not restore drm.debug to %s (%s)\n' \
				"$PROG" "$ORIG_MASK" "$PARAM" >&2
		fi
		MASK_CHANGED=0
	fi

	if [ -n "$TRACE_INST" ] && [ -d "$TRACE_INST" ]; then
		echo 0 >"$TRACE_INST/tracing_on" 2>/dev/null
		# Disable everything in the instance before removing it.
		echo >"$TRACE_INST/set_event" 2>/dev/null
		rmdir "$TRACE_INST" 2>/dev/null ||
			warn "could not remove trace instance $TRACE_INST (remove it with rmdir)"
		TRACE_INST=""
	fi
}

# Invoked only through "trap", which shellcheck cannot see.
# shellcheck disable=SC2317
on_signal() {
	log "interrupted, cleaning up"
	exit 130
}

trap cleanup EXIT
trap on_signal INT TERM HUP

# ---- argument parsing -------------------------------------------------------
# getopts has no long options; catch --help explicitly so that it is not taken
# as "end of options" followed by a command called "help".
case "${1:-}" in
--help) usage; exit 0 ;;
*) ;;
esac

while getopts ":m:t:o:c:b:TFh" opt; do
	case "$opt" in
	m) MASK=$OPTARG ;;
	t) SECONDS_WIN=$OPTARG ;;
	o) OUTDIR=$OPTARG ;;
	c) CARD=$OPTARG ;;
	b) BUF_KB=$OPTARG ;;
	T) TRACE_VBLANK=1 ;;
	F) TRACE_FENCE=1 ;;
	h) usage; exit 0 ;;
	:) usage >&2; die "option -$OPTARG needs an argument" ;;
	*) usage >&2; die "unknown option -$OPTARG" ;;
	esac
done
shift $((OPTIND - 1))
# Anything left ("$@") is the optional command to run during the capture.

is_uint() {
	case "$1" in
	'' | *[!0-9]*) return 1 ;;
	*) return 0 ;;
	esac
}

# Accept hex (0x...) or plain decimal. A leading 0 without x would be parsed
# as octal by the kernel (kstrtoul base 0), so it is rejected here to avoid
# surprises such as "010" meaning 8.
is_mask() {
	case "$1" in
	0x* | 0X*)
		_hex=${1#0[xX]}
		case "$_hex" in
		'' | *[!0-9a-fA-F]*) return 1 ;;
		*) return 0 ;;
		esac
		;;
	0) return 0 ;;
	0*) return 1 ;;
	*) is_uint "$1" ;;
	esac
}

if [ "$MASK" != "keep" ] && ! is_mask "$MASK"; then
	die "invalid mask '$MASK' (use hex like 0x16, decimal, or 'keep')"
fi
is_uint "$SECONDS_WIN" || die "invalid -t '$SECONDS_WIN' (whole seconds expected)"
is_uint "$CARD" || die "invalid -c '$CARD' (minor index expected)"
is_uint "$BUF_KB" || die "invalid -b '$BUF_KB' (KiB expected)"

# ---- privilege check ---------------------------------------------------------
# drm.debug is mode 0600 (module_param_named(debug, ..., 0600) in drm_print.c)
# and debugfs/tracefs are root-only on typical systems.
if [ "$(id -u)" -ne 0 ]; then
	die "must be run as root (try: sudo $0 ...)"
fi

# ---- output directory ---------------------------------------------------------
if [ -z "$OUTDIR" ]; then
	OUTDIR="./drm-capture-$(date +%Y%m%d-%H%M%S)"
fi
mkdir -p "$OUTDIR" || die "cannot create output directory $OUTDIR"
: >"$OUTDIR/capture.log" || die "cannot write into $OUTDIR"

log "drm-debug-capture: output in $OUTDIR"
log "date: $(date)"

# ---- environment summary -----------------------------------------------------
{
	echo "# uname -a"
	uname -a
	echo
	echo "# /proc/cmdline"
	cat /proc/cmdline 2>/dev/null || echo "(unreadable)"
	echo
	echo "# /sys/module/drm/parameters/*"
	if [ -d /sys/module/drm/parameters ]; then
		for _p in /sys/module/drm/parameters/*; do
			[ -r "$_p" ] || continue
			printf '%s = %s\n' "$(basename "$_p")" "$(cat "$_p" 2>/dev/null)"
		done
	else
		echo "(no /sys/module/drm/parameters - drm core not loaded?)"
	fi
	echo
	echo "# /sys/class/drm connectors (status / enabled / dpms / modes)"
	for _c in /sys/class/drm/card*-*; do
		[ -d "$_c" ] || continue
		printf '%s: status=%s enabled=%s dpms=%s\n' "$(basename "$_c")" \
			"$(cat "$_c/status" 2>/dev/null)" \
			"$(cat "$_c/enabled" 2>/dev/null)" \
			"$(cat "$_c/dpms" 2>/dev/null)"
		sed 's/^/    mode: /' "$_c/modes" 2>/dev/null
	done
} >"$OUTDIR/environment.txt" 2>&1

# ---- locate debugfs ------------------------------------------------------------
DRI=""
if [ -d "$DEBUGFS/dri" ]; then
	DRI="$DEBUGFS/dri/$CARD"
	if [ ! -d "$DRI" ]; then
		warn "$DRI does not exist; available entries:"
		for _d in "$DEBUGFS"/dri/*; do
			[ -e "$_d" ] && log "    $(basename "$_d")"
		done
		DRI=""
	fi
else
	warn "$DEBUGFS/dri not found. Is debugfs mounted and CONFIG_DEBUG_FS enabled?"
	warn "  mount it with: mount -t debugfs none /sys/kernel/debug"
fi

# ---- locate tracefs -------------------------------------------------------------
find_tracefs() {
	if [ -n "$TRACEFS" ]; then
		[ -d "$TRACEFS/events" ] && return 0
		warn "DRM_CAPTURE_TRACEFS=$TRACEFS has no events/ directory"
		TRACEFS=""
		return 1
	fi
	for _t in /sys/kernel/tracing /sys/kernel/debug/tracing; do
		if [ -d "$_t/events" ]; then
			TRACEFS=$_t
			return 0
		fi
	done
	return 1
}

if [ "$TRACE_VBLANK" -eq 1 ] || [ "$TRACE_FENCE" -eq 1 ]; then
	if ! find_tracefs; then
		warn "tracefs not found; tracepoints will not be recorded."
		warn "  mount it with: mount -t tracefs nodev /sys/kernel/tracing"
		TRACE_VBLANK=0
		TRACE_FENCE=0
	fi
fi

# ---- debugfs snapshot helper -----------------------------------------------------
# snap_file <src> <dst>: copy a debugfs file if it exists; never fatal.
snap_file() {
	if [ -f "$1" ]; then
		if cat "$1" >"$2" 2>"$2.err"; then
			rm -f "$2.err"
		else
			warn "reading $1 failed (see $(basename "$2").err)"
		fi
	fi
}

snapshot() {
	_tag=$1
	_dst="$OUTDIR/debugfs-$_tag"
	mkdir -p "$_dst"

	if [ -n "$DRI" ]; then
		# Directory listing, so the reader sees what this kernel exposes.
		find "$DRI/" -maxdepth 2 >"$_dst/_listing.txt" 2>&1

		# Generic DRM core files (drm_debugfs.c, drm_atomic.c,
		# drm_framebuffer.c, drm_client.c / drm_client_event.c).
		for _f in name clients internal_clients gem_names framebuffer state; do
			snap_file "$DRI/$_f" "$_dst/$_f"
		done

		# Rockchip BSP (rockchip-linux/kernel develop-5.10 / develop-6.1).
		for _f in summary active_regs regs mm_dump; do
			snap_file "$DRI/$_f" "$_dst/$_f"
		done

		# Rockchip mainline VOP2 (v6.14 and later).
		for _f in summary active_regs; do
			snap_file "$DRI/vop2/$_f" "$_dst/vop2-$_f"
		done

		# Per-CRTC CRC sources. Only "control" is read: opening "data"
		# would start CRC generation.
		for _crc in "$DRI"/crtc-*/crc/control; do
			[ -f "$_crc" ] || continue
			_crtc=$(basename "$(dirname "$(dirname "$_crc")")")
			snap_file "$_crc" "$_dst/$_crtc-crc-control"
		done
	fi

	snap_file "$DEBUGFS/dma_buf/bufinfo" "$_dst/dma_buf-bufinfo"

	_n=$(find "$_dst" -type f ! -name '_listing.txt' | wc -l)
	log "snapshot '$_tag': $_n file(s) saved to $_dst"
}

# ---- trace instance helpers ----------------------------------------------------------
enable_event() {
	# enable_event <system>/<event>  (inside the private instance)
	if [ -f "$TRACE_INST/events/$1/enable" ]; then
		if echo 1 >"$TRACE_INST/events/$1/enable" 2>/dev/null; then
			log "trace: enabled $1"
		else
			warn "trace: could not enable $1"
		fi
	else
		warn "trace: event $1 not available in this kernel"
	fi
}

start_trace() {
	TRACE_INST="$TRACEFS/instances/drm-capture-$$"
	if ! mkdir "$TRACE_INST" 2>/dev/null; then
		warn "cannot create ftrace instance under $TRACEFS/instances; tracing skipped"
		TRACE_INST=""
		return 1
	fi
	echo "$BUF_KB" >"$TRACE_INST/buffer_size_kb" 2>/dev/null ||
		warn "could not set buffer_size_kb=$BUF_KB"
	echo 0 >"$TRACE_INST/tracing_on" 2>/dev/null
	if [ "$TRACE_VBLANK" -eq 1 ]; then
		enable_event drm/drm_vblank_event
		enable_event drm/drm_vblank_event_queued
		enable_event drm/drm_vblank_event_delivered
	fi
	if [ "$TRACE_FENCE" -eq 1 ]; then
		enable_event dma_fence
	fi
	echo 1 >"$TRACE_INST/tracing_on" 2>/dev/null
	return 0
}

stop_trace() {
	[ -n "$TRACE_INST" ] || return 0
	echo 0 >"$TRACE_INST/tracing_on" 2>/dev/null
	if cat "$TRACE_INST/trace" >"$OUTDIR/trace.txt" 2>/dev/null; then
		_ev=$(grep -vc '^#' "$OUTDIR/trace.txt")
		log "trace: $_ev event line(s) saved to $OUTDIR/trace.txt"
		if [ "$TRACE_VBLANK" -eq 1 ]; then
			log "trace: drm_vblank_event lines: $(grep -c 'drm_vblank_event:' "$OUTDIR/trace.txt")"
		fi
	else
		rm -f "$OUTDIR/trace.txt"
		warn "could not read $TRACE_INST/trace"
	fi
	# cleanup() removes the instance directory.
}

# ---- 1. snapshot before ----------------------------------------------------------
snapshot before

# ---- 2. kernel log marker ----------------------------------------------------------
MARKER="drm-debug-capture[$$]: start $(date +%s)"
if [ -w "$KMSG" ] && printf '%s\n' "$MARKER" >"$KMSG" 2>/dev/null; then
	log "kernel log marker: $MARKER"
else
	warn "cannot write marker to $KMSG; the dmesg delta will fall back to a full dmesg"
	MARKER=""
fi

# ---- 3. drm.debug ---------------------------------------------------------------------
if [ "$MASK" = "keep" ]; then
	log "drm.debug left unchanged (-m keep)"
elif [ -f "$PARAM" ]; then
	ORIG_MASK=$(cat "$PARAM" 2>/dev/null)
	if [ -z "$ORIG_MASK" ]; then
		warn "cannot read $PARAM; drm.debug not changed"
	elif printf '%s\n' "$MASK" >"$PARAM" 2>/dev/null; then
		MASK_CHANGED=1
		log "drm.debug: $ORIG_MASK -> $(cat "$PARAM" 2>/dev/null) (requested $MASK)"
	else
		warn "writing $MASK to $PARAM failed; drm.debug not changed"
	fi
else
	warn "$PARAM not found (drm module not loaded?); drm.debug not changed"
fi

# ---- 4. tracing ------------------------------------------------------------------------
if [ "$TRACE_VBLANK" -eq 1 ] || [ "$TRACE_FENCE" -eq 1 ]; then
	start_trace
fi

# ---- 5. the capture window ----------------------------------------------------------------
CMD_STATUS=""
if [ $# -gt 0 ]; then
	log "running: $*"
	"$@"
	CMD_STATUS=$?
	log "command exited with status $CMD_STATUS"
else
	log "capturing for $SECONDS_WIN s (Ctrl-C to stop early)..."
	sleep "$SECONDS_WIN"
fi

# ---- 6. stop tracing, restore drm.debug, snapshot after ----------------------------------------
stop_trace
cleanup
snapshot after

# ---- 7. dmesg delta -------------------------------------------------------------------------
if command -v dmesg >/dev/null 2>&1; then
	if dmesg >"$OUTDIR/dmesg-full.txt" 2>/dev/null; then
		if [ -n "$MARKER" ] && grep -qF "$MARKER" "$OUTDIR/dmesg-full.txt"; then
			# Print every line after the last occurrence of the marker.
			awk -v m="$MARKER" 'index($0, m) { buf = ""; hit = 1; next }
				hit { buf = buf $0 "\n" }
				END { printf "%s", buf }' \
				"$OUTDIR/dmesg-full.txt" >"$OUTDIR/dmesg-delta.txt"
			rm -f "$OUTDIR/dmesg-full.txt"
			log "dmesg delta: $(wc -l <"$OUTDIR/dmesg-delta.txt") line(s)," \
				"$(grep -c '\[drm' "$OUTDIR/dmesg-delta.txt") containing '[drm'"
		else
			[ -n "$MARKER" ] &&
				warn "marker not found in dmesg (ring buffer overflow? consider log_buf_len=)"
			log "full dmesg saved to $OUTDIR/dmesg-full.txt"
		fi
	else
		warn "dmesg failed (kernel.dmesg_restrict?)"
	fi
else
	warn "dmesg not found"
fi

log "done. Files in $OUTDIR:"
(cd "$OUTDIR" && find . -type f | sort) | while read -r _f; do log "    $_f"; done

if [ -n "$CMD_STATUS" ]; then
	exit "$CMD_STATUS"
fi
exit 0
