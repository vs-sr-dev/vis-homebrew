# VIS Homebrew

Homebrew development for the **Tandy/Memorex Video Information System (VIS)** — a 1992 multimedia console running Modular Windows 3.1 on an Intel 80286 @ 12 MHz, with a Yamaha YMF262 (OPL3) and a Mitsumi 1× CD-ROM.

The headline goal of this repo is a **Wolfenstein 3D port** running natively as a Win16 NE on Modular Windows VIS, rendered via GDI palette blits, with OPL3 audio over direct port I/O and hand-controller input.

Detailed per-session log: see [VIS_sessions.md](VIS_sessions.md).

## Status

| Milestone | What it proves | Status |
|---|---|---|
| Hello World (S1) | Win16 toolchain + ISO + MAME boot path + CONTROL.TAT clone | ✅ |
| A.1 — Renderer foundation | Win16 chunky 320×200×8 + StretchDIBits + palette realization | ✅ |
| A.2 — Animation loop | DIB_PAL_COLORS fast path (5–6 FPS baseline) | ✅ |
| A.3 — Wolf3D palette | GAMEPAL.OBJ parser + 256-color grid on MAME VIS | ✅ |
| A.4 — VSWAP walls | Runtime asset loader from CD + 5 wall textures | ✅ |
| A.5 — VSWAP sprites | Sprite post format decoder + DrawSprite | ✅ |
| A.6 — HC input | Empirical VK_HC1_* codes (range 0x70..0x79) reverse-engineered | ✅ |
| A.7 — GAMEMAPS | MAPHEAD/Carmack/RLEW decompressors + minimap E1L1 | ✅ |
| A.8 — OPL3 | Direct port I/O 0x388/0x389 + sustained A4 note | ✅ |
| A.9 — Perf refactor | Static-bg snapshot + cursor erase/redraw + dirty-rect | ✅ |
| A.10 — IMF music | AUDIOT.WL1 / AUDIOHED.WL1 parser + IMF event scheduler over OPL3 (PoC) | ✅ |
| A.11 — Integrated scene | Walls + sprites + minimap + cursor + audio composited in one frame | ✅ |
| A.12 — Sprite scaler | Per-column post-walk fixed-point scaler over t_compshape sprites | ✅ |
| A.13 — Raycaster | Textured wall casting (DDA step-by-fraction) + ceiling/floor + player nav | ✅ |
| A.14 — Sprites in world | Static-decoration billboards over the cast scene + 1D z-buffer + painter's sort | ✅ |
| A.14.1 — Doors | DOORWALL texture + per-tile state machine + sliding slab + PRIMARY toggle | ✅ |
| A.15 — HUD | Wolf3D-style status bar with 7 panels + 4×6 digit font + face placeholder | ✅ |
| A.16a — Static enemies | Guard billboards (108..115 + 144..151 + 180..187) + 8-direction CalcRotate via atan2 LUT | ✅ |
| A.13.1 — Raycaster polish | Grid-line DDA + Tier-3 wall variety (32 pages, side-aware light/dark) + Watcom `-ox` discovery + time-scaled door anim + tight inner loops + partial-src StretchDIBits | ✅ |
| A.16b — Enemy AI ticker | State machine (Stand/Walk) + 32 walking frames + LOS Bresenham + 8-dir snap chase + sub-tile movement + per-axis collision + time-scaled phase advance | ✅ |

## Repository layout

```
src/             Win16 / DOS sources (.c, .lnk, .bat) + ISO build scripts (.py)
reverse/         BIOS recon scripts (BIOS dumps and extracted CONTROL.TAT excluded — see Assets)
VIS_sessions.md  Per-session work log with approaches, traps, and discoveries
README.md        This file
```

The following directories are git-ignored — they are either fetchable, regenerable, or copyrighted third-party material:

- `tools/` — Open Watcom V2 install (~537 MB; download from the project upstream)
- `docs/` — Modular Windows SDK PDFs (Microsoft, 1992)
- `assets/` — Wolfenstein 3D shareware data files (Apogee/id Software)
- `wolf3d/` — local clone of the Wolf3D source for reference
- `isos/` — retail VIS BIN/CUE images (Tandy/Memorex)
- `vis.zip`, `reverse/p513bk*.bin` — VIS BIOS extract (Tandy/Memorex)
- `build/`, `cd_root*/`, `cfg*/`, `nvram/` — build outputs and MAME runtime state

## Quick start

### Dependencies

- **Open Watcom V2** (Win16 toolchain). Install under `tools/OW/` or anywhere — adjust the `WATCOM` env in the `build_*.bat` scripts.
- **Python 3.10+** with `pycdlib` (`pip install pycdlib`) for ISO mastering.
- **MAME 0.287+** with the `vis` BIOS (ROM set `vis.zip` containing `p513bk0b.bin` + `p513bk1b.bin`).
- **VIS retail disc** (any one) to generate a valid `CONTROL.TAT` for your homebrew ISO. The disc validation is non-cryptographic, so cloning the 12 binary "random" bytes from a retail TAT file is enough.
- **Wolfenstein 3D shareware** (`*.WL1` files) placed under `assets/` for the asset-driven milestones (A.4 onward).

### Build the latest milestone

```bash
cd src
cmd /c ".\build_wolfvis_a16b.bat"    # produces build/WOLFA16B.EXE
python mkiso_a16b.py                 # produces build/wolfvis_a16b.iso
```

### Run on MAME

```bash
mame -rompath . vis -cdrom build/wolfvis_a16b.iso -window -nomax -skip_gameinfo -nomouse
```

(Place `vis.zip` in the same `-rompath` directory.)

### Generate CONTROL.TAT from a retail disc

```bash
cd reverse
python extract_tat.py path/to/RetailDisc.iso
python make_control_tat.py "MY HOMEBREW TITLE"
```

The output `CONTROL.TAT` goes into your `cd_root_*/` staging directory next to `AUTOEXEC`, `SYSTEM.INI`, and your `*.EXE`.

## Notes on third-party assets and copyright

This repository contains **only original code and documentation** authored for this project. It does **not** include:

- VIS BIOS dumps (`p513bk0b.bin`, `p513bk1b.bin`, `vis.zip`, `reverse/dispdib_raw.bin`) — copyright Tandy/Memorex.
- Retail VIS disc images (Atlas of Presidents, Bible Lands, Fitness Partner) — copyright Tandy/Memorex.
- Microsoft Modular Windows SDK PDFs — copyright Microsoft.
- Wolfenstein 3D shareware data files (`VSWAP.WL1`, `GAMEMAPS.WL1`, `AUDIOT.WL1`, etc.) — copyright Apogee/id Software.
- The Wolf3D source code clone used as a reference for porting (`wolf3d/`) — separately licensed under the GNU GPL by id Software (1995 release); fetch from id's official source release if needed.

You will need to source these files yourself to reproduce the build. Pointers are documented in [VIS_sessions.md](VIS_sessions.md).

## License

The original code in this repository (everything under `src/` and `reverse/*.py`) is released under the MIT License unless otherwise noted.

## Credits

- MAME `vis` driver authors (`src/mame/trs/vis.cpp`).
- VTDA for hosting the Microsoft Modular Windows SDK archive ([MS37741_ModularSDK_Oct92](https://vtda.org/docs/computing/Microsoft/MS37741_ModularSDK_Oct92/)).
- Open Watcom V2 maintainers for the only practical free Win16 toolchain in 2026.
- id Software for the Wolfenstein 3D source release.
