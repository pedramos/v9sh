/*	@(#)expand.c	1.4	*/
/*
 *	UNIX shell
 *
 *	Bell Telephone Laboratories
 */

#include "defs.h"

/*
 * globals (file name generation)
 *
 * "*" in params matches r.e ".*"
 * "?" in params matches r.e. "."
 * "[...]" in params matches character class
 * "[...a-z...]" in params matches a through z.
 *
 */
static void addg(char *as1, char *as2, char *as3);

int
expand(char *as, int rcnt)
{
	int	count;
	char	c;
	char	*s, *cs, *rescan = 0;
	BOOL	open, slash, dir = 0;
	DIR	*dirf;
	struct argnod *schain = gchain;
	static char entry[DIRSIZ+1];

	if (trapnote & SIGSET)
		return 0;
	s = cs = as;

	/*
	 * check for meta chars
	 */
	slash = open = 0;
	for (;;) {
		switch (*cs++) {
		case 0:
			if (rcnt && slash)
				break;
			else
				return 0;

		case '/':
			slash++;
			open = 0;
			continue;

		case '[':
			open++;
			continue;

		case ']':
			if (open == 0)
				continue;
			/* fall through */

		case '?':
		case '*':
			if (rcnt > slash)
				continue;
			else
				cs--;
			break;

		default:
			continue;
		}
		/* break in above switch gets us here, and we exit the loop */
		break;
	}

	for (;;)
		if (cs == s) {
			s = nullstr;
			break;
		} else if (*--cs == '/') {
			*cs = 0;
			if (s == cs)
				s = "/";
			break;
		}

	if ((dirf = openqdir(*s ? s : ".")) != 0)
		dir = TRUE;

	count = 0;
	if (*cs == 0)
		*cs++ = 0200;

	if(dir) {
		char *rs;
		struct dirent *e;

		rs = cs;
		do {
			if (*rs == '/') {
				rescan = rs;
				*rs = 0;
				gchain = 0;
			}
		} while (*rs++);

		if (setjmp(INTbuf) == 0)
			trapjmp = INTRSYSCALL; /* slow syscall happening */
		while ((e = readdir(dirf)) && (trapnote & SIGSET) == 0) {
			*movstrn(e->d_name, entry, DIRSIZ) = '\0';
			if (entry[0] == '.' && *cs != '.')
				if (entry[1] == '\0' ||
				    entry[1] == '.' && entry[2] == '\0')
					continue;
			if (gmatch(entry, cs)) {
				addg(s, entry, rescan);
				count++;
			}
		}
		closedir(dirf);
		trapjmp = 0;			/* slow syscall is done */

		if (rescan) {
			struct argnod *rchain;

			rchain = gchain;
			gchain = schain;
			if (count) {
				count = 0;
				while (rchain) {
					count += expand(rchain->argval, slash+1);
					rchain = rchain->argnxt;
				}
			}
			*rescan = '/';
		}
	}
	s = as;
	while (c = *s)
		*s++ = (c & STRIP ? c : '/');
	return count;
}

int
gmatch(char *s, char *p)
{
	int	lc, scc, notflag = 0;
	BOOL	matched;
	char	c;

	scc = *s++;
	if (scc != '\0') {
		scc &= STRIP;
		if (scc == 0)
			scc = 0200;
	}
	switch (c = *p++) {
	case '[':
		if (scc == 0)
			return 0;
		matched = 0;
		/*
		 * seems wise to keep ! for compatibility with SysV sh users.
		 * otherwise, forgetting the change could (e.g.) remove exactly
		 * the files one wanted to keep.
		 */
		if (*p == '^' || *p == '!') {
			notflag = 1;
			p++;
		}
		while ((c = *p++) != 0 && c != ']') {
			lc = c & STRIP;

			/* look to see if it's start of a range */
			c = 0;
			if (*p == MINUS) {
				c = *(p+1);
				if (c == ']')
					c = 0;
			}

			if (c != 0) {	/* range */
				p += 2;
				c &= STRIP;
				if (lc <= scc && scc <= c)
					matched++;
			} else			/* ordinary char */
				if (scc == lc)
					matched++;
		}
		if (c == 0)		/* no closing bracket */
			return 0;
		if (notflag)
			return matched ? 0 : gmatch(s, p);
		else
			return matched ? gmatch(s, p) : 0;

	default:
		if ((c & STRIP) != scc)
			return 0;
		/* fall through */

	case '?':
		return scc ? gmatch(s, p) : 0;

	case '*':
		while (*p == '*')
			p++;
		if (*p == 0)
			return 1;
		--s;
		while (*s)
			if (gmatch(s++, p))
				return 1;
		return 0;

	case 0:
		return scc == 0;
	}
}

static void
addg(char *as1, char *as2, char *as3)
{
	char	*s1;
	int	c;

	staktop = locstak() + BYTESPERWORD;
	s1 = as1;
	while (c = *s1++) {
		if ((c &= STRIP) == 0) {
			pushstak('/');
			break;
		}
		pushstak(c);
	}
	s1 = as2;
	while (c = *s1++)
		pushstak(c);
	if (s1 = as3) {
		pushstak('/');
		do {
			pushstak(*++s1);
		} while (*s1 != '\0');
	}
	makearg((struct argnod *)fixstak());
}

void
makearg(struct argnod *args)
{
	args->argnxt = gchain;
	gchain = args;
}

DIR *
openqdir(char *name)
{
	char buf[MAXPATHLEN+1];
	char *s;

	/* strip 0200 quoting bits */
	*movstrn(name, buf, sizeof buf - 1) = '\0';
	for (s = buf; *s != '\0'; s++)
		*s &= STRIP;

	/* now do normal opendir */
	return opendir(buf);
}
