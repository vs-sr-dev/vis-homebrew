#!/usr/bin/env python3
"""
WolfVIS A.23 SFX dump tool.

Reads AUDIOHED.WL1 + AUDIOT.WL1, extracts the 8 AdLib SFX chunks we
trigger, and reports:
  - chunk length (bytes; 0 = empty/missing)
  - parsed AdLibSound header (length, priority, instrument, block)
  - first 32 freq data bytes
  - whether the chunk exists in WL1 shareware

If pyopl is available, also renders each chunk to a WAV file at 44.1 kHz
mono so the user can audit "what it should sound like".

Usage: py -3 tools/dump_sfx.py
"""
import struct
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
AUDIOHED = ROOT / "cd_root_a23" / "AUDIOHED.WL1"
AUDIOT   = ROOT / "cd_root_a23" / "AUDIOT.WL1"
OUT_DIR  = ROOT / "tools" / "sfx_dump"
OUT_DIR.mkdir(exist_ok=True)

# AUDIOWL1.H STARTADLIBSOUNDS = 69. SFX trigger map (sound_id, label):
TRIGGERS = [
    (24, "ATKPISTOLSND",   "Player fires pistol"),
    (58, "NAZIFIRESND",    "Guard shoots back"),
    (21, "HALTSND",        "Guard yells halt"),
    (29, "DEATHSCREAM1SND","Guard dies"),
    (16, "TAKEDAMAGESND",  "Player hit"),
    (31, "GETAMMOSND",     "Pickup ammo clip"),
    (34, "HEALTH2SND",     "Pickup medkit"),
    (35, "BONUS1SND",      "Pickup treasure (cross)"),
]
START_ADLIB = 69


def load_offsets():
    raw = AUDIOHED.read_bytes()
    n = len(raw) // 4
    return list(struct.unpack(f"<{n}I", raw))


def extract_chunk(offsets, idx):
    if idx >= len(offsets) - 1:
        return b""
    off = offsets[idx]
    nxt = offsets[idx + 1]
    length = nxt - off
    if length <= 0:
        return b""
    with AUDIOT.open("rb") as f:
        f.seek(off)
        return f.read(length)


def parse_adlib(data):
    """Parse AdLibSound header: 6 B SoundCommon + 16 B Instrument + 1 B block + data[]."""
    if len(data) < 23:
        return None
    length, priority = struct.unpack("<IH", data[:6])
    inst = data[6:22]   # 16 bytes
    block = data[22]
    payload = data[23:]
    # Map to canonical labels
    inst_labels = {
        "mChar":   inst[0],  "cChar":   inst[1],
        "mScale":  inst[2],  "cScale":  inst[3],
        "mAttack": inst[4],  "cAttack": inst[5],
        "mSus":    inst[6],  "cSus":    inst[7],
        "mWave":   inst[8],  "cWave":   inst[9],
        "nConn":   inst[10],
    }
    return {
        "length": length,
        "priority": priority,
        "inst": inst_labels,
        "block": block,
        "block_decoded": ((block & 7) << 2) | 0x20,
        "payload": payload[:length],
        "payload_full_len": len(payload),
    }


# ---- Pure-python OPL2 minimal emulator (good enough for dumping) ----
# Implements 1 channel = 2 ops (modulator + carrier), FM phase modulation,
# AR/DR/SR/RR ADSR, sine output. Ignores tremolo/vibrato/feedback/wave-select
# beyond sine.

import math

OPL_RATE = 49716.0    # canonical OPL2 sample rate
OUT_RATE = 44100
F_REF = 49716.0 / (1 << 20)  # Hz per F-num at block 0

class Operator:
    def __init__(self):
        self.tl = 63          # total level (0..63, attenuation)
        self.ar = 0; self.dr = 0; self.sl = 0; self.rr = 0
        self.mult = 1
        self.env = 0.0
        self.phase = 0.0
        self.state = "off"

    def set_inst(self, ch, sc, at, su, wv):
        # AlChar (0x20): KSR/EG/VIB/AM/MULT (we use only MULT)
        self.mult = ch & 0x0F
        if self.mult == 0:
            self.mult = 0.5
        # AlScale (0x40): KSL/TL
        self.tl = sc & 0x3F
        # AlAttack (0x60): AR/DR
        self.ar = (at >> 4) & 0x0F
        self.dr = at & 0x0F
        # AlSus (0x80): SL/RR
        self.sl = (su >> 4) & 0x0F
        self.rr = su & 0x0F
        # AlWave (0xE0): waveform (0=sine; we always use sine for simplicity)

    def keyon(self):
        self.state = "attack"
        self.phase = 0.0
        # Reset envelope to 0 (silent), attack to peak
        self.env = 0.0

    def keyoff(self):
        self.state = "release"

    def step(self, freq_hz, dt):
        # Envelope (very rough; vanilla OPL is exponential)
        if self.state == "attack":
            rate = (1 << self.ar) * 50.0
            self.env += rate * dt
            if self.env >= 1.0:
                self.env = 1.0
                self.state = "decay"
        elif self.state == "decay":
            target = (15 - self.sl) / 15.0   # SL: 0=loudest, 15=quiet
            rate = (1 << self.dr) * 5.0
            self.env -= rate * dt
            if self.env <= target:
                self.env = target
                self.state = "sustain"
        elif self.state == "release":
            rate = (1 << self.rr) * 5.0
            self.env -= rate * dt
            if self.env <= 0.0:
                self.env = 0.0
                self.state = "off"

        # Output
        amp = self.env * (10.0 ** (-self.tl / 20.0))   # TL in dB-ish steps
        self.phase += freq_hz * self.mult * dt
        sample = math.sin(2.0 * math.pi * self.phase) * amp
        return sample


