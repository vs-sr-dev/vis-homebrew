/*
 * WolfVIS A.10 — IMF music playback (Wolf3D AdLib music format).
 *
 * Adds AUDIOT.WL1 / AUDIOHED.WL1 loading + IMF event scheduler driving
 * OPL3 register writes from a fast WM_TIMER. A.9 perf foundation
 * (static_bg + cursor erase + dirty-rect) is preserved verbatim.
 *
 * AUDIOHED.WL1 = array of DWORD offsets into AUDIOT.WL1, one per chunk
 * plus a sentinel offset = file size. Chunk N data = AUDIOT[offsets[N]
 * .. offsets[N+1]]. The shareware AUDIOHED we have is 1156 bytes = 289
 * DWORDs (288 chunks), so AUDIOWL1.H's NUMSNDCHUNKS=234 is wrong for
 * our re-pack. Music chunks empirically live at indices 260..287; the
 * SDK constants STARTMUSIC=207 are off-by-53. We hardcode chunk 261
 * (7546 B) as the smoke-test track — first big music chunk in the file.
 *
 * Wolf3D MusicGroup format:
 *     WORD length          // IMF byte count, multiple of 4
 *     WORD values[length/2] // IMF events
 *     byte trailer[~88]    // MUSE metadata, ignored by player
 *
 * Each IMF event = 2 WORDs:
 *     WORD reg_val   // low byte = OPL register, high byte = value
 *     WORD delay     // ticks before next event (alTimeCount units)
 *
 * Tick rate = 700 Hz per Wolf3D's SDL_t0Service. We can't drive WM_TIMER
 * that fast on Win16 (clamp ~55 ms), so we run the timer as fast as the
 * scheduler allows (request 50 ms) and use GetTickCount() delta to
 * advance alTimeCount by the appropriate number of 1/700-s units each
 * service call. The while-loop drains all events whose sqHackTime has
 * been reached, so bursty 0-delay event groups process atomically.
 *
 * --- Inherited reference (A.9) ---
 *
 * A.9 perf foundation (preserved unchanged in A.10):
 * Splits the scene into three layers:
 *   1. Static layer (background + minimap + minimap border) — rendered ONCE
 *      after LoadMap, snapshotted into static_bg[64000].
 *   2. Debug bar (top 30 rows) — repainted on WM_TIMER tick (500 ms cadence).
 *      Cursor never sits in y < 35, so debug-bar refresh never disturbs
 *      cursor pixels.
 *   3. Cursor — drawn on top of static_bg. Erase = copy static_bg back into
 *      framebuf for the previous-cursor 11x11 bbox. Re-draw at new pos.
 *
 * WM_KEYDOWN now invalidates only bbox(prev cursor) U bbox(new cursor).
 * WM_TIMER invalidates only top 30 rows.
 * WM_PAINT honors ps.rcPaint and StretchDIBits a partial dest rect.
 *
 * Net effect: ~80 pixel writes per keypress instead of ~80000, and a few
 * hundred bytes of GDI blit instead of 64 KB. Foundation prep before
 * raycaster, animations, and IMF audio.
 *
 * --- Inherited reference notes from A.7/A.8 ---
 *
 * Map format (Wolf3D ID_CA.C):
 *   MAPHEAD.WL1: WORD RLEWtag + DWORD headeroffsets[100]
 *   GAMEMAPS.WL1 at headeroffsets[n]: struct maptype {
 *       long planestart[3]; WORD planelength[3]; WORD w, h; char name[16];
 *   } (38 bytes)
 *   Per plane at planestart[p], planelength[p] bytes:
 *     WORD carmack_expanded_size
 *     Carmack compressed data (bytes)
 *     After Carmack decompress (carmack_expanded_size bytes):
 *       WORD rlew_expanded_size (= 2*w*h = 8192)
 *       RLEW compressed WORDs (tag = RLEWtag)
 *     After RLEW decompress: w*h WORDs = tile IDs
 *
 * OPL3 register layout (VIS Yamaha YMF262):
 *   Address port 0x388 = register index (bank 0); 0x389 = data port.
 *   F-num formula: F = freq_hz * 2^(20-block) / 49716. A4 -> 0x244, block 4.
 *   reg 0x20/0x23 bit 5 = EG type. MUST be 1 for sustained note.
 */
