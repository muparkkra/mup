
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

/* This file contains functions that generate a MIDI file,
 * based on the data that Mup parse phase (and part of placement phase)
 * placed in the "main list." 
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#ifdef __WATCOMC__
#include <io.h>
#endif
#include "defines.h"
#include "structs.h"
#include "globals.h"


/* Minimum and maximum number of quarter notes per minute.
 * This should be ridiculously wide enough range and
 * also prevents division by zero when calculating microsecs per quarter */
#define MINQNPM		(10)
#define MAXQNPM		(1000)

/* Default value for microseconds per quarter note */
#define DFLT_USEC_PER_QUARTER	(500000L)

#define USEC_PER_MINUTE		(60L * 1000000)

/* it is possible to get a legitimate rational overflow, and we don't really
 * care about absolutely precise time, so when a rational gets bigger than
 * MAXMIDI_RAT, throw away the lower bits and reduce */
#define MAXMIDI_RAT	1000000L
/* delta can get multiplied by MIDI_FACTOR. Need to make sure it can
 * never overflow a signed long, or bad things can happen,
 * so need smaller limit. */
#define MAXDELTA_RAT	300000L


/* Almost all midi values have a 7 bit unsigned range */
#define MIDI_MIN	(0)
#define MIDI_MAX	(127)

/* save information about stuff for the current measure so that it can
 * be applied at the proper time. */
struct MIDISTUFF {
	RATIONAL	time;		/* when to do the MIDI event */
	struct STUFF	*stuff_p;	/* STUFF to do */
	struct MIDISTUFF *next;		/* linked list */
};

/* if a roll spans multiple groups, save information about the other
 * groups. The reason for saving it around rather than just walking down the
 * gs_p chord list is that if the added groups have to go at the very
 * beginning of the measure, the only way to find where to link them is to
 * go back to the main list and search for the appropriate STAFF and follow
 * one of the groups_p lists. Since we'll get around to visiting that STAFF
 * eventually anyway, just save the info and do it when we get there. */
struct MIDIROLL {
	struct GRPSYL	*gs_p;		/* group roll applies to */
	short		notesbefore;	/* how many notes duration of rest
					 * to add before this group's portion
					 * of the roll */
	RATIONAL	duration;	/* duration of each note in roll */
	struct MIDIROLL	*link_p;	/* linked list  */
};

/* stuff is in floating point, but we keep track of time in RATIONAL. So
 * when we need to convert, put in terms of 1/F2RFACTOR of a count */
#define F2RFACTOR	((UINT32B)(192L * 256L))

/* Number of ticks in a whole note */
#define MIDI_FACTOR	((UINT32B)(4L * Division))

/* default note on velocity */
#define DFLT_VELOCITY	(64)

/* the MIDI STUFF info for current measure/voice */
static struct MIDISTUFF *Midistufflist_p;

/* list of pending rolls to do */
static struct MIDIROLL *Midirollinfo_p;

/* map staff number and voice index to track */
static short Voice2track_map [MAXSTAFFS + 1] [MAXVOICES];


/* keep track of all time to an absolute reference so that all tracks stay
 * in sync, even though midi times are stored as delta times */
static RATIONAL Absolute_time;
static RATIONAL Sum_of_deltas;

static int Status;	/* 0 if haven't yet written first MIDI status byte
			 * for current track. Otherwise is the current MIDI
			 * status byte. */

static short Channel = 0;	/* MIDI channel, 0-15 */
static char Onvelocity[MAXHAND];	/* note on velocity */
static char Offvelocity[MAXHAND];	/* note off velocity */
static short Time_specified_by_user = NO;	/* YES if user had a score SSV
				 * setting the time before any music data */
static short Key_specified_by_user = NO;	/* YES if user had a score SSV
				 * setting the key before any music data */
static int Division = DEFDIVISION;	/* clock ticks per quarter note */

static UINT32B Usec_per_quarter_note = DFLT_USEC_PER_QUARTER;

static short Pedbounce = NO;	/* if pedal bounce pending */

/* The valid keywords, listed here in alphabetical order */
static char *KW_channel = "channel";
static char *KW_chanpressure = "chanpressure";
static char *KW_copyright = "copyright";
static char *KW_cue = "cue";
static char *KW_hex = "hex";
static char *KW_instrument = "instrument";
static char *KW_marker = "marker";
static char *KW_name = "name";
static char *KW_offvelocity = "offvelocity";
static char *KW_onvelocity = "onvelocity";
static char *KW_parameter = "parameter";
static char *KW_port = "port";
static char *KW_program = "program";
static char *KW_seqnum = "seqnum";
static char *KW_tempo = "tempo";
static char *KW_text = "text";

/* local functions */
static RATIONAL eff_meas_time P((struct MAINLL *mll_p));
static void midi_header P((int mfile, int ntracks));
static void track_header P((int mfile));
static UINT32B write_midi_data P((int mfile, struct GRPSYL *gs_p));
static UINT32B midi_multirest P((int mfile, struct STAFF *staff_p, int staffno,
		int vno, int nummeas));
static int xlate_note P((struct NOTE *note_p, char *fname, int lineno,
		int staff, int *raw_notenum_p));
static void prepmidi_stuff P((struct STAFF *staff_p, int vno, int all));
static UINT32B do_midi_stuff P((RATIONAL timeval, int mfile, int all));
static UINT32B midihex P((int mfile, char *str, char *fname, int lineno));
static UINT32B midi_item P((struct STUFF *stuff_p, int mfile, int all));
static UINT32B wr_meta P((int mfile, int evtype, char *str));
static UINT32B all_midi P((int mfile));
static void midi_adjust P((void));
static void adjust_notes P((struct GRPSYL *gs_p, int staffno, int v,
		struct MAINLL *mll_p));
void steal4grace P((struct MAINLL *mll_p, int staffno, int voice,
		struct GRPSYL *prevgs_p, struct GRPSYL *gracelist_p,
		int numgrace));
RATIONAL calc_graceadj P((struct GRPSYL *gs_p, int numgrace));
static void add_release P((struct GRPSYL *gs_p, RATIONAL release_adjust,
		struct MAINLL *mll_p));
static UINT32B pedswitch P((int nfile, int on));
static void midi_roll P((struct GRPSYL *gs_p, struct GRPSYL **gslist_p_p));
static RATIONAL roll_time P((RATIONAL grptime, int nnotes));
static void do_mroll P((struct GRPSYL *gs_p, struct GRPSYL **gslist_p_p,
		RATIONAL rolltime, int notesbefore));
static void addrollgrp P((struct GRPSYL *gs_p, RATIONAL duration, int start,
		int end, struct GRPSYL **link_p_p, struct GRPSYL *prev_p));
static void savemidiroll P((struct GRPSYL *gs_p, int notesbefore,
		RATIONAL duration));
static struct MIDIROLL *getmidiroll P((struct GRPSYL *gs_p));
static void fix_tempo P((int to_end));
static void free_midistuff P((struct MIDISTUFF *ms_p));
static void adj4squeeze P((RATIONAL timeval));
static void comb_all_accs P((void));
static void comb_accs P((struct NOTE *note_p, char *fname, int linenum));


/* generate a MIDI file. Assigns each staff/voice combo to a MIDI track.
 * Write MIDI file header and first track with tempo, etc info. Then for
 * each staff/voice, generate a MIDI track with all note on/off and
 * whatever STUFF we know how to deal with. */

void
gen_midi(midifilename)

char *midifilename;		/* put MIDI data in this file */

{
	struct MAINLL *mll_p;	/* to index through main list */
	int i;
	int track = 0;
	int staff;
	int vno;		/* voice index */
	int mfile;		/* file descriptor for MIDI output file */
	UINT32B track_size;	/* bytes in track */
	UINT32B track_start;	/* offset in file where track begins */
	int t;			/* track index */
	/* tables for mapping track to staff number and voice index */
	short track2staff_map [MAXSTAFFS * MAXVOICES];
	short track2voice_map [MAXSTAFFS * MAXVOICES];
	int curr_map;		/* which tuning map we are using now */
	int saw_a_staff;	/* YES if we have seen a STAFF since the most
				 * recent run of SSVs */
	int got_data;		/* YES or NO, to deal with case where the
				 * number of staffs/voices changes mid-stream */
	struct STAFF *staff_p;	/* need to refer to STAFF a lot, so keep a
				 * pointer to it. */
	struct STAFF *last_staff_p = (struct STAFF *) 0;/* in case measure
				 * was invisible or something,
				 * this points to the most recent
				 * staff for current loop */
	struct GRPSYL *g_p;


	debug(256, "gen_midi");

	/* first go through main list, and find out which staff/voice pairs
	 * are used, and assign each to a track. */
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
					mll_p = mll_p->next) {

		if (mll_p->str == S_STAFF) {
			staff = mll_p->u.staff_p->staffno;

			/* ignore tab staffs -- we use the notes on the
			 * associated tabnote staff above it instead */
			if (is_tab_staff(staff) == YES) {
				continue;
			}

			for (vno = 0; vno < MAXVOICES; vno++) {

				if (mll_p->u.staff_p->groups_p[vno] ==
						(struct GRPSYL *) 0) {
					continue;
				}

				if (Voice2track_map [staff] [vno] == 0) {
					/* haven't allocated this staffno/vno to
					 * a track yet, so do so now. */
					track2staff_map [track] = (short) staff;
					track2voice_map [track] = (short) vno;
					Voice2track_map [staff] [vno] = ++track;
					debug(512, "assigned staff %d voice %d to track %d",
							staff, vno + 1, track);
				}
			}
		}
	}

	if (track == 0) {
		ufatal("no note data found");
	}

	/* open the specified MIDI file */
