/*
 Copyright (c) 1995-2023  by Arkkra Enterprises.
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
 * Name:	plutils.c
 *
 * Description:	This file contains utility functions belonging to the placement
 *		phase.  Some of them are also used by other phases.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

static int phrase_tieslur_note P((struct GRPSYL *gs_p, int nidx, int side,
		int interfere));
static int has_group_here(struct GRPSYL *gs_p, RATIONAL starttime,
		RATIONAL duration);
static int tied_to_nidx P((struct GRPSYL *gs_p, struct GRPSYL *ngs_p,
		int nidx));
static int slurred_to_nidx P((struct GRPSYL *gs_p, struct GRPSYL *ngs_p,
		int nidx, int sidx));
static RATIONAL lefttime P((double count, struct GRPSYL *firstgs_p,
		int timeden));
static RATIONAL righttime P((double count, struct GRPSYL *firstgs_p,
		int timeden));
static int hasspace_common P((struct GRPSYL *gs_p, int chk_col, int chkpvno,
		RATIONAL vtime, RATIONAL vtime2));
static int staff_wants_subbar(struct SUBBAR_APPEARANCE *sb_app_p, int s);

/*
 * Name:        nextnongrace()
 *
 * Abstract:    Return next nongrace group in a GRPSYL list.
 *
 * Returns:     pointer to GRPSYL of next nongrace group, 0 if none
 *
 * Description: This function loops down the GRPSYL linked list from the given
 *		starting point.  It returns the next nongrace GRPSYL, or 0
 *		if none.
 */

struct GRPSYL *
nextnongrace(gs_p)

struct GRPSYL *gs_p;	/* current group */

{
	gs_p = gs_p->next;
	while (gs_p != 0 && gs_p->grpvalue == GV_ZERO)
		gs_p = gs_p->next;
	return (gs_p);
}

/*
 * Name:        prevnongrace()
 *
 * Abstract:    Return previous nongrace group in a GRPSYL list.
 *
 * Returns:     pointer to GRPSYL of previous nongrace group, 0 if none
 *
 * Description: This function loop up the GRPSYL linked list from the given
 *		starting point.  It returns the previous nongrace GRPSYL, or 0
 *		if none.
 */

struct GRPSYL *
prevnongrace(gs_p)

struct GRPSYL *gs_p;	/* current group */

{
	gs_p = gs_p->prev;
	while (gs_p != 0 && gs_p->grpvalue == GV_ZERO)
		gs_p = gs_p->prev;
	return (gs_p);
}

/*
 * Name:        nextglobnongrace()
 *
 * Abstract:    Return next nongrace group in this voice.
 *
 * Returns:     pointer to GRPSYL of next nongrace group, 0 if none
 *
 * Description: This function, given a nongrace and the MLL structure it hangs
 *		off of, returns the next nongrace in this voice, even if it's in
 *		the next measure.  If it is in the next measure, *mll_p_p gets
 *		updated.  But if that next measure is a second or later ending,
 *		it's not considered to be a "next" measure, so return 0.
 */

struct GRPSYL *
nextglobnongrace(gs_p, mll_p_p)

struct GRPSYL *gs_p;		/* current group */
struct MAINLL **mll_p_p;	/* MLL structure it is hanging off of */

{
	do {
		gs_p = nextgrpsyl(gs_p, mll_p_p);
	} while (gs_p != 0 && gs_p->grpvalue == GV_ZERO);
	return (gs_p);
}

/*
 * Name:        prevglobnongrace()
 *
 * Abstract:    Return previous nongrace group in this voice.
 *
 * Returns:     pointer to GRPSYL of previous nongrace group, 0 if none
 *
 * Description:	This function, given a nongrace and the MLL structure it hangs
 *		off of, returns the prev nongrace in this voice, even if it's
 *		in an earlier measure.  If we are at the start of an ending,
 *		it skips over any previous ending and goes to the measure
 *		preceding the first ending.  If the resulting nongrace is in a
 *		previous measure, *mll_p_p gets updated.
 */

struct GRPSYL *
prevglobnongrace(gs_p, mll_p_p)

struct GRPSYL *gs_p;		/* current group */
struct MAINLL **mll_p_p;	/* MLL structure it is hanging off of */

{
	do {
		gs_p = prevgrpsyl(gs_p, mll_p_p);
	} while (gs_p != 0 && gs_p->grpvalue == GV_ZERO);
	return (gs_p);
}

/*
 * Name:        nextnongracenonspace()
 *
 * Abstract:    Return next nongrace nonspace group in a GRPSYL list.
 *
 * Returns:     pointer to GRPSYL of next nongrace group, 0 if none
 *
 * Description: This function loops down the GRPSYL linked list from the given
 *		starting point.  It returns the next nongrace nonspace GRPSYL,
 *		or 0 if none.
 */

struct GRPSYL *
nextnongracenonspace(gs_p)

struct GRPSYL *gs_p;	/* current group */

{
	gs_p = gs_p->next;
	while (gs_p != 0 && (gs_p->grpvalue == GV_ZERO ||
			     gs_p->grpcont == GC_SPACE))
		gs_p = gs_p->next;
	return (gs_p);
}

/*
 * Name:        drmo()
 *
 * Abstract:    Detect rightmost one.
 *
 * Returns:     void
 *
 * Description: This function returns the bit position of the rightmost bit
 *		that is a 1 in the given number, the low order bit being
 *		bit 0.  The given number must not be 0.
 */

int
drmo(num)

register int num;

{
	register int n;

	for (n = 0; n < 8 * sizeof(int); n++) {
		if ( (num & (1 << n)) != 0 )
			return (n);
	}
	pfatal("0 was passed to drmo");
	return (0);	/* dead code, but keeps lint happy */
}

/*
 * Name:        tieslurpad()
 *
 * Abstract:    How much tie/slur padding is needed after this group?
 *
 * Returns:     Padding in inches.
 *
 * Description: This function returns the amount of padding needed after a
 *		group due to ties or slurs, if the given group is tied to the
 *		next group, or any note in it is tied or slurred to a note
 *		in the following group.  Otherwise it returns zero.
 *		NOTE:  This function ignores staffscale.
 */

double
tieslurpad(mll_p, staff_p, gs_p)

struct MAINLL *mll_p;		/* the MLL item that is our staff */
struct STAFF *staff_p;		/* the staff the group is connected to */
struct GRPSYL *gs_p;		/* the group after which padding may occur */

{
	struct NOTE *note_p;	/* point at a note structure */
	struct GRPSYL *gtemp_p;	/* temp GRPSYL pointer */
	struct GRPSYL *this_p;	/* first GRPSYL in this voice */
	struct GRPSYL *that_p;	/* first GRPSYL in other voice */
	struct GRPSYL *ngs_p;	/* pointer to the tied/slurred-to group */
	RATIONAL starttime;	/* time into measure where *gs_p starts */
	float pad;		/* how much padding is needed */
	int interfere;		/* does other voice have notes/rests here? */
	int stepdiff;		/* vertical reach of a curve */
	int n;			/* index into notes in group */
	int s;			/* index into notes slurred to */
	int other_vno;		/* tied_to_voice or slurred_to_voice */


	/* padding doesn't matter for MIDI, and in one scenario causes pfatal */
	if (Doing_MIDI == YES)
		return (0);

	/* syllables can't have ties or slurs */
	if (gs_p->grpsyl != GS_GROUP)
		return (0);

	/* rests and spaces can't have ties or slurs */
	if (gs_p->grpcont != GC_NOTES)
		return (0);

	/* ties and slurs are never drawn on tab staffs */
	if (svpath(staff_p->staffno, CLEF)->clef == TABCLEF)
		return (0);

	/* if last group in measure, don't need any more space */
	if (gs_p->next == 0)
		return (0);

	/*
	 * Find the first group in this measure, and total time preceding the
	 * group we were given.  We need this to figure out which voice we are
	 * in, and, if there is another voice, whether it has only spaces
	 * during the time of our group, which affects how the curves should
	 * look.
	 */
	starttime = Zero;
	for (gtemp_p = gs_p->prev, this_p = gs_p; gtemp_p != 0;
			this_p = gtemp_p, gtemp_p = gtemp_p->prev)
		starttime = radd(starttime, gtemp_p->fulltime);

	/* point at other voice, or null pointer if none */
	if (staff_p->groups_p[0] == this_p)
		that_p = staff_p->groups_p[1];	/* might be 0 */
	else if (staff_p->groups_p[1] == this_p)
		that_p = staff_p->groups_p[0];	/* might be 0 */
	else
		that_p = 0;	/* we are voice 3, ignore other voices */

	/*
	 * If the other voice (if it exists) has a group that starts within the
	 * time interval of our group, that should provide enough room such
	 * that we don't need any padding; assuming that group is not a
	 * compressible space.
	 */
	if (has_group_here(that_p, starttime, gs_p->fulltime) == YES) {
		return (0);
	}

	if (that_p == 0 || hasspace(that_p, starttime,
				radd(starttime, gs_p->fulltime)))
		interfere = NO;
	else
		interfere = YES;

	pad = 0;		/* start with no padding */

	/*
	 * Loop through every note in this group.  If it's tied, check each
	 * note to see if either it or the note it's tied to is ineligible for
	 * phrase-like curves.  If so, there will be a horizontally aligned
	 * curve, and we need to pad.  The note must be the same in both
	 * groups, so there's no need to consider vertical distances at this
	 * point.  Then loop through the 0 or more slurs from this note to
	 * note(s) in the next group.  For each one, find the vertical distance
	 * between the two notes.  The padding it needs is based on this and on
	 * whether phrase-like curves can be drawn.  Keep track of the maximum
	 * padding needed by any pair of notes.
	 * We also need to pad if the first stem is on the right and the second
	 * stem is on the left, because that leaves no room for the curve.
	 */
	for (n = 0; n < gs_p->nnotes; n++) {
		note_p = &gs_p->notelist[n];
		if (note_p->tie == YES) {
			/* find group we are tied to */
			other_vno = gs_p->notelist[n].tied_to_voice;
			ngs_p = find_to_group(mll_p, gs_p, other_vno, "tie");
			if (ngs_p == 0) {
				pfatal("tieslurpad: find_to_group could not find the tied-to group");
			}

			if ((HAS_STEM_ON_RIGHT(gs_p) && HAS_STEM_ON_LEFT(ngs_p))
			|| phrase_tieslur_note(gs_p, n, STARTITEM, interfere)
			== NO || phrase_tieslur_note(ngs_p, tied_to_nidx(
			gs_p, ngs_p, n), ENDITEM, interfere) == NO) {
				pad = MAX(pad, TIESLURPAD);
			}
		}
		for (s = 0; s < note_p->nslurto; s++) {
			/*
			 * If it's a slur to/from nowhere, don't deal with it
			 * here.  It is considered along with the width of the
			 * individual note.
			 */
			if (IS_NOWHERE(note_p->slurtolist[s].octave))
				continue;	/* from nowhere */

			stepdiff = abs(
					( note_p->octave * 7 +
					Letshift[ note_p->letter - 'a' ] ) -
					( note_p->slurtolist[s].octave * 7 +
					Letshift[ note_p->slurtolist[s].letter
						- 'a' ] )
					);
			/* find group we are slurred to */
			other_vno = gs_p->notelist[n].slurtolist[s].
					slurred_to_voice;
			ngs_p = find_to_group(mll_p, gs_p, other_vno, "slur");
			if (ngs_p == 0) {
				pfatal("tieslurpad: find_to_group could not find the slurred-to group");
			}

			if ((HAS_STEM_ON_RIGHT(gs_p) && HAS_STEM_ON_LEFT(ngs_p))
			|| phrase_tieslur_note(gs_p, n, STARTITEM, interfere)
			== NO || phrase_tieslur_note(ngs_p,
			slurred_to_nidx(gs_p, ngs_p, n, s), ENDITEM, interfere)
			== NO) {
				pad = MAX(pad, stepdiff <= 3 ? TIESLURPAD :
				TIESLURPAD + (stepdiff - 3) * STEPSIZE / 2);
			} else {
				pad = MAX(pad, stepdiff <= 3 ? 0 :
				(stepdiff - 3) * STEPSIZE / 2);
			}
		}
	}

	return (pad);		/* max padding needed by any pair of notes */
}

/*
 * Name:        phrase_tieslur_note()
 *
 * Abstract:    Is the given note the end note and eligible for "new" tie/slur?
 *
 * Returns:     YES or NO
 *
 * Description: This function determines whether a tie or slur to/from the
 *		given note is to be drawn like a phrase mark (as opposed to
 *		drawing it vertically aligned with the note).
 */

static int
phrase_tieslur_note(gs_p, nidx, side, interfere)

struct GRPSYL *gs_p;		/* point at note's group */
int nidx;			/* index to this note in notelist */
int side;			/* STARTITEM (curve here to right) or ENDITEM */
int interfere;			/* does the other voice have notes/rests here?*/

