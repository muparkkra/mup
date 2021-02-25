/*
 Copyright (c) 1995-2021  by Arkkra Enterprises.
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
 *	rational.c	functions to do operations on rational numbers
 *
 *	Contents:
 *
 *		radd(), rsub(), rmul(), rdiv(), rneg(), rinv(), rrai(), rred(),
 *		ator(), rtoa(); also gtrat(), called by macros GT,GE,LT,LE
 *
 *		ratmsg(), add64_64(), mul32_64(), divmod64(), red64_64()
 *
 *		The first group of functions are for the user.	The second
 *		are for internal use only.
 *
 *	Description:
 *		The functions in this file do operations on rational numbers.
 *		The rational arguments to functions that the users can call
 *		must be in standard form (lowest terms, with positive
 *		denominator) except for rred().  There are checks for division
 *		by zero and for overflow of numerators and denominators.
 *		(The absolute values of each are limited to MAXLONG, defined
 *		in rational.h.)  If there is an error, the external int
 *		raterrno is set to RATDIV0 or RATOVER, as the case may be,
 *		and *raterrfuncp is checked.  If nonzero, it is assumed to
 *		point at the user's error handler, and it is called with a
 *		parameter equal to raterrno.  Otherwise, a message is printed
 *		to stderr.  In any case, the answer returned to the user is
 *		0/1.  If there was no error, raterrno is set to RATNOERR.
 *
 *		In general, the functions assume they are being called with
 *		valid parameters.  If they are not, results are not guaranteed
 *		to be correct.  However, they are defensive enough so that
 *		invalid parameters will not cause a crash in these routines.
 *		They will not always detect invalid parameters, but if they
 *		do, they will use the raterrno/raterrfuncp mechanism described
 *		above, with the value RATPARM.
 *
 *		These routines depend on a INT32B being a 32-bit number,
 *		stored in two's complement form, and UINT32B being the same
 *		for unsigned.  See rational.h.  Numerators and denominators
 *		are assumed to be INT32B.  Furthermore, the number 0x80000000
 *		is not allowed.  The routines should work on any machine and
 *		compiler where these requirements are met.
 *
 *		Internally, when 64-bit numbers are used, they are represented
 *		by an array of two INT32B.  The 0 subscript contains the low
 *		order bits and the 1 subscript contains the high order bits.
 *		The numbers are usually used as two's complement signed
 *		integers, so the high bit of the 1 subscript is a sign bit.
 */

#ifndef stderr
#	include <stdio.h>
#endif

#ifndef isspace
#	include <ctype.h>
#endif

#ifndef _RATIONAL
#	include "rational.h"
#endif



/*
 * Define SMALL to be a number that uses less than half as many bits as
 * MAXLONG (15 as compared to 31).  Define a SMALLRAT as a rational number
 * whose numerator (absolute value) and denominator are in that range.
 * The denominator is assumed to be positive.
 */
#define SMALL		0x7fff
#define SMALLRAT(q)	((q).n <= SMALL && (q).n >= -SMALL && (q).d <= SMALL)


/*
 * This macro checks whether a 64-bit integer is actually less than or
 * equal to MAXLONG in absolute value, 0x7fffffff.
 */
#define INT32(n)	(((n)[1] ==  0 && (n)[0] >= 0) ||		\
			 ((n)[1] == -1 && (n)[0] < 0 && (n)[0] != 0x80000000))


/*
 * Return whether the first 64-bit number equals the second.
 * To be equal, both words must be equal.
 */
#define EQ64(x, y)	( (x)[1] == (y)[1] && (x)[0] == (y)[0] )


/*
 * Return whether the first 64-bit number is greater than the second.
 * If the high order words are equal, use the low order words, unsigned;
 * otherwise, just use the high order words.
 */
#define GT64(x, y) (						\
	(x)[1] == (y)[1] ?					\
		(UINT32B)(x)[0] > (UINT32B)(y)[0]		\
	:							\
		(x)[1] > (y)[1]					\
)


/*
 * Return whether the first 64-bit number is less than the second.
 * If the high order words are equal, use the low order words, unsigned;
 * otherwise, just use the high order words.
 */
#define LT64(x, y) (						\
	(x)[1] == (y)[1] ?					\
		(UINT32B)(x)[0] < (UINT32B)(y)[0]		\
	:							\
		(x)[1] < (y)[1]					\
)


