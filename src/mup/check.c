
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

/* This file contains functions for checking for various classes of errors,
 * such as values out of range, or trying to do something in an illegal context.
 * It also contains the code to combine multiple measures of rests into
 * multirests, most of the extraction (-x) code, and various other things
 * that mostly happen sort of between parse and placement phases. */

#include "defines.h"
#include "structs.h"
#include "globals.h"


static struct MAINLL *clone_repeated_section P((struct MAINLL *begin_mll_p,
		struct MAINLL *end_mll_p));
static struct TIMEDSSV * clone_tssv P((struct TIMEDSSV *src_tssv_p,
		struct MAINLL *src_mll_p, struct MAINLL *dest_mll_p));
static struct MAINLL *add_pre_meas P((struct MAINLL *insert_p, int start,
		int end, int add_invis));
static int check_all_rests P((struct STAFF *staff_p, int count));
static void do_combine P((struct MAINLL *begin_p, struct MAINLL *end_p,
		int nummeas, int min_combine));
static int valid_mark_item P((int mark, int place));
static void mv_accs P((struct MAINLL *mll_p));
static void move_xoct P((struct STUFF *stuff_p, struct MAINLL *newfirst_p,
		int staffno, int bars, int start));
static void addped P((struct STUFF *pedal_p, struct MAINLL *mll_p));
static void set_leftright P((int flag_nonsided, int flag_left, int flag_right,
		struct BLOCKHEAD *nonsided_p, struct BLOCKHEAD *left_p,
		struct BLOCKHEAD *right_p));
static void set_mrpt P((struct STAFF *staff_p, int mrpt_type, int count));
static int is_unprocessed_mrpt P((struct MAINLL *mll_p));
static int mrpt_number P((int mrpt_type));
static char *mrt2name P((int mrt_type));
static int voices_override P((int staff, int field));
static int mult_rpt_ssv_ok P((struct SSV *ssv_p, int staff, int param,
		char *desc, char *inputfile, int inputlineno));
static int proc_multrpt P((struct MAINLL *mll_p));
static void do_midi_mrpt_copy P((struct MAINLL *mll_p));




/* give error message if given number is not within specified range. */
/* return NO if out of range, YES if okay */

/* once upon a time, there was the rangecheck function, and it got called
 * many times. Then midi support was added and that code needed to do lots
 * of rangechecks, but with the filename and line number something other
 * than Curr_filename, and yylineno, so the l_rangecheck function was
 * created, and rangecheck just calls that. There isn't an l_frangecheck
 * to go with frangecheck, because nothing needed it. Another case where
 * C++ would have been nice, so we could default added arguments....
 */

int
rangecheck(n, min, max, name)

int n;		/* the number to check */
int min;	/* has to be at least this big */
int max;	/* can be no bigger than this */
char *name;	/* describes what n represents, to use in error message */

{
	return(l_rangecheck(n, min, max, name, Curr_filename, yylineno));
}

int
l_rangecheck(n, min, max, name, fname, lineno)

int n;		/* the number to check */
int min;	/* has to be at least this big */
int max;	/* can be no bigger than this */
char *name;	/* describes what n represents, to use in error message */
char *fname;	/* file name */
int lineno;	/* line number */

{
	if ( (n < min) || (n > max) ) {
		l_yyerror(fname, lineno,
				"%s (%d) is out of range (must be between %d and %d)",
				name, n, min, max);
		return(NO);
	}
	return(YES);
}


/* This function is rather like rangecheck, except it also allows a special
 * "empty" value. */

int
erangecheck(n, min, max, empty_value, name)

int n;		/* the number to check */
int min;	/* has to be at least this big */
int max;	/* can be no bigger than this */
int empty_value;	/* this is also a legal value */
char *name;	/* describes what n represents, to use in error message */

{
	if (n == empty_value) {
		/* value is okay--means user set to empty */
		return(YES);
	}
	if ( (n < min) || (n > max) ) {
		l_yyerror(Curr_filename, yylineno,
				"%s (%d) out of range (must be between %d to %d or set to nothing at all)",
				name, n, min, max);
		return(NO);
	}
	return(YES);
}


/* just like rangecheck except for a float instead of int */

int
frangecheck(n, min, max, name)

float n;	/* the number to check */
float min;	/* has to be at least this big */
float max;	/* can be no bigger than this */
char *name;	/* describes what n represents, to use in error message */

{
	if ( (n < min) || (n > max) ) {
		l_yyerror(Curr_filename, yylineno,
				"%s (%.3f) is out of range (must be between %.3f and %.3f)",
				name, n, min, max);
		return(NO);
	}
	return(YES);
}


/* give error and return NO if given number is not a power of 2 */

int
power_of2check(n, name)

int n;		/* number to verify */
char *name;	/* what n represents, for error message */

{
	if ( (n <= 0) || ((n & (n - 1)) != 0)) {
		l_yyerror(Curr_filename, yylineno,
				"%s (%d) not a power of 2", name, n);
		return(NO);
	}
	return(YES);
}


/* check that current action is valid in current context. */
/* If so, return YES, otherwise print message and return NO */

int
contextcheck(validcontext, action)

UINT32B validcontext;	/* bitmap of valid contexts */
char *action;		/* what action is to be done, for error messages */

{
	static int shouldBmusic; /* count of how many consecutive times
				* we were called when we should have been
				* in music context, but weren't
				*/

	/* Forgetting to say 'music' can cause tons of errors,
	 * which may confuse the user. So try to deduce what they meant.
	 */
	if (validcontext == C_MUSIC) {
		if (Context != C_MUSIC) {
			if (++shouldBmusic > 5) {
				
				l_yyerror(Curr_filename, yylineno, "guessing you forgot to specify 'music'; changing to music context to try to recover");
				Context = C_MUSIC;
			}
		}
		else {
			shouldBmusic = 0;
		}
	}
	else {
		shouldBmusic = 0;
	}

	if ((validcontext & Context) == 0) {
		l_yyerror(Curr_filename, yylineno, "%s not valid in %s context",
			action, contextname(Context));
		return(NO);
	}
	return(YES);
}


/* convert context number back to name */

char *
contextname(cont)

UINT32B cont;	/* context number */

{
	switch(cont)  {
	case C_MUSIC:
		return("music");
	case C_SCORE:
		return("score");
	case C_STAFF:
		return("staff");
	case C_VOICE:
		return("voice");
	case C_HEADER:
		return("header");
	case C_FOOTER:
		return("footer");
	case C_HEAD2:
		return("header2");
	case C_FOOT2:
		return("footer2");
	case C_TOP:
		return("top");
	case C_TOP2:
		return("top2");
	case C_BOT:
		return("bottom");
	case C_BOT2:
		return("bottom2");
	case C_BLOCK:
		return("block");
	case C_HEADSHAPES:
		return("headshapes");
	case C_SHAPES:
		return("shapes");
	case C_GRIDS:
		return("grids");
	case C_SYMBOL:
		return("symbol");
	case C_KEYMAP:
		return("keymap");
	case C_ACCIDENTALS:
		return("accidentals");
	case C_CONTROL:
		return("control");
	default:
		return("unknown");
	}
}


/* Go though the list and expand repeated sections as if the user had
 * written everything out with no repeats. That way, the rest of the code
 * won't have to deal with any special cases for repeats.
 */

void
expand_repeats()
{
	struct MAINLL *begin_repeat_mll_p;
	struct MAINLL *mll_p;
	struct MAINLL *clone_end_p;


	/* First do any octave transpositions. Then if we need to clone a
	 * section because it is repeated, everything will already
	 * be transposed. Skip if tuning is used, since transposition
	 * get handled when generating tuning maps in that case. */
	if (Tuning_used == NO) {
		int staff;
		int vindex;

		initstructs();

		/* For historical reasons, octave_transpose expects to
		 * get called for each measure in turn for a single voice,
		 * so we loop through voices to force that. Normally it
		 * would expect going through each staff/voice combination
		 * individually, but it has arrays per staff, so we can
		 * actually do the staffs in parallel, just not voices.
		 */
		for (vindex = 0; vindex < MAXVOICES; vindex++) {
			initstructs();
			for (staff = 1; staff <= MAXSTAFFS; staff++) {
				Octave_adjust[staff] = 0;
				Octave_bars[staff] = 0;
				Octave_count[staff] = 0.0;
			}
			for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
				switch(mll_p->str) {
				case S_SSV:
					asgnssv(mll_p->u.ssv_p);
					break;
				case S_STAFF:
					if (mll_p->u.staff_p->groups_p[vindex] == 0) {
						break;
					}
					octave_transpose(mll_p->u.staff_p, mll_p,
							vindex, YES);
					break;
				default:
					break;
				}
			}
		}
	}

	/* Now loop through again, unrolling any repeats */
	initstructs();

	/* In case there is an implicit repeatstart at the beginning
	 * of the song, set repeat return point to beginning of song. */
	begin_repeat_mll_p = Mainllhc_p;

	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		switch(mll_p->str) {
		case S_SSV:
			asgnssv(mll_p->u.ssv_p);
			break;
		case S_BAR:
			clone_end_p = 0;
			switch(mll_p->u.bar_p->bartype) {
			case REPEATSTART:
				begin_repeat_mll_p = mll_p->next;
				break;
			case REPEATEND:
				clone_end_p = clone_repeated_section(begin_repeat_mll_p, mll_p);

				mll_p->u.bar_p->bartype = DOUBLEBAR;
				mll_p->u.bar_p->endingloc = NOITEM;
				/* We probably really should set
				 * begin_repeat_mll_p to 0 and then complain
				 * if we hit a repeatend that there was no
				 * matching begin, but the way the old code
				 * worked was to essentially treat this as
				 * a repeatboth and set the begin to the
				 * next measure, so we'll do that to stay
				 * backward compatible. */
				begin_repeat_mll_p = clone_end_p->next;
				break;
			case REPEATBOTH:
				clone_end_p = clone_repeated_section(begin_repeat_mll_p, mll_p);
				mll_p->u.bar_p->bartype = DOUBLEBAR;
				mll_p->u.bar_p->endingloc = NOITEM;
				begin_repeat_mll_p = clone_end_p->next;
				break;
			default:
				break;
			}
			if (mll_p->u.bar_p->endingloc == ENDITEM ||
				mll_p->u.bar_p->endingloc == INITEM) {
				mll_p->u.bar_p->endingloc = NOITEM;
			}
			if (clone_end_p != 0) {
				/* skip past the cloned section */
				mll_p = clone_end_p;
			}
			break;
		default:
			break;
		}
	}
}


/* Make a copy of the midi-relevant things in the main list, starting
 * from begin_mll_p and going to either end_mll_p or the first ending
 * that comes before that, if any. Place that copy right after end_mll_p.
 * Return pointer to last main list item added.
 */

static struct MAINLL *
clone_repeated_section(begin_mll_p, end_mll_p)

struct MAINLL *begin_mll_p;
struct MAINLL *end_mll_p;

{
	struct MAINLL *curr_src_p;	/* where we are in original */
	struct MAINLL *curr_dest_p;	/* where we are in the clone */
	struct MAINLL *new_mll_p;	/* for STAFF clone */
	struct MAINLL *src_staffs_p;	/* where current measure STAFFs start
					 * in the original */
	struct MAINLL *dest_staffs_p;	/* where current measure STAFFs start
					 * in the copy */
	struct BAR *bar_p;
	int v;				/* voice index */


	/* Before starting the second time, restore parameters as they were
	 * at the beginning of the repeated section.
	 */
	curr_dest_p = restoreparms(begin_mll_p, end_mll_p);
	/* The restoreparms will have changed the SSV state, so set back to
	 * what it should be at the end of the first time through */
	setssvstate(end_mll_p);

	src_staffs_p = dest_staffs_p = 0;

	for (curr_src_p = begin_mll_p;   ; curr_src_p = curr_src_p->next) {
		switch (curr_src_p->str) {

		case S_STAFF:
			new_mll_p = newMAINLLstruct(S_STAFF, curr_src_p->inputlineno);
			memcpy(new_mll_p->u.staff_p, curr_src_p->u.staff_p,
							sizeof(struct STAFF));
			for (v = 0; v < MAXVOICES; v++) {
				new_mll_p->u.staff_p->groups_p[v] =
					clone_gs_list(curr_src_p->u.staff_p->groups_p[v], YES);
			}

			/* There may be MIDI related STUFFs, so dup them. */
			new_mll_p->u.staff_p->stuff_p = curr_src_p->u.staff_p->stuff_p;

			/* We can ignore lyrics for MIDI purposes */
			new_mll_p->u.staff_p->nsyllists = 0;
			new_mll_p->u.staff_p->sylplace = 0;
			new_mll_p->u.staff_p->syls_p = 0;

			/* Keep track of where the measure starts, in case
			 * we need to clone timed SSVs */
			if (src_staffs_p == 0) {
				src_staffs_p = curr_src_p;
				dest_staffs_p = new_mll_p;
			}
			break;

		case S_BAR:
			bar_p = curr_src_p->u.bar_p;
			new_mll_p = newMAINLLstruct(S_BAR, curr_src_p->inputlineno);
        		memcpy(new_mll_p->u.bar_p, bar_p, sizeof(struct BAR));
			if (bar_p->bartype == REPEATEND || bar_p->bartype ==  REPEATBOTH) {
				new_mll_p->u.bar_p->bartype = DOUBLEBAR;
			}
			new_mll_p->u.bar_p->endingloc = NOITEM;

			/* Timed SSV include a pointer to a GRPSYL, so we need
			 * to make a copy of that list and adjust to point
			 * to the corresponding group in the copy. */
			new_mll_p->u.bar_p->timedssv_p = clone_tssv(bar_p->timedssv_p, src_staffs_p, dest_staffs_p);
			/* re-init for next measure */
			src_staffs_p = dest_staffs_p = 0;

			break;

		case S_SSV:
			asgnssv(curr_src_p->u.ssv_p);
			/* Clone it */
			new_mll_p = newMAINLLstruct(S_SSV, curr_src_p->inputlineno);
			memcpy(new_mll_p->u.ssv_p, curr_src_p->u.ssv_p, sizeof(struct SSV));

			break;

		default:
			/* All other things are irrelevant for MIDI */
			new_mll_p = 0;
			break;
		}

		if (new_mll_p != 0) {
			/* Add to main list, and keep track of where we are,
			 * for adding in the next one. */
			insertMAINLL(new_mll_p, curr_dest_p);
			/* If this begins the first ending, we are done. */
			if (curr_src_p->str == S_BAR &&
					curr_src_p->u.bar_p->endingloc
					== STARTITEM) {
				curr_src_p->u.bar_p->endingloc = NOITEM;
				return(new_mll_p);
			}
			/* Also done if we have reached the end of section */
			if (curr_src_p == end_mll_p) {
				curr_src_p->u.bar_p->endingloc = NOITEM;
				return(new_mll_p);
			}
			curr_dest_p = new_mll_p;
		}
	}
	return(curr_dest_p);
}


