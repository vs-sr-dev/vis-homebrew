/*
 * WolfVIS A.11 — Integrated demo scene.
 *
 * First milestone where every Wolf3D PoC primitive proven in A.1..A.10
 * runs together in one composited frame:
 *
 *   y=0..29   : debug bar (heartbeat + indicators, repainted on WM_TIMER)
 *   y=35..98  : 4 wall textures (x=0..255) + 64x64 minimap E1L1 (x=256..319)
 *   y=99..104 : black gutter
 *   y=105..168: 3 sprite gallery (x=15, 125, 235)
 *   y=169..199: free band, cursor allowed
 *
 * Static layer (walls + minimap + sprites) is rendered once after asset
 * load, snapshotted into static_bg[64000], and reused for cursor erase
 * via RestoreFromBg (A.9 pattern). Cursor d-pad input + IMF music
 * background + per-keypress click tones are dynamic.
 *
 * Asset pipeline used end-to-end: GAMEPAL palette (A.3), VSWAP walls
 * (A.4) + sprites (A.5), HC.DLL input (A.6), GAMEMAPS Carmack/RLEW
 * (A.7), OPL3 click tones (A.8), perf erase/redraw (A.9), AUDIOT IMF
 * music scheduler (A.10).
 *
 * Controls (HC1 hand controller):
 *   d-pad        : move cursor
 *   F1           : init OPL3 + start music (combined one-touch)
 *   F3           : stop music
 *   A (PRIMARY)  : high click tone
 *   B (SECONDARY): mid click tone (note-off after a release)
 *
 * Memory note: VSWAP loader needs ~30 KB more BSS than A.10 (walls +
 * sprites + chunk tables). walls[]/sprites[] are __far to keep them
 * out of DGROUP — same DGROUP-overflow guard as music_buf in A.10.
 */
#include <conio.h>
#include <windows.h>
#include "gamepal.h"

extern void FAR PASCAL hcGetCursorPos(LPPOINT lpp);

#define SCR_W        320
#define SCR_H        200

#define MAP_W        64
#define MAP_H        64
#define MAP_TILES    4096
#define NUMMAPS      100
#define CARMACK_SRC_MAX  4096
#define CARMACK_DST_MAX  8192

#define CUR_STEP     4

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
#define WALL_TOP_Y       35
#define WALL_COUNT       4
#define MINIMAP_X0       256
#define MINIMAP_Y0       35
#define MINIMAP_TILE_PX  1
#define SPRITE_TOP_Y     105
#define SPRITE_X0        15
#define SPRITE_X1        125
#define SPRITE_X2        235
#define DEBUG_BAR_H      30

/* VSWAP */
#define CHUNKS_MAX       700
#define NUM_SPRITES      3
#define SPRITE_MAX       4096

/* Audio */
#define NUMAUDIOCHUNKS    288
#define MUSIC_SMOKE_CHUNK 261
#define MUSIC_BUF_BYTES   24000
#define MUSIC_TICK_HZ     700L

static BYTE  framebuf[64000];
static BYTE  static_bg[64000];

/* VSWAP buffers — __far to keep DGROUP under 64 KB. */
static BYTE  __far walls[WALL_COUNT][4096];
static BYTE  __far sprites[NUM_SPRITES][SPRITE_MAX];
static WORD  sprite_len[NUM_SPRITES];
static DWORD __far pageoffs[CHUNKS_MAX];
static WORD  __far pagelens[CHUNKS_MAX];
static WORD  chunks_in_file, sprite_start_idx, sound_start_idx;
static int   gVSwapErr = -1;

/* Map */
static BYTE  carmack_src[CARMACK_SRC_MAX];
static WORD  carmack_dst[CARMACK_DST_MAX / 2];
static WORD  map_plane0[MAP_TILES];
static WORD  map_plane1[MAP_TILES];
static WORD  maphead_rlew_tag = 0;
static DWORD map_headeroffs[NUMMAPS];
static WORD  map_width        = 0;
static WORD  map_height       = 0;
static char  map_name[20]     = {0};

static struct { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[256]; } bmi;
static struct { BITMAPINFOHEADER bmiHeader; WORD    bmiColors[256]; } bmiPal;

static HPALETTE gPal = NULL;
static BOOL     gPaletteRealized = FALSE;
static int      gLoadErr = -1;
static int      gMapErr  = -1;
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
static DWORD sqLastTick = 0;

