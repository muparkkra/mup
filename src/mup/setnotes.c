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
/*
 * Name:	setnotes.c
 *
 * Description:	This file contains functions for setting relative vertical
 *		locations of notes.  It also sets relative vertical locations
 *		of the groups that contain notes, considering only the notes.
 *		It also sets the directions of stems.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

static void locnotes P((void));
static void locllnotes P((struct MAINLL *mll_p, int v,
		struct MAINLL *nextbar_p));
static void chktabcollision P((struct GRPSYL *start_p));
static void intertab P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
static void setstems P((void));
static void setonestem P((struct GRPSYL *gs_p));
static void setopstem P((struct GRPSYL *gs1_p, struct GRPSYL *gs2_p));
static void setfreestem P((struct GRPSYL *gs1_p, struct GRPSYL *gs2_p));
static void set1freestem P((struct GRPSYL *this_p, struct GRPSYL *other_p,
		int stemdir));
static void favorstemdir P((struct GRPSYL *start_p, int stemdir));
static void dobunch P((struct GRPSYL *start_p, struct GRPSYL *end_p));
static void setmidlinestem P((struct GRPSYL *start_p));
static void dograce P((struct GRPSYL *gs1_p, struct GRPSYL *gs2_p));
static int setv3stem P((struct GRPSYL *gs_p, int stemdir));
static int dov3bunch P((struct GRPSYL *start_p, struct GRPSYL *end_p,
		int stemdir));
static void setheads P((void));
static void setvoiceheads P((struct MAINLL *mll_p, struct GRPSYL *gs_p,
		int stafflines, short *shapes, int is_tab, int allx_hsi,
		int sharps, int keylet));
static void fixoneline P((void));

/*
 * Name:        setnotes()
 *
 * Abstract:    Sets the relative vert coords of each note, and stem dir.
 *
 * Returns:     void
 *
 * Description: This function calls subroutines to set the relative vertical
 *		coordinates of each note and each note group (considering only
 *		the note heads in it at this point), stem directions, and which
 *		notehead characters to use.
 */

void
setnotes()

{
	debug(16, "setnotes");

	locnotes();
	setstems();
	setheads();
	fixoneline();
}

/*
 * Name:        locnotes()
 *
 * Abstract:    Sets the relative vertical coordinates of each note.
 *
 * Returns:     void
 *
 * Description: This function loops through the main linked list, finding every
 *		STAFF structure.  It calls a subroutine to process each list of
 *		list of GRPSYLs for groups (not syllables).
 */

static void
locnotes()

{
	register struct MAINLL *mainll_p; /* point item in main linked list */
	int v;				/* index to voice linked lists */
	int did_a_voice;		/* have we processed a voice in meas?*/
	struct TIMEDSSV *tssv_p;	/* point along a timed SSV list */
	struct MAINLL *nextbar_p;	/* the next bar in the MLL */


	debug(16, "locnotes");
	initstructs();			/* clean out old SSV info */

	did_a_voice = NO;	/* prevent useless "used before set" warning */
	nextbar_p = 0;		/* prevent useless "used before set" warning */

	/*
	 * Loop through the main linked list, processing voices.  MLL SSVs are
	 * assigned when encountered.  But we also have to handle timed SSVs,
	 * because they may change the clef.  This algorithm would be simpler
	 * if we called setssvstate() after each voice (to undo the timed
	 * SSVs), and then always reapplied them when we get to the next bar.
	 * But to save time, we don't undo them after the last voice, and so
	 * usually don't have to reassign them at the bar (unless all the
	 * visible voices had measure repeats, and so we never assigned any).
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			asgnssv(mainll_p->u.ssv_p);
			break;

		case S_CHHEAD:
			/* find the next bar line */
			for (nextbar_p = mainll_p; nextbar_p->str != S_BAR;
					nextbar_p = nextbar_p->next) {
				;
			}
			/* we haven't processed a voice in this measure yet */
			did_a_voice = NO;
			break;

		case S_STAFF:
			/* if invisible, there is nothing to do */
			if (mainll_p->u.staff_p->visible == NO) {
				break;
			}

			/* loop through the voice(s), and process each list */
			for (v = 0; v < MAXVOICES && mainll_p->u.staff_p
					->groups_p[v] != 0; v++) {

				/*
				 * If this is not the first voice we've done in
				 * this measure, and there are timed SSVs,
				 * locllnotes() assigned them when we were in
				 * there for the previous voice we did.  So set
				 * the SSVs back to the state they were in at
				 * the start of the measure.
				 */
				if (did_a_voice == YES && nextbar_p->u.bar_p->
							timedssv_p != 0){
					setssvstate(mainll_p);
				}
				locllnotes(mainll_p, v, nextbar_p);
				did_a_voice = YES;
			}
			break;

		case S_BAR:
			if (did_a_voice == NO) {
				for (tssv_p = mainll_p->u.bar_p->timedssv_p;
						tssv_p != 0;
						tssv_p = tssv_p->next) {
					asgnssv(&tssv_p->ssv);
				}
			}
			break;
		}
	}
}

/*
 * Name:        locllnotes()
 *
 * Abstract:    Set the "stepsup" field for the notes in one GRPSYL list.
 *
 * Returns:     void
 *
 * Description: This function goes down one of the linked lists of GRPSYLs,
 *		one that is for groups, not syllables, and sets the stepsup
 *		value.
 */

static void
locllnotes(mll_p, v, nextbar_p)

struct MAINLL *mll_p;		/* point at the MLL struct voice hangs off */
int v;				/* voice to loop through */
struct MAINLL *nextbar_p;	/* point at MLL for the next bar line */

{
	register int upfromc4;	/* steps up from middle C */
	register int n;		/* loop through all notes in a group */
	int s;			/* staff number */
	int slines;		/* lines in this staff */
	int clef;		/* the clef currently in operation */
	int newclef;		/* the new clef, in case it changes */
	struct GRPSYL *gs_p;	/* starts pointing at first GRPSYL in list */
	struct TIMEDSSV *tssv_p;/* point along a timed SSV list */
	RATIONAL offset;	/* current group's offset into measure */


	s = mll_p->u.staff_p->staffno;
	debug(32, "locllnotes file=%s line=%d staff=%d vidx=%d",
			mll_p->inputfile, mll_p->inputlineno, s, v);
	slines = svpath(s, STAFFLINES)->stafflines;

	/* find the initial clef for this staff */
	clef = svpath(s, CLEF)->clef;

	/* point at the first timed SSV for this measure, if there is one */
	tssv_p = nextbar_p->u.bar_p->timedssv_p;
	offset = Zero;		/* first group's offset into measure */