/*
 * Return whether the first *unsigned* 64-bit number is less than or equal to
 * the second.  If the high order words are equal, use the low order words;
 * otherwise, just use the high order words.
 */
#define LEU64(x, y) (						\
	(x)[1] == (y)[1] ?					\
		(UINT32B)(x)[0] <= (UINT32B)(y)[0]		\
	:							\
		(UINT32B)(x)[1] <= (UINT32B)(y)[1]		\
)


/*
 * Negate a 64-bit number.
 */
#define NEG64(x)	{						\
	(x)[1] = ~(x)[1];	/* one's complement */			\
	(x)[0] = -(x)[0];	/* two's complement */			\
	if ((x)[0] == 0)	/* if "carry" must inc high word */	\
		(x)[1]++;						\
}


/*
 * Shift a 64-bit number left one bit as unsigned (not that it matters).
 */
#define SHL1U64(x)	{						\
	(x)[1] <<= 1;		/* shift high word */			\
	if ((x)[0] < 0)		/* if high bit of low word is set */	\
		(x)[1]++;	/* shift it into the high word */	\
	(x)[0] <<= 1;		/* shift low word */			\
}


/*
 * Shift a 64-bit number right one bit as unsigned.
 */
#define SHR1U64(x)	{						\
	(x)[0] = (UINT32B)(x)[0] >> 1;	/* shift low word */		\
	if ((x)[1] & 1)			/* if low bit of high word set*/\
		(x)[0] |= 0x80000000;	/* shift it into low word */	\
	(x)[1] = (UINT32B)(x)[1] >> 1;	/* shift low word */		\
}



/* declare as static the functions that are only used internally */
#ifdef __STDC__
static void ratmsg(int code);
static void add64_64(INT32B a[], INT32B x[], INT32B y[]);
static void mul32_64(INT32B a[], INT32B x, INT32B y);
static void divmod64(INT32B x[], INT32B y[], INT32B q[], INT32B r[]);
static void red64_64(INT32B num[], INT32B den[]);
#else
static void ratmsg(), add64_64(), mul32_64(), divmod64(), red64_64();
#endif


int raterrno;			/* set to error type upon return to user */
void (*raterrfuncp)();		/* error handler functions to be called */

static RATIONAL zero = {0,1};

/*
 *	radd()		add two rational numbers
 *
 *	This function adds two rational numbers.  They must be in standard
 *	form.
 *
 *	Parameters:	x	the first number
 *			y	the second number
 *
 *	Return value:	The sum (x + y), if it can be represented as a
 *			RATIONAL, else 0/1.
 *
 *	Side effects:	If radd() succeeds, it sets raterrno to RATNOERR.
 *			Otherwise, the numerator or denominator must have
 *			overflowed, so it sets raterrno to RATOVER and
 *			either prints a message or calls a user-supplied
 *			error handler.
 */

RATIONAL
radd(x, y)

RATIONAL x, y;

{
	RATIONAL a;			/* the answer */
	INT32B bign[2];			/* 64-bit numerator */
	INT32B bigd[2];			/* 64-bit denominator */
	INT32B bigt[2];			/* temp storage */


	raterrno = RATNOERR;		/* no error yet */

	/*
	 * If the numbers are small enough, do it the easy way, since there is
	 * then no danger of overflow.
	 */
	if (SMALLRAT(x) && SMALLRAT(y)) {
		a.n = x.n * y.d + x.d * y.n;
		a.d = x.d * y.d;
		rred(&a);		/* reduce to standard form */
		return(a);
	}

	/*
	 * To avoid overflow during the calculations, use two INT32B to
	 * hold numbers.
	 */
	mul32_64(bign, x.n, y.d);	/* get first part of numerator */
	mul32_64(bigt, x.d, y.n);	/* get second part of numerator */
	add64_64(bign, bign, bigt);	/* add to get full numerator */
	mul32_64(bigd, x.d, y.d);	/* get denominator */
	red64_64(bign, bigd);		/* reduce */

	/* overflow if the result can't fit in a RATIONAL */
	if ( ! INT32(bign) || ! INT32(bigd) ) {
		ratmsg(RATOVER);	/* set raterrno, report error */
		return(zero);
	}

	a.n = bign[0];			/* set answer */
	a.d = bigd[0];

	return(a);
}

