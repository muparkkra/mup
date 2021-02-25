
/*
 Copyright (c) 1995-2021  by Arkkra Enterprises.
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

/* This file contains functions that allow the user to specify gradual
 * changes in midi values, and have Mup figure out a series of discreet
 * changes to produce the gradual change.
 *
 * There are 3 top level functions that get called from the midi.c file:
 * - For any midi items where to/til is legal, process_to_list() is called.
 * - For any midi items where to/til is illegal, nix_til() is called.
 * - At the beginning of each measure, do_pending_gradual_midi() is
 * called to create any derived midi STUFFs for that measure.
 *
 * User input uses "to" inside the midi command string and a "til" clause.
 * There are three varations. Most are like:
 * 	midi all: 1 "tempo=120 to 90 to 40" til 2m+4;
 * but parameter has both a parameter number and value, so the parameter
 * number is only specified once:
 *	midi 2: 3 "parameter=7,60 to 70 to 96" til 6;
 * and {on|off}velocity can have comma-separated lists, and each list can
 * potentially be a different length:
 *	midi 1: 1 "onvelocity=80,60 to 92,60,46 to 100" til 3m+2;
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"


/* Maximum possible number of velocity values. There are 128 possible midi
 * notes (7 bit unsigned number), so that is the theoretical maximum. 
 * Mup itself actually only supports MAXHAND, which is less,
 * and some of those are outside the midi range.
 * So 128 should be more than generous.
 */
#define MAX_VELS	128

/* We have a mini-parser of the "to" lists. These are the parser states,
 * saying what kind of token we are expecting next. */
#define EXPECT_NUMBER		(0)
#define EXPECT_COMMA_OR_TO	(1)


/* This struct hold all the information we need about one midi STUFF that
 * has a "to" list and "til" clause. In the measure where it occurs, we
 * calculate all the points in time where changes will be needed, and the
 * values at those points. Then as we are walking through the song a
 * measure at a time, new midi STUFFs are created from this info for the
 * current measure. When we reach the end of the til, the instance is deleted.
 * In the case of repeats, the instance will get created and processed twice,
 * though if the til clause extends through endings, the actual measures
 * that it is processed for can be different.
 */
struct PENDGRAD {
	struct STUFF *stuff_p;	/* User's original STUFF that has a "to" and
				 * "til." We derive others based on this. */
	float *time_p;		/* List of time offsets at which to change */
	short **values_p_p;	/* Lists of new values at each change point.
				 * Usually will be only a single list,
				 * but with velocity, the number of lists
				 * will be the length of the longest
				 * comma-separated list of values. */
	int numpoints;		/* number of items in time and values lists */
	int numlists;		/* how many values lists there are */
	int index;		/* current index in time and values lists */
	double time_offset;	/* how far we have processed so far from
				 * the beginning of the measure containing
				 * the start of the gradual change */
	char *miditype;		/* e.g., "onvelocity" or "program" */
	short param;		/* If miditype is parameter, this is which
				 * parameter; otherwise it should be set
				 * to a negative value. */
	short timenum;		/* time sig numerator at start of orig STUFF */
	short timeden;		/* time sig denominator a start of orig STUFF */
	char *inputfile;	/* where the midi was defined, for errors */
	int inputlineno;	/* where the midi was defined, for errors */
	struct PENDGRAD *next;	/* for linked list */
};

/* This is the list of gradual changes yet to be processed. */
static struct PENDGRAD *Pendgrad_p;

/* Static functions for this file */
static struct CRVLIST **make_to_lists P((char *str, int minval, int maxval,
		int maxlists, char *miditype, int *numpoints_p,
		char *fname, int linenum));
static int do_1_pending_gradual_midi P((struct PENDGRAD *pendinfo_p));
static void free_pendinfo P((struct PENDGRAD *pendinfo_p));
static int set_crvlist_x P((struct CRVLIST **crvlist_p_p, struct STUFF *stuff_p,
		int numpoints, int numlists));
static void add_midi P((struct STUFF *stuff_p, double offset, char *miditype,
		char *value_string));
static void add_to_row P((struct CRVLIST **crvlist_p_p, int row, int value));
static struct CRVLIST *get_head P((struct CRVLIST *crvlist_tail_p));
static void clone_row P((struct CRVLIST **crvlist_p_p, int row));
static void free_to_lists P((struct CRVLIST **to_lists_p_p, int numlists));
static int fix_y_value P((double y, int min_limit, int max_limit));
static int interpolate_midi P((struct CRVLIST *begpoint_p, int min_limit,
		int max_limit, double x_incr, int keep_dups,
		float **x_p_p, short **y_p_p));


/* This function is called for midi item types for which til is not allowed.
 * If the STUFF has a til, this function prints a warning.
 * Note that these types would also not be allowed to have a "to" list,
 * but most of these types are arbitrary text strings, so if the string
 * happens to contain "to" in it somewhere, that is perfectly fine. The only
 * exception is hex, and in that case, since "to" is not valid hex,
 * that will already be caught elsewhere.
 */