#ifdef O_BINARY
	if ((mfile = open(midifilename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666))
#else
	if ((mfile = open(midifilename, O_WRONLY | O_CREAT | O_TRUNC, 0666))
#endif
									< 0) {
		ufatal("can't open MIDI file '%s'", midifilename);
	}

	/* adjust grace notes to get a little time, etc */
	midi_adjust();

	/* squeeze out any all-space chords */
	midi_squeeze();

	/* the default Guitar tablature staff notes get transposed an octave,
	 * so do that if appropriate */
	guitar_transpose();

	/* if the tuning feature was used, generate all the MIDI note maps */
	if (Tuning_used == YES) {
		gen_tuning_maps(track, track2staff_map, track2voice_map);
	}
	else {
		comb_all_accs();
	}

	/* generate MIDI file header */
	initstructs();
	Usec_per_quarter_note = DFLT_USEC_PER_QUARTER;
	midi_header(mfile, track);

	/* go through  the main list once for each staff/voice, generating a
	 * MIDI track for it. */
	for (t = 0; t < track; t++)  {

		/* initialize everything for this track */
		initstructs();
		init_tie_table();
		reset_sigs();
		track_start = lseek(mfile, 0L, SEEK_CUR);
		track_header(mfile);
		staff = track2staff_map[t];
		vno = track2voice_map[t];
		curr_map = 0;
		set_map_index(curr_map);
		saw_a_staff = NO;
		track_size = 0;
		got_data = NO;
		Octave_adjust[staff] = 0;
		Octave_bars[staff] = 0;
		Octave_count[staff] = 0.0;
		Channel = 0;
		for (i = 0; i < MAXHAND; i++) {
			Onvelocity[i] = (char) DFLT_VELOCITY;
			Offvelocity[i] = (char) 0;
		}

		/* go through main list */
		for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {

			switch (mll_p->str) {

			case S_STAFF:
				saw_a_staff = YES;
				staff_p = mll_p->u.staff_p;
				/* check if this is for the staff we are
				 * currently generating midi data for */
				if (staff_p->staffno == staff
						&& staff_p->groups_p[vno]
						!= (struct GRPSYL *) 0) {

					/* Handle inaudible staffs, or
					 * staffs that happen to be tab now,
					 * even though they aren't somewhere
					 * else */
					if (staff_audible(staff) == NO || is_tab_staff(staff) == YES) {
						/* Convert notes to rests.
						 * Can't just ignore,
						 * because if we do, space
						 * that has been squeezed
						 * out isn't handled right,
						 * and tracks can get out of
						 * sync with each other */
						for (g_p = staff_p->groups_p[vno];
								g_p != 0;
								g_p = g_p->next) {
							if (g_p->nnotes > 0) {
								g_p->grpcont
								= GC_REST;
							}
						}
					}

					/* found information for the track/voice
					 * we are currently generating */
					got_data = YES;

					/* check for multi-rest */
					if (staff_p->groups_p[vno]->is_multirest
								== YES) {
						track_size += midi_multirest
							(mfile, staff_p,
							staff, vno,
							-(staff_p->groups_p[vno]
							->basictime) );
						break;
					}

					/* generate MIDI data */
					init_accidental_map(staff);
					prepmidi_stuff(staff_p, vno, NO);
					track_size += write_midi_data(mfile,
						staff_p->groups_p[vno]);
				}
				else if (staff_p->staffno == staff) {
					/* Voice doesn't exist in this meas,
					 * so just keep track of the staff
					 * so when we hit the bar we can
					 * add the silence, and update any
					 * midi parameters and such. */
					last_staff_p = staff_p;
				}
				break;

			case S_SSV:
				asgnssv(mll_p->u.ssv_p);

				/* update key signature for this track
				 * if necessary. Note that score-wide
				 * key signature changes will be written
				 * out via all_midi(). */
				if (mll_p->u.ssv_p->context == C_STAFF &&
						mll_p->u.ssv_p->staffno
						== staff &&
						(mll_p->u.ssv_p->used[SHARPS]
						== YES ||
						mll_p->u.ssv_p->used[TRANSPOSITION]
						== YES ||
						mll_p->u.ssv_p->used[ADDTRANSPOSITION]
						== YES) ) {
					track_size += midi_keysig(mfile,
						eff_key(mll_p->u.ssv_p->staffno),
						mll_p->u.ssv_p->is_minor);
					Status = 0;
				}
				if (TUNEPARMSSV(mll_p->u.ssv_p)) {
					if (saw_a_staff == YES) {
						set_map_index(++curr_map);
					}
					/* If there are other SSVs having
					 * tune parameters before we see
					 * another staff, we don't want to
					 * increment the curr_map */
					saw_a_staff = NO;
				}
				break;

			case S_BAR:
				/* if this voice is defined somewhere in the
				 * song, but not in this measure, need to add
				 * virtual measure of rest. (This could happen
				 * if user changed the number of staffs
				 * and/or voices in mid-stream */
				if (got_data == NO && mll_p->inputlineno != -1) {
					/* Arrange to do any midi things
					 * for this voice, even though it
					 * doesn't exist at the moment. */
					if (last_staff_p != (struct STAFF *) 0) {
						prepmidi_stuff(last_staff_p,
								vno, NO);
					}
					/* This will update the current
					 * absolute time to include the
					 * effective time for this measure. */
					track_size += do_midi_stuff(
							eff_meas_time(mll_p),
							mfile, NO);
					last_staff_p = (struct STAFF *) 0;
				}
				else {
					got_data = NO;
				}
				break;

			default:
				break;
			}

			if (mll_p == (struct MAINLL *) 0) {
				/* Really shouldn't ever happen,
				 * but just in case, don't core dump... */
				break;
			}
		}

		/* add end of track mark */
		track_size += write(mfile, "\0\377/\0", 4);

		/* now that we know the track size, fill it in */
		fix_track_size(mfile, track_start, track_size);
	}

	(void) close(mfile);
}


/* Find the "effective" duration of a measure. Because of squeezing of
 * space chords, a measure may be shorter than the time signature.
 * Given a spot in the main list, this finds the first visible voice
 * at or above that place, and counts up the time of the GRPSYLS in it,
 * ignoring squeezed-out spaces.
 */

static RATIONAL
eff_meas_time(mll_p)

struct MAINLL *mll_p;

{
	RATIONAL eff_time;	/* calculated value to return */
	struct GRPSYL *g_p;	/* to loop through groups */
	int v = 0;		/* voice index. Initialization is just
				 * to avoid bogus "used before set"
				  */

	/* find top visible voice */
	for (   ; mll_p != 0; mll_p = mll_p->prev) {
		if (mll_p->str == S_STAFF && svpath(mll_p->u.staff_p->staffno,
					VISIBLE)->visible == YES) {
			for (v = 0; v < MAXVOICES; v++) {
				if (vvpath(mll_p->u.staff_p->staffno, v+1,
						VISIBLE)->visible == YES) {
					break;
				}
			}
			if (v < MAXVOICES) {
				break;
			}
		}
	}
	if (mll_p == 0) {
		pfatal("eff_meas_time couldn't find a visible voice");
	}

	/* All up the time in this voice */
	eff_time = Zero;
	for (g_p = mll_p->u.staff_p->groups_p[v]; g_p != 0; g_p = g_p->next) {
		if (g_p->grpvalue == GV_ZERO && g_p->grpcont == GC_SPACE) {
			/* squeezed out space, so doesn't count */
			continue;
		}
		eff_time = radd(eff_time, g_p->fulltime);
	}
	return(eff_time);
}


/* write MIDI header to file */

static void 
midi_header(mfile, ntracks)

int mfile;	/* file descriptor to write to */
int ntracks;	/* how many tracks are to be written */

{
	unsigned char buff[8];
	UINT32B track1start;
	UINT32B trklength;


	debug(512, "midi_header");

	trklength = write(mfile, "MThd\0\0\0\6\0", 9);

	/* always use format 1 */
	buff[0] = 1;

	/* 2 bytes for number of tracks */
	/* add 1 for the track giving time signature, etc */
	buff[1] = (unsigned char) (ntracks + 1) >> 8;
	buff[2] = (unsigned char) (ntracks + 1) & 0xff;

	/* division field. */
	buff[3] = (unsigned char) (Division >> 8);
	buff[4] = (unsigned char) (Division & 0xff);
	midiwrite(mfile, buff, 5);

	/* now do first track, which gives time and key signature info */
	reset_sigs();
	track1start = lseek(mfile, 0L, SEEK_CUR);
	track_header(mfile);
	trklength = 0;

	/* if there is a header and the first item to print is centered,
	 * it's probably a title, so do it as a text event. */
	if (Header.printdata_p != (struct PRINTDATA *) 0) {
		if (Header.printdata_p->justifytype == J_CENTER &&
				Header.printdata_p->string != (char *) 0) {
			trklength += write_delta(mfile);
			buff[0] = 0xff;
			buff[1] = 0x01;
			trklength += write(mfile, buff, 2);
			trklength += midi_wrstring(mfile,
					Header.printdata_p->string, YES);
		}
	}
	/* do default time signature if necessary */
	if (Time_specified_by_user == NO) {
		trklength += midi_timesig(mfile);
	}

	/* do default key signature if necessary */
	if (Key_specified_by_user == NO) {
		trklength += midi_keysig(mfile, eff_key(0), Score.is_minor);
	}

	/* output usecs per quarter note */
	trklength += write(mfile, "\0\377Q\3", 4);
	buff[0] = (Usec_per_quarter_note >> 16) & 0xff;
	buff[1] = (Usec_per_quarter_note >> 8) & 0xff;
	buff[2] = Usec_per_quarter_note & 0xff;
	trklength += write(mfile, buff, 3);

	/* do everything else for track 1 */
	trklength += all_midi(mfile);

	/* end of track marker */
	trklength += write(mfile, "\0\377/\0", 4);
	fix_track_size(mfile, track1start, trklength);
}


/* write a MIDI track header */

static void
track_header(mfile)

int mfile;	/* write track header to this file descriptor */

{
	debug(512, "track_header");

	midiwrite(mfile, (unsigned char *) "MTrk\0\0\0\0", 8);

	/* reset time reference */
	Absolute_time = Sum_of_deltas = Zero;

	/* reset "running status" */
	Status = 0;
}


/* write MIDI info. Return number of bytes written to mfile */

static UINT32B
write_midi_data(mfile, gs_p)

int mfile;		/* write MIDI data to this file descriptor */
struct GRPSYL *gs_p;	/* write info about these chords */

{
	UINT32B bytes = 0;	/* number of bytes written */
	int n;			/* walk through notes of chord */
	unsigned char buff[4];	/* temp storage for MIDI data */
	int notenum;		/* MIDI note number 0 to MAXMIDINOTES - 1*/
	int raw_notenum;	/* note numer not counting accidentals */
	short newstatus;	/* running status */


