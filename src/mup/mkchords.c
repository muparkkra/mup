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
 * Name:	mkchords.c
 *
 * Description:	This file contains functions for creating CHORD linked lists
 *		and erasing invisible voices.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"


static void swingmidi P((void));
static struct GRPSYL *voicevis P((struct GRPSYL *gs_p));
static void combine_voices P((void));
static void chkhand1 P((struct GRPSYL *gs1_p, struct MAINLL *staffmll_p));
static int chk2groups P((struct GRPSYL *hi_p, struct GRPSYL *lo_p,
		struct MAINLL *mll_p, int qual));
static void chkhand2 P((struct GRPSYL *gs1_p, struct MAINLL *staffmll_p));
static int chk2neighbors P((struct GRPSYL *first_p, struct GRPSYL *second_p,
		struct GRPSYL *gs1_p, struct MAINLL *mll_p));
static void dohand P((struct GRPSYL *gs1_p, struct MAINLL *staffmll_p));
static void comb2groups P((struct GRPSYL *dest_p, struct GRPSYL *src_p,
		struct MAINLL *mll_p));
static void addsrc2dest P((struct GRPSYL *dest_p, struct GRPSYL *src_p));
static int setgrpptrs P((struct GRPSYL *gs1_p, struct GRPSYL *v_p[]));
static int hastieslur P((struct GRPSYL *gs_p));
static int tieslur_othervoice P((struct GRPSYL *gs_p));

/*
 * Name:        makechords()
 *
 * Abstract:    Set up the linked lists of chords.
 *
 * Returns:     void
 *
 * Description: This function scans through the main linked list looking
 *		for STAFF structures.  It joins the GRPSYL structures in
 *		the lists that they head, into perpendicular linked lists,
 *		allocating a CHORD to head each of these.  It also links
 *		the CHORDs for each measure together into a linked list.
 *
 *		While doing the above, it also calls voicevis() to check if a
 *		voice should be invisible, and if so changes it to a measure
 *		space.  It also applies the swingunit and voicecombine
 *		parameters.
 */


void
makechords()

{
	struct MAINLL *mainll_p;	/* point at item in main linked list */
	struct MAINLL *mainch_p;	/* for headcell of a chord list */

	RATIONAL *vtime;		/* cum. time of each voice and verse */
	struct GRPSYL **grpsyl_p;	/* pointers along GRPSYL lists */
	struct STAFF *staff_p;		/* point at a staff structure */

	RATIONAL mintime;		/* the minimum vtime */
	register int num;		/* no. of visible voices/verses */
	register int v;			/* index into grpsyl_p[] */
	struct CHORD *ch_p;		/* pointer to current chord */
	struct CHORD *och_p;		/* pointer to old chord */
	struct GRPSYL *gs_p;		/* pointer to current group/syllable */
	int n;				/* loop variable */
	int firstgs;			/* flag for first group/syllable */
	int quit;			/* flag for being done */


	debug(16, "makechords");
	/*
	 * If we are generating MIDI, we may need to change lengths (fulltimes)
	 * of groups.  This has to be done now, before we link groups together
	 * into chords.
	 */
	if (Doing_MIDI == YES) {
		swingmidi();
	}

	gs_p = 0;		/* keep lint happy; will be set before used */

	/* malloc enough of these for all voices and verses */
	MALLOC(rational, vtime, MAXSTAFFS * (MAXVOICES + Maxverses));
	MALLOC(GRPSYL *, grpsyl_p, MAXSTAFFS * (MAXVOICES + Maxverses));

	mainll_p = Mainllhc_p;		/* point at first thing in main LL */

	initstructs();			/* clean out old SSV info */

	/*
	 * Loop once for each measure in the input.
	 */
	for (;;) {
		num = 0;		/* number of linked lists in measure */

		/*
		 * Look for the first structure in this measure that points off
		 * to a linked list of groups/syllables.  If we hit the end of
		 * the main linked list, we're all done, so break out.
		 */
		while (mainll_p != 0 && mainll_p->str != S_STAFF) {
			if (mainll_p->str == S_SSV)
				asgnssv(mainll_p->u.ssv_p);
			mainll_p = mainll_p->next;
		}

		if (mainll_p == 0) {
			FREE(vtime);
			FREE(grpsyl_p);
			break;
		}

		/*
		 * We've found another measure with STAFF in it.  Allocate
		 * a chord headcell for this measure, and put it in the
		 * main linked list.
		 */
		mainch_p = newMAINLLstruct(S_CHHEAD, 0);
		insertMAINLL(mainch_p, mainll_p->prev);

		/*
		 * Look for the last STAFF structure in the measure.  While
		 * doing this, point at first element of all the group/syllable
		 * linked lists, and keep count of them.  Ignore invisible
		 * staffs.  Skip over any grace groups.
		 */
		while (mainll_p != 0 && mainll_p->str == S_STAFF) {
			staff_p = mainll_p->u.staff_p;

			if (staff_p->visible == YES) {

				/* do all the voices on this staff */
				for (n = 0; n < MAXVOICES &&
				staff_p->groups_p[n] != 0; n++) {
					/*
					 * Zap voice if invisible.  Set both
					 * of these to point at the first
					 * GRPSYL, which will be a new one if
					 * we zapped it.
					 */
					grpsyl_p[num] = staff_p->groups_p[n] =
					voicevis(staff_p->groups_p[n]);

					/* skip leading grace groups */
					while (grpsyl_p[num] != 0 &&
					grpsyl_p[num]->grpvalue == GV_ZERO)
						grpsyl_p[num] = grpsyl_p[num]->next;
					if (grpsyl_p[num] == 0)
						pfatal("nothing but grace groups found");
					num++;
				}

				/* do all the verses on this staff */
				for (n = 0; n < staff_p->nsyllists; n++)
					grpsyl_p[num++] = staff_p->syls_p[n];
			}
			mainll_p = mainll_p->next;
		}

		/*
		 * Set up the first chord from the first note in each
		 * voice/verse.  Its linked list of GRPSYLs will include
		 * the first GRPSYL in every linked list off of a visible
		 * STAFF.
		 */
		MALLOC(CHORD, ch_p, 1);
		mainch_p->u.chhead_p->ch_p = ch_p; /* point at first chord */
		ch_p->ch_p = 0;		/* only member on list so far */
		ch_p->starttime = Zero; /* start time = any voice */

		/* point headcell at first and set its first group's time */
		ch_p->gs_p = grpsyl_p[0];
		vtime[0] = grpsyl_p[0]->fulltime;

		/* for each remaining one, point prev one at it & set time */
		for (v = 1; v < num; v++) {
			grpsyl_p[v-1]->gs_p = grpsyl_p[v];
			vtime[v] = grpsyl_p[v]->fulltime;
		}
		grpsyl_p[num-1]->gs_p = 0;	/* terminate linked list */

		/* point at second GRPSYL in each voice/verse, if any */
		for (v = 0; v < num; v++)
			grpsyl_p[v] = grpsyl_p[v]->next;

		/*
		 * Loop until groups/syllables in the voices/verses are used
		 * up.  Form a chord for each time at which any voice/verse
		 * has a GRPSYL structure, though ignore grace groups.
		 */
		for (;;) {
			/*
			 * If every GRPSYL currently pointed at by a grpsyl_p[]
			 * is the last in its list (the measure), there are no
			 * more chords in this measure, and quit will remain
			 * YES.
			 */
			quit = YES;		/* first assume "quit" */
			for (v = 0; v < num; v++) {
				/* find next item (if any) not a grace group */
				while (grpsyl_p[v] != 0 &&
				       grpsyl_p[v]->grpsyl == GS_GROUP &&
				       grpsyl_p[v]->grpvalue == GV_ZERO) {

					grpsyl_p[v] = grpsyl_p[v]->next;
				}

				/* check if voice/verse has another item */
				if (grpsyl_p[v] != 0) {
					quit = NO; /* yes, so don't quit yet */
				}
			}

			/* if time to quit, skip rest of loop, and get out */
			if (quit == YES)
				break;

			/*
			 * At least one voice/verse has another note in it.
			 * Find the earliest time at which something changes.
			 */
			mintime = vtime[0];
			for (v = 1; v < num; v++)
				if (LT(vtime[v], mintime))
					mintime = vtime[v];

			/* allocate memory for another chord */
			och_p = ch_p;	/* remember where previous chord is */
			MALLOC(CHORD, ch_p, 1);
			och_p->ch_p = ch_p;	/* point previous chord at it*/
			ch_p->ch_p = 0;		/* terminate in case last */

			ch_p->starttime = mintime; /* starting time for chord*/

			/*
			 * Form a new linked list.  The head cell is the new
			 * chord, and the list connects it to all the groups/
			 * syllables that start at this time.
			 */
			firstgs = YES;
			for (v = 0; v < num; v++) {
				if (EQ(vtime[v], mintime)) {
					/*
					 * This voice/verse has a grpsyl at
					 * this time.  Make the previous one
					 * point at it, set its pointer to 0
					 * in case it turns out to be the last,
					 * and add its length to vtime[v].
					 */
					if (firstgs == YES) {
						/* point headcell at first */
						ch_p->gs_p = grpsyl_p[v];
						firstgs = NO;
					} else {
						/* point previous one at ours*/
						gs_p->gs_p = grpsyl_p[v];
					}

					/* set gs_p to point at our new one */
					gs_p = grpsyl_p[v];

					vtime[v] = radd(vtime[v],
							gs_p->fulltime);

					/* get next GRPSYL in voice/verse */
					grpsyl_p[v] = gs_p->next;
				}
			}

			gs_p->gs_p = 0;		/* terminate linked list */
		}

		/*
		 * Set the duration of each chord in this measure.  It's the
		 * next chord's start time minus this chord's start time.
		 * But for the last chord, it's the time signature minus
		 * this chord's start time.  Also set pseudodur, which is a
		 * function of the duration.  The amount of width the chord
		 * "deserves" to be allocated is proportional to pseudodur.
		 */
		for (ch_p = mainch_p->u.chhead_p->ch_p; ch_p->ch_p != 0;
				ch_p = ch_p->ch_p) {
			ch_p->duration = rsub(ch_p->ch_p->starttime,
						ch_p->starttime);
			ch_p->pseudodur = pow(RAT2FLOAT(ch_p->duration),
						Score.packexp);
		}
		ch_p->duration = rsub(Score.time, ch_p->starttime);
		ch_p->pseudodur = pow(RAT2FLOAT(ch_p->duration), Score.packexp);
	}

	/* if voicecombine parm was ever set, combine voices if possible */
	if (Vcombused == YES) {
		combine_voices();
	}

	/* now that voices are combined, we can apply the useaccs parameter */
	apply_useaccs();
}

