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
/*
 * Name:	relvert.c
 *
 * Description:	This file contains functions for setting all remaining
 *		relative vertical coordinates.
 */

#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"


/* this fudge factor prevents roundoff error from causing overlap */
#define FUDGE	(0.001)

/* these symbols tell certain subroutines which things to work on */
#define	DO_OTHERS	0	/* default */
#define	DO_PHRASE	1

struct PHRINFO {
	struct MAINLL *mainll_p;
	struct STUFF *stuff_p;
};

static void procstaff P((struct MAINLL *mainll_p, int s)); 
static void dostaff P((int s, int place));
static void dogroups P((struct MAINLL *start_p, int s, int place));
static void llgrps P((struct MAINLL *mll_p, struct GRPSYL *gs_p, int place));
static void dobeamalt P((struct MAINLL *start_p, int s, int place));
static void onebeamalt P((struct GRPSYL *gs_p));
static double getstemendvert P((struct GRPSYL *gs_p));
static void linerects P((double x1, double y1, double x2, double y2, int side,
		double halfstaff));
static void docurve P((struct MAINLL *start_p, int s, int place));
static void dophrase P((struct MAINLL *start_p, int s, int place));
static void curverect P((int s, struct STUFF *stuff_p, double halfstaff));
static void curvepiecerect P((double x1, double y1, double x2, double y2,
		double halfstaff));
static void dotuplet P((struct MAINLL *start_p, int s, int place));
static void onetuplet P((struct STAFF *staff_p, struct GRPSYL *start_p,
		int place));
static void domiscstuff P((struct MAINLL *start_p, int s, int place,
		unsigned long do_which));
static int do_this_pass P((int staffno, struct STUFF *stuff_p, int place,
		unsigned long do_which));
static void get_misc_info P((int s, struct STUFF *stuff_p, int place,
		unsigned long do_which, float *high_p, float *lowpart_p,
		float *dist_p));
#ifdef __STDC__
static int compaligntags P((const void *tag1_p, const void *tag2_p));
static int compstuffs P((const void *tag1_p, const void *tag2_p));
static int comp_phrases P((const void *tag1_p, const void *tag2_p));
#else
static int compaligntags P((char *tag1_p, char *tag2_p));
static int compstuffs P((char *tag1_p, char *tag2_p));
static int comp_phrases P((char *tag1_p, char *tag2_p));
#endif
static void doaligned P((struct MAINLL *start_p, int s, int place,
		unsigned long do_which, int tag));
static void dolyrics P((struct MAINLL *start_p, int s, int place));
static void getvsize P((struct MAINLL *start_p, int s, int place, int v,
		float *asc_p, float *des_p));
static void setsylvert P((struct MAINLL *start_p, int s, int place, int v,
		double baseline));
static void dopedal P((struct MAINLL *start_p, int s));
static void doendings P((struct MAINLL *start_p, int s));
static void storeend P((struct MAINLL *start_p, struct MAINLL *end_p, int s));
static void dorehears P((struct MAINLL *start_p, int s));
static double stackit P((double west, double east, double height, double dist,
		int place));
static void multistackit P((int nrect, double dist, int place));

/*
 * Name:        relvert()
 *
 * Abstract:    Set all relative vertical coords not already set.
 *
 * Returns:     void
 *
 * Description: This function sets all remaining relative vertical coords.
 *		It calls procstaff() once for each staff in each score to
 *		do this.
 */

void
relvert()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct MAINLL *end_p;		/* point at end of a piece of MLL */
	struct MAINLL *m2_p;		/* another pointer along MLL */
	int s;				/* staff number */
	int gotbar;			/* was a bar found in this chunk? */


	debug(16, "relvert");
	/*
	 * Find each section of the main linked list, delimited by FEEDs.
	 * For each such section, call procstaff() for each visible staff.
	 * Keep SSVs up to date so that we always know what staffs are visible.
	 */
	initstructs();			/* clean out old SSV info */

	/* skip anything before first FEED first */
	for (mainll_p = Mainllhc_p; mainll_p->str != S_FEED;
			mainll_p = mainll_p->next) {
		if (mainll_p->str == S_SSV)
			asgnssv(mainll_p->u.ssv_p);
	}

	/* initialize table of rectangles */
	init_rectab();

	for (;;) {
		/*
		 * Find end of this chunk.  If it has no bars in it, this must
		 * either be the end of the MLL and there was a final feed
		 * after all the music data, or else this is a block.  Either
		 * way, there is no need to process this chunk.
		 */
		gotbar = NO;
		for (end_p = mainll_p->next; end_p != 0 &&
				end_p->str != S_FEED; end_p = end_p->next) {
			if (end_p->str == S_BAR)
				gotbar = YES;
		}
		if (gotbar == NO) {
			if (end_p == 0)
				break;		/* end of MLL, get out */

			/* update SSVs to beginning of next score */
			for (m2_p = mainll_p->next; m2_p != end_p;
						m2_p = m2_p->next) {
				if (m2_p->str == S_SSV)
					asgnssv(m2_p->u.ssv_p);
			}

			mainll_p = end_p;	/* block, skip by it */
			continue;
		}

		for (s = 1; s <= Score.staffs; s++) {
			if (svpath(s, VISIBLE)->visible == YES)
				procstaff(mainll_p, s);
		}

		/* update SSVs to beginning of next score */
		for (m2_p = mainll_p->next; m2_p != end_p; m2_p = m2_p->next) {
			if (m2_p->str == S_SSV)
				asgnssv(m2_p->u.ssv_p);
		}

		if (end_p == 0)
			break;
		mainll_p = end_p;
	}

	free_rectab();
}

/*
 * Name:        procstaff()
 *
 * Abstract:    Set all relative vertical coords for a staff in one score.
 *
 * Returns:     void
 *
 * Description: This function sets all remaining relative vertical coords
 *		for a given staff of a given score.
 */

static void
procstaff(start_p, s)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* the staff we are to work on */

{
	struct MAINLL *mainll_p;/* point along main linked list */
	char *order;		/* point at a subarray in markorder */
	int stk;		/* stacking order number */
	int mk;			/* mark type */
	unsigned long do_which;	/* bit map of which mark types to do */
	float north, south;	/* relative coords of staff */
	float hb;		/* height of "between" objects */
	int k;			/* loop variable */


	debug(32, "procstaff file=%s line=%d s=%d", start_p->inputfile,
			start_p->inputlineno, s);

	/* set globals like Staffscale for use by the rest of the file */
	set_staffscale(s);

	/*
	 * Each structure in Rectab[] represents something to be drawn that
	 * is associated with this staff, beginning with the staff itself.
	 * The coordinates define the rectangle that surrounds the object.
	 * The rectangles' edges are horizontal and vertical.  So if an object
	 * (like a slanted beam) doesn't fit well in such a recangle, multiple
	 * rectangles are used to enclose pieces of it, as in integration in
	 * calculus.
	 *
	 * The first part of this function does this for things that are above
	 * the staff.  The second part does it for things that are below it.
	 * The third part does it for items that are to be centered (if
	 * possible) between two staffs.  In the first two parts, rectangles
	 * are added to the table one at a time, working outwards from the
	 * staff.  In the third part, they are piled on an imaginary baseline.
	 *
	 * Some objects (like note groups) already have an assigned position.
	 * and their rectangles are simply added to the table, regardless of
	 * whether they overlap preexisting rectangles.
 	 *
	 * Some objects (like phrase marks) get their positions figured out
	 * now, by some unique algorithm that doesn't make use of the table of
	 * rectangles, and then their rectangles are added to the table, again
	 * not worrying about overlap with preexisting rectangles.
	 *
	 * Some objects (like "stuff" to be printed) make use of the table to
	 * figure out where their rectangles should be placed.  They are placed
	 * as close to the staff (or baseline, for "between") as is possible
	 * without overlapping preexisting rectangles (or, in the case of
	 * chords, getting closer to the staff than allowed by "chorddist"; or
	 * in the case of rom, ital, bold, boldital, or rehearsal marks, closer
	 * than "dist"; or in the case of dynamics, closer than "dyndist").
	 * (And some things have their own "dist" to override these parameters,
	 * and the optional ability to force a distance regardless of overlap.)
	 * To see if the rectangle being added overlaps, first its east and
	 * west are tested.  All previous rectangles that are "out of its way"
	 * horizontally are marked not "relevant"; the others are marked
	 * "relevant".  As positions are tried, working outwards, positions
	 * that fail to avoid overlap are marked "tried".  (For chords, and
	 * rom/ital/bold/boldital, previous rectangles that are closer to the
	 * staff than the stuff is allowed to come anyhow are pre-marked as if
	 * "tried".)
	 */

	/*
	 * Fill Rectab for the objects above this staff.
	 */
	Reclim = 0;			/* Rectab is initially empty */

	dostaff(s, PL_ABOVE);
	dogroups(start_p, s, PL_ABOVE);
	dobeamalt(start_p, s, PL_ABOVE);
	docurve(start_p, s, PL_ABOVE);
	dotuplet(start_p, s, PL_ABOVE);
	dophrase(start_p, s, PL_ABOVE);

	/* get stacking order of the user-controllable mark types */
	order = svpath(s, ABOVEORDER)->markorder[PL_ABOVE];

	/* loop on each possible stacking order number */
	for (stk = 1; stk <= NUM_MARK; stk++) {

		/* set bit map for each mark type that has this order number */
		do_which = 0;
		for (mk = 0; mk < NUM_MARK; mk++) {
			if (order[mk] == stk) {
				do_which |= (1L << mk);
			}
		}
		/* if no marks, we're done; stacking orders are contiguous */
		if (do_which == 0)
			break;

		/*
		 * Some mark types must have a unique order number, not shared
		 * with any others.  For each of them, do a case statement to
		 * call their subroutine.  The other ones all share the same
		 * subroutine, so call it in the default to do the mark types
		 * listed in the bit map.
		 */
		switch (do_which) {
		case 1L << MK_LYRICS:
			dolyrics(start_p, s, PL_ABOVE);
			break;
		case 1L << MK_ENDING:
			doendings(start_p, s);
			break;
		case 1L << MK_REHEARSAL:
			dorehears(start_p, s);
			break;
		case 1L << MK_PEDAL:
			break;	/* ignore for above */
		default:
			domiscstuff(start_p, s, PL_ABOVE, do_which);
			break;
		}
	}

	/*
	 * Find the northernmost rectangle, for setting the staff's north.
	 * But don't let north be so close that things sticking out might
	 * almost touch another staff.  Staffs smaller than a regular 5 line
	 * staff will still be given as much space.  In any case, we want at
	 * least 3 stepsizes of white space.
	 */
	north = staffvertspace(s) / 2.0 + 3.0 * Stepsize;
	for (k = 0; k < Reclim; k++) {
		if (Rectab[k].n > north) {
			north = Rectab[k].n;
		}
	}

	/*
	 * Fill Rectab for the objects below this staff.
	 */
	Reclim = 0;			/* Rectab is initially empty */

	dostaff(s, PL_BELOW);
	dogroups(start_p, s, PL_BELOW);
	dobeamalt(start_p, s, PL_BELOW);
	docurve(start_p, s, PL_BELOW);
	dotuplet(start_p, s, PL_BELOW);
	dophrase(start_p, s, PL_BELOW);

	/* get stacking order of the user-controllable mark types */
	order = svpath(s, BELOWORDER)->markorder[PL_BELOW];

	/* loop on each possible stacking order number */
	for (stk = 1; stk <= NUM_MARK; stk++) {

		/* set bit map for each mark type that has this order number */
		do_which = 0;
		for (mk = 0; mk < NUM_MARK; mk++) {
			if (order[mk] == stk) {
				do_which |= (1L << mk);
			}
		}
		/* if no marks, we're done; stacking orders are contiguous */
		if (do_which == 0)
			break;

		/*
		 * Some mark types must have a unique order number, not shared
		 * with any others.  For each of them, do a case statement to
		 * call their subroutine.  The other ones all share the same
		 * subroutine, so call it in the default to do the mark types
		 * listed in the bit map.
		 */
		switch (do_which) {
		case 1L << MK_LYRICS:
			dolyrics(start_p, s, PL_BELOW);
			break;
		case 1L << MK_ENDING:
		case 1L << MK_REHEARSAL:
			break;	/* ignore for below */
		case 1L << MK_PEDAL:
			dopedal(start_p, s);
			break;
		default:
			domiscstuff(start_p, s, PL_BELOW, do_which);
			break;
		}
	}

	/*
	 * Find the southernmost rectangle, for setting the staff's south.
	 * But don't let south be so close that things sticking out might
	 * almost touch another staff.  Staffs smaller than a regular 5 line
	 * staff will still be given as much space.  In any case, we want at
	 * least 3 stepsizes of white space.
	 */
	south = -(staffvertspace(s) / 2.0 + 3.0 * Stepsize);
	for (k = 0; k < Reclim; k++) {
		if (Rectab[k].s < south) {
			south = Rectab[k].s;
		}
	}

	/*
	 * Fill Rectab for the objects between this staff and the one below.
	 */
	Reclim = 0;			/* Rectab is initially empty */

	/* set up baseline, a rectangle of height 0 spanning the page */
	Rectab[Reclim].w = 0;
	Rectab[Reclim].e = EFF_PG_WIDTH;
	Rectab[Reclim].n = 0;
	Rectab[Reclim].s = 0;
	inc_reclim();


	/* get stacking order of the user-controllable mark types */
	order = svpath(s, BETWEENORDER)->markorder[PL_BETWEEN];

	/* loop on each possible stacking order number */
	for (stk = 1; stk <= NUM_MARK; stk++) {

		/* set bit map for each mark type that has this order number */
		do_which = 0;
		for (mk = 0; mk < NUM_MARK; mk++) {
			if (order[mk] == stk) {
				do_which |= (1L << mk);
			}
		}
		/* if no marks, we're done; stacking orders are contiguous */
		if (do_which == 0)
			break;

		/*
		 * Some mark types must have a unique order number, not shared
		 * with any others.  For each of them, do a case statement to
		 * call their subroutine.  The other ones all share the same
		 * subroutine, so call it in the default to do the mark types
		 * listed in the bit map.
		 */
		switch (do_which) {
		case 1L << MK_LYRICS:
			dolyrics(start_p, s, PL_BETWEEN);
			break;
		case 1L << MK_ENDING:
		case 1L << MK_REHEARSAL:
		case 1L << MK_PEDAL:
			break;	/* ignore for between */
		default:
			domiscstuff(start_p, s, PL_BETWEEN, do_which);
			break;
		}
	}

	/*
	 * Find the northernmost rectangle, for finding the height of these
	 * objects between.
	 */
	hb = 0;
	for (k = 0; k < Reclim; k++) {
		if (Rectab[k].n > hb) {
			hb = Rectab[k].n;
		}
	}

	/*
	 * Set the relative north and south of every STAFF structure for this
	 * staff number on this score.  (There's one per measure.)  While
	 * we're at it, set RX to 0, in case anyone cares.  Set the height of
	 * "between" objects in each STAFF, too.
	 */
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno == s) {

			mainll_p->u.staff_p->c[RN] = north;
			mainll_p->u.staff_p->c[RX] = 0;
			mainll_p->u.staff_p->c[RS] = south;
			mainll_p->u.staff_p->heightbetween = hb;
		}
	}
}

/*
 * Name:        dostaff()
 *
 * Abstract:    Set up the rectangle for the staff itself.
 *
 * Returns:     void
 *
 * Description: This function puts into Rectab the rectangle for the staff
 *		itself.  The staff's relative vertical coords are not set now,
 *		though, because they must later be set to include all the
 *		objects associated with the staff.
 */