	/* go through each GRPSYL in the measure */
	for (  ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {

		/* do any MIDI stuffs that happen right on this beat. They
		 * should happen after notes have been turned off for previous
		 * chord but before the notes for the following chord */
		bytes += do_midi_stuff(Zero, mfile, NO);

		/* if rest or space, just keep track of time used. */
		if ( gs_p->grpcont != GC_NOTES) {
			/* special case of all-space chord. It gets no time.
			 * Just adjust pending MIDI events so they happen
			 * at the right time */
			if (gs_p->grpcont == GC_SPACE && gs_p->grpvalue == GV_ZERO) {
				adj4squeeze(gs_p->fulltime);
				bytes += do_midi_stuff(Zero, mfile, NO);
			}
			else {
				bytes += do_midi_stuff(gs_p->fulltime, mfile, NO);
			}
			continue;
		}

		/* turn on each note in chord */
		for (n = 0; n < gs_p->nnotes; n++) {

			notenum = xlate_note( &(gs_p->notelist[n]),
					gs_p->inputfile, gs_p->inputlineno,
					gs_p->staffno, &raw_notenum);

			/* if this note is tied from previous, it is already
			 * turned on, so just mark off that we've done the tie.
			 */
			if (Tie_table[raw_notenum] == YES) {
				Tie_table[raw_notenum] = NO;
			}

			else {
				/* not tied from previous, so turn note on */
				bytes += write_delta(mfile);
	
				/* first time through have to put the status.
				 * After that we can use running status */
				newstatus = (0x90 | Channel) & 0xff;
				if (Status != newstatus) {
					buff[0] = (unsigned char) newstatus;
					midiwrite(mfile, buff, 1);
					Status = newstatus;
					bytes++;
				}

				buff[0] = (unsigned char) notenum;
				buff[1] = (unsigned char) Onvelocity[n];
				midiwrite(mfile, buff, 2);
				bytes += 2;
			}
		}
		
		bytes += do_midi_stuff(gs_p->fulltime, mfile, NO);

		/* now turn all the notes off, unless tied */
		for (n = 0; n < gs_p->nnotes; n++) {

			notenum = xlate_note( &(gs_p->notelist[n]),
				gs_p->inputfile, gs_p->inputlineno,
				gs_p->staffno, &raw_notenum);

			/* if this note is tied to next, mark that */
			/* Note that it would be nice if we could do the tie
			 * to a tied-to voice. But if the voice is on
			 * a different channel, then it is impossible.
			 * Even if it is the same channel, we put every voice
			 * in a separate track, so it is not entirely
			 * clear if all MIDI players would handle a note
			 * being turned on in one track and off
			 * in a different one. Even if that would work,
			 * we process each track in a separate pass
			 * through the main list, so while it would be easy
			 * enough to ignore the normal "note off"
			 * in the current voice, identifying that
			 * the other voice should not have a "note on"
			 * would be harder. Probably the best way would be
			 * to have parse set a new tied_from_voice field
			 * in NOTE, so our MIDI code earlier in this function
			 * could treat that field being other than NO_TO_VOICE
			 * like Tie_table[raw_notenum] being YES.
			 * But only if the channels match,
			 * which we wouldn't know without considerable work.
			 * Basically, we would have to walk through the
			 * tied_from_voice's GRPSYLs, either from the beginning
			 * to current place, or backwards from current place
			 * til a channel change STUFF is found.
			 * That seems too much work to bother for such a
			 * fringe case, especially since we can never do
			 * non-channel-matched cases, which may seem the
			 * same to a user, so may be better to consistently
			 * not even try. So we pretend there is no tie,
			 * which will result in a glich in sound, which,
			 * depending on the instrument and release and other
			 * things going on, may or may not be objectionable.
			 * But it's at least better than leaving the note
			 * stuck on, which we were accidentally doing
			 * for a while.
			 */
			if ( ((gs_p->notelist[n].tie == YES) || (gs_p->tie == YES) )
				&& (gs_p->notelist[n].tied_to_voice == NO_TO_VOICE)) {
					Tie_table[raw_notenum] = YES;
			}
			else {
				if (gs_p->notelist[n].tied_to_voice != NO_TO_VOICE) {
					l_warning(gs_p->inputfile, gs_p->inputlineno,
					"tie to another voice is not supported for MIDI, so will be omitted; try using ifdef MIDI to tie to same voice");
				}
				/* not tied to next, so turn off */
				bytes += write_delta(mfile);

				/* use note on with onvelocity 0 (which means
				 * note off), unless user explicitly set an
				 * off velocity */
				if (Offvelocity[n] != 0) {
					newstatus = (0x80 | Channel) & 0xff;
				}
				else {
					newstatus = (0x90 | Channel) & 0xff;
				}
				if (Status != newstatus) {
					buff[0] = (unsigned char) newstatus;
					midiwrite(mfile, buff, 1);
					Status = newstatus;
					bytes++;
				}
				buff[0] = (unsigned char) notenum;
				buff[1] = (unsigned char) Offvelocity[n];
				midiwrite(mfile, buff, 2);
				bytes += 2;

				/* If we had to defer the setting of
				 * sharps/flats because of a tie into the
				 * measure, do that now. */
				if (Deferred_acc[raw_notenum][0] !=
						(char) NO_DEFERRED_ACC) {
					COPY_ACCS(Accidental_map[raw_notenum],
						  Deferred_acc[raw_notenum]);
					Deferred_acc[raw_notenum][0] = NO_DEFERRED_ACC;
				}
			}
		}
	}

	/* do any midi events that happen at the end of the measure after
	 * the notes. */
	bytes += do_midi_stuff(Zero, mfile, NO);

	return(bytes);
}


/* write out delta value. Return number of bytes written */

UINT32B
write_delta(mfile)

int mfile;	/* file descriptor of MIDI file */

{
	UINT32B idelta;		/* delta rounded to 32-bit integer */
	RATIONAL delta;
	RATIONAL rounded;	/* idelta converted back to RATIONAL */


	/* avoid rational overflow, which can happen under certain
	 * circumstances with lots of grace, rolls, etc */
	while (Absolute_time.n > MAXMIDI_RAT
				|| Absolute_time.d > MAXMIDI_RAT) {
		/* avoid rational divide by zero */
		if (Absolute_time.d > 1) {
			Absolute_time.n >>= 1;
			Absolute_time.d >>= 1;
			rred ( &Absolute_time );
		}
		else {
			break;
		}
	}
	while( Sum_of_deltas.n > MAXMIDI_RAT
				|| Sum_of_deltas.d > MAXMIDI_RAT) {
		if (Sum_of_deltas.d > 1) {
			Sum_of_deltas.n >>= 1;
			Sum_of_deltas.d >>= 1;
			rred ( &Sum_of_deltas );
		}
		else {
			break;
		}
	}

	delta = rsub(Absolute_time, Sum_of_deltas);
	if (LT(delta, Zero)) {
		delta = Zero;
	}

	/* Multiply by factor to get delta value in MIDI clock ticks,
	 * then round off to UINT32B. Entire calculation must be done
	 * in floating point, then cast to UINT32B, otherwise overflow
	 * can occur, which really messes things up!! */
	idelta = (UINT32B)((((double) MIDI_FACTOR * (double) delta.n)
						/ (double) delta.d) + 0.5);

	/* now convert the rounded-off value back to a RATIONAL,
	 * and add it to the Sum_of_deltas, so we'll know exactly how far
	 * off we are the next time around and can compensate. */
	rounded.n = idelta;
	rounded.d = MIDI_FACTOR;
	rred(&rounded);
	/* certain combinations of division value, release values, and
	 * input could cause rounded to overflow and become negative.
	 * Because of all the other code added to try to avoid overflow,
	 * this should be extremely unlikely, but it's better to catch
	 * it and give an error than leave the user to wonder why the
	 * generated MIDI file says to hold a note for several days. */
	if (LT(rounded, Zero)) {
		pfatal("arithmetic overflow on MIDI delta calculation, input probably too complex\n  (hint: changing the 'release' parameter might help)");
	}

	Sum_of_deltas = radd(Sum_of_deltas, rounded);

	return(wr_varlength(mfile, idelta));
}


/* given a NOTE, return the corresponding MIDI note number */

static int
xlate_note(note_p, fname, lineno, staff, raw_notenum_p)

struct NOTE *note_p;
char *fname;	/* input file */
int lineno;	/* input line number */
int staff;	/* which staff note is associated with, for carryacc */
int *raw_notenum_p;	/* return the note number without accidentals */

{
	int val;
	int acc_adjust;	/* half steps to adjust note due to accident */

	/* get the value not considering accidentals. This is needed to
	 * keep track of ties properly. */
	val = get_raw_notenum(note_p, fname, lineno, staff);
	*raw_notenum_p = val;

	/* if tuning feature was used, call find_note_number to do the work */
	if (Tuning_used == YES) {
		return (find_note_number(staff, note_p, *raw_notenum_p));
	}

	if (standard_acc(note_p->acclist) == '\0') {
		/* leave accidental as is from before */
		acc_adjust = 0;
		if (Accidental_map[val][0] != 0) {
			switch (Accidental_map[val][1]) {
			case C_SHARP:
				acc_adjust = 1;
				break;
			case C_FLAT:
				acc_adjust = -1;
				break;
			case C_DBLSHARP:
				acc_adjust = 2;
				break;
			case C_DBLFLAT:
				acc_adjust = -2;
				break;
			}
		}
	}
	else {
		acc_adjust = strchr(Acclets, standard_acc(note_p->acclist))
							- Acclets - 2;
	}

	/* If user wants accidentals carried through the bar (the normal case),
	 * we save the accidental for use on future notes in the bar.
	 * We also have to save in the case the note is tied.
	 */
	if (svpath(staff, CARRYACCS)->carryaccs == YES || note_p->tie == YES) {
		standard_to_acclist(Acclets[acc_adjust + 2],
				Accidental_map[val]);
	}

	/* the top few notes in octave 9 are outside midi range */
	if (val + acc_adjust >= MAXMIDINOTES) {
		l_ufatal(fname, lineno, "note out of range");
	}
	return (val + acc_adjust);
}



/* handle a multi-rest. Adjust the absolute time reference value to account
 * for the length of the multi-rest */

static UINT32B
midi_multirest(mfile, staff_p, staffno, vno, nummeas)

int mfile;		/* MIDI file */
struct STAFF *staff_p;
int staffno;
int vno;	/* voice number */
int nummeas;	/* how many measure of rest */

{
	RATIONAL rat_nummeas;	/* number of measure as a rational */


	if (staff_p != (struct STAFF *) 0) {
		prepmidi_stuff(staff_p, vno, NO);
	}

	/* if truly a multirest and if octave were in progress,
	 * need to adjust number of measures remaining */
	if (nummeas > 1 && Octave_bars[staffno] > 0) {
		/* subtract 1 'cause the barline will count as one */
		Octave_bars[staffno] -= nummeas - 1;
		/* if whole octave stuff is done, re-init */
		if (Octave_bars[staffno] < 0) {
			Octave_bars[staffno] = 0;
			Octave_count[staffno] = 0.0;
			Octave_adjust[staffno] = 0;
		}
	}

	rat_nummeas.n = nummeas;
	rat_nummeas.d = 1;
	return(do_midi_stuff(rmul(rat_nummeas, Score.time), mfile, NO));
}


/* go through STUFF in a measure, saving away info about MIDI stuff for
 * later use. */

static void
prepmidi_stuff(staff_p, vindex, all)

struct STAFF *staff_p;	/* do STUFF off of here */
int vindex;		/* voice index, 0 or 1 for voice 1 or 2 */
int all;		/* YES if processing 'all' stuff */

{
	struct STUFF *st_p;		/* walk through staff_p->stuff_p */


	Midistufflist_p = (struct MIDISTUFF *) 0;

	/* Insert entries for any pending gradual changes that should
	 * occur in this measure. */
	do_pending_gradual_midi();

	for (st_p = staff_p->stuff_p; st_p != (struct STUFF *) 0;
							st_p = st_p->next) {

		if (st_p->stuff_type == ST_MIDI
					|| st_p->stuff_type == ST_PEDAL) {

			/* only do those with proper 'all' value */
			if (st_p->all != all) {
				continue;
			}

			if (st_p->place == PL_ABOVE && vindex != 0) {
				/* above only applies to voice 1 */
				continue;
			}
			if (st_p->place == PL_BELOW && vindex != 1) {
				/* below only applies to voice 2 */
				if (st_p->stuff_type != ST_PEDAL) {
					continue;
				}
			}
			if (st_p->place == PL_BETWEEN && vindex != 2) {
				/* between only applies to voice 3 */
				continue;
			}

			/* Add to list */
			insert_midistufflist(st_p);
		}
	}
}


/* given a timeval to add to Absolute_time, see if there are any MIDI
 * STUFF events that come before then. If so, do them first. If timeval
 * is Zero, do any events happening exactly at Absolute_time. In any case
 * update Absolute_time appropriately. Return number of bytes written */

static UINT32B
do_midi_stuff(timeval, mfile, all)

RATIONAL timeval;
int mfile;		/* MIDI file */
int all;		/* YES if processing 'all' stuffs */

{
	RATIONAL new_abs_time;	/* Absolute_time plus timeval */
	struct MIDISTUFF *ms_p;	/* index through MIDISTUFF list */
	UINT32B bytes = 0;	/* bytes written */


	/* If need to bounce pedal, do that now */
	if (Pedbounce == YES && NE(timeval, Zero)) {
		RATIONAL instant;

		instant.n = 1;
		instant.d = MIDI_FACTOR;
		Absolute_time = radd(Absolute_time, instant);
		bytes += pedswitch(mfile, YES);
		Absolute_time = rsub(Absolute_time, instant);
		Pedbounce = NO;
	}

	/* find out what final time will be */
	new_abs_time = radd(Absolute_time, timeval);

	/* go through list of MIDI STUFF, to see if anything to do before
	 * final time */
	for (ms_p = Midistufflist_p; ms_p != (struct MIDISTUFF *) 0;  ) {

		if ( LT(ms_p->time, new_abs_time) || (EQ(timeval, Zero) &&
					EQ(ms_p->time, new_abs_time) ) ) {

			/* an item to do. Do it */
			Absolute_time = ms_p->time;
			bytes += midi_item(ms_p->stuff_p, mfile, all);

			/* free this item and move to next one */
			ms_p = ms_p->next;
			FREE(Midistufflist_p);
			Midistufflist_p = ms_p;
		}
		else {
			break;
		}
	}
	Absolute_time = new_abs_time;

	/* return number of bytes written */
	return(bytes);
}


/* handle a MIDI stuff item */

static UINT32B
midi_item(stuff_p, mfile, all)

struct STUFF *stuff_p;	/* which STUFF to process */
int mfile;		/* the MIDI file */
int all;		/* YES if processing "all" type items now */

{
	UINT32B bytes = 0;	/* bytes written */
	unsigned char buff[8];
	char *key;		/* midi directive keyword */
	int leng;		/* length of key */
	char *arg;		/* arg after the =  */
	int num;		/* atoi value of argument */
	int n;			/* note velocity index */
	char *nextvel_p;	/* location in string of next velocity value */


	if (stuff_p->stuff_type == ST_PEDAL) {
		int font, size;
		char *string;


		if (stuff_p->string == (char *) 0) {
			/* continuation of pedal into an ending or something
			 * similar. I don't think this will every actually
			 * happen, since that's done in a later phase. */
			return(0);
		}

		/* extract the pedal character */
		font = stuff_p->string[0];
		size = stuff_p->string[1];
		string = stuff_p->string + 2;

		/* turn pedal switch on or off as appropriate */
		switch(next_str_char(&string, &font, &size) & 0xff) {

		case C_BEGPED:
			bytes += pedswitch(mfile, YES);
			break;
		
		case C_PEDAL:
			bytes += pedswitch(mfile, NO);
			/* have to put pedal back up after the next chord */
			Pedbounce = YES;
			break;

		case C_ENDPED:
			bytes += pedswitch(mfile, NO);
			break;
		
		default:
			pfatal("bad character in pedal string");
			/*NOTREACHED*/
			break;
		}
		return(bytes);
	}

	/* figure out which keyword was specified */
	if (getkeyword(stuff_p->string + 2, &key, &leng, &arg) == NO) {
		l_warning(stuff_p->inputfile, stuff_p->inputlineno,
				"midi directive not in keyword=value format");
		return(0);
	}
	
	/* do the code for the appropriate keyword. There are enough keywords
	 * that it would almost be worthwhile doing a hash or binary search
	 * rather than sequentially checking each. However, most of them will
	 * probably be rarely used, so by checking the most common ones first,
	 * there will rarely be more than half a dozen checks anyway. */
	if (matches(key, leng, KW_program) == YES) {
		if (stuff_p->all == YES) {
			l_warning(stuff_p->inputfile, stuff_p->inputlineno,
				"midi program cannot be used with 'all'");
			return(0);
		}
		if (all == YES) {
			return(0);
		}

		num = atoi(arg);
		if (l_rangecheck(num, MIDI_MIN, MIDI_MAX, KW_program,
				stuff_p->inputfile, stuff_p->inputlineno)
				== YES) {
			bytes = write_delta(mfile);
			Status = buff[0] = (unsigned char) (0xc0 | Channel);
			buff[1] = (unsigned char) num;
			bytes += write(mfile, buff, 2);
			process_to_list(stuff_p, KW_program,
				Usec_per_quarter_note, MIDI_MIN, MIDI_MAX);
		}
	}

	else if (matches(key, leng, KW_tempo) == YES) {
		UINT32B quarter_notes_per_minute;

		/* tempo only applies to 'all' */
		if (stuff_p->all == NO) {
			l_warning( stuff_p->inputfile, stuff_p->inputlineno,
				"midi tempo can only be set using 'all'");
			return(0);
		}

		if (all == NO) {
			return(0);
		}

		quarter_notes_per_minute = atoi(arg);
		if (l_rangecheck(quarter_notes_per_minute, MINQNPM, MAXQNPM,
				KW_tempo, stuff_p->inputfile,
				stuff_p->inputlineno) == YES) {
			bytes = write_delta(mfile);
			buff[0] = (unsigned char) 0xff;
			buff[1] = (unsigned char) 0x51;
			buff[2] = (unsigned char) 0x3;
			Usec_per_quarter_note = USEC_PER_MINUTE
						/  quarter_notes_per_minute;
			buff[3] = (Usec_per_quarter_note >> 16) & 0xff;
			buff[4] = (Usec_per_quarter_note >> 8) & 0xff;
			buff[5] = (Usec_per_quarter_note & 0xff);
			bytes += write(mfile, buff, 6);
			Status = 0;
			process_to_list(stuff_p, KW_tempo,
				Usec_per_quarter_note, MINQNPM, MAXQNPM);
		}
	}

	else if (matches(key, leng, KW_onvelocity) == YES) {
		if (stuff_p->all == YES) {
			l_warning(stuff_p->inputfile, stuff_p->inputlineno,
				"midi onvelocity cannot be used with 'all'");
			return(0);
		}
		if (all == YES) {
			return(0);
		}
		num = atoi(arg);
		if (l_rangecheck(num, 1, MIDI_MAX, KW_onvelocity, stuff_p->inputfile,
				stuff_p->inputlineno) == YES) {
			Onvelocity[0] = (char) num;
		}
		/* if there are more velocities given, process them. If
		 * there are N velocities given, they give the velocities
		 * to use for the top N notes, the first for the top note,
		 * the second for the second to the top, etc. If there are
		 * more than N notes in a chord, the remaining notes are
		 * given the velocity of the final velocity specified.
		 * Thus, if only one is specified, it applies to all notes
		 * in the chord, whereas if two are given, the top note
		 * will have the first velocity and the remaining notes will
	 	 * have the second (which could be useful for emphasizing
		 * the melody, for example). And the user can specify each
		 * note's velocity separately if they want to.
		 */
		nextvel_p = strchr(arg, ',');
		for (n = 1; n < MAXHAND; n++) {
			
			/* if user has listed another velocity, save it */
			if (nextvel_p != (char *) 0) {
				num = atoi(++nextvel_p);
				if (l_rangecheck(num, 1, MIDI_MAX, KW_onvelocity,
						stuff_p->inputfile,
						stuff_p->inputlineno) == YES) {
					Onvelocity[n] = (char) num;
				}

				/* point to next velocity, if any, for next
				 * time through the loop */
				nextvel_p = strchr(nextvel_p, ',');
			}
			else {
				/* use the last user-specified velocity for
				 * all subsequent notes */
				Onvelocity[n] = (char) num;
			}
		}
		process_to_list(stuff_p, KW_onvelocity,
				Usec_per_quarter_note, 1, MIDI_MAX);
	}

	/* Note: have to check "channel" before "chanpressure" so that
	 * channel takes precedence if the keyword is abbreviated */
	else if (matches(key, leng, KW_channel) == YES) {
		if (stuff_p->all == YES) {
			l_warning(stuff_p->inputfile, stuff_p->inputlineno,
				"midi channel cannot be used with 'all'");
			return(0);
		}
		if (all == YES) {
			return(0);
		}

		num = atoi(arg);
		if (l_rangecheck(num, 1, 16, KW_channel, stuff_p->inputfile,
					stuff_p->inputlineno) == YES) {
			/* external MIDI channel numbers are 1-16,
			 * internal are 0-15 */
			Channel = num - 1;
			/* Output a "channel prefix" meta event */
			bytes = write_delta(mfile);
			buff[0] = 0xff;
			buff[1] = 0x20;
			buff[2] = 0x01;
			buff[3] = Channel;
			bytes += write(mfile, buff, 4);
			Status = 0;
			process_to_list(stuff_p, KW_channel,
				Usec_per_quarter_note, MIDI_MIN, MIDI_MAX);
		}
	}

	else if (matches(key, leng, KW_parameter) == YES) {
		int parmnum;	/* parameter number */
		int parmval;	/* parameter value */

		if (get_param(arg, stuff_p->inputfile, stuff_p->inputlineno,
					&parmnum, &parmval) == YES) {
			bytes += write_delta(mfile);
			Status = buff[0] = 0xb0 | Channel;
			buff[1] = (unsigned char) parmnum;
			buff[2] = (unsigned char) parmval;
			bytes += write(mfile, buff, 3);
			process_to_list(stuff_p, KW_parameter,
				Usec_per_quarter_note, MIDI_MIN, MIDI_MAX);
		}
	}

	else if (matches(key, leng, KW_offvelocity) == YES) {
		if (stuff_p->all == YES) {
			l_warning(stuff_p->inputfile, stuff_p->inputlineno,
				"midi offvelocity cannot be used with 'all'");
			return(0);
		}
		if (all == YES) {
			return(0);
		}
		num = atoi(arg);
		if (l_rangecheck(num, MIDI_MIN, MIDI_MAX, KW_offvelocity,
				stuff_p->inputfile,
				stuff_p->inputlineno) == YES) {
			Offvelocity[0] = (char) num;
		}
		/* if there are more velocities given, process them. 
		 * See description of onvelocity above for details.
		 */
		nextvel_p = strchr(arg, ',');
		for (n = 1; n < MAXHAND; n++) {
			
			/* if user has listed another velocity, save it */
			if (nextvel_p != (char *) 0) {
				num = atoi(++nextvel_p);
				if (l_rangecheck(num, 1, MIDI_MAX, KW_offvelocity,
						stuff_p->inputfile,
						stuff_p->inputlineno) == YES) {
					Offvelocity[n] = (char) num;
				}

				/* point to next velocity, if any, for next
				 * time through the loop */
				nextvel_p = strchr(nextvel_p, ',');
			}
			else {
				/* use the last user-specified velocity for
				 * all subsequent notes */
				Offvelocity[n] = (char) num;
			}
		}
		process_to_list(stuff_p, KW_offvelocity,
				Usec_per_quarter_note, MIDI_MIN, MIDI_MAX);
	}

	else if (matches(key, leng, KW_hex) == YES) {
		nix_til(stuff_p, KW_hex);
		return(midihex(mfile, arg, stuff_p->inputfile, stuff_p->inputlineno));
	}
	else if (matches(key, leng, KW_text) == YES) {
		nix_til(stuff_p, KW_text);
		return(wr_meta(mfile, 0x01, arg));
	}
	else if (matches(key, leng, KW_copyright) == YES) {
		nix_til(stuff_p, KW_copyright);
		return(wr_meta(mfile, 0x02, arg));
	}
	else if (matches(key, leng, KW_name) == YES) {
		nix_til(stuff_p, KW_name);
		return(wr_meta(mfile, 0x03, arg));
	}
	else if (matches(key, leng, KW_instrument) == YES) {
		nix_til(stuff_p, KW_instrument);
		return(wr_meta(mfile, 0x04, arg));
	}
	else if (matches(key, leng, KW_marker) == YES) {
		nix_til(stuff_p, KW_marker);
		return(wr_meta(mfile, 0x06, arg));
	}
	else if (matches(key, leng, KW_cue) == YES) {
		nix_til(stuff_p, KW_cue);
		return(wr_meta(mfile, 0x07, arg));
	}

	else if (matches(key, leng, KW_seqnum) == YES) {
		num = atoi(arg);
		if (l_rangecheck(num, 0, 65535, KW_seqnum, stuff_p->inputfile,
				stuff_p->inputlineno) == YES) {
			bytes = write_delta(mfile);
			buff[0] = 0xff;
			buff[1] = 0x00;
			buff[2] = 0x02;
			buff[3] = (num >> 8) & 0xff;
			buff[4] = num & 0xff;
			bytes += write(mfile, buff, 5);
			Status = 0;
			if (GT(Absolute_time, Zero) || stuff_p->start.count != 0.0) {
				l_warning(stuff_p->inputfile, stuff_p->inputlineno,
				"seqnum is only supposed to be at the very beginning of a song, at time zero");
			}
			nix_til(stuff_p, KW_seqnum);
		}
	}

	else if (matches(key, leng, KW_port) == YES) {
		num = atoi(arg);
		if (l_rangecheck(num, MIDI_MIN, MIDI_MAX, KW_port,
				stuff_p->inputfile, stuff_p->inputlineno)
				== YES) {
			bytes = write_delta(mfile);
			buff[0] = 0xff;
			buff[1] = 0x21;
			buff[2] = 0x01;
			buff[3] = num;
			bytes += write(mfile, buff, 4);
			Status = 0;
			process_to_list(stuff_p, KW_port, Usec_per_quarter_note,
							MIDI_MIN, MIDI_MAX);
		}
	}

	/* Note: have to check "channel" before "chanpressure" so that
	 * channel takes precedence if the keyword is abbreviated */
	else if (matches(key, leng, KW_chanpressure) == YES) {
		num = atoi(arg);
		if (l_rangecheck(num, MIDI_MIN, MIDI_MAX, KW_chanpressure,
				stuff_p->inputfile, stuff_p->inputlineno)
				== YES) {
			bytes += write_delta(mfile);
			buff[0] = 0xd0 | Channel;
			buff[1] = (unsigned char) num;
			bytes += write(mfile, buff, 2);
			Status = 0;
			process_to_list(stuff_p, KW_chanpressure,
				Usec_per_quarter_note, MIDI_MIN, MIDI_MAX);
		}
	}

	else {
		l_warning(stuff_p->inputfile, stuff_p->inputlineno,
				"unrecognized midi item '%s'", key);
	}

	return(bytes);
}


/* handle raw hex to output to midi file. Allow white space. Set running
 * status to 0. */

static UINT32B
midihex(mfile, str, fname, lineno)

int mfile;
char *str;
char *fname;
int lineno;

{
	short nibble = 0;	/* 0 = upper nibble, 1 = lower */
	UINT32B bytes = 0;	/* how many bytes written */
	unsigned char data;	/* a byte of data to write */


	bytes += write_delta(mfile);
	for (   ; *str != '\0'; str++) {

		/* skip white space */
		if (isspace(*str)) {
			continue;
		}

		/* collect two hex digits per byte to write */
		if (isxdigit(*str)) {
			if (nibble == 0) {
				data = hexdig(*str) << 4;
			}
			else {
				data |= hexdig(*str);
				midiwrite(mfile, &data, 1);
				bytes++;
			}
			nibble ^= 1;
		}
		else {
			l_ufatal(fname, lineno, "illegal hex character");
		}
	}

	if (nibble != 0) {
		l_ufatal(fname, lineno, "odd number of hex digits");
	}

	/* set running status to unknown and return number of bytes written */
	Status = 0;
	return(bytes);
}


/* write a meta event of the form
 *	FF xx length text
 * Return number of bytes written.
 */

static UINT32B
wr_meta(mfile, evtype, str)

int mfile;	/* midi file */
int evtype;	/* meta event type */
char *str;	/* text string */

{
	UINT32B bytes;
	unsigned char buff[4];


	bytes = write_delta(mfile);
	buff[0] = 0xff;
	buff[1] = (unsigned char) (evtype & 0xff);
	midiwrite(mfile, buff, 2);
	bytes += 2;
	bytes += midi_wrstring(mfile, str, NO);

	Status = 0;
	return(bytes);
}


/* walk through main list. For each top-visible staff, check the stuff
 * list for any "midi all" items */

static UINT32B
all_midi(mfile)

int mfile;

{
	struct MAINLL *mll_p;
	UINT32B bytes = 0;	/* number of bytes written */
	unsigned char buff[4];
	struct GRPSYL *gs_p;
	int curr_notemap = -1;
	int saw_a_staff;


	debug(256, "all_midi");

	initstructs();

	saw_a_staff = NO;
	if (Tuning_used == YES) {
		bytes += write_delta(mfile);
		bytes += out_notemap(mfile, ++curr_notemap);
	}

	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {
		switch (mll_p->str) {
		case S_STAFF:
			saw_a_staff = YES;
			if ( svpath(mll_p->u.staff_p->staffno, VISIBLE)->visible
							== YES) {

				/* Find the top visible voice */
				int vindex;
				for (vindex = 0; vindex < MAXVOICES; vindex++) {
					if (vvpath(mll_p->u.staff_p->staffno,
							vindex+1, VISIBLE)->
							visible == YES) {
						break;
					}
				}
				if (vindex >= MAXVOICES) {
					pfatal("top visible staff has no visible voice");
				}

				prepmidi_stuff(mll_p->u.staff_p, vindex, YES);
				/* We have to do groups one at a time in order
				 * to adjust for any squeezed-out spaces. */
				for (gs_p = mll_p->u.staff_p->groups_p[vindex];
						gs_p != (struct GRPSYL *) 0;
						gs_p = gs_p->next) {

					/* if we find a squeezed-out space
					 * group, adjust to account for that */
					if (gs_p->grpcont == GC_SPACE &&
							gs_p->grpvalue
							== GV_ZERO) {
						adj4squeeze(gs_p->fulltime);
						bytes += do_midi_stuff(Zero,
								mfile, YES);
					}
					else {
						if (gs_p->is_multirest == YES) {
							/* multirest */
							RATIONAL fulltime;
							fulltime.n =
							    gs_p->fulltime.n *
							    -(gs_p->basictime);
							fulltime.d =
							    gs_p->fulltime.d;
							rred( &fulltime );
							bytes += do_midi_stuff(
							fulltime,
							mfile, YES);
						}
						else {
							bytes += do_midi_stuff(
							gs_p->fulltime,
							mfile, YES);
						}
					}
				}

				/* do any remaining MIDI stuffs. This would
				 * be any that occur at exactly the time
				 * signature denominator plus one. */
				bytes += do_midi_stuff(Zero, mfile, YES);


				/* can skip any immediately following STAFFs,
				 * because we've already found the top
				 * visible one, which is the only one that
				 * should have any "midi all" stuff */
				while (mll_p->next != (struct MAINLL *) 0 &&
						mll_p->next->str == S_STAFF) {
					mll_p = mll_p->next;
				}
			}
			break;

		case S_SSV:
			asgnssv(mll_p->u.ssv_p);

			/* if key sig changes, handle that */
			if (mll_p->u.ssv_p->context == C_SCORE &&
					(mll_p->u.ssv_p->used[SHARPS] == YES ||
					mll_p->u.ssv_p->used[TRANSPOSITION] == YES ||
					mll_p->u.ssv_p->used[ADDTRANSPOSITION] == YES) ) {
				bytes += midi_keysig(mfile,
						eff_key(mll_p->u.ssv_p->staffno),
						mll_p->u.ssv_p->is_minor);
			}

			/* if time signature changes, handle that */
			if (mll_p->u.ssv_p->used[TIME] == YES) {
				bytes += midi_timesig(mfile);
			}

			/* If any tuning related thing changed since the
			 * initial SSVs that were before any STAFFs,
			 * output the new map for that */
			if (TUNEPARMSSV(mll_p->u.ssv_p)) {
				if (saw_a_staff == YES) {
					bytes += write_delta(mfile);
					bytes += out_notemap(mfile, ++curr_notemap);
				}
				saw_a_staff = NO;
			}
			break;

		case S_BAR:
			/* rehearsal mark --> midi cue point */
			if (mll_p->u.bar_p->reh_string != (char *) 0) {
				bytes += write_delta(mfile);
				buff[0] = 0xff;
				buff[1] = 0x07;
				midiwrite(mfile, buff, 2);
				bytes += 2;
				bytes += midi_wrstring(mfile,
					mll_p->u.bar_p->reh_string, YES);
			}
			break;

		default:
			break;
		}

		if (mll_p == (struct MAINLL *) 0) {
			/* Shouldn't happen, but just in case,
			 * don't core dump... */
			break;
		}
	}

	return(bytes);
}


/* go through main list and adjust for grace notes, alternation groups,
 * staccato, etc */

static void
midi_adjust()

{
	struct MAINLL *mll_p;	/* index through main list */
	int v;			/* voice index */
	int got_data = NO;	/* if got any music data yet */
	int did_all = NO;	/* if processed "all" stuff for this meas */
	UINT32B begin_usec;	/* usec per quarter at beginning of measure */


	debug(256, "midi_adjust");

	initstructs();
	begin_usec = Usec_per_quarter_note;
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
							mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {

			if (is_tab_staff(mll_p->u.staff_p->staffno) == YES) {
				continue;
			}

			/* If accidentals on one voice should apply to notes
			 * on the other voice, fix that */
			other_voice_accidentals(mll_p->u.staff_p);

			/* need to check stufflist on top visible to
			 * update Usec_per_quarter if necessary */
			if (did_all == NO && svpath(mll_p->u.staff_p->staffno,
					VISIBLE)->visible == YES) {
				begin_usec = Usec_per_quarter_note;
				Absolute_time = Zero;
				prepmidi_stuff(mll_p->u.staff_p, 0, YES);
				did_all = YES;
			}

			/* go through all groups, making adjustments */
			for (v = 0; v < MAXVOICES; v++) {
				Usec_per_quarter_note = begin_usec;
				Absolute_time = Zero;
				adjust_notes(mll_p->u.staff_p->groups_p[v],
					mll_p->u.staff_p->staffno, v, mll_p);
			}
			got_data = YES;
		}
		else if (mll_p->str == S_SSV) {
			asgnssv(mll_p->u.ssv_p);
			if (got_data == NO &&
					mll_p->u.ssv_p->context == C_SCORE) {
				if (mll_p->u.ssv_p->used[TIME] == YES) {
					Time_specified_by_user = YES;
				}
				if (mll_p->u.ssv_p->used[SHARPS] == YES) {
					Key_specified_by_user = YES;
				}
				if (mll_p->u.ssv_p->used[DIVISION] == YES) {
					Division = mll_p->u.ssv_p->division;
				}
				/* setting transposition implicitly sets a key */
				if (mll_p->u.ssv_p->used[TRANSPOSITION] == YES ||
				mll_p->u.ssv_p->used[ADDTRANSPOSITION] == YES) {
					Key_specified_by_user = YES;
				}
				
			}
		}
		else if (mll_p->str == S_BAR) {
			/* reset for next measure */
			did_all = NO;
			fix_tempo(YES);
			/* free up saved stuff info */
			free_midistuff(Midistufflist_p);
			Midistufflist_p = (struct MIDISTUFF *) 0;
		}
	}
}


