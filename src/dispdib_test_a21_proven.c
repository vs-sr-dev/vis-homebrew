/*
 * dispdib_test.c — A.21 BEGIN + __A000H probe (S17 iter 4).
 *
 * Iter 3 hung after BEGIN before any beep_high. Likely cause: writing via
 * interp A `(BYTE __far *)&_A000H` corrupted KERNEL data segment.
 *
 * This iter:
 *   1. Read _A000H value FIRST (no write).
 *   2. Beep diagnostic encoding the value (tells us what KERNEL gave us).
 *   3. BEGIN.
 *   4. dwell 700ms.
 *   5. Single-byte write via interp B (value-is-selector). Beep "alive" pip.
 *   6. If alive, full pattern via interp B.
 *   7. END, beep_high.
 *
 * Beep diagnostic encoding (sent BEFORE BEGIN so we hear it even if hang):
 *   - 1 long beep         = _A000H == 0  (import resolved to 0, useless)
 *   - 2 short pips        = value < 0x0100  (small constant, e.g. literal 0xA0)
 *   - 3 short pips        = 0x0100 <= value < 0x1000 (mid)
 *   - 4 short pips        = value >= 0x1000  (large, plausibly a real selector)
 */
#include <windows.h>
#include <conio.h>

#define DD_MODE_320x200x8 0x0001
#define DD_NOCENTER       0x0040
#define DD_BEGIN          0x8000
#define DD_END            0x4000

#define SCR_W 320
#define SCR_H 200

WORD FAR PASCAL DisplayDib(LPBITMAPINFO lpbi, LPSTR lpBits, WORD wFlags);

extern WORD _A000H;  /* KERNEL ord 174 (0xAE) - __A000H */

static struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} bmi;

static void io_delay(void) { int i; for (i = 0; i < 96; i++) outp(0x80, 0); }
static void OPL_Reg(BYTE r, BYTE v) { outp(0x388,r); io_delay(); outp(0x389,v); io_delay(); }
static void OPL_Init(void) {
    OPL_Reg(0x01,0x20); OPL_Reg(0xBD,0);
    OPL_Reg(0x20,0x21); OPL_Reg(0x40,0x10); OPL_Reg(0x60,0xF0); OPL_Reg(0x80,0x77); OPL_Reg(0xE0,0);
    OPL_Reg(0x23,0x21); OPL_Reg(0x43,0x00); OPL_Reg(0x63,0xF0); OPL_Reg(0x83,0x77); OPL_Reg(0xE3,0);
    OPL_Reg(0xC0,0x06);
}
static void OPL_NoteOn(WORD f, BYTE b) { OPL_Reg(0xA0,(BYTE)(f&0xFF)); OPL_Reg(0xB0,(BYTE)(((f>>8)&3)|(b<<2)|0x20)); }
static void OPL_NoteOff(void) { OPL_Reg(0xB0,0); }

static void busy_ms(WORD ms) { DWORD t0 = GetTickCount(); while (GetTickCount()-t0 < (DWORD)ms) {} }
static void beep_at(WORD f, BYTE b, WORD ms) { OPL_NoteOn(f,b); busy_ms(ms); OPL_NoteOff(); busy_ms(140); }
static void beep_low(void)   { beep_at(0x2A0, 3, 250); }
static void beep_high(void)  { beep_at(0x2A0, 5, 250); }
static void beep_long(void)  { beep_at(0x2A0, 4, 600); }
static void beep_pip(void)   { beep_at(0x2A0, 4, 80); }

static void beep_diag_value(WORD val)
{
    int i, n;
    if (val == 0) { beep_long(); return; }
    if      (val < 0x0100) n = 2;
    else if (val < 0x1000) n = 3;
    else                   n = 4;
    for (i = 0; i < n; i++) beep_pip();
}

static void InitBmi(void)
{
    int i;
    bmi.bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth        = SCR_W;
    bmi.bmiHeader.biHeight       = SCR_H;
    bmi.bmiHeader.biPlanes       = 1;
    bmi.bmiHeader.biBitCount     = 8;
    bmi.bmiHeader.biCompression  = BI_RGB;
    bmi.bmiHeader.biClrUsed      = 256;
    bmi.bmiHeader.biClrImportant = 256;

    for (i = 0; i < 256; i++) {
        BYTE g = (BYTE)i;
        bmi.bmiColors[i].rgbRed = g; bmi.bmiColors[i].rgbGreen = g; bmi.bmiColors[i].rgbBlue = g;
        bmi.bmiColors[i].rgbReserved = 0;
    }
    bmi.bmiColors[0x10].rgbRed = 0xFF; bmi.bmiColors[0x10].rgbGreen = 0x00; bmi.bmiColors[0x10].rgbBlue = 0x00;
    bmi.bmiColors[0xFE].rgbRed = 0x00; bmi.bmiColors[0xFE].rgbGreen = 0xFF; bmi.bmiColors[0xFE].rgbBlue = 0x00;
}

long FAR PASCAL _export DDTestWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return DefWindowProc(hWnd, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    WNDCLASS    wc;
    HWND        hWnd;
    WORD        sel;
    BYTE __far *fb;
    int         x, y;
    (void)cmd;

    OPL_Init();
    InitBmi();

    if (!hPrev) {
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = DDTestWndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hInst;
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = GetStockObject(BLACK_BRUSH);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = "DDProbe4";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "DDProbe4", "DDProbe4",
        WS_POPUP | WS_VISIBLE,
        0, 0, 640, 480,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 0;

    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);

    busy_ms(800);

    /* Step 1+2: extract selector via interpretation C — OFFSET part of &_A000H
     * is the dynamic selector value (KERNEL exports __A000H as a CONSTANT
     * entry whose entry-table offset is patched to the runtime selector). */
    sel = (WORD)((DWORD)(LPVOID)&_A000H);   /* low WORD of far ptr = offset = selector */
    beep_diag_value(sel);

    busy_ms(600);  /* gap so user can count diagnostic beeps */
    beep_low();    /* test sequence start marker */

    /* Step 3: BEGIN */
    DisplayDib((LPBITMAPINFO)&bmi, NULL, DD_BEGIN | DD_MODE_320x200x8 | DD_NOCENTER);
    busy_ms(700);

    /* Step 4: single-byte probe via interp C selector */
    fb = (BYTE __far *)(((DWORD)sel) << 16);
    fb[0] = (BYTE)0xFE;  /* single-byte write probe */

    beep_pip();          /* alive after single-byte write */
    busy_ms(300);

    /* Step 5: full checker via interp B */
    for (y = 0; y < SCR_H; y++) {
        for (x = 0; x < SCR_W; x++) {
            fb[(long)y * SCR_W + x] = ((x ^ y) & 0x10) ? (BYTE)0xFE : (BYTE)0x10;
        }
    }

    busy_ms(2500);

    DisplayDib((LPBITMAPINFO)&bmi, NULL, DD_END | DD_MODE_320x200x8 | DD_NOCENTER);
    beep_high();
    busy_ms(1500);

    return 0;
}
