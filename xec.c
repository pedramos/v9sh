/*	@(#)xec.c	1.8	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include "defs.h"
#include "sym.h"

enum {				/* exec_link bits */
	Fork =	0<<0,		/* flag: fork before exec */
	Nofork =1<<0,		/* flag: just exec, don't fork first */
	Linkedshift = 1,	/* linked==1 means free here doc links */
};

static int parent;
static char **execintern(struct trenod *t, char **com, int internal, int argn,
	char *a1, struct ionod *io);

/* ========	command execution	========*/

/* clean up before returning from execute() */
static int
endexecute(char *sav, struct ionod *iosavptr)
{
	sigchk();
	tdystak(sav, iosavptr);
	flags |= eflag;
	return exitval;
}

/*
 * `stakbot' is preserved by this routine
 */
int
execute(struct trenod *t, int exec_link, int errorflg, int *pf1, int *pf2)
{
	int treeflgs, type, linked, execflg;
	char *sav = savstak();
	char **com = 0;
	struct ionod *iosavptr = iotemp;

	sigchk();
	if (!errorflg)
		flags &= ~errflg;

	/* exit early if no work to do */
	if (t == NIL || execbrk != 0)
		return endexecute(sav, iosavptr);

	linked =  exec_link >> Linkedshift;
	execflg = exec_link & Nofork;

	treeflgs = t->tretyp;
	type = treeflgs & COMMSK;

	switch (type) {
	case TFND: {
		struct fndnod *f = (struct fndnod *)t;
		struct namnod *n = lookup(f->fndnam);

		exitval = 0;

		if (special(n->namid))
			failed(n->namid, badfname, 0);
		if (!(n->namflg & N_ENVNAM))
			free_val(&n->namval);

		if (funcnt)
			f->fndval->tretyp++;

		n->namval.val = (char *)f->fndval;
		n->namval.flg = N_FUNCTN;
		if (flags & exportflg)
			attrib(n, N_EXPORT);
		break;
		}

	case TCOM: {
		char *a1;
		int argn, internal;
		struct argnod *schain = gchain;
		struct ionod *io = t->treio;
		struct namnod *n;

		exitval = 0;
		gchain = 0;
		argn = getarg((struct comnod *)t);
		com = scan(argn);
		a1 = com[1];
		gchain = schain;

		if ((n = findnam(com[0])) && !(n->namval.flg&N_FUNCTN))
			n = 0;
		if ((internal = syslook(com[0], commands, no_commands))
		    || argn == 0)
			setlist(comptr(t)->comset, 0);

		if (argn && (flags&noexec) == 0) {
			/* print command if execpr */
			if (flags & execpr)
				execprint(com);

			if (n) {		/* function */
				int index;

				funcnt++;
				index = initio(io, 1);
				pushargs(com);
				execute((struct trenod *)n->namval.val,
					exec_link, errorflg, pf1, pf2);
				execbrk = 0;
				popargs();
				restore(index);
				funcnt--;
				break;
			} else if (internal) {
				/* com = */ execintern(t, com, internal, argn,
					a1, io);
				break;
			}
		} else if (t->treio == 0) {
			chktrap();
			break;
		}
		}
		/* end of case TCOM */
		/* FALLTHROUGH */

	case TFORK:
		exitval = 0;
		if (execflg && (treeflgs & (FAMP | FPOU)) == 0)
			parent = 0;
		else {
			int forkcnt = 1;

			if (treeflgs & (FAMP | FPOU)) {
				link_iodocs(iotemp);
				linked = 1;
			}

			/*
			 * FORKLIM (often 32) is the max period between forks -
			 * power of 2 usually.  Currently shell tries after
			 * 2,4,8,16, and 32 seconds and then quits.
			 */
			while ((parent = fork()) == -1) {
				if ((forkcnt *= 2) > FORKLIM)
					switch (errno) {
					case ENOMEM:
						error(noswap);
					default:
					case EAGAIN:
						error(nofork);
					}
				sigchk();
				alarm(forkcnt);
				pause();
			}
		}

		if (parent) {
			/*
			 * This is the parent branch of fork;
			 * it may or may not wait for the child
			 */
			if (treeflgs & FPRS && flags & ttyflg) {
				prn(parent);
				newline();
			}
			if (treeflgs & FPCL)
				closepipe(pf1);
			if ((treeflgs & (FAMP | FPOU)) == 0)
				await(parent, 0);
			else if ((treeflgs & FAMP) == 0)
				post(parent);
			else
				assnum(&pcsadr, parent);
			chktrap();
		} else {
			/* this is the forked branch (child) of execute */
			flags |= forked;
			fiotemp = 0;

			if (linked == 1) {
				swap_iodoc_nm(iotemp);
				/* magic guess: 3 is neither 0 nor 1 */
				exec_link |= 3<<Linkedshift | Fork;
			} else if (linked == 0)
				iotemp = 0;

			postclr();
			settmp();
			/*
			 * Turn off INTR and QUIT if `FINT'
			 * Reset ramaining signals to parent
			 * except for those `lost' by trap
			 */
			oldsigs();
			if (treeflgs & FINT) {
				signal(SIGINT, (sigarg_t)SIG_IGN);
				signal(SIGQUIT, (sigarg_t)SIG_IGN);
#ifdef NICE
				nice(NICE);
#endif
			}
			/*
			 * pipe in or out
			 */
			if (treeflgs & FPIN) {
				myrename(pf1[INPIPE], 0);
				close(pf1[OTPIPE]);
			}
			if (treeflgs & FPOU) {
				close(pf2[INPIPE]);
				myrename(pf2[OTPIPE], 1);
			}
			/*
			 * default std input for &
			 */
			if (treeflgs & FINT && ioset == 0)
				myrename(chkopen(devnull), 0);
			/*
			 * io redirection
			 */
			initio(t->treio, 0);

			if (type != TCOM) {
				execute(forkptr(t)->forktre, exec_link | Nofork,
					errorflg, NIL, NIL);
				/* get here only on failed execute */
			} else if (com[0] != ENDARGS) {
				eflag = 0;
				setlist(comptr(t)->comset, N_EXPORT);
				rmtemp((struct ionod *)NIL);
				execa(com);
				/* get here only on failed execa */
			}
			/* else no-op, other than redirection */
			done(0);
		}
		/* end of case TFORK */
		break;

	case TPAR:
		execute(parptr(t)->partre, exec_link, errorflg, NIL, NIL);
		done(0);

	case TFIL: {
		int pv[2];

		chkpipe(pv);
		if (execute(lstptr(t)->lstlef, Fork, errorflg, pf1, pv)==0)
			execute(lstptr(t)->lstrit, exec_link, errorflg, pv, pf2);
		else
			closepipe(pv);
		}
		break;

	case TLST:
		execute(lstptr(t)->lstlef, Fork, errorflg, NIL, NIL);
		execute(lstptr(t)->lstrit, exec_link, errorflg, NIL, NIL);
		break;

	case TAND:
		if (execute(lstptr(t)->lstlef, Fork, 0, NIL, NIL) == 0)
			execute(lstptr(t)->lstrit, exec_link, errorflg, NIL, NIL);
		break;

	case TORF:
		if (execute(lstptr(t)->lstlef, Fork, 0, NIL, NIL) != 0)
			execute(lstptr(t)->lstrit, exec_link, errorflg, NIL, NIL);
		break;

	case TFOR: {
		struct namnod *n = lookup(forptr(t)->fornam);
		char	**args;
		struct dolnod *argsav = 0;

		if (forptr(t)->forlst == 0) {
			args = dolv + 1;
			argsav = useargs();
		} else {
			struct argnod *schain = gchain;

			gchain = 0;
			trim((args = scan(getarg(forptr(t)->forlst)))[0]);
			gchain = schain;
		}
		loopcnt++;
		while (*args != ENDARGS && execbrk == 0) {
			assign(n, *args++);
			execute(forptr(t)->fortre, Fork, errorflg, NIL, NIL);
			if (breakcnt < 0)
				execbrk = (++breakcnt != 0);
		}
		if (breakcnt > 0)
			execbrk = (--breakcnt != 0);

		loopcnt--;
		argfor = (struct dolnod *)freeargs(argsav);
		}
		break;

	case TWH:
	case TUN: {
		int i = 0;

		loopcnt++;
		while (execbrk == 0 &&
		    (execute(whptr(t)->whtre, Fork, 0, NIL, NIL) == 0) ==
		    (type == TWH)) {
			i = execute(whptr(t)->dotre, Fork, errorflg, NIL, NIL);
			if (breakcnt < 0)
				execbrk = (++breakcnt != 0);
		}
		if (breakcnt > 0)
			execbrk = (--breakcnt != 0);

		loopcnt--;
		exitval = i;
		}
		break;

	case TIF:
		if (execute(ifptr(t)->iftre, 0, 0, NIL, NIL) == 0)
			execute(ifptr(t)->thtre, exec_link, errorflg, NIL, NIL);
		else if (ifptr(t)->eltre)
			execute(ifptr(t)->eltre, exec_link, errorflg, NIL, NIL);
		else
			exitval = 0;	/* force zero exit for if-then-fi */
		break;

	case TSW: {
		char *s, *r = mactrim(swptr(t)->swarg);
		struct regnod *regp;
		struct argnod *rex;

		regp = swptr(t)->swlst;
		while (regp) {
			for (rex = regp->regptr; rex; rex = rex->argnxt)
				if (gmatch(r, s = macro(rex->argval))
				    || (trim(s), eq(r, s))) {
					execute(regp->regcom, Fork,
						errorflg, NIL, NIL);
					regp = NIL;
					break;
				}
			if (regp)
				regp = regp->regnxt;
		}
		}
		break;
	}					/* end of switch(type) */
	exitset();
	return endexecute(sav, iosavptr);
}

