
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

/* This file contains functions called during the Mup parse phase,
 * that are related to STUFF structs.
 */


#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"

/* if user specifies a "til" clause on stuff with a number of measures > 0,
 * we need to save away info about where the til clause will end, to make sure
 * that it doesn't fall off the end of the measure or the piece. This is the
 * struct we use to save this info. */
static struct TIL_INFO {
	char	*inputfile;	/* where STUFF was defined */
	int	inputlineno;	/* where STUFF was defined */
	int	measnum;	/* number of measure in which til clause ends */
	float	count;		/* count in measure where til clause ends */
	struct TIL_INFO *next;	/* for linked list */
} *Til_info_list_p;

/* info about the STUFF currently being collected from input */
static int Curr_stuff_type;	/* ST_* */
static int Stuff_size;		/* point size of stuff text string */
static int Modifier;		/* TM_* for text, L_* for phrase */
static int Measnum = 1;		/* to check til clauses. Can't use Meas_num
				 * global because it doesn't count invisible
				 * bars but til clauses do */
static int Multi_adjust;	/* adjustment to Measnum to account
				 * for multirests */

/* head and tail of list of STUFF currently being collected from input */
static struct STUFF *Head_stufflist_p;
static struct STUFF *Tail_stufflist_p;

/* current pedal state for each staff. YES if in the middle of doing pedal,
 * NO if not. */
static short Pedal_state[MAXSTAFFS + 1];
static char *Ped_begin_str;	/* will point to "\(begped)" */
static char *Ped_up_down_str;	/* will point to "\(pedal)" */

/* static functions */
static struct STUFF *clone_stufflist P((struct STUFF *stufflist_p,
			int staffno, int all));
static char *get_stuff_string P((char *string, int stuff_type, int all,
			int staffno, char *inputfile, int inputlineno));
static void do_attach P((int staffno, int all, struct RANGELIST *vno_range_p));
static void midi_attach P((int staffno, struct STAFF *staff_p,
			struct RANGELIST *vno_range_p, int all));
static void free_stufflist P((struct STUFF *stuff_P));
static void free_tils P((struct TIL_INFO *til_p));
static void fix_pedal P((int staffno, struct STUFF *stuff_p));
static void ped_order_chk P((void));
static void voice_phrases P((struct MAINLL *mll_p, int vindex));
static void find_eph P((struct MAINLL *mll_p, struct GRPSYL *gs_p, int vindex,
		int place));
static void mk_phrase_stuff P((struct MAINLL *mll_p,
		struct GRPSYL *begin_gs_p, struct MAINLL *end_mll_p,
		int bars, struct GRPSYL *end_gs_p, int end_ts_den, int place));


/* save current stuff type value. Also check that we are in data (music)
 * context */

void
set_stuff_type(stuff_type)

int stuff_type;

{
	Curr_stuff_type = stuff_type;

	(void) contextcheck(C_MUSIC, "statement");
}


/* return current stuff type */

int
get_stuff_type()

{
	return(Curr_stuff_type);
}


/* check all the things in an input line of stuff, up to the colon,
 * for consistency, and save interesting info away for later use. */

void
chk_stuff_header(size, modifier, place, dist_usage)

int size;		/* point size, or -1 if to use default */
int modifier;		/* TM_* for text, L_* for phrase */
int place;		/* PL_* */
int dist_usage;		/* SD_* */

{

	debug(4, "chk_stuff_header");

	switch (Curr_stuff_type) {
	case ST_ROM:
	case ST_BOLD:
	case ST_ITAL:
	case ST_BOLDITAL:
		break;
	case ST_MUSSYM:
		if (modifier != TM_NONE) {
			l_warning(Curr_filename, yylineno,
					"can't specify %s except with a font; ignoring",
					stuff_modifier(modifier));
			modifier = TM_NONE;
		}
		break;
	case ST_PEDAL:
		if (place != PL_BELOW && place != PL_UNKNOWN) {
			yyerror("pedal must be below");
		}
		/*FALLTHRU*/
	default:
		if (size != -1) {
			yyerror("can't specify size except with a font or mussym");
		}
		if (modifier != TM_NONE && Curr_stuff_type != ST_PHRASE) {
			l_warning(Curr_filename, yylineno,
					"can't specify %s except with a font; ignoring",
					stuff_modifier(modifier));
		}
		if (Curr_stuff_type == ST_PHRASE && modifier != L_NORMAL &&
				modifier != L_DOTTED && modifier != L_DASHED) {
			l_yyerror(Curr_filename, yylineno,
					"only dotted or dashed line type can be specified for phrase");
		}
		break;
	}

	if (Curr_stuff_type == ST_OCTAVE) {
		if (is_tab_range() == YES) {
			yyerror("octave not allowed on tablature staff");
		}
		else if(place == PL_BETWEEN) {
			yyerror("octave must be above or below");
			place = PL_ABOVE;
		}
	}

	if (Curr_stuff_type == ST_PHRASE && place == PL_BETWEEN) {
		yyerror("phrase must be above, below, or omitted");
		place = PL_ABOVE;
	}

	if (dist_usage != SD_NONE) {
		if (Curr_stuff_type == ST_PHRASE) {
			yyerror("dist not allowed on phrase");
		}
		else if (Curr_stuff_type == ST_MIDI) {
			yyerror("dist not allowed on midi");
		}

		if (place == PL_BETWEEN) {
			yyerror("dist not allowed with 'between'");
		}
	}

	/* Save the modifier value.
	 * Have to set this before calling dflt_place() */
	Modifier = modifier;

	/* fill in default values if user didn't specify */
	if (place == PL_UNKNOWN) {
		place = dflt_place();
	}

	Stuff_size = size;
	Place = (short) place;

	/* make sure current list of stuff is empty */
	Head_stufflist_p = Tail_stufflist_p = (struct STUFF *) 0;
}


/* return default value for place depending on value of Curr_stuff_type */

int
dflt_place()

{
	switch (Curr_stuff_type) {

	case ST_PEDAL:
		return(PL_BELOW);

	case ST_OCTAVE:
		yyerror("must specify above or below with octave");
		/* arbitrarily return above. If we leave it as unknown,
		 * we can get double error messages in some cases */
		return(PL_ABOVE);

	case ST_PHRASE:
		/* stays unknown at this point */
		return(PL_UNKNOWN);

	/* lyrics aren't stuff, but they can come through here,
	 * so we handle them specially */
	case -1:
		return(PL_BELOW);

	default:
		if (Modifier == TM_ANALYSIS || Modifier == TM_FIGBASS) {
			return(PL_BELOW);
		}
		/* default for everything else is above */
		return(PL_ABOVE);
	}
}


/* check a "stuff" item  and add to list */

void
add_stuff_item(start_count, start_steps, start_gracebackup, string, bars, count,
		end_steps, end_gracebackup, dist, dist_usage, aligntag,
		grid_label)

