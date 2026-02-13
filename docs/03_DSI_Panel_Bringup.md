# Experiment 03: DSI Panel Bring-up & Video Timing Analysis

## 1. Video Timing Breakdown
The target DSI panel resolution is 1024x600. The observed timing parameters are:
* **H-Pixels**: 1024 (Active), 1354 (Total)
* **V-Lines**: 600 (Active), 636 (Total)
* **Refresh Rate**: 60Hz

## 2. Pixel Clock Calculation
The Pixel Clock (PCLK) can be verified using the following formula:
$$PCLK = H_{total} \times V_{total} \times Refresh\_Rate$$
$$1354 \times 636 \times 60 \approx 51.668 \text{ MHz}$$
This matches the `51668` value reported by `modetest`.

## 3. Register Level Verification
By reading `debugfs/dri/0/regs`, we located the hardware values in the VP3 section:
* **fdd90f40**: Found `054a` (1354 in Hex, H_total).
* **fdd90f50**: Found `027c` (636 in Hex, V_total).
This confirms that the software configuration has been correctly committed to the hardware IP registers.
