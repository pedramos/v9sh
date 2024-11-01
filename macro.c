/*	@(#)macro.c	1.5	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include	"defs.h"
#include	"sym.h"

static char	quote, quoted;	/* used locally */

static void	comsubst(void);
static void	flush(int);
static int	getch(char);

static void
copyto(char endch)
{
	char	c;

	while ((c = getch(endch)) != endch && c)
		pushstak(c | quote);
	zerostak();
	if (c != endch)
		error(badsub);
}

/*
 * skip chars up to }
 */
static void
skipto(char endch)
{
	char	c;

	while ((c = readc()) && c != endch)
		switch (c) {
		case SQUOTE:
			skipto(SQUOTE);
			break;

		case DQUOTE:
			skipto(DQUOTE);
			break;

		case DOLLAR:
			if (readc() == LBRACE)
				skipto(RBRACE);
			break;
		}
	if (c != endch)
		error(badsub);
}

/*
 * macro scanner.  just a *little* tricky.  deals with quoting, command
 * substitution, and $var expansion.  uses a private ctype.
 */
static int
getch(char endch)
{
	int	c, dolg;
	BOOL	bra, nulflg;
	char	d;
	char	idb[2];
	char	*argp, *v, *id;
	struct namnod *n;

retry:
	d = readc();
	if (!subchar(d))
		return d;
	if (d != DOLLAR) {
		if (d == endch)
			return d;
		else if (d == SQUOTE) {
			comsubst();
			goto retry;
		} else if (d == DQUOTE) {
			quoted++;
			quote ^= QUOTE;
			goto retry;
		}
		/* else, ordinary character (common case) */
		return d;
	}

	c = readc();
	if (!dolchar(c)) {
		peekc = c | MARK;		/* push back char after $ */
		return d;
	}

	dolg = 0;
	n = NIL;
	v = NIL;
	id = idb;
	bra = (c == LBRACE);
	if (bra)				/* ${var}, not $c? */
		c = readc();

	/* classify and evaluate $c */
	if (letter(c)) {			/* variable? */
		argp = (char *)relstak();
		while (alphanum(c)) {
			pushstak(c);
			c = readc();
		}
		zerostak();
		n = lookup(absstak(argp));
		setstak(argp);
		if (n->namval.flg & N_FUNCTN)
			error(badsub);
		v = n->namval.val;
		id = n->namid;
		peekc = c | MARK;		/* push back terminating char */
	} else if (digchar(c)) {		/* positional param? */
		*id = c;
		idb[1] = 0;
		if (astchar(c)) {
			dolg = 1;
			c = '1';
		}
		c -= '0';
		/* $0 is program (script) name */
		v = c == 0? cmdadr: c <= dolc? dolv[c]: (char *)(dolg = 0);
	} else if (c == '$')		/* pid? */
		v = pidadr;
	else if (c == '!')		/* last bg pid? */
		v = pcsadr;
	else if (c == '#') {		/* arg count? */
		itos(dolc);
		v = numbuf;
	} else if (c == '?') {		/* exit status? */
		itos(retval);
		v = numbuf;
	} else if (c == '-')		/* all flags? */
		v = flagadr;
	else if (bra)
		error(badsub);
	else
		goto retry;		/* unknown $c */
	
	/* process trailing ${var:[-?+=]blah}, if any */
	c = readc();
	if (c == ':' && bra) {		/* null and unset fix */
		nulflg = 1;
		c = readc();
	} else
		nulflg = 0;
	if (!defchar(c) && bra)
		error(badsub);
	argp = 0;
	if (!bra) {
		peekc = c | MARK;	/* not in ${var}, push it back */
		c = 0;
	} else if (c != RBRACE) {	/* consume rest of ${var}? */
		argp = (char *)relstak();
		if ((v == 0 || (nulflg && *v == 0)) ^ setchar(c))
			copyto(RBRACE);
		else
			skipto(RBRACE);
		argp = absstak(argp);
	}

	if (v && (!nulflg || *v != '\0')) {
		/* non-empty evaluated result in v, or ${var} with no : */
		char dlm = (*id == '*'? SP | quote: SP);	/* $[*@]? */

		if (c != '+')		/* not default? expand $*, $@ */
			for (;;) {
				if (*v == 0 && quote)
					pushstak(QUOTE);
				else
					while (c = *v++)
						pushstak(c|quote);

				if (dolg == 0 || ++dolg > dolc)
					break;
				v = dolv[dolg];
				pushstak(dlm);
			}
	} else if (*id == '@' && quoted) /* nil "$@"?  it vanishes */
		quoted = -1;		/* swallow the quote later */
	else if (argp) {		/* more defaults? */
		if (c == '?')
			failed(id, *argp? argp: badparam, 0);
		else if (c == '=')
			if (n) {
				trim(argp);
				assign(n, argp);
			} else
				error(badsub);
	} else if (flags & setflg)	/* not a var we know; is -u? */
		failed(id, unset, 0);
	goto retry;
}