/* Clone a TIMEDSSV list, and return a pointer to the copy */

static struct TIMEDSSV *
clone_tssv(src_tssv_p, src_mll_p, dest_mll_p)

struct TIMEDSSV *src_tssv_p;	/* list to be cloned */
struct MAINLL *src_mll_p;	/* where STAFF structs start in the original */
struct MAINLL *dest_mll_p;	/* where STAFF structs start in the copy */

{
	struct TIMEDSSV *tssv_p;	/* walk through src_tssv_p list */
	struct TIMEDSSV *new_tssv_p;	/* items currently being cloned */
	struct TIMEDSSV *head_p;	/* head of the clone, to be returned */
	struct TIMEDSSV **place_p_p;	/* where to link in next entry */
	struct MAINLL *s_mll_p;		/* walk through src_mll_p */
	struct MAINLL *d_mll_p;		/* walk through dest_mll_p */
	struct GRPSYL *s_gs_p;		/* walk through source list of GRPSYLs */
	struct GRPSYL *d_gs_p;		/* walk through destination list */
	int numvoices;
	int found;
	int v;				/* voice index */

	/* start with empty list */
	head_p = 0;
	/* nowhere to link to yet */
	place_p_p = 0;
	/* walk through original list of timed SSVs, cloning the list */
	for (tssv_p = src_tssv_p; tssv_p != 0; tssv_p = tssv_p->next) {
		/* Make a copy */
		MALLOC(TIMEDSSV, new_tssv_p, 1);
		memcpy(new_tssv_p, tssv_p, sizeof(struct TIMEDSSV));

		/* Do list linkage */
		if (head_p == 0) {
			head_p = new_tssv_p;
		}
		if (place_p_p != 0) {
			*place_p_p = new_tssv_p;
		}
		place_p_p = &(new_tssv_p->next);
		new_tssv_p->next = 0;

		/* deduce the corresponding GRPSYL in the copy */
		found = NO;
		d_gs_p = 0;	/* for lint */
		for (s_mll_p = src_mll_p, d_mll_p = dest_mll_p;
				s_mll_p->str == S_STAFF;
				s_mll_p = s_mll_p->next, d_mll_p = d_mll_p->next) {
			/* Check every GRPSYL till we find the one the
			 * original TIMEDSSV pointed to, and set the copy's
			 * pointer to the corresponding group */
			numvoices = vscheme_voices(svpath(s_mll_p->u.staff_p->staffno, VSCHEME)->vscheme);
			for (v = 0; v < numvoices; v++) {
				s_gs_p = s_mll_p->u.staff_p->groups_p[v];
				d_gs_p = d_mll_p->u.staff_p->groups_p[v];
				while (s_gs_p != 0) {
					if (s_gs_p == tssv_p->grpsyl_p) {
						/* Eureka! */
						found = YES;
						break;
					}
					s_gs_p = s_gs_p->next;
					d_gs_p = d_gs_p->next;
				}
			}
			if (found == YES) {
				new_tssv_p->grpsyl_p = d_gs_p;
				break;
			}
		}
		if (found == NO) {
			pfatal("failed to find matching timed SSV grpsyl");
		}
	}
	return(head_p);
}


/* check that at least one staff is visible, print error message if not */

void
check_at_least1visible()

{
	int staffno;

	/* go through list of staffs, if we find a visible one, fine */
	for (staffno = Score.staffs; staffno > 0; staffno--) {
		if ( (svpath(staffno, VISIBLE))->visible == YES) {
			return;
		}
	}

	yyerror("no staffs visible");
	return;
}


/* if there is a change in visibility status, need to have a scorefeed after
 * that. So go through main list. If there is a change in visibility, or
 * in number of staffs, or stafflines, or staffscale, go
 * backwards in list till hit FEED, BAR, or beginning of list. If hit FEED
 * first, throw it away. Then search forward from the SSV until we hit
 * FEED or STAFF.  If FEED, fine. If STAFF, insert
 * a FEED. If we discarded the user FEED because it was in the wrong place,
 * mark the pagefeed field as they had it.
 * This function also adds a measure of space to the beginning of the song
 * if we are doing MIDI. If a song begins with a grace note,
 * we want to move that back into the "previous" measure, so this will
 * create that measure. It's easier to deal with that here, before
 * makechords() is called, than to try to add it in later.
 */

void
chk_vis_feed()

{
	struct MAINLL *mll_p, *m_p;	/* to walk through main list */
	struct MAINLL *new_feed_p;
	short set_pagefeed = NO;	/* if to set pagefeed field */
	short s;			/* staff index */
	short vis[MAXSTAFFS + 1];	/* which staffs are currently visible */
	short stlines[MAXSTAFFS + 1];	/* stafflines for each staff */
	float stscale[MAXSTAFFS + 1];	/* staffscale for each staff */
	short num_staffs;		/* number of staffs */
	short add_extra;		/* if to add extra space measure */


	debug(4, "chk_vis_feed");

	/* If doing MIDI, we want to add an extra space measure to the
	 * beginning */
	add_extra = Doing_MIDI;

	/* go through main list looking for visibility changes */
	initstructs();
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {

		if (mll_p->str == S_SSV) {
			/* SSV can only follow a BAR or BLOCKHEAD or another
			 * SSV. If it follows a LINE/CURVE/PRHEAD,
			 * then user must have entered music context,
			 * but not entered any music,
			 * just line/curve/print things. This violates our
			 * mainlist rules, so we have to disallow */
			if (mll_p->prev != 0 && mll_p->prev->str != S_BAR &&
					mll_p->prev->str != S_BLOCKHEAD &&
					mll_p->prev->str != S_SSV) {
				l_yyerror(mll_p->inputfile, mll_p->inputlineno,
					"music context containing only lines/curves/print statements is not allowed");
			}

			/* Only want to insert a FEED if there truly was
			 * a change in the number of staffs to be printed.
			 * User may have set visible when it was already set,
			 * or just changed voice visibility which didn't
			 * cause the visibily of staffs to change, or
			 * something like that, which doesn't count.
			 * So see what visiblity, stafflines,
			 * and staffscale are set to now,
			 * then assign the SSV and see if things changed.
			 */
			for (s = 1; s <= MAXSTAFFS; s++) {
				/* save current values of interest */
				vis[s] = svpath(s, VISIBLE)->visible;
				stlines[s] = svpath(s, STAFFLINES)->stafflines;
				stscale[s] = svpath(s, STAFFSCALE)->staffscale;
			}
			num_staffs = Score.staffs;

			/* make any updates */
			asgnssv(mll_p->u.ssv_p);

			/* now compare with previous values */
			for (s = 1; s <= MAXSTAFFS; s++) {
				if (vis[s] != svpath(s, VISIBLE)->visible ||
						stlines[s] != svpath(s,
						STAFFLINES)->stafflines ||
						stscale[s] != svpath(s,
						STAFFSCALE)->staffscale) {
					/* something changed */
					break;
				}
			}

			if (s <= MAXSTAFFS || Score.staffs != num_staffs) {

				/* found a change. Go backwards. If find a
				 * FEED, discard it. Otherwise ok */
				for (m_p = mll_p->prev;
						m_p != (struct MAINLL *) 0;
						m_p = m_p->prev) {

					if (IS_CLEFSIG_FEED(m_p)) {
						/* feed in wrong place. Discard
						 * this one. We'll put one in
						 * the proper place later */
						set_pagefeed =
							m_p->u.feed_p->pagefeed;
						unlinkMAINLL(m_p);
						FREE(m_p);
						break;
					}

					else if (m_p->str == S_BAR) {
						break;
					}
				}

				/* now look forwards. If find FEED, fine.
				 * If not, insert one */
				for (m_p = mll_p->next;
						m_p != (struct MAINLL *) 0;
						m_p = m_p->next) {

					if (m_p->str == S_FEED) {
						/* user already put one in */
						break;
					}

					else if (m_p->str == S_STAFF) {
						/* user neglected to put in an
						 * explicit feed, so we add
						 * one for them */
						new_feed_p =
							newMAINLLstruct(S_FEED,
							-1);
						new_feed_p->u.feed_p->pagefeed
								= set_pagefeed;
						insertMAINLL(new_feed_p,
								m_p->prev);
						break;
					}
				}
				set_pagefeed = NO;
			}
		}

		else if (add_extra == YES && mll_p->str == S_STAFF) {
			/* For MIDI purposes, add a measure space to the
			 * beginning of the song, in case we need to move
			 * grace notes back into it. Strictly speaking,
			 * we probably don't need to do this unless there
			 * truly is a grace note at the beginning, but
			 * it should never hurt to add it, and it doesn't
			 * seem worth the effort to check. */
			add_pre_meas(mll_p->prev, 1, Score.staffs, YES);

			add_extra = NO;
		}
	}
}


/* For MIDI, we add a measure of space preceding what the user put in.
 * This is used in case they start the piece with grace notes that we need
 * to move back in the preceding measure, since this guarantees there will
 * be a preceding measure. Also, for when taking a "slice" of the piece,
 * skipping measures at the beginning, this is where we attach any MIDI
 * STUFFs that happened during the skipped part.
 * Returns the last MAINLL added.
 */

static struct MAINLL *
add_pre_meas(insert_p, start, end, add_invis)

struct MAINLL *insert_p;	/* insert after here */
int start;			/* staff number of first STAFF to create */
int end;			/* staff number of last STAFF to create */
int add_invis;			/* if YES, add an invisible bar too */

{
	int staff;		/* loop through staffs to be created */
	struct MAINLL *new_p;	/* new STAFFs */
	int numvoices;		/* number of voices on current staff */
	int v;			/* voice index */

	/* Create a staff with measure space for all
	 * defined staffs/voices, and link onto main list */
	for (staff = start; staff <= end; staff++) {

		/* create the STAFF struct itself */
		new_p = newMAINLLstruct(S_STAFF, -1);
		new_p->u.staff_p->staffno = staff;
		new_p->u.staff_p->visible = svpath(staff, VISIBLE)->visible;

		numvoices = vscheme_voices(svpath(staff, VSCHEME)->vscheme);
		for (v = 0; v < numvoices; v++) {
			add_meas_space( &(new_p->u.staff_p->groups_p[v]),
						staff, v + 1);
		}

		/* link onto main list, and arrange to
		 * link the next thing after this one */
		insertMAINLL(new_p, insert_p);
		insert_p = new_p;
	}
	if (add_invis == YES) {
		/* add an invisible bar line */
		new_p = newMAINLLstruct(S_BAR, -1);
		new_p->u.bar_p->bartype = INVISBAR;
		/* If doing MIDI and there are grace notes at the beginning,
		 * there could be an implicit repeatstart to here.
		 * If that is the case, we make is explicit, so that midi
		 * code doesn't have to treat that case specially later.
		 */
		if (Doing_MIDI == YES) {
			/* Go through main list from here. If we hit a
			 * repeat(end|both) before hitting a repeatstart,
			 * then there was an implicit repeatstart. */
			struct MAINLL *mll_p;

			for (mll_p = insert_p; mll_p != 0; mll_p = mll_p->next) {
				if (mll_p->str != S_BAR) {
					continue;
				}
				if (mll_p->u.bar_p->bartype == REPEATSTART) {
					/* No implicit repeat at beginning */
					break;
				}
				if (mll_p->u.bar_p->bartype == REPEATEND ||
						mll_p->u.bar_p->bartype
						== REPEATBOTH) {
					/* Make the implicit explicit */
					new_p->u.bar_p->bartype = REPEATSTART;
					/* No need to look farther */
					break;
				}
			}
		}
		insertMAINLL(new_p, insert_p);
		insert_p = new_p;
	}
	return(insert_p);
}


/* check for valid interval. Unison, octave, fourths and fifths can not be
 * major or minor. The others cannot be perfect. */

void
chk_interval(inttype, intnum)

int inttype;	/* PERFECT, MINOR, etc */
int intnum;	/* e.g., 4 for fourth */