double start_count;		/* where in measure to start this stuff */
double start_steps;		/* offset by this many stepsizes */
int start_gracebackup;	/* how many grace notes to back up from start */
char *string;		/* what to print */
int bars;		/* how many bar lines to cross with this stuff */
double count;		/* how many beats into last measure */
double end_steps;	/* stepsizes to add to end (can be negative) */
int end_gracebackup;	/* how many grace notes to back up from end */
double dist;		/* dist for this specific STUFF, to override param */
int dist_usage;		/* meaning of dist, SD_*  */
int aligntag;		/* tag number for aligning STUFFS across score */
char *grid_label;	/* optional label to use in place of real chord name */

{
	struct STUFF *new_p;		/* where to store STUFF */
	struct TIL_INFO *til_info_p;	/* to save info about til clause */
	int len;			/* length of stuff text string */
	char *padded_string;		/* string with 1-space padding at end */
	char lch;			/* last character of string */


	if (bars != 0 || count != 0.0) {
		/* has a "til" clause. Check if that is valid */
		if (Curr_stuff_type == ST_MUSSYM) {
			if (string == (char *) 0) {
				yyerror("missing string");
				return;
			}

			/* not yet changed to internal form, need to compare
			 * in ASCII form */
			if ((strcmp(string + 2, "tr") != 0) &&
					(strcmp(string + 2, "\\(tr)") != 0) &&
					(strcmp(string + 2, "smtr") != 0) &&
					(strcmp(string + 2, "\\(smtr)") != 0)) {
				yyerror("til not allowed on mussym except on trills");
			}
		}

		else if (Curr_stuff_type == ST_PEDAL) {
			yyerror("til not allowed on pedal");
		}

		if (Curr_stuff_type != ST_PHRASE &&
			(Modifier == TM_CHORD || Modifier == TM_ANALYSIS) ) {
			l_yyerror(Curr_filename, yylineno,
					"til not allowed with %s",
					stuff_modifier(Modifier));
		}

		if (bars == 0) {
			if (count > Score.timenum + 1) {
				yyerror("'til' value must be <= numerator of time signature + 1");
			}

			if (count < start_count) {
				yyerror("til value must be >= start value");
			}
		}

	}
	else {
		/* doesn't have a "til" clause. Check if one is required */
		if (Curr_stuff_type == ST_CRESC ||
						Curr_stuff_type == ST_DECRESC) {
			yyerror("til required on cresc/decresc");
		}
	}
	
	if (start_count > Score.timenum + 1) {
		yyerror("beat offset must be <= numerator of time signature + 1");
	}

	if (Curr_stuff_type == ST_CRESC || Curr_stuff_type == ST_DECRESC) {
		if (string != (char *) 0) {
			yyerror("string not allowed with cresc/decresc");
		}
		Modifier = TM_DYN;
	}

	else if (Curr_stuff_type == ST_PHRASE) {
		if (string != (char *) 0) {
			yyerror("string not allowed with phrase");
		}
	}

	else if (Curr_stuff_type == ST_PEDAL) {
		if ( (string != (char *) 0)
				&& (strcmp(string + 2, "\\(endped)") != 0) ) {
			yyerror("pedal string must be either blank or *");
		}
	}

	else {
		if (string == (char *) 0) {
			yyerror("string is required");
			return;
		}
	}

	if ( (start_gracebackup != 0 || end_gracebackup != 0)
						 && Place == PL_BETWEEN) {
		yyerror("grace backup not allowed with 'between'");
	}

	if (aligntag != NOALIGNTAG && ! IS_TEXT(Curr_stuff_type)
				&& Curr_stuff_type != ST_CRESC
				&& Curr_stuff_type != ST_DECRESC) {
		if (Curr_stuff_type == ST_PEDAL) {
			/* User specified aligned on pedal is allowed only
			 * if alignped is set to n. But it could be set
			 * differently on different staffs. So we check
			 * all staffs this STUFF applies to, and if any of
			 * them have alignped=y, then we issue warning.
			 */
			struct SVRANGELIST *svr_p;
			struct RANGELIST *r_p;
			short staffno;
			int align_allowed = YES; /* assume ok til proven not */

			for (svr_p = Svrangelist_p; svr_p != 0;
						svr_p = svr_p->next) {
				for (r_p = svr_p->stafflist_p; r_p != 0;
						r_p = r_p->next) {
					for (staffno = r_p->begin;
							staffno <= r_p->end
							&& staffno <= MAXSTAFFS;
							staffno++) {
						if (svpath(staffno, ALIGNPED)->alignped == YES) {
							align_allowed = NO;
							break;
						}
					}
				}
			}
			if (align_allowed == NO) {
				l_warning(Curr_filename, yylineno,
				"align is only effective on pedal if alignped is n");
			}
		}
		else {
			l_warning(Curr_filename, yylineno,
			"align is only effective on rom, bold, ital, boldital, < and > (and pedal if alignped=n)");
		}
	}
	/* Phrase marks associate with a particular group,
	 * so step offset doesn't make a lot of sense,
	 * so warn and ignore if we get one */
	if ( (start_steps != 0.0 || end_steps != 0.0)
					 && Curr_stuff_type == ST_PHRASE) {
		l_warning(Curr_filename, yylineno, "step offset ignored on phrase mark");
		start_steps = 0.0;
		end_steps = 0.0;
	}

	switch (Curr_stuff_type) {
	case ST_ROM:
	case ST_BOLD:
	case ST_ITAL:
	case ST_BOLDITAL:
		/* the text-type stuffs are supposed to have a 1-space padding
		 * at the end of them */
		if (bars != 0 || count != 0.0) {
			/* don't add padding if has wavy or solid line
			 * til clause */
			lch = last_char(string);
			if (lch == '~' || lch == '_') {
				break;
			}
		}
		string = pad_string(string, Modifier);
		break;

	case ST_MUSSYM:
		/* in mussym, user can specify things without the usual
		 * \(---) convention. Change to include them */
		if (string[2] == '\\' && string[3] == '(') {
			/* if user unnecessarily put in the \(--), leave it */
			break;
		}

		len = strlen(string + 2);
		MALLOCA(char, padded_string, len + 6);
		(void) sprintf(padded_string, "%c%c\\(%s)", FONT_TR, DFLT_SIZE,
						string + 2);
		FREE(string);
		string = padded_string;
		break;

	default:
		break;
	}

	/* fill in a new STUFF struct with appropriate info */
	new_p = newSTUFF(string, dist, dist_usage, aligntag, start_count, start_steps,
		start_gracebackup, bars, count, end_steps, end_gracebackup,
		Curr_stuff_type, Modifier, Place, Curr_filename, yylineno);

	if (Modifier == TM_CHORD) {
		if (grid_label == 0) {
			/* In the most common case, the label to print and
			 * the actual grid name will be the same, but need to
			 * make a copy rather than point both
			 * to the same string, so that free_stufflist()
			 * won't attempt to free the same string twice.
			 */
			new_p->grid_name = strdup(new_p->string);
		}
		else {
			/* User wants a custom label for this particular
			 * chord instance, so move what would have been
			 * the default label to be just the grid name,
			 * and set the label to the custom value,
			 * appropriately padded, like the other was.
			 */
			new_p->grid_name = new_p->string;
			new_p->string = pad_string(grid_label, Modifier);
		}
	}
	else if (grid_label != 0) {
		l_yyerror(Curr_filename, yylineno,
				"alternate grid label only allowed with 'chord'");
	} 

	/* if bars > 0, need to save away til info for later error
	 * checking */
	if (bars > 0) {
		CALLOC(TIL_INFO, til_info_p, 1);
		til_info_p->measnum = Measnum + bars;
		til_info_p->count = count;
		til_info_p->inputfile = new_p->inputfile;
		til_info_p->inputlineno = new_p->inputlineno;
		til_info_p->next = Til_info_list_p;
		Til_info_list_p = til_info_p;
	}

	/* above/between go on the head of the list, below goes on the
	 * tail of the list, so that things come out in the right order.
	 * Midi always goes at the end */
	if (Place == PL_BELOW || Curr_stuff_type == ST_MIDI) {
		/* link onto list tail */
		if ( Tail_stufflist_p == (struct STUFF *) 0) {
			Head_stufflist_p = new_p;
		}
		else {
			Tail_stufflist_p->next = new_p;
		}
		Tail_stufflist_p = new_p;
	}
	else {
		/* link onto head of list */
		new_p->next = Head_stufflist_p;
		Head_stufflist_p = new_p;
		if (Tail_stufflist_p == (struct STUFF *) 0) {
			Tail_stufflist_p = new_p;
		}
	}
}


