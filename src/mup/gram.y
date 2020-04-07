
%{

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

/* This file contains the parser code for the Mup music publication program.
 * It uses lex to get the tokens.
 */


#include "rational.h"
#include "globals.h"
#include "defines.h"
#include "structs.h"
#include <errno.h>

#define YYDEBUG 1


/* how many items to allocate at a time for
 * "with" list, and location lists, etc */
#define ITEMS	(4)

/* Shorter way to refer to the current entry when building an entry from
 * accidentals context */
#define CURR_ACC_ENTRY	Acc_contexts_list_p->info[Curr_acc_index]

/* It user gives consecutive restoreparms with no intervening saveparms,
 * all but the last are pointless, as the last overwrites. So we have simple
 * state machine to catch this and print a warning. Even with an intervening
 * saveparms, multiple restoreparms is dubious, because really all you could
 * do is save something that you had already saved, but under a different
 * name. But we distinguish those cases. These value record what was
 * the previous control {Save|Restore}parms command type.
 * Once we see warn case, we stay in that state
 * to avoid duplicate warnings for the same issue.
 */
#define SR_SAVE		(0)
#define SR_RESTORE	(1)
#define SR_WARN	(2)


static struct MAINLL *Currstruct_p;	/* main list struct being filled in */
static struct BLOCKHEAD *Currblock_p;	/* current block, if non-zero */

static struct INPCOORD *Curr_loc_info_p; /* location info being collected */
static struct USER_SYMBOL *Curr_usym_p;	/* user-defined symbol being defined */


static short Saw_restoreparms;		/* Yes if we have seen a restoreparms
					 * in the current control context */
static short Prev_control_parms;	/* Previous {save|restore}parms */
static short Getting_tup_dur;		/* YES if currently collection tuplet
					 * duration rather than GRPSYL dur */

static struct GRPSYL *Curr_grpsyl_p;	/* current GRPSYL struct being filled in */
static struct GRPSYL *Last_grpsyl_p;	/* previous GRPSYL struct filled in (contains
					 * defaults for the current one */
static struct TIMELIST *Extra_time_p;	/* If user enters times to be added
					 * together, like 2.+16 this points to
					 * a linked list of the extra times */
static struct TIMELIST *Curr_timelist_p;/* current additive time, used to know
					 * where to link items to the list */
static struct TIMELIST *Last_alloced_timelist_p; /* Most recently allocated
					 * list. Keeping track of this lets us
					 * avoid trying to free the list
					 * when Extra_time_p is pointing
					 * to a list from an SSV. */
static struct GRPSYL *Prev_grpsyl_p;	/* Like Last_grpsyl_p except in the
					 * case of additive times,
					 * in which case it points to the
					 * first group. This is needed in case
					 * the following group is to get its
					 * time value by defaulting to that
					 * of its previous group. */
static int Extra_basictime;		/* for saving basictime, when doing
					 * additive times */
static int Doing_timeunit = NO;		/* YES if gathering timeunit param */
static struct GRPSYL *Lyrics_p;		/* pointer to current list of lyrics */
static struct WITH_ITEM *Curr_marklist;	/* current "with" list */
static int Mark_start;			/* Index into Curr_marklist at which
					 * the currently-being-gathered list
					 * began, since user could do things
					 * like [with - above;with . below][with "sfz"]
					 * Since we don't know the place till
					 * the end of a list, this lets us
					 * know how far back in the list to
					 * update with above or below.
					 */
static struct PRINTDATA **Next_print_link_p_p;	/* points to where to attach next
						 * print command that we get */
static int Item_count;			/* items in current list */
static int Max_items;			/* current amount of space in lists
					 * "with" list, curve locations, etc) */

static int Plus_minus;			/* if 1 add, if -1 subtract offset */
static int Defining_multiple = NO;	/* if defining multiple voices at once */
static int Chord_at_a_time = NO;	/* YES for chord-at-a-time,
					 * NO for voice-at-a-time */
static int EM_staff;			/* Staff number of most recent
					 * emptymeas parameter usage */
static short Endingloc;			/* if current bar starts or ends an
					 * ending, etc (STARTITEM, ENDITEM, etc) */
static short Endending_type;		/* EE_* value. Only relevent if
					 * Endingloc == ENDITEM */
static short Got_ending;		/* YES if got ending on this bar */

static int Til_bars;			/* number of bars in "til" clause */
static int Bar_between;                 /* YES or NO, if to put bar lines
                                         * between staffs rather than on then */
static struct SUBBAR_APPEARANCE *Subbar_app_p;	/* Info for subbartype parameter */
static double Dist;			/* dist for a specific STUFF */
static int Dist_usage;			/* meaning of Dist field */
static int Aligntag = NOALIGNTAG;	/* align tag number for STUFF */
static short User_meas_time;		/* YES if user specified a time for
					 * an 'm' item. Only valid on mr */
static int Order_place;			/* PL_* for markorder in SSV */
static int Order_prio;			/* value in markorder in SSV */
static int Start_gracebackup;		/* how many grace notes to back up
					 * before starting a "stuff" */
static int End_gracebackup;		/* how many grace notes to back up
					 * before ending a "stuff" */
static float Til_offset;		/* beat count in "til" clause */
static float Til_steps;			/* stepsize offset of "til" clause */
static int Linetype;			/* L_DOTTED, L_DASHED, or L_NORMAL */

static char *String1, *String2;		/* to temporarily keep track
					 * of labels and other temporary
					 * strings */
static char Tmpbuff[20];		/* temporary buffer for converting
					 * float numbers */
static char Stringbuff[100];		/* temporary string storage */
static int Titlefont, Titlesize;	/* font/size for title command */
static double Extra;			/* extra vertical space for prints */
static short Good_till_canceled = NO;	/* if [] things should be continued
					 * until turned off */
static short Curr_paratype = J_JUSTPARA; /* current paragraph type */
static char Curr_accs[2*MAX_ACCS];	/* list of current note's accidentals */
static int Acc_offset;			/* Current place in Curr_accs */
static int Curr_acc_index;		/* Current place in info array of
					 * current accidentals context */
static struct TIMEDSSV *Curr_tssv_p;	/* current mid-measure param change */
static char Timerep[MAXTSLEN];		/* Internal representation of time
					 * signature being collected */
static int Tsig_offset;			/* Current place in Timerep while
					 * collecting a time signature */
static struct TAG_REF *Curr_timeref_tag_p; /* Time offsets are relative
					 * to the most recent horizontal tag.
					 * This points to that tag,
					 * or is zero if no such tag yet */

/* Row index values must match the PS_* value definitions in defines.h.
 * Column [0] is height in portrait mode, and column [1] is height. */
double pagesztbl[7][2] = {
	{ 8.5, 11.0 },		/* PS_LETTER */
	{ 8.5, 14.0 },		/* PS_LEGAL */
	{ 8.5, 13.0 },		/* PS_FLSA */
	{ 5.5, 8.5 },		/* PS_HALFLETTER */
	{ 8.26, 11.69 },	/* PS_A4 */
	{ 5.84, 8.26 },		/* PS_A5 */
	{ 4.12, 5.84 }		/* PS_A6 */
};



/* static functions */
static char *stripquotes P((char *string));	/* rmv double quotes
						 * from string */
static void chkdup_headfoot P((int bit, char *which, struct BLOCKHEAD *block_p));
static void chkdup_topbot P((struct BLOCKHEAD **block_p_p, char *blocktype));
static void end_prev_context P((void));
static void clone_staff_ssvs P((struct MAINLL *insert_p));
static void clone_voice_ssvs P((struct MAINLL *insert_p));
static void clone1ssv P((struct SSV *clone_src_p, struct MAINLL *insert_p,
		int staffno, int voiceno));
static void chk_ssv_ranges P((UINT32B context));
static void var_valid P((void));	 /* check if okay to set location var */
static void proc_printcmd P((int justifytype, struct INPCOORD *inpc_p,
		char *str, int font, int size, int got_nl,
		int isPostScript, int ps_usage, int isfilename, 
		struct VAR_EXPORT *export_p, double extra));
static struct VAR_EXPORT *add_export P((char *varname, int index));
static double extra_needed P((int font, int size, char **string_p));
static void keyword_notes P((char *str));
static void free_extra_time P((void));
static void tsig_item P((int item));
static struct EXPR_NODE *newnode P((int op));
static void clear_curr_accs P((void));
static void add_acc P((char *acc_name));
static void new_acc_entry P((char *acc_name));
static void add_acc_offset P((int letter, double offset));
#ifdef __STDC__
static int comp_subbar P((const void *item1_p, const void *item2_p));
#else
static int comp_subbar P((char *item1_p, char *item2_p));
#endif
static int multiwhole2basictime P((int value));
static void parse_doremi_string P((char *str));


%}



%union
{
int	intval;		/* for tokens/nonterminals that return int values */
char	*stringval;	/* for tokens/nonterminals that return strings */
float	floatval;	/* for nonterminals that return floats */
RATIONAL ratval;	/* for nonterminals that return rational numbers */
struct INPCOORD *inpcoord_p;	/* for nonterminals that return coord info */
struct EXPR_NODE *expr_p;/* for nonterminals that return coord expression */
struct TAG_REF *tag_p;	/* for nonterminals that return a tag reference */
struct VAR_EXPORT *export_p;	/* for Mup variables to be passed to user PostScript */
}




/* Terminals, in alphabetical order */
%token <intval>	T_1ARG_FUNC
%token <intval>	T_2ARG_FUNC
%token <intval>	T_2FNUMVAR
%token		T_ABM
%token		T_ACCIDENTALS
%token <intval>	T_ADDSUB_OP
%token		T_ALIGN
%token <intval>	T_ALIGNLABELS
%token		T_ALL
%token		T_ALT
%token		T_AMPERSAND
%token		T_ANY
%token <intval>	T_ARRAYVAR
%token		T_AUTO
%token <intval>	T_BARSTLISTVAR
%token <intval>	T_BARTYPE
%token		T_BBOX
%token <intval>	T_BLOCKHEAD
%token		T_BM
%token <intval>	T_BULGE
%token		T_CENTS
%token <intval>	T_CHORDTRANSLATION
%token <intval>	T_CLEF
%token <intval>	T_CLEFVAR
%token		T_COLON
%token		T_COMMA
%token		T_COMMON
%token		T_CONTROL
%token <intval>	T_CUE
%token		T_CURVE
%token		T_CUT
%token		T_DASH
%token		T_DBLFLAT
%token		T_DIAM
%token		T_DOT
%token		T_DOWN
%token		T_DRUM
%token		T_EABM
%token		T_EBM
%token <intval>	T_EM_BEGIN
%token <intval>	T_EM_END
%token <intval>	T_ENDENDING
%token		T_ENDING
%token <intval>	T_ENDSTYLE
%token <intval>	T_ENDTYPE
%token		T_EPH
%token		T_EQUAL
%token		T_ESBM
%token		T_EVERY
%token		T_EXCLAM
%token <intval>	T_FAMILY
%token <intval>	T_FFAMILY
%token		T_FILE
%token <intval>	T_FIRSTPAGE
%token <intval>	T_FNUMVAR
%token <intval>	T_FONT
%token <intval>	T_FONTVAR
%token		T_GRACE
%token		T_GRIDS
%token		T_HAT
%token		T_HEADSHAPES
%token		T_HIDECHANGES
%token		T_HO
%token		T_HS
%token <intval> T_INPUTDIR
%token <intval>	T_JUSTIFYTYPE
%token <intval>	T_KEY
%token <intval>	T_KEYMAP
%token <intval>	T_KEYMAPVAR
%token		T_LBRACE
%token		T_LBRACKET
%token		T_LEN
%token		T_LET
%token		T_LETTER
%token		T_LET_M
%token		T_LET_N
%token		T_LET_R
%token		T_LET_S
%token		T_LET_U
%token <intval>	T_LET_X
%token		T_LINE
%token <intval>	T_LINETYPE
%token		T_LPAREN
%token <intval>	T_LVAR
%token		T_LYRICS
%token <intval>	T_L_ANGLE
%token		T_L_DBLANGLE
%token <intval>	T_MEASNUM
%token		T_MIDDLE
%token		T_MIDI
%token		T_MNUM
%token <intval>	T_MODIFIER
%token <intval>	T_MULDIV_OP
%token		T_MULTIREST
%token <intval>	T_MULTIWHOLE
%token		T_MUSIC
%token		T_MUSSYM
%token <intval>	T_NEWLINE
%token		T_NL
%token <intval>	T_NONE
%token		T_NONNAT
%token <intval>	T_NOWHERE_SLIDE
%token		T_NUM
%token		T_NUMBER
%token <intval>	T_NUMVAR
%token		T_OCTAVE
%token <intval>	T_OPTSTRVAR
%token <intval>	T_ORDER
%token <intval>	T_ORIENTATION
%token		T_OTHERTEXT
%token <intval>	T_PAD
%token <intval>	T_PAGESIDE
%token <intval>	T_PAGESIZE
%token		T_PARAGRAPH
%token <intval>	T_PARATYPE
%token		T_PEDAL
%token		T_PEDSTAR
%token <intval>	T_PEDSTYLE
%token		T_PH
%token		T_PHRASE
%token		T_PITCH
%token <intval>	T_PLACE
%token		T_PLUS
%token		T_POSTSCRIPT
%token <intval>	T_PRINTTYPE
%token <intval>	T_PRINTEDTIME
%token <intval> T_PSHOOKLOC
%token		T_PSVAR
%token		T_QUESTION
%token <intval>	T_RANGELISTVAR
%token <intval>	T_RATNUMLISTVAR
%token		T_RBRACE
%token		T_RBRACKET
%token		T_REHEARSAL
%token <intval>	T_REHTYPE
%token <intval>	T_REH_STYLE
%token <intval>	T_REPEATDOTS
%token		T_RESTOREPARMS
%token		T_ROLL
%token		T_RPAREN
%token		T_RPT
%token		T_R_ANGLE
%token		T_R_DBLANGLE
%token		T_SAVEPARMS
%token		T_SCORE
%token <intval>	T_SCOREFEED
%token <intval>	T_SCOREPAD
%token <intval>	T_SCORESEP
%token		T_SEMICOLON
%token		T_SHARP
%token <intval>	T_SHORTEN
%token		T_SLASH
%token		T_SLASHMARK
%token		T_SLOPE
%token <intval>	T_SLUR
%token		T_STAFF
%token <intval>	T_STAFFLINES
%token		T_STANDARD
%token		T_STAR
%token		T_STEMOFFSET
%token <stringval>	T_STRING
%token <intval>	T_STRVAR
%token <intval>	T_SUBBARSTYLE
%token <intval>	T_SWINGUNIT
%token		T_SYMBOL
%token		T_TAB
%token <intval>	T_TICKS
%token <intval>	T_TIE
%token		T_TIL
%token <intval>	T_TILDE
%token <intval>	T_TIME
%token <intval>	T_TIMEUNIT
%token		T_TITLE
%token		T_TO
%token <intval>	T_TRANSPOSE
%token <intval>	T_TRANSUSAGE
%token <intval>	T_TUNINGTYPE
%token <intval>	T_TUNINGVAR
%token		T_UNARY_OP
%token <intval>	T_UNITS
%token <intval>	T_UNITTYPE
%token		T_UNSET 
%token		T_UP
%token <intval>	T_USEACCS
%token		T_USING
%token <intval>	T_VCOMBINE
%token <intval>	T_VCOMBVAL
%token <intval>	T_VISVAR
%token		T_VOICE
%token <intval>	T_VVAR
%token		T_WHEREUSED
%token		T_WITH
%token		T_XNOTE
%token <intval>	T_XPOS_INT
%token <intval>	T_YESNOVAR

/* Nonterminals, in alphabetical order */
%type <floatval> acc_offset
%type <intval> acc_symbol
%type <export_p> alias
%type <export_p> aliased_var
%type <export_p> alias_or_export_var
%type <inpcoord_p> alloc_loc
%type <stringval> arraytag
%type <intval> bar_opt
%type <intval> barline
%type <intval> bartype
%type <ratval> basic_time_val
%type <floatval> beat_offset
%type <intval> between
%type <intval> ch_tran_type
%type <intval> crossbeam
%type <intval> dir
%type <intval> direction
%type <intval> dots
%type <intval> end_type
%type <stringval> ending_info
%type <export_p> export_item
%type <export_p> export_list
%type <export_p> export_var
%type <export_p> export_var_list
%type <expr_p> expression
%type <intval> flagval
%type <floatval> floatnum
%type <intval> font_family
%type <stringval> glabel
%type <intval> gracebackup
%type <intval> grp_staff_range
%type <intval> hor_offset
%type <intval> inputdir
%type <intval> justifytype
%type <floatval> line_offset
%type <intval> line_ref
%type <intval> linetype
%type <stringval> loc_variable
%type <inpcoord_p> location
%type <intval> majmin
%type <floatval> margin_override
%type <stringval> mark_item
%type <intval> marktype
%type <intval> mark_place
%type <intval> measnumval
%type <intval> minuslist
%type <intval> notelist
%type <intval> num
%type <stringval> numstr
%type <intval> octave
%type <intval> opt_accidental
%type <stringval> opt_bend
%type <stringval> opt_decimal_part
%type <intval> opt_dir
%type <intval> opt_file
%type <stringval> opt_line_str
%type <intval> opt_loc
%type <intval> opt_minus
%type <intval> opt_modifier
%type <intval> opt_n
%type <intval> opt_octave
%type <intval> opt_other_staff
%type <intval> opt_pageside
%type <intval> opt_paratype
%type <intval> opt_place
%type <intval> opt_plus_or_minus
%type <intval> opt_print_clef
%type <intval> opt_rs
%type <intval> opt_side
%type <intval> opt_size
%type <intval> opt_str_acc
%type <stringval> opt_string
%type <intval> opt_ticks
%type <ratval> opt_time
%type <intval> opt_to_voice
%type <intval> opt_ts_visibility
%type <intval> opt_vcombine_qualifier
%type <intval> orderitem
%type <intval> orientation
%type <floatval> padspec
%type <intval> pagesize
%type <intval> paramname
%type <intval> pedstyle
%type <intval> ph_opt_place
%type <intval> pitch
%type <intval> plus_or_minus
%type <intval> pluslist
%type <intval> prclef
%type <intval> precbartype
%type <intval> printtup
%type <intval> printtype
%type <intval> ps_opt_loc
%type <intval> rangetail
%type <stringval> rehearsal_mark
%type <intval> repeatdots_value
%type <stringval> shorthand_mark
%type <intval> some_notes
%type <intval> staff_range
%type <intval> staffnum
%type <floatval> steps_offset
%type <stringval> string
%type <floatval> stuff_dist
%type <intval> stuff_type
%type <ratval> swing_time
%type <intval> tab_string_list
%type <intval> til_suffix
%type <ratval> time_item
%type <intval> title_font
%type <intval> transpose_interval
%type <intval> trans_usage
%type <intval> updown
%type <intval> useaccs_qual
%type <intval> useaccs_value
%type <intval> value
%type <stringval> variable
%type <intval> verse_range
%type <intval> voice_spec
%type <intval> voicenum
%type <intval> vschemetype
%type <expr_p> xcoord
%type <expr_p> ycoord

/* associativity to get desired arithmetic precedence */
%left T_ADDSUB_OP
%left T_MULDIV_OP
%right T_UNARY_OP

%expect 4

%%


song:	item

	|
	song item
	;

item:	context opt_semi
	
	|
	assignment

	|
	title

	|
	paragraph

	|
	music_input

	|
	directive T_NEWLINE

	|
	stringpair

	|
	acc_entry

	|
	control_item terminator

	|
	os_directive opt_semi T_NEWLINE

	|
	T_NEWLINE
	{
		/* for blank lines -- nothing to do */
	}

	|
	error T_NEWLINE
	{
		/* to keep going after stumbling on an input error */
		Getting_tup_dur = NO;
		Good_till_canceled = NO;
		Defining_multiple = NO;
		Curr_grpsyl_p = (struct GRPSYL *) 0;
		Last_grpsyl_p = (struct GRPSYL *) 0;
		Prev_grpsyl_p = (struct GRPSYL *) 0;
	};

context:	ssv_context
	{
		/* fill in context field of the SSV struct */
		Currstruct_p->u.ssv_p->context = (short) Context;
	}

	|
	control_context
	{}

	|
	blockhead_context
	{
		Curr_family = Score.fontfamily;
	}

	|
	grid_context
	{}

	|
	headshape_context
	{}

	|
	accidentals_context
	{}

	|
	keymap_context
	{}

	|
	symbol_context
	{}

	|
	music_context new_context
	{
		/* have lots of separate structs in music context, so
		 * mark that we have no current struct */
		Currstruct_p = (struct MAINLL *) 0;

		/* make sure at least 1 staff is visible. Not good to
		 * check when user sets visible, because it would be
		 * nice to first set everything invisible and later
		 * turn one on. So if we wait till now to check,
		 * user doesn't have to switch back and forth from
		 * score to staff to get 1 voice visible. */
		check_at_least1visible();
	};

ssv_context:	T_SCORE new_context alloc_ssv
	{
		Context = C_SCORE;
		Curr_font = svpath(0, FONT)->font;
		Curr_family = svpath(0, FONTFAMILY)->fontfamily;
		Curr_size = svpath(0, SIZE)->size;
	}

	|
	T_STAFF new_context { begin_range(PL_UNKNOWN); } ssv_stafflist alloc_ssv
	{
		/* Make sure all the staffs are in range */
		chk_ssv_ranges(C_STAFF);

		/* Set current SSV's staff to the first one on the list.
		 * If there is more than one, we will clone the SSV later.
		 * Note that it is possible that the staff number the user
		 * specified is actually illegal, but we will use it anyway,
		 * because it keeps the code simpler to pretend all is well,
		 * and ssv.c code will ignore such illegal SSVs anyway.
		 */
		if (Svrangelist_p != 0 && Svrangelist_p->stafflist_p != 0) {
			Currstruct_p->u.ssv_p->staffno
					= Svrangelist_p->stafflist_p->begin;
		}
		Context = C_STAFF;
	}

	|
	T_VOICE new_context { begin_range(PL_UNKNOWN); } ssv_sv_list alloc_ssv
	{
		/* Make sure all the staffs and voices are in range */
		chk_ssv_ranges(C_VOICE);

		/* Set current SSV's staff/voice to the first one on the list.
		 * If there are more than, we will clone the SSV later */
		if (Svrangelist_p != 0 && Svrangelist_p->stafflist_p != 0) {
			Currstruct_p->u.ssv_p->staffno
					= Svrangelist_p->stafflist_p->begin;
		}
		if (Svrangelist_p != 0 && Svrangelist_p->vnolist_p != 0) {
			Currstruct_p->u.ssv_p->voiceno
					= Svrangelist_p->vnolist_p->begin;
		}
		Context = C_VOICE;
	};

ssv_stafflist: ssv_staff_item
	|
	ssv_stafflist sv_and ssv_staff_item
	;

ssv_staff_item:
	grp_stafflist
	{
		add_to_sv_list();
	};

ssv_sv_list: ssv_sv_item
	|
	ssv_sv_list sv_and ssv_sv_item
	;

ssv_sv_item: grp_stafflist voice_spec
	{
		add_to_sv_list();
	};

alloc_ssv: 
	{
		/* allocate an SSV struct */
		Currstruct_p = newMAINLLstruct(S_SSV, yylineno);
	};

new_context:
	{
		/* entering new context, if we have a current struct,
		 * we were in a different context, so add its info to the list */
		end_prev_context();
	};

blockhead_context:	T_BLOCKHEAD opt_pageside
	{
		struct FEED *feed_p;		/* where to attach blocks */
		int calloced;			/* YES if dynamically alloced */

		end_prev_context();
		Context = $1;
		calloced = NO;

		switch ($1) {
		case C_HEADER:
			switch ($2) {
			case PGSIDE_NOT_SET:
				chkdup_headfoot(GOT_HEADER, "header", &Header);
				break;
			case PGSIDE_LEFT:
				chkdup_headfoot(GOT_LHEADER,
					"header leftpage", &Leftheader);
				break;
			case PGSIDE_RIGHT:
				chkdup_headfoot(GOT_RHEADER,
					"header rightpage", &Rightheader);
				break;
			}
			break;

		case C_HEAD2:
			switch ($2) {
			case PGSIDE_NOT_SET:
				chkdup_headfoot(GOT_HEADER2,
					"header2", &Header2);
				break;
			case PGSIDE_LEFT:
				chkdup_headfoot(GOT_LHEADER2,
					"header2 leftpage", &Leftheader2);
				break;
			case PGSIDE_RIGHT:
				chkdup_headfoot(GOT_RHEADER2,
					"header2 rightpage", &Rightheader2);
				break;
			}
			break;
		case C_FOOTER:
			switch ($2) {
			case PGSIDE_NOT_SET:
				chkdup_headfoot(GOT_FOOTER,
					"footer", &Footer); 
				break;
			case PGSIDE_LEFT:
				chkdup_headfoot(GOT_LFOOTER,
					"footer leftpage", &Leftfooter);
				break;
			case PGSIDE_RIGHT:
				chkdup_headfoot(GOT_RFOOTER,
					"footer rightpage", &Rightfooter);
				break;
			}
			break;
		case C_FOOT2:
			switch ($2) {
			case PGSIDE_NOT_SET:
				chkdup_headfoot(GOT_FOOTER2,
					"footer2", &Footer2);
				break;
			case PGSIDE_LEFT:
				chkdup_headfoot(GOT_LFOOTER2,
					"footer2 leftpage", &Leftfooter2);
				break;
			case PGSIDE_RIGHT:
				chkdup_headfoot(GOT_RFOOTER2,
					"footer2 rightpage", &Rightfooter2);
				break;
			}
			break;
		case C_TOP:
		case C_TOP2:
		case C_BOT:
		case C_BOT2:
		case C_BLOCK:
			CALLOC(BLOCKHEAD, Currblock_p, 1);
			calloced = YES;
			break;
		default:
			pfatal("Unknown block-type context");
			/*NOTREACHED*/
			break;
		}
		set_win_coord(Currblock_p->c);
		/* Remember where to start attaching "print" commands */
		Next_print_link_p_p = &(Currblock_p->printdata_p);

		/* The dynamically allocated blocks
		 * (not the static head/foot blocks)
		 * need extra processing to populate
		 * the main list properly with FEEDs.
		 */
		if (calloced == YES) {
			if (Mainlltc_p == 0 || Mainlltc_p->str != S_FEED) {
				/* User had not specified a feed
				 * right before this block, so we need
				 * to add one for them implicitly.
				 * Create a new FEED and add it
				 * to the end of the main list.
				 */
				insertMAINLL(newMAINLLstruct(S_FEED, yylineno),
							Mainlltc_p);
			}
			feed_p = Mainlltc_p->u.feed_p;

			/* Normally, top/top2/bot/bot2 result in a pagefeed.
			 * But we have a special case for backward
			 * compatibility and for flexibility.
			 * User can get a separate
			 * "title" page if they really want to,
			 * by using an explicit newpage at the beginning.
			 * But in the normal case, they'd want to declare
			 * top/bot that will go on the first page without
			 * that causing a newpage. So if there hasn't been
			 * any music or block yet, we won't set newpage=YES
			 * for top/top2/bot/bot2, but otherwise we will.
			 */
			if (Context & (C_TOP|C_TOP2|C_BOT|C_BOT2)) {
				struct MAINLL *m_p;
				for (m_p = Mainlltc_p->prev; m_p != 0;
							m_p = m_p->prev) {
					if (m_p->str == S_BAR ||
							m_p->str == S_BLOCKHEAD) {
						/* There was music or block,
						 * so this is not
						 * at very beginning,
						 * so no special case:
						 * there will be new page.
						 */
						feed_p->pagefeed = YES;
						break;
					}
				}
			}

			/* Now check for redefined blocks and
			 * attach the BLOCKHEAD onto the FEED or the
			 * main list, as appropriate.
			 */
			switch (Context) {
			case C_TOP:
				switch ($2) {
				case PGSIDE_NOT_SET:
					chkdup_topbot( &(feed_p->top_p),
							"top" );
					break;
				case PGSIDE_LEFT:
					chkdup_topbot( &(feed_p->lefttop_p),
							"top leftpage" );
					break;
				case PGSIDE_RIGHT:
					chkdup_topbot( &(feed_p->righttop_p),
							"top rightpage" );
					break;
				}
				break;
			case C_TOP2:
				switch ($2) {
				case PGSIDE_NOT_SET:
					chkdup_topbot( &(feed_p->top2_p),
							"top2" );
					break;
				case PGSIDE_LEFT:
					chkdup_topbot( &(feed_p->lefttop2_p),
							"top2 leftpage" );
					break;
				case PGSIDE_RIGHT:
					chkdup_topbot( &(feed_p->righttop2_p),
							"top2 rightpage" );
					break;
				}
				break;
			case C_BOT:
				switch ($2) {
				case PGSIDE_NOT_SET:
					chkdup_topbot( &(feed_p->bot_p),
							"bot" );
					break;
				case PGSIDE_LEFT:
					chkdup_topbot( &(feed_p->leftbot_p),
							"bot leftpage" );
					break;
				case PGSIDE_RIGHT:
					chkdup_topbot( &(feed_p->rightbot_p),
							"bot rightpage" );
					break;
				}
				break;
			case C_BOT2:
				switch ($2) {
				case PGSIDE_NOT_SET:
					chkdup_topbot( &(feed_p->bot2_p),
							"bot2" );
					break;
				case PGSIDE_LEFT:
					chkdup_topbot( &(feed_p->leftbot2_p),
							"bot2 leftpage" );
					break;
				case PGSIDE_RIGHT:
					chkdup_topbot( &(feed_p->rightbot2_p),
							"bot2 rightpage" );
					break;
				}
				break;
			case C_BLOCK:
				insertMAINLL(newMAINLLstruct(S_BLOCKHEAD, yylineno),
						Mainlltc_p);
				Mainlltc_p->u.blockhead_p = Currblock_p;
				if ($2 != PGSIDE_NOT_SET) {
					l_warning(Curr_filename, yylineno,
						"leftpage/rightpage cannot be used on blocks; ignoring");
				}
				/* These blocks count like "music" for purposes
				 * of things that can only be set
				 * before any "music"
				 */
				Got_some_data = YES;
				break;
			case C_HEADER:
			case C_HEAD2:
			case C_FOOTER:
			case C_FOOT2:
				/* These are static, not in main list,
				 * so nothing needs to be done with them.
				 */
				break;
			default:
				pfatal("unexpected context (0x%x) for block", Context);
				/*NOTREACHED*/
				break;
			}

			/* We need a feed after a block.
			 * Use -1 as lineno to mark it as internally generated.
			 * That way if user puts their own explicit feed
			 * next in the input, we can know we can
			 * discard this internally generated one.
			 */
			if (Context & C_BLOCK) {
				insertMAINLL(newMAINLLstruct(S_FEED, -1), Mainlltc_p);
			}
		}
	};

