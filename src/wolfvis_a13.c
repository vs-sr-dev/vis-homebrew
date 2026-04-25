/*
 * WolfVIS A.13 — Raycaster (textured walls + ceiling/floor + player nav).
 *
 * The last structural unknown of the Wolf3D-on-VIS port. Foundation built
 * across A.1..A.12 (palette, walls, sprites, scaler, framebuf, input,
 * audio, perf, integrated scene) is consumed wholesale; this module adds
 * one new subsystem — the cast itself — and a column-walk renderer that
 * is a near-copy of A.12's DrawSpriteScaled inner loop.
 *
 * Layout 320x200:
 *   y=0..29       : debug bar (heartbeat + status, repainted on WM_TIMER)
 *   y=30..34      : black gutter
 *   y=35..162 x=0..127 : 3D viewport 128x128 (128 ray casts, ceil/wall/floor)
 *   y=35..98  x=140..203 : minimap 64x64 with player position + heading
 *   else          : black
 *
 * Controls (HC1 hand controller):
 *   d-pad UP/DOWN    : move forward / back along player heading
 *   d-pad LEFT/RIGHT : rotate counterclockwise / clockwise
 *   PRIMARY (A)      : OPL ch0 high click (legacy)
 *   SECONDARY (B)    : OPL ch0 mid click + key-off (legacy)
 *   F1               : OPL init + start music (idempotent)
 *   F3               : stop music
 *
 * Coordinate system:
 *   pos_x, pos_y in Q8.8 tile units. Map is 64x64 tiles, (0,0) is NW.
 *   Y+ is south (matches Wolf3D map storage). Angle 0 = east (+X), 256 =
 *   south (+Y), 512 = west, 768 = north. ANGLES=1024. sin_q15[a] = round
 *   (sin(2*pi*a/1024) * 32767), cos via shift by ANGLE_QUAD.
 *
 * Cast algorithm (PoC step-by-fraction):
 *   For each column, advance ray position by 1/16 tile per step. When the
 *   integer tile (tx,ty) changes and that tile is a wall, take it as the
 *   hit. Side X (vertical wall face) vs side Y (horizontal wall face) is
 *   detected by which axis crossed in this step. Returns Euclidean dist
 *   and tex_x (the 0..63 horizontal offset into the wall texture column).
 *   Fish-eye correction applied via per-column cos table.
 *
 * Cursor suppression (S8 fix for VIS native arrow showing through frames):
 *   1. WNDCLASS hCursor = NULL (no class default cursor).
 *   2. WM_SETCURSOR returns SetCursor(NULL); TRUE (suppress when entering).
 *   3. ShowCursor(FALSE) post-CreateWindow (decrement global counter).
 *
 * --- Inherited from A.12 (kept) ---
 *   Per-column post-walk pattern (the wall-strip is its texture-column twin).
 *   PIT-direct hi-res clock + skip-gap (A.10.1 polish).
 *   __far placement of large BSS to keep DGROUP under 64 KB.
 *   Channel-0 click separation from IMF ch1..8.
 */
#include <conio.h>
#include <windows.h>
#include "gamepal.h"
#include "wolfvis_a13_sintab.h"

extern void FAR PASCAL hcGetCursorPos(LPPOINT lpp);

#define SCR_W        320
#define SCR_H        200

#define MAP_W        64
#define MAP_H        64
#define MAP_TILES    4096
#define NUMMAPS      100
#define CARMACK_SRC_MAX  4096
#define CARMACK_DST_MAX  8192

#define VK_HC1_DOWN      0x70
#define VK_HC1_F1        0x71
#define VK_HC1_PRIMARY   0x72
#define VK_HC1_F3        0x73
#define VK_HC1_F4        0x74
#define VK_HC1_SECONDARY 0x75
#define VK_HC1_TOOLBAR   0x76
#define VK_HC1_LEFT      0x77
#define VK_HC1_UP        0x78
#define VK_HC1_RIGHT     0x79

/* Scene layout */
#define DEBUG_BAR_H      30
#define VIEW_X0          0
#define VIEW_Y0          35
#define VIEW_W           128
#define VIEW_H           128
#define VIEW_CY          (VIEW_Y0 + VIEW_H/2)
#define MINIMAP_X0       140
#define MINIMAP_Y0       35
#define MINIMAP_TILE_PX  1
#define CEIL_COLOR       29     /* mid grey */
#define FLOOR_COLOR      24     /* darker grey */

/* VSWAP */
#define WALL_COUNT       8      /* expand to 8 so we can pick varied tex per tile */
#define CHUNKS_MAX       700
#define NUM_SPRITES      0
#define SPRITE_MAX       4096

/* Audio */
#define NUMAUDIOCHUNKS    288
#define MUSIC_SMOKE_CHUNK 261
#define MUSIC_BUF_BYTES   24000
#define MUSIC_TICK_HZ     700L

/* Trig + raycaster */
#define ANGLES        1024
#define ANGLE_QUAD    256
#define ANGLE_HALF    512
#define ANGLE_MASK    (ANGLES - 1)
#define FOV_ANGLES    192       /* ~67.5 deg total horizontal FOV */
#define ROT_STEP      32        /* ~11.25 deg per LEFT/RIGHT tap */
#define MOVE_STEP_Q88 24        /* ~3/32 tile per UP/DOWN tap */
#define MAX_CAST_STEPS 1024
#define CAST_STEP_SHIFT 4       /* sub-tile step = step_q16 >> 4 = 1/16 tile-unit */

static BYTE  framebuf[64000];
static BYTE  static_bg[64000];

/* VSWAP buffers — __far to keep DGROUP under 64 KB. */
static BYTE  __far walls[WALL_COUNT][4096];
static DWORD __far pageoffs[CHUNKS_MAX];
static WORD  __far pagelens[CHUNKS_MAX];
static WORD  chunks_in_file, sprite_start_idx, sound_start_idx;
static int   gVSwapErr = -1;