static void
dostaff(s, place)

int s;				/* staff number */
int place;			/* above or below? */

{
	debug(32, "dostaff s=%d place=%d", s, place);
	/*
	 * Use the full page width, even though the staff will not actually
	 * reach the edges, due to margins, etc.  This way nothing will ever
	 * fall beyond this base rectangle.  Put a STDPAD of padding around
	 * it vertically.
	 */
	Rectab[Reclim].w = 0;
	Rectab[Reclim].e = EFF_PG_WIDTH;

	if (place == PL_ABOVE) {
		Rectab[Reclim].n = halfstaffhi(s) + Stdpad;
		Rectab[Reclim].s = 0;
	} else {	/* PL_BELOW */
		Rectab[Reclim].n = 0;
		Rectab[Reclim].s = -(halfstaffhi(s) + Stdpad);
	}

	inc_reclim();
}

/*
 * Name:        dogroups()
 *
 * Abstract:    Set up rectangles & relative vert coords for staff's groups.
 *
 * Returns:     void
 *
 * Description: This function puts into Rectab the rectangles for each group on
 *		this staff.  The groups' relative vertical coords were already
 *		set in proclist() in beamstem.c.
 */

static void
dogroups(start_p, s, place)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above or below? */

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	int v;				/* voice number */


	debug(32, "dogroups file=%s line=%d s=%d place=%d", start_p->inputfile,
			start_p->inputlineno, s, place);

	/* save SSVs so that we can restore them after this loop */
	savessvstate();

	/*
	 * Loop through this score's part of the MLL.
	 */
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		/*
		 * Whenever we find a structure for this staff (another
		 * measure of this staff), call llgrps() for each voice.
		 * If some voice doesn't exist, llgrps() will get a
		 * null pointer and just return.
		 */
		if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno == s) {

			for (v = 0; v < MAXVOICES; v++)
				llgrps(mainll_p,
				       mainll_p->u.staff_p->groups_p[v], place);
		}
	}

	restoressvstate();
}

/*
 * Name:        llgrps()
 *
 * Abstract:    Set up rectangles for note and rest groups.
 *
 * Returns:     void
 *
 * Description: This function puts rectangles into Rectab for all groups in
 *		this measure of this voice, for groups consisting of notes or
 *		rests.
 */

static void
llgrps(mll_p, first_p, place)

struct MAINLL *mll_p;		/* point to the staff */
struct GRPSYL *first_p;		/* point to first group */
int place;			/* above or below? */

{
	struct STAFF *staff_p;		/* point to the staff */
	struct GRPSYL *gs_p;		/* point at a group */
	struct NOTE *note_p;		/* point at a note */
	double mx, my_offset, mheight, mwidth;	/* multirest number coords */
	int n;				/* loop through notelist */
	float asc, des, wid;		/* ascent, descent, and width of acc */


	staff_p = mll_p->u.staff_p;

	/*
	 * For each group that is notes or a rest, put a rectangle into Rectab.
	 * However, on tablature staffs, don't do this for rests, since they
	 * aren't printed there.
	 */
	for (gs_p = first_p; gs_p != 0; gs_p = gs_p->next) {
		if (gs_p->grpcont == GC_SPACE)
			continue;

		if (gs_p->grpcont == GC_REST && is_tab_staff(gs_p->staffno))
			continue;

		if (place == PL_ABOVE && (
			(gs_p->is_multirest && svpath(staff_p->staffno,
					PRINTMULTNUM)->printmultnum == YES) ||
			(gs_p->meas_rpt_type == MRT_SINGLE &&
					svpath(staff_p->staffno,
					NUMBERMRPT)->numbermrpt == YES) ||
			((gs_p->meas_rpt_type == MRT_DOUBLE ||
					gs_p->meas_rpt_type == MRT_QUAD) &&
					svpath(staff_p->staffno,
					NUMBERMULTRPT)->numbermultrpt == YES)
		)) {
			/*
			 * Special case for multirests and measure repeats.
			 * The rest or mrpt symbol itself is inside the staff,
			 * so we don't have to worry about it.  But we need to
			 * make a rectangle for the number, if the number is
			 * to be printed.
			 */
			if (mr_num(mll_p, &mx, &my_offset, &mheight,
					&mwidth) == 0) {
				/* dbl or quad mrpt measure that has no number*/
				continue;
			}
			Rectab[Reclim].w = mx;
			Rectab[Reclim].e = mx + mwidth;
			Rectab[Reclim].n = my_offset + mheight;
			Rectab[Reclim].s = 0;

			inc_reclim();
			continue;
		}

		/* for "below", no rectangles are needed for multirests */
		if (gs_p->is_multirest)
			continue;

		/*
		 * We have a normal note or rest group.  Make a rectangle for
		 * it, making sure it reaches the center staff line.
		 */
		Rectab[Reclim].w = gs_p->c[AW];
		Rectab[Reclim].e = gs_p->c[AE];

		if (place == PL_ABOVE) {
			Rectab[Reclim].n = MAX(gs_p->c[RN], 0);
			Rectab[Reclim].s = 0;
		} else {	/* PL_BELOW */
			Rectab[Reclim].n = 0;
			Rectab[Reclim].s = MIN(gs_p->c[RS], 0);
		}

		inc_reclim();

		/* if a clef precedes this group, make a rectangle for it */
		if (gs_p->clef != NOCLEF) {
			float north, south;	/* clef coords */

			Rectab[Reclim].e = gs_p->c[AW] - Staffscale * CLEFPAD;
			Rectab[Reclim].w = Rectab[Reclim].e - Staffscale *
				clefwidth(gs_p->clef, gs_p->staffno, YES);
			(void)clefvert(gs_p->clef, gs_p->staffno, YES,
				&north, &south);
			Rectab[Reclim].n = north * Staffscale;
			Rectab[Reclim].s = south * Staffscale;

			inc_reclim();
		}

		/*
		 * An additional rectangle is needed for each note that has an
		 * accidental.  This is because although the east/west group
		 * boundaries include any accidentals, the north/south
		 * boundaries ingore them.  It needs to be this way because,
		 * for other reasons, like ties, we want the north/south group
		 * boundaries to consider only the note heads.  But for general
		 * stuff, the accidentals should also be considered.  The
		 * rectangles added below take care of this.
		 * Similarly, if the top or bottom note is on a line and has a
		 * dot in the space away from the group, it needs a rectangle.
		 * But don't consider any of this for CSS notes.  They should
		 * not contribute to our staff's rectangles.
		 */
		if (gs_p->grpcont == GC_NOTES &&
					! is_tab_staff(gs_p->staffno)) {
			for (n = 0; n < gs_p->nnotes; n++) {

				if (IS_CSS_NOTE(gs_p, n)) {
					continue;
				}

				note_p = &gs_p->notelist[n];

				if (gs_p->dots != 0 &&
				    note_p->stepsup % 2 == 0 &&
				    ((n == 0 && note_p->ydotr > 0.0) ||
				     (n == gs_p->nnotes - 1 && note_p->ydotr < 0.0))) {
					float radius;	/* of a dot, + pad */
					radius = Stdpad + Staffscale *
						ascent(FONT_MUSIC, (note_p->
						notesize == GS_NORMAL ?
						DFLT_SIZE : SMALLSIZE), C_DOT);
					Rectab[Reclim].n = gs_p->c[RY] +
						note_p->ydotr + radius;
					Rectab[Reclim].s = gs_p->c[RY] +
						note_p->ydotr - radius;
					Rectab[Reclim].w = gs_p->c[AX] +
						gs_p->xdotr - radius;
					Rectab[Reclim].e = gs_p->c[AX] +
						gs_p->xdotr + radius +
						(gs_p->dots - 1) * 2.0 *
						(radius + Stdpad);
					inc_reclim();
				}

				if ( ! has_accs(note_p->acclist)) {
					continue;
				}

				/* this note has an acc; create a rectangle */
				accdimen(gs_p->staffno, note_p,
						&asc, &des, &wid);
				asc *= Staffscale;
				des *= Staffscale;
				wid *= Staffscale;

				Rectab[Reclim].w = gs_p->c[AX] + note_p->waccr;
				Rectab[Reclim].e = Rectab[Reclim].w + wid;
				Rectab[Reclim].n = note_p->c[RY] + asc;
				Rectab[Reclim].s = note_p->c[RY] - des;

				inc_reclim();
			}
		}
	}
}

/*
 * Name:        dobeamalt()
 *
 * Abstract:    Set up rectangles for beams and alternation bars.
 *
 * Returns:     void
 *
 * Description: This function puts into Rectab rectangles for each beam or
 *		alternation bar on this staff in this score, where the thing
 *		is on the "place" side of the notes.
 */

static void
dobeamalt(start_p, s, place)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above or below? */

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct GRPSYL *gs_p;		/* point along a GRPSYL linked list */
	int v;				/* voice number */


	debug(32, "dobeamalt file=%s line=%d s=%d place=%d", start_p->inputfile,
			start_p->inputlineno, s, place);
	/*
	 * Loop through this score's part of the MLL.
	 */
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {
		/*
		 * Whenever we find a structure for this staff (another
		 * measure of this staff), loop through its voices.
		 */
		if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno == s) {

			for (v = 0; v < MAXVOICES; v++) {
				for (gs_p = mainll_p->u.staff_p->groups_p[v];
						gs_p != 0; gs_p = gs_p->next) {
					/*
					 * Whenever we find the first group of
					 * a nongrace beamed or alted set with
					 * the stem direction on the side we
					 * are dealing with, call onebeamalt()
					 * to put rectangle(s) in Rectab.
					 * But not for cross staff beams.
					 * Grace groups are included in the
					 * following nongrace group's rectangle
					 * already.
					 */
					if (gs_p->grpcont == GC_NOTES &&
					    gs_p->grpvalue == GV_NORMAL &&
					    gs_p->beamloc == STARTITEM &&
					    gs_p->beamto == CS_SAME) {

						if ((place == PL_ABOVE &&
						    gs_p->stemdir == UP) ||
						    (place == PL_BELOW &&
						    gs_p->stemdir == DOWN))

							onebeamalt(gs_p);
					}
				}
			}
		}
	}
}

/*
 * Name:        onebeamalt()
 *
 * Abstract:    Set up rectangle(s) for one beam or alternation bar.
 *
 * Returns:     void
 *
 * Description: This function puts zero or more rectangles in Rectab for the
 *		beam or alternation that starts at the given group.  The longer
 *		and more slanted the beam/alternation is, the more rectangles
 *		will be necessary to enclose it without wasting a lot of space.
 *		If the beam/alt lies within the staff, there's no need to make
 *		any rectangles.  All rectangles' inner edges are the center
 *		staff line.
 */

static void
onebeamalt(gs_p)

struct GRPSYL *gs_p;		/* initially points to first group */

{
	float stemshift;	/* how far a stem is from its group's X */
	float x1, y1;		/* coords of left end of beam/alt */
	float x2, y2;		/* coords of right end of beam/alt */


	/*
	 * Set coords of the ends of the beam/alt.  We are given the first
	 * group, but must search forward to the end to find the last group,
	 * being careful to ignore embedded grace groups.  We adjust the X
	 * coords (for groups that can have stems) because stems are offset
	 * from their group's X.  The Y coords can't always be based on the
	 * group boundaries, because there might be "with" lists on the
	 * abnormal (beam) side, and they don't affect the position of the beam.
	 */
	x1 = gs_p->c[AX];
	y1 = getstemendvert(gs_p);

	while (gs_p != 0 && (gs_p->grpvalue == GV_ZERO ||
			     gs_p->beamloc != ENDITEM))
		gs_p = gs_p->next;
	if (gs_p == 0)
		pfatal("beam or alt group has no ENDITEM");

	x2 = gs_p->c[AX];
	y2 = getstemendvert(gs_p);

	stemshift = getstemshift(gs_p);

	/* just check the first group; if it has a stem, the others must too */
	if (HAS_STEM_ON_RIGHT(gs_p)) {
		x1 += stemshift;
		x2 += stemshift;
	} else if (HAS_STEM_ON_LEFT(gs_p)) {
		x1 -= stemshift;
		x2 -= stemshift;
	}

	/* make zero or more rectangles for this beam/alt */
	linerects(x1, y1, x2, y2, gs_p->stemdir, halfstaffhi(gs_p->staffno));
}

/*
 * Name:        getstemendvert()
 *
 * Abstract:    Find the vertical coord of the end of a stem.
 *
 * Returns:     void
 *
 * Description: This function is given a GRPSYL of a group that has either a
 *		real, visible stem, or an invisible one (alt).  If finds
 *		the relative vertical coordinate of the end of the stems
 *		farthest from the note head(s).
 */

static double
getstemendvert(gs_p)

struct GRPSYL *gs_p;	/* the group in question */

{
	double y;	/* the answer */


	if (gs_p->nwith == 0 || has_nonnormwith(gs_p) == NO) {
		/*
		 * Either there are no "with" items, or at least none on the
		 * stem end.  So we can use the group boundary.
		 */
		y = gs_p->stemdir == UP ? gs_p->c[RN] : gs_p->c[RS];
	} else {
		/*
		 * There is a "with" list at this end of the stem.  Find where
		 * the end of the stem is by applying the stem's length to the
		 * farthest note on the opposite side.
		 */
		if (gs_p->stemdir == UP)
			y = gs_p->notelist[ gs_p->nnotes - 1 ].c[RY] +
					gs_p->stemlen;
		else
			y = gs_p->notelist[ 0 ].c[RY] - gs_p->stemlen;
	}

	/* counteract the stem shortening that was done in finalstemadjust() */
	if (gs_p->beamloc != NOITEM) {
		if (gs_p->stemdir == UP) {
			y += (W_WIDE * Stdpad / 2.0);
		} else {
			y -= (W_WIDE * Stdpad / 2.0);
		}
	}

	return (y);
}

/*
 * Name:        linerects()
 *
 * Abstract:    Set up rectangle(s) to contain a (possibly) slanted line.
 *
 * Returns:     void
 *
 * Description: This function puts zero or more rectangles in Rectab to contain
 *		a (possibly) slanted line.  The longer and more slanted the
 *		line is, the more rectangles will be necessary to enclose it
 *		without wasting a lot of space.  If the line lies within the
 *		staff, there's no need to make any rectangles.  All rectangles'
 *		inner edges are the center staff line.
 */

static void
linerects(x1, y1, x2, y2, side, halfstaff)

double x1, y1;		/* coords of left end of line */
double x2, y2;		/* coords of right end of line */
int side;		/* side to favor, UP or DOWN */
double halfstaff;	/* half the staff height */

{
	float slope, yintercept;/* of a line a STDPAD beyond beam/alt */
	float deltax;		/* width of one rectangle */
	float leftx, rightx;	/* X coord of sides of a rectangle */


	/* if line is within staff, no need for any rectangles */
	if (fabs(y1) < halfstaff && fabs(y2) < halfstaff)
		return;

	/*
	 * If this beam/alt is level, make one big rectangle, and get out.
	 */
	if (y1 == y2) {
		Rectab[Reclim].w = x1;
		Rectab[Reclim].e = x2;
		if (side == UP) {
			Rectab[Reclim].n = y1;
			Rectab[Reclim].s = 0;
		} else {
			Rectab[Reclim].n = 0;
			Rectab[Reclim].s = y1;
		}
		inc_reclim();
		return;
	}

	/*
	 * We may need multiple rectangles.  Make them narrow enough so that
	 * the change in Y across the width of one is one STEPSIZE.  The
	 * rightmost one will probably be narrower, using whatever room
	 * remains.  The equation of our line is  y = slope * x + yintercept.
	 */
	slope = (y1 - y2) / (x1 - x2);
	yintercept = y1 - slope * x1;
	deltax = Stepsize / fabs(slope);

	for (leftx = x1; leftx < x2; leftx += deltax) {
		rightx = MIN(x2, leftx + deltax);
		Rectab[Reclim].w = leftx;
		Rectab[Reclim].e = rightx;
		if (side == UP) {
			Rectab[Reclim].n = slope * (slope > 0 ? rightx : leftx)
					+ yintercept;
			Rectab[Reclim].s = 0;
		} else {
			Rectab[Reclim].n = 0;
			Rectab[Reclim].s = slope * (slope > 0 ? leftx : rightx)
					+ yintercept;
		}
		inc_reclim();
	}
}

