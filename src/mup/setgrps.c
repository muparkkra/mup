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
 * Name:	setgrps.c
 *
 * Description:	This file contains functions for setting the relative
 *		horizontal coordinates of all groups that contain notes
 *		(grpcont == GC_NOTES) and of all objects in these groups.
 *		It also sets relative vertical coordinates for the dots
 *		after notes.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/* check whether the group exists and is a grace */
#define IS_GRACE(g_p)	((g_p) != 0 && (g_p)->grpvalue == GV_ZERO)

struct NOTEPTRS {
	struct NOTE *top_p;	/* point at a note in top group */
	struct NOTE *bot_p;	/* point at same note in bottom group*/
	float wid;		/* width of the note head */
};

static struct GRPSYL *procallvoices P((struct MAINLL *mll_p,
		struct GRPSYL *gs_p));
static int contra_accs P((struct GRPSYL *top_p));
static void proc1or2voices P((struct MAINLL *mll_p, struct STAFF *staff_p,
		struct GRPSYL *gs1_p, struct GRPSYL *gs2_p, int contra));
static int compat P((struct NOTEPTRS noteptrs[], struct GRPSYL *gs1_p,
		struct GRPSYL *gs2_p));
static double can_overlap P((struct GRPSYL *gs1_p, struct GRPSYL *gs2_p));
static void apply_tsp P((struct MAINLL *mll_p, struct STAFF *staff_p,
		struct GRPSYL *gs_p));
static void procgrace P((struct NOTEPTRS noteptrs[], struct MAINLL *mll_p,
		struct STAFF *staff_p, struct GRPSYL *gs1_p,
		struct GRPSYL *gs2_p));
static int time2ticks P((int time));
static void procbunch P((struct NOTEPTRS noteptrs[], struct MAINLL *mll_p,
		struct STAFF *staff_p, struct GRPSYL *gs1_p,
		struct GRPSYL *gs2_p));
static void doaccparen P((struct NOTEPTRS noteptrs[], double halfwide,
		double halfhigh, int collinear, int sep_accs));
static int nextaccparen P((struct NOTEPTRS noteptrs[], int what, int found));
double stackleft P((struct NOTE *note_p, double base, float *parenwidth_p,
		int stackwhat));
static void dodot P((struct STAFF *staff_p, struct GRPSYL *gs1_p,
		struct GRPSYL *gs2_p, double halfwide, int collinear));
static void dogrpdot P((struct STAFF *staff_p, struct GRPSYL *gs_p,
		struct GRPSYL *ogs_p, double halfwide, int uppermost,
		int lowermost, int push));
static void westwith P((struct GRPSYL *gs_p));
static void eastwith P((struct GRPSYL *gs_p));
static void csbstempad P((struct MAINLL *mll_p, struct GRPSYL *gs_p));
static void proctab P((struct MAINLL *mll_p, struct STAFF *staff_p,
		struct GRPSYL *gs1_p));
static void noterparen P((struct NOTEPTRS noteptrs[], struct GRPSYL *gs1_p,
		struct GRPSYL *gs2_p, double halfwide, double halfhigh,
		int collinear));
static int nextacc P((struct NOTE *n_p[MAXVOICES][MAXHAND], int numgrps,
		int found));
static void css_alter_vert P((struct NOTE *note_p));
static void setwithside P((struct GRPSYL *gs_p, struct GRPSYL *ogs_p,
		struct GRPSYL *v3gs_p));
static void fixrollroom(struct CHORD *ch_p);

/*
 * Name:        setgrps()
 *
 * Abstract:	Find first group on each staff & call procallvoices to process.
 *
 * Returns:     void
 *
 * Description: This function goes through the chord lists, and for each chord,
 *		the list of GRPSYLs hanging off it.  It finds the first group
 *		on each staff, and calls procallvoices() to set the relative
 *		horizontal coordinates of all the note groups on that staff.
 *		Then it goes through the list again, deciding which side "with"
 *		lists should be on.
 */

void
setgrps()

{
	struct CHORD *ch_p;		/* point at a chord */
	struct GRPSYL *gs1_p;		/* point at a group */
	struct MAINLL *mainll_p;	/* point at items in main linked list*/
	struct MAINLL *mstaff_p;	/* for looking for staff */
	struct GRPSYL *gs_p;		/* loop through groups in this voice */
	struct GRPSYL *ogs_p;		/* first group in the "other" voice */
	struct STAFF *staff_p;		/* point at a staff */
	int vidx;			/* voice index, starts from 0 */


	debug(16, "setgrps");
	initstructs();		/* clean out old SSV info */
	init_rectab();		/* prepare for stacking accidentals */

	/*
	 * Loop down the main linked list looking for each chord list
	 * headcell.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		/* keep SSVs up to date */
		if (mainll_p->str == S_SSV)
			asgnssv(mainll_p->u.ssv_p);

		if (mainll_p->str != S_CHHEAD)
			continue;	/* skip everything but chord HC */

		/*
		 * Loop through each chord in this list.
		 */
		for (ch_p = mainll_p->u.chhead_p->ch_p; ch_p != 0;
					ch_p = ch_p->ch_p) {
			/*
			 * Loop through the linked list of GRPSYLs hanging off
			 * this chord.  Skip the syllables; just deal with the
			 * groups.  Upon finding the first group on a staff
			 * (which could be for any of the voices, since not all
			 * might be present in this chord), call procallvoices
			 * to process all the note groups.
			 */
			gs1_p = ch_p->gs_p;
			for (;;) {
				/* find first group on a staff */
				while (gs1_p != 0 &&
						gs1_p->grpsyl == GS_SYLLABLE)
					gs1_p = gs1_p->gs_p;
				if (gs1_p == 0)
					break;

				/* find the staff's MLL structure */
				mstaff_p = chmgrp2staffm(mainll_p, gs1_p);

				/* set gs1_p to after this staff's groups */
				gs1_p = procallvoices(mstaff_p, gs1_p);
			}

			/* make sure there is room for cross group rolls */
			fixrollroom(ch_p);
		}
	}

	free_rectab();

	initstructs();		/* clean out old SSV info */

	/*
	 * Loop down the main linked list looking for each staff.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		/* keep SSVs up to date; need it for vscheme */
		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
		}

		if (mainll_p->str != S_STAFF) {
			continue;	/* skip everything but staffs */
		}

		staff_p = mainll_p->u.staff_p;

		for (vidx = 0; vidx < MAXVOICES; vidx++) {

			/* set "with" side for each group in this voice */
			for (gs_p = staff_p->groups_p[ vidx ];
					gs_p != 0; gs_p = gs_p->next) {

				/* if no "with" list, nothing to do */
				if (gs_p->nwith == 0) {
					continue;
				}

				/* find the "other" voice */
				if (vidx == 2) {	/* voice 3 */
					ogs_p = 0;   /* default is no other */

					switch (gs_p->pvno) {
					case 1:	/* other voice is 2 */
						ogs_p = staff_p->groups_p[1];
						break;
					case 2:	/* other voice is 1 */
						ogs_p = staff_p->groups_p[0];
						break;
					}
				} else {  /* voice 1 or 2 */
					/* 1 ==> 2, 2 ==> 1 */
					ogs_p = staff_p->groups_p[ ! vidx ];
				}

				setwithside(gs_p, ogs_p, staff_p->groups_p[2]);
			}
		}
	}
}

/*
 * Name:        procallvoices()
 *
 * Abstract:    Process the groups for all the voices on one staff in a chord.
 *
 * Returns:     pointer to the first GRPSYL after these groups, 0 if none
 *
 * Description: This function is given the GRPSYL for the first (topmost) voice
 *		that is on this staff in this chord.  It finds what other
 *		GRPSYLs exist.  For each of them that is for notes (not rests
 *		or spaces), it calls proc1or2voices() to process them together
 *		and/or separately, as needed.  This file generally deals only
 *		with notes, not rests or spaces.  But this function also deals
 *		with rests to the following extent:  For both notes and rests,
 *		there are situations where voice 3 should "stand in" for voice 1
 *		or voice 2.  This function makes those decisions, and sets pvno.
 */

static struct GRPSYL *
procallvoices(mll_p, gs_p)

struct MAINLL *mll_p;		/* the MLL item the group is connected to */
struct GRPSYL *gs_p;		/* point at first voice on this staff */

{
	struct STAFF *staff_p;		/* point at staff */
	struct GRPSYL *g_p[MAXVOICES];	/* point at note groups */
	struct GRPSYL *g2_p[MAXVOICES];	/* point at note and rest groups */
	struct GRPSYL *gs1_p;		/* remember first group */
	struct GRPSYL *gs2_p;		/* another GRPSYL pointer */
	int numnonspace;		/* number of nonspace GRPSYLs */
	int numgrps;			/* how many note groups are here */
	int sep;			/* whether to apply acc separately */
	int contra;			/* return value of contra_accs() */
	int n;				/* loop variable, voices processed */


	staff_p = mll_p->u.staff_p;
	numgrps = 0;			/* no groups found yet */
	gs1_p = gs_p;			/* remember first group */

	/* find all groups in this chord on this staff; remember note groups */
	while (gs_p != 0 && gs_p->staffno == staff_p->staffno &&
			    gs_p->grpsyl == GS_GROUP) {
		gs_p->pvno = gs_p->vno;	/* init pseudo voice no. to voice no.*/
		if (gs_p->grpcont == GC_NOTES) {
			g_p[numgrps++] = gs_p;
		}
		gs_p = gs_p->gs_p;
	}

	/*
	 * Decide whether accidentals should be handled separately (per group)
	 * now; or whether they should be handled all together, later, after
	 * the groups are placed.  In theory, they should be handled later, so
	 * that they will be to the left of all groups if some groups end up
	 * side by side due to overlapping incompatible notes.  But in the
	 * case where the same note appears in multiple groups with different
	 * accidentals, there is no way to do it but to handle each group
	 * separately, and let some accs appear between the groups.  In some
	 * other cases, it doesn't matter much which way is chosen, because
	 * the result will look the same.  However, cases involving parentheses
	 * around a note that has an accidental, or slides from nowhere coming
	 * in to a note, look a little better when we handle the accs
	 * separately, so we'll do it that way when we have a choice.  On the
	 * other hand, if there are horizontal offsets ("ho"), we better handle
	 * the accs together if possible.
	 *
	 * Set the flag in each group.  When there is only one group, we can do
	 * it either way, so choose separate.  When there are three groups, we
	 * do the accs together unless there are contradictory accs.  For two,
	 * for now we set them to separate unless there are horizontal offsets;
	 * but that will be reversed later if proc1or2voices finds that the
	 * groups have to be offset horizontally (and contra is NO).
	 */
	/* if numgrps == 0, must avoid calling contra_accs (sep's irrelevant) */
	contra = NO;	/* default */
	if (numgrps <= 1 || (contra = contra_accs(g_p[0])) == YES) {
		sep = YES;
	} else if (numgrps == 3) {
		sep = NO;
	} else {
		sep = YES;
		for (n = 0; n < numgrps; n++) {
			if (g_p[n]->ho_usage != HO_NONE) {
				sep = NO;
				break;
			}
		}
	}
	for (n = 0; n < numgrps; n++) {
		g_p[n]->sep_accs = sep;
	}

	/*
	 * Before continuing on to process note groups, change voice 3's pvno
	 * when appropriate.  First find all nonspace groups.
	 */
	numnonspace = 0;		/* no nonspace groups found yet */
	gs2_p = gs1_p;

	/* find all nonspace groups in this chord on this staff */
	while (gs2_p != 0 && gs2_p->staffno == staff_p->staffno &&
			    gs2_p->grpsyl == GS_GROUP) {
		if (gs2_p->grpcont != GC_SPACE) {
			g2_p[numnonspace++] = gs2_p;
		} else {
			/*
			 * This is a convenient, though somewhat inappropriate,
			 * place to process grace groups that precede a space
			 * group.  Ones that precede notes groups will be
			 * processed in the normal flow, called from
			 * proc1or2voices.  Graces are not allowed before rest
			 * groups.  Graces before spaces are handled
			 * individually; there is no attempt to line them up
			 * with graces from other voices.
			 */
			struct NOTEPTRS noteptrs[MAXHAND + 1];
			procgrace(noteptrs, mll_p, staff_p, gs2_p,
					(struct GRPSYL *)0);
		}
		gs2_p = gs2_p->gs_p;
	}

	/*
	 * If the only nonspace voices are 1 and 3, or 2 and 3, and at least
	 * one of them is a rest and this is not a tab staff and "ho" was not
	 * used for either . . .
	 */
	if (numnonspace == 2 && g2_p[1]->vno == 3 &&
	   (g2_p[0]->grpcont == GC_REST || g2_p[1]->grpcont == GC_REST) &&
	   ! is_tab_staff(staff_p->staffno) && g2_p[0]->ho_usage == HO_NONE &&
	   g2_p[1]->ho_usage == HO_NONE) {
		/*
		 * If v1 is either a rest or stem-up notes and v3 is a rest or
		 * stem-down notes, let v3 stand in for v2.
		 */
		if (g2_p[0]->vno == 1 && ((g2_p[0]->grpcont == GC_NOTES &&
		    g2_p[0]->stemdir == UP) || g2_p[0]->grpcont == GC_REST) &&
		    ((g2_p[1]->grpcont == GC_NOTES && g2_p[1]->stemdir == DOWN)
		    || g2_p[1]->grpcont == GC_REST)) {
			g2_p[1]->pvno = 2;
		}
		/*
		 * If v2 is either a rest or stem-down notes and v3 is a rest or
		 * stem-up notes, let v3 stand in for v1.
		 */
		if (g2_p[0]->vno == 2 && ((g2_p[0]->grpcont == GC_NOTES &&
		    g2_p[0]->stemdir == DOWN) || g2_p[0]->grpcont == GC_REST) &&
		    ((g2_p[1]->grpcont == GC_NOTES && g2_p[1]->stemdir == UP) ||
		    g2_p[1]->grpcont == GC_REST)) {
			g2_p[1]->pvno = 1;
		}
	}

	/* if there were no note groups on this staff, nothing more to do */
	if (numgrps == 0)
		return (gs_p);

	n = 0;		/* number of voices processed so far */

	/*
	 * If voices 1 and 2 exist and are notes and do not have user specified
	 * horizontal offsets and this is not a tab staff, handle them together.
	 * If both voices 1 and 2 have a group here, they will be the first two
	 * found.  Tab staffs should be handled separately because their voices
	 * never conflict with each other (because of chktabcollision() in
	 * in setnotes.c).  Before checking the offsets, verify that they are
	 * legal and fix if not.
	 */
	if (numgrps >= 2 && g_p[0]->vno == 1 && g_p[1]->vno == 2 &&
			! is_tab_staff(staff_p->staffno)) {

		vfyoffset(g_p);		/* verify and fix */

		if (g_p[0]->ho_usage == HO_NONE && g_p[1]->ho_usage == HO_NONE){
			proc1or2voices(mll_p, staff_p, g_p[0], g_p[1], contra);
			n = 2;		/* processed 2 voices */
		}
	}

	/*
	 * Else, if v1 and v3, or v2 and v3, are notes, and only those two
	 * exist, and they do not have user specified horizontal offsets and
	 * this is not a tab staff, and v3's stem dir is compatible, let v3
	 * "stand in" for v1 or v2, as the case may be.  Handle the two voices
	 * together.
	 */
	else if (numgrps == 2 && numnonspace == 2 &&
			! is_tab_staff(staff_p->staffno) && g_p[0]->ho_usage ==
			HO_NONE && g_p[1]->ho_usage == HO_NONE) {

		if (g_p[0]->vno == 1 && g_p[0]->stemdir == UP &&
		    g_p[1]->vno == 3 && g_p[1]->stemdir == DOWN) {

			g_p[1]->pvno = 2;
			proc1or2voices(mll_p, staff_p, g_p[0], g_p[1], contra);
			n = 2;		/* processed 2 voices */

		} else if (g_p[0]->vno == 2 && g_p[0]->stemdir == DOWN &&
			   g_p[1]->vno == 3 && g_p[1]->stemdir == UP) {

			g_p[1]->pvno = 1;
			proc1or2voices(mll_p, staff_p, g_p[1], g_p[0], contra);
			n = 2;		/* processed 2 voices */
		}
	}

	/* process any remaining voices individually */
	for ( ; n < numgrps; n++) {
		proc1or2voices(mll_p, staff_p, g_p[n], (struct GRPSYL *)0,
				contra);
	}

	/* return the first GRPSYL after the groups we processed */
	return (gs_p);
}

/*
 * Name:        contra_accs()
 *
 * Abstract:    Do any groups in this chord/staff have contradictory accs?
 *
 * Returns:     YES or NO
 *
 * Description: This function is given the first group in a particular chord on
 *		a particular staff.  It checks whether any note exists in these
 *		groups with different accidentals.
 */

static int
contra_accs(top_p)

struct GRPSYL *top_p;	/* first group in the chord on the staff */

{
	struct GRPSYL *gs1_p, *gs2_p;	/* two groups in chord on staff */
	int n, k;			/* for looping through their notes */


	/* loop through every group in this chord on this staff */
	for (gs1_p = top_p; gs1_p != 0 &&
			gs1_p->staffno == top_p->staffno &&
			gs1_p->grpsyl == GS_GROUP;
			gs1_p = gs1_p->gs_p) {

		/* if it's not a note group, skip it */
		if (gs1_p->grpcont != GC_NOTES) {
			continue;
		}

		/* loop through every following group on this chord/staff */
		for (gs2_p = gs1_p->gs_p; gs2_p != 0 &&
				gs2_p->staffno == top_p->staffno &&
				gs2_p->grpsyl == GS_GROUP;
				gs2_p = gs2_p->gs_p) {

			/* if it's not a note group, skip it */
			if (gs2_p->grpcont != GC_NOTES) {
				continue;
			}

			/* loop through every note in first group */
			for (n = 0; n < gs1_p->nnotes; n++) {
				/* skip if it has no accs */
				if ( ! has_accs(gs1_p->notelist[n].acclist)) {
					continue;
				}

				/* loop through every note in 2nd group */
				for (k = 0; k < gs2_p->nnotes; k++) {
					/* skip if different letter/octave */
					if (gs1_p->notelist[n].letter !=
					    gs2_p->notelist[k].letter ||
					    gs1_p->notelist[n].octave !=
					    gs2_p->notelist[k].octave) {
						continue;
					}

					/* skip if it has no accs */
					if ( ! has_accs(
					    gs2_p->notelist[k].acclist)) {
						continue;
					}

					/* if they contradict, return YES */
					if (!eq_accs(gs1_p->notelist[n].acclist,
					    gs2_p->notelist[k].acclist)) {
						return (YES);
					}
				}
			}
		}
	}

	return (NO);
}

