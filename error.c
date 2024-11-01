/*	@(#)error.c	1.4	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include "defs.h"

void rmfunctmp(void);

/* ======== error handling ======== */
void
failed(char *s1, char *s2, int per)
{
	prp();
	prs_cntl(s1);
	if (s2) {
		prs(colon);
		prs(s2);
	}
	if (per) {
		prs(colon);
		prs(strerror(errno));
	}
	newline();
	exitsh(ERROR);
}

void
error(char *s)
{
	failed(s, (char *)NIL, 0);
}

void
exitsh(int xno)
{
	/*
	 * Arrive here from `FATAL' errors
	 *  a) exit command,
	 *  b) default trap,
	 *  c) fault with no trap set.
	 *
	 * Action is to return to command level or exit.
	 */
	exitval = xno;
	flags |= eflag;
	if ((flags & (forked | errflg | ttyflg)) != ttyflg)
		done(0);
	else {
		clearup();
		restore(0);
		clear_buff();
		execbrk = breakcnt = funcnt = 0;
		longjmp(errshell, 1);
	}
}

void
done(int sig)
{
	char *t;

	USED(sig);
	if (t = trapcom[0]) {
		trapcom[0] = 0;
		execexp(t, 0);
		free((free_t)t);
	} else
		chktrap();

	rmtemp((struct ionod *)NIL);
	rmfunctmp();
	exit(exitval);
}

/*
 * unlink any files "above" base on a stack of files.
 */
void
rmtemp(struct ionod *base)
{
	/* was "iotemp > base" when using sbrk */
	while (iotemp != NIL && iotemp != base) {
		unlink(iotemp->ioname);
		free(iotemp->iolink);
		iotemp = iotemp->iolst;
	}
}

void
rmfunctmp(void)
{
	for (; fiotemp; fiotemp = fiotemp->iolst)
		unlink(fiotemp->ioname);
}