/*
 * Name:	swingmidi()
 *
 * Abstract:	Alter groups' time to implement the "swingunit" parameter.
 *
 * Returns:	void
 *
 * Description:	This function loops through every GRPSYL in every voice,
 *		adjusting their fulltime so that they will start and end at
 *		different times, if necessary, to follow the "swingunit" parm.
 *		Each measure is divided into durations of "swingunit", starting
 *		at the beginning.  (Usually the timesig divided by swingunit
 *		will be an integer, but if not, the last piece will be shorter.)
 *		The time where one group ends and the next group starts will be
 *		altered in either of these two circumstances:
 *		1. The current boundary time is halfway into a swingunit, and
 *		   each group is at least half a swingunit long.
 *		2. The current boundary time is 3/4 of the way into a swingunit,
 *		   and the first group is at least 3/4 of a swingunit long, and
 *		   and the second group is at least 1/4 of a swingunit long.
 *		In both of these cases, the fulltimes are altered so that the
 *		meeting point is 2/3 of the way into the swingunit.
 */

void
swingmidi()
{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct GRPSYL *gs_p;		/* point along a GRPSYL list */
	struct GRPSYL *prev_p;		/* the GRPSYL before gs_p */
	int vidx;			/* voice index, 0 to MAXVOICES-1 */
	RATIONAL quot;			/* quotient */
	RATIONAL swingunit;		/* from SSV */
	RATIONAL starttime;		/* offset into measure of gs_p */
	RATIONAL halfswing;		/* swingunit/2 */
	RATIONAL sixthswing;		/* swingunit/6 */
	RATIONAL twelfthswing;		/* swingunit/12 */
	RATIONAL threefourthsswing;	/* 3/4 * swingunit */
	RATIONAL onefourthswing;	/* 1/4 * swingunit */
	static RATIONAL six = {6,1};
	static RATIONAL twelve = {12,1};


	initstructs();

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			/* need to keep swingunit up to date */
			asgnssv(mainll_p->u.ssv_p);
			continue;
		case S_STAFF:
			break;	/* break out and handle this staff */
		default:
			continue;
		}

		/* loop through every voice on this staff */
		for (vidx = 0; vidx < MAXVOICES; vidx++) {

			swingunit = vvpath(mainll_p->u.staff_p->staffno,
					vidx + 1, SWINGUNIT)->swingunit;

			/* skip this voice if swingunit was not set */
			if (EQ(swingunit, Zero)) {
				continue;
			}

			/* various rationals we will need in the loop below */
			halfswing = rdiv(swingunit, Two);
			threefourthsswing = rmul(swingunit, Three_fourths);
			onefourthswing = rmul(swingunit, One_fourth);
			sixthswing = rdiv(swingunit, six);
			twelfthswing = rdiv(swingunit, twelve);

			/* accumulate starttime */
			starttime = Zero;

			/* for lint; we'll never really check it when it's 0 */
			prev_p = 0;

			/* find first nongrace group in voice (0 if none) */
			gs_p = mainll_p->u.staff_p->groups_p[vidx];
			gs_p = gs_p != 0 && gs_p->grpvalue == GV_ZERO ?
					nextnongrace(gs_p) : gs_p;

			/* loop through every nongrace group in this voice */
			for ( ; gs_p != 0; gs_p = nextnongrace(gs_p)) {

				quot = rdiv(starttime, swingunit);

				/* set starttime for the following group here
				 * because we may alter fulltime below */
				starttime = radd(starttime, gs_p->fulltime);

				/* handle case 1 (see prolog above) */
				if (quot.d == 2 &&
				    GE(gs_p->fulltime, halfswing) &&
				    GE(prev_p->fulltime, halfswing)) {

					prev_p->fulltime = radd(
						prev_p->fulltime, sixthswing);
					gs_p->fulltime = rsub(
						gs_p->fulltime, sixthswing);
				}

				/* handle case 2 (see prolog above) */
				if (quot.d == 4 && quot.n % 4 == 3 &&
				    GE(gs_p->fulltime, onefourthswing) &&
				    GE(prev_p->fulltime, threefourthsswing)) {

					prev_p->fulltime = rsub(
						prev_p->fulltime, twelfthswing);
					gs_p->fulltime = radd(
						gs_p->fulltime, twelfthswing);
				}

				prev_p = gs_p;
			}
		}
	}
}

/*
 * Name:        voicevis()
 *
 * Abstract:    If this voice is to be invisible, make it a measure space.
 *
 * Returns:     pointer to first GRPSYL; this equals the input pointer if the
 *		voice is visible, else it points at the new measure space GRPSYL
 *
 * Description: This function finds out if the given voice is supposed to be
 *		invisible this measure.  If so, it throws away the current
 *		GRPSYL list, replacing it with a single GRPSYL that is a
 *		measure space.  See the big comment in svpath() in ssv.c.
 */


static struct GRPSYL *
voicevis(gs_p)

struct GRPSYL *gs_p;		/* first GRPSYL in this voice's linked list */

{
	struct GRPSYL *ngs_p;	/* pointer to the new GRPSYL */


