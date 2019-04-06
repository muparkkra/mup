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
 * Name:	trantab.c
 *
 * Description:	This file contains functions for translating tablature staffs
 *		to their corresponding tabnote staffs.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/*
 * Define a temp structure for holding info about the note generated from a
 * NOTE on a tablature staff.
 */
struct TABNOTENOTE {
	char letter;		/* 'a' to 'g' */
	char accidental;	/* '\0', 'x', '#', 'n', '&', 'B'(double flat)*/
	int octave;		/* 0 to 9 */
	int hs;			/* half steps above c0 */
	int strno;		/* the string this note is played on */
	struct NOTE *note_p;	/* point to the NOTE this is derived from */
};

static void tabv2tabnotev P((struct MAINLL *mainll_p, int vidx));
static void fixprevmeas P((struct GRPSYL *ngs_p, struct MAINLL *mll_p));
static int hasprebend P((struct GRPSYL *gs_p));
static int cancombine P((struct MAINLL *mll_p, struct GRPSYL *tgs_p,
		RATIONAL *comp_p));
static RATIONAL calctime P((struct GRPSYL *gs_p));
static void translate_group P((struct GRPSYL *tgs_p, struct GRPSYL *pregs_p,
		struct GRPSYL *ngs_p, struct MAINLL *mll_p, int combine));
static void calcnote P((struct STRINGINFO *sinfo_p, int fretno, RATIONAL bend,
		int sharps, struct TABNOTENOTE *note_p, struct GRPSYL *tgs_p));
static int findfret P((struct GRPSYL *tgs_p, struct MAINLL *mll_p, int strno));
static RATIONAL findbend P((struct GRPSYL *tgs_p, struct MAINLL *mll_p,
		int strno));
static void popnotes P((struct GRPSYL *gs_p, struct TABNOTENOTE tnn[],
		int ntnn, struct GRPSYL *tgs_p, struct MAINLL *mll_p,
		int is_prebend, int combine, int sharps));
static int neighbor P((struct TABNOTENOTE *high_p, struct TABNOTENOTE *low_p));
static int inkeysig P((struct TABNOTENOTE *note_p, int sharps));
static int upletter P((struct TABNOTENOTE *note_p));
static int downletter P((struct TABNOTENOTE *note_p));
static void cleanaccs P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
static char *standard_accstr P((char *acclist, char *file, int line));

/*
 * Name:        tab2tabnote()
 *
 * Abstract:    Populate tabnote staffs' voices from corresponding tab voices.
 *
 * Returns:     void
 *
 * Description: This function loops through the MLL looking for tab staffs.
 *		For each it does some work, but the main thing is that it loops
 *		through each voice and calls tabv2tabnotev() for each GRPSYL
 *		list.  That function populates the corresponding tabnote voice.
 */

void
tab2tabnote()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct STAFF *nstaff_p;		/* tabnote STAFF */
	struct GRPSYL *ngs_p;		/* tabnote GRPSYL */
	int vidx;


	debug(2, "tab2tabnote");
	initstructs();

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p); /* keep SSVs up to date */
			continue;
		}

		/* if not a tab staff, there's nothing to do */
		if (mainll_p->str != S_STAFF ||
		    is_tab_staff(mainll_p->u.staff_p->staffno) == NO) {
			continue;
		}

		/*
		 * The previous staff must be a tabnote staff.  Set pointers to
		 * this tabnote staff and its first voice's first GRPSYL.
		 */
		nstaff_p = mainll_p->prev->u.staff_p;
		ngs_p = nstaff_p->groups_p[0];

		/*
		 * If this is a multirest, adjust any octave marks in progress.
		 */
		if (ngs_p->basictime < -1 &&
				Octave_bars[nstaff_p->staffno] > 0) {

			/* add negative bars plus 1; barline will count as 1 */
			Octave_bars[nstaff_p->staffno] += ngs_p->basictime + 1;

			/* if whole octave stuff is done, re-init */
			if (Octave_bars[nstaff_p->staffno] < 0) {
				Octave_bars[nstaff_p->staffno] = 0;
				Octave_count[nstaff_p->staffno] = 0.0;
				Octave_adjust[nstaff_p->staffno] = 0;
			}
		}

		/* loop through all possible voices, populating tabnote */
		for (vidx = 0; vidx < MAXVOICES; vidx++) {
			tabv2tabnotev(mainll_p, vidx);
		}
	}
}

/*
 * Name:        tabv2tabnotev()
 *
 * Abstract:    Populate a tabnote voice from its corresponding tab voice.
 *
 * Returns:     void
 *
 * Description: This function populates a tabnote GRPSYL list based on its
 *		corresponding tab GRPSYL list, after making sure that the voice
 *		exists for each.
 */

static void
tabv2tabnotev(mainll_p, vidx)

struct MAINLL *mainll_p;	/* main LL struct for this staff */
int vidx;			/* voice index, 0 to MAXVOICES-1 */

