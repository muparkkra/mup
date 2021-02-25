
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

/* This file contains functions to deal with rolls.
 * This includes both parse phase and print phase code for rolls.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"



/* each "roll" input statement can have multiple beat offsets to specify
 * more than one roll. This is a linked list struct to hold these offsets */
struct ROLLOFFSET {
	float			offset;	/* beat offset where roll is */
	struct ROLLOFFSET	*next;	/* linked list */
};


/* struct to hold all needed info about a roll input statement */
struct ROLLINFO {
	short	topstaff;	/* roll goes from here */
	short	topvoice;
	short	botstaff;	/* roll ends here */
	short	botvoice;
	short	rolldir;	/* UP, DOWN, or UNKNOWN. UNKNOWN really means
				 * UP but with no arrow drawn (the default) */
	short	error;		/* YES if got bad value for some parameter */
	int	lineno;		/* input line number where defined */
	char 	*inputfile;	/* input file where defined */
	struct ROLLOFFSET *offsets_p;	/* list of beat offsets */
	struct ROLLINFO	*next;	/* linked list */
};

static struct ROLLINFO	*Roll_list_p;	/* rolls defined in current measure */
		/* rolls are linked to the head of this list, so this pointer
		 * will point to the roll currently being defined */


/* static functions declarations */
static void do_a_roll P((struct ROLLINFO *roll_p, struct MAINLL *mll_p));
static void roll P((struct MAINLL * mll_p, struct ROLLINFO *roll_p, 
		struct ROLLOFFSET * offset_p));
static void set_roll P((struct GRPSYL *gs_p, int item,
		struct ROLLINFO *roll_p));
static void draw_roll P((double x, double y1, double y2, int rolldir));



/* allocate struct for info about roll, and add to list */

void
newROLLINFO()

{
	struct ROLLINFO *roll_p;	/* newly allocated roll info */


	MALLOC(ROLLINFO, roll_p, 1);

	roll_p->offsets_p = (struct ROLLOFFSET *) 0;
	/* assume direction will be up, but with no direction arrow */
	roll_p->rolldir = UNKNOWN;
	/* incomplete info so far, so mark as in error */
	roll_p->error = YES;

	/* link onto head of list */
	roll_p->next = Roll_list_p;
	Roll_list_p = roll_p;

	roll_p->lineno = yylineno;
	roll_p->inputfile = Curr_filename;
}


/* alter the roll direction (default is UNKNOWN) */

void
setrolldir(int dir)

{
	Roll_list_p->rolldir = dir;
}


/* set roll parameters. Any parameter that is -1 should be left as is.
 * Others should be error checked, and if okay, filled in */
/* Must be called once to fill in the top staff and voice. At that point,
 * the bottom staff and voice are assummed to be the same as the top.
 * If they aren't, this function should be called again
 * with the bottom parameters set
 * to a non-negative number and the top parameters set to -1 */

void
rollparam(topstaff, topvoice, botstaff, botvoice)

int topstaff;		/* top of roll is here */
int topvoice;
int botstaff;		/* bottom of roll is here */
int botvoice;

{
	/* we now have enough info to check, so assume it's okay, then check */
	Roll_list_p->error = NO;

	/* for each value, if being set, error check and save away if okay.
	 * If no good, set error flag */
	if (topstaff >= 0) {
		if (rangecheck(topstaff, 1, Score.staffs, "staff number")
								== NO) {
			Roll_list_p->error = YES;
		}
		Roll_list_p->topstaff = Roll_list_p->botstaff
							= (short) topstaff;
	}

	if (topvoice >= 0) {
		if (rangecheck(topvoice, MINVOICES, NORMVOICES, "roll voice number")
							== NO) {
			Roll_list_p->error = YES;
		}
		Roll_list_p->topvoice = Roll_list_p->botvoice
							= (short) topvoice;
	}

	if (botstaff >= 0) {
		if (rangecheck(botstaff, 1, Score.staffs, "ending staff number")
							== NO) {
			Roll_list_p->error = YES;
		}
		Roll_list_p->botstaff = (short) botstaff;
	}

	if (botvoice >= 0) {
		if (rangecheck(botvoice, MINVOICES, NORMVOICES,
					"ending voice number") == NO) {
			Roll_list_p->error = YES;
		}
		Roll_list_p->botvoice = (short) botvoice;
	}

	if (Roll_list_p->topstaff > Roll_list_p->botstaff ||
			(Roll_list_p->topstaff == Roll_list_p->botstaff
			&& Roll_list_p->topvoice > Roll_list_p->botvoice)) {
		yyerror("end of roll must be below beginning of roll");
		Roll_list_p->error = YES;
	}
}