	/* loop through every group in this voice */
	for (gs_p = mll_p->u.staff_p->groups_p[v]; gs_p != 0;
				gs_p = gs_p->next) {

		/* if no timed SSVs, don't waste time doing the following */
		if (tssv_p != 0) {
			/* assign timed SSVs before current offset */
			while (tssv_p != 0 && LT(tssv_p->time_off, offset)) {
				asgnssv(&tssv_p->ssv);
				tssv_p = tssv_p->next;
			}

			/* get clef state just before this group */
			clef = svpath(s, CLEF)->clef;

			/* assign timed SSVs at current offset */
			while (tssv_p != 0 && EQ(tssv_p->time_off, offset)) {
				asgnssv(&tssv_p->ssv);
				tssv_p = tssv_p->next;
			}

			/* get clef for this group */
			newclef = svpath(s, CLEF)->clef;

			/*
			 * If the clef changed at this time, set it in GRPSYL.
			 * This could happen with multiple voices on the staff.
			 * If so, we'll later erase clef from all but one; but
			 * the choice depends on the coords, which we don't
			 * know yet, so that is done later.  The erasing is
			 * done in fixclef() in restsyl.c.
			 */
			if (newclef != clef) {
				clef = newclef;
				gs_p->clef = clef;
			}

			/* add our group's dur to get ready for next iteration*/
			offset = radd(offset, gs_p->fulltime);
		}

		/* nothing more to do for rests or spaces */
		if (gs_p->grpcont != GC_NOTES)
			continue;

		/*
		 * We found a group consisting of notes, normal or tablature.
		 * First handle the tablature case.
		 */
		if (clef == TABCLEF) {
			/*
			 * Make sure this voice's notes don't collide with
			 * some later voice's notes.
			 */
			chktabcollision(gs_p);

			for (n = 0; n < gs_p->nnotes; n++) {
				/*
				 * Set stepsup to be on the appropriate string.
				 */
				/* calc steps up from middle of staff */
				gs_p->notelist[n].stepsup = slines
					- 1 - 2 * gs_p->notelist[n].STRINGNO;

			}

			continue;
		}

		/*
		 * We found a non-tablature group consisting of notes.  For
		 * each note, find the number of steps it is up from middle C
		 * and from the center line of the staff.  (However, for 1-line
		 * staffs, we assume center line for now.)
		 * For CSS notes, we apply an offset to keep it far from the
		 * normal notes.  This is so that setgrps.c will understand
		 * that CSS and non-CSS notes in a group never interfere.
		 * Later, absvert.c will find the true stepsup on the other
		 * staff.
		 */
		for (n = 0; n < gs_p->nnotes; n++) {
			/* set steps up from middle line of staff */
			if (slines == 5) {
				/* get steps up from middle C */
				upfromc4 = (gs_p->notelist[n].octave - 4) * 7 +
				Letshift[ gs_p->notelist[n].letter - 'a' ];

				gs_p->notelist[n].stepsup = upfromc4
						+ clef - ALTO;
				if (gs_p->stemto == CS_ABOVE &&
						n <= gs_p->stemto_idx) {
					gs_p->notelist[n].stepsup += CSS_STEPS;
				} else if (gs_p->stemto == CS_BELOW &&
						n >= gs_p->stemto_idx) {
					gs_p->notelist[n].stepsup -= CSS_STEPS;
				}
			} else {
				/* 1-line staff; assume center line for now */
				gs_p->notelist[n].stepsup = 0;
			}
		}
	}

	/*
	 * Assign any timed SSVs that came after the last group, so that we are
	 * in the right state for the next measure (if we are the last voice).
	 */
	while (tssv_p != 0) {
		asgnssv(&tssv_p->ssv);
		tssv_p = tssv_p->next;
	}
}

/*
 * Name:        chktabcollision()
 *
 * Abstract:    Error if this GRPSYL conflicts with others on this staff.
 *
 * Returns:     void
 *
 * Description: This function checks for collisions between notes in this
 *		GRPSYL and notes in GRPSYLs of later voices in this chord on
 *		this staff.  On a tab staff, no two voices are allowed to use
 *		the same string at the same time.  If the frets are different,
 *		it would be impossible to play, and it seems best to disallow
 *		it even if they agreed.  So if this happens, do an l_ufatal.
 */

static void
chktabcollision(start_p)

struct GRPSYL *start_p;		/* first voice on this staff in this chord */

{
	int sv[MAXTABLINES];	/* which voice, if any, is using each string */
	int sidx;		/* string index, starting at 0 */
	struct GRPSYL *gs_p;	/* a GRPSYL on this staff in this chord */
	int n;			/* loop through notes (frets) in GRPSYL */


	/* if this chord has no more voices on this staff, return */
	if (start_p->gs_p == 0 ||
	    start_p->gs_p->grpsyl == GS_SYLLABLE ||
	    start_p->gs_p->staffno != start_p->staffno)
		return;

	/* we care only about notes (frets); rests and spaces are invisible */
	if (start_p->grpcont != GC_NOTES)
		return;

	/* init each string to an invalid voice number */
	for (sidx = 0; sidx < MAXTABLINES; sidx++) {
		sv[sidx] = 0;
	}

	/*
	 * Loop from this voice through the last voice on this staff that has
	 * a GRPSYL in this chord.  Don't worry about preceding voices; they
	 * already were in here and were checked against all these voices.
	 */
	for (gs_p = start_p; gs_p != 0 && start_p->gs_p->grpsyl == GS_GROUP &&
			gs_p->staffno == start_p->staffno; gs_p = gs_p->gs_p) {

		/* put each note into array, checking if string already used */
		for (n = 0; n < gs_p->nnotes; n++) {

			if (sv[(int)gs_p->notelist[n].STRINGNO] != 0) {

				l_ufatal(start_p->inputfile,
					 start_p->inputlineno,
					 "voices %d and %d on staff %d are using the \"%s\" string at the same time",
					 sv[(int)gs_p->notelist[n].STRINGNO],
					 gs_p->vno,
					 gs_p->staffno,
					 stringname(gs_p->notelist[n].STRINGNO,
						gs_p->staffno));
			}

			sv[(int)gs_p->notelist[n].STRINGNO] = gs_p->vno;
		}
	}
}

/*
 * Name:        intertab()
 *
 * Abstract:    Do additional work between tablature groups.
 *
 * Returns:     void
 *
 * Description: This function does checks to prevent certain bend sequences
 *		on a tab staff.  (It's unclear how such things would be drawn.)
 *		Also, when it finds the end of a single consecutive bend, it
 *		alters the previously set northern group boundaries of the
 *		groups, so that the arrows pointing at the bend strings will go
 *		up and down appropriately.
 *
 *		This function is called only with groups that have real bends
 *		(regular or prebends).
 */
#define	MAXBDIST	20	/* no. of unique bend distances in a sequence*/

static void
intertab(gs_p, mll_p)

struct GRPSYL *gs_p;		/* point at current tablature group */
struct MAINLL *mll_p;		/* point at the main LL struct it hangs off */

{
	RATIONAL bdist[MAXBDIST];	/* array of bend distances */
	RATIONAL bd;			/* a bend distance */
	int bdidx;			/* index into table of bend distances*/
	struct GRPSYL *nextgs_p;	/* point at the next GRPSYL */
	struct GRPSYL *gs2_p;		/* point at earlier GRPSYLs */
	struct MAINLL *mll2_p;		/* needed for crossing bar lines */
	int count, count2;		/* count numbers of bends */
	int n, k, j;			/* loop variables */
	int idx;			/* index into a notelist */
	int bad;			/* was a bad thing found? */


	/* count how many nonnull bends end at this group, remember last one */
	count = 0;
	idx = 0;		/* prevent useless 'used before set' warning */
	for (n = 0; n < gs_p->nnotes; n++) {
		if (HASREALBEND(gs_p->notelist[n])) {
			count++;
			idx = n;		/* remember where bend is */
		}
	}

	/* enforce restrictions on the following group, if there is one */
	mll2_p = mll_p;		/* we don't want to disturb mll_p */
	nextgs_p = nextgrpsyl(gs_p, &mll2_p);
	count2 = 0;	/* how many nonnull nonprebend bends are in *nextgs_p */

	if (nextgs_p != 0 && nextgs_p->grpcont == GC_NOTES) {

		bad = NO;	/* init to "nothing is bad" */

		for (n = 0; n < nextgs_p->nnotes; n++) {

			/* if this note has a nonnull nonprebend bend */
			if (HASREALBEND(nextgs_p->notelist[n]) &&
			    nextgs_p->notelist[n].FRETNO == NOFRET) {

				count2++;
				if (count > 1) {
					l_ufatal(gs_p->inputfile,
						gs_p->inputlineno,
						"no bend (other than a release) is allowed to follow a multiple string bend");
				}

				if (count2 > 1) {
					l_ufatal(gs_p->inputfile,
						gs_p->inputlineno,
						"only single string bends are allowed to be consecutive");
				}

				if (nextgs_p->notelist[n].STRINGNO !=
						gs_p->notelist[idx].STRINGNO) {
					bad = YES;
				}
			}
		}
		/*
		 * We check "bad" here instead of inside the above loop,
		 * because we want to give priority to the "only single string
		 * bends . . ." message if that condition is happening.
		 */
		if (bad == YES) {
			l_ufatal(gs_p->inputfile, gs_p->inputlineno,
				"consecutive bends must be on the same string");
		}
	}

