/*
 * WolfVIS A.3 - Wolf3D palette integration (baseline StretchDIBits path).
 *
 * Replaces gradient palette with real Wolf3D GAMEPAL (extracted from
 * wolf3d/WOLFSRC/OBJ/GAMEPAL.OBJ). Displays 16x16 color grid showing all
 * 256 Wolf3D palette entries. Proves palette path is compatible with our
 * renderer foundation before touching VSWAP textures.
 *
 * Render path: same as A.2 (chunky framebuf -> StretchDIBits + DIB_PAL_COLORS
 * + RealizePalette). No DisplayDib (parked separately).
 */
#include <windows.h>
#include "gamepal.h"  /* gamepal6[768]: VGA 6-bit values */

#define SCR_W 320
#define SCR_H 200
#define SCR_PIX 64000

static BYTE framebuf[64000];

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

static void InitPalette(void)
{
    int i;
    bmi.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth         = SCR_W;
    bmi.bmiHeader.biHeight        = SCR_H;
    bmi.bmiHeader.biPlanes        = 1;
    bmi.bmiHeader.biBitCount      = 8;
    bmi.bmiHeader.biCompression   = BI_RGB;
    bmi.bmiHeader.biSizeImage     = 0;
    bmi.bmiHeader.biClrUsed       = 256;
    bmi.bmiHeader.biClrImportant  = 256;

    /* Load Wolf3D palette: VGA 6-bit -> 8-bit RGBQUAD (shift left 2). */
    for (i = 0; i < 256; i++) {
        BYTE r = gamepal6[i*3 + 0];
        BYTE g = gamepal6[i*3 + 1];
        BYTE b = gamepal6[i*3 + 2];
        bmi.bmiColors[i].rgbRed      = (BYTE)(r << 2);
        bmi.bmiColors[i].rgbGreen    = (BYTE)(g << 2);
        bmi.bmiColors[i].rgbBlue     = (BYTE)(b << 2);
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

/* Draw 16x16 grid: each tile is 20 wide x 12 tall (= 320x192 used).
 * Tile (col,row) shows palette index (row*16 + col) solid-filled. */
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
            /* 1-pixel black gridlines at tile boundaries for readability */
            if ((x % 20) == 0 || (y % 12) == 0) {
                *p++ = 0;   /* palette index 0 = black in Wolf3D palette */
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
    DrawPaletteGrid();

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
        wc.lpszClassName = "WolfVISa3";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa3", "WolfVISa3",
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
