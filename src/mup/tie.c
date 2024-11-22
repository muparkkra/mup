
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

/* This file contains functions for handling ties and slurs.
 * This includes doing error checking to make sure
 * there is a note to tie/slur to.
 * There is also code to add padding to make space for ties/slurs
 * that are carried into second endings. */

#include "defines.h"
#include "structs.h"
#include "globals.h"



/* lengths of line segments of carried in tie marks */
#define HALFTIEPAD	(3 * STEPSIZE)

/* infomation about a tied or slurred note that gets carried into an ending */
struct TIECARRY {
	short	letter;		/* pitch to be tied or slurred to */
	short	octave;		/* octave of the pitch */
	short	fretno;		/* for tab, This is only used to give a
				 * better error message when user tries to
				 * to tie to a note that isn't in the
				 * next chord. */
	short	curveno;	/* slurto index or -1 for tie */
	short	is_bend;	/* if slurto is actually a bend */
	struct MAINLL *mll_p;	/* points to first group */
	struct GRPSYL *gs_p;	/* group of first note */
	struct TIECARRY *next;	/* linked list */
};

/* linked list of tie carry info for each staff/voice */
struct TIECARRY *Tiecarryinfolist_p [MAXSTAFFS + 1] [MAXVOICES];

/* flag to mark if there are any carry ins */
static short Have_carry_ins = NO;


/* static functions */
static void do_tie P((struct MAINLL *mll_p));
static void do_group_ties P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
static void chk_slides P((struct NOTE *note_p, struct GRPSYL *gs_p,
		struct MAINLL *mll_p));
static void chk_following4slide P((struct NOTE *note_p, struct GRPSYL *gs_p,
		struct MAINLL *mll_p));
static int give_tie_warning P((struct GRPSYL *gs_p, struct MAINLL *mll_p,
		struct NOTE * note_p));
static struct NOTE * prev_matching_note P((struct GRPSYL *gs_p,
		struct MAINLL *mll_p, int n));
static struct MAINLL *do_carry_ties P((struct MAINLL *staff_mll_p,
		struct MAINLL *bar_mll_p));
static void savetieinfo P((struct MAINLL *mll_p, struct GRPSYL *gs_p));
static void do_save_tieinfo P((int staffno, int vno, int letter, int octave,
		int fretno, int is_tie, struct MAINLL *mainll_p,
		struct GRPSYL *gs_p, int is_bend));
static void carryin_ties P((struct MAINLL *mll_p));
static void add_carryin P((struct STAFF *staff_p));
static void free_carryin_info P((void));
static void free_cinfo P((struct TIECARRY *carryinfo_p));
static void chk4xpose P((struct MAINLL *mll_p));
static void chkxpstaff P((struct MAINLL *mll_p, int s));
static void chkxpgrp P((struct GRPSYL *gs_p, char *inputfile, int lineno));
static void set_inhibitprint_if_appropriate P((struct GRPSYL *gs_p,
		struct MAINLL *mll_p));



/* go through main list, checking each STAFF struct for ties and slurs.
 * For each, do appropriate error checking */

void
tie()

{
	struct MAINLL * mll_p;	/* walk through main list */


	debug(2, "tie");

	/* first check for any ties across transpositions, and delete them.
	 * Can't do inside the next loop, because by then a less informative
	 * message could be generated for the error */
	initstructs();
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
				mll_p = mll_p->next) {
		if (mll_p->str == S_SSV) {
			chk4xpose(mll_p);
			asgnssv(mll_p->u.ssv_p);
		}
	}

	/* go through the main list again for other checks */
	initstructs();
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
				mll_p = mll_p->next) {
	
		/* process any GRPSYL lists with note info */
		if (mll_p->str == S_STAFF) {
			if ( (svpath(mll_p->u.staff_p->staffno, VISIBLE))
						->visible == YES) {
				do_tie(mll_p);
			}
		}
		else if (mll_p->str == S_SSV) {
			asgnssv(mll_p->u.ssv_p);
		}
	}
}


/* do the ties and slurs on all groups off a STAFF struct */

static void
do_tie(mll_p)

struct MAINLL *mll_p;	/* the STAFF struct with list of GRPSYLs */

{
	struct GRPSYL *gs_p;		/* walk through GRPSYL list */
	int v;				/* walk through voices per staff */
	int is_tab;			/* YES if tablature staff */


	is_tab = is_tab_staff(mll_p->u.staff_p->staffno);

	for (v = 0; v < MAXVOICES; v++) {
		/* go through entire list of GRPSYLs */
		for (gs_p = mll_p->u.staff_p->groups_p[v];
					gs_p != (struct GRPSYL *) 0;
					gs_p = gs_p->next) {

			/* error check */
			if (gs_p->grpcont != GC_NOTES) {
				if (gs_p->tie == YES) {
					l_warning(mll_p->inputfile,
						mll_p->inputlineno,
					 	"tie can only apply to notes");
					gs_p->tie = NO;
				}

				/* if rest or space, nothing more to do */
				continue;
			}

			do_group_ties(gs_p, mll_p);
			if (is_tab == YES) {
				set_inhibitprint_if_appropriate(gs_p, mll_p);
			}
		}
	}
}


/* do ties on all notes in a group that have ties. While we're at it, also
 * make sure any staffs with clefs not printed, don't have accidentals on any
 * notes  */

static void
do_group_ties(gs_p, mll_p)

struct GRPSYL *gs_p;		/* do ties from this group */
struct MAINLL *mll_p;		/* points to gs_p */

