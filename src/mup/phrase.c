
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

/* This file contains functions to determine the curves that make up
 * phrase marks, ties and slurs.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/* distance to the top/bottom of a V-shape (indicating a bend) relative to
 * the line connecting the endpoints of the V */
#define V_HEIGHT (2.7 * Stepsize)

/* We'd normally want curves to begin and end (in the x direction) exactly in
 * the middle of their note. But if one curve ends and another begins on
 * the same note, the curve endpoints would collide, which could look bad.
 * So we always offset the endpoints by a tiny amount (ends end a little
 * west of center, and beginnings begin a little east) so they don't touch.
 * This is the amount they are shifted from center.
 */
#define XOFFSET4CURVE	(0.75 * Stdpad)

/* Curves must be at least this far away from notes */
#define CLEARANCE	(3.0 * Stdpad)

/* We try to put phrase endpoints beside the stem when appropriate.
 * This tell how far left/right of the stem we go.
 */
#define SIDE_OFFSET (Stepsize)

/* If we tried to go beside the stem, but then discover we have to move
 * the endpoint so far vertically to get a nice looking
 * curve that it gets near or beyond the end of the stem, it looks better to
 * fall back to placing by the end of the stem. This tells us how much to
 * adjust x and y when we have to do that.
 */
#define SIDE_X_ADJ	(SIDE_OFFSET - XOFFSET4CURVE)
#define SIDE_Y_ADJ	(3.0 * Stepsize)

/* If the absolute value of the sin of the angle formed
 * between a horizontal line and a line drawn between endpoints of a phrase
 * curve is greater than this, we consider it "steep" and do some special
 * things to try to make it look better.
 */
#define STEEP_SIN  (0.4)

/* How long a segment from endpoint to make for flattening long curves */
#define MIN_FLATTEN_SEG	(5 * Stepsize)
#define MAX_FLATTEN_SEG	(12 * Stepsize)

/* How much to adjust x from note's AX on anend note tie/slur */
#define COLLIDING_TS_ADJUST	Stdpad
#define NONCOLLIDING_TS_ADJUST  (-2.0 * Stdpad)

/* try_bulge() is called lots of times in a row with mostly the same values,
 * and it needs lots of values, so it is convenient to put them in a struct,
 * and just pass a pointer to it */
struct TRYBULGE {
	struct MAINLL *mll_p;		/* STUFF hangs off here */
	struct GRPSYL *begin_gs_p;	/* group at left end of curve */
	struct GRPSYL *end_gs_p;	/* group at right end of curve */
	int place;			/* PL_*  */
	struct CRVLIST *curvelist_p;	/* points to beginning of curve */
	struct CRVLIST *endlist_p;	/* points to end of curve */
	double xlen;			/* distance to midpoint */
	double sign;			/* 1 or -1 for direction */
	double length;			/* between endpoints */
	double sintheta;		/* for rotation from horizontal */
	double costheta;
	double minbulge;		/* first bulge factor to try */
	double maxbulge;		/* last bulge to try before giving up */
	int beg_beside;			/* YES if begin is beside stem */
	int end_beside;			/* YES if end is beside stem */
	double beg_side_thresh;		/* when to give up on putting beside */
	double end_side_thresh;		/* when to give up on putting beside */
	double left_protrusion;		/* stick out near left end */
	double right_protrusion;		/* stick out near right end */
	int trials;			/* how many attempts so far */
};

static int nowhere_slide P((struct STUFF *stuff_p));
static void do_nowhere P((struct STUFF *stuff_p, double x1, double y1,
		double x2, double y2));
static void curve_points P((struct MAINLL *mll_p, struct STUFF *stuff_p,
		int is_phrase));
static double inner_adj P((struct GRPSYL *gs_p, struct NOTE *note_p,
		double y_adj, int place));
static int is_stemside P((struct GRPSYL *gs_p, int place));
static int move_side2end P((struct TRYBULGE *try_p));
static void set_values P((struct TRYBULGE *try_p));
static void set_try_params P((struct TRYBULGE *try_p));
static double bulge_value P((double length, double x1, double y1, double x2, double y2) );
static double stick_out P((struct TRYBULGE *info_p));
static void set_protrusion P((struct TRYBULGE *try_p, double x, double protrusion));
static int try_bulge P((struct TRYBULGE *info_p));
static double tieslurx P((struct GRPSYL *gs_p, struct NOTE *note_p, int place));
static struct MAINLL *next_staff P((int staff, struct MAINLL *mll_p));
static int try_redo_steep P((struct TRYBULGE *try_p));
static void redo_steep P((struct CRVLIST *first_p, struct CRVLIST *last_p,
		int place));
static void final_touches P((struct MAINLL *mll_p, struct GRPSYL *begin_gs_p,
		struct GRPSYL *end_gs_p, struct CRVLIST *crvlist_p, int place));
static struct CRVLIST *flatten_long_curve P((struct TRYBULGE *info_p));
static void rotate_curve P((struct CRVLIST *point_p, double costheta,
		double sintheta));
static double eff_tupext P((struct GRPSYL * gs_p, struct STAFF *staff_p, int side));
static int bulge_direction P((struct MAINLL *mll_p, struct GRPSYL *gs1_p,
		int note_index, int curveno));
static double left_endts_adj P((struct MAINLL *mll_p, struct GRPSYL *gs_p,
		struct NOTE *note_p));
static double right_endts_adj P((struct NOTE *note_p));


/* figure out what points are needed for a phrase mark */
/* attach a linked list of x,y coordinates that show where to draw the curve.
 * The curve will be out of the way of any groups within the phrase. */

void
phrase_points(mll_p, stuff_p)

struct MAINLL *mll_p;		/* MAINLL that stuff_p hangs off of */
struct STUFF *stuff_p;		/* info about the phrase mark */

{
	curve_points(mll_p, stuff_p, YES);
}


/* figure out what points are needed for a tie or slur mark */

void
tieslur_points(mll_p, stuff_p)

struct MAINLL *mll_p;		/* MAINLL that stuff_p hangs off of */
struct STUFF *stuff_p;		/* info about the phrase mark */

{
	/* if slide to/from nowhere in particular, do that */
	if (nowhere_slide(stuff_p) == YES) {
		return;
	}

	curve_points(mll_p, stuff_p, NO);
}


/* determine the 3 points that define a V_shaped bend indicator on the tabnote
 * staff associated with a tab staff, and put them in the stuff crvlist */

void
bend_points(mll_p, stuff_p)

struct MAINLL *mll_p;
struct STUFF *stuff_p;

{
	struct CRVLIST *first_point_p, *last_point_p;	/* the beginning
				 * and end points of the curve */
	struct CRVLIST *mid_point_p;		/* middle of the V-shape */
	struct CRVLIST *one2discard_p;		/* a point to discard */
	double midx, midy;			/* midpoint between the ends */
	double v_height;			/* V_HEIGHT, or less than
						 * that for narrow V's */
	double xlen;				/* to help find v_height */
	double slope;				/* of perpendicular line from
						 * (midx, midy) to the point
						 * of the V, v_height away. */


	/* first figure everything out as if it were a normal slur */
	curve_points(mll_p, stuff_p, NO);

	/* Now make into V-shaped curve.
	 * First throw away the inner points that we had found.
	 * It's a bit unfortunate to do all that work, then throw it
	 * away, but the curve_point() function that finds all the points
	 * also does lots of other good things that we want, so rather than
	 * make it more complicated than it already is by having it know
	 * about bends, we just save the things it did that help us here.
	 */
	first_point_p = stuff_p->crvlist_p;
	for (last_point_p = first_point_p->next;
			last_point_p->next != (struct CRVLIST *) 0;  ) {
		one2discard_p = last_point_p;
		last_point_p = last_point_p->next;
		FREE(one2discard_p);
	}

	/* get a midpoint struct and stitch it into the list */
	MALLOC(CRVLIST, mid_point_p, 1);
	first_point_p->next = mid_point_p;
	last_point_p->prev = mid_point_p;
	mid_point_p->prev = first_point_p;
	mid_point_p->next = last_point_p;

	/* find the midpoint of the line between the endpoints */
	midx = (last_point_p->x + first_point_p->x) / 2.0;
	midy = (last_point_p->y + first_point_p->y) / 2.0;

	/* get height. Use V_HEIGHT, except adjust for narrow V's */
	xlen = fabs(last_point_p->x - first_point_p->x);
	if (xlen < 2.0 * V_HEIGHT) {
		v_height = 0.35 * V_HEIGHT;
	}
	else if (xlen < 3.5 * V_HEIGHT) {
		v_height = 0.65 * V_HEIGHT;
	}
	else {
		v_height = V_HEIGHT;
	}

	/* if the y's of the endpoints are equal or nearly so, finding the
	 * midpoint of the V is easy: the x is midx, and the y is midy offset
	 * by v_height in the appropriate direction */
	if (fabs(last_point_p->y - first_point_p->y) < 0.001) {
		mid_point_p->x = midx;
		mid_point_p->y = midy + v_height *
				(stuff_p->place == PL_ABOVE ? 1.0 : -1.0);
		return;
	}

	/* find the slope of the perpendicular */
	slope = (first_point_p->x - last_point_p->x) /
				(last_point_p->y - first_point_p->y);

	/* we want the length of the perpendicular line from (midx, midy)
	 * to the point at the top (or bottom) of the V to be v_height.
	 * Using that line as the hypotenuse of a triangle, we know that
	 * we can find the x and y relative to (midx, midy) by using
	 * Pythagorean   x^2 + y^2 = v_height^2. Furthermore, we calculated 
	 * the slope of the line earlier, and knowing that slope = y/x,
	 * we now solve 2 equations in 2 unknowns:
	 *	x^2 + y^2 = v_height^2
	 *	slope = y / x
	 * Rearranging the first equation and substituting (slope * x) for y:
	 *	x^2 + (slope * x)^2 = v_height^2
	 * solve for x:
	 * 	(1 + slope^2) * x^2 = v_height^2
	 *	x = sqrt( v_height^2 / (1 + slope^2))
	 * Then having found x, solve the second equation for y.
	 *	y = x * slope
	 * Adjust for being relative to (midx, midy) and for bend direction
	 * and slope direction, and we are done.
	 */
	mid_point_p->x = (sqrt((v_height * v_height) / (1.0 + (slope * slope))))
			* (first_point_p->y > last_point_p->y ? 1.0 : -1.0)
			* (stuff_p->place == PL_ABOVE ? 1.0 : -1.0);
	mid_point_p->y = (slope * mid_point_p->x) + midy;
	mid_point_p->x += midx;
}


/* determine the 2 points that define a line indicating a slide for a
 * tab or tabnote staff, and put the points in the stuff crvlist */

void
tabslur_points(mll_p, stuff_p)

struct MAINLL *mll_p;
struct STUFF *stuff_p;

{
	struct CRVLIST *curvelist_p;
	struct GRPSYL *beggrp_p;
	struct GRPSYL *endgrp_p;
	struct NOTE *begnote_p;
	struct NOTE *endnote_p;
	float slant;		/* 0, 1 or -1 to show slant direction */
	int acc1, acc2;		/* effective accidentals on the 2 groups,
				 * -2 to +2 */
	int n, st;		/* index through notelist and slurtolist */


	/* if slide to/from nowhere in particular, do that */
	if (nowhere_slide(stuff_p) == YES) {
		return;
	}

	/* find the end note */
	if (stuff_p->carryin == YES) {
		/* on carryin, beggrp_p is really the ending group,
		 * and the previous group is the real beggrp_p */
		endgrp_p = stuff_p->beggrp_p;
		endnote_p = stuff_p->begnote_p;
		beggrp_p = prevgrpsyl(stuff_p->beggrp_p, &mll_p);

		/* go through all the notes in the previous group,
		 * to find the one that has a slide to the note being
		 * carried into. If there is more than one, use the first
		 * one we find. */
		for (n = 0; n < beggrp_p->nnotes; n++) {
			for (st = 0; st < beggrp_p->notelist[n].nslurto; st++) {
				if (endnote_p->letter ==
				  beggrp_p->notelist[n].slurtolist[st].letter
				  && (is_tab_staff(endgrp_p->staffno) == YES
				  || endnote_p->octave ==
				  beggrp_p->notelist[n].slurtolist[st].octave)) {
					/* found the one sliding to us */
					break;
				}
			}
			if (st < beggrp_p->notelist[n].nslurto) {
				/* found it, so need to jump out */
				break;
			}
		}
		if (n == beggrp_p->nnotes) {
			pfatal("can't find note being slid from");
		}
		begnote_p = &(beggrp_p->notelist[n]);
	}
	else {
		beggrp_p = stuff_p->beggrp_p;
		begnote_p = stuff_p->begnote_p;
		if ((endgrp_p = nextgrpsyl(stuff_p->beggrp_p, &mll_p))
						== (struct GRPSYL *) 0) {
			pfatal("failed to find next group in tabslur_points");
		}
		endnote_p = find_matching_note (endgrp_p, mll_p,
				stuff_p->begnote_p->slurtolist
				[stuff_p->curveno].letter,
				-1,
				stuff_p->begnote_p->slurtolist
				[stuff_p->curveno].octave, "slide");

		if (endnote_p == (struct NOTE *) 0) {
			pfatal("failed to find endnote in tabslur_points");
		}
	}

