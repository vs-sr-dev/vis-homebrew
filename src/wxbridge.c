#include <windows.h>

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    (void)hInst; (void)hPrev; (void)cmd; (void)show;
    WinExec("A:\\DHANG.EXE", SW_SHOWNORMAL);
    return 0;
}
