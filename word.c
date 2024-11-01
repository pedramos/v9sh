/*	@(#)word.c	1.4	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include	"defs.h"
#include	"sym.h"

static int	readb(void);
static void	histc(int);

/* ========	character handling for command lines	========*/

/* scan input into words */
int
word(void)
{
	char	c, d;
	int	alpha = 1, lbrok, rbrok;
	struct argnod *arg;

	wdnum = wdset = 0;
	for (;;) {
		c = skipc();
		if (c == COMCHAR) {
			while ((c = readc()) != NL && c != CEOF)
				;		/* consume comment */
			peekc = c;		/* push back terminator */
		} else
			break;		/* not in comment - white space loop */
	}
	if (!eofmeta(c)) {			/* ordinary char? */
		/* pushstak() starting at what will be arg->argval[0] */
		staktop += BYTESPERWORD;

		lbrok = rbrok = 0;
		do {
			if (c == LITERAL) {
				pushstak(DQUOTE);
				while ((c = readc()) && c != LITERAL) {
					pushstak(c | QUOTE);
					if (c == NL)
						chkpr();
				}
				pushstak(DQUOTE);
			} else {
				pushstak(c);
				if (c == '=')
					wdset |= alpha;
				/* hack for ${} */
				else if (c == DOLLAR)
					lbrok++;
				else if (lbrok) {
					if (c == LBRACE)
						rbrok++;
					lbrok = 0;
				} else if (rbrok && c == RBRACE)
					rbrok--;
				if (!alphanum(c))
					alpha = 0;
				if (qotchar(c)) {  /* quoted string? copy */
					d = c;
					while ((pushstak(c = nextc(d))) != 0 &&
					    c != d)
						if (c == NL)
							chkpr();
				}
			}
		} while (c = nextc(0), !eofmeta(c) ||
			lbrok && c == LBRACE || rbrok && c == RBRACE);
		arg = (struct argnod *)fixstak();
		if (!letter(arg->argval[0]))	/* not a plain word? */
			wdset = 0;

		peekn = c | MARK;		/* push back terminator */
		if (arg->argval[1] == 0 &&
		    (d = arg->argval[0], digit(d)) && (c == '>' || c == '<')) {
			word();
			wdnum = d - '0';	/* fd redirection like 3> */
		} else				/* check for reserved words */
			if (reserv == FALSE || (wdval = syslook(arg->argval,
			    reserved, no_reserved)) == 0) {
				wdarg = arg;
				wdval = 0;
			}
	} else if (dipchar(c)) {	/* possible doubled-char op. [|&;<>]? */
		if ((d = nextc(0)) == c) {	/* is actually doubled? */
			wdval = c | SYMREP;
			if (c == '<') {
				d = nextc(0);
				peekn = d | MARK;
			}
		} else {
			peekn = d | MARK;
			wdval = c;
		}
	} else {
		if ((wdval = c) == CEOF)
			wdval = EOFSYM;
		if (iopend && eolchar(c)) {
			copy(iopend);
			iopend = 0;
		}
	}
	reserv = FALSE;
	return wdval;
}

int
skipc(void)
{
	char c;

	while (c = nextc(0), space(c))
		;
	return c;
}

int
nextc(unsigned char quote)
{
	unsigned char c, d;

retry:
	if ((d = readc()) == ESCAPE)
		if ((c = readc()) == NL) {
			chkpr();
			goto retry;
		} else if (quote && c != quote && !escchar(c))
			peekc = c | MARK;
		else
			d = c | QUOTE;
	return d;
}

int
readc(void)
{
	char	c;
	int	len;
	struct fileblk *f;

	/* return pushed-back char, if any; peekn takes precedence */
	if (peekn) {
		peekc = peekn;
		peekn = 0;
	}
	if (peekc) {
		c = peekc;
		peekc = 0;
		return c;
	}
	f = standin;
retry:
	if (f->fnxt != f->fend) {
		if ((c = *f->fnxt++) == 0)
			if (f->feval)
				if (estabf(*f->feval++))
					c = CEOF;
				else
					c = SP;
			else
				goto retry;	/* = c = readc(); */
		if (standin->fstak == 0) {
			if (flags & readpr)
				prc(c);
			if (flags & prompt && histnod.namval.val)
				histc(c);
		}
		if (c == NL)
			f->flin++;
	} else if (f->feof || f->fdes < 0) {
		c = CEOF;
		f->feof++;
	} else if ((len = readb()) <= 0) {
		close(f->fdes);
		f->fdes = -1;
		c = CEOF;
		f->feof++;
	} else {
		f->fend = (f->fnxt = f->fbuf) + len;
		goto retry;
	}
	return c;
}

static int
readb(void)
{
	struct fileblk *f = standin;
	int	len;

	if (setjmp(INTbuf) == 0)
		trapjmp = INTRSYSCALL;		/* slow syscall coming */
	do {
		if (trapnote & SIGSET) {
			newline();
			sigchk();
		} else if ((trapnote & TRAPSET) && rwait > 0) {
			newline();
			chktrap();
			clearup();
		}
	} while ((len = read(f->fdes, f->fbuf, f->fsiz)) < 0 &&
		errno == EINTR && trapnote);
	trapjmp = 0;				/* slow syscall is over */
	return len;
}

int histfd = 0;

static void
histc(int c)
{
	static char histbuf[128];
	static char *histp = histbuf;

	if (histfd == 0)
		if ((histfd = open(histnod.namval.val, 1)) < 0 &&
		    (histfd = creat(histnod.namval.val, 0666)) < 0)
			return;
	if (histfd > 0) {
		*histp++ = c;
		if (c == '\n' || c == ';' || histp >= &histbuf[sizeof histbuf]) {
			lseek(histfd, 0L, 2);
			/*
			 * ignore history write failures since otherwise,
			 * the shell could be rendered useless.
			 */
			write(histfd, histbuf, histp-histbuf);
			histp = histbuf;
		}
	}
}
