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

/* This file contains functions that deal with
 * extending underscores and dashes on lyric syllables. 
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

static double end_dashes P((struct MAINLL *mll_p, struct GRPSYL *syl_p,
		int verse, int place, int *carryover_p));
static double end_underscore P((struct MAINLL *mll_p, struct GRPSYL *syl_p,
		int verse, int place, int *carryover_p));
static double endx P((struct GRPSYL *last_grp_p, double end));
static int has_above_lyr P((struct MAINLL *mll_p, RATIONAL begin_time,
		struct GRPSYL *group_p, int verse));
static int voice_is_above P((int v1, int v2));
static struct GRPSYL *find_verse_place P((struct STAFF *staff_p,
		int verse, int place));
static RATIONAL default_end P((struct MAINLL *mll_p, struct GRPSYL *syl_p,
		RATIONAL start_time, double *end_p));
static int bar_ends_extender P((struct BAR *bar_p, struct MAINLL *mll_p,
		int staffno, int verse, int place,
		struct GRPSYL **nextsyl_p_p));
static void pr_extender P((int ch, double start, double end, double y,
		int font, int size));
static void insert_carryover_syllable P((struct MAINLL *mll_p, int staffno,
		int sylplace, int verseno, char *dash_or_underscore,
		int font, int size));
static void add_syllable P((struct STAFF *staff_p, int sylplace, int verseno,
		char *dash_or_underscore, int font, int size,
		double begin_x, struct CHORD *chord_p));
static void stitch_syl_into_chord P((struct CHORD *chord_p,
		struct GRPSYL *syl_gs_p));


/* This function is called on lyric syllables in two cases:
 *	1) During placement phase to determine if an extender needs to
 *	be carried over to a following staff. In this case, really_print
 *	will be NO. (The easiest way to see if an extender needs to carry
 *	over is to pretend to print an extender). The return value will
 *	be YES if there should be a carryover.
 *	2) When printing the syllable, to draw an extender after it,
 *	if appropriate. In this case, really_print will be YES,
 *	and the return value is meaningless.
 *
 * If a syllable ends with a dash, the dash should be placed halfway between
 * where this syllable ends and the next begins. Or if there is a big space,
 * multiple dashes should be spread out in that space.  If it ends with an
 * underscore, then an underline should be drawn from the end of this
 * syllable to the east edge of the notes in the last chord before
 * the next syllable for the same staff/verse/place.
 * But if there is a carryover, this just does the extender up to the
 * end of the current score; it will get called again on the next score
 * to continue the extender.
 * If an underscore is used on a single chord, rather than a mellisma
 * (which might technically be considered an "incorrect" usage of underscore),
 * we figure the underscore should be drawn to just before the next syllable,
 * unless there is a rest earlier. Or if the next syllable begins a measure,
 * the underscore ends before the bar line, to look better.
 * If really printing, the dash or underscore is removed
 * from the end of the string so it won't get printed
 * with the syllable.
 */

int
spread_extender(syl_p, mll_p, verse, sylplace, really_print)

struct GRPSYL *syl_p;	/* current syllable */
struct MAINLL *mll_p;	/* which MAINLL struct it's hanging off of */
int verse;		/* verse number */
int sylplace;		/* PL_ABOVE, etc */
int really_print;	/* if YES, actually print, otherwise just return
			 * whether needs to be carried over to next score */

{
	char *syl;		/* to walk through characters of the syllable */
	int font;
	int size;
	double start;		/* dash area or underscore starts here */
	double end;		/* dash area or underscore ends here */
	int ch;			/* current character in syllable */
	int last_ch = '\0';	/* final character in syllable */
	int extndr_font;	/* in case user changes font/size after
				 * extender, keep track of what font/size the
				 * extender was */
	int extndr_size = -1;
	char *ch_p;		/* pointer to where - or _ is in string */
	int carryover;		/* YES if will carry over to next staff */


	
	/* See if there is a dash or underscore at the end of the syllable.
	 * If so, save pointer to it. Note that this may not be the last
	 * byte in the string, because there could be font/size changes
	 * after it. */
	font = syl_p->syl[0];
	size = syl_p->syl[1];
	syl = syl_p->syl + 2;

	/* These two assignments avoid "used without being set" warnings */
	extndr_font = font;
	ch_p = syl;

	/* Find last character of syllable */
	while ( (ch = next_str_char( &syl, &font, &size)) != '\0') {
		if ( ( ch == '-' || ch == '_') && IS_STD_FONT(font) ) {
			ch_p = syl - 1;
			extndr_font = font;
			extndr_size = size;
			last_ch = ch;
		}
		else {
			last_ch = '\0';
		}
	}

	/* If there is an extender, handle it */
	if (last_ch != '\0') {
		if ( last_ch == '-') {
			end = end_dashes(mll_p, syl_p, verse, sylplace,
							&carryover);
		}
		else {
			end = end_underscore(mll_p, syl_p, verse, sylplace,
							&carryover);
		}

		if (really_print == NO) {
			return(carryover);
		}

		/* Move the rest of the string
		 * over the dash or underscore,
		 * so it won't get printed with the syllable */
		do {
			*ch_p = *(ch_p + 1);
		} while  ( *++ch_p != '\0');
		start = syl_p->c[AE];

		/* procsyls() adjusted the east in certain cases for
		 * placement purposes. For printing we need to cancel out
		 * those adjustments. */
		if (syl_p->next != 0) {
			if (last_ch == '-') {
				start -= width(extndr_font, extndr_size, last_ch);
			}
		}
		if (syl_p->next == 0) {
			start += width(extndr_font, extndr_size, ' ');
		}

		/* actually print the extender */
		pr_extender(last_ch, start, end, syl_p->c[AY],
						extndr_font, extndr_size);
	}
	return(NO);
}


/* Given a syllable ending with a dash, and some other info,
 * return where to end the dash(es). If the dashes carry over
 * to the following score, this will return a point near the east end of
 * the current score, after setting *carryover_p to YES.
 */

static double
end_dashes(mll_p, syl_p, verse, place, carryover_p)

struct MAINLL *mll_p;	/* points to STAFF containing the syl with dash */
struct GRPSYL *syl_p;	/* this is the syllable with dash */
int verse;		/* which verse the syl_p is for */
int place;		/* a PL_* value for where the lyric is */
int *carryover_p;	/* return value, set to YES if there was a carryover */