{
	if (intnum <= 0) {
		yyerror("transposition interval must be > 0");
		return;
	}

	/* collapse into 1 octave. It's okay that a 7th will come out zero
	 * because of the way things are checked below. */
	intnum %= 7;

	switch (inttype) {

	case  PERFECT:
		switch (intnum) {
		case 1:	
		case 4:
		case 5:
			break;
		default:
			yyerror("specified interval cannot be perfect");
			break;
		}
		break;

	case MAJOR:
	case MINOR:
		switch(intnum) {
		case 1:
		case 4:
		case 5:
			yyerror("specified interval cannot be major or minor");
			break;
		default:
			break;
		}
		break;

	default:
		/* everything else is okay */
		break;
	}
}


/* if specified used[] field is set to YES, print warning that its value is
 * being overridden. This is to let user know they set the same parameter
 * twice in the same SSV context */

void
used_check(mll_p, var, name)

struct MAINLL *mll_p;	/* check used[] in the SSV pointed to by this */
int var;		/* check this index in the used[] array */
char *name;		/* name of variable, for warning message */

{
	if (mll_p == (struct MAINLL *) 0) {
		l_yyerror(Curr_filename, yylineno,
			"can't set %s in %s context", name, contextname(Context));
		return;
	}

	if (mll_p->str != S_SSV) {
		pfatal("bad argument passed to used_check()");
	}

	if (mll_p->u.ssv_p->used[var] == YES) {
		l_warning(Curr_filename, yylineno,
			"setting of '%s' parameter overrides previous setting",
			name);
	}
}


/* Go through list and combine multiple consecutive rests into multi-rests.
 * This also allocates space for the restc array for any rests groups that
 * didn't have location tags. This may not be the most intuitive place to
 * do that, but seems like a safe place. Nothing should use restc before
 * placement, but with all the code for mapping and cloning groups, it
 * might be possible to miss a place, so this runs in between, after all
 * this tricky cases should be done, and before placement. This also lets
 * us do it after all combining has happened, so we don't have to do rests
 * that were combined away. */

void
combine_rests(c)

int c;	/* argument to -c command line option;
	 * only combine when there are at least
	 * this many rest measures in a row.
	 * Set to NORESTCOMBINE if user didn't use -c option. */

{
	struct MAINLL *mll_p;	/* walk through main list */
	struct MAINLL *begin_p = (struct MAINLL *) 0;	/* where section to
							 * combine begins */
	struct MAINLL *end_p = (struct MAINLL *) 0;
	int all_rests = YES;
	int n;			/* how many measures minimum to combine */
	int count = 0;		/* how many measures of all rests */
	int begin_valid = NO;	/* if begin_p has been set. Can't just check to
				 * see if it is null, because null is valid */
	struct SSV *ssv_p;
	char *timerep;		/* current time signature representation */


	debug(2, "combine_rests");

	/* go through main list */
	initstructs();
	n = c;	/* init to value of -c option */
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {

		if (Doing_MIDI == YES && mll_p == Mainllhc_p) {
			/* Don't want to combine the "extra" measure
			 * we added in for MIDI, so skip it. */
			for (  ; mll_p != 0; mll_p = mll_p->next) {
				if (mll_p->str == S_BAR) {
					mll_p = mll_p->next;
					break;
				}
			}
			if (mll_p == 0) {
				/* Must be no valid music data in the file.
				 * check4barline_at_end() should have already
				 * reported this to user, so we can just
				 * return silently here. */
				return;
			}
		}

		/* for each STAFF that is visible, see if it is all rests */
		if (mll_p->str == S_STAFF) {
			/* remember where we are, in case this is the beginning
			 * of section that needs to be combined */
			if (begin_valid == NO) {
				begin_p = mll_p->prev;
				begin_valid = YES;
			}

			/* if we have all rests so far, check this staff */
			if (all_rests == YES) {
				if ((all_rests = check_all_rests
						(mll_p->u.staff_p, count))
						== NO) {
					/* this measure was not all rests.
					 * Check to see if before that we
					 * had seen a run of
					 * all rests. If so, combine them */
					do_combine(begin_p, end_p, count, n);
					/* If it was only stopped because of
					 * a STUFF at the beginning, this
					 * could begin another run. */
					if ((all_rests = check_all_rests
							(mll_p->u.staff_p, 0))
							== YES) {
						/* Prepare for another
						 * possible run.
						 * Set beginning of next
						 * potential run
						 * to just before the first
						 * staff in this measure,
						 * which had ended the
						 * current run. */
						for (begin_p = mll_p->prev;
							begin_p != 0 &&
							begin_p->str == S_STAFF;
							begin_p = begin_p->prev) {
						    ;
						}
						begin_valid = YES;
						all_rests = YES;
					}
					else {
						/* Had to stop because of
						 * something other than just a
						 * STUFF at the beginning of
						 * the measure, so we cannot
						 * include this measure in
						 * rests to combine. */
						all_rests = NO;
					}
					count = 0;
				}
			}
		}

		else if (mll_p->str == S_BAR) {
			if (all_rests == YES) {
				/* this measure was all rests, so
				 * bump counter */
				if (mll_p->u.bar_p->bartype != INVISBAR) {
					count++;
				}
				end_p = mll_p;
				/* if not an ordinary bar, end the combining */
				if ( (mll_p->u.bar_p->reh_type != REH_NONE) ||
						(mll_p->u.bar_p->endingloc
						!= NOITEM &&
						mll_p->u.bar_p->endingloc
						!= INITEM) ||
						( (mll_p->u.bar_p->bartype
						!= SINGLEBAR) &&
						(mll_p->u.bar_p->bartype !=
						INVISBAR) ) ) {
					if (mll_p->u.bar_p->bartype == RESTART) {
						/* There is an empty "measure"
						 * of space before restarts */
						count--;
						/* need to end combining at
						 * the previous bar line */
						if (count >= n) {
							for (end_p = end_p->prev;
							end_p != 0 &&
							end_p->str != S_BAR;
							end_p = end_p->prev) {
								;
							}
						}
					}

					do_combine(begin_p, end_p, count, n);

					/* re-initialize */
					count = 0;
					begin_p = (struct MAINLL *) 0;
					all_rests = YES;
					begin_valid = NO;
				}
			}
			else {
				/* re-init for next measure */
				all_rests = YES;
				count = 0;
				begin_p = (struct MAINLL *) 0;
				begin_valid = NO;
			}

			/* While we are here, make sure we don't print
			 * measure numbers on invisible bars, since they
			 * don't count. */
			if ((mll_p->u.bar_p->reh_type == REH_BAR_MNUM) &&
					(mll_p->u.bar_p->bartype == INVISBAR)) {
				mll_p->u.bar_p->reh_type = REH_NONE;
			}
		}

		else if (mll_p->str == S_SSV) {

			ssv_p = mll_p->u.ssv_p;

			/* If -c option was not used, we use the value of
			 * the restcombine parameter. */
			if (c == NORESTCOMBINE &&
					ssv_p->used[RESTCOMBINE] == YES) {
				n = ssv_p->restcombine;
			}

			/* if there is a change in visibility or a relevant
			 * change on an already-visible score,
			 * that is grounds to end the combination */
			if ( ssv_p->used[VISIBLE] == YES ||
					((((mll_p->u.ssv_p->staffno == 0) ||
					(svpath(mll_p->u.ssv_p->staffno,
					VISIBLE))->visible == YES)) &&
					(ssv_p->used[CLEF] == YES
					|| ssv_p->used[SHARPS] == YES
					|| ssv_p->used[TIME] == YES
					|| ssv_p->used[TRANSPOSITION] == YES
					|| ssv_p->used[ADDTRANSPOSITION] == YES
					|| ssv_p->used[VISIBLE] == YES))) {
				do_combine(begin_p, end_p, count, n);

				/* re-initialize */
				count = 0;
				begin_p = (struct MAINLL *) 0;
				all_rests = YES;
				begin_valid = NO;
			}

			/* keep track of visibility */
			asgnssv(mll_p->u.ssv_p);
		}
		else if (mll_p->str == S_FEED) {
			do_combine(begin_p, end_p, count, n);
			count = 0;
			begin_p = (struct MAINLL *) 0;
			all_rests = YES;
			begin_valid = NO;
		}
	}

	/* do final combination if any */
	do_combine(begin_p, end_p, count, n);

	/* If there were case of TS_ALWAYS with alternating time
	 * signatures, we had to save the entire time signature list for
	 * every measure in case it turned out to be a multirest.
	 * Now we can shorten down to one for those that aren't,
	 * and need to do this so later code works.
	 */
	initstructs();
	timerep = 0;
	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		struct MAINLL * nmll_p;

		if (mll_p->str != S_SSV) {
			continue;
		}
		asgnssv(mll_p->u.ssv_p);
		if (Score.timevis != PTS_ALWAYS) {
			continue;
		}

		/* If we need to change the current timerep,
		 * need to do the permanent copy in the main list,
		 * not the one current in Score, so save pointer to current
		 * one in main list. */
		if (mll_p->u.ssv_p->used[TIME]) {
			timerep = mll_p->u.ssv_p->timerep;
		}

		for (nmll_p = mll_p->next; nmll_p != 0;
						nmll_p = nmll_p->next) {
			if (nmll_p->str == S_STAFF) {
				if (nmll_p->u.staff_p->groups_p[0]->is_multirest== NO) {
					/* not followed by multi-rest,
					 * can truncate time sig */
					char * t;
					for (t = timerep; t != 0 && *t != 0; t++) {
						if (*t == TSR_ALTERNATING) {
							*t = TSR_END;
							break;
						}
					}
				}
				break;
			}
		}
	}

	/* Now go through list again, finding any rest groups without restc
	 * allocated, and allocating it. */
	initstructs();
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			struct GRPSYL *gs_p;
			int v;
		
			for (v = 0; v < MAXVOICES; v++) {
				for (gs_p = mll_p->u.staff_p->groups_p[v];
						gs_p != 0; gs_p = gs_p->next) {
					if (gs_p->grpcont == GC_REST &&
							gs_p->restc == 0) {
						MALLOCA(float, gs_p->restc, NUMCTYPE);
					}
				}
			}
		}
	}
}


/* given a STAFF, return NO if the staff is visible and has at least one
 * voice which contains notes, or has lyrics or STUFF (except if stuff is on
 * or before the first beat of the first measure. Otherwise return YES */

static int
check_all_rests(staff_p, count)

struct STAFF *staff_p;	/* which staff info to check */
int count;		/* how many measures of all rest so far */

{
	int v;			/* index through voices */
	struct GRPSYL *gs_p;	/* walk through grpsyl list */
	struct STUFF *stuff_p;	/* walk through stuff list */


	/* if not visible, then okay to treat as all rests */
	if ( (svpath(staff_p->staffno, VISIBLE))->visible == NO) {
		return(YES);
	}

	/* if has lyrics, consider that not rests */
	if (staff_p->nsyllists > 0) {
		return(NO);
	}

	/* Having STUFF is usually grounds for not being treated as all rests.
	 * It is somewhat unclear what to do about earlier STUFFs
	 * that happen to spill into this measure, but we ignore them.
	 * There is one special case:
	 * If this is the first measure of rests, and	
	 * all STUFFs occur on or before beat 1 and have no til clause,
	 * then that's okay. This allows user to change tempo
	 * or something similar at the beginning of a combined multirest.
	 * Note that rest combining only applies to printing, not MIDI,
	 * so all midi STUFFs should be ignored.
	 */
	for (stuff_p = staff_p->stuff_p; stuff_p != (struct STUFF *) 0;
					stuff_p = stuff_p->next) {
		if (stuff_p->stuff_type == ST_MIDI) {
			continue;
		}

		/* Ignore phrases on invisible voices */
		if (stuff_p->stuff_type == ST_PHRASE) {
			if ((stuff_p->vno == 1 ||
						(stuff_p->vno == 0 &&
						stuff_p->place == PL_ABOVE)) &&
					vvpath(staff_p->staffno, 1, VISIBLE)->visible == NO) {
				continue;
			}
			if ((stuff_p->vno == 2 ||
					(stuff_p->vno == 0 &&
					stuff_p->place == PL_BELOW)) && 
					vvpath(staff_p->staffno, 2, VISIBLE)->visible == NO) {
				continue;
			}
			if (stuff_p->vno == 3 && 
					vvpath(staff_p->staffno, 3, VISIBLE)->visible == NO) {
				continue;
			}
		}

		if (count > 0 || stuff_p->start.count > 1.0
					|| stuff_p->end.bars > 0
					|| stuff_p->end.count > 0.0) {
			return(NO);
		}
	}

	for (v = 0; v < MAXVOICES; v++) {
		for (gs_p = staff_p->groups_p[v]; gs_p != (struct GRPSYL *) 0;
							gs_p = gs_p->next) {
			/* if voice is invisible, treat like all rests */
			if (vvpath(staff_p->staffno, v + 1, VISIBLE)->visible == NO) {
				continue;
			}

			if (gs_p->grpcont == GC_NOTES) {
				return(NO);
			}

			else if (gs_p->is_meas == NO) {
				/* We only combine mr and ms. If user entered
				 * one or more rests/spaces that fill the
				 * entire measure, we don't combine,
				 * because the user may have had some reason
				 * for explicitly specifying time values rather
				 * than using a measure duration. */
				return(NO);
			}
			else if (gs_p->is_multirest == YES) {
				/* already multirest! */
				return(NO);
			}
		}
	}
	return(YES);
}


/* effect the combination of several measures of rests into a multirest. */

static void
do_combine(begin_p, end_p, nummeas, min_combine)

