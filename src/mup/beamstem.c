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
 * Name:	beamstem.c
 *
 * Description:	This file contains functions for setting lengths of note
 *		stems, which also involves beaming considerations.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/*
 * Several functions need to know the value of the "stemlen" parameter, so
 * instead of them all calling vvpath, define a holding place here.
 */
static float Defstemsteps;

static void proclist P((struct MAINLL *mainll_p, int vno));
static void proctablist P((struct MAINLL *mainll_p, int vno));
static int stemforced P((struct GRPSYL *gs_p, struct GRPSYL *ogs_p));
static void setbeam P((struct GRPSYL *start_p, struct GRPSYL *end_p,
		struct GRPSYL *ogs_p)); 
static int mensural_flags P((struct GRPSYL *gs_p));
static void restore_ry P((struct GRPSYL *start_p, struct GRPSYL *end_p));
static double desired_vert_stemoffset P((struct GRPSYL *gs_p));
static double embedgrace P((struct GRPSYL *start_p, double b1, double b0));
static double embedclef P((struct GRPSYL *start_p, double b1, double b0));
static double beamoff P((struct GRPSYL *gs_p, int side, double boundary,
		struct GRPSYL *start_p));
static double avoidothervoice P((struct GRPSYL *start_p, struct GRPSYL *last_p,
		double b1, double b0, struct GRPSYL *ogs_p));
static void setgroupvert P((int, struct GRPSYL *, struct GRPSYL *));
static void restbeam P((struct GRPSYL *start_p, struct GRPSYL *last_p));
static void settuplet P((struct GRPSYL *start_p, struct STAFF *staff_p));
static int tupconcave P((float *b1_p, int coord, int num,
		struct GRPSYL *start_p, struct GRPSYL *last_p,
		struct GRPSYL *(*nextfunc_p)()));
static int tupgroups P((struct GRPSYL *start_p, int *numng_p, int *numnrg_p,
		struct GRPSYL **startng_p_p, struct GRPSYL **lastng_p_p,
		struct GRPSYL **startnrg_p_p, struct GRPSYL **lastnrg_p_p,
		struct GRPSYL **last_p_p, struct GRPSYL **end_p));
static void trytuplet P((int forcehorz, float *b0_p, float *b1_p, int coord,
		int num, struct STAFF *staff_p, struct GRPSYL *start_p,
		struct GRPSYL *last_p, struct GRPSYL *brackstart_p,
		struct GRPSYL *bracklast_p, struct GRPSYL *end_p,
		struct GRPSYL *(*nextfunc_p)()));
static double outernote_y P((struct GRPSYL *gs_p, int coord));
static void applywith P((struct GRPSYL *gs_p));
static void revise_restvert P((void));
static void rr_voicemeas P((struct GRPSYL *this_p, struct GRPSYL *other_p,
		struct MAINLL *mll_p));
static void moverestvert P((struct GRPSYL *gs_p, double move, int relative));

/*
 * Name:        beamstem()
 *
 * Abstract:    Set stem lengths for all notes that have stems or slash/alt.
 *
 * Returns:     void
 *
 * Description: This function loops through the main linked list.  For each
 *		linked list of groups on each visible staff, it calls proclist
 *		to set stem lengths.
 */

void
beamstem()

{
	register struct MAINLL *mainll_p; /* point along main linked list */
	int n;				/* loop variable */


	debug(16, "beamstem CSSpass=%d", CSSpass);
	initstructs();			/* clean out old SSV info */

	/*
	 * Loop once for each item in the main linked list.  Apply any SSVs
	 * that are found.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		if (mainll_p->str == S_SSV) {

			asgnssv(mainll_p->u.ssv_p);

		} else if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->visible == YES &&
				! is_mrpt(mainll_p->u.staff_p->groups_p[0])) {
			/*
			 * For this visible staff, call a subroutine to process
			 * each list of groups on it.
			 */
			for (n = 0; n < MAXVOICES; n++) {
				if (mainll_p->u.staff_p->groups_p[n] != 0) {
					/* set global default stem steps */
					Defstemsteps = vvpath(mainll_p->
						u.staff_p->staffno,
						n + 1, STEMLEN)->stemlen;
					if (is_tab_staff(mainll_p->u.staff_p->
							staffno)) {
						proctablist(mainll_p, n);
					} else {
						proclist(mainll_p, n);
					}
				}
			}
		}
	}

	/* last time in here, revise the vertical position of certain rests */
	if (CSSused == NO || CSSpass == YES) {
		revise_restvert();
	}
}

/*
 * Name:        proclist()
 *
 * Abstract:    Process linked list of groups.
 *
 * Returns:     void
 *
 * Description: This function loops through the linked list of groups for one
 *		voice for one measure, first handling the grace groups, then
 *		doing a second loop for the nongrace groups.  For each non-
 *		beamed note that needs it, it sets the stem length.  For each
 *		beamed group, it calls setbeam to figure out the equation
 *		of the beam, and set the stem lengths accordingly.  It also
 *		sets the relative vertical coords of the groups.  These coords
 *		then get altered to include "with" lists and tuplet marks.
 */

static void
proclist(mainll_p, vno)

struct MAINLL *mainll_p;	/* MLL struct for staff we're dealing with */
int vno;			/* voice we're to deal with, 0 to MAXVOICES-1 */

{
	struct GRPSYL *gs_p;	/* point to first group in a linked list */
	struct GRPSYL *ogs_p;	/* point to first group in other linked list */
	struct STAFF *staff_p;	/* point to the staff it's connected to */
	struct GRPSYL *savegs_p;/* save incoming gs_p */
	struct GRPSYL *beamst_p;/* point at first group of a beamed set */
	float notedist;		/* distance between outer notes of a group */
	float extsteps;		/* steps a stem extends beyond the last note */
	float extlen;		/* length a stem extends beyond the last note */
	float temp;		/* (various uses) */
	int flags;		/* number of flags on a note */
	int eqflags;		/* equivalent flags: flags + slashes etc. */


	debug(32, "proclist file=%s line=%d vno=%d", mainll_p->inputfile,
			mainll_p->inputlineno, vno);
	/*
	 * Set pointers to 1st group in our list and in the "other" list, as
	 * appropriate.  Voices 1 and 2 (vno=0,1) refer to each other as the
	 * "other" voice.  (If there is only one voice, ogs_p is set to voice 2
	 * (vno=1) which is a null pointer.)  Voice 3 (vno=2) always ignores
	 * the other voices, so for it, ogs_p is a null pointer.
	 */
	gs_p = mainll_p->u.staff_p->groups_p[ vno ];
	ogs_p = vno == 2 ? (struct GRPSYL *)0 :
			mainll_p->u.staff_p->groups_p[ ! vno ];

	staff_p = mainll_p->u.staff_p;	/* also point at staff */

	/* set globals like Staffscale for use by the rest of the file */
	set_staffscale(staff_p->staffno);

	beamst_p = 0;	/* prevent useless 'used before set' warnings */

	/*
	 * Loop through every group, skipping rests, spaces, and nongrace
	 * groups, setting the stem length of grace groups.
	 */
	for (savegs_p = gs_p; gs_p != 0; gs_p = gs_p->next) {
		if (gs_p->grpcont != GC_NOTES)
			continue;
		if (gs_p->grpvalue == GV_NORMAL)
			continue;

		/*
		 * If we are at the start of a beamed set of groups, remember
		 * this place.  Then, when we find the end of the set, call
		 * setbeam to figure out the equation of the beam and set the
		 * stem lengths.
		 */
		if (gs_p->beamloc != NOITEM) {
			if (gs_p->beamloc == STARTITEM)
				beamst_p = gs_p;
			if (gs_p->beamloc == ENDITEM)
				setbeam(beamst_p, nextsimilar(gs_p), ogs_p);

			continue;
		}

		/* if we get here, this group is not in a beamed set */

		/* if not affected by CSS, do on normal pass, and only then */
		/* if affected by CSS, do on CSS pass, and only then */
		if (css_affects_stemtip(gs_p) != CSSpass) {
			continue;
		}

		/*
		 * If the user specified a nonzero stem length, that's only the
		 * part of it that's not between the notes.  So add the distance
		 * between the outer notes of the group.  However, if they
		 * specified 0, they should get no stem.
		 */
		if (IS_STEMLEN_KNOWN(gs_p->stemlen)) {
			if (gs_p->stemlen != 0.0) {
				gs_p->stemlen *= Staffscale;
				notedist = gs_p->notelist[0].c[RY] - gs_p->
					notelist[ gs_p->nnotes - 1 ].c[RY];
				gs_p->stemlen += notedist;
			}
			continue;
		}

		/*
		 * Grace notes longer than quarter notes default to just a note
		 * head and no stem.  So set their stem length to 0.
		 */
		if (gs_p->basictime <= 4) {
			gs_p->stemlen = 0;
			continue;
		}

		/*
		 * If stemlen parm is zero, force length to zero.  This will
		 * look bad for non-quarter notes, but that's what they
		 * asked for.
		 */
		if (Defstemsteps == 0.0) {
			gs_p->stemlen = 0.0;
			continue;
		}

		/* find number of steps we would like the stem to extend */
		extsteps = stemextsteps(gs_p);

		/*
		 * The grace default stem length has to be scaled a little
		 * differently from the other factors to be applied, so here
		 * we fake it out so that it will come out right later.
		 */
		if (gs_p->grpsize == GS_TINY) {
			extsteps *= TINY_STEMFACTOR / TINY_FACTOR;
		} else {
			extsteps *= SM_STEMFACTOR / SM_FACTOR;
		}

		/* find number of flags */
		if (gs_p->basictime >= 8) {
			flags = drmo(gs_p->basictime) - 2;
		} else {
			flags = 0;
		}

		/*
		 * Depending on how many flags there are, we may have to
		 * force extsteps to be bigger, so it'll look okay.
		 */
		switch (flags) {
		case 0:
			break;
		case 1:
			extsteps = MAX(extsteps, 6.0);
			break;
		default:
			if (flags == 2 && gs_p->slash_alt == 0 &&
					mensural_flags(gs_p)) {
				/* mensural flag 16th note flags do not need to
				 * force extra room, treat like 8th notes */
				extsteps = MAX(extsteps, 6.0);
				break;
			}
			temp = 7.0 + (flags - 2) * FLAGSEP / STEPSIZE;
			extsteps = MAX(extsteps, temp);
			break;
		}

		/* convert to inches, and apply grace scaling */
		if (gs_p->grpsize == GS_TINY) {
			extlen = extsteps * Stepsize * TINY_FACTOR;
		} else {
			extlen = extsteps * Stepsize * SM_FACTOR;
		}

		/* find distance between outer notes of the group */
		notedist = gs_p->notelist[0].c[RY] -
			gs_p->notelist[ gs_p->nnotes - 1 ].c[RY];

		/* total stem length is the sum of these */
		gs_p->stemlen = extlen + notedist;
	}

	/*
	 * Loop through every grace group, skipping rests and spaces,
	 * setting the relative vertical coordinates.
	 */
	setgroupvert(GV_ZERO, savegs_p, ogs_p);

	/*
	 * Loop through every group, skipping rests, spaces and grace groups,
	 * setting the stem length of all nongrace groups.
	 *
	 * WARNING:  The code in this loop is similar to stemroom().
	 * If you change one, you probably will need to change the other.
	 */
	for (gs_p = savegs_p; gs_p != 0; gs_p = gs_p->next) {
		if (gs_p->grpcont != GC_NOTES)
			continue;
		if (gs_p->grpvalue == GV_ZERO)
			continue;
		/*
		 * If this is cross staff beaming, don't do anything now.  We
		 * can't do anything until the absolute vertical coords are set
		 * in absvert.c.
		 */
		if (gs_p->beamto != CS_SAME) {
			continue;
		}

		/*
		 * If we are at the start of a beamed set of groups, remember
		 * this place.  Then, when we find the end of the set, call
		 * setbeam to figure out the equation of the beam and set the
		 * stem lengths.
		 */
		if (gs_p->beamloc != NOITEM) {
			if (gs_p->beamloc == STARTITEM)
				beamst_p = gs_p;
			if (gs_p->beamloc == ENDITEM)
				setbeam(beamst_p, nextsimilar(gs_p), ogs_p);
			continue;
		}

		/* if we get here, this group is not in a beamed set */

		/* if not affected by CSS, do on normal pass, and only then */
		/* if affected by CSS, do on CSS pass, and only then */
		if (css_affects_stemtip(gs_p) != CSSpass) {
			continue;
		}

		/*
		 * Whole notes and double whole notes have no stems, but
		 * they still need to have a pseudo stem length set if
		 * alternation beams are to be drawn between two neighboring
		 * groups, or the group has slashes.
		 */
		if (STEMLESS(gs_p) && gs_p->slash_alt == 0)
			continue;	/* no stem and no pseudo stem */

		/*
		 * If the user specified a nonzero stem length, that's only the
		 * part of it that's not between the notes.  So add the distance
		 * between the outer notes of the group.  But if they specified
		 * 0, leave it as 0.
		 */
		if (IS_STEMLEN_KNOWN(gs_p->stemlen)) {
			if (gs_p->stemlen == 0.0)
				continue;

			gs_p->stemlen *= Staffscale;
			notedist = gs_p->notelist[0].c[RY] -
				gs_p->notelist[ gs_p->nnotes - 1 ].c[RY];
			gs_p->stemlen += notedist;
			continue;
		}

		/* if stemlen parm is zero, force length to zero */
		if (Defstemsteps == 0.0) {
			gs_p->stemlen = 0.0;
			continue;
		}

		/* find number of steps we would like the stem to extend */
		extsteps = stemextsteps(gs_p);

		/*
		 * Mostly, normal and cue are handled the same until later,
		 * when we scale cue by SM_FACTOR.  But the default cue stem
		 * length scales by a somewhat different factor.  So here we
		 * fake out extsteps for cue, so that it'll end up right.
		 */
		if (allsmall(gs_p, gs_p)) {
			extsteps *= SM_STEMFACTOR / SM_FACTOR;
		}

		/* find number of flags */
		if (gs_p->basictime >= 8) {
			flags = drmo(gs_p->basictime) - 2;
		} else {
			flags = 0;
		}

		/* for equivalent flags, add in slashes or alternations */
		eqflags = flags + abs(gs_p->slash_alt);
		if (gs_p->slash_alt > 0 && gs_p->basictime >= 16) {
			eqflags++; /* slashes need extra one if 16, 32, ... */
		}

		/*
		 * Depending on how many eqflags there are, we may have to
		 * force extsteps to be bigger, so it'll look okay.
		 */
		switch (eqflags) {
		case 0:
			break;
		case 1:
			extsteps = MAX(extsteps, 6.0);
			break;
		default:
			if (flags == 2 && gs_p->slash_alt == 0 &&
					mensural_flags(gs_p)) {
				/* mensural flag 16th note flags do not need to
				 * force extra room, treat like 8th notes */
				extsteps = MAX(extsteps, 6.0);
				break;
			}
			temp = 7.0 + (eqflags - 2) * FLAGSEP / STEPSIZE;
			extsteps = MAX(extsteps, temp);
			break;
		}

		/*
		 * If the note has flag(s), is stem up, and has dot(s), we may
		 * have to force the stem longer, to prevent the flag(s) from
		 * hitting the dot(s).
		 */
		if (gs_p->basictime >= 8 && gs_p->stemdir == UP &&
				gs_p->dots != 0) {
			temp = gs_p->notelist[0].stepsup % 2 == 0 ? 9.0 : 8.0;
			temp += (flags - 2) * FLAGSEP / STEPSIZE;
			extsteps = MAX(extsteps, temp);
		}

		/* convert to inches, and for cues apply cue scaling */
		extlen = extsteps * Stepsize *
				(allsmall(gs_p, gs_p) ? SM_FACTOR : 1.0);

		/* find distance between outer notes of the group */
		notedist = gs_p->notelist[0].c[RY] -
			gs_p->notelist[ gs_p->nnotes - 1 ].c[RY];

		/* total stem length is the sum of these */
		gs_p->stemlen = extlen + notedist;

		/*
		 * Real (printed) stems must reach the center line for normal
		 * groups, though they need not for cue groups or voice 3 or
		 * when the stem direction has been forced the "wrong way" or
		 * for quad or oct notes or
		 * when all the notes are on another staff.
		 */
		if (gs_p->basictime >= 2 && gs_p->grpsize == GS_NORMAL &&
				vno != 2 && stemforced(gs_p, ogs_p) == NO &&
				NNN(gs_p) > 0) {

			if (gs_p->stemdir == UP && gs_p->notelist[ gs_p->nnotes
					- 1 ].c[RY] < -(gs_p->stemlen)) {
				gs_p->stemlen = -gs_p->notelist[ gs_p->nnotes-1
						].c[RY];
			}

			if (gs_p->stemdir == DOWN && gs_p->notelist[ 0 ].c[RY]
						> gs_p->stemlen) {
				gs_p->stemlen = gs_p->notelist[ 0 ].c[RY];
			}
		}
	}

	/*
	 * Loop through every nongrace group, skipping rests and spaces,
	 * setting the relative vertical coordinates.
	 */
	setgroupvert(GV_NORMAL, savegs_p, ogs_p);

	/*
	 * Loop through every group, looking for tuplets.  When encountering
	 * the first item in a tuplet, call a subroutine to figure out where
	 * the bracket should go, and based on that alter the RN or RS of
	 * the groups in the tuplet.  However, if this is a tuplet whose
	 * number and bracket are not to be printed, don't call the subrountine.
	 * Also, it should not be done when there is cross staff beaming.  Mup
	 * does not automatically print tuplet numbers or brackets in CSB sets.
	 */
	for (gs_p = savegs_p; gs_p != 0; gs_p = gs_p->next) {
		if ((gs_p->tuploc == STARTITEM || gs_p->tuploc == LONEITEM) &&
		    gs_p->beamto == CS_SAME && gs_p->printtup != PT_NEITHER)
			settuplet(gs_p, staff_p);
	}
}