{
	struct GRPSYL *gs2_p;		/* group to tie to */
	register int n;			/* walk through note list */
	struct NOTE *note1_p;		/* info about note */
	struct NOTE *note2_p;		/* note slurred to */
	int slur;			/* index into slurtolist */
	int d;				/* index for deleting illegal slurs */
	short err = NO;			/* error flag */


	/* go through all notes in group, looking for ties */
	gs2_p = (struct GRPSYL *) 0;
	for (n = 0; n < gs_p->nnotes; n++) {

		/* For staffs without clef, make sure there are no accidentals.
		 * But tab staffs keep accidentals, even with no clef.
		 * Philosophically, this isn't really a good place to
		 * do this, but since we're going through the list of notes
		 * anyway here, and time-wise in the scheme of the program
		 * this is the right time, rather than make yet another trip
		 * through the list later we do it here. But for MIDI
		 * purposes, we need to keep the accidental, or the wrong
		 * note will play! */
		if (svpath(gs_p->staffno, STAFFLINES)->printclef != SS_NORMAL
				&& is_tab_staff(gs_p->staffno) == NO
				&& Doing_MIDI == NO) {
			 standard_to_acclist( '\0', gs_p->notelist[n].acclist);
		}

		note1_p = &(gs_p->notelist[n]);
		/* if this note's tie flag is set, check the tie */
		if ( note1_p->tie == YES) {

			if (note1_p->tied_to_voice == gs_p->vno) {
				l_warning(gs_p->inputfile, gs_p->inputlineno,
				"the target voice of 'to voice' is the same as the current voice, so the 'to voice %d' is extraneous", note1_p->tied_to_voice);
				note1_p->tied_to_voice = NO_TO_VOICE;
			}

			if (note1_p->tied_to_voice != NO_TO_VOICE) {
				/* tying to a different voice, so force
			 	 * looking up next group, rather than relying
				 * on cached value */
				gs2_p = 0;
			}
			/* if haven't yet found the group to tie to, do that.
			 * (If this is not the first note to be tied
			 * in this group, we would have already found
			 * the other group) */
			if (gs2_p == (struct GRPSYL *) 0) {
				gs2_p = find_to_group(mll_p, gs_p, 
						note1_p->tied_to_voice, "tie");
			}

			if (gs2_p == (struct GRPSYL *) 0) {
				/* if nothing to tie to, cancel the tie. We
				 * will have already printed an error msg. */
				note1_p->tie = NO;
				gs_p->tie = NO;
			}
			else {
				if ((note2_p = find_matching_note(gs2_p, 0,
						note1_p->letter,
						note1_p->FRETNO,
						note1_p->octave, "tie"))
						== (struct NOTE *) 0) {
					note1_p->tie = NO;
					gs_p->tie = NO;
				}
				else if (note1_p->tied_to_voice != NO_TO_VOICE) {
					note2_p->tied_from_other = YES;
				}
			}
			/* If there was a 'to voice,' we invalidate the
			 * cached gs2_p. If the next use of gs2_p happens
			 * to be for the same 'to voice,' we will end up
			 * looking it up again, but it seems better to not
			 * try to be overly clever for such a rare case.
			 * This will let us optimize the most common case
			 * of no 'to voice,' by keeping gs2_p cached,
			 * while avoiding all the complication
			 * of trying to optimize rarer cases. */
			if (note1_p->tied_to_voice != NO_TO_VOICE) {
				gs2_p = 0;
			}
		}


		/* handle all slurs from current note */
		for (slur = 0; slur < note1_p->nslurto; slur++) {

			/* slides to/from nowhere don't get processed here */
			if (IS_NOWHERE(note1_p->slurtolist[slur].octave)) {
				continue;
			}

			if (note1_p->slurtolist[slur].slurred_to_voice == gs_p->vno) {
				l_warning(gs_p->inputfile, gs_p->inputlineno,
				"the target voice of 'to voice' is the same as the current voice, so the 'to voice %d' is extraneous",
				note1_p->slurtolist[slur].slurred_to_voice);
				note1_p->slurtolist[slur].slurred_to_voice = NO_TO_VOICE;
			}

			if (note1_p->slurtolist[slur].slurred_to_voice != NO_TO_VOICE) {
				/* slurring to a different voice, so force
			 	 * looking up next group, rather than relying
				 * on cached value */
				gs2_p = 0;
			}
			/* if haven't yet found the group to slur to, do that.
			 * (We may have already found it earlier) */
			if (gs2_p == (struct GRPSYL *) 0) {
				gs2_p = find_to_group(mll_p, gs_p,
					note1_p->slurtolist[slur].slurred_to_voice,
					"slur");
			}

			if (gs2_p == (struct GRPSYL *) 0) {
				/* if nothing to slur to, cancel all slurs */
				note1_p->nslurto = 0;
				continue;
			}

			/* special case of 'M' when a group 'slur'
		 	 * has been specified. Find matching note
			 * in the second chord */
			if (note1_p->slurtolist[slur].letter == 'M') {
				if (gs_p->nnotes != gs2_p->nnotes) {
					/* only print message first time */
					if (err == NO) {
						l_warning(gs_p->inputfile,
							gs_p->inputlineno,
							"'slur' requires equal number of notes in each chord");
					}
					note2_p = (struct NOTE *) 0;

					/* don't do any more on this
					 * chord, to avoid multiple
					 * error messages */
					err = YES;
				}
				else {
					note2_p = & (gs2_p->notelist[n]);
				}
			}
			else {
				note2_p = find_matching_note(gs2_p, 0,
					note1_p->slurtolist[slur].letter,
					-1,
					note1_p->slurtolist[slur].octave,
					note1_p->is_bend ? "bend" : "slur");
			}

			if (note2_p != (struct NOTE *) 0) {
				/* fill in the letter/octave if they had to
				 * be derived */
				if ((note1_p->slurtolist[slur].letter == 'U')
					 || (note1_p->slurtolist[slur].letter == 'M')
					 || is_tab_staff(gs_p->staffno) == YES) {
				    note1_p->slurtolist[slur].letter =
							note2_p->letter;
				    note1_p->slurtolist[slur].octave =
							note2_p->octave;
				}
				if (note1_p->slurtolist[slur].slurred_to_voice != NO_TO_VOICE) {
					note2_p->slurred_from_other = YES;
				}
			}
			else {
				/* discard this slur--
				 * nothing to slur to */
				for (d = slur + 1; d < note1_p->nslurto; d++) {
					note1_p->slurtolist[d-1] =
						note1_p->slurtolist[d];
				}
				(note1_p->nslurto)--;
			}
			if (note1_p->slurtolist[slur].slurred_to_voice != NO_TO_VOICE) {
				gs2_p = 0;
			}
		}

		/* do additional slide checks for tab and tabnote */
		if (is_tab_staff(gs_p->staffno) == YES
				|| (gs_p->staffno < MAXSTAFFS
				&& is_tab_staff(gs_p->staffno + 1) == YES)) {
			chk_slides(note1_p, gs_p, mll_p);
		}
	}
}

