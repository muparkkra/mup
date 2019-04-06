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
/*
 * Name:	stuff.c
 *
 * Description:	This file contains functions for handling "stuff".
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/* for STUFF, apply appropriate staffscale, based on "all" */
#define STUFFSTEP(stuff_p)	((stuff_p)->all == YES ?	\
				Score.staffscale * STEPSIZE : stepsize);

static void normalizestuff P((void));
static void movestuff P((struct STAFF *staff1_p, struct STAFF *staff2_p,
		struct STUFF *stuff_p));
static void setphrases P((void));
static void phrasestuff P((struct MAINLL *msbeg_p, struct STUFF *stuff_p));
static void set1phrase P((struct MAINLL *msbeg_p, struct STUFF *stuff_p,
		int vidx, int both));
static void invalid_phrase P((struct STUFF *stuff_p, char *end, int both));
static void rmstuff P((struct STAFF *staff_p, struct STUFF *stuff_p));
static void breakstuff P((void));
static struct STUFF *prevstuff P((struct MAINLL *mainll_p,
		struct STUFF *stuff_p));
static void breakone P((struct MAINLL *m2_p, struct STUFF *stuff_p,
		int timenum, char *origstr_p, int depth));
static void contpedal P((void));
static void setstuff P((void));
static double count2coord P((double count, struct BAR *bar_p,
		struct CHHEAD *chhead_p, int timeden));
static void setstuffcoord P((struct STUFF *stuff_p, int coordidx,
		struct MAINLL *mainll_p, struct MAINLL *m2_p, struct BAR *bar_p,
		struct CHHEAD *chhead_p, int timeden, int vscheme, double count,
		double steps, int gracebackup, double stepsize));
static int geteast P((struct STUFF *stuff_p, struct MAINLL **m2_p_p,
		short *timeden2_p, struct CHHEAD **chhead2_p_p,
		struct BAR **bar2_p_p, int *vscheme2_p));
static int setmrferm P((struct STAFF *staff_p, struct STUFF *stuff_p));
static int trygrid P((struct MAINLL *mainll_p, struct STUFF *stuff_p));
static void tieslurstuff P((void));
static void mktieslurstuff P((struct MAINLL *mllstaff_p, struct GRPSYL *gs_p,
		int n, int s));
static void mkextrastuff P((struct MAINLL *mll_p, struct STUFF *origstuff_p,
		struct GRPSYL *gs_p, int n, int s, int stufftype,
		int other_vno));

/*
 * Name:        stuff()
 *
 * Abstract:    Perform all necessary horizontal operations on STUFF.
 *
 * Returns:     void
 *
 * Description: This function calls subroutines to do all necessary horizontal
 *		operations on STUFF.
 */

void
stuff()
{
	debug(16, "stuff");
	normalizestuff();
	setphrases();
	breakstuff();
	contpedal();
	setstuff();
	tieslurstuff();
}

/*
 * Name:        normalizestuff()
 *
 * Abstract:	Normalize STUFF starting at count N + 1 before a FEED.
 *
 * Returns:     void
 *
 * Description: This function looks for all STUFF structures that start at
 *		count N + 1 (where N is the numerator of the time signature)
 *		in measures preceding a score feed.  These things really
 *		should be drawn at the start of the next score, so this
 *		function moves them to be at count 0 of the next measure.
 *		However, if the staff involved is invisible on the next score,
 *		it doesn't move the STUFF.  Also, fermatas and the pedal marks
 *		PEDAL and ENDPED are not moved.
 *		Only start.count is used; the policy is to apply start.steps
 *		only after everything else is done.
 */

static void
normalizestuff()
{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct MAINLL *m2_p, *m3_p;	/* secondary & tertiary MLL pointers */
	struct STUFF *stuff_p;		/* point at a STUFF structure */
	struct STUFF *next_p;		/* point at the next STUFF structure */


	debug(16, "normalizestuff");
	initstructs();

	/*
	 * Loop through the main linked list, looking for BARs that immediately
	 * precede a FEED.  When found, adjust the start count of STUFFs in the
	 * preceding measure if necessary.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			/* keep SSVs up to date; we need to know time sig */
			asgnssv(mainll_p->u.ssv_p);
			continue;

		case S_BAR:
			/* break out to handle this BAR */
			break;

		default:
			/* nothing to do, skip to next loop */
			continue;
		}

		/*
		 * Find out if this is a bar that immediately precedes a FEED.
		 * If not, there is no need to adjust any stuff, so continue.
		 */
		for (m2_p = mainll_p; m2_p != 0 && m2_p->str != S_FEED &&
				m2_p->str != S_CHHEAD; m2_p = m2_p->next)
			;
		if (m2_p == 0 || m2_p->str == S_CHHEAD)
			continue;

		/*
		 * This bar immediately precedes a FEED.  Make sure the number
		 * of staffs doesn't change across this FEED.  (And if what
		 * follows are blocks, continue looking until we find a FEED
		 * that is not a block.)  If it does, don't move any STUFF.
		 */
		for (m2_p = mainll_p; m2_p->next != 0; m2_p = m2_p->next) {
			/* break out if a FEED that is not followed by block */
			if (m2_p->str == S_FEED &&
					m2_p->next->str != S_BLOCKHEAD)
				break;
			/* break out if SSV changes number of staffs */
			if (m2_p->str == S_SSV && 
					m2_p->u.ssv_p->used[NUMSTAFF] == YES)
				break;
		}
		/* if at end of list or staffs changed, don't move stuff */
		if (m2_p->next == 0 || m2_p->str == S_SSV)
			continue;

		/*
		 * Search back to the CHHEAD of the preceding measure.
		 */
		for (m2_p = mainll_p; m2_p->str != S_CHHEAD; m2_p = m2_p->prev)
			;

		/*
		 * Loop through all the staffs in this preceding measure,
		 * adjusting STUFF when need be.
		 */
		for (m2_p = m2_p->next; m2_p->str == S_STAFF;
				m2_p = m2_p->next) {

			if (m2_p->u.staff_p->visible == NO)
				continue;

			/*
			 * Find the matching STAFF in the next measure.  If we
			 * can't find it (like end of MLL) or it's not visible,
			 * forget this staff.
			 */
			for (m3_p = mainll_p->next; m3_p != 0 &&
					m3_p->str != S_BAR; m3_p = m3_p->next) {
				if (m3_p->str == S_STAFF &&
						m3_p->u.staff_p->staffno ==
						m2_p->u.staff_p->staffno)
					break;
			}
			if (m3_p == 0 || m3_p->str != S_STAFF ||
					m3_p->u.staff_p->visible == NO)
				continue;

			/*
			 * Loop through all the stuff on this staff in the
			 * preceding measure, normalizing it.  That is, if it
			 * starts at count N + 1 (where N is the numerator of
			 * the time signature), change it to start at count 0
			 * of the next bar line.  However, don't move fermatas
			 * or pedal "bounce" or "ending" marks.
			 */
			for (stuff_p = m2_p->u.staff_p->stuff_p;
					stuff_p != 0; stuff_p = next_p) {
				/*
				 * Remember next one in case we have to move
				 * this one to the next measure's linked list.
				 */
				next_p = stuff_p->next;

				if (stuff_p->start.count == Score.timenum + 1 &&
				string_is_sym(stuff_p->string, C_PEDAL, FONT_MUSIC) == NO &&
				string_is_sym(stuff_p->string, C_ENDPED, FONT_MUSIC) == NO &&
				string_is_sym(stuff_p->string, C_FERM, FONT_MUSIC) == NO &&
				string_is_sym(stuff_p->string, C_UFERM, FONT_MUSIC) == NO) {
					/*
					 * Move this stuff from preceding to
					 * following measure's linked list.
					 */
					movestuff(m2_p->u.staff_p,
						  m3_p->u.staff_p, stuff_p);
					/*
					 * Set start count to 0.  If there is a
					 * "til" clause, bars would have to be
					 * greater than 0, since count can't
					 * refer to a count in this measure
					 * (can't be greater than N + 1).  So
					 * we only need to check bars to see if
					 * there's a "til" clause.  If there
					 * is, decrement it.
					 */
					stuff_p->start.count = 0;
					if (stuff_p->end.bars > 0)
						stuff_p->end.bars--;
				}
			}
		}
	}
}

/*
 * Name:        movestuff()
 *
 * Abstract:    Move a STUFF from one linked list to another.
 *
 * Returns:     void
 *
 * Description: This function, given two staff pointers, finds the given STUFF
 *		in the first one's linked list of STUFF.  It removes it from
 *		there and adds it to the start of the second staff's list.
 */

static void
movestuff(staff1_p, staff2_p, stuff_p)

struct STAFF *staff1_p;		/* first STAFF */
struct STAFF *staff2_p;		/* second STAFF */
struct STUFF *stuff_p;		/* the STUFF to be moved */

{
	struct STUFF *s_p;	/* loop pointer */


	debug(32, "movestuff file=%s line=%d", stuff_p->inputfile,
			stuff_p->inputlineno);
	if (staff1_p->stuff_p == stuff_p) {
		/*
		 * This STUFF is the first one in the first STAFF's linked
		 * list.  Change the headcell to point at the next one (which
		 * removes it from this list).  This "next one" could be null.
		 */
		staff1_p->stuff_p = stuff_p->next;
	} else {
		/*
		 * Find which STUFF in the first STAFF's list points at the one
		 * we want to move.  Make it point at the following one.
		 */
		for (s_p = staff1_p->stuff_p; s_p->next != stuff_p;
					s_p = s_p->next)
			;
		s_p->next = stuff_p->next;
	}

	/*
	 * Make the STUFF we are moving point at what used to be the first
	 * one in the second STAFF's linked list.  Make the second STAFF's
	 * headcell point at the STUFF we are moving.
	 */
	stuff_p->next = staff2_p->stuff_p;
	staff2_p->stuff_p = stuff_p;
}

/*
 * Name:        setphrases()
 *
 * Abstract:    Find endpoints and direction of all phrase marks.
 *
 * Returns:     void
 *
 * Description: This function loops through the main linked list and all STUFF,
 *		looking for phrase marks.  For each phrase mark, it calls
 *		phrasestuff() to find out the horizontal positioning of the
 *		endpoints and whether the phrase mark should be above or below.
 */

