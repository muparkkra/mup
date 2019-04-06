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
 * Name:	trnspose.c
 *
 * Description:	This file contains functions for transposing to different keys.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

#define BAD	(99)		/* a bad interval */

/*
 * For each possible transposition, this table shows the change in the number
 * of sharps in whatever key we are in.  Invalid intervals are marked as BAD.
 */
static short Delshtab[5][8] = {
       /*	1	2	3	4	5	6	7 */

      { 0,	-7,	-12,	-10,	-8,	-6,	-11,	-9  },	/* d */
      { 0,	BAD,	-5,	-3,	BAD,	BAD,	-4,	-2  },	/* m */
      { 0,	0,	BAD,	BAD,	-1,	1,	BAD,	BAD },	/* P */
      { 0,	BAD,	2,	4,	BAD,	BAD,	3,	5   },	/* M */
      { 0,	7,	9,	11,	6,	8,	10,	12  },	/* A */
};

/* index this by an interval type to get a string naming it */
static char *Inttab[] =
		{ "diminished", "minor", "perfect", "major", "augmented" };

/*
 * The following hold the transposition information for the score and all the
 * staffs.  The arrays without the "ch_" are for transposing notes.  The ones
 * with "ch_" are for transposing chords.   For the latter, the score's info is
 * stored in inttype[0], intnum[0], and octint[0] (for chords above or below
 * "all").  Notes are always on a staff, so the [0] elements are not used in
 * the notes' arrays.  After every bar line these arrays are updated if the
 * transposition changed.  The values are the "total" transposition,
 * transpose + addtranspose.
 */
static int inttype[MAXSTAFFS+1];	/* interval type of simple interval */
static int intnum[MAXSTAFFS+1];		/* simple interval (>0, octs removed)*/
static int octint[MAXSTAFFS+1];		/* number of octaves in interval */

static int ch_inttype[MAXSTAFFS+1];	/* interval type of simple interval */
static int ch_intnum[MAXSTAFFS+1];	/* simple interval (>0, octs removed)*/
static int ch_octint[MAXSTAFFS+1];	/* number of octaves in interval */


static int std_acc_redefined P((void));
static void transnote P((struct GRPSYL *g_p, struct NOTE *n_p, int inttype,
		int intnum, int octint));
static void translurto P((struct GRPSYL *g_p, struct NOTE *n_p, int tnum,
		int toct));
static int orig_eff_key P((int staff));
static void simptrans P((int origtype, int orignum, int *inttype_p,
		int *intnum_p, int *octint_p));
static void fixslurto P((int s, struct MAINLL *mainll_p, int nintnum,
		int noctint));
static void useaccs_sm P((struct MAINLL *mainll_p));
static int group_note_has_acc P((struct GRPSYL *g_p, struct NOTE *note_p));

/*
 * Name:        transgroups()
 *
 * Abstract:    Transpose all GRPSYLs by the requested intervals.
 *
 * Returns:     void
 *
 * Description: This function loops through the main linked list, applying
 *		SSVs to keep the transpositions of the score and each staff
 *		up to date.  Whenever it hits a STAFF, it loops through all
 *		the GRPSYLs in the linked list(s) for the voice(s), changing
 *		all the affected information.  It also loops through all the
 *		chords, transposing them.
 */

void
transgroups()