{
	int staffno;
	struct BAR *lastbar_p;

	staffno = syl_p->staffno;
	*carryover_p = NO;
	lastbar_p = 0;	/* will get set to something better before being used */
	syl_p = syl_p->next;

	do {
		/* Go forward looking for another non-space syllable */
		if (syl_p != 0) {
			for (    ; syl_p != 0; syl_p = syl_p->next) {
				if (syl_p->grpcont != GC_SPACE) {
					/* found it! */
					return(syl_p->c[AW]);
				}
			}
		}

		/* No ending syl in current measure. Try the next. */
		for (mll_p = mll_p->next; mll_p != 0; mll_p = mll_p->next) {
			if (mll_p->str == S_BAR) {
				if (bar_ends_extender(mll_p->u.bar_p,
						mll_p, staffno, verse,
						place, 0) == YES) {
					return(mll_p->u.bar_p->c[AW]);
				}
				lastbar_p = mll_p->u.bar_p;
			}

			else if (mll_p->str == S_FEED) {
				/* If this is a feed at the very end of the
				 * main list, or one or more blocks follow it,
				 * this is not the kind of feed
				 * we're looking for. */
				if (mll_p->next == 0 ||
						mll_p->next->str == S_BLOCKHEAD) {
					continue;
				}
				/* There is a carryover unless the
				 * pseudo-bar is something that would end
				 * the extender. */
				mll_p = mll_p->next;
				if (mll_p->str != S_CLEFSIG
						|| mll_p->u.clefsig_p->bar_p
						== 0) {
					if (mll_p->str == S_FEED || mll_p->str == S_PRHEAD) {
						/* Being here may mean there is
						 * a bug somewhere else,
						 * because the main list rules
						 * are violated. But we can
						 * render such a bug harmless
						 * by continuing here.
						 * The PRHEAD case can happen
						 * if user put a print cmd
						 * after a final newscore and
						 * then has a dangling
						 * extender at the end of their
						 * input.
						 */
						continue;
					}
					pfatal("end_dashes found unexpected main list contents after feed");
				}
				if (bar_ends_extender(mll_p->u.clefsig_p->bar_p,
						mll_p, staffno, verse, place, 0)
						== NO) {
					*carryover_p = YES;
				}
				return(lastbar_p->c[AW] - Stepsize);
			}

			else if (mll_p->str == S_STAFF
						&& mll_p->u.staff_p->staffno
						== staffno) {
				syl_p = find_verse_place(mll_p->u.staff_p,
								verse, place);
				break;
			}
		}
	} while (mll_p != 0);

	/* Fell off end of song. Use final bar */
	return(lastbar_p->c[AW] - Stepsize);
}


/* Given a syllable ending with an underscore, and some other info,
 * return where to end the underscore. If the underscore carries over
 * to the following score, this will return a point near the east end of
 * the current score, after setting *carryover_p to YES.
 */

static double
end_underscore(mll_p, syl_p, verse, place, carryover_p)

struct MAINLL *mll_p;	/* points to STAFF containing the syl with underscore */
struct GRPSYL *syl_p;	/* this is the syllable with underscore */
int verse;		/* which verse the syl_p is for */
int place;		/* a PL_* value for where the lyric is */
int *carryover_p;	/* return value, set to YES if there was a carryover */