/*
 * Name:        docurve()
 *
 * Abstract:    Get point list and set up rectangles for tie/slur/bend.
 *
 * Returns:     void
 *
 * Description: This function goes through all ties, slurs, and bends for
 *		staff.  The first time it is called for a staff (which is for
 *		place "above") it calls a function to set up the curve list.
 *		Whichever time it is called, it calls a function to put
 *		rectangles in Rectab.
 */

static void
docurve(start_p, s, place)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above or below? */

{
	struct MAINLL *mainll_p;	/* loop through main linked list */
	struct STUFF *stuff_p;		/* point along a STUFF list */
	float halfstaff;		/* half the staff height */


	debug(32, "docurve file=%s line=%d s=%d place=%d",
		start_p->inputfile, start_p->inputlineno, s, place);
	halfstaff = halfstaffhi(s);

	/*
	 * Loop through this score's part of the MLL, looking for matching
	 * staffs.
	 */
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {

		if (mainll_p->str != S_STAFF ||
				mainll_p->u.staff_p->staffno != s)
			continue;

		/* loop through each stuff of the indicated type */
		for (stuff_p = mainll_p->u.staff_p->stuff_p;
				stuff_p != 0; stuff_p = stuff_p->next){

			/*
			 * When we're in here the first time (for PL_ABOVE),
			 * call a function to set up the curve list and set
			 * "place".
			 */
			if (place == PL_ABOVE) {
				switch (stuff_p->stuff_type) {
				case ST_TIESLUR:
					/* don't call tieslur_points now if the
					 * positions of the tie/slur's endpoints
					 * would change later due to CSS */
					if (css_affects_tieslurbend(stuff_p,
							mainll_p) == YES) {
						break;
					}
					tieslur_points(mainll_p, stuff_p);
					break;
				case ST_TABSLUR:
					tabslur_points(mainll_p, stuff_p);
					break;
				case ST_BEND:
					/* don't call bend_points now if the
					 * positions of the bend's endpoints
					 * would change later due to CSS */
					if (css_affects_tieslurbend(stuff_p,
							mainll_p) == YES) {
						break;
					}
					bend_points(mainll_p, stuff_p);
					break;
				/* other types are done in other functions */
				}
			}

			/*
			 * Make rectangles no matter what side of the staff the
			 * curve is supposed to be on, because, depending on
			 * how high or low the notes are, rectangles may be
			 * needed even on the opposite side you'd expect.
			 */
			if (stuff_p->crvlist_p != 0) {
				curverect(s, stuff_p, halfstaff);
			}
		}
	}
}
/*
 * Name:        dophrase()
 *
 * Abstract:    Get point list and set up rectangles for phrases.
 *
 * Returns:     void
 *
 * Description: This function goes through all phrases for the given staff.
 *		The first time it is called for a staff (which is for place
 *		"above") it adds phrases to a list.  Each time it is called it
 *		processes the list in the correct order so that nested phrases
 *		will look right.
 */

static void
dophrase(start_p, s, place)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above or below? */

{
	struct MAINLL *mainll_p;	/* loop through main linked list */
	struct STUFF *stuff_p;		/* point along a STUFF list */
	float halfstaff;		/* half the staff height */
	int n;				/* loop variable */
	/* static, to keep it set between calls to this function */
	static struct PHRINFO *phrinfo;	/* malloc array of phr info pointers */
	static int nphrinfo;		/* how many in the array */


	debug(32, "dophrase file=%s line=%d s=%d place=%d",
		start_p->inputfile, start_p->inputlineno, s, place);
	halfstaff = halfstaffhi(s);

	/*
	 * The first time in, place will be PL_ABOVE.  To make nesting of
	 * phrases work correctly, we can't just fully process each phrase as
	 * we come to it.  Here we just create a list of all phrases that start
	 * on this score, for this staff.
	 */
	if (place == PL_ABOVE) {
		/* start with empty list */
		phrinfo = 0;
		nphrinfo = 0;

		for (mainll_p = start_p->next; mainll_p != 0 && mainll_p->str
				!= S_FEED; mainll_p = mainll_p->next) {

			if (mainll_p->str != S_STAFF ||
					mainll_p->u.staff_p->staffno != s) {
				continue;
			}

			/* loop through each stuff */
			for (stuff_p = mainll_p->u.staff_p->stuff_p;
					stuff_p != 0; stuff_p = stuff_p->next) {

				if (stuff_p->stuff_type != ST_PHRASE) {
					continue;
				}

				/*
				 * Don't handle the phrase now if the positions
				 * of the phrase's endpoints would change later
				 * due to CSS.
				 */
				if (css_affects_phrase(stuff_p, mainll_p)
						== YES) {
					continue;
				}

				/* add phrase and its mainll_p to the list */
				if (nphrinfo == 0) {
					MALLOCA(struct PHRINFO, phrinfo, 1);
				} else {
					REALLOCA(struct PHRINFO, phrinfo,
							nphrinfo + 1);
				}
				phrinfo[nphrinfo].mainll_p = mainll_p;
				phrinfo[nphrinfo].stuff_p = stuff_p;
				nphrinfo++;
			}
		}

		/*
		 * If we found any phrases, sort them according to the order
		 * they need to be processed in.
		 */
		if (nphrinfo > 0) {
			qsort((char *)phrinfo, nphrinfo,
					sizeof (struct PHRINFO), comp_phrases);
		}
	}

	/* if no phrases, there is nothing to do */
	if (nphrinfo == 0) {
		return;
	}

	/* process the list from the first to done to the last */
	for (n = 0; n < nphrinfo; n++) {
		/* first time in, set up phrase points */
		if (place == PL_ABOVE) {
			phrase_points(phrinfo[n].mainll_p, phrinfo[n].stuff_p);
		}
		/* both times in, make rectangles */
		curverect(s, phrinfo[n].stuff_p, halfstaff);
	}

	/* the second time in here, release memory */
	if (place == PL_BELOW) {
		FREE(phrinfo);
	}
}

/*
 * Name:        curverect()
 *
 * Abstract:    Put rectangles in Rectab for a tie, slur, bend, or phrase.
 *
 * Returns:     void
 *
 * Description: This function puts rectangles in Rectab for a tie, slur, bend,
 *		or phrase.  Each segment of the curve gets one or more
 *		rectangles, depending on how long and how slanted it is.  To do
 *		this, we call curvepiecerect().
 */

static void
curverect(s, stuff_p, halfstaff)

int s;				/* staff number */
struct STUFF *stuff_p;		/* the curve's STUFF */
double halfstaff;		/* half the staff height */

{
	struct CRVLIST *point_p; /* point at a phrase point */
	float x1, y1;		/* coords of left end of a segment */
	float x2, y2;		/* coords of right end of a segment */
	float midx, midy;	/* middle of one segment of a curve */


	/*
	 * Loop through the curve list.  For each pair of neighboring points,
	 * there is a segment of the curve.  For items that are actually
	 * straight line segments, call curvepiecerect() once.  But for actual
	 * curves, find the midpoint, and call curvepiecerect() for each half.
	 * This way we more closely approximate the real curve.
	 */
	for (point_p = stuff_p->crvlist_p; point_p->next != 0;
			point_p = point_p->next) {

		x1 = point_p->x;
		y1 = point_p->y;
		x2 = point_p->next->x;
		y2 = point_p->next->y;

		if (stuff_p->stuff_type == ST_BEND ||
		    stuff_p->stuff_type == ST_TABSLUR) {
			/* bend, or slur on tab or tabnote */
			curvepiecerect(x1, y1, x2, y2, halfstaff);
		} else {
			/* a real curve */
			midx = (x1 + x2) / 2.0;
			midy = curve_y_at_x(stuff_p->crvlist_p, midx);
			curvepiecerect(x1, y1, midx, midy, halfstaff);
			curvepiecerect(midx, midy, x2, y2, halfstaff);
		}
	}
}

/*
 * Name:        curvepiecerect()
 *
 * Abstract:    Put rects in Rectab for a piece of a tie, slur, bend, or phrase.
 *
 * Returns:     void
 *
 * Description: This function puts rectangles in Rectab for one piece of a
 *		curve.  The piece gets one or more rectangles, depending on how
 *		long and how slanted it is.
 */

static void
curvepiecerect(x1, y1, x2, y2, halfstaff)

double x1, y1;			/* coords of left end of the piece */
double x2, y2;			/* coords of right end of the piece */
double halfstaff;		/* half the staff height */

{
	float slope, yintercept;/* of a line a segment */
	float deltax;		/* width of one rectangle */
	float leftx, rightx;	/* X coord of sides of a rectangle */


	/* if whole piece is within the staff, no rectangles are needed */
	if (fabs(y1) < halfstaff && fabs(y2) < halfstaff)
		return;

	/*
	 * If this piece is level, make 1 big rectangle, and continue.
	 */
	if (y1 == y2) {
		Rectab[Reclim].w = x1;
		Rectab[Reclim].e = x2;
		Rectab[Reclim].n = MAX(y1 + 2 * Stdpad, 0.0);
		Rectab[Reclim].s = MIN(y1 - 2 * Stdpad, 0.0);
		inc_reclim();
		return;
	}

	/*
	 * We may need multiple rectangles.  Make them narrow enough so that
	 * the change in Y across the width of one is one Stepsize.  The
	 * rightmost one will probably be narrower, using whatever room
	 * remains.  The equation of our line is
	 *	y = slope * x + yintercept
	 * Initially each rectangle only includes its segment (plus padding),
	 * but then we extend it to reach the center line of the staff.
	 */
	slope = (y1 - y2) / (x1 - x2);
	yintercept = y1 - slope * x1;
	deltax = Stepsize / fabs(slope);

	for (leftx = x1; leftx < x2; leftx += deltax) {
		rightx = MIN(x2, leftx + deltax);

		Rectab[Reclim].w = leftx;
		Rectab[Reclim].e = rightx;

	 	/*
		 * For north and south boundaries, use the side of the rect
		 * that sticks out more, to err on the side of making the rect
		 * big enough.  Also add in padding, to 1) allow for the fact
		 * that the real curve probably bulges out beyond our segment
		 * approximation, and 2) because we don't want anything
		 * actually touching the curve.
		 */
		Rectab[Reclim].n = slope * (slope > 0.0 ?  rightx : leftx) +
				yintercept + 2.0 * Stdpad;
		Rectab[Reclim].s = slope * (slope < 0.0 ?  rightx : leftx) +
				yintercept - 2.0 * Stdpad;

		/* rectangle must reach the center line of the staff */
		if (Rectab[Reclim].n < 0.0)
			Rectab[Reclim].n = 0.0;
		if (Rectab[Reclim].s > 0.0)
			Rectab[Reclim].s = 0.0;

		inc_reclim();
	}
}

/*
 * Name:        dotuplet()
 *
 * Abstract:    Set up rectangles for tuplet brackets.
 *
 * Returns:     void
 *
 * Description: This function puts into Rectab rectangles for each tuplet
 *		bracket on this staff in this score, where the thing is on
 *		the "place" side of the notes.
 */


static void
dotuplet(start_p, s, place)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above or below? */

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct GRPSYL *gs_p;		/* point along a GRPSYL linked list */
	int v;				/* voice number */


	debug(32, "dotuplet file=%s line=%d s=%d place=%d", start_p->inputfile,
			start_p->inputlineno, s, place);

	/* tuplet brackets are never printed on tablature staffs */
	if (is_tab_staff(s))
		return;

	/* save SSVs so that we can restore them after this loop */
	savessvstate();

	/*
	 * Loop through this score's part of the MLL.
	 */
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {

		/* tupdir needs to know the correct vscheme */
		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		/*
		 * Whenever we find a structure for this staff (another
		 * measure of this staff), loop through its voices.
		 */
		if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno == s) {

			for (v = 0; v < MAXVOICES; v++) {
				for (gs_p = mainll_p->u.staff_p->groups_p[v];
						gs_p != 0; gs_p = gs_p->next) {
					/*
					 * Whenever we find the first group of
					 * a tuplet with a bracket on the
					 * "place" side of the group, call
					 * onetuplet() to put rectangle(s) in
					 * Rectab.
					 */
					if ((gs_p->tuploc == STARTITEM ||
					     gs_p->tuploc == LONEITEM) &&
					    gs_p->printtup != PT_NEITHER) {

						if (tupdir(gs_p, mainll_p->u.
							staff_p) == place)

							onetuplet(mainll_p->u.
							staff_p, gs_p, place);
					}
				}
			}
		}
	}

	restoressvstate();
}

/*
 * Name:        onetuplet()
 *
 * Abstract:    Set up rectangle(s) for one tuplet bracket or number.
 *
 * Returns:     void
 *
 * Description:	If this tuplet is not going to be given a bracket (like because
 *		its notes are already beamed), this function just makes one
 *		rectangle, for the number.  Otherwise, this function puts zero
 *		or more rectangles in Rectab for the tuplet that starts at the
 *		given group.  The longer and more slanted the tuplet bracket
 *		is, the more rectangles will be necessary to enclose it without
 *		wasting a lot of space.  All rectangles' inner edges are the
 *		center staff line.
 */

static void
onetuplet(staff_p, start_p, place)

struct STAFF *staff_p;		/* point to the staff we're on */
struct GRPSYL *start_p;		/* points to first group in tuplet */
int place;			/* above or below? */

{
	struct GRPSYL *gs_p;	/* point to a group in tuplet */
	float stemshift;	/* how far a stem is from its group's X */
	float x1, y1;		/* coords of left end of beam/alt */
	float x2, y2;		/* coords of right end of beam/alt */
	float numeast, numwest;	/* horizontal coords of the tuplet number */
	float height;		/* height of the tuplet number */


	/*
	 * Set coords of the ends of the tuplet.  We are given the first
	 * group, but must search forward to the end to find the last group,
	 * being careful to ignore embedded grace groups.  We adjust the X
	 * coords because brackets reach beyond their group's X.
	 */
	x1 = start_p->c[AX];
	y1 = (place == PL_ABOVE ? start_p->c[RN] : start_p->c[RS])
			+ start_p->tupextend;

	for (gs_p = start_p; gs_p != 0 && (gs_p->grpvalue == GV_ZERO ||
			(gs_p->tuploc != ENDITEM && gs_p->tuploc != LONEITEM));
			gs_p = gs_p->next)
		;
	if (gs_p == 0)
		pfatal("tuplet has no ENDITEM");

	x2 = gs_p->c[AX];
	y2 = (place == PL_ABOVE ? gs_p->c[RN] : gs_p->c[RS]) + gs_p->tupextend;

	/*
	 * If there is not going to be a bracket, create one rectangle for the
	 * tuplet number, and return.
	 */
	if (tupgetsbrack(start_p) == NO) {
		(void)tupnumsize(start_p, &numwest, &numeast, &height, staff_p);
		Rectab[Reclim].n = (y1 + y2) / 2 + height / 2;
		Rectab[Reclim].s = (y1 + y2) / 2 - height / 2;
		Rectab[Reclim].w = numwest;
		Rectab[Reclim].e = numeast;

		inc_reclim();
		return;
	}

	/* there is going to be a bracket; extend x coords to reach to end */
	stemshift = getstemshift(gs_p);

	x1 -= stemshift;
	x2 += stemshift;

	/* make zero or more rectangles for this bracket */
	linerects(x1, y1, x2, y2, place == PL_ABOVE ? UP : DOWN,
			halfstaffhi(gs_p->staffno));
}

