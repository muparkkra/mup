
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

/* This file contains utility functions for creating MIDI output
 * from Mup input. These functions are split out into this file
 * to keep midi.c from being too huge.
 */

#ifdef __WATCOMC__
#include <io.h>
#endif
#include "defines.h"
#include "structs.h"
#include "globals.h"

/* Saved signature values to avoid writing events if no actual change */
static short Curr_sharps;
static short Curr_is_minor;
static short Curr_timenum;
static short Curr_timeden;

static struct GRPSYL *create_prev_grp P((struct MAINLL *mll_p, int staffno,
		int v));
static struct GRPSYL *create_meas_space P((struct MAINLL *mll_p));
static void fix_spacechord P((struct MAINLL *chmll_p, struct CHORD *ch_p));
static void splitspace P((struct GRPSYL *gs_p, RATIONAL duration));
static void splicechord P((struct GRPSYL *gs_p, struct CHORD *ch_p));
static void guitar_grpsyl_transpose P((struct GRPSYL *gs_p));
static RATIONAL find_acc_end_time P((RATIONAL begin_time, struct GRPSYL *gs_p,
		int n));
static void propogate_accidental P((struct NOTE *note_p, RATIONAL begin_time,
		RATIONAL end_time, struct GRPSYL *gs_p));
static void mv_skipped_midi P((struct STUFF *stuff_p, int staffno,
		struct MAINLL *topstaff_mll_p));


/* seek back to where header size is in file, and fill in the correct size,
 * now that we know what it is. */

void
fix_track_size(mfile, track_start, track_size)

int mfile;		/* file descriptor of MIDI file */
long track_start;	/* offset in file where size needs to be put */
long track_size;	/* track length in bytes */

{
	unsigned char buff[4];


	debug(512, "fix_track_size");

	/* go to where track size is stored in file */
	(void) lseek(mfile, track_start + 4, SEEK_SET);

	/* convert to 4-byte number with correct byte ordering regardless
	 * of machine byte ordering */
	buff[0] = (track_size >> 24) & 0xff;
	buff[1] = (track_size >> 16) & 0xff;
	buff[2] = (track_size >> 8) & 0xff;
	buff[3] = track_size & 0xff;
	midiwrite(mfile, buff, 4);

	/* go back to end of file in case there are more track to write */
	(void) lseek(mfile, 0L, SEEK_END);
}


/* given an octave mark string, return number of octaves to tranpose (could
 * be negative if transposing down) */

int
parse_octave(string, place, fname, lineno)

char *string;		/* typically "8va" */
int place;		/* PL_ABOVE or PL_BELOW */
char *fname;		/* file name for errors */
int lineno;
		

{
	int font, size;
	int octave_value = 0;
	int code;		/* ASCII of character in string */

	
	font = string[0];
	size = string[1];
	string += 2;
	code = next_str_char(&string, &font, &size);
	if (isdigit(code)) {
		octave_value = code - '0';
		code = next_str_char(&string, &font, &size);
		/* might be a second digit. If user is crazy enough to use
		 * an octave number greater than 2 digits, ignore the rest */
		if (isdigit(code)) {
			octave_value = (octave_value * 10) + (code - '0');
		}
	}

	/* must be either a non-zero multiple of 8, or things like 15, 22,
	 * etc if some musical mathematician adds 7 instead of 8 */
	if (octave_value < 8 || ((octave_value - 8) % 7 != 0 &&
				 (octave_value - 8) % 8 != 0)) {
		l_ufatal(fname, lineno, "invalid octave mark string");
	}
	if (octave_value % 8 == 0) {
		octave_value /= 8;
	} else {
		octave_value = 1 + (octave_value - 8) / 7;
	}

	return(place == PL_BELOW ? -octave_value : octave_value);
}


/* determine "clocks per metronome tick." It not entirely clear to me
 * how that is supposed to work, but... if the time signature denominator is
 * 4, we'll use 24. If 8, use 12, etc. Only go down as far as 3, since that's
 * not divisible by 2. Really, in fast tempo triple time, the denominator
 * isn't the beat, but this will do for now. */

int
clocks(num)

int num;

{
	switch(num) {
	case 1:
		return(96);
	case 2:
		return(48);
	case 4:
		return(24);
	case 8:
		return(12);
	case 16:
		return(6);
	default:
		return(3);
	}
}


/* given a string, if it contains a word followed by an =, followed by a word,
 * any of which may be separated by white space, return, via pointers,
 * a pointer to the beginning of the word, the length of the word, and a pointer
 * to the first non-white-space character after the =, and return YES.
 * Otherwise, return NO. A "word" is a sequence of non-white-space chars. */

int
getkeyword(string, key_p, leng_p, arg_p_p)

char *string;		/* check this string */
char **key_p;		/* return pointer to keyword via this */
int *leng_p;		/* return length of keyword via this */
char **arg_p_p;		/* return pointer to argument after = via this */

{
	char *tok;


	/* skip leading white space */
	for (*key_p = string; **key_p == ' ' || **key_p == '\t'; (*key_p)++) {
		;
	}

	/* go till hit white space or equals sign */
	for (tok = *key_p; *tok != '\0'; tok++) {
		if (*tok == ' ' || *tok == '\t' || *tok == '=') {
			break;
		}
	}

	/* fill in length of key */
	*leng_p = tok - *key_p;

	if (*leng_p == 0) {
		return(NO);
	}

	/* find first non-white beyond the = */
	for (   ; *tok != '\0'; tok++) {
		if (*tok == '=') {
			for (tok++; *tok != '\0'; tok++) {
				if (*tok != ' ' && *tok != '\t') {
					*arg_p_p = tok;
					return(YES);
				}
			}
		}
	}
	return(NO);
}


/* given a user-specified key, see if it matches the given command name. Return
 * YES if it does, NO if it doesn't. User only has to specify the first 3 or
 * more characters of the command, because that's enough to make it unique,
 * and saves them from typing longer names. */

int
matches(key, leng, cmd)

char *key;		/* user specified key to be checked */
int leng;		/* length of key */
char *cmd;		/* check if key matches this command */

{
	if (leng < 3) {
		return(NO);
	}
	return(strncmp(key, cmd, leng) == 0 ? YES : NO);
}


/* given an ASCII hex digit, return its value 0-15 */

int
hexdig(ch)

int ch;

{
	if (ch >= '0' && ch <= '9') {
		return(ch - '0');
	}
	else if (ch >= 'a' && ch <= 'f') {
		return(ch - 'a' + 10);
	}
	else if (ch >= 'A' && ch <= 'F') {
		return(ch - 'A' + 10);
	}
	pfatal("bad hex digit");
	/*NOTREACHED*/
	return(0);
}


/* given a string, output it to midi file, prefixed by its length. */
/* return number of bytes written */

UINT32B
midi_wrstring(mfile, str, internalform)

int mfile;	/* MIDI file */
char *str;	/* string to write to file */
int internalform;	/* YES if str is in Mup format, NO if just ASCII,
			 * C-style null-terminated string to be copied */