/*
 * Name:        proc1or2voices()
 *
 * Abstract:    Process a single voice, or voices 1 and 2 together.
 *
 * Returns:     void
 *
 * Description: This function is given pointers to one or two groups on a
 *		staff.  If it's just one (the second one is a null pointer),
 *		that group is to be handled alone.  If it is two, they are
 *		voices 1 and 2, since voice 3 is always handled separately.
 *		(Except that voice 3 can sometimes "stand in" for v1 or v2.)
 *		In any case, these are always note groups, not rest or space.
 *
 *		The function sets up an array (noteptrs) to point at each
 *		note in the group(s), figuring out whether the groups overlap
 *		and, if so, if they are compatible (see below for definition).
 *		It calls procbunch() to set relative horizontal coordinates for
 *		the notes, which is done either separately for each group or
 *		both at once, depending on the situation.  Then it calls
 *		procgrace() to do this for any grace notes preceding these
 *		group(s).   procgrace() does it by recursively calling here.
 */

static void
proc1or2voices(mll_p, staff_p, gs1_p, gs2_p, contra)

struct MAINLL *mll_p;		/* the MLL item the group is connected to */
struct STAFF *staff_p;			/* the staff the groups are on */
register struct GRPSYL *gs1_p, *gs2_p;	/* point at groups in this hand */
int contra;		/* do any groups have contradictory accidentals? */

{
	/*
	 * Each structure in this array points at a note.  Notes from gs1_p
	 * are pointed at by top_p, and, when both groups exist, notes
	 * from gs2_p are pointed at by bot_p.  If there's no overlap
	 * between the groups, there won't be any here either.  But if
	 * the groups "share" notes, the shared notes will be pointed
	 * at by both.  If the groups are "incompatible" (must be
	 * drawn shifted horizontally to avoid interference), they will
	 * be done separately and use this array separately, one at a time.
	 * And in that case, notes from both gs1_p and gs2_p will use top_p,
	 * in turn.
	 */
	struct NOTEPTRS noteptrs[MAXHAND + 1];

	float offset;		/* how far to offset incompatible groups */
	int num1;		/* number of notes in top group */
	int n;			/* loop variable */
	int incompat;		/* are groups incompatible (special case) */


	/*
	 * For mrpt, we have nothing to do except set the horizontal group
	 * coordinates.  If the first group is a measure repeat, so is the
	 * second one, if it exists at all.  We set a very small width, as a
	 * placeholder, because if other staffs have normal notes, we don't
	 * want the first chord to be abnormally wide because of the mrpt
	 * symbol.  (It will be centered in the measure.)  If all the staffs
	 * have mrpt, abshorz.c will ensure that enough space is left for
	 * these symbols.
	 */
	if (is_mrpt(gs1_p)) {
		gs1_p->c[RX] = 0;
		gs1_p->c[RE] = TEMPMRPTWIDTH / 2.0;
		gs1_p->c[RW] = -TEMPMRPTWIDTH / 2.0;

		if (gs2_p != 0) {
			gs2_p->c[RX] = 0;
			gs2_p->c[RE] = TEMPMRPTWIDTH / 2.0;
			gs2_p->c[RW] = -TEMPMRPTWIDTH / 2.0;
		}
		return;
	}

	/* clear out the array */
	for (n = 0; n < NUMELEM(noteptrs); n++) {
		noteptrs[n].top_p = 0;
		noteptrs[n].bot_p = 0;
		noteptrs[n].wid = 0.0;
	}

	num1 = gs1_p->nnotes;

	/* set all the "top" group pointers */
	for (n = 0; n < num1; n++)
		noteptrs[n].top_p = &gs1_p->notelist[n];

	/* if there is no "bottom" group, process the first bunch and quit */
	if (gs2_p == 0) {
		procbunch(noteptrs, mll_p, staff_p, gs1_p, (struct GRPSYL *)0);

		apply_tsp(mll_p, staff_p, gs1_p); /* add RE padding if needed */

		/* if group is rolled, allow room for the roll */
		if (gs1_p->roll != NOITEM)
			gs1_p->c[RW] -= ROLLPADDING;

		procgrace(noteptrs, mll_p, staff_p, gs1_p, (struct GRPSYL *)0);
		return;
	}

	/*
	 * If both groups are involved in different rolls, and their outer
	 * notes are <= 2 steps apart (maybe even negative [overlap]), we need
	 * to combine the rolls.  But only attempt this if the voices are 1 and
	 * 2.  It would be more complicated to allow for the case of voice 3,
	 * and rolls don't work right for voice 3 anyhow.
	 */
	if (gs1_p->roll != NOITEM && gs2_p->roll != NOITEM &&
	    gs1_p->roll != INITEM && gs2_p->roll != INITEM &&
	    gs1_p->notelist[gs1_p->nnotes - 1].stepsup
				- gs2_p->notelist[0].stepsup <= 2 &&
	    gs1_p->vno == 1 && gs2_p->vno == 2) {
		if (gs1_p->roll == LONEITEM) {
			gs1_p->roll = STARTITEM;
		} else if (gs1_p->roll == ENDITEM) {
			gs1_p->roll = INITEM;
		}
		if (gs2_p->roll == LONEITEM) {
			gs2_p->roll = ENDITEM;
		} else if (gs2_p->roll == STARTITEM) {
			gs2_p->roll = INITEM;
		}
	}

	/*
	 * If the lowest note of the top group is higher than the highest
	 * note of the bottom group, point at all the bottom notes,
	 * process both, and quit; unless there are certain conditions that
	 * must prevent it, in which case set incompat = YES.
	 */
	incompat = NO;
	if (noteptrs[num1-1].top_p->stepsup > gs2_p->notelist[0].stepsup) {
		/* we are probably okay, but exclude some exceptions */

		float total_with_height, room_between_notes;

		/* find height of "with" items between the groups */
		total_with_height = withheight(gs1_p, PL_BELOW) +
				    withheight(gs2_p, PL_ABOVE);
		/* find room, excluding what the "with" items are using */
		room_between_notes = (noteptrs[num1-1].top_p->stepsup -
				gs2_p->notelist[0].stepsup - 2.0) * STEPSIZE -
				total_with_height;

		/* if any "with"s, require a stepsize of empty space */
		if (total_with_height != 0.0 && room_between_notes < STEPSIZE) {
			incompat = YES;
		} else if (noteptrs[num1-1].top_p->stepsup ==
				gs2_p->notelist[0].stepsup + 1 &&
				gs2_p->notelist[0].stepsup % 2 == 0 &&
				gs2_p->dots == 0 &&
				gs1_p->dots > 0) {
			/*
			 * The inner notes of the two groups are on neighboring
			 * steps, and the top note of the bottom group is on a
			 * line and has a dot, and the top group has no dots.
			 * This potentially could have been compatible, using
			 * collinear stems, but there is no decent way to
			 * position the dot(s).
			 */
			incompat = YES;
		} else if ((gs1_p->stemdir == DOWN || gs2_p->stemdir == UP) &&
				noteptrs[num1-1].top_p->stepsup <
				gs2_p->notelist[0].stepsup + 3) {
			/*
			 * Since stem(s) have been forced pointing inward, we
			 * can't allow it because of insufficient space.
			 * Even if we had more space, there might be overlap,
			 * but we don't know stem lengths yet, so we would
			 * have allowed it: user beware.  They might have to
			 * use "len" or "ho" to avoid a collision.
			 */
			incompat = YES;
		} else {
			/*
			 * We have decided the groups are compatible, so
			 * process them and return.
			 */
			for (n = 0; n < gs2_p->nnotes; n++)
				noteptrs[num1+n].bot_p = &gs2_p->notelist[n];
			procbunch(noteptrs, mll_p, staff_p, gs1_p, gs2_p);

			apply_tsp(mll_p, staff_p, gs1_p); /* pad RE if needed */
			apply_tsp(mll_p, staff_p, gs2_p); /* pad RE if needed */

			/* if a group is rolled, allow room for the roll */
			if (gs1_p->roll != NOITEM)
				gs1_p->c[RW] -= ROLLPADDING;
			if (gs2_p->roll != NOITEM)
				gs2_p->c[RW] -= ROLLPADDING;

			procgrace(noteptrs, mll_p, staff_p, gs1_p, gs2_p);
			return;
		}
	}

	/*
	 * Either we set incompat == YES above or there was overlap.  If we
	 * didn't set incompat == YES call compat() to see what it thinks.
	 * (It also fills in group 2 in noteptrs).  If it is happy with this,
	 * process the groups together, and return.
	 */
	if (incompat == NO && compat(noteptrs, gs1_p, gs2_p) == YES) {
		procbunch(noteptrs, mll_p, staff_p, gs1_p, gs2_p);

		apply_tsp(mll_p, staff_p, gs1_p); /* add RE padding if needed */
		apply_tsp(mll_p, staff_p, gs2_p); /* add RE padding if needed */

		/* if a group is rolled, allow room for the roll */
		if (gs1_p->roll != NOITEM)
			gs1_p->c[RW] -= ROLLPADDING;
		if (gs2_p->roll != NOITEM)
			gs2_p->c[RW] -= ROLLPADDING;

		procgrace(noteptrs, mll_p, staff_p, gs1_p, gs2_p);
		return;
	}

	/*
	 * The fact that we are here means the two groups are not compatible,
	 * meaning they overlap but can't share note heads.  Clear the array
	 * of any notes from the second group, in case compat() put some there.
	 */
	for (n = 0; n < NUMELEM(noteptrs); n++)
		noteptrs[n].bot_p = 0;

	/*
	 * It is possible that the groups can at least be given collinear
	 * stems.  For this to be allowed, it must be that the bottom note of
	 * the top group is on the same step as the top note of the bottom
	 * group.  The top group's note can't have dots, the bottom group's
	 * can't have accidentals or a roll, and neither can have parentheses,
	 * because they couldn't be drawn decently.  Neither note can have
	 * another note on a neighboring step.
	 */
	if (noteptrs[num1-1].top_p->stepsup == gs2_p->notelist[0].stepsup &&

			gs1_p->dots == 0 &&

			! has_accs(gs2_p->notelist[0].acclist) &&

			gs2_p->roll == NOITEM &&

			noteptrs[num1-1].top_p->note_has_paren == NO &&
			gs2_p->notelist[0].note_has_paren == NO &&

			(num1 == 1 || noteptrs[num1-2].top_p->stepsup
				> noteptrs[num1-1].top_p->stepsup + 1) &&

			(gs2_p->nnotes == 1 || gs2_p->notelist[0].stepsup
				> gs2_p->notelist[1].stepsup + 1) ) {
		/*
		 * Since we are not sharing noteheads, the notes of the bottom
		 * group must be put after the notes of the top group in the
		 * noteptrs table.  Then process them together.
		 */
		for (n = 0; n < gs2_p->nnotes; n++)
			noteptrs[num1+n].bot_p = &gs2_p->notelist[n];
		procbunch(noteptrs, mll_p, staff_p, gs1_p, gs2_p);

		/* due to offset, only the bottom group might need RE padding */
		apply_tsp(mll_p, staff_p, gs2_p);

		/* if top group is rolled, allow room for the roll */
		if (gs1_p->roll != NOITEM)
			gs1_p->c[RW] -= ROLLPADDING;

		procgrace(noteptrs, mll_p, staff_p, gs1_p, gs2_p);
		return;
	}

	/*
	 * At this point we know we have to handle the groups separately.
	 * Because they overlap vertically and will be shifted horizontally, in
	 * order to get their accidentals to the left of both, we must handle
	 * their accidentals together, later, if contra allows.
	 */
	if (contra == NO) {
		gs1_p->sep_accs = NO;
		gs2_p->sep_accs = NO;
	}

	/* process the top group first */
	procbunch(noteptrs, mll_p, staff_p, gs1_p, (struct GRPSYL *)0);

	/*
	 * Clear the top group out of the array, and fill it with just the
	 * bottom group, to process them.  But mark them as if "top", to
	 * simplify procsome().
	 */
	for (n = 0; n < NUMELEM(noteptrs); n++)
		noteptrs[n].top_p = 0;

	/* set all the "top" group pointers even though this is group 2 */
	for (n = 0; n < gs2_p->nnotes; n++)
		noteptrs[n].top_p = &gs2_p->notelist[n];

	procbunch(noteptrs, mll_p, staff_p, gs2_p, (struct GRPSYL *)0);

	/*
	 * Now that we've figured out all the relative horizontal coords for
	 * the two groups (and everything in them) separately, we need to
	 * decide how to offset them so they don't overlap.  We'll offset
	 * each the same distance, one right and one left, and apply that
	 * offset to every horizontal coord of the groups.
	 */
	/*
	 * If the groups can be placed so that their rectangles overlap, do it.
	 * Else if one of the groups is to be rolled and the other is not, the
	 * one to be rolled must be put on the left.  Otherwise, find which
	 * direction gives minimal offset, but bias the results (0.1) to favor
	 * putting the top group towards the left, so that the stems will be
	 * closer to lining up.  Set "offset" to the offset to be applied to
	 * group 1.  Group 2's will be -offset.
	 */
	offset = can_overlap(gs1_p, gs2_p);
	if (offset != 0.0) {
		if (gs1_p->roll != NOITEM)
			gs1_p->c[RW] -= ROLLPADDING;
		if (gs2_p->roll != NOITEM)
			gs2_p->c[RW] -= ROLLPADDING;
	} else if (gs1_p->roll != NOITEM && gs2_p->roll == NOITEM) {
		/* only top group is rolled; it goes on left; its offset < 0 */
		offset = ( gs2_p->c[RW] - gs1_p->c[RE] ) / 2;
		gs1_p->c[RW] -= ROLLPADDING;
	} else if (gs1_p->roll == NOITEM && gs2_p->roll != NOITEM) {
		/* only bottom is rolled; top goes on right; top's offset > 0 */
		offset = ( gs2_p->c[RE] - gs1_p->c[RW] ) / 2;
		gs2_p->c[RW] -= ROLLPADDING;
	} else {
		/* either both are rolled or neither is; use other criterion */
		if (gs1_p->c[RE] - gs2_p->c[RW] <
					gs2_p->c[RE] - gs1_p->c[RW] + 0.1) {
			/* top group goes on left; its offset is negative */
			offset = ( gs2_p->c[RW] - gs1_p->c[RE] ) / 2;
			if (gs1_p->roll != NOITEM)
				gs1_p->c[RW] -= ROLLPADDING;
		} else {
			/* top group goes on right; its offset is positive */
			offset = ( gs2_p->c[RE] - gs1_p->c[RW] ) / 2;
			if (gs2_p->roll != NOITEM)
				gs2_p->c[RW] -= ROLLPADDING;
		}
	}

	/* apply offset to the groups */
	shiftgs(gs1_p, offset);
	shiftgs(gs2_p, -offset);

	/*
	 * Add RE padding if need be.  Usually only the group on the right may
	 * need it.  But even the left group might, like when the groups
	 * overlap horizontally.
	 */
	apply_tsp(mll_p, staff_p, gs1_p);
	apply_tsp(mll_p, staff_p, gs2_p);

	procgrace(noteptrs, mll_p, staff_p, gs1_p, gs2_p);
}

/*
 * Name:        compat()
 *
 * Abstract:    Determine whether two groups in a hand are "compatible".
 *
 * Returns:     YES or NO
 *
 * Description: This function is given pointers to the two groups in a hand,
 *		in a situation where they overlap.  The noteptrs array has
 *		just the top group filled in at this point.  The function
 *		figures out whether the two groups are compatible (see block
 *		comment below), or whether they must be drawn separately and
 *		offset horizontally.  While doing this, it fills in the bottom
 *		group part of noteptrs.  If it returns YES, this has been
 *		completed.  If it returns NO, this may be partially done,
 *		and the caller should clear out the partially complete bot_p
 *		part of noteptrs.
 */

static int
compat(noteptrs, gs1_p, gs2_p)

struct NOTEPTRS noteptrs[];		/* array of ptrs to notes to process */
register struct GRPSYL *gs1_p, *gs2_p;	/* point at groups in this hand */

{
	int num1;		/* number of notes in top group */
	register int n, k;	/* loop variables */


	num1 = gs1_p->nnotes;

	/*
	 * There is overlap between the two groups.  Try to match the bottom
	 * N notes of the top group with the top N notes of the bottom group.
	 * If all N are "compatible", we can "share" these notes.  For two
	 * groups to be compatible, they must meet the following conditions:
	 *	1) both basic time values must be half notes, or both must be
	 *	   shorter than half notes
	 *	2) both have no dots or the same number of dots
	 *	3) the bottom N notes of the top group are the same letters
	 *	   and octaves as the top N notes of the bottom group
	 * 	4) no two of these N notes can be on neighboring letters
	 * 	5) for each of the N pairs, the two notes do not have
	 *	   contradictory accidentals
	 *	6) for each of the N pairs, the two notes must have the same
	 *	   size and headshape
	 */
	/* check rule 1 */
	if (gs1_p->basictime < 2  || gs2_p->basictime < 2)
		return (NO);
	if (gs1_p->basictime == 2 && gs2_p->basictime != 2)
		return (NO);
	if (gs1_p->basictime != 2 && gs2_p->basictime == 2)
		return (NO);

	/* check rule 2 */
	if (gs1_p->dots != gs2_p->dots)
		return (NO);

	/* check rules 3, 4, 5, and 6 together */
	/* see if any note in the top group matches the top note in the other*/
	for (n = 0; n < num1; n++) {
		if (noteptrs[n].top_p->stepsup == gs2_p->notelist[0].stepsup)
			break;
	}
	if (n == num1)
		return (NO);		/* didn't find any match */

	/* starting with this note, verify that it and the rest match */
	for (k = 0; n < num1; k++, n++) {
		if (k >= gs2_p->nnotes)	/* not enough notes in group 2? */
			return (NO);
		if (gs2_p->notelist[k].stepsup != noteptrs[n].top_p->stepsup)
			return (NO);
		if (k > 0 &&
		gs2_p->notelist[k-1].stepsup - 1 == gs2_p->notelist[k].stepsup)
			return (NO);
		if ( ! eq_accs(gs2_p->notelist[k].acclist,
				noteptrs[n].top_p->acclist) &&
		    has_accs(gs2_p->notelist[k].acclist) &&
		    has_accs(noteptrs[n].top_p->acclist))
			return (NO);
		if (gs2_p->notelist[k].notesize != noteptrs[n].top_p->notesize)
			return (NO);
		if (gs2_p->notelist[k].headshape != noteptrs[n].top_p->headshape)
			return (NO);

		/* this note matches; set up noteptrs */
		noteptrs[n].bot_p = &gs2_p->notelist[k];
	}

	/*
	 * The fact that we made it to here means all the overlapping notes
	 * matched.  So fill the rest of group 2's note pointers.
	 */
	for ( ; k < gs2_p->nnotes; k++, n++)
		noteptrs[n].bot_p = &gs2_p->notelist[k];
	/*
	 * It is possible that, although the overlapping notes' headshapes
	 * match, some of the characters are mirrors of each other due to the
	 * opposite stem dir.  In these cases, group 2 rules.  So overwrite the
	 * notes in group 1.  If the lowest note in group 1 has to be changed,
	 * that could affect the RS of group 1, so change that too.
	 * If either note has an accidental, copy it to the other note (in case
	 * one started out as null and the other didn't).
	 * Also, while doing this, if any of these notes or their accs have
	 * parens in one group but not the other, erase those parens.
	 */
	n -= k;
	for (k = 0; n < num1; k++, n++) {
		gs1_p->notelist[n].headchar = gs2_p->notelist[k].headchar;
		gs1_p->notelist[n].headfont = gs2_p->notelist[k].headfont;
		gs1_p->notelist[n].c[RN] = gs2_p->notelist[k].c[RN];
		gs1_p->notelist[n].c[RS] = gs2_p->notelist[k].c[RS];

		if ( ! has_accs(gs1_p->notelist[n].acclist)) {
			COPY_ACCS(gs1_p->notelist[n].acclist,
					gs2_p->notelist[k].acclist);
		} else if ( ! has_accs(gs2_p->notelist[k].acclist)) {
			COPY_ACCS(gs2_p->notelist[k].acclist,
					gs1_p->notelist[n].acclist);
		}

		if (gs1_p->notelist[n].note_has_paren !=
		    gs2_p->notelist[k].note_has_paren) {
			gs1_p->notelist[n].note_has_paren = NO;
			gs2_p->notelist[k].note_has_paren = NO;
		}
		if (gs1_p->notelist[n].acc_has_paren !=
		    gs2_p->notelist[k].acc_has_paren) {
			gs1_p->notelist[n].acc_has_paren = NO;
			gs2_p->notelist[k].acc_has_paren = NO;
		}
	}
	gs1_p->c[RS] = gs2_p->notelist[k - 1].c[RS];

	return (YES);
}