{
	/* check for each bad condition, returning NO if it exists */

	/* inner note of a group */
	if (nidx != 0 && nidx != gs_p->nnotes - 1)
		return (NO);

	/* bottom note of voice 1 and other voice interferes */
	if (gs_p->vno == 1 && nidx == gs_p->nnotes - 1 && interfere)
		return (NO);

	/* top note of voice 2 and other voice interferes */
	if (gs_p->vno == 2 && nidx == 0 && interfere)
		return (NO);

	/* "with" items on the antistem note when stem up */
	if (has_with(gs_p, PL_BELOW) && gs_p->stemdir == UP &&
			nidx == gs_p->nnotes - 1)
		return (NO);

	/* "with" items on the antistem note when stem down */
	if (has_with(gs_p, PL_ABOVE) && gs_p->stemdir == DOWN && nidx == 0)
		return (NO);

	/* stem in the way of left end of curve */
	if (side == STARTITEM && HAS_STEM_ON_RIGHT(gs_p) &&
			nidx == 0 && gs_p->nnotes > 1)
		return (NO);

	/* stem in the way of right end of curve */
	if (side == ENDITEM && HAS_STEM_ON_LEFT(gs_p) &&
			nidx == gs_p->nnotes - 1 && gs_p->nnotes > 1)
		return (NO);

	return (YES);
}

/*
 * Name:        has_group_here()
 *
 * Abstract:    Does this voice have a non-0 len grp starting in this interval?
 *
 * Returns:     YES or NO
 *
 * Description: This function determines whether the given voice has a group
 *		starting inside of the given time interval that has a nonzero
 *		width.  The endpoints of the interval are not included.
 *		Nonzero width is determined by being nonspace, or uncollapsable
 *		space.
 */

static int
has_group_here(gs_p, starttime, duration)

struct GRPSYL *gs_p;	/* first GRPSYL in the voice */
RATIONAL starttime;	/* start time of the interval */
RATIONAL duration;	/* duration of the interval */

{
	RATIONAL time;		/* keep track of time */
	RATIONAL endtime;	/* end point of the interval */


	/* if no groups, return the answer immediately */
	if (gs_p == 0) {
		return (NO);
	}

	time = Zero;
	endtime = radd(starttime, duration);

	/* loop through each group in the voice until we know the answer */
	for ( ; gs_p != 0; gs_p = gs_p->next) {
		if (LE(time, starttime)) {
			time = radd(time, gs_p->fulltime);
			continue;
		}
		if (GE(time, endtime)) {
			return (NO);
		}
		if (gs_p->grpcont != GC_SPACE || gs_p->uncompressible == YES) {
			return (YES);
		}
		time = radd(time, gs_p->fulltime);
	}

	return (NO);
}

/*
 * Name:        has_with()
 *
 * Abstract:    Return whether the group has "with" items at the given place.
 *
 * Returns:     YES or NO
 *
 * Description: This function is given a group and a place (PL_ABOVE or
 *		PL_BELOW).  It checks whether the group has "with" items at
 *		that place.
 */

int
has_with(gs_p, place)

struct GRPSYL *gs_p;
int place;

{
	int idx;


	/* if no with list, no items on either side */
	if (gs_p->withlist == 0) {
		return (NO);
	}

	for (idx = 0; idx < gs_p->nwith; idx++) {
		if (gs_p->withlist[idx].place == place) {
			/* found an item on the requested side */
			return (YES);
		}
	}

	return NO;
}

/*
 * Name:        tied_to_nidx()
 *
 * Abstract:    Return the note index of the note the given note is tied to.
 *
 * Returns:     index into ngs_p->notelist
 *
 * Description: This function is given a valid group (not the last one in the
 *		measure) and an index into its notelist to a note that is tied
 *		to the next group.  It returns the index into the next group's
 *		notelist to the note that the first group's note is tied to.
 *		The next group could be in a different voice.
 */

static int
tied_to_nidx(gs_p, ngs_p, nidx)

struct GRPSYL *gs_p;		/* point at note's group */
struct GRPSYL *ngs_p;		/* point at next group */
int nidx;			/* index to this note in notelist */

{
	struct NOTE *nl_ptr;	/* ptr to next group's notelist */
	int n;


	nl_ptr = ngs_p->notelist;

	for (n = 0; n < ngs_p->nnotes; n++) {
		if (gs_p->notelist[nidx].letter == nl_ptr[n].letter &&
		    gs_p->notelist[nidx].octave == nl_ptr[n].octave)
			return (n);
	}

	pfatal("tied_to_nidx: can't find note tied to");
	return (0);		/* to keep lint happy */
}

/*
 * Name:        slurred_to_nidx()
 *
 * Abstract:    Return the note index of the note the given note is slurred to.
 *
 * Returns:     index into ngs_p->notelist
 *
 * Description: This function is given a valid group (not the last one in the
 *		measure) and an index into its notelist to a note slurred
 *		to the next group.  It returns the index into the next group's
 *		notelist to the note that the first group's note is tied to.
 *		The next group could be in a different voice.
 */

static int
slurred_to_nidx(gs_p, ngs_p, nidx, sidx)

struct GRPSYL *gs_p;		/* point at note's group */
struct GRPSYL *ngs_p;		/* point at next group */
int nidx;			/* index to this note in notelist */
int sidx;			/* index to slurred to note in slurto list */

{
	struct NOTE *nl_ptr;	/* ptr to next group's notelist */
	int n;


	nl_ptr = ngs_p->notelist;

	for (n = 0; n < ngs_p->nnotes; n++) {
		if (gs_p->notelist[nidx].slurtolist[sidx].letter ==
					nl_ptr[n].letter &&
		    gs_p->notelist[nidx].slurtolist[sidx].octave ==
					nl_ptr[n].octave)
			return (n);
	}

	pfatal("slurred_to_nidx: can't find note slurred to");
	return (0);		/* to keep lint happy */
}

/*
 * Name:        hasspace()
 *
 * Abstract:    Finds out if the given voice has space during the given time.
 *
 * Returns:     YES or NO
 *
 * Description: This function is given a linked list of groups to check
 *		during a given time interval.  If the list consists entirely
 *		of space(s) during the time interval, the function returns
 *		YES.  Otherwise it returns NO.  If vtime2 is greater than the
 *		length of a measure, the extra, nonexistent time is regarded
 *		as all spaces.  If the linked list of groups doesn't exist
 *		(gs_p is a null pointer), the function returns YES, since
 *		there is nothing there but "space".
 */

int
hasspace(gs_p, vtime, vtime2)

struct GRPSYL *gs_p;	/* starts pointing at the first GRPSYL list */
RATIONAL vtime, vtime2;	/* time when to start and stop checking for space */

{
	return (hasspace_common(gs_p, NO, 0, vtime, vtime2));
}

/*
 * Name:        has_collapsible_space()
 *
 * Abstract:    Check if given voice has collapsible space during given time.
 *
 * Returns:     YES or NO
 *
 * Description: Same as hasspace, except that instead of checking for any
 *		spaces, it checks only for spaces that are collapsible.
 *		Similar to hasspace, time beyond the end of a measure is
 *		regarded as collapsible space, and so is gs_p being null.
 */

int
has_collapsible_space(gs_p, vtime, vtime2)

struct GRPSYL *gs_p;	/* starts pointing at the first GRPSYL list */
RATIONAL vtime, vtime2;	/* time when to start and stop checking for space */

{
	return (hasspace_common(gs_p, YES, 0, vtime, vtime2));
}

/*
 * Name:        has_space_pvno()
 *
 * Abstract:    Check if the first given voice and voice 3 acting as that voice
 *		are only spaces during the given time.
 *
 * Returns:     YES or NO
 *
 * Description: This is similar to hasspace, but it ensures not only that the
 *		first given voice has no notes or rests during the time
 *		interval, but also that voice 3 has no notes or rests that are
 *		acting like that voice (pseudo voice number set to the first
 *		voice's).  The first given voice must be v1 or v2, and must not
 *		be a null pointer.  Similar to hasspace, time beyond the end
 *		of a measure is regarded as space, and so is the voice 3
 *		pointer being null; but the other voice must not be null.
 */

int
has_space_pvno(gs_p, v3gs_p, vtime, vtime2)

struct GRPSYL *gs_p;	/* starts pointing at v1 or v2 GRPSYL list, not null */
struct GRPSYL *v3gs_p;	/* starts pointing at v3 GRPSYL list, or null */
RATIONAL vtime, vtime2;	/* time when to start and stop checking for space */

{
	if (hasspace_common(gs_p, NO, 0, vtime, vtime2) &&
	    hasspace_common(v3gs_p, NO, gs_p->pvno, vtime, vtime2)) {
		return (YES);
	}
	return (NO);
}

/*
 * Name:        hasspace_common()
 *
 * Abstract:    Common code used by has*space* functions.
 *
 * Returns:     YES or NO
 *
 * Description: Most of the logic is the same so do it in this subroutine.
 */

/*
 * For purposes of this function, a group is to be treated as effectively being
 * a space if either of these conditions holds:
 * 1.  It is a space, and either a) we are not checking exclusively for
 *     collapsible spaces, or b) it is collapsible.
 * 2.  It is not a space, but to recognize a group as effectively non-space we
 *     are requiring it to have a particular pseudo voice number, and it does
 *     not have that number.
 */
#define EFF_SPACE(gs_p, chkpvno) (					     \
	(gs_p->grpcont == GC_SPACE &&					     \
			(chk_col == NO || gs_p->uncompressible == NO)) ||    \
	(gs_p->grpcont != GC_SPACE && chkpvno != 0 && gs_p->pvno != chkpvno) \
)

static int
hasspace_common(gs_p, chk_col, chkpvno, vtime, vtime2)

struct GRPSYL *gs_p;	/* starts pointing at the first GRPSYL list */
int chk_col;		/* YES or NO: check only for collapsible spaces? */
int chkpvno;		/* if nonzero, treat groups that don't have this pvno
			 * as equivalent to spaces */
RATIONAL vtime, vtime2;	/* time when to start and stop checking for space */

{
	RATIONAL t;			/* accumulate time */
	struct GRPSYL *prevgs_p;	/* previous GRPSYL */


	/* "no linked list exists" counts as all spaces */
	if (gs_p == 0) {
		return (YES);
	}

	prevgs_p = 0;

	/* accumulate time until reaching or crossing vtime boundary */
	for (t = Zero; LT(t, vtime); gs_p = gs_p->next) {
		/* if we run out of GRPSYLs, the desired time window is space */
		if (gs_p == 0) {
			return (YES);
		}
		if (gs_p->grpvalue == GV_ZERO) {
			continue;
		}
		t = radd(t, gs_p->fulltime);
		prevgs_p = gs_p;
	}

	/*
	 * If we crossed over beyond the vtime boundary, check what kind of
	 * group it was.  (Note that prevgs_p can't be null, since we must
	 * have processed at least one nongrace to have gotten beyond time
	 * zero.)  Any effective nonspace means we should return NO.
	 */
	if (GT(t, vtime) && ! EFF_SPACE(prevgs_p, chkpvno)) {
		return (NO);
	}

	/* check every group that lies at least partially within the window */
	for ( ; gs_p != 0 && LT(t, vtime2); gs_p = gs_p->next) {
		if (gs_p->grpvalue == GV_ZERO) {
			continue;
		}
		if ( ! EFF_SPACE(gs_p, chkpvno)) {
			return (NO);
		}
		t = radd(t, gs_p->fulltime);
	}

	return (YES);
}

/*
 * Name:        closestgroup()
 *
 * Abstract:    Find closest nongrace group in this voice to given time value.
 *
 * Returns:     pointer to the closest nongrace GRPSYL
 *
 * Description: This function finds the GRPSYL in the given linked list that is
 *		closest, timewise, to the given count number, ignoring grace
 *		groups.
 */

struct GRPSYL *
closestgroup(count, firstgs_p, timeden)

double count;			/* which count of the measure */
struct GRPSYL *firstgs_p;	/* first GRPSYL of relevant voice in measure */
int timeden;			/* denominator of current time signature */

{
	RATIONAL reqtime;	/* time requested */
	RATIONAL tottime;	/* total time in measure so far */
	RATIONAL otottime;	/* old total time in measure so far */
	struct GRPSYL *gs_p;	/* point along group list */
	struct GRPSYL *ogs_p;	/* (old) point along group list */


	/* skip over any initial grace groups */
	if (firstgs_p->grpvalue == GV_ZERO)
		firstgs_p = nextnongrace(firstgs_p);

	/* if at or before the first count, it's closest to first group */
	if (count <= 1)
		return (firstgs_p);

	/* get requested time to nearest tiny part of a count, in lowest terms*/
	reqtime.n = 4 * MAXBASICTIME * (count - 1) + 0.5;
	reqtime.d = 4 * MAXBASICTIME * timeden;
	rred(&reqtime);

	/*
	 * Loop through this voice accumulating time values.  As soon as we
	 * equal or exceed the requested time value, check whether the
	 * requested time is closer to the new accumulated time, or that before
	 * this last group.  Return the closest one.
	 */
	tottime = Zero;
	for (ogs_p = firstgs_p, gs_p = nextnongrace(ogs_p); gs_p != 0;
				ogs_p = gs_p, gs_p = nextnongrace(gs_p)) {
		otottime = tottime;
		tottime = radd(tottime, ogs_p->fulltime);
		if (GE(tottime, reqtime)) {
			if (GT( rsub(reqtime,otottime), rsub(tottime,reqtime) ))
				return (gs_p);
			else
				return (ogs_p);
		}
	}

	/* requested time is after last group; return last group */
	return (ogs_p);
}

