/*	@(#)name.c	1.5	*/
/*
 * UNIX shell
 *
 * Bell Telephone Laboratories
 */

#include "defs.h"
#include "sym.h"

static void namwalk(struct namnod *np);

#define NILNM (struct namnod *)NIL

struct namnod ps2nod = { NILNM, NILNM, ps2name };
struct namnod cdpnod = { NILNM, NILNM, cdpname };
struct namnod ifsnod = { NILNM, NILNM, ifsname };
struct namnod ps1nod = { &pathnod, &ps2nod, ps1name };
struct namnod histnod = { &cdpnod, NILNM, histname };
struct namnod homenod = { &histnod, &ifsnod, homename };
struct namnod pathnod = { NILNM, NILNM, pathname };
struct namnod mailnod = { &homenod, &ps1nod, mailname };
struct namnod *namep = &mailnod;	/* top of the name tree */

/* ========	variable and string handling	======== */

int
syslook(char *w, struct sysnod syswds[], int n)
{
	int	low, mid, high, cond;

	if (w == 0 || *w == 0)
		return 0;

	low = 0;
	high = n - 1;
	while (low <= high) {
		mid = (low + high) / 2;

		if ((cond = strcmp(w, syswds[mid].sysnam)) < 0)
			high = mid - 1;
		else if (cond > 0)
			low = mid + 1;
		else
			return syswds[mid].sysval;
	}
	return 0;
}

void
setlist(struct argnod *arg, int xp)
{
	if (flags & exportflg)
		xp |= N_EXPORT;

	while (arg) {
		char *s = mactrim(arg->argval);
		setname(s, xp);
		arg = arg->argnxt;
		if (flags & execpr) {
			prs(s);
			if (arg)
				blank();
			else
				newline();
		}
	}
}

void
setname(char *argi, int xp)	/* does parameter assignments */
{
	char *argscan;
	struct namnod *n;

	for(argscan = argi; !ctrlchar(*argscan); argscan++)
		if (*argscan == '=') {
			*argscan = 0;	/* make name a cohesive string */

			n = lookup(argi);
			*argscan++ = '=';
			attrib(n, xp);
			if (xp & N_ENVNAM)
				n->namenv.val = n->namval.val = argscan;
			else
				assign(n, argscan);
			return;
		} else if ((xp & N_ENVNAM) && *argscan == '(') {
			/*
			 * flags==0 when we are called, so flags&ttyflg==0, so
			 * if there's an error we will exit
			 */
			execexp(argi, 0);
			*argscan = 0;
			n = lookup(argi);
			attrib(n, xp);
			return;
		}
	failed(argi, notid, 0);
}

void
replace(char **a, char *v)
{
	if (*a != 0)
		free((free_t)*a);
	*a = make(v);
}

void
dfault(struct namnod *n, char *v)
{
	if (n->namval.val == 0)
		assign(n, v);
}

void
assign(struct namnod *n, char *v)
{
	if (!(n->namflg & N_ENVNAM))
		free_val(&n->namval);
	else
		n->namflg &= ~N_ENVNAM;

	n->namval.val = make(v);
	n->namval.flg = N_VAR;

	if (flags & prompt)
		if (n == &mailnod)
			setmail(n->namval.val);
}

int
readvar(char **names)
{
	char c;
	int rc = 0;
	struct fileblk fb;
	struct fileblk *f = &fb;
	struct namnod *n = lookup(*names++); /* done now to avoid storage mess */
	int rel = relstak();

	push(f);
	initf(dup(0));

	if (lseek(0, 0, 1) == -1)	/* pipe or stream? */
		f->fsiz = 1;

	/*
	 * strip leading IFS characters
	 */
	while ((any((c = nextc(0)), ifsnod.namval.val)) && !(eolchar(c)))
		;
	for (;;)
		if ((*names && any(c, ifsnod.namval.val)) || eolchar(c)) {
			zerostak();
			assign(n, absstak(rel));
			setstak(rel);
			if (*names)
				n = lookup(*names++);
			else
				n = 0;
			if (eolchar(c))
				break;
			/* strip imbedded IFS characters */
			while ((any((c = nextc(0)), ifsnod.namval.val)) &&
			    !(eolchar(c)))
				;
		} else {
			pushstak(c);
			c = nextc(0);

			if (eolchar(c)) {
				char *top = staktop;

				while (any(*(--top), ifsnod.namval.val))
					;
				staktop = top + 1;
			}
		}
	while (n) {
		assign(n, nullstr);
		if (*names)
			n = lookup(*names++);
		else
			n = 0;
	}

	if (eof)
		rc = 1;
	lseek(0, f->fnxt - f->fend, 1);
	pop();
	return rc;
}

void
assnum(char **p, int i)
{
	itos(i);
	replace(p, numbuf);
}

char *
make(char *v)
{
	char *p;

	if (v) {
		movstr(v, p = alloc(length(v)));
		return p;
	} else
		return 0;
}