music_context:	T_MUSIC
	{
		Context = C_MUSIC;
	};

grid_context:	T_GRIDS
	{
		end_prev_context();
		Context = C_GRIDS;
	};

accidentals_context:	T_ACCIDENTALS begin_acc_context acc_context_name
	{
	};

begin_acc_context:
	{
		end_prev_context();
		Context = C_ACCIDENTALS;
	};

acc_context_name:	string
	{
		struct ACCIDENTALS *acc_name_map_p;

		/* Make sure not already in list */
		for (acc_name_map_p = Acc_contexts_list_p; acc_name_map_p != 0;
					acc_name_map_p = acc_name_map_p->next) {
			if (strcmp(acc_name_map_p->name, $1) == 0) {
				l_warning(Curr_filename, yylineno,
				"accidental context name %s re-defined; using last", $1);
				/* We'll free the table, since that is
				 * potentially big, and is easy to do.
				 * We'll leave the ACCIDENTALS struct itself,
				 * because it doesn't seem worth the trouble
				 * to keep track of the "prev" to free up a
				 * little memory in a rare error case.
				 * And we'll leave the name, so future loops
				 * though the list can strcmp it, even though
				 * it will never really be used. We will put
				 * the replacement at the head of the list,
				 * so we'll always see that copy first and
				 * never bother to come down to this one.
				 */
				if (acc_name_map_p->info != 0) {
					FREE(acc_name_map_p->info);
					acc_name_map_p->info = 0;
					acc_name_map_p->size = 0;
				}
				/* If redefined multiple times, don't
				 * bother giving multiple errors. */
				break;
			}
		}

		MALLOC(ACCIDENTALS, acc_name_map_p, 1);
		acc_name_map_p->name = strdup($1 + 2);
		MALLOC(ACCINFO, acc_name_map_p->info, ITEMS);
		acc_name_map_p->size = 0;
		acc_name_map_p->next = Acc_contexts_list_p;
		Acc_contexts_list_p = acc_name_map_p;
	};

acc_entry:	acc_name offsets_spec T_NEWLINE
	{
		if (Context != C_ACCIDENTALS) {
			yyerror("something like looks like an accidentals entry when not in accidentals context");
		}
	}
	;

acc_name: string
	{
		if (Context == C_ACCIDENTALS) {
			new_acc_entry($1 + 2);
		}
	};

offsets_spec:
	T_ALL acc_offset
	{
		/* A single value to use for all notes a-g */
		int i;
		for (i = 0; i < 7; i++) {
			add_acc_offset('a' + i, $2);
		}
	}

	|
	offsets_list
	;

offsets_list:
	pitch acc_offset
	{
		add_acc_offset($1, $2);
	}

	|
	offsets_list pitch acc_offset
	{
		add_acc_offset($2, $3);
	};

acc_offset:
	plus_or_minus floatnum T_CENTS
	{
		$$ = (float) cents2value( (double) ($1 * $2));
	}

	|
	floatnum
	{
		$$ = $1;
	}

	|
	floatnum T_SLASH floatnum
	{
		if ($3 == 0.0) {
			yyerror("denominator cannot be zero");
		}
		else {
			$$ = $1 / $3;
		}
	};
	
keymap_context:	T_KEYMAP string
	{
		end_prev_context();
		Context = C_KEYMAP;
		new_keymap($2 + 2);
	};


control_context:
	T_CONTROL
	{
		end_prev_context();
		Context = C_CONTROL;
		Saw_restoreparms = NO;
		Prev_control_parms = SR_SAVE;	/* really means "not restore"
						 * but didn't seem worth
						 * having separate state
						 * for that. */

	};

control_item:
	T_SAVEPARMS string
	{
		if (contextcheck(C_CONTROL, "saveparms") == YES) {
			/* Associate the user's name with the current place
			 * in the main list */
			add_saveparms($2 + 2);
			if (Prev_control_parms != SR_WARN) {
				Prev_control_parms = SR_SAVE;
			}
		}
	}

	|
	T_RESTOREPARMS string
	{
		if (contextcheck(C_CONTROL, "restoreparms") == YES) {
			if (do_restoreparms($2 + 2) == YES) {
				if (Saw_restoreparms == YES) {
					if (Prev_control_parms == SR_RESTORE) {
						l_warning(Curr_filename, yylineno,
						"consecutive instances of 'restoreparms' in the same control context; only the last is effective");
					}
					else if (Prev_control_parms != SR_WARN) {
						l_warning(Curr_filename, yylineno,
						"multiple instances of 'restorparms is of dubious usefulness");
					}
					Prev_control_parms = SR_WARN;
				}
				if (Prev_control_parms != SR_WARN) {
					Prev_control_parms = SR_RESTORE;
				}
			}
		}
	};

stringpair:
	string string T_NEWLINE
	{
		if (Context == C_GRIDS) {
			add_grid($1, $2);
		}
		else if (Context == C_HEADSHAPES) {
			/* skip past the font/size bytes */
			add_shape($1 + 2, $2 + 2);
		}
		else if (Context == C_KEYMAP) {
			add_to_keymap($1, $2);
		}
		else {
			l_warning(Curr_filename, yylineno - 1,
				"unexpected pair of text strings (possibly intended to be in grids, headshapes, or keymap context, or missing + for concatenation?)");
		}
	};

headshape_context:
	T_HEADSHAPES
	{
		end_prev_context();
		Context = C_HEADSHAPES;
	}
	;

symbol_context:
	T_SYMBOL string
	{
		end_prev_context();
		Context = C_SYMBOL;
		/* The +2 to skip font/size */
		Curr_usym_p = alloc_usym($2 + 2);
	}
	;

staffnum:	num
	;

voicenum:	num
	{
		$$ = $1;
		/* later on we check for being within current range */
	};

num:	T_NUMBER
	{
		errno = 0;
		$$ = strtol(yytext, 0, 10);
		if (errno == ERANGE) {
			l_yyerror(Curr_filename, yylineno,
				"Number %s is beyond the range of numbers that can be handled", yytext);
		}
	};

value:	opt_minus num
	{
		$$ = $1 * $2;
	}
	;

assignment:	assign terminator
	;

terminator:	T_SEMICOLON

	|
	T_NEWLINE
	;

assign:	T_NUMVAR T_EQUAL opt_minus num
	{
		/* Only a few things are allowed to have a negative number.
		 * Other code assumes that that restriction is enforced here,
		 * so we only honor the minus if it is legal. */
		if ($3 == -1) {
			if ($1 != SYLPOSITION) {
				yyerror("negative value not allowed");
			}
			else {
				assign_int($1, -($4), Currstruct_p);
			}
		}
		else {
			assign_int($1, $4, Currstruct_p);
		}
	}

	|
	T_NUMVAR T_EQUAL
	{
		/* only restcombine and gridfret can be set to empty */
		if ($1 == RESTCOMBINE) {
			assign_int($1, NORESTCOMBINE, Currstruct_p);
		}
		else if ($1 == GRIDFRET) {
			assign_int($1, NOGRIDFRET, Currstruct_p);
		}
		else {
			yyerror("parameter value required");
		}
	}

	|
	T_FIRSTPAGE T_EQUAL num opt_pageside
	{
		assign_firstpage($3, $4, Currstruct_p);
	}

	|
	T_FNUMVAR T_EQUAL opt_minus floatnum
	{
		if ($3 == -1) {
			if ($1 != STAFFPAD) {
				yyerror("negative value not allowed");
			}
			else {
				assign_float($1, -($4), Currstruct_p);
			}
		}
		else {
			assign_float($1, $4, Currstruct_p);
		}
	}

	|
	T_2FNUMVAR T_EQUAL floatnum T_COMMA floatnum
	{
		assign_2floats($1, $3, $5, Currstruct_p);
	}

	|
	T_SHORTEN T_EQUAL floatnum shorten_opts
	{
		assign_float($1, $3, Currstruct_p);
	}

	|
	T_INPUTDIR T_EQUAL inputdir
	{
		assign_direction($1, $3, Currstruct_p);
	}

	|
	T_PSVAR T_EQUAL pagesize orientation
	{
		double multiplier;
		multiplier = (Score.units == INCHES ? 1.0 : CMPERINCH);
		assign_float(PAGEWIDTH, pagesztbl[$3][$4] * multiplier, Currstruct_p);
		assign_float(PAGEHEIGHT, pagesztbl[$3][$4 ^ 1] * multiplier, Currstruct_p);
	}

	|
	T_VCOMBINE T_EQUAL { begin_range(PL_UNKNOWN);  } vcombine_value
	{ }

	|
	T_PAD T_EQUAL opt_minus floatnum
	{
		/* specified in stepsizes, stored in inches */
		assign_float(PAD, $3 * $4 * STEPSIZE, Currstruct_p);
	}

	|
	T_VVAR T_EQUAL num vschemetype
	{
		assign_vscheme ($3, $4, Currstruct_p);
	}

	|
	T_RATNUMLISTVAR T_EQUAL allocbeamlist ratnumlist opt_rs
	{
		set_beamlist(Currstruct_p);
		if (Currstruct_p != 0 && Currstruct_p->u.ssv_p != 0) {
			Currstruct_p->u.ssv_p->beamrests = ($5 & 0x1);
			Currstruct_p->u.ssv_p->beamspaces = (($5 >> 1) & 0x1);
		}
	}

	|
	T_RANGELISTVAR T_EQUAL alloc_rangelist rangelist
	{
		set_staffset($1, Currstruct_p);
	}

	|
	T_BARSTLISTVAR T_EQUAL alloc_barstlist barstlist
	{
		set_barstlist(Currstruct_p, 0);
	}

	|
	T_SUBBARSTYLE T_EQUAL subbar_check subbar_list
	{
		/* Put them in time order */
		if (Currstruct_p != 0) {
			qsort((char *)(Currstruct_p->u.ssv_p->subbarlist),
				Currstruct_p->u.ssv_p->nsubbar,
				sizeof(struct SUBBAR_INSTANCE),
				comp_subbar);
		}
	}

	|
	T_REPEATDOTS T_EQUAL repeatdots_value
	{
		if (contextcheck(C_SCORE|C_STAFF, "repeatdots") == YES
					&& Currstruct_p != 0) {
			Currstruct_p->u.ssv_p->repeatdots = $3;
			Currstruct_p->u.ssv_p->used[REPEATDOTS] = YES;
		}
	}

	|
	mark_order T_EQUAL orderlist
	{
		if (Currstruct_p != 0) {
			chk_order(Currstruct_p->u.ssv_p, Order_place);
		}
	}

	|
	T_KEY T_EQUAL keyspec

	|
	T_USEACCS T_EQUAL useaccs_value
	{
		assign_int($1, $3, Currstruct_p);
	}

	|
	T_TIME T_EQUAL timesig
	{
		/* all the work for this is done in timesig */
	}

	|
	T_PRINTEDTIME T_EQUAL printed_timesig

	|
	T_STAFFLINES T_EQUAL stafflinedef
	{
	}

	|
	T_FONTVAR T_EQUAL T_FONT
	{
		set_font($1, $3, Currstruct_p);
	}

	|
	T_FAMILY T_EQUAL T_FFAMILY
	{
		set_font($1, $3, Currstruct_p);
	}

	|
	T_CLEFVAR T_EQUAL T_CLEF opt_print_clef
	{
		if (contextcheck(C_SCORE|C_STAFF,"clef parameter") == YES) {
			used_check(Currstruct_p, CLEF, "clef");
			if (Context == C_STAFF && has_tab_staff()) {
				/* We used to be able to make this just a
				 * warning, but now that is hard, because
				 * we'd want to let it assign for any
				 * non-tab, but it is hard to know at this
				 * point which SSVs will be tab and which					 * non-tab, because we may not have created
				 * them all yet. We cannot let the tab ones
				 * get assigned, because later code would
				 * get confused by clef not being TABCLEF.
				 * While we could figure this all out with
				 * enough code, it really is a user error,
				 * so not worth bending over backwards
				 * to try to keep it just a warning.
				 */
				l_yyerror(Curr_filename, yylineno,
					"can't set clef on tab staff");
			}
			if (Currstruct_p != 0) {
				Currstruct_p->u.ssv_p->clef = $3;
				Currstruct_p->u.ssv_p->forceprintclef = $4;
				Currstruct_p->u.ssv_p->used[CLEF] = YES;
				asgnssv(Currstruct_p->u.ssv_p);
			}
		}
	}

	|
	T_CHORDTRANSLATION T_EQUAL ch_tran_type
	{
		if (contextcheck(C_SCORE | C_STAFF, "chordtranslation parameter") == YES
						&& (Currstruct_p != 0) ) {
			if (Currstruct_p->u.ssv_p->used[$1] == YES) {
				warning("setting chordtranslation parameter overrides previous setting");
			}
			Currstruct_p->u.ssv_p->chordtranslation = $3;
			Currstruct_p->u.ssv_p->used[CHORDTRANSLATION] = YES;
		}
	}

	|
	T_ALIGNLABELS T_EQUAL justifytype
	{
		assign_int($1, $3, Currstruct_p);
	}

	|
	T_YESNOVAR T_EQUAL flagval
	{
		assign_int($1, $3, Currstruct_p);
	}

	|
	T_VISVAR T_EQUAL visval

	|
	T_MEASNUM T_EQUAL measnumval
	{
		assign_int($1, $3, Currstruct_p);
	}

	|
	T_UNSET paramname
	{
		if (contextcheck(C_STAFF|C_VOICE, "unset") == YES
					&& Currstruct_p != 0) {
			if ($2 < 0 || $2 >= NUMFLDS) {
				pfatal("invalid parameter index %d on line %d", $2, yylineno);
			}
			if (Currstruct_p->u.ssv_p->used[$2] == YES) {
				warning("unsetting parameter overrides previous setting");
			}
			/* Note that if user tries to unset something that
			 * can't be set in the current context, we silently
			 * ignore that. We figure that since it's already unset,
			 * it doesn't hurt anything to unset it again,
			 * and it's easier to not bother to check.
			 */

			Currstruct_p->u.ssv_p->used[$2] = UNSET;

			/* special case: scoresep controls two fields */
			if ($2 == MINSCSEP) {
				Currstruct_p->u.ssv_p->used[MAXSCSEP] = UNSET;
			}

			/* Assign, just in case there are new interactions */
			asgnssv(Currstruct_p->u.ssv_p);
		}
	}

	|
	T_TIMEUNIT T_EQUAL { Doing_timeunit = YES; } time_item
	{
		if ( contextcheck(C_SSV, "timeunit parameter") == YES ) {
			used_check(Currstruct_p, TIMEUNIT, "timeunit");
			if (Currstruct_p != 0) {
				Currstruct_p->u.ssv_p->timeunit = $4;
				Currstruct_p->u.ssv_p->timelist_p = Extra_time_p;
				Currstruct_p->u.ssv_p->used[TIMEUNIT] = YES;
			}
			Extra_time_p = 0;
			Doing_timeunit = NO;
		}
	}

	|
	T_SWINGUNIT T_EQUAL swing_time
	{
		if ( contextcheck(C_SSV, "swingunit parameter") == YES ) {
			used_check(Currstruct_p, SWINGUNIT, "swingunit");
			if (Currstruct_p != 0) {
				Currstruct_p->u.ssv_p->swingunit = $3;
				Currstruct_p->u.ssv_p->used[SWINGUNIT] = YES;
			}
		}
	}

	|
	T_SCORESEP T_EQUAL minscsep maxscsep
	{
	}

	|
	T_SCOREPAD T_EQUAL minscpad maxscpad
	{
	}

	|
	T_ENDSTYLE T_EQUAL end_type
	{
		if ( contextcheck(C_SCORE, "endingstyle parameter") == YES
					&& Currstruct_p != 0 ) {
			used_check(Currstruct_p, ENDINGSTYLE, "endingstyle");
			Currstruct_p->u.ssv_p->endingstyle = $3;
			Currstruct_p->u.ssv_p->used[ENDINGSTYLE] = YES;
		}
	}

	|
	T_REH_STYLE T_EQUAL T_REHTYPE
	{
		if (Currstruct_p != 0) {
			if ($1 == REHSTYLE) {
				if (contextcheck(C_SCORE | C_STAFF, "rehstyle parameter") == YES ) {
					used_check(Currstruct_p, REHSTYLE, "rehstyle");
					Currstruct_p->u.ssv_p->rehstyle = $3;
					Currstruct_p->u.ssv_p->used[REHSTYLE] = YES;
				}
			}
			else if ($1 == MEASNUMSTYLE) {
				if (contextcheck(C_SCORE, "measnumstyle parameter") == YES) {
					used_check(Currstruct_p, MEASNUMSTYLE, "measnumstyle");
					Currstruct_p->u.ssv_p->measnumstyle = $3;
					Currstruct_p->u.ssv_p->used[MEASNUMSTYLE] = YES;
				}
			}
		}
	}

	|
	T_PEDSTYLE T_EQUAL pedstyle
	{
		if (contextcheck(C_SCORE | C_STAFF, "pedstyle parameter") == YES
					&& Currstruct_p != 0) {
			used_check(Currstruct_p, PEDSTYLE, "pedstyle");
			Currstruct_p->u.ssv_p->pedstyle = $3;
			Currstruct_p->u.ssv_p->used[PEDSTYLE] = YES;
			/* Since Ped_stop is a predefined string that
			 * doesn't go through fix_string(), we wouldn't
			 * realize that font ZD1 was used until printing
			 * phase after we ought to have emitted the
			 * encoding vector code, which is too late,
			 * so mark it here. If the user sets the
			 * parameter to pedstar but then never actually uses
			 * a star, this will just cause a few extraenous
			 * but otherwise harmless lines of output. It is
			 * less important the ZI gets marked as used,
			 * since we don't change its encoding vector,
			 * so don't need to emit special code, but might as
			 * well mark that here as well. */
			if ($3 != P_LINE) {
				Fontinfo[font_index(FONT_ZD1)].was_used = YES;
				Fontinfo[font_index(FONT_ZI)].was_used = YES;
			}
		}
	}

	|
	T_TRANSPOSE T_EQUAL updown transpose_interval num trans_usage
	{
		char *trans_name;
		trans_name = ($1 == TRANSPOSITION ?
			"transpose" : "addtranspose");
		if (contextcheck(C_SCORE|C_STAFF, trans_name) == YES) {
			if (Context == C_STAFF && has_tab_staff() == YES) {
				l_warning(Curr_filename, yylineno,
					"%s not allowed on tablature staff; ignoring",
					trans_name);
			}
			chk_interval($4, $5);
			if (Currstruct_p != 0) {
				used_check(Currstruct_p, $1, trans_name);
				if ($1 == TRANSPOSITION) {
					Currstruct_p->u.ssv_p->inttype = $4;
					Currstruct_p->u.ssv_p->intnum = $3 * $5;
					Currstruct_p->u.ssv_p->trans_usage = $6;
				}
				else {
					Currstruct_p->u.ssv_p->addinttype = $4;
					Currstruct_p->u.ssv_p->addintnum = $3 * $5;
					Currstruct_p->u.ssv_p->addtrans_usage = $6;
				}
				Currstruct_p->u.ssv_p->used[$1] = YES;
				asgnssv(Currstruct_p->u.ssv_p);
			}
		}
	}

	|
	T_CUE T_EQUAL flagval
	{
		assign_int(CUE, $3, Currstruct_p);
	}

	|
	T_UNITS T_EQUAL T_UNITTYPE
	{
		assign_unit($3, Currstruct_p);
	}

	|
	T_STRVAR T_EQUAL string
	{
		assign_string($1, $3, Currstruct_p);
	}

	|
	T_OPTSTRVAR T_EQUAL opt_string
	{
		assign_string($1, $3, Currstruct_p);
	}

	|
	T_TUNINGVAR T_EQUAL T_TUNINGTYPE
	{
		if (contextcheck(C_SCORE, "tuning") == YES
				&& Currstruct_p != 0) {
			used_check(Currstruct_p, TUNING, "tuning");
			Currstruct_p->u.ssv_p->used[TUNING] = YES;
			Currstruct_p->u.ssv_p->tuning = $3;
			Tuning_used = YES;
		}
	}

	|
	T_KEYMAPVAR T_EQUAL opt_string
	{
		set_keymap($1, $3, Currstruct_p);
	}

	|
	T_POSTSCRIPT T_EQUAL string
	{
		if (Curr_usym_p == 0) {
			l_yyerror(Curr_filename, yylineno,
				"postscript= only valid in symbol context; maybe you meant without '=' ?");
		}
		else {
			if (Curr_usym_p->flags & US_POSTSCRIPT) {
				l_warning(Curr_filename, yylineno,
					"Multiple postscript definitions given for symbol '%s'; using last",
					Curr_usym_p->name);
				FREE(Curr_usym_p->postscript);
			}
			Curr_usym_p->postscript = $3;
			Curr_usym_p->flags |= US_POSTSCRIPT;
		}
	}

	|
	T_BBOX T_EQUAL value T_COMMA value T_COMMA value T_COMMA value
	{
		if (Curr_usym_p == 0) {
			l_yyerror(Curr_filename, yylineno,
				"bbox= only valid in symbol context");
		}
		else {
			if (Curr_usym_p->flags & US_BBOX) {
				l_warning(Curr_filename, yylineno,
					"Multiple bbox definitions given for symbol %s; using last",
					Curr_usym_p->name);
			}
			Curr_usym_p->llx = $3;
			Curr_usym_p->lly = $5;
			Curr_usym_p->urx = $7;
			Curr_usym_p->ury = $9;
			Curr_usym_p->flags |= US_BBOX;
		}
	}

	|
	T_STEMOFFSET T_EQUAL value T_COMMA value
	{
		if (Curr_usym_p == 0) {
			l_yyerror(Curr_filename, yylineno,
				"stemoffset= only valid in symbol context");
		}
		else {
			if (Curr_usym_p->flags & US_STEMOFFSET) {
				l_warning(Curr_filename, yylineno,
					"Multiple stemoffset definitions given for %s; last used",
					Curr_usym_p->name);
			}
			Curr_usym_p->upstem_y = $3;
			Curr_usym_p->downstem_y = $5;
			Curr_usym_p->flags |= US_STEMOFFSET;
		}
	}
	;

visval:
	T_WHEREUSED
	{
		/* whereused is not allowed on voice, just score and staff */
		if (contextcheck(C_SCORE|C_STAFF, "visible=whereused") == YES) {
			assign_int(VISIBLE, YES, Currstruct_p);
			if (Currstruct_p != 0 && Currstruct_p->u.ssv_p != 0) {
				Currstruct_p->u.ssv_p->hidesilent = YES;
			}
		}
	}

	|
	T_LETTER
	{
		/* only valid letter is y */
		if (*yytext != 'y') {
			yyerror("visible value must be y, n, or whereused");
		}
		else if (contextcheck(C_SSV, "visible parameter")
								== YES) {
			assign_int(VISIBLE, YES, Currstruct_p);
			if (Currstruct_p != 0 && Currstruct_p->u.ssv_p != 0) {
				Currstruct_p->u.ssv_p->hidesilent = NO;
			}
		}
	}

	|
	T_LET_N
	{
		if (contextcheck(C_SSV, "visible parameter") == YES) {
			assign_int(VISIBLE, NO, Currstruct_p);
			if (Currstruct_p != 0 && Currstruct_p->u.ssv_p != 0) {
				Currstruct_p->u.ssv_p->hidesilent = NO;
			}
		}
	};
	
opt_pageside:
	{
		$$ = PGSIDE_NOT_SET;
	}

	|
	T_PAGESIDE
	{
		$$ = $1;
	};

opt_print_clef:
	{
		$$ = NO;
	}

	|
	T_LETTER
	{
		/* only valid letter is y */
		if (*yytext == 'y') {
			$$ = YES;
		}
		else {
			/* Our yyerror will end up making this into a
			 * nice error message that gives the offending token */
			yyerror("syntax error");
			$$ = NO;
		}
	};

measnumval:
	T_LETTER
	{
		/* only valid letter is y */
		if (*yytext != 'y') {
			yyerror("measnum value must be y, n, or every N");
			$$ = MN_NONE;
		}
		else {
			$$ = MN_SCORE;
		}
	}

	|
	T_LET_N
	{
		$$ = MN_NONE;
	}

	|
	T_EVERY num
	{
		if ($2 < MINEVERYMEASNUM || $2 > MAXEVERYMEASNUM) {
			/* We have to do range check here rather than let
			 * assign_int() do it, because internally we use
			 * additional values, and it would be confusing to
			 * the user to include those internal values in the
			 * valid range. */
			l_yyerror(Curr_filename, yylineno,
				"for measnum=every N, N must be between %d and %d",
				MINEVERYMEASNUM, MAXEVERYMEASNUM);
			$$ = MN_NONE;
		}
		else {
			$$ = $2;
		}
	}
	;

vschemetype:
	{
		/* none specified */
		$$ = V_1;
	}

	|
	T_PITCH
	{
		/* only valid value here is 'f'  */
		if (*yytext == 'f') {
			$$ = V_2FREESTEM;
		}
		else {
			yyerror("invalid voice qualifier: must be f or o");
			$$ = V_1;
		}
	} 

	|
	T_LETTER
	{
		/* only valid value here is 'o' */
		if (*yytext == 'o') {
			$$ = V_2OPSTEM;
		}
		else {
			yyerror("invalid voice qualifier: must be f or o");
			$$ = (int) V_1;
		}
	};

allocbeamlist:
	{
		/* NULL token for allocating a new beamlist */
		new_beamlist();
	};

ratnumlist:
	/* empty */

	|
	nonempty_ratnumlist
	;

