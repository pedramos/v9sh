/*	@(#)fault.c	1.5	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include "defs.h"

char	*trapcom[MAXTRAP];
BOOL	trapjmp;	/* flag: a slow sys call is happening (4.2+bsd only) */
BOOL	trapflg[MAXTRAP];

/* sorry for the ifdefs. the defined signals vary between unix and posix. */
void 	(*(sigval[MAXTRAP]))(int) = {
	0,
[SIGHUP] = done,	/* hangup */
[SIGINT] = fault,	/* interrupt */
[SIGQUIT] = fault,	/* quit */
[SIGILL] = done,	/* illegal instruction */
#ifdef SIGTRAP
[SIGTRAP] = done,	/* trace trap */
#endif
#ifdef SIGIOT
[SIGIOT] = done,
#endif
#ifdef SIGEMT
[SIGEMT] = done,
#endif
#ifdef SIGABRT
[SIGABRT] = done,
#endif
[SIGFPE] = done,	/* float pt. exp */
[SIGKILL] = 0,		/* kill */
#ifdef SIGBUS
[SIGBUS] = done,	/* bus error */
#endif
[SIGSEGV] = 0,	/* memory faults: dump core for debugging new stak.c */
#ifdef SIGSYS
[SIGSYS] = done,	/* bad sys call */
#endif
[SIGPIPE] = done,	/* bad pipe write */
[SIGALRM] = fault,	/* alarm */
[SIGTERM] = fault,	/* software termination */
	/* additional BSD signals follow */
};

void setsig(int);

/* ========	fault handling routines	   ======== */

void
fault(int sig)
{
	int flag;

	signal(sig, (sigarg_t)fault);
	if (sig == SIGALRM) {
		if (flags & waiting)
			done(0);
	} else {
		flag = (trapcom[sig] ? TRAPSET : SIGSET);
		trapnote |= flag;
		trapflg[sig] |= flag;
		if (sig == SIGINT) {
			wasintr++;
			if (trapjmp) { /* slow 4.2bd syscall was interrupted? */
				trapjmp = 0;
				/* longjmp to avoid resuming system call */
				longjmp(INTbuf, 1);
			}
		}
	}
}

void
stdsigs(void)
{
	setsig(SIGHUP);
	setsig(SIGINT);
	ignsig(SIGQUIT);
	setsig(SIGPIPE);
	setsig(SIGALRM);
	setsig(SIGTERM);
#ifdef SIGSTOP			/* BSD job-control signals */
	setsig(SIGSTOP);
	setsig(SIGTSTP);
	setsig(SIGCONT);
	setsig(SIGTTIN);
	setsig(SIGTTOU);
#endif
#ifdef SIGXCPU
	setsig(SIGXCPU);
#endif
#ifdef SIGXFSZ
	setsig(SIGXFSZ);
#endif
}

ignsig(int i)
{
	int s;

	if ((s = (signal(i, (sigarg_t)SIG_IGN) == SIG_IGN)) == 0)
		trapflg[i] |= SIGMOD;
	return s;
}

void
getsig(int i)
{
	if (trapflg[i] & SIGMOD || ignsig(i) == 0)
		signal(i, (sigarg_t)fault);
}

void
setsig(int i)
{
	if (ignsig(i) == 0)
		signal(i, (sigarg_t)sigval[i]);
}

void
oldsigs(void)
{
	int i;
	char *t;

	i = MAXTRAP;
	while (i--) {
		t = trapcom[i];
		if (t == 0 || *t)
			clrsig(i);
		trapflg[i] = 0;
	}
	trapnote = 0;
}

void
clrsig(int i)
{
	free(trapcom[i]);
	trapcom[i] = 0;
	if (trapflg[i] & SIGMOD) {
		trapflg[i] &= ~SIGMOD;
		signal(i, (sigarg_t)sigval[i]);
	}
}

/*
 * check for traps
 */
void
chktrap()
{
	int savxit, i = MAXTRAP;
	char *t;

	trapnote &= ~TRAPSET;
	while (--i)
		if (trapflg[i] & TRAPSET) {
			trapflg[i] &= ~TRAPSET;
			if (t = trapcom[i]) {
				savxit = exitval;
				execexp(t, 0);
				exitval = savxit;
				exitset();
			}
		}
}