/* return YES if given string consists entirely of the specific music symbol */
/* the string should be in the internal format of font/size/string */

int
string_is_sym(string, sym, symfont)

char *string;	/* which string to check */
int sym;	/* check for this music symbol */
int symfont;	/* FONT_MUSIC*  */

{
	int font, size;


	if (string == (char *) 0) {
		return(NO);
	}

	font = *string++;
	size = *string++;
	if (next_str_char(&string, &font, &size) != sym) {
		return(NO);
	}
	if (font != symfont) {
		return(NO);
	}
	if (next_str_char(&string, &font, &size)  == '\0') {
		return(YES);
	}
	return (NO);
}


/* connect a list of STUFF to a STAFF. If there is already something on
 * that STAFF's STUFF list, attach at the end or beginning as appropriate
 * depending on place. */

void
attach_stuff()

{
	struct SVRANGELIST *svr_p;	/* to walk through Svrangelist */
	struct RANGELIST *r_p;		/* to walk through staff range list */
	short staffno;


	debug(4, "attach_stuff");

	/* make sure we've got STAFF structs for this measure */
	create_staffs();

	for (svr_p = Svrangelist_p; svr_p != (struct SVRANGELIST *) 0;
						svr_p = svr_p->next) {
		for (r_p = svr_p->stafflist_p; r_p != (struct RANGELIST *) 0;
						r_p = r_p->next) {
	
			for (staffno = r_p->begin; staffno <= r_p->end
					&& staffno <= MAXSTAFFS; staffno++) {
				do_attach(staffno, r_p->all, svr_p->vnolist_p);

				if (Place == PL_BETWEEN) {
					/* between has 2 staffs in its range,
					 * but stuff is only associated
					 * with the top staff */
					break;
				}
			}
		}
	}

	free_rlists();

	/* have made copies of stuff for each staff that gets one, with
	 * the proper font/size etc, so need to free master stufflist copy */
	free_stufflist(Head_stufflist_p);
}


/* Attach STUFF for a specific staff. */

static void
do_attach(staffno, all, vno_range_p)

int staffno;
int all;
struct RANGELIST *vno_range_p;

{
	struct STAFF *staff_p;		/* where to attach STUFF */
	struct STUFF *stufflist_p;	/* current copy of STUFF list */


	if (staffno > Score.staffs) {
		l_yyerror(Head_stufflist_p->inputfile,
				Head_stufflist_p->inputlineno,
				"staff number out of range");
		return;
	}

	staff_p = Staffmap_p[staffno]->u.staff_p;

	if (Place == PL_BETWEEN) {
		if (staffno + 1 > Score.staffs) {
			/* will have already exclaimed about
			 * this error before, so no need to print message,
			 * but better skip next check */
			return;
		}

		/* if either staff of a between is invisible,
		 * throw this stuff away */
		if (svpath(staffno, VISIBLE)->visible == NO ||
				svpath(staffno + 1,
				VISIBLE)->visible == NO) {
			return;
		}
	}

	/* handle MIDI stuff specially */
	if (Curr_stuff_type == ST_MIDI) {
		if (all == YES) {
			/* need to find top visible staff/voice to attach to */
			int s;		/* staff number */
			int v;		/* voice number */
			struct RANGELIST range;
	
			v = 1;	/* avoid bogus "used before set" warning */
			for (s = 1; s <= MAXSTAFFS; s++) {
				if (svpath(s, VISIBLE)->visible == YES) {
					for (v = 1; v <= MAXVOICES; v++) {
						if (vvpath(s, v, VISIBLE)->visible == YES) {
							break;
						}
					}
					if (v <= MAXVOICES) {
						break;
					}
				}
			}
			if (s > MAXSTAFFS || v > MAXVOICES) {
				pfatal("failed to find top visible staff/voice");
			}
			/* make a special RANGELIST for this */
			range.begin = range.end = v;
			range.all = YES;
			range.next = 0;
			midi_attach(s, Staffmap_p[s]->u.staff_p, &range, all);
		}
		else {
			midi_attach(staffno, staff_p, vno_range_p, all);
		}
	}

	else {
		/* make the copy for this staff from master copy */
		stufflist_p = clone_stufflist(Head_stufflist_p, staffno, all);

		if (Curr_stuff_type == ST_PEDAL) {
			fix_pedal(staffno, stufflist_p);
		}

		connect_stuff(staff_p, stufflist_p);
	}
}


/* attach MIDI stuff. This is slightly different than other stuff because
 * it can be applied to one or both voices. */

static void
midi_attach(staffno, staff_p, vno_range_p, all)

int staffno;		/* attach to this staff number */
struct STAFF *staff_p;	/* attach to this staff struct */
struct RANGELIST *vno_range_p;
int all;		/* if associated with "all" */

{
	struct RANGELIST *r_p;		/* walk through vno_range_p */
	int vno;			/* voice number */
	struct STUFF *stufflist_p;	/* copy of stuff */
	struct STUFF *st_p;		/* walk through stufflist_p */
	short place;


	/* do for each voice that MIDI stuff applies to */
	for (r_p = vno_range_p; r_p != (struct RANGELIST *) 0; r_p = r_p->next) {
		for (vno = r_p->begin; vno <= r_p->end; vno++) {

			/* make the copy for this staff from master copy */
			stufflist_p = clone_stufflist(Head_stufflist_p,
						staffno, all);

			/* fix up place based on voice number */
			switch (vno) {
			case 1:
				place = PL_ABOVE;
				break;
			case 2:
				place = PL_BELOW;
				break;
			case 3:
				place = PL_BETWEEN;
				break;
			default:
				pfatal("illegal vno for midi");
				/*NOTREACHED*/
				place = PL_UNKNOWN;  /* avoid "used before set" warning */
				break;
			}
			for (st_p = stufflist_p; st_p != (struct STUFF *) 0;
						st_p = st_p->next) {
				st_p->place = place;
			}

			connect_stuff(staff_p, stufflist_p);
		}
	}
}


