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
 * Name:	abshorz.c
 *
 * Description:	This file contains functions for setting absolute
 *		horizontal coordinates.
 */

#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"

static void barclefsigs P((void));
static int barwithssv P((struct MAINLL *mainll_p));
static void setclefsigwid P((struct MAINLL *mainll_p, struct CHHEAD *chhead_p));
static void abschunk P((struct MAINLL *mainll_p, struct MAINLL *end_p));
static struct MAINLL *tryabs P((struct MAINLL *mainll_p,
		struct MAINLL *prevfeed_p, double scale, int *scores_p,
		short measinscore[]));
static int endchunk P((struct MAINLL *mainll_p));
static int prev_is_restart P((struct MAINLL *mainll_p));
static int this_is_restart P((struct MAINLL *mainll_p));
static int countable_measure P((struct MAINLL *mainll_p));
static double adjust_measwid4mrpt P((double oldmeaswid, struct CHORD *ch_p));
static void fillclefsig P((struct CLEFSIG *clefsig_p, struct MAINLL *feed_p));
static struct MAINLL *trymeasure P((struct MAINLL *mainll_p, double scale,
		float *measwidth_p, float *adjust_p, int *ressv_p));
static void setabs P((struct MAINLL *start_p, int scores, short measinscore[]));
static void set_right_margin P((struct MAINLL *start_p, double white));
static void chkrestart P((struct MAINLL *start_p, struct MAINLL *end_p));
static void setabsscore P((struct MAINLL *start_p, struct MAINLL *end_p));
static void adjustchords P((struct MAINLL *start_p, struct MAINLL *end_p));
static void setabschord P((struct CHORD *ch_p, double nomx));
static double effwidth P((struct CHORD *ch_p));
static double bardiff P((struct MAINLL *mainll_p, struct MAINLL *end_p));
static void fixfullmeas P((struct CHORD *ch_p, double x));
static void restore_grpsyl_west P((void));
static void setipw P((void));
static void setipwgrpsyl P((struct MAINLL *mainll_p, struct GRPSYL *gs_p));
static void setipwchord P((struct MAINLL *mainll_p));
static void setsubbars P((void));
static void fixendings P((void));
static void fixreh P((void));
static void clrinhprint P((void));
static int hidestaffs P((struct MAINLL *mainll_p, struct MAINLL *ml2_p));
static int silent P((struct MAINLL *mainll_p, struct MAINLL *ml2_p, int s,
		int *ressv_p));
static void move_allstuff P((struct MAINLL *mainll_p, struct MAINLL *ml2_p,
		int olds, int news));
static int getmultinum P((struct MAINLL *mll_p));

/*
 * Depending on which type of clefsig, get its full width, or its effective
 * width.  The latter is for user requested ones, which may overlap notes.
 */
#define EFF_WIDCLEFSIG(mainll_p, clefsig_p) \
	((clefsig_p)->clefsize == DFLT_SIZE ? \
		width_clefsig(mainll_p, clefsig_p) : (clefsig_p)->effwidth)

/*
 * Define the padding after a clefsig.  It's greater when there's a pseudobar,
 * because we need to allow room for carry-in ties/slurs.
 */
#define CSP(clefsig_p)	(((clefsig_p)->bar_p == 0 ? 2.0 : 9.0) * STDPAD)

/*
 * Name:        abshorz()
 *
 * Abstract:    Set absolute horizontal coordinates of everything.
 *
 * Returns:     void
 *
 * Description: This function inserts the initial FEED of the piece.  Then, for
 *		each section of the piece delimited by FEEDs (initial one and
 *		user-requested ones), it breaks it into the necessary number
 *		of scores (inserting more FEEDs), and sets all the horizontal
 *		absolute coordinates.  Finally, it does some fix-up work.
 */

void
abshorz()

{
	struct MAINLL *mainll_p;	/* point at any MAINLL member */
	struct MAINLL *ml2_p;		/* point at another MAINLL member */
	struct MAINLL *mainfeed_p;	/* point at MAINLL containing a FEED */
	struct MAINLL *end_p;		/* point to end of a chunk of MAINLL */
	int gotbar;			/* found a bar in this chunk */


	debug(16, "abshorz");
	ml2_p = 0;		/* prevent useless 'used before set' warning */

	/*
	 * The parse phase put any user-requested score feeds in the main
	 * linked list.  We must now insert a FEED before the first measure,
	 * unless the user already requested one there (unlikely!).  Later
	 * we'll add more as needed.
	 */
	for (mainll_p = Mainllhc_p;
	     mainll_p != 0 && mainll_p->str == S_SSV;
	     mainll_p = mainll_p->next)
		;	/* skip by all initial SSVs */

	if (mainll_p == 0)
		pfatal("main linked list has nothing but SSVs");

	if (mainll_p->str != S_FEED) {
		mainfeed_p = newMAINLLstruct(S_FEED, 0);
		insertMAINLL(mainfeed_p, mainll_p->prev);
	}

	/* whenever a CLEFSIG is needed after a bar line, put one there */
	barclefsigs();

	/*
	 * Find each section of the main linked list, delimited by FEEDs.
	 * For each such section, call abschunk() to set the absolute
	 * horizontal coords for things in that chunk.  At the end of each
	 * chunk, back up to avoid including SSVs/PRHEADs/LINEs/CURVEs which
	 * might follow the last measure of the chunk.  If a chunk contains no
	 * bar lines (like if there was a useless scorefeed at the end),
	 * don't call abschunk().
	 */
	/* skip anything before first FEED first */
	for (mainll_p = Mainllhc_p; mainll_p->str != S_FEED;
			mainll_p = mainll_p->next)
		;
	for (;;) {
		gotbar = NO;
		for (end_p = mainll_p->next;
		     end_p != 0 && end_p->str != S_FEED;
		     ml2_p = end_p, end_p = end_p->next) {

			if (end_p->str == S_BAR)
				gotbar = YES;
		}

		if (gotbar == NO) {
			/*
			 * If end_p is 0, this must be the end of the MLL, and
			 * there was a final feed after all the music data.
			 * There is no need to process this chunk.
			 */
			if (end_p == 0)
				break;
			/*
			 * This chunk must contain a BLOCKHEAD.  There is no
			 * need to process it.  Set the absolute horizontal
			 * coords.  Then point mainll_p at the FEED at the end
			 * of the chunk, and go to the next loop.
			 */
			mainll_p->u.feed_p->c[AW] = eff_leftmargin(mainll_p);
			mainll_p->u.feed_p->c[AE] = PGWIDTH -
					eff_rightmargin(end_p);
			mainll_p->u.feed_p->c[AX] = (mainll_p->u.feed_p->c[AW]
					+ mainll_p->u.feed_p->c[AE]) / 2.0;
			mainll_p = end_p;
			continue;
		}

		while (ml2_p->str == S_SSV || ml2_p->str == S_PRHEAD ||
				ml2_p->str == S_LINE || ml2_p->str == S_CURVE)
			ml2_p = ml2_p->prev;
		abschunk(mainll_p, ml2_p->next);
		if (end_p == 0)
			break;
		mainll_p = end_p;
	}

	/* restore west boundaries of GRPSYLs that have associated clefs */
	restore_grpsyl_west();

	/* set inches per whole ( c[INCHPERWHOLE] ) in the relevant structs */
	setipw();

	/* set positions of subbars */
	setsubbars();

	/* move endings that start at end of score to the next score */
	fixendings();

	/* move rehearsal marks at end of a score to the next score */
	fixreh();

	/* clear the inhibitprint flag on tablature staffs when appropriate */
	clrinhprint();
}

/*
 * Name:        barclefsigs()
 *
 * Abstract:    Put a CLEFSIG after each bar line that requires one.
 *
 * Returns:     void
 *
 * Description: This function loops through the main linked list, applying
 *		SSVs as it goes.  Whenever an SSV changes clef, key, or time,
 *		it inserts a CLEFSIG into the list to show what will need to
 *		be printed at that point.  It also inserts one after any bar
 *		that is a restart.
 */

static void
barclefsigs()

{
	struct MAINLL *mainll_p;	/* point at items in main linked list*/
	struct MAINLL *maincs_p;	/* point at MAINLL with CLEFSIG */
	struct MAINLL *mll_p;		/* point along MLL */
	struct CLEFSIG *clefsig_p;	/* point at CLEFSIG being filled in */
	struct BAR *bar_p;		/* point at the bar */
	struct CHHEAD *chhead_p;	/* point at the latest CHHEAD seen */
	struct TIMEDSSV *tssv_p;	/* point along timed SSV list */
	struct GRPSYL *gs_p;		/* point at a group */
	int oldstaffs;			/* no. of staffs before SSVs */
	int oldvis[MAXSTAFFS + 1];	/* visibility of staffs before SSVs */
	int oldclef[MAXSTAFFS + 1];	/* clefs before SSVs */
	int newclef[MAXSTAFFS + 1];	/* clefs after SSVs */
	int newclefforce[MAXSTAFFS + 1];/* force print clefs even if no change*/
	int premidclef[MAXSTAFFS + 1];	/* clefs before applying midmeas SSVs*/
	int oldkey[MAXSTAFFS + 1];	/* effective keys before SSVs */
	int newkey[MAXSTAFFS + 1];	/* effective keys after SSVs */
	int oldnormkey[MAXSTAFFS + 1];	/* can staff have a key sig? */
	int newnormkey[MAXSTAFFS + 1];	/* can staff have a key sig? */
	int oldnormclef[MAXSTAFFS + 1];	/* can staff have a clef? */
	int newnormclef[MAXSTAFFS + 1];	/* can staff have a clef? */
	char oldtimerep[MAXTSLEN];	/* time sig before SSVs */
	int oldclefsequal, oldkeysequal;/* were they equal on all staffs? */
	int newclefsequal, newkeysequal;/* are they equal on all staffs? */
	int gottimedssv;		/* were there any timed SSVs? */
	int vidx;			/* voice index */
	int lastclef;			/* last clef printed */
	RATIONAL offset;		/* offset of a group in a measure */
	RATIONAL lastclefoffset;	/* offset of last midmeasure clef */
	int timechg;			/* did they change time sig info? */
	int forceprinttime;		/* print time sig even if no change? */
	int printtime;			/* timechg or forceprinttime? */
	int change;			/* did they change clefs/keys/time? */
	register int s;			/* staff number */


	debug(16, "barclefsigs");
	initstructs();			/* clean out old SSV info */

	/* apply any SSVs that come before the first measure */
	mainll_p = Mainllhc_p;
	while (mainll_p != 0 && mainll_p->str == S_SSV) {
		asgnssv(mainll_p->u.ssv_p);
		mainll_p = mainll_p->next;
	}

	chhead_p = 0;		/* keep lint happy; will be set before used */

	/*
	 * Loop once for each bar line in the piece that has SSV(s) after it,
	 * or is a RESTART.  Whenever this occurs, insert a CLEFSIG after them
	 * if required.  RESTART always requires it.
	 */
	for (;;) {
		/*
		 * Find the next bar that is either followed by an SSV or has
		 * timed SSVs for the preceding measure or is a restart.  These
		 * are the cases where a CLEFSIG may be needed.  If we hit the
		 * end of the MLL, break out.
		 */
		while (mainll_p != 0 && ! (mainll_p->str == S_BAR &&
				(barwithssv(mainll_p) == YES ||
				mainll_p->u.bar_p->timedssv_p != 0 ||
				mainll_p->u.bar_p->bartype == RESTART))) {
			switch (mainll_p->str) {
			case S_CHHEAD:
				/* remember the last chhead */
				chhead_p = mainll_p->u.chhead_p;
				break;
			case S_SSV:
				/* apply SSVs */
				asgnssv(mainll_p->u.ssv_p);
				break;
			}
			mainll_p = mainll_p->next;
		}
		if (mainll_p == 0) {
			break;
		}
		bar_p = mainll_p->u.bar_p;

		/*
		 * If there were timed SSVs in the measure just ended, we need
		 * to make sure that any clef changes requested actually got
		 * printed before some GRPSYL.  If the clef was changed on one
		 * staff by <<score clef = whatever>>, some other staff may be
		 * affected and yet not have a GRPSYL after that point before
		 * which the clef can be printed.  In that case, we want to
		 * generate a CLEFSIG at this bar line, to print it.
		 */
		tssv_p = bar_p->timedssv_p;
		gottimedssv = tssv_p != 0;	/* remember if we had any */
		if (gottimedssv) {
			/* get clef state before the timed SSVs */
			for (s = 1; s <= Score.staffs; s++) {
				premidclef[s] = svpath(s, CLEF)->clef;
			}
			/* assign the timed SSVs */
			for ( ; tssv_p != 0; tssv_p = tssv_p->next) {
				asgnssv(&tssv_p->ssv);
			}
		}

		/*
		 * Save the current number of staffs, whether they are visible,
		 * and all clefs and effective keys in case the SSVs coming
		 * up will change some of these things.  Also, save the timesig
		 * info so we can check if it changed (it is settable only in
		 * the score).
		 * Set oldnormkey according to whether the staff is capable of
		 * having a key sig.
		 * Set oldnormclef according to whether the staff is capable of
		 * having a clef.
		 * Set oldclefsequal and oldkeysequal according to whether all
		 * staffs have the same values for clef and key.
		 */
		oldclefsequal = oldkeysequal = YES;
		for (s = 1; s <= Score.staffs; s++) {
			oldvis[s] = svpath(s, VISIBLE)->visible;
			oldclef[s] = svpath(s, CLEF)->clef;
			oldkey[s] = eff_key(s);
			oldnormkey[s] =
				svpath(s, STAFFLINES)->printclef == SS_NORMAL &&
				! is_tab_staff(s) ? YES : NO;
			oldnormclef[s] =
				svpath(s, STAFFLINES)->printclef != SS_NOTHING&&
				! is_tab_staff(s) ? YES : NO;
			if (s > 1 && oldclef[s - 1] != oldclef[s])
				oldclefsequal = NO;
			if (s > 1 && oldkey[s - 1] != oldkey[s])
				oldkeysequal = NO;
		}
		oldstaffs = Score.staffs;
		strcpy(oldtimerep, Score.timerep);

		/* see if clefs need printing due to timed SSVs */
		if (gottimedssv) {
			/* check this on every staff */
			for (s = 1; s <= Score.staffs; s++) {
				/* find this staff's MLL structure */
				for (mll_p = mainll_p; mll_p->str != S_STAFF ||
				     mll_p->u.staff_p->staffno != s;
				     mll_p = mll_p->prev) {
					;
				}
				/* don't force clefsig for an invisible staff*/
				if (mll_p->u.staff_p->visible == NO) {
					continue;
				}
				/*
				 * Find last clef that was printed for this
				 * measure.  Start with the value of the clef
				 * at the previous bar (before any midmeasure
				 * clefs).  Then update with the ones that were
				 * printed midmeasure, until we have the last.
				 */
				lastclef = premidclef[s];
				lastclefoffset = rneg(One);
				for (vidx = 0; vidx < MAXVOICES; vidx++) {
					/* look down this voice */
					offset = Zero;
					for (gs_p = mll_p->u.staff_p->groups_p[
					vidx]; gs_p != 0; gs_p = gs_p->next) {
						if (gs_p->clef != NOCLEF &&
						GT(offset, lastclefoffset)) {
							lastclef = gs_p->clef;
							lastclefoffset = offset;
						}
						offset = radd(offset,
							gs_p->fulltime);
					}
				}
				/*
				 * Set the oldclef to the last one printed.
				 * Then later code will create a CLEFSIG, if
				 * necessary.
				 */
				oldclef[s] = lastclef;
			}
		}

		/*
		 * Loop through this set of SSV(s), applying them.  If we hit
		 * the end of the main linked list, break out.  We don't want
		 * to put a CLEFSIG after the last bar line, regardless of
		 * whether it changed anything.
		 * Also, if an SSV requests printing the time sig regardless of
		 * whether it changed, set a flag.  (For alternating time sigs,
		 * the parse phases generates an SSV at each bar line, and
		 * since the time changes at every bar, we will print it
		 * unless they set timevis to PTS_NEVER to prevent it.  But for
		 * simple time sigs, if the user sets the y flag, this code has
		 * the logic for making sure we print it.)
		 */
		forceprinttime = NO;
		mainll_p = mainll_p->next;
		while (mainll_p != 0 && mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
			if (mainll_p->u.ssv_p->used[TIME] == YES &&
			    mainll_p->u.ssv_p->timevis == PTS_ALWAYS) {
				forceprinttime = YES;
			}
			mainll_p = mainll_p->next;
		}
		/* Retain mainll_p for later; loop onwards using mll_p.  Need */
		/* to keep looking for SSVs in case there is a block here. */
		mll_p = mainll_p;
		while (mll_p != 0 && (mll_p->str == S_SSV ||
				      mll_p->str == S_FEED ||
				      mll_p->str == S_BLOCKHEAD)) {
			if (mll_p->str == S_SSV) {
				asgnssv(mll_p->u.ssv_p);
			}
			mll_p = mll_p->next;
		}
		if (mainll_p == 0)
			break;


		/*
		 * Get the new clefs and effective keys.
		 * Again, find out if the clefs and keys are equal on all
		 * staffs.
		 */
		newclefsequal = newkeysequal = YES;
		for (s = 1; s <= Score.staffs; s++) {
			newclef[s] = svpath(s, CLEF)->clef;
			newclefforce[s] = svpath(s, CLEF)->forceprintclef;
			newkey[s] = eff_key(s);
			newnormkey[s] =
				svpath(s, STAFFLINES)->printclef == SS_NORMAL &&
				! is_tab_staff(s) ? YES : NO;
			newnormclef[s] =
				svpath(s, STAFFLINES)->printclef != SS_NOTHING&&
				! is_tab_staff(s) ? YES : NO;
			if (s > 1 && newclef[s - 1] != newclef[s])
				newclefsequal = NO;
			if (s > 1 && newkey[s - 1] != newkey[s])
				newkeysequal = NO;
		}

		/* first check if any time sig info changed */
		if (strcmp(Score.timerep, oldtimerep) != 0 &&
				Score.timevis != PTS_NEVER)
			timechg = YES;
		else
			timechg = NO;

		/* these are the reasons for printing the time sig */
		printtime = (timechg == YES || forceprinttime == YES);

		/*
		 * If the bar was a restart, we treat it as if it were at the
		 * start of a score.  That is, we always print the clefs and
		 * key signatures.  The clefs are full size, and no naturals
		 * are printed.  Print the time signature only if it changed.
		 */
		if (bar_p->bartype == RESTART) {
			/*
			 * We always require a CLEFSIG.  Allocate one and put
			 * it between where we are now and the preceding
			 * structure.
			 */
			maincs_p = newMAINLLstruct(S_CLEFSIG, 0);
			insertMAINLL(maincs_p, mainll_p->prev);
			clefsig_p = maincs_p->u.clefsig_p;

			clefsig_p->clefsize = DFLT_SIZE;	/* full size */
			clefsig_p->multinum = getmultinum(maincs_p);

			/*
			 * Note: If the number of staffs is changing here, the
			 * following might not be right.  But it doesn't
			 * matter, because in that case this CLEFSIG will be
			 * thrown away later anyway.
			 */
			for (s = 1; s <= oldstaffs; s++) {
				if (oldnormkey[s] == YES &&
				    newnormkey[s] == YES) {
					clefsig_p->sharps[s] = newkey[s];
				}
				if (oldnormclef[s] == YES &&
				    newnormclef[s] == YES) {
					clefsig_p->prclef[s] = YES;
				}
			}

		 	/* print the time signature if it is needed */
			clefsig_p->prtimesig = printtime;

			continue;
		}

		/*
		 * When the number of staffs changes, special rules apply.
		 * Handle this situation and continue.
		 */
		if (oldstaffs != Score.staffs) {
			/*
			 * Identify the cases where no clefsig is needed, and
			 * continue.  This is when time isn't needed, and no
			 * clefs or keys are to be printed.  Clefs are to be
			 * printed only when all the old staffs had the same
			 * clef, all the new ones had the same clef, and the
			 * old and new clefs are different.  The analogous rule
			 * holds for keys.  This is because, when the number of
			 * staffs changes, we can't really tell which old staff
			 * corresponds to which new staff (if any), so it's
			 * silly to print a clef or key change on any.
			 */
			if (printtime == NO &&
					(oldclefsequal == NO ||
					 newclefsequal == NO ||
					 oldclef[1] == newclef[1]) &&
					(oldkeysequal == NO ||
					 newkeysequal == NO ||
					 oldkey[1] == newkey[1])) {
				/* no CLEFSIG needed here */
				continue;
			}

			/*
			 * Something changed that requires a CLEFSIG.  Allocate
			 * one and put it between where we are now and the
			 * preceding structure, the last SSV we applied.
			 */
			maincs_p = newMAINLLstruct(S_CLEFSIG, 0);
			insertMAINLL(maincs_p, mainll_p->prev);
			clefsig_p = maincs_p->u.clefsig_p;

			/* any clefs to be printed should be small size */
			clefsig_p->clefsize = SMALLSIZE;
			clefsig_p->multinum = getmultinum(maincs_p);

			/*
			 * Since the number of staffs is changing, there will
			 * be a scorefeed here, and this CLEFSIG will be at the
			 * end of the first score.  Every old staff will have
			 * the same info printed by the CLEFSIG, except that
			 * clef and key never exist on "abnormal" staffs. Check
			 * against staff 1 since we know that has to exist on
			 * both sides.  It doesn't hurt to mark even invisible
			 * ones.
			 * Since we don't know which old staff is intended to
			 * match which new staff, we can't look at the new
			 * staffs' forceprintclef flags to force printing on
			 * these old staffs.
			 */
			for (s = 1; s <= oldstaffs; s++) {
				if (oldnormclef[s] == NO || newnormclef[s] == NO)
					continue;
				/*
				 * Draw the new clef if the clefs on each score
				 * are consistent and they changed.
				 */
				if (oldclefsequal && newclefsequal &&
						oldclef[1] != newclef[1])
					clefsig_p->prclef[s] = YES;

			}
			for (s = 1; s <= oldstaffs; s++) {
				if (oldnormkey[s] == NO || newnormkey[s] == NO)
					continue;
				/*
				 * Draw the new key if the keys on each score
				 * are consistent and they changed.  See below
				 * for a more detailed explanation.
				 */
				if (oldkeysequal && newkeysequal &&
						oldkey[1] != newkey[1]) {
					clefsig_p->sharps[s] = newkey[1];

					if (newkey[1] == 0) {
						clefsig_p->naturals[s] =
								oldkey[1];
					} else if (svpath(s, CANCELKEY)->
							cancelkey == YES) {
						if (oldkey[1] * newkey[1] < 0) {
							/* 1 has #s, 1 has &s */
							clefsig_p->naturals[s] =
								oldkey[1];
						} else if (abs(oldkey[1]) >
								abs(newkey[1])){
							/* new has fewer accs*/
							clefsig_p->naturals[s] =
							oldkey[1] - newkey[1];
						}
					}
				}
			}

		 	/* print the time signature if it is needed */
			if (printtime == YES) {
				clefsig_p->prtimesig = YES;
			}

			/* set clefsig's effective width */
			setclefsigwid(maincs_p, chhead_p);

			continue;
		}


		change = printtime;

		/* see if anything else requiring a CLEFSIG changed */
		for (s = 1; s <= oldstaffs; s++) {
			if (oldvis[s] == NO ||
			    svpath(s, VISIBLE)->visible == NO)
				continue;
			if (oldclef[s] != newclef[s] || newclefforce[s] == YES)
				change = YES;
			if (oldkey[s] != newkey[s])
				change = YES;
			if (change == YES)
				break;	/* don't waste any more time looping */
		}
		if (change == NO)
			continue;	/* no visible time, key, clef changed*/

		/*
		 * If we get here, it means either the clef, effective key, or
		 * time changed on some visible staff, or that the
		 * forceprintclef was used.  Allocate a CLEFSIG and
		 * put it between where we are now and the preceding structure,
		 * which is the last SSV we applied.
		 */
		maincs_p = newMAINLLstruct(S_CLEFSIG, 0);
		insertMAINLL(maincs_p, mainll_p->prev);
		clefsig_p = maincs_p->u.clefsig_p;

		/* any clefs to be printed should be small size */
		clefsig_p->clefsize = SMALLSIZE;
		clefsig_p->multinum = getmultinum(maincs_p);

		/*
		 * Loop through the staffs, marking in the CLEFSIG what should
		 * be drawn.
		 */
		for (s = 1; s <= Score.staffs; s++) {
			/* draw nothing if this staff is invisible */
			if (oldvis[s] == NO)
				continue;

			/* draw nothing if this staff is not "normal" */
			if (oldnormclef[s] == NO || newnormclef[s] == NO)
				continue;

			/* draw the new clef if the clef changed or forceprint*/
			if (oldclef[s] != newclef[s] || newclefforce[s] == YES)
				clefsig_p->prclef[s] = YES;

		}
		for (s = 1; s <= Score.staffs; s++) {
			/* draw nothing if this staff is invisible */
			if (oldvis[s] == NO)
				continue;

			/* draw nothing if this staff is not "normal" */
			if (oldnormkey[s] == NO || newnormkey[s] == NO)
				continue;
			/*
			 * If the effective key changed, draw the new key
			 * signature.  But if the new key has 0 sharps, we
			 * should draw naturals in the shape of the old key
			 * signature.  And if cancelkey is set, and a sharp key
			 * is changing to a flat key or vice versa, or the
			 * number of sharps or flats is being reduced, we need
			 * enough naturals to cancel the ones being removed.
			 */
			if (oldkey[s] != newkey[s]) {
				clefsig_p->sharps[s] = newkey[s];
				if (newkey[s] == 0) {
					clefsig_p->naturals[s] = oldkey[s];
				} else if (svpath(s, CANCELKEY)->
						cancelkey == YES) {
					if (oldkey[s] * newkey[s] < 0) {
						/* 1 has sharps, 1 has flats */
						clefsig_p->naturals[s] =
							oldkey[s];
					} else if (abs(oldkey[s]) >
							abs(newkey[s])) {
						/* new has fewer accidentals */
						clefsig_p->naturals[s] =
							oldkey[s] - newkey[s];
					}
				}
			}
		}

		/*
		 * Finally, print the time signature if it is needed.
		 */
		if (printtime == YES) {
			clefsig_p->prtimesig = YES;
		}

		/* set clefsig's effective width, and widestclef */
		setclefsigwid(maincs_p, chhead_p);
	}
}