/*
 * Name:        chkallspace()
 *
 * Abstract:    Check if voice is all spaces for the voice this stuff is on.
 *
 * Returns:     YES or NO
 *
 * Description: This function checks where one voice seems to be all spaces
 *		during the duration of a phrase mark, or other stuff which must
 *		be associated with a definite group.  The tricky thing is that
 *		until we've decided which voice the stuff is intended to
 * 		apply to, we don't exactly know what the endpoints of the
 *		stuff are going to be.  All we know is the "count" values the
 *		user asked for, which may or may not equal the positions of
 *		GRPSYLs in the voices.  So we look at voices 1 and 2 and take
 *		the worst (widest) case as the endpoints.  (This is called only
 *		when both of these voices exist.  We ignore any voice 3.)
 */

int
chkallspace(msbeg_p, stuff_p, vno)

struct MAINLL *msbeg_p;	/* staff at beginning of the stuff */
struct STUFF *stuff_p;	/* the STUFF */
int vno;		/* voice being tested for being all spaces */

{
	static RATIONAL tiny = {1, 4 * MAXBASICTIME};
	struct MAINLL *msend_p;		/* staff at end of the phrase */
	int timeden;			/* denom of time sig at end of stuff */
	RATIONAL begtime, endtime;	/* time into measures of begin & end */
	RATIONAL temptime;		/* temp var for storing time */


	/*
	 * Find what measure this stuff ends in.  Along the way, keep
	 * track of the time signature denominator, in case it changes.
	 */
	msend_p = getendstuff(msbeg_p, stuff_p, &timeden);

	/*
	 * If we hit a multirest, bail out, returning YES.  A multirest is a
	 * rest.  Yes, we are neglecting the slight chance that the STUFF will
	 * extend beyond the end of the multirest, and that that would matter.
	 */
	if (msend_p == 0) {
		return (YES);
	}

	/*
	 * Find time values that are sure to contain the stuff.  Take the
	 * outermost values of the two voices.
	 */
	begtime = lefttime(stuff_p->start.count,
				msbeg_p->u.staff_p->groups_p[0], Score.timeden);
	temptime = lefttime(stuff_p->start.count,
				msbeg_p->u.staff_p->groups_p[1], Score.timeden);
	if (LT(temptime, begtime))
		begtime = temptime;
	endtime = righttime(stuff_p->end.count,
				msend_p->u.staff_p->groups_p[0], timeden);
	temptime = righttime(stuff_p->end.count,
				msend_p->u.staff_p->groups_p[1], timeden);
	if (GT(temptime, endtime))
		endtime = temptime;

	/*
	 * If the beginning and end are in the same measure and at the same
	 * time, this phrase would normally be thrown away later, but we need
	 * to deal with it because the case of a phrase from a grace to its
	 * main note.  It doesn't make sense to ask what a zero time contains,
	 * so to handle this, add a tiny time value to the end time.
	 */
	if (msbeg_p == msend_p && EQ(begtime, endtime))
		endtime = radd(endtime, tiny);

	return (allspace(vno, msbeg_p, begtime, msend_p, endtime));
}

/*
 * Name:        allspace()
 *
 * Abstract:    Finds out if the given voice has space for the given time.
 *
 * Returns:     YES or NO
 *
 * Description: This function is a multi-measure version of hasspace(), and in
 *		fact works by calling hasspace() repeatedly.  It is given the
 *		linked list of groups for the voice in the first measure in
 *		question.  It checks whether the voice consists entirely of
 *		spaces from the duration point given for this first measure,
 *		until the endpoint, which may or may not be in the same measure.
 */

int
allspace(vno, msbeg_p, begtime, msend_p, endtime)

int vno;		/* voice number, numbering from voice 1 == 0 */
struct MAINLL *msbeg_p;	/* point at MLL (staff) where duration begins */
RATIONAL begtime;	/* time where duration begins */
struct MAINLL *msend_p;	/* point at MLL (staff) where duration ends */
RATIONAL endtime;	/* time where duration ends */

{
	struct MAINLL *mainll_p;		/* point along MLL */
	int staffno;


	/* if the time starts and ends in the same measure, let hasspace do it*/
	if (msbeg_p == msend_p) {
		return (hasspace(msbeg_p->u.staff_p->groups_p[vno],
				begtime, endtime));
	}

	/*
	 * If the first measure contains non-spaces, return NO.  Rather than
	 * keeping track of time signatures, we're going to pretend that we
	 * are in the longest possible time.  This relies on the fact that
	 * hasspace() in effect assumes that any phony time past the end of
	 * the actual measure is spaces.
	 */
	if (hasspace(msbeg_p->u.staff_p->groups_p[vno], begtime, Maxtime) == NO)
		return (NO);

	staffno = msbeg_p->u.staff_p->staffno;

	/* if any intermediate measures contain non-spaces, return NO */
	for (mainll_p = msbeg_p->next; mainll_p != 0 && mainll_p != msend_p;
			mainll_p = mainll_p->next) {

		/* skip everything but STAFFs for our staff number */
		if (mainll_p->str != S_STAFF ||
				mainll_p->u.staff_p->staffno != staffno)
			continue;

		if (hasspace(mainll_p->u.staff_p->groups_p[vno], Zero, Maxtime)
				== NO)
			return (NO);
	}

	if (mainll_p == 0)
		pfatal("bug found in allspace");

	/* the result is now determined by the last measure */
	return (hasspace(msend_p->u.staff_p->groups_p[vno], Zero, endtime));
}

/*
 * Name:        getendstuff()
 *
 * Abstract:    Find staff and time signature denominator for end of a stuff.
 *
 * Returns:     pointer to MLL structure for staff containing end of stuff, or 0
 *
 * Description: This function finds the staff for the end of the given stuff.
 *		As a byproduct, it also finds the denominator of the time
 *		signature at that place.  If a multirest is encountered, a null
 *		pointer is returned, and timeden is not guaranteed.
 *		If the end of the piece is encountered, it returns the last
 *		staff.
 */

struct MAINLL *
getendstuff(mainll_p, stuff_p, timeden_p)

struct MAINLL *mainll_p;/* staff at beginning of stuff, gets changed to end */
struct STUFF *stuff_p;	/* the STUFF */
int *timeden_p;		/* gets set to denom of time sig at end of stuff */

{
	int staffno;		/* staff number where stuff is */
	struct MAINLL *mst_p;	/* point at the last staffno staff seen */
	int timenum;		/* remember the last time sig numerator */
	int b;			/* count bar lines */


	/* bail out if multirest */
	if (mainll_p->u.staff_p->groups_p[0]->is_multirest)
		return (0);

	timenum = Score.timenum;	/* init to current time sig numerator*/
	*timeden_p = Score.timeden;	/* init to current time sig denom */

	/* if stuff doesn't cross any bar lines, we can return right away */
	if (stuff_p->end.bars == 0)
		return (mainll_p);

	mst_p = mainll_p;	/* remember last staff of this number */

	staffno = mainll_p->u.staff_p->staffno;

	/*
	 * Count past the right number of  bar lines, keeping the time sig
	 * denominator up to date.
	 */
	for (b = 0; b < stuff_p->end.bars; b++) {
		for (mainll_p = mainll_p->next;
				mainll_p != 0 && mainll_p->str != S_BAR;
				mainll_p = mainll_p->next) {

			if (mainll_p->str == S_SSV &&
				    mainll_p->u.ssv_p->used[TIME] == YES) {
				timenum = mainll_p->u.ssv_p->timenum;
				*timeden_p = mainll_p->u.ssv_p->timeden;
			}

			/* bail out if multirest encountered */
			if (mainll_p->str == S_STAFF && mainll_p->u.staff_p->
						groups_p[0]->is_multirest)
				return (0);

			/* remember last staff of this number */
			if (mainll_p->str == S_STAFF && mainll_p->u.staff_p->
						staffno == staffno)
				mst_p = mainll_p;
		}
		/* if end of song, set to last bar line and return this staff*/
		if (mainll_p == 0) {
			stuff_p->end.count = timenum + 1;
			return (mst_p);
		}
	}

	/*
	 * mainll_p points at the bar line preceding the place where the stuff
	 * ends.  Continue forward to find the correct STAFF.
	 */
	for (mainll_p = mainll_p->next ;
			mainll_p != 0 && mainll_p->str != S_BAR;
			mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV &&
				mainll_p->u.ssv_p->used[TIME] == YES)
			*timeden_p = mainll_p->u.ssv_p->timeden;

		if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno == staffno)
			break;
	}

	/* if end of song, set to last bar line and return this staff */
	if (mainll_p == 0) {
		stuff_p->end.count = timenum + 1;
		return (mst_p);	/* hit end of song, return last meas */
	}
	if (mainll_p->str == S_BAR)
		pfatal("stuff crosses FEED where number of staffs changes");
	if (mainll_p->u.staff_p->groups_p[0]->is_multirest)
		return (0);

	return (mainll_p);
}

/*
 * Name:        lefttime()
 *
 * Abstract:    Find time value of the nongrace group left of the given count.
 *
 * Returns:     time value into measure
 *
 * Description: This function finds the nongrace GRPSYL in the given linked
 *		list that is at or left of the given count number.  If the
 *		count is less 1, we return time zero, even though technically
 *		time zero is to the right of the given count number.
 */

static RATIONAL
lefttime(count, firstgs_p, timeden)

double count;			/* which count of the measure */
struct GRPSYL *firstgs_p;	/* first GRPSYL of relevant voice in measure */
int timeden;			/* denominator of current time signature */

{
	RATIONAL reqtime;	/* time requested */
	RATIONAL tottime;	/* total time in measure so far */
	RATIONAL otottime;	/* old total time in measure so far */
	struct GRPSYL *gs_p;	/* point along group list */
	struct GRPSYL *ogs_p;	/* (old) point along group list */


	/* skip over any initial grace groups */
	if (firstgs_p->grpvalue == GV_ZERO)
		firstgs_p = nextnongrace(firstgs_p);

	/* if at or before the first count, have to use first group */
	if (count <= 1)
		return (Zero);

	/*
	 * Get requested time to the nearest half of the smallest fraction of a
	 * count that a user can specify, +1, in lowest terms.  The +1 is so
	 * that if the user isn't too accurate, we still land on the intended
	 * group.
	 */
	reqtime.n = 2 * MAXBASICTIME * (count - 1) + 0.5 + 1.0;
	reqtime.d = 2 * MAXBASICTIME * timeden;
	rred(&reqtime);

	/*
	 * Loop through this voice accumulating time values.  As soon as we
	 * equal or exceed the requested time value, return the previous
	 * group's time.
	 */
	otottime = tottime = Zero;
	for (ogs_p = firstgs_p, gs_p = nextnongrace(ogs_p); gs_p != 0;
				ogs_p = gs_p, gs_p = nextnongrace(gs_p)) {
		otottime = tottime;
		tottime = radd(tottime, ogs_p->fulltime);
		if (GE(tottime, reqtime))
			return (otottime);
	}

	/* requested time is after last group; return time of last group */
	return (otottime);
}

/*
 * Name:        righttime()
 *
 * Abstract:    Find time value of the nongrace group right of the given count.
 *
 * Returns:     time value into measure
 *
 * Description: This function finds the nongrace GRPSYL in the given linked
 *		list that is at or right of the given count number.  If the
 *		count is greater then the rightmost group in the measure, we
 *		return the time up to the rightmost group, even though
 *		technically that time is to the left of the given count number.
 *		The GRPSYL list can be nonexistent, and then we return Zero.
 */

static RATIONAL
righttime(count, firstgs_p, timeden)

double count;			/* which count of the measure */
struct GRPSYL *firstgs_p;	/* first GRPSYL of relevant voice in measure */
int timeden;			/* denominator of current time signature */

{
	RATIONAL reqtime;	/* time requested */
	RATIONAL tottime;	/* total time in measure so far */
	struct GRPSYL *gs_p;	/* point along group list */
	struct GRPSYL *ogs_p;	/* (old) point along group list */


	/* if no linked list, arbitrarily return Zero */
	if (firstgs_p == 0) {
		return (Zero);
	}

	/* skip over any initial grace groups */
	if (firstgs_p->grpvalue == GV_ZERO)
		firstgs_p = nextnongrace(firstgs_p);

	/* if at or before the first count, use first group */
	if (count <= 1)
		return (Zero);

	/*
	 * Get requested time to the nearest half of the smallest fraction of a
	 * count that a user can specify, -1, in lowest terms.  The -1 is so
	 * that if the user isn't too accurate, we still land on the intended
	 * group.
	 */
	reqtime.n = 2 * MAXBASICTIME * (count - 1) + 0.5 - 1.0;
	reqtime.d = 2 * MAXBASICTIME * timeden;
	rred(&reqtime);

	/*
	 * Loop through this voice accumulating time values.  As soon as we
	 * equal or exceed the requested time value, return that new time,
	 * although don't go beyond the last group's time value.
	 */
	tottime = Zero;
	for (ogs_p = firstgs_p, gs_p = nextnongrace(ogs_p); gs_p != 0;
				ogs_p = gs_p, gs_p = nextnongrace(gs_p)) {
		tottime = radd(tottime, ogs_p->fulltime);
		if (GE(tottime, reqtime))
			return (tottime);
	}

	/* requested time is after last group; but must return last group */
	return (tottime);
}