/* Map */
static BYTE  carmack_src[CARMACK_SRC_MAX];
static WORD  carmack_dst[CARMACK_DST_MAX / 2];
static WORD  __far map_plane0[MAP_TILES];
static WORD  __far map_plane1[MAP_TILES];
static WORD  maphead_rlew_tag = 0;
static DWORD __far map_headeroffs[NUMMAPS];
static WORD  map_width        = 0;
static WORD  map_height       = 0;
static int   gMapErr  = -1;
static int   gLoadErr = -1;

/* Trig: sin_q15_lut[1024] is __far const (in wolfvis_a13_sintab.h). No
 * runtime sin() — pulling Watcom's math runtime would drag in the FP
 * emulation library that requires WIN87EM.DLL at load time, which VIS
 * does not ship → "Error loading <EXE>" loop reset. Static LUT keeps the
 * EXE self-contained. */
static int   __far fov_correct[VIEW_W];   /* Q1.15 cos of column angle offset */

/* Player state in Q8.8 tile units. Y+ is south. */
static long  g_px = (long)(32L << 8) | 0x80;   /* tile 32.5 */
static long  g_py = (long)(32L << 8) | 0x80;
static int   g_pa = 0;
static long  g_px_prev = (long)(32L << 8) | 0x80;
static long  g_py_prev = (long)(32L << 8) | 0x80;
static int   g_pa_prev = 0;
static int   g_player_inited = 0;

static struct { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[256]; } bmi;
static struct { BITMAPINFOHEADER bmiHeader; WORD    bmiColors[256]; } bmiPal;

static HPALETTE gPal = NULL;
static BOOL     gPaletteRealized = FALSE;
static BYTE     gAudioOn = 0;

/* IMF audio state (port of A.10) */
static DWORD __far audio_offsets[NUMAUDIOCHUNKS + 1];
static int   gAudioHdrErr = -1;
static BYTE  __far music_buf[MUSIC_BUF_BYTES];
static UINT  gMusicLen   = 0;
static int   gMusicLoadErr = -1;
static int   gMusicChunk = -1;
static WORD *sqHack    = NULL;
static WORD *sqHackPtr = NULL;
static WORD  sqHackLen = 0;
static WORD  sqHackSeqLen = 0;
static DWORD sqHackTime = 0;
static DWORD alTimeCount = 0;
static BOOL  sqActive = FALSE;
static DWORD sqLastTick = 0;     /* unused with PIT-direct, kept for ABI */

/* PIT-direct timing state. See A.12 comment for full rationale. */
#define PIT_CYCLES_PER_IMF_TICK 852U
static WORD  prev_pit_count = 0;
static DWORD pit_accum      = 0;

/* Debug bar state */
static WORD  last_key_wparam = 0;
static WORD  key_count       = 0;
static WORD  last_msg_type   = 0;
static WORD  msg_count       = 0;
static WORD  tick_count      = 0;
static BOOL  has_focus       = FALSE;

/* ---- OPL3 routines ---- */

static void OplDelay(int cycles)
{
    while (cycles-- > 0) inp(0x388);
}

static void OplOut(BYTE reg, BYTE val)
{
    outp(0x388, reg);
    OplDelay(6);
    outp(0x389, val);
    OplDelay(35);
}

static void OplReset(void)
{
    int i;
    for (i = 0xB0; i <= 0xB8; i++) OplOut((BYTE)i, 0x00);
    for (i = 0x40; i <= 0x55; i++) OplOut((BYTE)i, 0x3F);
}

static void OplInit(void)
{
    OplReset();
    OplOut(0x20, 0x21);
    OplOut(0x40, 0x3F);
    OplOut(0x60, 0xF0);
    OplOut(0x80, 0x05);
    OplOut(0xE0, 0x00);
    OplOut(0x23, 0x21);
    OplOut(0x43, 0x00);
    OplOut(0x63, 0xF0);
    OplOut(0x83, 0x05);
    OplOut(0xE3, 0x00);
    OplOut(0xC0, 0x00);
}

static void OplNoteOn(WORD fnum, BYTE block)
{
    BYTE b;
    OplOut(0xA0, (BYTE)(fnum & 0xFF));
    b = (BYTE)(0x20 | ((block & 7) << 2) | ((fnum >> 8) & 3));
    OplOut(0xB0, b);
    gAudioOn = 1;
}

static void OplNoteOff(void)
{
    OplOut(0xB0, 0x00);
}

/* ---- PIT-direct hi-res clock ---- */

static WORD ReadPitCounter(void)
{
    BYTE lo, hi;
    outp(0x43, 0x00);
    lo = (BYTE)inp(0x40);
    hi = (BYTE)inp(0x40);
    return ((WORD)hi << 8) | (WORD)lo;
}

static void AdvanceAlTimeFromPit(void)
{
    WORD  now;
    DWORD diff;

    now = ReadPitCounter();
    if (now > prev_pit_count) {
        diff = (DWORD)prev_pit_count + (65536UL - (DWORD)now);
    } else {
        diff = (DWORD)(prev_pit_count - now);
    }
    prev_pit_count = now;
    pit_accum     += diff;

    while (pit_accum >= PIT_CYCLES_PER_IMF_TICK) {
        pit_accum   -= PIT_CYCLES_PER_IMF_TICK;
        alTimeCount += 1;
    }
}

/* ---- IMF loader + scheduler ---- */