void
nix_til(stuff_p, miditype)

struct STUFF *stuff_p;	/* What to check */
char *miditype;		/* Like "text" or "hex" */

{
	if (has_til(stuff_p)) {
		l_warning(stuff_p->inputfile, stuff_p->inputlineno,
		"midi '%s' is not allowed to have a til clause; ignoring", miditype);
	}
}


/* This function checks if a midi command has a "to" list and a "til"
 * clause, and if so, processes it. It may add midi STUFFs to the current
 * measure and/or save info away for subsequent measures.
 *
 * First it determines if there is a til clause, meaning it actually has
 * to do something. If so, it calls make_to_lists(), which builds up one
 * or more CRVLIST linked lists, where the Y values are the values given
 * by the user at each point, and the X values are relative time.
 * Then it calls interpolate_midi() to find out at which specific times to
 * generate derived midi STUFFs, and the values to set at those times.
 * It generates any that are in the current measure,
 * and saves information away to be used in later measures,
 * to be processed by do_pending_gradual_midi().
 *
 * In the case of velocity, we may have multiple lists.
 * Each list may have changes at different points, and we need to generate
 * derived midi any time any of them change. So to keep things simple,
 * we call interpolate_midi on each, always passing a special flag
 * to tell it to keep all values, not compressing out duplicates. That
 * will ensure that all the lists that it gives us are the same length.
 * We will then generate a midi STUFF at every time increment,
 * whether any value actually changed since the last increment or not. 
 * Usually we want to suppress any no-change instances, but
 * since velocity STUFFs don't actually directly produce any actual midi
 * file output--they merely specify what velocity values to use if and when
 * there happens to be another note--if there is a large number of them,
 * it will take some CPU and memory for Mup itself, but the size of the MIDI
 * file will be unaffected, and we will not use any additional midi bandwidth.
 */

void
process_to_list(stuff_p, miditype, usec_per_quarter, minval, maxval)

struct STUFF *stuff_p;	/* The MIDI STUFF to process */
char *miditype;		/* The midi keyword, like "onvelocity", etc */
int usec_per_quarter;	/* microseconds per quarter note */
int minval;		/* minimum value allowed */
int maxval;		/* maximum value allowed */

