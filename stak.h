/*
 *	UNIX shell
 *
 *	Bell Telephone Laboratories
 */

/*
 * To use stack as temporary workspace across
 * possible storage allocation (eg name lookup)
 * a) get ptr from `relstak'
 * b) can now use `pushstak'
 * c) then reset with `setstak'
 * d) `absstak' gives real address if needed
 */

/*
 * The original implementation of stak.c worked like this:
 * ---
 * pushstak can generate memory faults, as can alloc().
 *
 * The stack (stakbot to staktop) can be moved if alloc()
 * needs more space and extended or moved if pushstak runs off the end.
 * The "stack" in region from stakbas to stakbot is actually immovable,
 * permanent storage that should really be allocated by malloc and
 * linked with stk.topitem (was stakbsy).
 * ---
 */

/*
 * amount to grow the "stak" stack each time.  must be >= Quantum.
 */
#define BRKINCR 256

#define relstak()	(staktop-stakbot)	/* offset into stack */

/* note the possibility of zerostak being used after this. */
#define pushstak(c) (voidstring = staktop >= stakend? growstak(BRKINCR): 0, \
		*staktop++ = (c))
#define zerostak()	(*staktop = 0)		/* zero top byte */

#define setstak(x)	(staktop = absstak(x))	/* pop back to start */
#define absstak(x)	(stakbot + (uintptr_t)(x)) /* address from offset */

/* Used to address an item left on the top of
 * the stack (very temporary)
 */
#define curstak()	staktop

/* `usestak' before `pushstak' then `fixstak'
 * These routines are safe against heap
 * being allocated.
 */
#define usestak()	locstak()

/* Will allocate the item being used and return its
 * address (safe now).
 */
#define fixstak()	endstak(staktop)

extern char *growstak(int);

/* for local use only since it hands
 * out a real address for the stack top
 */
extern char *locstak(void);

/* For use after `locstak' to hand back
 * new stack top and then allocate item
 */
extern char *endstak(char *);

/* Copy a string onto the stack and
 * allocate the space.
 */
extern char *cpystak(char *);

/* Allocate given amount of stack space */
extern char *getstak(intptr_t);

/* Used with tdystak */
extern char *savstak(void);

void	addblok(void);
void	stakchk(void);
void	tdystak(char *sav, struct ionod *iosav);

char *staktop;			/* Top of current item */
char *stakbot;			/* Base of current item */
char *stakend;			/* Top of entire stack */