/*
 * Name:        barwithssv()
 *
 * Abstract:    Is this bar followed by SSV(s), ignoring FEEDs and BLOCKHEADs?
 *
 * Returns:     YES or NO
 *
 * Description: This function is called with the MLL structure for a BAR.
 *		Ignoring the possible presence of FEEDs and BLOCKHEADs, return
 *		YES if the next structure is an SSV.
 */

static int
barwithssv(mainll_p)

struct MAINLL *mainll_p;	/* for the BAR */

{
	struct MAINLL *mll_p;		/* loop after the BAR */


	for (mll_p = mainll_p->next; mll_p != 0; mll_p = mll_p->next) {
		switch (mll_p->str) {
		case S_SSV:
			return (YES);
		case S_FEED:
		case S_BLOCKHEAD:
			break;		/* ignore these, keep looking */
		default:
			return (NO);
		}
	}

	return (NO);		/* hit end of MLL */
}

/*
 * Name:        setclefsigwid()
 *
 * Abstract:    Set effective width and widest clef of a user requested clefsig.
 *
 * Returns:     void
 *
 * Description: This function is called with a user-requested clefsig.  If it
 *		contains clefs, they should be printed before the bar line.  If
 *		we are lucky, part or all of the clef's widths can overlap notes
 *		on other staffs.  The effective width is the full width minus
 *		the amount that can overlap.  This function sets the effective
 *		width of the clefsig, and the widest clef that it contains.  We
 *		need to do this now, because we need to use the pseudo absolute
 *		coords set by relxchord() before they are overwritten later in
 *		abshorz.c.
 */

static void
setclefsigwid(mainll_p, chhead_p)

struct MAINLL *mainll_p;	/* point at the given clefsig's MLL struct */
struct CHHEAD *chhead_p;	/* point at the preceding chhead */

{
	struct MAINLL *m2_p;		/* another pointer down the MLL */
	struct CLEFSIG *clefsig_p;	/* point at a clefsig */
	struct CHORD *ch_p;		/* point at a chord */
	struct STAFF *staff_p;		/* point at a staff */
	struct GRPSYL *gs_p;		/* point at a group in a voice */
	struct GRPSYL *gs2_p;		/* point at a group in a chord */
	float lasteast;			/* phony AE of last chord in measure */
	float clefroom;			/* room needed for printing clefs */
	float clefwid;			/* width of a clef */
	float widestclef;		/* width of the widest to be printed */
	float gap;			/* between last group & last chord */
	int staffno;			/* staff number, 1 to MAXSTAFFS */
	int clef;			/* clef type */
	int v;				/* voice number, 0 to 2 */


	debug(16, "setclefsigwid");

	clefsig_p = mainll_p->u.clefsig_p;	/* convenient pointer */

	/*
	 * Although relxchord() overlaps chords in various ways, it does not
	 * overlap the last chord in the measure with anything following.
	 * And it sets phony absolute coordinates for each chord, based on
	 * pretending that everything is packed as tight as possible.  So, as
	 * the rightmost coord of all groups, we can use the AE of the last
	 * chord of the measure just ended.
	 */
	for (ch_p = chhead_p->ch_p; ch_p->ch_p != 0; ch_p = ch_p->ch_p)
		;
	lasteast = ch_p->c[AE];

	/*
	 * Init the amount of space needed for clefs to be printed.  We start
	 * at zero, and whenever a clef is to be printed, we find out how much
	 * of it can't be overlapped; and clefroom keeps track of the maximum
	 * of these for all staffs.
	 */
	clefroom = 0.0;

	widestclef = 0.0;	/* max width of any clef to be printed */

	/*
	 * Loop backwards through the preceding measure, looking for visible
	 * staffs to process.
	 */
	for (m2_p = mainll_p; m2_p->str != S_CHHEAD; m2_p = m2_p->prev) {

		if (m2_p->str != S_STAFF || m2_p->u.staff_p->visible == NO)
			continue;

		staff_p = m2_p->u.staff_p;
		staffno = staff_p->staffno;

		/* if no clef, it doesn't need any space */
		if (clefsig_p->prclef[staffno] == NO)
			continue;

		/* find width of this clef, including padding */
		clef = svpath(staffno, CLEF)->clef;
		set_staffscale(staffno);
		clefwid = (clefwidth(clef, YES) + CLEFPAD) * Staffscale;

		/* remember biggest clef width */
		if (clefwid > widestclef)
			widestclef = clefwid;

		/* loop through all voices on this staff */
		for (v = 0; v < MAXVOICES; v++) {

			/* find last group in this voice */
			gs_p = staff_p->groups_p[v];
			if (gs_p == 0)
				continue;
			for ( ; gs_p->next != 0; gs_p = gs_p->next)
				;

			/*
			 * Find out what chord this group belongs to, by
			 * searching down the GRPSYL list hanging off each
			 * chord in this measure.
			 */
			for (ch_p = chhead_p->ch_p; ch_p != 0;
					ch_p = ch_p->ch_p) {
				for (gs2_p = ch_p->gs_p; gs2_p != 0;
						gs2_p = gs2_p->gs_p) {
					/* if found, or after the right staff*/
					if (gs2_p == gs_p || gs2_p->staffno >
							     gs_p->staffno)
						break;
				}

				/*
				 * If we found it, find the gap between this
				 * group's AE and the last chord's.  If the
				 * amount of the clef's width that does not fit
				 * in that gap is the greatest so far, save it.
				 * However, if the clef wouldn't overlap the
				 * group vertically, ignore it.
				 */
				if (gs2_p == gs_p) {
					/* if no vertical overlap, no problem */
					if (clef_vert_overlap(clef, gs_p)==NO) {
						break;
					}
					gap = lasteast -
						(ch_p->c[AX] + gs_p->c[RE]);

					if (clefwid - gap > clefroom) {
						clefroom = clefwid - gap;
					}

					break;	/* look no more */
				}
			}
		} /* loop through voices on a staff */
	} /* loop through staffs */

	clefsig_p->widestclef = widestclef;

	/* (effective width) = (real width) - (what can overlap) */
	clefsig_p->effwidth = width_clefsig(mainll_p, clefsig_p) -
			(widestclef - clefroom);
}

/*
 * Name:        abschunk()
 *
 * Abstract:    Set the absolute horz. coords of everything in one chunk.
 *
 * Returns:     void
 *
 * Description: This function is given a chunk of the piece, which is
 *		delimited by FEEDs.  It estimates how many inches should
 *		be allocated to each whole note of time.  Then it calls
 *		tryabs() repeatedly, trying to find a scale factor that
 *		will avoid having the last score be too empty.  Finally,
 *		it calls setabs() to set the absolute horizontal coordinates
 *		of everything in the chunk.
 */

static void
abschunk(start_p, end_p)

struct MAINLL *start_p;		/* FEED at start of chunk of MAINLL */
struct MAINLL *end_p;		/* points after last struct in chunk (or 0) */

{
	float totwidth;		/* total minimal width of this chunk */
	float totpseudodur;	/* total pseudodur of measures in this chunk */
	struct MAINLL *mainll_p;/* point at items in main linked list*/
	struct MAINLL *prevfeed_p; /* remember where last FEED in chunk goes */
	struct CHORD *ch_p;	/* point at a chord */
	short *measinscore;	/* malloc'ed array; bars in each score */
	float packfact;		/* as it was at the start of this score */
	float lowscale;		/* small guess at inches per whole */
	float origlowscale;	/* remember original lowscale */
	float highscale;	/* large guess at inches per whole */
	float midscale;		/* average of low and high scale */
	float measwidth;	/* width of a measure */
	int numbars;		/* number of measures in this chunk */
	int scores;		/* number of scores needed for this chunk */
	int reqscores;		/* the number of score required */
	int trial;		/* trial number for getting correct scale */
	int must_set_right_margin;   /* did user say "rightmargin = auto"? */


	debug(16, "abschunk file=%s line=%d", start_p->inputfile,
			start_p->inputlineno);

	/*
	 * For our first estimate of how wide to make everything, we need to
	 * add up the total minimal width and total elapsed time.
	 */
	/* must apply all SSVs from start, to get the right time sig */
	initstructs();			/* clean out old SSV info */
	for (mainll_p = Mainllhc_p; mainll_p != start_p;
			mainll_p = mainll_p->next) {
		if (mainll_p->str == S_SSV)
			asgnssv(mainll_p->u.ssv_p);
	}

	packfact = Score.packfact;	/* use current value (start of score) */
	totwidth = 0;		/* start totals at 0 */
	totpseudodur = 0;
	numbars = 0;

	/* loop through chunk, adding up width and time */
	for ( ; mainll_p != end_p; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			/* assign to keep time sig accurate */
			asgnssv(mainll_p->u.ssv_p);
			break;

		case S_CHHEAD:
			/*
			 * Add in the minimum widths of all chords in the
			 * measure, and add up the pseudoduration.  There are
			 * special things done for mrpt: 1) if all staff(s)
			 * have mrpt, we don't want them to "deserve" as much
			 * space as an mr, so reduce their pseudodur; 2) the
			 * minimum width of the measure must be made wide
			 * enough to contain the symbol.  Also, allow room
			 * for any midmeasure clefs.  (This will happen
			 * automatically, because the group boundaries still
			 * include their preceding midmeasure clefs at this
			 * point.)
			 */
			measwidth = 0;
			ch_p = mainll_p->u.chhead_p->ch_p;
			if (ABSDIFF(ch_p->width, TEMPMRPTWIDTH) < 0.001) {
				/* the 0.001 is to allow for roundoff error */
				ch_p->pseudodur *= 0.5;
			}
			for ( ; ch_p != 0; ch_p = ch_p->ch_p) {
				measwidth += ch_p->width;
				/* only count time if there is a real width */
				/* (nonspace, or "us", or padded "s") */
				if (ch_p->width != 0)
					totpseudodur += ch_p->pseudodur;
			}
			/* add this measure into the total */
			totwidth += adjust_measwid4mrpt(measwidth,
					mainll_p->u.chhead_p->ch_p);
			break;

		case S_CLEFSIG:
			/* width of clef/key/time to print when changing */
			totwidth += EFF_WIDCLEFSIG(mainll_p,
					mainll_p->u.clefsig_p) +
					CSP(mainll_p->u.clefsig_p);
			break;

		case S_BAR:
			/* add width of bar line, and incr no. of bars */
			totwidth += width_barline(mainll_p->u.bar_p);
			numbars++;
			break;
		}
	}

	/* did the user use "rightmargin = auto" at the end of this chunk? */
	must_set_right_margin = eff_rightmargin(start_p->next) == MG_AUTO;

	/*
	 * Allocate the measinscore array.  We need an element for each score
	 * that this chunk will be broken up into.  We don't yet know how
	 * many that will be.  So allocate enough for the worst case, where
	 * each measure is so wide that it has to go on a separate score.
	 */
	MALLOCA(short, measinscore, numbars + 1);

	/*
	 * Our first trial is to allow "packfact" times the minimal
	 * width we have just added up, partly to allow for the stuff at the
	 * start of each score (more CLEFSIGs to be inserted after we know
	 * where the FEEDs are going to be), and partly so things can spread
	 * out nicely.
	 */
	lowscale = packfact * totwidth / totpseudodur;
	origlowscale = lowscale;
	(void)tryabs(start_p, (struct MAINLL *)0, lowscale, &scores,
			measinscore);

	/*
	 * If the whole chunk fits on one score, just set the absolute coords
	 * for this score and get out.  Also, if the user wants us to
	 * calculate a good right margin for the last score, skip all the
	 * rebalancing code below; just call setabs().
	 */
	if (scores == 1 || must_set_right_margin) {
		setabs(start_p, scores, measinscore);
		FREE(measinscore);
		return;
	}

	/*
	 * However many scores tryabs() says were needed, that is what we will
	 * require.  But it's likely that the last score is far from filled up.
	 * It would look bad to just spread out the stuff in the last score.
	 * We want to "balance the load".
	 *
	 * So make up a new scale (highscale) which would probably force us to
	 * use an additional score.  Then loop, binary searching, to find a
	 * value for scale almost as big as possible without forcing a new
	 * score.
	 */
	reqscores = scores;
	highscale = 3.0 * lowscale;
	for (trial = 0; trial < 12; trial++) {
		midscale = (lowscale + highscale) / 2;
		prevfeed_p = tryabs(start_p, (struct MAINLL *)0, midscale,
				&scores, measinscore);
		if (scores > reqscores) {
			highscale = midscale;
		} else { /* must be equal, can never be less */
			lowscale = midscale;
		}
	}
	/*
	 * If the last one we tried is not a good one, we have to run tryabs
	 * again to reset the scores array properly.
	 */
	if (midscale != lowscale) {
		prevfeed_p = tryabs(start_p, (struct MAINLL *)0, lowscale,
				&scores, measinscore);
	}

	/*
	 * Although the above algorithm works surprisingly well, there are
	 * still cases occasionally where the last score has too few measures.
	 * So, if there are more than TAIL scores, we will run the algorithm
	 * again, on just the last TAIL scores, leaving all earlier scores as
	 * before.  This gives a better chance at finding a good balance.
	 */