/*
 *	rsub()		subtract two rational numbers
 *
 *	This function subtracts two rational numbers.  They must be in standard
 *	form.
 *
 *	Parameters:	x	the first number
 *			y	the second number
 *
 *	Return value:	The difference (x - y), if it can be represented as a
 *			RATIONAL, else 0/1.
 *
 *	Side effects:	If rsub() succeeds, it sets raterrno to RATNOERR.
 *			Otherwise, the numerator or denominator must have
 *			overflowed, so it sets raterrno to RATOVER and
 *			either prints a message or calls a user-supplied
 *			error handler.
 */

RATIONAL
rsub(x, y)

RATIONAL x, y;

{
	/*
	 * Just negate the second operand and add.  We could call rneg() to
	 * negate y, but why waste the time?
	 */
	y.n = -y.n;
	return(radd(x, y));
}

/*
 *	rmul()		multiply two rational numbers
 *
 *	This function multiplies two rational numbers.  They must be in standard
 *	form.
 *
 *	Parameters:	x	the first number
 *			y	the second number
 *
 *	Return value:	The product (x * y), if it can be represented as a
 *			RATIONAL, else 0/1.
 *
 *	Side effects:	If rsub() succeeds, it sets raterrno to RATNOERR.
 *			Otherwise, the numerator or denominator must have
 *			overflowed, so it sets raterrno to RATOVER and
 *			either prints a message or calls a user-supplied
 *			error handler.
 */

RATIONAL
rmul(x, y)

RATIONAL x, y;

{
	RATIONAL a;			/* the answer */
	INT32B bign[2];			/* 64-bit numerator */
	INT32B bigd[2];			/* 64-bit denominator */


	raterrno = RATNOERR;		/* no error yet */

	/*
	 * If the numbers are small enough, do it the easy way, since there is
	 * then no danger of overflow.
	 */
	if (SMALLRAT(x) && SMALLRAT(y)) {
		a.n = x.n * y.n;
		a.d = x.d * y.d;
		rred(&a);		/* reduce to standard form */
		return(a);
	}

	/*
	 * To avoid overflow during the calculations, use two INT32B to
	 * hold numbers.
	 */
	mul32_64(bign, x.n, y.n);	/* get numerator */
	mul32_64(bigd, x.d, y.d);	/* get denominator */
	red64_64(bign, bigd);		/* reduce */

	/* overflow if the result can't fit in a RATIONAL */
	if ( ! INT32(bign) || ! INT32(bigd) ) {
		ratmsg(RATOVER);	/* set raterrno, report error */
		return(zero);
	}

	a.n = bign[0];			/* set answer */
	a.d = bigd[0];

	return(a);
}

/*
 *	rdiv()		divide two rational numbers
 *
 *	This function divides two rational numbers.  They must be in standard
 *	form.
 *
 *	Parameters:	x	the first number
 *			y	the second number
 *
 *	Return value:	The quotient (x / y), if it is defined and can be
 *			represented as a RATIONAL, else 0/1.
 *
 *	Side effects:	If rdiv() succeeds, it sets raterrno to RATNOERR.
 *			Otherwise, either the second number was zero or the
 *			numerator or denominator overflowed.  In this case,
 *			it sets raterrno to RATDIV0 or RATOVER, respectively,
 *			and either prints a message or calls a user-supplied
 *			error handler.
 */

RATIONAL
rdiv(x, y)

RATIONAL x, y;

{
	RATIONAL r;			/* reciprocal of y */


	r = rinv(y);			/* first find 1/y */

	if (raterrno != RATNOERR)	/* if y was 0, return failure now */
		return(zero);

	/*
	 * Return  x * r.   Whether rmul() succeeds or fails, we still just want
	 * to leave raterrno the same and return what rmul() returns.
	 */
	return(rmul(x, r));
}

/*
 *	rneg()		negate a rational number
 *
 *	This function negates a rational number.  It must be in standard form.
 *
 *	Parameters:	x	the number
 *
 *	Return value:	The negative (-x).
 *
 *	Side effects:	It sets raterrno to RATNOERR.
 */

RATIONAL
rneg(x)

RATIONAL x;