{
	struct MAINLL *mainll_p;	/* point along main LL */
	struct GRPSYL *g_p;		/* point along LL of groups */
	struct STUFF *stuff_p;		/* point at stuff, looking for chords*/
	int tinttype, tintnum;		/* current total transposition */
	int ninttype, nintnum, noctint;	/* new transposition in standard form*/
	int v;				/* voice number */
	int s;				/* staff number */
	int n;				/* loop variable */
	int gotssv;			/* seen an SSV since the last STAFF? */
	int did_acc_warn;		/* did we output warning about accs? */
	int rtran;			/* rest "transposition" */


	debug(16, "transgroups");
	initstructs();			/* clean out old SSV info */

	/*
	 * Loop through the rest of the main linked list, applying SSVs to keep
	 * the tranposition info up to date, and processing linked lists of
	 * groups and STUFF.
	 */
	gotssv = YES;	/* force init of arrays at start even if no SSVs */
	did_acc_warn = NO;	/* we have not output warning about accs */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			asgnssv(mainll_p->u.ssv_p);
			gotssv = YES;
			break;

		case S_STAFF:
			if (gotssv == YES) {
				/*
				 * This is the first staff encountered after
				 * hitting SSV(s).  If the transposition has
				 * changed on any staff, update any slurto
				 * lists in the previous measure that go across
				 * this bar line, and reset the transposition
				 * tables in preparation to processing this
				 * staff and the rest of the staffs in this
				 * measure.
				 */
				for (s = 1; s <= Score.staffs; s++) {
					/*
					 * Convert this staff's new transpostion
					 * to standard form, storing it in
					 * local variables.
					 */
					/* YES: get tranposition for notes */
					totaltrans(YES, s, &tinttype, &tintnum);
					simptrans(tinttype, tintnum,
					     &ninttype, &nintnum, &noctint);

					/*
					 * If num or oct changed since last
					 * measure, we have to fix up slurto
					 * lists.  Interval type is irrelevant.
					 */
					if (nintnum != intnum[s] ||
					    noctint != octint[s])
						fixslurto(s, mainll_p,
							nintnum, noctint);

					/* store whether changed or not */
					inttype[s] = ninttype;
					intnum[s] = nintnum;
					octint[s] = noctint;

					/*
					 * If the user redefined any standard
					 * acces, and is transposing by other
					 * than octaves, the result will sound
					 * wrong in MIDI.  So check for this
					 * and warn, provided we have not
					 * already warned about it.
					 */
					if (Doing_MIDI == YES &&
					    did_acc_warn == NO &&
					    (inttype[s] != PERFECT ||
							intnum[s] != 1) &&
					    std_acc_redefined() == YES) {
						warning("some pitches may be wrong because you redefined pitch offsets of standard accidentals and are transposing by other than octaves");
						did_acc_warn = YES;
					}

					/* NO: get tranposition for chords */
					/* store whether changed or not */
					totaltrans(NO, s, &tinttype, &tintnum);
					simptrans(tinttype, tintnum,
						&ch_inttype[s], &ch_intnum[s],
						&ch_octint[s]);
				}

				/*
				 * Do the score.  Notes don't use this, only
				 * chords, so only do the chords.  And there
				 * are no slurtos to worry about.
				 */
				totaltrans(NO, 0, &tinttype, &tintnum);
				simptrans(tinttype, tintnum, &ch_inttype[0],
					&ch_intnum[0], &ch_octint[0]);

				gotssv = NO;
			}

			/* the staff we're supposed to work on */
			s = mainll_p->u.staff_p->staffno;

			/* loop through stuff list, transposing chords */
			/* (also translates accs to music chars) */
			for (stuff_p = mainll_p->u.staff_p->stuff_p;
					stuff_p != 0; stuff_p = stuff_p->next) {
				if (stuff_p->string != 0 &&
						stuff_p->modifier == TM_CHORD)
					stuff_p->string = tranchstr(stuff_p->
						string, stuff_p->all ? 0 : s);
			}

			/*
			 * If no transposition, don't waste time looping for
			 * the purpose of transposing notes.
			 */
			if (inttype[s] == PERFECT && intnum[s] == 1 &&
					octint[s] == 0) {
				continue;
			}

			/* never transpose tablature staff */
			if (is_tab_staff(s))
				continue;

			/* don't transpose notes if a normal clef is not to */
			/*  be printed */
			if (svpath(s, STAFFLINES)->printclef != SS_NORMAL)
				continue;

			/* loop through all voices that can exist */
			for (v = 0; v < MAXVOICES; v++) {
				/*
				 * Loop through the voice's list of GRPSYLs.
				 * If the voice doesn't exist, the loop will
				 * execute 0 times.
				 */
				for (g_p = mainll_p->u.staff_p->groups_p[v];
						g_p != 0; g_p = g_p->next) {

					if (g_p->grpcont == GC_NOTES) {
						for (n = 0; n < g_p->nnotes;
									n++) {
							transnote(g_p, &g_p->
								notelist[n],
								inttype[s],
								intnum[s],
								octint[s]);
						}
					} else if (g_p->grpcont == GC_REST &&
					  g_p->restdist != NORESTDIST) {
						/*
						 * The user hardcoded a rest's
						 * position, so "transpose" it.
						 * This is complicated by the
						 * fact that we normally want
						 * to force an even result so
						 * that it will look good.
						 */
						/* vertical stepsize shift */
						rtran = 7 * octint[s] +
								intnum[s] - 1;

						if (EVEN(g_p->restdist)) {
							g_p->restdist += rtran;
							/*
							 * Force even result,
							 * rounded away from
							 * center line.
							 */
							if (ODD(g_p->restdist)){
								g_p->restdist =
								g_p->restdist > 0 ?
								g_p->restdist + 1 :
								g_p->restdist - 1;
							}
						} else {
							/* no rounding */
							g_p->restdist += rtran;
							
							/* warn if odd result*/
							if (ODD(g_p->restdist)){
								l_warning(
								g_p->inputfile,
								g_p->inputlineno,
								"'dist' on rest is an odd number, which may look bad");
							}
						}
					}
				}
			}
			break;
		}
	}
}

/*
 * Name:        std_acc_redefined()
 *
 * Abstract:    Do any standard accidentals have their pitch redefined?
 *
 * Returns:	YES or NO
 *
 * Description: This function decides whether any standard accidentals have
 *		their pitch redefined due to the acctable parameter.
 */

static int
std_acc_redefined()
{
	struct ACCIDENTALS *acc_name_map_p;	/* info about an acc context */
	int idx;


	if (Score.acctable == 0) {
		return (NO);
	}

	/* search for an acc table with the right name */
	for (acc_name_map_p = Acc_contexts_list_p; acc_name_map_p != 0;
			acc_name_map_p = acc_name_map_p->next) {
		if (strcmp(acc_name_map_p->name, Score.acctable) == 0) {
			break;
		}
	}
	if (acc_name_map_p == 0) {
		pfatal("parameter acctable=%s but cannot find that accidentals context",
				Score.acctable);
	}

	/* check every acc in the table, looking for standard ones */
	for (idx = 0; idx < acc_name_map_p->size; idx++) {
		if (acc_name_map_p->info[idx].font == FONT_MUSIC) {
			switch (acc_name_map_p->info[idx].code) {
			case C_NAT:
			case C_SHARP:
			case C_FLAT:
			case C_DBLSHARP:
			case C_DBLFLAT:
				return (YES);
			}
		}
	}
	return (NO);
}

/*
 * Name:        transnote()
 *
 * Abstract:    transpose a note
 *
 * Returns:	void
 *
 * Description: This function alters a NOTE structure according to the given
 *		transposition.  This involves changing the note itself (letter,
 *		accidental, and octave) and any notes in its slurred-to list
 *		(letter and octave).
 */

static void
transnote(g_p, n_p, ttype, tnum, toct)