#define TAIL	(3)
	if (scores > TAIL) {
		struct MAINLL *laststart_p;	/* start of last TAIL scores */
		int remember[TAIL];		/* old values from measinscore*/
		int last_scores_bars;		/* how many measures they have*/
		int lastscores;			/* we want this to remain TAIL*/
		int n;				/* loop variable */

		/*
		 * Find total number of measures in the last TAIL scores, while
		 * remembering the number on each.
		 */
		last_scores_bars = 0;
		for (n = 0; n < TAIL; n++) {
			remember[n] = measinscore[scores - TAIL + n];
			last_scores_bars += remember[n];
		}

		/* find where in the MLL they start */
		for (mainll_p = start_p, n = 0; n < numbars - last_scores_bars;
				mainll_p = mainll_p->next) {
			if (mainll_p->str == S_BAR) {
				n++;
			}
		}
		/* point at the CHHEAD of the first of these measures */
		while (mainll_p->str != S_CHHEAD) {
			mainll_p = mainll_p->next;
		}
		laststart_p = mainll_p;

		/*
		 * Run the algorithm on these TAIL scores.  To be sure that we
		 * can fit it on TAIL scores, start with a lowscale at zero.
		 * If we used the original lowscale, there are cases where it
		 * would go onto more scores.
		 */
		lowscale = 0.0;
		highscale = 3.0 * origlowscale;	/* as we did the first time */
		for (trial = 0; trial < 12; trial++) {
			midscale = (lowscale + highscale) / 2;
			(void)tryabs(laststart_p, prevfeed_p, midscale,
				&lastscores, &measinscore[scores - TAIL]);
			if (lastscores > TAIL) {
				highscale = midscale;
			} else { /* could be equal or less */
				lowscale = midscale;
			}
		}
		if (midscale != lowscale) {
			(void)tryabs(laststart_p, prevfeed_p, lowscale,
				&lastscores, &measinscore[scores - TAIL]);
		}

		/*
		 * This should never happen, unless possibly in some extreme
		 * case.  If it does, we give up and restore the original
		 * values.
		 */
		if (lastscores != TAIL) {
			for (n = 0; n < TAIL; n++) {
				measinscore[scores - TAIL + n] = remember[n];
			}
		}
	}

	/* set all coordinates based on the layout we just found */
	setabs(start_p, scores, measinscore);

	FREE(measinscore);
}

/*
 * Name:        tryabs()
 *
 * Abstract:    Given trial scale, find how many scores are needed, etc.
 *
 * Returns:	ptr to the last CHHEAD in chunk (where the last FEED will go)
 *
 * Description: This function, given a proposed scale factor for a chunk of
 *		measures, figures out how many measures would fit on each
 *		score.  The chunk is either a section of the piece delimited by
 *		FEEDs; or just the measures of such a section that a previous
 *		call here has determined should go onto the last TAIL scores.
 *		If the user said "rightmargin = auto", this function will
 *		calculate a good value and store it in the ending FEED,
 *		overwriting the "auto" indication.  That's okay, because in
 *		this case tryabs() will only be called once.
 */

static struct MAINLL *
tryabs(start_p, prevfeed_p, scale, scores_p, measinscore)

struct MAINLL *start_p;		/* if we're doing a whole chunk, this is the
				 * FEED at the start; if we're doing last TAIL
				 * scores of a chunk, this is the CHHEAD before
				 * the last TAIL scores */
struct MAINLL *prevfeed_p;	/* if we're doing a whole chunk, this is 0; if
				 * we're doing the last TAIL scores of a chunk,
				 * this at the start of the last TAIL+1 scores*/
double scale;			/* inches per "whole" unit of time */
int *scores_p;			/* return number of scores needed */
short measinscore[];		/* return number of measures in each score */

{
	struct MAINLL *mainll_p;/* points along main linked list */
	struct MAINLL *new_p;	/* points at first struct in new measure */
	struct MAINLL *ml2_p;	/* another general pointer into MAINLL */
	struct MAINLL *old_mainll_p; /* remember in case of indentrestart */
	float remwidth;		/* width remaining on this score */
	float old_remwidth;	/* remember in case of indentrestart */
	int old_measinscore;	/* remember in case of indentrestart */
	int old_countable_measures;	/* remember in case of indentrestart */
	int force_newscore;	/* due to restart, must we force a new score? */
	float userdelta;	/* (user right margin) - (normal right margin)*/
	int atend;		/* are we at the end of the chunk? */
	struct CLEFSIG clefsig;	/* temporary CLEFSIG for start of each score */
	struct BAR bar;		/* temp BAR; may be need by the above CLEFSIG*/
	float measwidth;	/* width needed by one measure */
	float adjust;		/* bar line adjust if last measure in score */
	int maxmeasures;	/* remember value at start of this score */
	int countable_measures;	/* on one score, limit this by maxmeasures */
	int ressv;		/* do we have to re-initstructs() and re-SSV?*/
	int must_set_right_margin;  /* did user say "rightmargin = auto"? */
	float initial_space;	/* space initially available on a score */
	float used_space;	/* total space used on all the scores */
	float unused_space;	/* total space not used on all the scores */


	debug(32, "tryabs file=%s line=%d scale=%f", start_p->inputfile,
			start_p->inputlineno, (float)scale);
	/* must apply all SSVs from start, to get the right clef/key/time; */
	setssvstate(start_p);
	maxmeasures = Score.maxmeasures;

	mainll_p = start_p;
	old_mainll_p = 0;

	/*
	 * Set up for beginning of first score in this chunk.
	 * Find out how much width is available, allowing for
	 * margins and stuff to the left (labels, etc.).
	 */
	*scores_p = 0;			/* no scores yet */
	if (start_p->str == S_FEED) {
		/*
		 * We're doing a whole chunk.  The left margin may have a user
		 * override.  For now, assume the right margin doesn't.  The
		 * previous FEED is in the previous chunk and really exists by
		 * now (unless this is the first chunk), so we can use
		 * width_left_of_score.
		 */
		remwidth = PGWIDTH - eff_rightmargin((struct MAINLL *)0)
					- eff_leftmargin(start_p);
		remwidth -= width_left_of_score(start_p);
	} else {
		/*
		 * We're (re)doing the last TAIL scores of a chunk.  So we know
		 * there is no user newscore/newpage and therefore no left
		 * margin user override.  Again, for now assume no right either.
		 * prevfeed_p is not really a FEED; we haven't inserted it yet.
		 */
		remwidth = PGWIDTH - eff_rightmargin((struct MAINLL *)0)
					- eff_leftmargin((struct MAINLL *)0);
		remwidth -= pwidth_left_of_score(start_p, prevfeed_p);
	}
	measinscore[0] = 0;
	countable_measures = 0;

	/*
	 * If the user overrode the right margin at the end of this chunk, we
	 * need to know the difference between what they requested and what the
	 * normal value is.  If they didn't, userdelta will be zero.
	 * We also set userdelta to zero if we are supposed to calculate the
	 * right margin later.
	 */
	must_set_right_margin = eff_rightmargin(start_p->next) == MG_AUTO;
	if (must_set_right_margin) {
		userdelta = 0.0;
	} else {
		userdelta = eff_rightmargin(start_p->next) -
			    eff_rightmargin((struct MAINLL *)0);
	}

	prevfeed_p = start_p;	/* init previous FEED to the start of chunk */

	/*
	 * We need to set up a provisional CLEFSIG containing what would need
	 * to be printed at the start of this new score.  We can't put it in
	 * the real MAINLL yet, since this function is just trying a
	 * possibility, and cannot alter MAINLL for real.  Set a pointer to
	 * a bar, which in real life would be allocated by fillclefsig.
	 * Subtract the clefsig's width from what we have left to work with.
	 */
	(void)memset((char *)&clefsig, 0, sizeof(clefsig));
	(void)memset((char *)&bar, 0, sizeof(bar));
	clefsig.bar_p = &bar;
	fillclefsig(&clefsig, mainll_p);
	remwidth -= width_clefsig(mainll_p, &clefsig) + CSP(&clefsig);
	old_remwidth = remwidth;
	used_space = 0.0;
	unused_space = 0.0;
	initial_space = remwidth;
	force_newscore = NO;
	old_measinscore = 0;
	old_countable_measures = 0;

	/* loop through chunk, once per measure, finding where FEEDs would go*/
	for (;;) {
		/* get width of this measure, and where next one starts */
		new_p = trymeasure(mainll_p, scale, &measwidth, &adjust,&ressv);

		atend = endchunk(new_p);	/* last measure of chunk? */

		/*
		 * This measure will be put on this score (line) if the
		 * following conditions are met:
		 */
		    /* There are no measures here yet (so we better force this
                     * one onto there; we'll fail later if this in fact cannot
		     * fit), OR */
		if (measinscore[*scores_p] == 0 ||

		    /* we are not forcing a restart measure onto the next
		     * score, AND */
		   (force_newscore == NO &&

		    /* we haven't reached the max number of measures allowed
		     * on a score, AND */
		    countable_measures < maxmeasures &&

		    /* there is enough room remaining for this measure to fit */
		    ((!atend && (measwidth - adjust) <= remwidth) ||
		      (atend && (measwidth - adjust) <= remwidth - userdelta)))){

			/*
			 * If this is restart pseudo measure, save the current
			 * values of the SSVs and some local variables, in case
			 * we need to back up later due to "indentrestart".
			 */
			if (this_is_restart(mainll_p)) {
				old_remwidth = remwidth;
				old_mainll_p = mainll_p;
				old_measinscore = measinscore[*scores_p];
				old_countable_measures = countable_measures;
				savessvstate();
			}

			/*
			 * Subtract this measure's width from what's left on
			 * this score, and increment the number of measures on
			 * it.  Point at the next measure.
			 */
			remwidth -= measwidth;
			measinscore[*scores_p]++;
			if (countable_measure(mainll_p)) {
				countable_measures++;
			}
			mainll_p = new_p;

			/* if we are at the end, inc no. of scores & return */
			if (atend) {
				(*scores_p)++;
				/* also set right margin if user requested */
				if (must_set_right_margin) {
					if (*scores_p == 1) {
						/*
						 * One score: move the margin in
						 * by "remwidth", so this score
						 * will be laid out as it is
						 * now, not stretched any more.
						 */
						set_right_margin(start_p,
							remwidth);
					} else {
						/*
						 * Multiple scores: find how
						 * much (on average) the prev
						 * scores will be stretched,
						 * and stretch this last one by
						 * the same factor.
						 */
						float stretch, wanted, white;
						/* prev scores' stretch factor*/
						stretch = (used_space +
						    unused_space) / used_space;
						/* stretch the score this much*/
						wanted = (initial_space
						    - remwidth) * stretch;
						/* amount to move margin in */
						white = initial_space - wanted;
						if (white < 0.0) { /*defensive*/
							white = 0.0;
						}
						set_right_margin(start_p,white);
					}
				}
				return (prevfeed_p);
			}
		} else {
			/*
			 * We will not put this measure on this score.  Before
			 * dealing with this measure, check indentrestart.  If
			 * that is set, and the previous measure was a "restart"
			 * measure, we need to also force that measure onto the
			 * the new score.  (Although we can't do that if this
			 * is the only measure on that score, a bizarre case.)
			 * Then the room that is allocated for the restart will
			 * be used as indentation on the new score.
			 */
			if (Score.indentrestart == YES &&
					measinscore[*scores_p] > 1 &&
					prev_is_restart(mainll_p) == YES) {
				/* set things back like they were */
				restoressvstate();
				remwidth = old_remwidth;
				mainll_p = old_mainll_p;
				measinscore[*scores_p] = old_measinscore;
				countable_measures = old_countable_measures;

				/* this time force that measure onto new score*/
				force_newscore = YES;
				continue;
			}

			force_newscore = NO;

			/*
			 * Increment the number of scores needed.
			 */
			(*scores_p)++;

			/*
			 * If this last measure ended with SSV(s) after the
			 * bar line that would cause a CLEFSIG, we need to
			 * undo the change so that the new score will start
			 * with the old info.  Sadly, we'll have to init
			 * the SSVs and apply them over from the beginning.
			 */
			if (ressv) {
				setssvstate(mainll_p);
			}

			/* add to space used & not used at the end of scores */
			used_space += initial_space - remwidth;
			unused_space += remwidth;

			/*
			 * Find out how much width is available, allowing for
			 * margins and stuff to the left (labels, etc.).
			 * For now, assume this is not the last score, so no
			 * user margin override.
			 */
			remwidth = PGWIDTH - eff_rightmargin((struct MAINLL *)0)
					 - eff_leftmargin((struct MAINLL *)0);
			remwidth -= pwidth_left_of_score(mainll_p, prevfeed_p);

			prevfeed_p = mainll_p;	/* where feed would go */

			/*
			 * We need to set up a provisional CLEFSIG containing
			 * what would need to be printed at the start of this
			 * new score.  We can't put it in the real MAINLL yet,
			 * since this function is just trying a possibility,
			 * and cannot alter MAINLL for real.  In case a repeat
			 * start is going to be needed, have a bar pointer
			 * ready.  Subtract the clefsig's width from what we
			 * have left to work with.
			 */
			(void)memset((char *)&clefsig, 0, sizeof(clefsig));
			(void)memset((char *)&bar, 0, sizeof(bar));
			clefsig.bar_p = &bar;
			fillclefsig(&clefsig, mainll_p);
			remwidth -= width_clefsig(mainll_p, &clefsig) +
					CSP(&clefsig);
			initial_space = remwidth;
			measinscore[*scores_p] = 0; /* no bars here yet */
			countable_measures = 0;

			/*
			 * If the measure we just figured is too wide for the
			 * new score we are about to begin, it must be that
			 * we are just padding things too much.  (If there
			 * really is too much stuff in the measure, we'll fail
			 * later.)  So assume we'll cram in it anyway, set up
			 * 0 width remaining, and prepare for next measure.
			 * We have to reapply the SSVs we removed above, since
			 * we won't be calling trymeasure() again for that
			 * measure.
			 *
			 * If the measure fits, don't do any of this.  Just let
			 * trymeasure figure the same one over again, next
			 * time around.
			 */
			if ((!atend && measwidth > remwidth) ||
			     (atend && measwidth > remwidth - userdelta)) {
				if (ressv) {
					for (ml2_p = mainll_p; ml2_p != new_p;
							ml2_p = ml2_p->next) {
						if (ml2_p->str == S_SSV)
							asgnssv(ml2_p->u.ssv_p);
					}
				}
				remwidth = 0;
				measinscore[*scores_p] = 1;
				if (countable_measure(mainll_p)) {
					countable_measures = 1;
				}
				mainll_p = new_p;

				/* if at the end, fix no. of scores & ret */
				if (atend) {
					(*scores_p)++;
					/* set right margin if user requested */
					if (must_set_right_margin) {
						/* don't move margin inward */
						set_right_margin(start_p, 0.0);
					}
					return (prevfeed_p);
				}
			}
		}
	}
}

/*
 * Name:        endchunk()
 *
 * Abstract:    Is this MLL item near the end of a chunk?
 *
 * Returns:     YES or NO
 *
 * Description: This function, given a main linked list structure, finds out
 *		whether there is nothing left in this chunk of the MLL other
 *		than possibly SSVs/PRHEADs/LINEs/CURVEs.  The very end of a
 *		chunk is determined by the end of the MLL, or the FEED that
 *		begins the next chunk.
 */

static int
endchunk(mainll_p)

struct MAINLL *mainll_p;	/* points into the MAINLL */

{
	/* loop past any SSVs or PRHEADs */
	while (mainll_p != 0 && (mainll_p->str == S_SSV ||
				 mainll_p->str == S_PRHEAD ||
				 mainll_p->str == S_LINE ||
				 mainll_p->str == S_CURVE))
		mainll_p = mainll_p->next;

	/* if we hit the end or a FEED, we found the end of this chunk */
	if (mainll_p == 0 || mainll_p->str == S_FEED)
		return (YES);
	return (NO);
}

/*
 * Name:        prev_is_restart()
 *
 * Abstract:    Is the previous meeasure a restart pseudo-measure?
 *
 * Returns:     YES or NO
 *
 * Description: This function, given a main linked list structure, finds out
 *		whether the measure before this one is a restart measure; that
 *		is, does it end with a restart bar.
 */

static int
prev_is_restart(mainll_p)

struct MAINLL *mainll_p;	/* BAR the ends the current measure, or an */
				/*  SSV or CLEFSIG that follows that BAR */

{
	/* find bar at start of current measure, if not there already */
	while (mainll_p != 0 && mainll_p->str != S_BAR) {
		mainll_p = mainll_p->prev;
	}
	if (mainll_p == 0) {
		return NO;	/* should not happen */
	}

	/* check for restart bar type */
	if (mainll_p->u.bar_p->bartype == RESTART) {
		return (YES);
	}
	return (NO);
}

/*
 * Name:        this_is_restart()
 *
 * Abstract:    Is the this meeasure a restart pseudo-measure?
 *
 * Returns:     YES or NO
 *
 * Description: This function, given a main linked list structure, finds out
 *		whether the measure starting here is restart measure; that is,
 *		does it end with a restart bar.
 */