{
	struct GRPSYL *current_grp_p[MAXVOICES];/* which group we are
						 * currently dealing with on
						 * each voice */
	RATIONAL group_time[MAXVOICES];		/* accumulated time value of
						 * groups up to the one we
						 * are currently dealing with */
	short had_rest[MAXVOICES];		/* YES or NO */
	RATIONAL current_time;			/* how far we are in meas */
	RATIONAL end_time;			/* where next non-space
						 * syllable is for this
						 * staff/place/verse, if
						 * there is one
						 * in the current measure,
						 * otherwise the end of the
						 * current measure. */
	struct GRPSYL *last_grp_p;		/* if non-zero, this is the
						 * current candidate group
						 * with which we could
						 * potentially align the
						 * end of the underscore. */
	double end;				/* this is how far we will
						 * draw the underscore if we
						 * don't find any reason to
						 * stop it sooner. */
	struct GRPSYL *grp_p;			/* walk through GRPSYLs */
	struct STAFF *staff_p;			/* next measure's STAFF */
	struct GRPSYL *nextsyl_p;		/* syl list for same verse/place
						 * in the next measure */
	RATIONAL grp_end_time;			/* where a current group ends */
	
	int vindex;				/* voice index */
	int v;					/* voice index */
	short found_feed;			/* YES or NO */
	int staffno;


	*carryover_p = NO;	/* assume no carryover for now */
	staffno = mll_p->u.staff_p->staffno;

	/* Back up from the syllable with underscore to count up time-wise how
	 * far into the measure it is */
	current_time = Zero;
	for (grp_p = syl_p->prev; grp_p != 0; grp_p = grp_p->prev) {
		current_time = radd(current_time, grp_p->fulltime);
	}
		
	/* Set a default end time and place.
	 * Most likely, we will discover later we need to stop the underscore
	 * earlier than this, but if the user is using underscore in a strange
	 * way, like on a single long note rather than a melissma,
	 * we'll use this as the default place to end the underscore. */
	end_time = default_end(mll_p, syl_p, current_time, &end);

	/* We don't yet have any candidate group with which to align
	 * the ending of the underscore. */
	last_grp_p = 0;

	/* For each voice, if it exists, find the group that contains
	 * the time of the syllable with the underscore, and make that
	 * the "current group" for that voice. If the voice doesn't exist,
	 * set the current group pointer to zero. */
	staff_p = mll_p->u.staff_p;
	for (vindex = 0; vindex < MAXVOICES; vindex++) {
		group_time[vindex] = Zero;
		had_rest[vindex] = NO;
		if (staff_p->groups_p[vindex] != 0) {
			for (current_grp_p[vindex] = staff_p->groups_p[vindex];
					current_grp_p[vindex] != 0;
					current_grp_p[vindex]
					= current_grp_p[vindex]->next) {
				if (GE(current_time, group_time[vindex]) &&
						LT(current_time,
						radd(group_time[vindex],
						current_grp_p[vindex]->fulltime))) {
					/* This group contains the syl's time */

					if (current_grp_p[vindex]->grpcont == GC_REST) {
						had_rest[vindex] = YES;
					}
					break;
				}
				group_time[vindex] = radd(group_time[vindex],
						current_grp_p[vindex]->fulltime);
			}
			if (current_grp_p[vindex] == 0) {
				pfatal("unable to find group containing syl's time");
			}
		}
		else {
			/* voice doesn't exist in this measure */
			current_grp_p[vindex] = 0;
		}
	}

	for (   ;  ; ) {
		/* Most of the time, we use voice 1 to determine where to
		 * end the underscore. However, if voice 1 has spaces,
		 * we'll use voice 3 (the "middle" voice), and 
		 * if that is non-existent or space, we use voice 2.
		 * If everything is space, we keep going and hope for the
		 * best. If all else fails, we would end up using the "end"
		 * value as the default.
		 * 
		 * However, if this is a below or between lyric,
		 * and there exists an above lyric
		 * during the time we are dealing with,
		 * we assume voice 1 goes with the above lyric, and the
		 * below lyric probably goes with voice 2, or possibly voice 3.
		 * If both those voices exist, it's probably not possible
		 * to divine which the user wants the lyric associated with
		 * without reading their mind. But 3 voices on a vocal staff
		 * is quite unusual, especially with rests in different places,
		 * so we use voice 2 if it exists and is non-space.
		 * If that fails, we try 3, then 1, then punt.
		 * If it is a between lyric, there is a slight chance the user
		 * really wanted us to use the staff below, but we always
		 * associate "between" things with the staff above.
		 * They should use "above" on the next staff instead.
		 */
		vindex = 0;	/* use voice 1 as default */
		if (place != PL_ABOVE) {
			/* The lyric is below or between.
			 * Test voices 2, 3, and 1 (indexes 1, 2, 0)
			 * in that order till we find one that isn't a space,
			 * and see if there is an above lyric
			 * during its time. If so, that is the voice to use
			 * during this time to figure out
			 * where to end underscore.
			 * We also need to use this voice if all the voices
			 * above it on the staff are all space.
			 */
			if (current_grp_p[1] != 0 &&
					current_grp_p[1]->grpcont != GC_SPACE &&
					(has_above_lyr(mll_p, current_time,
					current_grp_p[1], verse) == YES ||
					(LT(current_time, end_time) &&
					hasspace(staff_p->groups_p[0], current_time, end_time) == YES &&
					hasspace(staff_p->groups_p[2], current_time, end_time) == YES)))  {

				vindex = 1;
			}
			else if (current_grp_p[2] != 0 &&
					current_grp_p[2]->grpcont != GC_SPACE &&
					(has_above_lyr(mll_p, current_time,
					current_grp_p[2], verse) == YES ||
					(LT(current_time, end_time) &&
					hasspace(staff_p->groups_p[0], current_time, end_time) == YES))) {
				vindex = 2;
			}
			/* Otherwise we go with the default, voice 1.
			 * We know voice 1 will always exist. */
		}

		else {		/* place is above */
			/* Note that voice 1 always exists, so
			 * so we don't need to check for null first
			 * on that voice. */
			if (current_grp_p[0]->grpcont != GC_SPACE) {
				vindex = 0;
			}
			else if (current_grp_p[2] != 0 &&
					current_grp_p[2]->grpcont != GC_SPACE) {
				vindex = 2;
			}
			else if (current_grp_p[1] != 0 &&
					current_grp_p[1]->grpcont != GC_SPACE) {
				vindex = 1;
			}
		}

		/* At this point, we know which voice is most relevant for
		 * checking if it is time to end the underscore.
		 * See if the current group in that voice contains the
		 * time value of the ending syllable. */
		if ( GE(end_time, group_time[vindex]) && LT(end_time,
					radd(group_time[vindex],
					current_grp_p[vindex]->fulltime)) ) {
			/* We need to end the underscore now. */
			return(endx(last_grp_p, end));
		}

		/* If the relevant group is a rest, need to stop here */
		if (current_grp_p[vindex]->grpcont == GC_REST) {
			return(endx(last_grp_p, current_grp_p[vindex]->c[AW]));
		}
		/* If following group is a grace, which will take no time,
		 * check if we should end on the current note. */
		else if (current_grp_p[vindex]->next != 0 &&
				current_grp_p[vindex]->next->grpvalue == GV_ZERO) {
			RATIONAL time_with_this_grp;
			time_with_this_grp = radd(group_time[vindex], current_grp_p[vindex]->fulltime);
			if (GE(time_with_this_grp, end_time)) {
				return(endx(current_grp_p[vindex], end));
			}
		}
		else if (current_grp_p[vindex]->grpcont == GC_NOTES) {
			/* Save as last known group so far at which we
			 * could potentially end the underscore. */
			last_grp_p = current_grp_p[vindex];
		}

		/* We're done with this group; move to next */
		current_time = radd(current_time,
				current_grp_p[vindex]->fulltime);

		/* Catch up all the voices to the current time */
		for (v = 0; v < MAXVOICES; v++) {
			if (current_grp_p[v] != 0) {
				grp_end_time = radd(group_time[v],
					current_grp_p[v]->fulltime);

				while ( LE(grp_end_time, current_time) ){
					/* Special case. Suppose,
					 * as an example, soprano and
					 * alto share a staff and the
					 * soprano has a long note
					 * while the alto has a
					 * melissma. The underscore
					 * should then go to the
					 * last note of the melissma,
					 * even though soprano is
					 * the reference voice.
					 * However, if the alto line
					 * had had rests, it's likely
					 * it's just accompaniment,
					 * not a vocal line, or at
					 * least they should have
					 * used separate above/below
					 * lyrics. 
					 * So if this group is below
					 * the reference group and
					 * hasn't had any rests and
					 * is east of our candidate
					 * last group, make it the
					 * new candidate last group. */
					if (voice_is_above(vindex, v)
					  && place != PL_ABOVE
					  && had_rest[v] == NO
					  && last_grp_p != 0
					  && current_grp_p[v]->grpcont
					  == GC_NOTES
					  && current_grp_p[v]->c[AX]
					  > last_grp_p->c[AX]) {
						last_grp_p = current_grp_p[v];
					}

					/* move on to next group */
					current_grp_p[v] =
						current_grp_p[v]->next;
					if (current_grp_p[v] == 0) {
						break;
					}

					group_time[v] = grp_end_time;
					grp_end_time = radd(
						group_time[v],
						current_grp_p[v]->fulltime);
					if (current_grp_p[v]->grpcont
								== GC_REST) {
						had_rest[v] = YES;
					}
				}
			}
		}

		/* Are we now at the end of the current measure? */
		if (current_grp_p[vindex] == 0) {
			/* If there is a feed after this bar,
			 * we need to see if a carryover is needed.
			 * If so, we will end this underscore just before
			 * the bar, and carry it over to the next score.
			 */
			found_feed = NO;
			for (mll_p = mll_p->next; mll_p != 0; mll_p = mll_p->next) {
				if (mll_p->str == S_BAR) {
					if (bar_ends_extender(mll_p->u.bar_p, mll_p,
							syl_p->staffno, verse,
							place, &nextsyl_p)
							== YES) {
						/* It's not clear
						 * where we should stop if
						 * we can't deduce a following
						 * syllable. However,
						 * if we go to the bar line,
						 * the user can always use
						 * a <> syllable to force
						 * an earlier ending if needed,
						 * whereas if we go
						 * with the last group,
						 * there's probably no
						 * reasonable workaround
						 * if that's not what they want,
					 	 * so use the bar line. */
						if (nextsyl_p == 0) {
							return(end);
						}
						else if (nextsyl_p->grpcont
								== GC_SPACE) {
							/* "carries over" */
							return(end);
						}
						else {
							return(endx(last_grp_p, end));
						}
					}
				}
				else if (mll_p->str == S_FEED) {
					found_feed = YES;
				}
				else if (mll_p->str == S_STAFF &&
						mll_p->u.staff_p->staffno ==
						staffno) {
					break;
				}
				else if (mll_p->str == S_SSV) {
					/* if this staff becomes invisible,
					 * end the underscore at the last
					 * group before that. */
					struct SSV *ssv_p;
					ssv_p = mll_p->u.ssv_p;
					if (ssv_p->context == C_STAFF
						&& ssv_p->staffno == staffno
						&& ssv_p->used[VISIBLE] == YES
						&& ssv_p->visible == NO) {
					    return(endx(last_grp_p, end));
					}
				}
			}
			if (mll_p == 0) {
				/* fell off end of song */
				return(endx(last_grp_p, end));
			}
			staff_p = mll_p->u.staff_p;

			/* See if there is a syllable at the same verse/place */
			if ((nextsyl_p = find_verse_place(staff_p,
					verse, place)) != 0 &&
					nextsyl_p->grpcont != GC_SPACE) {
				/* There is a syllable at the
				 * beginning of the next meas,
				 * so we end the underscore,
				 * unless it was just a carryover syllable
				 * that we added earlier. */
				if (nextsyl_p->syl[2] != '_'
						|| nextsyl_p->syl[3] != '\0') {
					return(endx(last_grp_p, end));
				}
			}
			if (found_feed == YES) {
				if (staff_p->groups_p[vindex] != 0 &&
						staff_p->groups_p[vindex]->grpcont == GC_REST) {
					/* next meas begins with a rest,
					 * so no need to carry over */
					return(endx(last_grp_p, end));
				}
				/* We need to end the underscore on the
				 * current score, and arrange to carry it
				 * over on the next score. */
				*carryover_p = YES;
				return(end);
			}

	
			/* Move to next measure by initing each
			 * current_grp_p[vindex] to the first group
			 * in the next measure. */
			for (vindex = 0; vindex < MAXVOICES; vindex++) {
				current_grp_p[vindex] = staff_p->groups_p[vindex];
				group_time[vindex] = Zero;
				if (current_grp_p[vindex] != 0 &&
						current_grp_p[vindex]->grpcont
						== GC_REST) {
					had_rest[vindex] = YES;
				}
			}
			end_time = default_end(mll_p,
				find_verse_place(staff_p, verse, place),
				Zero, &end);
			current_time = Zero;
		}
	}
}