/* allocate space for offset information, fill it in, and link onto current
 * roll information struct */

void
rolloffset(offset)

double offset;		/* count offset where roll is to go */

{
	struct ROLLOFFSET *offset_p;	/* where to save offset info */


	/* error check */
	if (offset > Score.timenum + 1) {
		yyerror("roll offset beyond end of measure");
		Roll_list_p->error = YES;
		return;
	}

	/* allocate */
	MALLOC(ROLLOFFSET, offset_p, 1);

	/* fill in */
	offset_p->offset = offset;

	/* link to list */
	offset_p->next = Roll_list_p->offsets_p;
	Roll_list_p->offsets_p = offset_p;
}


/* at end of bar, do each roll. For each roll, find closest group of
 * top and bottom voice of roll. If they aren't at precisely the same
 * fulltime into the measure, error. Otherwise, mark these groups STARTITEM and
 * ENDITEM for roll, unless they are the same, in which case LONEITEM.
 * Also find any intervening groups that are also at the same fulltime.
 * Mark them INITEM. Finally, free the roll information. */

void
do_rolls(mll_p)

struct MAINLL *mll_p;	/* MAINLL of BAR */

{
	debug(4, "do_rolls lineno=%d", mll_p->inputlineno);

	do_a_roll(Roll_list_p, mll_p);
	Roll_list_p = (struct ROLLINFO *) 0;
}


/* recursively go down list of rolls, and mark the relevant GRPSYLs */

static void
do_a_roll(roll_p, mll_p)

struct ROLLINFO *roll_p;	/* current roll */
struct MAINLL *mll_p;		/* look from here to find appropriate staff */

{
	if (roll_p == (struct ROLLINFO *) 0) {
		/* end recursion */
		return;
	}

	/* recurse */
	do_a_roll(roll_p->next, mll_p);

	/* if error in early checking, ignore this one */
	if (roll_p->error == NO) {
	
		/* find the relevant STAFF */
		for (   ; mll_p != (struct MAINLL *) 0; mll_p = mll_p->prev) {
			if (mll_p->str == S_STAFF) {
				if (mll_p->u.staff_p->staffno ==
							roll_p->topstaff) {
					break;
				}
			}
		}

		if (mll_p == (struct MAINLL *) 0) {
			pfatal("couldn't find staff information for roll");
		}

		/* mark the GRPSYLs */
		roll(mll_p, roll_p, roll_p->offsets_p);
	}

	/* this one has been handled */
	FREE(roll_p);
}


/* for a specific roll on a specific chord, fill in the roll parameters for
 * all affected, visible groups */

static void
roll(mll_p, roll_p, offset_p)

struct MAINLL *mll_p;		/* STAFF of top of roll */
struct ROLLINFO *roll_p;	/* info about the roll */
struct ROLLOFFSET *offset_p;	/* count offset at which to place roll */

{
	RATIONAL timeoffset;	/* time into measure of group getting rolled */
	RATIONAL time1offset;	/* timeoffset of group in top voice of roll */
	struct GRPSYL *gs_p;
	struct GRPSYL *roll_grp_p;	/* group having roll */
	struct GRPSYL *lastvisgrp_p;	/* most recent grp with roll that is
					 * on a visible staff */
	int rollstate;			/* STARTITEM, etc */
	int staffno;
	int voice;
	double top_staffscale;	/* staffscale of staff at top of roll */


	/* recurse */
	if (offset_p == (struct ROLLOFFSET *) 0) {
		return;
	}
	roll(mll_p, roll_p, offset_p->next);


	/* find relevant group */
	gs_p = mll_p->u.staff_p->groups_p [ roll_p->topvoice - 1 ];
	roll_grp_p = closestgroup(offset_p->offset, gs_p, Score.timeden);

	if (roll_grp_p->roll != NOITEM) {
		l_yyerror(roll_p->inputfile, roll_p->lineno,
					"overlapping rolls not allowed");
		FREE(offset_p);
		return;
	}

	/* a lot of the time, the top and bottom staff/voice of roll will
	 * be the same, meaning we have a LONEITEM. This is the easy case
	 * so handle that. */
	if (roll_p->topstaff == roll_p->botstaff
				&& roll_p->topvoice == roll_p->botvoice) {
		if (svpath(roll_p->topstaff, VISIBLE)->visible == YES) {
			set_roll(roll_grp_p, LONEITEM, roll_p);
		}
		FREE(offset_p);
		return;
	}