{
	char *buff;		/* for all-ASCII version of str */
	UINT32B bytes;		/* number of bytes in length value */
	int length;		/* of string */


	/* get plain ascii version of string. Write out length of
	 * string, then plain string itself */
	if (internalform == YES) {
		buff = ascii_str(str, NO, YES, TM_NONE);
		length = strlen(buff);
		bytes = wr_varlength(mfile, (UINT32B) length);
		bytes += write(mfile, buff, (unsigned) length);
	}
	else {
		length = strlen(str);
		bytes = wr_varlength(mfile, (UINT32B) length);
		bytes += write(mfile, str, (unsigned) length);
	}

	/* return number of bytes written */
	return(bytes);
}


/* given a number, write to MIDI file in MIDI variable length format.
 * Return number of bytes written. */

UINT32B
wr_varlength(mfile, num)

int mfile;	/* MIDI file */
UINT32B num;

{
	unsigned char buff[4];
	int i;
	int shift;
	

	/* Because only 7 bits of each MIDI byte can be used,
	 * there is only support for numbers up to 28 bits long. */
	if ((num & 0xf0000000) != 0) {
		ufatal("midi value too large");
	}

	/* convert value to the MIDI variable-length number, which
	 * uses the lower 7 bits of each byte as parts of the number, and
	 * the high order bit as a flag to say which is the last byte of
	 * the (potentially) multi-byte number */
	for (i = 0, shift = 21; shift >= 7; shift -= 7) {
		if ( (num >> shift) || (i > 0)) {
			buff[i++] = 0x80 | ((num >> shift) & 0x7f);
		}
	}
	buff[i] = num & 0x7f;
	midiwrite(mfile, buff, (unsigned) (i + 1));
	return (UINT32B) (i+1);
}


/* We remember current key and time signature information and only output
 * the midi events if they have changed. So this should be called at the
 * beginning of each track to ensure the initial values for that track are
 * written out. 
 */

void
reset_sigs()
{
	/* Set to impossible values to ensure a mismatch */
	Curr_sharps = -9999;
	Curr_is_minor = UNSET;
	Curr_timenum = -9999;
	Curr_timeden = -9999;
}


/* do key signature. Return number of bytes written */

UINT32B
midi_keysig(mfile, sharps, is_minor)

int mfile;
int sharps;
int is_minor;	/* YES if minor */

{
	UINT32B bytes;
	unsigned char buff[8];


	/* If same as already is, don't bother */
	if (sharps == Curr_sharps && is_minor == Curr_is_minor) {
		return(0);
	}
	Curr_sharps = sharps;
	Curr_is_minor = is_minor;

	bytes = write_delta(mfile);
	buff[0] = 0xff;
	buff[1] = 0x59;
	buff[2] = 0x02;
	buff[3] = (char) sharps;
	buff[4] = (is_minor == YES ? 1 : 0);
	midiwrite(mfile, buff, 5);

	return(bytes + 5);
}


/* write out the timesig in Score SSV. Return number of bytes written */

UINT32B
midi_timesig(mfile)

int mfile;
{
	UINT32B bytes;
	unsigned char buff[8];


	/* With additive time signatures, it is possible to get an effective
	 * time signature that won't fit in 7 bits. In that case, we don't
	 * do any time signature, since we can't represent it. */
	if (Score.timenum > 127) {
		return(0);
	}

	/* If same as already is, don't bother */
	if (Score.timenum == Curr_timenum && Score.timeden == Curr_timeden) {
		return(0);
	}
	Curr_timenum = Score.timenum;
	Curr_timeden = Score.timeden;

	bytes = write_delta(mfile);
	buff[0] = 0xff;
	buff[1] = 0x58;
	buff[2] = 0x04;
	buff[3] = (char) Score.timenum;
	buff[4] = (unsigned char) drmo(Score.timeden);
	buff[5] = clocks(Score.timeden);
	buff[6] = 0x8;
	bytes += write(mfile, buff, 7);
	return(bytes);
}


/* find group before given group. If none before it in current measure,
 * back up in main list to find corresponding group list, and use final group
 * in that list. If no group exists, create one. */

struct GRPSYL *
grp_before(gs_p, mll_p, staffno, v)

struct GRPSYL *gs_p;	/* find group before this one */
struct MAINLL *mll_p;	/* the list containing gs_p is attached to main list here */
int staffno;
int v;			/* voice */

{
	int found_bar = NO;


	if (gs_p->prev != (struct GRPSYL *) 0) {
		/* oh good. There's another group before this one in the
		 * current measure, so just return it */
		return(gs_p->prev);
	}

	/* have to go back to previous measure, if any. Start searching
	 * backwards in main list. */
	for (mll_p = mll_p->prev; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->prev) {
		switch (mll_p->str) {
		case S_STAFF:
			if (found_bar == NO) {
				/* still in current measure */
				break;
			}

			if (mll_p->u.staff_p->staffno == staffno) {
				/* we found the previous measure */
				if (mll_p->u.staff_p->groups_p[v]
						!= (struct GRPSYL *) 0) {
					/* find and return last group */
					for (gs_p=mll_p->u.staff_p->groups_p[v];
							gs_p->next !=
							(struct GRPSYL *) 0;
							gs_p = gs_p->next) {
						;
					}
					return(gs_p);
				}
				else {
					/* this voice wasn't present before.
					 * Will have to create a measure */
					return(create_meas_space(mll_p));
				}
			}
			else if (mll_p->u.staff_p->staffno < staffno) {
				/* corresponding staff does not exist in this
				 * measure. The only time this should happen is
				 * if user changed the number of staffs.
				 * So create staff */
				return(create_prev_grp(mll_p, staffno, v));
			}
			break;

		case S_BAR:
			found_bar = YES;
			break;

		default:
			/* ignore other things */
			break;
		}
	}

	/* Fell off the top of the list. This used to be possible,
	 * and we called create_prev_grp() to create a measure.
	 * But the measure really needs to be created much earlier--
	 * before makechords() is run--in order for squeezing to work right.
	 * So that's what we do now. So we should never get here. */
	pfatal("fell off top of list in grp_before()");
	/* NOTREACHED */
	return(create_prev_grp(mll_p, staffno, v));
}


/* create a new STAFF struct and insert in main list, with grpcont of
 * space and fulltime of the measure. Return pointer to the GRPSYL of
 * appropriate voice of the STAFF that was created. */

static struct GRPSYL *
create_prev_grp(mll_p, staffno, v)

struct MAINLL *mll_p;	/* insert here */
int staffno;
int v;

{
	struct MAINLL *new_p;	/* new STAFF */
	int i;


	new_p = newMAINLLstruct(S_STAFF, -1);
	new_p->u.staff_p->staffno = (short) staffno;
	insertMAINLL(new_p, mll_p);
	for (i = 0; i < MAXVOICES; i++) {
		new_p->u.staff_p->groups_p[i]
					= create_meas_space(mll_p);
	}

	/* if added to beginning of list, have to add bar as well */
	if (mll_p == (struct MAINLL *) 0) {
		struct MAINLL *mbar_p;

		mbar_p = newMAINLLstruct(S_BAR, -1);
		insertMAINLL(mbar_p, new_p);
	}

	return(new_p->u.staff_p->groups_p[v]);
}