static char **
execintern(struct trenod *t, char **com, int internal, int argn, char *a1,
	struct ionod *io)
{
	int i, f;
	short index;

	index = initio(io, (internal != SYSEXEC));
    isbltin:						/* from below */
	switch (internal) {
	case SYSBLTIN:
		if (--argn > 0) {
			com++;
			a1 = com[1];
			if (internal = syslook(com[0], commands, no_commands))
				goto isbltin;		/* to above */
			failed(*com, notbltin, 0);
		}
		break;

	case SYSDOT:
		if (a1)
			if ((f = pathopen(getpath(a1), a1)) < 0)
				failed(a1, notfound, 0);
			else
				execexp(NIL, f);
		break;

	case SYSTIMES: {
		struct tms timesbuf;

		times(&timesbuf);
		prt(timesbuf.tms_cutime);
		prc_buff(SP);
		prt(timesbuf.tms_cstime);
		prc_buff(NL);
		}
		break;

	case SYSEXIT:
		flags |= forked;	/* force exit */
		exitsh(a1? stoi(a1): retval);

	case SYSNULL:
		/* io = 0; */
		break;

	case SYSCONT:
		if (loopcnt) {
			execbrk = breakcnt = 1;
			if (a1)
				breakcnt = stoi(a1);
			if (breakcnt > loopcnt)
				breakcnt = loopcnt;
			else
				breakcnt = -breakcnt;
		}
		break;

	case SYSBREAK:
		if (loopcnt) {
			execbrk = breakcnt = 1;
			if (a1)
				breakcnt = stoi(a1);
			if (breakcnt > loopcnt)
				breakcnt = loopcnt;
		}
		break;

	case SYSTRAP:
		if (a1) {
			BOOL clear;

			if ((clear = digit(*a1)) == 0)
				++com;
			while (*++com) {
				if ((i = stoi(*com)) >= MAXTRAP || i < MINTRAP)
					failed(*com, badtrap, 0);
				else if (clear)
					clrsig(i);
				else {
					replace(&trapcom[i], a1);
					if (*a1)
						getsig(i);
					else
						ignsig(i);
				}
			}
		} else
			/* print out current traps */
			for (i = 0; i < MAXTRAP; i++)
				if (trapcom[i]) {
					prs_buff("trap ");
					prs_buff(quotedstring(trapcom[i]));
					prc_buff(SP);
					prn_buff(i);
					prc_buff(NL);
				}
		break;

	case SYSEXEC:
		com++;
		ioset = 0;
		/* io = 0; */
		if (a1 == 0)
			break;
		/* fall through */

	case SYSNEWGRP:
		oldsigs();
		execa(com);
		done(0);

	case SYSCD:
		if ((a1 != 0 && *a1 != 0) ||
		    (a1 == 0 && (a1 = homenod.namval.val))) {
			char *cdpath;

			if ((cdpath = cdpnod.namval.val) == 0 ||
			     *a1 == '/' ||
			     strcmp(a1, ".") == 0 || strcmp(a1, "..") == 0 ||
			     (*a1 == '.' &&
			     (*(a1+1) == '/' || *(a1+1) == '.' && *(a1+2) == '/')))
				cdpath = nullstr;
			dochdir(a1, cdpath);
		} else
			if (a1)
				failed(a1, baddir, 0);
			else
				error(nohome);
		break;

	case SYSSHFT: {
		int places;

		places = a1 ? stoi(a1) : 1;
		if ((dolc -= places) < 0) {
			dolc = 0;
			error(badshift);
		} else
			dolv += places;
		}
		break;

	case SYSWAIT:
		await(a1 ? stoi(a1) : -1, 1);
		break;

	case SYSREAD:
		rwait = 1;
		exitval = a1? readvar(&com[1]) : 0;
		rwait = 0;
		break;

	case SYSSET:
		if (a1) {
			int argc;

			argc = options(argn, com);
			if (argc > 1)
				setargs(com + argn - argc);
		} else if (comptr(t)->comset == 0)
			/*
			 * scan name chain and print
			 */
			namscan(printnam);
		break;

	case SYSXPORT: {
		struct namnod *name;

		exitval = 0;
		if (a1)
			while (*++com) {
				name = lookup(*com);
				attrib(name, N_EXPORT);
			} else
				namscan(printexp);
		}
		break;

	case SYSEVAL:
		if (a1)
			execexp(a1, (intptr_t)&com[2]);
		break;

	case SYSUMASK:
		if (a1) {
			int c;

			i = 0;
			while ((c = *a1++) >= '0' && c <= '7')
				i = (i << 3) + c - '0';
			umask(i);
		} else {
			int j;

			umask(i = umask(0));
			prc_buff('0');
			for (j = 6; j >= 0; j -= 3)
				prc_buff(((i >> j) & 07) +'0');
			prc_buff(NL);
		}
		break;

	case SYSRETURN:
		if (funcnt == 0)
			error(badreturn);

		execbrk = 1;
		exitval = (a1? stoi(a1): retval);
		break;

	case SYSWHATIS:
		exitval = 0;
		if (a1)
			while (*++com)
				what_is(*com);
		break;

	case SYSUNS:
		exitval = 0;
		if (a1)
			while (*++com)
				unset_name(*com);
		break;

	default:
		prs_buff("unknown builtin\n");
	}				/* end of switch(internal) */