#include <conio.h>
#include <windows.h>
#include "gamepal.h"

/* HC.DLL static import — needed even if we don't use hcGetCursorPos actively.
 * The NE module-ref registration appears to route WM_KEYDOWN from HC hardware
 * to our focus window. Without this import A.7 silently dropped all
 * hand-controller input. Paired with IMPORT directive in link script. */
extern void FAR PASCAL hcGetCursorPos(LPPOINT lpp);

#define SCR_W        320
#define SCR_H        200

#define MAP_W        64
#define MAP_H        64
#define MAP_TILES    4096       /* 64*64 */
#define NUMMAPS      100
#define CARMACK_SRC_MAX  4096   /* planelength max observed ~3000 */
#define CARMACK_DST_MAX  8192   /* bytes (= 4096 WORDs) */

#define CUR_STEP     4

/* VK_HC1_* (S4 A.6 empirical) */
#define VK_HC1_DOWN    0x70
#define VK_HC1_F1      0x71
#define VK_HC1_PRIMARY 0x72
#define VK_HC1_F3      0x73
#define VK_HC1_F4      0x74
#define VK_HC1_SECONDARY 0x75
#define VK_HC1_TOOLBAR 0x76
#define VK_HC1_LEFT    0x77
#define VK_HC1_UP      0x78
#define VK_HC1_RIGHT   0x79

#define MINIMAP_X0   96
#define MINIMAP_Y0   35
#define TILE_PX      2

/* AUDIOHED.WL1 layout: array of DWORD offsets, sentinel at end. The
 * shareware file is 1156 bytes = 289 DWORDs (288 chunks). */
#define NUMAUDIOCHUNKS  288
#define MUSIC_SMOKE_CHUNK 261     /* first big music chunk found in our re-pack */
#define MUSIC_BUF_BYTES   24000   /* > largest music chunk (~18.7 KB) + slack */
#define MUSIC_TICK_HZ     700L    /* Wolf3D SDL_t0Service rate driving alTimeCount */

static BYTE  framebuf[64000];
static BYTE  static_bg[64000];   /* Snapshot of static layer (no debug bar, no cursor) */

static BYTE  carmack_src[CARMACK_SRC_MAX];
static WORD  carmack_dst[CARMACK_DST_MAX / 2];   /* 4096 WORDs */
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
static BYTE     gAudioOn = 0;     /* 0=off, 1=note playing or music active */
static WORD     gNoteFnum = 0x244; /* A4, block 4 */
static BYTE     gNoteBlock = 4;

/* Audio header + music chunk in-memory state.
 * __far placement forces these into their own far data segments instead
 * of DGROUP. Without it, music_buf alone (~24 KB) overflows DGROUP since
 * carmack/RLEW buffers + map planes already occupy the budget. */
static DWORD __far audio_offsets[NUMAUDIOCHUNKS + 1];   /* incl. sentinel */
static int   gAudioHdrErr = -1;
static BYTE  __far music_buf[MUSIC_BUF_BYTES];
static UINT  gMusicLen   = 0;     /* bytes in music_buf actually loaded */
static int   gMusicLoadErr = -1;
static int   gMusicChunk = -1;    /* chunk index currently in music_buf */

/* IMF playback state — mirrors Wolf3D's sqHack* / alTimeCount. */
static WORD  *sqHack    = NULL;        /* base pointer = music_buf + 2 (skip WORD length) */
static WORD  *sqHackPtr = NULL;        /* current event read cursor */
static WORD   sqHackLen = 0;           /* bytes remaining in current pass */
static WORD   sqHackSeqLen = 0;        /* total IMF byte count (for loop reset) */
static DWORD  sqHackTime = 0;          /* alTimeCount value at which next event fires */
static DWORD  alTimeCount = 0;         /* virtual 700 Hz tick counter */
static BOOL   sqActive = FALSE;
static DWORD  sqLastTick = 0;          /* last GetTickCount() in ms */