/* If we found a last group where we could end a underscore,
 * return where the east edge of its notes are,
 * otherwise return the "end" value as the default.
 */

static double
endx(last_grp_p, end)

struct GRPSYL *last_grp_p;	/* if != 0, use east edge of notes of this */
double end;			/* if all else fails, use this */

{
	int n;		/* note index */
	double edge;	/* return value */


	if (last_grp_p == 0) {
		return(end);
	}

	if (last_grp_p->grpcont != GC_NOTES) {
		/* This should actually never happen with the current code,
		 * but just in case, we use the east of the group */
		return(last_grp_p->c[AE]);
	}

	/* find east edge of notes, not counting any dots or flags */
	edge = -1000000.0;	/* init to impossible value */
	for (n = 0; n < last_grp_p->nnotes; n++) {
		if (last_grp_p->notelist[n].c[AE] > edge) {
			edge = last_grp_p->notelist[n].c[AE];
		}
	}
	/* If the edge we calculated is east of the default end, use
	 * the default end, because that is suppose to be the farthest
	 * possible east we can be. This could happen if the user used
	 * <^....> on a lyric to force part of the lyric to encroach
	 * into the previous groups' space.  In that case we need to end
	 * the underscore where the encroaching lyric begins, not where
	 * the last note group ends.
	 */
	if (edge > end) {
		return(end);
	}
	return(edge);
}


/*
 * Return YES if there is an above lyrics during the specified time.
 * We have to use some heuristics.
 *
 *	If there is any non-space above lyric for the given verse
 *	at any point between the begin time
 *	and the begin time plus the fulltime of the group_p,
 *	then there is an above lyric.	
 *
 *	If there is a rest on voice 1, that implies a rest in an above lyric
 *	line.
 *
 *	If there is lyric space for the duration in question, either
 *	explicit space, or just no above lyrics at all for the given verse
 *	in this measure, then we don't know for sure where there are no
 *	above lyrics, or there is an earlier above lyric for this verse
 *	that extends into the duration.
 *	If we find some earlier non-space above lyric
 *	and it ends with an extender (dash or underscore),
 *	we say there is an above lyric.
 *	If there is no such lyric, or the first non-space above lyric
 *	lyric we come to in backing up does not end with an extender,
 *	we say there isn't an above lyric.
 */

static int
has_above_lyr(mll_p, begin_time, group_p, verse)

struct MAINLL *mll_p;		/* points to syl's STAFF */
RATIONAL begin_time;
struct GRPSYL *group_p;		/* see if there a lyric above this group */
int verse;