/*
 * Name:        accdimen()
 *
 * Abstract:    Find the dimensions of a note's accidental.
 *
 * Returns:     void
 *
 * Description: This function finds the ascent, descent, and width of a note's
 *		accidentals, and returns them through pointers.  If a pointer
 *		is null, it doesn't try to fill it in.  The function takes into
 *		account whether the accidental is normal or small size, and
 *		whether it has parentheses around it.
 *		NOTE:  This function ignores staffscale.
 */

void
accdimen(staffno, note_p, ascent_p, descent_p, width_p)

int staffno;			/* staff where this acc is */
struct NOTE *note_p;		/* the note whose accidental we're working on*/
float *ascent_p;		/* ascent, to be filled in */
float *descent_p;		/* descent, to be filled in */
float *width_p;			/* width, to be filled in */

{
	float final_asc, final_desc, final_wid;	/* answers to be returned */
	float asc, desc;	/* results for one accidental */
	int size;		/* which size of character */
	float halfhigh;		/* half the height of a parenthesis */
	int idx;		/* loop through acclist */


	/* start with size of zero */
	final_asc  = 0.0;
	final_desc = 0.0;
	final_wid  = 0.0;

	size = size_def2font(note_p->notesize);

	/* loop through every accidental on this note */
	for (idx = 0; idx < MAX_ACCS*2 && note_p->acclist[idx] != 0; idx += 2) {

		int musfont = FONT_MUSIC;
		int muschar = note_p->acclist[idx + 1];

		/* override if appropriate; accs are at staff level, not voice*/
		(void)get_shape_override(staffno, 0, &musfont, &muschar);

		/* maintain the maximum ascent so far */
		asc  =   ascent(note_p->acclist[idx], size, muschar);
		final_asc = MAX(final_asc, asc);

		/* maintain the maximum descent so far */
		desc =  descent(note_p->acclist[idx], size, muschar);
		final_desc = MAX(final_desc, desc);

		/* accumulate the width */
		final_wid += width(note_p->acclist[idx], size, muschar);
	}

	/*
	 * If it has parentheses around them, account for that.  Assume the left
	 * and right parens are symmetrical.  They will be centered on the line
	 * or space of the note.
	 */
	if (note_p->acc_has_paren) {
		final_wid += 2 * width(FONT_TR, size, '(');
		halfhigh = height(FONT_TR, size, '(') / 2.0;
		final_asc = MAX(final_asc, halfhigh);
		final_desc = MAX(final_desc, halfhigh);
	}

	if (ascent_p != 0) {
		*ascent_p  = final_asc;
	}
	if (descent_p != 0) {
		*descent_p = final_desc;
	}
	if (width_p != 0) {
		*width_p   = final_wid;
	}
}

/*
 * Name:        has_accs()
 *
 * Abstract:    Decides whether the given acclist has accs in it.
 *
 * Returns:     YES or NO
 *
 * Description: This function is given an acclist.  If that list contains any
 *		accidentals, it returns YES.  If we are running MIDI, it checks
 *		for any accs.  Otherwise, it checks only for visible accs.
 */

int
has_accs(acclist)

char *acclist;			/* accidental list */

{
	int idx;


	/* if MIDI, just check first position to see if any accs exist */
	if (Doing_MIDI) {
		return (acclist[0] == 0 ? NO : YES);
	}

	/* not MIDI; loop through each possible accidental */
	for (idx = 0; idx < MAX_ACCS * 2 && acclist[idx] != 0; idx += 2) {
		/* consider only accs that are visible */
		if (ACC_IS_VISIBLE(&acclist[idx])) {
			return (YES);
		}
	}

	/* beyond end of list and didn't find any */
	return (NO);

}

/*
 * Name:        eq_accs()
 *
 * Abstract:    Decides whether two acclists are the same.
 *
 * Returns:     YES or NO
 *
 * Description: This function is given two acclists.  It returns YES if they
 *		are the same, including the order of accidentals.  But if we
 *		are not running MIDI, it checks only visible accidentals.
 */

int
eq_accs(acclist1, acclist2)

char *acclist1;			/* first accidental list */
char *acclist2;			/* second accidental list */

{
	int idx1, idx2;		/* index into each list */


	/* if MIDI, just check for identical lists */
	if (Doing_MIDI) {
		return (strncmp(acclist1, acclist2, MAX_ACCS * 2) == 0 ?
				YES : NO);
	}

	/* not MIDI; loop through each accidental in the first list */
	idx2 = 0;
	for (idx1 = 0; idx1 < MAX_ACCS * 2 && acclist1[idx1] != 0; idx1 += 2) {

		/* ignore it if it is not visible */
		if ( ! ACC_IS_VISIBLE(&acclist1[idx1])) {
			continue;
		}

		/* starting where we left off in the second list, find the
		 * next visible acc, if any exist */
		while (idx2 < MAX_ACCS * 2 && acclist2[idx2] != 0 &&
				! ACC_IS_VISIBLE(&acclist2[idx2])) {
			idx2 += 2;
		}

		/* if none, we failed to match the current acc in list 1 */
		if (idx2 == MAX_ACCS * 2 || acclist2[idx2] == 0) {
			return (NO);
		}

		/* fail if current accs in lists 1 and 2 are not equal */
		if (acclist1[idx1] != acclist2[idx2] ||
				acclist1[idx1 + 1] != acclist2[idx2 + 1]) {
			return (NO);
		}

		/* prepare to look at next acc in list 2 */
		idx2 += 2;
	}

	/* list 1 is used up, so if there's another visible in list 2, fail */
	for ( ; idx2 < MAX_ACCS * 2 && acclist2[idx2] != 0; idx2 += 2) {
		if (ACC_IS_VISIBLE(&acclist2[idx2])) {
			return (NO);
		}
	}

	return (YES);
}

/*
 * Name:        standard_acc()
 *
 * Abstract:    Convert an acclist to a standard accidental, if possible.
 *
 * Returns:     std acc as # & etc., \0 if no acc or not a single standard acc
 *
 * Description: This function is given an acclist.  If that list contains one
 *		and only one acc and it is one of the standard set of five, it
 *		returns the character symbol for it, one of x # n & B.
 *		Otherwise it returns \0.  It considers all accs if MIDI,
 *		otherwise only visible accs.
 */

int
standard_acc(acclist)

char *acclist;			/* accidental list */

{
	int idx;
	int std_acc;


	std_acc = '\0';		/* haven't found one yet */

	/* loop through every position in acclist */
	for (idx = 0; idx < MAX_ACCS * 2 && acclist[idx] != 0; idx += 2) {
		if (acclist[idx] == FONT_MUSIC) {
			switch (acclist[1]) {
			case C_NAT:
			case C_SHARP:
			case C_FLAT:
			case C_DBLSHARP:
			case C_DBLFLAT:
				/* found std acc: if already found one, fail */
				if (std_acc != '\0') {
					return ('\0');
				}
				/* remember the std acc we found */
				std_acc = acclist[1];
				break;
			default:
				/* nonstd: fail if MIDI or visible else ignore*/
				if (Doing_MIDI == YES || 
						ACC_IS_VISIBLE(&acclist[idx])) {
					return ('\0');
				}
				break;
			}
		} else {
			/* non std acc: fail if MIDI or visible, else ignore */
			if (Doing_MIDI == YES || 
					ACC_IS_VISIBLE(&acclist[idx])) {
				return ('\0');
			}
		}
	}

	/* return either \0 or a standard acc */
	return (char2acc(std_acc));
}

/*
 * Name:        standard_to_acclist()
 *
 * Abstract:    Convert a standard accidental to an acclist.
 *
 * Returns:     void
 *
 * Description: This function is given a standard accidental, in the form of a
 *		character representing it (like "&" for flat, etc.).  It
 *		populates the given acclist with this acc.
 */

void
standard_to_acclist(acc, acclist)

int acc;			/* the given acc (B, &, n, #, x, \0) */
char *acclist;			/* accidental list to be populated */

{
	CLEAR_ACCS(acclist);		/* wipe out any old accs */
	if (acc != '\0') {
		acclist[0] = FONT_MUSIC;
		acclist[1] = acc2char(acc);
	}
}

/*
 * Name:        acc_beam()
 *
 * Abstract:    Move a beam so that it won't collide with accidentals.
 *
 * Returns:     the beam's new Y intercept value, or same if not changed
 *
 * Description: This function looks at the accidentals in the given group, and
 *		if they are colliding with the beam, it changes the beam's Y
 *		intercept, b0, so that it no longer collides.
 *		NOTE:  This function takes staffscale into account.  The SSVs
 *		need not be up to date, but Stepsize must be set.
 */

double
acc_beam(gs_p, b1, b0, ycoordtype, numbeams)

struct GRPSYL *gs_p;		/* group whose notes we should check */
double b1;			/* slope of beam */
double b0;			/* Y intercept of beam */
int ycoordtype;			/* RY or AY */
int numbeams;			/* number of beams */

{
	int n;				/* loop through notes in this group */
	struct NOTE *note_p;		/* a note in this group */
	float accx, accy, wid;		/* dimensions of a note's acc(s) */
	float beamy;			/* y of beam where nearest to acc(s) */

	for (n = 0; n < gs_p->nnotes; n++) {

		note_p = &gs_p->notelist[n];
		if ( ! has_accs(note_p->acclist)) {
			continue;	/* no accs, nothing to do */
		}

		if (gs_p->stemdir == UP) {
			/* get ascent and width */
			accdimen(gs_p->staffno,note_p, &accy, (float *)0, &wid);
		} else {	/* stemdir == DOWN */
			/* get descent and width */
			accdimen(gs_p->staffno,note_p, (float *)0, &accy, &wid);
			accy = -accy;	/* need descent as negative */
		}
		accy += note_p->c[ycoordtype];

		/* find x for side of acc(s) where beam is closest; */
		/*  start on the left side of acc */
		accx = gs_p->c[AX] + note_p->waccr;
		if ((b1 < 0.0) == (gs_p->stemdir == UP)) {
			/* but for these cases, find the right side */
			accx += wid;
		}

		/* find y of nearest beam where nearest to acc(s) */
		beamy = b1 * accx + b0 + beam_offset(numbeams,
				gs_p->grpsize, gs_p->stemdir);

		/* pad by a step; move beam's y intercept if overlap */
		if (gs_p->stemdir == UP) {
			beamy -= Stepsize;
			if (beamy < accy) {
				b0 += accy - beamy;
			}
		} else {	/* stemdir == DOWN */
			beamy += Stepsize;
			if (beamy > accy) {
				b0 -= beamy - accy;
			}
		}
	}

	return (b0);
}

/*
 * Name:        staffvertspace()
 *
 * Abstract:    Find the minimum amount of vertical space a staff should have.
 *
 * Returns:     the amount of vertical distance in inches
 *
 * Description: This function finds the minimum amount of vertical space that
 *		should be allocated for a staff, based on how many lines it has
 *		and whether it is tablature.  This does not take into account
 *		the extra space required by things sticking out farther; it's
 *		just for the staff itself, plus the extra white space required
 *		by staffs that have few lines.  The SSVs must be up to date.
 *		NOTE:  This function takes staffscale into account.
 */

double
staffvertspace(s)

int s;				/* staff number */

{
	float space;		/* the answer */


	/*
	 * Base space on number of steps between top and bottom lines.  But for
	 * tablature, it must be scaled because the lines are farther apart.
	 */
	space = (svpath(s, STAFFLINES)->stafflines - 1) * 2 * STEPSIZE;
	if (is_tab_staff(s))
		space *= TABRATIO;

	/* but don't ever return less than a (scaled) regular 5 line staff */
	return (svpath(s, STAFFSCALE)->staffscale * MAX(space, 8.0 * STEPSIZE));
}

/*
 * Name:        halfstaffhi()
 *
 * Abstract:    Find half of the staff height.
 *
 * Returns:     half the staff height in inches
 *
 * Description: This function finds half of the staff's height, based on how
 *		many lines it has and whether it is tablature.  This does not
 *		take into account the extra space required by things sticking
 *		out farther; it's just for the staff itself, except that one
 *		line staffs are given a minimum instead of the zero you would
 *		expect.  The SSVs must be up to date.
 *		NOTE:  This function takes staffscale into account.
 */

double
halfstaffhi(s)

int s;				/* staff number */

{
	float space;		/* the answer */


	/*
	 * Base space on the number of steps between the top line and the
	 * middle of the staff.  But for tablature, it must be scaled because
	 * the lines are farther apart.
	 */
	space = (svpath(s, STAFFLINES)->stafflines - 1) * STEPSIZE;
	if (is_tab_staff(s))
		space *= TABRATIO;

	/* but don't ever return less than one (scaled) stepsize */
	return (MAX(space, STEPSIZE) * svpath(s, STAFFSCALE)->staffscale);
}

