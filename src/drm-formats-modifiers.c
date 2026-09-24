#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

/* ============================================================
 * DRM Pixel Formats & Format Modifiers Demo
 *
 * A framebuffer is more than "width x height x bpp".  The kernel
 * describes every scanout buffer with two independent pieces of
 * metadata:
 *
 *   fourcc   -- WHAT the pixels are (XR24, RG16, NV12, ...), i.e.
 *               channel order, bit depth, number of memory planes
 *               and chroma subsampling.
 *   modifier -- HOW those pixels are laid out in memory (linear,
 *               tiled, compressed such as ARM AFBC, ...).
 *
 * A plane can only scan out a (format, modifier) pair it supports.
 * The plane's "IN_FORMATS" blob property is the kernel's answer to
 * "which pairs does this plane accept?".
 *
 * Progression from previous demos:
 *   modeset-double-buffer.c    -- GEM / KMS basics, XRGB8888 only
 *   drm-atomic-demo.c          -- atomic commit, properties, planes
 *   drm-dmabuf-fence.c         -- DMA-BUF sharing, implicit/explicit fence
 *   drm-formats-modifiers.c    -- fourcc formats, IN_FORMATS modifiers,
 *                                 multi-planar YUV (NV12) scanout
 *
 * Runnable modes:
 *   (default) / --list   Per plane: fourcc list + format x modifier
 *                        matrix parsed by hand from the IN_FORMATS blob
 *   --nv12               Show BT.601 limited-range colour bars from an
 *                        NV12 dumb buffer via an atomic commit
 *                        (TEST_ONLY first), then restore the plane
 *   --selftest           Host-only checks of the blob parser, the
 *                        modifier decoder and the YCbCr maths (no
 *                        DRM device needed)
 *
 * Target: RK3588 / VOP2 with Ubuntu Lite (no compositor).
 * Not yet verified on hardware.
 * ============================================================ */

/* ============================================================
 * Modifier constants
 *
 * The values below come from include/uapi/drm/drm_fourcc.h (kernel
 * master).  The board's libdrm may ship an older copy of drm_fourcc.h
 * that lacks some of them (e.g. AFBC_FORMAT_MOD_USM or the MTK/APPLE
 * vendor IDs are missing in older libdrm releases), so each one is
 * guarded and only defined here if the header did not provide it.
 * ============================================================ */
#ifndef DRM_FORMAT_MOD_VENDOR_MTK
#define DRM_FORMAT_MOD_VENDOR_MTK	0x0b
#endif
#ifndef DRM_FORMAT_MOD_VENDOR_APPLE
#define DRM_FORMAT_MOD_VENDOR_APPLE	0x0c
#endif
#ifndef DRM_FORMAT_MOD_VENDOR_AMLOGIC
#define DRM_FORMAT_MOD_VENDOR_AMLOGIC	0x0a
#endif
#ifndef DRM_FORMAT_MOD_VENDOR_ALLWINNER
#define DRM_FORMAT_MOD_VENDOR_ALLWINNER	0x09
#endif
#ifndef DRM_FORMAT_RESERVED
#define DRM_FORMAT_RESERVED		((1ULL << 56) - 1)
#endif

/* ARM modifiers: bits 55:52 of the 56-bit vendor payload = category */
#ifndef DRM_FORMAT_MOD_ARM_TYPE_AFBC
#define DRM_FORMAT_MOD_ARM_TYPE_AFBC	0x00
#endif
#ifndef DRM_FORMAT_MOD_ARM_TYPE_MISC
#define DRM_FORMAT_MOD_ARM_TYPE_MISC	0x01
#endif
#ifndef DRM_FORMAT_MOD_ARM_TYPE_AFRC
#define DRM_FORMAT_MOD_ARM_TYPE_AFRC	0x02
#endif

/* AFBC flag bits (lower bits of the ARM AFBC payload) */
#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_MASK
#define AFBC_FORMAT_MOD_BLOCK_SIZE_MASK		0xf
#endif
#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_16x16
#define AFBC_FORMAT_MOD_BLOCK_SIZE_16x16	(1ULL)
#endif
#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_32x8
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8		(2ULL)
#endif
#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_64x4
#define AFBC_FORMAT_MOD_BLOCK_SIZE_64x4		(3ULL)
#endif
#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4	(4ULL)
#endif
#ifndef AFBC_FORMAT_MOD_YTR
#define AFBC_FORMAT_MOD_YTR	(1ULL << 4)
#endif
#ifndef AFBC_FORMAT_MOD_SPLIT
#define AFBC_FORMAT_MOD_SPLIT	(1ULL << 5)
#endif
#ifndef AFBC_FORMAT_MOD_SPARSE
#define AFBC_FORMAT_MOD_SPARSE	(1ULL << 6)
#endif
#ifndef AFBC_FORMAT_MOD_CBR
#define AFBC_FORMAT_MOD_CBR	(1ULL << 7)
#endif
#ifndef AFBC_FORMAT_MOD_TILED
#define AFBC_FORMAT_MOD_TILED	(1ULL << 8)
#endif
#ifndef AFBC_FORMAT_MOD_SC
#define AFBC_FORMAT_MOD_SC	(1ULL << 9)
#endif
#ifndef AFBC_FORMAT_MOD_DB
#define AFBC_FORMAT_MOD_DB	(1ULL << 10)
#endif
#ifndef AFBC_FORMAT_MOD_BCH
#define AFBC_FORMAT_MOD_BCH	(1ULL << 11)
#endif
#ifndef AFBC_FORMAT_MOD_USM
#define AFBC_FORMAT_MOD_USM	(1ULL << 12)
#endif

/* AFRC fields */
#define AFRC_CU_SIZE_P0(v)	((unsigned)((v) & 0xf))
#define AFRC_CU_SIZE_P12(v)	((unsigned)(((v) >> 4) & 0xf))
#define AFRC_LAYOUT_SCAN_BIT	((uint64_t)1 << 8)
#define AFRC_KNOWN_BITS		((uint64_t)0xff | AFRC_LAYOUT_SCAN_BIT)

/* Our own helpers for the bit fields documented in drm_fourcc.h */
#define MOD_VENDOR(m)		((unsigned)(((m) >> 56) & 0xff))   /* == fourcc_mod_get_vendor() */
#define MOD_PAYLOAD(m)		((uint64_t)((m) & 0x00ffffffffffffffULL))
#define MOD_ARM_TYPE(m)		((unsigned)(((m) >> 52) & 0xf))
#define MOD_ARM_VALUE(m)	((uint64_t)((m) & 0x000fffffffffffffULL))

/* Mirrors of the kernel's FORMAT_BLOB_CURRENT (drm_mode.h) */
#ifndef FORMAT_BLOB_CURRENT
#define FORMAT_BLOB_CURRENT 1
#endif

#define DEFAULT_DEVICE "/dev/dri/card0"
#define DEFAULT_HOLD_SECONDS 10

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

/*
 * libdrm's mode helpers (drmModeAddFB2, drmModeAtomicCommit, ...)
 * return -errno.  Raw drmIoctl() returns -1 and sets errno.  This
 * turns either convention into a printable string.
 */
static const char *err_str(int ret)
{
	if (ret == -1)
		return strerror(errno);
	return strerror(ret < 0 ? -ret : ret);
}

/* ============================================================
 * fourcc_str - Render a DRM fourcc as its 4 ASCII characters.
 *
 * fourcc_code(a,b,c,d) packs a in bits 7:0, b in 15:8, c in 23:16
 * and d in 31:24.  Bit 31 doubles as DRM_FORMAT_BIG_ENDIAN, so it is
 * masked off before decoding the last character.
 * ============================================================ */
static void fourcc_str(uint32_t fmt, char out[16])
{
	uint32_t code = fmt & ~DRM_FORMAT_BIG_ENDIAN;
	char c[4];

	for (int i = 0; i < 4; i++) {
		char ch = (char)((code >> (8 * i)) & 0xff);
		c[i] = (ch >= 0x20 && ch < 0x7f) ? ch : '?';
	}
	snprintf(out, 16, "%c%c%c%c%s", c[0], c[1], c[2], c[3],
		 (fmt & DRM_FORMAT_BIG_ENDIAN) ? "(BE)" : "");
}

