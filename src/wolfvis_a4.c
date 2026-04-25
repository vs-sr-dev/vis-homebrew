/*
 * WolfVIS A.4 — BISECT build: verbatim A.3 structure, class name WolfVISa4.
 * If this fails, naming/build pipeline issue. If succeeds, we iteratively
 * add VSWAP loader features.
 */
#include <windows.h>
#include "gamepal.h"

#define SCR_W 320
#define SCR_H 200
#define SCR_PIX 64000

static BYTE framebuf[64000];
static BYTE walls[5][4096];   /* 20 KB BSS test */

static struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} bmi;

static struct {
    BITMAPINFOHEADER bmiHeader;
    WORD             bmiColors[256];
} bmiPal;

static HPALETTE gPal = NULL;
static BOOL     gPaletteRealized = FALSE;
static int      gLoadErr = -1;     /* -1 = not attempted, 0 = ok, >0 = phase */

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

/* Load first 5 walls from A:\VSWAP.WL1. Returns 0 on success, 1..N on phase. */
/* BISECT: + _llseek + read offset table (20 bytes) */
static int LoadVSwap(void)
{
    HFILE f;
    OFSTRUCT of;
    WORD hdr[3];
    DWORD offs[5];
    UINT n;
    LONG pos;
    f = OpenFile("A:\\VSWAP.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;
    n = _lread(f, (LPVOID)hdr, 6);
    if (n != 6) { _lclose(f); return 2; }
    pos = _llseek(f, 6L, 0);
    if (pos == -1L) { _lclose(f); return 3; }
    n = _lread(f, (LPVOID)offs, 20);
    if (n != 20) { _lclose(f); return 4; }
    /* Read walls 0..4 */
    {
        int i;
        for (i = 0; i < 5; i++) {
            pos = _llseek(f, (LONG)offs[i], 0);
            if (pos == -1L) { _lclose(f); return 5; }
            n = _lread(f, (LPVOID)walls[i], 4096);
            if (n != 4096) { _lclose(f); return 6; }
        }
    }
    _lclose(f);
    return 0;   /* success - call DrawWallStrip */
}

/* Use pointer increments (like DrawPaletteGrid) to avoid Watcom 16-bit int
 * overflow on sy*SCR_W when sy >= 103 (103*320 = 32960 > 32767). */
static void DrawWallStrip(void)
{
    int w, col, row, sx;
    BYTE *rowptr;
    BYTE *p = framebuf;
    unsigned n;
    for (n = 0; n < 64000U; n++) *p++ = 0;
    for (w = 0; w < 5; w++) {
        for (col = 0; col < 64; col++) {
            sx = w * 64 + col;
            if (sx >= SCR_W) break;
            /* framebuf offset for (sx, 68) via unsigned math */
            rowptr = &framebuf[(unsigned)68 * (unsigned)SCR_W + (unsigned)sx];
            for (row = 0; row < 64; row++) {
                *rowptr = walls[w][col * 64 + row];
                rowptr += SCR_W;
            }
        }
    }
}

static void DrawPaletteGrid(void)
{
    int x, y, col, row;
    BYTE idx;
    BYTE *p = framebuf;
    for (y = 0; y < SCR_H; y++) {
        row = y / 12;
        if (row >= 16) row = 15;
        for (x = 0; x < SCR_W; x++) {
            col = x / 20;
            if (col >= 16) col = 15;
            idx = (BYTE)(row * 16 + col);
            if ((x % 20) == 0 || (y % 12) == 0) {
                *p++ = 0;
            } else {
                *p++ = idx;
            }
        }
    }
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
    if (gLoadErr == 0) {
        DrawWallStrip();
    } else {
        /* Show palette grid as fallback so we know app is running even if load failed */
        DrawPaletteGrid();
        /* overlay a strip of error-phase color at top to identify phase */
        {
            int x, y;
            BYTE idx = (BYTE)(gLoadErr * 16);
            for (y = 0; y < 10; y++)
                for (x = 0; x < SCR_W; x++)
                    framebuf[y * SCR_W + x] = idx;
        }
    }

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
        wc.lpszClassName = "WolfVISa4";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa4", "WolfVISa4",
        WS_POPUP | WS_VISIBLE,
        0, 0, 640, 480,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 0;

    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
