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
| A.17 — Weapon overlay | Vanilla SPR_PISTOLREADY blitted at viewport bottom-center, runtime chunk discovery (`total_sprites - 15`), 1:1 fixed-position blit via DrawSpriteFixed | ✅ |
| A.18 — Firing + hitscan + damage | PRIMARY rebind (door → SECONDARY), 4-frame ATK animation (PISTOLATK1..4 hot-swap), hitscan via z-buffer + first-hit screen-span scan, damage 5..12 + 3-frame DIE → frozen DEAD, ammo + score HUD partial re-blit | ✅ |
| A.15.1 — Real BJ face on HUD | VGAGRAPH chunked Huffman loader (VGADICT + VGAHEAD + VGAGRAPH), HuffExpand + 4-plane → linear deplane, FACE1APIC chunk 121 (empirical, not enum 113), 24×32 baked into static_bg, fallback to placeholder on load fail. Bundled fix: cardinal-angle DDA nudge for centre-column wall bleed | ✅ |
| A.19 — Centered viewport + minimap toggle (PF finale step 1) | `VIEW_X0` 0 → 96 (viewport horizontally centered), per-frame `DrawMinimapWithPlayer` removed (~25–30 ms/frame freed — H2 hot path captured), `VK_HC1_F1` (Xbox X) toggles a 64×64 centered minimap overlay, music F1/F3 debug bindings dropped (OPL/IMF infra dormant). User confirms "QUASI giocabile" — first PoC milestone where the gameplay framerate becomes usable for real navigation | ✅ |
| A.19.1 — Sprite scaler Q.16 accumulator (PF finale step 2) | `DrawSpriteWorld` inner pixel loop converts per-pixel long division (`sy_src = (dy - dy_top) * 64L / sprite_h`) into a `step_q16 = (64 << 16) / sprite_h` step accumulator (sy_acc / srcx_acc) — same primitive as `DrawWallStripCol`. dy bounds pre-clipped once per post, framebuf access via decrementing `__far` pointer (no per-pixel multiplication). Eliminates the close-enemy freeze (sprite_h saturated at 4×VIEW_H = 512 was ~10 M cyc per sprite per frame → now ~250 k cyc). User: "il rallentamento con guardia vicina rimane, ma è meno bloccante rispetto a prima (nessun freeze, solo drop fps)" — H1 freeze closed; residual cost is linear pixel volume | ✅ |
| A.19.2 — Micro-perf bundle (PF finale step 3) | `DrawWallStripCol` ceiling/wall/floor fills do WORD pair-writes (single aligned word store replaces two byte stores: ~6 cyc/pair → ~3 cyc/pair on 286). `DrawSpriteWorld` outer column loop pre-clips `dest_col` to viewport and seeds `srcx_acc` so off-viewport iterations are skipped entirely; inner bound check dropped. EXE 229,736 B (slightly smaller than A.19.1 — pair-write loops compile to fewer instructions). User verdict shifts "QUASI giocabile" → **"Giocabile"**, with residual close-quarter drop reframed by user as a non-realistic gameplay state ("le guardie sparano da lontano e un giocatore muore molto prima di raggiungerle da vicino"). Defensive: build batch now atomically copies EXE into `cd_root_a21/` to prevent the recurring "shell missing → PROGMAN.EXE error" trap | ✅ |
| A.21 — DispDib direct A000 framebuffer + post-port polish | S17 disasm of `DisplayDibCommon` (file 0x154 of `dispdib_raw.bin` after correcting the runtime-loader segment-base bug) identifies `BEGIN=0x8000` / `END=0x4000` and supersedes the S16 `NOWAIT=0x0100` misattribution (it was actually STRETCH 2X). KERNEL `__A000H` magic-constant export gives the runtime selector via `(WORD)((DWORD)(LPVOID)&_A000H)` — the OFFSET part of the far ptr IS the selector. WolfVis port replaces per-frame `StretchDIBits + Select/RealizePalette` with one-time DispDib BEGIN at startup + per-frame `_fmemcpy(g_fb_a000, framebuf, ...)` with bottom-up flip + DispDib END at WM_QUIT. Bonus polish: debug bar removed entirely (was the residual 500-ms heartbeat freeze via dirty-rect merge with `InvalidatePlayerView`); FireWeapon now returns BOOL so spam-at-zero-ammo / mid-animation taps don't trigger a wasted full-viewport redraw. EXE 229,102 B. Net: 7-8 FPS → 14-18 FPS (~2× speedup, dominant cost was GDI not raycaster) + freeze gone + spam fix. User verdict: "voglia di proseguire oltre le prime due guardie e stanze come invece fatto finora" — first time a build crosses from tech-demo to actually-engaging | ✅ |
| A.20 — Enemy fire-back + player health | New `OBJ_ST_SHOOT` 3-frame state (286 ms/phase, T_Shoot fires phase 0→1) wired into `AdvanceEnemies` chase trigger using vanilla `T_Chase` formula `chance = (tics<<4)/dist`. `ShootPlayer` reverse hitscan (LOS recheck + `hitchance = 256-dist*8` + tiered damage). `g_player_hp` 0..100 with HUD HEALTH panel redraw (red below 25). On `g_player_hp == 0`: world freezes, PRIMARY taps trigger `RestartLevel` (reset hp/ammo/score/kills + re-spawn + full repaint). 8 subsystems, single-iter zero-fix. wolfvis_a20.c baseline | ✅ |
| A.20.1 — Combat fairness + damage flash polish | Hitchance softened from 256-dist*8 (vanilla "not visible") → 160-dist*16 (vanilla "visible") since `LOSCheck`-pass implies symmetric visibility = vanilla `FL_VISABLE` case where player has dodge window. Net: ~50% hit at dist=2 (was ~94%). Damage flash: 5-px red border (color 40 = HUD_FG_LOW) painted around viewport on first 2 of 3 ticks via `g_damage_flash_ticks` counter; final tick skips paint = clear-frame guarantee. WM_TIMER forces redraw while flash counter active so the sequence renders even on idle hit. User verdict: "fair (78/100 dopo due guardie)". wolfvis_a201.c baseline | ✅ |
| A.22 — Pickups (medkit / clip / treasures / 1up) | 8 pickup kinds at obj_id 47..56 → VSWAP chunks 26-35: food (+10 hp), medkit (+25 hp), clip (+8 ammo), cross (+100), chalice (+500), bible (+1000), crown (+5000), 1up (+99 hp + 25 ammo). `Object.pickup_kind` BYTE field, `ScanObjects` pickup branch with `switch (obj)` for sparse mapping, `CheckPickups` same-tile proximity grab in WM_TIMER, `TryGiveBonus` with vanilla gates (hp==100 blocks health, ammo==99 blocks clip — pickup stays on floor). Removed pickups marked via `sprite_idx = -1` (DrawSpriteWorld + painter sort short-circuit on negative). **Trap caught**: `MAX_OBJECTS=128` overflow silently dropped guards in lower-y rows; bumped to 256, diagnosed via HUD-panel digit hijack (LEVEL/LIVES showing g_num_enemies/g_num_static at runtime). wolfvis_a22.c baseline | ✅ |
| A.23 — AdLib SFX subsystem (8 trigger points) | OPL3 ch0 SFX driven by 140-Hz frequency-byte stream from AUDIOT.WL1 chunks 69..137 (vanilla AdLib bank). `LoadSfx(chunk_idx)` parses 23-byte header (length + priority + 16-byte instrument + block) + body data. `WriteSfxInstrument` writes 11 OPL registers per vanilla `SDL_AlSetFXInst` (modifier op = reg 0, carrier op = reg 3, alFeedCon = 0). `ServiceSfx` PIT-direct accumulator (`PIT_CYCLES_PER_SFX_TICK = 596400/140 = 4260`) consumes 1 freq byte per tick: `freq=0` = key off, else freqL + alBlock keyon. 8 trigger points wired (ATKPISTOL on FireWeapon, NAZIFIRE on ShootPlayer T_Shoot, HALT on WALK→SHOOT transition, DEATHSCREAM on lethal DamageEnemy, TAKEDAMAGE on DamagePlayer, GETAMMO/HEALTH/BONUS on TryGiveBonus). PeekMessage idle loop refactored: ServiceMusic + ServiceSfx after every Translate/Dispatch + ~14 mid-frame call sites (every 16 cols inside DrawViewport + post-render + mid-WM_TIMER) so accumulator never needs a cap (cap eats real time = "half speed", uncapped+sparse = bursty clicks; dense calls eliminate both). Bonus tool: `tools/dump_sfx.py` extracts AdLib chunks + renders via minimal pure-Python OPL2 emulator to WAV reference set in `tools/sfx_dump/`. **A.23.2 follow-up** bumped `MOVE_STEP_Q88` 24→64 (~2.7×) restoring vanilla RUN feel — was at exactly vanilla WALK speed (1.875 tile/sec) since A.6. User verdict: "Direi meglio ora!" + "Giocabilità molto migliorata" | ✅ |

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
cmd /c ".\build_wolfvis_a23.bat"    # produces build/WOLFA23.EXE + stages it in cd_root_a23/
python mkiso_a23.py                  # produces build/wolfvis_a23.iso
```

### Run on MAME

```bash
mame -rompath . vis -cdrom build/wolfvis_a23.iso -window -nomax -skip_gameinfo -nomouse
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
