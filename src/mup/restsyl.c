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
 * Name:	restsyl.c
 *
 * Description:	This file contains functions for setting the relative
 *		horizontal and vertical coordinates of all groups that
 *		contain a rest or space (grpcont != GC_NOTES), and the relative
 *		horizontal coordinates of syllables.  It then completes
 *		the relative horizontal work by setting the relative
 *		horizontal coords of chords.  But before it does that last
 *		step, it scales all the relative coords set so far according
 *		to staffscale.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

#define PK_LEFT		(0)
#define PK_RIGHT	(1)
#define PK_CENTER	(2)
#define PK_NONE		(3)

static double highcoord P((struct GRPSYL *gs_p, struct GRPSYL *altgs_p));
static double lowcoord P((struct GRPSYL *gs_p, struct GRPSYL *altgs_p));
static void procrests P((struct CHORD *ch_p, struct STAFF *staff_p,
		struct GRPSYL *gs_p, double limhigh, double limlow));
static int hasspace_or_not_vno(struct GRPSYL *gs_p, RATIONAL vtime,
		RATIONAL vtime2, int vno);
static void procspaces P((struct GRPSYL *gs_p));
static int v3pack P((struct GRPSYL *g_p[], int numgrps));
static void fixclef P((struct GRPSYL *gs1_p, struct CHORD *pch_p));
static void restsize P((struct GRPSYL *gs_p, float *wid_p, float *asc_p,
		float *des_p));
static void procsyls P((struct GRPSYL *gs_p));
static void apply_staffscale P((void));
static void room4subbars P((void));
static void relxchord P((void));
static int collision_danger P((struct GRPSYL *g1_p, struct GRPSYL *g2_p));
static struct CHORD *prevchord P((struct MAINLL *mainll_p, struct CHORD *ch_p));
static double get_east_limit P((struct GRPSYL *gs_p, struct CHORD *ch_p));
static double get_west_limit P((struct GRPSYL *gs_p, struct CHORD *ch_p,
		struct MAINLL *mainll_p));
static void pedalroom P((void));
static struct CHORD *closestchord P((double count, struct CHORD *firstch_p));
static void fixspace P((void));

/*
 * Name:        restsyl()
 *
 * Abstract:    Sets all relative coords for rests and spaces, and horizontal
 *		ones for syllables; set relative horz. coords of chords.
 *
 * Returns:     void
 *
 * Description: This function loops through the main linked list, finding every
 *		STAFF structure.  For groups, it calls procrests() to process
 *		every rest in the linked list, and procspaces() to process
 *		every space in the linked list.  For syllables, it calls
 *		procsyls().  At the end it calls apply_staffscale() to apply
 *		staffscale to all the relative coords set so far, and then
 *		relxchord() to set the relative horizontal coords of chords.
 */

void
restsyl()

{
	register struct MAINLL *mainll_p; /* point item in main linked list */
	struct MAINLL *mll_p;		/* another MLL pointer */
	struct STAFF *staff_p;		/* point at a staff */
	struct STAFF *stafflist[MAXSTAFFS + 1];	/* point to this meas's staffs*/
	float limhigh[MAXSTAFFS + 1];	/* high y coord of limit of groups */
	float limlow[MAXSTAFFS + 1];	/* low y coord of limit of groups */
	int vscheme;			/* voice scheme */
	int v;				/* index into verse headcell array */
	struct CHORD *ch_p;		/* point at a chord */
	struct CHORD *pch_p;		/* point at previous chord */
	struct GRPSYL *gs1_p;		/* point at a group */


	debug(16, "restsyl");
	initstructs();


	init_rectab();		/* prepare for stacking accidentals */

	/*
	 * Loop down the main linked list looking for each chord list
	 * headcell.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		if (mainll_p->str != S_CHHEAD)
			continue;	/* skip everything but chord HC */

		/*
		 * For each visible staff in this measure, find the high and
		 * low limits of the relevant voices, so that we know what to
		 * avoid when placing rests in other voices.
		 */
		for (mll_p = mainll_p->next; mll_p->str == S_STAFF;
				mll_p = mll_p->next) {

			staff_p = mll_p->u.staff_p;
			stafflist[staff_p->staffno] = staff_p;
			vscheme = svpath(staff_p->staffno, VSCHEME)->vscheme;
			if (staff_p->visible == NO) {
				continue;
			}

			/*
			 * If there is more than one voice, each voice's rests
			 * have to worry about avoiding other voices' notes.
			 * So find how high voice 2 and how low voice 1, get,
			 * in this measure.  But voice 3 can "stand in" for
			 * either, so pass that in too.  If it's null, that's
			 * okay, the subroutines handle that.
			 */
			if (vscheme != V_1) {
				limhigh[staff_p->staffno] =
					highcoord(staff_p->groups_p[1],
					staff_p->groups_p[2]);
				limlow[staff_p->staffno] =
					lowcoord(staff_p->groups_p[0],
					staff_p->groups_p[2]);
			} else {
				/* prevent uninitialized var, though not used*/
				limhigh[staff_p->staffno] = 0;
				limlow[staff_p->staffno] = 0;
			}
		}

		/*
		 * Loop through each chord in this list.
		 */
		for (ch_p = mainll_p->u.chhead_p->ch_p, pch_p = 0; ch_p != 0;
					pch_p = ch_p, ch_p = ch_p->ch_p) {
			/*
			 * Loop through the linked list of GRPSYLs hanging off
			 * this chord.  Skip the syllables; just deal with the
			 * groups.  Upon finding the first group on a staff
			 * (which could be for any of the voices, since not all
			 * might be present in this chord), call procrests and
			 * finalgroupproc to process the groups.
			 */
			gs1_p = ch_p->gs_p;
			for (;;) {
				/* find first group on a staff */
				while (gs1_p != 0 &&
						gs1_p->grpsyl == GS_SYLLABLE)
					gs1_p = gs1_p->gs_p;
				if (gs1_p == 0)
					break;

				/*
				 * Call procrests() to place any rest in the
				 * voice on this staff in this chord.
				 */
				procrests(ch_p, stafflist[gs1_p->staffno],
						gs1_p, limhigh[gs1_p->staffno],
						limlow[gs1_p->staffno]);

				/* set gs1_p to after this staff's groups */
				gs1_p = finalgroupproc(gs1_p, pch_p);
			}
		}
	}

	free_rectab();

	initstructs();

	/*
	 * Loop once for each item in the main linked list.  Now that we're all
	 * done with notes and rests, do the spaces and syllables.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {

			asgnssv(mainll_p->u.ssv_p);

		} else if (mainll_p->str == S_STAFF &&
					mainll_p->u.staff_p->visible == YES) {

			/* for each voice that exists, process the spaces */
			for (v = 0; v < MAXVOICES; v++) {
				if (mainll_p->u.staff_p->groups_p[v] != 0) {
					procspaces(mainll_p->u.staff_p->
							groups_p[v]);
				}
			}

			/* set relative horizontal coords for syllables */
			for (v = 0; v < mainll_p->u.staff_p->nsyllists; v++) {
				procsyls(mainll_p->u.staff_p->syls_p[v]);
			}
		}
	}

	/* scale all relative coords set so far according to staffscale */
	apply_staffscale();

	/* put padding on groups to allow room for subbars */
	room4subbars();

	/* now we are ready to set relative horizontal coords for all chords */
	relxchord();
}

/*
 * Name:        highcoord()
 *
 * Abstract:    Find highest relative y coord of a group in one GRPSYL list.
 *
 * Returns:     void
 *
 * Description: This function goes down one of the linked lists of GRPSYLs,
 *		one that is for groups, not syllables, and finds the highest
 *		relative y coordinate of any group containing notes.  If there
 *		are no notes but there are rests, it returns 0.  If there are
 *		only spaces, it returns -100.  The answer, though, is rounded
 *		off to the nearest staff line.  Besides the primary linked list
 *		of GRPSYLs, it also looks for GRPSYLs in the alternate list
 *		(voice 3 if it exists) and considers ones that are "standing in"
 *		for the first list's voice.
 */

static double
highcoord(gs_p, altgs_p)

register struct GRPSYL *gs_p;	/* starts pointing at first GRPSYL in list */
register struct GRPSYL *altgs_p;/* first GRPSYL of voice 3, if any */

{
	float result;
	int normvoice;		/* main voice we are dealing with, 1 or 2 */
	float edge;		/* of a group in the other voice */


	debug(32, "highcoord file=%s line=%d", gs_p->inputfile,
			gs_p->inputlineno);
	result = -100;		/* init as if only spaces */
	normvoice = gs_p->vno;	/* remember the voice we're dealing with */

	/* 
	 * Loop through all groups (even grace), moving result up when
	 * something higher is found.  Rests count as 0 (the middle line).
	 */
	for ( ; gs_p != 0; gs_p = gs_p->next) {
		switch (gs_p->grpcont) {
		case GC_NOTES:
			edge = gs_p->c[RN];
			/* if wrong way stem, account for it as best we can */
			if (gs_p->stemdir == UP) {
				edge += (stemroom(gs_p) - 1.0) * STEPSIZE;
			}
			if (edge > result) {
				result = edge;
			}
			break;
		case GC_REST:
			if (result < 0)
				result = 0;
			break;
		/* ignore spaces */
		}
	}

	/*
	 * Look at every GRPSYL in voice 3, if any.  If it is "standing in" for
	 * the normal voice, move result up if need be.
	 */
	for ( ; altgs_p != 0; altgs_p = altgs_p->next) {
		if (altgs_p->pvno == normvoice) {
			switch (altgs_p->grpcont) {
			case GC_NOTES:
				if (altgs_p->c[RN] > result)
					result = altgs_p->c[RN];
				break;
			case GC_REST:
				if (result < 0)
					result = 0;
				break;
			/* ignore spaces */
			}
		}
	}

	return (nearestline(result));
}

/*
 * Name:        lowcoord()
 *
 * Abstract:    Find lowest relative y coord of a group in one GRPSYL list.
 *
 * Returns:     void
 *
 * Description: This function goes down one of the linked lists of GRPSYLs,
 *		one that is for groups, not syllables, and finds the lowest
 *		relative y coordinate of any group containing notes.  If there
 *		are no notes but there are rests, it returns 0.  If there are
 *		only spaces, it returns 100.  The answer, though, is rounded
 *		off to the nearest staff line.  Besides the primary linked list
 *		of GRPSYLs, it also looks for GRPSYLs in the alternate list
 *		(voice 3 if it exists) and considers ones that are "standing in"
 *		for the first list's voice.
 */

static double
lowcoord(gs_p, altgs_p)

register struct GRPSYL *gs_p;	/* starts pointing at first GRPSYL in list */
register struct GRPSYL *altgs_p;/* first GRPSYL of voice 3, if any */

{
	float result;
	int normvoice;		/* main voice we are dealing with, 1 or 2 */
	float edge;		/* of a group in the other voice */


	debug(32, "lowcoord file=%s line=%d", gs_p->inputfile,
			gs_p->inputlineno);
	result = 100;		/* init as if only spaces */
	normvoice = gs_p->vno;	/* remember the voice we're dealing with */

	/* 
	 * Loop through all groups (even grace), moving result down when
	 * something lower is found.  Rests count as 0 (the middle line).
	 */
	for ( ; gs_p != 0; gs_p = gs_p->next) {
		switch (gs_p->grpcont) {
		case GC_NOTES:
			edge = gs_p->c[RS];
			/* if wrong way stem, account for it as best we can */
			if (gs_p->stemdir == DOWN) {
				edge -= (stemroom(gs_p) - 1.0) * STEPSIZE;
			}
			if (edge < result) {
				result = edge;
			}
			break;
		case GC_REST:
			if (result > 0)
				result = 0;
			break;
		/* ignore spaces */
		}
	}

	/*
	 * Look at every GRPSYL in voice 3, if any.  If it is "standing in" for
	 * the normal voice, move result up if need be.
	 */
	for ( ; altgs_p != 0; altgs_p = altgs_p->next) {
		if (altgs_p->pvno == normvoice) {
			switch (altgs_p->grpcont) {
			case GC_NOTES:
				if (altgs_p->c[RS] < result)
					result = altgs_p->c[RS];
				break;
			case GC_REST:
				if (result > 0)
					result = 0;
				break;
			/* ignore spaces */
			}
		}
	}

	return (nearestline(result));
}