{
	struct STAFF *tstaff_p;		/* tablature STAFF */
	struct STAFF *nstaff_p;		/* tabnote STAFF */
	struct GRPSYL *tgs_p;		/* tablature GRPSYL */
	struct GRPSYL *ngs_p;		/* tabnote GRPSYL */
	struct GRPSYL *nngs_p;		/* next tabnote GRPSYL */
	struct GRPSYL *gs_p;		/* newly allocated GRPSYL */
	struct GRPSYL *pregs_p;		/* GRPSYL for prebend's grace group */
	int combine;			/* should two groups be combined? */
	RATIONAL totdur;		/* total duration of combined groups */
	int n;				/* loop variable */
	char *acc_p;			/* pointer to accidental string */


	/*
	 * Set pointers to the tablature staff's STAFF structure and the first
	 * GRPSYL in the voice we're working on.
	 */
	tstaff_p = mainll_p->u.staff_p;
	tgs_p = tstaff_p->groups_p[vidx];

	/* if this voice doesn't exist, there is nothing to copy to tabnote */
	if (tgs_p == 0) {
		return;
	}

	/*
	 * The previous staff must be a tabnote staff.  Set pointers to this
	 * tabnote staff and the first GRPSYL of the given voice.
	 */
	nstaff_p = mainll_p->prev->u.staff_p;
	ngs_p = nstaff_p->groups_p[vidx];

	/* tabnote staff must have at least as many voices as tab staff */
	if (ngs_p == 0) {
		l_ufatal(tgs_p->inputfile, tgs_p->inputlineno,
			"tab staff %d has a voice %d, but tabnote staff %d doesn't; change vscheme for tabnote staff",
			nstaff_p->staffno + 1, vidx + 1, nstaff_p->staffno);
	}

	/*
	 * If the tabnote staff/voice doesn't have a measure space, it was
	 * manually entered, and we will leave it alone.  However, it is
	 * possible that this staff/voice in the previous measure was
	 * generated, and that the last GRPSYL there had note(s) that were to
	 * slide to our GRPSYL.  If so, that slurtolist needs to be adjusted.
	 */
	if (ngs_p->is_meas == NO || ngs_p->grpcont != GC_SPACE) {
		fixprevmeas(ngs_p, mainll_p->prev);
		return;
	}

	FREE(ngs_p);		/* throw away the measure space */

	/*
	 * Loop once for each GRPSYL in this measure on the tablature staff/
	 * voice.  Usually, for each one found, we allocate one for the tabnote
	 * staff/voice.  But in cases involving prebends, we allocate a second
	 * one, since they translate to a grace note (in parentheses) and a
	 * normal note.  And in certain cases of bends of <= 1/4 step, we don't
	 * allocate any, since we combine the two notes into one.
	 */
	combine = NO;
	for ( ; tgs_p != 0; tgs_p = tgs_p->next) {
		/*
		 * If this group was combined into the previous group because
		 * of 1/4 step bends, we don't allocate any group now.
		 */
		if (combine) {
			combine = NO;
			continue;
		}

		/*
		 * Check whether any of the notes in this tab group have
		 * prebends.  If so, allocate a special GRPSYL for the grace
		 * note(s) in parentheses before the main group.  Link it and
		 * set its fields.
		 */
		if (hasprebend(tgs_p)) {
			CALLOC(GRPSYL, pregs_p, 1);
			if (tgs_p == tstaff_p->groups_p[vidx]) {
				/* first GRPSYL in list; no previous one, */
				/*  staff points at us */
				pregs_p->prev = 0;
				nstaff_p->groups_p[vidx] = pregs_p;
			} else {
				/* a later one; set both links */
				pregs_p->prev = ngs_p;
				ngs_p->next = pregs_p;
			}
			pregs_p->next = 0;	/* last one so far */
			ngs_p = pregs_p;

			/* set fields unless they'd be just hardcoded 0 */
			pregs_p->inputlineno = tgs_p->inputlineno;
			pregs_p->inputfile = tgs_p->inputfile;
			pregs_p->staffno = nstaff_p->staffno;
			pregs_p->vno = vidx + 1;
			pregs_p->grpsyl = GS_GROUP;
			pregs_p->basictime = 4;	/* stemless 1/4 note */
			pregs_p->is_meas = NO;
			pregs_p->tuploc = NOITEM;
			pregs_p->fulltime = Zero;
			pregs_p->grpcont = GC_NOTES;
			pregs_p->grpvalue = GV_ZERO;
			pregs_p->grpsize = GS_SMALL;
			pregs_p->headshape = HS_UNKNOWN;
			pregs_p->clef = NOCLEF;
			pregs_p->beamloc = NOITEM;
			pregs_p->beamto = CS_SAME;
			pregs_p->stemto = CS_SAME;
			pregs_p->tie = NO;
			pregs_p->roll = NOITEM;
			pregs_p->ho_usage = HO_NONE;
			/* nnotes and notelist get set later */
		} else {
			pregs_p = 0;		/* no prebend grace */
		}

		/*
		 * Allocate a GRPSYL.  Initialize it to be the same as the tab
		 * GRPSYL, since most fields are the same.  Later we'll change
		 * the ones that are different.  Link it appropriately to the
		 * staff (if it's the first one) or else the previous GRPSYL.
		 */
		CALLOC(GRPSYL, gs_p, 1);
		*gs_p = *tgs_p;
		if (tgs_p == tstaff_p->groups_p[vidx] && pregs_p == 0) {
			/* first one; no previous one, staff points at us */
			gs_p->prev = 0;
			nstaff_p->groups_p[vidx] = gs_p;
		} else {
			/* later one; set both links */
			gs_p->prev = ngs_p;
			ngs_p->next = gs_p;
		}
		gs_p->next = 0;		/* last one so far */
		ngs_p = gs_p;		/* use ngs_p var for new one */

		/* set the correct staff number */
		ngs_p->staffno = nstaff_p->staffno;

		/* can't share same withlist because fonts may be */
		/*  changed in them differently later */
		clone_withlist(ngs_p, tgs_p);

		/* allocate a separate restc so tab and tabnote don't interfere,
		 * even though tab doesn't print rests */
		if (ngs_p->grpcont == GC_REST) {
			CALLOCA(float, ngs_p->restc, NUMCTYPE);
		}

		/* notelist will be reset later; nnotes might change */

		combine = cancombine(mainll_p, tgs_p, &totdur);
		if (combine) {
			/* calc basictime and dots of combined note */
			ngs_p->basictime = reconstruct_basictime(totdur);
			ngs_p->dots = recalc_dots(totdur, ngs_p->basictime);

			/* combine fulltime (works even for tuplets) */
			ngs_p->fulltime = radd(tgs_p->fulltime,
					tgs_p->next->fulltime);
		}

		if (tgs_p->grpcont == GC_NOTES && ! is_mrpt(ngs_p)) {
			translate_group(tgs_p, pregs_p, ngs_p,
					mainll_p, combine);
		}
	}

	/*
	 * For measure repeats, mark the staff and get out.  If there is more
	 * than one voice, each will hit this code, but that's okay.
	 */
	if (is_mrpt(ngs_p)) {
		nstaff_p->mrptnum = tstaff_p->mrptnum;
		return;
	}

	/* blow away unneeded accidentals */
	cleanaccs(nstaff_p->groups_p[vidx], mainll_p->prev);

	/*
	 * Because we may have combined some groups, we might have lost the
	 * STARTITEM or ENDITEM of some beamed groups.  So find this situation
	 * and fix it.  This can only affect nongrace groups, so ignore grace
	 * groups.
	 */
	ngs_p = nstaff_p->groups_p[vidx];	/* first group in measure */
	if (ngs_p->grpvalue == GV_ZERO)	/* if grace, */
		ngs_p = nextnongrace(ngs_p);	/* get first nongrace*/

	for ( ; ngs_p != 0; ngs_p = nngs_p) {
		/* get the next group, if any, excluding graces */
		nngs_p = nextnongrace(ngs_p);

		/*
		 * For this and the next group, if it's a quarter or longer,
		 * make sure it won't get beamed.
		 */
		if (ngs_p->basictime <= 4)
			ngs_p->beamloc = NOITEM;
		if (nngs_p != 0 && nngs_p->basictime <= 4)
			nngs_p->beamloc = NOITEM;

		/* based on this and next group, change this group */
		switch (ngs_p->beamloc) {
		case STARTITEM:
			if (nngs_p == 0 ||
			    nngs_p->beamloc == STARTITEM ||
			    nngs_p->beamloc == NOITEM)
				ngs_p->beamloc = NOITEM;
			break;
		case INITEM:
			if (nngs_p == 0 ||
			    nngs_p->beamloc == STARTITEM ||
			    nngs_p->beamloc == NOITEM)
				ngs_p->beamloc = ENDITEM;
			break;
		case ENDITEM:
		case NOITEM:
			if (nngs_p != 0) {
				if (nngs_p->beamloc == INITEM)
					nngs_p->beamloc = STARTITEM;
				else if (nngs_p->beamloc == ENDITEM)
					nngs_p->beamloc = NOITEM;
			}
			break;
		}
	}

	/*
	 * Now we have start and end items on every beamed set, but it is
	 * possible that some of them are rests (if there were rests embedded
	 * in the original set).  So revise to get them out of there.
	 */
	ngs_p = nstaff_p->groups_p[vidx];	/* first group in measure */
	if (ngs_p->grpvalue == GV_ZERO)	/* if grace, */
		ngs_p = nextnongrace(ngs_p);	/* get first nongrace*/

	for ( ; ngs_p != 0; ngs_p = nextnongrace(ngs_p)) {
		int notegroups;		/* number in this set */
		struct GRPSYL *end_p;	/* point at the enditem */

		if (ngs_p->beamloc != STARTITEM)
			continue;
		/*
		 * We found a startitem; count how many note groups in the set.
		 * Also set end_p to point at the end item in case we need it
		 * later.
		 */
		notegroups = 0;
		end_p = 0;	/* avoid useless warnings */
		for (nngs_p = ngs_p; nngs_p != 0 &&
				(nngs_p->prev == 0 ||
				nngs_p->prev->beamloc != ENDITEM);
				nngs_p = nextnongrace(nngs_p)) {
			if (nngs_p->grpcont == GC_NOTES)
				notegroups++;
			end_p = nngs_p;
		}

		if (notegroups <= 1) {
			/* 0 or 1 note groups; blow away the set */
			for (nngs_p = ngs_p; nngs_p != 0 &&
					(nngs_p->prev == 0 ||
					nngs_p->prev->beamloc!=ENDITEM);
					nngs_p = nextnongrace(nngs_p)) {
				nngs_p->beamloc = NOITEM;
			}
		} else {
			/*
			 * There are at least two note groups, so we will keep
			 * the set, but we may need to move the endpoints to
			 * avoid rests.
			 */
			/* remove any rests at the start */
			for (nngs_p = ngs_p; nngs_p->grpcont == GC_REST;
					nngs_p = nextnongrace(nngs_p)) {
				nngs_p->beamloc = NOITEM;
			}
			nngs_p->beamloc = STARTITEM;
			/* remove any rests at the end */
			for (nngs_p = end_p; nngs_p->grpcont == GC_REST;
					nngs_p = prevnongrace(nngs_p)) {
				nngs_p->beamloc = NOITEM;
			}
			nngs_p->beamloc = ENDITEM;
		}
	}

	/* do beaming of tabnote staff based on beamstyle */
	if (has_cust_beaming(nstaff_p->groups_p[vidx]) == NO) {
		do_beaming(nstaff_p->groups_p[vidx], GS_NORMAL,
					nstaff_p->staffno, vidx + 1);
		do_beaming(nstaff_p->groups_p[vidx], GS_SMALL,
					nstaff_p->staffno, vidx + 1);
	}

	/*
	 * When there are octave marks on the tabnote staff, adjust the notes
	 * the opposite way so that the result is correct.  Then check to make
	 * sure nothing is out of range.
	 */
	octave_transpose(nstaff_p, mainll_p, vidx, NO);

	for (ngs_p = nstaff_p->groups_p[vidx]; ngs_p != 0; ngs_p = ngs_p->next){
		for (n = 0; n < ngs_p->nnotes; n++) {
			if (ngs_p->notelist[n].octave < MINOCTAVE ||
			    ngs_p->notelist[n].octave > MAXOCTAVE) {

				acc_p = standard_accstr(
					ngs_p->notelist[n].acclist,
					ngs_p->inputfile, ngs_p->inputlineno);

				l_ufatal(ngs_p->inputfile,
					ngs_p->inputlineno,
					"'octave' string on tabnote staff transposes note %c%s to out of range octave %d",
					ngs_p->notelist[n].letter,
					acc_p,
					ngs_p->notelist[n].octave);
			}
		}
	}
}