nonempty_ratnumlist:
	time_item
	{
		add_beamlist($1);
	}

	|
	subbeam

	|
	nonempty_ratnumlist T_COMMA time_item
	{
		add_beamlist($3);
	}

	|
	nonempty_ratnumlist T_COMMA subbeam
	;

subbeam:
	T_LPAREN { begin_subbeam(); } nonempty_ratnumlist T_RPAREN
	{
		end_subbeam();
	};

time_item: basic_time_val dots opt_more_time opt_m
	{
		$$ = calcfulltime($1, $2);

		/* If filling in a GRPSYL struct,
		 * need to fill in dots and basictime.
		 * Could also be here due to beamstyle
		 * or timeunit or location time offset,
		 * in which case the Curr_grpsyl_p will be NULL,
		 * or while getting tuplet duration. */
		if (Curr_grpsyl_p != (struct GRPSYL *) 0
						&& Getting_tup_dur == NO) {
			Curr_grpsyl_p->dots = $2;
			Curr_grpsyl_p->basictime = reconstruct_basictime((yyval.ratval));
		}
		else {
			struct TIMELIST *timelist_p;	/* walk thru list */
			struct TIMELIST *next_p;/* save next to delete curr */
			RATIONAL totaltime;	/* sum of extra times */

			/* Start with first time value (which was calculated
			 * above and is now in $$), then add on the added
			 * times, if any. */
			totaltime = $$;
			for (timelist_p = Extra_time_p; timelist_p != 0;
						timelist_p = next_p) {
				next_p = timelist_p->next;
				totaltime = radd(totaltime, timelist_p->fulltime);
				if (Doing_timeunit == NO) {
					/* For things like swingunit
					 * or tuplet time, we just need the
					 * resulting time, not the individual
					 * time units and arithmetic. */
					FREE(timelist_p);
				}
			}
			if (Extra_time_p != 0 && LE(totaltime, Zero)) {
				yyerror("Time values must add up to more than zero");
			}
			/* Current timelist is finished; can't append any
			 * more items to it. */
			Curr_timelist_p = 0;
			if (Doing_timeunit == YES) {
				/* For timeunit, we return the initial
				 * timeunit, but also preserve
				 * Extra_time_p for putting in SSV.
				 */
				$$ = calcfulltime($1, $2);
			}
			else {
				/* the list has been freed above */
				Extra_time_p = 0;
				$$ = totaltime;
			}
		}
	}

	|
	T_LET_M
	{
		$$.n = (svpath(1, TIME))->time.n;
		$$.d = (svpath(1, TIME))->time.d;
		if (Curr_grpsyl_p != (struct GRPSYL *) 0
					&& Getting_tup_dur == NO) {
			/* use whole note symbol as default */
			Curr_grpsyl_p->basictime = 1;
			Curr_grpsyl_p->is_meas = YES;
			User_meas_time = NO;
		}
		else {
			yyerror("'m' is not valid here");
		}
	};

swing_time:
	/* empty */
	{
		$$ = Zero;
	}

	|
	time_item
	;

opt_rs:
	/* empty */
	{
		$$ = NO;
	}

	|
	T_LET_R T_LET_S
	{
		$$ = (YES | (YES << 1));
	}

	|
	T_LET_S T_LET_R
	{
		$$ = ((YES << 1) | YES);
	}

	|
	T_LET_S
	{
		$$ = (YES << 1);
	}

	|
	T_LET_R
	{
		$$ = YES;
	};

alloc_rangelist:
	{
		/* null token for the purpose of allocating a new rangelist
		 * to be filled in with user data */
		new_staffset();
	};

rangelist:
	/* empty */

	|
	rangelistA
	;

rangelistA:
	rangeitem

	|
	rangelistA T_COMMA rangeitem
	;

rangeitem:	num rangetail grplabel
	{
		/* save information about this range in the list */
		/* if only a single number, not a range, rangetail will be
		 * 0, so use num as both beginning and end of range */
		add_staffset( $1, ($2 > 0 ? $2 : $1), String1, String2);
	};

rangetail:
	{
		/* no end of range, so return 0 as flag that there was only 1 number */
		$$ = 0;
	}

	|
	T_DASH num
	{
		/* make sure isn't 0, because that it illegal and would be used
		 * later as though there were no range. Other error checking
		 * on this number will be done later. */
		if ($2 == 0) {
			yyerror("second number of range may not be zero");
		}
		$$ = $2;
	};

grplabel:
	{
		/* no labels */
		String1 = String2 = (char *) 0;
	}

	|
	T_LPAREN glabel optlabel2 T_RPAREN
	{
		/* save label */
		String1 = $2;
	};

optlabel2:
	{
		/* no second label */
		String2 = (char *) 0;
	}

	|
	T_COMMA glabel
	{
		/* save label2 for later use */
		String2 = $2;
	};

glabel:	string
	;

alloc_barstlist:
	{
		/* NULL token for allocating space for a bar style list */
		new_barstlist();
		Bar_between = NO;
	};

barstlist:
	/* empty */

	|
	barst_item

	|
	barstlist T_COMMA opt_between barst_item

	|
	opt_between T_ALL
	{
		add_barst(1, MAXSTAFFS, Bar_between, YES);
	}
	;

opt_between:
	{
	}

	|
	between
	;

between:
	T_PLACE
	{
		if ($1 != PL_BETWEEN) {
			yyerror("barstyle range place can only be between");
		}
		else {
			Bar_between = YES;
		}
	}
	;

barst_item:
	num rangetail
	{
		/* if only 1 number, not range, rangetail will be 0, so use
		 * same number for beginning and end */
		add_barst($1, ($2 == 0 ? $1 : $2), Bar_between, NO);
	};

subbar_check:
	{
		used_check(Currstruct_p, SUBBARSTYLE, "subbarstyle");
		Bar_between = NO;
	};

subbar_list: alloc_subbar
	{
		set_barstlist(Currstruct_p, Subbar_app_p);
	}

	|
	subbar_list alloc_subbar sb_linetype sb_bartype sb_appearance alloc_sb_barstlist sb_barstlist T_TIME sb_timelist
	{
		set_barstlist(Currstruct_p, Subbar_app_p);
	};

alloc_subbar:
	{
		MALLOC(SUBBAR_APPEARANCE, Subbar_app_p, 1);
	};

alloc_sb_barstlist:
	{
		new_barstlist();
	};

sb_linetype:
	linetype
	{
		switch ($1) {
		case L_NORMAL:
		case L_DASHED:
		case L_DOTTED:
			break;
		default:
			l_yyerror(Curr_filename, yylineno,
				"subbar linetype, if specified, can only be dashed or dotted");
			break;
		}

		Subbar_app_p->linetype = $1;
	};

sb_bartype:
	T_BARTYPE
	{
		if ($1 != SINGLEBAR && $1 != DOUBLEBAR) {
			l_yyerror(Curr_filename, yylineno,
				"subbar type can only be bar or dblbar");
		}
		Subbar_app_p->bartype = $1;
	};

sb_appearance:
	{
		Subbar_app_p->upper_ref_line = LR_TOP;
		Subbar_app_p->upper_offset = 0.0;
		Subbar_app_p->lower_ref_line = LR_BOTTOM;
		Subbar_app_p->lower_offset = 0.0;
	}

	|
	between

	|
	T_LPAREN line_ref line_offset T_TO line_ref line_offset T_RPAREN
	{
		Subbar_app_p->upper_ref_line = $2;
		Subbar_app_p->upper_offset = $3;
		Subbar_app_p->lower_ref_line = $5;
		Subbar_app_p->lower_offset = $6;
	};

line_ref:
	T_BLOCKHEAD
	{
		switch ($1) {
		case C_TOP:
			$$ = LR_TOP;
			break;
		case C_BOT:
			$$ = LR_BOTTOM;
			break;
		default:
			/* our yyerror will turn this into more helpful msg */
			yyerror("syntax error");
			/* return some valid value */
			$$ = LR_MIDDLE;
			break;
		}
	}

	|
	T_MIDDLE
	{
		$$ = LR_MIDDLE;
	};

line_offset:
	{
		$$ = 0.0;
	}

	|
	plus_or_minus floatnum
	{
		$$ = $1 * $2;
	};

sb_barstlist:
	barst_item

	|
	sb_barstlist T_COMMA barst_item

	|
	T_ALL
	{
		add_barst(1, MAXSTAFFS, Bar_between, YES);
	};

sb_timelist:
	sb_count

	|
	sb_timelist T_COMMA sb_count
	;

sb_count:
	floatnum
	{
		struct SSV *ssv_p;
		int n;		/* to loop for checking for duplicates */

		if (Currstruct_p != 0) {
			ssv_p = Currstruct_p->u.ssv_p;
			if (ssv_p->nsubbar == 0) {
				MALLOC(SUBBAR_INSTANCE, ssv_p->subbarlist, 1);
			}
			else {
				REALLOC(SUBBAR_INSTANCE, ssv_p->subbarlist,
							ssv_p->nsubbar+1);
			}

			if ($1 <= 1.0 || $1 >= (double) Score.timenum + 1.0) {
				l_yyerror(Curr_filename, yylineno,
					"subbar time must be greater than 1.0 and less than %.3f",
							Score.timenum + 1.0);
			}
			for (n = 0; n < ssv_p->nsubbar; n++) {
				if (ssv_p->subbarlist[n].count == $1) {
					l_warning(Curr_filename, yylineno,
						"time %f specified more than once for same subbar instance", $1);
				}
			}
			ssv_p->subbarlist[ssv_p->nsubbar].count = $1;
			ssv_p->subbarlist[ssv_p->nsubbar].appearance_p = Subbar_app_p;
			(ssv_p->nsubbar)++;
		}
	};

repeatdots_value:
	T_ALL
	{
		$$ = RD_ALL;
	}

	|
	T_STANDARD
	{
		$$ = RD_STANDARD;
	};

mark_order:	T_ORDER
	{
		Order_place = $1;
		Order_prio = 1;
		if (Currstruct_p != 0) {
			switch ($1) {
			case PL_ABOVE:
				Currstruct_p->u.ssv_p->used[ABOVEORDER] = YES;
				break;
			case PL_BELOW:
				Currstruct_p->u.ssv_p->used[BELOWORDER] = YES;
				break;
			case PL_BETWEEN:
				Currstruct_p->u.ssv_p->used[BETWEENORDER] = YES;
				break;
			}
		}
	};

orderlist:
	orderitem
	{
		if (Currstruct_p != 0) {
			Currstruct_p->u.ssv_p->markorder[Order_place][$1] = Order_prio;
		}
	}
	
	|
	orderlist T_COMMA orderitem
	{
		if (Currstruct_p != 0) {
			Currstruct_p->u.ssv_p->markorder[Order_place][$3] = ++Order_prio;
		}
	}
	
	|
	orderlist T_AMPERSAND orderitem
	{
		if (Currstruct_p != 0) {
			Currstruct_p->u.ssv_p->markorder[Order_place][$3] = Order_prio;
		}
	};

orderitem:	marktype
	{
		if (Currstruct_p != 0) {
			if (Currstruct_p->u.ssv_p->markorder[Order_place][$1] != 0) {
				l_yyerror(Curr_filename, yylineno,
					"order item %s specified more than once", $1);
			}
		}
		$$ = $1;
	};

marktype:
	T_MUSSYM
	{
		$$ = MK_MUSSYM;
	}

	|
	T_OCTAVE
	{
		$$ = MK_OCTAVE;
	}

	|
	T_OTHERTEXT
	{
		$$ = MK_OTHERTEXT;
	}

	|
	T_MODIFIER
	{
		switch ($1) {
		case TM_CHORD:
			$$ = MK_CHORD;
			break;
		case TM_DYN:
			$$ = MK_DYN;
			break;
		default:
			yyerror("invalid text modifier in order list");
			/* set to something valid, since $$ will be used
			 * as an array subscript. */
			$$ = MK_CHORD;
			break;
		}
	}

	|
	T_LYRICS
	{
		$$ = MK_LYRICS;
	}

	|
	T_ENDING
	{
		$$ = MK_ENDING;
	}

	|
	T_REHEARSAL
	{
		$$ = MK_REHEARSAL;
	}

	|
	T_PEDAL
	{
		$$ = MK_PEDAL;
	} ;

timesig:
	tsiglist opt_ts_visibility
	{
		tsig_item(TSR_END);
		if (Currstruct_p != 0 && Currstruct_p->u.ssv_p != 0) {
			MALLOCA(char, Currstruct_p->u.ssv_p->timerep, Tsig_offset);
			strcpy(Currstruct_p->u.ssv_p->timerep, Timerep);
			assign_timesig(Currstruct_p, $2, &Next_alt_timesig);
			Tsig_visibility = $2;
			/* If there are alternating time signatures,
			 * need to save pointer to this list */
			if (Next_alt_timesig != 0) {
				Alt_timesig_list = Currstruct_p->u.ssv_p->timerep;
			}
			else {
				Alt_timesig_list = 0;
			}
		}
		/* Reset for next time signature usage, if any */
		Tsig_offset = 0;
	};

tsiglist:
	meas_tsig

	|
	tsiglist { tsig_item(TSR_ALTERNATING); } meas_tsig
	{
		/* this is for alternating time signature */
	};

meas_tsig:
	fraction
	{
	}

	|
	meas_tsig T_PLUS { tsig_item(TSR_ADD); } fraction
	{
		/* This would be for things like 2/4 + 3/8 */
	};

fraction:
	T_CUT
	{
		tsig_item(TSR_CUT);
	}

	|
	T_COMMON
	{
		tsig_item(TSR_COMMON);
	}

	|
	numerator T_SLASH num
	{
		tsig_item(TSR_SLASH);
		if ($3 < MINDENOMINATOR) {
			l_yyerror(Curr_filename, yylineno,
				"time signature denominator cannot be less than %d",
				MINDENOMINATOR);
			$3 = MINDENOMINATOR;
		}
		else if ($3 > MAXDENOMINATOR) {
			l_yyerror(Curr_filename, yylineno,
				"time signature denominator cannot be greater than %d",
				MAXDENOMINATOR);
			$3 = MAXDENOMINATOR;
		}
		else if (power_of2check($3, "time signature denominator") == NO) {
			$3 = 1 << drmo($3);
		}

		tsig_item($3);
	}

	|
	numerator T_PLUS T_MULTIWHOLE
	{
		/* this is something like 3+1/4 where the 1/4 get interpreted
		 * by lexer as quadwhole, but it's really 1 for numerator
		 * and 4 for denominator. 
		 */
		tsig_item(1);
		tsig_item(TSR_SLASH);
		tsig_item($3);
	}

	|
	T_MULTIWHOLE
	{
		tsig_item(1);
		tsig_item(TSR_SLASH);
		tsig_item($1);
	};

numerator:
	numerator_value

	|
	numerator T_PLUS numerator_value
	;

numerator_value: num
	{
		if ($1 < MINNUMERATOR) {
			l_yyerror(Curr_filename, yylineno,
				"time signature numerator cannot be less than %d",
				MINNUMERATOR);
			$1 = MINNUMERATOR;
		}
		else if ($1 > MAXNUMERATOR) {
			l_yyerror(Curr_filename, yylineno,
				"time signature numerator cannot be greater than %d",
				MAXNUMERATOR);
			$1 = MAXNUMERATOR;
		}
		tsig_item($1);
	};

opt_ts_visibility:
	/* empty */
	{
		$$ = PTS_ONCE;
	}

	|
	flagval
	{
		$$ = ($1 == YES ? PTS_ALWAYS : PTS_NEVER);
	};
	
opt_n:
	{
		$$ = NO;
	}

	|
	T_LET_N
	{
		/* 'n' is used in various places to mean do NOT
		 * print something that would normally be printed,
		 * so YES means we do want to inhibit the normal printing. */
		$$ = YES;
	};

printed_timesig:
	meas_tsig
	{
		tsig_item(TSR_END);
		if (Currstruct_p != 0 && Currstruct_p->u.ssv_p != 0) {
			MALLOCA(char, Currstruct_p->u.ssv_p->prtime_str1, Tsig_offset);
			strcpy(Currstruct_p->u.ssv_p->prtime_str1, Timerep);
			Currstruct_p->u.ssv_p->prtime_str2 = 0;
			Currstruct_p->u.ssv_p->prtime_is_arbitrary = NO;
			Currstruct_p->u.ssv_p->used[PRINTEDTIME] = YES;
		}
		/* Reset for next time signature usage, if any */
		Tsig_offset = 0;
	}

	|
	string
	{
		if (Currstruct_p != 0 && Currstruct_p->u.ssv_p != 0) {
			Currstruct_p->u.ssv_p->prtime_str1 = $1;
			Currstruct_p->u.ssv_p->prtime_str2 = 0;
			Currstruct_p->u.ssv_p->prtime_is_arbitrary = YES;
			Currstruct_p->u.ssv_p->used[PRINTEDTIME] = YES;

		}
	}

	|
	string string
	{
		if (Currstruct_p != 0 && Currstruct_p->u.ssv_p != 0) {
			Currstruct_p->u.ssv_p->prtime_str1 = $1;
			Currstruct_p->u.ssv_p->prtime_str2 = $2;
			Currstruct_p->u.ssv_p->prtime_is_arbitrary = YES;
			Currstruct_p->u.ssv_p->used[PRINTEDTIME] = YES;
		}
	};

keyspec: num acc_symbol majmin
	{
		if ( ($2 != '#') && ($2 != '&') ) {
			yyerror("key signature must include # or &");
		}
		else {
			assign_key($1, $2, $3, Currstruct_p);
		}
	}

	|
	pitch opt_accidental majmin
	{
		int sharps;

		/* Get number of sharps for the pitch letter */
		sharps = strchr(Circle, $1) - Circle - 1;

		/* adjust for sharp/flat */
		if ($2 == '#') {
			sharps += 7;
		}
		else if ($2 == '&') {
			sharps -= 7;
		}
		else if ($2 != 0) {
			yyerror("key letter can only be followed by # or &");
			/* set to some ridiculous value, so will fail below */
			sharps = 999;
		}

		/* Adjust if needed for minor */
		if ($3 == YES) {
			sharps -= 3;
		}

		/* disallow illegal keys, like f& */
		if (abs(sharps) > 7) {
			/* print error unless already printed one above */
			if (sharps < 100) {
				yyerror("invalid key--too many sharps or flats");
			}
		}
		else {
			assign_key(abs(sharps), (sharps >= 0 ? '#' : '&'),
							$3, Currstruct_p);
		}
	};

useaccs_value:
	flagval useaccs_qual
	{
		if ($1 == NO) {
			if ($2 != -1) {
				l_warning(Curr_filename, yylineno,
					"can't use useaccs qualifier with n; ignoring the qualifier");
			}
			$$ = UA_N;
		}
		else {
			if ($2 == -1) {
				$2 = UA_Y_NONE;
			}
			$$ = $2;
		}
	}
	;

useaccs_qual:
	{
		/* Empty means UA_Y_NONE when used with 'y', but we need
		 * to distinguish temporarily, in case user erroneously
		 * tries to use std with 'n' */
		$$ = -1;
	}

	|
	T_NONE
	{
		$$ = $1;
	}

	|
	T_ALL
	{
		$$ = UA_Y_ALL;
	}

	|
	T_NONNAT
	{
		$$ = UA_Y_NONNAT;
	}
	;

stafflinedef:
	num opt_n
	{
		asgn_stafflines($1, $2 == YES ? SS_NOTHING : SS_NORMAL, Currstruct_p);
	}

	|
	num T_DRUM
	{
		asgn_stafflines($1, SS_DRUM, Currstruct_p);
	}

	|
	T_TAB prclef
	{
		/* use default tablature */
		if (Currstruct_p != 0) {
			Currstruct_p->u.ssv_p->strinfo = Guitar;
			asgn_stafflines(DEFTABLINES, $2, Currstruct_p);
		}
	}

	|
	T_TAB T_LPAREN tab_string_list T_RPAREN prclef
	{
		/* user-specified tablature */
		asgn_stafflines($3, $5, Currstruct_p);
	};

prclef:
	{
		$$ = PTC_FIRST;
	}

	|
	T_LET_N
	{
		$$ = PTC_NEVER;
	}

	|
	T_LETTER
	{
		/* only valid letter is y */
		if (*yytext != 'y') {
			yyerror("flag value must be y or n");
		}
		$$ = PTC_ALWAYS;
	};

ch_tran_type:	/* empty */
	{
		$$ = CT_NONE;
	}

	|
	string
	{
		if (strcmp($1 + 2, "German") == 0) {
			$$ = CT_GERMAN;
		}
		else {
			parse_doremi_string($1 + 2);
			$$ = CT_DOREMI;
		}
	};

tab_string_list:
	tab_item
	{
		$$ = 1;
	}

	|
	tab_string_list tab_item
	{
		/* count up the number of strings */
		$$ = $1 + 1;
	};

tab_item:	pitch opt_str_acc opt_ticks opt_octave
	{
		/* save info about this tablature string */
		if (Currstruct_p != 0) {
			add_tab_string_info($1, $2, $3, $4, Currstruct_p->u.ssv_p);
		}
	};

opt_str_acc:
	{
		$$ = 0;
	}

	|
	acc_symbol
	{
		if ($1 != '&' && $1 != '#') {
			yyerror("accidental on string can only be & or #");
		}
	};

opt_ticks:
	{
		/* no ticks */
		$$ = 0;
	}

	|
	T_TICKS
	{
		/* limit number of ticks. Has to match the number of bits
		 * used by TMP_NTICKS in grpsyl.c */
		if ($1 > MAXTICKS) {
			l_yyerror(Curr_filename, yylineno,
				"too many ' marks (%d max)", MAXTICKS);
			$1 = MAXTICKS;
		}
	};

flagval:	T_LETTER
	{
		/* only valid letter is y */
		if (*yytext != 'y') {
			yyerror("flag value must be y or n");
		}
		$$ = YES;
	}

	|
	T_LET_N
	{
		$$ = NO;
	};

minscsep:	floatnum
	{
		if (Currstruct_p == 0) {
			l_warning(Curr_filename, yylineno,
				"wrong context for setting scoresep; ignoring");
		}
		else {
			/* Assign the value of minimum vertical space
			 * between scores. */
			assign_float(MINSCSEP, $1, Currstruct_p);
		}
	};

maxscsep:
	{
		if (Currstruct_p != 0) {
			/* if user set minimum greater than default maximum,
			 * adjust maximum to equal the minimum
			 * they specified. */
			if (Currstruct_p->u.ssv_p->minscsep
					> Currstruct_p->u.ssv_p->maxscsep) {
				Currstruct_p->u.ssv_p->maxscsep
					= Currstruct_p->u.ssv_p->minscsep;
			}
			else {
				/* fill in default */
				assign_float(MAXSCSEP, DEFMAXSCSEP, Currstruct_p);
			}
		}
	}

	|
	T_COMMA floatnum
	{
		if (Currstruct_p != 0) {
			if (Currstruct_p->u.ssv_p->minscsep > $2) {
				yyerror("scoresep maximum smaller than minimum");
			}
			assign_float(MAXSCSEP, $2, Currstruct_p);
		}
	};

minscpad:	opt_minus floatnum
	{
		if (Currstruct_p == 0) {
			l_warning(Curr_filename, yylineno,
				"wrong context for setting scorepad; ignoring");
		}
		else {
			/* Assign the value of minimum vertical space
			 * between scores. */
			assign_float(MINSCPAD, $1 * $2, Currstruct_p);
		}
	}
	;

maxscpad:
	{
		if (Currstruct_p != 0) {
			/* If user set minimum greater than default maximum,
			 * adjust maximum to equal the minimum
			 * they specified. */
			if (Currstruct_p->u.ssv_p->minscpad
					> Currstruct_p->u.ssv_p->maxscpad) {
				Currstruct_p->u.ssv_p->maxscpad
					= Currstruct_p->u.ssv_p->minscpad;
			}
			else {
				/* fill in default */
				assign_float(MAXSCPAD, DEFMAXSCPAD, Currstruct_p);
			}
		}
	}

	|
	T_COMMA opt_minus floatnum
	{
		if (Currstruct_p != 0) {
			if (Currstruct_p->u.ssv_p->minscpad > $2 * $3) {
				yyerror("scorepad maximum smaller than minimum");
			}
			assign_float(MAXSCPAD, $2 * $3, Currstruct_p);
		}
	}
	;

shorten_opts:	/* empty */
	{
	}

	|
	T_COMMA floatnum shorten_range
	{
		assign_float(MAXPROSHORT, $2, Currstruct_p);
	};

shorten_range:	/* empty */
	{
	}

	|
	T_COMMA value T_COMMA value
	{
		if ($2 > $4) {
			l_warning(Curr_filename, yylineno,
				"distance to begin shortening protruding stems cannot be greater than ending distance; making them equal");
			$4 = $2;
		}
		assign_int(BEGPROSHORT, $2, Currstruct_p);
		assign_int(ENDPROSHORT, $4, Currstruct_p);
	};

inputdir:
	T_ANY
	{
		$$ = UNKNOWN;
	}

	|
	dir
	{
		$$ = $1;
	};

pagesize:
	T_PAGESIZE

	|
	pitch num
	{
		if ($1 == (int) 'a' && $2 >= 4 && $2 <= 6) {
			switch ($2) {
			case 4:
				$$ = PS_A4;
				break;
			case 5:
				$$ = PS_A5;
				break;
			case 6:
				$$ = PS_A6;
				break;
			default:
				pfatal("impossible pagesize");
				break;
			}
		}
		else {
			yyerror("unsupported pagesize");
		}
	}
	;

orientation:
	/* empty */
	{
		$$ = 0;
	}

	|
	T_ORIENTATION
	{
		$$ = ($1 == O_PORTRAIT ? 0 : 1);
	}
	;

vcombine_value:
	/* empty */
	{
		assign_vcombine(VC_NOOVERLAP, Currstruct_p);
	}

	|
	voice_spec opt_vcombine_qualifier
	{
		assign_vcombine($2, Currstruct_p);
		Defining_multiple = NO;
	}
	;

opt_vcombine_qualifier:
	/* empty */
	{
		$$ = VC_NOOVERLAP;
	}

	|
	T_VCOMBVAL
	{
		$$ = $1;
	}
	;

end_type:
	T_ENDTYPE
	{
		$$ = $1;
	}

	|
	T_BLOCKHEAD
	{
		if ($1 != C_TOP) {
			l_yyerror(Curr_filename, yylineno,
					"%s not valid here", yytext);
		}
		$$ = ENDING_TOP;
	};

updown:	T_UP
	{
		$$ = 1;
	}

	|
	T_DOWN
	{
		$$ = -1;
	};

majmin:
	{
		$$ = NO;
	}

	|
	T_XPOS_INT
	{
		switch ($1) {
		case MINOR:
			$$ = YES;
			break;
		case MAJOR:
			$$ = NO;
			break;
		default:
			$$ = NO;
			yyerror("must be major or minor");
			break;
		}
	};

pedstyle:	T_LINE
	{
		$$ = P_LINE;
	}

	|
	T_ALT T_PEDSTAR
	{
		$$ = P_ALTPEDSTAR;
	}

	|
	T_PEDSTAR
	{
		$$ = P_PEDSTAR;
	};

transpose_interval: T_XPOS_INT
	;

trans_usage:
	{
		$$ = TR_BOTH;
	}

	|
	T_TRANSUSAGE
	{
		$$ = $1;
	};
	
title:	T_TITLE tfont title_size titleA opt_semi T_NEWLINE
	{
	};

tfont:	font_family title_font
	{
		if ($2 == FONT_UNKNOWN) {
			$2 = Curr_font;
		}
		if ($1 == FAMILY_DFLT) {
			/* user didn't specify a family. Use the current
			 * family, unless we don't have one yet, in which
			 * case look it up */
			if (Curr_family == FAMILY_DFLT) {
				Curr_family = Score.fontfamily;
			}
			Titlefont =  Curr_family + $2;
		}
		else {
			Titlefont = $1 + $2;
		}
	};

title_font:
	{
		$$ = FONT_UNKNOWN;
	}

	|
	T_FONT
	{
		$$ = $1;
	};

font_family:
	{
		$$ = FAMILY_DFLT;
	}

	|
	T_FFAMILY {
		$$ = $1;
	};