/*
 * Name:        procrests()
 *
 * Abstract:    Sets relative coordinates for rests in one CHORD/STAFF.
 *
 * Returns:     void
 *
 * Description: This function is given the top GRPSYL on a STAFF in a CHORD.
 *		It sets the relative coordinates of each rest GRPSYL on this
 *		STAFF/CHORD.
 */

static void
procrests(ch_p, staff_p, gs1_p, limhigh, limlow)

struct CHORD *ch_p;		/* the chord we are in */
struct STAFF *staff_p;		/* the staff we are processing */
struct GRPSYL *gs1_p;		/* point at top GRPSYL in chord */
double limhigh;			/* highest relative y coord below v1 */
double limlow;			/* lowest relative y coord above v2 */

{
	RATIONAL endtime;	/* time at the end of the rest */
	struct GRPSYL *g_p[MAXVOICES + 1]; /* index by vno, point at GRPSYL */
	struct GRPSYL *gs_p;	/* a GRPSYL we are now working on */
	struct GRPSYL *ogs_p;	/* other voice to be considered */
	float wid, asc, des;	/* width, ascent, and descent of a rest */
	int vscheme;		/* voice scheme */
	int restsabove;		/* are these rests above another voice? */
	int stafflines;		/* no. of lines in staff */
	int v;			/* voice number */
	int other_vno;		/* voice number of the other voice */
	float y;		/* relative y coord for this rest */


	debug(32, "procrests file=%s line=%d limhigh=%f limlow=%f",
		gs1_p->inputfile, gs1_p->inputlineno,
		(float)limhigh, (float)limlow);


	/* get voice scheme and number of lines in staff */
	vscheme = svpath(gs1_p->staffno, VSCHEME)->vscheme;
	stafflines = svpath(gs1_p->staffno, STAFFLINES)->stafflines;

	/* set pointers to all nonspace groups in this chord on this staff */
	for (v = 1; v <= MAXVOICES; v++) {
		g_p[v] = 0;
	}
	for (gs_p = gs1_p; gs_p != 0 && gs_p->staffno == gs1_p->staffno &&
			    gs_p->grpsyl == GS_GROUP; gs_p = gs_p->gs_p) {
		if (gs_p->grpcont != GC_SPACE) {
			g_p[gs_p->pvno] = gs_p;
		}
	}

	y = 0.0;	/* to avoid useless 'used before set' warning */
	ogs_p = 0;	/* to avoid useless 'used before set' warning */
	other_vno = 1;	/* to avoid useless 'used before set' warning */

	/*
	 * Loop through each possible voice, setting its coords if it is a rest.
	 */
	for (v = 1; v <= MAXVOICES; v++) {

		gs_p = g_p[v];

		if (gs_p == 0 || gs_p->grpcont != GC_REST) {
			continue;
		}

		/* find the time at the end of the rest */
		endtime = radd(ch_p->starttime, gs_p->fulltime);

		/* find width, ascent, and descent of the rest */
		restsize(gs_p, &wid, &asc, &des);

		/*
		 * Find out if another voice needs to be considered in the
		 * placement of this rest, and set ogs_p to that voice's first
		 * GRPSYL if so.
		 */
		ogs_p = 0;
		if (vscheme == V_2FREESTEM ||
				(vscheme == V_3FREESTEM && gs_p->pvno != 3)) {
			other_vno = 3 - gs_p->pvno;
			ogs_p =  staff_p->groups_p[other_vno - 1];
		}

		/* 
		 * Find the RY of the rest.
		 */
		if (vscheme == V_1 || ((vscheme == V_2FREESTEM ||
				vscheme == V_3FREESTEM) &&
				hasspace_or_not_vno(staff_p->groups_p[3-1],
					ch_p->starttime, endtime, other_vno) &&
				hasspace(ogs_p, ch_p->starttime, endtime))) {
			/*
			 * There is either only 1 voice, or we are 2f/3f and the
			 * other voice is all spaces during this time and
			 * anything in voice 3 that is standing in for that
			 * voice is also all spaces during ths time.  Usually
			 * RY should be 0.  But for one-line staffs, whole
			 * rest characters need to be lowered so that they hang
			 * under the line.
			 */
			if (stafflines == 1 && gs_p->basictime == 1) {
				y = -2 * STEPSIZE;
			} else {
				y = 0;
			}
		} else {
			/*
			 * We are 2o, or 2f/3f with notes/rests in the other
			 * voice that we must avoid hitting.  Set up the
			 * relative y coord, based on whether gs_p is acting
			 * as v1 or v2.  We also have to set up restsabove
			 * for use below.
			 */
			restsabove = NO;	/* default value for now */
			switch (gs_p->pvno) {
			case 1:
				y = limhigh < -4 * STEPSIZE ?
						0 : limhigh + 4 * STEPSIZE;
				restsabove = ! hasspace(staff_p->groups_p[1],
						Zero, Maxtime);
				/* also check for v3 groups acting as v2 */
				for (ogs_p = staff_p->groups_p[2];
						ogs_p != 0 && restsabove == NO;
						ogs_p = ogs_p->next) {
					if (ogs_p->pvno == 2 &&
					ogs_p->grpcont != GC_SPACE) {
						restsabove = YES;
						break;
					}
				}
				break;
			case 2:
				y = limlow > 4 * STEPSIZE ?
						0 : limlow - 4 * STEPSIZE;
				break;
			}

			/*
			 * Usually RY should be the y was set above.  But
			 * if this is the upper voice, half rests and longer
			 * should be lower to fall within the staff when
			 * feasible, since they don't take much space
			 * vertically and we don't want needless ledger lines.
			 * (But nothing should ever be lowered if already on
			 * the center line.)  Short rests need to be moved
			 * away from the other voice by varying amounts,
			 * depending on how tall they are.  Quad and oct
			 * whole rests below need to be raised a notch.
			 */
			if (restsabove == YES) {
				/* lower whole & longer only if above middle */
				if (gs_p->basictime <= 2 && y > 0)
					y -= 2 * STEPSIZE;
				if (gs_p->basictime >= 16)
					y += 2 * STEPSIZE;
				if (gs_p->basictime == 256)
					y += 2 * STEPSIZE;
			} else {
				if (gs_p->basictime >= 128)
					y -= 2 * STEPSIZE;
				if (gs_p->basictime <= BT_QUAD)
					y += 2 * STEPSIZE;
			}
		}

		/*
		 * If restdist was set by the user, use that instead of
		 * whatever we calculated above.
		 */
		if (gs_p->restdist != NORESTDIST) {
			y = gs_p->restdist * STEPSIZE;
		}

		/*
		 * But in any case, whole and double whole cue rests need to be
		 * moved up a little, so that they will be in an appropriate
		 * place relative to the staff lines.
		 */
		if (gs_p->grpsize == GS_SMALL) {
			switch (gs_p->basictime) {
			case 0:		/* double whole rest char*/
				y += STEPSIZE * 0.4;
				break;
			case 1:		/* whole rest char */
				y += STEPSIZE * 0.85;
				break;
			}
		}

		/* set the relative coords for the rest's GRPSYL */
		gs_p->c[RX] = 0;
		gs_p->c[RE] = wid / 2;
		gs_p->c[RW] = -wid / 2 - gs_p->padding -
				vvpath(gs_p->staffno, gs_p->vno, PAD)->pad;
		gs_p->c[RY] = 0.0;	/* all groups have RY == 0 */
		gs_p->c[RN] = y + asc;
		gs_p->c[RS] = y - des;

		/* if there are dot(s), add their widths to the east side */
		if (gs_p->dots > 0) {
			gs_p->c[RE] += gs_p->dots * (width(FONT_MUSIC,
					DFLT_SIZE, C_DOT) + 2 * STDPAD);
		}

		/*
		 * Set the relative coords for the rest itself.  These are the
		 * same as the GRPSYL, except we use the real RY instead of 0.
		 */
		gs_p->restc[RX] = gs_p->c[RX];
		gs_p->restc[RE] = gs_p->c[RE];
		gs_p->restc[RW] = gs_p->c[RW];
		gs_p->restc[RY] = y;
		gs_p->restc[RN] = gs_p->c[RN];
		gs_p->restc[RS] = gs_p->c[RS];
	}
}

/*
 * Name:        hasspace_or_not_vno()
 *
 * Abstract:    Checks a v3 GRPSYL list for space or not "vno" during this time.
 *
 * Returns:     YES or NO
 *
 * Description: This function is similar to hasspace(), except that it regards
 *		any GRPSYL not having the pvno of "vno" as equivalent to a
 *		space.  See hasspace() prolog.
 */

int
hasspace_or_not_vno(gs_p, vtime, vtime2, vno)

struct GRPSYL *gs_p;	/* starts pointing at the first GRPSYL list */
RATIONAL vtime, vtime2;	/* time when to start and stop checking for space */
int vno;		/* voice number that we care about */

{
	RATIONAL t;		/* accumulate time */
	int oldcont;		/* content of previous group */


	/* "no linked list exists" counts as all spaces */
	if (gs_p == 0) {
		return (YES);
	}

	oldcont = GC_SPACE;	/* prevent useless 'used before set' warning */

	/* accumulate time until crossing vtime boundary */
	for (t = Zero; LT(t, vtime); gs_p = gs_p->next) {
		if (gs_p->grpvalue == GV_ZERO) {
			continue;
		}
		t = radd(t, gs_p->fulltime);
		oldcont = gs_p->grpcont;

		/* if pvno is not the given vno, pretend this GRPSYL is space */
		if (gs_p->pvno != vno) {
			oldcont = GC_SPACE;
		}
	}

	if (GT(t, vtime) && oldcont != GC_SPACE) {
		return (NO);
	}

	for ( ; gs_p != 0 && LT(t, vtime2); gs_p = gs_p->next) {
		if (gs_p->grpvalue == GV_ZERO) {
			continue;
		}
		if (gs_p->grpcont != GC_SPACE && gs_p->pvno == vno) {
			return (NO);
		}
		t = radd(t, gs_p->fulltime);
	}

	return (YES);
}

/*
 * Name:        procspaces()
 *
 * Abstract:    Sets relative coordinates for spaces in one GRPSYL list.
 *
 * Returns:     void
 *
 * Description: This function goes down one of the linked lists of GRPSYLs,
 *		one that is for groups, not syllables, and sets the relative
 *		coordinates for each space found.  Usually these coords will
 *		be left as 0, the way they were calloc'ed, but not when there
 *		is padding or uncompressible spaces.
 */

static void
procspaces(gs_p)

register struct GRPSYL *gs_p;	/* starts pointing at first GRPSYL in list */

{
	static float half_us_width;	/* half width of uncompressible space*/
	char headchar;			/* char representing a note head */
	int headfont;			/* music font for head char */


	/*
	 * Loop, setting all relative coords of spaces, except that if they are
	 * to be zero there's no need to set them, since calloc zeroed them.
	 * The vertical ones are always zero, and so is RX.
	 */
	for ( ; gs_p != 0; gs_p = gs_p->next) {
		if (gs_p->grpcont != GC_SPACE)
			continue;

		if (gs_p->uncompressible == YES) {
			/*
			 * If this is the first time in here, set this to half
			 * a blank quarter note head plus standard pad.
			 */
			if (half_us_width == 0.0) {
				headchar = nheadchar(get_shape_num("blank"),
						4, UP, &headfont);
				half_us_width = width(headfont, DFLT_SIZE,
						headchar) / 2.0 + STDPAD;
			}

			/* center the imaginary note head */
			gs_p->c[RE] = half_us_width / 2;
			gs_p->c[RW] = -half_us_width / 2;

			/* apply global user requested padding; notice that
			 * normal spaces (s) don't get this */
			gs_p->c[RW] -= vvpath(gs_p->staffno,
						gs_p->vno, PAD)->pad;
		}

		/* add any user requested padding */
		gs_p->c[RW] -= gs_p->padding;
	}
}

/*
 * Name:        finalgroupproc()
 *
 * Abstract:    Do final processing of groups.
 *
 * Returns:     pointer to the first GRPSYL after these groups, 0 if none
 *
 * Description: This function is given the GRPSYL for the first (topmost) voice
 *		that is on this staff in this chord.  It find what other
 *		GRPSYLs exist.  For all the nonspace groups, it applies any
 *		horizontal offsets needed.
 */

struct GRPSYL *
finalgroupproc(gs1_p, pch_p)

struct GRPSYL *gs1_p;		/* first voice on this staff in this chord */
struct CHORD *pch_p;		/* prev chord to the one containing gs1_p */