static int LoadAudioHeader(void)
{
    HFILE f;
    OFSTRUCT of;
    UINT n;

    f = OpenFile("A:\\AUDIOHED.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;
    n = _lread(f, (LPVOID)audio_offsets, sizeof(audio_offsets));
    _lclose(f);
    if (n != sizeof(audio_offsets)) return 2;
    return 0;
}

static int LoadMusicChunk(int chunk_idx)
{
    HFILE f;
    OFSTRUCT of;
    LONG  pos;
    UINT  n;
    DWORD chunk_off, chunk_len;

    if (chunk_idx < 0 || chunk_idx >= NUMAUDIOCHUNKS) return 1;
    chunk_off = audio_offsets[chunk_idx];
    chunk_len = audio_offsets[chunk_idx + 1] - chunk_off;
    if (chunk_len < 4 || chunk_len > MUSIC_BUF_BYTES) return 2;

    f = OpenFile("A:\\AUDIOT.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 3;
    pos = _llseek(f, (LONG)chunk_off, 0);
    if (pos == -1L) { _lclose(f); return 4; }
    n = _lread(f, (LPVOID)music_buf, (UINT)chunk_len);
    _lclose(f);
    if (n != chunk_len) return 5;

    gMusicLen   = (UINT)chunk_len;
    gMusicChunk = chunk_idx;
    return 0;
}

static void OplMusicReset(void)
{
    int i;
    for (i = 0xB0; i <= 0xB8; i++) OplOut((BYTE)i, 0x00);
    OplOut(0xBD, 0x00);
}

static void StartMusic(void)
{
    WORD imf_len;

    if (gMusicLen < 4) { sqActive = FALSE; return; }
    imf_len = music_buf[0] | ((WORD)music_buf[1] << 8);
    if (imf_len == 0 || imf_len > gMusicLen - 2 || (imf_len & 3)) {
        sqActive = FALSE;
        return;
    }

    OplMusicReset();
    sqHack         = (WORD *)(music_buf + 2);
    sqHackPtr      = sqHack;
    sqHackSeqLen   = imf_len;
    sqHackLen      = imf_len;
    sqHackTime     = 0;
    alTimeCount    = 0;
    prev_pit_count = ReadPitCounter();
    pit_accum      = 0;
    sqActive       = TRUE;
    gAudioOn       = 1;
}

static void StopMusic(void)
{
    sqActive = FALSE;
    OplMusicReset();
    gAudioOn = 0;
}

static void ServiceMusic(void)
{
    WORD reg_val;
    BYTE reg, val;
    WORD delay;

    if (!sqActive) return;

    AdvanceAlTimeFromPit();
    if (alTimeCount > sqHackTime + 4) alTimeCount = sqHackTime;

    while (sqHackLen >= 4 && sqHackTime <= alTimeCount) {
        reg_val   = *sqHackPtr++;
        delay     = *sqHackPtr++;
        reg       = (BYTE)(reg_val & 0xFF);
        val       = (BYTE)(reg_val >> 8);
        OplOut(reg, val);
        sqHackTime += delay;
        sqHackLen -= 4;
    }

    if (sqHackLen < 4) {
        sqHackPtr      = sqHack;
        sqHackLen      = sqHackSeqLen;
        alTimeCount    = 0;
        sqHackTime     = 0;
        prev_pit_count = ReadPitCounter();
        pit_accum      = 0;
    }
}

/* ---- VSWAP loader (walls only — A.13 dropped sprite gallery) ---- */

static int LoadVSwap(void)
{
    HFILE f;
    OFSTRUCT of;
    WORD hdr[3];
    UINT cbOffs, cbLens;
    UINT n;
    LONG pos;
    int  i;

    f = OpenFile("A:\\VSWAP.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;

    n = _lread(f, (LPVOID)hdr, 6);
    if (n != 6) { _lclose(f); return 2; }
    chunks_in_file   = hdr[0];
    sprite_start_idx = hdr[1];
    sound_start_idx  = hdr[2];
    if (chunks_in_file == 0 || chunks_in_file > CHUNKS_MAX) { _lclose(f); return 3; }

    cbOffs = (UINT)chunks_in_file * 4U;
    cbLens = (UINT)chunks_in_file * 2U;

    n = _lread(f, (LPVOID)pageoffs, cbOffs);
    if (n != cbOffs) { _lclose(f); return 4; }
    n = _lread(f, (LPVOID)pagelens, cbLens);
    if (n != cbLens) { _lclose(f); return 5; }

    for (i = 0; i < WALL_COUNT; i++) {
        pos = _llseek(f, (LONG)pageoffs[i], 0);
        if (pos == -1L) { _lclose(f); return 6; }
        n = _lread(f, (LPVOID)walls[i], 4096);
        if (n != 4096) { _lclose(f); return 7; }
    }

    _lclose(f);
    return 0;
}

static void InitPalette(void)
{
    int i;
    bmi.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth         = SCR_W;
    bmi.bmiHeader.biHeight        = SCR_H;
    bmi.bmiHeader.biPlanes        = 1;
    bmi.bmiHeader.biBitCount      = 8;
    bmi.bmiHeader.biCompression   = BI_RGB;
    bmi.bmiHeader.biClrUsed       = 256;
    bmi.bmiHeader.biClrImportant  = 256;
    for (i = 0; i < 256; i++) {
        bmi.bmiColors[i].rgbRed      = (BYTE)(gamepal6[i*3 + 0] << 2);
        bmi.bmiColors[i].rgbGreen    = (BYTE)(gamepal6[i*3 + 1] << 2);
        bmi.bmiColors[i].rgbBlue     = (BYTE)(gamepal6[i*3 + 2] << 2);
        bmi.bmiColors[i].rgbReserved = 0;
    }
}

static void BuildPalette(void)
{
    struct { WORD ver; WORD n; PALETTEENTRY p[256]; } lp;
    int i;
    lp.ver = 0x300;
    lp.n   = 256;
    for (i = 0; i < 256; i++) {
        lp.p[i].peRed   = bmi.bmiColors[i].rgbRed;
        lp.p[i].peGreen = bmi.bmiColors[i].rgbGreen;
        lp.p[i].peBlue  = bmi.bmiColors[i].rgbBlue;
        lp.p[i].peFlags = PC_NOCOLLAPSE;
    }
    gPal = CreatePalette((LOGPALETTE FAR *)&lp);
    bmiPal.bmiHeader = bmi.bmiHeader;
    for (i = 0; i < 256; i++) bmiPal.bmiColors[i] = (WORD)i;
}

static int CarmackExpand(const BYTE far *src, int src_len, WORD far *dst, int dst_words_max)
{
    const BYTE far *sp = src;
    const BYTE far *send = src + src_len;
    WORD far *dp = dst;
    WORD far *dend = dst + dst_words_max;
    WORD ch;
    BYTE lo, hi;
    int count, offset;
    WORD far *copyptr;

    while (dp < dend && sp + 2 <= send) {
        lo = *sp++;
        hi = *sp++;
        ch = ((WORD)hi << 8) | lo;
        if (hi == 0xA7) {
            count = lo;
            if (count == 0) {
                if (sp >= send) return -1;
                *dp++ = ((WORD)0xA7 << 8) | *sp++;
            } else {
                if (sp >= send) return -1;
                offset = *sp++;
                if (offset == 0) return -2;
                copyptr = dp - offset;
                if (copyptr < dst) return -3;
                while (count-- && dp < dend) *dp++ = *copyptr++;
            }
        } else if (hi == 0xA8) {
            count = lo;
            if (count == 0) {
                if (sp >= send) return -1;
                *dp++ = ((WORD)0xA8 << 8) | *sp++;
            } else {
                WORD off;
                if (sp + 2 > send) return -1;
                off = sp[0] | ((WORD)sp[1] << 8);
                sp += 2;
                if ((int)off >= dst_words_max) return -4;
                copyptr = dst + off;
                while (count-- && dp < dend) *dp++ = *copyptr++;
            }
        } else {
            *dp++ = ch;
        }
    }
    return (int)(dp - dst);
}

static int RLEWExpand(const WORD far *src, int src_words, WORD far *dst, int dst_words, WORD rlewtag)
{
    const WORD far *sp = src;
    const WORD far *send = src + src_words;
    WORD far *dp = dst;
    WORD far *dend = dst + dst_words;
    WORD w, count, value;

    while (sp < send && dp < dend) {
        w = *sp++;
        if (w == rlewtag) {
            if (sp + 2 > send) return -1;
            count = *sp++;
            value = *sp++;
            while (count-- && dp < dend) *dp++ = value;
        } else {
            *dp++ = w;
        }
    }
    return (int)(dp - dst);
}

static int LoadMapHead(void)
{
    HFILE f;
    OFSTRUCT of;
    WORD tag;
    UINT n;

    f = OpenFile("A:\\MAPHEAD.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;
    n = _lread(f, (LPVOID)&tag, 2);
    if (n != 2) { _lclose(f); return 2; }
    maphead_rlew_tag = tag;
    n = _lread(f, (LPVOID)map_headeroffs, (UINT)(NUMMAPS * 4));
    if (n != NUMMAPS * 4) { _lclose(f); return 3; }
    _lclose(f);
    return 0;
}

static int LoadMapPlane(HFILE f, DWORD plane_start, UINT plane_len, WORD far *dst)
{
    LONG pos;
    UINT n;
    WORD carmack_expanded;
    int  carmack_words_out;
    WORD rlew_expanded;
    int  rlew_words_out;

    if (plane_len == 0 || plane_len > CARMACK_SRC_MAX) return 10;

    pos = _llseek(f, (LONG)plane_start, 0);
    if (pos == -1L) return 11;
    n = _lread(f, (LPVOID)carmack_src, plane_len);
    if (n != plane_len) return 12;

    carmack_expanded = carmack_src[0] | ((WORD)carmack_src[1] << 8);
    if (carmack_expanded == 0 || carmack_expanded > CARMACK_DST_MAX) return 13;

    carmack_words_out = CarmackExpand(
        carmack_src + 2, (int)(plane_len - 2),
        carmack_dst, carmack_expanded / 2);
    if (carmack_words_out < 2) return 14;

    rlew_expanded = carmack_dst[0];
    if (rlew_expanded != (WORD)(MAP_W * MAP_H * 2)) return 15;

    rlew_words_out = RLEWExpand(
        carmack_dst + 1, carmack_words_out - 1,
        dst, MAP_TILES, maphead_rlew_tag);
    if (rlew_words_out != MAP_TILES) return 16;

    return 0;
}

static int LoadMap(int mapnum)
{
    HFILE f;
    OFSTRUCT of;
    UINT n;
    LONG pos;
    DWORD hdr_off;
    struct {
        DWORD planestart[3];
        WORD  planelength[3];
        WORD  width, height;
        char  name[16];
    } maptype_buf;
    int rc;

    if (mapnum < 0 || mapnum >= NUMMAPS) return 1;
    hdr_off = map_headeroffs[mapnum];
    if (hdr_off == 0 || hdr_off == 0xFFFFFFFFUL) return 2;

    f = OpenFile("A:\\GAMEMAPS.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 3;

    pos = _llseek(f, (LONG)hdr_off, 0);
    if (pos == -1L) { _lclose(f); return 4; }
    n = _lread(f, (LPVOID)&maptype_buf, 38);
    if (n != 38) { _lclose(f); return 5; }

    map_width  = maptype_buf.width;
    map_height = maptype_buf.height;
    if (map_width != MAP_W || map_height != MAP_H) { _lclose(f); return 6; }

    rc = LoadMapPlane(f, maptype_buf.planestart[0], maptype_buf.planelength[0], map_plane0);
    if (rc) { _lclose(f); return 20 + rc; }
    rc = LoadMapPlane(f, maptype_buf.planestart[1], maptype_buf.planelength[1], map_plane1);
    if (rc) { _lclose(f); return 40 + rc; }

    _lclose(f);
    return 0;
}

/* ---- Trig table init ---- */

static void InitTrig(void)
{
    int i;
    for (i = 0; i < VIEW_W; i++) {
        int off = ((i - VIEW_W/2) * FOV_ANGLES) / VIEW_W;
        int idx = (off + ANGLES) & ANGLE_MASK;
        /* cos(off) = sin_q15_lut[off + ANGLE_QUAD] */
        idx = (idx + ANGLE_QUAD) & ANGLE_MASK;
        fov_correct[i] = sin_q15_lut[idx];
    }
}

#define SIN_Q15(a) (sin_q15_lut[(a) & ANGLE_MASK])
#define COS_Q15(a) (sin_q15_lut[((a) + ANGLE_QUAD) & ANGLE_MASK])

/* ---- IsWall + InitPlayer ---- */

static int IsWall(int tx, int ty)
{
    WORD tile;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 1;
    tile = map_plane0[ty * MAP_W + tx];
    /* Wolf3D wall tiles: 1..63. Doors 90..101 also block (treat as walls). */
    if (tile >= 1 && tile <= 63)   return 1;
    if (tile >= 90 && tile <= 101) return 1;
    return 0;
}

/* Pick wall texture for a map tile. plane0 wall ids 1..63 directly map onto
 * VSWAP wall pages 0..N-1 (Wolf3D convention is wall_page = (tile-1)*2 for
 * the light face, +1 for the dark face). For PoC we ignore light/dark and
 * just modulo into our loaded WALL_COUNT bank. */
static int TileToWallTex(WORD tile)
{
    if (tile == 0) return 0;
    /* (tile-1)*2 is the canonical light-face. Modulo into loaded set. */
    return (int)(((tile - 1) * 2) % WALL_COUNT);
}

static void InitPlayer(void)
{
    int tx, ty;
    WORD obj;
    int found = 0;

    if (gMapErr != 0) {
        /* No map — fallback to map center, angle 0. */
        g_px = (32L << 8) | 0x80;
        g_py = (32L << 8) | 0x80;
        g_pa = 0;
        return;
    }

    /* First pass: look for plane1 spawn markers 19=N, 20=E, 21=S, 22=W. */
    for (ty = 0; ty < MAP_H && !found; ty++) {
        for (tx = 0; tx < MAP_W && !found; tx++) {
            obj = map_plane1[ty * MAP_W + tx];
            if (obj >= 19 && obj <= 22 && !IsWall(tx, ty)) {
                g_px = ((long)tx << 8) | 0x80;
                g_py = ((long)ty << 8) | 0x80;
                /* 19=N(-Y), 20=E(+X), 21=S(+Y), 22=W(-X). Our angle conv:
                 * 0=E, 256=S, 512=W, 768=N. */
                switch (obj) {
                case 19: g_pa = 768; break;
                case 20: g_pa = 0;   break;
                case 21: g_pa = 256; break;
                case 22: g_pa = 512; break;
                }
                found = 1;
            }
        }
    }

    /* Fallback: first non-wall tile, facing east. */
    if (!found) {
        for (ty = 0; ty < MAP_H && !found; ty++) {
            for (tx = 0; tx < MAP_W && !found; tx++) {
                if (!IsWall(tx, ty)) {
                    g_px = ((long)tx << 8) | 0x80;
                    g_py = ((long)ty << 8) | 0x80;
                    g_pa = 0;
                    found = 1;
                }
            }
        }
    }

    g_px_prev = g_px;
    g_py_prev = g_py;
    g_pa_prev = g_pa;
    g_player_inited = 1;
}

/* ---- Drawing primitives ---- */

static void ClearFrame(void)
{
    BYTE *p = framebuf;
    unsigned n;
    for (n = 0; n < 64000U; n++) *p++ = 0;
}

static void FB_Put(int sx, int sy, BYTE pix)
{
    int fb_y;
    if (sx < 0 || sx >= SCR_W || sy < 0 || sy >= SCR_H) return;
    fb_y = (SCR_H - 1) - sy;
    framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)sx] = pix;
}

static void FB_FillRect(int x, int y, int w, int h, BYTE pix)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            FB_Put(x + i, y + j, pix);
}

static BYTE TileToColor(WORD tile)
{
    if (tile == 0) return 31;
    if (tile == 21 || tile == 22) return 15;
    if (tile >= 90 && tile <= 101) return (BYTE)176;
    if (tile <= 63) return (BYTE)(64 + tile * 3);
    return (BYTE)(tile & 0xFF);
}

static BYTE ObjectToColor(WORD obj)
{
    if (obj == 0) return 0;
    if (obj >= 19 && obj <= 22) return 14;
    if (obj >= 23 && obj <= 74) return 135;
    if (obj >= 108 && obj <= 115) return 42;
    if (obj >= 116 && obj <= 127) return 44;
    return 127;
}

static void DrawMinimapWithPlayer(void)
{
    int tx, ty;
    WORD tile, obj;
    BYTE wc, oc;
    int  ppx, ppy, hx, hy;

    /* Background tiles + objects */
    for (ty = 0; ty < MAP_H; ty++) {
        for (tx = 0; tx < MAP_W; tx++) {
            tile = map_plane0[ty * MAP_W + tx];
            obj  = map_plane1[ty * MAP_W + tx];
            wc = TileToColor(tile);
            oc = ObjectToColor(obj);
            FB_Put(MINIMAP_X0 + tx, MINIMAP_Y0 + ty, oc ? oc : wc);
        }
    }

    /* Player position dot (3x3 cyan), heading line 4 px in cos/sin direction. */
    ppx = MINIMAP_X0 + (int)(g_px >> 8);
    ppy = MINIMAP_Y0 + (int)(g_py >> 8);
    {
        int dx, dy;
        for (dy = -1; dy <= 1; dy++)
            for (dx = -1; dx <= 1; dx++)
                FB_Put(ppx + dx, ppy + dy, 14);
    }
    /* Heading: 4 px line. Our angle 0=E (+X), 256=S (+Y). */
    hx = ppx + (int)(((long)COS_Q15(g_pa) * 4L) >> 15);
    hy = ppy + (int)(((long)SIN_Q15(g_pa) * 4L) >> 15);
    FB_Put(hx, hy, 15);
    /* Halfway dot to make the line readable on 1px-per-tile minimap. */
    FB_Put((ppx + hx) / 2, (ppy + hy) / 2, 15);
}

static void DrawCrosshair(void)
{
    int cx = VIEW_X0 + VIEW_W/2;
    int cy = VIEW_Y0 + VIEW_H/2;
    int i;
    for (i = -3; i <= 3; i++) {
        if (i < -1 || i > 1) {
            FB_Put(cx + i, cy, 15);
            FB_Put(cx, cy + i, 15);
        }
    }
}

/* ---- Raycaster ---- */

/* Cast a ray from g_px,g_py at angle ra. Step-by-fraction (1/16 tile per
 * step) DDA approximation — robust for PoC, will swap to grid-line DDA in
 * a follow-up if perf demands. Returns Euclidean distance in Q8.8 tile
 * units; out_tex_idx is the WALL_COUNT index, out_tex_x is 0..63. */
static long CastRay(int ra, int *out_tex_idx, int *out_tex_x)
{
    long pos_x_q16 = g_px << 8;     /* Q16.16 */
    long pos_y_q16 = g_py << 8;
    long step_dx_q16 = (long)COS_Q15(ra) << 1;   /* Q16.16, max ~1.0 tile */
    long step_dy_q16 = (long)SIN_Q15(ra) << 1;
    long sub_dx = step_dx_q16 >> CAST_STEP_SHIFT;  /* 1/16 tile */
    long sub_dy = step_dy_q16 >> CAST_STEP_SHIFT;
    int  prev_tx = (int)(pos_x_q16 >> 16);
    int  prev_ty = (int)(pos_y_q16 >> 16);
    int  tx = prev_tx, ty = prev_ty;
    int  side = 0;     /* 0 = X-side (vertical wall face), 1 = Y-side */
    int  steps;
    long dx_total, dy_total, dist_q88;
    WORD hit_tile;

    *out_tex_idx = 0;
    *out_tex_x   = 0;

    for (steps = 0; steps < MAX_CAST_STEPS; steps++) {
        pos_x_q16 += sub_dx;
        pos_y_q16 += sub_dy;
        tx = (int)(pos_x_q16 >> 16);
        ty = (int)(pos_y_q16 >> 16);
        if (tx != prev_tx || ty != prev_ty) {
            /* Determine which axis crossed. If both, pick the one that
             * would have been crossed first by examining sub-step
             * fractional position. Approximation: if abs(frac_y - 0.5)
             * is larger after step than abs(frac_x - 0.5), Y crossed
             * first → X-side hit. */
            if (tx != prev_tx && ty != prev_ty) {
                long fx = pos_x_q16 & 0xFFFFL;
                long fy = pos_y_q16 & 0xFFFFL;
                long ax = (sub_dx > 0) ? fx : (0x10000L - fx);
                long ay = (sub_dy > 0) ? fy : (0x10000L - fy);
                /* The axis with smaller "distance into the new tile"
                 * is the one that crossed last → the OTHER is the side. */
                side = (ax < ay) ? 0 : 1;
            } else if (tx != prev_tx) {
                side = 0;
            } else {
                side = 1;
            }
            if (IsWall(tx, ty)) {
                hit_tile = map_plane0[ty * MAP_W + tx];
                *out_tex_idx = TileToWallTex(hit_tile);
                /* tex_x: for X-side, fractional Y of hit point. For
                 * Y-side, fractional X. Converted to 0..63. */
                if (side == 0) {
                    long fy = pos_y_q16 & 0xFFFFL;
                    *out_tex_x = (int)((fy * 64L) >> 16);
                    if (sub_dx < 0) *out_tex_x = 63 - *out_tex_x;
                } else {
                    long fx = pos_x_q16 & 0xFFFFL;
                    *out_tex_x = (int)((fx * 64L) >> 16);
                    if (sub_dy > 0) *out_tex_x = 63 - *out_tex_x;
                }
                if (*out_tex_x < 0)   *out_tex_x = 0;
                if (*out_tex_x > 63)  *out_tex_x = 63;
                /* Euclidean distance in Q8.8. */
                dx_total = pos_x_q16 - (g_px << 8);
                dy_total = pos_y_q16 - (g_py << 8);
                /* Use just X or Y component depending on side — this
                 * is naturally perpendicular to that wall face and so
                 * is already partially fish-eye corrected. We finish
                 * the correction in DrawViewport via fov_correct[]. */
                if (side == 0) {
                    dist_q88 = (dx_total >> 8);
                    if (dist_q88 < 0) dist_q88 = -dist_q88;
                    if (step_dx_q16 != 0) {
                        /* dist = dx / cos(ra). dx_total is along ray
                         * by sub_dx units; project using ray dir. */
                        dist_q88 = ((dx_total >> 8) * 32767L) /
                                   (step_dx_q16 >> 1);
                        if (dist_q88 < 0) dist_q88 = -dist_q88;
                    }
                } else {
                    if (step_dy_q16 != 0) {
                        dist_q88 = ((dy_total >> 8) * 32767L) /
                                   (step_dy_q16 >> 1);
                        if (dist_q88 < 0) dist_q88 = -dist_q88;
                    } else {
                        dist_q88 = (dy_total >> 8);
                        if (dist_q88 < 0) dist_q88 = -dist_q88;
                    }
                }
                if (dist_q88 < 16) dist_q88 = 16;   /* clamp very-near */
                return dist_q88;
            }
            prev_tx = tx;
            prev_ty = ty;
        }
    }
    /* Ran out of steps — return a far distance with default texture. */
    return (long)(64L << 8);
}

static void DrawWallStripCol(int col, long perp_dist_q88, int tex_idx, int tex_x)
{
    long wall_h_long;
    int  wall_h, dy_top, dy_bot, sx, dy, sy_src, fb_y;
    BYTE *texcol;
    long sy_acc, sy_step;
    int  tex_idx_clamped;

    if (perp_dist_q88 < 16) perp_dist_q88 = 16;
    wall_h_long = ((long)VIEW_H << 8) / perp_dist_q88;
    wall_h = (int)wall_h_long;
    if (wall_h < 1)  wall_h = 1;
    if (wall_h > 4 * VIEW_H) wall_h = 4 * VIEW_H;

    dy_top = VIEW_CY - wall_h / 2;
    dy_bot = dy_top + wall_h;

    sx = VIEW_X0 + col;
    if (sx < 0 || sx >= SCR_W) return;

    tex_idx_clamped = tex_idx;
    if (tex_idx_clamped < 0)              tex_idx_clamped = 0;
    if (tex_idx_clamped >= WALL_COUNT)    tex_idx_clamped = WALL_COUNT - 1;
    texcol = walls[tex_idx_clamped] + ((unsigned)tex_x * 64U);

    /* Ceiling */
    for (dy = VIEW_Y0; dy < dy_top && dy < VIEW_Y0 + VIEW_H; dy++) {
        if (dy < 0 || dy >= SCR_H) continue;
        fb_y = (SCR_H - 1) - dy;
        framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)sx] = CEIL_COLOR;
    }
    /* Wall */
    sy_step = (64L << 16) / wall_h;
    /* If dy_top is above viewport, advance acc into texture by clipped delta. */
    if (dy_top < VIEW_Y0) {
        sy_acc = sy_step * (long)(VIEW_Y0 - dy_top);
        dy = VIEW_Y0;
    } else {
        sy_acc = 0;
        dy = dy_top;
    }
    for (; dy < dy_bot && dy < VIEW_Y0 + VIEW_H; dy++) {
        if (dy < 0 || dy >= SCR_H) { sy_acc += sy_step; continue; }
        sy_src = (int)(sy_acc >> 16);
        if (sy_src < 0)  sy_src = 0;
        if (sy_src > 63) sy_src = 63;
        fb_y = (SCR_H - 1) - dy;
        framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)sx] = texcol[sy_src];
        sy_acc += sy_step;
    }
    /* Floor */
    for (dy = (dy_bot > VIEW_Y0 ? dy_bot : VIEW_Y0); dy < VIEW_Y0 + VIEW_H; dy++) {
        if (dy < 0 || dy >= SCR_H) continue;
        fb_y = (SCR_H - 1) - dy;
        framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)sx] = FLOOR_COLOR;
    }
}