title_size:
	{
		Titlesize = Curr_size;
	}

	|
	T_LPAREN num T_RPAREN
	{
		Titlesize = $2;
	};

titleA:
	string 
	{
		String1 = $1;
		Extra = extra_needed(Titlefont, Titlesize, &String1);
		proc_printcmd(J_CENTER, (struct INPCOORD *) 0, String1,
			Titlefont, Titlesize, YES, NO, PU_NORMAL, NO, 0, Extra);
	}

	|
	string string
	{
		double tmp_extra;

		/* If one is taller than the default for the font/size,
		 * figure out how much more to add on. */
		String1 = $1;
		Extra = extra_needed(Titlefont, Titlesize, &String1);
		String2 = $2;
		tmp_extra = extra_needed(Titlefont, Titlesize, &String2);
		Extra = MAX(Extra, tmp_extra);

		proc_printcmd(J_LEFT, (struct INPCOORD *) 0, String1,
				Titlefont, Titlesize, YES, NO, PU_NORMAL,
				NO, 0, Extra);
		proc_printcmd(J_RIGHT, (struct INPCOORD *) 0, String2,
				Titlefont, Titlesize, NO, NO, PU_NORMAL,
				NO, 0, (double) 0.0);
	}

	|
	string string string
	{
		double tmp_extra;
		char *string3;

		String1 = $1;
		Extra = extra_needed(Titlefont, Titlesize, &String1);
		String2 = $2;
		tmp_extra = extra_needed(Titlefont, Titlesize, &String2);
		Extra = MAX(Extra, tmp_extra);
		string3 = $3;
		tmp_extra = extra_needed(Titlefont, Titlesize, &string3);
		Extra = MAX(Extra, tmp_extra);

		proc_printcmd(J_LEFT, (struct INPCOORD *) 0, String1,
				Titlefont, Titlesize, YES, NO, PU_NORMAL,
				NO, 0, Extra);
		proc_printcmd(J_CENTER, (struct INPCOORD *) 0, String2,
				Titlefont, Titlesize, NO, NO, PU_NORMAL,
				NO, 0, (double) 0.0);
		proc_printcmd(J_RIGHT, (struct INPCOORD *) 0, string3,
				Titlefont, Titlesize, NO, NO, PU_NORMAL,
				NO, 0, (double) 0.0);
	};

paragraph:	opt_paratype T_PARAGRAPH tfont title_size string opt_semi T_NEWLINE
	{
		char *p;		/* pointer through the string */
		int backslash_count;	/* how many backslashes in a row */
		int font, size;		/* at end of \n-delimited segment */
		char *string_start;	/* where current segment begins */

		/* Convert unescaped input newlines to spaces.
		 * The +2 in the 'for' initialization is to skip font/size.
		 */
		backslash_count = 0;
		string_start = $5;
		/* If we are running on a file using \r or \r\n for
		 * line separator, normalize to \n instead */
		for (p = string_start + 2; *p != 0; p++) {
			if (*p == '\r') {
				if ( *(p+1) != '\n') {
					/* \r by itself. Use \n instead */
					*p = '\n';
				}
				else {
					/* delete the \r */
					char *src, *dest;
					for (src = p + 1, dest = p;  ; src++, dest++) { 
						*dest = *src;
						if (*src == '\0') {
							break;
						}
					}
				}
			}
		}

		font = Titlefont;
		size = Titlesize;
		for (p = string_start + 2; *p != 0; p++) {
			/* Real newlines not preceded by an odd number
			 * of backslashes are converted to spaces. */
			if (*p == '\n' && (backslash_count & 0x1) == 0) {
				*p = ' ';
			}

			/* Count up number of consecutive backslashes.
			 * Also, if user explicitly uses \n, split into
			 * a new paragraph
			 */
			if (*p == '\\') {
				backslash_count++;
				if (((backslash_count & 0x1) == 1)
						&& ( *(p+1) == 'n') ) {
					/* Need to split.
					 * Terminate the current string
					 * where we are in it, and process it,
					 * then arrange to continue processing
					 * on the rest of the string.
					 */
					*p = '\0';
					(void) fix_string(string_start,
						font, size,
						Curr_filename, yylineno);
					String1 = map_print_str(strdup(string_start), Curr_filename, yylineno);
					proc_printcmd($1, (struct INPCOORD*) 0,
						String1, font, size,
						YES, NO, PU_NORMAL, NO, 0, 0.0);

					/* The user could change font/size
					 * in mid-string with \f() and/or \s().
					 * so we have to determine what they
					 * are at the end of the current segment
					 * and use that for making the copy
					 * of the remainder of the string.
					 * The original copy of the remainder
					 * of the string gets "memory leaked,"
					 * but this is a rare case, so not
					 * worth worrying about.
					 */
					end_fontsize(String1, &font, &size);
					string_start = copy_string(p+2, font, size);
					p++;
				}
			}
			else {
				backslash_count = 0;
			}
		}
		(void) fix_string(string_start, font, size,
					Curr_filename, yylineno);
		String1 = map_print_str(strdup(string_start), Curr_filename, yylineno);
		proc_printcmd($1, (struct INPCOORD*) 0, String1,
				font, size, YES, NO, PU_NORMAL, NO, 0, 0.0);
	};

opt_paratype:
	/* empty */
	{
		/* Use same paragraph time as last time (or default if
		 * this is the first paragraph ever in this song.) */
		$$ = Curr_paratype;
	}

	|
	T_PARATYPE
	{
		Curr_paratype = $1;
		$$ = $1;
	};

paramname:
	T_NUMVAR

	|
	T_YESNOVAR

	|
	T_SCORESEP

	|
	T_STAFFLINES

	|
	T_TIMEUNIT

	|
	T_SWINGUNIT

	|
	T_PAD

	|
	T_BARSTLISTVAR

	|
	T_FNUMVAR

	|
	T_2FNUMVAR

	|
	T_FIRSTPAGE

	|
	T_INPUTDIR

	|
	T_SHORTEN

	|
	T_REPEATDOTS

	|
	T_UNITS

	|
	T_ENDSTYLE

	|
	T_REH_STYLE

	|
	T_MEASNUM

	|
	T_PEDSTYLE

	|
	T_VVAR

	|
	T_VCOMBINE

	|
	T_SCOREPAD

	|
	T_RANGELISTVAR

	|
	T_SUBBARSTYLE

	|
	T_ORDER

	|
	T_USEACCS

	|
	T_KEY

	|
	T_TIME

	|
	T_PRINTEDTIME

	|
	T_TRANSPOSE

	|
	T_CHORDTRANSLATION

	|
	T_ALIGNLABELS

	|
	T_RATNUMLISTVAR

	|
	T_VISVAR

	|
	T_STRVAR

	|
	T_CUE

	|
	T_OPTSTRVAR

	|
	T_TUNINGVAR

	|
	T_KEYMAPVAR

	|
	T_FONTVAR

	|
	T_FAMILY

	|
	T_CLEFVAR
	;

string:	T_STRING
	{
		/* strip the quotes from the string and make a copy for later use */
		if (Curr_family == FAMILY_DFLT) {
			Curr_family = Score.fontfamily;
		}
		$$ = copy_string(stripquotes(yytext), Curr_family + Curr_font,
								Curr_size);
	}

	|
	string T_PLUS T_STRING
	{
		char *old_string;

		/* append new string part to existing part */
		old_string = $1;
		/* new string part has quotes, so subtract 2 from needed length,
		 * but need space for null, so overall need 1 less. */
		MALLOCA(char, $$, strlen(old_string) + strlen(yytext) - 1);
		sprintf($$, "%s%s", old_string, stripquotes(yytext));
		FREE(old_string);
	};

music_input:	noteinfo T_NEWLINE
	{
		(void) contextcheck(C_MUSIC, "music information");
		tssv_line();
		/* reset flags for next voice */
		Defining_multiple = NO;
		Curr_grpsyl_p = (struct GRPSYL *) 0;
		Good_till_canceled = NO;
		free_extra_time();
		Prev_grpsyl_p = 0;
	};

noteinfo:	init_staff sv_spec notecolon groupinfo
	{
		/* Here's the deal: the staff/voice specification
		 * could be lists of staff ranges and/or voices.
		 * We need a separate copy of the groupinfo
		 * for each staff/voice. So as we parse the sv_spec,
		 * we remember which staffs and voices are specified,
		 * and keep a pointer to the first STAFF in the measure.
		 * Then after collecting the groupinfo,
		 * we make copies of the group info for each staff/voice given.
		 * It has to be copies rather than just a pointer to the
		 * same stuff, because the coordinates will be different
		 * for each staff/voice. */

		/* attach the groupinfo to each of the STAFF structs */
		link_groups();
	}

	|
	T_MULTIREST num opt_semi
	{
		add_multirest($2);
		/* If there are alternating time signatures, figure out
		 * which one should be in effect at the end of the multirest */
		if (Alt_timesig_list != 0) {
			int m;

			/* The Next_alt_timesig is already pointing to
			 * the value for after the first measure in the
			 * multirest, so have to subtract 1 here */
			for (m = 0; m < $2 - 1; m++) {
				if (Next_alt_timesig == 0) {
					/* Shouldn't really ever get here,
					 * since other code should do the
					 * wrap-around, but just in case... */
					Next_alt_timesig = Alt_timesig_list;
					continue;
				}
				do {
					Next_alt_timesig++;
					if (*Next_alt_timesig == TSR_END) {
						Next_alt_timesig = Alt_timesig_list;
						break;
					}
				} while (*Next_alt_timesig != TSR_ALTERNATING);
			}
			if (*Next_alt_timesig == TSR_ALTERNATING) {
				Next_alt_timesig++;
			}
		}
		Currstruct_p = (struct MAINLL *) 0;
	}

	|
	lyricinfo
	{
		free_staffrange();
	};

init_staff:
	{
		begin_range(PL_UNKNOWN);
	};

sv_spec:	voice_id
	{
		Chord_at_a_time = NO;
	}

	|
	sv_map_list
	{
		Chord_at_a_time = YES;
	}
	;

sv_map_list:	sv_map_entry

	|
	sv_map_list sv_map_entry
	;

sv_map_entry:	map_start sv_map_item_list map_finish
	;

map_start:	T_LBRACKET
	{
		begin_map();
	};

map_finish:	T_RBRACKET
	{
		end_map();
	};

sv_map_item_list:	voice_id
	{
		map_item();
	}

	|
	sv_map_item_list T_SEMICOLON voice_id
	{
		map_item();
		Defining_multiple = YES;
	};

voice_id:	sv_item
	{
		/* initialize grpsyl info */
		Curr_grpsyl_p = Last_grpsyl_p = (struct GRPSYL *) 0;
	}

	|
	voice_id sv_and sv_item
	;

sv_and:	T_AMPERSAND
	{
		/* prepare for a new set of ranges */
		begin_range(Place);
		Defining_multiple = YES;
	};

sv_item:	grp_stafflist
	{
		/* data is for staff with only one voice */
		save_vno_range(1, 1);
		add_to_sv_list();
	}

	|
	grp_stafflist voice_spec
	{
		if ($2 == YES) {
			add_to_sv_list();
		}
	}
	;

notecolon:
	T_COLON
	{
		/* set flag as to whether we are processing tab staff
		 * information or normal staff info */
		Doing_tab_staff = is_tab_range();
	};

grp_stafflist:	grp_staff_item

	|
	grp_stafflist T_COMMA grp_staff_item
	{
		/* several staffs have the same note data */
		Defining_multiple = YES;
	};

grp_staff_item:	staffnum grp_staff_range
	{
		/* remember which staffs we are currently getting data for */
		/* if only a single number, not a range, staff_range will be
		 * 0, so use staffnum as high and low of range */
		save_staff_range($1, ($2 == 0 ? $1 : $2));
	};

grp_staff_range:
	{
		/* empty - no range, just a single number */
		$$ = 0;
	}

	|
	T_DASH staffnum
	{
		Defining_multiple = YES;
		$$ = $2;
		if ($2 == 0) {
			yyerror("staff of 0 is illegal");
		}
	};

voice_spec:	
	voicenum voice_tail
	{
		/* note data applies to a single voice */
		if (rangecheck($1, MINVOICES, MAXVOICES, "voice") == YES) {
			save_vno_range($1, $1);
			$$ = YES;
		}
		else {
			$$ = NO;
		}
	}

	|
	voicenum T_DASH voicenum voice_tail
	{
		/* note data applies to range of voices */
		if (rangecheck($1, MINVOICES, MAXVOICES, "voice") == YES &&
				rangecheck($3, MINVOICES, MAXVOICES, "voice") == YES) {
			if ($3 < $1) {
				l_yyerror(Curr_filename, yylineno,
					"end of voice range is smaller than beginning");
				$$ = NO;
			}
			else {
				save_vno_range($1, $3);
				Defining_multiple = YES;
				$$ = YES;
			}
		}
		else {
			$$ = NO;
		}
	};

voice_tail:
	/* empty */

	|
	T_COMMA voice_spec
	{
		Defining_multiple = YES;
	};

groupinfo:	groupdata

	|
	groupinfo groupdata
	;

groupdata:
	ntuplet

	|
	mid_meas_param grp_attributes group other_attr T_SEMICOLON
	{
		/* Associate any TIMEDSSVs with this group */
		if (Curr_tssv_p != 0) {
			tssv_setgrpsyl(Curr_grpsyl_p);
			Curr_tssv_p = 0;
		}

		/* if no custom beaming indicated, fill in based on previous. */
		if ( Curr_grpsyl_p->beamloc == NOITEM) {
			setbeamloc(Curr_grpsyl_p, Last_grpsyl_p);
		}

		/* later we check that if there is an 'm', it is
		 * the only thing in the measure */

		link_notegroup(Curr_grpsyl_p, Last_grpsyl_p);
		Curr_marklist = 0;
		/* Save pointer to this group, in case there are additive
		 * times, and the next group gets its time value from this
		 * group. In that case, we need this group, not the last one
		 * of the added groups. */
		Prev_grpsyl_p = Curr_grpsyl_p;
		/* Add groups if there were additive time values. */
		if (Extra_time_p != 0) {
			Curr_grpsyl_p = expandgrp(Curr_grpsyl_p, Extra_time_p);
			/* If there was a custom beam ebm on something with
			 * additive time, we need make sure ENDITEM is on only
			 * the last of the expanded groups. */
			if (Curr_grpsyl_p != Prev_grpsyl_p
					&& Prev_grpsyl_p->beamloc == ENDITEM) {
				struct GRPSYL *g_p;
				for (g_p = Prev_grpsyl_p; g_p != Curr_grpsyl_p;
							g_p = g_p->next) {
					g_p->beamloc = INITEM;
				}
				Curr_grpsyl_p->beamloc = ENDITEM;
			}
			Last_grpsyl_p = Curr_grpsyl_p->prev;
		}
		Curr_timelist_p = 0;
	};

mid_meas_param:
	/* empty */

	|
	mid_meas_list
	;

mid_meas_list:
	mid_meas_item

	|
	mid_meas_list mid_meas_item
	;

mid_meas_item:
	T_L_DBLANGLE mm_context mm_params T_R_DBLANGLE
	{
		if (Chord_at_a_time == YES) {
			yyerror("mid-measure changes not allowed on chord-at-a-time input");
		}
	}
	;

mm_context:
	T_SCORE
	{
		Curr_tssv_p = tssv_create(C_SCORE);
	}

	|
	T_STAFF
	{
		Curr_tssv_p = tssv_create(C_STAFF);
	}

	|
	T_VOICE
	{
		Curr_tssv_p = tssv_create(C_VOICE);
	}
	;

mm_params:
	mm_param_item

	|
	mm_params T_SEMICOLON mm_param_item
	;

mm_param_item:
	T_CLEFVAR T_EQUAL T_CLEF
	{
		if (Curr_grpsyl_p != 0 && Curr_grpsyl_p->grpsyl == GS_GROUP
				&& Curr_grpsyl_p->grpvalue == GV_ZERO) {
			yyerror("mid-measure clef change not allowed after grace note\n");
		}
		tssv_update(Curr_tssv_p, $1, $3);
	}

	|
	T_NUMVAR T_EQUAL num
	{
		switch ($1) {

		case DEFOCT:
		case RELEASE:
			tssv_update(Curr_tssv_p, $1, $3);
			break;

		default:
			l_warning(Curr_filename, yylineno,
				"parameter type cannot be changed mid-measure; ignoring");
			break;
		}
	}

	|
	T_YESNOVAR T_EQUAL flagval
	{
		switch ($1) {

		case ALIGNRESTS:
			tssv_update(Curr_tssv_p, $1, $3);
			break;

		default:
			l_warning(Curr_filename, yylineno,
				"parameter type cannot be changed mid-measure; ignoring");
			break;
		}
	}
	;

grp_attributes:	styleinfo time_val
	;

styleinfo:
	alloc_grpsyl
	{
		if (Good_till_canceled == NO) {
			/* use default attributes */
			Curr_grpsyl_p->grpvalue = GV_NORMAL;
			Curr_grpsyl_p->grpsize = GS_NORMAL;
			Curr_grpsyl_p->headshape = HS_UNKNOWN;
		}
		else {
			/* re-use previous style */
			copy_attributes(Curr_grpsyl_p, Last_grpsyl_p);
			Curr_grpsyl_p->with_was_gtc = YES;
		}
	}

	|
	T_LBRACKET T_DASH T_RBRACKET alloc_grpsyl
	{
		Good_till_canceled = NO;
	}

	|
	T_LBRACKET alloc_grpsyl marklist T_RBRACKET gtc extra_styleinfo
	{
	};

gtc:
	{
		Good_till_canceled = NO;
	}

	|
	T_DOT T_DOT T_DOT
	{
		Good_till_canceled = YES;
	};

extra_styleinfo:
	{
	}

	|
	extra_styleinfo T_LBRACKET marklist T_RBRACKET
	{
		if (Good_till_canceled == YES) {
			yyerror("can't use ... and more than one [] on same chord");
		}
	};

alloc_grpsyl:
	{
		/* NULL token to cause allocation of a GRPSYL struct */
		Last_grpsyl_p = Curr_grpsyl_p;
		Curr_grpsyl_p = newGRPSYL(GS_GROUP);
	};

marklist:
	{
		/* an empty marklist means we should use
		 * the same attributes as the last time */
		copy_attributes(Curr_grpsyl_p, Last_grpsyl_p);
	}

	|
	marklistA
	;

marklistA: markitem

	|
	marklistA T_SEMICOLON markitem
	;

markitem:	T_WITH alloc_marklist marks mark_place
	{
		Curr_grpsyl_p->nwith = (short) Item_count;
		Curr_grpsyl_p->withlist = Curr_marklist;
		if ($4 != PL_UNKNOWN) {
			int m;

			/* User specified a side, so remember that */
			for (m = Mark_start; m < Item_count; m++) {
				Curr_marklist[m].place = $4;
			}
		}
	}

	|
	grp_locsave

	|
	notetype

	|
	padding

	|
	dir
	{
		if (Curr_grpsyl_p->stemdir != UNKNOWN &&
					Curr_grpsyl_p->stemdir != $1) {
			l_warning(Curr_filename, yylineno,
				"both stem directions specified; using last instance");
		}
		if (Doing_tab_staff == YES) {
			l_warning(Curr_filename, yylineno,
				"stem direction specification is pointless on tab staff; ignoring");
		}
		else {
			Curr_grpsyl_p->stemdir = $1;
		}
	}

	|
	T_FNUMVAR opt_minus num
	{
		if ($1 != DIST) {
			l_warning(Curr_filename, yylineno,
				"unexpected parameter name; ignoring");
		}
		else {
			if (Curr_grpsyl_p->restdist != NORESTDIST &&
					Curr_grpsyl_p->restdist != $2 * $3) {
				l_warning(Curr_filename, yylineno,
						"more than one dist value specified; using last instance");
			}
			Curr_grpsyl_p->restdist = $2 * $3;
		}
	}

	|
	T_HO hor_offset
	{
		if (Curr_grpsyl_p->ho_usage != HO_NONE &&
				Curr_grpsyl_p->ho_usage != $2) {
			l_warning(Curr_filename, yylineno,
				"More than one ho type specified; last instance used");
		}
		Curr_grpsyl_p->ho_usage = $2;
	}

	|
	T_HS string
	{
		if (Curr_grpsyl_p != 0) {
			if (Curr_grpsyl_p->headshape != HS_UNKNOWN) {
				l_warning(Curr_filename, yylineno,
					"multiple head shapes specified, using last");
			}
			/* +2 to skip past extraneous font/size */
			if (Doing_tab_staff == YES && strcmp($2 + 2, "allx") != 0) {
				yyerror("allx is the only headshape allowed on tab staffs");
			}
			if ((Curr_grpsyl_p->headshape = get_shape_num($2 + 2))
					== HS_UNKNOWN) {
				l_yyerror(Curr_filename, yylineno,
					"'%s' is not a valid head shape name",
					ascii_str($2, YES, NO, TM_NONE));
			}
		}
		else {
			pfatal("Curr_grpsyl_p was null for setting hs");
		}
		/* We don't need the name anymore; we have its corresponding
		 * number that we use internally. */
		FREE($2);
	}

	|
	T_LEN floatnum
	{
		if (Curr_grpsyl_p->stemlen != STEMLEN_UNKNOWN &&
				fabs(Curr_grpsyl_p->stemlen - $2 * STEPSIZE) > 0.0001) {
			l_warning(Curr_filename, yylineno,
				"more than one len specified; using last instance");
		}
		Curr_grpsyl_p->stemlen = $2 * STEPSIZE;
	}

	|
	T_SLASHMARK num
	{
		if ($2 <= 0) {
			yyerror("slash number must be > 0");
		}
		else if ($2 > 8) {
			/* we decided 8 slashes would be at least 256th note
			 * or shorter, and 256th is the shortest note we
			 * support, 8 slashes is plenty */
			l_warning(Curr_filename, yylineno,
				"number of slashes only allowed to be <= 8; limiting to 8");
			$2 = 8;
		}
		if (Curr_grpsyl_p->slash_alt > 0 &&
					Curr_grpsyl_p->slash_alt != $2) {
			l_warning(Curr_filename, yylineno,
				"more than one slash value specified; using last instance");
		}
		if (Curr_grpsyl_p->slash_alt < 0) {
			yyerror("only one slash/alt allowed per group");
		}
		Curr_grpsyl_p->slash_alt = $2;
	};

hor_offset:
	T_PLUS
	{
		$$ = HO_RIGHT;
	}

	|
	T_DASH
	{
		$$ = HO_LEFT;
	}

	|
	opt_plus_or_minus floatnum
	{
		/* We want to silently accept identical values specified
		 * more than once, so use fuzzy compare to deal with roundoff */
		if (Curr_grpsyl_p->ho_usage == HO_VALUE &&
				fabs(Curr_grpsyl_p->ho_value - $1 * $2) > 0.0001) {
			l_warning(Curr_filename, yylineno,
				"More than one ho value specified; last instance used");
		}
		if ($1 == 0) {
			Curr_grpsyl_p->ho_value = $2;
		}
		else {
			Curr_grpsyl_p->ho_value = $1 * $2;
		}
		$$ = HO_VALUE;
	};

opt_plus_or_minus:
	/* empty */
	{
		$$ = 0;
	}

	|
	plus_or_minus
	{
		$$ = $1;
	};

other_attr:

	|
	attr_list
	;

attr_list:
	attr_item

	|
	attr_list T_COMMA attr_item
	;

attr_item:
	beammark

	|
	T_SLOPE opt_minus floatnum
	{
		$3 *= $2;
		if (fabs(Curr_grpsyl_p->beamslope - NOBEAMANGLE) > 0.01) {
			l_warning(Curr_filename, yylineno,
				"multiple slope values specified; using last one");
		}
		if (frangecheck($3, MINBEAMANGLE, MAXBEAMANGLE, "slope") == YES) {
			Curr_grpsyl_p->beamslope = $3;
		}
	}

	|
	T_ALT num
	{
		if ($2 <= 0) {
			yyerror("alt number must be > 0");
		}
		if (Curr_grpsyl_p->slash_alt != 0) {
			yyerror("only one slash/alt allowed per group");
		}
		if (Doing_tab_staff == YES) {
			l_warning(Curr_filename, yylineno,
				"alt not allowed on tablature staff; ignoring");
		}
		else {
			/* Should at least keep small enough
			 * that 1 shifted left by this still fits
			 * in a 16-bit int. For a while we limited to 15,
			 * but for slashes, we limit to 8, and
			 * that seems like more than enough for here too. */
			if ($2 > 8) {
				l_warning(Curr_filename, yylineno,
					"alt value too large; limiting to 8");
				$2 = 8;
			}
			Curr_grpsyl_p->slash_alt = -$2;
		}
	}

	|
	T_PH ph_opt_place
	{
		if (Curr_grpsyl_p->phcount < PH_COUNT) {
			Curr_grpsyl_p->phplace[Curr_grpsyl_p->phcount++] = $2;
		}
		else {
			l_yyerror(Curr_filename, yylineno,
				"too many ph marks on the same chord; limiting to %d", PH_COUNT);
		}
	}

	|
	T_EPH
	{
		if (Curr_grpsyl_p->ephcount < PH_COUNT) {
			Curr_grpsyl_p->ephcount++;
		}
		else {
			l_yyerror(Curr_filename, yylineno,
				"too many eph marks on the same chord; limiting to %d", PH_COUNT);
		}
	}
	|
	T_SLUR opt_dir opt_to_voice
	{
		int n;

		/* add a slur to each note in the chord. Don't know
		 * which pitch to slur to yet, just that it will be to
		 * the "matching" note in the next chord, so use
		 * special magic pitch of 'M' to convey this */
		for (n = 0; n < Curr_grpsyl_p->nnotes; n++) {
			add_slurto(Curr_grpsyl_p, 'M', USE_DFLT_OCTAVE, n, $1);
			set_slurdir(Curr_grpsyl_p, -1, $2, $3);
		}
	}

	|
	T_TIE opt_dir opt_to_voice
	{
		int n;

		Curr_grpsyl_p->tie = YES;
		for (n = 0; n < Curr_grpsyl_p->nnotes; n++) {
			Curr_grpsyl_p->notelist[n].tie = YES;
			Curr_grpsyl_p->notelist[n].tiestyle = $1;
			Curr_grpsyl_p->notelist[n].tiedir = $2;
			Curr_grpsyl_p->notelist[n].tied_to_voice = $3;
		}
	};

alloc_marklist:
	{
		if (Curr_marklist == 0) {
			/* allocate space for a list of marks */
			MALLOC(WITH_ITEM, Curr_marklist, ITEMS);
			Item_count = 0;
			Max_items = ITEMS;
		}
		Mark_start = Item_count;
	};

mark_place:
	{
		$$ = PL_UNKNOWN;
	}

	|
	T_PLACE
	{
		$$ = $1;
	};

opt_dir:
	{
		$$ = UNKNOWN;
	}

	|
	dir
	{
		$$ = $1;
	}
	;

opt_to_voice:
	{
		$$ = NO_TO_VOICE;
	}

	|
	T_TO T_VOICE num
	{
		if ($3 >= MINVOICES && $3 <= MAXVOICES) {
			/* Note that if the current staff doesn't have this
			 * to voice, that error will be caught later,
			 * by find_to_group() in tie.c */
			$$ = $3;
		}
		else {
			l_yyerror(Curr_filename, yylineno,
					"to voice must be between %d and %d",
					MINVOICES, MAXVOICES);
			$$ = NO_TO_VOICE;
		}
	}
	;
	
marks:	mark

	|
	marks T_COMMA mark
	;

mark:	mark_item
	{
		/* if too many items, get some more space */
		if (Item_count >= Max_items) {
			Max_items += ITEMS;
			if ((Curr_marklist = (struct WITH_ITEM *) realloc(Curr_marklist,
						Max_items * sizeof(struct WITH_ITEM)))
						==  0) {
				l_no_mem(__FILE__, __LINE__);
			}
		}
		Curr_marklist[Item_count].string = $1;
		Curr_marklist[Item_count].place = PL_UNKNOWN;
		Item_count++;
	};

mark_item:	shorthand_mark
	{
		MALLOCA(char, $$, strlen($1) + 3);
		$$[0] = FONT_TR;
		$$[1] = DFLT_SIZE;
		sprintf($$ + 2, "%s", $1);
	}

	|
	string
	;