/*
 * Name:        domiscstuff()
 *
 * Abstract:    Set up rectangles and vert coords for miscellaneous STUFF.
 *
 * Returns:     void
 *
 * Description: This function puts into Rectab a rectangle for each STUFF
 *		structure in the "place" relationship to the given staff on
 *		this score, except for stuff types that have special,
 *		dedicated functions for their type.  It also sets their
 *		relative vertical coordinates.
 */
#define	ALIGNCHUNK	10	/* set of aligntag slots to allocate */

static void
domiscstuff(start_p, s, place, do_which)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above, below, or between? */
unsigned long do_which;		/* which stuff types are to be handled */

{
	struct MAINLL *mainll_p;	/* loop through main linked list */
	struct STUFF *stuff_p;		/* point along a STUFF list */
	float high;			/* height of a rectangle */
	float lowpart;			/* dist between stuff's Y and S */
	float dist;			/* how close chord can get to staff */
	int naligns;			/* number of aligntags encountered */
	short *alignlist;		/* list of aligntags encountered */
	int szalignlist;		/* allocated slots in alignlist */
	int n;				/* loop index */
	int found;			/* YES or NO */


	debug(32, "domiscstuff file=%s line=%d s=%d place=%d do_which=%ld",
		start_p->inputfile, start_p->inputlineno, s, place, do_which);

	/* save SSVs so that we can restore them after this loop */
	savessvstate();

	naligns = 0;			/* no aligntags seen yet */
	alignlist = 0;			/* pointer to list is null */
	szalignlist = 0;		/* no slots allocated yet */

	/*
	 * Loop through this score's part of the MLL.  Whenever we find a
	 * structure for this staff (another measure), loop through its
	 * STUFF list, dealing with each STUFF that is above, below, or
	 * between, as specified by "place".
	 */
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		if (mainll_p->str != S_STAFF ||
				mainll_p->u.staff_p->staffno != s) {
			continue;
		}

		for (stuff_p = mainll_p->u.staff_p->stuff_p;
				stuff_p != 0; stuff_p = stuff_p->next) {

			/* if STUFF should not be done on this pass, skip it */
			if (do_this_pass(s, stuff_p, place, do_which) == NO) {
				continue;
			}

			/* if aligntag is set, store it & deal with it later */
			if (stuff_p->aligntag != NOALIGNTAG) {
				/* have we encountered this tag before? */
				found = NO;
				for (n = 0; n < naligns; n++) {
					if (alignlist[n] == stuff_p->aligntag) {
						found = YES;
						break;
					}
				}
				/* if not found, add it to the list */
				if (found == NO) {
					/* allocate more slots if none free */
					if (szalignlist == 0) {
						szalignlist = ALIGNCHUNK;
						MALLOCA(short, alignlist,
								szalignlist);
					} else if (naligns == szalignlist) {
						szalignlist += ALIGNCHUNK;
						REALLOCA(short, alignlist,
								szalignlist);
					}
					alignlist[naligns] = stuff_p->aligntag;
					naligns++;
				}
				continue;
			}

			/* get the info we need about this non-aligned STUFF */
			get_misc_info(s, stuff_p, place, do_which,
					&high, &lowpart, &dist);

			if (stuff_p->dist_usage == SD_FORCE) {
				/*
				 * The user is forcing this dist, so don't
				 * stack; just put it there.  Note: the user
				 * cannot specify "dist" for "between" items.
				 */
				if (stuff_p->place == PL_ABOVE) {
					Rectab[Reclim].n = dist + high;
					Rectab[Reclim].s = dist;
					stuff_p->c[RS] = dist;
				} else {	/* PL_BELOW */
					Rectab[Reclim].n = -dist;
					Rectab[Reclim].s = -dist - high;
					stuff_p->c[RS] = -dist - high;
				}
				Rectab[Reclim].e = stuff_p->c[AE];
				Rectab[Reclim].w = stuff_p->c[AW];
				inc_reclim();
			} else if (stuff_p->stuff_type == ST_PEDAL &&
						stuff_p->string == 0) {
				/*
				 * This is a pedal continuation (invisible)char.
				 * (They are not user input, so never SD_FORCE.)
				 * To get here, the alignped parameter is n,
				 * so each pedal character gets its own
				 * rectangle; and the "align" keyword was not
				 * used on this one's BEGPED.  Being invisible,
				 * we don't want a rectangle for it, but its
				 * position matters, at least for the P_LINE
				 * case, because the print phase will draw a
				 * line starting here.  All we can do is put it
				 * just below the staff.  If the line runs
				 * through something, well, the user should
				 * have used the "align" keyword.
				 */
				stuff_p->c[RS] = - halfstaffhi(s)
						- Stdpad - high;
			} else {
				/*
				 * Stack the usual way.  For the case of
				 * "between", stackit() will ignore "dist".
				 */
				stuff_p->c[RS] = stackit(stuff_p->c[AW],
					stuff_p->c[AE], high, dist, place);
			}

			stuff_p->c[RN] = stuff_p->c[RS] + high;
			stuff_p->c[RY] = stuff_p->c[RS] + lowpart;
		}
	}

	restoressvstate();

	/*
	 * Now that all the non-aligned STUFFs have been placed, place the
	 * aligned ones, in ascending order of aligntag.
	 */
	if (naligns > 0) {
		qsort((char *)alignlist, naligns, sizeof (short),
				compaligntags);
		for (n = 0; n < naligns; n++) {
			doaligned(start_p, s, place, do_which, alignlist[n]);
		}
		FREE(alignlist);
	}
}

/*
 * Name:        do_this_pass()
 *
 * Abstract:    Should this STUFF be handled on this pass?
 *
 * Returns:     YES or NO
 *
 * Description: This function determines whether the give STUFF should be
 *		handled on this pass.
 */

static int
do_this_pass(s, stuff_p, place, do_which)

int s;				/* staff number */
struct STUFF *stuff_p;		/* the STUFF in question */
int place;			/* above, below, or between? */
unsigned long do_which;		/* which stuff types are to be handled */

{
	int stype;			/* stuff type */


	/* if this STUFF isn't of the right place, skip it */
	if (stuff_p->place != place) {
		return (NO);
	}

	stype = stuff_p->stuff_type;

	/* if this STUFF is not of the type we are doing now, skip it */
	if (stype == ST_MUSSYM) {
		if ((do_which & (1L << MK_MUSSYM)) == 0) {
			return (NO);
		}
	} else if (stype == ST_PEDAL) {
		if ((do_which & (1L << MK_PEDAL)) == 0) {
			return (NO);
		}
	} else if (stype == ST_OCTAVE) {
		if ((do_which & (1L << MK_OCTAVE)) == 0) {
			return (NO);
		}
	} else if (stype != ST_PHRASE && stuff_p->modifier == TM_DYN) {
		if ((do_which & (1L << MK_DYN)) == 0) {
			return (NO);
		}
	} else if (stype != ST_PHRASE && IS_CHORDLIKE(stuff_p->modifier)) {
		if ((do_which & (1L << MK_CHORD)) == 0) {
			return (NO);
		}
	} else if (IS_TEXT(stype)) {
		if ((do_which & (1L << MK_OTHERTEXT)) == 0) {
			return (NO);
		}
	}

	switch (stype) {
	case ST_PEDAL:
	if (svpath(s, ALIGNPED)->alignped == YES) {
		return (NO);	/* handled by a different subroutine */
	} else {
		return (YES);
	}

	case ST_PHRASE:
	case ST_TIESLUR:
	case ST_TABSLUR:
	case ST_BEND:
	case ST_MIDI:
		/* don't handle these types here; they have their own
		 * subroutines */
		return (NO);

	case ST_OCTAVE:
	case ST_ROM:
	case ST_BOLD:
	case ST_ITAL:
	case ST_BOLDITAL:
	case ST_MUSSYM:
	case ST_CRESC:
	case ST_DECRESC:
		return (YES);

	default:
		pfatal("unknown stuff type (%d)", stype);
	}

	return (NO);	/* we never really get here */
}

/*
 * Name:        get_misc_info()
 *
 * Abstract:    Find the info about the STUFF we will need for placing it.
 *
 * Returns:     void
 *
 * Description: This function finds the dimensions of the STUFF and/or the
 *		minimum distance it should be from the staff or baseline.
 */

static void
get_misc_info(s, stuff_p, place, do_which, high_p, lowpart_p, dist_p)

int s;				/* staff number */
struct STUFF *stuff_p;		/* the STUFF in question */
int place;			/* above, below, or between? */
unsigned long do_which;		/* which stuff types are to be handled */
float *high_p;			/* return height of the rectangle */
float *lowpart_p;		/* return dist between stuff's Y and S */
float *dist_p;			/* return how close rect can get to staff */

{
	int stype;			/* stuff type */
	float len;			/* length of a cresc/descresc */
	float high;			/* height of a rectangle */
	float lowpart;			/* dist between stuff's Y and S */
	float dist;			/* how close chord can get to staff */


	stype = stuff_p->stuff_type;

	/*
	 * The "stuff" needs to be positioned.  First find its total height,
	 * and the height of the part of it below its Y coord.
	 */
	/* avoid 'used before set' warning */
	high = lowpart = 0.0;

	/* handle various types differently */
	switch (stype) {
	case ST_OCTAVE:
	case ST_ROM:
	case ST_BOLD:
	case ST_ITAL:
	case ST_BOLDITAL:
	case ST_MUSSYM:
		/* high is string's height */
		high = strheight(stuff_p->string);
		lowpart = strdescent(stuff_p->string);

		/*
		 * If a chord grid is to be printed under the string, the Y and
		 * N of the stuff remain unchanged, but its S is lowered by the
		 * total height of the grid.  So add its height to both "high"
		 * and "lowpart".
		 */
		if (stuff_p->modifier == TM_CHORD &&
		    ((stuff_p->all == YES && Score.gridswhereused) ||
		     (stuff_p->all == NO &&
			    svpath(s, GRIDSWHEREUSED)->gridswhereused == YES))){
			struct GRID *grid_p;
			float gnorth, gsouth;

			grid_p = findgrid(stuff_p->grid_name);
			/* if none, skip this; stuff.c warned */
			if (grid_p == 0)
				break;

			gridsize(grid_p, stuff_p->all ? 0 : s,
				&gnorth, &gsouth, (float *)0, (float *)0);

			high += gnorth - gsouth;
			lowpart += gnorth - gsouth;
		}
		break;

	case ST_PEDAL:
		/*
		 * Whichever pedal character this is, always use C_BEGPED if
		 * pedstyle is P_LINE and the "Ped." string for the other
		 * cases.  For the former, all three characters are the same
		 * height; and for the latter, this string is taller than the
		 * "*".  This also handles the pedal continuation situation.
		 */
		if (svpath(s, PEDSTYLE)->pedstyle == P_LINE) {
			lowpart = descent(FONT_MUSIC, DFLT_SIZE, C_BEGPED);
			high  = height(FONT_MUSIC, DFLT_SIZE, C_BEGPED);
		} else { /* P_PEDSTAR or P_ALTPEDSTAR */
			lowpart = strdescent(Ped_start);
			high  = strheight(Ped_start);
		}

		if (stuff_p->all) {
			lowpart *= Score.staffscale;
			high  *= Score.staffscale;
		} else {
			lowpart *= Staffscale;
			high  *= Staffscale;
		}
		break;

	case ST_CRESC:
	case ST_DECRESC:
		/* height depends on length */
		len = stuff_p->c[AE] - stuff_p->c[AW];

		if (len < 0.5) {
			high = 2.00 * STEPSIZE + 2 * STDPAD;
		} else if (len < 2.0) {
			high = 2.67 * STEPSIZE + 2 * STDPAD;
		} else {
			high = 3.33 * STEPSIZE + 2 * STDPAD;
		}

		if (stuff_p->all) {
			high *= Score.staffscale;
		} else {
			high *= Staffscale;
		}

		lowpart = high / 2;

		break;

	default:
		pfatal("wrong stuff type (%d) in get_misc_info", stype);
	}

	/*
	 * Now find "dist", the minimum distance it should be put from the
	 * staff.
	 */
	if (stuff_p->dist_usage == SD_NONE) {
		/*
		 * The user didn't specify the dist, so we get it from the
		 * appropriate parameter or hard-coded value, as the case may
		 * be.  For parameters, if the stuff belongs to the score as a
		 * whole ("all"), use the Score value instead of svpath.
		 */
		/* if "dyn", fake stype to use the same logic as cresc */
		if (stuff_p->modifier == TM_DYN) {
			stype = ST_CRESC;
		}
		switch (stype) {
		case ST_ROM:
		case ST_BOLD:
		case ST_ITAL:
		case ST_BOLDITAL:
			if (stuff_p->all) {
				if (IS_CHORDLIKE(stuff_p->modifier)) {
					dist = halfstaffhi(s) + STEPSIZE *
					Score.staffscale * Score.chorddist;
				} else {
					dist = halfstaffhi(s) + STEPSIZE *
					Score.staffscale * Score.dist;
				}
			} else {
				if (IS_CHORDLIKE(stuff_p->modifier)) {
					dist = halfstaffhi(s) + Stepsize *
					svpath(s, CHORDDIST)->chorddist;
				} else {
					dist = halfstaffhi(s) + Stepsize *
					svpath(s, DIST)->dist;
				}
			}
			break;
		case ST_CRESC:
		case ST_DECRESC:
			if (stuff_p->all) {
				dist = halfstaffhi(s) + STEPSIZE *
				Score.staffscale * Score.dyndist;
			} else {
				dist = halfstaffhi(s) + Stepsize *
				svpath(s, DYNDIST)->dyndist;
			}
			break;
		default:
			dist = 0;
			break;
		}
	} else {
		/* the user specified the dist, so use that */
		if (stuff_p->all) {
			dist = halfstaffhi(s) + STEPSIZE * stuff_p->dist;
		} else {
			dist = halfstaffhi(s) + Stepsize * stuff_p->dist;
		}
	}

	/* return the answers */
	*high_p = high;
	*lowpart_p = lowpart;
	*dist_p = dist;
}

/*
 * Name:        compaligntags()
 *
 * Abstract:    Function for qsort to call to compare aligntags
 *
 * Returns:     positive for tag1 > tag2, negative for tag1 < tag2, 0 if equal
 *
 * Description: See above.
 */

static int
compaligntags(tag1_p, tag2_p)

#ifdef __STDC__
const void *tag1_p;
const void *tag2_p;
#else
char *tag_1_p;
char *tag_2_p;
#endif
{
	return (*(short *)tag1_p - *(short *)tag2_p);
}

/*
 * Name:        compstuffs()
 *
 * Abstract:    Function for qsort to call to compare STUFFs based on their AX
 *
 * Returns:     positive if stuff1.c[AX] > stuff2.c[AX], neg if <, 0 if ==
 *
 * Description: See above.
 */

static int
compstuffs(stuff1_p, stuff2_p)

#ifdef __STDC__
const void *stuff1_p;
const void *stuff2_p;
#else
char *stuff_1_p;
char *stuff_2_p;
#endif
{
	float ax1, ax2;		/* AX of the STUFFs */


	ax1 = (*(struct STUFF **)stuff1_p)->c[AX];
	ax2 = (*(struct STUFF **)stuff2_p)->c[AX];

	if (ax1 > ax2) {
		return (1);
	} else if (ax1 < ax2) {
		return (-1);
	} else {
		return (0);
	}
}