/* create a measure space as long as that of the reference measure (or of 4/4
 * if no reference) and return it */

static struct GRPSYL *
create_meas_space(mll_p)

struct MAINLL *mll_p;	/* use this for reference to get measure length */

{
	struct GRPSYL *gs_p;	/* new grpsyl */
	struct GRPSYL *egs_p;	/* existing grpsyl */


	gs_p = newGRPSYL(GS_GROUP);
	gs_p->grpcont = GC_SPACE;

	/* figure out how much full time to give the group. If mll_p is not
 	 * null, we are adding a staff to an existing measure, so use
	 * length of its first voice. Count up the length of existing measure */
	gs_p->fulltime = Zero;
	if (mll_p != (struct MAINLL *) 0 && mll_p->str == S_STAFF) {
		for (egs_p = mll_p->u.staff_p->groups_p[0];
					egs_p != (struct GRPSYL *) 0;
					egs_p = egs_p->next) {

			gs_p->fulltime = radd(gs_p->fulltime, egs_p->fulltime);
		}
	}
	else {
		/* at beginning of list, use default  of 1/1 (the reduced
		 * form of 4/4) */
		gs_p->fulltime.n = gs_p->fulltime.d = 1;
	}

	return(gs_p);
} 


/* add a rest of the specified fulltime duration after the specified group.
 * Since this is just for midi purposes, don't worry about filling in all
 * the fields. */

void
add_rest(gs_p, fulltime)

struct GRPSYL *gs_p;	/* add rest after this group */
RATIONAL fulltime;	/* make it this long */

{
	struct GRPSYL *newgs_p;


	if (gs_p == (struct GRPSYL *) 0) {
		pfatal("null group passed to add_rest");
	}

	newgs_p = newGRPSYL(GS_GROUP);
	newgs_p->grpcont = GC_REST;
	rred (&fulltime);
	newgs_p->fulltime = fulltime;
	newgs_p->next = gs_p->next;
	newgs_p->prev = gs_p;
	gs_p->next = newgs_p;
	if (newgs_p->next != (struct GRPSYL *) 0) {
		newgs_p->next->prev = newgs_p;
	}
}


/* when all voices have space, that should be squeezed to zero time.
 * Go through main list. For each CHHEAD found,
 * go down the list of chords. For each chord, see if
 * if it is an all-space chord. If so, call fix_spacechord() to
 * handle it.
 */

void
midi_squeeze()

{
	struct MAINLL *mll_p;	/* walk through main list */
	struct CHORD *ch_p;	/* walk through list of chords */


	debug(256, "midi_squeeze");

	initstructs();
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {

		/* skip everything except CHHEADs and SSV updates */
		if (mll_p->str != S_CHHEAD) {
			if (mll_p->str == S_SSV) {
				asgnssv(mll_p->u.ssv_p);
			}
			continue;
		}

		/* do each chord */
		for (ch_p = mll_p->u.chhead_p->ch_p;
						ch_p != (struct CHORD *) 0;
						ch_p = ch_p->ch_p) {

			if (ch_p->uncollapsible == 0.0) {
				/* found one to squeeze, do it */
				fix_spacechord(mll_p, ch_p);
			}
		}
	}
}


/* given an all-space chord to crunch, split up any groups that
 * spill into this chord or extend beyond it. Then set the grpvalue of each
 * group in the chord to GV_ZERO.
 */

static void
fix_spacechord(chmll_p, ch_p)

struct MAINLL *chmll_p;		/* chord is hanging off this CHHEAD */
struct CHORD *ch_p;		/* zero-width chord */

{
	struct MAINLL *mll_p;	/* walk through STAFFs in main list */
	struct GRPSYL *gs_p;	/* walk through groups in chord */
	struct GRPSYL *group_p;	/* head of list of grpsyls in measure */
	RATIONAL minspacetime;	/* time of shortest space in chord */
	RATIONAL chordstart;	/* where in measure chord begins */
	RATIONAL chordend;	/* where the space ends */
	RATIONAL new_chordend;	/* tentative new value for chordend */
	RATIONAL grpstart;	/* where in measure grpsyl begins */
	int v;			/* voice index */


	/* first find the smallest duration in the chord. Can't use
	* ch_p->duration here because grace notes have been adjusted
	* by now to take some time */
	minspacetime = ch_p->gs_p->fulltime;
	for (gs_p = ch_p->gs_p->gs_p; gs_p != (struct GRPSYL *) 0;
						gs_p = gs_p->gs_p) {

		/* skip lyrics */
		if (gs_p->grpsyl == GS_SYLLABLE) {
			continue;
		}

		/* skip things on tab staff--we use the associated tabnotes
		 * staff instead */
		if (is_tab_staff(gs_p->staffno) == YES) {
			continue;
		}

		/* double check */
		if (gs_p->grpcont != GC_SPACE) {
			/* This used to be a pfatal, but now we actually
			 * allow chords to overlap in certain cases to the
			 * point where they end up with a zero width.
			 * So the easiest thing to do is just catch that
			 * here, and say, "oops, this isn't really a chord
			 * that we can squeeze."
			 */
			return;
		}

		/* find minimum time value of space */
		if (LT(gs_p->fulltime, minspacetime)) {
			minspacetime = gs_p->fulltime;
		}
	}

	/* find the start of the chord. Can't use ch_p->startime because
	 * we may have squeezed out time earlier in the measure and need
	 * to compensate for that. So have to count up the time before
	 * the group at the top of the chord. */
	for (chordstart = Zero, gs_p = ch_p->gs_p->prev;
				gs_p != (struct GRPSYL *) 0;
				gs_p = gs_p->prev) {
		chordstart = radd(chordstart, gs_p->fulltime);
	}

	/* if spaces are overlapped in strange ways between different
	 * voices, the minspacetime we found above may actually be too long.
	 * If the time between this chord and the next is less than the
	 * minspacetime found so far, make minspacetime the time till the
	 * next chord */
	if (ch_p->ch_p != (struct CHORD *) 0) {
		if (LT(rsub(ch_p->ch_p->starttime, chordstart), minspacetime)) {
			minspacetime = rsub(ch_p->ch_p->starttime, chordstart);
		}
	}