{
	char *string;		/* where list starts in stuff_p->string */
	int param;		/* If parameter, its number, else -1 */
	struct CRVLIST **to_lists_p_p;	/* array of linked lists of "to"
				 * values. Usually the array has only one
				 * element, by for velocity can have more. */
	int numpoints;		/* how many points in lists of "to" values */
	double increment;	/* how often to check for changes in value */
	struct PENDGRAD *pendinfo_p;	/* where we save information */
	int maxlists;		/* MAX_VELS if velocity, else 1 */
	int numlists;		/* actual number of lists (<= maxlists) */
	int n;			/* for looping through lists */


	/* See if we have to do anything at all. Usually we won't. */
	if (strstr(stuff_p->string, "to") == 0) {
		/* This is a normal MIDI stuff with no "to" */
		if (has_til(stuff_p)) {
			l_yyerror(stuff_p->inputfile, stuff_p->inputlineno,
				"midi without 'to' cannot have 'til'");
		}
		return;
	}

	/* There is a "to," so there must also be a "til." */
	if ( ! has_til(stuff_p) ) {
		l_yyerror(stuff_p->inputfile, stuff_p->inputlineno,
			"midi with a 'to' must also have a 'til'");
		return;
	}

	/* We have no idea how much a "stepsize" should alter the duration
	 * because that only makes sense for visual things, not for MIDI. */
	if (stuff_p->start.steps != 0.0 || stuff_p->end.steps != 0.0) {
		l_warning(stuff_p->inputfile, stuff_p->inputlineno,
			"steps offset is senseless on midi, and is being ignored");
	}

	/* Skip in the string to the first value. Usually this is just
	 * beyond the =, but in the case of parameter, we skip past the
	 * parameter number as well, after saving the parameter number.
	 */
	if ((string = strchr(stuff_p->string, '=')) == 0) {
		/* We shouldn't have been called if there was no = */
		pfatal("can't find = in process_to_list()");
	}

	/* Skip past the = */
	string++;

	if (strcmp(miditype, "parameter") == 0) {
		/* Save the parameter number */
		param = strtol(string, &string, 10);
		/* Skip past the comma */
		if ((string = strchr(string, ',')) == 0) {
			/* We shouldn't have been called if there was no comma */
			pfatal("can't find comma in param in process_to_list()");
		}
		string++;
	}
	else {
		/* Some midi thing other than a parameter */
		param = -1;
	}

	/* Only velocity things can have a comma-separated list of values;
	 * everything else can only have individual values.
	 * Since onvelocity and offvelocity happen to be the only midi things
	 * that start with an "o," we can cheat, and just check that.
	 */
	maxlists = (miditype[0] == 'o' ? MAX_VELS : 1);

	/* Extract all the "to" values and put them in one or more lists. */
	if ((to_lists_p_p = make_to_lists(string, minval, maxval,
				maxlists, miditype,
				&numpoints, stuff_p->inputfile,
				stuff_p->inputlineno)) == 0) {
		/* Something went wrong; give up.
		 * It should have already printed an error message
		 * explaining what the problem was.
	 	 */
		return;
	}

	/* Count up number of lists actually used. We go till we find
	 * the first of:
	 * - the maximum allowed number of lists (which is usually 1)
	 * - a zero array element, indicating no more lists.
	 */
	for (numlists = 0; numlists < maxlists; numlists++) {
		if (to_lists_p_p[numlists] == 0) {
			break;
		}
	}

	/* Calculate and fill in all the x values */
	if (set_crvlist_x(to_lists_p_p, stuff_p, numpoints, numlists) == NO) {
		/* Something went wrong */
		free_to_lists(to_lists_p_p, numlists);
		return;
	}

	/* We want to do time increments of about 50 ms. We choose that value
	 * because we figure 20 notes per second is probably faster than
	 * almost any music goes, and too little time for most people
	 * to tell a difference, but (hopefully) not so short
	 * that we overwhelm the MIDI data stream with changes. If we figure
	 * most MIDI things are two bytes, which takes 640 microseconds,
	 * we should be able to do about 78 average events in 50 ms.
	 * We know the number of microseconds per quarter note,
	 * so we figure out how many quarter notes there are in 50,000
	 * microseconds, then adjust if the time signature denominator
	 * is something other than quarter note.
	 * Note that if what the user is changing is the tempo, then
	 * the number of beats in 50 ms will change over the time,
	 * so maybe we should take the average or worse case or something,
	 * but it isn't clear exact what, and since the 50 ms is
	 * a semi-arbitrary guess anyway, unless the tempo change
	 * is really large, we'll probably have something close enough.
	 */
	increment = 50000.0 / usec_per_quarter * Score.timeden  / 4.0;

	/* Allocate and fill in a record for storing the info. */
	MALLOC(PENDGRAD, pendinfo_p, 1);
	pendinfo_p->stuff_p = stuff_p;
	pendinfo_p->miditype = miditype;
	pendinfo_p->numlists = numlists;
	pendinfo_p->param = param;
	pendinfo_p->timenum = Score.timenum;
	pendinfo_p->timeden = Score.timeden;
	pendinfo_p->inputfile = stuff_p->inputfile;
	pendinfo_p->inputlineno = stuff_p->inputlineno;
	/* We start off relative to beginning of measure of the STUFF */
	pendinfo_p->time_offset = 0.0;
	/* We start at index 1, because the [0] point is handled just
	 * like a normal MIDI STUFF that doesn't have to/til. */
	pendinfo_p->index = 1;

	/* Allocate an array of value lists */
	MALLOCA(short *, pendinfo_p->values_p_p, pendinfo_p->numlists);

	/* Calculate when to change values */
	for (n = 0; n < numlists; n++) {
		if (n > 1) {
			/* For multiple lists, we will always get an
			 * identical time list for each, so if we already
			 * have one, free it. */
			FREE(pendinfo_p->time_p);
		}
		/* Call the function that will return a pair of lists that tell
		 * at what points in time to change the MIDI value. */
		if ((pendinfo_p->numpoints = interpolate_midi(to_lists_p_p[n],
					minval, maxval, increment,
					(numlists == 1 ? NO : YES),
					&(pendinfo_p->time_p),
					&(pendinfo_p->values_p_p[n]) ) ) == 0) {
			/* Something went wrong; give up. */
			free_to_lists(to_lists_p_p, numlists);
			FREE(pendinfo_p);
			return;
		}
	}

	/* We don't need the to_lists anymore. They were just a
	 * handy intermediate format to pass to interpolate_midi(). */
	free_to_lists(to_lists_p_p, numlists);

	/* Add midi STUFFs for this item for the current measure.
	 * Subsequent measures will be handled via do_gradual_midi() call. */
	if (do_1_pending_gradual_midi(pendinfo_p) == YES) {
		/* More to do in subsequent measures, so link onto list. */
		pendinfo_p->next = Pendgrad_p;
		Pendgrad_p = pendinfo_p;
	}
}


/* This function calculates and sets all the x values in an array
 * of CRVLIST lists. It divides the duration evenly among the segments.
 * It returns YES on success, NO on failure.
 */

static int
set_crvlist_x(crvlist_p_p, stuff_p, numpoints, numlists)

struct CRVLIST **crvlist_p_p;	/* the array of lists to fix */
struct STUFF *stuff_p;		/* get start and til from here */
int numpoints;			/* to get number of segments */
int numlists;			/* how many lists in array */

