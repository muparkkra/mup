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

/* This file contains functions for printing tablature staffs. */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/* position of the last bend arrow for each staff, or 0.0 if no
 * arrow on previous group */
static double Last_x_arrow[MAXSTAFFS + 1], Last_y_arrow[MAXSTAFFS + 1];

static double pr_tab_note P((int note_index, struct GRPSYL *gs_p,
		double y_adjust, struct MAINLL *mll_p));
static double pr_bstring P((struct GRPSYL *gs_p, struct NOTE *note_p,
		int note_index, double y_adjust, struct MAINLL *mll_p));
static void pr_b_arrow P((struct GRPSYL *gs_p, struct MAINLL *mll_p,
		double y_adjust));
static void pr_b_curve P((float *xlist, float *ylist, struct GRPSYL *gs_p,
		double y_adjust));
static void pr_arrowhead P((struct GRPSYL *gs_p, double y_adjust, int headchar));
static int bend_dir P((struct GRPSYL *gs_p, int n, struct GRPSYL *pgs_p,
		int carried_in));
static int is_carried_in_bend P((struct MAINLL *mll_p,
		struct MAINLL *prev_mll_p));



/* given a GRPSYL list for a tab staff, print all the notes in the list */

void
pr_tab_groups(gs_p, mll_p)

struct GRPSYL *gs_p;
struct MAINLL *mll_p;

{
	int n;
	double y_adjust;	/* to account for bend string space */
	struct GRPSYL *ngs_p;	/* next group */
	struct MAINLL *m_p;


