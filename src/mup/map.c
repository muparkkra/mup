
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

/* This file contains functions for mapping input to individual voices.
 * The user can give a single line of input
 * that gets expanded into several voices.
 * There are two flavors of this:
 * voice-at-a-time input and chord-at-a-time input.
 * For the former, the GRPSYL list just gets cloned and altered as needed.
 * For the latter, brand new GRPSYL lists are created
 * by distributing individual notes from the user's input.
 * For any given staff/voice, only one type of input can be used per measure.
 * For chord-at-a-time, a given staff/voice can appear more than once
 * within a single input line, but not on multiple input lines per measure.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/* mark whether we mapped user data, only mapped implicit spaces, or nothing */
#define MAPPED_NOTHING	(0)
#define MAPPED_IMPLICIT	(1)
#define MAPPED_EXPLICIT	(2)

/* This struct tells how to map one note for chord-at-a-time input,
 * i.e., which staff/voice combinations it should be mapped to.
 * Each map item in input (semicolon-separated)
 * gets saved in one of these. */
struct NOTEMAP {
	struct SVRANGELIST *svlist_p;	/* staffs/voices to map this note to */
	struct NOTEMAP *next;		/* linked list */
};

/* This struct gives the mapping for a particular number of notes.
 * The set of things inside a pair of brackets gets saved in one of these. */
struct MAP {
	int num_entries;		/* how many item in list */
	struct NOTEMAP *notemap_p;	/* one entry for each note in chord */
	struct MAP *next;		/* linked list */
};

/* This points to the list of maps, or to 0 if doing voice-at-a-time input */
static struct MAP *Map_p;
/* This points to where to add to the map list */
static struct MAP **End_map_p_p = &Map_p;

/* This is where to insert the next NOTEMAP */
static struct NOTEMAP **Note_p_p;

/* It is handy to be able to treat both input styles identically
 * as much as possible. So for when input is voice-at-a-time, it can be handy
 * to have a MAP that just points to Svrangelist_p. These two structs are
 * used for that purpose. Since the value of Svrangelist_p changes
 * at runtime, we have to set Sv_notemap->svlist_p each time
 * before these are used. */
static struct NOTEMAP Sv_notemap = {
	(struct SVRANGELIST *) 0, (struct NOTEMAP *) 0
};
static struct MAP Voice_at_a_time_map = {
	1, &Sv_notemap, (struct MAP *) 0
};

/* Keep track of which input style was used */
static short Input_style[MAXSTAFFS][MAXVOICES];

static int map_groups P((void));
static void clean_map_data P((void));
static void free_maps P((struct MAP *map_p));
static void free_notemaps P((struct NOTEMAP *notemap_p));
static struct NOTEMAP *find_notemap P((int num_notes));
static void map1note P((struct GRPSYL *from_gs_p, int n, int staff, int voice,
	short allocated[MAXSTAFFS][MAXVOICES]));
static void do_link_groups P((void));
static void link_a_grouplist P((int staffno, int vno, int copies));
static void convert_rest_space P((struct GRPSYL *grpsyl_p, int staffno, int vno));
static void conv_grpsyl P((struct GRPSYL *grpsyl_p, int staffno, int vno));
static void mult_def P((int staff, int voice));


/* Initialize Input_style to default of IS_VOICE_INPUT */

void
reset_input_style()

{
	int staff;
	int voice;

	for (staff = 0; staff < MAXSTAFFS; staff++) {
		for (voice = 0; voice < MAXVOICES; voice++) {
			Input_style[staff][voice] = IS_VOICE_INPUT;
		}
	}
}


/* Return the current input style for a given staff/voice */

int
input_style(staff, voice)

int staff;
int voice;

{
	return(Input_style[staff-1][voice-1]);
}


/* This is called when a '[' is encountered in input, starting a new map */

void
begin_map()

{
	struct MAP *new_p;


	/* allocate space for a new map */
	CALLOC(MAP, new_p, 1);

	/* Keep track of where to link on the first NOTEMAP */
	Note_p_p = &(new_p->notemap_p);

	/* Add to MAP list */
	*End_map_p_p = new_p;

	begin_sv_list();
}


/* Save one item of a map. Items are the semicolon-separated specifications. */

void
map_item()
{
	CALLOC(NOTEMAP, *Note_p_p, 1);

	/* Save the current range */
	(*Note_p_p)->svlist_p = Svrangelist_p;
	((*End_map_p_p)->num_entries)++;

	/* prepare for another, if any */
	Note_p_p = &( (*Note_p_p)->next);

	begin_sv_list();
	begin_range(PL_UNKNOWN);
}


/* At the end of a map specification, this function is called to save the
 * info about the map for later use, and prepare for another map, if any.
 */

void
end_map()

{
	/* prepare for another map, if any */
	End_map_p_p = &( (*End_map_p_p)->next);

	begin_range(Place);
}


/* Map chord-at-a-time input so it looks just like voice-at-a-time input.
 * Return YES if current measure is chord-at-a-time, and thus
 * mapping was done, NO if current measure is voice-at-a-time.
 */

static int
map_groups()

