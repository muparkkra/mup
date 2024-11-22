
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

/* This file contains functions for building up lists of GRPSYL structs,
 * error checking them, etc. These functions are called at parse
 * time from gram.y
 */


#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"

/* 1-line staff allows for default pitch on first chord of measure. However,
 * at the time we are gathering note information, it may not be known whether
 * we are on a 1-line staff or not, so it is temporarily set to PP_NO_PITCH,
 * then later if we have a 1-line staff, it gets converted to default of C4 */
#define DFLT_PITCH	'c'
#define DFLT_OCT	4

/* how many NOTE structs to allocate at a time. We don't know in advance
 * how many we'll need, so get a bunch and realloc as necessary. */
#define NOTE_CHUNK (4)

static int Tuplet_state = NOITEM;	/* current tuplet state */
static struct GRPSYL *Tuplet_list_p;	/* first GRPSYL in a tuplet group */
static short Slur_begin;	/* slurtolist index at which <> began */

/* Current emptymeas parameter value being processing, or 0 if none.
 * This is used for error messages to help user know the real error was
 * from the parameter rather than the place where it was recognized. */
static char *Curr_emptymeas = 0;
/* These remember the current staff/and voice for emptymeas, also for errors. */
static int EM_staff;
static int EM_voice;

/* Magic markers used to tell parser it has virtual input based on emptymeas.
 * There also needs to be lex patterns for these, but there doesn't seem any
 * handy way to have a single definition of the pattern and a string,
 * so these strings and the lex patterns must be manually kept in sync.
 * Fortunately, there should be little need to ever change them.
 * They are chosen to be something extremely unlikely for a user to put
 * in by accident, which could cause the parser to get confused. */
static char *EM_begin_marker = "|||-.->>";
static char *EM_end_marker = "<<-.-|||";

/* Buffer used for virtual input based on emptymeas parameter.
 * Gets malloced and freed as needed. This is a copy of Curr_emptymeas
 * plus the magic markers. */
static char *EM_buffer;

/* static functions */
static void clone_notelist P((struct GRPSYL *new_p, struct GRPSYL *old_p,
		int copy_acc_etc));
static void finish_bar P((void));
static void restart_bar P((void));
static void fix_grpsyl_list P((struct MAINLL *mainll_item_p));
static void fix_a_group_list P((struct GRPSYL *grpsyllist_p,
		struct MAINLL *mll_p, int staffno, int vno, char *fname,
		int lineno, int mrptnum, struct TIMEDSSV * tssv_p));
static void add_space_padding P((struct GRPSYL *grpsyllist_p, int grpsyl,
			RATIONAL used_time));
static void chk_a_syl_list P((struct GRPSYL *grpsyl_p, char *fname,
		int lineno));

/* change relative octaves to absolute */
static void resolve_octaves P((struct GRPSYL *grpsyl_item_p,
		int default_octave));
static void fix_frets P((struct GRPSYL *grpsyl_item_p));
static int string_number P((struct STRINGINFO *stringinfo_p, int nstrings,
		int letter, int accidental, int nticks));

static void check_grace P((struct MAINLL *mll_p,struct GRPSYL *grpsyl_item_p,
		char *fname, int lineno));
static void fix_strlist P((struct GRPSYL *gs_p, int font, int size,
		char *fname, int lineno));
static void check4missing_voices P((struct MAINLL *list_p));
static int replace_emptymeas P((int staffno, int vno));
static void sort_notes P((struct GRPSYL *grpsyl_p, char *fname, int lineno));
static int parse_bend_string P((char *bendstring));
static void check_bends P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
static void filltime P((struct GRPSYL *gs_p, RATIONAL totaltime));


/* allocate a new GRPSYL struct and return a pointer to it */

struct GRPSYL *
newGRPSYL(grp_or_syl)

int grp_or_syl;		/* GS_GROUP or GS_SYLLABLE */

{
	struct GRPSYL *new_p;


	/* allocate a new structure */
	CALLOC(GRPSYL, new_p, 1);

	/* fill in default values */
	new_p->grpsize = GS_NORMAL;
	new_p->grpvalue = GV_NORMAL;
	new_p->grpcont = GC_NOTES;
	new_p->beamloc = NOITEM;
	new_p->breakbeam = NO;
	new_p->clef_vert = NO;
	new_p->tuploc = NOITEM;
	new_p->grpsyl = (short) grp_or_syl;
	new_p->is_multirest = NO;
	new_p->is_meas = NO;
	new_p->meas_rpt_type = MRT_NONE;
	new_p->roll = NOITEM;
	new_p->inputfile = Curr_filename;
	new_p->inputlineno = (short) yylineno;
	new_p->with_was_gtc = NO;
	new_p->stemdir = (short) UNKNOWN;
	new_p->beamto = CS_SAME;
	new_p->stemto = CS_SAME;
	/* default to no user-specified stem length */
	new_p->stemlen = STEMLEN_UNKNOWN;
	/* default to no user-specified rest dist */
	new_p->restdist = NORESTDIST;
	/* default to no user-specified syllable position */
	new_p->sylposition = NOSYLPOSITION;
	/* no clef change before this group */
	new_p->clef = NOCLEF;
	/* by default, placement will figure out the note head shape */
	new_p->headshape = HS_UNKNOWN;
	/* let Mup figure out angle for beams */
	new_p->beamslope = NOBEAMANGLE;
	/* assume no explicit abm/eabm */
	new_p->autobeam = NOITEM;
	new_p->tupletslope = NOTUPLETANGLE;
	/* init pointers, just to be sure */
	new_p->restc = 0;
	new_p->notelist = 0;
	new_p->prev = 0;
	new_p->next = 0;
	new_p->gs_p = 0;
	new_p->vcombdest_p = 0;
	/* everything else zero-ed is proper default */

	return(new_p);
}


/* Copy group attributes (grpsize, grpvalue, "with" list, etc)
 * from one GRPSYL to another */

void
copy_attributes(newgrp_p, oldgrp_p)

struct GRPSYL *newgrp_p;	/* copy info into this one */
struct GRPSYL *oldgrp_p;	/* copy from this one */

{
	if (oldgrp_p == 0) {
		yyerror("empty [] not allowed on the first chord of a measure");
		return;
	}

	newgrp_p->grpvalue = oldgrp_p->grpvalue;
	newgrp_p->grpsize = oldgrp_p->grpsize;
	newgrp_p->headshape = oldgrp_p->headshape;
	newgrp_p->stemdir = oldgrp_p->stemdir;
	newgrp_p->stemlen = oldgrp_p->stemlen;
	newgrp_p->ho_usage = oldgrp_p->ho_usage;
	newgrp_p->ho_value = oldgrp_p->ho_value;

	clone_withlist(newgrp_p, oldgrp_p);

	if (oldgrp_p->slash_alt > 0) {
		newgrp_p->slash_alt = oldgrp_p->slash_alt;
	}
	newgrp_p->padding = oldgrp_p->padding;
	/* restdist is only used for rests, but easiest to just always copy,
	 * since grpcont hasn't been set yet.  */
	newgrp_p->restdist = oldgrp_p->restdist;
}


/* make a copy of withlist */

void
clone_withlist(newgrp_p, oldgrp_p)

struct GRPSYL *newgrp_p;
struct GRPSYL *oldgrp_p;

{
	register int n;		/* index through with list */


	newgrp_p->nwith = oldgrp_p->nwith;
	newgrp_p->with_was_gtc = oldgrp_p->with_was_gtc;
	if (oldgrp_p->nwith > 0) {
		MALLOC(WITH_ITEM, newgrp_p->withlist, oldgrp_p->nwith);
		for (n = 0; n < oldgrp_p->nwith; n++) {
			newgrp_p->withlist[n].string =
					copy_string(oldgrp_p->withlist[n].string + 2,
					Curr_font, Curr_size);
			newgrp_p->withlist[n].place = oldgrp_p->withlist[n].place;
		}
	}
}


/* Copy timeunit info from one GRPSYL to another,
 * or use default if source is NULL. If needed to use default timeunit,
 * and that included additive times, returns pointer to the list of those
 * times, else return NULL. */

struct TIMELIST *
copy_timeunit(newgrp_p, oldgrp_p, timelist_p)

struct GRPSYL *newgrp_p;	/* copy info into here */
struct GRPSYL *oldgrp_p;	/* copy from here. If NULL, use default value */
struct TIMELIST *timelist_p;	/* If non-null use this instead of oldgrp_p */

{
	struct SSV *ssv_p;

	ssv_p = 0;

	if (oldgrp_p == (struct GRPSYL *) 0) {

		/* use default timeunit. Have to figure out differently for
		 * lyrics than notes, because for lyrics the voice depends
		 * on the lyrics place. */
		if (newgrp_p->grpsyl == GS_GROUP) {
			ssv_p = get_dflt_timeunit_ssv();
		}
		else {
			ssv_p = get_lyr_dflt_timeunit_ssv();
		}
		newgrp_p->fulltime = ssv_p->timeunit;

		newgrp_p->basictime = reconstruct_basictime(newgrp_p->fulltime);
		newgrp_p->is_multirest = NO;
		newgrp_p->dots = recalc_dots(newgrp_p->fulltime,
							newgrp_p->basictime);
		return(ssv_p->timelist_p);
	}

	else {
		RATIONAL basictime;

		newgrp_p->basictime = oldgrp_p->basictime;
		newgrp_p->dots = oldgrp_p->dots;

		/* actual time value is (2 * basictime)
		 *      - (basic_time x 1/2 to the (dots) power) */
		/* can't just copy the fulltime from oldgrp, because that
		 * may have been altered if it was a tuplet */
		if (newgrp_p->basictime == BT_DBL) {
			basictime.n = 2;
			basictime.d = 1;
		}
		else if (newgrp_p->basictime == BT_QUAD) {
			basictime.n = 4;
			basictime.d = 1;
		}
		else if (newgrp_p->basictime == BT_OCT) {
			basictime.n = 8;
			basictime.d = 1;
		}
		else {
			basictime.n = 1;
			basictime.d = newgrp_p->basictime;
		}

		newgrp_p->fulltime = calcfulltime(basictime, newgrp_p->dots);
	}

	/* note that tuplet values are dealt with in end_tuplet(). Once we
	 * collect an entire tuplet, we can go through and multiply
	 * the fulltimes by the appropriate amount */
	return(timelist_p);
}


/* copy notes (list of NOTE structs) from one GRPSYL to another, along with
 * a few group attributes that should be copied when a GRPSYL is copied. */

void
copy_notes(newgrp_p, oldgrp_p)

struct GRPSYL *newgrp_p;	/* copy into here */
struct GRPSYL *oldgrp_p;	/* copy from here */

{
	/* first GRPSYL in bar must have notes specified */
	if (oldgrp_p == (struct GRPSYL *) 0) {
		add_note(newgrp_p, PP_NO_PITCH, No_accs, DFLT_OCT,
							0, NO, (char *) 0);
		newgrp_p->notelist [ newgrp_p->nnotes ].tie = NO;
		newgrp_p->notelist [ newgrp_p->nnotes ].tie = NO;
		newgrp_p->notelist [ newgrp_p->nnotes ].notesize = GS_NORMAL;
	}

	else {
		/* there was a previous GRPSYL -- use it for defaults */
		newgrp_p->grpcont = oldgrp_p->grpcont;
		clone_notelist(newgrp_p, oldgrp_p, NO);
		newgrp_p->is_meas = oldgrp_p->is_meas;
		newgrp_p->uncompressible = oldgrp_p->uncompressible;
	}
}


/* once the info about a group has been gathered,
 * add it to list of GRPSYL structs */

void
link_notegroup(newgrp_p, last_grp_p)

struct GRPSYL *newgrp_p;		/* one to add on */
struct GRPSYL *last_grp_p;		/* one to add it onto */

{
	int n;

	debug(4, "link_notegroup file=%s line=%d", newgrp_p->inputfile,
				newgrp_p->inputlineno);

	if (last_grp_p == (struct GRPSYL *) 0) {
		/* first one: we need to keep a pointer to this one to
		 * link onto one or more STAFF structs later */
		Curr_gs_list_p = newgrp_p;
	}

	else {
		/* link onto list */
		last_grp_p->next = newgrp_p;
	}
	newgrp_p->prev = last_grp_p;

	/* fill in tuplet information, based on current state. Adjust tuplet
	 * state if necessary for use with later groups */
	newgrp_p->tuploc = (short) Tuplet_state;
	if (Tuplet_state == STARTITEM) {
		Tuplet_list_p = newgrp_p;
		Tuplet_state = INITEM;
	}

	/* dist is only allowed on rests */
	if (newgrp_p->restdist != NORESTDIST) {
		for (n = 0; n < newgrp_p->nnotes; n++) {
			if (newgrp_p->notelist[n].letter != PP_REST) {
				yyerror("dist only allowed on rests");
			}
		}
	}
}


/* make a copy of a linked list of GRPSYL structs and return a pointer to it */
/* Note that while this function is called for both groups and syllables,
 * lyric syllables are not copied by this function. Lyrics syllables are
 * handled in lyrics.c  */

struct GRPSYL *
clone_gs_list(list_p, copy_noteinfo)

struct GRPSYL *list_p;	/* the list to be cloned */
int copy_noteinfo;	/* if YES, copy notes and with lists */

{
	struct GRPSYL *new_p, *newlist_p;
	struct GRPSYL *prev_p = (struct GRPSYL *) 0;	/* to remember last one,
				 to know where to link on current one */



	/* walk down the existing list */
	for (newlist_p = (struct GRPSYL *) 0; list_p != (struct GRPSYL *) 0;
					list_p = list_p->next) {

		/* allocate space for copy of list element and fill it in */
		MALLOC(GRPSYL, new_p, 1);

		(void) memcpy(new_p, list_p, sizeof(struct GRPSYL) );

		/* we can't just copy the links--they have to be recalculated */
		if (newlist_p == (struct GRPSYL *) 0) {

			/* first in new list: keep track of where list begins,
			 * since we need to return this value, and keep track
			 * of where we are, so we can link the next element
			 * onto this one, which will be its prev element */
			newlist_p = new_p;
			prev_p = new_p;
		}

		else {
			prev_p->next = new_p;
			new_p->prev = prev_p;
			prev_p = new_p;
		}

		if (copy_noteinfo == YES) {
			/* also need to make copies of the notelist, since
			 * they contain COORDS that are unique
			* for each instance */
			clone_notelist(new_p, list_p, YES);

			/* with lists cannot be shared,
			 * because otherwise fix_string
			 * could get called on an already fixed string. */
			clone_withlist(new_p, list_p);
		}
	}

	return(newlist_p);
}


/* make a copy of the notelist in one GRPSYL struct into another */

static void
clone_notelist(new_p, old_p, copy_acc_etc)

struct GRPSYL *new_p;	/* copy into here */
struct GRPSYL *old_p;	/* from here */
int copy_acc_etc;	/* if YES, copy accidentals.  If just reusing a
			 * group on the same staff, we don't want to
			 * copy these things */