/*
 * Name:        fixprevmeas()
 *
 * Abstract:    Fixed unresolved slides in the previous tabnote measure.
 *
 * Returns:     void
 *
 * Description: This function is called for the first GRPSYL of a manually
 *		entered tabnote measure.  If the previous tabnote measure was
 *		generated, it might be trying to slide to a note in our GRPSYL.
 *		If so, its slurtolist would have a string number in place of a
 *		letter and NOFRET for the octave.  This function resolves this
 *		to the true letter and octave.
 */

static void
fixprevmeas(ngs_p, mll_p)

struct GRPSYL *ngs_p;		/* tabnote GRPSYL */
struct MAINLL *mll_p;		/* MLL for this GRPSYL */

{
	struct GRPSYL *prevngs_p;	/* previous GRPSYL */
	struct SLURTO *s_p;		/* point at a SLURTO structure */
	int j, k;			/* for looping through notes */


	/* find previous GRPSYL, if any; if not notes, nothing to do */
	prevngs_p = prevgrpsyl(ngs_p, &mll_p);
	if (prevngs_p == 0 || prevngs_p->grpcont != GC_NOTES)
		return;

	/*
	 * Check every note in the preceding group.  Don't break out after
	 * finding one, since multiple ones could slide to our note.  But there
	 * is a max of one slur/bend coming from each note.
	 */
	s_p = 0;		/* prevent useless 'used before set' warning */
	for (k = 0; k < prevngs_p->nnotes; k++) {
		/* check every slur from that note */
		for (j = 0; j < prevngs_p->notelist[k].nslurto; j++) {
			s_p = &prevngs_p->notelist[k].slurtolist[j];

			/* only deal with unresolved tabslurs */
			if (s_p->letter < MAXTABLINES && s_p->octave == NOFRET)
				break;
		}
		/* if found a tabslur to our GRPSYL */
		if (j < prevngs_p->notelist[k].nslurto) {
			/* if our GRPSYL has no notes, this is no good */
			if (ngs_p->grpcont != GC_NOTES) {
				l_warning(prevngs_p->inputfile,
				prevngs_p->inputlineno,
				"no note on tabnote staff to slide to from %c%s%d",
				prevngs_p->notelist[k].letter,
				standard_accstr(
					prevngs_p->notelist[k].acclist,
					prevngs_p->inputfile,
					prevngs_p->inputlineno),
				prevngs_p->notelist[k].octave);

				prevngs_p->notelist[k].nslurto = 0;

			/* if our GRPSYL has >= 2 notes, this is no good */
			} else if (ngs_p->nnotes >= 2) {
				l_warning(prevngs_p->inputfile,
				prevngs_p->inputlineno,
				"can't slide from %c%s%d because multiple notes in the next group",
				prevngs_p->notelist[k].letter,
				standard_accstr(
					prevngs_p->notelist[k].acclist,
					prevngs_p->inputfile,
					prevngs_p->inputlineno),
				prevngs_p->notelist[k].octave);

				prevngs_p->notelist[k].nslurto = 0;
			/* there is one note, so we can do the slide */
			} else {
				s_p->letter = ngs_p->notelist[0].letter;
				s_p->octave = ngs_p->notelist[0].octave;
			}
		}
	}
}

/*
 * Name:        hasprebend()
 *
 * Abstract:    Check whether any of the notes in this group have a prebend.
 *
 * Returns:     YES or NO
 *
 * Description: This function checks whether any of the notes in this group
 *		have a prebend.  That's the case where a NOTE structure has
 *		both a fret number and a bend other than "".  A bend of "" is
 *		the release of a bend.  It will also return NO if this group
 * 		is a rest or space.
 */

static int
hasprebend(tgs_p)

struct GRPSYL *tgs_p;

{
	int n;			/* for looping through notes */


	for (n = 0; n < tgs_p->nnotes; n++) {
		/* check for a fret with a nonnull bend */
		if (tgs_p->notelist[n].FRETNO != NOFRET &&
		    HASREALBEND(tgs_p->notelist[n])) {
			return (YES);
		}
	}

	return (NO);
}

/*
 * Name:        cancombine()
 *
 * Abstract:    Check two groups can be joined into one on the tabnote staff.
 *
 * Returns:     YES or NO
 *
 * Description: This function checks whether the two given groups are joined
 *		by 1/4 step or less bends and should be combined into one group
 *		on the tabnote staff.  There are a number of other conditions
 *		that must be met for this to be allowed.  If the answer is YES,
 *		it sets *comb_p to the total duration, ignoring the effect of
 *		tuplets.
 */

static int
cancombine(mll_p, tgs_p, comb_p)

struct MAINLL *mll_p;	/* main linked list structure we are hanging off of */
struct GRPSYL *tgs_p;	/* tablature GRPSYL for the first of the two groups */
RATIONAL *comb_p;	/* for returning total time if combinable */

{
	struct GRPSYL *nexttgs_p;	/* the second GRPSYL */
	struct GRPSYL *next2tgs_p;	/* the next one after that */
	int n;				/* for looping through note lists */


	/* must be a note group */
	if (tgs_p->grpcont != GC_NOTES)
		return (NO);

	/* must not be grace */
	if (tgs_p->grpvalue == GV_ZERO)
		return (NO);

	for (n = 0; n < tgs_p->nnotes; n++) {
		/* no note can be the destination of a nonnull bend */
		if (HASREALBEND(tgs_p->notelist[n]))
			return (NO);
	}

	nexttgs_p = tgs_p->next;	/* find the next group */

	/* first group must not be at the end of a measure */
	if (nexttgs_p == 0)
		return (NO);

	/* first and second group must have same number of notes */
	if (tgs_p->nnotes != nexttgs_p->nnotes)
		return (NO);

	/* each pair of notes must be joined by a bend of <= 1/4 step */
	for (n = 0; n < nexttgs_p->nnotes; n++) {
		if ( ! HASBEND(nexttgs_p->notelist[n]) ||
		   GT(ratbend(&nexttgs_p->notelist[n]), One_fourth))
			return (NO);
	}

	/* find the following (3rd) group, if there is one */
	next2tgs_p = nextgrpsyl(nexttgs_p, &mll_p);

	/* no notes in the second group are allowed to bend into the third */
	if (next2tgs_p != 0) {
		for (n = 0; n < next2tgs_p->nnotes; n++) {
			if (HASBEND(next2tgs_p->notelist[n]))
				return (NO);
		}
	}

	/* first group must not be at the end of a tuplet */
	if (tgs_p->tuploc == ENDITEM || tgs_p->tuploc == LONEITEM)
		return (NO);

	/* second group must not be at the start of a tuplet */
	if (nexttgs_p->tuploc == STARTITEM || nexttgs_p->tuploc == LONEITEM)
		return (NO);

	/* get total duration of the two groups */
	*comb_p = radd(calctime(tgs_p), calctime(nexttgs_p));

	/* total must be double whole, or else numerator must be 2**n - 1 */
	if (NE(*comb_p, Two) && (comb_p->n & (comb_p->n + 1)) != 0)
		return (NO);

	return (YES);
}