{
	struct MAP *map_p;
	/* Multiple notes in a chord might get mapped to a single staff/voice.
	 * In that case, after the first note for that staff/voice,
	 * we need to just add a note to the existing GRPSYL rather than
	 * allocating a new one. This keeps track of whether we have already
	 * allocated a GRPSYL for the current chord of a given staff/voice. */
	short allocated[MAXSTAFFS][MAXVOICES];
	/* This array tells us which staffs/voices we are mapping things to */
	short used[MAXSTAFFS][MAXVOICES];
	/* This array will have MAPPED_EXPLICIT
	 * in entries where we mapped actual user data.
	 * If we only mapped implicit spaces (MAPPED_IMPLICIT),
	 * we can treat things as if user didn't use the voice
	 * on this input line. */
	short mapped_something[MAXSTAFFS][MAXVOICES];
	/* This says if we've printed an error yet for multiply defined voice,
	 * to make sure we only print it once. */
	short printed_mult_err[MAXSTAFFS][MAXVOICES];
	/* This tells which numbers of notes we have maps for. */
	short have_map[MAXHAND];
	int s;			/* staff number */
	int v;			/* voice number */
	int n;			/* note index */
	struct NOTEMAP *notemap_p;	/* how to map notes to voices */
	struct SVRANGELIST *svr_p;
	struct RANGELIST *sr_p;	/* range of staffs being defined */
	struct RANGELIST *vr_p;	/* range of vno's being defined */
	struct GRPSYL *gs_p;
	struct GRPSYL *g_p;
	int errors;


	if (Map_p == (struct MAP *) 0) {
		/* not chord-at-a-time mapping, so nothing to do here */
		return(NO);
	}

	/* remember current error count */
	errors = Errorcount;

	/* Initialize arrays. These will later tell us
	 * which GRPSYL lists we are mapping to, and whether we mapped
	 * any actual user input, or just implicit spaces. */
	for (s = 0; s < Score.staffs; s++) {
		for (v = 0; v < MAXVOICES; v++) {
			used[s][v] = NO;
			mapped_something[s][v] = MAPPED_NOTHING;
			printed_mult_err[s][v] = NO;
		}
	}
	/* This tells for which numbers of notes we have maps */
	for (n = 0; n < MAXHAND; n++) {
		have_map[n] = NO;
	}

	/* Do some error checking on the MAP list */
	for (map_p = Map_p; map_p != (struct MAP *) 0; map_p = map_p->next) {

		if (have_map[map_p->num_entries] == YES) {
			l_yyerror(Curr_filename, yylineno,
				"more than one map for chords with %d notes",
				map_p->num_entries);
			continue;
		}
		else {
			have_map[map_p->num_entries] = YES;
		}

		for (notemap_p = map_p->notemap_p;
				notemap_p != (struct NOTEMAP *) 0;
				notemap_p = notemap_p->next) {
			for (svr_p = notemap_p->svlist_p;
					svr_p != (struct SVRANGELIST *) 0;
					svr_p = svr_p->next) {
			    for (sr_p = svr_p->stafflist_p; sr_p != 0;
							sr_p = sr_p->next) {
				for (s = sr_p->begin; s <= sr_p->end; s++) {

					if (s > Score.staffs) {
						l_yyerror(Curr_filename,
							yylineno,
							"staff %d does not exist",
							s);
						continue;
					}

					for (vr_p = svr_p->vnolist_p; vr_p != 0;
							vr_p = vr_p->next) {
					    for (v = vr_p->begin;
							v <= vr_p->end; v++) {

						/* make sure voice exists */
						if (v > 1 && svpath(s, VSCHEME)
							->vscheme == V_1) {
						   l_yyerror(Curr_filename,
							yylineno,
							"there is no voice %d on staff %d",
							v, s);
						}
						used[s-1][v-1] = YES;
						Input_style[s-1][v-1]
							= IS_CHORD_INPUT;
					    }
					}
				}
			    }
			}
		}
	}

	if (Errorcount > errors) {
		clean_map_data();
		return(YES);
	}

	/* process each chord in the GRPSYL list */
	for (gs_p = Curr_gs_list_p; gs_p != (struct GRPSYL *) 0;
							gs_p = gs_p->next) {
		/* initialize the allocation array for current chord */
		for (s = 0; s < Score.staffs; s++) {
			for (v = 0; v < MAXVOICES; v++) {
				allocated[s][v] = NO;
			}
		}

		/* With voice-at-a-time input, we allow the first group
		 * to have no pitch specified iff it is on a 1-line staff.
		 * For chord-at-a-time, to allow that
		 * we would have to allow a mapping of zero notes,
		 * which doesn't make sense, or map an implicit note,
		 * which seems questionable at best.
		 * If there is a mixture of 1-line and not-1-line
		 * staffs being mapped, things get even more confusing.
		 * So we disallow implicit pitch on chord-at-at-time. */
		if (gs_p->nnotes == 1 && gs_p->notelist[0].letter == PP_NO_PITCH) {
			l_yyerror(Curr_filename, yylineno, "no notes specified");
			notemap_p = (struct NOTEMAP *) 0;
		}
		else {
			/* Find the pattern matching the number of notes.
			 * If none is found, this will return 0, and the 'for'
			 * below will get skipped, and we'll add spaces */
			notemap_p = find_notemap(gs_p->nnotes);
		}

		/* Go through each note in the chord, and copy it
		 * to the appropriate staffs/voices */
		for (n = 0; notemap_p != (struct NOTEMAP *) 0;
					n++, notemap_p = notemap_p->next) {
			for (svr_p = notemap_p->svlist_p;
					svr_p != (struct SVRANGELIST *) 0;
					svr_p = svr_p->next) {
			    for (sr_p = svr_p->stafflist_p; sr_p != 0;
							sr_p = sr_p->next) {
				for (s = sr_p->begin; s <= sr_p->end; s++) {
				    for (vr_p = svr_p->vnolist_p; vr_p != 0;
							vr_p = vr_p->next) {
					for (v = vr_p->begin;
							v <= vr_p->end; v++) {
						/* If we have not yet mapped
						 * anything from the current
						 * input line, yet there is
						 * something in the grpsyl
						 * list for this staff/voice,
						 * that means user must have
						 * already defined data for
						 * this staff/voice on some
						 * other input line, and thus
						 * is not allowed to map
						 * anything from the current
						 * line. */
						if (mapped_something[s-1][v-1]
							== MAPPED_NOTHING
							&& Staffmap_p[s]->u.staff_p->groups_p[v-1] != 0
							&& printed_mult_err[s-1][v-1] == NO) {
						    mult_def(s, v);
						    printed_mult_err[s-1][v-1] = YES;
						    continue;
						}
						map1note(gs_p, n, s, v,
								allocated);
						mapped_something[s-1][v-1]
							= MAPPED_EXPLICIT;
					}
				    }
				}
			    }
			}
		}

		/* For any staff/voice that is being mapped to, but which
		 * didn't get anything mapped for this particular chord,
		 * add a space group. This could happen either because
		 * user specified several patterns and some patterns don't
		 * contain all the staffs/voices, which implies they want
		 * us to fill in spaces, or because there was an error in
		 * input. If there was an error, it's still nice to add the
		 * space, because it prevents extra error messages */
		for (s = 1; s <= Score.staffs; s++) {
			for (v = 1; v <= MAXVOICES; v++) {
				/* If we haven't mapped anything to this
				 * voice, but there is something there,
				 * user must have defined it on an earlier
				 * input line. In that case we should leave
				 * it be, because either (1) user didn't
				 * actually use any pattern that uses this
				 * voice, or (2) they multiply defined the
				 * voice, in which case the error is caught
				 * elsewhere. In either case, their
				 * earlier input should stand. */
				if (mapped_something[s-1][v-1] == MAPPED_NOTHING
						&& Staffmap_p[s]->u.staff_p->groups_p[v-1] != 0) {
					continue;
				}

				if (used[s-1][v-1] == YES &&
						allocated[s-1][v-1] == NO) {
					map1note(gs_p, -1, s, v, allocated);
					if (mapped_something[s-1][v-1] !=
							MAPPED_EXPLICIT) {
						mapped_something[s-1][v-1]
							= MAPPED_IMPLICIT;
					}
				}
			}
		}
	}

	/* If this particular input line didn't actually use some of the
	 * patterns, some voices might not *really* have been used--we
	 * merely filled in implicit spaces for it. So we can undo that
	 * so user can specify the voice via voice-at-a-time if they want to.
	 * If they don't, the regular filling in of missing voices with
	 * implicit spaces will happen later. */
	for (s = 0; s < Score.staffs; s++) {
		for (v = 0; v < MAXVOICES; v++) {
			if (used[s][v] == YES &&
					mapped_something[s][v] != MAPPED_EXPLICIT) {
				used[s][v] = NO;
				Input_style[s][v] = IS_VOICE_INPUT;

				/* If only implict, we free that up */
				if (mapped_something[s][v] == MAPPED_IMPLICIT) {
					free_grpsyls(Staffmap_p[s+1]->u.staff_p->groups_p[v]);
					Staffmap_p[s+1]->u.staff_p->groups_p[v] = 0;
				}
			}
		}
	}

	/* Now we can go through and free up any wasted space */
	for (s = 0; s < Score.staffs; s++) {
		for (v = 0; v < MAXVOICES; v++) {
			if (used[s][v] == YES) {
				/* Rests and spaces get moved from
				 * NOTE pseudo-pitches to GRPSYL */
				convert_rest_space(Staffmap_p[s+1]->u.
						staff_p->groups_p[v], s+1, v+1);
				for (g_p = Staffmap_p[s+1]->u.staff_p->groups_p[v];
						g_p != (struct GRPSYL *) 0;
						g_p = g_p->next) {
					resize_notelist(g_p);
				}
			}
		}
	}

	clean_map_data();

	/* Everything in the original GRPSYL list
	 * has been copied to other lists, so original can be freed */
	free_grpsyls(gs_p);

	return(YES);
}