{
	register int n;		/* index through note list */


	/* mark the number of notes in the new list */
	new_p->nnotes = old_p->nnotes;

	/* copy cross-staff stem info */
	new_p->stemto = old_p->stemto;
	new_p->stemto_idx = old_p->stemto_idx;

	if (old_p->nnotes > 0) {

		/* allocate new memory and copy from old to new */
		CALLOC(NOTE, new_p->notelist, old_p->nnotes);

		(void)memcpy((char *) new_p->notelist, (char *) old_p->notelist,
			(unsigned) (old_p->nnotes * sizeof(struct NOTE)));

	}

	else if ((new_p->grpsyl == GS_GROUP) && (new_p->grpcont == GC_NOTES) &&
				is_mrpt(new_p) == NO) {
		yyerror("tried to use previous chord for defaults, but it had no notes");
		return;
	}

	for (n = 0; n < old_p->nnotes; n++) {

		if (copy_acc_etc == YES) {
			new_p->notelist[n].nslurto = old_p->notelist[n].nslurto;
			/* It isn't necessarily safe to share slurto lists.
			 * In particular, if multiple staffs are specified,
			 * and they have different clefs, the default octave
			 * will be different. So make a copy to be safe. */
			if (new_p->notelist[n].nslurto > 0) {
				MALLOC(SLURTO, new_p->notelist[n].slurtolist, 
						new_p->notelist[n].nslurto);
				memcpy(new_p->notelist[n].slurtolist,
					old_p->notelist[n].slurtolist,
					sizeof(struct SLURTO) *
					old_p->notelist[n].nslurto);
			}
			else {
				new_p->notelist[n].slurtolist = (struct SLURTO *) 0;
			}
		}
		else {
			new_p->notelist[n].nslurto = 0;
			new_p->notelist[n].slurtolist = (struct SLURTO *) 0;
			new_p->notelist[n].is_bend = NO;
			new_p->notelist[n].smallbend = NO;
		}

		/* alloc space for coordinates */
		CALLOCA(float, new_p->notelist[n].c, NUMCTYPE);
	}

	/* if a note that was cloned had an accidental on it,
	 * the clone doesn't need the accidental since we are in
	 * the same measure. Ties also do not get copied. But on tab staff,
	 * the accidental does need to get copied since it's part of the
	 * name of the string. */
	for (n = 0; n < old_p->nnotes; n++) {
		if(copy_acc_etc == NO) {
			if (Doing_tab_staff == NO) {
				CLEAR_ACCS(new_p->notelist[n].acclist);
				new_p->notelist[n].acc_has_paren = NO;
			}
			new_p->notelist[n].tie = NO;
		}
		/* noteleft should never be copied */
		new_p->notelist[n].noteleft_string = 0;
	}
}


/* add a NOTE to a list of notes off a GRPSYL */

void
add_note(grpsyl_p, pitch, acclist, octave, nticks, has_paren, bendstring)

struct GRPSYL *grpsyl_p;	/* what group to associate note with */
int	pitch;			/* 'a' to 'g' */
				/* can also be pseudo-pitches for rest, space and rpt */
char *	acclist;		/* list of accidentals */
int	octave;			/* octave for normal staffs,
				 * fret for tab staffs, uncompressibility
				 * for space pseudo-notes */
int nticks;			/* number of tick marks, for tab staffs */
int has_paren;			/* YES if accidental has parentheses (for
				 * non-tab staffs) or if fret has parentheses
				 * (for tab staffs) */
char *bendstring;		/* bend specification, or null if no bend */

{
	register int index;		/* into notelist */


	if (grpsyl_p->nnotes == 0) {
		/* first time--need to allocate */
		CALLOC(NOTE, grpsyl_p->notelist, NOTE_CHUNK);
	}

	/* see if overflowed and thus need to re-allocate more space */
	else if ((grpsyl_p->nnotes % NOTE_CHUNK) == 0) {
		REALLOC(NOTE, grpsyl_p->notelist, grpsyl_p->nnotes + NOTE_CHUNK);
		memset((void *) (& grpsyl_p->notelist[grpsyl_p->nnotes]),
			0, NOTE_CHUNK * sizeof(struct NOTE));
	}

	/* find index into notelist where we put this note */
	index = grpsyl_p->nnotes;

	/* allocate space for coordinates. The coordinates must be in a
	 * malloc-ed array and not a static array. At one point, we thought
	 * we could save a few bytes by changing back to array, but that
	 * would be a lot of work. The reason is that the
	 * coordinates are pointed to whenever there is a location tag on a
	 * note. The array must not move or else the location tag reference
	 * would be wrong. Since we qsort the notes later on, the coords would
	 * move if they were a static array rather than a malloc-ed array.
	 * In order to allow moving them, we'd have to keep track of all note
	 * location tags for much longer than we currently do, and then go
	 * back and change their values when notes are sorted. It would require
	 * finding entries in a hash table by other than their key (i.e,
	 * require a linear search or another whole indexing scheme), and
	 * also require either going back to patch up any lines, curves, and
	 * prints that used the coords, or delaying their definition by saving
	 * lots of information around. */
	CALLOCA(float, grpsyl_p->notelist[index].c, NUMCTYPE);

	grpsyl_p->notelist [ index ].letter = (short) pitch;
	COPY_ACCS(grpsyl_p->notelist [ index ].acclist, acclist);
	grpsyl_p->notelist [ index ].acc_has_paren = has_paren;
	if (Doing_tab_staff == YES && pitch >= 'a' && pitch <= 'g') {
		int fret;

		fret = octave;

		/* do error checks */
		if (acclist[0] != '\0'
				&& (acclist[0] != FONT_MUSIC ||
				(acclist[1] != C_SHARP
				&& acclist[1] != C_FLAT))) {
			yyerror("accidental on tab staff can only be # or &");
		}
		if (acclist[2] != 0) {
			yyerror("tab staff can only have a single accidental");
		}

		if (fret == NOFRET && bendstring == (char *) 0) {
			yyerror("a fret number and/or bend must be specified");
			/* put in something valid so later functions won't
			 * complain */
			fret = MINFRET;
		}
		else if (fret < 0) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
				"cannot use - on tablature");
			fret = MINFRET;
		}
		else if (fret > USE_DFLT_OCTAVE) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
				"cannot use + on tablature");
			fret = MINFRET;
		}
		else if ((fret < MINFRET || fret > MAXFRET) && fret != NOFRET) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
				"invalid fret number %d, must be %d-%d or have a bend",
				fret, MINFRET, MAXFRET);
			fret = MINFRET;
		}

		/* Cram fret and nticks in stepsup field for now. Later
		 * we will put fret in its proper place, but right now that
		 * is used for something else */
		TMP_SAVE( (&(grpsyl_p->notelist[index])), nticks, fret);

		/* cram the bend info into octave field */
		grpsyl_p->notelist [ index ].octave
				= parse_bend_string(bendstring);
	}
	else {
		if (nticks > 0) {
			yyerror("' not allowed on non-tablature staff");
		}
		grpsyl_p->notelist [ index ].octave = (short) octave;
	}
	grpsyl_p->notelist [ index ].nslurto = 0;
	grpsyl_p->notelist [ index ].tie = NO;
	grpsyl_p->notelist [ index ].tiestyle = L_NORMAL;
	grpsyl_p->notelist [ index ].tiedir = UNKNOWN;
	grpsyl_p->notelist [ index ].slurtolist = (struct SLURTO *)0;
	grpsyl_p->notelist [ index ].notesize = GS_NORMAL;
	grpsyl_p->notelist [ index ].tied_to_voice = NO_TO_VOICE;
	grpsyl_p->notelist [ index ].tied_from_other = NO;
	grpsyl_p->notelist [ index ].slurred_from_other = NO;
	grpsyl_p->notelist [ index ].noteleft_string = 0;

	/* don't need to initialize anything else in NOTE */

	(grpsyl_p->nnotes)++;
}


/* once an entire chord is gathered, release any extra space gotten for
 * notes */

void
resize_notelist(gs_p)

struct GRPSYL *gs_p;

{
	int extra;	/* how many extra NOTE structs there are */


	extra = NOTE_CHUNK - (gs_p->nnotes % NOTE_CHUNK);

	if (extra > 0 && extra < NOTE_CHUNK) {
		/* release extra NOTE space */
		REALLOC(NOTE, gs_p->notelist, gs_p->nnotes);
	}
}


/* when we get to a bar line, we have to go back and rearrange things and
 * do some error checks that couldn't be done until we had everything
 * collected. All the non-STAFF structures have to be put after the STAFFs.
 */

void
do_bar(bartype)

int bartype;

{
	struct MAINLL *mll_p;		/* to walk through list */
	static short firstbar = YES;	/* if first time this func was called */
	static int rptend_line = -1;	/* if most recent repeat* was a
					 * repeatend, this is its line number,	
i					 * else -1 */
	short v;			/* voice number */
	short found_above_for[MAXSTAFFS+1];	/* how many times this staff 
					 * was succesfully matched up with a
					 * "bm with staff below" */
	int i;				/* index though above array */
	int other_staff;		/* other staff for cross-staff beam */
	int with_above;			/* how many "bm with staff above"
					 * found on current staff */


	debug(2, "do_bar");

	/* The "restart" is a strange thing only vaguely like a regular bar,
	 * so handle it separately */
	if (bartype == RESTART) {
		restart_bar();
		return;
	}

	/* Make sure all voices are present; fill in measure space for
	 * any that are missing */
	check4missing_voices(List_of_staffs_p);

	/* attach all the lyrics to the right places */
	attach_lyrics2staffs(List_of_staffs_p);

	/* set up all the cross-staff beams */
	for (i = 1; i <= MAXSTAFFS; i++) {
		found_above_for[i] = 0;
	}
	for (mll_p = List_of_staffs_p; mll_p != (struct MAINLL *) 0;
				mll_p = mll_p->next) {

		if (mll_p->str != S_STAFF) {
			break;
		}

		with_above = 0;
		/* check every group of every voice */
		for (v = 0; v < MAXVOICES; v++) {
			struct GRPSYL *gs_p;

			/* Only check visible voices */
			if (vvpath(mll_p->u.staff_p->staffno, v+1, VISIBLE)->visible == NO) {
				continue;
			}

			for (gs_p = mll_p->u.staff_p->groups_p[v];
					gs_p != (struct GRPSYL *) 0;
					gs_p = gs_p->next) {

				/* if this is the first group in a cross-staff
				 * beam, take care to that */
				if (gs_p->beamloc == STARTITEM &&
						gs_p->beamto == CS_BELOW) {
					/* before calling chk_crossbeams, make
					 * sure the gs_p we are passing has its
					 * vno set properly, since normally
					 * that doesn't happen till later.
					 * Maybe this cross-staff stuff should
					 * be done later, but I don't want to
					 * move it without a lot of thought to
					 * make sure that wouldn't break
					 * something, so this will work for now.
					 */
					gs_p->vno = v;
					other_staff = chk_crossbeam(gs_p, mll_p);
					if (other_staff > 0) {
						(found_above_for[other_staff])++;
					}
				}
				else if (gs_p->beamloc == STARTITEM &&
						gs_p->beamto == CS_ABOVE) {
					with_above++;
				}
			}
		}
		if (found_above_for[mll_p->u.staff_p->staffno] < with_above) {
			l_yyerror(mll_p->inputfile, mll_p->inputlineno,
				"'bm with staff above' has no visible matching 'bm with staff below' (may be missing, invisible, or on wrong voice)");
		}
	}

	/* if first measure, check if "pickup" measure. If so, it doesn't
	 * count toward measure number */
	if (firstbar == YES && bartype != INVISBAR) {
		if (has_pickup() == YES) {
			/* don't count pickup measure */
			set_meas_num(Meas_num - 1, mll_p->inputfile,
						mll_p->inputlineno);
		}
		/* set flag so we won't check again */
		firstbar = NO;
	}

	/* increment measure number for rehearsal numbers */
	if (bartype != INVISBAR && Got_multirest == 0) {
			set_meas_num(Meas_num + 1, mll_p->inputfile,
						mll_p->inputlineno);
	}

	/* If there are consecutive repeatends (without a start between them),
	 * if user changes tuning related parameters, we can't find the
	 * accidents information. It's really unclear where the user wants
	 * us to go back to anyway. The repeatend? A repeatstart before that?
	 * The beginning of the song? So we disallow it.
	 */
	if (Tuning_used == YES) {
		switch (bartype) {
		case REPEATSTART:
		case RESTART:
			rptend_line = -1;
			break;
		case REPEATEND:
			if (rptend_line > 0) {
				l_yyerror(Curr_filename, yylineno,
					"second repeatend without an intervening repeatstart not allowed if there are tuning parameter settings; previous was on line %d", rptend_line);
			}
			rptend_line = yylineno;
			break;
		case REPEATBOTH:
			if (rptend_line > 0) {
				l_yyerror(Curr_filename, yylineno,
					"repeatend followed by repeatboth without an intervening repeatstart not allowed if there are tuning parameter settings; previous was on line %d", rptend_line);
			}
			rptend_line = -1;
			break;
		default:
			break;
		}
	}

	finish_bar();
}


/* Returns YES if piece begins with a pickup measure. */

int
has_pickup()
{
	int v;			/* voice index */
	struct MAINLL *mll_p;	/* to step through main list */

	/* go through each voice of each staff looking for non-space */
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
							mll_p = mll_p->next) {

		/* Measures where the entire measure is compressible space
		 * and which end with an invisbar were probably just for the
		 * purpose of kludging in a funny time signature or something
		 * like that, so we ignore those measures while checking for a
		 * pickup measure.
		 */
		if (mll_p->str == S_BAR && mll_p->u.bar_p->bartype != INVISBAR) {
			/* got through entire first measure */
			break;
		}
		if (mll_p->str != S_STAFF) {
			continue;
		}

		for (v = 0; v < MAXVOICES; v++) {
			if (mll_p->u.staff_p->groups_p[v] != 0 &&
						(mll_p->u.staff_p->groups_p[v]
						->grpcont != GC_SPACE ||
						mll_p->u.staff_p->groups_p[v]
						->uncompressible == YES) ) {
				/* Found a voice that exists,
				 * but begins with a non-space or an
				 * uncompressible space. So it's not a pickup */
				return(NO);
			}
		}
	}
	return(YES);
}

/* Add a BAR struct to the main list after insert_p, using the specified
 * BAR parameters. The address of the MAINLL struct created is returned. */

struct MAINLL *
add_bar(bartype, linetype, endingloc, endending_type, insert_p)

int bartype;			/* SINGLEBAR, DOUBLEBAR, etc. */
int linetype;			/* L_DASHED, L_DOTTED or L_NORMAL */
int endingloc;			/* NOITEM, STARTITEM, etc */
int endending_type;		/* EE_* */
struct MAINLL *insert_p;	/* insert the bar immediately after this */

{
	struct MAINLL * new_mll_p;	/* the struct to add and return */

	new_mll_p = newMAINLLstruct(S_BAR, yylineno);
	new_mll_p->u.bar_p->bartype = bartype;
	new_mll_p->u.bar_p->endingloc = endingloc;
	new_mll_p->u.bar_p->endending_type
			= (endingloc == ENDITEM ? endending_type : EE_DEFAULT);
	if (linetype != L_NORMAL) {
		if (linetype != L_DASHED && linetype != L_DOTTED) {
			yyerror("bar modifier can only be 'dashed' or 'dotted'");
		}
		else if (bartype != SINGLEBAR && bartype != DOUBLEBAR) {
			l_warning(Curr_filename, yylineno,
				"dashed/dotted only allowed on bar or dblbar; ignoring");
			linetype = L_NORMAL;
		}
	}
	new_mll_p->u.bar_p->linetype = linetype;
	insertMAINLL(new_mll_p, insert_p);
	new_mll_p->u.bar_p->timedssv_p = tssv_sort();

	/* Re-order things in the  bar,
	 * make sure they are all consistent, etc */
	do_bar(bartype);

	return(new_mll_p);
}


/* Do final cleanup in preparation for next bar of input */

static void
finish_bar()

{
	struct MAINLL *mll_p;

	/* Walk thru the list, fixing everything that is left to fix */
	for (mll_p = List_of_staffs_p; mll_p != (struct MAINLL *) 0;
				mll_p = mll_p->next) {

		if (mll_p->str == S_BAR) {
			break;
		}

		if (mll_p->str == S_STAFF) {
			/* Walk down each GRPSYL list and
			 * look for various errors and fix up things that
			 * couldn't be determined before */
			fix_grpsyl_list(mll_p);

			/* Set the visibility attribute for staff */
			mll_p->u.staff_p->visible =
					(svpath(mll_p->u.staff_p->staffno,
					VISIBLE))->visible;
		}

	}

	/* add padding etc for any rolls in the measure */
	do_rolls(Mainlltc_p);

	/* in case things are screwed up because of user input errors,
	 * reset tuplet state. */
	Tuplet_state = NOITEM;

	/* do measure level checks on "stuff" */
	meas_stuff_chk();

	/* get ready for next measure */
	List_of_staffs_p = (struct MAINLL *) 0;
	Got_multirest = 0;
	Got_group = NO;
	reset_input_style();
	lyr_new_bar();
}


