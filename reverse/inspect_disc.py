#!/usr/bin/env python3
"""Dump AUTOEXEC, SYSTEM.INI, WIN.INI from all 3 real VIS discs for comparison."""
import io
import pathlib
import pycdlib

SRC = pathlib.Path(__file__).resolve().parent.parent / "isos"

def bin2iso(bin_path):
    raw = bin_path.read_bytes()
    n = len(raw) // 2352
    buf = bytearray(n * 2048)
    for i in range(n):
        buf[i*2048:(i+1)*2048] = raw[i*2352+16:i*2352+16+2048]
    return bytes(buf)

for bin_path in sorted(SRC.glob("*.bin")):
    print(f"\n{'='*78}\n{bin_path.name}\n{'='*78}")
    iso = pycdlib.PyCdlib()
    iso.open_fp(io.BytesIO(bin2iso(bin_path)))
    for fname in ("AUTOEXEC.;1", "AUTOEXEC.BAT;1", "SYSTEM.INI;1", "WIN.INI;1"):
        try:
            out = io.BytesIO()
            iso.get_file_from_iso_fp(out, iso_path=f"/{fname}")
            data = out.getvalue()
            print(f"\n--- /{fname} ({len(data)} bytes) ---")
            try:
                print(data.decode("ascii"))
            except UnicodeDecodeError:
                print(repr(data))
        except Exception as e:
            pass
    iso.close()