{
	struct STAFF *staff_p;
	struct GRPSYL *grp_p;
	RATIONAL cumm_time;		/* current cummulative time */
	RATIONAL new_cumm_time;		/* cumm_time + group's fulltime */
	RATIONAL end_time;		/* begin_time + syl's fulltime */
	int n;				/* syllist index */
	int prev_extends;		/* YES/NO if prev syl has extender */


	if (mll_p->str != S_STAFF) {
		pfatal("has_above_lyr passed wrong type of struct");
	}

	staff_p = mll_p->u.staff_p;
	end_time = radd(begin_time, group_p->fulltime);

	/* Go through syllists for the staff */
	prev_extends = NO;
	for (n = 0; n < staff_p->nsyllists; n++) {
		if (staff_p->sylplace[n] == PL_ABOVE &&
					staff_p->syls_p[n]->vno == verse) {
			cumm_time = Zero;
			for (grp_p = staff_p->syls_p[n]; grp_p != 0; grp_p = grp_p->next) {
				new_cumm_time = radd(cumm_time, grp_p->fulltime);

				if ( LT(new_cumm_time, begin_time) &&
						grp_p->grpcont != GC_SPACE) {
					prev_extends = has_extender(grp_p->syl);
				}

				/* See if this syllable overlaps the time
				 * of the group we are checking against. */
				else if ( (GE(begin_time, cumm_time) &&
						LT(begin_time, new_cumm_time)) ||
						(GE(end_time, cumm_time) &&
						LT(end_time, new_cumm_time)) ) {

					/* This is a relevant group. If it isn't
					 * a space, then we know there is
					 * indeed an above lyric. */
					if (grp_p->grpcont != GC_SPACE) {
						return(YES);
					}
					if (prev_extends == YES) {
						/* A syllable
						 * earlier in the measure
						 * is extending into the
						 * duration, so that counts.
						 */
						return(YES);
					}
				}
				else if (GT(new_cumm_time, end_time)) {
					/* we're past the relevant syl(s) */
					break;
				}
				cumm_time = new_cumm_time;
			}
			/* We've dealt with the only relevant syl list
			 * in this measure. */
			break;
		}
	}

	/* If there is a rest on voice 1, there is an implicit above
	 * lyric (albeit a pause in the above lyrics). Or at least it hardly
	 * makes sense to use voice 1 for below/between lyrics if voice 1
	 * is a rest but there is another voice below it that isn't.
	 */
	cumm_time = Zero;
	for (grp_p = mll_p->u.staff_p->groups_p[0]; grp_p != 0; grp_p = grp_p->next) {
		new_cumm_time = radd(cumm_time, grp_p->fulltime);
		if ( (GE(begin_time, cumm_time) &&
					LT(begin_time, new_cumm_time)) ||
					(GE(end_time, cumm_time) &&
					LT(end_time, new_cumm_time)) ) {
			if (grp_p->grpcont == GC_REST) {
				return(YES);
			}
		}
		cumm_time = new_cumm_time;
		if (GT(cumm_time, end_time)) {
			/* past the relevant groups */
			break;
		}
	}

	/* If we got here, we weren't able to tell for
	 * sure if there is an above lyric, because there
	 * was either implicit or explicit space.
	 * Most likely there is no above lyric,
	 * but there is a slight possibility there is
	 * a lyric holding over into this time period
	 * via a melisma or tied notes from a previous measure.
	 * So we back up looking for such a lyric. If we find an above lyric
	 * that ends with an extender (underscore or dash),
	 * we declare that there is an above lyric.
	 * If we find one without an extender or back up
	 * all the way to the beginning of the song without
	 * finding any above lyric, there is no above lyric here.
	 * But we give up after 20 measures, figuring it's
	 * really unlikely for any melisma or tie to last
	 * that long, especially since any scorefeeds
	 * would cause a syllable to get added. The exact
	 * value of 20 is arbitrary; it just seems like plenty.
	 *
	 * prevgrpsyl doesn't work on syls, just groups,
	 * but by giving it staff_p->groups_p[0] (we
	 * know voice 1 will always exist), it will give
	 * us the mll_p for the staff we need.
	 */
	for (n = 0; n < 20; n++) {
		struct GRPSYL *last_non_space_p;

		if (prevgrpsyl(mll_p->u.staff_p->groups_p[0],
					&mll_p) == 0) {
			/* Got to beginning of song */
			return(NO);
		}

		grp_p = find_verse_place(mll_p->u.staff_p, verse, PL_ABOVE);

		if (grp_p == 0) {
			/* No relevant lyrics in this meas */
			continue;
		}

		last_non_space_p = 0;
		for (  ; grp_p != 0; grp_p = grp_p->next) {
			if (grp_p->grpcont != GC_SPACE) {
				last_non_space_p = grp_p;
			}
		}
		if (last_non_space_p != 0) {
			/* Found a preceding syllable */
			return(has_extender(last_non_space_p->syl));
		}
	}
	/* We've backed up far enough that the chances of there actually being
	 * an above lyrics are very, very slim. */
	return(NO);
}


/* Returns YES if voice with index v1 is "above" voice v2; else NO */

static int
voice_is_above(v1, v2)

int v1;
int v2;

{
	/* Voice number is one more than its index, so convert index to
	 * number so it's easier to think about */
	v1++;
	v2++;

	/* Voice 1 is above voice 2 and 3 */
	if (v1 == 1) {
		return(YES);
	}

	/* Voice 3 is the "middle" voice and thus "above" voice 2 */
	if (v1 == 3 && v2 == 2) {
		return(YES);
	}

	return(NO);
}


/* Given a STAFF, return the first GRPSYL in the syllable list for the given
 * verse and place, if one exists. Otherwise return 0.
 */

static struct GRPSYL *
find_verse_place(staff_p, verse, place)

struct STAFF *staff_p;
int verse;
int place;

{
	int n;

	for (n = 0; n < staff_p->nsyllists; n++) {
		if (staff_p->sylplace[n] == place &&
				staff_p->syls_p[n]->vno == verse) {
			return(staff_p->syls_p[n]);
		}
	}
	return(0);
}


/* Given a syl and related info, return the default time and place at which to
 * end an underscore from that syl, for this measure. If there is a
 * non-space syl later in the measure, this will be right before that syl,
 * otherwise right before the bar line.
 */

static RATIONAL
default_end(mll_p, syl_p, start_time, end_p)

struct MAINLL *mll_p;		/* the STAFF pointing to the syl */
struct GRPSYL *syl_p;		/* start looking from the syl */
RATIONAL start_time;		/* syl is already this far into measure */
double *end_p;			/* X value at which to end underscore is
				 * returned via this pointer */

