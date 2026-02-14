# Compiler and Linker configurations
CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags libdrm)
LDFLAGS = $(shell pkg-config --libs libdrm)

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
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Clean up all built binaries
clean:
	rm -f $(BINS)

.PHONY: all clean