struct GRPSYL *g_p;	/* ptr to note's group, used only in error messages */
register struct NOTE *n_p; /* pointer to the note structure */
int ttype;		/* interval type (DIMINISHED, MINOR, . . .) */
int tnum;		/* simple interval (positive, with octaves removed) */
int toct;		/* number of octaves in interval */

{
	int oldaccnum;		/* old accidental number (&&=0, &=1, ...) */
	int newaccnum;		/* new accidental number (&&=0, &=1, ...) */
	int oldcircnum;		/* position of old note in circle of 5ths */
	int newcircnum;		/* position of new note in circle of 5ths */
	char newlet;		/* new note letter */
	char newacc;		/* new accidental letter */
	int newoct;		/* new octave number */
	int oldaccoffset;	/* old accidental offset, (&&=-2, &=-1, ...) */


	/*
	 * First do the note itself:  letter, accidental, octave.
	 */
	/* calculate new note letter from old */
	newlet = (n_p->letter - 'a' + tnum - 1) % 7 + 'a';

	/* don't use has_accs(): we need to check for all, even invisible */
	if (n_p->acclist[0] == 0) {
		newacc = '\0';	/* no acc before, so no acc now */
		/* set as if natural, for benefit of error messages later */
		oldaccnum = strchr(Acclets, 'n') - Acclets;
	} else {
		/* try to convert acclist to a single standard acc */
		oldaccoffset = accs_offset(n_p->acclist);
		if (oldaccoffset == BAD_ACCS_OFFSET) {
			l_ufatal(g_p->inputfile, g_p->inputlineno,
				"transposition is not allowed because note %c%d has nonstandard accidental(s), or multiple accidentals adding up to more than double sharp or flat",
				n_p->letter, n_p->octave);
		}

		/*
		 * There was an accidental, so we need to get the proper
		 * transposition of it.  Get position of the old note letter
		 * in the circle of 5ths, and the old accidental index.
		 * The index to the new note letter is shifted by delsh.
		 * If this falls outside the circle string, change the index
		 * and accidental until it lies within the string.
		 */
		oldcircnum = strchr(Circle, n_p->letter) - Circle;
		oldaccnum = oldaccoffset + 2;
		newaccnum = oldaccnum;
		newcircnum = oldcircnum + Delshtab [ ttype ] [ tnum ];
		while (newcircnum < 0) {
			newaccnum--;		/* one more flat */
			newcircnum += 7;	/* 7 letters "sharper" */
		}
		while (newcircnum >= 7) {
			newaccnum++;		/* one more sharp */
			newcircnum -= 7;	/* 7 letters "flatter" */
		}

		/* test for accidental overflow */
		if (newaccnum < 0 || newaccnum > 4) {
			l_ufatal(g_p->inputfile, g_p->inputlineno,
				"note %c%s%d is transposed to have triple sharp or flat",
				n_p->letter, Acctostr[oldaccnum], n_p->octave);
		}

		newacc = Acclets[newaccnum];
	}

	/*
	 * Calculate the new octave.  Add toct (number of octaves to
	 * transpose) to the old octave.  Then, add tnum to the old note
	 * number.  If it exceeds a 7th, wrap into the next octave.
	 */
	newoct = n_p->octave + toct;
	if (Letshift[n_p->letter - 'a'] + tnum - 1 >= 7)
		newoct++;

	/* check for octave overflow, and exit if so */
	if (newoct < MINOCTAVE || newoct > MAXOCTAVE) {
		l_ufatal(g_p->inputfile, g_p->inputlineno,
				"note %c%s%d octave is transposed out of range",
				n_p->letter, Acctostr[oldaccnum], n_p->octave);
	}

	/* store away the new values */
	n_p->letter = newlet;
	standard_to_acclist(newacc, n_p->acclist);
	n_p->octave = (short)newoct;


	/*
	 * Now do any notes in the slurred-to list, notes this note is slurred
	 * to:  letter, octave.  (There is never an accidental here.)
	 */
	translurto(g_p, n_p, tnum, toct);
}

/*
 * Name:        translurto()
 *
 * Abstract:    transpose a note's slurred-to list
 *
 * Returns:	void
 *
 * Description: This function is given a pointer to a note and a transposition.
 *		It transposes the note's slurto list.  Notice that the "type"
 *		of transposition interval is not needed, since these lists
 *		never contain accidentals.
 */

static void
translurto(g_p, n_p, tnum, toct)

struct GRPSYL *g_p;	/* note's group, used only in error messages */
struct NOTE *n_p;	/* note whose slurto list is to be transposed */
int tnum;		/* transposition interval number */
int toct;		/* transposition interval octave */

{
	int s;			/* index into slurto list */
	char newlet;		/* new note letter */
	int newoct;		/* new octave number */


	/* loop through each note (if any) in the slurred-to list */
	for (s = 0; s < n_p->nslurto; s++) {

		/* if this is a slur to or from nowhere, don't change it */
		if (IS_NOWHERE(n_p->slurtolist[s].octave))
			continue;

		if (n_p->slurtolist[s].letter == 'U') {
			/*
			 * This is an empty <> whose target pitch has not yet
			 * been determined, so we can't transpose now.
			 * Later, when savetieinfo() finds it, it will have
			 * already have been transposed.
			 */
			continue;
		}

		/* calculate new note letter from old */
		newlet = (n_p->slurtolist[s].letter - 'a' + tnum - 1) % 7 + 'a';

		newoct = n_p->slurtolist[s].octave + toct;
		if (Letshift[n_p->slurtolist[s].letter - 'a'] + tnum - 1 >= 7)
			newoct++;

		/* check for octave overflow, and exit if so */
		if (newoct < MINOCTAVE || newoct > MAXOCTAVE) {
			l_ufatal(g_p->inputfile, g_p->inputlineno,
					"note in slurred-to list transposed to out of range octave (%c%d)",
					newlet, newoct);
		}

		/* store away the new values */
		n_p->slurtolist[s].letter = newlet;
		n_p->slurtolist[s].octave = (short)newoct;
	}
}