/*
 * Name:        can_overlap()
 *
 * Abstract:    Decides whether incompatible groups' rectangles can overlap.
 *
 * Returns:     0.0 if they cannot, otherwise the offset to be used.
 *
 * Description: This function is given two incompatible groups in a hand.  It
 *		decides whether they can be placed such that their rectangles
 *		overlap.  This arrangement is where the first group is to the
 *		right of the second group, and the stems are about 3 stepsizes
 *		apart (though the amount depend on the note size and whether
 *		parentheses are present).  The noteheads must be separated
 *		enough vertically so that they don't collide, and various other
 *		things must also be true for this to work.
 */

static double
can_overlap(gs1_p, gs2_p)

struct GRPSYL *gs1_p, *gs2_p;	/* point at group(s) in this hand */

{
	struct NOTE *note_p;	/* a note in one of the groups */
	float offset;		/* return value */
	int notedist;		/* steps between two notes (absolute value) */
	int n, k;		/* loop counters */
	int parens;		/* do any overlapping notes have parens? */


	/*
	 * If there are "with" items between the two groups, that must have
	 * caused the incompatibility, and certainly we can't overlap.
	 */
	if (has_with(gs1_p, PL_BELOW) || has_with(gs2_p, PL_ABOVE)) {
		return (0.0);
	}

	/*
	 * Ensure that no note heads would collide.  We don't yet know
	 * whether any will be on the "wrong" side of their stem.  This is not
	 * too common and would rarely help things, so for now we assume the
	 * worst case, which is that all are on the "correct" side and thus
	 * have the potential of colliding with the other group's notes.
	 */
	parens = NO;
	for (n = 0; n < gs1_p->nnotes; n++) {
		for (k = 0; k < gs2_p->nnotes; k++) {

			notedist = gs1_p->notelist[n].stepsup -
				   gs2_p->notelist[k].stepsup;

			/* remember if overlapping notes have parens */
			if (notedist < 0 &&
					(gs1_p->notelist[n].note_has_paren ||
					 gs2_p->notelist[k].note_has_paren)) {
				parens = YES;
			}

			notedist = abs(notedist);

			/* never allow closer than 2 steps, 3 if parens */
			if (notedist < 2 || (notedist < 3 && parens == YES)) {
				return (0.0);
			}

			/* if either is double whole, restrict even more */
			if ((gs1_p->basictime == 0 || gs2_p->basictime == 0) &&
			    (notedist < 3 || (notedist < 4 && parens == YES))) {
				return (0.0);
			}
		}
	}

	/* neither group can have slashes */
	if (gs1_p->slash_alt > 0 || gs2_p->slash_alt > 0) {
		return (0.0);
	}

	/* the first group can't have accs, if they are applied separately */
	if (gs1_p->sep_accs == YES) {
		for (n = 0; n < gs1_p->nnotes; n++) {
			if (has_accs(gs1_p->notelist[n].acclist)) {
				return (0.0);
			}
		}
	}

	/* the first group can't any preceding grace groups */
	if (gs1_p->prev != 0 && gs1_p->prev->grpvalue == GV_ZERO) {
		return (0.0);
	}

	/* the first group can't have a roll unless the second group has one */
	if (gs1_p->roll != NOITEM && gs2_p->roll == NOITEM) {
		return (0.0);
	}

	/* the first group can't have slurs from nowhere */
	for (n = 0; n < gs1_p->nnotes; n++) {
		note_p = &gs1_p->notelist[n];
		for (k = 0; k < note_p->nslurto; k++) {
			if (note_p->slurtolist[k].octave == IN_UPWARD ||
			    note_p->slurtolist[k].octave == IN_DOWNWARD) {
				return (0.0);
			}
		}
	}

	/* the second group can't have any dots */
	if (gs2_p->dots > 0) {
		return (0.0);
	}

	/* the second group can't have any flags */
	if (gs2_p->basictime >= 8 && gs2_p->beamloc == NOITEM) {
		return (0.0);
	}

	/* the second group can't have slurs to nowhere */
	for (n = 0; n < gs2_p->nnotes; n++) {
		note_p = &gs2_p->notelist[n];
		for (k = 0; k < note_p->nslurto; k++) {
			if (note_p->slurtolist[k].octave == OUT_UPWARD ||
			    note_p->slurtolist[k].octave == OUT_DOWNWARD) {
				return (0.0);
			}
		}
	}

	/* neither group can have a stem forced the "wrong" way */
	if (gs1_p->stemdir == DOWN || gs2_p->stemdir == UP) {
		return (0.0);
	}

	/*
	 * This has to fail, because gs2 (stem down) is on the "wrong"
	 * (right hand) side for these notes.
	 */
	if (gs2_p->basictime <= BT_QUAD) {
		return (0.0);
	}

	/*
	 * At this point we know we can overlap.  The offset has to be bigger
	 * if the groups are of different size, or if overlapping note(s) have
	 * parentheses.
	 */
	if (allsmall(gs1_p, gs1_p) == allsmall(gs2_p, gs2_p)) {
		offset = 0.50 * STEPSIZE;
	} else {
		offset = 0.75 * STEPSIZE;
	}
	if (parens) {
		offset += STEPSIZE / 2.0;
	}
	return (offset);
}

/*
 * Name:        apply_tsp()
 *
 * Abstract:    Apply tie/slur padding to the group's RE if needed.
 *
 * Returns:     void
 *
 * Description: This function finds out if additional space is needed on the
 *		east side of the group to make space for ties or slurs.  If
 *		so, it extends the RE of the group.
 */

void
apply_tsp(mll_p, staff_p, gs_p)

struct MAINLL *mll_p;		/* the MLL item that is our staff */
struct STAFF *staff_p;		/* the staff our group is on */
struct GRPSYL *gs_p;		/* the group to deal with */

{
	float max_horz;		/* max notehorz of any note */
	float east;		/* temp variable */
	float pad;		/* padding needed beyond notehorz */
	int k;			/* loop var */


	/* get the RE of the notes, the max of all the notes */
	max_horz = 0.0;
	for (k = 0; k < gs_p->nnotes; k++) {
		east = notehorz(gs_p, &gs_p->notelist[k], RE);
		if (east > max_horz) {
			max_horz = east;
		}
	}

	/* find padding needed beyond the basic amount */
	pad = tieslurpad(mll_p, staff_p, gs_p);

	/* if padding requires more space, extend group boundary */
	if (max_horz + pad > gs_p->c[RE] - gs_p->c[RX]) {
		gs_p->c[RE] = gs_p->c[RX] + max_horz + pad;
	}
}

/*
 * Name:        procgrace()
 *
 * Abstract:    Sets coords for grace groups associated with the given groups.
 *
 * Returns:     void
 *
 * Description: This function loops leftward from the given nongrace group(s),
 *		processing grace groups.  If there really are two groups (gs2_p
 *		is not 0), and both groups have graces, the graces are matched
 *		up between voices according to time.  It calls proc1or2voices
 *		for each each matching pair of graces, and for each grace
 *		individually that doesn't match a grace from the other voice.
 */

static void
procgrace(noteptrs, mll_p, staff_p, gs1_p, gs2_p)

struct NOTEPTRS noteptrs[];	/* array of ptrs to notes to process */
struct MAINLL *mll_p;		/* the MLL item the group is connected to */
struct STAFF *staff_p;		/* the staff the groups are connected to */
struct GRPSYL *gs1_p, *gs2_p;	/* group(s) which may have preceding graces */

{
	int v1_has_grace;	/* are there grace group(s) preceding gs1_p? */
	int v2_has_grace;	/* are there grace group(s) preceding gs2_p? */
	int v1_ticks, v2_ticks;	/* "time" before the normal groups */
	struct GRPSYL *v1g_p, *v2g_p;	/* point at graces */
	float rightwest;	/* west edge of group(s) to the right */
	float center;		/* center line of where grace(s) are placed */


	/*
	 * If we were given grace group(s), this is from a recusive call to
	 * proc1or2voices to handle grace groups. So we have nothing more to do.
	 */
	if (gs1_p->grpvalue == GV_ZERO) {
		return;
	}

	/* it was nongrace */

	/*
	 * If there are graces, RW will get moved leftwards to encompass the
	 * graces.  But we need to remember the original RW too.
	 */
	gs1_p->orig_rw = gs1_p->c[RW];
	if (gs2_p != 0) {
		gs2_p->orig_rw = gs2_p->c[RW];
	}

	/* see if they are preceded by grace groups */
	v1_has_grace = IS_GRACE(gs1_p->prev);
	v2_has_grace = gs2_p != 0 && IS_GRACE(gs2_p->prev);

	/* if neither has graces, we are done */
	if ( ! v1_has_grace && ! v2_has_grace ) {
		return;
	}

	/*
	 * Because grace groups in front of nongrace groups might collide,
	 * and because it's probably the right thing to do anyway, align the
	 * two voices' groups according to "time value".  Graces' fulltime is
	 * zero, but we will just use their basictime.  We can do that because
	 * graces can't have dots and are not affected by tuplets.  We count
	 * time in "ticks" which are in units of the shortest possible note.
	 * We loop right to left, matching groups between the voices.  If one
	 * voice runs out of graces, we keep going until both voices run out.
	 */

	/* find the leftmost west coord of the nongrace group(s) */
	if (gs2_p != 0 && gs2_p->c[RW] < gs1_p->c[RW]) {
		rightwest = gs2_p->c[RW];
	} else {
		rightwest = gs1_p->c[RW];
	}

	/* point at the rightmost graces and set times to where they start */
	v1g_p = gs1_p->prev;
	if (IS_GRACE(v1g_p)) {
		v1_ticks = time2ticks(v1g_p->basictime);
	} else {
		v1_ticks = 0;		/* gs1_p had no graces */
	}
	if (gs2_p == 0) {
		v2g_p = 0;		/* there is no gs2_p, so no graces */
		v2_ticks = 0;
	} else {
		v2g_p = gs2_p->prev;
		if (IS_GRACE(v2g_p)) {
			v2_ticks = time2ticks(v2g_p->basictime);
		} else {
			v2_ticks = 0;	/* gs2_p had no graces */
		}
	}

	/*
	 * Loop right to left, processing the grace(s) at each time value.  Call
	 * proc1or2voices() recursively to do its work.  Then call
	 * finalgroupproc().  Note that restsyl() will not be calling
	 * finalgroupproc(), because graces are not in the chords lists.  We
	 * can do it here, because we will never encounter rests or spaces and
	 * so don't have to deal with those first, like restsyl() does.
	 */
	while (IS_GRACE(v1g_p) || IS_GRACE(v2g_p)) {

		if ( ! IS_GRACE(v2g_p) ||
				(IS_GRACE(v1g_p) && v1_ticks < v2_ticks) ) {
			/* only v1 has a group starting at this time */

			v1g_p->sep_accs = YES;	/* because only 1 group */
			proc1or2voices(mll_p, staff_p, v1g_p,
				(struct GRPSYL *)0, NO);

			(void)finalgroupproc(v1g_p, (struct CHORD *)0);
			if (v1g_p->ho_usage == HO_VALUE) {
				l_warning(v1g_p->inputfile, v1g_p->inputlineno,
				"horizonal offset has no effect on a grace group unless it is paired with another grace group in another voice on the same staff");
			}

			center = rightwest - v1g_p->c[RE];

			/* position relative to the nongraces */
			v1g_p->c[RX] += center;
			v1g_p->c[RW] += center;
			v1g_p->c[RE] += center;

			/* make nongrace's rectangles include their graces */
			gs1_p->c[RW] = v1g_p->c[RW];

			rightwest = v1g_p->c[RW];   /* set up for next loop */

			/* point at prev grace if any, find its start time */
			v1g_p = v1g_p->prev;
			if (IS_GRACE(v1g_p)) {
				v1_ticks += time2ticks(v1g_p->basictime);
			}
		} else if ( ! IS_GRACE(v1g_p) ||
				(IS_GRACE(v2g_p) && v1_ticks > v2_ticks) ) {
			/* only v2 has a group starting at this time */

			v2g_p->sep_accs = YES;	/* because only 1 group */
			proc1or2voices(mll_p, staff_p, v2g_p,
				(struct GRPSYL *)0, NO);

			(void)finalgroupproc(v2g_p, (struct CHORD *)0);
			if (v2g_p->ho_usage == HO_VALUE) {
				l_warning(v2g_p->inputfile, v2g_p->inputlineno,
				"horizonal offset has no effect on a grace group unless it is paired with another grace group in another voice on the same staff");
			}

			center = rightwest - v2g_p->c[RE];

			/* position relative to the nongraces */
			v2g_p->c[RX] += center;
			v2g_p->c[RW] += center;
			v2g_p->c[RE] += center;

			/* make nongrace's rectangles include their graces */
			gs2_p->c[RW] = v2g_p->c[RW];

			rightwest = v2g_p->c[RW];   /* set up for next loop */

			/* point at prev grace if any, find its start time */
			v2g_p = v2g_p->prev;
			if (IS_GRACE(v2g_p)) {
				v2_ticks += time2ticks(v2g_p->basictime);
			}
		} else {
			/* both voices have groups starting at this time */

			/*
			 * Set these the same as procallvoices would have for
			 * nongraces.  The logic is much simpler, since we
			 * know we have two groups, and graces ignore "ho".
			 */
			v1g_p->sep_accs = YES;
			v2g_p->sep_accs = YES;

			/*
			 * contra_accs and finalgroupproc depend on the groups
			 * being linked as if they were in a chord, so do that.
			 */
			v1g_p->gs_p = v2g_p;
			proc1or2voices(mll_p, staff_p, v1g_p, v2g_p,
					contra_accs(v1g_p));

			(void)finalgroupproc(v1g_p, (struct CHORD *)0);

			/*
			 * Set the center line of the graces such that the
			 * grace that sticks out farthest right just touches
			 * the groups that are to the right of here.
			 */
			center = rightwest - MAX(v1g_p->c[RE], v2g_p->c[RE]);

			/* position relative to the nongraces */
			v1g_p->c[RX] += center;
			v2g_p->c[RX] += center;
			v1g_p->c[RW] += center;
			v2g_p->c[RW] += center;
			v1g_p->c[RE] += center;
			v2g_p->c[RE] += center;

			/* make nongrace's rectangles include their graces */
			gs1_p->c[RW] = v1g_p->c[RW];
			gs2_p->c[RW] = v2g_p->c[RW];

			/* set up for next loop */
			rightwest = MIN(v1g_p->c[RW], v2g_p->c[RW]);

			/* point at prev graces if any, find their start times*/
			v1g_p = v1g_p->prev;
			if (IS_GRACE(v1g_p)) {
				v1_ticks += time2ticks(v1g_p->basictime);
			}
			v2g_p = v2g_p->prev;
			if (IS_GRACE(v2g_p)) {
				v2_ticks += time2ticks(v2g_p->basictime);
			}
		}
	}
}

/*
 * Name:	time2ticks()
 *
 * Abstract:	Convert a basictime to units of the shortest supported note.
 *
 * Returns:	the number of these units
 *
 * Description:	Convert basictime to "ticks", where a tick is the length of the
 *		shortest basictime that Mup supports.
 */
static int
time2ticks(time)

int time;		/* basictime */
{
	switch (time) {
	case BT_DBL:
		return MAXBASICTIME * 2;
	case BT_QUAD:
		return MAXBASICTIME * 4;
	case BT_OCT:
		return MAXBASICTIME * 8;
	default:
		return MAXBASICTIME / time;
	}
}

/*
 * Name:        procbunch()
 *
 * Abstract:    Sets relative horizontal coords of note heads, accs, & dots.
 *
 * Returns:     void
 *
 * Description: This function figures out which note heads in the given
 *		group(s) need to be put on the "wrong" side of the stem to
 *		avoid overlapping.  Then it sets all note heads' horizontal
 *		coords.  It calls doaccparen() to find and store the positions
 *		for the accidentals, dodot() for the dots.  It sets RW and
 *		RE for the group(s), also taking flags into consideration.
 */

/*
 * This macro checks the n'th structure in noteptrs.  If the top group has
 * a note there, it returns a pointer to that note, else it returns the
 * bottom pointer, which may or may not be 0.
 */
#define	GETPTR(n)	(noteptrs[n].top_p != 0 ?		\
			noteptrs[n].top_p : noteptrs[n].bot_p)

static void
procbunch(noteptrs, mll_p, staff_p, gs1_p, gs2_p)

struct NOTEPTRS noteptrs[];	/* array of ptrs to notes to process */
struct MAINLL *mll_p;		/* the MLL item the group is connected to */
struct STAFF *staff_p;		/* the staff the groups are connected to */
struct GRPSYL *gs1_p, *gs2_p;	/* point at group(s) in this hand */

{
	int normhead[MAXHAND + 1];	/* position of note heads */
	float gwide;			/* width of any note in these groups */
	float nwide;			/* width of a particular note */
	float maxwide;			/* max of gwide for the two groups */
	float ghigh;			/* height of any note in these groups*/
	float nhigh;			/* height of a particular note */
	float g1wide, g2wide;		/* gwide for the two groups */
	float maxhigh;			/* max of ghigh for the two groups */
	float flagwidth;		/* width of a flag */
	float rh;			/* relative horizontal of a note */
	int collinear;			/* are the 2 groups' stems collinear? */
	register int k, n;		/* loop variables */
	int size;


	/*
	 * If this is a tablature staff, call a special function to handle it,
	 * and return.  Voices on tab staffs are handled one at a time, so
	 * gs2_p will never be used for them.
	 */
	if (is_tab_staff(staff_p->staffno)) {
		proctab(mll_p, staff_p, gs1_p);
		return;
	}

	collinear = NO;			/* assume not collinear stems */