	/* That still isn't completely adequate to find where the space actually
	 * ends, because if there were lots of grace notes moved back into a
	 * space, and a very short space in another voice, they could overlap.
	 * So go through the list of voices, seeing where the real end is.
	 * This code is unfortunately very similar to the code below it,
	 * yet different enough to make it hard to make it
	 * into a common function. */
	chordend = radd(chordstart, minspacetime);
	for (mll_p = chmll_p->next; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {

		/* CHHEAD is followed immediately by STAFFS, so when we
		 * hit something other than STAFF, we are done */
		if (mll_p->str != S_STAFF) {
			break;
		}

		/* do each voice */
		for (v = 0; v < MAXVOICES; v++ ) {
			/* Check if we have a special case where the actual
			 * space is shorter than we thought. We unfortunately
			 * cannot use hasspace() here, because it ignores grace
			 * notes, and by now, grace notes have some time.
			 * Find the chord that begins the space in question. */
			grpstart = Zero;

			if ((gs_p = mll_p->u.staff_p->groups_p[v])
						== (struct GRPSYL *) 0) {
				/* no voice here, so that's all space.
				 * That's good: we're done with this voice */
				continue;
			}

			/* grace notes at the beginning of the measure
			 * have effectively been moved timewise
			 * into previous measure, so discount them. */
			for (   ; gs_p->grpvalue == GV_ZERO
					&& gs_p->grpcont == GC_NOTES;
					gs_p = gs_p->next) {
				;
			}

			for (  ; gs_p != (struct GRPSYL *) 0;
							gs_p = gs_p->next) {

				/* see if this group start corresponds
				 * with start of chord, or spills into
				 * the chord or is the last chord. If
				 * so, that's the one we want. */
				if (EQ(grpstart, chordstart) ||
						GT(radd(grpstart,
						gs_p->fulltime),
						chordstart) ||
						gs_p->next ==
						(struct GRPSYL *) 0) {
					/* found appropriate group */
					break;
				}
				else {
					/* accummulate time so far */
					grpstart = radd(grpstart, gs_p->fulltime);
				}
			}

			if (gs_p == (struct GRPSYL *) 0) {
				pfatal("failed to find space group");
			}

			if (gs_p->grpcont != GC_SPACE) {
				/* things overlapped so much after the
				 * grace note adjustments and such
				 * that this isn't really a
				 * crunch-able chord after all. */
				return;
			}

			/* need to adjust amount of space we
			 * can really crunch. Find where this chord
			 * ends, and add in the time of any immediately
			 * following space groups. */
			new_chordend = radd(grpstart, gs_p->fulltime);
			for (gs_p = gs_p->next; gs_p != (struct GRPSYL *) 0;
						gs_p = gs_p->next) {

				if (gs_p->grpcont == GC_SPACE) {
					new_chordend = radd(new_chordend,
							gs_p->fulltime);
				}
				else {
					break;
				}
			}

			/* if the newly calculated end is sooner than
			 * what we had before, then adjust accordingly. */
			if (LT(new_chordend, chordend)) {
				chordend = new_chordend;
			}
		}
	}

	/* recalculate the minspace time after any adjustment */
	minspacetime = rsub(chordend, chordstart);
	

	/* go down each voice of each staff.
	 * For each, find the space group associated with the chord.
	 * If it has the same starttime as the chord and has minspacetime
	 * duration, it's easy: we just mark it GV_ZERO. Otherwise, if
	 * it starts earlier, we have to split off a group in the front
	 * first, and if it lasts longer than the end of the chord, we
	 * have to split off a group at the end first.
	 */
	for (mll_p = chmll_p->next; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {
		/* CHHEAD is followed immediately by STAFFS, so when we
		 * hit something other than STAFF, we are done */
		if (mll_p->str != S_STAFF) {
			break;
		}

		/* do each voice */
		for (v = 0; v < MAXVOICES; v++ ) {

			/* get shorter name for list of grpsyls */
			group_p = mll_p->u.staff_p->groups_p[v];

			if (group_p == (struct GRPSYL *) 0) {
				continue;
			}

			/* go through groups, add up time till we find the
			 * group we're looking for */
			grpstart = Zero;

			/* grace notes at the beginning of the measure
			 * have effectively been moved timewise
			 * into previous measure, so discount them. */
			for (gs_p = group_p; gs_p->grpvalue == GV_ZERO
					&& gs_p->grpcont == GC_NOTES;
					gs_p = gs_p->next) {
				;
			}

			for (  ; gs_p != (struct GRPSYL *) 0;
						gs_p = gs_p->next) {

				/* see if this group start corresponds with
				 * start of chord */
				if (EQ(grpstart, chordstart)) {
					/* found appropriate group */
					break;
				}

				else if (GT(radd(grpstart,gs_p->fulltime),
								 chordstart)) {
					/* This group spills into space
					 * to be crunched.
					 * Split off beginning of group. */
					splitspace(gs_p,
						rsub(chordstart, grpstart));

					/* point to added group */
					gs_p = gs_p->next;

					/* splice added group into chord */
					splicechord(gs_p, ch_p);

					/* found appropriate group */
					break;
				}
				else {
					/* haven't gotten to the group yet.
					 * Add on the time taken by this
					 * group in preparation for next
					 * trip around the loop */
					grpstart = radd(grpstart, gs_p->fulltime);
					/* if last group in measure, this has
					 * to be the appropriate one */
					if (gs_p->next == (struct GRPSYL *) 0) {
						break;
					} 
				}
			}

			if (gs_p == (struct GRPSYL *) 0) {
				pfatal("failed to find space group");
			}

			/* if group extended beyond end of
			 * chord, split the group and splice added group
			 * into chord */
			if (GT(gs_p->fulltime, minspacetime)) {
				splitspace(gs_p, minspacetime);
				splicechord(gs_p->next, ch_p->ch_p);
			}

			/* mark as taking no time */
			gs_p->grpvalue = GV_ZERO;
		}
	}
}


/* split a space grpsyl into two. The original group becomes the first
 * group, having the specified duration. A new group is added after it,
 * having the remainder of the time taken by the original group. */

static void
splitspace(gs_p, duration)

struct GRPSYL *gs_p;	/* split this group */
RATIONAL duration;	/* make the first group of split this long */

{
	struct GRPSYL *newgs_p;	/* added group */


	/* bug insurance */
	if (gs_p == (struct GRPSYL *) 0 ||
			(gs_p->grpcont != GC_SPACE &&
			svpath(gs_p->staffno, VISIBLE)->visible == YES) ) {
		pfatal("bad group passed to splitspace");
	}

	/* split into 2 groups, one taking duration, and the
	 * other taking the remainder */
	newgs_p = newGRPSYL(GS_GROUP);
	copy_attributes(newgs_p, gs_p);
	newgs_p->grpcont = GC_SPACE;
	newgs_p->fulltime = rsub(gs_p->fulltime, duration);
	gs_p->fulltime = duration;

	/* link new one into list */
	newgs_p->next = gs_p->next;
	newgs_p->prev = gs_p;
	gs_p->next = newgs_p;	
	if (newgs_p->next != (struct GRPSYL *) 0) {
		newgs_p->next->prev = newgs_p;
	}
}


/* splice a grpsyl into a chord */

static void
splicechord(gs_p, ch_p)

struct GRPSYL *gs_p;	/* splice in this group */
struct CHORD *ch_p;	/* splice into this chord */

{
	struct GRPSYL *nxtgs_p;		/* next group in chord list */
	struct GRPSYL **ins_p_p;	/* where to insert new grpsyl */


	if (ch_p == (struct CHORD *) 0) {
		/* this could happen if user gave measure of space, and
		 * then following measure started with a grace note. The
		 * grace note got moved into the space, but there was no
		 * chhead created for it. It should be safe to just
		 * not link it into a chhead, so return */
		return;
	}

	if (gs_p == (struct GRPSYL *) 0) {
		pfatal("null pointer in splicechord");
	}

	/* Figure out where to insert. */
	for (ins_p_p = &(ch_p->gs_p); (*ins_p_p) != (struct GRPSYL *) 0;
						ins_p_p = &((*ins_p_p)->gs_p)) {

		/* gets inserted here if next group is for a higher staffno or
		 * for a higher voice number of the same staffno */
		nxtgs_p = *ins_p_p;
		if (nxtgs_p == (struct GRPSYL *) 0) {
			/* goes at end of list */
			break;
		}
		if (nxtgs_p->staffno > gs_p->staffno ||
				   (nxtgs_p->staffno == gs_p->staffno &&
				   nxtgs_p->vno > gs_p->vno)) {
			break;
		}
	}

	if (ins_p_p == (struct GRPSYL **) 0) {
		pfatal("couldn't find where to insert new grpsyl into chord");
	}

	/* insert it */
	gs_p->gs_p = *ins_p_p;
	*ins_p_p = gs_p;
}


/* if user specifies the default guitar tab staff, then we want to transpose
 * everything down an octave, because a standard guitar sounds an octave
 * lower than it is written. */

void
guitar_transpose()

{
	struct MAINLL *mll_p;


	/* go through the main list looking for things to transpose */
	initstructs();
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			/* If there is a staff below this and that staff is a
			 * guitar tab staff, then we need to transpose.
			 */
			if (mll_p->u.staff_p->staffno < Score.staffs &&
					svpath(mll_p->u.staff_p->staffno + 1, STAFFLINES)->strinfo
					== Guitar) {
				guitar_grpsyl_transpose(mll_p->u.staff_p->groups_p[0]);
			}
		}
		else if (mll_p->str == S_SSV) {
			asgnssv(mll_p->u.ssv_p);
		}
	}
}