	/*
	 * We know the current group has bend(s).  If the following group has
	 * a nonnull nonprebend bend, just return now.  We will handle this
	 * bend sequence when we find the last nonnull bend in it.
	 */
	if (count2 > 0)
		return;

	/*
	 * Loop backwards through the sequence of bends.  The start should be
	 * either a bend following no nonnull bend, or a prebend.  While
	 * searching, build a table of all the unique bend distances.  Usually
	 * we break out by finding a group with a nonnull bend, which means we
	 * went one too far with gs2_p, and gs_p is the start of the sequence.
	 * But if we hit the start, gs2_p will become 0 and we get out of the
	 * loop naturally.  Again, gs_p is the start of the sequence.
	 */
	bdidx = 0;	/* number of unique bend distances found */
	gs2_p = gs_p;
	while (gs2_p != 0) {
		/* find which note, if any, has the bend in this group */
		for (n = 0; n < gs2_p->nnotes; n++) {
			if (HASREALBEND(gs2_p->notelist[n])) {
				bd = ratbend(&gs2_p->notelist[n]);
				break;
			}
		}

		if (n < gs2_p->nnotes) {
			/*
			 * We found a nonnull bend.  Search the bdist array to
			 * see if this value has already occurred.  Get out
			 * when the value is found, or when we find a greater
			 * value (the list is in ascending order).
			 */
			for (k = 0; k < bdidx; k++) {
				if (GE(bdist[k], bd))
					break;
			}
			if (k == bdidx) {
				/* bd > everything in the array */
				/* add it at the end */
				bdist[k] = bd;
				bdidx++;
			} else if (GT(bdist[k], bd)) {
				/* bd should be put at this position */
				/* move all later ones down a notch */
				for (j = bdidx - 1; j >= k; j--)
					bdist[j+1] = bdist[j];
				bdist[k] = bd;
				bdidx++;
			}
			/* else bd is already in the table */

			if (bdidx >= MAXBDIST)
				pfatal("too many unique bend distances in sequence of bends");
			/* if this bend was a prebend, break */
			if (gs2_p->notelist[n].FRETNO != NOFRET) {
				gs_p = gs2_p;	/* series starts at prebend */
				break;
			}
		} else {
			/* there was no bend; start at the following group; */
			/* gs_p is now the beginning of the sequence */
			break;
		}

		/*
		 * It was a nonprebend bend.  Point gs2_p to the preceding
		 * group, remember the one we just looked at in gs_p, and keep
		 * looping.
		 */
		gs_p = gs2_p;
		gs2_p = prevgrpsyl(gs2_p, &mll_p);
	}

	/*
	 * Loop forward through these groups.  For each one, alter its northern
	 * boundary according to where its bend distance occurs in the bdist
	 * table.  This will cause the print phase to print the bend strings
	 * at varying heights so that the arrows will bend up and down as
	 * appropriate.
	 */
	while (gs_p != nextgs_p && gs_p != 0) {
		/* find which note has the bend in this group, get distance */
		for (n = 0; n < gs_p->nnotes; n++) {
			if (HASREALBEND(gs_p->notelist[n])) {
				bd = ratbend(&gs_p->notelist[n]);
				break;
			}
		}
		/* find distance in table, raise RN proportionally to index */
		for (n = 0; n < bdidx; n++) {
			if (EQ(bdist[n], bd)) {
				gs_p->c[RN] += 3.0 * STEPSIZE * TABRATIO * n;
				break;
			}
		}

		gs_p = nextgrpsyl(gs_p, &mll_p);
	}
}

/*
 * Name:        setstems()
 *
 * Abstract:    Sets stem direction for each group.
 *
 * Returns:     void
 *
 * Description: This function sets the stem direction for each group, based
 *		on the voice scheme at the time and other factors.
 */

static void
setstems()

{
	/* remember the previous stem direction of voice 3 on each staff */
	short v3stemdir[MAXSTAFFS + 1];

	int staffno;			/* staff number */
	int n;				/* loop variable */
	register struct MAINLL *mainll_p; /* point at main linked list item */
	int vscheme;			/* current voice scheme */


	debug(16, "setstems");
	initstructs();			/* clean out old SSV info */

	/* set initial default direction of voice 3 stems to be UP */
	for (n = 1; n <= MAXSTAFFS; n++)
		v3stemdir[n] = UP;

	/*
	 * Loop once for each item in the main linked list.  Apply any SSVs
	 * that are found.  Call subroutines to process linked lists of
	 * groups.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		if (mainll_p->str == S_SSV) {

			asgnssv(mainll_p->u.ssv_p);

		} else if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->visible == YES &&
				! is_mrpt(mainll_p->u.staff_p->groups_p[0])) {
			/*
			 * We've found a visible staff, which will have one
			 * or more voices, depending on the voice scheme.
			 */
			staffno = mainll_p->u.staff_p->staffno;
			vscheme = svpath(staffno, VSCHEME)->vscheme;

			switch (vscheme) {
			case V_1:
				/*
				 * There's only one voice on this staff, so
				 * call a routine to decide which way to point
				 * each stem.  It handles both grace & nongrace.
				 */
				setonestem(mainll_p->u.staff_p->groups_p[0]);
				break;

			case V_2OPSTEM:
				/*
				 * There are two voices on this staff, and
				 * the stems are always supposed to point
				 * opposite.  Call a routine to mark their
				 * stem directions.  It handles both nongrace
				 * and grace.
				 */
				setopstem(mainll_p->u.staff_p->groups_p[0],
					  mainll_p->u.staff_p->groups_p[1]);
				break;

			case V_2FREESTEM:
				/*
				 * There are two voices on this staff, and
				 * the stems are free to point either way
				 * when one voice is a space.  Call routines
				 * to mark their stem directions; first
				 * nongrace, then grace.
				 */
				setfreestem(mainll_p->u.staff_p->groups_p[0],
					    mainll_p->u.staff_p->groups_p[1]);
				dograce(mainll_p->u.staff_p->groups_p[0],
					mainll_p->u.staff_p->groups_p[1]);

				break;

			case V_3OPSTEM:
				/*
				 * This is just like V_2OPSTEM for the first
				 * two voices, but also allows a voice 3.
				 */
				setopstem(mainll_p->u.staff_p->groups_p[0],
					  mainll_p->u.staff_p->groups_p[1]);
				v3stemdir[staffno] = setv3stem(
					mainll_p->u.staff_p->groups_p[2],
					v3stemdir[staffno]);
				break;

			case V_3FREESTEM:
				/*
				 * This is just like V_2FREESTEM for the first
				 * two voices, but also allows a voice 3.
				 */
				setfreestem(mainll_p->u.staff_p->groups_p[0],
					    mainll_p->u.staff_p->groups_p[1]);
				dograce(mainll_p->u.staff_p->groups_p[0],
					mainll_p->u.staff_p->groups_p[1]);
				v3stemdir[staffno] = setv3stem(
					mainll_p->u.staff_p->groups_p[2],
					v3stemdir[staffno]);

				break;
			}
		}
	}
}

/*
 * Name:        setonestem()
 *
 * Abstract:    Sets stem direction for each group in a linked list for V_1.
 *
 * Returns:     void
 *
 * Description: This function sets the stem direction for each group in a
 *		linked list for a voice/measure, for the case where there is
 *		only one voice on the staff.
 */

static void
setonestem(gs_p)

struct GRPSYL *gs_p;	/* starts pointing at the first GRPSYL in a list */

{
	register struct GRPSYL *start_p, *end_p; /* first and last of a set */