{
	struct CRVLIST *list_p;	/* to walk through crvlist_p_p */
	double start, end;	/* time endpoints of the STUFF */
	double duration;	/* overall duration: end - start */
	double x_segment;	/* duration between "to" points */
	int n;			/* to loop through array of lists */


	/* We use the start beat as the start time, and then calculate
	 * the end time based on the "til" value and the time signature.
	 * We don't allow changing time signature in the middle,
	 * but won't catch that till later; for now we assume things are good.
	 */
	start = stuff_p->start.count;
	if (stuff_p->end.bars == 0) {
		/* All in the same measure */
		end = stuff_p->end.count;
	}
	else {
		/* End is the number of measures times the time signature
		 * numerator plus the offset into the final measure. */
		end = stuff_p->end.bars * Score.timenum + stuff_p->end.count;
	}

	/* Now that we know start and end, we can calculate the duration. */
	duration = end - start;
	if (duration <= 0.0) {
		l_yyerror(stuff_p->inputfile, stuff_p->inputlineno,
				"duration must be greater than zero");
		return(NO);
	}

	/* Calculate the duration of each segment.
	 * If there are 3 points, it will be half the full duration;
	 * if 4 points, one third, etc.
	 */
	x_segment = duration / (numpoints - 1);

	/* Do all the lists */
	for (n = 0; n < numlists; n++) {
		/* Get which list to do this time through the loop */
		list_p = crvlist_p_p[n];

		if (list_p == 0) {
			pfatal("list_p is null on list %d in set_crvlist_x()", n);
		}

		/* Fill in the points. We already know the start and end.
		 * We loop through the intermediates, adding the segment length
		 * to the previous value.
		 */
		list_p->x = start;
		for (list_p = list_p->next; list_p->next != 0;
						list_p = list_p->next) {
			list_p->x = list_p->prev->x + x_segment;
		}
		list_p->x = end;
	}
	return(YES);
}


/* This function processes one PENDGRAD struct for the current measure,
 * adding midi STUFFs at the appropriate times. It returns YES if there is
 * still more to do beyond this measure, NO if we are done.
 * If done, the struct and what it points to will have been freed.
 */

static int
do_1_pending_gradual_midi(pendinfo_p)

struct PENDGRAD *pendinfo_p;	/* what to process */

{
	double end_curr_meas;	/* offset from beginning of measure where
				 * the til clause began to the end of
				 * this current measure */
	int i;			/* current index into time/value list */
	double offset;		/* offset into current measure */
	short *values_p;	/* current values list */
	char value_as_string[MAX_VELS * 4]; /* Buffer for the string version
				 * of value. Longest possible should be for
				 * velocity. Each value could be up to
				 * 3 digits, although most will be
				 * only two digits, and some only one.
				 * Then there is a comma between each,
				 * and a null after the last.
				 * So allowing 4 bytes per value is sure
				 * to give us more than enough.
				 */


	/* We don't allow the time signature to change during a til clause.
	 * There are two reasons for this. First,
	 * to make it easy to deal with repeats/endings, we do things a 
	 * measure at a time, and a side effect of that is that we can't
	 * easily know at the time we are calculating the points
	 * whether some future measure (especially those in endings)
	 * might have a time signature change;
	 * we only discover it now, when it is too late.
	 * So we neatly avoid the issue by simply disallowing it.
	 * But secondly, even apart from any implementation complexities,
	 * it is questionable what it would mean.
	 * E.g., if there is one measure in 4/4 and another in 6/8,
	 * does the user want the value spread out over 10 counts (4 plus 6)
	 * or over 7 quarter note counts (keeping quarter notes constant,
	 * so 6/8 effectively takes as much time as 3/4),
	 * or maybe something else?
	 * So if they change, we print a warning, and ignore the rest.
	 */
	if (pendinfo_p->timenum != Score.timenum
				|| pendinfo_p->timeden != Score.timeden) {
		l_warning(pendinfo_p->inputfile, pendinfo_p->inputlineno,
			"time signature changed from %d/%d to %d/%d, %d measures into midi '%s;' remainder of til clause will have no effect",
			pendinfo_p->timenum, pendinfo_p->timeden,
			Score.timenum, Score.timeden,
			(int) (pendinfo_p->time_offset / pendinfo_p->timeden),
			pendinfo_p->miditype);
		free_pendinfo(pendinfo_p);
		return(NO);
	}

	/* Find the count at which the current measure ends. */
	end_curr_meas = pendinfo_p->time_offset + Score.timeden + 1.0;

	/* Walk through pending change points to end of list or measure. */
	for (   ; pendinfo_p->index < pendinfo_p->numpoints;
						(pendinfo_p->index)++) {
		/* Get shorter name for index. */
		i = pendinfo_p->index;

		if (pendinfo_p->time_p[i] >= end_curr_meas) {
			/* We are done with the current measure,
			 * but there are more measures to do.
			 * Update the information on where we are,
			 * then tell caller there is more to do.
			 */
			pendinfo_p->time_offset = end_curr_meas;
			return(YES);
		}
		/* Find the offset into the current measure. */
		offset = pendinfo_p->time_p[i] - pendinfo_p->time_offset;

		/* Construct the value like the user would have typed in. */
		if (pendinfo_p->param >= 0) {
			values_p = pendinfo_p->values_p_p[0];
			(void) sprintf(value_as_string, "%d,%d",
				pendinfo_p->param, values_p[i]);
		}
		else {
			int n;		/* loop through lists */
			char *str_p;	/* current end of value_as_string,
					 * so we can concatenate to the end
					 * of it efficiently. */

			str_p = value_as_string;
			/* Add the values for the current time from each list */
			for (n = 0; n < pendinfo_p->numlists; n++) {
				if (n > 0) {
					/* velocity may have multiple,
					 * comma-separated values. */
					(void) sprintf(str_p, ",");
					str_p++;
				}
				values_p = pendinfo_p->values_p_p[n];
				(void) sprintf(str_p, "%d", values_p[i]);
				/* Keep track of end, for concatenation */
				str_p += strlen(str_p);
			}
		}

		/* Add a MIDI STUFF. */
		add_midi(pendinfo_p->stuff_p, offset, pendinfo_p->miditype,
							value_as_string);
	}

