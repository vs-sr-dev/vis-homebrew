/*
 * WolfVIS A.6 — Hand controller input PoC.
 * A.5 baseline + WM_KEYDOWN cursor movement + VK-code debug bar.
 * HC driver sends WM_KEYDOWN with remapped VKs (default: VK_UP/DOWN/LEFT/RIGHT
 * for d-pad, VK_RETURN for primary). We also log the raw wParam of any key
 * in a top debug bar so we can identify HC-specific VK codes if default
 * remap differs on VIS.
 */
#include <windows.h>
#include "gamepal.h"

#define SCR_W        320
#define SCR_H        200
#define CHUNKS_MAX   700
#define NUM_WALLS    5
#define NUM_SPRITES  3
#define SPRITE_MAX   4096

#define CUR_SIZE     10
#define CUR_STEP     6

/* Modular Windows VIS hand controller VK codes — discovered empirically via
 * wParam bitgrid (S4 A.6). Range 0x70..0x79 reused from VK_F1..VK_F10
 * standard Windows slots for HC-specific events. SDK docs refer to these
 * as VK_HC1_* but never enumerate values. */
#define VK_HC1_DOWN    0x70
#define VK_HC1_F1      0x71   /* Xbox X */
#define VK_HC1_PRIMARY 0x72   /* Xbox A = primary action button */
#define VK_HC1_F3      0x73   /* Xbox Y */
#define VK_HC1_SECONDARY 0x75 /* Xbox B = secondary action button */
#define VK_HC1_LEFT    0x77
#define VK_HC1_UP      0x78
#define VK_HC1_RIGHT   0x79

static BYTE  framebuf[64000];
static BYTE  walls[NUM_WALLS][4096];
static BYTE  sprites[NUM_SPRITES][SPRITE_MAX];
static WORD  sprite_len[NUM_SPRITES];

static DWORD pageoffs[CHUNKS_MAX];
static WORD  pagelens[CHUNKS_MAX];

static WORD  chunks_in_file, sprite_start, sound_start;

static struct { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[256]; } bmi;
static struct { BITMAPINFOHEADER bmiHeader; WORD    bmiColors[256]; } bmiPal;

static HPALETTE gPal = NULL;
static BOOL     gPaletteRealized = FALSE;
static int      gLoadErr = -1;

/* A.6 input state */
static int   cursor_x       = 160;
static int   cursor_y       = 100;
static BYTE  cursor_color   = 42;       /* bright red in Wolf3D palette */
static WORD  last_key_wparam = 0;
static WORD  key_count       = 0;
static WORD  last_msg_type   = 0;       /* last non-paint msg received */
static WORD  msg_count       = 0;
static WORD  tick_count      = 0;
static int   mx_last         = 0;
static int   my_last         = 0;
static int   hc_x            = 0;
static int   hc_y            = 0;
static BOOL  has_focus       = FALSE;

/* HC.DLL statically imported via link script (IMPORT hcGetCursorPos HC.HCGETCURSORPOS). */
extern void FAR PASCAL hcGetCursorPos(LPPOINT lpp);

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

static int LoadVSwap(void)
{
    HFILE f;
    OFSTRUCT of;
    WORD hdr[3];
    UINT cbOffs, cbLens;
    UINT n;
    LONG pos;
    int i;

    f = OpenFile("A:\\VSWAP.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;

    n = _lread(f, (LPVOID)hdr, 6);
    if (n != 6) { _lclose(f); return 2; }
    chunks_in_file = hdr[0];
    sprite_start   = hdr[1];
    sound_start    = hdr[2];
    if (chunks_in_file == 0 || chunks_in_file > CHUNKS_MAX) { _lclose(f); return 3; }
    if (sprite_start >= chunks_in_file)                     { _lclose(f); return 3; }

    cbOffs = (UINT)chunks_in_file * 4U;
    cbLens = (UINT)chunks_in_file * 2U;

    n = _lread(f, (LPVOID)pageoffs, cbOffs);
    if (n != cbOffs) { _lclose(f); return 4; }
    n = _lread(f, (LPVOID)pagelens, cbLens);
    if (n != cbLens) { _lclose(f); return 5; }

    for (i = 0; i < NUM_WALLS; i++) {
        pos = _llseek(f, (LONG)pageoffs[i], 0);
        if (pos == -1L) { _lclose(f); return 6; }
        n = _lread(f, (LPVOID)walls[i], 4096);
        if (n != 4096) { _lclose(f); return 7; }
    }

    for (i = 0; i < NUM_SPRITES; i++) {
        WORD chunk = (WORD)(sprite_start + i);
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

static void ClearFrame(void)
{
    BYTE *p = framebuf;
    unsigned n;
    for (n = 0; n < 64000U; n++) *p++ = 0;
}

/* Canonical Y-flip helper: write screen coord (x,y) into bottom-up DIB. */
static void FB_Put(int sx, int sy, BYTE pix)
{
    int fb_y;
    if (sx < 0 || sx >= SCR_W || sy < 0 || sy >= SCR_H) return;
    fb_y = (SCR_H - 1) - sy;
    framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)sx] = pix;
}

static void DrawWallStrip(int top_y)
{
    int w, col, row, sx, fb_y0;
    BYTE *rowptr;
    for (w = 0; w < NUM_WALLS; w++) {
        for (col = 0; col < 64; col++) {
            sx = w * 64 + col;
            if (sx >= SCR_W) break;
            fb_y0 = (SCR_H - 1) - top_y;
            rowptr = &framebuf[(unsigned)fb_y0 * (unsigned)SCR_W + (unsigned)sx];
            for (row = 0; row < 64; row++) {
                *rowptr = walls[w][col * 64 + row];
                rowptr -= SCR_W;
            }
        }
    }
}

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
                sy = dst_y + y;
                src_idx = (WORD)(corr_top + (WORD)y);
                if (src_idx >= SPRITE_MAX) continue;
                FB_Put(sx, sy, sprite[src_idx]);
            }
            post += 3;
        }
    }
}