{
	raterrno = RATNOERR;		/* no errors are possible */

	x.n = -x.n;

	/* answer is already in standard form since x was */
	return(x);
}

/*
 *	rinv()		invert a rational number
 *
 *	This function inverts a rational number.  It must be in standard form.
 *
 *	Parameters:	x	the number
 *
 *	Return value:	The reciprocal (1 / x), if it is defined, else 0/1.
 *
 *	Side effects:	If rinv() succeeds, it sets raterrno to RATNOERR.
 *			Otherwise, the second number must have been zero,
 *			so it sets raterrno to RATDIV0 and either prints a
 *			message or calls a user-supplied error handler.
 */

RATIONAL
rinv(x)

RATIONAL x;

{
	RATIONAL a;			/* the answer */


	/* check for division by 0 */
	if (ZE(x)) {
		ratmsg(RATDIV0);	/* set raterrno, report error */
		return(zero);
	}

	raterrno = RATNOERR;		/* no errors from here on */

	a.n = x.d;			/* flip numerator and denominator */
	a.d = x.n;

	if (a.d < 0) {			/* if x was negative, reverse signs */
		a.n = -a.n;
		a.d = -a.d;
	}

	return(a);
}

/*
 *	rrai()		raise a rational number to an integral power
 *
 *	This function raises a rational number to an integral power.  The
 *	rational number must be in standard form.
 *
 *	Parameters:	x	the rational number
 *			n	the power, an integer
 *
 *	Return value:	The result (x to the nth power), if it is defined and
 *			can be represented as a RATIONAL, else 0/1.
 *
 *	Side effects:	If rrai() succeeds, it sets raterrno to RATNOERR.
 *			Otherwise, either zero is being raised to a non-
 *			positive power, or the numerator or denominator
 *			overflowed.  In this case, it sets raterrno to
 *			RATDIV0 or RATOVER, respectively, and either prints
 *			a message or calls a user-supplied error handler.
 */

RATIONAL
rrai(x, n)

RATIONAL x;
register int n;

{
	static RATIONAL one = {1,1};

	RATIONAL a;			/* the answer */
	register int i;			/* loop counter */


	/* it is undefined to raise zero to a nonpositive power */
	if (ZE(x) && n <= 0) {
		ratmsg(RATDIV0);	/* set raterrno, report error */
		return(zero);
	}

	raterrno = RATNOERR;		/* no error yet */

	a = one;			/* init to 1 */
	if (n >= 0) {
		for (i = 0; i < n; i++) {
			a = rmul(a, x);		/* mul again by x */
			if (raterrno != RATNOERR)
				return(zero);
		}
	} else {
		for (i = 0; i > n; i--) {
			a = rdiv(a, x);		/* div again by x */
			if (raterrno != RATNOERR)
				return(zero);
		}
	}

	return(a);
}

/*
 *	rred()		reduce a rational number to standard form
 *
 *	This function puts a rational number into standard form; that is,
 *	numerator and denominator will be relatively prime and the denominator
 *	will be positive.  On input, they may be any integers whose absolute
 *	values do not exceed MAXLONG.
 *
 *	Parameters:	ap	pointer to the rational number
 *
 *	Return value:	None.
 *
 *	Side effects:	If ap->d is 0, the function sets raterrno to RATDIV0,
 *			either prints a message or calls a user-supplied
 *			error handler, and sets *ap to 0/1.  Otherwise, it
 *			sets raterrno to RATNOERR and puts *ap in standard form.
 */

void
rred(ap)

register RATIONAL *ap;

