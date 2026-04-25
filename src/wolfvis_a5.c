/*
 * WolfVIS A.5 — VSWAP sprite loader PoC.
 * A.4 baseline + full offset/length tables + 3 sprite chunks loaded + DrawSprite 1:1.
 * Sprite format (Wolf3D t_compshape):
 *   WORD leftpix, rightpix
 *   WORD dataofs[rightpix-leftpix+1]  (byte offset within sprite to each column's post list)
 * Post list (per column): 3 WORDs per post, 0-word terminator:
 *   WORD end_y  (pixel_row << 1; codeofs byte-offset in scaler)
 *   WORD corr_top (pix_base - starty; "corrected top" so sprite[corr_top + y] = pixel for row y)
 *   WORD start_y (pixel_row << 1)
 */
#include <windows.h>
#include "gamepal.h"

#define SCR_W        320
#define SCR_H        200
#define CHUNKS_MAX   700      /* VSWAP.WL1 shareware actual = 663 */
#define NUM_WALLS    5
#define NUM_SPRITES  3
#define SPRITE_MAX   4096

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
static int      gLoadErr = -1;     /* -1=not attempted, 0=ok, >0=phase */

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
    if (sprite_start  >= chunks_in_file)                     { _lclose(f); return 3; }

    cbOffs = (UINT)chunks_in_file * 4U;     /* max 700*4 = 2800 */
    cbLens = (UINT)chunks_in_file * 2U;     /* max 700*2 = 1400 */

    n = _lread(f, (LPVOID)pageoffs, cbOffs);
    if (n != cbOffs) { _lclose(f); return 4; }
    n = _lread(f, (LPVOID)pagelens, cbLens);
    if (n != cbLens) { _lclose(f); return 5; }

    /* Load walls 0..NUM_WALLS-1 (fixed 4096 B each, col-major 64x64). */
    for (i = 0; i < NUM_WALLS; i++) {
        pos = _llseek(f, (LONG)pageoffs[i], 0);
        if (pos == -1L) { _lclose(f); return 6; }
        n = _lread(f, (LPVOID)walls[i], 4096);
        if (n != 4096) { _lclose(f); return 7; }
    }

    /* Load sprites: chunks [sprite_start .. sprite_start+NUM_SPRITES-1]. */
    for (i = 0; i < NUM_SPRITES; i++) {
        WORD chunk = (WORD)(sprite_start + i);
        WORD len   = pagelens[chunk];
        sprite_len[i] = 0;
        if (len == 0) continue;                 /* blank sprite slot */
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

/* Bottom-up DIB (biHeight > 0) is required on MW VIS driver, so framebuf
 * row 0 = bottom of screen. To get natural top-down rendering we write at
 * fb_y = (SCR_H-1) - screen_y. DrawWallStrip starts at the LAST row in fb
 * space and decrements the row pointer. Use pointer-walk to dodge Watcom
 * 16-bit int overflow on sy*SCR_W for sy>=103. */
static void DrawWallStrip(int top_y)
{
    int w, col, row, sx, fb_y0;
    BYTE *rowptr;
    for (w = 0; w < NUM_WALLS; w++) {
        for (col = 0; col < 64; col++) {
            sx = w * 64 + col;
            if (sx >= SCR_W) break;
            /* wall row 0 -> screen y=top_y -> fb row (SCR_H-1)-top_y */
            fb_y0 = (SCR_H - 1) - top_y;
            rowptr = &framebuf[(unsigned)fb_y0 * (unsigned)SCR_W + (unsigned)sx];
            for (row = 0; row < 64; row++) {
                *rowptr = walls[w][col * 64 + row];
                rowptr -= SCR_W;
            }
        }
    }
}

/* Draw sprite idx at (dst_x, dst_y) 1:1 top-left. Posts define opaque regions
 * only; transparent columns/rows are simply absent in the post stream. */
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
                src_idx = (WORD)(corr_top + (WORD)y);    /* WORD add wraps intentionally */
                if (src_idx >= SPRITE_MAX) continue;
                fb_y = (SCR_H - 1) - sy;
                framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)sx] = sprite[src_idx];
            }
            post += 3;
        }
    }
}

/* Fallback: visualize load phase via palette grid + colored top strip */
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

    ClearFrame();
    if (gLoadErr == 0) {
        DrawWallStrip(10);
        /* 3 sprites below wall strip at y=90, x-spaced 100 px apart */
        DrawSprite(0,  30, 90);
        DrawSprite(1, 130, 90);
        DrawSprite(2, 230, 90);
    } else {
        DrawPaletteGrid();
        /* top strip color encodes error phase (fb bottom rows = screen top) */
        {
            int x, y, fb_y;
            BYTE idx = (BYTE)(gLoadErr * 16);
            for (y = 0; y < 10; y++) {
                fb_y = (SCR_H - 1) - y;
                for (x = 0; x < SCR_W; x++)
                    framebuf[fb_y * SCR_W + x] = idx;
            }
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
        wc.lpszClassName = "WolfVISa5";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa5", "WolfVISa5",
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