static void DrawViewport(void)
{
    int  col;
    int  ra, half_fov_a;
    int  tex_idx, tex_x;
    long dist_q88, perp_dist;

    for (col = 0; col < VIEW_W; col++) {
        half_fov_a = ((col - VIEW_W/2) * FOV_ANGLES) / VIEW_W;
        ra = (g_pa + half_fov_a) & ANGLE_MASK;
        dist_q88 = CastRay(ra, &tex_idx, &tex_x);
        /* Fish-eye correction: multiply by cos(half_fov_a). */
        perp_dist = (dist_q88 * (long)fov_correct[col]) >> 15;
        if (perp_dist < 16) perp_dist = 16;
        DrawWallStripCol(col, perp_dist, tex_idx, tex_x);
    }
}

static void DrawBitGrid(int sx, int sy, int h, WORD val, BYTE lit, BYTE unlit)
{
    int i;
    for (i = 0; i < 16; i++) {
        BYTE c = (val & ((WORD)1 << i)) ? lit : unlit;
        FB_FillRect(sx + i * 18, sy, 16, h, c);
    }
}

static void DrawDebugBar(void)
{
    BYTE hb = (BYTE)((tick_count & 1) ? 42 : 15);
    FB_FillRect(0, 0, SCR_W, 30, 8);
    FB_FillRect(2, 1, 28, 6, hb);
    FB_FillRect(34, 1, 10, 6, has_focus ? 105 : 31);
    FB_FillRect(50, 1, 14, 6, gMapErr == 0 ? 105 : (gMapErr > 0 ? 42 : 15));
    FB_FillRect(68, 1, 14, 6, gVSwapErr == 0 ? 105 : (gVSwapErr > 0 ? 42 : 15));
    FB_FillRect(86, 1, 14, 6, gAudioOn ? 14 : 31);
    FB_FillRect(220, 1, 40, 6, (BYTE)((msg_count & 0x07) ? 42 : 15));
    FB_FillRect(266, 1, 40, 6, (BYTE)((key_count & 0x07) ? 105 : 15));
    DrawBitGrid(16, 10, 7, last_msg_type, 42, 15);
    DrawBitGrid(16, 19, 7, last_key_wparam, 143, 15);
}