{
	struct GRPSYL *grp_p;
	RATIONAL end_time;	/* return value */

	if (syl_p == 0) {
		/* No syllable for current verse/place in current measure.
		 * Time signature may not be up to date, so add up the
		 * time of voice 1, which we know exists.
		 */
		end_time = start_time;
		for (grp_p = mll_p->u.staff_p->groups_p[0]; grp_p != 0;
							grp_p = grp_p->next) {
			end_time = radd(end_time, grp_p->fulltime);
		}
	}
	else {
		/* Go forward in the syl list, finding where the next non-space
		 * syllable is, if there is one in the current measure,
		 * otherwise find the end of the measure.
		 * Save the time and location of this.
		 */
		end_time = radd(start_time, syl_p->fulltime);
		for (grp_p = syl_p->next; grp_p != 0; grp_p = grp_p->next) {
			if (grp_p->grpcont == GC_SPACE) {
				/* Underscore continues through "space" syls */
				end_time = radd(end_time, grp_p->fulltime);
				continue;
			}
			else {
				/* We have found the syllable
				 * at which the underscore
				 * from the previous syllable ends */
				break;
			}
		}
	}

	/* If a next syl was found, set end to near its west.
	 * Most likely, we will discover later we need to stop the underscore
	 * earlier than this, but if the user is using underscore in a strange
	 * way, like on a single long note rather than a melissma,
	 * we'll use this as the default place to end the underscore.*/
	if (grp_p != 0) {
		*end_p = grp_p->c[AW] - Stepsize;
	}
	else {
		/* The ending syllable (if any) is not in the current measure.
		 * So for now we set the end to near the west of the bar line.
		 * Note that end_time will have added up to
		 * the full measure duration in this case.
		 */
		for (  ; mll_p != 0; mll_p = mll_p->next) {
			if (mll_p->str == S_BAR) {
				*end_p = mll_p->u.bar_p->c[AW] - Stepsize;
				break;
			}
		}
		if (mll_p == 0) {
			pfatal("underscore: failed to find next bar");
		}
	}
	return(end_time);
}


/* Given a bar, see if it is a bar that might force stopping an extender,
 * and return YES, if so. If nextsyl_p_p is non-null, it also attempts
 * to fill that in with a pointer to the next "logical" syllable.
 * (Usually the next syllable, but at the end of a repeat it would
 * be the first syllable in the repeated section).
 * If it can't figure out the correct syllable, it fills in null.
 */

static int
bar_ends_extender(bar_p, mll_p, staffno, verse, place, nextsyl_p_p)

struct BAR *bar_p;
struct MAINLL *mll_p;	/* points to a BAR  or CLEFSIG*/
int staffno;
int verse;
int place;
struct GRPSYL **nextsyl_p_p;	/* If this is non-zero, and we can deduce
				 * the next "logical" syl, the pointed to value
				 * will be updated to point to that next syl,
				 * else will be zero. */
		
{
	int bartype;

	bartype = bar_p->bartype;
	if (bartype == RESTART) {
		/* We shouldn't continue an extender over a restart.
		 * The "next" logical measure is probably the
		 * target of a D.S. or a D.C.
		 * But we don't attempt to parse
		 * STUFF strings to know such things.
		 * So we say the extender ends here,
		 * but we don't know what the "next" measure is.
		 */
		if (nextsyl_p_p != 0) {
			*nextsyl_p_p = 0;
		}
		return(YES);
	}

	if (bartype == REPEATEND || bartype == REPEATBOTH) {
		if (nextsyl_p_p == 0) {
			return(YES);
		}

		/* This ends the extender. The next logical measure
		 * is at the beginning of the repeat. */
		for (mll_p = mll_p->prev; mll_p != 0; mll_p = mll_p->prev) {
			if (mll_p->str == S_BAR &&
					(mll_p->u.bar_p->bartype
					== REPEATSTART ||
					mll_p->u.bar_p->bartype
					== REPEATBOTH)) {
				mll_p = mll_p->next;
				break;
			}
		}
		if (mll_p == 0) {
			/* repeatstart is implicit at beginning of song */
			mll_p = Mainllhc_p;
		}
		for (  ; mll_p != 0; mll_p = mll_p->next) {
			if (mll_p->str == S_BAR) {
				/* staff doesn't exist in this measure */
				*nextsyl_p_p = 0;
				return(YES);
			}
			if (mll_p->str == S_STAFF && mll_p->u.staff_p->staffno
						== staffno) {
				*nextsyl_p_p = find_verse_place(
						mll_p->u.staff_p, verse, place);
				return(YES);
			}
		}
	}

	if (mll_p->u.bar_p->endingloc == STARTITEM) {
		/* If this is the start of a second or subsequent ending,
		 * this ends the extender. This is the case if the previous
		 * bar was a STARTITEM on INITEM. But apparently there
		 * isn't a repeat ending here, or we would have hit the
		 * bartype check for that. So it is too hard to try to deduce
		 * the next logical syllable. */
		for (mll_p = mll_p->prev; mll_p != 0; mll_p = mll_p->prev) {
			if (mll_p->str == S_BAR) {
				if (mll_p->u.bar_p->endingloc == STARTITEM ||
						mll_p->u.bar_p->endingloc
						== INITEM) {
					if (nextsyl_p_p != 0) {
						*nextsyl_p_p = 0;
					}
					return(YES);
				}
				break;
			}
		}
	}

	return(NO);
}


/* Actually print an extender (dash or underscore) */

static void
pr_extender(ch, start, end, y, font, size)

int ch;		/* dash or underscore */
double start;	/* where to start printing */
double end;	/* where to end printing */
double y;	/* y coordinate */
int font;	/* font to use for dash */
int size;	/* size to use for dash */