	/* We are done with this item. */
	free_pendinfo(pendinfo_p);
	return(NO);
}


/* Given a PENDGRAD, free it and everything it points to.
 * If it is linked onto a list, caller has to unlink it from there
 * (which means caller may need to save its next pointer before it
 * gets destroyed here).
 */

static void
free_pendinfo(pendinfo_p)

struct PENDGRAD *pendinfo_p;	/* Free this and what is points to */

{
	int n;		/* index through lists */


	/* Free the times list (x). */
	FREE(pendinfo_p->time_p);

	/* Free the lists of values (y). */
	for (n = 0; n < pendinfo_p->numlists; n++) {
		FREE(pendinfo_p->values_p_p[n]);
	}

	/* Free the array pointing to the values lists. */
	FREE(pendinfo_p->values_p_p);

	/* Free the top level struct. */
	FREE(pendinfo_p);
}


/* For gradual MIDI changes, create and add a new STUFF to the list
 * of MIDI STUFFs for the current measure.
 */

static void
add_midi(stuff_p, offset, miditype, value_string)

struct STUFF *stuff_p;	/* clone from this STUFF */
double offset;		/* time offset into measure to put the new STUFF */
char *miditype;		/* E.g., "program," "port", etc */
char *value_string;	/* the text that goes after the = in the midi string */

{
	struct STUFF *newstuff_p;	/* the new item to insert */
	char *command;			/* the new "miditype=value_string" */


	/* Create the midi string like the user would have typed in */
	MALLOCA(char, command, strlen(miditype) + strlen(value_string) + 4);
	(void) sprintf(command, "%c%c%s=%s",
				stuff_p->string[0], stuff_p->string[1],
				miditype, value_string);

	/* Create new STUFF based on the template to clone from, but with
	 * the string we just created. */
	newstuff_p = newSTUFF(command, 0.0, SD_NONE, NOALIGNTAG,
			offset, 0.0, 0,		/* start */
			0, 0.0, 0.0, 0,		/* end (not til anymore) */
			stuff_p->stuff_type,
			stuff_p->modifier,
			stuff_p->place,
			stuff_p->inputfile,
			stuff_p->inputlineno);
	newstuff_p->all = stuff_p->all;

	/* Add to list of MIDI tasks to do in the current measure. */
	insert_midistufflist(newstuff_p);
}


/* This function walks through the list of PENDGRAD structs, and
 * processes each one for the current measure.
 */

void
do_pending_gradual_midi()

{
	struct PENDGRAD *pendinfo_p;	/* walk through pending list */
	struct PENDGRAD *next_p;	/* in case we delete the current */
	struct PENDGRAD **parent_p_p;	/* for closing gap when deleting */


	for (pendinfo_p = Pendgrad_p, parent_p_p = &Pendgrad_p; pendinfo_p != 0;
					pendinfo_p = next_p) {

		/* In case we need to delete the current... */
		next_p = pendinfo_p->next;

		if (do_1_pending_gradual_midi(pendinfo_p) == YES) {
			/* This measure worked; more measures pending. */
			parent_p_p = &(pendinfo_p->next);
		}
		else {
			/* Either we've reached the end of the til
			 * or something went wrong.
			 * Clean up: unhook entry from pending list.
			 */
			*parent_p_p = next_p;
		}
	}
}


/* This function parses a string, and returns a malloc-ed array of
 * malloc-ed CRVLIST lists that are the internal representation.
 * It also returns (via pointer) the number of points.
 *
 * This has a mini-paser. It just has to look for numbers, the "to" keyword,
 * and in the case of velocity, commas.
 *
 * For the simple (non-velocity) case, it will generate a single linked list
 * of CRVLIST structs, where each number in the user's input string
 * becomes a list element.
 *
 * In the case of velocity, if the user specifies multiple values, it will
 * create multiple parallel lists. Each list will have the same number of
 * points. The number of lists will be equal to the length of the longest
 * set of comma-separated values. Since the last number in such sets means
 * "all remaining notes in the chord," we basically pretend the user entered
 * that number as many times as necessary to be as long as the longest list.
 * So, for example, consider an input of
 *	50 to 60,40 to 80,70,60,50,40 to 75,65,55
 * we would end up with 5 lists (because of the 80,70,60,50,40 that is
 * length 5), and the lists would be
 *	50 60 80 75
 *	50 40 70 65
 *	50 40 60 55
 *	50 40 50 55
 *	50 40 40 55
 * Then we need to generate a midi any time any one of the values changes.
 * See process_to_lists() for explanation of how we handle that.
 */