	/*
	 * At this point we know that the command line -s option allows this
	 * staff to be printed, and in fact this staff will be printed, because
	 * at least one of its voice(s) is supposed to be.  (We know this
	 * because where we were called from, staff_p->visible == YES, and that
	 * was set by calling svpath().)  If the staff weren't being printed,
	 * we wouldn't need to bother wiping out its invisible voice(s).
	 */

	/*
	 * This voice must be changed to a measure space if the -s option says
	 * it should be invisible, or if the SSVs say so.  The vvpath function
	 * checks both things.
	 */
	if (vvpath(gs_p->staffno, gs_p->vno, VISIBLE)->visible == NO) {

		/*
		 * Allocate a new GRPSYL and make it a measure space, with the
		 * same inputlineno etc. as the first in the current list.
		 */
		ngs_p = newGRPSYL(GS_GROUP);

		ngs_p->inputlineno = gs_p->inputlineno;
		ngs_p->inputfile = gs_p->inputfile;
		ngs_p->staffno = gs_p->staffno;
		ngs_p->vno = gs_p->vno;
		ngs_p->grpsyl = GS_GROUP;
		ngs_p->is_meas = YES;
		ngs_p->basictime = -1;

		/* in one compiler the following had to be done separately */
		/* like this, because it couldn't do the structure assignment */
		ngs_p->fulltime.n = Score.time.n;
		ngs_p->fulltime.d = Score.time.d;

		ngs_p->grpcont = GC_SPACE;
		ngs_p->prev = 0;
		ngs_p->next = 0;
		ngs_p->gs_p = 0;

		/* throw away the old GRPSYL list */
		free_grpsyls(gs_p);

		return (ngs_p);	/* ret pointer to the measure space */
	}

	return (gs_p);	/* ret pointer to the original, unchanged GRPSYL */
}

/*
 * Name:        combine_voices()
 *
 * Abstract:    Combine GRPSYLs in voices according to the voicecombine parm.
 *
 * Returns:     void
 *
 * Description: This function, if requested by the voicecombine parameter,
 *		combines the appropriate groups in each chord on each staff
 *		where it can.  (The set of groups in one chord on one staff is
 *		called a "hand" in the following code and in some other places
 *		in Mup, because I get tired of spelling out the whole phrase.)
 *		Because of beaming and ties/slurs, some groups may need to
 *		remain uncombined because their neighbors can't be combined.
 *		So the work is done in three passes.  The first pass looks at
 *		each hand individually to see what can be done there.  The
 *		second pass applies the neighbor rules.  The third pass does
 *		the actual combining.
 */
/*
 * NOTE:  GRPSYL.pvno is used as a scratch area in these functions.  The macro
 * below defines COMB to be pvno for this use.  We rely on the fact that CALLOC
 * initialized it to 0.  We count the low order bit as bit 0.  Two sets of bits
 * are used, as follows.
 *
 * Bit "x+y" being set to 1 means that the GRPSYLs of voices x and y can be
 * combined (looking only at the groups in this chord, not groups they may be
 * tie/beamed to, etc.).  This scheme works only because the only voice numbers
 * are 1, 2, and 3, so that uses bits 3, 4, and 5.  The first pass sets them,
 * and the second pass reads them.
 *
 * Bits 6 through 13 are used in pairs for voice numbers of groups that really
 * should be combined, with all rules applied.  The SHIFT macros below give the
 * rightmost bit of each of the voice numbers, for the two possible sources and
 * destinations that can happen, given that only 3 voices can exist.  Pass 2
 * sets these bits, and pass 3 reads them.
 */
#define COMB		pvno
#define SRC1SHIFT	6
#define DEST1SHIFT	8
#define SRC2SHIFT	10
#define DEST2SHIFT	12

static void
combine_voices()
{
	struct MAINLL *mll_p;	/* point along main linked list */
	struct MAINLL *staffmll_p; /* point a group's staff's MLL struct */
	struct MAINLL *mll2_p;	/* for finding the next bar line */
	struct CHORD *ch_p;	/* point along a chord list */
	struct GRPSYL *gs1_p;	/* point along the GRPSYL list of a chord */
	struct TIMEDSSV *tssv_p;/* point along a timed SSV list */
	RATIONAL offset;	/* current chord's offset into meas */
	int staff;		/* staff number we are working on */
	int pass;		/* the three passes we must make */


	/* run the three passes */
	for (pass = 1; pass <= 3; pass++) {

		initstructs();			/* clean out old SSV info */

		for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {

			switch (mll_p->str) {
			case S_SSV:
				/* keep MLL SSVs up to date */
				asgnssv(mll_p->u.ssv_p);
				continue;
			case S_CHHEAD:
				break;	/* break out to handle the chords */
			default:
				continue;	/* ignore everything else */
			}

			/* find the bar line at the end of this measure */
			for (mll2_p = mll_p; mll2_p != 0 &&
			     mll2_p->str != S_BAR; mll2_p = mll2_p->next) {
				;
			}
			if (mll2_p == 0) {
				pfatal("no bar line at end of measure");
			}
			tssv_p = mll2_p->u.bar_p->timedssv_p;

			offset = Zero;	/* first chord's time offset into meas*/
			/*
			 * Loop through each chord in this list.
			 */
			for (ch_p = mll_p->u.chhead_p->ch_p; ch_p != 0;
						ch_p = ch_p->ch_p) {

				/* apply midmeasure SSVs up to this chord */
				while (tssv_p != 0 &&
						LE(tssv_p->time_off, offset)) {
					asgnssv(&tssv_p->ssv);
					tssv_p = tssv_p->next;
				}
				/* update offset to be to the following chord */
				offset = radd(offset, ch_p->duration);

				/*
				 * Loop through the linked list of GRPSYLs
				 * hanging off this chord.  Skip the syllables;
				 * just deal with the groups.
				 */
				gs1_p = ch_p->gs_p;
				staff = 0;	/* before first staff */
				for (;;) {
					/* find first group on next staff */
					while (gs1_p != 0 &&
					       gs1_p->staffno == staff) {
						gs1_p = gs1_p->gs_p;
					}
					if (gs1_p == 0) {  /* no next staff */
						break;
					}

					/* remember this new staff number */
					staff = gs1_p->staffno;

					/* tab ignores vcombine */
					if (is_tab_staff(staff)) {
						continue;
					}

					/* 1-line ignores vcombine */
					if (svpath(staff, STAFFLINES)->
							stafflines == 1) {
						continue;
					}

					/* if no groups, only syls, ignore */
					if (gs1_p->grpsyl == GS_SYLLABLE) {
						continue;
					}

					/* skip staff if only one voice */
					if (svpath(staff, VSCHEME)->vscheme
							== V_1) {
						continue;
					}

					/* get the staff's MLL struct */
					staffmll_p = chmgrp2staffm(
								mll_p, gs1_p);

					switch (pass) {
					case 1:
						/*
						 * Set individual combineability
						 * in the "COMB" field.
						 */
						chkhand1(gs1_p, staffmll_p);
						break;
					case 2:
						/*
						 * Apply the neighbor rules.
						 */
						chkhand2(gs1_p, staffmll_p);
						break;
					case 3:
						/*
						 * Do the actual combining.
						 */
						dohand(gs1_p, staffmll_p);
						break;
					}
				}
			}
		}
	}
}

/*
 * Name:        chkhand1()
 *
 * Abstract:    Find which voices are combineable, ignoring neighbor rules.
 *
 * Returns:     void
 *
 * Description: This function finds out which groups in this hand can be
 *		combined, of the ones requested by the voicecombine parameter,
 *		but ignoring neighbor rules.  It stores the results in
 *		the COMB field of all the groups.
 */

static void
chkhand1(gs1_p, mll_p)

struct GRPSYL *gs1_p;		/* first group in this hand */
struct MAINLL *mll_p;		/* MLL struct for this group */

{
	/*
	 * Index the following by voice number to find the expected "height" of
	 * the voices, relative to each other.  A higher number means we expect
	 * that that voice should normally be higher.
	 */				   /* v1 v2 v3 */
	static int pitch[MAXVOICES + 1] = { 0, 3, 1, 2 };

