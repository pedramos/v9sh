/*	@(#)service.c	1.11	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include	<sys/param.h>
#include	"defs.h"	/* includes sys/types.h if not yet included */

/* N.B.: ARGMK must be an unused bit in a pointer to word */
#ifdef	CRAY				/* weird pointers */
#define	ARGMK	040000000
#else
#define ARGMK	01
#endif

static char *	execs(char *ap, char *t[]);
static void	gsort(char *from[], char *to[]);
static int	split(char *s);

extern char	*sysmsg[];
extern short topfd;

/*
 * service routines for `execute'
 */
int
initio(struct ionod *iop, int save)
{
	char	*ion;
	int	iof, fd, ioufd;
	short	lastfd;

	lastfd = topfd;
	while (iop) {
		iof = iop->iofile;
		ion = mactrim(iop->ioname);
		ioufd = iof & IOUFD;

		/* don't allow null redirection ("cmd <") */
		if (*ion && (flags&noexec) == 0) {
			if (save) {
				fdmap[topfd].org_fd = ioufd;
				fdmap[topfd++].dup_fd = savefd(ioufd);
			}

			if (iof & IODOC) {
				struct tempblk tb;

				/*
				 * ion is the name of a temp. file containing
				 * the original here doc.  Expand macros from
				 * ion into a new temp. file, whose name will
				 * be left in tmpout.
				 */
				subst(chkopen(ion), tmpfil(&tb));
				/*
				 * pushed in tmpfil() --
				 * bug fix for problem with in-line scripts.
				 * poptemp() will close the new temp. file
				 * containing the expanded here doc.
				 */
				poptemp();
				/*
				 * Re-open the expanded here document and
				 * unlink it.  The original here document
				 * is still in ion and will be removed later,
				 * unless the shell is interrupted first.
				 */
				fd = chkopen(tmpout);
				unlink(tmpout);
			} else if (iof & IOMOV) {
				if (eq(minus, ion)) {
					fd = -1;
					close(ioufd);
				} else if ((fd = stoi(ion)) >= USERIO)
					failed(ion, badfile, 0);
				else
					fd = dup(fd);
			} else if ((iof & IOPUT) == 0)
				fd = chkopen(ion);
			else if (iof & IOAPP && (fd = open(ion, 1)) >= 0)
				lseek(fd, 0L, 2);
			else
				fd = create(ion);
			if (fd >= 0)
				myrename(fd, ioufd);
		}

		iop = iop->ionxt;
	}
	if (histfd > 0) {
		close(histfd);
		histfd = 0;
	}
	return lastfd;
}

char *
simple(char *s)
{
	for(;;)
		if (any('/', s))
			while (*s++ != '/')
				;
		else
			return s;
}

char *
getpath(char *s)
{
	char	*path;

	if (any('/', s) || any(('/' | QUOTE), s))
		return nullstr;
	else if ((path = pathnod.namval.val) == 0)
		return defpath;
	else
		return(cpystak(path));
}

pathopen(path, name)
char *path, *name;
{
	int	f;

	do {
		path = catpath(path, name);
	} while ((f = open(curstak(), 0)) < 0 && path);
	return f;
}

/*
 * leaves result on top of stack
 */
char *
catpath(char *path, char *name)
{
	char *scanp = path;

	usestak();
	/* push first component of path */
	while (*scanp && *scanp != COLON)
		pushstak(*scanp++);
	if (scanp != path)		/* first char wasn't ':', */
		pushstak('/');
	path = (*scanp? scanp+1: 0);	/* reset path to just after ':' */

	/* append name by pushing it */
	scanp = name;
	do {
		pushstak(*scanp);
	} while (*scanp++);

	setstak(0);
	return path;
}

char *
nextpath(char *path)
{
	while (*path && *path != COLON)
		path++;
	return *path? path+1: 0;	/* return next component */
}

static char	*xecmsg;
static char	**xecenv;

void
execa(char *t[])
{
	char *path;

	if ((flags & noexec) == 0) {
		xecmsg = notfound;
		path = getpath(*t);
		xecenv = shsetenv();

		while (path = execs(path,t))
			;
		failed(*t, xecmsg, 0);
	}
}

static char *
execs(char *ap, char *t[])
{
	char *p, *prefix;

	prefix = catpath(ap, t[0]);
	trim(p = curstak());
	sigchk();

	execve(p, &t[0], xecenv);

	switch (errno) {
	case ENOEXEC:		/* could be a shell script */
		funcnt = 0;
		flags = 0;
		*flagadr = 0;
		comdiv = 0;
		ioset = 0;
		clearup();	/* remove open files and for loop junk */
		if (input)
			close(input);
		input = chkopen(p);

		/*
		 * set up new args
		 */

		setargs(t);
		longjmp(subshell, 1);

	case ENOMEM:
	case E2BIG:
	case ETXTBSY:
		failed(p, (char *)NIL, 1);

	default:
		xecmsg = badexec;
		/* fall through */
	case ENOENT:
		return prefix;
	}
}


/*
 * for processes to be waited for
 */