/* Small append-to-buffer helper used by the modifier decoder */
static void bappend(char *buf, size_t len, size_t *pos, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

static void bappend(char *buf, size_t len, size_t *pos, const char *fmt, ...)
{
	va_list ap;
	int n;

	if (*pos >= len)
		return;
	va_start(ap, fmt);
	n = vsnprintf(buf + *pos, len - *pos, fmt, ap);
	va_end(ap);
	if (n > 0)
		*pos += (size_t)n;
	if (*pos >= len)
		*pos = len - 1;
}

/* ============================================================
 * Modifier vendor table
 *
 * The top 8 bits of a modifier are the vendor ID
 * (fourcc_mod_get_vendor() in drm_fourcc.h).  The remaining 56 bits
 * are vendor-defined.  Values as in include/uapi/drm/drm_fourcc.h.
 * ============================================================ */
static const char *mod_vendor_name(unsigned vendor)
{
	switch (vendor) {
	case DRM_FORMAT_MOD_VENDOR_NONE:      return "NONE";
	case DRM_FORMAT_MOD_VENDOR_INTEL:     return "INTEL";
	case DRM_FORMAT_MOD_VENDOR_AMD:       return "AMD";
	case DRM_FORMAT_MOD_VENDOR_NVIDIA:    return "NVIDIA";
	case DRM_FORMAT_MOD_VENDOR_SAMSUNG:   return "SAMSUNG";
	case DRM_FORMAT_MOD_VENDOR_QCOM:      return "QCOM";
	case DRM_FORMAT_MOD_VENDOR_VIVANTE:   return "VIVANTE";
	case DRM_FORMAT_MOD_VENDOR_BROADCOM:  return "BROADCOM";
	case DRM_FORMAT_MOD_VENDOR_ARM:       return "ARM";
	case DRM_FORMAT_MOD_VENDOR_ALLWINNER: return "ALLWINNER";
	case DRM_FORMAT_MOD_VENDOR_AMLOGIC:   return "AMLOGIC";
	case DRM_FORMAT_MOD_VENDOR_MTK:       return "MTK";
	case DRM_FORMAT_MOD_VENDOR_APPLE:     return "APPLE";
	default:                              return NULL;
	}
}

/* ============================================================
 * describe_modifier - Human-readable decoding of a 64-bit modifier.
 *
 * We deliberately do NOT call libdrm's drmGetFormatModifierName():
 * it is absent from libdrm 2.4.101 and present in 2.4.107 (checked
 * against the release tarballs), and the board's libdrm version is
 * unknown.  Decoding by hand also makes the bit layout visible.
 *
 *   63........56 55.......52 51..................................0
 *   [ vendor   ] [ ARM type ] [ type-specific value (AFBC flags) ]
 *                 ^-- only for vendor ARM (DRM_FORMAT_MOD_ARM_CODE)
 * ============================================================ */
static void describe_modifier(uint64_t mod, char *buf, size_t len)
{
	size_t pos = 0;
	unsigned vendor = MOD_VENDOR(mod);
	const char *vname = mod_vendor_name(vendor);

	buf[0] = '\0';

	if (mod == DRM_FORMAT_MOD_LINEAR) {
		bappend(buf, len, &pos, "LINEAR");
		return;
	}
	if (mod == DRM_FORMAT_MOD_INVALID) {
		bappend(buf, len, &pos, "INVALID (implicit/unknown layout)");
		return;
	}
	if (!vname) {
		bappend(buf, len, &pos, "vendor 0x%02x (unknown), value 0x%014" PRIx64,
			vendor, MOD_PAYLOAD(mod));
		return;
	}
	if (vendor != DRM_FORMAT_MOD_VENDOR_ARM) {
		bappend(buf, len, &pos, "%s, value 0x%014" PRIx64
			" (vendor-specific, not decoded here)",
			vname, MOD_PAYLOAD(mod));
		return;
	}

	/* ---- ARM ---- */
	unsigned type = MOD_ARM_TYPE(mod);
	uint64_t v = MOD_ARM_VALUE(mod);

	if (type == DRM_FORMAT_MOD_ARM_TYPE_AFBC) {
		static const struct { uint64_t bit; const char *name; } flags[] = {
			{ AFBC_FORMAT_MOD_YTR,    "YTR"    },
			{ AFBC_FORMAT_MOD_SPLIT,  "SPLIT"  },
			{ AFBC_FORMAT_MOD_SPARSE, "SPARSE" },
			{ AFBC_FORMAT_MOD_CBR,    "CBR"    },
			{ AFBC_FORMAT_MOD_TILED,  "TILED"  },
			{ AFBC_FORMAT_MOD_SC,     "SC"     },
			{ AFBC_FORMAT_MOD_DB,     "DB"     },
			{ AFBC_FORMAT_MOD_BCH,    "BCH"    },
			{ AFBC_FORMAT_MOD_USM,    "USM"    },
		};
		uint64_t known = AFBC_FORMAT_MOD_BLOCK_SIZE_MASK;
		const char *bs;

		switch (v & AFBC_FORMAT_MOD_BLOCK_SIZE_MASK) {
		case AFBC_FORMAT_MOD_BLOCK_SIZE_16x16:     bs = "16x16"; break;
		case AFBC_FORMAT_MOD_BLOCK_SIZE_32x8:      bs = "32x8"; break;
		case AFBC_FORMAT_MOD_BLOCK_SIZE_64x4:      bs = "64x4"; break;
		case AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4: bs = "32x8_64x4"; break;
		default:                                   bs = "block?"; break;
		}
		bappend(buf, len, &pos, "ARM AFBC(%s", bs);
		for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
			known |= flags[i].bit;
			if (v & flags[i].bit)
				bappend(buf, len, &pos, "|%s", flags[i].name);
		}
		if (v & ~known)
			bappend(buf, len, &pos, "|unknown:0x%" PRIx64, v & ~known);
		bappend(buf, len, &pos, ")");
		return;
	}

	if (type == DRM_FORMAT_MOD_ARM_TYPE_AFRC) {
		static const char *cu[] = { "-", "16", "24", "32" };
		unsigned p0 = AFRC_CU_SIZE_P0(v), p12 = AFRC_CU_SIZE_P12(v);

		bappend(buf, len, &pos, "ARM AFRC(CU_P0=%s CU_P12=%s %s",
			p0 < 4 ? cu[p0] : "?", p12 < 4 ? cu[p12] : "?",
			(v & AFRC_LAYOUT_SCAN_BIT) ? "SCAN" : "ROT");
		if (v & ~AFRC_KNOWN_BITS)
			bappend(buf, len, &pos, " unknown:0x%" PRIx64,
				(uint64_t)(v & ~AFRC_KNOWN_BITS));
		bappend(buf, len, &pos, ")");
		return;
	}

	if (type == DRM_FORMAT_MOD_ARM_TYPE_MISC) {
		if (v == 1)
			bappend(buf, len, &pos, "ARM 16X16_BLOCK_U_INTERLEAVED");
		else if (v == 2)
			bappend(buf, len, &pos, "ARM INTERLEAVED_64K");
		else
			bappend(buf, len, &pos, "ARM MISC value 0x%" PRIx64, v);
		return;
	}

	bappend(buf, len, &pos, "ARM type %u value 0x%" PRIx64, type, v);
}

/* ============================================================
 * IN_FORMATS blob parsing
 *
 * The kernel builds the blob in create_in_format_blob()
 * (drivers/gpu/drm/drm_plane.c) with this layout
 * (struct drm_format_modifier_blob, include/uapi/drm/drm_mode.h):
 *
 *   offset 0                 struct drm_format_modifier_blob (24 bytes)
 *                              version, flags,
 *                              count_formats,   formats_offset,
 *                              count_modifiers, modifiers_offset
 *   formats_offset           __u32 formats[count_formats]
 *   (padded to 8 bytes)
 *   modifiers_offset         struct drm_format_modifier[count_modifiers]
 *                              __u64 formats;  bitmask over formats[]
 *                              __u32 offset;   bit 0 == formats[offset]
 *                              __u32 pad;
 *                              __u64 modifier;
 *
 * So each modifier carries a 64-bit window of "which formats in the
 * list above can use me".  We never trust the offsets blindly: every
 * range is checked against the blob length, and the arrays are
 * memcpy()d out so no unaligned pointer is ever dereferenced.
 * ============================================================ */
struct in_formats {
	uint32_t version;
	uint32_t count_formats;
	uint32_t *formats;
	uint32_t count_modifiers;
	struct drm_format_modifier *mods;
};

static void in_formats_free(struct in_formats *inf)
{
	free(inf->formats);
	free(inf->mods);
	memset(inf, 0, sizeof(*inf));
}