static int   cursor_x       = 160;
static int   cursor_y       = 100;
static int   cursor_prev_x  = 160;   /* tracked for erase before redraw */
static int   cursor_prev_y  = 100;
static BYTE  cursor_color   = 42;
#define      CURSOR_HALF    5         /* cursor extends -4..+4 (corners), use 5 for safety */
#define      DEBUG_BAR_H    30
static WORD  last_key_wparam = 0;
static WORD  key_count       = 0;
static WORD  last_msg_type   = 0;
static WORD  msg_count       = 0;
static WORD  tick_count      = 0;
static BOOL  has_focus       = FALSE;

/* ---- OPL3 routines ---- */

static void OplDelay(int cycles)
{
    while (cycles-- > 0) inp(0x388);   /* IN wastes ~1 us on 286 */
}

static void OplOut(BYTE reg, BYTE val)
{
    outp(0x388, reg);
    OplDelay(6);                /* ~3 us after address write */
    outp(0x389, val);
    OplDelay(35);               /* ~20 us after data write */
}

static void OplReset(void)
{
    int i;
    /* Key-off all 9 channels first */
    for (i = 0xB0; i <= 0xB8; i++) OplOut((BYTE)i, 0x00);
    /* Zero all operator envelope/output regs (won't hurt) */
    for (i = 0x40; i <= 0x55; i++) OplOut((BYTE)i, 0x3F);   /* max attenuation = silent */
}

/* Program channel 0 for a simple sine-only note (op1 carrier audible). */
static void OplInit(void)
{
    OplReset();
    /* Operator 0 (modulator, offset 0x00)
     * reg 0x20 bit 5 (EG type) = 1 → sustained envelope (note holds until key-off).
     * Without this bit the envelope is "percussive" and decays after reaching
     * sustain level → note fades out as a "pluck" — what we observed in the
     * first A.8 build. */
    OplOut(0x20, 0x21);   /* AM=0, VIB=0, EG=1 (sustained), KSR=0, mult=1 */
    OplOut(0x40, 0x3F);   /* modulator silent (max attenuation) */
    OplOut(0x60, 0xF0);   /* attack=15, decay=0 */
    OplOut(0x80, 0x05);   /* sustain level=0 (loudest held), release=5 */
    OplOut(0xE0, 0x00);   /* waveform = sine */
    /* Operator 1 (carrier, offset 0x03) */
    OplOut(0x23, 0x21);   /* EG=1 sustained — crucial for carrier */
    OplOut(0x43, 0x00);   /* output level 0 = loudest */
    OplOut(0x63, 0xF0);
    OplOut(0x83, 0x05);
    OplOut(0xE3, 0x00);
    /* Channel 0: feedback=0, algorithm=0 (2-op FM: op0 modulates op1) */
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
    gAudioOn = 0;
}

/* ---- IMF music loader + scheduler ---- */

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

/* Reset OPL3 to a clean state before / after music. Mirrors what
 * SD_MusicOff does: key-off all 9 channels + clear effects. */
static void OplMusicReset(void)
{
    int i;
    for (i = 0xB0; i <= 0xB8; i++) OplOut((BYTE)i, 0x00);  /* key-off */
    OplOut(0xBD, 0x00);                                     /* AM/Vib/Rhythm reset */
}

/* Start music playback from music_buf. Assumes LoadMusicChunk succeeded. */
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
    /* music_buf[2..] holds WORDs of IMF events. Cast through unsigned to
     * avoid Watcom 16-bit signed overflow on the +2 offset. */
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