/* connect a new stuff list into an existing stuff list. Add below stuff and
 * MIDI stuff to the end of the list,
 * and others to beginning of list, but make sure any
 * "above all" comes after any above non-all, and that any below non-all
 * comes before any "below all."
 */

void
connect_stuff(staff_p, stufflist_p)

struct STAFF *staff_p;		/* connect to stuff off of this staff */
struct STUFF *stufflist_p;	/* connect this list  of stuff */

{
	struct STUFF *st_p;		/* to find link place in STUFF list */
	struct STUFF *s_p;		/* to find end of stufflist_p */
	struct STUFF **ins_p_p;		/* where to insert in list */


	if (staff_p == (struct STAFF *) 0 || stufflist_p == (struct STUFF *) 0) {
		return;
	}

	if (staff_p->stuff_p == (struct STUFF *) 0) {
		/* no list before, so attach this one
		 * directly to STAFF */
		staff_p->stuff_p = stufflist_p;
	}

	else if (Place == PL_BELOW || stufflist_p->stuff_type == ST_MIDI) {
		/* if this set of stuff isn't associated with
		 * "all", then it goes before any below "all" stuff */
		if (stufflist_p->all == NO) {
			for (ins_p_p = &(staff_p->stuff_p); 
					*ins_p_p != (struct STUFF *) 0;
					ins_p_p = &((*ins_p_p)->next)) {
				if ( (*ins_p_p)->place == PL_BELOW &&
						(*ins_p_p)->all == YES) {
					break;
				}
			}
			/* find end of list to be inserted */
	 		for (s_p = stufflist_p; s_p->next != (struct STUFF *) 0;
							s_p = s_p->next) {
		    		;
			}

			/* insert */
			s_p->next = *ins_p_p;
			*ins_p_p = stufflist_p;
		}

		else {
			/* goes at end of list. find the end */
			for (st_p = staff_p->stuff_p;
					st_p->next != (struct STUFF *)0;
					st_p = st_p->next) {
				;
			}

			/* connect in the new list */
			st_p->next = stufflist_p;
		}
	}
	else {
		/* find end of new list */
		for (s_p = stufflist_p;
				s_p->next != (struct STUFF *) 0;
				s_p = s_p->next) {
			;
		}

		if (stufflist_p->all == NO) {
			/* goes at the head of the list */
			s_p->next = staff_p->stuff_p;
			staff_p->stuff_p = stufflist_p;
		}
		else {
			/* goes before any existing above all */
			for (ins_p_p = &(staff_p->stuff_p); 
					*ins_p_p != (struct STUFF *) 0;
					ins_p_p = &((*ins_p_p)->next)) {
				if ( (*ins_p_p)->place == PL_ABOVE &&
						(*ins_p_p)->all == YES) {
					break;
				}
			}
			/* find end of list to be inserted */
	 		for (s_p = stufflist_p; s_p->next != (struct STUFF *) 0;
							s_p = s_p->next) {
		    		;
			}

			/* insert */
			s_p->next = *ins_p_p;
			*ins_p_p = stufflist_p;
		}
	}
}


/* given a list of STUFF, return a clone of the list */

static struct STUFF *
clone_stufflist(stufflist_p, staffno, all)

struct STUFF *stufflist_p;	/* what stuff to clone */
int staffno;			/* which staff, to get proper point size */
int all;			/* YES if was "above all" or "below all" */

{
	struct STUFF *new_p;	/* copy of STUFF */
	char *newstring;	/* copy of text string */


	if (stufflist_p == (struct STUFF *) 0) {
		return( (struct STUFF *) 0 );
	}

	/* make copy of string with appropriate font and size */
	if (stufflist_p->string != (char *) 0) {
		newstring = get_stuff_string(stufflist_p->string,
				stufflist_p->stuff_type,
				all, staffno,
				stufflist_p->inputfile,
				stufflist_p->inputlineno);
	}
	else {
		newstring = (char *) 0;
	}
	
	/* create and fill in clone of stuff, then return it */
	new_p = newSTUFF(newstring, stufflist_p->dist,
				stufflist_p->dist_usage,
				stufflist_p->aligntag,
				stufflist_p->start.count,
				stufflist_p->start.steps,
				stufflist_p->start.gracebackup,
				stufflist_p->end.bars, stufflist_p->end.count,
				stufflist_p->end.steps,
				stufflist_p->end.gracebackup,
				stufflist_p->stuff_type, stufflist_p->modifier,
				stufflist_p->place, stufflist_p->inputfile,
				stufflist_p->inputlineno);
	new_p->all = (short) all;
	if (stufflist_p->grid_name != 0) {
		new_p->grid_name = get_stuff_string(stufflist_p->grid_name,
				stufflist_p->stuff_type,
				all, staffno,
				stufflist_p->inputfile,
				stufflist_p->inputlineno);
	}
	new_p->next = clone_stufflist(stufflist_p->next, staffno, all);
	return(new_p);
}


/* Make and return a copy of a STUFF string, adjusting the font/size for
 * the staff, and adjusting chord strings as they need to be.
 */

static char *
get_stuff_string(string, stuff_type, all, staffno, inputfile, inputlineno)

char *string;		/* the string to make a copy of */
int stuff_type;		/* ST_* */
int all;		/* YES or NO */
int staffno;
char *inputfile;
int inputlineno;

{
	int font;
	int fontfamily;
	int size;
	char *newstring;

	switch(stuff_type) {
	case ST_BOLD:
		font = FONT_TB;
		break;
	case ST_OCTAVE:
		Stuff_size = DFLT_SIZE;
		font = FONT_TI;
		break;
	case ST_ITAL:
		font = FONT_TI;
		break;
	case ST_BOLDITAL:
		font = FONT_TX;
		break;
	default:
		font = FONT_TR;
		break;
	}

	/* figure out the proper size if not already determined */
	if (Stuff_size  < 0) {
		if (all == YES) {
			size = Score.size;
		}
		else {
			size = svpath(staffno, SIZE)->size;
		}
	}
	else {
		size = Stuff_size;
	}

	/* determine fontfamily and font if not already known */
	if (Curr_family == FAMILY_DFLT) {
		if (all == YES) {
			fontfamily = Score.fontfamily;
		}
		else {
			fontfamily = svpath(staffno, FONTFAMILY)->
						fontfamily;
		}
	}
	else {
		fontfamily = Curr_family;
	}

	/* clone text string */
	newstring = copy_string(string + 2, font, size);
	if (IS_CHORDLIKE(Modifier)) {
		newstring = modify_chstr(newstring, Modifier);
	}
	fix_string(newstring, fontfamily + font, size,
		inputfile, inputlineno);
	if (Modifier == TM_FIGBASS || Modifier == TM_ANALYSIS) {
		newstring = acc_trans(newstring);
	}
	return(newstring);
}


/* allocate a STUFF and fill in all the values given. Initialize carry fields
 * and "all" to NO. Leave coordinates and next link as 0.
 * Note that the string pointer
 * is copied; it does not make a copy of the string itself, so never call this
 * function more than once with the same string--make a copy. */

