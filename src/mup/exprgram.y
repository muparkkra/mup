%{

/*
 Copyright (c) 1995-2022  by Arkkra Enterprises.
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

/* This file contains a "mini-parser" that handles "if" and "eval" expressions.
 * It uses a buffer containing a compressed form of the user's input.
 * The compressed format is described in lex.c, above add2expr().
 * This tries to follow the ANSI C preprocessor rules as much as is reasonable.
 * It uses C operator precedences and associativity.
 * The "eval" expression adds support for floating point. Once any float is
 * encountered, arithmetic is done in float, so the float field is always
 * filled in, in case it becomes necessary to promote a value to float.
 * An "eval" expression can also have certain functions, such as sqrt, sin,
 * cos, hypot, etc
 */

#include "defines.h"
#include "globals.h"
#include "structs.h"
#include <errno.h>
#ifndef __DJGPP__
#include <fenv.h>
#else
double round P((double num));
#endif

#define YYDEBUG 1

/* The result in stored here, and do_eval() in macros.c gets the answer from
 * here. That allows the parser to just return YES/NO, which is what the
 * if clause usage wants, and is the normal return type of a yacc/bison parser.
 */
struct VALUE Expr_result;
/* Current function name is stored here, for NaN error messages */
static char *Setfunc;
static void nancheck P((double num, char *opname));

/* Define macros for the code that is the same for several operators */
#define COMPARISON(NAME, OPERATOR) \
	static struct VALUE \
	NAME(val1, val2) \
	struct VALUE val1, val2; \
	{ \
		struct VALUE result; \
		result.type = TYPE_INT; \
		if ((val1.type == TYPE_INT) && (val2.type == TYPE_INT)) { \
			result.intval = (val1.intval OPERATOR val2.intval); \
		} \
		else { \
			result.intval = (val1.floatval OPERATOR val2.floatval); \
		} \
		result.floatval = (double) result.intval; \
		return(result); \
	}
COMPARISON(lessthan, <)
COMPARISON(greaterthan, >)
COMPARISON(lessequal, <=)
COMPARISON(greaterequal, >=)
COMPARISON(equal, ==)
COMPARISON(notequal, !=)

#define INT_ONLY_UNARY_OP(NAME, OPERATOR) \
	static struct VALUE \
	NAME(val1) \
	struct VALUE val1; \
	{ \
		struct VALUE result; \
		result.type = TYPE_INT; \
		if (val1.type == TYPE_INT) { \
			result.intval = OPERATOR (val1.intval); \
		} \
		else { \
			l_yyerror(Curr_filename, yylineno, "Can only use " #OPERATOR " with integer type"); \
			/* Put something in intval */ \
			result.intval = 0; \
		} \
		result.floatval = (double) result.intval; \
		return(result); \
	}
INT_ONLY_UNARY_OP(not, !)
INT_ONLY_UNARY_OP(complement, ~)


#define SIGN_OP(NAME, OPERATOR) \
	static struct VALUE \
	NAME(val1) \
	struct VALUE val1; \
	{ \
		struct VALUE result; \
		if (val1.type == TYPE_INT) { \
			result.type = TYPE_INT; \
			result.intval = OPERATOR (val1.intval); \
			result.floatval = (double) result.intval; \
		} \
		else { \
			result.type = TYPE_FLOAT; \
			result.floatval = OPERATOR (val1.floatval); \
			nancheck(result.floatval, #OPERATOR); \
		} \
		return(result); \
	}
SIGN_OP(unary_minus, -)
SIGN_OP(unary_plus, +)

#define BINARY_ARITH_OP(NAME, OPERATOR) \
	static struct VALUE \
	NAME(val1, val2) \
	struct VALUE val1, val2; \
	{ \
		struct VALUE result; \
		if ((val1.type == TYPE_INT) && (val2.type == TYPE_INT)) { \
			result.type = TYPE_INT; \
			result.intval = val1.intval OPERATOR val2.intval; \
		} \
		else { \
			result.type = TYPE_FLOAT; \
		} \
		result.floatval = val1.floatval OPERATOR val2.floatval; \
		nancheck(result.floatval, #OPERATOR); \
		return(result); \
	}
BINARY_ARITH_OP(mult, *)
BINARY_ARITH_OP(add, +)
BINARY_ARITH_OP(sub, -)

#define BINARY_INT_ONLY_OP(NAME, OPERATOR) \
	static struct VALUE \
	NAME(val1, val2) \
	struct VALUE val1, val2; \
	{ \
		struct VALUE result; \
		if ((val1.type == TYPE_INT) && (val2.type == TYPE_INT)) { \
			result.type = TYPE_INT; \
			result.intval = val1.intval OPERATOR val2.intval; \
			result.floatval = (double) result.intval; \
		} \
		else { \
			yyerror("can only use " #OPERATOR " on integer types"); \
			result.type = TYPE_FLOAT; \
			/* Not clear what value to return; use left expr */ \
			result.floatval = val1.floatval; \
		} \
		return(result); \
	}
BINARY_INT_ONLY_OP(shiftleft, <<)
BINARY_INT_ONLY_OP(shiftright, >>)
BINARY_INT_ONLY_OP(bitand, &)
BINARY_INT_ONLY_OP(bitxor, ^)
BINARY_INT_ONLY_OP(bitor, |)
BINARY_INT_ONLY_OP(and, &&)
BINARY_INT_ONLY_OP(or, ||)

%}

%union {
struct VALUE value;		/* A number value. The floatnum field should
				 * always be filled in, even when an int,
				 * in case it needs to be promoted. */
double (*dfuncn)(double);	/* A function that takes a double arg
				 * and returns a double */
double (*dfuncnn)(double, double); /* A function that takes two doubles as args,
				    * and returns a double */
}


%token ET_AND
%token ET_ASSIGN
%token ET_BITAND
%token ET_BITCOMPLEMENT
%token ET_BITOR
%token ET_BITXOR
%token ET_COLON
%token ET_COMMA
%token <dfuncn> ET_DFUNCN
%token <dfuncn> ET_DFUNCR
%token <dfuncnn> ET_DFUNCNN
%token ET_DIVIDE
%token ET_END
%token ET_ERROR
%token ET_EQ
%token ET_EVAL
%token ET_FALSE
%token <value> ET_FLOAT_VALUE
%token ET_GT
%token ET_GE
%token ET_IFCLAUSE
%token <dfuncn> ET_IFUNCN
%token <value> ET_INT_VALUE
%token ET_LPAREN
%token ET_LE
%token ET_LT
%token ET_MINUS
%token ET_MOD
%token ET_MULT
%token ET_NE
%token ET_NOT
%token ET_OR
%token ET_PLUS
%token ET_QUESTION
%token <dfuncn> ET_RFUNCN
%token <dfuncnn> ET_RFUNCNN
%token ET_RPAREN
%token ET_SHLEFT
%token ET_SHRIGHT
%token ET_TRUE

%type <value> boolval
%type <value> expr

%right ET_QUESTION ET_COLON
%left ET_OR
%left ET_AND
%left ET_BITOR
%left ET_BITXOR
%left ET_BITAND
%left ET_EQ ET_NE
%left ET_LT ET_GT ET_LE ET_GE
%left ET_SHLEFT ET_SHRIGHT
%left ET_PLUS ET_MINUS
%left ET_MULT ET_DIVIDE ET_MOD
%right ET_NOT ET_BITCOMPLEMENT

%%

expression:
	ET_EVAL ET_ASSIGN expr ET_END
	{
		Expr_result = $3;
		return(YES);
	}

	|
	ET_IFCLAUSE expr ET_END
	{
		return($2.intval ? YES : NO);
	}

	|
	ET_ERROR
	{
		pfatal("expression parser received unknown token type");
	}

	|
	error
	{
		l_yyerror(Curr_filename, yylineno, "syntax error in expression");
		return(NO);
	}
	;

expr:	ET_LPAREN expr ET_RPAREN
	{
		$$ = $2;
	}

	|
	ET_NOT expr
	{
		$$ = not($2);
	}

	|
	ET_BITCOMPLEMENT expr
	{
		$$ = complement($2);
	}

	|
	ET_MINUS expr  %prec ET_NOT
	{
		/* unary minus */
		$$ = unary_minus($2);
	}

	|
	ET_PLUS expr  %prec ET_NOT
	{
		/* unary plus. Not very useful, but ANSI C supports it... */
		$$ = unary_plus($2);
	}

	|
	expr ET_MULT expr
	{
		$$ = mult($1, $3);
	}

	|
	expr ET_DIVIDE expr
	{
		if (($1.type == TYPE_INT) && ($3.type == TYPE_INT)) {
			$$.type = TYPE_INT;
			if ($3.intval == 0) {
				yyerror("attempt to divide by 0");
				/* gcc appears to return the numerator in this case,
				 * so we do the same */
				$$.intval = $1.intval;
			}
			else {
				$$.intval = $1.intval / $3.intval;
			}
			$$.floatval = (double) $$.intval;
		}
		else {
			$$.type = TYPE_FLOAT;
			if ($3.floatval == 0.0) {
				yyerror("attempt to divide by 0.0");
				$$.floatval = $1.floatval;
			}
			else {
				$$.floatval = $1.floatval / $3.floatval;
				nancheck($$.floatval, "/");
			}
		}
	}

	|
	expr ET_MOD expr
	{
		if (($1.type == TYPE_INT) && ($3.type == TYPE_INT)) {
			$$.type = TYPE_INT;
			if ($3.intval == 0) {
				yyerror("attempt to %% by 0");
				$$.intval = $1.intval;
			}
			else {
				$$.intval = $1.intval % $3.intval;
			}
			$$.floatval = (double) $$.intval;
		}
		else {
			yyerror("can only use %% on integer types");
			$$.type = TYPE_FLOAT;
			$$.floatval = $1.floatval;
		}
	}

	|
	expr ET_PLUS expr
	{
		$$ = add($1, $3);
	}

	|
	expr ET_MINUS expr
	{
		$$ = sub($1, $3);
	}

	|
	expr ET_SHLEFT expr
	{
		$$ = shiftleft($1, $3);
	}

	|
	expr ET_SHRIGHT expr
	{
		$$ = shiftright($1, $3);
	}
	
	|
	expr ET_LT expr
	{
		$$ = lessthan($1, $3);
	}

	|
	expr ET_LE expr
	{
		$$ = lessequal($1, $3);
	}

	|
	expr ET_GT expr
	{
		$$ = greaterthan($1, $3);
	}

	|
	expr ET_GE expr
	{
		$$ = greaterequal($1, $3);
	}

	|
	expr ET_EQ expr
	{
		$$ = equal($1, $3);
	}

	|
	expr ET_NE expr
	{
		$$ = notequal($1, $3);
	}

	|
	expr ET_BITAND expr
	{
		$$ = bitand($1, $3);
	}

	|
	expr ET_BITXOR expr
	{
		$$ = bitxor($1, $3);
	}

	|
	expr ET_BITOR expr
	{
		$$ = bitor($1, $3);
	}

	|
	expr ET_OR expr
	{
		$$ = or($1, $3);
	}

	|
	expr ET_AND expr
	{
		$$ = and($1, $3);
	}

	|
	expr ET_QUESTION expr ET_COLON expr
	{
		if ($1.type == TYPE_INT) {
			$$ = ( $1.intval ? $3 : $5 );
		}
		else {
			$$ = ( $1.floatval ? $3 : $5 );
		}
		if ( ($3.type != TYPE_INT) || ($5.type != TYPE_INT) ) {
			$$.type = TYPE_FLOAT;
		}
	}

	|
	ET_INT_VALUE
	{
		$$ = $1;
	}

	|
	ET_FLOAT_VALUE
	{
		$$ = $1;
	}

	|
	ET_DFUNCN ET_LPAREN expr ET_RPAREN
	{
		/* A function with a double arg that returns double */
		$$.type = TYPE_FLOAT;
		$$.floatval = (*$1)($3.floatval);
		nancheck($$.floatval, Setfunc);
	}

	|
	ET_DFUNCR ET_LPAREN expr ET_RPAREN
	{
		/* A function with double arg in radians that returns double,
		 * so need to convert user's degrees to radians */
		$$.type = TYPE_FLOAT;
		$$.floatval = (*$1)(DEG2RAD($3.floatval));
		nancheck($$.floatval, Setfunc);
	}

	|
	ET_RFUNCN ET_LPAREN expr ET_RPAREN
	{
		/* A function with double arg that returns radians,
		 * so need to convert result to degrees */
		$$.type = TYPE_FLOAT;
		$$.floatval = RAD2DEG((*$1)($3.floatval));
		nancheck($$.floatval, Setfunc);
	}

	|
	ET_RFUNCNN ET_LPAREN expr ET_COMMA expr ET_RPAREN
	{
		/* A function that takes two doubles as args,
		 * and returns radians as a double, so need to
		 * convert to degrees, like the user expects.
		 */
		$$.type = TYPE_FLOAT;
		/* Convert radians to degrees */
		$$.floatval =  RAD2DEG((*$1)($3.floatval, $5.floatval));
		nancheck($$.floatval, Setfunc);
	}

	|
	ET_DFUNCNN ET_LPAREN expr ET_COMMA expr ET_RPAREN
	{
		/* A function that takes two doubles as args,
		 * and returns a double. */
		$$.type = TYPE_FLOAT;
		$$.floatval =  (*$1)($3.floatval, $5.floatval);
		nancheck($$.floatval, Setfunc);
	}

	|
	ET_IFUNCN ET_LPAREN expr ET_RPAREN
	{
		/* A function that takes a double as arg,
		 * and returns a double, but we then cast that to int. */
		$$.type = TYPE_INT;
		$$.intval =  (int) (*$1)($3.floatval);
	}
	|
	boolval
	{
		$$ = $1;
	}
	;

boolval:	ET_TRUE
	{
		$$.type = TYPE_INT;
		$$.intval = 1;
		$$.floatval = 1.0;
	}

	|
	ET_FALSE
	{
		$$.type = TYPE_INT;
		$$.intval = 0;
		$$.floatval = 0.0;
	}
	;

%%

/* This is a pointer to the buffer containing the expression
 * to be prased, in compressed form.
 * A call to set_expr_buffer() sets this to point to an actual array. */
static char *Buffer;

/* Our current position in Buffer */
static int Offset;

/* Length of the expresion in Buffer */
static int Length;

/* Tables that have the literal number values, and indexes into those tables */
extern double *Dbls_table;
extern int Dbls_index;
extern int *Ints_table;
extern int Ints_index;


/* To use this mini-parser, put the expression in compressed internal form
 * in an array, and pass that array and its length to this function,
 * then call exprparse().
 */

void
set_expr_buffer(buff, len)

char * buff;
int len;

{
	Buffer = buff;
	Length = len;
	Offset = 0;
	Dbls_index = 0;
	Ints_index = 0;
	/* Arrange to check for floating point exceptions */
	errno = 0;
#ifndef __DJGPP__
	feclearexcept(FE_ALL_EXCEPT);
#endif
}


/* The compressed expression format is described in lex.c above add2expr().
 * Since it's very simple (most tokens are a single character)
 * we have a hand-coded lexer.
 * It maps the compressed format tokens to bison tokens.
 */

int
exprlex()
{
	if (Offset == Length) {
		return(ET_END);
	}

	switch(Buffer[Offset++]) {

	case 'i':
		return(ET_IFCLAUSE);
	case 'e':
		return(ET_EVAL);
	case '=':
		return(ET_ASSIGN);
	case '(':
		return(ET_LPAREN);
	case ')':
		return(ET_RPAREN);

	case '!':
		return(ET_NOT);
	case '~':
		return(ET_BITCOMPLEMENT);

	case '*':
		return(ET_MULT);
	case '/':
		return(ET_DIVIDE);
	case '%':
		return(ET_MOD);

	case '+':
		return(ET_PLUS);
	case '-':
		return(ET_MINUS);

	case 'l':
		return(ET_SHLEFT);
	case 'r':
		return(ET_SHRIGHT);

	case '<':
		return(ET_LT);
	case '>':
		return(ET_GT);
	case 'L':
		return(ET_LE);
	case 'G':
		return(ET_GE);

	case 'E':
		return(ET_EQ);
	case 'N':
		return(ET_NE);

	case '&':
		return(ET_BITAND);

	case '^':
		return(ET_BITXOR);

	case '|':
		return(ET_BITOR);

	case 'a':
		return(ET_AND);

	case 'o':
		return(ET_OR);

	case '?':
		return(ET_QUESTION);
	case ':':
		return(ET_COLON);

	case 'T':
		return(ET_TRUE);
	case 'F':
		return(ET_FALSE);
	case '#':
		exprlval.value.type = TYPE_INT;
		exprlval.value.intval = Ints_table[Ints_index++];
		exprlval.value.floatval = (double) exprlval.value.intval;
		return(ET_INT_VALUE);
	case '$':
		exprlval.value.type = TYPE_FLOAT;
		exprlval.value.floatval = Dbls_table[Dbls_index++];
		return(ET_FLOAT_VALUE);
	case ',':
		return(ET_COMMA);
	case 'f':
		switch(Buffer[Offset++]) {
		case 'q':
			exprlval.dfuncn = sqrt;
			Setfunc = "sqrt";
			return(ET_DFUNCN);
		case 's':
			exprlval.dfuncn = sin;
			Setfunc = "sin";
			return(ET_DFUNCR);
		case 'c':
			exprlval.dfuncn = cos;
			Setfunc = "cos";
			return(ET_DFUNCR);
		case 't':
			exprlval.dfuncn = tan;
			Setfunc = "tan";
			return(ET_DFUNCR);
		case 'S':
			exprlval.dfuncn = asin;
			Setfunc = "asin";
			return(ET_RFUNCN);
		case 'C':
			exprlval.dfuncn = acos;
			Setfunc = "acos";
			return(ET_RFUNCN);
		case 'T':
			exprlval.dfuncn = atan;
			Setfunc = "atan";
			return(ET_RFUNCN);
		case 'a':
			exprlval.dfuncnn = atan2;
			Setfunc ="atan2";
			return(ET_RFUNCNN);
		case 'h':
			exprlval.dfuncnn = hypot;
			Setfunc = "hypot";
			return(ET_DFUNCNN);
		case 'r':
			exprlval.dfuncn = round;
			return(ET_IFUNCN);
		case 'f':
			exprlval.dfuncn = floor;
			return(ET_IFUNCN);
		case 'L':
			exprlval.dfuncn = ceil;
			return(ET_IFUNCN);
		default:
			pfatal("unrecognized  eval function %c", Buffer[Offset-1]);
			/*NOTREACHED*/
			return(ET_ERROR);
		}
	default:
		return(ET_ERROR);
	}
}	


/* Report error if the given number is NaN */

static void
nancheck(num, opname)

double num;
char *opname;

{
	if (isnan(num)) {
		l_yyerror(Curr_filename, yylineno, "result of %s was not a valid number", opname);
	}
}

/* Error message printer for syntax/semantic errors in expressions.
 * WARNING: bison appears to somehow use yyerror even when using a prefix
 * other than yy. So if this function calls yyerror, it gets into an
 * infinite loop! So don't do that...
 */

int
exprerror(msg)

char * msg;

{
	l_yyerror(Curr_filename, yylineno, msg);
	return(0);
}

/* At least some early versions of djgpp compiler do not include a round()
 * function in the math library, so we include an implementation here. */

#ifdef __DJGPP__
double
round(double num)
{
        double sign = 1.0;
        if (num < 0.0) {
                sign = -1.0;
                num = -num;
        }
        return (sign * (long long)(num + 0.5));
}
#endif