{
	struct GRPSYL *g_p[MAXVOICES];	/* point at nonspace voices' groups */
	struct GRPSYL *gs_p;		/* point at groups in the chord */
	struct GRPSYL *last_p;		/* point at last nonspace group */
	int numgrps;			/* how many nonspace groups are here */
	int staffno;			/* staff these groups are on */
	int n;				/* loop variable */
	float offset;			/* of each when + and - are used */
	float edge;			/* west or east coord of group 1 or 2 */
	int pack;			/* optimization for voice 3 */


	staffno = gs1_p->staffno;	/* remember staffno of first group */
	numgrps = 0;			/* no groups found yet */
	last_p = 0;			/* no last nonspace group yet */

	/* find all groups in this chord on this staff; remember nonspaces */
	for (gs_p = gs1_p; gs_p != 0 && gs_p->staffno == staffno &&
			    gs_p->grpsyl == GS_GROUP; gs_p = gs_p->gs_p) {
		if (gs_p->grpcont != GC_SPACE) {
			g_p[numgrps++] = gs_p;
			last_p = gs_p;
		}
	}

	/*
	 * If all groups on this staff were spaces, just make sure clef is
	 * marked correctly and return, though it's unlikely we have a clef
	 * change before a space.
	 */
	if (numgrps == 0) {
		fixclef(gs1_p, pch_p);
		return (gs_p);
	}

	/* nothing to do for tab if group is a rest, since invisible */
	if (is_tab_staff(g_p[0]->staffno) && g_p[0]->grpcont == GC_REST) {
		return (gs_p);
	}

	/* for any voice with a user supplied offset value, apply it now */
	for (n = 0; n < numgrps; n++) {
		if (g_p[n]->ho_usage == HO_VALUE)
			shiftgs(g_p[n], g_p[n]->ho_value * STEPSIZE);
	}

	/* no other offsets are needed for tab; never any collisions */
	if (is_tab_staff(g_p[0]->staffno)) {
		return (gs_p);
	}

	/*
	 * If both voices 1 and 2 are nonspace, handle any ho "+" or "-".
	 */
	if (numgrps >= 2 && g_p[0]->vno == 1 && g_p[1]->vno == 2) {
		/*
		 * Verify and fix offsets.  We did this in setgrps.c for note
		 * groups so that compatible note groups could then be handled
		 * together.  But we need to check again in case rest groups
		 * are involved.
		 */
		vfyoffset(g_p);

		/*
		 * Check each of these 2 groups:  If it has "+" or "-" and the
		 * other one doesn't, shift it to be next to the other one on
		 * the appropriate side.
		 */
		for (n = 0; n < 2; n++) {
			if ((g_p[n]->ho_usage == HO_LEFT ||
			     g_p[n]->ho_usage == HO_RIGHT) &&
			  ! (g_p[1-n]->ho_usage == HO_LEFT ||
			     g_p[1-n]->ho_usage == HO_RIGHT)) {

				if (g_p[n]->ho_usage == HO_LEFT) {
					shiftgs(g_p[n],
					g_p[1-n]->c[RW] - g_p[n]->c[RE]);
				} else {
					shiftgs(g_p[n],
					g_p[1-n]->c[RE] - g_p[n]->c[RW]);
				}
			}
		}

		/*
		 * If one has "+" and one has "-", shift them each by half of
		 * the amount of space needed to avoid a collision.
		 */
		if (g_p[0]->ho_usage == HO_LEFT &&
		    g_p[1]->ho_usage == HO_RIGHT) {

			offset = (g_p[0]->c[RE] - g_p[1]->c[RW]) / 2.0;
			shiftgs(g_p[0], -offset);
			shiftgs(g_p[1], offset);
		}
		if (g_p[0]->ho_usage == HO_RIGHT &&
		    g_p[1]->ho_usage == HO_LEFT) {

			offset = (g_p[1]->c[RE] - g_p[0]->c[RW]) / 2.0;
			shiftgs(g_p[0], offset);
			shiftgs(g_p[1], -offset);
		}

	} else if (g_p[0]->vno != 3) {
		/*
		 * If only one of groups 1 and 2 is nonspace, check whether it
		 * has "+" or "-", and warn if so.
		 */
		if (g_p[0]->ho_usage == HO_LEFT || g_p[0]->ho_usage == HO_RIGHT)
		{
			l_warning(
				g_p[0]->inputfile, g_p[0]->inputlineno,
				"voice %d cannot have horizontal offset '%c' since voice %d is not present; ignoring it",
				g_p[0]->vno,
				g_p[0]->ho_usage == HO_LEFT ? '-' :'+',
				3 - g_p[0]->vno);

			g_p[0]->ho_usage = HO_NONE;
		}
	}

	/*
	 * If voice 3 and at least one other voice exist here, and the user
	 * didn't state an offset value for voice 3, offset it next to the
	 * other voices, on the left or right, as requested.  But exclude the
	 * case where voice 3 was being treated as 1 or 2, by checking pvno
	 * instead of vno.
	 */
	if (numgrps > 1 && last_p->pvno == 3 && last_p->ho_usage != HO_VALUE) {
		/*
		 * See if we can pack v3 tightly against v1 and v2.  (This will
		 * not be allowed if ho_usage != HO_NONE for any voice, or any
		 * other of many conditions doesn't hold true.)
		 */
		pack = v3pack(g_p, numgrps);
		if (pack != PK_NONE) {
			/*
			 * Yes, we can; shift v3 a little if necessary.  Make
			 * it so that v3's stem is one stepsize away from the
			 * group that its stem is pointing toward.
			 */
			switch (pack) {
			case PK_LEFT:
				/* since v3 is on left, v2 must exist, and is
				 * the voice preceding v3 in g_p */
				shiftgs(last_p, -STEPSIZE +
					widest_head(last_p) / 2.0 +
					g_p[numgrps-2]->c[RW]);
				break;
			case PK_RIGHT:
				/* since v3 is on right, v1 must exist, and is
				 * the first voice in g_p */
				shiftgs(last_p, STEPSIZE +
					g_p[0]->c[RE] -
					widest_head(last_p) / 2.0);
				break;
			/* for PK_CENTER, nothing to do */
			}

		} else if (last_p->ho_usage == HO_LEFT) {

			/* find leftmost edge of the other voice(s) */
			edge = g_p[0]->c[RW];
			for (n = 1; n < numgrps - 1; n++) {
				if (g_p[n]->c[RW] < edge)
					edge = g_p[n]->c[RW];
			}
			/* set right edge of voice 3 == left edge of others */
			shiftgs(last_p, edge - last_p->c[RE]);

		} else { /* HO_RIGHT, or HO_NONE which defaults to HO_RIGHT */

			/* find rightmost edge of the other voice(s) */
			edge = g_p[0]->c[RE];
			for (n = 1; n < numgrps - 1; n++) {
				if (g_p[n]->c[RE] > edge)
					edge = g_p[n]->c[RE];
			}
			/* set left edge of voice 3 == right edge of others */
			shiftgs(last_p, edge - last_p->c[RW]);
		}
	} else if (g_p[0]->vno == 3 && (g_p[0]->ho_usage == HO_LEFT ||
					g_p[0]->ho_usage == HO_RIGHT)) {
		/*
		 * If the first (and thus only) voice is 3, it should not have
		 * ho "+" or "-".
		 */
		l_warning(
			g_p[0]->inputfile, g_p[0]->inputlineno,
			"voice 3 cannot have horizontal offset '%c' since voices 1 and 2 are not present; ignoring it",
			g_p[0]->ho_usage == HO_LEFT ? '-' :'+');

		g_p[0]->ho_usage = HO_NONE;
	}

	/* stack the accs/strings for all these groups, if not already done */
	applyaccstrs(g_p, numgrps);

	/* in case of midmeasure clef change, make sure it's marked right */
	/* but don't do it for grace groups; their main group handles it */
	if (gs1_p->grpvalue == GV_NORMAL) {
		fixclef(gs1_p, pch_p);
	}

	/* return the first GRPSYL after the groups we processed */
	return (gs_p);
}

/*
 * Name:        v3pack()
 *
 * Abstract:    Decide whether v3 can be packed tighter than the default.
 *
 * Returns:     PK_NONE		no, it can't
 *		PK_LEFT		pack tightly on left
 *		PK_RIGHT	pack tightly on right
 *		PK_CENTER	pack in the center
 *
 * Description: This function decides whether the voice 3 group can be packed
 *		in more tightly against voices 1 and 2 than the usual default
 *		of just putting v3's group's rectangle to the right of the
 *		other voices.  If there seems to be any danger that v3 would
 *		collide with v1 or v2, it gives up and returns PK_NONE.  It
 *		could be made a lot more sophisticated and not give up so soon
 *		in many cases.  However many of these improvements can't be
 *		done very well at this stage of the game, where we don't know
 *		yet about stem lengths, beam positions, etc.
 */

static int
v3pack(g_p, numgrps)

struct GRPSYL *g_p[];		/* point at nonspace voices' groups */
int numgrps;			/* how many nonspace groups are here */