/* A "restart" is a funny sort of bar.
 * We create a measure of space before it. */

static void
restart_bar()

{
	struct MAINLL *mainbar_p;	/* the restart */

	if (List_of_staffs_p != 0) {
		yyerror("restart cannot be preceded by music data");
		return;
	}
	if (Got_some_data == NO) {
		yyerror("restart cannot be used at the beginning");
		return;
	}

	/* save the bar so we can move after the staffs */
	mainbar_p = Mainlltc_p;
	/* create a "measure" of all space */
	create_staffs();
	check4missing_voices(List_of_staffs_p);
	/* move the restart BAR after the empty measure */
	unlinkMAINLL(mainbar_p);
	insertMAINLL(mainbar_p, Mainlltc_p);
	finish_bar();
}


/* walk down the linked lists of GRPSYLs and fix everything that is left to
 * fix: font/size, octaves, default timeunits, etc */

static void
fix_grpsyl_list(mainll_item_p)

struct MAINLL *mainll_item_p;

{
	register int v;
	struct STAFF *staff_p;
	struct MAINLL *mll_p;		/* for finding timed SSVs */
	struct TIMEDSSV *tssv_p;
	int numvoices;


	/* error checks */
	if (mainll_item_p->str != S_STAFF) {
		pfatal("wrong struct type passed to fix_grpsyl_list()");
	}
	staff_p = mainll_item_p->u.staff_p;


	/* verify voice is valid for current vscheme */
	numvoices = vscheme_voices(svpath(staff_p->staffno,VSCHEME)->vscheme);
	if (staff_p->groups_p[1] != (struct GRPSYL *) 0 && numvoices == 1) {
		l_yyerror(staff_p->groups_p[1]->inputfile,
				staff_p->groups_p[1]->inputlineno,
				"can't have voice 2 when vscheme=1");
	}
	if (staff_p->groups_p[2] != (struct GRPSYL *) 0 && numvoices < 3) {
		l_yyerror(staff_p->groups_p[2]->inputfile,
				staff_p->groups_p[2]->inputlineno,
				"can't have voice 3 unless vscheme is 3o or 3f");
	}

	/* Determine if there are timed SSVs to deal with */
	tssv_p = 0;
	for (mll_p = mainll_item_p; mll_p != 0; mll_p = mll_p->next) {
		if (mll_p->str == S_BAR) {
			tssv_p = mll_p->u.bar_p->timedssv_p;
			break;
		}
	}

	/* fix the GROUPS */
	for (v = 0; v < MAXVOICES; v++) {
		if (staff_p->groups_p[v] != (struct GRPSYL *) 0) {
			if (tssv_p != 0) {
				setssvstate(mainll_item_p);
			}
			fix_a_group_list(staff_p->groups_p[v],
				mll_p,
				staff_p->staffno, v + 1,
				staff_p->groups_p[v]->inputfile,
				staff_p->groups_p[v]->inputlineno,
				staff_p->mrptnum,
				tssv_p);

			/* on tablature staffs, must deal with bends */
			if (is_tab_staff(staff_p->staffno) == YES) {
				check_bends(staff_p->groups_p[v], mainll_item_p);
			}
		}
	}

	/* make sure timed SSVs are all up to date at end of measure */
	for (  ; tssv_p != 0; tssv_p = tssv_p->next) {
		asgnssv(&tssv_p->ssv);
	}

	/* then check syllables */
	for (v = 0; v < staff_p->nsyllists; v++) {
		chk_a_syl_list(staff_p->syls_p[v],
				staff_p->syls_p[v]->inputfile,
				staff_p->syls_p[v]->inputlineno);
	}
}


/* walk through a list of GRPSYLs (GROUPs, not SYLLABLEs), and fix up things
 * like octaves, etc */


static void
fix_a_group_list(grpsyllist_p, mll_p, staffno, vno, fname, lineno, mrptnum, tssv_p)

struct GRPSYL *grpsyllist_p;	/* which list to fix */
struct MAINLL *mll_p;		/* the grpsyl list hangs off of this */
int staffno;
int vno;		/* voice number */
char *fname;		/* file name for error messages */
int lineno;		/* input line number, for error messages */
int mrptnum;		/* to decide whether to run fix_string or not */
struct TIMEDSSV *tssv_p;	/* points to any timed SSVs in this measure */

{
	int font, size;
	int n;			/* index through notelist */
	RATIONAL total_time;	/* to add up all fulltimes in current voice/verse */
	short default_octave;
	struct GRPSYL *grpsyl_p;
	struct GRPSYL *gs_p;		/* for walking through grace runs */
	int nn;



	if (grpsyllist_p == (struct GRPSYL *) 0) {
		/* this is un undefined voice, so nothing to do */
		return;
	}

	debug(4, "fix_a_group_list file=%s lineno=%d staffno=%d vno=%d",
			grpsyllist_p->inputfile, grpsyllist_p->inputlineno,
			staffno, vno);

	/* will need to fix up the font and size of strings in the
	 * "with" lists, so figure out
	 * which font and size to use for that */
	font = (svpath(staffno, FONT))->font
				+ (svpath(staffno, FONTFAMILY))->fontfamily;
	size = (svpath(staffno, SIZE))->size;

	/* initialize total time for this voice in this meas */
	total_time = Zero;

	/* find default octave for this staff/voice */
	default_octave = (vvpath(staffno, vno, DEFOCT))->defoct;

	grpsyl_p = grpsyllist_p;

	Doing_tab_staff = is_tab_staff(staffno);

	/* do each group */
	for (   ; grpsyl_p != (struct GRPSYL *) 0; grpsyl_p = grpsyl_p->next) {

		/* fill in staffno and vno */
		grpsyl_p->staffno = (short) staffno;
		grpsyl_p->vno = (short) vno;

		/* Apply any timed SSVs */
		while (tssv_p != 0 && LE(tssv_p->time_off, total_time)) {
			asgnssv(&tssv_p->ssv);
			tssv_p = tssv_p->next;
			/* the default octave might have changed,
			 * so look it up again.
			 */
			default_octave = (vvpath(staffno, vno, DEFOCT))->defoct;
		}
		/* if no pitch given on first group of measure, use default
		 * pitch if 1-line staff, otherwise error */
		if (grpsyl_p->nnotes == 1 && grpsyl_p->notelist[0].letter
								== PP_NO_PITCH) {
			if (svpath(staffno, STAFFLINES)->stafflines == 1 &&
					Doing_tab_staff == NO) {
				grpsyl_p->notelist[0].letter = DFLT_PITCH;
			}
			else {
				l_yyerror(fname, lineno,
					"must have note(s) specified for first group in bar");
			}
		}

		/* make sure "with" lists are in right size/font */
		/* When groups are copied to expand measure repeats for
		 * MIDI purposes, any 'with' lists would already be in
		 * internal format, so we don't want to try to fix them,
		 * but otherwise strings need to be put in internal format */
		if (mrptnum == 0) {
			fix_strlist(grpsyl_p, font, size, fname, lineno);
			if (Doing_tab_staff == YES) {
				/* determine proper STRINGNO and FRETNO information */
				fix_frets(grpsyl_p);
			}
			else {
				/* take care of relative octaves */
				resolve_octaves(grpsyl_p, default_octave);
			}
		}

		if (svpath(staffno, STAFFLINES)->stafflines == 1
						&& grpsyl_p->nnotes > 1) {
			l_warning(fname, lineno,
				"more than one note in chord on 1-line staff");
		}

		/* put notes in order top to bottom */
		sort_notes(grpsyl_p, fname, lineno);

		/* Apply cue parameter */
		if (vvpath(staffno, vno, CUE)->cue == YES &&
						 is_mrpt(grpsyl_p) == NO) {
			grpsyl_p->grpsize = (grpsyl_p->grpvalue == GV_ZERO ? GS_TINY : GS_SMALL);
		}


		/* If this is the first grace group, search forward for its non-grace
		 * note, and if that is a cue group, make the graces tiny.
		 * Also check for mixture of cue and normal grace. We don't
		 * allow that. If user really wants that, they could get the
		 * effect by using ? on all notes on group they want cue.
		 */
		if (grpsyl_p->grpvalue == GV_ZERO &&
				(grpsyl_p->prev == 0 || grpsyl_p->prev->grpvalue != GV_ZERO)) {
			struct GRPSYL *aftergrace_p;
			int cue = 0;
			int noncue = 0;
			int make_tiny = NO;

			if (vvpath(staffno, vno, CUE)->cue == YES) {
				make_tiny = YES;
			}
			else {
				for (aftergrace_p = grpsyl_p->next;
						aftergrace_p != 0;
						aftergrace_p = aftergrace_p->next) {
					if (aftergrace_p->grpvalue != GV_ZERO) {
						if (aftergrace_p->grpsize != GS_NORMAL) {
							make_tiny = YES;
						}
						break;
					}
				}
			}
			if (make_tiny == YES) {
				for (aftergrace_p = grpsyl_p; aftergrace_p != 0;
						aftergrace_p = aftergrace_p->next) {
					if (aftergrace_p->grpvalue != GV_ZERO) {
						break;
					}
					aftergrace_p->grpsize = GS_TINY;
				}
			}
			for (aftergrace_p = grpsyl_p; aftergrace_p != 0;
					aftergrace_p = aftergrace_p->next) {
				if (aftergrace_p->grpvalue != GV_ZERO) {
					break;
				}
				else if (aftergrace_p->grpsize == GS_TINY) {
					cue++;
				}
				else {
					noncue++;
				}
			}
			if (cue > 0 && noncue > 0) {
				l_yyerror(fname, lineno,
					"cannot mix cue and non-cue grace notes");
			}
		}

		/* If group is not normal size, mark each note to match the
		 * group size (small or tiny) */
		if (grpsyl_p->grpsize != GS_NORMAL) {

			for (n = 0; n < grpsyl_p->nnotes; n++) {

				/* If it is already tiny, the user must have
				 * specified that, so leave it as is */
				if (grpsyl_p->notelist[n].notesize == GS_TINY) {
					continue;
				}
				grpsyl_p->notelist[n].notesize
							= grpsyl_p->grpsize;
			}
		}

#ifdef GRPSMALL
	/* at first we thought we should mark the whole group small if all
	 * notes were small, but later decided not, so this is ifdef-ed out */
		/* on the other hand, if all notes are explicitly marked
		 * small, make the whole group small. This is handy in case,
		 * for example, the user uses ? instead of [cue] on a
		 * single-note group */
		else if (grpsyl_p->grpcont == GC_NOTES) {
			int numsmall;	/* how many small notes */

			for (numsmall = n = 0; n < grpsyl_p->nnotes; n++) {
				if (grpsyl_p->notelist[n].notesize
								== GS_SMALL) {
					numsmall++;
				}
			}
			if (grpsyl_p->nnotes > 0 &&
						numsmall == grpsyl_p->nnotes) {
				grpsyl_p->grpsize = GS_SMALL;
			}
		}
#endif

		/* error check rests */
		if (grpsyl_p->grpcont == GC_REST) {
			if ( (grpsyl_p->prev != (struct GRPSYL *) 0)
					&& (grpsyl_p->prev->grpcont
					== GC_NOTES)
					&& (grpsyl_p->prev->grpvalue
					== GV_ZERO) ) {
				l_yyerror(fname, lineno,
					"can't have grace notes before rest");
			}

			/* We used to disallow stemdir on rests, but it could
			 * come from an earlier []... and doesn't really hurt;
			 * it doesn't matter which direction the non-existent
			 * stem is, so we silently ignore.
			 */
			if (IS_STEMLEN_KNOWN(grpsyl_p->stemlen)) {
				/* Might be just from a [ ]... on an earlier
				 * group, and doesn't really hurt anything,
				 * so silently ignore.
				 */
				grpsyl_p->stemlen = 0.0;
			}
		}

		/* error check space */
		else if (grpsyl_p->grpcont == GC_SPACE) {
			if (IS_STEMLEN_KNOWN(grpsyl_p->stemlen)) {
				grpsyl_p->stemlen = 0.0;
			}
		}

		/* error check measure repeats */
		if (is_mrpt(grpsyl_p) == YES) {
			if (grpsyl_p->nwith != 0
					|| grpsyl_p->grpvalue != GV_NORMAL
					|| grpsyl_p->headshape != HS_UNKNOWN
					|| grpsyl_p->grpsize != GS_NORMAL
					|| IS_STEMLEN_KNOWN(grpsyl_p->stemlen)
					|| grpsyl_p->stemdir != UNKNOWN
					|| grpsyl_p->slash_alt > 0) {
				l_yyerror(fname, lineno,
					"can't specify items in [ ] with measure repeat (except padding and tag)");
			}
			if (grpsyl_p->slash_alt < 0) {
				l_yyerror(fname, lineno,
					"can't specify alt with measure repeat");
			}
			if (grpsyl_p->tie == YES) {
				l_yyerror(fname, lineno,
					"can't specify tie with measure repeat");
			}
		}
		/* grace notes need special handling */
		if (grpsyl_p->grpvalue == GV_ZERO && is_mrpt(grpsyl_p) == NO) {

			/* grace notes really take no time */
			grpsyl_p->fulltime = Zero;

			/* do some error checks */
			check_grace(mll_p, grpsyl_p, fname, lineno);

			/* if more than one grace note in a row, beam them */
			if (grpsyl_p->beamloc == NOITEM
						&& grpsyl_p->basictime > 4) {

				/* if there is another grace following, will
				 * need to beam them */
				if ((grpsyl_p->next != (struct GRPSYL *)0) &&
						(grpsyl_p->next->grpvalue
						== GV_ZERO) &&
						(grpsyl_p->next->basictime
						> 4) ) {

					/* mark the first one as STARTITEM */
					grpsyl_p->beamloc = STARTITEM;

					/* mark intermediates, if any,
					 * as INITEM, and last in run
					 * of grace as ENDITEM */
					for (gs_p = grpsyl_p->next;
							gs_p->next
							!= (struct GRPSYL *) 0;
							gs_p = gs_p->next) {
						if (gs_p->next->grpvalue
								== GV_ZERO &&
								gs_p->next->basictime
								> 4) {
							gs_p->beamloc = INITEM;
						}
						else {
							gs_p->beamloc = ENDITEM;
							break;
						}
					}
					if (gs_p->next == (struct GRPSYL *) 0){
						/* grace at end of measure is
						 * illegal, but we catch that
						 * in check_grace(). So just
						 * mark the beam as ended */
						gs_p->beamloc = ENDITEM;
					}
				}
			}
		}

		/* if this group is alternating with the next, need to
		 * adjust fulltimes to be 1/2 of their current values. */
		if (grpsyl_p->slash_alt < 0 && is_mrpt(grpsyl_p) == NO) {
			/* need to error check first. First, there must
			 * be another group in this measure. Moreover, it
			 * must have the same fulltime value, and both
			 * must be either NORMAL or SMALL size.  Also, the
			 * next group can't have an alt. */
			if (grpsyl_p->next == (struct GRPSYL *) 0) {
				l_yyerror(fname, lineno,
					"cannot specify 'alt' on last group in measure");
				continue;
			}

			else if (grpsyl_p->next->grpvalue == GV_ZERO ||
					grpsyl_p->grpvalue == GV_ZERO) {
				l_yyerror(fname, lineno,
					"can't use 'alt' on grace notes");
			}

			else if ( ! EQ(grpsyl_p->fulltime,
						grpsyl_p->next->fulltime) ) {
				l_yyerror(fname, lineno,
					"groups in 'alt' pair must have identical time values");
			}

			else if (grpsyl_p->next->grpsize
					!= grpsyl_p->grpsize) {
				l_yyerror(fname, lineno,
					"can't mix normal and cue notes for 'alt'");
			}

			else if (grpsyl_p->next->slash_alt < 0) {
				l_yyerror(fname, lineno,
					"can't have 'alt' on consecutive chords");

			}

			else if (grpsyl_p->next->slash_alt > 0) {
				l_yyerror(fname, lineno,
					"can't have 'slash' on second chord of 'alt' pair");
			}

			else if (grpsyl_p->grpcont != GC_NOTES ||
					grpsyl_p->next->grpcont != GC_NOTES) {
				l_yyerror(fname, lineno,
					"'alt' must be preceded and followed by notes, not rest or space");
			}

			else {
				grpsyl_p->fulltime =
						rdiv(grpsyl_p->fulltime, Two);
				grpsyl_p->next->fulltime =
						rdiv(grpsyl_p->next->fulltime, Two);
			}

			if (grpsyl_p->stemdir != grpsyl_p->next->stemdir) {
				l_yyerror(fname, lineno,
					"'alt' pair chords must have identical stem directions");
			}

			if (grpsyl_p->beamto != CS_SAME) {
				l_yyerror(fname, lineno,
					"'alt' not allowed on cross-staff beams");
			}
		}
		else if (grpsyl_p->grpvalue != GV_ZERO &&
						grpsyl_p->slash_alt > 0 &&
						is_mrpt(grpsyl_p) == NO) {

			/* need to make sure number of slashes is valid */
			/* figure out how many actual chords are represented
			 * by the slashed chord */
			switch (grpsyl_p->basictime) {
			case 0:
				nn = 16;
				break;
			case 1:
				nn = 8;
				break;
			case 2:
				nn = 4;
				break;
			default:
				nn = 2;
				break;
			}
			/* multiply by two for each additional slash beyond
			 * the first. We shouldn't really need this IF, since
			 * if should be okay to shift by 0, but for some reason,
			 * on my system, if slash_alt is 1 and the optimizer
			 * is run on this code, the following "if (n == 0)"
			 * statement doesn't work right. It works fine if
			 * the optimizer isn't run! (YIKES!!!!) */
			if (grpsyl_p->slash_alt >  1) {
				nn = nn << (grpsyl_p->slash_alt - 1);
			}

			if (nn == 0) {
				/* shifted left into oblivion */
				yyerror("too many slashes");
			}
			/* We are okay as long as the number of dots is no
			 * more than the number of zero bits on the right end
			 * of the number. If there are more, user is trying
			 * to subdivide too much, so disallow */
			else if (drmo(nn) < grpsyl_p->dots) {
				yyerror("illegal number of slashes");
			}

			if (grpsyl_p->beamto != CS_SAME) {
				l_yyerror(fname, lineno,
					"'slash' not allowed on cross-staff beams");
			}
		}

		/* check for unclosed custom beams */
		if (grpsyl_p->next == (struct GRPSYL *) 0 &&
				(grpsyl_p->beamloc == STARTITEM ||
				grpsyl_p->beamloc == INITEM) ) {
			yyerror("missing ebm");
		}

		if (grpsyl_p->is_meas == YES && (grpsyl_p->prev != 0 ||
						grpsyl_p->next != 0) ) {
			yyerror("measure duration item must be the only thing in the measure");
		}

		/* add up the total time in the measure */
		total_time = radd(total_time, grpsyl_p->fulltime);
	}