/*
 * Name:        ratbend()
 *
 * Abstract:    Convert a bend distance to rational.
 *
 * Returns:     the rational number answer; 0/1 if null bend or no bend
 *
 * Description: This function, given a NOTE structure from a tab staff, returns
 *		the amount of the bend (if any) as a rational number.
 */

RATIONAL
ratbend(note_p)

struct NOTE *note_p;

{
	RATIONAL answer;


	if (note_p->BEND == 0)
		return (Zero);

	answer.d = BENDDEN(*note_p);
	answer.n = BENDNUM(*note_p) + BENDINT(*note_p) * answer.d;
	rred(&answer);

	return (answer);
}

/*
 * Name:        notehorz()
 *
 * Abstract:    Find horizontal boundary of note and associated things.
 *
 * Returns:     the RE or RW
 *
 * Description: This function finds the horizontal boundary of a note,
 *		including accidentals, dots, etc., all the things that can be
 *		on the note.  The note's own RE and RW only tell about the note
 *		head itself.
 *		NOTE:  This function takes staffscale into account.  The SSVs
 *		need not be up to date, but Staffscale and Stdpad must be set.
 */

double
notehorz(gs_p, note_p, coord)

struct GRPSYL *gs_p;		/* the group the note is in */
struct NOTE *note_p;		/* point at the note */
int coord;			/* RE or RW */

{
	int s;			/* index into slurtolist */
	double h;		/* the answer */


	if (coord == RE) {
		if (note_p->note_has_paren == YES &&
					! is_tab_staff(gs_p->staffno)) {
			/*
			 * If there are parens around the note, start there.
			 * Note: this field does not apply on tab staff; it
			 * is only there for carrying over to tabnote staff.
			 * Tab staff uses FRET_HAS_PAREN, and this distance is
			 * included in the size of the "note" (fret) itself.
			 */
			h = note_p->erparen;
		} else  {
			/*
			 * If non-tablature and there are dots, start from the
			 * first dot.  Otherwise start from the note.
			 */
			if (is_tab_staff(gs_p->staffno) == NO &&
					gs_p->dots > 0) {
				h = gs_p->xdotr + 6 * Stdpad;
				if (gs_p->dots > 1) {
					h += (gs_p->dots - 1) * (2 * Stdpad +
					width(FONT_MUSIC, DFLT_SIZE, C_DOT));
				}
			} else {
				h = note_p->c[RE] + Stdpad;
			}
		}

		/*
		 * If this note has a slur to nowhere (and there can be at most
		 * one such), include its length.
		 */
		for (s = 0; s < note_p->nslurto; s++) {
			if (note_p->slurtolist[s].octave == OUT_UPWARD ||
			    note_p->slurtolist[s].octave == OUT_DOWNWARD) {
				h += Staffscale * (SLIDEXLEN + STDPAD);
				break;
			}
		}

	} else {	/* RW */

		if (gs_p->sep_accs == YES && note_p->noteleft_string != 0) {
			/* if string left of note, start there */
			h = note_p->wlstring;
		} else if (note_p->note_has_paren == YES &&
					! is_tab_staff(gs_p->staffno)) {
			/* if parens around note, start there */
			h = note_p->wlparen;
		} else if (is_tab_staff(gs_p->staffno) == NO &&
				gs_p->sep_accs == YES &&
				has_accs(note_p->acclist)) {
			/* if there's an accidental, start there */
			/* (this includes any parens around the accidental) */
			h = note_p->waccr;
		} else {
			/* the note head itself, with padding */
			h = note_p->c[RW] - Stdpad;
		}

		/*
		 * If this note has a slur from nowhere (and there can be at
		 * most one such), include its length.
		 */
		for (s = 0; s < note_p->nslurto; s++) {
			if (note_p->slurtolist[s].octave == IN_UPWARD ||
			    note_p->slurtolist[s].octave == IN_DOWNWARD) {
				h -= Staffscale * (SLIDEXLEN + STDPAD);
				break;
			}
		}
	}

	return (h);
}

/*
 * Name:        allsmall()
 *
 * Abstract:    Do the given groups (of notes) consist only of small/tiny notes?
 *
 * Returns:     YES or NO
 *
 * Description: This function is given pointer to two GRPSYLs in a linked list.
 *		They may point to the same GRPSYL, or the second may point to a
 *		later GRPSYL in the list.  The function returns YES if all the
 *		notes in these GRPSYLs and any intervening GRPSYLs are small
 *		or tiny.  Any GRPSYLs that are not for notes are ignored.
 */

int
allsmall(gs1_p, gs2_p)

struct GRPSYL *gs1_p;	/* starting group */
struct GRPSYL *gs2_p;	/* ending group (may be same as starting group) */

{
	struct GRPSYL *gs_p;	/* point along the list */
	int n;			/* index into notelist */


	/* check every group, and return NO if anything is normal size */
	for (gs_p = gs1_p; gs_p != gs2_p->next; gs_p = gs_p->next) {
		if (gs_p->grpcont == GC_NOTES && gs_p->grpsize == GS_NORMAL) {
			for (n = 0; n < gs_p->nnotes; n++) {
				if (gs_p->notelist[n].notesize == GS_NORMAL)
					return (NO);
			}
		}
	}

	return (YES);	/* everything must have been small or tiny */
}

/*
 * Name:        size_def2font()
 *
 * Abstract:    Convert a size #define to a font size.
 *
 * Returns:     font size
 *
 * Description: The input is a GS_* size value.  The output is *SIZE.
 */

int
size_def2font(defsize)

int defsize;

{
	switch (defsize) {
	case GS_NORMAL:
		return (DFLT_SIZE);
	case GS_SMALL:
		return (SMALLSIZE);
	case GS_TINY:
		return (TINYSIZE);
	}

	pfatal("size_def2font was called with bad defsize %d", defsize);
	return (0);		/* actually never reached */
}

/*
 * Name:        finalstemadjust()
 *
 * Abstract:    Make final adjustments to the stem length.
 *
 * Returns:     void
 *
 * Description: This function makes final adjustments to the stem length that
 *		all the cases have in common.  Coming in, it is set to the
 *		stem length not counting the part between notes of a multinote
 *		group, and it doesn't account for the thickness of a beam.
 *		The SSVs must be up to date.
 *		NOTE:  This function takes staffscale into account.
 */

void
finalstemadjust(gs_p)

struct GRPSYL *gs_p;	/* group whose stemlen should be adjusted */

{
	float stepdiff;		/* distance between outer notes in steps */


	/* if it is negative (note on wrong side of beam), zap it */
	if (gs_p->stemlen < 0)
		gs_p->stemlen = 0;

	/* add distance between outer notes of group */
	stepdiff = gs_p->notelist[0].c[RY] -
			gs_p->notelist[ gs_p->nnotes - 1 ].c[RY];
	gs_p->stemlen += stepdiff;

	/*
	 * Decr the length by half the thickness of the beam, but don't let it
	 * get less than the distance between the outer notes of the group.
	 */
	gs_p->stemlen -= HALF_BEAM_THICKNESS(gs_p);
	gs_p->stemlen = MAX(gs_p->stemlen, stepdiff);
}

/*
 * Name:        getstemshift()
 *
 * Abstract:    Find how far a stem is from the group's X coordinate.
 *
 * Returns:     the distance in inches
 *
 * Description: This function finds how far a group's stem is shifted
 *		horizontally from the group's X coordinate.
 *		NOTE:  This function takes staffscale into account.
 */

double
getstemshift(gs_p)

struct GRPSYL *gs_p;	/* group whose stemlen should be adjusted */

{
	switch (gs_p->grpcont) {
	case GC_NOTES:
		/* return half the width of the widest note in the group */
		return (svpath(gs_p->staffno, STAFFSCALE)->staffscale *
				widest_head(gs_p) / 2.0);
	case GC_REST:
		/* for rests, RE == -RW, or close enough, so return RE */
		return (svpath(gs_p->staffno, STAFFSCALE)->staffscale *
				gs_p->c[RE]);
	case GC_SPACE:
		/* semi-arbitrary */
		return (0.0);
	default:
		pfatal("invalid grpcont in getstemshift()");
	}

	return (0.0);
}

/*
 * Name:        vscheme_voices()
 *
 * Abstract:    Given a vscheme, how many voices are in it?
 *
 * Returns:     number of voices
 *
 * Description: This function is given one of the valid vschemes, and it
 *		returns the number of voices that vscheme allows.
 */

int
vscheme_voices(vscheme)

int vscheme;		/* the given vscheme */

{
	switch (vscheme) {
	case V_1:
		return (1);

	case V_2OPSTEM:
	case V_2FREESTEM:
		return (2);

	case V_3OPSTEM:
	case V_3FREESTEM:
		return (3);

	default:
		pfatal("invalid vscheme in vscheme_voices()");
	}

	return (0);
}

/*
 * Name:        chmgrp2staffm()
 *
 * Abstract:    Given a chord and group, find the group's staff.
 *
 * Returns:     pointer to staff's MLL item
 *
 * Description: This function is given the MLL item for a chord, and a group
 *		connected to the chord.  It returns the MLL item for the staff
 *		that the group is connected to.  The group can belong to any of
 *		the staff's voices.
 */

struct MAINLL *
chmgrp2staffm(mll_p, gs_p)

struct MAINLL *mll_p;		/* starts as MLL for the chord */
struct GRPSYL *gs_p;		/* starts as GRPSYL the given group */

{
	/* find the first group in this measure */
	for ( ; gs_p->prev != 0; gs_p = gs_p->prev)
		;

	/* find the staff that it belongs to */
	for ( ; mll_p != 0; mll_p = mll_p->next) {

		if (mll_p->str == S_STAFF &&
		(mll_p->u.staff_p->groups_p[0] == gs_p ||
		 mll_p->u.staff_p->groups_p[1] == gs_p ||
		 mll_p->u.staff_p->groups_p[2] == gs_p))
			break;
	}
	if (mll_p == 0)
		pfatal("can't find staff in chmgrp2staffm");

	return (mll_p);
}

/*
 * Name:        shiftgs()
 *
 * Abstract:    Shift a GRPSYL's relative horizontal coords.
 *
 * Returns:     void
 *
 * Description: This function is a GRPSYL and a shift amount.  It shifts the
 *		GRPSYL's relative horizontal coords by that amount.
 */

void
shiftgs(gs_p, shift)

struct GRPSYL *gs_p;		/* the main GRPSYL */
double shift;

{
	gs_p->c[RX] += shift;
	gs_p->c[RW] += shift;
	gs_p->c[RE] += shift;
}

/*
 * Name:        nearestline()
 *
 * Abstract:    Round a vertical offset to the nearest staff line.
 *
 * Returns:     the resulting offset
 *
 * Description: This function rounds its parameter off to a multiple of 2
 *		stepsizes.
 *		NOTE:  This function takes staffscale into account.  The SSVs
 *		need not be up to date, but Stepsize must be set.
 */

double
nearestline(offset)

double offset;			/* offset from center staff line */

{
	if (offset >= 0) {
		offset /= (2 * Stepsize);
		offset = (int)(offset + 0.5);
		offset *= (2 * Stepsize);
	} else {
		offset = -offset;
		offset /= (2 * Stepsize);
		offset = (int)(offset + 0.5);
		offset *= (2 * Stepsize);
		offset = -offset;
	}

	return (offset);
}

/*
 * Name:        vfyoffset()
 *
 * Abstract:    Verify horizontal offsets are not in conflict.
 *
 * Returns:     void
 *
 * Description: This function prints a warning if the horizontal offsets of
 *		voices 1 and 2 are in conflict.  In that case it also zaps
 *		the bad offsets.
 */

void
vfyoffset(g_p)

struct GRPSYL *g_p[];		/* array of pointers to two groups */

{
	/* the only errors are cases where "+" and "-" are used */
	if (g_p[0]->ho_usage != HO_LEFT && g_p[0]->ho_usage != HO_RIGHT)
		return;

	/* can't both be "+" or both be "-" */
	if (g_p[0]->ho_usage == g_p[1]->ho_usage) {

		l_warning(
			g_p[1]->inputfile, g_p[1]->inputlineno,
			"voices 1 and 2 cannot both have horizontal offset '%c'; ignoring them",
			g_p[0]->ho_usage == HO_LEFT ? '-' :'+');

		g_p[0]->ho_usage = HO_NONE;
		g_p[1]->ho_usage = HO_NONE;
	}
}

/*
 * Name:        adjslope()
 *
 * Abstract:    Adjust the slope of a beam or a tuplet bracket.
 *
 * Returns:     the new slope
 *
 * Description: This function is given the slope of a beam or tuplet bracket
 *		as determined by linear regression.  It adjusts it according
 *		to the given parameter.
 */

double
adjslope(g_p, oldslope, betweencsb, param)

struct GRPSYL *g_p;	/* pointer to GRPSYL to get staff and voice from */
double oldslope;	/* the given slope */
int betweencsb;		/* is this beam CSB and between the staffs? */
int param;		/* BEAMSLOPE or TUPLETSLOPE */