/* do extra checks for slide. There can be no more than one incoming and one
 * outgoing slide for any given note */

static void
chk_slides(note_p, gs_p, mll_p)

struct NOTE *note_p;		/* this note might have slides */
struct GRPSYL *gs_p;		/* note is in this group */
struct MAINLL *mll_p;		/* group is tied to this main list struct */

{
	int s;		/* index through slurtolist */
	int incoming = 0, outgoing = 0;	/* how many slides of each type */


	/* go through list counting up incoming and outgoing slides */
	for (s = 0; s < note_p->nslurto; s++) {
		switch(note_p->slurtolist[s].octave) {

		case OUT_UPWARD:
		case OUT_DOWNWARD:
			outgoing++;
			break;

		case IN_UPWARD:
		case IN_DOWNWARD:
			incoming++;
			break;

		default:
			outgoing++;
			/* make sure following group doesn't have any
			 * incoming nowhere slides */
			chk_following4slide(note_p, gs_p, mll_p);
			break;
		}
	}

	if (incoming > 1) {
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
			"can't have more than one slide into a note");
	}
	if (outgoing > 1) {
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
			"can't have more than one slide from a note");

	}
}


/* Given a note with a slide to a specific fret,
 * if there is a following group, see if it has a matching note,
 * and if so, check that note's slurtolist to see if it
 * contains any incoming nowhere slides. If so, there is a
 * problem, because we already have a slide to that note */

static void
chk_following4slide(note_p, gs_p, mll_p)

struct NOTE *note_p;	/* this note has a slide to a specific fret */
struct GRPSYL *gs_p;	/* note is in this group */
struct MAINLL *mll_p;	/* group is attached to this main list struct */

{
	struct GRPSYL *ngs_p;	/* next group */
	int n;			/* index through notes */
	int ns;			/* index through slides on next group note */


	if ((ngs_p = nextgrpsyl(gs_p, &mll_p)) == (struct GRPSYL *) 0) {
		/* no next group, so no problem */
		return;
	}

	/* check each note in next group */
	for (n = 0; n < ngs_p->nnotes; n++) {
		/* is this the matching note?  If the letter matches and
		 * either it's a tab staff or the octave also matches,
		 * then it is the matching note. */
		if (ngs_p->notelist[n].letter == note_p->letter &&
				(is_tab_staff(gs_p->staffno) ||
				ngs_p->notelist[n].octave == note_p->octave)) {

			/* found the matching note. Check its slurtolist */
			for (ns = 0; ns < ngs_p->notelist[n].nslurto; ns++) {
				switch (ngs_p->notelist[n].slurtolist[ns].octave) {
				case IN_UPWARD:
				case IN_DOWNWARD:
					l_yyerror(gs_p->inputfile,
						gs_p->inputlineno,
						"can't slide to note that has </n> or <\\n>");
					break;
				default:
					break;
				}
			}
		}
	}
}


/* find note in following chord having specified pitch/octave.
 * If the note is found, return pointer to it, otherwise 0
 * Note that when this function is called before makechords(), it must
 * pass a mll_p of 0, so it won't attempt to find a CHORD that hasn't
 * been created yet. It can also be passed 0 if you know you don't care
 * about warnings for accidentals, because that is all it is used for.
 */

struct NOTE *
find_matching_note(gs_p, mll_p, letter, fretno, octave, type)

struct GRPSYL *gs_p;	/* which GRPSYL we're tying to */
struct MAINLL *mll_p;	/* Where GRPSYL attaches, or 0 if before placement */
int letter;		/* find note with this pitch, 'a' to 'g' */
int fretno;		/* for tab. Caller can pass -1 if they don't know,
			 * and we'll make do with not checking fretno,
			 * which is probably fine for slurs anyway, which
			 * should be only the case when we don't know. */
int octave;		/* find note with this octave */
char *type;		/* "tie", "slur", "slide", or "bend",
			 * or null if not to print any error messages */