/*
 * Name:        tranchnote()
 *
 * Abstract:    transpose a note name that's inside a chord symbol
 *
 * Returns:	void
 *
 * Description: This function is given a letter and accidental that occur
 *		inside a chord symbol.  It could be the main chord name itself,
 *		or the name of a note, like the E in "C/E" or "DaddE".  It
 * 		works for both capital and small letters.  It returns a pointer
 *		to a static area containing the transposed string.
 */

char *
tranchnote(letter, acc, s)

int letter;		/* A to G */
int acc;		/* one of:  'x', '#', '\0', '&', 'B' */
int s;			/* staff number, needed to get tranposition interval */
			/*  0 means the score as a whole ("all") */

{
	static char circle[] = "FCGDAEB";	/* circle of 5ths */
	static char newchord[3];	/* put transposed result here */

	int capital;		/* is the given letter a capital letter? */
	char origacc;		/* original accidental */
	int oldaccnum;		/* old accidental number (&&=0, &=1, ...) */
	int newaccnum;		/* new accidental number (&&=0, &=1, ...) */
	int oldcircnum;		/* position of old note in circle of 5ths */
	int newcircnum;		/* position of new note in circle of 5ths */
	char newlet;		/* new note letter */
	char newacc;		/* new accidental letter */


	debug(32, "tranchnote letter=%c acc=%c s=%d", letter,
			acc==0 ? ' ' : acc, s);
	/* need to translate naturals so that strchr can use Acclets[] */
	origacc = acc;
	if (acc == '\0')
		acc = 'n';

	/* convert to capital letter if it isn't, and remember that */
	if (letter >= 'A' && letter <= 'G') {
		capital = YES;
	} else {
		capital = NO;
		letter += 'A' - 'a';
	}

	/* calculate new note letter from old */
	newlet = (letter - 'A' + ch_intnum[s] - 1) % 7 + 'A';

	if (capital == NO) {
		/* original letter was lower case, so convert this too */
		newlet += 'a' - 'A';
	}

	/*
	 * Get the proper transposition of the accidental.  Get position of the
	 * old note letter in the circle of 5ths, and the old accidental index.
	 * The index to the new note letter is shifted by delsh.  If this falls
	 * outside the circle string, change the index and accidental until it
	 * lies within the string.
	 */
	oldcircnum = strchr(circle, letter) - circle;
	oldaccnum = strchr(Acclets, acc) - Acclets;
	newaccnum = oldaccnum;
	newcircnum = oldcircnum + Delshtab [ ch_inttype[s] ] [ ch_intnum[s] ];
	while (newcircnum < 0) {
		newaccnum--;		/* one more flat */
		newcircnum += 7;	/* 7 letters "sharper" */
	}
	while (newcircnum >= 7) {
		newaccnum++;		/* one more sharp */
		newcircnum -= 7;	/* 7 letters "flatter" */
	}

	/* test for accidental overflow */
	if (newaccnum < 0 || newaccnum > 4)
		ufatal("chord note %c%s is transposed to have triple sharp or flat",
			letter, Acctostr[oldaccnum]);

	newacc = Acclets[newaccnum];

	/* store away the new values */
	newchord[0] = newlet;
	newchord[1] = (newacc == 'n' && origacc != 'n' ? '\0' : newacc);
	newchord[2] = '\0';

	return (newchord);
}

/*
 * Name:        eff_key()
 *
 * Abstract:    Return the "effective key" (the key after any transposition).
 *
 * Returns:     the number of sharps in the effective key (flats are negative)
 *
 * Description: This function, given a staff number, returns the number of
 *		sharps currently in effect, considering any transpostion that
 *		may have been requested, and whether the key sig is turned off
 *		because of useaccs, and whether this is some other kind of
 *		staff that has no key.  If given 0 for the staff number, it
 *		does this for the score's key signature.  It assumes the SSVs
 *		are up to date.
 *		does this for the score's key signature.  It assumes the SSVs
 *		are up to date.
 */

int
eff_key(staff)

int staff;			/* staff number to do it for (0 = score) */

{
	if (staff == 0) {
		if (Score.useaccs != UA_N)  {
			return (0);
		}
	} else {
		if (svpath(staff, USEACCS)->useaccs != UA_N) {
			return (0);
		}
	}

	return (orig_eff_key(staff));
}

/*
 * Name:        orig_eff_key()
 *
 * Abstract:    Return the "effective key", ignoring useaccs.
 *
 * Returns:     the number of sharps in the effective key (flats are negative)
 *
 * Description: This function is like eff_key(), but it skips the check for
 *		useaccs.
 */

static int
orig_eff_key(staff)

int staff;			/* staff number to do it for (0 = score) */