struct CRVLIST **
make_to_lists(string, minval, maxval, maxlists, miditype, numpoints_p, filename, lineno)

char *string;		/* The string to parse, starting at the list */
int minval;		/* minimum allowed y value */
int maxval;		/* maximum allowed y value */
int maxlists;		/* max number of lists, 1 for all except velocities */
char *miditype;		/* like "parameter" or "onvelocity" */
int *numpoints_p;	/* number of points in "to" list is returned here */
char *filename;		/* input file name, for errors */
int lineno;		/* input line number, for errors */

{
	struct CRVLIST **crvlists_p_p;	/* the lists we will return */
	long value;			/* current value being parsed */
	int expecting;			/* state--what we expect to see next */
	int c;				/* index into crvlists_p_p */
	int row;			/* which list we are processing */
	int error;			/* YES when error occurs */


	/* Allocate the array that points to the lists. Usually this will
	 * be a degenerate case of an array with only one member, but in
	 * the case of of velocities, will be bigger. We handle everything
	 * the same as much as possible to keep the code common and general.
	 */
	MALLOCA(struct CRVLIST *, crvlists_p_p, maxlists);

	/* Start with all empty lists. */
	for (c = 0; c < maxlists; c++) {
		crvlists_p_p[c] = 0;
	}
	/* We know we have at least one point.
	 * Each 'to' we find will increment. */
	*numpoints_p = 1;

	/* Initialize for parsing. */
	expecting = EXPECT_NUMBER;
	row = 0;
	error = NO;
	value = 0;	/* for lint */

	/* Parse the string till done or we find an error. */
	while (*string != '\0' && error == NO) {

		/* Skip all white space */
		if (*string == ' ' || *string == '\t') {
			string++;
			continue;
		}

		switch (expecting) {

		case EXPECT_NUMBER:
			if (isdigit(*string)) {
				value = strtol(string, &string, 10);
				if (value < minval || value > maxval) {
					l_yyerror(filename, lineno,
						"value %d out of range (%d-%d)",
						value, minval, maxval);
					error = YES;
					break;
				}

				/* If the current row is empty, that means
				 * this is the first point that had this
				 * many values. So clone the row above,
				 * if any.
				 */
				if (crvlists_p_p[row] == 0 && row > 0) {
					clone_row(crvlists_p_p, row);
				}

				add_to_row(crvlists_p_p, row, value);

				expecting = EXPECT_COMMA_OR_TO;
			}
			else {
				l_yyerror(filename, lineno,
						"expecting a number");
				error = YES;
				break;
			}
			break;

		case EXPECT_COMMA_OR_TO:
			if (*string == ',') {
				string++;
				if (++row >= maxlists) {
					if (maxlists == 1) {
						l_yyerror(filename, lineno,
						"'%s' can only have a single value, not a list of values",
						miditype);
					}
					else {
						l_yyerror(filename, lineno,
						"'%s' cannot have more than %d values",
						miditype, maxlists);
					}
					error = YES;
					break;
				}
				expecting = EXPECT_NUMBER;
			}
			else if (*string == 't' && *(string + 1) == 'o') {

				/* Duplicate last value downward as needed. */
				for (row++; row < maxlists && crvlists_p_p[row] != 0; row++) {
					add_to_row(crvlists_p_p, row, value);
				}
				(*numpoints_p)++;
				row = 0;
				expecting = EXPECT_NUMBER;
				string += 2;
			}
			else {
				if (maxlists == 1) {
					l_yyerror(filename, lineno,
						"expecting 'to'");
				}
				else {
					l_yyerror(filename, lineno,
						"expecting comma or 'to'");
				}
				error = YES;
				break;
			}
			break;

		default:
			pfatal("invalid state in make_vel_to_lists()");
			/*NOTREACHED*/
			break;
		}
	}

	if (expecting == EXPECT_NUMBER) {
		/* must have ended with "to" without following number */
		l_yyerror(filename, lineno, "expecting number");
		error = YES;
	}

	if (error == YES) {
		free_to_lists(crvlists_p_p, row);
		*numpoints_p = 0;
		return(0);
	}

	/* Expand last one downward if necessary. */
	for (row++; row < maxlists && crvlists_p_p[row] != 0; row++) {
		add_to_row(crvlists_p_p, row, value);
	}

	/* The array is pointing to the tails of the lists, which was done
	 * to make inserting quick and easy. But we want the returned array
	 * to point to the heads, so fix that.
	 */
	for (row = 0; row < maxlists && crvlists_p_p[row] != 0; row++) {
		crvlists_p_p[row] = get_head(crvlists_p_p[row]);
	}

	return(crvlists_p_p);
}