{
	struct NOTE *note2_p;	/* note to tie to */
	register int n2;	/* index through notelist of 2nd group */


	if (gs_p == (struct GRPSYL *) 0) {
		return( (struct NOTE *) 0);
	}

	/* we don't allow tie/slur into a measure repeat. */
	if (is_mrpt(gs_p) == YES) {
		l_warning(gs_p->inputfile, gs_p->inputlineno,
			"tie/slur/bend not allowed into measure rpt");
		return (struct NOTE *) 0;
	}

	/* special case. On slurto, if second group has only a single
	 * note, user doesn't have to specify it. We will have marked the
	 * pitch as 'U'. If second group has only one note in it, use that
	 * one. If not, error */
	if ( letter == 'U') {
		if ( gs_p->nnotes != 1) {
			if (type != (char *) 0) {
				l_warning(gs_p->inputfile, gs_p->inputlineno,
					"note to %s to not specified", type);
			}
			return(struct NOTE *) 0;
		}
		else {
			return( &(gs_p->notelist[0]) );
		}
	}

	/* try to find matching note in second note group */
	/* If first note has an accidental and the corresponding one in
	 * the next group doesn't, that's a
	 * match, 'cause we only print the accidental once.
	 */
	for (n2 = 0; n2 < gs_p->nnotes; n2++) {

		note2_p = &(gs_p->notelist[n2]);

		if (is_tab_staff(gs_p->staffno) == YES) {
			/* On tab staff, we just have to match
			 * the string number, unless this is for a tie,
			 * in which case we should match fret too.
			 */
			if (note2_p->letter == letter) {
				if (type != 0 && (strcmp(type, "tie") == 0)) {
					if (fretno != -1 && note2_p->FRETNO != fretno) {
						return(0);
					}

					/* Ties and bends are sort of the
					 * same thing, so having both seems
					 * redundant, so give a warning
					 * for that. */
					if (HASBEND(*note2_p) ){
						l_warning(gs_p->inputfile, gs_p->inputlineno,
						"should not have both tie and bend");
						return(0);
					}
				}

				return(note2_p);
			}
			else {
				continue;
			}
		}

		if ( (note2_p->letter == letter)
				&& (note2_p->octave == octave) ) {

			if (type != (char *) 0 && (strcmp(type, "tie") == 0)
					&& (standard_acc(note2_p->acclist) != '\0')
					&& is_tab_staff(gs_p->staffno) == NO) {
				if (give_tie_warning(gs_p, mll_p, note2_p) == YES) {
					l_warning(gs_p->inputfile, gs_p->inputlineno,
						"second note of tie not allowed to have an accidental");
					/* fix it so in case we're called again on the
					 * same note (which is possible), we'll only
					 * print one error message */
					standard_to_acclist('\0', note2_p->acclist);
				}
			}

			/* found it! */
			return(note2_p);
		}
	}

	/* oh-oh. User goofed */
	if (is_tab_staff(gs_p->staffno) == YES) {
		if (type != (char *) 0) {
			l_yyerror(gs_p->inputfile, gs_p->inputlineno,
				"can't do %s: %s string not in chord%s",
				type, stringname(letter, gs_p->staffno),
				gs_p->nnotes == 0 ?
				" (in fact no strings at all)" : "");
		}
	}
	/* Normally we should not give errors on tabnote staff,
	 * because if there is a problem, we will get an error
	 * on the tab staff, and don't want to give double errors.
	 * So that could be done with
	 *	else if (is_tab_staff(gs_p->staffno + 1) == NO) {
	 * But that is only sure to be true if tabnote was derived,
	 * and we really don't know if it was derived or not.
	 * And we might also miss some errors if the tab staff was invisible.
	 * Or if there was a bug in the tab staff error checking.
	 * So it seems better to double report than miss something.
	 */
	else {
		if (type != (char *) 0) {
			if (letter < 'a' || letter > 'g' || octave < MINOCTAVE
						|| octave > MAXOCTAVE) {
				l_warning(gs_p->inputfile, gs_p->inputlineno,
					"can't do %s: note not in chord",
					type);
			}
			else {
				l_warning(gs_p->inputfile, gs_p->inputlineno,
					"can't do %s: %c%d not in chord%s",
					type, letter, octave,
					gs_p->nnotes == 0 ?
					" (in fact no notes at all)" : "");
			}
		}
	}
	return( (struct NOTE *) 0 );
}


/* Find the group to tie or slur to. Usually this will be in just the next
 * group in the current voice, but in the case of "to voice N" it has to
 * find the group at the correct time in that other voice. */

struct GRPSYL *
find_to_group(mll_p, gs_p, to_voice, type)

struct MAINLL *mll_p;		/* current place in main list */
struct GRPSYL *gs_p;		/* group to tie from */
int to_voice;			/* which voice to find the group in */
char *type;			/* "tie" or "slur" */