/*
 * Name:        proctablist()
 *
 * Abstract:    Process linked list of groups on a tablature staff.
 *
 * Returns:     void
 *
 * Description: This function loops through the linked list of groups for one
 *		measure of a tablature staff.  It sets the relative vertical
 *		coords of the groups.  These coords then get altered to include
 *		"with" lists and tuplet marks.
 */

static void
proctablist(mainll_p, vno)

struct MAINLL *mainll_p;	/* MLL struct for staff we're dealing with */
int vno;			/* voice we're to deal with, 0 to MAXVOICES-1 */

{
	struct GRPSYL *gs_p;	/* point to first group in a linked list */
	int stepdiff;		/* steps between highest & lowest of a group */
	int defsteps;		/* additional default steps long to make stem*/
	int bf;			/* number of beams/flags (really slashes) */


	debug(32, "proctablist file=%s line=%d", mainll_p->inputfile,
			mainll_p->inputlineno);
	/* no such thing as cross staff stemming for tab */
	if (CSSpass == YES) {
		return;
	}

	gs_p = mainll_p->u.staff_p->groups_p[ vno ];

	/*
	 * Loop through every group, setting some group vertical coordinates.
	 */
	for ( ; gs_p != 0; gs_p = gs_p->next) {
		/*
		 * Just as for nontablature groups, RY is always 0, the center
		 * of the staff, even if it falls outside the group's
		 * rectangle.  RN and RS were set in locllnotes() and
		 * intertab() in setnotes.c. 
		 */
		gs_p->c[RY] = 0;

		/*
		 * Slashes and "with" lists are allowed only if there are
		 * frets, so if there aren't any frets, skip the rest.
		 */
		if (gs_p->grpcont != GC_NOTES || gs_p->nnotes == 0)
			continue;

		/*
		 * No tab groups have stems, but we still need to set a pseudo
		 * stem length if the group has slashes and otherwise 0.
		 */
		if (gs_p->slash_alt == 0) {
			gs_p->stemlen = 0;	/* no slashes */
		} else {
			/* find distance between outer frets of the group */
			stepdiff = gs_p->notelist[0].stepsup -
				gs_p->notelist[ gs_p->nnotes - 1 ].stepsup;

			/* default length + distance between outer notes */
			defsteps = Defstemsteps * (allsmall(gs_p, gs_p) == YES
					? SM_STEMFACTOR : 1.0);
			gs_p->stemlen = stepdiff * Stepsize * TABRATIO +
					defsteps * Stepsize;

			bf = abs(gs_p->slash_alt);	/* slashes */
			if (gs_p->basictime >= 16)
				bf++;	/* slashes need extra 1 if 16, 32, ...*/
			if (bf > 2)
				gs_p->stemlen += (bf - 2) * Flagsep;

			if (gs_p->stemdir == UP) {
				gs_p->c[RN] = gs_p->notelist[gs_p->nnotes - 1]
						.c[RN] + gs_p->stemlen;
			} else {
				gs_p->c[RS] = gs_p->notelist[0]
						.c[RY] - gs_p->stemlen;
			}
		}

		/* decrease RS based on "with" lists */
		applywith(gs_p);
	}
}

/*
 * Name:        stemforced()
 *
 * Abstract:    Did the user force stem(s) to go the wrong way?
 *
 * Returns:     YES	at least one group was forced
 *		NO	no groups were forced
 *
 * Description: This function figures out whether the user forced *gs_p's stem
 *		to go DOWN for voice 1 or UP for voice 2 when the vscheme and
 *		the other voice would normally prevent it; or if *gs_p is at
 *		the start of a beamed set, it checks this for all groups in
 *		the set.
 */

static int
stemforced(gs_p, ogs_p)

struct GRPSYL *gs_p;		/* the group we are asking about */
struct GRPSYL *ogs_p;		/* first group in other voice's linked list */

{
	RATIONAL starttime;	/* of the group in question */
	RATIONAL endtime;	/* of the group in question */
	struct GRPSYL *gs2_p;	/* loop through groups */


	/* voice 3 never cares, so is never considered to be forced */
	if (gs_p->vno == 3) {
		return (NO);
	}

	/* grace cannot be forced */
	if (gs_p->grpvalue == GV_ZERO) {
		return (NO);
	}

	switch (svpath(gs_p->staffno, VSCHEME)->vscheme) {
	case V_1:
		return (NO);	/* no forcing is needed in this vscheme */
	case V_2OPSTEM:
	case V_3OPSTEM:
		/*
		 * If and only if a stem is backwards, we are forced.  Note
		 * that even for the beamed case, we only have to check one
		 * group, since all stems in the set go the same direction.
		 */
		if ((gs_p->vno == 1 && gs_p->stemdir == DOWN) ||
		    (gs_p->vno == 2 && gs_p->stemdir == UP)) {
			return (YES);
		}
		return (NO);
	}

	/*
	 * We are in one of the freestem vschemes.
	 */

	/* if the other voice doesn't exist, we know we were not forced */
	if (ogs_p == 0) {
		return (NO);	/* other voice does not exist */
	}

	/* if all stems are normal, we are not forced (only need to check 1) */
	if ((gs_p->vno == 1 && gs_p->stemdir == UP) ||
	    (gs_p->vno == 2 && gs_p->stemdir == DOWN)) {
		return (NO);
	}

	/* check if the other voice is all spaces during this time */

	/* find start time of *gs_p by summing all previous groups */
	starttime = Zero;
	for (gs2_p = gs_p->prev; gs2_p != 0; gs2_p = gs2_p->prev) {
		starttime = radd(starttime, gs2_p->fulltime);
	}

	/* find end time of *gs_p (or the whole beamed set) */
	endtime = starttime;
	for (gs2_p = gs_p; gs2_p != 0; gs2_p = gs2_p->next) {
		endtime = radd(endtime, gs2_p->fulltime);
		if (gs2_p->beamloc == NOITEM || (gs2_p->beamloc == ENDITEM &&
						gs_p->grpvalue != GV_ZERO)) {
			break;
		}
	}

	if (hasspace(ogs_p, starttime, endtime) == YES) {
		return (NO);	/* all spaces, forcing was not needed */
	} else {
		return (YES);	/* notes/rests, we were forced */
	}
}

/*
 * Name:        setbeam()
 *
 * Abstract:    Set stem lengths for a beamed set of groups.
 *
 * Returns:     void
 *
 * Description: This function uses linear regression to figure out where the
 *		best place to put the beam is, for a beamed set of groups, or
 *		two groups that are alted together.  (Although there are
 *		special cases where the beam needs to be forced horizontal
 *		instead of using linear regression.)  But if the user specified
 *		the stem lengths of the first and last group, it just goes with
 *		that, instead of using linear regression.  It then sets the
 *		stem lengths for all the groups in the set.
 *
 *		Groups involved in cross staff beaming should never call here.
 *		That work must be done later in absvert.c.
 */

static void
setbeam(start_p, end_p, ogs_p)

struct GRPSYL *start_p;		/* first in beamed set */
struct GRPSYL *end_p;		/* after last in beamed set */
struct GRPSYL *ogs_p;		/* first group in other voice's GRPSYL list */

{
	struct GRPSYL *gs_p;	/* loop through the groups in the beamed set */
	struct GRPSYL *last_p;	/* point at last valid group before end_p */
	float sx, sy;		/* sum of x and y coords of notes */
	float xbar, ybar;	/* average x and y coords of notes */
	float top, bottom;	/* numerator & denominator for finding b1 */
	float temp;		/* scratch variable */
	float startx, endx;	/* x coord of first and last note */
	float starty, endy;	/* y coord of first and last note */
	float b0, b1;		/* y intercept and slope */
	float stemshift;	/* x distance of stem from center of note */
	float extsteps;		/* steps a stem extends beyond the last note */
	float extlen;		/* length a stem extends beyond the last note */
	float minsteps;		/* minimum number of stepsizes */
	float minstemsteps;	/* min len in steps for any group in set */
	float sumb0;		/* sum of the Y intercepts for each group */
	float reqb0;		/* required Y intercept for a group */
	float tip;		/* where the tip of the stem is */
	float x;		/* x coord of a stem */
	float cutoff;		/* in Stepsizes */
	int css_affects_beam;	/* does CSS affect the position of the beam? */
	int all_notes_other_staff; /* all notes in all groups on other staff */
	int one_end_forced;	/* is stem len forced on one end only? */
	int slope_forced;	/* is the slope of the beam forced? */
	float forced_slope;	/* slope that the user forced */
	int bf;			/* number of beams/flags */
	int accbf;		/* special use for accidental situation */
	int num;		/* number of notes */
	short *steps;		/* stepsup of beamside notes */
	int patlen;		/* length of a pattern of notes */
	int match;		/* does the pattern match? */
	int k;			/* loop variable */
	int n;			/* loop variable */


	/*
	 * If CSS was used, there are two calls to here.  If we did the work
	 * the first time, don't do it again.
	 */
	if (start_p->ran_setbeam) {
		return;
	}