	/* go through the list of groups */
	for (  ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {

		/* measure repeats are special */
		if (is_mrpt(gs_p) == YES) {
			pr_mrpt(gs_p, mll_p);
			continue;
		}

		/* multirests are special. Print on tab staff only if its
		 * corresponding tabnote staff is not printed */
		if (gs_p->is_multirest == YES) {
			if (svpath(gs_p->staffno - 1, VISIBLE)->visible == NO) {
				pr_multirest(gs_p, mll_p);
			}
			continue;
		}

		/* print the fret number and bend string
		 * for each note in the group. The
		 * y_adjust gets changed to account for space taken up
		 * by bend text strings */
		y_adjust = 0.0;
		for (n = 0; n < gs_p->nnotes; n++) {
			y_adjust = pr_tab_note(n, gs_p, y_adjust, mll_p);
		}

		/* if there were any bends, draw the arrow. Otherwise
		 * remember that there are no bends in progress */
		if (y_adjust > 0.0) {
			pr_b_arrow(gs_p, mll_p, y_adjust);
		}
		else {
			Last_y_arrow[gs_p->staffno] = 0.0;
			Last_x_arrow[gs_p->staffno] = 0.0;
		}

		/* print any slashes */
		if (gs_p->slash_alt > 0) {
			/* slashes on tab staff get hard-coded
			 * tilt value of 2.2 * Stdpad. */
			if (gs_p->stemdir == UP) {
				pr_slashes(gs_p, (double) gs_p->c[AX],
					(double) (gs_p->notelist[gs_p->nnotes - 1].c[AY]
					+ gs_p->stemlen), (double) -1.0,
					(double) 0.0, (double) (2.2 * Stdpad));
			}
			else {
				pr_slashes(gs_p, (double) gs_p->c[AX],
					(double) (gs_p->notelist[0].c[AY]
					- gs_p->stemlen), (double) 1.0,
					(double) 0.0, (double) (2.2 * Stdpad));
			}
		}

		if (gets_roll(gs_p, mll_p->u.staff_p, 0) == YES) {
			print_roll(gs_p);
		}

		/* print any "with" list items */
		pr_withlist(gs_p);

		/* on last group in measure, have to look ahead to next group
		 * if any to see if it has a bend from this note and is on
		 * the next score. If so, have to draw a horizontal line for
		 * the bend out to near the end of the current score */
		if (gs_p->next == (struct GRPSYL *) 0) {
			/* is the last group in the measure. See if we are
			 * at a scorefeed. First search for end of current
			 * measure. */
			for (m_p = mll_p; m_p != (struct MAINLL *) 0 &&
					m_p->str != S_BAR; m_p = m_p->next) {
				;
			}

			/* now go until we find either a feed or a staff */
			for (     ; m_p != (struct MAINLL *) 0;
							m_p = m_p->next) {
				if (m_p->str == S_FEED || m_p->str == S_STAFF) {
					break;
				}
			}
			if (m_p == (struct MAINLL *) 0 || m_p->str != S_FEED) {
				/* no next group or not at a scorefeed */
				continue;
			}

			/* See if there is another group in the next measure */
			if ((ngs_p = nextgrpsyl(gs_p, &mll_p))
						== (struct GRPSYL *) 0) {
				/* no next group */
				continue;
			}
			if (ngs_p->nnotes == 0) {
				continue;
			}

			for (n = 0; n < ngs_p->nnotes; n++) {
				if (HASREALBEND(ngs_p->notelist[n]) &&
					ngs_p->notelist[n].FRETNO == NOFRET) {
					/* next group has a real, non-prebend
					 * bend, so have to deal with it */
					break;
				}
				else if (HASNULLBEND(ngs_p->notelist[n])) {
					/* has null bend. have to do this one */
					break;
				}
			}
			if (n == ngs_p->nnotes) {
				/* no non-prebend bend */
				continue;
			}

			/* if we got here, this is the special case where we
			 * do need to draw the line, so do it */
			do_linetype(L_NORMAL);
			if (Last_y_arrow[gs_p->staffno] != 0.0) {
				draw_line( (double) (Last_x_arrow[gs_p->staffno]),
					(double) (Last_y_arrow[gs_p->staffno]),
					(double) (PGWIDTH
					- eff_rightmargin(mll_p) - Stepsize),
					(double) (Last_y_arrow[gs_p->staffno]));
				Last_x_arrow[gs_p->staffno] =
					PGWIDTH - eff_rightmargin(mll_p)
					- Stepsize;
			}
			else {
				/* the bend is on the next score, but not a
				 * continuation bend. so draw an arrow towards
				 * the margin, followed by a short
				 * dashed line */
				float xlist[6], ylist[6];  /* curve coords */
				struct GRPSYL dummy_gs;  /* for curve end */


				/* set place for beginning of curve */
				xlist[0] = gs_p->notelist[n].c[AX] + Stdpad +
					notehorz(gs_p, &(gs_p->notelist[n]), RE);
				ylist[0] = gs_p->notelist[n].c[AY];

				/* fill in a dummy GRPSYL struct with just
				 * the pieces of information that pr_b_curve
				 * will need to draw a bend curve */
				dummy_gs.c[AX] =  PGWIDTH
						- eff_rightmargin(mll_p)
						- 2.5 * Stepsize;
				dummy_gs.c[AN] = ylist[0] + 2.4 * Stepsize
							* TABRATIO;
				/* if we're too close to the margin, make the
				 * curve a little longer, so it will look
				 * decent, even if it runs into the margin
				 * a little ways */
				if (dummy_gs.c[AX] - xlist[0]
							< 3.0 * Stepsize) {
					dummy_gs.c[AX] = 3.0 * Stepsize;
					/* If that still didn't move it enough
					 * to avoid having the bend backwards,
					 * adjust some more */
					if (dummy_gs.c[AX] - xlist[0]
							< 2.0 * Stepsize) {
						dummy_gs.c[AX] = xlist[0]
							+ 2.0 * Stepsize;
					}  
				}

				/* draw the curve */
				pr_b_curve(xlist, ylist, &dummy_gs, 0.0);

				/* draw an arrow at the end of the curve */
				pr_muschar( (double) dummy_gs.c[AX],
					(double) (dummy_gs.c[AN] -
					(height(FONT_MUSIC, DFLT_SIZE, C_UWEDGE)
					/ 2.0)), C_UWEDGE, DFLT_SIZE, FONT_MUSIC);

				/* draw a short dashed line to the right
				 * from the arrow, to indicate the actual
				 * bend is on the next score */
				do_linetype(L_DASHED);
				ylist[5] += 2.0 * Stdpad;
				draw_line( (double) xlist[5] + Stdpad,
					(double) ylist[5],
					(double) xlist[5] + 3.5 * Stepsize,
					(double) ylist[5]);
			}
		}
	}
}


/* print things for a tab staff note. Return any adjustment to North needed
 * to account for bend string */

static double
pr_tab_note(note_index, gs_p, y_adjust, mll_p)

int note_index;		/* which note in gs_p */
struct GRPSYL *gs_p;
double y_adjust;	/* to account for bend string space */
struct MAINLL *mll_p;	/* main list struct pointing to gs_p */

{
	struct NOTE *note_p;
	char *fretstr;	/* text string to print for fret number */

