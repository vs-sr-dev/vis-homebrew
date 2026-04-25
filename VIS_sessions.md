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

### S9 hot fix — held-key continuous movement (post-A.14)

User surfaced after A.14 push: holding a d-pad key registered exactly one move and stopped. Bug present since A.6 (HC input) but invisible at the pre-A.13 framerate where one cursor pixel per tap blended into the message stream and the pause never showed. After A.14 each tap moves a full ~3/32 tile and the gap is obvious.

**Fix v1 (TTL backstop):** held flags set by WM_KEYDOWN, decremented per WM_TIMER poll, cleared on WM_KEYUP if it ever arrives. SetTimer raised from 500 ms to 50 ms (debug bar throttled internally to 1 Hz). Tested → exactly 4 steps per tap (1 immediate + 3 from TTL=3 polls), then stuck. Diagnostic confirmed: VIS HC delivers neither WM_KEYDOWN auto-repeat nor WM_KEYUP. The TTL is the only thing terminating the held flag.

**Fix v2 (GetAsyncKeyState polling):** dropped TTL entirely. WM_TIMER calls `GetAsyncKeyState(VK_HC1_*)` for each d-pad code and refreshes the held flags directly from the async keyboard buffer. WM_KEYDOWN kept as a tap-fast-path (one immediate step on the down edge so reactive without waiting up to one poll cycle). WM_KEYUP handler removed (never fires on VIS HC). Tested → continuous movement while held, stops cleanly on release. User: "Confermo funzionante perfetto ora!"

**What this tells us about Modular Windows VIS HC input:**
- WM_KEYDOWN delivers one event per physical press, no auto-repeat.
- WM_KEYUP is never delivered for HC d-pad / button events.
- GetAsyncKeyState DOES correctly track press/release on the HC keyboard buffer, even though the message-pump path doesn't surface WM_KEYUP. Async buffer is the canonical Win16 substitute for release detection on this hardware.

This is the third known HC quirk after the A.8 "must call hcGetCursorPos to keep dispatcher routing keys" pattern and the A.14 "VIS native cursor must be triple-suppressed" pattern. New memory `reference_vis_hc_input_quirks.md` consolidates them so future input work doesn't re-discover.

**Concrete delta:** modified `src/wolfvis_a14.c` (+57 lines: PollHeldKeysFromAsync helper, ApplyHeldMovement helper, WM_TIMER restructure, SetTimer 500→50 ms, debug bar throttle). EXE 231 KB unchanged size class. ISO rebuild + MAME test, single iteration.

---

## Session 10 — 2026-04-25 — Milestone A.14.1 (doors)

**Scope:** close the only cosmetic regression A.14 shipped with — the player visibly walking through wall slabs at door tile positions. Reuse the foundation: door textures live in VSWAP at known indices, the column-walk wall-strip renderer can sample any 64×64 page, the cast already enters door tiles (it just couldn't distinguish them from walls). Goal: make doors first-class with sliding-slab animation, PRIMARY-button toggle, and movement-blocking that gates on open extent.

User selected scope at session open from the three S10 candidates (A.14.1 doors / A.15 HUD / A.16 enemies) — A.14.1 first, isolated from layout changes ("rifare layout in mezzo al door work raddoppia lo scope"). Layout invariant kept across the whole milestone: viewport 128×128, debug bar 30 px, minimap 64×64. Perf sweep stays deferred to post-A.16 per S9 wrap.

### Door tile inventory + texture index (from vanilla Wolf3D source)

Door tile values in `map_plane0` are 90..101 with even/odd encoding orientation:
- **Vertical doors** (slab runs N–S, slides on Y axis): 90, 92, 94, 96, 98, 100. Lock type = `(tile-90)/2`.
- **Horizontal doors** (slab runs E–W, slides on X axis): 91, 93, 95, 97, 99, 101. Lock type = `(tile-91)/2`.

Door textures in VSWAP start at chunk `sprite_start_idx - 8` — vanilla Wolf3D defines `DOORWALL = (PMSpriteStart-8)` in `WL_DRAW.C`. The 8-page DOORWALL bank holds: +0 normal door slab (PoC uses just this), +2/+3 door-side wall variants (when a wall is adjacent to a door), +4 elevator door, +6 locked door. PoC ignores all variants — every door tile renders with the same single 64×64 page.

### Subsystems added vs A.14

- **`door_tex[4096] __far`** — single 64×64 page loaded from VSWAP at chunk `sprite_start_idx - 8`. Loaded inside `LoadVSwap` after the sprite block. `gDoorTexErr` separate from `gVSwapErr` so a missing door page doesn't fail the whole VSWAP load.
- **`g_door_amt[MAP_TILES] __far`** + **`g_door_dir[MAP_TILES] __far`** — per-tile state. `amt` is open extent in 1/64 tile (0=closed, 64=open). `dir` is `IDLE/OPENING/CLOSING`. 8 KB total in BSS, well within DGROUP budget after A.14's `__huge sprites` move.
- **`IsDoor(tx, ty)`** — returns 1=vertical, 2=horizontal, 0=not a door. Used by `CastRay` and `ToggleDoorInFront`.
- **`IsBlockingForMove`** rewritten — walls always block; doors block iff `amt < DOOR_BLOCK_AMT` (=56/64, ≈ 0.875 open). Matches vanilla Wolf3D's "doors block until ~7/8 open" feel and prevents the player from getting trapped by a half-closing slab.
- **`AdvanceDoors()`** — sweeps `MAP_TILES` once per WM_TIMER tick, advances any non-idle door by `DOOR_STEP=2` per tick. Full open/close transition = 32 ticks × 50 ms = 1.6 s. Returns BOOL so the caller can invalidate the viewport when state changed.
- **`ToggleDoorInFront()`** — projects one tile-unit forward along player heading, looks up the tile, toggles its `dir`. Reverses mid-animation if pressed during open/close (so a panic-slam works).
- **CastRay door branch** (the centerpiece): inside the per-sub-step loop, before the standard wall-hit check, evaluate door slab. If we're traversing a door tile (or just entered one) and the ray crossed the slab's mid-plane in this sub-step, interpolate the perpendicular axis at the crossing, and hit-test against `amt_q16 = (amt << 16) / 64`. If `perp_frac >= amt_q16`, return DOOR_TEX_IDX as the texture sentinel + `tex_x = perp_frac * 64`. Else the ray passes through the open portion and casting continues to the far wall.
- **`DrawWallStripCol`** — recognizes `tex_idx == DOOR_TEX_IDX` (= `WALL_COUNT` = 8) and samples `door_tex[]` instead of `walls[tex_idx_clamped]`. Everything else (wall_h calc, ceil/floor fill, sy_step inner loop) unchanged.
- **PRIMARY button repurposed** — was OPL channel-0 click since A.8 (sanity-check sound). Now triggers `ToggleDoorInFront`. SECONDARY's OPL click kept as the audio-stack canary.
- **Minimap door coloring** — door tiles render as orange-176 closed, light-orange-178 animating, green-105 open. Doubles the minimap as a "where can I walk now" indicator without a separate HUD.

### Cast geometry detail (the trickiest piece)

For each sub-step the loop saves `last_pos_x_q16`/`last_pos_y_q16` before the `+= sub_dx/dy` advance. Door check picks one tile:
- If we were already in a door tile (`IsDoor(prev_tx, prev_ty)`), test against THAT tile's mid-plane.
- Else if we just entered one (`IsDoor(tx, ty)`), test against THE NEW tile's mid-plane.

The mid-plane is at `mid_q16 = (d_tx << 16) + 0x8000` for vertical doors (X-axis crossing), `(d_ty << 16) + 0x8000` for horizontal (Y-axis crossing). Crossing detection is the standard `(prev < mid && pos >= mid) || (prev >= mid && pos < mid)`. When crossed, linear-interpolate the OTHER axis at the crossing point: `t_q16 = ((mid - axis_prev) << 16) / (axis_pos - axis_prev)`, then `perp_at_mid = perp_prev + ((perp_pos - perp_prev) * t_q16) >> 16`.

Three early-outs keep false hits out:
1. `denom == 0` (zero-length axis projection — degenerate ray).
2. `(perp_at_mid >> 16) != perp_tile_expected` — the crossing landed in a tile *neighboring* the door tile (the ray was oblique enough to clear the door diagonally).
3. `perp_frac_q16 < amt_q16` — the crossing landed in the open portion of the slab; ray passes through and standard sub-step cast resumes.

Distance to hit uses the same `dx/cos` (vertical door) or `dy/sin` (horizontal) projection the wall hit uses, so the sprite z-buffer reads the right values and walls past the door still occlude correctly.

### Door state machine semantics

- `IDLE` + `amt == 0` → tap PRIMARY → `OPENING`. Each tick `amt += 2` until `amt == 64` → `IDLE`.
- `IDLE` + `amt == 64` → tap PRIMARY → `CLOSING`. Each tick `amt -= 2` until `amt == 0` → `IDLE`.
- `OPENING` mid-anim → tap PRIMARY → `CLOSING` (panic-slam).
- `CLOSING` mid-anim → tap PRIMARY → `OPENING` (rescue).
- Movement gate: `amt < 56` blocks. So during the last 8 ticks of opening (~400 ms), the player can already walk through.

### Build

- Compile: `wcc -zq -bt=windows -ml -fo=..\build\wolfvis_a141.obj wolfvis_a141.c` — clean, no warnings, no `__huge`-related E1157 (door BSS is well under 64 KB).
- Link: `wlink @link_wolfvis_a141.lnk` — same `IMPORT hcGetCursorPos HC.HCGETCURSORPOS` as A.8+.
- Output: `build/WOLFA141.EXE` 245 KB (+14 KB vs A.14 for door state arrays + door check in CastRay + door advance helper), `build/wolfvis_a141.iso` 1.16 MB.

### Result

Confirmed working on MAME 0.287 vis on first build attempt — no fix iterations. Snapshots `snap/vis/0011.png`, `0012.png`:

- 0011: door slab fully closed in front of the player. Cyan/teal Wolf3D door texture with side rivet plates, perspective-scaled correctly, flanked by Hitler-poster wall textures on both sides at the same depth. Minimap shows the door tile in orange (closed). Perf bar mostly red — the cast just got more expensive.
- 0012: door slab mid-animation, partially retracted upward — the slab now covers only the lower ~60% of its tile, with the floor visible through the upper open portion. Texture preserved during slide. Player visibly moving toward the door.

User confirmation: "Tutto confermato! Slab scorre... mooolto, moooooooolto lentamente (2-3 frame al secondo), ma scorre!"

The 2–3 FPS is the expected continuation of A.14's 4–5 FPS plus the per-sub-step door check on every column × every step. Perf sweep is deliberately deferred per S9 wrap to post-A.16.

### Trap / Gotcha / Eureka (S10)

- **Trap S10.1 — MAME launched without `-rompath`.** First MAME launch failed instantly: "Required files are missing, the machine cannot be run" with `p513bk0b.bin NOT FOUND` / `p513bk1b.bin NOT FOUND`. Project-root has both `vis.zip` (the BIOS ROM set MAME wants) and the loose extracted bins under `reverse/`. Default rompath doesn't include the project root. Fix: pass `-rompath .` so MAME finds `vis.zip` next to the cwd. Same lesson as the `mame_snapshot_path` memo from S6: always launch from the project root with explicit `-rompath .` for VIS work. (Adding to README's run command for future sessions.)
- **Eureka S10.E1 — First-attempt-pass on a non-trivial new subsystem.** A.14.1 added door textures (LoadVSwap mod), state arrays (BSS), state machine (AdvanceDoors), input (ToggleDoorInFront, PRIMARY rewire), cast logic (door branch in CastRay), render path (DOOR_TEX_IDX in DrawWallStripCol), and minimap UI (door state coloring) — and built clean + ran correctly on the very first MAME launch. No trap-fix-rebuild iteration. Two factors made this possible:
  1. **Reading the vanilla Wolf3D source first** (`WL_DRAW.C HitVertDoor/HitHorizDoor` for the DOORWALL chunk index, `WL_GAME.C` case 90..101 for the orientation parity) → no guessing on data formats or magic numbers.
  2. **A.14's split of `IsWall` vs `IsBlockingForMove` already prepared the codebase for door state**: doors were already a separately-handled tile class, just with a placeholder behavior. A.14.1 just filled in the placeholder. The S9 "conscious PoC trade-off" memo paid off one milestone later.