	/*
	 * Find whether CSS affects the position of the beam, and whether all
	 * groups have all their notes on the other staff.  css_affects_stemtip
	 * asks (for this beamed case) whether any group's other-staff notes
	 * are stemside; that is, whether the stem points to the other staff,
	 * because then obviously the coord of the stem tip depends on where
	 * those notes are.  If all of this group's notes are on the other
	 * staff, you might expect that we would have to regard the stem tip as
	 * affected even if the stem is towards the normal staff.  But we
	 * prefer to pretend they aren't (unless the user is forcing the first
	 * or last group's stem length) so that we can handle more beamed
	 * sets on the first pass.  We fake out those groups (see the comment a
	 * little later).  And yet, if all the groups are this way, we do
	 * regard the beam as affected, because then we aren't going to enforce
	 * the rule about stems reaching the middle staff line.
	 */
	/* first set normal (non-CSS) values */
	css_affects_beam = NO;
	all_notes_other_staff = NO;
	if (CSSused == YES) {	/* don't waste time looking if CSS not used */
		all_notes_other_staff = YES;
		css_affects_beam = css_affects_stemtip(start_p);
		for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
			if (NNN(gs_p) != 0) {
				all_notes_other_staff = NO;
			}
		}
		if (all_notes_other_staff == YES) {
			css_affects_beam = YES;
		}
	}

	/*
	 * If the beam is not affected by CSS, handle this beamed set on the
	 * first pass only.  If it is affected, handle it on the second
	 * pass only.
	 */
	if (css_affects_beam != CSSpass) {
		return;
	}

	/* remember that we are doing the work on this pass */
	start_p->ran_setbeam = YES;

	/*
	 * If the beam is "not affected by CSS", there could still be groups
	 * where all the notes are CSS.  We fake them out here, setting the
	 * BNOTE's RY an octave from the center line.  We need some plausible
	 * value there for finding the beam position.  AY hasn't been used yet,
	 * so use it as a holding area.  We need to restore RY before returning
	 * from this function.
	 */
	if (CSSused == YES && CSSpass == NO) {
		for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
			if (NNN(gs_p) == 0) {
				BNOTE(gs_p).c[AY] = BNOTE(gs_p).c[RY];
				BNOTE(gs_p).c[RY] = 7 * Stepsize *
					((gs_p->stemdir == UP) ? -1.0 : 1.0);
			}
		}
	}

	last_p = 0;	/* prevent useless 'used before set' warnings */

	/* find the last valid group */
	for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
		last_p = gs_p;
	}

	/*
	 * If the user specified the stem length on one end (first or last) but
	 * not the other, remember that fact.  In that case we will execute the
	 * normal (both ends unforced) algorithm, but then at the last minute
	 * force the end that was given.
	 */
	one_end_forced = IS_STEMLEN_KNOWN(start_p->stemlen) !=
			 IS_STEMLEN_KNOWN(last_p->stemlen);

	/*
	 * If the user specified the stem length for the first and last groups,
	 * simply use these values to define where the beam is, and set all the
	 * stem lengths.
	 */
	if (IS_STEMLEN_KNOWN(start_p->stemlen) &&
	    IS_STEMLEN_KNOWN(last_p->stemlen)) {

		/*
		 * If the first and last groups had stemlen set to zero, force
		 * all groups to have stemlen zero, and return.  No beam will
		 * be drawn.
		 */
		if (start_p->stemlen == 0.0 && last_p->stemlen == 0.0) {
			for (gs_p = start_p; gs_p != end_p;
					gs_p = nextsimilar(gs_p)) {
				gs_p->stemlen = 0.0;
			}
			restore_ry(start_p, end_p);
			return;
		}

		/* they weren't both zero, so continue on finding the beam */
		start_p->stemlen *= Staffscale;
		stemshift = getstemshift(start_p);
		if (start_p->stemdir == DOWN)
			stemshift = -stemshift;
		last_p->stemlen *= Staffscale;

		/* find coords of the ends of the stems on the outer groups */
		startx = start_p->c[AX] + stemshift;
		endx = last_p->c[AX] + stemshift;
		starty = BNOTE(start_p).c[RY] + start_p->stemlen *
				(start_p->stemdir == UP ? 1.0 : -1.0);
		endy = BNOTE(last_p).c[RY] + last_p->stemlen *
				(last_p->stemdir == UP ? 1.0 : -1.0);

		/* find slope and y intercept of line through those points */
		b1 = (starty - endy) / (startx - endx);
		b0 = starty - b1 * startx;

		/* loop through all groups, setting stem length */
		for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
			x = gs_p->c[AX] + stemshift;	/* X coord of stem */

			/* first set stemlen to beam's Y coord minus note's */
			gs_p->stemlen = (b0 + b1 * x) - BNOTE(gs_p).c[RY];

			/* if stems are down, reverse it */
			if (gs_p->stemdir == DOWN)
				gs_p->stemlen = -(gs_p->stemlen);

			finalstemadjust(gs_p);
		}

		/* set relative vertical coords of any embedded rests */
		embedrest(start_p, start_p, (struct GRPSYL *)0, b1, b0);

		restore_ry(start_p, end_p);
		return;
	}

	/*
	 * If the user forced the beam's angle to some value, find what that is
	 * in terms of slope.  Later we will force this value to be used.  The
	 * 0.001 is to allow for floating point roundoff error.
	 */
	if (fabs(start_p->beamslope - NOBEAMANGLE) < 0.001) {
		slope_forced = NO;
		forced_slope = 0.0;	/* not used, keep lint happy */
	} else {
		slope_forced = YES;
		forced_slope = tan(start_p->beamslope * PI / 180.0);
	}

	/*
	 * When both end groups have stemlen zero, we set all groups' stemlens
	 * to zero, and no beam will be drawn.  Above we handled the case
	 * where the user forced both ends to zero.  Here we handle the case
	 * where the ends are defaulting to zero, or one end is defaulting to
	 * zero and the user forced the other one.  But don't do this if the
	 * slope is forced.
	 */
	if (Defstemsteps == 0.0 && ! slope_forced && ( ! one_end_forced ||
			start_p->stemlen == 0.0 || last_p->stemlen == 0.0)) {
		for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
			gs_p->stemlen = 0.0;
		}
		restore_ry(start_p, end_p);
		return;
	}

	/*
	 * Use linear regression to find the best-fit line through the place
	 * where the tips of the stems of the groups would be if the stems
	 * weren't beamed and so could be the "desired" length, based on the
	 * stemlen and stemshorten parameters.  The X coordinates are the AX
	 * (absolute X) of the groups, but the Y coordinates are RY, relative
	 * to the center line of the staff, since we don't know the absolute Y
	 * coords yet, and it wouldn't affect the result anyway.
	 *
 	 * First get sum of x and y coords, to find averages.
	 */
	sx = sy = 0;
	num = 0;
	for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
		sx += gs_p->c[AX];
		sy += BNOTE(gs_p).c[RY] + desired_vert_stemoffset(gs_p);
		num++;			/* count number of notes */
	}

	xbar = sx / num;
	ybar = sy / num;

	/* accumulate numerator & denominator of regression formula for b1 */
	top = bottom = 0;
	for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
		temp = gs_p->c[AX] - xbar;
		top += temp * (BNOTE(gs_p).c[RY] +
				desired_vert_stemoffset(gs_p) - ybar);
		bottom += temp * temp;
	}

	b1 = top / bottom;		/* slope */
	/*
	 * We could also figure:
	 *	b0 = ybar - b1 * xbar;		y intercept
	 * to get the equation of the regression line:  y = b0 + b1 * x
	 * but we're going to change b0 later anyway.  Now, there are certain
	 * cases where we want to override the slope determined by regression,
	 * so revise b1 if that is the case.
	 */

	/* if first and last notes are equal, force horizontal */
	if (BNOTE(start_p).stepsup == BNOTE(last_p).stepsup)
		b1 = 0.0;

	/* check for more reasons to force the beam horizontal */
	if (b1 != 0.0 && num >= 3) {
		/* get an array of each group's beamside note's stepsup */
		MALLOCA(short, steps, num);
		for (n = 0, gs_p = start_p; n < num;
				n++, gs_p = nextsimilar(gs_p)) {
			steps[n] = BNOTE(gs_p).stepsup;
		}

		/*
		 * Check for a repeating pattern of notes.  Try every possible
		 * pattern length <= half as long as set.  If found, force the
		 * beam horizontal.
		 */
		for (patlen = num / 2; patlen >= 2; patlen--) {
			/* must be an integer number of pattern repetitions */
			if (num % patlen != 0) {
				continue;	/* groups were left over */
			}
			/* see if initial pattern repeats perfectly */
			match = YES;
			for (n = 0; n < patlen && match == YES; n++) {
				for (k = n + patlen; k < num; k += patlen) {
					if (steps[k] != steps[n]) {
						match = NO;
						break;
					}
				}
			}
			/* if all repeats matched, force horizontal & break */
			if (match == YES) {
				b1 = 0.0;
				break;
			}
		}

		/*
		 * If still not horizontal, check for the case where all the
		 * beamside notes are the same except for just the first, or
		 * just the last, being different and in the direction
		 * opposite the stemdir.  If so, force horizontal.
		 */
		if (b1 != 0.0) {
			/* make sure all the inner groups are the same */
			match = YES;
			for (n = 2; n < num - 1; n++) {
				if (steps[n] != steps[1]) {
					match = NO;
					break;
				}
			}
			/* if inner groups same, check the other conditions */
			if (match == YES) {
				if (start_p->stemdir == DOWN) {
					if ((steps[0] > steps[1] &&
					    steps[num-1] == steps[1]) ||
					    (steps[0] == steps[1] &&
					    steps[num-1] > steps[1])) {
						b1 = 0.0;
					}
				} else {	/* UP */
					if ((steps[0] < steps[1] &&
					    steps[num-1] == steps[1]) ||
					    (steps[0] == steps[1] &&
					    steps[num-1] < steps[1])) {
						b1 = 0.0;
					}
				}
			}
		}
		FREE(steps);
	}

	/*
	 * Find half the width of a note head; the stems will need to be
	 * shifted by that amount from the center of the notes so that they
	 * will meet the edge of the notes properly.  If the stems are up,
	 * they will be on the right side of (normal) notes, else left.  Set
	 * the X positions for the first and last stems.  (If these are alted
	 * groups, the noteheadchar may not be 4; but this is close enough.)
	 */
	stemshift = getstemshift(start_p);
	if (start_p->stemdir == DOWN)
		stemshift = -stemshift;
	startx = start_p->c[AX] + stemshift;	/* first group's stem */
	endx = last_p->c[AX] + stemshift;	/* last group's stem */

	/*
	 * The original slope derived by linear regression must be adjusted in
	 * certain ways.  First, override it if the user wants that; otherwise
	 * adjust according to the beamslope parameter.
	 */
	if (slope_forced) {
		b1 = forced_slope;
	} else {
		b1 = adjslope(start_p, b1, NO, BEAMSLOPE);
	}

	/*
	 * Calculate a new y intercept (b0).  First find for each group
	 * individually where it would like b0 to be, and average the answers.
	 */
	sumb0 = 0.0;
	for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
		/* get desired stem protrusion based on stemlen & stemshorten */
		extsteps = stemextsteps(gs_p);

		/* if all small notes, apply factor */
		if (allsmall(gs_p, gs_p)) {
			if (gs_p->grpsize == GS_TINY) {
				extsteps *= TINY_STEMFACTOR;
			} else {
				extsteps *= SM_STEMFACTOR;
			}
		}

		/* convert to inches */
		extlen = extsteps * Stepsize;

		/* find where this group would like the stem tip to be */
		tip = BNOTE(gs_p).c[RY];
		if (gs_p->stemdir == UP) {
			tip += extlen;
		} else {
			tip -= extlen;
		}

		/* find where that puts the Y intercept, add into the sum */
		sumb0 += tip - b1 * gs_p->c[AX];
	}

	/* take the average */
	b0 = sumb0 / num;

	/* find min stem length as determined by the first parm of stemshorten*/
	minstemsteps = Defstemsteps;
	if (allsmall(start_p, last_p)) {
		if (start_p->grpsize == GS_TINY) {
			minstemsteps *= TINY_STEMFACTOR;
		} else {
			minstemsteps *= SM_STEMFACTOR;
		}
	}
	minstemsteps -=	vvpath(start_p->staffno, start_p->vno, BEAMSHORT)->
			beamshort;
	minstemsteps = MAX(minstemsteps, MINSTEMLENFRAC * Defstemsteps);

	/*
	 * Now go through the groups again, forcing b0 to change if any of
	 * them require a longer stem than what they're getting.
	 */
	for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {

		/* we know we need to be at least this long */
		extsteps = minstemsteps;

		/* get number of beams, and adjust to "equivalent beams" */
		if (gs_p->basictime >= 8) {
			bf = drmo(gs_p->basictime) - 2;	/* no. of beams */
		} else {
			bf = 0;			/* none on quarter or longer */
		}
		accbf = bf;	/* for use in the "acc" block below */
		if (gs_p->grpvalue == GV_NORMAL) {
			bf += abs(gs_p->slash_alt);/* slashes or alternations */
			/* accs only care about alternations */
			if (gs_p->slash_alt < 0) {
				accbf -= gs_p->slash_alt;
			}
		}
		if (bf > 2.0) {
			minsteps = 7.0 + (bf - 2) * (FLAGSEP / STEPSIZE);
		} else {
			minsteps = 5.0 + (bf - 1) * (FLAGSEP / STEPSIZE);
		}
		extsteps = MAX(extsteps, minsteps);
		extlen = extsteps * Stepsize;
		if (allsmall(gs_p, gs_p)) {
			if (gs_p->grpsize == GS_TINY) {
				extlen *= TINY_STEMFACTOR;
			} else {
				extlen *= SM_STEMFACTOR;
			}
		}

		/* find where the stem tip would be; change b0 if need be */
		tip = BNOTE(gs_p).c[RY];
		if (gs_p->stemdir == UP) {
			tip += extlen;
			reqb0 = tip - b1 * gs_p->c[AX];
			b0 = MAX(b0, reqb0);
		} else {
			tip -= extlen;
			reqb0 = tip - b1 * gs_p->c[AX];
			b0 = MIN(b0, reqb0);
		}
		/*
		 * In certain cases where there are accidentals, we need to
		 * move the beam farther away, to avoid collisions.  For the
		 * first group, there is never a problem, because the accs are
		 * left of the beam.
		 *
		 */
		if (gs_p != start_p) {
			b0 = acc_beam(gs_p, b1, b0, RY, accbf);
		}
	}

	/*
	 * Another adjustment may be needed so that all stems will reach the
	 * center line of the staff.  (Not to be done for small groups, or when
	 * all notes in all groups are on the other staff [CSS], or when
	 * some stemdirs have been forced wrong way despite the other voice, or
	 * we have alternations and no normal beams, or for voice 3.)
	 */
	starty = b0 + b1 * startx;	/* y coord near left end of beam */
	endy = b0 + b1 * endx;		/* y coord near right end of beam */
	if (start_p->basictime >= 2 && start_p->grpsize == GS_NORMAL &&
			stemforced(start_p, ogs_p) == NO &&
			start_p->vno != 3 && all_notes_other_staff == NO) {
		if (slope_forced) {
			/* move both ends the same amount to preserve slope */
			if (start_p->stemdir == UP) {
				if (starty < 0) {
					endy -= starty;
					starty = 0;
				}
				if (endy < 0) {
					starty -= endy;
					endy = 0;
				}
			} else { /* DOWN */
				if (starty > 0) {
					endy -= starty;
					starty = 0;
				}
				if (endy > 0) {
					starty -= endy;
					endy = 0;
				}
			}
		} else {
			/* move just the end(s) that need to be moved */
			if (start_p->stemdir == UP) {
				if (starty < 0)
					starty = 0;
				if (endy < 0)
					endy = 0;
			} else { /* DOWN */
				if (starty > 0)
					starty = 0;
				if (endy > 0)
					endy = 0;
			}
		}
	}

	/*
	 * If the first and last groups's stems now end at the center line, and
	 * the beam slope used to be nonzero, force one end to be a step beyond
	 * the center line, so that the beam will still have some slope to it.
	 * But don't do this if the user is forcing the beam's slope.
	 */
	if ( ! slope_forced && fabs(starty) < Stdpad &&
				fabs(endy) < Stdpad && b1 != 0.0) {
		if (start_p->stemdir == UP) {
			if (b1 > 0.0) {
				endy = Stepsize;
			} else if (b1 < 0.0) {
				starty = Stepsize;
			}
		} else {	/* DOWN */
			if (b1 > 0.0) {
				starty = -Stepsize;
			} else if (b1 < 0.0) {
				endy = -Stepsize;
			}
		}
	}

	/*
	 * If y at the ends of the beam differs by less than a certain cutoff,
	 * force the beam horizontal by setting one end farther away from the
	 * notes.  We do this because lines very close to horizontal may not
	 * print too well, and may not look too good against staff lines.
	 * But don't do it if the user is forcing a particular slope.  Also,
	 * if there are only 2 notes, use a cutoff small enough that, with the
	 * default settings of stemshorten, even if the notes are one stepsize
	 * apart they won't be forced horizontal, because that looks bad.  If
	 * there are more notes, it isn't obvious to the eye what the slope
	 * should be, and we can afford a higher cutoff.
	 */
	cutoff = Stepsize * (num == 2 ?
		DEFBEAMFACT * (1.0 - (DEFMAXPROSHORT /
		(DEFENDPROSHORT - (DEFBEGPROSHORT-1)))) : 1.0) - 0.001;
	if ( ! slope_forced && fabs(starty - endy) < cutoff) {
		if (start_p->stemdir == UP) {
			if (starty > endy) {
				endy = starty;
			} else {
				starty = endy;
			}
		} else {	/* DOWN */
			if (starty < endy) {
				endy = starty;
			} else {
				starty = endy;
			}
		}
	}

	/* recalculate slope and y intercept from (possibly) new endpoints */
	b1 = (endy - starty) / (endx - startx);		/* slope */
	b0 = starty - b1 * startx;			/* y intercept */
	temp = b0;			/* remember this value for later */

	/* do some additional work for nongrace groups */
	if (start_p->grpvalue == GV_NORMAL) {
		/*
		 * If this is not an alted pair, there may be embedded grace
		 * notes, and we may need to lengthen our stems to avoid them.
		 */
		if (start_p->slash_alt >= 0)
			b0 = embedgrace(start_p, b1, b0);

		/* may need to lengthen stems to avoid embedded clefs */
		b0 = embedclef(start_p, b1, b0);

		/* set relative vertical coords of any embedded rests */
		embedrest(start_p, start_p, (struct GRPSYL *)0, b1, b0);

		/*
		 * If there is another voice, we might need to lengthen our
		 * stems so their notes won't run into our beam.  If we had
		 * embedded rests, they would also be moved.
		 */
		b0 = avoidothervoice(start_p, last_p, b1, b0, ogs_p);

		/* update these by the amount the y intercept changed */
		starty += temp - b0;
		endy += temp - b0;
	}

	restore_ry(start_p, end_p);

	/*
	 * If one end's stem len was forced but not the other, now is the time
	 * to apply that forcing.  So in effect, we have taken the beam as
	 * determined by the normal algorithm and now we change the vertical
	 * coord of this end.  If the slope was also forced, move the other
	 * end by the same amount so that the slope won't change.
	 */
	if (one_end_forced) {
		if (IS_STEMLEN_KNOWN(start_p->stemlen)) {
			start_p->stemlen *= Staffscale;
			temp = starty;
			starty = BNOTE(start_p).c[RY] + start_p->stemlen *
					(start_p->stemdir == UP ? 1.0 : -1.0);
			if (slope_forced) {
				endy += starty - temp;
			}
		} else {
			last_p->stemlen *= Staffscale;
			temp = endy;
			endy = BNOTE(last_p).c[RY] + last_p->stemlen *
					(last_p->stemdir == UP ? 1.0 : -1.0);
			if (slope_forced) {
				starty += endy - temp;
			}
		}

		/* recalculate */
		b1 = (endy - starty) / (endx - startx);	/* slope */
		b0 = starty - b1 * startx;		/* y intercept */

		/*
		 * Re-do embedded rests now that things have moved.  As for the
		 * other adjustments above, we can't re-do them because they
		 * may force stem lengths to change.  If things collide, too
		 * bad, the user forced the one stem length.  It might be
		 * possible to avoid the collision by moving the other end,
		 * but likely not, and it's too late now anyhow.
		 */
		embedrest(start_p, start_p, (struct GRPSYL *)0, b1, b0);
	}

	/*
	 * At this point we know where to put the main beam (the one needed for
	 * eighth notes).  Figure out and set the correct stem lengths for all
	 * of these beamed groups.
	 */
	for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
		x = gs_p->c[AX] + stemshift;	/* X coord of stem */

		/* first set stemlen to beam's Y coord minus note's */
		gs_p->stemlen = (b0 + b1 * x) - BNOTE(gs_p).c[RY];

		/* if stems down, reverse stemlen, should make it positive */
		if (gs_p->stemdir == DOWN) {
			gs_p->stemlen = -(gs_p->stemlen);
		}
		/* but if negative length, error */
		if (gs_p->stemlen < 0) {
			l_ufatal(gs_p->inputfile, gs_p->inputlineno,
					"stem length was forced negative");
		}

		finalstemadjust(gs_p);
	}
}

