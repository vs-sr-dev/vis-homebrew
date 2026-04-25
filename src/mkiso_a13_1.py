#!/usr/bin/env python3
"""Build VIS-compatible ISO for A.13.1 raycaster polish (grid-line DDA + light/dark walls)."""
import pathlib
import pycdlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "cd_root_a13_1"
OUT = ROOT / "build" / "wolfvis_a13_1.iso"

iso = pycdlib.PyCdlib()
iso.new(interchange_level=1, vol_ident="WOLFA131", sys_ident="MODWIN")

for item in sorted(SRC.iterdir()):
    if not item.is_file():
        continue
    name = item.name.upper()
    iso_name = f"/{name};1" if "." in name else f"/{name}.;1"
    print(f"add: {item} -> {iso_name}")
    iso.add_file(str(item), iso_path=iso_name)

iso.write(str(OUT))
iso.close()
print(f"\nWROTE {OUT} ({OUT.stat().st_size} bytes)")