	debug(32, "setonestem file=%s line=%d", gs_p->inputfile,
			gs_p->inputlineno);
	/*
	 * Loop once for each bunch of groups that must be stemmed the same
	 * way.  A beamed group must all be stemmed the same way, but nonbeamed
	 * notes are independent.
	 */
	start_p = gs_p;
	for (;;) {
		/*
		 * Find the next nongrace group.  While doing this, set the
		 * stemdir for any grace groups encountered.
		 */
		while (start_p != 0 && start_p->grpvalue == GV_ZERO) {
			favorstemdir(start_p, UP);
			start_p = start_p->next;
		}
		if (start_p == 0)	/* get out if no more this measure */
			break;

		/* if this group is not beamed, handle it, and point at next */
		if (start_p->beamloc == NOITEM) {
			dobunch(start_p, start_p->next);
			start_p = start_p->next;
			continue;
		}

		/*
		 * Find end of this beamed group, setting grace groups UP.  If
		 * this is a cross staff beamed group, we may be starting at an
		 * INITEM or even the ENDITEM, since on this staff STARTITEM
		 * may have been a space.  But that doesn't matter; we still
		 * look for ENDITEM, whether or not it's also a space; and
		 * dobunch handles these cases.
		 */
		for (end_p = start_p; end_p != 0 &&
		(end_p->grpvalue == GV_ZERO || end_p->beamloc != ENDITEM);
		end_p = end_p->next) {
			if (end_p->grpvalue == GV_ZERO)
				favorstemdir(end_p, UP);
		}
		if (end_p == 0)
			pfatal("beamed group is not terminated");

		/* handle this bunch of groups, and point at next */
		dobunch(start_p, end_p->next);
		start_p = end_p->next;
	}

	/* go through list again, setting groups affected by midlinestemfloat */
	setmidlinestem(gs_p);
}

/*
 * Name:        setopstem()
 *
 * Abstract:    Sets stemdir for v1 or v2 groups for V_2OPSTEM/V_3OPSTEM.
 *
 * Returns:     void
 *
 * Description: This function sets the stem direction for each group in
 *		2 linked lists for a staff/measure, for the case where
 *		the linked list is for voice 1 or voice 2 and stems are always
 *		supposed to be opposed.  This function does both grace and
 *		nongrace groups.  For this vscheme, they act the same.
 *		The user can force the stems against the normal direction.
 */

static void
setopstem(gs1_p, gs2_p)

register struct GRPSYL *gs1_p;	/* starts at first GRPSYL in voice 1 list */
register struct GRPSYL *gs2_p;	/* starts at first GRPSYL in voice 2 list */

{
	debug(32, "setopstem file=%s line=%d", gs1_p->inputfile,
			gs1_p->inputlineno);
	/* mark first voice's stems up unless the user forced them down */
	for ( ; gs1_p != 0; gs1_p = gs1_p->next) {
		favorstemdir(gs1_p, UP);
	}

	/* mark second voice's stems down unless the user forced them up */
	for ( ; gs2_p != 0; gs2_p = gs2_p->next) {
		favorstemdir(gs2_p, DOWN);
	}
}

/*
 * Name:        setfreestem()
 *
 * Abstract:    Sets stemdir for each group in 2 linked lists for V_2FREESTEM.
 *
 * Returns:     void
 *
 * Description: This function sets the stem direction for each (nongrace)
 *		group in 2 linked lists for a staff/measure, for the case
 *		where there are two voices on the staff and the stems are
 *		allowed to point either way for one voice when the other
 *		voice has a space.
 */

static void
setfreestem(gs1_p, gs2_p)

struct GRPSYL *gs1_p;	/* starts pointing at first GRPSYL in voice 1 list */
struct GRPSYL *gs2_p;	/* starts pointing at first GRPSYL in voice 2 list */

{
	debug(32, "setfreestem file=%s line=%d", gs1_p->inputfile,
			gs1_p->inputlineno);
	/* call to handle first voice, then call to handle second voice */
	set1freestem(gs1_p, gs2_p, UP);
	set1freestem(gs2_p, gs1_p, DOWN);
}

/*
 * Name:        set1freestem()
 *
 * Abstract:    Sets stemdir for v1 or v2 groups for V_2FREESTEM/V_3FREESTEM.
 *
 * Returns:     void
 *
 * Description: This function sets the stem direction for each (nongrace)
 *		group in one linked list for a staff/measure, for the case
 *		where the linked list is for voice 1 or voice 2 and stems are
 *		allowed to point either way for one voice when the other
 *		voice has a space.  The function sets the directions just
 *		for "this" voice; the other voice is only used as a reference
 *		(we need to check when it has spaces).
 */

static void
set1freestem(this_p, other_p, stemdir)

struct GRPSYL *this_p;	/* starts pointing at first GRPSYL in linked list */
			/* for the voice whose stems we are now setting */
struct GRPSYL *other_p;	/* starts pointing at first GRPSYL in linked list */
			/* for the other voice */
int stemdir;		/* which way the stem must point if forced */

{
	register struct GRPSYL *start_p, *end_p; /* first and last of a set */
	RATIONAL vtime, vtime2;		/* elapsed time this measure */


	debug(32, "set1freestem file=%s line=%d stemdir=%d", this_p->inputfile,
			this_p->inputlineno, stemdir);
	vtime = Zero;			/* init to no time elapsed */

	/*
	 * Loop once for each bunch of groups in this voice that must be
	 * stemmed the same way.  A beamed group must all be stemmed the same
	 * way, but nonbeamed notes are independent.
	 */
	start_p = this_p;
	for (;;) {
		/* find next nongrace group */
		while (start_p != 0 && start_p->grpvalue == GV_ZERO) {
			start_p = start_p->next;
		}
		if (start_p == 0)	/* get out if no more this measure */
			break;

		/* if this group is not beamed, handle it, and point at next */
		if (start_p->beamloc == NOITEM) {
			vtime2 = radd(vtime, start_p->fulltime);

			if (hasspace(other_p, vtime, vtime2)) {
				/* other voice has space; decide stem */
				dobunch(start_p, start_p->next);
			} else {
				/*
				 * The other voice has notes/rests; force the
				 * the direction, unless the user has already
				 * forced it.
				 */
				if (start_p->stemdir == UNKNOWN) {
					start_p->stemdir = (short)stemdir;
				}
			}

			start_p = start_p->next;
			vtime = vtime2;
			continue;
		}

		/* find end of this beamed group, ignoring grace groups */
		vtime2 = vtime;
		for (end_p = start_p; end_p != 0 &&
		(end_p->grpvalue == GV_ZERO || end_p->beamloc != ENDITEM);
		end_p = end_p->next)
			vtime2 = radd(vtime2, end_p->fulltime);
		if (end_p == 0)
			pfatal("beamed group is not terminated");
		vtime2 = radd(vtime2, end_p->fulltime);	/* add in final note */

		/* handle this bunch of groups, and point at next */
		if (hasspace(other_p, vtime, vtime2)) {
			/* other voice has space; decide our stems */
			dobunch(start_p, end_p->next);
		} else {
			/* other voice has notes/rests; this forces ours */
			favorstemdir(start_p, stemdir);
		}

		vtime = vtime2;
		start_p = end_p->next;
	}

	/* go through list again, setting groups affected by midlinestemfloat */
	setmidlinestem(this_p);
}

/*
 * Name:        favorstemdir()
 *
 * Abstract:    Sets stem direction, one note or beam, favoring one direction.
 *
 * Returns:     void
 *
 * Description: If the given group is nonbeamed, this function sets its stem
 *		direction to the one given, if it is not already set.  If it is
 *		the first group of a beamed set, it sets all the set's group's
 *		stemdirs to the one given, if none are set, or if some are set,
 *		it uses that direction.  Otherwise is does nothing.  The
 *		functions works on nongrace and grace.
 */

static void
favorstemdir(start_p, stemdir)

struct GRPSYL *start_p;	/* point at the first GRPSYL in beamed set */
int stemdir;		/* which way we will try to point the stems */