static void FB_FillRect(int x, int y, int w, int h, BYTE pix)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            FB_Put(x + i, y + j, pix);
}

/* 10x10 crosshair cursor: filled 2-px frame, center dot */
static void DrawCursor(int cx, int cy)
{
    int i;
    BYTE c = cursor_color;
    /* vertical bar */
    for (i = -4; i <= 4; i++) FB_Put(cx, cy + i, c);
    /* horizontal bar */
    for (i = -4; i <= 4; i++) FB_Put(cx + i, cy, c);
    /* corners for visibility */
    FB_Put(cx - 4, cy - 4, 15);
    FB_Put(cx + 4, cy - 4, 15);
    FB_Put(cx - 4, cy + 4, 15);
    FB_Put(cx + 4, cy + 4, 15);
}

/* Render a 16-bit value as a horizontal bit grid — 16 cells, bit 0 = leftmost.
 * lit_color for 1-bits, unlit for 0-bits. Cells are 18 px wide x h px tall
 * starting at screen_x. Total width = 16*18 = 288 px (fits 320). */
static void DrawBitGrid(int sx, int sy, int h, WORD val, BYTE lit, BYTE unlit)
{
    int i;
    for (i = 0; i < 16; i++) {
        BYTE c = (val & ((WORD)1 << i)) ? lit : unlit;
        FB_FillRect(sx + i * 18, sy, 16, h, c);
    }
}

/* Debug bar (top 30 px): heartbeat/focus row + msg_type bitgrid + wParam bitgrid. */
static void DrawDebugBar(void)
{
    BYTE hb = (BYTE)((tick_count & 1) ? 42 : 15);   /* flash red/white */
    /* Background gray strip (screen y=0..29) */
    FB_FillRect(0, 0, SCR_W, 30, 8);
    /* Heartbeat rect (row 0..7, far left) */
    FB_FillRect(2, 1, 28, 6, hb);
    /* Focus indicator */
    FB_FillRect(34, 1, 10, 6, has_focus ? 105 : 31);
    /* msg_count + key_count small rects on row 0..7 right side */
    FB_FillRect(220, 1, 40, 6, (BYTE)((msg_count & 0x07) ? 42 : 15));
    FB_FillRect(266, 1, 40, 6, (BYTE)((key_count & 0x07) ? 105 : 15));

    /* Row 10..17: msg_type bit grid (16 bits, bit 0 left) */
    DrawBitGrid(16, 10, 7, last_msg_type, 42, 15);   /* red=1, white=0 */

    /* Row 19..26: wParam bit grid */
    DrawBitGrid(16, 19, 7, last_key_wparam, 143, 15); /* blue=1, white=0 */
}

/* Additional HC cursor indicator: big crosshair in blue at hc_x/hc_y polled from HC.DLL */
static void DrawHcIndicator(void)
{
    int i;
    BYTE c = 57;   /* blue-ish in Wolf3D palette */
    int cx = hc_x;
    int cy = hc_y;
    if (cx < 20 || cx >= SCR_W - 20 || cy < 30 || cy >= SCR_H - 20) return;
    for (i = -10; i <= 10; i++) { FB_Put(cx + i, cy, c); FB_Put(cx, cy + i, c); }
    /* diamond corners */
    FB_Put(cx - 10, cy - 10, 15);
    FB_Put(cx + 10, cy - 10, 15);
    FB_Put(cx - 10, cy + 10, 15);
    FB_Put(cx + 10, cy + 10, 15);
}