/* given a group, transpose everything down by an octave */

static void
guitar_grpsyl_transpose(gs_p)

struct GRPSYL *gs_p;

{
	register int n;

	for(   ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {
		for (n = 0; n < gs_p->nnotes; n++) {
			if (gs_p->notelist[n].octave > 0) {
				(gs_p->notelist[n].octave)--;
			}
			else {
				pfatal("guitar transposition results in note below octave 0");
			}
		}
	}
}


/* If there is an accidental on a note on one voice, it should really apply
 * the other voices on that same staff too. So propogate these accidentals
 * to the other voices if necessary.
 * However, if carryaccs == NO, then don't propogate anything.
 */

void
other_voice_accidentals(staff_p)

struct STAFF *staff_p;

{
	struct GRPSYL *gs_p;
	int v;			/* voice */
	int otherv;		/* a different voice */
	int relevant_voices;	/* how many voices might have actual notes */
	int n;			/* notelist index */
	RATIONAL begin_time, end_time;


	if (svpath(staff_p->staffno, CARRYACCS)->carryaccs == NO) {
		return;
	}

	/* If there is only one voice on the staff, or at least only one
	 * voice that is not a measure space/rest or multirest, nothing to do.
	 * Since we're doing MIDI, we know any "measure" GRPSYL
	 * will be rest or space, not mrpt, so can just check that. */
	relevant_voices = 0;
	for (v = 0; v < MAXVOICES; v++) {
		if (staff_p->groups_p[v] == 0) {
			break;
		}
	 	if (staff_p->groups_p[0]->is_meas == NO) {
			relevant_voices++;
		}
	}
	if (relevant_voices < 2) {
		return;
	}


	/* do each voice */
	for (v = 0; v < MAXVOICES; v++) {

		/* do each note of each chord */
		begin_time = Zero;
		for (gs_p = staff_p->groups_p[v]; gs_p != (struct GRPSYL *) 0;
							gs_p = gs_p->next) {

			for (n = 0; n < gs_p->nnotes; n++) {

				/* if this note doesn't have an accidental
				 * nothing more to do on it */
				if (standard_acc(gs_p->notelist[n].acclist)
								 == '\0') {
					continue;
				}

				/* This note does have an accidental.
				 * Go forward and see how long it lasts.
				 * It will last until there is another
				 * accidental on the same note letter/octave
				 * or until end of measure.
				 */
				end_time = find_acc_end_time(begin_time, gs_p, n);

				/* Now check the other voice(s).
				 * If the same letter/octave note appears
				 * on that voice on or after the begin time but
				 * before the end time, and that note
				 * does not already have an accidental,
				 * give it the same accidental as
				 * was found on the current voice.
				 */
				for (otherv = 0; otherv < MAXVOICES; otherv++) {
					if (otherv == v) {
						/* skip ourself */
						continue;
					}
					if (staff_p->groups_p[otherv] == 0) {
						/* other voice doesn't exist */
						continue;
					}
					propogate_accidental(
						&(gs_p->notelist[n]),
						begin_time, end_time,
						staff_p->groups_p[otherv]);
				}
			}

			/* accumulate time so far in measure */
			begin_time = radd(begin_time, gs_p->fulltime);
		}
	}
}

/* Given an GRPSYL and a note n in its notelist, and a begin_time, return
 * the end_time which is either the end of the measure or the next instance
 * of the given note which has an accidental on it. */

static RATIONAL
find_acc_end_time(begin_time, gs_p, n)

RATIONAL begin_time;
struct GRPSYL *gs_p;
int n;

{
	RATIONAL end_time;
	struct GRPSYL *end_gs_p;
	int n2;


	end_time = begin_time;
	end_gs_p = gs_p;
	for ( ; ; ) {
		/* Add up time that accidental lasts */
		end_time = radd(end_time, end_gs_p->fulltime);

		if ((end_gs_p = end_gs_p->next) == (struct GRPSYL *) 0) {
			/* Hit end of measure */
			return(end_time);
		}

		/* See if this group has the same note as had the accidental,
		 * and if so, whether it has a new accidental. */
		for (n2 = 0; n2 < end_gs_p->nnotes; n2++) {
			if ((gs_p->notelist[n].letter
					== end_gs_p->notelist[n2].letter) &&	
					(gs_p->notelist[n].octave
					== end_gs_p->notelist[n2].octave)) {

				/* does have the same note. check accidental */
				if (standard_acc(end_gs_p->notelist[n2].acclist)
								!= '\0') {
					/* a new accidental, so this cancels
					 * the one we had */
					return(end_time);
				}
				break;
			}
		}
	}
}


/* Check the groups in list pointed to by gs_p.
 * If the letter/octave of the given note_p appears on or after the begin time
 * but before the end time, and that note does not already have an accidental,
 * give it the same accidental as was found on note_p. */

static void
propogate_accidental(note_p, begin_time, end_time, gs_p)

struct NOTE *note_p;	/* look for note matching this one */
RATIONAL begin_time;	/* look between the begin_time and end_time */
RATIONAL end_time;
struct GRPSYL *gs_p;	/* look in this list and adjust if needed */

{
	RATIONAL accumulated_time;
	int n;