struct STUFF *
newSTUFF(string, dist, dist_usage, aligntag,
		start_count, start_steps, start_gracebackup,
		bars, count, end_steps, end_gracebackup,
		stuff_type, modifier, place, inputfile, inputlineno)

char *string;		/* text string of stuff */
double dist;		/* dist for this STUFF to override dist parameter */
int dist_usage;		/* meaning of dist, SD_* */
int aligntag;		/* tag for aligning STUFF vertically */
double start_count;	/* count at which to begin stuff */
double start_steps;	/* offset by this many steps */
int start_gracebackup;	/* how many grace notes to back up from start */
int bars;		/* bars in "til" clasue */
double count;		/* counts in "til" clause */
double end_steps;	/* in the "til" clause */
int end_gracebackup;	/* how many grace notes to back up from end */
int stuff_type;		/* ST_* */
int modifier;		/* TM_* */
int place;		/* PL_* */
char *inputfile;	/* which file stuff was defined in */
int inputlineno;	/* where stuff was defined in input file */

{
	struct STUFF *new_p;	/* the new STUFF to fill in */


	CALLOC(STUFF, new_p, 1);
	new_p->string = string;
	new_p->grid_name = 0;
	new_p->start.count = start_count;
	new_p->start.steps = start_steps;
	new_p->start.gracebackup = (short) start_gracebackup;
	new_p->dist = (float) dist;
	new_p->dist_usage = (short) dist_usage;
	new_p->aligntag = aligntag;
	new_p->horzscale = DEFHORZSCALE;
	new_p->end.bars = (short) bars;
	new_p->end.count = count;
	new_p->end.steps = end_steps;
	new_p->end.gracebackup = end_gracebackup;
	new_p->stuff_type = (short) stuff_type;
	new_p->modifier = (short) modifier;
	new_p->place = (short) place;
	new_p->carryin = new_p->carryout = new_p->all = NO;
	new_p->costuff_p = 0;
	new_p->inputfile = inputfile;
	new_p->inputlineno = (short) inputlineno;

	return(new_p);
}


/* recursively free up a stufflist and any strings hanging off of it */

static void
free_stufflist(stuff_p)

struct STUFF *stuff_p;

{
	if (stuff_p == (struct STUFF *) 0 ) {
		return;
	}

	free_stufflist(stuff_p->next);
	if (stuff_p->string != (char *) 0) {
		FREE(stuff_p->string);
	}
	if (stuff_p->grid_name != (char *)0) {
		FREE(stuff_p->grid_name);
	}
	FREE(stuff_p);
}


/* at each bar line, see if there are any "til" clauses that are supposed
 * to end in this measure. If so, make sure they end within the time
 * signature for this measure. */

void
meas_stuff_chk()

{
	struct TIL_INFO *til_info_p;		/* to index thru list */
	struct TIL_INFO **del_place_p_p;	/* for deleting from list */
	struct TIL_INFO *one2free_p;		/* pointer to which element
						 * to free */

	debug(2, "meas_chk_stuff");

	/* update measure number to conpensate for any multirests */
	Measnum += Multi_adjust;
	Multi_adjust = 0;

	/* go through list of in-progress til clauses */
	for (til_info_p = Til_info_list_p, del_place_p_p = &Til_info_list_p;
				til_info_p != (struct TIL_INFO *) 0;  ) {

		if (til_info_p->measnum == Measnum) {

			/* at measure where this til clause ends */
			/* check if within time signature */
			if (til_info_p->count > Score.timenum + 1.0) {
				l_yyerror(til_info_p->inputfile,
					til_info_p->inputlineno,
					"beats in 'til' clause must be <= numerator of time signature + 1 of the measure in which the 'til' clause ends (i.e., <= %d)",
					Score.timenum);
			}

			/* this one has been taken care of: delete from list */
			*del_place_p_p = til_info_p->next;
			one2free_p = til_info_p;
		}
		else if (til_info_p->measnum < Measnum) {
			/* must have ended inside a multirest, so delete
 			 * from list */
			*del_place_p_p = til_info_p->next;
			one2free_p = til_info_p;
		}
		else {
			/* this one stays on the list for now, so move pointer
			 * to where to potentially delete to next element */
			del_place_p_p = &(til_info_p->next);
			one2free_p = (struct TIL_INFO *) 0;
		}

		/* have to move to next element
		 * before freeing the current one */
		til_info_p = til_info_p->next;

		if (one2free_p != (struct TIL_INFO *) 0) {
			FREE(one2free_p);
		}
	}

	/* update number of measures. */
	Measnum++;

	/* make sure pedal marks are in proper order */
	ped_order_chk();
}


/* adjust number of measures to account for multirests. Called when there is
 * a multirest. Saved the number of measures in the multirest (minus 1 since
 * the barline at the end will count for one measure) */

void
multi_stuff(nmeas)

int nmeas;	/* number of measures in multirest */

{
	/* subtract 1 to account for the fact that at the bar line at the
	 * end of the multirest we will peg the measure counter */
	Multi_adjust = nmeas - 1;
}


/* handle pedal going into endings. When we hit a first ending, save the
 * state of the pedal for all staffs. On subsequent endings in the set,
 * reset the pedal state to what it was at the beginning of the first ending.
 * At the endending, go back to normal operation. This is similar to
 * the saveped() function used at print time. */

void
ped_endings(endingloc)

int endingloc;		/* STARTITEM, INITEM, etc */

{
	register int s;		/* staff index */


	if (endingloc == STARTITEM) {
		if (Ped_snapshot[0] == YES) {

			/* starting 2nd ending: restore pedal state as it was
			 * at beginning of first ending */
			for (s = 1; s <= MAXSTAFFS; s++) {
				Pedal_state[s] = Ped_snapshot[s];
			}
		}

		else {
			/* starting a set of endings,
			 * need to save pedal state at this
			 * point so we can carry it into subsequent endings */
			for (s = 1; s <= Score.staffs; s++) {
				Ped_snapshot[s] = Pedal_state[s];
			}
			/* make sure any remaining staffs are set to pedal off,
			 * in case user increases the number of staffs
			 * during the endings... */
			for (   ; s <= MAXSTAFFS; s++) {
				Ped_snapshot[s] = NO;
			}

			/* mark that we now have a snapshot */
			Ped_snapshot[0] = YES;
		}
	}

	else if (endingloc == ENDITEM) {
		/* at end of endings, discard snapshot of pedal states */
		Ped_snapshot[0] = NO;
	}
}


/* When all input has been processed, or when changing the number
 * of staffs, we better not have any 'til' clauses
 * still unfinished. If we do, print a warning message. */

void
chk4dangling_til_clauses(boundary_desc)

char *boundary_desc;		/* "the end of the song" or
				 * "a change in number of staffs" */

