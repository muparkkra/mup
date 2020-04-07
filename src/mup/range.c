
/*
 Copyright (c) 1995-2020  by Arkkra Enterprises.
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

/* This file contains functions for saving away ranges of staff numbers
 * and voice/verse numbers, when the user  is defining groups, lyrics,
 * or STUFF for one or more staffs and/or voices/verses.
 */


#include "defines.h"
#include "structs.h"
#include "globals.h"

/* special marker for "all." use something that can't possibly be a staff num */
#define ALL	(-1)


static void free_rangelist P((struct RANGELIST *list_p));



/* When a line of input is being gathered for groups, lyrics, or stuff,
 * this function should be called first. It makes
 * sure the current range list is set to empty and makes a note of the place,
 * (PL_ABOVE, PL_BELOW, or PL_BETWEEN) for later reference
 */

void
begin_range(place)

int place;	/* PL_ABOVE, etc */

{
	Staffrange_p = (struct RANGELIST *) 0;
	Vnorange_p = (struct RANGELIST *) 0;
	Place = (short) place;
}


/* This function is called when the parser has found a range of staffs that
 * is to get the current set of groups, lyrics or stuff.
 * In the degenerate case, the range
 * may be a single staff. In the case of PL_BETWEEN, the ending staff number
 * must be one more than the beginning. 
 * If the endstaffno is ALL, this is a special case of "all" as in
 * "above all" or "below all."
 */

void
save_staff_range(beginstaffno, endstaffno)

int beginstaffno;	/* first staff in range */
int endstaffno;		/* last staff in range */

{
	struct RANGELIST *new_p;	/* to save info about this range */
	short is_all = NO;
	

	/* handle special case of "all" */
	if (endstaffno == ALL) {
		is_all = YES;
		endstaffno = beginstaffno;
	}

	/* do error checks */
	if (rangecheck(beginstaffno, 1, MAXSTAFFS, "staff number") == NO) {
		return;
	}
	if (rangecheck(endstaffno, 1, MAXSTAFFS, "staff number") == NO) {
		return;
	}

	if (endstaffno < beginstaffno) {
		yyerror("end of staff range smaller than beginning");
		return;
	}

	if (Place == PL_BETWEEN) {
		if (endstaffno != beginstaffno + 1) {
			yyerror("if place is 'between', second staff must be 1 greater than first");
			return;
		}

		if (beginstaffno == Score.staffs) {
			yyerror("can't use 'between' on bottom staff");
			return;
		}
	}

	/* allocate a new struct and link onto head of list */
	CALLOC(RANGELIST, new_p, 1);
	new_p->next = Staffrange_p;
	Staffrange_p = new_p;

	/* fill in other fields */
	new_p->begin = (short) beginstaffno;
	new_p->end = (short) (Place == PL_BETWEEN ? beginstaffno : endstaffno);
	new_p->all = is_all;
	new_p->place = Place;
}


/* given a range of vno's, save the range for later use */
/* Any error checking of the numbers should be done before calling this
 * function. */

void
save_vno_range(begin, end)

int begin;	/* first vno */
int end;	/* last vno */

{
	struct RANGELIST *new_p;	/* to store vno info */


	/* allocate a new struct and link onto head of list */
	CALLOC(RANGELIST, new_p, 1);
	new_p->next = Vnorange_p;
	Vnorange_p = new_p;

	/* fill in other fields */
	new_p->begin = (short) begin;
	new_p->end = (short) end;
}


/* free list of staff ranges */

void
free_staffrange()

{
	free_rangelist(Staffrange_p);
	Staffrange_p = (struct RANGELIST *) 0;
}



/* free list of vno ranges */

void
free_vnorange()

{
	free_rangelist(Vnorange_p);
	Vnorange_p = (struct RANGELIST *) 0;
}




/* free both the staff and vno lists */

void
free_rlists()

{
	if (Svrangelist_p != (struct SVRANGELIST *) 0) {
		free_sv_list(Svrangelist_p);
		Svrangelist_p = (struct SVRANGELIST *) 0;
	}
	else {
		free_staffrange();
		free_vnorange();
	}
}


/* free the Svrangelist and the RANGELISTs hanging off of it */

void
free_sv_list(svrangelist_p)

struct SVRANGELIST *svrangelist_p;

{
	if (svrangelist_p == (struct SVRANGELIST *) 0) {
		return;
	}

	free_rangelist(svrangelist_p->stafflist_p);
	free_rangelist(svrangelist_p->vnolist_p);

	/* recurse */
	free_sv_list(svrangelist_p->next);
	FREE(svrangelist_p);
}


/* recursively free a list of RANGELIST structs */

static void
free_rangelist(list_p)

struct RANGELIST *list_p;	/* the list to free */

{
	if (list_p == (struct RANGELIST *) 0) {
		return;
	}

	free_rangelist(list_p->next);
	FREE(list_p);
}


/* If doing between, staff ranges must be of the form N&M. If not doing
 * between, must be either a single number or N-M. Make sure this is so.
 */

void
chk_range_type(has_ampersand)

int has_ampersand;	/* YES if range was of the form N&M */

{
	if (has_ampersand == YES && Place != PL_BETWEEN) {
		yyerror("& only valid with 'between'");
		return;
	}

	if (has_ampersand == NO && Place == PL_BETWEEN) {
		yyerror("must use & to specify ranges with 'between'");
	}
}


/* Create a STAFF struct in the main list for every staff.
 * Point List_of_staffs_p to the first of them.
 * Fill in Staffmap_p for each of them to allow quick mapping from staffno
 * to STAFF struct.
 */