static void
setphrases()
{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct STUFF *stuff_p;		/* point at a STUFF structure */
	struct STUFF **phrasearray;	/* malloc array of pointers to stuff */
	int numphrases;			/* number of phrases in linked list */
	int n;				/* loop through phrasearray */


	debug(16, "setphrases");
	initstructs();

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			/* keep SSVs up to date; we need to know time sig */
			asgnssv(mainll_p->u.ssv_p);
			continue;

		case S_STAFF:
			/* break out to handle this STAFF */
			break;

		default:
			/* nothing to do, skip to next loop */
			continue;
		}

		/*
		 * For each phrase in this staff's stuff list, we need to
		 * determine whether it should print 0, 1, or 2 phrase marks,
		 * and the direction and endpoints of each.  We can't simply
		 * loop through the linked list, because the list gets altered
		 * by the subroutines we call, which can add and delete phrases
		 * from the list.  So we copy the pointers to an array first.
		 */
		/* count how many phrases are in this staff's stuff list */
		numphrases = 0;
		for (stuff_p = mainll_p->u.staff_p->stuff_p; stuff_p != 0;
					stuff_p = stuff_p->next) {
			if (stuff_p->stuff_type == ST_PHRASE)
				numphrases++;
		}

		/* if no phrases, nothing to do */
		if (numphrases == 0)
			continue;

		/* allocate an array to hold pointer(s) to the phrase(s) */
		MALLOCA(struct STUFF *, phrasearray, numphrases);

		/* fill the array with pointers to each phrase */
		n = 0;
		for (stuff_p = mainll_p->u.staff_p->stuff_p; stuff_p != 0;
					stuff_p = stuff_p->next) {
			if (stuff_p->stuff_type == ST_PHRASE)
				phrasearray[n++] = stuff_p;
		}

		/* find endpoints and direction of phrase(s) from each stuff */
		for (n = 0; n < numphrases; n++) {
			phrasestuff(mainll_p, phrasearray[n]);
		}

		FREE(phrasearray);
	}
}

/*
 * Name:        phrasestuff()
 *
 * Abstract:    Find endpoints and direction of phrase(s) from one STUFF.
 *
 * Returns:     void
 *
 * Description: This function decides whether the given phrase STUFF should
 *		try to generate one or two phrase marks above, below, or both.
 *		If we already know the voice, it'll be just one.  Else we have
 *		to figure out whether one or two, & what voice(s) to attach to.
 *		If it's both, it inserts a new STUFF in the list for it.  Then
 *		it calls set1phrase() for each desired phrase to find endpoints
 *		and set the place if it's unknown.
 */

static void
phrasestuff(msbeg_p, stuff_p)

struct MAINLL *msbeg_p;	/* point at main linked list where phrase starts */
struct STUFF *stuff_p;	/* point at STUFF structure for start of phrase */

{
	struct STUFF *stuff2_p;		/* if we need to create a 2nd phrase */


	debug(32, "phrasestuff file=%s line=%d", stuff_p->inputfile,
			stuff_p->inputlineno);

	/*
	 * If the parse phase knows what voice the phrase is for, it sets vno.
	 * In that case we just call set1phrase with vidx = vno - 1.
	 */
	if (stuff_p->vno != 0) {
		/*
		 * If this voice is invisible, remove the phrase.  That can
		 * happen when the parse phase chooses the voice, because of
		 * the "visible" parameter or "-s" command line option.
		 */
		if (vvpath(msbeg_p->u.staff_p->staffno, stuff_p->vno, VISIBLE)
				->visible == NO) {
			rmstuff(msbeg_p->u.staff_p, stuff_p);
			return;
		}

		/*
		 * Typically, voice 3 is treated like vscheme V_1.  But if one
		 * of the other voices is all space, and the other isn't, and
		 * place is unknown, we'll force place to be away from the
		 * non-space voice.
		 */
		if (stuff_p->vno == 3 && stuff_p->place == PL_UNKNOWN) {
			int v1_space, v2_space;	/* is v1 or v2 space? */

			v1_space = chkallspace(msbeg_p, stuff_p, 0);
			v2_space = chkallspace(msbeg_p, stuff_p, 1);

			if (v1_space && ! v2_space) {
				stuff_p->place = PL_ABOVE;
			} else if (v2_space && ! v1_space) {
				stuff_p->place = PL_BELOW;
			}
		}
		set1phrase(msbeg_p, stuff_p, stuff_p->vno - 1, NO);
		return;
	}

	/*
	 * The parse phase doesn't know the voice.  So we have to figure out
	 * the appropriate voice, possibly even making two phrases.  It will
	 * always be voice 1 and/or 2 here.  To attach to voice 3, the user
	 * has to specify that in the input, and it would be handled above.
	 */
	switch (svpath(msbeg_p->u.staff_p->staffno, VSCHEME)->vscheme) {
	case V_1:		/* one voice */
		/* do phrase mark for first (only) voice if possible */
		set1phrase(msbeg_p, stuff_p, 0, NO);
		break;

	case V_2FREESTEM:	/* two voices that are free if one is space */
	case V_3FREESTEM:	/* same as above; don't attach to voice 3 */
		if (chkallspace(msbeg_p, stuff_p, 0) == YES) {
			/* first voice all spaces, apply phrase to second */
			set1phrase(msbeg_p, stuff_p, 1, NO);
			break;
		} else if (chkallspace(msbeg_p, stuff_p, 1) == YES) {
			/* second voice all spaces, apply phrase to first */
			set1phrase(msbeg_p, stuff_p, 0, NO);
			break;
		}
		/* FALL THROUGH to handle like V_2OPSTEM */

	case V_2OPSTEM:		/* two voices that always oppose */
	case V_3OPSTEM:		/* same as above; don't attach to voice 3 */
		switch (stuff_p->place) {

		case PL_ABOVE:
			/* phrase requested only for top voice */
			set1phrase(msbeg_p, stuff_p, 0, NO);
			break;

		case PL_BELOW:
			/* phrase requested only for bottom voice */
			set1phrase(msbeg_p, stuff_p, 1, NO);
			break;

		default:
			/*
			 * We're going to have two phrase marks, one above the
			 * first voice and one below the second voice.  Let the
			 * current STUFF be for the top one, and create a new
			 * STUFF for the bottom one.
			 */
			stuff_p->place = PL_ABOVE;
			stuff2_p = newSTUFF((char *)0,
					stuff_p->dist,
					stuff_p->dist_usage,
					stuff_p->aligntag,
					stuff_p->start.count,
					stuff_p->start.steps,
					stuff_p->start.gracebackup,
					stuff_p->end.bars, stuff_p->end.count,
					stuff_p->end.steps,
					stuff_p->end.gracebackup,
					ST_PHRASE, stuff_p->modifier, PL_BELOW,
					stuff_p->inputfile,
					stuff_p->inputlineno);
			stuff2_p->next = stuff_p->next;
			stuff_p->next = stuff2_p;

			set1phrase(msbeg_p, stuff_p, 0, YES);
			set1phrase(msbeg_p, stuff2_p, 1, YES);
			break;
		}
		break;
	}
}

/*
 * Name:        set1phrase()
 *
 * Abstract:    Find endpoints and direction of one phrase.
 *
 * Returns:     void
 *
 * Description: This function, given a STUFF for a single phrase mark and the
 *		voice it should apply to, finds the endpoint GRPSYLs for the
 *		phrase.  If it can't find valid GRPSYLs, it removes the phrase
 *		and prints a warning.  Otherwise, it also decides whether the
 *		phrase should be above or below, if that is not already known.
 */

static void
set1phrase(msbeg_p, stuff_p, vidx, both)

struct MAINLL *msbeg_p;	/* point at MLL (staff) where phrase begins */
struct STUFF *stuff_p;	/* point at STUFF structure for start of phrase */
int vidx;		/* which voice to attach the phrase to */
int both;		/* will there be a phrase both above and below? */
			/* (We're only drawing one now; this flag is only for
			 * providing better warning messages.) */