/* Cursor + debug bar */
static int   cursor_x       = 160;
static int   cursor_y       = 100;
static int   cursor_prev_x  = 160;
static int   cursor_prev_y  = 100;
static BYTE  cursor_color   = 42;
#define      CURSOR_HALF    5
static WORD  last_key_wparam = 0;
static WORD  key_count       = 0;
static WORD  last_msg_type   = 0;
static WORD  msg_count       = 0;
static WORD  tick_count      = 0;
static BOOL  has_focus       = FALSE;

/* ---- OPL3 routines (A.8) ---- */

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

/* ---- IMF loader + scheduler (A.10) ---- */

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
    sqHack       = (WORD *)(music_buf + 2);
    sqHackPtr    = sqHack;
    sqHackSeqLen = imf_len;
    sqHackLen    = imf_len;
    sqHackTime   = 0;
    alTimeCount  = 0;
    sqLastTick   = GetTickCount();
    sqActive     = TRUE;
    gAudioOn     = 1;
}

static void StopMusic(void)
{
    sqActive = FALSE;
    OplMusicReset();
    gAudioOn = 0;
}

static void ServiceMusic(void)
{
    DWORD now, elapsed_ms, ticks_advance;
    WORD  reg_val;
    BYTE  reg, val;
    WORD  delay;

    if (!sqActive) return;

    now        = GetTickCount();
    elapsed_ms = now - sqLastTick;
    sqLastTick = now;
    ticks_advance = (elapsed_ms * MUSIC_TICK_HZ) / 1000UL;
    if (ticks_advance == 0) return;
    alTimeCount += ticks_advance;

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
        sqHackPtr   = sqHack;
        sqHackLen   = sqHackSeqLen;
        alTimeCount = 0;
        sqHackTime  = 0;
    }
}

/* ---- VSWAP loader (A.4 walls + A.5 sprites) ---- */

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
    if (sprite_start_idx >= chunks_in_file)                 { _lclose(f); return 3; }

    cbOffs = (UINT)chunks_in_file * 4U;
    cbLens = (UINT)chunks_in_file * 2U;

    n = _lread(f, (LPVOID)pageoffs, cbOffs);
    if (n != cbOffs) { _lclose(f); return 4; }
    n = _lread(f, (LPVOID)pagelens, cbLens);
    if (n != cbLens) { _lclose(f); return 5; }

    /* Walls 0..WALL_COUNT-1: 4096 B each, col-major 64x64. */
    for (i = 0; i < WALL_COUNT; i++) {
        pos = _llseek(f, (LONG)pageoffs[i], 0);
        if (pos == -1L) { _lclose(f); return 6; }
        n = _lread(f, (LPVOID)walls[i], 4096);
        if (n != 4096) { _lclose(f); return 7; }
    }

    /* Sprites: 3 chunks starting at sprite_start_idx. */
    for (i = 0; i < NUM_SPRITES; i++) {
        WORD chunk = (WORD)(sprite_start_idx + i);
        WORD len   = pagelens[chunk];
        sprite_len[i] = 0;
        if (len == 0) continue;
        if (len > SPRITE_MAX) { _lclose(f); return 8; }
        pos = _llseek(f, (LONG)pageoffs[chunk], 0);
        if (pos == -1L) { _lclose(f); return 9; }
        n = _lread(f, (LPVOID)sprites[i], len);
        if (n != len) { _lclose(f); return 10; }
        sprite_len[i] = len;
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
    {
        int i;
        for (i = 0; i < 16; i++) map_name[i] = maptype_buf.name[i];
        map_name[16] = 0;
    }
    if (map_width != MAP_W || map_height != MAP_H) { _lclose(f); return 6; }

    rc = LoadMapPlane(f, maptype_buf.planestart[0], maptype_buf.planelength[0], map_plane0);
    if (rc) { _lclose(f); return 20 + rc; }
    rc = LoadMapPlane(f, maptype_buf.planestart[1], maptype_buf.planelength[1], map_plane1);
    if (rc) { _lclose(f); return 40 + rc; }

    _lclose(f);
    return 0;
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

/* Compressed minimap: 64x64 px (TILE_PX=1), no border, top-right corner. */
static void DrawMinimapCompressed(void)
{
    int tx, ty;
    WORD tile, obj;
    BYTE wc, oc;

    for (ty = 0; ty < MAP_H; ty++) {
        for (tx = 0; tx < MAP_W; tx++) {
            tile = map_plane0[ty * MAP_W + tx];
            obj  = map_plane1[ty * MAP_W + tx];
            wc = TileToColor(tile);
            oc = ObjectToColor(obj);
            FB_Put(MINIMAP_X0 + tx, MINIMAP_Y0 + ty, oc ? oc : wc);
        }
    }
}

/* Wall strip: WALL_COUNT walls drawn left-to-right at y=WALL_TOP_Y. Bottom-up
 * DIB so we walk fb-row pointer upward (subtract SCR_W) per wall row. */
static void DrawWallStrip(void)
{
    int w, col, row, sx, fb_y0;
    BYTE *rowptr;

    for (w = 0; w < WALL_COUNT; w++) {
        for (col = 0; col < 64; col++) {
            sx = w * 64 + col;
            if (sx >= SCR_W) break;
            fb_y0 = (SCR_H - 1) - WALL_TOP_Y;
            rowptr = &framebuf[(unsigned)fb_y0 * (unsigned)SCR_W + (unsigned)sx];
            for (row = 0; row < 64; row++) {
                *rowptr = walls[w][col * 64 + row];
                rowptr -= SCR_W;
            }
        }
    }
}

/* DrawSprite from A.5 — 1:1 blit, opaque-only via post lists. */
static void DrawSprite(int idx, int dst_x, int dst_y)
{
    BYTE *sprite;
    WORD leftpix, rightpix;
    WORD far *dataofs;
    WORD far *post;
    int  col, y, sx, sy;
    WORD starty, endy, corr_top, src_idx;

    if (sprite_len[idx] == 0) return;
    sprite = sprites[idx];

    leftpix  = *(WORD far *)(sprite + 0);
    rightpix = *(WORD far *)(sprite + 2);
    if (leftpix > 63 || rightpix > 63 || leftpix > rightpix) return;

    dataofs = (WORD far *)(sprite + 4);

    for (col = (int)leftpix; col <= (int)rightpix; col++) {
        WORD col_ofs = dataofs[col - (int)leftpix];
        if (col_ofs >= SPRITE_MAX) continue;
        post = (WORD far *)(sprite + col_ofs);

        sx = dst_x + col;
        if (sx < 0 || sx >= SCR_W) continue;

        while (post[0] != 0) {
            endy     = (WORD)(post[0] >> 1);
            corr_top = post[1];
            starty   = (WORD)(post[2] >> 1);
            if (endy > 64 || starty > endy) break;
            for (y = (int)starty; y < (int)endy; y++) {
                int fb_y;
                sy = dst_y + y;
                if (sy < 0 || sy >= SCR_H) continue;
                src_idx = (WORD)(corr_top + (WORD)y);
                if (src_idx >= SPRITE_MAX) continue;
                fb_y = (SCR_H - 1) - sy;
                framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)sx] = sprite[src_idx];
            }
            post += 3;
        }
    }
}