{
	RATIONAL this_end_time;	/* time in measure where gs_p ends */
	RATIONAL other_start_time;/* to find matching time in other voice */
	struct GRPSYL *tgs_p;	/* walk through this voice */
	struct GRPSYL *ogs_p;	/* walk through other voice */

	if (to_voice == NO_TO_VOICE) {
		/* The most normal case, going to same voice */
		return(find_next_group(mll_p, gs_p, type));
	}

	/* Figure out where in time the end of this note is */
	this_end_time = gs_p->fulltime;
	for (tgs_p = gs_p->prev; tgs_p != 0; tgs_p = tgs_p->prev) {
		this_end_time = radd(this_end_time, tgs_p->fulltime);
	}

	/* Now find the note at that time in the target voice */
	other_start_time = Zero;
	for (ogs_p = mll_p->u.staff_p->groups_p[to_voice-1];
					ogs_p != 0; ogs_p = ogs_p->next) {
		if (EQ(other_start_time, this_end_time)) {
			/* found it */
			return(ogs_p);
		}
		/* If from group was the last group in the measure,
		 * and this is the last group in the other voice,
		 * we want the group after this one */
		if (gs_p->next == 0 && ogs_p->next == 0) {
			if ((ogs_p = nextgrpsyl(ogs_p, &mll_p)) != 0) {
				return(ogs_p);
			}
			else {
				/* jump out to print error */
				break;
			}
		}
		if (GT(other_start_time, this_end_time)) {
			/* too far */
			break;
		}
		other_start_time = radd(other_start_time, ogs_p->fulltime);
	}

	l_yyerror(gs_p->inputfile, gs_p->inputlineno,
		"can't %s to voice %d; no chord at the correct time in that voice",
		type, to_voice);
	return(0);
}


/* given one GRPSYL, find the next one in the same staff and voice,
 * which might be in the next measure */

struct GRPSYL *
find_next_group(mll_p, gs_p, type)

struct MAINLL *mll_p;		/* current place in main list */
struct GRPSYL *gs_p;		/* group to tie from */
char *type;			/* "tie" or "slur" */

{
	struct MAINLL *ml_p;

	ml_p = mll_p;
	if ((gs_p = nextgrpsyl(gs_p, &ml_p)) == (struct GRPSYL *) 0) {
		l_warning(mll_p->inputfile, mll_p->inputlineno,
						"no chord to %s to", type);
	}

	return(gs_p);
}


/* go through main list. If we hit a bar that begins an ending, back up
 * and go through the previous measure. If the final group of any voice
 * has any tied or slurred notes, save information about them. Then for
 * each additional beginning of an ending up until an endending, add
 * user padding to allow for carried in tie mark. At the endending, free
 * the information and continue through the rest of the main list */

void
tie_carry()

{
	struct MAINLL *mll_p;			/* walk through main list */
	struct MAINLL *first_staff_mll_p;	/* points to first STAFF
						 * struct of measure */


	debug(2, "tie_carry");

	initstructs();
	first_staff_mll_p = (struct MAINLL *) 0;
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {
		switch (mll_p->str) {

		case S_STAFF:
			/* remember where list of staffs begins and skip
			 * the rest of the STAFFs */
			first_staff_mll_p = mll_p;
			for (   ; mll_p->next->str == S_STAFF;
						mll_p = mll_p->next) {
				;
			}
			break;

		case S_BAR:
			if (mll_p->u.bar_p->endingloc == STARTITEM) {
				mll_p = do_carry_ties(first_staff_mll_p, mll_p);
			}
			break;

		case S_CLEFSIG:
			/* actually, it should be impossible to hit this case,
			 * because clefsigs with pseudo-bar haven't been
			 * created yet at the time this is called, but if things
			 * are changed some day so things get done in a different
			 * order, this should then work. */
			if (mll_p->u.clefsig_p->bar_p != (struct BAR *) 0 &&
					mll_p->u.clefsig_p->bar_p->endingloc
					== STARTITEM) {
				mll_p = do_carry_ties(first_staff_mll_p, mll_p);
			}
			break;

		case S_SSV:
			asgnssv(mll_p->u.ssv_p);
			break;
		default:
			break;
		}
	}
}


/* Second note of tie is not allowed to have an accidental,
 * because it is always implied.  However, if user had this same note in a
 * different voice that was not tied, then it is okay for that one to have an
 * accidental, even required, and sometimes placement will have "moved"
 * the accidental to this note to ensure accidentals are
 * to the left of all groups. So we try our best to detect
 * that special case and not give a warning for that.
 * It is possible we might not give a warning in cases when
 * we really should, but that should be very rare,
 * and it's only a warning anyway.
 */

static int
give_tie_warning(gs_p, mll_p, note_p)

struct GRPSYL *gs_p;	/* check this group */
struct MAINLL *mll_p;	/* gs_p hangs off of here. 0 means no CHORDs yet */
struct NOTE *note_p;	/* this is the note being tied. */

{
	struct CHORD *chord_p;	/* chord containing the gs_p */
	struct GRPSYL *ogs_p;	/* other groups at this time on same staff */
	struct NOTE *pnote_p;	/* matching note in previous group */
	int n;			/* note index */


	/* This function is often called before placement, before CHORDs
	 * are set up. We know that those are not the special case where
	 * we turn off the warning. */
	if (mll_p == 0) {
		return(YES);
	}

	if ((chord_p = gs2ch(mll_p, gs_p)) != 0) {
		for (ogs_p = chord_p->gs_p; ogs_p != 0 && ogs_p->staffno <
				gs_p->staffno; ogs_p = ogs_p->gs_p) {
			;
		}
		for (   ; ogs_p != 0 && ogs_p->grpsyl == GS_GROUP &&
				ogs_p->staffno == gs_p->staffno;
				ogs_p = ogs_p->gs_p) {
			if (ogs_p == gs_p) {
				/* don't try to compare with ourself! */
				continue;
			}
			for (n = 0; n < ogs_p->nnotes; n++) {
				if (ogs_p->notelist[n].letter
						== note_p->letter &&
						ogs_p->notelist[n].octave
						== note_p->octave) {
					/* Another voice had this same note.
					 * If it wasn't tied, we should not
					 * give a warning. */
					if ((pnote_p = prev_matching_note(
							ogs_p, mll_p, n)) == 0
							|| pnote_p->tie == NO) {
						return(NO);
					}
				}
			}
		}
	}
	return(YES);
}