/* map one note to one staff/voice */

static void
map1note(from_gs_p, n, staff, voice, allocated)

struct GRPSYL *from_gs_p;	/* copy from here */
int n;				/* copy the nth note in from_gs_p, or if -1,
				 * create a space group */
int staff;
int voice;
short allocated[MAXSTAFFS][MAXVOICES];	/* tracks whether to allocate a new
				 * GRPSYL; may be updated */

{
	struct GRPSYL *to_gs_p;		/* where to map note to */
	struct GRPSYL **add_p_p;	/* where to add to_gs_p */
	struct NOTE *from_note_p;
	struct NOTE *to_note_p;
	struct GRPSYL *prev;		/* value to set to_gs_p->prev to */
	int p;				/* index for phplace */


	/* If original group is a grace group, we don't need to add a
	 * space group, since grace already take no time */
	if (n == -1 && from_gs_p->grpvalue == GV_ZERO) {
		return;
	}

	/* If this is the first note allocated to this staff/voice for
	 * current chord, have to allocate a GRPSYL for it. */
	if (allocated [staff - 1] [voice - 1] == NO) {
		to_gs_p = newGRPSYL(GS_GROUP);
		copy_attributes(to_gs_p, from_gs_p);
		allocated [staff - 1] [voice - 1] = YES;

		/* Add to end of list */
		prev = (struct GRPSYL *) 0;
		for (add_p_p = &(Staffmap_p[staff]->u.staff_p->groups_p[voice-1]);
					*add_p_p != (struct GRPSYL *) 0;
					add_p_p = &((*add_p_p)->next) ) {
			prev = *add_p_p;
		}
		to_gs_p->prev = prev;
		*add_p_p = to_gs_p;

		/* copy the other attributes */
		to_gs_p->staffno = staff;
		to_gs_p->vno = voice;
		to_gs_p->basictime = from_gs_p->basictime;
		to_gs_p->fulltime = from_gs_p->fulltime;
		to_gs_p->dots = from_gs_p->dots;
		to_gs_p->is_meas = from_gs_p->is_meas;
		to_gs_p->tuploc = from_gs_p->tuploc;
		to_gs_p->tupcont = from_gs_p->tupcont;
		to_gs_p->tupside = from_gs_p->tupside;
		to_gs_p->beamloc = from_gs_p->beamloc;
		to_gs_p->breakbeam = from_gs_p->breakbeam;
		to_gs_p->beamto = from_gs_p->beamto;
		to_gs_p->printtup = from_gs_p->printtup;
		to_gs_p->tie = from_gs_p->tie;
		to_gs_p->phcount = from_gs_p->phcount;
		for (p = 0; p < from_gs_p->phcount; p++) {
			to_gs_p->phplace[p] = from_gs_p->phplace[p];
		}
		to_gs_p->ephcount = from_gs_p->ephcount;
		to_gs_p->ho_usage = from_gs_p->ho_usage;
		to_gs_p->ho_value = from_gs_p->ho_value;
	}
	else {
		/* find the last group for this staff/voice */
		for (to_gs_p = Staffmap_p[staff]->u.staff_p->groups_p[voice-1];
					to_gs_p->next != (struct GRPSYL *) 0;
					to_gs_p = to_gs_p->next) {
			;
		}
	}

	/* Special case: If n == -1, make this a space group */
	if (n == -1) {
		to_gs_p->grpcont = GC_SPACE;
		/* some things don't make sense with space,
		 * so nullify the things that just apply to notes */
		to_gs_p->grpvalue = GV_NORMAL;
		to_gs_p->headshape = HS_UNKNOWN;
		to_gs_p->grpsize = GS_NORMAL;
		to_gs_p->stemdir = UNKNOWN;
		to_gs_p->stemlen = STEMLEN_UNKNOWN;
		to_gs_p->roll = NOITEM;
		to_gs_p->beamloc = NOITEM;
		to_gs_p->breakbeam = NO;
		to_gs_p->beamto = CS_SAME;
		to_gs_p->stemto = CS_SAME;
		to_gs_p->slash_alt = 0;
		return;
	}

	from_note_p = &(from_gs_p->notelist[n]);
	if (Doing_tab_staff == YES) {
		/* fret, nticks, and bendstring exist in from_note_p
		 * in an internal format, whereas add_note() needs them
		 * in something close to user input format,
		 * so we have to reconstruct
		 * what the user input must have been. */
		add_note(to_gs_p, from_gs_p->notelist[n].letter,
			from_note_p->acclist,
			TMP_FRET(from_note_p),
			TMP_NTICKS(from_note_p),
			from_note_p->acc_has_paren,
			(HASBEND(from_gs_p->notelist[n])
			  ? bend_string(from_note_p)
			  : (char *) 0) );
	}
	else {
		add_note(to_gs_p, from_gs_p->notelist[n].letter,
			from_note_p->acclist,
			from_note_p->octave,
			0,
			from_note_p->acc_has_paren,
			(char *) 0);
	}

	/* copy remaining note attributes: tie, slur, etc */
	to_note_p = &(to_gs_p->notelist[to_gs_p->nnotes - 1]);
	to_note_p->tie = from_note_p->tie;
	to_note_p->tiestyle = from_note_p->tiestyle;
	to_note_p->tiedir = from_note_p->tiedir;
	to_note_p->tied_to_voice = from_note_p->tied_to_voice;
	to_note_p->nslurto = from_note_p->nslurto;
	/* Actually inhibitprint processing has not happened yet,
	 * but we copy anyway to be consistent. */
	to_note_p->inhibitprint = from_note_p->inhibitprint;
	if (from_note_p->nslurto > 0) {
		/* slurto lists cannot be safely shared, so make copy */
		MALLOC(SLURTO, to_note_p->slurtolist, 
					from_note_p->nslurto);
		(void) memcpy(to_note_p->slurtolist,
					from_note_p->slurtolist,
					sizeof(struct SLURTO) *
					from_note_p->nslurto);
	}
	else {
		to_note_p->slurtolist = (struct SLURTO *) 0;
	}
	to_note_p->notesize = from_note_p->notesize;
	to_note_p->note_has_paren = from_note_p->note_has_paren;
	to_note_p->is_bend = from_note_p->is_bend;
	to_note_p->smallbend = from_note_p->smallbend;
}


