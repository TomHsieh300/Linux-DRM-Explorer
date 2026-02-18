# Experiment 08: Double Buffering and Performance Optimization

## 1. Objective
Advance the display pipeline by implementing **Double Buffering**. This experiment manages two independent framebuffers to prepare for flicker-free animations and introduces software-level optimizations for high-performance pixel rendering.

## 2. Key Implementation Details
* **Buffer Management (Array-based)**: Managed multiple `buffer_object` structures within an array. This provides a scalable architecture for Double or even Triple Buffering.
* **Branchless Pattern Generation**: Optimized the `draw_test_pattern` function by replacing nested `if/else` conditionals with a pre-defined color array and indexed lookup.
    * *Why?*: Modern CPU pipelines suffer from "branch misprediction" penalties. By removing branches from the inner loop (which runs once per pixel), we significantly improve rendering throughput for high-resolution displays.
* **Main Loop Refactoring**: Replaced redundant setup code with indexed loops, adhering to the DRY (Don't Repeat Yourself) principle and improving code maintainability.

## 3. Compilation
This project now utilizes a universal **Makefile** that automatically detects all source files in the `src/` directory.
```bash
# Build all experiments including double-buffer
make

# Execute the double buffering demo
sudo ./src/modeset-double-buffer
```

## 4. High-Level Logic Flow (C-Style Pseudocode)

The logic of `src/modeset-double-buffer.c` focuses on initializing multiple buffers and demonstrating the manual switching between them.

```c
// 1. Initialization & Pipeline Setup
// Identical to Experiment 07: Open device, find connected DSI and compatible CRTC.

// 2. Optimized Pixel Generation (Branchless Logic)
void draw_test_pattern(bo, pattern_type) {
    // Pre-defining colors in an array eliminates 'if/else' inside the loop
    uint32_t colors[2][3] = {{RED, GREEN, BLUE}, {GREEN, BLUE, RED}};
    
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int segment = x * 3 / width; // Map X to 0, 1, or 2
            // Direct array lookup - No branching!
            pixel[offset] = colors[pattern_type][segment];
        }
    }
}

// 3. Multi-Buffer Allocation Loop
struct buffer_object bufs[MAX_BUFFERS];
for (int i = 0; i < MAX_BUFFERS; i++) {
    // Allocate GEM dumb buffer, Add Framebuffer (FB), and mmap
    modeset_create_fb(fd, &bufs[i], i % 2); 
}

// 4. Manual Double Buffering Demo
for (int i = 0; i < MAX_BUFFERS; i++) {
    // Switch the CRTC scanout source to the next buffer
    // This is the 'Manual Flip' using the Legacy API
    drmModeSetCrtc(fd, crtc_id, bufs[i].fb_id, 0, 0, &conn_id, 1, &mode);
    
    printf("Buffer %d active. Press Enter to switch...", i);
    getchar();
}

// 5. Cleanup
// Destroy each FB and close file descriptor.
```
