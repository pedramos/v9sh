/*	@(#)io.c	1.5	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include	"defs.h"

short topfd;

/* ========	input output and file copying ======== */

void
initf(int fd)
{
	struct fileblk *f = standin;

	f->fdes = fd;
	f->fsiz = ((flags & oneflg) == 0 ? BUFSIZE : 1);
	f->fnxt = f->fend = f->fbuf;
	f->feval = 0;
	f->flin = 1;
	f->feof = FALSE;
}

int
estabf(char *s)
{
	struct fileblk *f;

	(f = standin)->fdes = -1;
	f->fend = length(s) + (f->fnxt = (unsigned char *)s);
	f->flin = 1;
	return f->feof = (s == 0);
}

void
push(struct fileblk *f)
{
	f->fstak = standin;
	f->feof = 0;
	f->feval = 0;
	standin = f;
}

int
pop(void)
{
	struct fileblk *f;

	if ((f = standin)->fstak) {
		if (f->fdes >= 0)
			close(f->fdes);
		standin = f->fstak;
		return TRUE;
	} else
		return FALSE;
}

static struct tempblk *tmpfptr;

void
pushtemp(int fd, struct tempblk *tb)
{
	tb->fdes = fd;
	tb->fstak = tmpfptr;
	tmpfptr = tb;
}

int
poptemp(void)
{
	if (tmpfptr) {
		close(tmpfptr->fdes);
		tmpfptr = tmpfptr->fstak;
		return TRUE;
	} else
		return FALSE;
}

void
chkpipe(int *pv)
{
	if (pipe(pv) < 0 || pv[INPIPE] < 0 || pv[OTPIPE] < 0)
		error(piperr);
}

int
chkopen(char *idf)
{
	int rc;

	if ((rc = open(idf, 0)) < 0)
		failed(idf, (char *)NIL, 1);
	return rc;
}

/* renumber, actually.  similar to Ldup(). */
void
myrename(int f1, int f2)
{
	int ce;

	if (f1 >= 0 && f2 >= 0 && f1 != f2) {
		/* are we sure that this should be f2 and not f1? */
		ce = fcntl(f2, F_GETFD, 0);  /* get close-on-exec flag */
		close(f2);
		if (dup2(f1, f2) < 0)
			error("sh: dup2 in myrename failed");
		if (ce & FD_CLOEXEC)
			clsexec(f2);
		close(f1);
		if (f2 == 0)
			ioset |= 1;
	}
}

int
create(char *s)
{
	int rc;

	if ((rc = creat(s, 0666)) < 0)
		failed(s, (char *)NIL, 1);
	return rc;
}

int
tmpfil(struct tempblk *tb)
{
	int fd;

	itos(serial++);
	movstr(numbuf, tempname);
	fd = create(tmpout);
	pushtemp(fd, tb);
	return fd;
}


/*
 * here document processing.
 *
 * this somewhat complicated output buffering reduces system and real times
 * by about an order of magnitude when processing large here documents.
 */

/*
 * set by trim
 */
extern BOOL nosubst;

typedef struct outstate outstate;
struct outstate {
	char	*clinep;		/* pointer into cline */
	char	*cline;			/* start of a line (in buf) or NIL */
	char	buf[2*CPYSIZ];		/* buffer, holds a few lines */
};

static char errdlmlng[] = "here document delimiter too long";
static char errhdwr[] = "error writing here document temporary file";

static void
flushlongln(int fd, outstate *outst)
{
	if (write(fd, outst->buf, CPYSIZ) != CPYSIZ)
		error(errhdwr);
	/* shuffle partial line back to start of buf */
	memmove(outst->buf, outst->buf + CPYSIZ, sizeof outst->buf - CPYSIZ);
	outst->clinep -= CPYSIZ;		/* adjust for new position */
	outst->cline = NIL;			/* don't match against ends */
}

/*
 * when clinep >= buf + sizeof buf, the current line must exceed CPYSIZ bytes,
 * so it can't match ends (limited to CPYSIZ bytes above), so flush the
 * fragment & remember to not match the remainder of the line against ends.
 */
static void
out(unsigned char c, int fd, outstate *outst)
{
	if (outst->clinep >= &outst->buf[sizeof outst->buf])	/* buf full? */
		flushlongln(fd, outst);
	*outst->clinep++ = c;
}

