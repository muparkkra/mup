#ifndef _ALLOC_DEBUG_
#define _ALLOC_DEBUG_

/* This is used for debugging difficult bugs that corrupt malloc space.
 * Mup can be compiled with -DMUP_ALLOC_DEBUG, to save information
 * in a memory-mapped file, that a post processor can then make human readable,
 * to hopefully give useful clues.
 */

#include <sys/types.h>

/* what kind of information is stored in the current slot */
#define AI_MALLOC 1
#define AI_CALLOC 2
#define AI_REALLOC 3

#define AI_MALLOCA 4
#define AI_CALLOCA 5
#define AI_REALLOCA 6

#define AI_FREE 7
#define AI_FREE_SKIPPED 8

/* The data structure for information about allocs and frees */
#define CSIZE 32
struct ALLOC_INFO {
	short ai_type;		/* AI_* value */
	size_t size;		/* first arg to malloc/calloc */
	size_t num_elem;	/* second arg to calloc */
	void *arg_addr;		/* first arg passed to realloc or free */
	void *ret_value;	/* return value from malloc/calloc/realloc */
	void *call_addr;	/* where malloc/calloc/realloc/free was called from */
	char type[CSIZE];	/* e.g., char, struct GRPSYL, etc; the type in
				 * the Mup allocation macros */
	char newp[CSIZE];	/* name of variable the allocated space will
				 * be assigned to; the new_p in the Mup
				 * allocation macros. */
};

/* Call this at the beginning of memory functions */
extern void alloc_debug(short ai_type, size_t size, size_t num_elem,
				void *arg_addr, char *type, char *newp);
/* Call this at the end of memory allocation functions */
extern void alloc_debug_ret_value(void *ret_value);

/* Call this instead of the real free */
#ifdef __STDC__
extern void free_debug(void*);
#else
extern void free_debug(char*);
#endif

#endif
