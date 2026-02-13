# Experiment 04: DRM Master Concepts & Race Conditions

## 1. Observation: DPMS Modification Failure
In a Ubuntu Lite environment with only a console running, attempting to set `DPMS:3` (Off) via `modetest` resulted in the screen turning off briefly and then immediately restoring.

## 2. Root Cause: fbcon Intervention
* **DRM Master**: Only one process can hold the "Master" status to perform modesetting at a time.
* **fbcon (Framebuffer Console)**: In the absence of a Window Manager, the kernel's `fbcon` acts as the Master.
* **Race Condition**: `modetest` modifies the property and exits. The kernel detects a change and `fbcon` re-triggers a commit to restore the console view.

## 3. Debugging Technique
Monitor active clients using `cat /sys/kernel/debug/dri/0/clients`. A process marked with `master: y` is the current owner of the display resources.