def render_chunk(parsed, out_path):
    """Render the SFX chunk to a 44.1 kHz mono WAV via minimal OPL2 emulation."""
    inst = parsed["inst"]
    block = parsed["block"]
    payload = parsed["payload"]

    mod = Operator()
    car = Operator()
    mod.set_inst(inst["mChar"], inst["mScale"], inst["mAttack"], inst["mSus"], inst["mWave"])
    car.set_inst(inst["cChar"], inst["cScale"], inst["cAttack"], inst["cSus"], inst["cWave"])

    # Sound runs at 140 Hz (one byte per 1/140 sec)
    SFX_HZ = 140
    sec_per_byte = 1.0 / SFX_HZ
    samples_per_byte = int(OUT_RATE * sec_per_byte)
    total_samples = samples_per_byte * len(payload) + OUT_RATE // 4   # +0.25 s tail

    out = []
    cur_freq = 0.0
    keyed = False
    for byte in payload:
        if byte == 0:
            car.keyoff()
            mod.keyoff()
            keyed = False
        else:
            # Compute Hz: F_REF * fnum * 2^block
            fnum = byte
            hz = F_REF * fnum * (1 << (block & 7))
            cur_freq = hz
            if not keyed:
                car.keyon()
                mod.keyon()
                keyed = True

        # Run samples_per_byte samples at this freq
        for _ in range(samples_per_byte):
            dt = 1.0 / OUT_RATE
            # Modulator output feeds into carrier phase
            mod_out = mod.step(cur_freq, dt)
            car_freq_mod = cur_freq + mod_out * cur_freq * 2.0   # rough FM
            sample = car.step(car_freq_mod, dt)
            out.append(sample)

    # Tail (release phase)
    car.keyoff(); mod.keyoff()
    for _ in range(OUT_RATE // 4):
        dt = 1.0 / OUT_RATE
        sample = car.step(cur_freq, dt)
        out.append(sample)

    # Normalize and write WAV
    peak = max(abs(s) for s in out) or 1.0
    pcm = bytearray()
    for s in out:
        v = int(s / peak * 32000)
        pcm += struct.pack("<h", v)

    with wave.open(str(out_path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(OUT_RATE)
        wf.writeframes(bytes(pcm))


def main():
    if not AUDIOHED.exists() or not AUDIOT.exists():
        print(f"ERROR: AUDIOHED.WL1 / AUDIOT.WL1 missing under {AUDIOHED.parent}", file=sys.stderr)
        sys.exit(1)

    offsets = load_offsets()
    print(f"AUDIOHED.WL1: {len(offsets)-1} chunks (file size sentinel = offsets[-1])")
    print()

    summary = []
    for sid, label, desc in TRIGGERS:
        chunk_idx = START_ADLIB + sid
        data = extract_chunk(offsets, chunk_idx)
        if not data:
            summary.append((label, desc, chunk_idx, 0, "EMPTY/MISSING"))
            print(f"{label:20s} chunk {chunk_idx:3d} = EMPTY/MISSING ({desc})")
            continue
        parsed = parse_adlib(data)
        if parsed is None:
            summary.append((label, desc, chunk_idx, len(data), "TOO SHORT"))
            print(f"{label:20s} chunk {chunk_idx:3d} = TOO SHORT ({len(data)} bytes, < 23 hdr)")
            continue
        status = "OK" if parsed["length"] > 0 else "ZERO LENGTH"
        summary.append((label, desc, chunk_idx, len(data), status))
        print(f"{label:20s} chunk {chunk_idx:3d} = {status}")
        print(f"    file_chunk_len={len(data)}, hdr.length={parsed['length']}, "
              f"priority={parsed['priority']}, block=0x{parsed['block']:02X} "
              f"-> alBlock=0x{parsed['block_decoded']:02X}")
        print(f"    inst: {parsed['inst']}")
        print(f"    payload first 32: "
              f"{[f'{b:02X}' for b in parsed['payload'][:32]]}")
        # Render
        if parsed["length"] > 0:
            try:
                wav_path = OUT_DIR / f"{label}_chunk{chunk_idx}.wav"
                render_chunk(parsed, wav_path)
                print(f"    -> rendered: {wav_path.relative_to(ROOT)}")
            except Exception as e:
                print(f"    -> render FAILED: {e}")
        print()

    print("=" * 70)
    print("SUMMARY:")
    for label, desc, idx, sz, status in summary:
        print(f"  {label:20s} chunk {idx:3d}  size={sz:5d} B  {status}")
    print()
    print(f"WAV files in: {OUT_DIR.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