{
	struct SSV *ssv_p;	/* for getting fact and max */
	float fact;		/* to multiply by */
	float max;		/* max angle in degrees */
	float newangle;		/* the adjusted angle */


	/* find parameter values */
	ssv_p = vvpath(g_p->staffno, g_p->vno, param);
	if (param == BEAMSLOPE) {
		fact = ssv_p->beamfact;
		max  = ssv_p->beammax;
	} else {
		fact = ssv_p->tupletfact;
		max  = ssv_p->tupletmax;
	}

	/*
	 * If cross staff beaming and the beam is between the staffs, we allow
	 * a somewhat bigger angle, because it may be necessary to avoid
	 * collisions.  (Tuplet brackets don't get printed for CSB so this code
	 * won't run for them.)
	 */
	if (betweencsb == YES) {
		max *= 1.4;
	}

	/* new angle = old angle * fact */
	newangle = (atan(oldslope) * 180.0 / PI) * fact;

	/* force it to stay within the limit */
	if (newangle > max) {
		newangle = max;
	} else if (newangle < -max) {
		newangle = -max;
	}

	/* return as slope */
	return (tan(newangle * PI / 180.0));
}

/*
 * Name:	curve_y_at_x()
 *
 * Abstract:	Given a curve and an X value, return the Y value there.
 *
 * Returns:	the Y value
 *
 * Description:	This function should only be called for curves where the X
 *		value of each point in the curve list is greater than the
 *		previous point's X value, although it's okay if the curve
 *		itself is not strictly increasing in X value all the time as
 *		you go from the start to the end.
 *
 *		If the X value given is less than the first point's, it returns
 *		the Y of the first point.  If the X value is greater than the
 *		last point's, it returns the Y of the last point.  Otherwise,
 *		it returns a Y value of the curve at that X value.  I say "a"
 *		Y value, because if the curve isn't strictly increasing, there
 *		can be multiple answers, and it just returns one of them.
 *
 *		The function assumes that the curve points will be connected by
 *		cubic curves, according to the algorithm in calccurve() and
 *		findcontrol().
 */

double
curve_y_at_x(first_p, x)

struct CRVLIST *first_p;	/* left endpoint of curve */
double x;			/* X coord at which we need Y */

{
	float y;		/* the answer */
	float a, b, c;		/* coefficients for a cubic */
	struct CRVLIST *left_p, *right_p; /* endpoints of a cubic segment */
	float rotangle;		/* rotate new system to get old (in radians) */
	float tranx, trany;	/* a point translated to another coord system */
	float pointx, pointy;	/* trans & rotated in another coord system */
	float lineslope, intercept;	/* of a line through the given x */
	float cos_rotangle, sin_rotangle;	/* for saving these values */
	float deltax, deltay;	/* of endpoints of segment between 2 points */
	float len;		/* length of segment between 2 points */


	/*
	 * If the first point of the curve is at or already beyond the given x,
	 * return the first point's y.
	 */
	if (first_p->x >= x) {
		return (first_p->y);
	}
	right_p = 0;	/* for lint */
	for (left_p = first_p; left_p->next != 0; left_p = left_p->next) {
		right_p = left_p->next;
		/* if x is right at this point, use this point's y */
		if (right_p->x == x) {
			return (right_p->y);
		}
		/* if x is within this interval, break out */
		if (left_p->x < x && x < right_p->x) {
			break;
		}
	}
	/* if this happens, x is beyond the last point, so use last point's y */
	if (left_p->next == 0) {
		return (left_p->y);
	}

	/*
	 * The given x is between the x coords of two of the points in the
	 * curvelist.  So we need to find the cubic arc that calccurve() and
	 * findcontrol() would use, if this curve is going to be used.  The
	 * cubic arc is determined in a translated/rotated coordinate system
	 * where left_p is (0,0) and right_p is on the positive X axix.
	 * rotangle is the angle from the segment between left_p and right_p
	 * to the real X axis.  The cubic, in the translated/rotated system, is
	 * y = a x^3 + b x^2 + c x.  It turns out that the constant term is
	 * always zero.
	 */
	rotangle = findcubic(left_p, right_p, &a, &b, &c);

	/*
	 * If left_p->y == right_p->y, rotangle is zero, meaning no rotation was
	 * necessary, only translation.  In that case we can simply plug into
	 * the cubic we found, adjusting for the translation.  A fudge factor is
	 * needed so that we don't take the tangent of almost 90 degrees below.
	 */
	if (fabs(rotangle) < 0.001) {
		pointx = x - left_p->x;
		pointy = ((a * pointx + b) * pointx + c) * pointx;
		y = pointy + left_p->y;
		return (y);
	}

	/*
	 * Rotation was necessary.  In the original coord system, picture a
	 * vertical line at the given x value.  It intersects the cubic,
	 * possibly in more than one place.  We want the y value at the
	 * intersection.  In the translated/rotated system, this line has a
	 * slope as determine below.
	 */
	if (rotangle < 0.0) {
		lineslope = tan(PI / 2.0 + rotangle);
	} else {
		lineslope = tan(-PI / 2.0 + rotangle);
	}

	/*
	 * In the real coord system, the vertical line hits (x, 0).  Find this
	 * point in the translated/rotated system.
	 */
	/* first translate */
	tranx = x - left_p->x;
	trany = -left_p->y;
	/* then rotate */
	cos_rotangle = cos(rotangle);	/* save to avoid recalculation */
	sin_rotangle = sin(rotangle);
	pointx = tranx * cos_rotangle - trany * sin_rotangle;
	pointy = trany * cos_rotangle + tranx * sin_rotangle;

	/* find Y intercept in the translated/rotated system */
	intercept = pointy - lineslope * pointx;

	/*
	 * Now, in the tran/rot coord system, we need to find the intersection
	 * of this line
	 *	y  =  lineslope * x  +  intercept
	 * and the cubic
	 *	y  =  a * x^3  +  b * x^2  +  c * x
	 * Setting the two values of y equal, we get
	 *	lineslope * x  +  intercept  =  a * x^3  +  b * x^2  +  c * x
	 * or
	 *	a * x^3  +  b * x^2  + (c - lineslope) * x  -  intercept  =  0
	 */
	/* find intersection point in the tran/rot coord system */
	deltax = right_p->x - left_p->x;
	deltay = right_p->y - left_p->y;
	len = sqrt(SQUARED(deltax) + SQUARED(deltay));
	pointx = solvecubic(a, b, c-lineslope, -intercept,
			0.0, len, Stdpad / 10.0);
	pointy = lineslope * pointx + intercept;

	/* rotate backwards, getting Y value */
	trany = pointy * cos_rotangle - pointx * sin_rotangle;

	/* translate back to the original coord system */
	y = trany + left_p->y;

	return (y);
}

/*
 * Name:	findcubic()
 *
 * Abstract:	Given neighboring curve points, find cubic and rotation angle.
 *
 * Returns:	angle from new coord system's X axis to old system's (radians)
 *
 * Description:	This function uses a new coordinate system, where the left
 *		curve point is (0, 0), and the right curve point is on the
 *		positive X axis.  It finds the coefficients for the cubic arc
 *		that will be put through these points.  It returns the angle
 *		that the old coord system needs to be rotated by to get to
 *		the new system.
 */

double
findcubic(left_p, right_p, a_p, b_p, c_p)

struct CRVLIST *left_p;		/* left endpoint of cubic arc */
struct CRVLIST *right_p;	/* right endpoint of cubic arc */
float *a_p, *b_p, *c_p;		/* return the answers here, the coefficients */

{
	double langle;		/* half angle from prev segment to this one */
	double rangle;		/* half angle from this segment to next one */
	float deltax, deltay;	/* for this segment */
	float len;		/* of this segment */
	float lslope, rslope;	/* slope of tangent line at left & right point*/
	float thisang, prevang, nextang; /* angle of segment with horizontal */


	langle = rangle = 0.0;	/* for lint */

	/* get angle of this segment */
	thisang = atan((right_p->y - left_p->y) / (right_p->x - left_p->x));

	if (left_p->prev != 0) {
		/* there is a previous segment; find its angle */
		prevang = atan((left_p->y - left_p->prev->y) /
			       (left_p->x - left_p->prev->x));
		/* half the change in angle */
		langle = (thisang - prevang) / 2.0;
	}
	if (right_p->next != 0) {
		/* there is a next segment; find its angle */
		nextang = atan((right_p->next->y - right_p->y) /
			       (right_p->next->x - right_p->x));
		/* half the change in angle */
		rangle = (nextang - thisang) / 2.0;
	}
	if (left_p->prev == 0) {
		/* no previous segment; use same angle as on the right */
		langle = rangle;
	}
	if (right_p->next == 0) {
		/* no next segment; use same angle as on the left */
		rangle = langle;
	}

	/* get lengths of this segment */
	deltax = right_p->x - left_p->x;
	deltay = right_p->y - left_p->y;
	len = sqrt(SQUARED(deltax) + SQUARED(deltay));

	/*
	 * Rotate and translate the axes so that the starting point (left_p)
	 * is at the origin, and the ending point (right_p) is on the positive
	 * x axis.  Their coords are (0, 0) and (len, 0).  We are going to
	 * find a cubic equation that intersects the endpoints, and has the
	 * required slope at those points.  The equation is
	 *	y  =  a x^3  +  b x^2  +  c x  +  d
	 * so the slope is
	 *	y' =  3 a x^2  +  2 b x  +  c
	 * By plugging the two points into these, you get 4 equations in the 4
	 * unknowns a, b, c, d.
	 */
	/* find the slope of the tangent lines at the first & second points */
	lslope = -tan(langle);
	rslope = tan(rangle);

	/* set values of a, b, c (d turns out to be always 0) */
	*a_p = (lslope + rslope) / SQUARED(len);
	*b_p = (-2.0 * lslope - rslope) / len;
	*c_p = lslope;

	return (-thisang);
}

/*
 * Name:	solvecubic()
 *
 * Abstract:	Find a solution to a cubic equation within a given interval.
 *
 * Returns:	the solution
 *
 * Description:	This function is given the coefficients of a cubic equation and
 *		the boundaries of an interval.  The function must be positive
 *		at one end and negative at the other (or zero is okay at
 *		either).  It uses the "modified regula falsi" algorithm to find
 *		a solution, meaning that it keeps narrowing down the interval.
 *		It stops when the inverval size <= the threshhold given.  But
 *		in case something goes wrong, it also stops after 20 loops.
 */

double
solvecubic(a, b, c, d, lo, hi, thresh)

double a, b, c, d;	/* in equation   a x^3 + b x^2 + c x + d = 0  */
double lo, hi;		/* low and high boundaries of interval to look in */
double thresh;		/* how close must result be to the true answer */

#define FUNC(x) (((a * x + b) * x + c) * x + d)
{
	float lovert, hivert;	/* y values */
	float mid, midvert;	/* a point in the middle and its y value */
	float oldmidvert;	/* midvert in previous loop */
	int n;


	lovert = FUNC(lo);
	hivert = FUNC(hi);

	/*
	 * If the function is positive at both endpoints or negative at both
	 * endpoints, it was called incorrectly.  But we're going to allow for
	 * a small violation of this due to presumed roundoff error.  If one
	 * endpoint if "very near" zero, we'll pretend it was zero and return
	 * it as the solution.
	 */
	if (lovert * hivert > 0.0) {
		if (fabs(lovert) < thresh)
			return (lo);
		if (fabs(hivert) < thresh)
			return (hi);
		pfatal("solvecubic was called with an invalid interval");
	}

	mid = lo;
	midvert = lovert;

	for (n = 0; n < 20 && hi - lo > thresh; n++) {
		oldmidvert = midvert;

		/*
		 * Find a point somewhere in the interval by passing a segment
		 * through (lo, lovert) and (hi, hivert) and seeing where it
		 * hits the X axis.
		 */
		mid = (lovert * hi - hivert * lo) / (lovert - hivert);
		midvert = FUNC(mid);

		/*
		 * Set either the hi or the lo equal to the mid.  If the value
		 * at mid is the same sign as the previous one, divide the
		 * vert value by 2, so we can eventually get the segment to
		 * hit on the other side.
		 */
		if ((lovert > 0.0) != (midvert > 0.0)) {
			hi = mid;
			hivert = midvert;
			if ((midvert > 0.0) == (oldmidvert > 0.0)) {
				lovert /= 2.0;
			}
		} else {
			lo = mid;
			lovert = midvert;
			if ((midvert > 0.0) == (oldmidvert > 0.0)) {
				hivert /= 2.0;
			}
		}
	}

	return (mid);
}

/*
 * Name:        css_affects_stemtip()
 *
 * Abstract:    Do CSS notes (if any) affect the position of the stem's tip?
 *
 * Returns:     YES or NO
 *
 * Description: This function is given a pointer to a GRPSYL.  It must be a
 *		note group, but can be grace or nongrace.  It may be a member
 *		of a beamed set, or not a member of a beamed set.  It decides
 *		whether the position of the tip of the stem (or where the stem
 *		would be for a non-stemmed note) is affected by CSS notes.
 */

int
css_affects_stemtip(gs_p)

struct GRPSYL *gs_p;	/* starts at the given group */

{
	/*
	 * For the single (unbeamed) group case, the position of the tip of the
	 * stem is affected if either the CSS notes are on the stem side, or if
	 * all the notes are CSS.
	 */
	if (gs_p->beamloc == NOITEM) {
		return (STEMSIDE_CSS(gs_p) || NNN(gs_p) == 0 ? YES : NO);
	}