{
	struct MAINLL *msend_p;	/* MLL (staff) where phrase ends */
	struct MAINLL *mll_p;	/* for looping through MLL */
	struct GRPSYL *beggrp_p;/* (eventually is) beginning of phrase */
	struct GRPSYL *endgrp_p;/* (eventually is) end of phrase */
	struct GRPSYL *gs_p;	/* for looping through GRPSYL lists */
	struct GRPSYL *ngs_p;	/* next GRPSYL */
	int timeden;		/* denominator of time sig at end of phrase */
	int up, down;		/* count stem directions */
	int upgrace, downgrace;	/* count stem directions for grace */
	int n;			/* loop variable */


	debug(32, "set1phrase file=%s line=%d vidx=%d", stuff_p->inputfile,
			stuff_p->inputlineno, vidx);

	stuff_p->vno = vidx + 1;	/* find voice number from voice idx */

	/*
	 * Find what measure this phrase mark ends in.  Along the way, keep
	 * track of the time signature denominator, in case it changes.  If
	 * getendstuff() returns 0, it means the phrase runs into a multirest,
	 * which is not allowed.  If the phrase is supposed to be attached to
	 * the bottom voice, but it ends in a measure where the vscheme is 1,
	 * we must also throw it away.  (This condition is signaled by the
	 * groups_p[v] being a null pointer.)
	 */
	msend_p = getendstuff(msbeg_p, stuff_p, &timeden);

	if (msend_p == 0) {
		/*
		 * This scenario can legitimately happen, because of
		 * restcombine forming a multirest when our staff becomes
		 * invisible, so don't warn if we are invisible.
		 */
		if (msbeg_p->u.staff_p->visible == YES) {
			l_warning(stuff_p->inputfile, stuff_p->inputlineno,
			"removing invalid phrase (runs into multirest)");
		}
		rmstuff(msbeg_p->u.staff_p, stuff_p);
		return;
	}
	if (msend_p->u.staff_p->groups_p[vidx] == 0) {
		invalid_phrase(stuff_p, "last", both);
		rmstuff(msbeg_p->u.staff_p, stuff_p);
		return;
	}

	/*
	 * Find the GRPSYLs that are closest, timewise, to the requested
	 * beginning and ending times of the phrase mark.
	 */
	beggrp_p = closestgroup(stuff_p->start.count,
			msbeg_p->u.staff_p->groups_p[vidx], Score.timeden);
	endgrp_p = closestgroup(stuff_p->end.count,
			msend_p->u.staff_p->groups_p[vidx], timeden);

	/*
	 * It's possible that *beggrp_p is a rest or a space, but a phrase must
	 * start on a note group.  So search forward, if necessary, to the first
	 * note group after this point.  If there is none in this measure, this
	 * is an illegal phrase mark.  Also, if the phrase begins and ends in
	 * the same measure, check that we don't go past the end in doing this
	 * search.  A phrase is not allowed to begin and end at the same group.
	 * But the two groups can be the same if gracebackup is being used,
	 * and the start is backing up more than the end (if the end is backing
	 * up at all), because in that case the beginning and end groups are
	 * really different groups.  In any case, apply gracebackup if
	 * requested.
	 */
	while (beggrp_p != 0 &&
			(beggrp_p->grpcont != GC_NOTES || beggrp_p->is_meas)) {
		if (beggrp_p == endgrp_p)
			break;
		beggrp_p = nextnongrace(beggrp_p);
	}
	if (beggrp_p == 0) {
		invalid_phrase(stuff_p, "first", both);

		rmstuff(msbeg_p->u.staff_p, stuff_p);
		return;
	}
	if (beggrp_p == endgrp_p &&
		      stuff_p->start.gracebackup <= stuff_p->end.gracebackup) {
		if (stuff_p->start.gracebackup == stuff_p->end.gracebackup) {
			l_warning(stuff_p->inputfile, stuff_p->inputlineno,
			"removing invalid phrase (need multiple notes)");
		} else {
			l_warning(stuff_p->inputfile, stuff_p->inputlineno,
			"removing invalid phrase (from a note to an earlier grace note)");
		}

		rmstuff(msbeg_p->u.staff_p, stuff_p);
		return;
	}
	/* found a valid starting point; back up grace notes if requested */
	for (n = 0; n < stuff_p->start.gracebackup; n++) {
		beggrp_p = beggrp_p->prev;
		if (beggrp_p == 0 || beggrp_p->grpvalue != GV_ZERO) {
			l_ufatal(stuff_p->inputfile, stuff_p->inputlineno,
			"not enough grace groups to back up to for start of phrase");
		}
	}

	/*
	 * Do the equivalent thing with the end of the phrase mark.
	 */
	while (endgrp_p != 0 &&
			(endgrp_p->grpcont != GC_NOTES || endgrp_p->is_meas) &&
			beggrp_p != endgrp_p) {
		endgrp_p = prevnongrace(endgrp_p);
	}
	if (beggrp_p == endgrp_p &&
		      stuff_p->start.gracebackup <= stuff_p->end.gracebackup) {
		if (stuff_p->start.gracebackup == stuff_p->end.gracebackup) {
			l_warning(stuff_p->inputfile, stuff_p->inputlineno,
			"removing invalid phrase (need multiple notes)");
		} else {
			l_warning(stuff_p->inputfile, stuff_p->inputlineno,
			"removing invalid phrase (from a note to an earlier grace note)");
		}

		rmstuff(msbeg_p->u.staff_p, stuff_p);
		return;
	}
	if (endgrp_p == 0) {
		invalid_phrase(stuff_p, "last", both);

		rmstuff(msbeg_p->u.staff_p, stuff_p);
		return;
	}
	/* found a valid ending point; back up grace notes if requested */
	for (n = 0; n < stuff_p->end.gracebackup; n++) {
		endgrp_p = endgrp_p->prev;
		if (endgrp_p == 0 || endgrp_p->grpvalue != GV_ZERO) {
			l_ufatal(stuff_p->inputfile, stuff_p->inputlineno,
			"not enough grace groups to back up to for end of phrase");
		}
	}

	/*
	 * We have now determined the correct beginning and ending GRPSYLs for
	 * the phrase.  Store them for later use.
	 */
	stuff_p->beggrp_p = beggrp_p;
	stuff_p->endgrp_p = endgrp_p;

	/*
	 * If we don't know yet whether the phrase should be drawn above or
	 * below the staff, decide that now.  We will put it on the side that
	 * has more note heads than stems, usually based on nongraces but in
	 * case of a tie considering graces, etc.
	 */
	if (stuff_p->place == PL_UNKNOWN) {
		up = down = upgrace = downgrace = 0;

		/* find GRPSYL after the last note (could be null) */
		mll_p = msend_p;
		ngs_p = nextgrpsyl(endgrp_p, &mll_p);

		/* loop from first GRPSYL through last */
		mll_p = msbeg_p;
		for (gs_p = beggrp_p; gs_p != 0 && gs_p != ngs_p;
					gs_p = nextgrpsyl(gs_p, &mll_p)) {

			/* we only care about notes */
			if (gs_p->grpcont != GC_NOTES) {
				continue;
			}

			/* count nongraces and graces separately */
			if (gs_p->grpvalue == GV_NORMAL) {
				if (gs_p->stemdir == UP) {
					up++;
				} else {
					down++;
				}
			} else {	/* GV_ZERO */
				if (gs_p->stemdir == UP) {
					upgrace++;
				} else {
					downgrace++;
				}
			}
		}

		/*
		 * Put phrase opposite the majority of the stems.  First check
		 * the nongraces.  If their up and down are equal, check the
		 * graces.
		 */
		if (up > down) {
			stuff_p->place = PL_BELOW;
		} else if (up < down) {
			stuff_p->place = PL_ABOVE;
		} else if (upgrace > downgrace) {
			stuff_p->place = PL_BELOW;
		} else if (upgrace < downgrace) {
			stuff_p->place = PL_ABOVE;
		} else {
			/*
			 * Nongraces and graces were both tied.  If there are
			 * any nongraces, find the first one.  Otherwise find
			 * the first grace.  Base the answer on that.
			 */
			gs_p = beggrp_p;
			if (up > 0) {
				if (gs_p->grpvalue == GV_ZERO) {
					gs_p = nextnongrace(gs_p);
				}
			}
			if (gs_p->stemdir == UP) {
				stuff_p->place = PL_BELOW;
			} else {
				stuff_p->place = PL_ABOVE;
			}
		}
	}
}

/*
 * Name:        invalid_phrase()
 *
 * Abstract:    Output an appropriate warning for a bad phrase mark.
 *
 * Returns:     void
 *
 * Description: This function prints a warning and returns.
 */

static void
invalid_phrase(stuff_p, end, both)

struct STUFF *stuff_p;
char *end;
int both;

{
	char buf[256];
	int n;

	n = sprintf(buf, "removing invalid phrase (covers no notes in %s "
			"measure of voice %d", end, stuff_p->vno);
	if (both) {
		sprintf(&buf[n], "; maybe you meant to use \"%s\" to apply "
				"phrase to voice %d only?)",
				stuff_p->vno == 1 ? "below" : "above",
				3 - stuff_p->vno);
	} else {
		sprintf(&buf[n], ")");
	}

	l_warning(stuff_p->inputfile, stuff_p->inputlineno, "%s", buf);
}

/*
 * Name:        rmstuff()
 *
 * Abstract:    Remove a STUFF from a linked list, and free it.
 *
 * Returns:     void
 *
 * Description: This function removes the given STUFF from the linked list
 *		hanging off the given staff, and frees it.
 */

static void
rmstuff(staff_p, stuff_p)

struct STAFF *staff_p;		/* the staff that the stuff hangs off of */
struct STUFF *stuff_p;		/* the stuff to be removed */

{
	struct STUFF *stuff2_p;		/* point along STUFF list */


	debug(32, "rmstuff file=%s line=%d", stuff_p->inputfile,
			stuff_p->inputlineno);
	if (staff_p->stuff_p == stuff_p) {
		/* the given stuff is the first one in the list */
		staff_p->stuff_p = stuff_p->next;
	} else {
		/* find which stuff in list points at the one to be removed */
		for (stuff2_p = staff_p->stuff_p; stuff2_p->next != stuff_p;
				stuff2_p = stuff2_p->next)
			;
		stuff2_p->next = stuff_p->next;
	}

	FREE(stuff_p);
}

/*
 * Name:        breakstuff()
 *
 * Abstract:    Break all "stuff" that crosses scorefeeds.
 *
 * Returns:     void
 *
 * Description: This function loops through the main linked list and all STUFF,
 *		looking for stuff that crosses score feeds.  When it finds such
 *		stuff, it breaks them at that point.
 */

static void
breakstuff()
{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct STUFF *stuff_p;		/* point at a STUFF structure */


	debug(16, "breakstuff");
	initstructs();

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			/* keep SSVs up to date; we need to know time sig */
			asgnssv(mainll_p->u.ssv_p);
			continue;

		case S_STAFF:
			/* break out to handle this STAFF */
			break;

		default:
			/* nothing to do, skip to next loop */
			continue;
		}

		/*
		 * Loop through all the stuff on this staff.  Do it in reverse
		 * order so that when stuff gets broken, the new STUFFs end up
		 * in the correct order.
		 */
		if (mainll_p->u.staff_p->stuff_p != 0) {

			stuff_p = mainll_p->u.staff_p->stuff_p;
				while (stuff_p->next != 0)
					stuff_p = stuff_p->next;
			/* now stuff_p points at the last one in the list */

			for ( ; stuff_p != 0; stuff_p = prevstuff(mainll_p,
					stuff_p)) {
				/*
				 * If there's a "til" clause, and it crosses
				 * bar lines, call a function to see if it
				 * needs to be broken, and if so, break it.
				 */
				if (stuff_p->end.bars > 0)
					breakone(mainll_p, stuff_p,
							Score.timenum,
							stuff_p->string, 1);
			}
		}
	}
}

/*
 * Name:        prevstuff()
 *
 * Abstract:    Find stuff preceding the given one.
 *
 * Returns:     pointer to previous stuff, or 0 if none
 *
 * Description: This function is given a pointer to a staff mainll item and a
 *		stuff in that list.  It finds the preceding stuff, returning
 *		it, or 0 if none.  If stuff linked lists were doubly linked,
 *		we wouldn't have to go through this aggravation.
 */

static struct STUFF *
prevstuff(mainll_p, stuff_p)

struct MAINLL *mainll_p;	/* ptr to MLL item for a stuff */
struct STUFF *stuff_p;		/* ptr to current stuff */

{
	register struct STUFF *prevstuff_p;


	prevstuff_p = mainll_p->u.staff_p->stuff_p; /* get 1st stuff in list */

	/* if current stuff is first stuff, there is none before it */
	if (prevstuff_p == stuff_p)
		return (0);

	/* loop until we find it, then return */
	while (prevstuff_p->next != stuff_p)
		prevstuff_p = prevstuff_p->next;
	return (prevstuff_p);
}

/*
 * Name:        breakone()
 *
 * Abstract:    Break one "stuff" item if it crosses scorefeeds.
 *
 * Returns:     void
 *
 * Description: This function is given a STUFF with a "til" clause that crosses
 *		bar line(s).  It finds out if the STUFF crosses the FEED at the
 *		end of the score.  If so, it stops the current stuff there, and
 *		starts a continuation on the next score.  If there are more bar
 *		lines yet to be crossed, it calls itself recursively.
 */

static void
breakone(mllstaff_p, stuff_p, timenum, origstr_p, depth)

struct MAINLL *mllstaff_p;	/* point at MLL struct holding stuff's staff */
struct STUFF *stuff_p;		/* point at a STUFF structure */
int timenum;			/* numerator of current time sig */
char *origstr_p;		/* original string, from first score of stuff */
int depth;			/* depth of recursion */