/* Find and return note, if any, in the group previous to gs_p that has the
 * same letter and octave. If none, returns 0. */

static struct NOTE *
prev_matching_note(gs_p, mll_p, n)

struct GRPSYL *gs_p;	/* find matching note in group before this one */
struct MAINLL *mll_p;	/* gs_p hangs off of here */
int n;			/* find match for the nth note in gs_p */

{
	struct MAINLL *pmll_p;	/* for passing to prevgrpsyl */
	struct GRPSYL *pgs_p;	/* the group before gs_p */
	int pn;			/* note index in pgs_p */

	pmll_p = mll_p;
	if ((pgs_p = prevgrpsyl(gs_p, &pmll_p)) != 0) {
		for (pn = 0; pn < pgs_p->nnotes; pn++) {
			if (pgs_p->notelist[pn].letter
					== gs_p->notelist[n].letter &&
					pgs_p->notelist[pn].octave
					== gs_p->notelist[n].octave) {
				/* Found matching note. Return it */
				return( &(pgs_p->notelist[pn]) );
			}
		}
	}
	/* No matching note */
	return(0);
}


/* Save info about any ties and slurs on the last chords before the beginning
 * of the ending. Then search forward in main list. If there are any more
 * beginnings of endings, add padding to the appropriate groups.
 * Return MAINLL at the end of the last ending processed. */

static struct MAINLL *
do_carry_ties(staff_mll_p, bar_mll_p)

struct MAINLL *staff_mll_p;	/* first staff in measure which ends on
				 * bar that begins an ending */
struct MAINLL *bar_mll_p;	/* the bar that begins an ending */

{
	struct MAINLL *mll_p;	/* walk through list of staffs */
	int v;			/* voice number */


	/* save all the tie / slur info */
	for (mll_p = staff_mll_p; mll_p->str == S_STAFF; mll_p = mll_p->next) {

		for (v = 0; v < MAXVOICES; v++) {
			savetieinfo(mll_p, mll_p->u.staff_p->groups_p[v]);
		}
	}

	/* now search ahead for other endings */
	for (mll_p = bar_mll_p->next; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {
		if (mll_p->str != S_BAR) {
			continue;
		}

		switch (mll_p->u.bar_p->endingloc) {
		case NOITEM:
		case ENDITEM:
			free_carryin_info();
			return(mll_p);
		case STARTITEM:
			carryin_ties(mll_p->next);
			break;
		default:
			break;
		}
	}
	pfatal("fell off end of list while doing tie carries");
	/*NOTREACHED*/
	return( (struct MAINLL *) 0);
}


/* given a GRPSYL, save info about any notes in it that have ties or slurs */

static void
savetieinfo(mll_p, gs_p)

struct MAINLL *mll_p;		/* main list struct that gs_p is connected to */
struct GRPSYL *gs_p;		/* save info about ties/slurs on last group
				 * in this list */

{
	int n;		/* note index */
	int s;		/* slurto index */


	if (gs_p == (struct GRPSYL *) 0) {
		return;
	}

	/* find last group in list */
 	for (  ; gs_p->next != (struct GRPSYL *) 0; gs_p = gs_p->next) {
		;
	}

	for (n = 0; n < gs_p->nnotes; n++) {

		/* save tie info */
		if (gs_p->notelist[n].tie == YES) {
			do_save_tieinfo(gs_p->staffno, gs_p->vno,
					gs_p->notelist[n].letter,
					gs_p->notelist[n].octave,
					gs_p->notelist[n].FRETNO, -1,
					mll_p, gs_p, NO);
		}

		/* save slurto info */
		for (s = 0; s < gs_p->notelist[n].nslurto; s++) {
			if (gs_p->notelist[n].slurtolist[s].letter == 'U') {
				struct NOTE *n_p;
				struct GRPSYL *ngs_p;
				ngs_p = find_next_group(mll_p, gs_p, "slur");
				n_p = find_matching_note(ngs_p, mll_p,
					gs_p->notelist[n].slurtolist[s].letter,
					-1, gs_p->notelist[n].octave, "slur");
				if (n_p != 0) {
                        		gs_p->notelist[n].slurtolist[s].letter = n_p->letter;
                        		gs_p->notelist[n].slurtolist[s].octave = n_p->octave;
				}
			}
			do_save_tieinfo(gs_p->staffno, gs_p->vno,
					gs_p->notelist[n].slurtolist[s].letter,
					gs_p->notelist[n].slurtolist[s].octave,
					-1, s, mll_p, gs_p,
					gs_p->notelist[n].is_bend);
		}
	}
}


/* save info about one tie or slur mark that will need to be carried into
 * subsequent endings. Malloc space for info, fill it in, and put into table */

static void
do_save_tieinfo(staffno, vno, letter, octave, fretno, curveno, mll_p, gs_p, is_bend)

int staffno;
int vno;
int letter;	/* a to g */
int octave;
int fretno;
int curveno;
struct MAINLL *mll_p;	/* points to first group */
struct GRPSYL *gs_p;	/* group of first note */
int is_bend;		/* YES if is actually a bend rather than slur */

{
	struct TIECARRY *new_p;

	MALLOC(TIECARRY, new_p, 1);
	new_p->letter = (short) letter;
	new_p->octave = (short) octave;
	new_p->curveno = (short) curveno;
	new_p->fretno = (short) fretno;
	new_p->is_bend = is_bend;
	new_p->mll_p = mll_p;
	new_p->gs_p = gs_p;
	new_p->next = Tiecarryinfolist_p [staffno] [vno - 1];
	Tiecarryinfolist_p [staffno] [vno - 1] = new_p;

	Have_carry_ins = YES;
}


/* Once an ending has been found that may have ties/slurs carried in, use
 * the saved information to add padding. */

static void
carryin_ties(mll_p)

struct MAINLL *mll_p;	/* look for staffs from here for chords that may have
			 * things tied or slurred in */

{
	if (Have_carry_ins == NO) {
		/* nothing to do */
		return;
	}

	/* skip everything up to STAFFS */
	for (  ; mll_p != (struct MAINLL *) 0; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			add_carryin(mll_p->u.staff_p);
		}
		else if (mll_p->str == S_BAR) {
			break;
		}
	}
}