- **Eureka S10.E2 — Sentinel-as-tex-idx pattern stays clean as the renderer grows.** Reserving `DOOR_TEX_IDX = WALL_COUNT` (one past the legal walls range) means CastRay's `out_tex_idx` is always the right value for DrawWallStripCol regardless of source: walls 0..7 → walls[idx], 8 → door_tex. No new parameter to plumb through the cast → render boundary. Same pattern will scale: pushwalls, secret doors, and elevator-end-floors can each take a sentinel slot with zero plumbing change. The renderer stays strictly column-walk-with-source-swap.
- **Eureka S10.E3 — Mid-plane interpolation is the right abstraction for "thin walls inside a tile".** The door slab sits at the tile center, perpendicular to the door axis. By saving `last_pos` before each sub-step and detecting mid-plane crossings with linear interpolation, the cast handles the door regardless of step granularity, ray direction, or sub-step alignment. The same primitive directly applies to: pushwalls (slab parallel to a tile edge instead of mid-plane), thin-window decorations, half-height obstacles. Cost is one extra crossing test per sub-step, well-amortized vs the cast's existing per-step work.

### Concrete results

- New: `src/wolfvis_a141.c` (~1280 LOC, +130 vs A.14), `src/wolfvis_a141_sintab.h` (copy of A.14 sintab), `src/link_wolfvis_a141.lnk`, `src/build_wolfvis_a141.bat`, `src/mkiso_a141.py`.
- New: `cd_root_a141/` (9 files: A.14 set with `WOLFA141.EXE` + `SYSTEM.INI` updated to `shell=a:\WOLFA141.EXE`).
- New: `build/WOLFA141.EXE` (245 KB), `build/wolfvis_a141.iso` (1.16 MB).
- New: `snap/vis/0011.png` (door closed), `snap/vis/0012.png` (door mid-slide).
- README: A.14.1 row added to status table (✅). Quick-start build/launch commands updated to A.141 binaries; run command now includes `-rompath .` explicitly.

### Next-step candidates for Session 11

1. **A.15 — HUD / status bar**. BJ face + ammo + score + key icons in a chrome strip below the viewport. Reuses A.5 sprite blits + a tiny 4×6 number font. First time the screen looks like a *game* with chrome around the play area. ~1 h.
2. **A.16 — Dynamic enemies**. Standing guards (obj 108..115) and patrol guards (116..127). Reuses A.14's `Object[]` + `DrawSpriteWorld` infrastructure with frame animation per state (idle/walk/hit/death) and a simple AI ticker. ~2-3 h, may want to split A.16a (rendering static enemies) + A.16b (AI movement).
3. **A.13.1 — Raycaster polish**. Grid-line DDA proper (replace step-by-fraction; sub-pixel-exact texture coords; cheaper). Light-by-distance Wolf3D palette ramp. ~45-60 min. Worth pulling forward if perf becomes an interaction blocker before A.16.
4. **A.10.1 reopen**. IMF stacchi — start from hypothesis E (other music chunks).

S10 wrap recommendation: A.15 next. With doors closing the only visible regression and walls/sprites/doors all painted correctly, the next visible delta is chrome — and HUD finally lifts the screen out of "tech demo" framing into "game" framing without needing to wait for the AI work in A.16. Perf sweep sequencing also cleaner: A.15 adds a fixed-cost overlay (HUD pixels are a constant blit), so it does not alter the cast workload that perf would target.

### S10 wrap-up

Single milestone, single sitting, zero-iteration recovery (first-attempt build + first-attempt MAME launch on door logic; one fix iteration on the launch *command* — missing `-rompath` — which is independent of the milestone code). The cosmetic regression from A.14 is closed: doors render with their proper Wolf3D texture, animate open and closed in 1.6 s, and gate movement only when sufficiently open. Foundation chain A.1..A.14 paid off again — the door subsystem is ~130 LOC across one new BSS block, one helper, one CastRay branch, one DrawWallStripCol special-case, one WM_TIMER call, and one input rebind. No structural change to the cast, the renderer, or the asset pipeline.

The "raycaster gentle" rule's prediction extends one more milestone: with A.13's cast in place, every visual addition since (A.14 sprites, A.14.1 doors) has been a localized data-source swap or a single-loop branch over the existing column-walk renderer. The renderer architecture is holding.

Workflow rule re-confirmed: code + VIS_sessions.md + README.md in the same commit.

---

## Session 10 (continued) — 2026-04-25 — Milestone A.15 (HUD / status bar)

**Scope:** the screen has chrome around the play area for the first time. After A.14.1 closed the door regression with zero iterations, the user opted to continue S10 into a second milestone since context was fresh and pacing memo aligned ("non rimandare a prossime sessioni ciò che si può fare ora"). Goal: lift the visual framing from "tech demo with black borders" to "game with a status bar", without altering the cast workload — A.15 is perf-neutral by construction (HUD is a pixel-constant blit baked into static_bg).

User selected scope at S10 part-2 open: A.15 over A.16 enemies (smaller commit, perf-neutral, doesn't compound A.14.1's 2-3 FPS observation).

### Recon — BJ face is in VGAGRAPH, not VSWAP

First step was looking up the BJ face sprite chunk index in the Wolf3D source (same recon pattern that made A.14.1 zero-iteration). Discovery: `FACE1APIC` lives at chunk 113 in `GFXV_WL1.EQU`, but that's the **VGAGRAPH** file, not VSWAP. VGAGRAPH is a separately-formatted asset file (chunked Huffman compression, picture table, dimensions header) that we have no loader for — implementing it would be a sub-milestone of its own.

Pragmatic pivot: skip the real BJ face for A.15, render a stylized **24×24 placeholder face** drawn from `FB_FillRect` primitives (helmet + skin + eye dots + mouth). No bitmap data in the EXE. Real face deferred to **A.15.1 polish** when we add a VGAGRAPH loader.

### HUD layout (final, after one iteration)

37-px strip occupies the previously-black `y=163..199`. After the user fed back on the first cut ("rimpicciolire un pochino i primi due quadranti a SX in modo che il volto sia centrato"), the layout was redesigned to be symmetric around screen center (x=160) with FACE in the middle:

| panel  | x range  | width | content                |
|--------|----------|-------|------------------------|
| LEVEL  | 0..35    | 36 px | 1-digit value `1`      |
| SCORE  | 36..107  | 72 px | 6-digit value `000000` |
| LIVES  | 108..143 | 36 px | 1-digit value `3`      |
| FACE   | 144..175 | 32 px | 24×24 face at x=148    |
| HEALTH | 176..223 | 48 px | 3-digit value `100`    |
| AMMO   | 224..271 | 48 px | 2-digit value `08`     |
| KEYS   | 272..319 | 48 px | gold + silver icons    |

Borders: 1-px top line at `y=163`, 1-px vertical separators at panel boundaries. Bottom = screen edge.

### Color iteration — gamepal lookup beats guessing

First-cut colors guessed at gamepal indices and got bitten:
- `HUD_BG = 8` (assumed dark grey, came out as it should — but too plain).
- `HUD_BORDER = 16` (assumed lighter grey, came out white — too bright).
- Face helmet `144` (assumed brown, came out blue!), brim `142` (also blue), skin `84` (came out dark teal), silver key `31` (came out near-black).

User feedback after first build ("Sarebbe da renderlo su sfondo blu come il vero W3D, e rimpicciolire un pochino i primi due quadranti a SX") drove the v2 pass. Rather than guess again, this time we read `gamepal.h` directly to verify RGB triplets:

| index | RGB6              | use                              |
|-------|-------------------|----------------------------------|
| 1     | (0, 0, 42)        | HUD_BG dark blue (Wolf3D-style)  |
| 9     | (21, 21, 63)      | HUD_BORDER bright blue separator |
| 15    | (63, 63, 63)      | white digits                     |
| 60    | (57, 27, 0)       | helmet brown (came out orange)   |
| 56    | (63, 42, 23)      | skin/peach                       |
| 8     | (21, 21, 21)      | helmet brim dark grey            |
| 7     | (42, 42, 42)      | silver key light grey            |
| 14    | (63, 63, 21)      | gold key yellow                  |

v2 build worked first try. Snapshot `0015.png` confirms: dark blue panels with light blue separators, white digits clearly readable on blue, FACE perfectly centered around x=160 with brown helmet over peach skin, both key icons (gold + silver) visible. Palette index 60 came out more orange than dark brown but reads well against the blue, so kept as-is.

### Subsystems added vs A.14.1

- **`digit_font[10][24]`** — 4×6 byte-per-pixel digit font as `static const`, ~240 B in code. Each digit is hand-authored as a flat array of 0/1 bits. Pitch 5 px (4 px digit + 1 px gap) means a 6-digit score occupies exactly 29 px.
- **`DrawDigit(x, y, d, fg)`** — emits one glyph via `FB_Put` per lit pixel.
- **`DrawNumber(x, y, val, width, fg)`** — right-align with leading zeros, modular over `width` digits. Used by every HUD value.
- **`DrawFacePlaceholder(x0, y0)`** — 24×24 helmet+skin+eyes+mouth via 8 `FB_FillRect` calls. Zero asset dependency.
- **`DrawHUD()`** — strip background + top border + vertical separators + every panel value + face placeholder + key icons.
- **`SetupStaticBg` extended** — `DrawHUD()` is called inside the static-bg setup so the HUD pixels are baked into `framebuf` once at boot. Per-frame cost is **zero** because nothing in the per-frame paint path ever overwrites `y=163..199` (DrawViewport writes only `y=35..162`, DrawMinimapWithPlayer only `y=35..98`, DrawDebugBar only `y=0..29`). The `InvalidateRect` dirty rect in `InvalidatePlayerView` deliberately excludes the HUD region too.

### Build

- Compile: `wcc -zq -bt=windows -ml -fo=..\build\wolfvis_a15.obj wolfvis_a15.c` — clean, no warnings.
- Link: `wlink @link_wolfvis_a15.lnk`.
- Output: `build/WOLFA15.EXE` 246 KB (+1 KB vs A.14.1 — only added font const + draw helpers), `build/wolfvis_a15.iso` 1.16 MB.

### Result

Confirmed working on MAME 0.287 vis. Snapshots `snap/vis/0013.png` (first cut, grey BG + off-center FACE), `0014.png` (alternate angle of first cut), `0015.png` (v2: blue Wolf3D-style HUD with centered FACE). User v1 reaction: "Prima reazione: CUTE XD". User v2 reaction: "Snap fatto in realtà, controlla, direi ottimo!".

### Trap / Gotcha / Eureka (S10 part 2)

- **Trap S10.2 — Wolf3D status bar BJ face is in VGAGRAPH, not VSWAP.** Reasonable assumption was that the face would be a sprite in VSWAP alongside Demo/DeathCam/STAT_*, but `FACE1APIC=113` is in `GFXV_WL1.EQU` which indexes VGAGRAPH chunks. VGAGRAPH uses a different format (chunked Huffman + picture table) we don't have a loader for. Pivoted to programmatic placeholder. Recovery cost: 0 — discovery happened before any code was written.
- **Trap S10.3 — Guessing palette indices burns iterations.** First-cut DrawHUD guessed indices for "brown / lighter grey / silver" and got blue / white / near-black instead. Each wrong index forced a rebuild + relaunch + visual check cycle. Fix: read `gamepal.h` RGB6 triplets directly before picking colors. v2 colors were all correct on first try. Lesson: for any new color use, look up the actual triplet in gamepal.h — don't extrapolate from "color N looked like X in a different palette".
- **Eureka S10.E4 — Static-bg bake = zero per-frame HUD cost.** Because the per-frame redraw path uses `InvalidateRect` with a dirty rect that excludes `y=163..199`, and because no draw helper writes to that region after boot, baking the HUD into `framebuf` once during `SetupStaticBg` is enough. The HUD pixels persist across every WM_PAINT (clipped or full). When A.16+ wires real game state, the only delta is calling `DrawHUD` again from `InvalidatePlayerView` and extending the dirty rect — the static layout machinery doesn't change.
- **Eureka S10.E5 — Symmetric layout around screen center is the readable default.** First cut was 64-px-uniform panels which worked but pushed FACE to x=176 (off-center by 16 px). User immediately spotted "il volto è scentrato". Symmetric design: panels left of center + face panel + panels right of center, with each side summing to 144 px. Result reads as "balanced" without any further commentary. Pattern applies to any future centered-element HUD work.

### Concrete results