static int parse_in_formats_blob(const void *data, size_t length,
				 struct in_formats *out,
				 char *err, size_t errlen)
{
	struct drm_format_modifier_blob hdr;
	uint64_t fmt_end, mod_end;

	memset(out, 0, sizeof(*out));

	if (!data || length < sizeof(hdr)) {
		snprintf(err, errlen, "blob too small (%zu bytes)", length);
		return -1;
	}
	memcpy(&hdr, data, sizeof(hdr));

	if (hdr.version != FORMAT_BLOB_CURRENT) {
		snprintf(err, errlen, "unknown blob version %u (expected %u)",
			 hdr.version, FORMAT_BLOB_CURRENT);
		return -1;
	}

	fmt_end = (uint64_t)hdr.formats_offset +
		  (uint64_t)hdr.count_formats * sizeof(uint32_t);
	mod_end = (uint64_t)hdr.modifiers_offset +
		  (uint64_t)hdr.count_modifiers * sizeof(struct drm_format_modifier);
	if (fmt_end > length || mod_end > length) {
		snprintf(err, errlen,
			 "offsets out of range (formats end %" PRIu64
			 ", modifiers end %" PRIu64 ", blob %zu bytes)",
			 fmt_end, mod_end, length);
		return -1;
	}

	out->version = hdr.version;
	out->count_formats = hdr.count_formats;
	out->count_modifiers = hdr.count_modifiers;

	if (hdr.count_formats) {
		out->formats = calloc(hdr.count_formats, sizeof(uint32_t));
		if (!out->formats)
			goto nomem;
		memcpy(out->formats, (const uint8_t *)data + hdr.formats_offset,
		       hdr.count_formats * sizeof(uint32_t));
	}
	if (hdr.count_modifiers) {
		out->mods = calloc(hdr.count_modifiers,
				   sizeof(struct drm_format_modifier));
		if (!out->mods)
			goto nomem;
		memcpy(out->mods, (const uint8_t *)data + hdr.modifiers_offset,
		       hdr.count_modifiers * sizeof(struct drm_format_modifier));
	}
	return 0;

nomem:
	snprintf(err, errlen, "out of memory");
	in_formats_free(out);
	return -1;
}

/* Is formats[fmt_idx] usable with mods[mod_idx]? */
static bool in_formats_bit(const struct in_formats *inf,
			   uint32_t mod_idx, uint32_t fmt_idx)
{
	const struct drm_format_modifier *m = &inf->mods[mod_idx];

	if (fmt_idx < m->offset || fmt_idx - m->offset >= 64)
		return false;
	return (m->formats >> (fmt_idx - m->offset)) & 1;
}

static bool in_formats_supports(const struct in_formats *inf,
				uint32_t format, uint64_t modifier)
{
	for (uint32_t j = 0; j < inf->count_formats; j++) {
		if (inf->formats[j] != format)
			continue;
		for (uint32_t i = 0; i < inf->count_modifiers; i++)
			if (inf->mods[i].modifier == modifier &&
			    in_formats_bit(inf, i, j))
				return true;
	}
	return false;
}

/* ============================================================
 * Property helpers (same idea as drm-atomic-demo.c, but returning
 * the current value as well as the ID)
 * ============================================================ */
static bool find_prop(int fd, drmModeObjectProperties *props,
		      const char *name, uint32_t *id, uint64_t *value)
{
	for (uint32_t i = 0; i < props->count_props; i++) {
		drmModePropertyRes *p = drmModeGetProperty(fd, props->props[i]);
		if (!p)
			continue;
		bool hit = strcmp(p->name, name) == 0;
		drmModeFreeProperty(p);
		if (hit) {
			if (id)
				*id = props->props[i];
			if (value)
				*value = props->prop_values[i];
			return true;
		}
	}
	return false;
}

/* Look up the numeric value of an enum entry by its name */
static bool find_enum_value(int fd, uint32_t prop_id, const char *enum_name,
			    uint64_t *value)
{
	drmModePropertyRes *p = drmModeGetProperty(fd, prop_id);
	bool found = false;

	if (!p)
		return false;
	if (p->flags & DRM_MODE_PROP_ENUM) {
		for (int i = 0; i < p->count_enums; i++) {
			if (strcmp(p->enums[i].name, enum_name) == 0) {
				*value = p->enums[i].value;
				found = true;
				break;
			}
		}
	}
	drmModeFreeProperty(p);
	return found;
}

static const char *enum_name_of(int fd, uint32_t prop_id, uint64_t value,
				char *buf, size_t len)
{
	drmModePropertyRes *p = drmModeGetProperty(fd, prop_id);

	snprintf(buf, len, "%" PRIu64, value);
	if (!p)
		return buf;
	for (int i = 0; i < p->count_enums; i++)
		if (p->enums[i].value == value)
			snprintf(buf, len, "%s", p->enums[i].name);
	drmModeFreeProperty(p);
	return buf;
}

static const char *plane_type_str(uint64_t type)
{
	switch (type) {
	case DRM_PLANE_TYPE_PRIMARY: return "PRIMARY";
	case DRM_PLANE_TYPE_OVERLAY: return "OVERLAY";
	case DRM_PLANE_TYPE_CURSOR:  return "CURSOR";
	default:                     return "UNKNOWN";
	}
}

/* Fetch and parse a plane's IN_FORMATS-style blob property. */
static int load_in_formats(int fd, drmModeObjectProperties *props,
			   const char *prop_name, struct in_formats *inf,
			   uint32_t *blob_id_out)
{
	uint64_t blob_id = 0;
	char err[160];

	if (!find_prop(fd, props, prop_name, NULL, &blob_id) || blob_id == 0)
		return 1; /* property absent: not an error */

	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, (uint32_t)blob_id);
	if (!blob) {
		fprintf(stderr, "    drmModeGetPropertyBlob(%" PRIu64 "): %s\n",
			blob_id, strerror(errno));
		return -1;
	}
	int ret = parse_in_formats_blob(blob->data, blob->length, inf,
					err, sizeof(err));
	if (ret)
		fprintf(stderr, "    %s blob %" PRIu64 ": %s\n",
			prop_name, blob_id, err);
	drmModeFreePropertyBlob(blob);
	if (blob_id_out)
		*blob_id_out = (uint32_t)blob_id;
	return ret;
}

/* ============================================================
 * print_in_formats_matrix - Print the modifier legend and the
 * format x modifier matrix ("x" = supported, "." = not).
 * ============================================================ */
static void print_in_formats_matrix(const struct in_formats *inf,
				    const char *indent)
{
	char desc[160], fcc[16];

	printf("%sModifiers (%u):\n", indent, inf->count_modifiers);
	for (uint32_t i = 0; i < inf->count_modifiers; i++) {
		describe_modifier(inf->mods[i].modifier, desc, sizeof(desc));
		printf("%s  M%-2u 0x%016" PRIx64 "  vendor=%-9s %s\n",
		       indent, i, (uint64_t)inf->mods[i].modifier,
		       mod_vendor_name(MOD_VENDOR(inf->mods[i].modifier)) ?
		       mod_vendor_name(MOD_VENDOR(inf->mods[i].modifier)) : "?",
		       desc);
		if (inf->mods[i].offset)
			printf("%s      (bitmask window offset=%u)\n",
			       indent, inf->mods[i].offset);
	}

	printf("%s%-10s", indent, "format");
	for (uint32_t i = 0; i < inf->count_modifiers; i++)
		printf(" M%-2u", i);
	printf("\n");

	for (uint32_t j = 0; j < inf->count_formats; j++) {
		bool any = false;

		fourcc_str(inf->formats[j], fcc);
		printf("%s%-10s", indent, fcc);
		for (uint32_t i = 0; i < inf->count_modifiers; i++) {
			bool on = in_formats_bit(inf, i, j);
			any |= on;
			printf("  %s ", on ? "x" : ".");
		}
		/*
		 * A format with no modifier at all is legal in the blob
		 * (libdrm's iterator skips such entries); flag it.
		 */
		printf("%s\n", any ? "" : "   <- no modifier listed");
	}
}

/* ============================================================
 * run_list - Default mode: dump formats and modifiers per plane.
 * ============================================================ */
