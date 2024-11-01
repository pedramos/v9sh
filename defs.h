#ifndef _DEFS_H
#define _DEFS_H 1
/*	@(#)defs.h	1.7	*/
/*
 *	UNIX shell
 */

#include <stddef.h>
/* #include <stdint.h>	/* if you have it */
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>	/* on 9, declares lseek, etc. if after sys/types.h */

#include "mac.h"
#include "mode.h"
#include "name.h"
#include "ctype.h"
#include "stak.h"

#ifndef PLAN9
#define USED(x)		/* plan 9 compiler directive */
#endif

#define CPYSIZ 512	/* buffer size; must exceed longest here doc delim. */

#ifndef DIRSIZ		/* upper bound on a component */
#ifdef MAXNAMLEN
#define DIRSIZ MAXNAMLEN
#else
#define DIRSIZ 256
#endif			/* MAXNAMLEN */
#endif			/* DIRSIZ */

#ifndef MAXPATHLEN	/* defined in 4.2+bsd */
#define MAXPATHLEN 1024	/* upper bound on complete path (faked for Unix) */
#endif

#define EXECUTE	01

/* error exits from various parts of shell */
#define ERROR	1
#define SYNBAD	2
#define SIGFAIL	2000 /* mystery: truncates to 0320 as exit status; 3 in v7 */
#define SIGFLG	0200	/* bit to indicate killed by a signal */

/* command tree */
#define FPRS	0x0100
#define FINT	0x0200
#define FAMP	0x0400
#define FPIN	0x0800
#define FPOU	0x1000
#define FPCL	0x2000
#define FCMD	0x4000
#define COMMSK	0x00F0		/* command type (T* below) */
#define CNTMSK	0x000F		/* reference count, in low bits */

#define TCOM	0x0000
#define TPAR	0x0010
#define TFIL	0x0020
#define TLST	0x0030
#define TIF	0x0040
#define TWH	0x0050
#define TUN	0x0060
#define TSW	0x0070
#define TAND	0x0080
#define TORF	0x0090
#define TFORK	0x00A0
#define TFOR	0x00B0
#define TFND	0x00C0

/* execute table */
#define SYSSET	1
#define SYSCD	2
#define SYSEXEC	3
#define SYSNEWGRP	4
#define SYSTRAP	5
#define SYSEXIT	6
#define SYSSHFT	7
#define SYSWAIT	8
#define SYSCONT	9
#define SYSBREAK 10
#define SYSEVAL	11
#define SYSDOT	12
#define SYSTIMES	14
#define SYSXPORT	15
#define SYSNULL		16
#define SYSREAD		17
#define SYSUMASK	20
#define SYSRETURN	25
#define SYSUNS		26
#define SYSWHATIS	28
#define SYSBLTIN	29

/* Unix file descriptor used for input and output of shell */
#define INIO	19

/*io nodes*/
#define USERIO	10	/* file descriptor for user i/o? */
/* contents of iofile */
#define IOUFD	((1<<4)-1) /* mask for Unix file descriptor */
#define IODOC	(1<<4)	/* here document flag */
#define IOPUT	(1<<5)	/* output flag */
#define IOAPP	(1<<6)	/* appending-output flag */
#define IOMOV	(1<<7)	/* <& or >& flag */
#define IORDW	(1<<8)	/* read/write flag */
/* not contents of iofile */
#define INPIPE	0
#define OTPIPE	1

/* arg list terminator */
#define ENDARGS	0

/* signal oddities */
#ifdef INTSIGF			/* the old world */
typedef int (*sigret_t)();
typedef int (*sigarg_t)();
#else
typedef void (*sigret_t)();
typedef void (*sigarg_t)();
#endif

/* error catching */
extern int errno;

int voidtrash;			/* a place to void functions */
char *voidstring;

#define attrib(n,f)	((n)->namflg |= (f))
/* b must be a power of 2 */
#define round(a,b)	((((uintptr_t)(a) + (b)-1)) & ~((b)-1))
#define closepipe(x) (voidtrash = close((x)[INPIPE]), \
		      voidtrash = close((x)[OTPIPE]))
#define eq(a,b)		(strcmp(a, b) == 0)
#define max(a,b)	((a) > (b)? (a): (b))

/* temp files and io */
extern int output;
extern int ioset;
extern struct ionod *iotemp;	/* files to be deleted sometime */
extern struct ionod *fiotemp;	/* function files to be deleted sometime */
extern struct ionod *iopend;	/* documents waiting to be read at NL */
extern struct fdsave fdmap[];