{
	/*
	 * Make the local variables static, to use less stack when making
	 * recursive calls.  We can get away with this because the recursive
	 * call is at the end of the function, and the variables aren't used
	 * after that.
	 */
	static struct MAINLL *m2_p;	/* point along main linked list */
	static struct MAINLL *m3_p;	/* point along main linked list */
	static struct MAINLL *m4_p;	/* point along main linked list */
	static struct STUFF *s_p;	/* point at a new STUFF */
	static struct GRPSYL *gs_p;	/* point along a GRPSYL list */
	static int bars;		/* count how many bars long it is */
	static int otimenum;		/* num of time sig before last bar */
	static float endcount;		/* count where the stuff ends */
	static float endsteps;		/* step offset where the stuff ends */
	static int endgracebackup;	/* how far to back up from the end */


	debug(32, "breakone file=%s line=%d timenum=%d origstr_p=\"%s\"",
			stuff_p->inputfile, stuff_p->inputlineno, timenum,
			origstr_p == 0 ? "" : origstr_p);
	/*
	 * Save info about where this stuff ends, before any breaking.
	 */
	bars = stuff_p->end.bars;
	endcount = stuff_p->end.count;
	endsteps = stuff_p->end.steps;
	endgracebackup = stuff_p->end.gracebackup;

	/*
	 * Loop forward until crossing the given number of bars, or hitting a
	 * FEED, whichever comes first.  Keep the timenum updated.  At each
	 * bar line, save what it was before in otimenum.
	 */
	for (m2_p = mllstaff_p; bars >= 0 && m2_p != 0 && m2_p->str != S_FEED;
			m2_p = m2_p->next) {

		switch (m2_p->str) {
		case S_SSV:
			if (m2_p->u.ssv_p->used[TIME] == YES)
				timenum = m2_p->u.ssv_p->timenum;
			break;

		case S_BAR:
			otimenum = timenum;
			bars--;
			break;

		case S_STAFF:
			/*
			 * If this is the first staff of a measure, check the
			 * first voice to see if it's a multirest.  (If it is,
			 * all voices on all staffs would be.)  If so, decrement
			 * the bars remaining appropriately.  It could end up
			 * less than 0.  That's okay; we won't be breaking it.
			 */
			if (m2_p->u.staff_p->staffno == 1 &&
			m2_p->u.staff_p->groups_p[0]->basictime < -1) {

				bars += 1 + m2_p->u.staff_p->
						groups_p[0]->basictime;
			}
			break;
		}
	}

	/*
	 * If bars <= -1, this stuff doesn't reach the end of this score, so
	 * just return.  If we hit the end of the piece, end the stuff at the
	 * last count of the last measure (thus the last bar line).  The
	 * parser has already put out a warning for this.
	 * If the stuff ends at the pseudobar of the next score, make it end at
	 * the last bar of this score instead.  However, phrases should not be
	 * adjusted this way, because under these conditions they will get
	 * pulled to the note at count 1.
	 * Note also that if it ends at the pseudobar, but end.steps > 0, we
	 * fall through to break the stuff.
	 * And if gracebackup is being used, we have to assume that this STUFF
	 * will attach to something after this bar line, so in that case also,
	 * we fall through.
	 */
	if (bars <= -1)
		return;
	if (m2_p == 0 || (bars == 0 && stuff_p->end.count == 0 &&
			stuff_p->end.steps <= 0.0 &&
			stuff_p->end.gracebackup == 0 &&
			stuff_p->stuff_type != ST_PHRASE)) {
		stuff_p->end.bars -= bars + 1;
		stuff_p->end.count = timenum + 1;
		return;
	}

	/*
	 * At this point we know that the stuff truly crosses the FEED we're
	 * at, and we're going to have to break it.  First, terminate the part
	 * of the stuff on this score and mark the carryout.  (However, if the
	 * next staff starts with a multirest that fully contains the rest of
	 * the stuff, we'll cancel the carryout later and not insert a new
	 * stuff for continuation.)
	 */
	stuff_p->end.bars -= bars + 1;		/* only cross this many bars */
	stuff_p->end.count = otimenum + 1;	/* end at end of last measure*/
	stuff_p->end.steps = 0.0;		/* zap any offset from there */
	stuff_p->end.gracebackup = 0;		/* zap any gracebackup */
	stuff_p->carryout = YES;

	/*
	 * Find the matching STAFF in the next measure.  It might not exist
	 * due to blocks.
	 */
	for (m3_p = m2_p->next; m3_p != 0 && m3_p->str != S_BAR;
			m3_p = m3_p->next) {
		if (m3_p->str == S_STAFF && m3_p->u.staff_p->staffno ==
					    mllstaff_p->u.staff_p->staffno)
			break;
	}
	if (m3_p == 0) {
		return;
	}
	if (m3_p->str != S_STAFF)
		pfatal("can't find staff in main linked list [breakone]");

	/*
	 * If the staff has a multirest that's longer than the remaining
	 * length of the stuff, don't insert a new stuff; just get out now.
	 */
	if (m3_p->u.staff_p->groups_p[0]->basictime < -1) {
		if (bars < -(m3_p->u.staff_p->groups_p[0]->basictime)) {
			stuff_p->carryout = NO;	/* cancel the carryout */
			return;
		}
	}

	/*
	 * Create a new STUFF starting at count 0 of the new measure.  Link it
	 * in to its STAFF and set fields appropriately.
	 */
	s_p = newSTUFF(origstr_p, stuff_p->dist, stuff_p->dist_usage,
			stuff_p->aligntag, (double)0, (double)0, (short)0,
			bars, endcount, endsteps, endgracebackup,
			stuff_p->stuff_type, stuff_p->modifier, stuff_p->place,
			stuff_p->inputfile, stuff_p->inputlineno);
	s_p->next = m3_p->u.staff_p->stuff_p;
	m3_p->u.staff_p->stuff_p = s_p;
	s_p->carryin = YES;
	s_p->vno = stuff_p->vno;	/* actually only needed for phrases */

	switch (s_p->stuff_type) {
	case ST_ROM:
	case ST_BOLD:
	case ST_ITAL:
	case ST_BOLDITAL:
		/*
		 * If this staff was visible on the previous score, we just
		 * want the continuation dashes on this score, not the original
		 * string, so we change the string in that case.  (If the staff
		 * is also invisible on this score, we don't care what happens,
		 * so don't even check for that.)  Otherwise, leave it alone.
		 * Also, all other types that have strings should always repeat
		 * them on each score, so leave them alone.
		 */
		if (mllstaff_p->u.staff_p->visible == YES)
			s_p->string = dashstr(stuff_p->string);
		break;
	case ST_PHRASE:
		/*
		 * The new (2nd half of the) phrase mark has as its first group
		 * its voice's first GRPSYL in the new score.  Its last group
		 * is the last GRPSYL of the entire phrase.  The last group of
		 * the old (1st half of the) phrase mark is the last GRPSYL on
		 * that score for this voice.
		 */
		s_p->beggrp_p = m3_p->u.staff_p->groups_p[ stuff_p->vno - 1 ];
		s_p->endgrp_p = stuff_p->endgrp_p;
		/* find matching staff in the measure before the FEED */
		for (m4_p = m2_p; m4_p->str != S_STAFF ||
				m4_p->u.staff_p->staffno !=
				mllstaff_p->u.staff_p->staffno;
				m4_p = m4_p->prev)
			;
		/* find last group in the measure in that voice */
		for (gs_p = m4_p->u.staff_p->groups_p[ stuff_p->vno - 1 ];
				gs_p->next != 0; gs_p = gs_p->next)
			;
		stuff_p->endgrp_p = gs_p;
		break;
	}

	/*
	 * If there are still more bars to be crossed, call ourselves
	 * recursively to break the stuff again if necessary.  But first check
	 * to see we aren't too deep in recursion, using too much stack.
	 */
	if (s_p->end.bars > 0) {
		if (depth >= 300)
			l_ufatal(stuff_p->inputfile, stuff_p->inputlineno,
			"stuff crosses more than 300 scores (break into shorter pieces)");
		breakone(m3_p, s_p, timenum, origstr_p, depth + 1);
	}
}

/*
 * Name:        contpedal()
 *
 * Abstract:    Insert pedal continuation STUFFs when needed.
 *
 * Returns:     void
 *
 * Description: When a pedal is to be held down through a score feed, the
 *		print phase needs to know where (vertically) to draw the pedal
 *		line on the new score.  There might not occur another pedal
 *		character in the first measure there, or even on that whole
 *		score.  So whenever this happens, regardless of whether there
 *		are already pedal characters on the new score, this function
 *		inserts a new STUFF with a special, null pedal indication, at
 *		count 0 of the first measure of the new score.
 */

static void
contpedal()

{
	short inpedal[MAXSTAFFS + 1];	/* does this staff have pedal on now?*/
	short snappedal[MAXSTAFFS + 1];	/* same, when an ending is entered */
	short align[MAXSTAFFS + 1];	/* aligntag of last pedal mark */
					/*  (meaningful only when inpedal YES)*/
	short snapalign[MAXSTAFFS + 1];	/* same, when an ending is entered */
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct MAINLL *m2_p;		/* another pointer along MLL */
	struct STUFF *stuff_p;		/* point at a STUFF structure */
	struct STUFF *stuff2_p;		/* point at new STUFF structure */
	struct BAR *bar_p;		/* point at a bar line */
	int inending;			/* are we now in an ending? */
	int s;				/* staff number */


	debug(16, "contpedal");
	/* init pedal state to NO for all staffs */
	for (s = 1; s <= MAXSTAFFS; s++)
		inpedal[s] = NO;

	inending = NO;

	/*
	 * Loop through main linked list.  For each STAFF found, update pedal
	 * state when necessary.  For each FEED found, if there's a pedal
	 * crossing it, insert a pedal continuation.  There is special work to
	 * do when endings are encountered, since this can change pedal states.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {

		case S_STAFF:
			/*
			 * Loop through all the stuff on this staff.  Whenever
			 * there's a pedal mark, update the state for that
			 * staff, except that pedal up/down is irrelevant.
			 */
			for (stuff_p = mainll_p->u.staff_p->stuff_p;
					stuff_p != 0; stuff_p = stuff_p->next) {

				if (stuff_p->stuff_type != ST_PEDAL)
					continue;

				/*
				 * For non-ENDPED, in addition to noting that we
				 * are in a pedal sequence, note its aligntag.
				 */
				if (string_is_sym(stuff_p->string,
						C_BEGPED, FONT_MUSIC) == YES ||
				    string_is_sym(stuff_p->string,
						C_PEDAL, FONT_MUSIC) == YES) {
					inpedal[mainll_p->u.staff_p->staffno]
						= YES;
					align[mainll_p->u.staff_p->staffno]
						= stuff_p->aligntag;
				}

				else if (string_is_sym(stuff_p->string,
						C_ENDPED, FONT_MUSIC) == YES)
					inpedal[mainll_p->u.staff_p->
							staffno] = NO;
			}
			break;

		case S_BAR:
			/*
			 * When entering a second or later ending, the state of
			 * all pedals must be set back to what they were when
			 * entering the first ending.
			 * First deal with the case where an ending starts at
			 * the pseudobar after a feed.  In that case, we need
			 * to ignore any endending on the previous real bar.
			 */
			bar_p = mainll_p->u.bar_p;
			for (m2_p = mainll_p->next; m2_p != 0 &&
					m2_p->str != S_BAR &&
					(m2_p->str != S_CLEFSIG ||
					m2_p->u.clefsig_p->bar_p == 0);
					m2_p = m2_p->next)
				;
			if (m2_p != 0 && m2_p->str == S_CLEFSIG) {
				/*
				 * If an ending starts at this pseudobar,
				 * substitute this pseudobar for the real one.
				 */
				if (m2_p->u.clefsig_p->bar_p->endingloc
							== STARTITEM)
					bar_p = m2_p->u.clefsig_p->bar_p;
			}

			switch (bar_p->endingloc) {
			case STARTITEM:
				if (inending == NO) {
					/* entering first ending */
					/* snapshot pedal states here */
					for (s = 1; s <= MAXSTAFFS; s++) {
						snappedal[s] = inpedal[s];
						snapalign[s] = align[s];
					}
					inending = YES;
				} else {
					/* entering a later ending */
					/* restore snapshotted state */
					for (s = 1; s <= MAXSTAFFS; s++) {
						inpedal[s] = snappedal[s];
						align[s] = snapalign[s];
					}
				}
				break;

			case ENDITEM:
				/* leaving the last ending */
				inending = NO;
				break;
			}
			break;

		case S_FEED:
			/* only consider FEEDs that are before music */
			if ( ! IS_CLEFSIG_FEED(mainll_p)) {
				break;
			}

			/*
			 * For every staff where the pedal is now down, find
			 * the structure for that staff in the first measure,
			 * and insert a STUFF to indicate the continuation of
			 * the pedal mark line.
			 */
			for (s = 1; s <= MAXSTAFFS; s++) {

				if (inpedal[s] == NO)
					continue;

				for (m2_p = mainll_p; m2_p != 0 &&
						(m2_p->str != S_STAFF ||
						m2_p->u.staff_p->staffno != s);
						m2_p = m2_p->next)
					;
				if (m2_p == 0)
					pfatal("couldn't find staff in contpedal");
				/* pedal continuation at count 0, w/ null str */
				stuff2_p = newSTUFF((char *)0, (double)0,
					SD_NONE, align[s], (double)0,
					(double)0, (short)0, 0, (double)0,
					(double)0, (short)0, ST_PEDAL, TM_NONE,
					PL_BELOW, (char *)0, -1);

				/* put at beginning of staff's linked list */
				stuff2_p->next = m2_p->u.staff_p->stuff_p;
				m2_p->u.staff_p->stuff_p = stuff2_p;
			}
			break;
		}
	}
}