	struct SSV *ssv_p;		/* the SSV with voice combine info */
	struct GRPSYL *gs_p;		/* point along GRPSYLs of the chord */
	/* index the following by vno; it points at the GRPSYL for this vno */
	struct GRPSYL *v_p[MAXVOICES + 1];
	int slot1, slot2;		/* indices into SSV's vcombine array */
	int firstvno, secondvno;	/* two voices from ssv_p->vcombine[] */
	int combineable;		/* YES or NO */
	int comb;			/* accumulate combineable bits */


	/* point at each group; if only one, no combining can be done */
	if (setgrpptrs(gs1_p, v_p) == 1) {
		return;
	}

	/* get the list of groups we want to combine */
	ssv_p = svpath(gs1_p->staffno, VCOMBINE);

	/* for each pair of voices to be combined, see if they can be */
	comb = 0;
	for (slot1 = 0; slot1 < MAXVOICES; slot1++) {
		firstvno = ssv_p->vcombine[slot1];
		if (firstvno == 0) {
			break;	/* end of list, done with outer loop */
		}
		/* voice must have a group in this chord */
		if (v_p[firstvno] == 0) {
			continue;	/* skip this iteration */
		}
		for (slot2 = slot1 + 1; slot2 < MAXVOICES; slot2++) {
			secondvno = ssv_p->vcombine[slot2];
			if (secondvno == 0) {
				break;	/* end of list, done with inner loop */
			}
			/* voice must have a group in this chord */
			if (v_p[secondvno] == 0) {
				continue;	/* skip this iteration */
			}
			/* must pass higher voice first */
			if (pitch[firstvno] > pitch[secondvno]) {
				combineable = chk2groups(v_p[firstvno],
					v_p[secondvno], mll_p,
					ssv_p->vcombinequal);
			} else {
				combineable = chk2groups(v_p[secondvno],
					v_p[firstvno], mll_p,
					ssv_p->vcombinequal);
			}

			/* if this pair was combineable, remember that */
			if (combineable == YES) {
				comb |= 1 << (firstvno + secondvno);
			}
		}
	}

	/* save comb in all the groups, for easy access */
	for (gs_p = gs1_p; gs_p != 0 && gs_p->staffno == gs1_p->staffno &&
			gs_p->grpsyl == GS_GROUP; gs_p = gs_p->gs_p) {
		gs_p->COMB = comb;
	}
}

/*
 * Name:        chk2groups()
 *
 * Abstract:    Find whether two groups are combineable.
 *
 * Returns:     YES or NO
 *
 * Description: This function finds out if the given groups can be combined,
 *		ignoring beam and tie/slur issues.  If either has preceding
 *		grace groups, they are also checked (via recursive calls) and
 *		they must also be able to be combined, to get a YES answer.
 */

static int
chk2groups(hi_p, lo_p, mll_p, qual)

struct GRPSYL *hi_p;		/* group that should have higher pitch */
struct GRPSYL *lo_p;		/* group that should have lower pitch */
struct MAINLL *mll_p;		/* MLL struct for this group */
int qual;			/* voice combine qualifier */