/* adjust any grace notes to get a little time. We
 * don't know for sure how much time they should get, and whether they should
 * be on the beat or before or when, so make some guesses.
 * Take half of the fulltime of the preceding group and divide that time
 * among the number of grace notes.
 * Compare that value with 0.1 second, and use whichever is shorter.
 * Also shorten groups slightly to give a little release time between
 * notes (so that repeated notes don't run together so much), and
 * shorten groups that have staccato or wedge in their "with" list, and do
 * alternation groups and rolls */

static void
adjust_notes(gs_p, staffno, v, mll_p)

struct GRPSYL *gs_p;	/* adjust groups in this list */
int staffno;
int v;
struct MAINLL *mll_p;	/* groups are attached to main list here */

{
	int numgrace = 0;
	struct GRPSYL *gracelist_p;	/* one or more grace notes */
	RATIONAL time_adj;		/* adjustment for alt groups */
	int pairs;			/* how many pair of GRPSYLS to add
					 * for an alt pair */
	struct GRPSYL *add1_p, *add2_p;	/* groups added for alt pairs */
	int alt;			/* alternation number */
	int nn;				/* number of number due to slashes */
	int d;				/* number of dots */
	int dot, wedge, legato;		/* YES if these set in "with" list */
	int font, size;
	char *str;			/* string in with list */
	int ch;				/* music character in with list */
	int w;				/* index through with list */
	struct GRPSYL *prevgs_p;	/* group before grace note(s) */
	RATIONAL release;		/* how soon to release note */
	RATIONAL release_adjust;	/* how soon to release current note,
					 * the shorter of release and 1/4
					 * of the group's time value */
	RATIONAL fulltime;
	RATIONAL total_time;		/* time so far in measure */
	struct MAINLL *m_p;		/* for finding TIMEDSSVs, if any */
	struct TIMEDSSV *tssv_p;	/* for mid-measure release changes */
	static int had_tssv = NO;	/* If have any timed SSVs anywhere,
					 * we'll always call setssvstate() to
					 * make sure RELEASE is up to date.
					 * Just knowing if they were in this
					 * measure is not sufficient.
					 * Could be done on some granularity
					 * smaller than the whole song,
					 * but this is simple, and probably
					 * efficient enough.
					 */