{
	struct GRPSYL *g_p;		/* point into the set */
	int forcedir;			/* direction forced by user */
	int grpvalue;			/* the one that this set is */


	switch (start_p->beamloc) {
	case NOITEM:
		/* nonbeamed: set stemdir if not already set */
		if (start_p->stemdir == UNKNOWN) {
			start_p->stemdir = stemdir;
		}
		return;

	case STARTITEM:
		break;		/* go handle the whole set */

	default:
		return;		/* do nothing for later members of a set */
	}

	grpvalue = start_p->grpvalue;
	forcedir = UNKNOWN;		/* no forcing yet */

	/* look for groups in this set where the user has forced stemdir */
	for (g_p = start_p; g_p != 0; g_p = g_p->next) {
		/* consider only nonspace groups of the same grpvalue */
		if (g_p->grpcont == GC_SPACE || g_p->grpvalue != grpvalue) {
			/* if enditem of this set, break out */
			/* this can happen when a space ends a CSB */
			if (g_p->grpvalue == grpvalue &&
						g_p->beamloc == ENDITEM) {
				break;
			}
			continue;
		}
		/* if user forced the stemdir */
		if (g_p->stemdir != UNKNOWN) {
			if (forcedir == UNKNOWN) {
				/* first such occurrence; remember it */
				forcedir = g_p->stemdir;
			} else if (g_p->stemdir != forcedir) {
				/* any later occurrence must agree */
				l_warning(g_p->inputfile, g_p->inputlineno,
				"cannot have both 'up' and 'down' stem in same set of beamed or 'alt'-ed note groups");
				forcedir = g_p->stemdir; /* use latest */
			}
		}
		if (g_p->beamloc == ENDITEM) {
			break;
		}
	}

	/* if user forced any stems, we'll go with that direction */
	if (forcedir != UNKNOWN) {
		stemdir = forcedir;
	}

	/* set all the stems in this set */
	for (g_p = start_p; g_p != 0; g_p = g_p->next) {
		if (g_p->grpcont == GC_SPACE || g_p->grpvalue != grpvalue) {
			if (g_p->grpvalue == grpvalue &&
						g_p->beamloc == ENDITEM) {
				break;
			}
			continue;
		}
		g_p->stemdir = stemdir;
		if (g_p->beamloc == ENDITEM) {
			break;
		}
	}
}

/*
 * Name:        dobunch()
 *
 * Abstract:    Sets stem direction for a single group or a beamed set.
 *
 * Returns:     void
 *
 * Description: This function is given a single (nongrace) group, or a set
 *		of them that will be beamed together, for the case where
 *		the stems are allowed to go either way.  It decides which
 *		way is best, and sets the result.
 *		Exception:  If parameter midlinestemfloat == YES, it will leave
 *		the direction UNKNOWN in cases where it has to look at the
 *		neighboring group(s)' settings, done later in setmidlinestem().
 */

static void
dobunch(start_p, end_p)

struct GRPSYL *start_p;	/* starts pointing at the first GRPSYL in a bunch */
struct GRPSYL *end_p;	/* starts pointing after the last GRPSYL in a bunch */

{
	register struct GRPSYL *gs_p;	/* point along list of them */
	int lonesum;		/* sum of offsets of single notes from center*/
	int topsum;		/* sum of offsets of top notes from center */
	int botsum;		/* sum of offsets of bottom notes from center*/
	int insum;		/* sum of offsets of inner notes from center */
	int n;			/* loop counter */
	int stemdir;		/* answer of where stems should point */
	int div_pt;		/* dividing point between prefering up/down */
	int midlinestemfloat;	/* look at neighbors for notes on middle line?*/


	midlinestemfloat = vvpath(start_p->staffno, start_p->vno,
			MIDLINESTEMFLOAT)->midlinestemfloat;

	/*
	 * Loop through all (nongrace) notes in these group(s), adding up
	 * the offsets of their outer notes from the center line.  For groups
	 * that have only one note, count this in lonesum.  For other groups,
	 * count the top notes and bottom notes separately.  We consider only
	 * outer notes in these counters, and we count single note groups
	 * separately to avoid counting the same note twice.  But to be able to
	 * breaks ties in the best way, we keep a separate counter for inner
	 * notes of groups that have 3 or more notes.
	 * While doing this, also keep track of whether the user requested a
	 * specific stem direction on any of these groups.  If so, there must
	 * not be any contradictions between what they asked for.
	 * Note: for quad and oct notes, we favor stem-down, so instead of the
	 * center line being the dividing place, the bottom line is.
	 */
	/* if first group is quad/oct, it will be the only group (no beams) */
	div_pt = start_p->basictime <= BT_QUAD ? -4 : 0;
	lonesum = topsum = botsum = insum = 0;
	stemdir = UNKNOWN;	/* user hasn't asked for anything yet */
	for (gs_p = start_p; gs_p != end_p; gs_p = gs_p->next) {
		if (gs_p->grpcont != GC_SPACE && gs_p->grpvalue == GV_NORMAL) {
			if (gs_p->stemdir != UNKNOWN) {
				if (stemdir == UNKNOWN) {
					stemdir = gs_p->stemdir;
				} else if (gs_p->stemdir != stemdir) {
					l_warning(gs_p->inputfile,
					gs_p->inputlineno,
					"cannot have both 'up' and 'down' stem in same set of beamed or 'alt'-ed note groups");
					stemdir = gs_p->stemdir;
				}
			}

			if (gs_p->nnotes == 1) {
				lonesum += gs_p->notelist[0].stepsup - div_pt;
			} else if (gs_p->nnotes > 1) {
				topsum += gs_p->notelist[0].stepsup - div_pt;
				botsum += gs_p->notelist[ gs_p->nnotes - 1 ].
						stepsup - div_pt;
			}

			/* this loop happens only if >= 3 notes in the group */
			for (n = 1; n < gs_p->nnotes - 1; n++ ) {
				insum += gs_p->notelist[n].stepsup - div_pt;
			}
		}
	}

	/*
	 * If the user requested a stem direction, that's what they will get,
	 * for 5-line regular staffs, but for 1-line regular staffs stems are
	 * always UP and for tablature staffs, always DOWN.  For tab staffs, the
	 * parse phase blocks any user requests for stemdir, so we don't have
	 * to cover that in the warning and error messages here.
	 *
	 * For a regular 5-line staff where the user didn't specify, if we are
	 * involved in cross staff beaming, the direction defaults such that
	 * the beam ends up between the two staffs; else, these rules apply:
	 * If lonesum + topsum + botsum is positive, the "average" outer note
	 * in these group(s) is above the center line, so the stems should go
	 * down.  If negative, they should go up.  In case of tie, they should
	 * go down, unless we can break the tie by using the inner notes.
	 * For 1-line staff, the stem should go up, regardless.
	 */
	if (svpath(start_p->staffno, STAFFLINES)->stafflines == 5 &&
			is_tab_staff(start_p->staffno) == NO) {
		if (stemdir == UNKNOWN) {
			switch (start_p->beamto) {
			case CS_ABOVE:		/* bm with staff above */
				stemdir = UP;
				break;
			case CS_BELOW:		/* bm with staff below */
				stemdir = DOWN;
				break;
			case CS_SAME:		/* no cross staff beaming */
				/* normal case: base on note distances */
				if (lonesum + topsum + botsum > 0)
					stemdir = DOWN;
				else if (lonesum + topsum + botsum < 0)
					stemdir = UP;
				else if (insum > 0)
					stemdir = DOWN;
				else if (insum < 0)
					stemdir = UP;
				/* it's tied (0); beamed and non-notes go DOWN*/
				else if (start_p->beamloc == STARTITEM ||
						 start_p->grpcont != GC_NOTES)
					stemdir = DOWN;
				/* if parm is NO, default to DOWN */
				else if (midlinestemfloat == NO)
					stemdir = DOWN;
				else
					return;	/* bail out, set in later */
				break;
			}
		}
	} else if (is_tab_staff(start_p->staffno) == YES) {
		stemdir = DOWN;
	} else {
		if (stemdir == DOWN)
			l_ufatal(start_p->inputfile, start_p->inputlineno,
			"cannot specify 'down' stem on voice 1 or 2 of a one-line staff");
		if (stemdir == UP)
			l_warning(start_p->inputfile, start_p->inputlineno,
			"stem direction should not be specified on voice 1 or 2 of a one-line staff");
		stemdir = UP;	/* in case it was UNKNOWN */
	}