struct MAINLL *begin_p;	/* start combining from here */
struct MAINLL *end_p;	/* stop here */
int nummeas;		/* hom many measures are being combined */
int min_combine;	/* minimum number to combine, or NORESTCOMBINE */

{
	struct MAINLL *new_p;
	struct MAINLL *old_p;		/* first of items to discard */
	struct GRPSYL *gs_p;		/* for multirest */
	struct MAINLL *mll_p;		/* to find staff for transferring stuffs */
	short s;			/* index through staffs */
	short numvoices;


	if (min_combine == NORESTCOMBINE || nummeas < min_combine) {
		/* don't bother to combine */
		return;
	}

	/* discard everything in main list between the given points.
	 * It will be either STAFFs with all rests to be replaced or
	 * BARs to be discarded, or things associated with invisible staffs.
 	 * I guess maybe we should free up the space rather than merely
	 * unhitching it from the list, but it hardly seems worth the bother,
	 * especially since we'd have to be careful not to delete the STUFFs
	 * on the first one in case they were needed at the end of this
	 * function.
	 */
	if (begin_p == (struct MAINLL *) 0) {
		old_p = Mainllhc_p;
		Mainllhc_p = end_p;
	}
	else {
		old_p = begin_p->next;
		begin_p->next = end_p;
	}
	if (end_p != (struct MAINLL *) 0) {
		end_p->prev = begin_p;
	}

	/* add multirest to list */
	for (s = Score.staffs; s > 0; s--) {
		new_p = newMAINLLstruct(S_STAFF, -1);
		gs_p = newGRPSYL(GS_GROUP);
		gs_p->grpcont = GC_REST;
		gs_p->basictime = -nummeas;
		gs_p->is_multirest = YES;
		gs_p->fulltime = Score.time;
		gs_p->staffno = s;
		gs_p->vno = 1;

		new_p->u.staff_p->groups_p[0] = gs_p;
		numvoices = vscheme_voices(svpath(s, VSCHEME)->vscheme);
		if (numvoices > 1) {
			add_meas_space( &(new_p->u.staff_p->groups_p[1]), s, 2);
			new_p->u.staff_p->groups_p[1]->basictime = -nummeas;
			/* if the first voice was invisible,
			 * but the second voice is visible, need to convert
			 * the space just created into a rest. */
			if (vvpath(s, 1, VISIBLE)->visible == NO &&
					vvpath(s, 2, VISIBLE)->visible == YES) {
				new_p->u.staff_p->groups_p[1]->grpcont = GC_REST;
				new_p->u.staff_p->groups_p[1]->is_multirest = YES;
			}
		}
		if (numvoices > 2) {
			add_meas_space( &(new_p->u.staff_p->groups_p[2]), s, 3);
			new_p->u.staff_p->groups_p[2]->basictime = -nummeas;
			new_p->u.staff_p->groups_p[2]->is_multirest = YES;
			/* if only the third voice is visible, need to convert
			 * the space just created into a rest. */
			if (vvpath(s, 1, VISIBLE)->visible == NO &&
					vvpath(s, 2, VISIBLE)->visible == NO &&
					vvpath(s, 3, VISIBLE)->visible == YES) {
				new_p->u.staff_p->groups_p[2]->grpcont = GC_REST;
				new_p->u.staff_p->groups_p[2]->is_multirest = YES;

			}
		}
		new_p->u.staff_p->staffno = s;
		new_p->u.staff_p->visible = svpath(s, VISIBLE)->visible;
		insertMAINLL(new_p, begin_p);

		/* if there were any STUFFs on or before the first beat of the
		 * first measure, we have to transfer them to the stufflist of
		 * the multirest. We can transfer the entire list, because if
		 * there were any items that shouldn't be transferred, we
		 * wouldn't have allowed the multirest combination in the
		 * first place. */
		for (mll_p = old_p; mll_p != (struct MAINLL *) 0
				&& mll_p != end_p; mll_p = mll_p->next) {
			if (mll_p->str == S_STAFF
					&& mll_p->u.staff_p->staffno == s) {
				new_p->u.staff_p->stuff_p =
						mll_p->u.staff_p->stuff_p;
				break;
			}
		}
	}
}


/* translate MK_* to printable name */

char *
markname(mark)

int mark;	/* MK_* value */

{
	switch(mark) {
	case MK_MUSSYM:
		return("mussym");
	case MK_OCTAVE:
		return("octave");
	case MK_DYN:
		return("dyn");
	case MK_OTHERTEXT:
		return("othertext");
	case MK_CHORD:
		return("chord");
	case MK_LYRICS:
		return("lyrics");
	case MK_ENDING:
		return("ending");
	case MK_REHEARSAL:
		return("rehearsal");
	case MK_PEDAL:
		return("pedal");
	default:
		pfatal("markname(): missing case");
		/*NOTREACHED*/
		return("");
	}
}


/* verify that a mark order list is valid */

void
chk_order(ssv_p, place)

struct SSV *ssv_p;	/* check the markorder list in here */
int place;		/* PL_*, which list to check */

{
	int m, n;	/* index through MK_* */
	int level;	/* value in markorder table */

	for (m = 0; m < NUM_MARK; m++) {
		if (ssv_p->markorder[place][m] == 0) {
			/* no level set for this mark, so skip it */
			continue;
		}

		/* some mark types cannot be equal with any other types */
		switch (m) {

		case MK_LYRICS:
		case MK_ENDING:
		case MK_REHEARSAL:
		case MK_PEDAL:
			level = ssv_p->markorder[place][m];
			for (n = 0; n < NUM_MARK; n++) {
				if (n == m) {
					continue;
				}
				if (ssv_p->markorder[place][n] == level) {
					l_yyerror(Curr_filename, yylineno,
						"%s cannot be at same level as %s",
						markname(m), markname(n));
				}
			}
			break;

		default:
			break;
		}

		if (valid_mark_item(m, place) == NO) {
			char *placename;
			switch (place) {
			case PL_ABOVE:
				placename = "above";
				break;
			case PL_BELOW:
				placename = "below";
				break;
			case PL_BETWEEN:
				placename = "between";
				break;
			default:
				pfatal("chk_order: invalid place %d", place);
				/* not reached; it just avoids bogus
				 * "used before set" warning */
				placename = "";
			}
			l_warning(Curr_filename, yylineno,
					"%s not valid in %sorder list",
					markname(m), placename);
		}
	}
}


/* return YES if MK_* item is valid at given place, NO if not */

static int
valid_mark_item(mark, place)

int mark;	/* MK_* */
int place;	/* PL_* */

{
	if (mark == MK_OCTAVE && place == PL_BETWEEN) {
		return(NO);
	}
	if ((mark == MK_ENDING || mark == MK_REHEARSAL)
						&& place != PL_ABOVE) {
		return(NO);
	}
	if (mark == MK_PEDAL && place != PL_BELOW) {
		return(NO);
	}

	/* everything else is okay */
	return(YES);
}


/*
 * User can specify that only a portion of the song is to be processed.
 * This is done via one or two numbers, the first measure to include and
 * the last.
 * Positive numbers are relative to the beginning of the song, negative
 * are relative to the end. So, as examples:
 *	1	// the whole song (default)
 *	1,-1	// another way to say the whole song
 *	5	// start at measure 5, through the end
 *	6,7	// only measures 6 and 7
 *	1,10	// measures 1 through 10
 *	1,-8	// skip the last 7 measures
 *	-12,-3	// only process from 12 measures from the end through
 *		// the third from the end
 *
 * When counting measures for this, invisbar bars do not count.
 * It is measured by the number of bars encountered in input, not in
 * performance: the bars are not double counted in sections
 * between repeat signs. 
 *
 * A value of zero is not allowed.
 * A positive start larger than the number of measures in the song
 *	is a user error.
 * A negative start that would result in starting before the beginning
 *	starts at the beginning (with a warning)
 * A positive end larger than the number of measures in the song goes to
 *	end of song (with a warning)
 * An end value that would result in starting before the beginning of the
 *	song or before the start value is a user error.
 *
 * Only one slice is supported. I.e., you can't ask for something
 * like measures 4-10, 17-24, and 46-80.
 * You can only ask for one of those ranges.
 * (You could get that effect by making 3 files and playing them one after
 * another, although there might be slight pauses in between.)
 *
 * A possible future enhancement might be
 * to also be able to specify by rehearsal mark.
 * If a rehearsal mark string is specified rather than a number,
 * the rehearsal mark  having that string (ASCII-ized by removing font, size,
 * and other special things) would be used as the marked place. In this case,
 * the end place would be only up to the rehearsal mark, not through the
 * measure that starts there.
 */

void
chk_x_arg(x_arg, start_p, end_p)

char *x_arg;	/* arg to -x option specified by user */
int *start_p;	/* start gets returned here */
int *end_p;	/* end gets returned here */

{
	char *arg_p;	/* pointer to where end starts in x_arg */

	/* set to defaults */
	*start_p = 1;
	*end_p = -1;

	if (x_arg == 0 || *x_arg == '\0') {
		/* No -x option, use whole song as normal */
		return;
	}

	*start_p = (int) strtol(x_arg, &arg_p, 0);
	if (arg_p == x_arg) {
		if (Mupmate == YES) {
			l_yyerror(0, -1, "Run > Set Options > Extract Measures: value must be one or two numbers.");
		}
		else {
			l_yyerror(0, -1, "argument for %cx option must be one or two numbers", Optch);
		}
	}

	/* If there is a comma, get the "end" argument as well */
	if (*arg_p == ',') {
		*end_p = (int) strtol(arg_p + 1, &arg_p, 0);
	}

	/* We should be at end of string, either after first arg if there
	 * was only one arg, or after second if there were two. */
	if (*arg_p != '\0') {
		if (Mupmate == YES) {
			l_yyerror(0, -1, "Run > Set Options > Extract Measures: value must be one or two numbers.");
		}
		else {
			l_yyerror(0, -1, "argument for %cx option must be one or two numbers", Optch);
		}
	}
}


/* This function does the slicing to extract selected measures from the input */

void
extract(start, end)

int start;	/* Start at this measure number.  A negative
		 * number means count from the end of the piece,
		 * so -3 would mean the last 3 bars. */
int end;	/* Play only through this measure number.
		 * Negative is relative to the end of the piece. */

