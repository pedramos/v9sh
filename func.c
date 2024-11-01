/*	@(#)func.c	1.2	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include "defs.h"

#define	prfc pushstak

void
freefunc(char *val)
{
	freetree((struct trenod *)val);
}

void
freetree(struct trenod *t)
{
	int type;

	if (t == NIL)
		return;
	if (t->tretyp & CNTMSK) {	/* 4-bit reference count > 0? */
		t->tretyp--;
		return;
	}

	type = t->tretyp & COMMSK;
	switch (type) {
	case TFND:
		free((free_t)fndptr(t)->fndnam);
		freetree(fndptr(t)->fndval);
		break;

	case TCOM:
		freeio(comptr(t)->comio);
		free_arg(comptr(t)->comarg);
		free_arg(comptr(t)->comset);
		break;

	case TFORK:
		freeio(forkptr(t)->forkio);
		freetree(forkptr(t)->forktre);
		break;

	case TPAR:
		freetree(parptr(t)->partre);
		break;

	case TFIL:
	case TLST:
	case TAND:
	case TORF:
		freetree(lstptr(t)->lstlef);
		freetree(lstptr(t)->lstrit);
		break;

	case TFOR: {
		struct fornod *f = (struct fornod *)t;

		free((free_t)f->fornam);
		freetree(f->fortre);
		if (f->forlst) {
			freeio(f->forlst->comio);
			free_arg(f->forlst->comarg);
			free_arg(f->forlst->comset);
			free((free_t)f->forlst);
		}
		break;
		}

	case TWH:
	case TUN:
		freetree(whptr(t)->whtre);
		freetree(whptr(t)->dotre);
		break;

	case TIF:
		freetree(ifptr(t)->iftre);
		freetree(ifptr(t)->thtre);
		freetree(ifptr(t)->eltre);
		break;

	case TSW:
		free((free_t)swptr(t)->swarg);
		freereg(swptr(t)->swlst);
		break;
	}
	free(t);
}

void
free_arg(struct argnod *argp)
{
	struct argnod *sav;

	while (argp) {
		sav = argp->argnxt;
		free(argp);
		argp = sav;
	}
}

void
freeio(struct ionod *iop)
{
	struct ionod *sav;

	while (iop) {
		if ((iop->iofile & IODOC) && fiotemp != 0) {
			unlink(iop->ioname);

			if (fiotemp == iop)
				fiotemp = iop->iolst;
			else {
				struct ionod *fiop = fiotemp;

				while (fiop->iolst != iop)
					fiop = fiop->iolst;

				fiop->iolst = iop->iolst;
			}
		}
		free((free_t)iop->ioname);
		free((free_t)iop->iolink);
		sav = iop->ionxt;
		free((free_t)iop);
		iop = sav;
	}
}

void
freereg(struct regnod *regp)
{
	struct regnod 	*sav;

	while (regp) {
		free_arg(regp->regptr);
		freetree(regp->regcom);
		sav = regp->regnxt;
		free(regp);
		regp = sav;
	}
}

/*
 * Leaves string on top of stack
 */
char *
strf(struct namnod *n)
{
	int rel = relstak();

	prfstr(n->namid);
	prfstr("(){\n");
	prf((struct trenod *)n->namval.val);
	prfstr("\n}");
	prfc('\0');
	USED(staktop);
	return setstak(rel);
}

void
prfstr(char *s)
{
	while (*s != '\0')
		prfc(*s++);
}

