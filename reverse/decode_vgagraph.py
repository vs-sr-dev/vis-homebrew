#!/usr/bin/env python3
"""Decode WL1 VGAGRAPH chunk 0 (pictable) and dump face pic dimensions.

Just a recon helper — establishes FACE1APIC layout before we write the
C-side Huffman + deplane decoder. Read VGADICT, VGAHEAD, VGAGRAPH from
assets/, expand chunk 0, print the pictable[FACE1APIC..FACE3CPIC].
"""
import struct
import sys
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"

NUMPICS = 136  # WL1 shareware
STARTPICS = 3
FACE1APIC = 113  # absolute chunk id


def load_dict(p: Path):
    """VGADICT.WL1 is 256 huffnodes, 4 bytes each (bit0:WORD, bit1:WORD)."""
    data = p.read_bytes()
    assert len(data) == 1024, f"VGADICT.WL1 expected 1024 bytes, got {len(data)}"
    nodes = []
    for i in range(256):
        b0, b1 = struct.unpack_from("<HH", data, i * 4)
        nodes.append((b0, b1))
    return nodes


def load_head(p: Path):
    """VGAHEAD.WL1: 24-bit LE offsets into VGAGRAPH, one per chunk + 1 sentinel."""
    data = p.read_bytes()
    n = len(data) // 3
    starts = []
    for i in range(n):
        b = data[i * 3 : i * 3 + 3]
        v = b[0] | (b[1] << 8) | (b[2] << 16)
        starts.append(v)
    return starts


def huff_expand(src: bytes, expanded_len: int, nodes: list) -> bytearray:
    """Bit-streamed Huffman expand. Head node = 254."""
    head = 254
    node_idx = head
    src_pos = 0
    byte_val = src[src_pos]
    src_pos += 1
    bit = 1
    out = bytearray(expanded_len)
    out_pos = 0

    while out_pos < expanded_len:
        b0, b1 = nodes[node_idx]
        code = b1 if (byte_val & bit) else b0
        bit <<= 1
        if bit == 256:
            byte_val = src[src_pos]
            src_pos += 1
            bit = 1
        if code < 256:
            out[out_pos] = code
            out_pos += 1
            node_idx = head
        else:
            node_idx = code - 256
    return out


def main():
    nodes = load_dict(ASSETS / "VGADICT.WL1")
    starts = load_head(ASSETS / "VGAHEAD.WL1")
    graph = (ASSETS / "VGAGRAPH.WL1").read_bytes()

    print(f"[head] {len(starts)} entries (NUMCHUNKS+1)")
    print(f"[head] chunk[0..3]   = {[hex(s) for s in starts[:4]]}")
    print(f"[head] chunk[110..117] = {[hex(s) for s in starts[110:118]]}")
    print(f"[head] last = {hex(starts[-1])}, file size = {hex(len(graph))}")

    # Decompress chunk 0 = pictable
    chunk0_pos = starts[0]
    chunk0_size = starts[1] - starts[0]
    expanded_len = struct.unpack_from("<I", graph, chunk0_pos)[0]
    print(f"\n[chunk 0] file_pos={hex(chunk0_pos)}, file_size={chunk0_size}, expanded={expanded_len}")
    comp = graph[chunk0_pos + 4 : chunk0_pos + chunk0_size]
    pic_table_raw = huff_expand(bytes(comp), expanded_len, nodes)

    # Parse pictabletype { int width, height; }
    pic_count = expanded_len // 4
    print(f"[pictable] {pic_count} entries (NUMPICS computed = {pic_count}, header NUMPICS = {NUMPICS})")
    pic_table = []
    for i in range(pic_count):
        w, h = struct.unpack_from("<hh", pic_table_raw, i * 4)
        pic_table.append((w, h))

    # Dump face pics. FACE1APIC = chunk 113 = pic index 113-STARTPICS = 110
    print("\n[face pics] (chunk_idx, pic_idx, width, height, expanded_size_via_W*H, chunk_compressed_size)")
    for chunk_idx in range(FACE1APIC, FACE1APIC + 13):
        if chunk_idx >= len(starts) - 1:
            break
        pic_idx = chunk_idx - STARTPICS
        if pic_idx >= len(pic_table):
            continue
        w, h = pic_table[pic_idx]
        comp_size = starts[chunk_idx + 1] - starts[chunk_idx]
        # Read expanded length stored in file
        exp_in_file = struct.unpack_from("<I", graph, starts[chunk_idx])[0]
        print(f"  chunk[{chunk_idx}] pic[{pic_idx}]: {w}x{h}, W*H={w*h}, file_exp={exp_in_file}, file_size={comp_size}")


if __name__ == "__main__":
    main()