static int run_list(int fd)
{
	uint64_t cap = 0;
	char fcc[16];

	if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1))
		fprintf(stderr, "warning: DRM_CLIENT_CAP_UNIVERSAL_PLANES failed: %s "
			"(primary/cursor planes will be hidden)\n", strerror(errno));

	/*
	 * DRM_CAP_ADDFB2_MODIFIERS tells whether ADDFB2 accepts the
	 * DRM_MODE_FB_MODIFIERS flag at all.  Per the IN_FORMATS doc in
	 * drm_plane.c, when it is set every plane exposes IN_FORMATS
	 * (reliably so since v5.1).
	 */
	if (drmGetCap(fd, DRM_CAP_ADDFB2_MODIFIERS, &cap) == 0)
		printf("DRM_CAP_ADDFB2_MODIFIERS = %" PRIu64 "\n", cap);
	else
		printf("DRM_CAP_ADDFB2_MODIFIERS: query failed (%s)\n",
		       strerror(errno));

	drmModePlaneRes *pres = drmModeGetPlaneResources(fd);
	if (!pres) {
		fprintf(stderr, "drmModeGetPlaneResources: %s\n", strerror(errno));
		return -1;
	}

	printf("%u planes\n", pres->count_planes);

	for (uint32_t i = 0; i < pres->count_planes; i++) {
		drmModePlane *pl = drmModeGetPlane(fd, pres->planes[i]);
		if (!pl) {
			fprintf(stderr, "drmModeGetPlane(%u): %s\n",
				pres->planes[i], strerror(errno));
			continue;
		}
		drmModeObjectProperties *props =
			drmModeObjectGetProperties(fd, pl->plane_id,
						   DRM_MODE_OBJECT_PLANE);
		uint64_t type = ~0ULL, zpos = 0;
		uint32_t enc_id = 0, rng_id = 0;
		uint64_t enc_val = 0, rng_val = 0;
		bool has_zpos = false, has_enc = false, has_rng = false;

		if (props) {
			find_prop(fd, props, "type", NULL, &type);
			has_zpos = find_prop(fd, props, "zpos", NULL, &zpos);
			has_enc = find_prop(fd, props, "COLOR_ENCODING", &enc_id, &enc_val);
			has_rng = find_prop(fd, props, "COLOR_RANGE", &rng_id, &rng_val);
		}

		printf("\n============================================================\n");
		printf("Plane id=%u  type=%s  possible_crtcs=0x%x  crtc_id=%u  fb_id=%u",
		       pl->plane_id, plane_type_str(type), pl->possible_crtcs,
		       pl->crtc_id, pl->fb_id);
		if (has_zpos)
			printf("  zpos=%" PRIu64, zpos);
		printf("\n");
		if (has_enc || has_rng) {
			char a[48], b[48];
			printf("  COLOR_ENCODING=%s  COLOR_RANGE=%s\n",
			       has_enc ? enum_name_of(fd, enc_id, enc_val, a, sizeof(a)) : "(absent)",
			       has_rng ? enum_name_of(fd, rng_id, rng_val, b, sizeof(b)) : "(absent)");
		} else {
			printf("  COLOR_ENCODING/COLOR_RANGE: absent "
			       "(driver chooses the YCbCr->RGB matrix itself)\n");
		}

		/* 1. The classic list from DRM_IOCTL_MODE_GETPLANE */
		printf("  drmModePlane->formats (%u):", pl->count_formats);
		for (uint32_t f = 0; f < pl->count_formats; f++) {
			if (f % 10 == 0)
				printf("\n    ");
			fourcc_str(pl->formats[f], fcc);
			printf("%-6s", fcc);
		}
		printf("\n");

		/* 2. IN_FORMATS: the same list plus modifiers */
		if (props) {
			struct in_formats inf;
			uint32_t blob_id = 0;
			int r = load_in_formats(fd, props, "IN_FORMATS", &inf, &blob_id);

			if (r == 1) {
				printf("  IN_FORMATS: absent -> plane takes only buffers "
				       "without explicit modifiers\n");
			} else if (r == 0) {
				printf("  IN_FORMATS blob id=%u: version %u, %u formats x %u modifiers\n",
				       blob_id, inf.version, inf.count_formats,
				       inf.count_modifiers);
				if (inf.count_formats != pl->count_formats)
					printf("  NOTE: blob lists %u formats but GETPLANE "
					       "lists %u\n", inf.count_formats,
					       pl->count_formats);
				print_in_formats_matrix(&inf, "    ");
				in_formats_free(&inf);
			}

			/* Newer kernels may also expose a list for async flips */
			r = load_in_formats(fd, props, "IN_FORMATS_ASYNC", &inf, &blob_id);
			if (r == 0) {
				printf("  IN_FORMATS_ASYNC blob id=%u: %u formats x %u modifiers\n",
				       blob_id, inf.count_formats, inf.count_modifiers);
				print_in_formats_matrix(&inf, "    ");
				in_formats_free(&inf);
			}
			drmModeFreeObjectProperties(props);
		}
		drmModeFreePlane(pl);
	}
	drmModeFreePlaneResources(pres);
	return 0;
}

/* ============================================================
 * YCbCr colour maths
 *
 * Full-range R'G'B' in [0,1] -> 8-bit limited-range Y'CbCr:
 *
 *   Y'  = Kr*R' + (1-Kr-Kb)*G' + Kb*B'
 *   Cb' = (B' - Y') / (2*(1-Kb))          in [-0.5, 0.5]
 *   Cr' = (R' - Y') / (2*(1-Kr))          in [-0.5, 0.5]
 *
 *   Y   = 16  + 219 * Y'                  in [16, 235]
 *   Cb  = 128 + 224 * Cb'                 in [16, 240]
 *   Cr  = 128 + 224 * Cr'                 in [16, 240]
 *
 * BT.601: Kr = 0.299,  Kb = 0.114  -> Y' = 0.299R' + 0.587G' + 0.114B'
 * BT.709: Kr = 0.2126, Kb = 0.0722 -> Y' = 0.2126R' + 0.7152G' + 0.0722B'
 *
 * Expanded for BT.601 this gives the familiar
 *   Cb' = -0.1687R' - 0.3313G' + 0.5B',  Cr' = 0.5R' - 0.4187G' - 0.0813B'
 * (see Documentation/userspace-api/media/v4l/colorspaces-details.rst).
 * ============================================================ */
struct ycbcr { uint8_t y, cb, cr; };

enum matrix { MATRIX_BT601, MATRIX_BT709 };

static uint8_t clamp_round(double v)
{
	if (v < 0.0)
		return 0;
	if (v > 255.0)
		return 255;
	return (uint8_t)(v + 0.5);
}

static struct ycbcr rgb_to_ycbcr_limited(double r, double g, double b,
					 enum matrix m)
{
	double kr = (m == MATRIX_BT709) ? 0.2126 : 0.299;
	double kb = (m == MATRIX_BT709) ? 0.0722 : 0.114;
	double y  = kr * r + (1.0 - kr - kb) * g + kb * b;
	double cb = (b - y) / (2.0 * (1.0 - kb));
	double cr = (r - y) / (2.0 * (1.0 - kr));
	struct ycbcr out = {
		.y  = clamp_round(16.0 + 219.0 * y),
		.cb = clamp_round(128.0 + 224.0 * cb),
		.cr = clamp_round(128.0 + 224.0 * cr),
	};
	return out;
}

/* 100% SMPTE-style bar order */
static const struct { const char *name; double r, g, b; } bars[8] = {
	{ "white",   1, 1, 1 }, { "yellow", 1, 1, 0 },
	{ "cyan",    0, 1, 1 }, { "green",  0, 1, 0 },
	{ "magenta", 1, 0, 1 }, { "red",    1, 0, 0 },
	{ "blue",    0, 0, 1 }, { "black",  0, 0, 0 },
};

/* ============================================================
 * fill_nv12 - Paint colour bars (top 3/4) and a luma ramp (bottom 1/4).
 *
 * NV12 memory layout (DRM_FORMAT_NV12 in drm_fourcc.h: "index 0 = Y
 * plane, index 1 = Cr:Cb plane, [15:0] Cr:Cb little endian"):
 *
 *   base + 0             Y plane:  h rows of w bytes (row stride = pitch)
 *   base + pitch*h       CbCr plane: h/2 rows of w/2 (Cb,Cr) byte pairs
 *                        -- Cb at the lower address, Cr right after it
 *
 * One CbCr pair is shared by a 2x2 block of luma samples (4:2:0).
 *
 * The luma ramp runs over the FULL code range 0..255 with neutral
 * chroma.  A display pipeline that decodes limited range maps 16 to
 * black and 235 to white, so the outer ~6% at each end of the ramp
 * should look flat (clipped).  If they don't, the pipeline is treating
 * the data as full range.
 * ============================================================ */