	/* Some compilers warn that gracelist_p might be used uninitialized.
	 * Actually it won't be, but keep them quiet.
	 */
	gracelist_p = (struct GRPSYL *) 0;

	/* See if there are any timed SSVs to worry about */
	for (tssv_p = 0, m_p = mll_p; m_p != 0; m_p = m_p->next) {
		if (m_p->str == S_BAR) {
			tssv_p = m_p->u.bar_p->timedssv_p;
			if (tssv_p != 0) {
				had_tssv = YES;
			}
			break;
		}
	}
	if (had_tssv == YES) {
		setssvstate(mll_p);
	}

	total_time = Zero;
	for (   ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {

		/* apply any timed SSVs */
		if (gs_p->prev != 0) {
			total_time = radd(total_time, gs_p->prev->fulltime);
		}
		while (tssv_p != 0 && LE(tssv_p->time_off, total_time)) {
			asgnssv(&tssv_p->ssv);
			tssv_p = tssv_p->next;
		}

		if (gs_p->grpvalue == GV_ZERO)  {
			if (numgrace == 0) {
				/* save starting point of list of grace notes */
				gracelist_p = gs_p;
			}
			/* count how many grace notes */
			numgrace++;
		}

		else {
			/* if there were grace groups before this group,
			 * adjust to give them some time. */
			if (numgrace > 0) {
				/* find the previous group */
				prevgs_p = grp_before(gracelist_p, mll_p,
								staffno, v);

				steal4grace(mll_p, staffno, v, prevgs_p,
							gracelist_p, numgrace);

			}
			numgrace = 0;
			fulltime = gs_p->fulltime;

			/* check for alternation.
			 * For each alt, double the number
			 * of notes and make each half as long. */
			if (gs_p->slash_alt < 0) {

				alt = -(gs_p->slash_alt);

				/* for long notes, adjust so we get down to
				 * 8th notes for alternation */
				if (gs_p->basictime == BT_OCT) {
					alt += 4;
				}
				else if (gs_p->basictime == BT_QUAD) {
					alt += 3;
				}
				else if (gs_p->basictime == BT_DBL) {
					alt += 2;
				}
				else if (gs_p->basictime == 1) {
					alt += 1;
				}
				else if (gs_p->basictime >= 4) {
					alt -= 1;
				}

				/* adjust time values */
				time_adj.n = 1;
				time_adj.d = 1 << alt;
				gs_p->fulltime = rmul(gs_p->fulltime, time_adj);
				rred ( &(gs_p->fulltime) );
				gs_p->next->fulltime = gs_p->fulltime;

				/* turn off slash_alt so we won't do it again
				 * on the added pairs */
				gs_p->slash_alt = 0;
				gs_p->next->slash_alt = 0;

				/* add as many more pairs as necessary */
				/* If user specifies an insane
				 * alt number, we could try to make
				 * millions of groups. So limit to 1000.
				 */
				pairs = (1 << alt) - 1;
				if (pairs > 1000) {
					pairs = 1000;
				}
				for (  ; pairs > 0; pairs--) {
					/* create a new pair clone */
					add1_p = newGRPSYL(GS_GROUP);
					copy_attributes(add1_p, gs_p);
					add1_p->fulltime = gs_p->fulltime;
					copy_notes(add1_p, gs_p);
					add2_p = newGRPSYL(GS_GROUP);
					copy_attributes(add2_p, gs_p->next);
					add2_p->fulltime = gs_p->next->fulltime;
					copy_notes(add2_p, gs_p->next);

					/* link pair into list */
					add1_p->next = add2_p;
					add2_p->prev = add1_p;
					add2_p->next = gs_p->next->next;
					if (add2_p->next != (struct GRPSYL *) 0) {
						add2_p->next->prev = add2_p;
					}
					gs_p->next->next = add1_p;
					add1_p->prev = gs_p->next;
				}
			}
			else if (gs_p->slash_alt > 0
						&& gs_p->grpvalue != GV_ZERO) {
				/* do slashed notes */
				/* figure out how many actual chords are
				 * represented by the slashed chord */
				switch (gs_p->basictime) {
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
				/* multiply by two for each additional slash
				 * beyond the first.  Need to keep this IF
				 * here to avoid hitting potential optimizer
				 * bug. See comment in grpsyl.c */
				if (gs_p->slash_alt > 1) {
					nn = nn << (gs_p->slash_alt - 1);
				}

				if (nn == 0) {
					/* shifted left into oblivion */
					/* parser should have caught this */
					pfatal("bug in slash handling");
				}
				/* Add an additional 1 bit at the right for
				 * each dot */
				else {
					for (d = gs_p->dots; d > 0; d--) {
						 nn |= 1 << (drmo(nn) - 1);
					}
				}
				/* adjust time of notes, by dividing the
				 * fulltime of the group by the number of
				 * notes. We don't look at the basictime
				 * or dots again, so can leave them */
				gs_p->fulltime.d *= nn;
				rred ( &(gs_p->fulltime) );

				/* mark that we have done the slash */
				gs_p->slash_alt = 0;

				/* create as many clones of the groups as
				 * needed. Use > 1 because we
				 * already have the original group. */
				for (    ; nn > 1; nn--) {
					add1_p = newGRPSYL(GS_GROUP);
					copy_attributes(add1_p, gs_p);
					add1_p->fulltime = gs_p->fulltime;
					copy_notes(add1_p, gs_p);
					/* link into list */
					add1_p->next = gs_p->next;
					add1_p->prev = gs_p;
					gs_p->next = add1_p;
					if (add1_p->next != (struct GRPSYL *) 0) {
						add1_p->next->prev = add1_p;
					}
				}
			}

			/* now shorten any groups with dots or wedges */
			dot = wedge = legato = NO;
			for (w = 0; w < gs_p->nwith; w++) {
				if (is_music_symbol(gs_p->withlist[w].string) == NO) {
					continue;
				}
				font = gs_p->withlist[w].string[0];
				size =  gs_p->withlist[w].string[1];
				str = gs_p->withlist[w].string + 2;
				ch = next_str_char(&str, &font, &size);
				switch (ch) {
				case C_DOT:
					dot = YES;
					break;
				case C_WEDGE:
				case C_UWEDGE:
					wedge = YES;
					break;
				case C_LEG:
					legato = YES;
					break;
				}
			}
			if (wedge == YES) {
				/* reduce to 1/3 time, add rest for other 2/3 */
				add_rest(gs_p, rmul(gs_p->fulltime, Two_thirds));
				gs_p->fulltime = rmul(gs_p->fulltime, One_third);
			}
			else if (dot == YES) {
				if (legato == YES) {
					/* reduce by 1/4 */
					add_rest(gs_p, rmul(gs_p->fulltime,
								One_fourth));
					gs_p->fulltime = rmul(gs_p->fulltime,
								Three_fourths);
				}
				else {
					/* reduce by half */
					add_rest(gs_p, rmul(gs_p->fulltime,
								One_half));
					gs_p->fulltime = rmul(gs_p->fulltime,
								One_half);
				}
			}

			else if (gs_p->grpcont == GC_NOTES && legato == NO) {

				/* Figure out fulltime value for
				 * milliseconds of release time */
				release.n = (UINT32B)
					vvpath(staffno, v + 1, RELEASE)->release;
				release.d = 4L * Usec_per_quarter_note / 1000L;

				if (GT(release, Zero)) {
					/* shorten by a little bit.
					 * Otherwise repeated notes
					 * run together so much they can sound
					 * like a single note
					 * on some instruments. */

					/* round off to nearest 1024 note.
					 * Otherwise we can very quickly
					 * get to the point that the
					 * lowest common denominator of
					 * accumulated time values will get
					 * so big we overflow an UINT32B,
					 * which can cause lots of problems.
					 * Besides, this is going to get
					 * rounded off to the granularity
					 * of MIDI clock tick eventually anyway,
					 * and will be affected by MIDI latency,
					 * so if it is off by a few
					 * microseconds, it is very doubtful
					 * anyone will notice. */
					release.n = 1024 * release.n
								/ release.d;
					release.d = 1024;

					rred (&release);

					/* Shorten by the lesser of 1/4 of the
					 * note time or the amount user
					 * asked for */
					release_adjust = rmul(gs_p->fulltime,
								One_fourth);
					if (LT(release, release_adjust)) {
						release_adjust = release;
					}
					add_release(gs_p, release_adjust, mll_p);
				}
			}

			/* handle any rolls */
			fix_tempo(NO);
			midi_roll(gs_p, &(mll_p->u.staff_p->groups_p[v]));
			Absolute_time = radd(Absolute_time, fulltime);
		}
	}
}


/* We move grace notes to steal some time from the previous group.
 * Depending on where in history the music was written, that may or may not
 * be the right thing to do, but we have to do something, and this is often
 * the correct guess. This function is given the list of graces and the
 * previous group, and it gives time to them, taking away the same amount
 * of time from the group before. If these are at the very beginning,
 * add_pre_meas will have added a measure of space so we have something
 * to steal from.
 * It adjusts the length of the graces depending on how much time
 * seems safe to steal from the other group, how many graces there are,
 * and to keep them short enough as would seem reasonable to qualify
 * as grace notes.
 */

void
steal4grace(mll_p, staffno, vidx, prevgs_p, gracelist_p, numgrace)

struct MAINLL *mll_p;		/* gracelist is off of here */
int staffno;
int vidx;
struct GRPSYL *prevgs_p;	/* Steal time from here */
struct GRPSYL *gracelist_p;	/* Adjust graces starting here */
int numgrace;			/* How many graces there are before
				 * hitting a nongrace. We could figure that
				 * out here, but caller already knows. */

{
	RATIONAL graceadj;	/* how much to allocate to graces */
	RATIONAL gracetime;	/* duration of grace notes */
	RATIONAL tenthsec;	/* note value that is about 0.1 sec */

	/* Calculate what portion of the previous group to give to each grace */
	graceadj = calc_graceadj(prevgs_p, numgrace);

	gracetime = rmul(prevgs_p->fulltime, graceadj);

	/* If grace notes come out to more than about 1/10th second,
	 * use 1/10th second instead, because grace notes really should be
	 * pretty quick. First figure out what sort of
	 * fulltime value would be 0.1 second, by taking 1 over
	 * the number of microseconds in a whole note divided by 100000. */
	tenthsec.n = 1L;
	tenthsec.d = 4L * Usec_per_quarter_note / 100000L;

	if (tenthsec.d == 0) {
		/* We're in some outrageously fast tempo,
		 * over 600 quarter notes per minute, which is so fast
		 * that even a whole note isn't a tenth of a second.
		 * Make sure the denominator isn't zero, so we won't
		 * core dump. At this absurd tempo, we'd not going to
		 * be using this value anyway, except to compare
		 * against gracetime, so if it's off some, it won't matter. */
		tenthsec.d = 1L;
	}
	rred ( &tenthsec );
	if ( LT(tenthsec, gracetime)) {
		gracetime = tenthsec;
	}

	/* Round to nearest 2048th note to try to
	 * avoid arithmetic overflows */
	gracetime.n = 2048 * gracetime.n / gracetime.d;
	gracetime.d = 2048;
	rred ( &gracetime );

	/* Subtract time from previous group */
	graceadj.n = numgrace;
	graceadj.d = 1;
	prevgs_p->fulltime = rsub(prevgs_p->fulltime,
						rmul(gracetime, graceadj));

	/* Give each grace note its time */
	for (   ; numgrace > 0; numgrace--) {
		gracelist_p->fulltime = gracetime;
		gracelist_p = gracelist_p->next;
	}
}


/* Given the group before one or more grace notes, return what portion of
 * that group's time we are willing to steal for each grace.  This is the
 * maximum; the caller can choose to use less. */

RATIONAL
calc_graceadj(gs_p, numgrace)

struct GRPSYL *gs_p;	/* group before the grace(s) */
int numgrace;

{
	RATIONAL graceadj;

	/* If the group is notes, allow 1/2 of that group's
	 * time value and apportion among the grace notes.
	 * If rest or space, we can probably afford to use more.
	 * Exactly how much is unclear, so we'll use 3/4 */
	if (gs_p->grpcont == GC_NOTES) {
		graceadj.n = 1;
		graceadj.d = 2 * numgrace;
	}
	else {
		graceadj.n = 3;
		graceadj.d = 4 * numgrace;
	}
	rred( &graceadj );
	return(graceadj);
}


/* Usually we can just add in the release as is. But if the next group is
 * a grace group, it could end up being really short, because it steals
 * from the previous group, which would be this added release rest, which
 * is likely to be quite short. So peek ahead. If the next group is a grace,
 * only add in the release if it is at least 135 ms per following grace
 * group, which would allow them about 100 ms each. If it's shorter than
 * that, don't add any release, and just let the grace(s) steal from the
 * note group. */

static void
add_release(gs_p, release_adjust, mll_p)

struct GRPSYL *gs_p;
RATIONAL release_adjust;
struct MAINLL *mll_p;

{
	struct GRPSYL *nextgs_p;
	int numgrace;
	double rel_time;	/* release adjust in milliseconds */

	if ((nextgs_p = nextgrpsyl(gs_p, &mll_p)) != (struct GRPSYL *) 0) {
		/* count how many grace notes coming up after our gs_p */
		for (numgrace = 0; nextgs_p->grpvalue == GV_ZERO;
						nextgs_p = nextgs_p->next) {
			numgrace++;
		}

		if (numgrace > 0) {
			/* Calculate length of proposed release,
			 * by multiplying its time value by the number
			 * of milliseconds in a whole note. */
			rel_time = RAT2FLOAT(release_adjust) *
				Usec_per_quarter_note * 4L / 1000L;

			/* now see if it's too short */
			if ( rel_time < 135.0 * numgrace) {
				return;
			}
		}
	}

	/* add in a rest to accomplish the release */
	add_rest(gs_p, release_adjust);
	gs_p->fulltime = rsub(gs_p->fulltime, release_adjust);
}


/* turn damper pedal switch on or off. Return number of bytes written */

static UINT32B
pedswitch(mfile, on)

int mfile;
int on;		/* YES if to turn damper pedal on, NO if to turn off */

{
	UINT32B bytes;
	unsigned char buff[4];


	bytes = write_delta(mfile);
	Status = buff[0] = (unsigned char) (0xb0 | Channel);
	buff[1] = (unsigned char) 64;
	buff[2] = (on ? 127 : 0);
	bytes += write(mfile, buff, 3);
	return(bytes);
}


/* do rolls. Separate into several groups with notes tied together */

static void
midi_roll(gs_p, gslist_p_p)

struct GRPSYL *gs_p;
struct GRPSYL **gslist_p_p;	/* head of list of groups for this voice/meas */

{
	RATIONAL rolltime;		/* roll time adjust per note */
	struct GRPSYL *g_p;		/* walk through groups in roll */
	RATIONAL shortest;		/* shortest group in roll */
	int nnotes;			/* how many notes in roll */
	struct MIDIROLL *mrinfo;	/* information about a roll */


	switch (gs_p->roll) {
	case LONEITEM:
		if (gs_p->nnotes  < 2) {
			/* degenerate roll */
			return;
		}

		rolltime = roll_time(gs_p->fulltime, gs_p->nnotes);
		do_mroll(gs_p, gslist_p_p, rolltime, 0);
		break;

	case STARTITEM:
		/* count how many notes total to roll, and get duration of
		 * shortest group in the roll */
		nnotes = gs_p->nnotes;
		shortest = gs_p->fulltime;
		for (g_p = gs_p->gs_p; g_p != (struct GRPSYL *) 0;
							g_p = g_p->gs_p) {
			nnotes += g_p->nnotes;
			if (LT(g_p->fulltime, shortest)) {
				shortest = g_p->fulltime;
			}
			if (g_p->roll == ENDITEM) {
				break;
			}
		}

		rolltime = roll_time(shortest, nnotes);

		/* do first group */
		if (gs_p->rolldir != DOWN) {
			nnotes -= gs_p->nnotes;
			do_mroll(gs_p, gslist_p_p, rolltime, nnotes);
		}
		else {
			do_mroll(gs_p, gslist_p_p, rolltime, 0);
			nnotes = gs_p->nnotes;
		}

		/* now go down the chord again saving information about the
		 * roll on other groups */
		for (g_p = gs_p->gs_p; g_p != (struct GRPSYL *) 0;
							g_p = g_p->gs_p) {
			if (gs_p->rolldir != DOWN) {
				nnotes -= g_p->nnotes;
				savemidiroll(g_p, nnotes, rolltime);
			}
			else {
				savemidiroll(g_p, nnotes, rolltime);
				nnotes += g_p->nnotes;
			}
			if (g_p->roll == ENDITEM) {
				break;
			}
		}
		break;

	case INITEM:
	case ENDITEM:
		/* retrieve info about this roll and do it */
		if ((mrinfo = getmidiroll(gs_p)) == (struct MIDIROLL *) 0) {
			/* if staff is invisible, this is okay, otherwise
			 * something must have gone wrong */
			if (svpath(gs_p->staffno, VISIBLE)->visible == YES) {
				pfatal("info about roll is missing");
			}
		}
		else {
			do_mroll(gs_p, gslist_p_p, mrinfo->duration,
						mrinfo->notesbefore);
			FREE(mrinfo);
		}
		break;

	default:
		break;
	}
}


/* given a chord duration and number of notes, return how long to make
 * each note of roll. Use 1/20 second or whatever would add up to a total
 * of half the duration, whichever is shorter */

static RATIONAL
roll_time(grptime, nnotes)

RATIONAL grptime;	/* duration of rolled chord */
int nnotes;		/* how many notes in the chord */

{
	RATIONAL rolltime;		/* roll time adjust per note */
	RATIONAL maxdur;		/* note equal to 0.05 second */


	/* if not enough notes to roll, don't do anything here */
	if (nnotes < 2) {
		return(Zero);
	}

	/* as first guess, apportion the extra groups into half of
	 * the group time */
	rolltime = rmul(grptime, One_half);
	rolltime.d *= (nnotes - 1);
	rred ( &rolltime );

	/* find 0.05 second time */
	maxdur.n = 1;
	maxdur.d = 4L * Usec_per_quarter_note / 50000L;

	/* use whichever is shorter */
	return( LT(maxdur, rolltime) ? maxdur : rolltime);
}


/* create and link the extra groups to implement roll sound */

static void
do_mroll(gs_p, gslist_p_p, rolltime, notesbefore)

struct GRPSYL *gs_p;		/* group having roll */
struct GRPSYL **gslist_p_p;	/* addr of groups_p list containing gs_p */
RATIONAL rolltime;		/* duration per roll note */
int notesbefore;		/* how many notes of roll before this in
				 * chords in other voices */

{
	register int i;
	struct GRPSYL **link_p_p;	/* where to link added groups */
	struct GRPSYL *prev_p;		/* previous group */
	RATIONAL factor;		/* multiplier of duration */
	struct GRPSYL *newgs_p;		/* added rest group */


	/* figure out where to link added groups */
	if (gs_p->prev == (struct GRPSYL *) 0) {
		link_p_p = gslist_p_p;
	}
	else {
		link_p_p = &( gs_p->prev->next);
	}
	prev_p = gs_p->prev;

	/* add in groups with appropriate subset of notes, tied to
	 * the existing group */
	if (gs_p->rolldir != DOWN) {
		for (i = 1; i < gs_p->nnotes; i++) {
			addrollgrp(gs_p, rolltime, i, gs_p->nnotes - 1,
						link_p_p, prev_p);
		}
	}
	else {
		for (i = gs_p->nnotes - 2; i >= 0; i--) {
			addrollgrp(gs_p, rolltime, 0, i,
						link_p_p, prev_p);
		}
	}

	/* adjust group time */
	factor.n = gs_p->nnotes - 1 + notesbefore;
	factor.d = 1;
	gs_p->fulltime = rsub(gs_p->fulltime, rmul(rolltime, factor));

	/* add rest before if necessary */
	if (notesbefore > 0) {
		factor.n = notesbefore;
		CALLOC(GRPSYL, newgs_p, 1);
		newgs_p->grpcont = GC_REST;
		newgs_p->fulltime = rmul(rolltime, factor);
		/* mark as internally generated, so octave adjust works */
		newgs_p->inputlineno = -1;

		/* stitch into list */
		(*link_p_p)->prev = newgs_p;
		newgs_p->next = *link_p_p;
		newgs_p->prev = prev_p;
		*link_p_p = newgs_p;
	}
}


/* add group to form part of a roll */

static void
addrollgrp(gs_p, duration, start, end, link_p_p, prev_p)

struct GRPSYL *gs_p;
RATIONAL duration;
int start;		/* index into notelist, where to start copying notes */
int end;		/* index into notelist, where to stop copying notes */
struct GRPSYL **link_p_p; /* where to link into list */
struct GRPSYL *prev_p;	/* previous group */

{
	struct GRPSYL *newgs_p;
	int i;


	newgs_p = newGRPSYL(GS_GROUP);
	newgs_p->grpcont = GC_NOTES;
	newgs_p->fulltime = duration;
	newgs_p->nnotes = end - start + 1;
	/* mark as internally generated, so octave adjusting will work */
	newgs_p->inputlineno = -1;

	/* copy appropriate subset of notes from original group */
	CALLOC(NOTE, newgs_p->notelist, newgs_p->nnotes);
	for (i = 0; start <= end; i++, start++) {
		newgs_p->notelist[i].letter = gs_p->notelist[start].letter;
		COPY_ACCS(newgs_p->notelist[i].acclist,
					gs_p->notelist[start].acclist);
		newgs_p->notelist[i].octave = gs_p->notelist[start].octave;
		newgs_p->tie = YES;
	}
	
	/* stitch into list */
	(*link_p_p)->prev = newgs_p;
	newgs_p->next = *link_p_p;
	newgs_p->prev = prev_p;
	*link_p_p = newgs_p;
}


/* Create struct to hold info about roll that crosses groups and fill it in.
 * Link onto list of info of this type */

static void
savemidiroll(gs_p, notesbefore, duration)

struct GRPSYL *gs_p;
int notesbefore;
RATIONAL duration;

{
	struct MIDIROLL *new_p;

	CALLOC(MIDIROLL, new_p, 1);
	new_p->gs_p = gs_p;
	new_p->notesbefore = (short) notesbefore;
	new_p->duration = duration;
	new_p->link_p = Midirollinfo_p;
	Midirollinfo_p = new_p;
}


/* given a GRPSYL, return pointer to the MIDIROLL struct associated with it,
 * after detaching it from the list.  Caller is responsible for freeing it.
 * Returns null if not on the list */

static struct MIDIROLL *
getmidiroll(gs_p)

struct GRPSYL *gs_p;

{
	struct MIDIROLL **mr_p_p;
	struct MIDIROLL *the_one;	/* the one matching gs_p */


	/* walk down list. Since there aren't likely to be all that many
	 * multi-voice rolls per measure, we just use a linked list instead
	 * of hashing or something. */
	for (mr_p_p = &Midirollinfo_p; *mr_p_p != (struct MIDIROLL *) 0;
			mr_p_p = &( (*mr_p_p)->link_p) ){

		if ( (*mr_p_p)->gs_p == gs_p) {
			/* found it. detach and return it */
			the_one = *mr_p_p;
			*mr_p_p = (*mr_p_p)->link_p;
			return(the_one);
		}
	}
	return (struct MIDIROLL *) 0;
}


/* go through list of STUFFs for this measure. If there is a MIDI "tempo"
 * STUFF prior to the current time, update Usec_per_quarter_note */

static void
fix_tempo(to_end)

int to_end;	/* if YES, go all the way to end of Midistufflist_p */

{
	struct MIDISTUFF *ms_p;		/* index through list of STUFF */
	char *key;			/* to check for "tempo" */
	int leng;			/* length of key */
	char *arg;			/* tempo argument */
	int quarter_notes_per_min;	/* notes per minute */


	/* check stuff in this measure */
	for (ms_p = Midistufflist_p; ms_p != (struct MIDISTUFF *) 0;
					ms_p = ms_p->next) {
		if (GE(ms_p->time, Absolute_time) && to_end == NO) {
			/* beyond where we are in time so far */
			return;
		}

		/* see if MIDI tempo */
		if (ms_p->stuff_p->stuff_type == ST_MIDI) {
			if (getkeyword(ms_p->stuff_p->string + 2, &key, &leng,
							&arg) == YES) {
				if (matches(key, leng, "tempo") == YES) {
					/* is it tempo. Update */
					quarter_notes_per_min = atoi(arg);
					if (quarter_notes_per_min >= MINQNPM
						&& quarter_notes_per_min
						<= MAXQNPM)  {
					    Usec_per_quarter_note =
						USEC_PER_MINUTE
						/  quarter_notes_per_min;
					}
				}
			}
		}
	}
}


/* recursively free MIDISTUFF list */

static void
free_midistuff(ms_p)

struct MIDISTUFF *ms_p;

{
	if (ms_p == (struct MIDISTUFF *) 0) {
		return;
	}

	free_midistuff(ms_p->next);
	FREE(ms_p);
}


/* when a group is squeezed to zero time because the chord was all spaces,
 * we need to adjust the time to do any pending stuffs by the amount
 * of time squeezed out. So go through the list of pending stuffs, and
 * mark them as occurring that much earlier, or immediately if the time
 * would end up negative. Octave marks are handled a measure at a time,
 * so we don't have to worry about them.
 * If user put a stuff in the middle of an all-space chord,
 * maybe they really wanted the space not squeezed, but tough.
 * If they really want time taken up they should use rest, not space.
 * It isn't worth the effort to figure out that some particular space
 * chord has a stuff in the middle of it, so that it should be treated
 * specially.
 */

static void
adj4squeeze(timeval)

RATIONAL timeval;	/* adjust by this much */

{
	struct MIDISTUFF *ms_p;	/* walk through list of MIDI stuff to do */


	for (ms_p = Midistufflist_p; ms_p != (struct MIDISTUFF *) 0;
					ms_p = ms_p->next) {

		/* adjust the time */
		ms_p->time = rsub(ms_p->time, timeval);

		if (LT(ms_p->time, Zero)) {
			/* Oops. User put a stuff in the middle of an
			 * all-space group. Schedule the stuff to happen
			 * immediately */
			ms_p->time = Zero;
		}
	}
}


/* return YES if specified staff/voice is used somewhere in the piece */

int
voice_used(staffno, vno)

int staffno;
int vno;

{
	return (Voice2track_map [staffno] [vno] != 0 ? YES : NO);
}


/* If user specified multiple accidentals, combine them into a single
 * effective accidental. This should only be called if the effective
 * accidental is within range of double flat to double sharp. Otherwise,
 * Tuning_used should have been set, and we have to use the more complicated
 * way, using note tuning. */

static void
comb_all_accs()

{
	struct MAINLL *mll_p;	/* walk through main list */
	int vindex;		/* index through voices */
	struct GRPSYL *gs_p;	/* walk through groups for a voice */
	int n;			/* index through notes in a group */


	initstructs();
	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		if (mll_p->str != S_STAFF) {
			continue;
		}
		for (vindex = 0; vindex < MAXVOICES; vindex++) {
			for (gs_p = mll_p->u.staff_p->groups_p[vindex];
						gs_p != 0; gs_p = gs_p->next) {
				for (n = 0; n < gs_p->nnotes; n++) {
					comb_accs( &(gs_p->notelist[n]),
					gs_p->inputfile, gs_p->inputlineno);
				}
			}
		}
	}
}


/* Given a note, if it had multiple accidentals, replace with a single
 * accidental that is the net of the multiple ones. The net is expected
 * to be in range of double flat to double sharp. */

static void
comb_accs(note_p, fname, linenum)

struct NOTE *note_p;	/* process this note */
char *fname;		/* for error message */
int linenum;		/* for error message */

{
	int a;


	/* No combining needed if no accidentals or only one accidental */
	if (note_p->acclist[0] == 0 || note_p->acclist[2] == 0) {
		return;
	}

	switch (accs_offset(note_p->acclist)) {
	case -2:
		note_p->acclist[1] = C_DBLFLAT;
		break;
	case -1:
		note_p->acclist[1] = C_FLAT;
		break;
	case 0:
		note_p->acclist[1] = C_NAT;
		break;
	case 1:
		note_p->acclist[1] = C_SHARP;
		break;
	case 2:
		note_p->acclist[1] = C_DBLSHARP;
		break;
	default:
		/* We should have turned on Tuning_used */
		l_pfatal(fname, linenum,
			"comb_accs got an invalid effective offset (%d)",
			accs_offset(note_p->acclist));
		break;
	}

	/* Clean out the rest of the acc list */
	for (a = 2; a < MAX_ACCS * 2; a++) {
		note_p->acclist[a] = 0;
	}
}


/* Given a STUFF, create a MIDISTUFF for it, and link it into the proper place
 * in the list pointed to by Midistufflist_p, based on time offset.
 */

void
insert_midistufflist(stuff_p)

struct STUFF *stuff_p;	/* what to add to the list */

{
	struct MIDISTUFF *ms_p;		/* the allocated struct */
	struct MIDISTUFF **ms_p_p;	/* for inserting into list */