/* given a STAFF which is at the beginning of an ending that may have ties/slurs
 * carried in, go through each voice. If there is anything to carry in, 
 * add appropriate padding, then generate curve */

static void
add_carryin(staff_p)

struct STAFF *staff_p;		/* which staff to do ties/slur carry in on */

{
	int staffno;
	int v;				/* voice number */
	int n;				/* index into notelist */
	struct GRPSYL *gs_p;		/* first chord in measure */
	struct TIECARRY *info_p;	/* info about things carried in */
	int found;			/* if matching note found in chord */
	double padding;			/* how much padding to add */


	staffno = staff_p->staffno;
	/* do each carried in item on each voice */
	for (v = 0; v < MAXVOICES; v++) {

		padding = HALFTIEPAD;

		for (info_p = Tiecarryinfolist_p [staffno] [v];
					info_p != (struct TIECARRY *) 0;
					info_p = info_p->next) {

			gs_p = staff_p->groups_p[v];

			/* add padding to allow for carried-in mark */
			gs_p->padding += padding;
			/* only add padding once per chord! */
			padding = 0.0;

			/* mark any notes that will get carried-in mark */
			for (found = NO, n = 0; n <  gs_p->nnotes; n++) {
				if (gs_p->notelist[n].letter
						== info_p->letter &&
						gs_p->notelist[n].octave
						== info_p->octave) {

					/* A carried-in tie on a tablature
					 * staff isn't printed, but the fret
					 * is put in parentheses. */
					if (is_tab_staff(staff_p->staffno) == YES
							&& info_p->curveno == -1) {
						gs_p->notelist[n].FRET_HAS_PAREN = YES;
					}
					found = YES;
					break;
				}
			}

			if (found == NO) {
				if (is_tab_staff(gs_p->staffno) == YES) {
					struct STRINGINFO *strinfo_p;

                                        strinfo_p = &(svpath(gs_p->staffno,
                                                STAFFLINES)->strinfo[ (int)
                                                info_p->STRINGNO]);
					l_warning(gs_p->inputfile, gs_p->inputlineno,
					"can't carry tie/slur/bend into ending: %s%d not in chord",
					format_string_name(strinfo_p->letter,
					strinfo_p->accidental, strinfo_p->nticks),
					info_p->fretno);
				}
				else {
					l_warning(gs_p->inputfile, gs_p->inputlineno,
					"can't carry tie/slur/bend into ending: %c%d not in chord",
					info_p->letter, info_p->octave);
				}
			}
		}
	}
}


/* free all the tie carry in info */

static void
free_carryin_info()

{
	int s;
	int v;


	for (s = 1; s <= MAXSTAFFS; s++) {
		for (v = 0; v < MAXVOICES; v++) {
			free_cinfo(Tiecarryinfolist_p [s] [v]);
			Tiecarryinfolist_p [s] [v] = (struct TIECARRY *) 0;
		}
	}

	Have_carry_ins = NO;
}


/* recursively free list of tie carry information */

static void
free_cinfo(carryinfo_p)

struct TIECARRY *carryinfo_p;

{
	if (carryinfo_p == (struct TIECARRY *) 0) {
		return;
	}

	free_cinfo(carryinfo_p->next);
	FREE(carryinfo_p);
}


/* check if a transposition occurred, and if so, see if there were any
 * ties that would cross the bar. If so, print warning and discard the tie */

static void
chk4xpose(mll_p)

struct MAINLL *mll_p;	/* containing SSV that might contain transpose */

{
	struct SSV *ssv_p;
	int s;			/* staff index */
	int intnum;		/* transposition interval */
	int inttype;		/* transposition interval type */


	if (mll_p->str != S_SSV) {
		return;
	}

	ssv_p = mll_p->u.ssv_p;
	if (ssv_p->used[TRANSPOSITION] == YES ||
					ssv_p->used[ADDTRANSPOSITION] == YES) {
		/* this SSV changes transpose value, need to check further */
		if (ssv_p->context == C_STAFF) {
			/* if staff now has a different transpose value than
			 * before, need to see if any notes tied over the
			 * previous bar */
			s = ssv_p->staffno;
			totaltrans(YES, s, &inttype, & intnum);
			if (ssv_p->inttype != inttype
					|| ssv_p->intnum != intnum) {
				chkxpstaff(mll_p, s);
			}
		}
		else {
			/* must be score wide change. This is a little
			 * trickier. Go through each staff. If its transpose
			 * value is not set in staff context and it's
			 * different than the new transpose value, then
			 * we need to check for ties */
			for (s = 1; s <= Score.staffs; s++) {
				totaltrans(YES, 0, &inttype, & intnum);
				if (staff_field_used(TRANSPOSITION, s) == NO &&
					staff_field_used(ADDTRANSPOSITION, s) == NO &&
					(ssv_p->inttype != inttype
					|| ssv_p->intnum != intnum)) {
				    chkxpstaff(mll_p, s);
				}
			}
		}
	}
}