/*
 * Name:        setstuff()
 *
 * Abstract:    Set horizontal absolute coordinates of "stuff".
 *
 * Returns:     void
 *
 * Description: This function loops through the main linked list.  For every
 *		visible staff, it loops through its list of STUFF structures,
 *		setting all their absolute horizontal coordinates.
 */

static void
setstuff()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct CHHEAD *chhead_p;	/* chord headcell of current measure */
	struct BAR *bar_p;		/* [pseudo] bar before current meas */
	struct STUFF *stuff_p;		/* point at a STUFF structure */
	struct MAINLL *m2_p;		/* MLL for end of stuff */
	short timeden2;			/* time sig denom at end of stuff */
	struct CHHEAD *chhead2_p;	/* chord headcell for end of stuff */
	struct BAR *bar2_p;		/* bar line for end of stuff */
	float stepsize;			/* STEPSIZE scaled by staffscale */
	float leftwid;			/* width of string left of align pt. */
	float streast;			/* east end of a string */
	int pedstyle;			/* P_* */
	int pedchar;			/* pedal char, in the P_LINE form */
	int font, size;			/* of a pedal char */
	char *string;			/* point to the pedal char */
	float staffscale;		/* store it here for convenience */
	float wid;			/* width of one side of the string */
	int vscheme2;			/* vscheme at the end of the STUFF */
	int ret;			/* return code from geteast */


	debug(16, "setstuff");
	initstructs();

	chhead_p = 0;		/* prevent useless 'used before set' warning */
	bar_p = 0;		/* prevent useless 'used before set' warning */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		/*
		 * Do various set up work per structure type.  If it's a
		 * visible staff, break and go do the rest of this loop.
		 * Otherwise, "continue" on to the next structure.
		 */
		switch (mainll_p->str) {
		case S_SSV:
			/* keep SSVs up to date; we need to know timeden */
			asgnssv(mainll_p->u.ssv_p);
			continue;

		case S_CHHEAD:
			/* always remember preceding chord headcell */
			chhead_p = mainll_p->u.chhead_p;
			continue;

		case S_BAR:
			/* always remember where the preceding bar was */
			bar_p = mainll_p->u.bar_p;
			continue;

		case S_CLEFSIG:
			/*
			 * The pseudo bar that's in a clefsig following a
			 * scorefeed overrides the real bar at the end of the
			 * previous score.
			 */
			if (mainll_p->u.clefsig_p->bar_p != 0)
				bar_p = mainll_p->u.clefsig_p->bar_p;
			continue;

		case S_STAFF:
			/* visible staff breaks out to do rest of the loop */
			if (mainll_p->u.staff_p->visible == YES)
				break;
			continue;

		default:
			continue;
		}

		/* get these for this staff */
		staffscale = svpath(mainll_p->u.staff_p->staffno,
				STAFFSCALE)->staffscale;
		stepsize = STEPSIZE * staffscale;

		/*
		 * Loop through all the stuff on this staff.
		 */
		for (stuff_p = mainll_p->u.staff_p->stuff_p; stuff_p != 0;
				stuff_p = stuff_p->next) {

			/* coords are not used for phrases */
			if (stuff_p->stuff_type == ST_PHRASE) {
				continue;
			}

			/*
			 * Handle the special case of a fermata on a measure
			 * rest.  No matter what count the user requested,
			 * we're going to center it above or below the measure
			 * rest character.  We also assume there is no "til"
			 * clause.
			 */
			if (string_is_sym(stuff_p->string, C_FERM, FONT_MUSIC) == YES ||
			    string_is_sym(stuff_p->string, C_UFERM, FONT_MUSIC) == YES) {

				if (setmrferm(mainll_p->u.staff_p, stuff_p)
						== YES)
					continue;
				/*
				 * The fermata is not on a measure rest.  Fall
				 * through to handle like any normal stuff.
				 */
			}

			/*
			 * Set c[AX] for the stuff.
			 */
			setstuffcoord(stuff_p, AX, mainll_p, mainll_p, bar_p,
				chhead_p, Score.timeden, svpath(
				mainll_p->u.staff_p->staffno, VSCHEME)->vscheme,
				stuff_p->start.count, stuff_p->start.steps,
				stuff_p->start.gracebackup, stepsize);

			/*
			 * If this stuff has a "til" clause, find where the
			 * ending point is and set c[AE] to that.  If this
			 * stuff also has a string, and the string extends
			 * beyond the point that the "til" clause asks for,
			 * c[AE] will be changed later to agree with the string.
			 */
			if (stuff_p->end.bars != 0 || stuff_p->end.count != 0
					|| stuff_p->end.gracebackup != 0
					|| stuff_p->end.steps != 0) {
				/*
				 * Init variables to the current position
				 * (beginning of the stuff).  If the stuff is	
				 * supposed to extend across one or more bar
				 * lines, geteast() will alter them to the
				 * correct values for the bar where the stuff
				 * ends.  It will blow away the 'til' clause in
				 * certain multirest scenarios.
				 */
				m2_p = mainll_p;	/* this stuff's staff*/
				timeden2 = Score.timeden;
				chhead2_p = chhead_p;
				bar2_p = bar_p;
				vscheme2 = svpath(m2_p->u.staff_p->staffno,
					VSCHEME)->vscheme;

				ret = geteast(stuff_p, &m2_p, &timeden2,
					 &chhead2_p, &bar2_p, &vscheme2);

				switch (ret) {
				case 1:
					setstuffcoord(stuff_p, AE, mainll_p,
						m2_p, bar2_p, chhead2_p,
						timeden2, vscheme2,
						stuff_p->end.count,
						stuff_p->end.steps,
						stuff_p->end.gracebackup,
						stepsize);
					break;
				case 2:
					/* end the stuff a little beyond the */
					/* barline at the start of the stuff */
					stuff_p->c[AE] = bar2_p->c[AX]
						+ STUFFSTEP(stuff_p);
					break;
				case 3:
					/* no "til" clause anymore */
					/* set to 0 temporarily */
					stuff_p->c[AE] = 0;
					break;
				}
				/*
				 * If there is still a "til" clause (it wasn't
				 * blown away), make sure AE isn't left of AX.
				 * Although the parse phase checks the bars and
				 * counts for this, it could still happen here
				 * if start.steps is too big or end.steps is
				 * too small (too negative).  If it happens,
				 * warn, and set AE to AX.
				 */
				if (stuff_p->end.bars != 0 ||
						stuff_p->end.count != 0 ||
						stuff_p->end.gracebackup != 0 ||
						stuff_p->end.steps != 0) {
					if (stuff_p->c[AE] < stuff_p->c[AX]) {
						l_warning(stuff_p->inputfile,
						stuff_p->inputlineno,
						"mark begins after 'til' position; moving 'til' position to the right");
						stuff_p->c[AE] = stuff_p->c[AX];
					}
				}
			} else {
				/* no "til" clause; set to 0 temporarily */
				stuff_p->c[AE] = 0;
			}

			/*
			 * Set c[AW] for all stuff types, and c[AE] for ones
			 * that don't have a "til" clause.  Reset c[AE] for
			 * ones that have a "til" clause, if they have a string
			 * that sticks out farther than that.
			 */
			switch (stuff_p->stuff_type) {
			case ST_ROM:
			case ST_BOLD:
			case ST_ITAL:
			case ST_BOLDITAL:
			case ST_MUSSYM:
			case ST_OCTAVE:
				/*
				 * These types (and only these) have a string,
				 * except that pedal is different enough to
				 * have separate code.  Set c[AW] such that
				 * the first character will be centered at
				 * c[AX].
				 */

				/*
				 * If this is a chord with a grid, handle it
				 * all in the subroutine.
				 */
				if (stuff_p->modifier == TM_CHORD && trygrid(
						mainll_p, stuff_p) == YES) {
					break;
				}

				leftwid = left_width(stuff_p->string);
				stuff_p->c[AW] = stuff_p->c[AX] - leftwid;

				/* find the initial east of the string */
				streast = stuff_p->c[AW] +
						strwidth(stuff_p->string);
				/*
				 * If the string would go beyond the right
				 * margin, try to split it onto multiple lines,
				 * and reset the string's east.  Don't do this
				 * for chord/analysis/figbass.
				 */
				if (streast - endspace_width(stuff_p->string) >
				   PGWIDTH - eff_rightmargin(mainll_p) &&
				   ! IS_CHORDLIKE(stuff_p->modifier)) {
					stuff_p->string = split_string(
						stuff_p->string, PGWIDTH -
						eff_rightmargin(mainll_p) -
						stuff_p->c[AW]);
					streast = stuff_p->c[AW] +
						strwidth(stuff_p->string);
				}

				/*
				 * If this east is beyond the east given by the
				 * "til" clause (if any), use it for the AE.
				 */
				if (streast > stuff_p->c[AE])
					stuff_p->c[AE] = streast;

				break;

			case ST_PEDAL:
				if (stuff_p->string == 0) {
					/* must be a pedal continuation; */
					/*  these have no width */
					stuff_p->c[AW] = stuff_p->c[AE] =
							stuff_p->c[AX];
					break;
				}

				/* for P_LINE, the right char is in the string*/
				pedstyle = svpath(mainll_p->u.staff_p->staffno,
						PEDSTYLE)->pedstyle;
				if (pedstyle == P_LINE) {
					stuff_p->c[AW] = stuff_p->c[AX] - 
						left_width(stuff_p->string);
					stuff_p->c[AE] = stuff_p->c[AW] +
						strwidth(stuff_p->string);
					break;
				}

				/* must use string for "Ped.", star, etc. */

				/* extract the pedal character to be printed */
				font = stuff_p->string[0];
				size = stuff_p->string[1];
				string = stuff_p->string + 2;
				pedchar = next_str_char(&string, &font, &size)
						& 0xff;

				/* subtract the left part from AX to get AW */
				wid = leftped(pedstyle, pedchar);
				if (stuff_p->all == YES) {
					wid *= Score.staffscale;
				} else {
					wid *= staffscale;
				}
				stuff_p->c[AW] = stuff_p->c[AX] - wid;

				/* add the right part to AX to get AE */
				wid = rightped(pedstyle, pedchar);
				if (stuff_p->all == YES) {
					wid *= Score.staffscale;
				} else {
					wid *= staffscale;
				}
				stuff_p->c[AE] = stuff_p->c[AX] + wid;

				break;

			case ST_CRESC:
			case ST_DECRESC:
			case ST_PHRASE:
			case ST_TIESLUR: /* due to carry-in into ending */
			case ST_TABSLUR: /* due to carry-in into ending */
			case ST_BEND:    /* due to carry-in into ending */
				/*
				 * These types (and only these) have no string.
				 * Set c[AW] the same as c[AX].  These always
				 * have "til" clauses, so c[AE] is already set.
				 * (Actually, c[AE] is not set for TIESLUR,
				 * TABSLUR, or BEND, but no one uses it anyhow.)
				 */
				stuff_p->c[AW] = stuff_p->c[AX];
				break;

			case ST_MIDI:
				/* this file doesn't deal with MIDI at all */
				break;

			default:
				pfatal("unknown stuff type %d", 
						stuff_p->stuff_type);
			}
		}
	}
}