/* When done with temporary map data, clean everything up, to prepare
 * for potentially getting another set of data */

static void
clean_map_data()

{
	/* free up the lists */
	free_maps(Map_p);

	/* reset pointers to be ready for more data */
	Map_p = (struct MAP *) 0;
	End_map_p_p = &Map_p;
}


/* free up the MAP list and everything hanging off of it */

static void
free_maps(map_p)

struct MAP *map_p;

{
	if (map_p == (struct MAP *) 0) {
		/* end recursion */
		return;
	}

	/* free the list hanging off of this struct */
	free_notemaps(map_p->notemap_p);

	/* recurse */
	free_maps(map_p->next);

	/* free the passed-in struct */
	FREE(map_p);
}

/* free up a NOTEMAP list and everything hanging off of it */

static void
free_notemaps(notemap_p)

struct NOTEMAP *notemap_p;

{
	if (notemap_p == (struct NOTEMAP *) 0) {
		return;
	}

	free_sv_list(notemap_p->svlist_p);
	free_notemaps(notemap_p->next);
	FREE(notemap_p);
}


/* Given a number of notes, find the NOTEMAP list for that many and return it.
 * If not found, return 0. */

static struct NOTEMAP *
find_notemap(num_notes)

int num_notes;

{
	struct MAP *m_p;

	for (m_p = Map_p; m_p != (struct MAP *) 0; m_p = m_p->next) {
		if (m_p->num_entries == num_notes) {
			return(m_p->notemap_p);
		}
	}
	
	l_yyerror(Curr_filename, yylineno,
			"there is no bracketed mapping for chords containing %d note%s", num_notes, num_notes == 1 ? "" : "s");
	return ((struct NOTEMAP *) 0);
}