{
	struct GRPSYL *phi_p;	/* group before hi_p */
	struct GRPSYL *plo_p;	/* group before lo_p */
	struct MAINLL *tempmll_p; /* temp copy of mll_p */
	int widx;		/* index into "with" lists */
	int hidx, lidx;		/* index into note lists */
	int mintop;		/* steps above c0 of top group's lowest note */
	int maxbot;		/* steps above c0 of bot group's highest note */
	int n;			/* loop variable */
	struct NOTE *hnote_p;	/* point at a note in the high group */
	struct NOTE *lnote_p;	/* point at a note in the low group */
	struct SLURTO *hslur_p;	/* point at a slur in the high group */
	struct SLURTO *lslur_p;	/* point at a slur in the high group */


	/*
	 * Since the groups are in the same chord, we know they start at the
	 * same time.  Require that they end at the same time.
	 */
	if (NE(hi_p->fulltime, lo_p->fulltime)) {
		return (NO);
	}

	/* covered by the fulltime check, except for bizarre tuplet cases */
	if (hi_p->basictime != lo_p->basictime) {
		return (NO);
	}
	if (hi_p->dots != lo_p->dots) {
		return (NO);
	}

	/* don't allow "mr" and "1r" (for example) to combine */
	if (hi_p->is_meas != lo_p->is_meas) {
		return (NO);
	}

	/* tuploc must agree */
	if (hi_p->tuploc != lo_p->tuploc) {
		return (NO);
	}

	/* tupcont must agree */
	if (hi_p->tupcont != lo_p->tupcont) {
		return (NO);
	}

	/* if printtup is set in both groups, they must agree */
	if (hi_p->printtup != PT_DEFAULT && lo_p->printtup != PT_DEFAULT &&
	    hi_p->printtup != lo_p->printtup) {
		return (NO);
	}

	/* if tupside is set in both groups, they must agree */
	if (hi_p->tupside != PL_UNKNOWN && lo_p->tupside != PL_UNKNOWN &&
	    hi_p->tupside != lo_p->tupside) {
		return (NO);
	}

	/* don't allow cross staff beamed groups */
	if (hi_p->beamto != CS_SAME || lo_p->beamto != CS_SAME) {
		return (NO);
	}

	/* don't allow cross staff stemmed groups */
	if (hi_p->stemto != CS_SAME || lo_p->stemto != CS_SAME) {
		return (NO);
	}

	/* don't allow any ties/slurs from/to other voices */
	if (tieslur_othervoice(hi_p) == YES ||
	    tieslur_othervoice(lo_p) == YES) {
		return (NO);
	}

	/* if either is a space, there can be no other conflict */
	if (hi_p->grpcont == GC_SPACE || lo_p->grpcont == GC_SPACE) {
		return (YES);
	}

	/***** both groups are GC_NOTES and/or GC_REST *****/

	/* group size must agree */
	if (hi_p->grpsize != lo_p->grpsize) {
		return (NO);
	}

	/* if both are rests, there can be no other conflict */
	if (hi_p->grpcont == GC_REST && lo_p->grpcont == GC_REST) {
		return (YES);
	}

	/* if only one is a rest, there is a conflict */
	if (hi_p->grpcont == GC_REST || lo_p->grpcont == GC_REST) {
		return (NO);
	}

	/***** both groups are GC_NOTES *****/

	/* if only rests are allowed to combine, fail */
	if (qual == VC_RESTSONLY) {
		return (NO);
	}

	/* head shape must agree */
	if (hi_p->headshape != lo_p->headshape) {
		return (NO);
	}

	/* beaming must be the same on both, except on grace beamloc doesn't
	 * matter, since if we combine them we'll beam them all anyway */
	if (hi_p->beamloc != lo_p->beamloc && hi_p->grpvalue == GV_NORMAL) {
		return (NO);
	}
	if (hi_p->breakbeam != lo_p->breakbeam) {
		return (NO);
	}
	if (hi_p->beamslope != NOBEAMANGLE && lo_p->beamslope != NOBEAMANGLE &&
	    hi_p->beamslope != lo_p->beamslope) {
		return (NO);
	}

	/* if both stemdirs are set, they must agree */
	if (hi_p->stemlen != STEMLEN_UNKNOWN &&
	    lo_p->stemlen != STEMLEN_UNKNOWN &&
	    hi_p->stemlen != lo_p->stemlen) {
		return (NO);
	}

	/* if both stemdirs are forced, they must agree */
	if (hi_p->stemdir != UNKNOWN && lo_p->stemdir != UNKNOWN &&
	    hi_p->stemdir != lo_p->stemdir) {
		return (NO);
	}

	/* group ties out of this group must agree */
	if (hi_p->tie != lo_p->tie) {
		return (NO);
	}

	/* group ties into this group must agree */
	tempmll_p = mll_p;
	phi_p = prevgrpsyl(hi_p, &tempmll_p);
	tempmll_p = mll_p;
	plo_p = prevgrpsyl(lo_p, &tempmll_p);
	if (phi_p != 0 && plo_p != 0 && phi_p->tie != plo_p->tie) {
		return (NO);
	}

	/* tremelo slashes or alternation slashes must agree */
	if (hi_p->slash_alt != lo_p->slash_alt) {
		return (NO);
	}

	/* neither group can have a horizontal offset */
	if (hi_p->ho_usage != HO_NONE || lo_p->ho_usage != HO_NONE) {
		return (NO);
	}

	/* "with" lists must both be nonexistent, or else identical */
	if (hi_p->nwith != lo_p->nwith) {
		return (NO);
	}
	for (widx = 0; widx < hi_p->nwith; widx++) {
		if (strcmp(hi_p->withlist[widx].string,
			   lo_p->withlist[widx].string) != 0 ||
		    hi_p->withlist[widx].place != lo_p->withlist[widx].place) {
			return (NO);
		}
	}

	/*
	 * Rolls aren't really implemented for voice 3.  Voice 3 is just along
	 * for the ride, if it's in the middle of a roll.  So we'll let it
	 * combine as long as both groups are in a roll, or not.  But for
	 * voices 1 and 2, only allow specific combinations that make sense.
	 */
	if (hi_p->vno == 1 && lo_p->vno == 2) {
		switch ((hi_p->roll << 8) | lo_p->roll) {
		case (NOITEM	<< 8) | NOITEM:
			break;
		case (STARTITEM << 8) | INITEM:
		case (STARTITEM << 8) | ENDITEM:
		case (INITEM	<< 8) | INITEM:
		case (INITEM	<< 8) | ENDITEM:
			if (hi_p->rolldir != lo_p->rolldir) {
				return (NO);
			}
			break;
		default:
			return (NO);
		}
	} else {
		if ((hi_p->roll == NOITEM) != (lo_p->roll == NOITEM)) {
			return (NO);
		}
	}

	/*
	 * If each group is preceded by a grace group, those grace groups must
	 * also be combineable.  Notice that if there are multiple preceding
	 * grace groups, more recursive calls will occur.  If one is preceded
	 * and the other isn't, there is no problem and nothing to check.
	 */
	if (hi_p->prev != 0 && hi_p->prev->grpvalue == GV_ZERO &&
	    lo_p->prev != 0 && lo_p->prev->grpvalue == GV_ZERO) {
		if (chk2groups(hi_p->prev, lo_p->prev, mll_p, qual) == NO) {
			return (NO);
		}
	}

	/* if either is a mrpt, the other must be, and there is no conflict */
	if (hi_p->is_meas == YES) {
		return (YES);
	}

	/***** both groups have a notelist *****/

	/*
	 * Do some checks involving overlapping of the groups.  We'd like to
	 * use stepsup, but it won't be set to its correct value until
	 * setnotes.c.  Fortunately, we don't need to know the absolute
	 * vertical position, only the relative.  So just use steps above c0.
	 */
	mintop = hi_p->notelist[hi_p->nnotes - 1].octave * 7 +
		Letshift[hi_p->notelist[hi_p->nnotes - 1].letter - 'a'];
	maxbot = lo_p->notelist[0].octave * 7 +
		Letshift[lo_p->notelist[0].letter - 'a'];
	/*
	 * If the lowest note of the high group is higher than the highest of
	 * the low group plus 1, we are okay regardless of the vcombine
	 * qualifier, and there are no shared notes to check.
	 */
	if (mintop > maxbot + 1) {
		return (YES);
	}

	/* if separated by one step . . . */
	if (mintop == maxbot + 1) {
		if (qual == VC_STEPSAPART) {
			return (NO);
		} else {
			return (YES);	/* no shared notes to check */
		}
	}

	/* if they are equal, and parm doesn't allow it, fail */
	if (mintop == maxbot &&
			(qual == VC_NOOVERLAP || qual == VC_STEPSAPART)) {
		return (NO);
	}

	/* if there is true overlap, and parm doesn't allow it, fail */
	if (mintop < maxbot && qual != VC_OVERLAP) {
		return (NO);
	}

	/*
	 * Any shared notes must be compatible, so check every note in the top
	 * group against every note in the bottom group.  If any are equal,
	 * they must be compatible.
	 */
	for (hidx = 0; hidx < hi_p->nnotes; hidx++) {

		hnote_p = &hi_p->notelist[hidx];

		for (lidx = 0; lidx < lo_p->nnotes; lidx++) {

			lnote_p = &lo_p->notelist[lidx];

			/* if the notes aren't equal, don't check them */
			if (hnote_p->octave != lnote_p->octave ||
			    hnote_p->letter != lnote_p->letter) {
				/* the notes differ; no problem */
				continue;
			}

			/* notes match; check all the items that must agree */

			/* any slurs they have must be the same */
			if (hnote_p->nslurto != lnote_p->nslurto) {
				return (NO);
			}
			for (n = 0; n < hnote_p->nslurto; n++) {
				hslur_p = &hnote_p->slurtolist[n];
				lslur_p = &lnote_p->slurtolist[n];
				if (hslur_p->letter != lslur_p->letter) {
					return (NO);
				}
				if (hslur_p->octave != lslur_p->octave) {
					return (NO);
				}
				if (hslur_p->slurstyle != lslur_p->slurstyle) {
					return (NO);
				}
				if (hslur_p->slurdir != lslur_p->slurdir) {
					return (NO);
				}
			}

			/* must have same accidental, or none */
			if ( ! eq_accs(hnote_p->acclist, lnote_p->acclist)) {
				return (NO);
			}

			/* head shape override, or none, must agree */
			if (hnote_p->headshape != lnote_p->headshape) {
				return (NO);
			}

			/* note size must agree */
			if (hnote_p->notesize != lnote_p->notesize) {
				return (NO);
			}

			/* tie related attributes must be the same */
			if (hnote_p->tie != lnote_p->tie) {
				return (NO);
			}
			if (hnote_p->tiestyle != lnote_p->tiestyle) {
				return (NO);
			}
			if (hnote_p->tiedir != lnote_p->tiedir) {
				return (NO);
			}
			if (hnote_p->is_bend != lnote_p->is_bend) {
				return (NO);
			}
			if (hnote_p->smallbend != lnote_p->smallbend) {
				return (NO);
			}
		}
	}

	return (YES);	/* passed all tests, the groups are combineable */
}

/*
 * Name:        chkhand2()
 *
 * Abstract:    Make final decision on what can be combined in this hand.
 *
 * Returns:     void
 *
 * Description: This function applies the additional rules about beaming and
 *		ties/slurs to the results of chkhand1(), to decide what groups
 *		in this hand truly should be combined.  It stores the results
 *		in the COMB field of all the groups.
 */

static void
chkhand2(gs1_p, mll_p)

struct GRPSYL *gs1_p;		/* first group in this hand */
struct MAINLL *mll_p;		/* MLL struct for this group */

{
	struct SSV *ssv_p;		/* the SSV with voice combine info */
	/* index the following by vno; it points at the GRPSYL for this vno */
	struct GRPSYL *v_p[MAXVOICES + 1];
	int slot1, slot2;		/* indices into SSV's vcombine array */
	int firstvno, secondvno;	/* two voices from ssv_p->vcombine[] */
	int keep_going;			/* try some more? */


	/* if we already know no combining can be done, get out */
	if (gs1_p->COMB == 0) {
		return;
	}

	/* point at all the groups in this hand */
	(void)setgrpptrs(gs1_p, v_p);

	/* get the list of groups we want to combine */
	ssv_p = svpath(gs1_p->staffno, VCOMBINE);