void
copy(struct ionod *iop)
{
	int fd, i;
	char c;
	char *ends;			/* "end string": here doc. delimiter */
	outstate outs;
	outstate *outst = &outs;
	struct tempblk tb;

	if (iop == NIL)
		return;
	copy(iop->iolst);
	ends = mactrim(iop->ioname);
	if (length(ends) > CPYSIZ)
		error(errdlmlng);
	if (nosubst)
		iop->iofile &= ~IODOC;
	fd = tmpfil(&tb);

	if (fndef)
		iop->ioname = make(tmpout);
	else
		iop->ioname = cpystak(tmpout);

	/* push iop onto iotemp's stack */
	iop->iolst = iotemp;
	iotemp = iop;

	/*
	 * after the current line makes the text in "start" exceed
	 * CPYSIZ bytes, CPYSIZ bytes are written from "start", so
	 * "start" must be (much) larger than CPYSIZ bytes.
	 */
	outst->clinep = outst->buf;
	for (outst->cline = outst->clinep; ; outst->cline = outst->clinep) {
		/* process one line of here-doc. input */
		chkpr();

		while (c = (nosubst? readc(): nextc(*ends)), !eolchar(c))
			out(c, fd, outst);
		out('\0', fd, outst);
		outst->clinep--;

		if (eof || outst->cline != NIL && eq(outst->cline, ends))
			break;			/* end of here doc */
		*outst->clinep++ = NL;

		/* > 1 block of output accumulated? */
		if (outst->clinep - outst->buf >= CPYSIZ)
			flushlongln(fd, outst);
	}
	/*
	 * If start of line has been flushed to disk already,
	 * pretend there is a delimiter line starting at clinep.
	 */
	if (outst->cline == NIL)
		outst->cline = outst->clinep;
	/*
	 * if any output is buffered before the delimiter
	 * line, flush the partial block.
	 */
	if ((i = outst->cline - outst->buf) > 0)
		if (write(fd, outst->buf, i) != i)
			error(errhdwr);
	/*
	 * pushed in tmpfil -- bug fix for problem deleting in-line scripts
	 */
	poptemp();
}

int
copyfile(char *old, char *new)
{
	int fold, fnew, rc, n;
	char buf[CPYSIZ];

	rc = -1;
	if (access(new, 0) == 0)
		return -1;		/* don't overwrite existing file */
	fold = open(old, O_RDONLY);
	fnew = creat(new, 0666);
	if (fold >= 0 && fnew >= 0) {
		rc = 0;
		while ((n = read(fold, buf, sizeof buf)) > 0)
			if (write(fnew, buf, n) != n) {
				rc = -1;
				break;
			}
		if (n < 0)
			rc = -1;
	}
	close(fold);
	close(fnew);
	return rc;
}

void
link_iodocs(struct ionod *i)
{
	while (i != NIL) {
		free(i->iolink);

		itos(serial++);
		movstr(numbuf, tempname);
		i->iolink = make(tmpout);
#ifdef PLAN9
		/* no links on plan 9, so just copy */
		if (copyfile(i->ioname, i->iolink) < 0)
			error("can't make here doc copy");
#else
		if (link(i->ioname, i->iolink) < 0)
			error("can't make here doc link");
#endif
		i = i->iolst;
	}
}

void
swap_iodoc_nm(struct ionod *i)
{
	while(i != NIL) {
		free(i->ioname);
		i->ioname = i->iolink;
		i->iolink = 0;

		i = i->iolst;
	}
}

int
savefd(int fd)
{
	int nfd;
	struct stat fsbuf;

	/* find an fd in the range [USERIO .. IOUFD] which is closed */
	for (nfd = USERIO; nfd <= IOUFD && fstat(nfd, &fsbuf) >= 0; nfd++)
		;
	if (nfd > IOUFD)
		error("savefd: no available fds!");
	/* else fstat failed, so nfd is closed */
	return dup2(fd, nfd);
}

void
restore(int last)
{
	int i, dupfd;

	for (i = topfd - 1; i >= last; i--)
		if ((dupfd = fdmap[i].dup_fd) > 0)
			myrename(dupfd, fdmap[i].org_fd);
		else
			close(fdmap[i].org_fd);
	topfd = last;
}