/* Once a measure-worth of data is gathered for one or more staffs/voices,
 * link copies onto the appropriate STAFF structs */

void
link_groups()

{
	/* if haven't yet set up the STAFFs for this measure, do so now */
	create_staffs();

	/* if we are in this function, user specified some music data */
	Got_some_data = YES;
	Got_group = YES;

	/* do error check--can't have notes and multirest in same measure */
	if (Got_multirest == 1) {
		report_mix_error();
		return;
	}

	/* Do either chord-to-voice-mapping or standard voice mapping,
	 * as appropriate. */
	if (map_groups() == NO) {
		do_link_groups();
	}

	/* re-initialize for next measure */
	Curr_gs_list_p = (struct GRPSYL *) 0;
	free_rlists();
}


/* Go through Svrangelist, creating copies of the GRPSYL lists and
 * linking them to the appropriate STAFFs. */

static void
do_link_groups()

{
	struct SVRANGELIST *svr_p;	/* list of ranges of staffs and vnos */
	register int s;		/* staff index */
	register int v;		/* voice index */
	struct RANGELIST *sr_p;	/* range of staffs being defined */
	struct RANGELIST *vr_p;	/* range of vno's being defined */
	int copies = 0;		/* how many copies of grpsyl list made so far */


	for (svr_p = Svrangelist_p; svr_p != (struct SVRANGELIST *) 0;
						svr_p = svr_p->next) {

		for (sr_p = svr_p->stafflist_p; sr_p != (struct RANGELIST *) 0;
							sr_p = sr_p->next) {

			for (s = sr_p->begin; s <= sr_p->end; s++) {

				for (vr_p = svr_p->vnolist_p;
						vr_p != (struct RANGELIST *) 0;
						vr_p = vr_p->next) {

					for (v = vr_p->begin;
							v <= vr_p->end; v++) {
						link_a_grouplist(s, v, copies++);
					}
				}
			}
		}
	}
}


/* connect list of GRPSYLs to a staff. If copies == 0, use the current
 * grpsyl list, otherwise make a copy of it and use the copy */

static void
link_a_grouplist(staffno, vno, copies)

int staffno;
int vno;
int copies;	/* if non-zero, need to make a copy */

{
	if (rangecheck(staffno, MINSTAFFS, Score.staffs, "staff number")
					== NO) {
		return;
	}

	if (rangecheck(vno, MINVOICES, MAXVOICES, "voice number") == NO) {
		return;
	}

	if (Staffmap_p[staffno] == (struct MAINLL *) 0) {
		return;
	}

	if (Staffmap_p[staffno]->u.staff_p == (struct STAFF *) 0) {
		pfatal("null staff pointer while linking group list");
	}

	if (Staffmap_p[staffno]->u.staff_p->groups_p[vno-1]
			!= (struct GRPSYL *) 0) {
		mult_def(staffno, vno);
		return;
	}

	/* the first time through, we can use the
	 * existing list. After that we need to
	 * make a clone of the list */
	if (copies == 0) {
		(Staffmap_p[staffno])->u.staff_p->groups_p[vno-1]
					= Curr_gs_list_p;
		convert_rest_space(Staffmap_p[staffno]->u.
					staff_p->groups_p[vno-1], staffno, vno);
	}
	else {
		(Staffmap_p[staffno])->u.staff_p->groups_p[vno-1]
					= clone_gs_list(Curr_gs_list_p, YES);
	}
}


/* With chord-at-a-time input style, it is legal to have
 * a mixture of pitches, spaces, and rests. However, once
 * everything has been distributed to individual voices, we need to check
 * that there aren't still any mixtures, and convert the rest and
 * space pseudo-notes into rest and space groups. Some error checks
 * also get done that couldn't be done till after this conversion. */