shorthand_mark:
	T_DOT
	{
		$$ = (Curr_grpsyl_p->grpsize == GS_NORMAL
					? "\\(dot)" : "\\(smdot)");
	}

	|
	T_DASH
	{
		$$ = (Curr_grpsyl_p->grpsize == GS_NORMAL
					? "\\(leg)" : "\\(smleg)");
	}

	|
	T_R_ANGLE
	{
		$$ = (Curr_grpsyl_p->grpsize == GS_NORMAL
					? "\\(acc_gt)" : "\\(smacc_gt)");
	}

	|
	T_HAT
	{
		$$ = (Curr_grpsyl_p->grpsize == GS_NORMAL
					? "\\(acc_hat)" : "\\(smacc_hat)");
	};

notetype:	T_CUE
	{
		if (Curr_grpsyl_p->grpvalue == GV_ZERO) {
			Curr_grpsyl_p->grpsize = GS_TINY;
		}
		else {
			Curr_grpsyl_p->grpvalue = GV_NORMAL;
			Curr_grpsyl_p->grpsize = GS_SMALL;
		}
	}

	|
	T_GRACE
	{
		Curr_grpsyl_p->grpvalue = GV_ZERO;
		if (Curr_grpsyl_p->grpsize == GS_SMALL) {
			Curr_grpsyl_p->grpsize = GS_TINY;
		}
		else {
			Curr_grpsyl_p->grpsize = GS_SMALL;
		}
	}

	|
	T_DIAM
	{
		if (Curr_grpsyl_p->headshape != HS_UNKNOWN) {
			l_warning(Curr_filename, yylineno,
				"diam is overriding previous headshapes specification");
		}
		Curr_grpsyl_p->headshape = get_shape_num("diam");
	}

	|
	T_XNOTE
	{
		if (Curr_grpsyl_p->headshape != HS_UNKNOWN) {
			l_warning(Curr_filename, yylineno,
				"xnote is overriding previous headshapes specification");
		}
		Curr_grpsyl_p->headshape = get_shape_num(
					Doing_tab_staff == YES ? "allx" : "x");
	};

dir:
	T_UP
	{
		$$ = UP;
	}

	|
	T_DOWN
	{
		$$ = DOWN;
	};

beammark:
	T_BM crossbeam
	{
		/* custom beaming cannot be nested */
		struct GRPSYL *prev_like_gs_p;
		/* Find previous group with same grace-ness. Can't use
		 * prevsimilar() here, since it looks at grpcont too. */
		for (prev_like_gs_p = Last_grpsyl_p; prev_like_gs_p != 0 &&
			prev_like_gs_p->grpvalue != Curr_grpsyl_p->grpvalue;
			prev_like_gs_p = prev_like_gs_p->prev) {
			;
		}
		if (prev_like_gs_p != 0 &&
				((prev_like_gs_p->beamloc == STARTITEM)
				|| (prev_like_gs_p->beamloc == INITEM)) ) {
			yyerror("custom beaming may not be nested");
		}
		/* Non-custom beaming, if any, is done later, in do_bar() */
		else {
			/* begin custom beaming */
			Curr_grpsyl_p->beamloc = STARTITEM;
			if (Curr_grpsyl_p->basictime < 8 && $2 == CS_SAME) {
				yyerror("beamed notes must be 8th or shorter");
			}
		}
		Curr_grpsyl_p->beamto = $2;
	}

	|
	T_ABM
	{
		Curr_grpsyl_p->autobeam = STARTITEM;
	}

	|
	T_EABM
	{
		Curr_grpsyl_p->autobeam = ENDITEM;
	}

	|
	T_ESBM
	{
		Curr_grpsyl_p->breakbeam = YES;
	}

	|
	T_EBM
	{
		struct GRPSYL *last_nongrace_p;

		/* find the previous group, skipping grace groups */
		for (last_nongrace_p = Last_grpsyl_p;
				last_nongrace_p != (struct GRPSYL *) 0
				&& last_nongrace_p->grpvalue == GV_ZERO;
				last_nongrace_p = last_nongrace_p->prev) {
			;
		}

		/* check that a custom beam is in progress */
		if ((last_nongrace_p == (struct GRPSYL *) 0)
				|| ((last_nongrace_p != (struct GRPSYL *) 0)
				&& (last_nongrace_p->beamloc != STARTITEM)
				&& (last_nongrace_p->beamloc != INITEM) )) {
			l_warning(Curr_filename, yylineno,
				"'ebm' unexpected: no custom beaming in progress; ignoring");
		}
		else {
			Curr_grpsyl_p->beamloc = ENDITEM;
			Curr_grpsyl_p->beamto = last_nongrace_p->beamto;
			if (Curr_grpsyl_p->basictime < 8
					&& Last_grpsyl_p->beamto == CS_SAME) {
				yyerror("beamed notes must be 8th or shorter");
			}
		}
	};

time_val:
	{
		/* Use the same as last time or default timeunit.
		 * If that involves additive time values,
		 * save pointer to that info in Extra_time_p for later use. */
		Extra_time_p = copy_timeunit(Curr_grpsyl_p, Prev_grpsyl_p,
						Extra_time_p);
	}

	|
	time_item
	{
		if (Curr_grpsyl_p->is_meas == YES) {
			Curr_grpsyl_p->fulltime = Score.time;
		}
		else {
			Curr_grpsyl_p->fulltime = $1;
		}
	};

crossbeam:
	{
		$$ = CS_SAME;
	}

	|
	T_WITH T_STAFF T_PLACE
	{
		CSBused = YES;
		switch ($3) {
		case PL_ABOVE:
			$$ = CS_ABOVE;
			break;
		case PL_BELOW:
			$$ = CS_BELOW;
			break;
		default:
			yyerror("bm with staff must be 'above' or 'below'");
			$$ = CS_SAME;
		}
	};

basic_time_val:	T_MULTIWHOLE
	{
		$$.n = $1;
		$$.d = 1;

		/* if filling in a GRPSYL struct, need to fill in basic time,
		 * could also be here due to beamstyle, in which case the
		 * Curr_grpsyl_p will be NULL, or when getting tuplet duration,
		 * in which case flag will be set */
		if (Curr_grpsyl_p != (struct GRPSYL *) 0
					&& Getting_tup_dur == NO) {
			/* If we are gathering basictime as part of a list
			 * of additive times, we will save the value a few
			 * lines down from here. But in the normal case,
			 * we set basictime in the current GRPSYL. */
			if (Extra_time_p == 0) {
				 Curr_grpsyl_p->basictime = multiwhole2basictime($1);
			}
		}
		/* If doing additive times, need to save value. */
		if (Extra_time_p != 0) {
			Extra_basictime = multiwhole2basictime($1);
		}
	}

	|
	num
	{
		/* check that a power of two from 1 to MAXBASICTIME */
		if (power_of2check($1, "note basic time value") == NO) {
			/* force to a power to two, so that other code
			 * (like in expandgrp) that expect a sane value
			 * will not blow up. This may lead to a somewhat
			 * misleading "time does not add up to time
			 * signature" message, but we don't know what
			 * time they really meant, and this is better
			 * than pfataling. */
			$1 = 2;
		}
		/* can't use rangecheck here because the error message would
		 * say 0 and -1 are valid times, which is only true internally--
		 * the user has to use 1/2, 1/4, 1/8, or m. */
		if ($1 < MINBASICTIME || $1 > MAXBASICTIME) {
			l_yyerror(Curr_filename, yylineno,
				"time value must be between 1 and %d, or 1/2 or 1/4 or 1/8 or m",
				MAXBASICTIME);
			/* If user entered outrageous value, clamp to
			 * something semi-reasonable, else we could get
			 * rational overflow, and pfatal, even though it
			 * it is really a user error. */
			if ($1 > MAXBASICTIME * 4) {
				$1 = MAXBASICTIME * 4;
			}
		}
		$$.n = 1;
		/* avoid division by zero */
		if ($1 == 0) {
			$1 = 1;
		}
		$$.d = $1;
		if (Curr_grpsyl_p != (struct GRPSYL *) 0
					&& Getting_tup_dur == NO) {
			if (Curr_timelist_p == 0) {
				Curr_grpsyl_p->basictime = $1;
			}
		}
		/* If doing additive times, need to save value */
		if (Extra_time_p != 0) {
			Extra_basictime = $1;
		}
	};

opt_more_time:
	/* empty */
	{
		free_extra_time();
	}

	|
	opt_more_time more_time_item 
	;

more_time_item:
	plus_or_minus alloc_timelist basic_time_val dots
	{
		/* Set basictime to what we saved in basic_time_val rule,
		 * then calculate fulltime from that and number of dots. */
		Curr_timelist_p->basictime = Extra_basictime;
		Curr_timelist_p->fulltime = calcfulltime($3, $4);
		/* handle subtracted times by negating the fulltime */
		if ($1 == -1) {
			Curr_timelist_p->fulltime = rneg(Curr_timelist_p->fulltime);
		}
	}
	;

alloc_timelist:
	/* empty -- just for allocating TIMELIST struct */
	{
		struct TIMELIST *timelist_p;

		MALLOC(TIMELIST, timelist_p, 1);
		/* Add to end of linked list */
		timelist_p->next = 0;
		/* Init fulltime to something to avoid garbage if there
		 * in a user input error */
		timelist_p->fulltime = Zero;
		if (Extra_time_p == 0) {
			Last_alloced_timelist_p = Extra_time_p = timelist_p;
		}
		else {
			Curr_timelist_p->next = timelist_p;
		}
		/* Keep track of where to append next item to list, if any */
		Curr_timelist_p = timelist_p;
	}
	;

opt_m:	
	{
		User_meas_time = NO;
	}

	|
	T_LET_M
	{
		if (Curr_grpsyl_p != (struct GRPSYL *) 0
					&& Getting_tup_dur == NO) {
			Curr_grpsyl_p->is_meas = YES;
			User_meas_time = YES;
		}
		else {
			yyerror("'m' is not valid here");
		}
	};

dots:
	{
		$$ = 0;
	}

	|
	dots T_DOT
	{
		/* count up the number of dots */
		$$ = $1 + 1;
	};

padding: padspec
	{
		/* We can't distinguish between the default 0.0 and
		 * if user explicitly sets to 0.0, so if they set to 0.0
		 * then to something else, we won't catch that as setting
		 * twice, but that shouldn't be common. Besides, if they
		 * expect them to be additive, adding to zero will work
		 * as they expect... Because of roundoff, we can't do
		 * exact compare for specifying the same value more than once,
		 * so treat as identical if pretty close. */
		if (Curr_grpsyl_p->padding != 0.0 && 
				fabs(Curr_grpsyl_p->padding - ($1 * STEPSIZE))
				> 0.0001) {
			l_warning(Curr_filename, yylineno,
				"padding specified more than once; using last instance");
		}
		Curr_grpsyl_p->padding = $1 * STEPSIZE;
	}
	;

padspec: T_PAD opt_minus floatnum
	{
		$$ = (float) $2 * $3;
	};

opt_minus:
	{
		/* no sign--must be a positive number */
		$$ = 1;
	}

	|
	T_DASH
	{
		/* user wants a negative number */
		$$ = -1;
	};

group:	notelist
	{
		Curr_grpsyl_p->grpcont = GC_NOTES;
		if ($1 == 0) {
			/* no notes listed, use same as previous group */
			copy_notes(Curr_grpsyl_p, Last_grpsyl_p);
		}
		else {
			resize_notelist(Curr_grpsyl_p);
		}
	}

	|
	special_group

	;

special_group:
	extra_attr note_opts
	;

extra_attr:
	spec_grp_alloc extra_item
	;

spec_grp_alloc:
	{
		/* allocate GRPSYL for the case where everything is defaulted
		 * from the previous group, with just an extra attribute
		 * like ? or ~ specified. */
		if (Curr_grpsyl_p == (struct GRPSYL *) 0) {
			/* shouldn't ever happen, but just in case... */
			Curr_grpsyl_p = newGRPSYL(GS_GROUP);
		}
		if (Last_grpsyl_p != (struct GRPSYL *) 0 &&
					Last_grpsyl_p->nnotes >= 1) {
			copy_notes(Curr_grpsyl_p, Last_grpsyl_p);
		}
		else if (Last_grpsyl_p != (struct GRPSYL *) 0 && 
				Last_grpsyl_p->grpcont == GC_REST) {
			Curr_grpsyl_p->grpcont = GC_REST;
		}
	};

extra_item:
	T_QUESTION
	{
		if (Curr_grpsyl_p->grpcont == GC_NOTES &&
					Curr_grpsyl_p->nnotes > 0) {
			if (Curr_grpsyl_p->grpvalue == GV_ZERO) {
				Curr_grpsyl_p->notelist[Curr_grpsyl_p->nnotes - 1]
						.notesize = GS_TINY;
			}
			else {
				Curr_grpsyl_p->notelist[Curr_grpsyl_p->nnotes - 1]
						.notesize = GS_SMALL;
			}
		}
		else if (Curr_grpsyl_p->grpcont == GC_REST) {
			Curr_grpsyl_p->grpsize  = GS_SMALL;
		}
		else {
			yyerror("no note specified for '?'");
		}
	}

	|
	T_TILDE opt_dir opt_to_voice
	{
		if (Curr_grpsyl_p->grpcont != GC_NOTES) {
			yyerror("can't tie a rest or space");
		}
		else if (Curr_grpsyl_p->nnotes > 0) {
			struct NOTE *n_p;
			n_p = &(Curr_grpsyl_p->notelist[Curr_grpsyl_p->nnotes - 1]);
			n_p->tie = YES;
			n_p->tiestyle = $1;
			n_p->tiedir = $2;
			n_p->tied_to_voice = $3;
		}
		else {
			yyerror("no note specified for '~'");
		}
	}

	|
	T_EQUAL loc_variable
	{
		if (Curr_grpsyl_p->nnotes > 0) {
			switch (Curr_grpsyl_p->notelist
                                        [Curr_grpsyl_p->nnotes - 1].letter) {
			case PP_REST:
				/* If we haven't already allocated space for
				 * the restc array, so that now. It could
				 * already be allocated if user specified
				 * more than one tag. */
				if (Curr_grpsyl_p->restc == 0) {
					MALLOCA(float, Curr_grpsyl_p->restc, NUMCTYPE);
				}
				addsym($2, Curr_grpsyl_p->restc, CT_NOTE);
				break;
			case PP_SPACE:
			case PP_RPT:
			case PP_NO_PITCH:  /* this one not really possible */
				addsym($2, Curr_grpsyl_p->c, CT_GRPSYL);
				break;
			default:
				addsym($2, Curr_grpsyl_p->notelist
					[Curr_grpsyl_p->nnotes - 1].c, CT_NOTE);
				break;
			}
			var_valid();
		}
		else if (Curr_grpsyl_p->grpcont == GC_REST) {
			/* This should really never be hit anymore since
			 * chord-at-a-time code was added, but shouldn't
			 * hurt to leave it, just in case. */
			addsym($2, Curr_grpsyl_p->c, CT_GRPSYL);
			var_valid();
		}
		else {
			l_yyerror(Curr_filename, yylineno,
				"no note specified for location tag '%s'", $2);
		}
	}

	|
	T_HAT pitch opt_octave
	{
		/* this is for bend on a non-tablature staff */
		if (Doing_tab_staff == YES) {
			yyerror("^ bend not allowed on tablature staff; use quoted bend string");
		}
		else {
			add_slurto(Curr_grpsyl_p, $2, $3,
					Curr_grpsyl_p->nnotes - 1, L_NORMAL);
			if (Curr_grpsyl_p->nnotes > 0) {
				Curr_grpsyl_p->notelist
						[Curr_grpsyl_p->nnotes - 1]
						.is_bend = YES;
			}
		}
	}

	|
	T_HAT T_SLASH
	{
		/* this is for a small bend (1/4 step) on a non-tab staff */
		if (Doing_tab_staff == YES) {
			yyerror("^/ not allowed on tablature staff; use quoted bend string");
		}
		else if (Curr_grpsyl_p != 0 && Curr_grpsyl_p->nnotes > 0) {
			Curr_grpsyl_p->notelist[Curr_grpsyl_p->nnotes - 1]
					.smallbend = YES;
		}
	}

	|
	T_HS string
	{
		if (Curr_grpsyl_p->grpcont == GC_NOTES &&
					Curr_grpsyl_p->nnotes > 0) {
			if ((Curr_grpsyl_p->notelist[Curr_grpsyl_p->nnotes - 1]
					.headshape = get_shape_num($2 + 2))
					== HS_UNKNOWN) {
				l_yyerror(Curr_filename, yylineno,
					"'%s' is not a valid head shape name",
					ascii_str($2, YES, NO, TM_NONE));
			}
		}
		else {
			yyerror("no note specified for headshape");
		}
		FREE($2);
	}

	|
	slur_item
	;

note_opts:

	|
	note_opts extra_item
	;

notelist:
	some_notes opt_other_staff
	{
		$$ = $1 + $2;	/* total number of notes */
	};

opt_other_staff:
	/* empty */
	{
		$$ = 0;
	}

	|
	css_with some_notes T_PLACE
	{
		switch ($3) {
		case PL_ABOVE:
			Curr_grpsyl_p->stemto = CS_ABOVE;
			break;
		case PL_BELOW:
			Curr_grpsyl_p->stemto = CS_BELOW;
			break;
		default:
			yyerror("cross staff stem must be with 'above' or 'below'");
			Curr_grpsyl_p->stemto = CS_SAME;
		}
		if ($2 == 0) {
			/* Maybe this wouldn't really hurt to allow,
			 * but it's rather silly--why would user bother to go
			 * to the trouble of saying there are cross staff stem
			 * notes, but then not list any?
			 */
			yyerror("cross-staff stem note list is empty");
		}

		/* It doesn't make sense to do "with r below" or "with s below"
		 * since we don't allow mixing rests and notes, and the only
		 * reason for CSS is for something that has a stem. */
		else if ($2 > 0) {
			int idx;
			int pitch;

			for (idx = FCNI(Curr_grpsyl_p);
					idx <= LCNI(Curr_grpsyl_p); idx++) {
				pitch = Curr_grpsyl_p->notelist[idx].letter;
				if (pitch == PP_REST || pitch == PP_SPACE) {
					yyerror("can't use with above/below on rest or space");
				}
			}
		}
		$$ = $2;
	};

css_with:	T_WITH
	{
		CSSused = YES;
		Curr_grpsyl_p->stemto_idx = Curr_grpsyl_p->nnotes;
	};

some_notes:
	{
		/* No notes.  If this is for the list of "normal" staff notes,
		 * and there are no "other" staff notes (for cross-staff stems),
		 * this means use the same notes as the last group. */
		$$ = 0;
	}

	|
	some_notes notedata note_opts
	{
		/* return number of notes in notelist */
		$$ = $1 + 1;
	};

notedata:	notedata1

	|
	T_LET_R
	{
		add_note(Curr_grpsyl_p, (int) PP_REST, No_accs,
				USE_DFLT_OCTAVE, 0, NO, (char *) 0);
	}

	|
	T_LET_S
	{
		/* temporarily stash uncompressibility (NO in this case)
		 * in the octave field */
		add_note(Curr_grpsyl_p, (int) PP_SPACE, No_accs, NO,
							0, NO, (char *) 0);
		if (Curr_grpsyl_p->is_meas == YES && User_meas_time == YES) {
			l_warning(Curr_filename, yylineno,
				"specifying time value on measure space is pointless; ignoring");
		}
	}

	|
	T_LET_U T_LET_S
	{
		/* temporarily stash uncompressibility (YES in this case)
		 * in the octave field */
		add_note(Curr_grpsyl_p, (int) PP_SPACE, No_accs, YES,
							0, NO, (char *) 0);
		if (Curr_grpsyl_p->is_meas == YES && User_meas_time == YES) {
			l_warning(Curr_filename, yylineno,
				"specifying time value on measure uncollapsible space is pointless; ignoring");
		}
	}

	|
	T_RPT
	{
		if (Curr_grpsyl_p->is_meas == YES) {
			if (User_meas_time == YES) {
				l_warning(Curr_filename, yylineno,
					"specifying time value on measure repeat is pointless; ignoring");
			}
		}
		else {
			yyerror("rpt can only be used with m");
		}
		add_note(Curr_grpsyl_p, (int) PP_RPT, No_accs, USE_DFLT_OCTAVE,
							0, NO, (char *) 0);
	}

	|
	T_BARTYPE
	{
		if ($1 == SINGLEBAR) {
			/* lexer thinks this is 'bar'
			 * but is really 'b' 'a' 'r' */
			keyword_notes(yytext);
		}
		else if ($1 == ENDBAR) {
			/* lexer thinks this is 'endbar'
			 * but is really 'en' 'd' 'b' 'a' 'r' */
			add_acc("nat");
			add_note(Curr_grpsyl_p, (int) 'e', Curr_accs,
				USE_DFLT_OCTAVE, 0, NO, (char *) 0);
			keyword_notes(yytext + 2);
		}
		else {
			yyerror("bar type not valid here");
		}
	}

	|
	T_GRACE
	{
		/* lexer thinks this is 'grace' but really 'g' 'r' 'a' 'c' 'e' */
		keyword_notes(yytext);
	}

	|
	T_ENDTYPE
	{
		if ($1 == ENDING_BARRED) {
			keyword_notes(yytext);
		}
		else {
			l_yyerror(Curr_filename, yylineno,
					"%s not valid here", yytext);
		}
	}

	|
	T_PARATYPE {
		if ($1 == J_RAGPARA) {
			keyword_notes(yytext);
		}
		else {
			l_yyerror(Curr_filename, yylineno,
					"%s not valid here", yytext);
		}
	}

	| 
	T_RANGELISTVAR
	{
		if ($1 == BRACELIST) {
			keyword_notes(yytext);
		}
		else {
			l_yyerror(Curr_filename, yylineno,
					"%s not valid here", yytext);
		}
	}

	|
	T_CLEF
	{
		if ($1 == BASS) {
			keyword_notes(yytext);
		}
		else {
			l_yyerror(Curr_filename, yylineno,
					"%s not valid here", yytext);
		}
	}

	|
	T_USEACCS
	{
		/* in chord-at-time mode, lex could mistake useaccs
		 * for a 6-note chord: us e a c c s  */
		keyword_notes(yytext);
	}

	|
	T_LPAREN notedata1 T_RPAREN
	{
		Curr_grpsyl_p->notelist[Curr_grpsyl_p->nnotes - 1].note_has_paren = YES;
	};

notedata1:	pitch opt_bend
	{
		if (Doing_tab_staff == YES) {
			add_note(Curr_grpsyl_p, $1, No_accs, NOFRET, 0, NO, $2);
		}
		else {
			add_note(Curr_grpsyl_p, $1, No_accs, USE_DFLT_OCTAVE,
								0, NO, $2);
		}
	}

	|
	pitch T_LPAREN acc_list T_RPAREN opt_octave
	{
		if (Doing_tab_staff == YES) {
			l_warning(Curr_filename, yylineno,
				"parentheses around accidentals are extraneous on tab staffs; ignoring");
		}
		add_note(Curr_grpsyl_p, $1, Curr_accs, $5, 0, YES, (char *) 0);
	}

	|
	pitch T_LPAREN octave T_RPAREN opt_bend
	{
		if (Doing_tab_staff == NO) {
			l_warning(Curr_filename, yylineno,
				"parentheses around octave are extraneous; ignoring");
		}
		add_note(Curr_grpsyl_p, $1, No_accs, $3, 0, YES, $5);
	}

	|
	pitch T_LPAREN pitch
	{
		/* Whoops! The left paren was really for a new note,
		 * which happens to be parenthesized. But beause yacc only
		 * looks ahead one token, it finds out too late. So we
		 * catch it here, push the parenthesis and pitch back into
		 * the input and return back to the parent grammar rule,
		 * since we now have complete note. */
		pushback(yytext[0]);
		pushback('(');
		add_note(Curr_grpsyl_p, $1, No_accs,
			(Doing_tab_staff ? NOFRET : USE_DFLT_OCTAVE),
			0, NO, (char *) 0);
	}

	|
	pitch acc_list opt_ticks opt_octave opt_bend
	{
		add_note(Curr_grpsyl_p, $1, Curr_accs, $4, $3, NO, $5);
	}

	|
	pitch acc_list opt_ticks T_LPAREN octave T_RPAREN opt_bend
	{
		if (Doing_tab_staff == NO) {
			l_warning(Curr_filename, yylineno,
				"extraneous parentheses around octave; ignoring");
		}
		add_note(Curr_grpsyl_p, $1, Curr_accs, $5, $3, YES, $7);
	}

	|
	pitch acc_list opt_ticks T_LPAREN pitch
	{
		/* Whoops! The left paren was really for a new note,
		 * which happens to be parenthesized. But beause yacc only
		 * looks ahead one token, it finds out too late. So we
		 * catch it here, push the parenthesis and pitch back into
		 * the input and return back to the parent grammar rule,
		 * since we now have complete note. */
		pushback(yytext[0]);
		pushback('(');
		add_note(Curr_grpsyl_p, $1, Curr_accs,
			(Doing_tab_staff ? NOFRET : USE_DFLT_OCTAVE),
			$3, NO, (char *) 0);
	}

	|
	pitch T_TICKS opt_octave opt_bend
	{
		add_note(Curr_grpsyl_p, $1, No_accs, $3, $2, NO, $4);
	}

	|
	pitch T_TICKS T_LPAREN octave T_RPAREN opt_bend
	{
		add_note(Curr_grpsyl_p, $1, No_accs, $4, $2, YES, $6);
	}

	|
	pitch octave opt_acc_list opt_bend
	{
		if (Doing_tab_staff == YES && Curr_accs[0] != '\0') {
			yyerror("accidental must be before fret number");
		}
		add_note(Curr_grpsyl_p, $1, Curr_accs, $2, 0, NO, $4);
	}

	|
	pitch octave T_LPAREN acc_list T_RPAREN
	{
		add_note(Curr_grpsyl_p, $1, Curr_accs, $2, 0, YES, (char *) 0);
	}

	|
	pitch octave T_LPAREN pitch
	{
		/* Whoops! The left paren was really for a new note,
		 * which happens to be parenthesized. But beause yacc only
		 * looks ahead one token, it finds out too late. So we
		 * catch it here, push the parenthesis and pitch back into
		 * the input and return back to the parent grammar rule,
		 * since we now have complete note. */
		pushback(yytext[0]);
		pushback('(');
		add_note(Curr_grpsyl_p, $1, No_accs, $2, 0, NO, (char *) 0);
	};

opt_bend:
	{
		$$ = (char *) 0;
	}

	|
	string
	{
		if (Doing_tab_staff == NO) {
			/* try to give helpful error message */
			if (get_shape_num($1 + 2) != HS_UNKNOWN) {
				yyerror("missing 'hs' before headshape string");
			}
			else if (strcmp($1 + 2, "full") == 0 || isdigit($1[2])) {
				yyerror("bend string not allowed on non-tablature staff; use ^");
			}
			else {
				yyerror("unexpected string");
			}
			$$ = (char *) 0;
		}
	};

pitch:	T_PITCH
	{
		/* return 'a' to 'g' value */
		$$ = (int) *yytext;
	};

opt_accidental:
	{
		/* no accidental */
		$$ = 0;
	}

	|
	acc_symbol
	{
	};

opt_acc_list:
	{
		clear_curr_accs();
	}

	|
	acc_list
	{
	};
opt_octave:
	{
		/* no octave or fret designation */
		$$ = (Doing_tab_staff ? NOFRET : USE_DFLT_OCTAVE);
	}

	|
	octave
	{
	};

acc_list:
	begin_acc_list note_acc acc_list_tail
	{
	};

begin_acc_list:
	{
		clear_curr_accs();
	};

acc_list_tail:
	{
	}

	|
	acc_list_tail note_acc
	{
	};

note_acc:
	T_SHARP
	{
		add_acc("sharp");
	}

	|
	T_AMPERSAND
	{
		add_acc("flat");
	}

	|
	T_LET_N
	{
		add_acc("nat");
	}

	|
	T_LET_X
	{
		add_acc("dblsharp");
	}

	|
	T_DBLFLAT
	{
		add_acc("dblflat");
	}

	|
	T_LBRACE string T_RBRACE
	{
		add_acc($2 + 2);
		Tuning_used = YES;
	};