	if (is_tab_staff(mll_p->u.staff_p->staffno) == YES) {
		/* figure out whether to slant up or down based on whether
		 * first or second fret is higher */
		if (begnote_p->FRETNO > endnote_p->FRETNO) {
			slant = 1;
		}
		else {
			slant = -1;
		}
	}
	else {
		/* on non-tab staff, usually the line goes to the midpoint of
		 * the note head, so no need to adjust, so set slant to 0 */
		slant = 0;

		/* there are two exceptions: first, if both notes have the same
		 * letter/octave, but different accidentals, then we have to
		 * determine the slant based on the accidental. */
		if (begnote_p->letter == endnote_p->letter
				&& begnote_p->octave == endnote_p->octave) {

			/* if the accidental on the begin note is higher than
			 * the accidental on the end note, then it slants
			 * down from left to right, and vice versa. Get the
			 * effective accidental on each group,
			 * accounting for key signature, accidentals earlier
			 * in the measure, etc. */
			acc1 = eff_acc(beggrp_p, begnote_p, mll_p);
			acc2 = eff_acc(endgrp_p, endnote_p, mll_p);

			/* error if the slide is between identical notes */
			if (acc1 == acc2) {
				l_ufatal(endgrp_p->inputfile,
						endgrp_p->inputlineno,
						"can't slide to the same note");
			}
			else if (acc1 > acc2) {
				slant = 1;
			}
			else {
				slant = -1;
			}
		}

		/* second exception: if the slide is carried in, then it needs
		 * to be slanted, so figure out which way */
		if (stuff_p->carryin == YES) {
#ifdef __STDC__
			switch(notecomp( (const void *) begnote_p,
						(const void *) endnote_p)) {
#else
			switch(notecomp( (char *) begnote_p, (char *) endnote_p)) {
#endif
			case 1:
				slant = 0.5;
				break;
			case -1:
				slant = -0.5;
				break;
			default:
				/* same note, so have to use accidental as
				 * the deciding factor */
				acc1 = eff_acc(beggrp_p, begnote_p, mll_p);
				acc2 = eff_acc(endgrp_p, endnote_p, mll_p);

				/* error if the slide is
				 * between identical notes */
				if (acc1 == acc2) {
					l_ufatal(endgrp_p->inputfile,
						endgrp_p->inputlineno,
						"can't slide to the same note");
				}
				else if (acc1 > acc2) {
					slant = 0.5;
				}
				else {
					slant = -0.5;
				}
				break;
			}
		}
	}

	/* find beginning point of line */
	MALLOC(CRVLIST, curvelist_p, 1);
	curvelist_p->prev = (struct CRVLIST *) 0;
	if (stuff_p->carryin == YES) {
		/* start a bit west of the end note */
		curvelist_p->x = stuff_p->beggrp_p->c[AX] +
			notehorz(stuff_p->beggrp_p, stuff_p->begnote_p, RW)
			- 3.0 * Stepsize;
		curvelist_p->y = endnote_p->c[RY] + (slant * Stepsize);
	}
	else {
		/* start just beyond east of begin note */
		curvelist_p->x = begnote_p->c[AE] + Stdpad;
		curvelist_p->y = stuff_p->begnote_p->c[RY] + (slant * Stepsize);
	}

	/* end point of line */
	MALLOC(CRVLIST, curvelist_p->next, 1);
	curvelist_p->next->prev = curvelist_p;
	curvelist_p->next->next = (struct CRVLIST *) 0;
	if (stuff_p->carryout == YES) {
		/* extend to near end of score */
		curvelist_p->next->x = PGWIDTH - eff_rightmargin(mll_p) - Stepsize;
	}
	else  {
		/* go to just before west of end note */
		curvelist_p->next->x = endgrp_p->c[AX] +
				notehorz(endgrp_p, endnote_p, RW) - Stdpad;
	}
	curvelist_p->next->y = endnote_p->c[RY] - (slant * Stepsize);

	/* attach to stuff */
	stuff_p->crvlist_p = curvelist_p;

	/* place doesn't really make sense, so set arbitrarily */
	stuff_p->place = PL_ABOVE;
}


/* if the slide for given tabslur stuff is to/from nowhere in particular,
 * then handle that here and return YES. Otherwise return NO. */

static int
nowhere_slide(stuff_p)

struct STUFF *stuff_p;

{
	double boundary;	/* east or west boundary of note, with
				 * the slide included */
	double adjust = 0.0;	/* to move the slanted line slightly when
				 * there is a note on the other side of
				 * the stem that is in the way. */
	struct GRPSYL *gs_p;
	struct NOTE *note_p;
	int n;
	float slidexlen;	/* SLIDEXLEN * Staffscale */


	if (stuff_p->curveno < 0) {
		return(NO);
	}

	if (stuff_p->begnote_p->nslurto == 0) {
		return(NO);
	}

	/* find which note it is in the chord, so check later for possible
	 * collisions between the slide and a neighboring note */
	gs_p = stuff_p->beggrp_p;
	note_p = stuff_p->begnote_p;
	for (n = 0; n < gs_p->nnotes; n++) {
		if ( &(gs_p->notelist[n]) == note_p) {
			break;
		}
	}
	if (n == gs_p->nnotes) {
		pfatal("couldn't find note in chord for slide");
	}

	slidexlen = SLIDEXLEN * Staffscale;