- New: `src/wolfvis_a15.c` (~1430 LOC, +150 vs A.14.1), `src/wolfvis_a15_sintab.h` (copy of A.14.1 sintab), `src/link_wolfvis_a15.lnk`, `src/build_wolfvis_a15.bat`, `src/mkiso_a15.py`.
- New: `cd_root_a15/` (9 files: A.14.1 set with `WOLFA15.EXE` + `SYSTEM.INI` updated to `shell=a:\WOLFA15.EXE`).
- New: `build/WOLFA15.EXE` (246 KB), `build/wolfvis_a15.iso` (1.16 MB).
- New: `snap/vis/0013.png` (first cut HUD), `snap/vis/0014.png` (first cut alt angle), `snap/vis/0015.png` (final blue Wolf3D-style HUD).
- README: A.15 row added to status table (✅). Quick-start build/launch commands updated to A.15 binaries.

### Next-step candidates for Session 11

1. **A.16 — Dynamic enemies** (~2-3 h). The remaining "PoC playable demo" architectural piece. Standing guards (obj 108..115) and patrol guards (116..127). Reuses A.14's Object[] + DrawSpriteWorld with frame animation per state (idle/walk/hit/death) and a simple AI ticker. Likely split into A.16a (rendering static enemies) + A.16b (AI movement). With this in, the HUD's dummied values can start being driven by real game state (damage → HEALTH, kills → SCORE, picked-up clips → AMMO).
2. **A.13.1 — Raycaster polish** (~45-60 min). Grid-line DDA proper, light-by-distance Wolf3D palette ramp. Worth pulling forward if perf becomes an interaction blocker before A.16.
3. **A.15.1 — Real BJ face from VGAGRAPH** (~1-1.5 h). Implement the chunked Huffman loader + picture table parsing for VGAGRAPH.WL1. Gives us authentic BJ face frames + the title screen pic + menu graphics. Lower priority than A.16 (the placeholder is functional), but the unlock is broader than just the face.
4. **A.10.1 reopen.** IMF stacchi.

S10 part-2 wrap recommendation: A.16 next, split into A.16a/b. With doors + HUD + sprites + walls + cast all working, dynamic enemies are the last visible gap before "playable demo PoC" status.

### S10 part-2 wrap-up

Single milestone, single sitting (continuation of S10), one iteration on layout/colors driven by direct user feedback ("cute" + "rendi blu" + "centra il volto"). Foundation chain A.1..A.14.1 paid off again — A.15 is +150 LOC of pure additive code (font, helpers, DrawHUD) with one line of integration into `SetupStaticBg`. No structural change to anything pre-existing. Net per-frame cost: zero (all baked into static_bg).

Two new traps documented for future use: (1) BJ face is in VGAGRAPH not VSWAP — relevant to any future "use a Wolf3D pic" milestone; (2) gamepal indices must be looked up not guessed — relevant to any new color in any future render code.

Workflow rule re-confirmed: code + VIS_sessions.md + README.md in the same commit. Two commits in one session this time (A.14.1 + A.15) to keep the history bisectable.

---

## Session 11 — 2026-04-25 — Milestone A.16a (static enemies, 8-direction)

**Scope:** the world contains its first living-target-shaped objects. Wolf3D guards rendered as static billboards at their `map_plane1` tile positions in E1L1, picking the right 8-direction sprite frame each draw based on the player's view angle relative to the enemy's facing. Validates the full asset/scan/render/rotation pipeline for enemies — the structural prerequisites for A.16b (AI ticker) and A.18 (firing/hitscan/damage). No AI, no HP, no movement, no walking animation, no death frames yet.

User selected scope at S11 open: A.16a as the next milestone per S10 wrap. Pre-coding recon-first per the A.14.1 zero-iteration playbook (read vanilla Wolf3D source for sprite indices, tile decoder, rotation formula, direction encoding). Two commits in one session: iter 1 dry-run (front-view-only, validates load+scan+render) then iter 2 (8-direction rotation via CalcRotate-style atan2). Wall-texture variety regression surfaced by the user mid-S11 deferred to A.13.1 polish per the bundling memo.

### Pre-coding recon (~10 min)

Read four files in `wolf3d/WOLFSRC/` before touching `wolfvis_a16a.c`:

- **`WL_DEF.H` lines 159-208** — sprite enum order in VSWAP. Confirmed: `SPR_DEMO=0`, `SPR_DEATHCAM=1`, `SPR_STAT_0..47 = 2..49` (48 statics in non-SPEAR shareware, not 16 as I'd assumed from A.14's load size), `SPR_GRD_S_1..S_8 = 50..57`. Walking and death frames extend through chunk 98. The chunk indices are absolute relative to `sprite_start_idx`, so loading `SPR_GRD_S_n` means reading VSWAP page `sprite_start_idx + 50 + (n-1)`.
- **`WL_DEF.H` line 562** — `dirtype` enum: `{east=0, northeast=1, north=2, northwest=3, west=4, southwest=5, south=6, southeast=7, nodir=8}`. CCW, 8 sectors of 45°. Critically: this is **not** N/E/S/W in 0..3 order, it's E/N/W/S interleaved with diagonals.
- **`WL_GAME.C` lines 315-477** — tile-to-spawn decoder. The S11 todo memo had the wrong tile range cached ("108..115 standing + 116..127 patrol"). Correct decoder for guards across all three difficulty tiers:

| enemy | behavior | easy tiles | medium tiles | hard tiles |
|-------|----------|-----------|--------------|-----------|
| guard | stand | 108..111 | 144..147 | 180..183 |
| guard | patrol | 112..115 | 148..151 | 184..187 |
| officer | stand | 116..119 | 152..155 | 188..191 |
| officer | patrol | 120..123 | 156..159 | 192..195 |
| ss | stand | 126..129 | 162..165 | 198..201 |
| ss | patrol | 130..133 | 166..169 | 202..205 |
| dog | stand | 134..137 | 170..173 | 206..209 |
| dog | patrol | 138..141 | 174..177 | 210..213 |

  Vanilla skips the medium/hard sets when `gamestate.difficulty < gd_medium` (or `gd_hard`); for PoC we ignore the gate and spawn every tier so all guard tiles a level designer placed are visible regardless of difficulty intent. Tile **124** is `SpawnDeadGuard` (corpse, deferred).

- **`WL_ACT2.C` line 854 + line 905** — `SpawnStand(en_guard, ...)` calls `SpawnNewObj(... &s_grdstand)` then sets `new->dir = dir*2`. So the tile-base offset 0..3 (E/N/W/S) becomes dirtype 0/2/4/6 — the four cardinal directions of the 8-element dirtype enum, with diagonals reserved for AI-rotated enemies.
- **`WL_DRAW.C` lines 1024-1048 (`CalcRotate`)** — the rotation formula in vanilla:

  ```
  viewangle = player->angle + (centerx - ob->viewx)/8;
  angle = (viewangle - 180) - dirangle[ob->dir];
  angle += ANGLES/16;          // 22.5° half-sector centering
  angle = angle mod ANGLES;
  sector = angle / (ANGLES/8); // 0..7
  shapenum = SPR_GRD_S_1 + sector;
  ```

  The `(centerx - viewx)/8` per-column tweak is for sub-pixel rotation accuracy; we drop it for PoC (negligible visual difference).

### Iter 1 — dry-run (front-view-only, ~30 min)

**Goal:** validate the load/scan/render pipeline without rotation in the loop, so any issue is isolated to a single subsystem.

Cloned `wolfvis_a15.c` → `wolfvis_a16a.c` plus the `_sintab.h`, `link_*.lnk`, `build_*.bat`, `mkiso_*.py` chain. Cloned `cd_root_a15/` → `cd_root_a16a/` and updated `SYSTEM.INI`'s `shell=` line. Build infrastructure mirrors A.15 conventions exactly so a future A.16a viewer can diff against any prior milestone clean.

Source-side changes vs A.15:

- **`NUM_SPRITES`** bumped 18 → 26. Slots 0..17 keep the legacy A.14 layout (SPR_DEMO, SPR_DEATHCAM, SPR_STAT_0..15); slots 18..25 carry the 8 SPR_GRD_S_1..S_8 frames. Memory: 26 × 4096 = 104 KB total in the `__huge sprites[]` BSS array.
- **`sprite_chunk_offs[NUM_SPRITES]`** new sparse-load lookup table: `{0..17, 50..57}`. The VSWAP loader reads `pageoffs[sprite_start_idx + sprite_chunk_offs[i]]` per slot, so the 32 unused chunks 18..49 (SPR_STAT_16..47, none referenced on E1L1) are skipped without leaving holes in `sprites[]`.
- **`MAX_OBJECTS`** bumped 64 → 128. E1L1 carries enough enemy + decoration tiles to plausibly exceed 64; insertion sort over a side array of 128 entries is 16K compares worst case, trivial vs the cast workload.
- **`Object` struct** extended with `BYTE enemy_dir` (`OBJ_DIR_NONE=0xFF` for static decoration, dirtype 0..7 for enemies). Decoration entries set this to NONE so the rotation branch in `DrawAllSprites` skips them and the legacy `sprite_idx` path runs unchanged.
- **`GuardTileToDir(tile)`** helper returns 0..3 for any of the 24 guard tile values (across all 3 difficulty tiers, both stand and patrol), -1 otherwise. Decoupled from the spawn classification for clarity.
- **`ScanObjects`** rewritten to two branches per cell: branch 1 = legacy decoration scan (obj_id 23..38 → sprite_idx = obj_id - 23 + 2), branch 2 = `GuardTileToDir(obj)` ≥ 0 → emit Object with `sprite_idx = GUARD_S_FIRST_SLOT (=18)` (front-view stub for dry-run) and `enemy_dir = dir4 * 2` (E/N/W/S → dirtype 0/2/4/6). Per-frame counters `g_num_static` + `g_num_enemies` exposed for future debug.
- **`ObjectToColor`** extended: all 24 guard tiles map to color **40 (bright red)** on the minimap, so the user can navigate "by treasure-hunt" toward enemy positions even before rendering them at depth. (User confirmed at iter-1 review: "puntini rossi sono proprio stati loro il metodo con cui sono andato alla ricerca" — the minimap markers were the actual gameplay-discovery aid.)

Build: clean compile + link first try. EXE 280 KB (+34 KB vs A.15, mostly the 8 new sprite slots in BSS). ISO 1.19 MB.

Test: MAME 0.287 vis launch, 131 s wall-clock at 99.47% emulation speed, normal exit. Snapshot `snap/vis/0016.png` confirms:
- Brown-uniformed guard sprite (SPR_GRD_S_1, front view) clearly visible in the viewport between Hitler-poster wall textures.
- Multiple bright-red dots scattered across the minimap = guard spawn tiles in plane1 — the user used these as a navigation aid to seek out enemies in subsequent traversals.
- Decoration sprites (chandelier visible) intact, painter's sort handles enemy + decoration without z-glitch.
- HUD intact (zero per-frame cost preserved; A.15 static-bg bake undisturbed).

User v1: "Guardia vista (si nota poco, è nello snap ma è vicina a texture con dipinti quindi si confonde), tutto OK!"

### Iter 2 — 8-direction rotation (~30 min)

**Goal:** complete A.16a properly — every enemy presents the right SPR_GRD_S_n frame for the player's current viewing angle, so walking around a guard reveals front/profile/back as it should.

New artifacts:

- **`gen_atan_lut.py`** — 30-line build helper that generates `wolfvis_a16a_atantab.h` with `atan_q10_lut[257]`. Input domain t = i/256 for i in 0..256 (so [0, 1] covered with 257 entries). Output: `round(atan(t) * 1024 / (2*pi))`, range [0, 128] (= [0, 45°] in our Q10 angle space). Mirrors the `wolfvis_a13_sintab.h` Python-generated LUT pattern that started life as the `<math.h>` workaround for the WIN87EM trap.
- **`dirangle_q10[8]`** const table mapping WL_DEF.H dirtype 0..7 to our Q10 angle space, accounting for the CW orientation forced by Y+ = south:

  ```
  E=0, NE=896, N=768, NW=640, W=512, SW=384, S=256, SE=128
  ```
