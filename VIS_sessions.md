# VIS Homebrew — Sessions log

Tandy/Memorex Video Information System (MD-2500), 1992.
CPU Intel 80286 @ 12 MHz, 1 MB RAM, Yamaha YMF262 OPL3 + 16-bit R-2R DAC stereo, ADAC-1 video, Mitsumi 1× CD-ROM, Modular Windows 3.1 in 1 MB mask ROM.

---

## Session 1 — 2026-04-24 — Feasibility + Hello World

**Scope:** full technical recon + reach a Hello World runnable in emulator.

**User prediction:** "uncharted territory but potentially simple" (DOS + Windows variant).
**Actual result:** simple, baptism completed. Hello World runs in MAME by end of session.

### Recon

**Hardware (MAME driver `vis` in `src/mame/trs/vis.cpp`, v0.287):**
- CPU Intel 80286 @ 12 MHz (performance ~386SX-16/20 thanks to 0 wait state on local bus)
- 1 MB RAM (640 KB conventional + 384 KB extended)
- 1 MB mask ROM: minimal MS-DOS 3.x + drivers + Modular Windows 3.1
- Yamaha YMF262 (OPL3) @ 14.318 MHz + 2× DAC 16-bit R-2R stereo (Adlib Gold compat, NOT Sound Blaster)
- ADAC-1 video (YUV + RGB), VGA 640×480 @ 53 Hz, also 320×200x8 via TVVGA
- Mitsumi 1× CD-ROM (150 KB/s)
- Hand controller IR/wired, Dallas Save-It memory cartridge (32 KB removable NVRAM)
- MAME BIOS: `vis.zip` = `p513bk0b.bin` + `p513bk1b.bin` (512 KB × 2)

