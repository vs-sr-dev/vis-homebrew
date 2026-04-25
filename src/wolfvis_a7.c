/*
 * WolfVIS A.7 — GAMEMAPS loader + minimap render.
 * A.6 input baseline + MAPHEAD.WL1 + GAMEMAPS.WL1 parse + Carmack + RLEW
 * decompressors + minimap 64x64 tile top-down render (2 px per tile =
 * 128x128 px minimap). HC cursor moves over minimap.
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
 */
#include <windows.h>
#include "gamepal.h"

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

static BYTE  framebuf[64000];

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

static int   cursor_x       = 160;
static int   cursor_y       = 100;
static BYTE  cursor_color   = 42;
static WORD  last_key_wparam = 0;
static WORD  key_count       = 0;
static WORD  last_msg_type   = 0;
static WORD  msg_count       = 0;
static WORD  tick_count      = 0;
static BOOL  has_focus       = FALSE;

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
    FB_FillRect(220, 1, 40, 6, (BYTE)((msg_count & 0x07) ? 42 : 15));
    FB_FillRect(266, 1, 40, 6, (BYTE)((key_count & 0x07) ? 105 : 15));
    DrawBitGrid(16, 10, 7, last_msg_type, 42, 15);
    DrawBitGrid(16, 19, 7, last_key_wparam, 143, 15);
}

static void RedrawScene(void)
{
    ClearFrame();
    if (gMapErr == 0) {
        DrawMinimap();
    } else {
        /* Error rect encoded by gMapErr value */
        FB_FillRect(60, 60, 200, 80, (BYTE)((gMapErr * 8) & 0xFF));
    }
    DrawDebugBar();
    DrawCursor(cursor_x, cursor_y);
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
    case WM_SYSKEYDOWN:
        last_key_wparam = (WORD)wp;
        last_msg_type   = (WORD)msg;
        key_count++;
        msg_count++;
        switch (wp) {
        case VK_HC1_UP:    cursor_y -= CUR_STEP; break;
        case VK_HC1_DOWN:  cursor_y += CUR_STEP; break;
        case VK_HC1_LEFT:  cursor_x -= CUR_STEP; break;
        case VK_HC1_RIGHT: cursor_x += CUR_STEP; break;
        case VK_HC1_PRIMARY:   cursor_color = (BYTE)((cursor_color + 4) & 0xFF); break;
        case VK_HC1_SECONDARY: cursor_color = (BYTE)((cursor_color - 4) & 0xFF); break;
        default: break;
        }
        if (cursor_x <  5)         cursor_x = 5;
        if (cursor_x >= SCR_W - 5) cursor_x = SCR_W - 6;
        if (cursor_y <  35)        cursor_y = 35;
        if (cursor_y >= SCR_H - 5) cursor_y = SCR_H - 6;
        RedrawScene();
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

    case WM_TIMER:
        tick_count++;
        has_focus = (GetFocus() == hWnd);
        RedrawScene();
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

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
    RedrawScene();

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
        wc.lpszClassName = "WolfVISa7";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa7", "WolfVISa7",
        WS_POPUP | WS_VISIBLE,
        0, 0, 640, 480,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 0;

    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);
    SetFocus(hWnd);
    SetActiveWindow(hWnd);
    SetTimer(hWnd, 1, 500, NULL);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