/*
 * Name:        mensural_flags()
 *
 * Abstract:    Does this GRPSYL use mensural flags?
 *
 * Returns:     YES or NO
 *
 * Description: This function finds whether any flags on the given group will
 *		be mensural.
 */

static int
mensural_flags(gs_p)

struct GRPSYL *gs_p;

{
	int musfont;
	int muschar;


	musfont = FONT_MUSIC;

	if (gs_p->stemdir == UP) {
		muschar = C_DNFLAG;
	} else {
		muschar = C_UPFLAG;
	}

	(void)get_shape_override(gs_p->staffno, gs_p->vno, &musfont, &muschar);

	/* if it got translated to a mensural flag, return YES */
	if (muschar == C_MENSURUPFLAG || muschar == C_MENSURDNFLAG) {
		return (YES);
	}

	return (NO);
}

/*
 * Name:        restore_ry()
 *
 * Abstract:    Restore RY coordinates if need be.
 *
 * Returns:     void
 *
 * Description: This function undoes what the code near the start of setbeam()
 *		did.  But it doesn't have to set AY back, because it is garbage
 *		and will be overwritten later anyway.
 */

static void
restore_ry(start_p, end_p)

struct GRPSYL *start_p;		/* first in beamed set */
struct GRPSYL *end_p;		/* after last in beamed set */

{
	struct GRPSYL *gs_p;	/* loop through the groups in the beamed set */


	if (CSSused == YES && CSSpass == NO) {
		for (gs_p = start_p; gs_p != end_p; gs_p = nextsimilar(gs_p)) {
			if (NNN(gs_p) == 0) {
				BNOTE(gs_p).c[RY] = BNOTE(gs_p).c[AY];
			}
		}
	}
}

/*
 * Name:        desired_vert_stemoffset()
 *
 * Abstract:    Find the desired amount that a stem should extend.
 *
 * Returns:     distance in inches, negative means down
 *
 * Description: For a group in a beamed set, this function finds the desired
 *		amount that the stem should extend out from the stemside note.
 *		This is just the desired amount, so we don't consider the
 *		various factors that will later influence where the beam will
 *		actually go.
 */

static double
desired_vert_stemoffset(gs_p)

struct GRPSYL *gs_p;		/* group */

{
	double answer;


	/* convert steps to inches, scaled by staffscale */
	answer = stemextsteps(gs_p) * Stepsize;

	if (gs_p->stemdir == DOWN) {
		answer = -answer;		/* negate for down stem */
	}

	if (allsmall(gs_p, gs_p)) {
		answer *= SM_STEMFACTOR;	/* change for cue/grace */
	}

	return (answer);
}

/*
 * Name:        embedgrace()
 *
 * Abstract:    Change the Y intercept if necessary for embedded grace groups.
 *
 * Returns:     new y intercept value (may be no change)
 *
 * Description: When grace groups are embedded inside a set of nongrace groups,
 *		the beam(s) for the nongrace may have to be put farther away
 *		from their note heads, so that these beams won't collide with
 *		the grace groups.  This function returns the new Y intercept
 *		for the equation of the nongraces' main beam, which accom-
 *		plishes this.  When there aren't any embedded grace groups,
 *		or they are in certain positions, this Y intercept will be the
 *		same as the old Y intercept.
 */

static double
embedgrace(start_p, b1, b0)

struct GRPSYL *start_p;	/* first group in nongrace beamed set */
double b1;		/* slope */
double b0;		/* y intercept */

{
	struct GRPSYL *gs_p;	/* point to grace group being looked at */
	struct GRPSYL *prev_p;	/* point to nongrace group preceding gs_p */
	struct GRPSYL *next_p;	/* point to nongrace group following gs_p */
	float beamthick;	/* total thickness of beams and space between*/
	float ycross;		/* where grace stem would hit nongrace beam */


	/*
	 * Loop through all the grace groups that are embedded somewhere
	 * between the first and last groups of this nongrace beamed set.
	 * If their stems point the opposite way, there is no problem.  But
	 * if not, we may need to move the main beam(s) out of the way.
	 */
	for (gs_p = start_p; gs_p->grpvalue == GV_ZERO ||
				gs_p->beamloc != ENDITEM; gs_p = gs_p->next) {
		if (gs_p->grpvalue == GV_NORMAL)
			continue;	/* ignore nongrace groups */

		/*
		 * Find the preceding and following nongrace group.  Whichever
		 * has the least (slowest) basictime, that determines how many
		 * full beams will connect those two groups.  (You take log2 of
		 * it and subtract 2.)
		 */
		prev_p = prevnongrace(gs_p);
		next_p = nextnongrace(gs_p);

		/* thickness of relevant beams at right side of grace */
		beamthick = beamoff(next_p, PB_LEFT, gs_p->c[AE], start_p);

		/*
		 * Find the AX and RY coords of the end of the grace group
		 * stem that is nearest the nongrace beam(s).  Then, if this
		 * point would run into or beyond the nongrace beam(s), change
		 * the Y intercept (b0) so that it won't.
		 */
		ycross = b1 * gs_p->c[AE] + b0;
		if (start_p->stemdir == UP) {
			if (ycross - beamthick < gs_p->c[RN])
				b0 += gs_p->c[RN] - (ycross - beamthick);
		} else {	/* stemdir == DOWN */
			if (ycross + beamthick > gs_p->c[RS])
				b0 -= (ycross + beamthick) - gs_p->c[RS];
		}

		/* thickness of relevant beams at left side of grace */
		beamthick = beamoff(prev_p, PB_RIGHT, gs_p->c[AW], start_p);

		ycross = b1 * gs_p->c[AW] + b0;
		if (start_p->stemdir == UP) {
			if (ycross - beamthick < gs_p->c[RN])
				b0 += gs_p->c[RN] - (ycross - beamthick);
		} else {	/* stemdir == DOWN */
			if (ycross + beamthick > gs_p->c[RS])
				b0 -= (ycross + beamthick) - gs_p->c[RS];
		}
	}

	return (b0);	/* new (possibly changed) Y intercept */
}

/*
 * Name:        embedclef()
 *
 * Abstract:    Change the Y intercept if necessary for embedded clefs.
 *
 * Returns:     new y intercept value (may be no change)
 *
 * Description: When clef changes occur before groups in a beamed set, the
 *		beam(s) for the set may have to be put farther away from their
 *		note heads, so that these beams won't collide with the clefs.
 *		This function returns the new Y intercept for the equation of
 *		the nongraces' main beam, which accomplishes this.  When there
 *		aren't any embedded clefs, or they are in certain positions,
 *		this Y intercept will be the same as the old Y intercept.
 */

static double
embedclef(start_p, b1, b0)

struct GRPSYL *start_p;	/* first group in nongrace beamed set */
double b1;		/* slope */
double b0;		/* y intercept */

{
	struct GRPSYL *gs_p;	/* point to group being looked at */
	struct GRPSYL *pbgs_p;	/* group whose partial beams may impact us */
	float north, south;	/* top and bottom edge of a clef */
	float horizontal;	/* left or right edge of a clef */
	float beamthick;	/* total thickness of beams and space between*/
	float ycross;		/* where grace stem would hit nongrace beam */


	/*
	 * Loop through all the groups between the first and last groups of
	 * this nongrace beamed set, including the last but not the first, and
	 * including any embedded graces.  If any are preceded by a clef, we
	 * may need to move the beam(s) out of the way.
	 */
	for (gs_p = start_p->next; gs_p != 0 && ! (gs_p->prev->grpvalue ==
			GV_NORMAL && gs_p->prev->beamloc == ENDITEM);
			gs_p = gs_p->next) {

		if (gs_p->clef == NOCLEF) {
			continue;	/* ignore groups with no clef */
		}

		/* find the vertical edges of the clef */
		(void)clefvert(gs_p->clef, gs_p->staffno, YES, &north, &south);
		north *= Staffscale;
		south *= Staffscale;

		/*
		 * Make sure the right side of the clef doesn't collide with
		 * the beams.
		 */
		/* find right side of the clef */
		horizontal = gs_p->c[AW] - CLEFPAD * Staffscale;

		/* group whose partial beams we need to worry about */
		pbgs_p = gs_p->grpvalue == GV_ZERO ? nextnongrace(gs_p) : gs_p;

		/* thickness of relevant beams at right side of clef */
		beamthick = beamoff(pbgs_p, PB_LEFT, horizontal, start_p);

		/* Find RY where right edge of clef would hit the main beam. If
		 * that edge of clef would hit any beam, change Y intercept. */
		ycross = b1 * horizontal + b0;
		if (start_p->stemdir == UP) {
			if (ycross - beamthick < north) {
				b0 += north - (ycross - beamthick);
			}
		} else {	/* stemdir == DOWN */
			if (ycross + beamthick > south) {
				b0 -= (ycross + beamthick) - south;
			}
		}

		/*
		 * Make sure the left side of the clef doesn't collide with
		 * the beams.
		 */
		/* find left side of the clef */
		horizontal -= clefwidth(gs_p->clef, gs_p->staffno,YES)
				* Staffscale;

		/* group whose partial beams we need to worry about */
		pbgs_p = prevnongrace(gs_p);

		/* thickness of relevant beams at left side of clef */
		beamthick = beamoff(pbgs_p, PB_RIGHT, horizontal, start_p);

		/* Find RY where left edge of clef would hit main beam.  If
		 * that edge of clef would hit any beam, change Y intercept. */
		ycross = b1 * horizontal + b0;
		if (start_p->stemdir == UP) {
			if (ycross - beamthick < north) {
				b0 += north - (ycross - beamthick);
			}
		} else {	/* stemdir == DOWN */
			if (ycross + beamthick > south) {
				b0 -= (ycross + beamthick) - south;
			}
		}
	}

	return (b0);	/* new (possibly changed) Y intercept */
}

/*
 * Name:        beamoff()
 *
 * Abstract:    On one side of group, get height of beams and spaces between.
 *
 * Returns:     height in inches
 *
 * Description: This function is called with a nongrace group in beamed set, to
 *		find out how many beams it has on one side of it and how high
 *		they are.  If the group is the first or last in the set, the
 *		side must be the interior side.  Partial beams are also figured
 *		in, if they might extend far enough to reach the "boundary"
 *		coordinate.
 */

static double
beamoff(gs_p, side, boundary, start_p)

struct GRPSYL *gs_p;	/* group we are concerned with */
int side;		/* which side of the group, PB_LEFT or PB_RIGHT */
double boundary;	/* X coord of edge of thing that must not collide */
struct GRPSYL *start_p;	/* first group in nongrace beamed set */

{
	struct GRPSYL *ogs_p;	/* nongrace group on "side" side of gs_p */
	struct GRPSYL *o2gs_p;	/* nongrace group on other side of gs_p */
	int beams;		/* number of beams for figuring collision */
	int minbasic;		/* minimum (longest) basictime */


	/*
	 * If it's the left side of this group we're worried about, set ogs_p
	 * to the previous nongrace, and o2gs_p to the next.  If right, do the
	 * opposite.
	 */
	if (side == PB_LEFT) {
		ogs_p = prevnongrace(gs_p);
		o2gs_p = nextnongrace(gs_p);
	} else {
		ogs_p = nextnongrace(gs_p);
		o2gs_p = prevnongrace(gs_p);
	}

	/*
	 * Whichever of the two groups {this group, the group on the side
	 * that we're worried about} has the least (slowest) basictime, that
	 * determines how many full beams will connect those two groups.  (You
	 * take log2 of it and subtract 2.)
	 */
	minbasic = MIN(gs_p->basictime, ogs_p->basictime);
	if (minbasic >= 8) {
		beams = drmo(MIN(gs_p->basictime, ogs_p->basictime)) - 2;
	} else {
		beams = 0;	/* must be an alternation */
	}

	/* add the number of alternation beams, if any */
	if (gs_p->slash_alt < 0) {
		beams -= gs_p->slash_alt;
	}

	/*
	 * If our group needs more beams than the group on the requested side,
	 * and the stem is in the direction where partial beams would stick out
	 * beyond our GRPSYL boundary and the partial beams are long enough to
	 * possibly collide with the thing we're trying to avoid . . .
	 */
	if (gs_p->basictime > ogs_p->basictime &&
			((side == PB_LEFT && gs_p->stemdir == DOWN &&
				gs_p->c[AW] - 5.0 * Stepsize < boundary) ||
			(side == PB_RIGHT && gs_p->stemdir == UP &&
				gs_p->c[AE] + 5.0 * Stepsize > boundary))) {
		/*
		 * If we are the start or end of this beamed set, or we need
		 * more beams than the group on the other side . . .
		 */
		if (gs_p->beamloc == STARTITEM || gs_p->beamloc == ENDITEM ||
				gs_p->basictime > o2gs_p->basictime) {
			/*
			 * We have partial beam(s); if on the side that matters
			 * to us, reset the number of beams to include partials.
			 */
			if (pbeamside(gs_p, start_p) == side) {
				beams = drmo(gs_p->basictime) - 2;
			}
		}
	}

	/*
 	 * To get total beam thickness, multiply the size of one beam by the
	 * number of beams.  Also add in a small fudge factor.
	 */
	return (Flagsep * beams + Stepsize / 2.0);
}

/*
 * Name:        embedrest()
 *
 * Abstract:    Set relative vertical coords of rests embedded in beamed sets.
 *
 * Returns:     void
 *
 * Description: Rests' vertical coords were set in restsyl.c.  But when a rest
 *		is embedded in a beamed set, its coords may have to be changed
 *		now so that it fits well.  The beamed set must be nongrace.
 *		Rests can't be embedded in grace beamed sets.  This function
 *		handles non-CSB, and also the CSB case where the stem directions
 *		on the two staffs are the same.
 */

void
embedrest(first_p, start1_p, start2_p, b1, b0)

struct GRPSYL *first_p;	/* note group at start of beamed set */
struct GRPSYL *start1_p;/* first group in top staff of beamed set */
			/* start1_p == first_p if this is not CSB */
struct GRPSYL *start2_p;/* first group in bottom staff, 0 if not CSB */
double b1;		/* slope */
double b0;		/* y intercept */