{
	struct GRPSYL *gs_p;		/* point at a group */
	struct GRPSYL *v3_p;		/* point at v3's group */
	struct NOTE *v3note_p;		/* v3 note that neighbors other voice*/
	struct NOTE *onote_p;		/* v1/v2 note that neighbors v3 */
	float north;			/* highest coord of note or acc */
	float south;			/* lowest coord of note or acc */
	float topdesc;			/* descent of acc of top group */
	float botasc;			/* ascent of acc of bottom group */
	int v3hasacc, otherhasacc;	/* do v3 and other voice have acc(s)?*/
	int pack;
	int n;				/* loop variable */
	int k;				/* loop variable */


	/* either v1 or v2 must be nonspace */
	if (numgrps == 1) {
		return (PK_NONE);
	}

	/* point at v3's group for convenience */
	v3_p = g_p[numgrps - 1];

	/* set up what the answer will be if we can apply the optimization */
	if (v3_p->basictime >= 2) {
		/* there is a stem, so offset such that stem will avoid v1/v2 */
		if (v3_p->stemdir == UP) {
			pack = PK_RIGHT;
		} else {	/* DOWN */
			pack = PK_LEFT;
		}
	} else {
		pack = PK_CENTER;	/* no stem, so we can center v3 */
	}

	/* v3 must not be standing in for v1 or v2 */
	if (v3_p->pvno != 3) {
		return (PK_NONE);
	}

	/* if v3 would be on left, it must not have a flag or be start of beam*/
	if (pack == PK_LEFT && ((v3_p->basictime >= 8 &&
	    v3_p->beamloc == NOITEM) || v3_p->beamloc == STARTITEM)) {
		return (PK_NONE);
	}

	/* if v3 would be on right, it must not have grace groups preceding */
	if (pack == PK_RIGHT && v3_p->prev != 0 &&
				v3_p->prev->grpvalue == GV_ZERO) {
		return (PK_NONE);
	}

	/* v3 cannot have slashes or alternation */
	if (v3_p->slash_alt != 0) {
		return (PK_NONE);
	}

	/*
	 * Loop through all voices, checking for rule violations.  We do it
	 * in reverse so that we know v3 is notes (the first check) before
	 * checking the other voices.
	 */
	for (n = numgrps - 1; n >= 0; n--) {
		gs_p = g_p[n];	/* set to current voice for convenience */

		/* voice must be notes, and not measure repeat */
		if (gs_p->grpcont != GC_NOTES || gs_p->is_meas) {
			return (PK_NONE);
		}

		/* not worth the effort to support quad and oct notes */
		if (gs_p->basictime <= BT_QUAD) {
			return (PK_NONE);
		}

		/* voice cannot have user requested horizontal offset */
		if (gs_p->ho_usage != HO_NONE) {
			return (PK_NONE);
		}

		/* voice cannot have a "with" list */
		if (gs_p->nwith != 0) {
			return (PK_NONE);
		}

		/* voice cannot have a roll */
		if (gs_p->roll != NOITEM) {
			return (PK_NONE);
		}

		/* do voice specific checks */
		switch (gs_p->vno) {

		case 1:
			/* stem must be up */
			if (gs_p->stemdir != UP) {
				return (PK_NONE);
			}

			/* find neighboring notes of v1 and v3 */
			v3note_p = &v3_p->notelist[0];
			onote_p = &gs_p->notelist[gs_p->nnotes - 1];

			/* neighboring notes in v1 & v3 must not be too close */
			if (onote_p->stepsup < v3note_p->stepsup + 2 ||
			(onote_p->stepsup == v3note_p->stepsup + 2 &&
			pack != PK_CENTER &&
			(v3_p->basictime < 1 || gs_p->basictime < 1))) {
				return (PK_NONE);
			}

			/* if 2 steps apart and on lines and v3 would not be on
			 * right, v1 can't have dots and v3 can't unless it
			 * is on the right */
			if (onote_p->stepsup == v3note_p->stepsup + 2 &&
			EVEN(v3note_p->stepsup) &&
			(gs_p->dots != 0 ||
			(pack != PK_RIGHT && v3_p->dots != 0))) {
				return (PK_NONE);
			}

			/*
			 * If accidentals are not applied separately to each
			 * group, they will (later) be put to the left of
			 * everything, and we can ignore them here.
			 */
			if (gs_p->sep_accs == NO) {
				break;
			}

			/*
			 * Find the lowest extension of any accidental in v1.
			 * If no accidentals, the initial value for "south"
			 * will remain.  It's not good enough to check accs
			 * only on the neighboring notes, because some of them
			 * stick out pretty far.  We have to go through these
			 * gyrations because group boundaries do not consider
			 * accidentals that stick out up or down.
			 */
			otherhasacc = NO;
			south = onote_p->c[RY] - STEPSIZE + 0.001;
			for (k = 0; k < gs_p->nnotes; k++) {
				accdimen(gs_p->staffno, &gs_p->notelist[k],
					(float *)0, &topdesc, (float *)0);
				if (gs_p->notelist[k].c[RY] - topdesc < south) {
					south = gs_p->notelist[k].c[RY]
							- topdesc;
				}
				if (has_accs(gs_p->notelist[k].acclist)) {
					otherhasacc = YES;
				}
			}
			/* similarly, find highest extension of v3 accs */
			v3hasacc = NO;
			north = v3note_p->c[RY] + STEPSIZE - 0.001;
			for (k = 0; k < v3_p->nnotes; k++) {
				accdimen(v3_p->staffno, &v3_p->notelist[k],
					&botasc, (float *)0, (float *)0);
				if (v3_p->notelist[k].c[RY] + botasc > north) {
					north = v3_p->notelist[k].c[RY]
							+ botasc;
				}
				if (has_accs(v3_p->notelist[k].acclist)) {
					v3hasacc = YES;
				}
			}
			/* if v1 and v3 overlap due to acc(s), fail */
			if (south < north) {
				switch (pack) {
				case PK_RIGHT:
					if (v3hasacc == YES) {
						return (PK_NONE);
					}
					break;
				case PK_CENTER:
					if (v3hasacc == YES &&
					    otherhasacc == YES) {
						return (PK_NONE);
					}
					break;
				case PK_LEFT:
					if (otherhasacc == YES) {
						return (PK_NONE);
					}
					break;
				}
			}

			/* if left or right offset, neighboring notes in v1 &
			 * v3 must not have parentheses when accs exist */
			if ((pack != PK_CENTER || v3hasacc == YES || otherhasacc == YES) &&
			   (v3note_p->note_has_paren || onote_p->note_has_paren)) {
				return (PK_NONE);
			}

			break;

		case 2:
			if (gs_p->stemdir != DOWN) {
				return (PK_NONE);
			}

			/* find neighboring notes of v2 and v3 */
			v3note_p = &v3_p->notelist[v3_p->nnotes - 1];
			onote_p = &gs_p->notelist[0];

			/* neighboring notes in v1 & v3 must not be too close */
			if (onote_p->stepsup > v3note_p->stepsup - 2 ||
			(onote_p->stepsup == v3note_p->stepsup - 2 &&
			pack != PK_CENTER &&
			(v3_p->basictime < 1 || gs_p->basictime < 1))) {
				return (PK_NONE);
			}

			/* if 2 steps apart and on lines and v3 would not be on
			 * right, neither can have dots */
			if (onote_p->stepsup == v3note_p->stepsup - 2 &&
					EVEN(v3note_p->stepsup) &&
					pack != PK_RIGHT &&
					(gs_p->dots != 0 || v3_p->dots != 0)) {
				return (PK_NONE);
			}

			/*
			 * If accidentals are not applied separately to each
			 * group, they will (later) be put to the left of
			 * everything, and we can ignore them here.
			 */
			if (gs_p->sep_accs == NO) {
				break;
			}

			/*
			 * Find the highest extension of any accidental in v2.
			 * If no accidentals, the initial value for "north"
			 * will remain.
			 */
			otherhasacc = NO;
			north = onote_p->c[RY] + STEPSIZE - 0.001;
			for (k = 0; k < gs_p->nnotes; k++) {
				accdimen(gs_p->staffno, &gs_p->notelist[k],
					&botasc, (float *)0, (float *)0);
				if (gs_p->notelist[k].c[RY] + botasc > north) {
					north = gs_p->notelist[k].c[RY]
							+ botasc;
				}
				if (has_accs(gs_p->notelist[k].acclist)) {
					otherhasacc = YES;
				}
			}
			/* similarly, find highest extension of v3 accs */
			v3hasacc = NO;
			south = v3note_p->c[RY] - STEPSIZE + 0.001;
			for (k = 0; k < v3_p->nnotes; k++) {
				accdimen(v3_p->staffno, &v3_p->notelist[k],
					(float *)0, &topdesc, (float *)0);
				if (v3_p->notelist[k].c[RY] - topdesc < south) {
					south = v3_p->notelist[k].c[RY]
							- topdesc;
				}
				if (has_accs(v3_p->notelist[k].acclist)) {
					v3hasacc = YES;
				}
			}
			/* if v2 and v3 overlap due to acc(s), fail */
			if (south < north) {
				switch (pack) {
				case PK_RIGHT:
					if (v3hasacc == YES) {
						return (PK_NONE);
					}
					break;
				case PK_CENTER:
					if (v3hasacc == YES &&
					    otherhasacc == YES) {
						return (PK_NONE);
					}
					if (v3hasacc == YES &&
					gs_p->nnotes >= 2 &&
					onote_p->stepsup ==
					gs_p->notelist[1].stepsup + 1) {
						return (PK_NONE);
					}
					break;
				case PK_LEFT:
					if (otherhasacc == YES) {
						return (PK_NONE);
					}
					break;
				}
			}

			/* if left or right offset, neighboring notes in v2 &
			 * v3 must not have parentheses when accs exist */
			if ((pack != PK_CENTER || v3hasacc == YES || otherhasacc == YES) &&
			   (v3note_p->note_has_paren || onote_p->note_has_paren)) {
				return (PK_NONE);
			}

			break;
		}
	}

	/* all checks passed, so return the answer */
	return (pack);
}

/*
 * Name:        fixclef()
 *
 * Abstract:    If midmeasure clef change at this chord, mark in right GRPSYL.
 *
 * Returns:     void
 *
 * Description: This function is given the GRPSYL for the first (topmost) voice
 *		that is on this staff in this chord.  If the clef changed at
 *		this time value, locllnotes() in setnotes.c will have set the
 *		"clef" field in each of the GRPSYLs in this chord on this
 *		staff (actually in their first preceding grace group, if
 *		any).  But it should only be set in the GRPSYL that has the
 *		westernmost west boundary.  So this function erases it from
 *		any other GRPSYLs.  It alters this group's boundary to contain
 *		the clef only if the preceding chord doesn't exist or has
 *		groups that would overlap the clef vertically.
 */

static void
fixclef(gs1_p, pch_p)

struct GRPSYL *gs1_p;		/* starts at first voice on this staff */
struct CHORD *pch_p;		/* prev chord to the one containing gs1_p */

{
	struct GRPSYL *g_p[MAXVOICES];	/* point at voices' groups */
	struct GRPSYL *gs_p;		/* point at groups in the chord */
	int numgrps;			/* how many groups are in the chord */
	int overlap_chord;		/* can clef overlap preceding chord? */
	struct GRPSYL *westgs_p;	/* remember westernmost */
	int staffno;			/* staff number */
	int n;				/* loop variable */


	staffno = gs1_p->staffno;	/* remember staffno of first group */

	/* point at all groups in this chord on this staff */
	numgrps = 0;			/* no groups found yet */
	for (gs_p = gs1_p; gs_p != 0 && gs_p->staffno == staffno &&
			    gs_p->grpsyl == GS_GROUP; gs_p = gs_p->gs_p) {
		g_p[numgrps++] = gs_p;
	}

	/*
	 * For each that is preceded by grace group(s), change the pointer to
	 * point at the first in that sequence of grace groups.  Any clef
	 * change would occur at that group.
	 */
	for (n = 0; n < numgrps; n++) {
		while (g_p[n]->prev != 0 && g_p[n]->prev->grpvalue == GV_ZERO) {
			g_p[n] = g_p[n]->prev;
		}
	}

	/* if clef not marked in first, it's not marked in any, so return */
	if (g_p[0]->clef == NOCLEF) {
		return;
	}

	/*
	 * If voice 1 is mrpt, they all are mrpt.  As an exception to what we
	 * normally do in this function, leave the clef marked in all the
	 * groups, and widen them all to include the clef.  Force all the
	 * coords to be equal so that the clefs will overlay each other
	 * perfectly.
	 */
	if (is_mrpt(g_p[0])) {
		g_p[0]->c[RW] -= clefwidth(g_p[0]->clef, g_p[0]->staffno, YES)
				+ CLEFPAD;
		for (n = 1; n < numgrps; n++) {
			g_p[n]->c[RW] = g_p[0]->c[RW];	/* widen */
			g_p[n]->c[RX] = g_p[0]->c[RX];	/* shouldn't be needed*/
			g_p[n]->c[RE] = g_p[0]->c[RE];	/* shouldn't be needed*/
		}
		return;
	}

	/*
	 * Attach the clef to the westernmost group in our chord.  If it's a
	 * tie, use the first one.
	 */
	westgs_p = 0;	/* prevent useless "used before set" warning */
	for (n = 0; n < numgrps; n++) {
		if (westgs_p == 0 || g_p[n]->c[RW] < westgs_p->c[RW]) {
			westgs_p = g_p[n];
		}
	}

	/* erase clef from all but the group found above */
	for (n = 0; n < numgrps; n++) {
		if (g_p[n] != westgs_p) {
			g_p[n]->clef = NOCLEF;
		}
	}

	/*
	 * Find out if the clef can be made to overlap into the previous chord.
	 */
	if (pch_p == 0) {
		/* no previous chord to overlap */
		overlap_chord = NO;
	} else {
		/*
		 * The clef can be allowed to horizontally overlap the
		 * preceding chord only if the clef does not vertically overlap
		 * any of that chord's groups.
		 */
		overlap_chord = YES;	/* start out assuming okay */

		/* check the prev chord's groups that are on this staff */
		for (gs_p = pch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
			if (gs_p->grpsyl != GS_GROUP) {
				continue;
			}
			if (gs_p->staffno < staffno) {
				continue;
			}
			if (gs_p->staffno > staffno) {
				break;	/* we're past our staff, so get out */
			}
			if (clef_vert_overlap(westgs_p->clef, gs_p) == YES) {
				/* clef a overlaps group, so don't allow it */
				overlap_chord = NO;
				break;
			}
		}
	}

	/*
	 * If the clef can overlap the preceding chord, set clef_vert to
	 * remember this fact.  Then get out, without changing this group's
	 * boundaries.  If necessary, relxchord() will change the previous
	 * chord's east to contain the clef properly.
	 */
	if (overlap_chord == YES) {
		westgs_p->clef_vert = YES;
		return;
	}

	/* move western boundary of GRPSYL to allow room to print the clef */
	westgs_p->c[RW] -= clefwidth(westgs_p->clef, westgs_p->staffno, YES)
			+ CLEFPAD;

	/*
	 * If this is a grace group, we also have to alter its main group's
	 * boundary, because the main group's boundary needs to enclose all
	 * its grace groups.
	 */
	for (gs_p = westgs_p; gs_p->grpvalue == GV_ZERO; gs_p = gs_p->next) {
		;
	}
	if (gs_p != westgs_p) {
		gs_p->c[RW] -= clefwidth(westgs_p->clef, westgs_p->staffno,
				YES) + CLEFPAD;
	}
}

/*
 * Name:        restsize()
 *
 * Abstract:    Find the size of a rest.
 *
 * Returns:     void
 *
 * Description: This function is given a GRPSYL which is a rest.  It returns
 *		the width, ascent, and descent through pointers.
 */

static void
restsize(gs_p, wid_p, asc_p, des_p)

register struct GRPSYL *gs_p;	/* the GRPSYL containing the rest */
float *wid_p, *asc_p, *des_p;	/* return width, ascent, and descent of rest */