	note_p = &(gs_p->notelist[note_index]);

	/* Skip if inhibitprint flag is set (for a tied-to note) */
	if (note_p->inhibitprint == YES) {
		return(0.0);
	}

	/* print fret number (with parentheses if appropriate) */
	fretstr = fret_string(note_p, gs_p);
	if ( *fretstr != '\0') {

		if (vvpath(gs_p->staffno, gs_p->vno, TABWHITEBOX)->tabwhitebox == YES) {
			/* Put a white box behind the number, so the line
			 * doesn't go through it, making it easier to read. */
			do_whitebox( note_p->c[AX] - strwidth(fretstr) / 2.0,
				note_p->c[AY] - strheight(fretstr) / 2.0,
				note_p->c[AX] + strwidth(fretstr) / 2.0,
				note_p->c[AY] + strheight(fretstr) / 2.0);
		}

		pr_string( (double) (note_p->c[AX] - (strwidth(fretstr) / 2.0)),
			(double) (note_p->c[AY] + (strheight(fretstr) / 2.0)
			- strascent(fretstr)),
			fretstr, J_LEFT, gs_p->inputfile, gs_p->inputlineno);
	}

	/* print a bend string if appropriate */
	if ( HASBEND((*note_p)) ) {
		if (HASREALBEND(*note_p)) {
			y_adjust += pr_bstring(gs_p, note_p, note_index,
							y_adjust, mll_p);
		}
		else {
			/* need to return a little bit, so later code
			 * knows that there was a bend of some sort */
			if (y_adjust == 0.0) {
				y_adjust = STDPAD;
			}
		}
	}
	return (y_adjust);
}


/* print the bend string ("full", "1/2", etc). Return its height */

static double
pr_bstring(gs_p, note_p, note_index, y_adjust, mll_p)

struct GRPSYL *gs_p;	/* group having a bend */
struct NOTE *note_p;	/* which note in gs_p has the bend */
int note_index;		/* index into gs_p->notelist of note_p */
double y_adjust;	/* to account for bend strings done previously
			 * for this group (on other notes in the group */
struct MAINLL *mll_p;	/* main list struct pointing to gs_p */

{
	char *bstring;
	double x_adjust;	/* to center upward and left justify
				 * downward, to not collide with curve */
	struct GRPSYL *pgs_p;	/* previous group */
	int n;			/* index through notelist */


	/* get what to print for the bend */
	bstring = bend_string(note_p);

	/* generally, the string should be centered. */
	x_adjust = strwidth(bstring) / 2.0;

	/* However, if not a prebend and the bend is downward,
	 * then we have to move the bend string over so it 
	 * doesn't collide with the curve. So first get the previous group
	 * and see it is has a bend too. If it does, see if the bend is
	 * downward, and if so, change to left justify rather than center */
	if (note_p->FRETNO == NOFRET && (pgs_p = prevgrpsyl(gs_p, &mll_p))
						!= (struct GRPSYL *) 0) {

		for (n = 0; n < pgs_p->nnotes; n++) {
			if (pgs_p->notelist[n].STRINGNO
					== gs_p->notelist[note_index].STRINGNO
					&& HASBEND(pgs_p->notelist[n])) {
				if (bend_dir(gs_p, note_index, pgs_p, NO) == DOWN) {
					x_adjust = 0.0;
				}
			}
		}
	}