{
	struct GRPSYL *gs_p;	/* point to group in the set */
	struct GRPSYL *gp_p, *gpp_p; /* prev nongrace note, and prev to that */
	struct GRPSYL *gn_p, *gnn_p; /* next nongrace note, and next to that */
	int bp, bn;		/* beams on gp_p and gn_p */
	int partial;		/* partial beams in our way */
	int rchar;		/* char for the rest */
	int rfont;		/* font for the rest */
	int size;		/* font size */
	float asc, des;		/* ascent and descent of a rest */
	float beamthick;	/* total thickness of beams and space between*/
	float ycross;		/* where rest would hit beam */
	float *yr_p, *nr_p, *sr_p;	/* point at rest coordinates */
	float *ng_p, *sg_p;	/* point at group coordinates (don't need Y) */
	float move;		/* how much we have to move a rest */
	int beams;		/* number of beams joining two groups */


	/*
	 * If not CSB (start2_p == 0), point gs_p at the first group, which
	 * will be a note group.  If CSB, point at the first group (a note or
	 * space group) of the voice that is nearest the beam.
	 */
	if (start2_p == 0 || first_p->stemdir == UP) {
		gs_p = start1_p;
	} else {
		gs_p = start2_p;
	}

	/* sets of grace groups cannot have embedded rests */
	if (gs_p->grpvalue == GV_ZERO) {
		return;
	}

	/*
	 * Loop through the interior groups of this set on the staff that is
	 * beamside, setting relative vertical coords of rest groups.  (Outer
	 * groups are never rests.)
	 */
	for (gs_p = gs_p->next; gs_p->grpvalue == GV_ZERO ||
			gs_p->beamloc != ENDITEM; gs_p = gs_p->next) {

		/* skip nonrests */
		if (gs_p->grpcont != GC_REST)
			continue;

		/* skip cases where the user is forcing the coords */
		if (gs_p->restdist != NORESTDIST)
			continue;

		rchar = restchar(gs_p, &rfont);
		size = (gs_p->grpsize == GS_NORMAL ? DFLT_SIZE : SMALLSIZE);
		asc = ascent(rfont, size, rchar) * Staffscale;
		des = descent(rfont, size, rchar) * Staffscale;


		/*
		 * Find prev nongrace note group, whichever staff.  We know
		 * there is one in this beamed set, because a beamed group
		 * must have a note group at the start.
		 */
		gp_p = prevbmgrp(gs_p, start1_p);

		/* find prev nongrace note group to that in this set, if any */
		if (gp_p->beamloc == STARTITEM) {
			gpp_p = 0;
		} else {
			gpp_p = prevbmgrp(gp_p, start1_p);
		}


		/*
		 * Find next nongrace note group, whichever staff.  We know
		 * there is one in this beamed set, because a beamed group
		 * must have a note group at the end.
		 */
		gn_p = nextbmgrp(gs_p, start1_p, (struct GRPSYL *)0);

		/* find next nongrace note group to that in this set, if any */
		if (gn_p->beamloc == ENDITEM) {
			gnn_p = 0;
		} else {
			gnn_p = nextbmgrp(gn_p, start1_p, (struct GRPSYL *)0);
		}


		/* get number of beams needed by prev and next */
		bp = numbeams(gp_p->basictime);
		bn = numbeams(gn_p->basictime);

		partial = 0;	/* init to no partial beams */

		/*
		 * If the group just before our rest is notes, and this beamed
		 * set's stems are up, and the prev note needs more beams than
		 * the next note, we may have to deal with partial beams.
		 * For non-CSB, we just look at the previous group.  For CSB,
		 * we look on either staff for a note or rest group.
		 */
		if (((start2_p == 0 && gs_p->prev->grpcont == GC_NOTES) ||
		     (start2_p != 0 && prev_bm_grp_w_rests(gs_p, start1_p)
					->grpcont == GC_NOTES))
		     && first_p->stemdir == UP && bp > bn) {

			if (gpp_p == 0) {
				/* definitely partial beams on this side */
				partial = bp - bn;
			} else {
				/* maybe partial beams on this side */
				if (numbeams(gpp_p->basictime) < bp &&
				pbeamside(gp_p, start1_p) == PB_RIGHT)
					partial = bp - bn;
			}
			/* but if far enough away horizontally, we can ignore */
			if (gs_p->c[AW] - gp_p->c[AE] > 1.5 * Stepsize)
				partial = 0;
		}

		/*
		 * If the group just after our rest is notes, and this beamed
		 * set's stems are down, and the next note needs more beams than
		 * the prev note, we may have to deal with partial beams.  If
		 * the next group is grace, we might fall into this block, but
		 * that's okay; the next nongrace (gn_p) will be far enough
		 * away that partial will (correctly) be forced back to 0.
		 * For non-CSB, we just look at the next group.  For CSB, we
		 * look on either staff for a note or rest group.
		 */
		if (((start2_p == 0 && gs_p->next->grpcont == GC_NOTES) ||
		     (start2_p != 0 && next_bm_grp_w_rests(gs_p, start1_p,
				(struct GRPSYL *)0) ->grpcont == GC_NOTES))
		     && first_p->stemdir == DOWN && bn > bp) {

			if (gnn_p == 0) {
				/* definitely partial beams on this side */
				partial = bn - bp;
			} else {
				/* maybe partial beams on this side */
				if (numbeams(gnn_p->basictime) < bn &&
				pbeamside(gn_p, start1_p) == PB_LEFT)
					partial = bn - bp;
			}
			/* but if far enough away horizontally, we can ignore */
			if (gn_p->c[AW] - gs_p->c[AE] > 1.5 * Stepsize)
				partial = 0;
		}

		/* full beams joining prev and next, plus relevant partials */
		beams = MIN(bp, bn) + partial;

		/*
 		 * To get total beam thickness, multiply the size of one beam
		 * by the number of beams.
		 */
		beamthick = Flagsep * beams;

		/* find where outer beam hits our rest's X coord */
		ycross = b1 * gs_p->c[AX] + b0;

		/*
		 * Point at the coords we need to set.  For non-CSB, they are
		 * the relative ones, since we are called from beamstem.c.  For
		 * CSB, they are absolute, since we are called from absvert.c.
		 */
		if (start2_p == 0) {
			ng_p = &gs_p->c[RN];
			sg_p = &gs_p->c[RS];
			yr_p = &gs_p->restc[RY];
			nr_p = &gs_p->restc[RN];
			sr_p = &gs_p->restc[RS];
		} else {
			ng_p = &gs_p->c[AN];
			sg_p = &gs_p->c[AS];
			yr_p = &gs_p->restc[AY];
			nr_p = &gs_p->restc[AN];
			sr_p = &gs_p->restc[AS];
		}

		/* find vertical coords, quantizing the results */
		if (first_p->stemdir == UP) {
			/* move = where it should be minus where it was */
			move = nearestline(ycross - beamthick - asc - Stepsize)
				- *yr_p;
		} else {	/* stemdir == DOWN */
			/* move = where it should be minus where it was */
			move = nearestline(ycross + beamthick + des + Stepsize)
				- *yr_p;
		}

		/* move all coord except Y of the group, which remains 0 */
		*nr_p += move;
		*yr_p += move;
		*sr_p += move;
		*ng_p += move;
		*sg_p += move;
	}
}

/*
 * Name:        avoidothervoice()
 *
 * Abstract:    Change the Y intercept if necessary to avoid the other voice.
 *
 * Returns:     new y intercept value (may be no change)
 *
 * Description: When there is another voice, its groups might collide with our
 *		voice's beams, unless we lengthen our groups' stems.  This
 *		function returns the new Y intercept for the equation of the
 *		our voice's main beam, which accomplishes this.  When there is
 *		no other voice, or its groups don't interfere with our beam,
 *		this Y intercept will be the same as the old Y intercept.
 *		When it changes, embedded rests' coords need to be changed too.
 */

static double
avoidothervoice(start_p, last_p, b1, b0, ogs_p)

struct GRPSYL *start_p;	/* first group in nongrace beamed set */
struct GRPSYL *last_p;	/* last group in nongrace beamed set */
double b1;		/* slope */
double b0;		/* y intercept */
struct GRPSYL *ogs_p;	/* first group in the other voice */

{
	struct GRPSYL *prev_p;	/* point to nongrace group preceding gs_p */
	struct GRPSYL *prev2_p;	/* point to nongrace group before that one */
	struct GRPSYL *next_p;	/* point to nongrace group following gs_p */
	struct GRPSYL *next2_p;	/* point to nongrace group after that one */
	struct GRPSYL *gs_p;	/* point to group being looked at */
	float beamthick;	/* total thickness of beams and space between*/
	float ycross;		/* where grace stem would hit nongrace beam */
	float fary;		/* farthest y coord of other voice's group */
	int beams;		/* number of beams joining two nongrace groups*/
	float thismove;		/* how far one item requires the beam to move*/
	float move;		/* distance to move intercept */


	move = 0.0;		/* init to no move */

	/*
	 * Loop through all the groups in the other voice.  (If there is no
	 * other voice, this loop will execute zero times.)  If any of its
	 * groups land on or beyond our beam, move our beam farther away so
	 * they don't.
	 */
	for (gs_p = ogs_p; gs_p != 0; gs_p = gs_p->next) {

		/* spaces and rests can't interfere with anything */
		if (gs_p->grpcont != GC_NOTES)
			continue;

		/* if this group is outside our beamed set, ignore it */
		if (gs_p->c[AX] <= start_p->c[AX] ||
		    gs_p->c[AX] >=  last_p->c[AX])
			continue;

		/*
		 * Find which groups in our set immediately precede and follow
		 * the other voice's group.  These will be prev_p and next_p.
		 */
		for (prev_p = next_p = start_p;
		     next_p->c[AX] < gs_p->c[AX];
		     prev_p = next_p, next_p = nextnongracenonspace(next_p))
			;

		/*
		 * If next_p is lined up with gs_p, and is a note group, that
		 * means these groups were "compatible" (see setgrps.c), and so
		 * there can be no way that we would have to move our beam.
		 * But if next_p is a rest, handle the situation and continue.
		 */
		if (next_p->c[AX] == gs_p->c[AX]) {
			if (next_p->grpcont == GC_NOTES)
				continue;	/* compatible, no problem */

			/*
			 * Find the AX and RY coords of the outer edge of the
			 * outer note of the other voice's group that is the
			 * farthest in the direction of our beam.  Then, if
			 * this point would run into or beyond the rest, find
			 * how far to move the Y intercept (b0) so that it
			 * won't.  Remember the farthest move needed.
			 */
			if (start_p->stemdir == UP) {
				fary = gs_p->notelist[0].c[RN] + Stdpad;
				if (next_p->c[RS] < fary) {
					thismove = fary - next_p->c[RS];
					move = MAX(move, thismove);
				}
			} else { /* stemdir == DOWN */
				fary = gs_p->notelist[ gs_p->nnotes-1 ].c[RS]
						- Stdpad;
				if (next_p->c[RN] > fary) {
					thismove = fary - next_p->c[RN];
					move = MIN(move, thismove);
				}
			}

			continue;
		}

		/*
		 * Find which of prev_p and next_p has the least (slowest)
		 * basictime.  That determines how many full beams will connect
		 * those two groups.  (You take log2 of it and subtract 2.)
		 * Then add in any alternation beams.
		 */
		if (prev_p->basictime >= 8)
			beams = drmo(MIN(prev_p->basictime, next_p->basictime))
					- 2;
		else
			beams = 0;

		if (prev_p->slash_alt < 0)
			beams -= prev_p->slash_alt;

		/*
		 * Find out if there are partial beams on the left side of the
		 * following group or right side of the preceding group.  If
		 * so, that group's basictime may determine the total number of
		 * beams that could interfere with our group, if it's close
		 * enough.
		 */
		if (prev_p->basictime < next_p->basictime && next_p->stemdir ==
		    DOWN && next_p->c[AX] - gs_p->c[AX] < 5 * Stepsize) {

			/* find nongrace group after "next", if one exists */
			next2_p = nextnongrace(next_p);

			/* if "next" group has partial beams . . . */
			if (next2_p == 0 || next_p->beamloc == ENDITEM ||
				next_p->basictime > next2_p->basictime) {

				/* if on its left side, reset total beams */
				if (pbeamside(next_p, start_p) == PB_LEFT)
					beams = drmo(next_p->basictime) - 2;
			}
		} else if (prev_p->basictime > next_p->basictime && prev_p->
		stemdir == UP && gs_p->c[AX] - prev_p->c[AX] < 5 * Stepsize) {

			/* find nongrace group before "prev", if one exists */
			prev2_p = prevnongrace(prev_p);

			/* if "prev" group has partial beams . . . */
			if (prev2_p == 0 || prev_p->beamloc == STARTITEM ||
				prev_p->basictime > prev2_p->basictime) {

				/* if on its right side, reset total beams */
				if (pbeamside(prev_p, start_p) == PB_RIGHT)
					beams = drmo(prev_p->basictime) - 2;
			}
		}

		beamthick = Flagsep * beams + Stepsize;

		/*
		 * Find the AX and RY coords of the outer edge of the outer
		 * note of the other voice's group that is the farthest in the
		 * direction of our beam.  Then, if this point would run into
		 * or beyond the nongrace beam(s), find how much the Y
		 * intercept (b0) would have to move to avoid the collision.
		 * Remember the farthest move found so far.
		 */
		ycross = b1 * gs_p->c[AX] + b0;
		if (start_p->stemdir == UP) {

			fary = gs_p->notelist[0].c[RN] + Stdpad;
			if (ycross - beamthick < fary) {
				thismove = fary - (ycross - beamthick);
				move = MAX(move, thismove);
			}

		} else { /* stemdir == DOWN */

			fary = gs_p->notelist[ gs_p->nnotes-1 ].c[RS] - Stdpad;
			if (ycross + beamthick > fary) {
				thismove = fary - (ycross + beamthick);
				move = MIN(move, thismove);
			}
		}
	}

	if (move == 0.0)
		return (b0);		/* no change; return old intercept */

	/*
	 * If our beamed set has any embedded rests, we want to move the rests
	 * too.  We really only have to move rests that the other voice is
	 * bumping into, but it will probably look better to move them all.
	 * We need to move everything by a multiple of 2 stepsizes, since rests
	 * should be positioned that way.
	 */
	for (gs_p = start_p->next; gs_p != last_p; gs_p = gs_p->next) {
		/* break out if we find a rest */
		if (gs_p->grpcont == GC_REST)
			break;
	}
	if (gs_p != last_p) {
		/*
		 * We found a rest.  Round the amount the intercept moved up to
		 * a multiple of 2 stepsizes.
		 */
		move = (move < 0.0 ? -1.0 : 1.0) * 2.0 * Stepsize *
			((int)(fabs(move) / (2.0 * Stepsize)) + 1);

		/* move every embedded rest by this amount */
		for (gs_p = start_p->next; gs_p != last_p; gs_p = gs_p->next) {
			if (gs_p->grpcont == GC_REST) {
				gs_p->c[RN] += move;
				gs_p->c[RS] += move;
				gs_p->restc[RN] += move;
				gs_p->restc[RY] += move;
				gs_p->restc[RS] += move;
			}
		}
	}

	return (b0 + move);	/* new Y intercept */
}

/*
 * Name:        setgroupvert()
 *
 * Abstract:    Set RN and RS for each group of given type in a linked list.
 *
 * Returns:     void
 *
 * Description: This function loops through the linked list of groups for one
 *		voice for one measure.  It handles either grace groups or non-
 *		grace groups, whichever it is told to do.  It sets the RN and
 *		RS for the groups.
 */

static void
setgroupvert(grpvalue, firstgs_p, ogs_p)

int grpvalue;			/* should we do grace groups or normal groups?*/
struct GRPSYL *firstgs_p;	/* point to first group in a linked list */
struct GRPSYL *ogs_p;		/* point to first group in other linked list */

{
	struct GRPSYL *gs_p;	/* point along groups in a linked list */
	struct GRPSYL *start_p;	/* first in a beamed set */
	float outstem;	/* the part of the stemlen outside notes of group */
	float stemtip;	/* coord of the end of the stem */
	float old;		/* old group boundary */
	float delta;		/* change in group boundary */


	debug(32, "setgroupvert file=%s line=%d grpvalue=%d",
			firstgs_p->inputfile, firstgs_p->inputlineno, grpvalue);

	start_p = 0;		/* prevent "used before set" */

