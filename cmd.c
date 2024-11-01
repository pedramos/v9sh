/*	@(#)cmd.c	1.5	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include	"defs.h"
#include	"sym.h"

static void	chksym(int);
static void	chkword(void);
static struct ionod *inout(struct ionod *);
static struct trenod *item(BOOL);
static struct trenod *list(int);
static struct trenod *makelist(int type, struct trenod *i, struct trenod *r);
static void	prsym(int);
static int	skipnl(void);
static void	synbad(void);
static struct regnod *syncase(int);
static struct trenod *term(int);

/* ======== storage allocation for functions ======== */

char *
getstor(int asize)
{
	if (fndef)
		return alloc(asize);
	else
		return getstak(asize);
}


/* ========	command line decoding	========*/

struct trenod *
makefork(int flgs, struct trenod *i)
{
	struct forknod *t;

	t = (struct forknod *)getstor(sizeof(struct forknod));
	t->forktyp = flgs|TFORK;
	t->forktre = i;
	t->forkio = 0;
	return (struct trenod *)t;
}

static struct trenod *
makelist(int type, struct trenod *i, struct trenod *r)
{
	struct lstnod *t = NIL;

	if (i == NIL || r == NIL)
		synbad();
	else {
		t = (struct lstnod *)getstor(sizeof(struct lstnod));
		t->lsttyp = type;
		t->lstlef = i;
		t->lstrit = r;
	}
	return (struct trenod *)t;
}

/*
 * cmd
 *	empty
 *	list
 *	list & [ cmd ]
 *	list [ ; cmd ]
 */
struct trenod *
cmd(int sym, int flg)
{
	struct trenod *i, *e;

	i = list(flg);
	if (wdval == NL) {
		if (flg & NLFLG) {
			wdval = ';';
			chkpr();
		}
	} else if (i == 0 && (flg & MTFLG) == 0)
		synbad();

	switch (wdval) {
	case '&':
		if (i)
			i = makefork(FINT | FPRS | FAMP, i);
		else
			synbad();
		/* fall through */

	case ';':
		if (e = cmd(sym, flg | MTFLG))
			i = makelist(TLST, i, e);
		else if (i==0 && (flg & MTFLG) == 0)
			synbad();
		break;

	case EOFSYM:
		if (sym == NL)
			break;
		/* fall through */

	default:
		if (sym)
			chksym(sym);
		break;
	}
	return i;
}

/*
 * list
 *	term
 *	list && term
 *	list || term
 */
static struct trenod *
list(int flg)
{
	struct trenod *r;
	int b;

	r = term(flg);
	while (r && ((b = (wdval == ANDFSYM)) || wdval == ORFSYM))
		r = makelist((b ? TAND : TORF), r, term(NLFLG));
	return r;
}

/*
 * term
 *	item
 *	item | term
 */
static struct trenod *
term(int flg)
{
	struct trenod *t, *left, *right;

	reserv++;
	if (flg & NLFLG)
		skipnl();
	else
		word();
	if ((t = item(TRUE)) && wdval == '|') {
		left = makefork(FPOU, t);
		right = makefork(FPIN | FPCL, term(NLFLG));
		return(makefork(0, makelist(TFIL, left, right)));
	} else
		return t;
}

static struct regnod *
syncase(int esym)
{
	struct argnod *argp;
	struct regnod *r;

	skipnl();
	if (wdval == esym)
		return 0;
	r = (struct regnod *)getstor(sizeof(struct regnod));
	r->regptr = 0;
	for (;;) {
		if (fndef) {
			argp = wdarg;
			wdarg = (struct argnod *)alloc(length(argp->argval) +
				BYTESPERWORD);
			movstr(argp->argval, wdarg->argval);
		}

		wdarg->argnxt = r->regptr;
		r->regptr = wdarg;
		if (wdval || (word() != ')' && wdval != '|'))
			synbad();
		if (wdval == '|')
			word();
		else
			break;
	}
	r->regcom = cmd(0, NLFLG | MTFLG);
	if (wdval == ECSYM)
		r->regnxt = syncase(esym);
	else {
		chksym(esym);
		r->regnxt = 0;
	}
	return r;
}

