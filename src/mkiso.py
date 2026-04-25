#!/usr/bin/env python3
"""Build a VIS-compatible ISO 9660 from ../cd_root -> ../build/hello.iso."""
import sys
import pathlib
import pycdlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "cd_root"
OUT = ROOT / "build" / "hello.iso"

iso = pycdlib.PyCdlib()
iso.new(interchange_level=1, vol_ident="HELLO_VIS", sys_ident="MODWIN")

for item in sorted(SRC.iterdir()):
    if not item.is_file():
        continue
    name = item.name.upper()
    # ISO 9660 Level 1 requires "NAME.EXT;VER" — AUTOEXEC has no ext, add "."
    if "." in name:
        iso_name = f"/{name};1"
    else:
        iso_name = f"/{name}.;1"
    print(f"add: {item} -> {iso_name}")
    iso.add_file(str(item), iso_path=iso_name)

iso.write(str(OUT))
iso.close()

size = OUT.stat().st_size
print(f"\nWROTE {OUT} ({size} bytes, {size/1024:.1f} KB)")