{
	struct TIL_INFO *til_info_p;


	debug(2, "chk4dangling_til_clauses");

	/* Go through the whole list of remaining til clauses,
	 * and print a warning message for each. */
	for (til_info_p = Til_info_list_p; til_info_p != (struct TIL_INFO *) 0;
					til_info_p = til_info_p->next) {

		/* If right on the boundary or spills over only a very tiny
		 * amount, don't bother to complain */
		if (til_info_p->measnum - Measnum == 0
						&& til_info_p->count < .001) {
			continue;
		}

		l_warning(til_info_p->inputfile, til_info_p->inputlineno,
				"'til' clause extends beyond %s by %dm + %.3f",
				boundary_desc, til_info_p->measnum - Measnum,
				til_info_p->count);
	}

	/* mop up. */
	free_tils(Til_info_list_p);
	Til_info_list_p = (struct TIL_INFO *) 0;
}


/* recursively free a list of TIL_INFO structs */

static void
free_tils(til_p)

struct TIL_INFO *til_p;		/* free this list */

{
	if (til_p == (struct TIL_INFO *) 0) {
		return;
	}

	free_tils(til_p->next);
	FREE(til_p);
}


/* user only has to specify when pedal marks end. We deduce from current
 * pedal state whether a pedal mark is begin or up/down. This gets called
 * whenever we have a list of pedal STUFFs. Later we enforce that pedal
 * marks are put in in ascending order only, so that if user enters more
 * than one pedal line for the same staff, that will be handled properly. */

static void
fix_pedal(staffno, stuff_p)

int staffno;			/* pedal is for this staff */
struct STUFF *stuff_p;		/* list of pedal mark info */

{
	/* walk through list of pedal marks */
	for (  ; stuff_p != (struct STUFF *) 0; stuff_p = stuff_p->next) {

		if (stuff_p->string == (char *) 0) {
			/* no star, so have to deduce state */

			if (Pedal_state[staffno] == NO) {
				/* pedal currently off, so begin pedal */
				Pedal_state[staffno] = YES;
				stuff_p->string = copy_string(Ped_begin_str + 2,
					(int) Ped_begin_str[0],
					(int) Ped_begin_str[1]);
			}
			else {
				/* pedal currently down, so pedal up/down */
				stuff_p->string = copy_string(Ped_up_down_str + 2,
					(int) Ped_up_down_str[0],
					(int) Ped_up_down_str[1]);
			}
		}

		else if (Pedal_state[staffno] == NO) {
			yyerror("can't end pedal -- none in progress");
		}

		else {
			/* user gave star, so end pedal */
			Pedal_state[staffno] = NO;
		}
	}
}


/* reset pedal states for all staffs. This should be called at init time
 * and at any time when the number of staffs changes. This function also
 * initializes the Ped_begin_str and Ped_up_down_str. */

void
reset_ped_state()

{
	static int first_time = YES;	/* flag if function called before */
	register int s;			/* index through staffs */


	/* mark pedal off for all staffs */
	for (s = 1; s <= Score.staffs; s++) {
		Pedal_state[s] = NO;
	}
	Ped_snapshot[0] = NO;

	/* the first time this function is called, initialize the strings
	 * for pedal begin and pedal end. We just have one copy of these
	 * and then make as many copies from these as necessary */
	if (first_time == YES) {
		first_time = NO;
		Ped_begin_str = copy_string("\\(begped)", FONT_MUSIC,
							DFLT_SIZE);
		Ped_up_down_str = copy_string("\\(pedal)", FONT_MUSIC,
							DFLT_SIZE);
		fix_string(Ped_begin_str, FONT_MUSIC, DFLT_SIZE,
					Curr_filename, -1);
		fix_string(Ped_up_down_str, FONT_MUSIC, DFLT_SIZE,
					 Curr_filename, -1);
	}
}


/* fill in rehearsal mark string. This doesn't go in a STUFF, but it's
 * sort of like stuff and there didn't seem to be any more appropriate file for
 * this function */


static int Reh_let = 0;		/* current value of rehearsal letter. 0 == "A",
				 * 25 == "Z", 26 == "AA", etc to 701 == "ZZ" */
static int Reh_num = 1;		/* current value of rehearsal number */


void
set_reh_string(bar_p, fontfamily, font, size, string)

struct BAR *bar_p;	/* which bar gets the rehearsal mark */
int fontfamily;		/* what font family to use, or FAMILY_DFLT
			 * if to use current default */
int font;		/* what font to use, or FONT_UNKNOWN if to use the
			 * current default font */
int size;		/* font size to use, or -1 if to use current default */
char *string;		/* string for rehearsal mark */

{
	char reh_str[12];	/* temporary buff for string version of
				 * rehearsal number or letter */
	static int reh_size = DFLT_SIZE;	/* size to use for reh marks */
	static int reh_family = FAMILY_DFLT;	/* font family to use */
	static int reh_font = FONT_TB;		/* font to use */


	/* if first time through, init the font family to the score family */
	if (reh_family == FAMILY_DFLT) {
		reh_family = Score.fontfamily;
	}

	/* if user specified a new size, save that */
	if (size != -1) {
		if (size > 100) {
			yyerror("reh mark size too large");
			return;
		}
		else {
			reh_size = size;
		}
	}

	/* if user specified new font or font family, save that */
	if (font != FONT_UNKNOWN) {
		reh_font = font;
	}
	if (fontfamily != FAMILY_DFLT) {
		reh_family = fontfamily;
	}

	switch(bar_p->reh_type) {

	case REH_NUM:
		/* get string version of current rehearsal number, and
		 * incrment it */
		bar_p->reh_string = copy_string(num2str(Reh_num++) + 2,
				reh_family + reh_font, reh_size);
		break;

	case REH_LET:
		/* Get string version of current rehearsal letter.
		 * Start with A-Z, then AA, AB, AC, ... BA, BB, ... up to ZZ.
		 */
		if (Reh_let < 26) {
			/* 1-letter long mark */
			reh_str[0] = (char) (Reh_let + 'A');
			reh_str[1] = '\0';
		}
		else if (Reh_let < 27 * 26) {
			/* 2-letter long mark */
			reh_str[0] = (char) ((Reh_let / 26) + 'A' - 1);
			reh_str[1] = (char) ((Reh_let % 26) + 'A');
			reh_str[2] = '\0';
		}
		else {
			ufatal("too many rehearsal letters!");
		}
		bar_p->reh_string = copy_string(reh_str,
				reh_family + reh_font, reh_size);
		/* increment for next time around */
		Reh_let++;
		break;

	case REH_MNUM:
		/* get string version of current measure number */
		bar_p->reh_string = copy_string(num2str(Meas_num) + 2,
				reh_family + reh_font, reh_size);
		break;

	case REH_STRING:
		/* user-specified string */
		bar_p->reh_string = fix_string(string,
				reh_family + reh_font, reh_size,
				Curr_filename, yylineno);
		break;

	case REH_NONE:
		break;

	default:
		pfatal("set_reh_string passed bad value");
		break;
	}
}


/* Set rehearsal letter or number to user-specified value.
 * If the current bar has a rehearsal mark of the type being changed,
 * also replace its current mark with the changed one. This allows user
 * to say either
 *	reh num num=5
 * or
 *	num=5 reh num
 * and get the same results, which is consistent with how mnum= setting
 * had worked.
 */

void
init_reh(rehnumber, rehletter, mainbar_p)