{
	register INT32B b, c, r; /* temp variables for Euclidean algorithm */
	register int sign;	/* answer is pos (1) or neg (-1) */


	/*
	 * Since the numerator and denominator can be anything <= MAXLONG,
	 * we must guard against division by 0.
	 */
	if (ap->d == 0) {
		ratmsg(RATDIV0);	/* set raterrno, report error */
		*ap = zero;
		return;
	}

	raterrno = RATNOERR;		/* no errors possible from here on */

	if (ap->n == 0) {	 	/* if so, answer is "0/1" */
		ap->d = 1;
		return;
	}

	/* now figure out sign of answer, and make n & d positive */
	sign = 1;			/* init to positive */
	if (ap->n < 0) {		/* reverse if numerator neg */
		sign = -sign;
		ap->n = -(ap->n);
	}
	if (ap->d < 0) {		/* reverse if denominator neg */
		sign = -sign;
		ap->d = -(ap->d);
	}

	/* now check whether numerator or denominator are equal */
	if (ap->n == ap->d) {		/* if so, answer is +1 or -1 */
		ap->n = sign;
		ap->d = 1;
		return;
	}

	if (ap->n < ap->d) {		/* set so that c > b */
		c = ap->d;
		b = ap->n;
	} else {
		c = ap->n;
		b = ap->d;
	}

	/* use Euclidean Algorithm to find greatest common divisor of c & b */
	do {
		r = c % b;
		c = b;
		b = r;
	} while (r != 0);

	/* now c is the greatest common divisor */

	ap->n /= c;		/* divide out greatest common divisor */
	ap->d /= c;		/* divide out greatest common divisor */

	if (sign < 0)		/* put sign in if result should be negative */
		ap->n = -(ap->n);

	return;
}

/*
 *	ator()		convert an ascii string to a rational number
 *
 *	This function takes an ascii string as input and interprets it as
 *	a rational number.  White space may precede the number, but the
 *	number may not contain white space.  The numerator may be preceded
 *	by a minus sign.  The denomintor is optional, but if present, must
 *	not contain a sign.  In short, the number must match one of the
 *	following lex regular expressions, which starts where s points and
 *	ends before the first character not matching the pattern:
 *		[ \t\n]*-?[0-9]+
 *		[ \t\n]*-?[0-9]+\/[0-9]+
 *	Further restrictions are that the absolute values of numerator and
 *	denominator cannot exceed MAXLONG, and the denominator cannot be 0.
 *	If neither pattern is matched, or the further restrictions are
 *	violated, the function sets *rp to 0/1 and returns NULL.  Otherwise,
 *	it sets *rp to the result in standard form, and returns a pointer to
 *	the first char after the number found.
 *
 *	Parameters:	rp	pointer to where the answer goes
 *			s	string containing ascii rational number
 *
 *	Return value:	If a valid rational number is found, the function
 *			returns a pointer to the next char in the string
 *			following the number.  Otherwise it returns NULL.
 *
 *	Side effects:	If ator() succeeds, it sets *rp to the result.
 *			Otherwise, it sets it to 0/1.
 */

char *
ator(rp, s)

register RATIONAL *rp;
register char s[];

{
	register char *p;	/* point somewhere in s[] */
	int sign;		/* 1 means positive, -1 negative */


	/* skip by white space */
	for (p = s; isspace(*p); p++)
		;

	/* init sign to positive; then reverse it if a dash is found */
	sign = 1;
	if (p[0] == '-') {
		sign = -1;
		p++;
	}

	/* fail if there are no digits */
	if ( ! isdigit(*p) ) {
		*rp = zero;
		return(NULL);
	}

	/*
	 * Collect the numerator digits, and defend against overflow.
	 */
	rp->n = 0;
	while ( isdigit(*p) ) {
		if (rp->n > MAXLONG / 10) {
			*rp = zero;
			return(NULL);
		}
		rp->n *= 10;
		if (rp->n > MAXLONG - (*p - '0')) {
			*rp = zero;
			return(NULL);
		}
		rp->n += *p++ - '0';
	}

	if (sign < 0)			/* make negative if necessary */
		rp->n = -(rp->n);


	/*
	 * If there is to be a denominator, collect its digits.  Otherwise,
	 * set it to 1.  Defend against overflow.
	 */
	if (*p == '/') {
		p++;
		if ( ! isdigit(*p) ) {	/* must be digit (no '-' allowed) */
			*rp = zero;
			return(NULL);
		}
		rp->d = 0;
		while ( isdigit(*p) ) {
			if (rp->d > MAXLONG / 10) {
				*rp = zero;
				return(NULL);
			}
			rp->d *= 10;
			if (rp->d > MAXLONG - (*p - '0')) {
				*rp = zero;
				return(NULL);
			}
			rp->d += *p++ - '0';
		}
		if (rp->d == 0)	{	/* zero denominator is a failure */
			*rp = zero;
			return(NULL);
		}
	} else {
		rp->d = 1;		/* no denominator; assume 1 */
	}

	rred(rp);			/* reduce the fraction */

	return(p);			/* first char after the number */
}