	/* for each pair of voices to be combined, apply the neighbor rules */
	for (slot1 = 0; slot1 < MAXVOICES; slot1++) {
		firstvno = ssv_p->vcombine[slot1];
		if (firstvno == 0) {
			break;	/* end of list, done with outer loop */
		}
		/* voice must have a group in this chord */
		if (v_p[firstvno] == 0) {
			continue;	/* skip this iteration */
		}
		for (slot2 = slot1 + 1; slot2 < MAXVOICES; slot2++) {
			secondvno = ssv_p->vcombine[slot2];
			if (secondvno == 0) {
				break;	/* end of list, done with inner loop */
			}
			/* voice must have a group in this chord */
			if (v_p[secondvno] == 0) {
				continue;	/* skip this iteration */
			}
			/*
			 * The group with the lower voice number should be the
			 * one that absorbs the other.  Pass it first.
			 */
			if (firstvno < secondvno) {
				keep_going = chk2neighbors(v_p[firstvno],
						v_p[secondvno], gs1_p, mll_p);
			} else {
				keep_going = chk2neighbors(v_p[secondvno],
						v_p[firstvno], gs1_p, mll_p);
			}

			/* get out now if we are told to */
			if (keep_going == NO) {
				return;
			}
		}
	}
}

/*
 * Name:        chk2neighbors()
 *
 * Abstract:    Apply beaming and tie/slur rules to two groups in a hand.
 *
 * Returns:     YES or NO: should any further combining be attempted?
 *
 * Description: This function checks whether the two groups should be combined,
 *		by looking at neighboring groups.  It stores the answer in the
 *		COMB field of all the groups.
 *
 *		We return YES if the caller can call again with a different
 *		pair, or NO if not.  (Only two combinings can be done per hand.)
 */

static int
chk2neighbors(dest_p, src_p, gs1_p, mll_p)

struct GRPSYL *dest_p;		/* group that should absorb the other */
struct GRPSYL *src_p;		/* group that should be absorbed */
struct GRPSYL *gs1_p;		/* first group in staff/chord */
struct MAINLL *mll_p;		/* MLL struct for these groups' staff */

{
	struct GRPSYL *d2_p, *s2_p;	/* point at neighboring groups */
	struct GRPSYL *gs_p;		/* point at groups in this chord */
	struct MAINLL *mll2s_p, *mll2d_p, *mll3_p; /* more pointers into MLL */
	short mask;			/* to be applied to "COMB" */
	short comb;			/* hold combineability bits */
	int keep_going;			/* return code */
	int vcombinemeas;		/* keep consistent within measure? */


	vcombinemeas = svpath(gs1_p->staffno, VCOMBINE)->vcombinemeas;

	/*
	 * Set up the bit that we must check in each chord to see if these two
	 * voices are combineable.
	 */
	mask = 1 << (dest_p->vno + src_p->vno);

	/*
	 * Start at this chord, and keep looking to the right as long as the
	 * there is a need to be consistent with the neighboring groups.
	 */
	mll2s_p = mll2d_p = mll_p;
	for (d2_p = dest_p, s2_p = src_p; d2_p != 0 && s2_p != 0;
			d2_p = nextglobnongrace(d2_p, &mll2d_p),
			s2_p = nextglobnongrace(s2_p, &mll2s_p)) {
		/*
		 * If the groups in that chord are not combineable, we must not
		 * combine our original groups either.  The same flags are set
		 * in d2_p and s2_p, so we could have checked either.
		 */
		if ((d2_p->COMB & mask) == 0) {
			return (YES);
		}

		/*
		 * If we are supposed to keep the combining consistent
		 * throughout the measure, and we are not yet at the edge of
		 * the measure, keep going.  We don't have to check both the
		 * source and the dest; they would give the same answer,
		 * as long as we made it through pass 1 as combineable.
		 */
		if (vcombinemeas == YES && nextnongrace(d2_p) != 0) {
			continue;
		}

		/*
		 * If the groups are not beamed or tied/slurred to the right,
		 * we don't have to look any farther.  For beams, we only have
		 * to check one group, since the other must agree or we
		 * wouldn't be here.  For ties/slurs, we only have to check the
		 * the src, since those are the ones that wouldn't work if
		 * these groups were combined and the next ones weren't.
		 */
		if ((s2_p->beamloc == NOITEM || s2_p->beamloc == ENDITEM) &&
		    hastieslur(s2_p) == NO && tieslur_othervoice(s2_p) == NO) {
			break;
		}
	}

	/*
	 * Start at this chord, and keep looking to the left as long as the
	 * there is a need to be consistent with the neighboring groups.
	 */
	mll2s_p = mll2d_p = mll_p;
	for (d2_p = dest_p, s2_p = src_p; d2_p != 0 && s2_p != 0;
			d2_p = prevglobnongrace(d2_p, &mll2d_p),
			s2_p = prevglobnongrace(s2_p, &mll2s_p)) {

		if ((d2_p->COMB & mask) == 0) {
			return (YES);
		}

		if (vcombinemeas == YES && prevnongrace(d2_p) != 0) {
			continue;
		}

		mll3_p = mll2s_p;	/* don't alter mll2s_p */
		if ((s2_p->beamloc == NOITEM || s2_p->beamloc == STARTITEM) &&
		    hastieslur(prevglobnongrace(s2_p, &mll3_p)) == NO &&
		    tieslur_othervoice(s2_p) == NO) {
			break;
		}
	}

	/* for convenience, copy the COMB bits into a local variable */
	comb = dest_p->COMB;

	/*
	 * We passed all the tests, so remember that we should combine the
	 * original groups.  Also decide whether any more should be tried.
	 */
	if (((comb >> SRC1SHIFT) & 0x3) == 0) {
		/* first groups to be combined in this hand */
		comb |=  src_p->vno <<  SRC1SHIFT;
		comb |= dest_p->vno << DEST1SHIFT;
		keep_going = YES;
	} else {
		/* second groups to be combined in this hand */
		comb |=  src_p->vno <<  SRC2SHIFT;
		comb |= dest_p->vno << DEST2SHIFT;
		keep_going = NO;	/* only 2 combinings can be done */
	}

	/*
	 * Since we will combine these two groups, the new dest group has
	 * inherited any incompatibility that src might have had with the
	 * other group (if any).  So find the new value of the comb bits.
	 * Set this new comb value into all the groups in this hand.  We do
	 * this for the benefit of later chords on this staff, if they are
	 * beamed or tied/slurred to us.
	 */
	switch (dest_p->vno + src_p->vno) {
	case 1 + 2:	/* combined 2 into 1 */
		/* if 2 and 3 can't combine, now 1 and 3 can't */
		if ((comb & (1 << (2 + 3))) == 0) {
			comb &= ~(1 << (1 + 3));
		}
		break;
	case 1 + 3:	/* combined 3 into 1 */
		/* if 3 and 2 can't combine, now 1 and 2 can't */
		if ((comb & (1 << (3 + 2))) == 0) {
			comb &= ~(1 << (1 + 2));
		}
		break;
	case 2 + 3:	/* combined 3 into 2 */
		/* if 3 and 1 can't combine, now 2 and 1 can't */
		if ((comb & (1 << (3 + 1))) == 0) {
			comb &= ~(1 << (2 + 1));
		}
		break;
	}
	for (gs_p = gs1_p; gs_p != 0 && gs_p->staffno == gs1_p->staffno &&
			gs_p->grpsyl == GS_GROUP; gs_p = gs_p->gs_p) {
		gs_p->COMB = comb;
	}

	return (keep_going);
}

/*
 * Name:        dohand()
 *
 * Abstract:    Combine the appropriate GRPSYLs in one chord on one staff.
 *
 * Returns:     void
 *
 * Description: This function combines GRPSYLs in one chord on one staff.  It
 *		uses the previously determined list of what is combineable in
 *		gs1_p->COMB.
 */

static void
dohand(gs1_p, mll_p)

struct GRPSYL *gs1_p;		/* first group on this staff in this chord */
struct MAINLL *mll_p;		/* MLL struct for this group */

{
	/* index the following by vno; it points at the GRPSYL for this vno */
	struct GRPSYL *v_p[MAXVOICES + 1];
	int srcv, destv;	/* src and dest voice numbers */


	/* if we already know no combining can be done, get out */
	if (gs1_p->COMB == 0) {
		return;
	}

	/* point at all the groups in this hand */
	(void)setgrpptrs(gs1_p, v_p);