/*
 * Name:        comp_phrases()
 *
 * Abstract:    Function for qsort to call to compare phrases in the
 *		processing order to make nesting look good
 *
 * Returns:     positive if phrase 2 should be processed before phrase 1
 *		negative if phrase 1 should be processed before phrase 2
 *		0 if they are at the same place, so it doesn't matter
 *
 * Description: See above.
 */

static int
comp_phrases(phrinfo1_p, phrinfo2_p)

#ifdef __STDC__
const void *phrinfo1_p;
const void *phrinfo2_p;
#else
char *phrinfo1_p;
char *phrinfo2_p;
#endif
{
	struct STUFF *stuff1_p;
	struct STUFF *stuff2_p;


	stuff1_p = ((struct PHRINFO *)phrinfo1_p)->stuff_p;
	stuff2_p = ((struct PHRINFO *)phrinfo2_p)->stuff_p;

	/* whichever starts later should be processed first */
	if (stuff1_p->beggrp_p->c[AX] < stuff2_p->beggrp_p->c[AX]) {
		return (1);
	}
	if (stuff1_p->beggrp_p->c[AX] > stuff2_p->beggrp_p->c[AX]) {
		return (-1);
	}
	/* they start at the same place */

	/*
	 * Whichever ends sooner should be processed first.  But we can't just
	 * look at AX, because the phrase might cross onto later score(s).  So
	 * first check how many bars they cross.
	 */
	if (stuff1_p->end.bars > stuff2_p->end.bars) {
		return (1);
	}
	if (stuff1_p->end.bars < stuff2_p->end.bars) {
		return (-1);
	}

	/* they end in the same measure, so now use AX */
	if (stuff1_p->endgrp_p->c[AX] > stuff2_p->endgrp_p->c[AX]) {
		return (1);
	}
	if (stuff1_p->endgrp_p->c[AX] < stuff2_p->endgrp_p->c[AX]) {
		return (-1);
	}

	return (0);
}
/*
 * Name:        doaligned()
 *
 * Abstract:    Place the STUFFs that have the given align tag.
 *
 * Returns:     void
 *
 * Description: This function goes through the given score/staff/place, finding
 *		all the STUFFs with the given align tag.  It puts rectangles
 *		into Rectab for them, and if any need to be squeezed, updates
 *		their AE and AW.
 */

static void
doaligned(start_p, s, place, do_which, tag)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above, below, or between? */
unsigned long do_which;		/* which stuff types are to be handled */
int tag;			/* the aligntag of the STUFFs to be placed */

{
	struct MAINLL *mainll_p;	/* loop through main linked list */
	struct STUFF *stuff_p;		/* point along a STUFF list */
	int nstuffs;			/* number of stuffs encountered */
	struct STUFF **stufflist;	/* list of stuffs encountered */
	int szstufflist;		/* allocated slots in stufflist */
	int n;				/* loop index */
	int ridx;			/* index into Rectab */
	float overlap;			/* of two rectangles */
	float extwid;			/* width of an extender */
	int dist_usage;			/* to be used by the aligned set */
	float setdist;			/* dist set by the user */
	float defdist;			/* a default dist */
	float newdist;			/* another dist */
	float fact;			/* factor for squeezing */
	int rfirst;			/* idx to first new rectangle */
	float mas;			/* current value of minalignscale */
	struct RECTAB *relrec;		/* points to a rectangle in Rectab[] */
	int n2, n3;


	nstuffs = 0;			/* no STUFFs seen yet */
	stufflist = 0;			/* pointer to list is null */
	szstufflist = 0;		/* no slots allocated yet */

	/*
	 * Loop through this score's part of the MLL, finding the appropriate
	 * STUFFs, like the loop in domiscstuff, except deal with only the ones
	 * that have the given align tag.  Add them to the stufflist.
	 */
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {

		if (mainll_p->str != S_STAFF ||
				mainll_p->u.staff_p->staffno != s) {
			continue;
		}

		for (stuff_p = mainll_p->u.staff_p->stuff_p;
				stuff_p != 0; stuff_p = stuff_p->next) {

			/* if STUFF should not be done on this pass, skip it */
			if (do_this_pass(s, stuff_p, place, do_which) == NO) {
				continue;
			}

			/* if not the tag we are dealing with now, skip it */
			if (stuff_p->aligntag != tag) {
				continue;
			}

			/* allocate more slots if none free */
			if (szstufflist == 0) {
				szstufflist = ALIGNCHUNK;
				MALLOCA(struct STUFF *, stufflist, szstufflist);
			} else if (nstuffs == szstufflist) {
				szstufflist += ALIGNCHUNK;
				REALLOCA(struct STUFF *,stufflist, szstufflist);
			}
			stufflist[nstuffs] = stuff_p;
			nstuffs++;
		}
	}

	/* sort the STUFFs in order of increasing AX coordinate */
	qsort((char *)stufflist, nstuffs, sizeof (struct STUFF *), compstuffs);

	rfirst = Reclim;	/* remember where first new rectangle will be*/

	/*
	 * Allocate rectangles, initializing their horizontal coords.  Also get
	 * each one's high, lowpart, and dist, for later use.  We use the
	 * absolute vert coords as a scratch area for them, to avoid having to
	 * allocate and free memory.  Later, absstaff() will overwrite them
	 * with the true absolute coords.
	 */
#define SHIGH		c[AY]
#define SLOWPART	c[AN]
#define SDIST		c[AS]
	dist_usage = SD_NONE;
	defdist = -1000000.0;
	setdist = 0.0;		/* for lint */
	for (n = 0; n < nstuffs; n++) {
		ridx = rfirst + n;

		Rectab[ridx].e = stufflist[n]->c[AE];
		Rectab[ridx].w = stufflist[n]->c[AW];

		get_misc_info(s, stufflist[n], place, do_which,
				&stufflist[n]->SHIGH,
				&stufflist[n]->SLOWPART,
				&stufflist[n]->SDIST);
		newdist = stufflist[n]->SDIST;	/* for convenience */

		if (stufflist[n]->dist_usage == SD_NONE) {
			/* remember the max dist that was not set by the user*/
			if (newdist > defdist) {
				defdist = newdist;
			}
		} else {
			/*
			 * The user set the dist for this STUFF.  If this is
			 * not the first one they did it on, it should agree,
			 * else warn and use the last one.
			 */
			if (dist_usage != SD_NONE && newdist != setdist) {
				l_warning(stufflist[n]->inputfile,
				stufflist[n]->inputlineno,
				"mark 'dist' overrides 'dist' used by earlier aligned mark");
			}
			setdist = newdist;

	 		/*
			 * If any are forced ("!") we will force them all.
			 * And if any are set, remember to use that set value.
			 */
			if (dist_usage != SD_FORCE) {
				dist_usage = stufflist[n]->dist_usage;
			}
		}

		inc_reclim();
	}

	/*
	 * Working right to left (but skipping the rightmost), try to prevent
	 * each rectangle from overlapping the one to its right.
	 * Initially we only shorten or remove extenders.
	 */
	for (n = nstuffs - 2; n >= 0; n--) {
		ridx = rfirst + n;

		/*
		 * See how far our rectangle overlaps the one on the right, if
		 * at all.
		 */
		overlap = Rectab[ridx].e - Rectab[ridx + 1].w;

		/*
		 * If if overlaps, take it off the extender, up to the whole
		 * extender length if need be, but no more.  Set the new
		 * amount of overlap, and fix the east boundary.
		 */
		if (overlap > 0.0) {
			extwid = extwidth(stufflist[n]);
			if (extwid > 0.0) {
				if (overlap <= extwid) {
					Rectab[ridx].e -= overlap;
				} else {
					Rectab[ridx].e -= extwid;
				}
				stufflist[n]->c[AE] = Rectab[ridx].e;
			}
		}
	}

	/*
	 * Now check again to see if there is still overlap.  Work right to
	 * left.  Wherever more than two in a row overlap, we shrink each one
	 * in turn, starting from the second rightmost.  We go right to left
	 * because the amount of shrinkage needed for a rectangle depends on
	 * the (possibly shrunk) size of the rectangle to its right.  The
	 * theory here is that many STUFFs stick out much farther to the right
	 * of AX than to the left.
	 * But when only two overlap, we use a more complex algorithm, to
	 * shrink each to some degree.  This algorithm better handles cases
	 * where STUFFs may stick out farther left than right.
	 */
	relrec = &Rectab[rfirst];	/* first in our set of rectangles */
	n = nstuffs - 2;
	while (n >= 0) {
		/*
		 * See how far our rectangle overlaps the one on the right, if
		 * at all.
		 */
		overlap = relrec[n].e - relrec[n + 1].w;

		/* if no overlap, move on to the next */
		if (overlap <= 0.0) {
			n--;
			continue;
		}

		/*
		 * relrec rectangles n and n+1 overlap.  Look left to see how
		 * many consecutive ones overlap.
		 */
		for (n2 = n; n2 > 0 && relrec[n2 - 1].e - relrec[n2].w > 0.0;
				n2--) {
			; /* rectangles n2-1 and n2 overlap */
		}

		/* rectangles n2 and n2+1 are the leftmost two in this
		 * sequence that overlap */

		if (n - n2 == 0) {
			/*
			 * Only n and n+1 overlap; apply a special algorithm.
			 * They will be shrunk by different factors: fact1 and
			 * fact2.  We want the two rectangles to just touch,
			 * instead of overlap.  If the east side of "n" is
			 * wider than the west side of "n+1", "n" should be
			 * penalized (shrunk) more.  (And likewise the
			 * opposite case.)  Let's make it so that
			 * (1 - fact1) / (1 - fact2) = (n's east) / (n+1's west)
			 * We also know that
			 * (n's east) * fact1 + (n+1's west) * fact2 =
			 *		(n+1's AX) - (n's AX)
			 * So we have two equations in the two unknowns fact1
			 * and fact2.  Do the algebra, and the answers are as
			 * calculated below.
			 */
			float n_e, n1_w, fact1, fact2;
			n_e = stufflist[n]->c[AE] - stufflist[n]->c[AX];
			n1_w = stufflist[n+1]->c[AX] - stufflist[n+1]->c[AW];
			fact1 = (n1_w*n1_w/n_e - n1_w - stufflist[n]->c[AX]
			    + stufflist[n + 1]->c[AX]) / (n_e + n1_w*n1_w/n_e);
			fact2 = (n_e - n1_w + n1_w*fact1) / n_e;

			mas = svpath(s, MINALIGNSCALE)->minalignscale;

			/* compress the left one, n */
			if (fact1 < mas) {
				fact1 = mas;
				l_warning(stufflist[n]->inputfile,
				stufflist[n]->inputlineno,
				"cannot shrink aligned mark enough (%.3f) to avoid collision with the next one; you can set parameter minalignscale to avoid this", fact1);
			}

			/* tell print phase how much to shrink */
			stufflist[n]->horzscale = fact1;

			/* revise the boundaries in relrec */
			relrec[n].e = stufflist[n]->c[AX] + fact1 *
				(stufflist[n]->c[AE] - stufflist[n]->c[AX]);
			relrec[n].w = stufflist[n]->c[AX] + fact1 *
				(stufflist[n]->c[AW] - stufflist[n]->c[AX]);

			/* revise the boundaries in the STUFF structure too */
			stufflist[n]->c[AE] = relrec[n].e;
			stufflist[n]->c[AW] = relrec[n].w;

			/* compress the right one, n+1 */
			if (fact2 < mas) {
				fact2 = mas;
				l_warning(stufflist[n+1]->inputfile,
				stufflist[n+1]->inputlineno,
				"cannot shrink aligned mark enough (%.3f) to avoid collision with the next one; you can set parameter minalignscale to avoid this", fact2);
			}

			/* tell print phase how much to shrink */
			stufflist[n+1]->horzscale = fact2;

			/* revise the boundaries in relrec */
			relrec[n+1].e = stufflist[n+1]->c[AX] + fact2 *
				(stufflist[n+1]->c[AE] - stufflist[n+1]->c[AX]);
			relrec[n+1].w = stufflist[n+1]->c[AX] + fact2 *
				(stufflist[n+1]->c[AW] - stufflist[n+1]->c[AX]);

			/* revise the boundaries in the STUFF structure too */
			stufflist[n+1]->c[AE] = relrec[n+1].e;
			stufflist[n+1]->c[AW] = relrec[n+1].w;

			n -= 2;
			continue;
		}

		/*
		 * Rectangles n2 through n+1 overlap, which is at least 3.
		 * Start at the next to rightmost, and moving left, shrink.
		 */
		for (n3 = n; n3 >= n2; n3--) {

			fact = (relrec[n3 + 1].w - stufflist[n3]->c[AX]) /
			       (relrec[n3].e     - stufflist[n3]->c[AX]);
			mas = svpath(s, MINALIGNSCALE)->minalignscale;
			if (fact < mas) {
				fact = mas;
				l_warning(stufflist[n3]->inputfile,
				stufflist[n3]->inputlineno,
				"cannot shrink aligned mark enough (%.3f) to avoid collision with the next one; you can set parameter minalignscale to avoid this", fact);
			}

			/* tell print phase how much to shrink */
			stufflist[n3]->horzscale = fact;

			/* revise the boundaries in relrec */
			relrec[n3].e = stufflist[n3]->c[AX] + fact *
				(stufflist[n3]->c[AE] - stufflist[n3]->c[AX]);
			relrec[n3].w = stufflist[n3]->c[AX] + fact *
				(stufflist[n3]->c[AW] - stufflist[n3]->c[AX]);

			/* revise the boundaries in the STUFF structure too */
			stufflist[n3]->c[AE] = relrec[n3].e;
			stufflist[n3]->c[AW] = relrec[n3].w;
		}

		n -= n + 1 - n2;

	}

	/*
	 * For P_LINE pedal marks, for each BEGPED, optional PEDAL(s), ENDPED
	 * group of them, we want to cover all the space between them, to
	 * prevent other STUFF from overwriting the lines that join them.  So
	 * we widen their rectangles.
	 */
	if (do_which == (1L << MK_PEDAL) &&
			svpath(s, PEDSTYLE)->pedstyle == P_LINE) {

		/* keep lint happy, but not needed; we know nstuffs > 0 */
		ridx = rfirst;

		/* loop through all stuffs (pedal marks) in the list */
		for (n = 0; n < nstuffs; n++) {
			ridx = rfirst + n;	/* get the rectangle for it */

			/*
			 * If the pedal mark is not BEGPED, move its west to
			 * touch the east of the previous pedal; unless it's the
			 * first (non-continuation) pedal mark on this score.
			 */
			if (string_is_sym(stufflist[n]->string, C_BEGPED,
						FONT_MUSIC) == NO) {
				if (n > 0 &&
				   ! (n == 1 && stufflist[0]->string == 0)) {
					Rectab[ridx].w = Rectab[ridx - 1].e;
				}
			}
		}
	}

	if (dist_usage == SD_FORCE) {
		/*
		 * The user forced a particular dist, so just use it, without
		 * regard for any overlap of existing rectangles.
		 */
		for (n = 0; n < nstuffs; n++) {
			ridx = rfirst + n;
			if (place == PL_ABOVE) {
				Rectab[ridx].n = setdist + stufflist[n]->SHIGH;
				Rectab[ridx].s = setdist;
			} else {	/* PL_BELOW */
				Rectab[ridx].n = -setdist;
				Rectab[ridx].s = -setdist - stufflist[n]->SHIGH;
			}
		}
	} else {
		/*
		 * The dist is not forced, so we will use multistackit.  It
		 * needs Rectab[ridx].n set to the height, as a scratch
		 * variable.
		 */
		for (n = 0; n < nstuffs; n++) {
			ridx = rfirst + n;
			Rectab[ridx].n = stufflist[n]->SHIGH;
		}

		/* pass default dist or, if the user set dist, the set dist */
		multistackit(nstuffs,
			(dist_usage == SD_NONE ? defdist : setdist), place);
	}

	/* set the STUFF structures' relative vert coords from the rectangles*/
	for (n = 0; n < nstuffs; n++) {
		ridx = rfirst + n;
		stufflist[n]->c[RN] = Rectab[ridx].n;
		stufflist[n]->c[RS] = Rectab[ridx].s;
		stufflist[n]->c[RY] = stufflist[n]->c[RS] +
				stufflist[n]->SLOWPART;
	}

	FREE(stufflist);
}