	/* check for not equal to time signature. */
	if ( NE(total_time, Score.time) && (grpsyllist_p->is_meas == NO
			|| grpsyllist_p->next != 0) ) {
		if (LT(total_time, Score.time)) {
			l_warning(fname, lineno,
				"time in measure (%ld/%ld) does not add up to time signature; adding space at end to compensate",
				total_time.n, total_time.d);

			add_space_padding(grpsyllist_p, GS_GROUP, total_time);
		}
		else {
			l_yyerror(fname, lineno,
				"time in measure (%ld/%ld) adds up to more than the time signature",
				total_time.n, total_time.d);
		}
	}

	/* set up any beaming */
	if (needs_auto_beaming(grpsyllist_p) == YES) {
		do_beaming(grpsyllist_p, GS_NORMAL, staffno, vno);
		do_beaming(grpsyllist_p, GS_SMALL, staffno, vno);
	}
	set_alt_beams(grpsyllist_p);

	for (grpsyl_p = grpsyllist_p; grpsyl_p != (struct GRPSYL *) 0;
						grpsyl_p = grpsyl_p->next) {
		/* We only allow specifying slope for beam on first group
		 * of a beam. To be very certain of avoiding any possiblity
		 * of floating point roundoff errors,
		 * we compare for being close to NOBEAMANGLE.
		 */
		if (fabs(grpsyl_p->beamslope - NOBEAMANGLE) > 0.01) {
			if (grpsyl_p->beamloc != STARTITEM) {
				l_yyerror(fname, lineno,
					"slope can only be specified on the first chord of a beam");
			}
			else if (grpsyl_p->grpcont == GC_SPACE) {
				if (grpsyl_p->beamto != CS_SAME) {
					l_yyerror(fname, lineno,
					"on cross-staff beam, specify slope on staff with notes, not with space");
				}
				else {
					/* Don't think it should really be
					 * possible to get here... */
					l_yyerror(fname, lineno,
					"slope can't be specified on space");
				}
			}
		}
	}
}


/* Add enough space groups on the end of the given last group to
 * pad out the list by the specified amount of time. */

static void
add_space_padding(grpsyllist_p, grpsyl, used_time)

struct GRPSYL *grpsyllist_p;	/* add onto here */
int grpsyl;			/* GS_GROUP or GS_SYLLABLE */
RATIONAL used_time;		/* how much time was used; amount to pad by
				 * is Score.time minus this */

{
	struct GRPSYL *lastgrp_p;	/* end of current list */
	struct GRPSYL *spacepad_p;	/* the padding list */

	/* find the current end of list, where we need to add */
	for (lastgrp_p = grpsyllist_p; lastgrp_p->next != 0;
					lastgrp_p = lastgrp_p->next) {
		;
	}

	/* Create a space group, and link onto end of list */
	spacepad_p = newGRPSYL(grpsyl);
	spacepad_p->grpcont = GC_SPACE;
	spacepad_p->staffno = lastgrp_p->staffno;
	spacepad_p->vno = lastgrp_p->vno;
	lastgrp_p->next = spacepad_p;
	spacepad_p->prev = lastgrp_p;
	/* Create as much space as needed to fill up measure */
	filltime(spacepad_p, rsub(Score.time, used_time));
}


/* make sure all the syllable times add up to time signature */

static void
chk_a_syl_list(grpsyl_p, fname, lineno)

struct GRPSYL *grpsyl_p;	/* list to check */
char *fname;			/* file name for error messages */
int lineno;			/* input line for error messages */

{
	RATIONAL total_time;
	struct GRPSYL *gs_p;	/* for adding up time */


	total_time = Zero;

	for (gs_p = grpsyl_p; gs_p != 0; gs_p = gs_p->next) {
		total_time = radd(total_time, gs_p->fulltime);
	}

	/* check for not equal to time signature. */
	if ( LT(total_time, Score.time) ) {
		/* This used to be fatal, but now we just give a warning
		 * and add enough space. */
		l_warning(fname, lineno,
			"time in measure (%ld/%ld) does not add up to time signature; adding space at end to compensate",
			total_time.n, total_time.d);
		add_space_padding(grpsyl_p, GS_SYLLABLE, total_time);
	}
	else if (GT(total_time, Score.time) ) {
		l_yyerror(fname, lineno,
			"time in measure (%ld/%ld) adds up to more than time signature",
			total_time.n, total_time.d);
	}
}


/* resolve all the relative octaves to absolute octaves.
 * Here's the deal: if the relative octave is negative,
 * add (the negative value) to the default to get the
 * real octave. If the relative is >= USE_DFLT_OCTAVE,
 * add (octave - USE_DFLT_OCT) to the default to get the real
 * value. Otherwise the relative is the absolute.  */

static void
resolve_octaves(grpsyl_item_p, default_octave)

struct GRPSYL *grpsyl_item_p;	/* GRPSYL containing notes to resolve */
int default_octave;		/* default octave to use */

{
	register int n;		/* walk through note list */
	register int sl;	/* walk through slurtolist */
	int octave;
	struct NOTE *note_p;	/* current note */
	int prevnote_position;	/* relative to c of previous note in chord */
	int currnote_position;	/* relative to c of current note in chord */
	int inputdir;		/* UP, DOWN, or UNKNOWN */


	/* Determine input mode */
	inputdir = vvpath(grpsyl_item_p->staffno, grpsyl_item_p->vno,
						NOTEINPUTDIR)->noteinputdir;

	/* We don't allow up/down noteinputdir on chord-at-a-time input.
	 * It just seems too confusing. So for any input of that type,
	 * we silently change to  "any." */
	if (inputdir != UNKNOWN &&
			input_style(grpsyl_item_p->staffno, grpsyl_item_p->vno)
			== IS_CHORD_INPUT) {
		inputdir = UNKNOWN;
	}

	for (n = 0; n < grpsyl_item_p->nnotes; n++) {

		note_p = &(grpsyl_item_p->notelist[n]);
		octave = note_p->octave;

		if (n > 0 && inputdir != UNKNOWN) {
			/* Do error checks for inputdir of UP or DOWN */
			if (octave >= MINOCTAVE && octave <= MAXOCTAVE) {
				l_yyerror(grpsyl_item_p->inputfile,
					grpsyl_item_p->inputlineno,
					"octave number cannot be specified on subsequent notes when noteinputdir is up or down ");
				continue;
			}
			if (inputdir == UP && octave < 0) {
				l_yyerror(grpsyl_item_p->inputfile,
					grpsyl_item_p->inputlineno,
					"cannot use - on subsequent notes when noteinputdir=up");
				continue;
			}
			if (inputdir == DOWN && octave > USE_DFLT_OCTAVE) {
				l_yyerror(grpsyl_item_p->inputfile,
					grpsyl_item_p->inputlineno,
					"cannot use + on subsequent notes when noteinputdir=down");
				continue;
			}

			/* Start out assuming this note is in the same octave
			 * as the previous note. Then, if based on input
			 * direction, it has to be in at least the next
			 * octave, adjust for that. Then if the user specified
			 * pluses (for UP direction) or minuses (for DOWN)
			 * adjust by however many they specified. */
			note_p->octave =  grpsyl_item_p->notelist[n-1].octave;
			currnote_position = Letshift[grpsyl_item_p->notelist[n].letter - 'a'];
			prevnote_position = Letshift[grpsyl_item_p->notelist[n-1].letter - 'a'];
			if (inputdir == UP) {
				if (currnote_position <= prevnote_position) {
					/* has to be in higher octave */
					(note_p->octave)++;
				}
				if (octave > USE_DFLT_OCTAVE) {
					/* adjust up by number of pluses */
					(note_p->octave) += octave - USE_DFLT_OCTAVE;
				}
			}
			else {
				if (currnote_position >= prevnote_position) {
					/* has to be in lower octave */
					(note_p->octave)--;
				}
				if (octave < 0) {
					/* adjust down by number of minuses */
					(note_p->octave) += octave;
				}
			}
			if (note_p->octave < MINOCTAVE || note_p->octave > MAXOCTAVE) {
				l_yyerror(grpsyl_item_p->inputfile,
					grpsyl_item_p->inputlineno,
					"octave %d out of range (%d-%d)",
					note_p->octave, MINOCTAVE, MAXOCTAVE);
			}
		}

		/* Handle either inputdir or UNKNOWN or the first note of
		 * inputdir of UP or DOWN. */
		else if (octave < 0) {

			/* add (the negative value) to default octave */
			note_p->octave = (short)(default_octave + octave);
			if (note_p->octave < MINOCTAVE) {
				l_yyerror(grpsyl_item_p->inputfile,
					grpsyl_item_p->inputlineno,
					"octave %d out of range (%d-%d)",
					note_p->octave, MINOCTAVE, MAXOCTAVE);
			}
		}
		else if (octave >= USE_DFLT_OCTAVE) {
			/* this means 0 or more to add to default octave */
			note_p->octave = (short)(default_octave + octave
							- USE_DFLT_OCTAVE);
			if (note_p->octave > MAXOCTAVE) {
				l_yyerror(grpsyl_item_p->inputfile,
					grpsyl_item_p->inputlineno,
					"octave %d out of range (%d-%d)",
					note_p->octave, MINOCTAVE, MAXOCTAVE);
			}
		}
		else if (octave > MAXOCTAVE) {
			l_yyerror(grpsyl_item_p->inputfile,
				grpsyl_item_p->inputlineno,
				"octave %d out of range (%d-%d)",
				octave, MINOCTAVE, MAXOCTAVE);
		}

		/* also adjust any slurto */
		if (note_p->nslurto > 0) {
			for (sl = note_p->nslurto - 1; sl >= 0; sl--) {
				octave = note_p->slurtolist[sl].octave;
				if (octave < 0) {
					note_p->slurtolist[sl].octave =
							default_octave + octave;
				}
				else if (octave >= USE_DFLT_OCTAVE) {
					note_p->slurtolist[sl].octave =
							default_octave + octave
							- USE_DFLT_OCTAVE;
				}
			}
		}
	}
}


/* given a fulltime value, figure out what its basictime value was.
 * Here's the deal: First we find the power of 2 that is less than or
 * equal to the numerator of fulltime. Then we take that number over the
 * denominator of fulltime and reduce to lowest terms. The new denominator
 * is the basictime. Note that this won't work for tuplets, but we don't
 * call this for tuplets, so we're safe... */

int
reconstruct_basictime(fulltime)

RATIONAL fulltime;	/* find the basic time of this */

{
	RATIONAL newtime;	/* RATIONAL of basictime */


	if (fulltime.n < 0) {
		pfatal("negative fulltime numerator");
	}

	/* guess MAXBASICTIME and work down by powers of 2
	 * till we get <= fulltime numerator */
	for (newtime.n = MAXBASICTIME; newtime.n > fulltime.n; newtime.n >>= 1) {
		;
	}
	newtime.d = fulltime.d;
	rred( &newtime );

	/* special cases for double whole, quad, and oct */
	if (EQ(newtime, Two)) {
		return(BT_DBL);
	}
	else if (EQ(newtime, Four)) {
		return(BT_QUAD);
	}
	else if (EQ(newtime, Eight)) {
		return(BT_OCT);
	}
	else {
		return ( (int) newtime.d);
	}
}


/* Given a fulltime and basictime,
 * figure out how many dots there must have been.
 * Start with basictime as a RATIONAL. If equal to fulltime, fine, we are
 * done. Otherwise, keep adding 50% of previous value until it is equal.
 * The number of times we have to do this is the number of dots */

int
recalc_dots(fulltime, basictime)

RATIONAL fulltime;
int basictime;

{
	RATIONAL rat_basictime;
	RATIONAL halftime;	/* half of previous note or dot */
	int dots;

	/* special cases for double whole and longer */
	if (basictime == BT_DBL) {
		rat_basictime.n = 2;
		rat_basictime.d = 1;
	}
	else if (basictime == BT_QUAD) {
		rat_basictime.n = 4;
		rat_basictime.d = 1;
	}
	else if (basictime == BT_OCT) {
		rat_basictime.n = 8;
		rat_basictime.d = 1;
	}
	else {
		rat_basictime.n = 1;
		rat_basictime.d = basictime;
	}

	halftime = rmul(rat_basictime, One_half);
	for (dots = 0; LT(rat_basictime, fulltime); dots++) {
		rat_basictime = radd(rat_basictime, halftime);
		halftime = rmul(halftime, One_half);
	}
	return(dots);
}


/* make sure all the GRPSYL fields for a grace group are valid */

static void
check_grace(mll_p, grpsyl_item_p, fname, lineno)

struct MAINLL *mll_p;
struct GRPSYL *grpsyl_item_p;	/* which grace group to check */
char *fname;			/* file name for error messages */
int lineno;			/* in input, for error messages */