/*
 * item
 *
 *	( cmd ) [ < in  ] [ > out ]
 *	word word* [ < in ] [ > out ]
 *	if ... then ... else ... fi
 *	for ... while ... do ... done
 *	case ... in ... esac
 *	begin ... end
 */
static struct trenod *
item(BOOL flag)
{
	struct trenod *r;
	struct ionod *io;

	if (flag)
		io = inout((struct ionod *)0);
	else
		io = 0;
	switch (wdval) {
	case CASYM: {
		struct swnod *t;

		t = (struct swnod *)getstor(sizeof(struct swnod));
		r = (struct trenod *)t;

		chkword();
		if (fndef)
			t->swarg = make(wdarg->argval);
		else
			t->swarg = wdarg->argval;
		skipnl();
		chksym(INSYM);
		t->swlst = syncase(ESSYM);
		t->swtyp = TSW;
		break;
		}

	case IFSYM: {
		int	w;
		struct ifnod *t;

		t = (struct ifnod *)getstor(sizeof(struct ifnod));
		r = (struct trenod *)t;

		t->iftyp = TIF;
		t->iftre = cmd(THSYM, NLFLG);
		t->thtre = cmd(ELSYM | FISYM | EFSYM, NLFLG);
		t->eltre = ((w = wdval) == ELSYM? cmd(FISYM, NLFLG):
			(w == EFSYM? (wdval = IFSYM, item(0)): 0));
		if (w == EFSYM)
			return r;
		break;
		}

	case FORSYM: {
		struct fornod *t;

		t = (struct fornod *)getstor(sizeof(struct fornod));
		r = (struct trenod *)t;

		t->fortyp = TFOR;
		t->forlst = 0;
		chkword();
		if (fndef)
			t->fornam = make(wdarg->argval);
		else
			t->fornam = wdarg->argval;
		if (skipnl() == INSYM) {
			chkword();

			t->forlst = (struct comnod *)item(0);

			if (wdval != NL && wdval != ';')
				synbad();
			if (wdval == NL)
				chkpr();
			skipnl();
		} else if (wdval == ';') {
			reserv++;
			word();		/* eat the ';' */
		}
		chksym(DOSYM);
		t->fortre = cmd(ODSYM, NLFLG);
		break;
		}

	case WHSYM:
	case UNSYM: {
		struct whnod *t;

		t = (struct whnod *)getstor(sizeof(struct whnod));
		r = (struct trenod *)t;

		t->whtyp = (wdval == WHSYM ? TWH : TUN);
		t->whtre = cmd(DOSYM, NLFLG);
		t->dotre = cmd(ODSYM, NLFLG);
		break;
		}

	case '{':
		r = cmd('}', NLFLG);
		break;

	case '(': {
		struct parnod *p;

		p = (struct parnod *)getstor(sizeof(struct parnod));
		p->partre = cmd(')', NLFLG);
		p->partyp = TPAR;
		r = makefork(0, (struct trenod *)p);
		break;
		}

	default:
		if (io == 0)
			return 0;
		/* fall through */

	case 0: {
		struct comnod *t;
		struct argnod *argp;
		struct argnod **argtail, **argset = 0;
		int	keywd = 1;

		if (wdval != NL && (peekn = skipc()) == '(') {
			struct fndnod *f;
			struct ionod  *saveio;

			saveio = iotemp;
			peekn = 0;
			if (skipc() != ')')
				synbad();

			f = (struct fndnod *)getstor(sizeof(struct fndnod));
			r = (struct trenod *)f;

			f->fndtyp = TFND;
			if (fndef)
				f->fndnam = make(wdarg->argval);
			else
				f->fndnam = wdarg->argval;
			reserv++;
			fndef++;
			skipnl();
			f->fndval = (struct trenod *)item(TRUE);
			fndef--;

			/*
			 * ionods may have been pushed onto iotemp's
			 * stack, on top of *saveio.
			 */
			if (iotemp != saveio) {
				struct ionod *ioptr = iotemp;

				while (ioptr->iolst != saveio)
					ioptr = ioptr->iolst;
				/*
				 * ioptr now points at the item just
				 * "above" (nearer the stack top) *saveio.
				 *
				 * put the fiotemp stack below the
				 * newly-pushed ionods.
				 */
				ioptr->iolst = fiotemp;
				/*
				 * make fiotemp point at the top of the
				 * newly-pushed ionods.
				 */
				fiotemp = iotemp;
				/*
				 * make iotemp point at the top of the
				 * original iotemp stack.
				 */
				iotemp = saveio;
				/*
				 * now, iotemp's stack is as it was
				 * when iotemp was copied to saveio above.
				 * fiotemp's stack has had the newly-
				 * pushed ionods pushed onto it.
				 */
			}
			return r;
		} else {
			t = (struct comnod *)getstor(sizeof(struct comnod));
			r = (struct trenod *)t;

			t->comio = io;		/* initial io chain */
			argtail = &t->comarg;
			while (wdval == 0) {
				if (fndef) {
					argp = wdarg;
					wdarg = (struct argnod *)
						alloc(length(argp->argval) +
							BYTESPERWORD);
					movstr(argp->argval, wdarg->argval);
				}

				argp = wdarg;
				if (wdset && keywd) {
					argp->argnxt = (struct argnod *)argset;
					argset = (struct argnod **)argp;
				} else {
					*argtail = argp;
					argtail = &(argp->argnxt);
					keywd = flags & keyflg;
				}
				word();
				if (flag)
					t->comio = inout(t->comio);
			}

			t->comtyp = TCOM;
			t->comset = (struct argnod *)argset;
			*argtail = 0;

			return r;
		}
	}
	/* end of cases default and 0 */
	}
	reserv++;
	word();
	if (io = inout(io)) {
		r = makefork(0,r);
		r->treio = io;
	}
	return r;
}