#define MAXP 20
static int	pwlist[MAXP];
static int	pwc;

void
postclr(void)
{
	int	*pw = pwlist;

	while (pw <= &pwlist[pwc])
		*pw++ = 0;
	pwc = 0;
}

void
post(int pcsid)
{
	if (pcsid) {
		int *pw = pwlist;

		while (*pw)
			pw++;
		if (pwc >= MAXP - 1)
			pw--;
		else
			pwc++;
		*pw = pcsid;
	}
}

void
await(int i, int bckg)
{
	int found, ipwc = pwc, p, rc = 0, sig, w, w_hi, wx = 0;
	int *pw;

	post(i);
	while (pwc) {
		found = 0;
		pw = pwlist;
		if (setjmp(INTbuf) == 0) {
			trapjmp = INTRSYSCALL;	/* slow syscall coming */
			p = wait(&w);
		} else
			p = -1;			/* wait interrupted */
		trapjmp = 0;			/* slow syscall is over */
		if (wasintr) {
			wasintr = 0;
			if (bckg)
				break;
		}
		while (pw <= &pwlist[ipwc])
			if (*pw == p) {
				*pw = 0;
				pwc--;
				found++;
			} else
				pw++;
		if (p == -1) {
			if (bckg) {
				int *pw = pwlist;

				while (pw <= &pwlist[ipwc] && i != *pw)
					pw++;
				if (i == *pw) {
					*pw = 0;
					pwc--;
				}
			}
			continue;
		}
		w_hi = (w >> 8) & LOBYTE;
		if ((sig = w & 0177) != 0) {
			if (sig == 0177) {	/* ptrace! return */
				prs("ptrace: ");
				sig = w_hi;
			}
			if (sysmsg[sig]) {
				if (i != p || (flags & prompt) == 0) {
					prp();
					prn(p);
					blank();
				}
				prs(sysmsg[sig]);
				if (w & 0200)
					prs(coredump);
			}
			newline();
		}
		if (rc == 0 && found != 0)
			rc = (sig ? sig | SIGFLG : w_hi);
		wx |= w;
		if (p == i)
			break;
	}
	if (wx && flags & errflg)
		exitsh(rc);
	flags |= eflag;
	exitval = rc;
	exitset();
}

BOOL nosubst;

void
trim(char *p)
{
	char	*ptr;
	char	c, q = 0;

	if (p) {
		ptr = p;
		while (c = *p++) {
			if (*ptr = c & STRIP)
				++ptr;
			q |= c;
		}
		*ptr = 0;
	}
	nosubst = q & QUOTE;
}

char *
mactrim(char *s)
{
	char *t = macro(s);

	trim(t);
	return t;
}

char **
scan(int argn)
{
	struct argnod *argp = (struct argnod *)((uintptr_t)gchain & ~ARGMK);
	char **comargn, **comargm;

	comargn = (char **)getstak(BYTESPERWORD * (argn + 1));
	comargm = comargn += argn;
	*comargn = ENDARGS;
	while (argp) {
		*--comargn = argp->argval;

		trim(*comargn);
		argp = argp->argnxt;
		if (argp == 0 || (uintptr_t)argp & ARGMK) {
			gsort(comargn, comargm);
			comargm = comargn;
		}
		argp = (struct argnod *)((uintptr_t)argp & ~ARGMK);
	}
	return comargn;
}

static void
gsort(char *from[], char *to[])			/* a Shell sort, amusingly */
{
	int k, m, n, i, j;
	char **fromi;
	char *s;

	if ((n = to - from) <= 1)
		return;
	for (j = 1; j <= n; j *= 2)
		;
	for (m = 2 * j - 1; m /= 2; ) {
		k = n - m;
		for (j = 0; j < k; j++)
			for (i = j; i >= 0; i -= m) {
				fromi = &from[i];
				if (strcmp(fromi[m], fromi[0]) > 0)
					break;
				s = fromi[m];
				fromi[m] = fromi[0];
				fromi[0] = s;
			}
	}
}

/*
 * Argument list generation
 */
getarg(struct comnod *c)
{
	struct argnod	*argp;
	int count = 0;

	if (c)
		for (argp = c->comarg; argp; argp = argp->argnxt)
			count += split(macro(argp->argval));
	return count;
}

static int
split(char *s)		/* blank interpretation routine */
{
	char	*argp;
	int	c, count = 0;

	for (;;) {
		sigchk();
		staktop = locstak() + BYTESPERWORD;
		while (c = *s++, !any(c, ifsnod.namval.val) && c)
			pushstak(c);
		if (BYTESPERWORD == relstak()) {
			if (c)
				continue;
			setstak(0);
			return count;
		} else if (c == 0)
			s--;
		/*
		 * file name generation
		 */

		argp = fixstak();

		if ((flags & nofngflg) == 0 &&
		    (c = expand(((struct argnod *)argp)->argval, 0)))
			count += c;
		else {
			makearg((struct argnod *)argp);
			count++;
		}
		gchain = (struct argnod *)((uintptr_t)gchain | ARGMK);
	}
}