{
	int n;		/* index through notelist  */

	if (grpsyl_item_p->dots != 0) {
		l_yyerror(fname, lineno, "can't have dots on grace notes");
	}

	else if (grpsyl_item_p->slash_alt == 1) {
		if ( (grpsyl_item_p->prev != (struct GRPSYL *) 0
				&& grpsyl_item_p->prev->grpvalue == GV_ZERO)
				|| (grpsyl_item_p->next != (struct GRPSYL *) 0
				&& grpsyl_item_p->next->grpvalue == GV_ZERO)) {
			l_yyerror(fname, lineno,
				"slash only allowed on individual (non-beamed) grace note chord");
		}
	}

	else if (grpsyl_item_p->slash_alt > 1) {
		l_yyerror(fname, lineno, "only 1 slash allowed on grace note");
	}

	/* Can't mix quarter or longer grace notes (which don't have stems)
	 * with shorter grace notes (that do have stems and get beamed). */
	if (grpsyl_item_p->basictime <= 4 &&
			((grpsyl_item_p->prev != (struct GRPSYL *) 0
			&& grpsyl_item_p->prev->grpvalue == GV_ZERO
			&& grpsyl_item_p->prev->basictime > 4) ||
			(grpsyl_item_p->next != (struct GRPSYL *) 0
			&& grpsyl_item_p->next->grpvalue == GV_ZERO
			&& grpsyl_item_p->next->basictime > 4))) {
		l_yyerror(fname, lineno,
				"can't mix quarter and shorter grace notes");
		/* set to shorter to avoid any future error messages */
		grpsyl_item_p->basictime = 8;
	}

	if (is_tab_staff(grpsyl_item_p->staffno) == YES) {
		for (n = 0; n < grpsyl_item_p->nnotes; n++) {
			if (HASBEND(grpsyl_item_p->notelist[n])) {
				l_warning(fname, lineno,
					"can't have bend on grace note");
				break;
			}
		}
	}
	else {
		struct GRPSYL * prevgs_p;

		if ((prevgs_p = prevgrpsyl(grpsyl_item_p, &mll_p)) != 0) {
			for (n = 0; n < prevgs_p->nnotes; n++) {
				if (prevgs_p->notelist[n].is_bend) {
					l_warning(fname, lineno,
						"can't have bend on grace note");
					break;
				}
			}
		}
	}


	if (grpsyl_item_p->next == (struct GRPSYL *) 0) {
		l_yyerror(fname, lineno,
				"grace note cannot be last thing in measure");
	}

	if (grpsyl_item_p->beamloc == STARTITEM) {
		l_yyerror(fname, lineno,
				"custom beaming not allowed on grace notes");
	}

	/* Only first and last in a series of grace are allowed
	 * to have stem length specified, and then both have to be.
	 * If this one is first of a series, do the check.
	 */
	if ( (grpsyl_item_p->prev == 0
			|| grpsyl_item_p->prev->grpvalue != GV_ZERO)
			&& grpsyl_item_p->next != 0
			&& grpsyl_item_p->next->grpvalue == GV_ZERO) {
		struct GRPSYL *gs_p;
		for (gs_p = grpsyl_item_p->next;
					gs_p->next != 0 &&
					gs_p->next->grpvalue == GV_ZERO;
					gs_p = gs_p->next) {
			if (gs_p->stemlen != STEMLEN_UNKNOWN) {
				l_warning(fname, lineno,
					"stem length cannot be specified in the middle of a series of grace notes");
				gs_p->stemlen = STEMLEN_UNKNOWN;
			}
		}
		if ( (gs_p->stemlen == STEMLEN_UNKNOWN) !=
				(grpsyl_item_p->stemlen == STEMLEN_UNKNOWN) ) {
			l_warning(fname, lineno,
				"stem length must be specified on both first and last grace or on neither");
			gs_p->stemlen = STEMLEN_UNKNOWN;
			grpsyl_item_p->stemlen = STEMLEN_UNKNOWN;
		}
	}
}


/* given a GRPSYL struct, fill in the font and size for all the
 * strings in the with list */

static void
fix_strlist(gs_p, font, size, fname, lineno)

struct GRPSYL *gs_p;	/* fix all with lists in this list of GRPSYLS */
int font;		/* set their default font to this */
int size;		/* set their default size to this */
char *fname;		/* file name for error messages */
int lineno;		/* input line number for error messages */

{
	register int n;
	int staffno;
	int vno;


	if (gs_p->grpcont == GC_SPACE && gs_p->nwith > 0) {
		l_warning(fname, lineno,
				" 'with' items on space ignored");
		gs_p->nwith = 0;
		return;
	}

	for (n = 0; n < gs_p->nwith; n++) {
		staffno = gs_p->staffno;
		vno = gs_p->vno;
		(void) fix_string(gs_p->withlist[n].string,
			(vvpath(staffno, vno, WITHFONT))->withfont +
			(vvpath(staffno, vno, WITHFAMILY))->withfamily,
			(vvpath(staffno, vno, WITHSIZE))->withsize,
			fname, lineno);
	}

	for (n = 0; n < gs_p->nnotes; n++) {
		if (gs_p->notelist[n].noteleft_string != 0) {
			staffno = gs_p->staffno;
			vno = gs_p->vno;
			gs_p->notelist[n].noteleft_string =
				pad_string(gs_p->notelist[n].noteleft_string,
				TM_NONE);
			(void) fix_string(gs_p->notelist[n].noteleft_string,
				(vvpath(staffno, vno, NOTELEFTFONT))->noteleftfont +
				(vvpath(staffno, vno, NOTELEFTFAMILY))->noteleftfamily,
				(vvpath(staffno, vno, NOTELEFTSIZE))->noteleftsize,
				fname, lineno);
		}
	}

}


/* when a tuplet begins, mark tuplet state accordingly, and keep track to
 * mark subsequent notes till the end of the tuplet. */

void
begin_tuplet()

{
	if ((Tuplet_state == INITEM) || (Tuplet_state == STARTITEM)) {
		yyerror("nested tuplets not allowed");
		return;
	}

	/* just remember we are starting a tuplet. When we actual get
	 * a note, we will mark it appropriately (in link_notegroup() ) */
	Tuplet_state = STARTITEM;
}


/* once we reach the end of a tuplet, adjust all the time values */

void
end_tuplet(tupcont, tuptime, printtup, tupside, tupslope)

int tupcont;		/* number to print on top of tuplet */
RATIONAL tuptime;	/* what time value to stuff tuplet into. If 0,
			 * then use the next lower power of 2 of tupcont */
int printtup;		/* one of the PT_* values, specifying whether to print
			 * number and bracket */
int tupside;		/* side at which to print number/bracket */
double tupslope;	/* forced slope of tuplet bracket,
			 * or (usually) NOTUPLETSLOPE */

{
	RATIONAL mult_factor;	/* how much to adjust due to tuplet */
	int num_non_grace = 0;	/* how many non-grace notes in tuplet */
	int factor;		/* numerator of mult_factor */
	RATIONAL tot_time;	/* total time taken by tuplet by adding up
				 * the un-adjusted times. */
	struct GRPSYL *gs_p;	/* walk through tuplet list */



	/* future notes that come in are not in this tuplet */
	Tuplet_state = NOITEM;

	if (tupcont <= 1) {
		yyerror("tuplet number must be greater than 1");
		return;
	}

	if ( (printtup == PT_NEITHER) && (tupside != PL_UNKNOWN) ) {
		l_warning(Curr_filename, yylineno,
			"when tuplet number is not printed, tuplet side is ignored");
	}

	if (EQ(tuptime, Zero)) {
		/* this means user didn't specify, so we have to use
		 * the next lower power of two */
		if ( (tupcont & (tupcont - 1)) == 0) {
			yyerror("if tuplet number is a power of 2, tuplet time must be given as well");
			return;
		}

		/* guess MAXBASICTIME, then keep taking the next
		 * lower power of 2  until we get under the tupcont:
		 * that's the right value to use */
		for (factor = MAXBASICTIME; factor > tupcont; factor >>= 1) {
			;
		}
		/* determine what to multiple each fulltime by */
		mult_factor.n = factor;
		mult_factor.d = tupcont;
		rred(&mult_factor);
	}
	else {
		/* figure out adjustment based on user-specified amount of
		 * time the tuplet is supposed to take */
		tot_time = Zero;
		for (gs_p = Tuplet_list_p; gs_p != (struct GRPSYL *) 0;
						gs_p = gs_p->next) {
			if (gs_p->grpvalue != GV_ZERO) {
				if (gs_p->slash_alt < 0 || (gs_p->prev != 0 &&
						gs_p->prev->slash_alt < 0)) {
					/* Compensate for alt notes being
					 * twice as long */
					tot_time = radd(tot_time,
						rdiv(gs_p->fulltime, Two));
				}
				else {
					tot_time = radd(tot_time, gs_p->fulltime);
				}
			}
		}

		if (EQ(tot_time, Zero) && Errorcount > 0) {
			/* Must have been some error, like user attempting
			 * to use mested tuplets. Get out to avoid divide
			 * by zero. */
			return;
		}

		rred(&tuptime);
		rred(&tot_time);
		mult_factor = rdiv(tuptime, tot_time);
		rred(&mult_factor);

		/* if factor is <= 1/2 or >= 2, the user must have specified
		 * the wrong note lengths */
		if (LE(mult_factor, One_half) || GE(mult_factor, Two)) {
			/* Originally we allowed the summation of tuplet times
			 * to be between half and double the total tuplet time,
			 * since anything outside that the user ought to
			 * use time values that would let the total be in
			 * that range.  At least one notation book says that
			 * the summation should be between the total and double
			 * the total, with a doublet being the only exception.
			 * For that rule, the mult_facter should be
			 *    1/2 < mult_facter < 1 or mult_facter == 3/2
			 * So we were actually being generous
			 * in what we allowed.
			 * But some people want to use tuplets for cadenzas
			 * or chant, where they just want lots of notes with
			 * no more than a beam or two, even if strictly
			 * speaking mathematically, there should be more beams.
			 * So we softened to a warning, and only print it when
			 * the user didn't explicitly say they want no number
			 * printed, since if they went to the trouble of
			 * saying no number, they are probably using tuplets in
			 * a strange way.
			 */
			if (printtup != PT_NEITHER) {
				l_warning(Curr_filename, yylineno,
					"summation of tuplet time values is dubious, relative to the total tuplet time");
			}
		}
	}

	/* Store the tuplet bracket slope in the first group of the tuplet */
	if (Tuplet_list_p != 0) {
		Tuplet_list_p->tupletslope = tupslope;
	}

	/* go through the list, filling in tupcont, tuptime, and adjusting
	 * fulltime of each note */
	for (  ; Tuplet_list_p != (struct GRPSYL *) 0;
					Tuplet_list_p = Tuplet_list_p->next) {

		Tuplet_list_p->tupcont = (short) tupcont;
		Tuplet_list_p->printtup = printtup;
		Tuplet_list_p->tupside = tupside;

		if (Tuplet_list_p->grpvalue != GV_ZERO) {
			num_non_grace++;
			if (Tuplet_list_p->next != (struct GRPSYL *) 0 ||
					num_non_grace >= 1) {
				/* adjust fulltime */
				Tuplet_list_p->fulltime =
						rmul(Tuplet_list_p->fulltime,
						mult_factor);
			}
		}
		else if (num_non_grace == 0) {
			/* don't include leading grace notes in tuplet */
			Tuplet_list_p->tuploc = NOITEM;
			if (Tuplet_list_p->next != 0) {
				Tuplet_list_p->next->tuploc = STARTITEM;
			}
		}

		/* if this is the end of the list, mark end of tuplet */
		if (Tuplet_list_p->next == (struct GRPSYL *) 0) {

			if (Tuplet_list_p->grpvalue == GV_ZERO) {
				yyerror("can't end tuplet with grace note");
			}

			if(num_non_grace == 1) {
				Tuplet_list_p->tuploc = LONEITEM;
				break;
			}
			else {
				Tuplet_list_p->tuploc = ENDITEM;
			}
		}
	}
}


/* check that all voices are actually present in a measure, add a measure
 * space for any that are missing. Also make sure any measure repeats
 * are correct: if more than one voice and one is a measure repeat,
 * either all must be measure repeats, or those that aren't must be
 * measure spaces, in which case the spaces are turned into measure repeats.
 * Also fill in the mrptnum field of measure repeat STAFF
 * to be the count of repeated measures.
 */

static void
check4missing_voices(list_p)

struct MAINLL *list_p;	/* list of STAFFs */

{
	register int s;		/* walk through staff list */
	int v;			/* index through voices */
	int numvoices;		/* how many voices on current staff */


	debug(4, "check4missing_voices");

	if (list_p == (struct MAINLL *) 0) {
		pfatal("null list passed to check4missing_voices()");
	}

	/* check each staff */
	for ( s = 1; s <= Score.staffs; s++) {

		if (list_p->str != S_STAFF || list_p->u.staff_p->staffno != s) {
			if (Errorcount > 0) {
				/* if the input is garbage, we might get called
				 * with strange things in the main list. We
				 * would have already exclaimed about
				 * the errors, so don't bother to try to
				 * deal with the mess, just return */
				return;
			}
			else {
				/* if input was good, we shouldn't get here */
				pfatal("info about staff %d not in list", s);
			}
		}

		/* insert space for any missing voices */
		numvoices = vscheme_voices(svpath(s, VSCHEME)->vscheme);
		for (v = 0; v < numvoices; v++) {
			if (list_p->u.staff_p->groups_p[v]
						== (struct GRPSYL *) 0) {
				add_meas_space( &(list_p->u.staff_p->groups_p[v]),
							s, v + 1);
			}
		}

		if (list_p->next != (struct MAINLL *) 0) {
			list_p = list_p->next;
		}
	}
}


/* fill in measure space for empty voice */

void
add_meas_space(gs_p_p, staff, voice)

struct GRPSYL **gs_p_p;	/* where to put space */
int staff;
int voice;

{
	struct GRPSYL *grpsyl_p;


	grpsyl_p = newGRPSYL(GS_GROUP);
	*gs_p_p = grpsyl_p;

	grpsyl_p->staffno = (short) staff;
	grpsyl_p->vno = (short) voice;
	grpsyl_p->basictime = -1;
	grpsyl_p->is_multirest = NO;
	grpsyl_p->is_meas = YES;

	/* We are creating a line that doesn't appear in the user's input.
	 * However, if we make it an anonymous line, if this grpsyl ever
	 * gets referenced to print an error message (which it might--
	 * like if user tries to tie something into this generated measure
	 * space) the user will have no clue as to where the problem is in
	 * their input. So put in the current file/line which will point to
	 * the bar line at the end of the measure, which will at least point
	 * the user to the right general area. */
	grpsyl_p->inputfile = Curr_filename;
	grpsyl_p->inputlineno = (short) yylineno;
	grpsyl_p->grpcont = GC_SPACE;

	/* in another place, one compiler didn't fill in properly
	 * when I assigned Score.time directly in one step, so to
	 * take no chances, do numerator and denominator separately */
	grpsyl_p->fulltime.n = Score.time.n;
	grpsyl_p->fulltime.d = Score.time.d;
}


/* Fill in missing voices with values set by emptymeas parameters, if any.
 * This may get called multiple times, each time adding another voice worth
 * of emptymeas input until there are no more to do.
 * Returns which staff it generated input for, if any,
 * or 0 if all staffs have been accounted for.
 */

int
proc_emptymeas(startstaff)

int startstaff;		/* Which staff to start from when looking for
			 * emptymeas values. Each time this is called,
			 * we return which staff we were working on,
			 * so the next call can pass that to us,
			 * so we don't have to start over. */