	/*
	 * "Normal" position of a note head means to the left of the stem
	 * for an upward stem, and right for downward.  When two notes in a
	 * group are on neighboring letters, one of the note heads has to be
	 * in "abnormal" position so that they don't collide.  Shared
	 * note heads must always be in normal position.  (The fact
	 * that no two of them can be on neighboring letters is enforced
	 * when checking for compatibility of groups.)
	 */
	/*
	 * See if there are any shared notes first.
	 */
	for (n = 0; noteptrs[n].top_p != 0; n++) {
		if (noteptrs[n].bot_p != 0)
			break;		/* found a shared note */
	}

	if (noteptrs[n].top_p != 0) {
		/*
		 * There are shared notes, and n indexes to the first one
		 * (starting from the top).  Set this first one to normal.
		 * First work upwards from there, reversing normality
		 * whenever there are neighboring notes, setting back to
		 * normal otherwise.  Then work downwards from there, doing
		 * the same.
		 */
		normhead[n] = YES;
		for (k = n - 1 ; k >= 0; k--) {
			if (noteptrs[k+1].top_p->stepsup ==
			    noteptrs[ k ].top_p->stepsup - 1)
				normhead[k] = ! normhead[k+1];
			else
				normhead[k] = YES;
		}
		for (k = n + 1 ; noteptrs[k].bot_p != 0; k++) {
			if (noteptrs[k-1].bot_p->stepsup ==
			    noteptrs[ k ].bot_p->stepsup + 1)
				normhead[k] = ! normhead[k-1];
			else
				normhead[k] = YES;
		}
	} else {
		/*
		 * There are no shared notes.  It may even be that there's only
		 * one group.  In each group, the note that's opposite the stem
		 * must be normal, and then we go down the list of other notes
		 * in the group, reversing normality whenever there are
		 * neighboring notes, and setting back to normal otherwise.
		 * There's a special concern if the bottom note of the top
		 * group is on the neighboring letter to the top note of the
		 * bottom group, or if it is on the same letter.  In that case,
		 * we want to offset the groups slightly, such that their stems
		 * are collinear, so set that flag.
		 */
		if (STEMSIDE_RIGHT(gs1_p)) {
			normhead[n-1] = YES;	/* bottom note normal */
			for (k = n - 2; k >= 0; k--) {
				if (noteptrs[k+1].top_p->stepsup ==
				    noteptrs[ k ].top_p->stepsup - 1)
					normhead[k] = ! normhead[k+1];
				else
					normhead[k] = YES;
			}
		} else {	/* STEMSIDE_LEFT */
			normhead[0] = YES;	/* top note normal */
			for (k = 1; k < n; k++) {
				if (noteptrs[k-1].top_p->stepsup ==
				    noteptrs[ k ].top_p->stepsup + 1)
					normhead[k] = ! normhead[k-1];
				else
					normhead[k] = YES;
			}
		}

		if (gs2_p != 0) {
			if (STEMSIDE_RIGHT(gs2_p)) {
				/* find the slot beyond the last bottom note */
				for (k = n + 1; noteptrs[k].bot_p != 0; k++) {
				}
				normhead[k-1] = YES;	/* bottom note normal */
				for (k -= 2; k >= n; k--) {
					if (noteptrs[k+1].bot_p->stepsup ==
					    noteptrs[ k ].bot_p->stepsup - 1)
						normhead[k] = ! normhead[k+1];
					else
						normhead[k] = YES;
				}
			} else {	/* STEMSIDE_LEFT */
				normhead[n] = YES;	/* top note normal */
				for (k = n + 1; noteptrs[k].bot_p != 0; k++) {
					if (noteptrs[k-1].bot_p->stepsup ==
					    noteptrs[ k ].bot_p->stepsup + 1)
						normhead[k] = ! normhead[k-1];
					else
						normhead[k] = YES;
				}
			}

			collinear = (noteptrs[n-1].top_p->stepsup <=
				     noteptrs[ n ].bot_p->stepsup + 1);
		}
	}

	/*
	 * Set gwide and ghigh to be the biggest values of any note in the top
	 * group, also storing the width of each note for later use.
	 */
	gwide = ghigh = 0.0;
	for (n = 0; noteptrs[n].top_p != 0; n++) {
		size = size_def2font(noteptrs[n].top_p->notesize);
		nwide = width(noteptrs[n].top_p->headfont, size,
				noteptrs[n].top_p->headchar);
		noteptrs[n].wid = nwide;
		if (nwide > gwide) {
			gwide = nwide;
		}
		nhigh = height(noteptrs[n].top_p->headfont, size,
				noteptrs[n].top_p->headchar);
		if (nhigh > ghigh) {
			ghigh = nhigh;
		}
	}

	/* remember these values, for comparing to the other group (if any) */
	maxwide = g1wide = gwide;	/* widest group so far */
	maxhigh = ghigh;		/* highest group so far */

	if (HAS_STEM_ON_RIGHT(gs1_p)) {
		gs1_p->stemx = gwide / 2;
	} else if (HAS_STEM_ON_LEFT(gs1_p)) {
		gs1_p->stemx = -gwide / 2;
	} else {
		gs1_p->stemx = 0.0;	/* center the imaginary stem */
	}

	for (n = 0; noteptrs[n].top_p != 0; n++) {
		nwide = noteptrs[n].wid;

		if (normhead[n] == YES) {
			/*
			 * The note head is in normal position, so usually its
			 * relative x coord is 0, and west and east are half a
			 * width off.  But if the note is smaller than the
			 * group's max, and there is a stem, and the note is
			 * not shared by the other group, the note needs to
			 * be off center so that it touches the stem.
			 */
			if (nwide != gwide && noteptrs[n].bot_p == 0) {
				if (HAS_STEM_ON_RIGHT(gs1_p)) {
					noteptrs[n].top_p->c[RE] = gwide / 2;
					noteptrs[n].top_p->c[RX] =
							gwide / 2 - nwide / 2;
					noteptrs[n].top_p->c[RW] =
							gwide / 2 - nwide;
				} else if (HAS_STEM_ON_LEFT(gs1_p)) {
					noteptrs[n].top_p->c[RW] = -gwide / 2;
					noteptrs[n].top_p->c[RX] =
							-gwide / 2 + nwide / 2;
					noteptrs[n].top_p->c[RE] =
							-gwide / 2 + nwide;
				}
			} else {
				noteptrs[n].top_p->c[RX] = 0;
				noteptrs[n].top_p->c[RW] = -nwide / 2;
				noteptrs[n].top_p->c[RE] = nwide / 2;
			}
		} else {
			/*
			 * The note head is in abnormal position.  Its relative
			 * x coord, and west and east, depend on which way the
			 * stem is going.  Smaller than normal notes need to
			 * be placed differently regardless of whether stemed.
			 * In all case, adjust by W_NORMAL*POINT, the width of
			 * the stem, so that the note overlays the stem.
			 */
			if (nwide != gwide) {
				if (STEMSIDE_RIGHT(gs1_p)) {
					noteptrs[n].top_p->c[RW] =
						gwide / 2 - W_NORMAL * POINT;
					noteptrs[n].top_p->c[RX] =
						gwide / 2 + nwide / 2
						- W_NORMAL * POINT;
					noteptrs[n].top_p->c[RE] =
						gwide / 2 + nwide
						- W_NORMAL * POINT;
				} else {	/* STEMSIDE_LEFT */
					noteptrs[n].top_p->c[RE] =
						W_NORMAL * POINT - gwide / 2;
					noteptrs[n].top_p->c[RX] =
						W_NORMAL * POINT
						- gwide / 2 - nwide /2;
					noteptrs[n].top_p->c[RW] =
						W_NORMAL * POINT
						- gwide / 2 - nwide;
				}
			} else {
				if (STEMSIDE_RIGHT(gs1_p)) {
					noteptrs[n].top_p->c[RX] =
						nwide - W_NORMAL * POINT;
					noteptrs[n].top_p->c[RW] =
						nwide * 0.5 - W_NORMAL * POINT;
					noteptrs[n].top_p->c[RE] =
						nwide * 1.5 - W_NORMAL * POINT;
				} else {	/* STEMSIDE_LEFT */
					noteptrs[n].top_p->c[RX] =
						W_NORMAL * POINT - nwide;
					noteptrs[n].top_p->c[RW] =
						W_NORMAL * POINT - nwide * 1.5;
					noteptrs[n].top_p->c[RE] =
						W_NORMAL * POINT - nwide * 0.5;
				}
			}
		}
	}

	/*
	 * If there is a bottom group, get note head character width for
	 * it, find where in noteptrs that group starts, then loop through
	 * it, setting coords.  While doing this, set the group's
	 * horizontal coords.
	 */
	g2wide = 0.0;	/* to avoid useless 'used before set' warning */
	if (gs2_p != 0) {
		/* skip by notes that are only in the top group */
		for (n = 0; noteptrs[n].bot_p == 0; n++)
			;
		/*
		 * Set gwide and ghigh to be the biggest values of any note in
		 * the bottom group, also storing the width of each note for
		 * later use.  If the note is shared between groups, the width
		 * has already been stored in noteptrs[].wid, so we don't have
		 * to recalculate it.
		 */
		gwide = ghigh = 0.0;
		for ( ; noteptrs[n].bot_p != 0; n++) {
			size = size_def2font(noteptrs[n].bot_p->notesize);
			if (noteptrs[n].wid == 0.0) {
				nwide = width(noteptrs[n].bot_p->headfont, size,
						noteptrs[n].bot_p->headchar);
				noteptrs[n].wid = nwide;
			} else {
				nwide = noteptrs[n].wid;
			}
			if (nwide > gwide) {
				gwide = nwide;
			}
			nhigh = height(noteptrs[n].bot_p->headfont, size,
					noteptrs[n].bot_p->headchar);
			if (nhigh > ghigh) {
				ghigh = nhigh;
			}
		}
		g2wide = gwide;
		if (HAS_STEM_ON_RIGHT(gs2_p)) {
			gs2_p->stemx = gwide / 2;
		} else if (HAS_STEM_ON_LEFT(gs2_p)) {
			gs2_p->stemx = -gwide / 2;
		} else {
			gs2_p->stemx = 0.0;	/* center the imaginary stem */
		}

		/* if groups have different note head sizes, adjust maxes */
		if (gwide > maxwide)
			maxwide = gwide;
		if (ghigh > maxhigh)
			maxhigh = ghigh;

		for (n = 0; noteptrs[n].bot_p == 0; n++)
			;
		for ( ; noteptrs[n].bot_p != 0; n++) {
			nwide = noteptrs[n].wid;

			if (normhead[n] == YES) {
				/*
				 * The note head is in normal position, so its
				 * relative x coord is 0, and west and east are
				 * half a width off.  But if the note is smaller
				 * than the widest note in the group and there
				 * is a stem, and the note is not shared by the
				 * other group, the note needs to be off center
			 	 * so that it touches the stem.
				 */
				if (nwide != gwide && noteptrs[n].top_p == 0) {
					if (STEMSIDE_RIGHT(gs2_p)) {
						noteptrs[n].bot_p->c[RE] =
							gwide / 2;
						noteptrs[n].bot_p->c[RX] =
							gwide / 2 - nwide / 2;
						noteptrs[n].bot_p->c[RW] =
							gwide / 2 - nwide;
					} else {  /* STEMSIDE_LEFT */
						noteptrs[n].bot_p->c[RW] =
							-gwide / 2;
						noteptrs[n].bot_p->c[RX] =
							-gwide / 2 + nwide / 2;
						noteptrs[n].bot_p->c[RE] =
							-gwide / 2 + nwide;
					}
				} else {
					noteptrs[n].bot_p->c[RX] = 0;
					noteptrs[n].bot_p->c[RW] = -nwide * 0.5;
					noteptrs[n].bot_p->c[RE] = nwide * 0.5;
				}
			} else {
				/*
				 * The note head is in abnormal position.  Its
				 * relative x coord, and west and east, depend
				 * on which way the stem is going.  Smaller
			 	 * than normal notes need to be placed
				 * differently regardless of whether stemmed.
				 */
				if (nwide != gwide) {
					if (STEMSIDE_RIGHT(gs2_p)) {
						noteptrs[n].bot_p->c[RW] =
							gwide / 2 - W_NORMAL * POINT;
						noteptrs[n].bot_p->c[RX] =
							gwide / 2 + nwide / 2
							- W_NORMAL * POINT;
						noteptrs[n].bot_p->c[RE] =
							gwide / 2 + nwide
							- W_NORMAL * POINT;
					} else {  /* STEMSIDE_LEFT */
						noteptrs[n].bot_p->c[RE] =
							W_NORMAL * POINT - gwide / 2;
						noteptrs[n].bot_p->c[RX] =
							W_NORMAL * POINT
							- gwide / 2 - nwide /2;
						noteptrs[n].bot_p->c[RW] =
							W_NORMAL * POINT
							- gwide / 2 - nwide;
					}
				} else {
					if (STEMSIDE_RIGHT(gs2_p)) {
						noteptrs[n].bot_p->c[RX] =
							nwide - W_NORMAL * POINT;
						noteptrs[n].bot_p->c[RW] =
							nwide * 0.5 - W_NORMAL * POINT;
						noteptrs[n].bot_p->c[RE] =
							nwide * 1.5 - W_NORMAL * POINT;
					} else {  /* STEMSIDE_LEFT */
						noteptrs[n].bot_p->c[RX] =
							W_NORMAL * POINT - nwide;
						noteptrs[n].bot_p->c[RW] =
							W_NORMAL * POINT - nwide * 1.5;
						noteptrs[n].bot_p->c[RE] =
							W_NORMAL * POINT - nwide * 0.5;
					}
				}
			}
		}
	}

	/*
	 * Find position of left parentheses around notes, and accidentals if
	 * they are to be applied separately to each group.
	 */
	doaccparen(noteptrs, maxwide / 2, maxhigh / 2, collinear,
			gs1_p->sep_accs);

	/* find position of dots after notes */
	dodot(staff_p, gs1_p, gs2_p, maxwide / 2, collinear);

	/* find position of right parentheses around notes */
	noterparen(noteptrs, gs1_p, gs2_p, maxwide/2, maxhigh/2, collinear);

	/*
	 * Set RX for the group(s) to 0 for now if stems are offset (the
	 * normal case), or to the appropriate value if stems are collinear.
	 * If we only have one group it will thus be set to 0 now, though
	 * later, if there's an incompatible group next to it, this coord
	 * and all others will be adjusted.
	 */
	if (collinear) {
		gs1_p->c[RX] = (W_NORMAL * POINT - maxwide) / 2;
		gs2_p->c[RX] = (maxwide - W_NORMAL * POINT) / 2;
	} else {
		gs1_p->c[RX] = 0;
		if (gs2_p != 0)
			gs2_p->c[RX] = 0;
	}

	/*
	 * Set the western boundaries for the group(s).
	 */
	/*
	 * Init the group's RW to 0.  Then loop through the notes, finding the
	 * westernmost thing associated with a note, and leaving the group's RW
	 * set to that.
	 */
	gs1_p->c[RW] = 0;
	for (k = 0; k < gs1_p->nnotes; k++) {
		rh = notehorz(gs1_p, &gs1_p->notelist[k], RW);
		if (rh < gs1_p->c[RW])
			gs1_p->c[RW] = rh;
	}
	/*
	 * If the stem is down on a half note or shorter that is to have
	 * slashes through its stem, make sure there is room for the slashes.
	 */
	if (gs1_p->slash_alt > 0 && HAS_STEM_ON_LEFT(gs1_p)) {
		gwide = g1wide;
		/* if position of stem minus slash room < current west . . . */
		if (-gwide / 2 - SLASHPAD < gs1_p->c[RW])
			gs1_p->c[RW] = -gwide / 2 - SLASHPAD;
	}
	westwith(gs1_p);		/* expand RW for "with" list if needbe*/
	gs1_p->c[RW] -= gs1_p->padding;	/* add user requested padding */

	/* add the pad parameter that user wants for this voice */
	gs1_p->c[RW] -= vvpath(gs1_p->staffno, gs1_p->vno, PAD)->pad;

	csbstempad(mll_p, gs1_p);	/* cross staff beaming may need space */
	gs1_p->c[RW] += gs1_p->c[RX];	/* shift by RX, in case RX isn't 0 */

	/* if group 2 exists, do the same for it */
	if (gs2_p != 0) {
		gs2_p->c[RW] = 0;
		for (k = 0; k < gs2_p->nnotes; k++) {
			rh = notehorz(gs2_p, &gs2_p->notelist[k], RW);
			if (rh < gs2_p->c[RW])
				gs2_p->c[RW] = rh;
		}
		if (gs2_p->slash_alt > 0 && HAS_STEM_ON_LEFT(gs2_p)) {
			gwide = g2wide;
			/* if pos of stem minus slash room < current west . .*/
			if (-gwide / 2 - SLASHPAD < gs2_p->c[RW])
				gs2_p->c[RW] = -gwide / 2 - SLASHPAD;
		}
		westwith(gs2_p);
		gs2_p->c[RW] -= gs2_p->padding;
		gs2_p->c[RW] -= vvpath(gs2_p->staffno, gs2_p->vno, PAD)->pad;
		csbstempad(mll_p, gs2_p);
		gs2_p->c[RW] += gs2_p->c[RX];
	}

	/*
	 * Set the eastern boundaries for the group(s).
	 */
	/*
	 * Init the group's RE to 0.  Then loop through the notes, finding the
	 * easternmost thing associated with a note, and leaving the group's RE
	 * set to that.
	 */
	gs1_p->c[RE] = 0;
	for (k = 0; k < gs1_p->nnotes; k++) {
		rh = notehorz(gs1_p, &gs1_p->notelist[k], RE);
		if (rh > gs1_p->c[RE])
			gs1_p->c[RE] = rh;
	}
	if (gs1_p->slash_alt < 0 && gs1_p->beamloc == STARTITEM)
		gs1_p->c[RE] += ALTPAD;
	/*
	 * If the stem is up and a flag is needed, and the east boundary
	 * doesn't yet contain it, adjust the east boundary so the flag will
	 * fit.
	 */
	if (gs1_p->stemdir == UP && gs1_p->basictime >= 8 &&
				gs1_p->beamloc == NOITEM) {
		flagwidth = width(FONT_MUSIC, size_def2font(gs1_p->grpsize),
			C_UPFLAG);
		if (gs1_p->notelist[0].c[RE] + flagwidth > gs1_p->c[RE])
			gs1_p->c[RE] = gs1_p->notelist[0].c[RE] + flagwidth;
	}
	/*
	 * If a note that has a stem on the right is to have slashes
	 * through its stem, make sure there's room for the slashes.
	 */
	if (gs1_p->slash_alt > 0 && HAS_STEM_ON_RIGHT(gs1_p)) {
		gwide = g1wide;
		/* if position of stem plus slash room > current east . . . */
		if (gwide / 2 + SLASHPAD > gs1_p->c[RE])
			gs1_p->c[RE] = gwide / 2 + SLASHPAD;
	}
	/*
	 * Expand RE some more if need be to accommodate the "with" list.  Then
	 * shift it over by RX, in case RX isn't 0.
	 */
	eastwith(gs1_p);
	gs1_p->c[RE] += gs1_p->c[RX];