	/* mark all groups except spaces, somehow that causes a CSB problem */
	for (gs_p = start_p; gs_p != end_p; gs_p = gs_p->next) {
		if (gs_p->grpcont != GC_SPACE && gs_p->grpvalue == GV_NORMAL) {
			gs_p->stemdir = (short)stemdir;
		}
	}
}

/*
 * Name:        setmidlinestem()
 *
 * Abstract:    Sets stem direction for groups affected by midlinestemfloat.
 *
 * Returns:     void
 *
 * Description:	This function sets stem directions for groups that would
 *		normally have been handled by dobunch(), but could not be,
 *		because they are centered and the midlinestemfloat parm makes
 *		it necessary to look at the group's neighbor(s)' settings.
 *		We scan one measure of one voice, setting the stem directions
 *		that have not yet been set.
 */

static void
setmidlinestem(start_p)

struct GRPSYL *start_p;	/* starts pointing at the first GRPSYL in a bunch */

{
	struct GRPSYL *gs_p;
	struct GRPSYL *nextgs_p;
	int prev_dir, next_dir;
	int prev_exists, next_exists;
	int result;


	/*
	 * As we loop left to right, we have to remember our left neighbor's
	 * original stemdir, since its value may be changed.  But our right
	 * neighbor is no problem, since we haven't changed it yet.
	 */
	prev_exists = NO;
	prev_dir = UNKNOWN;

	/* skip any leading graces; all graces are ignored in this function */
	gs_p = start_p;
	if (gs_p->grpvalue == GV_ZERO) {
		gs_p = nextnongrace(gs_p);
	}

	for ( ; gs_p != 0; gs_p = nextnongrace(gs_p)) {

		if (gs_p->grpcont != GC_NOTES) {
			/* rest/space means there is no neighboring note */
			prev_exists = NO;
			prev_dir = UNKNOWN;
			continue;
		}
		if (gs_p->stemdir != UNKNOWN) {
			/* already known; nothing to do but remember it */
			prev_exists = YES;
			prev_dir = gs_p->stemdir;
			continue;
		}

		/*
		 * Okay, we found one that needs to be set.  First get info on
		 * its right neighbor, if any.
		 */
		nextgs_p = nextnongrace(gs_p);
		if (nextgs_p != 0 && nextgs_p->grpcont == GC_NOTES) {
			next_exists = YES;
			next_dir = nextgs_p->stemdir;
		} else {
			next_exists = NO;
			next_dir = UNKNOWN;
		}

		/* find result, but don't overwrite stemdir yet */
		result = DOWN;	/* the default if neighbors are inconclusive */
		if (prev_exists == YES && next_exists == YES) {
			/* both neighbors exist; follow if known and agree */
			if (prev_dir != UNKNOWN && next_dir != UNKNOWN) {
				if (prev_dir == next_dir) {
					result = prev_dir;
				}
			}
		} else if (prev_exists == YES && prev_dir != UNKNOWN) {
			/* only prev exists; it is known, so use it */
			result = prev_dir;
		} else if (next_exists == YES && next_dir != UNKNOWN) {
			/* only next exists; it is known, so use it */
			result = next_dir;
		}

		/* remember the original state for the next loop */
		prev_exists = YES;
		prev_dir = gs_p->stemdir;

		/* update this group to what we decided above */
		gs_p->stemdir = result;
	}
}

/*
 * Name:        dograce()
 *
 * Abstract:    Sets stem direction for a single grace group.
 *
 * Returns:     void
 *
 * Description:	This function sets stem directions for grace groups when the
 *		vscheme V_2FREESTEM.  (The V_1 and V_2OPSTEM cases were handled
 *		along with nongrace groups.)  If the next nongrace group occurs
 *		at a time when the other voice has a space, the grace stem goes
 *		up (like V_1), else the same as the main group (like V_2OPSTEM).
 *		For the first voice, these rules boil down to the fact that
 *		graces stems are always up.  The second voice can end up
 *		going either way.
 */

static void
dograce(gs1_p, gs2_p)

register struct GRPSYL *gs1_p;	/* starts at first GRPSYL in voice 1 list */
register struct GRPSYL *gs2_p;	/* starts at first GRPSYL in voice 2 list */

{
	register struct GRPSYL *gs_p;	/* point along list of them */
	RATIONAL vtime;			/* elapsed time in measure */
	static RATIONAL tiny = {1, 4 * MAXBASICTIME};


	/* for the first voice, mark all grace stems up */
	for (gs_p = gs1_p; gs_p != 0; gs_p = gs_p->next) {
		if (gs_p->grpvalue == GV_ZERO)
			favorstemdir(gs_p, UP);
	}

	/*
	 * For the 2nd voice, loop though all groups.  For each nongrace group,
	 * accumulate the fulltime.  For each grace group, find out if the
	 * other voice has a space at the moment the following nongrace group
	 * starts.  If so, treat as V_1.  If not, treat as V_2OPSTEM.
	 */
	vtime = Zero;
	for (gs_p = gs2_p; gs_p != 0; gs_p = gs_p->next) {
		if (gs_p->grpvalue == GV_NORMAL) {
			vtime = radd(vtime, gs_p->fulltime);
		} else {
			/* does other voice have space? */
			if (hasspace(gs1_p, vtime, radd(vtime, tiny)) == YES) {
				favorstemdir(gs_p, UP);
			} else {
				favorstemdir(gs_p, DOWN);
			}
		}
	}
}

/*
 * Name:        setv3stem()
 *
 * Abstract:    Sets stem direction for each group in a linked list for voice 3.
 *
 * Returns:     default stem direction after this measure
 *
 * Description: This function sets the stem direction for each group in a
 *		linked list for a voice/measure that is for voice 3.  Voice 3
 *		ignores the other voices.
 */

static int
setv3stem(gs_p, stemdir)

struct GRPSYL *gs_p;	/* starts pointing at the first GRPSYL in a list */
int stemdir;		/* stem direction of the previous group */

{
	register struct GRPSYL *start_p, *end_p; /* first and last of a set */


	debug(32, "setv3stem file=%s line=%d", gs_p->inputfile,
			gs_p->inputlineno);
	/*
	 * Loop once for each bunch of groups that must be stemmed the same
	 * way.  A beamed group must all be stemmed the same way, but nonbeamed
	 * notes are independent.
	 */
	start_p = gs_p;
	for (;;) {
		/*
		 * Find the next nongrace group.  While doing this set the
		 * stemdir for any grace groups encountered.  For voice 3,
		 * grace stems go up unless user forced them down.
		 */
		while (start_p != 0 && start_p->grpvalue == GV_ZERO) {
			favorstemdir(start_p, UP);
			start_p = start_p->next;
		}
		if (start_p == 0)	/* get out if no more this measure */
			break;

		/* if this group is not beamed, handle it, and point at next */
		if (start_p->beamloc == NOITEM) {
			stemdir = dov3bunch(start_p, start_p->next, stemdir);
			start_p = start_p->next;
			continue;
		}

		/*
		 * Find end of this beamed group, setting grace groups.
		 * Note that voice 3 does not allow cross staff beaming.
		 */
		for (end_p = start_p; end_p != 0 &&
		(end_p->grpvalue == GV_ZERO || end_p->beamloc != ENDITEM);
		end_p = end_p->next) {
			if (end_p->grpvalue == GV_ZERO)
				favorstemdir(end_p, UP);
		}
		if (end_p == 0)
			pfatal("beamed group is not terminated");

		/* handle this bunch of groups, and point at next */
		stemdir = dov3bunch(start_p, end_p->next, stemdir);
		start_p = end_p->next;
	}

	return (stemdir);
}

/*
 * Name:        dov3bunch()
 *
 * Abstract:    Sets stem dir for a single group or a beamed set on voice 3.
 *
 * Returns:     stem direction that was chosen
 *
 * Description: This function is given a single (nongrace) group, or a set
 *		of them that will be beamed together, for voice 3.  It decides
 *		which stemdir is needed, and sets it for each group.
 */