static void
convert_rest_space(grpsyl_p, staffno, vno)

struct GRPSYL *grpsyl_p;
int staffno;
int vno;

{
	for (   ; grpsyl_p != (struct GRPSYL *) 0; grpsyl_p = grpsyl_p->next) {
		conv_grpsyl(grpsyl_p, staffno, vno);
	}
}


/* Given a GRPSYL, convert all the rest and space
 * pseudo notes to groups and do related error checking */

static void
conv_grpsyl(grpsyl_p, staffno, vno)

struct GRPSYL *grpsyl_p;
int staffno;
int vno;

{
	int notes, rests, spaces, rpts;/* count how many of each in chord */
	int n;				/* index through notes */

	/* Count how many notes, rests, and spaces in the group */
	rests = spaces = notes = rpts = 0;
	for (n = 0; n < grpsyl_p->nnotes; n++) {
		if (grpsyl_p->notelist[n].letter == PP_REST) {
			rests++;
		}
		else if (grpsyl_p->notelist[n].letter == PP_SPACE) {
			spaces++;
		}
		else if (grpsyl_p->notelist[n].letter == PP_RPT) {
			rpts++;
		}
		else {
			notes++;
		}
	}

	/* Group may not mix space, rest, rpt, and notes */
	if (spaces > 0 && spaces != grpsyl_p->nnotes) {
		l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
			"staff %d voice %d: mixture of space and non-space",
			staffno, vno);
		return;
	}
	if (rests > 0 && rests != grpsyl_p->nnotes) {
		l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
			"staff %d voice %d: mixture of rest and non-rest",
			staffno, vno);
		return;
	}
	if (rpts > 0 && rpts != grpsyl_p->nnotes) {
		l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
			"staff %d voice %d: mixture of rpt and non-rpt",
			staffno, vno);
		return;
	}

	/* convert rest, space, and rpt pseudo-notes to groups */
	if (notes < grpsyl_p->nnotes) {
		if (rests > 0) {
			/* This is actually a rest group */
			grpsyl_p->grpcont = GC_REST;
			if (grpsyl_p->tie == YES) {
				l_warning(grpsyl_p->inputfile,
						grpsyl_p->inputlineno,
						"cannot tie from a rest");
			}
			grpsyl_p->tie = NO;

			/* gtc with lists should not apply to rests */
			if (grpsyl_p->with_was_gtc) {
				for (n = 0; n < grpsyl_p->nwith; n++) {
					if (grpsyl_p->withlist[n].string != (char *) 0) {
						FREE(grpsyl_p->withlist[n].string);
					}
				}
				FREE(grpsyl_p->withlist);
				grpsyl_p->nwith = 0;
			}

			/* If entire group was marked cue, leave it that way.
			 * Otherwise, if multiple rests map to this group,
			 * grpsize should be the biggest of them.
			 * So initialize to small size,
			 * and if we find any normal size,
			 * set to normal and jump out of the loop. */
			if (grpsyl_p->grpsize != GS_SMALL) {
				grpsyl_p->grpsize = GS_SMALL;
				for (n = 0; n < grpsyl_p->nnotes; n++) {
					if (grpsyl_p->notelist[n].notesize == GS_NORMAL) {
	
						grpsyl_p->grpsize = GS_NORMAL;
						break;
					}
				}
			}
		}
		else if (spaces > 0) {
			/* This is actually a space group */
			grpsyl_p->grpcont = GC_SPACE;
			if (grpsyl_p->tie == YES) {
				l_warning(grpsyl_p->inputfile,
						grpsyl_p->inputlineno,
						"cannot tie from a space");
			}
			grpsyl_p->tie = NO;
			/* Uncompressibility was temporarily saved
			 * in octave, so move it now. If multiple spaces
			 * mapped to this group, if any of them are
			 * uncompressible, make the group uncompressible. */
			for (n = 0; n < grpsyl_p->nnotes; n++) {
				if (grpsyl_p->notelist[n].octave == YES) {
					grpsyl_p->uncompressible = YES;
					break;
				}
			}
		}
		else if (rpts > 0) {
			/* This is actually a rpt. Internally, that is stored
			 * as a note group with no notes (nnotes gets zeroed
			 * a few lines down from here). This should 
			 * already be notes but doesn't hurt to set again. */
			grpsyl_p->grpcont = GC_NOTES;
		}
		if (grpsyl_p->notelist[0].slurtolist != (struct SLURTO *) 0) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
					"can't have slur on rest, space, or rpt");
		}
		free_notelist(grpsyl_p);
		grpsyl_p->notelist = 0;
		grpsyl_p->nnotes = 0;
	}

	if (grpsyl_p->grpcont == GC_NOTES) {
		if (grpsyl_p->is_meas == YES && grpsyl_p->nnotes > 0) {
			l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
				"'m' can only be used with rest, space, or rpt, not notes");
			return;
		}
	}

	/* grace can only apply to notes */
	if (grpsyl_p->grpvalue == GV_ZERO && (grpsyl_p->grpcont != GC_NOTES ||
					grpsyl_p->nnotes == 0)) {
		l_yyerror(grpsyl_p->inputfile, grpsyl_p->inputlineno,
				"grace can only be used with notes");
	}
}


/* If first group of a measure has no time value specified, we have to use
 * the default. This is complicated by the fact that the user could be 
 * defining mulitples staffs/voices at once. If they are, we need to make
 * sure that all of them have the same default. */