/*
 *	rtoa()		convert a rational number to an ascii string
 *
 *	This function takes a rational number as input converts it into
 *	an ascii string.  If the denominator is 1, it will not be printed.
 *	The number must be in standard form.
 *
 *	Parameters:	s	pointer to where the answer goes
 *			rp	pointer to the rational number
 *
 *	Return value:	The function returns a pointer to the next char in
 *			the string following the number.
 *
 *	Side effects:	The function sets s[] to the result.
 */

char *
rtoa(s, rp)

register char s[];
RATIONAL *rp;

{
	register INT32B num, den;	/* copy of num and den from *rp */
	register int i; 		/* index into t[] */
	char t[12];			/* temp answer string */


	num = rp->n;			/* copy num and den for efficiency */
	den = rp->d;

	if (num < 0) {			/* if num is negative */
		*s++ = '-';		/* output minus sign */
		num = -num;		/* and make num positive */
	}

	i = 0;
	do {				/* calc digits in reverse order */
		t[i++] = num % 10 + '0';
		num /= 10;
	} while (num > 0);		/* always loop at least once so 0="0"*/

	while (--i >= 0)		/* copy digits to answer string */
		*s++ = t[i];

	if (den != 1) { 		/* if a denominator is needed */
		*s++ = '/';		/* fraction bar */
		i = 0;
		do {			/* calc digits in reverse order */
			t[i++] = den % 10 + '0';
			den /= 10;
		} while (den > 0);

		while (--i >= 0)	/* copy digits to answer string */
			*s++ = t[i];
	}

	return(s);
}

/*
 *	gtrat()		decide whether one rational is greater than another
 *
 *	This function decides whether its first parameter is greater than
 *	its second.  It is used by the macros GT, GE, LT, and LE.
 *	The numbers must be in standard form.  (Actually, all that is
 *	matters is that the denominators be positive.)
 *
 *	Parameters:	x	the first rational
 *			y	the second rational
 *
 *	Return value:	1 if x > y, otherwise 0
 *
 *	Side effects:	none
 */

int
gtrat(x, y)

RATIONAL x, y;

{
	INT32B a[2];		/* temp holding areas for 64-bit numbers */
	INT32B b[2];


	/* if no overflow possible, cross-multiply and return truth value */
	/* note:  this depends on positive denominators */
	if (SMALLRAT(x) && SMALLRAT(y))
		return(x.n * y.d > x.d * y.n);

	/*
	 * The numbers are too big; we have to do it the hard way to avoid
	 * overflow.  Cross-multiply.  Note: this depends on positive
	 * denominators.
	 */
	mul32_64(a, x.n, y.d);
	mul32_64(b, x.d, y.n);

	return(GT64(a, b));
}

/*
 *	ratmsg()	handle rational error of type "code"
 *
 *	This function sets raterrno.  Then calls the user's error handler,
 *	if there is one, or else prints a message to standard error.
 *
 *	Parameters:	code	the error code
 *
 *	Return value:	None.
 *
 *	Side effects:	raterrno is set; then either a message is printed
 *			to standard error or the user's error handler is
 *			called.
 */

static void
ratmsg(code)

int code;

{
	raterrno = code;		/* set global error flag */

	if (raterrfuncp == 0) {
		/* no user trap exists, so print message from here */
		switch (code) {
		case RATOVER:
			(void)fputs("rational overflow\n", stderr);
			break;

		case RATDIV0:
			(void)fputs("rational division by zero\n", stderr);
			break;

		case RATPARM:
			(void)fputs("invalid number passed to rational number routine\n", stderr);
			break;

		default:
			(void)fputs("error in rational routines\n", stderr);
			break;
		}
	} else {
		/* call user trap function to handle the error */
		(*raterrfuncp)(code);
	}
}

/*
 *	add64_64()	add 64-bit numbers to get a 64-bit number
 *
 *	This function adds two 64-bit signed numbers to get a 64-bit
 *	signed number.  It is assumed that the result will not overflow.
 *	Any of the inputs may be the same arrays.
 *
 *	Parameters:	a	answer goes here
 *			x	the first input
 *			y	the second input
 *
 *	Return value:	none
 *
 *	Side effects:	a is set to the result.
 */