	CALLOC(MIDISTUFF, ms_p, 1);

	/* Figure out when to do this event. We have the time in floating
	 * point, but need it in RATIONAL, so have to convert. From MIDI's
	 * point of view, the first beat of a measure occurs at
	 * time zero, but stuff calls that time 1, and things may
	 * happen before that. So adjust for that,
	 * and consider anything happening from stuff time 0 to
	 * 1 to happen instantaneously at midi time 0. */
	ms_p->time.n = (UINT32B) ( (stuff_p->start.count - 1.0) * F2RFACTOR);
	ms_p->time.d = F2RFACTOR * Score.timeden;
	rred( &(ms_p->time) );
	if ( LT(ms_p->time, Zero) ) {
		ms_p->time = Zero;
	}
	/* Round to 4096th note to reduce chance of
	 * getting rational overflow. */
	ms_p->time.n = (INT32B) (4096.0 * ms_p->time.n / ms_p->time.d);
	ms_p->time.d = 4096;

	ms_p->time = radd(ms_p->time, Absolute_time);

	ms_p->stuff_p = stuff_p;

	/* Link onto list. */
	for (ms_p_p = &Midistufflist_p; *ms_p_p != 0;
						ms_p_p = &((*ms_p_p)->next)) {
		if (GT( (*ms_p_p)->time, ms_p->time)) {
			break;
		}
	}
	ms_p->next = *ms_p_p;
	*ms_p_p = ms_p;
}