	/* we must have a roll that encompasses more than one voice */
	lastvisgrp_p = (struct GRPSYL *) 0;

	/* find group's actual RATIONAL time offset into the measure */
	for (time1offset = Zero; gs_p != roll_grp_p; gs_p = gs_p->next) {
		time1offset = radd(time1offset, gs_p->fulltime);
	}

	/* if the top voice is visible, mark it as the top of the roll.
	 * If not, make a note that we don't have a real top yet */
	if (svpath(roll_p->topstaff, VISIBLE)->visible == YES) {
		set_roll(roll_grp_p, STARTITEM, roll_p);
		rollstate = INITEM;
		lastvisgrp_p = roll_grp_p;
	}
	else {
		rollstate = STARTITEM;
	}

	staffno = roll_p->topstaff;
	voice = roll_p->topvoice;
	top_staffscale = svpath(staffno, STAFFSCALE)->staffscale;

	/* find all groups down to ending of roll */
	for ( ; ; ) {

		/* move to next voice, which may be on next staff */
		if (++voice > MAXVOICES) {
			++staffno;
			voice = 1;
			mll_p = mll_p->next;
			if (mll_p->str != S_STAFF ||
					mll_p->u.staff_p->staffno != staffno) {
				pfatal("main list messed up while doing rolls");
			}
		}

		if (staffno > Score.staffs) {
			pfatal("could not find bottom roll staff");
		}

		/* if no more voices on staff, no reason to check more */
		if (voice > vscheme_voices(svpath(staffno, VSCHEME)->vscheme))  {
			continue;
		}

		/* find relevant group */
		gs_p = mll_p->u.staff_p->groups_p[ voice - 1];
		if (gs_p == (struct GRPSYL *) 0) {
			l_yyerror(roll_p->inputfile, roll_p->lineno,
					"no chord associated with roll");
			return;
		}
		roll_grp_p = closestgroup(offset_p->offset, gs_p, Score.timeden);


		if (svpath(roll_grp_p->staffno, STAFFSCALE)->staffscale !=
							top_staffscale) {
			l_yyerror(roll_p->inputfile, roll_p->lineno,
				"roll cannot span staffs with differing staffscale values");
			return;
		}

		/* find group's actual RATIONAL time offset into the measure */
		for (timeoffset = Zero; gs_p != roll_grp_p; gs_p = gs_p->next) {
			timeoffset = radd(timeoffset, gs_p->fulltime);
		}

		/* if this group's RATIONAL time isn't the same as
		 * the top group's, it doesn't get included in the roll */
		if (EQ(timeoffset, time1offset)) {
			/* need roll on this group */
			if (roll_grp_p->grpcont == GC_NOTES) {
				set_roll(roll_grp_p, rollstate, roll_p);
				rollstate = INITEM;
				if (svpath(staffno, VISIBLE)->visible == YES) {
					lastvisgrp_p = roll_grp_p;
				}
			}
		}

		/* check if at bottom of roll */
		if (staffno == roll_p->botstaff && voice == roll_p->botvoice) {
			if (NE(timeoffset, time1offset)) {
				l_yyerror(roll_p->inputfile, roll_p->lineno,
					"groups on top and bottom of roll are in different chords");
			}
			else if (svpath(staffno, VISIBLE)->visible == NO) {
				/* bottom staff of roll is invisible */
				if (lastvisgrp_p == (struct GRPSYL *) 0) {
					/* no visible staffs in roll */
					break;
				}

				/* change last visible staff included in the
				 * roll, to be the end of the roll, or
				 * set to LONEITEM if only visible */
				if (lastvisgrp_p->roll == STARTITEM) {
					lastvisgrp_p->roll = LONEITEM;
				}
				else {
					lastvisgrp_p->roll = ENDITEM;
				}
			}

			else if (roll_grp_p->grpcont != GC_NOTES) {
				l_yyerror(roll_p->inputfile, roll_p->lineno,
					"bottom of roll must not be rest or space");
			}
			else if (lastvisgrp_p->roll == STARTITEM) {
				/* all but the last were invisible */
				roll_grp_p->roll = LONEITEM;
			}
			else {
				roll_grp_p->roll = ENDITEM;
			}
			break;
		}
	}