/*
 * Name:        calctime()
 *
 * Abstract:    Calculate time duration, considering basictime and dots.
 *
 * Returns:     The rational number answer.
 *
 * Description: This function, given a GRPSYL structure, returns the duration
 *		as a rational number.  It considers basictime and dots, but it
 *		does not include the effect of tuplets.  It assumes nongrace,
 *		it assumes GC_NOTES (thus no quadruple wholes), and it assumes
 *		not multirest.
 */

static RATIONAL
calctime(gs_p)

struct GRPSYL *gs_p;

{
	RATIONAL base;


	if (gs_p->basictime == 0) {
		/* double whole note is 2 */
		base.n = 2;
		base.d = 1;
	} else {
		/* anything else is 1/basictime */
		base.n = 1;
		base.d = gs_p->basictime;
	}

	/* return  ( base * (2 - (1/2)**dots) )   */
	return (rmul(base, rsub(Two, rrai(One_half, gs_p->dots))));
}

/*
 * Name:        translate_group()
 *
 * Abstract:    Translate tablature group notes to tabnote group notes.
 *
 * Returns:     void
 *
 * Description: This function is given a tablature staff group.  It normally
 *		translates the notes in this one group to the notes in the
 *		corresponding tabnote staff group.  But in the case where the
 *		tab staff group has note(s) that are prebends, it also creates
 *		the notes for the tabnote grace group.  And in the case where
 *		this tab group is to be combined with the following one, it
 *		translates the two together into one tabnote group.
 */

static void
translate_group(tgs_p, pregs_p, ngs_p, mll_p, combine)

struct GRPSYL *tgs_p;		/* tablature GRPSYL */
struct GRPSYL *ngs_p;		/* corresponding main tabnote GRPSYL */
struct GRPSYL *pregs_p;		/* GRPSYL for prebend's grace group */
struct MAINLL *mll_p;		/* main LL struct we come from */
int combine;			/* combining two tab groups into one tabnote?*/

{
	struct TABNOTENOTE notes[MAXTABLINES];	  /* notes in the main group */
	struct TABNOTENOTE prenotes[MAXTABLINES]; /* notes in prebend group */
	struct TABNOTENOTE tempnote;	/* temporary storage for sorting */
	struct STRINGINFO *strinfo;	/* info about the strings */
	int fret;		/* fret number */
	int idx, pidx;		/* indices into regular and prebend arrays */
	int n, k;		/* loop variables */
	int strno;		/* string number */
	RATIONAL bend;		/* bend distance as a rational number */
	int sharps;		/* number of sharps in tabnote staff's keysig*/


	/* find the key signature; flats count negative */
	sharps = svpath(ngs_p->staffno, SHARPS)->sharps;

	/* point to the array of structures describing the strings */
	strinfo = svpath(tgs_p->staffno, STAFFLINES)->strinfo;

	/*
	 * Loop through the note structures in the tab staff's GRPSYL, filling
	 * the prebend and regular note arrays.
	 */
	idx = pidx = 0;
	for (n = 0; n < tgs_p->nnotes; n++) {

		/* get string and fret number */
		strno = tgs_p->notelist[n].STRINGNO;
		fret = tgs_p->notelist[n].FRETNO;

		/*
		 * If this note is a prebend note, put the original (unbent)
		 * note in the prenotes array.  Link this to the tab staff's
		 * GRPSYL.  Keep the array sorted so that the highest notes
		 * come first.
		 */
		if (fret != NOFRET && HASREALBEND(tgs_p->notelist[n])) {
			calcnote(&strinfo[strno], fret, Zero, sharps,
					&prenotes[pidx++], tgs_p);
			prenotes[pidx-1].strno = strno;
			prenotes[pidx-1].note_p = &tgs_p->notelist[n];
			for (k = pidx - 1; k > 0; k--) {
				if (prenotes[k].hs > prenotes[k-1].hs) {
					tempnote = prenotes[k-1];
					prenotes[k-1] = prenotes[k];
					prenotes[k] = tempnote;
				}
			}
		}

		/* find the bend amount; if none we will get Zero */
		bend = ratbend(&tgs_p->notelist[n]);

		/* if none, copy if an earlier note tied to us has a bend */
		if (EQ(bend, Zero)) {
			bend = findbend(tgs_p, mll_p, strno);
		}

		/*
		 * If there is no fret number in this note, it must be the
		 * destination of a bend.  Call findfret to search backwards
		 * through earlier notes on this string, until we find the fret
		 * number, which is where the bend started.
		 */
		if (fret == NOFRET)
			fret = findfret(tgs_p, mll_p, strno);
		if (fret == NOFRET)
			pfatal("cannot find fret number for tablature note");

		/*
		 * Now we have the fret, and maybe a bend too.  Calculate what
		 * note this comes out to, and put it in the notes array.
		 * Link this to the GRPSYL it was derived from.  Keep the
		 * array sorted so that the highest notes come first.
		 */
		calcnote(&strinfo[strno], fret, bend, sharps, &notes[idx++],
				tgs_p);
		notes[idx-1].note_p = &tgs_p->notelist[n];
		notes[idx-1].strno = strno;
		for (k = idx - 1; k > 0; k--) {
			if (notes[k].hs > notes[k-1].hs) {
				tempnote = notes[k-1];
				notes[k-1] = notes[k];
				notes[k] = tempnote;
			}
		}
	}

	/*
	 * If we are generating a prebend group, populate NOTE structures for
	 * it.  Then, in any case, populate NOTE structures for the main group.
	 */
	if (pregs_p != 0)
		popnotes(pregs_p, prenotes, pidx, tgs_p, mll_p, YES, NO,sharps);

	popnotes(ngs_p, notes, idx, tgs_p, mll_p, NO, combine, sharps);
}

/*
 * Name:        calcnote()
 *
 * Abstract:    Calculate note info for a tabnote NOTE structure.
 *
 * Returns:     void
 *
 * Description: This function is given the info about the string in question,
 *		the fret number on that string, and the amount of bend (which
 *		might be zero).  From this it calculates what note that results
 *		in, and how best to represent it, which depends on what the
 *		key sig is.  Bends are rounded to the nearest half step,
 *		rounding downward when they fall on an exact quarter step.
 *		The results are put in the TABNOTENOTE structure provided.
 */

static void
calcnote(sinfo_p, fret, bend, sharps, note_p, tgs_p)

struct STRINGINFO *sinfo_p;	/* pointer to info about the string */
int fret;			/* fret number on the string */
RATIONAL bend;			/* bend distance */
int sharps;			/* number of sharps in tabnote staff's keysig*/
struct TABNOTENOTE *note_p;	/* note structure to be filled */
struct GRPSYL *tgs_p;		/* pointer to tab group */