	/* print the bend string */
	pr_string( (double) (gs_p->c[AX] - x_adjust),
			(double) (gs_p->c[AN] - strascent(bstring) - y_adjust),
			bstring, J_LEFT, gs_p->inputfile, gs_p->inputlineno);

	/* return the height of what was just printed */
	return(strheight(bstring));
}


/* print bend arrow */

static void
pr_b_arrow(gs_p, mll_p, y_adjust)

struct GRPSYL *gs_p;
struct MAINLL *mll_p;
double y_adjust;

{
	struct GRPSYL *pgs_p;		/* previous grpsyl */
	struct MAINLL *prev_mll_p;	/* where pgs_p is connected */
	float xlist[6], ylist[6];	/* coords of bend curve */
	int n;				/* note index */
	int staffno;
	int carried_in;			/* YES if bend is carried in */
	int dir;			/* bend direction */


	staffno = gs_p->staffno;

	/* leave a little room between bend string and the arrow. This is
	 * especially needed when bending down, so the string and curve
	 * don't collide */
	y_adjust += 2.0 * Stdpad;

	/* find the first note with a bend on it */
	for (n = 0; n < gs_p->nnotes; n++) {
		if (HASBEND(gs_p->notelist[n])) {
			break;
		}
	}

	/* the function isn't supposed to get called unless there really
	 * is a bend somewhere on the group */
	if (n == gs_p->nnotes) {
		pfatal("pr_b_arrow couldn't find note with bend");
	}

	/* check if prebend or non-prebend */
	if (gs_p->notelist[n].FRETNO != NOFRET
				&& ! HASNULLBEND(gs_p->notelist[n])) {
		/* this is a prebend */
		/* draw the line straight up from the fret number */
		do_linetype(L_NORMAL);
		draw_line( (double) (gs_p->c[AX]),
			(double) (gs_p->notelist[n].c[AN] + Stdpad),
			(double) (gs_p->c[AX]),
			(double) (gs_p->c[AN] - y_adjust));

		/* draw triangle at top */
		pr_arrowhead(gs_p, y_adjust, C_UWEDGE);
		return;
	}

	/* a normal bend, not a prebend */
	prev_mll_p = mll_p;
	if ((pgs_p = prevgrpsyl(gs_p, &prev_mll_p)) == (struct GRPSYL *) 0) {
		l_ufatal(gs_p->inputfile, gs_p->inputlineno,
				"no previous chord for bend");
	}

	carried_in = is_carried_in_bend(mll_p, prev_mll_p);

	if (Last_y_arrow[staffno] == 0.0) {
		/* not the continuation of an in-progress bend */

		/* do each note that has a bend */
		for (n = 0; n < gs_p->nnotes; n++) {
			if ( ! HASBEND(gs_p->notelist[n])) {
				continue;
			}

			if ((mll_p != prev_mll_p && pgs_p->c[AE] > gs_p->c[AX])
					|| carried_in == YES) {

				/* either an intervening scorefeed or
				 * carried in to subsequent ending,
				 * so just start a bit west
				 * of the current group */
				xlist[0] = gs_p->c[AW] - 2.0 * Stepsize;
				ylist[0] = gs_p->c[AY] +
						gs_p->notelist[n].stepsup
						* Stepsize * TABRATIO;
			}
			else {
				/* beginning of bend arrow is at east
				 * and just a bit
				 * above the center of the fret number
				 * of the previous group */
				xlist[0] = pgs_p->c[AE] + Stdpad;
				ylist[0] = gs_p->notelist[n].c[AY] + Stdpad;
			}

			pr_b_curve(xlist, ylist, gs_p, y_adjust);
		}
		pr_arrowhead(gs_p, y_adjust, C_UWEDGE);
	}
	else {
		/* continuation of an in-progress bend */

		/* find the note that has a bend. Only allowed to be
		 * one note with a continuation bend, or could be a release,
		 * in which case we use the first note we find */
		for (n = 0; n < gs_p->nnotes; n++) {
			if ( HASBEND(gs_p->notelist[n])) {
				break;
			}
		}
		if (n == gs_p->nnotes) {
			pfatal("unable to find note with continuation bend");
		}

		/* find the starting point of the bend curve */
		if ((mll_p != prev_mll_p && pgs_p->c[AE] > gs_p->c[AX])
					|| carried_in == YES) {

			/* must have been an intervening scorefeed,
			 * so just start a bit west
			 * of the current group */
			xlist[0] = gs_p->c[AW] - 2.0 * Stepsize;
			Last_y_arrow[staffno] = gs_p->c[AN] - y_adjust;

			/* need to adjust more if bending is actually up */
			if (bend_dir(gs_p, n, pgs_p, carried_in) == UP) {
				Last_y_arrow[staffno] -= 3.0 * Stepsize;
			}

			/* null bends carried over a scorefeed
			 * have to be done specially */
			if (HASNULLBEND(gs_p->notelist[n])) {
				Last_y_arrow[staffno] = gs_p->notelist[n].c[AN]
					+ (2 * gs_p->notelist[n].STRINGNO
					* Stepsize * TABRATIO);
				if (gs_p->notelist[n].STRINGNO == 0) {
					Last_y_arrow[staffno] += 2.0 * Stepsize
							 * TABRATIO;
				}
			}
		}
		else {
			/* beginning of bend curve is where last
			 * one left off */
			xlist[0] = Last_x_arrow[staffno];
		}
		ylist[0] = Last_y_arrow[staffno];

		/* determine whether curve goes up or down
		 * and find its endpoint */
		if ((dir = bend_dir(gs_p, n, pgs_p, carried_in)) == UP) {
			/* bending up some more */
			pr_b_curve(xlist, ylist, gs_p, y_adjust);
			pr_arrowhead(gs_p, y_adjust, C_UWEDGE);
		}
		else if (dir == DOWN) { /* bending back downwards */

			if ( ! HASREALBEND(gs_p->notelist[n])) {
				/* null bend. Have to draw curve all the way
				 * down to the appropriate "note" */
				y_adjust = gs_p->c[AN] -
					gs_p->notelist[n].c[AN] - Stdpad -
					height(FONT_MUSIC, DFLT_SIZE, C_WEDGE)
					/ 2.0;
			}
			pr_b_curve(xlist, ylist, gs_p, y_adjust);
			pr_arrowhead(gs_p, y_adjust, C_WEDGE);
		}
		else {
			struct GRPSYL tempgs;
			/* must be a tied bend */
			do_linetype(L_NORMAL);
			draw_line(Last_x_arrow[staffno],
				Last_y_arrow[staffno],
				(double) (gs_p->c[AX]),
				Last_y_arrow[staffno]);
			/* We don't have a character for an arrow
			 * pointing right, so we print a regular one
			 * while rotated, adjusting the coordinates
			 * to compensate for the rotation. */	
			/* Note that we only fill in the fields in tempgs
			 * that pr_arrowhead will use. */
			tempgs.staffno = staffno;
			tempgs.c[AX] = gs_p->c[AN] - y_adjust;
			tempgs.c[AN] = - gs_p->c[AX];
			do_rotate(90);
			pr_arrowhead(&tempgs, 0.0, C_WEDGE);
			do_rotate(-90);
			Last_x_arrow[staffno] = gs_p->c[AX];
			Last_y_arrow[staffno] = gs_p->c[AN] - y_adjust;
		}
	}
}


/* given the first point of a bend curve, plus the gs_p and y adjustment for
 * the end of the curve, figure out the points of the curve and draw it */

static void
pr_b_curve(xlist, ylist, gs_p, y_adjust)

float *xlist;	/* arrays of x & y coordinates, with 6 members */
float *ylist;
struct GRPSYL *gs_p;
double y_adjust;	/* how far from the north of the gs_p the curve is */

{
	float xlen, ylen;


	/* last point of bend arrow is just below
	 * the bend string */
	xlist[5] = gs_p->c[AX];
	ylist[5] = gs_p->c[AN] - y_adjust - Stdpad;

	/* fill in intermediate points */
	xlen = xlist[5] - xlist[0];
	ylen = ylist[5] - ylist[0];
	xlist[1] = xlist[0] + xlen * 0.15;
	ylist[1] = ylist[0];
	xlist[2] = xlist[0] + xlen * 0.7;
	ylist[2] = ylist[0] + ylen * 0.2;
	xlist[3] = xlist[5] - xlen * 0.2;
	ylist[3] = ylist[5] - ylen * 0.7;
	xlist[4] = xlist[5];
	ylist[4] = ylist[5] - ylen * 0.15;

	/* draw the curve */
	do_linetype(L_NORMAL);
	pr_allcurve(xlist, ylist, 6, W_NORMAL, NO);
}


/* print arrowhead at the end of a bend curve */

static void
pr_arrowhead(gs_p, y_adjust, headchar)

struct GRPSYL *gs_p;
double y_adjust;
int headchar;	/* C_WEDGE or C_UWEDGE */

{
	pr_muschar( (double) gs_p->c[AX],
		(double) (gs_p->c[AN] - y_adjust -
		(height(FONT_MUSIC, DFLT_SIZE, headchar) / 2.0)),
		headchar, DFLT_SIZE, FONT_MUSIC);

	Last_y_arrow[gs_p->staffno] = gs_p->c[AN] - y_adjust;
	Last_x_arrow[gs_p->staffno] = gs_p->c[AX];
}


/* Given a GRPSYL that has a continuation bend, or is a carried-in bend,
 * return UP if it is bending upwards from the previous bend,
 * DOWN if bending down, and UNKNOWN if straight (for tied bend).
 * UNKNOWN isn't really accurate, but that's the third value we have
 * defined for direction, so we use it.
 */

static int
bend_dir(gs_p, n, pgs_p, carried_in)

struct GRPSYL *gs_p;	/* group having a continuation bend */
int n;			/* the note index in gs_p having the bend */
struct GRPSYL *pgs_p;	/* previous group */
int carried_in;		/* YES if bend is carried in */

{
	RATIONAL thisgrp_bend;	/* rational version of bend on this group */
	RATIONAL prevgrp_bend;	/* rational version of bend on previous group */
	int i;			/* index through pgs_p notes */


	if (gs_p == (struct GRPSYL *) 0 || pgs_p == (struct GRPSYL *) 0) {
		pfatal("null pointer passed to bend_dir");
	}

	/* get rational version of the bend distance on continuation note */
	thisgrp_bend = ratbend( &(gs_p->notelist[n]) );

	/* find the corresponding note in the previous group */
	for (i = 0; i < pgs_p->nnotes; i++) {
		if (pgs_p->notelist[i].STRINGNO == gs_p->notelist[n].STRINGNO) {
			break;
		}
	}
	if (i == pgs_p->nnotes) {
		pfatal("couldn't find the note being bent from");
	}

	/* get rational version of that bend */
	if ( ! HASBEND(pgs_p->notelist[i]) ) {
		/* if this is a carried-in bend, it may not be a continuation
		 * bend, but in that case, it has to be bending up */
		if (carried_in == YES) {
			return(UP);
		}
		else {
			l_ufatal(gs_p->inputfile, gs_p->inputlineno,
				HASNULLBEND(gs_p->notelist[n]) ?
				"bend release not preceded by a bend" :
				"no bend on note supposedly being bent from");
		}
	} 
	prevgrp_bend = ratbend( &(pgs_p->notelist[i]) );

	/* compare the bends */
	if (GT(thisgrp_bend, prevgrp_bend)) {
		return(UP);
	}
	else if (LT(thisgrp_bend, prevgrp_bend)) {
		return(DOWN);
	}
	else {
		return(UNKNOWN);
	}
}


/* Return YES if the bend is carried in to a subsequent ending, NO if just
 * an ordinary bend.
 */

static int
is_carried_in_bend(mll_p, prev_mll_p)

struct MAINLL *mll_p;		/* where group with a bend is attached */
struct MAINLL *prev_mll_p;	/* where the previous group is attached */

{
	int num_startitems;	/* how many endingloc==STARTITEM were found */