static int
this_is_restart(mainll_p)

struct MAINLL *mainll_p;	/* start of a measure */

{
	if (mainll_p == 0) {
		return NO;	/* should not happen */
	}

	/* find bar at end of current measure */
	mainll_p = mainll_p->next;
	while (mainll_p != 0 && mainll_p->str != S_BAR) {
		mainll_p = mainll_p->next;
	}

	/* check for restart bar type */
	if (mainll_p != 0 && mainll_p->u.bar_p->bartype == RESTART) {
		return (YES);
	}
	return (NO);
}

/*
 * Name:        countable_measure()
 *
 * Abstract:    Return YES if this measure should be counted for maxmeasures.
 *
 * Returns:     YES or NO
 *
 * Description: This function, given the first structure in a measure, decides
 *		whether this measure should be counted for purposes of the
 *		maxmeasures parameter.
 */

static int
countable_measure(mainll_p)

struct MAINLL *mainll_p;	/* start of a measure */

{
	/* find the preceding bar line, if any */
	for ( ; mainll_p != 0; mainll_p = mainll_p->prev) {

		if (mainll_p->str == S_BAR) {
			/*
			 * If invisbar, the user isn't treating our measure
			 * like a new measure, but rather a continuation of the
			 * previous measure; so don't count this measure.
			 */
			if (mainll_p->u.bar_p->bartype == INVISBAR) {
				return (NO);
			} else {
				return (YES);
			}
		}
	}

	/* this is the first measure; don't count it if it is a "pickup" */
	if (has_pickup()) {
		return (NO);
	} else {
		return (YES);
	}
}

/*
 * Name:        adjust_measwid4mrpt()
 *
 * Abstract:    If the measure contains a mrpt, adjust the measure's width.
 *
 * Returns:     the new (possibly increased) measure width
 *
 * Description: This function, given the supposed width of a measure and the
 *		first chord of the measure, looks to see if there is an mrpt
 *		in the measure.  If so, it enlarges the chord(s) in the measure
 *		if necessary, to make sure there's enough room for the symbol.
 */

static double
adjust_measwid4mrpt(oldmeaswid, ch_p)

double oldmeaswid;	/* old measure width */
struct CHORD *ch_p;	/* points at a chord, should be first chord in meas */

{
	int gotmrpt;		/* is there an mrpt? */
	struct GRPSYL *gs_p;	/* point down list of GRPSYLs in chord */
	float newmeaswid;	/* possible new measure width */
	float thismrpt;		/* space needed by one mrpt and its padding */
	float increase;		/* how much bigger must we make the measure? */


	/*
	 * Scan down the first chord and see if any groups have an mrpt.
	 */
	gotmrpt = NO;
	newmeaswid = 0.0;
	for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
		if (is_mrpt(gs_p)) {
			gotmrpt = YES;
			/* get width of this mrpt + its padding, if any */
			thismrpt = width(FONT_MUSIC, DFLT_SIZE, C_MEASRPT) *
				svpath(gs_p->staffno, STAFFSCALE)->staffscale +
				gs_p->padding;
			if (thismrpt > newmeaswid) {
				newmeaswid = thismrpt;
			}
		}
	}

	if (gotmrpt == NO)
		return (oldmeaswid);	/* no mrpt, return original width */

	/* if measure is already wider than all mrpts, return unchanged */
	if (oldmeaswid >= newmeaswid)
		return (oldmeaswid);

	/*
	 * Some staff(s) have mrpts, and the existing chord(s) add up to
	 * narrower than the width of an mrpt.  It's really rare that this
	 * could happen if there is only one chord.  So we will handle it by
	 * forcing the first chord to be wider, rather than allocating the
	 * extra amongst all the chords.
	 */
	if (ABSDIFF(ch_p->width, TEMPMRPTWIDTH) < 0.001) {
		/* first chord is all mrpt, so no other chords exist; */
		/* the 0.001 is to allow for roundoff error */
		ch_p->c[RE] = newmeaswid / 2.0;
		ch_p->c[RW] = -newmeaswid / 2.0;
		ch_p->width = newmeaswid;
	} else {
		/* add extra to the right side of the first chord */
		increase = newmeaswid - oldmeaswid;
		ch_p->c[RE] += increase;
		ch_p->width += increase;
	}

	return (newmeaswid);
}

/*
 * Name:        fillclefsig()
 *
 * Abstract:    Fill the CLEFSIG for after a FEED.
 *
 * Returns:     void
 *
 * Description: This function, given an empty CLEFSIG structure and a pointer
 *		to a FEED structure in the MAINLL, fills the CLEFSIG according
 *		to what should be printed.  If called from tryabs() (bar_p !=
 *		0), the bar will be set to REPEATSTART if need be, based on the
 *		type of the preceding bar.  If called from setabs() (bar_p ==
 *		0), the same will be done, if need be, after allocating a BAR
 *		and setting the pointer to it.  In one bizarre case, abschunk()
 *		calls here directly, and this is treated the same as tryabs();
 *		bar_p != 0.
 */

static void
fillclefsig(clefsig_p, feed_p)

struct CLEFSIG *clefsig_p;	/* points at empty clefsig to be filled in */
struct MAINLL *feed_p;		/* points at a FEED in the MAINLL */

{
	struct MAINLL *mainll_p;/* points along the MAINLL */
	struct BAR *realbar_p;	/* point at bar before this feed */
	float barpad;		/* padding on that bar */
	int s;			/* staff number */


	/*
	 * On every visible staff, the clef and key signature are to
	 * be printed.
	 */
	for (s = 1; s <= Score.staffs; s++) {
		if (svpath(s, VISIBLE)->visible == NO)
			continue;	/* invisible */
		clefsig_p->prclef[s] = YES;
		clefsig_p->sharps[s] = eff_key(s);
	}

	/* clefs to be printed should be regular size */
	clefsig_p->clefsize = DFLT_SIZE;

	/*
	 * The time signature is to be printed on the first score, or if
	 * it just changed.  Search back to see if there was a CLEFSIG just
	 * before this FEED where the time changed, or if this is the first
	 * measure.
	 */
	for (mainll_p = feed_p->prev; mainll_p != 0 &&
			mainll_p->str != S_BAR && mainll_p->str != S_CLEFSIG;
			mainll_p = mainll_p->prev)
		;

	/* see chkrestart() for explanation of the bar_p->c[AY] part of this */
	if (mainll_p == 0 ||
			(mainll_p->str == S_CLEFSIG &&
				mainll_p->u.clefsig_p->prtimesig == YES) ||
			(mainll_p->str == S_BAR &&
				mainll_p->u.bar_p->c[AY] > 0.0))
		clefsig_p->prtimesig = YES;

	/*
	 * If the preceding BAR (if any) was a REPEATSTART or REPEATBOTH, it
	 * has to be "split up".  Search back to find this bar.
	 */
	for (mainll_p = feed_p->prev; mainll_p != 0 && mainll_p->str != S_BAR;
			mainll_p = mainll_p->prev)
		;

	if (clefsig_p->bar_p != 0) {
		/*
		 * tryabs() called us.  If there is a preceding bar, and it is
		 * REPEATSTART or REPEATBOTH, it would have to be "split", and
		 * our pseudo bar must be made a REPEATSTART.  Otherwise, the
		 * the pseudo bar should be INVISBAR since nothing really
		 * should be printed there.  Since tryabs() called us, we can't
		 * tamper with the preceding (real) bar.  That's okay; the
		 * change would just make that bar slightly narrower, so
		 * things will still fit on that score.
		 */
		if (mainll_p != 0 &&
				(mainll_p->u.bar_p->bartype == REPEATSTART ||
				 mainll_p->u.bar_p->bartype == REPEATBOTH)) {
			clefsig_p->bar_p->bartype = REPEATSTART;
		} else {
			clefsig_p->bar_p->bartype = INVISBAR;
		}
	} else {
		/*
		 * setabs() called us, so we must allocate a pseudo bar and
		 * point the clefsig at it.  The same splitting rules apply as
		 * for tryabs(), except since we're now doing it for real, we
		 * have to really alter the preceding bar's bar type in those
		 * cases.  This preceding bar's AW and AX must also be
		 * adjusted, since the bar is now going to be narrower than it
		 * was before.
		 */
		CALLOC(BAR, clefsig_p->bar_p, 1);

		if (mainll_p != 0 &&
				(mainll_p->u.bar_p->bartype == REPEATSTART ||
				 mainll_p->u.bar_p->bartype == REPEATBOTH)) {

			realbar_p = mainll_p->u.bar_p;
			realbar_p->bartype = realbar_p->bartype == REPEATSTART ?
					realbar_p->precbartype : REPEATEND;
			realbar_p->c[AW] = realbar_p->c[AE] - 
					width_barline(realbar_p);
			/* to get AX, temporarily set padding to 0; find the */
			/* width of the resulting nonpadded bar, and subtract*/
			/* half of that from AE; then restore padding */
			barpad = realbar_p->padding;
			realbar_p->padding = 0;
			realbar_p->c[AX] = realbar_p->c[AE] -
					width_barline(realbar_p) / 2;
			realbar_p->padding = barpad;
			clefsig_p->bar_p->bartype = REPEATSTART;
		} else {
			clefsig_p->bar_p->bartype = INVISBAR;
		}
	}
}

/*
 * Name:        trymeasure()
 *
 * Abstract:    Find trial width of a measure.
 *
 * Returns:     Pointer to the first MAINLL structure of the next measure,
 *		or 0 if we hit the end of MAINLL.
 *
 * Description: This function, given a pointer to the first MAINLL structure
 *		in a measure (or the FEED preceding), finds and fills in the
 *		width of the measure.
 */

static struct MAINLL *
trymeasure(mainll_p, scale, measwidth_p, adjust_p, ressv_p)

struct MAINLL *mainll_p;	/* points first thing in meas, or FEED */
double scale;			/* inches per "whole" unit of time */
float *measwidth_p;		/* width of measure to be filled in */
float *adjust_p;		/* bar line adjust, to be filled in; if last
				 * meas in score, bar shouldn't be padded on
				 * right, so subtract this for measwidth */
int *ressv_p;			/* did we apply a CLEFSIG-causing SSV? */

{
	struct CHORD *ch_p;	/* point at a chord */
	struct TIMEDSSV *tssv_p;/* point along timed SSV list */
	float idealwidth;	/* the width a chord should be, based on time*/


	if (mainll_p == 0)
		pfatal("invalid mainll_p argument (0) to trymeasure");

	*ressv_p = NO;		/* assume no SSVs for now */

	/* every measure has one CHHEAD; find it */
	while (mainll_p != 0 && mainll_p->str != S_CHHEAD)
		mainll_p = mainll_p->next;
	if (mainll_p == 0)
		pfatal("missing CHHEAD near end of main linked list");

	*measwidth_p = 0;

	/*
	 * For each chord, find out how much width it "deserves",
	 * based on its pseudodur.  But if it requires more space
	 * than that, give it what it needs.  Accumulate in *measwidth_p.
	 */
	for (ch_p = mainll_p->u.chhead_p->ch_p; ch_p != 0;
				ch_p = ch_p->ch_p) {

		idealwidth = scale * ch_p->pseudodur;
		/* but a chord of all collapseable space deserves no width */
		if (ch_p->uncollapseable == NO) {
			idealwidth = 0;
		}
		*measwidth_p += MAX(idealwidth, ch_p->width);
	}

	/*
	 * Find the bar line and add in its width.
	 */
	while (mainll_p->str != S_BAR)
		mainll_p = mainll_p->next;
	*measwidth_p += width_barline(mainll_p->u.bar_p);

	/* apply any timed SSVs */
	for (tssv_p = mainll_p->u.bar_p->timedssv_p; tssv_p != 0;
			tssv_p = tssv_p->next) {
		asgnssv(&tssv_p->ssv);
		*ressv_p = YES;		/* remember we've assigned SSVs */
	}

	/* need this in case this will be the last measure in the score */
	*adjust_p = eos_bar_adjust(mainll_p->u.bar_p);

	mainll_p = mainll_p->next;	/* point after bar line */
	/* if end of MAINLL or next measure is starting up, return now */
	if (mainll_p == 0 || (mainll_p->str != S_SSV &&
			      mainll_p->str != S_CLEFSIG))
		return (mainll_p);

	/* apply any SSVs at this point */
	while (mainll_p != 0 && mainll_p->str == S_SSV) {
		asgnssv(mainll_p->u.ssv_p);
		mainll_p = mainll_p->next;
		*ressv_p = YES;		/* remember we've assigned SSVs */
	}

	if (mainll_p != 0 && mainll_p->str == S_CLEFSIG) {
		*measwidth_p += EFF_WIDCLEFSIG(mainll_p, mainll_p->u.clefsig_p)
				+ CSP(mainll_p->u.clefsig_p);
		mainll_p = mainll_p->next;
	}

	return (mainll_p);
}

/*
 * Name:        set_right_margin()
 *
 * Abstract:    Set right margin when user said "rightmargin = auto".
 *
 * Returns:     void
 *
 * Description: This function, given a chunk of the piece delimited by FEEDs
 *		and the amount of white space that should be put before the
 * 		normal margin, sets the rightmargin field to make that happen.
 *		This value will overwrite the MG_AUTO value that is currently
 *		sitting in rightmargin.
 */

static void
set_right_margin(start_p, white)

struct MAINLL *start_p;		/* FEED at start of chunk of MAINLL */
double white;			/* white space to put before normal margin */

{
	struct MAINLL *mainll_p;


	/* find the next FEED; it has the right margin we need to set */
	for (mainll_p = start_p->next; mainll_p != 0 && mainll_p->str != S_FEED;
				mainll_p = mainll_p->next) {
		;
	}
	if (mainll_p == 0) {
		pfatal("can't find FEED that contains 'rightmargin = auto'");
	}

	mainll_p->u.feed_p->rightmargin =
			Score.rightmargin + Score.scale_factor * white;
	mainll_p->u.feed_p->right_mot = MOT_ABSOLUTE;
}

/*
 * Name:        setabs()
 *
 * Abstract:    Sets horizontal absolute coordinates for one chunk.
 *
 * Returns:     void
 *
 * Description: This function, given a chunk of the piece delimited by FEEDs,
 *		and the number of measures to be put on each score, loops
 *		through the scores, inserting FEEDs between them, and calling
 *		setabsscore() to set the horizontal absolute coordinates.
 */

static void
setabs(start_p, scores, measinscore)

struct MAINLL *start_p;		/* FEED at start of chunk of MAINLL */
int scores;			/* number of scores this chunk needs */
short measinscore[];		/* number of measures in each score */

{
	struct MAINLL *mainll_p;/* points along main linked list */
	struct MAINLL *ml2_p;	/* another general pointer into MAINLL */
	int score;		/* score number, 0 to scores-1 */
	int n;			/* loop counter */


	debug(16, "setabs file=%s line=%d scores=%d", start_p->inputfile,
			start_p->inputlineno, scores);
	/* must apply all SSVs from start, to get the right clef/key/time; */
	setssvstate(start_p);

	/* point at first item in first measure of chunk (skip initial FEED) */
	mainll_p = start_p->next;

	for (score = 0; score < scores; score++) {
		/* the first score already has a FEED; insert if later score */
		if (score != 0) {
			ml2_p = newMAINLLstruct(S_FEED, 0);
			insertMAINLL(ml2_p, mainll_p->prev);
		}
		mainll_p = mainll_p->prev;	/* point at the FEED */

		/*
		 * Insert CLEFSIG following the FEED, and fill as needed.
		 * fillclefsig() will also allocate a BAR, and point the
		 * clefsig at it.  If the previous bar line was a REPEATSTART
		 * or REPEATBOTH, it will set REPEATSTART in the new pseudo
		 * BAR, and alter the preceding bar as necessary.
		 */
		ml2_p = newMAINLLstruct(S_CLEFSIG, 0);
		insertMAINLL(ml2_p, mainll_p);
		fillclefsig(ml2_p->u.clefsig_p, mainll_p);
		ml2_p->u.clefsig_p->multinum = getmultinum(ml2_p);

		/*
		 * Find end of score by searching forward the correct number
		 * of measures.  Each measure begins with a CHHEAD, and a block
		 * begins with a BLOCKHEAD, so stop at either of these.  Call
		 * a subroutine to process this score.
		 */
		ml2_p = ml2_p->next;		/* point at CHHEAD */
		for (n = 0; n < measinscore[score]; n++) {
			do {
				ml2_p = ml2_p->next;
			} while (ml2_p != 0 && ml2_p->str != S_CHHEAD &&
					       ml2_p->str != S_BLOCKHEAD);
		}
		chkrestart(mainll_p, ml2_p);
		if (hidestaffs(mainll_p, ml2_p) == YES) {
			/* if we had to force any staffs invisible, we have to
			 * reapply SSVs so that the new ones we inserted take
			 * effect */
			setssvstate(mainll_p);
		}
		setabsscore(mainll_p, ml2_p);
		mainll_p = ml2_p;
	}
}

/*
 * Name:        chkrestart()
 *
 * Abstract:    Check for restart bars and remove if necessary.
 *
 * Returns:     void
 *
 * Description: This function, given one score's worth of input, checks for
 *		restart bars.  These are used for making a gap in the score.
 *		So when they are the first or last bar in a score, they don't
 *		make sense, and should be removed.  Well, not simply removed;
 *		various things need to be done to the main linked list.
 */

static void
chkrestart(start_p, end_p)

struct MAINLL *start_p;		/* point at the initial FEED of this score */
struct MAINLL *end_p;		/* point after the last thing on this score */

{
	struct MAINLL *mainll_p;/* points along main linked list */
	struct MAINLL *m2_p;	/* another pointer along main linked list */
	int s;			/* staff number */


	/* find first bar on this score; there has to be at least one */
	for (mainll_p = start_p; mainll_p->str != S_BAR;
			mainll_p = mainll_p->next)
		;

	if (mainll_p->u.bar_p->bartype == RESTART) {
		/*
		 * The first bar on the score is a restart.  So the score would
		 * start with whitespace followed by the restart bar, which
		 * would look bad.  So make the restart into an invisbar (which
		 * eliminates the whitespace).  A little negative padding is
		 * needed to make things line up.  Clean out the beginning of
		 * score clefsig so that nothing prints there.  The former
		 * restart's clefsig will print at the start of the line as if
		 * it were the beginning of line clefsig.
		 */
		mainll_p->u.bar_p->bartype = INVISBAR;
		mainll_p->u.bar_p->padding = -0.12;

		start_p->next->u.clefsig_p->prtimesig = NO;
		for (s = 1; s <= MAXSTAFFS; s++) {
			start_p->next->u.clefsig_p->prclef[s] = NO;
			start_p->next->u.clefsig_p->sharps[s] = 0;
			/* no need to zap the naturals, already 0 */
		}

		/*
		 * If we are supposed to indent a score when a restart falls at
		 * a new score, do it by setting the left margin.   tryabs()
		 * has ensured that the restart measure got put at the start of
		 * this score, not the end of the previous score.
		 */
		if (Score.indentrestart == YES) {
			start_p->u.feed_p->leftmargin = Score.leftmargin +
					2.0 * HALF_RESTART_WIDTH;
			start_p->u.feed_p->left_mot = MOT_ABSOLUTE;
		}
	}