/*
 * Name:        count2coord()
 *
 * Abstract:    Convert a count in a measure to the absolute horizontal coord.
 *
 * Returns:     coordinate corresponding to the given count
 *
 * Description: This function, given a count number in a measure, and the
 *		preceding bar line, chord head cell, and time signature
 *		denominator, calculates and returns the absolute horizonal
 *		coordinate of that place.  In a measure, the preceding bar line
 *		(or pseudo bar line, if this is the first measure of a score)
 *		is regarded as count 0, and the following bar line is regarded
 *		as count N + 1, where N is the numerator of the time signature.
 *		Between any adjacent chords, or chord and bar line, time is
 *		treated as proportional to distance.
 */

static double
count2coord(count, bar_p, chhead_p, timeden)

double count;		/* count in measure */
struct BAR *bar_p;	/* bar at start of measure (pseudobar if 1st in score)*/
struct CHHEAD *chhead_p;/* chord headcell for this measure */
int timeden;		/* denominator of time signature in this measure */

{
	struct CHORD *ch_p, *nch_p;	/* point at CHORD structures */
	float frac;		/* what fraction of a whole it starts at */
	float coord;		/* the answer */


	if (count < 1) {
		/*
		 * The stuff is before the first chord ("count 1").  So we
		 * consider the preceding bar line to be count 0, and allocate
		 * time proportionally.
		 */
		coord = bar_p->c[AX] + (count / timeden) *
				bar_p->c[INCHPERWHOLE];
	} else {
		/*
		 * Convert the "count" where this stuff begins to what fraction
		 * of a whole it starts at.
		 */
		frac = (count - 1) / timeden;

		/*
		 * In this loop, ch_p starts at the first chord and nch_p is
		 * the next chord.  They move forward in parallel.  We get out
		 * either when nch_p is 0 (ch_p is the last chord), or when
		 * nch_p is beyond the place where our stuff goes (the stuff
		 * goes at or following ch_p).
		 */
		for (ch_p = chhead_p->ch_p, nch_p = ch_p->ch_p;
		     nch_p != 0 && RAT2FLOAT(nch_p->starttime) <= frac;
		     ch_p = nch_p, nch_p = nch_p->ch_p)
			;

		/*
		 * Subtract to find how far (timewise) the stuff is after chord
		 * ch_p.  Then allocate space proportionally.
		 */
		coord = ch_p->c[AX] + (frac - RAT2FLOAT(ch_p->starttime)) *
				ch_p->c[INCHPERWHOLE];
	}

	return (coord);
}

/*
 * Name:        setstuffcoord()
 *
 * Abstract:    Set the AX or AE coordinate for a STUFF.
 *
 * Returns:     void
 *
 * Description: This function sets AX for a STUFF, or the AE for the end of a
 *		"til" clause of a STUFF.
 */

static void
setstuffcoord(stuff_p, coordidx, mainll_p, m2_p, bar_p, chhead_p, timeden,
		vscheme, count, steps, gracebackup, stepsize)

struct STUFF *stuff_p;		/* the STUFF in question */
int coordidx;			/* AX if coord at start, AE if coord at end */
struct MAINLL *mainll_p;	/* at the start of the STUFF */
struct MAINLL *m2_p;		/* start or end of the STUFF, where coord is */
struct BAR *bar_p;		/* bar before the coord */
struct CHHEAD *chhead_p;	/* chhead before the coord */
int timeden;			/* time sig denominator in effect at coord */
int vscheme;			/* vscheme in effect at coord */
double count;			/* count into measure for this coord */
double steps;			/* step offset for this coord */
int gracebackup;		/* gracebackup for this coord */
double stepsize;		/* for this staff or score, as applicable */

{
	struct GRPSYL *gs_p;
	int vidx;
	int n;


	if (gracebackup == 0) {
		/*
		 * Use the c[INCHPERWHOLE] coordinate of the chord or bar line
		 * that is at or before its "start".
		 */
		stuff_p->c[coordidx] =
				count2coord(count, bar_p, chhead_p, timeden) +
				steps * STUFFSTEP(stuff_p);
		return;
	}

	/*
	 * Since we have to back up by some number of grace notes, we have to
	 * find a group to associate the STUFF with.  Then we can back up from
	 * there.
	 */
	vidx = 0;	/* avoid 'used before set' warning */
	switch (vscheme) {
	case V_1:		/* 1 */
		vidx = 0;	/* only one voice, use it */
		break;
	case V_2FREESTEM:	/* 2f */
	case V_3FREESTEM:	/* 3f */
		if (chkallspace(mainll_p, stuff_p, 0) == YES) {
			/* first voice all spaces, apply stuff to second */
			vidx = 1;
			break;
		} else if (chkallspace(mainll_p, stuff_p, 1) == YES) {
			/* second voice all spaces, apply stuff to first */
			vidx = 0;
			break;
		}
		/* FALL THROUGH;handle like V_2OPSTEM */
	case V_2OPSTEM:		/* 2o */
	case V_3OPSTEM:		/* 3o */
		vidx = stuff_p->place == PL_ABOVE ? 0 : 1;
		break;
	}

	/* find closest note group in this voice */
	gs_p = closestgroup(count, m2_p->u.staff_p->groups_p[vidx], timeden);

	/* back up right number of grace groups */
	for (n = 0; n < gracebackup; n++) {
		gs_p = gs_p->prev;
		if (gs_p == 0 || gs_p->grpvalue != GV_ZERO) {
			l_ufatal(stuff_p->inputfile, stuff_p->inputlineno,
				"not enough grace groups to back up to for stuff");
		}
	}

	/*
	 * Put the stuff at this grace group, except that the steps offset must
	 * be applied.
	 */
	stuff_p->c[coordidx] = gs_p->c[AX] + steps * STUFFSTEP(stuff_p);
}

/*
 * Name:        geteast()
 *
 * Abstract:    Point variables at things relevant to the east end of a STUFF.
 *
 * Returns:     1 if count2coord() should be used to find AE of stuff
 *		2 if AE of stuff should be near *bar_p_p due to multirest
 *		3 if 'til' clause is being blown away due to multirest
 *
 * Description: This function is given pointers pertaining to a measure where
 *		a STUFF begins.  It searches forward to the end of the stuff,
 *		according to the given number of measures.  It sets *timeden2_p,
 *		*chhead2_p_p, and *bar2_p_p to the values they should be for
 *		the end of the stuff, if the return code is 1 (the usual case).
 *		Also update *m2_p_p and *vscheme2_p in case 1.
 *		For 2 and 3, only *bar2_p_p is guaranteed to be meaningful.
 */

static int
geteast(stuff_p, m2_p_p, timeden2_p, chhead2_p_p, bar2_p_p, vscheme2_p)

struct STUFF *stuff_p;		/* pointer to the stuff */
struct MAINLL **m2_p_p;		/* starts at start of stuff, change to end */
short *timeden2_p;		/* starts at start of stuff, change to end */
struct CHHEAD **chhead2_p_p;	/* starts at start of stuff, change to end */
struct BAR **bar2_p_p;		/* starts at start of stuff, change to end */
int *vscheme2_p;		/* return vscheme at end */

{
	struct MAINLL *m2_p;	/* convenient pointer */
	int applied_ssvs;	/* did we apply any SSVs? */
	int staffno;		/* staff that this stuff hangs off of */
	int blimit;		/* number of bars to search forward past */
	int timenum;		/* numerator of time signature */
	int b;			/* count bar lines */


	applied_ssvs = NO;
	m2_p = *m2_p_p;
	staffno = m2_p->u.staff_p->staffno;

	blimit = stuff_p->end.bars;	/* number of bars to cross */

	/* if all within one bar, return right away */
	if (blimit == 0) {
		/* if starts and ends inside multirest, blow away 'til' clause*/
		if (m2_p->u.staff_p->groups_p[0]->basictime < -1) {
			stuff_p->end.count = 0;
			return (3);
		}

		return (1);	/* normal case; retain 'til' clause */
	}

	timenum = Score.timeden;	/* keep track of time sig numerator */

	/*
	 * The input parameters point at values that are for the beginning of
	 * the stuff.  Loop forward the requested number of bars to get to the
	 * end of the stuff.  While doing this, keep those variables updated.
	 * By the end of this function, they will be correct for the end of
	 * the stuff.
	 */
	b = 0;
	/* if it starts at a multirest, account for that */
	if (m2_p->u.staff_p->groups_p[0]->basictime < -1) {
		/* if it ends inside the multirest, point at previous bar, */
		/*  blow away 'til' clause, and get out */
		if (-(m2_p->u.staff_p->groups_p[0]->basictime) > blimit) {
			*bar2_p_p = m2_p->u.bar_p;
			stuff_p->end.bars = 0;
			stuff_p->end.count = 0;
			return (3);
		}

		b = -1 - m2_p->u.staff_p->groups_p[0]->basictime;
	}
	for ( ; b < blimit; b++) {
		for (m2_p = m2_p->next; m2_p != 0 && m2_p->str != S_BAR;
					m2_p = m2_p->next) {

			switch (m2_p->str) {
			case S_SSV:
				/*
				 * Assign SSV, and remember we did, so that we
				 * will restore them before we return.
				 */
				asgnssv(m2_p->u.ssv_p);
				applied_ssvs = YES;
				break;

			case S_STAFF:
				/* multirests count as multiple measures */
				if (m2_p->u.staff_p->staffno == 1 &&
				m2_p->u.staff_p->groups_p[0]->basictime < -1) {

					/* add (multi - 1) to no. of bars */
					b -= 1 + m2_p->u.staff_p->
							groups_p[0]->basictime;

					/*
					 * If the stuff doesn't make it into
					 * the last measure, or doesn't make it
					 * to the last measure's bar line, make
					 * it stop at the bar before multirest.
					 */
					if (b > blimit || (b == blimit &&
					stuff_p->end.count < timenum + 1)) {
						/* if we messed with SSVs,
						 * restore them */
						if (applied_ssvs == YES) {
							setssvstate(*m2_p_p);
						}
						return (2);
					}

					/*
					 * If it ends at the bar after the
					 * multirest, move forward to that bar
					 * and end the stuff there.
					 */
					if (b == blimit) {
						while (m2_p->str != S_BAR)
							m2_p = m2_p->next;
						*bar2_p_p = m2_p->u.bar_p;

						/* if we messed with SSVs,
						 * restore them */
						if (applied_ssvs == YES) {
							setssvstate(*m2_p_p);
						}
						return (2);
					}
				}
				break;
			}
		}

		if (m2_p == 0)
			pfatal("'til' clause extends beyond end of the piece [geteast1]");

		*bar2_p_p = m2_p->u.bar_p;
	}

	/*
	 * m2_p points at the bar line preceding or at the place where the
	 * stuff ends.  Continue forward to be pointing at the CHHEAD of that
	 * measure and also do final variable updates.
	 */
	for ( ; m2_p != 0 && m2_p->str != S_CHHEAD;
			m2_p = m2_p->next) {

		switch (m2_p->str) {
		case S_SSV:
			if (m2_p->u.ssv_p->used[TIME] == YES)
				*timeden2_p = m2_p->u.ssv_p->timeden;
			break;
		case S_CLEFSIG:
			if (m2_p->u.clefsig_p->bar_p != 0)
				*bar2_p_p = m2_p->u.clefsig_p->bar_p;
			break;
		}
	}

	if (m2_p == 0)
		pfatal("'til' clause extends beyond end of the piece [geteast2]");

	/* if we messed with SSVs, restore them */
	if (applied_ssvs == YES) {
		setssvstate(*m2_p_p);
	}

	/*
	 * If the first staff in this measure has a multirest, return that fact
	 * and have the stuff end at this bar line.
	 */
	if (m2_p->next->u.staff_p->groups_p[0]->basictime < -1)
		return (2);

	*chhead2_p_p = m2_p->u.chhead_p;

	/* move m2_p forward to the matching staff in this measure */
	for (m2_p = m2_p->next; m2_p != 0 && m2_p->str == S_STAFF &&
			m2_p->u.staff_p->staffno != staffno; m2_p = m2_p->next){
		;
	}
	if (m2_p->u.staff_p->staffno != staffno) {
		l_warning(stuff_p->inputfile, stuff_p->inputlineno,
			"removing 'til' clause: staff %d does not exist in the measure where this item ends",
			staffno);
		return (3);
	}
	*m2_p_p = m2_p;		/* update the input variable */
	return (1);		/* didn't end during a multirest */
}

