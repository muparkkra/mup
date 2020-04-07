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
/*
 * Name:	miditune.c
 *
 * Description:	This file contains code for MIDI that deals with non-standard
 *		tuning.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"


/* indices for the note letters (the letter minus 'a') */
#define	A	0
#define	B	1
#define	C	2
#define	D	3
#define	E	4
#define	F	5
#define	G	6

#define NUM_STD_ACCS	5	/* there are 5 standard accs */

/*
 * This structure holds information about one octave/letter/acclist combination
 * in a MIDI note map.  Each map has MAXMIDIMAPS of them.  The entries that are
 * in use are sorted in order of octave/letter/acclist.
 */
struct MIDINOTE {
	char octave;		/* octave number */
	char letter;		/* note letter */
	char acclist[2 * MAX_ACCS]; /* accs; if none, put a natural here */
	char notenum;		/* MIDI note num to use, 0 to MAXMIDINOTES-1 */
	float freq;		/* frequency */
};

/*
 * There can be up to MAXMIDIMAPS maps.  Each is allocated when needed.  Define
 * an array of pointers to each (actually pointers to the first struct in each).
 */
static struct MIDINOTE *Accmap_p[MAXMIDIMAPS];

/* define how many slots are actually being used in each map */
static int Accsused[MAXMIDIMAPS];

/* define the index into Accmap_p; the index to the current map */
static int Curmapidx;

/* index into Accmap_p for the last map that was defined */
static int Lastmapidx;

/* keep track of which MIDI note numbers have been used so far */
static unsigned char Notenum_used[MAXMIDINOTES];

/* frequencies of the white notes c0 to b0; index by A, B, C, etc. */
static float White_freq[7];


static void populate_map P((struct GRPSYL *gs_p, struct ACCINFO *ctx_accinfo_p,
		 int num_accs, struct ACCINFO *def_accinfo_p));
static void set_up_def_accs P((struct ACCINFO default_accs[]));
static void set_white_freq P((void));
static void find_note_acclist P((char acclist[], int staff,
		struct NOTE *note_p, int raw_notenum));
static double get_freq P((struct NOTE *note_p, char acclist[],
		struct ACCINFO *ctx_accinfo_p, int num_accs,
		struct ACCINFO *def_accinfo_p, char *file, int line));
static int comp_rough P((const void *, const void *));
static int comp_exact P((const void *, const void *));

/*
 * Name:	gen_tuning_maps()
 *
 * Abstract:	Generate all note maps needed for this song.
 *
 * Returns:	void
 *
 * Description:	This function loops through the song, generating all the MIDI
 *		note maps that will be needed.  A map is needed for each
 *		setting of the parameters a4freq, tuning, and acctable.
 */

void
gen_tuning_maps(numtracks, track2staff_map, track2voice_map)

int numtracks;			/* number of tracks (staff/voice combos used) */
short track2staff_map[];	/* map a track number to staff number */
short track2voice_map[];	/* map a track number to voice index */