	/* find the last bar on this score */
	m2_p = 0;	/* keep lint happy */
	for ( ; mainll_p != end_p; mainll_p = mainll_p->next) {
		if (mainll_p->str == S_BAR)
			m2_p = mainll_p;
	}

	if (m2_p->u.bar_p->bartype == RESTART) {
		/*
		 * The last bar on the score is a restart.  So the score would
		 * end with whitespace followed by a clefsig, which would look
		 * bad.  So make the restart into an invisbar (which eliminates
		 * the whitespace) and remove the clefsig from the MLL, since
		 * we don't want to show those things at a restart.  The next
		 * score will now be like a restart.
		 */
		m2_p->u.bar_p->bartype = INVISBAR;
		m2_p->u.bar_p->padding = 0;

		mainll_p = m2_p;	/* remember bar */

		/* find the clefsig; defensive check for end of MLL */
		for ( ; m2_p != 0 && m2_p->str != S_CLEFSIG; m2_p = m2_p->next)
			;
		if (m2_p == 0)
			pfatal("the last bar in the piece is a restart");

		/*
		 * When it comes time to create the coming feed and clefsig,
		 * that clefsig's value for prtimesig depends on the one in the
		 * clefsig we are about to remove.  So as a special kluge, if
		 * that value is YES, set the bar's AY to a positive number.
		 * It will get overwritten in absvert.c.
		 */
		if (m2_p->u.clefsig_p->prtimesig == YES) {
			mainll_p->u.bar_p->c[AY] = 1.0;
		}

		m2_p->prev->next = m2_p->next;
		m2_p->next->prev = m2_p->prev;
		FREE(m2_p->u.clefsig_p);
		FREE(m2_p);
	}
}

/*
 * Name:        setabsscore()
 *
 * Abstract:    Sets horizontal absolute coordinates for one score.
 *
 * Returns:     void
 *
 * Description: This function, given one score's worth of input, decides how
 *		to space everything horizontally to look pleasing, and then
 *		sets the horizontal absolute coordinates.
 */

static void
setabsscore(start_p, end_p)

struct MAINLL *start_p;		/* point at the initial FEED of this score */
struct MAINLL *end_p;		/* point after the last thing on this score */

{
	struct MAINLL *mainll_p;/* points along main linked list */
	struct MAINLL *m2_p;	/* another pointer along main linked list */
	struct CHORD *ch_p;	/* point at a chord */
	struct CHORD *firstch_p;/* point at first chord in a measure */
	struct BAR *bar_p;	/* convenient pointer at a clefsig's bar */
	struct MAINLL *mm_p;	/* another pointer along MLL */
	struct MAINLL *lastbarmll_p; /* remember the last bar in the score */
	struct CLEFSIG *clefsig_p; /* point at a clefsig */
	struct TIMEDSSV *tssv_p;/* point along a timed SSV list */
	float prevbarae;	/* remember previous bar's AE */
	float wid;		/* temp variable, width of something */
	float eff_right;	/* effective right margin */
	float scorewidth;	/* total width allowed for the score */
	float totwidth;		/* total minimum width */
	float totwhole;		/* total equivalent whole notes of time */
	float chw;		/* total minimum width of chords */
	float notespace;	/* space for chords */
	float nnotespace;	/* space for expandable chords */
	float ntotwhole;	/* total equiv wholes for expandables */
	float inchpwhole;	/* inches each whole note should have */
	float expanded;		/* width of something after expansion */
	float absx;		/* absolute x coordinate */
	float leftx, rightx;	/* start and end positions of a measure */
	float eff;		/* effective width */
	int toowide;		/* number of chords wider than they deserve */
	int ntoowide;		/* new no. of chords wider than deserved */


	debug(32, "setabsscore file=%s line=%d",
		start_p->inputfile, start_p->inputlineno);
	firstch_p = 0;		/* keep lint happy; will be set before used */
	prevbarae = 0.0;	/* keep lint happy; will be set before used */
	lastbarmll_p = 0;	/* keep lint happy; will be set before used */

	/*
	 * Get total available width on this score.
	 */
	if (end_p == 0) {
		/* find last feed or last bar, whichever comes last */
		for (m2_p = Mainlltc_p; m2_p->str != S_FEED && 
				m2_p->str != S_BAR; m2_p = m2_p->prev)
			;
		if (m2_p->str == S_FEED) {
			/* a feed after the last bar; use it */
			eff_right = eff_rightmargin(m2_p);
		} else {
			/* no feed after the last bar */
			eff_right = eff_rightmargin((struct MAINLL *)0);
		}
	} else {
		/*
		 * end_p must be the chhead of the following measure.  Its prev
		 * may be a user FEED.  (The CLEFSIG which should be between
		 * them has not been inserted yet.)
		 */
		if (end_p->prev->str == S_FEED) {
			/* it is a feed, so use it */
			eff_right = eff_rightmargin(end_p->prev);
		} else {
			/* no feed */
			eff_right = eff_rightmargin((struct MAINLL *)0);
		}
	}

	scorewidth = PGWIDTH - eff_right - eff_leftmargin(start_p);
	scorewidth -= width_left_of_score(start_p);

	/*
	 * Accumulate the total minimum width, total pseudodur in equivalent
	 * wholes, and the total minimum width needed by chords.
	 */
	totwidth = 0;
	totwhole = 0;
	chw = 0;
	for (mainll_p = start_p; mainll_p != end_p; mainll_p = mainll_p->next){
		switch (mainll_p->str) {

		case S_SSV:
			/* assign to keep time sig accurate */
			asgnssv(mainll_p->u.ssv_p);
			break;

		case S_CHHEAD:
			/*
			 * Add in min widths & time of all chords in measure.
			 * The west part of the first chord is considered
			 * a fixed width.  Chords of collapseable spaces are
			 * also fixed width, and their time value doesn't
			 * count either.  The "effwidth" of a chord is its east
			 * part plus the west part of the next chord, if any.
			 */
			ch_p = mainll_p->u.chhead_p->ch_p;
			if (ch_p == 0) {
				break;
			}
			totwidth -= ch_p->c[RW];
			for ( ; ch_p != 0; ch_p = ch_p->ch_p) {
				totwidth += effwidth(ch_p);
				if (ch_p->uncollapseable == YES) {
					chw += effwidth(ch_p);
					totwhole += ch_p->pseudodur;
				}
			}
			break;

		case S_CLEFSIG:
			/*
			 * If this clefsig is the last thing on this score
			 * (except possibly the FEED that starts the next chunk)
			 * find the preceding bar line.  If that bar has
			 * hidechanges set, it means that we are not to print
			 * this clefsig.
			 */
			if (mainll_p->next == end_p ||
			    mainll_p->next->str == S_FEED) {
				for (m2_p = mainll_p; m2_p->str != S_BAR;
						m2_p = m2_p->prev)
					;
				if (m2_p->u.bar_p->hidechanges) {
					mainll_p->u.clefsig_p->hide = YES;
					mainll_p->u.clefsig_p->effwidth = 0.0;
				}
			}

			/* width of clef/key/time/repeatstart when needed */
			totwidth += EFF_WIDCLEFSIG(mainll_p,
					mainll_p->u.clefsig_p);
			/* pad clefsig, unless it's the last thing on score */
			if (mainll_p->next != end_p &&
			    mainll_p->next->str != S_FEED)
				totwidth += CSP(mainll_p->u.clefsig_p);
			break;

		case S_BAR:
			/* bar's width */
			totwidth += width_barline(mainll_p->u.bar_p) +
					mainll_p->u.bar_p->padding;
			/* apply any timed SSVs */
			for (tssv_p = mainll_p->u.bar_p->timedssv_p;
					tssv_p != 0; tssv_p = tssv_p->next) {
				asgnssv(&tssv_p->ssv);
			}
			lastbarmll_p = mainll_p;
			break;
		}
	}

	/*
	 * If the last bar is truly at the end of the line, it doesn't need its
	 * full width, because there is no padding after it.  But when there is
	 * a visible clefsig with time or sig there, the bar is not at the end.
	 */
	for (mm_p = lastbarmll_p; mm_p != 0; mm_p = mm_p->next) {
		if (mm_p->str == S_STAFF || mm_p->str == S_CLEFSIG) {
			break;
		}
	}
	if (mm_p == 0 || mm_p->str != S_CLEFSIG ||
				mm_p->u.clefsig_p->hide == YES) {
		/* no visible clefsig; get rid of padding */
		totwidth -= eos_bar_adjust(lastbarmll_p->u.bar_p);
	} else {
		/* If there is a clefsig, but it has hidechanges,
		 * or it has no time signature or any key signatures,
		 * it needs to be moved to the edge of the score. */
		if (mm_p->u.clefsig_p->prtimesig == NO) {
			int s;
			for (s = 1; s <= MAXSTAFFS; s++) {
				if ((mm_p->u.clefsig_p->sharps[s] != 0 ||
				     mm_p->u.clefsig_p->naturals[s] != 0) &&
				     svpath(s, VISIBLE)->visible == YES) {
					break;
				}
			}
			if (s > MAXSTAFFS) {
				totwidth -= eos_bar_adjust(
						lastbarmll_p->u.bar_p);
			}
		}
	}

	/* fail if even the minimum size for everything doesn't fit */
	if (totwidth > scorewidth) {
		if (Score.units == INCHES) {
			l_ufatal(start_p->inputfile, start_p->inputlineno,
					"too much (%f inches) to put in score",
					totwidth * Score.scale_factor);
		} else {
			l_ufatal(start_p->inputfile, start_p->inputlineno,
					"too much (%f cm) to put in score",
					totwidth * Score.scale_factor *
					CMPERINCH);
		}
	}

	/*
	 * Only chords are allowed to expand when there is extra space;
	 * other items have a fixed width.  To find how much space is
	 * available for chords, take the total screen width minus the
	 * space needed by the fixed-size things.
	 */
	notespace = scorewidth - (totwidth - chw);

	/*
	 * Some chords' "effwidths" are already wider than what they deserve
	 * based on their pseudodur.  Let them keep that minimum size.  We
	 * will consider their size as fixed and allocate the remaining
	 * space among chords that deserve more.  Remove the too-wide (and
	 * just right) chords from the totals.  This has to be done
	 * repeatedly, since after each iteration the number of inches
	 * deserved by each remaining chord shrinks.  Leave the loop when
	 * it is found that all remaining chords deserve to expand.
	 */
	ntotwhole = totwhole;	/* initially assume all may be expandable */
	nnotespace = notespace;
	ntoowide = 0;
	do {
		/*
		 * If there are no notes in this score, totwhole will already
		 * be 0 on the first loop iteration, and there is nothing that
		 * can expand.  Each measure will be very small, just the width
		 * of the bar line and its padding, and the rightmost bar line
		 * won't be at the right edge of the score.  This is usually a
		 * useless situation; but if invisbars are used, and "newscore"
		 * every measure, it provides a way to print blank music paper.
		 *
		 * inchpwhole won't ever get used, but we set it to something
		 * arbitrary in case lint cares.  Then break out of this loop.
		 */
		if (totwhole == 0.0) {
			inchpwhole = 1.0;
			break;
		}
		/*
		 * Find how much space each whole note worth of chords
		 * deserves, allocating proportionally.  Consider just the
		 * ones not known to be too big already.
		 */
		inchpwhole = nnotespace / ntotwhole;

		/* start with all chords' time and space */
		ntotwhole = totwhole;
		nnotespace = notespace;

		toowide = ntoowide;	/* remember how many last time */
		ntoowide = 0;

		/* remove from consideration ones that are too big already */
		for (mainll_p = start_p; mainll_p != end_p;
					mainll_p = mainll_p->next) {

			if (mainll_p->str == S_CHHEAD) {
				/* loop through all chords doing this */
				for (ch_p = mainll_p->u.chhead_p->ch_p;
						ch_p != 0; ch_p = ch_p->ch_p) {
					if (effwidth(ch_p) >=
					ch_p->pseudodur * inchpwhole) {
						ntotwhole -= ch_p->pseudodur;
						nnotespace -= effwidth(ch_p);
						ntoowide++;
					}
				}
			}
		}

		/*
		 * In the (rare) case where nothing is now expandable (every-
		 * thing is packed perfectly tightly), we should break out now.
		 * The "<" is defensive.
		 */
		if (ntotwhole <= 0)
			break;

	} while (ntoowide > toowide);

	/*
	 * Now inchpwhole is the number of inches that should be given to each
	 * whole note worth of chords that deserve to be wider than their
	 * minimum.  Allocate width proportionally to these chords.
	 */
	for (mainll_p = start_p; mainll_p != end_p; mainll_p = mainll_p->next){
		if (mainll_p->str == S_CHHEAD) {
			for (ch_p = mainll_p->u.chhead_p->ch_p;
					ch_p != 0; ch_p = ch_p->ch_p) {

				/* normal case (proportional) */
				expanded = ch_p->pseudodur * inchpwhole;

				/* if collapseable, set this 0 so we'll end up
				 * using effective width unexpanded */
				if (ch_p->uncollapseable == NO) {
					expanded = 0;
				}

				/* get min dist needed from our X to next's X */
				eff = effwidth(ch_p);

				/* the dist we'll really have from X to X */
				ch_p->fullwidth = MAX(eff, expanded);
			}
		}
	}

	/*
	 * Now that we know everything's width, set all absolute horizontal
	 * coordinates for this score.  The absx variable keeps track of
	 * where we are, working from left to right.  At all times, keep
	 * track of the start and end of each measure (leftx and rightx)
	 * and the first chord in it, so we can reposition measure rests.
	 */
	/* first reset SSVs to how they were at start of this score */
	setssvstate(start_p);

	start_p->u.feed_p->c[AW] = eff_leftmargin(start_p);
	start_p->u.feed_p->c[AE] = PGWIDTH - eff_right;
	absx = eff_leftmargin(start_p) + width_left_of_score(start_p);
	start_p->u.feed_p->c[AX] = absx;
	leftx = 0.0;		/* prevent useless 'used before set' warning */

	for (mainll_p = start_p; mainll_p != end_p; mainll_p = mainll_p->next) {
		switch (mainll_p->str) {
		case S_SSV:
			/* assign to keep time sig accurate */
			asgnssv(mainll_p->u.ssv_p);
			break;

		case S_CLEFSIG:
			clefsig_p = mainll_p->u.clefsig_p;

			/* this kind partly already handled by preceding bar */
			if (clefsig_p->clefsize == SMALLSIZE &&
					clefsig_p->hide == NO) {
				/* absx points at AE of barline, so add width*/
				/* of clef excluding any clef space */
				absx += width_clefsig(mainll_p, clefsig_p) -
						clefsig_p->widestclef +
						CSP(clefsig_p);
				leftx = absx;
				break;
			}

			/* "beginning of line" or "restart" clefsig */
			clefsig_p->wclefsiga = absx;
			if (clefsig_p->hide == NO) {
				absx += width_clefsig(mainll_p, clefsig_p) +
						CSP(clefsig_p);
			}
			bar_p = clefsig_p->bar_p;
			if (bar_p != 0) {
				/* clefsig has a pseudo bar in it; set coords*/
				bar_p->c[AE] = absx;
				bar_p->c[AW] = absx - width_barline(bar_p);
				bar_p->c[AX] = (bar_p->c[AW] + absx) / 2;

				/* remember the AE of this pseudobar */
				prevbarae = absx;
			}
			leftx = absx;
			break;

		case S_BAR:
			bar_p = mainll_p->u.bar_p;
			absx += bar_p->padding;

			/* apply any timed SSVs */
			for (tssv_p = bar_p->timedssv_p; tssv_p != 0;
					tssv_p = tssv_p->next) {
				asgnssv(&tssv_p->ssv);
			}

			/*
			 * If this bar is followed by a clefsig, any clefs in
			 * it must be printed before this bar.  Note that any
			 * padding will go before the clef (see above).  But
			 * the previous measure "ends" after the clefs.
			 */
			for (m2_p = mainll_p; m2_p != 0 &&
					m2_p->str != S_CLEFSIG &&
					m2_p->str != S_CHHEAD;
					m2_p = m2_p->next)
				;
			/* if clefsig that belongs to this bar line . . . */
			if (m2_p != 0 && m2_p->str == S_CLEFSIG && m2_p->u.
					clefsig_p->clefsize == SMALLSIZE &&
					m2_p->u.clefsig_p->hide == NO) {
				clefsig_p = m2_p->u.clefsig_p;

				/*
				 * Apply SSVs to get the time & clef changes
				 * that occur at this bar, if any, since we
				 * are going to print the new values of them.
				 * After the width_clefsig, restore the SSVs to
				 * the proper state at this bar line.
				 */
				for (m2_p = mainll_p; m2_p->str != S_CLEFSIG;
						m2_p = m2_p->next) {
					if (m2_p->str == S_SSV) {
						asgnssv(m2_p->u.ssv_p);
					}
				}
				wid = width_clefsig(mainll_p, clefsig_p);
				setssvstate(mainll_p);

				/* if wid > effwid, this will overlap the */
				/* widest clef by that difference */
				clefsig_p->wclefsiga = absx -
						(wid - clefsig_p->effwidth) +
						bardiff(mainll_p, end_p);

				/* point absx after any clefs in clefsig */
				absx += clefsig_p->effwidth -
						(wid - clefsig_p->widestclef);
				rightx = clefsig_p->wclefsiga;
			} else {	/* no relevant clefsig */
				rightx = absx;	/* prev measure "ends" here */
			}
			bar_p->c[AW] = absx;
			absx += width_barline(bar_p);
			bar_p->c[AE] = absx;
			bar_p->c[AX] = (bar_p->c[AW] + bar_p->c[AE]) / 2.0;
			fixfullmeas(firstch_p, (leftx + rightx) / 2.0);
			leftx = absx;	/* next measure starts here */

			/*
			 * for each staff in the measure just ended, set its AE
			 * to this bar's AW, and set AX to the midpoint now
			 * that we know both AW and AE.
			 */
			for (m2_p = mainll_p; m2_p->str != S_CHHEAD;
					m2_p = m2_p->prev) {
				if (m2_p->str == S_STAFF) {
					m2_p->u.staff_p->c[AE] = bar_p->c[AW];
					m2_p->u.staff_p->c[AX] =
						(m2_p->u.staff_p->c[AW] +
						 m2_p->u.staff_p->c[AE]) / 2.0;
				}
			}

			/* remember the AE of this bar */
			prevbarae = absx;
			break;

		case S_STAFF:
			/* as we come to each staff, set AW to prev bar's AE */
			mainll_p->u.staff_p->c[AW] = prevbarae;
			break;

		case S_CHHEAD:
			for (ch_p = mainll_p->u.chhead_p->ch_p; ch_p != 0 &&
				     ch_p->width == 0; ch_p = ch_p->ch_p)
				setabschord(ch_p, absx);
			if ((firstch_p = ch_p) == 0)
				break;
			absx -= firstch_p->c[RW];
			for (ch_p = firstch_p; ch_p != 0; ch_p = ch_p->ch_p) {
				setabschord(ch_p, absx);
				absx += ch_p->fullwidth;
			}
			break;
		}
	}