void
create_staffs()

{
	struct MAINLL *mll_insert_p;	/* where to insert in main list */
	struct MAINLL *new_p;		/* newly allocated struct */
	struct MAINLL *mll_p;		/* for verifiying proper order */
	struct MAINLL *next_mll_p;	/* next main list item to be checked */
	register int s;			/* index through staffs */


	debug(4, "create_staffs");

	if (List_of_staffs_p != (struct MAINLL *) 0) {
		/* this function has already been called for current measure, so
		 * nothing more to do. This is normal, because this function
		 * is called whenever another function needs to make sure
		 * the STAFFs have been created.
		 */
		return;
	}

	/* STAFFS are supposed to come before LINES, CURVES, and PRHEADS.
	 * However, the user is not constrained to put things in in that
	 * order, so there may be some already on the list. If so, back
	 * up to before where they should begin and insert the STAFFS there. */
	for (mll_insert_p = Mainlltc_p; mll_insert_p != (struct MAINLL *) 0;
					mll_insert_p = mll_insert_p->prev) {

		if (mll_insert_p->str != S_LINE
					&& mll_insert_p->str != S_CURVE
					&& mll_insert_p->str != S_PRHEAD) {
			break;
		}
	}

	/* keep track of place in main list, for later use */
	mll_p = mll_insert_p;

	/* allocate and add a struct for each staff in the range */
	for ( s = 1; s <= Score.staffs; s++) {

		new_p = newMAINLLstruct(S_STAFF, yylineno);
		new_p->u.staff_p->staffno = (short) s;
		insertMAINLL(new_p, mll_insert_p);

		if (List_of_staffs_p == (struct MAINLL *) 0) {
			List_of_staffs_p = new_p;
		}

		Staffmap_p[s] = new_p;
		mll_insert_p = new_p;
	}

	/* while we're making sure the main list in in the prescribed order,
	 * back up all the way to the previous bar (or beginning of list).
	 * If there are any LINES, CURVES, or PRHEADS in between there, move
	 * them to the end. This could happen if, for example, the user
	 * put in a print statement followed by a change of clef */
	while (mll_p != (struct MAINLL *) 0) {
		if (mll_p->str == S_BAR) {
			/* this is far enough to back up */
			break;
		}

		if (mll_p->str == S_LINE || mll_p->str == S_CURVE ||
					mll_p->str == S_PRHEAD) {
			next_mll_p = mll_p->prev;
			unlinkMAINLL(mll_p);
			insertMAINLL(mll_p, mll_insert_p);
			mll_insert_p = mll_p;
			mll_p = next_mll_p;
		}
		else {
			mll_p = mll_p->prev;
		}
	}
}


/* if user specifies staff as "all", need to find the top visible
 * staff (if above) or bottom visible (if below). If not above or below,
 * error. */

void
all()

{
	int s;		/* staff number */


	/* if user didn't specify a place, have to get default value */
	if (Place == PL_UNKNOWN) {
		Place = dflt_place();
	}

	switch(Place) {
	case PL_ABOVE:
		for (s = 1; s <= Score.staffs; s++) {
			if ( (svpath(s, VISIBLE))->visible == YES) {
				save_staff_range(s, ALL);
				return;
			}
		}
		break;

	case PL_BELOW:
		for (s = Score.staffs; s > 0; s--) {
			if ( (svpath(s, VISIBLE))->visible == YES) {
				save_staff_range(s, ALL);
				return;
			}
		}
		break;

	default:
		yyerror("'all' invalid");
		return;
	}

	yyerror("no staffs visible");
}


/* start a new staff-voice list */

void
begin_sv_list()

{
	Svrangelist_p = (struct SVRANGELIST *) 0;
}


/* add the current staff and vno list to the staff-vno list */

void
add_to_sv_list()

{
	struct SVRANGELIST *new_p;
	struct SVRANGELIST **insert_p_p;

	MALLOC(SVRANGELIST, new_p, 1);
	new_p->stafflist_p = Staffrange_p;
	new_p->vnolist_p = Vnorange_p;
	new_p->next = (struct SVRANGELIST *) 0;

	/* link onto end of list */
	for (insert_p_p = & Svrangelist_p;
				*insert_p_p != (struct SVRANGELIST *) 0;
				insert_p_p = & ((*insert_p_p)->next) ) {
		;
	}
	*insert_p_p = new_p;
}


/* return YES if given staff is a tab staff, NO if not */

int
is_tab_staff(staffno)

int staffno;

{
	if (staffno < 1 || staffno > MAXSTAFFS) {
		/* not a staff, so not a tab staff. */
		return(NO);
	}
	return (svpath(staffno, STAFFLINES)->strinfo == 0 ? NO : YES);
}


/* Returns the staff number of the first staff on the current list of staffs.
 * In this context, the "first staff" means the first staff the user defined,
 * so if they said something like 5,6,9-12,2 this would return 5.
 * It also returns, via pointer, the place (PL_* value) being defined. */

int
leadstaff(place_p)

int *place_p;

{
	struct RANGELIST *r_p;

	if (Staffrange_p == (struct RANGELIST *) 0) {
		pfatal("leadstaff called when no staffs on list");
	}

	/* since new ranges are linked onto the head of the list, we need
	 * to find the last range on the list. That will be the first one
	 * the user specified. */
	for (r_p = Staffrange_p; r_p->next != (struct RANGELIST *) 0;
						r_p = r_p->next) {
		;
	}
	*place_p = r_p->place;
	return(r_p->begin);
}
