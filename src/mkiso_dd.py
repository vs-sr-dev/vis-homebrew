#!/usr/bin/env python3
"""Build VIS-compatible ISO for DISPDIB experiment (WOLFVDD.EXE)."""
import pathlib
import pycdlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "cd_root_dd"
OUT = ROOT / "build" / "wolfvis_dd.iso"

iso = pycdlib.PyCdlib()
iso.new(interchange_level=1, vol_ident="WOLFVDD", sys_ident="MODWIN")

for item in sorted(SRC.iterdir()):
    if not item.is_file():
        continue
    name = item.name.upper()
    iso_name = f"/{name};1" if "." in name else f"/{name}.;1"
    print(f"add: {item} -> {iso_name}")
    iso.add_file(str(item), iso_path=iso_name)

iso.write(str(OUT))
iso.close()
size = OUT.stat().st_size
print(f"\nWROTE {OUT} ({size} bytes, {size/1024:.1f} KB)")