acc_symbol:
	T_SHARP
	{
		$$ = '#';
	}

	|
	T_AMPERSAND
	{
		$$ = '&';
	}

	|
	T_LET_N
	{
		/* natural */
		$$ = 'n';
	}

	|
	T_LET_X
	{
		/* double sharp */
		$$ = 'x';
	}

	|
	T_DBLFLAT
	{
		$$ = 'B';
	};

octave:
	num
	{
		$$ = $1;
	}

	|
	minuslist
	{
		/* we can't really fill in the actual octave yet, because
		 * it may be different on different staffs or voices, so
		 * we store the relative octave and fill in actual value
		 * later.
		 */
		/* this will be a negative number */
		$$ = $1;
	}

	|
	pluslist
	{
		/* we can't really fill in the actual octave yet, because
		 * it may be different on different staffs or voices, so
		 * we store the relative octave and fill in actual value
		 * later.
		 */
		$$ = USE_DFLT_OCTAVE + $1;
	};

minuslist:	T_DASH
	{
		$$ = -1;
	}

	|
	minuslist T_DASH
	{
		/* count up the number of minus signs */
		$$ = $1 - 1;
	};

pluslist:	T_PLUS
	{
		$$ = 1;
	}

	|
	pluslist T_PLUS
	{
		/* count up the number of plus signs */
		$$ = $1 + 1;
	};

slur_item:
	start_slur slurlist T_R_ANGLE opt_dir opt_to_voice
	{
		set_slurdir(Curr_grpsyl_p, Curr_grpsyl_p->nnotes - 1, $4, $5);
	}
	;

start_slur:
	T_L_ANGLE
	{
		Linetype = $1;
		begin_slur(Curr_grpsyl_p, Curr_grpsyl_p->nnotes - 1);
	};

slurlist:
	{
		/* empty list. Only allowed if only one note in following
		 * group. However, we don't know that yet, so marked pitch
		 * as 'U' for unknown */
		add_slurto(Curr_grpsyl_p, 'U', USE_DFLT_OCTAVE,
				Curr_grpsyl_p->nnotes - 1, Linetype);
	}

	|
	T_NOWHERE_SLIDE
	{
		add_slurto(Curr_grpsyl_p, 0, $1, Curr_grpsyl_p->nnotes - 1,
						Linetype);
	}

	|
	fullslurlist
	;

fullslurlist:
	slurnote

	|
	fullslurlist slurnote
	;

slurnote:
	pitch opt_octave
	{
		if (Doing_tab_staff == YES) {
			yyerror("cannot specify string inside <> on tab staffs");
		}
		else {
			add_slurto(Curr_grpsyl_p, $1, $2,
					Curr_grpsyl_p->nnotes - 1, Linetype);
		}
	};

grp_locsave:
	T_EQUAL loc_variable 
	{
		/* save address associated with entire group */
		if ( $2 != (char *) 0) {
			addsym($2, Curr_grpsyl_p->c, CT_GRPSYL);
			var_valid();
		}
	};

loc_variable:	T_LETTER 
	{
		$$ = strcpy(Stringbuff, yytext);
	}

	|
	T_PITCH
	{
		/* a-g are usually pitches, but in this context, they
		 * are 1-character variable names. */
		$$ = strcpy(Stringbuff, yytext);
	}

	|
	T_LET_M
	{
		/* usually m means measure, but here it is the variable m */
		$$ = strcpy(Stringbuff, yytext);
	}

	|
	T_LET_R
	{
		/* usually rest, here variable r */
		$$ = strcpy(Stringbuff, yytext);
	}

	|
	T_LET_S
	{
		/* usually space, here variable s */
		$$ = strcpy(Stringbuff, yytext);
	}

	|
	T_LET_U
	{
		/* usually uncompressible, here variable u */
		$$ = strcpy(Stringbuff, yytext);
	}

	|
	T_LET_N
	{
		/* usually natural, here variable n */
		$$ = strcpy(Stringbuff, yytext);
	}

	|
	T_LET_X
	{
		/* usually double sharp, here variable x */
		$$ = strcpy(Stringbuff, yytext);
	}

	|
	T_LVAR
	{
		/* tag name with more than one character */
		$$ = strcpy(Stringbuff, yytext);
	}

	|
	arraytag T_DOT num
	{
		(void) sprintf(Stringbuff + strlen($1), ".%d", $3);
		$$ = Stringbuff;
	};

arraytag:
	T_ARRAYVAR
	{
		/* tag name with more than one character */
		$$ = strcpy(Stringbuff, yytext);
	};


ntuplet:	start_tuplet groupinfo rbrace opt_side num printtup opt_time T_SEMICOLON
	{
		end_tuplet($5, $7, $6, $4);
		Getting_tup_dur = NO;
	};

printtup:
	{
		/* nothing -- use default of when to print tuplet number/bracket */
		$$ = PT_DEFAULT;
	}

	|
	T_LET_N
	{
		/* don't print tuplet or bracket */
		$$ = PT_NEITHER;
	}

	|
	T_NUM
	{
		/* print number only */
		$$ = PT_NUMBER;
	}

	|
	T_LETTER
	{
		if (*yytext == 'y') {
			$$ = PT_BOTH;
		}
		else {
			yyerror("tuplet number/bracket qualifier must be y or n or num");
		}
	};


start_tuplet:
	T_LBRACE
	{
		begin_tuplet();
	};

rbrace:		T_RBRACE
	{
		Getting_tup_dur = YES;
	};

opt_side:
	{
		$$ = PL_UNKNOWN;
	}

	|
	T_PLACE
	{
		if (yylval.intval == PL_BETWEEN) {
			yyerror("between not allowed for tuplet side");
			$$ = PL_UNKNOWN;
		}
	};

opt_time:
	{
		/* optional time value is missing */
		$$.n = 0;
		$$.d = 1;
	}

	|
	T_COMMA time_item
	{
		$$ = $2;
	};

lyricinfo:	T_LYRICS lyric_id using T_COLON lyricdata
	{
	};

lyric_id:	lyr_opt_place staff_list more_lyric_ids
	{
		Last_grpsyl_p = Curr_grpsyl_p = (struct GRPSYL *) 0;
	};

lyr_opt_place:	opt_place
	{
		/* If user does "all" without specifying a place,
		 * all() will call dflt_place() which was really intended
		 * to only handle STUFF. So we have to tell it we have lyrics,
		 * not a true STUFF. */
		set_stuff_type(-1);
	};

using:
	{
		Using_staff = -1;
		Using_voice = -1;
	}

	|
	T_USING staffnum
	{
		Using_staff = $2;
		Using_voice = 1;
	}

	|
	T_USING staffnum voicenum
	{
		Using_staff = $2;
		Using_voice = $3;
	};

opt_place:
	{
		/* empty, use default */
		begin_range(PL_UNKNOWN);
		$$ = PL_UNKNOWN;
	}

	|
	T_PLACE
	{
		begin_range($1);
	};

ph_opt_place:
	{
		$$= PL_UNKNOWN;
	}

	|
	T_PLACE 
	{
		if ($1 == PL_BETWEEN) {
			yyerror("place for ph, if specified, must be above or below, not between");
		}
		$$ = $1;
	};

staff_list:	staff_item

	|
	T_ALL
	{
		/* means goes above or below all the staffs, regardless if
		 * some happen to be invisible at the moment, so find top
		 * or bottom visible staff as appropriate,and save that
		 * away as the staff range */
		all();
	}

	|
	staff_list T_COMMA staff_item
	;

staff_item: num staff_range
	{
		/* if staff_range == 0, then only one staff specified, so
		 * use first staff number for both beginning and end of range */
		save_staff_range($1, ($2 == 0 ? $1 : $2) );
	};

staff_range:
	{
		/* empty */
		chk_range_type(NO);
		$$ = 0;
	}

	|
	T_DASH num
	{
		chk_range_type(NO);
		$$ = $2;
		if ($2 == 0) {
			yyerror("staff of 0 is illegal");
		}
	}

	| 
	T_AMPERSAND num
	{
		chk_range_type(YES);
		$$ = $2;
	};

more_lyric_ids:
	/* empty */
	
	|
	more_lyric_ids T_SEMICOLON T_PLACE { Place = $3; } staff_list
	;

verse_spec:
	verselist
	{
	}

	|
	T_PITCH
	{
		if (*yytext != 'c') {
			yyerror("verse must be 'c' or number(s)");
		}
		else {
			save_vno_range(0, 0);
		}
	};

verselist:
	verse_item
	{
	}

	|
	verselist T_COMMA verse_item
	{
	};

verse_item:
	num verse_range
	{
		/* if end of range is 0, not really a range, use first number
		 * for both beginning and end */
		lyr_verse($1, ($2 == 0 ? $1 : $2));
	};

verse_range:
	{
		/* empty */
		$$ = 0;
	}

	|
	T_DASH num
	{
		$$ = $2;
	};

alloc_lyr_grpsyl:
	{
		/* null token to allocate a GRPSYL for a lyric */
		Last_grpsyl_p = Curr_grpsyl_p;
		Curr_grpsyl_p = newGRPSYL(GS_SYLLABLE);
		if (Last_grpsyl_p == (struct GRPSYL *) 0) {
			Lyrics_p = Curr_grpsyl_p;
		}
	};

lyricdata:	lyr_tlist lyr_verse
	{
		/* If user didn't specify a place, fix that. */
		if (Place == PL_UNKNOWN) {
			Place = PL_BELOW;
		}
		/* copies of the lyrics info has been made for all staffs/verses
		 * at this point, so get rid of master copy */
		free_grpsyls(Lyrics_p);
	};

lyr_tlist:
	{
		/* empty -- need to derive times from music */
		Lyrics_p = derive_lyrtime();
	}

	|
	lyrics_timelist
	{
		not_derived();
	}
	;

lyrics_timelist: lyr_time_item

	|
	lyrics_timelist lyr_time_item 
	;

lyr_time_item:	lyr_tm T_SEMICOLON
	;

lyr_tm:	alloc_lyr_grpsyl time_val opt_space
	{
		link_notegroup(Curr_grpsyl_p, Last_grpsyl_p);
		/* Save pointer to current GRPSYL in case the next
		 * group gets its time value based on this one,
		 * and there are additive times on this one. */
		Prev_grpsyl_p = Curr_grpsyl_p;
		/* If there are additive times, add those in.
		 * We don't need to make extra groups for lyrics, because we
		 * don't need to tie groups together--the lyrics time
		 * can hold any legal RATIONAL, even those that aren't
		 * specifiable with a single time value. */
		if (Extra_time_p != 0) {
			struct TIMELIST *timelist_p;
			for (timelist_p = Extra_time_p; timelist_p != 0;
						timelist_p = timelist_p->next) {
				Curr_grpsyl_p->fulltime = radd(Curr_grpsyl_p->fulltime, timelist_p->fulltime);
			}
		}
	}

	|
	lyrtuplet
	{
	};

lyrtuplet:	start_tuplet lyrics_timelist rbrace num opt_time
	{
		end_tuplet($4, $5, NO, PL_UNKNOWN);
		Getting_tup_dur = NO;
	};

opt_space:
	{
		/* empty, will fill in an actual syllable later */
		Curr_grpsyl_p->syl = (char *) 0;
	}

	|
	T_LET_S
	{
		/* space, not a lyric, so mark for later use */
		Curr_grpsyl_p->grpcont = GC_SPACE;
	};

lyr_verse:	verse_string

	|
	lyr_verse verse_string
	;

verse_string:
	string T_SEMICOLON
	{
		/* If no [verseno] is specified, use -1 as special flag
		 * to be resolved later to "one more than previous",
		 * or if there wasn't a previous, to verse 1.
		 */
		lyr_verse(-1, -1);
		proc_lyrics(Lyrics_p, $1);
	}

	|
	T_LBRACKET verse_spec T_RBRACKET string T_SEMICOLON
	{
		proc_lyrics(Lyrics_p, $4);
	};

os_directive:	barinfo bar_items
	{
		/* If user set measnum=every N and did not already put some
		 * kind of reh mark on this bar, and this measure is a
		 * multiple of N, we add a mnum mark,
		 * but using measnumstyle */
		if (Currstruct_p != 0 && IS_EVERY(Score.measnum) &&
				Currstruct_p->u.bar_p->reh_type == REH_NONE &&
				Meas_num % Score.measnum == 0) {
			Currstruct_p->u.bar_p->reh_type = REH_BAR_MNUM;
			/* Do like the code in set_reh_string() except use the
			 * measnum* parameters for family, font, and size. */
			Currstruct_p->u.bar_p->reh_string =
					copy_string(num2str(Meas_num) + 2,
                        		Score.measnumfamily + Score.measnumfont,
                        		Score.measnumsize);
		}

		Currstruct_p = (struct MAINLL *) 0;
		/* If there are alternating time signature,
		 * add an implicit time signature SSV for the
		 * next time signature in the list.
		 */
		if (Alt_timesig_list != 0) {
			end_prev_context();
			Context = C_SCORE;
			Currstruct_p = newMAINLLstruct(S_SSV, -1);
			Currstruct_p->u.ssv_p->context = Context;
			if (Tsig_visibility == PTS_ALWAYS &&
					Next_alt_timesig != Alt_timesig_list) {
				/* If user wants alternating time signatures
				 * printed on every measure, if there is
				 * a multirest, we will print multiple
				 * time signatures there, and might have to
				 * wrap around to the beginning of the list.
				 * So make a copy of the entire list,
				 * starting from wherever we are now,
				 * wrapping around to the beginning,
				 * and ending just before where we are now.
				 * Most of the time,
				 * there probably won't be a multirest,
				 * so this will be a waste, but unfortunately,
				 * we don't know yet whether there will be
				 * one or not, so need to do the whole list
				 * just in case.
				 *
				 * Calculate length of the two pieces:
				 * from where we are to end, and from
				 * beginning to where we are.
				 */
				int remlength;
				int wraplength;
				remlength = strlen(Next_alt_timesig);
				wraplength = strlen(Alt_timesig_list)
						- remlength;
				/* need one more for terminator. */
				MALLOCA(char, Currstruct_p->u.ssv_p->timerep,
						remlength + wraplength + 1);

				/* copy remainder into beginning */
				strcpy(Currstruct_p->u.ssv_p->timerep,
						Next_alt_timesig);
				/* copy the wrap-around part of list,
				 * but move the TSR_ALTERNATING from the
				 * end of that part to between the two parts */
				Currstruct_p->u.ssv_p->timerep[remlength]
						= TSR_ALTERNATING;
				strncpy(Currstruct_p->u.ssv_p->timerep +
						remlength + 1, Alt_timesig_list,
						wraplength - 1);
				/* Add the terminator */
				Currstruct_p->u.ssv_p->timerep
						[remlength + wraplength]
						= TSR_END;

				assign_timesig(Currstruct_p, PTS_ALWAYS,
							&Next_alt_timesig);


				/* Make this new copy the new
				 * head of the list */
				Alt_timesig_list =
						Currstruct_p->u.ssv_p->timerep;
			}
			else {
				/* make a copy of the remaining alternating
				 * signatures and assign that */
				MALLOCA(char, Currstruct_p->u.ssv_p->timerep,
					strlen(Next_alt_timesig) + 1);
				strcpy(Currstruct_p->u.ssv_p->timerep,
							Next_alt_timesig);
				assign_timesig(Currstruct_p,
					(Tsig_visibility == PTS_ALWAYS ?
					PTS_ALWAYS : PTS_NEVER),
					&Next_alt_timesig);

				/* If we reached end of list, start over */
				if (Next_alt_timesig == 0) {
					Next_alt_timesig = Alt_timesig_list;
				}
			}

			asgnssv(Currstruct_p->u.ssv_p);
			end_prev_context();
			Context = C_MUSIC;
		}
	}

	|
	line

	|
	curve

	|
	T_SCOREFEED
	{
		struct MAINLL * mll_p;
		char *name;


		name = (yylval.intval == YES ? "newpage" : "newscore");

		(void) contextcheck(C_MUSIC | C_BLOCK, name);
		if (Mainlltc_p != 0 && Mainlltc_p->str == S_FEED) {
			/* Must be in block; Last thing on the list is a
			 * FEED, and it is illegal to have two FEEDs in a
			 * row, we just reuse the existing */
			Currstruct_p = Mainlltc_p;
			/* If this need feed is a pagefeed, set that in the
			 * existing struct. If not, leave it however it was.
			 * Otherwise if it was already a pagefeed, we would
			 * be downgrading, and we probably want to max */
			if ($1 == YES) {
				Currstruct_p->u.feed_p->pagefeed = $1;
			}
		}
		else {
			Currstruct_p = newMAINLLstruct(S_FEED, yylineno);
			Currstruct_p->u.feed_p->pagefeed = $1;
		}

		/* make sure we're not in the middle of a measure. Go
		 * backwards in main list. If we hit STAFF before a BAR,
		 * then there is a problem.
		 */
		for (mll_p = Mainlltc_p; mll_p != (struct MAINLL *) 0;
					mll_p = mll_p->prev) {

			if (mll_p->str == S_BAR) {
				break;
			}
			else if (mll_p->str == S_STAFF) {
				l_yyerror(Curr_filename, yylineno,
					"%s not allowed in middle of measure",
					name);
				break;
			}
			else if (mll_p->str == S_FEED) {
				struct MAINLL *m_p;
				/* Need to look backwards for blocks, but need
				 * to ignore any SSVs that are in between */
				for (m_p = mll_p->prev;
						m_p != 0 && m_p->str == S_SSV;
						m_p = m_p->prev) {
					;
				}
				if (m_p != 0 && m_p->str == S_BLOCKHEAD) {
					/* This is a feed following a block.
					 * If it was an implicit feed,
					 * we can get rid of it, because
					 * user now put an explicit one.
					 */
					if (mll_p->inputlineno == -1) {
						unlinkMAINLL(mll_p);
						FREE(mll_p);
					}
					/* We must be in block context,
					 * so no need to check farther back.
					 */
					break;
				}
				else {
					l_yyerror(Curr_filename, yylineno,
						"consecutive newscore/newpage not allowed");
				}
				break;
			}
		}
		/* Add to main list, unless reuing the tail cell FEED */
		if (Currstruct_p != Mainlltc_p) {
			insertMAINLL(Currstruct_p, Mainlltc_p);
		}

		/* If this is inside a block, we need to create a new block
		 * and feed after it.
		 */
		if (Context == C_BLOCK) {
			CALLOC(BLOCKHEAD, Currblock_p, 1);
			set_win_coord(Currblock_p->c);
			Next_print_link_p_p = &(Currblock_p->printdata_p);
			insertMAINLL(newMAINLLstruct(S_BLOCKHEAD, yylineno),
						Mainlltc_p);
			Mainlltc_p->u.blockhead_p = Currblock_p;
			insertMAINLL(newMAINLLstruct(S_FEED, -1), Mainlltc_p);
		}

	}
	scorefeed_options
	{
		Currstruct_p = (struct MAINLL *) 0;
	}

	|
	printcmd
	;

scorefeed_options:
	/* empty */

	|
	scorefeed_options scorefeed_item
	;

scorefeed_item:
	temp_margin

	|
	temp_scoresep
	;

temp_margin:	T_FNUMVAR opt_equals opt_plus_or_minus margin_override
	{
		int mot;	/* margin override type */

		if ($3 == 0) {
			/* User didn't specify a sign, so this is an
			 * absolute, not relative value. Or auto. */
			if ($4 == MG_AUTO) {
				mot = MOT_AUTO;
			}
			else {
				mot = MOT_ABSOLUTE;
			}
		}
		else {
			mot = MOT_RELATIVE;
		}

		if ($4 > 0.0) {
			$3 = adjust2inches($3);	/* in case we are in centimeter mode */
		}
		if ($1 == RIGHTMARGIN) {
			if (Currstruct_p->u.feed_p->right_mot != MOT_UNUSED) {
				l_warning(Curr_filename, yylineno,
					"rightmargin specified more than once, using last instance");
				/* fall through to override the previous */
			}
			if (mot == MOT_ABSOLUTE) {
				Currstruct_p->u.feed_p->rightmargin = $4;
			}
			else if (mot == MOT_RELATIVE) {
				Currstruct_p->u.feed_p->rightmargin
						= Score.rightmargin + $3 * $4;
				/* Even if user specified as relative,
				 * we have now converted to absolute */
				mot = MOT_ABSOLUTE;
			}
			Currstruct_p->u.feed_p->right_mot = mot;
		}
		else if ($1 == LEFTMARGIN) {
			if (mot == MG_AUTO) {
				l_warning(Curr_filename, yylineno,
					"auto can only be used with rightmargin, not leftmargin; ignoring");
				mot = MOT_UNUSED;
			}
			if (Currstruct_p->u.feed_p->left_mot != MOT_UNUSED) {
				l_warning(Curr_filename, yylineno,
					"leftmargin specified more than once, using last instance");
				/* fall through to override the previous */
			}
			if (mot == MOT_ABSOLUTE) {
				Currstruct_p->u.feed_p->leftmargin = $4;
			}
			else if (mot == MOT_RELATIVE) {
				Currstruct_p->u.feed_p->leftmargin
						= Score.leftmargin + $3 * $4;
				/* Even if user specified as relative,
				 * we have now converted to absolute */
				mot = MOT_ABSOLUTE;
			}
			Currstruct_p->u.feed_p->left_mot = mot;
		}
		else {
			yyerror("unexpected parameter; only 'leftmargin' or 'rightmargin' allowed here");
		}
		chkmargin(Score.topmargin, Score.botmargin,
				Currstruct_p->u.feed_p->leftmargin,
				Currstruct_p->u.feed_p->rightmargin);
	};
margin_override:
	floatnum
	{
		$$ = $1;
	}

	|
	T_AUTO
	{
		$$ = MG_AUTO;
	};

temp_scoresep:
	T_SCORESEP opt_equals opt_minus floatnum
	{
		if ( SEP_IS_VALID(Currstruct_p->u.feed_p->scoresep) ) {
			l_warning(Curr_filename, yylineno, "scoresep specified more than once, last being used");
		}
		if ( Currstruct_p->u.feed_p->pagefeed == YES) {
			l_warning(Curr_filename, yylineno, "scoresep ignored on newpage; only used on newscore");
		}
		frangecheck($4, -MAXSEPVAL, MAXSEPVAL, "scoresep value");
		Currstruct_p->u.feed_p->scoresep = $3 *$4;
	};

opt_equals:	/* optional equal sign */
	/* empty */
	|
	T_EQUAL
	;

opt_semi:
	/* optional semi-colon */
	|
	T_SEMICOLON
	;
	
directive:
	stuff_info

	|
	roll_cmd
	;

barinfo:	linetype precbartype barline
	{

		/* build a BAR struct and add it to the main list */
		if (contextcheck(C_MUSIC, "bar") == YES) {

			Currstruct_p = add_bar($3, $1, Endingloc,
						Endending_type, Mainlltc_p);
			Got_ending = NO;
		}
		if ($3 == RESTART && Endingloc != NOITEM) {
			yyerror("restart cannot be used inside an ending");
		}

		/* User can specify bar type to use on preceding staff when
		 * a repeatstart gets moved to the next scores's pseudo-bar */
		if ($2 != -1) {
			if ($3 != REPEATSTART) {
				l_warning(Curr_filename, yylineno,
					"bar type for preceding score only allowed on repeatstart; ignoring");
			}
			else if (Currstruct_p != 0 && Currstruct_p->u.bar_p != 0) {
				Currstruct_p->u.bar_p->precbartype = $2;
				if ($2 != SINGLEBAR && $2 != DOUBLEBAR &&
						(Linetype == L_DASHED ||
						Linetype == L_DOTTED)) {
					l_warning(Curr_filename, yylineno,
						"dashed/dotted only allowed on bar or dblbar; ignoring");
					Linetype = L_NORMAL;
				}
				Currstruct_p->u.bar_p->linetype = Linetype;
			}
		}
		else {
			/* Default is single bar. It could be argued that in
			 * the case of a key change on this bar, it really
		 	 * should be a dblbar, but user can force that
			 * if they want it. */
			if (Currstruct_p != 0 && Currstruct_p->u.bar_p != 0) {
				Currstruct_p->u.bar_p->precbartype = SINGLEBAR;
			}
		}
		/* Placement phase indirectly uses Doing_tab_staff when it
		 * calls notecomp, so clear it in case we don't get a
		 * non-tab staff input before the end of file.
		 * We could probably get by just doing after parsing is done,
		 * but this seems cleaner, and cheap enough.
		 */
		Doing_tab_staff = NO;
	};

precbartype:
	/* empty */
	{
		$$ = -1;	/* special value to mean "none specified" */
		Linetype = L_NORMAL;
	}

	|
	T_LPAREN linetype T_BARTYPE T_RPAREN
	{
		/* Parens wouldn't be strictly necessary to be able to parse,
		 * but seem useful semantically to make it clear this is
		 * optional, only applying when repeatstart is moved to
		 * next score's pseudo bar. */
		$$ = $3;
		if ($2 == L_DOTTED || $2 == L_DASHED) {
			if ($3 != SINGLEBAR && $3 != DOUBLEBAR) {
				l_warning(Curr_filename, yylineno,
					"Only bar or dblbar can be dashed or dotted; ignoring");
			}
			else {
				Linetype = $2;
			}
		}
		else if ($2 != L_NORMAL) {
			l_warning(Curr_filename, yylineno,
					"Bar line type modifier can only can be dashed or dotted; ignoring");
		}
	};

barline:	bartype emptymeas_input
	{
		/* Call proc_emptymeas() to free up the last buffer, if any,
		 * and return back to normal input stream. */
		proc_emptymeas(MAXSTAFFS+1);
		$$ = $1;
	};

bartype:	T_BARTYPE
	{
		/* If there was any empty voices in the input for this
		 * measure which have emptymeas parameter set,
		 * proc_emptymeas() will arrange to create just-in-time
		 * virtual input for it that will get parsed via
		 * the emptymeas_input rule.
		 * We pass arg of 1 to tell it to start looking from
		 * staff 1. Subsequent calls to proc_emptymeas(), if any,
		 * will generate input for additional voices, until
		 * no more are needed for this measure.
		 */
		if ($1 != RESTART) {
			EM_staff = proc_emptymeas(1);
		}
		else {
			/* don't create any music before a restart */
			EM_staff = MAXSTAFFS + 1;
		}
		$$ = $1;
	};

emptymeas_input:	/* empty */
	{
	}

	|
	emptymeas_input T_EM_BEGIN music_input T_EM_END
	{
		/* See if there is any more virtual input needed */
		EM_staff = proc_emptymeas(EM_staff);
	};

bar_items:

	|
	bar_items bar_opt
	;