	/* if both groups are in the same measure, then it's definitely
	 * not a carried in bend */
	if (mll_p == prev_mll_p) {
		return(NO);
	}

	/* go forward from prev_mll_p until we get to mll_p. If we encounter
	 * more than 1 STARTITEM in endingloc, checking both normal and
	 * pseudo bars, then this was a carried in bend. */
	num_startitems = 0;
	for (   ; prev_mll_p != (struct MAINLL *) 0 && prev_mll_p != mll_p;
					prev_mll_p = prev_mll_p->next) {

		switch (prev_mll_p->str) {

		case S_BAR:
			if (prev_mll_p->u.bar_p->endingloc == STARTITEM) {
				if (++num_startitems > 1) {
					/* it is carried in bend */
					return(YES);
				}
			}
			break;

		case S_CLEFSIG:
			if (prev_mll_p->u.clefsig_p->bar_p != (struct BAR *) 0) {
				if (prev_mll_p->u.clefsig_p->bar_p->endingloc
							== STARTITEM) {
					if (++num_startitems > 1) {
						return(YES);
					}
				}
			}
			break;

		default:
			/* nothing else is relevant at this point */
			break;
		}
	}

	/* fell out without finding 2 STARTITEMS, so not carried in */
	return(NO);
}


/* given internal representation of bend info,
 * return string representation. Returns string in
 * static area that is overwritten on each call.
 */

char *
bend_string(note_p)

struct NOTE *note_p;

{
	static char buff[12];
	int intpart, num, den;