{
	int sharps;		/* sharps in old key (flats count negative) */
	int origtype;		/* original transposition interval type */
	int orignum;		/* original transposition interval number */
	int inttype;		/* interval type of simple interval */
	int intnum;		/* simple interval (positive, octs removed) */
	int octint;		/* number of octaves in interval */
	int newsharps;		/* sharps in key after transposition */


	/*
	 * If no normal clef is to be printed, always treat it like there is
	 * no key signature.
	 */
	if (staff == 0) {
		if (Score.printclef != SS_NORMAL)
			return (0);
	} else {
		if (svpath(staff, STAFFLINES)->printclef != SS_NORMAL)
			return (0);
	}

	/* viewpath to get this staff's current key and transposition */
	if (staff == 0) {
		sharps = Score.sharps;
	} else {
		sharps = svpath(staff, SHARPS)->sharps;
	}
	/* YES: use the notes' transposition values, not the chords' */
	totaltrans(YES, staff, &origtype, &orignum);

	simptrans(origtype, orignum, &inttype, &intnum, &octint);

	/*
	 * Change number of sharps by the appropriate delta.  We assume the
	 * interval isn't BAD, because the parser wouldn't have allowed it.
	 */
	newsharps = sharps + Delshtab [ inttype ] [ intnum ];

	/* make sure the resulting key is valid */
	if (newsharps < -7 || newsharps > 7) {
		/*
		 * Normally we take the final "else" here.  But for the "1"
		 * interval there is an ambiguity.  If transpose + addtranspose
		 * add up to an aug or dim 1, there are two ways to state the
		 * result.  (Also for per 1, but in that case, we'd never have
		 * an invalid key.)  So we state both ways of looking at it.
		 */
		if ((orignum == 1  && origtype == AUGMENTED) ||
		    (orignum == -1 && origtype == DIMINISHED)) {
			ufatal("staff %d: key of %d %s transposed up by augmented 1 or down diminished 1 results in %d %s",
				staff,
				abs(sharps),
				(sharps >= 0 ? "sharps" : "flats"),
				abs(newsharps),
				(newsharps >= 0 ? "sharps" : "flats"));
		} else if ((orignum == -1 && origtype == AUGMENTED) ||
			   (orignum == 1  && origtype == DIMINISHED)) {
			ufatal("staff %d: key of %d %s transposed down by augmented 1 or up diminished 1 results in %d %s",
				staff,
				abs(sharps),
				(sharps >= 0 ? "sharps" : "flats"),
				abs(newsharps),
				(newsharps >= 0 ? "sharps" : "flats"));
		} else {
			ufatal("staff %d: key of %d %s transposed %s by %s %d results in %d %s",
				staff,
				abs(sharps),
				(sharps >= 0 ? "sharps" : "flats"),
				(orignum > 0 ? "up" : "down"),
				Inttab[origtype],
				abs(orignum),
				abs(newsharps),
				(newsharps >= 0 ? "sharps" : "flats"));
		}
	}
	return (newsharps);
}

/*
 * Name:        simptrans()
 *
 * Abstract:    Simplify a transpostion into standard form.
 *
 * Returns:     void
 *
 * Description: This function, given a transposition, converts it into
 *		standard form (a "simple" upwards interval and an octave).
 */

static void
simptrans(origtype, orignum, inttype_p, intnum_p, octint_p)

int origtype;		/* original transposition interval type */
int orignum;		/* original transposition interval number */
int *inttype_p;		/* interval type (DIMINISHED, MINOR, . . .) */
int *intnum_p;		/* simple interval (positive, with octaves removed) */
int *octint_p;		/* number of octaves in interval */

{
	int direction;		/* UP or DOWN */


	*inttype_p = origtype;
	*intnum_p = orignum;

	/* set direction; if down, make intnum positive */
	if (*intnum_p > 0) {
		direction = UP;
	} else {
		direction = DOWN;
		*intnum_p = -*intnum_p;
	}

	/* break interval into octaves plus a simple interval */
	*octint_p = (*intnum_p - 1) / 7;
	*intnum_p -= 7 * *octint_p;

	/* if downwards, adjust so that *intnum_p is upwards */
	if (direction == DOWN) {
		if (*intnum_p == 1) {
			/* for unison, negate octaves and reverse intvl type */
			*octint_p = -*octint_p;
			*inttype_p = 4 - *inttype_p;
		} else {
			/* for other intervals, octave becomes one less than */
			/* negation, and *intnum_p flips as does its type */
			*octint_p = -1 - *octint_p;
			*intnum_p = 9 - *intnum_p;
			*inttype_p = 4 - *inttype_p;
		}
	}
}

/*
 * Name:        fixslurto()
 *
 * Abstract:    Fix transposition of notes in a slurred-to list.
 *
 * Returns:     void
 *
 * Description: Notes in a slurred-to list are initially transposed the same as
 *		the regular notes in that measure.  But if they occur in a
 *		group immediately before a bar line where the transposition
 *		changes, they should have been transposed according to the new
 *		transposition.  This function is called when entering the new
 *		measure.  It searches back and finds any such slurred-to lists
 *		in the previous measure and fixes their transposition.
 */

static void
fixslurto(s, mainll_p, nintnum, noctint)

int s;				/* staff number */
struct MAINLL *mainll_p;	/* initially points at current staff */
int nintnum;			/* interval number of new transposition */
int noctint;			/* octaves in new transposition */