static void fill_nv12(uint8_t *base, uint32_t w, uint32_t h, uint32_t pitch,
		      enum matrix m)
{
	struct ycbcr c[8];
	uint32_t bars_h = (h * 3 / 4) & ~1u;
	uint8_t *yp = base;
	uint8_t *uvp = base + (size_t)pitch * h;

	for (int i = 0; i < 8; i++)
		c[i] = rgb_to_ycbcr_limited(bars[i].r, bars[i].g, bars[i].b, m);

	/* Luma plane */
	for (uint32_t y = 0; y < h; y++) {
		uint8_t *row = yp + (size_t)y * pitch;
		for (uint32_t x = 0; x < w; x++) {
			if (y < bars_h)
				row[x] = c[(x * 8) / w].y;
			else
				row[x] = (uint8_t)((x * 255u) / (w - 1));
		}
	}

	/* Interleaved chroma plane: one (Cb, Cr) byte pair per 2x2 block */
	for (uint32_t cy = 0; cy < h / 2; cy++) {
		uint8_t *row = uvp + (size_t)cy * pitch;
		for (uint32_t cx = 0; cx < w / 2; cx++) {
			uint8_t cb = 128, cr = 128;
			if (cy * 2 < bars_h) {
				int bar = (int)((cx * 2 * 8) / w);
				cb = c[bar].cb;
				cr = c[bar].cr;
			}
			row[cx * 2 + 0] = cb;
			row[cx * 2 + 1] = cr;
		}
	}
}

/* ============================================================
 * NV12 demo state
 * ============================================================ */
#define MAX_SAVED_PROPS 16

struct saved_prop {
	uint32_t id;
	uint64_t value;
	const char *name;
};

struct nv12_demo {
	int fd;
	uint32_t conn_id, crtc_id, crtc_idx, plane_id;
	uint64_t plane_type;
	drmModeModeInfo mode;
	bool need_modeset;
	uint32_t mode_blob;

	/* property IDs */
	uint32_t p_fb, p_crtc, p_src_x, p_src_y, p_src_w, p_src_h;
	uint32_t p_crtc_x, p_crtc_y, p_crtc_w, p_crtc_h;
	uint32_t p_zpos, p_enc, p_range;
	uint64_t zpos_max;
	bool zpos_mutable;
	uint32_t c_active, c_mode_id, k_crtc_id;

	/* state captured before we touch anything, for restore */
	struct saved_prop saved[MAX_SAVED_PROPS];
	int n_saved;
	uint64_t saved_active, saved_mode_id, saved_conn_crtc;

	/* the NV12 buffer */
	uint32_t w, h, pitch, handle, fb_id;
	uint64_t size;
	uint8_t *map;
};

static void save_plane_prop(struct nv12_demo *d, drmModeObjectProperties *props,
			    const char *name, uint32_t *id_out)
{
	uint32_t id = 0;
	uint64_t val = 0;

	if (!find_prop(d->fd, props, name, &id, &val))
		return;
	if (id_out)
		*id_out = id;
	if (d->n_saved < MAX_SAVED_PROPS) {
		d->saved[d->n_saved].id = id;
		d->saved[d->n_saved].value = val;
		d->saved[d->n_saved].name = name;
		d->n_saved++;
	}
}

/* ============================================================
 * pick_pipeline - Connector -> CRTC, and the current mode.
 *
 * If the connector is already lit (e.g. by fbcon), keep its CRTC and
 * mode: then no modeset is needed and we only touch one plane.
 * ============================================================ */
static int pick_pipeline(struct nv12_demo *d, drmModeRes *res)
{
	drmModeConnector *conn = NULL;

	for (int i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(d->fd, res->connectors[i]);
		if (conn && conn->connection == DRM_MODE_CONNECTED &&
		    conn->count_modes > 0)
			break;
		drmModeFreeConnector(conn);
		conn = NULL;
	}
	if (!conn) {
		fprintf(stderr, "No connected connector with modes\n");
		return -1;
	}
	d->conn_id = conn->connector_id;

	/* Prefer the CRTC currently driving this connector */
	if (conn->encoder_id) {
		drmModeEncoder *enc = drmModeGetEncoder(d->fd, conn->encoder_id);
		if (enc) {
			d->crtc_id = enc->crtc_id;
			drmModeFreeEncoder(enc);
		}
	}
	/* Otherwise the first CRTC any of its encoders can use */
	for (int e = 0; !d->crtc_id && e < conn->count_encoders; e++) {
		drmModeEncoder *enc = drmModeGetEncoder(d->fd, conn->encoders[e]);
		if (!enc)
			continue;
		for (int i = 0; i < res->count_crtcs; i++) {
			if (enc->possible_crtcs & (1u << i)) {
				d->crtc_id = res->crtcs[i];
				break;
			}
		}
		drmModeFreeEncoder(enc);
	}
	if (!d->crtc_id) {
		fprintf(stderr, "No usable CRTC for connector %u\n", d->conn_id);
		drmModeFreeConnector(conn);
		return -1;
	}
	for (int i = 0; i < res->count_crtcs; i++)
		if (res->crtcs[i] == d->crtc_id)
			d->crtc_idx = (uint32_t)i;

	drmModeCrtc *crtc = drmModeGetCrtc(d->fd, d->crtc_id);
	if (crtc && crtc->mode_valid) {
		d->mode = crtc->mode;
		d->need_modeset = false;
	} else {
		/* CRTC is off: use the preferred mode (or the first one) */
		d->mode = conn->modes[0];
		for (int i = 0; i < conn->count_modes; i++)
			if (conn->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
				d->mode = conn->modes[i];
				break;
			}
		d->need_modeset = true;
	}
	if (crtc)
		drmModeFreeCrtc(crtc);
	drmModeFreeConnector(conn);
	return 0;
}

/* ============================================================
 * pick_nv12_plane - Choose a plane that can scan out LINEAR NV12.
 *
 * Preference order:
 *   1. the PRIMARY plane of our CRTC, if it lists NV12
 *   2. otherwise the first idle (CRTC_ID == 0) plane usable on this
 *      CRTC that lists NV12
 *
 * "Lists NV12" means: NV12 is in drmModePlane->formats AND, when the
 * plane has IN_FORMATS, the blob pairs NV12 with DRM_FORMAT_MOD_LINEAR
 * (a dumb buffer is always linear).
 * ============================================================ */
static bool plane_takes_linear_nv12(int fd, drmModePlane *pl,
				    drmModeObjectProperties *props)
{
	bool in_list = false;
	struct in_formats inf;

	for (uint32_t f = 0; f < pl->count_formats; f++)
		if (pl->formats[f] == DRM_FORMAT_NV12)
			in_list = true;
	if (!in_list)
		return false;

	int r = load_in_formats(fd, props, "IN_FORMATS", &inf, NULL);
	if (r == 1)
		return true;	/* no IN_FORMATS: implicit (linear) only */
	if (r < 0)
		return false;
	bool ok = in_formats_supports(&inf, DRM_FORMAT_NV12, DRM_FORMAT_MOD_LINEAR);
	in_formats_free(&inf);
	return ok;
}

static int pick_nv12_plane(struct nv12_demo *d)
{
	drmModePlaneRes *pres = drmModeGetPlaneResources(d->fd);
	uint32_t primary = 0, idle = 0;
	uint64_t primary_type = 0, idle_type = 0;

	if (!pres) {
		fprintf(stderr, "drmModeGetPlaneResources: %s\n", strerror(errno));
		return -1;
	}

	for (uint32_t i = 0; i < pres->count_planes; i++) {
		drmModePlane *pl = drmModeGetPlane(d->fd, pres->planes[i]);
		if (!pl)
			continue;
		if (!(pl->possible_crtcs & (1u << d->crtc_idx))) {
			drmModeFreePlane(pl);
			continue;
		}
		drmModeObjectProperties *props =
			drmModeObjectGetProperties(d->fd, pl->plane_id,
						   DRM_MODE_OBJECT_PLANE);
		if (!props) {
			drmModeFreePlane(pl);
			continue;
		}
		uint64_t type = 0, cur_crtc = 0;
		find_prop(d->fd, props, "type", NULL, &type);
		find_prop(d->fd, props, "CRTC_ID", NULL, &cur_crtc);

		bool ok = plane_takes_linear_nv12(d->fd, pl, props);
		printf("  plane %u (%s, CRTC_ID=%" PRIu64 "): linear NV12 %s\n",
		       pl->plane_id, plane_type_str(type), cur_crtc,
		       ok ? "YES" : "no");

		if (ok && type == DRM_PLANE_TYPE_PRIMARY &&
		    (cur_crtc == d->crtc_id || cur_crtc == 0) && !primary) {
			primary = pl->plane_id;
			primary_type = type;
		} else if (ok && cur_crtc == 0 && !idle &&
			   type != DRM_PLANE_TYPE_CURSOR) {
			idle = pl->plane_id;
			idle_type = type;
		}
		drmModeFreeObjectProperties(props);
		drmModeFreePlane(pl);
	}
	drmModeFreePlaneResources(pres);

	if (primary) {
		d->plane_id = primary;
		d->plane_type = primary_type;
	} else if (idle) {
		d->plane_id = idle;
		d->plane_type = idle_type;
	} else {
		return -1;
	}
	return 0;
}