static void DrawPaletteGrid(void)
{
    int x, y, col, row;
    BYTE *p = framebuf;
    for (y = 0; y < SCR_H; y++) {
        row = y / 12;
        if (row >= 16) row = 15;
        for (x = 0; x < SCR_W; x++) {
            col = x / 20;
            if (col >= 16) col = 15;
            if ((x % 20) == 0 || (y % 12) == 0) {
                *p++ = 0;
            } else {
                *p++ = (BYTE)(row * 16 + col);
            }
        }
    }
}

/* Compose full frame from scene state. */
static void RedrawScene(void)
{
    ClearFrame();
    if (gLoadErr == 0) {
        DrawWallStrip(10);
        DrawSprite(0,  30, 90);
        DrawSprite(1, 130, 90);
        DrawSprite(2, 230, 90);
    } else {
        DrawPaletteGrid();
    }
    DrawDebugBar();
    DrawCursor(cursor_x, cursor_y);
    DrawHcIndicator();
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
            if (!gPaletteRealized) {
                RealizePalette(hdc);
                gPaletteRealized = TRUE;
            }
        }
        StretchDIBits(
            hdc,
            0, 0, SCR_W, SCR_H,
            0, 0, SCR_W, SCR_H,
            framebuf,
            (BITMAPINFO FAR *)&bmiPal,
            DIB_PAL_COLORS,
            SRCCOPY);
        EndPaint(hWnd, &ps);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        last_key_wparam = (WORD)wp;
        last_msg_type   = (WORD)msg;
        key_count++;
        msg_count++;
        /* WM_KEYDOWN/SYSKEYDOWN drive cursor motion. Try known VKs first;
         * if none match, advance cursor right regardless so user sees SOME
         * feedback that input arrived. */
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            switch (wp) {
            case VK_HC1_UP:    cursor_y -= CUR_STEP; break;
            case VK_HC1_DOWN:  cursor_y += CUR_STEP; break;
            case VK_HC1_LEFT:  cursor_x -= CUR_STEP; break;
            case VK_HC1_RIGHT: cursor_x += CUR_STEP; break;
            case VK_HC1_PRIMARY:   cursor_color = (BYTE)((cursor_color + 4) & 0xFF); break;
            case VK_HC1_SECONDARY: cursor_color = (BYTE)((cursor_color - 4) & 0xFF); break;
            case VK_HC1_F1: cursor_color = (BYTE)((cursor_color + 32) & 0xFF); break;
            case VK_HC1_F3: cursor_color = (BYTE)((cursor_color - 32) & 0xFF); break;
            default: break;
            }
            if (cursor_x <  5)         cursor_x = 5;
            if (cursor_x >= SCR_W - 5) cursor_x = SCR_W - 6;
            if (cursor_y <  35)        cursor_y = 35;   /* below debug bar */
            if (cursor_y >= SCR_H - 5) cursor_y = SCR_H - 6;
        }
        RedrawScene();
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = (int)(short)LOWORD(lp);
        int my = (int)(short)HIWORD(lp);
        /* Detect change to avoid noise */
        if (mx != mx_last || my != my_last) {
            mx_last = mx;
            my_last = my;
            last_msg_type = (WORD)WM_MOUSEMOVE;
            last_key_wparam = (WORD)mx;   /* report mouse X in debug */
            msg_count++;
            RedrawScene();
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_TIMER: {
        POINT pt;
        tick_count++;
        has_focus = (GetFocus() == hWnd);
        /* Poll HC cursor position — captures HC input if WM msgs don't route. */
        pt.x = 0; pt.y = 0;
        hcGetCursorPos((LPPOINT)&pt);
        hc_x = (int)pt.x;
        hc_y = (int)pt.y;
        RedrawScene();
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_SETFOCUS:
        has_focus = TRUE;
        last_msg_type = (WORD)msg;
        msg_count++;
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

    case WM_KILLFOCUS:
        has_focus = FALSE;
        last_msg_type = (WORD)msg;
        msg_count++;
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

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
        if (gPal) {
            SelectPalette(dc, gPal, FALSE);
            RealizePalette(dc);
        }
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
    gLoadErr = LoadVSwap();
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
        wc.lpszClassName = "WolfVISa6";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa6", "WolfVISa6",
        WS_POPUP | WS_VISIBLE,
        0, 0, 640, 480,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 0;

    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);
    SetFocus(hWnd);
    SetActiveWindow(hWnd);
    SetTimer(hWnd, 1, 500, NULL);      /* 2 Hz heartbeat */

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