struct SSV *
get_dflt_timeunit_ssv()

{
	struct MAP *map_p;		/* list of maps */
	struct NOTEMAP *notemap_p;	/* list of notemaps per map */
	struct SVRANGELIST *svr_p;	/* list of staff/voice ranges */
	struct RANGELIST *sr_p;		/* list of staffs being defined */
	struct RANGELIST *vr_p;		/* list of voices being defined */
	int s;				/* staff number */
	int v;				/* voice */
	int got_one = NO;		/* YES if have found a dflt value */
	RATIONAL this_timeunit;		/* value for current staff/voice */
	RATIONAL dflt_timeunit;		/* the default time unit to use */
	struct SSV *tu_ssv_p;		/* SSV containing relevent timeunit */
	struct SSV *ref_ssv_p;		/* SSV we had checked on prev
					 * staff/voice being defined together */


	/* If doing voice-at-a-time input, use the special MAP for that case,
	 * otherwise use the Map_p */
	if (Map_p == (struct MAP *) 0) {
		map_p = &Voice_at_a_time_map;
		map_p->notemap_p->svlist_p = Svrangelist_p;
	}
	else {
		map_p = Map_p;
	}

	/* score value is the ultimate default */
	dflt_timeunit = Score.timeunit;
	tu_ssv_p = ref_ssv_p = &Score;

	/* check each map/notemap/svrangelist/svrange/staff/voice combination */
	for (   ; map_p != (struct MAP *) 0; map_p = map_p->next) {
	    for (notemap_p = map_p->notemap_p;
				notemap_p != (struct NOTEMAP *) 0;
				notemap_p = notemap_p->next) {
		for (svr_p = notemap_p->svlist_p;
					svr_p != (struct SVRANGELIST *) 0;
					svr_p = svr_p->next) {
		    for (sr_p = svr_p->stafflist_p;
						sr_p != (struct RANGELIST *) 0;
						sr_p = sr_p->next) {
			for (s = sr_p->begin; s <= sr_p->end; s++) {
			    for (vr_p = svr_p->vnolist_p;
						vr_p != (struct RANGELIST *) 0;
						vr_p = vr_p->next) {
				for (v = vr_p->begin; v <= vr_p->end; v++) {

				    /* find default timeunit for
				     * this staff/voice */
				    tu_ssv_p = vvpath(s, v, TIMEUNIT);
				    this_timeunit = tu_ssv_p->timeunit;

				    if (got_one == NO) {
					/* now we have one to
					 * compare against */
					dflt_timeunit = this_timeunit;
					ref_ssv_p = tu_ssv_p;
					got_one = YES;
				    }
				    else if ( NE(this_timeunit, dflt_timeunit)
						|| timelists_equal(
						tu_ssv_p->timelist_p,
						ref_ssv_p->timelist_p)
						== NO) {
					yyerror("timeunit value must be the same for all staffs and voices being defined on the same input line");
				    }
				}
			    }
			}
		    }
		}
	    }
	}

	return(tu_ssv_p);
}


/* Return YES if the given lists are equivalent, NO if they aren't. */

int
timelists_equal(list1_p, list2_p)

struct TIMELIST *list1_p;
struct TIMELIST *list2_p;

{
	for (   ; list1_p != 0 && list2_p != 0;
			list1_p = list1_p->next, list2_p = list2_p->next) {
		if ( NE(list1_p->fulltime, list2_p->fulltime) ) {
			return(NO);
		}
	}
	return((list1_p == 0 && list2_p == 0) ? YES : NO);
}


/* Return YES if the current staff range is for all tablature staffs.
 * Return NO if not. If there are a mixture, this is an error, so print
 * a message. It still returns NO in this case, so if the user wanted tab,
 * they may get a lot of errors.  Oh well. After all, they did make an error.
 */ 

int
is_tab_range()

{
	struct MAP *map_p;
	struct NOTEMAP *notemap_p;
	struct SVRANGELIST *svr_p;
	struct RANGELIST *sr_p;
	int s;				/* staff number */
	int found_tab_staff = NO;
	int found_non_tab_staff = NO;

	/* If doing voice-at-a-time input, use the special MAP for that case,
	 * otherwise use the Map_p */
	if (Map_p == (struct MAP *) 0) {
		map_p = &Voice_at_a_time_map;
		map_p->notemap_p->svlist_p = Svrangelist_p;
	}
	else {
		map_p = Map_p;
	}

	/* check each map/notemap/svrangelist/svrange/staff/voice combination */
	for (   ; map_p != (struct MAP *) 0; map_p = map_p->next) {
		for (notemap_p = map_p->notemap_p;
				notemap_p != (struct NOTEMAP *) 0;
				notemap_p = notemap_p->next) {
			for (svr_p = notemap_p->svlist_p;
					svr_p != (struct SVRANGELIST *) 0;
					svr_p = svr_p->next) {
		    		for (sr_p = svr_p->stafflist_p;
						sr_p != (struct RANGELIST *) 0;
						sr_p = sr_p->next) {
					for (s = sr_p->begin; s <= sr_p->end; s++) {
						if (is_tab_staff(s) == YES) {
							found_tab_staff = YES;
						}
						else {
							found_non_tab_staff = YES;
						}
					}
				}
			}
		}
	}

	if (found_tab_staff == YES && found_non_tab_staff == YES) {
		yyerror("mixture of tab and non-tab staffs not allowed");
	}

	return(found_tab_staff);
}