{
	struct GRPSYL *g_p;	/* point to a group */
	int deltanum, deltaoct;	/* change in transposition */
	int v;			/* voice number */
	int n;			/* loop index */


	/* search back to the last staff of the preceding measure, if any */
	for (mainll_p = mainll_p->prev; mainll_p != 0 &&
			mainll_p->str != S_STAFF; mainll_p = mainll_p->prev)
		;
	if (mainll_p == 0)
		return;	/* no preceding measure; nothing to do */

	/* search back to the matching staff in that measure, if any */
	while (mainll_p != 0 && mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno != s)
		mainll_p = mainll_p->prev;
	if (mainll_p == 0 || mainll_p->str != S_STAFF)
		return;		/* no matching staff (no. of staffs changed) */
	/* we found a matching staff in the preceding measure */

	/*
	 * "Subtract" the old transposition from the new one, to find the
	 * "delta" transposition.  This is what we need to apply to the
	 * slurred-to notes to change them from the old to new transposition.
	 */
	deltanum = nintnum - intnum[s] + 1;
	deltaoct = noctint - octint[s];
	if (deltanum < 1) {
		deltanum += 7;
		deltaoct--;
	}

	/*
	 * mainll_p now points to the matching staff in the preceding measure.
	 * Loop through all voices that can exist.
	 */
	for (v = 0; v < MAXVOICES; v++) {
		g_p = mainll_p->u.staff_p->groups_p[v];
		if (g_p == 0)
			continue;

		/* find the last grpsyl in this voice */
		while (g_p->next != 0)
			g_p = g_p->next;

		/* if it doesn't contain notes, there is nothing to do */
		if (g_p->grpcont != GC_NOTES)
			continue;

		/* found a group with notes at end of measure; process it */
		for (n = 0; n < g_p->nnotes; n++) {
			translurto(g_p, &g_p->notelist[n], deltanum, deltaoct);
		}
	}
}

/*
 * Name:        totaltrans()
 *
 * Abstract:    Find the current total transposition of a staff or the score.
 *
 * Returns:     void
 *
 * Description: This function is given a staff number, or zero for the score.
 *		It assumes that the SSVs are up to date.  It gets the two
 *		tranposition parameters, and adds them to get the current
 *		total transpostion.  If it's invalid, it does a ufatal.
 */

void
totaltrans(do_notes, s, type_p, num_p)

int do_notes;			/* transpose for notes (YES) or chords (NO) */
int s;				/* staff number, or 0 for score */
int *type_p;			/* return type of resulting transposition */
int *num_p;			/* return number of resulting transposition */

{
	/*
	 * inths is to be indexed by interval type and number (1 through 7).
	 * It gives the number of half steps in the interval.
	 */
	static short inths[5][8] = {
            /*	1	2	3	4	5	6	7 */

	   { 0,	-1,	0,	2,	4,	6,	7,	9    },	/* d */
	   { 0,	BAD,	1,	3,	BAD,	BAD,	8,	10   },	/* m */
	   { 0,	0,	BAD,	BAD,	5,	7,	BAD,	BAD  },	/* P */
	   { 0,	BAD,	2,	4,	BAD,	BAD,	9,	11   },	/* M */
	   { 0,	1,	3,	5,	6,	8,	10,	12   },	/* A */
	};

	struct SSV *ssv_p;	/* point at the SSV hold a transposition */
	int type[2];		/* interval types (DIMINISHED, MINOR, . . .) */
	int num[2];		/* interval numbers (down is negative) */
	int totalhs;		/* total half steps in resulting interval */
	char place[15];		/* temp storage for error message use */
	int offset;		/* like interval no. but counting from 0 */
	int n;			/* loop variable */


	/*
	 * Get the value of the transposition.  If we are working on notes,
	 * the transposition is used only if it is supposed to apply to notes,
	 * otherwise "up perfect 1".  Similarly for chords.
	 */
	ssv_p = svpath(s, TRANSPOSITION);
	if ((do_notes == YES && (ssv_p->trans_usage & TR_NOTES)  != 0) ||
	    (do_notes == NO  && (ssv_p->trans_usage & TR_CHORDS) != 0)) {
		type[0] = ssv_p->inttype;
		num[0] = ssv_p->intnum;
	} else {
		type[0] = PERFECT;
		num[0] = 1;
	}

	/* same as above, but for addtranspose */
	ssv_p = svpath(s, ADDTRANSPOSITION);
	if ((do_notes == YES && (ssv_p->addtrans_usage & TR_NOTES)  != 0) ||
	    (do_notes == NO  && (ssv_p->addtrans_usage & TR_CHORDS) != 0)) {
		type[1] = ssv_p->addinttype;
		num[1] = ssv_p->addintnum;
	} else {
		type[1] = PERFECT;
		num[1] = 1;
	}

	/*
	 * To get the interval number of the num of the transpostions, we
	 * basically add the two.  But musicians unfortunately start counting
	 * intervals from 1 instead of 0, so we have to play some games.
	 */
	offset = (num[0] > 0 ? num[0] - 1 : num[0] + 1) + /* add true offsets*/
		 (num[1] > 0 ? num[1] - 1 : num[1] + 1);
	if (offset >= 0) {		/* get interval number from offset */
		*num_p = offset + 1;
	} else {
		*num_p = offset - 1;
	}

	/* accumulate total half steps in both transpositions */
	totalhs = 0;
	for (n = 0; n < NUMELEM(num); n++) {
		if (num[n] > 0) {		/* interval is up */
			while (num[n] > 7) {
				num[n] -= 7;	/* subtract an octave */
				totalhs += 12;	/* add 12 half steps */
			}
			/* account for this simple interval */
			totalhs += inths[type[n]][num[n]];
		} else {			/* interval is down */
			while (num[n] < -7) {
				num[n] += 7;	/* add an octave */
				totalhs -= 12;	/* subtract 12 half steps */
			}
			/* account for this simple interval */
			totalhs -= inths[type[n]][abs(num[n])];
		}
	}

	/* if interval is down, find the up version of offset and halfsteps */
	if (offset < 0) {
		offset = -offset;
		totalhs = -totalhs;
	}
	/* bring it into the range of a simple interval */
	totalhs -= (offset / 7) * 12;
	offset %= 7;

	/* make sure this simple interval is valid */
	if (totalhs < inths[DIMINISHED][offset + 1] ||
	    totalhs > inths[AUGMENTED ][offset + 1]) {

		if (s == 0) {
			(void)sprintf(place, "score");
		} else {
			(void)sprintf(place, "staff %d", s);
		}
		ufatal("on %s, 'transpose' %s %s %d and 'addtranspose' %s %s %d add up to an invalid interval",
		place,
		num[0] > 0 ? "up" : "down", Inttab[type[0]], abs(num[0]),
		num[1] > 0 ? "up" : "down", Inttab[type[1]], abs(num[1]));
	}

	/* search table for the type of interval this is; it will be found */
	for (n = DIMINISHED; n <= AUGMENTED; n++) {
		if (totalhs == inths[n][offset + 1]) {
			break;
		}
	}

	*type_p = n;
}