static void DrawCursor(int cx, int cy)
{
    int i;
    BYTE c = cursor_color;
    for (i = -4; i <= 4; i++) FB_Put(cx, cy + i, c);
    for (i = -4; i <= 4; i++) FB_Put(cx + i, cy, c);
    FB_Put(cx - 4, cy - 4, 15);
    FB_Put(cx + 4, cy - 4, 15);
    FB_Put(cx - 4, cy + 4, 15);
    FB_Put(cx + 4, cy + 4, 15);
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

static void RestoreFromBg(int x, int y, int w, int h)
{
    int j;
    int sy, fb_y;
    unsigned off;
    BYTE *dst;
    const BYTE *src;
    int copy_w;

    if (w <= 0 || h <= 0) return;
    if (x < 0)        { w += x; x = 0; }
    if (y < 0)        { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;

    for (j = 0; j < h; j++) {
        sy   = y + j;
        fb_y = (SCR_H - 1) - sy;
        off  = (unsigned)fb_y * (unsigned)SCR_W + (unsigned)x;
        dst  = framebuf + off;
        src  = static_bg + off;
        for (copy_w = w; copy_w; copy_w--) *dst++ = *src++;
    }
}

/* Composite the static layer once: walls + minimap + sprites. Snapshot to
 * static_bg for cursor erase lookups. Error states (no VSWAP, no map) fall
 * back to a tinted strip so the bug is visible at runtime instead of a
 * mysterious black scene. */
static void SetupStaticBg(void)
{
    unsigned i;

    ClearFrame();

    if (gVSwapErr == 0) {
        DrawWallStrip();
        DrawSprite(0, SPRITE_X0, SPRITE_TOP_Y);
        DrawSprite(1, SPRITE_X1, SPRITE_TOP_Y);
        DrawSprite(2, SPRITE_X2, SPRITE_TOP_Y);
    } else {
        FB_FillRect(0, WALL_TOP_Y, SCR_W, 64, (BYTE)((gVSwapErr * 8) & 0xFF));
    }

    if (gMapErr == 0) {
        DrawMinimapCompressed();
    } else {
        FB_FillRect(MINIMAP_X0, MINIMAP_Y0, 64, 64, (BYTE)((gMapErr * 4) & 0xFF));
    }

    /* Gutter between walls/minimap and sprites for visual separation. */
    FB_FillRect(0, 99, SCR_W, 6, 0);

    for (i = 0; i < 64000U; i++) static_bg[i] = framebuf[i];
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

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        RECT dirty;
        int  lo_x, lo_y, hi_x, hi_y;

        last_key_wparam = (WORD)wp;
        last_msg_type   = (WORD)msg;
        key_count++;
        msg_count++;
        switch (wp) {
        case VK_HC1_UP:    cursor_y -= CUR_STEP; break;
        case VK_HC1_DOWN:  cursor_y += CUR_STEP; break;
        case VK_HC1_LEFT:  cursor_x -= CUR_STEP; break;
        case VK_HC1_RIGHT: cursor_x += CUR_STEP; break;
        case VK_HC1_PRIMARY:
            /* High click tone — feedback for "press A". Music continues since
             * the click only writes ch0 regs and music writes ch1..ch8. */
            OplNoteOn(0x300, 5);
            break;
        case VK_HC1_SECONDARY:
            /* Mid click + key-off. */
            OplNoteOn(0x244, 4);
            OplNoteOff();
            break;
        case VK_HC1_F1:
            /* One-button "bring it to life": init OPL + start music. Idempotent
             * if already running (StartMusic resets the cursor). */
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
        if (cursor_x <  5)         cursor_x = 5;
        if (cursor_x >= SCR_W - 5) cursor_x = SCR_W - 6;
        if (cursor_y <  35)        cursor_y = 35;
        if (cursor_y >= SCR_H - 5) cursor_y = SCR_H - 6;

        RestoreFromBg(cursor_prev_x - CURSOR_HALF,
                      cursor_prev_y - CURSOR_HALF,
                      CURSOR_HALF * 2 + 1,
                      CURSOR_HALF * 2 + 1);
        DrawCursor(cursor_x, cursor_y);

        lo_x = cursor_prev_x < cursor_x ? cursor_prev_x : cursor_x;
        lo_y = cursor_prev_y < cursor_y ? cursor_prev_y : cursor_y;
        hi_x = cursor_prev_x > cursor_x ? cursor_prev_x : cursor_x;
        hi_y = cursor_prev_y > cursor_y ? cursor_prev_y : cursor_y;
        dirty.left   = lo_x - CURSOR_HALF;
        dirty.top    = lo_y - CURSOR_HALF;
        dirty.right  = hi_x + CURSOR_HALF + 1;
        dirty.bottom = hi_y + CURSOR_HALF + 1;
        if (dirty.left   < 0)     dirty.left   = 0;
        if (dirty.top    < 0)     dirty.top    = 0;
        if (dirty.right  > SCR_W) dirty.right  = SCR_W;
        if (dirty.bottom > SCR_H) dirty.bottom = SCR_H;
        InvalidateRect(hWnd, &dirty, FALSE);

        cursor_prev_x = cursor_x;
        cursor_prev_y = cursor_y;
        return 0;
    }

    case WM_TIMER: {
        POINT pt;
        RECT  dirty;

        tick_count++;
        has_focus = (GetFocus() == hWnd);
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
    gLoadErr = LoadMapHead();
    if (gLoadErr == 0) gMapErr = LoadMap(0);
    else               gMapErr = 100 + gLoadErr;

    gVSwapErr = LoadVSwap();

    gAudioHdrErr = LoadAudioHeader();
    if (gAudioHdrErr == 0) {
        gMusicLoadErr = LoadMusicChunk(MUSIC_SMOKE_CHUNK);
        if (gMusicLoadErr == 0) OplInit();
    }

    SetupStaticBg();
    DrawDebugBar();
    DrawCursor(cursor_x, cursor_y);
    cursor_prev_x = cursor_x;
    cursor_prev_y = cursor_y;

    if (!hPrev) {
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WolfVisWndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hInst;
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = GetStockObject(BLACK_BRUSH);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = "WolfVISa11";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa11", "WolfVISa11",
        WS_POPUP | WS_VISIBLE,
        0, 0, 640, 480,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 0;

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
        } else if (sqActive) {
            ServiceMusic();
        } else {
            WaitMessage();
        }
    }
}