{
	int pickup;				/* YES if song begins
						 * with pickup measure */
	int numbars;				/* total number of bars */
	int bars;				/* how many processed so far */
	int mrbars;				/* how many bars of multirest */
	struct MAINLL *topstaff_mll_p = 0;	/* "all" MIDI STUFFS will
						 * be attached here */
	struct MAINLL *most_recent_multi_p = 0; /* pointer to most recent
						 * multi-rest, if any */
	struct MAINLL *mll_p;			/* loop through list */
	struct MAINLL *m_p;			/* to look ahead in list */
	struct MAINLL *next_p;			/* saved next */
	struct STUFF *nextstuff_p;		/* saved next of a STUFF */
	struct MAINLL *first_p;			/* first STAFF at start */
	struct STUFF *stuff_p;			/* loop through STUFF list */
	struct STUFF *pedal[MAXSTAFFS+1];	/* YES if pedal is down */
	int in_endings;				/* YES if inside endings */
	int i;					/* index */


	pickup = has_pickup();
	if ( ( (pickup == YES && start == 0) || (pickup == NO && start == 1) )
			&& end == -1) {
		/* Use whole song as normal; nothing to do here */
		return;
	}

	/* If song has a pickup measure, compensate for that.
	 * This function treats the partial measure as a measure.
	 * So if user specified 0 for start, they want to start at the pickup,
	 * which will be effectively measure 1 here. If they specified 1,
	 * they want to skip the pickup, which means they want to start at
	 * what will be considered measure 2. And so forth.
	 */
	if (pickup == YES) {
		if (start >= 0) {
			start++;
		}
		if (end >= 0) {
			end++;
		}
	}
	else {
		/* It's not clear if these should be warnings or errors,
		 * but it seems friendlier to be just warnings.
		 * That way, if someone wants the beginning,
		 * but can't remember if the piece has a pickup or not,
		 * they can use 0, and it will always work,
		 * albeit possibly with a warning.
		 */
		if (start == 0) {
			warning("x option start of 0 is only valid if there is a pickup measure; using 1");
			start = 1;
		}
		if (end == 0) {
			warning("x option end of 0 is only valid if there is a pickup measure; using 1");
			end = 1;
		}
	}

	/* Count total number of bars in song */
	for (numbars = 0, mll_p= Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		if (mll_p->str == S_BAR && mll_p->u.bar_p->bartype != INVISBAR) {
			numbars++;
		}
		/* Multirests count as multiple bars */
		else if (mll_p->str == S_STAFF
				&& mll_p->u.staff_p->groups_p[0] != 0) {
			if (mll_p->u.staff_p->groups_p[0]->is_multirest == YES) {
				numbars += -(mll_p->u.staff_p->groups_p[0]->basictime) - 1 ;
			}
			/* skip the rest of the STAFFs till BAR */
			while (mll_p->next != 0 && mll_p->next->str == S_STAFF) {
				mll_p = mll_p->next;
			}
		}
	}
	if (numbars == 0) {
		/* Only non-invisible bars count. If there aren't any
		 * identifiable measures, -x is pointless. */
		ufatal("can't use -x on song with no visible bar lines");
	}

	/* If user specified things relative to the end, convert
	 * the relative-to-end negative values to relative-to-beginning
	 * positive values.
	 */
	if (start < 0) {
		start = numbars + start + 1;
	}
	if (end < 0) {
		end = numbars + end + 1;
	}

	if (start > numbars) {
		ufatal("Attempt to start beyond end of song");
	}
	if (end < start) {
		ufatal("Attempt to end before start");
	}
	if (end <= 0) {
		ufatal("Attempt to end before beginning of song");
	}
	if (start < 1) {
		warning("attempt to start before beginning; ignoring");
		start = 1;
	}
	if (end > numbars) {
		warning("attempt to go past end; ignoring");
		end = numbars;
	}

	if (start == 1 && end == numbars) {
		/* After all the conversions, we ended up with
		 * the entire song. Nothing more to do here. */
		return;
	}
	/* compensate for bar being at end of measure */
	start--;


	/* First find the bar where we're going to start.
	 * Find out if there are any notes tied into that measure
	 * that have an accidental. If so, move the accidental
	 * into the new starting measure.
	 * Note: the 'mll_p != 0' checks are defensive; shouldn't happen.
	 */
	initstructs();
	mrbars = 0;
	for (bars = 0, mll_p = Mainllhc_p; mll_p != 0 && bars < start;
							mll_p = mll_p->next){
		if (mll_p->str == S_SSV) {
			/* need to keep things like keysig up to date */
			asgnssv(mll_p->u.ssv_p);
		}
		else if (mll_p->str == S_BAR && mll_p->u.bar_p->bartype != INVISBAR) {
			bars++;
		}
		else if (mll_p->str == S_STAFF
				&& mll_p->u.staff_p->groups_p[0] != 0) {
			if (mll_p->u.staff_p->groups_p[0]->is_multirest == YES) {
				bars += -(mll_p->u.staff_p->groups_p[0]->basictime) - 1 ;
			}
			/* skip the rest of the STAFFs till BAR */
			while (mll_p->next->str == S_STAFF) {
				mll_p = mll_p->next;
			}
		}
	}
	first_p = 0;
	for (  ; mll_p != 0 && mll_p->str != S_BAR; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			if (mll_p->u.staff_p->groups_p[0]->is_multirest == YES) {
				mrbars = -(mll_p->u.staff_p->groups_p[0]->basictime) - 1 ;
			}
			mv_accs(mll_p);
			if (first_p == 0) {
				first_p = mll_p;
			}
		}
	}
	/* Fix up the measure number to account for what was chopped off.
	 * The pickup part may be a bit counter-intuitive...
	 * It's because when there was a pickup, we've already added 1 to
	 * "start" for that partial measure; with no pickup, we didn't.
	 * As an example, if user specified -x2, then the measure number
	 * at the barline at the end of the first printed measure should be 3.
	 * "start" will have value 1 if there was no pickup,
	 * but 2 if there was a pickup. So....
	 *
	 *			start + 1 + (pickup ? 0 : 1);
	 *			-----------------------------
	 *    no pickup:          1   + 1 +               1     = 3
	 *    pickup:             2   + 1 +           0         = 3
	 */
	Meas_num = mll_p->u.bar_p->mnum = bars + mrbars + 1 + (pickup ? 0 : 1);

	/* Find the top visible staff on the new effective "first" measure,
	 * We will be moving MIDI STUFFs into the special "extra"
	 * space measure that exists for MIDI right before the first real
	 * measure entered by the user.
	 * The current top visible tells us which staff to use for "all" items.
	 * We leave everything on the list up through the "extra" measure */
	if (Doing_MIDI) {
		int staffs_needed;	/* number of staffs at new first meas */
		struct MAINLL *laststaffmll_p = 0;	/* last staff of the extra meas
			 * added at the beginning for MIDI.
			 * Initialization just to avoid "used before set." */

		for (mll_p = Mainllhc_p; mll_p->str != S_BAR; mll_p = mll_p->next) {
			if (mll_p->str == S_STAFF && svpath(
					mll_p->u.staff_p->staffno, VISIBLE)
					->visible == YES) {
				topstaff_mll_p = mll_p;
				break;
			}
		}

		/* The number of staffs in the "extra" added measure might be
		 * smaller than the number of staffs on the new
		 * effective first measure.
		 */
		staffs_needed = Score.staffs;
		for (mll_p = Mainllhc_p; mll_p->str != S_BAR; mll_p = mll_p->next){
			if (mll_p->str == S_STAFF) {
				laststaffmll_p = mll_p;
			}
		}
		if (laststaffmll_p == 0){
			pfatal("extract failed to find last staff in extra measure");
		}
		if (laststaffmll_p->u.staff_p->staffno < staffs_needed) {
			(void) add_pre_meas(laststaffmll_p,
					laststaffmll_p->u.staff_p->staffno,
					staffs_needed, NO);
		}

		/* We want to start discarding things after the extra measure,
		 * so skip past that. */
		for (mll_p = Mainllhc_p; mll_p->str != S_BAR; mll_p = mll_p->next) {
			;
		}
		mll_p = mll_p->next;
	}
	else {
		mll_p = Mainllhc_p;
	}

	/* Pedal is off at the start */
	for (i = 1; i <= MAXSTAFFS; i++) {
		pedal[i] = 0;
	}

	/* Now go through and discard anything irrelevant before the
	 * start measure. We save all SSVs. If doing MIDI, we save any
	 * MIDI directives that matter.
	 * Everything else gets discarded.
	 */
	mrbars = 0;
	next_p = 0;
	in_endings = NO;
	/* In case the very first thing in the input is a multirest, we might
	 * have to go back to that, and it will not have a bar line before it,
	 * so we would need to use the beginning of the main list. */
	most_recent_multi_p = Mainllhc_p;
	for (bars = 0; bars < start; mll_p = next_p) {
		if (mll_p == 0) {
			pfatal("got null mll_p when finding starting measure");
		}
		next_p = mll_p->next;

		switch (mll_p->str) {

		case S_BAR:
			if (mll_p->u.bar_p->bartype != INVISBAR) {
				bars++;
			}
			if (mrbars > 0) {
				bars += mrbars - 1;
				if (bars > start || (mll_p->u.bar_p->bartype
						== INVISBAR && bars == start)) {
					/* New first bar is or was a multirest,
					 * so we need to retain this bar line.
					 */
					continue;
				}
			}
			else {
				most_recent_multi_p = 0;
			}
			mrbars = 0;

			/* Keep track of if we're in a first ending. */
			if (mll_p->u.bar_p->endingloc == STARTITEM) {
				in_endings = YES;
			}
			else if (mll_p->u.bar_p->endingloc == ENDITEM) {
				in_endings = NO;
			}
			break;

		case S_STAFF:
			/* Keep track of current pedal state for this staff.
			 * If we find a C_ENDPED the pedal is up, otherwise
			 * it's down (because even with a bounce, at the end
			 * of the bounce, the pedal is down).
			 * Also deal with an octave marks that might spill
			 * over into the new first measure.
			 */
			for (stuff_p = mll_p->u.staff_p->stuff_p; stuff_p != 0;
						stuff_p = nextstuff_p) {
				/* In a previous implementation,
				 * stuff_p->next could be invalid after call
				 * to move_xoct, so we had to save it here.
				 * With the current implementation, this
				 * shouldn't be strictly necessary, but
				 * it doesn't hurt. */
				nextstuff_p = stuff_p->next;

				/* Note that the only time string should be
				 * null is on pedal carried over from a
				 * previous score, so that isn't really a
				 * change in pedal state, so we can ignore it.
				 */
				if (stuff_p->stuff_type == ST_PEDAL &&
							stuff_p->string != 0) {
					pedal[mll_p->u.staff_p->staffno] =
								stuff_p;
				}
				if (stuff_p->stuff_type == ST_OCTAVE) {
					move_xoct(stuff_p, first_p,
						mll_p->u.staff_p->staffno,
						bars, start);
				}
			}

			if (Doing_MIDI) {
				mv_midi_items(mll_p, topstaff_mll_p);
			}

			/* Deal with multirest */
			if (mll_p->u.staff_p->groups_p[0]->is_multirest == YES) {
				mrbars = -(mll_p->u.staff_p->groups_p[0]->basictime);
				if (bars + mrbars > start) {
					/* Slice starts in middle of multirest.
					 * This multirest will be the new
					 * first measure, but we need
					 * to adjust the number of
					 * measures worth of multirest to
					 * account for starting in the middle
					 * of it.
					 */
				    mll_p->u.staff_p->groups_p[0]->basictime =
						-((bars + mrbars) - start);

				    /* If the end of the slice is also inside
				     * this same multirest, shorten it down
				     * to end there. It's slightly silly to
				     * be saving only a part of a multirest,
				     * but it's better to handle it than
				     * to core dump...
				     */
				   if (bars + mrbars > end) {
					mll_p->u.staff_p->groups_p[0]->basictime
						+= (end - bars - mrbars + 2);
				   }

				    if (most_recent_multi_p == 0) {
					most_recent_multi_p = mll_p;
				    }
				    /* If down to single measure, have to
				     * convert from multirest to meas rest */
				    if (mll_p->u.staff_p->groups_p[0]->basictime == -1) {
					mll_p->u.staff_p->groups_p[0]->is_multirest = NO;
					mll_p->u.staff_p->groups_p[0]->is_meas = YES;
					mll_p->u.staff_p->groups_p[0]->basictime = 1;
					mll_p->u.staff_p->groups_p[0]->fulltime = Score.time;
				    }
				    continue;
				}
			}
			break;

		case S_SSV:
			/* Need to keep all the SSVs. Not sure they
			 * really need to be assigned, but shouldn't hurt. */
			asgnssv(mll_p->u.ssv_p);
			continue;

		default:
			break;

		}

		/* Get rid of this MAINLL. User wants it skipped. */
		unlinkMAINLL(mll_p);
	}

	/* If the slice begins inside a multirest, the first bar line
	 * that we are at is actually later than the first measure,
	 * so we have to use where the multirest began.
	 * We saved that away earlier for this purpose. */
	if (most_recent_multi_p != 0) {
		mll_p = most_recent_multi_p;
	}

	/* Make sure there is a bar after where mll_p currently is.
	 * If song consists of just a multirest, followed by
	 * blocks, prints, lines, or curves, but no more music,
	 * we need to force mll_p to get set to the bar at the end of
	 * the multirest.
	 */
	for (m_p = mll_p; m_p != 0 && m_p->str != S_BAR; m_p = m_p->next) {
		;
	}
	if (m_p == 0) {
		/* No bar.  Force into hitting the next 'if' */
		mll_p = 0;
	}

	/* If start was in a multirest, mll_p could be zero,
	 * so we have to use the beginning bar. */
	if (mll_p == 0) {
		for (mll_p = Mainllhc_p; mll_p != 0 && mll_p->str != S_BAR;
							mll_p = mll_p->next) {
			;
		}
		if (mll_p == 0) {
			pfatal("-x option failed to find beginning bar correctly");
		}
	}

	/* Add pedal starts if necessary. Go through the new effective
	 * first measure. For any staff that had the pedal down coming
	 * into that measure, make sure the measure starts with a BEGPED.
	 * Also, if there are any measure repeats, ufatal.
	 * Trying to start at a measure repeat doesn't really make a lot of
	 * sense, and it would be work to allow it, so don't bother.
	 * Using mrpt is discouraged anyway.
	 */
	for ( ; mll_p->str != S_BAR; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			int v;		/* voice index */
			/* check for mrpt */
			for (v = 0; v < MAXVOICES; v++) {
				if (is_mrpt(mll_p->u.staff_p->groups_p[v]) == YES) {
					l_ufatal(mll_p->inputfile, mll_p->inputlineno,
					"-x option not allowed to start at a mrpt");
				}
			}

			addped(pedal[mll_p->u.staff_p->staffno], mll_p);
		}	
	}

	/* If slice starts inside an ending, we disallow that.
	 * It turns out there a number of pathological cases,
	 * particularly regarding pedal. If the pedal was down going into
	 * the first ending, but up at the beginning of the slice, then
	 * what should the pedal state be at the start of subsequent endings?
	 * It should normally be based on the state coming into the ending,
	 * but that no longer exists and conflicts with the new "beginning
	 * of the ending" state. And more fundamentally, where do you go back
	 * to when you reach the end of a non-final ending?
	 * There's nothing there to go back to!
	 * I suppose if someone has a really long ending and wants
	 * just the middle of it, this restriction will be annoying,
	 * but that shouldn't happen too often, and trying to handle this
	 * was just getting too complicated!
	 */
	if (in_endings == YES) {
		ufatal("-x section not allowed to begin inside an ending");
	}

	/* If the new first measure is in the middle of an ending,
	 * patch up the endingloc. */
	if (mll_p->u.bar_p->endingloc == ENDITEM ||
					mll_p->u.bar_p->endingloc == INITEM) {
		struct MAINLL *prevmll_p;
		struct MAINLL *topstaffmll_p = 0;

		for (prevmll_p = mll_p->prev; prevmll_p != 0;
						prevmll_p = prevmll_p->prev) {
			if (prevmll_p->str == S_STAFF) {
				topstaffmll_p = prevmll_p;
			}
			else if (prevmll_p->str == S_BAR) {
				prevmll_p->u.bar_p->endingloc = STARTITEM;
				break;
			}
		}
		if (prevmll_p == 0) {
			if (topstaffmll_p == 0) {
				pfatal("unexpected null topstaffmll_p");
			}
			prevmll_p = add_pre_meas(
				topstaffmll_p->prev, 1, Score.staffs, YES);
			prevmll_p->u.bar_p->endingloc = STARTITEM;
		}
	}

	/* Now chop off end if needed */
	if (end < numbars) {
		for (  ; mll_p != 0 && (bars < end || mrbars > 0);
							mll_p = mll_p->next) {
			if (mll_p->str == S_BAR && mll_p->u.bar_p->bartype != INVISBAR) {
				bars++;
				mrbars = 0;
				if (bars == end) {
					break;
				}
			}
			if (mll_p->str == S_STAFF &&
					mll_p->u.staff_p->groups_p[0] != 0 &&
					mll_p->u.staff_p->groups_p[0]->is_multirest == YES) {
				/* It's a multirest */
				mrbars = -(mll_p->u.staff_p->groups_p[0]->basictime);
				bars += mrbars - 1;
				if (bars >= end) {
					/* Slice ends inside the multirest.
					 * Adjust its length. If down to a
					 * single measure, convert */
					for (  ; mll_p->str == S_STAFF;
							mll_p = mll_p->next) {
					    mll_p->u.staff_p->groups_p[0]->basictime
							+= bars - end + 1;
					    if (mll_p->u.staff_p->groups_p[0]->basictime == -1) {
						mll_p->u.staff_p->groups_p[0]->is_multirest = NO;
						mll_p->u.staff_p->groups_p[0]->is_meas = YES;
						mll_p->u.staff_p->groups_p[0]->basictime = 1;
						mll_p->u.staff_p->groups_p[0]->fulltime = Score.time;
					    }
					}
					bars -= bars - end;
					mrbars = 0;
				}
				/* multi-rest has been handled; skip over
				 * the rest of the staffs in this measure */
				while (mll_p->next != 0 &&
						mll_p->next->str == S_STAFF) {
					mll_p = mll_p->next;
				}
			}
		}
		if (mll_p != 0) {
			/* If not at the end of the list, we need to be at
			 * a BAR, but there are a few cases where we might
			 * not be, like if ending in the middle of a multirest
			 * followed by an invisbar. So fix those,
			 * by going to just beyond the next bar */
			for (  ; mll_p != 0; mll_p = mll_p->next) {
				if (mll_p->str == S_BAR) {
					mll_p = mll_p->next;
					break;
				}
			}

			/* Peek ahead. If the next thing is a score feed,
			 * and it contains a margin override, then probably
			 * the user wanted that override to still be used.
			 */
			if (mll_p != 0 && mll_p->str == S_FEED) {
				mll_p = mll_p->next;
			}
			/* If didn't get all the way to end of main list,
			 * chop off the rest.
			 * We don't bother to free the space.
			 */
			if (mll_p != 0) {
				mll_p->prev->next = 0;
				Mainlltc_p = mll_p->prev;
			}
		}

		/* Remove ties/slurs going into the chopped-off part. */
		for (mll_p = Mainlltc_p->prev; mll_p != 0; mll_p = mll_p->prev) {
			if (mll_p->str == S_STAFF) {
				int v;		/* voice index */
				for (v = 0; v < MAXVOICES; v++) {
					struct GRPSYL *g_p;
					int n;		/* note index */
					if (mll_p->u.staff_p->groups_p[v] == 0) {
						/* this voice doesn't exist */
						continue;
					}
					/* find last group in this voice */
					for (g_p = mll_p->u.staff_p->groups_p[v];
							g_p->next != 0;
							g_p = g_p->next) {
						;
					} 
					/* only notes can have tie/slur */
					if (g_p->grpcont != GC_NOTES) {
						continue;
					}
					g_p->tie = NO;
					for (n = 0; n < g_p->nnotes; n++) {
						g_p->notelist[n].tie = NO;
						if (g_p->notelist[n].nslurto > 0) {
							g_p->notelist[n].nslurto = 0;
							FREE(g_p->notelist[n].slurtolist);
						}
					}
				}

				if (mll_p->u.staff_p->staffno == 1) {
					/* No more staffs in the final bar */
					break;
				}
			}
		}

		/* If end is in the middle of an ending, or with a restart,
		 * fix that */
		for (mll_p = Mainlltc_p; mll_p != 0; mll_p = mll_p->prev) {
			if (mll_p->str == S_BAR) {
				if (mll_p->u.bar_p->endingloc == INITEM) {
					mll_p->u.bar_p->endingloc = ENDITEM;
				}
				else if (mll_p->u.bar_p->endingloc == STARTITEM) {
					struct MAINLL *pmll_p;

					/* If there is a previous bar and it's
					 * in an ending, we need to change this
					 * one to an ENDITEM, else to NOITEM.
					 */
					mll_p->u.bar_p->endingloc = NOITEM;
					for (pmll_p = mll_p->prev; pmll_p != 0;
							pmll_p = pmll_p->prev) {
						if (pmll_p->str == S_BAR) {
							if (pmll_p->u.bar_p->endingloc == INITEM ||
							pmll_p->u.bar_p->endingloc == STARTITEM) {
								mll_p->u.bar_p->endingloc = ENDITEM;
							}
							break;
						}
					}
				}
				if (mll_p->u.bar_p->bartype == RESTART) {
					/* Not exactly clear what to do here.
					 * Could ufatal. We'll convert to
					 * an invisible bar. */
					mll_p->u.bar_p->bartype = INVISBAR;
				}
				break;
			}
		}
		
		/* Note: it shouldn't be necessary to do anything
		 * about location tags in chopped-off areas.
		 * Locvar code runs much later, so doesn't know
		 * about what was deleted, and that code has to be able
		 * to handle tags pointing to invisible things anyway
		 * for other reasons. Similarly, it is not necessary to
		 * shorten til clauses, since any that spill into the
		 * chopped-off part will still be handled okay, since a
		 * staff could be made invisible in the middle of a til
		 * clause, and this is a similar case.
		 */
	}
}