{
	/*
	 * If the user doesn't set the acctable parameter (which says which
	 * accidentals context to use), we need to define our own pseudo
	 * accidentals context.  We store it here.  It contains only the 5
	 * standard accidentals.
	 */
	struct ACCINFO default_accs[5];

	struct MAINLL *mainll_p;	/* to index through main list */
	struct MAINLL *mll_p;		/* to index through main list */
	struct ACCIDENTALS *acc_name_map_p;	/* info about an acc context */
	struct ACCINFO *ctx_accinfo_p;	/* first in info from an acc context */
	int num_accs;			/* number of accs in acc context */
	int staff;			/* staff number */
	int vidx;			/* voice index */
	int t;				/* track number */
	int n;


	initstructs();

	/* apply all SSVs before the first music data */
	for (mainll_p = Mainllhc_p; mainll_p != 0 && mainll_p->str != S_STAFF;
			mainll_p = mainll_p->next) {
		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
		}
	}

	/* prepare for populating the first map, which will be 0 */
	Curmapidx = 0;

	mll_p = 0;

	/*
	 * Loop once for each map that needs to be created, populating the map
	 * for all of the notes found to be put in this map.  If the user never
	 * changes a4freq/tuning/acctable after the first music data is given,
	 * there will be only one map, and we go once through the loop.  But
	 * for each bar line where they change one of these parms, there will
	 * be another map and another time through the loop to collect note
	 * data for it.
	 */
	while (mainll_p != 0) {

		/* we are at the first staff of a section */

		debug(512, "generating MIDI note map %d", Curmapidx);

		/*
		 * Allocate structures; each can be used for one
		 * octave/letter/acc combination.
		 */
		MALLOCA(struct MIDINOTE, Accmap_p[Curmapidx], MAXMIDINOTES);
		Accsused[Curmapidx] = 0;	/* no notes yet */

		/* set frequency of white notes c0 through b0 */
		set_white_freq();

		/*
		 * Get a pointer to a table of info about all the possible
		 * accidentals, and how many accs there are in the table.
		 */
		if (Score.acctable == 0) {
			/* no acc context is set, so no accs there */
			ctx_accinfo_p = 0;
			num_accs = 0;
		} else {
			/* search for an acc table with the right name */
			for (acc_name_map_p = Acc_contexts_list_p;
					acc_name_map_p != 0;
					acc_name_map_p = acc_name_map_p->next) {
				if (strcmp(acc_name_map_p->name,
						Score.acctable) == 0) {
					break;
				}
			}
			if (acc_name_map_p == 0) {
				pfatal("parameter acctable=%s but cannot find that accidentals context", Score.acctable);
			}
			ctx_accinfo_p = acc_name_map_p->info;
			num_accs = acc_name_map_p->size;
		}

		/*
		 * In case not all of the standard accs are defined in the
		 * acc context (or there is no acc context), make a table of
		 * the standard ones to default to.
		 */
		set_up_def_accs(default_accs);

		/* so far, no note slots have been used in this map */
		for (n = 0; n < MAXMIDINOTES; n++) {
			Notenum_used[n] = NO;
		}

		/*
		 * Remember state of SSVs, since we need to go back to that
		 * state after going through each staff/voice.
		 */
		savessvstate();

		/* for each track, handle that staff & voice in this section */
		for (t = 0; t < numtracks; t++)  {

			restoressvstate();

			/* find out what staff and voice this track is for */
			staff = track2staff_map[t];
			vidx  = track2voice_map[t];

			init_tie_table();

			/* loop through this section, doing this staff/voice */
			for (mll_p = mainll_p; mll_p != 0 &&
					(mll_p->str != S_SSV ||
					! TUNEPARMSSV(mll_p->u.ssv_p));
					mll_p = mll_p->next) {

				switch (mll_p->str) {
				case S_SSV:
					asgnssv(mll_p->u.ssv_p);
					continue;

				case S_STAFF:
					break;

				default:
					continue;
				}

				/* skip if it is not the staff we want */
				if (mll_p->u.staff_p->staffno != staff) {
					continue;
				}

				/* skip if we aren't going to play this staff*/
				if ( ! staff_audible(staff)) {
					continue;
				}

				/*
				 * Do this now instead of later in gen_midi()
				 * so that we will have the correct octaves.
				 */
				octave_transpose(mll_p->u.staff_p, mll_p,
					vidx, YES);

				/* set up table based on key sig and ties */
				init_accidental_map(staff);

				/* pop map for this measure for this voice */
				populate_map(mll_p->u.staff_p->groups_p[vidx],
					ctx_accinfo_p, num_accs, default_accs);
			}
		}

		/* if debug level 512, print the contents of this map */
		if (debug_on(512)) {
			struct MIDINOTE *mn_p;
			int idx;
			fprintf(stderr, "=========== Map %d =============\n",
					Curmapidx);
			for (n = 0; n < Accsused[Curmapidx]; n++) {
				mn_p = &Accmap_p[Curmapidx][n];
				fprintf(stderr, "oct=%d let=%c notenum=%03d freq=%.4f\tacclist=",
						mn_p->octave, mn_p->letter,
						mn_p->notenum, mn_p->freq);
				for (idx = 0; idx < MAX_ACCS * 2 &&
					    mn_p->acclist[idx] != 0; idx += 2) {
					fprintf(stderr, "%s ", get_charname(
							mn_p->acclist[idx + 1],
							mn_p->acclist[idx]));
				}
				fprintf(stderr, "\n");
			}
		}

		/* apply SSVs until the next music data */
		for (mainll_p = mll_p; mainll_p != 0 && mainll_p->str !=
				S_STAFF; mainll_p = mainll_p->next) {
			if (mainll_p->str == S_SSV) {
				asgnssv(mainll_p->u.ssv_p);
			}
		}

		Curmapidx++;			/* index to next map */

		if (Curmapidx >= MAXMIDIMAPS) {
			ufatal("parameters a4freq, tuning, and acctable have changed too many times; can change in only %d places", MAXMIDIMAPS);
		}
	}

	/* this is now the last map */
	Lastmapidx = Curmapidx;
}