	/* if group 2 exists, do the same for it */
	if (gs2_p != 0) {
		gs2_p->c[RE] = 0;
		for (k = 0; k < gs2_p->nnotes; k++) {
			rh = notehorz(gs2_p, &gs2_p->notelist[k], RE);
			if (rh > gs2_p->c[RE])
				gs2_p->c[RE] = rh;
		}
		if (gs2_p->slash_alt < 0 && gs2_p->beamloc == STARTITEM)
			gs2_p->c[RE] += ALTPAD;
		eastwith(gs2_p);
		gs2_p->c[RE] += gs2_p->c[RX];

		if (gs2_p->stemdir == UP && gs2_p->basictime >= 8 &&
					gs2_p->beamloc == NOITEM) {
			flagwidth = width(FONT_MUSIC,
				size_def2font(gs2_p->grpsize), C_UPFLAG);
			if (gs2_p->notelist[0].c[RE] + flagwidth > gs2_p->c[RE])
				gs2_p->c[RE] =
					gs2_p->notelist[0].c[RE] + flagwidth;
		}

		if (gs2_p->slash_alt > 0 && HAS_STEM_ON_RIGHT(gs2_p)) {
			gwide = g1wide;
			/* if position of stem plus slash room > current east */
			if (gwide / 2 + SLASHPAD > gs2_p->c[RE])
				gs2_p->c[RE] = gwide / 2 + SLASHPAD;
		}
	}
}

/*
 * Name:        doaccparen()
 *
 * Abstract:    Finds horizontal position for accidentals/parens in group(s).
 *
 * Returns:     void
 *
 * Description: If sep_accs == YES, this function determines (for each note)
 *		the horizontal positioning of 1) its accidental if any
 *		(including any parentheses around the acc, which are treated as
 *		part of the acc), and 2) the left parenthesis of the note if
 *		there are parens around the note.  It figures out where to place
 *		them to avoid overlap, and stores the relative west coord of
 *		each in the NOTE structure.  If sep_accs == NO, it does only
 *		the notes' parens; and the accs are done later by applyaccs().
 *		It uses the appropriate size of accidentals (based on normal
 *		versus cue/grace), and places them appropriately, considering
 *		also the size of the notes.  However, if there are two groups,
 *		the note head sizes could be different.  The halfwide and
 *		halfhigh passed in are supposed to be the right size for the
 *		bigger of the two sizes, and accidentals will not be packed
 *		as tightly against the other notes.  This doesn't hurt, and
 *		isn't worth the trouble to do it "right".
 */

/* this fudge factor prevents roundoff error from causing overlap */
#define	FUDGE		(.01)

/* when CSS applies to a note or acc, move it by this much */
#define CSS_OFF		(CSS_STEPS * STEPSIZE)

/* code to tell stackleft() what it should stack */
#define	SL_ACC		(1 << 0)
#define	SL_PAREN	(1 << 1)

static void
doaccparen(noteptrs, halfwide, halfhigh, collinear, sep_accs)

struct NOTEPTRS noteptrs[];	/* array of ptrs to notes to process */
double halfwide;		/* half of max of width & height of (notes */
double halfhigh;		/*  in group 1, notes in group 2) */
int collinear;			/* are stems collinear? */
int sep_accs;			/* put accs on each group individually? */

{
	struct NOTE *note_p;		/* point at a note */
	float west;
	float parenwidth;		/* width of note's left parenthesis */
	int found;			/* accs/parens found so far */
	int what;			/* what to stack during this call */
	int stackwhat;			/* what to stack for this note */
	int k;				/* loop through notes needing stacking*/


	Reclim = 0;			/* table initially empty */

	/*
	 * Loop through noteptrs, finding all notes that are left of normal
	 * position, entering them in Rectab.  Include padding around them.
	 * First loop through all notes, finding ones that are on the left
	 * side of a down stem; then, if stems are collinear, loop through
	 * the top group, finding all normal notes.
	 */
	for (k = 0; (note_p = GETPTR(k)) != 0; k++) {
		if (note_p->c[RX] < 0) {
			Rectab[Reclim].n = note_p->c[RY] + halfhigh + STDPAD;
			Rectab[Reclim].s = note_p->c[RY] - halfhigh - STDPAD;
			Rectab[Reclim].e = note_p->c[RE] + STDPAD;
			Rectab[Reclim].w = note_p->c[RW] - STDPAD;
			css_alter_vert(note_p);
			inc_reclim();
		}
	}
	if (collinear) {
		for (k = 0; (note_p = noteptrs[k].top_p) != 0; k++) {
			if (note_p->c[RX] == 0) {
				Rectab[Reclim].n = note_p->c[RY] + halfhigh
						+ STDPAD;
				Rectab[Reclim].s = note_p->c[RY] - halfhigh
						- STDPAD;
				Rectab[Reclim].e = W_NORMAL * POINT
						- halfwide + STDPAD;
				Rectab[Reclim].w = W_NORMAL * POINT
						- 3 * halfwide - STDPAD;
				css_alter_vert(note_p);
				inc_reclim();
			}
		}
	}

	/*
	 * If accidentals are to be stacked separately onto each group, we need
	 * to do both parentheses and accidentals now.  Otherwise we do only
	 * parens, and accs will be done later, after all the groups have been
	 * positioned.
	 */
	what = sep_accs ? (SL_ACC | SL_PAREN) : SL_PAREN;

	/*
	 * Loop through all notes, finding the ones with accs and/or parens,
	 * whatever we're dealing with now.  Find where they will fit, storing
	 * that info in waccr, and adding them to Rectab.  Call a function so
	 * that we loop in the proper order.
	 */
	for (found = 0, k = nextaccparen(noteptrs, what, found);  k != -1;
			found++, k = nextaccparen(noteptrs, what, found)) {
		note_p = GETPTR(k);

		/* set stackwhat to what this note has that should be stacked*/
		stackwhat = 0;
		/* always stack paren, if note has one */
		if (note_p->note_has_paren == YES) {
			stackwhat |= SL_PAREN;
		}
		/* stack accs, if note has them and we're doing them this pass*/
		if (has_accs(note_p->acclist) && (what & SL_ACC) != 0) {
			stackwhat |= SL_ACC;
		}

		/* stack whatever we should for this note */
		west = stackleft(note_p, -halfwide - STDPAD, &parenwidth,
				stackwhat);

		/*
		 * If this note exists in the top group, set coords for the
		 * paren and/or acc, whichever exist.  Notice that parenwidth
		 * will be 0 if there is no paren.
		 */
		if (noteptrs[k].top_p != 0) {
			if ((stackwhat & SL_PAREN) != 0) {
				noteptrs[k].top_p->wlparen = west;
			}
			if ((stackwhat & SL_ACC) != 0) {
				noteptrs[k].top_p->waccr = west + parenwidth;
			}
		}

		/* same if the note exists in the bottom group */
		if (noteptrs[k].bot_p != 0) {
			if ((stackwhat & SL_PAREN) != 0) {
				noteptrs[k].bot_p->wlparen = west;
			}
			if ((stackwhat & SL_ACC) != 0) {
				noteptrs[k].bot_p->waccr = west + parenwidth;
			}
		}

	} /* end of loop for each accidental and/or note paren */

	/*
	 * Finally, if the stems were collinear, we have to adjust waccr for
	 * all the notes of the top group, so that it's relative to the top
	 * group instead of the bottom group.
	 */
	if (collinear) {
		for (k = 0; noteptrs[k].top_p != 0; k++) {
			if (noteptrs[k].top_p->note_has_paren == YES)
				noteptrs[k].top_p->wlparen += 2 * halfwide
					 	- W_NORMAL * POINT;
			if (has_accs(noteptrs[k].top_p->acclist) &&
			    (what & SL_ACC) != 0)
				noteptrs[k].top_p->waccr += 2 * halfwide
					 	- W_NORMAL * POINT;
		}
	}
}

/*
 * Name:	nextaccparen()
 *
 * Abstract:	Find the next note that has an acc or paren to be processed.
 *
 * Returns:	Index to the NOTE, or -1 if no more.
 *
 * Description:	This function is called by doaccparen(), to return in the
 *		correct order the notes that have accidentals and/or parens to
 *		be processed.  The "what" parameter says which or these (or
 *		both) we are looking for.  The first time in here, found is 0,
 *		and it looks for the first eligible note (top down).  The next
 *		time, found is 1, and it looks for the bottom-most eligible
 *		note.  After that, it goes through the inner notes, top down.
 *		In the great majority of cases, this will result in the most
 *		desirable packing of accidentals, and it's probably a good way
 *		to order parens too.
 */

static int
nextaccparen(noteptrs, what, found)

struct NOTEPTRS noteptrs[];	/* array of ptrs to notes to process */
int what;			/* what are we looking for (acc and/or paren)*/
int found;			/* no. of accidentals found already */

{
	struct NOTE *note_p;	/* point at a note */
	static int previdx;	/* idx to note chosen the last time in here */
	static int lastidx;	/* idx to the bottommost note chosen */
	int n;			/* loop counter */


	/*
	 * If this is the first call for this group(s), find the topmost
	 * eligible note.
	 */
	if (found == 0) {
		for (n = 0; (note_p = GETPTR(n)) != 0; n++) {
			if (((what & SL_ACC) != 0 &&
					has_accs(note_p->acclist)) ||
			    ((what & SL_PAREN) != 0 &&
					note_p->note_has_paren == YES)) {
				previdx = n;	/* remember it for next time */
				return (n);
			}
		}
		return (-1);	/* no notes have acc or parens */
	}

	/*
	 * If this is the second call, find the bottom of the list, then look
	 * backwards for the last eligible note.  Stop before finding the first
	 * note again.
	 */
	if (found == 1) {
		/* find the slot beyond the last note */
		for (n = 0; (note_p = GETPTR(n)) != 0; n++) {
			;
		}
		/* search from last note going backwards */
		for (n-- ; n > previdx; n--) {
			note_p = GETPTR(n);
			if (((what & SL_ACC) != 0 &&
					has_accs(note_p->acclist)) ||
			    ((what & SL_PAREN) != 0 &&
					note_p->note_has_paren == YES)) {
				lastidx = n;	/* remember it for next time */
				return (n);
			}
		}
		return (-1);	/* only 1 note has acc or parens */
	}

	/*
	 * Third or later call:  Scan inner notes top to bottom.
	 */
	for (n = previdx + 1; n < lastidx; n++) {
		note_p = GETPTR(n);
		if (((what & SL_ACC) != 0   && has_accs(note_p->acclist)) ||
		    ((what & SL_PAREN) != 0 && note_p->note_has_paren == YES)) {
			previdx = n;
			return (n);
		}
	}
	return (-1);		/* all eligible notes were already found */
}

/*
 * Name:        stackleft()
 *
 * Abstract:    Stack note's acc and/or paren onto the left side of a baseline.
 *
 * Returns:     RW of the leftmost item stacked (relative to the chord).
 *
 * Description: This function is given a note that has a paren and/or acc that
 *		is to be stacked.  Only the items indicated by "stackwhat" are
 *		stacked.  The stacking goes leftward from the given "base"
 *		line.   *parenwidth_p gets set to the width of the paren if a
 *		paren is being stacked, otherwise zero.
 */

double
stackleft(note_p, base, parenwidth_p, stackwhat)

struct NOTE *note_p;		/* note whose acc and/or paren to stack */
double base;			/* the base line, relative to the chord */
float *parenwidth_p;		/* this will be set */
int stackwhat;			/* what items are to be stacked? */

{
	float north, south, east, west;	/* relative coords of new accidental */
	float accasc, accdesc;		/* ascent & descent of accidental */
	float accwidth;			/* width of new accidental */
	float parenv;			/* half the vertical size of paren */
	float totwidth;			/* width of acc plus paren */
	int overlap;			/* does our acc overlap existing ones*/
	int try;			/* which element of Rectab to try */
	int j;				/* loop variable */
	int size;			/* font size of paren */
	float horfn, verfn;		/* horz & vert flat/nat notch sizes */
	float savehorfn;		/* save original horfn */
	char std_acc;			/* standard acc, one of the five */


	/* prevent false "may be used before set" lint warning */
	verfn = savehorfn = 0.0;

	/* get dimensions of accidental if there is one */
	if ((stackwhat & SL_ACC) != 0) {
		accdimen(note_p, &accasc, &accdesc, &accwidth);
	} else {
		accwidth = accasc = accdesc = 0.0;
	}

	/* get dimensions of note's left paren, if there is one */
	if ((stackwhat & SL_PAREN) != 0) {
		size = size_def2font(note_p->notesize);
		*parenwidth_p = width(FONT_TR, size, '(');
		parenv = height(FONT_TR, size, '(') / 2.0;
	} else {
		*parenwidth_p = parenv = 0.0;
	}

	/* set the north, south, and width of what we have found */
	north = note_p->c[RY] + MAX(accasc, parenv);
	south = note_p->c[RY] - MAX(accdesc, parenv);
	if (note_p->stepsup >= CSS_STEPS / 2) {	/* adjust like css_alter_vert */
		north += CSS_OFF;
		south += CSS_OFF;
	} else if (note_p->stepsup <= -CSS_STEPS / 2) {
		north -= CSS_OFF;
		south -= CSS_OFF;
	}
	totwidth = accwidth + *parenwidth_p;

	/*
	 * For each rectangle in Rectab, decide whether (based on its vertical
	 * coords) it could possibly overlap with our new accidental.  If it's
	 * totally above or below ours, it can't.  We allow a slight overlap
	 * (FUDGE) so that round off errors don't stop us from packing things
	 * as tightly as possible.
	 */
	for (j = 0; j < Reclim; j++) {
		if (Rectab[j].s + FUDGE > north || Rectab[j].n < south + FUDGE){
			Rectab[j].relevant = NO;
		} else {
			Rectab[j].relevant = YES;
		}
	}

	/*
	 * Mark that none of the relevant rectangles' boundaries have been
	 * tried yet for positioning our acc.
	 */
	for (j = 0; j < Reclim; j++) {
		if (Rectab[j].relevant == YES) {
			Rectab[j].tried = NO;
		}
	}

	/*
	 * Set up first trial position for this acc, just to the left of normal
	 * notes, allowing padding.
	 */
	east = base;
	west = east - totwidth;

	/*
	 * Keep trying positions for this acc, working right to left.  When we
	 * find one that doesn't overlap an existing rectangle, break.  This
	 * has to succeed at some point, at the leftmost rectangle position if
	 * not earlier.
	 */
	for (;;) {
		overlap = NO;
		for (j = 0; j < Reclim; j++) {
			/* ignore ones too far north or south */
			if (Rectab[j].relevant == NO) {
				continue;
			}

			/* if all west or east, okay; else overlap */
			if (Rectab[j].w + FUDGE <= east &&
			    Rectab[j].e >= west + FUDGE) {
				overlap = YES;
				break;
			}
		}

		/* if no rectangle overlapped, we found a valid place*/
		if (overlap == NO) {
			break;
		}

		/*
		 * Something overlapped, so we have to try again.  Find the
		 * eastermost relevant west rectangle boundary that hasn't been
		 * tried already, and whose west is not to the east of the
		 * current trial east, to use as the next trial position for
		 * our acc's east.
		 */
		try = -1;
		for (j = 0; j < Reclim; j++) {
			/* ignore ones too far north or south */
			if (Rectab[j].relevant == NO || Rectab[j].tried == YES){
				continue;
			}

			/* ignore ones east of where we already are */
			if (Rectab[j].w > east) {
				continue;
			}

			/*
			 * If this is the first eligible one we haven't tried,
			 * or if this is farther east than the easternmost so
			 * far, save it as being the new easternmost so far.
			 */
			if (try == -1 || Rectab[j].w > Rectab[try].w) {
				try = j;
			}
		}

		if (try == -1) {
			pfatal("bug in stackleft()");
		}

		/*
		 * Mark this one as having been tried (for next time around, if
		 * necessary).  Set new trial values for east and west of our
		 * acc.
		 */
		Rectab[try].tried = YES;
		east = Rectab[try].w;
		west = east - totwidth;

	} /* end of while loop trying positions for this acc */

	/*
	 * We found the correct position for the new acc.  However, for flats,
	 * double flats, and naturals, we would like a notch to be taken out of
	 * the upper right corner of their rectangle, in effect, since there's
	 * nothing there but white space.  This can only be done if the acc is
	 * not already right next to the group.
	 */
	std_acc = standard_acc(note_p->acclist);
	if (std_acc == '&' || std_acc == 'B' || std_acc == 'n') {
		/* get notch size; if paren, add width to horz */
		if (std_acc == 'n') {
			horfn = 1.4 * STEPSIZE;	/* horizontal notch */
			verfn = 1.6 * STEPSIZE;	/* vertical notch */
		} else {
			horfn = 1.5 * STEPSIZE;	/* horizontal notch */
			verfn = 2.8 * STEPSIZE;	/* vertical notch */
		}
		if (note_p->notesize == GS_SMALL) {
			horfn *= SM_FACTOR;
			verfn *= SM_FACTOR;
		}
		if (note_p->acc_has_paren) {
			size = size_def2font(note_p->notesize);
			horfn += width(FONT_TR, size, ')');
		}
		savehorfn = horfn;	/* may need it later */
		/*
		 * If notch width is bigger than the max possible dist we could
		 * move the acc (we would overwrite the note), reduce it to be
		 * the space available.
		 */
		if (horfn > - east + base) {
			horfn = - east + base;
		}

		/* only attempt the shift if > 0 width available */
		if (horfn > 0.0) {
			/*
			 * The useable notch size is horfn by verfn.  We'd like
			 * to move the acc to the right by horfn.  We can only
			 * do this if the space is unoccupied that is immedi-
			 * ately to the right of the acc, of width = horfn and
			 * height = (height of acc) - verfn.  (If only part of
			 * that space is available, we'll make one more try,
			 * see below.)  So check whether any existing rectangle
			 * overlaps that space.
			 */
			overlap = NO;
			for (j = 0; j < Reclim; j++) {
				if (Rectab[j].s + FUDGE <= north - verfn &&
				    Rectab[j].n - FUDGE >= south &&
				    Rectab[j].w + FUDGE <= east + horfn &&
				    Rectab[j].e - FUDGE >= east) {
					overlap = YES;
					break;
				}
			}
			/*
			 * If the space is free, move the acc to the
			 * right by HORFN.
			 */
			if (overlap == NO) {
				west += horfn;
				east += horfn;
			} else {
				/*
				 * All right, let's try again with 1/2 of the
				 * previous horfn.
				 */
				horfn /= 2.0;
				overlap = NO;
				for (j = 0; j < Reclim; j++) {
					if (Rectab[j].s + FUDGE <= north - verfn &&
					    Rectab[j].n - FUDGE >= south &&
					    Rectab[j].w + FUDGE <= east + horfn &&
					    Rectab[j].e - FUDGE >= east) {
						overlap = YES;
						break;
					}
				}
				if (overlap == NO) {
					west += horfn;
					east += horfn;
				}
			}
		}
	}

	/*
	 * We have the final position for the new acc.  Enter it into Rectab.
	 * But for naturals, we don't want to reserve the lower left corner,
	 * where there is nothing but white space; so in that case, put two
	 * overlapping entries in Rectab to account for the rest of the space.
	 * Naturals are symmetrical, so we can use the same horfn and verfn as
	 * were calculated above for the upper right corner.
	 */
	if (std_acc == 'n') {
		/* upper part of natural */
		Rectab[Reclim].n = north;
		Rectab[Reclim].s = south + verfn;
		Rectab[Reclim].e = east;
		Rectab[Reclim].w = west;
		inc_reclim();

		/* right hand part of natural */
		Rectab[Reclim].n = north;
		Rectab[Reclim].s = south;
		Rectab[Reclim].e = east;
		Rectab[Reclim].w = west + savehorfn;
	} else {
		/* some other accidental; reserve the whole rectangle*/
		Rectab[Reclim].n = north;
		Rectab[Reclim].s = south;
		Rectab[Reclim].e = east;
		Rectab[Reclim].w = west;
	}
	inc_reclim();

	return (west);
}

