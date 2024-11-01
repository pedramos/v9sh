#include "defs.h"

#define SPeq(s, t) (strcmp(s, t) == 0)

static int SPdist(char *s, char *t);

/*
 * returns pointer to correctly spelled name,
 * or 0 if no reasonable name is found;
 * uses a static buffer to store correct name,
 * so copy it if you want to call the routine again.
 * score records how good the match was; ignore if NIL return.
 */
char *
spname(char *name, int *score)
{
	char *p, *q, *new;
	int d, nd;
	DIR *dirf;
	struct dirent *ep;
	static char newname[DIRSIZ*8], guess[DIRSIZ+1], best[DIRSIZ+1];

	new = newname;
	*score = 0;
	for (;;) {
		if (new >= &newname[sizeof newname-DIRSIZ-2])
			return 0;
		while (*name == '/')
			*new++ = *name++;
		*new = '\0';
		if (*name == '\0')
			return newname;
		p = guess;
		while (*name != '/' && *name != '\0') {
			if (p != guess + DIRSIZ)
				*p++ = *name;
			name++;
		}
		*p = '\0';
		if ((dirf = openqdir(*newname? newname: ".")) == NIL)
			return 0;
		d = 3;
		while ((ep = readdir(dirf)) != 0) {
			nd = SPdist(ep->d_name, guess);
			if (nd > 0 &&
			    (SPeq(".", ep->d_name) || SPeq("..", ep->d_name)))
				continue;
			if (nd < d) {
				p = best;
				q = ep->d_name;
				while ((*p++ = *q++) != '\0')
					continue;
				d = nd;
				if (d == 0)
					break;
			}
		}
		closedir(dirf);
		if (d == 3)
			return 0;
		p = best;
		*score += d;
		while ((*new++ = *p++) != '\0')
			continue;
		--new;
	}
}

/*
 * very rough spelling metric
 * 0 if the strings are identical
 * 1 if two chars are interchanged
 * 2 if one char wrong, added or deleted
 * 3 otherwise
 */
static int
SPdist(char *s, char *t)
{
	while (*s++ == *t)
		if (*t++ == '\0')
			return 0;
	if (*--s != 0) {
		if (*t != 0) {
			if (s[1] && t[1] && *s == t[1] && *t == s[1] &&
			    SPeq(s + 2, t + 2))
				return 1;
			if (SPeq(s + 1, t + 1))
				return 2;
		}
		if (SPeq(s + 1, t))
			return 2;
	}
	if (*t && SPeq(s, t + 1))
		return 2;
	return 3;
}