/* When using -x option, if there is a tie across the beginning split point,
 * we need to check if there was an accidental on the note being tied from,
 * and if so, move the accidental to the new effective first measure.
 */

static void
mv_accs(mll_p)

struct MAINLL *mll_p;

{
	int v;			/* voice index */
	int n;			/* note index */
	int staffno;
	int ea;			/* effective accidental */
	struct GRPSYL *gs_p;
	struct NOTE *note_p;

	staffno = mll_p->u.staff_p->staffno;
	for (v = 0; v < MAXVOICES; v++) {
		if ((gs_p = mll_p->u.staff_p->groups_p[v]) == 0) {
			/* voice doesn't exist on this staff */
			continue;
		}
		/* Only non-grace note groups can be tied to */
		if (gs_p->grpcont == GC_NOTES && gs_p->grpvalue != GV_ZERO) {
			/* check all notes for being tied to */
			for (n = 0; n < gs_p->nnotes; n++) {
				note_p = &(gs_p->notelist[n]);
				/* Only need to check if doesn't already
				 * have an accidental. If effective accidental
				 * differs from what note would get from	
				 * key sig, need to add explicit accidental. */
				if (standard_acc(note_p->acclist) == '\0') {
					ea = eff_acc(gs_p, note_p, mll_p);
					if (ea != acc_from_keysig(
					  note_p->letter, staffno, mll_p)) {
						standard_to_acclist(Acclets[ea+2], note_p->acclist);
					}
				}
			}
		}
	}
}


/* If there is an octave mark whose til clause goes into the new
 * effective first measure due to -x option, we need to move that octave mark
 * to that new first measure, with its til clause appropriately shortened.
 */

static void
move_xoct(stuff_p, newfirst_p, staffno, bars, start)

struct STUFF *stuff_p;		/* the STUFF to potentially move */
struct MAINLL *newfirst_p;	/* points to first STAFF in new first meas */
int staffno;			/* which STAFF this STUFF is for */
int bars;			/* how many bars into song the STUFF is */
int start;			/* start bar number from -x option */

{
	struct STAFF *staff_p;	/* where to move the STUFF */
	struct MAINLL *mll_p;	/* to search for proper STAFF */
	struct STUFF *newstuff_p;	/* moved STUFF */


	/* See if this STUFF is an octave that spills into new first measure */
	if (stuff_p->stuff_type == ST_OCTAVE &&
					bars + stuff_p->end.bars >= start) {
		/* Make a new STUFF, adjusting length of til clause
		 * to compensate for -x, and starting at count 1 */
		newstuff_p = newSTUFF(stuff_p->string,
					stuff_p->dist,
					stuff_p->dist_usage,
					NOALIGNTAG,
					1.0, 0.0, 0,
					bars + stuff_p->end.bars - start,
					stuff_p->end.count,
					stuff_p->end.steps,
					stuff_p->end.gracebackup,
					ST_OCTAVE,
					stuff_p->modifier,
					stuff_p->place,
					stuff_p->inputfile,
					stuff_p->inputlineno);

		/* Find corresponding STAFF in new first measure */
		staff_p = 0;
		for (mll_p = newfirst_p; mll_p != 0 && mll_p->str == S_STAFF;
							mll_p = mll_p->next) {
			if (mll_p->u.staff_p->staffno == staffno) {
				staff_p = mll_p->u.staff_p;
				break;
			}
		}
		if (staff_p == 0) {
			/* Staff apparently doesn't exist here,
			 * so we can ignore the octave mark. */
			return;
		}

		/* Make the octave stuff into a 1-element stufflist,
		 * and call the function to merge that list with the
		 * existing list. */
		newstuff_p->next = 0;
		connect_stuff(staff_p, newstuff_p);
	}
}


/* If the passed-in pedal_p points to a pedal stuff that indicates the
 * pedal state needs to be altered at the beginning of the STAFF passed in,
 * then do that. This is for -x, for like when the slice begins where the
 * pedal was held down from the previous measure, so we have to add a begped.
 */

static void
addped(pedal_p, mll_p)

struct STUFF *pedal_p;	/* last pedal state of previous measures */
struct MAINLL *mll_p;	/* points to a STAFF, to add pedal to */

{
	struct STUFF *stuff_p;	/* walk through stuff list */
	struct STUFF **prev_p_p; /* This holds the address
				 * of the previous' next,
				 * which is where we will need
				 * to update if we need to delete
				 * or insert a STUFF.
				 */

	if (pedal_p == 0 || pedal_p->string == 0) {
		/* There is no pedal carrying over into this measure,
		 * so no need to add anything. */
		return;
	}

	/* If last pedal mark was an endped, we don't need to move one in,
	 * because pedal is off; otherwise we do. */
	if (pedal_p->string[4] != C_ENDPED) {

		/* Need to add a pedal STUFF at count 1 if
		 * there isn't already one at or before there.
		 * See if there's already one there.
		 */
		for (stuff_p = mll_p->u.staff_p->stuff_p,
				prev_p_p = &(mll_p->u.staff_p->stuff_p);
				stuff_p != 0;
				prev_p_p = &(stuff_p->next),
				stuff_p = stuff_p->next) {
			if (stuff_p->stuff_type == ST_PEDAL &&
					stuff_p->start.count
					<= 1.0) {
				/* Already had a pedal at beginning of measure.
				 * If it's an ENDPED,
				 * that negates the one coming in,
				 * and it ought to be removed.
				 * If it's a PEDAL, need to change to a BEGPED.
				 * In any case, we don't need
				 * to check any more STUFFs for this STAFF.
				 */
				if (stuff_p->string[4] == C_ENDPED) {
					/* delete it */
					*prev_p_p = stuff_p->next;
				}
				else if (stuff_p->string[4] == C_PEDAL) {
					stuff_p->string[4] = C_BEGPED;
				}
				break;
			}
		}
		if (stuff_p == 0) {
			/* There wasn't a pedal at the
			 * beginning of the new first measure,
			 * so we will need to add one.
			 */
			struct STUFF *newped_p;

			newped_p = newSTUFF(pedal_p->string,
					pedal_p->dist,
					pedal_p->dist_usage,
					NOALIGNTAG,
					1.0, 0.0, 0, 0, 0.0, 0.0, 0,
					ST_PEDAL,
					pedal_p->modifier,
					pedal_p->place,
					pedal_p->inputfile,
					pedal_p->inputlineno);
			newped_p->string[4] = C_BEGPED;

			/* figure out where to insert it */
			for (stuff_p = mll_p->u.staff_p->stuff_p,
					prev_p_p = &(mll_p->u.staff_p->stuff_p);
					stuff_p != 0;
					prev_p_p = &(stuff_p->next),
					stuff_p = stuff_p->next) {
				if (stuff_p->place == PL_ABOVE) {
					/* not far enough yet */
					continue;
				}
				if (pedal_p->all == YES && stuff_p->all == NO){
					continue;
				}
				if (stuff_p->start.count < 1.0) {
					continue;
				}
				/* found right place */
				break;
			}
			if (prev_p_p == 0) {
				pfatal("failed to find place to insert pedal for -x");
			}

			/* insert into STUFF list */
			newped_p->next = *prev_p_p;
			*prev_p_p = newped_p;
		}
	}
}


/* This function returns what Firstpageside should be set to.
 * If the user set it via -p, that overrules any SSV settings.
 * Explictly setting to right and also setting panelsperpage == 2
 * yields a warning and is forced to left.
 * This also populates the left and right headers from the non-sided
 * version, if any, if they weren't set explicitly.
 */

int
set_firstpageside(p_option_side)

int p_option_side;	/* PGSIDE_* value set via -p option (or PGSIDE_NOT_SET
			 * if user didn't set it there */