	flushb();
	restore(index);
	chktrap();
	return com;			/* because com has changed */
}

void
dochdir(char *place, char *cdpath)
{
	char *dir;
	char bestplace[256], tempdir[256];
	int score, bestscore = 1000;
	struct stat sb;

	do {
		dir = cdpath;
		cdpath = catpath(cdpath, place);
		/* kludge: will call opendir, so must lock down the string */
		movstr(curstak(), tempdir);
		if (chdir(tempdir) < 0) {
			char *dir1;

			if (flags&ttyflg && (dir1 = spname(tempdir, &score)) &&
			    stat(dir1, &sb) == 0 &&
			    (sb.st_mode&S_IFMT) == S_IFDIR &&
			    access(dir1, EXECUTE) == 0) {
				/* dir1 is a searchable directory. */
				if (score < bestscore) {
					movstr(dir1, bestplace);
					bestscore = score;
				}
			}
		} else {
			if (strcmp(nullstr, dir) && *dir != ':' &&
			    any('/', tempdir) && flags&prompt) {
				prs_buff(tempdir);
				prc_buff(NL);
			}
			return;
		}
	} while (cdpath);
	if (bestscore < 1000 && chdir(bestplace) >= 0) {
		prs_buff(bestplace);
		prc_buff(NL);
		return;
	}
	failed(place, baddir, 0);
}

void
execexp(char *s, intptr_t f)
{
	struct fileblk	fb;

	push(&fb);
	if (s) {
		estabf(s);
		fb.feval = (char **)f;	/* ugh */
	} else if (f >= 0)
		initf(f);
	execute(cmd(NL, NLFLG|MTFLG), 0, flags & errflg, NIL, NIL);
	pop();
}

void
execprint(char **com)
{
	int argn = 0;

	prs(execpmsg);
	while(com[argn] != ENDARGS) {
		prs(quotedstring(com[argn++]));
		blank();
	}
	newline();
}