	for (accumulated_time = Zero;
				gs_p != (struct GRPSYL *) 0;
				accumulated_time = radd(accumulated_time,
				gs_p->fulltime), gs_p = gs_p->next) {

		if (LT(accumulated_time, begin_time)) {
			/* haven't gotten to begin yet */
			continue;
		}

		if (GE(accumulated_time, end_time)) {
			/* reached end time */
			return;
		}

		/* See if this group contains the note of interest */
		for (n = 0; n < gs_p->nnotes; n++) {
			if ( (gs_p->notelist[n].letter == note_p->letter) &&
					(gs_p->notelist[n].octave == note_p->octave) ) {
				/* if note already has an accidental,
				 * the one from the other voice doesn't
				 * count. Otherwise propogate the
				 * accidental from the other voice */
				if (has_accs(gs_p->notelist[n].acclist) == NO) {
					COPY_ACCS(gs_p->notelist[n].acclist,
							note_p->acclist);
				}

				/* Found the note and fixed it if needed,
				 * so our job here is done */
				return;
			}
		}
	}
}


/* If doing MIDI with the -x option, we move all the midi STUFFs in the
 * skipped leading measures to the "extra" empty measure that exists for MIDI.
 * This ensure that any MIDI parameters, instruments, etc are correct
 * for the place in the song where we are actually starting.
 */

void
mv_midi_items(mll_p, topstaff_mll_p)

struct MAINLL *mll_p;	/* points to a STAFF that may have MIDI items to move */
struct MAINLL *topstaff_mll_p;	/* points to STAFF where "all" MIDI items will go */

{
	struct STUFF *stuff_p;	/* stuff currently being processed */
	struct STUFF *next;	/* we may move the current STUFF to another
				 * list, so need to save its "next" */
	char *key;		/* midi directive keyword */
	int leng;		/* length of key */
	char *arg;		/* arg after the = */
	int staffno;