{
	int rchar;		/* char for the rest */
	int rfont;		/* font for the rest */
	int size;		/* font size */


	/* multirest has no music character; just return the answer */
	if (gs_p->is_multirest) {
		*wid_p = MINMULTIWIDTH;
		*asc_p = 2 * STEPSIZE;
		*des_p = 2 * STEPSIZE;
		return;
	}

	/* on a tab staff rests are invisible, so set to a very small size */
	if (is_tab_staff(gs_p->staffno)) {
		*wid_p = *asc_p = *des_p = 0.01;
		return;
	}

	/*
	 * The "normal" rest case.  Find the name of the character.  Then get
	 * the width, ascent, and descent of the rest.
	 */
	rchar = restchar(gs_p, &rfont);
	size = (gs_p->grpsize == GS_NORMAL ? DFLT_SIZE : SMALLSIZE);
	*wid_p = width(rfont, size, rchar);
	*asc_p = ascent(rfont, size, rchar);
	*des_p = descent(rfont, size, rchar);
}

/*
 * Name:        procsyls()
 *
 * Abstract:    Sets relative horizontal coords for syllables in 1 GRPSYL list.
 *
 * Returns:     void
 *
 * Description: This function goes down one of the linked lists of GRPSYLs,
 *		one that is for syllables, not groups, and sets the relative
 *		horizontal coordinates for each syllable found.
 */

static void
procsyls(gs_p)

register struct GRPSYL *gs_p;	/* starts pointing at first GRPSYL in list */

{
	float wid_b4_syl;	/* width of leading non-lyrics */
	float wid_real_syl;	/* width of actual lyric */
	float wid_after_syl;	/* width of trailing non-lyrics */
	float lyricsalign;	/* fraction of syl to go left of chord center*/
	int font, size;		/* of the last char in a syllable */
	char lc;		/* last char of syllable */


	debug(32, "procsyls file=%s line=%d", gs_p->inputfile,
			gs_p->inputlineno);
	/* find what fraction of each syl should go left of center of chord */
	lyricsalign = svpath(gs_p->staffno, LYRICSALIGN)->lyricsalign;

	/*
	 * Set coords for every syllable.  A syllable can consist of 3 parts.
	 * The middle part is the actual lyric.  The optional first and last
	 * parts are surrounded in the user's input by angle brackets.  The
	 * syllable is to be positioned such that "lyricsalign" of the middle
	 * part goes to the left of the chord's center, and the rest goes to
	 * the right; unless sylposition is set, in which case the left edge of
	 * the actual lyric is offset by that many points from the chord's
	 * center.  Then adjust the east side for padding purposes.
	 */
	for ( ; gs_p != 0; gs_p = gs_p->next) {
		sylwidth(gs_p->syl, &wid_b4_syl, &wid_real_syl, &wid_after_syl);

		gs_p->c[RX] = 0;
		if (gs_p->sylposition == NOSYLPOSITION) {
			gs_p->c[RW] = -lyricsalign * wid_real_syl - wid_b4_syl;
			gs_p->c[RE] = (1 - lyricsalign) * wid_real_syl +
					wid_after_syl;
		} else {
			gs_p->c[RW] = gs_p->sylposition * POINT - wid_b4_syl;
			gs_p->c[RE] = gs_p->sylposition * POINT + wid_real_syl +
					wid_after_syl;
		}

		/* get last char of syl; if null syl don't alter RE any more */
		lc = last_char(gs_p->syl);
		if (lc == '\0')
			continue;

		/*
		 * If this is not the last syllable of the measure, and it
		 * doesn't end in '-', leave space for a blank after it, to
		 * separate it from the next syllable.
		 */
		if ( gs_p->next != 0 && lc != '-') {
			end_fontsize(gs_p->syl, &font, &size);
			gs_p->c[RE] += width(font, size, ' ');
		}

		/*
		 * If this is the last syllable of the measure, and it ends in
		 * '-', back up a space, letting the '-' go into the bar line.
		 */
		if ( gs_p->next == 0 && lc == '-' ) {
			end_fontsize(gs_p->syl, &font, &size);
			gs_p->c[RE] -= width(font, size, ' ');
		}
	}
}

/*
 * Name:        apply_staffscale()
 *
 * Abstract:    Scale all relative coordinates according to staffscale.
 *
 * Returns:     void
 *
 * Description:	Throughout Mup, we are able to almost entirely avoid dealing
 *		with the "scale" parameter, by the following trick:  We pretend
 *		the paper is a different size than it really is, by the inverse
 *		of the "scale" factor, place and print everything at standard
 *		size, and then at the end apply the scale to everything at
 *		once, in PostScript.  (Margins are exempt from scaling, hence
 *		the EFF_* macros to cancel it out.)
 *
 *		But for the "staffscale" parameter, this kind of trick only
 *		works up to a point.  As long as we are dealing only with
 *		relative coords on one staff at a time, as we have up to this
 *		point in placement, we can ignore staffscale.  But now we're
 *		about to start dealing with chord coords, and chords span
 *		staffs.  So the jig is up.
 *
 *		This function goes through all the relative coords set so far,
 *		and scales them according to staffscale.  It also scales the
 *		font sizes in strings.  From this point on staffscale must
 *		always be considered.
 */

static void
apply_staffscale()

{
	struct MAINLL *mainll_p;	/* point at items in main linked list*/
	struct STAFF *staff_p;		/* point at a staff structure */
	register float staffscale;	/* current staffscale */
	register struct GRPSYL *gs_p;	/* point at groups */
	register struct NOTE *note_p;	/* point at notes */
	struct STUFF *stuff_p;		/* point at a stuff structure */
	int n;				/* loop variable */
	int v;				/* voice number, 0 to 2 */


	debug(16, "apply_staffscale");

	/*
	 * From this point on in Mup, we observe staffscale.  See the comment
	 * about Ignore_staffscale in globals.c.
	 */
	Ignore_staffscale = NO;

	initstructs();

	/*
	 * Loop down the main linked list looking for each staff.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			/* apply SSVs to keep staffscale up to date */
                        asgnssv(mainll_p->u.ssv_p);
			continue;

		case S_STAFF:
			staff_p = mainll_p->u.staff_p;
			break;		/* break out to handle staffs */

		default:
			continue;	/* nothing to do */
		}

		/* get staffscale for this staff in this measure */
		staffscale = svpath(staff_p->staffno, STAFFSCALE)->staffscale;

		/* go through each voice this staff has */
		for (v = 0; v < MAXVOICES; v++) {

			/* and each group in each voice */
			for (gs_p = staff_p->groups_p[v]; gs_p != 0;
					gs_p = gs_p->next) {

				/* scale the group's relative coords */
				gs_p->c[RX] *= staffscale;
				gs_p->c[RN] *= staffscale;
				gs_p->c[RY] *= staffscale;
				gs_p->c[RS] *= staffscale;
				gs_p->orig_rw *= staffscale;

				/* but don't disturb this E,W constant value */
				/* (see setgrps.c and abshorz.c) */
				if (gs_p->c[RE] != TEMPMRPTWIDTH / 2.0) {
					gs_p->c[RE] *= staffscale;
					gs_p->c[RW] *= staffscale;
				}

				gs_p->xdotr *= staffscale;

				/* usually we're done caring about padding */
				/*  by now, but not always, so scale it */
				gs_p->padding *= staffscale;

				switch (gs_p->grpcont) {
				case GC_NOTES:
					for (n = 0; n < gs_p->nnotes; n++) {
						note_p = &gs_p->notelist[n];

						/* scale note's rel. coords */
						note_p->c[RW] *= staffscale;
						note_p->c[RX] *= staffscale;
						note_p->c[RE] *= staffscale;
						note_p->c[RN] *= staffscale;
						note_p->c[RY] *= staffscale;
						note_p->c[RS] *= staffscale;

						note_p->waccr *= staffscale;
						note_p->ydotr *= staffscale;
						note_p->wlparen *= staffscale;
						note_p->erparen *= staffscale;

						/* this isn't really scaling,
						 * but it's a convenient place
						 * to undo CSS_STEPS */
						if (gs_p->stemto == CS_ABOVE &&
						    n <= gs_p->stemto_idx) {
							gs_p->notelist[n].stepsup -= CSS_STEPS;
							gs_p->notelist[n].ydotr -=
							CSS_STEPS * STEPSIZE * staffscale;
						} else if (gs_p->stemto == CS_BELOW &&
						    n >= gs_p->stemto_idx) {
							gs_p->notelist[n].stepsup += CSS_STEPS;
							gs_p->notelist[n].ydotr +=
							CSS_STEPS * STEPSIZE * staffscale;
						}
					}
					break;

				case GC_REST:
					gs_p->restc[RW] *= staffscale;
					gs_p->restc[RX] *= staffscale;
					gs_p->restc[RE] *= staffscale;
					gs_p->restc[RN] *= staffscale;
					gs_p->restc[RY] *= staffscale;
					gs_p->restc[RS] *= staffscale;
					break;
				}

				for (n = 0; n < gs_p->nwith; n++) {
					(void)resize_string(
						gs_p->withlist[n].string,
						staffscale,
						gs_p->inputfile,
						gs_p->inputlineno);
				}
			}
		}

		/* scale the syllables' coords and font sizes */
		for (v = 0; v < staff_p->nsyllists; v++) {
			for (gs_p = staff_p->syls_p[v]; gs_p != 0;
							gs_p = gs_p->next) {
				gs_p->c[RW] *= staffscale;
				gs_p->c[RX] *= staffscale;
				gs_p->c[RE] *= staffscale;
				gs_p->c[RN] *= staffscale;
				gs_p->c[RY] *= staffscale;
				gs_p->c[RS] *= staffscale;

				(void)resize_string(gs_p->syl, staffscale,
					gs_p->inputfile, gs_p->inputlineno);
			}
		}

		/* scale the STUFF structures' font sizes */
		/* (their coords won't be set until we get to stuff.c) */
		for (stuff_p = staff_p->stuff_p; stuff_p != 0;
				stuff_p = stuff_p->next) {
			if (stuff_p->string != 0) {
				(void)resize_string(
					stuff_p->string,
					stuff_p->all == YES ? Score.staffscale
							: staffscale,
					stuff_p->inputfile,
					stuff_p->inputlineno);
			}
		}
	}
}

/*
 * Name:        room4subbars()
 *
 * Abstract:    Extend RW of groups to make room for subbars.
 *
 * Returns:     void
 *
 * Description:	Whenever a subbar is at a count where there are notes/rests, the
 *		subbar needs to be put to the left of the note, and room needs
 *		to be provides before the notes/rests to make room for the
 *		subbar.  This function changes the RW to make room.
 */

void
room4subbars()
{
	struct MAINLL *mainll_p;	/* point at item in main linked list */
	struct CHHEAD *chhead_p;	/* the last chord headcell we saw */
	struct CHORD *ch_p;		/* a chord linked from chhead */
	struct GRPSYL *gs_p;		/* a group in the chord */
	struct GRPSYL *gs2_p;		/* group or first grace group */
	float chord_count;		/* the count where a chord is */
	short subbar_ok[MAXSTAFFS + 1];	/* YES or NO, index by staff */
	float min_grp_rw;		/* leftmost RW of groups by subbar */
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
			if (Score.nsubbar == 0) {
				continue;	/* no subbars in this measure */
			}
			if (chhead_p == 0) {
				pfatal("no chhead for measure in room4subbars()");
			}
			break;
		default:
			continue;
		}

		/*
		 * We are at a bar line.  Loop through each subbar, if any,
		 * that is in the preceding measure, to extend group boundaries
		 *  where it is needed.
		 */
		for (idx = 0; idx < Score.nsubbar; idx++) {

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
				/* no chord at this time value, forget subbar */
				continue;
			}

			min_grp_rw = -1.0;	/* init to an invalid number */

			/*
			 * We found a chord starting at the same time as this
			 * subbar.  Find out which staffs could allow a subbar
			 * to be printed at that time.
			 */
			allow_subbar(ch_p, Score.subbarlist[idx].appearance_p,
					subbar_ok);

			/*
			 * Extend RW for each GRPSYL that is on a staff that
			 * allows subbars.  Actually, for each, search left to
			 * the first grace group, if any, and do it there.
			 */
			for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {

				if (subbar_ok[gs_p->staffno] == NO) {
					continue;
				}

				for (gs2_p = gs_p; gs2_p->prev != 0 &&
				     gs2_p->prev->grpvalue == GV_ZERO;
				     gs2_p = gs2_p->prev) {
					;
				}

				/* make room for the bar plus a stepsize of
				 * space on each side of it */
				gs2_p->c[RW] -= width_subbar(Score.
					subbarlist[idx].appearance_p);

				/* maintain minimum valid coord */
				if (min_grp_rw < 0.0 ||
				    gs2_p->c[RW] < min_grp_rw) {
					min_grp_rw = gs2_p->c[RW];
				}
			}

			/*
			 * Loop the same as the first time, setting the RW of
			 * each relevant group to the leftmost RW of any of
			 * them that were set above.
			 */
			for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {

				if (subbar_ok[gs_p->staffno] == NO) {
					continue;
				}

				for (gs2_p = gs_p; gs2_p->prev != 0 &&
				     gs2_p->prev->grpvalue == GV_ZERO;
				     gs2_p = gs2_p->prev) {
					;
				}

				gs2_p->c[RW] = min_grp_rw;
			}
		}
	}
}