/* IMF event scheduler. Called from WM_TIMER. Advances alTimeCount by the
 * number of 1/700-s ticks elapsed since the last call (real-time delta
 * via GetTickCount), then drains all events whose sqHackTime has been
 * reached. Loops the track when sqHackLen reaches 0. */
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
    /* Convert elapsed real ms -> 700 Hz ticks. ticks = ms * 700 / 1000. */
    ticks_advance = (elapsed_ms * MUSIC_TICK_HZ) / 1000UL;
    if (ticks_advance == 0) return;
    alTimeCount += ticks_advance;

    while (sqHackLen >= 4 && sqHackTime <= alTimeCount) {
        reg_val   = *sqHackPtr++;
        delay     = *sqHackPtr++;
        reg       = (BYTE)(reg_val & 0xFF);
        val       = (BYTE)(reg_val >> 8);
        OplOut(reg, val);
        /* Accumulate virtual time from previous due-time, NOT from
         * current alTimeCount. With batched advance, alTimeCount jumps
         * by ~38 ticks per WM_TIMER call, so `alTimeCount + delay`
         * would push every queued event to the END of the current
         * batch, accumulating massive drift. `+= delay` keeps each
         * event's due time at its true virtual tick. */
        sqHackTime += delay;
        sqHackLen -= 4;
    }

    if (sqHackLen < 4) {
        /* Loop: rewind to start. */
        sqHackPtr   = sqHack;
        sqHackLen   = sqHackSeqLen;
        alTimeCount = 0;
        sqHackTime  = 0;
    }
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

/* --- Carmack decompressor. Returns # WORDs written, or negative error. */
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

/* --- RLEW decompressor. */
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
    int carmack_words_out;
    WORD rlew_expanded;
    int rlew_words_out;

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