struct namnod *
lookup(char *nam)
{
	struct namnod *nscan = namep;
	struct namnod **prev = 0;
	int lr;

	if (!chkid(nam))
		failed(nam, notid, 0);
	while (nscan) {
		if ((lr = strcmp(nam, nscan->namid)) == 0)
			return nscan;
		else if (lr < 0)
			prev = &nscan->namlft;
		else
			prev = &nscan->namrgt;
		nscan = *prev;
	}
	/*
	 * add name node
	 */
	nscan = (struct namnod *)alloc(sizeof *nscan);
	nscan->namlft = nscan->namrgt = NIL;
	nscan->namid = make(nam);
	nscan->namval.val = 0;
	nscan->namval.flg = N_VAR;
	nscan->namenv.val = 0;
	nscan->namenv.flg = N_VAR;
	nscan->namflg = N_DEFAULT;
	*prev = nscan;
	return nscan;
}

BOOL
chkid(char *nam)
{
	if (nam == NIL)
		return 0;			/* false */
	while(!ctrlchar(*nam) && (*nam&QUOTE)==0 && *nam != '(' && *nam != '=')
		nam++;
	return *nam==0;
}

static void (*namfn)(struct namnod *);

void
namscan(void (*fn)(struct namnod *))
{
	namfn = fn;
	namwalk(namep);
}

static void
namwalk(struct namnod *np)
{
	if (np) {
		namwalk(np->namlft);
		(*namfn)(np);
		namwalk(np->namrgt);
	}
}

void
printnam(struct namnod *n)
{
	char	*s;

	sigchk();

	if (n->namval.flg & N_FUNCTN) {
		prs_buff(strf(n));
		prc_buff(NL);
	} else if (s = n->namval.val) {
		prs_buff(n->namid);
		prc_buff('=');
		prs_buff(quotedstring(s));
		prc_buff(NL);
	}
}

void
pushstr(char *s)
{
	do {
		pushstak(*s);
	} while (*s++);
	--staktop;
}

static char *
staknam(struct namnod *n)
{
	if (n->namval.flg & N_FUNCTN)
		pushstr(strf(n));
	else {
		pushstr(n->namid);
		pushstak('=');
		pushstr(n->namval.val);
	}
	return getstak(staktop + 1 - (char *)(stakbot));
}

static int namec;

void
exname(struct namnod *n)
{
	int flg = n->namflg;

	if (!(flg & N_ENVNAM)) {
		n->namflg |= N_ENVNAM;
		if (flg & N_EXPORT) {
			free_val(&n->namenv);
			n->namenv = n->namval;
		} else {
			free_val(&n->namval);
			n->namval = n->namenv;
		}
	}
	n->namflg &= N_ENVNAM;
	if (n->namval.val)
		namec++;
}

void
printexp(struct namnod *n)
{
	if (n->namflg & N_EXPORT) {
		prs_buff(export);
		prc_buff(SP);
		prs_buff(n->namid);
		prc_buff(NL);
	}
}

void
setup_env(void)
{
	char **e = environ;

	while (*e)
		setname(*e++, N_ENVNAM);
}

void
killenvfn(struct namnod *n)
{
	if (n->namflg & N_ENVNAM && n->namval.flg & N_FUNCTN) {
		freefunc(n->namval.val);
		n->namval.val = N_VAR;
		n->namval.flg = 0;
	}
}

void
prot_env(void)
{
	namscan(killenvfn);
	if (ifsnod.namval.val)
		free((free_t)ifsnod.namval.val);
	ifsnod.namval.val = 0;
	dfault(&ifsnod, sptbnl);
}

static char **argnam;

void
pushnam(struct namnod *n)
{
	if (n->namval.val || n->namval.flg & N_FUNCTN)
		*argnam++ = staknam(n);
}

char **
shsetenv(void)
{
	char **er;

	namec = 0;
	namscan(exname);

	argnam = er = (char **)getstak((namec + 1) * sizeof(char *));
	namscan(pushnam);
	*argnam++ = NIL;
	return er;
}

struct namnod *
findnam(char *nam)
{
	int lr;
	struct namnod *nscan = namep;

	if (!chkid(nam))
		return 0;
	while (nscan)
		if ((lr = strcmp(nam, nscan->namid)) == 0)
			return nscan;
		else if (lr < 0)
			nscan = nscan->namlft;
		else
			nscan = nscan->namrgt;
	return 0;
}

int
special(char *s)
{
	return strcmp(s, "builtin") == 0 || strcmp(s, ifsname) == 0 ||
		strcmp(s, pathname) == 0 || strcmp(s, ps1name) == 0 ||
		strcmp(s, ps2name) == 0;
}

void
free_val(struct namx *nx)
{
	if (nx->flg & N_FUNCTN)
		freefunc(nx->val);
	else
		free((free_t)nx->val);
}

void
unset_name(char *name)
{
	struct namnod *n;

	if (n = findnam(name)) {
		if (special(name))
			failed(name, badunset, 0);

		if (!(n->namflg & N_ENVNAM))
			free_val(&n->namval);
		free_val(&n->namenv);

		n->namval.val = N_VAR;
		n->namval.flg = 0;
		n->namenv = n->namval;
		n->namflg = N_DEFAULT;

		if (flags & prompt && n == &mailnod)
			setmail((char *)0);
	}
}