	/* get first src group's voice number */
	srcv = (gs1_p->COMB >> SRC1SHIFT) & 0x3;

	if (srcv != 0) {
		/* find the dest's voice and combine */
		destv = (gs1_p->COMB >> DEST1SHIFT) & 0x3;
		comb2groups(v_p[destv], v_p[srcv], mll_p);

		/* get second src group's voice number */
		srcv = (gs1_p->COMB >> SRC2SHIFT) & 0x3;

		if (srcv != 0) {
			/* find the dest's voice and combine */
			destv = (gs1_p->COMB >> DEST2SHIFT) & 0x3;
			comb2groups(v_p[destv], v_p[srcv], mll_p);
		}
	}
}

/*
 * Name:        comb2groups()
 *
 * Abstract:    Combine one group into another.
 *
 * Returns:     void
 *
 * Description: This function combines the src group into the dest group,
 *		including handling any preceding grace groups.
 */

static void
comb2groups(dest_p, src_p, mll_p)

struct GRPSYL *dest_p;		/* group that should absorb the other */
struct GRPSYL *src_p;		/* group that should be absorbed */
struct MAINLL *mll_p;		/* MLL struct for these groups' staff */

{
	struct GRPSYL *dgrace_p;	/* dest grace group */
	struct GRPSYL *sgrace_p;	/* src grace group */
	struct GRPSYL *dmem_p;		/* remember a group from dest voice */
	struct GRPSYL *smem_p;		/* remember a group from src voice */
	struct GRPSYL *temp_p;		/* temp pointer */
	struct GRPSYL *fgs_p;		/* first grace in src voice */


	/*
	 * Add the src into the dest, and make the src group into a space if it
	 * isn't already a space.
	 */
	addsrc2dest(dest_p, src_p);
	if (src_p->grpcont != GC_SPACE) {
		src_p->grpcont = GC_SPACE;
		src_p->grpvalue = GV_NORMAL;
		src_p->grpsize = GS_NORMAL;
		src_p->headshape = HS_UNKNOWN;
		src_p->beamloc = NOITEM;
		src_p->beamslope = NOBEAMANGLE;
		src_p->stemlen = STEMLEN_UNKNOWN;
		src_p->stemdir = UNKNOWN;
		src_p->breakbeam = NO;
		src_p->beamloc = NOITEM;
		src_p->notelist = 0;
		src_p->nnotes = 0;
		src_p->tie = NO;
		src_p->slash_alt = 0;
		src_p->restdist = NORESTDIST;
		src_p->nwith = 0;
		src_p->roll = NOITEM;
	}

	/* if source has no grace notes, we're done */
	if (src_p->prev == 0 || src_p->prev->grpvalue == GV_NORMAL) {
		return;
	}

	/*
	 * Loop through the matching grace groups preceding the src and dest
	 * groups, adding the src into the dest.  Remember the leftmost pair,
	 * or the nongrace groups if there are no matching pairs.
	 */
	dmem_p = dest_p;
	smem_p = src_p;
	for (sgrace_p = src_p->prev, dgrace_p = dest_p->prev;
	     sgrace_p != 0 && sgrace_p->grpvalue == GV_ZERO &&
	     dgrace_p != 0 && dgrace_p->grpvalue == GV_ZERO;
	     sgrace_p = sgrace_p->prev, dgrace_p = dgrace_p->prev) {

		addsrc2dest(dgrace_p, sgrace_p);
		dmem_p = dgrace_p;	/* remember last one done */
		smem_p = sgrace_p;
	}

	if (sgrace_p != 0 && sgrace_p->grpvalue == GV_ZERO) {
		/* extra src grace group(s) left over must be moved */

		/* sgrace_p is the last (rightmost) grace to move */
		fgs_p = 0;	/* to keep lint happy; this loop will set it */
		for (temp_p = sgrace_p; temp_p != 0 && temp_p->grpvalue ==
				GV_ZERO; temp_p = temp_p->prev) {
			fgs_p = temp_p;
		}
		/* fgs_p is the first (leftmost) grace to move */

		if (dgrace_p == 0) {	/* at start of measure? */
			/* in src voice, skip over all the graces */
			mll_p->u.staff_p->groups_p[src_p->vno-1] = src_p;
			src_p->prev = 0;

			/* splice the left grace into the dest voice */
			mll_p->u.staff_p->groups_p[dest_p->vno-1] = fgs_p;
			fgs_p->prev = 0;
		} else {
			/* in src voice, skip over all the graces */
			fgs_p->prev->next = src_p;
			src_p->prev = fgs_p->prev;

			/* splice the left grace into the dest voice */
			dgrace_p->next = fgs_p;
			fgs_p->prev = dgrace_p;
		}

		/* splice the rightmost left over grace into the dest voice */
		dmem_p->prev = sgrace_p;
		sgrace_p->next = dmem_p;

		/*
		 * If there is now more than one grace in the dest, we have to
		 * correct beamloc in a couple places.
		 */
		if (fgs_p != dest_p->prev) {
			/*
			 * If the dest voice had multiple graces before, the
			 * leftmost one, which was STARTITEM, must be made
			 * INITEM.  If it had only one, it was NOITEM and
			 * must be made ENDITEM, but go ahead and set INITEM;
			 * it'll be set correctly below.
			 */
			if (dmem_p != dest_p) {	  /* if dest had grace(s) */
				dmem_p->beamloc = INITEM;
			}

			/* rightmost dest grace must always be made ENDITEM */
			dest_p->prev->beamloc = ENDITEM;
		}

		/* change the voice number of the moved graces */
		for (temp_p = fgs_p; temp_p != sgrace_p->next;
				temp_p = temp_p->next) {
			temp_p->vno = dest_p->vno;
		}
	} else {
		/* no extra src grace group(s); just remove all src graces */
		if (sgrace_p == 0) {
			mll_p->u.staff_p->groups_p[src_p->vno-1] = src_p;
			src_p->prev = 0;
		} else {
			smem_p->prev->next = src_p;
			src_p->prev = smem_p->prev;
		}
	}
	/* could free the src graces, but is it worth the trouble? */
}

/*
 * Name:        addsrc2dest()
 *
 * Abstract:    Adds src group to dest group.
 *
 * Returns:     void
 *
 * Description: This function adds the src group info (if the src group exists)
 *		into the dest group.  But it doesn't mess with linkages.  That
 *		is the calling function's job.
 */

static void
addsrc2dest(dest_p, src_p)

struct GRPSYL *dest_p;		/* destination group must exist */
struct GRPSYL *src_p;		/* source group, or can be NULL */

{
	struct NOTE *dnote_p;	/* point at a note in the destination group */
	struct NOTE *snote_p;	/* point at a note in the source group */
	struct NOTE *newlist;	/* combined notelist */
	int didx, sidx;		/* index into dest and src note lists */
	int totnotes;		/* total number of notes after combining */
	int n;			/* loop variable */


	/* if src doesn't exist, there is nothing to do */
	if (src_p == 0) {
		return;
	}

	/* move references to the src group's coords */
	upd_ref(src_p->c, dest_p->c);

	/* if src is space, nothing to do except carry over this space field */
	if (src_p->grpcont == GC_SPACE) {
		if (dest_p->grpcont == GC_SPACE &&
				src_p->uncompressible == YES) {
			dest_p->uncompressible = YES;
		}
		return;
	}

	/* padding is the max of the two groups */
	dest_p->padding = MAX(dest_p->padding, src_p->padding);

	/*
	 * Some fields were allowed to disagree between the groups as long as
	 * one was defaulted.  The non-default value should prevail.
	 */
	if (src_p->printtup != PT_DEFAULT) {
		dest_p->printtup = src_p->printtup;
	}
	if (src_p->tupside != PL_UNKNOWN) {
		dest_p->tupside = src_p->tupside;
	}
	if (src_p->beamslope != NOBEAMANGLE) {
		dest_p->beamslope = src_p->beamslope;
	}
	if (src_p->stemlen != STEMLEN_UNKNOWN) {
		dest_p->stemlen = src_p->stemlen;
	}
	if (src_p->stemdir != UNKNOWN) {
		dest_p->stemdir = src_p->stemdir;
	}