{
	/*
	 * The following table, indexed by a note letter minus 'a', tells how
	 * many half steps that note is above C.
	 */
	static int hstab[] = { 9, 11, 0, 2, 4, 5, 7 };
			    /* a   b  c  d  e  f  g */
	/*
	 * The following table, given the number of sharps in a major key
	 * (flats count negative), is to be indexed by (sharps + 7).  The
	 * result is the number of half steps the key note is above C.
	 */
	static int sh2keyhs[] = { 11,6, 1, 8, 3,10, 5, 0, 7, 2, 9, 4,11, 6, 1 };
			       /* c& g& d& a& e& b& f  c  g  d  a  e  b  f# c#*/
			       /* -7 -6 -5 -4 -3 -2 -1 0  1  2  3  4  5  6  7 */
	/*
	 * The following table, given the number of sharps in a major key
	 * (flats count negative), is to be indexed by (sharps + 7).  The
	 * result is the number of letters the key note is above C.
	 */
	static int sh2keylet[] ={ 0, 4, 1, 5, 2, 6, 3, 0, 4, 1, 5, 2, 6, 3, 0 };
			       /* c& g& d& a& e& b& f  c  g  d  a  e  b  f# c#*/
			       /* -7 -6 -5 -4 -3 -2 -1 0  1  2  3  4  5  6  7 */

	/*
	 * The following table, given the number of half steps a note is above
	 * the key note, tells how many letters above the key note letter the
	 * given note should be written as.  For example, in the key of C, the
	 * note 6 half steps up (augmented 4th, f#), should be written with
	 * the letter 3 letters above c (c + 3 = f), thus f# and not g&.
	 */
	static int hs2s[] = { 0, 0, 1, 2, 2, 3, 3, 4, 4, 5, 6, 6 };
	/* intervals:         P1 A1 M2 m3 M3 P4 A4 P5 A5 M6 m7 M7 */
	/* key of C example:  c  c# d  e& e  f  f# g  g# a  b& b  */


	int hs;			/* half steps above c0 */
	RATIONAL bendqs;	/* bend quarter steps */
	int keyhs;		/* hs above C the key note is */
	int intervhs;		/* hs above key note the given note is */
	int intervlet;		/* letters above key note the given note is */
	int keyletidx;		/* the key letter (c=1, d=1, etc.) */
	int octacc;		/* half steps due to octave & accidental */
	int accoffset;		/* resulting note's acc (&=-1, #=1, etc.) */


	/*
	 * Convert the bend to quarter steps.  If the result is an odd integer,
	 * dividing by 2 will give the number of half steps we want (we are
	 * rounding downward).  Otherwise, we round to the nearest half step.
	 */
	bendqs = rmul(bend, Four);
	if (bendqs.n % 2 == 1 && bendqs.d == 1) {
		hs = bendqs.n / 2;
	} else {
		bendqs = radd(bendqs, One);
		hs = bendqs.n / bendqs.d / 2;
	}

	/* add on the half steps due to letter, octave, and fret */
	hs += hstab[ sinfo_p->letter - 'a' ] + 12 * sinfo_p->octave + fret;

	/* adjust if string has an accidental; only '#' and '&' are allowed */
	switch (sinfo_p->accidental) {
	case '#':
		hs++;		/* sharp */
		break;
	case '&':
		hs--;		/* flat */
		break;
	}

	/* hs is now the correct note, in half steps above c0 */
	note_p->hs = hs;

	/*
	 * Now that we know the note, we still have to decide how to represent
	 * it, like G# versus A&.  If the note falls within the key signature,
	 * we go with that.  Otherwise, we take our best shot at which way to
	 * do it.
	 *
	 * For C major, for example, it seems clear that F# and C# are more
	 * likely to be appropriate than G& and D&, due to borrowed dominant
	 * chords.  G# is probably better than A&, since it's in the dominant
	 * of the relative minor, though it's not good for the flat 6 chord.
	 * But D# is getting a little extreme, and E& is probably better.
	 * Certainly B& is better than A#.
	 *
	 * For A minor, a similar analysis shows that the same note choices
	 * are good, except that D# is probably better than E&.  We could use
	 * the is_minor flag from the key sig to make this difference.  But
	 * since it's only one note, which is questionable anyway, and since
	 * people wouldn't normally bother to set that flag except for some
	 * unknown MIDI purpose, we elect not to differentiate, but rather
	 * simply go with the "major" analysis.  Of course, we have to
	 * transpose this to the appropriate major key.
	 */

	/*
	 * Find how many half steps above a C the key note is, assuming major.
	 * Then find how many half steps higher the given note is, being
	 * careful to add 12 in there because hs could be as small as -1.
	 */
	keyhs = sh2keyhs[sharps + 7];
	intervhs = (hs + 12 - keyhs) % 12;

	/* find how many letters above the key letter this note should be */
	intervlet = hs2s[intervhs];

	/* find the key letter (c = 0, d = 1, etc.) */
	keyletidx = sh2keylet[sharps + 7];

	/* find the letter that should be used for this note */
	note_p->letter = "cdefgab"[(keyletidx + intervlet) % 7];

	/*
	 * Subtract out the half steps due to letter (half steps the letter is
	 * above C).  This leaves the half steps due to the octave & accidental.
	 */
	octacc = hs - hstab[ note_p->letter - 'a' ];

	/*
	 * The nearest multiple of 12 gives the octave.  What's left gives the
	 * accidental from -2 to 2 (double flat to double sharp).  Index a
	 * character array by that plus 2 to get the right character.
	 */
	note_p->octave = (octacc + 12 + 6) / 12 - 1;
	accoffset = octacc - 12 * note_p->octave;
	note_p->accidental = Acclets[accoffset + 2];

	if (note_p->octave < MINOCTAVE || note_p->octave > MAXOCTAVE) {
		l_ufatal(tgs_p->inputfile, tgs_p->inputlineno,
		"the indicated note on the %s string is out of range, too %s",
		format_string_name(sinfo_p->letter, sinfo_p->accidental,
		sinfo_p->nticks), note_p->octave > MAXOCTAVE ? "high" : "low");
	}
}

/*
 * Name:        findbend()
 *
 * Abstract:    Find the bend that should apply to the given GRPSYL.
 *
 * Returns:     The bend, or Zero if none found.
 *
 * Description: This function starts at the given GRPSYL and works backwards
 *		through that voice, stopping when there is no tie joining from
 *		there forwards to our group, or when it finds a group with a
 *		bend.  This function is needed because when a note with a bend
 *		has a tie, the bend info should be carried forward, analogous
 *		to the way accidentals are carried forward on normal staffs.
 */

static RATIONAL
findbend(tgs_p, mll_p, strno)

struct GRPSYL *tgs_p;		/* tab GRPSYL to start from */
struct MAINLL *mll_p;		/* MLL struct it hangs off of */
int strno;			/* string number */

{
	int n;			/* for looping through notelist */


	/* loop back through notes tied to us until finding a bend, if any */
	for (;;) {
		/* search backwards to the prev GRPSYL */
		tgs_p = prevgrpsyl(tgs_p, &mll_p);
		if (tgs_p == 0) {
			return (Zero);
		}

		/* to have a bend, the GRPSYL must contain notes */
		if (tgs_p->grpcont != GC_NOTES) {
			return (Zero);
		}

		/* find the note for the string in question, if it exists */
		for (n = 0; n < tgs_p->nnotes; n++) {
			if (tgs_p->notelist[n].STRINGNO == strno)
				break;
		}
		if (n == tgs_p->nnotes) {
			return (Zero);
		}

		/* if not tied, it has no effect on us */
		if (tgs_p->notelist[n].tie == NO) {
			return (Zero);
		}

		/* if this note (string) has a bend, return it */
		if (HASREALBEND(tgs_p->notelist[n])) {
			return (ratbend(&tgs_p->notelist[n]));
		}
	}
}

/*
 * Name:        findfret()
 *
 * Abstract:    Find fret number that applies to the given GRPSYL.
 *
 * Returns:     The fret number, or NOFRET if none found.
 *
 * Description: This function starts at the given GRPSYL and works backwards
 *		through that voice until it find a GRPSYL containing a fret
 *		number.  This is needed since when there is a bend (other than
 *		a prebend), the fret number does not exist in this GRPSYL, but
 *		in some earlier GRPSYL which is "bent" to this GRPSYL by a
 *		series of one or more bends.  Every intervening GRPSYL must
 *		have a "note" for this string.  If the function hits an
 *		invalid GRPSYL or the beginning of the MLL, it returns NOFRET.
 *		This should never happen.
 */