/*
 * Name:	populate_map()
 *
 * Abstract:	Add note information to map for the given measure and voice.
 *
 * Returns:	void
 *
 * Description:	This function loops through the groups for this voice in this
 *		measure, adding information to the current map for any
 *		octave/letter/acclist that is not already in the map.
 */

static void
populate_map(gs_p, ctx_accinfo_p, num_accs, def_accinfo_p)

struct GRPSYL *gs_p;		/* first group in this measure */
struct ACCINFO *ctx_accinfo_p;	/* info from accidentals context, null if none*/
int num_accs;			/* number of accs in that table */
struct ACCINFO *def_accinfo_p;	/* default info for the std accs */

{
	int n;				/* index into nnotes */
	struct NOTE *note_p;		/* the note we are working on */
	struct MIDINOTE *slot_p;	/* points at a place in array */
	struct MIDINOTE key_elem;	/* info from current note */
	int raw_notenum;		/* MIDI notenum of the white note */
	int desired_notenum;		/* the one for the equal temperament
					 * note closest in freq to our note */
	int idx;


	/*
	 * Loop through every note in every group.  For rest, space, and mrpt,
	 * nnotes is 0, so this works.
	 */
	for ( ; gs_p != 0; gs_p = gs_p->next) {
		for (n = 0; n < gs_p->nnotes; n++) {

			note_p = &gs_p->notelist[n];

			/* load note info into a MIDINOTE structure */
			key_elem.octave = note_p->octave;
			key_elem.letter = note_p->letter;

			/* get the normal note number of the white note */
			raw_notenum = get_raw_notenum(note_p,
				gs_p->inputfile, gs_p->inputlineno,
				gs_p->staffno);

			/* load the acclist that should apply */
			find_note_acclist(key_elem.acclist, gs_p->staffno,
				note_p, raw_notenum);

			/*
			 * If we had to defer the setting of accs because of a
			 * tie into the measure, do that now.
			 */
			if (Deferred_acc[raw_notenum][0] != NO_DEFERRED_ACC) {
				COPY_ACCS(Accidental_map[raw_notenum],
					Deferred_acc[raw_notenum]);
				Deferred_acc[raw_notenum][0] = NO_DEFERRED_ACC;
			}

			/*
			 * See if this octave/letter/acc combination is already
			 * in the current map, or where it would go.
			 */
			slot_p = bsearch(&key_elem, Accmap_p[Curmapidx],
					Accsused[Curmapidx],
					sizeof (struct MIDINOTE), comp_rough);

			/* if already in map, no more work to do for it */
			if (slot_p != 0 && comp_exact(slot_p, &key_elem) == 0) {
				continue;
			}

			/*
			 * It's not in the map, so we need to add it.  Error if
			 * the map is already full.
			 */
			if (Accsused[Curmapidx] == MAXMIDINOTES) {
				/* printf("The table is full.\n"); */
				l_ufatal(gs_p->inputfile, gs_p->inputlineno,
				"too many unique note/accidental combinations have been used; only %d combinations are allowed in one section of the piece; reset one of the parameters a4freq, tuning, or acctable before this place (even if to the same value as before) to clear the table", MAXMIDINOTES);
			}

			if (slot_p == 0) {	/* need to insert in slot 0 */
				/* point where new item will go, slot 0 */
				slot_p = &Accmap_p[Curmapidx][0];

				/* move entire array down one slot */
				memmove(&Accmap_p[Curmapidx][1],
					&Accmap_p[Curmapidx][0],
					Accsused[Curmapidx] *
						sizeof (struct MIDINOTE));
			} else {	/* need to after this slot */
				/* point where new item will go */
				slot_p++;

				/* move rest of array down one slot */
				/* (if we are at end, it moves 0 bytes) */
				memmove(slot_p + 1, slot_p,
					(char *)&Accmap_p[Curmapidx]
						[Accsused[Curmapidx]] -
					(char *)slot_p);
			}

			/* copy new element into array */
			*slot_p = key_elem;
			Accsused[Curmapidx]++;

			/* now decide what note number to use for this pitch */

			/* calculate the frequency of this note */
			slot_p->freq = get_freq(note_p, slot_p->acclist,
				ctx_accinfo_p, num_accs, def_accinfo_p,
				gs_p->inputfile, gs_p->inputlineno);

			/*
			 * Find the note number we'd like to use, the one for
			 * the note whose normal frequency is closest to the
			 * frequency we have:  Divide our freq by the freq of
			 * c-1 (MIDI note 0).  Then convert that ratio to
			 * halfsteps.  The 0.5 is for rounding to the nearest.
			 */
			desired_notenum =
				log(slot_p->freq / (White_freq[C] / 2.0)) *
				12.0 / log(2.0) + 0.5;

			/* if it's valid and not used yet, claim it */
			if (desired_notenum >= 0 &&
					desired_notenum < MAXMIDINOTES &&
					Notenum_used[desired_notenum] == NO) {
				slot_p->notenum = desired_notenum;
				Notenum_used[desired_notenum] = YES;
				continue;
			}

			/*
			 * The note number we wanted has already been taken.
			 * Okay, so start from 0 and find the first unused one.
			 */
			for (idx = 0; idx < MAXMIDINOTES; idx++) {
				if (Notenum_used[idx] == NO) {
					break;
				}
			}
			if (idx >= MAXMIDINOTES) {
				l_pfatal(gs_p->inputfile, gs_p->inputlineno,
				"can't find any empty slot in Accmap_p[%d]",
				Curmapidx);
			}

			slot_p->notenum = idx;
			Notenum_used[idx] = YES;
		}
	}
}