/* substitution */
extern int dolc;
extern char **dolv;
extern struct dolnod *argfor;
extern struct argnod *gchain;

/* string constants */
extern char	atline[];
extern char	readmsg[];
extern char	colon[];
extern char	minus[];
extern char	nullstr[];
extern char	sptbnl[];
extern char	unexpected[];
extern char	endoffile[];
extern char	synmsg[];

/* name tree and words */
extern struct sysnod	reserved[];
extern int		no_reserved;
extern struct sysnod	commands[];
extern int		no_commands;

extern int wdval;
extern int wdnum;
extern int fndef;
extern struct argnod *wdarg;
extern int wdset;
extern BOOL reserv;

/* prompting */
extern char stdprompt[];
extern char supprompt[];
extern char profile[];

/* built in names */
extern struct namnod cdpnod;
extern struct namnod ifsnod;
extern struct namnod histnod;
extern struct namnod homenod;
extern struct namnod mailnod;
extern struct namnod pathnod;
extern struct namnod ps1nod;
extern struct namnod ps2nod;

/* special names */
extern char flagadr[];
extern char *pcsadr;
extern char *pidadr;
extern char *cmdadr;

extern char defpath[];

/* names always present */
extern char mailname[];
extern char homename[];
extern char pathname[];
extern char cdpname[];
extern char ifsname[];
extern char histname[];
extern char ps1name[];
extern char ps2name[];

/* transput */
extern char tmpout[];
extern char *tempname;
extern int serial;

extern struct fileblk *standin;

#define input	(standin->fdes)
#define eof	(standin->feof)

extern int peekc;
extern int peekn;
extern int histfd;
extern char *comdiv;
extern char devnull[];

/* flags */
#define noexec	01
#define sysflg	01
#define intflg	02
#define prompt	04
#define setflg	010
#define errflg	020
#define ttyflg	040
#define forked	0100
#define oneflg	0200
#define protflg	0400
#define waiting	01000
#define stdflg	02000
#define STDFLG	's'
#define execpr	04000
#define readpr	010000
#define keyflg	020000
#define nofngflg 0200000
#define exportflg 0400000

extern long flags;
extern int rwait;	/* flags read waiting */

/* error exits from various parts of shell */
extern jmp_buf subshell;
extern jmp_buf errshell;
extern jmp_buf INTbuf;	/* for interrupting system calls in 4.2+BSD */


/* fault handling */
#define MINTRAP	0
#define MAXTRAP	32

#define TRAPSET	2
#define SIGSET	4
#define SIGMOD	8
#define SIGCAUGHT 16

#ifdef BSD4_2
#define INTRSYSCALL 1  /* longjmp to avoid resuming interrupted slow sys call */
#else
#define INTRSYSCALL 0	/* don't longjmp on interrupts */
#endif

extern BOOL trapnote;
extern char *trapcom[];
extern BOOL trapflg[];
extern BOOL trapjmp;		/* for interrupting system calls in 4.2+BSD */

/* name tree and words */
extern char **environ;
extern char numbuf[];
extern char export[];

/* execflgs */
extern int exitval;
extern int retval;
extern BOOL execbrk;
extern int loopcnt;
extern int breakcnt;
extern int funcnt;

/* messages */
extern char mailmsg[];
extern char coredump[];
extern char badopt[];
extern char badparam[];
extern char unset[];
extern char badsub[];
extern char nospace[];
extern char nostack[];
extern char notfound[];
extern char notbltin[];
extern char badtrap[];
extern char baddir[];
extern char badshift[];
extern char execpmsg[];
extern char notid[];
extern char nofork[];
extern char noswap[];
extern char piperr[];
extern char badnum[];
extern char badexec[];
extern char badfile[];
extern char badreturn[];
extern char badunset[];
extern char nohome[];
extern char badfname[];

/* fork constant */
#define FORKLIM	32

extern int wasintr;	/* flag: interrupted while executing a wait */
extern int eflag;

/*
 * Find out if it is time to go away.
 * `trapnote' is set to SIGSET when fault is seen and
 * no trap has been set.
 */

#define sigchk() if (trapnote & SIGSET)	exitsh(exitval ? exitval : SIGFAIL)

#define exitset()	retval = exitval

