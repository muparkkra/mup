/*
 Copyright (c) 1995-2019  by Arkkra Enterprises.
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

/* This file contains parse-time functions related to TIMEDSSVs.
 * These are used to specify mid-measure parameter changes,
 * like change of clef.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/* Mid-measure SSV's eventually get attached to the BAR, but we need
 * to point to the list temporarily while the BAR doesn't exist yet. */
static struct TIMEDSSV *Timedssv_p;

/* When there is more than one TIMEDSSV at the same moment in time,
 * we use user input order, so we add items to the end of the list.
 * This keeps track of where to put the next one.
 */
static struct TIMEDSSV **Timed_tail_p_p = &Timedssv_p;

static struct TIMEDSSV *tssv_new P((UINT32B context));


/* Allocate and initalize a TIMEDSSV for each staff/voice being defined,
 *  and return pointer to the first one.
 */

struct TIMEDSSV *
tssv_create(context)

UINT32B context;

{
	struct SVRANGELIST *svr_p;	/* list of staffs/voices being entered */
	struct RANGELIST *srange_p;	/* list of staffs being entered */
	struct RANGELIST *vrange_p;	/* list of voices being entered */
	struct TIMEDSSV *new_p;
	struct TIMEDSSV *first_p = 0;	/* first one created */
	int s;				/* staff number */
	int v;				/* voice number */

	/* Create as many new TIMEDSSVs as needed, based on the current list
	 * of staffs/voices being defined,
	 * link them onto the temporary list for the current measure.
	 */

	switch (context) {
	case C_SCORE:
		first_p = new_p = tssv_new(context);
		break;
	case C_STAFF:
	case C_VOICE:
		for (svr_p = Svrangelist_p; svr_p != 0; svr_p = svr_p->next) {
			for (srange_p = svr_p->stafflist_p; srange_p != 0;
						srange_p = srange_p->next) {
				for (s = srange_p->begin; s <= srange_p->end; s++) {
					if (context == C_STAFF) {
						new_p = tssv_new(context);
						if (first_p == 0) {
							first_p = new_p;
						}
						new_p->ssv.staffno = s;
					}
					else {
						for (vrange_p = svr_p->vnolist_p;
								vrange_p != 0;
								vrange_p = vrange_p->next) {
							for (v = vrange_p->begin;
								v <= vrange_p->end;
								v++) {
							   new_p = tssv_new(context);
							   if (first_p == 0) {
								first_p = new_p;
							   }
							   new_p->ssv.staffno = s;
							   new_p->ssv.voiceno = v;
							}
						}
					}
				}
			}
		}
		break;
	default:
		pfatal("invalid context %d when creating TIMEDSSV", context);
		/*NOTREACHED*/
		break;
	}
	return(first_p);
}


/* Create a single TIMEDSSV and link it onto the list */

static struct TIMEDSSV *
tssv_new(context)

UINT32B context;

{
	struct TIMEDSSV *curr_tssv_p;

	CALLOC(TIMEDSSV, curr_tssv_p, 1);
	zapssv( & (curr_tssv_p->ssv) );
	curr_tssv_p->ssv.context = context;
	curr_tssv_p->grpsyl_p = 0;
	curr_tssv_p->time_off.n = -1;
	curr_tssv_p->time_off.d = 1;

	curr_tssv_p->next = 0;
	*Timed_tail_p_p = curr_tssv_p;
	Timed_tail_p_p = &(curr_tssv_p->next);

	return(curr_tssv_p);
}


/* Save a parameter setting in the given TIMEDSSV. We only support a very
 * limited list of parameters that can be changed mid-measure,
 * so this checks for valid ones.
 */

void
tssv_update(timedssv_p, param, value)

struct TIMEDSSV *timedssv_p;
int param;
int value;

{
	int have_non_v3 = NO;	/* YES if have at least one non voice 3 */

	/* Could be multiple staffs/voices, so do them all */
	for (  ; timedssv_p != 0; timedssv_p = timedssv_p->next) {
		switch (param) {
		case CLEF:
			timedssv_p->ssv.clef = value;
			break;
		case RELEASE:
			if (rangecheck(value, MINRELEASE, MAXRELEASE,
					"mid-measure release change") == YES) {
				timedssv_p->ssv.release = value;
			}
			break;
		case DEFOCT:
			if (rangecheck(value, MINOCTAVE, MAXOCTAVE,
					"mid-measure defoct change") == YES) {
				timedssv_p->ssv.defoct = value;
			}
			break;

		case ALIGNRESTS:
			timedssv_p->ssv.alignrests = value;
			if (timedssv_p->ssv.voiceno != 3) {
				have_non_v3 = YES;
			}
			break;

		default:
			yyerror("only alignrests, clef, defoct, and release parameters can be changed mid-measure");
			return;
		}
		if (timedssv_p->ssv.used[param] == YES) {
			l_warning(Curr_filename, yylineno,
				"multiple changes of the same parameter; last used");
		}
		timedssv_p->ssv.used[param] = YES;
	}

	if (param == ALIGNRESTS && have_non_v3 == NO) {
		l_warning(Curr_filename, yylineno,
				"alignrests not used on voice 3; ignoring");
	}
}


