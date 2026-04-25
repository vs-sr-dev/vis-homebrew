#include <dos.h>

static char msg[] = "DOS MODE ALIVE ON VIS\r\nPress any key...\r\n$";

int main(void)
{
    union REGS r;

    r.h.ah = 0x09;
    r.x.dx = (unsigned)msg;
    int86(0x21, &r, &r);

    r.h.ah = 0x00;
    int86(0x16, &r, &r);

    return 0;
}