{
	if (ch == '-') {
		double dashwidth;
		char dashstring[4];

		dashwidth = width(font, size, '-');

		/* generate the internal string format of a dash */
		/* can't use dash_string function here since that also
		 * deals with ~ which is okay for stuff but not lyrics */
		dashstring[0] = (char) font;
		dashstring[1] = (char) size;
		dashstring[2] = '-';
		dashstring[3] = '\0';

		if ( (end - start) < (15.0 * dashwidth) ) {
			/* not much space, so find midpoint of
			 * available distance and put dash there */
			pr_string(start + ((end - start) / 2.0)
					- (dashwidth / 2.0),
					y, dashstring, J_LEFT,
					(char *) 0, -1);
		}
		else {
			int numdashes;		/* how many dashes to print */
			double spacebetween;	/* between dashes */

			/* Lots of space, so will need to print multiple dashes.
			 * Figure out how to spread out */
			numdashes = (int) ((end - start) / (8.0 * dashwidth));
			spacebetween = ((end - start) - (dashwidth * numdashes))
					/ numdashes;
			
			for (  ; numdashes > 0; numdashes--) {
				pr_string(start +
					(numdashes - 0.5) * spacebetween
					+ ((numdashes - 1.0) * dashwidth),
					y, dashstring, J_LEFT,
					(char *) 0, -1);
			}
		}
	}
	else {
		/* if long enough to bother drawing underscore, draw it */
		if (end - start > Stepsize) {
			/* Temporarily change the effective staffscale
			 * to compensate for lyrics size. See comment in
			 * extend() in prntdata.c for more description. */
			float saved_staffscale;
			saved_staffscale = Staffscale;
			Staffscale *= ((float) size / Staffscale) / (float) DFLT_SIZE;
			do_linetype(L_NORMAL);
			draw_line(start, y, end, y);
			Staffscale = saved_staffscale;
		}
	}
}


/* Return YES if last character of syllable is an underscore or dash,
 * NO if it isn't.
 */

int
has_extender(syl)

char *syl;	/* the syllable to check */

{
	switch (last_char(syl)) {

	case '_':
	case '-':
		return(YES);

	default:
		return(NO);
	}
}


/* Return last character in a string.
 * If last character is a music character,
 * or the string is null, return null.
 */

int
last_char(str)

char *str;	/* return last character in this string */

{
	int font, size;
	int ch;			/* current character in string */
	int last_font = FONT_UNKNOWN;	/* font of last character */
	int last_ch = '\0';


	if (str == (char *) 0) {
		return('\0');
	}

	font = str[0];
	size = str[1];

	/* keep track of each character. When we hit
	 * end of string, return the last character we saw */
	for ( str += 2; (ch = next_str_char(&str, &font, &size)) != 0;  ) {
		last_font = font;
		last_ch = ch;
	}
	/* music characters don't count */
	if (IS_MUSIC_FONT(last_font)) {
		return('\0');
	}
	return (last_ch & 0xff);
}


/* See if an underscore or dash will need to be carried to the following score.
 * If so, add an appropriate "syllable" at the beginning of that score */

void
cont_extender(mll_p, sylplace, verseno)

struct MAINLL *mll_p;	/* the syllable is hanging off of this STAFF */
int sylplace;		/* PL_ABOVE, etc */
int verseno;		/* verse number */

{
	struct GRPSYL *syl_p;	/* walk through GRPSYL list */
	struct GRPSYL *last_non_space_p;
	int last_ch;		/* last character of syllable */
	int font;		/* of syllable */
	int size;		/* of syllable */


	if (mll_p->str != S_STAFF) {
		pfatal("cont_extender called with wrong argument");
	}

	/* Find the actual syl grpsyl that is the last on the score */
	syl_p = find_verse_place(mll_p->u.staff_p, verseno, sylplace);

	if (syl_p == 0) {
		pfatal("cont_extender called without any syllable");
	}

	/* Find the final non-space syllable in the list  */
	last_non_space_p = 0;
	for (  ; syl_p != 0; syl_p = syl_p->next) {
		if (syl_p->grpcont != GC_SPACE) {
			last_non_space_p = syl_p;
		}
	}

	if (last_non_space_p == 0) {
		pfatal("cont_extender couldn't find non-space syllable");
	}

	last_ch = last_char(last_non_space_p->syl);
	if (last_ch != '-' && last_ch != '_') {
		pfatal("cont_extender called on syl without extender");
	}

	/* See if will carry over */
	if (spread_extender(last_non_space_p, mll_p, verseno, sylplace, NO)
								== YES) {

		/* determine proper font/size of
		 * carried over dash/underscore
		 * based on font/size at end of syllable */
		end_fontsize(last_non_space_p->syl, &font, &size);

		/* insert the syllable on next score */
		insert_carryover_syllable(mll_p,
					last_non_space_p->staffno, sylplace,
					verseno,
					(last_ch == '-' ? "-" : "_"),
					font, size);
	}
}


/* A dash or underscore needs to be carried over to the following score.
 * Search forward for the appropriate STAFF. If there is already a lyric
 * there for the sylplace and verseno, if its first syllable is a space,
 * change it to a dash or underscore as appropriate. 
 * If there is no lyric in that measure for the sylplace and verseno,
 * insert a measure long syllable of the appropriate type.
 * If there is no STAFF of the proper number after a FEED, assume we are
 * at the end of the piece or of visibility of the staff, and do nothing.
 * If there is already a syllable, leave it as is.
 */


static void
insert_carryover_syllable(mll_p, staffno, sylplace, verseno, dash_or_underscore,
		font, size)

struct MAINLL *mll_p;	/* points to staff info */
int staffno;		/* staff number */
int sylplace;		/* PL_ABOVE, etc */
int verseno;		/* verse number */
char *dash_or_underscore;	/* "-" or "_" */
int font;		/* font and size to use for dash or underscore */
int size;