/*
 * Name:        dolyrics()
 *
 * Abstract:    Set up rectangles and vert coords for lyrics.
 *
 * Returns:     void
 *
 * Description: This function puts into Rectab a rectangle for each verse in
 *		the "place" relationship to the given staff on this score.
 */
#define LYRIC_SIDEPAD	5

static void
dolyrics(start_p, s, place)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above, below, or between? */

{
	int *versenums;		/* malloc'ed array of verse numbers in score */
	struct MAINLL *mainll_p;/* point along main linked list */
	struct STAFF *staff_p;	/* point at a staff structure */
	struct GRPSYL *gs_p;	/* point at a syllable */
	float protrude;		/* farthest protrusion of rectangle */
	int vfound;		/* number of verse numbers found in score */
	int v;			/* verse number */
	int begin, end, delta;	/* for looping over verses in proper order */
	float dist;		/* how close lyrics can get to staff */
	float farwest, fareast;	/* farthest east and west of any syllable */
	float baseline;		/* baseline of a verse of syllables */
	float maxasc, maxdes;	/* max ascent & descent of syllables */
	int gotverse0;		/* is there a verse 0 (centered verse)? */
	int gototherverse;	/* is there a normal verse (not 0)? */
	int n, k, j;		/* loop variables */


	debug(32, "dolyrics file=%s line=%d s=%d place=%d", start_p->inputfile,
			start_p->inputlineno, s, place);
	/* if there are no lyrics in this song, get out now */
	if (Maxverses == 0)
		return;

	/*
	 * Allocate an array containing room for all the verse numbers used in
	 * this score.  Maxverses is the number of verse numbers used in the
	 * whole user input, so this will certainly be enough.
	 */
	MALLOCA(int, versenums, Maxverses);

	/*
	 * Loop through this score's part of the MLL, noting whether verse 0
	 * (the centered verse) and/or other verses exist on the "place" side
	 * of the staff.  We have to find this out before actually processing
	 * the verses, because verse 0 is to be treated as a normal verse if
	 * and only if there are no other verses.
	 */
	gotverse0 = NO;
	gototherverse = NO;
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {
		/*
		 * Whenever we find a structure for this staff (another
		 * measure of this staff), loop through its verse headcells.
		 */
		if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno == s) {
			staff_p = mainll_p->u.staff_p;
			for (n = 0; n < staff_p->nsyllists; n++) {
				if (staff_p->sylplace[n] == place) {
					if (staff_p->syls_p[n]->vno == 0)
						gotverse0 = YES;
					else
						gototherverse = YES;
				}
			}
		}
	}

	/* if no verses, get out now */
	if (gotverse0 == NO && gototherverse == 0) {
		FREE(versenums);
		return;
	}

	/*
	 * Loop through this score's part of the MLL, recording all the verse
	 * numbers that occur on the "place" side of the staff in versenums[].
	 * Verse 0 may or may not be included, depending on the above results.
	 * Also set farwest and fareast.
	 */
	vfound = 0;			/* no verses have been found yet */
	farwest = EFF_PG_WIDTH;		/* init it all the way east */
	fareast = 0;			/* init it all the way west */
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {
		/*
		 * Whenever we find a structure for this staff (another
		 * measure of this staff), loop through its verse headcells.
		 */
		if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno == s) {

			staff_p = mainll_p->u.staff_p;

			for (n = 0; n < staff_p->nsyllists; n++) {

				if (staff_p->sylplace[n] == place) {
					/*
					 * We found a verse number.  Search the
					 * the array to see if it's already
					 * been found.  If not, insert it into
					 * versenums[] in the right place, so
					 * that they'll end up being in order
					 * (actually, reverse order).
					 */
					v = staff_p->syls_p[n]->vno;
					/* ignore verse 0 if others exist */
					if (v == 0 && gototherverse == YES)
						continue;
					for (k = 0; k < vfound &&
						    v < versenums[k]; k++) {
						;
					}
					if (k == vfound || v > versenums[k]) {
						for (j = vfound; j > k; j--) {
							versenums[j] =
							versenums[j-1];
						}
						versenums[k] = v;
						vfound++;  /* found one more */
					}

					/*
					 * If any syl sticks out farther than
					 * any previous one, extend farwest or
					 * fareast.
					 */
					for (gs_p = staff_p->syls_p[n];
					     gs_p != 0; gs_p = gs_p->next) {

						if (gs_p->c[AW] < farwest)
							farwest = gs_p->c[AW];
						if (gs_p->c[AE] > fareast)
							fareast = gs_p->c[AE];
					}
				}
			}
		}
	}

	/*
	 * Enclose all the syllables of all the verses (of this place) in one
	 * big rectangle.  Pad on west and east by LYRIC_SIDEPAD.  Pretend the
	 * rectangle is EFF_PG_HEIGHT high.  We don't actually know yet how high
	 * it is, and this will prevent it from getting between the staff and
	 * anything else.  Later in this function we will correct the entry
	 * that stackit put in Rectab, to reflect the true height.  For above
	 * and below cases, don't let it get any closer to the staff than
	 * allowed by the lyricsdist parameter.  The half-height of a one-line
	 * staff is regarded as 1 instead of the true 0, to give a little
	 * breathing room.
	 */
	if (place == PL_BETWEEN)
		dist = 0;
	else
		dist = halfstaffhi(s) +
				svpath(s, LYRICSDIST)->lyricsdist * Stepsize;

	(void)stackit(farwest - LYRIC_SIDEPAD * STEPSIZE,
			fareast + LYRIC_SIDEPAD * STEPSIZE,
			EFF_PG_HEIGHT, dist, place);

	/*
	 * Find the greatest protrusion of any currently existing rectangle
	 * that horizontally is within the span of our new rectangle.  That's
	 * the same as the top or bottom of the new rectangle.
	 */
	if (place == PL_BELOW)
		protrude = Rectab[Reclim - 1].n;
	else
		protrude = Rectab[Reclim - 1].s;

	/*
	 * Loop through the verses, from the inside out. setting the relative
	 * vertical coords of their syllables.  When necessary, we also insert
	 * new syllables on the next score for continuing underscores.
	 */
	if (place == PL_BELOW) {	/* work downward from staff */
		begin = vfound - 1;	/* first verse number */
		end = -1;		/* beyond last verse number */
		delta = -1;
	} else {	/* above and between both work upwards from bottom */
		begin = 0;		/* last verse number */
		end = vfound;		/* before first verse number */
		delta = 1;
	}
	for (n = begin; n != end; n += delta) {
		/*
		 * Find the farthest any syllable ascends and descends from the
		 * baseline of the verse.
		 */
		getvsize(start_p, s, place, versenums[n], &maxasc, &maxdes);

		/*
		 * Set the baseline for this verse, based on where we're
		 * pushing up against (the last verse we did, or earlier
		 * things), and how far this verse sticks out.
		 */
		if (place == PL_BELOW)
			baseline = protrude - maxasc;
		else	/* above or between */
			baseline = protrude + maxdes;

		/* set syllables' vertical coords; continue underscores */
		setsylvert(start_p, s, place, versenums[n], baseline);

		/* set new lower bound, for next time through loop */
		if (place == PL_BELOW)
			protrude = baseline - maxdes;
		else	/* above or between */
			protrude = baseline + maxasc;

	} /* for every verse */

	/*
	 * If there was a verse 0 (centered verse) and also normal verses, then
	 * in the above code we have handled only the normal verses, and we now
	 * need to handle verse 0.
	 */
	if (gotverse0 == YES && gototherverse == YES) {
		float mid;	/* RY of the middle of the normal verses */
		struct RECTAB rec;	/* one rectangle */

		/* get ascent and descent of verse 0 */
		getvsize(start_p, s, place, 0, &maxasc, &maxdes);

		/*
		 * We will use stackit's "dist" mechanism to try to get verse 0
		 * to line up with the center of the other verses.  The last
		 * rectangle in Rectab is currently the normal verses', but the
		 * one coord isn't really set right yet.  Fortunately, the
		 * "protrude" variable is what we need for that coord.
		 */
		if (place == PL_BELOW) {
			mid = (Rectab[Reclim - 1].n + protrude) / 2.0;
			dist = -mid - (maxasc + maxdes) / 2.0;
		} else {
			mid = (protrude + Rectab[Reclim - 1].s) / 2.0;
			dist = mid - (maxasc + maxdes) / 2.0;
		}

		/*
		 * Find the easternmost and westernmost points of verse 0.
		 * It's easier to loop through all the syllables than to try to
		 * find the first and last syllables on the line.
		 */
		farwest = EFF_PG_WIDTH;		/* init it all the way east */
		fareast = 0;			/* init it all the way west */
		for (mainll_p = start_p->next;
				mainll_p != 0 && mainll_p->str != S_FEED;
				mainll_p = mainll_p->next) {

			if (mainll_p->str != S_STAFF ||
					mainll_p->u.staff_p->staffno != s)
				continue;

			staff_p = mainll_p->u.staff_p;
			for (n = 0; n < staff_p->nsyllists; n++) {
				if (staff_p->sylplace[n] == place &&
						staff_p->syls_p[n]->vno == 0) {
					for (gs_p = staff_p->syls_p[n];
					     gs_p != 0; gs_p = gs_p->next) {

						if (gs_p->c[AW] < farwest)
							farwest = gs_p->c[AW];
						if (gs_p->c[AE] > fareast)
							fareast = gs_p->c[AE];
					}
				}
			}
		}

		/*
		 * Squeeze the regular verses' rectangle to zero so that it
		 * won't affect verse 0's.  We hope they wouldn't interfere
		 * anyway, but the +8 and -8 might make them.  The regular
		 * verses' rectangle will be corrected later anyway.
		 */
		Rectab[Reclim - 1].n = Rectab[Reclim - 1].s = 0;

		/*
		 * Stack verse 0's rectangle and set its baseline.  We have to
		 * play games with "place", because for "between" stackit
		 * ignores "dist", but we need it to use "dist".
		 */
		baseline = stackit(farwest - LYRIC_SIDEPAD * STEPSIZE,
			fareast + LYRIC_SIDEPAD * STEPSIZE,
			maxasc + maxdes, dist,
			place == PL_BETWEEN ? PL_ABOVE : place) + maxdes;

		/*
		 * Switch verse 0's rectangle and the normal verses' so that
		 * the later code can always use reclim-1 for the normal.
		 */
		rec = Rectab[Reclim - 2];
		Rectab[Reclim - 2] = Rectab[Reclim - 1];
		Rectab[Reclim - 1] = rec;

		setsylvert(start_p, s, place, 0, baseline);
	}

	/*
	 * Now that we know how high this rectangle really is, correct it in
	 * Rectab.  Make it reach the center of the staff/baseline, to prevent
	 * anything later from getting in between there.
	 */
	if (place == PL_BELOW) {
		Rectab[Reclim - 1].n = 0;
		Rectab[Reclim - 1].s = protrude;
	} else {	/* above or between */
		Rectab[Reclim - 1].n = protrude;
		Rectab[Reclim - 1].s = 0;
	}

	FREE(versenums);
}

/*
 * Name:        getvsize()
 *
 * Abstract:    Get the maximum ascent and descent for a verse on a score.
 *
 * Returns:     void
 *
 * Description: This function returns (through pointers) the maximum ascent and
 *		descent of a verse on this score.  Usually this is the standard
 *		ascent and descent of the font, but it could be greater if
 *		there are font or size changes inside some syllable.
 */

static void
getvsize(start_p, s, place, v, maxasc_p, maxdes_p)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above, below, or between? */
int v;				/* verse number */
float *maxasc_p, *maxdes_p;	/* ascent and descent to be returned */

{
	int lyricsfont;		/* that is set for this staff */
	int lyricssize;		/* that is set for this staff */
	float asc, des;		/* max ascent & descent of syllables */
	struct MAINLL *mainll_p;/* point along main linked list */
	struct STAFF *staff_p;	/* point at a staff structure */
	struct GRPSYL *gs_p;	/* point at a syllable */
	int k;			/* loop variable */


	/*
	 * Get the standard max ascent and descent for any syllable.
	 */
	lyricsfont = svpath(s, LYRICSFONT)->lyricsfont;
	lyricssize = svpath(s, LYRICSSIZE)->lyricssize;
	*maxasc_p = fontascent(lyricsfont, lyricssize) * Staffscale;
	*maxdes_p = fontdescent(lyricsfont, lyricssize) * Staffscale;

	/* save SSVs so that we can restore them after this loop */
	savessvstate();

	/*
	 * Find the farthest any syllable ascends and descends from the
	 * baseline of the verse.  Start with the standard amount for this font
	 * size.  If the loop finds any weird syllable with bigger characters
	 * embedded, they will be increased.
	 */
	for (mainll_p = start_p->next; mainll_p != 0 && mainll_p->str
				!= S_FEED; mainll_p = mainll_p->next) {


		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);

			/*
			 * If that SSV affected lyrics font or size, update the
			 * values.  We use asgnssv() and svpath() instead of
			 * using the SSV's value directly, to avoid duplicating
			 * the viewpathing logic here, outside of ssv.c.
			 */
			if (mainll_p->u.ssv_p->used[LYRICSFONT] == YES) {
				lyricsfont = svpath(s, LYRICSFONT)->lyricsfont;
				asc = fontascent(lyricsfont, lyricssize)
						* Staffscale;
				if (asc > *maxasc_p) {
					*maxasc_p = asc;
				}
			}

			if (mainll_p->u.ssv_p->used[LYRICSSIZE] == YES) {
				lyricssize = svpath(s, LYRICSSIZE)->lyricssize;
				des = fontdescent(lyricsfont, lyricssize)
						* Staffscale;
				if (des > *maxdes_p) {
					*maxdes_p = des;
				}
			}
			continue;
		}

		if (mainll_p->str != S_STAFF ||
				mainll_p->u.staff_p->staffno != s)
			continue;

		/* found a STAFF of the number we're dealing with */
		staff_p = mainll_p->u.staff_p;

		/*
		 * See if this verse is present in this staff,
		 * and if so, loop through it.
		 */
		for (k = 0; k < staff_p->nsyllists; k++) {

			if (staff_p->sylplace[k] == place &&
					staff_p->syls_p[k]->vno == v) {

				for (gs_p = staff_p->syls_p[k]; gs_p != 0;
						gs_p = gs_p->next) {
					/*
					 * If asc or des is greater
					 * for this syl, save it.
					 */
					asc = strascent(gs_p->syl);

					des = strdescent(gs_p->syl);

					if (asc > *maxasc_p)
						*maxasc_p = asc;
					if (des > *maxdes_p)
						*maxdes_p = des;
				}

				/* no need to look any more */
				break;
			}
		}
	} /* for every MLL stucture in score */

	restoressvstate();
}