/* Associate grpsyl with TIMEDSSV. This should be called at the end of
 * parsing of a grpsyl, in case it has at least one timed ssv. */

void
tssv_setgrpsyl(gs_p)

struct GRPSYL *gs_p;

{
	struct TIMEDSSV *tssv_p;

	/* User could input multiple << >> things, and each gets their
	 * own TIMEDSSV, so we need to associate this grpsyl with
	 * all that don't yet have one.  */
	for (tssv_p = Timedssv_p; tssv_p != 0; tssv_p = tssv_p->next) {
		if (tssv_p->grpsyl_p == 0) {
			tssv_p->grpsyl_p = gs_p;

			/* Do some error checks */
			if (tssv_p->ssv.used[CLEF] == YES) {
				if (tssv_p->ssv.context == C_STAFF
						&& is_tab_staff(tssv_p->ssv.staffno)) {
					yyerror("can't change clef of tab staff");
				}
				if (tssv_p->ssv.context == C_VOICE) {
					yyerror("can't change clef in voice context");
				}
			}
		}
	}
}



/* Do processing on one input line worth of TIMEDSSVs.
 */

void
tssv_line()
{
	struct TIMEDSSV *ts_p;
	struct GRPSYL *gs_p;

	/* First we have to find the time offsets of each TIMEDSSV.
	 * We can't necessarily calculate them at the time
	 * they were added to the list, since for tuplets
	 * we don't know fulltimes till we reach the end
	 * of the tuplet, and know how to adjust.
	 * So we just save the GRPSYL* at that point,
	 * and now we go through and find all the actual times.
	 */
	for (ts_p = Timedssv_p; ts_p != 0; ts_p = ts_p->next) {
		if (GE(ts_p->time_off, Zero)) {
			/* already set from some previous line */
			continue;
		}

		if (ts_p->grpsyl_p == 0) {
			/* This could happen if there was a user input
			 * error, because we could have set up the TIMEDSSV
			 * and then not been able to parse the GRPSYL that
			 * was intended to go with it.  Set time offset to
			 * a safe value, and skip the rest of the loop,
			 * so we don't try to dereference the null ptr. */
			ts_p->time_off = Zero;
			continue;
		}

		/* Count up the time before the group where the timed
		 * SSV was specified. */
		for (ts_p->time_off = Zero, gs_p = ts_p->grpsyl_p->prev;
						gs_p != 0; gs_p = gs_p->prev) {
			/* Alt groups have not yet had their time adjusted,
			 * so we have to compensate for that. */
			if (gs_p->slash_alt < 0 || (gs_p->prev != 0
					&& gs_p->prev->slash_alt < 0) ) {
				ts_p->time_off = radd(ts_p->time_off,
						rdiv(gs_p->fulltime, Two));
			}
			else if (gs_p->grpvalue != GV_ZERO) {
				ts_p->time_off = radd(ts_p->time_off,
							gs_p->fulltime);
			}
		}
	}
}


/* Sort the current TIMEDSSV list by time and return pointer to the head
 * of the sorted list. The sorting is done by time. When there is a tie
 * things are put in user input order. Also re-inits for the next measure.
 */

struct TIMEDSSV *
tssv_sort()

{
	struct TIMEDSSV *ret;	/* return value is pointer to sorted list */
	short moved;		/* YES if something was moved during sort */

	/* Most of the time, the list will be empty. */
	if (Timedssv_p == 0) {
		return(0);
	}

	/* Sort in time order.
	 * The list is almost certain to be very short,
	 * so sort needn't be very efficient. So we check pairs
	 * and swap ones that are backwards. */
	do {
		struct TIMEDSSV **ts_p_p;
		struct TIMEDSSV *tmp_ts_p;

		moved = NO;
		for (ts_p_p = &Timedssv_p; (*ts_p_p)->next != 0;
						ts_p_p = &((*ts_p_p)->next) ) {
			if ( GT( (*ts_p_p)->time_off, (*ts_p_p)->next->time_off ) ) {
				/* Wrong order. Swap them */
				tmp_ts_p = (*ts_p_p)->next;
				(*ts_p_p)->next = (*ts_p_p)->next->next;
				tmp_ts_p->next = *ts_p_p;
				*ts_p_p = tmp_ts_p;
				moved = YES;
				break;
			}
		}
	} while (moved == YES);
	ret = Timedssv_p;

	/* re-init for next measure */
	Timedssv_p = 0;
	Timed_tail_p_p = &(Timedssv_p);
	return(ret);
}