int rehnumber;		/* New value for Reh_num or negative if setting Reh_let */
char *rehletter;	/* "A" to "ZZ" or null if setting number */
struct MAINLL *mainbar_p;	/* points to the current BAR */

{
	struct BAR *bar_p;
	char *oldstr;		/* previous reh_string */

	if (mainbar_p == 0 || mainbar_p->str != S_BAR) {
		pfatal("bad mainbar_p passed to init_reh");
	}
	bar_p = mainbar_p->u.bar_p;
	oldstr = bar_p->reh_string;

	if (rehnumber >= 0) {
		Reh_num = rehnumber;
		/* If this bar has a rehearsal number on this bar,
		 * replace it, and free the old one. */
		if (bar_p->reh_type == REH_NUM) {
			set_reh_string(bar_p, FAMILY_DFLT, FONT_UNKNOWN, -1,
								(char *) 0);
			FREE(oldstr);
		}
	}

	if (rehletter != 0) {
		/* Letter is stored internally as a number,
		 * which is then converted, so we have to convert in reverse.
		 * We only allow "A" through "ZZ" */
		if (isupper(rehletter[0]) && rehletter[1] == '\0') {
			Reh_let = rehletter[0] - 'A';
		}
		else if (isupper(rehletter[0]) && isupper(rehletter[1])
						&& rehletter[2] == '\0') {
			Reh_let = 26 + (rehletter[1] - 'A')
					+ (rehletter[0] - 'A') * 26;
		}
		else {
			yyerror("rehearsal letter setting must be \"A\" through \"ZZ\"");
			return;
		}
		/* If this bar has a rehearsal letter on this bar,
		 * replace it, and free the old one. */
		if (bar_p->reh_type == REH_LET) {
			set_reh_string(bar_p, FAMILY_DFLT, FONT_UNKNOWN, -1,
								(char *) 0);
			FREE(oldstr);
		}
	}
}


/* go through all stuff lists and verify that pedal marks are given in
 * ascending order. If not, error. Some code in both parse and
 * placement phases requires that pedal marks be in order. */

static void
ped_order_chk()

{
	int staffno;
	struct STUFF *stuff_p;	/* walk through stuff list */
	float last_ped_count;	/* count where last pedal occurred */
	int last_backup;	/* gracebackup of last pedal */


	/* check every staff */
	for (staffno = 1; staffno <= Score.staffs; staffno++) {

		/* initialize for current staff */
		last_ped_count = -1.0;
		last_backup = 0;

		/* In error cases, pointers may not be set right */
		if (Staffmap_p[staffno] == 0 ||
				Staffmap_p[staffno]->u.staff_p == 0) {
			continue;
		}

		/* go through stuff list for current staff, looking for pedal */
		for (stuff_p = Staffmap_p[staffno]->u.staff_p->stuff_p;
					stuff_p != (struct STUFF *) 0;
					stuff_p = stuff_p->next) {
			if (stuff_p->stuff_type == ST_PEDAL) {

				/* found a pedal. Make sure it is later than
				 * the previous pedal */
				if (stuff_p->start.count < last_ped_count ||
						(stuff_p->start.count
						== last_ped_count
						&& stuff_p->start.gracebackup
						>= last_backup) ) {
					l_yyerror(stuff_p->inputfile,
						stuff_p->inputlineno,
						"pedal must be specified in ascending order");
					/* no need to print error more than
					 * once if multiple errors */
					continue;
				}

				/* keep track of where this pedal is, for
				 * comparing with the next one */
				last_ped_count = stuff_p->start.count;
				last_backup = stuff_p->start.gracebackup;
			}
		}
	}
}


/* Translate STUFF text modifier to a printable string. */

char *
stuff_modifier(modifier)

int modifier;

{
	switch (modifier) {

	case TM_CHORD:
		return("chord");
	case TM_ANALYSIS:
		return("analysis");
	case TM_FIGBASS:
		return("figbass");
	case TM_DYN:
		return("dyn");
	case TM_NONE:
		return("(no modifier)");
	default:
		return("(invalid modifier)");
	}
}


/* Go through main list and convert any embedded phrase marks
 * (as specified by ph - eph pairs in the music input) into phrase STUFFs.
 * That way placement and later phases don't need to know or care which
 * syntax the user used.
 */

void
conv_ph_eph()

{
	struct MAINLL *mll_p;
	int vindex;		/* voice index */

	debug(2, "conv_ph_eph");

	initstructs();
	/* Process every voice on every staff */
	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			for (vindex = 0; vindex < MAXVOICES; vindex++) {
				if (mll_p->u.staff_p->groups_p[vindex] == 0) {
					break;
				}
				voice_phrases(mll_p, vindex);
			}
		}
		/* need to keep track on time signature changes at least */
		else if (mll_p->str == S_SSV) {
			asgnssv(mll_p->u.ssv_p);
		}
	}
}


/* Given a MAINLL pointing to a STAFF, and a voice index, go through
 * the GRPSYLs for that voice for the current measure, finding any
 * beginning of embedded phrases. For any found, find its matching
 * end phrase, and create a STUFF with the proper start/end values.
 */
 
static void
voice_phrases(mll_p, vindex)

struct MAINLL *mll_p;	/* points to a STAFF */
int vindex;		/* which voice to check */

{
	struct GRPSYL *gs_p;	/* to walk throug the measure */
	int ph_index;		/* index into phplace array */
	int ph_count;		/* how many ph instances on current group */


	for (gs_p = mll_p->u.staff_p->groups_p[vindex]; gs_p != 0;
							gs_p = gs_p->next) {
		/* A given GRPSYL could have multiple ph starts.
		  Loop through them all. */
		for (ph_index = 0, ph_count = gs_p->phcount;
					ph_index < ph_count; ph_index++) {
			/* There is a ph; go off to find its matching end */
			find_eph(mll_p, gs_p, vindex,
					(int) gs_p->phplace[ph_index]);
			gs_p->phcount--;
		}

		/* find_eph will decrement the ephcount every time we handle
		 * a phrase, so if we encounter an eph here,
		 * it must be an orphan. */
		if (gs_p->ephcount > 0) {
			l_warning(gs_p->inputfile, gs_p->inputlineno,
					"found eph without matching ph; ignored");
		}
	}
}


/* Given information about a GRPSYL with a ph, find its matching eph
 * and make a phrase STUFF for it. */

static void
find_eph(mll_p, gs_p, vindex, place)

struct MAINLL *mll_p;	/* Points to STAFF containing the GRPSYL with ph */
struct GRPSYL *gs_p;	/* The GRPSYL starting the phrase */
int vindex;		/* which voice */
int place;		/* PL_* */