/*
 * Name:        setsylvert()
 *
 * Abstract:    Set the maximum ascent and descent for a verse on a score.
 *
 * Returns:     void
 *
 * Description: This function, using the given baseline, sets the relative
 *		vertical coords of each syllable in the verse on this score.
 *		If there are any nonnull syllables, it calls a function to
 *		continue underscores if need be.
 */

static void
setsylvert(start_p, s, place, v, baseline)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */
int place;			/* above, below, or between? */
int v;				/* verse number */
double baseline;		/* baseline of a verse of syllables */

{
	struct MAINLL *mainll_p;/* point along main linked list */
	struct STAFF *staff_p;	/* point at a staff structure */
	struct GRPSYL *gs_p;	/* point at a syllable */
	struct MAINLL *laststaff_p; /* point last staff that has a syllable */
	struct GRPSYL *lastgs_p;/* point at last nonnull syllable in a verse */
	int k;			/* loop variable */


	/*
	 * Loop through all these syllables as before, setting their relative
	 * vertical coords.
	 */
	lastgs_p = 0;		/* set later to last nonnull syl, if exists */
	laststaff_p = 0;	/* set later to staff containing lastgs_p */

	/*
	 * On entry to the caller, dolyrics, the SSV state is set for the start
	 * of this score.   dolyrics doesn't change that.  But cont_extender
	 * needs the SSV state as during the first measure of the following
	 * score.
	 */
	savessvstate();

	for (mainll_p = start_p->next; mainll_p != 0 && mainll_p->str
				!= S_FEED; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		if (mainll_p->str != S_STAFF ||
				mainll_p->u.staff_p->staffno != s)
			continue;

		/* found a STAFF of the number we're dealing with */
		staff_p = mainll_p->u.staff_p;

		/*
		 * See if this verse is present in this staff,
		 * and if so, loop through it.
		 */
		for (k = 0; k < staff_p->nsyllists; k++) {

			if (staff_p->sylplace[k] == place &&
					staff_p->syls_p[k]->vno == v) {

				for (gs_p = staff_p->syls_p[k]; gs_p != 0;
						gs_p = gs_p->next) {

					if (gs_p->syl == 0) {
						continue;
					}

					gs_p->c[RY] = baseline;

					gs_p->c[RN] = baseline
						+ strascent(gs_p->syl);

					gs_p->c[RS] = baseline
						- strdescent(gs_p->syl);

					/* remember last nonnull syl */
					if (gs_p->syl[0] != '\0') {
						lastgs_p = gs_p;
						laststaff_p = mainll_p;
					}
				}
			}
		}
	} /* for every MLL stucture in score */

	/*
	 * At this point, if this score has any nonnull syllables for
	 * this verse, lastgs_p points at the last one and laststaff_p
	 * points at its STAFF.  If that last syllable ends in '_' or
	 * '-', we may need to continue this character onto the next
	 * score, so call a function to do that.
	 */
	if (lastgs_p != 0 && has_extender(lastgs_p->syl))
		cont_extender(laststaff_p, place, v);

	restoressvstate();
}

/*
 * Name:        dopedal()
 *
 * Abstract:    Set a rectangle for pedal marks, if there are any.
 *
 * Returns:     void
 *
 * Description: This function puts rectangle(s) into Rectab for pedal marks, if
 *		there are any on this score.  It also sets their relative
 *		vertical coordinates.
 */

static void
dopedal(start_p, s)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */

{
	struct MAINLL *mainll_p;	/* loop through main linked list */
	struct STUFF *stuff_p;		/* point along a STUFF list */
	float protrude;			/* farthest protrusion of rectangle */
	float lowpoint;			/* the lowest any mark goes */
	float asc;			/* ascent of a pedal mark */
	float hi;			/* height of a pedal mark */
	int k;				/* loop variable */
	int dist_usage;			/* to be used by the rectangle */
	float reqdist;			/* requested dist */
	float offset;			/* amount to move marks due to dist */
	/* have we seen an aligned P_LINE pedal mark of the following kinds? */
	int ped_cont_align;		/* aligned pedal continuation */
	int ped_non_cont;		/* pedal mark other than continuation */


	debug(32, "dopedal file=%s line=%d s=%d", start_p->inputfile,
			start_p->inputlineno, s);
	/*
	 * Find the greatest protrusion of any currently existing rectangle.
	 */
	protrude = 0;
	for (k = 0; k < Reclim; k++) {
		if (Rectab[k].s < protrude) {
			protrude = Rectab[k].s;
		}
	}

	lowpoint = 0;
	ped_cont_align = NO;
	ped_non_cont = NO;
	dist_usage = SD_NONE;
	reqdist = 0.0;		/* keep lint happy */

	/* save SSVs so that we can restore them after this loop */
	savessvstate();

	/*
	 * Loop through this score's part of the MLL.  Whenever we find a
	 * structure for this staff (another measure), loop through its
	 * STUFF list, setting coords for each pedal mark.
	 */
	for (mainll_p = start_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}


		if (mainll_p->str != S_STAFF ||
				mainll_p->u.staff_p->staffno != s)
			continue;

		for (stuff_p = mainll_p->u.staff_p->stuff_p;
				stuff_p != 0; stuff_p = stuff_p->next) {

			if (stuff_p->stuff_type != ST_PEDAL)
				continue;

			/*
			 * Whichever pedal character this is, always use
			 * C_BEGPED if pedstyle is P_LINE and the "Ped." string
			 * for the other cases.  For the former, all three
			 * characters are the same height; and for the latter,
			 * this string is taller than the "*".  This also
			 * handles the pedal continuation situation.
			 */
			stuff_p->c[RN] = protrude;
			if (svpath(s, PEDSTYLE)->pedstyle == P_LINE) {
				asc = ascent(FONT_MUSIC, DFLT_SIZE, C_BEGPED);
				hi  = height(FONT_MUSIC, DFLT_SIZE, C_BEGPED);

				if (stuff_p->string == 0) {
					if (stuff_p->aligntag != NOALIGNTAG) {
						ped_cont_align = YES;
					}
				} else {
					ped_non_cont = YES;
				}
			} else { /* P_PEDSTAR or P_ALTPEDSTAR */
				asc = strascent(Ped_start);
				hi  = strheight(Ped_start);
			}
			if (stuff_p->all) {
				asc *= Score.staffscale;
				hi  *= Score.staffscale;
			} else {
				asc *= Staffscale;
				hi  *= Staffscale;
			}
			stuff_p->c[RY] = protrude - asc;
			stuff_p->c[RS] = protrude - hi;

			if (stuff_p->c[RS] < lowpoint)
				lowpoint = stuff_p->c[RS];

			/* if user request a dist, remember that */
			if (stuff_p->dist_usage != SD_NONE) {
				if (dist_usage != SD_NONE &&
						stuff_p->dist != reqdist) {
					l_warning(stuff_p->inputfile,
					stuff_p->inputlineno,
					"mark 'dist' overrides 'dist' used by earlier aligned mark");
				}
				/* update, in case it is new or changed */
				reqdist = stuff_p->dist;

				/* remember the highest level */
				if (dist_usage == SD_NONE ||
				    stuff_p->dist_usage == SD_FORCE) {
					dist_usage = stuff_p->dist_usage;
				}
			}
		}
	}

	restoressvstate();

	/*
	 * If we found pedal mark(s), we need to make rectangle(s) for them.
	 * When alignped is y, we put one big rectangle in Rectab, spanning
	 * the width of the page.  When alignped is n, we normally call
	 * domiscstuff(), to handle the pedal marks separately, similar to
	 * other STUFF.  However, in the P_LINE case, if there was only an
	 * aligned pedal continuation mark, we treat is like alignped = y.
	 */
	if (lowpoint < 0) {
		if (svpath(s, ALIGNPED)->alignped == NO &&
		    ! (ped_cont_align == YES && ped_non_cont == NO)) {
			domiscstuff(start_p, s, PL_BELOW, 1L << MK_PEDAL);
			return;
		}

		/* alignped is y or we have only ped continuation */

		/* if user wanted "dist", we may have to revise the positions */
		if (dist_usage != SD_NONE) {
			/* convert to inches from center of staff */
			reqdist = -(halfstaffhi(s) + Stepsize * reqdist);
			offset = reqdist - protrude;
		}
		if (dist_usage == SD_FORCE ||
		   (dist_usage == SD_MIN && offset < 0)) {

			for (mainll_p = start_p->next; mainll_p != 0 &&
					mainll_p->str != S_FEED;
					mainll_p = mainll_p->next) {

				if (mainll_p->str != S_STAFF ||
				    mainll_p->u.staff_p->staffno != s) {
					continue;
				}

				for (stuff_p = mainll_p->u.staff_p->stuff_p;
						stuff_p != 0;
						stuff_p = stuff_p->next) {

					if (stuff_p->stuff_type != ST_PEDAL) {
						continue;
					}
					stuff_p->c[RN] += offset;
					stuff_p->c[RY] += offset;
					stuff_p->c[RS] += offset;
				}
			}
			protrude = reqdist;
		}

		Rectab[Reclim].n = protrude;
		Rectab[Reclim].s = lowpoint;
		Rectab[Reclim].w = 0;
		Rectab[Reclim].e = EFF_PG_WIDTH;

		inc_reclim();
	}
}

/*
 * Name:        doendings()
 *
 * Abstract:    Set up rectangles and vert coords for ending marks.
 *
 * Returns:     void
 *
 * Description: This function puts into Rectab rectangles for ending marks.
 *		Also, MARKCOORD structures get linked to BARs for them.
 */

static void
doendings(start_p, s)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */

{
	struct MAINLL *mainll_p;/* point along main linked list */
	struct BAR *bar_p;	/* point at a bar or pseudobar on this score */


	debug(32, "doendings file=%s line=%d s=%d", start_p->inputfile,
			start_p->inputlineno, s);
	/* if endings are not to be drawn over this staff, get out */
	if (has_ending(s) == NO)
		return;

	/* point at pseudobar in clefsig that immediately follows this feed */
	mainll_p = start_p->next;
	bar_p = mainll_p->u.clefsig_p->bar_p;

	/*
	 * If an ending starts at the pseudobar, or is continuing on from the
	 * previous score, handle it, along with any following continguous ones.
	 */
	if (bar_p->endingloc != NOITEM) {
		/*
		 * Search forward for the end of this ending (or following
		 * contiguous ones), or the end of the score, whichever comes
		 * first.
		 */
		while ( ! (mainll_p->str == S_BAR &&
					mainll_p->u.bar_p->endingloc == ENDITEM)
				&& mainll_p->str != S_FEED) {

			mainll_p = mainll_p->next;
		}

		/* handle ending(s) from start to this bar or feed */
		storeend(start_p, mainll_p, s);

		/* if feed, there's nothing more to look for */
		if (mainll_p->str == S_FEED)
			return;

		/* point after this bar at end of this ending(s) */
		mainll_p = mainll_p->next;
	}

	/*
	 * Search the rest of the score for contiguous groups of endings.
	 */
	while (mainll_p != 0 && mainll_p->str != S_FEED) {

		/* find another bar; return if there aren't any more */
		while (mainll_p != 0 && mainll_p->str != S_BAR &&
					mainll_p->str != S_FEED)
			mainll_p = mainll_p->next;
		if (mainll_p == 0 || mainll_p->str == S_FEED)
			return;

		/*
		 * We found another bar.  If it isn't associated with an
		 * ending, point beyond it and continue to go look for the
		 * next bar.
		 */
		if (mainll_p->u.bar_p->endingloc == NOITEM) {
			mainll_p = mainll_p->next;
			continue;
		}

		/*
		 * This bar is the start of an ending.  Search forward for the
		 * end of this ending (or following contiguous ones), or the
		 * end of the score, whichever comes first.
		 */
		start_p = mainll_p;
		while ( ! (mainll_p->str == S_BAR &&
					mainll_p->u.bar_p->endingloc == ENDITEM)
				&& mainll_p->str != S_FEED) {

			mainll_p = mainll_p->next;
		}

		/* handle ending(s) from start to this bar or feed */
		storeend(start_p, mainll_p, s);

		/* if feed, there's nothing more to look for */
		if (mainll_p->str == S_FEED)
			return;

		/* point after this bar at end of this ending */
		mainll_p = mainll_p->next;
	}
}

/*
 * Name:        storeend()
 *
 * Abstract:    Set up rectangles and vert coords for contiguous endings.
 *
 * Returns:     void
 *
 * Description: This function is given the starting and ending bars of a group
 *		of continguous ending marks on a staff.  The starting "bar"
 *		may be the pseudobar at the start of the score; and the ending
 *		bar may be the end of the score.  This function applies stackit
 *		to them as a unit.  It adds another rectangle to Rectab to
 *		prevent anything later from getting in between the ending(s)
 *		and the staff.  Then, for the starting bar of each ending in
 *		the group, it allocates a MARKCOORD structure.
 */

static void
storeend(start_p, end_p, s)

struct MAINLL *start_p;		/* the start of these ending(s) */
struct MAINLL *end_p;		/* the end of these ending(s) */
int s;				/* staff number */

{
	struct MAINLL *mainll_p;/* point along main linked list */
	struct BAR *bar_p;	/* point at a bar or pseudobar on this score */
	struct MARKCOORD *mark_p; /* we allocate these for bars to point at */
	float west, east;	/* extremities of group of ending(s) */
	float south;		/* their bottom boundary */


	/*
	 * Find the west and east boundaries of the ending(s).
	 */
	if (start_p->str == S_FEED)
		west = start_p->next->u.clefsig_p->bar_p->c[AX]; /* pseudobar */
	else
		west = start_p->u.bar_p->c[AX];		/* normal bar */

	if (end_p->str == S_FEED)
		east = EFF_PG_WIDTH - eff_rightmargin(end_p); /* end of score */
	else
		east = end_p->u.bar_p->c[AX];		/* normal bar */

	/* make a rectangle out of the ending(s) and find where they go */
	south = stackit(west, east, ENDINGHEIGHT, (double)0.0, PL_ABOVE);

	/*
	 * Superimpose another rectangle on top of the one stackit put there;
	 * one that reaches down to the staff.  This ensures that nothing later
	 * will get between the ending(s) and the staff.
	 */
	Rectab[Reclim].n = south + ENDINGHEIGHT;
	Rectab[Reclim].s = 0;
	Rectab[Reclim].e = east;
	Rectab[Reclim].w = west;
	inc_reclim();

	/*
	 * If the pseudobar has an ending, calloc a markcoord structure and put
	 * it in the pseudobar's linked list of them.
	 */
	if (start_p->str == S_FEED) {
		bar_p = start_p->next->u.clefsig_p->bar_p;
		CALLOC(MARKCOORD, mark_p, 1);
		mark_p->next = bar_p->ending_p;
		bar_p->ending_p = mark_p;
		mark_p->staffno = (short)s;
		mark_p->ry = south;
	}

	/*
	 * Loop through this part of the score.  Wherever there is a bar that
	 * is the start of an ending, calloc a markcoord structure and put it
	 * in the bar's linked list of them.
	 */
	for (mainll_p = start_p; mainll_p != end_p; mainll_p = mainll_p->next) {
		if (mainll_p->str != S_BAR)
			continue;
		bar_p = mainll_p->u.bar_p;
		if (bar_p->endingloc != STARTITEM)
			continue;
		CALLOC(MARKCOORD, mark_p, 1);
		mark_p->next = bar_p->ending_p;
		bar_p->ending_p = mark_p;
		mark_p->staffno = (short)s;
		mark_p->ry = south;
	}
}