	/* CSB is never CSS */
	if (gs_p->beamto != CS_SAME) {
		return (NO);
	}

	/*
	 * For the beamed case, either all or none of the groups can have CSS
	 * notes on the stem side, if there are any other-staff notes at all
	 * in the group.  This is because all the groups' stems go
	 * the same direction, and we don't allow the beaming together of
	 * groups where some have stemto == CS_ABOVE and others have CS_BELOW.
	 * Theoretically a group with all CSS notes could affect the position
	 * of the beam regardless of whether its CSS notes are stemside or not;
	 * but we will pretend that it can't, except in the case where its
	 * stem length is being forced by the user.  We'll fake things out in
	 * setbeam().  This way, we can handle beaming and set the beam
	 * position and good group boundaries on the beamside during the
	 * CSSpss==NO pass.  Then the placement of "stuff" on that side will
	 * be better.
	 */
	/* if we're not at the start of the beamed set, go back to there */
	while (gs_p->beamloc != STARTITEM) {
		gs_p = prevsimilar(gs_p);
	}
	if (NNN(gs_p) == 0 && IS_STEMLEN_KNOWN(gs_p->stemlen)) {
		return (YES);	/* user forcing stem len of first group */
	}
	/* check each member to see if any have stemside CSS */
	while (gs_p != 0) {
		if (STEMSIDE_CSS(gs_p)) {
			return (YES);
		}
		if (gs_p->beamloc == ENDITEM) {
			if (NNN(gs_p) == 0 && IS_STEMLEN_KNOWN(gs_p->stemlen)) {
				return (YES);	/* forced last group's stemlen*/
			}
			break;
		}
		gs_p = nextsimilar(gs_p);
	}
	return (NO);
}

/*
 * Name:        css_affects_tieslurbend()
 *
 * Abstract:    Do CSS notes (if any) affect the position of this tie/slur/bend?
 *
 * Returns:     YES or NO
 *
 * Description: This function decides whether the given tie, slur, or bend is
 *		affected by CSS notes in any of the groups it covers.
 */

int
css_affects_tieslurbend(stuff_p, mll_p)

struct STUFF *stuff_p;	/* the tie, slur, or bend */
struct MAINLL *mll_p;	/* MLL item where this tie/slur/bend starts */

{
	struct GRPSYL *sg_p;	/* starting group of the tie/slur/bend */
	struct GRPSYL *eg_p;	/* starting group of the tie/slur/bend */
	struct NOTE *snote_p;	/* starting note of the tie/slur/bend */
	struct NOTE *enote_p;	/* ending note of the tie/slur/bend */
	int idx;		/* index of note in the group */


	/* if not cross staff stemming, don't waste time checking */
	if (CSSused == NO) {
		return (NO);
	}

	/* second half (after crossing scorefeed); was handled by first half */
	if (stuff_p->carryin == YES) {
		return (NO);
	}

	sg_p = stuff_p->beggrp_p;
	snote_p = stuff_p->begnote_p;

	/* find the index of the note in the group */
	for (idx = 0; idx < sg_p->nnotes; idx++) {
		if (&sg_p->notelist[idx] == snote_p) {
			break;
		}
	}
	if (idx == sg_p->nnotes) {
		pfatal("can't find tied/slurred/bent note in group");
	}

	/* if this starting note is CSS, return YES */
	if (IS_CSS_NOTE(sg_p, idx)) {
		return (YES);
	}

	/*
	 * Find the end note of the tie/slur/bend.  If none, we don't care
	 * about the end note.
	 */
	eg_p = nextgrpsyl(sg_p, &mll_p);
	if (eg_p == 0) {
		return (NO);
	}

	/* find the note tied/slurred/bent to */
	if (stuff_p->curveno == -1) {	/* this is a tie */
		enote_p = find_matching_note(eg_p, mll_p, snote_p->letter,
				snote_p->FRETNO,
				snote_p->octave, (char *)0);
	} else {			/* this is a slur or bend */
		enote_p = find_matching_note(eg_p, mll_p,
				snote_p->slurtolist[stuff_p->curveno].letter,
				-1,
				snote_p->slurtolist[stuff_p->curveno].octave,
				(char *)0);
	}

	if (enote_p == 0) {
		return (NO);
	}

	/* find the index of the note in the group */
	for (idx = 0; idx < eg_p->nnotes; idx++) {
		if (&eg_p->notelist[idx] == enote_p) {
			break;
		}
	}
	if (idx == eg_p->nnotes) {
		pfatal("can't find tied/slurred/bent-to note in group");
	}

	/* if this ending note is CSS, return YES */
	if (IS_CSS_NOTE(eg_p, idx)) {
		return (YES);
	}

	return (NO);
}
/*
 * Name:        css_affects_phrase()
 *
 * Abstract:    Do CSS notes (if any) affect the position of this phrase mark?
 *
 * Returns:     YES or NO
 *
 * Description: This function decides whether the given phrase mark is
 *		affected by CSS notes in any of the groups it covers.
 */

int
css_affects_phrase(stuff_p, mll_p)

struct STUFF *stuff_p;	/* the phrase */
struct MAINLL *mll_p;	/* for the group at start of phrase */

{
	struct GRPSYL *gs_p;	/* point at end group covered by phrase */
	int place;		/* PL_ABOVE or PL_BELOW */
	int staffno;		/* staff number */
	int vidx;		/* voice index */


	place = stuff_p->place;
	gs_p = stuff_p->beggrp_p;
	staffno = gs_p->staffno;
	vidx = gs_p->vno - 1;
	
	/* loop once for each group covered by the phrase */
	while (gs_p != 0) {
		/* return YES right away if we find an affected group */
		switch (gs_p->stemto) {
		case CS_SAME:
			break;
		case CS_ABOVE:
			if (place == PL_ABOVE || NNN(gs_p) == 0) {
				return (YES);
			}
			break;
		case CS_BELOW:
			if (place == PL_BELOW || NNN(gs_p) == 0) {
				return (YES);
			}
			break;
		}

		/* if we've seen the last group, we are done */
		if (gs_p == stuff_p->endgrp_p) {
			break;
		}

		/* find the next note group in this voice */
		do {
			gs_p = gs_p->next;
		} while (gs_p != 0 && gs_p->grpcont != GC_NOTES);

		/* if we hit the end of the measure, look for next measure */
		if (gs_p == 0) {
			/* find the bar line */
			while (mll_p != 0 && mll_p->str != S_BAR) {
				mll_p = mll_p->next;
			}
			/* find the matching staff in the next measure */
			while (mll_p != 0 && ! (mll_p->str == S_STAFF &&
					mll_p->u.staff_p->staffno == staffno)) {
				mll_p = mll_p->next;
			}
			/* defensive check, should not happen */
			if (mll_p == 0) {
				break;
			}
			/* point at the first group in new measure */
			gs_p = mll_p->u.staff_p->groups_p[vidx];
		}
	}

	return (NO);
}

/*
 * Name:        nextsamecont()
 *
 * Abstract:    Return next group in a GRPSYL list that has the same grpcont.
 *
 * Returns:     pointer to GRPSYL of next desired group, 0 if none
 *
 * Description: This function loops down the GRPSYL linked list from the given
 *		starting point.  It returns the next GRPSYL in the list that has
 *		the same grpcont as the given one, or 0 if none.
 */

struct GRPSYL *
nextsamecont(gs_p)

struct GRPSYL *gs_p;	/* current group */

{
	int curcont;

	curcont = gs_p->grpcont;
	gs_p = gs_p->next;
	while (gs_p != 0 && gs_p->grpcont != curcont) {
		gs_p = gs_p->next;
	}
	return (gs_p);
}

/*
 * Name:        nextnonspace()
 *
 * Abstract:    Return next group in a GRPSYL list that is not a space.
 *
 * Returns:     pointer to GRPSYL of next desired group, 0 if none
 *
 * Description: This function loops down the GRPSYL linked list from the given
 *		starting point.  It returns the next GRPSYL in the list that is
 *		not a space, or 0 if none.
 */

struct GRPSYL *
nextnonspace(gs_p)

struct GRPSYL *gs_p;	/* current group */

{
	gs_p = gs_p->next;
	while (gs_p != 0 && gs_p->grpcont == GC_SPACE) {
		gs_p = gs_p->next;
	}
	return (gs_p);
}

/*
 * Name:        nextsimilar()
 *
 * Abstract:    Return next group in a GRPSYL list that is "like" the current.
 *
 * Returns:     pointer to GRPSYL of next desired group, 0 if none
 *
 * Description: This function loop down the GRPSYL linked list from the given
 *		starting point.  It returns the next GRPSYL in the list that has
 *		the same grpcont and grpvalue as the given one, or 0 if none.
 */

struct GRPSYL *
nextsimilar(gs_p)

struct GRPSYL *gs_p;	/* current group */

{
	int curvalue;
	int curcont;

	curvalue = gs_p->grpvalue;
	curcont = gs_p->grpcont;
	gs_p = gs_p->next;
	while (gs_p != 0 &&
	      (gs_p->grpvalue != curvalue || gs_p->grpcont != curcont)) {
		gs_p = gs_p->next;
	}
	return (gs_p);
}

/*
 * Name:        prevsimilar()
 *
 * Abstract:    Return prev group in a GRPSYL list that is "like" the current.
 *
 * Returns:     pointer to GRPSYL of previous desired group, 0 if none
 *
 * Description: This function loops down the GRPSYL linked list from the given
 *		starting point.  It returns the prev GRPSYL in the list that has
 *		the same grpcont and grpvalue as the given one, or 0 if none.
 */

struct GRPSYL *
prevsimilar(gs_p)

struct GRPSYL *gs_p;	/* current group */

{
	int curvalue;
	int curcont;

	curvalue = gs_p->grpvalue;
	curcont = gs_p->grpcont;
	gs_p = gs_p->prev;
	while (gs_p != 0 &&
	      (gs_p->grpvalue != curvalue || gs_p->grpcont != curcont)) {
		gs_p = gs_p->prev;
	}
	return (gs_p);
}

/*
 * Name:        gs2ch()
 *
 * Abstract:    Given a GRPSYL and its staff's MLL, find chord for that time.
 *
 * Returns:     pointer to CHORD structure
 *
 * Description: This function is given a GRPSYL and the MLL structure for the
 *		GRPSYL's staff.  It finds the CHORD structure that heads the
 *		list of GRPSYLs occurring at that time in that measure.  Note
 *		that if the given GRPSYL is grace, it won't actually occur in
 *		that linked list of GRPSYLs; but in that case the following
 *		non-grace GRPSYL will.
 */

struct CHORD *
gs2ch(mll_p, gs_p)

struct MAINLL *mll_p;	/* the MLL for the given GRPSYL */
struct GRPSYL *gs_p;	/* the given GRPSYL */

{
	struct CHORD *ch_p;		/* loop through chords */
	struct GRPSYL *gs2_p;		/* point along a GRPSYL list */
	RATIONAL time;			/* time offset where our group is */


	/* find chord headcell for this measure */
	while (mll_p != 0 && mll_p->str == S_STAFF) {
		mll_p = mll_p->prev;
	}
	if (mll_p == 0 || mll_p->str != S_CHHEAD) {
		pfatal("missing chord head cell in gs2ch");
	}

	/* find time offset of our group by summing all previous groups */
	time = Zero;
	for (gs2_p = gs_p->prev; gs2_p != 0; gs2_p = gs2_p->prev) {
		time = radd(time, gs2_p->fulltime);
	}

	/*
	 * Find the chord that contains our group (or, if we are a grace group,
	 * the following normal group).
	 */
	for (ch_p = mll_p->u.chhead_p->ch_p;
			ch_p != 0 && NE(ch_p->starttime, time);
			ch_p = ch_p->ch_p) {
		;
	}
	if (ch_p == 0) {
		pfatal("can't find chord in gs2ch");
	}

	return (ch_p);
}

/*
 * Name:        stemroom()
 *
 * Abstract:    Try to find how much room a "wrong way" stem needs.
 *
 * Returns:     The room needed, measured in stepsizes.
 *
 * Description: This function is given a nongrace note group whose stem has
 *		been forced the wrong way (down for the top group or up for the
 *		bottom group) despite the other voice being nonspace.  It tries
 *		to find how long the stem will be so that we can decide whether
 *		the groups need to be horizontally offset.  It works well for
 *		nonbeamed groups, but for beamed groups it can only guess.  It
 *		is to be used in places where we need to know the stem length
 *		(to the extent possible) even though beamstem.c hasn't run yet.
 *
 *		WARNING:  This code is similar to the nongrace section of
 *		proclist() in beamstem.c.  If you change one, you probably
 *		will need to change the other.
 */

double
stemroom(gs_p)

struct GRPSYL *gs_p;		/* the group in question */

{
	float room;		/* the answer, in stepsizes */
	int bf;			/* number of beams/flags */


	/*
	 * If user specified stem length, use that.
	 */
	if (IS_STEMLEN_KNOWN(gs_p->stemlen)) {
		return (gs_p->stemlen / STEPSIZE);
	}