	adjustchords(start_p, end_p);
}

/*
 * Name:        adjustchords()
 *
 * Abstract:    Move some chords to achieve a better layout.
 *
 * Returns:     void
 *
 * Description: This function is called at the end of setabsscore.  Earlier,
 *		relxchord set the chord boundaries.  It allowed syllables to
 *		exceed their chord boundaries when it was sure that they would
 *		not collide with syllables in neighboring chords.  But at that
 *		time, we didn't know how tightly the chords would end up being
 *		packed.  It had to assume the worst case, that all the chords
 *		were touching.
 *
 *		Now we know how tightly packed the chords are:  setabsscore set
 *		all the coordinates.  This function tries to improve things,
 *		for a common, specific, annoying case, where long syllables
 *		stick out far to the right, and we now know we have more room
 *		than we knew in relxchord.  It moves the following chord to the
 *		left if it doesn't have any syllables.
 */

static void
adjustchords(start_p, end_p)

struct MAINLL *start_p;		/* point at the initial FEED of this score */
struct MAINLL *end_p;		/* point after the last thing on this score */

{
	struct MAINLL *mainll_p;/* points along main linked list */
	struct MAINLL *mll_p;	/* points along main linked list */
	struct CHORD *ch_p;	/* point at a chord */
	struct CHORD *pch_p;	/* point at previous chord */
	struct GRPSYL *gs_p;	/* point down list of GRPSYLs in chord */
	struct GRPSYL *g_p;	/* move left through grace groups */
	int prev_has_syls;	/* does prev chord have any syllables, YES/NO */
	int this_has_syls;	/* does this chord have any syllables, YES/NO */
	float next_x;		/* absolute X of following thing, chord or bar*/
	float effective;	/* effective AE or AW of some group */
	float ideal_x;		/* where we would like to put our chord */
	float move_left_dist;	/* how far to move our chord to the left */
	float prev_ae;		/* biggest AE of any group in prev chord */
	float this_aw;		/* smallest AW of any group in this chord */
	int n;			/* loop variable */


	/*
	 * Loop through this score, looking for chord head cells.
	 */
	for (mainll_p = start_p; mainll_p != end_p; mainll_p = mainll_p->next) {

		if (mainll_p->str != S_CHHEAD) {
			continue;
		}

		/* loop through every chord on this score */

		prev_has_syls = NO;
		this_has_syls = NO;
		pch_p = 0;
		for (ch_p = mainll_p->u.chhead_p->ch_p; ch_p != 0;
				prev_has_syls = this_has_syls,
				this_has_syls = NO,
				pch_p = ch_p,
				ch_p = ch_p->ch_p) {

			/* find whether this chord has any nonempty syllables */
			for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
				if (gs_p->grpsyl == GS_SYLLABLE &&
						gs_p->syl != 0) {
					this_has_syls = YES;
					break;
				}
			}

			/* in the following case do nothing */
			if (prev_has_syls == NO || this_has_syls == YES) {
				continue;
			}

			/*
			 * Prev chord has syls and this chord doesn't, so we
			 * may need to do something.  First, find the X of the
			 * next thing, chord or bar line.
			 */
			if (ch_p->ch_p == 0) {
				/* no more chords, use bar line */
				for (mll_p = mainll_p->next;
				     mll_p->str != S_BAR;
				     mll_p = mll_p->next) {
					;
				}
				next_x = mll_p->u.bar_p->c[AX];
			} else {
				/* use the next chord */
				next_x = ch_p->ch_p->c[AX];
			}

			/*
			 * Find the ideal X for our chord.  That is where the
			 * distances left and right of our chord are propor-
			 * tional to the pseudodurs.
			 */
			ideal_x = pch_p->c[AX] +
				pch_p->pseudodur * (next_x - pch_p->c[AX]) /
				(pch_p->pseudodur + ch_p->pseudodur);

			/* if to the right of current X, don't do anything */
			if (ideal_x >= ch_p->c[AX]) {
				continue;
			}

			move_left_dist =  ch_p->c[AX] - ideal_x;

			/*
			 * We'd like to move our chord to the left.  It's
			 * likely that we were forced to the right because of
			 * long syllables in the preceding chord.  But we don't
			 * want to move so far as to risk group collisions.
			 */
			/* find easternmost extension of prev chord's groups */
			prev_ae = pch_p->c[AX];
			for (gs_p = pch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
				if (gs_p->grpsyl == GS_GROUP &&
				    gs_p->grpcont != GC_SPACE &&
				    gs_p->is_meas == NO) {
					effective = gs_p->c[AX] +
						effeast(pch_p, gs_p);
					if (effective > prev_ae) {
						prev_ae = effective;
					}
				}
			}
			/* find westernmost extension of this chord's groups */
			this_aw = ch_p->c[AX];
			for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
				if (gs_p->grpsyl == GS_GROUP &&
				    gs_p->grpcont != GC_SPACE &&
				    gs_p->is_meas == NO) {
					effective = gs_p->c[AX] +
						effwest(mainll_p, ch_p, gs_p);
					if (effective < this_aw) {
						this_aw = effective;
					}
				}
			}
			/* if groups might collide, reduce the move dist */
			if (prev_ae > this_aw - move_left_dist) {
				move_left_dist = this_aw - prev_ae;
			}

			/* move the chord and everything in it */
			ch_p->c[AX] -= move_left_dist;
			ch_p->c[AW] -= move_left_dist;
			ch_p->c[AE] -= move_left_dist;
			for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
				if (gs_p->grpsyl == GS_SYLLABLE) {
					continue;	/* must be empty */
				}
				/* do main group and its grace groups if any */
				g_p = gs_p;	/* init to the normal group */
				do {
					g_p->c[AX] -= move_left_dist;
					g_p->c[AW] -= move_left_dist;
					g_p->c[AE] -= move_left_dist;
					for (n = 0; n < g_p->nnotes; n++) {
						g_p->notelist[n].c[AX] -=
								move_left_dist;
						g_p->notelist[n].c[AW] -=
								move_left_dist;
						g_p->notelist[n].c[AE] -=
								move_left_dist;
					}
					g_p = g_p->prev;
				} while (g_p != 0 && g_p->grpvalue == GV_ZERO);

				if (gs_p->grpcont == GC_REST) {
					gs_p->restc[AX] -= move_left_dist;
					gs_p->restc[AW] -= move_left_dist;
					gs_p->restc[AE] -= move_left_dist;
				}
			}
		}
	}
}

/*
 * Name:        setabschord()
 *
 * Abstract:    Sets horizontal absolute coordinates for everything in a chord.
 *
 * Returns:     void
 *
 * Description: This function, given a chord, and its absolute offset, sets
 *		the horizontal absolute coordinates of everything in it.
 */

static void
setabschord(ch_p, nomx)

struct CHORD *ch_p;		/* point at the chord */
double nomx;			/* nominal X coord; may shift it right a bit */

{
	struct GRPSYL *gs_p;	/* point at a group or syllable in chord */
	struct GRPSYL *g_p;	/* point at a group with notes */
	float extra;		/* width available beyond what chord needs */
	float leftspace;	/* how much space on left side of chord */
	int n;			/* loop counter */


	/*
	 * Set the CHORD's horizonal absolute coordinates.  If the chord had
	 * no room to expand (effwidth == fullwidth), there's no question
	 * where its AX has to be.  But otherwise, we want to place it close
	 * to as far left as it can go, but not jammed up against there.
	 */
	if ((extra = ch_p->fullwidth - effwidth(ch_p)) > 0) {
		leftspace = extra * Score.leftspacefact;
		if (leftspace > Score.leftspacemax * STEPSIZE) {
			leftspace = Score.leftspacemax * STEPSIZE;
		}
		nomx += leftspace;
	}

	ch_p->c[AX] = nomx;
	ch_p->c[AW] = nomx + ch_p->c[RW];
	ch_p->c[AE] = nomx + ch_p->c[RE];

	/*
	 * Loop through all GRPSYLs in this chord, setting absolute horizontal
	 * coordinates.  To avoid the aggravation of dealing with SSVs again,
	 * don't bother checking if the staffs in question are visible, just
	 * do it.  It doesn't hurt anything to increment garbage.
	 */
	for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
		/*
		 * For groups, do the group itself and all the notes in it (if
		 * any), and do the same for all preceding grace groups.
		 */
		if (gs_p->grpsyl == GS_GROUP) {
			g_p = gs_p;	/* init to the normal group */
			do {
				/* do the group itself, based off the chord */
				g_p->c[AX] = ch_p->c[AX] + g_p->c[RX];
				g_p->c[AW] = ch_p->c[AX] + g_p->c[RW];
				g_p->c[AE] = ch_p->c[AX] + g_p->c[RE];

				/* do each note, based off the group */
				for (n = 0; n < g_p->nnotes; n++) {
					g_p->notelist[n].c[AX] = g_p->c[AX] +
							g_p->notelist[n].c[RX];
					g_p->notelist[n].c[AW] = g_p->c[AX] +
							g_p->notelist[n].c[RW];
					g_p->notelist[n].c[AE] = g_p->c[AX] +
							g_p->notelist[n].c[RE];
				}
				g_p = g_p->prev;
			} while (g_p != 0 && g_p->grpvalue == GV_ZERO);

			/* for rest, horz coords are the same as its group's */
			if (gs_p->grpcont == GC_REST) {
				gs_p->restc[AX] = gs_p->c[AX];
				gs_p->restc[AW] = gs_p->c[AW];
				gs_p->restc[AE] = gs_p->c[AE];
			}
		} else {
			/* this is a syllable; just do the syllable */
			gs_p->c[AX] = ch_p->c[AX] + gs_p->c[RX];
			gs_p->c[AW] = ch_p->c[AX] + gs_p->c[RW];
			gs_p->c[AE] = ch_p->c[AX] + gs_p->c[RE];
		}
	}
}

/*
 * Name:        effwidth()
 *
 * Abstract:    Find "effective" width of a chord.
 *
 * Returns:     the width
 *
 * Description: This function returns the "effective width" of a chord.  This
 *		is the (minimum) width of its east part, plus the width of the
 *		west part of the following chord, if there is one.
 */

static double
effwidth(ch_p)

struct CHORD *ch_p;		/* point at the chord */

{
	struct CHORD *next_p;


	next_p = ch_p->ch_p;

	/*
	 * If it's the last one in the measure, return the east side of the
	 * current chord.  Otherwise, return that plus the west side of the
	 * next nonspace chord.
	 */
	if (next_p == 0)
		return (ch_p->c[RE]);
	else
		return (ch_p->c[RE] - next_p->c[RW]);
}

/*
 * Name:        bardiff()
 *
 * Abstract:    Find size difference of end of score bar vs. what it will be.
 *
 * Returns:     void
 *
 * Description: When a REPEATSTART occurs at the end of a score, it gets
 *		changed to a SINGLEBAR, and a REPEATBOTH becomes a REPEATEND
 *		(the following pseudobar getting set to REPEATSTART).  Other
 *		bartypes are left alone.  This function, given the MLL of a bar,
 *		just returns zero if the bar is not at the end of a score; but
 *		otherwise it returns the size of that bartype minus the size of
 *		what it will be replaced by.
 */

static double
bardiff(mainll_p, end_p)

struct MAINLL *mainll_p;	/* MLL for the bar line */
struct MAINLL *end_p;		/* MLL after end of the score */

{
	struct MAINLL *mll_p;	/* for searching the MLL */
	struct BAR bar;		/* phony BAR structure */
	double temp;		/* hold the width of the orginal bar */


	/*
	 * Search forward from the bar.  If we hit a CHHEAD before hitting the
	 * end of the score, then this is not the last barline in the score, so
	 * return zero.
	 */
	for (mll_p = mainll_p; mll_p != end_p; mll_p = mll_p->next) {
		if (mll_p->str == S_CHHEAD)
			return (0.0);
	}

	/* last bar in the score, so do the arithmetic */
	switch (mainll_p->u.bar_p->bartype) {
	case REPEATSTART:
		bar.bartype = REPEATSTART;
		temp = width_barline(&bar);
		bar.bartype = SINGLEBAR;
		return (temp - width_barline(&bar));

	case REPEATBOTH:
		bar.bartype = REPEATBOTH;
		temp = width_barline(&bar);
		bar.bartype = REPEATEND;
		return (temp - width_barline(&bar));
	}

	return (0.0);	/* all other types remain the same; difference = 0 */
}

/*
 * Name:        fixfullmeas()
 *
 * Abstract:    Adjust the AE of full measure symbols (mr, multirest, mrpt).
 *
 * Returns:     void
 *
 * Description: This function, given the first chord in a measure (the only
 *		one that can contain a one of these symbols), adjusts the AE
 *		coord of each GRPSYL in the chords that is one of these.  AW
 *		stays where it is, near the left bar line, except that for
 *		multirests it moves it to the right, especially for ones
 *		that are drawn with rest symbols.  For multirests and
 *		measure repeats, AX gets moved leftwards a little, to be
 *		where it would have been for a measure rest, but for measure
 *		rests, it stays where it is, not far to the right of that.
 *		For all three things, AE is put near the right bar line, the
 *		same distance from it that AW is from the left.
 *		It also set AX for the chord itself when need be.
 */

static void
fixfullmeas(ch_p, x)

struct CHORD *ch_p;		/* point at the chord */
double x;			/* absolute X coord of center of measure */

{
	struct GRPSYL *gs_p;	/* point at a group or syllable in chord */


	/* in case we have all spaces */
	if (ch_p == 0)
		return;

	debug(32, "fixfullmeas file=%s line=%d x=%f", ch_p->gs_p->inputfile,
			ch_p->gs_p->inputlineno, (float)x);

	/* loop through all GRPSYLs, resetting AE/AW for full measure symbols */
	/* and AX for the chord itself when need be */
	for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
		/* skip syllables */
		if (gs_p->grpsyl != GS_GROUP) {
			continue;
		}

		if (gs_p->is_meas == YES) {
			gs_p->c[AE] = x + (x - gs_p->c[AW]);
		} else if (gs_p->basictime < -1) {
			/* multirest; move the left end to the right a little */
			set_staffscale(gs_p->staffno);
			gs_p->c[AW] += 2.0 * Stepsize;
			/*
			 * For multirests that are drawn with rest symbols,
			 * the width may need to be reduced.  If half the
			 * multirest's width exceeds 10 stepsizes, reduce it
			 * by 0.8 of the excess.
			 */
			if (gs_p->basictime >= -8 && svpath(gs_p->staffno,
						RESTSYMMULT)->restsymmult) {
				if (x - gs_p->c[AW] > 10.0 * Stepsize) {
					gs_p->c[AW] += ((x - gs_p->c[AW]) -
						      (10.0 * Stepsize)) * 0.8;
				}
			}
			gs_p->c[AE] = x + (x - gs_p->c[AW]);
		}

		/*
		 * For multirest/mrpt, put AX where it would have been for a mr.
		 * This code may set ch_p->c[AX] multiple times (for each gs_p
		 * in this chord) but that's okay.  Due to invisible voices, it
		 * may not get hit for every gs_p.
		 */
		if (gs_p->basictime < -1 || is_mrpt(gs_p)) {
			ch_p->c[AX] = ch_p->c[AW] +
				width(FONT_MUSIC, DFLT_SIZE, C_1REST) / 2;
		}
	}
}

/*
 * Name:        restore_grpsyl_west()
 *
 * Abstract:    Restore all GRPSYLs' west coords when there was a clef there.
 *
 * Returns:     void
 *
 * Description: In fixclef() in restsyl.c, we altered the west of any GRPSYL
 *		that was associated with a midmeasure clef.  This was needed so
 *		that room would be made for the clefs.  Now that the packing
 *		part of abshorz.c is done, we can restore these coords, for the
 *		benefit of the print phrase.
 */

static void
restore_grpsyl_west()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct GRPSYL *gs_p;		/* point along a GRPSYL list */
	struct GRPSYL *gs2_p;		/* look for a grace group's main grp */
	int vidx;			/* voice index */
	float staffscale;		/* scale for a staff */
	float clefwid;			/* width of a clef */


	initstructs();

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		switch (mainll_p->str) {
		case S_SSV:
			/* keep staffscale up to date */
			asgnssv(mainll_p->u.ssv_p);
			continue;
		case S_STAFF:
			/* break out to handle staff */
			break;
		default:
			continue;
		}

		for (vidx = 0; vidx < MAXVOICES; vidx++) {
			for (gs_p = mainll_p->u.staff_p->groups_p[vidx];
					gs_p != 0; gs_p = gs_p->next) {
				/* if no clef, or piled vertically, do nothing*/
				if (gs_p->clef == NOCLEF ||
						gs_p->clef_vert == YES) {
					continue;
				}

				staffscale = svpath(gs_p->staffno,
						STAFFSCALE)->staffscale;
				clefwid = (clefwidth(gs_p->clef, YES) +
						CLEFPAD) * staffscale;
				gs_p->c[RW] += clefwid;
				gs_p->c[AW] += clefwid;

				/*
				 * If we are a grace group, look ahead to the
				 * main group and restore it too.
				 */
				if (gs_p->grpvalue == GV_ZERO) {
					for (gs2_p = gs_p; gs2_p->grpvalue ==
					GV_ZERO; gs2_p = gs2_p->next) {
						;
					}
					gs2_p->c[RW] += clefwid;
					gs2_p->c[AW] += clefwid;
				}
			}
		}
	}
}

/*
 * Name:        setipw()
 *
 * Abstract:    Set INCHPERWHOLE "coordinate" for all structures having it.
 *
 * Returns:     void
 *
 * Description: This function sets the special pseudocoord "c[INCHPERWHOLE]"
 *		for all nongrace GRPSYLs, notes, chords, and BARs.  BARs is
 *		done right here; for the others, it calls subroutines.
 */

