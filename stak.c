/*
 * UNIX shell
 *
 * Stacked-storage allocation.
 *
 * This replaces the original V9 shell's stak.c.  This implementation
 * does not rely on memory faults to manage storage.  See ``A Partial
 * Tour Through the UNIX Shell'' for details.  This implementation is
 * newer than the one published in that paper, but differs mainly in
 * just being a little more portable.  In particular, it works on
 * Ultrasparc and Alpha processors, which are insistently 64-bit processors.
 *
 * Maintains a linked stack (of mostly character strings), the top (most
 * recently allocated item) of which is a growing string, which pushstak()
 * inserts into and grows as needed.
 *
 * Each item on the stack consists of a pointer to the previous item
 * (the "word" pointer; stk.topitem points to the top item on the stack),
 * an optional magic number, and the data.  There may be malloc overhead storage
 * on top of this.  Heap storage returned by alloc() lacks the "word" pointer.
 *
 * Pointers returned by these routines point to the first byte of the data
 * in a stack item; users of this module should be unaware of the "word"
 * pointer and the magic number.  To confuse matters, stk.topitem points to the
 * "word" linked-list pointer of the top item on the stack, and the
 * "word" linked-list pointers each point to the corresponding pointer
 * in the next item on the stack.  This all comes to a head in tdystak().
 *
 * Geoff Collyer
 */

/* see also stak.h */
#include "defs.h"
#undef free				/* refer to free(3) here */

#define TPRS(s)		do { if (Tracemem) prs(s); } while (0)
#define TPRN(n)		do { if (Tracemem) prn(n); } while (0)
#define TPRNN(n)	do { if (Tracemem) prnn(n); } while (0)

#define STPRS(s)	do { if (Stackdebug) prs(s); } while (0)
#define STPRN(n)	do { if (Stackdebug) prn(n); } while (0)
#define STPRNN(n)	do { if (Stackdebug) prnn(n); } while (0)

#define ALIGNED(p)	(((uintptr_t)(p) & (sizeof(long) - 1)) == 0)

enum {
	/* unit of allocation. keeps data aligned, perhaps reduces reallocs */
	Quantum	= 2 * BYTESPERWORD,
	Tracemem = 0,
	Stackdebug = 0,

	STMAGICNUM = 0xcafe1235dead1375ULL,	/* stak item magic */
	HPMAGICNUM = 0xbabe1675beef1835ULL,	/* heap item magic */
};

/*
 * to avoid relying on features of the Plan 9 C compiler, these structs
 * are expressed rather awkwardly.
 */
typedef struct stackblk Stackblk;
typedef struct {
	Stackblk *word;			/* pointer to previous stack item */
	unsigned long long magic;	/* force pessimal alignment */
} Stackblkhdr;
struct stackblk {
	Stackblkhdr h;
	char	userdata[1];
};

typedef struct {
	unsigned long long magic;	/* force pessimal alignment */
} Heapblkhdr;
typedef struct {
	Heapblkhdr h;
	char	userdata[1];
} Heapblk;

typedef struct {
	char	*base;
	/*
	 * A chain of ptrs of stack blocks that have become covered
	 * by heap allocation.  `tdystak' will return them to the heap.
	 */
	Stackblk *topitem;
} Stack;

static Stack stk;

/* forwards */
void	prnn(long long);
static char *stalloc(int);
static void debugsav(char *);

static void
tossgrowing(void)				/* free the growing stack */
{
	if (stk.topitem != NIL) {		/* any growing stack? */
		Stackblk *nextitem;

		/* verify magic before freeing */
		if (stk.topitem->h.magic != STMAGICNUM) {
			prs("tossgrowing: stk.topitem->h.magic ");
			prn(stk.topitem->h.magic);
			prs("\n");
			error("tossgrowing: bad magic on stack");
		}
		stk.topitem->h.magic = 0;	/* erase magic */

		/* about to free the ptr to next, so copy it first */
		nextitem = stk.topitem->h.word;

		TPRS("tossgrowing freeing ");
		TPRN((uintptr_t)stk.topitem);
		TPRS("\n");

		free(stk.topitem);
		stk.topitem = nextitem;
		assert(ALIGNED(stk.topitem));
	}
}

static char *
stalloc(int size)		/* allocate requested stack space (no frills) */
{
	Stackblk *nstk;

	TPRS("stalloc allocating ");
	TPRN(sizeof(Stackblkhdr) + size);
	TPRS(" user bytes ");

	size = round(size, Quantum) + sizeof(Stackblkhdr);
	if (size < sizeof(long long))
		size = sizeof(long long);
	nstk = malloc(size);
	if (nstk == NIL)
		error(nostack);
	memset(nstk, 0, size);

	TPRS("@ ");
	TPRN((uintptr_t)nstk);
	TPRS("\n");

	/* stack this item */
	nstk->h.word = stk.topitem;		/* point back @ old stack top */
	stk.topitem = nstk;			/* make this new stack top */

	nstk->h.magic = STMAGICNUM;	/* add magic number for verification */
	assert(ALIGNED(nstk->userdata));
	return nstk->userdata;
}