	/* this one has been handled */
	FREE(offset_p);
}


/* do final checking and actually fill in roll parameters in grpsyl */

static void
set_roll(gs_p, item, roll_p)

struct GRPSYL *gs_p;		/* which GRPSYL to mark */
int item;			/* LONEITEM, STARTITEM, etc */
struct ROLLINFO *roll_p;	/* info about roll associated with group */

{
	if (gs_p->grpcont != GC_NOTES) {
		switch (item) {
		case STARTITEM:
			l_yyerror(roll_p->inputfile, roll_p->lineno,
				"top visible chord of roll must not be rest or space");
			return;
		case ENDITEM:
			l_yyerror(roll_p->inputfile, roll_p->lineno,
				"bottom visible chord of roll must not be rest or space");
			return;
		case LONEITEM:
			l_yyerror(roll_p->inputfile, roll_p->lineno,
				"rolled chord must not be rest or space");
			return;
		}
	}

	/* fill in values */
	gs_p->roll = (short) item;
	gs_p->rolldir = roll_p->rolldir;
}


/* print a roll */

void
print_roll(gs_p)

struct GRPSYL *gs_p;	/* GRPSYL that might have a roll on it */

{
	struct GRPSYL *botgs_p;		/* bottom group of roll */
	struct GRPSYL *prevgs_p;	/* chord above current chord */
	float north;
	float south;
	float westmost;			/* if voices overlap, the west of
					 * different groups in the chord may
					 * be different, so have to find
					 * whichever is farther west */
	float groupwest;		/* west of one group */


	switch (gs_p->roll) {

	case  LONEITEM:
		/* The orig_rw field in GRPSYL is the value of c[RW] before
		 * that gets adjusted for graces notes, if any. To get the
		 * AW--which is where we want to draw the roll--that needs
		 * to be added to the chord's AX:
		 *	original group AW = gs_p->orig_rw + chord AX
		 * Unfortunately, we don't have easy access to the CHORD.
		 * But we can deduce the the chord AX value by noting that
		 *	group AX = chord AX + group RX
		 * Therefore, by simple algebra that means
		 *	chord AX = group AX - group RX
		 * So we substitute that in the formala given earlier and get
		 *	original group AW = gs_p->orig_rw + (group AX - group RX)
		 */
		westmost = gs_p->orig_rw + (gs_p->c[AX] - gs_p->c[RX]);
		draw_roll(westmost + ROLLPADDING * Staffscale / 2.0,
				gs_p->notelist[0].c[AN],
				gs_p->notelist[ gs_p->nnotes - 1].c[AS],
				gs_p->rolldir);
		return;

	case STARTITEM:
		/* normally, the north of the roll is the north of the top
		 * group. However, there is one special case. If the roll
		 * starts on voice 1 and goes through voice 2 on that staff,
		 * and the top note on voice 2 is higher than the top note
		 * on voice 1, need to start roll at the top note of voice 2 */
		north = gs_p->notelist[0].c[AN];
		if (gs_p->vno == 1 && gs_p->gs_p != (struct GRPSYL *) 0 &&
					gs_p->gs_p->staffno == gs_p->staffno &&
					gs_p->gs_p->grpsyl == GS_GROUP &&
					gs_p->gs_p->grpcont == GC_NOTES) {
			if (gs_p->gs_p->notelist[0].c[AN] >
						gs_p->notelist[0].c[AN]) {
				north = gs_p->gs_p->notelist[0].c[AN];
			}
		}

		/* start definitely too far east, till we find actual west */
		westmost = gs_p->c[AE];

		/* find the bottom group of the roll */
		prevgs_p = gs_p;
		for (botgs_p = gs_p; botgs_p != 0; botgs_p = botgs_p->gs_p) {
			if (botgs_p->grpsyl != GS_SYLLABLE) {
				/* See comment earlier in this function to
				 * explain this formula using orig_rw */
				groupwest = botgs_p->orig_rw +
					(botgs_p->c[AX] - botgs_p->c[RX]);
				if (groupwest < westmost) {
					westmost = groupwest;
				}
			}

			if (botgs_p->roll == ENDITEM) {

				/* normally, the end of the roll is the bottom
				 * note of the ending group. However, there is
				 * one special case. If the roll ends on voice
				 * 2, and the bottom note of voice 1 is lower
				 * than the bottom note of voice 1, the south
				 * of the roll is the bottom of voice 1 */
				south = botgs_p->notelist
						[ botgs_p->nnotes - 1].c[AS];

				if (botgs_p->vno == 2 && prevgs_p->staffno
						== botgs_p->staffno &&
						prevgs_p->grpsyl == GS_GROUP) {

					if (prevgs_p->nnotes > 0 &&
							prevgs_p->notelist[prevgs_p->nnotes-1].c[AS]
							< botgs_p->notelist
							[botgs_p->nnotes - 1]
							.c[AS]) {

						south = prevgs_p->notelist
							[prevgs_p->nnotes - 1]
							.c[AS];
					}
				}

				draw_roll(westmost
					+ ROLLPADDING * Staffscale / 2.0,
					north, south, gs_p->rolldir);
				return;
			}
			prevgs_p = botgs_p;
		}
		pfatal("failed to find end of multi-voice roll");
		break;

	default:
		/* nothing to do */
		break;
	}
}