/*
 * Name:        dodot()
 *
 * Abstract:    Finds horizontal and vertical positions of dots.
 *
 * Returns:     void
 *
 * Description: This function figures out the limitations on where dots
 *		can be put, for each group, and calls dogrpdot() for each
 *		group that has dots, to figure their positions.
 */

static void
dodot(staff_p, gs1_p, gs2_p, halfwide, collinear)

struct STAFF *staff_p;		/* the staff the groups are connected to */
register struct GRPSYL *gs1_p, *gs2_p;	/* point at group(s) in this hand */
double halfwide;			/* half of max of width of notes */
int collinear;				/* are stems collinear? */

{
	/* the highest and lowest values of steps above the middle staff */
	/* line that a dot is allowed to be for the given group */
	int uppermost, lowermost;

	int lowtopidx;		/* index to lowest note of top group */
	int push;		/* steps to protruding note */
	register int k;		/* loop variable */


	lowtopidx = gs1_p->nnotes - 1;	/* for convenience */

	/*
	 * For each group that needs dots, set the outer limits of where
	 * they are allowed.  If the other group doesn't need dots, we
	 * have to be careful to keep them out of its way.  Otherwise,
	 * don't worry about that; let them fall on top of each other if
	 * that would happen.
	 */

	/*
	 * If the first group needs dots, find out how high and low they are
	 * allowed to be.  Also find out if nearby notes in the other group
	 * could be in the way of dots.  Call dogrpdot() with this info to
	 * find their positions.
	 */
	if (gs1_p->dots > 0) {
		/* upper limit is always as described above */
		uppermost = gs1_p->notelist[0].stepsup;
		if (uppermost % 2 == 0)		/* line note */
			uppermost++;

		/* set lower limit as if no other group */
		lowermost = gs1_p->notelist[lowtopidx].stepsup;
		if (lowermost % 2 == 0)		/* line note */
			lowermost--;

		/* but adjust if the other group exists & would interfere */
		if ((gs2_p != 0 && gs2_p->dots == 0) || collinear) {
			if (lowermost <= gs2_p->notelist[0].stepsup)
				lowermost += 2;
		}

		/*
		 * If the stems are collinear, bottom group notes that are
		 * in normal position for that group protrude to the right
		 * relative to the top group.  From top down, search for notes
		 * in the bottom group that are like this.  Set push to the
		 * first one.  If none are found, let push be 1000 to be out of
		 * the way.  In setting horizontal dot positions, dogrpdot()
		 * needs to know this.
		 */
		push = 1000;
		if ( gs2_p != 0 && collinear ) {
			for (k = 0; k < gs2_p->nnotes; k++) {
				if (gs2_p->notelist[k].c[RX] == 0) {
					push = gs2_p->notelist[k].stepsup;
					break;
				}
			}
		}

		/* do top group's dots */
		dogrpdot(staff_p, gs1_p, (struct GRPSYL *)0, halfwide,
				uppermost, lowermost, push);
	}

	/*
	 * If the second group exists and needs dots, find out how high and
	 * low they are allowed to be, and find their positions.
	 */
	if (gs2_p != 0 && gs2_p->dots > 0) {
		/* set upper limit as if no other group */
		uppermost = gs2_p->notelist[0].stepsup;
		if (uppermost % 2 == 0)		/* line note */
			uppermost++;

		/* but adjust if the other group would interfere */
		if (gs1_p->dots == 0 || collinear) {
			if (uppermost >= gs1_p->notelist[lowtopidx].stepsup)
				uppermost -= 2;
		}

		/* lower limit is always as described above */
		lowermost = gs2_p->notelist[ gs2_p->nnotes - 1 ].stepsup;
		if (lowermost % 2 == 0)		/* line note */
			lowermost--;

		/*
		 * Unless the stems are collinear, in which case no problem,
		 * from bottom up, search for notes in the top group that
		 * protrude towards the right.  Set push to the first one.
		 * If none are found, let push be 1000 to be out of the way.
		 * In setting horizontal dot positions, dogrpdot() needs to
		 * know this.
		 */
		push = 1000;
		if ( ! collinear ) {
			for (k = lowtopidx; k >= 0; k--) {
				if (gs1_p->notelist[k].c[RX] > 0) {
					push = gs1_p->notelist[k].stepsup;
					break;
				}
			}
		}

		/* do bottom group's dots */
		dogrpdot(staff_p, gs2_p, gs1_p, halfwide, uppermost, lowermost,
				push);
	}
}

/*
 * Name:        dogrpdot()
 *
 * Abstract:    Finds horizontal and vertical positions of dots for one group.
 *
 * Returns:     void
 *
 * Description: This function loops through all the notes belonging to the
 *		given group, setting the coords of the dots relative to it.
 */

/* recover dotsteps from ydotr, avoiding roundoff error */
#define	DOTSTEPS(ydotr)	(				\
	ydotr > 0.0 ?					\
		(int)((ydotr + 0.001) / STEPSIZE)	\
	:						\
		-(int)((-ydotr + 0.001) / STEPSIZE)	\
)

static void
dogrpdot(staff_p, gs_p, ogs_p, halfwide, uppermost, lowermost, push)

struct STAFF *staff_p;		/* the staff the groups are connected to */
register struct GRPSYL *gs_p;	/* point at group */
struct GRPSYL *ogs_p;		/* if we're doing group 1 and 2 together, and
				 * gs_p is group 2, ogs_p is group 1, else 0 */
double halfwide;		/* half of max of width of notes */
int uppermost;			/* highest step where a dot is permitted */
int lowermost;			/* lowest step where a dot is permitted */
int push;			/* avoid protruding note at this position */

{
	float dotwidth;		/* width of a dot (includes padding) */
	int normhorz;		/* use normal horizontal dot position? */
	int notesteps;		/* steps note is above center line of staff */
	int dotsteps;		/* steps dot is above center line of staff */
	register int n, k;	/* loop variables */


	/* until proven otherwise, assume normal horizontal dot position */
	normhorz = YES;

	/*
	 * The rules for vertical positioning of dots are as follows.
	 * For space notes, dots will be put in the same space.  For line
	 * notes we'd like them to be in the space directly above, except for
	 * voice 2 in vscheme=2o,3o or 2f,3f when voice 1 is not space, in
	 * which case we'd like them to be in the space below.  But if notes in
	 * a group are jammed onto neighboring steps, we may need to put some
	 * line note dots on the space below regardless; and we may
	 * even have to let some dots land on top of each other.  But in
	 * any case, never exceed the uppermost/lowermost bounds, which
	 * would interfere with the other group.
	 *
	 * The rules for horizontal positioning of dots are as follows.
	 * If the note on the dot's space, or either neighboring line,
	 * is in abnormal position to the right, the dot must be put
	 * farther right than normal.  The parameter "push" is the nearest
	 * note from the other group that protrudes this way.  And the dots
	 * of all the notes have to line up, so if any one has this problem,
	 * they must all be moved.
	 */

	/*
	 * Loop through all notes in the group, setting dot positions.  At
	 * the top of the loop, "dotsteps" is the previous dot, but by the
	 * end it gets set to the current dot.
	 */
	dotsteps = uppermost + 2;	/* pretend previous dot was here */

	for (n = 0; n < gs_p->nnotes; n++) {

		notesteps = gs_p->notelist[n].stepsup;

		if (notesteps % 2 == 0) {
			/*
			 * This note is on a line.  If the dot cannot be put
			 * above the line, or if doing that would overlay the
			 * previous dot and we are allowed to put it below
			 * the line, then put it below the line.  Else, put
			 * it above the line.  Notice that we're putting the
			 * dot in the space above if at all possible; later on,
			 * we'll make adjustments for voice 2 if appropriate.
			 */
			if (notesteps + 1 > uppermost ||
			   (notesteps + 1 == dotsteps &&
			    notesteps - 1 >= lowermost)) {
				dotsteps = notesteps - 1;
			} else {
				dotsteps = notesteps + 1;
			}
		} else {
			/*
			 * This note is on a space.  The dot must be put in
			 * this same space, regardless of anything else.
			 */
			dotsteps = notesteps;
		}

		/* set relative y coord based on step position */
		gs_p->notelist[n].ydotr = dotsteps * STEPSIZE;

		/*
		 * Now see if this dot forces abnormal positioning.  "Push" may
		 * indicate a protruding note in the other group.  If this
		 * note is within 1 step of our dot, use abnormal positioning
		 * for the dot.  Else if the stem is down, all dots can be
		 * normal.  Else, we have to search for protruding notes to
		 * see where the dot can be.
		 */
		if (normhorz == YES) {
			if (abs(dotsteps - push) <= 1) {
				normhorz = NO;
			} else if (gs_p->stemdir == UP) {
				for (k = 0; k < gs_p->nnotes; k++) {
					notesteps = gs_p->notelist[k].stepsup;

					if (gs_p->notelist[k].c[RE] >halfwide &&
					    notesteps <= dotsteps + 1 &&
					    notesteps >= dotsteps - 1) {

						normhorz = NO;
						break;
					}
				}
			}
		}
	}

	/*
	 * Set horizontal dot positions, relative to the group.  STDPAD is
	 * needed because notehead characters don't include padding.  The
	 * abnormal case adds in one more notehead width, minus the width
	 * of the stem.  Since the dots for all notes line up vertically,
	 * xdotr is in GRPSYL instead of in each NOTE.
	 */
	dotwidth = width(FONT_MUSIC,
			allsmall(gs_p, gs_p) ? SMALLSIZE : DFLT_SIZE, C_DOT);
	gs_p->xdotr = halfwide + STDPAD + dotwidth / 2;
	if (normhorz == NO) {
		gs_p->xdotr += 2 * halfwide - W_NORMAL * POINT;
	}

	/*
	 * If this is voice 2, we may need to adjust the vertical position of
	 * nonshared line notes.  The same should happen if this is voice 3
	 * "standing in" for voice 2.
	 */
	if (gs_p->pvno == 2) {
		int trymove;		/* try to move dots? */
		int vscheme;		/* voice scheme */
		RATIONAL vtime;		/* time so far in this measure */
		int prevdotsteps;	/* Y distance of prev note's dot */
		struct GRPSYL *pgs_p;	/* point along GRPSYL list */
		int onotesteps;		/* lowest note of voice 1 */

		trymove = NO;		/* first assume leave them alone */
		vscheme = svpath(gs_p->staffno, VSCHEME)->vscheme;
		if (vscheme == V_2OPSTEM || vscheme == V_3OPSTEM) {
			/* always try to move if 2o or 3o */
			trymove = YES;
		} else {
			/* 2f or 3f; move iff voice 1 is not all spaces here */
			vtime = Zero;	/* add up time of preceding groups */
			for (pgs_p = gs_p->prev; pgs_p != 0;
					pgs_p = pgs_p->prev) {
				vtime = radd(vtime, pgs_p->fulltime);
			}
			if ( ! hasspace(staff_p->groups_p[0], vtime,
					radd(vtime, gs_p->fulltime))) {
				/* not all space during duration of our group*/
				trymove = YES;
			}
		}

		if (trymove == YES) {
			/*
			 * We need to try to move the dots of line notes from
			 * the space above them to the space below them.  We
			 * will work from bottom to top.  Initially, pretend
			 * that the previous note is way low out of the way.
			 * If a voice 1 group was being handled along with our
			 * group, find the stepsup of its lowest note.
			 */
			prevdotsteps = -1000;
			if (ogs_p != 0) {
				onotesteps = ogs_p->notelist[
						ogs_p->nnotes - 1].stepsup;
			} else {
				onotesteps = 0;	/* for lint; set before used */
			}
			for (n = gs_p->nnotes - 1; n >= 0; n--) {
				notesteps = gs_p->notelist[n].stepsup;
				/*
				 * We want to stop if we run into notes shared
				 * by group 1 if it exists.  ( > is defensive).
				 */
				if (ogs_p != 0 && notesteps >= onotesteps)
					break;
				/*
				 * Recover our dotsteps from our dots coord
				 * calculated earlier in this function.  Then,
				 * consider moving our dot only if we are a
				 * line note and our dot is currently in the
				 * space above.  (It could already be below,
				 * do to tightly packed notes.)
				 */
				dotsteps = DOTSTEPS(gs_p->notelist[n].ydotr);
				if (notesteps % 2 == 0 &&
						dotsteps == notesteps + 1) {
					/*
					 * If the previous (lower) note is at
					 * least 2 steps away, we can certainly
					 * move our dot.  But also move it if
					 * we are the top note of group 2, and
					 * group 1 exists and has a note 2 steps
					 * away, and they don't have a dot at
					 * the same horz position; because our
					 * dot would be confusing if above.  If
					 * it make our dot land on top of the
					 * previous note's dot, tough.
					 */
					if (prevdotsteps < notesteps - 1 ||
					    (n == 0 && ogs_p != 0 &&
					    notesteps + 2 == onotesteps &&
					    ogs_p->xdotr != gs_p->xdotr)) {

						dotsteps -= 2;
						gs_p->notelist[n].ydotr -=
							2.0 * STEPSIZE;
					}
				}
				prevdotsteps = dotsteps;
			}
		}
	}
}

/*
 * Name:        westwith()
 *
 * Abstract:    Adjust west coord of a group to allow for its "with" lists.
 *
 * Returns:     void
 *
 * Description: This function is given a GRPSYL whose relative horizontal
 *		coords are set, relative to the center of the group, except
 *		that "with" lists have not yet been considered.  It alters
 *		gs_p->c[RW] if need be so that the group's rectangle includes
 *		all "with" lists.
 */

static void
westwith(gs_p)

struct GRPSYL *gs_p;		/* point at this group */

{
	int n;			/* loop through the "with" list items */
	int font, size;		/* of the chars in the "with" list item */
	int first_char;		/* first char of string to print */
	char *str_p;		/* point into the item */
	float x_offset;		/* half the width of the first char in item */


	for (n = 0; n < gs_p->nwith; n++) {
		/* should center first character on x */
		font = gs_p->withlist[n].string[0];
		size = gs_p->withlist[n].string[1];
		str_p = gs_p->withlist[n].string + 2;
		first_char = next_str_char(&str_p, &font, &size);
		x_offset = width(font, size, first_char) / 2.0;
		if (-x_offset < gs_p->c[RW])
			gs_p->c[RW] = -x_offset;
	}
}

/*
 * Name:        eastwith()
 *
 * Abstract:    Adjust east coord of a group to allow for its "with" lists.
 *
 * Returns:     void
 *
 * Description: This function is given a GRPSYL whose relative horizontal
 *		coords are set, relative to the center of the group, except
 *		that "with" lists have not yet been considered.  It alters
 *		gs_p->c[RE] if need be so that the group's rectangle includes
 *		all "with" lists.
 */

static void
eastwith(gs_p)

struct GRPSYL *gs_p;		/* point at this group */

{
	int n;			/* loop through the "with" list items */
	int font, size;		/* of the chars in the "with" list item */
	int first_char;		/* first char of string to print */
	char *str_p;		/* point into the item */
	float x_offset;		/* half the width of the first char in item */


	for (n = 0; n < gs_p->nwith; n++) {
		/* should center first character on x */
		font = gs_p->withlist[n].string[0];
		size = gs_p->withlist[n].string[1];
		str_p = gs_p->withlist[n].string + 2;
		first_char = next_str_char(&str_p, &font, &size);
		x_offset = strwidth(gs_p->withlist[n].string) -
				width(font, size, first_char) / 2.0;
		if (x_offset > gs_p->c[RE])
			gs_p->c[RE] = x_offset;
	}
}

/*
 * Name:        csbstempad()
 *
 * Abstract:    Pad a group's RW for cross staff beaming if need be.
 *
 * Returns:     void
 *
 * Description: In cross staff beamed groups, where the beams are between the
 *		staffs, and a note on the bottom staff is followed by a note on
 *		the top staff, and the first note has no dots or anything else
 *		that would force more space after it, and the top note has no
 *		accidentals, graces, or anything that would force more space
 *		before it, the stems of the two groups can be very close
 *		together, too close.  This function checks for that case, and
 *		when found, adds padding to the left of the top group.
 */

static void
csbstempad(mll_p, gs_p)

struct MAINLL *mll_p;		/* the MLL item the group is connected to */
struct GRPSYL *gs_p;		/* point at the top staff's group */

{
	struct GRPSYL *gs2_p;		/* point at various GRPSYLs */
	struct CHORD *ch_p, *pch_p;	/* our chord and preceding chord */
	struct MAINLL *m2_p;		/* loop through MLL */
	int k;				/* loop through notelist */
	int found;			/* have we found our group? */


	/* if this group is not a candidate for this, return */
	if (gs_p->beamto != CS_BELOW)	/* must be CSB beamed with below */
		return;
	if (gs_p->stemdir == UP)	/* stem must be down */
		return;
	if (gs_p->beamloc == STARTITEM)	/* must not be first item in CSB */
		return;
	if (gs_p->prev == 0)		/* (defensive) */
		return;
	if (gs_p->prev->grpcont != GC_SPACE)	/* prev must be a space */
		return;

	/*
	 * The notes should all have the same RW (even cues) unless a note is
	 * on the "wrong" side of the stem, because they are all supposed to
	 * touch the stem.  In the latter case, there's already enough space in
	 * the group to the left of the stem, so return.
	 */
	for (k = 1; k < gs_p->nnotes; k++) {
		if (ABSDIFF(gs_p->notelist[k].c[RW], gs_p->notelist[0].c[RW])
				> FUDGE)
			return;
	}

	/*
	 * If there's anything to the left of the notes' RWs (the stem
	 * position), it should be enough space, so return.
	 */
	if (gs_p->c[RW] < gs_p->notelist[0].c[RW] - STDPAD - FUDGE)
		return;