/* ============================================================
 * cache_and_save - Cache every property ID we need and snapshot the
 * current values so that restore_state() can put them back.
 * ============================================================ */
static int cache_and_save(struct nv12_demo *d)
{
	drmModeObjectProperties *pp =
		drmModeObjectGetProperties(d->fd, d->plane_id, DRM_MODE_OBJECT_PLANE);
	drmModeObjectProperties *cp =
		drmModeObjectGetProperties(d->fd, d->crtc_id, DRM_MODE_OBJECT_CRTC);
	drmModeObjectProperties *kp =
		drmModeObjectGetProperties(d->fd, d->conn_id, DRM_MODE_OBJECT_CONNECTOR);
	int ret = -1;

	if (!pp || !cp || !kp) {
		fprintf(stderr, "drmModeObjectGetProperties failed: %s\n",
			strerror(errno));
		goto out;
	}

	save_plane_prop(d, pp, "FB_ID",   &d->p_fb);
	save_plane_prop(d, pp, "CRTC_ID", &d->p_crtc);
	save_plane_prop(d, pp, "SRC_X",   &d->p_src_x);
	save_plane_prop(d, pp, "SRC_Y",   &d->p_src_y);
	save_plane_prop(d, pp, "SRC_W",   &d->p_src_w);
	save_plane_prop(d, pp, "SRC_H",   &d->p_src_h);
	save_plane_prop(d, pp, "CRTC_X",  &d->p_crtc_x);
	save_plane_prop(d, pp, "CRTC_Y",  &d->p_crtc_y);
	save_plane_prop(d, pp, "CRTC_W",  &d->p_crtc_w);
	save_plane_prop(d, pp, "CRTC_H",  &d->p_crtc_h);
	/* Optional properties: skipped gracefully if absent */
	save_plane_prop(d, pp, "zpos",           &d->p_zpos);
	save_plane_prop(d, pp, "COLOR_ENCODING", &d->p_enc);
	save_plane_prop(d, pp, "COLOR_RANGE",    &d->p_range);

	if (!d->p_fb || !d->p_crtc || !d->p_src_x || !d->p_src_y ||
	    !d->p_src_w || !d->p_src_h || !d->p_crtc_x || !d->p_crtc_y ||
	    !d->p_crtc_w || !d->p_crtc_h) {
		fprintf(stderr, "Plane %u lacks mandatory atomic properties\n",
			d->plane_id);
		goto out;
	}

	if (d->p_zpos) {
		drmModePropertyRes *z = drmModeGetProperty(d->fd, d->p_zpos);
		if (z) {
			d->zpos_mutable = !(z->flags & DRM_MODE_PROP_IMMUTABLE) &&
					  (z->flags & DRM_MODE_PROP_RANGE) &&
					  z->count_values >= 2;
			if (d->zpos_mutable)
				d->zpos_max = z->values[1];
			drmModeFreeProperty(z);
		}
	}

	if (!find_prop(d->fd, cp, "ACTIVE", &d->c_active, &d->saved_active) ||
	    !find_prop(d->fd, cp, "MODE_ID", &d->c_mode_id, &d->saved_mode_id) ||
	    !find_prop(d->fd, kp, "CRTC_ID", &d->k_crtc_id, &d->saved_conn_crtc)) {
		fprintf(stderr, "CRTC/connector lacks ACTIVE/MODE_ID/CRTC_ID\n");
		goto out;
	}
	ret = 0;
out:
	if (pp) drmModeFreeObjectProperties(pp);
	if (cp) drmModeFreeObjectProperties(cp);
	if (kp) drmModeFreeObjectProperties(kp);
	return ret;
}

/* ============================================================
 * create_nv12_buffer - The "one dumb buffer, two planes" trick.
 *
 * DRM_IOCTL_MODE_CREATE_DUMB only knows width/height/bpp, i.e. a
 * single-plane packed buffer.  NV12 needs w*h bytes of Y followed by
 * w*h/2 bytes of interleaved CbCr.  So we ask for an 8-bpp buffer that
 * is h*3/2 rows tall:
 *
 *     rows [0, h)          -> Y plane       (offset 0)
 *     rows [h, h*3/2)      -> CbCr plane    (offset pitch*h)
 *
 * The CbCr plane has w/2 pairs of 2 bytes = w bytes per row, so the
 * same pitch works for both planes.  The kernel's pitch (which may be
 * padded, e.g. to 64 bytes by rockchip_gem_dumb_create()) is used for
 * both, never "w".
 *
 * The FB is then registered with drmModeAddFB2() passing the SAME GEM
 * handle twice with different offsets.  drmModeAddFB2() does not set
 * DRM_MODE_FB_MODIFIERS, so the modifier fields stay 0.  Strictly that
 * means "implicit, driver-defined layout" (see the DRM_FORMAT_MOD_LINEAR
 * comment in drm_fourcc.h); the kernel stores 0 in fb->modifier, which
 * is numerically DRM_FORMAT_MOD_LINEAR, and a dumb buffer is a plain
 * pitch x height allocation, so the two agree here.  A modifier-aware
 * client would call drmModeAddFB2WithModifiers() with
 * DRM_FORMAT_MOD_LINEAR and DRM_MODE_FB_MODIFIERS instead.
 * ============================================================ */
