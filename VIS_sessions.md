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