/*
 * Name:	set_white_freq()
 *
 * Abstract:	Set the frequencies of the white notes in the 4 octave.
 *
 * Returns:	void
 *
 * Description:	Based on the a4freq and tuning parameters, this function sets
 *		the frequencies of the white notes in the 4 octave.  It also
 *		finds the equal temperament c-1, which we need later.
 */

static void
set_white_freq()
{
	float fifth;		/* ratio for a perfect fifth */


	/* set frequency of a0 */
	White_freq[A] = Score.a4freq / (1 << 4);	/* down 4 octaves */

	/* set the ratio for a perfect fifth, based on the tuning */
	switch (Score.tuning) {
	case TU_EQUAL:
	default:	/* default will never actually happen */
		fifth = pow(2.0, 7.0 / 12.0);	/* 7 equal tempered halfsteps */
		break;
	case TU_PYTHAGOREAN:
		fifth = 3.0 / 2.0;
		break;
	case TU_MEANTONE:
		fifth = pow(5.0, 1.0 / 4.0);	/* fourth root of 5 */
		break;
	}

	White_freq[E] = White_freq[A] * fifth / 2.0;   /* down a 4th */
	White_freq[B] = White_freq[E] * fifth;         /* up a 5th */
	White_freq[D] = White_freq[A] / fifth;         /* down a 5th */
	White_freq[G] = White_freq[D] / fifth * 2.0;   /* up a 4th */
	White_freq[C] = White_freq[G] / fifth;         /* down a 5th */
	White_freq[F] = White_freq[C] / fifth * 2.0;   /* up a 4th */
}

/*
 * Name:	find_note_acclist()
 *
 * Abstract:	Find the acclist that applies to the given note.
 *
 * Returns:	void
 *
 * Description:	This function loads the given acclist with what should apply to
 *		the given note.  This is either from what the note already has,
 *		or it is from the key sig or the previous note carrying over.
 */

static void
find_note_acclist(acclist, staff, note_p, raw_notenum)