**Software SDK:** Microsoft Modular Windows SDK (codename "Haiku") Oct 1992. Full archive at [VTDA MS37741_ModularSDK_Oct92](https://vtda.org/docs/computing/Microsoft/MS37741_ModularSDK_Oct92/) — downloaded into `docs/`: Getting Started (1.8 MB), Design Guide (10 MB), Programmer's Reference (12 MB).
- Win 3.1 API with reduced surface: no menu, no sizeable window borders, no disk writes, hand control is the primary input
- Header MODW_API.H as a drop-in for WINDOWS.H with unsupported APIs removed
- Detect VIS at runtime: `int 2Fh` AH=0x81 AL=0x00 → AL=0xFF if launcher present
- Clean exit: `int 2Fh` AH=0x81 AL=0x11 (open door) + `int 19h`

**VIS-bootable CD (from Getting Started):**
- Root requires: `AUTOEXEC` (text: `modwin a:`), `SYSTEM.INI` (shell=APP.EXE + driver stack), `APP.EXE`, **`CONTROL.TAT`** (VIS-specific validation)

### Approaches

**1. Win16 toolchain.** Open Watcom V2 current build (GitHub). ~128 MB download `open-watcom-2_0-c-win-x64.exe`. User install (UAC required) under `tools\OW`. Layout: `binnt64/wcc.exe wlink.exe wcl.exe wdis.exe`, headers `h/win/`, Win16 libs in `lib286/win/`.

**2. Build Hello World.**
- `src/hello.c` — WinMain + WndProc, CreateWindow WS_POPUP fullscreen, multiline DrawText
- `src/hello.def` — module definition (NAME HELLO, DESCRIPTION, HEAPSIZE, STACKSIZE)
- `src/link.lnk` — Watcom linker script (SYSTEM windows, NAME, FILE, OPTION)
- `src/build.bat` — sets WATCOM/PATH/INCLUDE env, calls `wcc` + `wlink`

Build succeeded on second attempt. Output: `build/HELLO.EXE` 2080 bytes, NE header at offset 0x80, canonical MZ stub.

**3. ISO 9660 Level 1.** `pycdlib` (pip install). Script `src/mkiso.py` takes everything from `cd_root/` and writes `build/hello.iso`. Quirk: a file with no extension (AUTOEXEC) needs `.` to be valid L1 → `AUTOEXEC.;1`.

**4. MAME launch.** `vis` driver standalone with `:mcd` slot for CD. CD-ROM is A: in the emulated DOS environment. Canonical command:
```
mame -rompath . vis -cdrom build/hello.iso -window -nomax -skip_gameinfo
```

**First test:** BIOS boots, VIS logo appears, then RED-BLACK SCREEN: `This disc cannot be used on this system. Insert a disc or a cartridge.` My first `SYSTEM.INI` had no `CONTROL.TAT`.

**5. Reverse engineering CONTROL.TAT.**
- Extracted BIOS ROMs (torrentzip, MAME MD5-matching): `p513bk0b.bin` and `p513bk1b.bin` in `reverse/`.
- String grep: the reject message lives in bank1 at offset 0x6a530; shortly after at 0x6a5e5 there's `a:control.tat` and at 0x6a584 the template `[ ATTENTION: This is an Authorized Video Information System.Title. END OF STATEMENT ]`.
- First hypothesis: write CONTROL.TAT with the literal ATTENTION string. FALSIFIED — disc still rejected.
- Pivot: user procures 3 real VIS discs (Atlas of Presidents, Bible Lands, Fitness Partner) into `isos/` as BIN/CUE Mode1/2352.
- Script `reverse/extract_tat.py`: converts BIN→ISO on-the-fly (strip 16B sync+header per sector) and reads CONTROL.TAT via pycdlib.
- **Decoded CONTROL.TAT structure (474 B)**: fixed 82B copyright + variable 60B title + Ctrl-Z header + 12 binary "random" bytes + `fdiv` struct + padding + `03 00 1a 00 00 00 00 00` + ATTENTION statement with byte `a0` (not `. `) between "System" and "Title" + `minwin A:\` + `Maketat - Version is ...` tagline.
- Original source tool was Tandy's `MAKETAT.EXE` (mentioned in tagline), not publicly preserved.
- **Test 2:** clone Atlas's CONTROL.TAT byte-for-byte, changing ONLY the Title. `reverse/make_control_tat.py`.

**6. Rebuild + relaunch.** VALIDATION PASSED. New screenshot: Win3.1 dialog with `Please insert the main disc and press OK.` — therefore Modular Windows is running, but HELLO.EXE launch isn't completing.

**7. Fix SYSTEM.INI.** Inspecting `isos/Atlas.../SYSTEM.INI` shows `shell=a:\gprs\GPRS.EXE` — ABSOLUTE path. I had `shell=hello.exe` relative. Change to `shell=a:\HELLO.EXE`. Add empty `network.drv=` and `language.dll=` like Atlas.

**8. FINAL TEST.** `build/snap/vis/0000.png` shows HELLO.EXE running on emulated Modular Windows with centered text: "Hello, Tandy/Memorex VIS! / Modular Windows homebrew lives."

### Concrete results

- `tools/OW/` — Open Watcom V2 installed (~300 MB)
- `docs/` — Modular Windows SDK PDF + text (23 MB PDF + 580 KB .txt extracted via poppler)
- `reverse/` — BIOS extract + 3 real CONTROL.TAT + generator + extract script
- `src/hello.c`, `src/hello.def`, `src/link.lnk`, `src/build.bat`, `src/mkiso.py` — project sources
- `cd_root/` — CD staging: AUTOEXEC, SYSTEM.INI, CONTROL.TAT, HELLO.EXE
- `build/HELLO.EXE` — 2080 B Win16 NE
- `build/hello.iso` — 58 KB VIS-bootable ISO 9660 L1
- `build/snap/vis/0000.png` — proof of work

### Trap / Gotcha / Eureka

- **Gotcha W1:** Open Watcom installer demands UAC even when renamed. Binary has `requireAdministrator` manifest. User installed manually.
- **Gotcha W2:** `wcl` passes the `.def` file to the C compiler instead of the linker → absurd syntax errors. **Use `wcc` + `wlink` separately with explicit linker script.**
- **Gotcha W3:** in `mkiso.py`, a file with no extension (AUTOEXEC) needs `/AUTOEXEC.;1` (explicit dot) to be valid ISO 9660 L1.
- **Gotcha W4:** `shell=` in SYSTEM.INI must be ABSOLUTE path (`a:\HELLO.EXE`). Relative path → "Please insert the main disc" (Modular Windows can't find shell → fallback).
- **Eureka E1:** VIS disc validation is NOT cryptographic. The 12 "random" bytes at offset 0x94 of CONTROL.TAT are NOT verified — clone-with-title-swap works.
- **Eureka E2:** Tandy's original tool was `MAKETAT.EXE` (version 1(12) 31-Aug-92 or 1(13) 9-Oct-92). Not publicly preserved but reconstructible from the structure.
- **Eureka E3:** MAME `vis` driver accepts raw ISO (besides BIN/CUE and CHD) directly on `-cdrom`. No CHD conversion needed for testing.
- **Eureka E4:** The "uncharted territory" user prediction was overstated. VIS is a proprietary console but 100% based on standard era tech (286+Win3.1+OPL3), much more accessible than consoles with custom toolchains.

### Next steps for Session 2

Open technical priorities:
1. **OPL3 audio output.** Test sound generation via OPL3 (port 0x388/0x389 or via `mmsystem`/MIDI driver). SDK mentions `vwavmidi.drv` + MIDI base-level mode default + general MIDI mode via SysEx `F0 7E 7F 09 01 F7`.
2. **Hand control input.** Use `HC.DLL` (HC.A header) — hand control events via WM_HC_* messages. WinShell sample as reference.
3. **Higher-resolution display modes.** VIS default = 640×400 HighRes, LowRes 320×200x8 via `[TVVGA] resolution=320x200x8` in SYSTEM.INI. Verify rendering works.
4. **Save-It memory cartridge.** `MC.DLL` API — mcFormat, mcRead, mcWrite, mcRegister for NVRAM persistence (32 KB typical).
5. **YUV video display.** DisplayDib + Convert24 utility (mentioned in SDK, not owned). Video playback needs MCIavi + RLE compressor.

User's choice. Technically recommended start at (1) OPL3 audio: Yamaha YMF262 is the most interesting VIS chip and would be definitive proof we have access to the silicon.

Open question: where is the original `MAKETAT.EXE`? Archive.org VIS collection might contain it. Reconstruction of the tool from the decoded structure is now 100% feasible (not needed).

---

## Session 2 — 2026-04-24 — "Crazy idea" Wolf3D + architectural pivot + Milestone A.1

**Scope:** explore the idea conceived at end of S1 (Wolf3D on VIS). S2 was supposed to be: DOS real-mode smoke test → real porting. Actual: demolition of the DOS plan, pivot to Win16 native port, Milestone A.1 completed (chunky 256-color renderer proven).

### Approaches

**Step 0 — DOS real-mode smoke test via AUTOEXEC.**

Assumption from S1 recon: AUTOEXEC can launch DOS `.EXE` before `modwin`, allowing pure DOS real-mode Wolf3D. Never tested in S1.

*Approach 0.1:* `src/dosh.c` Watcom DOS 16-bit small model + `wlink SYSTEM dos` → DOSH.EXE 2424 B. AUTOEXEC just `a:\dosh.exe`. Result: **reboot loop**, flash of "Error loading..."

*Approach 0.2:* add `modwin a:` as second AUTOEXEC line (for syntactic satisfaction; if dosh blocks, modwin is never reached). Still reboot loop — user reads "Error loading PROGMAN.EXE" in one frame.

*Approach 0.3:* eliminate the problem at the root — DOSH.COM tiny 31 B hand-assembled (`mov ah,9; mov dx,0x109; int 21h; jmp $; "DOS MODE ALIVE ON VIS$"`). AUTOEXEC `\dosh.com` + `modwin a:`. Reboot loop persists.

*Approach 0.4:* DOSH.COM reduced to 2 B `EB FE` (pure hang), added HELLO.EXE to CD as shell fallback. **Hello World shows up.** Meaning: DOSH.COM SKIPPED by the launcher. modwin reached, HELLO.EXE shell worked.

*Approach 0.5:* DHANG.EXE hand-assembled valid MZ (34 B, EB FE). AUTOEXEC `A:\DHANG.EXE` uppercase+drive, + modwin. Hello World. Even the valid MZ skipped.

*Approach 0.6:* AUTOEXEC = ONLY `A:\DHANG.EXE` (no modwin). **Hello World still shows up**. **Definitive conclusion: the tlaunch/minwin launcher in VIS ROM completely ignores AUTOEXEC content and always forces `modwin a:`.** The `\init.exe` example in `docs/getting_started.txt:697` is aspirational, not implemented in VIS firmware. Pure DOS real-mode on VIS is **impossible**.

**Step 0.5 — Test B: WinExec bridge.**

*Approach B.1:* Win16 stub `wxbridge.c` → `WinExec("A:\\DHANG.EXE", SW_SHOWNORMAL); return 0;`. Shell in SYSTEM.INI. Result: reboot loop. **WinExec on a DOS app not implemented in the Modular Windows VIS variant.** Path B dead. User confirms: "definitely going with A".

**Milestone A.1 — Win16 native renderer foundation.**

*Approach A.1:* `src/wolfvis.c` — Win16 NE with BYTE framebuf[64000], BITMAPINFO 256 RGBQUAD, WinMain + RegisterClass + CreateWindow WS_POPUP 640x480, WM_PAINT does SetDIBitsToDevice on the framebuf with `(x+y)&0xFF` gradient.

*Watcom trap:* `#define SCR_PIX (SCR_W * SCR_H)` → `E1020 Dimension cannot be 0 or negative`. 320*200=64000 overflows signed 16-bit int. Fix: literal `64000` directly.

Build OK. CD + MAME: black screen + cursor. WM_PAINT firing? Added debug: red FillRect + `TextOut "WOLFVIS paint hit"` + `SetDIBitsToDevice` + `TextOut "DIB rc=N"`. Runtime: **red + paint hit + "DIB rc=0"**. SetDIBitsToDevice fails.

*Approach A.1b:* StretchDIBits instead of SetDIBitsToDevice, positive biHeight (bottom-up DIB). Result: **rc=1 + visible gradient** (pink/cream/black bands, only 16-color).

*Approach A.1c:* added uppercase `[TVVGA]` section in SYSTEM.INI with `resolution=320x200x8`. No change — still 16-color 640x480.

*Approach A.1d:* extracted SYSTEM.INI from Atlas/Bible/Fitness retail discs. **Discovery: `[tvvga]` LOWERCASE** (Atlas uses display.drv=tvvga.drv, Bible uses display.drv=vga.drv; both with lowercase `[tvvga]`). INI parser is case-sensitive. Fix: lowercase section. Result: **pixels 2x larger (320x200 mode active) but still color bands → palette not realized**.

*Approach A.1e:* added `CreatePalette` LOGPALETTE with 256 `PALETTEENTRY.peFlags=PC_NOCOLLAPSE`, `SelectPalette` + `RealizePalette` in WM_PAINT, WM_QUERYNEWPALETTE + WM_PALETTECHANGED handlers. Result: **smooth black→red→yellow→white gradient at 256-color fullscreen 320x200**. Milestone A.1 completed.

### Concrete results

- `src/dosh.c`, `src/link_dosh.lnk`, `src/build_dosh.bat` — DOS Watcom toolchain (validated but dead path)
- `build/DOSH.EXE`, `build/DOSH.COM`, `build/DHANG.EXE` — DOS test binaries
- `src/wxbridge.c` + link/build — Win16 WinExec bridge (dead path)
- `src/wolfvis.c` (168 LOC) + link + build bat — Win16 256-color renderer foundation
- `build/WOLFVIS.EXE` 66 KB Win16 NE
- `build/wolfvis_a1.iso` 122 KB VIS-bootable ISO
- `cd_root_step0/`, `cd_root_testB/`, `cd_root_a1/` — CD staging dirs (test iterations)
- `cd_root_ctrl/` — control test (S1 config replica)
- 4 new memory files: `project_autoexec_firmware_limitation`, `project_wolf3d_path_A_commit`, `project_milestone_A1_complete`, `reference_win16_rendering_gotchas`

### Trap / Gotcha / Eureka

- **Gotcha S2.1:** Watcom 16-bit `array[320 * 200]` overflows signed int. Use literal 64000 or unsigned cast.
- **Gotcha S2.2:** `cmd /c build.bat` fails without `.\` prefix even when the file is present. CWD is not in cmd.exe PATH. Use `cmd /c ".\\build.bat"`.
- **Gotcha S2.3:** `SetDIBitsToDevice` rejected by the Modular Windows VIS driver. **Always use StretchDIBits.**
- **Gotcha S2.4:** Negative `biHeight` (top-down DIB) rejected. Must be positive (bottom-up).
- **Gotcha S2.5:** SYSTEM.INI uppercase `[TVVGA]` ignored. INI parser is case-sensitive: `[tvvga]` LOWERCASE.
- **Gotcha S2.6:** 256-color mode alone isn't enough. Without `CreatePalette` + `SelectPalette` + `RealizePalette` the blit uses 20 system colors → reduced to 3-4 visible bands.
- **Gotcha S2.7:** `getting_started.txt:697` doc with `\init.exe` before `modwin a:` is aspirational. tlaunch firmware on VIS ignores AUTOEXEC content.
- **Eureka S2.E1:** the VIS ROM has TLAUNCH + MINWIN + MSCDEX + REDIR + GBIOS + ROMA + ROMB components in the process list (bank 1 @ 0x5c3c0). Launcher is custom, not standard DOS.
- **Eureka S2.E2:** CONTROL.TAT found in BIOS reject strings + `minwin A:\` — suggestive of deep integration between CONTROL.TAT and launcher, but validation is NOT crypto (as discovered in S1).
- **Eureka S2.E3:** all 3 retail discs (Atlas/Bible/Fitness) have **identical** `[tvvga] resolution=320x200x8` lowercase — de facto standard.
- **Eureka S2.E4:** WinExec on a DOS app not supported in the Modular Windows VIS variant. Confirms VIS is effectively a Win16-only environment.

### Next-step candidates for Session 3

1. **Animation loop** — SetTimer + InvalidateRect for dynamic test (30 min). Confirms the Win16 main loop holds an acceptable frame-rate on the VIS 286.
2. **Wolf3D palette loader** — integrate VSWAP/GAMEPAL parser (simplified id CA cache manager) and replace gradient with the real Wolf3D palette.
3. **Stub renderer integration** — comment out ID_VL_A.ASM, wrap `VL_SetPixel` over `framebuf[y*320+x]=c`, first static frame.
4. **Hand controller input** — LoadLibrary HC.DLL + WM_HC_* handler for movement test (on-screen arrow).
5. **OPL3 audio smoke test** — direct port writes 0x388/0x389 to produce a note (independent of Modular Windows, proves port I/O allowance).

Recommended: start with (1) + (4) in parallel in S3 to have main loop + input working before touching Wolf3D code.

### Milestone A.2 (bonus) — Animation loop + perf baseline

*Approach A.2:* SetTimer 50ms (target 20fps) + scroll offset + recomputed FillGradient + InvalidateRect per frame. First test: **1 FPS**. Not close to target.

*Analysis:* two candidate bottlenecks — FillGradient (64000 pixels, `y*320+x` multiply per element) and palette realization on every WM_PAINT.

*Optimization A.2a:* FillGradient via `*p++` pointer arithmetic eliminating MUL + palette realized once only (`gPaletteRealized` flag). Result: **4-5 FPS but colors reduced to bands (16-color)**. Palette not applied to BeginPaint DC.

*Optimization A.2b:* re-select palette every WM_PAINT (SelectPalette is cheap) + RealizePalette once only. Colors back to 256 but **FPS dropped to 1**.

*Insight:* `StretchDIBits(DIB_RGB_COLORS)` with a selected palette runs per-pixel color-match even when the app palette == hardware palette. GDI doesn't recognize the equivalence.

*Optimization A.2c (decisive):* alternative BITMAPINFO with `WORD bmiColors[256] = 0..255` (direct indices) + `StretchDIBits(DIB_PAL_COLORS)`. GDI skips color mapping, byte-per-byte copy into hardware palette. Result: **5-6 FPS + full 256-color**. Win-win.

5-6 FPS baseline still far from Wolf3D target (10-15 fps desired, 8-10 fps minimum playable). Residual optimization paths: DisplayDibEx (VIS-native fast DIB), DDB caching, dirty rect (Wolf3D renderer writes only visible columns).

### S2 wrap-up

Big architectural pivot mid-session (DOS path dead) cost momentum but opened the real problem: how to do Wolf3D on Modular Windows. Milestones A.1 + A.2 in ~3.5h total is above-expectations pace. Win16-native Wolf3D path defined deliberately — no longer speculation about a "perfect DOS envelope", now a Win16 project with sharp boundaries and known perf budget (1→5-6 fps with DIB_PAL_COLORS, target ~10fps with dirty-rect + DisplayDibEx in S3).

---

## Session 3 — 2026-04-24 — DISPDIB deep-dive + Wolf3D palette integration (Milestone A.3)

**Scope:** perf optimization via DisplayDibEx + Wolf3D palette/VSWAP loaders. Initial user feedback: "be gentle with the raycaster" → revised work order: perf + assets + input + audio + menu, raycaster only at the end.

### Approaches

**Step 1 — DISPDIB recon.**

- BIOS bank 1 @ 0x5A22 = NE module DISPDIB.DLL embedded in ROM. Exports: DISPLAYDIB, DISPLAYDIBEX. Imports: GDI, USER, KERNEL. Description string: "TVVGA (GRYPHON) DIB Display DLL".
- SDK `programmers_ref.txt:2766-3015` documents the full API: `WORD DisplayDib(LPBITMAPINFO, LPSTR, WORD flags)` + flags BEGIN/END/NOWAIT/MODE_320x200x8/NOPALETTE/NOCENTER/etc.
- Documented animation pattern: BEGIN → per-frame NOWAIT → END.

**Step 2 — FAILED LoadLibrary paths.**

- `LoadLibrary("DISPDIB")` at runtime → Modular Windows dialog **"Please insert the main disc and press OK"**. MW scans A:\ for the DLL, doesn't find it (DISPDIB is in ROM but MW doesn't expose it via LoadLibrary), error fallback.
- **Discovery:** static binding via link script: `IMPORT DisplayDib DISPDIB.DISPLAYDIB`. NE modref table adds DISPDIB as module 0. MW loader resolves from ROM automatically, shell launch OK.

**Step 3 — Flag values search (fail).**

- Single-call blocking `DisplayDib(bmi, buf, MODE_320x200x8 | NOCENTER)` → gradient visible, ret=0. Rendering path works.
- BEGIN + per-frame NOWAIT pattern: ret=0 but screen stays black. DisplayDib doesn't keep display mode across calls.
- Tested NOWAIT values: 0x1000, 0x0400 (VfW 1.1 std), 0x0080 (swapped w/ TEST) — all fail.
- Per-frame blocking (no NOWAIT): gradient→black loop showing DisplayDib does blit but gives up the display after ~100ms.
- Conclusion: VIS DISPDIB flag layout differs from Microsoft VfW 1.1 standard. Would need ROM disassembly for exact flag values.

**Step 4 — Park DISPDIB.**

- Pragmatic decision: ~2h of flag-bit guessing → negative ROI vs working A.2 baseline at 5-6 FPS.
- Assets saved for the future: `reverse/dispdib_raw.bin` (42 KB extracted from bank 1 0x5A00..0x10000) contains the full NE module for disassembly in a later session.
- Memory file `project_dispdib_parked.md` + `reference_modwin_runtime_gotchas.md` document the findings.

**Step 5 — Pivot to Wolf3D palette integration (Milestone A.3).**

- `wolf3d/WOLFSRC/OBJ/GAMEPAL.OBJ` (893 B OMF) = pre-compiled palette. LEDATA record offset 0x77, 768 bytes VGA 6-bit (R,G,B triplets 0..63).
- Python parser extracts → `wolf3d/gamepal.bin` + C header `src/gamepal.h` with `static const unsigned char gamepal6[768]`.
- `src/wolfvis_a3.c` (167 LOC): A.2 renderer baseline + InitPalette loads Wolf3D values + DrawPaletteGrid (16×16 tile grid, each tile a palette entry, 1px black gridlines).
- Build: `build/WOLFA3.EXE` 67 KB NE. ISO 122 KB.
- MAME test: `snap_a3/a3_0000.png` shows all 256 Wolf3D colors visible. EGA 16 in row 0, wall/enemy/sky colors in the other rows. Palette realization works with the real Wolf3D set.

### Concrete results

- `src/wolfvis_dd.c`, `src/link_wolfvis_dd.lnk`, `src/build_wolfvis_dd.bat` — DispDib experiment (parked)
- `build/WOLFVDD.EXE` — DispDib static-bind variants (multiple tests, all non-working)
- `cd_root_dd/` + `build/snap_dd/` — CD staging + screenshot debug log
- `reverse/dispdib_raw.bin` — 42 KB ROM extract for future disassembly
- `wolf3d/gamepal.bin` — 768 byte raw Wolf3D palette
- `src/gamepal.h` — C header with gamepal6[768]
- `src/wolfvis_a3.c`, `src/link_wolfvis_a3.lnk`, `src/build_wolfvis_a3.bat`, `src/mkiso_a3.py` — Milestone A.3
- `build/WOLFA3.EXE` — Win16 NE with Wolf3D palette
- `build/wolfvis_a3.iso` — VIS-bootable A.3 test ISO
- `build/snap_a3/a3_0000.png` — Milestone A.3 proof
- `cd_root_a3/` — A.3 CD staging
- 4 new memory files: `project_dispdib_parked`, `reference_modwin_runtime_gotchas`, `project_milestone_A3_palette`, `feedback_raycaster_gentle`

### Trap / Gotcha / Eureka

- **Gotcha S3.1:** `LoadLibrary("ANY")` in Modular Windows VIS triggers the "Please insert the main disc" dialog. MW scans the CD for the DLL even if the name lives in ROM. **Always use static import via NE modref** for ROM/stock DLLs.
- **Gotcha S3.2:** `cmd /c ".bat" 2>&1 | tail -N` chain inside bash often skips execution. Stale build files, false test results. **Use PowerShell `cmd /c "full\path.bat"`** or direct invocation for robust builds.
- **Gotcha S3.3:** MAME `-snapname` without `-snapshot_directory` routes to the MAME default dir, not the project. Always specify `-snapshot_directory`.
- **Gotcha S3.4:** DisplayDib ret=0 with MODE_320x200x8 + BEGIN + NOWAIT does *not* guarantee the display is active. The VIS flag layout differs from VfW 1.1 standard.
- **Gotcha S3.5:** Wolf3D palette is VGA 6-bit (0..63). Conversion to 8-bit RGBQUAD requires shift left 2 (not *4.25 or clamping). Skip this and colors are 4× darker.
- **Eureka S3.E1:** GAMEPAL.OBJ has the palette at file offset 0x77 (after OMF header + PUBDEF). LEDATA record length = 772 = 1 seg + 2 offset + 768 data + 1 checksum.
- **Eureka S3.E2:** Retail VIS Atlas GPRS.EXE imports MMSYSTEM + HC + standard — NOT DISPDIB. DisplayDib likely unused in retail production.
- **Eureka S3.E3:** The user feedback pattern "be gentle with the raycaster" implies an ordering: assets/audio/input/menu → raycaster last.

### Next-step candidates for Session 4

1. **VSWAP.WL1 loader.** User provides Wolf3D shareware install. Format: 64 KB lump header + texture/sprite chunks. Palette-indexed. Draw a single 64×64 wall texture at (0,0) in the framebuf using the Wolf3D gamepal.
2. **Dirty-rect perf PoC.** Animated bar sweeping over A.3 grid, invalidate only the necessary rect, measure FPS gain vs full-screen blit.
3. **Hand controller input.** Static LoadLibrary HC.DLL (if static-bind works like for DISPDIB). WM_HC_* handler mapped to mouse/keyboard in MAME.
4. **DISPDIB disassembly.** Open Watcom `wdis dispdib_raw.bin` + analyze flag bit masks. On success, return to DispDib path for 10+ FPS target.
5. **OPL3 smoke test.** Direct port I/O 0x388/0x389 to play a note — verifies port access allowance in MW.

S4 recommendation: (1) if user provides the WL1 assets, it's the most linear path toward "Wolf3D shows up" visually. Otherwise (3) hand controller for interactivity, or (4) DISPDIB disassembly to close the open bug.

### Milestone A.4 (bonus, same S3) — VSWAP asset loader

**Trigger:** user notifies that shareware WL1 has been placed in `assets/` (VSWAP.WL1 742 KB + AUDIOT/GAMEMAPS/VGAGRAPH/headers). All 7 shareware WL1 files available.

**VSWAP.WL1 parser:** 6 B header (chunks_in_file=663, pm_sprite_start=106, pm_sound_start=542) → 106 walls 0..105 + 436 sprites 106..541 + 121 sounds 542..662. Each wall = 4096 B chunk col-major 64x64.

**Approach:** `wolfvis_a4.c` = A.3 baseline + LoadVSwap (OpenFile "A:\\VSWAP.WL1" + _lread header + _llseek + _lread offset table + loop _llseek+_lread for 5 walls) + DrawWallStrip. VSWAP.WL1 placed on the CD as an asset file.

**Bisect debug:** app crashed with "An error has occurred / Please turn system off" MW dialog (severe crash dialog). Bisect phase-by-phase: open+close OK → header read OK → offset table OK → wall 0 read OK → 5 wall reads OK → DrawWallStrip crashed.

**Root cause:** Watcom 16-bit `int` overflow. `framebuf[sy * SCR_W + sx]` with sy=131, SCR_W=320: 131*320=41920 > 32767 signed int16 max → negative offset → out-of-array memory access → crash.

**Fix:** `framebuf[(unsigned)sy * (unsigned)SCR_W + (unsigned)sx]` or pointer-increment pattern with `rowptr = &framebuf[(unsigned)68*SCR_W + sx]; rowptr += SCR_W`. Same gotcha as S2 (define SIZE (320*200) → E1020) in runtime form.

**Result:** `build\snap_a4\fix_0000.png` shows 5 original Wolfenstein 3D wall textures: 4 gray stone brick variants + Nazi banner with eagle emblem (wall #4). Runtime asset pipeline from CD confirmed.

### Updated S3 results

- New: `src\wolfvis_a4.c` (241 LOC), `src\build_wolfvis_a4.bat`, `src\link_wolfvis_a4.lnk`, `src\mkiso_a4.py`
- New: `cd_root_a4\` (with VSWAP.WL1), `build\wolfvis_a4.iso` (848 KB), `build\WOLFA4.EXE` (68 KB), `build\snap_a4\fix_0000.png`
- New: `wolf3d\gamepal.bin` (768 B), `wolf3d\wall0.raw` (4096 B)
- `assets\` (user-provided): VSWAP.WL1 + AUDIOT.WL1 + GAMEMAPS.WL1 + MAPHEAD.WL1 + VGADICT.WL1 + VGAGRAPH.WL1 + VGAHEAD.WL1 + AUDIOHED.WL1 + WOLF3D.EXE
- Memory: `project_milestone_A4_vswap`, updated `reference_win16_rendering_gotchas` with runtime int-overflow

### Trap / Gotcha / Eureka (updated)

- **Gotcha S3.A4:** Watcom 16-bit int runtime overflow `y * 320` with y>=103. Only `(unsigned)y * (unsigned)320 + (unsigned)x` cast prevents it. Compile-time gives E1020, runtime crashes MW silently.
- **Eureka S3.A4.E1:** Win16 `OpenFile`/`_lread`/`_llseek`/`_lclose` fully functional in VIS MW against the CD. `OF_READ`, canonical path `A:\\FILE.EXT`.
- **Eureka S3.A4.E2:** `_lread(f, buf, 4096)` OK in one shot; UINT limit probably 65535-1. Multiple seek+read in loop OK. 8 KB stack sufficient.
- **Eureka S3.A4.E3:** VSWAP.WL1 shareware is 742 KB. Fits CD 150 KB/s without issues. MAME -cdrom with an ISO containing VSWAP loads in ~2s emulated.

### Revised next-steps for Session 4

1. **VSWAP sprite loader.** 436 sprite chunks, different format (transparent column layout with `post` entries). Draw a static Nazi guard at the screen center.
2. **GAMEMAPS loader.** Shareware level 1 map. Format: 2D 64x64 array of tile IDs + wall types. Draw top-down minimap for debug.
3. **Raycaster integration.** **Last.** After sprite + map + input. Reuse gamepal + walls + DrawWallStrip primitives already built.
4. **Hand controller input.** Static-bind HC.DLL (like DISPDIB pattern). WM_HC_* handler to move the wall strip horizontally (mapping proof).
5. **AUDIOT.WL1 parse + OPL3.** Music chunks in IMF format. Port I/O 0x388/0x389.

### Final S3 wrap-up

DispDib dead but paid in knowledge. Milestones A.3 + A.4 tail-end of the session — palette + asset loader in 2h after user unblocked the assets. Wolf3D textures visible in MAME VIS is a historic moment in the project: first time ever Wolfenstein 3D graphics appear on Tandy/Memorex VIS hardware (a 1992 platform that was never an id Software target). Foundation now has all primitives: palette, asset I/O, chunky blit. Raycaster remains future but in reach.

S3 pacing: ~4h total. Part 1 (DispDib rabbit hole) slow but documented. Part 2 (palette+VSWAP) fast and visually rewarding.

---

## Session 4 — 2026-04-25 — Sprite loader → input → maps → audio (4 milestones)

**Scope:** opened with the sprite loader proposed by user. Technical momentum led to 4 consecutive milestones: A.5 (sprites) → A.6 (input) → A.7 (GAMEMAPS) → A.8 (OPL3). Wolf3D port tech stack substantially complete before the raycaster.

### Milestone A.5 — VSWAP sprite loader

**Approach:** A.4 baseline extended with full `pageoffs[chunks_in_file]` + `pagelens[chunks_in_file]` instead of just the first 5 offsets. 3 sprite chunks loaded (sprite_start+0..2). DrawSprite 1:1 top-left.

**Decoded VSWAP sprite format** (from ID_VL_A.ASM + OLDSCALE.C):
- Chunk: `WORD leftpix, rightpix` + `WORD dataofs[rp-lp+1]` + posts + pixel data
- Post = 3 WORDs: `(endy<<1, corrected_top, starty<<1)`, terminator 0
- Pixel byte at row y = `sprite[corrected_top + y]` (corr_top pre-subtracts starty)

**First render FALSIFIED** (shows "Demo"/"DEATHCAM" letters slanted/distorted) → user notes upside-down: A.4 walls were also flipped but symmetry masked it. **Y-flip bug pre-existing from A.4** — bottom-up DIB biHeight>0 = framebuf[0..319] is the LAST row on screen. Canonical fix `fb_y = (SCR_H-1) - screen_y` in every FB write.

**Result:** `snap_a5/flip_0000.png` — gothic red SPR_DEMO, italic yellow SPR_DEATHCAM, blue line SPR_STAT_0. Walls now correctly oriented (Nazi eagle banner upright).

### Milestone A.6 — Hand controller input

**Step 0 — Initial test:** WM_KEYDOWN with standard VK_UP/DOWN/LEFT/RIGHT. No input arrives. Nibble debug bar doesn't change.

**Step 1 — SetFocus + HC polling path:** added `SetFocus(hWnd) + SetActiveWindow(hWnd)` after ShowWindow (discovery: WS_POPUP MW doesn't get focus automatically). Plus static-bind `IMPORT hcGetCursorPos HC.HCGETCURSORPOS` as a fallback. Inputs finally arrive but the cursor doesn't move — switch cases for VK_UP/etc don't match.

**Step 2 — Bit-grid debug bar:** replaced nibble color encoding (Wolf3D palette doesn't have 16 distinct colors) with 16 on/off cells (blue=1, white=0, bit 0 leftmost). User describes the patterns for each press.

**Decode user descriptions → VK codes:**
- DOWN = 0x70, UP = 0x78, LEFT = 0x77, RIGHT = 0x79
- A (Xbox) = 0x72 (PRIMARY), B = 0x75 (SECONDARY), X = 0x71, Y = 0x73

Range 0x70..0x79 = slots reused from standard Windows VK_F1..VK_F10. These are empirical **VK_HC1_*** never enumerated in the SDK.

**Result:** cursor moved by d-pad/arrows, buttons change color. Input working.

### Milestone A.7 — GAMEMAPS loader + minimap

**MAPHEAD.WL1 format** (402 B): WORD RLEWtag (=0xABCD) + DWORD headeroffsets[100]. Shareware populates only [0..9] (E1L1..E1L10).

**GAMEMAPS.WL1 format**: magic "TED5v1.0" + at headeroffsets[mapnum] a maptype struct (38 B: planestart[3] + planelength[3] + w/h + name[16]).

**Per plane p (2 used + 1 unused):**
1. Read planelength[p] bytes from planestart[p]
2. First WORD = Carmack expanded size (bytes)
3. Carmack decompress → buffer
4. First WORD of Carmack output = RLEW expanded size (= 2*64*64 = 8192)
5. RLEW decompress skip first WORD → w*h WORDs of tile IDs

**C implementation:**
- `CarmackExpand`: NEARTAG 0xA7 / FARTAG 0xA8 + count byte + offset byte. Count=0 → escape (copy next byte as low-half of tag word).
- `RLEWExpand`: tag word + count + value run.

**Map 0 "Wolf1 Map1" header:** planestart={0xb,0x5a5,0x8c0}, planelength={1434,795,10}, w/h=64. Plane0 Carmack expanded=3190B.

**Empirical tile value mapping:**
- 0 = exterior, 1..63 = walls, 64..107 = floor codes, 90..101 = doors
- plane1: 19..22 = player start N/E/S/W, 23..74 = static obj, 108..115 = guards, 116..127 = bosses

**Result:** `snap_a7/map_0000.png` shows recognizable E1L1 minimap: green corridors, blue/cyan walls, olive border (door), red guards, scattered yellow objects.

### Milestone A.8 — OPL3 smoke test

**Watcom intrinsics:** `outp(port, val)` and `inp(port)` emit inline OUT/IN.

**Init sequence:**
1. Reset key-off regs 0xB0..0xB8
2. ch0 operators: mult=1, modulator silent (att=0x3F), carrier loud (att=0x00)
3. Attack=15, decay=0, sustain=0 (loudest), release=5
4. Sine waveform, feedback=0, algorithm=0 (2-op FM)
5. Fnum = 0x244 (A4), block=4

**FIRST BUILD = "bling + fadeout"** instead of continuous tone. Root cause: reg 0x20/0x23 without bit 5 (`EG type`) = 0x01. With EG=0 the envelope is "percussive" → decays past sustain. Fix: reg 0x20/0x23 = 0x21 (EG=1 sustained).

**SECOND BUILD = sustained note BUT input no longer works**. Initial diagnosis wrong: hypothesis "OPL emulation starves input". A/B test with the A.7 ISO → **A.7 also no longer receives input**. Nuke MAME cfg → still nothing.

**Final root cause:** A.7 link script DID NOT have `IMPORT hcGetCursorPos HC.HCGETCURSORPOS`. I had thought it was optional (only for hcGetCursorPos polling). **FALSE**: the presence of the NE module-ref to HC.DLL is what Modular Windows uses to decide whether to route HC events to the focused window. Without the import, **WM_KEYDOWN silently dropped**.

Canonical fix: every Win16 VIS app that wants HC input MUST have:
1. Link script: `IMPORT hcGetCursorPos HC.HCGETCURSORPOS`
2. C code: `extern void FAR PASCAL hcGetCursorPos(LPPOINT);` + at least 1 call (anti dead-code elimination)

**A.8 final result:** sustained A4 note, arrows move the cursor on the minimap, X (VK_HC1_F1) init+play, A/B pitch up/note off.

### Concrete S4 results

- `src/wolfvis_a5.c` (290 LOC) + build/link/iso: VSWAP sprite loader
- `src/wolfvis_a6.c` (~370 LOC): HC input + bit-grid diagnostic
- `src/wolfvis_a7.c` (~440 LOC): MAPHEAD/GAMEMAPS + Carmack + RLEW + minimap
- `src/wolfvis_a8.c` (~520 LOC): OPL3 direct port I/O + audio mgmt
- 4 ISOs: `wolfvis_a{5,6,7,8}.iso` all VIS-bootable
- 4 proof screenshots: `snap_a{5..8}/` with visible output (sprites, cursor, minimap, debug bar with audio indicator)
- Memory: `project_milestone_A5_sprites`, `A6_hc_input`, `A7_gamemaps`, `A8_opl3`, `reference_vk_hc1_codes`
- Updated `reference_win16_rendering_gotchas` with Y-flip gotcha

### Trap / Gotcha / Eureka (S4)

- **Gotcha S4.1 — Latent Y-flip from A.4:** top-down framebuf + BITMAPINFO.biHeight>0 (mandatory bottom-up DIB) produces a flipped image. Symmetric A.4 walls masked it; "Demo"/"DeathCam" text sprites in A.5 reveal it. Canonical fix `fb_y = (SCR_H-1) - screen_y` in every FB write.
- **Gotcha S4.2 — SetFocus mandatory for WS_POPUP MW:** without SetFocus(hWnd)+SetActiveWindow(hWnd) after ShowWindow, WS_POPUP doesn't receive WM_KEYDOWN. Behavior differs from Win95+ default in MW.
- **Gotcha S4.3 — HC.DLL IMPORT MANDATORY (not just for polling):** thought it was optional → brutally falsified in A.8. Without `IMPORT hcGetCursorPos HC.HCGETCURSORPOS` in the link script, MW silently drops WM_KEYDOWN. The A/B diagnosis with the A.7 ISO (also broken) definitively excluded the OPL starvation theory.
- **Gotcha S4.4 — OPL3 EG bit 5 mandatory for sustained:** reg 0x20/0x23 = 0x21 (not 0x01). Without EG=1 the envelope is percussive, note "blings + fades out" instead of sustaining.
- **Gotcha S4.5 — Wolf3D palette doesn't have 16 distinct colors for nibble encoding:** indices i*16+15 produce 5-6 similar shades (mostly dark blue). Fix: bit-grid debug (16 on/off cells) instead of nibble hex encoding.
- **Gotcha S4.6 — FillRect name collision with win16.h:** renamed to FB_FillRect.
- **Eureka S4.E1 — VK_HC1_* reverse-engineered:** range 0x70..0x79 reuses VK_F1..VK_F10 slots. SDK names the constants but doesn't provide numeric values. Confirmed PC arrows + Xbox d-pad map to the same HC signal in the MAME vis driver.
- **Eureka S4.E2 — Sprite post format WAS correctly decoded on first attempt:** the "skewed render" wasn't a decoding bug, it was Y-flip. My assumptions on `corrected_top` + `endy*2/starty*2` were right from the first iteration.
- **Eureka S4.E3 — Carmack + RLEW + maptype decoding worked on first try:** complete decompression path (1764 bytes Carmack-compressed → 3190 B intermediate → 4096 WORDs map grid) without iteration. ID_CA.C format clear enough for direct port.
- **Eureka S4.E4 — OPL3 port I/O works from Win16 MW:** no protection fault, no device driver conflict. Standard Mode Win16 allows direct port I/O freely. Audible note after EG fix.
- **Eureka S4.E5 — MAME vis input maps Xbox pad + PC arrows equivalently:** confirmed HC d-pad parity between both input devices, same VK_HC1_* output.

### Next-step candidates for Session 5

**PRIORITY 1 (S5 opening, mandatory, ~30 min):**
0. **A.9 perf refactor RedrawScene** — mitigation of microfreeze observed in S4 A.6/A.8 on input. Estimated ~150-200ms current per keypress (ClearFrame 64KB + DrawMinimap 16K FB_Put + full StretchDIBits). Triple fix:
   - Static minimap (DrawMinimap only after LoadMap, not every redraw)
   - Localized cursor erase+redraw (20×20 region instead of full frame)
   - Dirty-rect `InvalidateRect(hWnd, &dirty, FALSE)` + StretchDIBits with partial dest
   Perf foundation before raycaster or animations. Don't skip.

**After the refactor:**
1. **AUDIOT.WL1 parser + IMF playback** — IMF format (Wolf3D music): stream of (reg, val, delay_word). Extract tracks 0-4 shareware, play real music instead of the test A4 note.
2. **Unified integrated scene** — walls + sprites + minimap + player cursor + OPL click-sound all together. Demonstrative consolidation pre-raycaster.
3. **Scaler port (SimpleScaleShape)** — port WL_SCALE/OLDSCALE simple-path to scale sprites in DrawSprite. Final step before the raycaster.
4. **Raycaster integration** — LAST stop (feedback_raycaster_gentle). Map grid already in memory (plane0), palette + textures + framebuf + input + audio all present. WL_DRAW to be ported as foundation.
5. **hcControl HC_SET_KEYMAP** — remap VK_HC1_* to standard VK for convenience (e.g. VK_HC1_UP → VK_UP). Cosmetic, not critical.

S5 recommendation: **(0) mandatory perf refactor**, then (1) IMF music for the "Wolfenstein 3D theme on VIS" emotional milestone, then (2) integrated scene and assess whether we're ready for the raycaster.

### S4 wrap-up

4 milestones in ~3h real time. Wolf3D port stack complete before the raycaster: palette (A.3) + walls (A.4) + sprites (A.5) + input (A.6) + maps (A.7) + audio (A.8). Critical HC.DLL gotcha discovered by serendipity (A.7 regression invisible until A.8 forced an interactive test). Without this session we'd have given the raycaster a buggy foundation → guaranteed debug hell. Excellent pacing: momentum from the sprite loader led to closing 3 additional unplanned milestones. User approved every step without deferring, feedback_pacing_calibration confirmed.

S4 is the **most productive VIS project session so far** — more results than S1 (feasibility) and S3 (DispDib+palette+VSWAP) put together. Wolf3D PoC within reach: 1-2 sessions for scaler + raycaster = playable game.

---

## Session 5 — 2026-04-25 — Public repo publication + Milestone A.9 (perf refactor)

**Scope:** S5 opened with a declared S5 priority-1 (A.9 perf refactor — eliminate WM_KEYDOWN microfreeze before any further milestone). User added a parallel objective: bring the project under public version control on GitHub for the first time. So the session split into two parts: (1) repository publication, (2) A.9 perf refactor with a partial-blit detour.

### Part 1 — Repository publication

**Established workflow rules (saved as memory):**
- Repo-committed files (this log, `README.md`, code comments, commit messages) in English. Conversation between user and assistant stays in Italian.
- `VIS_sessions.md` must be updated **before each incremental commit** — the public repo cannot be allowed to drift between code and log.
- MIT License chosen as default for public repos: user prioritizes "spark / temporal authorship" over downstream control.

**Pre-publication scrub:**
- Searched committed files for cross-pipeline / personal markers — sanitized 3 prose references in the sessions log to keep VIS as a self-contained project narrative.
- Translated `VIS_sessions.md` (S1..S4) entirely from Italian to English.

**Toolchain setup:**
- `winget install --id GitHub.cli` (~14 MB, UAC required). Installed under `C:\Program Files\GitHub CLI\gh.exe`. Not added to PATH for already-open shells; called via full path for the rest of the session.
- `gh auth login --web --git-protocol https` produced a device code (`https://github.com/login/device`); user pasted+authorized; auth completed as `vs-sr-dev` with scopes `repo + gist + read:org`.
- `git config --global user.email` already matched the GitHub account email, so commits are author-verified out of the box without per-repo override.

**`.gitignore` design (copyright-aware):**
```
tools/                         # Open Watcom V2 (~537 MB, fetchable)
isos/                          # retail VIS BIN/CUE — copyright Tandy/Memorex
vis.zip                        # MAME BIOS — copyright Tandy/Memorex
reverse/*.bin                  # BIOS dumps + DISPDIB extract
reverse/extracted/
reverse/CONTROL.TAT*           # retail TAT clones — copyright Tandy
reverse/*.tat
reverse/*.iso
reverse/*.exe
docs/                          # Modular Windows SDK PDFs — copyright Microsoft
assets/                        # Wolf3D shareware data — copyright Apogee/id
wolf3d/                        # Wolf3D source clone — separately GPL
build/
cd_root*/
cfg/
cfg_backup_*/
nvram/
*.obj *.exe *.com              # Watcom intermediate
*.pyc __pycache__/
.DS_Store Thumbs.db
```

**Path genericization across 22 scripts:**
- 9 `mkiso_*.py` (mkiso, _a3..a8, _dd, _step0): hardcoded `r"d:\Homebrew4\VIS\..."` replaced with `pathlib.Path(__file__).resolve().parent.parent / "<reldir>"`.
- 2 `reverse/*.py` (extract_tat, sniff_tat) plus inspect_disc, find_tat_code, make_control_tat: hardcoded paths replaced with `__file__`-derived (`.parent` for in-dir, `.parent.parent` for sibling-dir lookups).
- 11 `build_*.bat`: `set WATCOM=d:\Homebrew4\VIS\tools\OW` replaced with `if not defined WATCOM set WATCOM=%~dp0..\tools\OW`. Respects existing `WATCOM` env var if set globally; otherwise resolves relative to repo.
- A separate sweep caught absolute paths in 3 prose lines of `VIS_sessions.md` (canonical MAME command, GAMEPAL.OBJ recon path, assets dir reference) — also genericized.

**`README.md` written:**
- Project intro (VIS hardware + Wolf3D port goal).
- Status table: 8 milestones complete (S1 + A.1..A.8), A.9 next, raycaster pending.
- Layout description with copyright callout for each git-ignored directory.
- Quick-start: dependencies (Open Watcom V2, Python+pycdlib, MAME 0.287+, retail VIS disc for CONTROL.TAT, Wolf3D shareware), build invocation, MAME launch command.
- Pointer to `VIS_sessions.md` for the full work log.

**Commits:**
- `7a7f07d` — Initial commit: 53 files, 4469 insertions. `gh repo create vs-sr-dev/vis-homebrew --public --source=. --remote=origin --push --description "..."`. Repo lives at https://github.com/vs-sr-dev/vis-homebrew.
- `90555f6` — Add MIT LICENSE file. The README mentioned MIT but the file itself was missing — this left the project nominally "all rights reserved" until GitHub's licensee gem could classify the LICENSE text. After this commit the sidebar correctly shows "MIT License" with the scales icon.

### Part 2 — Milestone A.9 (perf refactor)

**Goal:** eliminate the ~150-200 ms microfreeze A.8 had on every WM_KEYDOWN. The bottleneck was that every input event re-rendered the full 320x200 framebuf (ClearFrame 64KB + DrawMinimap ~16K FB writes + DrawDebugBar + DrawCursor) and then InvalidateRect'd the whole window, forcing a full-screen StretchDIBits.

**Architecture:**
1. Static layer (cleared bg + minimap + minimap border) rendered once after `LoadMap()` and snapshotted into `static_bg[64000]` — a second 64 KB buffer in its own large-model data segment.
2. Debug bar (top 30 rows, `DEBUG_BAR_H = 30`) repainted only on WM_TIMER ticks (500 ms cadence) and invalidates just `(0, 0, SCR_W, 30)`. The cursor never enters y < 35, so debug-bar refresh never disturbs cursor pixels.
3. Cursor erase/redraw: each WM_KEYDOWN copies `static_bg → framebuf` for the previous-cursor 11x11 bbox (`RestoreFromBg(prev_x - 5, prev_y - 5, 11, 11)`), draws the cursor at the new position, and `InvalidateRect`'s only `bbox(prev) U bbox(new)`.

**Per-keypress cost drop:** ~80 KB framebuf writes + full StretchDIBits → ~150 byte ops + GDI-clipped partial repaint.

**StretchDIBits partial-source detour (A.9 first build, falsified):**

First attempt used a partial source rect: `StretchDIBits(hdc, px, py, pw, ph, px, py, pw, ph, ...)` with `(px, py, pw, ph) = ps.rcPaint`. Visually, every keypress left the cursor's 11x11 bounding box painted at the previous position — a clear "trail" effect.

Diagnosis: bottom-up DIBs (mandatory on VIS, `biHeight > 0`) interpret `YSrc` in DIB-coord space (origin at lower-left, scanline 0 = visual bottom of image). When the source rect is the *entire DIB*, `(YSrc=0, SrcHeight=H)` happens to coincide with the upper-left convention an API user might assume — so A.8's full-screen blit worked correctly. With a *partial* source rect the convention diverges: passing `YSrc = py` (top-down coords from window) reads from DIB scanlines that store visual content from the *opposite half* of the image. The framebuf bytes for the new cursor position were written correctly by `RestoreFromBg + DrawCursor`, but `WM_PAINT` was reading the wrong storage scanlines and displaying stale or unrelated pixels at the dirty rect — so the old cursor was never visually replaced.

Fix: revert to full-source `StretchDIBits(hdc, 0, 0, SCR_W, SCR_H, 0, 0, SCR_W, SCR_H, ...)`. GDI clips physical screen writes to the invalid region (set by partial `InvalidateRect` from KEYDOWN/TIMER), so only the dirty pixels are actually drawn. The full-source read is ~64 KB but cheap with `DIB_PAL_COLORS` (no per-pixel color match — straight passthrough to hardware palette indices). Cursor responsiveness measurably the same as the partial-src first build, and trail eliminated.

**Result:** smoke-tested in MAME 0.287 vis. User report: "molto più fluido, anni luce rispetto a prima"; after the StretchDIBits fix: "assolutamente perfetto ora, nessuna scia residua e audio confermato tutto ok". OPL3 audio and heartbeat indicator unchanged.

### Part 3 — Milestone A.10 (IMF music playback)

**Goal:** Wolfenstein 3D AdLib music audible on Tandy/Memorex VIS hardware. AUDIOT.WL1 + AUDIOHED.WL1 loader + IMF event scheduler driving OPL3 register writes.

**Recon:**
- AUDIOWL1.H declares NUMSNDCHUNKS = 234 (= 3 * 69 SFX + 27 music) with `STARTMUSIC = 207`. The shareware AUDIOHED.WL1 we have is 1156 B = 289 DWORDs (288 chunks) — the SDK constants don't match this re-pack.
- Actual music chunks empirically live at indices 260..287, with each track represented by a small 88-byte placeholder + the real data block. Chunk 261 (7546 B) = first big music chunk = CORNER_MUS ("Enemy Around the Corner"), confirmed by user listening. Chunks 263, 264, 268, 270, 272, 273, 275, 277, 284, 285 are also non-trivial music data.
- MusicGroup format: `WORD length` + `WORD values[length/2]` IMF stream + ~88 B trailing MUSE metadata (ignored by player). Each IMF event = 2 WORDs: `(reg+val packed)` low=reg high=val, then `delay` in 700 Hz ticks.
- Tick rate confirmed 700 Hz from `SDL_SetTimerSpeed` in ID_SD.C: `rate = TickBase * 10 = 70 * 10 = 700`. Cross-checked: at 700 Hz, total tick sum 42893 → track length 61 sec, matches YouTube reference for CORNER_MUS.

**Implementation (`wolfvis_a10.c`):**
- `audio_offsets[289]` and `music_buf[24000]` declared `__far` (forces them out of DGROUP — without it the linker errors with "default data segment exceeds maximum size by 7891 bytes").
- `LoadAudioHeader()` reads AUDIOHED.WL1 in one shot (1156 B). `LoadMusicChunk(idx)` seeks AUDIOT.WL1 to `audio_offsets[idx]`, reads the chunk into `music_buf`.
- `StartMusic()` parses the WORD length prefix, sets up `sqHack`/`sqHackPtr`/`sqHackLen`/`sqHackSeqLen`, resets `alTimeCount = 0` and OPL3 registers.
- `ServiceMusic()` is the port of SDL_ALService: GetTickCount delta → ticks_advance via `elapsed_ms * 700 / 1000` → drains all events whose `sqHackTime <= alTimeCount` via `OplOut(reg, val)` + `sqHackTime += delay`.

**Three iterative bugs (now memorized as `reference_imf_scheduler_gotchas.md`):**

1. **First build — slow tempo (~50%).** I had `sqHackTime = alTimeCount + delay` in the inner loop, copied verbatim from the original SDL_ALService. The original increments `alTimeCount` by 1 per ISR call (at 700 Hz), so `alTimeCount` is always "the current tick exactly". With *batched* advance (`alTimeCount += 38` per WM_TIMER call), `alTimeCount` jumps, and `alTimeCount + delay` pushes every queued event to the *end* of the current batch instead of to its true virtual due time. Each in-flight event accumulates a +38-tick drift. **Fix**: `sqHackTime += delay` — accumulate cumulative virtual time independent of when alTimeCount catches up.

2. **Second build — per-beat drag.** Tempo correct on average, but the music sounded jerky, "struggling at every new beat". Cause: I was driving `ServiceMusic()` from `WM_TIMER`, which on Win16 has ~55 ms minimum granularity. IMF events arrive at ~1.43 ms cadence (700 Hz), so a single WM_TIMER tick processed all events within a ~38-tick burst, then went silent for the rest of the 55 ms. Audible as a "lurch" every beat. **Fix**: moved the scheduler to a `PeekMessage` idle loop in WinMain — `ServiceMusic()` is now called thousands of times per second between message dispatches, dispatch granularity drops to ~1 ms, residual frame-skip is ~1-2% (within PoC tolerance).

3. **DGROUP overflow (link-time).** Adding `music_buf[24000]` + `audio_offsets[1156 B]` on top of the existing carmack/RLEW buffers + map planes overflowed Watcom's default data segment by 7891 B. Watcom auto-segments arrays >= ~32 KB into their own segment (so `framebuf[64000]` and `static_bg[64000]` were already isolated), but smaller arrays go into DGROUP. **Fix**: `static DWORD __far audio_offsets[...]` + `static BYTE __far music_buf[...]` forces explicit far-segment placement.

**Final architecture:**
- `WM_TIMER` reverted to 500 ms — used only for heartbeat / debug-bar refresh, no longer for music.
- WinMain message loop:
  ```c
  for (;;) {
      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
          if (msg.message == WM_QUIT) return msg.wParam;
          TranslateMessage(&msg); DispatchMessage(&msg);
      } else if (sqActive) ServiceMusic();
      else WaitMessage();
  }
  ```
- `WaitMessage()` when music is off avoids a 100 % busy-loop. When music is on, the loop spins as fast as the CPU permits.

**Controls:** Y (`VK_HC1_F3`) starts music, X (`VK_HC1_F4`) stops it. `gAudioOn` cyan indicator in the debug bar reflects play state. The pre-existing single-A4-note path on `VK_HC1_F1` is preserved.

**Result:** user report after fix #3 — "ora è sostanzialmente quasi a 1:1, si perde ogni tanto magari un frame (si nota uno 'stacco') ma è fixabile in seguito. Come PoC è perfetto." The Wolf3D theme "Enemy Around the Corner" plays recognizably on emulated VIS hardware — first time Wolfenstein 3D music has been heard on a Tandy/Memorex platform. Cursor responsiveness during playback unaffected (PeekMessage drains the queue ahead of the music idle).

### Concrete results (S5)

- New: `src/wolfvis_a9.c` (~575 LOC), `src/link_wolfvis_a9.lnk`, `src/build_wolfvis_a9.bat`, `src/mkiso_a9.py`.
- New: `cd_root_a9/` staging (uses A.8 file set + WOLFA9.EXE + patched SYSTEM.INI shell line). `build/wolfvis_a9.iso` (216 KB), `build/WOLFA9.EXE` (135 KB; +64 KB vs A.8 because of `static_bg`).
- New: `src/wolfvis_a10.c` (~720 LOC), `src/link_wolfvis_a10.lnk`, `src/build_wolfvis_a10.bat`, `src/mkiso_a10.py`.
- New: `cd_root_a10/` (A.9 file set + AUDIOHED.WL1 + AUDIOT.WL1 + WOLFA10.EXE). `build/wolfvis_a10.iso` (374 KB), `build/WOLFA10.EXE` (161 KB).
- New: `LICENSE` (MIT, 2026 Samuele Voltan). `README.md` (with milestone status table kept in sync per commit). `.gitignore`.
- Translated and scrubbed `VIS_sessions.md` (this file).
- Public GitHub repo `vs-sr-dev/vis-homebrew` with 5 commits at session close: `7a7f07d` (initial), `90555f6` (LICENSE), `4a0199c` (A.9 code), `4c6fbac` (S5 docs catch-up for A.9 — workflow reformed afterward), `b73acc8` (README status sync for A.9 → A.10), then a single combined commit for A.10 code + sessions log + README.
- Memory: `feedback_repo_files_english`, `feedback_session_log_granular` (rule update for per-commit log + README update), `user_licensing_philosophy`, `reference_mame_path`, `reference_stretchdibits_partial_src_gotcha`, `project_milestone_A9_perf`, `project_milestone_A10_imf`, `reference_imf_scheduler_gotchas`. Updated `MEMORY.md` index.

### Trap / Gotcha / Eureka (S5)

- **Gotcha S5.1 — `.gitignore` first pass missed copyright artifacts:** initial pattern `reverse/control_tat_*.bin` (lowercase, underscore) failed to match the actual filenames `CONTROL.TAT.Atlas`, `CONTROL.TAT.Bible`, `CONTROL.TAT.Fitness` (uppercase, dot-separated). Plus `reverse/atlas.iso` and `reverse/atlas_gprs.exe` (retail extracts) weren't covered. Discovered during dry-run staging review **before** the first commit — would have leaked retail Tandy material to a public repo otherwise. Lesson: always `git add . && git status` and read the staged list line by line before the first commit on any new repo, especially with copyright-mixed working trees.
- **Gotcha S5.2 — Hardcoded absolute paths in many scripts:** 11 mkiso_*.py + 2 reverse/*.py + 11 build_*.bat all had `d:\Homebrew4\VIS\...` hardcoded. The first grep used a slightly off escaping and underreported (only 3 hits). A second pass with a different pattern caught them all. Genericized via `__file__` (Python) and `%~dp0` (batch). Lesson: when sweeping for path leaks, run multiple distinct patterns — single grep can underreport.
- **Gotcha S5.3 — sed in Git Bash + `\` paths:** `sed -i 's|d:\\Homebrew4\\...|...|'` produced no matches even though grep confirmed the pattern was present. Suspected MSYS2 path-conversion of `d:\` arguments. Workaround: dropped sed, used a small Python one-liner via `pathlib.glob` + `read_text/write_text`. Lesson: don't trust sed for backslash-heavy Windows path edits in MSYS2.
- **Gotcha S5.4 — `gh` not on PATH after winget install:** new console / Git Bash session doesn't pick up the modified PATH until restart. Worked around by calling `"/c/Program Files/GitHub CLI/gh.exe"` with full path for the rest of the session. Memory `reference_mame_path` documents an analogous case for `mame.exe`.
- **Gotcha S5.5 — Bottom-up DIB + partial source rect in StretchDIBits:** central A.9 bug; documented as `reference_stretchdibits_partial_src_gotcha`. Always use full-source `StretchDIBits` for biHeight>0 DIBs and rely on `InvalidateRect` to clip physical writes.
- **Gotcha S5.6 — `LicenseInfo: null` on first `gh repo view` after LICENSE push:** GitHub's licensee gem hadn't reindexed yet. Sidebar showed "MIT License" within seconds anyway — `gh` just returned stale cached metadata.
- **Eureka S5.E1 — Watcom large-model handles a second 64 KB buffer transparently:** `static BYTE static_bg[64000]` next to the existing `framebuf[64000]` compiled and ran without any `__far` / `__huge` annotation, no segment-overflow warning, no runtime issue. Watcom puts each 64 KB array in its own data segment automatically.
- **Eureka S5.E2 — GDI clipping makes "full-source partial-dest blit" effectively as fast as a true partial blit on small dirty regions:** the readout of 64 KB src is dwarfed by the savings from not having to recompute the framebuf. Combined with `DIB_PAL_COLORS` (zero per-pixel color match), the practical perf is dominated by the size of `InvalidateRect`'s rect, not by the StretchDIBits source size.
- **Eureka S5.E3 — VIS_sessions.md scrub before publication is non-trivial:** even a careful initial pass missed 3 absolute-path leaks in prose lines. The git remote being public makes every paragraph a potential leak surface, not just code.
- **Gotcha S5.7 — IMF batched scheduler `sqHackTime = alTimeCount + delay`:** verbatim port from SDL_ALService produces the wrong tempo when `alTimeCount` advances in batches instead of by 1 per ISR. Cumulative `sqHackTime += delay` is the canonical fix. See `reference_imf_scheduler_gotchas`.
- **Gotcha S5.8 — WM_TIMER too coarse for IMF dispatch:** Win16 timer minimum ~55 ms vs IMF tick 1.43 ms. Driving `ServiceMusic` from WM_TIMER produces audible per-beat drag. Move scheduler to a `PeekMessage` idle loop with `WaitMessage()` fallback when music inactive.
- **Gotcha S5.9 — DGROUP overflow with audio buffers:** `music_buf[24000]` + `audio_offsets[1156 B]` overflowed Watcom's default data segment by 7891 B. `__far` keyword on the array declarations forces explicit far-segment placement.
- **Eureka S5.E4 — `AUDIOHED.WL1` shareware re-pack ≠ `AUDIOWL1.H` constants:** the SDK header has `NUMSNDCHUNKS = 234` and `STARTMUSIC = 207`, but our actual `AUDIOHED.WL1` is 1156 B = 289 DWORDs (288 chunks) with music at indices 260..287. Don't trust the SDK constants — count the file. Music chunks are paired with 88-byte placeholders (likely empty MUSE slot headers), so identify "real" tracks by length > 1 KB.
- **Eureka S5.E5 — `__far` keyword still useful in Watcom -ml:** even though `-ml` (large memory model) defaults pointers to far, static array placement is heuristic — small arrays go in DGROUP, big ones (>= ~32 KB) get their own segment. Explicit `__far` overrides the heuristic.
- **Eureka S5.E6 — `PeekMessage` + `WaitMessage` idle loop is the canonical pattern for sub-WM_TIMER scheduling on Win16.** Holds for music, animations, polling external IO. No need to involve MMSYSTEM unless ms-precise scheduling is required.

### Next-step candidates for Session 6

A.9 + A.10 closed in S5. Foundation is now performant enough AND can play music; remaining items toward Wolf3D PoC:

1. **A.10.1 — IMF frame-skip polish (optional).** User reports occasional audible "stacco" during playback, likely 1-2 % drift accumulated by integer rounding in `(elapsed_ms * 700) / 1000`. Two possible fixes: (a) maintain a fractional remainder accumulator (`ticks_remainder` in tick-thousandths); (b) switch to MMSYSTEM `timeSetEvent` for ms-precise scheduling. Not blocking — fixable when convenient.
2. **A.11 — Unified integrated scene.** Walls + sprites + minimap + cursor + click-sounds + music together in one demo scene. All A.3..A.8 + A.9 + A.10 primitives composited. No new tech, careful integration.
3. **A.12 — Scaler port.** Port `WL_SCALE` / `OLDSCALE.C` simple-path so `DrawSprite` can render at variable size. Final tooling before the raycaster.
4. **A.13 — Raycaster.** Last (per the "be gentle with the raycaster" rule). Map grid in memory (plane0), palette + textures + sprites + framebuf + input + audio all present and proven.
5. **`hcControl HC_SET_KEYMAP`** — remap VK_HC1_* slots back to standard VK codes for ergonomic switch-cases. Cosmetic, not critical.
6. **Asset audit utility.** Small Python script to list AUDIOHED chunk lengths + music name guesses (matching paired chunk indices to AUDIOWL1.H enum order). Useful for picking different music tracks than the chunk-261 default.

S6 recommendation: (2) integrated scene — consolidates everything we have, sets the stage cleanly for the raycaster. The IMF frame-skip polish (1) is worth a quick stab if the user notices it during the integrated demo.

### S5 wrap-up

Three distinct deliverables in one session: (a) project went from local-only to public open-source on GitHub with proper licensing, copyright hygiene, and English documentation; (b) A.9 perf foundation closes a real input-lag problem and unlocks animations / scheduler work; (c) A.10 IMF playback PoC — Wolfenstein 3D music audible on Tandy/Memorex VIS for the first time, with three iteratively-debugged scheduler bugs producing two new gotcha memories. Pacing was uneven (Part 1 took longer than expected because of the multi-pass copyright scrub), but no scope was deferred. Workflow rule established mid-session: `VIS_sessions.md` + `README.md` status table both updated as part of every milestone commit, not in catch-up commits afterward — adopted starting with A.10.

S5 produced two big foundation pieces (perf + audio) plus a publishable repo. Wolf3D PoC remaining work narrows to: integrated scene composition, sprite scaler, and raycaster. With audio + input + minimap + sprites + walls + music all proven, the raycaster is the only major unknown left.

---

## Session 6 — 2026-04-25 — Milestone A.11 (integrated demo scene)

**Scope:** consolidate every primitive proven in A.1..A.10 into a single composited scene before any new tech (scaler/raycaster). The `project_S6_todo_opening` memo recommended this as the global compatibility check between perf foundation, asset loaders, audio scheduler, and HC input. User confirmed at session open. No new subsystem; the value is provability that the existing primitives coexist cleanly.

### Layout (320×200)

| Region | Content | Source |
|---|---|---|
| y=0..29 | Debug bar (heartbeat, focus, map/vswap status, audio LED, msg/key counters, bit grids) | A.6/A.9 |
| y=35..98, x=0..255 | 4 wall textures from VSWAP (chunks 0..3) | A.4 |
| y=35..98, x=256..319 | E1L1 minimap compressed at TILE_PX=1 (64×64) | A.7 |
| y=99..104 | Black gutter (visual separator) | new in A.11 |
| y=105..168 | 3 sprite gallery (Demo / DeathCam / STAT_0) at x=15, 125, 235 | A.5 |
| y=169..199 | Free band, cursor allowed | — |

Cursor is clamped to `x∈[5,314], y∈[35,194]` so it can move over walls, sprites, minimap, or the bottom band — but never enters the debug bar (which is repainted every 500 ms on WM_TIMER and would clobber cursor pixels).

### Implementation

`src/wolfvis_a11.c` (~720 LOC) is a clone of `wolfvis_a10.c` with:

- **VSWAP loader** ported from `wolfvis_a5.c` (`LoadVSwap`, `DrawWallStrip`, `DrawSprite`). Now sized for `WALL_COUNT=4` (4×64 = 256 wide, leaves a clean 64-wide column for the minimap on the right). `NUM_SPRITES=3` unchanged.
- **`DrawMinimapCompressed`** — new variant of `DrawMinimap` at `TILE_PX=1`, no border, fixed at top-right (256, 35). Combines plane0 wall color with plane1 object color in a single per-pixel pass.
- **`SetupStaticBg`** — composites walls + minimap + sprites + gutter into framebuf, then snapshots into `static_bg[64000]`. Cursor erase via `RestoreFromBg` (A.9 pattern) now correctly restores wall/sprite/minimap pixels under the cursor.
- **`VK_HC1_F1`** rebound to one-shot "init OPL3 + start music" (A.10 had separate F1=note and F3=start music). Idempotent — checks `!sqActive` before re-entering. Brings the whole audio stack to life with a single button press from the cold-start state.
- **`VK_HC1_F3`** = stop music (was secondary purpose in A.10).
- **`VK_HC1_PRIMARY` / `VK_HC1_SECONDARY`** = high/mid click tones using channel-0 OPL writes; music continues unaffected because the IMF stream uses channels 1..8.

### Memory

VSWAP additions: `walls[4][4096] = 16 KB`, `sprites[3][4096] = 12 KB`, `pageoffs[700] = 2.8 KB`, `pagelens[700] = 1.4 KB`. Both `walls[]` and `sprites[]` declared `__far` to keep DGROUP under 64 KB on top of A.10's existing carmack/RLEW/map planes + audio buffers. Without `__far` placement, link-time DGROUP overflow would have hit again as in A.10's S5 gotcha.

### Build

- Compile: `wcc -zq -bt=windows -ml -fo=..\build\wolfvis_a11.obj wolfvis_a11.c` — clean, no warnings.
- Link: `wlink @link_wolfvis_a11.lnk` — IMPORT `hcGetCursorPos HC.HCGETCURSORPOS` retained (mandatory for WM_KEYDOWN routing per A.8 gotcha).
- Output: `build/WOLFA11.EXE` (195 KB; +34 KB vs A.10 for `walls[] + sprites[] + pageoffs[] + pagelens[]`).
- ISO: `build/wolfvis_a11.iso` (1.16 MB). `cd_root_a11/` carries A.10 file set + `VSWAP.WL1` (742 KB shareware) + new `WOLFA11.EXE` and `SYSTEM.INI` with `shell=a:\WOLFA11.EXE`.

### Result

First-attempt success in MAME 0.287 vis. Snapshot captured in `src/snap/vis/0001.png` (note: MAME writes snapshots to `<cwd>/snap/`, which depends on where MAME is launched from — not the rompath). Visible:

- Debug bar with bit grids and heartbeat block alternating.
- 4 wall textures at top-left (Wolf3D first-bank wall art: stone, brick, wood-like).
- Minimap top-right showing E1L1's room layout with cyan dots for guards, green markers for items, "T" shape (the elevator).
- "Demo" sprite (red text bitmap, BJ shareware-version splash) and "DeathCam" sprite (yellow text) in the middle band. STAT_0 visible as a small cyan blip in the right gallery slot — STAT_0 is a small floor-prop sprite, just a few columns wide at the bottom of its 64×64 bbox.
- Cursor moves smoothly over the entire scene under d-pad input. Erase-and-redraw via static_bg restores walls/sprites/minimap pixels correctly under the cursor's previous position.
- F1 starts CORNER_MUS playback. PRIMARY and SECONDARY trigger short OPL clicks without disrupting the music stream (channel-0 vs channels-1..8 separation).

User confirmation: "Tutto confermato! Tutto funzionante!"

### IMF "swallowed note" diagnosis (carryover A.10 issue)

User reports the same "stacco" observed at the end of S5 — but characterizes it more precisely now: occasionally a note is *eaten* and the next phrase starts early, as if a 4/4 bar shortened to 7/8. Not constant, not periodic.

Diagnosis: this is *not* the integer-rounding drift originally hypothesized in `project_S6_todo_opening` (that would slow the tempo, not skip notes). The actual mechanism is **burst-processing after a paint stall**:

1. Every 500 ms the WM_TIMER fires `DrawDebugBar` + `InvalidateRect(top 30 rows)`.
2. The next WM_PAINT runs `StretchDIBits(... 320×200 ...)`. Even with `DIB_PAL_COLORS` (no per-pixel color match), a 64 KB blit on emulated 80286 at 12 MHz takes 20-50 ms.
3. During that paint, neither PeekMessage nor `ServiceMusic` runs. `GetTickCount` keeps advancing.
4. When PeekMessage returns control to the idle path, `elapsed_ms` is 20-50 ms → `ticks_advance = 14-35` → the while loop drains 20-30 IMF events back-to-back without time for OPL envelopes to play out between coupled key-on/key-off pairs.
5. Audible effect: a phrase of stacked notes loses its inter-note attack/release dynamics and the perceived rhythm "compresses" by one beat.

The cumulative *average* tempo is still correct (because `sqHackTime += delay` accumulates virtual time independently of when alTimeCount catches up — the S5 fix). But intra-burst microtiming is collapsed.

Fix candidates for A.10.1 (deferred — not blocking PoC):
- **(a) Cap events per call.** Limit the while loop to e.g. 4 events per `ServiceMusic`, then return. Idle-loop spins fast enough that the next ServiceMusic call happens within 1-2 ms and processes the next batch with proper inter-event spacing.
- **(b) Service music between dispatched messages too.** Currently ServiceMusic only runs when PeekMessage returns FALSE. Adding `if (sqActive) ServiceMusic();` after each DispatchMessage gives an extra slot during message bursts.
- **(c) Multimedia timer (`timeSetEvent`).** Higher-resolution callback independent of the message loop. More invasive (link-script change, callback function in fixed code segment) but architecturally cleaner.

### Concrete results

- New: `src/wolfvis_a11.c` (~720 LOC), `src/link_wolfvis_a11.lnk`, `src/build_wolfvis_a11.bat`, `src/mkiso_a11.py`.
- New: `cd_root_a11/` (8 files: AUTOEXEC, CONTROL.TAT, SYSTEM.INI, MAPHEAD/GAMEMAPS/AUDIOHED/AUDIOT/VSWAP .WL1, WOLFA11.EXE).
- New: `build/WOLFA11.EXE` (195 KB), `build/wolfvis_a11.iso` (1.16 MB).
- New: `src/snap/vis/0001.png` (PoC screenshot).
- README status table: A.11 ✅, A.12 marked 🚧 next. Quick-start build/launch commands updated to reference A.11 binaries.

### Trap / Gotcha / Eureka (S6)

- **Gotcha S6.1 — MAME snapshot directory follows MAME's cwd, not `-rompath`.** F12 in MAME writes to `<cwd>/snap/<driver>/NNNN.png` where `<cwd>` is the directory MAME was launched from. We launched MAME from `/d/Homebrew4/VIS/src` (residual cwd from a previous bash command), so the A.11 snap landed in `src/snap/vis/0001.png` instead of the expected `snap/vis/`. Not a bug — just MAME default behavior. To pin it, pass `-snapshot_directory <abspath>` or always launch MAME from the project root.
- **Eureka S6.E1 — `__far` BSS placement scales linearly to multiple large arrays.** A.10 needed `__far` on `music_buf[24000]`. A.11 added `walls[16 KB]` + `sprites[12 KB]` + `pageoffs[2.8 KB]` + `pagelens[1.4 KB]`. All marked `__far` (or auto-segmented for the 16 KB walls), and Watcom's large-model linker placed each in its own data segment without complaint. DGROUP usage stayed flat at the A.10 level; no new overflow despite +34 KB of asset BSS.
- **Eureka S6.E2 — IMF "swallowed note" is not drift, it's burst-processing.** Originally hypothesized as integer-rounding drift in `(elapsed_ms * 700) / 1000`. The user's musical characterization (4/4 → 7/8, missing beat) localized it to a *temporal compression* during a single ServiceMusic call after WM_PAINT held the message loop. Average tempo still correct. Fix is local (cap events per call), not architectural (no need for multimedia timer).
- **Eureka S6.E3 — Channel separation lets click SFX coexist with IMF music.** OPL channel 0 is unused by Wolf3D's IMF stream (which targets channels 1..8 and rhythm). Writing OPL note-on to channel 0 from WM_KEYDOWN does not interfere with running music. Useful for future click-feedback / sound effects without needing a full SFX scheduler.

### Next-step candidates for Session 7

1. **A.10.1 — IMF burst polish.** Add per-call event cap (4-8 events) + ServiceMusic between dispatched messages. Should eliminate the audible "swallowed note" without architectural changes. ~30 min.
2. **A.12 — Sprite scaler.** Port `SimpleScaleShape` from `WOLFSRC/OLDSCALE.C`. Fixed-point math (no WOLFHACK assembly path). One sprite cycles size with a button. Last tooling step before the raycaster. ~1-1.5 h.
3. **A.13 — Raycaster.** Per the "raycaster gentle" rule, this comes only after A.12. All foundation present and proven (palette, walls, sprites, framebuf, input, audio, perf, integrated scene). WL_DRAW port over the existing primitives.
4. **`hcControl HC_SET_KEYMAP`** — remap VK_HC1_* slots to standard VK codes. Cosmetic ergonomics.
5. **Asset audit utility.** Python script to map AUDIOHED chunk indices → Wolf3D track names (CORNER_MUS, NAZI_NOR, etc.) for music selection.

S6 wrap recommendation: start S7 with (1) A.10.1 polish (~30 min warm-up that eliminates a known audible defect), then attack (2) A.12 scaler — the last subsystem before the raycaster.

### S6 wrap-up (interim — A.11 closed, A.10.1 follows)

Single milestone, single sitting, first-attempt build success, first-attempt MAME run validated by user. The "no new tech, careful integration" prediction held — every primitive (walls, sprites, minimap, cursor, music, click tones, perf erase/redraw) coexists in one frame without subsystem conflicts. The compatibility check the memo identified as the *purpose* of A.11 returned green: PeekMessage idle + dirty-rect blit + audio scheduler + HC input all interoperate cleanly.

The IMF burst-processing diagnosis carried over into the A.10.1 polish that ran immediately after A.11.

### Milestone A.10.1 — IMF burst polish (S6 follow-up)

User confirmed momentum after A.11 success: "direi A.10.1 di polish, possiamo proseguire direttamente qui". Goal: eliminate the residual "stacchi" / "nota mangiata" effect in IMF playback. What followed was a multi-fix iteration that produced infrastructure improvements and platform diagnostics, but the user-perceived audio defect remained. The polish is now committed as **partial / infrastructural** and the residual artifact is documented as a known-limitation for future investigation.

**Patch sequence (each tested in MAME between iterations):**

1. **Events cap = 4 + service-after-dispatch + remove early-return on `ticks_advance == 0`.**
   - Theory: post-WM_PAINT burst-drain compresses note timing.
   - Fix: cap events per ServiceMusic call so backlog spreads across multiple idle iterations; service music between every dispatched message so paint stalls have a shorter window.
   - Result: no perceptible change. Stacchi still present.

2. **MMSYSTEM `timeGetTime` + `timeBeginPeriod(1)`.**
   - Theory: GetTickCount on Modular Windows VIS is bound to the BIOS PIT (18.2 Hz) → 55 ms-quantized `elapsed_ms` is the real source of bursts.
   - Fix: switch to `timeGetTime` for ms-precision, request 1 ms via `timeBeginPeriod`. MMSYSTEM.DLL is loaded by SYSTEM.INI so static-import is safe.
   - **Result: hard regression — music plays at ~50 % speed.** Tempo halved exactly. Either MMSYSTEM on Modular Windows VIS uses a counter at half the wall-clock rate, or `timeBeginPeriod` reprograms a timer that `timeGetTime` then mis-reads. Reverted; documented as `reference_mmsystem_vis_half_rate`.

3. **PIT-direct via latch read (port 0x43 mode 0x00 + two `inp(0x40)`).**
   - Theory: bypass GetTickCount and MMSYSTEM altogether. Read PIT counter 0 directly for sub-ms precision. Counter 0 is shared with the BIOS PIT IRQ 0 handler, but that handler only increments BDA tick on wrap and never touches the counter, so latched reads are non-disruptive — no CLI/STI required.
   - Implementation: `ReadPitCounter` (latch + 2 reads), `AdvanceAlTimeFromPit` (cycle-diff with wrap detection + fractional accumulator → IMF tick increments), wired into `ServiceMusic` and `StartMusic`.
   - First test with theoretical divisor `PIT_CYCLES_PER_IMF_TICK = 1704` (1193182 / 700): music **very slow**.
   - Calibration: empirically tried `852` (half). **Result: tempo correct.** This implies MAME-VIS emulates the PIT at ~596 kHz (half the 1.193 MHz standard PC rate), or BIOS programs counter 0 in mode 2 with a 596 kHz input clock derived from the 14.318 MHz OSC / 24. Either way, 852 visible-counter cycles = 1 IMF tick. Documented as `reference_pit_596khz_vis`.
   - **Stacchi still present** even with hi-res clock running. So GetTickCount granularity was *not* the root cause of the artifact (or not the only one).

4. **Skip-the-gap (`alTimeCount > sqHackTime + 4` → snap back).**
   - Theory: even with PIT-direct, post-WM_PAINT (~50 ms) accumulates ~35 IMF ticks of backlog; the while loop drains all of them in a few hundred microseconds, compressing note durations.
   - Fix: when alTimeCount runs >4 ticks (~5.7 ms) ahead of next event, snap alTimeCount back to sqHackTime. The while loop then fires only the next chord and exits; natural pacing resumes from there. Trade-off: brief out-of-phase from heartbeat after each stall, in exchange for preserved note durations.
   - Result: no perceptible change. Either skip-the-gap rarely fires (PIT-direct already stays close to natural pace), or the artifact comes from elsewhere.

5. **OplDelay sweep.**
   - Theory: MAME-VIS OPL3 emulation may need different recovery timing between register writes; chord burst events too close together get merged.
   - Doubled (12, 70) instead of (6, 35): **regression — note mangiate appear *earlier* in playback**.
   - Halved (3, 17): no change vs baseline (6, 35).
   - Conclusion: OplDelay below the original threshold is irrelevant; above it, things degrade. The OPL register-write timing is *not* the dimension causing the artifact. Restored canonical (6, 35).

**Final state of `wolfvis_a11.c` (committed):**
- PIT-direct clock with empirical 852 divisor — kept (gives sub-ms precision, infrastructural value for future work).
- Skip-the-gap with threshold +4 — kept (defends against worst-case stalls even if doesn't fix the perceived artifact).
- Service-after-dispatch — kept.
- Events cap removed (PIT-direct natural pacing makes it redundant for steady-state).
- OplDelay restored to canonical (6, 35).
- GetTickCount logic removed; `sqLastTick` retained as a stub for symmetry but unused.

**Unresolved hypotheses (deferred to S7+):**
- (C) PIT wrap-detection edge case causing spurious alTimeCount jumps and triggering skip-the-gap spuriously. Requires runtime tracing/counter dump to validate.
- (D) Modular Windows VIS owns its own timer ISR that touches PIT counter 0 between our latch and reads, breaking the non-disruptive assumption. Would require disassembly of MW timer driver.
- (E) MAME OPL3 emulation in the `vis` driver has a higher minimum inter-event time than real Yamaha YMF262, causing chord events to drop voices at sub-ms spacing. Would require comparison with a known-good IMF player on emulated VIS, or running on real VIS hardware.
- (F) The IMF stream itself has dense passages where, even at correct timing, OPL envelope ADSR doesn't have time to render coupled key-on/key-off pairs audibly. Would require comparing chunks 261, 263, 268, 285 etc. to see if the artifact is stream-specific or generic.

**Why kept the partial fix instead of full revert:** PIT-direct + 852 divisor is platform knowledge worth committing — discovered by this session, useful for any future VIS audio/timing work. Skip-the-gap and service-after-dispatch are non-regressive (don't make anything worse, may help in some cases). The OplDelay value is the canonical Wolf3D one, restored. Net: code is at the diagnostic frontier with documented hypotheses, not in a worse state than baseline A.11.

**Time spent:** ~50 min real-time on A.10.1 across 5 patch iterations. Original budget 30 min — overrun was OK because diagnostic mileage (MMSYSTEM regression, 596 kHz PIT, hypothesis space narrowed) was high and user explicitly extended the slot.

### S6 final wrap-up

Two milestones in one session: **A.11 (integrated scene)** clean first-attempt success, and **A.10.1 (IMF polish)** which closed as a partial/infrastructural milestone with diagnostic value but no audible improvement to the artifact. Wolf3D PoC remaining work narrows further: only A.12 sprite scaler and A.13 raycaster left as new subsystems. The audio polish sits in the "good enough for PoC" tier with documented reopening conditions if the artifact becomes blocking for the playable demo.

Workflow rule re-confirmed: code + VIS_sessions.md + README.md updated together in the same commit (regola consolidated since S5).

---

## Session 7 — 2026-04-25 — Milestone A.12 (sprite scaler)

**Scope:** port a sprite scaler over the existing A.5 t_compshape pipeline. This is the last subsystem before the raycaster (A.13). Per the "raycaster gentle" rule, A.12 is the natural warm-up — the column-by-column post-walk pattern is exactly what the raycaster needs for wall-strip rendering. User confirmed the scope at session open ("procedi"); A.10.1 left in its partial state from S6 (audio polish reopens only before showcase).

### Design choice — chunky 2D scaler, NOT the original Wolf3D JIT path

`WOLFSRC/OLDSCALE.C` ships two scalers: `ScaleShape` (clipped) and `SimpleScaleShape` (no clipping). Both are *compiled scalers* — `BuildCompScale` JITs x86 machine code into a per-height buffer, then `ScaleLine` calls into the compiled code with EGA/VGA SC_MAPMASK port writes and write-mode 2 plane manipulation. **Completely irrelevant to our chunky 320×200×8 framebuf model.** No plane masks, no GC_MODE, no compiled buffers — we already write one byte per pixel into a flat array.

Two viable approaches for our model:

1. **Decompress sprite to 64×64 chunky + 2D bilinear scale.** Simpler code, but needs ~12 KB additional BSS (3 sprites × 4 KB) and the inner loop is two separate scales (x and y) without any structural similarity to what the raycaster needs.
2. **Per-column post-walk fixed-point scaler over the existing t_compshape.** Same data layout `DrawSprite` already consumes; for each destination column dx, compute `srcx = (dx - dest_left) * 64 / scale`, walk the post triples for that source column, map each `(starty..endy)` source range onto a destination range via the same fixed-point ratio, write one pixel per destination row.

Picked (2). The function's column-walk shape is the seed for the raycaster wall-strip renderer in A.13 (raycaster gives a per-column wall height; for each column we draw a strip of texture pixels at that height — same scaling math, different source). Zero additional memory. The two long divisions per pixel inside the inner loop are not free on a 286 but they're amortized over single-sprite scale; for the raycaster we'll precompute step deltas to amortize them across columns.

### Implementation

`src/wolfvis_a12.c` (~770 LOC) is a clone of `wolfvis_a11.c` plus:

- **`DrawSpriteScaled(int xc, int yc_top, int idx, int scale_h)`** — square dest, side = `scale_h` pixels. Centered horizontally on `xc`, top-anchored at `yc_top`. Walks `dataofs[srcx - leftpix]` to find each column's post list, then for each post `(endy*2, corr_top, starty*2)` maps to `[dy_start, dy_end)` and writes pixels with bottom-up DIB y-flip (same as A.5 `DrawSprite`). Inner loop clamps `sy_src` to `[starty_src, endy_src - 1]` to guard against integer round-up at segment boundaries (which would feed a wrong `corr_top + sy_src` and write the next post's first pixel to the previous post's last row).
- **`ScaleRect(RECT *r, xc, yc_top, scale_h)`** — bbox helper for invalidate/restore math.
- **A.12 state**: `g_scale = 64` (initial 1:1), `g_scale_xc = 157`, `g_scale_yc = 105`, `g_scale_prev_rc = {125,105,189,169}`. Bounds `SCALE_MIN=16`, `SCALE_MAX=160`, step 8.
- **Layout change**: center sprite slot (idx 1, DeathCam) is now *dynamic*. `SetupStaticBg` only draws sides (idx 0 Demo at x=15, idx 2 STAT_0 at x=235). The center sprite is rendered after `SetupStaticBg` in `WinMain`, and re-rendered on every `WM_KEYDOWN` so it stays in sync with `g_scale`.
- **WM_KEYDOWN unified erase/redraw**: prev rect of cursor + prev rect of scaled sprite both restored from `static_bg`, then both re-drawn at current state, then a single `InvalidateRect` over the union of all four rects (cursor prev/curr + scale prev/curr). Cursor is drawn after the sprite so it can ride on top.
- **A/B remap**: `VK_HC1_PRIMARY` now does `g_scale += 8` (clamp 160) plus the A.11 high click tone; `VK_HC1_SECONDARY` does `g_scale -= 8` (clamp 16) plus the A.11 mid click + key-off. Click feedback retained — both effects coexist (scaler is visual, OPL ch0 click is audio).

### Build

- Compile: `wcc -zq -bt=windows -ml -fo=..\build\wolfvis_a12.obj wolfvis_a12.c` — clean, no warnings.
- Link: `wlink @link_wolfvis_a12.lnk` — same `IMPORT hcGetCursorPos HC.HCGETCURSORPOS` as A.11.
- Output: `build/WOLFA12.EXE` (197 KB; +1 KB vs A.11, just `DrawSpriteScaled` + `ScaleRect`).
- ISO: `build/wolfvis_a12.iso` (1.16 MB). `cd_root_a12/` is the A.11 file set with `WOLFA12.EXE` and `SYSTEM.INI` updated to `shell=a:\WOLFA12.EXE`.

### Result

First-attempt success in MAME 0.287 vis (after the launch-command false-start; see Trap S7.1). Three snapshots in `snap/vis/0000.png` (~150 px), `0001.png` (~100 px), `0002.png` (~50 px) — the DeathCam sprite at three different scales, side sprites unchanged 1:1 as size reference, walls / minimap / debug bar / cursor all intact. Zero ghosting. Zero observable regression on any A.11 subsystem.

User confirmation: "Confermo tutto funzionante."

### Trap / Gotcha / Eureka (S7)

- **Trap S7.1 — MAME launch command must include `-rompath .`.** First launch attempt used `mame vis -cdrom ... -window -resolution 640x480 -nofilter -snapshot_directory snap` and crashed pre-VIS-logo. Without `-rompath .` MAME can't find `vis.zip` (the BIOS rom set in project root) and exits before the driver finishes initializing — manifests as a window that closes immediately. The canonical command from `README.md` is `mame -rompath . vis -cdrom build/wolfvis_aXX.iso -window -nomax -skip_gameinfo`. Lesson: never improvise MAME flags when the project README documents a working command — copy verbatim. (`-rompath .` is the single non-optional bit; `-window -nomax -skip_gameinfo` are ergonomic.)
- **Eureka S7.E1 — Per-column post-walk scaler is the right shape for both sprites and raycaster walls.** The Wolf3D source ships the JIT compiled-scaler approach because in 1992 on a 386 in EGA/VGA planar mode, runtime-emitted `mov al,[si+N] ; mov es:[di+M],al` sequences eliminated all loop overhead. In our 8bpp chunky model with two long divisions per pixel, the per-column shape is *cleaner* than the 2D decompress-and-scale alternative AND directly maps to "for each ray column, draw a wall-strip of N texture pixels at scaled height." A.13 will be a remarkably small delta from this code — same column loop, source pixels come from a wall texture column instead of a sprite post chain.
- **Eureka S7.E2 — RestoreFromBg-then-redraw works for two independent dynamic layers.** A.9 introduced `static_bg` for cursor erase. A.12 stresses it with a *second* dynamic layer (the scaled sprite) whose bbox can change every keypress. Because `static_bg` contains neither the cursor nor the scaled sprite, the two restores are commutative; we can erase both, then redraw both, with no ordering hazard. The pattern generalizes: for the raycaster, the entire 3D viewport will be a single dynamic layer over a static HUD background — same primitive, larger rect.
- **Eureka S7.E3 — Sub-pixel rounding at segment boundaries needs an inline clamp.** Inside the inner dy loop, `sy_src = (dy - yc_top) * 64 / scale_h` was occasionally rounding to `endy_src` (one past the post's last source row) when `dy = dy_end - 1` and `scale_h` was non-power-of-2 (e.g., scale=72). Without the `if (sy_src >= endy_src) sy_src = endy_src - 1` guard, the function would index into the next post's first source row using the *current* post's `corr_top`, painting a single wrong-color pixel at the bottom of each segment. Caught during code review before the first build — so the artifact never appeared in MAME, but the guard stays. Lesson: when both dy_start and dy_end are computed by the same `*scale_h/64` formula, the inverse computed inside the loop can land outside the inclusive-exclusive range.

### Concrete results

- New: `src/wolfvis_a12.c` (~770 LOC), `src/link_wolfvis_a12.lnk`, `src/build_wolfvis_a12.bat`, `src/mkiso_a12.py`.
- New: `cd_root_a12/` (9 files: A.11 set + WOLFA12.EXE, SYSTEM.INI updated).
- New: `build/WOLFA12.EXE` (197 KB), `build/wolfvis_a12.iso` (1.16 MB).
- New: `snap/vis/0000.png`, `0001.png`, `0002.png` (DeathCam at three scales, in `<project-root>/snap/` because we launched MAME from project root this time — see S6.1 gotcha).
- README status table: A.12 ✅, A.13 marked 🚧 next. Quick-start build/launch commands updated to reference A.12 binaries.

### Next-step candidates for Session 8

1. **A.13 — Raycaster.** All foundation present and proven (palette, walls, sprites, scaler, framebuf, input, audio, perf, integrated scene). `WL_DRAW.C` port over the existing primitives. Per the column-walk eureka above, the scaler from A.12 is essentially the wall-strip renderer with a different source. Estimated 2–3 h.
2. **A.10.1 reopen.** Only if the IMF "stacchi" become user-blocking for the showcase of the playable PoC. Start from hypothesis E (test other music chunks 263, 268, 285).
3. **HUD / status bar.** BJ face, ammo, score — would build on A.5 sprite blits and a tiny number-rendering routine. Cosmetic relative to the raycaster.
4. **`hcControl HC_SET_KEYMAP`**. Remap `VK_HC1_*` to standard `VK_*` for ergonomic switch-cases. Cosmetic.

S7 wrap recommendation: go straight to A.13. The scaler closes the last subsystem prerequisite; everything else is now polish or post-PoC.

### S7 wrap-up

Single milestone, single sitting, first-attempt build success after one launch-command false-start. The "raycaster gentle" rule's prediction held: with the scaler in place, A.13 is no longer a structural unknown — it's a data-source swap (wall texture column instead of sprite post chain) over an already-proven column-walk renderer. The Wolf3D PoC's remaining work has now been narrowed to a single new subsystem (the cast itself) plus polish. Pacing was tight (~30 min real-time on A.12 from "procedi" to "confermato"), well inside the budget that the calibration memo predicts for ~1 h estimates.

Workflow rule re-confirmed once more: code + VIS_sessions.md + README.md in the same commit.

---

## Session 8 — 2026-04-25 — Milestone A.13 (raycaster) + native cursor suppression

**Scope:** the headline milestone the whole port has been deferring. Foundation built across A.1..A.12 is consumed wholesale; A.13 adds a single new subsystem — the cast itself — over an already-proven column-walk renderer (A.12's `DrawSpriteScaled` inner loop, repurposed as a wall-strip). Plus a S8-only side fix: the VIS native arrow cursor that has been visible across every snapshot since A.6 is finally suppressed.

User confirmed scope at session open: "affrontare finalmente il raycaster, forti delle fondamenta che abbiamo costruito".

### Cursor suppression (S8 side fix)

Long-standing leftover the user surfaced at session open: a system arrow cursor visible in every snapshot since A.6, shown over our framebuf even though our app never asked for it. Confirmed by user-supplied snapshot 0001.png from S6 (a clear ~10 px white arrow over the "C" of DeathCam). It is **not** a MAME overlay (`-nomouse` already passed); it's the Modular Windows native cursor that MW renders on its own because the hand-controller subsystem produces cursor events.

Three-point fix applied in `wolfvis_a13.c`:
1. `wc.hCursor = NULL` in WNDCLASS — no class default cursor.
2. `case WM_SETCURSOR: SetCursor(NULL); return TRUE;` — explicit suppression when the mouse / HC enters our client area.
3. `ShowCursor(FALSE)` after `CreateWindow` — backstop the global cursor counter.

Confirmed working in A.13 snapshots 0003.png / 0004.png / 0005.png — the arrow is gone in all three, even when overlapping the wall textures, the player marker, and the heading line. All three points kept (intentional redundancy: we do not know in this firmware which of the three actually wins, and the cost is three lines).

### Layout 320×200 (A.13)

| y | x | content |
|---|---|---|
| 0..29 | 0..319 | Debug bar (heartbeat, status, bit grids) — repainted on WM_TIMER |
| 30..34 | 0..319 | Black gutter |
| 35..162 | 0..127 | **3D viewport 128×128** — 128 ray casts, ceiling / textured wall / floor |
| 35..98 | 140..203 | Minimap 64×64 with player position dot + heading line |
| 99..162 | 140..319 | Black |
| 163..199 | 0..319 | Black |

Cursor HC custom dropped from this milestone: the d-pad now rotates and moves the player, not a cursor. The minimap player dot is the only on-screen indication of position.

### Player + ray model

Player state in Q8.8 tile units:
- `g_px, g_py` = position (long), `g_pa` = angle 0..1023 (int).
- Coordinate convention: X+ east, Y+ south (matches Wolf3D map storage).
- Angle convention: 0 = E (+X), 256 = S (+Y), 512 = W, 768 = N.

`InitPlayer` walks `map_plane1` looking for spawn markers `19/20/21/22` (Wolf3D N/E/S/W player object IDs). On E1L1 this yields a real spawn position and heading. Falls back to first non-wall tile + east if no marker found.

`IsWall` treats both wall tiles `1..63` and door tiles `90..101` as blocking. Doors render with a fallback wall texture for now (no door logic in A.13 — that's a future milestone).

`TileToWallTex` maps Wolf3D wall ID → VSWAP page modulo `WALL_COUNT=8` (we load the first 8 wall pages, A.4 used 4). The Wolf3D `(tile-1)*2` formula gives a "light face" page; we ignore the dark-face variant to keep memory flat.

### Cast algorithm (PoC step-by-fraction)

For each viewport column `col ∈ [0, VIEW_W)`:
1. `half_fov_a = (col - VIEW_W/2) * FOV_ANGLES / VIEW_W`, where `FOV_ANGLES=192` over `ANGLES=1024` → ~67.5° total horizontal FOV.
2. `ra = (g_pa + half_fov_a) & ANGLE_MASK`.
3. `CastRay(ra)` returns Euclidean distance + `tex_idx` + `tex_x`.
4. Fish-eye correction: `perp_dist = dist * cos(half_fov_a)` via `fov_correct[col]` precomputed Q15 cos table.
5. `DrawWallStripCol(col, perp_dist, tex_idx, tex_x)`.

`CastRay` advances the ray in 1/16-tile sub-steps and watches for the integer tile (tx, ty) to change. When it does, it picks the wall side (X-face vs Y-face) based on which axis crossed last in the sub-step (sub-tile fractional positions) and reads the texture column offset from the perpendicular fractional coordinate. This is **not** the canonical Wolf3D grid-line DDA — it's a step-by-fraction approximation that's robust to write the first time and trivially debuggable. The Wolf3D-style grid-line DDA is the obvious A.13.1 polish if perf demands it; the PoC at 128 columns × ~50 sub-steps avg already runs faster than user input, so polish is deferred.

Distance is computed via `dx_total / cos(ra)` (X-side) or `dy_total / sin(ra)` (Y-side), both Q8.8. Clamped to ≥ 16 (1/16 tile) to avoid a divide-by-near-zero blowing up `wall_h`.

### Wall-strip renderer

`DrawWallStripCol` is the column-walk pattern from A.12's `DrawSpriteScaled` inner loop with the source swapped: instead of walking sprite post triples, we sample `walls[tex_idx][tex_x*64 + sy_src]`. The rest is identical math:
- `wall_h_pixels = (VIEW_H * 256) / perp_dist_q88` — height in pixels.
- `dy_top = VIEW_CY - wall_h/2`, `dy_bot = dy_top + wall_h`.
- Texture sample step: `sy_step = (64 << 16) / wall_h` in Q16.16.
- Inner loop: `sy_src = sy_acc >> 16; framebuf[fb_y][sx] = texcol[sy_src]; sy_acc += sy_step;`.
- Above `dy_top`: ceiling solid color. Below `dy_bot`: floor solid color.

When `dy_top` is above the viewport top (very close walls), we pre-advance `sy_acc` by `sy_step * (VIEW_Y0 - dy_top)` so the visible portion samples the correct vertical center of the texture. This is the same clipping pattern A.12 ended up needing for very-large sprites.

Eureka S7.E1 prediction held perfectly: the inner loop is a near-exact copy of `DrawSpriteScaled`'s, with the post-walk replaced by a single linear sample. The scaler was indeed the wall-strip's seed.

### Memory layout

- `walls[8][4096]` → 32 KB `__far` (was 16 KB / 4 walls in A.11; A.13 doubled to vary tile textures).
- `sin_q15_lut[1024]` → 2 KB `__far const` (auto-generated `wolfvis_a13_sintab.h`).
- `fov_correct[128]` → 256 B `__far` (init at boot from `sin_q15_lut`).
- `map_plane0/1`, `map_headeroffs`, `audio_offsets`, `music_buf` → kept `__far` from A.10/A.11.
- DGROUP stays comfortably under 64 KB; final EXE 220 KB.

### Build

- Compile: `wcc -zq -bt=windows -ml -fo=..\build\wolfvis_a13.obj wolfvis_a13.c` — clean, no warnings.
- Link: `wlink @link_wolfvis_a13.lnk` — same `IMPORT hcGetCursorPos HC.HCGETCURSORPOS` as A.8+.
- Output: `build/WOLFA13.EXE` (220 KB), `build/wolfvis_a13.iso` (1.16 MB).

### Result

Confirmed working on MAME 0.287 vis after one fix iteration (see Trap S8.1 below). Snapshots `snap/vis/0003.png`, `0004.png`, `0005.png` show three different player orientations:
- Stone walls with embedded Hitler poster textures (Wolf3D wall pages 0, 2, 4, 6 from VSWAP), correctly perspective-scaled.
- Vanishing point at viewport center; nearer walls larger, distant walls smaller, no fish-eye distortion.
- Mid-grey ceiling above, dark-grey floor below.
- Minimap at right with player position dot (cyan) + heading line (white).
- Native VIS cursor absent in all three snapshots.

User confirmation: "Sembra tutto OK!"

### Trap / Gotcha / Eureka (S8)

- **Trap S8.1 — `<math.h>` in Watcom Win16 large model trips WIN87EM.DLL load.** First A.13 build used `sin()` in `InitTrig` to populate `sin_q15[ANGLES]`. EXE built clean (231 KB), but on MAME-VIS the firmware showed "Error loading WOLFA13.EXE" → loop reset to PROGMAN — the same regression that hit pre-A.1 era. Cause: any `<math.h>` FP call drags Watcom's FP emulation runtime into the EXE, which expects `WIN87EM.DLL` at load time. Modular Windows VIS does not ship `WIN87EM.DLL`. Fix: precompute the 1024-entry Q15 sine table at *build* time via a Python helper (4-line generator in shell), embed as `static const int __far sin_q15_lut[1024]` in `wolfvis_a13_sintab.h`, drop `#include <math.h>` and the `sin()` call. EXE shrank to 220 KB (the 11 KB delta = the FP runtime that's no longer linked). Fingerprint for future regressions: EXE size jump + "Error loading" boot loop after adding any `<math.h>` symbol. Documented in new memory `reference_win87em_trap.md`.
- **Eureka S8.E1 — Column-walk renderer reuses across sprite scaler and raycaster wall-strip with one swap.** A.12's `DrawSpriteScaled` and A.13's `DrawWallStripCol` are structurally identical: outer loop over destination columns, inner loop over destination rows, fixed-point step `sy_step = (src_h << 16) / dest_h`, sample `framebuf[dy] = src[sy_src >> 16]`. The only difference is whether `src` is a sprite post chain (variable-length per-column data) or a contiguous 64-byte texture column. Eureka S7.E1 explicitly predicted this; A.13 confirmed at code-write time, before the first build attempt. The pattern generalizes further: any future textured renderer (floor/ceiling cast, sprite-in-world cast, weapon overlay scaler) is the same loop with a different source.
- **Eureka S8.E2 — Native cursor suppression takes three lines, not three sessions.** The native VIS cursor was visible in every PoC snapshot from A.6 onward (~7 sessions). Treated as "annoying but defer" until S8 user surfaced it. The three-point fix (WNDCLASS hCursor=NULL + WM_SETCURSOR + ShowCursor(FALSE)) is canonical Win 3.x / Win16, took ~5 minutes including reasoning, and worked first try on MAME-VIS. Lesson: cosmetic-but-trivially-fixable defects deserve a "fix-now" tier separate from "polish-later" — three lines that have been waiting seven sessions cost more in user-friction than in code review.
- **Eureka S8.E3 — Spawn-marker scan from `map_plane1` is portable across every Wolf3D map.** `InitPlayer` walks the loaded plane1 looking for object IDs 19/20/21/22 and reads position + facing from the first match. No hardcoded coordinates per level; future level switches (E1L2, E1L3, ...) reuse the same bootstrap with no edit. The fallback "first non-wall tile, facing east" path catches the case where the map data is malformed or the markers aren't placed (custom map, demo data corruption, etc.).

### Concrete results

- New: `src/wolfvis_a13.c` (~830 LOC), `src/wolfvis_a13_sintab.h` (Python-generated Q15 sine LUT), `src/link_wolfvis_a13.lnk`, `src/build_wolfvis_a13.bat`, `src/mkiso_a13.py`.
- New: `cd_root_a13/` (9 files: A.12 set with WOLFA13.EXE + SYSTEM.INI updated to `shell=a:\WOLFA13.EXE`).
- New: `build/WOLFA13.EXE` (220 KB), `build/wolfvis_a13.iso` (1.16 MB).
- New: `snap/vis/0003.png`, `0004.png`, `0005.png` (three player orientations in E1L1 with textured walls + minimap + no native cursor).
- New memory: `reference_win87em_trap.md` (added to MEMORY.md index).
- README status table: A.13 ✅. Quick-start build/launch commands updated to reference A.13 binaries (and to include `-nomouse` consistently).

### Next-step candidates for Session 9

1. **A.13.1 — Polish.** Grid-line DDA replacing step-by-fraction (cheaper, sub-pixel-exact texture coords). Light-by-distance shading (Wolf3D's per-distance palette ramp). Door rendering (tile 90..101 should render with a different texture and a thinner profile). Low individual cost but each improves the render quality visibly.
2. **A.14 — Sprites in world.** Place the loaded sprite gallery (Demo / DeathCam / STAT_0) as billboards in E1L1, transform-and-clip per frame, sort by distance, render after walls with z-buffer or painter's algorithm. Reuses A.5 `DrawSprite` and A.12's scaling math. The first time the player sees a Wolf3D guard in 3D space.
3. **A.10.1 reopen.** IMF stacchi — start from hypothesis E (test other music chunks 263, 268, 285) per the partial memo. Should now happen before any "playable demo" showcase.
4. **A.15 — HUD.** BJ face, ammo, score box at bottom of screen. Cosmetic; uses A.5 sprite blits and a tiny number-rendering routine.
5. **`hcControl HC_SET_KEYMAP`**. Remap `VK_HC1_*` to standard VK codes for ergonomic switch-cases. Cosmetic.

S8 wrap recommendation: A.13.1 polish + A.14 sprites-in-world together would deliver the first "this is recognizably Wolfenstein 3D running on a 1992 console" visual. Roughly the size of A.10+A.11 combined — one full session.

### S8 wrap-up

Single milestone, single sitting, one-iteration recovery from the WIN87EM trap. The headline structural unknown of the entire VIS Wolf3D port — the raycaster — is now closed. Foundation chain A.1..A.12 paid off exactly as A.7+A.12 memos predicted: the renderer was a 100-line addition to a 700-line baseline, no architectural change, no new BSS pattern (just a Python-generated LUT), no MAME-side regression on any prior subsystem. The "raycaster gentle" rule held to the letter: by the time we touched the cast, every other subsystem was green and the only failure mode was the FP-runtime side path, which had nothing to do with the cast itself.

Bonus deliverable: native cursor suppression that has been a visible-but-deferred defect for seven sessions, dispatched in 5 minutes inside the same milestone.

Workflow rule re-confirmed: code + VIS_sessions.md + README.md in the same commit.

---

## Session 9 — 2026-04-25 — Milestone A.14 (sprites in world)

**Scope:** the first milestone where the viewport is recognizably Wolfenstein 3D, not just a tech demo of textured walls. Static decoration sprites placed at their plane1 tile positions in E1L1, transformed world→camera each frame, projected to viewport columns with focal=96 px, scaled by inverse depth, rendered after walls with a 1D per-column z-buffer so walls correctly occlude near-side sprites. User confirmed scope at session open: "Procedi" — straight from the S8 wrap recommendation of A.14 first, A.13.1 polish later.

### Subsystems added

- **`sprites[18][4096] __huge`** — Demo + DeathCam + 16 SPR_STAT_*, re-loaded from VSWAP via the A.5 chunk-by-chunk path. The 18×4096 = 72 KB array exceeds Watcom's 64 KB segment cap, so we bumped the qualifier from `__far` (which Watcom rejects with `E1157: Variable must be 'huge'`) to `__huge`. Per-row 4 KB chunks still fit one segment each, so DrawSprite/DrawSpriteWorld dereference into a single sprite row without huge-pointer arithmetic per pixel — only the row index selects which segment.
- **`Object[] g_objects`, `ScanObjects()`** — at boot, walk plane1 and emit one Object per tile whose obj_id lies in 23..38 (Wolf3D static decoration range, mapping to SPR_STAT_0..SPR_STAT_15). Tile center is the world position; sprite_idx = (obj_id - 23) + 2 (the +2 skips Demo/DeathCam slots in our sprite array). Capped at MAX_OBJECTS=64.
- **`g_zbuffer[VIEW_W]`** — long Q8.8 array, written by `DrawWallStripCol` per column with the per-column perp distance, read by sprites for occlusion test.
- **`DrawSpriteWorld(tile_x, tile_y, sprite_idx)`** — world→camera rotation by `-g_pa`, depth/right axes via dot products with cos/sin Q15. Cull if cam_y < 32 (1/8 tile, behind or too close). Screen X = `VIEW_W/2 + cam_x * focal / cam_y` with constant `FOCAL_PIXELS=96` (matches `(VIEW_W/2)/tan(FOV_ANGLES/2 in rad)` for FOV_ANGLES=192). Sprite height in pixels = `(VIEW_H * 256) / cam_y` — same inverse-depth formula as walls so a 64-tile-tall sprite covers the same vertical range as a wall at the same depth. Per-column z-test against `g_zbuffer[col]` skips columns where a wall is in front. Inner sample loop is the A.13 wall-strip / A.12 scaler column-walk pattern unchanged.
- **`DrawAllSprites()`** — painter's-order render: insertion-sort visible objects by descending cam_y, draw back-to-front so closer sprites paint over farther ones in their overlap region. Side-arrays of (depth, obj_idx) pairs avoid reordering `g_objects[]` itself (scan order stable across frames).

### Movement vs render split (door workaround)

User reported first launch trapped in the spawn cell with "stessa stanza chiusa con quadri nazi": E1L1's BJ spawn is a 2-tile cell with one closed door, and our `IsWall` treats door tiles 90..101 as blocking, so the player could not exit. Door-open / door-swing logic is non-trivial (Wolf3D has an AI-driven open/close state machine over WL_INTER) and is correctly A.14.1 polish, not A.14 PoC content. Quick split:

- `IsWall(tx, ty)`: walls + doors (kept). Used by `CastRay` so doors render as wall slabs on screen, preserving the visual integrity of the cast.
- `IsBlockingForMove(tx, ty)`: walls only (new). Used by `TryMovePlayer` so the player walks through closed-door tiles.

Cosmetic glitch: the player visibly "phases through" a wall slab where a door is. Consciously accepted PoC trade-off — being stuck in spawn forever is a strictly worse user experience than walking through a wall. Real door rendering goes into A.14.1 polish.

### Build

- Compile: `wcc -zq -bt=windows -ml -fo=..\build\wolfvis_a14.obj wolfvis_a14.c`. First attempt failed with `E1157: Variable must be 'huge'` on the new `sprites[18][4096]` array — added `__huge` qualifier, recompiled clean.
- Link: `wlink @link_wolfvis_a14.lnk` — same `IMPORT hcGetCursorPos HC.HCGETCURSORPOS` pattern as A.8+.
- Output: `build/WOLFA14.EXE` (231 KB; +11 KB vs A.13 for sprite re-load + ScanObjects + DrawSpriteWorld + insertion sort + z-buffer), `build/wolfvis_a14.iso` (1.16 MB).

### Result

Confirmed working on MAME 0.287 vis after one fix iteration (door-passable movement). Snapshots `snap/vis/0006.png`, `0007.png`:

- 0006: green chandelier sprite (SPR_STAT_2) hanging from ceiling at mid-corridor distance, correctly scaled, opaque pixels in upper half of bbox (matching Wolf3D's ceiling-anchored decoration convention) so it visually appears above horizon. Walls Hitler-poster intact. Z-buffer occludes the sprite cleanly at viewport edges.
- 0007: two sprites visible at different depths down a corridor, scaling proportional to inverse distance, painter's order keeps the near one painting over the farther one in their column overlap.

User confirmation: "Credo funzioni!" — sprite-in-world rendering live and operational.

### Trap / Gotcha / Eureka (S9)

- **Trap S9.1 — Watcom large model 64 KB segment cap on single arrays.** First A.14 build failed on `static BYTE __far sprites[18][4096]` (72 KB) with `E1157: Variable must be 'huge'`. The `__far` qualifier in Watcom large model places the array in a single far segment; the segment max is 64 KB. For arrays > 64 KB use `__huge` instead — Watcom splits across multiple segments and synthesizes huge-pointer arithmetic where needed. Per-row dereferences (`sprites[idx]` to a single 4 KB row) stay within one segment so the inner pixel-sample loops aren't burdened with huge-pointer cost at every access. Fingerprint for future regressions: any new `__far` BSS / data array that totals > 64 KB. Documented inline in `wolfvis_a14.c` BSS comment.
- **Trap S9.2 — Spawn cell trap on E1L1.** First A.14 launch loaded clean (no WIN87EM regression), rendered the viewport, but the player started in BJ's spawn cell (2-tile room with one closed door). Our `IsWall` blocked all movement out — door tiles 90..101 are flagged the same as walls 1..63. Fix: split `IsBlockingForMove` from `IsWall` so movement allows passing through doors while the cast still treats them as walls. Cosmetic: the player visibly walks through a wall slab. A.14.1 polish path: implement Wolf3D-style door AI (open animation, sliding, blocking when partway). Documented in code with rationale block.
- **Eureka S9.E1 — Painter's sort with side-arrays is the right shape for billboard rendering.** Sorting `g_objects[]` in place would shuffle ScanObjects' boot-time order across frames (no harm but loses stability for debugging). Insertion sort over a side array of `(depth, obj_idx)` pairs is O(N²) in worst case but trivially fast at MAX_OBJECTS=64 (real visible count usually 5-15), keeps `g_objects[]` order-stable, and reads cleaner than the in-place version. Same pattern will scale to dynamic enemies in a future milestone — only the side-array gets rebuilt per frame from current world state.
- **Eureka S9.E2 — Per-row dereference dodges huge-pointer cost.** With `BYTE __huge sprites[18][4096]`, an unguarded `sprites[i][j]` would pay huge arithmetic on every pixel sample. But all our reads do `BYTE *row = sprites[idx]; row[j]` (or `row[corr_top + sy]`) where `row` is a `BYTE *` (near-segment within the 4 KB row that fits in one segment). Watcom resolves `sprites[idx]` once into a `BYTE __far *` typed result, then the `*row` reads are simple far accesses inside the single 4 KB sprite-row segment. No huge math per pixel. The inner-loop cost matches `__far` placement — the only added cost was the one-time row-segment selection. Practical lesson: `__huge` for arrays > 64 KB is fine for performance as long as inner loops dereference by row first.

### Concrete results

- New: `src/wolfvis_a14.c` (~1080 LOC), `src/wolfvis_a14_sintab.h` (Q15 sine LUT — copy of A.13 sintab), `src/link_wolfvis_a14.lnk`, `src/build_wolfvis_a14.bat`, `src/mkiso_a14.py`.
- New: `cd_root_a14/` (9 files: A.13 set with WOLFA14.EXE + SYSTEM.INI updated to `shell=a:\WOLFA14.EXE`).
- New: `build/WOLFA14.EXE` (231 KB), `build/wolfvis_a14.iso` (1.16 MB).
- New: `snap/vis/0006.png`, `0007.png` (sprites-in-world PoC validation in E1L1).
- README status table: A.14 ✅. Quick-start build/launch commands updated to reference A.14 binaries.

### Next-step candidates for Session 10

1. **A.14.1 — Door rendering + door-open AI.** Real door swing animation, slide-in-frame visual, blocking state machine. Removes the cosmetic "walking through walls" glitch from A.14. Reuses asset path: door textures are already in VSWAP at known indices. ~45-60 min.
2. **A.13.1 — Raycaster polish.** Grid-line DDA proper (replace step-by-fraction; sub-pixel-exact texture coords; cheaper). Light-by-distance Wolf3D palette ramp. ~45-60 min.
3. **A.15 — HUD / status bar.** BJ face, ammo, score, key icons. Reuses A.5 sprite blits + a tiny 4×6 number font. The first time the screen looks like a *game* with chrome around the viewport. ~1 h.
4. **A.16 — Dynamic enemies.** Standing guards (obj 108..115) and patrol guards (116..127) with simple state, walk cycles, hit/death frames. Significantly larger scope — sprites with N animation frames per state, AI ticker, line-of-sight check. ~2-3 h.
5. **A.10.1 reopen.** IMF stacchi — start from hypothesis E (other music chunks).

S9 wrap recommendation: A.14.1 next (closes the cosmetic glitch this milestone ships with), then A.15 HUD for chrome polish before A.16 enemies. The "PoC playable demo" target is now architecturally one milestone away: doors that open + ammo/health box + at least one enemy that reacts.

### S9 wrap-up

Single milestone, single sitting, two-iteration recovery (huge qualifier + door movement split). The PoC viewport now shows the recognizable Wolfenstein 3D look — wall textures, decoration sprites at correct depth and scale, occlusion working both ways (walls hide far sprites, near sprites paint over far). Foundation chain A.1..A.13 paid off with literally one new function per added subsystem: ScanObjects (~30 lines), DrawSpriteWorld (~80 lines), DrawAllSprites (~40 lines), and a 4-line z-buffer write inside DrawWallStripCol. The "raycaster gentle" rule's structural prediction held all the way to A.14: no architectural rework, no new BSS pattern (just the `__huge` qualifier upgrade), no MAME-side regression on any prior subsystem.

Workflow rule re-confirmed: code + VIS_sessions.md + README.md in the same commit. Push to origin/main after commit per S9 user request.

---