/*
 * Name:        apply_useaccs()
 *
 * Abstract:    Apply the useaccs parameter.
 *
 * Returns:	void
 *
 * Description: This function goes through the main linked list, applying the
 *		useaccs parameter.  That means it may put accidentals on notes.
 */

void
apply_useaccs()

{
	struct MAINLL *mainll_p;	/* point along main LL */
	int s;				/* staff number */


	initstructs();			/* clean out old SSV info */

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			asgnssv(mainll_p->u.ssv_p);
			break;

		case S_STAFF:
			/* the staff we're supposed to work on */
			s = mainll_p->u.staff_p->staffno;

			/* break if the useaccs parm says to do nothing */
			if (svpath(s, USEACCS)->useaccs == UA_N) {
				break;
			}

			/* no key sig on tab staff */
			if (is_tab_staff(s)) {
				break;
			}

			/* no key sig if normal clefs are not to be printed */
			if (svpath(s, STAFFLINES)->printclef != SS_NORMAL) {
				break;
			}

			/* ignore invisible staffs */
			if (mainll_p->u.staff_p->visible == NO) {
				break;
			}

			/* process this staff */
			useaccs_sm(mainll_p);
			break;
		}
	}
}

/*
 * Name:        useaccs_sm()
 *
 * Abstract:    Apply the useaccs parameter for one staff, one measure.
 *
 * Returns:	void
 *
 * Description: This function applies the useaccs parameter for one measure of
 *		one staff.
 */

static void
useaccs_sm(mainll_p)

struct MAINLL *mainll_p;	/* main LL struct STAFF hangs off of */

{
	/* current accidentals for letter and octave */
	char curacc[7][MAXOCTAVE + 1][MAX_ACCS * 2];/* assumes MINOCTAVE == 0 */

	char *curacc_p;		/* point at a member of curacc */
	struct GRPSYL *curg_p[MAXVOICES]; /* point along the voices' lists */
	struct GRPSYL *g_p;	/* point at group to be worked on */
	struct GRPSYL *g2_p;	/* point at another group at that time */
	RATIONAL vtime[MAXVOICES]; /*time elapsed in each voice */
	struct STAFF *staff_p;	/* the staff structure itself */
	struct GRPSYL *prevg_p;	/* pointer to preceding GRPSYL */
	struct MAINLL *mll_p;	/* copy of mainll_p */
	struct NOTE *note_p;	/* pointer to a NOTE structure */
	RATIONAL mintime;	/* minimum time */
	int vidx;		/* voice index, starts at 0 */
	int savevidx;		/* vidx of the group to work on */
	int s;			/* staff number */
	int allow_acc;		/* should an acc be allowed on this note? */
	int useaccs;		/* current value */
	int carryaccs;		/* current value */
	int sharps;		/* of the current effective key */
	int circnum;		/* where in the circle of 5ths the note is */
	char pitchacc[MAX_ACCS * 2]; /* accs for this pitch, regardless of
				      * whether we are going to print them */
	char pitchaccstd;
	int rememberaccs;	/* remember accidentals in the table? */
	int n, k;		/* loop variables */


	staff_p = mainll_p->u.staff_p;
	s = staff_p->staffno;
	useaccs = svpath(s, USEACCS)->useaccs;
	carryaccs = svpath(s, CARRYACCS)->carryaccs;
	sharps = orig_eff_key(s);

	/*
	 * Initialize the table to say that for all octaves of all letters,
	 * no accidental is in effect.
	 */
	memset(curacc, 0, sizeof (curacc));

	/*
	 * Point at first group in each voice (some pointers may be null).
	 * Also set the elapsed time to zero for each.
	 */
	for (vidx = 0; vidx < MAXVOICES; vidx++) {
		curg_p[vidx] = staff_p->groups_p[vidx];
		vtime[vidx] = Zero;
	}