static void SetupStaticBg(void)
{
    unsigned i;
    ClearFrame();
    /* Debug bar area only — viewport and minimap are drawn dynamically. */
    DrawDebugBar();
    for (i = 0; i < 64000U; i++) static_bg[i] = framebuf[i];
}

/* Try to move player by (dx, dy) in Q8.8 tile units. Reject if would
 * step into a wall (separately on each axis so we slide along walls). */
static void TryMovePlayer(long dx_q88, long dy_q88)
{
    long nx = g_px + dx_q88;
    long ny = g_py + dy_q88;
    int  tx_check, ty_check;

    /* X axis */
    tx_check = (int)(nx >> 8);
    ty_check = (int)(g_py >> 8);
    if (!IsWall(tx_check, ty_check)) g_px = nx;

    /* Y axis */
    tx_check = (int)(g_px >> 8);
    ty_check = (int)(ny >> 8);
    if (!IsWall(tx_check, ty_check)) g_py = ny;
}

long FAR PASCAL _export WolfVisWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    HDC         hdc;

    switch (msg) {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        if (gPal) {
            SelectPalette(hdc, gPal, FALSE);
            if (!gPaletteRealized) { RealizePalette(hdc); gPaletteRealized = TRUE; }
        }
        StretchDIBits(hdc, 0, 0, SCR_W, SCR_H, 0, 0, SCR_W, SCR_H,
            framebuf, (BITMAPINFO FAR *)&bmiPal, DIB_PAL_COLORS, SRCCOPY);
        EndPaint(hWnd, &ps);
        return 0;

    case WM_SETCURSOR:
        /* Suppress VIS native cursor over our client area. The arrow
         * the player would otherwise see blinking through our framebuf
         * is the system cursor MW renders for the hand controller. */
        SetCursor(NULL);
        return TRUE;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        long dx_q88, dy_q88;
        RECT dirty;
        BOOL moved = FALSE, rotated = FALSE;

        last_key_wparam = (WORD)wp;
        last_msg_type   = (WORD)msg;
        key_count++;
        msg_count++;
        switch (wp) {
        case VK_HC1_UP:
            /* Forward along heading. dx = cos(g_pa) * MOVE_STEP, dy = sin. */
            dx_q88 = ((long)COS_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15;
            dy_q88 = ((long)SIN_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15;
            TryMovePlayer(dx_q88, dy_q88);
            moved = TRUE;
            break;
        case VK_HC1_DOWN:
            dx_q88 = -(((long)COS_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15);
            dy_q88 = -(((long)SIN_Q15(g_pa) * (long)MOVE_STEP_Q88) >> 15);
            TryMovePlayer(dx_q88, dy_q88);
            moved = TRUE;
            break;
        case VK_HC1_LEFT:
            g_pa = (g_pa - ROT_STEP) & ANGLE_MASK;
            rotated = TRUE;
            break;
        case VK_HC1_RIGHT:
            g_pa = (g_pa + ROT_STEP) & ANGLE_MASK;
            rotated = TRUE;
            break;
        case VK_HC1_PRIMARY:
            OplNoteOn(0x300, 5);
            break;
        case VK_HC1_SECONDARY:
            OplNoteOn(0x244, 4);
            OplNoteOff();
            break;
        case VK_HC1_F1:
            if (!sqActive && gMusicLoadErr == 0) {
                OplInit();
                StartMusic();
            }
            break;
        case VK_HC1_F3:
            StopMusic();
            break;
        default: break;
        }

        if (moved || rotated) {
            DrawViewport();
            DrawCrosshair();
            DrawMinimapWithPlayer();
            dirty.left   = VIEW_X0;
            dirty.top    = VIEW_Y0;
            dirty.right  = MINIMAP_X0 + MAP_W;
            dirty.bottom = VIEW_Y0 + VIEW_H;
            InvalidateRect(hWnd, &dirty, FALSE);
            g_px_prev = g_px;
            g_py_prev = g_py;
            g_pa_prev = g_pa;
        }
        return 0;
    }

    case WM_TIMER: {
        RECT  dirty;
        POINT pt;
        tick_count++;
        has_focus = (GetFocus() == hWnd);
        /* Pump HC cursor pos to keep MW HC dispatcher routing keys. The
         * returned coords are unused — we just need the side effect. */
        pt.x = 0; pt.y = 0;
        hcGetCursorPos((LPPOINT)&pt);
        DrawDebugBar();
        dirty.left   = 0;
        dirty.top    = 0;
        dirty.right  = SCR_W;
        dirty.bottom = DEBUG_BAR_H;
        InvalidateRect(hWnd, &dirty, FALSE);
        return 0;
    }

    case WM_SETFOCUS:   has_focus = TRUE;  return 0;
    case WM_KILLFOCUS:  has_focus = FALSE; return 0;

    case WM_QUERYNEWPALETTE:
        gPaletteRealized = FALSE;
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;

    case WM_PALETTECHANGED:
        if ((HWND)wp != hWnd) {
            gPaletteRealized = FALSE;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;

    case WM_CREATE: {
        HDC dc = GetDC(hWnd);
        if (gPal) { SelectPalette(dc, gPal, FALSE); RealizePalette(dc); }
        ReleaseDC(hWnd, dc);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    HWND     hWnd;
    MSG      msg;
    (void)cmd;

    InitPalette();
    BuildPalette();
    InitTrig();

    gLoadErr = LoadMapHead();
    if (gLoadErr == 0) gMapErr = LoadMap(0);
    else               gMapErr = 100 + gLoadErr;

    gVSwapErr = LoadVSwap();

    gAudioHdrErr = LoadAudioHeader();
    if (gAudioHdrErr == 0) {
        gMusicLoadErr = LoadMusicChunk(MUSIC_SMOKE_CHUNK);
        if (gMusicLoadErr == 0) OplInit();
    }

    InitPlayer();

    SetupStaticBg();
    /* Initial render of the dynamic layers so the first WM_PAINT shows
     * the viewport + minimap, not a blank panel. */
    DrawViewport();
    DrawCrosshair();
    DrawMinimapWithPlayer();

    if (!hPrev) {
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WolfVisWndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hInst;
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        /* Cursor suppression point #1: no class default cursor. */
        wc.hCursor       = NULL;
        wc.hbrBackground = GetStockObject(BLACK_BRUSH);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = "WolfVISa13";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa13", "WolfVISa13",
        WS_POPUP | WS_VISIBLE,
        0, 0, 640, 480,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 0;

    /* Cursor suppression point #3: backstop ShowCursor counter. */
    ShowCursor(FALSE);

    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);
    SetFocus(hWnd);
    SetActiveWindow(hWnd);
    SetTimer(hWnd, 1, 500, NULL);

    for (;;) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (sqActive) ServiceMusic();
        } else if (sqActive) {
            ServiceMusic();
        } else {
            WaitMessage();
        }
    }
}