	/* separate internal representation into integer, numerator,
	 * and denominator */
	intpart = BENDINT(*note_p);
	num = BENDNUM(*note_p);
	den = BENDDEN(*note_p);

	/* construct the string representation */
	buff[0] = FONT_HR;
	buff[1] = adj_size(DFLT_SIZE, Staffscale, (char *) 0, -1);

	if (intpart == 1 && num == 0) {
		(void) strcpy(buff+2, "full");
	}
	else if (intpart == 0 && num == 0) {
		/* no bend at all */
		buff[0] = '\0';
	}
	else if (num == 0) {
		/* integer part only, no fraction */
		(void) sprintf(buff+2, "%d", intpart);
	}
	else if (intpart == 0) {
		/* fraction only */
		(void) sprintf(buff+2, "%d/%d", num, den);
	}
	else {
		/* both integer and fractional parts */
		(void) sprintf(buff+2, "%d %d/%d", intpart, num, den);
	}
	return(buff);
}


/* given a NOTE on a tab staff, return char * of what is to be printed
 * for the fret number. Returned string is a static area
 * that is overwritten on each call.
 */

char *
fret_string(note_p, gs_p)

struct NOTE *note_p;
struct GRPSYL *gs_p;	/* group containing the note */

{
	static char fretbuff[8];
	int size;