	/*
	 * Loop once for each group on this staff in this measure, working
	 * from left (time zero) to right.
	 */
	for (;;) {
		/*
		 * Find the earliest not yet processed group in any voice.  In
		 * case of tie, arbitrarily take the lower voice number.  (If
		 * this would really make a difference, the user should have
		 * manually put accidentals in.)  Notice that grace notes will
		 * also be handled; and if multiple voices have them at the
		 * same time, the ones in the lower numbered voice will be
		 * processed first.
		 */
		g_p = 0;		/* haven't found a group yet */
		mintime = Maxtime;	/* longer than any measure can be */
		savevidx = 0;		/* keep lint happy */
		for (vidx = 0; vidx < MAXVOICES; vidx++) {
			if (curg_p[vidx] != 0 && LT(vtime[vidx], mintime)) {
				g_p = curg_p[vidx];
				mintime = vtime[vidx];
				savevidx = vidx;
			}
		}

		/* if we hit the end on all the voices, break out */
		if (g_p == 0) {
			break;
		}

		/* set up for the next group */
		curg_p[savevidx] = curg_p[savevidx]->next;
		vtime[savevidx] = radd(mintime, g_p->fulltime);

		/* there's nothing to do if not a note group */
		if (g_p->grpcont != GC_NOTES) {
			continue;
		}

		/* find previous group; could be in previous measure if any */
		mll_p = mainll_p;
		prevg_p = prevgrpsyl(g_p, &mll_p);

		for (n = 0; n < g_p->nnotes; n++) {
			note_p = &g_p->notelist[n];
			curacc_p = curacc[ note_p->letter - 'a' ]
					 [ note_p->octave ];

			/*
			 * If the note is the destination of a tie, we never
			 * need to put an accidental on it, and it has no
			 * effect on later notes, so ignore it.
			 */
			if (prevg_p != 0 && prevg_p->grpcont == GC_NOTES) {
				for (k = 0; k < prevg_p->nnotes; k++) {
					if (prevg_p->notelist[k].tie == YES &&
					    prevg_p->notelist[k].letter ==
					    note_p->letter && prevg_p->notelist
					    [k].octave == note_p->octave) {
						break;
					}
				}
				/* if we found it was tied, continue to next */
				if (k < prevg_p->nnotes) {
					continue;
				}
			}

			/* find what pitch this note really is */
			rememberaccs = NO;
			if (has_accs(note_p->acclist)) {
				/* user provided acc; that is the pitch */
				COPY_ACCS(pitchacc, note_p->acclist);

				/* if we must keep user accs, continue */
				if (useaccs != UA_Y_NONEREMUSER &&
				    useaccs != UA_Y_NONNATREMUSER) {
					/* remember them if we are supposed to*/
					if (carryaccs == YES) {
						COPY_ACCS(curacc_p,
							note_p->acclist);
					}
					continue;
				}

				/* wipe out user accs; if really needed, they
				 * will be restored later */
				CLEAR_ACCS(note_p->acclist);

				/* remember them, if we are supposed to */
				rememberaccs = YES;

			} else if (has_accs(curacc_p)) {
				/* acc is in effect; that is the pitch */
				COPY_ACCS(pitchacc, curacc_p);
			} else {
				/* base the pitch on the (former) keysig */
				circnum = strchr(Circle, note_p->letter)
						- Circle;
				if (circnum - sharps < 0) {
					standard_to_acclist('#', pitchacc);
				} else if (circnum - sharps >= 7) {
					standard_to_acclist('&', pitchacc);
				} else {
					standard_to_acclist('n', pitchacc);
				}
			}

			/*
			 * If this note is a nongrace with no user-supplied
			 * accidental, and another nongrace on the same staff
			 * at the same time has accidental (user-supplied or
			 * put there in a previous loop iteration), do not put
			 * an accidental on this note.
			 */
			if (g_p->grpvalue == GV_NORMAL && rememberaccs == NO) {
				allow_acc = YES;
				for (g2_p = stafftime2firstgrp(
						mainll_p->u.staff_p, mintime);
				     g2_p != 0 && g2_p->staffno == s;
				     g2_p = g2_p->gs_p) {
					if (group_note_has_acc(g2_p, note_p)) {
						allow_acc = NO;
						break;
					}
				}
				if (allow_acc == NO) {
					continue;
				}
			}

			pitchaccstd = standard_acc(pitchacc);

			/*
			 * Set the acc when needed; and if so, also remember it
			 * in the table if carracc allows.
			 */
			switch (useaccs) {
			case UA_Y_NONE:
			case UA_Y_NONEREMUSER:
				/*
				 * Need an acc when pitch differs from the
				 * remembered acc, except not when we need a
				 * natural pitch and there has been no acc.
				 */
				if ( ! eq_accs(pitchacc, curacc_p) &&
				   ! (pitchaccstd == 'n' && ! has_accs(curacc_p))) {
					COPY_ACCS(note_p->acclist, pitchacc);
					rememberaccs = YES;
				}
				break;

			case UA_Y_NONNAT:
			case UA_Y_NONNATREMUSER:
				/*
				 * Need an acc when pitch is nonnatural; also
				 * when pitch differs from the remembered acc
				 * and there has been an acc.  This is like
				 * (pitch nonnatural || the previous case).
				 * Do the boolean algebra.
				 */
				if (pitchaccstd != 'n' ||
				   ( ! eq_accs(pitchacc, curacc_p) &&
				   has_accs(curacc_p))) {
					COPY_ACCS(note_p->acclist, pitchacc);
					rememberaccs = YES;
				}
				break;

			case UA_Y_ALL:
				/*
				 * Always need an acc.
				 */
				COPY_ACCS(note_p->acclist, pitchacc);
				break;
			}

			/* remember accs, if we are supposed to */
			if (carryaccs == YES && rememberaccs == YES) {
				COPY_ACCS(curacc_p, pitchacc);
			}
		}
	}
}

/*
 * Name:        group_note_has_acc()
 *
 * Abstract:    Find whether group has matching note with accidental.
 *
 * Returns:	YES or NO
 *
 * Description: This function searches the given group to see if it has a note
 *		that matches the letter and octave of the given note, and
 *		furthermore whether that note has an accidental.
 */

static int
group_note_has_acc(g_p, note_p)

struct GRPSYL *g_p;	/* the group to search */
struct NOTE *note_p;	/* the note to match */

{
	int n;		/* index into note list */


	for (n = 0; n < g_p->nnotes; n++) {
		if (g_p->notelist[n].letter == note_p->letter &&
		    g_p->notelist[n].octave == note_p->octave) {
			if (has_accs(g_p->notelist[n].acclist)) {
				return (YES);
			} else {
				return (NO);
			}
		}
	}

	return (NO);
}
