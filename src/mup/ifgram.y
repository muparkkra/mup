
%{

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

/* This file contains a "mini-parser" that handles "if" clauses.
 * It uses a buffer containing a compressed form of the user's input.
 * The compressed format is described in lex.c, above add_ifclause().
 * This tries to follow the ANSI C preprocessor rules as much as is reasonable.
 * It uses C operator precedences and associativity.
 * We do use 32-bit ints rather than longs to try to avoid any
 * incompatibilities if someone creates a Mup file on a 32-bit machine
 * and then tries to run it on 64-bit or vice-versa.
 */

#include "defines.h"
#include "globals.h"

#define YYDEBUG 1

%}

%token ICT_AND
%token ICT_BITAND
%token ICT_BITCOMPLEMENT
%token ICT_BITOR
%token ICT_BITXOR
%token ICT_COLON
%token ICT_DIVIDE
%token ICT_END
%token ICT_ERROR
%token ICT_EQ
%token ICT_FALSE
%token ICT_GT
%token ICT_GE
%token ICT_LPAREN
%token ICT_LE
%token ICT_LT
%token ICT_MINUS
%token ICT_MOD
%token ICT_MULT
%token ICT_NE
%token ICT_NOT
%token ICT_OR
%token ICT_PLUS
%token ICT_QUESTION
%token ICT_RPAREN
%token ICT_SHLEFT
%token ICT_SHRIGHT
%token ICT_TRUE
%token ICT_VALUE

%right ICT_QUESTION ICT_COLON
%left ICT_OR
%left ICT_AND
%left ICT_BITOR
%left ICT_BITXOR
%left ICT_BITAND
%left ICT_EQ ICT_NE
%left ICT_LT ICT_GT ICT_LE ICT_GE
%left ICT_SHLEFT ICT_SHRIGHT
%left ICT_PLUS ICT_MINUS
%left ICT_MULT ICT_DIVIDE ICT_MOD
%right ICT_NOT ICT_BITCOMPLEMENT

%%

ifclause:	expr ICT_END
	{
		return($1 ? YES : NO);
	}

	|
	ICT_ERROR
	{
		pfatal("if clause parser received unknown token type");
	}

	|
	error
	{
		l_yyerror(Curr_filename, yylineno, "syntax error in 'if' condition");
		return(NO);
	}
	;

expr:	ICT_LPAREN expr ICT_RPAREN
	{
		$$ = $2;
	}

	|
	ICT_NOT expr
	{
		$$ = !($2);
	}

	|
	ICT_BITCOMPLEMENT expr
	{
		$$ = ~($2);
	}

	|
	ICT_MINUS expr  %prec ICT_NOT
	{
		/* unary minus */
		$$ = -($2);
	}

	|
	ICT_PLUS expr  %prec ICT_NOT
	{
		/* unary plus. Not very useful, but ANSI C supports it... */
		$$ = ($2);
	}

	|
	expr ICT_MULT expr
	{
		$$ = $1 * $3;
	}

	|
	expr ICT_DIVIDE expr
	{
		if ($3 == 0) {
			yyerror("attempt to divide by 0");
			/* gcc appears to return the numerator in this case,
			 * so we do the same */
			$$ = $1;
		}
		else {
			$$ = $1 / $3;
		}
	}

	|
	expr ICT_MOD expr
	{
		if ($3 == 0) {
			yyerror("attempt to modulo by 0");
			$$ = $1;
		}
		else {
			$$ = $1 % $3;
		}
	}

	|
	expr ICT_PLUS expr
	{
		$$ = $1 + $3;
	}

	|
	expr ICT_MINUS expr
	{
		$$ = $1 - $3;
	}

	|
	expr ICT_SHLEFT expr
	{
		$$ = $1 << $3;
	}

	|
	expr ICT_SHRIGHT expr
	{
		$$ = $1 >> $3;
	}
	
	|
	expr ICT_LT expr
	{
		$$ = ($1 < $3);
	}

	|
	expr ICT_LE expr
	{
		$$ = ($1 <= $3);
	}

	|
	expr ICT_GT expr
	{
		$$ = ($1 > $3);
	}

	|
	expr ICT_GE expr
	{
		$$ = ($1 >= $3);
	}

	|
	expr ICT_EQ expr
	{
		$$ = ($1 == $3);
	}

	|
	expr ICT_NE expr
	{
		$$ = ($1 != $3);
	}

	|
	expr ICT_BITAND expr
	{
		$$ = $1 & $3;
	}

	|
	expr ICT_BITXOR expr
	{
		$$ = $1 ^ $3;
	}

	|
	expr ICT_BITOR expr
	{
		$$ = $1 | $3;
	}

	|
	expr ICT_OR expr
	{
		$$ = $1 || $3;
	}

	|
	expr ICT_AND expr
	{
		$$ = $1 && $3;
	}

	|
	expr ICT_QUESTION expr ICT_COLON expr
	{
		$$ = ( $1 ? $3 : $5 );
	}

	|
	ICT_VALUE
	{
		$$ = $1;
	}

	|
	boolval
	{
		$$ = $1;
	}
	;