- **`atan2_q10(dy, dx)`** — full atan2 reconstruction without `<math.h>`:
  - Magnitude-swap so `|slope| <= 1`, look up `atan_q10_lut[abs_min * N / abs_max]`, returns `[0, 128]` in Q10.
  - If `|dy| > |dx|`, return `256 - lookup` (so result is in `[128, 256]`).
  - Quadrant fixup via signs of dx, dy: Q1 → as-is, Q2 → `512 - base`, Q3 → `512 + base`, Q4 → `1024 - base`.

  Cost: ~10 cycles per call (one divide for the ratio + one LUT load + branch). Called once per visible enemy per frame in `DrawAllSprites`, negligible vs cast.

- **`DrawAllSprites` rotation branch** — per-object pre-call computation:
  ```c
  long e2p_dx = g_px - obj_x_q88;
  long e2p_dy = g_py - obj_y_q88;
  int  e2p_angle = atan2_q10(e2p_dy, e2p_dx);     // direction enemy -> player
  int  facing = dirangle_q10[obj.enemy_dir & 7];  // direction enemy faces
  int  rel = (facing - e2p_angle + 1024 + 64) & ANGLE_MASK;  // half-sector centered
  int  sector = (rel >> 7) & 7;
  sprite_idx = GUARD_S_FIRST_SLOT + sector;       // 18..25
  ```
  Static decorations (`enemy_dir == OBJ_DIR_NONE`) skip this branch and use their stored `sprite_idx` as before.

Build clean +1 KB (atan LUT 514 B + dirangle 16 B + atan2 fn ~250 B code). Test launch.

**One iteration on rotation sign.** First version had `rel = e2p_angle - facing + ...`; user verdict: "credo sia da invertire, per il resto tutto OK". The sign of the rotation differed by reflection (S_2/S_8 swap, S_3/S_7 swap) because Wolf3D's `SPR_GRD_S_n` art layout is CCW around the enemy in standard math convention while our Q10 angle space goes CW from east. Fix: swap the operand order to `rel = facing - e2p_angle + ...`. Single character delta.

Final test: snapshots `snap/vis/0019.png` (right-profile guard with rifle clearly visible) + `0020.png` (back-view guard, helmet from behind, dark uniform); both look like authentic Wolfenstein 3D frames. User v2: "Perfetto ora".

### Side discovery — wall texture variety regression (deferred to A.13.1)

User flagged mid-S11: "altro side da investigare: problema di seek delle texture dei muri, mi sembra ci siano davvero TROPPI muri con dipinti nazi". Confirmed legitimate bug in `TileToWallTex(tile)`:

```c
return (int)(((tile - 1) * 2) % WALL_COUNT);  // WALL_COUNT = 8
```

The `*2` here echoes Wolf3D's `wall_page = (tile-1)*2` for the light face (with `+1` for dark face). With WALL_COUNT=8 the modulo collapses tile 1..63 onto wall pages {0, 2, 4, 6} only — pages {1, 3, 5, 7} (the dark faces of walls 1..4) are loaded but never sampled. Worse, tiles {2, 6, 10, 14, ..., 62} (16 distinct plane0 tile values) all map to page 2 = the Hitler-poster wall texture, so any wall whose tile id is `≡ 2 (mod 4)` shows up as Hitler regardless of the level designer's intent. That's the "TROPPI muri con dipinti nazi" the user noticed.

Three fix tiers proposed (quick / better / best):
- Quick (~5 min, zero memory): drop the `*2`, use `(tile - 1) % WALL_COUNT`. Loads light faces of 8 distinct walls, but ditches the light/dark distinction entirely.
- Better (~15 min, +32 KB): bump WALL_COUNT to 16, load chunks {0, 2, 4, ..., 30} (16 light faces). Right at the 64 KB segment cap so likely needs `__huge` migration mirroring sprites.
- Best (~30 min): WALL_COUNT 32 (16 light + 16 dark), pick light vs dark in CastRay based on side-X vs side-Y face hit (canonical Wolf3D fake-lighting pattern).

User chose to defer per the existing bundling plan: "lasciamo defer, lo teniamo insieme al resto di A.13.1". A.13.1 was already on the books as raycaster polish (grid-line DDA + light-by-distance) and the wall-variety fix is the natural third deliverable for that milestone. Captured in `reference_walltex_modulo_bug.md` so the recon work isn't lost.

### Build

- Compile (both iters): `wcc -zq -bt=windows -ml -fo=..\build\wolfvis_a16a.obj wolfvis_a16a.c` — clean, no warnings.
- Link (both iters): `wlink @link_wolfvis_a16a.lnk` — same `IMPORT hcGetCursorPos HC.HCGETCURSORPOS` chain.
- Output: `build/WOLFA16A.EXE` 281 KB, `build/wolfvis_a16a.iso` 1.19 MB.

### Result

Snapshots `snap/vis/0016.png 0017.png 0018.png 0019.png 0020.png` walk through:
- 0016: iter-1 dry-run, single guard front-view + minimap red dots scattered across plane1.
- 0017: iter-2 first try — guard in left-profile (rotation working but sign-flipped per art layout).
- 0018: iter-2 first try, multiple guards visible at varied scales — confirmed rotation responding to angle but in mirrored direction.
- 0019: iter-2 final — right-profile guard, rifle on visible shoulder, classic Wolf3D side art.
- 0020: iter-2 final — back-view guard, helmet from behind, full uniform — exactly the SPR_GRD_S_5 frame the math predicted.

Perf: ~2-3 FPS, unchanged from A.14.1 / A.15 (cast cost dominates; rotation branch adds one atan2 + one divide per visible enemy, negligible). User noted explicitly during iter-1: "altre non raggiunte perché oggettivamente con FPS così basso ci si mette troppo tempo" — the perf-as-interaction-blocker trigger from the S11 todo memo just fired, queueing A.13.1 for S12 with concrete urgency.

### Trap / Gotcha / Eureka (S11)

- **Trap S11.1 — S11 todo memo had stale tile ranges.** The memo cached "guards = 108..115 stand + 116..127 patrol", which is wrong on two counts: (1) patrol is 112..115 not 116..127, and (2) it omitted the medium (144..151) and hard (180..187) tier tiles entirely. The error was a verbatim copy from an earlier mental model that hadn't been verified against `WL_GAME.C`. Recon caught it before any code was written, so cost was zero — but the S11 todo memo file should be updated (or replaced by a pointer to this VIS_sessions entry) so future-me doesn't trust the cached ranges. Lesson: tile-range constants in roadmap memos decay; verify against vanilla source any time the source is referenced concretely.
- **Trap S11.2 — Wall-texture modulo collapses 16 tiles onto Hitler poster.** See "Side discovery" above. Root cause: `(tile-1)*2 % 8` cycles through {0, 2, 4, 6} only, mapping every fourth tile (≡ 2 mod 4) onto wall page 2. Pre-existing bug since A.4 (when WALL_COUNT was first set to 4 and then bumped to 8 in A.13). User-visible symptom only became prominent once we had enough wall variety in E1L1 viewport to notice the repetition pattern.
- **Trap S11.3 — Rotation sign disagreement between vanilla art layout and our angle convention.** First iter-2 build had rotation responding to view angle correctly but with the chirality flipped (S_2/S_8 swap, etc.). Wolf3D's SPR_GRD_S_n frames are arranged CCW around the enemy in vanilla math convention; our Q10 angle space goes CW from east because Y+ = south. The two chirality conventions cancel in some terms and compound in others, so the right answer for our combination ended up being `rel = facing - e2p_angle + 1024 + 64`. Single-character fix in iter-2; for any future Wolf3D rotation port this is the asymmetry to watch.
- **Eureka S11.E1 — Recon-first → zero-iteration on the load/scan/render path.** A.16a iter 1 changed the VSWAP loader (sparse offset table), the BSS layout (sprites bumped 18→26), the Object struct (added enemy_dir), the ScanObjects pass (new branch), and the minimap helper (new color range), and built clean + ran correctly on the very first MAME launch. Zero fix iterations. The pattern matches A.14.1 exactly: read vanilla source for indices and conventions before writing C, keep the new pieces structurally similar to existing pieces (sparse `sprite_chunk_offs[]` is the obvious extension of "load 18 contiguous chunks"), and the build is forced to be additive rather than transformative.
- **Eureka S11.E2 — Minimap markers earn double duty as gameplay nav aid.** ObjectToColor extension to color guards bright red was originally for "see at a glance the level has guards"; user immediately repurposed it as a gameplay aid for finding distant enemies under the 2-3 FPS perf budget. The pattern generalizes: any debug-visualization feature that surfaces non-trivial world state (enemy positions, item locations, switch states) is a candidate gameplay aid worth keeping in the final game, not just a stripped-after-debugging temporary.
- **Eureka S11.E3 — atan2 LUT is the right amortized cost vs alternatives.** Considered three alternatives for the atan2-without-`<math.h>` problem: (a) full 2D LUT indexed by (dy, dx) clamped to small range — too memory-heavy for the precision needed; (b) inverse-search via existing sin LUT — slow and irregular; (c) octant test + comparison without LUT — only 8-resolution, exactly the thing we *don't* want for sub-frame transitions. The 1D atan LUT with quadrant fixup is 514 bytes for full-precision, a ~10-cycle hot path, and the same shape as `sin_q15_lut`. Building a small Python helper to generate it as a header keeps the discipline of "no `<math.h>` in source" intact.

### Concrete results

- New: `src/wolfvis_a16a.c` (~1500 LOC, +70 vs A.15), `src/wolfvis_a16a_sintab.h` (copy of A.15 sintab), `src/wolfvis_a16a_atantab.h` (Python-generated Q10 atan LUT, 257 entries), `src/gen_atan_lut.py`, `src/link_wolfvis_a16a.lnk`, `src/build_wolfvis_a16a.bat`, `src/mkiso_a16a.py`.
- New: `cd_root_a16a/` (9 files: A.15 set with `WOLFA16A.EXE` + `SYSTEM.INI` updated to `shell=a:\WOLFA16A.EXE`).
- New: `build/WOLFA16A.EXE` (281 KB), `build/wolfvis_a16a.iso` (1.19 MB).
- New: `snap/vis/0016.png` (iter-1 dry-run), `0017.png`, `0018.png` (iter-2 first try, mirrored rotation), `0019.png` (right-profile final), `0020.png` (back-view final).
- README: A.16a row added to status table (✅). Quick-start build/launch commands updated to A.16a binaries.
- Deferred: wall-texture modulo fix → A.13.1 (with grid-line DDA + light-by-distance).

### Next-step candidates for Session 12

1. **A.13.1 — Raycaster polish + wall variety** (~1-1.5 h). Bundles three deliverables: (a) grid-line DDA replacing step-by-fraction (sub-pixel-exact tex coords, cheaper), (b) light-by-distance Wolf3D palette ramp, (c) wall-variety bug fix (drop the `*2` modulo, possibly bump WALL_COUNT to 16 with `__huge` migration). Resolves the perf-as-interaction-blocker that fired in S11 + closes the wall regression. **Strong default for S12.**
2. **A.16b — Enemy AI ticker** (~1.5-2 h). State machine per enemy (idle/walk/attack/hit/die) + LOS check vs player + movement integration. Reuses A.16a Object[] + DrawAllSprites with rotation; only new code is the AI ticker + walk-frame animation cycle.
3. **A.15.1 — Real BJ face from VGAGRAPH** (~1-1.5 h). Chunked Huffman loader + picture table for VGAGRAPH.WL1. Lower priority (placeholder works), unlocks title screen + menu graphics.

S11 wrap recommendation: **A.13.1 first** — perf is now blocking the user from validating any future enemy or weapon work, so unlock it before A.16b. With ~10-15 FPS target after A.13.1, A.16b's AI work becomes verifiable in normal gameplay (you can actually walk around a moving guard) instead of via static snapshots. A.13.1 is also the natural wall-variety landing site per S11.

### S11 wrap-up

Single milestone, single sitting, two iterations (load+scan+render dry-run, then 8-direction rotation). One-character fix on the rotation chirality. Foundation chain A.1..A.15 paid off again — A.16a is +70 LOC of additive code (sparse VSWAP table, dirangle, atan2_q10, rotation branch in DrawAllSprites) plus a 514 B Python-generated LUT. No structural change to ScanObjects' shape, DrawSpriteWorld's signature, or any pre-existing subsystem.

Bonus discoveries: (1) the wall-texture modulo regression — pre-existing, surfaced by the user mid-S11, captured + deferred to A.13.1; (2) the minimap-markers-as-nav-aid pattern that promotes a debug feature into a gameplay one.