/* When two notes in a chord are duplicates from chord-at-a-time input,
 * we want to merge the notes, and get rid of the extra. This function will
 * change the value of gs_p->nnotes and the size of the notelist array. */

void
merge_dup_notes(gs_p, n)

struct GRPSYL *gs_p;	/* remove duplicate from here */
int n;			/* merge note n and n+1 into slot n, then remove
			 * the note in slot n+1 by moving any remaining
			 * notes down. */

{
	int i;
	int sn, sn1;		/* slurto index */
	struct NOTE *note_p, *extra_p;


	/* get shorter names for what we will use a lot */
	note_p = &(gs_p->notelist[n]);
	extra_p = &(gs_p->notelist[n+1]);
	/* Merge the data between the two as best we can.
	 * In general, if one has a "stronger" version of some attribute,
	 * go with that one. We check the second; if it is "stronger",
	 * force the result to that--if it was already set the same, fine--
	 * probably faster to just assign than check and maybe assign.
	 * If second wasn't stronger, go with whatever the first was. */

	/* If one is normal, one small, override the small */
	/* Note that things work for grace, because they come in
	 * here as either NORMAL (usual case)
	 * or TINY (if the user explicitly added a ?).
	 * If any are NORMAL, the combined stays NORMAL, and then later
	 * get made SMALL because it is grace. If all are TINY, then the
	 * combined stays TINY. */
	if (extra_p->notesize == GS_NORMAL) {
		note_p->notesize = GS_NORMAL;
	}

	/* If either has a tie, do a tie */
	if (extra_p->tie == YES) {
		note_p->tie = YES;
	}
	/* Consider normal tie the strongest, then dashed, then dotted */
	if (extra_p->tiestyle == L_NORMAL) {
		note_p->tiestyle = L_NORMAL;
	}
	else if (extra_p->tiestyle == L_DASHED &&
			note_p->tiestyle != L_NORMAL) {
		note_p->tiestyle = L_DASHED;
	}
	if ( (extra_p->tiedir == UP && note_p->tiedir == DOWN) ||
			(extra_p->tiedir == DOWN && note_p->tiedir == UP)) {
		/* It would be nice to allow both up and down tie,
		 * especially since we can do that for slurs,
		 * but we only support one tie per note. */
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
			"duplicate notes with opposite tie direction not allowed");
	}
	else if (extra_p->tiedir != UNKNOWN) {
		note_p->tiedir = extra_p->tiedir;
	}


	/* Parentheses around an accidental means "just in case you forgot..."
	 * which is weaker than no parentheses, so only use parentheses if
	 * both have it. */
	if (extra_p->acc_has_paren == NO) {
		note_p->acc_has_paren = NO;
	}
	/* Parentheses around a note generally means the note is optional.
	 * An optional merged with a non-optional, is non-optional. */
	if (extra_p->note_has_paren == NO) {
		note_p->note_has_paren = NO;
	}

	/* Sorry, we don't deal with incompatible bends */
	if (note_p->is_bend != extra_p->is_bend ||
			extra_p->smallbend != note_p->smallbend) {
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
			"duplicate notes with bend mismatch not allowed");
	}

	/* Slurs... If duplicate between the two slurto lists, just leave
	 * the one, but use strongest slurstyle. If there is one in the
	 * NOTE to be deleted but not in the one to keep, move it. */
	for (sn1 = 0; sn1 < extra_p->nslurto; sn1++) {
		for (sn = 0; sn < note_p->nslurto; sn++) {
			if (note_p->slurtolist[sn].letter ==
					extra_p->slurtolist[sn1].letter &&
					note_p->slurtolist[sn].slurdir ==
					extra_p->slurtolist[sn1].slurdir &&
					note_p->slurtolist[sn].octave ==
					extra_p->slurtolist[sn1].octave) {
				/* duplicate; just fix style if necessary */
				if (extra_p->slurtolist[sn1].slurstyle
								== L_NORMAL) {
					note_p->slurtolist[sn].slurstyle
							= L_NORMAL;
				}
				else if (extra_p->slurtolist[sn1].slurstyle
						== L_DASHED &&
						note_p->slurtolist[sn].slurstyle
						!= L_NORMAL) {
					note_p->slurtolist[sn].slurstyle
							= L_DASHED;
				}
				break;
			}
		}
		if (sn == note_p->nslurto) {
			/* wasn't on the list to keep, so add it */
			add_slurto(gs_p, extra_p->slurtolist[sn1].letter,
					extra_p->slurtolist[sn1].octave,
					sn,
					extra_p->slurtolist[sn1].slurstyle);
		}
	}

	/* Move things down in the notelist to remove the extra */
	for (i = n + 1; i < gs_p->nnotes - 1; i++) {
		gs_p->notelist[i] = gs_p->notelist[i+1];
	}
	(gs_p->nnotes)--;
	REALLOC(NOTE, gs_p->notelist, gs_p->nnotes);
}


/* print error message for multiply defined voice */

static void
mult_def(staff, voice)

int staff;
int voice;

{
	l_yyerror(Curr_filename, yylineno,
		"staff %d voice %d multiply defined (first defined on line %d)",
		staff, voice,
		Staffmap_p[staff]->u.staff_p->groups_p[voice-1]->inputlineno);
}
