/*	@(#)hashserv.c	1.4	*/
/*
 *	UNIX shell
 *
 *	Bell Telephone Laboratories
 */

#include "defs.h"

int
findpath(char *name)
{
	char	*p, *path;
	int	count = 1, ok = 1, e_code = 1;

	path = getpath(name);
	while (path != 0) {
		path = catpath(path, name);
		p = curstak();
		if ((ok = chk_access(p)) == 0)
			break;
		e_code = max(e_code, ok);
		count++;
	}
	return ok? -e_code: count;
}

int
chk_access(char	*name)
{
	struct stat buf;

	if (stat(name, &buf) == 0 && (buf.st_mode&S_IFMT) == S_IFREG &&
	    access(name, EXECUTE) == 0)
		return 0;
	else
		return errno == EACCES? 3: 1;
}

void
pr_path(char *name, int count)
{
	char *path;

	path = getpath(name);
	while (--count && path)
		path = nextpath(path);

	catpath(path, name);
	prs_buff(curstak());
}

void
what_is(char *name)
{
	struct namnod *n;
	int	cnt, isparam = 0;

	if ((n = findnam(name)) && !(n->namval.flg & N_FUNCTN)) {
		printnam(n);
		isparam = n->namval.val!=0;
	}

	if (n && (n->namval.flg & N_FUNCTN)) {
		prs_buff(strf(n));
		prc_buff(NL);
		return;
	}

	if (syslook(name, commands, no_commands)) {
		prs_2buff("builtin ", name);
		prc_buff(NL);
		return;
	}
	if ((cnt = findpath(name)) > 0) {
		pr_path(name, cnt);
		prc_buff(NL);
	} else if (!isparam)
		failed(name, notfound, 0);
}
