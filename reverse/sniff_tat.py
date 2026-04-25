#!/usr/bin/env python3
"""Extract CONTROL.TAT from a VIS disc image (ISO or BIN/CUE or CHD converted to ISO).
Usage:
    python sniff_tat.py <path_to_iso> [<path_to_iso2> ...]

For each disc: prints the file listing of the root + hex+ASCII dump of CONTROL.TAT,
and writes CONTROL.TAT.<n> next to this script for side-by-side compare.
"""
import sys
import pathlib
import pycdlib

OUT_DIR = pathlib.Path(__file__).resolve().parent

def dump_bytes(data, name):
    print(f"\n{name}: {len(data)} bytes")
    for i in range(0, min(len(data), 1024), 16):
        chunk = data[i:i+16]
        hexs = " ".join(f"{b:02x}" for b in chunk)
        asci = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        print(f"  {i:04x}  {hexs:<48s}  {asci}")
    if len(data) > 1024:
        print(f"  ... ({len(data) - 1024} more bytes)")

def process(path, idx):
    print(f"\n{'='*72}\n[{idx}] {path}\n{'='*72}")
    iso = pycdlib.PyCdlib()
    iso.open(path)
    root_entries = []
    for child in iso.list_children(iso_path="/"):
        if child is None or child.is_dot() or child.is_dotdot():
            continue
        root_entries.append(child.file_identifier().decode("ascii", "replace"))
    print("ROOT:", root_entries)

    # Look for CONTROL.TAT (case / version suffix tolerant)
    for ent in root_entries:
        bare = ent.split(";")[0].upper()
        if bare in ("CONTROL.TAT", "CONTROL.TAT."):
            buf = pycdlib.pycdlibio.BytesIO() if hasattr(pycdlib, "pycdlibio") else None
            import io
            out = io.BytesIO()
            iso.get_file_from_iso_fp(out, iso_path=f"/{ent}")
            data = out.getvalue()
            dump_bytes(data, f"/{ent}")
            dst = OUT_DIR / f"CONTROL.TAT.{idx}"
            dst.write_bytes(data)
            print(f"saved -> {dst}")
            break
    iso.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    for i, p in enumerate(sys.argv[1:], 1):
        try:
            process(p, i)
        except Exception as e:
            print(f"ERROR on {p}: {e}")
