/*
 * WolfVIS - milestone A.1
 * Win16 NE + 320x200x8 chunky framebuffer blitted via SetDIBitsToDevice.
 * Gradient test pattern, proves GDI rendering path for Wolf3D port.
 */
#include <windows.h>

#define SCR_W 320
#define SCR_H 200
/* 320*200 = 64000 overflows Watcom 16-bit signed int in const expr — use literal. */
#define SCR_PIX 64000

static BYTE framebuf[64000];

/* Two parallel BITMAPINFO: RGB mode (for reference) and PAL mode (fast path).
   PAL mode uses 256 WORD palette indices 0..255 for 1:1 mapping to realized palette. */
static struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} bmi;

static struct {
    BITMAPINFOHEADER bmiHeader;
    WORD             bmiColors[256];
} bmiPal;

static HPALETTE gPal = NULL;
static int      gScroll = 0;
static BOOL     gPaletteRealized = FALSE;

#define ANIM_TIMER_ID 1
#define ANIM_TIMER_MS 50   /* ~20 fps target */

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

    /* PAL-indexed BMI: clone header, set 1:1 index array */
    bmiPal.bmiHeader = bmi.bmiHeader;
    for (i = 0; i < 256; i++) bmiPal.bmiColors[i] = (WORD)i;
}

static void InitPalette(void)
{
    int i;
    bmi.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth         = SCR_W;
    bmi.bmiHeader.biHeight        = SCR_H;          /* positive = bottom-up */
    bmi.bmiHeader.biPlanes        = 1;
    bmi.bmiHeader.biBitCount      = 8;
    bmi.bmiHeader.biCompression   = BI_RGB;
    bmi.bmiHeader.biSizeImage     = 0;
    bmi.bmiHeader.biXPelsPerMeter = 0;
    bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmiHeader.biClrUsed       = 256;
    bmi.bmiHeader.biClrImportant  = 256;

    /* red/green/blue bands: 0..85 pure red ramp, 86..170 red->yellow, 171..255 yellow->white */
    for (i = 0; i < 256; i++) {
        BYTE r = 0, g = 0, b = 0;
        if (i < 86) {
            r = (BYTE)(i * 3);
        } else if (i < 171) {
            r = 255;
            g = (BYTE)((i - 86) * 3);
        } else {
            r = 255;
            g = 255;
            b = (BYTE)((i - 171) * 3);
        }
        bmi.bmiColors[i].rgbRed   = r;
        bmi.bmiColors[i].rgbGreen = g;
        bmi.bmiColors[i].rgbBlue  = b;
        bmi.bmiColors[i].rgbReserved = 0;
    }
}

static void FillGradient(void)
{
    int x, y;
    int s = gScroll;
    BYTE *p = framebuf;
    for (y = 0; y < SCR_H; y++) {
        int base = y + s;
        for (x = 0; x < SCR_W; x++) {
            *p++ = (BYTE)((x + base) & 0xFF);
        }
    }
}

long FAR PASCAL _export WolfVisWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    HDC         hdc;

    switch (msg) {
    case WM_PAINT: {
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
    }

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

    case WM_TIMER:
        gScroll = (gScroll + 2) & 0xFF;
        FillGradient();
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

    case WM_CREATE: {
        HDC dc = GetDC(hWnd);
        if (gPal) {
            SelectPalette(dc, gPal, FALSE);
            RealizePalette(dc);
        }
        ReleaseDC(hWnd, dc);
        SetTimer(hWnd, ANIM_TIMER_ID, ANIM_TIMER_MS, NULL);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hWnd, ANIM_TIMER_ID);
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
    FillGradient();

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
        wc.lpszClassName = "WolfVIS";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVIS", "WolfVIS",
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