	/* find the chord headcell for this measure */
	for (m2_p = mll_p->prev; m2_p->str != S_CHHEAD; m2_p = m2_p->prev)
		;
	/*
	 * Loop through the chords.  For each chord, loop through all its
	 * groups, trying to find our group.  It should be found.  At the point
	 * it is found, pch_p will point to the chord preceding the one that
	 * contains our group.
	 */
	found = NO;
	pch_p = 0;	/* to avoid useless 'used before set' warning */
	for (ch_p = m2_p->u.chhead_p->ch_p; ch_p != 0;
				pch_p = ch_p, ch_p = ch_p->ch_p) {
		for (gs2_p = ch_p->gs_p; gs2_p != 0; gs2_p = gs2_p->gs_p) {
			if (gs2_p == gs_p) {
				found = YES;
				break;
			}
		}
		if (found == YES)
			break;
	}
	if (found == NO)	/* defensive; this should never happen */
		return;

	/* find next visible staff after our staff */
	for (m2_p = mll_p->next; m2_p->str == S_STAFF &&
			m2_p->u.staff_p->visible == NO; m2_p = m2_p->next)
		;
	if (m2_p->str != S_STAFF)	/* defensive; should not happen */
		return;

	/*
	 * Loop down the preceding chord, looking for a group that is on the
	 * next visible staff after our staff and is CSB'ed to the staff above.
	 */
	for (gs2_p = pch_p->gs_p; gs2_p != 0; gs2_p = gs2_p->gs_p) {

		if (gs2_p->staffno == m2_p->u.staff_p->staffno &&
				gs2_p->beamto == CS_ABOVE) {
			/*
			 * We found such a group; it must be the only one.
			 * Check that it meets the conditions.
			 */
			if (gs2_p->grpcont != GC_NOTES) {
				return;
			}
			if (gs2_p->stemdir == DOWN)
				return;
			/*
			 * The notes need to all have the same RE, analogous to
			 * the earlier check on gs_p's RW.
			 */
			for (k = 1; k < gs2_p->nnotes; k++) {
				if (ABSDIFF(gs2_p->notelist[k].c[RE], gs2_p->
						notelist[0].c[RE]) > FUDGE)
					return;
			}
			/*
			 * If there's anything to the right of the notes' REs,
			 * there's already enough space.
			 */
			if (gs2_p->c[RE] > gs2_p->notelist[0].c[RE] +
					STDPAD + FUDGE)
				return;

			/*
			 * FINALLY!  We have established the need for more
			 * space.  Append it to our group's RW.
			 */
			gs_p->c[RW] -= STEPSIZE;
			gs_p->padded_csb_stem = YES;	/* remember we did it */
			return;
		}
	}

	/* didn't find one; shouldn't happen, but just return */
}

/*
 * Name:        proctab()
 *
 * Abstract:    Sets relative horizontal coords of fret numbers.
 *
 * Returns:     void
 *
 * Description: This function sets all the horizontal coords of "notes" on a
 *		tablature staff, which are actually fret numbers.  It sets RW
 *		and RE for the group, too.  They also take bends into account.
 */

static void
proctab(mll_p, staff_p, gs_p)

struct MAINLL *mll_p;		/* the MLL item the group is connected to */
struct STAFF *staff_p;		/* the staff the group is connected to */
struct GRPSYL *gs_p;		/* point at this group */

{
	int n;			/* loop through the "notes" in the group */
	float halfwide;		/* half the width of a fret or bend number */
	float maxhalffret;	/* half the max width of a fret number */
	float maxhalfbend;	/* half the max width of a bend number */
	float maxbend;		/* width of a bend number that sticks right */
	struct GRPSYL *prevgs_p;/* point at previous group */
	int center;		/* should bend string be centered? */
	int k;			/* loop variable */


	maxhalffret = 0.0;
	maxhalfbend = 0.0;
	maxbend = 0.0;

	prevgs_p = prevgrpsyl(gs_p, &mll_p);	/* in case we need it */

	/* loop though all frets and bends in this group */
	for (n = 0; n < gs_p->nnotes; n++) {
		/*
		 * If there is a fret, find half the width of that number.  It
		 * should be centered on the center of the group.  Keep track
		 * of the maximum width so far.  Allow 1.5*STDPAD on each side
		 * of the fret number, since we don't ever want the numbers so
		 * close that they look like one number.
		 */
		if (gs_p->notelist[n].FRETNO != NOFRET) {
			halfwide = strwidth(fret_string(&gs_p->notelist[n],
					gs_p)) / 2.0;
			gs_p->notelist[n].c[RX] = 0.0;
			gs_p->notelist[n].c[RE] = halfwide;
			gs_p->notelist[n].c[RW] = -halfwide;
			maxhalffret = MAX(halfwide + 1.5*STDPAD, maxhalffret);
		} else {
			/* no fret, treat as zero width */
			gs_p->notelist[n].c[RX] = 0.0;
			gs_p->notelist[n].c[RE] = 0.0;
			gs_p->notelist[n].c[RW] = 0.0;
			maxhalffret = MAX(1.5*STDPAD, maxhalffret);;
		}

		/*
		 * If there is a bend, figure out if it's the normal situation
		 * (centered on the group's X) or the the case where its left
		 * edge should be at the group's X (the case of a continuation
		 * bend where the previous group's bend was higher).  In the
		 * latter case, the string had to be shifted to avoid colliding
		 * with the arrow coming down from the previous group.
		 */
		if (HASREALBEND(gs_p->notelist[n])) {
			center = YES;	/* first assume normal */

			/* search previous group, if any, for a bend */
			if (prevgs_p != 0) {
				for (k = 0; k < prevgs_p->nnotes; k++) {
					if (HASREALBEND(prevgs_p->notelist[k]))
						break;
				}
				/*
				 * If previous group had a bend and its
				 * distance was higher than the current group,
				 * we have the special case.
				 */
				if (k < prevgs_p->nnotes &&
				    GT( ratbend(&prevgs_p->notelist[k]),
				    ratbend(&gs_p->notelist[n]) ) ) {
					center = NO;
				}
			}
			if (center == YES) {
				/*
				 * Normal case of a bend string: centered at
				 * group's X.  Maintain maxhalfbend as the
				 * the widest so far.
				 */
				halfwide = strwidth(bend_string(
						&gs_p->notelist[n])) / 2.0;
				maxhalfbend = MAX(halfwide, maxhalfbend);
			} else {
				/*
				 * A bend string that has its left edge at the
				 * group's X.  There can only be one such,
				 * since multiple continuation bends are not
				 * allowed (other than releases).
				 */
				maxbend = strwidth(bend_string(
						&gs_p->notelist[n]));
			}
		}
	}

	/*
	 * Set the group's relative horizontal coordinates.  On west, add user
	 * requested padding.  Also adjust for "with" lists.  They can extend
	 * into tie/slur padding, but not into user requested padding.
	 */
	gs_p->c[RX] = 0.0;

	gs_p->c[RW] = -MAX(maxhalffret, maxhalfbend);
	westwith(gs_p);
	gs_p->c[RW] -= gs_p->padding;
	gs_p->c[RW] -= vvpath(gs_p->staffno, gs_p->vno, PAD)->pad;

	gs_p->c[RE] = MAX(MAX(maxhalffret, maxhalfbend), maxbend);
	eastwith(gs_p);
}

/*
 * Name:        noterparen()
 *
 * Abstract:    Finds horizontal position notes' right parentheses.
 *
 * Returns:     void
 *
 * Description: If any of the notes in the given group(s) are to have
 *		parentheses around them, this function finds the horizontal
 *		positions of the right parentheses.  The left ones are done
 *		in doaccparen() or applyaccs().  For each group, it uses
 *		the appropriate size of parentheses (based on normal versus
 *		cue/grace), and places them appropriately, considering also
 *		the size of the notes.  However, if there are two groups,
 *		the note head sizes could be different.  The halfwide and
 *		halfhigh passed in are supposed to be the right size for the
 *		bigger of the two sizes, and accidentals will not be packed
 *		as tightly against the other notes.  This doesn't hurt, and
 *		isn't worth the trouble to do it "right".
 */

static void
noterparen(noteptrs, gs1_p, gs2_p, halfwide, halfhigh, collinear)

struct NOTEPTRS noteptrs[];	/* array of ptrs to notes to process */
struct GRPSYL *gs1_p, *gs2_p;	/* point at group(s) in this hand */
double halfwide;		/* half of max of width & height of (notes */
double halfhigh;		/*  in group 1, notes in group 2) */
int collinear;			/* are stems collinear? */

{
	struct NOTE *note_p;		/* point at a note */
	int parensexist;		/* does any note have parens? */
	float north, south, east, west;	/* relative coords of new paren */
	float parenwidth;		/* width of note's left parenthesis */
	float parenv;			/* half the vertical size of paren */
	float dotoff;			/* additional offset caused by dots */
	float dotoff1, dotoff2;		/* same, for groups 1 and 2 */
	int overlap;			/* does our acc overlap existing ones*/
	int try;			/* which element of Rectab to try */
	int k, j;			/* loop variables */
	int size;


	/*
	 * If no notes have parentheses, we can get out because there is
	 * nothing to do.
	 */
	parensexist = NO;		/* init to no parens */
	for (k = 0; (note_p = GETPTR(k)) != 0; k++) {
		if (note_p->note_has_paren == YES)
			parensexist = YES;
	}
	if (parensexist == NO)
		return;

	Reclim = 0;			/* table initially empty */

	/* set up dot offsets for both groups, zero if no dots */
	dotoff1 = gs1_p->dots * (width(FONT_MUSIC,DFLT_SIZE,C_DOT) + 2*STDPAD);
	dotoff2 = 0.0;		/* prevent useless 'used before set' warning */
	if (gs2_p != 0) {
		dotoff2 = gs2_p->dots * (width(FONT_MUSIC, DFLT_SIZE, C_DOT) +
				2 * STDPAD);
	}

	/*
	 * Loop through noteptrs, loading Rectab with all the things that are
	 * already present that are to the right of the baseline.
	 */
	for (k = 0; (note_p = GETPTR(k)) != 0; k++) {
		/*
		 * If note exists in top group, use its dot offset, else use
		 * bottom's.  If it's in both, the results would be the same.
		 */
		if (noteptrs[k].top_p != 0)
			dotoff = dotoff1;
		else
			dotoff = dotoff2;

		/* if note is right of normal position, put it in the table */
		if (note_p->c[RX] > 0) {
			Rectab[Reclim].n = note_p->c[RY] + halfhigh + STDPAD;
			Rectab[Reclim].s = note_p->c[RY] - halfhigh - STDPAD;
			Rectab[Reclim].e = note_p->c[RE] + STDPAD;
			Rectab[Reclim].w = note_p->c[RW] - STDPAD;
			inc_reclim();
		}

		/* if collinear, bottom group's notes go into table if normal */
		if (collinear && noteptrs[k].bot_p != 0) {
			if (note_p->c[RX] == 0) {
				Rectab[Reclim].n = note_p->c[RY] + halfhigh
						+ STDPAD;
				Rectab[Reclim].s = note_p->c[RY] - halfhigh
						- STDPAD;
				Rectab[Reclim].e = W_NORMAL * POINT
						+ 3 * halfwide + STDPAD;
				Rectab[Reclim].w = W_NORMAL * POINT
						+ halfwide - STDPAD;
				inc_reclim();
			}
		}

		/* if this group has dots, do rectangle for dots */
		if (dotoff > 0) {
			Rectab[Reclim].n = note_p->ydotr + STDPAD;
			Rectab[Reclim].s = note_p->ydotr - STDPAD;
			if (noteptrs[k].top_p != 0) {
				Rectab[Reclim].e = gs1_p->xdotr + dotoff;
			} else {
				Rectab[Reclim].e = gs2_p->xdotr + dotoff;
			}
			Rectab[Reclim].w = 0;
			inc_reclim();
		}
	}

	/*
	 * Loop through all parentheses, finding where they will fit, storing
	 * that info in erparen, and adding them to Rectab.
	 */
	for (k = 0; (note_p = GETPTR(k)) != 0; k++) {

		/* if no parens around the note, skip the note */
		if (note_p->note_has_paren == NO)
			continue;

		/* get dimensions of note's right paren */
		size = size_def2font(note_p->notesize);
		parenwidth = width(FONT_TR, size, ')');
		parenv = height(FONT_TR, size, ')') / 2.0;

		/* set the north and south of the paren */
		north = note_p->c[RY] + parenv;
		south = note_p->c[RY] - parenv;

		/*
		 * For each rectangle in Rectab, decide whether (based on
		 * its vertical coords) it could possibly overlap with our
		 * new paren.  If it's totally above or below ours, it
		 * can't.  We allow a slight overlap (FUDGE) so that round
		 * off errors don't stop us from packing things as tightly
		 * as possible.
		 */
		for (j = 0; j < Reclim; j++) {
			if (Rectab[j].s + FUDGE > north ||
			    Rectab[j].n < south + FUDGE) {
				Rectab[j].relevant = NO;
			} else {
				Rectab[j].relevant = YES;
			}
		}

		/*
		 * Mark that none of the relevant rectangles' boundaries have
		 * been tried yet for positioning our paren.
		 */
		for (j = 0; j < Reclim; j++) {
			if (Rectab[j].relevant == YES) {
				Rectab[j].tried = NO;
			}
		}

		/*
		 * Set up first trial position for this paren, just to the
		 * right of normal notes, allowing padding.
		 */
		west = halfwide + STDPAD;
		east = west + parenwidth;

		/*
		 * Keep trying positions for this paren, working left to
		 * right.  When we find one that doesn't overlap an existing
		 * rectangle, break.  This has to succeed at some point,
		 * at the rightmost rectangle position if not earlier.
		 */
		for (;;) {
			overlap = NO;
			for (j = 0; j < Reclim; j++) {
				/* ignore ones too far north or south */
				if (Rectab[j].relevant == NO) {
					continue;
				}

				/* if all west or east, okay; else overlap */
				if (Rectab[j].w + FUDGE <= east &&
				    Rectab[j].e >= west + FUDGE) {
					overlap = YES;
					break;
				}
			}

			/* if no rectangle overlapped, we found a valid place*/
			if (overlap == NO)
				break;

			/*
			 * Something overlapped, so we have to try again.
			 * Find the westermost relevant east rectangle boundary
			 * that hasn't been tried already, and whose east is
			 * not to the west of the current trial west, to use as
			 * the next trial position for our paren's west.
			 */
			try = -1;
			for (j = 0; j < Reclim; j++) {
				/* ignore ones too far north or south */
				if (Rectab[j].relevant == NO ||
				    Rectab[j].tried == YES) {
					continue;
				}

				/* ignore ones west of where we already are */
				if (Rectab[j].e < west) {
					continue;
				}

				/*
				 * If this is the first relevant one we haven't
				 * tried, or if this is farther west than the
				 * westernmost so far, save it as being the
				 * new westernmost so far.
				 */
				if (try == -1 || Rectab[j].e < Rectab[try].e) {
					try = j;
				}
			}

			if (try == -1)
				pfatal("bug in noterparen()");

			/*
			 * Mark this one as having been tried (for next time
			 * around, if necessary).  Set new trial values for
			 * east and west of our paren.
			 */
			Rectab[try].tried = YES;
			west = Rectab[try].e;
			east = west + parenwidth;

		} /* end of while loop trying positions for this acc */

		/*
		 * We have the final position for the new paren.  Enter it into
		 * Rectab.  Store its east in erparen in the NOTE structure for
		 * whichever groups have this note.
		 */
		Rectab[Reclim].n = north;
		Rectab[Reclim].s = south;
		Rectab[Reclim].e = east;
		Rectab[Reclim].w = west;
		inc_reclim();
		if (noteptrs[k].top_p != 0) {
			noteptrs[k].top_p->erparen = east;
		}
		if (noteptrs[k].bot_p != 0) {
			noteptrs[k].bot_p->erparen = east;
		}

	} /* end of loop for each paren */

	/*
	 * Finally, if the stems were collinear, we have to adjust erparen for
	 * all the notes of the bottom group, so that it's relative to the
	 * bottom group instead of the top group.
	 */
	if (collinear) {
		for (k = 0; (note_p = GETPTR(k)) != 0; k++) {
			if (noteptrs[k].bot_p != 0) {
				noteptrs[k].bot_p->erparen -= 2 * halfwide
						- W_NORMAL * POINT;
			}
		}
	}
}

/*
 * Name:        applyaccs()
 *
 * Abstract:    Stack accidentals against all the groups at once.
 *
 * Returns:     void
 *
 * Description: This function is used only when sep_accs == NO for the groups
 *		in a chord on a staff, meaning the accidentals are to be
 *		stacked to the left of all the groups, after restsyl.c is done
 *		placing all the groups relative to each other.
 */

void
applyaccs(g_p, numgrps)

struct GRPSYL *g_p[];		/* initially point at nonspace voices' groups */
int numgrps;			/* initially how many nonspace groups exist */