/* Actually draw a roll at the given x from y1 to y2. If rolldir is DOWN,
 * draw an arrow at the bottom; if it is UP draw an arrow at the top;
 * if UNKNOWN don't draw any arrow. */

static void
draw_roll(x, y1, y2, rolldir)

double x;		/* horizontal location of roll */
double y1;		/* vertical location */
double y2;
int rolldir;		/* UP, DOWN, or UNKNOWN (i.e., UP but no arrow) */

{
	/* draw the roll itself */
	draw_wavy(x, y1, x, y2);

	/* If arrow was requested, draw it */
	if (rolldir != UNKNOWN) {
		float len;

		/* draw arrow at bottom */
		len = ROLLPADDING * Staffscale / 2.0 - Stdpad;
		do_linetype(L_NORMAL);
		if (rolldir == DOWN) {
			draw_line(x, y2 - Stepsize, x - (0.8 * len), y2 + len);
			draw_line(x, y2 - Stepsize, x + (0.8 * len), y2 + len);
		}
		else {
			draw_line(x, y1 + Stepsize, x - (0.8 * len), y1 - len);
			draw_line(x, y1 + Stepsize, x + (0.8 * len), y1 - len);
		}
	}
}


/* return YES if given group is the top group that gets a roll drawn by it,
 * NO if not. This is called from the print phase */

int
gets_roll(gs_p, staff_p, v)

struct GRPSYL *gs_p;		/* check if this group gets a roll */
struct STAFF *staff_p;	/* it's connected to this staff */
int v;			/* and is in this voice */

{
	float width1, width2;		/* widest note heads in each group */
	float maxwide;			/* widest notehead */
	struct GRPSYL *othergs_p;	/* group in other voice */


	if (gs_p->roll != STARTITEM && gs_p->roll != LONEITEM) {
		return(NO);
	}

	/* check for strange case where we don't print a roll because groups
	 * are incompatible (had to be moved horizontally because they
	 * overlapped), and both have rolls. If the group's RX is greater
	 * than (maxwide - W_NORMAL * POINT) / 2 where
	 * maxwide is the maximum of width of the note head characters
	 * the two groups, then don't print the roll. */
	if (svpath(staff_p->staffno, VSCHEME)->vscheme == V_1) {
		/* strange case only happens with 2 voices */
		return(YES);
	}
	/* We never move things on tab, so no need to check tab.
	 * And we better not, because widest_head() doesn't make sense
	 * on tab staff, so we will get wrong results. */
	else if (is_tab_staff(staff_p->staffno) == NO) {
		/* find width of widest note of this group */
		width1 = widest_head(gs_p) * Staffscale;

		/* find other group. If this is first voice,
		 * just look down the chord link */
		if (v == 0) {
			othergs_p = gs_p->gs_p;
		}
		else {
			/* follow groups until we find the one linked to this
			 * one */
			for (othergs_p = staff_p->groups_p[0];
					othergs_p != (struct GRPSYL *) 0;
					othergs_p = othergs_p->next) {
				if (othergs_p->gs_p == gs_p) {
					break;
				}
			}
		}

		if (othergs_p != (struct GRPSYL *) 0 &&
				othergs_p->grpcont == GC_NOTES) {

			/* find width of widest note of the other group */
			width2 = widest_head(othergs_p) * Staffscale;

			maxwide = MAX(width1, width2);

			if (gs_p->c[RX] > ((maxwide - W_NORMAL * POINT) / 2.0)){
				/* we hit the strange case */
				return(NO);
			}
		}
	}
	return(YES);
}
