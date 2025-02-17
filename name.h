/*	@(#)name.h	1.3	*/
/*
 *	UNIX shell
 *
 *	Bell Telephone Laboratories
 *
 */


#define N_FUNCTN 0004
#define N_EXPORT 0002
#define N_ENVNAM 0001
#define N_VAR	 0000

#define N_DEFAULT 0

struct namx
{
	char 	*val;
	char	flg;
};

struct namnod
{
	struct namnod *namlft;
	struct namnod *namrgt;
	char	*namid;
	struct namx namval;
	struct namx namenv;
	char	namflg;
};