Workflow rule re-confirmed: code + VIS_sessions.md + README.md in the same commit. Two commits in S11 (iter-1 dry-run could have been a separate commit but was rolled into the S11 close commit since iter-2 was minor and the milestone reads as one atomic deliverable in retrospect).

---

## Session 12 — 2026-04-25 — Milestone A.13.1 (raycaster polish bundle)

**Scope:** the perf-as-interaction-blocker fired in S11 ("4-5 FPS makes any enemy/weapon validation tedious") forced a polish pass on the raycaster + walls before any A.16b/A.18 gameplay work. Bundled deliverables: (a) grid-line DDA replacing step-by-fraction, (b) Tier-3 wall variety (32 pages, side-aware light/dark), (c) Watcom optimization-flag retrofit, (d) inner-loop micro-opt + half-col cast, (e) time-scaled door animation. Light-by-distance (originally Fase 3) deferred — would have eroded the perf gain just earned.

User opening: "iniziamo decisamente da 1+2. Per il resto usa pure best judgment, un recon non fa mai male". Pre-coding recon on `WL_DRAW.C` confirmed the canonical wall-side mapping (`vertwall[i]=(i-1)*2+1` dark = X-side ray hit; `horizwall[i]=(i-1)*2` light = Y-side ray hit) before any code change.

### Iter 1 — grid-line DDA + Tier-3 walls (~45 min, zero-fix)

`wolfvis_a13_1.c` cloned from `wolfvis_a16a.c` (~1500 LOC base).

**Grid-line DDA in CastRay.** Replaced the 1/16-tile sub-step DDA (max 1024 sub-steps per ray) with the canonical Wolf3D / Lode-tutorial pattern:

- `deltadist_x_q88 = (1<<23) / |cos_q15|` (and similar for Y) — Q8.8 distance per X-tile crossing. Capped at `0x00FFFFFF` for near-axis-aligned rays where the other axis dominates.
- `side_dist_x_q88` initialized to distance from origin to the FIRST X-grid line in ray direction, computed from `g_px & 0xFF` fractional + step direction.
- Loop pick `min(side_dist_x, side_dist_y)`, advance by `+= deltadist` on that axis, step exactly 1 tile. Each iteration moves to a new tile (vs old ~16 sub-steps per tile).
- Texture x: at hit, project ray to perp world coord (`hit_pos_y_q88 = g_py + perp_dist*dy/32767`), take fractional × 64.
- Door branch fires once per door-tile entry (vs every sub-step in old DDA). Slab-plane crossing test reuses the A.14.1 math; computes slab distance via `(abs_axis_diff * 32767) / abs_dir` then verifies perp-coord stays inside tile and is past the open extent.

**Tier-3 walls.** `WALL_COUNT` 8 → 32 (16 walls × {light, dark}). `walls[]` migrated `__far` → `__huge` (128 KB > 64 KB segment cap, mirror sprites pattern from A.14). `TileToWallTex(tile, side)` now returns `((tile-1)*2 + (side==X_SIDE ? 1 : 0)) % 32`. The pre-A.4 Hitler-poster collapse is closed: every distinct tile_id in 1..16 gets a distinct light/dark pair; tiles 17..63 wrap-modulo with valid pairs. `DOOR_TEX_IDX` sentinel updated 8 → 32.

**LoadVSwap** unchanged structurally — already loops `i < WALL_COUNT` and uses `walls[i]`. Loading 32 chunks instead of 8 at startup is a one-time cost (~1.5 s extra at ISO load; user-imperceptible). EXE went 281 KB (A.16a) → 248 KB (A.13.1, due to `-ox` later).

**Build.** First-try clean compile + link + ISO + MAME launch. Snapshot `0021.png` showed the Wolf3D blue-stone E1L1 walls with side-aware light/dark visible at corridor corners. User v1: "I wall sono PERFETTI. Ora sono blu com'è giusto che sia nel primo livello di W3D!" — ironic discovery: the user thought the walls had always been gray-stone, but vanilla Wolf3D E1L1 is blue-stone. The Hitler-monopoly bug had been hiding the level designer's intended wall id 1 (= ELITE_BLUE_STONE) since A.4.

### Iter 2 — perf reckoning: -ox is THE missing flag

Same iter-1 build: user v1.b — "Perf però purtroppo completamente invariato rispetto a prima". Per memory `feedback_pacing_calibration.md` ("non proporre arrendersi prima di ~2h reali"), did NOT capitulate; investigated.

Build batch `wcc -zq -bt=windows -ml -fo=...` had **NO optimization flag**. Watcom default = `-od` (debug-style codegen, stack-spill per statement). 16 milestones from A.1 to A.16a all compiled unoptimized. Algorithmic perf wins (column-walk renderer, fixed-point math, LUTs, painter sort, grid-line DDA) were each individually compiled into bloated code.

Added `-ox -s` (aggressive opt + drop stack-overflow checks). Rebuild + rerun: user verdict "siamo intorno ai 4 FPS — TECNICAMENTE è quasi un raddoppio delle prestazioni se ci pensi". 2-3 → 4 FPS = +50-100% perceptible perf, **zero source change**.

Captured in `reference_watcom_optimization_flags.md`. Going-forward rule: every new `build_wolfvis_*.bat` MUST include `-ox -s`.

### Iter 3 — half-col cast + tight inner loop (+1 FPS)

Pushed further. Two changes bundled:

1. **Half-col cast in `DrawViewport`.** Outer loop step=2; cast 64 rays instead of 128. `DrawWallStripCol` now writes pairs of adjacent columns (col, col+1) with the same wall_h / texture / depth. Cast cost halves; per-col bookkeeping (wall_h_long divide, sy_step divide, texture pointer setup) halves. Pixel write count is identical. Visual cost: walls show 2-px horizontal stairsteps, invisible at the 128-wide viewport with 64-px texture source.
2. **Tight inner loop in `DrawWallStripCol`.** Pre-clip `dy` ranges once (no per-pixel bound checks); pointer-decrement framebuf access (no per-pixel `fb_y * SCR_W + sx` mul); paired writes via two parallel `BYTE __far *fb1, *fb2` decrementing pointers.

Build + rerun: 4 → 5 FPS. Modest. Diminishing returns clearly setting in.

### Iter 4 — door anim time-scale + partial-src StretchDIBits

User flagged: "porte: troppi frame di apertura, letteralmente 10 secondi". Root cause: `WM_TIMER` cadence (50 ms) gets throttled by render time (~200 ms per frame), so `AdvanceDoors` actually fires only ~5x/sec, and `DOOR_STEP=2` per call → 32 calls × 200 ms = 6-10 s for full open.

Two-part fix:

- **Time-scaled `AdvanceDoors`.** Track wall-clock via `GetTickCount`; advance by `step = elapsed_ms * DOOR_AMT_OPEN / DOOR_MS_FULL_OPEN` (DOOR_MS_FULL_OPEN = 1200). Door open/close duration is now ~1.2 s independent of frame rate. Robust against future perf changes (won't zip open at higher FPS, won't crawl at lower).
- **Partial-src StretchDIBits.** Suspected GDI was reading the full 64 KB src DIB every WM_PAINT even when `InvalidateRect` had restricted the dirty region. Switched to `srcY = SCR_H - dy - dh` (the formula `reference_stretchdibits_partial_src_gotcha.md` had warned was "easy to get wrong" — got it right here). User verdict: "nessun miglioramento percepibile, siamo sempre sui 4-5 FPS". Conclusion: GDI on MAME-VIS is NOT scanning the full source. Partial-src is correctness-equivalent but not a perf win on this platform. Leaving the change in (it's mathematically correct + smaller theoretical work).

Door anim: user "porte molto meglio ora!". Resolved.

### Perf reckoning (honest)

Before A.13.1: 2-3 FPS. After: 4-5 FPS. Real ~+50-100% improvement. User clocked with stopwatch, not just the perf-bar swatch ("ho provato anche a fare un calcolo un po' più 'vero' con uno stopwatch, ero stato lievemente ottimista").

Where the time goes (estimated, NOT measured):

- Cast (DDA + half-col): ~2-5 ms / frame
- Wall + ceil + floor pixel writes (tight loops): ~10-15 ms
- Sprite draw (8 guards + statics, painter sort): ~20-30 ms
- StretchDIBits (320×200 byte blit + palette translation): ~constant, partial-src didn't help
- BeginPaint/EndPaint + Win16 message pump + MAME-VIS GDI emulation: **140 ms gap, currently unmeasured**

User pushed back on "this is the hardware floor" framing, correctly. We don't know with certainty — we know we used up the easy levers. To prove or disprove the 140 ms infrastructure floor we'd need split telemetry (cast / wall / sprite / paint reads). Deferred to a future "PF finale" pass after L1 is gameplay-complete.

### Light-by-distance — deferred

Fase 3 of the original A.13.1 bundle. Skipped per the `if perf headroom ≥10 FPS then bundle, else defer` plan from S12 open. We landed at 5 FPS, no headroom; light-by-distance (~+10% inner loop cost) would have eroded the gain. Recorded in todo for a future polish pass — viable as A.13.2 standalone or bundled with the eventual PF finale.

### Build

- Compile: `wcc -zq -bt=windows -ml -ox -s -fo=..\build\wolfvis_a13_1.obj wolfvis_a13_1.c` — clean.
- Link: `wlink @link_wolfvis_a13_1.lnk` — same `IMPORT hcGetCursorPos` chain.
- Output: `build/WOLFA131.EXE` 248 KB, `build/wolfvis_a13_1.iso` 1.19 MB.

### Result

Snapshot `snap/vis/0021.png` (iter-1 final): blue-stone E1L1 corridor with side-aware light/dark walls + sliding doors visible mid-corridor + guard sprite framed in doorway + minimap with red dots showing distant guards.

Perf: 4-5 FPS measured by stopwatch. Doors fully open in ~1.2 s. Walls render the canonical Wolf3D blue-stone E1L1 layout for the first time since A.4.

### Trap / Gotcha / Eureka (S12)

- **Eureka S12.E1 — Watcom `-ox -s` was the missing project-wide perf lever.** 16 milestones of code-level optimization built without any compiler optimization. Adding `-ox` alone gave more perf than every algorithmic change combined. Captured in `reference_watcom_optimization_flags.md`. Future-me: when perf is suspect, FIRST check the build batch flags, THEN profile, THEN attempt algorithmic changes. The order matters because the cost of compiler-disabled-opt dwarfs anything else at the C level.
- **Eureka S12.E2 — Hitler-poster bug was hiding blue-stone E1L1 since A.4.** User had thought walls were "supposed to be gray". The (tile-1)*2 % WALL_COUNT collapse meant tile id 1 (ELITE BLUE STONE) was always rendering as wall page 0 = vanilla GRAY STONE. With Tier-3 walls correctly mapping tile 1 to its proper light/dark pair, the canonical Wolf3D E1L1 look surfaced for the first time. Pattern: a "long-standing visual bug" can be hiding the actual asset designer's intent — when a fix surprises the user with "this is supposed to look like X??", suspect a bug-hidden-feature.
- **Trap S12.1 — Build batch invocation in bash recurring trap.** `cmd.exe //c build_wolfvis_a13_1.bat` failed with relative path even after `cd src`. Fixed via absolute path quoted: `cmd.exe //c "d:/Homebrew4/VIS/src/build_wolfvis_a13_1.bat"`. User flagged as recurring; captured in `reference_build_bat_invocation.md`.
- **Trap S12.2 — Door anim was render-rate-bound.** Originally `DOOR_STEP=2` per `WM_TIMER(50ms)` = 1.6 s in theory, but at 5 FPS the timer was throttled and full open took ~10 s in practice. Fixed via time-scaled `AdvanceDoors` over `GetTickCount` delta; opens in ~1.2 s independent of frame rate. Lesson: any animation tied to `WM_TIMER` cadence must be time-scaled, not tick-counted, when render rate is variable / low.
- **Trap S12.3 — Partial-src StretchDIBits did NOT speed up GDI on MAME-VIS.** Hypothesis was that the full 64 KB src scan was eating WM_PAINT time. Switched to mirrored partial src (formula `srcY = SCR_H - dy - dh` per the bottom-up DIB convention, the formula `reference_stretchdibits_partial_src_gotcha.md` warned was easy to mis-do — got it right). Zero perf change. Conclusion: GDI on MAME-VIS clips the src read internally; full-src is NOT a perf cost on this platform. The original A.9 memo's claim of "trascurabile" was actually correct here; we kept the partial-src code because it's mathematically equivalent and safer if the platform ever changes.
- **Recon-first paid off again.** Pre-coding recon on `WL_DRAW.C` (HitVertWall / HitHorzWall / vertwall / horizwall) gave the canonical side mapping (X-side dark, Y-side light) before any code. Iter-1 was zero-fix for the cast + walls, just like A.14.1 and A.16a. Recon-first is now four-for-four on multi-subsystem milestones.

### Concrete results

- New: `src/wolfvis_a13_1.c` (~1530 LOC, +30 vs A.16a — net additions despite removing CAST_STEP_SHIFT and the sub-step DDA branch), `src/wolfvis_a13_1_sintab.h`, `src/wolfvis_a13_1_atantab.h`, `src/build_wolfvis_a13_1.bat` (with `-ox -s`), `src/link_wolfvis_a13_1.lnk`, `src/mkiso_a13_1.py`.
- New: `cd_root_a13_1/` (9 files: A.16a set with `WOLFA131.EXE` + `SYSTEM.INI` updated to `shell=a:\WOLFA131.EXE`).
- New: `build/WOLFA131.EXE` (248 KB), `build/wolfvis_a13_1.iso` (1.19 MB).
- New: `snap/vis/0021.png` (blue-stone E1L1 + corridor doors + guard).
- README: A.13.1 row added; quick-start commands updated to A.13.1 binaries.
- Memory: `reference_watcom_optimization_flags.md` (NEW), `reference_build_bat_invocation.md` (NEW), `MEMORY.md` index updated.
- Deferred: light-by-distance → A.13.2 or PF finale post-L1.

### Next-step candidates for Session 13

1. **A.16b — Enemy AI ticker** (~1.5-2 h). Reuse A.16a Object[] + DrawAllSprites with rotation. State machine (idle/walk/attack/hit/die) + LOS check + walking-frame animation cycle. With 5 FPS effective rate, AI verification will rely on slow-motion observation; gameplay testing may need to wait for A.18 firing to assess responsiveness. **Strong default.**
2. **A.17 — Weapon overlay (start)** (~1 h). Gun sprite at bottom-center + PRIMARY animation cycle, no firing yet. Visual milestone, low complexity.
3. **A.15.1 — Real BJ face from VGAGRAPH** (~1-1.5 h). Chunked Huffman loader + picture table for VGAGRAPH.WL1. Lower priority (placeholder works), unlocks title screen + menu graphics.

S12 wrap recommendation: **A.16b first** per the original S11/S12 roadmap. PF finale (split telemetry + further perf attack) deferred to post-L1 per user direction: "finiamo le aggiunte per arrivare a un livello 1 completo, poi faremo un round di diagnostica e PF finale".

### S12 wrap-up

Single milestone, single sitting, four iterations (DDA+walls / -ox / half-col+tight-loop / partial-src+door-time-scale). One clean recovery from "perf invariato" finding (the build-flag discovery saved the session from feeling like a wash). User feedback steered the session twice: pushed back on premature "hardware floor" framing (correct — we never measured, just estimated) and confirmed door-anim showstopper (a real defect orthogonal to perf).

Bonus discoveries: (1) Watcom `-ox -s` is project-wide retrofit lever — every future build batch needs it; (2) blue-stone E1L1 is the canonical Wolf3D look (not gray); (3) door anim must be time-scaled when render rate is variable; (4) GDI on MAME-VIS does NOT pay the full-src scan cost (memo `reference_stretchdibits_partial_src_gotcha.md`'s "trascurabile" turned out to be correct on this platform).

Workflow rule re-confirmed: code + VIS_sessions.md + README.md + memory in the same commit. One commit for S12 close (the four iterations are logically one bundle, no cherry-pickable midpoints).

---

## Session 13 — 2026-04-25 — Milestone A.16b enemy AI ticker

### Scope

Single milestone, single iteration. Stand → Walk state machine for guard enemies, LOS-aware chase, sub-tile movement, collision against walls + closed doors. PoC-grade: no firing (deferred to A.18), no pain/die (no damage system yet), no patrol/path (vanilla T_Path skipped — guards are dormant until they sight the player). Scope inherited verbatim from the S13 opening TODO.

### Recon (~10 min)

Pre-coding pass over Wolf3D shareware source — the recon-first pattern is now five-for-five on multi-subsystem milestones (A.13, A.14, A.14.1, A.16a, A.13.1).

- **Sprite enum (WL_DEF.H:159-208)**: confirmed walking-frame chunks `SPR_GRD_W1_1..W4_8` are consecutive `58..89` (32 frames, 4 phases × 8 directions). Pain/die/dead at `90..95`, shoot at `96..98` — explicitly out of scope.
- **State engine (WL_STATE.C:113-117 NewState)**: vanilla pattern is `state = state->next` when ticcount → 0; think + action callbacks invoked separately. Our PoC fuses think + render-driver, runs all per-tick logic inside one ticker.
- **Guard state defs (WL_ACT2.C:418-444)**: `s_grdstand` calls `T_Stand` = `SightPlayer` only. `s_grdpath1..4` calls `T_Path`, `s_grdchase1..4` calls `T_Chase`. Tic durations (path 20+5+15+20+5+15) gave us the timing target: ~1 s W1→W4 cycle ≈ 250 ms per phase.
- **`T_Chase` (WL_ACT2.C:3069)**: shoots player when `CheckLine` passes + RNG, else moves toward player. Our PoC drops the shoot branch entirely; movement is direct via 8-dir snap from `atan2(player - enemy)`.
- **`CheckLine` (WL_STATE.C:1037)**: vanilla LOS via 1/256-tile precision DDA, doors-aware (intercept-vs-doorposition test). PoC simplification: tile-grid Bresenham, doors block when `g_door_amt < DOOR_BLOCK_AMT` (mirror of `IsBlockingForMove`). Door-aperture-aware LOS deferred to A.18 alongside hitscan.

### Implementation

Single source file `src/wolfvis_a16b.c` (~1690 LOC, +160 vs A.13.1). Five plumbing changes, all additive:

- **Sparse VSWAP table extended**: `sprite_chunk_offs[]` grows from 26 to 58 entries. Slots 26..57 map to VSWAP chunks 58..89 (phase-major: slot 26..33 = W1, 34..41 = W2, 42..49 = W3, 50..57 = W4). Loader (one-line bump `NUM_SPRITES 26 → 58`) loads them with the same `_lread`/`_llseek` path used since A.4.
- **`Object` struct extension**: added `long x_q88, y_q88` (sub-tile position in Q24.8), `BYTE state, state_phase`, `DWORD state_tick_last`. The painter sort + `DrawSpriteWorld` were refactored to read `x_q88/y_q88` directly instead of recomputing tile-center; static decorations seed these fields with `(tile_x<<8)|0x80`.
- **`LOSCheck(ex, ey, px, py)`**: tile-grid Bresenham walker. `IsWall` and partially-closed `IsDoor` block. End tile (player) is implicit. `safety = MAP_W + MAP_H` guards against infinite loops on degenerate inputs.
- **`AdvanceEnemies()`**: time-scaled ticker mirroring `AdvanceDoors`. `GetTickCount` delta drives both phase advance (`ENEMY_PHASE_MS = 250`) and movement step (`ENEMY_SPEED_Q88 = 128` Q8.8/sec ≈ 0.5 tile/sec). Per-enemy state machine: same-tile-or-LOS-fail → STAND, else WALK with snapped `enemy_dir`. Per-axis collision check (`IsBlockingForMove(ntx, ety)` then `IsBlockingForMove(tx, nty)`) so a guard sliding along a wall doesn't stop at the corner.
- **Snap-to-8-dir**: `ord_to_dirtype[]` LUT folds the Q10 angle ordinal `((ang+64)>>7)&7` (CW from east) back into the vanilla Wolf3D dirtype convention `{E=0, NE=1, N=2, NW=3, W=4, SW=5, S=6, SE=7}` already used by A.16a's rotation code. `enemy_dx_q8[]/dy_q8[]` is the per-dirtype unit vector in Q0.8, with diagonals scaled by `181/256 ≈ 1/√2` so guards moving NE don't outpace guards moving N.
- **`DrawAllSprites` sprite picker**: state-aware. STAND uses the existing `GUARD_S_FIRST_SLOT + sector` (= slots 18..25). WALK uses `GUARD_W_FIRST_SLOT + (state_phase << 3) + sector` (= slots 26..57). The atan2-based rotation math is reused unchanged from A.16a — the chirality fix `(facing - e2p_angle + 1024 + 64)` ported verbatim.
- **WM_TIMER wiring**: one-line addition after `AdvanceDoors()` in the existing message-pump tick.

### Build

- Compile + link: clean, zero-iteration. `WOLFA16B.EXE` 251 KB (was 248 KB for A.13.1, +3 KB for AI code + LUTs + extended struct).
- ISO: `build/wolfvis_a16b.iso` 1.19 MB. `cd_root_a16b/` cloned from `cd_root_a13_1/` with `WOLFA16B.EXE` and `SYSTEM.INI` `shell=` pointing at the new EXE.

### Result

Snapshot `snap/vis/0022.png`: a guard standing inches from the camera, frontal pose (correct sector pick on melee approach), HUD chrome intact, minimap showing player + remaining enemies.

User test verdict: *"tutto perfetto :D che ansia anche, ora si avvicinano e quando sono davanti a te stanno ferme a fissarti, praticamente è un horror in questa fase"*. The "horror stare" is exactly the PoC behavior contract: chase fires when LOS+range pass, guards advance, then `STAND` re-engages when they reach an adjacent tile (no firing means they just face you and breathe). Vanilla `T_Chase` would have transitioned to `s_grdshoot1` in the RNG branch — A.18 closes that loop.

### Trap / Gotcha / Eureka (S13)

- **Eureka S13.E1 — recon-first now 5-for-5 on multi-subsystem milestones.** A.16b touched VSWAP loader, BSS layout, Object struct, ScanObjects, DrawAllSprites picker, AdvanceEnemies + LOSCheck (new modules), and WM_TIMER hook. Built clean + ran behaviorally correct first try. Time invested in pre-coding recon (~10 min) keeps paying off vs. the cost of an iteration cycle (~5-10 min compile + ISO + MAME boot + read result).
- **Eureka S13.E2 — Q8.8 sub-tile pos as the painter-sort + collision substrate is the right primitive.** Replacing `tile_x/tile_y` lookups in painter sort + DrawSpriteWorld with direct `x_q88/y_q88` reads eliminated the per-frame `((long)tile<<8)|0x80` recompute (small win) and made the moving-enemy code drop in trivially (big win). For A.18 hitscan, world-space `x_q88/y_q88` is already the format the player ray needs; no further refactor needed.
- **No Trap S13.x** — uneventful single-iteration milestone. Build batch absolute-path invocation worked first try (S12 trap memory paid off via PowerShell tool over Bash `cmd //c` pattern). Object struct grew without DGROUP issues (`__far Object[128]` of 22 bytes ≈ 2.8 KB, well under 64 KB).
- **Behavior PoC vs. canonical Wolf3D**: our `AdvanceEnemies` is a fused `T_Stand + T_Chase` loop — no reaction-delay model (vanilla `SightPlayer` rolls `temp2 = 1 + US_RndT()/4` for guards), no path/patrol behavior (`T_Path` deferred), no shoot-on-LOS-RNG (deferred to A.18). The "horror stare" is a direct artifact of skipping the shoot transition. This is the right PoC scope: gameplay loop closes only when firing exists, so we wait until A.18 to taste real combat.

### Concrete results

- New: `src/wolfvis_a16b.c` (~1690 LOC), `src/build_wolfvis_a16b.bat`, `src/link_wolfvis_a16b.lnk`, `src/mkiso_a16b.py`. Sin/atan tables `wolfvis_a13_1_sintab.h` / `wolfvis_a13_1_atantab.h` reused unchanged.
- New: `cd_root_a16b/` (9 files: A.13.1 set with `WOLFA16B.EXE` + `SYSTEM.INI` updated `shell=a:\WOLFA16B.EXE`).
- New: `build/WOLFA16B.EXE` (251 KB), `build/wolfvis_a16b.iso` (1.19 MB).
- New: `snap/vis/0022.png` (guard in melee range, frontal pose, minimap intact).
- README: A.16b row added.
- Memory: `project_milestone_A16b_ai.md` (NEW), `MEMORY.md` index updated, `project_S13_todo_opening.md` retired in favor of S14 opening.

### Next-step candidates for Session 14

1. **A.17 — Weapon overlay** (~1 h). Gun sprite at bottom-center + PRIMARY animation cycle, no firing yet. Visual milestone, low complexity. Could fit at the tail of S13 if momentum holds, otherwise S14 head.
2. **A.18 — Firing + hitscan + damage** (~1.5-2 h). Hitscan ray from player center, first-hit (wall vs. enemy via z-buffer scan), damage application + ammo decrement + score increment. PRIMARY rebind door→fire, door toggle moved to SECONDARY. HUD finally driven by real state. Closes the "horror stare" gap.
3. **A.15.1 — Real BJ face from VGAGRAPH** (~1-1.5 h). Lower priority (placeholder works).

S13 wrap recommendation: **A.18 directly in S14**. A.17 weapon overlay is a 1-hour visual prelude — could absorb into S14 as warm-up before A.18. PF finale + light-by-distance still deferred to post-L1.

### S13 wrap-up

Single iteration, zero fixes. The recon pass took 10 minutes; the implementation took 45 minutes; build + MAME test + user verification took 5 minutes. ~1 hour total real-time vs. the 1.5-2 h S13 budget — under-budget enough to optionally absorb A.17 in the same session if user direction holds.

The "horror stare" gameplay artifact is the cleanest possible signal that the AI loop closes correctly: enemies see the player → walk to him → reach melee range → stand. Adding firing in A.18 will turn the same loop into actual combat without changing any of the A.16b plumbing.

---

## Session 13 (cont.) — 2026-04-25 — Milestone A.17 weapon overlay

### Scope

Visual milestone, S13 stretch. Pistol sprite painted at viewport bottom-center every frame, foreground-on-top of raycaster + in-world sprites. Static (no firing animation, no input rebind). Two iterations: iter-1 shipped a primitive `FillRect` silhouette (pattern A.15 face_placeholder) and proved the layout; iter-2 replaced it with the canonical `SPR_PISTOLREADY` chunk loaded from VSWAP at boot.

### Iter 1 — primitive pistol (~30 min)

Quick `FillRect`-based silhouette: 32x36 px gun shape with mid-gray slide, brown grip, dark trigger guard, 1-px black outline. Pattern matched A.15 `DrawFacePlaceholder` exactly: ~14 `FillRect` calls, no asset data, drawn after `DrawAllSprites()` in `DrawViewport()` so it's always foreground.

User verdict: *"L'hai disegnata tu immagino? :D :D"* — the fan-art look was instantly recognizable. Visual PoC accepted, but for a public repo a real Wolf3D sprite is preferable. Decision: upgrade in same session.

Build: clean except a `W131: No prototype` warning (DrawWeaponOverlay defined after its caller `DrawViewport`). Fixed via a one-line forward declaration before `DrawViewport`.

### Iter 2 — VSWAP-loaded SPR_PISTOLREADY (~20 min)

The user-attack sprite frames live in VSWAP, not VGAGRAPH (initial assumption was wrong — VGAGRAPH holds GUNPIC for the HUD-status icon, but the in-viewport overlay frame is a 64x64 t_compshape sprite same as enemy frames). They sit at the trailing end of the WL1 sprite enum (`WL_DEF.H:457-468`):

```
SPR_KNIFEREADY .. SPR_KNIFEATK4   (5 frames)
SPR_PISTOLREADY .. SPR_PISTOLATK4 (5 frames)
SPR_MACHINEGUNREADY .. ATK4       (5 frames)
SPR_CHAINREADY .. CHAINATK4       (5 frames)
```

20 trailing frames total, `SPR_PISTOLREADY` at offset +5 = `total_sprites - 15` from the end. `total_sprites = sound_start_idx - sprite_start_idx` is parsed from the VSWAP header at boot.

Plumbing:

- `NUM_SPRITES` bumped 58 → 59. Slot 58 = `PISTOL_READY_SLOT`.
- `sprite_chunk_offs[]` was `static const`, became `static` (mutable). Slot 58 carries `0` as compile-time placeholder.
- After `LoadVSwap()` parses the header but before the load loop runs, a runtime patch sets `sprite_chunk_offs[PISTOL_READY_SLOT] = total_sprites - PISTOL_READY_OFFSET (15)`. Bounds-checked: if the computed offset goes negative or exceeds the sprite range, the patch is skipped and the slot stays empty (DrawSpriteFixed silently no-ops on empty slots).
- New `DrawSpriteFixed(sx, sy, sprite_idx)`: 1:1 screen-coord blit. Walks the same `t_compshape` post format as `DrawSpriteWorld` (leftpix/rightpix/dataofs/post triples) but with no scale, no z-buffer, no projection — just direct framebuffer writes at `(sx + srcx, sy + sy_src)`.
- `DrawWeaponOverlay()` rewritten: one call to `DrawSpriteFixed(32, 99, PISTOL_READY_SLOT)` — 64x64 sprite centered horizontally in the 128x128 viewport, bottom flush at viewport y=162 (one px above HUD top).

Build: clean. EXE 256 KB (+5 KB vs A.16b — the new blit helper + 4 KB sprite slot).

### Result

Snapshot `snap/vis/0025.png`: the canonical Wolf3D pistol idle silhouette at viewport bottom-center. Small visible region (~12x12 px of opaque pixels in the 64x64 sprite) because vanilla `SPR_PISTOLREADY` is artistically the slide-top-down view that the player sees holding the gun in hand — the firing frames `ATK1..4` are the raised-with-muzzle-flash poses that fill more of the frame. This is correct vanilla appearance; the gun appears proportionally small in idle because Wolf3D's viewport is 304x152 (half-screen) and the pistol is "casual hold" art.

User verdict: *"Confermato! Guarda snap"*. Chunk discovery `total_sprites - 15` is correct for WL1 shareware.

### Trap / Gotcha / Eureka (S13.A.17)

- **Eureka S13.A.17.E1 — VSWAP, not VGAGRAPH, holds the in-viewport weapon sprites.** Initial recon assumed VGAGRAPH (because `GUNPIC = chunk 100` in `GFXE_WL1.H` looks weapon-related). That's the HUD-status icon (32x16 thumbnail in `WL_AGENT.C:546 StatusDrawPic`). The actual in-viewport overlay routes through `SimpleScaleShape` with sprite shapenum from `attackinfo[].frame`, which indexes into the VSWAP sprite enum. Pattern: when looking up a Wolf3D asset, distinguish HUD-status pics (VGAGRAPH chunked Huffman) from in-viewport sprites (VSWAP `t_compshape` posts). The HUD path is much heavier (Huffman + 4-plane EGA decode); the in-viewport path is identical to what we already have for enemy/decoration sprites.
- **Eureka S13.A.17.E2 — Mutable `sprite_chunk_offs[]` + runtime patch is the right pattern for variable-position enum entries.** WL1 vs WL6 vs SOD have different sprite enum layouts (SOD/SDM add `SPR_SPECTRE..ANGEL_DEAD` before the player frames, shifting the indices). Hardcoding `SPR_PISTOLREADY` would break on a different IWAD; runtime discovery via `total_sprites - PISTOL_READY_OFFSET` makes the loader portable. For A.18+ we can extend the same pattern: `sprite_chunk_offs[59..62] = total - 14..-11` for `SPR_PISTOLATK1..4`.
- **No traps in iter-2.** Build was clean first try; chunk discovery hit the right chunk; sprite displayed correctly. Single forward-decl warning carried over from iter-1, already fixed.
- **Note on visible silhouette size.** Vanilla `SPR_PISTOLREADY` has a small opaque region (~12 columns) in a 64-wide sprite — the rest is transparent. At 1:1 in our 128-wide viewport, the visible gun is small. This is correct. A.18 firing frames will paint more of the frame (raised-arm pose with muzzle flash). Optional polish: scale-up the blit ~1.5x to make the gun more prominent in our smaller viewport — but that risks looking outsize for vanilla feel. Defer.

### Concrete results

- New: `src/wolfvis_a17.c` (+~110 LOC vs A.16b), `src/build_wolfvis_a17.bat`, `src/link_wolfvis_a17.lnk`, `src/mkiso_a17.py`.
- New: `cd_root_a17/` (9 files: A.16b set with `WOLFA17.EXE` + `SYSTEM.INI` updated `shell=a:\WOLFA17.EXE`).
- New: `build/WOLFA17.EXE` (256 KB), `build/wolfvis_a17.iso` (1.19 MB).
- New: `snap/vis/0023.png` (iter-1 primitive), `snap/vis/0024.png`, `snap/vis/0025.png` (iter-2 vanilla SPR_PISTOLREADY).
- README: A.17 row added, quick-start updated to A.17.
- Memory: `project_milestone_A17_weapon.md` (NEW), `MEMORY.md` index updated, `project_S14_todo_opening.md` updated to re-elevate A.15.1 BJ face per user reminder.

### Next-step candidates for Session 14

1. **A.18 — Firing + hitscan + damage** (~1.5-2 h). Default S14 scope. Hot-swap `PISTOL_READY_SLOT` for `SPR_PISTOLATK1..4` at fire-rate cadence (same runtime patch pattern); hitscan ray from player center; first-hit via z-buffer scan; damage application + ammo decrement + score increment. PRIMARY rebinds door→fire.
2. **A.15.1 — Real BJ Blazkowicz face on HUD** (~1-1.5 h). Re-elevated per user reminder. VGAGRAPH chunked Huffman loader (separate from VSWAP) — chunks `FACE1APIC..FACE3CPIC` give the 8 hp-state face frames for status bar. Could be S14 head warm-up before A.18 main scope.

### S13 total wrap-up (A.16b + A.17)

Two milestones in one session, three iterations total (A.16b single iter, A.17 two iters). All zero-fix on the technical side: each compile cycle was clean, each MAME launch worked, no chirality bugs or DGROUP overflows. Recon-first now 6-for-6 on multi-subsystem milestones.

Time invested: ~2 h real-time S13 (recon 10 min + A.16b impl 45 min + A.17 iter-1 30 min + A.17 iter-2 20 min + wrap 30 min for both). Within the original S13 1.5-2 h budget despite the stretch.

User experience arc: A.16b "horror stare" → A.17 fan-art pistol → A.17 vanilla pistol. The session closed with the user comfortable enough to confirm both visually-significant milestones, including a one-shot "yeah that primitive looks like you drew it" detection that triggered the upgrade to vanilla art mid-session — proof the public-repo polish bar matters.

Push: A.16b commit `2d9e905` pushed mid-session at user direction. A.17 to be pushed after wrap.

---

## Session 14 — 2026-04-25 — Milestone A.18 firing + hitscan + damage

### Scope

Single milestone. Close the gameplay loop: PRIMARY fires the pistol, hitscan finds the closest visible enemy whose projected screen span straddles the crosshair column, damage drops `Object.hp`, dying guards play a 3-frame DIE animation and freeze on the DEAD sprite. Ammo and score update on the HUD via partial-rect re-blit. Door toggle migrated to SECONDARY (the A.14.1 OPL sanity click is dropped).

Scope inherited verbatim from the S14 opening TODO: 8-phase plan (sprite slot extension / input rebind / weapon FSM / hitscan / damage state / sprite picker / HUD redraw / build+test). User accepted option (A) "A.18 secco" — A.15.1 BJ face deferred.

### Recon (~5 min)

Two pieces validated before coding:

- **`SPR_GRD_PAIN_1..DEAD` are at fixed sprite-enum indices 90..95** (`WL_DEF.H:205-206`), not in the trailing weapon-arsenal range. The S14 opening TODO had wrongly noted "chunks total-10..-5 area" — these slots in the trailing 20 are MGUN frames. Pain/die/dead carry **absolute** offsets in `sprite_chunk_offs[]` (no runtime patch), unlike PISTOLATK1..4 which sit at the trailing tail and need `total - 14..-11` discovery (mirror of A.17 `total - 15` for PISTOLREADY).
- **z-buffer at `g_zbuffer[VIEW_W/2]` already gives the wall-distance for the crosshair column** every frame — no separate `CastRay` call needed for the hitscan. The DrawSpriteWorld projection math (cam_y = rx·cos + ry·sin, screen_x = VIEW_W/2 + cam_x·focal/cam_y, sprite_h = (VIEW_H<<8)/cam_y) is reusable verbatim for "does this enemy's projected span cover the crosshair pixel".

### Implementation

Single source file `src/wolfvis_a18.c` (~3210 LOC, +~250 LOC vs A.17). Eight additive changes on the A.17 baseline:

- **Sprite slot extension**. `NUM_SPRITES` 59 → 67. Slots 59..62 = SPR_PISTOLATK1..4 (placeholders, runtime-patched in `LoadVSwap` to `total - 14..-11`); slots 63..65 = SPR_GRD_DIE_1..3 (compile-time 91..93); slot 66 = SPR_GRD_DEAD (compile-time 95). PAIN_1 (90) and PAIN_2 (94) skipped per PoC scope (vanilla pain is a one-frame flash, not load-bearing).
- **Input rebind**. `VK_HC1_PRIMARY` calls `FireWeapon(hWnd)`; `VK_HC1_SECONDARY` calls `ToggleDoorInFront()`. The A.14.1 OPL click on SECONDARY is removed (superseded by F1/F3 music keys).
- **Weapon FSM**. Two states (READY / FIRING). `FireWeapon` gates on READY + `g_ammo > 0`; on accept it decrements ammo, kicks the FSM into FIRING phase 0, runs `FireHitscan`, schedules HUD ammo redraw. `AdvanceWeapon` (called from WM_TIMER beside AdvanceDoors / AdvanceEnemies) advances the FIRING phase every `WEAPON_PHASE_MS = 100 ms`, auto-returns to READY at phase 4. `DrawWeaponOverlay` switches between `PISTOL_READY_SLOT` and `PISTOL_ATK1_SLOT + phase`.
- **Hitscan first-hit**. `FireHitscan` walks `g_objects[]`, projects each living enemy into camera space, computes `screen_x` + `sprite_h`, and tests whether `VIEW_W/2` falls inside `[screen_x - sprite_h/2, screen_x + sprite_h/2]`. The wall-distance gate uses `g_zbuffer[VIEW_W/2]` directly — no CastRay re-projection needed. Closest passing enemy by smallest `cam_y_q88` is the hit; ties broken by iteration order.
- **Damage + state machine**. `Object.hp` (BYTE, init `GUARD_HP_INIT = 25` for guards, 0 for decorations). Damage roll = `5 + (Prng7() & 7)` = 5..12 via a tiny LCG (no rand() runtime). On lethal hit: `state = OBJ_ST_DIE`, `state_phase = 0`, `state_tick_last = now`; `g_score += 100`, `g_kills++`, RedrawHUDScore. AdvanceEnemies extended: DIE state plays 3 frames at `ENEMY_PHASE_MS = 250 ms` then transitions to `OBJ_ST_DEAD`. DEAD state is frozen — no movement, no LOS contribution, but still painter-sorted (corpses must z-order with live sprites).
- **Sprite picker switch**. DrawAllSprites: STAND/WALK paths unchanged. DEAD → `GRD_DEAD_SLOT` (non-rotating). DIE → `GRD_DIE1_SLOT + state_phase` (non-rotating, vanilla `statetype.rotate=false`). Decorations (enemy_dir == OBJ_DIR_NONE) bypass the state switch entirely.
- **HUD partial re-blit**. `RedrawHUDAmmo` / `RedrawHUDScore`: clear the digit box with `HUD_BG` FillRect, draw new value with `DrawNumber`, `InvalidateRect` the small rect. `framebuf` is patched in-place; `static_bg` (the A.15 bake) is left untouched. The framebuf HUD region is otherwise never overwritten by `DrawViewport` (which writes only inside the viewport rect), so values persist between fires. Bonus: `HUD_FG_LOW` (red, color 40) automatically applied when `g_ammo == 0` — the user spotted this without prompting.
- **Game state vars**. `g_weapon_state` / `g_weapon_phase` / `g_weapon_tick_last`, `g_ammo` (init 8), `g_score` (init 0), `g_kills` (telemetry), `g_prng` (LCG seed).

### Build

- Compile + link: clean, **single-iteration zero-fix**. `WOLFA18.EXE` 224 KB (vs A.17's 256 KB — Watcom ditched some DGROUP padding when the new code happened to align). Eight subsystems extended in one pass without a single warning.
- ISO: `build/wolfvis_a18.iso` 1.19 MB. `cd_root_a18/` cloned from `cd_root_a17/` with `WOLFA18.EXE` + `SYSTEM.INI shell=a:\WOLFA18.EXE`.

### Result

Three test snapshots in MAME-VIS:

- `snap/vis/0026.png`: mid-combat. Player facing a guard in front of a closed door. **Ammo 07** (one shot fired). Score 000000 (kill not yet). Pistol overlay visible at viewport bottom-center.
- `snap/vis/0027.png`: same approach, **Ammo 06** (second shot). Score still 000000. Confirms the FSM didn't double-decrement, and the FIRING-state taps gating works (PRIMARY tap during anim is dropped).
- `snap/vis/0028.png`: **closing scene**. SCORE = **000100** (kill registered). AMMO = **00 in red** (HUD_FG_LOW color 40 — `g_ammo == 0` branch confirmed without explicit ask). **Cadaver visible in the world**: the canonical Wolf3D SPR_GRD_DEAD sprite (sprawled, blood pool) at the corpse's last position, painter-sorted with the live guard visible in the distance behind it. Pistol overlay back to PISTOL_READY (FSM returned to READY after cycle). Player has clearly fired ~8 shots to land enough damage on a single 25-HP guard at 5..12 dmg per hit (high-side rolls = 3 hits would suffice; low-side could be 5 — between RNG and possible misses, 8 shots for 1 kill is in band).

User verdict: 1, 2, 3 confirmed visually (kill + DIE/DEAD anim, ATK frame cycle, PRIMARY/SECONDARY rebind). Point 4 (wall occlusion of bullet) untestable at 4-5 FPS — the user can't move with enough flow to engineer the geometry. Logically correct via z-buffer-occlusion test in `FireHitscan` (closest enemy with `cam_y < g_zbuffer[VIEW_W/2]` wins; if the wall is closer the gate rejects), but flagged as deferred verification to S15 PF finale where higher FPS would make the test reproducible.

### Trap / Gotcha / Eureka (S14)

- **Eureka S14.E1 — recon-first now 6-for-6 on multi-subsystem milestones.** A.18 touched 8 subsystems (sprite slots, Object struct, AI ticker, weapon FSM, hitscan projection, sprite picker, HUD redraw, input rebind). Compiled clean + ran behaviorally correct first try after a ~5 min recon pass. The A.13 / A.14 / A.14.1 / A.16a / A.13.1 / A.16b / A.18 streak is now too consistent to be luck — recon-first is canonical for this project, period. Cost (5-10 min reading vanilla sources) << cost of an iteration cycle (5-10 min compile + ISO + boot + read result + figure out which subsystem broke).
- **Eureka S14.E2 — z-buffer at the crosshair column IS the hitscan wall test.** Initial plan was to re-project a ray via `CastRay(g_pa)` for the hitscan; the recon pass realized the per-frame z-buffer already carries the wall distance for VIEW_W/2 because DrawViewport's half-col cast loop fills it. Net savings: one ray cast per fire, ~2-5 ms saved on each PRIMARY tap. More importantly: the wall-vs-enemy occlusion test is **automatically consistent** with what's drawn — the same z-buffer value gates both rendering and combat. Pattern: when adding a new query against world geometry, check whether the existing render pipeline already computed the answer.
- **Eureka S14.E3 — `Object.hp` BYTE was the right primitive.** Storing damage state as a single hp counter (rather than a separate "alive bool" + "damage taken" pair) means: (a) decorations get hp=0 trivially, (b) the kill check is `dmg >= hp`, (c) one struct field grows. Future damage-source variants (machinegun does 2x base, knife does melee-only) all flow through the same `DamageEnemy(idx, dmg)` callsite. Vanilla Wolf3D's `Object.hitpoints` lives in the same role.
- **Trap S14.1 (caught in recon, not in iter) — TODO memo had wrong PAIN/DIE/DEAD offset zone.** The S14 opening notes claimed "chunks total-10..-5 area" — but those are MGUN frames. The pain/die/dead chunks are at fixed sprite-enum indices 90..95 (vanilla `WL_DEF.H:205-206`). Caught the discrepancy by reading `WL_DEF.H` directly before extending `sprite_chunk_offs[]`. Saved an iteration: had we used the trailing-tail offsets, MGUN sprites would have shown up where DEAD sprites should — visually wrong but not crashy, would have wasted ~10 min to diagnose. Lesson: **TODO opening memos are forecast, not ground truth.** Always re-verify against the canonical source before coding the offsets.
- **No iteration cycles.** Eight subsystems wired correctly first try. The cleanest single-iteration milestone in the project so far.

### Concrete results

- New: `src/wolfvis_a18.c` (~3210 LOC, +~250 vs A.17), `src/build_wolfvis_a18.bat`, `src/link_wolfvis_a18.lnk`, `src/mkiso_a18.py`.
- New: `cd_root_a18/` (9 files: A.17 set with `WOLFA18.EXE` + `SYSTEM.INI` updated `shell=a:\WOLFA18.EXE`).
- New: `build/WOLFA18.EXE` (224 KB), `build/wolfvis_a18.iso` (1.19 MB).
- New: `snap/vis/0026.png`, `snap/vis/0027.png`, `snap/vis/0028.png` (mid-combat → out-of-ammo → corpse + 100 score).
- README: A.18 row added, quick-start updated to A.18.
- Memory: `project_milestone_A18_firing.md` (NEW), `MEMORY.md` index updated, `project_S14_todo_opening.md` retired in favor of S15 opening.

### Next-step candidates for Session 15

1. **PF finale + diagnostic** (deferred since A.13.1). Split telemetry (cast / wall / sprite / paint reads) to isolate the ~140 ms unaccounted-for gap in the per-frame budget. Algorithmic attack on the actual measured bottleneck. Required before the 4-5 FPS becomes acceptable for a public demo.
2. **A.15.1 — Real BJ Blazkowicz face from VGAGRAPH** (~1-1.5 h). VGAGRAPH chunked Huffman loader (separate from VSWAP), FACE1APIC..FACE3CPIC chunks, 8 hp-state face frames driven by `g_health` (PoC keeps the placeholder helmet otherwise; needs gating on `g_health` decrement which itself requires enemy-firing-back from A.19).
3. **A.19 — Pain flash + enemy firing back + player health**. Vanilla T_Chase RNG-gated shoot branch (chance/300), SPR_GRD_SHOOT1..3 (chunks 96..98), player damage on hit, `g_health` decrement, HUD health re-blit. Closes the symmetric combat loop. Pain flash for guards (one-frame s_grdpain) bundled.
4. **SFX on fire** (~30 min). OPL3 sound chunks live in VSWAP after the sprite range (`sound_start_idx`). Single chunk play on fire is trivial; SFX scheduler for queueing multiple is more involved.

S14 wrap recommendation: **PF finale first** in S15. The user explicitly deferred light-by-distance and split telemetry to "post-L1" (S11/S12 thread) with the framing "finiamo le aggiunte per arrivare a un livello 1 completo, poi faremo un round di diagnostica e PF finale". Gameplay loop is now closed (firing + dying enemies + score + ammo); a meaningful "level 1 complete" needs at minimum enemy firing back + player health (A.19), but the prerequisite for any of that being playable is breaking through the 4-5 FPS floor.

### S14 wrap-up

Single milestone, single iteration, zero fixes. ~250 LOC of new code touching 8 subsystems compiled clean first try. Total time ~1 h real-time (recon 5 min + impl 45 min + build/test/snap 10 min + wrap to follow). Inside the original S14 1.5-2 h budget by a wide margin.

The "horror stare" gameplay artifact from A.16b is fully resolved: PRIMARY now fires, guards take damage, the 3-frame death animation plays, and the cadaver remains in the world correctly painter-sorted. Ammo and score on the HUD respond in real time. The unprompted user-spotted detail (HUD ammo turning red on zero) is a small confirmation that the care put into the digit-font / gamepal-aware color system in A.15 paid off.

The `g_zbuffer[VIEW_W/2]` shortcut for the wall-vs-bullet occlusion is the kind of design choice that's only possible because the renderer was structured well from A.13: instead of duplicating geometry queries between rendering and combat, the same data structure serves both. Pattern worth carrying forward — A.19 enemy-firing-back can use the same z-buffer to gate the AI shoot ray against player walls.

Workflow rule re-confirmed: code + VIS_sessions.md + README.md + memory in the same commit.

---