/*
 * Strip "" and do $ substitution
 * Leaves result on top of stack
 */
char *
macro(char *as)
{
	BOOL	savqu = quoted;
	char	savq = quote;
	struct filehdr	fb;

	push((struct fileblk *)&fb);
	estabf(as);
	usestak();
	quote = 0;
	quoted = 0;
	copyto(0);
	pop();
	if (quoted && (stakbot == staktop))
		pushstak(QUOTE);
	/*
	 * above is the fix for *'.c' bug
	 */
	quote = savq;
	quoted = savqu;
	return fixstak();
}

/*
 * command substn (`cmd`)
 */
static void
comsubst(void)
{
	int len;
	int pv[2];
	char d;
	char *argc;
	/* make top of stak permanent & remember its address for later */
	char *savptr = fixstak();
	struct ionod *iosavptr = iotemp;
	struct ionod *saviopend;
	struct trenod *t;
	struct fileblk cb;

	usestak();			/* begin new local stack */
	while ((d = readc()) != SQUOTE && d)
		pushstak(d);
	trim(argc = fixstak());		/* end local stack; make permanent */
	push(&cb);
	estabf(argc);

	saviopend = iopend;
	iopend = 0;
	t = makefork(FPOU, cmd(EOFSYM, MTFLG|NLFLG));

	/*
	 * this is done like this so that the pipe
	 * is open only when needed
	 */
	chkpipe(pv);
	initf(pv[INPIPE]);
	execute(t, 0, flags & errflg, (int *)0, pv);

	close(pv[OTPIPE]);
	iopend = saviopend;
	tdystak(savptr, iosavptr);

	/* make room for savptr on stak */
	len = strlen(savptr) + 1;
	if (len > BRKINCR)
		growstak(len-BRKINCR);

	/* copy top of stak at entry to current top of stak */
	staktop = movstr(savptr, stakbot);	/* staktop -> '\0' */
	while (d = readc())		/* read from pipe to `...` */
		pushstak(d | quote);	/* can change stakbot & staktop */
	await(0, 0);

	/* strip trailing newlines */
	while (stakbot < staktop)
		if ((*--staktop & STRIP) != NL) {
			++staktop;	/* staktop -> '\n' */
			break;
		}
	pop();
}

void
subst(int in, int ot)
{
	char	c;
	struct fileblk	fb;
	int	count = CPYSIZ;

	push(&fb);
	initf(in);
	/*
	 * DQUOTE used to stop it from quoting
	 */
	while (c = (getch(DQUOTE) & STRIP)) {
		pushstak(c);
		if (--count == 0) {
			flush(ot);
			count = CPYSIZ;
		}
	}
	flush(ot);
	pop();
}

static void
flush(int ot)
{
	int bytes;
	static char ewrite[] = "error writing temporary file";

	bytes = staktop - stakbot;
	if (write(ot, stakbot, bytes) != bytes)
		error(ewrite);
	if (flags & execpr)
		if (write(output, stakbot, bytes) != bytes)
			error(ewrite);
	staktop = stakbot;
}