/*
 * Name:        relxchord()
 *
 * Abstract:    Set relative horizontal coordinates of each chord.
 *
 * Returns:     void
 *
 * Description: This function goes through the chord lists, and for each chord,
 *		sets its horizontal relative coordinates, by going down the
 *		list of GRPSYLs hanging off it.
 */

static void
relxchord()

{
	struct CHORD *ch_p;		/* point at a chord */
	struct CHORD *pch_p;		/* point at previous chord */
	struct MAINLL *mainll_p;	/* point at items in main linked list*/
	struct GRPSYL *gs_p;		/* point at groups */
	struct GRPSYL *pgs_p;		/* point at previous group */
	float clefwid;			/* width of a vertically piled clef */
	float maxclefwid;		/* max of any clefwid in a chord */
	float limit;			/* how far a syllable can go */
	float eff;			/* effective coord */


	debug(16, "relxchord");
	initstructs();

	/*
	 * Loop down the main linked list looking for each chord list headcell.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		/* need to keep "pad" up to date */
		if (mainll_p->str == S_SSV)
			asgnssv(mainll_p->u.ssv_p);

		if (mainll_p->str != S_CHHEAD)
			continue;	/* skip everything but chord HC */

		/*
		 * Pretend that all the chords will be jammed tightly together,
		 * starting at absolute coordinate zero.  We set absolute
		 * coords here for the benefit of effwest(), but they will be
		 * overwritten with their true values later in abshorz().
		 */
		mainll_p->u.chhead_p->ch_p->c[AW] = 0.0; /* west of 1st chord*/

		/*
		 * First, loop forwards through the chord list, setting the
		 * boundaries and widths of each chord based only on its
		 * groups.  The chord is to extend outwards just enough to
		 * contain every group.
		 */
		for (ch_p = mainll_p->u.chhead_p->ch_p; ch_p != 0;
					ch_p = ch_p->ch_p) {

			/* start chord as if 0 width */
			ch_p->c[RX] = 0;
			ch_p->c[RE] = 0;
			ch_p->c[RW] = 0;

			maxclefwid = 0.0;	/* no clefs found yet */

			/* loop through groups, expanding chord when necessary*/
			for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {
				if (gs_p->grpsyl == GS_GROUP) {
					/*
					 * If last chord in measure, add pad
					 * parameter on right side of groups;
					 * but not for collapsible spaces (s).
					 */
					if (ch_p->ch_p == 0 &&
					   (gs_p->grpcont != GC_SPACE ||
					   gs_p->uncompressible == YES)) {
						gs_p->c[RE] += vvpath(gs_p->
						staffno, gs_p->vno, PAD)->pad *
						svpath(gs_p->staffno,
						STAFFSCALE)->staffscale;
					}

					eff = effwest(mainll_p, ch_p, gs_p);
					if (eff < ch_p->c[RW])
						ch_p->c[RW] = eff;
					eff = effeast(ch_p, gs_p);
					if (eff > ch_p->c[RE])
						ch_p->c[RE] = eff;

					/*
					 * Check for vertically piled clef.  If
					 * there are graces before gs_p, flag
					 * and clef would be on the first grace.
					 */
					for (pgs_p = gs_p; pgs_p->prev != 0 &&
					     pgs_p->prev->grpvalue == GV_ZERO;
					     pgs_p = pgs_p->prev) {
						;
					}
					if (pgs_p->clef_vert == YES) {
						clefwid = clefwidth(pgs_p->clef,
							pgs_p->staffno, YES)
							* svpath(pgs_p->staffno,
							STAFFSCALE)->staffscale;

						if (clefwid > maxclefwid) {
							maxclefwid = clefwid;
						}
					}
				}
			}

			/*
			 * Revise prev chord's east and width if there is a
			 * vertically piled clef wider than current width.
			 * Let the clef overlap only the east part of the chord.
			 */
			if (maxclefwid > 0.0 && (pch_p = prevchord(mainll_p,
				     ch_p)) != 0 && maxclefwid > pch_p->c[RE]) {
				pch_p->c[RE] = maxclefwid;
				pch_p->c[AE] += pch_p->c[AX] + maxclefwid;
				pch_p->width = maxclefwid - pch_p->c[RW];
			}

			/* store width; will be updated later to include syls */
			ch_p->width = ch_p->c[RE] - ch_p->c[RW];

			/* set phony absolute coords for effwest() */
			ch_p->c[AX] = ch_p->c[AW] - ch_p->c[RW];
			ch_p->c[AE] = ch_p->c[AX] + ch_p->c[RE];
			if (ch_p->ch_p != 0)
				ch_p->ch_p->c[AW] = ch_p->c[AE];
		}

		/*
		 * Loop again through each chord in this list, this time
		 * expanding chords when necessary to include eastward
		 * extensions of syllables.  Work right to left, so that when
		 * a syllable steals space from the following chord, the
		 * following chord has already been widened eastwards, if it
		 * needed to be, based on its syllables.
		 */
		/* find last chord in the chord LL */
		ch_p = mainll_p->u.chhead_p->ch_p;	/* first chord */
		while (ch_p->ch_p != 0)
			ch_p = ch_p->ch_p;

		/* loop backwards through them (too bad there's no back ptr) */
		for ( ; ch_p != 0; ch_p = prevchord(mainll_p, ch_p)) {
			/*
			 * Loop through the linked list of GRPSYLs hanging off
			 * this chord, altering RE when finding a syl that
			 * sticks out farther.  There is one exception to
			 * this.  If a syllable extends farther east than any
			 * one so far, a test is made so that it can steal
			 * space from the following chord if that chord has
			 * no syllable there.
			 */
			for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {

				/* if not a syl or not sticking out east */
				if (gs_p->grpsyl != GS_SYLLABLE ||
						gs_p->c[RE] <= ch_p->c[RE])
					continue;

				/* syl seems to be sticking out east */

				/*
				 * If this is the last chord in the measure,
				 * the chord boundary must include the syl.
				 */
				if (ch_p->ch_p == 0) {
					ch_p->c[RE] = gs_p->c[RE];
					continue;
				}

				/*
				 * The syl is sticking out east of the current
				 * chord boundary, and this is not the last
				 * chord in the measure.  See how far east our
				 * syllable could go, which depends on how many
				 * chords, looking eastward, have no syllables.
				 */
				limit = get_east_limit(gs_p, ch_p);

				/* if not enough room, enlarge our chord */
				if (gs_p->c[RE] > limit) {
					ch_p->c[RE] += gs_p->c[RE] - limit;
				}
			}

			/* revise width; will be revised again later */
			ch_p->width = ch_p->c[RE] - ch_p->c[RW];

		} /* end of backwards loop through chords in this measure */

		/*
		 * Loop again through each chord in this list, this time
		 * expanding chords when necessary to include westward
		 * extensions of syllables.  Work left to right, so that when
		 * a syllable steals space from the preceding chord, the
		 * preceding chord has already been widened westwards, if it
		 * needed to be, based on its syllables.
		 */
		for (ch_p = mainll_p->u.chhead_p->ch_p; ch_p != 0;
					ch_p = ch_p->ch_p) {
			/*
			 * Loop through the linked list of GRPSYLs hanging off
			 * this chord, altering RW when finding a syl that
			 * sticks out farther.  There is one exception to
			 * this.  If a syllable extends farther west than any
			 * one so far, a test is made so that it can steal
			 * space from the following chord if that chord has
			 * no syllable there.
			 */
			for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {

				/* if not a syl or not sticking out west */
				if (gs_p->grpsyl != GS_SYLLABLE ||
						gs_p->c[RW] >= ch_p->c[RW])
					continue;

				/* syl seems to be sticking out west */

				/*
				 * If this is the first chord in the measure,
				 * the chord boundary must include the syl.
				 */
				if (prevchord(mainll_p, ch_p) == 0) {
					ch_p->c[RW] = gs_p->c[RW];
					continue;
				}

				/*
				 * The syl is sticking out west of the current
				 * chord boundary, and this is not the first
				 * chord in the measure.  See how far west our
				 * syllable could go, which depends on how many
				 * chords, looking westward, have no syllables.
				 */
				limit = get_west_limit(gs_p, ch_p, mainll_p);

				/* if not enough room, enlarge our chord */
				if (gs_p->c[RW] < limit) {
					ch_p->c[RW] += gs_p->c[RW] - limit;
				}
			}

			/* final revision of width */
			ch_p->width = ch_p->c[RE] - ch_p->c[RW];

		} /* end of forwards loop through chords in this measure */

	} /* end of loop through each CHHEAD in main linked list */

	pedalroom();		/* make room for "Ped." and "*" if need be */

	fixspace();		/* set a width for certain space chords */
}

/*
 * Name:        effwest()
 *
 * Abstract:    Find the effective west boundary of a group.
 *
 * Returns:     the RW to be used for the group
 *
 * Description: This function returns an "effective" RW for the given group.
 *		Sometimes this is just the true RW.  But if the previous chord
 *		has no groups on this staff that are in danger of colliding, we
 *		pretend it is a smaller number, so that our group can overlap
 *		horizonally with previous ones that have no possibly colliding
 *		groups.
 */

double
effwest(mainll_p, ch_p, gs_p)

struct MAINLL *mainll_p;	/* point at MLL item for this chord */
struct CHORD *ch_p;		/* point at this chord */
struct GRPSYL *gs_p;		/* point at this group */

{
	struct CHORD *pch_p;	/* point at previous chord */
	struct CHORD *ech_p;	/* point at earlier chord */
	struct GRPSYL *pgs_p;	/* point a group in previous chord */
	float small;		/* small number to be used */
	int found;		/* found a chord with a group on our staff? */
	float ourax;		/* tentative value for our chord's AX */
	float temp;		/* temp variable */


	pch_p = prevchord(mainll_p, ch_p);	/* find previous chord */

	/* if we are the first chord, return our group's true RW */
	if (pch_p == 0)
		return (gs_p->c[RW]);

	/* set default to -1.5 stepsize */
	small = -1.5 * STEPSIZE * svpath(gs_p->staffno, STAFFSCALE)->staffscale;

	/* if already closer to 0 than "small", return true RW */
	if (gs_p->c[RW] > small)
		return (gs_p->c[RW]);

	/*
	 * Loop through the previous chord's GRPSYLs to see if it has any
	 * groups on this staff.  If so, return our true RW, if there is a
	 * danger of collision.  If there isn't a group, or it's far enough
	 * away vertically that we know we won't collide with it, we will leave
	 * the loop and later return a phony RW.
	 */
	for (pgs_p = pch_p->gs_p; pgs_p != 0; pgs_p = pgs_p->gs_p) {
		/* skip cases where there can't be any interference */
		if (pgs_p->staffno > gs_p->staffno)
			break;		/* nothing more could be on our staff*/
		if (pgs_p->staffno < gs_p->staffno)
			continue;	/* ignore if wrong staff */
		if (pgs_p->grpsyl == GS_SYLLABLE)
			continue;	/* ignore if not a group */
		if (collision_danger(pgs_p, gs_p) == NO)
			continue;

		/* found a group that might collide, return our true RW */
		return (gs_p->c[RW]);
	}

	/*
	 * If our group is an upper group in a CSB set, csbstempad() may have
	 * forced extra with on the left of our group, to keep the stems from
	 * getting too close.  If so, don't allow overlap.
	 */
	if (gs_p->padded_csb_stem) {
		return (gs_p->c[RW]);
	}