bar_opt:
	padspec
	{
		if (Currstruct_p != (struct MAINLL *) 0) {
			Currstruct_p->u.bar_p->padding += $1 * STEPSIZE;
		}
	}

	|
	T_EQUAL loc_variable
	{
		if (Currstruct_p != (struct MAINLL *) 0) {
			/* fill in location info */
			if ($2 != (char *) 0) {
				addsym($2, Currstruct_p->u.bar_p->c, CT_BAR);
			}
		}
	}

	|
	ending_info
	{
		if (Got_ending == YES) {
			yyerror("Only one ending allowed per bar");
		}
		Got_ending = YES;

		if (Currstruct_p != (struct MAINLL *) 0) {
			if (Currstruct_p->u.bar_p->bartype == RESTART) {
				yyerror("ending not allowed on restart");
			}
			/* fill in ending label if any */
			Currstruct_p->u.bar_p->endinglabel = $1;
			Currstruct_p->u.bar_p->endingloc = Endingloc;
			Currstruct_p->u.bar_p->endending_type = Endending_type;
			ped_endings(Endingloc);

			/* for next time around, figure out what endingloc will
			 * be if user doesn't specify something different */
			switch (Endingloc) {
			case STARTITEM:
				Endingloc = INITEM;
				break;
			case ENDITEM:
				Endingloc = NOITEM;
				break;
			default:
				break;
			}
		}
	}

	|
	T_HIDECHANGES
	{
		if (Currstruct_p != 0) {
			if (Currstruct_p->u.bar_p->bartype == RESTART) {
				l_warning(Curr_filename, yylineno,
					"hidechanges not allowed on restart; ignoring");
			}
			else {
				Currstruct_p->u.bar_p->hidechanges = YES;
			}
		}
	}

	|
	T_MNUM T_EQUAL num {
		if (Currstruct_p != 0) {
			set_mnum(Currstruct_p->u.bar_p, $3);
		}
	}

	|
	T_MNUM plus_or_minus num {
		if (Currstruct_p != 0) {
			int newval;

			newval = Meas_num + ($2 * $3);
			if (newval < 1) {
				l_warning(Curr_filename, yylineno,
				"resulting measure number of %d is less than 1; using 1", newval);
				newval = 1;
			}
			set_mnum(Currstruct_p->u.bar_p, newval);
		}
	}

	|
	T_LET T_EQUAL string {
		/* +2 to skip font/size */
		init_reh(-1, $3 + 2, Currstruct_p);
	}

	|
	T_NUM T_EQUAL num {
		init_reh($3, (char *)0, Currstruct_p);
	}

	|
	T_REHEARSAL chk_dup_reh font_family title_font opt_size rehearsal_mark opt_dist
	{
		if (Currstruct_p != 0 && Currstruct_p->str == S_BAR) {
			if (Currstruct_p->u.bar_p->reh_string != 0) {
				/* This indicates user specified multiple
				 * rehearsal strings. We would have already
				 * given a warning message. */
				FREE(Currstruct_p->u.bar_p->reh_string);
				Currstruct_p->u.bar_p->reh_string = 0;
			}
			set_reh_string(Currstruct_p->u.bar_p, $3, $4, $5, $6);
			Currstruct_p->u.bar_p->dist = Dist;
			Currstruct_p->u.bar_p->dist_usage = Dist_usage;
		}
	};

ending_info:
	T_ENDING string
	{
		$$ = $2;
		(void) fix_string($2, FONT_TR, DFLT_SIZE, Curr_filename, yylineno);
		Endingloc = STARTITEM;
		Endending_type = EE_DEFAULT;
	}

	|
	T_ENDENDING
	{
		if (Endingloc == NOITEM) {
			l_warning(Curr_filename, yylineno,
				"no ending in progress; extraneous 'endending' ignored");
			Endending_type = EE_DEFAULT;
		}
		else {
			Endingloc = ENDITEM;
			Endending_type = $1;
		}
		$$ = (char *) 0;
	};

chk_dup_reh:
	{
		/* null token to check for more than one rehearsal mark
		 * on one BAR */
		if (Currstruct_p != (struct MAINLL *) 0 &&
				Currstruct_p->u.bar_p->reh_type != REH_NONE) {
			l_warning(Curr_filename, yylineno,
				"only one rehearsal mark allowed per bar; using last");
		}
	};

rehearsal_mark:	string
	{
		if (Currstruct_p != (struct MAINLL *) 0) {
			Currstruct_p->u.bar_p->reh_type = REH_STRING;
		}
	}

	|
	T_MNUM
	{
		if (Currstruct_p != (struct MAINLL *) 0) {
			Currstruct_p->u.bar_p->reh_type = REH_MNUM;
		}
		$$ = (char *) 0;
	}

	|
	T_NUM
	{
		if (Currstruct_p != (struct MAINLL *) 0) {
			Currstruct_p->u.bar_p->reh_type = REH_NUM;
		}
		$$ = (char *) 0;
	}

	|
	T_LET
	{
		if (Currstruct_p != (struct MAINLL *) 0) {
			Currstruct_p->u.bar_p->reh_type = REH_LET;
		}
		$$ = (char *) 0;
	};

location:	T_LPAREN alloc_loc xcoord T_COMMA ycoord T_RPAREN
	{
		$$ = $2;
		$2->hexpr_p = $3;
		$2->vexpr_p = $5;
	};

alloc_loc:
	{
		/* null token to allocate an INPCOORD */
		CALLOC(INPCOORD, Curr_loc_info_p, 1);
		$$ = Curr_loc_info_p;
		/* set to positive in case input starts with an absolute
		 * x coordinate */
		Plus_minus = 1;
	};

plus_or_minus:	T_PLUS
	{
		$$ = Plus_minus = 1;
	}

	|
	T_DASH
	{
		$$ = Plus_minus = -1;
	};


xcoord:
	coord_setup expression
	{
		$$ = $2;
		Curr_timeref_tag_p = 0;
	}
	;

ycoord:
	expression
	{
		$$ = $1;

		/* Put lex back into normal (not coord arithemetic) mode */
		set_lex_mode(NO);
	}
	;

coord_setup:
	{
		/* Put lex into special mode to return different tokens
		 * for arithmetic operators. This is just in case the
		 * precedence rules might mess up other rules if we used
		 * the same tokens. The + - * / symbols are used for very
		 * different things elsewhere in the grammer, where
		 * precedence doesn't make sense. It probably wouldn't hurt,
		 * but it seems better to be safe, and only do the precedence
		 * when really needed.
		 */
		set_lex_mode(YES);
		Curr_timeref_tag_p = 0;
	}
	;


expression:
	T_LPAREN expression T_RPAREN
	{
		$$ = $2;
	}

	|
	expression T_ADDSUB_OP expression
	{
		$$ = newnode($2);
		$$->left.lchild_p = $1;
		$$->right.rchild_p = $3;
	}

	|
	expression T_MULDIV_OP expression
	{
		$$ = newnode($2);
		$$->left.lchild_p = $1;
		$$->right.rchild_p = $3;
	}

	|
	T_ADDSUB_OP expression %prec T_UNARY_OP
	{
		if ($1 == OP_SUB) {
			$$ = newnode(OP_MUL);
			$$->left.lchild_p = newnode(OP_FLOAT_LITERAL);
			$$->left.lchild_p->left.value = -1.0;
			$$->right.rchild_p = $2;
		}
		else {
			$$ = $2;
		}
	}

	|
	T_1ARG_FUNC T_LPAREN expression T_RPAREN
	{
		$$ = newnode($1);
		$$->left.lchild_p = $3;
	}


	|
	T_2ARG_FUNC T_LPAREN expression T_COMMA expression T_RPAREN
	{
		$$ = newnode($1);
		$$->left.lchild_p = $3;
		$$->right.rchild_p = $5;
	}

	|
	floatnum
	{
		$$ = newnode(OP_FLOAT_LITERAL);
		$$->left.value = $1;
	}

	|
	variable T_DOT direction
	{
		$$ = newnode(OP_TAG_REF);

		/* Get space to save info about this tag reference */
		MALLOC(TAG_REF, $$->left.ltag_p, 1);

		/* Get the address of the c[] associated with this tag.
		 * We also save which index into the c[]
		 */
		if (($$->left.ltag_p->c = symval($1, &($$->left.ltag_p->c))) != 0) {
			$$->left.ltag_p->c_index = $3;
			save_tag_ref($$->left.ltag_p->c, &($$->left.ltag_p->c));
		}

		if (index_type($3) == AX) {
			/* Remember this tag, in case user
			 * wants to do a time offset relative to it.
			 * We always take time offsets relative
			 * to the most recent horizontal tag
			 * in the current expression.
			 * Since there may be expressions as arguments
			 * to functions, we keep a stack of these. */
			Curr_timeref_tag_p = $$->left.ltag_p;
		}

		/* If this is the first tag of the proper direction in the
		 * expression, that's the base tag used to determine
		 * which staff/score the tag is associated with, if any */
		if (index_type($3) == AX && Curr_loc_info_p->hor_p == 0) {
			Curr_loc_info_p->hor_p = $$->left.ltag_p->c;
			save_tag_ref(Curr_loc_info_p->hor_p,
						&(Curr_loc_info_p->hor_p));
		}
		else if (index_type($3) == AY && Curr_loc_info_p->vert_p == 0) {
			Curr_loc_info_p->vert_p = $$->left.ltag_p->c;
			save_tag_ref(Curr_loc_info_p->vert_p,
						&(Curr_loc_info_p->vert_p));
		}
	}

	|
	T_TIME floatnum
	{
		if (Curr_timeref_tag_p == 0) {
			yyerror("time offset requires a prior horizontal tag reference");
			$$ = 0;
		}
		else if (is_builtin_tag(Curr_timeref_tag_p->c)) {
			yyerror("cannot have a time offset relative to a builtin tag like _win or _page");
			$$ = 0;
		}
		else {
			$$ = newnode(OP_TIME_OFFSET);
			$$->left.value = $2;
			MALLOC(TAG_REF, $$->right.rtag_p, 1);
			$$->right.rtag_p = Curr_timeref_tag_p;
			save_tag_ref($$->right.rtag_p->c,
						&($$->right.rtag_p->c));
		}
	}
	;

direction:
	T_LETTER
	{
		/* only valid letters are w and y */
		if (*yytext == 'w') {
			$$ = AW;
		}
		else if (*yytext == 'y') {
			$$ = AY;
		}
		else {
			yyerror("invalid direction: must be x, y, e, w, n or s");
			/* Return something valid, so if any code tries
			 * to use it later, it won't core dump */
			$$ = AX;
		}
	}
	|
	T_LET_X
	{
		$$ = AX;
	}

	|
	T_PITCH
	{
		/* only valid value is e */
		if (*yytext != 'e') {
			yyerror("invalid direction: must be x, y, e, w, n or s");
		}
		$$ = AE;
	}

	|
	T_LET_N
	{
		$$ = AN;
	}

	|
	T_LET_S
	{
		$$ = AS;
	}
	;

floatnum:	num
	{
		$$ = (float) $1;
	}

	| num T_DOT opt_decimal_part
	{
		sprintf(Tmpbuff,"%d.%s", $1, $3);
		$$ = (float) atof(Tmpbuff);
	}

	| T_DOT numstr
	{
		sprintf(Tmpbuff,"0.%s", $2);
		$$ = (float) atof(Tmpbuff);
	};

opt_decimal_part:
	{
		/* no decimal fraction part of float number */
		$$ = "";
	}

	|
	numstr
	;

numstr:	T_NUMBER
	{
		$$ = yytext;
	};

variable:	loc_variable
	;

line:		linetype T_LINE line_alloc location T_TO location opt_line_str
	{
		Currstruct_p->u.line_p->linetype = $1;
		Currstruct_p->u.line_p->start = *($4);
		Currstruct_p->u.line_p->end = *($6);
		rep_inpcoord($4, &(Currstruct_p->u.line_p->start));
		rep_inpcoord($6, &(Currstruct_p->u.line_p->end));
		Currstruct_p->u.line_p->string = $7;

		/* copies of the location info went into the LINE struct,
		 * so we can free the original copy of the information */
		if ($4) {
			FREE($4);
		}
		if ($6) {
			FREE($6);
		}
		insertMAINLL(Currstruct_p, Mainlltc_p);
		Currstruct_p = (struct MAINLL *) 0;
	};

line_alloc:
	{
		/* null token to cause allocation of LINE struct */
		(void) contextcheck(C_MUSIC, "line");
		Currstruct_p = newMAINLLstruct(S_LINE, yylineno);
	};

linetype:
	{
		$$ = L_NORMAL;
	}

	|
	T_LINETYPE
	;

opt_line_str:
	/* empty */
	{
		$$ = 0;
	}

	|
	T_WITH tfont title_size string {
		$$ = fix_string($4, Titlefont, Titlesize, Curr_filename, yylineno);
	}
	;

curve:		linetype T_CURVE curve_alloc curve_loc T_TO curve_loc curve_tail
	{
		if ($1 == L_WAVY) {
			l_warning(Curr_filename, yylineno,
						"wavy curve not allowed; using normal curve");
			$1 = L_NORMAL;
		}
		Currstruct_p->u.curve_p->curvetype = $1;
		insertMAINLL(Currstruct_p, Mainlltc_p);
		Currstruct_p = (struct MAINLL *) 0;
	};

curve_alloc:
	{
		(void) contextcheck(C_MUSIC, "curve");
		Currstruct_p = newMAINLLstruct(S_CURVE, yylineno);

		/* get space for a list of locations and mark it as empty */
		Item_count = 0;
		Currstruct_p->u.curve_p->ncoord = (short) Item_count;
		Currstruct_p->u.curve_p->nbulge = (short) Item_count;
		MALLOC(INPCOORD, Currstruct_p->u.curve_p->coordlist, ITEMS);
		Max_items = ITEMS;
	};

curve_tail:
	T_TO loc_list

	|
	T_BULGE bulgelist
	;

loc_list:	curve_loc

	|
	loc_list T_TO curve_loc
	;

bulgelist:
	bulge_value
	
	|
	bulgelist T_COMMA bulge_value
	;

bulge_value:
	opt_minus floatnum
	{
		/* get enough space to store some values if don't have enough */
		if (Currstruct_p->u.curve_p->nbulge == 0) {
			MALLOCA(float, Currstruct_p->u.curve_p->bulgelist, ITEMS);
		}
		else if ( (Currstruct_p->u.curve_p->nbulge % ITEMS) == 0) {
			REALLOCA(float, Currstruct_p->u.curve_p->bulgelist,
				Currstruct_p->u.curve_p->nbulge + ITEMS);
		}

		/* fill in the value and increment the count of how many */
		Currstruct_p->u.curve_p->bulgelist[Currstruct_p->u.curve_p->nbulge] = $1 * $2;
		(Currstruct_p->u.curve_p->nbulge)++;
	};

curve_loc:	location
	{
		struct INPCOORD *curve_inpcoord_p;

		/* If ran off end of list, make list bigger.
		 * We cannot use REALLOC here, since we need to update
		 * the tag reference information on each individual array
		 * element, so we alloc all new space, copy the existing data,
		 * and update the tag references, then free the old space,
		 * and redirect the pointer to the new space. */
		if (Currstruct_p->u.curve_p->ncoord >= Max_items) {
			struct INPCOORD *newlist_p;
			int n;
			Max_items += ITEMS;
			MALLOC(INPCOORD, newlist_p, Max_items);
			for (n = 0; n < Currstruct_p->u.curve_p->ncoord; n++) {
				newlist_p[n] = Currstruct_p->u.curve_p->coordlist[n];
				rep_inpcoord(
				    &(Currstruct_p->u.curve_p->coordlist[n]),
				    &(newlist_p[n]));
			}
			FREE(Currstruct_p->u.curve_p->coordlist);
			Currstruct_p->u.curve_p->coordlist = newlist_p;
		}

		/* Add this entry to the end of the list, update the
		 * tag reference to point to this permanent copy rather
		 * the temporary one we will be deleting, and update the
		 * count of how many elements in the list. */
		curve_inpcoord_p = &(Currstruct_p->u.curve_p->coordlist
					[Currstruct_p->u.curve_p->ncoord]);
		*curve_inpcoord_p = *($1);
		rep_inpcoord($1, curve_inpcoord_p);
		(Currstruct_p->u.curve_p->ncoord)++;
		FREE($1);
	};

stuff_info: stuff_header T_COLON stufflist
	{
		attach_stuff();
		/* so reset flag */
		Defining_multiple = NO;
	};

stuff_header:
	stuff_id opt_size opt_modifier opt_place staff_list opt_dist opt_align
	{
		chk_stuff_header($2, $3, $4, Dist_usage);
		add_to_sv_list();
	}

	|
	linetype T_PHRASE opt_size opt_place staff_list opt_dist
	{
		/* Separate rule for phrase because it has the linetype
		 * modifier in front, unlike any other STUFF. The size,
		 * and dist are not really allowed, but we match them
		 * if they are there in order to give more clear error
		 * message than just "syntax error," since there was
		 * already code elsewhere to check for these errors.
		 */
		set_stuff_type(ST_PHRASE);
		chk_stuff_header($3, $1, $4, Dist_usage);
		add_to_sv_list();
		Aligntag = NOALIGNTAG;
	}
	|
	midi midi_staff
	{
		/* need a separate rule for midi, because it can include
		 * a voice as well as staff. It also cannot have the chord,
		 * size or place, although those are checked elsewhere anyway */
		chk_stuff_header(-1, NO, PL_UNKNOWN, SD_NONE);
		Aligntag = NOALIGNTAG;
	};

midi:
	T_MIDI
	{
		set_stuff_type(ST_MIDI);
		begin_range(PL_UNKNOWN);
	};

midi_staff:
	voice_id

	|
	T_ALL
	{
		all();
		add_to_sv_list();
	};

stuff_id:
	stuff_type
	{
		/* dummy token for sole purpose of saving away current
		 * stuff type. We have to do this in order to have it in
		 * time to figure out the default place for "all" */
		set_stuff_type($1);
	};

stuff_type:
	font_family T_FONT
	{
		Curr_family = $1;

		switch ($2) {
		case FONT_TR:
			$$ = ST_ROM;
			break;
		case FONT_TI:
			$$ = ST_ITAL;
			break;
		case FONT_TB:
			$$ = ST_BOLD;
			break;
		case FONT_TX:
			$$ = ST_BOLDITAL;
			break;
		default:
			pfatal("unknown font");
			break;
		}
	}

	|
	T_L_ANGLE
	{
		if ($1 != L_NORMAL) {
			/* user used 'dotted' or 'dashed' which applies only
			 * to use of T_L_ANGLE for slurs. Use generic error
			 * message so looks like any other similar error. */
			yyerror("syntax error");
		}
		$$ = ST_CRESC;
	}

	|
	T_R_ANGLE
	{
		$$ = ST_DECRESC;
	}

	|
	T_PEDAL
	{
		$$ = ST_PEDAL;
	}

	|
	T_MUSSYM
	{
		$$ = ST_MUSSYM;
	}

	|
	T_OCTAVE
	{
		$$ = ST_OCTAVE;
	}
	;

opt_size:
	{
		/* use -1 to indicate user didn't specify so must use default */
		$$ = -1;
	}

	|
	T_LPAREN num T_RPAREN
	{
		$$ = $2;
	};

opt_modifier:
	{
		$$ = TM_NONE;
	}

	|
	T_MODIFIER
	{
		$$ = $1;
	};

opt_dist:
	{
		Dist = 0.0;
		Dist_usage = SD_NONE;
	}

	|
	T_FNUMVAR stuff_dist
	{
		if ($1 != DIST) {
			yyerror("unexpected parameter name instead of 'dist'");
		}
		else {
			if (frangecheck($2, -MAXDIST, MAXDIST, "dist") == YES) {
				Dist = $2;
			}
		}
	};

stuff_dist:
	floatnum opt_exclam
	{
		$$ = $1;
	}

	|
	T_DASH floatnum opt_exclam
	{
		if (Dist_usage != SD_FORCE) {
			l_warning(Curr_filename, yylineno,
				"negative dist requires !, setting to 0");
			$$ = 0.0;
		}
		else {
			$$ = - $2;
		}
	};

opt_exclam:
	{
		Dist_usage = SD_MIN;
	}

	|
	T_EXCLAM
	{
		Dist_usage = SD_FORCE;
	};

opt_align:
	{
		Aligntag = NOALIGNTAG;
	}

	|
	T_ALIGN num
	{
		if (rangecheck($2, MINALIGNTAG, MAXALIGNTAG, "align tag number") == YES) {
			Aligntag = $2;
		}
		else {
			Aligntag = NOALIGNTAG;
		}
	};
	
stufflist:
	stuff_item

	|
	stufflist stuff_item
	;

stuff_item:
	beat_offset steps_offset opt_string til_clause T_SEMICOLON
	{
		add_stuff_item($1, $2, Start_gracebackup, $3,
			Til_bars, Til_offset, Til_steps, End_gracebackup,
			Dist, Dist_usage, Aligntag);
	};

beat_offset:
	floatnum gracebackup
	{
		Til_bars = 0;
		Til_offset = 0.0;
		Til_steps = 0.0;
		$$ = $1;
		Start_gracebackup = $2;
	};

gracebackup:
	{
		$$ = 0;
	}

	|
	T_LPAREN T_DASH num T_RPAREN
	{
		$$ = $3;
	};

steps_offset:
	{
		$$ = 0.0;
	}

	|
	T_LBRACKET plus_or_minus floatnum T_RBRACKET
	{
		$$ = $3 * (float) $2;
	};

opt_string:
	{
		$$ = (char *) 0;
	}

	|
	T_STAR
	{
		if (get_stuff_type() != ST_PEDAL) {
			yyerror("'*' only valid with pedal");
		}
		MALLOCA(char, $$, 12);
		$$[0] = FONT_MUSIC;
		$$[1] = DFLT_SIZE;
		sprintf($$ + 2, "\\(endped)");
	}

	|
	string
	{
		$$ = $1;
	};

til_clause:
	{
		End_gracebackup = 0;
	}

	|
	T_TIL til_val
	{
	};

til_val:
	floatnum til_suffix
	{
		/** if there is a non-empty til_suffix, the floatnum better
		 ** have been an integer, and is really the number
		 * of bars. **/
		if ($2 == YES) {
			Til_bars = (int) $1;
			if ($1 - Til_bars != 0.0) {
				yyerror("number of measures in til clause must be a whole number");
			}
		}
		else {
			/* only a number, that means it was really a beat offset */
			Til_offset = $1;
		}
	};

til_suffix:
	gracebackup steps_offset
	{
		$$ = NO;
		End_gracebackup = $1;
		Til_steps = $2;
	}

	|
	T_LET_M til_offset
	{
		$$ = YES;
	};

til_offset:
	gracebackup steps_offset
	{
		Til_offset = 0.0;
		Til_steps = $2;
		End_gracebackup = $1;
	}

	|
	T_PLUS floatnum gracebackup steps_offset
	{
		Til_offset = $2;
		Til_steps = $4;
		End_gracebackup = $3;
	};

roll_cmd:	T_ROLL opt_roll_dir roll_extent T_COLON roll_offsetlist
	;

opt_roll_dir:
	{
		newROLLINFO();
	}
	
	|
	T_UP
	{
		newROLLINFO();
		setrolldir(UP);
	}

	|
	T_DOWN
	{
		newROLLINFO();
		setrolldir(DOWN);
	};

roll_extent:
	roll_start roll_end
	;

roll_start:
	staffnum
	{
		/* save staffnum and voice 1 for start of roll */
		rollparam($1, 1, -1, -1);
	}

	|
	staffnum voicenum
	{
		/* save staff and voice for start of roll */
		rollparam($1, $2, -1, -1);
	} ;

roll_end:

	|
	T_TO staffnum
	{
		/* save staffnum, voice 1 for end of roll */
		rollparam(-1, -1, $2, 1);
	}

	|
	T_TO staffnum voicenum
	{
		/* save staffnum and voice for end of roll */
		rollparam(-1, -1, $2, $3);
	};

roll_offsetlist:
	floatnum T_SEMICOLON
	{
		rolloffset( (double) $1);
	}

	|
	roll_offsetlist floatnum T_SEMICOLON
	{	
		rolloffset( (double) $2);
	};

printcmd:	printtype opt_loc string
	{
		if (Curr_family == FAMILY_DFLT) {
			Curr_family = Score.fontfamily;
		}
		String1 = $3;
		Extra = extra_needed(Curr_family + Curr_font, Curr_size, &String1);
		proc_printcmd($1, Curr_loc_info_p, String1,
			Curr_family + Curr_font, Curr_size, $2,
			NO, PU_NORMAL, NO, 0,
			($2 == YES ? Extra : (double) 0.0));
	}

	|
	postscript ps_opt_loc export_list opt_file string
	{
		proc_printcmd(J_NONE, Curr_loc_info_p, $5, FONT_UNKNOWN,
				DFLT_SIZE, NO, YES, $2, $4, $3, (double) 0.0);
		if ($4 == NO) {
			end_raw();
		}
	};

postscript:
	T_POSTSCRIPT
	{
	};

opt_file:
	{
		begin_raw();
		$$ = NO;
	}

	|
	T_FILE
	{
		$$ = YES;
	};

ps_opt_loc:
	opt_loc
	{
		if ($1 == YES) {
			warning("nl ignored for postscript");
		}
		$$ = PU_NORMAL;
	}

	|
	T_PSHOOKLOC
	{
		Curr_loc_info_p = (struct INPCOORD *) 0;
		$$ = $1;
	};

export_list:
	{
		$$ = 0;
	}

	|
	T_WITH export_var_list
	{
		$$ = $2;
	};

export_var_list:
	export_item

	|
	export_var_list T_COMMA export_item
	{
		/* add onto list and return the list */
		struct VAR_EXPORT *exp_p;
		for (exp_p = $1; exp_p->next != 0; exp_p = exp_p->next) {
			;
		}
		exp_p->next = $3;
		$$ = $1;
	};

export_item:
	export_var
	{
		$1->tag_addr = symval($1->name, &($1->tag_addr));
		$$ = $1;
	}

	|
	aliased_var
	{
		$1->tag_addr = symval($1->name, &($1->tag_addr));
		$$ = $1;
	};

aliased_var:
	alias T_EQUAL export_var
	{
		if ($1->index == NUMCTYPE && $3->index != NUMCTYPE && $3->index != EXPORT_ALL_STAFFS) {
			l_yyerror(Curr_filename, yylineno,
			"if alias does not have a dot and direction, tag must not either");
		}
		if ($1->index != NUMCTYPE && $3->index == NUMCTYPE) {
			l_yyerror(Curr_filename, yylineno,
			"if alias has dot and direction, tag must also");
		}
		$3->alias = $1;
		$$ = $3;
	};

alias:	alias_or_export_var
	{
		$$ = $1;
	};

export_var:
	arraytag
	{
		if (strcmp($1, "_staff") != 0) {
			pfatal("got arraytag other than _staff");
		}
		$$ = add_export($1, EXPORT_ALL_STAFFS);
	}

	|
	alias_or_export_var
	{
		$$ = $1;
	};

alias_or_export_var:
	variable
	{
		/* export all six [nsewxy] values */
		$$ = add_export($1, NUMCTYPE);
	}

	|
	variable T_DOT direction
	{
		$$ = add_export($1, $3);
	};

printtype:	T_PRINTTYPE

	|
	justifytype
	;

justifytype:	T_JUSTIFYTYPE
	;

opt_loc:
	{
		/* this means print at default location */
		Curr_loc_info_p = (struct INPCOORD *) 0;
		$$ = NO;
	}

	|
	T_NL
	{
		Curr_loc_info_p = (struct INPCOORD *) 0;
		$$ = YES;
	}

	|
	location
	{
		$$ = NO;
	};


%%

#include <stdio.h>


/* print error message along with current filename and line number */

int
yyerror(msg)

char *msg;

{
	if (strncmp(msg, "syntax error", 12) == 0 ||
			strncmp(msg, "parse error", 11) == 0) {
		/* beef up yacc's default syntax error message */
		if (*yytext == '\n') {
			l_yyerror(Curr_filename, yylineno,
				"error detected at end of line (maybe missing semicolon or other required element?)");
		}
		else if (is_internal_token(yytext) == YES) { 
			/* We add some funny tokens for our internal use
			 * when doing emptymeas parameter expansion.
			 * It is conceivable that there could be
			 * some (rare) scenarios where a user error
			 * in their value for that parameter would cause the
			 * parser to detect the error when it got to our
			 * funny token, and it would be very confusing to the
			 * user if we printed that out as the offending token,
			 * since they didn't input it, and it is chosen
			 * specifically to be strange looking.
			 */
			l_yyerror(Curr_filename, yylineno,
				"error detected during internal emptymeas parameter processing");
		}
		else {
			l_yyerror(Curr_filename, yylineno,
				"error detected at '%s'", yytext);
		}
	}
	else {
		l_yyerror(Curr_filename, yylineno, "%s", msg);
	}

	/* return something to keep lint happy */
	return(0);
}


/* if argument is surrounded by double quotes, remove them and return the
 * unquoted string. Actually just NULL out the end quote and return a
 * pointer to beyond the leading quote */

static char *
stripquotes(string)

char *string;

{
	char *p;

	p = string + strlen(string) - 1;
	if (*p == '"') {
		*p = '\0';
	}
	return( *string == '"' ? string + 1 : string);
}


/* Check if the given header/footer type has already been specified,
 * and if so, issue a warning. In any case, set Currblock_p
 * to the passed in BLOCKHEAD pointer. */

static void
chkdup_headfoot(bit, which, block_p)

int bit;	/* which GOT_* bit is associated with this head/foot thing */
char *which;	/* header, footer, header2, footer2 or left/right versions */
struct BLOCKHEAD *block_p;	/* assign this to Currblock_p */

