/* @(#)defs.c 1.5 */
/*
 * UNIX shell
 */

#include "defs.h"
#include <sys/param.h>

#ifndef NOFILE
#define NOFILE NOFILES_MAX
#endif

/* temp files and io */

int	output = 2;
int	ioset;
/*
 * Both iotemp and fiotemp point at the top of a stack of struct ionods.
 */
struct ionod	*iotemp;	/* files to be deleted sometime */
struct ionod	*fiotemp;	/* function files to be deleted sometime */
struct ionod	*iopend;	/* documents waiting to be read at NL */
struct fdsave	fdmap[NOFILE];

/* substitution */
int	dolc;
char	**dolv;
struct dolnod *argfor;
struct argnod *gchain;

/* name tree and words */
int	wdval;
int	wdnum;
int	fndef;
struct argnod *wdarg;
int	wdset;
BOOL	reserv;

/* special names */
char	*pcsadr;
char	*pidadr;
char	*cmdadr;

/* transput */
char	*tempname;
int	serial;
int	peekc;
int	peekn;
char	*comdiv;

long	flags;
int	rwait;		/* flags read waiting */

/* error exits from various parts of shell */
jmp_buf	subshell;
jmp_buf	errshell;
jmp_buf	INTbuf;		/* for interrupting system calls in 4.2+BSD */

/* fault handling */
BOOL	trapnote;

/* execflgs */
int	exitval;
int	retval;
BOOL	execbrk;
int	loopcnt;
int	breakcnt;
int	funcnt;

int	wasintr;	/* used to tell if wait was interrupted */
int	eflag;

/* The following stuff is from stak.h; should be in a struct */
char *staktop;
char *stakbot;
char *stakend;			/* Top of entire stack */
