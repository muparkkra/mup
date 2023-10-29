/*
 Copyright (c) 1995-2023  by Arkkra Enterprises.
 All rights reserved.

 Redistribution and use in source and binary forms,
 with or without modification, are permitted provided that
 the following conditions are met:

 1. Redistributions of source code must retain
 the above copyright notice, this list of conditions
 and the following DISCLAIMER.

 2. Redistributions in binary form must reproduce the above
 copyright notice, this list of conditions and
 the following DISCLAIMER in the documentation and/or
 other materials provided with the distribution.

 3. Any additions, deletions, or changes to the original files
 must be clearly indicated in accompanying documentation,
 including the reasons for the changes,
 and the names of those who made the modifications.

	DISCLAIMER

 THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS
 OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*
 *	rational.h	definitions for using rational numbers
 */
#ifndef _RATIONAL
#define _RATIONAL


/*
 * The code in rational.h/rational.c is written in a way that depends on
 * certain variables being 32-bit numbers stored in two's complement form.  For
 * most compilers, "long" works for this, but for some compilers, "long" is
 * 64 bits, or maybe something else.  C standards after (and not including) the
 * 1994 version guarantee the existence of a header file stdint.h which defines
 * typedefs int32_t and uint32_t.  So for those standards we will use those
 * typedefs from that header when defining our INT32B and UINT32B.  Otherwise
 * we check __LP64__ to try to determine the right types to use.
 */
#if __STDC_VERSION__ > 199409L
#include <stdint.h>
#define	INT32B	int32_t
#define	UINT32B	uint32_t
#elif __LP64__
#define INT32B int
#define UINT32B unsigned int
#else
#define	INT32B	long
#define	UINT32B	unsigned long
#endif
/*
 * We verify that the resulting types are 32 bits by declaring the array
 * Fix_rational.  If INT32B is not size 4, Fix_rational will have size -1
 * and the compilation will fail.  In that case you must provide some other
 * type to be 32 bits.  Also note, the types must be in two's complement form.
 */
extern int Fix_rational[ 1 - 2 * (sizeof (INT32B) != 4) ];

/*
 * Define the structure that represents a rational number.  The number
 * should be kept in standard form; that is, in lowest terms with a positive
 * denominator.  Zero should be stored as 0/1.
 */
struct rational {
	INT32B n;	/* numerator */
	INT32B d;	/* denominator */
};

typedef struct rational RATIONAL;


/* macros for comparing rational numbers */
/* numbers must be in standard form */

#define EQ(a, b)	((a).n == (b).n && (a).d == (b).d)
#define NE(a, b)	((a).n != (b).n || (a).d != (b).d)

/* must call function for inequalities, because overflow possibilities */

#define GT(a, b)	gtrat(a, b)
#define LT(a, b)	gtrat(b, a)
#define GE(a, b)	( ! gtrat(b, a) )
#define LE(a, b)	( ! gtrat(a, b) )


/* macros for testing a rational number */
/* numbers must be in standard form, or at least have positive denominator */

#define ZE(a)		((a).n == 0)
#define PL(a)		((a).n >  0)
#define MI(a)		((a).n <  0)
#define NZ(a)		((a).n != 0)
#define NM(a)		((a).n >= 0)
#define NP(a)		((a).n <= 0)


/* definitions for error handling */

#define	RATNOERR	0
#define RATOVER		1
#define RATDIV0		2
#define RATPARM		3

#ifndef MAXLONG
#define MAXLONG		0x7fffffff
#endif

extern int raterrno;			/* holds error code */
#ifdef __STDC__
extern void (*raterrfuncp)(int);	/* pointer to user error handler */
#else
extern void (*raterrfuncp)();		/* pointer to user error handler */
#endif


/* functions to do operations on rational numbers */

#ifdef __STDC__
extern RATIONAL radd(RATIONAL x, RATIONAL y);
extern RATIONAL rsub(RATIONAL x, RATIONAL y);
extern RATIONAL rmul(RATIONAL x, RATIONAL y);
extern RATIONAL rdiv(RATIONAL x, RATIONAL y);
extern RATIONAL rneg(RATIONAL x);
extern RATIONAL rinv(RATIONAL x);
extern RATIONAL rrai(RATIONAL x, int n);
extern void rred(RATIONAL *ap);
extern char *ator(RATIONAL *rp, char s[]);
extern char *rtoa(char s[], RATIONAL *rp);
extern int gtrat(RATIONAL x, RATIONAL y);
#else
extern RATIONAL radd(), rsub(), rmul(), rdiv(), rneg(), rinv(), rrai();
extern void rred();
extern char *ator(), *rtoa();
extern int gtrat();
#endif

#endif