{
	int s;		/* staff number */
	int v;		/* voice number */
	int numvoices;	/* how many voices of the current staff */
	struct MAINLL *list_p;	/* for walking through list of staffs */


	/* If we had previous generated virtual input, free it now,
	 * and put the input back to what it was before. */
	if (EM_buffer != 0) {
		FREE(EM_buffer);
		EM_buffer = 0;
		Curr_emptymeas = 0;
		del_lexbuff();
	}

	/* If already past end of staffs, nothing to do, so get out now.
	 * This simplifies some special cases elsewhere; if we know
	 * this function will be called later but we don't want it to do
	 * anything, we just make sure it will get called with MAXSTAFFS+1. */
	if (startstaff > MAXSTAFFS) {
		return(0);
	}

	/* If there are any empty voices, see if they have emptymeas
	 * set up the viewpath. If so, create a virtual input buffer
	 * containing that value, and parse it.
	 * Any remaining empty measures will be handled to old way,
	 * in check4missing_voices as called from do_bar.
	 */

	if (List_of_staffs_p == 0) {
		/* User put in no input at all for the current measure,
		 * which means they want us to deduce all voices!
		 * Okay. We create an empty list to fill in.
		 */
		create_staffs();
	}
	/* This next block of code is similar to check4missing_voices() */
	list_p = List_of_staffs_p;
	for (s = 1; s <= Score.staffs; s++) {
		if (list_p->str != S_STAFF) {
			/* Do error check like in check4missing_voices() */
			if (Errorcount > 0) {
				return(0);
			}
			else {
				pfatal("proc_emptymeas: info about staff %d not in list", s);
			}
		}
		/* We optimize somewhat by keeping track
		 * of which staff we did last time,
		 * and only do that one and later ones. */
		if (list_p->u.staff_p->staffno >= startstaff) {
			/* Check each voice. It is possible
			 * we've already processed some of the voices,
			 * but we'll just see there is nothing to
			 * do for them, and the number of voices is so small,
			 * it isn't worth optimizing. */
			numvoices = vscheme_voices(svpath(s, VSCHEME)->vscheme);
			for (v = 0; v < numvoices; v++) {
				if (list_p->u.staff_p->groups_p[v] == 0) {
					if (replace_emptymeas(list_p->u.staff_p->staffno, v + 1) == YES) {
						/* Generated some virtual input.
						 * Return to have yacc parse
						 * what we generated. */
						 return(s);
					}
				}
			}
		}

		if (list_p->next != (struct MAINLL *) 0) {
			list_p = list_p->next;
		}
	}
	return(0);
}


/* If the emptymeas parameter is set for the given staff and voice,
 * generate virtual input for it, arrange for flex to use it for input,
 * and return YES. Otherwise just return NO.
 */

static int
replace_emptymeas(staffno, vno)

int staffno;
int vno;

{
	int bufflen;		/* space needed for EM_buffer */

	if ((Curr_emptymeas = vvpath(staffno, vno, EMPTYMEAS)->emptymeas) == 0) {
		/* No emptymeas for this voice, we will fill in space later */
		return(NO);
	}
	EM_staff = staffno;
	EM_voice = vno;

	/* We need to create a buffer with
	 *  staff voice:Curr_emptymeas
	 * In addition to the replacement text from the parameter,
	 * we also need space for
	 *	- special begin/end markers around the text
	 *	to tell the parser to temporarily
	 *	go back to parsing lines of music input
	 *	- the staff, voice, and colon. We know voice can only be
	 *	1 digit long, and currently staff can be no longer than
	 *	2 digits, plus a space between them and a colon at the end.
	 *	But to be extra safe, we will print the staff as %3d,
	 *	so in case someone changes MAXSTAFFS to a 3-digit number
	 *	without realizing this code could be affected,
	 *	we will still be safe. And we have compile time checks
	 *	for going beyond 3 digit staffs or 1 digit voices.
	 *	So "SSS V:" adds up to 6 bytes.
	 *	- a newline at the end (1 byte)
	 *	- an end-of-buffer marker for flex. (2 bytes)
	 *	These will be added by the function in lex.c,
	 *	so this file doesn't have to know any more than necessary
	 * 	about flex internals.
	 */
#if (MAXSTAFFS > 999) || (MAXVOICES > 9)
#error too many staffs or voices
#endif
	bufflen = strlen(EM_begin_marker) + strlen(EM_end_marker) +
			strlen(Curr_emptymeas) + 9;
	MALLOCA(char, EM_buffer, bufflen);
	(void) sprintf(EM_buffer, "%s%3d %d:%s\n%s", EM_begin_marker,
			staffno, vno, Curr_emptymeas, EM_end_marker);

	/* Arrange for lexer to use it for input. */
	new_lexstrbuff(EM_buffer, bufflen);

	return(YES);
}


/* Returns YES if the given token is one of the funny internal tokens we
 * use to delimit expanding emptymeas parameters, NO if not.
 * This is to prevent us from accidentally printing the funny token in
 * an error message, since it would surely confuse the user.
 */

int
is_internal_token(token)

char * token;

{
	if (strcmp(token, EM_begin_marker) == 0 ||
			strcmp(token, EM_end_marker) == 0) {
		return(YES);
	}
	return(NO);
}

/* If we are processing an emptymeas parameter expansion, add that info
 * to error messages. Otherwise user could be mystified by an error being
 * reported on "bar" when the true error was many line earlier in a
 * parameters section of their input.
 */

void
emptym_err(severity)

char *severity;	/* "error" or "warning" */
 
{
	if (Curr_emptymeas != 0) {
		fprintf(stderr, "note: previous %s found while expanding emptymeas parameter value \"%s\" for staff %d voice %d\n",
			severity, Curr_emptymeas, EM_staff, EM_voice);
	}
}


/* return YES if given GRPSYL represents a measure repeat */

int
is_mrpt(gs_p)

struct GRPSYL *gs_p;

{
	if ((gs_p != 0) && (gs_p->meas_rpt_type != MRT_NONE)) {
		return(YES);
	}
	else {
		return(NO);
	}
}



/* sort the notes to be top to bottom */

static void
sort_notes(grpsyl_p, fname, lineno)

struct GRPSYL *grpsyl_p;	/* sort the notes off this GRPSYL */
char *fname;			/* file name for error messages */
int lineno;			/* input line number */

{
	register int n;
	struct NOTE *highest_p;	/* note with highest pitch on a staff */
	struct NOTE *lowest_p;	/* note with lowest pitch on a staff */
	int othervis = -1;		/* staff number of adjacent visible staff */


	if (grpsyl_p->nnotes < 2) {
		/* nothing to sort! */
		return;
	}

	/* If have cross-staff stemming, do extra error checks */
	if (vvpath(grpsyl_p->staffno, grpsyl_p->vno, VISIBLE)->visible == YES
					&& grpsyl_p->stemto != CS_SAME) {

		if (Doing_tab_staff == YES) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
			"stemming with another staff is not allowed on a tablature staff");
		}
		else if (svpath(grpsyl_p->staffno, STAFFLINES)->stafflines != 5) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
			"stemming to another staff only allowed from a 5-line staff");
		}
		if (input_style(grpsyl_p->staffno, grpsyl_p->vno)
					== IS_CHORD_INPUT) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
			"stemming to another staff not allowed on chord-at-a-time input");
		}

		if (grpsyl_p->stemto == CS_ABOVE) {
			for (othervis = grpsyl_p->staffno - 1; othervis > 0; othervis--) {
				if (svpath(othervis, VISIBLE)->visible == YES) {
					if (svpath(grpsyl_p->staffno, STAFFLINES)->stafflines != 5
						    || is_tab_staff(othervis)) {
						l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
						"for stem with staff above, preceding visible staff must be a normal 5-line staff");
					}
					break;
				}
			}
			if (othervis <= 0) {
				l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
				"can't stem with above from top visible staff");
			}
		}
		else if (grpsyl_p->stemto == CS_BELOW) {
			for (othervis = grpsyl_p->staffno + 1; othervis <= Score.staffs; othervis++) {
				if (svpath(othervis, VISIBLE)->visible == YES) {
					if (svpath(grpsyl_p->staffno, STAFFLINES)->stafflines != 5
						    || is_tab_staff(othervis)) {
						l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
						"for stem with staff below, following visible staff must be a normal 5-line staff");
					}
					break;
				}
			}
			if (othervis >= Score.staffs + 1) {
				l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
				"can't stem with below from bottom visible staff");
			}
		}

		/* The two staffs must have the same staffscale.
		 * Use floating point comparision for "close enough" */
		if (othervis > 0 && othervis <= Score.staffs) {
			if (fabs(svpath(othervis,STAFFSCALE)->staffscale -
			svpath(grpsyl_p->staffno, STAFFSCALE)->staffscale) > 0.001) {
				l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
				"staff being stemed to must have same staffscale");
			}
		}
	}

	/* If this group is stemmed to another staff,
	 * we have to make sure the notes on the two staffs don't overlap,
	 * since we can't handle duplicate notes,
	 * and it's kind of silly anyway.
	 * At this point, the stemto_idx will be at the first
	 * "other staff" note. In the case of stem with above,
	 * we need to adjust that to be at the last normal staff group,
	 * because that is the convention we use. In any case, once
	 * we verify there is no overlap, we can go ahead and sort the
	 * group as usual, since we know that will end up sorting each
	 * staff's notes properly.
	 */
	if (grpsyl_p->stemto == CS_ABOVE) {
		/* Find highest note on normal staff */
		if (grpsyl_p->stemto_idx == 0) {
			/* no notes at all on normal staff */
			highest_p = 0;
			n = 0;
		}
		else {
			highest_p = &(grpsyl_p->notelist[0]);
			for (n = 1; n < grpsyl_p->stemto_idx; n++) {
				if (notecomp(&(grpsyl_p->notelist[n]), highest_p) > 0) {
					highest_p = &(grpsyl_p->notelist[n]);
				}
			}
		}
		/* Find lowest note on above staff */
		if (grpsyl_p->stemto_idx == grpsyl_p->nnotes) {
			/* Actually, we don't currently allow this case
			 * (blocked in parsing code),
			 * but if we ever do, this code should handle it...
			 */
			lowest_p = 0;
		}
		else {
			lowest_p = &(grpsyl_p->notelist[n]);
			for ( ; n < grpsyl_p->nnotes; n++) {
				if (notecomp(&(grpsyl_p->notelist[n]), lowest_p) < 0) {
					lowest_p = &(grpsyl_p->notelist[n]);
				}
			}
		}
		/* Make sure there is no overlap */
		if (highest_p != 0 && lowest_p != 0 &&
					notecomp(highest_p, lowest_p) <= 0) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
			"the 'with above' notes are not allowed to overlap with notes on the normal staff");
		}

		/* With CS_ABOVE, the index is supposed to point to the last on
		 * the normal, so adjust to do that. */
		grpsyl_p->stemto_idx = grpsyl_p->nnotes - grpsyl_p->stemto_idx - 1;
	}
	else if (grpsyl_p->stemto == CS_BELOW) {
		/* Do analogous for below */
		/* Find lowest note on normal staff */
		if (grpsyl_p->stemto_idx == 0) {
			/* no notes at all on normal staff */
			lowest_p = 0;
			n = 0;
		}
		else {
			lowest_p = &(grpsyl_p->notelist[0]);
			for (n = 1; n < grpsyl_p->stemto_idx; n++) {
				if (notecomp(&(grpsyl_p->notelist[n]), lowest_p) < 0) {
					lowest_p = &(grpsyl_p->notelist[n]);
				}
			}
		}
		/* Find highest note on below staff */
		if (grpsyl_p->stemto_idx == grpsyl_p->nnotes) {
			highest_p = 0;
			n = 0;
		}
		else {
			highest_p = &(grpsyl_p->notelist[n]);
			for (n++ ; n < grpsyl_p->nnotes; n++) {
				if (notecomp(&(grpsyl_p->notelist[n]), highest_p) > 0) {
					highest_p = &(grpsyl_p->notelist[n]);
				}
			}
		}
		/* Make sure there is no overlap */
		if (highest_p != 0 && lowest_p != 0 &&
					notecomp(highest_p, lowest_p) <= 0) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
			"the 'with below' notes are not allowed to overlap with notes on the normal staff");
		}
	}

	/* sort top to bottom */
	qsort( (char *) grpsyl_p->notelist, (unsigned int) grpsyl_p->nnotes,
					sizeof (struct NOTE), notecomp);

	/* now that they are sorted, check for duplicates. */
	for (n = 0; n < grpsyl_p->nnotes - 1; n++) {
		if (notecomp(&(grpsyl_p->notelist[n]),
					&(grpsyl_p->notelist[n+1])) == 0) {
			/* For voice-at-a-time, duplicate is an error.
			 * For chord-at-a-time, we merge them, except that
			 * for tablature, if the frets don't match we
			 * can't merge them. */
			if (input_style(grpsyl_p->staffno, grpsyl_p->vno)
					== IS_VOICE_INPUT ||
					(Doing_tab_staff == YES &&
					grpsyl_p->notelist[n].FRETNO !=
					grpsyl_p->notelist[n+1].FRETNO)
					) {
				if (Doing_tab_staff == YES) {
					struct STRINGINFO *strinfo_p;

					strinfo_p = &(svpath(grpsyl_p->staffno,
						STAFFLINES)->strinfo[ (int)
						grpsyl_p->notelist[n].STRINGNO]);
					l_yyerror(fname, lineno,
						"string %s occurred more than once in a single chord",
						format_string_name(
						strinfo_p->letter,
						strinfo_p->accidental,
						strinfo_p->nticks));
				}
				/* Multiple rests or space only get through to
				 * here if one of them was on the non-normal
				 * staff of CSS, which would already have had
				 * an error printed, and beside,
				 * we don't want to print out the pitch r4
				 * or s4 is used more than once, so we
				 * only give error for duplicate notes. */
				else if (grpsyl_p->notelist[n].letter
						!= PP_REST
						&& grpsyl_p->notelist[n].letter
						!= PP_SPACE) {
					l_yyerror(fname, lineno,
						"pitch %c%d occurred more than once in a single chord",
						grpsyl_p->notelist[n].letter,
						grpsyl_p->notelist[n].octave);
				}
			}
			else {
				merge_dup_notes(grpsyl_p, n);
				/* in case there was more than one duplicate,
				 * arrange to check this same one again,
				 * against the group that has taken the place
				 * of the removed group (if any) */
				n--;
			}
		}
	}
}


/* Compare notes. Return 1 if first is lower, -1 if higher, 0 if same.
 * For tablature, this just compares the strings; for non-tablature,
 * it just compares pitch and octave, not counting accidentals. */

int
notecomp(item1_p, item2_p)

#ifdef __STDC__
const void *item1_p;		/* the notes to compare */
const void *item2_p;
#else
char *item1_p;		/* the notes to compare */
char *item2_p;
#endif

{
	struct NOTE *note1_p;
	struct NOTE *note2_p;

	/* cast to proper type */
	note1_p = (struct NOTE *) item1_p;
	note2_p = (struct NOTE *) item2_p;

	if (Doing_tab_staff == YES) {
		/* for tablature, just need to compare the string numbers. */
		if (note1_p->STRINGNO < note2_p->STRINGNO) {
			return(-1);
		}
		else if (note1_p->STRINGNO > note2_p->STRINGNO) {
			return(1);
		}
		else {
			return(0);
		}
	}

	/* first compare octaves */
	if ( note1_p->octave < note2_p->octave) {
		return(1);
	}

	if (note1_p->octave > note2_p->octave) {
		return(-1);
	}
	
	/* if same octaves, compare pitches */
	if (Letshift[note1_p->letter - 'a'] < Letshift[note2_p->letter - 'a']) {
		return(1);
	}

	if (Letshift[note1_p->letter - 'a'] > Letshift[note2_p->letter - 'a']) {
		return(-1);
	}

	return(0);
}


/* Make sure data ends with some kind of bar line (which can optionally
 * be followed by blocks). Start at end of  main list and go backwards.
 * If we hit a S_BAR, then all is well.
 * If we hit a S_STAFF first or beginning of list, we have a problem.
 * Also does error checks on any FEED at the end of the piece.
 * It does a similar check on any FEED at the beginning. (It doesn't
 * really fit the name of the function, but keeps the checks together.)
 * Similarly, checks to unclosed samescore or samepage zones.
 */

void
check4barline_at_end()