	/*
	 * If this group is part of a beamed set, there is no way to know how
	 * long the stem will be, since the beaming hasn't been done yet, and
	 * can't be done until all horizontal placement has been done.  So
	 * return the default stem length and hope for the best.
	 */
	if (gs_p->beamloc != NOITEM) {
		return (DEFSTEMLEN);
	}

	/*
	 * All notes but whole and double whole have stems, but whole and double
	 * whole notes still need to have a pseudo stem length set if
	 * alternation beams are to be drawn between two neighboring
	 * groups, or the group has slashes.
	 */
	if (STEMLESS(gs_p) && gs_p->slash_alt == 0) {
		/* no (pseudo)stem */
		return (0.0);
	}

	/* find default stemlen for this voice */
	if (vvpath(gs_p->staffno, gs_p->vno, STEMLEN)->stemlen == 0.0) {
		return (0.0);
	}
	room = stemextsteps(gs_p);

	/* if small notes, reduce this default */
	room *= (allsmall(gs_p, gs_p) == YES ? SM_STEMFACTOR : 1.0);

	/* add more, if needed, for flags/beams/slashes/alternations */
	if (gs_p->basictime >= 8) {
		bf = drmo(gs_p->basictime) - 2;	/* no. of beams/flags*/
	} else {
		bf = 0;			/* none on quarter or longer */
	}
	bf += abs(gs_p->slash_alt);	/* slashes or alternations */
	if (gs_p->slash_alt > 0 && gs_p->basictime >= 16) {
		bf++;	/* slashes need an extra one if 16, 32, ... */
	}
	if (bf > 2) {
		room += (bf - 2) * FLAGSEP / STEPSIZE;
	}

	/*
	 * If the note may have flag(s), is stem up, and has dot(s), we must
	 * prevent the flag(s) from hitting the dot(s), by lengthening the stem.
	 */
	if (gs_p->basictime >= 8 && gs_p->stemdir == UP && gs_p->dots != 0) {
		if (gs_p->notelist[0].stepsup % 2 == 0) {
			/* note is on a line */
			if (gs_p->basictime == 8) {
				room += 1.0;
			} else {
				room += 2.0;
			}
		} else {
			/* note is on a space */
			if (gs_p->basictime > 8) {
				room += 1.0;
			}
		}
	}

	return (room);
}

/*
 * Name:        stemextsteps()
 *
 * Abstract:    Find a group's default stem len, using stemlen and stemshorten.
 *
 * Returns:     default stem length, in stepsizes
 *
 * Description: Given a group, this function finds its default stem length in
 *		stepsizes, starting with the stemlen parameter but then
 *		applying the stemshorten parameters values for protrusion, and
 *		a rule to keep it from getting "too" short.  The answer is in
 *		stepsizes and does not include the part of the stem that lies
 *		between the outermost notes of the group.
 */

double
stemextsteps(gs_p)

struct GRPSYL *gs_p;		/* the group in question */

{
	float deflen;		/* def length as given by "stemlen" parm */
	float max;		/* from "stemshorten" */
	float len;		/* the proposed length before the 10% rule */
	int beg;		/* from "stemshorten" but then alter */
	int end;		/* from "stemshorten" */
	int stepsup;		/* of the stemside note */
	float shorten;		/* the shortening to be applied */


	deflen = vvpath(gs_p->staffno, gs_p->vno, STEMLEN)->stemlen;

	/* the default is shorter for quad and oct groups */
	if (gs_p->basictime <= BT_QUAD) {
		deflen *= DEFSTEMLEN_LONG / DEFSTEMLEN;
	}

	beg = vvpath(gs_p->staffno, gs_p->vno, BEGPROSHORT)->begproshort;
	end = vvpath(gs_p->staffno, gs_p->vno, ENDPROSHORT)->endproshort;
	max = vvpath(gs_p->staffno, gs_p->vno, MAXPROSHORT)->maxproshort;

	beg--;		/* shortening really begins one step before */

	if (gs_p->stemdir == UP) {
		stepsup = gs_p->notelist[0].stepsup;
		if (stepsup <= beg) {
			return (deflen);	/* no shortening */
		}
		if (stepsup >= end) {
			len = deflen - max;	/* max shortening */
			return (MAX(len, MINSTEMLENFRAC * deflen));
		}
	} else {
		stepsup = gs_p->notelist[gs_p->nnotes - 1].stepsup;
		beg = -beg;
		end = -end;
		if (stepsup >= beg) {
			return (deflen);	/* no shortening */
		}
		if (stepsup <= end) {
			len = deflen - max;	/* max shortening */
			return (MAX(len, MINSTEMLENFRAC * deflen));
		}
	}

	/* apply linearly between the endpoints */
	shorten = max * ((float)stepsup - (float)beg) /
			((float)end - (float)beg);

	len = deflen - shorten;
	return (MAX(len, MINSTEMLENFRAC * deflen));
}

/*
 * Name:        stafftime2firstgrp()
 *
 * Abstract:    Given staff and time offset, find group of minimum voice then.
 *
 * Returns:     pointer to the GRPSYL found, or 0 if not found
 *
 * Description: This function is given a staff, and a time offset.  It tries to
 *		find a nongrace group that starts at that time on this staff in
 *		this measure.  If there are multiple such, it returns the one
 *		having the minimum voice number.
 */

struct GRPSYL *
stafftime2firstgrp(staff_p, time)

struct STAFF *staff_p;		/* the staff */
RATIONAL time;			/* time offset to look for */

{
	struct GRPSYL *gs_p;
	int vno;
	RATIONAL t;


	/*
	 * Look down each voice's GRPSYL list, trying to find a nongrace that
	 * starts at the given time.  If a voice doesn't exist, the inner loop
	 * will execute zero times.
	 */
	for (vno = 0; vno < MAXVOICES; vno++) {
		t = Zero;
		for (gs_p = staff_p->groups_p[vno]; gs_p != 0;
					gs_p = gs_p->next) {

			/* ignore grace groups */
			if (gs_p->grpvalue == GV_ZERO) {
				continue;
			}

			/* if the group starts at the given time return it */
			if (EQ(t, time)) {
				return (gs_p);
			}

			/* if we're beyond the time, go try the next voice */
			if (GT(t, time)) {
				break;
			}
			t = radd(t, gs_p->fulltime);
		}
	}

	return (0);
}

/*
 * Name:        init_rectab()
 *
 * Abstract:    Initialize Rectab, allocating an initial chunk.
 *
 * Returns:     void
 *
 * Description: This function does the initial allocation of Rectab, and
 *		sets Reclim to show it is empty.
 */

static int Rectabsize;

void
init_rectab()
{
	Rectabsize = RECTCHUNK;
	MALLOC(RECTAB, Rectab, Rectabsize);
	Reclim = 0;
}

/*
 * Name:        inc_reclim()
 *
 * Abstract:    Increment no. of rectangles, and realloc more if we run out.
 *
 * Returns:     void
 *
 * Description: This function increments Reclim, the index into Rectab.  If it
 *		finds that Rectab[Reclim + 1] is now beyond the end of the
 *		space that's been allocated, it does a realloc to get more
 *		space.
 */

void
inc_reclim()
{
	Reclim++;

	/* if Rectab[Reclim + 1] is still valid, no need to allocate more */
	if (Reclim + 1 < Rectabsize) {
		return;
	}

	/* must allocate another chunk of rectangles */
	Rectabsize += RECTCHUNK;
	REALLOC(RECTAB, Rectab, Rectabsize);
}

/*
 * Name:        free_rectab()
 *
 * Abstract:    Free the memory used by of Rectab.
 *
 * Returns:     void
 *
 * Description: This function frees Rectab.
 */

void
free_rectab()
{
	FREE(Rectab);
}

/*
 * Name:        clef_vert_overlap()
 *
 * Abstract:    Does the clef overlap vertically with the group?
 *
 * Returns:     YES or NO
 *
 * Description: This function is given a clef and a group.  It is to be called
 *		at a time when the vertical group boundaries have been set to
 *		include the notes, but we haven't dealt with stems and "with"
 *		lists yet.  It tries to determine whether they overlap
 *		vertically, but it can't always tell at this stage of the game.
 *		When in doubt, it plays it safe and returns YES.
 *		NOTE:  This function takes staffscale into account.  The SSVs
 *		need not be up to date, but Staffscale and Stepsize must be set.
 */

int
clef_vert_overlap(clef, gs_p)

int clef;			/* clef type */
struct GRPSYL *gs_p;		/* the group to be checked */

{
	float grp_rn, grp_rs;	/* group vertical boundaries, as best we know */
	float clef_rn, clef_rs;	/* clef vertical boundaries */
	float stemextend;	/* how far the stem extends from note center */
	float stemtip;		/* relative coord of stem tip */


	/* a space never overlaps anything */
	if (gs_p->grpcont == GC_SPACE) {
		return (NO);
	}

	/*
	 * For the clef boundaries, this convenient function gives the answer,
	 * though we have to do the adjustment for staff scale.
	 */
	(void)clefvert(clef,  gs_p->staffno, YES, &clef_rn, &clef_rs);
	clef_rn *= Staffscale;
	clef_rs *= Staffscale;

	/*
	 * For the group boundaries, start from what we know at this stage of
	 * the game, which is just based on the notes or rest.
	 */
	grp_rn = gs_p->c[RN];
	grp_rs = gs_p->c[RS];

	/*
	 * For the note case (excluding mrpt), there may be a stem, so try to
	 * find out how far that will force us to extend one of the boundaries.
	 */
	if (gs_p->nnotes > 0) {
		/*
		 * If we are not beamed, stemroom() will return an accurate
		 * answer.  Otherwise, it can't predict accurately.  Instead of
		 * using its guess of DEFSTEMLEN, force a huge answer, to play
		 * it safe and not let it overlap the clef, if the clef is on
		 * that side.
		 */
		if (gs_p->beamloc == NOITEM) {
			stemextend = stemroom(gs_p) * Stepsize;
		} else {	/* beamed */
			stemextend = 1000.0;
		}

		/*
		 * Extend the appropriate boundary.  Allow for the case of zero
		 * length stem, and the bizarre case where it's positive but so
		 * short that the notehead bulges out farther than the stem
		 * tip, so we don't extend the group boundary at all.
		 */
		if (gs_p->stemdir == UP) {
			stemtip = gs_p->notelist[0].stepsup *
						Stepsize + stemextend;
			if (stemtip > grp_rn) {
				grp_rn = stemtip;
			}
		} else {	/* DOWN */
			stemtip = gs_p->notelist[gs_p->nnotes - 1].stepsup *
						Stepsize - stemextend;
			if (stemtip < grp_rs) {
				grp_rs = stemtip;
			}
		}
	}

	/* extend for the "with" on each side, if any */
	grp_rn += withheight(gs_p, PL_ABOVE);
	grp_rs -= withheight(gs_p, PL_BELOW);

	/* now we can return the answer */
	if (clef_rn < grp_rs || clef_rs > grp_rn) {
		return (NO);
	} else {
		return (YES);
	}
}

/*
 * Name:        allow_subbar()
 *
 * Abstract:    Decide which staffs can have a subbar at the given chord.
 *
 * Returns:     void
 *
 * Description: Pass in the array subbar_ok, and it will be populated with YES
 *		for each staff that can have a subbar at the given chord, and
 *		NO if not.
 */

void
allow_subbar(ch_p, sb_app_p, subbar_ok)

struct CHORD *ch_p;		/* a chord in that measure */
struct SUBBAR_APPEARANCE *sb_app_p;	/* says what staffs it can be on */
short subbar_ok[];		/* index by staff number, to be filled in */

{
	int s;				/* staff number */
	struct GRPSYL *gs_p;


	for (s = 1; s <= Score.staffs; s++) {
		subbar_ok[s] = NO;	/* init to disallowing subbar */
	}

	for (gs_p = ch_p->gs_p; gs_p != 0; gs_p = gs_p->gs_p) {

		s = gs_p->staffno;

		/* only visible staffs can have subbar */
		if (svpath(s, VISIBLE)->visible == NO) {
			continue;
		}

		/* staff must be in some range where we want subbars */
		if (staff_wants_subbar(sb_app_p, s) == NO) {
			continue;
		}

		/* syllables are irrelevant */
		if (gs_p->grpsyl == GS_SYLLABLE) {
			continue;
		}

		/* spaces count for nothing */
		if (gs_p->grpcont == GC_SPACE) {
			continue;
		}

		/* a note or rest starts here, so we want the subbar */
		subbar_ok[s] = YES;
	}
}

/*
 * Name:        staff_wants_subbar()
 *
 * Abstract:    Find whether this staff wants to have this subbar.
 *
 * Returns:     YES or NO
 *
 * Description: Find whether the given subbar can apply to the given staff,
 *		if other conditions are met.
 */

static int
staff_wants_subbar(sb_app_p, s)

struct SUBBAR_APPEARANCE *sb_app_p;
int s;

{
	struct TOP_BOT *range_p;
	int ridx;			/* index to a range */


	/* if this staff falls inside any range, allow subbar */
	for (ridx = 0; ridx < sb_app_p->nranges; ridx++) {
		range_p = &sb_app_p->ranges_p[ridx];
		if (range_p->all == YES) {
			return (YES);
		}
		if (s >= range_p->top && s <= range_p->bottom) {
			return (YES);
		}
	}

	return (NO);
}