static int create_nv12_buffer(struct nv12_demo *d, enum matrix m)
{
	struct drm_mode_create_dumb create = {0};
	struct drm_mode_map_dumb map = {0};
	uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
	int ret;

	/* 4:2:0 subsampling: keep both dimensions even */
	d->w = d->mode.hdisplay & ~1u;
	d->h = d->mode.vdisplay & ~1u;
	if (d->w < 4 || d->h < 4) {
		fprintf(stderr, "Mode %ux%u too small\n", d->w, d->h);
		return -1;
	}

	create.width  = d->w;
	create.height = d->h * 3 / 2;
	create.bpp    = 8;
	if (drmIoctl(d->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
		fprintf(stderr, "CREATE_DUMB %ux%u@8bpp: %s\n",
			create.width, create.height, strerror(errno));
		return -1;
	}
	d->handle = create.handle;
	d->pitch  = create.pitch;
	d->size   = create.size;
	printf("Dumb buffer: %ux%u @ 8bpp -> pitch=%u size=%" PRIu64 "\n",
	       create.width, create.height, d->pitch, d->size);

	map.handle = d->handle;
	if (drmIoctl(d->fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
		fprintf(stderr, "MAP_DUMB: %s\n", strerror(errno));
		return -1;
	}
	d->map = mmap(NULL, d->size, PROT_READ | PROT_WRITE, MAP_SHARED,
		      d->fd, map.offset);
	if (d->map == MAP_FAILED) {
		d->map = NULL;
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		return -1;
	}

	fill_nv12(d->map, d->w, d->h, d->pitch, m);

	handles[0] = d->handle;  pitches[0] = d->pitch;  offsets[0] = 0;
	handles[1] = d->handle;  pitches[1] = d->pitch;  offsets[1] = d->pitch * d->h;

	ret = drmModeAddFB2(d->fd, d->w, d->h, DRM_FORMAT_NV12,
			    handles, pitches, offsets, &d->fb_id, 0);
	if (ret) {
		fprintf(stderr, "drmModeAddFB2(NV12): %s\n", err_str(ret));
		d->fb_id = 0;
		return -1;
	}
	printf("NV12 FB id=%u: plane0 (Y) offset=0 pitch=%u, "
	       "plane1 (CbCr) offset=%u pitch=%u\n",
	       d->fb_id, pitches[0], offsets[1], pitches[1]);
	return 0;
}

static void destroy_nv12_buffer(struct nv12_demo *d)
{
	if (d->fb_id)
		drmModeRmFB(d->fd, d->fb_id);
	if (d->map)
		munmap(d->map, d->size);
	if (d->handle) {
		struct drm_mode_destroy_dumb destroy = { .handle = d->handle };
		drmIoctl(d->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
	}
	d->fb_id = 0;
	d->map = NULL;
	d->handle = 0;
}

/* ============================================================
 * build_show_request - Plane (+ optional modeset) atomic request.
 * ============================================================ */
static drmModeAtomicReq *build_show_request(struct nv12_demo *d, enum matrix m)
{
	drmModeAtomicReq *req = drmModeAtomicAlloc();
	if (!req)
		return NULL;

	if (d->need_modeset) {
		drmModeAtomicAddProperty(req, d->conn_id, d->k_crtc_id, d->crtc_id);
		drmModeAtomicAddProperty(req, d->crtc_id, d->c_mode_id, d->mode_blob);
		drmModeAtomicAddProperty(req, d->crtc_id, d->c_active, 1);
	}

	drmModeAtomicAddProperty(req, d->plane_id, d->p_fb,     d->fb_id);
	drmModeAtomicAddProperty(req, d->plane_id, d->p_crtc,   d->crtc_id);
	drmModeAtomicAddProperty(req, d->plane_id, d->p_src_x,  0);
	drmModeAtomicAddProperty(req, d->plane_id, d->p_src_y,  0);
	drmModeAtomicAddProperty(req, d->plane_id, d->p_src_w,  (uint64_t)d->w << 16);
	drmModeAtomicAddProperty(req, d->plane_id, d->p_src_h,  (uint64_t)d->h << 16);
	drmModeAtomicAddProperty(req, d->plane_id, d->p_crtc_x, 0);
	drmModeAtomicAddProperty(req, d->plane_id, d->p_crtc_y, 0);
	drmModeAtomicAddProperty(req, d->plane_id, d->p_crtc_w, d->w);
	drmModeAtomicAddProperty(req, d->plane_id, d->p_crtc_h, d->h);

	/*
	 * An overlay only shows if it is stacked above whatever the primary
	 * plane is scanning out (fbcon, usually).  If zpos is writable, put
	 * our plane on top; the saved value is restored on exit.
	 */
	if (d->plane_type != DRM_PLANE_TYPE_PRIMARY && d->zpos_mutable) {
		drmModeAtomicAddProperty(req, d->plane_id, d->p_zpos, d->zpos_max);
		printf("Setting zpos=%" PRIu64 " so the overlay is on top\n",
		       d->zpos_max);
	}

	/*
	 * Tell the driver how to interpret our YCbCr, if it lets us.  The
	 * enum entry names are fixed by drm_color_mgmt.c.
	 */
	if (d->p_enc) {
		const char *want = (m == MATRIX_BT709) ? "ITU-R BT.709 YCbCr"
						       : "ITU-R BT.601 YCbCr";
		uint64_t v;
		if (find_enum_value(d->fd, d->p_enc, want, &v)) {
			drmModeAtomicAddProperty(req, d->plane_id, d->p_enc, v);
			printf("COLOR_ENCODING = %s\n", want);
		} else {
			printf("COLOR_ENCODING has no \"%s\" entry\n", want);
		}
	} else {
		printf("No COLOR_ENCODING property: the driver picks the "
		       "YCbCr->RGB matrix (see the doc for mainline VOP2).\n");
	}
	if (d->p_range) {
		uint64_t v;
		if (find_enum_value(d->fd, d->p_range, "YCbCr limited range", &v)) {
			drmModeAtomicAddProperty(req, d->plane_id, d->p_range, v);
			printf("COLOR_RANGE = YCbCr limited range\n");
		}
	}
	return req;
}

/* ============================================================
 * restore_state - Put back every plane property we snapshotted and,
 * if we had to switch the CRTC on, switch it off again.
 * ============================================================ */
static void restore_state(struct nv12_demo *d)
{
	drmModeAtomicReq *req = drmModeAtomicAlloc();
	uint32_t flags = 0;
	int ret;

	if (!req)
		return;

	for (int i = 0; i < d->n_saved; i++)
		drmModeAtomicAddProperty(req, d->plane_id, d->saved[i].id,
					 d->saved[i].value);

	if (d->need_modeset) {
		drmModeAtomicAddProperty(req, d->conn_id, d->k_crtc_id,
					 d->saved_conn_crtc);
		drmModeAtomicAddProperty(req, d->crtc_id, d->c_mode_id,
					 d->saved_mode_id);
		drmModeAtomicAddProperty(req, d->crtc_id, d->c_active,
					 d->saved_active);
		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
	}

	ret = drmModeAtomicCommit(d->fd, req, flags, NULL);
	if (ret)
		fprintf(stderr, "Restore commit failed: %s "
			"(fbdev emulation, if enabled, restores the console "
			"when the last DRM client closes)\n", err_str(ret));
	else
		printf("Previous plane%s state restored\n",
		       d->need_modeset ? "/CRTC" : "");
	drmModeAtomicFree(req);
}

static void wait_for_seconds(int seconds)
{
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };

	for (int i = 0; i < seconds * 10 && !g_stop; i++)
		nanosleep(&ts, NULL);
}

/* ============================================================
 * run_nv12 - --nv12 mode.
 * ============================================================ */
static int run_nv12(int fd, enum matrix m, int hold_seconds)
{
	struct nv12_demo d = { .fd = fd };
	drmModeAtomicReq *req = NULL;
	drmModeRes *res = NULL;
	bool shown = false;
	uint32_t flags;
	int ret = -1, r;

	if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) ||
	    drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
		fprintf(stderr, "Atomic/universal planes not supported: %s\n",
			strerror(errno));
		return -1;
	}

	res = drmModeGetResources(fd);
	if (!res) {
		fprintf(stderr, "drmModeGetResources: %s\n", strerror(errno));
		return -1;
	}
	if (pick_pipeline(&d, res))
		goto out;

	printf("Connector %u -> CRTC %u (index %u), mode %ux%u@%u%s\n",
	       d.conn_id, d.crtc_id, d.crtc_idx, d.mode.hdisplay,
	       d.mode.vdisplay, d.mode.vrefresh,
	       d.need_modeset ? " [CRTC is off: will modeset]" : " [already active]");

	printf("Looking for a plane that accepts LINEAR NV12:\n");
	if (pick_nv12_plane(&d)) {
		printf("No plane usable on CRTC %u advertises LINEAR NV12. "
		       "Nothing to do.\n", d.crtc_id);
		ret = 0;
		goto out;
	}
	printf("Using plane %u (%s)\n", d.plane_id, plane_type_str(d.plane_type));

	if (cache_and_save(&d))
		goto out;

	if (d.need_modeset &&
	    drmModeCreatePropertyBlob(fd, &d.mode, sizeof(d.mode), &d.mode_blob)) {
		fprintf(stderr, "drmModeCreatePropertyBlob: %s\n", strerror(errno));
		goto out;
	}

	printf("Filling %s limited-range colour bars\n",
	       m == MATRIX_BT709 ? "BT.709" : "BT.601");
	if (create_nv12_buffer(&d, m))
		goto out;

	req = build_show_request(&d, m);
	if (!req)
		goto out;

	/* Dry run first: the driver's atomic_check decides. */
	flags = d.need_modeset ? DRM_MODE_ATOMIC_ALLOW_MODESET : 0;
	r = drmModeAtomicCommit(fd, req, flags | DRM_MODE_ATOMIC_TEST_ONLY, NULL);
	if (r) {
		fprintf(stderr,
			"TEST_ONLY rejected NV12 on plane %u: %s\n"
			"  The driver's atomic_check refused this configuration.\n"
			"  For the exact reason enable KMS debug output, e.g.\n"
			"    echo 0x16 | sudo tee /sys/module/drm/parameters/debug\n"
			"  re-run, then check dmesg (set it back to 0 afterwards).\n",
			d.plane_id, err_str(r));
		if (r == -EACCES || r == -EPERM)
			fprintf(stderr, "  (EACCES/EPERM: another process is DRM master?)\n");
		ret = 1;
		goto out;
	}
	printf("TEST_ONLY passed -- committing\n");

	r = drmModeAtomicCommit(fd, req, flags, NULL);
	if (r) {
		fprintf(stderr, "Atomic commit failed: %s\n", err_str(r));
		goto out;
	}
	shown = true;
	printf("Showing NV12 colour bars for %d s (Ctrl+C to stop)...\n",
	       hold_seconds);
	wait_for_seconds(hold_seconds);
	ret = 0;

out:
	if (shown)
		restore_state(&d);
	if (req)
		drmModeAtomicFree(req);
	destroy_nv12_buffer(&d);
	if (d.mode_blob)
		drmModeDestroyPropertyBlob(fd, d.mode_blob);
	drmModeFreeResources(res);
	return ret;
}