	/*
	 * Loop through every group, skipping rests, spaces, and groups of the
	 * wrong type (grace vs. nongrace), setting the relative vertical
	 * coordinates.
	 */
	for (gs_p = firstgs_p; gs_p != 0; gs_p = gs_p->next) {
		if (gs_p->grpcont != GC_NOTES)
			continue;
		if (gs_p->grpvalue != grpvalue)
			continue;

		/*
		 * Back in setnotes.c, we set RY to 0, the center line of the
		 * staff.  N was set to the top of the highest note, plus
		 * padding, excluding any CSS notes.  S is the analogous thing,
		 * below.  But if all notes are CSS, N and S were set to 0.
		 */

		/*
		 * Now we want to set the stemlen, as well as we can.  For
		 * groups whose step tips are not affected by CSS, we do it in
		 * the non-CSS pass; otherwise we do it in the CSS pass.
		 */
		if (css_affects_stemtip(gs_p) == CSSpass) {

			/*
			 * If the group has a stem or pseudostem, we do this
			 * work.  Extend the appropriate group boundary to
			 * reach to the end of the stem.  Do this for all
			 * groups with real stems or pseudostems, excluding
			 * cross staff beaming (where we don't know yet how
			 * long the stems will be and we don't want to include
			 * them in the group boundary anyway, since it would
			 * prevent stem overlapping that we want).  That means
			 * quad note and oct notes and
			 * half notes or shorter (excluding grace quarter
			 * notes), or anything with slash/alternations.
			 */
			if (gs_p->beamto == CS_SAME &&
			   (STEMMED(gs_p) || gs_p->slash_alt != 0) &&
			    gs_p->stemlen != 0.0) {

				outstem = gs_p->stemlen
					- (gs_p->notelist[0].c[RY]
					- gs_p->notelist[gs_p->nnotes-1].c[RY]);
				/*
				 * In the CSS pass we also have to adjust the
				 * absolute coords, by the same amount as the
				 * relative, since those have been set by now.
				 */
				if (gs_p->stemdir == UP) {
					stemtip = gs_p->notelist[0].c[RY]
						+ outstem;
					old = gs_p->c[RN];
					gs_p->c[RN] = MAX(stemtip, gs_p->c[RN])
						+ Stdpad;
					if (gs_p->beamloc != NOITEM) {
						/* half beam thickness */
						gs_p->c[RN] += W_WIDE * Stdpad
						/ 2.0 * (gs_p->grpsize ==
						GS_NORMAL ? 1.0 : SM_FACTOR);
					}
					if (CSSpass == YES) {
						delta = gs_p->c[RN] - old;
						gs_p->c[AN] += delta;
					}
				} else {
					stemtip = gs_p->notelist[gs_p->nnotes-1]
						.c[RY] - outstem;
					old = gs_p->c[RS];
					gs_p->c[RS] = MIN(stemtip, gs_p->c[RS])
						- Stdpad;
					if (gs_p->beamloc != NOITEM) {
						/* half beam thickness */
						gs_p->c[RS] -= W_WIDE * Stdpad
						/ 2.0 * (gs_p->grpsize ==
						GS_NORMAL ? 1.0 : SM_FACTOR);
					}
					if (CSSpass == YES) {
						delta = gs_p->c[RS] - old;
						gs_p->c[AS] += delta;

					}
				}
			}
		}

		if (CSSpass == NO) {
			/*
			 * Increase RN and decrease RS based on "with" lists.
			 * Do this only in the first pass.  This depends on the
			 * fact that "with" lists are always put on the side
			 * away from the other staff, when CSS is involved.
			 */
			applywith(gs_p);
		} else {
			/*
			 * In the CSS pass, various group boundaries need more
			 * adjustment.
			 */
			if (gs_p->stemdir == UP) {
				if (gs_p->stemto == CS_ABOVE && NNN(gs_p) == 0){
					gs_p->c[RS] = gs_p->notelist[
						gs_p->nnotes-1].c[RS] - Stdpad;
					gs_p->c[AS] += gs_p->c[RS];
				}
				if (gs_p->stemto == CS_BELOW && NNN(gs_p) == 0){
					gs_p->c[RN] = gs_p->notelist[
						gs_p->nnotes-1].c[RY] +
						gs_p->stemlen;
					applywith(gs_p);
					gs_p->c[AN] = gs_p->c[AY] + gs_p->c[RN];
				}
				if (gs_p->stemto == CS_SAME &&
						gs_p->stemlen > 0) {
					gs_p->c[RN] = gs_p->notelist
					[gs_p->nnotes-1].c[RY] + gs_p->stemlen
					+ Stdpad;

					gs_p->c[AN] = gs_p->notelist
					[gs_p->nnotes-1].c[AY] + gs_p->stemlen
					+ Stdpad;
				}
				if (gs_p->stemto == CS_ABOVE &&
						gs_p->stemlen == 0) {
					gs_p->c[RN] = gs_p->notelist[0].c[RN]
						+ Stdpad;
					gs_p->c[AN] = gs_p->notelist[0].c[AN]
						+ Stdpad;
				}
			} else {
				if (gs_p->stemto == CS_BELOW && NNN(gs_p) == 0){
					gs_p->c[RN] = gs_p->notelist[0].c[RN]
						+ Stdpad;
					gs_p->c[AN] += gs_p->c[RN];
				}
				if (gs_p->stemto == CS_ABOVE && NNN(gs_p) == 0){
					gs_p->c[RS] = gs_p->notelist[0].c[RY] -
						gs_p->stemlen;
					applywith(gs_p);
					gs_p->c[AS] = gs_p->c[AY] + gs_p->c[RS];
				}
				if (gs_p->stemto == CS_SAME &&
						gs_p->stemlen > 0) {
					gs_p->c[RS] = gs_p->notelist[0].c[RY]
						- gs_p->stemlen - Stdpad;

					gs_p->c[AS] = gs_p->notelist[0].c[AY]
						- gs_p->stemlen - Stdpad;
				}
				if (gs_p->stemto == CS_BELOW &&
						gs_p->stemlen == 0) {
					gs_p->c[RS] = gs_p->notelist
						[gs_p->nnotes-1].c[RS] - Stdpad;
					gs_p->c[AS] = gs_p->notelist
						[gs_p->nnotes-1].c[AS] - Stdpad;
				}
			}
		}
	}

	/*
	 * On the non-CSS pass, when dealing with nongrace, adjust the coords
	 * of rests when necessary.
	 */
	if (CSSpass == NO && grpvalue == GV_NORMAL) {
		/*
		 * Find each beamed set, and make the embedded rests reach
		 * the outer beam.  It'll prevent any stuff from getting in
		 * between the rests and the beam, plus it'll make applywith
		 * handle "with" on rests correctly, when we call it later.
		 */
		for (gs_p = firstgs_p; gs_p != 0; gs_p = gs_p->next) {
			if (gs_p->grpvalue == GV_NORMAL) {
				switch (gs_p->beamloc) {
				case STARTITEM:
					start_p = gs_p;
					break;
				case ENDITEM:
					restbeam(start_p, gs_p);
					break;
				}
			}
		}

		/* handle "with" groups on rests */
		for (gs_p = firstgs_p; gs_p != 0; gs_p = gs_p->next) {
			if (gs_p->grpcont == GC_REST) {
				applywith(gs_p);
			}
		}
	}
}

/*
 * Name:        restbeam()
 *
 * Abstract:    Make the group boundaries of embedded rests reach the beam.
 *
 * Returns:     void
 *
 * Description: Given the first and last groups of a beamed set, this function
 *		extends the group boundaries of embedded rests to reach the
 *		beam.
 */

static void
restbeam(start_p, last_p)

struct GRPSYL *start_p;		/* first group in the beamed set */
struct GRPSYL *last_p;		/* last group in the beamed set */

{
	struct GRPSYL *gs_p;	/* loop through beamed set */
	float starty, lasty;	/* Y value at end of stems */
	float b1;		/* slope */
	float b0;		/* Y intercept */
	int coord;		/* RN or RS */


	/* don't do anything for CSB, not worth the effort */
	if (start_p->beamto != CS_SAME) {
		return;
	}

	/*
	 * We need to know where the stem tip of the first and last groups are.
	 * The note groups already have "with" applied, so we can't use the
	 * group boundaries.  We need the relative Y, so we can't use
	 * find_y_stem().
	 */
	if (start_p->stemdir == UP) {
		coord = RN;
		starty = start_p->notelist[start_p->nnotes-1].c[RY] +
				start_p->stemlen;
		lasty = last_p->notelist[last_p->nnotes-1].c[RY] +
				last_p->stemlen;
	} else {
		coord = RS;
		starty = start_p->notelist[0].c[RY] - start_p->stemlen;
		lasty = last_p->notelist[0].c[RY] - last_p->stemlen;
	}

	/* find slope and Y intercept of the beam */
	b1 = (lasty - starty) / (last_p->c[AX] - start_p->c[AX]);
	b0 = starty - b1 * start_p->c[AX];

	/*
	 * Set coord of all embedded rests.  The Stdpad is an adjustment due to
	 * the thickness of the beam.  No need to look at the first and last
	 * group in the sets, because they can't be rests.
	 */
	for (gs_p = start_p->next; gs_p != last_p; gs_p = gs_p->next) {
		if (gs_p->grpcont == GC_REST) {
			gs_p->c[coord] = b1 * gs_p->c[AX] + b0 +
					(coord == RN ? Stdpad : -Stdpad);
		}
	}
}

/*
 * Name:        settuplet()
 *
 * Abstract:    Figure out where tuplet bracket goes and change RN and RS.
 *
 * Returns:     void
 *
 * Description: This function is given a pointer to the first GRPSYL in a
 *		tuplet whose bracket is to be printed.  It figures out where
 *		the tuplet bracket and number should go, and sets tupextend for
 *		all the groups, to show where the tuplet bracket would go.
 *		Even if the bracket ends up not getting printed, this is needed
 *		for placing the number.
 */

static void
settuplet(start_p, staff_p)

struct GRPSYL *start_p;		/* first group in the tuplet */
struct STAFF *staff_p;		/* staff the tuplet is on */

{
	struct GRPSYL *gs_p;	/* loop through the groups in the tuplet */
	struct GRPSYL *last_p;	/* point the last group in the tuplet */
	struct GRPSYL *end_p;	/* point beyond the last group in the tuplet */
	struct GRPSYL *startng_p;/* point at first note group in the tuplet */
	struct GRPSYL *lastng_p; /* point at last  note group in the tuplet */
	struct GRPSYL *startnrg_p;/* point at first note/rest group in tuplet */
	struct GRPSYL *lastnrg_p; /* point at last  note/rest group in tuplet */
	float b0, b1;		/* y intercept and slope */
	int css_affects_tup;	/* does CSS affect any group in the tuplet? */
	int coord;		/* RN or RS, depending on where bracket goes */
				/* or AN or AS if CSSpass == YES */
	int numng;		/* number of note groups in tuplet */
	int numnrg;		/* number of note/rest groups in tuplet */


	debug(32, "settuplet file=%s line=%d", start_p->inputfile,
			start_p->inputlineno);
	/*
	 * If start_p is pointing at a grace group that precedes the first real
	 * group of the tuplet, move start_p forward to the first real group.
	 * Actually, this shouldn't be necessary; the parser is doing it now.
	 */
	while (start_p->grpvalue == GV_ZERO)
		start_p = start_p->next;

	/*
	 * Find out which side the tuplet number (and bracket, if needed)
	 * should go on.  That determines which coord we pay attention to.
	 * The other determining factor is whether this is the CSS pass.
	 */
	if (tupdir(start_p, staff_p) == PL_ABOVE) {
		coord = CSSpass == YES ? AN : RN;
	} else {
		coord = CSSpass == YES ? AS : RS;
	}

	/* find whether CSS affects any group in the set */
	css_affects_tup = NO;
	if (CSSused == YES) {	/* don't waste time looking if CSS not used */
		for (gs_p = start_p; gs_p != 0 && ! (gs_p != start_p &&
					gs_p->prev->tuploc == ENDITEM);
					gs_p = gs_p->next) {
			if ((gs_p->stemto == CS_ABOVE &&
						(coord == AN || coord == AN)) ||
			    (gs_p->stemto == CS_BELOW &&
						(coord == AS || coord == AS))) {
				css_affects_tup = YES;
				break;
			}
		}
	}

	/*
	 * If no groups are affected by CSS, handle this tuplet on the
	 * first pass only.  If some are affected, handle it on the second
	 * pass only.
	 */
	if (css_affects_tup != CSSpass) {
		return;
	}

	/* find how many groups of different types, and first and last */
	(void)tupgroups(start_p, &numng, &numnrg, &startng_p, &lastng_p,
			&startnrg_p, &lastnrg_p, &last_p, &end_p);

	/* find slope & Y intercept, basing the slope on note and rest groups */
	trytuplet(NO, &b0, &b1, coord, numnrg, staff_p, startnrg_p, lastnrg_p,
			start_p, last_p, end_p, nextnonspace);

	/*
	 * If some groups have notes and some have rests, also find slope and
	 * Y intercept where slope is based only on the notes.  Then determine
	 * determine which way is "better".
	 */
	if (numng > 0 && numng != numnrg) {
		float nb0, nb1;		/* Y intercept & slope based on notes */

		/* find slope & Y intercept, basing the slope on note groups */
		trytuplet(NO, &nb0, &nb1, coord, numng, staff_p, startng_p,
				lastng_p, start_p, last_p, end_p, nextsamecont);

		/* if slopes have opposite sign, or one is 0, force horizontal*/
		if (b1 * nb1 <= 0.0) {
			trytuplet(YES, &b0, &b1, coord, numng, staff_p,
				startng_p, lastng_p, start_p, last_p, end_p,
				nextsamecont);
		} else if (fabs(nb1) < fabs(b1)) {
			/* else choose the one that is the most level */
			b0 = nb0;
			b1 = nb1;
		}
	}

	/* set tupextend for every group */
	for (gs_p = start_p; gs_p != end_p; gs_p = gs_p->next) {
		gs_p->tupextend = (b0 + b1 * gs_p->c[AX]) - gs_p->c[coord];
	}
}

/*
 * Name:        tupgroups()
 *
 * Abstract:    Count groups of different types in tuplet, and set pointers.
 *
 * Returns:     number of groups in tuplet, including grace groups
 *
 * Description: This function counts the note groups, note + rest groups, and
 *		total number of groups.  The total number include embedded
 *		grace groups.  If sets pointers to the first and last of each
 *		of these three categories, and a pointer to the first group
 *		beyond the end.  If there are none of some category, it sets
 *		null pointers for its first and last.  Since there is always
 *		at least one group (start_p), *last_p_p will always be nonnull.
 *		If we are at the end of a measure, end_p will be null.
 */

static int
tupgroups(start_p, numng_p, numnrg_p, startng_p_p, lastng_p_p,
		startnrg_p_p, lastnrg_p_p, last_p_p, end_p_p)

struct GRPSYL *start_p;		/* first group in the tuplet */
int *numng_p;			/* set to number of note groups */
int *numnrg_p;			/* set to number of note/rest groups */
struct GRPSYL **startng_p_p;	/* set to first note group in the tuplet */
struct GRPSYL **lastng_p_p;	/* set to last  note group in the tuplet */
struct GRPSYL **startnrg_p_p;	/* set to first note/rest group in the tuplet */
struct GRPSYL **lastnrg_p_p;	/* set to last  note/rest group in the tuplet */
struct GRPSYL **last_p_p;	/* set to last group in the tuplet */
struct GRPSYL **end_p_p;	/* set to after last group in the tuplet */

{
	struct GRPSYL *gs_p;	/* for looping */
	int num;		/* the answer, number of groups */


	/* start with zero counts and null pointers */
	*numng_p = *numnrg_p = num = 0;
	*startng_p_p = *lastng_p_p = *startnrg_p_p = *lastnrg_p_p = 0;
	*last_p_p = 0;

	for (gs_p = start_p; gs_p != 0; gs_p = gs_p->next) {
		if (gs_p->grpcont == GC_NOTES) {
			(*numng_p)++;
			*lastng_p_p = gs_p;	/* we'll remember the last */
			if (*startng_p_p == 0) {
				*startng_p_p = gs_p;
			}
		}
		if (gs_p->grpcont == GC_NOTES || gs_p->grpcont == GC_REST) {
			(*numnrg_p)++;
			*lastnrg_p_p = gs_p;	/* we'll remember the last */
			if (*startnrg_p_p == 0) {
				*startnrg_p_p = gs_p;
			}
		}
		num++;
		*last_p_p = gs_p;	/* we'll remember the last */

		/* break out after handling last item in tuplet */
		if (gs_p->grpvalue == GV_NORMAL && /* don't be fooled by grace*/
		   (gs_p->tuploc == ENDITEM || gs_p->tuploc == LONEITEM)) {
			gs_p = gs_p->next;	/* point after last */
			break;
		}
	}

	*end_p_p = gs_p;	/* after the last group, might be null */
	return (num);
}

/*
 * Name:        trytuplet()
 *
 * Abstract:    Propose slope and Y intercept for a tuplet bracket.
 *
 * Returns:     void
 *
 * Description: This function proposes a slope and Y intercept for a tuplet
 *		bracket.  It uses nextfunc_p to loop through just the groups
 *		that the caller wants it to look at.  But for some purposes
 *		it looks at all the groups.
 */

static void
trytuplet(forcehorz, b0_p, b1_p, coord, num, staff_p, start_p, last_p,
		brackstart_p, bracklast_p, end_p, nextfunc_p)

int forcehorz;			/* should we force the bracket horizontal? */
float *b0_p;			/* return the y intercept */
float *b1_p;			/* return the slope */
int coord;			/* RN or RS, depending on where bracket goes */
				/*   or AN or AS if CSSpass == YES */