static int
findfret(tgs_p, mll_p, strno)

struct GRPSYL *tgs_p;		/* tab GRPSYL to start from */
struct MAINLL *mll_p;		/* MLL struct it hangs off of */
int strno;			/* string number */

{
	int n;			/* for looping through notelist */


	/* loop until we find the fret or something goes wrong */
	for (;;) {
		/* the GRPSYL must contain notes */
		if (tgs_p->grpcont != GC_NOTES)
			return (NOFRET);

		/* find the note for the string in question; it must exist */
		for (n = 0; n < tgs_p->nnotes; n++) {
			if (tgs_p->notelist[n].STRINGNO == strno)
				break;
		}
		if (n == tgs_p->nnotes)
			return (NOFRET);

		/* if this note (string) has a valid fret, return it */
		if (tgs_p->notelist[n].FRETNO != NOFRET)
			return (tgs_p->notelist[n].FRETNO);

		/* search backwards for the next GRPSYL, which must exist */
		tgs_p = prevgrpsyl(tgs_p, &mll_p);
		if (tgs_p == 0)
			return (NOFRET);
	}
}

/*
 * Name:        popnotes()
 *
 * Abstract:    Adjust the desired notes and populate the NOTE structures.
 *
 * Returns:     void
 *
 * Description: This function is given the notes desired for one tabnote
 *		GRPSYL.  If too many notes are too close together to print, it
 *		throws some away, with warnings.  Then it allocates NOTE
 *		structures and fills them in.
 */

static void
popnotes(ngs_p, tnn, ntnn, tgs_p, mll_p, is_prebend, combine, sharps)

struct GRPSYL *ngs_p;		/* tabnote GRPSYL */
struct TABNOTENOTE tnn[];	/* array of TABNOTENOTE structures */
int ntnn;			/* number of the above structures */
struct GRPSYL *tgs_p;		/* tablature GRPSYL we came from */
struct MAINLL *mll_p;		/* main LL structure we came from */
int is_prebend;			/* is this a prebend group? */
int combine;			/* this group formed by combining tab grps? */
int sharps;			/* current key signature */

