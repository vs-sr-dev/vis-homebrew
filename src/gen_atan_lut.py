#!/usr/bin/env python3
"""Generate Q10 atan LUT for wolfvis A.16a iter 2 (8-direction enemy
rotation). Output: wolfvis_a16a_atantab.h.

Input domain: t = i/256, i in 0..256 (so 257 entries; t in [0, 1]).
Output: atan_q10_lut[i] = round(atan(t) * 1024 / (2*pi)), in [0, 128].

The full atan2(dy, dx) is reconstructed at runtime via:
  - magnitude swap so |slope| <= 1 (lookup with t = abs(min)/abs(max))
  - quadrant fixup using signs of dx, dy

128 entries per quarter-sector keep rotation transitions visually
smooth at our 8-direction sprite resolution (45 deg per sector).
"""
import math
import pathlib

N = 256
vals = []
for i in range(N + 1):
    t = i / N
    rad = math.atan(t)         # 0 .. pi/4
    q10 = int(round(rad * 1024 / (2 * math.pi)))
    vals.append(q10)

assert vals[0] == 0
assert vals[N] == 128, f"atan(1) should map to 128 in Q10, got {vals[N]}"

OUT = pathlib.Path(__file__).parent / "wolfvis_a16a_atantab.h"
with OUT.open("w", encoding="ascii") as f:
    f.write("/* Auto-generated Q10 atan table for ANGLES=1024. */\n")
    f.write(f"/* atan_q10_lut[i] = round(atan(i/{N}) * 1024 / (2*pi)), i in 0..{N}. */\n")
    f.write("/* Range [0..128] = [0..pi/4] in our Q10 angle space. */\n")
    f.write(f"#define ATAN_LUT_N {N}\n")
    f.write(f"static const int __far atan_q10_lut[ATAN_LUT_N + 1] = {{\n")
    for i in range(0, len(vals), 8):
        line = ", ".join(f"{v:5d}" for v in vals[i:i + 8])
        f.write(f"    {line},\n")
    f.write("};\n")

print(f"WROTE {OUT} ({len(vals)} entries, max {max(vals)})")