static void DrawMinimap(void)
{
    int tx, ty;
    int sx, sy;
    WORD tile, obj;
    BYTE wc, oc;

    FB_FillRect(MINIMAP_X0 - 2, MINIMAP_Y0 - 2,
                MAP_W * TILE_PX + 4, MAP_H * TILE_PX + 4, 15);

    for (ty = 0; ty < MAP_H; ty++) {
        for (tx = 0; tx < MAP_W; tx++) {
            tile = map_plane0[ty * MAP_W + tx];
            obj  = map_plane1[ty * MAP_W + tx];
            wc = TileToColor(tile);
            oc = ObjectToColor(obj);
            sx = MINIMAP_X0 + tx * TILE_PX;
            sy = MINIMAP_Y0 + ty * TILE_PX;
            FB_FillRect(sx, sy, TILE_PX, TILE_PX, wc);
            if (oc) FB_Put(sx, sy, oc);
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
    /* Map load status: green=ok, red=error, white=not attempted */
    FB_FillRect(50, 1, 30, 6, gMapErr == 0 ? 105 : (gMapErr > 0 ? 42 : 15));
    /* Audio active indicator: bright cyan while note playing */
    FB_FillRect(84, 1, 16, 6, gAudioOn ? 14 : 31);
    FB_FillRect(220, 1, 40, 6, (BYTE)((msg_count & 0x07) ? 42 : 15));
    FB_FillRect(266, 1, 40, 6, (BYTE)((key_count & 0x07) ? 105 : 15));
    DrawBitGrid(16, 10, 7, last_msg_type, 42, 15);
    DrawBitGrid(16, 19, 7, last_key_wparam, 143, 15);
}

/* Copy a rect from static_bg back into framebuf. Used to "erase" the cursor
 * before drawing it at the new position. Coordinates are in screen space
 * (top-down); the bottom-up DIB flip is handled inline.
 *
 * Bounds: silently clamped to [0..SCR_W) x [0..SCR_H). No-op for empty rects. */
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

/* Render the static layer (background + minimap or error indicator) into
 * framebuf, then snapshot framebuf into static_bg for later cursor-erase
 * lookups. Called once at startup after LoadMap. */
static void SetupStaticBg(void)
{
    unsigned i;
    ClearFrame();
    if (gMapErr == 0) {
        DrawMinimap();
    } else {
        FB_FillRect(60, 60, 200, 80, (BYTE)((gMapErr * 8) & 0xFF));
    }
    /* Snapshot — copy framebuf -> static_bg byte by byte (both 64000). */
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
        /* Full-source blit: GDI clips physical screen writes to the invalid
         * region (set by InvalidateRect from KEYDOWN/TIMER), so only the
         * dirty pixels are actually drawn. Avoids the bottom-up-DIB partial
         * src-rect orientation ambiguity that caused cursor trails earlier. */
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
            gNoteFnum = (WORD)(gNoteFnum + 32);
            if (gNoteFnum > 0x3FF) { gNoteFnum = 0x200; gNoteBlock++; if (gNoteBlock > 7) gNoteBlock = 4; }
            OplNoteOn(gNoteFnum, gNoteBlock);
            break;
        case VK_HC1_SECONDARY:
            OplNoteOff();
            break;
        case VK_HC1_F1:
            gNoteFnum = 0x244; gNoteBlock = 4;
            OplInit();
            OplNoteOn(gNoteFnum, gNoteBlock);
            break;
        case VK_HC1_F3:
            /* Y button: start music playback (chunk loaded at startup). */
            if (gMusicLoadErr == 0 && !sqActive) StartMusic();
            break;
        case VK_HC1_F4:
            /* (no standard X mapping yet) — stop music. */
            StopMusic();
            break;
        default: break;
        }
        if (cursor_x <  5)         cursor_x = 5;
        if (cursor_x >= SCR_W - 5) cursor_x = SCR_W - 6;
        if (cursor_y <  35)        cursor_y = 35;
        if (cursor_y >= SCR_H - 5) cursor_y = SCR_H - 6;

        /* Erase old cursor by copying static_bg back into framebuf. */
        RestoreFromBg(cursor_prev_x - CURSOR_HALF,
                      cursor_prev_y - CURSOR_HALF,
                      CURSOR_HALF * 2 + 1,
                      CURSOR_HALF * 2 + 1);
        /* Draw cursor at new position. */
        DrawCursor(cursor_x, cursor_y);

        /* Dirty rect = bbox(prev) U bbox(new), clamped to screen. */
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

        /* ServiceMusic now runs from the PeekMessage idle loop in WinMain
         * — calling it from WM_TIMER would only fire at ~55 ms cadence
         * (IMF tick rate is 1.43 ms), causing the audible per-beat drag
         * we hit on the first A.10 build. WM_TIMER stays for heartbeat
         * + debug-bar refresh only. */
        tick_count++;
        has_focus = (GetFocus() == hWnd);
        /* Call hcGetCursorPos so linker retains HC.DLL module-ref even
         * if optimizer would otherwise drop the import (S4 A.8 gotcha). */
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
    /* Audio NOT initialized at startup — press X (VK_HC1_F1) to init + play A4. */

    /* Audio: load AUDIOHED.WL1 chunk table + the smoke-test music chunk
     * into music_buf. Press Y (VK_HC1_F3) to start playback. */
    gAudioHdrErr = LoadAudioHeader();
    if (gAudioHdrErr == 0) {
        gMusicLoadErr = LoadMusicChunk(MUSIC_SMOKE_CHUNK);
        if (gMusicLoadErr == 0) OplInit();   /* prep OPL3 so StartMusic doesn't have to */
    }

    /* Build the static layer once and snapshot it. Dynamic layers (debug bar,
     * cursor) are then composited on top of the snapshot in framebuf. */
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
        wc.lpszClassName = "WolfVISa10";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa10", "WolfVISa10",
        WS_POPUP | WS_VISIBLE,
        0, 0, 640, 480,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 0;

    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);
    SetFocus(hWnd);
    SetActiveWindow(hWnd);
    /* 500 ms heartbeat / debug-bar refresh. Music scheduler runs from
     * the PeekMessage idle loop, so this timer no longer needs to be
     * fast. */
    SetTimer(hWnd, 1, 500, NULL);

    /* PeekMessage idle loop — drains the message queue, then polls
     * ServiceMusic between messages. Win16 WM_TIMER granularity is
     * ~55 ms, way coarser than the IMF tick rate (1.43 ms). Driving
     * the scheduler from idle pumps it as fast as the message loop
     * cycles, giving near-tick-accurate event dispatch. When music
     * is off, WaitMessage suspends the thread until input arrives. */
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