{
	if ((bit & Gotheadfoot) != 0) {
		l_yyerror(Curr_filename, yylineno,
				"%s context specified more than once", which);
	}
	Gotheadfoot |= bit;
	Currblock_p = block_p;
}


/* If the given top/bot block has already been set without having been used,
 * issue a warning. In any case, set it to the Currblock_p value.
 */

static void
chkdup_topbot(block_p_p, blocktype)

struct BLOCKHEAD **block_p_p;	/* address of a FEED field like top_p, bot2_p,
				 * lefttop_p, rightbot_p, etc. What it points
				 * to will be updated with the value of
				 * Currblock_p. */
char *blocktype;		/* Name to be used on error message if the
				 * field was already set before. */

{
	if (*block_p_p != 0) {
		warning("%s redefined before being used", blocktype);
	}
	*block_p_p = Currblock_p;

}


/* when entering new context, need to save away everything about current one */

static void
end_prev_context()

{
	struct MAINLL *place_p;		/* where to insert SSV in main list */


	/* assume we will place at the end of main list. Later we'll set
	 * the right value if that assumption is wrong */
	place_p = Mainlltc_p;

	/* If current main list item hasn't been added to list yet,
	 * do that now. */
	if (Currstruct_p != (struct MAINLL *) 0 && Currstruct_p != place_p) {

		if (Currstruct_p->str == S_SSV) {

			if (List_of_staffs_p != (struct MAINLL *) 0) {
				yyerror("can't change context inside a measure");
				Currstruct_p = (struct MAINLL *) 0;
				return;
			}

			check_beamstyle(Currstruct_p->u.ssv_p);

			/* if is an SSV struct, we also need to set the
			 * correct values at this point, because the
			 * parser needs to know current values of some
			 * things (like font and point size) */
			asgnssv(Currstruct_p->u.ssv_p);

			/* if this SSV is after a FEED, we have to move it
			 * before the FEED in order to follow the rules of
			 * what comes when on the main list */
			for (   ; place_p != (struct MAINLL *) 0 &&
					place_p->str == S_FEED;
					place_p = place_p->prev) {
			}
		}

		insertMAINLL(Currstruct_p, place_p);
	}

	if (Currstruct_p != 0 && Currstruct_p->str == S_SSV) {
		/* memorize or recall beamstyle and timeunit if appropriate */
		remember_tsig_params(Currstruct_p);

		/* If user specified multiple staffs/voices, clone the
		 * necessary SSV nows */
		if (Currstruct_p->u.ssv_p->context == C_STAFF) {
			clone_staff_ssvs(place_p);
			free_rlists();
		}
		else if (Currstruct_p->u.ssv_p->context == C_VOICE) {
			clone_voice_ssvs(place_p);
			free_rlists();
		}
		Defining_multiple = NO;
	}

	if (Curr_usym_p != 0) {
		define_usym();
		Curr_usym_p = 0;
	}

	Curr_grpsyl_p = (struct GRPSYL *) 0;
	Currblock_p = (struct BLOCKHEAD *) 0;
	Currstruct_p = (struct MAINLL *) 0;
	set_win_coord(0);
}


/* If user gave a list of staffs for a "staff" context, clone copies of
 * the SSV that we made for the first on the list for the rest of the list. */

static void
clone_staff_ssvs(insert_p)

struct MAINLL *insert_p;
{
	struct SVRANGELIST *svr_p;
	struct RANGELIST *sr_p;		/* list of staff ranges */
	struct SSV *clone_src_p;	/* the SSV to clone */
	int s;				/* staff number */

	clone_src_p = Currstruct_p->u.ssv_p;
	for (svr_p = Svrangelist_p; svr_p != 0; svr_p = svr_p->next) {
		for (sr_p = svr_p->stafflist_p; sr_p != 0; sr_p = sr_p->next) { 
			for (s = sr_p->begin; s <= sr_p->end && s <= Score.staffs; s++) {
				if (clone_src_p->staffno == s) {
					/* skip past the clone source */
					continue;
				}
				clone1ssv(clone_src_p, insert_p, s, clone_src_p->voiceno);
			}
		}
	}
}


/* If user gave a list of staffs and/or voices  for a "voice" context,
 * clone copies of the SSV that we made for the first on the list
 * for the rest of the list. */

static void
clone_voice_ssvs(insert_p)

struct MAINLL *insert_p;

{
	struct SVRANGELIST *svr_p;
	struct RANGELIST *sr_p;		/* list of staff ranges */
	struct RANGELIST *vr_p;		/* list of voice ranges */
	struct SSV *clone_src_p;	/* the SSV to clone */
	int s;				/* staff number */
	int v;				/* voice number */
	int numvoices;			/* how many voices on current staff */
	int have_non_v3 = NO;		/* If any voice other than voice 3 */


	clone_src_p = Currstruct_p->u.ssv_p;
	for (svr_p = Svrangelist_p; svr_p != 0; svr_p = svr_p->next) {
	    for (sr_p = svr_p->stafflist_p; sr_p != 0; sr_p = sr_p->next) { 
		for (s = sr_p->begin; s <= sr_p->end && s <= Score.staffs; s++) {
		    numvoices = vscheme_voices(svpath(s, VSCHEME)->vscheme);
		    for (vr_p = svr_p->vnolist_p; vr_p != 0; vr_p = vr_p->next) {
			for (v = vr_p->begin; v <= vr_p->end && v <= numvoices; v++) {
				if (v != 3) {
					have_non_v3 = YES;
				}
				if (clone_src_p->staffno == s
						&& clone_src_p->voiceno == v) {
					/* skip past the clone source */
					continue;
				}
				clone1ssv(clone_src_p, insert_p, s, v);
			}
		    }
		}
	    }
	}

	if (clone_src_p->used[ALIGNRESTS] == YES && have_non_v3 == NO) {
		l_warning(Curr_filename, yylineno,
				"alignrests not used for voice 3; ignoring");
	}
}

/* Clone the given SSV and insert it in the specified place in the main list.
 * Change the copied versions staffno/voiceno to the specified values,
 * and do any additional error checks and processsing as needed.
 */

static void
clone1ssv(clone_src_p, insert_p, staffno, voiceno)

struct SSV *clone_src_p;
struct MAINLL *insert_p;
int staffno;
int voiceno;

{
	struct MAINLL *newmllssv_p;	/* the cloned SSV */

	newmllssv_p = newMAINLLstruct(S_SSV, Currstruct_p->inputlineno);
	memcpy(newmllssv_p->u.ssv_p, clone_src_p, sizeof(struct SSV));
	newmllssv_p->u.ssv_p->staffno = staffno;
	newmllssv_p->u.ssv_p->voiceno = voiceno;
	/* Fix beamstyle/timeunit. It could be different for each clone */
	remember_tsig_params(newmllssv_p);
	insertMAINLL(newmllssv_p, insert_p);

	/* Make sure any tab staffs have non-tabs above them.
	 * One disadvantage of waiting till making the clone to check this is
	 * that if there is a user error, the line number will be the end
	 * of the context rather than the one with "stafflines" on it.
	 * But the error message will tell them which staff is the problem,
	 * and this should be pretty rare--they have to be setting stafflines
	 * in a staff context for multiple staffs where the combination of
	 * staffs is illegal--so it doesn't seem worth the extra work it would
	 * be to be more precise. */
	if (newmllssv_p->u.ssv_p->context == C_STAFF &&
				newmllssv_p->u.ssv_p->used[STAFFLINES] == YES) {
		chk_tab(newmllssv_p);
	}
	asgnssv(newmllssv_p->u.ssv_p);
}


/* Check that the staffs/voices specified for an SSV are valid.
 * We only give a warning if all values specified are out of range.
 * If at least one is valid, we assume user did something like
 * 	voice 1-40 3
 * to set something for voice 3 on all staffs without having to worry
 * about which staffs actually have a voice 3.
 */

static void
chk_ssv_ranges(context)

UINT32B context;	/* expected to only be either C_STAFF or C_VOICE */

{
	struct SVRANGELIST *svr_p;	/* list of staff/voice ranges */
	struct RANGELIST *sr_p;		/* list of staff ranges */
	struct RANGELIST *vr_p;		/* list of voice ranges */
	int s;				/* staff number */
	int v;				/* voice number */
	int numvoices;			/* how many voices on current staff */


	for (svr_p = Svrangelist_p; svr_p != 0; svr_p = svr_p->next) {
	    for (sr_p = svr_p->stafflist_p; sr_p != 0; sr_p = sr_p->next) { 
		for (s = sr_p->begin; s <= sr_p->end; s++) {
		    if (context == C_STAFF) {
			if (s >= MINSTAFFS && s <= Score.staffs) {
				/* Found a valid one. One is all we need. */
				return;
			}
			continue;
		    }

		    /* a voice context */
		    numvoices = vscheme_voices(svpath(s, VSCHEME)->vscheme);
		    for (vr_p = svr_p->vnolist_p; vr_p != 0; vr_p = vr_p->next) { 
			for (v = vr_p->begin; v <= vr_p->end; v++) {
			    if (s >= MINSTAFFS && s <= Score.staffs &&
					v >= MINVOICES && v <= numvoices) {
				/* Found a valid one. One is all we need. */
				return;
			    }
			}
		    }
		}
	    }
	}
	/* Fell through all the loops without finding a single valid case */
	if (context == C_STAFF) {
		l_warning(Curr_filename, yylineno,
			"staff out of range; must be between %d and %d",
			MINSTAFFS, Score.staffs);
	}
	else {
		l_warning(Curr_filename, yylineno,
			"staff/voice combination out of range; staff must be between %d and %d, voice between %d and %d and within vscheme limit for the staff",
			MINSTAFFS, MAXSTAFFS, MINVOICES, MAXVOICES);
	}
}


/* if defining more than one voice at the moment, not valid to set
 * a location variable. Print message and reset flag so we don't
 * give any more of the same message this measure */

static void
var_valid()

{
	if (Defining_multiple == YES) {
		yyerror("location tag is only allowed when defining a single voice");
		Defining_multiple = NO;
	}
	else if (Chord_at_a_time == YES) {
		yyerror("location tag not allowed with chord-at-a-time input");
	}
}


/* Save information about a Mup variable that the user wants to have
 * exported to their PostScript */

static struct VAR_EXPORT *
add_export(varname, index)

char *varname;	/* Name of variable to be exported */
int index;	/* AX, AW, etc or NUMCTYPE if all are to be exported,
		 * or EXPORT_ALL_STAFFS for all indexes of all staffs */

{
	struct VAR_EXPORT *var_p;

	MALLOC(VAR_EXPORT, var_p, 1);
	var_p->name = strdup(varname);
	/* For now we set the alias to ourself. If the user specifies an
	 * alias, then that will be set later. But by defaulting to pointing
	 * to ourself, code can assume there is always an alias. */
	var_p->alias = var_p;
	var_p->index = index;
	var_p->next = 0;
	return(var_p);
}


/* do a print command like left, right, or center. Allocate a PRINTDATA
 * struct and fill in all the information about what to print and where
 * to print it. */

static void
proc_printcmd(justifytype, inpc_p, str, font, size, got_nl, isPostScript, ps_usage, isfilename, export_p, extra)

int justifytype;	/* J_LEFT, etc */
struct INPCOORD *inpc_p;	/* where to print */
char *str;		/* the string to print */
int font;
int size;
int got_nl;	/* YES if should go to next line before printing */
int isPostScript;	/* YES if is raw PostScript rather than normal print */
int ps_usage;		/* PU_* value; tells how is PostScript is used */
int isfilename;		/* Yes if the string is the name of a file containing PostScript */
struct VAR_EXPORT *export_p; /* Mup vars to export to user PostScript */
double extra;	/* how much extra vertical padding to add */

{
	struct PRINTDATA *curr_print_p;
	struct EXPR_NODE *node_p;
	int htype;


	(void) contextcheck(C_MUSIC | C_BLOCKHEAD, "print");

	/* save all the info in a PRINTDATA struct */
	MALLOC(PRINTDATA, curr_print_p, 1);

	if (inpc_p == (struct INPCOORD *) 0) {

		/* print at default location */
		curr_print_p->location.vert_p = symval("_cur", (float **) 0);
		/* Create an expression that is _cur - extra */
		node_p = newnode(OP_SUB);
		curr_print_p->location.vexpr_p = node_p;

		node_p->left.lchild_p = newnode(OP_TAG_REF);
		MALLOC(TAG_REF, node_p->left.lchild_p->left.ltag_p, 1);
		node_p->left.lchild_p->left.ltag_p->c = curr_print_p->location.vert_p;
		node_p->left.lchild_p->left.ltag_p->c_index = AY;
		save_tag_ref(node_p->left.lchild_p->left.ltag_p->c,
					&(node_p->left.lchild_p->left.ltag_p->c));

		node_p->right.rchild_p = newnode(OP_FLOAT_LITERAL);
		node_p->right.rchild_p->left.value = extra;

		switch (justifytype) {
		case J_LEFT:
		case J_RAGPARA:
		case J_JUSTPARA:
			/* default x is relative to _win.w */
			curr_print_p->location.hor_p = symval("_win", (float **) 0);
			htype = AW;
			break;
		case J_RIGHT:
			/* default x is relative to _win.e */
			curr_print_p->location.hor_p = symval("_win", (float **) 0);
			htype = AE;
			break;
		case J_CENTER:
			/* default x is relative to _win.x */
			curr_print_p->location.hor_p = symval("_win", (float **) 0);
			htype = AX;
			break;
		case J_NONE:
		default:
			/* default is at current location */
			curr_print_p->location.hor_p = symval("_cur", (float **) 0);
			htype = AX;
			break;
		}

		curr_print_p->location.hexpr_p = newnode(OP_TAG_REF);
		MALLOC(TAG_REF, curr_print_p->location.hexpr_p->left.ltag_p, 1);
		curr_print_p->location.hexpr_p->left.ltag_p->c = curr_print_p->location.hor_p;
		curr_print_p->location.hexpr_p->left.ltag_p->c_index = htype;
		save_tag_ref(curr_print_p->location.hexpr_p->left.ltag_p->c,
			&(curr_print_p->location.hexpr_p->left.ltag_p->c));

		/* If this is the first print command in a block,
		 * we implicitly go down by the font ascent. */
		if (isPostScript == NO && Currblock_p != 0
					&& Next_print_link_p_p
					== &(Currblock_p->printdata_p) ) {
			/* Alter expression, to subtract the fontascent */
			node_p = newnode(OP_SUB);
			node_p->left.lchild_p = curr_print_p->location.vexpr_p;
			curr_print_p->location.vexpr_p = node_p;
			node_p->right.rchild_p = newnode(OP_FLOAT_LITERAL);
			node_p->right.rchild_p->left.value = fontascent(font, size) / STEPSIZE;

			got_nl = NO;
		}

	}
	else {
		curr_print_p->location = *(inpc_p);
		rep_inpcoord(inpc_p, &(curr_print_p->location) );
		FREE(inpc_p);
	}

	curr_print_p->isPostScript = isPostScript;
	curr_print_p->ps_usage = ps_usage;
	curr_print_p->export_p = export_p;
	if (isPostScript == YES) {
		curr_print_p->width = 0.0;
	}
	else {
		curr_print_p->width = strwidth(str);
	}
	curr_print_p->isfilename = isfilename;
	curr_print_p->justifytype = (short) justifytype;
	curr_print_p->string = str;
	curr_print_p->inputfile = Curr_filename;
	curr_print_p->inputlineno = (short) yylineno;

	/* For PostScript hooks that apply once to the entire file,
	 * store directly into hook table. */
	switch (ps_usage) {
	case PU_AFTERPROLOG:
	case PU_BEFORETRAILER:
		if (PostScript_hooks[ps_usage] != 0) {
		l_warning(Curr_filename, yylineno, "PostScript hook already set on line %d of %s; using last",
			PostScript_hooks[ps_usage]->inputlineno,
			PostScript_hooks[ps_usage]->inputfile);
		}
		PostScript_hooks[ps_usage]  = curr_print_p;
		return;
	default:
		break;
	}

	/* special case of user asking that this be printed
	 * on the next line. In this case we go down by the
	 * current point size */
	if (got_nl) {
		node_p = newnode(OP_SUB);
		node_p->left.lchild_p = curr_print_p->location.vexpr_p;
		curr_print_p->location.vexpr_p = node_p;
		node_p->right.rchild_p = newnode(OP_FLOAT_LITERAL);
		node_p->right.rchild_p->left.value = fontheight(font, size) / STEPSIZE;
	}

	/* Now link onto proper list -- could be in BLOCKHEAD
	 * context or doing a PRHEAD. If we are in BLOCKHEAD context, we
	 * definitely have something in the main list to attach to.
	 * If in C_MUSIC, we may or may not depending on whether
	 * this is the first in a series of print commands. */
	if ( ((Context & C_BLOCKHEAD) != 0)
		|| ((Currstruct_p != (struct MAINLL *) 0) &&
		(Currstruct_p->str == S_PRHEAD) ) ) {
			if (Next_print_link_p_p == (struct PRINTDATA **) 0) {
				pfatal("Next_print_link_p_p is null");
			}
			/* this means the value of Next_print_link_p_p
			 * is valid and will tell us where to link */
			*Next_print_link_p_p = curr_print_p;
	}
	else {
		/* We must be in music context, but the last
		 * thing we saw wasn't a print command, so
		 * we have to allocate a PRHEAD and put it
		 * in the main list and attach to that. */
		/* allocate a new struct, put on main list and attach
		 * the print command to it */
		Currstruct_p = newMAINLLstruct(S_PRHEAD, yylineno);
		insertMAINLL(Currstruct_p, Mainlltc_p);
		Currstruct_p->u.prhead_p->printdata_p = curr_print_p;
		Currstruct_p = (struct MAINLL *) 0;
	}

	/* in any case, if we get another print command right away, it
	 * should be linked onto this one, so remember where we are */
	Next_print_link_p_p = &(curr_print_p->next);
	curr_print_p->next = (struct PRINTDATA *) 0;
}


/* if the given string has a higher ascent than the given font/size, return
 * the amount is it higher, in stepsizes */

static double
extra_needed(font, size, string_p)

int font;	/* default font for this string */
int size;	/* default size for this string */
char **string_p;/* string, before being put in internal format,
		 * which might contain \s(xx) to make it bigger than normal.
		 * If it is key-mapped, the passed-in pointer will be
		 * updated to point to the mapped string. */

{
	double usual_ascent;
	double this_string_ascent;


	/* get expected ascent for this font/size and actual for string */
	usual_ascent = fontascent(font, size);
	(void) fix_string(*string_p, font, size, Curr_filename, yylineno);
	*string_p = map_print_str(*string_p, Curr_filename, yylineno);
	this_string_ascent = strascent(*string_p);

	/* if this string is too tall, return by how much it is too tall */
	if (this_string_ascent > usual_ascent) {
		return (this_string_ascent - usual_ascent) / STEPSIZE;
	}
	else {
		return(0.0);
	}
}


/* Sometimes lexer will recognize something as a keyword, but it's
 * really a bunch of note letters, due to context. This function turns
 * the mistaken keyword into notes. It handles a through g and r and s, and
 * us, but not n, which would also be troublesome, but which we don't (yet)
 * need to handle.
 */

static void
keyword_notes(str)

char *str;

{
	for (  ; *str != '\0'; str++) {
		if (*str == 'u' && *(str+1) == 's') {
			str++;
			continue;
		}

		/* On the last letter, if it's a true pitch, have to
		 * push it back into input, because it could be followed
		 * by an accidental or octave mark or tick or something. */
		if (*(str+1) == '\0' && *str >= 'a' && *str <= 'g') {
			pushback(*str);
		}
		else {
			add_note(Curr_grpsyl_p, (int) *str, No_accs,
					USE_DFLT_OCTAVE, 0, NO, (char *) 0);
		}
	}
}


/* Free up the current additive time list, if any */

static void
free_extra_time()
{
	struct TIMELIST *tl_p;

	/* If this was the most recently allocated list, we can free it.
	 * We can't free otherwise, since Extra_time_p might be
	 * pointing to one from an SSV, which must be kept around. */
	if (Extra_time_p == Last_alloced_timelist_p) {
		while (Extra_time_p != 0) {
			tl_p = Extra_time_p->next;
			FREE(Extra_time_p);
			Extra_time_p = tl_p;
		}
	}
	Extra_time_p = 0;
	Last_alloced_timelist_p = 0;
	Curr_timelist_p = 0;
}


/* Add an item to the internal representation of the time signature
 * currently being collected */

static void
tsig_item(item)

int item;	/* the byte to add to timerep */

{
	if (Tsig_offset >= MAXTSLEN  - 1) {
		l_ufatal(Curr_filename, yylineno,
				"time signature is too complicated");
	}
	Timerep[Tsig_offset++] = (char) item;
}


/* Allocate a new expression node, fill in its opearator field, and return it. */

static struct EXPR_NODE *
newnode(op)

int op;		/* OP_* value for the new node */

{
	struct EXPR_NODE * node_p;

	CALLOC(EXPR_NODE, node_p, 1);
	node_p->op = op;
	return(node_p);
}


/* Zero out the list of accidentals for the current note */

static void
clear_curr_accs()

{
	CLEAR_ACCS(Curr_accs);
	Acc_offset = 0;
}


/* Add given accidental to the list of accidental for the current note.
 * (if it is valid and there is room) */

static void
add_acc(acc_name)

char *acc_name;

{
	int code;
	int font;

	if (Acc_offset >= 2 * MAX_ACCS) {
		l_yyerror(Curr_filename, yylineno,
				"Too many accidentals (%d max)", MAX_ACCS);
		return;
	}
	if (is_bad_char((code = find_char(acc_name, &font, NO, NO))) == YES) {
		l_yyerror(Curr_filename, yylineno,
			"No symbol named %s to use as accidental", acc_name);
		return;
	}

	/* Note that probably the font should be either one of the music
	 * fonts or user defined font, but we go ahead and allow anything. */
	Curr_accs[Acc_offset++] = font;
	Curr_accs[Acc_offset++] = code;

	/* If we have a non standard accidental, or if the accidentals
	 * net out to something outside the range of double flat to
	 * double sharp, mark Tuning_used, because we can't use simple
	 * ordinary MIDI commands. */
	if (accs_offset(Curr_accs) == BAD_ACCS_OFFSET) {
		Tuning_used = YES;
	}
}


/* Add another entry to the list of accidentals in accidentals context.
 * It is passed the name of the accidental. It allocates an entry,
 * if necessary, sets Curr_acc_index to the index of the entry, fills in
 * the code and font, and sets offsets to a negative value to indicate
 * "not yet filled in."
 */

static void
new_acc_entry(acc_name)

char *acc_name;		/* like "sharp" or "dlbflat" or a user name */

{
	int i;
	int is_small;


	if (Acc_contexts_list_p == 0) {
		/* must have been an earlier error */
		return;
	}

	/* Check for already defined */
	for (i = 0; i < Acc_contexts_list_p->size; i++) {
		if (strcmp(acc_name, get_charname(
				Acc_contexts_list_p->info[i].code,
				Acc_contexts_list_p->info[i].font))
				== 0) {
			/* Use the existing entry */
			Curr_acc_index = i;
			return;
		}
	}

	/* If space is full, alloc another chunk */
	if (Acc_contexts_list_p->size % ITEMS == 0
				&& Acc_contexts_list_p->size > 0) {
		REALLOC(ACCINFO, Acc_contexts_list_p->info,
				Acc_contexts_list_p->size + ITEMS);
	}

	/* Use the next available slot */
	Curr_acc_index = Acc_contexts_list_p->size;

	/* One more slot being used */
	(Acc_contexts_list_p->size)++;

	/* Fill in all the offset slots with an illegal
	 * (i.e., negative) value, so we can tell later which the user
	 * has set. Normally I wouldn't hard-code a 7, but if the
	 * number of "white keys" a-g changes from 7, we have
	 * much bigger problems ;-). 
	 */
	for (i = 0; i < 7; i++) {
		CURR_ACC_ENTRY.offset[i] = -1.0;
	}

	/* Translate symbol name to code and font */
	CURR_ACC_ENTRY.code = find_char(acc_name,
					&(CURR_ACC_ENTRY.font), &is_small, YES);
}


/* Fill in an element of the offset[] array for a accidental,
 * in accidentals context. 
 */

static void
add_acc_offset(letter, offset)

int letter;	/* which index into offset[], 'a' is 0, 'b' is 1, etc */
double offset;	/* what value to fill in */

{
	int index;

	if (Context != C_ACCIDENTALS) {
		return;
	}
	if (Acc_contexts_list_p == 0) {
		/* Must have been earlier error */
		return;
	}

	index = letter - 'a';

	/* Check if already has been set */
	if (CURR_ACC_ENTRY.offset[index] >= 0 ) {
		l_warning(Curr_filename, yylineno,
			"offset for accidental '%s' specified more than once, using last", get_charname(CURR_ACC_ENTRY.code, CURR_ACC_ENTRY.font));
	}

	/* Warn about outrageous values. We consider an accidental
	 * that changes by more than a major third outrageous. */
	if (offset < 0.8 || offset > 1.25) {
		l_warning(Curr_filename, yylineno,
			"dubious accidental offset (factor of %f alters pitch by more than a major third)", offset);
	}

	/* Fill in the value */
	CURR_ACC_ENTRY.offset[index] = offset;
}


static int
comp_subbar(item1_p, item2_p)

#ifdef __STDC__
const void *item1_p;            /* the subbars to compare */
const void *item2_p;
#else
char *item1_p;          	/* the subbars to compare */
char *item2_p;
#endif

{
	struct SUBBAR_INSTANCE *sb1_p, *sb2_p;

	sb1_p = (struct SUBBAR_INSTANCE *) item1_p;
	sb2_p = (struct SUBBAR_INSTANCE *) item2_p;
	if (sb1_p->count < sb2_p->count) {
		return(-1);
	}
	else if (sb1_p->count > sb2_p->count) {
		return(1);
	}
	else {
		return(0);
	}
}


/* Given a MULTIWHOLE value (2, 4, or 8) return the corresponding BT_* value */
static int
multiwhole2basictime(value)

int value;

{
	if (value == 2) {
		return(BT_DBL);
	}
	else if (value == 4) {
		return(BT_QUAD);
	}
	else if (value == 8) {
		return(BT_OCT);
	}
	else {
		pfatal("invalid value %d passed to multiwhole2basictime", value);
		/*NOTREACHED*/
		return(0);
	}
}


/* Given a chordtranslation parameter "do/re/mi" string, split it into
 * the 7 syllables, and place them in the doremi_syls field of current SSV. */

static void
parse_doremi_string(str)

char *str;	/* Typically "do re mi fa sol la si" */

{
	int nsyl;	/* count of syllables */
	int syl_leng;
	char saved;


	if (Currstruct_p == 0 || Currstruct_p->u.ssv_p == 0) {
		/* Must have been a user error earlier */
		return;
	}

	CALLOCA(char *, Currstruct_p->u.ssv_p->doremi_syls, 7);
	/* This code is similar to that for noteheads parameter in assign.c */
	for (nsyl = 0; nsyl < 7; nsyl++) {
		/* skip any leading spaces */
		while (isspace(*str)) {
			str++;
		}
		if (*str == '\0') {
			break;
		}
		syl_leng = strcspn(str, " \t\r\n");
		saved = str[syl_leng];
		str[syl_leng] = '\0';
		Currstruct_p->u.ssv_p->doremi_syls[nsyl] =
				copy_string(str, FONT_TR, DFLT_SIZE);
		fix_string(Currstruct_p->u.ssv_p->doremi_syls[nsyl],
				FONT_TR, DFLT_SIZE,
				Curr_filename, yylineno);
		str[syl_leng] = saved;
		str += syl_leng;
	}
	while (isspace(*str)) {
		str++;
	}
	if (nsyl != 7 || *str != '\0') {
		l_yyerror(Curr_filename, yylineno,
			 "Wrong number of do/re/mi syllables; expecting 7");
		/* If there were less than 7, later code could attempt to
		 * access an uninitalized value, so fix that. */
		for (  ; nsyl < 7; nsyl++) {
			Currstruct_p->u.ssv_p->doremi_syls[nsyl] =
				copy_string(" ", FONT_TR, DFLT_SIZE);
		}
	}
}
