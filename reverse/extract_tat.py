#!/usr/bin/env python3
"""Extract CONTROL.TAT from all VIS BIN files in ../isos/, write into ./ (next to this script)."""
import io
import pathlib
import pycdlib

HERE = pathlib.Path(__file__).resolve().parent
SRC = HERE.parent / "isos"
OUT = HERE

def bin2iso(bin_path: pathlib.Path) -> bytes:
    """Strip Mode1/2352 sync+header+ECC, return pure 2048-byte-per-sector image."""
    raw = bin_path.read_bytes()
    if len(raw) % 2352 != 0:
        raise ValueError(f"{bin_path} size not multiple of 2352")
    n = len(raw) // 2352
    buf = bytearray(n * 2048)
    for i in range(n):
        src = i * 2352 + 16          # skip 12 SYNC + 4 HEADER
        dst = i * 2048
        buf[dst:dst + 2048] = raw[src:src + 2048]
    return bytes(buf)

def dump(data, label, width=16):
    print(f"\n{label}  ({len(data)} bytes)")
    for i in range(0, len(data), width):
        chunk = data[i:i+width]
        hx = " ".join(f"{b:02x}" for b in chunk)
        ax = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        print(f"  {i:04x}  {hx:<47s}  {ax}")

for bin_path in sorted(SRC.glob("*.bin")):
    print(f"\n{'='*78}\n{bin_path.name}\n{'='*78}")
    iso_bytes = bin2iso(bin_path)
    iso = pycdlib.PyCdlib()
    iso.open_fp(io.BytesIO(iso_bytes))
    # List root
    root = []
    for child in iso.list_children(iso_path="/"):
        if child is None or child.is_dot() or child.is_dotdot():
            continue
        root.append(child.file_identifier().decode("ascii", "replace"))
    print("ROOT:", root)
    # Extract CONTROL.TAT
    tat_name = None
    for ent in root:
        bare = ent.split(";")[0].upper()
        if bare == "CONTROL.TAT":
            tat_name = ent
            break
    if not tat_name:
        print("  ! CONTROL.TAT not in root")
        iso.close()
        continue
    out = io.BytesIO()
    iso.get_file_from_iso_fp(out, iso_path=f"/{tat_name}")
    data = out.getvalue()
    dst = OUT / f"CONTROL.TAT.{bin_path.stem.split(' ')[0]}"
    dst.write_bytes(data)
    dump(data, f"CONTROL.TAT  (saved {dst.name})")
    iso.close()
