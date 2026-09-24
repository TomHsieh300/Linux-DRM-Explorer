# Compiler and Linker configurations
#
# Native build on the board:
#   make
#
# Cross build from an x86_64 Debian/Ubuntu host using multiarch
# (apt install gcc-aarch64-linux-gnu libdrm-dev:arm64), see README "Building":
#   make CROSS_COMPILE=aarch64-linux-gnu- \
#        PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig
#
# With a board sysroot instead of multiarch (not tested in this repo), set
# PKG_CONFIG_SYSROOT_DIR, point PKG_CONFIG_LIBDIR into the sysroot and pass
# --sysroot=<dir> in both CFLAGS and LDFLAGS.
CROSS_COMPILE ?=

# Only replace make's built-in default "cc"; an explicit CC=... still wins
ifeq ($(origin CC),default)
CC = $(CROSS_COMPILE)gcc
endif

PKG_CONFIG ?= pkg-config

# $(shell) does not see variables given on the make command line (GNU make
# < 4.4), so hand the pkg-config search settings to it explicitly
PKG_ENV := $(foreach v,PKG_CONFIG_LIBDIR PKG_CONFIG_PATH PKG_CONFIG_SYSROOT_DIR,\
             $(if $(value $(v)),$(v)='$(value $(v))'))

DRM_CFLAGS := $(shell $(PKG_ENV) $(PKG_CONFIG) --cflags libdrm 2>/dev/null)
DRM_LIBS   := $(shell $(PKG_ENV) $(PKG_CONFIG) --libs libdrm 2>/dev/null)
ifneq ($(MAKECMDGOALS),clean)
ifeq ($(DRM_LIBS),)
$(error libdrm not found by '$(PKG_CONFIG)'. Install libdrm-dev (or set PKG_CONFIG_LIBDIR for cross builds))
endif
endif

# Warnings are always on; extra flags (e.g. EXTRA_CFLAGS=-Werror in CI) can be appended
CFLAGS ?= -O2
ALL_CFLAGS = -Wall -Wextra $(CFLAGS) $(EXTRA_CFLAGS) $(DRM_CFLAGS)

# Directories
SRC_DIR = src

# Find all .c files in src directory
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Generate executable names by removing the .c extension
BINS = $(SRCS:.c=)

# Default target: build all executables
all: $(BINS)

# Pattern rule: how to build each binary from its corresponding .c file
$(SRC_DIR)/%: $(SRC_DIR)/%.c
	$(CC) $(ALL_CFLAGS) $(LDFLAGS) -o $@ $< $(DRM_LIBS)

# Clean up all built binaries
clean:
	rm -f $(BINS)

.PHONY: all clean