void
prf(struct trenod *t)
{
	sigchk();

	if (t) {
		int type;

		type = t->tretyp & COMMSK;
		switch(type) {
		case TFND: {
			struct fndnod *f = (struct fndnod *)t;

			prfstr(f->fndnam);
			prfstr("(){\n");
			prf(f->fndval);
			prfstr("\n}");
			break;
			}

		case TCOM: {
			prarg(comptr(t)->comset);
			prarg(comptr(t)->comarg);
			prio(comptr(t)->comio);
			break;
			}

		case TFORK:
			prf(forkptr(t)->forktre);
			prio(forkptr(t)->forkio);
			if (forkptr(t)->forktyp & FAMP)
				prfstr(" &");
			break;

		case TPAR:
			prfstr("(");
			prf(parptr(t)->partre);
			prfstr(")");
			break;

		case TFIL:
			prf(lstptr(t)->lstlef);
			prfstr(" | ");
			prf(lstptr(t)->lstrit);
			break;

		case TLST:
			prf(lstptr(t)->lstlef);
			prfc(NL);
			prf(lstptr(t)->lstrit);
			break;

		case TAND:
			prf(lstptr(t)->lstlef);
			prfstr(" && ");
			prf(lstptr(t)->lstrit);
			break;

		case TORF:
			prf(lstptr(t)->lstlef);
			prfstr(" || ");
			prf(lstptr(t)->lstrit);
			break;

		case TFOR: {
			struct argnod *arg;
			struct fornod *f = (struct fornod *)t;

			prfstr("for ");
			prfstr(f->fornam);

			if (f->forlst) {
				arg = f->forlst->comarg;
				prfstr(" in");

				while(arg != ENDARGS) {
					prfc(SP);
					prfqstr(arg->argval);
					arg = arg->argnxt;
				}
			}

			prfstr("\ndo\n");
			prf(f->fortre);
			prfstr("\ndone");
			break;
			}

		case TWH:
		case TUN:
			if (type == TWH)
				prfstr("while ");
			else
				prfstr("until ");
			prf(whptr(t)->whtre);
			prfstr("\ndo\n");
			prf(whptr(t)->dotre);
			prfstr("\ndone");
			break;

		case TIF: {
			struct ifnod *f = (struct ifnod *)t;

			prfstr("if ");
			prf(f->iftre);
			prfstr("\nthen\n");
			prf(f->thtre);

			if (f->eltre) {
				prfstr("\nelse\n");
				prf(f->eltre);
			}

			prfstr("\nfi");
			break;
			}

		case TSW: {
			struct regnod *swl;

			prfstr("case ");
			prfqstr(swptr(t)->swarg);
			prfstr(" in\n");

			swl = swptr(t)->swlst;
			while(swl) {
				struct argnod *arg = swl->regptr;

				if (arg) {
					prfqstr(arg->argval);
					arg = arg->argnxt;
				}

				while(arg) {
					prfc('|');
					prfqstr(arg->argval);
					arg = arg->argnxt;
				}

				prfstr(") ");
				prf(swl->regcom);
				prfstr(";;\n");
				swl = swl->regnxt;
			}
			prfstr("esac");
			break;
			}
		}
	}

	sigchk();
}

void
prarg(struct argnod *argp)
{
	while (argp) {
		prfqstr(argp->argval);
		prfc(SP);
		argp = argp->argnxt;
	}
}

void
prio(struct ionod *iop)
{
	int	iof;
	char	*ion;

	while (iop) {
		iof = iop->iofile;
		ion = iop->ioname;

		if (*ion) {
			itos(iof & IOUFD);
			prfstr(numbuf);

			if (iof & IODOC)
				prfstr("<");
			else if (iof & IOMOV) {
				if (iof & IOPUT)
					prfstr(">&");
				else
					prfstr("<&");

			} else if ((iof & IOPUT) == 0)
				prfc('<');
			else if (iof & IOAPP)
				prfstr(">>");
			else
				prfc('>');

			prfstr(ion);
			prfc(SP);
		}
		iop = iop->ionxt;
	}
}

void
prfqstr(char *p)
{
	int quote, equote, allquotes;
	char *t;
    more:
	quote = equote = allquotes = 0;
	if(*p == '"'){
		t = p;
		quote = equote = '"';
		allquotes = 1;
		while(*++t!='"')
			if(!(*t & QUOTE)){
				allquotes=0;
				break;
			}
		if(allquotes)
			quote='\'';
		prfc(quote);
		p++;
	}
	while(*p != equote){
		if(*p=='"')
			goto more;
		if(!allquotes && (*p & QUOTE))
			prfc('\\');
		prfc(*p++ & ~QUOTE);
	}
	if(quote){
		prfc(quote);
		if(*++p != 0)
			goto more;
	}
}