char acclist[];			/* acclist to be loaded */
int staff;			/* staff number this note is on */
struct NOTE *note_p;		/* the note we're working on */
int raw_notenum;		/* note number of the white note */

{
	if (note_p->acclist[0] == '\0') {
		/*
		 * Note has no accs, so copy from whatever is remembered from
		 * key sig or prevous note.
		 */
		COPY_ACCS(acclist, Accidental_map[raw_notenum]);
	} else {
		/*
		 * Note has acc(s).  Copy them.  Also, if the user wants
		 * accidentals carried through the bar (the normal case), or
		 * the note is tied, remember acc(s) for later notes.
		 */
		COPY_ACCS(acclist, note_p->acclist);
		if (svpath(staff, CARRYACCS)->carryaccs == YES ||
				note_p->tie == YES) {
			COPY_ACCS(Accidental_map[raw_notenum], note_p->acclist);
		}
	}

	if (acclist[0] == '\0') {
		/* no accs, so put a natural there */
		standard_to_acclist('n', acclist);
	}
}

/*
 * Name:	get_freq()
 *
 * Abstract:	Find the frequency of an octave/letter/acclist combination.
 *
 * Returns:	void
 *
 * Description:	Find the frequency of an octave/letter/acclist combination.
 */

static double
get_freq(note_p, acclist, ctx_accinfo_p, num_accs, def_accinfo_p, file, line)

struct NOTE *note_p;		/* the note we're working on */
char acclist[];			/* acclist that is to be used for this note */
struct ACCINFO *ctx_accinfo_p;	/* info from accidentals context, null if none*/
int num_accs;			/* number of acc in that table */
struct ACCINFO *def_accinfo_p;	/* default info for the std accs */
char *file;			/* needed for l_ufatal */
int line;			/* needed for l_ufatal */

{
	float freq;		/* the answer */
	float ratio;		/* for one accidental */
	int n;			/* index into acclist */
	int infoidx;		/* index into an accinfo array */


	/* get freq of the white note in the correct octave */
	freq = White_freq[note_p->letter - 'a'] * (1 << note_p->octave);

	/* loop, applying the ratio of each accidental */
	for (n = 0; n < MAX_ACCS * 2 && acclist[n] != 0; n += 2) {
		/*
		 * First search acc context for this acc, if there is an acc
		 * context.  If there isn't, num_accs will be 0.
		 */
		for (infoidx = 0; infoidx < num_accs; infoidx++) {
			if (ctx_accinfo_p[infoidx].font == acclist[n] &&
			    ctx_accinfo_p[infoidx].code == acclist[n + 1]) {
				break;
			}
		}
		if (infoidx < num_accs) {
			/*
			 * We found the acc.  Remember its ratio for this note.
			 * If it wasn't defined for that note, it'll be -1.
			 */
			ratio = ctx_accinfo_p[infoidx].
					offset[note_p->letter - 'a'];
		} else {
			/* not found; remember bad ratio */
			ratio = -1.0;
		}

		if (ratio <= 0.0) {
			/*
			 * If this is a standard acc, we'll find it in
			 * def_accinfo_p, and use the ratio from there.  If it
			 * is a custom acc, we won't find it.
			 */
			for (infoidx = 0; infoidx < NUM_STD_ACCS; infoidx++) {
				if (def_accinfo_p[infoidx].font == acclist[n] &&
				    def_accinfo_p[infoidx].code == acclist[n + 1]) {
					break;
				}
			}
			if (infoidx < NUM_STD_ACCS) {
				/* found it, use that ratio */
				ratio = def_accinfo_p[infoidx].
						offset[note_p->letter - 'a'];
			}
		}

		if (ratio <= 0.0) {
			/* must be a nonstandard acc or we would have found it*/
			if (Score.acctable == 0) {
				/* no acc context */
				l_ufatal(file, line, "accidental %s on note %c%d is not defined; use an accidentals context to define it",
				get_charname(acclist[n + 1],acclist[n]),
				note_p->letter, note_p->octave);
			} else {
				/* acc's ratio not defined for this note */
				l_ufatal(file, line, "accidental %s on note %c%d has no value defined in accidentals context \"%s\" for note \"%c\"",
					get_charname(acclist[n + 1], acclist[n]),
					note_p->letter, note_p->octave,
					Score.acctable, note_p->letter);
			}
		}

		freq *= ratio;
	}

	if (freq < MINFREQ || freq > MAXFREQ) {
		l_ufatal(file, line, "frequency %f of note \"%c%d\" is out of range; must be between %f and %f",
			freq, note_p->letter, note_p->octave, MINFREQ, MAXFREQ);
	}

	return (freq);
}