	/* only when combining 1 & 2 might we have to adjust roll */
	if (dest_p->vno == 1 && src_p->vno == 2) {
		switch ((dest_p->roll << 8) | src_p->roll) {
		case (NOITEM	<< 8) | NOITEM:
			/* dest remains NOITEM */
			break;
		case (STARTITEM << 8) | INITEM:
			/* dest remains STARTITEM */
			break;
		case (STARTITEM << 8) | ENDITEM:
			dest_p->roll = LONEITEM;
			break;
		case (INITEM	<< 8) | INITEM:
			/* dest remains INITEM */
			break;
		case (INITEM	<< 8) | ENDITEM:
			dest_p->roll = ENDITEM;
			break;
		}
	}

	/* if either is a rest, the other must be rest or space; combine */
	if (dest_p->grpcont == GC_REST || src_p->grpcont == GC_REST) {
		if (dest_p->grpcont == GC_REST) {
			/* throw away any restdist; probably inappropriate */
			dest_p->restdist = 0;
		} else {
			/* make dest a rest; allocate restc for it */
			dest_p->grpcont = GC_REST;
			MALLOCA(float, dest_p->restc, NUMCTYPE);
		}
		return;
	}

	/* if either is a mrpt, the other must be; nothing to copy */
	if (dest_p->is_meas == YES) {
		return;
	}

	/* at this point we know src has notes, & dest has notes or is space */
	dest_p->grpcont = GC_NOTES;	/* the result will be notes */

	/*
	 * Make a new notelist.  First copy the old dest notes into there, and
	 * then add the src notes.  We can't just realloc the dest, since
	 * pointers into the array must be updated by calling upd_ref for each
	 * array element.  If any notes are shared, we won't really need this
	 * many, but it doesn't waste much memory.
	 */
	MALLOC(NOTE, newlist, dest_p->nnotes + src_p->nnotes);
	for (n = 0; n < dest_p->nnotes; n++) {
		newlist[n] = dest_p->notelist[n];
	}

	/*
	 * Keep track of the number of total resulting notes, which starts out
	 * as the number of notes in dest.  Loop through the src notes.
	 */
	totnotes = dest_p->nnotes;
	for (sidx = 0; sidx < src_p->nnotes; sidx++) {
		snote_p = &src_p->notelist[sidx];

		/* see if it matches any of the dest notes in the result array*/
		for (didx = 0; didx < dest_p->nnotes; didx++) {
			dnote_p = &newlist[didx];
			if (dnote_p->octave == snote_p->octave &&
			    dnote_p->letter == snote_p->letter) {
				/*
				 * If one has parens on the note or acc and the
				 * other doesn't, ensure no parens in result.
				 */
				if (dnote_p->note_has_paren !=
				    snote_p->note_has_paren) {
					dnote_p->note_has_paren = NO;
				}
				if (dnote_p->acc_has_paren !=
				    snote_p->acc_has_paren) {
					dnote_p->acc_has_paren = NO;
				}
				break;
			}
		}
		if (didx == dest_p->nnotes) {
			/* didn't find a match, so add it to the end */
			newlist[totnotes++] = *snote_p;
		}
	}

	/* sort the combined list in pitch order */
	Doing_tab_staff = is_tab_staff(dest_p->staffno); /* notecomp needs it */
	qsort((char *)newlist, (unsigned int)totnotes,
			sizeof (struct NOTE), notecomp);

	/*
	 * For every original dest and src note, find out where it ended up in
	 * the combined list, and update and location tag references.
	 */
	for (didx = 0; didx < dest_p->nnotes; didx++) {
		dnote_p = &dest_p->notelist[didx];
		for (n = 0; n < totnotes; n++) {
			if (newlist[n].octave == dnote_p->octave &&
			    newlist[n].octave == dnote_p->letter) {
				upd_ref(newlist[n].c, dnote_p->c);
			}
		}
	}
	for (sidx = 0; sidx < src_p->nnotes; sidx++) {
		dnote_p = &src_p->notelist[sidx];
		for (n = 0; n < totnotes; n++) {
			if (newlist[n].octave == dnote_p->octave &&
			    newlist[n].octave == dnote_p->letter) {
				upd_ref(newlist[n].c, dnote_p->c);
			}
		}
	}

	/* free old dest list, and install the new one and its note count */
	if (dest_p->notelist != 0) {	/* not a space */
		FREE(dest_p->notelist);
	}
	dest_p->notelist = newlist;
	dest_p->nnotes = totnotes;
}

/*
 * Name:        setgrpptrs()
 *
 * Abstract:    Set pointers to the groups in a hand.
 *
 * Returns:     number of GRPSYLs found in this hand
 *
 * Description: This function fill in the given array with pointers to the
 *		groups in this hand.  When a group doesn't exist for some
 *		voice, a null pointer is used.
 */

static int
setgrpptrs(gs1_p, v_p)

struct GRPSYL *gs1_p;		/* first group in this hand */
struct GRPSYL *v_p[];		/* fill this in, index by voice number */

{
	struct GRPSYL *gs_p;	/* point along GRPSYLs of the chord */
	int v;			/* a voice number */
	int num;		/* number of groups found */


	/* null out the list of pointers initially (note [0] is not used) */
	for (v = 1; v <= MAXVOICES; v++) {
		v_p[v] = 0;
	}
	/* for each group in the hand, set pointer to it */
	num = 0;
	for (gs_p = gs1_p; gs_p != 0 && gs_p->staffno == gs1_p->staffno &&
			gs_p->grpsyl == GS_GROUP; gs_p = gs_p->gs_p) {
		v_p[gs_p->vno] = gs_p;
		num++;
	}

	return (num);
}

/*
 * Name:        hastieslur()
 *
 * Abstract:    Is this group tied/slurred/bent to the next?
 *
 * Returns:     YES or NO
 *
 * Description: This function checks whether there are any ties/slurs/bends to
 *		the next group.  If the given group doesn't exist, the answer
 *		is NO.
 */

static int
hastieslur(gs_p)

struct GRPSYL *gs_p;		/* group to check */

{
	int idx;		/* index into note list */


	/* if there's no group, it isn't tied/slurred */
	if (gs_p == 0) {
		return (NO);
	}

	/* check group tie */
	if (gs_p->tie == YES) {
		return (YES);
	}

	/* check note ties and slurs (bends are covered by slurs) */
	for (idx = 0; idx < gs_p->nnotes; idx++) {
		if (gs_p->notelist[idx].tie == YES ||
		    gs_p->notelist[idx].nslurto != 0) {
			return (YES);
		}
	}

	return (NO);
}

/*
 * Name:        tieslur_othervoice()
 *
 * Abstract:    Is this note in this group tied/slurred from/to another voice?
 *
 * Returns:     YES or NO
 *
 * Description: This function checks whether any notes in this group have
 *		ties or slurs coming from, or going to, another voice.
 */

static int
tieslur_othervoice(gs_p)

struct GRPSYL *gs_p;		/* group to check */

{
	struct NOTE *note_p;	/* point at a note in the group */
	int idx;		/* index into note list */
	int n;			/* loop variable */


	/* if there's no group, it isn't tied/slurred */
	if (gs_p == 0) {
		return (NO);
	}

	for (idx = 0; idx < gs_p->nnotes; idx++) {
		note_p = &gs_p->notelist[idx];
		if (note_p->tied_from_other == YES) {
			return (YES);
		}
		if (note_p->slurred_from_other == YES) {
			return (YES);
		}
		if (note_p->tied_to_voice != NO_TO_VOICE) {
			return (YES);
		}
		for (n = 0; n < note_p->nslurto; n++) {
			if (note_p->slurtolist[n].slurred_to_voice !=
						NO_TO_VOICE) {
				return (YES);
			}
		}
	}

	return (NO);
}