boolval:	ICT_TRUE
	{
		$$ = 1;
	}

	|
	ICT_FALSE
	{
		$$ = 0;
	}
	;


%%

/* This is a pointer to the buffer containing the clause in compressed form.
 * A call to set_ifclause_buffer() sets this to point to an actual array */
static unsigned char * Buffer;

/* Our current position in Buffer */
static int Offset;

/* length of Buffer */
static int Length;



/* To use this mini-parser, put the clause in compressed internal form
 * in an array, and pass that array and its length to this function,
 * then call ifparse().
 */

void
set_ifclause_buffer(buff, len)

unsigned char * buff;
int len;

{
	Buffer = buff;
	Length = len;
	Offset = 0;
}


/* The compressed if-clause format is described in lex.c above add_ifclause().
 * Since it's very simple (everything except numbers
 * are single character tokens), we have a hand-coded lexer.
 * It maps the compressed format tokens to bison tokens.
 */

int
iflex()
{
	if (Offset == Length) {
		return(ICT_END);
	}

	switch(Buffer[Offset++]) {

	case '(':
		return(ICT_LPAREN);
	case ')':
		return(ICT_RPAREN);

	case '!':
		return(ICT_NOT);
	case '~':
		return(ICT_BITCOMPLEMENT);

	case '*':
		return(ICT_MULT);
	case '/':
		return(ICT_DIVIDE);
	case '%':
		return(ICT_MOD);

	case '+':
		return(ICT_PLUS);
	case '-':
		return(ICT_MINUS);

	case 'l':
		return(ICT_SHLEFT);
	case 'r':
		return(ICT_SHRIGHT);

	case '<':
		return(ICT_LT);
	case '>':
		return(ICT_GT);
	case 'L':
		return(ICT_LE);
	case 'G':
		return(ICT_GE);

	case 'E':
		return(ICT_EQ);
	case 'N':
		return(ICT_NE);

	case '&':
		return(ICT_BITAND);

	case '^':
		return(ICT_BITXOR);

	case '|':
		return(ICT_BITOR);

	case 'a':
		return(ICT_AND);

	case 'o':
		return(ICT_OR);

	case '?':
		return(ICT_QUESTION);
	case ':':
		return(ICT_COLON);

	case 'T':
		return(ICT_TRUE);
	case 'F':
		return(ICT_FALSE);
	case '#':
		iflval = ((Buffer[Offset] << 24) & 0xff000000)
			| ((Buffer[Offset+1] << 16) & 0xff0000)
			| ((Buffer[Offset+2] << 8) & 0xff00)
			| (Buffer[Offset+3] & 0xff);
		Offset += 4;
		return(ICT_VALUE);

	default:
		return(ICT_ERROR);
	}
}	



/* Error message printer for syntax/semantic errors in 'if' clauses.
 * WARNING: bison appears to somehow use yyerror even when using a prefix
 * other than yy. So if this function calls yyerror, it gets into an
 * infinite loop! So don't do that...
 */

int
iferror(msg)

char * msg;

{
	l_yyerror(Curr_filename, yylineno, msg);
	return(0);
}
