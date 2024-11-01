/*	@(#)mode.h	1.5	*/
/*
 *	UNIX shell
 */

typedef short BOOL;

#define BYTESPERWORD	(sizeof (char *))
#define	NIL	0

/* heap storage; used only by stak.c and in union address */
struct blk {
	struct blk *word;	/* pointer to previous stack item */
};

typedef void *free_t;		/* type of arguments to free() */

#define	BUFSIZE	128
struct fileblk
{
	int	fdes;
	unsigned flin;
	BOOL	feof;
	unsigned char fsiz;	/* big enough to hold BUFSIZE constant */
	unsigned char *fnxt;
	unsigned char *fend;
	char	**feval;
	struct fileblk *fstak;
	unsigned char fbuf[BUFSIZE];
};

struct tempblk
{
	int fdes;
	struct tempblk *fstak;
};


/* for files not used with file descriptors */
struct filehdr
{
	int	fdes;
	unsigned flin;
	BOOL	feof;
	unsigned char fsiz;
	unsigned char *fnxt;
	unsigned char *fend;
	char	**feval;
	struct fileblk *fstak;
	unsigned char _fbuf[1];		/* may actually be longer */
};

struct sysnod
{
	char	*sysnam;
	int	sysval;
};

/* this node is a proforma for those that follow */
struct trenod
{
	int	tretyp;
	struct ionod	*treio;
};

/* dummy for access only */
struct argnod
{
	struct argnod	*argnxt;
	char	argval[1];		/* may actually be longer */
};

struct dolnod
{
	struct dolnod	*dolnxt;
	struct dolnod	*dolfor;
	int	doluse;
	char	*dolarg[1];		/* may actually be longer */
};

struct forknod
{
	int	forktyp;
	struct ionod	*forkio;
	struct trenod	*forktre;
};

struct comnod
{
	int	comtyp;
	struct ionod	*comio;
	struct argnod	*comarg;
	struct argnod	*comset;
};

struct fndnod
{
	int 	fndtyp;
	char	*fndnam;
	struct trenod	*fndval;
};

struct ifnod
{
	int	iftyp;
	struct trenod	*iftre;
	struct trenod	*thtre;
	struct trenod	*eltre;
};

struct whnod
{
	int	whtyp;
	struct trenod	*whtre;
	struct trenod	*dotre;
};

struct fornod
{
	int	fortyp;
	struct trenod	*fortre;
	char	*fornam;
	struct comnod	*forlst;
};

struct swnod
{
	int	swtyp;
	char *swarg;
	struct regnod	*swlst;
};

struct regnod
{
	struct argnod	*regptr;
	struct trenod	*regcom;
	struct regnod	*regnxt;
};

struct parnod
{
	int	partyp;
	struct trenod	*partre;
};

struct lstnod
{
	int	lsttyp;
	struct trenod	*lstlef;
	struct trenod	*lstrit;
};

struct ionod				/* file description, often on a stack */
{
	int	iofile;			/* file descriptor & flag bits */
	char	*ioname;		/* file name */
	char	*iolink;		/* file name of a second link */
	struct ionod	*ionxt;		/* next on the stack toward the top */
	struct ionod	*iolst;		/* next on the stack toward the bottom */
};

struct fdsave
{
	int org_fd;
	int dup_fd;
};


#define		fndptr(x)	((struct fndnod *)(x))
#define		comptr(x)	((struct comnod *)(x))
#define		forkptr(x)	((struct forknod *)(x))
#define		parptr(x)	((struct parnod *)(x))
#define		lstptr(x)	((struct lstnod *)(x))
#define		forptr(x)	((struct fornod *)(x))
#define		whptr(x)	((struct whnod *)(x))
#define		ifptr(x)	((struct ifnod *)(x))
#define		swptr(x)	((struct swnod *)(x))
