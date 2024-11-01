/*	@(#)profile.c	1.3	*/
#include "defs.h"

void
monitor(char *lowpc, char *highpc, int *buf, int bufsiz, int cntsiz)
{
	int o;
	static int *sbuf, ssiz;
	static char temp[] = "profXXXXXXXXXXXX";

	if (lowpc == 0) {
		profil(0, 0, 0, 0);
		o = creat(mktemp(temp), 0666);
		(void) write(o, sbuf, ssiz<<1);
		(void) close(o);
		return;
	}
	ssiz = bufsiz;
	buf[0] = (int)lowpc;
	buf[1] = (int)highpc;
	buf[2] = cntsiz;
	sbuf = buf;
	buf    += 3*(cntsiz+1);
	bufsiz -= 3*(cntsiz+1);
	if (bufsiz<=0)
		return;
	o = ((highpc - lowpc)>>1) & 077777;
	if(bufsiz < o)
		o = ((long)bufsiz<<15) / o;
	else
		o = 077777;
	profil(buf, bufsiz<<1, lowpc, o<<1);
}
