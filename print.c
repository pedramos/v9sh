/*	@(#)print.c	1.5	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include <sys/param.h>
#include "defs.h"

#ifndef HZ
#define	HZ 60
#endif

#define BUFLEN 128

static char	buffer[BUFLEN];
static int	idx = 0;
char		numbuf[24];

/*
 * printing and io conversion
 */
void
prp(void)
{
	if ((flags & prompt) == 0 && cmdadr) {
		prs_cntl(cmdadr);
		prs(colon);
	}
}

void
prs(char *s)
{
	if (s && s[0] != '\0')
		write(output, s, length(s) - 1);
}

void
prc(char c)
{
	if (c)
		write(output, &c, 1);
}

void
prt(time_t t)		/* actually, t is time in clock times, not seconds */
{
	int hr, min, sec;

	t += HZ / 2;		/* round to nearest second */
	t /= HZ;
	sec = t % 60;
	t /= 60;
	min = t % 60;

	if (hr = t / 60) {
		prn_buff(hr);
		prc_buff('h');
	}

	prn_buff(min);
	prc_buff('m');
	prn_buff(sec);
	prc_buff('s');
}

/*
 * process id's can be very large nowadays, or we could be printing
 * 64-bit addresses for debugging.
 */
void
prn(unsigned long long n)
{
	itos(n);
	prs(numbuf);
}

void
itos(unsigned long long a)
{
	char *abuf;
	char revbuf[24];

	abuf = revbuf + sizeof revbuf - 1;
	*abuf-- = '\0';
	do {
		*abuf-- = a%10 + '0';
		a /= 10;
	} while (a > 0);
	strncpy(numbuf, abuf+1, sizeof numbuf);
}

int
stoi(char *icp)
{
	int	r = 0;
	char	c;
	char	*cp;

	for (cp = icp; (c = *cp, digit(c)) && c && r >= 0; cp++)
		r = r*10 + c - '0';
	if (r < 0 || cp == icp)
		failed(icp, badnum, 0);
	return r;
}

void
flushb(void)
{
	if (idx) {
		buffer[idx] = '\0';
		write(1, buffer, length(buffer) - 1);
		idx = 0;
	}
}

void
prc_buff(char c)
{
	if (c) {
		if (idx + 1 >= BUFLEN)
			flushb();
		buffer[idx++] = c;
	} else {
		flushb();
		write(1, &c, 1);	/* why write a NUL? */
	}
}

void
prs_2buff(char *s, char *t)
{
	prs_buff(s);
	prs_buff(t);
}

void
prs_buff(char *s)
{
	int len = length(s) - 1;

	if (idx + len >= BUFLEN)
		flushb();

	if (len >= BUFLEN)
		write(1, s, len);
	else {
		movstr(s, &buffer[idx]);
		idx += len;
	}
}

void
clear_buff(void)
{
	idx = 0;
}


void
prs_cntl(char *s)
{
	char *ptr = buffer;
	char c;

	for (; *s != '\0'; s++) {
		c = *s & STRIP;

		/* translate a control character into a printable sequence */
		if (c < '\040') {		/* assumes ASCII char */
			*ptr++ = '^';
			c += 0100;		/* assumes ASCII char */
		} else if (c == 0177) {
			*ptr++ = '^';
			c = '?';
		}
		*ptr++ = c;
	}
	*ptr = '\0';
	prs(buffer);
}

void
prn_buff(unsigned long long n)
{
	itos(n);
	prs_buff(numbuf);
}

char *
quotedstring(char *s)
{
	char *t = s;
	int quoting = 0;

	usestak();
	while (*t)
		if (any(*t++, " \t\n\\\"'`;&|$*[](){}<>")) {
			while (*s) {
				if (*s == '\'') {
					if (quoting)
						pushstak(*s);	/* end quote */
					pushstak('\\');
					quoting=0;
				} else if(!quoting) {
					pushstak('\'');
					quoting=1;
				}
				pushstak(*s++);
			}
			if(quoting)
				pushstak('\'');
			break;
		}
	do {
		pushstak(*s);
	} while (*s++);
	setstak(0);
	return stakbot;
}
