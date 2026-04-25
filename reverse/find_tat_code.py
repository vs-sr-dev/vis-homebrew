#!/usr/bin/env python3
"""Locate code that references the 'a:control.tat' string in bank 1 of the VIS BIOS,
then disassemble surrounding 16-bit x86 code."""

import pathlib

import capstone

BIOS = pathlib.Path(__file__).resolve().parent / "p513bk1b.bin"
data = BIOS.read_bytes()

STR_POS = 0x6a5dd  # 'a:control.tat\0' starts here (from prior grep, +16 offset to end of 'modwin b: ')

# Find the exact offset of 'a:control.tat'
import re
m = re.search(rb"a:control\.tat", data)
assert m, "string not found"
str_off = m.start()
print(f"'a:control.tat' at 0x{str_off:x}\n")

# BIOS bank 1 is mapped... where? Per MAME VIS bank layout, ROM is at 0xF0000 or so,
# but for the 286 real-mode BIOS usually mapped at FE000:0 or F0000:0.
# The bank1 file is offset 0x80000 in the XML ROM (region bios). So file offset
# 0x6a5dd in bank1 = BIOS region offset 0x80000 + 0x6a5dd = 0xea5dd.
# On VIS, ROM is typically placed at top of real-mode address space (F0000-FFFFF
# for 64KB segment or higher for >64KB). A 1MB ROM can be paged. Without the
# driver's map we'll just search for pointers to str_off (as near-pointer word).

# Search for 16-bit little-endian words matching the lower 16 bits of str_off.
# The BIOS is 1 MB, so a near pointer would be 16-bit segment-relative.
# We'll scan for str_off & 0xFFFF in bank1.
target_lo = str_off & 0xFFFF
target_hi = (str_off >> 16) & 0xFFFF

print(f"Hunting near-pointers (word = 0x{target_lo:04x}) in bank 1...")
hits = []
for i in range(0, len(data) - 1):
    w = data[i] | (data[i+1] << 8)
    if w == target_lo:
        hits.append(i)
print(f"  {len(hits)} matches")
for h in hits[:30]:
    print(f"    0x{h:x}")

# Disassemble around the first few candidate pointer references
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
md.detail = False

print("\n=== Disasm of ~64 bytes immediately before the 'a:control.tat' string (raw data, may be garbage) ===")
for ins in md.disasm(data[str_off-64:str_off], str_off-64):
    print(f"  {ins.address:08x}  {ins.bytes.hex():<20}  {ins.mnemonic} {ins.op_str}")

print("\n=== Disasm ~128 bytes BEFORE each candidate ptr ref (first 5) ===")
for h in hits[:5]:
    print(f"\n--- candidate ref at 0x{h:x} ---")
    start = max(0, h - 128)
    for ins in md.disasm(data[start:h+8], start):
        print(f"  {ins.address:08x}  {ins.bytes.hex():<20}  {ins.mnemonic} {ins.op_str}")
