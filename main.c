/*	@(#)main.c	1.7	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include "defs.h"
#include "sym.h"
#ifdef EXECARGS
#include <execargs.h>	/* v7, actually: a word at stack base */
#endif

#ifndef TIMEOUT
#define alarm(s)		/* no time-outs usually */
#define TIMEOUT 0
#endif

#define	TMPNAM	(sizeof "/tmp/sh" - 1)	/* offset into tmpout */

static BOOL	beenhere = FALSE;
char		tmpout[32] = "/tmp/sh-";
struct fileblk	stdfile;
struct fileblk *standin = &stdfile;

static long	oldmboxsize = 0;
static int	mailchk = 60;
static char	*mailp;
static long	mod_time = 0;

static void	exfile(BOOL);
static void	Ldup(int, int);

int
main(int c, char *v[])
{
	char *flagc;

	/* renounce set-id; security fix courtesy of Michael Baldwin */
	setgid(getgid());
	setuid(getuid());

 	assert(BRKINCR > BYTESPERWORD);

	stdsigs();

	/*
	 * initialise storage allocation
	 */
	stakbot = 0;
	addblok();

	/*
	 * set names from userenv
	 */
	setup_env();

	/*
	 * look for options
	 * dolc is $#
	 */
	dolc = options(c, v);

	if (dolc < 2) {
		flags |= stdflg;
		flagc = flagadr;
		while (*flagc)
			flagc++;
		*flagc++ = STDFLG;
		*flagc = '\0';
	}
	if ((flags & stdflg) == 0)
		dolc--;
	dolv = v + c - dolc;
	dolc--;

	/*
	 * return here for shell file execution
	 * but not for parenthesis subshells
	 */
	setjmp(subshell);

	/*
	 * number of positional parameters
	 */
	replace(&cmdadr, dolv[0]);	/* cmdadr is $0 */

	/*
	 * set pidname '$$'
	 */
	assnum(&pidadr, getpid());

	/*
	 * set up temp file names
	 */
	settmp();

	/* override any imported IFS value with the default " \t\n" */
	assign(&ifsnod, sptbnl);

	if (beenhere++ == FALSE) {	/* ? profile */
		if ((flags&protflg) == 0 && *(simple(cmdadr)) == '-') {
			if ((input = pathopen(nullstr, profile)) >= 0) {
				exfile(ttyflg);
				flags &= ~ttyflg;
			}
		}
		/*
		 * open input file if specified
		 */
		if (comdiv) {
			estabf(comdiv);
			input = -1;
		} else {
			input = (flags & stdflg? 0: chkopen(cmdadr));
			comdiv--;
		}
	}
#ifdef EXECARGS
	else
		*execargs = dolv;	/* for `ps' cmd */
#endif

	exfile(0);
	done(0);
	return 0;
}

static void
exfile(BOOL prof)
{
	/* Must not be register variables due to setjmp */
	time_t mailtime = 0, curtime = 0;
	time_t *tp;

	tp = &mailtime;
	USED(tp);

	/*
	 * move input
	 */
	if (input > 0) {
		Ldup(input, INIO);
		input = INIO;
	}

	/*
	 * decide whether interactive
	 */
	if ((flags & intflg) ||
	    ((flags&oneflg) == 0 && isatty(output) && isatty(input))) {
		dfault(&ps1nod, (geteuid() ? stdprompt : supprompt));
		dfault(&ps2nod, readmsg);
		flags |= ttyflg | prompt;
		ignsig(SIGTERM);
		setmail(mailnod.namval.val);
	} else {
		flags |= prof;
		flags &= ~prompt;
	}

	if (setjmp(errshell) && prof) {
		close(input);
		return;
	}
	/*
	 * error return here
	 */

	loopcnt = peekc = peekn = 0;
	fndef = 0;
	iopend = 0;

	if (input >= 0)
		initf(input);
	/*
	 * command loop
	 */
	for (;;) {
		tdystak((char *)NIL, (struct ionod *)NIL);
		stakchk();		/* may reduce sbrk */
		exitset();

		if ((flags & prompt) && standin->fstak == 0 && !eof) {
			if (mailp) {
				curtime = time(&curtime);
				if ((curtime - mailtime) >= mailchk) {
					chkmail();
				        mailtime = curtime;
				}
			}

			prs(ps1nod.namval.val);
			alarm(TIMEOUT);
			flags |= waiting;
		}
		trapnote = 0;
		peekc = readc();
		if (eof)
			return;
		alarm(0);
		flags &= ~waiting;

		execute(cmd(NL, MTFLG), 0, eflag, 0, 0);
		eof |= (flags & oneflg);
	}
}

void
chkpr(void)
{
	if ((flags & prompt) && standin->fstak == 0)
		prs(ps2nod.namval.val);
}

void
settmp(void)
{
	itos(getpid());
	serial = 0;
	tempname = movstr(numbuf, &tmpout[TMPNAM]);
	tempname = movstr(".", tempname);
}

int
clsexec(int fd)
{
#ifdef USE_FIOCLEX
#include <sys/ioctl.h>
	return ioctl(fd, FIOCLEX, 0);
#else
	return fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
}

/*
 * dup file descriptor fa to fb, close fa (if different)
 * and set fb to close-on-exec.
 */
static void
Ldup(int fa, int fb)
{
	if (fa < 0 || fb < 0)
		return;
	if (fa != fb) {
		if (dup2(fa, fb) < 0)
			error("sh: dup2 in Ldup failed");
		close(fa);
	}
	clsexec(fb);
}

/*
 * Has mail arrived since we last checked?
 */
void
chkmail(void)
{
	struct stat statb;

	if (mailp && stat(mailp, &statb) >= 0) {	/* mailbox exists? */
		if (statb.st_size > oldmboxsize &&  /* bigger than last time? */
		    mod_time > 0 && statb.st_mtime > mod_time)	/* touched? */
			prs(mailmsg);
		mod_time = statb.st_mtime;
		oldmboxsize = statb.st_size;
	} else if (mod_time == 0)	/* no mailbox now & wasn't one before */
		mod_time = 1;		/* pretend there was one before */
}

void
setmail(char *mailpath)
{
	mailp = mailpath;
	if (mailpath != 0) {		/* new mailbox */
		mod_time = 0;		/* reset remembered characteristics */
		oldmboxsize = 0;
	}
}