	/*
	 * There is no group on our staff in the preceding chord, or at least
	 * none that we're in danger of colliding with.  We'd like to
	 * let our group overlap into that space if necessary.  But there
	 * might be a group in some earlier chord, and if there are enough dots
	 * on it, or enough accidentals on our group, they could still
	 * interfere.  Find the first earlier chord, looking right to left,
	 * that has a group neighboring our group that might collide.
	 */
	found = NO;
	for (ech_p = prevchord(mainll_p, pch_p); ech_p != 0;
				ech_p = prevchord(mainll_p, ech_p)) {

		for (pgs_p = ech_p->gs_p; pgs_p != 0; pgs_p = pgs_p->gs_p) {

			if (pgs_p->staffno > gs_p->staffno)
				break;	/* nothing more could be on our staff*/
			if (pgs_p->staffno < gs_p->staffno)
				continue;	/* ignore if wrong staff */
			if (pgs_p->grpsyl == GS_SYLLABLE)
				continue;	/* ignore if not a group */
			if (collision_danger(pgs_p, gs_p) == NO)
				continue;

			/* found a group that might collide */
			found = YES;
			break;
		}
		if (found == YES)
			break;
	}

	if (ech_p == 0)
		pfatal("no preceding group in effwest()");

	/*
	 * Since there could be multiple voices on this staff, there could be
	 * multiple groups on this staff in the chord we found.  Loop through
	 * each of them, keeping track of the max value our chord's AX would
	 * have to be to keep our group from overlapping that group.
	 */
	ourax = 0.0;
	for ( ; pgs_p != 0 && pgs_p->staffno == gs_p->staffno &&
			pgs_p->grpsyl == GS_GROUP; pgs_p = pgs_p->gs_p) {

		/* ignore ones that are vertically out of the way */
		if (collision_danger(pgs_p, gs_p) == NO)
			continue;

		temp = ech_p->c[AX] + pgs_p->c[RE] - gs_p->c[RW];
		if (temp > ourax)
			ourax = temp;
	}

	/* find what that value for our AX would make our RW be */
	temp = ch_p->c[AW] - ourax;

	/* return that amount, but not more than "small" */
	return (MIN(temp, small));
}

/*
 * Name:        effeast()
 *
 * Abstract:    Find the effective east boundary of a group.
 *
 * Returns:     the RE to be used for the group
 *
 * Description: This function returns an "effective" RE for the given group.
 *		Sometimes this is just the true RE.  But if the next chord
 *		has no groups on this staff that are in danger of colliding, we
 *		pretend it is a smaller number, so that our group can overlap
 *		horizonally with the next chord.  Don't worry about colliding
 *		with a group in a later chord; effwest() will handle that when
 *		processing that later group.
 */

double
effeast(ch_p, gs_p)

struct CHORD *ch_p;		/* point at this chord */
struct GRPSYL *gs_p;		/* point at this group */

{
	struct CHORD *nch_p;	/* point at next chord */
	struct GRPSYL *ngs_p;	/* point a group in next chord */
	float small;		/* small number to be used */


	nch_p = ch_p->ch_p;	/* find next chord */

	/* if we are the last chord, return our group's true RE */
	if (nch_p == 0)
		return (gs_p->c[RE]);

	small = 0.1 * STEPSIZE * svpath(gs_p->staffno, STAFFSCALE)->staffscale;

	/* if already closer to 0 than "small", return true RE */
	if (gs_p->c[RE] < small)
		return (gs_p->c[RE]);

	/*
	 * Loop through the next chord's GRPSYLs to see if it has any
	 * groups on this staff.  If so, return our true RE, unless they are
	 * not in danger of colliding.
	 */
	for (ngs_p = nch_p->gs_p; ngs_p != 0; ngs_p = ngs_p->gs_p) {
		/* skip cases where there can't be any interference */
		if (ngs_p->staffno > gs_p->staffno)
			break;		/* nothing more could be on our staff*/
		if (ngs_p->staffno < gs_p->staffno)
			continue;	/* ignore if wrong staff */
		if (ngs_p->grpsyl == GS_SYLLABLE)
			continue;	/* ignore if not a group */
		if (collision_danger(gs_p, ngs_p) == NO)
			continue;

		/* found a group that might collide, return true RE */
		return (gs_p->c[RE]);
	}

	return (small);
}

/*
 * Name:        collision_danger()
 *
 * Abstract:    Find whether the given groups are in danger of colliding.
 *
 * Returns:     YES or NO
 *
 * Description: This function is given two groups, a left and a right group,
 *		that are on the same staff.  If they are in the same voice, it
 *		just returns YES (we don't want one note of a voice to go under
 *		another in the same voice, and it would rarely work anyhow due
 *		to stem directions).  Otherwise it decides whether they are so
 *		close vertically that they are in danger of colliding unless
 *		kept apart horizontally.
 */

static int
collision_danger(g1_p, g2_p)

struct GRPSYL *g1_p;		/* ptr to left group */
struct GRPSYL *g2_p;		/* ptr to right group */

{
	float staffscale;
	float stepsize;			/* adjusted by staff scale */
	float north[2], south[2];	/* RN and RS of the groups */
	float dotoutside;		/* RY just beyond outside edge of dot*/
	float ascent, descent;		/* of an accidental */
	struct GRPSYL *g_p[2];		/* point at these two groups */
	int k, j;			/* loop variables */
	float accedge;			/* RN or RS of edge of accidental */


	/* same voice, always assume collideable */
	if (g1_p->vno == g2_p->vno) {
		return (YES);
	}

	/* a space can't collide with anything */
	if (g1_p->grpcont == GC_SPACE || g2_p->grpcont == GC_SPACE) {
		return (NO);
	}

	/* if measure repeat, there won't be anything else to collide with */
	if (is_mrpt(g1_p) || is_mrpt(g2_p)) {
		return (NO);
	}

	staffscale = svpath(g1_p->staffno, STAFFSCALE)->staffscale;
	stepsize = STEPSIZE * staffscale;
	g_p[0] = g1_p;
	g_p[1] = g2_p;

	/* find the RN and RS of the groups */
	for (k = 0; k < 2; k++) {
		if (g_p[k]->grpcont == GC_REST) {
			/* for rests, simply use the group boundaries */
			north[k] = g_p[k]->c[RN];
			south[k] = g_p[k]->c[RS];

		/*
		 * We can't use the group boundaries for notes.  For one thing,
		 * we don't know the stem length yet.  Assume the worst, that
		 * they are way long.  It won't usually negatively impact the
		 * result, nor will the fact that some notes don't have stems,
		 * because most collisions would be on the non-stem side of the
		 * groups.  On the non-stem side, we can't use the group
		 * boundary because it includes padding which would often make
		 * it seem like there'd be a collision, when really there won't
		 * be.
		 */
		} else if (g_p[k]->stemdir == UP) {

			north[k] = 10000.;	/* way long stem */

			/* one step below lowest note */
			south[k] = (g_p[k]->notelist[g_p[k]->nnotes-1].
					stepsup - 1) * stepsize;

			/* if dots, and lower than current RS, lower the RS */
			if (k == 0 && g_p[k]->dots > 0) {
				dotoutside = g_p[k]->notelist[g_p[k]->nnotes-1].
						ydotr - 0.6 * stepsize;
				if (dotoutside < south[k]) {
					south[k] = dotoutside;
				}
			}

			/* if any note has acc going below RS, lower the RS */
			if (k == 1) {
				for (j = 0; j < g_p[k]->nnotes; j++) {
					if (has_accs(g_p[k]->
							notelist[j].acclist)) {
						accdimen(g_p[k]->staffno,
							&g_p[k]->notelist[j],
							(float *)0, &descent,
							(float *)0);
						descent *= staffscale;

						/* bottom edge of acc */
						accedge = stepsize *
						g_p[k]->notelist[j].stepsup -
						descent;

						if (accedge < south[k]) {
							south[k] = accedge;
						}
					}
				}
			}

			/* if bottom note has parens, the group boundary shows
			 * close to how far down they go; extend to there */
			if (g_p[k]->notelist[g_p[k]->nnotes-1].note_has_paren
					&& g_p[k]->c[RS] < south[k]) {
				south[k] = g_p[k]->c[RS];
			}

		} else {	/* stemdir == DOWN */

			south[k] = -10000.;	/* way long stem */

			/* one step above highest note */
			north[k] = (g_p[k]->notelist[0].
					stepsup + 1) * stepsize;

			/* if dots, and higher than current RN, raise the RN */
			if (k == 0 && g_p[k]->dots > 0) {
				dotoutside = g_p[k]->notelist[0].
						ydotr + 0.6 * stepsize;
				if (dotoutside > north[k]) {
					north[k] = dotoutside;
				}
			}

			/* if any note has acc going above RN, raise the RN */
			if (k == 1) {
				for (j = 0; j < g_p[k]->nnotes; j++) {
					if (has_accs(g_p[k]->
							notelist[j].acclist)) {
						accdimen(g_p[k]->staffno,
							&g_p[k]->notelist[j],
							&ascent, (float *)0,
							(float *)0);
						ascent *= staffscale;

						/* top edge of acc */
						accedge = stepsize *
						g_p[k]->notelist[j].stepsup +
						ascent;

						if (accedge > north[k]) {
							north[k] = accedge;
						}
					}
				}
			}

			/* if top note has parens, the group boundary shows
			 * close to how far up they go; extend to there */
			if (g_p[k]->notelist[0].note_has_paren
					&& g_p[k]->c[RN] > north[k]) {
				north[k] = g_p[k]->c[RN];
			}
		}
	}

	/* if the groups don't overlap vertically, no collision danger */
	if (south[0] >= north[1] || north[0] <= south[1]) {
		return (NO);
	}

	return (YES);		/* collision danger */
}

/*
 * Name:        prevchord()
 *
 * Abstract:    Find chord preceding the given one.
 *
 * Returns:     pointer to previous chord, or 0 if none
 *
 * Description: This function is given a pointer to a chord headcell and a
 *		chord in that list.  It finds the preceding chord, returning
 *		it, or 0 if none.  If chord linked lists were doubly linked,
 *		we wouldn't have to go through this aggravation.
 */

static struct CHORD *
prevchord(mainll_p, ch_p)

struct MAINLL *mainll_p;	/* ptr to current syllable */
struct CHORD *ch_p;		/* ptr to current chord */

{
	register struct CHORD *prevch_p;


	prevch_p = mainll_p->u.chhead_p->ch_p;	/* get first chord in list */

	/* if current chord is first chord, there is none before it */
	if (prevch_p == ch_p)
		return (0);

	/* loop until we find it, then return */
	while (prevch_p->ch_p != ch_p)
		prevch_p = prevch_p->ch_p;
	return (prevch_p);
}


/*
 * Name:        get_east_limit()
 *
 * Abstract:    Find the limit of how far right a syllable can go.
 *
 * Returns:     the limit: measure from syllable's X to where it would collide
 *
 * Description: This function is given a pointer to a syllable, and the chord
 *		it is in.  It through the following chords until it finds a
 *		another syllable.  It returns how big our syllable's RE could
 *		be before it would collide with the other syllable.
 *		Note:  syllables given as a space count as if not there.
 */

static double
get_east_limit(gs_p, ch_p)

struct GRPSYL *gs_p;	/* starts at current syllable */
struct CHORD *ch_p;	/* starts at current chord */

{
	struct GRPSYL *nextgs_p;	/* point, looking for next syl */
	float east_limit;

	east_limit = ch_p->c[RE];

	/* loop forwards through the following chords */
	for (ch_p = ch_p->ch_p; ch_p != 0; ch_p = ch_p->ch_p) {
		/*
		 * Look down this chord until we hit either the end, or the
		 * syllable that follows the given one.
		 */
		for (nextgs_p = ch_p->gs_p;
				nextgs_p != 0 && nextgs_p != gs_p->next;
				nextgs_p = nextgs_p->gs_p) {
			;
		}

		/*
		 * If we found a nonempty syllable, return east_limit adjusted
		 * to get us to the west of that syllable.
		 */
		if (nextgs_p != 0 && nextgs_p->syl != 0) {
			return (east_limit - ch_p->c[RW] + nextgs_p->c[RW]);
		}

		/* no syl, or empty syl; we can use this chord's space */
		east_limit += ch_p->width;

		/* if empty syl, start looking for the one following it */
		if (nextgs_p != 0) {
			gs_p = nextgs_p;
		}
	}

	return (east_limit);
}