{
	struct MAINLL *mll_p;	/* to step through main list */
	int side;		/* user's final setting of firstpage side */
	int panels_per_page;	/* user's final setting of panelsperpage */


	/* Set to defaults, in case user doesn't set at all */
	side = PGSIDE_NOT_SET;
	panels_per_page = DEFPANELSPERPAGE;

	/* Walk though main list up to the first music. */
	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			/* Can't change panelsperpage or firstpage
			 * after this, so no need to check any farther. */
			break;
		}

		if (mll_p->str != S_SSV) {
			/* We only care about SSVs here */
			continue;
		}

		/* Remember the user's final setting of firstside */
		if (mll_p->u.ssv_p->used[FIRSTPAGE] == YES &&
				mll_p->u.ssv_p->firstside != PGSIDE_NOT_SET) {
			side = mll_p->u.ssv_p->firstside;
		}
		/* Remember the user's final setting of panelsperpage */
		if (mll_p->u.ssv_p->used[PANELSPERPAGE] == YES) {
			panels_per_page = mll_p->u.ssv_p->panelsperpage;
		}
	}

	/* If side was specified via -p command line option, that overrules */
	if (p_option_side != PGSIDE_NOT_SET) {
		side = p_option_side;
	}

	/* If after we take all the user's settings into account,
	 * they specified right page with panelsperpage of 2, give warning,
	 * and force to left, because by definition,
	 * the first page is left in that case.
	 */
	if (side == PGSIDE_RIGHT && panels_per_page == 2) {
		warning("when panelsperpage is 2, firstpage side must be leftpage; ignoring setting to rightpage");
		side = PGSIDE_LEFT;
	}

	/* If user never set to anything, default is right. */
	if (side == PGSIDE_NOT_SET) {
		side = PGSIDE_RIGHT;
	}

	/* For any left/right headfoots that the user didn't supply
	 * if the user did provide a corresponding non-sided headfoot.
	 * copy that. */
	set_leftright(GOT_HEADER, GOT_LHEADER, GOT_RHEADER,
			&Header, &Leftheader, &Rightheader);
	set_leftright(GOT_FOOTER, GOT_LFOOTER, GOT_RFOOTER,
			&Footer, &Leftfooter, &Rightfooter);
	set_leftright(GOT_HEADER2, GOT_LHEADER2, GOT_RHEADER2,
			&Header2, &Leftheader2, &Rightheader2);
	set_leftright(GOT_FOOTER2, GOT_LFOOTER2, GOT_RFOOTER2,
			&Footer2, &Leftfooter2, &Rightfooter2);

	return(side);
}


/* If didn't get either left or right of something,
 * but did get a non-sided one, then copy the non-sided one.
 * That way placement and print code can always just use left/right,
 * and not have to care if it was explicitly that, or defaulted.
 */

static void
set_leftright(flag_nonsided, flag_left, flag_right, nonsided_p, left_p, right_p)

int flag_nonsided;	/* GOT_* of which nonsided to use */
int flag_left;		/* GOT_* of which left headfoot to use */
int flag_right;		/* GOT_* of which right headfoot to use */
struct BLOCKHEAD *nonsided_p;	/* address of Header/Footer/Header2/Footer2 */
struct BLOCKHEAD *left_p;	/* address of Leftheader/Leftfooter, etc */
struct BLOCKHEAD *right_p;	/* address of Rightheader/Rightfooter, etc */

{
	if ( (Gotheadfoot & flag_nonsided) == 0) {
		/* No non-sided one to copy */
		return;
	}

	if ( (Gotheadfoot & flag_left) == 0) {
		memcpy(left_p, nonsided_p, sizeof(struct BLOCKHEAD));
	}
	if ( (Gotheadfoot & flag_right) == 0) {
		memcpy(right_p, nonsided_p, sizeof(struct BLOCKHEAD));
	}
}


/* Loop through the main list, processing multi measure repeats.
 * This is mostly doing error checks */

void
set_mrpt_info()

{
	struct MAINLL *mll_p;


	/* Loop through main list looking for multi-rpts */
	initstructs();
	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			if (is_unprocessed_mrpt(mll_p) == YES) {
				if (proc_multrpt(mll_p) == YES) {
					if (Doing_MIDI == YES) {
						do_midi_mrpt_copy(mll_p);
					}
				}
			}
		}
		else if (mll_p->str == S_SSV) {
			asgnssv(mll_p->u.ssv_p);
		}
	}
}


/* In a measure with a mult rpt, we want all voices to be set to that,
 * even if some are currently space. This function does that. */

static void
set_mrpt(staff_p, mrpt_type, count)

struct STAFF *staff_p;
int mrpt_type;		/* MRT_* */
int count;		/* which measure into the mult rpt this is */

{
	int v;

	for (v = 0; v < MAXVOICES; v++) {
		if (staff_p->groups_p[v] == 0) {
			break;
		}
		staff_p->groups_p[v]->grpcont = GC_NOTES;
		staff_p->groups_p[v]->grpvalue = GV_NORMAL;
		staff_p->groups_p[v]->nnotes = 0;
		staff_p->groups_p[v]->meas_rpt_type = mrpt_type;
		staff_p->groups_p[v]->is_meas = YES;
		staff_p->groups_p[v]->basictime = -1;
		staff_p->groups_p[v]->fulltime.n = Score.time.n;
		staff_p->groups_p[v]->fulltime.d = Score.time.d;
		staff_p->groups_p[v]->dots = 0;

		/* In case user did something like 2s;; instead of ms,
		 * free any extra GRPSYLS. */
		free_grpsyls(staff_p->groups_p[v]->next);
		staff_p->groups_p[v]->next = 0;
	}
	staff_p->mult_rpt_measnum = count;
}


/* This checks if given STAFF pointed to by the given mll_p
 * is the start of a valid mult rpt (single, double, or quad).
 * If it is, it sets all voices to mrpt,
 * even if they had been space  before, and returns YES.
 * If it isn't the start of a mrpt,
 * or at least not a valid one (not all voices have same mrpt length,
 * or some have non-space), it returns NO.
 */

static int
is_unprocessed_mrpt(mll_p)

struct MAINLL *mll_p;

{
	short hasrpt;		/* bitmap of MRT values found */
	short hasnonspace;	/* YES or NO */
	int v;			/* voice index */
	RATIONAL meas_dur;
	struct STAFF *staff_p;


	staff_p = mll_p->u.staff_p;
	if (staff_p->mult_rpt_measnum != 0) {
		/* This is not the beginning, it is a part of one
		 * that has already been handled when we saw its beginning. */
		return(NO);
	}

	/* See if any of the voices are meas repeats */
	hasrpt = 0;
	hasnonspace = NO;
	meas_dur.n = Score.time.n + 1;
	meas_dur.d = Score.time.d;
	for (v = 0; v < MAXVOICES; v++) {
		if (staff_p->groups_p[v] == 0) {
			/* no more voices */
			break;
		}
		if (staff_p->groups_p[v]->meas_rpt_type != MRT_NONE) {
			/* Note: this depends on MRT_NONE being zero and the
			 * other MRT_* values each being their own bit. */
			hasrpt |= staff_p->groups_p[v]->meas_rpt_type;

			/* This is the first measure of the mrpt */
			staff_p->mult_rpt_measnum = 1;
		}
		else if (hasspace(staff_p->groups_p[v], Zero, meas_dur) == NO) {
			hasnonspace = YES;
		}
	}

	if ( (hasrpt & MRT_SINGLE) && (hasrpt & MRT_DOUBLE) ) {
		l_yyerror(mll_p->inputfile, mll_p->inputlineno,
				"cannot mix mrpt and dbl mrpt on a staff");
		return(NO);
	}
	if ( (hasrpt & MRT_SINGLE) && (hasrpt & MRT_QUAD) ) {
		l_yyerror(mll_p->inputfile, mll_p->inputlineno,
				"cannot mix mrpt and quad mrpt on a staff");
		return(NO);
	}
	if ( (hasrpt & MRT_DOUBLE) && (hasrpt & MRT_QUAD) ) {
		l_yyerror(mll_p->inputfile, mll_p->inputlineno,
				"cannot mix dbl mrpt and quad mrpt on a staff");
		return(NO);
	}
	if (hasnonspace == YES && hasrpt != MRT_NONE) {
		l_yyerror(mll_p->inputfile, mll_p->inputlineno,
				"cannot mix mrpt with nonspace on a staff");
		return(NO);
	}
	if (hasrpt == MRT_NONE) {
		return(NO);
	}
	set_mrpt(staff_p, hasrpt, 1);
	return(YES);

}


/* This maps an MRT_* value to the number of measures that represents.
 * Currently, the MRT_* value is deliberately made to match
 * the number of measures, so it is a trivial mapping,
 * but in case that would ever become untrue some day, having this function
 * should reduce the impact.
 */

static int
mrpt_number(mrpt_type)

int mrpt_type;	/* MRT_* */

{
	return(mrpt_type);
}


/* Given an MRT_* value, return its name, for error messages */

static char *
mrt2name(mrt_type)

int mrt_type;	/* MRT_* */

{
	switch(mrt_type) {
	case MRT_SINGLE:
		return("single");
	case MRT_DOUBLE:
		return("dbl");
	case MRT_QUAD:
		return("quad");
	default:
		pfatal("invalid mrt_type arg %d to mrt2name()", mrt_type);
		/*NOTREACHED*/
		return("invalid");
	}
}


/* Returns YES if the given SSV field is overriden by all voices
 * on the given staff, NO if not. */

static int
voices_override(staff, field)

int staff;	/* check this staff */
int field;	/* check this field */

{
	int numvoices;
	int v;

	numvoices = vscheme_voices(svpath(staff, VSCHEME)->vscheme);
	for (v = 1; v <= numvoices; v++) {
		if (voice_field_used(field, staff, v) == NO) {
			/* this voice does not override */
			return(NO);
		}
	}
	return(YES);
}


/* Returns YES if the SSV pointed to by the given mll_p
 * does not contain setting of the given parameter,
 * which should be a parameter type that is illegal 
 * during a mult rpt (either what it is repeating, or the rpt itself).
 * Returns NO if the SSV contains a problematic change for the staff.
 */

static int
mult_rpt_ssv_ok(ssv_p, staff, param, desc, inputfile, inputlineno)

struct SSV *ssv_p;
int staff;
int param;		/* which parameter to check */
char *desc;		/* name of parameter, for error message */
char *inputfile;
int inputlineno;

{
	int retval = YES; /* assume good till proven otherwise */


	if (ssv_p->used[param] == NO) {
		/* No change to this parameter, so no problem */
		return(YES);
	}
	if (ssv_p->staffno != 0 && ssv_p->staffno != staff) {
		/* staff/voice level change for some other staff; irrelevant */
		return(YES);
	}

	if (ssv_p->staffno == staff) {
		/* defoct can be overridden at voice level */
		if (param == DEFOCT) {
			if (ssv_p->voiceno != 0) {
				/* Is changed at voice level, so is bad */
				retval = NO;
			}
			else {
				/* Is changed at staff level, so is bad 
				 * unless the voices override */
				if (voices_override(staff, param) == YES) {
					retval = YES;
				}
				else {
					retval = NO;
				}
			}
		}
		else { 
			/* Changed on this staff, so that is bad */
			retval = NO;
		}
	}


	if (ssv_p->staffno == 0) {
		/* score level change */
		switch (param) {

		case TIME:
			/* Cannot be overridden at staff, so definitely bad,
			 * unless "changing" to what it already is */
			if ( (Score.timenum != ssv_p->timenum)
					|| (Score.timeden != ssv_p->timeden) ) {
				retval = NO;
			}
			break;

		case NUMSTAFF:
			/* If the number of staffs was or is changing
			 * to less than the staff number we are concerned with,
			 * that would be a problem. */
			if (Score.staffs < staff || ssv_p->staffs < staff) {
				retval = NO;
			}
			break;

		case SHARPS:
		case CLEF:
		case TRANSPOSITION:
		case ADDTRANSPOSITION:
		case VSCHEME:
			/* If this item is overridden on this staff
			 * then a change at score level is irrelevant,
			 * but if it is only set globally,
			 * and that is changing, that is a problem. */
			if (Staff[staff-1].used[param] == NO) {
				retval = NO;
			}
			break;
		case DEFOCT:
			/* Like above, plus check if any per voice
			 * on this staff overrides. */
			if (Staff[staff-1].used[param] == NO) {
				if (voices_override(staff, param)) {
					retval = YES;
				}
				else {
					retval = NO;
				}
			}
			break;

		default:
			pfatal("Unexpected parameter %d in mult_rpt_ssv_ok()", param);
			/*NOTREACHED*/
			break;
		}
	}
	if (retval == NO) {
		l_yyerror(inputfile, inputlineno,
				"cannot change %s during a mrpt", desc);
	}
	return(retval);
}


/* Given a main list entry that points to a staff that begins a mult rpt,
 * do error checks on it. Returns YES is all goes well; NO if not. */

static int
proc_multrpt(mll_p)

struct MAINLL *mll_p;

{
	int window_size;	/* how many bars on either side to deal with */
	struct MAINLL *begin_mll_p;	/* portion of main list to check */
	struct MAINLL *end_mll_p;
	struct MAINLL *m_p;
	struct TIMEDSSV *tssv_p;
	int barcount;
	int staff;
	int v;				/* voice */
	int mrpt_type;			/* MRT_* */
	char *mrt_name; 		/* single, dbl, quad */
	RATIONAL meas_dur;		/* for checking all space */
	int retval;			/* return value, YES or NO */


	mrpt_type = mll_p->u.staff_p->groups_p[0]->meas_rpt_type;
	window_size = mrpt_number(mrpt_type);
	mrt_name = mrt2name(mrpt_type);
	staff = mll_p->u.staff_p->staffno;
	retval = YES;