{
	struct MAINLL *list_p;	/* walk through main list */
	int saw_block = NO;	/* if saw an S_BLOCKHEAD */


	/* first check for FEED at the beginning */
	for (list_p = Mainllhc_p; list_p != (struct MAINLL *) NULL;
				list_p = list_p->next) {
		if (list_p->str == S_STAFF) {
			/* no user-supplied feed at beginning */
			break;
		}
		else if (list_p->str == S_FEED) {
			/* rightmargin applies to previous feed,
			 * but there is no previous feed to the first score */
			if (list_p->u.feed_p->rightmargin >= 0.0) {
				l_warning(list_p->inputfile, list_p->inputlineno,
					"rightmargin on newscore at beginning of piece is ignored");
			}
			/* Note: we probably ought to give a
			 * warning for pagefeed here and ignore it,
			 * to be like at the end of a piece.
			 * But this was inadvertently allowed in the past
			 * (it produces a blank page),
			 * so we leave it that way
			 * for backward compatibility.
			 */
		}
	}

	/* Check for unclosed samescore/samepage zones */
	check_same_ended();

	for (list_p = Mainlltc_p; list_p != (struct MAINLL *) NULL;
				list_p = list_p->prev) {

		if (list_p->str == S_STAFF) {
			/* User failed to include an ending bar line.
			 * This used to be fatal, but now we are nicer,
			 * and supply a bar with a warning.
			 * We don't know what kind of bar line they intended,
			 * but it's end of file, so we use ENDBAR.
			 * Most likely we should use endingloc of NOITEM,
			 * but if they were inside an ending, we should use
			 * ENDITEM instead, so check for that.
			 */
			struct MAINLL *mll_p; /* to find preceding bar */
			int endingloc;

			endingloc = NOITEM;
			for (mll_p = list_p->prev; mll_p != 0;
							mll_p = mll_p->prev) {
				if (mll_p->str == S_BAR) {
					if (mll_p->u.bar_p->endingloc == INITEM
							|| mll_p->u.bar_p->endingloc
							== STARTITEM) {
						endingloc = ENDITEM;
					}
					break;
				}
			}
			warning("ending bar line missing; supplying an endbar");
			list_p = add_bar(ENDBAR, L_NORMAL, endingloc, EE_DEFAULT, list_p);
		}

		if (list_p->str == S_BAR) {
			/* Either we found the user's final BAR or the one
			 * we supplied that they forget to supply.
			 * If user forgot to end an ending, do it for them */
			if (list_p->u.bar_p->endingloc == INITEM) {
				list_p->u.bar_p->endingloc = ENDITEM;
			}
			else if (list_p->u.bar_p->endingloc == STARTITEM) {
				l_yyerror(list_p->inputfile, list_p->inputlineno,
					"can't begin ending on final bar line");
			}

			if (list_p->u.bar_p->bartype == RESTART) {
				l_yyerror(list_p->inputfile, list_p->inputlineno,
					"final bar line cannot be a restart");
			}
			/* If we added a "measnum=every N" type rehearsal mark,
			 * we don't really want that on the final bar,
			 * so remove it. At the time we added it, we didn't
			 * yet know it was going to be the final bar. */
			if (list_p->u.bar_p->reh_type == REH_BAR_MNUM) {
				list_p->u.bar_p->reh_type = REH_NONE;
				FREE(list_p->u.bar_p->reh_string);
				list_p->u.bar_p->reh_string = 0;
			}
			return;
		}

		else if (list_p->str == S_BLOCKHEAD) {
			saw_block = YES;
		}
		else if (list_p->str == S_FEED) {
			if (list_p->u.feed_p->leftmargin >= 0.0) {
				l_warning(list_p->inputfile, list_p->inputlineno,
					"leftmargin on newscore at end of piece is ignored");
			}
		}
	}

	/* if we are here, we went all through the list */
	if (saw_block == NO) {
		yyerror("no music data or blocks found in input");
	}
}


/* When user wants a multi-rest, create a STAFF for each staff. Then for
 * each of them, fill in a GRPSYL with basictime as the negative of the
 * number of measures, and fulltime as the length of a measure. Also set
 * the is_multirest flag to distinguish from the BT_* values. Attach this
 * GRPSYL as the only item off of voice 1, and to other voices as well,
 * if there are other voices and we are doing MIDI. */

void
add_multirest(nummeas)

int nummeas;		/* how many measures in the multi-rest */

{
	register int s;		/*staff index */
	struct GRPSYL *new_p;	/* newly allocated GRPSYL for multirest */
	int v;			/* voice index */
	int numvoices;


	/* if already have notes in this measure, user goofed */
	if (Got_group == YES) {
		report_mix_error();
		return;
	}

	if (Got_multirest == 1) {
		yyerror("can't have consecutive multirests (maybe missing bar?)");
		return;
	}

	/* we could allow up to 32767 and still fit in a short, but huge
	 * numbers can lead to rational overflow in midi, and who in their
	 * right mind would want that many measures of multirest anyway? */
	if (rangecheck(nummeas, 2, MAXMULTINUM, "multirest measures") == NO) {
		return;
	}

	/* create all the staffs and fill them in */
	create_staffs();
	for (s = 1; s <= Score.staffs; s++) {
		numvoices = vscheme_voices(svpath(s, VSCHEME)->vscheme);
		for (v = 0; v < numvoices; v++) {
			Staffmap_p[s]->u.staff_p->groups_p[v] = new_p
						= newGRPSYL(GS_GROUP);
			new_p->grpcont = GC_REST;
			new_p->basictime = -nummeas;
			new_p->is_multirest = YES;
			new_p->fulltime = Score.time;
		}
	}

	Got_multirest = 1;
	Got_some_data = YES;

	/* update measure numbers for rehearsal mark and stuff use */
	set_meas_num(Meas_num + nummeas, Curr_filename, yylineno);
	multi_stuff(nummeas);
}


/* this function gets called when we discover user has mixed multi-rest and
 * music data in a single measure. If we've already reported this on the
 * current measure, Got_multirest will be 2, so we just return,
 * otherwise report it and set Got_multirest to 2 */

void
report_mix_error()

{
	if (Got_multirest != 2) {
		yyerror( "can't mix music data and multi-rest in same measure");
		Got_multirest = 2;
	}
}


/* Recursively free a list of GRPSYLs. If this is a GS_GROUP,
 * also free the withlist and notelist if they exist,
 * and anything hanging off the notelist. If this is a GS_SYLLABLE, anything
 * that is hanging of the grpsyl will NOT be freed.
 */

void
free_grpsyls(gs_p)

struct GRPSYL *gs_p;		/* what to free */

{
	int n;			/* to index through lists */

	if (gs_p == (struct GRPSYL *) 0) {
		/* end of recursion */
		return;
	}

	if (gs_p->grpsyl == GS_GROUP) {
		free_notelist(gs_p);

		/* free the withlist */
		for (n = 0; n < gs_p->nwith; n++) {
			if (gs_p->withlist[n].string != (char *) 0) {
				FREE(gs_p->withlist[n].string);
			}
		}
		if (gs_p->withlist != 0 && gs_p->nwith > 0) {
			FREE(gs_p->withlist);
		}
	}

	free_grpsyls(gs_p->next);
	FREE(gs_p);
}


/* Free things off of NOTEs in the notelist, if any, plus the list itself */

void
free_notelist(gs_p)

struct GRPSYL *gs_p;

{
	int n;

	for (n = 0; n < gs_p->nnotes; n++) {
		/* free coordinate array, if any */
		if (gs_p->notelist[n].c != (float *) 0) {
			FREE(gs_p->notelist[n].c);
		}

		/* free any slurto lists */
		if (gs_p->notelist[n].slurtolist != (struct SLURTO *) 0) {
			FREE(gs_p->notelist[n].slurtolist);
		}
	}

	/* free the notelist itself, if any */
	if (gs_p->notelist != (struct NOTE *) 0 && gs_p->nnotes > 0) {
		FREE(gs_p->notelist);
	}
}


/* add a slurto note to list */

void
add_slurto(grpsyl_p, pitch, octave, note_index, slurstyle)

struct GRPSYL *grpsyl_p;	/* what group to associate slurto with */
int	pitch;			/* 'a' to 'g' */
int	octave;
int	note_index;		/* which note in the chord to slur from */
int	slurstyle;		/* L_NORMAL, L_DOTTED, or L_DASHED */

{
	struct NOTE *note_p;	/* note to attach to */


	if (note_index < 0) {
		/* user tried to slur from a rest or something like that */
		yyerror("no note to slur from");
		return;
	}

	note_p = &(grpsyl_p->notelist [ note_index ]);
	if (note_p->nslurto == 0) {
		/* first time--need to allocate */
		MALLOC(SLURTO, note_p->slurtolist, 1);
	}

	/* else re-allocate more space */
	else {
		REALLOC(SLURTO, note_p->slurtolist, note_p->nslurto + 1);
	}

	note_p->slurtolist [ note_p->nslurto ].letter = (char) pitch;
	note_p->slurtolist [ note_p->nslurto ].octave = (short) octave;
	note_p->slurtolist [ note_p->nslurto ].slurstyle = (short) slurstyle;
	/* Actual slurdir and slurred_to_voice, if any, will be set later,
	 * in set_slurdir(). We set defaults for now. */
	note_p->slurtolist [ note_p->nslurto ].slurdir = (short) UNKNOWN;
	note_p->slurtolist [ note_p->nslurto ].slurred_to_voice = NO_TO_VOICE;

	(note_p->nslurto)++;
}


/* At the beginning of a <> which indicates slurs,
 * save which slurtolist index we are are on.
 * When at the end of the slur, we will discover if the user specified a
 * bend direction for the slurs, and if so, have to go back and fill in
 * that direction for all the slurtolist items from the saved index to
 * the end of the list. This is needed since user may have several <>
 * items with different directions, e.g.,
 *	a<c+>up<g>down<e>
 */

void
begin_slur(grpsyl_p, note_index)

struct GRPSYL *grpsyl_p;
int note_index;

{
	if (grpsyl_p == 0 || note_index < 0 || note_index >= grpsyl_p->nnotes) {
		/* must have been bad input */
		return;
	}
	Slur_begin = grpsyl_p->notelist[note_index].nslurto;
}

/* At the end of a slur, either <> style or "slur" keyword style,
 * go back and fill in the bulge direction. If note_index is -1,
 * this is a "slur" that applies to all notes of the group, and
 * there should be one slurto item on each note.
 */

void
set_slurdir(grpsyl_p, note_index, dir, slurred_to_voice)

struct GRPSYL *grpsyl_p;
int note_index;		/* which note's slurtolist to use */
int dir;		/* UP, DOWN, UNKNOWN */
int slurred_to_voice;	/* voice number, or NO_TO_VOICE (current voice) */

{
	int n;
	int i;

	if (note_index == -1) {
		for (n = 0; n < grpsyl_p->nnotes; n++) {
			if (grpsyl_p->notelist[n].nslurto >= 1) {
				grpsyl_p->notelist[n]
					.slurtolist[grpsyl_p->notelist[n].nslurto-1]
					.slurdir = dir;
				grpsyl_p->notelist[n]
					.slurtolist[grpsyl_p->notelist[n].nslurto-1]
					.slurred_to_voice = slurred_to_voice;
			}
			/* If no slurto, should already have
			 * error generated elsewhere */
		}
	}
	else {
		for (i = Slur_begin; i < grpsyl_p->notelist[note_index].nslurto; i++) {
			grpsyl_p->notelist[note_index].slurtolist[i].slurdir = dir;
			grpsyl_p->notelist[note_index].slurtolist[i].slurred_to_voice
					= slurred_to_voice;
		}
	}
}


/* given a bend string, return what should be crammed into the
 * octave field to save this info, using the TABOCT macro. If the string
 * is incomprehensible, print error, and do what we can. */

/* define states for the simple parser */
#define DOING_INTEGER		1
#define DOING_NUMERATOR		2
#define DOING_DENOMINATOR	3
#define GOT_ERROR		4

static int
parse_bend_string(bendstring)

char *bendstring;

{
	int intpart = 0;	/* integer part of bend */
	int num = 0, den = 1;	/* numerator and denominator
				 * of fractional part of bend */


	/* if null string, easy to parse */
	if (bendstring == (char *) 0) {
		return (TABOCT(0, 0, 0));
	}

	/* string is in internal format, so skip first 2 bytes, plus any
	 * leading white space */
	for (bendstring += 2; *bendstring != '\0'; bendstring++) {
		if (isgraph(*bendstring)) {
			break;
		}
	}

	/* the text "full" is a special case */
	if (strcmp(bendstring, "full") == 0) {
		intpart = 1;
	}
	else {
		/* the text better be a number and/or fractional part,
		 * so parse that */
		int state;	/* DOING_INTEGER, DOING_NUMERATOR,
				 * DOING_DENOMINATOR, or GOT_ERROR */

		for (state = DOING_INTEGER;
				*bendstring != '\0' && state < GOT_ERROR;
				bendstring++) {

			if (isdigit(*bendstring)) {
				/* add to the correct item */
				switch(state) {
				case DOING_INTEGER:
					intpart = (intpart * 10) + (*bendstring - '0');
					break;
				case DOING_NUMERATOR:
					num = (num * 10) + (*bendstring - '0');
					break;
				case DOING_DENOMINATOR:
					den = (den * 10) + (*bendstring - '0');
					break;
				default:
					pfatal("bad state in parse_bend_string");
					break;
				}
			}

			/* handle white space */
			else if (*bendstring == ' ' || *bendstring == '\t') {
				if (state == DOING_INTEGER) {
					/* end of integer part */
					state = DOING_NUMERATOR;
				}
				/* ignore any other white space */
			}

			else if (*bendstring == '/') {
				/* starting parsing of denominator */
				den = 0;

				switch (state) {

				case DOING_INTEGER:
					/* oops. what we thought was the integer
					 * part was really the numerator of the
					 * fractional part. Adjust accordingly. */
					num = intpart;
					intpart = 0;
					state = DOING_DENOMINATOR;
					break;

				case DOING_NUMERATOR:
					state = DOING_DENOMINATOR;
					break;

				case DOING_DENOMINATOR:
					yyerror("more than one / in bend string");
					state = GOT_ERROR;
					break;

				default:
					pfatal("bad state in parse_bend_string");
					break;
				}
			}

			else {
				yyerror("invalid character in bend string");
				state = GOT_ERROR;
			}
		}
	}

	/* make sure we can fit them in our cramped space */
	(void) rangecheck(intpart, MINBENDINT, MAXBENDINT,
					"integer part of bend");
	(void) rangecheck(num, MINBENDNUM, MAXBENDNUM,
					"numerator of bend fraction");
	(void) rangecheck(den, MINBENDDEN, MAXBENDDEN,
					"denominator of bend fraction");

	return (TABOCT(intpart, num, den));
}


/* given a tablature GRPSYL, look up the proper string number based on the
 * letter/accidental that is currently there, and fill in the STRINGNO and
 * FRETNO fields with their correct values. The slurto items also get
 * their string letter/accidental values translated to string number. */

static void
fix_frets(grpsyl_item_p)

struct GRPSYL *grpsyl_item_p;

{
	int n;			/* index through note list */
	int s;			/* index through slurto */
	struct NOTE *note_p;	/* current note */
	struct STRINGINFO *string_info_p;	/* string translation table,
				 * to map between pitch/accidental and
				 * string number */
	int nstrings;		/* how many strings there are */


	/* find appropriate string mapping table and its size */
	string_info_p = Staff[grpsyl_item_p->staffno - 1].strinfo;
	nstrings = Staff[grpsyl_item_p->staffno - 1].stafflines;