/* Create a new CRVLIST struct, fill in its y value with the given value
 * (x is left garbage for now since we don't know it yet),
 * and add it to the given row in the array of lists. The array
 * points to the *last* entry in the list, so we link to what it points to.
 */

static void
add_to_row(crvlists_p_p, row, y_value)

struct CRVLIST **crvlists_p_p; /* This point to the *tails* of the lists */
int row;			/* Which row to add to */
int y_value;			/* What y value to add */

{
	struct CRVLIST *item_p;

	MALLOC(CRVLIST, item_p, 1);
	item_p->y = (float) y_value;
	/* Note that x will be filled in later; we leave garbage for now. */
	item_p->next = 0;
	item_p->prev = crvlists_p_p[row];
	if (item_p->prev != 0) {
		item_p->prev->next = item_p;
	}
	crvlists_p_p[row] = item_p;
}


/* Given a pointer to the last element in a linked list of CRVLIST structs,
 * follow the prev links, and return a pointer to the first element.
 */

static struct CRVLIST *
get_head(crvlist_tail_p)

struct CRVLIST *crvlist_tail_p;

{
	struct CRVLIST *list_p;

	for (list_p = crvlist_tail_p; list_p->prev != 0;
						list_p = list_p->prev) {
		;
	}
	return(list_p);
}


/* Make the given row a clone of the row before it,
 * but stopping before the final entry, which the caller will add.
 */

static void
clone_row(crvlist_p_p, row)

struct CRVLIST **crvlist_p_p;	/* clone a row in this array */
int row;			/* duplicate this row from previous row */

{
	struct CRVLIST *list_p;		/* walk through row to be cloned */

	for (list_p = get_head(crvlist_p_p[row-1]); list_p->next != 0;
						list_p = list_p->next) {
		add_to_row(crvlist_p_p, row, list_p->y);
	}
}


/* Given an array of pointers to CRVLIST lists, free the lists and the array. */

static void
free_to_lists(to_lists_p_p, numlists)

struct CRVLIST **to_lists_p_p;	/* the array of lists to free */
int numlists;			/* how many array elements are used */

{
	int n;				/* index through lists */
	struct CRVLIST *list_p;		/* current item being deleted */
	struct CRVLIST *next_p;		/* saved value for list_p->next */


	for (n = 0; n < numlists; n++) {
		for (list_p = to_lists_p_p[n]; list_p != 0; list_p = next_p) {
			next_p = list_p->next;
			FREE(list_p);
		}
	}
	FREE(to_lists_p_p);
}


/*
 * Name:	fix_y_value()
 *
 * Abstract:	Adjust the given Y value to be the final result.
 *
 * Returns:	The adjusted value.
 *
 * Description:	This function enforces a minimum and maximum on the given value
 *		and then rounds to the nearest integer.  It assumes nonnegative
 *		input.
 */

static int
fix_y_value(y, min_limit, max_limit)

double y;		/* y value from formula */
int min_limit;		/* min y value allowed */
int max_limit;		/* max y value allowed */

{
	if (y <= min_limit) {
		return (min_limit);
	}
	if (y >= max_limit) {
		return (max_limit);
	}
	return ((short)(y + 0.5));
}


/*
 * Name:	interpolate_midi()
 *
 * Abstract:	Calculate MIDI values for when there is a "til" clause.
 *
 * Returns:	The total number of values, which includes the original values
 *		passed in plus the new interpolated values generated.
 *
 * Description:	This function is passed a curve list of two or more points,
 *		which actually represents not a curve, but rather values of a
 *		MIDI item.  The X values represent the time offset in
 *		beats, and the Y values are the item values at those times.
 *		The function mallocs two parallel arrays, for X and Y values
 *		of all of the points, original points and the additional
 *		intermediate points interpolated in between them.
 *		If keep_dups is NO, then only points when the value actually	 *		changed are returned.
 *		The values are clamped to the given min and max values,
 *		which must be non-negative.
 */

static int
interpolate_midi(begpoint_p, min_limit, max_limit, x_incr, keep_dups, x_p_p, y_p_p)

struct CRVLIST *begpoint_p;	/* first point of curve list of user's points */
int min_limit;			/* min y value allowed */
int max_limit;			/* max y value allowed */
double x_incr;			/* increment of x for creating new points */
int keep_dups;			/* if NO, collapse runs of same value */
float **x_p_p;			/* return malloc'ed array of all x values */
short **y_p_p;			/* return malloc'ed array of all y values */

