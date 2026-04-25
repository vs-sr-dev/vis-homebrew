#!/usr/bin/env python3
"""Generate CONTROL.TAT starting from Atlas.bin template, swapping the title."""
import pathlib

HERE  = pathlib.Path(__file__).resolve().parent
ATLAS = HERE / "CONTROL.TAT.Atlas"
OUT   = HERE.parent / "cd_root" / "CONTROL.TAT"

tat = bytearray(ATLAS.read_bytes())

# Title field: 60 bytes at offset 0x54, space-padded. Must end with spaces
# (Atlas template uses spaces up to offset 0x90).
title = b"HELLO VIS - Homebrew Session 1 - 2026-04-24"
assert len(title) <= 60, "title too long"
new_title = title + b" " * (60 - len(title))
tat[0x54:0x54+60] = new_title

# "minwin A:\" at 0x1A3 — keep Atlas's exact variant (trailing backslash)
# Version tagline at 0x1B4 — keep Atlas's exact variant

OUT.write_bytes(bytes(tat))
print(f"Wrote {OUT} ({len(tat)} bytes)")
print("Title slice:", bytes(tat[0x54:0x54+60]))