{
	struct MAINLL *curr_mll_p;	/* in case we cross bar lines */
	struct MAINLL *m_p;		/* for finding time sig changes */
	struct MAINLL *end_mll_p;	/* points to STAFF having end */
	struct GRPSYL *end_gs_p;	/* the group with matching end */
	int bars;			/* how many bars lines crossed */
	int numstarts;			/* in case of nested phrases */
	int ecount;			/* how many eph marks seen */
	int end_ts_den;		/* time signature denominator at the end */


	/* initialize */
	bars = 0;
	curr_mll_p = end_mll_p = mll_p;
	end_gs_p = gs_p;
	numstarts = gs_p->phcount;
	end_ts_den = Score.timeden;

	/* Walk through groups looking for matching eph */
	while ((end_gs_p = nextgrpsyl(end_gs_p, &end_mll_p)) != 0) {
		if (end_mll_p != curr_mll_p) {
			/* We are now in a different measure,
			 * so we must have crossed a bar line.
			 * Note that when specifying number of bars to
			 * cross for STUFF when using the other syntax,
			 * invisbars and restarts count just like
			 * any other bars, so we do the same here.
			 */
			bars++;
			curr_mll_p = end_mll_p;

			/* Handle multirest. Actually, placement code
			 * discards any phrases that have multirests inside
			 * them, but this should make the phrase we generate
			 * here consistent with what the user would have put
			 * in using the other input method,
			 * so if we ever support phrase across multirest,
			 * this ought to work equally well. */
			if (end_gs_p->is_multirest ==YES) {
				bars -= end_gs_p->basictime + 1;
			}

			/* If we passed a time signature change,
			 * need to keep track of the new denominator.
			 * Back up to the previous bar line, or until
			 * we encounter an SSV with a time signature change.
			 * If the latter, save its denominator. */
			for (m_p = curr_mll_p; m_p != 0; m_p = m_p->prev) {
				if (m_p->str == S_SSV) {
					if (m_p->u.ssv_p->used[TIME] == YES) {
						end_ts_den = m_p->u.ssv_p->timeden;
						break;
					}
				}
				else if (m_p->str == S_BAR) {
					break;
				}
			}
		}
		for (ecount = end_gs_p->ephcount; ecount > 0; ecount--) {
			if (numstarts > 1) {
				/* end of a nested phrase */
				numstarts--;
			}
			else {
				mk_phrase_stuff(mll_p, gs_p, end_mll_p,
					bars, end_gs_p, end_ts_den, place);
				/* Mark this endphrase as handled. That
				 * way if the caller comes across a
				 * GRPSYL that still has eph set, it can
				 * know that it must be an orphan that
				 * had no matching start ph. */
				(end_gs_p->ephcount)--;
				return;
			}
		}

		/* Keep track of nested phrases */
		if (end_gs_p->phcount > 0) {
			numstarts += end_gs_p->phcount;
		}
	}
	l_warning(gs_p->inputfile, gs_p->inputlineno,
					"No matching eph for ph; phrase will be omitted");
}


/* Create a phrase STUFF for embedded ph-eph */

static void
mk_phrase_stuff(mll_p, begin_gs_p, end_mll_p, bars, end_gs_p, end_ts_den, place)

struct MAINLL *mll_p;		/* attach new STUFF to this STAFF */
struct GRPSYL *begin_gs_p;	/* where the ph was */
struct MAINLL *end_mll_p;	/* end is off of here; need for allspace() */
int bars;			/* how many bar lines were crossed to eph */
struct GRPSYL *end_gs_p;	/* where in measure the eph was */
int end_ts_den;			/* time signature denominator at end */
int place;			/* PL_* */

{
	struct GRPSYL *g_p;
	struct STUFF *stuff_p;	/* the created phrase STUFF */
	RATIONAL begin_time;	/* time offset in measure where phrase begins */
	RATIONAL end_time;	/* time offset in measure where phrase ends */
	double begin_beats;	/* begin_time converted to beats */
	double end_beats;	/* end_time converted to beats */
	int beg_gracebackup;
	int end_gracebackup;
	int ok;


	ok = YES;
	if (begin_gs_p->grpcont != GC_NOTES) {
		l_warning(begin_gs_p->inputfile, begin_gs_p->inputlineno,
				"ph must be on notes, not rest or space; phrase will be omitted");
		ok = NO;
	}
	if (end_gs_p->grpcont != GC_NOTES) {
		l_warning(end_gs_p->inputfile, end_gs_p->inputlineno,
				"eph must be on notes, not rest or space; phrase will be omitted");
		ok = NO;
	}
	if (ok == NO) {
		return;
	}

	/* Calculate how far we are into begin measure */
	for (g_p = begin_gs_p->prev, begin_time = Zero; g_p != 0;
						g_p = g_p->prev) {
		begin_time = radd(begin_time, g_p->fulltime);
	}
	/* Calculate how far we are into end measure */
	for (g_p = end_gs_p->prev, end_time = Zero; g_p != 0; g_p = g_p->prev) {
		end_time = radd(end_time, g_p->fulltime);
	}

	/* Calculate grace backup for begin */
	for (beg_gracebackup = 0, g_p = begin_gs_p; g_p->grpvalue == GV_ZERO;
							g_p = g_p->next) {
		beg_gracebackup++;
	}

	/* calculate end gracebackup */
	for (end_gracebackup = 0, g_p = end_gs_p; g_p->grpvalue == GV_ZERO;
							g_p = g_p->next) {
		end_gracebackup++;
	}

	/* Convert time offset to form needed by STUFF */
	begin_beats = 1.0 + RAT2FLOAT(begin_time) * Score.timeden;
	end_beats = 1.0 + RAT2FLOAT(end_time) * end_ts_den;
	debug(4, "creating phrase STUFF from ph-eph from %f(-%d) til %dm+%f(-%d)",
			begin_beats, beg_gracebackup,bars, end_beats, end_gracebackup);
			
	stuff_p = newSTUFF((char *)0, (double) 0.0, SD_NONE, NOALIGNTAG,
			begin_beats, (double) 0.0, beg_gracebackup,
			bars, end_beats, (double) 0.0,
			end_gracebackup, ST_PHRASE, TM_NONE, place,
			mll_p->inputfile, mll_p->inputlineno);

	stuff_p->vno = begin_gs_p->vno;
	/* If user didn't already explicitly specify a side,
	 * then if the vscheme at the beginning of the phrase (which is
	 * as far up to date as the SSV's are, fortunately) is not 1 voice,
	 * then if both voices 1 & 2 have non-space somewhere during the
	 * phrase, we need to explicitly set the place.
	 * Otherwise placement would create an extra STUFF to make phrases
	 * both above and below, and since user put this on a specific voice,
	 * we shouldn't do that.
	 */
	if (place == PL_UNKNOWN && begin_gs_p->vno != 3) {
		if (svpath(begin_gs_p->staffno, VSCHEME)->vscheme != V_1) {
			if (vvpath(begin_gs_p->staffno, 1, VISIBLE)->visible == YES
			&& vvpath(begin_gs_p->staffno, 2, VISIBLE)->visible == YES
			&& allspace(0, mll_p, begin_time, end_mll_p, end_time) == NO
		 	&& allspace(1, mll_p, begin_time, end_mll_p, end_time) == NO) {
				if (begin_gs_p->vno == 1) {
					stuff_p->place = PL_ABOVE;
				}
				else {
					stuff_p->place = PL_BELOW;
				}
			}
		}
	}

	/* Insert into proper place in stuff list */
	connect_stuff(mll_p->u.staff_p, stuff_p);
}