int num;			/* how many groups to look at */
struct STAFF *staff_p;		/* the staff we are attached to */
struct GRPSYL *start_p;		/* point at first group to look at */
struct GRPSYL *last_p;		/* point at last group to look at */
struct GRPSYL *brackstart_p;	/* point at group where bracket starts */
struct GRPSYL *bracklast_p;	/* point at last where bracket ends */
struct GRPSYL *end_p;		/* point beyond the last group in the tuplet */
struct GRPSYL *(*nextfunc_p)();	/* call here to get the next relevant group */

{
	struct GRPSYL *gs_p;	/* loop through the groups in the tuplet */
	struct NOTE *note_p;	/* pointer to an outside note of a group */
	float sx, sy;		/* sum of x and y coords of north or south */
	float xbar, ybar;	/* average x and y coords of north or south */
	float top, bottom;	/* numerator & denominator for finding b1 */
	/* like sy, ybar, top, b1, but ignoring stems and using notes only */
	float sy_note, ybar_note, top_note, b1_note;
	float temp;		/* scratch variable */
	float startx, endx;	/* x coord of first and last north or south */
	float starty, endy;	/* y coord of first and last north or south */
	float b0, b1;		/* y intercept and slope */
	float maxb0, minb0;	/* max and min y intercepts */
	float shift;		/* x dist bracket reaches beyond end groups */
	float acceast, accwest;	/* horizontal coords of an accidental */
	float accx;		/* equals either acceast or accwest */
	float accvert;		/* north or south of an accidental */
	float asc, des, wid;	/* ascent, descent, and width of an acc */
	float bracky;		/* vert position of bracket at x = accx */
	float numeast, numwest;	/* horizontal coords of the tuplet number */
	float numvert;		/* vertical edge of number closest to staff */
	float height;		/* height of the tuplet number */
	float mindist;		/* min distance from center line to bracket */
	int slope_forced;	/* is the slope of the bracket forced? */
	int halfstaff;		/* half the height of staff, in stepsizes */
	float vert[2];		/* vertical coords of two groups */
	int n;			/* loop variable */


	slope_forced = NO;	/* default internal forcing to NO */

	if (forcehorz == YES || num <= 1) {
		/*
		 * Either the caller wants to force horizontal, or there's one
		 * group or none so only horizontal makes sense.
		 */
		b1 = 0.0;
		slope_forced = YES;
	} else if (fabs(start_p->tupletslope - NOTUPLETANGLE) >= 0.001) {
		/*
		 * The user is forcing the angle, so use that.  Note: this won't
		 * conflict with the above zero forcing because:  1) forcehorz
		 * is YES only when previous calls returned opposite signed
		 * slope or zero, and that can't happen if the user is forcing
		 * nonzero slope, and 2) for <= 1 group we must go with the 0
		 * regardless of the user.
		 */
		b1 = tan(brackstart_p->tupletslope * PI / 180.0);
		slope_forced = YES;
	} else if (tupconcave(&b1, coord, num, start_p, last_p, nextfunc_p)) {
		; /* try to use the b1 that was returned, but don't force it */
	} else {
		/*
		 * Use linear regression to find the best-fit line through the
		 * RN or RS, or AN or AS, of the groups, as the case may be.
		 * The X coords used are absolute, but the Y coords are, in the
		 * normal (non-CSSpass case) relative to the center line of the
		 * staff, since we don't know the absolute Y coords yet, and it
		 * wouldn't affect the result anyway.  But if this is the CSS
		 * pass, we do know the absolute vertical coords, and we have
		 * to use them, since we are dealing with two staffs.
		 * We also find the best-fit line using the outer note head
		 * (for note groups), ignoring the stems.  We use the slopes of
		 * the two lines to decide on a compromise slope to use.
		 *
	 	 * First get sum of x and y coords, to find averages.
		 */
		sx = sy = sy_note = 0;
		for (n = 0, gs_p = start_p; n < num;
				n++, gs_p = (*nextfunc_p)(gs_p)) {
			sx += gs_p->c[AX];
			sy += gs_p->c[coord];
			sy_note += outernote_y(gs_p, coord);
		}

		xbar = sx / num;
		ybar = sy / num;
		ybar_note = sy_note / num;

		/* accum numerator & denominator of regression formula for b1 */
		top = top_note = bottom = 0;
		for (n = 0, gs_p = start_p; n < num;
				n++, gs_p = (*nextfunc_p)(gs_p)) {
			temp = gs_p->c[AX] - xbar;
			top += temp * (gs_p->c[coord] - ybar);
			top_note += temp *
					(outernote_y(gs_p, coord) - ybar_note);
			bottom += temp * temp;
		}

		b1 = top / bottom;		/* slope */
		b1_note = top_note / bottom;	/* slope ignoring stems */

		if (b1 * b1_note <= 0.0) {
			/* signs are different, or one of them is zero */
			b1 = 0.0;	/* force horizontal */
			slope_forced = YES;
		} else {
			/* take the average of the slopes */
			b1 = (b1 + b1_note) / 2.0;
		}
		/*
		 * We could also figure the y intercept based on the original
		 * b1 or b1_note by an equation like  b0 = ybar - b1 * xbar
		 * to get the equation of the regression line:  y = b0 + b1 * x
		 * but we're going to change b0 later anyway.  Now, there are
		 * certain cases where we want to override the slope determined
		 * by regression, so revise b1 if that is the case.
		 */

		/* if first and last groups are equal, force horizontal */
		if (start_p->c[coord] == last_p->c[coord]) {
			b1 = 0.0;
			slope_forced = YES;
		}

		/* if repeating pattern of two coords, force horizontal */
		if (b1 != 0.0 && num >= 4 && num % 2 == 0) {
			vert[0] = start_p->c[coord];
			vert[1] = (*nextfunc_p)(start_p)->c[coord];
			for (n = 0, gs_p = start_p; n < num;
					n++, gs_p = (*nextfunc_p)(gs_p)) {
				if (n >= 2 && gs_p->c[coord] != vert[n % 2])
					break;
			}
			if (n == num) {
				b1 = 0.0;
				slope_forced = YES;
			}
		}
	}

	/*
	 * The end of the tuplet bracket reaches beyond the X coord of the
	 * outer groups, to where a stem would be if it started with a down
	 * stem group and ended with an up stem group.  Set the X positions for
	 * these ends.
	 */
	shift = getstemshift(brackstart_p);
	startx = brackstart_p->c[AX] - shift;	/* start of tuplet bracket */
	shift = getstemshift(bracklast_p);
	endx = bracklast_p->c[AX] + shift;	/* end of tuplet bracket */

	/*
	 * The original line derived by linear regression must be adjusted in
	 * certain ways, but only if it is not being forced.
	 * since that would look bad.
	 */
	if (slope_forced == NO) {
		b1 = adjslope(start_p, b1, NO, TUPLETSLOPE);
	}

	/*
	 * Calculate a new y intercept (b0).  First pass parallel lines
	 * through each group's extremity, and record the maximum and minimum
	 * y intercepts that result.  (We depend on spaces' Y coords being
	 * inside the staff, equal to the middle line actually.)
	 */
	b0 = brackstart_p->c[coord] - b1 * brackstart_p->c[AX];
	maxb0 = minb0 = b0;		/* init to value for first group */
	/* look at rest of them */
	for (gs_p = brackstart_p; gs_p != end_p; gs_p = gs_p->next) {
		b0 = gs_p->c[coord] - b1 * gs_p->c[AX];
		if (b0 > maxb0) {
			maxb0 = b0;
		} else if (b0 < minb0) {
			minb0 = b0;
		}
	}

	/*
	 * The outer edge of the tuplet bracket, including the number, should
	 * be TUPHEIGHT away from the group that sticks out the farthest.
	 */
	if (coord == RN || coord == AN) {
		b0 = maxb0 + Tupheight;
	} else {	/* RS or AS */ 
		b0 = minb0 - Tupheight;
	}

	/*
	 * Calculate the Y positions of the start and end of the bracket from
	 * the X positions, and the slope and Y intercept we have tentatively
	 * chosen.  If, however, the bracket is going to fall within the staff,
	 * make adjustments so it won't.  (Doesn't apply to CSS.)
	 * (And don't do this if no bracket is going to be printed.)
	 */
	starty = b0 + b1 * startx;	/* y coord near left end of bracket */
	endy = b0 + b1 * endx;		/* y coord near right end of bracket */

	halfstaff = svpath(staff_p->staffno, STAFFLINES)->stafflines == 5
			? 4 : 1;

	if (tupgetsbrack(brackstart_p)) {
		/* how close a bracket can come to the center staff line */
		mindist = halfstaff * Stepsize + Tupheight;

		if (slope_forced) {
			/* move both ends the same amount to preserve slope */
			if (coord == RN) {
				if (starty < mindist) {
					endy += mindist - starty;
					starty = mindist;
				}
				if (endy < mindist) {
					starty += mindist - endy;
					endy = mindist;
				}
			} else if (coord == RS) {
				if (starty > -mindist) {
					endy -= starty + mindist;
					starty = -mindist;
				}
				if (endy > -mindist) {
					starty -= endy + mindist;
					endy = -mindist;
				}
			}
		} else {
			if (coord == RN) {
				if (starty < mindist) {
					starty = mindist;
				}
				if (endy < mindist) {
					endy = mindist;
				}
			} else if (coord == RS) {
				if (starty > -mindist) {
					starty = -mindist;
				}
				if (endy > -mindist) {
					endy = -mindist;
				}
			}
		}
	}

	/*
	 * If y at the ends of the bracket only differs by less than 2 points,
	 * set end equal to the start to avoid a jagged look.
	 * But don't do it if the slope is forced.
	 */
	if (slope_forced == NO && endy - starty <  2 * POINT &&
				  endy - starty > -2 * POINT) {
		endy = (starty + endy) / 2.;
		starty = endy;
	}

	/* recalculate slope and y intercept from (possibly) new endpoints */
	b1 = (endy - starty) / (endx - startx);		/* slope */
	b0 = starty - b1 * startx;			/* y intercept */

	/*
	 * The vertical extension of accidentals is not included in group
	 * boundaries, and so the calculation of the tuplet bracket's equation
	 * has ignored them so far.  In general, this is no problem.  If an
	 * accidental touches or slightly crosses that line, who cares?  But we
	 * would like to keep it from running into the tuplet number.  So scan
	 * through the notes closest to the bracket, checking for accidentals.
	 * (Notes a step or more from there would never really be a problem.)
	 * Also, accidentals on the first group can never be a problem.
	 */
	(void)tupnumsize(brackstart_p, &numwest, &numeast, &height, staff_p);
	numvert = (starty + endy) / 2 + (coord == RN || coord == AN ?
			-height : height) / 2;

	for (gs_p = brackstart_p->next; gs_p != end_p; gs_p = gs_p->next) {

		if (gs_p->grpcont != GC_NOTES) {
			continue;
		}

		for (n = 0; n < gs_p->nnotes; n++) {
			note_p = &gs_p->notelist[n];
			if ( ! has_accs(note_p->acclist)) {
				continue;
			}

			accdimen(gs_p->staffno, note_p, &asc, &des, &wid);
			asc *= Staffscale;
			des *= Staffscale;
			wid *= Staffscale;

			accwest = gs_p->c[AX] + note_p->waccr;
			acceast = accwest + wid;

			if (coord == RN || coord == AN) {
				accvert = note_p->c[CSSpass == YES ? AY : RY]
						+ asc + Stepsize;
			} else {
				accvert = note_p->c[CSSpass == YES ? AY : RY]
						- des - Stepsize;
			}

			/* find x for side of acc(s) where bracket is */
			if ((b1 < 0.0) == (coord == RN || coord == AN)) {
				accx = acceast;
			} else {
				accx = accwest;
			}

			/* find vert position of bracket at x = accx */
			bracky = b1 * accx + b0;

			/* alter it to allow padding */
			bracky += coord == RN || coord == AN ?
					-Stepsize : Stepsize;

			/*
			 * If acc collides with bracket, move the y intercept,
			 * also which moves the tuple number.
			 */
			if (((coord == RN || coord == AN) && accvert > bracky)||
			    ((coord == RS || coord == AS) && accvert < bracky)){
				b0 += accvert - bracky;
				numvert += accvert - bracky;
			}

			/*
			 * If acc is completely to the left or right of the
			 * number, there can be no collision with it.
			 */
			if (acceast < numwest || accwest > numeast) {
				continue;
			}

			/*
			 * If acc sticks out beyond the edge of the number,
			 * change the y intercept by that amount to prevent it.
			 */
			if (((coord == RN || coord == AN) && accvert > numvert) ||
			    ((coord == RS || coord == AS) && accvert < numvert)) {
				b0 += accvert - numvert;
				numvert = accvert;
			}
		}
	}

	/* return the answers */
	*b0_p = b0;
	*b1_p = b1;
}

/*
 * Name:        outernote_y()
 *
 * Abstract:    Find the outer coord of the outside note.
 *
 * Returns:     the coord
 *
 * Description: This function, given any group and a coord type, finds the note
 *		on that side and returns the coord of that side of the note.
 *		But if there are no notes (rest, space, or mrpt), it just uses
 *		the group's boundary.
 */

static double
outernote_y(gs_p, coord)

struct GRPSYL *gs_p;		/* the group */
int coord;			/* RN or RS, depending on where bracket goes */
				/*   or AN or AS if CSSpass == YES */
{
	if (gs_p->nnotes < 1) {
		/* no notes: use the group boundary */
		return (gs_p->c[coord]);
	}

	if (coord == RN || coord == AN) {
		/* top of the top note */
		return (gs_p->notelist[0].c[coord]);
	} else {
		/* bottom of the bottom note */
		return (gs_p->notelist[gs_p->nnotes - 1].c[coord]);
	}
}

/*
 * Name:        tupconcave()
 *
 * Abstract:    Is tuplet "concave"?
 *
 * Returns:     YES or NO
 *
 * Description: This function decides whether the tuplet's inner groups are on
 *		a direct line between the outer groups or closer to the staff
 *		than that (versus being farther away).  It looks only at note
 *		groups, or both note and rest groups, based on nextfunc_p.
 *		It's the edge farthest from the staff that we are looking at,
 *		for each group.
 */

static int
tupconcave(b1_p, coord, num, start_p, last_p, nextfunc_p)

float *b1_p;			/* set to the slope */
int coord;			/* RN or RS, depending on where bracket goes */
				/*   or AN or AS if CSSpass == YES */
int num;			/* how many groups to look at */
struct GRPSYL *start_p;		/* point at first group to look at */
struct GRPSYL *last_p;		/* point at last group to look at */
struct GRPSYL *(*nextfunc_p)();	/* call here to get the next relevant group */

{
	struct GRPSYL *gs_p;	/* loop through the groups in the tuplet */
	float b0, b1;		/* slope & Y intercept */
	int n;			/* loop variable */


	/* find line through the first and last groups we are looking at */
	b1 = (last_p->c[coord] - start_p->c[coord]) /
		(last_p->c[AX] - start_p->c[AX]);
	b0 = start_p->c[coord] - b1 * start_p->c[AX];

	/* check every group we care about */
	for (n = 0, gs_p = start_p; n < num; n++, gs_p = (*nextfunc_p)(gs_p)) {
		if (coord == RN || coord == AN) {
			/* if above the line, not concave, but allow a fudge */
			/* factor so that roundoff error won't force "NO" */
			if (gs_p->c[coord] - 0.001 > b1 * gs_p->c[AX] + b0) {
				return (NO);
			}
		} else {	/* RS or AS */
			/* if below the line, not concave, but allow a fudge */
			/* factor so that roundoff error won't force "NO" */
			if (gs_p->c[coord] + 0.001 < b1 * gs_p->c[AX] + b0) {
				return (NO);
			}
		}
	}

	/* all groups are closer to the staff than the line */
	*b1_p = b1;
	return (YES);
}

/*
 * Name:        applywith()
 *
 * Abstract:    Expand vertical boundaries of group, based on "with" list.
 *
 * Returns:     void
 *
 * Description: This function adds to the RN coord of a group and/or subtracts
 *		from the RS coord, if a "with" list is present.
 */

static void
applywith(gs_p)

struct GRPSYL *gs_p;	/* the group to be worked on */

