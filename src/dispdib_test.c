/*
 * dispdib_test.c — empirical NOWAIT flag bit identification (S16 A.21 brute force).
 *
 * Builds a tiny Win16 NE that imports DISPDIB.DISPLAYDIB statically (no
 * LoadLibrary -> no insert-disc dialog), beeps OPL3 channel 0 BEFORE the
 * DisplayDib call, then beeps a HIGHER pitch AFTER the call returns. Time
 * delta between the two beeps reveals whether NOWAIT is in the candidate
 * flag bit:
 *
 *   beep1 -> screen flips to 320x200x8 gradient -> beep2 IMMEDIATELY
 *     => NOWAIT is in CANDIDATE_BIT
 *
 *   beep1 -> screen flips, stays in gradient until user presses HC button
 *   -> mode reverts -> beep2
 *     => NOWAIT NOT in CANDIDATE_BIT (DisplayDib blocked)
 *
 *   beep1 -> screen does NOT change -> beep2 immediately
 *     => candidate rejected (NOTSUPPORTED return), bit is NOT a flag bit
 *
 * S3 already empirically falsified 0x0080, 0x0400, 0x1000 for NOWAIT.
 * Remaining candidates (in plausibility order from VfW heritage):
 *   0x0008 0x0010 0x0020 0x0100 0x0200 0x0800 0x2000 0x4000 0x8000
 *
 * Set CANDIDATE_BIT below before each rebuild. Window class name embeds
 * the value so we can distinguish builds at a glance.
 */
#include <windows.h>
#include <conio.h>   /* outp */

#define DD_MODE_320x200x8 0x0001
#define DD_NOCENTER       0x0040
#define DD_NOWAIT         0x0100   /* S16 confirmed empirically (gradient + immediate return) */

/* === EDIT THIS LINE BETWEEN BUILDS =====================================
 * Now testing for DISPLAYDIB_BEGIN. Pattern: with NOWAIT alone, the
 * mode reverts ~immediately after return. With BEGIN bit ALSO set, the
 * mode should persist until DisplayDib is called again with END or
 * until app exits.
 */
#define CANDIDATE_BIT     0x0000   /* test: lpBits=NULL hypothesis (BEGIN implicit?) */
/* ======================================================================= */

#define SCR_W 320
#define SCR_H 200

WORD FAR PASCAL DisplayDib(LPBITMAPINFO lpbi, LPSTR lpBits, WORD wFlags);

static BYTE framebuf[64000];
static struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} bmi;

static WORD g_last_err = 0xFFFF;

/* OPL3 helpers — port I/O 0x388 (register), 0x389 (data). VIS routes the
 * Adlib-Gold-compatible YMF262 here. From A.8 we know this works
 * regardless of GDI mode, so beeps fire even after DisplayDib has taken
 * over the display. */
static void io_delay(void)
{
    int i;
    for (i = 0; i < 96; i++) outp(0x80, 0);
}

static void OPL_Reg(BYTE reg, BYTE val)
{
    outp(0x388, reg);
    io_delay();
    outp(0x389, val);
    io_delay();
}

static void OPL_Init(void)
{
    /* Clear all OPL3 registers we touch. */
    OPL_Reg(0x01, 0x20);   /* WAVE_SELECT enable, otherwise OPL2-compatible */
    OPL_Reg(0xBD, 0x00);   /* no rhythm/percussion */

    /* Channel 0 (operators 0 + 3 = slots 0,3) — simple AM-additive sine. */
    OPL_Reg(0x20, 0x21);   /* op0: mult=1, no AM/VIB/EG/KSR */
    OPL_Reg(0x40, 0x10);   /* op0: KSL=0, level=0x10 */
    OPL_Reg(0x60, 0xF0);   /* op0: attack=15, decay=0 */
    OPL_Reg(0x80, 0x77);   /* op0: sustain=7, release=7 */
    OPL_Reg(0xE0, 0x00);   /* op0: waveform=sine */

    OPL_Reg(0x23, 0x21);   /* op1: mult=1 */
    OPL_Reg(0x43, 0x00);   /* op1: KSL=0, level=0 (loud carrier) */
    OPL_Reg(0x63, 0xF0);   /* op1: AD */
    OPL_Reg(0x83, 0x77);   /* op1: SR */
    OPL_Reg(0xE3, 0x00);   /* op1: waveform=sine */

    OPL_Reg(0xC0, 0x06);   /* feedback=3, FM connection */
}

