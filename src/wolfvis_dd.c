/*
 * WolfVIS A.3 DISPDIB experiment, STATIC binding.
 * Imports DisplayDib/DisplayDibEx directly via NE module ref (DISPDIB.DLL is in VIS ROM).
 * No LoadLibrary -> no CD scan for DLL -> no "insert main disc".
 */
#include <windows.h>

#define SCR_W 320
#define SCR_H 200
#define SCR_PIX 64000

/* Microsoft VfW 1.1 standard flag values. */
#define DISPLAYDIB_MODE_DEFAULT      0x0000
#define DISPLAYDIB_MODE_320x200x8    0x0001
#define DISPLAYDIB_NOPALETTE         0x0004
#define DISPLAYDIB_NOTEARING         0x0010
#define DISPLAYDIB_ZOOM2             0x0020
#define DISPLAYDIB_NOCENTER          0x0040
#define DISPLAYDIB_NOWAIT            0x0080
#define DISPLAYDIB_BEGIN             0x0100
#define DISPLAYDIB_END               0x0200
#define DISPLAYDIB_TEST              0x0400
#define DISPLAYDIB_NOFLIP            0x0800

/* Imported via link script from DISPDIB.DLL (resident in VIS ROM). */
WORD FAR PASCAL DisplayDib(LPBITMAPINFO lpbi, LPSTR lpBits, WORD wFlags);

static BYTE framebuf[64000];

static struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} bmi;

static int  gScroll = 0;
static BOOL gDispActive = FALSE;
static WORD gLastErr = 0xFFFF;
static DWORD gFrame = 0;
static DWORD gStartTime = 0;

#define ANIM_TIMER_ID 1
#define ANIM_TIMER_MS 16

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
    bmi.bmiHeader.biXPelsPerMeter = 0;
    bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmiHeader.biClrUsed       = 256;
    bmi.bmiHeader.biClrImportant  = 256;

    for (i = 0; i < 256; i++) {
        BYTE r = 0, g = 0, b = 0;
        if (i < 86) { r = (BYTE)(i * 3); }
        else if (i < 171) { r = 255; g = (BYTE)((i - 86) * 3); }
        else { r = 255; g = 255; b = (BYTE)((i - 171) * 3); }
        bmi.bmiColors[i].rgbRed   = r;
        bmi.bmiColors[i].rgbGreen = g;
        bmi.bmiColors[i].rgbBlue  = b;
        bmi.bmiColors[i].rgbReserved = 0;
    }
}

static void FillGradient(void)
{
    int x, y, s = gScroll;
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
    switch (msg) {
    case WM_CREATE:
        gLastErr = DisplayDib(
            (LPBITMAPINFO)&bmi, NULL,
            DISPLAYDIB_BEGIN | DISPLAYDIB_MODE_320x200x8
            | DISPLAYDIB_NOCENTER | DISPLAYDIB_NOWAIT);
        gDispActive = (gLastErr == 0);
        gStartTime = GetTickCount();
        SetTimer(hWnd, ANIM_TIMER_ID, ANIM_TIMER_MS, NULL);
        return 0;

    case WM_TIMER:
        if (gDispActive) {
            WORD flags = DISPLAYDIB_MODE_320x200x8
                       | DISPLAYDIB_NOCENTER | DISPLAYDIB_NOWAIT;
            if (gFrame & 0x3F) flags |= DISPLAYDIB_NOPALETTE;
            gScroll = (gScroll + 2) & 0xFF;
            FillGradient();
            gLastErr = DisplayDib(
                (LPBITMAPINFO)&bmi, (LPSTR)framebuf,
                flags);
            gFrame++;
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hWnd, ANIM_TIMER_ID);
        if (gDispActive) {
            DisplayDib(NULL, NULL, DISPLAYDIB_END);
            gDispActive = FALSE;
        }
        PostQuitMessage(0);
        return 0;
    }
    (void)gStartTime;
    return DefWindowProc(hWnd, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    HWND     hWnd;
    MSG      msg;
    (void)cmd;

    InitPalette();
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
        wc.lpszClassName = "WolfVISdd";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISdd", "WolfVISdd",
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