{
	/*
	 * If there is cross staff beaming and the "with" items are to be on
	 * the beam side, we can't do anything yet since we don't know yet
	 * where the beam will be.
	 */
	if (gs_p->beamto != CS_SAME &&
			((gs_p->stemdir == UP && has_with(gs_p, PL_ABOVE)) ||
			 (gs_p->stemdir == DOWN && has_with(gs_p, PL_BELOW)))) {
		return;
	}

	/*
	 * Get the height of the "with" items, and move the group's N and/or S
	 * coordinate to contain them.
	 */
	gs_p->c[RN] += withheight(gs_p, PL_ABOVE);
	gs_p->c[RS] -= withheight(gs_p, PL_BELOW);
}

/*
 * Name:        revise_restvert()
 *
 * Abstract:    Revise the vertical position of certain rests.
 *
 * Returns:     void
 *
 * Description: Now that we know the absolute horizontal positions of
 *		everything, and stem lengths, we can improve the vertical
 *		position of rests in a way that restsyl.c couldn't do.
 *		Basically, when there are multiple voices, in each voice the
 *		rests should try to line up with the notes in that voice, while
 *		still of course avoiding collisions with the other voices, if
 *		the user has requested this via the alignrests parameter.
 */

static void
revise_restvert()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	struct STAFF *staff_p;		/* the staff we are working on */
	struct MAINLL *mll_p;		/* for finding bar line */
	struct TIMEDSSV *firsttssv_p;	/* first timed SSV for a measure */
	struct TIMEDSSV *tssv_p;	/* point along a timed SSV list */
	struct GRPSYL *gs_p;		/* loop through a voice for a measure */
	RATIONAL offset;		/* current group's offset into meas */
	int vidx;			/* voice index */


	/*
	 * First, we loop through the whole song, setting the current value of
	 * the alignrests parameter in every group in voices 1 and 2.  Because
	 * of the way we have to search forward and backwards from each rest
	 * later, over multiple measures, it is simpler to store this value
	 * here on one pass, and then go through again and use it.
	 */

	initstructs();

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		switch (mainll_p->str) {
		case S_SSV:
			asgnssv(mainll_p->u.ssv_p);
			continue;
		case S_STAFF:
			/*
			 * Break out to handle staff.  Must do all staffs, even
			 * invisible, since they may harbor score level timed
			 * SSVs, which are relevant to the other staffs.
			 */
			staff_p = mainll_p->u.staff_p;
			break;
		default:
			continue;
		}

		/* find the bar line at the end of this measure */
		for (mll_p = mainll_p; mll_p != 0 && mll_p->str != S_BAR;
					mll_p = mll_p->next) {
			;
		}
		if (mll_p == 0) {
			pfatal("no bar line at end of measure");
		}

		/* point at the first timed SSV for this measure, if any */
		firsttssv_p = mll_p->u.bar_p->timedssv_p;

		/*
		 * Loop through the voices that exist.  Even though we won't
		 * move rests on voice 3, voice 3 could harbor timed SSVs that
		 * affect us.  And we do this even if there is only one voice
		 * now, because it could have timed SSVs and vscheme could
		 * change later.
		 */
		for (vidx = 0; vidx < MAXVOICES &&
					staff_p->groups_p[vidx] != 0; vidx++) {
			tssv_p = firsttssv_p;

			/* 
			 * Loop through the groups, applying any timed SSVs,
			 * and setting current value of alignrests.
			 */
			offset = Zero;  /* first group's offset into measure */
			for (gs_p = staff_p->groups_p[vidx]; gs_p != 0;
						gs_p = gs_p->next) {
				/*
				 * Apply timed SSVs up to and including the
				 * the current time.  Don't worry about grace
				 * groups; the are ignored in the lining up.
				 */
				while (tssv_p != 0 &&
						LE(tssv_p->time_off, offset)) {

					asgnssv(&tssv_p->ssv);
					tssv_p = tssv_p->next;
				}

				gs_p->alignrests = vvpath(gs_p->staffno,
					gs_p->vno, ALIGNRESTS)->alignrests;

				/* add our group's duration to the offset */
				offset = radd(offset, gs_p->fulltime);
			}

			/*
			 * If there were any timed SSVs, we need to undo their
			 * effects before we work on the next voice, if there
			 * is a next voice (not the final loop iteration).  If
			 * there isn't a next voice, we need to apply any
			 * remaining timed SSVs in preparation for the next
			 * measure.
			 */
			if (firsttssv_p != 0) {
				if (vidx + 1 < NORMVOICES &&
				    staff_p->groups_p[vidx + 1] != 0) {

					setssvstate(mainll_p);
				} else {
					while (tssv_p != 0) {
						asgnssv(&tssv_p->ssv);
						tssv_p = tssv_p->next;
					}
				}
			}
		}
	}


	/*
	 * Now loop through the song again, moving rests according to the
	 * parameter.
	 */

	initstructs();

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		switch (mainll_p->str) {
		case S_SSV:
			/* need this for a couple parms, but not alignrests, */
			/* so don't bother with timed SSVs */
			asgnssv(mainll_p->u.ssv_p);
			continue;
		case S_STAFF:
			staff_p = mainll_p->u.staff_p;
			if (staff_p->visible == NO) {
				continue;
			}
			/* break out to handle a visible staff */
			break;
		default:
			continue;
		}

		/*
		 * Do something only if more than one voice.  And for now, only
		 * if voice 3 doesn't exist or has all spaces; it could be
		 * enhanced to handle v3, but dealing with pvno gets
		 * complicated.
		 */
		if (svpath(staff_p->staffno, VSCHEME)->vscheme != V_1 &&
		    hasspace(staff_p->groups_p[3-1], Zero, Score.time)) {

			Stepsize = STEPSIZE * svpath(staff_p->staffno,
					STAFFSCALE)->staffscale;

			/* move voice 1 rests */
			rr_voicemeas(staff_p->groups_p[0],
					staff_p->groups_p[1], mainll_p);

			/* move voice 2 rests */
			rr_voicemeas(staff_p->groups_p[1],
					staff_p->groups_p[0], mainll_p);
		}
	}
}

/*
 * Name:        rr_voicemeas()
 *
 * Abstract:    Revise position of certain rests for one measure of one voice.
 *
 * Returns:     void
 *
 * Description: This is called by revise_restvert to do the job for one
 *		measure of one voice.
 */

static void
rr_voicemeas(this_p, other_p, mainll_p)

struct GRPSYL *this_p;		/* the voice to be worked on */
struct GRPSYL *other_p;		/* the voice not to collide with */
struct MAINLL *mainll_p;	/* the staff they are linked to */

{
	struct MAINLL *mll_p;		/* for loop through MLL */
	struct MAINLL *omll_p;		/* old value of mll_p */
	struct GRPSYL *gs_p;		/* point along our GRPSYL list */
	struct GRPSYL *gs2_p;		/* a group in the other voice */
	struct GRPSYL *prevgs_p;	/* another temp pointer */
	struct GRPSYL *nextgs_p;	/* another temp pointer */
	int pbars, nbars;		/* prev and next bar lines we crossed */
	int yc, nc, sc;			/* which Y, N, and S coord to use */
	float center_y;			/* the Y of the center line */
	float next_y, prev_y;		/* midpoint of next and prev groups */
	float slope;			/* slope of line between them */
	float rmid_y;			/* middle of the rest */
	float quant_y;			/* quantized Y position */
	RATIONAL resttime;		/* rest's time offset into measure */
	RATIONAL othertime;		/* time in the other voice */
	float floor, ceiling;		/* for containing the rest */


	if (CSSpass == YES) {
		/* in CSS pass, we're working with absolute vertical coords */
		yc = AY; nc = AN; sc = AS;
		center_y = mainll_p->u.staff_p->c[AY];
	} else {
		/* normal case, we're working with relative vertical coords */
		yc = RY; nc = RN; sc = RS;
		center_y = 0.0;
	}

	prev_y = next_y = 0.0;	/* prevent useless warning */

	/* loop through each rest in this voice, moving it if appropriate */
	for (gs_p = this_p; gs_p != 0; gs_p = gs_p->next) {

		/* if not a rest, nothing to do */
		if (gs_p->grpcont != GC_REST) {
			continue;
		}

		/* if the parameter says don't do it, don't do it */
		if (gs_p->alignrests == NO) {
			continue;
		}

		/* if user requested a position, don't change it */
		if (gs_p->restdist != NORESTDIST) {
			continue;
		}

		/* do not do embedded rests; they are where they should be */
		if (gs_p->beamloc == INITEM) {
			continue;
		}

		/* find time offset of our rest */
		resttime = Zero;
		for (gs2_p = gs_p->prev; gs2_p != 0; gs2_p = gs2_p->prev) {
			resttime = radd(resttime, gs2_p->fulltime);
		}


		/**********************************************************/
		/* line up the rest appropriately with notes in our voice */
		/**********************************************************/

		/*
		 * Find the last note preceding our rest on this score, where
		 * alignrests is still set, if any.
		 */
		omll_p = mll_p = mainll_p;
		prevgs_p = gs_p;
		pbars = 0;
		do {
			prevgs_p = prevgrpsyl(prevgs_p, &mll_p);
			if (mll_p != omll_p) {
				pbars++;	/* crossed another bar line */
				omll_p = mll_p;
			}
		} while (prevgs_p != 0 && prevgs_p->grpcont != GC_NOTES &&
					  prevgs_p->alignrests == YES);

		if (prevgs_p != 0) {
			if (prevgs_p->alignrests == NO) {
				/* no notes in this region where parm is YES */
				prevgs_p = 0;
			} else if (pbars > 0) {
				/* if we crossed a FEED, forget this group */
				for ( ; mll_p != mainll_p; mll_p = mll_p->next){
					if (mll_p->str == S_FEED) {
						prevgs_p = 0;
						break;
					}
				}
			}
		}

		/*
		 * Find the first nongrace note after our rest on this score,
		 * where alignrests is still set, if any.
		 */
		omll_p = mll_p = mainll_p;
		nbars = 0;
		nextgs_p = gs_p;
		do {
			nextgs_p = nextgrpsyl(nextgs_p, &mll_p);
			if (mll_p != omll_p) {
				nbars++;	/* crossed another bar line */
				omll_p = mll_p;
			}
		} while (nextgs_p != 0 && (nextgs_p->grpcont != GC_NOTES ||
					   nextgs_p->grpvalue == GV_ZERO) &&
					   nextgs_p->alignrests == YES);

		if (nextgs_p != 0) {
			if (nextgs_p->alignrests == NO) {
				/* no notes in this region where parm is YES */
				nextgs_p = 0;
			} else if (nbars > 0) {
				/* if we crossed a FEED, forget this group */
				for ( ; mll_p != mainll_p; mll_p = mll_p->prev){
					if (mll_p->str == S_FEED) {
						nextgs_p = 0;
						break;
					}
				}
			}
		}

		/* if no notes to line up with, do nothing */
		if (prevgs_p == 0 && nextgs_p == 0) {
			continue;
		}

		/* find midpoint (may not equal Y coord) of each note found */
		if (prevgs_p != 0) {
			prev_y = (prevgs_p->c[nc] + prevgs_p->c[sc]) / 2.0;
		}
		if (nextgs_p != 0) {
			next_y = (nextgs_p->c[nc] + nextgs_p->c[sc]) / 2.0;
		}

		/* set midpoint of rest */
		if (prevgs_p != 0 && nextgs_p == 0) {
			/* no next; line up rest with prev */
			rmid_y = prev_y;
		} else if (prevgs_p == 0 && nextgs_p != 0) {
			/* no prev; line up rest with next */
			rmid_y = next_y;
		} else {
			/* center rest on line joining the groups' centers */
			slope = (next_y - prev_y) /
				(nextgs_p->c[AX] - prevgs_p->c[AX]);
			rmid_y = prev_y + slope*(gs_p->c[AX] - prevgs_p->c[AX]);
		}

		/*
		 * From rmid_y, subtract the center line Y (in case we're
		 * dealing with absolute Y values), to be sure it's relative.
		 * Then quantize that position.  Add back the center line, so
		 * that quant_y will be absolute, if we're dealing with
		 * absolute positions.  Move the rest by the difference between
		 * where we want it and where it is.
		 */
		quant_y = nearestline(rmid_y - center_y) + center_y;
		moverestvert(gs_p, quant_y - gs_p->restc[yc], yc == RY);


		/**********************************************************/
		/* now move it again if need be, to avoid the other voice */
		/* and keep the Y coord from going past the center line   */
		/**********************************************************/

		/*
		 * Set prevgs_p to the group in the other voice that is just
		 * left of our rest, or 0 if there is none.   gs2_p will be
		 * either lined up with our rest (if the times are equal), or
		 * to the right (if the other voice has no group at the same
		 * time as the rest), or 0 (if the other voice has nothing
		 * after prevgs_p).
		 */
		othertime = Zero;
		prevgs_p = 0;
		for (gs2_p = other_p;
		     gs2_p != 0 && (LT(othertime, resttime) ||
		       (EQ(othertime, resttime) && gs2_p->grpvalue == GV_ZERO));
		     prevgs_p = gs2_p, gs2_p = gs2_p->next) {

			othertime = radd(othertime, gs2_p->fulltime);
		}

		/*
		 * Set a floor (to be used if our rest is voice 1) and a
		 * ceiling (to be used if our rest is voice 2).  When the other
		 * voice has a note, we want to keep out of its way.  When it
		 * has a rest, we want to keep away from the center line.
		 */
		floor = -1000.0;	/* way below any possible group */
		ceiling = 1000.0;	/* way above any possible group */
		if (prevgs_p != 0 && NE(othertime, resttime)) {
			/*
			 * The other voice has no group at this time, but does
			 * have a group before it; consider that group.
			 */
			if (prevgs_p->grpcont == GC_NOTES) {
				floor = prevgs_p->c[nc];
				ceiling = prevgs_p->c[sc];
			} else if (prevgs_p->grpcont == GC_REST) {
				floor = center_y;
				ceiling = center_y;
			}
		}
		if (gs2_p != 0) {
			/*
			 * Consider the group that is at this time, if there is
			 * one; and if there isn't, consider the group after,
			 * if there is one.
			 */
			if (gs2_p->grpcont == GC_NOTES) {
				floor = MAX(floor, gs2_p->c[nc]);
				ceiling = MIN(ceiling, gs2_p->c[sc]);
			} else if (gs2_p->grpcont == GC_REST) {
				floor = MAX(floor, center_y);
				ceiling = MIN(ceiling, center_y);
			}
		}

		/*
		 * Use the floor or ceiling, depending on voice, to force the
		 * rest far enough away.  Also make sure the center of a v1 rest
		 * doesn't go below the center line, and analogously for v2.
		 */
		if (gs_p->vno == 1) {
			while (gs_p->c[sc] < floor || gs_p->c[yc] < center_y) {
				moverestvert(gs_p, 2.0 * Stepsize, yc == RY);
			}
		} else {  /* vno == 2 */
			while (gs_p->c[nc] > ceiling || gs_p->c[yc] > center_y){
				moverestvert(gs_p, -2.0 * Stepsize, yc == RY);
			}
		}
	}
}

/*
 * Name:        moverestvert()
 *
 * Abstract:    Move the vertical coordinates of a rest.
 *
 * Returns:     void
 *
 * Description: This function alters the relative vertical coords of a rest,
 *		both the rest's group and the rest itself.  It can move either
 *		relative or absolute.
 */

static void
moverestvert(gs_p, move, relative)

struct GRPSYL *gs_p;		/* the group to be moved */
double move;			/* how far to move it */
int relative;			/* move the relative, not absolute, coords? */

{
	/* add offset to all, except the group's Y never changes */
	if (relative) {
		gs_p->c[RN] += move;
		gs_p->c[RS] += move;
		gs_p->restc[RN] += move;
		gs_p->restc[RY] += move;
		gs_p->restc[RS] += move;
	} else {
		gs_p->c[AN] += move;
		gs_p->c[AS] += move;
		gs_p->restc[AN] += move;
		gs_p->restc[AY] += move;
		gs_p->restc[AS] += move;
	}
}