static void
add64_64(a, x, y)

INT32B a[2];
INT32B x[2];
INT32B y[2];

{
	INT32B t[2];			/* temp storage */


	/* first add low and high parts separately */
	/* use temp storage in case a[] is the same array as x[] or y[] */
	t[0] = x[0] + y[0];
	t[1] = x[1] + y[1];

	/* figure out if the low part carries into the high part */
	if (x[0] < 0 && y[0] < 0) {		/* both high order bits set */
		t[1]++;				/* must be a carry */
	} else if (x[0] < 0 || y[0] < 0) {	/* exactly one high bit set */
		if (t[0] >= 0)			/* if result high bit clear */
			t[1]++;			/* must be a carry */
	}

	a[0] = t[0];			/* copy results */
	a[1] = t[1];
}

/*
 *	mul32_64()	multiply 32-bit numbers to get a 64-bit number
 *
 *	This function multiplies two 32-bit signed numbers to get a 64-bit
 *	signed number.  The numbers must not equal 0x80000000.  Overflow
 *	cannot occur.
 *
 *	Parameters:	a	answer goes here
 *			x	the first 32-bit number
 *			y	the second 32-bit number
 *
 *	Return value:	none
 *
 *	Side effects:	a is set to the result.
 */

static void
mul32_64(a, x, y)

INT32B a[2];
INT32B x;
INT32B y;

{
	INT32B t[2];			/* temp storage for inner terms */
	INT32B xl, xh;			/* low and high 16 bits of x */
	INT32B yl, yh;			/* low and high 16 bits of y */
	int sign;			/* sign of the result */


	/* make both numbers positive and determine the sign of the result */
	sign = 1;				/* start at positive */
	if (x < 0) {
		x = -x;
		sign = -sign;
	}
	if (y < 0) {
		y = -y;
		sign = -sign;
	}

	/* break x and y into high and low pieces */
	xl = x & 0xffff;		/* 0 <= xl <= 0xffff */
	xh = x >> 16;			/* 0 <= xh <= 0x7fff */
	yl = y & 0xffff;		/* 0 <= yl <= 0xffff */
	yh = y >> 16;			/* 0 <= yh <= 0x7fff */

	/* multiply the outer parts */
	a[0] = xl * yl;			/* 0 <= a[0] <= 0xfffe0001 */
	a[1] = xh * yh;			/* 0 <= a[1] <= 0x3fff0001 */

	/* multiply the inner parts and break the result in two pieces */
	t[0] = xl * yh + xh * yl;	/* 0 <= t[0] <= 0xfffd0002 */
	t[1] = (UINT32B)t[0] >> 16;		/* 0 <= t[1] <= 0x0000fffd */
	t[0] <<= 16;				/* 0 <= t[0] <= 0xffff0000 */

	/* add the two partial products */
	add64_64(a, a, t);		/* 0 <= a <= 0x3fffffff00000001 */

	/* if the answer is supposed to be negative, negate it */
	if (sign < 0)
		NEG64(a);
}

/*
 *	divmod64()	find quotient and remainder of two 64-bit numbers
 *
 *	This function takes two 64-bit numbers and divides the first by the
 *	second, to get a quotient and remainder, both 64 bits.  It is assumed
 *	that the first number is nonnegative and the second number is positive.
 *	q and r must be different arrays.
 *
 *	Parameters:	x	first number (dividend)
 *			y	second number (divisor)
 *			q	quotient
 *			r	remainder
 *
 *	Return value:	none
 *
 *	Side effects:	q and r are altered to be the results
 *			x and y are not altered
 */

static void
divmod64(x, y, q, r)

INT32B x[2];
INT32B y[2];
INT32B q[2];
INT32B r[2];