static int
dov3bunch(start_p, end_p, stemdir)

struct GRPSYL *start_p;	/* starts pointing at the first GRPSYL in a bunch */
struct GRPSYL *end_p;	/* starts pointing after the last GRPSYL in a bunch */
int stemdir;		/* stem direction of the previous group */

{
	register struct GRPSYL *gs_p;	/* point along list of them */
	int userdir;			/* stemdir requested by user */


	/*
	 * Loop through all groups in this bunch, keeping track of any user-
	 * specified direction.  Grace groups are handled when encountered but
	 * don't affect the other groups.
	 */
	userdir = UNKNOWN;	/* user hasn't asked for anything yet */
	for (gs_p = start_p; gs_p != end_p; gs_p = gs_p->next) {
		if (gs_p->grpvalue == GV_ZERO) {
			favorstemdir(gs_p, UP);	/* grace group */
		} else if (gs_p->grpcont != GC_SPACE) {
			if (gs_p->stemdir != UNKNOWN) {	/* user request */
				if (userdir == UNKNOWN) {
					userdir = gs_p->stemdir;
				} else if (gs_p->stemdir != userdir) {
					l_warning(gs_p->inputfile,
					gs_p->inputlineno,
					"cannot have both 'up' and 'down' stem in same set of beamed or 'alt'-ed note groups");
					userdir = gs_p->stemdir;
				}
			}
		}
	}

	/* if user requested a direction, we will use that, else keep previous*/
	if (userdir != UNKNOWN)
		stemdir = userdir;

	/* mark all nongrace groups and embedded rests */
	for (gs_p = start_p; gs_p != end_p; gs_p = gs_p->next) {
		if (gs_p->grpvalue == GV_NORMAL)
			gs_p->stemdir = (short)stemdir;
	}

	return (stemdir);
}

/*
 * Name:        setheads()
 *
 * Abstract:    Set headshape, headfont, headchar, and coords for all notes.
 *
 * Returns:     void
 *
 * Description: This function sets the headshape, headfont, and headchar for
 *		all notes.  (However, the headchar is changed later, in
 *		setgrps.c, in certain cases where two GRPSYLs share a note.)
 *		It also sets the relative vertical coords of the notes and
 *		their groups.  We waited until now to do this so that stemdir
 *		would be known.
 */

static void
setheads()

{
	struct MAINLL *mainll_p;	/* point at main linked list item */
	struct STAFF *staff_p;		/* point at a STAFF */
	int stafflines;			/* lines in a tablature staff */
	int is_tab;			/* is this a tablature staff? */
	int sharps;			/* in the key sig */
	char keylet;			/* letter of the key, assuming major */
	short *shapes;			/* 7 shapes for the 7 notes */
	int vidx;			/* voice index */
	int allx_hsi;			/* headshape index for allx */


	debug(16, "setheads");
	initstructs();			/* clean out old SSV info */

	/* just in case we'll need it later */
	allx_hsi = get_shape_num("allx");

	/*
	 * Loop once for each item in the main linked list.  Apply any SSVs
	 * that are found.  For each voice on each staff, call setvoiceheads
	 * to do the the work.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			/* apply the SSV and go to the next item */
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		/* deal only with visible staffs */
		if (mainll_p->str != S_STAFF ||
				mainll_p->u.staff_p->visible == NO) {
			continue;
		}

		/*
		 * We found a staff to work on.  Set up some variables we'll
		 * be needing.
		 */
		staff_p = mainll_p->u.staff_p;
		stafflines = svpath(staff_p->staffno, STAFFLINES)->stafflines;
		is_tab = svpath(staff_p->staffno, CLEF)->clef == TABCLEF;

		/*
		 * Find the key letter.  We don't care about any sharp or flat
		 * in the key name, just the letter.  For tab it's meaningless,
		 * but that's okay.
		 */
		sharps = eff_key(staff_p->staffno);
		keylet = Circle[(sharps + 1 + 7) % 7];

		/* loop through every possible voice on this staff */
		for (vidx = 0; vidx < MAXVOICES; vidx++) {

			/* point at array of headshapes for this voice */
			shapes = vvpath(staff_p->staffno, vidx + 1,
					NOTEHEADS)->noteheads;

			setvoiceheads(mainll_p, staff_p->groups_p[vidx],
				stafflines, shapes, is_tab, allx_hsi, sharps,
				keylet);
		}
	}
}

/*
 * Name:        setvoiceheads()
 *
 * Abstract:    Set headshape, headfont, headchar, and coords for one GRPSYL.
 *
 * Returns:     void
 *
 * Description: This function sets the headshape, headfont, and headchar for
 *		one GRPSYL.  (However, the headchar is changed later, in
 *		setgrps.c, in certain cases where two GRPSYLs share a note.)
 *		It also sets the relative vertical coords of the notes and
 *		the group.
 */

static void
setvoiceheads(mll_p, gs_p, stafflines, shapes, is_tab, allx_hsi, sharps, keylet)

struct MAINLL *mll_p;		/* point at the main LL struct gs_p hangs off */
struct GRPSYL *gs_p;		/* starts at start of GRPSYL list */
int stafflines;			/* lines in a tablature staff */
short *shapes;			/* 7 shapes for the 7 notes */
int is_tab;			/* is this a tablature staff? */
int allx_hsi;			/* headshape index for allx */
int sharps;			/* in the key sig */
int keylet;			/* letter of the key, assuming major */

