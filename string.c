/*	@(#)string.c	1.4	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include "defs.h"

/* ======== general purpose string handling ======== */

char *
movstr(char *src, char *dest)		/* strcpy with odd return value */
{
	while (*dest++ = *src++)
		;
	return --dest;			/* points to NUL byte in dest */
}

/* strncpy with odd return value used only to terminate dest with NUL */
char *
movstrn(char *src, char *dest, int n)
{
	while (n-- > 0 && *src)
		*dest++ = *src++;
	return dest;		/* points to 1st byte in dest after copy */
}

int
length(char *s)				/* includes NUL byte */
{
	return s == NIL? 0: strlen(s) + 1;
}

int
any(char c, char *s)		/* reversed strchr with odd return value */
{
	return strchr(s, c) != 0;
}