{
	INT32B s[2];		/* temp storage for divisor */
	INT32B t[2];		/* temp storage for scratch */
	register int shift;	/* how far has y been shifted? */


	r[0] = x[0];		/* copy dividend to remainder place */
	r[1] = x[1];
	s[0] = y[0];		/* copy divisor to temp storage */
	s[1] = y[1];

	/* shift divisor left until greater than dividend */
	/* compare as unsigned so no problem if it gets shifted into sign bit */
	for (shift = 0; LEU64(s, r); shift++)
		SHL1U64(s);

	SHR1U64(s);		/* shift it back right one, so <= dividend */
	shift--;

	q[0] = 0;		/* start quotient at 0 */
	q[1] = 0;

	/*
	 * Loop once for each bit shifted.
	 */
	for ( ; shift >= 0; shift--) {
		/*
		 * If the current divisor does not exceed what's left of the
		 * dividend, subtract it from it, and record that by setting
		 * the low order bit of the current quotient.
		 */
		if ( ! GT64(s, r) ) {
			t[0] = s[0];
			t[1] = s[1];
			NEG64(t);
			add64_64(r, r, t);
			q[0] |= 1;
		}

		/* shift quotient left and divisor right */
		SHL1U64(q);
		SHR1U64(s);
	}

	SHR1U64(q);		/* shift quotient right */
}

/*
 *	red64_64()	reduce a 64 bit over 64 bit rational to lowest terms
 *
 *	This function takes two 64-bit numbers as numerator and denominator
 *	of a rational number, and reduces them to lowest terms, with the
 *	denominator positive.  If the user called this package correctly, the
 *	denominator cannot be zero, but to be defensive we check for that, and
 *	if it happens, set raterrno to RATPARM and either print a message or
 *	call a user-supplied error handler.
 *
 *	Parameters:	num	numerator
 *			den	denominator
 *
 *	Return value:	none
 *
 *	Side effects:	num and den are altered to be the result
 */

static void
red64_64(num, den)

INT32B num[2];
INT32B den[2];

{
	INT32B b[2], c[2], r[2]; /* temp variables for Euclidean algorithm */
	INT32B junk[2];		/* placeholder for calling divmod64 */
	int sign;		/* answer is pos (1) or neg (-1) */


	if (den[1] == 0 && den[0] == 0)	{ /* if den == 0 */
		/*
		 * This is an error.  The user must have called a routine with
		 * an invalid number for us to get here, in fact a number with
		 * a zero denominator, since "den" here always was created as
		 * the product of denominators.  Report the error and return
		 * zero.
		 */
		num[1] = 0;		/* set num = 0 */
		num[0] = 0;
		den[1] = 0;		/* set den = 1 */
		den[0] = 1;
		ratmsg(RATPARM);	/* set raterrno, report error */
		return;
	}

	if (num[1] == 0 && num[0] == 0)	{ /* if num == 0 */
		den[1] = 0;		/* set den = 1; answer is 0/1 */
		den[0] = 1;
		return;
	}

	/* now figure out sign of answer, and make num & den positive */
	sign = 1;			/* init to positive */
	if (num[1] < 0) {		/* if numerator neg */
		sign = -sign;		/* reverse the sign */
		NEG64(num);
	}
	if (den[1] < 0) {		/* if denominator neg */
		sign = -sign;		/* reverse the sign */
		NEG64(den);
	}

	/* now check whether numerator or denominator is larger */
	if (EQ64(num, den)) {
		num[0] = sign;		/* answer is +1 or -1 */

		if (sign < 0)		/* set high order word to sign bit */
			num[1] = -1;
		else
			num[1] = 0;

		den[1] = 0;		/* set den to 1 */
		den[0] = 1;

		return;
	}

	/* set up c and b so that one is num, the other den, and c > b */
	if (LT64(num, den)) {			/* if num < den */
		c[0] = den[0];			/* c = den */
		c[1] = den[1];
		b[0] = num[0];			/* b = num */
		b[1] = num[1];
	} else {
		c[0] = num[0];			/* c = num */
		c[1] = num[1];
		b[0] = den[0];			/* b = den */
		b[1] = den[1];
	}

	/* use Euclidean Algorithm to find greatest common divisor of c & b */
	do {
		divmod64(c, b, junk, r);	/* r = c % b */
		c[0] = b[0];			/* c = b */
		c[1] = b[1];
		b[0] = r[0];			/* b = r */
		b[1] = r[1];
	} while (r[0] != 0 || r[1] != 0);	/* while r != 0 */

	/* now c is the greatest common divisor of num and den */

	/* divide out the greatest common divisor and put the sign in */
	divmod64(num, c, num, junk);		/* num /= c */
	if (sign < 0)				/* if should be negative */
		NEG64(num);			/* negate the numerator */
	divmod64(den, c, den, junk);		/* den /= c */

	return;
}