{
	float bendheight;		/* total height of bend numbers */
	float high;			/* height of something */
	int havebend;			/* any bends in this group? */
	int n;				/* loop variable */
	int i;				/* temp variable */
	int hfont;			/* font of note head */
	int hchar;			/* char of note head */
	float vhalf;			/* half the vert size of note head */
	int stepsup;			/* local copy */


	/* loop through every GRPSYL in voice (may be none) */
	for ( ; gs_p != 0; gs_p = gs_p->next) {

		/* we only care about notes, not rest/space */
		if (gs_p->grpcont != GC_NOTES) {
			continue;
		}

		bendheight = 0;		/* init to no bends */
		havebend = NO;

		/*
		 * Loop through every note in the GRPSYL, setting its
		 * headshape, head font/char, and coords.
		 */
		for (n = 0; n < gs_p->nnotes; n++) {

			/* if there is no note-level override... */
			if (gs_p->notelist[n].headshape == HS_UNKNOWN) {

				/* set to group-level override if present */
				gs_p->notelist[n].headshape = gs_p->headshape;

				/*
				 * If still no setting (which is the usual
				 * case), set according to what the SSVs said.
				 * Set i to how far note is above the tonic
				 * (assuming a major key).  Tab uses tonic.
				 * Then get shape from array.
				 */
				if (gs_p->notelist[n].headshape == HS_UNKNOWN) {

					if (is_tab) {
						i = 0;	/* arbitrary */
					} else {
						i = (gs_p-> notelist[n].letter
							+ 7 - keylet) % 7;
					}

					gs_p->notelist[n].headshape = shapes[i];
				}
			}

			/*
			 * Now that we know the stepsup (set in locllnotes())
			 * and the headshape, we can set the note's coords.
			 */
			if (is_tab && gs_p->notelist[n].headshape != allx_hsi) {

				/* handle tab (except when it's an X) */

				gs_p->notelist[n].c[RY] = gs_p->notelist[n].
					stepsup * TABRATIO * STEPSIZE;

				if (gs_p->notelist[n].FRETNO == NOFRET) {
					/* set RN and RS the same as RY */
					gs_p->notelist[n].c[RN] =
						gs_p->notelist[n].c[RY];
					gs_p->notelist[n].c[RS] =
						gs_p->notelist[n].c[RY];
				} else {
					/*
					 * Set vertical coordinates of the
					 * "note" (fret number).  It is to be
					 * centered on the appropriate line.
					 */
					vhalf = strheight(fret_string(&gs_p->
						notelist[n], gs_p)) / 2.0;
					gs_p->notelist[n].c[RN] =
						gs_p->notelist[n].c[RY] + vhalf;
					gs_p->notelist[n].c[RS] =
						gs_p->notelist[n].c[RY] - vhalf;
				}

			} else {

				/* handle non-tab and tab X-notes */

				/* find & store music font and char */
				hchar = nheadchar(gs_p->notelist[n].headshape,
					gs_p->basictime, gs_p->stemdir, &hfont);
				gs_p->notelist[n].headchar = hchar;
				gs_p->notelist[n].headfont = hfont;

				/* half the height of the note head */
				vhalf = height(hfont, gs_p->notelist[n].notesize
					== GS_NORMAL ? DFLT_SIZE : SMALLSIZE,
					hchar) / 2;

				/*
				 * Set actual relative vertical coords.  We need
				 * to recalculate the original stepsup, which
				 * was modified for CSS notes, because absvert.c
				 * needs to know what the note's coords would
				 * have been if it hadn't been CSS.  Sigh.
				 */
				stepsup = gs_p->notelist[n].stepsup;
				switch (gs_p->stemto) {
				case CS_ABOVE:
					if (n <= gs_p->stemto_idx) {
						stepsup -= CSS_STEPS;
					}
					break;
				case CS_BELOW:
					if (n >= gs_p->stemto_idx) {
						stepsup += CSS_STEPS;
					}
					break;
				}
				gs_p->notelist[n].c[RY] = stepsup * STEPSIZE *
					(is_tab ? TABRATIO : 1.0);

				gs_p->notelist[n].c[RN] =
					gs_p->notelist[n].c[RY] + vhalf;

				gs_p->notelist[n].c[RS] =
					gs_p->notelist[n].c[RY] - vhalf;
			}

			if (is_tab) {
				/*
				 * If there was a real bend, add to total height
				 * of the bend numbers.
				 */
				if (HASREALBEND(gs_p->notelist[n])) {
					bendheight += strheight(bend_string(
						&gs_p->notelist[n])) + STDPAD;
				}

				/* if any bend at all, remember it */
				if (HASBEND(gs_p->notelist[n])) {
					havebend = YES;
				}
			}
		}

		/*
		 * Set the group's coords.
		 */
		if (is_mrpt(gs_p)) {
			/* special handling for measure repeat */
			int mrpt_sym;
			int mrpt_font;
			mrpt_sym = mrptchar(gs_p, &mrpt_font);
			high = height(mrpt_font, DFLT_SIZE, mrpt_sym);
			gs_p->c[RX] = mr_y_offset(gs_p->staffno) * STEPSIZE;
			gs_p->c[RN] = gs_p->c[RX] + high / 2.0;
			gs_p->c[RS] = gs_p->c[RX] - high / 2.0;
		} else if (is_tab) {
			/*
			 * Set the group's north based on the top of the top
			 * bend number if there is one, otherwise the top of
			 * the top fret number.  We leave 3 "tab stepsizes" of
			 * white space between the staff and the lowest bend
			 * number, for the arrow.
			 */
			if (havebend == NO) {	/* no bends present */
				/* there must be frets, since no bends */
				gs_p->c[RN] = gs_p->notelist[0].c[RN] + STDPAD;
			} else {		/* bend(s) present */
				gs_p->c[RN] = (stafflines + 2) *
					STEPSIZE * TABRATIO + bendheight;
			}

			/*
			 * Set the group's south based on the bottom of the
			 * bottom fret number if there is one, otherwise the
			 * middle of the staff.
			 */
			if (gs_p->nnotes == 0) {	/* no frets present */
				gs_p->c[RS] = 0;
			} else {			/* frets present */
				gs_p->c[RS] = gs_p->notelist
					[ gs_p->nnotes - 1 ].c[RS] - STDPAD;
			}

			/* if bends, do work between this and other groups */
			if (bendheight > 0) {
				intertab(gs_p, mll_p);
			}
		} else {
			/*
			 * Non-tab: use the outermost non-CSS notes, but pad.
			 * If all notes are CSS, then set RN and RS to zero.
			 */
			switch (gs_p->stemto) {
			case CS_SAME:
				gs_p->c[RN] = gs_p->notelist
					[0].c[RN] + STDPAD;
				gs_p->c[RS] = gs_p->notelist
					[gs_p->nnotes-1].c[RS] - STDPAD;
				break;
			case CS_ABOVE:
				if (gs_p->stemto_idx == gs_p->nnotes - 1) {
					gs_p->c[RN] = gs_p->c[RS] = 0.0;
				} else {
					gs_p->c[RN] = gs_p->notelist
					   [gs_p->stemto_idx+1].c[RN] + STDPAD;
					gs_p->c[RS] = gs_p->notelist
					   [gs_p->nnotes-1].c[RS] - STDPAD;
				}
				break;
			case CS_BELOW:
				if (gs_p->stemto_idx == 0) {
					gs_p->c[RN] = gs_p->c[RS] = 0.0;
				} else {
					gs_p->c[RN] = gs_p->notelist
					   [0].c[RN] + STDPAD;
					gs_p->c[RS] = gs_p->notelist
					   [gs_p->stemto_idx-1].c[RS] - STDPAD;
				}
				break;
			}
		}
	}
}

/*
 * Name:        fixoneline()
 *
 * Abstract:    Fix stemsup and vertical coord for notes on one-line staffs.
 *
 * Returns:     void
 *
 * Description: stepsup and notes' vertical coords are set in locllnotes().
 *		For one-line staffs, it assumes the notes are on the line.
 *		But if the notes are not to be on the line, that isn't right.
 *		It depends on which voice this is.  Now that we have set the
 *		stem direction, we need to correct this info.
 */

static void
fixoneline()

{
	struct MAINLL *mainll_p;	/* point at main linked list item */
	struct STAFF *staff_p;		/* point at a STAFF */
	struct GRPSYL *gs_p;		/* point along a GRPSYL list */
	int v;				/* voice number, 0 or 1 */


	debug(16, "fixoneline");
	initstructs();			/* clean out old SSV info */

	/*
	 * Loop once for each item in the main linked list.  Apply any SSVs
	 * that are found.  Move notes that are not to be on the line.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			/* apply the SSV and go to the next item */
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		/* deal only with visible staffs that aren't measure rpts */
		if (mainll_p->str != S_STAFF ||
				mainll_p->u.staff_p->visible == NO ||
				is_mrpt(mainll_p->u.staff_p->groups_p[0])) {
			continue;
		}

		staff_p = mainll_p->u.staff_p;

		/* deal only with non-tab one-line staffs */
		if (svpath(staff_p->staffno, STAFFLINES)->stafflines != 1 ||
				svpath(staff_p->staffno, CLEF)->clef
				== TABCLEF) {
			continue;
		}

		/*
		 * Loop through voices 1 and 2, and process each list.  Note
		 * that voice 3 is always on the line, so we don't need to do
		 * anything to it.
		 */
		for (v = 0; v < NORMVOICES && staff_p->groups_p[v] != 0; v++) {

			/* change stepsup from 0 only if notes not on the line*/
			if (vvpath(staff_p->staffno, v + 1, ONTHELINE)->
					ontheline == YES) {
				continue;
			}

			for (gs_p = staff_p->groups_p[v]; gs_p != 0;
					gs_p = gs_p->next) {

				/* only notes are to be changed */
				if (gs_p->grpcont != GC_NOTES) {
					continue;
				}

				/* move up or down a step based on voice */
				if (gs_p->vno == 1) {
					gs_p->notelist[0].stepsup = 1;
					gs_p->notelist[0].c[RY] = STEPSIZE;
					gs_p->notelist[0].c[RN] += STEPSIZE;
					gs_p->notelist[0].c[RS] += STEPSIZE;
				} else {
					gs_p->notelist[0].stepsup = -1;
					gs_p->notelist[0].c[RY] = -STEPSIZE;
					gs_p->notelist[0].c[RN] -= STEPSIZE;
					gs_p->notelist[0].c[RS] -= STEPSIZE;
				}
			}
		}
	}
}