{
	struct CRVLIST *crv_p;	/* point along the curve list */
	struct CRVLIST *endpoint_p; /* point at user's last point */
	int orig_count;		/* number of points in the curve list */
	int tot_count;		/* number of points we are returning */
	float x, y;		/* values for some point */
	float slope;		/* between two neighboring input points */
	float maxslope;		/* max absolute value of "slope" */
	float scale;		/* for temporarily scaling Y */
	short prevy;		/* previous Y value */
	short newy;		/* new Y value */
	int n;			/* loop variable */


	/*
	 * First we will find out how many points have been passed in, and how
	 * many we should return.  Also find the steepest slope between any two
	 * input points.
	 */

	orig_count = 1;		/* init to 1 to account for the last point */
	tot_count = 1;		/* init to 1 to account for the last point */

	/*
	 * The outer loop has one iteration for every user provided point
	 * except the last.  The inner loop has one iteration for the user
	 * point at its start and for each point we will generate.  The inner
	 * loop could be replaced by dividing the interval by the increment,
	 * but due to floating point roundoff we might get a different result
	 * from later on when we really do need to loop to find Y values.
	 */
	maxslope = 0.0;
	for (crv_p = begpoint_p; crv_p->next != 0; crv_p = crv_p->next) {

		orig_count++;

		/* keep track of the steepest slope */
		slope = fabs((crv_p->next->y - crv_p->y) /
			     (crv_p->next->x - crv_p->x));
		if (slope > maxslope) {
			maxslope = slope;
		}

		/*
		 * Stop a little short of the end to avoid landing almost on
		 * top of the next user point.
		 */
		for (x = crv_p->x; x < crv_p->next->x - x_incr / 10.0;
				x += x_incr) {
			tot_count++;
		}
	}

	endpoint_p = crv_p;

	/* allocate the arrays to be returned */
	MALLOCA(float, *x_p_p, tot_count);
	MALLOCA(short, *y_p_p, tot_count);

	prevy = -1;	/* no prev, so set prev Y to a value Y can never have */

	/*
	 * If there are only 2 user points, it's a special case.  We just do
	 * linear interpolation.
	 */
	if (orig_count == 2) {
		slope = (endpoint_p->y - begpoint_p->y) /
			(endpoint_p->x - begpoint_p->x);

		/*
		 * Calculate the Y value at every increment of X.  But if it
		 * rounds off to the same integer as before, don't create a
		 * point for it, unless special flag is set to keep dups.
		 */
		n = 0;
		for (x = begpoint_p->x; x < endpoint_p->x - x_incr / 10.0;
				x += x_incr) {

			/* Get float y value for this x */
			y = begpoint_p->y + slope * (x - begpoint_p->x);

			/* Get integer value for this y */
			newy = fix_y_value(y, min_limit, max_limit);

			/* If it changed from last time, save a point */
			if ( (keep_dups == YES) || (newy != prevy) ) {
				(*x_p_p)[n] = x;
				(*y_p_p)[n] = newy;
				prevy = newy;
				n++;
			}
		}

		/* Get the last point; store it if changed. */
		newy = fix_y_value(endpoint_p->y, min_limit, max_limit);
		if ( (keep_dups == YES) || (newy != prevy) ) {
			(*x_p_p)[n] = endpoint_p->x;
			(*y_p_p)[n] = newy;
			n++;
		}

		/*
		 * Return the total number of points, which may be less than
		 * the size of the arrays we allocated to hold them (when Y
		 * values repeated).
		 */
		return (n);
	}

	/*
	 * There were more than 2 input points, so treat as a curve.  We will
	 * use the same code as real curves do, calling curve_y_at_x.  But
	 * real curves are allowed to bend back on themselves.  For our case,
	 * X is time, and we want to prevent that.  The input points have
	 * increasing X values, but the curve might still bulge backwards.
	 *
	 * We can almost certainly prevent this by keeping the slope between
	 * neighboring user points small enough; namely, no more than 1.  If it
	 * is more than 1, we divide all Y values by that amount, which forces
	 * that limit; and after running curve_y_at_x we multiply by that value
	 * to restore the correct values.
	 */
	if (maxslope <= 1.0) {
		scale = 1.0;
	} else {
		scale = maxslope;
		for (crv_p = begpoint_p; crv_p != 0;
				crv_p = crv_p->next) {
			crv_p->y /= scale;
		}
	}

	/* Loop as before through all X values, calculating Y. */
	n = 0;
	for (crv_p = begpoint_p; crv_p->next != 0; crv_p = crv_p->next) {
		for (x = crv_p->x; x < crv_p->next->x - x_incr / 10.0;
				x += x_incr) {

			/* Get float y value for this x */
			y = scale * curve_y_at_x(begpoint_p, x);

			/* Get integer value for this y */
			newy = fix_y_value(y, min_limit, max_limit);

			/* Save a point, unless it is to be suppressed
			 * because it is the same as the previous. */
			if ( (keep_dups == YES) || (newy != prevy) ) {
				(*x_p_p)[n] = x;
				(*y_p_p)[n] = newy;
				prevy = newy;
				n++;
			}
		}
	}

	/* Get the last point, store it if appropriate. */
	newy = fix_y_value(scale * crv_p->y, min_limit, max_limit);

	if ( (keep_dups == YES) || (newy != prevy) ) {
		(*x_p_p)[n] = crv_p->x;
		(*y_p_p)[n] = newy;
		n++;
	}

	/*
	 * Return the total number of points, which may be less than the size
	 * of the arrays we allocated to hold them (when Y values repeated).
	 */
	return (n);
}
