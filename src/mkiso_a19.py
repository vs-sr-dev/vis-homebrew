#!/usr/bin/env python3
"""Build VIS-compatible ISO for A.19 centered viewport + minimap toggle."""
import pathlib
import pycdlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "cd_root_a19"
OUT = ROOT / "build" / "wolfvis_a19.iso"

iso = pycdlib.PyCdlib()
iso.new(interchange_level=1, vol_ident="WOLFA19", sys_ident="MODWIN")

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