	/* for each type, find the outer boundary of the note with the
	 * nowhere slide included and draw a line from there towards the
	 * note, slanted the appropriate direction */
	switch (stuff_p->begnote_p->slurtolist[stuff_p->curveno].octave) {

	case IN_UPWARD:
		boundary = stuff_p->beggrp_p->c[AX] +
			notehorz(stuff_p->beggrp_p, stuff_p->begnote_p, RW);
		/* If there is a note one stepsize below this note, and it's
		 * to the left of the stem while the target note is on the
		 * right side, move the slide up a
		 * tiny bit so it doesn't get swallowed up in that other note
		 * and/or a slide coming into it.
		 * If we're sliding into the middle of a cluster with
		 * wrong-side notes both above and below the target note, it
		 * will still get somewhat swallowed, but that's unlikely to
		 * happen very often, and if it does, this is still about the
		 * best we can manage in that case. */
		n++;
		if (n < gs_p->nnotes && gs_p->notelist[n].stepsup
				== note_p->stepsup - 1 &&
				gs_p->notelist[n].c[AX] < note_p->c[AX]) {
			adjust = Stdpad;
		}
		do_nowhere(stuff_p,
			boundary, stuff_p->begnote_p->c[RY] - Stepsize + adjust,
			boundary + slidexlen, stuff_p->begnote_p->c[RY] + adjust);
		return(YES);

	case IN_DOWNWARD:
		boundary = stuff_p->beggrp_p->c[AX] +
			notehorz(stuff_p->beggrp_p, stuff_p->begnote_p, RW);
		/* if there is a note just above that we might
		 * collide with, adjust to dodge it. */
		n--;
		if (n >= 0 && gs_p->notelist[n].stepsup
				== note_p->stepsup + 1 &&
				gs_p->notelist[n].c[AX] < note_p->c[AX]) {
			adjust = Stdpad;
		}
		do_nowhere(stuff_p,
			boundary, stuff_p->begnote_p->c[RY] + Stepsize - adjust,
			boundary + slidexlen, stuff_p->begnote_p->c[RY] - adjust);
		return(YES);

	case OUT_UPWARD:
		boundary = stuff_p->beggrp_p->c[AX] +
			notehorz(stuff_p->beggrp_p, stuff_p->begnote_p, RE);
		/* If note just above this one that we might collide with,
		 * dodge it */
		n--;
		if (n >= 0 && gs_p->notelist[n].stepsup
				== note_p->stepsup + 1 &&
				gs_p->notelist[n].c[AX] > note_p->c[AX]) {
			adjust = Stdpad;
		}
		do_nowhere(stuff_p,
			boundary - slidexlen, stuff_p->begnote_p->c[RY] - adjust,
			boundary, stuff_p->begnote_p->c[RY] + Stepsize - adjust);
		return(YES);

	case OUT_DOWNWARD:
		boundary = stuff_p->beggrp_p->c[AX] +
			notehorz(stuff_p->beggrp_p, stuff_p->begnote_p, RE);
		/* If note below we might collide with, dodge it */
		n++;
		if (n < gs_p->nnotes && gs_p->notelist[n].stepsup
				== note_p->stepsup - 1 &&
				gs_p->notelist[n].c[AX] > note_p->c[AX]) {
			adjust = Stdpad;
		}
		do_nowhere(stuff_p,
			boundary - slidexlen, stuff_p->begnote_p->c[RY] + adjust,
			boundary, stuff_p->begnote_p->c[RY] - Stepsize + adjust);
		return(YES);

	default:
		return(NO);
	}
}


/* make a CRVLIST with the 2 given points and put it in the given stuff */

static void
do_nowhere(stuff_p, x1, y1, x2, y2)

struct STUFF *stuff_p;
double x1, y1, x2, y2;

{
	MALLOC(CRVLIST, stuff_p->crvlist_p, 1);
	stuff_p->crvlist_p->x = x1;
	stuff_p->crvlist_p->y = y1;
	MALLOC(CRVLIST, stuff_p->crvlist_p->next, 1);
	stuff_p->crvlist_p->next->x = x2;
	stuff_p->crvlist_p->next->y = y2;

	stuff_p->crvlist_p->prev = stuff_p->crvlist_p->next->next
							= (struct CRVLIST *) 0;
	stuff_p->crvlist_p->next->prev = stuff_p->crvlist_p;

	/* place is not really relevant, but put something in it */
	stuff_p->place = PL_ABOVE;
}


/* Figure out what points are needed for a curve, either a phrase mark
 * or a tie/slur, or a bend.
 * First it figures out where the endpoints should be,
 * then finds a curve that will be beyond all the groups that it covers.
 */

static void
curve_points(mll_p, stuff_p, is_phrase)

struct MAINLL *mll_p;		/* MAINLL that stuff_p hangs off of */
struct STUFF *stuff_p;		/* info about the phrase mark or tie/slur */
int is_phrase;			/* YES if phrase, NO if tie or slur */

{
	struct GRPSYL *begin_gs_p;	/* curve starts on this group */
	struct GRPSYL *end_gs_p;	/* curve ends on this group */
	struct NOTE *begnote_p;		/* first note for tie/slur */
	struct NOTE *endnote_p = 0;	/* last note of tie/slur */
	int place;			/* bend PL_ABOVE or PL_BELOW */
	int side;			/* RN or RS */
	int side_adj;			/* AN or AS. This field is used to
					 * adjust for nested phrase marks */
	double protruding_stemlen;	/* length of stem beyond the notes */
	int found_good;			/* YES if found a good-looking curve */
	struct TRYBULGE tb;		/* Info for try_bulge() */
	struct TRYBULGE *try_p;		/*  = &tb */
	float sign;			/* based on if curve is up or down */
	struct CRVLIST *curvelist_p;	/* beginning of curve */
	struct CRVLIST *endlist_p;	/* last point of curve */
	struct CRVLIST *new_p;		/* point to add to list of points */
	struct MAINLL *bar_mll_p = 0;	/* to find bar or pseudo bar */
	float y_adj = 0.0, y2_adj = 0.0;/* if moved because was an end note */
	char *name;		/* "phrase" or "tie/slur" */


	debug(32, "curve_points lineno %d", stuff_p->inputlineno );

	/* get short names to groups and notes we'll use a lot */
	begin_gs_p = stuff_p->beggrp_p;
	end_gs_p = stuff_p->endgrp_p;
	begnote_p = stuff_p->begnote_p;
	
	/* set up bulge trial and init a few things in it. These may be
	 * set differently later, and other fields will be filled in later. */
	try_p = &tb;
	try_p->beg_beside = try_p->end_beside = NO;

	/* figure out what string ("phrase" or "tie/slur") to use for error
	 * messages and make sure begin group is not null */
	if (is_phrase == YES) {
		name = "phrase";
		if ( (begin_gs_p == (struct GRPSYL *) 0)
				|| (end_gs_p == (struct GRPSYL *) 0) ) {
			pfatal("no group associated with phrase");
		}
	}
	else {
		int indx;

		name = "tie/slur";
		if (begin_gs_p == (struct GRPSYL *) 0) {
			pfatal("no group associated with tie/slur");
		}
		/* figure out which direction to bend the tie/slur */
		if (stuff_p->carryin == YES) {
			struct MAINLL *m_p;
			struct GRPSYL *g_p;
			struct STUFF *st_p;

			/* Need to base bend direction on the
			 * group/note/curve that was the carryout,
			 * otherwise the carryin and carryout could have
			 * different bend directions.
			 * We also need the costuff_p to get
			 * any user override of bend direction.
			 *
			 * Find the MAINLL pointing to the STAFF that
			 * should contain the costuff. Use prevgrpsyl,
			 * since it knows how to deal with endings,
			 * but we're really interested in the MAINLL
			 * pointing to the GRPSYL
			 * rather than the GRPSYL itself.
			 */

			/* First make sure we have the first group
			 * in the measure. */
			for (g_p = begin_gs_p; g_p->prev != 0; g_p = g_p->prev) {
				;
			}

			/* Now find the MAINLL pointing to the prev meas */
			m_p = mll_p;
			(void) prevgrpsyl(g_p, &m_p);
			if (m_p == 0 || m_p->str != S_STAFF) {
				pfatal("failed to find costaff_p's mainll");
			}

			/* Locate the costuff. We could just use
			 * stuff_p->costuff_p, but by searching for it here,
			 * we double check that we really found the right
			 * MAINLL, and can pfatal if not. */
			for (st_p = m_p->u.staff_p->stuff_p; st_p != 0;
							st_p = st_p->next) {
				if (st_p == stuff_p->costuff_p) {
					break;
				}
			}
			if (st_p == 0) {
				pfatal("failed to find costaff_p from mainll");
			}

			indx = st_p->begnote_p - &(st_p->beggrp_p->notelist[0]);
			stuff_p->place = (bulge_direction(m_p, st_p->beggrp_p,
				indx, st_p->curveno) == UP
				? PL_ABOVE : PL_BELOW);
		}
		else {
			indx = begnote_p - &(begin_gs_p->notelist[0]);
			stuff_p->place = (bulge_direction(mll_p, begin_gs_p,
				indx, stuff_p->curveno) == UP
				? PL_ABOVE : PL_BELOW);
		}
	}

	place = stuff_p->place;

	/* determine whether to use north or south of groups, and what sign to
	 * use to get the bends in the correct direction */
	if (place == PL_ABOVE) {
		side = RN;
		side_adj = AN;
		sign = 1.0;
	}
	else {
		side = RS;
		side_adj = AS;
		sign = -1.0;
	}

	/* set up the beginning coord */
	MALLOC(CRVLIST, curvelist_p, 1);
	curvelist_p->prev = (struct CRVLIST *) 0;
	if (is_phrase == YES) {
		/* Start slightly to east of center, so that if another
		 * curves ends on this group, they won't quite touch */
		curvelist_p->x = begin_gs_p->c[AX] + XOFFSET4CURVE;
		if (begin_gs_p->grpcont != GC_SPACE) {
			curvelist_p->y = begin_gs_p->c[side]
				+ eff_tupext(begin_gs_p, mll_p->u.staff_p, place)
				+ (sign * 2.0 * Stdpad);
			/* If there is something in [side_adj] there
			 * was another phrase on this group. But if that phrase
			 * ended on this group, it can be ignored. */
			if (begin_gs_p->c[side_adj] != 0.0 &&
					(begin_gs_p->phraseside & EAST_SIDE)) {
				curvelist_p->y += begin_gs_p->c[side_adj];
			}

			/* If this phrase mark is not a carryin and
			 * it is on the stem side, and
			 * if this group is either longer than an 8th note
			 * (and thus has no flag/beam) or is a beam end,
			 * then we will try to start the phrase mark
			 * at the vertical midpoint of that segment,
			 * and a bit to the right, rather than at the
			 * place we had calculated above. So first
			 * we calculate the length of stem beyond the notes.
			 * We take the length of the entire stem, and subtract
			 * off the distance between the top and bottom notes.
			 * Since notes are always stored top to bottom,
			 * the stem direction does not matter.
			 * Then we subtract off another Stepsize to account
			 * for the half of the note head that protrudes
			 * beyond its Y. Actually, depending on the head shape
			 * and size, one Stepsize may not be exactly right,
			 * but it probably close enough,
			 * since we may well need to adjust
			 * where we start the phase mark vertically anyway,
			 * and the "halfway" is just a reasonably guess of
			 * what may look best anyway.
			 * If the protruding stem is shorter than 4
			 * Stepsizes, we don't bother.
			 */
			if (begin_gs_p->nnotes == 0) {
				/* Strange; must be from a mrpt or something */
				protruding_stemlen = 0.0;
			}
			else {
				protruding_stemlen = begin_gs_p->stemlen
				- (begin_gs_p->notelist[0].stepsup
				- begin_gs_p->notelist[begin_gs_p->nnotes-1].stepsup)
				* Stepsize  - Stepsize;
			}

			if ( stuff_p->carryin == NO &&
			 ((place == PL_ABOVE && begin_gs_p->stemdir == UP) ||
			(place == PL_BELOW && begin_gs_p->stemdir == DOWN)) &&
			(begin_gs_p->basictime < 8 || begin_gs_p->beamloc == ENDITEM)
			&& (protruding_stemlen > (4 * Stepsize)) ) {

				/* So now we know we want to start the phrase
				 * mark beside the stem rather than at the
				 * end of the group. Figure out where
				 * to put it. The note absolute coords are
				 * not filled in yet at this point, and we
				 * are doing everything relative to the
				 * group's Y, which is the center of the staff,
				 * so we use the "end" note offset from there.
				 * We add a Stepsize for half the height of
				 * the note head, and then plus (or minus)
				 * half the length of the
				 * protruding part of the stem.
				 * If we later discover we need to move this
				 * endpoint, once that move gets
				 * to within one Stepsize
				 * of the end of the stem, we give up on
				 * putting it beside.
				 */
				if (begin_gs_p->stemdir == UP) {
					/* Relative to the middle of the staff,
					 * take enough to get to
					 * the middle of the top note,
					 * plus the top half of the top note,
					 * plus half the protruding stem,
					 * to get up the y location of halfway
					 * up the protruding stem.
					 */
					curvelist_p->y = (begin_gs_p->notelist[0].stepsup * Stepsize) + 
						Stepsize +
						(protruding_stemlen / 2.0);
					/* From what we just calculated above,
					 * add half the protruding stem length
					 * to get to the top of the stem,
					 * and then come down from there by
					 * one Stepsize. If we move the
					 * endpoint of the phrase that far,
					 * we'll give up on putting it beside.
					 */
					try_p->beg_side_thresh = curvelist_p->y
						+ (protruding_stemlen / 2.0)
						- Stepsize;
				}
				else {
					curvelist_p->y = (begin_gs_p->notelist[begin_gs_p->nnotes-1].stepsup * Stepsize) -
						Stepsize -
						(protruding_stemlen / 2.0);
					try_p->beg_side_thresh = curvelist_p->y
						- (protruding_stemlen / 2.0)
						+ Stepsize;
				}
				/* Horizontally, we place to
				 * the right of the stem. That should be
				 * empty space, since we checked that the
				 * protruding stem was long enough that we
				 * should be clear of accidentals and dots
				 * and such. There is a slight chance we
				 * could brush a tie or slur, but very unlikely
				 * to look bad enough to worry about.
				 */
				curvelist_p->x = begin_gs_p->c[AX]
					+ begin_gs_p->stemx + SIDE_OFFSET;
				try_p->beg_beside = YES;
			}
		}
		else {
			/* Bizarre case. First group is a space. Use 3 steps
			 * from top or bottom of staff for y coord */
			curvelist_p->y = sign * (3.0 * Stepsize
					+ halfstaffhi(begin_gs_p->staffno));
		}
	}

	else { /* is tie or slur */

		curvelist_p->y = begnote_p->c[RY];
		y_adj = 0.0;

		/* if on the "end" note of a group,
		 * the curve can probably be moved
		 * to the x of the note instead of the edge of the group.
		 * We assume it can if the curve bends away
		 * from the stem and there are no "with"
		 * list items on the group. If there is a with list, move
		 * a little bit, but not enough to hit with items */
		if (begin_gs_p->stemdir == UP && place == PL_BELOW
				&& begnote_p == &(begin_gs_p->notelist
				[begin_gs_p->nnotes - 1])) {
			if (has_normwith(begin_gs_p) == NO) {
				curvelist_p->x = begnote_p->c[AX]
					+ left_endts_adj(mll_p, begin_gs_p, begnote_p);
				y_adj = (Stepsize * (begnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
				curvelist_p->y -= y_adj;
			}
			else {
				curvelist_p->x = begnote_p->c[AE];
				y_adj = (Stepsize * (begnote_p->notesize
						== GS_NORMAL ? 1.2 : 0.9));
				curvelist_p->y -= y_adj;
			}
		}
		else if (begin_gs_p->stemdir == DOWN && place == PL_ABOVE
				&& begnote_p == &(begin_gs_p->notelist[0])) {
			if (has_normwith(begin_gs_p) == NO) {
				curvelist_p->x = begnote_p->c[AX]
					+ left_endts_adj(mll_p, begin_gs_p, begnote_p);
				y_adj = (Stepsize * (begnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
				curvelist_p->y += y_adj;
			}
			else {
				curvelist_p->x = begnote_p->c[AE];
				y_adj = (Stepsize * (begnote_p->notesize
						== GS_NORMAL ? 1.2 : 0.9));
				curvelist_p->y += y_adj;
			}
		}

		/* Whole and double whole notes don't really have a stem,
		 * so the top note of "stem up" can be moved.
		 * Stemless grace notes also don't have a stem,
		 * so the same logic applies. */
		else if ( (STEMLESS(begin_gs_p)
				|| (begin_gs_p->grpvalue == GV_ZERO
				&& (begin_gs_p->basictime < 8 || begin_gs_p->stemlen < Stepsize)))
				&& begin_gs_p->stemdir == UP
				&& place == PL_ABOVE &&
				begnote_p == &(begin_gs_p->notelist[0])) {
			if (has_nonnormwith(begin_gs_p) == NO) {
				curvelist_p->x = begnote_p->c[AX]
					+ left_endts_adj(mll_p, begin_gs_p, begnote_p);
				y_adj = (Stepsize * (begnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
				curvelist_p->y += y_adj;
			}
			else {
				curvelist_p->x = begnote_p->c[AE];
				y_adj = (Stepsize * (begnote_p->notesize
						== GS_NORMAL ? 1.2 : 0.9));
				curvelist_p->y += y_adj;
			}
		}

		/* Can also be moved if bottom note of a whole or
		 * double whole stem-down group */
		else if (begin_gs_p->stemdir == DOWN && place == PL_BELOW
				&& begnote_p == &(begin_gs_p->notelist
				[begin_gs_p->nnotes - 1])  &&
				stuff_p->carryin == NO) {
			if ( STEMLESS(begin_gs_p)
					&& has_nonnormwith(begin_gs_p) == YES) {
				curvelist_p->x = begin_gs_p->c[AE];
				y_adj = (Stepsize * (begnote_p->notesize
						== GS_NORMAL ? 1.2 : 0.9));
				curvelist_p->y -= y_adj;
			}
			else {
				curvelist_p->x = begnote_p->c[AX]
					+ left_endts_adj(mll_p, begin_gs_p, begnote_p);
				y_adj = (Stepsize * (begnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
				curvelist_p->y -= y_adj;
			}
		}
		else {
			curvelist_p->x = begin_gs_p->c[AX] +
					notehorz(begin_gs_p, begnote_p, RE) +
					Stdpad;
		}

		/* If two notes are a stepsize apart and the curve from the
		 * west note is bending towards the east note,
		 * then the x should be moved east a little.
		 * First case: this isn't the top note, but the note just
		 * above is 1 stepsize away and on the east side, and the
		 * curve is going up and it's not a carryin. */
		if (begnote_p != &(begin_gs_p->notelist[0]) &&
				begnote_p->stepsup ==
				begnote_p[-1].stepsup - 1 &&
				begnote_p->c[RX] < begnote_p[-1].c[RX] &&
				place == PL_ABOVE &&
				stuff_p->carryin == NO) {
			curvelist_p->x += 1.5 * Stepsize;
		}
		/* Second case: not bottom note, note just
		 * below is one step away and on the east side, the curve
		 * is going down, and it's not a carryin. */
		else if (begnote_p != &(begin_gs_p->notelist[begin_gs_p->nnotes-1]) &&
				begnote_p->stepsup ==
				begnote_p[1].stepsup + 1 &&
				begnote_p->c[RX] < begnote_p[1].c[RX] &&
				place == PL_BELOW &&
				stuff_p->carryin == NO) {
			curvelist_p->x += 1.5 * Stepsize;
		}
				
	}

	/* if carried over from previous score, start a bit farther left */
	if (stuff_p->carryin == YES) {

		/* find the pseudo bar and set x to that */
		for (bar_mll_p = mll_p->prev;
					bar_mll_p != (struct MAINLL *) 0;
					bar_mll_p = bar_mll_p->prev) {
			if (bar_mll_p->str == S_CLEFSIG) {
				if (bar_mll_p->u.clefsig_p->bar_p
							== (struct BAR *) 0) {
					/* carryin to an ending */
					continue;
				}
				curvelist_p->x =
					bar_mll_p->u.clefsig_p->bar_p->c[AE]
					- (TIESLURPAD * Staffscale);

				/* Long notes (wholes, etc) generally get
				 * more space on their left than short notes,
				 * so a curve carried in to a long note
				 * may look overly long, especially if other
				 * scores on the same page have carryins
				 * to short notes. So limit carryin curve
				 * length to 5 stepsizes.
				 */
				if (begin_gs_p->c[AW] - curvelist_p->x > 5.0 * Stepsize) {
					curvelist_p->x = begin_gs_p->c[AW]
							- 5.0 * Stepsize;
				}
				break;
			}
			else if (bar_mll_p->str == S_BAR) {
				/* carryin to an ending */
				curvelist_p->x = begin_gs_p->c[AW];
				break;
			}
		}

		if (bar_mll_p == (struct MAINLL *) 0) {
			pfatal("missing CELFSIG when carrying over %s mark",
								name);
		}
	}

	/* set up ending coord */
	MALLOC(CRVLIST, endlist_p, 1);
	if (is_phrase == YES) {
		/* End slightly to west of group center, so that another
		 * curve can start on this group (if needed) with
		 * touching this curve. */
		endlist_p->x = end_gs_p->c[AX] - XOFFSET4CURVE;
		if (end_gs_p->grpcont != GC_SPACE) {
			endlist_p->y = end_gs_p->c[side]
				+ eff_tupext(end_gs_p, mll_p->u.staff_p, place)
				+ (sign * 2.0 * Stdpad);
			/* Add in space for any relevant nested phrases */
			if (end_gs_p->c[side_adj] != 0.0 &&
					(end_gs_p->phraseside & WEST_SIDE)) {
				endlist_p->y += end_gs_p->c[side_adj];
			}

			/* If this phrase mark is on the stem side, and
			 * if this group is not ending or inside a beam,
			 * then we will try to start the phrase mark
			 * at the vertical midpoint of that segment,
			 * and one Stepsize to the left, rather than at the
			 * place we had calculated above. See comment above,
			 * on the similar code for the beginning point
			 * for more details. Note that there is one point
			 * of non-symmetry here: since flags only occur on the
			 * right of the stem, they only interfere on the
			 * beginning of a phrase, not the end. So the beamloc
			 * portions of the 'if' statements are different.
			 */
			if (end_gs_p->nnotes == 0) {
				/* strange; must be mrpt or something */
				protruding_stemlen = 0.0;
			}
			else {
				protruding_stemlen = end_gs_p->stemlen
				- (end_gs_p->notelist[0].stepsup
				- end_gs_p->notelist[end_gs_p->nnotes-1].stepsup)
				* Stepsize  - Stepsize;
			}
			if ( stuff_p->carryout == NO &&
			((place == PL_ABOVE && end_gs_p->stemdir == UP) ||
			(place == PL_BELOW && end_gs_p->stemdir == DOWN))
			&& (end_gs_p->beamloc != ENDITEM && end_gs_p->beamloc != INITEM)
			&& (protruding_stemlen > (4 * Stepsize)) ) {
				if (end_gs_p->stemdir == UP) {
					endlist_p->y = (end_gs_p->notelist[0].stepsup * Stepsize) + 
						Stepsize +
						(protruding_stemlen / 2.0);
					try_p->end_side_thresh = endlist_p->y
						+ (protruding_stemlen / 2.0)
						- Stepsize;
				}
				else {
					endlist_p->y = (end_gs_p->notelist[end_gs_p->nnotes-1].stepsup * Stepsize) -
						Stepsize -
						(protruding_stemlen / 2.0);
					try_p->end_side_thresh = endlist_p->y
						- (protruding_stemlen / 2.0)
						+ Stepsize;
				}
				endlist_p->x = end_gs_p->c[AX]
						+ end_gs_p->stemx - SIDE_OFFSET;
				try_p->end_beside = YES;
			}
		}
		else {
			/* Bizarre case. Last group is a space.  Use 3 steps
			 * from top or bottom of staff for y coord */
			endlist_p->y = sign * (3.0 * Stepsize
					+ halfstaffhi(begin_gs_p->staffno));
		}
	}
	else {
		if (stuff_p->carryin == YES) {
			/* in case of carryin, the "begin" group is actually
			 * the ending group, so set the end group, and
			 * adjust the beginning y */
			endlist_p->x = begin_gs_p->c[AW];

			/* adjust things carried into endings to account for
			 * the padding that was added */
			if (bar_mll_p->str == S_BAR) {
				endlist_p->x += TIESLURPAD * Staffscale;
			}

			endlist_p->y = curvelist_p->y;
			end_gs_p = begin_gs_p;

			/* if end note, adjust */
			if (place == PL_ABOVE && begnote_p
						== &(begin_gs_p->notelist[0])) {
				endlist_p->x = begnote_p->c[AX];
				if ( STEMMED(begin_gs_p) &&
						(begin_gs_p->stemdir == UP)) {
					endlist_p->y += Stepsize;
					curvelist_p->y += Stepsize;
				}
			}
			else if (place == PL_BELOW && begnote_p ==
						&(begin_gs_p->notelist
						[begin_gs_p->nnotes - 1])
						&& (begin_gs_p->stemdir == UP
						|| STEMLESS(begin_gs_p) ) ) {
				endlist_p->x = begnote_p->c[AX];
				if (STEMLESS(begin_gs_p) &&
						(begin_gs_p->stemdir == DOWN)) {
					endlist_p->y -= Stepsize;
					curvelist_p->y -= Stepsize;
				}
			}
		}
		else {
			/* not carryin */
			if (end_gs_p == 0) {
				/* This is the most usual case. But end_gs_p
				 * will have already been set from the STUFF if
				 * this is a tie/slur to a different voice. */
				end_gs_p = find_next_group (mll_p, begin_gs_p,
						(stuff_p->curveno == -1
						? "tie" : "slur"));
			}
			if (stuff_p->curveno == -1) {
				/* this is a tie */
				endnote_p = find_matching_note (end_gs_p,
						mll_p, begnote_p->letter,
						begnote_p->FRETNO,
						begnote_p->octave, "tie");
			}
			else {
				if (IS_NOWHERE(begnote_p->slurtolist
						[stuff_p->curveno].octave)) {
					pfatal("curve_points called on slide to nowhere");
				}

				endnote_p = find_matching_note (end_gs_p, 0,
						begnote_p->slurtolist
						[stuff_p->curveno].letter,
						-1,
						begnote_p->slurtolist
						[stuff_p->curveno].octave,
						"slur/slide");
			}

			if (endnote_p == 0) {
				pfatal("curve_points: unable to find matching note that had been found earlier");
			}

			endlist_p->y = endnote_p->c[RY];

			y2_adj = 0.0;

			/* move if below curve and bottom note with stem up */
			if (end_gs_p->stemdir == UP && place == PL_BELOW
					&& endnote_p == &(end_gs_p->notelist
					[end_gs_p->nnotes - 1])) {
				if (has_normwith(end_gs_p) == NO) {
					endlist_p->x = endnote_p->c[AX]
						- right_endts_adj(endnote_p);
					y2_adj = (Stepsize *
						(endnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
					endlist_p->y -= y2_adj;
				}
				else {
					endlist_p->x = endnote_p->c[AW];
					y2_adj = (Stepsize *
						(endnote_p->notesize
						== GS_NORMAL ? 1.2 : 0.9));
					endlist_p->y -= y2_adj;
				}
			}

			/* move if above and top note with stem down */
			else if (end_gs_p->stemdir == DOWN && place == PL_ABOVE
				     && endnote_p == &(end_gs_p->notelist[0])) {
				if (has_normwith(end_gs_p) == NO ) {
					endlist_p->x = endnote_p->c[AX]
						- right_endts_adj(endnote_p);
					y2_adj = (Stepsize *
						(endnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
					endlist_p->y += y2_adj;
				}
				else {
					endlist_p->x = endnote_p->c[AW];
					y2_adj = (Stepsize *
						(endnote_p->notesize
						== GS_NORMAL ? 1.2 : 0.9));
					endlist_p->y += y2_adj;
				}
			}

			/* Whole and dblwhole don't have stem, so end note where
			 * a stem would be (if there were one) can be moved */
			else if (STEMLESS(end_gs_p) &&
					end_gs_p->stemdir == DOWN
					&& place == PL_BELOW
					&& endnote_p == &(end_gs_p->notelist
					[end_gs_p->nnotes - 1])) {
				if (has_nonnormwith(end_gs_p) == NO) {
					endlist_p->x = endnote_p->c[AX]
						- right_endts_adj(endnote_p);
					y2_adj = (Stepsize *
						(endnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
					endlist_p->y -= y2_adj;
				}
				else {
					endlist_p->x = endnote_p->c[AW];
					y2_adj = (Stepsize *
						(endnote_p->notesize
						== GS_NORMAL ? 1.2 : 0.9));
					endlist_p->y -= y2_adj;
				}
			}

			/* move if above and top note of stem up */
			else if (end_gs_p->stemdir == UP && place == PL_ABOVE
					&& endnote_p ==
					&(end_gs_p->notelist[0]) ) {
				endlist_p->x = endnote_p->c[AX]
						- right_endts_adj(endnote_p);
				y2_adj = (Stepsize * (endnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
				endlist_p->y += y2_adj;

				/* if tied from note is also the top of its
				 * group, level the tie/slur */
				if (begin_gs_p->stemdir == UP &&
						begnote_p ==
						&(begin_gs_p->notelist[0])  &&
						STEMMED(begin_gs_p) ) {
					curvelist_p->y += (Stepsize *
						(begnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
				}
			}
			else if (begin_gs_p->grpvalue == GV_ZERO) {
				/* grace note to main note, can't use the west
				 * of the end group because that would include
				 * the grace note. */
				endlist_p->x = endnote_p->c[AX] +
					notehorz(end_gs_p, endnote_p, RW);
			}
			else {
				endlist_p->x = tieslurx(end_gs_p, endnote_p,
					stuff_p->place) - (2.0 * Stdpad);
			}

			/* if note tied from is bottom of group with stem down,
			 * level the tie/slur */
			if (end_gs_p->stemdir == DOWN && place == PL_BELOW
					&& endnote_p == &(end_gs_p->notelist
					[end_gs_p->nnotes - 1]) &&
					begin_gs_p->stemdir == DOWN &&
					begnote_p == &(begin_gs_p->notelist
					[begin_gs_p->nnotes - 1]) &&
					STEMMED(end_gs_p) ) {
				endlist_p->y -= (Stepsize *
						(begnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
			}

			/* if beginning of curve was adjusted and this is
			 * an inner note, but there is room on the relevant
			 * side, and this is a tie, then adjust this end's y
			 * to level the curve */
			else if (y_adj != 0.0 && stuff_p->curveno == -1) {
				endlist_p->y += inner_adj(end_gs_p, endnote_p,
							y_adj, place);
			}

			/* level beginning if the note in the previous
			 * chord was the same note but wasn't the top,
			 * but the next note is more than a stepsize
			 * away. */
			if (y2_adj != 0.0 && stuff_p->curveno == -1) {
				curvelist_p->y += inner_adj(begin_gs_p,
					begnote_p, y2_adj, place);
			}
		}
	}

	/* One final adjustment. If the stem of first group is up and stem
	 * of second group is down, and the notes being tied/slurred are both
	 * the top notes if the place is above or both bottom notes if the
	 * place is below, then move the y coord on the side that wasn't
	 * already moved, to level the curve. Do only if the note is not a
	 * whole or double whole, because those notes were already moved because
	 * they had no stem. */
	if (is_phrase == NO && begin_gs_p->stemdir == UP
					&& end_gs_p != (struct GRPSYL *) 0
					&& end_gs_p->stemdir == DOWN) {
		if (place == PL_ABOVE && begnote_p ==
				&(begin_gs_p->notelist[0])
				&& endnote_p == &(end_gs_p->notelist[0])
				&& STEMMED(begin_gs_p) ) {
			curvelist_p->y += (Stepsize * (begnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
		}
		else if (place == PL_BELOW && begnote_p ==
				&(begin_gs_p->notelist[begin_gs_p->nnotes - 1])
				&& endnote_p ==
				&(end_gs_p->notelist[end_gs_p->nnotes - 1])
				&& STEMMED(end_gs_p) ) {
			endlist_p->y -= (Stepsize * (endnote_p->notesize
						== GS_NORMAL ? 1.7 : 1.2));
		}
	}

	endlist_p->next = (struct CRVLIST *) 0;
	/* no need to set other links now because we will be adding other nodes
	 * in between in a moment anyway */
	
	/* if carrying over, extend x to margin */
	if (stuff_p->carryout) {
		endlist_p->x = PGWIDTH - eff_rightmargin(mll_p);
	}

	/* set up node for another point on curve */
	MALLOC(CRVLIST, new_p, 1);
	new_p->prev = curvelist_p;
	new_p->next = endlist_p;
	curvelist_p->next = new_p;
	endlist_p->prev = new_p;

	/* Fill in information to be used to try to find a good curve. */
	tb.mll_p = mll_p;
	tb.begin_gs_p = begin_gs_p;
	tb.end_gs_p = end_gs_p;
	tb.place = place;
	tb.curvelist_p = curvelist_p;
	tb.endlist_p = endlist_p;
	try_p->sign = sign;
	try_p->trials = 0;
	set_try_params(try_p);

	/* Adjust y of carryouts */
	if (stuff_p->carryout == YES) {
		if (is_phrase == YES) {
			endlist_p->y += try_p->minbulge / 3.0 * sign;
		}
		else {
			end_gs_p = begin_gs_p;
		}
	}

	if ((found_good = try_bulge(try_p)) == YES) {
		/* Try to beautify any really steep curves. */
		try_redo_steep(try_p);

		curvelist_p = flatten_long_curve(try_p);
		/* adjust group boundaries to include the curve */
		final_touches(mll_p, begin_gs_p, end_gs_p, curvelist_p, place);

		/* attach the curve to the stuff */
		stuff_p->crvlist_p = curvelist_p;
		return;
	}

	/* Just adjusting bulge didn't work,
	 * so we try repeatedly moving the ends slightly
	 * and trying again until something works.
	 * Worst case should be something like an above curve encompassing c0
	 * to b9 back to c0, with a stem up on the b9. That would be about 80
	 * stepsizes. But if an end is a cross-staff stem group completely
	 * on the other staff, and if that other staff is ridiculously
	 * far away because of very tall STUFF, even 100 iterations
	 * of moving by a Stepsize sometimes isn't enough.
	 * So we'll try 200 times before giving up with a pfatal.
	 */
	for (  ; tb.trials < 200; (tb.trials)++) {
		/* Try moving the endpoints. If it is "steep" we move
		 * just the end that will make it more horizontal. */
		if ( (place == PL_ABOVE && try_p->sintheta > STEEP_SIN) ||
				(place == PL_BELOW
				&& try_p->sintheta < - STEEP_SIN) ) {
			curvelist_p->y += Stepsize * sign;
		}
		else if ( (place == PL_ABOVE && try_p->sintheta < - STEEP_SIN) ||
				(place == PL_ABOVE
				&& try_p->sintheta > STEEP_SIN) ) {
			endlist_p->y += Stepsize * sign;
		}

		/* Next if protrusion is considerably worse near one end,
		 * try to move that end. But only do that 2/3 of the time,
		 * because if we move one end an enormous amount and the
		 * other end not at all, that looks lopsided. */
		else if ( ((try_p->trials % 3) < 2) &&
				try_p->left_protrusion >
				try_p->right_protrusion + 2.0 * Stepsize) {
			curvelist_p->y += Stepsize * sign;
		}
		else if ( ((try_p->trials % 3) < 2) &&
				try_p->right_protrusion >
				try_p->left_protrusion + 2.0 * Stepsize) {
			endlist_p->y += Stepsize * sign;
		}

		/* If one end is stem side and the other not, we favor
		 * the stem side. But we only weakly favor it, because
		 * they are times when moving the other side just a tiny bit
		 * will help more than moving the stem side a huge amount.
		 */
		else if (try_p->trials < 3 && is_stemside(begin_gs_p, place)
					&& !is_stemside(end_gs_p, place)) {
			curvelist_p->y += Stepsize * sign;
			endlist_p->y += 0.25 * Stepsize * sign;
	}
		else if (try_p->trials < 3 && !is_stemside(begin_gs_p, place)
					&& is_stemside(end_gs_p, place)) {
			endlist_p->y += Stepsize * sign;
			curvelist_p->y += 0.25 * Stepsize * sign;
		}

		/* Otherwise we move both ends equally */
		else {
			curvelist_p->y += Stepsize * sign;
			endlist_p->y += Stepsize * sign;
		}

		set_try_params(try_p);
		if (try_bulge(try_p) == YES) {
			found_good = YES;
			break;
		}

	}

	if (found_good == NO) {
		pfatal("unable to find a usable curve");
	}

	if (try_redo_steep(try_p) == YES) {
		curvelist_p->y -= sign * Stepsize;
		endlist_p->y -= sign * Stepsize;
		if (try_bulge(try_p) == NO) {
			/* Moving closer ran into something. :(
			 * Put back the previous. */
			curvelist_p->y += sign * Stepsize;
			endlist_p->y += sign * Stepsize;
			(void) try_bulge(try_p);
		}
	}

	curvelist_p = flatten_long_curve(try_p);

	final_touches(mll_p, begin_gs_p, end_gs_p, curvelist_p, place);

	/* attach the curve to the stuff */
	stuff_p->crvlist_p = curvelist_p;
}


/* This returns YES if the end point of a phrase in the direction
 * specified by the given place on the given group is on the stem end,
 * or NO if it is on the note end.
 */

static int
is_stemside(gs_p, place)

struct GRPSYL *gs_p;	/* get stem direction from here */
int place;		/* get phrase location (PL_*) from here */

{
	if (place == PL_ABOVE && gs_p->stemdir == UP) {
		return(YES);
	}
	if (place == PL_BELOW && gs_p->stemdir == DOWN) {
		return(YES);
	}
	return(NO);
}


/* If we are starting beside the stem,
 * and we've already moved things enough to be too near the end
 * of the stem, give up on putting it beside,
 * and move the start point to the stem end.
 * We also move to the end if the curve is "steep" and by moving one
 * end point to the stem, it would become less steep.
 * Returns YES if it changed at least one endpoint, NO if not.
 */

static int
move_side2end(try_p)

struct TRYBULGE * try_p;

{
	int changed = NO;

	if (try_p->beg_beside == YES) {
		if (try_p->begin_gs_p->stemdir == UP && (
				(try_p->curvelist_p->y > try_p->beg_side_thresh)
				|| (try_p->sintheta > STEEP_SIN)
				) ) {
			try_p->curvelist_p->y = try_p->beg_side_thresh + SIDE_Y_ADJ;
			try_p->curvelist_p->x -= SIDE_X_ADJ;
			try_p->beg_beside = NO;
			changed = YES;
		}
		else if (try_p->begin_gs_p->stemdir == DOWN && (
				(try_p->curvelist_p->y < try_p->beg_side_thresh)
				|| (try_p->sintheta < - STEEP_SIN)
				) ) {
			try_p->curvelist_p->y = try_p->beg_side_thresh - SIDE_Y_ADJ;
			try_p->curvelist_p->x -= SIDE_X_ADJ;
			try_p->beg_beside = NO;
			changed = YES;
		}
	}

	/* Similar for end */
	if (try_p->end_beside == YES) {
		if (try_p->end_gs_p->stemdir == UP && (
				(try_p->endlist_p->y > try_p->end_side_thresh)
				|| (try_p->sintheta < - STEEP_SIN)
				)) {
			try_p->endlist_p->y = try_p->end_side_thresh + SIDE_Y_ADJ;
			try_p->endlist_p->x += SIDE_X_ADJ;
			try_p->end_beside = NO;
			changed = YES;
		}
		else if (try_p->end_gs_p->stemdir == DOWN && (
				(try_p->endlist_p->y < try_p->end_side_thresh)
				|| (try_p->sintheta > STEEP_SIN)
				) ){
			try_p->endlist_p->y = try_p->end_side_thresh - SIDE_Y_ADJ;
			try_p->endlist_p->x += SIDE_X_ADJ;
			try_p->end_beside = NO;
			changed = YES;
		}
	}
	return (changed);
}


/* This sets length, xlen, sintheta, and costheta in the given try_p struct.
 * The x and y values of try_p->curvelist_p and try_p->endlist_p must be set
 * before this is called.
 */
 
static void
set_values(try_p)

struct TRYBULGE *try_p;

{
	/* This is the length between endpoints of the curve,
	 * via Pythagorean theorem. The length influences
	 * how much bulge we allow.
	 */
	try_p->length = sqrt(SQUARED(try_p->endlist_p->x - try_p->curvelist_p->x)
			+ SQUARED(try_p->endlist_p->y - try_p->curvelist_p->y));

	/* This is the distance to the midpoint, if the curve were rotated
	 * to be horizontal. */
	try_p->xlen = try_p->length / 2.0;

	/* These let us pretend the curve is horizontal */
	try_p->sintheta = (try_p->endlist_p->y - try_p->curvelist_p->y)
							/ try_p->length;
	try_p->costheta = (try_p->endlist_p->x - try_p->curvelist_p->x)
							/ try_p->length;
}


/* This function sets length, xlen, sintheta, costheta, minbulge, and maxbulge
 * fields in the passed in TRYBULGE struct, based on the other fields, which
 * are expected to already be filled in.
 */

static void
set_try_params(try_p) struct TRYBULGE *try_p;

{
	double steep_adj;	/* We allow steep curves to bulge more,
				 * because that generally looks better than
				 * moving them too far from the groups,
				 * which would otherwise frequently be needed
				 * with steep curves.
				 */


	/* Set length, xlen, sintheta, and costheta for current values.
	 * Then check if we should give up on putting next to stem,
	 * If so, we have to re-evaluate. 
	 * Since move_side2end() needs the values that set_values() sets,
	 * we have to call set_values twice in that scenario, but there
	 * are only two ends, and we can only move each from side to end
	 * once, so the double call will only happen twice worst case.
	 */
	set_values(try_p);
	if (move_side2end(try_p) == YES) {
		set_values(try_p);
	}

	/* We allow more bulge on steep curves */
	steep_adj = fabs(try_p->sintheta);

	/* Really short curves need a lot of bulge
	 * by proportion to their length or they sort of get swallowed up,
	 * whereas long curves need to not have so much bulge or they
	 * end up taking taking too much vertical space. Possibly we
	 * should really represent the bulge length function by a curve,
	 * (for curve length x, get the bulge from the y
	 * of some appropriate curve),
	 * but this is just heuristics anyway,
	 * so we effectively use 3 line segments. The first segment
	 * is a cutoff value for the minimum bulge, then the line for
	 * the normal part of the function, then another cutoff value
	 * for curves that are so long we don't want any more bulge.
	 * We could use more segments or different
	 * values to try to make things better, or some day could even
	 * maybe allow user to specify them somehow.
	 * We calculate two bulge values: the minimum,
	 * which is really what we would prefer to use if it works, and a
	 * maximum, which is the most we think we can get by with,
	 * without looking ugly enough to notice.
	 * After a few trials, we allow more ugliness.
	 * See bulge_value() comment for what the parameters mean.
	 */
	if (try_p->trials < 3) {
		try_p->minbulge = bulge_value(try_p->length,
				12.0, 1.0 + steep_adj, 36.0, 3.0 + steep_adj);
		try_p->maxbulge = bulge_value(try_p->length,
				12.0, 1.8 + steep_adj, 36.0, 7.0 + steep_adj);
	}
	else if (try_p->trials < 16) {
		try_p->minbulge = bulge_value(try_p->length,
				12.0, 1.5 + steep_adj, 36.0, 4.0 + steep_adj);
		try_p->maxbulge = bulge_value(try_p->length,
				12.0, 2.5 + steep_adj, 36.0, 10.0 + steep_adj);
	}
	else {
		try_p->minbulge = bulge_value(try_p->length,
				12.0, 2.0 + steep_adj, 60.0, 12.0 + steep_adj);
		try_p->maxbulge = bulge_value(try_p->length,
				12.0, 3.0 + steep_adj, 60.0, 24.0 + steep_adj);
	}
}


/* This function calculates a bulge value, in Stepsizes,
 * based on a curve length and two points defining a linear function.
 * The x is the curve length and the y is the desired bulge for that length.
 * Suppose, as an example,
 * the two points (x1, y1) and (x2, y2) are (2,1) and (50,12).
 * This means that for a curve length of 2, we want a bulge value of 1,
 * and for a curve length of 50, we want a bulge value of 12,
 * with lengths between that getting bulge values linearly between 1 and 12.
 * The y2 is also a cutoff value, meaning that
 * any curve longer than 50 will get a bulge of 12.
 * Similarly, y1 is a lower cutoff, so any curve shorter than 2 would
 * get a bulge of 1, but we don't expect any curves to actually be so short
 * that that would get used.
 */

static double
bulge_value(length, x1, y1, x2, y2)

double length;	/* distance between endpoints of the curve */
double x1, y1;	/* first point of line defining the bulge function. */
double x2, y2;	/* last point of line defining bulge function. */

{
	double m;	/* slope */
	double b;	/* y intercept */


	/* If past cutoff value, use that */
	if (length > (x2 * Stepsize)) {
		return(y2 * Stepsize);
	}
	if (length < (x1 * Stepsize)) {
		return(y1 * Stepsize);
	}

	/* Solve for slope and y intercept using the formula for a line
	 *   y = mx + b
	 * and doing the algebra to solve two equations in two unknowns.
	 */
	b = ( (x2 * y1) -  (x1 * y2) ) / (x2 - x1);
        m = (y2 - b) / x2;

	/* Solve for bulge based on curve length */
	return(m * length + (b * Stepsize));
}


/* 
 * Returns YES if the curve specified by try_p info worked, NO if not.
 * The curvelist_p pointed to by try_p should point to a curve with 3 points.
 */

static int
try_bulge(info_p)

struct TRYBULGE *info_p;	/* points to all the info this func needs */

{
	struct CRVLIST *mid_p;		/* interior point of curve */
	double bulge;			/* how much to bulge */
	int attempt;			/* count of how many tries */
	int max_attempts;		/* how many bulge values to try */
	double incr;			/* increment for each attempt */


	/* Get pointer to the midpoint */
	mid_p = info_p->curvelist_p->next;

	/* We try up to 4 times, first with the mininum bulge, then
	 * by thirds up to the max. We return early if an early attempt works.
	 */
	max_attempts = 4;
	incr = (info_p->maxbulge - info_p->minbulge) / max_attempts;
	for (attempt = 0; attempt < max_attempts; attempt++) {
		bulge = info_p->minbulge + (incr * attempt);

		/* Find (x,y) values for midpoint taking the rotation
		 * from horizontal into account. */
		mid_p->x = info_p->curvelist_p->x
			+ (info_p->xlen * info_p->costheta)
			- (bulge * info_p->sign * info_p->sintheta);
		mid_p->y = info_p->curvelist_p->y
			+ (bulge * info_p->sign * info_p->costheta)
			+ (info_p->xlen * info_p->sintheta);

		if (stick_out(info_p) <= 0.0) {
			/* This curve works. Go with it */
			return(YES);
		}
	}

	return(NO);
}


/* adjust the endpoint of an inner note if the opposite end was adjusted,
 * and there is room to adjust this end. */

static double
inner_adj(gs_p, note_p, y_adj, place)

struct GRPSYL *gs_p;	/* note is in this group */
struct NOTE *note_p;	/* this is the note being tied to */
double y_adj;		/* how much other end of tie was adjusted */
int place;		/* PL_ABOVE or PL_BELOW */

{
	int i;


	if (gs_p->nnotes <= 2) {
		/* can't possibly be an inner note, so no adjust */
		return(0.0);
	}

	/* find index of note */
	for (i = 0; i < gs_p->nnotes; i++) {
		if (note_p == &(gs_p->notelist[i])) {
			break;
		}
	}

	if (i == gs_p->nnotes) {
		pfatal("couldn't find note in chord");
	}

	if (i == 0 || i == gs_p->nnotes - 1) {
		/* not an inner note. no adjust */
		return(0.0);
	}

	/* check if next note in chord is within STEPSIZE away. If not,
	 * we can adjust this end */
	if (place == PL_ABOVE && gs_p->notelist[i-1].stepsup
					> gs_p->notelist[i].stepsup + 1) {
		return(y_adj);
	}
	else if (place == PL_BELOW && gs_p->notelist[i+1].stepsup
					< gs_p->notelist[i].stepsup - 1) {
		/* y_adj will always come in as a positive number and will be
		 * added on return, so return negative value for below curves */
		return(-y_adj);
	}
	return(0.0);
}


/* Returns the sum of the "stick out" of groups in the given curve.
 * If all groups are inside, this will be 0.0
 */

static double
stick_out(info_p)

struct TRYBULGE *info_p;

{
	struct GRPSYL *gs_p;	/* to walk through list */
	struct GRPSYL *begin_gs_p, *end_gs_p;
	double yleft, yright;	/* y value of point on the line that is
				 * at the x position of the left and right
				 * sides of the current GRPSYL, */
	double yg;		/* y of group accounting for other phrases */
	struct MAINLL *mll_p;	/* the curve's STUFF hangs off of here */
	int place;		/* PL_* */
	struct CRVLIST *curvelist_p;	/* beginning of curve to check */
	int staff;
	int voice;
	double stickout;	/* stick out amount of current group */
	double worst_stickout;	/* return value */
	double tupext;
	double clearance;
	double leftsteps;
	double rightsteps;


	begin_gs_p = info_p->begin_gs_p;
	end_gs_p = info_p->end_gs_p;
	if (begin_gs_p == 0 || end_gs_p == 0) {
		pfatal("got null pointer when checking phrase marks");
	}

	if (begin_gs_p->vno != end_gs_p->vno) {
		/* Must be a tie/slue to another voice. We don't attempt
		 * to try to avoid anything that is in the way. */
		return(0.0);
	}

	/* If starting phrase on last note of score or ending one on first
	 * note of a score, begin and end will be the same. We know that
	 * note has already been accounted for, so nothing to do. */
	if (begin_gs_p == end_gs_p) {
		return(0.0);
	}

	staff = begin_gs_p->staffno;
	voice = begin_gs_p->vno;
	curvelist_p = info_p->curvelist_p;
	mll_p = info_p->mll_p;
	place = info_p->place;
	stickout = 0.0;
	worst_stickout = 0;
	info_p->left_protrusion = info_p->right_protrusion = 0.0;

	/* Go through each group between the beginning and end. We've
	 * already set the curve endings to clear the group boundaries */
	for (gs_p = begin_gs_p->next; gs_p != end_gs_p; gs_p = gs_p->next) {

		/* If hit end of measure go to next measure.
		 * If the vscheme changes to 1 and then back to 2 in the
		 * middle of the phrase, there will be missing measures,
		 * so skip by all of them. */
		while (gs_p == (struct GRPSYL *) 0) {
			mll_p = next_staff(staff, mll_p->next);
			if (info_p->mll_p == (struct MAINLL *) 0) {
				pfatal("fell off end of list while doing phrase marks");
			}
			gs_p = mll_p->u.staff_p->groups_p[voice - 1];
		}

		if (gs_p == end_gs_p) {
			break;
		}

		/* Find out where the y of the curve is at this group.
		 * We actually check two points, one each slightly
		 * to the east and west of the group's x.
		 * We start by guessing 1.5 Stepsizes. Then we
		 * look at what is at the end. If it is
		 * a stem and there is no "with list,"
		 * we can probably get closer,
		 * especially if no flag/beam. Note that the flag/beam
		 * always affects the right side equally, but the stem
		 * effect depends on stem direction. */
		leftsteps = rightsteps = 1.5;
		if (info_p->place == PL_ABOVE && gs_p->stemdir == UP
					&& gs_p->stemlen > 3.0 * Stepsize
					&& has_nonnormwith(gs_p) == NO) {
			leftsteps = 0.5;
			if (gs_p->basictime < 8) {
				rightsteps = 0.5;
			}
		}
		else if (info_p->place == PL_BELOW && gs_p->stemdir == DOWN
					&& gs_p->stemlen > 3.0 * Stepsize
					&& has_nonnormwith(gs_p) == NO) {
			if (gs_p->basictime < 8 || gs_p->beamloc == ENDITEM) {
				rightsteps = 0.3;
			}
		}
		yleft = curve_y_at_x(curvelist_p, gs_p->c[AX]
					- leftsteps * Stepsize);
		yright = curve_y_at_x(curvelist_p, gs_p->c[AX]
					+ rightsteps * Stepsize);

		/* See if this group is within the curve */
		if (info_p->place == PL_ABOVE) {
			/* Consider the group (RN) plus any relevant
			 * nested phrase marks (their space is stored in AN).
			 * It is relevant unless it's for the begin group
			 * and that group's east is not relevent, or it's the
			 * end group and that group's west is not relevant */
			tupext = eff_tupext(gs_p, mll_p->u.staff_p, place);
			if (tupext > 0.001) {
				/* We can afford to go a little closer to
				 * tuplets than to notes. */
				clearance = CLEARANCE / 3.0;
			}
			else {
				clearance = CLEARANCE;
			}
			yg = gs_p->c[RN] + clearance + tupext;
			if ( (gs_p != begin_gs_p ||
					((gs_p->phraseside & EAST_SIDE) == 0))
					&& (gs_p != end_gs_p ||
					((gs_p->phraseside & WEST_SIDE) == 0)) ) {
				yg += gs_p->c[AN];
			}
			if (yleft > yg && yright > yg) {
				/* Good. It's inside */
				continue;
			}
			else {
				/* Bad. It stuck over */
				stickout = yg - MIN(yleft, yright);
				worst_stickout = MAX(stickout, worst_stickout);
				set_protrusion(info_p, gs_p->c[AX], stickout);
				/* If we are getting desperate, and
				 * one of the corners of the "bounding box"
				 * was cleared, and the other one only got
				 * clipped down into the area of clearance,
				 * we can declare that as good
				 * enough to live with, because at worse
				 * it should just barely brush one corner. */
				if (info_p->trials > 5 &&
						(yleft > yg || yright > yg ) &&
						stickout < clearance) {
					continue;
				}
			}
		}
		else {
			/* Do the same for curve going down */
			tupext = eff_tupext(gs_p, mll_p->u.staff_p, place);
			if (tupext < -0.001) {
				/* We can afford to go a little closer to
				 * tuplets than to notes. */
				clearance = CLEARANCE / 3.0;
			}
			else {
				clearance = CLEARANCE;
			}
			yg = gs_p->c[RS] - clearance + tupext;
			if ( (gs_p != begin_gs_p ||
					((gs_p->phraseside & EAST_SIDE) == 0))
					&& (gs_p != end_gs_p ||
					((gs_p->phraseside & WEST_SIDE) == 0)) ) {
				yg += gs_p->c[AS];
			}
			if (yleft < yg && yright < yg) {
				continue;
			}
			else {
				stickout = MAX(yleft, yright) - yg;
				worst_stickout = MAX(stickout, worst_stickout);
				set_protrusion(info_p, gs_p->c[AX], stickout);
				if (info_p->trials > 5 &&
						(yleft < yg || yright < yg ) &&
						stickout < clearance) {
					continue;
				}
			}
		}
	}
	return(worst_stickout);
}


/* If the given protrusion value is worse than the worst seen near that
 * end of the curve, update the value in the TRYBULGE struct to record that.
 */

static void
set_protrusion(try_p, x, protrusion)

struct TRYBULGE *try_p;	/* info about the curve */
double x;		/* the x of the group that sticks out */
double protrusion;	/* how much the groups sticks beyonds the curve */

{
	double rotx; /* where X would be if curve was rotated to horizontal */


	/* Figure out value of X if the curve was rotated to horizontal */
	rotx = (x - try_p->begin_gs_p->c[AX]) / try_p->costheta;

	/* If the x is within 40% of either end, and this is the worst
	 * protrusion seen so far in that region, remember it as the new worst.
	 */
	if (rotx <= try_p->length * 0.4) {
		if (protrusion > try_p->left_protrusion) {
			try_p->left_protrusion = protrusion;
		}
	}
	if (rotx >= try_p->length * 0.6) {
		if (protrusion > try_p->right_protrusion) {
			try_p->right_protrusion = protrusion;
		}
	}
}


/* find the x of the end of a tie/slur. Usually we could just used the west of
 * the group, but if there are lots of accidentals on notes that are far
 * away from the note in question, the end of the tie can come out rather
 * far away from its note. So try to see if we can move it closer, by
 * checking to see if there are any accidentals on notes nearby. This
 * function is not foolproof, sometimes leaving space when the tie/slur
 * could actually get threaded through a tiny opening, and sometimes
 * overwriting the edge of an accidental somewhat, but tries to do a better
 * job than the original single line of code for figuring this out had done. */

static double
tieslurx(gs_p, note_p, place)

struct GRPSYL *gs_p;	/* check notes in this group */
struct NOTE *note_p;	/* check for accidentals near this note */
int place;		/* PL_ABOVE or PL_BELOW to tell which side to look on */

{
	int n;		/* index through notelist */
	int acc;	/* accidental */
	int use_group_boundary;	/* YES or NO */
	int steps_away;	/* how many steps away from the note to be checked
			 * some other note is that might have a accidental
			 * that could interfere */
	int i;		/* index through acclist */
	int accsize;	/* DFLT_SIZE or SMALLSIZE */
	float this_height;	/* height of one accidental */
	float farthest;	/* greatest distance up or down of all accs in list */


	/* if "wrong" side of a stem up group, better use group boundary */
	if (note_p->c[AX] > gs_p->c[AX] && gs_p->stemdir == UP) {
		return(gs_p->c[AW] + gs_p->padding +
				vvpath(gs_p->staffno, gs_p->vno, PAD)->pad);
	}

	/* if there is another note nearby,
	 * and that note has an accidental, better use
	 * the west of the group to be safe, otherwise
	 * use the west of the note. */
	for (n = 0; n < gs_p->nnotes; n++) {

		use_group_boundary = NO;
		steps_away = gs_p->notelist[n].stepsup - note_p->stepsup;
		if ((acc = standard_acc(gs_p->notelist[n].acclist))
							!= '\0') {
			switch (steps_away) {
			case 1:
			case 2:
				/* Close enough that sharp, flat, and dblflat
				 * may interfere, if coming in from above */
				if (place == PL_ABOVE &&
							(acc == '#' ||
							acc == '&' ||
							acc == 'B') ) {
					use_group_boundary = YES;
				}
				break;
			case 3:
				/* Close enough that sharp may interfere,
				 * if coming in from above */
				if (place == PL_ABOVE && acc == '#') {
					use_group_boundary = YES;
				}
				break;
			case 0:
				/* The note itself */
				break;
			case -1:
				/* Sharp, flat, and dblflat may interfere from
				 * either direction */
				if (acc == '#' || acc == '&' || acc == 'B') {
					use_group_boundary = YES;
				}
				break;

			case -2:
			case -3:
			case -4:
				/* Close enough that sharp, flat or
				 * double flat may interfere if coming
				 * from below. */
				if (place == PL_BELOW &&
							(acc == '#' ||
							acc == '&' ||
							acc == 'B') ) {
					use_group_boundary = YES;
				}
				break;

			default:
				/* this note is too far away to matter */
				break;
			}
		}
		else  if (gs_p->notelist[n].acclist[0] != 0) {
			/* A non-standard accidental. We don't really know
			 * whether it will interfere or not, since we don't
			 * know what it looks like. So we look up
			 * its dimensions, to get worst case, but can't
			 * know if there are "notches" we could take
			 * advantage of. */
			accsize = (gs_p->notelist[n].notesize == GS_NORMAL ?
					DFLT_SIZE : SMALLSIZE);
			if (place == PL_ABOVE && steps_away > 0) {
				/* Find the tallest accidental */
				farthest = -10000.0;	/* init far away */
				for (i = 0; i < 2 * MAX_ACCS; i += 2) {
					if (gs_p->notelist[n].acclist[i] == 0) {
						break;
					}
					this_height = height(
						gs_p->notelist[n].acclist[i],
						accsize,
						gs_p->notelist[n].acclist[i + 1]);
					if (this_height > farthest) {
						farthest = this_height;
					}
				}

				/* Convert to stepsizes and see if we are
				 * inside of that */
				if (farthest / Stepsize > steps_away) {
					use_group_boundary = YES;
				}
			}
			else if (place == PL_BELOW && steps_away < 0) {
				/* Find the accidental with lowest descent */
				farthest = -10000.0;	/* init far away */
				for (i = 0; i < 2 * MAX_ACCS; i += 2) {
					if (gs_p->notelist[n].acclist[i] == 0) {
						break;
					}
					this_height = descent(
						gs_p->notelist[n].acclist[i],
						accsize,
						gs_p->notelist[n].acclist[i + 1]);
					if (this_height > farthest) {
						farthest = this_height;
					}
				}

				/* Convert to stepsizes and see if we are
				 * inside of that */
				if (farthest / Stepsize > -steps_away) {
					use_group_boundary = YES;
				}
			}
		}
		if (use_group_boundary == YES) {
			return(gs_p->c[AW] + gs_p->padding
				+ vvpath(gs_p->staffno, gs_p->vno, PAD)->pad);
		}
	}

	/* it seems there are no accidentals in the way, so use the note
	 * boundary, rather than group boundary */
	return(gs_p->c[AX] + notehorz(gs_p, note_p, AW));
}


/* given a main list struct, search forward from there for the STAFF matching
 * the given staff. If fall off end of main list, return NULL */

static struct MAINLL *
next_staff(staff, mll_p)

int staff;		/* find this staff number */
struct MAINLL *mll_p;	/* where to start */

{
	/* walk through main list looking for desired staff */
	for (  ; mll_p != (struct MAINLL *) 0; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			if (mll_p->u.staff_p->staffno == staff) {
				return(mll_p);
			}
		}
	}

	/* didn't find it */
	return( (struct MAINLL *) 0);
}


/* Try to redo a curve that might be steep, to make it more beautiful.
 * Returns YES if successful.
 * Returns NO if the beautified curve collided, in which case the original
 * curve will have been restored.
 */

static int
try_redo_steep (try_p)

struct TRYBULGE *try_p;		/* curve information */

{
	double save_x, save_y;

	save_x = try_p->curvelist_p->next->x;
	save_y = try_p->curvelist_p->next->y;
	redo_steep(try_p->curvelist_p, try_p->endlist_p, try_p->place);
	if (stick_out(try_p) > 0.0) {
		/* Collided. Put the ugly one back */
		try_p->curvelist_p->next->x = save_x;
	 	try_p->curvelist_p->next->y = save_y;
		return(NO);
	}
	return(YES);
}

/*
 * Name:	redo_steep()
 *
 * Abstract:	Redo curves that are very steep.
 *
 * Returns:	void
 *
 * Description:	If the curve is "too steep", it redoes it
 *		so that it's horizontal at the outer end, rather than already
 *		sloping in towards the inner end.
 *		Caller needs to verify the redone curve doesn't collide
 *		with any groups. Assumes the curve contains 3 points.
 */

static void
redo_steep (first_p, last_p, place)

struct CRVLIST *first_p;	/* left endpoint of curve */
struct CRVLIST *last_p;		/* right endpoint of curve */
int place;			/* above or below */

{
	struct CRVLIST *mid_p;		/* new midpoint of curve */
	float delx;			/* distance from the end to test */
	float a, b;			/* some distances, see below */
	float midoff;			/* vert offset of midpoint */


	/*
	 * We need to test whether either end of the curve is sloping in.  So
	 * we really should find the derivative at the endpoints.  But we can
	 * approximate it close enough by finding the y value at a point "near"
	 * the end and comparing it to the end's y value.  "delx" tells how
	 * near.  We'd like to set it to a millionth of an inch, but due to
	 * apparent roundoff errors in curve_y_at_x(), we make it bigger than
	 * that: 1/4 the curve length, but never more than 2 stepsizes.
	 */
	if (last_p->x - first_p->x > 8 * Stepsize) {
		delx = 2 * Stepsize;
	} else {
		delx = (last_p->x - first_p->x) / 4.0;
	}
	if (place == PL_ABOVE) {
		/* if both near points are higher than end points, return */
		if (curve_y_at_x(first_p, first_p->x + delx) >= first_p->y &&
		    curve_y_at_x(first_p, last_p->x  - delx) >= last_p->y) {
			return;
		}
	} else {
		/* if both near points are lower than end points, return */
		if (curve_y_at_x(first_p, first_p->x + delx) <= first_p->y &&
		    curve_y_at_x(first_p, last_p->x  - delx) <= last_p->y) {
			return;
		}
	}

	/*
	 * The curve is steep.  First, we choose a new point,
	 * horizontally in the middle.  We are
	 * going to choose its vertical position so that the outer end of the
	 * curve starts out horizontal.
	 *
	 * Imagine the case of PL_BELOW where the left end is the outer (lower)
	 * end.  (The other 3 cases are symmetrical to this, and we can use the
	 * analogous result.)  Set the axes so that the left end is at the
	 * origin, and the right end is at (2*a, b).  The new point will be at
	 * (a, y), and we have to find y.  We know that y will be between 0 and
	 * b/2.  (Draw a picture.)
	 *
	 * Draw segments from (0, 0) to (a, y), and (a, y) to (2*a, b).  Then
	 * draw a line through (a, y) such that it forms the same angle theta
	 * with each of these segments.  The way calccurve() works, it will
	 * form two cubic arcs (in rotated coordinate systems) through the
	 * three points, such that the slope of each arc at each point forms
	 * the same angle theta with the segment next to it.  The last line we
	 * drew hits the X axis at a point which, with (0, 0) and (a, y) forms
	 * an isoceles triangle, where the angles at (0, 0) and (a, y) are
	 * both theta (because we're saying the arc at (0, 0) is horizontal).
	 * So the other angle is 180 degrees minus 2*theta.  That means the
	 * other angle the line forms with the X axis is 2*theta.  And that
	 * means the angle between the horizontal line through (a, y) and the
	 * second segment (a, y) to (2*a, b) is 3*theta.
	 *
	 * Looking at triangle (0, 0) to (a, 0) to (a, y), we see that
	 *	tan(theta) = y/a
	 * Looking at triangle (a, y) to (2*a, y) to (2*a, b), we see that
	 *	tan(3*theta) = (b-y)/a
	 * There is a trig identity
	 * 			3*tan(theta) - (tan(theta))^3
	 *	tan(3*theta) =	------------------------------
	 *				1 - 3*(tan(theta))^2
	 * Plug into this our values for tan(theta) and tan(3*theta), and you
	 * end up with
	 *	4 y^3 - 3 b y^2 - 4 a^2 y + a^2 b = 0
	 * To solve this cubic, we could do a whole routine for solving cubics,
	 * but it's easier to approximate as follows.
	 *
	 # Define
	 *	F(x) = 4 x^3 - 3 b x^2 - 4 a^2 x + a^2 b
	 * a and b are positive.  So at x = 0, F(x) > 0.  At x=b/2, F(x) < 0.
	 * Thus, as we expect, F(x) = 0 somewhere in between.  For the
	 * following algorithm to work, we need to know that F(x) is strictly
	 * decreasing (the slope is always negative).  The slope is
	 *	F'(x) = 12 x^2 - 6 b x - 4 a^2
	 * (the derivative).  It is a parabola opening upward and going through
	 * (0, -4a^2) and (b/2, -4a^2).  So it is always negative in this
	 * interval.
	 *
	 * The algorithm starts with lo = 0 and hi = b/2.  It draws a straight
	 * line between (lo, F(lo)) and (hi, F(hi)).  The point where this
	 * crosses the X axis we call "mid".  Based on whether F(mid) is
	 * positive or negative, we reset lo or hi to mid, and repeat the
	 * process until F(mid) is within b/1000 of the axis.  Then we will use
	 * mid as our y value in the picture.
	 */
	a = ABSDIFF(first_p->x, last_p->x) / 2.0;
	b = ABSDIFF(first_p->y, last_p->y);

	midoff= solvecubic(4.0, -3.0*b, -4.0*a*a, a*a*b, 0.0, b/2.0, POINT/2.0);

	mid_p = first_p->next;
	mid_p->x = first_p->x + a;	/* horizontally halfway between */

	/* handle the 4 cases, using the "mid" value for y in the diagram */
	if (place == PL_ABOVE) {
		if (first_p->y < last_p->y) {
			mid_p->y = last_p->y - midoff;
		} else {
			mid_p->y = first_p->y - midoff;
		}
	} else {
		if (first_p->y < last_p->y) {
			mid_p->y = first_p->y + midoff;
		} else {
			mid_p->y = last_p->y + midoff;
		}
	}
}


/* do final refinements of curve.
 * Remove any really tiny line segments.
 * Then reset the group north or south boundaries to reflect
 * the inclusion of the phrase mark.
 */

static void
final_touches(mll_p, begin_gs_p, end_gs_p, curvelist_p, place)

struct MAINLL *mll_p;			/* points to first group in curve */
struct GRPSYL *begin_gs_p;		/* first group in curve */
struct GRPSYL *end_gs_p;		/* last group in curve */
struct CRVLIST *curvelist_p;		/* the curve */
int place;				/* PL_ABOVE or PL_BELOW */

{
	int voice;	
	int staff;
	int index;	/* in coord array: RN or RS */
	int adj_index;	/* in coord array: AN or AS. Used to store how much
			 * to adjust for this phrase, in case there are
			 * nested phrases. */
	float y_c;	/* y of curve */
	float x1, y1;	/* lengths of segments in each dimension */
	float length;	/* of line segment */
	struct CRVLIST *crvlist_p;
	struct CRVLIST *extra_p;	/* pointer to point to be freed */
	struct GRPSYL *gs_p;	/* index through groups */


	if ( (mll_p == (struct MAINLL *) 0)
			|| (begin_gs_p == (struct GRPSYL *) 0)
			|| (end_gs_p == (struct GRPSYL *) 0)
			|| (curvelist_p == (struct CRVLIST *) 0) ) {
		pfatal("null pointer in final_touches()");
	}

	if (begin_gs_p->vno != end_gs_p->vno) {
		/* Must be a tie/slue to another voice. We don't attempt
		 * to try to avoid anything that is in the way. */
		return;
	}

	/* If there are really tiny line segments in a curve, the code for
	 * tapering the curve has problems because if the width of the curve
	 * is more than the length of the line and the angles work out just
	 * wrong, various warts, sometimes huge ones, appear on the curves.
	 * So go through the curve and if there are any really tiny lines,
	 * throw away one of the points and make the remaining point the
	 * average of what it was and what the discarded one was.
	 * With the new way of calculating curves, this is probably now
	 * unnecessary, but it seems safer to leave it in, just in case.
	 */
	for (crvlist_p = curvelist_p; crvlist_p->next != (struct CRVLIST *) 0;
						crvlist_p = crvlist_p->next) {
		x1 = crvlist_p->next->x - crvlist_p->x;
		y1 = crvlist_p->next->y - crvlist_p->y;
		length = sqrt(SQUARED(x1) + SQUARED(y1));
		if (length < 0.01) {
			/* replace with average */
			crvlist_p->x = (crvlist_p->x + crvlist_p->next->x)
								/ 2.0;
			crvlist_p->y = (crvlist_p->y + crvlist_p->next->y)
								/ 2.0;
			/* take the extra out of the list */
			extra_p = crvlist_p->next;
			if (crvlist_p->next->next != (struct CRVLIST *) 0) {
				crvlist_p->next->next->prev = crvlist_p;
			}
			crvlist_p->next = crvlist_p->next->next;
			if (crvlist_p->next == (struct CRVLIST *) 0) {
				/* avoid trying to take ->next of null ptr */
				break;
			}
			FREE(extra_p);
		}
	}

	/* adjust north or south of each group within the curve to account for
	 * the space needed for the curve */
	voice = begin_gs_p->vno;
	staff = begin_gs_p->staffno;
	if (place == PL_ABOVE) {
		index = RN;
		adj_index = AN;
	}
	else {
		index = RS;
		adj_index = AS;
	}

	for (gs_p = begin_gs_p;     ; gs_p = gs_p->next) {

		/* if hit end of measure go to next measure, skipping over
		 * any empty measure (which could happen if vscheme changed
		 * from 2 to 1 and back in the middle of the phrase) */
		while (gs_p == (struct GRPSYL *) 0) {
			mll_p = next_staff(staff, mll_p->next);
			if (mll_p == (struct MAINLL *) 0) {
				pfatal("fell off end of list while doing phrase marks");
			}
			gs_p = mll_p->u.staff_p->groups_p[voice - 1];
		}

		/* find where the curve y is at the x of the group, and
		 * adjust the north or south of the group appropriately,
		 * to be used later by any nesting phrase marks */
		y_c = curve_y_at_x(curvelist_p, gs_p->c[AX]);

		/* check for an inner tie. They don't affect the boundary */
		if ( ((index == RN) && (y_c < gs_p->c[index])) ||
				((index == RS) && (y_c > gs_p->c[index]))) {
			gs_p->c[adj_index] = 0.0;
		}
		else {
			if (place == PL_ABOVE) {
				gs_p->c[adj_index] = (y_c - gs_p->c[index]
								+ Stepsize);
			}
			else {
				gs_p->c[adj_index] =  - (gs_p->c[index] - y_c
								+ Stepsize);
			}
		}

		if (gs_p == end_gs_p) {
			/* On the last group on the phrase, this phrase
			 * only affects the west side--another phrase can
			 * start on this same group with considering this one */
			gs_p->phraseside |= WEST_SIDE;
			/* We are done with this curve */
			break;
		}
		else if (gs_p == begin_gs_p){
			/* Only affects east side of first group */
			gs_p->phraseside |= EAST_SIDE;
		}
		else {
			/* not of the end, so both side are relevant */
			gs_p->phraseside |= (EAST_SIDE | WEST_SIDE);
		}
	}
}


/* When phrase marks get relatively long, quite often we can flatten the
 * middle part of the curve and still get enough clearance, and they will
 * then look better and take up less space. So try to do that.
 * Return a pointer to a CRVLIST, which will be unchanged from what was
 * passed in if we decide we can't make that one any better using this
 * technique, or a new list if we can,
 * The passed-in curve is expected to have exactly 3 points, If we make a
 * new one, it will have 5 points. The endpoints will be like the original.
 * The y of the two points inwards from the ends will be where they would
 * have been at that x on the original. But we try to move the middle point
 * to make a shallower curve. First we try it half as shallow, and if that
 * runs into something, try 3/4. We only do this if the curve is "long"
 * and not very steep.
 */

static struct CRVLIST *
flatten_long_curve(info_p)

struct TRYBULGE *info_p;

{
	struct CRVLIST *crv_p; /* the orginal curve with 3 points */
	/* The new points. Since CRVLIST are linked lists where individual
	 * elements may normally get freed individually, we allocate them
	 * individually rather than as as array. */
	struct CRVLIST *newcrv0_p, *newcrv1_p, *newcrv2_p, *newcrv3_p, *newcrv4_p;

	float lev1y;	/* Y coord of point 1 of leveled 5 point curve */

	double flatten_segment;	/* distance from endpoints for additional points */
	float orig_lev_mid_x;	/* original midpoint X of leveled curve */
	float orig_lev_mid_y;	/* original midpoint Y of leveled curve */
				

	if (info_p->sintheta > STEEP_SIN || info_p->sintheta < - STEEP_SIN) {
		/* Too steep to bother */
		return(info_p->curvelist_p);
	}

	flatten_segment = info_p->length / 5.0;
	if (flatten_segment <  MIN_FLATTEN_SEG) {
		/* Too short to be worth flattening */
		return(info_p->curvelist_p);
	}

	if (flatten_segment > MAX_FLATTEN_SEG) {
		flatten_segment = MAX_FLATTEN_SEG;
	}

	/* allocate structures for the points of the new list */
	MALLOC(CRVLIST, newcrv0_p, 1);
	MALLOC(CRVLIST, newcrv1_p, 1);
	MALLOC(CRVLIST, newcrv2_p, 1);
	MALLOC(CRVLIST, newcrv3_p, 1);
	MALLOC(CRVLIST, newcrv4_p, 1);

	/* first connect just first 3 points, use for a leveled 3 point curve */
	newcrv0_p->prev = 0;
	newcrv0_p->next = newcrv1_p;
	newcrv1_p->prev = newcrv0_p;
	newcrv1_p->next = newcrv2_p;
	newcrv2_p->prev = newcrv1_p;
	newcrv2_p->next = 0;

	/* point at the old, 3 point list */
	crv_p = info_p->curvelist_p;


	/*
	 * Make a leveled version of the original 3 point list.  We will rotate
	 * the curve around the origin (0, 0), to make the curve level
	 * temporarily.
	 */

	/* copy the original (tilted) list to the new list */
	newcrv0_p->x = crv_p->x;
	newcrv0_p->y = crv_p->y;
	newcrv1_p->x = crv_p->next->x;
	newcrv1_p->y = crv_p->next->y;
	newcrv2_p->x = crv_p->next->next->x;
	newcrv2_p->y = crv_p->next->next->y;

	/* level the list by rotating by -theta */
	/* note that cos(-theta) = cos(theta), sin(-theta) = -sin(theta) */
	rotate_curve(newcrv0_p, info_p->costheta, -info_p->sintheta);

	orig_lev_mid_x = newcrv1_p->x;	/* remember this midpoint */
	orig_lev_mid_y = newcrv1_p->y;

	/* find Y of leveled 3 point curve at "flatten_segment" from left end */
	lev1y = curve_y_at_x(newcrv0_p, newcrv0_p->x + flatten_segment);

	/* join the rest of the structures to make a list of 5 points */
	newcrv2_p->next = newcrv3_p;
	newcrv3_p->prev = newcrv2_p;
	newcrv3_p->next = newcrv4_p;
	newcrv4_p->prev = newcrv3_p;
	newcrv4_p->next = 0;


	/***** set coords for a leveled 5 point list *****/

	/* left endpoint (0) is already set */

	/* copy right endpoint to be point 4 now */
	newcrv4_p->x = newcrv2_p->x;
	newcrv4_p->y = newcrv2_p->y;

	/* set point 1 to the intersection point we found */
	newcrv1_p->x = newcrv0_p->x + flatten_segment;
	newcrv1_p->y = lev1y;

	/* set point 3 symmetrically */
	newcrv3_p->x = newcrv4_p->x - flatten_segment;
	newcrv3_p->y = lev1y;

	/* try a midpoint with a Y value 1/4 of the way between the original
	 * leveled midpoint's Y and the Y of the new points (1 and 3) */
	newcrv2_p->x = orig_lev_mid_x;
	newcrv2_p->y = (3.0 * orig_lev_mid_y + lev1y) / 4.0;

	/* rotate the curve to its original tilt */
	rotate_curve(newcrv0_p, info_p->costheta, info_p->sintheta);

	/* Check if this new curve is okay */
	info_p->curvelist_p = newcrv0_p;

	if (stick_out(info_p) <= 0.0) {
		/* attempt worked */
		FREE(crv_p->next->next);
		FREE(crv_p->next);
		FREE(crv_p);
		return(newcrv0_p);
	}

	/* attempt isn't good, so put back the original,
	 * and get rid of the new */
	info_p->curvelist_p = crv_p;
	FREE(newcrv0_p);
	FREE(newcrv1_p);
	FREE(newcrv2_p);
	FREE(newcrv3_p);
	FREE(newcrv4_p);
	return(crv_p);
}


/*
 * Rotate all the points of the given curve around the origin, (0, 0), by the
 * angle theta.  Instead of passing in the actual angle theta, pass in its
 * cosine and sine, so that we can just use these values which were calculated
 * once before and stored.
 */
static void
rotate_curve(point_p, costheta, sintheta)

struct CRVLIST *point_p;	/* starts as first point in curve linked list */
double costheta, sintheta;	/* cos and sin of the angle to rotate by */

{
	float x, y;

	/* loop once per point in the curve */
	while (point_p != 0) {
		/* find this point's new (x, y) using the standard formula */
		x = point_p->x * costheta - point_p->y * sintheta;
		y = point_p->x * sintheta + point_p->y * costheta;

		/* store these new values */
		point_p->x = x;
		point_p->y = y;

		point_p = point_p->next;
	}
}


/* Determine effective tuplet extension value. Normally, the tupext tells
 * us how much room to leave to allow for the tuplet bracket. However,
 * if the tuplet doesn't get a bracket, this can cause us to leave extra
 * space. We do still need to leave room for the tuplet number
 * even if the bracket isn't there. And the bracket is not quite as tall
 * as the number, so away from the number we can get a little bit closer.
 * We only adjust the tupext value if the group
 * is west or east of the number. This is only done approximately.
 * We find the east and west of the number and add 2 Stepsizes on each side,
 * and if any part of the group would fall inside there, we use the full
 * tupextend if there is no bracket, or 75% of tupextend if there is a bracket.
 * In theory, when we are close to the number,
 * we could calculate a value between zero and tupext that would allow just
 * enough room to clear the number, if we knew exactly what angle the phrase
 * was going to have. But at this point we're still trying to determine that.
 * And usually the approximation will be good enough.
 * Worst case we should either bulge a little too much or slightly clip the
 * number with the phrase mark, so if we're off once in a while,
 * it shouldn't be too bad.
 * But if this isn't a tuplet, or its bracket is on the opposite side
 * as where we're trying to put a curve, then it doesn't count as all.
 */

static double
eff_tupext(gs_p, staff_p, side)

struct GRPSYL *gs_p;
struct STAFF *staff_p;
int side;		/* where the curve will be */

{
	struct GRPSYL *firstgs_p;	/* first group of tuplet */
	float west, east, height;	/* boundary of tuplet number */

	/* if not a tuplet, return tupextend as is */
	if (gs_p->tuploc == NOITEM) {
		return(gs_p->tupextend);
	}

	/* if curve is on opposite side as tuplet bracket, ignore tupextend */
	if (side != tupdir(gs_p, staff_p)) {
		return(0.0);
	}
	for (firstgs_p = gs_p; firstgs_p->tuploc != STARTITEM &&
					firstgs_p->tuploc != LONEITEM;
					firstgs_p = firstgs_p->prev) {
		;
	}
	(void) tupnumsize(firstgs_p, &west, &east, &height, staff_p);
	if (gs_p->c[AE] < west - 2.0 * Stepsize
				|| gs_p->c[AW] > east + 2.0 * Stepsize) {
		if (tupgetsbrack(firstgs_p) == YES) {
			return(0.75 * gs_p->tupextend);
		}
		else {
			return(0.0);
		}
	}


	/* if no special case applies, just return tupextend as is */
	return(gs_p->tupextend);
}


/* determine correct bend direction for curve, return UP or DOWN */

static int
bulge_direction(mll_p, gs1_p, note_index, curveno)

struct MAINLL *mll_p;	/* main list struct pointing to gs1_p */
struct GRPSYL *gs1_p;	/* curve will be from note in gs1_p to note in gs2_p */
int note_index;		/* which note in first group to tie */
int curveno;		/* index into slurto, or -1 for a tie */

{
	struct GRPSYL *gs_p;
	RATIONAL vtime1, vtime2;	/* beginning and ending time of group */
	int othervoice;			/* array index of other voice */


	/* If user explicitly set a bend direction, use that */
	if (curveno == -1 && gs1_p->notelist[note_index].tiedir != UNKNOWN) {
		return(gs1_p->notelist[note_index].tiedir);
	}
	else if (curveno >= 0 && gs1_p->notelist[note_index]
				.slurtolist[curveno].slurdir != UNKNOWN) {
		return(gs1_p->notelist[note_index].slurtolist[curveno].slurdir);
	}

	/* If there are 2 voices on the staff, bend is toward the stem */
	/* However, if the other voice is all spaces, pretend there is only
	 * one voice */
	if ( (mll_p->u.staff_p->groups_p[0] != (struct GRPSYL *) 0)
				&& (mll_p->u.staff_p->groups_p[1]
				!= (struct GRPSYL *) 0) ) {

		/* there are 2 voices */

		/* calculate begin and end time of tied group */
		vtime1 = Zero;
		for (gs_p = mll_p->u.staff_p->groups_p[gs1_p->vno - 1];
					gs_p != gs1_p; gs_p = gs_p->next) {
			vtime1 = radd(vtime1, gs_p->fulltime);
		}

		/* ending time is vtime1 plus the length of group 1. If group
		 * 1 is a grace note, use a very short time */
		if (EQ(gs1_p->fulltime, Zero)) {
			RATIONAL tiny;

			tiny.n = 1;
			tiny.d = 4 * MAXBASICTIME;
			vtime2 = radd(vtime1, tiny);
		}
		else {
			vtime2 = radd(vtime1, gs1_p->fulltime);
		}

		/* get array index of other voice to check it */
		othervoice = (gs1_p->vno == 1 ? 1 : 0);

		if (hasspace(mll_p->u.staff_p->groups_p[othervoice],
					vtime1, vtime2) == NO) {
			/* there IS another voice, so stem goes opposite */
			return(gs1_p->stemdir);
		}
	}

	/* if only one voice (either because there is actually only one
	 * or because there is effectively only one since the other is space)
	 * and there is only one note in group, then bend is opposite stem */
	if (gs1_p->nnotes < 2) {
		/* Stemless grace groups are a special case> Usually they
		 * are for showing prebends). Since they have no stem, we
		 * put the bend opposite the stem of the following group. */
		if (gs1_p->grpvalue == GV_ZERO && gs1_p->stemlen < Stepsize &&
				gs1_p->next != (struct GRPSYL *) 0) {
			return(gs1_p->next->stemdir == UP ? DOWN : UP);
		}
		return(gs1_p->stemdir == UP ? DOWN : UP);
	}
	
	/* if one voice on staff with more than one note in the group, all
	 * bend opposite the stem except the top if the stem is up or the
	 * bottom if the stem is down */
	if ( ((gs1_p->stemdir == DOWN) && (note_index == gs1_p->nnotes - 1))
			|| ((gs1_p->stemdir == UP) && (note_index == 0)) ) {
		return(gs1_p->stemdir);
	}
	else {
		return(gs1_p->stemdir == UP ? DOWN : UP);
	}
}


/* Returns the amount to add to given note's AX to get to where the
 * left end of a tie or slur should go. The note should be an "end" note.
 * If there is an incoming tie/slur we go just a little east of the
 * center, to avoid collision, but if there is to tie/slur to collide with,
 * then we go somewhat farther west of center.
 */

static double
left_endts_adj(mll_p, gs_p, note_p)

struct MAINLL *mll_p;	/* gs_p hangs off of here */
struct GRPSYL *gs_p;	/* the group of interest */
struct NOTE *note_p;	/* the note in gs_p of interest */

{
	struct GRPSYL *prevgrp_p;	/* group before gs_p */
	struct NOTE *prevgrp_note_p;	/* a note in prevgrp's note list */
	int n;				/* loop through list of notes */
	int s;				/* loop through slurto list */


	/* First check easy cases where we have a flag */
	if (note_p->tied_from_other == YES || note_p->slurred_from_other == YES) {
		return(COLLIDING_TS_ADJUST);
	}

	/* Get the previous group, if any */
	prevgrp_p = prevgrpsyl(gs_p, &mll_p);
	if (prevgrp_p == 0) {
		return(NONCOLLIDING_TS_ADJUST);
	}

	/* Loop through notes in previous group, checking if any are
	 * tied or slurs to the note of interest. */
	for (n = 0; n < prevgrp_p->nnotes; n++) {
		prevgrp_note_p = &(prevgrp_p->notelist[n]);
		/* is this note tied to the note of interest ? */
		if (prevgrp_note_p->letter == note_p->letter
				&& prevgrp_note_p->octave == note_p->octave
		 		&& prevgrp_note_p->tie == YES) {
			return(COLLIDING_TS_ADJUST);
		}
		/* is this note slurred to the note of interest ? */
		for (s = 0; s < prevgrp_note_p->nslurto; s++) {
			if (prevgrp_note_p->slurtolist[s].letter == note_p->letter
					&& prevgrp_note_p->slurtolist[s].octave
					== note_p->octave) {
				return(COLLIDING_TS_ADJUST);
			}
		}
	}
	return(NONCOLLIDING_TS_ADJUST);
}


/* This function is like the previous function, but for outgoing.
 * It is much simpler, since we have all we need in the NOTE. */

static double
right_endts_adj(note_p)

struct NOTE *note_p;	/* the note of interest */

{
	if (note_p->tie == YES || note_p->nslurto > 0) {
		return(COLLIDING_TS_ADJUST);
	}
	return(NONCOLLIDING_TS_ADJUST);
}