/*
 * Name:        setmrferm()
 *
 * Abstract:    Set absolute horizonal coords of a fermata on measure rest/rpt.
 *
 * Returns:     YES or NO
 *
 * Description: This function is given a staff and a stuff for a fermata there.
 *		It decides whether the fermata applies to a measure rest or a
 *		measure repeat.  If it does, it sets its coords to align with
 *		the symbol and returns YES.  Otherwise it returns NO.
 */

static int
setmrferm(staff_p, stuff_p)

struct STAFF *staff_p;
struct STUFF *stuff_p;

{
	struct GRPSYL *gs_p;	/* point at the relevant grpsyl list */
	float fermx;		/* the center of the fermata */
	float fermwidth;	/* width of the fermata */
	int font, size, code;	/* of the fermata */
	char *s_p;		/* point into the string holding the fermata */


	debug(32, "setmrferm file=%s line=%d", stuff_p->inputfile,
			stuff_p->inputlineno);
	/*
	 * Who knows why the user would put a fermata between two staffs, but
	 * if they did, treat it as normal stuff.
	 */
	if (stuff_p->place == PL_BETWEEN)
		return (NO);

	/*
	 * Figure out which voice this fermata is meant to apply to.  It could
	 * be that one voice has a measure rest and the other doesn't.  For
	 * measure repeats, both would have it, so we wouldn't care which
	 * voice, but we might as well fall through the same code.  Point
	 * at the first GRPSYL in that voice's list.
	 */
	gs_p = 0;		/* prevent useless 'used before set' warning */
	switch (svpath(staff_p->staffno, VSCHEME)->vscheme) {
	case V_1:
		/* only 1 voice, this must be it */
		gs_p = staff_p->groups_p[0];
		break;

	case V_2OPSTEM:
	case V_3OPSTEM:
		/*
		 * Technically we should attach the fermata to the voice it is
		 * next to.  But if that voice is all spaces, that wouldn't
		 * make much sense.  So fall through and handle the same as
		 * the freestem cases.
		 */

	case V_2FREESTEM:
	case V_3FREESTEM:
		/* apply it to the nearest voice, unless it's all spaces */
		if (stuff_p->place == PL_ABOVE) {
			if ( ! hasspace(staff_p->groups_p[0], Zero, Score.time))
				gs_p = staff_p->groups_p[0];
			else
				gs_p = staff_p->groups_p[1];
		} else {
			if ( ! hasspace(staff_p->groups_p[1], Zero, Score.time))
				gs_p = staff_p->groups_p[1];
			else
				gs_p = staff_p->groups_p[0];
		}
		break;
	}

	/*
	 * If the relevant voice is not a measure rest or measure repeat, don't
	 * do anything more.  Just return and let the normal "stuff" code
	 * handle this fermata.
	 */
	if (gs_p->is_meas == NO)
		return (NO);

	/*
	 * This fermata is on a measure rest/repeat.  We can't rely on the AX
	 * of this; it's offset like a whole rest would be.  We have to
	 * average the AW and AE to know where the center should be.
	 * If the user requested a stepsize offset, we will apply it to the
	 * result, so in that case it won't really be centered after all.
	 */
	fermx = (gs_p->c[AW] + gs_p->c[AE]) / 2 +
				stuff_p->start.steps * STEPSIZE;

	/* find width of fermata */
	font = stuff_p->string[0];
	size = stuff_p->string[1];
	s_p = &stuff_p->string[2];
	code = next_str_char(&s_p, &font, &size);
	fermwidth = width(font, size, code);

	/* set the absolute horizontal coords */
	stuff_p->c[AX] = fermx;
	stuff_p->c[AW] = fermx - fermwidth / 2;
	stuff_p->c[AE] = fermx + fermwidth / 2;

	return (YES);
}

/*
 * Name:        trygrid()
 *
 * Abstract:	Handle a chord STUFF if there are chord grids.
 *
 * Returns:     YES if a grid will be printed with this chord, NO if not
 *
 * Description: If there are to be chord grids printed at the end of the song,
 *		this function sets the "used" flag.  If a grid should be
 *		printed here (where the chord is used), it sets the coordinates
 *		of this STUFF appropriately.
 */

static int
trygrid(mainll_p, stuff_p)

struct MAINLL *mainll_p;	/* MLL struct for this chord stuff */
struct STUFF *stuff_p;		/* the chord stuff */

{
	int gridswhereused;	/* YES or NO for this staff */
	int gridsatend;		/* YES or NO for this staff, or the score */
	struct GRID *grid_p;	/* point to the grid definition */
	char *asciichord_p;	/* point at ASCII version of chord */
	float geast, gwest;	/* relative coords of the grid */
	float hstrw;		/* half the width of the chord string */


	if (stuff_p->all == YES) {
		gridsatend = Score.gridsatend;
		gridswhereused = Score.gridswhereused;
	} else {
		gridsatend = svpath(mainll_p->u.staff_p->staffno,
				GRIDSATEND)->gridsatend;
		gridswhereused = svpath(mainll_p->u.staff_p->staffno,
				GRIDSWHEREUSED)->gridswhereused;
	}

	if (gridsatend == NO && gridswhereused == NO) {
		return (NO);
	}

	grid_p = findgrid(stuff_p->string);

	/* if grid was never defined, warn and return no grid here */
	if (grid_p == 0) {
		asciichord_p = ascii_str(stuff_p->string, YES, NO, TM_CHORD);
		l_warning(stuff_p->inputfile, stuff_p->inputlineno,
				"no grid defined for chord %s", asciichord_p);
		return (NO);
	}

	if (gridsatend == YES) {	/* grids print at end of song*/
		/* mark grid as used if it hasn't been, and keep count */
		if (grid_p->used == NO) {
			grid_p->used = YES;
			Atend_info.grids_used++;
		}
	}

	/* if no grid is to be printed here, get out */
	if (gridswhereused == NO)
		return (NO);

	/* we only need east & west here */
	gridsize(grid_p, stuff_p->all ? 0 : mainll_p->u.staff_p->staffno,
			(float *)0, (float *)0, &geast, &gwest);
	hstrw = strwidth(stuff_p->string) / 2.0;

	/*
	 * The stuff extends as far as necessary to contain both the string and
	 * the grid.  Unlike for other string stuffs, the string is centered at
	 * this count.  (So is the grid, if you ignore any "X fr" string on its
	 * right side.)  Note:  "til" clauses are not allowed on chords.
	 */
	stuff_p->c[AE] = stuff_p->c[AX] + MAX(geast, hstrw);
	stuff_p->c[AW] = stuff_p->c[AX] + MIN(gwest, -hstrw);

	return (YES);		/* grid gets printed here */
}

/*
 * Name:        tieslurstuff()
 *
 * Abstract:	Create STUFF structures for all ties, slurs, and bends.
 *
 * Returns:     void
 *
 * Description: This function puts ST_TIESLUR, ST_TABSLUR, and ST_BEND STUFF
 *		structures in the linked lists.  Unlike for the other stuff
 *		types, the parser never does this.  So up until now, there have
 *		been none of these in the linked lists.  More than one STUFF is
 *		needed if there the tie/slur/bend is across a bar line into
 *		endings, or across a scorefeed.
 */

static void
tieslurstuff()
{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct GRPSYL *gs_p;		/* point at a group */
	int v;				/* voice number, 0 or 1 */
	int n, k;			/* loop variables */


	debug(16, "tieslurstuff");
	initstructs();

	/*
	 * Loop through main linked list, looking for visible STAFFs.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			/* keep SSVs up to date for benefit of l_warning */
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		if (mainll_p->str != S_STAFF ||
		    mainll_p->u.staff_p->visible == NO)
			continue;

		/*
		 * Loop through all voices on this staff.
		 */
		for (v = 0; v < MAXVOICES && mainll_p->u.staff_p->
				groups_p[v] != 0; v++) {
			/*
			 * Loop through the groups in this voice.  Ignore rests
			 * and spaces.
			 */
			for (gs_p = mainll_p->u.staff_p->groups_p[v];
					gs_p != 0; gs_p = gs_p->next) {

				if (gs_p->grpcont != GC_NOTES)
					continue;

				/*
				 * For each note in this group, for each tie
				 * and slur starting at it, call a function to
				 * allocate a STUFF for it (more if into an
				 * ending or scorefeed).  However, do not do
				 * this for ties on a tablature staff.  They
				 * are not to be printed.
				 */
				for (n = 0; n < gs_p->nnotes; n++) {
					if (gs_p->notelist[n].tie == YES &&
							! is_tab_staff(mainll_p
							->u.staff_p->staffno))
						mktieslurstuff(mainll_p,
							gs_p, n, -1);

					for (k = 0; k < gs_p->notelist[n].
								nslurto; k++) {
						mktieslurstuff(mainll_p,
							gs_p, n, k);
					}
				}
			}
		}
	}
}