/*
 * Name:        dorehears()
 *
 * Abstract:    Set up rectangles and vert coords for rehearsal marks.
 *
 * Returns:     void
 *
 * Description: This function puts into Rectab rectangles for rehearsal marks.
 *		Also, MARKCOORD structures get linked to BARs for them.
 */

static void
dorehears(start_p, s)

struct MAINLL *start_p;		/* FEED at the start of this score */
int s;				/* staff number */

{
	struct MAINLL *mainll_p;/* point along main linked list */
	struct BAR *bar_p;	/* point at a bar or pseudobar on this score */
	struct MARKCOORD *mark_p; /* we allocate these for bars to point at */
	float west, east;	/* of a rehearsal mark */
	float south;		/* of a rehearsal mark */
	float height;		/* of a rehearsal mark */
	float dist;		/* distance from center of staff */
	int dopseudo;		/* do the pseudobar's rehearsal mark? */
	char *reh_string;	/* string for the reh mark */


	debug(32, "dorehears file=%s line=%d s=%d", start_p->inputfile,
			start_p->inputlineno, s);
	/* if rehearsal marks are not to be drawn over this staff, get out */
	if (has_ending(s) == NO)
		return;

	/* point at pseudobar in clefsig that immediately follows this feed */
	mainll_p = start_p->next;
	bar_p = mainll_p->u.clefsig_p->bar_p;

	/* if there's a rehearsal mark at the pseudobar, note that fact */
	if (bar_p->reh_type != REH_NONE)
		dopseudo = YES;
	else
		dopseudo = NO;

	/* save SSVs so that we can restore them after this loop */
	savessvstate();

	/*
	 * Loop through the score, dealing with the pseudobar (if it has a
	 * rehearsal mark), and all real bars that have a rehearsal mark.
	 */
	for ( ; mainll_p != 0 && mainll_p->str != S_FEED;
				mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		if (dopseudo == YES || (mainll_p->str == S_BAR &&
				     mainll_p->u.bar_p->reh_type != REH_NONE)) {
			if (dopseudo == YES)
				dopseudo = NO;
			else
				bar_p = mainll_p->u.bar_p;

			/*
			 * Find the size of the rehearsal label, including 6
			 * more points to allow for the box around it.  Make
			 * its first character be centered over the bar line.
			 * Place it by using stackit.
			 */
			reh_string = get_reh_string(bar_p, s);
			height = strheight(reh_string);
			west = bar_p->c[AX] - left_width(reh_string);
			east = west + strwidth(reh_string);

			if (bar_p->dist_usage == SD_NONE) {
				/* get the usual dist */
				dist = svpath(s, DIST)->dist;
			} else {
				/* override with this bar's dist */
				dist = bar_p->dist;
			}
			/* convert to inches from center of staff */
			dist = halfstaffhi(s) + STEPSIZE * dist;

			if (bar_p->dist_usage == SD_FORCE) {
				/*
				 * The user is forcing this dist, so don't
				 * stack; just put it there.
				 */
				south = dist;
				Rectab[Reclim].n = south + height;
				Rectab[Reclim].s = south;
				Rectab[Reclim].e = east;
				Rectab[Reclim].w = west;
				inc_reclim();
			} else {
				/* stack the usual way */
				south = stackit(west, east, height, dist,
						PL_ABOVE);
			}

			/*
			 * Allocate and link a MARKCOORD, and put the necessary
			 * info in it.
			 */
			CALLOC(MARKCOORD, mark_p, 1);
			mark_p->next = bar_p->reh_p;
			bar_p->reh_p = mark_p;
			mark_p->staffno = (short)s;
			mark_p->ry = south + strdescent(reh_string);
		}
	}

	restoressvstate();
}

/*
 * Name:        stackit()
 *
 * Abstract:    Place a rectangle and add it to Rectab.
 *
 * Returns:     south boundary of the new rectangle
 *
 * Description: This function puts the given rectangle into Rectab.  It is put
 *		as close to the staff or baseline as is possible without
 *		overlapping rectangles already in Rectab, and without letting
 *		it get any closer to the staff/baseline than "dist" STEPSIZE.
 */

static double
stackit(west, east, height, dist, place)

double west;			/* west edge of the new rectangle */
double east;			/* east edge of the new rectangle */
double height;			/* height of the new rectangle */
double dist;			/* min dist from item to center line of staff*/
int place;			/* above, below, or between? */

{
	float north, south;	/* trial boundaries for new rectangle */
	int try;		/* which element of Rectab to try */
	int overlap;		/* does our rectangle overlap existing ones? */
	int j;			/* loop variable */


	/*
	 * For each rectangle in Rectab, decide whether (based on
	 * its horizontal coords) it could possibly overlap with our
	 * new rectangle.  If it's totally left or right of ours, it
	 * can't.  We allow a slight overlap (FUDGE) so that round
	 * off errors don't stop us from packing things as tightly
	 * as possible.
	 */
	for (j = 0; j < Reclim; j++) {
		if (Rectab[j].w + FUDGE > east ||
		    Rectab[j].e < west + FUDGE) {
			Rectab[j].relevant = NO;
		} else {
			Rectab[j].relevant = YES;
		}
	}

	/*
	 * Set up first trial position for this rectangle:  "dist" inches
	 * away from the center line of the staff.  For "between", it always
	 * starts at the baseline.
	 */
	north = south = 0.0;	/* prevent useless 'used before set' warning */
	switch (place) {
	case PL_BELOW:
		/* work downward from staff, allowing "dist" distance */
		north = -dist;
		south = north - height;
		break;
	case PL_ABOVE:
		/* work upward from staff, allowing "dist" distance */
		south = dist;
		north = south + height;
		break;
	case PL_BETWEEN:
		/* work upward from baseline */
		south = 0;
		north = height;
		break;
	}

	/*
	 * Mark the "tried" field for all relevant rectangles.  This says
	 * whether we have already tried using their boundaries for positioning
	 * our rectangle.  Any rectangle that is closer to the staff/baseline
	 * than we want to allow, we mark as if we have tried it already.
	 */
	for (j = 0; j < Reclim; j++) {
		if (Rectab[j].relevant == YES) {
			if ((place == PL_BELOW && Rectab[j].s > north) ||
			    (place != PL_BELOW && Rectab[j].n < south)) {
				Rectab[j].tried = YES;
			} else {
				Rectab[j].tried = NO;
			}
		}
	}

	/*
	 * Keep trying positions for this rectangle, working outwards from the
	 * first trial position.  When we find one that doesn't overlap an
	 * existing rectangle, break.  This has to succeed at some point, at
	 * at the outermost rectangle position if not earlier.
	 */
	for (;;) {
		overlap = NO;
		for (j = 0; j < Reclim; j++) {
			/* ignore ones too far east or west */
			if (Rectab[j].relevant == NO) {
				continue;
			}

			/* if all south or north, okay; else overlap */
			if (Rectab[j].s + FUDGE <= north &&
			    Rectab[j].n >= south + FUDGE) {
				overlap = YES;
				break;
			}
		}

		/* if no rectangle overlapped, we found a valid place */
		if (overlap == NO)
			break;

		/*
		 * Something overlapped, so we have to try again.  Find the
		 * innermost relevant outer rectangle boundary that hasn't been
		 * tried already, to use as the next trial position for our
		 * rectangle's inner boundary.
		 */
		try = -1;
		for (j = 0; j < Reclim; j++) {
			/* ignore ones too far east or west */
			if (Rectab[j].relevant == NO || Rectab[j].tried == YES){
				continue;
			}

			/*
			 * If this is the first relevant one we haven't tried,
			 * or if this is farther in than the innermost so far,
			 * save it as being the new innermost so far.
			 */
			if (place == PL_BELOW) {
				if (try == -1 || Rectab[j].s > Rectab[try].s) {
					try = j;
				}
			} else {
				if (try == -1 || Rectab[j].n < Rectab[try].n) {
					try = j;
				}
			}
		}

		if (try == -1)
			pfatal("bug in stackit()");

		/*
		 * Mark this one as having been tried (for next time around, if
		 * necessary).  Set new trial values for north and south of our
		 * rectangle.
		 */
		Rectab[try].tried = YES;
		if (place == PL_BELOW) {
			north = Rectab[try].s;
			south = north - height;
		} else {
			south = Rectab[try].n;
			north = south + height;
		}

	} /* end of while loop trying positions for this rectangle */

	/*
	 * We found the correct position for the new rectangle.  Enter it
	 * into Rectab.
	 */
	Rectab[Reclim].n = north;
	Rectab[Reclim].s = south;
	Rectab[Reclim].e = east;
	Rectab[Reclim].w = west;

	inc_reclim();

	return (south);
}

/*
 * Name:        multistackit()
 *
 * Abstract:    Place multiple rectangles in unison and update them in Rectab.
 *
 * Returns:     void
 *
 * Description: This function completes the placement of the last "nrect"
 *		rectangles in Rectab.  These represent STUFFs that are to be
 *		aligned horizontally.  The east and west are already set in
 *		each rectangle, and the north fields are (temporarily) set to
 *		be the heights.  This function sets the north and south fields
 *		to the correct values.  For "above" and "between", all the
 *		souths will be the same; for "below" all the norths will.
 *		All rectangles are placed as close to the staff or baseline as
 *		is possible without overlapping rectangles already in Rectab,
 *		and without letting them get any closer to the staff than
 *		"dist" STEPSIZE.  (Between, which stacks rectangles above a
 *		baseline, doesn't use dist.)
 */

static void
multistackit(nrect, dist, place)

int nrect;			/* number of rectangles to place */
double dist;			/* min dist to center line of staff/baseline */
int place;			/* above, below, or between? */

{
	float north, south;	/* outermost boundaries of the new rectangles*/
	float move;		/* how far to move for the next trial */
	float height;		/* height of one rectangle */
	int try;		/* which element of Rectab to try */
	int overlap;		/* do our rectangles overlap existing ones? */
	int oidx, nidx;		/* loop variables for old and new rectangles */
	int ofirst, nfirst;	/* idx to first old and new rectangles */
	int olimit, nlimit;	/* idx beyond last old and new rectangles */


	/*
	 * Set loop boundaries for looping through the old (existing)
	 * rectangles, and the new ones (those that are being placed).
	 */
	ofirst = 0;
	olimit = Reclim - nrect;
	nfirst = Reclim - nrect;
	nlimit = Reclim;

	/*
	 * For each old rectangle in Rectab, decide whether (based on its
	 * horizontal coords) it could possibly overlap with any of our new
	 * rectangles.  If it's totally left or right of each of ours, it
	 * can't.  We allow a slight overlap (FUDGE) so that round off errors
	 * don't stop us from packing things as tightly as possible.
	 */
	for (oidx = ofirst; oidx < olimit; oidx++) {
		Rectab[oidx].relevant = NO;
		for (nidx = nfirst; nidx < nlimit; nidx++) {
			if (Rectab[oidx].w + FUDGE < Rectab[nidx].e &&
			    Rectab[oidx].e > Rectab[nidx].w + FUDGE) {
				Rectab[oidx].relevant = YES;
				break;
			}
		}
	}

	/*
	 * Set up first trial position for these rectangles:  "dist" inches
	 * away from the center line of the staff.  For "between", it always
	 * starts at the baseline.
	 */
	north = south = 0.0;	/* remember the farthest distances */
	for (nidx = nfirst; nidx < nlimit; nidx++) {
		height = Rectab[nidx].n;
		switch (place) {
		case PL_BELOW:
			/* work downward from staff, allowing "dist" distance */
			Rectab[nidx].n = -dist;
			Rectab[nidx].s = -dist - height;
			break;
		case PL_ABOVE:
			/* work upward from staff, allowing "dist" distance */
			Rectab[nidx].s = dist;
			Rectab[nidx].n = dist + height;
			break;
		case PL_BETWEEN:
			/* work upward from baseline */
			Rectab[nidx].s = 0.0;
			Rectab[nidx].n = height;
			break;
		}
		/* remember the farthest north and south of any new rectangle*/
		if (Rectab[nidx].n > north) {
			north = Rectab[nidx].n;
		}
		if (Rectab[nidx].s < south) {
			south = Rectab[nidx].s;
		}
	}

	/*
	 * Mark the "tried" field for all relevant rectangles.  This says
	 * whether we have already tried using their boundaries for positioning
	 * our rectangles.  Any rectangle that is closer to the staff/baseline
	 * than we want to allow, we mark as if we have tried it already.
	 */
	for (oidx = ofirst; oidx < olimit; oidx++) {
		if (Rectab[oidx].relevant == YES) {
			if ((place == PL_BELOW && Rectab[oidx].s > north) ||
			    (place != PL_BELOW && Rectab[oidx].n < south)) {
				Rectab[oidx].tried = YES;
			} else {
				Rectab[oidx].tried = NO;
			}
		}
	}

	/*
	 * Keep trying positions for the rectangles, working outwards from the
	 * first trial position.  When we find one that doesn't overlap an
	 * existing rectangle, break.  This has to succeed at some point, at
	 * at the outermost rectangle position if not earlier.
	 */
	for (;;) {
		overlap = NO;
		for (oidx = ofirst; oidx < olimit; oidx++) {
			/* ignore ones east or west of the new rectangles */
			if (Rectab[oidx].relevant == NO) {
				continue;
			}

			/*
			 * Test for overlap with each new rectangle.  Unlike in
			 * stackit(), we have to check all four directions,
			 * because "relevant == YES" doesn't guarantee
			 * vertical overlap with the same rectangle that
			 * horizontally overlaps.  The rectangles may have
			 * different heights.
			 */
			for (nidx = nfirst; nidx < nlimit; nidx++) {
				if (Rectab[oidx].s + FUDGE <= Rectab[nidx].n &&
				    Rectab[oidx].n >= Rectab[nidx].s + FUDGE &&
				    Rectab[oidx].w + FUDGE <= Rectab[nidx].e &&
				    Rectab[oidx].e >= Rectab[nidx].w + FUDGE) {
					overlap = YES;
					break;
				}
			}
			if (overlap == YES) {
				break;
			}
		}

		/* if no rectangle overlapped, we found a valid place */
		if (overlap == NO) {
			break;
		}

		/*
		 * Something overlapped, so we have to try again.  Find the
		 * innermost relevant outer rectangle boundary that hasn't been
		 * tried already, to use as the next trial position for our
		 * rectangles' inner boundaries.
		 */
		try = -1;
		for (oidx = ofirst; oidx < olimit; oidx++) {
			/* ignore ones that are sure not to overlap */
			if (Rectab[oidx].relevant == NO ||
			    Rectab[oidx].tried == YES) {
				continue;
			}

			/*
			 * If this is the first relevant one we haven't tried,
			 * or if this is farther in than the innermost so far,
			 * save it as being the new innermost so far.
			 */
			if (place == PL_BELOW) {
				if (try == -1 ||
				    Rectab[oidx].s > Rectab[try].s) {
					try = oidx;
				}
			} else {
				if (try == -1 ||
				    Rectab[oidx].n < Rectab[try].n) {
					try = oidx;
				}
			}
		}

		if (try == -1) {
			pfatal("bug in multistackit()");
		}

		/*
		 * Mark this one as having been tried (for next time around, if
		 * necessary).  Set new trial values for north and south of our
		 * rectangle.
		 */
		Rectab[try].tried = YES;
		if (place == PL_BELOW) {
			move = Rectab[try].s - Rectab[nfirst].n;
		} else {
			move = Rectab[try].n - Rectab[nfirst].s;
		}
		for (nidx = nfirst; nidx < nlimit; nidx++) {
			Rectab[nidx].n += move;
			Rectab[nidx].s += move;
		}
		north += move;
		south += move;

	} /* end of while loop trying positions for the rectangles */
}