{
	/* point at each possible note in each possible group */
	struct NOTE *n_p[MAXVOICES][MAXHAND];

	struct NOTE *note_p;	/* point at one note */
	struct NOTE *accnote_p;	/* point at a note with an acc */
	struct GRPSYL *grace_p;	/* a grace group */
	struct GRPSYL *temp_p;	/* to the right of a grace group */
	float gwest[MAXVOICES];	/* original west boundary of group */
	float parenwidth;	/* passed to stackleft, otherwise not used */
	float base;		/* baseline to stack against */
	float west;		/* west edge of acc to be stacked */
	float noteleft;		/* left side of note relative to group - pad */
	float note_ry;		/* remember note_p->[RY] */
	float stem_rx;		/* RX of the center of a stem */
	float grace_re;		/* graces' new RE */
	float rollpad;		/* padding for a roll, if any */
	float shift;		/* how far to shift the grace groups */
	int found;		/* accs/parens found so far */
	int k;			/* loop variable */
	int size;		/* DFLT_SIZE or SMALLSIZE */
	int maxnotesize;	/* the biggest of any note of same pitch */
	int nidx;		/* which note in a group */
	int pidx;		/* pitch index: steps above c0 */
	int gidx;		/* group index: into g_p[gidx] or n_p[gidx][] */
	float high;		/* height and width of something */
	int gotacc;		/* are there any accidentals? */
	char acclist[MAX_ACCS * 2];	/* accs for a note */


	Reclim = 0;		/* init Rectab to empty */

	/*
	 * Remove groups from the array that are not notes, or are mrpt.  While
	 * doing this, provide a rectangle for any rest groups found.
	 */
	k = 0;
	for (gidx = 0; gidx < numgrps; gidx++) {
		g_p[k] = g_p[gidx];
		if (g_p[gidx]->grpcont == GC_REST) {
			Rectab[Reclim].n = g_p[gidx]->c[RN];
			Rectab[Reclim].s = g_p[gidx]->c[RS];
			Rectab[Reclim].e = g_p[gidx]->c[RE];
			Rectab[Reclim].w = g_p[gidx]->c[RW];
			inc_reclim();
		} else if (g_p[gidx]->grpcont == GC_NOTES &&
				is_mrpt(g_p[gidx]) == NO) {
			k++;	/* retain this group */
		}
	}
	numgrps = k;

	/* if no non-mrpt note groups, there are no accs to deal with */
	if (numgrps == 0) {
		return;
	}

	/* if accs were already applied separately to the groups, get out */
	if (g_p[0]->sep_accs == YES) {
		return;
	}

	/*
	 * Load the array pointing at all the notes.  If we find that no notes
	 * have accidentals, return, since there is nothing to do.
	 */
	gotacc = NO;
	for (gidx = 0; gidx < numgrps; gidx++) {
		/* first null out all the pointers for this group */
		for (pidx = 0; pidx < MAXHAND; pidx++) {
			n_p[gidx][pidx] = 0;
		}

		/* for every note that exists in this group, point at it */
		for (nidx = 0; nidx < g_p[gidx]->nnotes; nidx++) {
			note_p = &g_p[gidx]->notelist[nidx];
			pidx = 7 * note_p->octave +
					Letshift[note_p->letter - 'a'];
			n_p[gidx][pidx] = note_p;
			if (has_accs(note_p->acclist)) {
				gotacc = YES;
			}
		}
	}
	if (gotacc == NO) {
		return;
	}

	for (gidx = 0; gidx < numgrps; gidx++) {
		/*
		 * If grace notes exist before this group, this group's west
		 * boundary has been extended to contain them.  Temporarily
		 * put it back to its original place.
		 */
		if (g_p[gidx]->prev != 0 &&
				g_p[gidx]->prev->grpvalue == GV_ZERO) {
			g_p[gidx]->c[RW] = g_p[gidx]->prev->c[RE];
			gwest[gidx] = g_p[gidx]->c[RW];
		}

		/*
		 * For every note in every group, put a rectangle in Rectab.
		 * If there are parentheses around the note, or dots, just make
		 * one rectangle enclosing them all.  It's not worth making
		 * separate rectangles.  The coords are relative to the RX of
		 * the chord and the RY of the groups (meaning the center staff
		 * line).
		 */
		for (nidx = 0; nidx < g_p[gidx]->nnotes; nidx++) {
			note_p = &g_p[gidx]->notelist[nidx];

			/* get note's width/height; it can vary per note */
			size = size_def2font(note_p->notesize);
			high = height(note_p->headfont, size, note_p->headchar);

			/* create rectangle, and adjust for CSS if need be */
			Rectab[Reclim].n = note_p->c[RY] + high/2.0 + STDPAD;
			Rectab[Reclim].s = note_p->c[RY] - high/2.0 - STDPAD;
			Rectab[Reclim].e = g_p[gidx]->c[RX] +
					notehorz(g_p[gidx], note_p, RE);
			Rectab[Reclim].w = g_p[gidx]->c[RX] +
					notehorz(g_p[gidx], note_p, RW);
			css_alter_vert(note_p);
			inc_reclim();
		}

		/*
		 * If there is a stem, make a rectangle for it.  We rarely know
		 * how long it is yet, so just make it way long.
		 */
		if (STEMMED(g_p[gidx]) && g_p[gidx]->stemlen != 0) {
			stem_rx = g_p[gidx]->c[RX] + g_p[gidx]->stemx;
			if (g_p[gidx]->stemdir == UP) {
				Rectab[Reclim].n = 1000.0;
				Rectab[Reclim].s = g_p[gidx]->notelist[
					g_p[gidx]->nnotes - 1].c[RY];
			} else { /* DOWN */
				Rectab[Reclim].n = g_p[gidx]->notelist[0].c[RY];
				Rectab[Reclim].s = -1000.0;
			}
			Rectab[Reclim].e = stem_rx + W_NORMAL * POINT;
			Rectab[Reclim].w = stem_rx - W_NORMAL * POINT;
			inc_reclim();
		}
	}

	/*
	 * Loop through all notes, finding the ones with accs.  Find where they
	 * will fit, storing that info in the waccr and adding them to Rectab.
	 * Call a function so that we loop in the proper order.
	 */
	for (found = 0, pidx = nextacc(n_p, numgrps, found);  pidx != -1;
			found++, pidx = nextacc(n_p, numgrps, found)) {
		/*
		 * Find RW of the leftmost note of this pitch.  Also, of all
		 * the notes, remember one of them that has the acc, 
		 * normal size if any do.
		 */
		base = 1000.0;		/* way to the right of everything */
		accnote_p = 0;		/* haven't found one yet */
		maxnotesize = GS_SMALL;	/* start with the minimum */
		CLEAR_ACCS(acclist);	/* not found yet */
		for (gidx = 0; gidx < numgrps; gidx++) {
			note_p = n_p[gidx][pidx];
			if (note_p != 0) {
				noteleft = g_p[gidx]->c[RX] + note_p->c[RW]
						- STDPAD;
				if (has_accs(note_p->acclist) &&
						noteleft < base) {
					base = noteleft;
					accnote_p = note_p;
					COPY_ACCS(acclist, accnote_p->acclist);
				}
				if (note_p->notesize == GS_NORMAL) {
					maxnotesize = GS_NORMAL;
				}
			}
		}

		/*
		 * If any notes are of normal size, make sure all and only
		 * those notes have the acc, so that the print phase will print
		 * the correct (normal) size.  Also point accnote_p at one of
		 * the normal notes.
		 */
		if (maxnotesize == GS_NORMAL) {
			for (gidx = 0; gidx < numgrps; gidx++) {
				note_p = n_p[gidx][pidx];
				if (note_p != 0) {
					if (note_p->notesize == GS_NORMAL) {
						COPY_ACCS(note_p->acclist,
								acclist);
						accnote_p = note_p;
					} else {
						CLEAR_ACCS(note_p->acclist);
					}
				}
			}
		}

		/*
		 * Stack the acc.  By using this base, the acc will be left of
		 * the leftmost note of this pitch.  Since we're not asking for
		 * paren info, parenwidth will be set to 0.
		 */
		west = stackleft(accnote_p, base, &parenwidth, SL_ACC);

		/*
		 * Loop through the groups, dealing with the ones that have
		 * this note.
		 */
		note_ry = 0.0;		/* keep lint happy */
		for (gidx = 0; gidx < numgrps; gidx++) {
			note_p = n_p[gidx][pidx];
			if (note_p == 0) {
				continue;	/* this group doesn't have it */
			}

			/*
			 * The note exists in this group, so if it has the acc
			 * there, set waccr.  (In some groups there might be no
			 * acc, so we assumed the user "meant" it, but since
			 * the field isn't set, we don't need to set waccr.)
			 */
			if (has_accs(note_p->acclist)) {
				note_p->waccr = west - g_p[gidx]->c[RX];
			}

			/*
			 * Remember the note's RY.  At least one group has to
			 * have the note, so note_ry will get set.
			 */
			note_ry = note_p->c[RY];
		}

		/*
		 * For each group that overlaps vertically with the acc's RY
		 * (which equals the note's RY), expand the group's boundary to
		 * include the acc.  Allow for a roll, if any.  Thanks to CSS,
		 * we can't just use the group coordinates; we have to look at
		 * the coords of the outer notes.
		 */
		for (gidx = 0; gidx < numgrps; gidx++) {
			rollpad = g_p[gidx]->roll == NOITEM ? 0.0 : ROLLPADDING;
			if (g_p[gidx]->notelist[0].c[RN] > note_ry &&
			    g_p[gidx]->notelist[g_p[gidx]->nnotes - 1].c[RS]
					< note_ry &&
			    g_p[gidx]->c[RW] > west - rollpad) {
				g_p[gidx]->c[RW] = west - rollpad;
				g_p[gidx]->orig_rw = g_p[gidx]->c[RW];
			}
		}
	}

	/*
	 * Groups' west boundaries may have been extended due to accidentals.
	 * Move move any grace groups that exist to the left, if necessary to
	 * avoid hitting these accidentals.  Then restore the main groups'
	 * west boundaries to include their graces.
	 */

	/* if we are dealing with grace groups, we are done */
	if (g_p[0]->grpvalue == GV_ZERO) {
		return;
	}

	/* we are dealing with nongrace groups */

	/* set grace_re to be the leftmost main group boundary of any group */
	grace_re = 0.0;
	for (gidx = 0; gidx < numgrps; gidx++) {
		if (g_p[gidx]->c[RW] < grace_re) {
			grace_re = g_p[gidx]->c[RW];
		}
	}

	/* for each nongrace group, move its graces, if any */
	for (gidx = 0; gidx < numgrps; gidx++) {
		/* if no graces before this group, skip it */
		if (g_p[gidx]->prev == 0 ||
				g_p[gidx]->prev->grpvalue == GV_NORMAL) {
			continue;
		}

		/*
		 * Find how much we need to shift the rightmost grace, and then
		 * shift them all by that amount.
		 */
		shift = gwest[gidx] - grace_re;
		temp_p = g_p[gidx];
		for (grace_p = temp_p->prev;
		     grace_p != 0 && grace_p->grpvalue == GV_ZERO;
		     temp_p = grace_p, grace_p = grace_p->prev) {
			grace_p->c[RX] -= shift;
			grace_p->c[RW] -= shift;
			grace_p->c[RE] -= shift;
		}

		/* restore main group's west */
		g_p[gidx]->c[RW] = temp_p->c[RW];
	}
}

/*
 * Name:	nextacc()
 *
 * Abstract:	Find the next note that has an accidental to be processed.
 *
 * Returns:	Index to the NOTE, or -1 if no more.
 *
 * Description:	This function is called by applyaccs(), to return in the
 *		correct order the notes that have accidentals to be processed.
 *		The first time in here, count is 0, and it looks for the first
 *		eligible note (top down).  The next time, count is 1, and it
 *		looks for the bottommost eligible note.  After that, it goes
 *		through the inner notes, top down.  In the great majority of
 *		cases, this will result in the most desirable packing of
 *		accidentals.
 */

static int
nextacc(n_p, numgrps, found)

struct NOTE *n_p[MAXVOICES][MAXHAND];
int numgrps;			/* how many nonspace groups are here */
int found;			/* no. of accidentals found already */

{
	struct NOTE *note_p;	/* point at a note */
	static int previdx;	/* idx to note chosen the last time in here */
	static int lastidx;	/* idx to the bottommost note chosen */
	int pidx;		/* pitch index: steps above c0 */
	int gidx;		/* loop counter through the groups */


	/*
	 * If this is the first call for this group(s), find the highest note
	 * with an accidental.
	 */
	if (found == 0) {
		for (pidx = MAXHAND - 1; pidx >= 0; pidx--) {
			for (gidx = 0; gidx < numgrps; gidx++) {
				note_p = n_p[gidx][pidx];
				if (note_p != 0 && has_accs(note_p->acclist)) {
					/* remember for next time */
					previdx = pidx;
					return (pidx);
				}
			}
		}
		return (-1);	/* no notes have acc, should not get here */
	}

	/*
	 * If this is the second call, find the bottom of the list, then look
	 * backwards for the last eligible note.  Stop before finding the first
	 * note again.
	 */
	if (found == 1) {
		/* search from lowest possible note, stopping before the top */
		for (pidx = 0; pidx < previdx; pidx++) {
			for (gidx = 0; gidx < numgrps; gidx++) {
				note_p = n_p[gidx][pidx];
				if (note_p != 0 && has_accs(note_p->acclist)) {
					/* remember it for next time */
					lastidx = pidx;
					return (pidx);
				}
			}
		}
		return (-1);	/* only 1 note has acc */
	}

	/*
	 * Third or later call:  Scan inner notes top to bottom.
	 */
	for (pidx = previdx - 1; pidx > lastidx; pidx--) {
		for (gidx = 0; gidx < numgrps; gidx++) {
			note_p = n_p[gidx][pidx];
			if (note_p != 0 && has_accs(note_p->acclist)) {
				/* remember it for next time */
				previdx = pidx;
				return (pidx);
			}
		}
	}
	return (-1);		/* all eligible notes were already found */
}

/*
 * Name:	css_alter_vert()
 *
 * Abstract:	Alter the last rectangle in Rectab for CSS purposes if need be.
 *
 * Returns:	void
 *
 * Description:	If the given note is CSS, this function alters the last
 *		rectangle in Rectab so that it looks like the item is far away
 *		from the normal notes, as explained in setnotes.c.
 */

static void
css_alter_vert(note_p)

struct NOTE *note_p;

{
	if (note_p->stepsup >= CSS_STEPS / 2) {
		Rectab[Reclim].n += CSS_OFF;
		Rectab[Reclim].s += CSS_OFF;
	} else if (note_p->stepsup <= -CSS_STEPS / 2) {
		Rectab[Reclim].n -= CSS_OFF;
		Rectab[Reclim].s -= CSS_OFF;
	}
}

/*
 * Name:        setwithside()
 *
 * Abstract:    Decide which side a "with" list should go on.
 *
 * Returns:     void
 *
 * Description: This function decides which side of the group each "with" list
 *		element should be put on, if it was not already set by the user.
 */

static void
setwithside(gs_p, ogs_p, v3gs_p)

struct GRPSYL *gs_p;	/* the group to be worked on */
struct GRPSYL *ogs_p;	/* first group in other voice */
struct GRPSYL *v3gs_p;	/* voice 3 (used only when gs_p is v1 or v2) */

{
	struct GRPSYL *g_p;	/* earlier GRPSYLs in *gs_p's list */
	RATIONAL vtime;		/* time preceding this group in measure */
	int normwith;		/* should PL_UNKNOWN items be put in the
				 * normal place? */
	int place;		/* PL_ABOVE or PL_BELOW */
	int idx;		/* index into "with" list */


	normwith = PL_UNKNOWN;	/* keep compiler happy */

	/*
	 * Define a chunk of code for the cases where the stem may be allowed
	 * to go either way.  It goes opposite the stem for normal, with the
	 * stem for tab.
	 */
#define FREESTEM							\
	{								\
		if (is_tab_staff(gs_p->staffno) == YES) {		\
			normwith = NO;					\
		} else {						\
			normwith = YES;					\
		}							\
	}

	/*
	 * Define a chunk of code for the cases where the stem has to go a
	 * certain way, determined by which voice this is, unless forced by the
	 * user.  The "with" items are always above a voice acting as voice 1,
	 * and below a voice acting as voice 2.
	 */
#define FIXEDSTEM							\
	{								\
		if (gs_p->pvno == 1) {					\
			normwith = gs_p->stemdir == UP ? NO : YES;	\
		} else {						\
			normwith = gs_p->stemdir == DOWN ? NO : YES;	\
		}							\
	}

	/*
	 * If there is cross staff stemming, that consideration overrides all
	 * others.  We want to keep the "with" items towards our staff, hoping
	 * they will be less likely to collide with something there.
	 */
	if (gs_p->stemto != CS_SAME) {
		place = gs_p->stemto == CS_ABOVE ? PL_BELOW : PL_ABOVE;

		/* set all the unknown items to this place */
		for (idx = 0; idx < gs_p->nwith; idx++) {
			if (gs_p->withlist[idx].place == PL_UNKNOWN) {
				gs_p->withlist[idx].place = place;
			}
		}
		return;
	}

	/*
	 * Switch on vscheme to decide which side of the group the "with"
	 * things will be put on.
	 */
	switch (svpath(gs_p->staffno, VSCHEME)->vscheme) {
	case V_1:
		FREESTEM
		break;

	case V_2OPSTEM:
		FIXEDSTEM
		break;

	case V_2FREESTEM:
		/*
		 * Figure out where this group starts by adding up the time
		 * values of all previous groups in the measure.  Then, treat
		 * this like V_1 or V_2OPSTEM, based on whether the other
		 * voice has space here.
		 */
		vtime = Zero;
		for (g_p = gs_p->prev; g_p != 0; g_p = g_p->prev)
			vtime = radd(vtime, g_p->fulltime);

		if (hasspace(ogs_p, vtime, radd(vtime, gs_p->fulltime))) {
			FREESTEM
		} else {
			FIXEDSTEM
		}
		break;

	case V_3OPSTEM:
		if (gs_p->pvno == 3) {
			FREESTEM	/* voice 3 is always like V_1 */
		} else {
			FIXEDSTEM
		}
		break;

	case V_3FREESTEM:
		if (gs_p->pvno == 3) {
			FREESTEM	/* voice 3 is always like V_1 */
		} else {
			/* voices 1 and 2 act like V_2FREESTEM */
			vtime = Zero;
			for (g_p = gs_p->prev; g_p != 0; g_p = g_p->prev)
				vtime = radd(vtime, g_p->fulltime);

			if (has_space_pvno(ogs_p, v3gs_p, vtime,
					radd(vtime, gs_p->fulltime))) {
				FREESTEM
			} else {
				FIXEDSTEM
			}
		}
		break;

	default:
		pfatal("unknown vscheme %d",
				svpath(gs_p->staffno, VSCHEME)->vscheme);
	}

	/* "normal" means below for stem up, above for stem down */
	if ((normwith == YES) == (gs_p->stemdir == UP)) {
		place = PL_BELOW;
	} else {
		place = PL_ABOVE;
	}

	/* set all the unknown items to this place */
	for (idx = 0; idx < gs_p->nwith; idx++) {
		if (gs_p->withlist[idx].place == PL_UNKNOWN) {
			gs_p->withlist[idx].place = place;
		}
	}
}

/*
 * Name:	fixrollroom()
 *
 * Abstract:	Make sure each GRPSYL.c[RW] has room for cross group rolls.
 *
 * Returns:	void
 *
 * Description:	This function is called after all groups in this chord have
 *		had their RW adjusted when a roll is present.  But if a roll
 *		goes across multiple groups, the RW of all those groups must be
 *		set to the westernmost's RW.
 */

static void
fixrollroom(ch_p)

struct CHORD *ch_p;
{
	struct GRPSYL *gs_p;		/* any group in this chord */
	struct GRPSYL *startgs_p;	/* STARTITEM of a roll */
	struct GRPSYL *endgs_p;		/* ENDITEM of a roll */
	struct GRPSYL *rollgs_p;	/* any group in a roll */
	float farwest;			/* farthest west RW for a roll */


	/* loop through every group in this chord */
	for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
		if (gs_p->grpsyl == GS_SYLLABLE) {
			continue;
		}
		if (gs_p->roll != STARTITEM) {
			continue;
		}

		/* start of a multigroup roll (single would be LONEITEM) */

		/* find westernmost GRPSYL boundary in roll */
		startgs_p = gs_p;
		farwest = 0.0;
		endgs_p = startgs_p;	/* set laster, but keep lint happy */
		for (rollgs_p = startgs_p; rollgs_p != 0;
					rollgs_p = rollgs_p->gs_p) {
			if (rollgs_p->grpsyl == GS_SYLLABLE) {
				continue;
			}
			if (rollgs_p->c[RW] < farwest) {
				farwest = rollgs_p->c[RW];
			}
			if (rollgs_p->roll == ENDITEM) {
				endgs_p = rollgs_p;
				break;
			}
		}

		/* set all the roll's GRPSYLs' boundaries to westernmost */
		for (rollgs_p = startgs_p; rollgs_p != endgs_p->gs_p;
					rollgs_p = rollgs_p->gs_p) {
			if (rollgs_p->grpsyl == GS_SYLLABLE) {
				continue;
			}
			rollgs_p->c[RW] = farwest;
		}
	}
}