/*
 * Name:        mktieslurstuff()
 *
 * Abstract:	Create STUFF structure(s) for one tie/slur/bend.
 *
 * Returns:     void
 *
 * Description: This function puts ST_TIESLUR, ST_TABSLUR, or ST_BEND STUFF
 *		structure(s) in the linked list for one tie or slur curve.
 *		Normally only one is needed, but if it crosses a scorefeed
 *		two are needed, and if it crosses into a first ending, one is
 *		needed for each following ending.
 */

static void
mktieslurstuff(mllstaff_p, gs_p, n, s)

struct MAINLL *mllstaff_p;	/* MLL of staff where tie/slur/bend starts */
struct GRPSYL *gs_p;		/* ptr to group where tie/slur/bend starts */
int n;				/* index to note in notelist where it starts */
int s;				/* index into slurtolist, or -1 for tie */

{
	struct STUFF *stuff_p;		/* point at a STUFF structure */
	struct STAFF *staff_p;		/* point at staff of mllstaff */
	struct MAINLL *mll_p;		/* point along MLL */
	struct MAINLL *mll2_p;		/* another pointer along MLL */
	struct BAR *bar_p;		/* point to a bar or pseudobar */
	struct MAINLL *ebmll_p;		/* end bar MLL struct */
	int endingloc;			/* ending location */
	int stufftype;			/* which of the stuff types is it? */
	int other_vno;			/* tied_to_voice or slurred_to_voice */


	staff_p = mllstaff_p->u.staff_p;

	if (s == -1) {					/* tie */
		stufftype = ST_TIESLUR;
	} else if (gs_p->notelist[n].is_bend == YES &&
		   s == gs_p->notelist[n].nslurto - 1) {/* bend */
		stufftype = ST_BEND;
	} else if (is_tab_staff(staff_p->staffno) ||	/* slur on tab or */
		   is_tab_staff(staff_p->staffno + 1)) { /* tabnote staff */
		stufftype = ST_TABSLUR;
	} else {					/* slur on other staff*/
		stufftype = ST_TIESLUR;
	}

	/* allocate a STUFF structure for this tie/slur/bend */
	stuff_p = newSTUFF((char *)0, (double)0, SD_NONE, NOALIGNTAG, (double)0,
			(double)0, 0, 0, (double)0, (double)0, (short)0,
			stufftype, TM_NONE, PL_UNKNOWN, (char *)0, 0);

	/* fill in additional items needed for tie/slur */
	stuff_p->vno = gs_p->vno;
	stuff_p->beggrp_p = gs_p;
	stuff_p->begnote_p = &gs_p->notelist[n];
	stuff_p->curveno = (short)s;

	/*
	 * Normally endgrp_p is to be left null.  But if we are tied/slurred to
	 * a different voice, find the destination group, and store it there.
	 */
	other_vno = s == -1 ? gs_p->notelist[n].tied_to_voice :
			      gs_p->notelist[n].slurtolist[s].slurred_to_voice;
	if (other_vno != NO_TO_VOICE) {
		stuff_p->endgrp_p = find_to_group(mllstaff_p, gs_p,
				other_vno, s == -1 ? "tie" : "slur");
	}


	/* put the new STUFF into this staff's stuff list, at start of list */
	stuff_p->next = staff_p->stuff_p;
	staff_p->stuff_p = stuff_p;

	/* if this is not the last group in measure, no more STUFFs needed */
	if (gs_p->next != 0)
		return;

	/* if this is a slur to/from nowhere, no second piece needed */
	if (s >= 0 && (stufftype == ST_TABSLUR || stufftype == ST_TIESLUR) &&
			IS_NOWHERE(gs_p->notelist[n].slurtolist[s].octave))
		return;

	/*
	 * We might need more STUFF(s), either because of a scorefeed, or
	 * because of endings.  First check for the scorefeed case.
	 */
	/* find bar line at end of measure */
	for (ebmll_p = mllstaff_p; ebmll_p != 0 && ebmll_p->str != S_BAR;
			ebmll_p = ebmll_p->next)
		;
	if (ebmll_p == 0)
		pfatal("final measure has no bar line");

	/* does a feed occur before next staff? */
	for (mll_p = ebmll_p; mll_p != 0 && mll_p->str != S_FEED &&
			mll_p->str != S_STAFF; mll_p = mll_p->next)
		;
	if (mll_p == 0)
		return;

	if (mll_p->str == S_FEED) {
		/* found a feed, so try to make the extra STUFF */
		stuff_p->carryout = YES;
		mkextrastuff(mll_p, stuff_p, gs_p, n, s, stufftype, other_vno);
	}

	/*
	 * Find whether we need to check for endings.  This is true when we are
	 * at the start of a first ending.  So endingloc has to be STARTITEM,
	 * and the previous bar must not be in an ending.  If we are at a FEED,
	 * look at the pseudobar's endingloc, else look at the previous real
	 * bar.  In any case, search back from the real bar.
	 */
	if (mll_p->str == S_FEED) {
		/*
		 * Find the next CLEFSIG.  Normally it's the next thing after
		 * this FEED, but it's possible there are block(s) here and so
		 * it will be after some later FEED.
		 */
		while (mll_p != 0 && mll_p->str != S_CLEFSIG)
			mll_p = mll_p->next;
		if (mll_p == 0)
			return;	/* there must not be any more music */
		bar_p = mll_p->u.clefsig_p->bar_p;
	} else {
		bar_p = ebmll_p->u.bar_p;
	}
	if (bar_p->endingloc != STARTITEM)
		return;
	for (mll2_p = ebmll_p->prev; mll2_p != 0 && mll2_p->str != S_BAR;
			mll2_p = mll2_p->prev)
		;
	if (mll2_p != 0 && (mll2_p->u.bar_p->endingloc == STARTITEM ||
			    mll2_p->u.bar_p->endingloc == INITEM))
		return;

	/*
	 * The curve was into a first ending.  So we have to loop forward,
	 * looking for the bar lines at the start of each ending, and try to
	 * make another STUFF for the curve carrying into that ending.
	 */
	for ( ; mll_p != 0; mll_p = mll_p->next) {

		if (mll_p->str != S_BAR)
			continue;

		/* see if a pseudo bar occurs before the next staff */
		for (mll2_p = mll_p; mll2_p != 0 && mll2_p->str != S_STAFF &&
				(mll2_p->str != S_CLEFSIG ||
				 mll2_p->u.clefsig_p->bar_p == 0);
				mll2_p = mll2_p->next)
			;
		if (mll2_p == 0 || mll2_p->str == S_STAFF) {
			/* normal case, no pseudobar */
			endingloc = mll_p->u.bar_p->endingloc;
		} else {
			/* found pseudobar; use it instead of normal bar */
			endingloc = mll2_p->u.clefsig_p->bar_p->endingloc;
		}

		switch (endingloc) {


		case NOITEM:
		case ENDITEM:
			/* out of all endings, we're done */
			return;

		case STARTITEM:
			/*
			 * We are at a bar that begins another ending.  Try to
			 * find the matching staff in this measure.
			 */
			mkextrastuff(mll_p, stuff_p, gs_p, n, s, stufftype,
					other_vno);
		}
	}
}

/*
 * Name:        mkextrastuff()
 *
 * Abstract:	Create STUFF extra structure(s) for one tie/slur/bend.
 *
 * Returns:     void
 *
 * Description: This function puts ST_TIESLUR, ST_TABSLUR, or ST_BEND STUFF
 *		structure(s) in the linked list for the ending piece of a tie,
 *		slur, or bend that is broken across a scorefeed or into a
 *		second or later ending.
 */

static void
mkextrastuff(mll_p, origstuff_p, gs_p, n, s, stufftype, other_vno)

struct MAINLL *mll_p;		/* starts before first staff in measure */
struct STUFF *origstuff_p;	/* original STUFF for this curve */
struct GRPSYL *gs_p;		/* GRPSYL where curve started */
int n;				/* index to note in notelist where it starts */
int s;				/* index into slurtolist, or -1 for tie */
int stufftype;			/* which of the stuff types is it? */
int other_vno;			/* other voice we are tied/slurred to */

{
	struct STUFF *stuff_p;		/* new stuff */
	struct GRPSYL *endgs_p;		/* group where tie/slur/bend ends */
	int k;				/* loop variable */


	/* find first staff */
	for ( ; mll_p != 0 && mll_p->str != S_STAFF;
			mll_p = mll_p->next)
		;

	/* find matching staff */
	for ( ; mll_p != 0 && mll_p->str == S_STAFF &&
			mll_p->u.staff_p->staffno !=
			gs_p->staffno; mll_p = mll_p->next)
		;
	if (mll_p == 0)
		pfatal("last bar missing in main linked list [mkextrastuff]");
	if (mll_p->str != S_STAFF)	/* number of staffs changed? */
		return;


	/* allocate a STUFF structure second half of tie/slur/bend */
	stuff_p = newSTUFF((char *)0, 0, SD_NONE, NOALIGNTAG, (double)0,
			(double)0, 0, 0, (double)0, (double)0, (short)0,
			stufftype, TM_NONE, PL_UNKNOWN, (char *)0, 0);

	stuff_p->carryin = YES;
	stuff_p->costuff_p = origstuff_p;

	/*
	 * Fill in additional items needed for tie/slur.  Let vno match the
	 * first STUFF's vno, even if we are tied/slurred to a different voice.
	 * The later phases don't care about that field anyhow.
	 */
	stuff_p->vno = gs_p->vno;
	endgs_p = mll_p->u.staff_p->groups_p[
		(other_vno == NO_TO_VOICE ? gs_p->vno : other_vno) - 1];
	stuff_p->beggrp_p = endgs_p;	/* "beg" is really set to the end here*/

	/* begnote_p must be pointed at the note where tie/slur ends; find it */
	for (k = 0; k < endgs_p->nnotes; k++) {
		if (s == -1) {
			/* tie; check for matching note */
			if (endgs_p->notelist[k].letter ==
					gs_p->notelist[n].letter
			 && endgs_p->notelist[k].octave ==
					gs_p->notelist[n].octave)
				break;
		} else {
			/* slur/bend; check for note we're slurring/bending to*/
			if (endgs_p->notelist[k].letter ==
					gs_p->notelist[n].slurtolist[s].letter
			 && endgs_p->notelist[k].octave ==
					gs_p->notelist[n].slurtolist[s].octave)
				break;
		}
	}
	if (k == endgs_p->nnotes) {
		l_warning(endgs_p->inputfile, endgs_p->inputlineno,
			"can't find note being tied, slurred, or bent to");
		FREE(stuff_p);
		return;
	}

	stuff_p->begnote_p = &endgs_p->notelist[k];	/* end of tie/slur */
	stuff_p->curveno = (short)s;	/* same as for the carryout stuff */

	/* put the new STUFF into this staff's stuff list, at start of list */
	stuff_p->next = mll_p->u.staff_p->stuff_p;
	mll_p->u.staff_p->stuff_p = stuff_p;
}