	staffno = mll_p->u.staff_p->staffno;
	for (stuff_p = mll_p->u.staff_p->stuff_p; stuff_p != 0; stuff_p = next) {
		/* keep track of next, in case we delete the current */
		next = stuff_p->next;

		if (next == stuff_p) {
			pfatal("loop detected in MIDI list");
		}

		if (stuff_p->stuff_type == ST_MIDI) {
			if (getkeyword(stuff_p->string+2, &key, &leng, &arg)
								== NO) {
				/* invalid, so ignore it. */
				continue;
			}

			/* Some MIDI things, like cue point should just be
			 * ignored if we are skipping over the music,
			 * so just check for and move things we care about.
			 */
			if (matches(key, leng, "program") == YES ||
					matches(key, leng, "tempo") == YES ||
					matches(key, leng, "onvelocity") == YES ||
					matches(key, leng, "channel") == YES ||
					matches(key, leng, "parameter") == YES ||
					matches(key, leng, "offvelocity") == YES ||
					matches(key, leng, "name") == YES ||
					matches(key, leng, "instrument") == YES ||
					matches(key, leng, "hex") == YES ||
					matches(key, leng, "port") == YES ||
					matches(key, leng, "chanpressure") == YES) {
				/* Found something to move; move it */
				mv_skipped_midi(stuff_p, staffno, topstaff_mll_p);
			}
		}
	}
}


/* With -x option and MIDI, this moves a MIDI STUFF from where it was in
 * a skipped measure to the special "empty" measure at the beginning of the
 * piece.  In general, for any given MIDI thing, only the last one matters,
 * so we only need to keep the last one.
 */

static void
mv_skipped_midi(stuff_p, staffno, topstaff_mll_p)

struct STUFF *stuff_p; 		/* Midi STUFF to be moved */
int staffno;			/* Which staff it is for */
struct MAINLL *topstaff_mll_p;	/* The STAFF where midi "all" STUFF go.
				 * Other STAFFs, if any, will surround it. */

{
	int leng;			/* length of MIDI keyword */
	char *oldtext;			/* text of STUFF that already
					 * exists in the list */
	char *newtext;			/* text of STUFF to be added */
	struct MAINLL *mll_p;		/* for finding STAFF to link to */
	struct STAFF *staff_p = 0;	/* the STAFF to link to */
	struct STUFF **stuff_p_p;	/* for finding where to add
					 * the new stuff_p into list */
	struct STUFF *st_p;		/* for checked for already moved */
	static char *alphabet = "abcdefghijklmnopqrstuvwxyz";

	/* Find the correct STAFF to attach to. We all passed a pointer
	 * to the STAFF for midi "all" items. For non-"all" items,
	 * we search in the main list for the correct STAFF. 
	 */
	if (stuff_p->all == YES || staffno == topstaff_mll_p->u.staff_p->staffno) {
		staff_p = topstaff_mll_p->u.staff_p;
	}
	else if (staffno < topstaff_mll_p->u.staff_p->staffno) {
		/* Search backward for correct staff from the "all" staff.
		 * This case probably won't happen often--only if some
		 * staffs at the top are currently invisible (so they are
		 * not the current "top visible staff").
		 */
		for (mll_p = topstaff_mll_p; mll_p->str == S_STAFF;
						mll_p = mll_p->prev) {
			if (mll_p->u.staff_p->staffno == staffno) {
				staff_p = mll_p->u.staff_p;
				break;
			}
		}
	}
	else {
		/* Search forwards for correct staff from the "all" staff. */
		for (mll_p = topstaff_mll_p; mll_p->str == S_STAFF;
						mll_p = mll_p->next) {
			if (mll_p->u.staff_p->staffno == staffno) {
				staff_p = mll_p->u.staff_p;
				break;
			}
		}
	}
	if (staff_p == 0) {
		/* User must have reduced the number of staffs during the
		 * skipped part, so this one is irrelevant. */
		return;
	}

	/* If there was a repeat that went back to the "extra" measure,
	 * the STUFF may have already been moved. If we move it again,
	 * that creates an infinite loop. So avoid that. */
	for (st_p = staff_p->stuff_p; st_p != 0; st_p = st_p->next) {
		if (st_p == stuff_p) {
			/* Already on list; don't add again. */
			return;
		}
	}

	/* Add to end of list. When going through the list,
	 * see if there is already an item of that type (for param
	 * have to check the specific param matches too), and if so delete
	 * the earlier one, because this new one overrides it.
	 */
	newtext = stuff_p->string + 2;
	for (stuff_p_p = &(staff_p->stuff_p); *stuff_p_p != 0;
					stuff_p_p = &((*stuff_p_p)->next) ) {

		/* If for different place,
		 * this one doesn't count for matching */
		if ( (*stuff_p_p)->place != stuff_p->place) {
			continue;
		}

		oldtext = (*stuff_p_p)->string + 2;

		/* Since hex is arbitrary data, we always keep it */
		if (matches(oldtext, strlen(oldtext), "hex")) {
			continue;
		}

		/* Names can be abbreviated,
		 * so match up to whichever is shorter */
		leng = MIN(strspn(oldtext, alphabet), strspn(newtext, alphabet));
		if (matches(newtext, leng, oldtext)== YES) {
			/* If it's something other than param, we can get
			 * rid of the existing one */
			if (strncmp(newtext, "par", 3) != 0) {
				*stuff_p_p = (*stuff_p_p)->next;
			}
			else {
				/* Compare parameter numbers and if they match,
				 * delete the existing one.
				 */
				char *new_eq_p, *old_eq_p;	/* loc of '=' */
				int oldparm, oldval;
				int newparm, newval;

				new_eq_p = strchr(newtext, '=');
				old_eq_p = strchr(oldtext, '=');
				if (new_eq_p != 0 && old_eq_p != 0) {
					if (get_param(new_eq_p + 1,
							stuff_p->inputfile,
							stuff_p->inputlineno,
							&newparm, &newval)
							== YES &&
							get_param(old_eq_p + 1,
							(*stuff_p_p)->inputfile,
							(*stuff_p_p)->inputlineno,
							&oldparm, &oldval)
							== YES &&
							oldparm == newparm) {
						*stuff_p_p = (*stuff_p_p)->next;
					}
				}
			}

			if (*stuff_p_p == 0) {
				/* If deleted last item in the existing list,
				 * jump out, so we won't try to deference
				 * this null pointer. */
				break;
			}
		}
	}

	/* Just do everything at beat zero. The beat at which user specified
	 * might not even exist in the current time signature, and basically
	 * we just want to do everything ASAP anyway. We know there is never
	 * a 'til' on midi, so no need to deal with that. We could end up
	 * with a lot to do at count zero, potentially enough to overwhelm
	 * the limited MIDI bandwidth and delay the first actual note.
	 * But that potential is always there, and by discarding things
	 * that were overwritten later, which we did above, unless there
	 * are an awfully lot of parameters on every possible channel,
	 * it's probably only going to take less than 0.1 second,
	 * so it doesn't seem worth the bother to try to do something
	 * fancy to calculate how much time we need for this stuff
	 * and delay the actual music by that much. */
	stuff_p->start.count = 0.0;
	stuff_p->start.steps = 0.0;

	/* link onto end of list */
	*stuff_p_p = stuff_p;
	stuff_p->next = 0;
}


/* Given the argument to a MIDI "parameter" command, extract and return
 * (via pointers) the two numbers (the parameter number and its value).
 * If successful, returns YES, else prints a warning and returns NO.
 * If NO is returned, the pointed to return values are not fill in.
 */

int
get_param(arg, inputfile, inputlineno, parmnum_p, parmval_p)

char *arg;		/* the argument part after "parameter=" */
char *inputfile;	/* for error messages */
int inputlineno;	/* for error messages */
int *parmnum_p;		/* parameter number is returned here */
int *parmval_p;		/* parameter value is returned here */

{
	int parmnum;	/* parameter number */
	int parmval;	/* parameter value */
	char *parm_p;	/* pointer to current place in string */


	/* extract first number */
	parmnum = strtol(arg, &parm_p, 10);
	if (parm_p == arg) {
		l_warning(inputfile, inputlineno,
			"parameter requires two values");
		return(NO);
	}
	/* skip white space, if any */
	while (*parm_p == ' ' || *parm_p == '\t') {
		parm_p++;
	}
	/* next we better have a comma */
	if (*parm_p != ','){
		l_warning(inputfile, inputlineno,
			"parameter is missing required comma");
		return(NO);
	}
	/* extract the second number */
	arg = parm_p + 1;
	parmval = strtol(arg, &parm_p, 10);
	if (parm_p == arg) {
		l_warning(inputfile, inputlineno,
			"parameter is missing second value");
		return(NO);
	}
	/* verify both numbers are within range */
	if ((l_rangecheck(parmnum, 0, 127, "parameter number",
			inputfile, inputlineno) == YES) &&
			(l_rangecheck(parmval, 0, 127, "parameter value",
			inputfile, inputlineno) == YES)) {
		*parmnum_p = parmnum;
		*parmval_p = parmval;
		return(YES);
	}
	return(NO);
}


/* Return YES if the given staff should be audible. It should be audible
 * if it is visible, or if it is a tabnote staff and the tab staff
 * is visible. */
int
staff_audible(staff)

int staff;	/* which staff */

{
	if (svpath(staff, VISIBLE)->visible == YES) {
		return(YES);
	}
	if ( (staff < Score.staffs)
			&& (is_tab_staff(staff + 1) == YES)
			&& (svpath(staff + 1, VISIBLE)->visible == YES) ) {
		return(YES);
	}
	return(NO);
}


/* given a NOTE, return the corresponding MIDI note number */

int
get_raw_notenum(note_p, fname, lineno, staff)

struct NOTE *note_p;
char *fname;	/* input file */
int lineno;	/* input line number */
int staff;	/* which staff note is associated with, for carryacc */

{
	int val;

	switch (note_p->letter) {
	case 'a':
		val = 9;
		break;
	case 'b':
		val = 11;
		break;
	case 'c':
		val = 0;
		break;
	case 'd':
		val = 2;
		break;
	case 'e':
		val = 4;
		break;
	case 'f':
		val = 5;
		break;
	case 'g':
		val = 7;
		break;
	default:
		pfatal("invalid note letter 0x%x on staff %d from line %d\n", note_p->letter, staff, lineno);
		/*NOTREACHED*/
		return(0);
	}

	/* adjust for octave */
	val += 12 * (note_p->octave + 1);

	/* If user overlaps octave marks inside a measure, which we don't
	 * catch, or if they transpose a note so many octaves that it is out
	 * of range (like transposing something down when it's already in
	 * octave 0) a note could end up out of range, so catch that here. */
	if (note_p->octave < MINOCTAVE || note_p->octave > MAXOCTAVE ||
				val < 0 || val >= MAXMIDINOTES) {
		l_ufatal(fname, lineno, "note is out of range");
	}


	/* return the value not considering accidentals. This is needed to
	 * keep track of ties properly. */
	return val;
}

/* initialize map of accidentals, based on key signature. */

void
init_accidental_map(staffno)

int staffno;

{
	register int n;

	/* first clear the map for all MIDI notes */
	for (n = 0; n < MAXMIDINOTES; n++) {
		/* If a note with any sort of accidental on it is currently
		 * tied, then we don't clear that note's accidental map,
		 * but mark that we will need to set the key signature
		 * value later, after the tie ends. Otherwise we set it
		 * immediately. */
		if (Tie_table[n] == YES) {
			/* set to zero for now. Will set to something else
			 * in mark_accidental later if appropriate. */
			standard_to_acclist('n', Deferred_acc[n]);
		}
		else {
			standard_to_acclist('n', Accidental_map[n]);
			Deferred_acc[n][0] = NO_DEFERRED_ACC;
		}
	}

	/* now fill in the key signature items in all octaves */
	switch (eff_key(staffno) ) {
	case 7:
		/* B# */
		mark_accidental(11, 1);
		/*FALLTHRU*/
	case 6:
		/* E# */
		mark_accidental(4, 1);
		/*FALLTHRU*/
	case 5:
		/* A# */
		mark_accidental(9, 1);
		/*FALLTHRU*/
	case 4:
		/* D# */
		mark_accidental(2, 1);
		/*FALLTHRU*/
	case 3:
		/* G# */
		mark_accidental(7, 1);
		/*FALLTHRU*/
	case 2:
		/* C# */
		mark_accidental(0, 1);
		/*FALLTHRU*/
	case 1:
		/* F# */
		mark_accidental(5, 1);
		break;
	case 0:
		break;
	case -7:
		/* F& */
		mark_accidental(5, -1);
		/*FALLTHRU*/
	case -6:
		/* C& */
		mark_accidental(0, -1);
		/*FALLTHRU*/
	case -5:
		/* G& */
		mark_accidental(7, -1);
		/*FALLTHRU*/
	case -4:
		/* D& */
		mark_accidental(2, -1);
		/*FALLTHRU*/
	case -3:
		/* A& */
		mark_accidental(9, -1);
		/*FALLTHRU*/
	case -2:
		/* E& */
		mark_accidental(4, -1);
		/*FALLTHRU*/
	case -1:
		/* B& */
		mark_accidental(11, -1);
		break;
	default:
		pfatal("unknown key signature");
		break;
	}
}


/* initialize table to say that no notes are tied */

void
init_tie_table()

{
	register int i;


	for (i = 0; i < MAXMIDINOTES; i++) {
		Tie_table[i] = NO;
	}
}


/* in each octave, mark the given note as having the given accidental, if not
 * tied. If tied, mark to set it later. */

void
mark_accidental(pitch_offset, acc)

int pitch_offset;	/* 0 = C, 2 = D, 4 = E, 5 = F, 7 = G, 9 = A, 11 = B */
int acc;		/* 1 = sharp, -1 = flat */

{
	register int n;

	for (n = pitch_offset; n < MAXMIDINOTES; n += 12) {
		if (Tie_table[n] == YES) {
			standard_to_acclist(Acclets[acc+2], Deferred_acc[n]);
		} else {
			standard_to_acclist(Acclets[acc+2], Accidental_map[n]);
		}
	}
}


/* Given a frequency, fill in the 3 bytes pointed to by value_p with
 * the correct values to represent that frequency in a tuning map sysex. */

static void
freq2tuningval(freq, value_p)

double freq;
unsigned char *value_p;

{
        float semitones, temp;
        int byte1, byte2, byte3;

        float midi0freq = 440.0 / pow(2.0, 9.0 / 12.0) / (1 << 5);

        semitones = log(freq / midi0freq) * 12 / log(2);

        /* round to closest representable value */
        semitones += 0.5 / (1 << 14);
        byte1 = semitones;
        temp = (semitones - byte1) * (1 << 7);
        byte2 = temp;
        temp = (temp - byte2) * (1 << 7);
        byte3 = temp;

	value_p[0] = (unsigned char) byte1;
	value_p[1] = (unsigned char) byte2;
	value_p[2] = (unsigned char) byte3;
}


/* Output a tuning map to MIDI file. Returns number of bytes written. */

int
out_notemap(mfile, map_number)

int mfile;		/* write to this MIDI file */
int map_number;		/* Use this map number */

{
	unsigned char tuning_sysex[6];
	int bytes = 0;
	int numtunings;		/* how many notes entries there are */
	float freq;
	int i;
	int length = 0;


	set_map_index(map_number);

	/* Look up how many notes there are defined in this mapping */
	numtunings = notemap_size(map_number);

	/* Calculate the length of the sysex message. In addition to
	 * the bytes to say it is a tuning sysex, there are
	 * 4 bytes for each note, and a 1 byte end-of-sysex. */
	length = sizeof(tuning_sysex) + (numtunings * 4) + 1;

	/* Output sysex start indicator */
	tuning_sysex[0] = 0xF0;
	midiwrite(mfile, tuning_sysex, 1);
	bytes = 1;

	/* Output the sysex length */
	bytes += wr_varlength(mfile, (UINT32B) length);

	/* Build and output the tuning command */
	tuning_sysex[0] = 0x7F;		/* Universal realtime sysex */
	tuning_sysex[1] = 0x7F;		/* To all devices */
	tuning_sysex[2] = 0x08;		/* MIDI tuning command */
	tuning_sysex[3] = 0x02;		/* note change */
	tuning_sysex[4] = 0;	/* we just use MIDI map number 0 all the time,
				 * so we don't have to do parameters changes
				 * to switch maps. */
	tuning_sysex[5] = numtunings; /* how many note tunings follow */
	midiwrite(mfile, tuning_sysex, sizeof(tuning_sysex));
	bytes += sizeof(tuning_sysex);

	/* Output each tuning as note number and 3 byte frequency value */
	for (i = 0; i < numtunings; i++) {
		tuning_sysex[0] = notefreq(map_number, i, &freq);
		freq2tuningval(freq, &(tuning_sysex[1]) );
		midiwrite(mfile, tuning_sysex, 4);
		bytes += 4;
	}

	/* Add end-of-sysex */
	tuning_sysex[0] = 0xF7;
	midiwrite(mfile, tuning_sysex, 1);
	bytes++;

	return(bytes);
}


/* Given an acclist, return the net effective accidental if all the accidentals
 * in it are "standard" ones, and the net is within range of double flat
 * to double sharp. In all other cases, return BAD_ACCS_OFFSET. */

int
accs_offset(acclist)

char *acclist;

{
	int halfsteps;
	int a;

	halfsteps = 0;
	for (a = 0; a < 2 * MAX_ACCS; a += 2) {
		if (acclist[a] == 0) {
			break;
		}
		if ( (acclist[a] & 0xff) == FONT_MUSIC) {
			switch (acclist[a+1] & 0xff) {
			case C_DBLFLAT:
				halfsteps -= 2;
				break;
			case C_FLAT:
				halfsteps --;
				break;
			case C_NAT:
				break;
			case C_SHARP:
				halfsteps ++;
				break;
			case C_DBLSHARP:
				halfsteps += 2;
				break;
			default:
				/* non standard */
				return(BAD_ACCS_OFFSET);
			}
		}
		else {
			return(BAD_ACCS_OFFSET);
		}
	}
	if (halfsteps < -2 || halfsteps > 2) {
		return(BAD_ACCS_OFFSET);
	}
	else {
		return(halfsteps);
	}
}


/* Anyone compiling with -Wunused-result may not get warnings if the
 * return from write() is ignored even if it is explicitly invoked
 * as   (void) write(...)   to make it clear we know it is unused and
 * would be okay with that. So to quiet them, we make a function to
 * check the return.
 */

void
midiwrite(int fd, unsigned char * data, int length)
{
	if (write(fd, data, length) != length) {
		ufatal("write of midi file failed");
	}
}