/* check a specific staff for possible ties across transposition */

static void
chkxpstaff(mll_p, s)

struct MAINLL *mll_p;	/* look backward in main list from here */
int s;			/* which staff */

{
	int v;


	/* back up to find appropriate staff */
	for (mll_p = mll_p->prev; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->prev) {
		if (mll_p->str == S_STAFF) {
			if (mll_p->u.staff_p->staffno == s) {
				/* found the correct staff. check each voice */
				for (v = 0; v < MAXVOICES; v++) {
					chkxpgrp(mll_p->u.staff_p->groups_p[v],
						mll_p->inputfile,
						mll_p->inputlineno);
				}
				return;
			}
			else if (mll_p->u.staff_p->staffno < s) {
				/* user must have increased the number of
				 * staffs as well, so the staff in question
				 * didn't exist in previous measure */
				return;
			}
		}
	}
}


/* find the last group in a list of grpsyls. If it has any ties on it,
 * print warning message for trying to tie across a transposition, and discard
 * the tie(s). */

static void
chkxpgrp(gs_p, inputfile, lineno)

struct GRPSYL *gs_p;	/* check this grpsyl list */
char *inputfile;	/* for error message */
int lineno;

{
	register int n;		/* index through notelist */


	/* find last group in list */
	for (   ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {

		if (gs_p->next == (struct GRPSYL *) 0) {
			/* this is the last group in the measure. See if
			 * it has any ties on it */
			for (n = 0; n < gs_p->nnotes; n++) {
				if (gs_p->notelist[n].tie == YES) {
					/* Aha! User tried to do a tie over
					 * a transpose */
					l_warning(inputfile, lineno,
					 	"can't tie into transposition change (use slur)");
					/* cancel any and all ties on this grp,
					 * and return, so we don't print more
					 * than one error per group */
					for (n = 0; n < gs_p->nnotes; n++) {
						gs_p->notelist[n].tie = NO;
					}
					gs_p->tie = NO;
					return;
				}
			}
		}
	}
}


/* On tablature staffs, if notes in two consecutive groups are tied together,
 * normally the frets are not printed for the second group, so we
 * set the inhibitprint flag on the note. Placement may later unset the
 * flag if the group ends up after a score feed.  However if there is any
 * reason why inhibiting printing on a given tied-to note isn't a good idea,
 * we won't set the inhibitprint flag. So this function checks all tied-to
 * notes, and if none of the reasons for not setting inhibitprint apply,
 * then the inhibitprint flag is set.
 */

static void
set_inhibitprint_if_appropriate(gs_p, mll_p)

struct GRPSYL *gs_p;	/* Since inhibitprint is only set on tied-to notes,
			 * this function will actually deal with the
			 * group after this one. */
struct MAINLL *mll_p;	/* We need this for passing to nextgrpsyl() */

{
	struct GRPSYL *nextgs_p;	/* the group after gs_p */
#ifdef MAYBE
	struct GRPSYL *following_p;	/* the group after nextgs_p */
#endif
	int n;				/* index through notelist */
	struct NOTE *tied_to_note_p;	/* if a note has a tie, this points
					 * to the matching note in nextgs_p */


	if ((nextgs_p = nextgrpsyl(gs_p, &mll_p)) == (struct GRPSYL *) 0) {
		/* no next group, so nothing to set */
		return;
	}

	for (n = 0; n < gs_p->nnotes; n++) {
		/* We will only set inhibitprint on notes that are tied to. */
		if (gs_p->notelist[n].tie == NO && gs_p->tie == NO) {
			continue;
		}

		tied_to_note_p = find_matching_note(nextgs_p, 0,
				gs_p->notelist[n].letter,
				gs_p->notelist[n].FRETNO,
				gs_p->notelist[n].octave, 0);

		if (tied_to_note_p == 0) {
			continue;
		}

		/* If the tied-to note has any slides to/from nowhere, or slurs
		 * to the next group, it won't get inhibitprint set */
		if (tied_to_note_p->nslurto != 0) {
			continue;
		}
		/* Similarly, if it has a bend, we don't inhibit printing */
		if (HASBEND(*tied_to_note_p) == YES) {
			continue;
		}

#ifdef MAYBE
		/* If following group has a non-prebend bend on this string,
		 * then we should not set inhibitprint. */
		if ((following_p = nextgrpsyl(nextgs_p, &mll_p)) != 0) {
			int nn;		/* index through notes of following */
			for (nn = 0; nn < following_p->nnotes; nn++) {
				if (following_p->notelist[nn].STRINGNO !=
						tied_to_note_p->STRINGNO) {
					/* This is a note on a different
					 * string, so not relevant here */
					continue;
				}
				if (HASBEND(following_p->notelist[nn]) == YES &&
						following_p->notelist[nn].FRETNO
						== NOFRET) {
					break;
				}
			}
			if (nn < following_p->nnotes) {
				/* must have found a non-prebend bend */
				continue;
			}
		}

#endif
		/* Is tied to and none of the special cases apply,
		 * so inhibit printing. */
		tied_to_note_p->inhibitprint = YES;
	}
}