	/* Back up in main list to where the repeated music begins,
	 * which is one more bar line than the window_size. */
	barcount = 0;
	for (begin_mll_p = mll_p->prev; begin_mll_p != 0;
					begin_mll_p = begin_mll_p->prev) {
		if (begin_mll_p->str == S_BAR) {
			barcount++;
			if (barcount >= window_size + 1) {
				/* backed up far enough */
				break;
			}
			/* Don't allow repeats/restart in the source area */
			switch (begin_mll_p->u.bar_p->bartype) {
			case REPEATSTART:
			case REPEATEND:
			case REPEATBOTH:
				l_yyerror(begin_mll_p->inputfile,
				begin_mll_p->inputlineno,
				"repeats are not allowed in what a mrpt (line %d) is repeating",
				mll_p->inputlineno);
				retval = NO;
				break;
			case RESTART:
				l_yyerror(begin_mll_p->inputfile,
				begin_mll_p->inputlineno,
				"restart is not allowed in what a mrpt (line %d) is repeating",
				mll_p->inputlineno);
				retval = NO;
				break;
			default:
				break;
			}
		}
	}
	if (begin_mll_p == 0) {
		/* Back to beginning of song, where there is the beginning
		 * of a measure, so effectively another bar. */
		barcount++;
		begin_mll_p = Mainllhc_p;
	}
	else {
		/* start just beyond the bar */
		begin_mll_p = begin_mll_p->next;
	}

	/* We need to start past any SSVs. */
	for (  ; begin_mll_p != 0; begin_mll_p = begin_mll_p->next) {
		if (begin_mll_p->str != S_SSV) {
			break;
		}
	}
	if (begin_mll_p == 0) {
		pfatal("could not find beginning of song in proc_multrpt()");
	}

	if (barcount != window_size + 1) {
		l_yyerror(mll_p->inputfile, mll_p->inputlineno,
			"need at least %d measure%s before a %s mrpt",
			window_size, (mrpt_type == MRT_SINGLE ? "" : "s"),
			mrt_name);
		return(NO);
	}

	/* Similarly, search forward for window_size bar lines for
	 * the time covered by the mrpt itself. */
	barcount = 0;
	for (end_mll_p = mll_p->next; end_mll_p != 0;
					end_mll_p = end_mll_p->next) {
		if (end_mll_p->str == S_BAR) {
			barcount++;
			if (barcount >= window_size) {
				/* forward far enough */
				break;
			}
			/* Don't allow repeats / restart in the rpt area */
			switch (end_mll_p->u.bar_p->bartype) {
			case REPEATSTART:
			case REPEATEND:
			case REPEATBOTH:
				l_yyerror(end_mll_p->inputfile,
				end_mll_p->inputlineno,
				"repeats are not allowed inside a mrpt (line %d)",
				mll_p->inputlineno);
				retval = NO;
				break;
			case RESTART:
				l_yyerror(end_mll_p->inputfile,
				end_mll_p->inputlineno,
				"restart is not allowed inside a mrpt (line %d)",
				mll_p->inputlineno);
				retval = NO;
				break;
			default:
				break;
			}
		}
	}
	if (end_mll_p == 0) {
		l_yyerror(mll_p->inputfile, mll_p->inputlineno,
			"not enough measures after a %s mrpt", mrt_name);
		return(NO);
	}

	if (retval == NO) {
		/* We had kept going to try to catch multiple "bar" errors,
		 * but if there were problems there, safer not to go on. */
		return(retval);
	}

	/* We need to include the last BAR, for its timed SSVs. */
	end_mll_p = end_mll_p->next;

	/* Save the current SSV state and set it to what is should be at
	 * begin_mll_p.  When we are done checking, we'll restore */
	savessvstate();
	setssvstate(begin_mll_p);

	/* Since we need to restore the SSV state later, don't return
	 * from this function without doing that. So we assume we will get
	 * through cleanly, but set this to NO is something bad is found. */
	retval = YES;

	/* Loop through the window on both sides, looking for illegal things. */
	/* When barcount gets above zero, we are to measures beyond where
	 * the mrpt beings, and thus should check for all space. */
	barcount = - window_size;
	for (m_p = begin_mll_p; m_p != end_mll_p; m_p = m_p->next) {
		if (m_p == 0) {
			pfatal("failed to find end_mll_p in proc_multrpt");
		}

		/* Things like clef, time, and key changes are not allowed. */
		if (m_p->str == S_SSV) {
			if (mult_rpt_ssv_ok(m_p->u.ssv_p, staff, SHARPS,
						"key signature",
						m_p->inputfile,
						m_p->inputlineno) == NO) {
				retval = NO;
			}
			if (mult_rpt_ssv_ok(m_p->u.ssv_p, staff, TIME,
						"time signature",
						m_p->inputfile,
						m_p->inputlineno) == NO) {
				retval = NO;
			}
			if (mult_rpt_ssv_ok(m_p->u.ssv_p, staff, NUMSTAFF,
						"number of staffs to less than the staff of the mrpt",
						m_p->inputfile,
						m_p->inputlineno) == NO) {
				retval = NO;
			}
			if (mult_rpt_ssv_ok(m_p->u.ssv_p, staff, CLEF,
						"clef",
						m_p->inputfile,
						m_p->inputlineno) == NO) {
				retval = NO;
			}
			if (mult_rpt_ssv_ok(m_p->u.ssv_p, staff, TRANSPOSITION,
						"transpose",
						m_p->inputfile,
						m_p->inputlineno) == NO) {
				retval = NO;
			}
			if (mult_rpt_ssv_ok(m_p->u.ssv_p, staff, ADDTRANSPOSITION,
						"addtranspose",
						m_p->inputfile,
						m_p->inputlineno) == NO) {
				retval = NO;
			}
			if (mult_rpt_ssv_ok(m_p->u.ssv_p, staff, DEFOCT,
						"defoct",
						m_p->inputfile,
						m_p->inputlineno) == NO) {
				retval = NO;
			}
			if (mult_rpt_ssv_ok(m_p->u.ssv_p, staff, VSCHEME,
						"vscheme",
						m_p->inputfile,
						m_p->inputlineno) == NO) {
				retval = NO;
			}

			/* Now apply it, so we are up to date. */
			asgnssv(m_p->u.ssv_p);
		}
		else if (m_p->str == S_BAR) {
			barcount++;
			/* Loop though any timed SSVs, looking for problems */
			for (tssv_p = m_p->u.bar_p->timedssv_p; tssv_p != 0;
						tssv_p = tssv_p->next) {
				/* If there were user input errors,
				 * tssv_p->grpsyl_p may not have been set. */
				if (tssv_p->grpsyl_p == 0) {
					continue;
				}
				if (mult_rpt_ssv_ok( &(tssv_p->ssv),
						staff, CLEF,
						"clef (midmeasure)",
						tssv_p->grpsyl_p->inputfile,
						tssv_p->grpsyl_p->inputlineno)
						== NO) {
					retval = NO;
				}
				if (mult_rpt_ssv_ok(&(tssv_p->ssv),
						staff, DEFOCT,
						"defoct (midmeasure)",
						tssv_p->grpsyl_p->inputfile,
						tssv_p->grpsyl_p->inputlineno)
						== NO) {
					retval = NO;
				}
			}
		}
		else if (m_p->str == S_STAFF
					&& m_p->u.staff_p->staffno == staff) {
			if (barcount <= 0) {
				/* In preceeding measures, check for
				 * multirests, which are not allowed */
				if (m_p->u.staff_p->groups_p[0]->is_multirest == YES) {
					l_yyerror(m_p->inputfile, m_p->inputlineno,
					"can't use multirest in the source of an mrpt (line %d)",
					mll_p->inputlineno);
				}

				/* On a single mrpt need to fill in the
				 * mrptnum field of which in a row of mrpt */
				if (barcount == -1 && mrpt_type == MRT_SINGLE) {
					if (m_p->u.staff_p->groups_p[0]->meas_rpt_type == MRT_SINGLE) {
						mll_p->u.staff_p->mrptnum =
							m_p->u.staff_p->mrptnum + 1;
					}
					else {
						/* the first mrpt is labeled 2 */
						mll_p->u.staff_p->mrptnum = 2;
					}
				}
			}
			else if (barcount >= 1) {
				/* In subsequent measure; need to check for all space */
				int is_allspace;

				meas_dur.n = Score.time.n + 1;
				meas_dur.d = Score.time.d;
				is_allspace = YES;
				for (v = 0; v < MAXVOICES; v++) {
					if (m_p->u.staff_p->groups_p[v] == 0) {
						break;
					}
					if (hasspace(m_p->u.staff_p->groups_p[v],
						Zero, meas_dur) == NO) {
						l_yyerror(m_p->inputfile, m_p->inputlineno,
						"subsequent measure%s of %s mrpt (line %d) must be all space",
						(mrpt_type == MRT_QUAD ? "s" : "" ),
						mrt_name, mll_p->inputlineno);
						is_allspace = NO;
					}
				}
				if (is_allspace == YES) {
					set_mrpt(m_p->u.staff_p, mrpt_type, barcount + 1);
				}
			}
		}
	}

	/* Restore the SSV state to what is should be for where we are
	 * in processing the main list. */
	restoressvstate();

	return(retval);
}


/* Copy multi-rest measures for MIDI purposes. */

static void
do_midi_mrpt_copy(mll_p)

struct MAINLL *mll_p; /* points to first STAFF of a mrpt */

{
	struct MAINLL *from_mll_p;	/* copy from this main list staff */
	struct GRPSYL *from_grpsyl_p;	/* copy from this grpsyl */
	struct GRPSYL *grpsyl_p;	/* copy to this grpsyl */
	int m;				/* count of measures copied */
	int mrpt_type;			/* MRT_* */


	mrpt_type = mll_p->u.staff_p->groups_p[0]->meas_rpt_type;

	/* Back up to find the beginning of the section to be repeated. */
	from_mll_p = mll_p;
	from_grpsyl_p = mll_p->u.staff_p->groups_p[0];
	for (m = 0; m < mrpt_type; m++) {
		from_grpsyl_p = prevgrpsyl(from_mll_p->u.staff_p->groups_p[0],
							&from_mll_p);
		if (from_grpsyl_p == 0) {
			pfatal("unable to find first measure to copy mprt from");
		}
	}

	/* Copy as many measures as appropriate */
	grpsyl_p = mll_p->u.staff_p->groups_p[0];
	for (m = 0; m < mrpt_type; m++) {
		int v;

		/* For each voice, discard the mrpt or ms that is there
		 * and replace with the prior measure to be repeated. */
		for (v = 0; v < MAXVOICES; v++) {
			struct GRPSYL *g_p;

			if (mll_p->u.staff_p->groups_p[v] == 0) {
				break;
			}
			free_grpsyls(mll_p->u.staff_p->groups_p[v]);
			/* Note that the notelist does need to be copied,
			 * (arg 2 == YES) because even though coords
			 * are not used in MIDI, vcombine code may delete
			 * the list, so it cannot be shared across groups.
			 */
			mll_p->u.staff_p->groups_p[v] = clone_gs_list(
					from_mll_p->u.staff_p->groups_p[v], YES);
			/* The cloned list may already have breakbeam set,
			 * which will confuse has_cust_beaming(). For MIDI
			 * purposes, we don't care about beams anyway,
			 * much less subbeams, and breakbeam will get set
			 * properly later anyway on these groups,
			 * so just set to NO here. Similar for autobeam. */
			for (g_p = mll_p->u.staff_p->groups_p[v]; g_p != 0;
							g_p = g_p->next) {
				g_p->breakbeam = NO;
				g_p->autobeam = NOITEM;
			}
		}
		if (m == mrpt_type - 1) {
			/* We've copied enough */
			break;
		}

		/* Move to the next measure to copy from */
		for (from_grpsyl_p = from_mll_p->u.staff_p->groups_p[0];
				from_grpsyl_p->next != 0;
				from_grpsyl_p = from_grpsyl_p->next) {

			;
		}
		if ((from_grpsyl_p = nextgrpsyl(from_grpsyl_p, &from_mll_p)) == 0) {
			pfatal("unable to find measure to copy mprt from");
		}

		/* Move to the next measure to copy to */
		for (grpsyl_p = mll_p->u.staff_p->groups_p[0];
				grpsyl_p->next != 0;
				grpsyl_p = grpsyl_p->next) {
			;
		}
		if ((grpsyl_p = nextgrpsyl(grpsyl_p, &mll_p)) == 0) {
			l_yyerror(0, -1, "song ends with an mrpt in progress");
			return;
		}
	}
}


/* Set Meas_num to the given value, but if that is out of range,
 * print an error, and clamp to the extreme. */

void
set_meas_num(value, filename, lineno)

int value;
char *filename;
int lineno;

{
	/* Measure number really shouldn't be less than 1,
	 * but it seems safer to allow zero, just in case some code somewhere
	 * might decrement to zero temporarily, due to pickup
	 * measure or something. And zero wouldn't really hurt anything,
	 * unlike being too big, which could wrap around if we didn't
	 * constrain to fit.
	 */
	if (value < 0) {
		l_yyerror(filename, lineno,
			"measure number must not be negative");
		value = 1;
	}
	else if (value > MAX_SONG_MEASURES) {
		l_yyerror(filename, lineno,
			"measure number must not be more than %d", MAX_SONG_MEASURES);
		value = MAX_SONG_MEASURES;
	}
	Meas_num = value;
}