/* ============================================================
 * run_selftest - Host-side checks that need no DRM device.
 *
 * Builds an IN_FORMATS blob byte-for-byte the way
 * create_in_format_blob() does (24-byte header, formats padded to
 * 8 bytes, then the modifier array), parses it back, and checks the
 * modifier decoder and the YCbCr maths against hand-computed values.
 * ============================================================ */
static int run_selftest(void)
{
	static const uint32_t fmts[3] = {
		DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12, DRM_FORMAT_RGB565,
	};
	const uint64_t afbc = DRM_FORMAT_MOD_ARM_AFBC(
		AFBC_FORMAT_MOD_BLOCK_SIZE_16x16 | AFBC_FORMAT_MOD_YTR |
		AFBC_FORMAT_MOD_SPARSE | AFBC_FORMAT_MOD_SPLIT);
	struct drm_format_modifier_blob hdr = {0};
	struct drm_format_modifier mods[2];
	uint8_t blob[256] = {0};
	struct in_formats inf;
	char err[160], desc[160];
	int fails = 0;

	memset(mods, 0, sizeof(mods));
	hdr.version = FORMAT_BLOB_CURRENT;
	hdr.count_formats = 3;
	hdr.formats_offset = sizeof(hdr);
	hdr.count_modifiers = 2;
	hdr.modifiers_offset = (uint32_t)((sizeof(hdr) + sizeof(fmts) + 7) & ~7u);
	mods[0].modifier = DRM_FORMAT_MOD_LINEAR;
	mods[0].formats = 0x7;		/* all three */
	mods[1].modifier = afbc;
	mods[1].formats = 0x5;		/* XRGB8888 and RGB565, not NV12 */

	memcpy(blob, &hdr, sizeof(hdr));
	memcpy(blob + hdr.formats_offset, fmts, sizeof(fmts));
	memcpy(blob + hdr.modifiers_offset, mods, sizeof(mods));
	size_t len = hdr.modifiers_offset + sizeof(mods);

#define CHECK(cond) do { if (!(cond)) { printf("FAIL: %s\n", #cond); fails++; } \
			 else printf("ok:   %s\n", #cond); } while (0)

	CHECK(sizeof(struct drm_format_modifier_blob) == 24);
	CHECK(sizeof(struct drm_format_modifier) == 24);
	CHECK(hdr.modifiers_offset == 40);
	CHECK(parse_in_formats_blob(blob, len, &inf, err, sizeof(err)) == 0);
	CHECK(inf.count_formats == 3 && inf.count_modifiers == 2);
	CHECK(in_formats_supports(&inf, DRM_FORMAT_NV12, DRM_FORMAT_MOD_LINEAR));
	CHECK(!in_formats_supports(&inf, DRM_FORMAT_NV12, afbc));
	CHECK(in_formats_supports(&inf, DRM_FORMAT_RGB565, afbc));
	print_in_formats_matrix(&inf, "      ");
	in_formats_free(&inf);

	/* Truncated blob must be rejected, not over-read */
	CHECK(parse_in_formats_blob(blob, len - 1, &inf, err, sizeof(err)) != 0);
	printf("      (rejected with: %s)\n", err);

	describe_modifier(afbc, desc, sizeof(desc));
	printf("      0x%016" PRIx64 " -> %s\n", afbc, desc);
	CHECK(strcmp(desc, "ARM AFBC(16x16|YTR|SPLIT|SPARSE)") == 0);
	CHECK(afbc == 0x0800000000000071ULL);
	describe_modifier(DRM_FORMAT_MOD_LINEAR, desc, sizeof(desc));
	CHECK(strcmp(desc, "LINEAR") == 0);

	char fcc[16];
	fourcc_str(DRM_FORMAT_NV12, fcc);
	CHECK(strcmp(fcc, "NV12") == 0);
	fourcc_str(DRM_FORMAT_XRGB8888, fcc);
	CHECK(strcmp(fcc, "XR24") == 0);

	/* 100% colour bars, BT.601 limited range (values derived in the doc) */
	static const uint8_t ref601[8][3] = {
		{235, 128, 128}, {210,  16, 146}, {170, 166,  16}, {145,  54,  34},
		{106, 202, 222}, { 81,  90, 240}, { 41, 240, 110}, { 16, 128, 128},
	};
	bool bars_ok = true;
	for (int i = 0; i < 8; i++) {
		struct ycbcr c = rgb_to_ycbcr_limited(bars[i].r, bars[i].g,
						      bars[i].b, MATRIX_BT601);
		printf("      %-8s Y=%3u Cb=%3u Cr=%3u\n", bars[i].name,
		       c.y, c.cb, c.cr);
		if (c.y != ref601[i][0] || c.cb != ref601[i][1] ||
		    c.cr != ref601[i][2])
			bars_ok = false;
	}
	CHECK(bars_ok);
#undef CHECK

	printf("%s (%d failure%s)\n", fails ? "SELFTEST FAILED" : "SELFTEST PASSED",
	       fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static void usage(const char *prog)
{
	printf("Usage: %s [options]\n"
	       "\n"
	       "Modes:\n"
	       "  -l, --list       (default) per plane: fourcc list and the\n"
	       "                   format x modifier matrix from IN_FORMATS\n"
	       "  -n, --nv12       show NV12 colour bars from a dumb buffer on the\n"
	       "                   primary plane (or the first idle plane taking\n"
	       "                   LINEAR NV12), then restore the previous state\n"
	       "      --selftest   run host-only parser/decoder/colour checks\n"
	       "\n"
	       "Options:\n"
	       "  -d, --device <path>  DRM device (default %s)\n"
	       "      --bt709          encode the bars with BT.709 instead of BT.601\n"
	       "  -t, --time <sec>     how long --nv12 keeps the pattern (default %d)\n"
	       "  -h, --help           this text\n",
	       prog, DEFAULT_DEVICE, DEFAULT_HOLD_SECONDS);
}

int main(int argc, char **argv)
{
	enum { MODE_LIST, MODE_NV12, MODE_SELFTEST } mode = MODE_LIST;
	const char *dev = DEFAULT_DEVICE;
	enum matrix m = MATRIX_BT601;
	int hold = DEFAULT_HOLD_SECONDS;
	int opt, ret;

	static const struct option longopts[] = {
		{ "device",   required_argument, NULL, 'd' },
		{ "list",     no_argument,       NULL, 'l' },
		{ "nv12",     no_argument,       NULL, 'n' },
		{ "bt709",    no_argument,       NULL, '7' },
		{ "time",     required_argument, NULL, 't' },
		{ "selftest", no_argument,       NULL, 'S' },
		{ "help",     no_argument,       NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};

	while ((opt = getopt_long(argc, argv, "d:lnt:h", longopts, NULL)) != -1) {
		switch (opt) {
		case 'd': dev = optarg; break;
		case 'l': mode = MODE_LIST; break;
		case 'n': mode = MODE_NV12; break;
		case '7': m = MATRIX_BT709; break;
		case 'S': mode = MODE_SELFTEST; break;
		case 't': {
			char *end = NULL;
			long v = strtol(optarg, &end, 10);
			if (!end || *end || v < 1 || v > 3600) {
				fprintf(stderr, "invalid --time value '%s'\n", optarg);
				return 2;
			}
			hold = (int)v;
			break;
		}
		case 'h': usage(argv[0]); return 0;
		default:  usage(argv[0]); return 2;
		}
	}
	if (optind < argc) {
		fprintf(stderr, "unexpected argument '%s'\n", argv[optind]);
		usage(argv[0]);
		return 2;
	}

	if (mode == MODE_SELFTEST)
		return run_selftest();

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	int fd = open(dev, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
		return 1;
	}

	if (mode == MODE_NV12)
		ret = run_nv12(fd, m, hold);
	else
		ret = run_list(fd);

	close(fd);
	return ret ? 1 : 0;
}