static void
grostalloc(void)			/* allocate growing stack */
{
	int size = round(BRKINCR, Quantum);

	/* fiddle global variables to point into this (growing) stack */
	staktop = stakbot = stk.base = stalloc(size);
	stakend = stk.base + size - 1;
	assert(ALIGNED(stk.base));
}

/*
 * allocate requested stack.
 * staknam() assumes that getstak just realloc's the growing stack,
 * so we must do just that.  Grump.
 */
char *
getstak(intptr_t asize)
{
	intptr_t staklen, size;
	char *nstk;

	size = round(asize, Quantum);		/* allocate lumps */
	/* +1 is because stakend points at the last byte of the growing stack */
	staklen = stakend + 1 - stk.base;	/* # of usable bytes */

	TPRS("getstak(");
	TPRN(asize);
	TPRS(") calling growstak(");
	TPRNN(size - staklen);
	TPRS("):\n");

	nstk = growstak(size - staklen); /* grow growing stack to requested size */
	grostalloc();				/* allocate new growing stack */
	assert(ALIGNED(nstk));
	return nstk;
}

void
prnn(long long i)			/* print possibly negative vlong */
{
	if (i < 0) {
		prs("-");
		i = -i;
	}
	prn(i);
}

/*
 * set up stack for local use (i.e. make it big).
 * should be followed by `endstak'
 */
char *
locstak(void)
{
	intptr_t stksz = stakend + 1 - stakbot;

	if (stksz < BRKINCR) {
		TPRS("locstak calling growstak(");
		TPRNN(BRKINCR - stksz);
		TPRS("):\n");
		growstak(BRKINCR - stksz);
	}
	assert(ALIGNED(stakbot));
	return stakbot;
}

/*
 * return an address to be used by tdystak later,
 * so it must be returned by getstak because it may not be
 * a part of the growing stack, which is subject to moving.
 */
char *
savstak(void)
{
/*	assert(staktop == stakbot);	/* assert empty stack */
	return getstak(1);
}

/*
 * tidy up after `locstak'.
 * make the current growing stack a semi-permanent item and
 * generate a new tiny growing stack.
 */
char *
endstak(char *argp)
{
	char *ostk;

	*argp++ = 0;				/* terminate the string */
	TPRS("endstak calling growstak(");
	TPRNN(-(stakend + 1 - argp));
	TPRS("):\n");
	ostk = growstak(-(stakend + 1 - argp));	/* reduce growing stack size */
	grostalloc();				/* alloc. new growing stack */
	assert(ALIGNED(ostk));
	return ostk;				/* perm. addr. of old item */
}

/*
 * Try to bring the "stack" back to sav (the address of userdata[0] in some
 * Stackblk, returned by growstak()), and on V9 bring iotemp's stack back to
 * iosav (an old copy of iotemp, which may be NIL).
 */
void
tdystak(char *sav, struct ionod *iosav)
{
	Stackblk *blk = NIL;

	rmtemp(iosav);				/* pop temp files */

	if (sav != NIL) {
		blk = (Stackblk *)(sav - sizeof(Stackblkhdr));
		assert(ALIGNED(blk));
	}
	if (sav == NIL)
		STPRS("tdystak(0)\n");
	else if (blk->h.magic == STMAGICNUM) {
		STPRS("tdystak(data ptr: ");
		STPRN((uintptr_t)sav);
		STPRS(")\n");
	} else {
		STPRS("tdystak(garbage: ");
		STPRN((uintptr_t)sav);
		STPRS(")\n");
		error("tdystak: bad magic in argument");
	}

	/*
	 * pop stack to sav (if zero, pop everything).
	 * stk.topitem points at the ptr before the data & magic.
	 */
	while (stk.topitem != NIL && (sav == NIL || stk.topitem != blk)) {
		debugsav(sav);
		tossgrowing();			/* toss the stack top */
	}
	assert(ALIGNED(stk.topitem));
	debugsav(sav);
	STPRS("tdystak: done popping\n");
	grostalloc();				/* new growing stack */
	STPRS("tdystak: exit\n");
}

static void
debugsav(char *sav)
{
	if (stk.topitem == NIL)
		STPRS("tdystak: stk.topitem == 0\n");
	else if (sav != NIL &&
	    stk.topitem == (Stackblk *)(sav - sizeof(Stackblkhdr))) {
		STPRS("tdystak: stk.topitem == link ptr of arg: ");
		STPRN((uintptr_t)stk.topitem);
		STPRS("\n");
	} else {
		STPRS("tdystak: stk.topitem == link ptr of item above arg: ");
		STPRN((uintptr_t)stk.topitem);
		STPRS("\n");
	}
}