{
	struct NOTE *newnote_p;	/* handy pointer to an allocated NOTE */
	struct GRPSYL *nexttgs_p; /* the tab GRPSYL following ours */
	struct GRPSYL *prevngs_p; /* the tabnote GRPSYL preceding ours */
	struct SLURTO *s_p;	/* point at a SLURTO structure */
	struct MAINLL *mainll_p;/* place to store mll_p */
	int hightight, lowtight;/* are notes next to a conflict neighboring? */
	int remnote;		/* should a note be removed? */
	int keysigidx;		/* index to the note that is in the key sig */
	int nonkeysigidx;	/* index to the note not in the key sig */
	int n, k, j;		/* loop variables */


	/*
	 * Loop through the notes, throwing away duplicates.  They are already
	 * in order, highest note first.
	 */
	for (n = 1; n < ntnn; n++) {
		if (tnn[n - 1].hs == tnn[n].hs) {
			l_warning(tgs_p->inputfile, tgs_p->inputlineno,
                        "throwing away duplicate note %c%c%d on tabnote staff",
			tnn[n].letter, tnn[n].accidental, tnn[n].octave);

			for (k = n; k < ntnn; k++)
				tnn[k - 1] = tnn[k];
			ntnn--;
			n--;
		}
	}

	/*
	 * Loop through the remaining notes, looking for conflicts, such as f
	 * versus f#.  Because of the way the notes were generated, and because
	 * of the previous loop, at this point any remaining conflicts will
	 * consist of two notes, one fitting the key signature and one not.
	 * Try to rewrite enharmonically the one that is not in the key sig.
	 * The only problem that can arise is if there is a neighoring note in
	 * the way.  Like, with f and f# in the key of 0 sharps, we'd like to
	 * change f# to g&, but we can't if there's already a g.  So if this
	 * happens, try to rewrite the other note (in this case, we'd get e#
	 * and f#).  If there's already an e, give up and throw one note away.
	 * Yes, we could rewrite g as a&& to make room, but this is so rare
	 * that it's not worth the effort.
	 */
	for (n = 1; n < ntnn; n++) {
		if (tnn[n - 1].letter == tnn[n].letter &&
		    tnn[n - 1].octave == tnn[n].octave) {
			/*
			 * We have a conflict.  See whether the notes next to
			 * the conflicting notes (if any) are on neighboring
			 * steps.
			 */
			if (n > 1 && neighbor(&tnn[n - 2], &tnn[n - 1]) == YES)
				hightight = YES;
			else
				hightight = NO;

			if (n < ntnn-1 && neighbor(&tnn[n], &tnn[n + 1]) == YES)
				lowtight = YES;
			else
				lowtight = NO;

			/* remember which of the notes is in the key sig */
			if (inkeysig(&tnn[n], sharps) == YES) {
				keysigidx = n;
				nonkeysigidx = n - 1;
			} else {
				keysigidx = n - 1;
				nonkeysigidx = n;
			}

			remnote = NO;	/* init to not remove any note */

			if (hightight == YES && lowtight == YES) {
				/* no room to move either note */
				remnote = YES;
			} else if (hightight == YES) {
				/* no room to move high note; move low note */
				if (downletter(&tnn[n]) == NO) {
					/* can't move it */
					remnote = YES;
				}
			} else if (lowtight == YES) {
				/* no room to move low note; move high note */
				if (upletter(&tnn[n - 1]) == NO) {
					/* can't move it */
					remnote = YES;
				}
			} else {
				/* room on both sides */
				if (keysigidx == n) {
					/* try moving nonkeysig note first */
					if (upletter(&tnn[n - 1]) == NO &&
						downletter(&tnn[n]) == NO) {
							/* can't move either */
							remnote = YES;
					}
				} else {
					/* try moving nonkeysig note first */
					if (downletter(&tnn[n]) == NO &&
						upletter(&tnn[n - 1]) == NO) {
							/* can't move either */
							remnote = YES;
					}
				}
			}

			/*
			 * We have to remove one of the two notes in this
			 * conflict.  Arbitrarily, remove the one that doesn't
			 * fit the key signature.
			 */
			if (remnote == YES) {
				l_warning(tgs_p->inputfile, tgs_p->inputlineno,
				"throwing away note %c%c%d on tabnote staff due to conflict with %c%c%d",
						tnn[nonkeysigidx].letter,
						tnn[nonkeysigidx].accidental,
						tnn[nonkeysigidx].octave,
						tnn[keysigidx].letter,
						tnn[keysigidx].accidental,
						tnn[keysigidx].octave);

				for (k = nonkeysigidx + 1; k < ntnn; k++)
					tnn[k - 1] = tnn[k];
				ntnn--;
				n--;
			}
		}
	}

	/* find the tab GRPSYL following our tab GRPSYL, if there is one */
	mainll_p = mll_p;	/* save mll_p */
	nexttgs_p = nextgrpsyl(tgs_p, &mll_p);

	/* find the tabnote GRPSYL preceding ours, if there is one */
	mll_p = mainll_p;	/* restore mll_p in case it was changed */
	prevngs_p = prevgrpsyl(ngs_p, &mll_p);

	CALLOC(NOTE, ngs_p->notelist, ntnn);
	ngs_p->nnotes = (short)ntnn;

	/*
	 * Loop through the newly allocated notes, setting all their fields.
	 */
	for (n = 0; n < ntnn; n++) {
		newnote_p = &ngs_p->notelist[n];   /* set short cut pointer */

		/* allocate the array of coordinates */
		MALLOCA(float, ngs_p->notelist[n].c, NUMCTYPE);

		/* copy the calculated letter/accidental/octave */
		newnote_p->letter = tnn[n].letter;
		standard_to_acclist(tnn[n].accidental, newnote_p->acclist);
		newnote_p->octave = tnn[n].octave;

		/* copy note size, except that prebend graces are always small*/
		newnote_p->notesize = is_prebend == YES ?
				GS_SMALL : tnn[n].note_p->notesize;

		newnote_p->headshape = tnn[n].note_p->headshape;
		newnote_p->tie = tnn[n].note_p->tie;
		newnote_p->tiestyle = tnn[n].note_p->tiestyle;
		newnote_p->tiedir = tnn[n].note_p->tiedir;

		/* flag to draw curved line after notes */
		if (combine)
			newnote_p->smallbend = YES;

		/* copy whether note has paren; but if prebend, always set it*/
		newnote_p->note_has_paren = tnn[n].note_p->note_has_paren;
		if (is_prebend)
			newnote_p->note_has_paren = YES;

		/*
		 * If this note has slur(s), allocate a slur-to list.  If a
		 * slur is to real frets, set the string number in the letter
		 * for now, and fix it up later when we know the true letter
		 * and octave.  If it's a slur to or from nowhere, copy over
		 * the "octave" (pseudo fret) as is.  The letter on these is
		 * meaningless.  Note:  a maximum of one slur can be to a real
		 * fret and a maximum of two slurs can exist.
		 */
		if (tnn[n].note_p->nslurto != 0) {
			newnote_p->nslurto = tnn[n].note_p->nslurto;
			CALLOC(SLURTO, newnote_p->slurtolist,
					newnote_p->nslurto);

			for (k = 0; k < newnote_p->nslurto; k++) {
				s_p = &tnn[n].note_p->slurtolist[k];

				if (IS_NOWHERE(s_p->octave)) {
					/* slur to or from nowhere */
					/* letter is meaningless */
					newnote_p->slurtolist[k].octave =
							s_p->octave;
				} else {	/* real fret number */
					newnote_p->slurtolist[k].letter =
							tnn[n].strno;
					newnote_p->slurtolist[k].octave =
							NOFRET;	/* unknown */
				}

				/* always copy slur style */
				newnote_p->slurtolist[k].slurstyle =
					s_p->slurstyle;
			}
		}

		/*
		 * If we are a prebend note, allocate a slur-to list.
		 */
		if (is_prebend) {
			CALLOC(SLURTO, newnote_p->slurtolist, 1);
			newnote_p->is_bend = YES;
			newnote_p->nslurto = 1;
			/*
			 * Although the bent-to note has been calculated, at
			 * this point in the code we don't know what it is.
			 * For now, we store the string number in the letter.
			 * When we are doing the next GRPSYL on this staff, we
			 * will change this to be the true bent-to letter, and
			 * set the octave too.
			 */
			newnote_p->slurtolist[0].letter = tnn[n].strno;
			newnote_p->slurtolist[0].octave = NOFRET;
		}

		/*
		 * If we are not a prebend group (those are handled elsewhere)
		 * and our group was not formed by combining two tab groups
		 * and there is a next GRPSYL on the tablature staff (in this
		 * or the next measure), our note might be bent to a note in
		 * that next GRPSYL.  So search it to see if it contains a note
		 * that our current note is bent to.  If so, allocate a slur-to
		 * list.
		 */
		if ( ! is_prebend && ! combine && nexttgs_p != 0) {
			for (k = 0; k < nexttgs_p->nnotes; k++) {
				/*
				 * Find matching string with a bend but either
				 * no fret or else it has to be a null bend.
				 * This eliminates finding a prebend.
				 */
				if (nexttgs_p->notelist[k].STRINGNO == tnn[n].
				    strno && HASBEND(nexttgs_p->notelist[k]) &&
				    (nexttgs_p->notelist[k].FRETNO == NOFRET ||
				    HASNULLBEND(nexttgs_p->notelist[k])))
					break;
			}
			if (k < nexttgs_p->nnotes) {
				if (newnote_p->nslurto == 0) {
					/* no slurs, one item list for bend */
					CALLOC(SLURTO, newnote_p->slurtolist,1);
					newnote_p->nslurto = 1;
				} else {	/* we have slur(s) already */
					/* increase list to hold bend also */
					newnote_p->nslurto++;
					REALLOC(SLURTO, newnote_p->slurtolist,
							newnote_p->nslurto);
				}
				/* bend will be the last item in the list */
				newnote_p->is_bend = YES;
				/*
				 * We don't yet know what the bent-to note is
				 * going to be.  For now, we store the string
				 * number in the letter.  When we are doing the
				 * next GRPSYL on this staff, we will change
				 * this to be the true bent-to letter, and set
				 * the octave too.
				 */
				newnote_p->slurtolist[newnote_p->nslurto - 1].
						letter = tnn[n].strno;
			}
		}

		/*
		 * If we are not a prebend note and there is a previous GRPSYL
		 * on the tabnote staff (in this or the previous measure), our
		 * note may be the destination of a bend or a slur.  Try to
		 * find the note in the previous GRPSYL that bends or slurs to
		 * our note, if any.  If found, set its slur-to list correctly,
		 * now that we know what should be put in it.
		 */
		if ( ! is_prebend && prevngs_p != 0) {
			/*
			 * Check every note in the preceding group.  Don't
			 * break out after finding one, since multiple ones
			 * could slide to our note if our note was a duplicate
			 * (derived from multiple strings).  But there is a
			 * max of one slur/bend coming from each note.
			 */
			for (k = 0; k < prevngs_p->nnotes; k++) {
				/* check every slur from that note */
				s_p = 0;	/* avoid 'used before set' */
				for (j = 0; j < prevngs_p->notelist[k].nslurto;
						j++) {
					s_p = &prevngs_p->notelist[k].
							slurtolist[j];
					if (s_p->letter == tnn[n].strno &&
					    ! IS_NOWHERE(s_p->octave))
						break;
				}
				/* if found a slur to our note */
				if (j < prevngs_p->notelist[k].nslurto) {
					s_p->letter = ngs_p->notelist[n].letter;
					s_p->octave = ngs_p->notelist[n].octave;
				}
			}
		}
	}
}

/*
 * Name:        neighbor()
 *
 * Abstract:    Find whether the given notes are on neighboring letters.
 *
 * Returns:     YES or NO
 *
 * Description: This function figures out whether the given notes are on
 *		neighboring letter, taking octaves into account.
 */

static int
neighbor(high_p, low_p)

struct TABNOTENOTE *high_p;	/* the higher note */
struct TABNOTENOTE *low_p;	/* the lower note */

{
	/* if the letter themselves aren't right, return NO */
	if (low_p->letter == 'g') {
		if (high_p->letter != 'a')
			return (NO);
	} else {
		if (high_p->letter != low_p->letter + 1)
			return (NO);
	}

	/* if the octaves aren't right, return NO */
	if (low_p->letter == 'b') {
		if (high_p->octave != low_p->octave + 1)
			return (NO);
	} else {
		if (high_p->octave != low_p->octave)
			return (NO);
	}

	return (YES);
}

/*
 * Name:        inkeysig()
 *
 * Abstract:    Find whether the given note fits the key signature.
 *
 * Returns:     YES or NO
 *
 * Description: This function figures out whether the given note fits the key
 *		signature, and returns the answer.
 */

static int
inkeysig(note_p, sharps)

struct TABNOTENOTE *note_p;	/* note contained in a TABNOTENOTE structure */
int sharps;			/* current key signature */

{
	int circnum;		/* position in the circle of 5ths */
	int accnum;		/* B = 0, & = 1, n = 2, # = 3, x = 4 */
	int sharpness;		/* how "sharp" is the note */


	circnum = strchr(Circle, note_p->letter) - Circle;
	accnum = strchr(Acclets, note_p->accidental) - Acclets;
	sharpness = circnum + 7 * (accnum - 2);

	if (sharpness >= sharps && sharpness <= sharps + 6)
		return (YES);
	else
		return (NO);
}

/*
 * Name:        upletter()
 *
 * Abstract:    Alter note enharmonically, incrementing the letter.
 *
 * Returns:     void
 *
 * Description: This function alters the given note enharmonically.  It raises
 *		the letter, and adjusts the accidental and octave as needed.
 */