/* function prototypes */
char	*alloc(uintptr_t);
int	any(char, char *);
void	assign(struct namnod *n, char *v);
void	assnum(char **p, int i);
void	await(int, int);
char	*catpath(char *path, char *name);
BOOL	chkid(char *);
void	chkmail(void);
int	chkopen(char *idf);
void	chkpipe(int *pv);
void	chkpr(void);
void	chktrap(void);
int	chk_access(char	*name);
void	clear_buff(void);
void	clearup(void);
void	clrsig(int i);
int	clsexec(int fd);
struct trenod *cmd(int sym, int flg);
void	copy(struct ionod *ioparg);
void	countnam(struct namnod *n);
int	create(char *s);
void	dfault(struct namnod *n, char *v);
void	dochdir(char *place, char *cdpath);
void	done(int);
void	error(char *s);
int	estabf(char *s);
void	execa(char *at[]);
void	execexp(char *s, intptr_t fd);
int	execute(struct trenod * argt, int, int execflg, int *pf1, int *pf2);
void	execprint(char **com);
void	exitsh(int xno);
void	exname(struct namnod *n);
int	expand(char *as, int rflg);
void	failed(char *s1, char *s2, int);
void	fault(int sig);
struct namnod *findnam(char *nam);
void	flushb(void);
#define free shfree		/* shfree(0) is harmless */
struct dolnod *freeargs(struct dolnod * blk);
void	freefunc(char *val);
void	freeio(struct ionod *);
void	freereg(struct regnod *);
void	freetree(struct trenod *);
void	free_arg(struct argnod *);
void	free_val(struct namx *nx);
int	getarg(struct comnod * ac);
char	*getpath(char *s);
void	getsig(int n);
char	*getstor(int);
int	gmatch(char *s, char *p);
int	ignsig(int n);
void	initf(int fd);
int	initio(struct ionod *iop, int);
void	itos(unsigned long long n);
int	length(char *s);
void	link_iodocs(struct ionod *i);
struct namnod *lookup(char *nam);
char	*macro(char *as);
char	*mactrim(char *s);
char	*make(char *v);
void	makearg(struct argnod *);
struct trenod *makefork(int flgs, struct trenod * i);
char	*movstr(char *a, char *b);
char	*movstrn(char *src, char *dest, int n);
void	mygetenv(void);
void	myrename(int, int);
void	namscan(void (*fn)(struct namnod *));
int	nextc(unsigned char);
char	*nextpath(char *path);
void	oldsigs(void);
DIR	*openqdir(char *);
int	options(int argc, char **argv);
int	pathopen(char *path, char *name);
int	pop(void);
void	popargs(void);
int	poptemp(void);
void	post(int pcsid);
void	postclr(void);
void	prarg(struct argnod *);
void	prc(char);
void	prc_buff(char c);
void	prf(struct trenod *);
void	prfqstr(char *);
void	prfstr(char *);
void	printexp(struct namnod *n);
void	printflg(struct namnod *n);
void	printnam(struct namnod *n);
void	prio(struct ionod *);
void	prn(unsigned long long n);
void	prn_buff(unsigned long long n);
void	prot_env(void);
void	prp(void);
void	prs(char *as);
void	prs_2buff(char *s, char *t);
void	prs_buff(char *s);
void	prs_cntl(char *s);
void	prt(time_t t);
void	push(struct fileblk *);
void	pushargs(char *argi[]);
void	pushnam(struct namnod *n);
void	pushstr(char *s);
void	pushtemp(int fd, struct tempblk *tb);
char	*quotedstring(char *s);
int	readc(void);
int	readvar(char **names);
void	replace(char **a, char *v);
void	restore(int);
void	rmtemp(struct ionod *base);
int	savefd(int);
void	*sbrk(uintptr_t);
char	**scan(int argn);
void	setargs(char *argi[]);
void	setlist(struct argnod * arg, int xp);
void	setmail(char *mailpath);
void	setname(char *argi, int xp);
void	settmp(void);
void	setup_env(void);
void	shfree(void *p);
char	**shsetenv(void);
char	*simple(char *s);
int	skipc(void);
char	*spname(char *name, int *score);
int	special(char *s);
void	stdsigs(void);
int	stoi(char *icp);
char	*strf(struct namnod *n);
void	subst(int in, int ot);
void	swap_iodoc_nm(struct ionod *);
int	syslook(char *w, struct sysnod syswds[], int);
int	tmpfil(struct tempblk *);
void	trim(char *at);
void	unset_name(char *name);
struct dolnod *useargs(void);
void	what_is(char *name);
int	word(void);
#endif			/* _DEFS_H */