/*
 * Name:        get_west_limit()
 *
 * Abstract:    Find the limit of how far left a syllable can go.
 *
 * Returns:     the limit: measure from syllable's X to where it would collide
 *
 * Description: This function is given a pointer to a syllable, and the chord
 *		it is in.  It through the preceding chords until it finds a
 *		another syllable.  It returns how small (it's a negative number)
 *		our syllable's RW could be before it would collide with the
 *		other syllable.
 *		Note:  syllables given as a space count as if not there.
 */

static double
get_west_limit(gs_p, ch_p, mainll_p)

struct GRPSYL *gs_p;		/* starts at current syllable */
struct CHORD *ch_p;		/* starts at current chord */
struct MAINLL *mainll_p;	/* main LL for this chord */

{
	struct GRPSYL *prevgs_p;	/* point, looking for prev syl */
	float west_limit;

	west_limit = ch_p->c[RW];

	/* loop backwards through the preceding chords */
	for (ch_p = prevchord(mainll_p, ch_p); ch_p != 0;
			ch_p = prevchord(mainll_p, ch_p)) {
		/*
		 * Look down this chord until we hit either the end, or the
		 * syllable that precedes the given one.
		 */
		for (prevgs_p = ch_p->gs_p;
				prevgs_p != 0 && prevgs_p != gs_p->prev;
				prevgs_p = prevgs_p->gs_p) {
			;
		}

		/*
		 * If we found a nonempty syllable, return west_limit adjusted
		 * to get us to the east of that syllable.
		 */
		if (prevgs_p != 0 && prevgs_p->syl != 0) {
			return (west_limit - ch_p->c[RE] + prevgs_p->c[RE]);
		}

		/* no syl, or empty syl; we can use this chord's space */
		west_limit -= ch_p->width;

		/* if empty syl, start looking for the one following it */
		if (prevgs_p != 0) {
			gs_p = prevgs_p;
		}
	}

	return (west_limit);
}

/*
 * Name:        pedalroom()
 *
 * Abstract:    Increase some chords' width to make room for pedal characters.
 *
 * Returns:     void
 *
 * Description: This function tries to make room for "Ped." and "*", so that
 *		they don't overwrite each other.  For each "pedstar" style
 *		pedal mark, it finds the chord it's closest to.  If two of them
 *		are on neighboring chords, it may widen the left chord to
 *		provide enough room.  The problem is, the best it can do is
 *		assume that the pedal marks are exactly aligned with their
 *		closest chords.  It doesn't do anything about marks that are
 *		not on neighboring chords, since that would be quite a bit
 *		more work and would rarely be necessary.  Worst of all, if two
 *		marks' closest chords are the same chord, nothing can be done.
 */

static void
pedalroom()

{
	struct MAINLL *mainll_p;	/* point at items in main linked list*/
	struct CHHEAD *chhead_p;	/* point at a chord head cell */
	struct STAFF *staff_p;		/* point at a staff */
	struct STUFF *stuff_p;		/* point at a stuff */
	struct CHORD *pedch_p;		/* point at a chord near a pedal mark*/
	struct CHORD *opedch_p;		/* point at prev chord near pedal */
	int pedstyle;			/* P_* */
	int pedchar, opedchar;		/* current and previous pedal char */
	int font, size;			/* of a pedal char */
	char *string;			/* for pedal char */
	float needed;			/* amount of room needed */


	debug(16, "pedalroom");
	initstructs();

	chhead_p = 0;		/* prevent useless 'used before set' warning */

	/*
	 * Loop down the main linked list looking for each chord list headcell.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			/* need to keep pedstyle and timeden up to date */
			asgnssv(mainll_p->u.ssv_p);
			continue;	/* go to next MLL structure */

		case S_CHHEAD:
			/* remember this measure's chord list */
			chhead_p = mainll_p->u.chhead_p;
			continue;	/* go to next MLL structure */

		case S_STAFF:
			pedstyle = svpath(mainll_p->u.staff_p->staffno,
					PEDSTYLE)->pedstyle;
			if (pedstyle != P_LINE) {
				staff_p = mainll_p->u.staff_p;
				break;		/* break out and handle this */
			}

			continue;	/* not pedstar, ignore this staff */

		default:
			continue;	/* skip everything else */
		}

		/*
		 * At this point we are at a staff that has a pedstyle that
		 * uses "Ped." and "*".  Loop down the stuff list, looking for
		 * pedal marks.
		 */
		opedch_p = 0;		/* no pedal mark yet in measure */
		opedchar = '\0';/* prevent useless 'used before set' warning */
		for (stuff_p = staff_p->stuff_p; stuff_p != 0;
					stuff_p = stuff_p->next) {
			/*
			 * If it is not a pedal stuff, or it has no character,
			 * like a continuation from the previous score, skip.
			 */
			if (stuff_p->stuff_type != ST_PEDAL ||
			    stuff_p->string == 0)
				continue;
			/*
			 * Find the chord that is closest to this pedal mark,
			 * and which character this pedal mark is.
			 * But following the usual policy of applying "steps"
			 * offsets only after everything else is done, we
			 * ignore start.steps and use only start.count.
			 */
			pedch_p = closestchord(stuff_p->start.count,
					chhead_p->ch_p);
			font = stuff_p->string[0];
			size = stuff_p->string[1];
			string = stuff_p->string + 2;
			pedchar = next_str_char(&string, &font, &size) & 0xff;

			/* if first pedal mark in measure, nothing more to do*/
			if (opedch_p == 0) {
				/* remember as previous chord with pedal */
				opedch_p = pedch_p;
				opedchar = pedchar;
				continue;
			}

			/*
			 * If this pedal mark and the previous one are by
			 * neighboring chords, assume these marks are exactly
			 * aligned with their chords.  Make sure the east half
			 * of the previous chord plus the west half of this
			 * chord is enough room for them.  If it isn't, enlarge
			 * the east half of the previous chord.  (Note: RW is
			 * negative, so it must be negated.)
			 */
			if (pedch_p == opedch_p->ch_p) {
				needed = rightped(pedstyle, opedchar) +
					 leftped(pedstyle, pedchar);
				if (stuff_p->all == YES) {
					needed *= Score.staffscale;
				} else {
					needed *= svpath(staff_p->staffno,
						STAFFSCALE)->staffscale;
				}
				if (opedch_p->c[RE] - pedch_p->c[RW] < needed){
					opedch_p->c[RE] = needed +
							pedch_p->c[RW];
					opedch_p->width = opedch_p->c[RE] -
							opedch_p->c[RW];
				}
			}

			/* remember previous chord with pedal, and its char */
			opedch_p = pedch_p;
			opedchar = pedchar;
		}
	}
}

/*
 * Name:        closestchord()
 *
 * Abstract:    Find closest chord to given time value.
 *
 * Returns:     pointer to the closest chord
 *
 * Description: This function finds the CHORD in the given linked list that is
 *		closest, timewise, to the given count number.
 */

static struct CHORD *
closestchord(count, firstch_p)

double count;			/* which count of the measure */
struct CHORD *firstch_p;	/* first CHORD in this measure */

{
	RATIONAL reqtime;	/* time requested */
	struct CHORD *ch_p;	/* point along chord list */
	struct CHORD *och_p;	/* (old) point along chord list */


	/* if at or before the first count, it's closest to first group */
	if (count <= 1)
		return (firstch_p);

	/* get requested time to nearest tiny part of a count, in lowest terms*/
	reqtime.n = 4 * MAXBASICTIME * (count - 1) + 0.5;
	reqtime.d = 4 * MAXBASICTIME * Score.timeden;
	rred(&reqtime);

	/*
	 * Loop through the chord list.  As soon as a chord starts at or after
	 * the requested time value, check whether the requested time is closer
	 * to the new chord's time, or the previous chord's.  Return the
	 * closest one.
	 */
	for (och_p = firstch_p, ch_p = och_p->ch_p; ch_p != 0;
				och_p = ch_p, ch_p = ch_p->ch_p) {
		if (GE(ch_p->starttime, reqtime)) {
			if (GT( rsub(reqtime, och_p->starttime),
					rsub(ch_p->starttime, reqtime) ))
				return (ch_p);
			else
				return (och_p);
		}
	}

	/* requested time is after last chord; return last chord */
	return (och_p);
}

/*
 * Name:        rightped()
 *
 * Abstract:    Find the size of the right side of a pedstar pedal char.
 *
 * Returns:     the size
 *
 * Description: This function finds the size of the part of the given pedal
 *		character (pedstar style) that is to the right of where it
 *		should be centered.
 */

double
rightped(pedstyle, pedchar)

int pedstyle;			/* pedstar or alt pedstar */
int pedchar;			/* the given char */

{
	switch (pedchar) {
	case C_BEGPED:
		return (strwidth(Ped_start) / 2.0);
	case C_PEDAL:
		if (pedstyle == P_PEDSTAR)
			return (strwidth(Ped_start) - ped_offset());
		else /* P_ALTPEDSTAR */
			return (strwidth(Ped_start) / 2.0);
	case C_ENDPED:
		return (strwidth(Ped_stop) / 2.0);
	default:
		pfatal("bad pedal character passed to rightped()");
	}
	return (0);	/* to keep lint happy */
}

/*
 * Name:        leftped()
 *
 * Abstract:    Find the size of the left side of a pedstar pedal char.
 *
 * Returns:     the size
 *
 * Description: This function finds the size of the part of the given pedal
 *		character (pedstar style) that is to the left of where it
 *		should be centered.
 */

double
leftped(pedstyle, pedchar)

int pedstyle;			/* pedstar or alt pedstar */
int pedchar;			/* the given char */

{
	switch (pedchar) {
	case C_BEGPED:
		return (strwidth(Ped_start) / 2.0);
	case C_PEDAL:
		if (pedstyle == P_PEDSTAR)
			return (strwidth(Ped_stop) + ped_offset());
		else /* P_ALTPEDSTAR */
			return (strwidth(Ped_start) / 2.0);
	case C_ENDPED:
		return (strwidth(Ped_stop) / 2.0);
	default:
		pfatal("bad pedal character passed to leftped()");
	}
	return (0);	/* to keep lint happy */
}

/*
 * Name:        fixspace()
 *
 * Abstract:    Reset width, if need be, for chords of all spaces.
 *
 * Returns:     void
 *
 * Description: This function loops through chord lists, looking for each chord
 *		and setting its "uncollapsible" field.  This will be YES if
 *		any GRPSYL during that time duration fails to be a
 *		collapsible space.  So we have to look not only at GRPSYLs
 *		belonging to the chord (i.e. starting at this time) but also
 *		GPRSYLs that start earlier or later but overlap this time
 *		duration.
 */

static void
fixspace()

{
	struct CHORD *ch_p;		/* point at a chord */
	struct MAINLL *mainll_p;	/* point at items in main linked list*/
	struct MAINLL *m2_p;		/* another pointer down the MLL */
	int v;				/* voice number, 0 or 1 */


	debug(16, "fixspace");
	/*
	 * Loop down the main linked list looking for each chord list headcell.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		if (mainll_p->str != S_CHHEAD)
			continue;	/* skip everything but chord HC */

		/*
		 * Loop through the chord list, and set "uncollapsible" for
		 * every chord.
		 */
		for (ch_p = mainll_p->u.chhead_p->ch_p; ch_p != 0;
					ch_p = ch_p->ch_p) {

			ch_p->uncollapsible = NO;	/* default to NO */

			/*
			 * Loop through every staff until we find a reason to
			 * be uncollapsible.
			 */
			for (m2_p = mainll_p->next; m2_p->str == S_STAFF;
					m2_p = m2_p->next) {

				/* check the voices */
				for (v = 0; v < MAXVOICES && m2_p->u.staff_p->
							groups_p[v] != 0; v++) {

					/* if anything but collapsible space */
					if ( ! has_collapsible_space(
						   m2_p->u.staff_p->groups_p[v],
						   ch_p->starttime,
						   radd(ch_p->starttime,
							ch_p->duration))) {

						/* set this and get out */
						ch_p->uncollapsible = YES;
						goto endchord;
					}
				}

				/* check the verses */
				for (v=0; v < m2_p->u.staff_p->nsyllists; v++) {

					/* if anything but space */
					if ( ! hasspace(
						   m2_p->u.staff_p->syls_p[v],
						   ch_p->starttime,
						   radd(ch_p->starttime,
							ch_p->duration))) {

						/* set this and get out */
						ch_p->uncollapsible = YES;
						goto endchord;
					}
				}

			}
			endchord: ;
		}

	} /* looping through MLL, dealing with chord headcells */
}