/*
 * Name:	comp_rough()
 *
 * Abstract:	Comparison for bsearch that finds where new element should go.
 *
 * Returns:	0 if the element being searched for (key) equals the current
 *			array element, or is between it and the next array
 *			element (or in the case that the current element is the
 *			last, that the key is after the last)
 *		-1 if the key is before the current array element
 *		1 otherwise
 *
 * Description:	If you pass this function to bsearch, bsearch will return NULL
 *		pointer only if the key is before the first element currently
 *		in the array.  Otherwise it will return a pointer to the slot
 *		that has the key, if the key is present; else a pointer to the
 *		slot after which the key should be inserted.
 */

static int
comp_rough(key_element_p, cur_arr_element_p)

const void *key_element_p;
const void *cur_arr_element_p;

{
	/* pointers to the correct data type */
	struct MIDINOTE *key_elem_p;
	struct MIDINOTE *cur_arr_elem_p;
	struct MIDINOTE *next_arr_elem_p;


	key_elem_p = (struct MIDINOTE *)key_element_p;
	cur_arr_elem_p = (struct MIDINOTE *)cur_arr_element_p;
	next_arr_elem_p = cur_arr_elem_p + 1;

	/* return 0 if:   cur <= key && (cur is last || key < next)   */
	if (comp_exact(cur_arr_elem_p, key_elem_p) <= 0 &&
	    (cur_arr_elem_p - Accmap_p[Curmapidx] == Accsused[Curmapidx] - 1 ||
	    comp_exact(key_elem_p, next_arr_elem_p) < 0)) {
		return (0);
	}

	/* return -1 if:   key <= cur   */
	if (comp_exact(key_elem_p, cur_arr_elem_p) < 0) {
		return (-1);
	}

	return (1);
}

/*
 * Name:	comp_exact()
 *
 * Abstract:	Comparison for bsearch that finds an exact match
 *
 * Returns:	0 if the element being searched for (key) equals the current
 *			array element
 *		-1 if the key is before the current array element
 *		1 otherwise
 *
 * Description:	If you pass this function to bsearch, bsearch will return NULL
 *		pointer if the key is not found, otherwise a pointer to the
 *		array slot that has the key.
 */

static int
comp_exact(key_element_p, cur_arr_element_p)

const void *key_element_p;
const void *cur_arr_element_p;

{
	/* pointers to the correct data type */
	struct MIDINOTE *key_elem_p;
	struct MIDINOTE *cur_arr_elem_p;


	key_elem_p = (struct MIDINOTE *)key_element_p;
	cur_arr_elem_p = (struct MIDINOTE *)cur_arr_element_p;

	/* if octaves are different, they determine the result */
	if (key_elem_p->octave != cur_arr_elem_p->octave) {
		return ((int)key_elem_p->octave - (int)cur_arr_elem_p->octave);
	}

	/* else if letters are different, they determine the result (note the
	 * ordering a to g, not c to b; it doesn't need to be in pitch order) */
	if (key_elem_p->letter != cur_arr_elem_p->letter) {
		return ((int)key_elem_p->letter - (int)cur_arr_elem_p->letter);
	}

	/* else the acclists determine the result */
	return (strncmp(key_elem_p->acclist, cur_arr_elem_p->acclist,
			MAX_ACCS * 2));
}

/*
 * Name:	set_up_def_accs()
 *
 * Abstract:	Set up an ACCINFO array for the five standard accs.
 *
 * Returns:	void
 *
 * Description:	When acctable is not set, meaning the user isn't using an
 *		accidentals context, this function sets up the equivalent thing
 *		internally for the standard accs.
 */

static void
set_up_def_accs(default_accs)

struct ACCINFO default_accs[];

{
	int accidx;		/* index to which of the accs we are doing */
	int offidx;		/* index to offset of which of the 7 notes */