{
	struct STAFF *staff_p;	/* add syllable to this staff */
	struct CHORD *chord_p;	/* chord syllables goes with */
	int v;			/* verse index */
	float begin_x;		/* where to start carryover syllable */


	/* search forward for FEED */
	for (   ; mll_p != (struct MAINLL *) 0; mll_p = mll_p->next) {
		if (IS_CLEFSIG_FEED(mll_p)) {
			break;
		}
	}

	if (mll_p == (struct MAINLL *) 0) {
		return;
	}

	/* The AE coordinates of syllable groups
	 * have already been set, but we need to have
	 * this one set for the underscore/dash syllable being added. So deduce
	 * where it should be using the pseudo-bar */
	if ((mll_p = mll_p->next) == (struct MAINLL *) 0) {
		return;
	}
	if (mll_p->str == S_CLEFSIG) {
		begin_x = mll_p->u.clefsig_p->bar_p->c[AE] + STDPAD;
	}
	else {
		/* setting begin_x is just to shut up compilers that erroneously
		 * think it could be used without being set. */
		begin_x = 0.0;
		pfatal("no clefsig after feed");
	}

	/* silence compilers that think chord_p might not be set */
	chord_p = (struct CHORD *) 0;

	/* search forward for STAFF of interest, and save CHORD info */
	for ( mll_p = mll_p->next; mll_p != (struct MAINLL *) 0;
							mll_p = mll_p->next) {

		if (mll_p->str == S_CHHEAD) {
			chord_p = mll_p->u.chhead_p->ch_p;
		}
		else if (mll_p->str == S_STAFF) {
			if (mll_p->u.staff_p->staffno == staffno) {
				break;
			}
		}
	}

	/* see if has syllable of specified place and verse */
	if (mll_p != (struct MAINLL *) 0) {

		staff_p = mll_p->u.staff_p;
		for (v = 0; v < staff_p->nsyllists; v++) {

			if (staff_p->sylplace[v] == sylplace &&
					staff_p->syls_p[v]->vno == verseno) {

				/* are lyrics in this measure. See if first
				 * syllable is a space. If so, replace with
				 * a dash. Otherwise we are done */
				if (staff_p->syls_p[v]->syl == (char *) 0) {
					staff_p->syls_p[v]->syl =
						copy_string(dash_or_underscore,
								font, size);
					/* no longer a "space" syllable */
					staff_p->syls_p[v]->grpcont = GC_NOTES;
				}
				return;
			}
		}

		/* no lyrics in first measure on next score for this
		 * verse/place. Need to insert one */
		add_syllable(staff_p, sylplace, verseno,
					dash_or_underscore, font, size,
					begin_x, chord_p);
	}
}


/* Add a dash or underscore syllable to list of lyrics. Need to alloc new
 * space for the sylplace and syls_p arrays, copy the existing data into
 * them, adding the new syllable at the proper place (sorted by verseno),
 * then free the old arrays */

static void
add_syllable(staff_p, sylplace, verseno, dash_or_underscore, font, size,
		begin_x, chord_p)

struct STAFF *staff_p;		/* add syllable to this staff */
int sylplace;			/* PL_ABOVE, etc */
int verseno;
char *dash_or_underscore;	/* "-" or "_" */
int font;
int size;
double begin_x;			/* where syllable is to start */
struct CHORD *chord_p;		/* what chord to attach to */

{
	short *new_sylplace;		/* new, expanded sylplace array */
	struct GRPSYL **new_syls_p;	/* new, expanded syls_p array */
	int v;				/* verse index */
	int insert_index;		/* where to put in new arrays */
	int inserted;			/* 0 if haven't found where to insert
					 * yet, 1 if we have. This is then the
					 * difference between the index of the
					 * original arrays and where the copy
					 * goes in the new arrays. Since it's
					 * used in array subscript calculation
					 * we can't use YES and NO here */

	
	/* alloc arrays that are one larger than the current arrays */
	MALLOCA(short, new_sylplace, staff_p->nsyllists + 1);
	MALLOCA(struct GRPSYL *, new_syls_p, staff_p->nsyllists + 1);

	/* now copy and insert */
	insert_index = staff_p->nsyllists;
	for (inserted = v = 0; v < staff_p->nsyllists; v++) {
		if (insert_index > v && staff_p->syls_p[v]->vno > verseno) {
			/* insert here */
			insert_index = v;
			inserted = 1;
		}

		new_sylplace[v + inserted] = staff_p->sylplace[v];
		new_syls_p[v + inserted] = staff_p->syls_p[v];
	}

	/* alloc and fill in the new GRPSYL */
	new_sylplace[insert_index] = (short) sylplace;
	new_syls_p[insert_index] = newGRPSYL(GS_SYLLABLE);
	new_syls_p[insert_index]->syl = copy_string(dash_or_underscore,
							font, size);
	new_syls_p[insert_index]->inputlineno = -1;
	new_syls_p[insert_index]->basictime = -1;
	new_syls_p[insert_index]->is_multirest = NO;
	new_syls_p[insert_index]->is_meas = YES;
	new_syls_p[insert_index]->fulltime = Score.time;
	new_syls_p[insert_index]->staffno = staff_p->staffno;
	new_syls_p[insert_index]->vno = (short) verseno;
	/* X coords of normal syllables already set, so have to set for
	 * this special syllable here */
	new_syls_p[insert_index]->c[AE] = begin_x
				+ strwidth(new_syls_p[insert_index]->syl);
	new_syls_p[insert_index]->c[AW] = begin_x;
	new_syls_p[insert_index]->c[AX] = begin_x;

	/* now have one one list of syllables */
	(staff_p->nsyllists)++;

	/* free old arrays if non-null */
	if (staff_p->sylplace != (short *) 0) {
		FREE(staff_p->sylplace);
	}
	if (staff_p->syls_p != (struct GRPSYL **) 0) {
		FREE(staff_p->syls_p);
	}

	/* now link up the new arrays */
	staff_p->sylplace = new_sylplace;
	staff_p->syls_p = new_syls_p;

	/* add to appropriate chord */
	stitch_syl_into_chord(chord_p, new_syls_p[insert_index]);
}


/* Given a syllable and chord to attach it to, attach it */
/* Strictly speaking, this function probably isn't necessary, since I think
 * all use of the CHORD struct has already been done before this function is
 * called, but it's probably good to do anyway on general principles, in case
 * some day the CHORDs are looked at later */

static void
stitch_syl_into_chord(chord_p, syl_gs_p)

struct CHORD *chord_p;		/* add to this chord */
struct GRPSYL *syl_gs_p;	/* add this syllable */

{
	struct GRPSYL *gs_p;	/* walk through chord */


	/* go down chord list */
	for (gs_p = chord_p->gs_p; gs_p->gs_p != (struct GRPSYL *) 0;
							gs_p = gs_p->gs_p) {

		/* if next grpsyl in chord has staffno < staffno of syl to add,
		 * keep going */
		if (gs_p->gs_p->staffno < syl_gs_p->staffno) {
			continue;
		}

		/* if next grpsyl in chord had staffno > staffno of syl to add,
		 * put it here */
		if (gs_p->gs_p->staffno > syl_gs_p->staffno) {
			/* found where to insert */
			break;
		}

		/* If here, must be same staffno.
		 * Keep going until find syllable with larger vno. */
		if (gs_p->gs_p->grpsyl == GS_GROUP) {
			continue;
		}

		if (gs_p->gs_p->vno > syl_gs_p->vno) {
			/* found where to insert */
			break;
		}
	}

	/* insert syllable */
	syl_gs_p->gs_p = gs_p->gs_p;
	gs_p->gs_p = syl_gs_p;
}