static int
upletter(note_p)

struct TABNOTENOTE *note_p;	/* note contained in a TABNOTENOTE structure */

{
	int accnum;		/* B = 0, & = 1, n = 2, # = 3, x = 4 */


	accnum = strchr(Acclets, note_p->accidental) - Acclets;

	/*
	 * If the old letter is e or b, the next higher white note is only a
	 * half step away, so the accidental has to be a half step flatter.
	 * Otherwise, it's a whole step.  If the result would be flatter than
	 * a double flat, we can't do this, so return NO.
	 */
	if (note_p->letter == 'e' || note_p->letter == 'b') {
		if (accnum == 0)	/* double flat */
			return (NO);
		accnum--;
	} else {
		if (accnum <= 1)	/* flat or double flat */
			return (NO);
		accnum -= 2;
	}

	/* if the note is b, increment the octave; fail if impossible */
	if (note_p->letter == 'b') {
		if (note_p->octave == MAXOCTAVE)
			return (NO);
		note_p->octave++;
	}

	/* increment the letter; g wraps around to a */
	if (note_p->letter == 'g')
		note_p->letter = 'a';
	else
		note_p->letter++;

	note_p->accidental = Acclets[accnum];

	return (YES);
}

/*
 * Name:        downletter()
 *
 * Abstract:    Alter note enharmonically, decrementing the letter.
 *
 * Returns:     void
 *
 * Description: This function alters the given note enharmonically.  It lowers
 *		the letter, and adjusts the accidental and octave as needed.
 */

static int
downletter(note_p)

struct TABNOTENOTE *note_p;	/* note contained in a TABNOTENOTE structure */

{
	int accnum;		/* B = 0, & = 1, n = 2, # = 3, x = 4 */


	accnum = strchr(Acclets, note_p->accidental) - Acclets;

	/*
	 * If the old letter is f or c, the next lower white note is only a
	 * half step away, so the accidental has to be a half step sharper.
	 * Otherwise, it's a whole step.  If the result would be sharper than
	 * a double sharp, we can't do this, so return NO.
	 */
	if (note_p->letter == 'f' || note_p->letter == 'c') {
		if (accnum == 4)	/* double sharp */
			return (NO);
		accnum++;
	} else {
		if (accnum >= 3)	/* sharp or double sharp */
			return (NO);
		accnum += 2;
	}

	/* if the note is c, decrement the octave; fail if impossible */
	if (note_p->letter == 'c') {
		if (note_p->octave == MINOCTAVE)
			return (NO);
		note_p->octave--;
	}

	/* decrement the letter; a wraps around to g */
	if (note_p->letter == 'a')
		note_p->letter = 'g';
	else
		note_p->letter--;

	note_p->accidental = Acclets[accnum];

	return (YES);
}

/*
 * Name:        cleanaccs()
 *
 * Abstract:    Remove unnecessary accidentals from a tabnote staff measure.
 *
 * Returns:     void
 *
 * Description: This function removes all the unnecessary accidentals from one
 *		measure of a tabnote staff.  It takes into consideration the
 *		key signature and the fact that accidentals last for the
 *		duration of the measure unless changed by another accidental.
 *		Also, if a note in the first group was tied from the previous
 *		measure, any accidental is carried over to this note and
 *		any series of notes it ties into, but if the note occurs again
 *		in this measure it reverts to the key signature; so the bottom
 *		line is we can ignore this and pretend that the tied note is in
 *		the key signature.
 */

static void
cleanaccs(gs_p, mll_p)

struct GRPSYL *gs_p;		/* starts at first tabnote GRPSYL */
struct MAINLL *mll_p;		/* main LL struct that tabnote staff hangs off*/

{
	/* current accidental for letter and octave */
	char curacc[7][MAXOCTAVE + 1];	/* assumes MINOCTAVE == 0 */

	int oct;		/* octave number */
	char let;		/* note letter */
	int sharps;		/* number of sharps in tabnote staff's keysig*/
	int cidx;		/* index into circle of fifths */
	struct GRPSYL *prevgs_p;/* pointer to preceding GRPSYL */
	struct NOTE *note_p;	/* pointer to a NOTE structure */
	int n, k;		/* loop variables */
	char std_acc;		/* accidental as x # n & B */



	/*
	 * Initialize the table to say that for all octaves of all letters,
	 * the accidental is a natural.
	 */
	for (oct = 0; oct <= MAXOCTAVE; oct++) {
		for (let = 'a'; let <= 'g'; let++) {
			curacc[let - 'a'] [oct] = 'n';
		}
	}

	/* find the key signature; flats count negative */
	sharps = svpath(gs_p->staffno, SHARPS)->sharps;

	/*
	 * Load the key signature's accidentals into the array for every
	 * octave.
	 */
	for (oct = 0; oct <= 9; oct++) {
		for (cidx = 0; cidx < sharps; cidx++) {
			curacc[ Circle[cidx] - 'a' ] [oct] = '#';
		}
		for (cidx = 0; cidx > sharps; cidx--) {
			curacc[ Circle[6 + cidx] - 'a' ] [oct] = '&';
		}
	}

	/*
	 * Loop through every note group and every note in them, clearing
	 * accidentals when that accidental is already in force, and updating
	 * the table when not.
	 */
	for ( ; gs_p != 0; gs_p = gs_p->next) {
		if (gs_p->grpcont != GC_NOTES)
			continue;

		/* find previous group; could be in previous measure if any */
		prevgs_p = prevgrpsyl(gs_p, &mll_p);

		for (n = 0; n < gs_p->nnotes; n++) {
			note_p = &gs_p->notelist[n];

			/*
			 * If the note is the destination of a tie, blow away
			 * its accidental.  This wouldn't have to be a special
			 * check if it weren't for ties across bar lines.
			 */
			if (prevgs_p != 0 && prevgs_p->grpcont == GC_NOTES) {
				for (k = 0; k < prevgs_p->nnotes; k++) {
					if (prevgs_p->notelist[k].tie == YES &&
					    prevgs_p->notelist[k].letter ==
					    note_p->letter && prevgs_p->notelist
					    [k].octave == note_p->octave) {
						CLEAR_ACCS(note_p->acclist);
						break;
					}
				}
				/* if we found it was tied, continue to next */
				if (k < prevgs_p->nnotes)
					continue;
			}
			/*
			 * If this note's accidental agrees with the accidental
			 * already in force for this letter and octave, wipe it
			 * out.  If it doesn't, leave it alone, and update the
			 * table to show the new accidental that is in force.
			 */
			std_acc = standard_acc(note_p->acclist);
			if (std_acc == curacc[ note_p->letter - 'a' ]
					 [ note_p->octave ]) {
				CLEAR_ACCS(note_p->acclist);
			} else {
				curacc[ note_p->letter - 'a' ] [ note_p->
						octave ] = std_acc;
			}
		}
	}
}
/*
 * Name:        standard_accstr()
 *
 * Abstract:    Convert an acclist to a standard accidental string, if possible.
 *
 * Returns:     std acc as string "#", "&&", etc., "" if none, ufatal if nonstd
 *
 * Description: This function is given an acclist.  If that list contains one
 *		and only one acc and it is one of the standard set of five, it
 *		returns the printable string representing it as in Mup input.
 *		If there is no acc, it returns "".  If nonstandard acc or
 *		multiple accs, ufatal.  It considers all accs if MIDI,
 *		otherwise only visible accs.
 */

static char *
standard_accstr(acclist, file, line)

char *acclist;			/* accidental list */
char *file;			/* input file */
int line;			/* line in input file */

{
	int std_acc;


	if ( ! has_accs(acclist)) {
		return ("");
	}

	std_acc = standard_acc(acclist);
	if (std_acc == '\0') {
		l_pfatal(file, line,
			"standard_accstr() encountered custom accidental used with tablature");
	}

	return (Acctostr[strchr(Acclets, std_acc) - Acclets]);
}