	/* if no fret, return "" */
	if (note_p->FRETNO == NOFRET) {
		fretbuff[0] = '\0';
		return(fretbuff);
	}

	if (note_p->notesize == GS_SMALL) {
		size = SMFRETSIZE;
	}
	else if (note_p->notesize == GS_TINY) {
		size = TINYFRETSIZE;
	}
	else {
		size = DFLT_SIZE;
	}
	size = adj_size(size, Staffscale, gs_p->inputfile, gs_p->inputlineno);

	/* make proper string for X-note or normal fret number, in parentheses
	 * if appropriate */
	if (IS_MUSIC_FONT(note_p->headfont)) {
		(void) sprintf(fretbuff,
			(note_p->FRET_HAS_PAREN ? "%c%c(%c%c%c)" : "%c%c%c%c%c"),
				FONT_HB, size, mfont2str(note_p->headfont),
				size, note_p->headchar);
	}
	else {
		(void) sprintf(fretbuff,
			(note_p->FRET_HAS_PAREN ? "%c%c(%d)" : "%c%c%d"),
			FONT_HB, size, note_p->FRETNO);
	}
	return(fretbuff);
}


/* print a TAB "clef" which is really just the word "TAB" written vertically.
 * By convention, this only gets printed once per staff at the very beginning
 * of the song. To keep things simple, the width of the clef is always 
 * returned as if the clef was printed even when it really isn't */

double
pr_tabclef(staffno, x, really_print, size)

int staffno;
double x;
int really_print;
int size;

{
	static int did_tab_clef[MAXSTAFFS + 1];	/* set to YES once we print a
			 * TAB clef on a given staff. Convention is to print
			 * this "clef" only at the very beginning of a song. */
	int print_this_time;	/* YES or NO based on setting of
				 * printtabclef in SSV and--if that is
				 * PTC_FIRST--on did_tab_clef */
	int printtabclef;
	int stafflines;
	int ptsize;	/* point size to use for "TAB" */
	double width, widest;	/* of the letters in "TAB" */
	double height = 0.0;
	char letter[4];	/* internal format version of one letter of "TAB" */
	char *tabstr;	/* pointer through "TAB" */
	double y = 0.0;


	/* adjust the size based on how many stafflines there are */
	stafflines = svpath(staffno, STAFFLINES)->stafflines;
	if (stafflines < 4) {
		ptsize = 7;
	}
	else if (stafflines == 4) {
		ptsize = 13;
	}
	else if (stafflines == 5) {
		ptsize = 16;
	}
	else {
		ptsize = 20;
	}

	/* if small clef, adjust the size (actually, this shouldn't
	 * ever happen unless we change some other things some day, but this
	 * way we will be prepared if/when that happens). */
	if (size != DFLT_SIZE) {
		ptsize = (int) (ptsize * (size / DFLT_SIZE));
	}
	ptsize = adj_size(ptsize, Staffscale, (char *) 0, -1);

	/* Decide if we should actually print this time. */
	print_this_time = NO;
	if (really_print == YES) {
		printtabclef = svpath(staffno, STAFFLINES)->printtabclef;

		/* If not doing PTC_FIRST, then did_tab_clef is irrelevant.
		 * and if it had been set to that in the past, we reset
		 * did_tab_clef in case the user decides to change it
		 * to PTC_FIRST later. */
		if (printtabclef != PTC_FIRST) {
			did_tab_clef[staffno] = NO;
		}

		if ( printtabclef == PTC_ALWAYS ||
				(printtabclef == PTC_FIRST &&
				did_tab_clef[staffno] == NO) ) {
			print_this_time = YES;
		}

	}
	else {
		/* In case lint thinks it could be used when not set */
		printtabclef = PTC_FIRST;
	}

	/* print/get width of "TAB" */
	for (widest = 0, tabstr = "TAB"; *tabstr != '\0'; tabstr++) {

		/* create internal format string for current letter */
		(void) sprintf(letter, "%c%c%c", FONT_HB, ptsize, *tabstr);
		/* get its width */
		width = strwidth(letter);

		/* save the widest letter width */
		if (width > widest) {
			widest = width;
		}

		/* if we're really supposed to print,
		 * print this letter of "TAB" */
		if (really_print == YES && print_this_time == YES) {

			/* figure out where to place vertically */
			if (*tabstr == 'T') {
				/* place the top letter */
				height = strheight(letter);
				y = Staffs_y[staffno] + height / 2.0
								+ Stdpad;
			}
			else {
				/* move subsequent letters down by height
				 * of the previous */
				y -= height;
			}

			/* print the letter with a little space before */
			pr_string(x + 3.0 * Stdpad, y, letter, J_LEFT,
							(char *) 0, -1);
		}
	}

	/* Only print once per staff if that what user wants */
	if (really_print == YES && printtabclef == PTC_FIRST) {
		did_tab_clef[staffno] = YES;
	}

	/* allow some space on either side */
	return(widest + 6.0 * Stdpad);
}