void
stakchk(void)			/* reduce growing-stack size if feasible */
{
	uintptr_t unused;

	unused = stakend - staktop;
	if (unused > 2*BRKINCR) {	/* lots of unused stack headroom */
		TPRS("stakchk calling growstak(");
		TPRNN(-(unused - BRKINCR));
		TPRS("):\n");
		growstak(-(unused - BRKINCR));
	}
}

char *			/* address of copy of newstak */
cpystak(char *newstak)
{
	return strcpy(getstak(strlen(newstak) + 1), newstak);
}

char *			/* new address of 1st usable byte of grown stak */
growstak(int incr)	/* grow the growing stack by incr */
{
	intptr_t staklen, oldsize;
	uintptr_t topoff, botoff, basoff;
	char *oldbsy;

	if (stk.topitem == NIL)			/* paranoia */
		grostalloc();			/* make a trivial stack */
	if (incr == 0)
		return stk.base;

	/* +1 is because stakend points at the last byte of the growing stack */
	oldsize = stakend + 1 - (char *)stk.topitem;
	staklen = round(oldsize + incr, Quantum);
	if (staklen < sizeof(Stackblkhdr))	/* paranoia */
		staklen = sizeof(Stackblkhdr);
	if (round(oldsize, Quantum) == staklen)
		return stk.base;

	/* paranoia: during realloc, point @ previous item in case of signals */
	oldbsy = (char *)stk.topitem;
	assert(ALIGNED(oldbsy));
	stk.topitem = stk.topitem->h.word;
	assert(ALIGNED(stk.topitem));

	topoff = staktop - oldbsy;
	botoff = stakbot - oldbsy;
	basoff = stk.base - oldbsy;

	TPRS("growstak growing ");
	TPRN((uintptr_t)oldbsy);
	TPRS(" from ");
	TPRN(oldsize);
	TPRS(" bytes; ");

	if (incr < 0) {
		/*
		 * V7 realloc wastes the memory given back when
		 * asked to shrink a block, so we malloc new space
		 * and copy into it in the hope of later reusing the old
		 * space, then free the old space.
		 */
		char *new = malloc(staklen);

		if (new == NIL)
			error(nostack);
		memmove(new, oldbsy, staklen);
		free(oldbsy);
		oldbsy = new;
	} else {
		/* get realloc to grow the stack to match the stack top */
		if ((oldbsy = realloc(oldbsy, staklen)) == NIL)
			error(nostack);
		memset(oldbsy + staklen - incr, 0, incr);
	}
	TPRS("now @ ");
	TPRN((uintptr_t)oldbsy);
	TPRS(" of ");
	TPRN(staklen);
	TPRS(" bytes (");
	if (incr < 0) {
		TPRN(oldsize - staklen);
		TPRS(" smaller");
	} else {
		TPRN(staklen - oldsize);
		TPRS(" bigger");
	}
	TPRS(")\n");

	assert(ALIGNED(oldbsy));
	stakend = oldbsy + staklen - 1;	/* see? points at the last byte */
	staktop = oldbsy + topoff;
	stakbot = oldbsy + botoff;
	assert(ALIGNED(stakbot));
	stk.base = oldbsy + basoff;
	assert(ALIGNED(stk.base));

	stk.topitem = (Stackblk *)oldbsy;	/* restore after realloc */
	return stk.base;
}

void
addblok(void)				/* called from main at start only */
{
	assert(BRKINCR >= Quantum);
	if (stakbot == NIL)
		grostalloc();			/* allocate initial arena */
}

/*
 * Heap allocation.
 */
char *
alloc(uintptr_t size)
{
	uintptr_t rsize = round(size, Quantum) + sizeof(Heapblkhdr);
	Heapblk *p = malloc(rsize);

	if (p == NIL)
		error(nospace);
	memset(p, 0, rsize);
	p->h.magic = HPMAGICNUM;

	TPRS("alloc allocated ");
	TPRN(rsize);
	TPRS(" user bytes @ ");
	TPRN((uintptr_t)p->userdata);
	TPRS("\n");
	assert(ALIGNED(p->userdata));
	return p->userdata;
}

/*
 * the shell's private "free" - frees only heap storage.
 * only works on non-null pointers to heap storage
 * (below the data break and stamped with HPMAGICNUM).
 * so it is "okay" for the shell to attempt to free data on its
 * (real) stack, including its command line arguments and environment,
 * or its fake stak.
 * this permits a quick'n'dirty style of programming to "work".
 * the use of sbrk is relatively slow, but effective.
 *
 * the return type of sbrk is apparently changing from char* to void*.
 */
void
shfree(void *ap)
{
	char *p = ap;

	if (p != NIL && ALIGNED(p) && p < (char *)sbrk(0)) {
		/* plausible data seg ptr */
		Heapblk *blk = (Heapblk *)(p - sizeof(Heapblkhdr));

		TPRS("shfree freeing user data @ ");
		TPRN((uintptr_t)p);
		TPRS("\n");
		/* ignore attempts to free non-heap storage */
		if (blk->h.magic == HPMAGICNUM) {
			blk->h.magic = 0;		/* erase magic */
			free(blk);
		}
	}
}