static void OPL_NoteOn(WORD freq, BYTE block)
{
    OPL_Reg(0xA0, (BYTE)(freq & 0xFF));
    OPL_Reg(0xB0, (BYTE)(((freq >> 8) & 0x03) | (block << 2) | 0x20));
}

static void OPL_NoteOff(void)
{
    OPL_Reg(0xB0, 0x00);
}

/* Busy-delay using PIT polling. ~SLEEP_TICKS_PER_MS approx for VIS @
 * 12 MHz. Pure busy loop is fine here; we don't care about message
 * pumping during the test. */
static void busy_ms(WORD ms)
{
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < (DWORD)ms) { /* spin */ }
}

/* Pre-call beep: low pitch, ~A3 (220 Hz region in OPL block 3). */
static void beep_pre(void)
{
    OPL_NoteOn(0x2A0, 3);
    busy_ms(250);
    OPL_NoteOff();
    busy_ms(150);
}

/* Post-call beep: high pitch, ~A5 region in OPL block 5. */
static void beep_post(void)
{
    OPL_NoteOn(0x2A0, 5);
    busy_ms(250);
    OPL_NoteOff();
}

static void InitBmi(void)
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

    /* Distinctive RGB ramp so user can recognize when 320x200x8 mode is
     * active. Red-green-blue gradient by index. */
    for (i = 0; i < 256; i++) {
        BYTE r = 0, g = 0, b = 0;
        if (i < 86)        { r = (BYTE)(i * 3); }
        else if (i < 171)  { r = 255; g = (BYTE)((i - 86) * 3); }
        else               { r = 255; g = 255; b = (BYTE)((i - 171) * 3); }
        bmi.bmiColors[i].rgbRed      = r;
        bmi.bmiColors[i].rgbGreen    = g;
        bmi.bmiColors[i].rgbBlue     = b;
        bmi.bmiColors[i].rgbReserved = 0;
    }

    /* Diagonal gradient pattern. */
    {
        int x, y;
        BYTE *p = framebuf;
        for (y = 0; y < SCR_H; y++) {
            for (x = 0; x < SCR_W; x++) {
                *p++ = (BYTE)((x + y) & 0xFF);
            }
        }
    }
}

long FAR PASCAL _export DDTestWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return DefWindowProc(hWnd, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    HWND     hWnd;
    char     classname[32];
    (void)cmd;

    /* Window class name embeds the candidate so we can verify which
     * build is actually running. */
    wsprintf(classname, "DDTest_%04X", CANDIDATE_BIT);

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
        wc.lpszClassName = classname;
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        classname, classname,
        WS_POPUP | WS_VISIBLE,
        0, 0, 640, 480,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 0;

    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);

    /* Settle ~1s so user has time to register the test starting. */
    busy_ms(1000);

    /* === THE TEST ====================================================
     *
     * Beep1 (low) -> DisplayDib(...) -> Beep2 (high). Time delta
     * between beeps tells us about NOWAIT bit.
     *
     * We pass the framebuf as bits and a full BMI. We do NOT pass
     * BEGIN/END separately — the goal here is solely to identify which
     * single bit (if any) gates the WAIT/NOWAIT decision.
     */
    beep_pre();

    /* HYPOTHESIS: lpBits=NULL => BEGIN-style behavior (mode held). */
    g_last_err = DisplayDib(
        (LPBITMAPINFO)&bmi,
        NULL,
        DD_MODE_320x200x8 | DD_NOCENTER | DD_NOWAIT | CANDIDATE_BIT);

    /* 2.5 sec dwell so user can observe whether the mode PERSISTS in
     * 320x200x8 (gradient stays = BEGIN in CANDIDATE_BIT) or reverts
     * almost immediately (BEGIN not in CANDIDATE_BIT). */
    busy_ms(2500);

    beep_post();

    busy_ms(2000);

    return 0;
}
