
/*
 Copyright (c) 1995-2024  by Arkkra Enterprises.
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

/* This file contains functions for manipulating Mup's main list of structs,
 * such as allocating, inserting and deleting.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"




/* allocate a new MAINLL struct and return a pointer to it */

struct MAINLL *
newMAINLLstruct(structtype, lineno)

int structtype;		/* what kind to allocate: S_SSV, S_BAR, etc */
int lineno;		/* input line number that caused this call */

{
	struct MAINLL *new_p;	/* the newly allocated struct */


	debug(4, "newMAINLLstruct lineno=%d structtype=%d", lineno, structtype);

	/* allocate the struct */
	CALLOC(MAINLL, new_p, 1);

	/* fill in the type to say which union member will be used */
	new_p->str = (short) structtype;

	/* initialize link pointers to point nowhere */
	new_p->next = new_p->prev = (struct MAINLL *) 0;


	/* now allocate and initialize the proper S_* struct */
	switch (structtype) {

	case S_SSV:
		CALLOC(SSV, new_p->u.ssv_p, 1);
		break;

	case S_STAFF:
		CALLOC(STAFF, new_p->u.staff_p, 1);
		break;

	case S_BAR:
		CALLOC(BAR, new_p->u.bar_p, 1);
		break;

	case S_LINE:
		CALLOC(LINE, new_p->u.line_p, 1);
		break;

	case S_CURVE:
		CALLOC(CURVE, new_p->u.curve_p, 1);
		break;

	case S_PRHEAD:
		CALLOC(PRHEAD, new_p->u.prhead_p, 1);
		break;

	case S_CHHEAD:
		CALLOC(CHHEAD, new_p->u.chhead_p, 1);
		break;

	case S_FEED:
		CALLOC(FEED, new_p->u.feed_p, 1);
		/* Init margin values to what means to use score parameter */
		new_p->u.feed_p->rightmargin = MG_DEFAULT;
		new_p->u.feed_p->leftmargin = MG_DEFAULT;
		new_p->u.feed_p->left_mot = MOT_UNUSED;
		new_p->u.feed_p->right_mot = MOT_UNUSED;
		 new_p->u.feed_p->scoresep = INVSEPVAL;
		break;

	case S_BLOCKHEAD:
		CALLOC(BLOCKHEAD, new_p->u.blockhead_p, 1);
		break;

	case S_CLEFSIG:
		CALLOC(CLEFSIG, new_p->u.clefsig_p, 1);
		break;

	default:
		pfatal("unknown structure type %d requested", structtype);
		break;
	}

	/* remember the user's input file and line number,
	 * so in case we have to print an error message later,
	 * we know which line number to print */
	new_p->inputlineno = (short) lineno;
	new_p->inputfile = Curr_filename;

	/* return the newly allocated struct */
	return(new_p);
}


/* insert MAINLL struct into main list, after an arbitrary existing struct. */
/* If where to insert is NULL, put at beginning of list */

void
insertMAINLL(info_p, where)

struct MAINLL *info_p;	/* what to insert */
struct MAINLL *where;	/* put it right after this one in the list */

{
	if (info_p == where) {
		/* Any bug that gets us here would cause an infinite loop */
		pfatal("attempt to insert a MAINLL after itself, type %d from %s line %d",
			info_p->str, info_p->inputfile, info_p->inputlineno);
	}

	/* if where is NULL, this means to insert at beginning of list */
	if (where == (struct MAINLL *) 0) {
		if (Mainllhc_p != (struct MAINLL *) 0) {
			Mainllhc_p->prev = info_p;
		}
		info_p->prev = (struct MAINLL *) 0;
		info_p->next = Mainllhc_p;	
		Mainllhc_p = info_p;
	}

	else {
		/* standard linked list stuff --
		 * fix up the next and prev pointers */
		info_p->next = where->next;
		info_p->prev = where;
		if (where->next != (struct MAINLL *) 0) {
			where->next->prev = info_p;
		}
		where->next = info_p;
	}

	/* if we just added to the very end of the list, need to adjust the
	 * tail pointer */
	if ( (Mainlltc_p == (struct MAINLL *) 0) || (where == Mainlltc_p) ) {
		Mainlltc_p = info_p;
	}

	/* If this was an internally generated item, we adjust the
	 * filename and line number to be like that of the next entry if any,
	 * otherwise the previous. This may make error messages
	 * more helpful. */
	if (info_p->inputfile == 0 || info_p->inputlineno < 1) {
		if (info_p->next != 0) {
			info_p->inputfile = info_p->next->inputfile;
			info_p->inputlineno = info_p->next->inputlineno;
		}
		else if (info_p->prev != 0) {
			info_p->inputfile = info_p->prev->inputfile;
			info_p->inputlineno = info_p->prev->inputlineno;
		}
	}
}


/* unlink a MAINLL struct from the main list
 * (probably for re-inserting elsewhere). The struct is not freed. */

void
unlinkMAINLL(which_p)

struct MAINLL *which_p;	/* the one to unlink */

{
	if (which_p->prev != (struct MAINLL *) 0) {
		which_p->prev->next = which_p->next;
	}
	if (which_p->next != (struct MAINLL *) 0) {
		which_p->next->prev = which_p->prev;
	}

	/* if this happened to be the tail cell, need to fix up */
	if (Mainlltc_p == which_p) {
		Mainlltc_p = which_p->prev;
	}

	/* likewise for head cell */
	if (Mainllhc_p == which_p) {
		Mainllhc_p = which_p->next;
	}
}
