#!/usr/bin/env python3
"""
Generate DDS script for frequency ramp
Ramps from 100 MHz to 150 MHz in 50 steps over 2 seconds
"""

start_freq = 100.0  # MHz
end_freq = 150.0    # MHz
num_steps = 50
total_time_ms = 2000  # 2 seconds

# Calculate step parameters
freq_step = (end_freq - start_freq) / (num_steps - 1)
time_per_step = total_time_ms / num_steps

# Generate script
channel = 0
amplitude = 0x3FFF  # Max amplitude (14-bit)
phase = 0

print(f"# Frequency ramp: {start_freq} MHz -> {end_freq} MHz in {num_steps} steps over {total_time_ms} ms")
print(f"# Step size: {freq_step:.6f} MHz, delay per step: {time_per_step:.2f} ms\n")

for step in range(num_steps):
    freq = start_freq + step * freq_step
    delay = int(time_per_step)
    print(f"tone {channel} {freq:.6f} {amplitude} {phase} {delay}")

print(f"\n# Final frequency: {start_freq + (num_steps - 1) * freq_step:.6f} MHz")
