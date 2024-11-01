/*	@(#)args.c	1.4	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include "defs.h"

static struct dolnod *copyargs(char *from[], int n);
static void	freedolh(void);

static struct dolnod	*dolh;
static int		dollev = 0;
static char		**savdolv;
static int		savdolc;

char	flagchar[] =
{
	'x',
	'n',
	'v',
	't',
	STDFLG,
	'i',
	'e',
	'k',
	'u',
	'f',
	'a',
	'p',
	 0
};

char	flagadr[sizeof(flagchar)/sizeof(flagchar[0])];

long	flagval[]  =
{
	execpr,
	noexec,
	readpr,
	oneflg,
	stdflg,
	intflg,
	errflg,
	keyflg,
	setflg,
	nofngflg,
	exportflg,
	protflg,
	0
};

/* ========	option handling	======== */

int
options(int argc, char **argv)
{
	char *cp, *flagc, *flagp;
	char **argp = argv;

	if (argc > 1 && argp && *argp[1] == '-') {
		/*
		 * if first argument is "--" then options are not
		 * to be changed. Fix for problems getting
		 * $1 starting with a "-"
		 */

		cp = argp[1];
		if (cp[1] == '-') {
			argp[1] = argp[0];
			argc--;
			return argc;
		}
		if (cp[1] == '\0')
			flags &= ~(execpr|readpr);

		/*
		 * Step along 'flagchar[]' looking for matches.
		 * 'sic' are not legal with 'set' command.
		 */

		while (*++cp) {
			flagc = flagchar;
			while (*flagc && *flagc != *cp)
				flagc++;
			if (*cp == *flagc) {
				if (eq(argv[0], "set") && any(*cp, "sic"))
					failed(argv[1], badopt, 0);
				else {
					flags |= flagval[flagc-flagchar];
					if (flags & errflg)
						eflag = errflg;
					if (*cp == 'p')
						prot_env();
				}
			} else if (*cp == 'c' && argc > 2 && comdiv == 0) {
				comdiv = argp[2];
				argp[1] = argp[0];
				argp++;
				argc--;
			} else
				failed(argv[1], badopt, 0);
		}
		argp[1] = argp[0];
		argc--;
	} else if (argc > 1 && argp && *argp[1] == '+') {
		/* unset flags x, k, t, n, v, e, u, p */
		cp = argp[1];
		while (*++cp) {
			flagc = flagchar;
			while (*flagc && *flagc != *cp)
				flagc++;
			/*
			 * step through flags
			 */
			if (!any(*cp, "sic") && *cp == *flagc) {
				/*
				 * only turn off if already on
				 */
				if ((flags & flagval[flagc-flagchar])) {
					flags &= ~(flagval[flagc-flagchar]);
					if (*cp == 'e')
						eflag = 0;
				}
			}
		}
		argp[1] = argp[0];
		argc--;
	}
	/*
	 * set up $-
	 */
	flagp = flagadr;
	if (flags) {
		flagc = flagchar;
		while (flagc && *flagc) {
			if (flagval && flags & flagval[flagc-flagchar])
				*flagp++ = *flagc;
			flagc++;
		}
	}
	*flagp = 0;
	return argc;
}

/*
 * sets up positional parameters
 */
void
setargs(char *argi[])
{
	char **argp = argi;		/* count args */
	int argn = 0;
	struct dolnod *nxtblk = 0;

	while (*argp++ != ENDARGS)
		argn++;
	/*
	 * free old ones unless on 'for' loop chain
	 */
	if (dolh)
		nxtblk = dolh->dolnxt;
	freedolh();
	dolh = copyargs(argi, argn);
	dolc = argn - 1;
	dolh->dolnxt = nxtblk;
}

/*
 * pushes a set of positional parameters
 */
void
pushargs(char *argi[])
{
	char **argp = argi;		/* count args */
	int argn = 0;
	struct dolnod *nxtblk;

	while (*argp++ != ENDARGS)
		argn++;
	if (nxtblk = dolh)
		++nxtblk->doluse;
	else /* if (dollev == 0) */ {
		savdolv = dolv;
		savdolc = dolc;
	}
	dolh = copyargs(argi, argn);	/* sets dolv also */
	dolc = argn - 1;
	++dollev;
	dolh->dolnxt = nxtblk;
}

/*
 * pop a set of positional parameters
 */
void
popargs(void)
{
	char **argp;			/* count args */
	int argn;
	struct dolnod *nxtblk = 0;

	if (dolh && (nxtblk = dolh->dolnxt))
		--nxtblk->doluse;
	--dollev;
	freedolh();
	if (dolh = nxtblk) {
		dolv = dolh->dolarg;
		for (argp = dolv, argn = 0; *argp++ != ENDARGS; argn++)
			;
		dolc = argn - 1;
	} else /* if (dollev == 0) */ {
		dolv = savdolv;
		dolc = savdolc;
	}
}

static void
freedolh(void)
{
	char **argp;
	struct dolnod *argblk;

	if ((argblk = dolh) != 0 && (--argblk->doluse) == 0) {
		for (argp = argblk->dolarg; *argp != ENDARGS; argp++)
			free((free_t)*argp);
		free((free_t)argblk);
	}
}

struct dolnod *
freeargs(struct dolnod *blk)
{
	char **argp;
	struct dolnod *argblk, *argr = NIL;
	int cnt;

	if ((argblk = blk) == 0)
		return argr;

	argr = argblk->dolfor;
	cnt  = --argblk->doluse;

	if (argblk == dolh)
		if (cnt == 1)
			return argr;
		else
			return argblk;
	else if (cnt == 0) {
		for (argp = argblk->dolarg; *argp != ENDARGS; argp++)
			free(*argp);
		free(argblk);
	}
	return argr;
}

static struct dolnod *
copyargs(char *from[], int n)
{
	struct dolnod *np = (struct dolnod *)
		alloc(sizeof(struct dolnod) + sizeof(char **) * n);
	char **pp, **fp = from;

	np->doluse = 1;	/* use count */
	np->dolnxt = 0;
	np->dolfor = 0;
	pp = np->dolarg;
	dolv = pp;

	while (n--)
		*pp++ = make(*fp++);
	*pp = ENDARGS;
	return np;
}

struct dolnod *
clean_args(struct dolnod *blk)
{
	char **argp;
	struct dolnod *argblk, *argr = NIL;

	if ((argblk = blk) == 0)
		return argr;

	argr = argblk->dolfor;

	if ((--argblk->doluse) == 0) {
		for (argp = argblk->dolarg; *argp != ENDARGS; argp++)
			free(*argp);
		free(argblk);
	}
	return argr;
}

void
clearup(void)
{
	/*
	 * force `for' $* lists to go away
	 */
	while (argfor = clean_args(argfor))
		;
	/*
	 * now throw away pushed on arglists for functions
	 */
	while (dolh && dollev > 0)
		popargs();
	/*
	 * clean up io files
	 */
	while (pop())
		;

	/*
	 * clean up tmp files
	*/
	while (poptemp())
		;
}

struct dolnod *
useargs()
{
	if (dolh) {
		if (dolh->doluse++ == 1) {
			dolh->dolfor = argfor;
			argfor = dolh;
		}
	}
	return dolh;
}