	default_accs[0].code = C_NAT;
	default_accs[1].code = C_SHARP;
	default_accs[2].code = C_FLAT;
	default_accs[3].code = C_DBLSHARP;
	default_accs[4].code = C_DBLFLAT;

	for (accidx = 0; accidx < 5; accidx++) {
		default_accs[accidx].font = FONT_MUSIC;

		/* set all to -1 so that set_default_acc_offsets will reset */
		for (offidx = 0; offidx < 7; offidx++) {
			default_accs[accidx].offset[offidx] = -1.0;
		}

		set_default_acc_offsets(&default_accs[accidx]);
	}
}

/*
 * Name:	find_note_number()
 *
 * Abstract:	Find the note number for this octave/letter/acclist.
 *
 * Returns:	the note number
 *
 * Description:	This function returns the note number for a given note, taking
 *		into account accidentals it has or inherits from the key sig or
 *		the previous note.
 */

int
find_note_number(staff, note_p, raw_notenum)

int staff;			/* staff number this note is on */
struct NOTE *note_p;		/* the note we're working on */
int raw_notenum;		/* note number of the white note */

{
	struct MIDINOTE key_elem;	/* info from current note */
	struct MIDINOTE *slot_p;	/* points at a place in array */


	/* load note info into a MIDINOTE structure */
	key_elem.octave = note_p->octave;
	key_elem.letter = note_p->letter;

	find_note_acclist(key_elem.acclist, staff, note_p, raw_notenum);

	slot_p = bsearch(&key_elem, Accmap_p[Curmapidx], Accsused[Curmapidx],
			sizeof (struct MIDINOTE), comp_exact);
	if (slot_p == 0) {
		pfatal("can't find accidentals for note %c%d",
			note_p->letter, note_p->octave);
	}

	return (slot_p->notenum);
}

/*
 * Name:	notemap_size()
 *
 * Abstract:	Find the note number for this octave/letter/acclist.
 *
 * Returns:	number of accs in the given map
 *
 * Description:	This function returns the number of accidentals used in the
 *		given map.  It depends on the fact that Curmapidx is set
 *		to the last map we created.
 */

int
notemap_size(map_index)

int map_index;		/* index to which map */

{
	/* at this point, Curmapidx is the last map we created */
	if (map_index > Lastmapidx) {
		pfatal("invalid map index %d passed to notemap_size",
				map_index);
	}

	return (Accsused[map_index]);
}

/*
 * Name:	notefreq()
 *
 * Abstract:	Return note number and frequency of a note map entry.
 *
 * Returns:	Note number.  Frequency is returned via a pointer.
 *
 * Description:	This function is given an index to a map, and to an entry
 *		within the map.  It depends on the fact that Curmapidx is set
 *		to the last map we created.
 */

int
notefreq(map_index, slot_index, freq_p)

int map_index;		/* index to which map */
int slot_index;		/* which entry in the map */
float *freq_p;		/* return frequency here */

{
	/* at this point, Curmapidx is the last map we created */
	if (map_index > Lastmapidx) {
		pfatal("invalid map index %d passed to notemap_size",
				map_index);
	}
	if (slot_index > Accsused[map_index]) {
		pfatal("invalid map index %d passed to notemap_size",
				slot_index);
	}

	*freq_p = Accmap_p[map_index][slot_index].freq;
	return (Accmap_p[map_index][slot_index].notenum);

}

/*
 * Name:	num_notemaps()
 *
 * Abstract:	Return the number of maps that exist.
 *
 * Returns:	the number
 *
 * Description:	This function returns the number of maps that have been
 *		defined.  This is provided so that Lastmapidx can be static
 *		yet other files can query for this number.
 */

int
num_notemaps()
{
	return (Lastmapidx + 1);
}

/*
 * Name:	set_map_index()
 *
 * Abstract:	Set the current map index to the given value 
 *
 * Returns:	void
 *
 * Description:	This function sets the current map index to the given value.
 *		This is provided so that Curmapidx can be static and yet be set
 *		by other files.
 */
void
set_map_index(idx)

int idx;

{
	if (idx < 0 || idx > Lastmapidx) {
		pfatal("bad index %d in set_map_index", idx);
	}
	Curmapidx = idx;
}