	/* for each note, translate to string number, and put fret in its
	 * proper field */
	for (n = 0; n < grpsyl_item_p->nnotes; n++) {
		/* get pointer to the current note in the group */
		note_p = &(grpsyl_item_p->notelist[n]);

		/* doing mapping from pitch/accidental to string number */
		note_p->STRINGNO = string_number(string_info_p, nstrings,
			note_p->letter, standard_acc(note_p->acclist),
			TMP_NTICKS(note_p));

		/* fill in the correct fret number, now that its space is
		 * available */
		note_p->FRETNO = TMP_FRET(note_p);
		note_p->stepsup = 0;	/* no longer need this temp space */

		/* also fix up any slurto items */
		for (s = 0; s < note_p->nslurto; s++) {
			if (note_p->slurtolist[s].octave == USE_DFLT_OCTAVE) {
				note_p->slurtolist[s].STRINGNO
							= note_p->STRINGNO;
			}
		}
	}
}


/* look up a letter/acc/nticks combination in the string_info_p table of
 * size nstrings, and return the index of the string that matched. If none
 * match, print an error and return 0. However, if the pitch is PP_NO_PITCH,
 * don't print the error, because an error would already have been printed
 * earlier, and it just adds confusion to print another one.
 * Also don't complain about rest and space, because they are legal.
 */

static int
string_number(string_info_p, nstrings, letter, accidental, nticks)

struct STRINGINFO *string_info_p;	/* string translation table */
int nstrings;				/* number of entries in table */
int letter;
int accidental;
int nticks;				/* number of ' marks */

{
	register int i;

	if (string_info_p == (struct STRINGINFO *) 0) {
		pfatal("null stringinfo");
	}

	/* look through string_info list for a matching string */
	for (i = 0; i < nstrings; i++) {
		if (string_info_p[i].letter == letter &&
				string_info_p[i].accidental == accidental &&
				string_info_p[i].nticks == nticks) {
			/* found it. return its index */
			return(i);
		}
	}

	/* If letter is PP_NO_PITCH, that means user failed to put a letter
	 * for the first group in the measure. We would have already
	 * exclaimed about that in fix_a_grpsyl_list(), so rather than
	 * generate lots more messages due to the same error,
	 * just silently return the default string of 0.
	 * Rests, spaces, and rpts are also fine--they will get converted to
	 * groups later. Otherwise the error message is needed. */
	if (letter >= 'a' && letter <= 'g') {
		/* no match found */
		l_yyerror(Curr_filename, yylineno, "no %s string",
				format_string_name(letter, accidental, nticks));
	}

	/* use string 0 as default, to have something to return */
	return(0);
}


/* given a string number and staff number, return the text representation
 * of that string. Returns pointer to static area. */

char *
stringname(stringno, staffno)

int stringno;
int staffno;

{
	struct STRINGINFO *stringinfo_p;


	/* make sure we have a valid string number */
	if (stringno < 0 || stringno >= Staff[staffno - 1].stafflines) {
		pfatal("string number %d out of range", stringno);
	}

	/* get pointer to proper table entry for this string */
	stringinfo_p = & (Staff[staffno - 1].strinfo[stringno] );

	/* return text representation */
	return(format_string_name(stringinfo_p->letter,
			stringinfo_p->accidental,
			stringinfo_p->nticks));
}


/* to print some ticks, print as much of this string as necessary */
static char tickstring[MAXTICKS + 1];

/* given string letter, accidental, and number of tick marks, return
 * pointer to static area that contains the text representation of the
 * string name */

char *
format_string_name(letter, accidental, nticks)

int letter;
int accidental;
int nticks;

{
	static char name[MAXTICKS + 3];	/* this is what will get returned */
	int tickoffset = 1;	/* where in name that ticks begin */
	int i;

	/* fill in string letter plus accidental if any */
	name[0] = (char) letter;
	if (accidental != 0) {
		name[1] = (char) accidental;
		tickoffset++;
	}

	/* first time through, fill in maximum number of ticks. This makes
	 * it possible to have the right number, even if MAXTICKS changes
	 * some day */
	if (tickstring[0] == '\0') {
		for (i = 0; i < MAXTICKS; i++) {
			tickstring[i] = '\'';
		}
	}

	/* add appropriate number of ticks to name */
	(void) strcpy(name + tickoffset, tickstring + strlen(tickstring) - nticks);

	return(name);
}


/* given a list of grpsyls, check that every non-prebend bend has a previous
 * groups which contains the same string as the bend is on. Also disallow
 * slashes on groups that just have a bend. */

static void
check_bends(gs_p, mll_p)

struct GRPSYL *gs_p;
struct MAINLL *mll_p;

{
	int n;				/* notelist index */
	int ns;				/* slurto index */
	struct GRPSYL *prevgrp_p;	/* previous group in same staff/voice */
	struct NOTE *prevnote_p;	/* note in previous group */
	int has_bend, has_prebend;	/* YES or NO */
	struct MAINLL *nmll_p;


	/* check each group in the list */
	for (  ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {

		/* check each note in the group */
		has_bend = has_prebend = NO;
		for (n = 0; n < gs_p->nnotes; n++) {

			if (HASBEND(gs_p->notelist[n])
					&& gs_p->notelist[n].FRETNO == NOFRET) {

				/* this is a non-prebend bend, so it needs to
				 * be checked. Find the previous group */
				has_prebend = YES;
				nmll_p = mll_p;
				if ((prevgrp_p = prevgrpsyl(gs_p, &nmll_p))
						!= (struct GRPSYL *) 0) {

					/* find the matching note (string),
					 * If not found, the function will
					 * print an error. */
					prevnote_p =
						find_matching_note(prevgrp_p,
						0, gs_p->notelist[n].letter,
						gs_p->notelist[n].FRETNO,
						gs_p->notelist[n].octave,
						"bend");

					if (prevnote_p == (struct NOTE *) 0) {
						continue;
					}

					/* previous group not allowed to have
					 * slide other than inward nowhere */
					for (ns = 0; ns < prevnote_p->nslurto;
									ns++) {
						switch (prevnote_p->slurtolist
								[ns].octave) {
						case IN_UPWARD:
						case IN_DOWNWARD:
							/* these are okay */
							break;
						default:
							l_yyerror(gs_p->inputfile,
							gs_p->inputlineno,
							"can't have bend on note following a slide");
							break;
						}
					}
				}
				else {
					yyerror("can't do bend--no previous group");
					continue;
				}

				/* slashes are not allowed on bend groups */
				if (gs_p->slash_alt != 0) {
					yyerror("slash not allowed with bend");
					gs_p->slash_alt = 0;
				}
			}
			else if (HASBEND(gs_p->notelist[n])
					&& gs_p->notelist[n].FRETNO != NOFRET) {
				has_bend = YES;
			}
		}

		if (has_bend == YES && has_prebend == YES) {
			yyerror("mixture of prebend and bend not allowed on the same chord");
		}
	}
}


/*
 * When the input includes additive time values like 2+8 or 2.+32
 * on groups, we need to create a group for each extra time value.
 * For notes, the groups are all tied together to make them effectively
 * a single group. If there are any subtractions in the list of time values,
 * we have no way to determine what time values the user really wants,
 * so we keep using as big as possible (including dots) until we get enough.
 * Should be called with the group currently last in the GRPSYL list.
 * Returns the (possibly new) last group.
 */

struct GRPSYL *
expandgrp(grpsyl_p, timelist_p)

struct GRPSYL *grpsyl_p;	/* the first group; pattern to clone */
struct TIMELIST *timelist_p;	/* the list of additional time values */

{
	struct GRPSYL *gs_p;	/* group being processed */
	struct TIMELIST *tl_p;	/* index through list to check for subtracts */
	int had_neg;		/* YES if there was at least one subtraction */
	RATIONAL totaltime;	/* all times added together */
	int n;			/* index through notelist */


	if (timelist_p == 0) {
		/* nothing to expand */
		return(grpsyl_p);
	}

	/* disallow on grace */
	if (grpsyl_p->grpvalue == GV_ZERO) {
		yyerror("can't use additive time values on grace notes");
		return(grpsyl_p);
	}
	/* disallow if alt on group */
	if (grpsyl_p->slash_alt < 0) {
		yyerror("can't use additive time values and alt on same chord");
		return(grpsyl_p);
	}

	/* If there were any subtraction, need to do that differently
	 * than if all are additions. Check if any subtractions, and
	 * add up total time in case we need it. */
	for (had_neg = NO, totaltime = grpsyl_p->fulltime, tl_p = timelist_p;
					tl_p != 0; tl_p = tl_p->next) {
		if (MI(tl_p->fulltime)) {
			had_neg = YES;
		}
		totaltime = radd(totaltime, tl_p->fulltime);
	}
	if (had_neg == YES) {
		gs_p = grpsyl_p;
		filltime(grpsyl_p, totaltime);
	}

	else {		 /* (No subtractions) */
		/* We will add a group for each added time value */
		for (gs_p = grpsyl_p; timelist_p != 0;
						timelist_p = timelist_p->next) {
			/* Make a copy the GRPSYL. This function is to be called
			 * with the group currently at the end of a list,
			 * so the "list" being cloned is only a single group,
			 * but might as well use the existing function... */
			gs_p->next = clone_gs_list(gs_p, YES);
			gs_p->next->prev = gs_p;
			gs_p = gs_p->next;
			/* Now fix up the time values in the cloned group */
			gs_p->fulltime = timelist_p->fulltime;
			gs_p->basictime = reconstruct_basictime(gs_p->fulltime);
			gs_p->dots = recalc_dots(gs_p->fulltime,
						gs_p->basictime);
			/* If the original group started a beam,
			 * added groups must be inside */
			if (gs_p->beamloc == STARTITEM) {
				gs_p->beamloc = INITEM;
			}
		}
	}

	/* We need to tie the note groups together, and remove any
	 * accidentals on the tied-to groups. Rests and spaces
	 * are not yet in final form, so have to look for PP_* forms. */
	if (grpsyl_p->grpcont == GC_NOTES) {
		for (gs_p = grpsyl_p; gs_p->next != 0; gs_p = gs_p->next) {
			int num_notes;	/* number of not rests/spaces */
			for (num_notes = n = 0; n < gs_p->nnotes; n++) {
				if (gs_p->notelist[n].letter == PP_REST ||
						gs_p->notelist[n].letter
						== PP_SPACE) {
					continue;
				}
				num_notes++;
				gs_p->notelist[n].tie = YES;
			}
			if (num_notes == gs_p->nnotes) {
				gs_p->tie = YES;
			}
		}
		/* tied-to notes accidentals must be implied, not explicit */
		for (n = 0; n < grpsyl_p->nnotes; n++) {
			if (grpsyl_p->notelist[n].acclist[0] != '\0') {
				struct GRPSYL *ngs_p;
				for (ngs_p = grpsyl_p->next; ngs_p != 0;
							ngs_p = ngs_p->next) {
					CLEAR_ACCS(ngs_p->notelist[n].acclist);
				}
			}
		}
	}
	return(gs_p);
}


/* Given a GRPSYL and a time value, add on enough GRPSYLs so that together
 * they add up to the specified totaltime.
 * One use for this is when user has "additive" time values, but used
 * subtraction, so we have to deduce what note time values will add up
 * to the result after the subtraction.
 * It's impossible to know what time values the user
 * really wants us to use, so we keep using the largest
 * possible values until we get enough, using as many
 * dots as possible along the way.
 * Another use is when padding out a measure with spaces to the time signature.
 * This could be useful if user is still composing, and has only decided
 * on the first part of a measure, but wants to print out
 * what they have so far. Or if they just made a mistake, rather than force
 * them to fix the time, we show them what they gave us, with warning.
 */

static void
filltime(gs_p, totaltime)

struct GRPSYL *gs_p;		/* clone this group as needed */
RATIONAL totaltime;		/* make enough groups to add up to this */

{
	RATIONAL try;	/* The time value we're currently attempting
			 * to use; will be used if remaining time
			 * is at least this long, else take half
			 * and try again. */
	int needgrp;	/* YES if need to alloc a new group.
			 * (Have one group to start with,
			 * so first time through loop don't need
			 * to allocate.) */


	/* We start with maximum possible time value,
	 * and keep using smaller times from there until we get enough,
	 * using dots if possible.
	 * For rests, we need to start with quad whole, since this could
	 * get called if user explicitly specifies a quad whole rest in
	 * an "additive" time involving subtraction. But we have to start
	 * with double whole for notes, because we don't support quad whole
	 * notes. At this point, rests and spaces are still stored as
	 * PP_REST and PP_SPACE, and the grpcont is always GC_NOTES,
	 * so we have to deduce what the eventual grpcont value will be.
	 * Unfortunately, in the case of chord-at-a-time,
	 * there could be a mixture, and then we don't know what to do,
	 * since if we use quad, notes could get mishandled, but if we use
	 * double, rests could get mishandled. On the good side, this
	 * pathological case requires four simultaneous rare circumstances:
	 * the user must use chord-at-a-time, with mixed notes and non-notes,
	 * and specify subtractive time that adds up to a quad whole or more
	 * (which implies a time signature of at least 4/1 or longer).
	 * Since there is always the workaround of voice-at-a-time input,
	 * we just admit defeat in that rare case.
	 * Start by assuming we can use double whole, and then see if
	 * we have to resort to something else.
	 */
	try = Two;
	if (GE(totaltime, Four)) {
		int num_non_note;
		int n;

		/* Count up number of non-notes */
		for (num_non_note = n = 0; n < gs_p->nnotes; n++) {
			if (gs_p->notelist[n].letter == PP_REST ||
					gs_p->notelist[n].letter == PP_SPACE) {
				num_non_note++;
			}
		}
		/* We only have an issue if any non-notes */
		if (num_non_note > 0) {
			try = Four;
			if (num_non_note != gs_p->nnotes) {
				/* This is really sort of a pfatal, since
				 * our program just doesn't know how to handle
				 * this case, but we don't really want to
				 * core dump */
				yyerror("unable to deduce time value for mixed note/non-note chord-at-a-time input, when quad whole or longer; try simplifying or use voice-at-a-time");
			}
		}
	}
	for (needgrp = NO; PL(totaltime);   ) {
		if (GE(totaltime, try)) {
			/* We can use this trial time value */
			if (needgrp == YES) {
				gs_p->next = clone_gs_list(gs_p, YES);
				gs_p->next->prev = gs_p;
				gs_p = gs_p->next;
				/* If the original group started a beam,
				 * added groups must be inside */
				if (gs_p->beamloc == STARTITEM) {
					gs_p->beamloc = INITEM;
				}
			}
			gs_p->fulltime = try;
			gs_p->dots = 0;
			gs_p->basictime = reconstruct_basictime(gs_p->fulltime);
			totaltime = rsub(totaltime, try);
			if (GE(totaltime, try)) {
				/* Total time is so long we can use
				 * another one of this time value. */
				needgrp = YES;
				continue;
			}
		}
		else {
			/* Trial time is too long. Try half as much. */
			try = rdiv(try, Two);
			if (try.d > 4000) {
				/* We must be trying to fill in enough time
				 * for part of a tuplet, where taking
				 * half on a non-tuplet will never get us
				 * down to zero. We should be cloning space
				 * in that case, so we can just make a space
				 * of whatever amount is still left, since
				 * we'll never really need to draw a note.
				 * If not space, we probably need a more
				 * sophisticated algorithm that can deduce
				 * exactly what tuplet to try. */
				if (gs_p->grpcont == GC_SPACE) {
					try = totaltime;
				}
				else {
					pfatal("Unable to determine appropriate time in filltime()");
				}
			}
			continue;
		}

		/* If still some time left, see if we can add one
		 * or more dots to use up more time. */
		for (try = rdiv(try, Two); GE(totaltime, try);
						try = rdiv(try, Two)) {
			(gs_p->dots)++;
			gs_p->fulltime = radd(gs_p->fulltime,
							try);
			totaltime = rsub(totaltime, try);
		}
		needgrp = YES;
	}
}


/*
 * Given a RATIONAL basictime and number of dots,
 * return the corresponding fulltime. That is calculated by 
 *	(2 * basictime) - (basictime x 1/2 to the (dots) power)
 */

RATIONAL
calcfulltime(basictime, dots)

RATIONAL basictime;
int dots;

{
	return( rsub (rmul(Two, basictime), rmul(basictime, rrai(One_half, dots))) );
}