static int
skipnl(void)
{
	while ((reserv++, word() == NL))
		chkpr();
	return wdval;
}

static struct ionod *
inout(struct ionod *lastio)
{
	int	iof;
	struct ionod *iop;
	char	c;

	iof = wdnum;
	switch (wdval) {
	case DOCSYM:		/* << */
		iof |= IODOC;
		break;

	case APPSYM:		/* >> */
	case '>':
		if (wdnum == 0)
			iof |= 1;
		iof |= IOPUT;
		if (wdval == APPSYM) {
			iof |= IOAPP;
			break;
		}
		/* fall through */

	case '<':
		if ((c = nextc(0)) == '&')
			iof |= IOMOV;
		else if (c == '>')
			iof |= IORDW;
		else
			peekc = c | MARK;
		break;

	default:
		return lastio;
	}

	chkword();
	iop = (struct ionod *)getstor(sizeof(struct ionod));

	if (fndef)
		iop->ioname = make(wdarg->argval);
	else
		iop->ioname = wdarg->argval;

	iop->iolink = 0;
	iop->iofile = iof;
	if (iof & IODOC) {
		iop->iolst = iopend;
		iopend = iop;
	}
	word();
	iop->ionxt = inout(lastio);
	return iop;
}

static void
chkword(void)
{
	if (word())
		synbad();
}

static void
chksym(int sym)
{
	int x = sym & wdval;

	if ((x & SYMFLG? x : sym) != wdval)
		synbad();
}

static void
prsym(int sym)
{
	if (sym & SYMFLG) {
		struct sysnod *sp = reserved;

		while (sp->sysval && sp->sysval != sym)
			sp++;
		prs(sp->sysnam);
	} else if (sym == EOFSYM)
		prs(endoffile);
	else {
		if (sym & SYMREP)
			prc(sym);
		if (sym == NL)
			prs("newline or ;");
		else
			prc(sym);
	}
}

static void
synbad(void)
{
	prp();
	prs(synmsg);
	if ((flags & ttyflg) == 0) {
		prs(atline);
		prn((int)standin->flin);
	}
	prs(colon);
	prc(LQ);
	if (wdval)
		prsym(wdval);
	else
		prs_cntl(wdarg->argval);
	prc(RQ);
	prs(unexpected);
	newline();
	exitsh(SYNBAD);
}