static void
setipw()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct MAINLL *m2_p;		/* look forward for bar line */
	struct GRPSYL *gs_p;		/* point along a GRPSYL list */
	int timeden;			/* denominator of a time signature */
	int v;				/* index into voices or verses */


	debug(16, "setipw");
	initstructs();			/* clean out old SSV info */

	/*
	 * Loop through MLL, applying SSVs and processing each visible linked
	 * list of GRPSYLs.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			/* this is needed to keep time sig up to date */
			asgnssv(mainll_p->u.ssv_p);
			break;

		case S_CHHEAD:
			/* set the thing for all chords in this measure */
			setipwchord(mainll_p);
			break;

		case S_STAFF:
			/* skip this staff if it's invisible */
			if (mainll_p->u.staff_p->visible == NO)
				break;

			/* do all the voices on this staff */
			for (v = 0; v < MAXVOICES && (gs_p = mainll_p->u.
					staff_p->groups_p[v]) != 0; v++) {
				setipwgrpsyl(mainll_p, gs_p);
			}

			/* do all the verses on this staff */
			for (v = 0; v < mainll_p->u.staff_p->nsyllists; v++) {
				gs_p = mainll_p->u.staff_p->syls_p[v];
				setipwgrpsyl(mainll_p, gs_p);
			}

			break;

		case S_BAR:
			/*
			 * If this is the ending bar line of a score, ignore
			 * it.  The following measure would refer to its
			 * preceding CLEFSIG's pseudo bar instead.  So see if
			 * we hit a FEED while trying to find the next CHHEAD.
			 * While doing this, keep track of the denominator of
			 * the time signature, in case it changes at this bar.
			 */
			timeden = Score.timeden;
			for (m2_p = mainll_p; m2_p != 0 && m2_p->str != S_FEED
					&& m2_p->str != S_CHHEAD;
					m2_p = m2_p->next) {
				if (m2_p->str == S_SSV && m2_p->u.ssv_p->used[
						TIME] == YES) {
					timeden = m2_p->u.ssv_p->timeden;
				}
			}
			if (m2_p == 0 || m2_p->str == S_FEED)
				break;

			/*
			 * This is not the last bar of a score, and m2_p points
			 * at the CHHEAD of the following measure, with timeden
			 * being the denominator of the time sig.  The space
			 * between the bar ("count 0") and the first chord
			 * ("count 1") must be multiplied by the number of
			 * counts in a whole note (timeden).
			 */
			mainll_p->u.bar_p->c[INCHPERWHOLE] = timeden *
					(m2_p->u.chhead_p->ch_p->c[AX] -
					mainll_p->u.bar_p->c[AX]);
			break;

		case S_CLEFSIG:
			/*
			 * If this clefsig is not at the start of a score,
			 * ignore it.  If it is, it will contain a pseudo bar
			 * line, and we need to set that bar's coord just like
			 * for a normal bar line.
			 */
			if (mainll_p->u.clefsig_p->bar_p == 0)
				break;

			if (mainll_p->next->str != S_CHHEAD)
				pfatal("CLEFSIG with pseudo bar not followed by CHHEAD");

			mainll_p->u.clefsig_p->bar_p->c[INCHPERWHOLE] =
				Score.timeden *
				(mainll_p->next->u.chhead_p->ch_p->c[AX] -
				mainll_p->u.clefsig_p->bar_p->c[AX]);
			break;
		}		
	}
}

/*
 * Name:        setipwgrpsyl()
 *
 * Abstract:    Set INCHPERWHOLE "coordinate" for the GRPSYLs in one list.
 *
 * Returns:     void
 *
 * Description: This function sets the special pseudocoord "c[INCHPERWHOLE]"
 *		for all the nongrace GRPSYLs and notes in one voice or verse
 *		list hanging off a STAFF.
 */

static void
setipwgrpsyl(mainll_p, gs_p)

struct MAINLL *mainll_p;		/* point along main linked list */
struct GRPSYL *gs_p;			/* point along this GRPSYL list */

{
	struct MAINLL *m2_p;		/* look forward for bar line */
	struct GRPSYL *ngs_p;		/* the next nongrace GRPSYL in list */
	float inchperwhole;		/* inches per whole note */
	int n;				/* loop variable */


	debug(32, "setipwgrpsyl file=%s line=%d", gs_p->inputfile,
			gs_p->inputlineno);
	/* get first nongrace GRPSYL */
	for ( ; gs_p != 0 && gs_p->grpsyl == GS_GROUP &&
			     gs_p->grpvalue == GV_ZERO; gs_p = gs_p->next)
		;
	if (gs_p == 0)
		pfatal("nothing but grace notes in measure");

	/*
	 * Loop down the list of GRPSYLs.  gs_p always points the current
	 * (nongrace) GRPSYL, whose inches per whole we want to set.  ngs_p
	 * points at the next nongrace GRPSYL.
	 */
	for (;;) {
		/* find next nongrace GRPSYL; break if none */
		for (ngs_p = gs_p->next;
		     ngs_p != 0 && ngs_p->grpsyl == GS_GROUP &&
				   ngs_p->grpvalue == GV_ZERO;
		     ngs_p = ngs_p->next)
			;
		if (ngs_p == 0)
			break;

		/*
		 * Distance between them divided by time gives the space a
		 * a whole note theoretically would have been given.
		 */
		inchperwhole = (ngs_p->c[AX] - gs_p->c[AX]) /
				RAT2FLOAT(gs_p->fulltime);

		/* store in GRPSYL & each note (if notes) */
		gs_p->c[INCHPERWHOLE] = inchperwhole;
		if (gs_p->grpsyl == GS_GROUP && gs_p->grpcont == GC_NOTES) {
			for (n = 0; n < gs_p->nnotes; n++)
				gs_p->notelist[n].c[INCHPERWHOLE]
						= inchperwhole;
		}

		/* point current at next, for next iteration */
		gs_p = ngs_p;
	}

	/*
	 * We've hit the end of the measure.  Loop forward through the MLL
	 * until we find the bar line.
	 */
	for (m2_p = mainll_p;
	     m2_p != 0 && m2_p->str != S_BAR;
	     m2_p = m2_p->next)
		;
	if (m2_p == 0)
		pfatal("no bar at end of last measure");

	/* this time use bar line as terminating point */
	inchperwhole = (m2_p->u.bar_p->c[AX] - gs_p->c[AX]) /
			RAT2FLOAT(gs_p->fulltime);

	gs_p->c[INCHPERWHOLE] = inchperwhole;
	if (gs_p->grpsyl == GS_GROUP && gs_p->grpcont == GC_NOTES) {
		for (n = 0; n < gs_p->nnotes; n++)
			gs_p->notelist[n].c[INCHPERWHOLE] = inchperwhole;
	}
}

/*
 * Name:        setipwchord()
 *
 * Abstract:    Set INCHPERWHOLE "coordinate" for the CHORDs in one list.
 *
 * Returns:     void
 *
 * Description: This function sets the special pseudocoord "c[INCHPERWHOLE]"
 *		for all the CHORDs in the list hanging off of one CHHEAD.
 */

static void
setipwchord(mainll_p)

struct MAINLL *mainll_p;		/* point at the CHHEAD */

{
	struct MAINLL *m2_p;		/* look forward for bar line */
	struct CHORD *ch_p, *nch_p;	/* point at chords */


	debug(32, "setipwchord file=%s line=%d", mainll_p->inputfile,
				mainll_p->inputlineno);
	/*
	 * Loop down the list of CHORDs.  ch_p always points the current
	 * CHORD, whose inches per whole we want to set.  nch_p points at
	 * the next CHORD.  When nch_p is 0, ch_p is the last chord, and we
	 * get out of the loop.
	 */
	for (ch_p = mainll_p->u.chhead_p->ch_p, nch_p = ch_p->ch_p;
			nch_p != 0; ch_p = nch_p, nch_p = nch_p->ch_p) {
		/*
		 * Distance between them divided by time gives the space a
		 * a whole note theoretically would have been given.
		 */
		ch_p->c[INCHPERWHOLE] = (nch_p->c[AX] - ch_p->c[AX]) /
					RAT2FLOAT(ch_p->duration);
	}

	/*
	 * We've hit the end of the measure.  Loop forward through the MLL
	 * until we find the bar line.
	 */
	for (m2_p = mainll_p;
	     m2_p != 0 && m2_p->str != S_BAR;
	     m2_p = m2_p->next)
		;
	if (m2_p == 0)
		pfatal("no bar at end of last measure");

	/* this time use bar line as terminating point */
	ch_p->c[INCHPERWHOLE] = (m2_p->u.bar_p->c[AX] - ch_p->c[AX]) /
				RAT2FLOAT(ch_p->duration);
}

/*
 * Name:        setsubbars()
 *
 * Abstract:    Set the position of subbars.
 *
 * Returns:     void
 *
 * Description: This function sets the horizontal coords for subbars.
 */

static void
setsubbars()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct CHHEAD *chhead_p;	/* the last chord headcell we saw */
	struct CHORD *ch_p;		/* a chord attached to chhead_p */
	struct GRPSYL *gs_p;		/* attached to chord */
	struct GRPSYL *gs2_p;		/* group or first grace group */
	struct BAR *bar_p;		/* bar line before current measure */
	float chord_count;		/* time of chord */
	short subbar_ok[MAXSTAFFS + 1];	/* YES or NO, index by staff */
	float subbar_ax;		/* AX coord of a subbar */
	int idx;


	initstructs();
	chhead_p = 0;	/* useless, but compiler warns otherwise */

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			asgnssv(mainll_p->u.ssv_p);
			continue;
		case S_CHHEAD:
			/* remember this, used when we get to the next bar */
			chhead_p = mainll_p->u.chhead_p;
			continue;
		case S_BAR:
			bar_p = mainll_p->u.bar_p;
			if (Score.nsubbar == 0) {
				continue;	/* no subbars in this measure */
			}
			if (chhead_p == 0) {
				pfatal("no chhead for measure in setsubbars()");
			}
			break;
		default:
			continue;
		}

		/* alloc array for this bar line */
		MALLOCA(struct SUBBAR_LOC, bar_p->subbar_loc, Score.nsubbar);

		/*
		 * Loop through each subbar, setting the correct X coordinate
		 * for it, if it is to be printed on any staff.  Also mark
		 * which staffs it should be printed on.
		 */
		for (idx = 0; idx < Score.nsubbar; idx++) {

			/* alloc print flag per staff, init to NO */
			CALLOCA(short, bar_p->subbar_loc[idx].pr_subbars,
					Score.staffs + 1);


			/* try to find a chord that starts at subbar's time */
			for (ch_p = chhead_p->ch_p; ch_p != 0;
					ch_p = ch_p->ch_p) {

				chord_count = RAT2FLOAT(ch_p->starttime)
						* Score.timeden + 1.0;

				/* allow fudge factor for roundoff error */
				if (fabs(chord_count - Score.subbarlist[idx].
						count) < SUBBARFUDGE) {
					break;	/* times essentially equal */
				}
			}

			if (ch_p == 0) {
				/* no chord at this time value, forget subbar */				continue;
			}

			subbar_ax = -1.0; 	/* invalid value */

			/*
			 * We found a chord starting at the same time as this
			 * subbar.  Find out which staffs could allow a subbar
			 * to be printed at that time.
			 */
			allow_subbar(ch_p, Score.subbarlist[idx].appearance_p,
					subbar_ok);

			for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {

				if (subbar_ok[gs_p->staffno] == NO) {
					continue;
				}

				bar_p->subbar_loc[idx].pr_subbars
					[gs_p->staffno] = YES;

				for (gs2_p = gs_p; gs2_p->prev != 0 &&
				     gs2_p->prev->grpvalue == GV_ZERO;
				     gs2_p = gs2_p->prev) {
					;
				}

				/*
				 * Set if not already set.  The AW of all these
				 * groups is the same, since we forced all
				 * their RW to be the same in room4subbars().
				 */
				if (subbar_ax < 0.0) {
					subbar_ax = gs2_p->c[AW] +
					width_subbar(Score.subbarlist[idx].
						appearance_p) / 2.0;
				}
			}

			bar_p->subbar_loc[idx].ax = subbar_ax;
		}
	}
}

/*
 * Name:        fixendings()
 *
 * Abstract:    Fix endings at end of score and in pseudobars.
 *
 * Returns:     void
 *
 * Description: This function finds endings that start at the final bar of a
 *		score.  It moves them so that they will start at the pseudobar
 *		at the start of the next score.  Then, wherever an ending is
 *		continuing through a scorefeed, set the pseudobar's endingloc.
 */

static void
fixendings()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct MAINLL *m2_p;		/* look forward for bar line */
	struct BAR *bar_p;		/* point at preceding bar */
	char *str_p;			/* point at an ending string */

	
	debug(16, "fixendings");
	/*
	 * Loop through the main linked list, looking for endings that start at
	 * the end of a score, and moving them.  We do it in reverse, to make
	 * it slightly easier to deal with the case of scores that have only
	 * one measure on them.  (Previous endings won't have been moved yet.)
	 */
	for (mainll_p = Mainlltc_p; mainll_p != 0; mainll_p = mainll_p->prev) {
		if (mainll_p->str != S_BAR)
			continue;
		if (mainll_p->u.bar_p->endingloc != STARTITEM)
			continue;

		/*
		 * We are at a bar where an ending starts.  Find out if this is
		 * at the end of a score, by seeing if we find a FEED before
		 * the next bar.
		 */
		for (m2_p = mainll_p->next; m2_p != 0 && m2_p->str != S_BAR &&
				m2_p->str != S_FEED; m2_p = m2_p->next)
			;
		if (m2_p == 0)
			pfatal("unterminated ending");
		if (m2_p->str == S_BAR)
			continue;

		/*
		 * The ending starts at the last bar of a score.  We need to
		 * know whether a previous ending also ends there, or not.  So
		 * search back to the previous bar.  Since we're doing the main
		 * loop in reverse, we don't have to look at pseudobars, only
		 * real ones.
		 */
		for (m2_p = mainll_p->prev; m2_p != 0 && m2_p->str != S_BAR;
				m2_p = m2_p->prev)
			;

		/*
		 * If the previous bar was the end of an ending or not involved
		 * in one at all, the bar at the end of the score should not be
		 * involved.  Otherwise, there was a preceding ending which
		 * ends here (where the new one starts), so mark that it ends.
		 */
		if (m2_p == 0 || m2_p->u.bar_p->endingloc == ENDITEM ||
				 m2_p->u.bar_p->endingloc == NOITEM)

			mainll_p->u.bar_p->endingloc = NOITEM;
		else
			mainll_p->u.bar_p->endingloc = ENDITEM;

		str_p = mainll_p->u.bar_p->endinglabel;
		mainll_p->u.bar_p->endinglabel = 0;

		/*
		 * Find the first feed after this bar that is not at the start
		 * of a "block", and mark in the following pseudobar that an
		 * ending starts there.
		 */
		for (m2_p = mainll_p->next; m2_p != 0 && (m2_p->str != S_FEED ||
			  (m2_p->next != 0 && m2_p->next->str == S_BLOCKHEAD));
			   m2_p = m2_p->next)
			;
		if (m2_p == 0) {
			pfatal("can't find any music after ending begins");
		}
		m2_p->next->u.clefsig_p->bar_p->endingloc = STARTITEM;
		m2_p->next->u.clefsig_p->bar_p->endinglabel = str_p;
	}

	/*
	 * Loop again through the main linked list, this time forwards.
	 * Remember each bar as we find one.  Then, adjust the following
	 * pseudobar if need be.
	 */
	bar_p = 0;		/* no previous bar yet */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		if (mainll_p->str == S_BAR)
			bar_p = mainll_p->u.bar_p;
		if (mainll_p->str == S_CLEFSIG &&
					mainll_p->u.clefsig_p->bar_p != 0) {
			/*
			 * We're at a pseudobar.  If the preceding bar was
			 * inside an ending, mark the pseudobar that way too.
			 * (If this is the first pseudobar, there won't have
			 * been any preceding bar.)
			 */
			if (bar_p != 0 && bar_p->endingloc == INITEM)
				mainll_p->u.clefsig_p->bar_p->endingloc
						= INITEM;
		}
	}
}

/*
 * Name:        fixreh()
 *
 * Abstract:    Move rehearsal marks at end of a score to the next score.
 *
 * Returns:     void
 *
 * Description: This function finds rehearsal marks at the final bar of a
 *		score.  It moves them so that they will be at the pseudobar
 *		at the start of the next score.
 */

static void
fixreh()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct MAINLL *m2_p;		/* look forward for bar line */

	
	debug(16, "fixreh");
	/*
	 * Loop through the main linked list, looking for rehearsal marks at
	 * the end of a score, and moving them.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		if (mainll_p->str != S_BAR)
			continue;
		if (mainll_p->u.bar_p->reh_type == REH_NONE)
			continue;

		/*
		 * We are at a bar with a rehearsal mark.  Find out if this is
		 * at the end of a score, by seeing if we find a FEED before
		 * the next bar.
		 */
		for (m2_p = mainll_p->next; m2_p != 0 && m2_p->str != S_BAR &&
				m2_p->str != S_FEED; m2_p = m2_p->next)
			;
		if (m2_p == 0)
			return;	/* nothing more we can do in this case */
		if (m2_p->str == S_BAR)
			continue;

		/*
		 * The ending starts at the last bar of a score.  m2_p is at
		 * the FEED there, but what follows could be either music or a
		 * "block".  If it is a block, we need to keep moving forward
		 * until we find a FEED followed by music.
		 */
		while (m2_p != 0 && ! IS_CLEFSIG_FEED(m2_p)) {
			m2_p = m2_p->next;
		}
		if (m2_p == 0) {
			return;	/* there is no more music, can't move reh */
		}

		/*
		 * We found the FEED.  Move the rehearsal mark to the pseudo
		 * bar after the FEED.
		 */
		m2_p->next->u.clefsig_p->bar_p->reh_type =
				mainll_p->u.bar_p->reh_type;
		mainll_p->u.bar_p->reh_type = REH_NONE;

		m2_p->next->u.clefsig_p->bar_p->reh_string =
				mainll_p->u.bar_p->reh_string;
		mainll_p->u.bar_p->reh_string = 0;

		m2_p->next->u.clefsig_p->bar_p->dist =
				mainll_p->u.bar_p->dist;
		mainll_p->u.bar_p->dist = 0;

		m2_p->next->u.clefsig_p->bar_p->dist_usage =
				mainll_p->u.bar_p->dist_usage;
		mainll_p->u.bar_p->dist_usage = SD_NONE;
	}
}

/*
 * Name:        clrinhprint()
 *
 * Abstract:    Clear the inhibitprint on tablature staffs when appropriate.
 *
 * Returns:     void
 *
 * Description: This function clears the inhibitprint bit in notes in the first
 *		group of a tablature staff after a scorefeed.  (Because in that
 *		situation, all notes should be printed regardless of the usual
 *		conditions that inhibit printing.)  Also, parentheses should be
 *		put around every note (fret number) that was inhibited in such
 *		groups.
 */

static void
clrinhprint()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct GRPSYL *gs_p;		/* point at first group */
	int sawscorefeed;		/* did we just see a scorefeed? */
	int vidx;			/* voice index */
	int n;				/* loop through the notes */

	
	debug(16, "clrinhprint");
	sawscorefeed = YES;		/* "new score" at start of song */

	/*
	 * Loop through main linked list, looking for visible tablature STAFFs,
	 * scorefeeds, and bar lines.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_FEED:
			sawscorefeed = YES;	/* just saw a feed */
			continue;
		case S_BAR:
			sawscorefeed = NO;	/* next bar, forget the feed */
			continue;
		case S_STAFF:
			/*
			 * If we just saw a scorefeed, and this is a visible
			 * tablature staff, break to handle it.  Otherwise
			 * continue to the next loop iteration.
			 */
			if (sawscorefeed == YES &&
			mainll_p->u.staff_p->visible == YES &&
			is_tab_staff(mainll_p->u.staff_p->staffno))
				break;
			continue;
		default:
			continue;
		}

		/* loop through each possible voice on tab staff */
		for (vidx = 0; vidx < MAXVOICES; vidx++) {

			/* if voice doesn't exist, break out */
			gs_p = mainll_p->u.staff_p->groups_p[vidx];
			if (gs_p == 0)
				break;

			/* if not a note group, there's nothing to do */
			if (gs_p->grpcont != GC_NOTES)
				continue;

			/*
			 * For every note that had inhibitprint set, we need to
			 * put parens around the note (fret) and clear the bit.
			 */
			for (n = 0; n < gs_p->nnotes; n++) {
				if (gs_p->notelist[n].inhibitprint == YES) {
					gs_p->notelist[n].FRET_HAS_PAREN = YES;
					gs_p->notelist[n].inhibitprint = NO;
				}
			}
		}
	}
}

/*
 * Name:        hidestaffs()
 *
 * Abstract:    Make staffs invisible if visible=whereused and they're empty.
 *
 * Returns:     YES or NO: did we force any staffs invisible?
 *
 * Description: If the user set visible=whereused for staffs, up until now we
 *		have been treating it as visible=y, because the internal field
 *		visible==YES.  But now that we know where all the scorefeeds
 *		are, this function looks at hidesilent for the given score, and
 *		when a staff should be invisible based on that, inserts SSVs
 *		and sets the visible field in the STAFF structures to make it
 *		invisible.
 */

static int
hidestaffs(mainll_p, ml2_p)

struct MAINLL *mainll_p;	/* point at feed at start of score */
struct MAINLL *ml2_p;		/* point at last thing on the score */

{
	struct SSV *ssv_p;	/* a static SSV containing visibility */
	int s;			/* staff number */
	int firstvis;		/* first visible staff in a score */
	int lastvis;		/* last visible staff in a score */
	int new_firstvis;	/* after applying hidesilent */
	int new_lastvis;	/* after applying hidesilent */
	int foundvis;		/* is a staff after first still visible? */
	int forced_invis;	/* did we force any staffs invisible? */
	int ressv;		/* must we reapply SSVs from the start? */


	debug(16, "hidestaffs");

	/* for MIDI, there is no point in hiding any staffs */
	if (Doing_MIDI) {
		return (NO);
	}

	/*
	 * Loop through main linked list, applying SSVs and looking for FEEDs.
	 * When a FEED is found, check all the staffs and make the appropriate
	 * ones invisible.
	 */
	/* find the first and last (currently) visible staffs in this score */
	firstvis = lastvis = 0;
	for (s = 1; s <= Score.staffs; s++) {
		if (svpath(s, VISIBLE)->visible == YES) {
			if (firstvis == 0) {
				firstvis = s;
			}
			lastvis = s;
		}
	}
	if (firstvis == 0) {
		pfatal("no visible staffs in score");
	}

	/*
	 * Working bottom up, check each currently visible staff to see whether
	 * it should be made invisible.  If so, make it so.  But if nothing
	 * below the first visible staff ends up visible, we leave it alone,
	 * since at least one staff must always be visible.
	 * There are cases when silent() has to apply some SSVs.  In such
	 * cases, it sets ressv=YES.  Sadly, we have to reapply SSVs from the
	 * start in that case.
	 */
	foundvis = NO;
	forced_invis = NO;
	new_firstvis = new_lastvis = 0;	/* find new first and last visible */
	for (s = Score.staffs; s >= firstvis; s--) {
		if (s == firstvis && foundvis == NO) {
			/* only the top visible staff remains visible */
			new_firstvis = new_lastvis = s;
			break;
		}
		ssv_p = svpath(s, VISIBLE);
		if (ssv_p->visible == NO) {	/* already invisible */
			continue;
		}
		if (ssv_p->hidesilent == YES) {
			if (silent(mainll_p, ml2_p, s, &ressv) == YES) {
				/* silent() forced it invisible */
				forced_invis = YES;
			} else {
				/* silent() left it visible */
				foundvis = YES;
				new_firstvis = s;
				if (new_lastvis == 0) {
					new_lastvis = s;
				}
			}
			if (ressv == YES) {
				setssvstate(mainll_p);
			}
		} else {	/* hidesilent == NO */
			foundvis = YES;	/* leave it visible */
			new_firstvis = s;
			if (new_lastvis == 0) {
				new_lastvis = s;
			}
		}
	}

	/* if no staffs got forced invisible, we're done */
	if (forced_invis == NO) {
		return (NO);
	}

	/*
	 * If the formerly top visible staff is no lnoger visible, move all
	 * of its "above all" STUFFs to the current top visible staff.
	 */
	if (new_firstvis != firstvis) {
		move_allstuff(mainll_p, ml2_p, firstvis, new_firstvis);
	}

	/*
	 * If the formerly bottom visible staff is no lnoger visible, move all
	 * of its "below all" STUFFs to the current bottom visible staff.
	 */
	if (new_lastvis != lastvis) {
		move_allstuff(mainll_p, ml2_p, lastvis, new_lastvis);
	}

	return (YES);
}

/*
 * Name:        silent()
 *
 * Abstract:    Make a staff invisible for this score, if appropriate.
 *
 * Returns:     YES if we made it invisible, else NO
 *
 * Description: This function decides whether the given staff should be made
 *		invisible on the given score (line).  It should be called only
 *		when visible==YES and hidesilent==YES.  If it should be made
 *		invisible, it does that by inserting new "input" SSVs into the
 *		MLL before and after that line, and setting the visible field
 *		in the staffs to NO.  There are cases where this function calls
 *		asgnssv(); in those cases it sets *ressv_p to YES, otherwise NO.
 */

static int
silent(feedmll_p, ml2_p, s, ressv_p)

struct MAINLL *feedmll_p;	/* point along main linked list */
struct MAINLL *ml2_p;		/* point at MLL item at end of this score */
int s;				/* staff number */
int *ressv_p;			/* must the caller reapply SSVs? */

{
	struct MAINLL *mll_p;	/* point along MLL */
	struct MAINLL *lastbar_p; /* last bar line in score */
	struct MAINLL *ins_p;	/* point at MLL after which a new SSV goes */
	struct MAINLL *new_p;	/* point at MLL struct for a new SSV */
	struct SSV *ssv_p;	/* an SSV */
	struct STAFF *staff_p;	/* point at a STAFF */
	struct STAFF *pstaff_p;	/* point at the previous STAFF */
	struct GRPSYL *gs_p;	/* point at a group or syllable */
	struct STUFF *stuff_p;	/* point along a STUFF list */
	int vidx;		/* voice or verse index */


	*ressv_p = NO;		/* no SSVs have been applied yet */

	/* find the last bar line in this score; it's where we should stop */
	lastbar_p = 0;
	for (mll_p = feedmll_p->next; mll_p != ml2_p && mll_p->str != S_FEED;
				mll_p = mll_p->next) {
		if (mll_p->str == S_BAR) {
			lastbar_p = mll_p;
		}
	}
	/* if none, there is no music here */
	if (lastbar_p == 0) {
		return (NO);	/* nothing to hide */
	}

	/*
	 * Loop through this score, checking SSVs and looking in the STAFFs for
	 * this staff number, looking for reasons we must keep the staff
	 * visible.
	 */
	for (mll_p = feedmll_p; mll_p != lastbar_p; mll_p = mll_p->next) {
		switch (mll_p->str) {
		case S_SSV:
			/*
			 * To minimize the chances that we will apply an SSV
			 * and thus have to initstructs() and reapply from the
			 * beginning, apply only if it is relevent to what we
			 * are doing.
			 */
			ssv_p = mll_p->u.ssv_p;
			if (ssv_p->context != C_SCORE && ssv_p->staffno != s) {
				/* this SSV is irrelevant to our staff */
				continue;
			}
			if (ssv_p->used[VISIBLE] != NO) {
				/*
				 * This SSV could affect our staff's visibility.
				 * Apply it, and remember that we've now messed
				 * with the fixed SSVs, and so we'll have to
				 * reapply from the start.
				 */
				asgnssv(ssv_p);
				*ressv_p = YES;
			}
			/*
			 * This staff started this score with visible==YES and
			 * hidesilent==YES.  We know we are not going to see an
			 * SSV that causes our staff to go invisible, since
			 * that would have forced a scorefeed.  But we could
			 * see one that causes our hidesilent value to be NO,
			 * and in that case we can immediately return NO, since
			 * it must remain visible.
			 */
			if (svpath(s, VISIBLE)->hidesilent == NO) {
				return (NO);
			}
			continue;

		case S_STAFF:
			staff_p = mll_p->u.staff_p;
			if (staff_p->staffno != s) {
				continue;	/* some other staff, ignore */
			}
			break;		/* break out to handle our staff */

		default:
			continue;
		}

		/* decide whether this staff can be made invisible */

		/*
		 * Look at each group in each possible voice.  If any contain
		 * notes, our staff must remain visible.
		 */
		for (vidx = 0; vidx < MAXVOICES; vidx++) {
			for (gs_p = staff_p->groups_p[vidx]; gs_p != 0;
					gs_p = gs_p->next) {
				if (gs_p->grpcont == GC_NOTES) {
					return (NO);
				}
			}
		}

		/* if there are any syllables, our staff must remain visible */
		if (staff_p->nsyllists != 0) {
			return (NO);
		}

		/*
		 * If there is any non-all stuff, our staff must remain visible.
		 * (However, MIDI stuff doesn't matter.  MIDI doesn't get here.)
		 * Stuff that is above or below "all" must not be allowed to
		 * force a staff to remain visible.  That stuff will be moved
		 * to the appropriate staff later, in hidestaffs().
		 */
		for (stuff_p = staff_p->stuff_p; stuff_p != 0;
				stuff_p = stuff_p->next) {
			if (stuff_p->all == NO) {
				return (NO);
			}
		}

		/*
		 * If the previous MLL structure is a staff, it could have
		 * lyrics or "stuff" between it and our staff.  If this
		 * previous staff is already invisible, ignore it since these
		 * things would be invisible.  But the previous staff is
		 * visible, check for any of them being "between", in which
		 * case our staff must remain visible.
		 */
		if (mll_p->prev->str == S_STAFF) {
			pstaff_p = mll_p->prev->u.staff_p;
			if (pstaff_p->visible == YES) {
				for (vidx = 0; vidx < pstaff_p->nsyllists;
							vidx++) {
					if (pstaff_p->sylplace[vidx] ==
								PL_BETWEEN) {
						return (NO);
					}
				}
				for (stuff_p = pstaff_p->stuff_p; stuff_p != 0;
						stuff_p = stuff_p->next) {
					if (stuff_p->place == PL_BETWEEN) {
						return (NO);
					}
				}
			}
		}
	}

	/*
	 * At this point we've looked through everything and found that there
	 * is no need to keep this staff visible.  So we are going to force it
	 * invisible.  If a staff's SSV says visible==NO but it has voice(s)
	 * with visible==YES, it ends up being visible anyhow.  So in addition
	 * to forcing the staff to visible=NO, we will unset all its voices'
	 * visibility.  Rather than checking how many voices there are, it's
	 * easiest just to force all possible ones invisible.
	 */

	/*
	 * Set ins_p to the SSV after which the new ones should be put.  There
	 * may be a CLEFSIG before the FEED; if so, they should be put before
	 * there, otherwise just before the FEED.  This is to maintain the
	 * correct ordering of structures; see comment at the end of structs.h.
	 * If the FEED is at the start, ins_p will be 0.
	 */
	ins_p = feedmll_p->prev;
	if (ins_p != 0 && ins_p->str == S_CLEFSIG) {
		ins_p = ins_p->prev;
	}

	/* force staff's visible to NO */
	new_p = newMAINLLstruct(S_SSV, -1);
	ssv_p = new_p->u.ssv_p;
	ssv_p->context = C_STAFF;
	ssv_p->staffno = s;
	ssv_p->used[VISIBLE] = YES;
	ssv_p->visible = NO;
	insertMAINLL(new_p, ins_p);

	/* force voices' visible to unset */
	for (vidx = 0; vidx < MAXVOICES; vidx++) {
		new_p = newMAINLLstruct(S_SSV, -1);
		ssv_p = new_p->u.ssv_p;
		ssv_p->context = C_VOICE;
		ssv_p->staffno = s;
		ssv_p->voiceno = vidx + 1;
		ssv_p->used[VISIBLE] = UNSET;
		insertMAINLL(new_p, ins_p);
	}

	/* do not let any SSVs on this line alter this staff's visibility */
	for (mll_p = feedmll_p; mll_p != lastbar_p; mll_p = mll_p->next) {
		if (mll_p->str != S_SSV) {
			continue;
		}
		ssv_p = mll_p->u.ssv_p;
		/*
		 * Since we know we are overriding the score, we don't care if
		 * the score is changing.  Just force all staff and voice SSVs
		 * for this staff to not be setting VISIBLE.
		 */
		if (ssv_p->context != C_SCORE && ssv_p->staffno == s) {
			ssv_p->used[VISIBLE] = NO;
		}
	}

	/* the SSVs to be put at the end go after the last bar line */
	ins_p = lastbar_p;

	/*
	 * Insert "input" SSVs that will cause the staff's fixed SSV and its
	 * voices' fixed SSVs to be restored to how they would have been if we
	 * hadn't changed anything.  That is the state they are in right now.
	 */
	new_p = newMAINLLstruct(S_SSV, -1);
	ssv_p = new_p->u.ssv_p;
	ssv_p->context = C_STAFF;
	ssv_p->staffno = s;
	if (Staff[s-1].used[VISIBLE] == YES) {
		ssv_p->used[VISIBLE] = YES;
		ssv_p->visible    = Staff[s-1].visible;
		ssv_p->hidesilent = Staff[s-1].hidesilent;
	} else {
		ssv_p->used[VISIBLE] = UNSET;
	}
	insertMAINLL(new_p, ins_p);

	for (vidx = 0; vidx < MAXVOICES; vidx++) {
		new_p = newMAINLLstruct(S_SSV, -1);
		ssv_p = new_p->u.ssv_p;
		ssv_p->context = C_VOICE;
		ssv_p->staffno = s;
		ssv_p->voiceno = vidx + 1;
		if (Voice[s-1][vidx].used[VISIBLE] == YES) {
			ssv_p->used[VISIBLE] = YES;
			ssv_p->visible    = Voice[s-1][vidx].visible;
			ssv_p->hidesilent = Voice[s-1][vidx].hidesilent;
		} else {
			ssv_p->used[VISIBLE] = UNSET;
		}
		insertMAINLL(new_p, ins_p);
	}

	/* set visible to NO in every staff of this number on this line */
	for (mll_p = feedmll_p; mll_p != lastbar_p; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			staff_p = mll_p->u.staff_p;
			if (staff_p->staffno == s) {
				staff_p->visible = NO;
			}
		}
	}

	return (YES);
}

/*
 * Name:        move_allstuff()
 *
 * Abstract:    Move "above/below all" STUFF to new a different STAFF.
 *
 * Returns:     void
 *
 * Description: STUFFs that are above all are attached to the top visible
 *		staff, and below all to the bottom visible staff.  When one of
 *		these is forced invisible by hidesilent, these STUFFs have to
 *		be moved to the new top or bottom visible staff.  This function
 *		is called for each of these two cases.
 */

static void
move_allstuff(mainll_p, ml2_p, olds, news)

struct MAINLL *mainll_p;	/* point at feed at start of score */
struct MAINLL *ml2_p;		/* point at last thing on the score */
int olds;			/* staff to move the stuff from */
int news;			/* staff to move the stuff to */

{
	struct MAINLL *mll_p;		/* point along MLL */
	struct STAFF *fromstaff_p;	/* move STUFFs from here */
	struct STAFF *tostaff_p;	/* move STUFFs to here */
	struct STUFF *stuff_p;		/* any STUFF */
	struct STUFF *move_p;		/* a STUFF we are moving */
	struct STUFF *prev_p;		/* the one that precedes stuff_p */


	/*
	 * Loop through this part of the score, looking for each occurrence of
	 * the "from" and "to" staffs.  Whenever you hit a bar line, move the
	 * STUFFs.
	 */
	fromstaff_p = tostaff_p = 0;
	for (mll_p = mainll_p; mll_p != ml2_p; mll_p = mll_p->next) {
		switch (mll_p->str) {
		case S_STAFF:
			if (mll_p->u.staff_p->staffno == olds) {
				fromstaff_p = mll_p->u.staff_p;
			} else if (mll_p->u.staff_p->staffno == news) {
				tostaff_p = mll_p->u.staff_p;
			}
			break;
		case S_BAR:
			if (fromstaff_p == 0 || tostaff_p == 0) {
				pfatal("no staffs in measure");
			}

			/* go down the "from" list, moving the "all" STUFFs */
			prev_p = 0;
			stuff_p = fromstaff_p->stuff_p;
			while (stuff_p != 0) {
				if (stuff_p->all == YES) {
					move_p = stuff_p;
					if (prev_p == 0) {
						fromstaff_p->stuff_p =
							move_p->next;
					} else {
						prev_p->next = move_p->next;
					}
					stuff_p = move_p->next;
					move_p->next = 0;
					connect_stuff(tostaff_p, move_p);
				} else {
					prev_p = stuff_p;
					stuff_p = stuff_p->next;
				}
			}
			fromstaff_p = tostaff_p = 0;
		}
	}
}

/*
 * Name:        getmultinum()
 *
 * Abstract:    Find number of measures in the next staff's multirest.
 *
 * Returns:     The number, or 0 if next staff is not a multirest.
 *
 * Description: This function is given an MLL struct, and if it's not a STAFF,
 *		searches forward to the next STAFF.  It returns as stated above.
 */

static int
getmultinum(mll_p)

struct MAINLL *mll_p;		/* point along MLL, starts at the CLEFSIG */

{
	int basictime;	/* of the first group in the first following staff */


	/* find the first staff after this clefsig */
	for ( ; mll_p != 0 && mll_p->str != S_STAFF; mll_p = mll_p->next) {
		;
	}

	/* if no staff, there is no multirest */
	if (mll_p == 0) {
		return (0);
	}

	basictime = mll_p->u.staff_p->groups_p[0]->basictime;
	return (basictime < -1 ? -basictime : 0);
}
