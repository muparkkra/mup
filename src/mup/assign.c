
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

/* Assign values to internal variables in an SSV struct. The functions in
 * this file are called from the parse phase. */


#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"


/* there are several cases where we ultimately want arrays, but don't
 * know in advance how many elements will be in the array. This could be
 * done by building a linked list first, then malloc-ing the right size and
 * copying everything. However, what we'll do is allocate CHUNK elements
 * to start, and realloc if necessary. When we're done, we realloc down
 * to the actual size */
#define CHUNK  (8)

/* minimum allowable height or width of page (in inches)
 * after subtracting off the margins */
#define MIN_USABLE_SPACE	0.5


/* Macros to adjust numbers between inches to centimeters . */

/* Given a number in inches, use that number if in inches mode.
 * If in centimeter mode, then use the equivalent distance in centimeters,
 * rounded to the nearest quarter of a centimeter. */
#define ADJUST4UNITS(n)  (Score.units == INCHES ? n : NEARESTQUARTER(n * CMPERINCH))

/* Given an input number, leave as is if in inches mode. If in centimeter
 * mode, treat the number as being in centimeters, and convert to the inch
 * equivalent distance. */
#define ADJUST2INCHES(n)  { if (Score.units == CM) n /= CMPERINCH; }

static struct STAFFSET *Curr_staffset_p;/* staffset currently
					 * being filled in */
static int Ss_count;			/* num of elements in Curr_staffset_p
					 * currently actually being used */
static int Ss_length;			/* num of elements allocated
					 * to Curr_staffset_p */

static struct TOP_BOT *Curr_barstlist_p;/* bar style list being filled in */
static int Barst_count;			/* number of elements in
					 * Curr_barstlist_p that are currently
					 * filled in with valid values */
static int Barst_length;		/* number of elements allocated to
					 * Curr_barstlist_p */

struct BEAMLIST {
	RATIONAL *list_p;		/* beam list being filled in */
	int count;			/* elements filled in list_p */
	int length;			/* elements allocated to list_p */
};
/* These store info from the beamstyle parameter. */
static struct BEAMLIST Curr_beamstyle;
static struct BEAMLIST Curr_subbeamstyle;
static int Subbeam_index;		/* Index into Curr_subbeamstyle where
					 * the most recent '(' was, or -1 if
					 * no pending unmatched parenthesis. */
static char *parmformat = "%s parameter";	/* for error messages */

/* functions to do all the various checks. They check the context, range, etc.
 * mark the variable as used if everything passes the checks. The first
 * is for int variables (actually short) in SSV, the second for floats */
static int do_assign P((int var, int value, int min, int max,
		UINT32B cont, int empty_value, char *name,
		struct MAINLL *mainll_item_p, short *ptr2dest));

static int do_fassign P((int var, double value, double min,
		double max, UINT32B cont, char *name,
		struct MAINLL *mainll_item_p, float *ptr2dest));
static void chg_too_late P((char *var_name));
static void set_vcombine_fields P((int qualifier, int bymeas, struct SSV *ssv_p));

/* comparison functions to be passed to qsort */
static int comp_staffset P((const void *item1_p,
		const void *item2_p));
static int comp_barst P((const void *item1_p, const void *item2_p));
static int is_tablature_staff P((struct SSV *ssv_p));
static void init_beamlist P((struct BEAMLIST *beamlist_p));
static void add2outerbeam P((RATIONAL value));
static void wrong_context P((char *param_name));
static char * parm_name P((int param));


/* assign a value to an SSV variable having a domain of short,
 * after doing any appropriate checks */

void
assign_int(var, value, mainll_item_p)

int var;			/* SSV index of which variable to set */
int value;			/* what to set it to */
struct MAINLL *mainll_item_p;	/* where to store SSV info in main list */

{
	char * name;	/* the parameter's name that user knows about */

	name = parm_name(var);
	/* all of these things except font can only go
	 * into score/staff sorts of things,
	 * not head/foot. If we are in a head/foot, the MAINLL will be null */
	if (Context == C_MUSIC || Context == C_GRIDS || Context == C_HEADSHAPES
				|| Context == C_KEYMAP
				|| Context == C_ACCIDENTALS
				|| (mainll_item_p == 0 && var != SIZE)) {
		wrong_context(name);
		return;
	}


	if (mainll_item_p == (struct MAINLL *) 0) {
		if (var != SIZE) {
			/* must have been some other error */
			return;
		}
	}
	else {
		debug(4, "assign_int file=%s line=%d var=%d (%s) value=%d",
			mainll_item_p->inputfile, mainll_item_p->inputlineno,
			var, name, value);
	}

	/* handle each variable appropriately */
	switch (var) {

	case SIZE:
		Curr_size = value;
		if ( Context & C_BLOCKHEAD ) {
			(void) rangecheck(value, MINSIZE, MAXSIZE, name);

			/* special case -- size can be set in block,
			 * and there we don't put in SSV struct,
			 * just save its value in Curr_size */
			return;
		}
		else if (mainll_item_p == 0) {
			/* Some other error must have occurred that
			 * we should have already reported. */
			return;
		}
		else {
			(void) do_assign(var, value, MINSIZE, MAXSIZE,
				C_SCORE | C_STAFF, NO, name,
				mainll_item_p, &(mainll_item_p->u.ssv_p->size));
			/* set in Score, in case we get an "all" */
			if (Context == C_SCORE) {
				Score.size = (short) value;
			}
		}
		break;

	case LYRICSSIZE:
		if (do_assign(var, value, MINSIZE, MAXSIZE, C_SCORE | C_STAFF,
				NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->lyricssize) )
				== YES) {
			/* save values for any lyrics that come later */
			if (Context == C_SCORE) {
				/* set in case we get some "all" lyrics */
				Score.lyricssize = (short) value;
			}
			setlyrsize(mainll_item_p->u.ssv_p->staffno, value);
		}
		break;

	case MEASNUMSIZE:
		(void) do_assign(var, value, MINSIZE, MAXSIZE, C_SCORE,
				NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->measnumsize) );
		break;

	case WITHSIZE:
		(void) do_assign(var, value, MINSIZE, MAXSIZE,
				C_SCORE | C_STAFF | C_VOICE,
				NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->withsize) );
		break;

	case NOTELEFTSIZE:
		(void) do_assign(var, value, MINSIZE, MAXSIZE,
				C_SCORE | C_STAFF | C_VOICE,
				NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->noteleftsize) );
		break;

	case SYLPOSITION:
		(void) do_assign(var, value, MINSYLPOSITION, MAXSYLPOSITION,
				C_SCORE | C_STAFF,
				NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->sylposition) );
		break;

	case ALIGNLABELS:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, J_LEFT, J_CENTER,
				C_SCORE,
				NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->alignlabels) );
		break;

	case DEFOCT:
		(void) do_assign(var, value, MINOCTAVE, MAXOCTAVE,
				C_SSV, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->defoct) );
		if (Context != C_SCORE && has_tab_staff() == YES) {
			l_warning(Curr_filename, yylineno,
				"defoct not allowed on tablature staff; ignoring");
		}
		/* We go ahead and assign even if it had the tab warning above,
		 * because there could be other cloned SSVs
		 * that aren't tab, and it doesn't hurt to set even on tab--
		 * it will just be ignored.
		 */
		asgnssv(mainll_item_p->u.ssv_p);
		break;

	case NUMSTAFF:
		if (do_assign(var, value, MINSTAFFS, MAXSTAFFS, C_SCORE, NO,
					name, mainll_item_p,
					&(mainll_item_p->u.ssv_p->staffs) )
					== YES) {

			/* can only change number of staffs if not in the
			 * middle of doing music data. We exclaimed about
			 * the user error in end_prev_context(), so if this
			 * occurs, we just skip over the code in the "if"
			 */
			if (List_of_staffs_p == (struct MAINLL *) 0) {
				/* NUMSTAFFS is a special case,
				 * in that several other
				 * items have to do error checking
				 * based on the number of staffs specified,
				 * so we have to set it immediately rather than
				 * waiting for the whole SSV struct
				 * to be built. */
				Score.staffs = (short) value;

				/* if the number of scores changes,
				 * there can't be any
				 * pending "til" clauses on STUFF */
				chk4dangling_til_clauses(
						"change in number of staffs");

				/* any pedal information is no long valid */
				reset_ped_state();
			}
		}
		break;

	case VISIBLE:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, 0, 1, C_SCORE|C_STAFF|C_VOICE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->visible) );
		break;

	case MEASNUM:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, MINMEASNUM, MAXMEASNUM, C_SCORE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->measnum) );
		break;

	case INDENTRESTART:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, 0, 1, C_SCORE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->indentrestart) );
		break;

	case FLIPMARGINS:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, 0, 1, C_SCORE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->flipmargins) );
		break;

	case SLASHESBETWEEN:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, 0, 1, C_SCORE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->slashesbetween) );
		break;

	case CANCELKEY:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, 0, 1, C_SCORE | C_STAFF, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->cancelkey) );
		break;

	case USEACCS:
		(void) do_assign(var, value, UA_N, UA_Y_NONNATREMUSER, C_SCORE | C_STAFF, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->useaccs) );
		break;

	case CARRYACCS:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, 0, 1, C_SCORE | C_STAFF, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->carryaccs) );
		break;

	case EXTENDLYRICS:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, 0, 1, C_SCORE | C_STAFF | C_VOICE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->extendlyrics) );
		break;


	case MAXSCORES:
		(void) do_assign(var, value, MINMAXSCORES, MAXMAXSCORES,
				C_SCORE, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->maxscores) );
		break;
	case MAXMEASURES:
		(void) do_assign(var, value, MINMAXMEASURES, MAXMAXMEASURES,
				C_SCORE, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->maxmeasures) );
		break;

	case MIDLINESTEMFLOAT:
		(void) do_assign(var, value, 0, 1,
				C_SCORE|C_STAFF|C_VOICE, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->midlinestemfloat) );
		break;

	case DIVISION:
		chg_too_late(name);
		(void) do_assign(var, value, MINDIVISION, MAXDIVISION,
				C_SCORE, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->division) );
		/* division values that aren't divisible by 2 and 3 are
		 * unlikely to be right and are likely to lead to rational
		 * overflow in MIDI code, so give warning */
		if ((value % 6) != 0) {
			l_warning(Curr_filename, yylineno,
						"dubious division value (usually it should be divisible by both 2 and 3)");
		}

		break;

	case RELEASE:
		(void) do_assign(var, value, MINRELEASE, MAXRELEASE,
				C_SCORE | C_STAFF | C_VOICE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->release) );
		break;

	case PANELSPERPAGE:
		chg_too_late(name);
		(void) do_assign(var, value, MINPANELSPERPAGE, MAXPANELSPERPAGE,
				C_SCORE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->panelsperpage) );
		break;

	case RESTCOMBINE:
		(void) do_assign(var, value,
				MINRESTCOMBINE, MAXRESTCOMBINE,
				C_SCORE, NORESTCOMBINE,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->restcombine) );
		break;

	case ALIGNRESTS:
		(void) do_assign(var, value,
				0, 1, C_SCORE | C_STAFF | C_VOICE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->alignrests) );
		break;

	case ALIGNPED:
		(void) do_assign(var, value,
				0, 1, C_SCORE | C_STAFF, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->alignped) );
		break;

	case BEGPROSHORT:
		(void) do_assign(var, value,
				MINBEGPROSHORT, MAXBEGPROSHORT,
				C_SCORE|C_STAFF|C_VOICE, DEFBEGPROSHORT,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->begproshort) );
		break;

	case ENDPROSHORT:
		(void) do_assign(var, value,
				MINENDPROSHORT, MAXENDPROSHORT,
				C_SCORE|C_STAFF|C_VOICE, DEFENDPROSHORT,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->endproshort) );
		break;

	case GRIDFRET:
		(void) do_assign(var, value, MINGRIDFRET, MAXGRIDFRET,
				C_SCORE | C_STAFF, NOGRIDFRET,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->gridfret) );
		break;

	case MINGRIDHEIGHT:
		(void) do_assign(var, value, MINGRIDFRET, MAXGRIDFRET,
				C_SCORE | C_STAFF, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->mingridheight) );
		break;

	case GRIDSWHEREUSED:
		(void) do_assign(var, value, 0, 1,
				C_SCORE | C_STAFF, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->gridswhereused) );
		break;

	case GRIDSATEND:
		(void) do_assign(var, value, 0, 1, C_SCORE | C_STAFF, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->gridsatend) );
		break;

	case TABWHITEBOX:
		/* actually yacc already guarantees will be in range */
		(void) do_assign(var, value, 0, 1,
				C_SCORE | C_STAFF | C_VOICE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->tabwhitebox) );
		break;

	case ONTHELINE:
		/* This only makes sense on 1-line staffs, but we decided
		 * it is best to silently accept it elsewhere. For example,
		 * you might want to set it in score context to apply to all
		 * the 1-line staffs there are. */
		(void) do_assign(var, value, 0, 1,
				C_SCORE | C_STAFF | C_VOICE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->ontheline) );
		break;

	case WARN:
		(void) do_assign(var, value, 0, 1,
				C_SCORE, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->warn) );
		/* Make this take effect immediately. It would seem
		 * strange to the user to set warn=n only to have Mup
		 * still warn on the very next line. They shouldn't have
		 * to enter a new context to make it take effect.
		 */
		asgnssv(mainll_item_p->u.ssv_p);
		break;

	case BRACKETREPEATS:
		(void) do_assign(var, value, 0, 1,
				C_SCORE, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->bracketrepeats) );
		break;

	case NUMBERMRPT:
		(void) do_assign(var, value, 0, 1,
				C_SCORE | C_STAFF, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->numbermrpt) );
		break;

	case NUMBERMULTRPT:
		(void) do_assign(var, value, 0, 1,
				C_SCORE | C_STAFF, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->numbermultrpt) );
		break;

	case PRINTMULTNUM:
		(void) do_assign(var, value, 0, 1,
				C_SCORE | C_STAFF, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->printmultnum) );
		break;

	case RESTSYMMULT:
		(void) do_assign(var, value, 0, 1,
				C_SCORE | C_STAFF, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->restsymmult) );
		break;

	case CUE:
		(void) do_assign(var, value, 0, 1,
				C_SCORE | C_STAFF | C_VOICE, NO, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->cue) );
		break;

	default:
		pfatal("invalid int parameter (%d)\n", var);
		break;
	}
}


/* do all the error checks for an int variable. If it passes all checks,
 * set the used flag to YES and return YES. If something fails, return NO. */
/* Checks are: must be within range, must be in valid context, the MAINLL
 * struct passed must be non-NULL. Also give warning if field was already used.
 * Getting a null pointer is not fatal--it can happen if user tried to do
 * something in the wrong context. */

static int
do_assign(var, value, min, max, cont, empty_value, name, mainll_item_p, ptr2dest)

int var;			/* which variable to set */
int value;			/* what to set it to */
int min;			/* minimum valid value */
int max;			/* maximum valid value */
UINT32B cont;			/* valid context(s)  (bitmap) */
int empty_value;		/* if NO, value must be strictly within the
				 * given min/max. If != NO, it is an extra
				 * value, outside the min/max range, that
				 * is legal, and indicates
				 * user set the value to empty */
char *name;			/* of internal variable, for error messages */
struct MAINLL *mainll_item_p;	/* which structure to set it in */
short *ptr2dest;		/* the address of the variable to be set */

{
	char fullname[100];	/* name + " parameter" */
	if (mainll_item_p == (struct MAINLL *) 0) {
		wrong_context(name);
		return(NO);
	}
	(void) sprintf(fullname, parmformat, name);
	if (contextcheck(cont, fullname) == NO) {
		return(NO);
	}

	/* exclaim if already set in this SSV */
	used_check(mainll_item_p, var, name);

	/* do the checks */
	if (empty_value != NO && erangecheck(value, min, max, empty_value, name) == NO) {
		return(NO);
	}
	else if (empty_value == NO && rangecheck(value, min, max, name) == NO) {
		return(NO);
	}
	/* passed all the checks-- assign and mark it as used */
	mainll_item_p->u.ssv_p->used[var] = YES;
	*ptr2dest = (short) value;
	return(YES);
}


/* do assignment of float type SSV variables */

void
assign_float(var, value, mainll_item_p)

int var;			/* which variable to set */
double value;			/* what to set it to */
struct MAINLL *mainll_item_p;	/* where to store info */

{
	char *name;

	name = parm_name(var);

	/* all of these things can only go into score/staff sorts of things,
	 * not head/foot. If we are in a head/foot, the MAINLL will be null */
	if (mainll_item_p == 0 || Context == C_MUSIC) {
		wrong_context(name);
		return;
	}

	debug(4, "assign_float file=%s line=%d var=%d (%s) value=%f",
		mainll_item_p->inputfile, mainll_item_p->inputlineno,
		var, name, value);

	/* some changes are only allowed before music data is entered */
	if (Got_some_data == YES) {
		switch (var) {
		case TOPMARGIN:
		case BOTMARGIN:
		case LEFTMARGIN:
		case RIGHTMARGIN:
			chg_too_late( "margin");
			break;
		case SCALE_FACTOR:
			chg_too_late( "scale");
			break;
		case MUSICSCALE:
			chg_too_late( "musicscale");
			break;
		case PAGEHEIGHT:
		case PAGEWIDTH:
			chg_too_late( "page size");
			break;
		default:
			break;
		}
	}

	/* if pagesize minus the margins get too small (or even worse,
	 * negative), we better complain */
	switch (var) {
	case TOPMARGIN:
	case BOTMARGIN:
	case LEFTMARGIN:
	case RIGHTMARGIN:
	case PAGEHEIGHT:
	case PAGEWIDTH:
		chkmargin(Score.topmargin, Score.botmargin, Score.leftmargin,
					Score.rightmargin);
		break;
	default:
		break;
	}

	switch (var) {

	case TOPMARGIN:
		if (do_fassign(var, (double) value,
				(double) ADJUST4UNITS(MINVMARGIN),
				(double) ADJUST4UNITS(Score.pageheight - MIN_USABLE_SPACE),
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->topmargin) )
				== YES) {
			ADJUST2INCHES(mainll_item_p->u.ssv_p->topmargin)

			/* put in score so we can check for margins exceeding paper size */
			Score.topmargin = mainll_item_p->u.ssv_p->topmargin;
		}
		break;

	case BOTMARGIN:
		if (do_fassign(var, (double) value,
				(double) ADJUST4UNITS(MINVMARGIN),
				(double) ADJUST4UNITS(Score.pageheight - MIN_USABLE_SPACE),
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->botmargin) )
				== YES) {
			ADJUST2INCHES(mainll_item_p->u.ssv_p->botmargin)
			Score.botmargin = mainll_item_p->u.ssv_p->botmargin;
		}
		break;

	case LEFTMARGIN:
		if (do_fassign(var, (double) value,
				(double) ADJUST4UNITS(MINHMARGIN),
				(double) ADJUST4UNITS(Score.pagewidth - MIN_USABLE_SPACE),
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->leftmargin) )
				== YES) {
			ADJUST2INCHES(mainll_item_p->u.ssv_p->leftmargin)
			Score.leftmargin = mainll_item_p->u.ssv_p->leftmargin;
		}
		break;

	case RIGHTMARGIN:
		if (do_fassign(var, (double) value,
				(double) ADJUST4UNITS(MINHMARGIN),
				(double) ADJUST4UNITS(Score.pagewidth - MIN_USABLE_SPACE),
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->rightmargin) )
				== YES) {
			ADJUST2INCHES(mainll_item_p->u.ssv_p->rightmargin)
			Score.rightmargin = mainll_item_p->u.ssv_p->rightmargin;
		}
		break;

	case PACKFACT:
		(void) do_fassign(var, (double) value,
				(double) MINPACKFACT, (double) MAXPACKFACT,
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->packfact) );
		break;

	case PACKEXP:
		(void) do_fassign(var, (double) value,
				(double) MINPACKEXP, (double) MAXPACKEXP,
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->packexp) );
		break;

	case SCALE_FACTOR:
		(void) do_fassign(var, (double) value,
				(double) MINSCALE, (double) MAXSCALE,
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->scale_factor) );
		break;

	case MUSICSCALE:
		(void) do_fassign(var, (double) value,
				(double) MINMUSICSCALE, (double) MAXMUSICSCALE,
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->musicscale) );
		break;

	case STAFFSCALE:
		(void) do_fassign(var, (double) value,
				(double) MINSTFSCALE, (double) MAXSTFSCALE,
				C_SCORE | C_STAFF, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->staffscale) );
		break;

	case STAFFPAD:
		(void) do_fassign(var, (double) value,
				(double) MINSTPAD, (double) MAXSTPAD,
				C_SCORE | C_STAFF, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->staffpad) );
		break;

	case MINSCPAD:
		(void) do_fassign(var, (double) value,
				(double) MINMINSCPAD, (double) MAXPADVAL,
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->minscpad) );
		break;

	case MAXSCPAD:
		(void) do_fassign(var, (double) value,
				(double) MINMAXSCPAD, (double) MAXPADVAL,
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->maxscpad) );
		break;

	case MINSCSEP:
		(void) do_fassign(var, (double) value,
				(double) MINMINSCSEP, (double) MAXSEPVAL,
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->minscsep) );
		break;

	case MAXSCSEP:
		(void) do_fassign(var, (double) value,
				(double) MINMAXSCSEP, (double) MAXSEPVAL,
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->maxscsep) );
		break;

	case MINSTSEP:
		(void) do_fassign(var, (double) value,
				(double) MINMINSTSEP, (double) MAXSEPVAL,
				C_SCORE | C_STAFF, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->minstsep) );
		break;

	case GRIDSCALE:
		(void) do_fassign(var, (double) value,
				(double) MINGRIDSCALE, (double) MAXGRIDSCALE,
				C_SCORE | C_STAFF, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->gridscale) );
		break;

	case MINALIGNSCALE:
		(void) do_fassign(var, (double) value,
				(double) MINMINALIGNSCALE, (double) MAXMINALIGNSCALE,
				C_SCORE | C_STAFF, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->minalignscale) );
		break;

	case MAXPROSHORT:
		(void) do_fassign(var, (double) value,
				(double) MINMAXPROSHORT,
				(double) MAXMAXPROSHORT,
				C_SCORE|C_STAFF|C_VOICE,
				name,
				mainll_item_p,
				&(mainll_item_p->u.ssv_p->maxproshort) );
		break;

	case PAGEHEIGHT:
		if (do_fassign(var, (double) value,
				(double) ADJUST4UNITS(MINPAGEHEIGHT),
				(double) ADJUST4UNITS(MAXPAGEHEIGHT),
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->pageheight) )
				== YES) {
			ADJUST2INCHES(mainll_item_p->u.ssv_p->pageheight)
			Score.pageheight = mainll_item_p->u.ssv_p->pageheight;
		}
		break;

	case PAGEWIDTH:
		if (do_fassign(var, (double) value,
				(double) ADJUST4UNITS(MINPAGEWIDTH),
				(double) ADJUST4UNITS(MAXPAGEWIDTH),
				C_SCORE, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->pagewidth) )
				== YES) {
			ADJUST2INCHES(mainll_item_p->u.ssv_p->pagewidth)
			Score.pagewidth = mainll_item_p->u.ssv_p->pagewidth;
		}
		break;

	case CHORDDIST:
		(void) do_fassign(var, (double) value,
				(double) MINCHORDDIST,
				(double) MAXCHORDDIST,
				C_SCORE | C_STAFF, name,
				mainll_item_p,
				&(mainll_item_p->u.ssv_p->chorddist) );
		break;

	case DIST:
		(void) do_fassign(var, (double) value,
				(double) MINDIST, (double) MAXDIST,
				C_SCORE | C_STAFF, name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->dist) );
		break;

	case DYNDIST:
		(void) do_fassign(var, (double) value,
				(double) MINDYNDIST, (double) MAXDYNDIST,
				C_SCORE | C_STAFF, name,
				mainll_item_p,
				&(mainll_item_p->u.ssv_p->dyndist) );
		break;

	case LYRICSDIST:
		(void) do_fassign(var, (double) value,
				(double) MINLYRICSDIST,
				(double) MAXLYRICSDIST,
				C_SCORE | C_STAFF, name,
				mainll_item_p,
				&(mainll_item_p->u.ssv_p->lyricsdist) );
		break;

	case LYRICSALIGN:
		(void) do_fassign(var, (double) value,
			(double) MINLYRICSALIGN,
			(double) MAXLYRICSALIGN,
			C_SCORE | C_STAFF, name, mainll_item_p,
			&(mainll_item_p->u.ssv_p->lyricsalign) );
		break;

	case PAD:
		(void) do_fassign(var, (double) value,
			(double) MINPAD,
			(double) MAXPAD,
			C_SCORE | C_STAFF | C_VOICE, name, mainll_item_p,
			&(mainll_item_p->u.ssv_p->pad) );
		/* What the user calls zero means notes can
		 * just touch, but internally we want zero to mean the
		 * default of 1 point of padding, so adjust. */
		mainll_item_p->u.ssv_p->pad -= POINT;
		break;

	case STEMLEN:
		(void) do_fassign(var, (double) value,
			(double) MINSTEMLEN,
			(double) MAXSTEMLEN,
			C_SSV, name, mainll_item_p,
			&(mainll_item_p->u.ssv_p->stemlen) );
		break;


	case BEAMSHORT:
		(void) do_fassign(var, (double) value,
			(double) MINBEAMSHORT,
			(double) MAXBEAMSHORT,
			C_SSV, name, mainll_item_p,
			&(mainll_item_p->u.ssv_p->beamshort) );
		break;

	case A4FREQ:
		(void) do_fassign(var, (double) value,
			(double) MINA4FREQ,
			(double) MAXA4FREQ,
			C_SCORE, name, mainll_item_p,
			&(mainll_item_p->u.ssv_p->a4freq) );
		Tuning_used = YES;
		break;

	default:
		pfatal("invalid float parameter (%d)", var);
		break;
	}
}



/* Handle parameters that have two float numbers as their value */

void
assign_2floats(var, value1, value2, mainll_item_p)

int var;			/* which variable to set */
double value1, value2;		/* which values to set */
struct MAINLL *mainll_item_p;	/* where to store info */

{
	char *name;

	name = parm_name(var);

	/* All of these things can only go into score/staff sorts of things,
	 * not head/foot. If we are in a head/foot, the MAINLL will be null */
	if (mainll_item_p == 0 || Context == C_MUSIC) {
		wrong_context(name);
		return;
	}

	switch (var) {

	case BEAMSLOPE:
		/* First float value is the factor */
		if (do_fassign(var, (double) value1,
			(double) MINBEAMFACT,
			(double) MAXBEAMFACT,
			C_SCORE | C_STAFF | C_VOICE, "beamslope factor",
			mainll_item_p,
			&(mainll_item_p->u.ssv_p->beamfact) ) == YES) {

			/* Fool do_fassign into thinking we haven't set the used
			 * flag yet. This is a little kludgy... */
			mainll_item_p->u.ssv_p->used[var] = NO;

			/* Second value is the max angle in degrees */
			(void) do_fassign(var, value2,
					(double) MINBEAMMAX,
					(double) MAXBEAMMAX,
					C_SCORE | C_STAFF | C_VOICE,
					"beamslope maximum slope angle",
					mainll_item_p,
					&(mainll_item_p->u.ssv_p->beammax) );
		}
		break;

	case TUPLETSLOPE:
		/* This is just like BEAMSLOPE, but just with the tuplet
		 * defines and fields. */
		if (do_fassign(var, (double) value1,
			(double) MINTUPLETFACT,
			(double) MAXTUPLETFACT,
			C_SCORE | C_STAFF | C_VOICE, "tupletslope factor",
			mainll_item_p,
			&(mainll_item_p->u.ssv_p->tupletfact) ) == YES) {

			mainll_item_p->u.ssv_p->used[var] = NO;

			(void) do_fassign(var, value2,
					(double) MINTUPLETMAX,
					(double) MAXTUPLETMAX,
					C_SCORE | C_STAFF | C_VOICE,
					"tupletslope maximum slope angle",
					mainll_item_p,
					&(mainll_item_p->u.ssv_p->tupletmax) );
		}
		break;

	case LEFTSPACE:
		/* First float value is the fraction of room to put on left */
		if (do_fassign(var, (double) value1,
			(double) MINLEFTFACT,
			(double) MAXLEFTFACT,
			C_SCORE, "leftspace portion",
			mainll_item_p,
			&(mainll_item_p->u.ssv_p->leftspacefact) ) == YES) {

			/* As in case above,  fool do_fassign into thinking
			 * we haven't set the used flag yet. */
			mainll_item_p->u.ssv_p->used[var] = NO;

			/* Second value is the max steps of room on left */
			(void) do_fassign(var, value2,
					(double) MINLEFTMAX,
					(double) MAXLEFTMAX,
					C_SCORE,
					"leftspace maximum space in stepsizes",
					mainll_item_p,
					&(mainll_item_p->u.ssv_p->leftspacemax) );
		}
		break;

	default:
		pfatal("bad var value for assign_2floats %d", var);
		/*NOTREACHED*/
		break;
	}
}


/* If user tries to change something that can only be changed before music
 * data is entered, but music has been entered, print error message. */

static void
chg_too_late(var_name)

char *var_name;

{
	if (Got_some_data == YES) {
		l_yyerror(Curr_filename, yylineno,
			"Can't change %s after music or block data has been entered, or after saveparms",
			var_name);
	}
}


/* Do error checks for a float variable. If it passes all checks,
 * set the used flag to YES and return YES.
 * If something fails check, return NO.
 * Checks are: value within range, valid context, and MAINLL struct pointer
 * passed non-NULL.
 * Also give warning if field already used.
 */

static int
do_fassign(var, value, min, max, cont, name, mainll_item_p, ptr2dest)

int var;			/* which variable to set */
double value;			/* what to set it to */
double min;			/* minimum valid value */
double max;			/* maximum valid value */
UINT32B cont;			/* valid context(s) (bitmap) */
char *name;			/* for error messages */
struct MAINLL *mainll_item_p;	/* which structure to set it in */
float *ptr2dest;		/* pointer to the float variable
				 * to be assigned */

{
	char fullname[100];	/* name + " parameter" */

	if (mainll_item_p == (struct MAINLL *) 0) {
		wrong_context(name);
		return(NO);
	}
	(void) sprintf(fullname, parmformat, name);
	if ( contextcheck(cont, fullname) == NO) {
		return(NO);
	}

	/* exclaim if already set in this SSV */
	used_check(mainll_item_p, var, name);

	/* do checks */
	if (frangecheck(value, min, max, name) == NO) {
		return(NO);
	}
	else {
		/* passed all the checks-- assign and mark it as used */
		mainll_item_p->u.ssv_p->used[var] = YES;
		*ptr2dest = value;
		return(YES);
	}
}


/* Set value(s) for the firstpage parameter. It will for sure have
 * the page number to use for the first page, and optionally, whether
 * the first page should be consider a "left" or "right" page.
 */

void
assign_firstpage(pagenum, firstside, mainll_item_p)

int pagenum;			/* page number to use for first page */
int firstside;			/* PGSIDE_* value */
struct MAINLL *mainll_item_p;	/* assign in this SSV */

{
	char *name;
	char fullname[100];	/* varname + " parameter" */


	name = parm_name(FIRSTPAGE);
	(void) sprintf(fullname, parmformat, name);
	if (contextcheck(C_SCORE, fullname) == NO) {
		return;
	}
	chg_too_late(name);
	if (mainll_item_p != 0) {
		(void) do_assign(FIRSTPAGE, pagenum, MINFIRSTPAGE, MAXFIRSTPAGE,
				C_SCORE, NO,
				name, mainll_item_p,
				&(mainll_item_p->u.ssv_p->firstpage) );
		mainll_item_p->u.ssv_p->firstside = firstside;
	}
}


/* assign value to vscheme variable */

void
assign_vscheme(numvoices, vtype, mainll_item_p)

int numvoices;			/* 1, 2, or 3 */
int vtype;			/* V_1, V_2FREESTEM, or V_2OPSTEM. For 3 voice
				 * case, this is still one of the V_2* values,
				 * and in that case it specifies whether
				 * the stems are free or opposing,
				 * with the numvoices indicating the 3 */
struct MAINLL *mainll_item_p;	/* where to assign it */

{
	/* check for proper context */
	if (contextcheck(C_SCORE | C_STAFF, "vscheme parameter") == NO) {
		return;
	}

	if (mainll_item_p == 0) {
		return;
	}

	/* exclaim if already set in this SSV */
	used_check(mainll_item_p, VSCHEME, "vscheme");

	if (rangecheck(numvoices, MINVOICES, MAXVOICES, "vscheme value") == NO) {
		return;
	}

	/* check for valid combination */
	if ( (numvoices == 1) && (vtype != V_1) ) {
		yyerror("can't have 'o' or 'f' qualifier when vscheme=1");
		return;
	}

	if ( (numvoices == 2 || numvoices == 3) && (vtype == V_1) ) {
		yyerror("'o' or 'f' qualifier required when vscheme=2 or vscheme=3");
		return;
	}

	/* The 3 voice things are really just the 2 voice ones, but a third
	 * voice is allowed. They get passed in as V_2*, so fix that */
	if (numvoices == 3) {
		if (vtype == V_2FREESTEM) {
			vtype = V_3FREESTEM;
		}
		else if (vtype == V_2OPSTEM) {
			vtype = V_3OPSTEM;
		}
	}

	/* set variable to requested value */
	mainll_item_p->u.ssv_p->vscheme = (short) vtype;
	mainll_item_p->u.ssv_p->used[VSCHEME] = YES;

	asgnssv(mainll_item_p->u.ssv_p);
}


/* assign value to voicecombine parameter */

void
assign_vcombine(qualifier, bymeas, mainll_p, tssv_p)

int qualifier;			/* VC_* */
int bymeas;			/* YES or NO */
struct MAINLL * mainll_p;	/* A normal SSV */
struct TIMEDSSV * tssv_p;	/* Used instead of mainll_p for mid-measure */

{
	if ( (mainll_p != 0) && (mainll_p->str == S_SSV) ) {
		/* Most common case of setting in a normal SSV */
		if (contextcheck(C_SCORE | C_STAFF, "voicecombine parameter") == NO) {
			return;
		}
		used_check(mainll_p, VCOMBINE, "voicecombine");
		set_vcombine_fields(qualifier, bymeas, mainll_p->u.ssv_p);
	}
	else if (tssv_p != 0) {
		/* being set via mid-measure. */
		if (tssv_p->ssv.used[VCOMBINE] == YES) {
			l_warning(Curr_filename, yylineno, "vcombine specified more than once in mid-measure change; using last");
		}
		if (tssv_p->ssv.context == C_VOICE) {
			l_yyerror(Curr_filename, yylineno, "vcombine cannot be set at voice level");
		}
		set_vcombine_fields(qualifier, bymeas, &(tssv_p->ssv));
	}
	else if ( (mainll_p == 0) && (tssv_p == 0) ) {
		/* must be being called from an invalid context */
		return;
	}
	else {
		pfatal("assign_vcombine called with unexpected struct");
	}
}


/* Set the vcombine fields in an SSV. */

static void
set_vcombine_fields(qualifier, bymeas, ssv_p)

int qualifier;	/* VC_* */
int bymeas;	/* YES or NO */
struct SSV *ssv_p;

{
	short listed[MAXVOICES + 1];	/* If user mentioned the voice */
	struct RANGELIST * curr_p;	/* walk through list of voices */
	int v;				/* voice */
	int offset;			/* index into vcombine array */


	/* Clear list of voices mentioned by user,
	 * and initialize list of voices to combine to none. */
	for (v = 1; v <= MAXVOICES; v++) {
		listed[v] = NO;
		ssv_p->vcombine[v-1] = 0;
	}

	/* We start filling in at beginning of vcombine array */
	offset = 0;

	/* Add the specified voices in input order to SSV vcombine array */
	for (curr_p = VCrange_p; curr_p != 0; curr_p = curr_p->next) {
		/* add voices into voice combine list after error checks */
		for (v = curr_p->begin; v <= curr_p->end; v++) {
			if (listed[v] == YES) {
				l_yyerror(Curr_filename, yylineno,
					"voice %d specified more than once", v);
			}
			if (offset >= MAXVOICES) {
				/* The only way this can happen is if user
				 * specified at least one voice more than once,
				 * and we would have already reported that
				 * above, so no need to print another error.
				 * But we must not attempt to write beyond
				 * end of vcombine array, so jump out of loop.
				 */
				break;
			}
			ssv_p->vcombine[offset++] = v;
			listed[v] = YES;
		}
	}

	free_vcombine_range();
	ssv_p->vcombinequal = (short) qualifier;
	ssv_p->vcombinemeas = (short) bymeas;
	ssv_p->used[VCOMBINE] = YES;
	/* Since voicecombine is relatively rare, we set a flag if it is
	 * ever used. If flag is not set, all the voicecombine placement
	 * code can be skipped entirely. If turning off or only a
	 * single voice is specified, that doesn't really count as being used.
	 */
	if (offset > 1) {
		Vcombused = YES;
	}
}


/* assign key signature */

void
assign_key(num, acc, is_minor, mainll_item_p)

int num;			/* number of sharps or flats */
int acc;			/* # or & for sharp or flat */
int is_minor;			/* YES or NO */
struct MAINLL *mainll_item_p;	/* where to assign */

{
	if (contextcheck( C_SCORE | C_STAFF, "key parameter") == NO) {
		return;
	}

	if (mainll_item_p == 0) {
		return;
	}
	/* exclaim if already set in this SSV */
	used_check(mainll_item_p, SHARPS, "key");

	/* error check. Must be no more than 7 flats or sharps, and can only
	 * be set in score or staff contexts */
	if (rangecheck(num, 0, MAXSHARPS,
			"number of flats or sharps in key signature") == NO) {
		return;
	}

	/* looks okay, so make assignment */
	/* NOTE: num of flats == negative number of sharps */
	mainll_item_p->u.ssv_p->sharps = num * (acc == '#' ? 1 : -1);
	mainll_item_p->u.ssv_p->used[SHARPS] = YES;
	mainll_item_p->u.ssv_p->is_minor = (short) is_minor;

	asgnssv(mainll_item_p->u.ssv_p);
}


/* Assign a string to an SSV variable. It just assigns the pointer for labels,
 * so temporary strings should be copied before being passed.
 * For NOTEHEADS, it parses the string and saves the numeric internal numbers.
 * For SHAPES, it looks up the shape map and assigns that.
 */

void
assign_string(var, string, mainll_item_p)

int var;			/* LABEL, LABEL2, NOTEHEADS, etc */
char *string;			/* the string to assign */
struct MAINLL *mainll_item_p;	/* where to assign it */

{
	int n;			/* note shape index */
	char namebuff[100];	/* For note shape names. Builtin names are
				 * fairly short, but user could define longer
				 * ones. We figure 100 should be plenty,
				 * and ufatal if they try to go longer. */
	int nameleng;		/* strlen of a name shape name */
	UINT32B context;	/* which context to check */
	char *error_msg;	/* what to print in error message */
	struct ACCIDENTALS *acctable_p;	/* to loop through defined accidentals
				 * contexts to verify an acctable parameter
				 * value specified is valid */

	if (mainll_item_p == 0) {
		return;
	}
	if (var == NOTEHEADS) {
		context = C_SSV;
		error_msg = "noteheads parameter";
	}
	else if (var == EMPTYMEAS) {
		context = C_SSV;
		error_msg = "emptymeas parameter";
	}
	else if (var == ACCTABLE) {
		context = C_SCORE;
		error_msg = "acctable parameter";
	}
	else if (var == SHAPES) {
		context  = C_SSV;
		error_msg = "shapes parameter";
	}
	else {
		context = C_SCORE | C_STAFF;
		error_msg = (var == LABEL ? "label parameter"
					: "label2 parameter");
	}

	if (contextcheck(context, error_msg) == YES) {

		/* get string into proper internal format */
		if (string != 0) {
			(void) fix_string(string, string[0], string[1],
					Curr_filename, yylineno);
		}

		switch (var) {

		case LABEL:
			used_check(mainll_item_p, var, "label");
			mainll_item_p->u.ssv_p->label = string;
			break;

		case LABEL2:
			used_check(mainll_item_p, var, "label2");
			mainll_item_p->u.ssv_p->label2 = string;
			break;

		case NOTEHEADS:
			if (has_tab_staff() == YES
					&& strcmp(string+2, "allx") != 0
					&& strcmp(string+2, "norm") != 0) {
				l_warning(Curr_filename, yylineno,
					"noteheads parameter ignored on tablature staffs (unless allx or norm)");
			}

			/* skip past font/size */
			string += 2;
			/* split into tokens */
			for (n = 0; n < 7; n++) {
				/* skip past white space */
				while ( isspace(*string) ) {
					string++;
				}

				if ( *string == '\0') {
					break;
				}

				nameleng = strcspn(string, " \t\r\n");
				if (nameleng > sizeof(namebuff) - 1) {
					ufatal("note head name too long");
				}
				strncpy(namebuff, string, nameleng);
				namebuff[nameleng] = '\0';
				if ((mainll_item_p->u.ssv_p->noteheads[n] =
							get_shape_num(namebuff))
							== HS_UNKNOWN) {
					l_yyerror(Curr_filename, yylineno,
						"'%s' is not a valid headshape name",
						namebuff);
				}
				string += nameleng;
			}
			if (n == 1) {
				/* copy same shape for all 7 */
				for (  ; n < 7; n++) {
					mainll_item_p->u.ssv_p->noteheads[n] =
					mainll_item_p->u.ssv_p->noteheads[0];
				}
			}

			/* Skip past trailing white space, and make sure we got
			 * right number of tokens. */
			while ( isspace(*string) ) {
				string++;
			}
			if (n != 7 || *string != '\0') {
				yyerror("wrong number of notehead names: expecting either 1 or 7");
			}
			break;

		case SHAPES:
			if (string == 0) {
				mainll_item_p->u.ssv_p->shapes = 0;
			}
			else if ((mainll_item_p->u.ssv_p->shapes =
					get_shape_map(string + 2)) == 0) {
				l_yyerror(Curr_filename, yylineno,
					"shape map '%s' has not been defined",
					string + 2);
			}
			break;

		case EMPTYMEAS:
			used_check(mainll_item_p, var, "emptymeas");
			/* skip the font/size byte */
			mainll_item_p->u.ssv_p->emptymeas = string + 2;
			break;

		case ACCTABLE:
			used_check(mainll_item_p, var, "acctable");
			if (string != 0) {
				mainll_item_p->u.ssv_p->acctable = string + 2;
				/* Make sure the name is a valid table
				 * (i.e., one that has been defined). */
				for (acctable_p = Acc_contexts_list_p; acctable_p != 0;
						acctable_p = acctable_p->next) {
					if (strcmp(acctable_p->name, string + 2) == 0) {
						break;
					}
				}
				if (acctable_p == 0) {
					l_yyerror(Curr_filename, yylineno,
						"acctable '%s' has not been defined", string + 2);
				}

				Tuning_used = YES;
			}
			else {
				mainll_item_p->u.ssv_p->acctable = 0;
			}
			break;

		default:
			pfatal("invalid string variable type");
			/*NOTREACHED*/
			break;
		}

		mainll_item_p->u.ssv_p->used[var] = YES;
	}
}


/* Set one of the keymap parameters. */

void
set_keymap(which, name, mll_p)

int which;		/* The *KEYMAP SSV field */
char *name;		/* name of the keymap */
struct MAINLL *mll_p;	/* where to save */

{
	UINT32B valid_contexts = 0;
	struct KEYMAP *keymap_p = 0;	/* The named map */


	if (mll_p == 0) {
		return;
	}
	if (name != 0) {
		/* Skip past the font/size bytes */
		name += 2;

		/* Set flag to tell us that keymap code needs to be called.
		 * If this is never set (the most normal case) we can skip
		 * the string mapping code. */
		Keymap_used = YES;

		/* Make sure a map by that name has been defined */
		if ((keymap_p = get_keymap(name)) == 0) {
			l_yyerror(Curr_filename, yylineno, "keymap '%s' has not been defined", name);
			name = (char *) "";
		}
	}

	/* Verify used in valid context */
	switch (which) {
	case DEFAULTKEYMAP:
	case LYRICSKEYMAP:
	case TEXTKEYMAP:
	case WITHKEYMAP:
	case ENDINGKEYMAP:
	case LABELKEYMAP:
	case REHEARSALKEYMAP:
		valid_contexts = C_SCORE | C_STAFF;
		break;
	case PRINTKEYMAP:
		valid_contexts = C_SCORE | C_BLOCKHEAD;
		break;
	default:
		pfatal("Invalid keymap which value passed to set_keymap");
		break;
	}
	if (contextcheck(valid_contexts, parm_name(which)) == NO) {
		return;
	}

	if (Context & C_BLOCKHEAD) {
		/* Special case. In blocks, we just keep track
		 * of the current keymap value. */
		if (which == DEFAULTKEYMAP) {
			Score.defaultkeymap = keymap_p;
		}
		else if (which == PRINTKEYMAP) {
			Score.printkeymap = keymap_p;
		}
		return;
	}

	switch (which) {
	case DEFAULTKEYMAP:
		mll_p->u.ssv_p->defaultkeymap = keymap_p;
		break;
	case ENDINGKEYMAP:
		mll_p->u.ssv_p->endingkeymap = keymap_p;
		break;
	case LABELKEYMAP:
		mll_p->u.ssv_p->labelkeymap = keymap_p;
		break;
	case LYRICSKEYMAP:
		mll_p->u.ssv_p->lyricskeymap = keymap_p;
		break;
	case PRINTKEYMAP:
		mll_p->u.ssv_p->printkeymap = keymap_p;
		break;
	case REHEARSALKEYMAP:
		mll_p->u.ssv_p->rehearsalkeymap = keymap_p;
		break;
	case TEXTKEYMAP:
		mll_p->u.ssv_p->textkeymap = keymap_p;
		break;
	case WITHKEYMAP:
		mll_p->u.ssv_p->withkeymap = keymap_p;
		break;
	default:
		pfatal("invalid case for assignment in set_keymap");
		break;
	}
	mll_p->u.ssv_p->used[which] = YES;
}


/* make a copy of a string and return pointer to it,
 * or return NULL if string was NULL. The incoming string is a regular C-style
 * string. The returned string is 2 bytes longer, with the font in the first
 * byte, size in the second byte, and the copy of the original string in the
 * remainder. */

char *
copy_string(string, font, size)

char *string;		/* make a copy of this string */
int font;		/* use this font */
int size;		/* use this point size */

{
	char *copy;	/* pointer to new copy of string */


	if (string == (char *) 0) {
		return (string);
	}

	/* need 2 extra bytes at beginning for font and size,
	 * and 1 at end for '\0' */
	MALLOCA(char, copy, strlen(string) + 3);

	/* fill in font and size in first 2 bytes */
	*copy = (char) font;
	*(copy + 1) = (char) size;

	/* copy the string and return pointer to copy */
	(void) strcpy(copy + 2, string);
	return(copy);
}


/* Assign time signature in SSV.
 * Derives the effective numerator/denominator and the RATIONAL time value
 * from the timerep, and fills them in the SSV. If there are alternating
 * time signatures, that will be for the first of them, and a pointer
 * to the remaining signature(s) will be returned via next_alternation_p.
 * If there are additive time signatures, the effective num/den will
 * be based on the largest denominator.
 */

void
assign_timesig(mainll_item_p, visibility, next_alternation_p)

struct MAINLL *mainll_item_p;   /* SSV to assign time signature in */
int visibility;                	/* PTS_* value */
char **next_alternation_p;	/* If this time signature includes alternating
				 * time signatures, as in  3/4  4/4,
				 * this will be filled in with a pointer to
				 * where the next alternate time signature
				 * begins in the timerep. If there are no
				 * alternating time signatures, it will be
				 * filled by a null pointer. */

{
	struct SSV *ssv_p;		/* mainll_item_p->u.ssv_p */
	RATIONAL curr_value;		/* There may be compound time
					 * signatures, and each of those
					 * may have multiple numerator
					 * components, so this is used for
					 * getting value of one fraction */
	int biggest_denominator;	/* for calculating effective
					 * numerator and denominator */
	char *t;			/* to walk through timerep */


	if (contextcheck(C_SCORE, "time parameter") == NO) {
		return;
	}

	if (mainll_item_p == 0) {
		return;
	}
	/* exclaim if already set in this SSV */
	used_check(mainll_item_p, TIME, "time signature");

	ssv_p = mainll_item_p->u.ssv_p;

	ssv_p->timevis = visibility;

	curr_value = Zero;
	ssv_p->time = Zero;
	biggest_denominator = 0;
	*next_alternation_p = 0;

	for (t = ssv_p->timerep; *t != TSR_END; t++) {
		if (*t == TSR_CUT) {
			curr_value.n = 2;
			curr_value.d = 2;
		}
		else if (*t == TSR_COMMON) {
			curr_value.n = 4;
			curr_value.d = 4;
		}
		else if (*t == TSR_SLASH) {
			curr_value.d = *++t;
		}
		else if (*t == TSR_ALTERNATING) {
			*next_alternation_p = ++t;
			break;
		}
		else if (*t == TSR_ADD) {
			continue;
		}
		else {
			curr_value.n += *t;
			continue;
		}
		biggest_denominator = MAX(biggest_denominator, curr_value.d);
		rred(&curr_value);
		ssv_p->time = radd(ssv_p->time, curr_value);
		curr_value = Zero;
	}

	/* If there were mixed denominators, use the biggest for the
	 * purpose of effective numerator and denominator */
	if (biggest_denominator > ssv_p->time.d) {
		ssv_p->timenum = ssv_p->time.n * (biggest_denominator / ssv_p->time.d);
	}
	else {
		ssv_p->timenum = ssv_p->time.n;
	}
	ssv_p->timeden = biggest_denominator;

	/* mark time as used */
	mainll_item_p->u.ssv_p->used[TIME] = YES;

	/* We have to set this for real immediately, since beamstyle and
	 * other things may need to have it set */
	asgnssv(mainll_item_p->u.ssv_p);

}


/* Assign a font variable, either a family or face within that family
 * (e.g., FONT, LYRICSFONT, FONTFAMILY, LYRICSFAMILY) */

void
set_font(var, value, mainll_item_p)

int var;			/* which variable to set */
int value;			/* which font to set it too */
struct MAINLL *mainll_item_p;	/* where to assign it in main list */

{
	char *varname;	/* name of variable, for error messages */
	char fullname[100];	/* varname + " parameter" */


	if (mainll_item_p == 0 && (!(Context & C_BLOCKHEAD)
				|| (var != FONT && var != FONTFAMILY))) {
		wrong_context(parm_name(var));
		return;
	}
	/* determine the name of the variable, for error messages */
	varname = parm_name(var);
	(void) sprintf(fullname, parmformat, varname);

	/* if being called from SSV, exclaim if already set */
	if ((Context & C_SSV) != 0) {
		used_check(mainll_item_p, var, varname);
	}

	switch (var) {

	case FONT:
		Curr_font = value;

		if (Context & C_BLOCKHEAD) {
			/* Special case. In block, we just
			 * keep track of the current font */
			return;
		}
		else if (contextcheck(C_SCORE | C_STAFF, fullname) == YES) {
			mainll_item_p->u.ssv_p->font = (short) value;
		}
		else {
			return;
		}

		break;

	case FONTFAMILY:
		Curr_family = value;

		if (Context & C_BLOCKHEAD) {
			/* Special case. In block, we just
			 * keep track of the current font */
			return;
		}
		else if (contextcheck(C_SCORE | C_STAFF, fullname) == YES) {
			mainll_item_p->u.ssv_p->fontfamily = (short) value;
		}
		else {
			return;
		}

		break;

	case LYRICSFONT:
		if (contextcheck(C_SCORE | C_STAFF, fullname) == YES) {
			mainll_item_p->u.ssv_p->lyricsfont = (short) value;

			/* assign immediately in case there is a following
			 * font family change that needs to read it */
			mainll_item_p->u.ssv_p->used[var] = YES;
			asgnssv(mainll_item_p->u.ssv_p);
			setlyrfont(mainll_item_p->u.ssv_p->staffno, value);
			return;
		}
		else {
			return;
		}

		/*NOTREACHED*/
		break;

	case LYRICSFAMILY:
		if (contextcheck(C_SCORE | C_STAFF, fullname) == YES) {
			mainll_item_p->u.ssv_p->lyricsfamily = (short) value;
			/* assign immediately, so we can reset all
			 * lyrics info for this staff */
			mainll_item_p->u.ssv_p->used[var] = YES;
			asgnssv(mainll_item_p->u.ssv_p);

			setlyrfont(mainll_item_p->u.ssv_p->staffno,
				svpath(mainll_item_p->u.ssv_p->staffno,
				LYRICSFONT)->lyricsfont);
			return;
		}
		else {
			return;
		}

		/*NOTREACHED*/
		break;

	case MEASNUMFONT:
		if (contextcheck(C_SCORE, fullname) == YES) {
			mainll_item_p->u.ssv_p->measnumfont = (short) value;
		}
		else {
			return;
		}
		break;

	case MEASNUMFAMILY:
		if (contextcheck(C_SCORE, fullname) == YES) {
			mainll_item_p->u.ssv_p->measnumfamily = (short) value;
		}
		else {
			return;
		}
		break;

	case WITHFONT:
		if (contextcheck(C_SCORE | C_STAFF | C_VOICE, fullname) == YES) {
			mainll_item_p->u.ssv_p->withfont = (short) value;
		}
		else {
			return;
		}
		break;

	case WITHFAMILY:
		if (contextcheck(C_SCORE | C_STAFF | C_VOICE, fullname) == YES) {
			mainll_item_p->u.ssv_p->withfamily = (short) value;
		}
		else {
			return;
		}
		break;

	case NOTELEFTFONT:
		if (contextcheck(C_SCORE | C_STAFF | C_VOICE, fullname) == YES) {
			mainll_item_p->u.ssv_p->noteleftfont = (short) value;
		}
		else {
			return;
		}
		break;

	case NOTELEFTFAMILY:
		if (contextcheck(C_SCORE | C_STAFF | C_VOICE, fullname) == YES) {
			mainll_item_p->u.ssv_p->noteleftfamily = (short) value;
		}
		else {
			return;
		}
		break;

	default:
		pfatal("unknown font variable");
		break;
	}

	mainll_item_p->u.ssv_p->used[var] = YES;
}


/* set number of stafflines and whether or not to print clef. Number of
 * lines must be 1 or 5, unless it's a tablature staff,
 * in which case it can be anything from MINTABLINES to MAXTABLINES.
 * In any case, it must be set before any music data. */

void
asgn_stafflines(numlines, printclef, mainll_item_p)

int numlines;	/* 1 or 5 for normal, or MINTABLINES to MAXTABLINES for tablature */
int printclef;	/* SS_* for not-tab or PTC for tab */
struct MAINLL *mainll_item_p;	/* where to set value */

{
	int is_tab; 	/* YES if is tablature staff */

	
	if (mainll_item_p == (struct MAINLL *) 0) {
		/* must be in here due to some user syntax error */
		return;
	}

	is_tab = is_tablature_staff(mainll_item_p->u.ssv_p);
	if (is_tab == YES) {
		/* Note that since this is setting to tab,
		 * we don't need to check for mixture of tab/nontab SSVs
		 * being defined, since all of them will change together
		 * to being tab, even if some weren't before.
		 */ 
		if (contextcheck(C_STAFF, "stafflines=tab") == NO) {
			return;
		}
	}
	else {
		if (contextcheck(C_SCORE | C_STAFF, "stafflines parameter") == NO) {
			return;
		}
	}

	/* exclaim if already set in this SSV */
	used_check(mainll_item_p, STAFFLINES, "stafflines");

	if (is_tab == YES) {
		/* is a tablature staff */
		(void) rangecheck(numlines, MINTABLINES, MAXTABLINES,
					"number of tab strings specified");
	}
	else {
		/* not a tablature staff */
		if (numlines != 5 && numlines != 1) {
			yyerror("stafflines must be 1 or 5");
		}
	}


	mainll_item_p->u.ssv_p->stafflines = (short) numlines;

	/* single line never has clef except the drum clef,
	 * so if user didn't explictly set 'n' we do it for them */
	if (is_tab == NO && numlines == 1 && printclef == SS_NORMAL) {
		printclef = SS_NOTHING;
	}
	if (is_tab == YES) {
		mainll_item_p->u.ssv_p->printtabclef = (short) printclef;
		/* printclef shouldn't actually be used on tab,
		 * but just in case, we set it to its default */
		mainll_item_p->u.ssv_p->printclef = SS_NORMAL;
	}
	else {
		mainll_item_p->u.ssv_p->printclef = (short) printclef;
		mainll_item_p->u.ssv_p->printtabclef = PTC_FIRST;
	}
	mainll_item_p->u.ssv_p->used[STAFFLINES] = YES;

	/* tab and tabnote need addition consistency checks */
	chk_tab(mainll_item_p);
}


/* An SSV that sets stafflines needs extra checks to make sure tab/tabnote
 * are still consistent. Every tab staff must have a non-tab above it.
 * This function should only be called on main list SSVs that have the
 * stafflines field used.
 */

void
chk_tab(mainll_item_p)

struct MAINLL *mainll_item_p;	/* points to an SSV */

{
	int staff_index;
	int is_tab;

	if (mainll_item_p == 0) {
		return;
	}
	is_tab = is_tablature_staff(mainll_item_p->u.ssv_p);
	/* index into Staff array is staffno - 1 */
	staff_index = mainll_item_p->u.ssv_p->staffno - 1;

	/* need to do extra consistency check for tablature staffs */
	if (is_tab == YES) {
		
		if (staff_index == 0) {
			yyerror("staff 1 can't be a tablature staff");
		}
		else {
			if (is_tablature_staff( &(Staff[staff_index - 1]) ) == YES
				|| (staff_index < MAXSTAFFS - 1 &&
				is_tablature_staff( &(Staff[staff_index + 1]) )
				== YES) ) {
			    l_yyerror(Curr_filename, yylineno,
					"can't have two consecutive tablature staffs (%d-%d)",
					mainll_item_p->u.ssv_p->staffno - 1,
					mainll_item_p->u.ssv_p->staffno);
			}
			if (svpath(mainll_item_p->u.ssv_p->staffno - 1,
					STAFFLINES)->stafflines != 5) {
				l_yyerror(Curr_filename, yylineno,
					"staff %d before a tablature staff must be a 5-line staff",
					mainll_item_p->u.ssv_p->staffno - 1);
			}
		}
		if (mainll_item_p->u.ssv_p->used[CLEF] == YES) {
			/* User setting this combination in the other order
			 * (clef after tablature) now has to be an error,
			 * (see comment in gram.y)
			 * so maybe we should make this an error too
			 * for consistency, but we can fix this case,
			 * so can make it only a warning.
			 */
			l_warning(Curr_filename, yylineno,
				"Previously set clef will be ignored on tablature staff");
			mainll_item_p->u.ssv_p->used[CLEF] = NO;
		}
	}
	else {
		/* if trying to establish a non-5-line non-tablature staff,
		 * and it's not the bottom staff, and the staff below is a
		 * tablature staff, that's a no-no */
		if (mainll_item_p->u.ssv_p->stafflines != 5
				&& staff_index < MAXSTAFFS - 1 &&
				is_tablature_staff( &(Staff[staff_index + 1]) )
				== YES) {
			l_yyerror(Curr_filename, yylineno,
				"staff %d before a tablature staff must be a 5-line staff",
				mainll_item_p->u.ssv_p->staffno);
		} 
	}

	/* assign, so we can do error checking on tablature staffs
	 * for future SSVs that we process */
	asgnssv(mainll_item_p->u.ssv_p);
}


/* When the input contains a rangelist, we need to allocate some space for
 * the information. Allocate an array of length CHUNK. Set the Ss_count to start
 * filling in element 0, and mark the Ss_length as CHUNK. As we add elements,
 * Ss_count will be incremented, and the array size enlarged if it overflows.
 */

void
new_staffset()

{
	CALLOC(STAFFSET, Curr_staffset_p, CHUNK);

	Ss_count = 0;
	Ss_length = CHUNK;
}


/* add information about one staffset, at the current offset in the list,
 * re-allocating additional space if necessary */

void
add_staffset(start, end, label1, label2)

int start, end;		/* of the range */
char *label1, *label2;	/* malloc-ed copies of labels for the range, or NULL */

{
	/* Murphey's Law insurance */
	if (Curr_staffset_p == (struct STAFFSET *) 0) {
		pfatal("NULL staffset");
	}

	/* swap if backwards */
	if ( start > end) {
		int tmp;

		tmp = start;
		start = end;
		end = tmp;
	}

	/* if we guessed too small, need to make a bigger array */
	if (Ss_count >= Ss_length) {
		Ss_length += CHUNK;
		REALLOC(STAFFSET, Curr_staffset_p, Ss_length);
	}

	/* fill in values */
	Curr_staffset_p[Ss_count].topstaff = (short) start;
	Curr_staffset_p[Ss_count].botstaff = (short) end;
	if (label1 != (char *) 0) {
		(void) fix_string(label1, label1[0], label1[1],
					Curr_filename, yylineno);
	}
	Curr_staffset_p[Ss_count].label = label1;
	if (label2 != (char *) 0) {
		(void) fix_string(label2, label2[0], label2[1],
					Curr_filename, yylineno);
	}
	Curr_staffset_p[Ss_count].label2 = label2;

	/* one more item in list */
	Ss_count++;
}


/* When we have collected an entire list of ranges, assign the
 * list to the appropriate place in the SSV struct.
 * (Using "set" instead of "assign" for function name because some
 * people's compilers are too stupid to tell the difference in names
 * after 8 characters, which would clash with another function name) */

void
set_staffset(var, mainll_item_p)

int var;			/* which rangelist to set */
struct MAINLL *mainll_item_p;	/* which struct to assign it in */

{
	register int i;		/* index through ranges */
	short okay = NO;	/* if passed all overlap checks */


	/* can only do this in score context */
	if (contextcheck(C_SCORE, "list of staff ranges") == NO) {
		return;
	}

	/* first we need to make sure no ranges are out of range */
	/* go through the list of ranges */
	for (i = 0; i < Ss_count; i++) {

		/* range check. Make sure it is within number of staffs that
		 * user specified. Since when we assign the user-specified
		 * number, we make sure that is within MAXSTAFFS, it will
		 * be within the absolute maximum as well.
		 */
		if (rangecheck(Curr_staffset_p[i].botstaff, 1, Score.staffs,
					"brace/bracket staff number") == NO) {
			return;
		}
	}
	
	/* if explicitly empty, can free space */
	if (Ss_count == 0) {
		FREE(Curr_staffset_p);
	}
	else {
		/* we probably have too much space allocated, shed the rest */
		REALLOC(STAFFSET, Curr_staffset_p, Ss_count);

		/* sort lowest to highest */
		qsort( (char *) Curr_staffset_p, (unsigned int) Ss_count,
					sizeof(struct STAFFSET), comp_staffset);
	}

	if (mainll_item_p == (struct MAINLL *) 0) {
		/* must have been an earlier error */
		return;
	}

	/* now assign to appropriate variable */
	switch (var) {

	case BRACELIST:
		mainll_item_p->u.ssv_p->bracelist = (Ss_count == 0 ?
				(struct STAFFSET *) 0 : Curr_staffset_p);
		mainll_item_p->u.ssv_p->nbrace = (short) Ss_count;
		okay = brac_check(Curr_staffset_p, Ss_count,
					Score.bracklist, Score.nbrack);
		used_check(mainll_item_p, var, "brace");
		break;

	case BRACKLIST:
		mainll_item_p->u.ssv_p->bracklist = (Ss_count == 0 ?
				(struct STAFFSET *) 0 : Curr_staffset_p);
		mainll_item_p->u.ssv_p->nbrack = (short) Ss_count;
		okay = brac_check(Score.bracelist, Score.nbrace,
					Curr_staffset_p, Ss_count);
		used_check(mainll_item_p, var, "bracket");
		break;

	case SUBBARSTYLE:
		/* The assignment is done in gram.y, via call to set_sb_range,
		 * since it goes into a sub-struct we don't know about here. */
		break;

	default:
		pfatal("unknown staffset type");
		break;
	}

	if (okay == YES) {
		mainll_item_p->u.ssv_p->used[var] = YES;

		/* assign now, so we can check for overlap */
		asgnssv(mainll_item_p->u.ssv_p);
	}

	/* the list has been attached to its permanent place, so
	 * reset the temporary pointer to an empty list */
	Curr_staffset_p = (struct STAFFSET *) 0;
	Ss_count = 0;
}


/* compare 2 STAFFSETs for sorting using qsort */

static int
comp_staffset(item1_p, item2_p)

#ifdef __STDC__
const void *item1_p;	/* the two items to compare */
const void *item2_p;
#else
char *item1_p;	/* the two items to compare */
char *item2_p;
#endif

{
	int top1, top2;
	int bot1, bot2;

	top1 = ((struct STAFFSET *)item1_p)->topstaff;
	top2 = ((struct STAFFSET *)item2_p)->topstaff;
	bot1 = ((struct STAFFSET *)item1_p)->botstaff;
	bot2 = ((struct STAFFSET *)item2_p)->botstaff;

	if (top1 < top2) {
		return(-1);
	}

	else if (top1 > top2) {
		return(1);
	}

	else if (bot1 < bot2) {
		return(-1);
	}

	else if (bot1 > bot2) {
		return(1);
	}

	else {
		return(0);
	}
}


/* allocate an array of TOP_BOT structs for building up a list of bar style
 * information (which staffs to bar together) */

void
new_barstlist()

{
	CALLOC(TOP_BOT, Curr_barstlist_p, CHUNK);

	/* initialize used and allocated lengths */
	Barst_count = 0;
	Barst_length = CHUNK;
}


/* add a pair of staff numbers to bar style list */

void
add_barst(start, end, between, all)

int start;	/* first staff to bar together */
int end;	/* last staff to bar together */
int between;	/* YES or NO; if to draw between staffs */
int all;	/* YES if special range meaning all staffs, however many there
		 * are at any given time. */

{
	if (Curr_barstlist_p == (struct TOP_BOT *) 0) {
		pfatal("NULL barstlist");
	}

	/* swap if backwards */
	if ( start > end) {
		int tmp;

		tmp = start;
		start = end;
		end = tmp;
	}

	/* if we guessed too small, make a bigger array */
	if (Barst_count >= Barst_length) {

		Barst_length += CHUNK;

		REALLOC(TOP_BOT, Curr_barstlist_p, Barst_length);
	}

	Curr_barstlist_p[Barst_count].top = (short) start;
	Curr_barstlist_p[Barst_count].bottom = (short) end;
	Curr_barstlist_p[Barst_count].between = (short) between;
	Curr_barstlist_p[Barst_count].all = (short) all;

	/* one more item on list */
	Barst_count++;
}


/* When we have collected an entire list of ranges, assign the
 * list to the appropriate place in the SSV struct */

void
set_barstlist(mainll_item_p, subbar_app_p)

struct MAINLL *mainll_item_p;	/* which struct to assign it */
struct SUBBAR_APPEARANCE *subbar_app_p;	/* which struct to assign to
				 * if subbarstyle, else NULL */ 

{
	register int i, s;		/* index for ranges and staffs */
	short mentioned[MAXSTAFFS + 1];	/* mark whether each staff occurs in
					 * this list somewhere. Element 0 is
					 * unused */
	short bet_mentioned[MAXSTAFFS + 1]; /* similar for between */


	if (subbar_app_p == 0) {
		/* is barstyle, not subbarstyle */
		if (contextcheck(C_SCORE, "barstyle parameter") == NO) {
			return;
		}

		/* exclaim if already set in this SSV */
		used_check(mainll_item_p, BARSTLIST, "barstyle");
	}
	else {
		/* is subbarstyle */
		if (contextcheck(C_SCORE, "subbarstyle parameter") == NO) {
			return;
		}

		/* Note: the used_check is done in subbar_check rule.
		 * Can't do here, because this function may be called
		 * multiple times, once per appearance type */
	}

	/* first we need to make sure no ranges overlap or are out of range */

	/* initialize that we haven't seen anything yet */
	for (s = 1; s < MAXSTAFFS + 1; s++) {
		mentioned[s] = NO;
		bet_mentioned[s] = NO;
	}

	/* go through the list of ranges */
	for (i = 0; i < Barst_count; i++) {

		/* range check. */
		if (rangecheck(Curr_barstlist_p[i].bottom, 1,
				(Curr_barstlist_p[i].all == YES ? MAXSTAFFS : Score.staffs),
				"barstyle staff number") == NO) {
			continue;
		}
		
		/* fill in each in the range as having been mentioned.
		 * If already mentioned, we have a problem */
		for (s = Curr_barstlist_p[i].top;
					s <= Curr_barstlist_p[i].bottom; s++) {

			if (Curr_barstlist_p[i].all == YES) {
				if (bet_mentioned[s] == YES) {
					yyerror("overlapping range in between barstyle list");
				}
				else {
					bet_mentioned[s] = YES;
				}
			}
			else {
				if (mentioned[s] == YES) {
					yyerror("overlapping range in barstyle list");
				}
				else {
					mentioned[s] = YES;
				}
			}
		}
	}
	
	/* if explicitly empty, free space */
	if (Barst_count == 0) {
		FREE(Curr_barstlist_p);
	}

	else {
		/* we probably have too much space allocated, shed the rest */
		REALLOC(TOP_BOT, Curr_barstlist_p, Barst_count);

		/* sort lowest to highest */
		qsort ( (char *) Curr_barstlist_p, (unsigned int) Barst_count,
			sizeof(struct TOP_BOT), comp_barst);
	}

	if (mainll_item_p == (struct MAINLL *) 0) {
		return;
	}

	/* fill in data */
	if (subbar_app_p == 0) {
		mainll_item_p->u.ssv_p->nbarst = (short) Barst_count;
		if (Barst_count > 0) {
			mainll_item_p->u.ssv_p->barstlist = Curr_barstlist_p;
		}
		mainll_item_p->u.ssv_p->used[BARSTLIST] = YES;
	}
	else {
		subbar_app_p->nranges = (short) Barst_count;
		if (Barst_count > 0) {
			subbar_app_p->ranges_p = Curr_barstlist_p;
		}
		mainll_item_p->u.ssv_p->used[SUBBARSTYLE] = YES;
	}

	/* now that list has been assigned to its proper place,
	 * re-initialize pointer to null to prepare for another list */
	Curr_barstlist_p = (struct TOP_BOT *) 0;
	Barst_count = 0;
}


/* compare 2 barslist items for sorting using qsort */

static int
comp_barst(item1_p, item2_p)

#ifdef __STDC__
const void *item1_p;	/* the two items to compare */
const void *item2_p;
#else
char *item1_p;	/* the two items to compare */
char *item2_p;
#endif

{
	if ( ((struct TOP_BOT *)item1_p)->top
				< ((struct TOP_BOT *)item2_p)->top) {
		return(-1);
	}

	else if ( ((struct TOP_BOT *)item1_p)->top
				> ((struct TOP_BOT *)item2_p)->top) {
		return(1);
	}

	else {
		/* actually this should never occur */
		return(0);
	}
}


/* Initialize and allocate space for beamstyle information */

void
new_beamlist()

{
	init_beamlist(&Curr_beamstyle);
	init_beamlist(&Curr_subbeamstyle);
	Subbeam_index = -1;
}


/* Initalize a BEAMLIST struct.
 * Allocate CHUNK entries, and mark that 0 of them are currently used.
 */

static void
init_beamlist(beamlist_p)

struct BEAMLIST *beamlist_p;

{
	MALLOCA(RATIONAL, beamlist_p->list_p, CHUNK);
	beamlist_p->count = 0;
	beamlist_p->length = CHUNK;
}


/* This function is called at the parenthesis to begin a sub-beam grouping.
 * It saves the current index into the subbeam list. At the ending parenthesis,
 * we add up add the subbeam list time values since that saved index.
 */

void
begin_subbeam()

{
	if (Subbeam_index >= 0) {
		yyerror("Nested sub-beam groups not allowed");
		return;
	}
	Subbeam_index = Curr_subbeamstyle.count;
}

void
end_subbeam()
{
	RATIONAL tot_time;

	/* Do error checks */
	if (Subbeam_index < 0) {
		yyerror("Missing '(' for sub-beam grouping");
		return;
	}
	if (Subbeam_index >= Curr_subbeamstyle.count - 1) {
		warning("sub-beam grouping needs at least two values");
	}

	/* Count up all the time values of subbeams that make up the
	 * single outer beam. */
	for (tot_time = Zero; Subbeam_index < Curr_subbeamstyle.count;
						Subbeam_index++) {
		tot_time = radd(tot_time, Curr_subbeamstyle.list_p[Subbeam_index]);
	}
	add2outerbeam(tot_time);
	Subbeam_index = -1;
}


/* Add an entry to the current beam list */

void
add_beamlist(value)

RATIONAL value;		/* what to add to beam list */

{
	/* If we guessed too small, make a bigger array */
	if (Curr_subbeamstyle.count >= Curr_subbeamstyle.length) {
		Curr_subbeamstyle.length += CHUNK;
		REALLOCA(RATIONAL, Curr_subbeamstyle.list_p, Curr_subbeamstyle.length);
	}

	Curr_subbeamstyle.list_p[Curr_subbeamstyle.count] = value;
	(Curr_subbeamstyle.count)++;

	/* If not in a subbeam grouping, goes into Curr_beamstyle too */
	if (Subbeam_index < 0) {
		add2outerbeam(value);
	}
}


/* Add entry to time values for the outermost beam. In the case of subbeaming,
 * the value will be the sum of the subbeams values; otherwise it will be
 * the same as that in the subbeam list.
 */

static void
add2outerbeam(value)

RATIONAL value;

{
	/* If we guessed too small, make a bigger array */
	if (Curr_beamstyle.count >= Curr_beamstyle.length) {
		Curr_beamstyle.length += CHUNK;
		REALLOCA(RATIONAL, Curr_beamstyle.list_p, Curr_beamstyle.length);
	}
	Curr_beamstyle.list_p[Curr_beamstyle.count] = value;
	(Curr_beamstyle.count)++;
}


/* Assign current beam list to SSV structure in main list */

void
set_beamlist(mainll_item_p)

struct MAINLL *mainll_item_p;		/* where to attach list */

{
	if (contextcheck(C_SSV, "beamstyle parameter") == NO) {
		return;
	}

	/* exclaim if already set in this SSV */
	used_check(mainll_item_p, BEAMSTLIST, "beamstyle");

	/* Shed any extra allocated space */
	if (Curr_beamstyle.count == 0) {
		FREE(Curr_beamstyle.list_p);
		Curr_beamstyle.list_p = 0;
	}
	else if (Curr_beamstyle.count < Curr_beamstyle.length) {
		REALLOCA(RATIONAL, Curr_beamstyle.list_p, Curr_beamstyle.count);
	}
	if (Curr_subbeamstyle.count == 0) {
		FREE(Curr_subbeamstyle.list_p);
		Curr_subbeamstyle.list_p = 0;
	}
	else if (Curr_subbeamstyle.count < Curr_subbeamstyle.length) {
		REALLOCA(RATIONAL, Curr_subbeamstyle.list_p, Curr_subbeamstyle.count);
	}

	if (mainll_item_p == (struct MAINLL *) 0) {
		return;
	}

	if (Context != C_SCORE && has_tab_staff() == YES) {
		warning("beamstyle not allowed on tablature staff; ignoring");
		/* go ahead and fall through to assign, since other SSVs
		 * that get cloned might be non-tab. */
	}

	/* attach to the SSV struct and mark as used */
	mainll_item_p->u.ssv_p->beamstlist = Curr_beamstyle.list_p;
	mainll_item_p->u.ssv_p->subbeamstlist = Curr_subbeamstyle.list_p;
	mainll_item_p->u.ssv_p->nbeam = (short) Curr_beamstyle.count;
	mainll_item_p->u.ssv_p->nsubbeam = (short) Curr_subbeamstyle.count;
	mainll_item_p->u.ssv_p->used[BEAMSTLIST] = YES;

	/* Mark temporary lists as invalid */
	Curr_beamstyle.list_p = 0;
	Curr_subbeamstyle.list_p = 0;

	asgnssv(mainll_item_p->u.ssv_p);
}


/* Do checks at end of measure for interactions between time and beamstyle.
 * Setting time zaps the beamstyle, but if the user also changes beamstyle
 * in the same context, we use that even if it occurs earlier in the input.
 */

void
check_beamstyle(ssv_p)

struct SSV *ssv_p;

{
	RATIONAL tot_time;	/* sum of times in beamstyle list */
	register int n;

	if (ssv_p->used[BEAMSTLIST] == YES && ssv_p->nbeam > 0) {
		/* make sure time adds up to exactly a measure */
		tot_time = Zero;
		for (n = 0; n < ssv_p->nbeam; n++) {
			tot_time = radd(tot_time, ssv_p->beamstlist[n]);
		}
		if (NE(tot_time, Score.time)) {
			yyerror("beam list does not add up to a measure");
		}
	}
}


/* Save value of direction parameter, like noteinputdir */

void
assign_direction(param, value, mainll_p)

int param;
int value;
struct MAINLL *mainll_p;

{
	char *varname;		/* user's name for the value */

	varname = parm_name(param);
	if (contextcheck(C_SCORE | C_STAFF | C_VOICE, varname) == NO) {
		return;
	}
	if (mainll_p == 0) {
		return;
	}
	used_check(mainll_p, param, varname);
	mainll_p->u.ssv_p->noteinputdir = value;
	mainll_p->u.ssv_p->used[param] = YES;
}


void
assign_unit(unittype, mainll_p)

int unittype;
struct MAINLL *mainll_p;

{
	if (mainll_p == 0) {
		return;
	}
	if (contextcheck(C_SCORE, "units parameter") == NO) {
		return;
	}

	mainll_p->u.ssv_p->units = unittype;
	Score.units = unittype;
}


/* return YES if given SSV refers to a tablature staff, NO if not.
 * This function is different than the is_tab_staff() function in that
 * this takes an ssv_p and thus can be used on a user's SSV, whereas
 * is_tab_staff takes a staff number and only works on the SSVs in
 * the Staff array. */

static int
is_tablature_staff(ssv_p)

struct SSV *ssv_p;

{
	return (ssv_p->strinfo != (struct STRINGINFO *) 0 ? YES : NO);
}


/* When in staff or voice context, there could be a mixture of tab and non-tab.
 * Some things are errors/warnings only in one or the other case. This function
 * returns YES if at least one of the staffs currently getting an SSV built
 * is a tab staff.
 */

int
has_tab_staff()

{
	struct SVRANGELIST *svr_p;	/* list of current SSV staffs/voices */
	struct RANGELIST *sr_p;		/* list of staffs being built */
	int s;				/* staff number */

	for (svr_p = Svrangelist_p; svr_p != 0; svr_p = svr_p->next) {
		for (sr_p = svr_p->stafflist_p; sr_p != 0; sr_p = sr_p->next) {
			for (s = sr_p->begin; s <= sr_p->end; s++) {
				if (is_tab_staff(s)) {
					return(YES);
				}
			}
		}
	}
	return(NO);
}


/* add information about a string for a tablature staff. This gets put
 * in a malloc-ed strinfo array off the ssv_p struct */

void
add_tab_string_info(letter, accidental, nticks, octave, ssv_p)

int letter;		/* pitch letter 'a' to 'g' */
int accidental;		/* #, &, or \0, others are blocked by parser */
int nticks;		/* how many tick marks, to distinguish multiple
			 * strings with the same pitch/accidental */
int octave;		/* for MIDI and translating to tabnote staff */
struct SSV *ssv_p;	/* add the info to this struct */

{
	int index;	/* which strinfo array element we are filling in */
	int i;


	/* increment number of stafflines. This gets done for real later
	 * by asgn_stafflines(), but we do it here to keep track of how
	 * many structs we have malloc-ed from previous calls to this function
	 * for previous strings */
	(ssv_p->stafflines)++;

	/* first get space. If first one to add, malloc, otherwise realloc */
	if (ssv_p->stafflines == 1) {
		MALLOC(STRINGINFO, ssv_p->strinfo, ssv_p->stafflines);
	}
	else {
		REALLOC(STRINGINFO, ssv_p->strinfo, ssv_p->stafflines);
	}

	/* get the index of the new element we are adding */
	index = ssv_p->stafflines - 1;

	/* fill in the data */
	ssv_p->strinfo[index].letter = (char) letter;
	ssv_p->strinfo[index].accidental = (char) accidental;
	ssv_p->strinfo[index].nticks = (short) nticks;
	ssv_p->strinfo[index].octave
		= (short) (octave == USE_DFLT_OCTAVE ? TABDEFOCT : octave);

	/* check for duplicate strings */
	for (i = 0; i < index; i++) {
		if (ssv_p->strinfo[i].letter == letter
				&& ssv_p->strinfo[i].accidental == accidental
				&& ssv_p->strinfo[i].nticks == nticks) {
			l_yyerror(Curr_filename, yylineno,
				"duplicate %c%c%sstring, use ' marks to distinguish",
				letter, accidental ? accidental : ' ',
				accidental ? " " : "");
		}
	}
}


/* Save user-specified measure number in given bar struct,
 * after verifying it is valid (that is it > 0).
 */

void
set_mnum(bar_p, mnum)

struct BAR *bar_p;
int mnum;

{
	char *old_reh_string;
	char num_string[16];

	if (mnum < 1) {
		l_yyerror(Curr_filename, yylineno, "mnum must be > 0");
	}
	else if (bar_p->mnum != 0) {
		l_yyerror(Curr_filename, yylineno,
			"mnum cannot be specified more than once per bar");
	}
	else {
		Meas_num = bar_p->mnum = mnum;
		/* If user had already specified " reh mnum" on this bar,
		 * we would already have made a reh_string for it, so we
		 * have to undo that. It would be nicer to delay the call
		 * to set_reh_string till after we've set mnum, but by then
		 * we would have lost the information we needed unless we
		 * added a bunch more code, so even though this approach
		 * isn't ideal, it's easiest. This shouldn't happen very
		 * often anyway. */
		if ( (bar_p->reh_type == REH_MNUM) &&
					(bar_p->reh_string != (char *) 0) ) {
			old_reh_string = bar_p->reh_string;
			(void) sprintf(num_string, "%d", mnum);
			bar_p->reh_string = copy_string(num_string,
					(int) old_reh_string[0],
					(int) old_reh_string[1]);
			FREE(old_reh_string);
		}
	}
}


/* Give error if margin is too wide, If any is negative, use from Score */

void
chkmargin(topmargin, botmargin, leftmargin, rightmargin)

double topmargin;
double botmargin;
double leftmargin;
double rightmargin;

{
	if (topmargin < 0.0) {
		topmargin = Score.topmargin;
	}
	if (botmargin < 0.0) {
		botmargin = Score.botmargin;
	}
	if (leftmargin < 0.0) {
		leftmargin = Score.leftmargin;
	}
	if (rightmargin < 0.0) {
		rightmargin = Score.rightmargin;
	}
	if (((Score.pageheight - topmargin - botmargin) < MIN_USABLE_SPACE)
			|| ((Score.pagewidth - leftmargin - rightmargin)
			< MIN_USABLE_SPACE)) {
		yyerror("page size minus margins is too small");
	}
}


/* Give error message when user tries to set a parameter in a context
 * where that parameter is not valid. */

static void
wrong_context(param_name)

char * param_name;

{
	l_yyerror(Curr_filename, yylineno,
			"%s parameter not valid in %s context",
			param_name, contextname(Context));
}


/* Map a parameter index to its name. Usually this is the same as the name
 * the user knows, but sometimes we add a modifier like "minimum" when
 * there is a single external parameter with multiple values,
 * but multiple internal ones. This is to be used in error messages.
 * Warning: currently this only maps the
 * specific subset of parameters we know are needed for error messages
 * in this file. If you call it with some other parameter, it will silently
 * return "" so that the user will merely not get the specific parameter
 * name in an error message if we forgot one, not a pfatal. */

static char *
parm_name(param)

int param;	/* #define name from ssvused.h */

{
	switch (param) {
	/* list is alphabetical order, so it is easy to see if a given
	 * parameter is handled by this function */
	case A4FREQ:		return("a4freq");
	case ACCTABLE:		return("acctable");
	case ALIGNLABELS:	return("alignlabels");
	case ALIGNPED:		return("alignped");
	case ALIGNRESTS:	return("alignrests");
	case BEAMSHORT:		return("stemshorten for beamed notes");
	case BEAMSLOPE:		return("beamslope");
	case BEGPROSHORT:	return("stemshorten shortening protruding stem begin steps");
	case BOTMARGIN:		return("bottommargin");
	case BRACKETREPEATS:	return("bracketrepeats");
	case CANCELKEY:		return("cancelkey");
	case CARRYACCS:		return("carryaccs");
	case CHORDDIST:		return("chorddist");
	case CHORDTRANSLATION:	return("chordtranslation");
	case CUE:		return("cue");
	case DEFAULTKEYMAP:	return("defaultkeymap");
	case DEFOCT:		return("defoct");
	case DIST:		return("dist");
	case DIVISION:		return("division");
	case DYNDIST:		return("dyndist");
	case ENDINGKEYMAP:	return("endingkeymap");
	case ENDPROSHORT:	return("stemshorten shortening protruding stem end steps");
	case EXTENDLYRICS:	return("extendlyrics");
	case FIRSTPAGE:		return("firstpage");
	case FLIPMARGINS:	return("flipmargins");
	case FONT: 		return("font");
	case FONTFAMILY:	return("fontfamily");
	case GRIDFRET:		return("gridfret");
	case GRIDSATEND:	return("gridsatend");
	case GRIDSCALE:		return("gridscale");
	case GRIDSWHEREUSED:	return("gridswhereused");
	case INDENTRESTART:	return("indentrestart");
	case LABELKEYMAP:	return("labelkeymap");
	case LEFTMARGIN:	return("leftmargin");
	case LEFTSPACE:		return("leftspace");
	case LYRICSALIGN:	return("lyricsalign");
	case LYRICSDIST:	return("lyricsdist");
	case LYRICSFAMILY:	return("lyricsfontfamily");
	case LYRICSFONT:	return("lyricsfont");
	case LYRICSKEYMAP:	return("lyricskeymap");
	case LYRICSSIZE:	return("lyricssize");
	case MAXMEASURES:	return("maxmeasures");
	case MAXPROSHORT:	return("stemshorten maximum protruding stem shortening");
	case MAXSCPAD:		return("maximum scorepad");
	case MAXSCSEP:		return("maximum scoresep");
	case MAXSCORES:		return("maxscores");
	case MEASNUM:		return("measnum");
	case MEASNUMFAMILY:	return("measnumfontfamily");
	case MEASNUMFONT:	return("measnumfont");
	case MEASNUMSIZE:	return("measnumsize");
	case MIDLINESTEMFLOAT:	return("midlinestemfloat");
	case MINALIGNSCALE:	return("minalignscale");
	case MINGRIDHEIGHT:	return("mingridheight");
	case MINSCPAD:		return("minimum scorepad");
	case MINSCSEP:		return("minimum scoresep");
	case MINSTSEP:		return("staffsep");
	case MUSICSCALE:	return("musicscale");
	case NOTEINPUTDIR:	return("noteinputdir");
	case NOTELEFTFAMILY:	return("noteleftfontfamily");
	case NOTELEFTFONT:	return("noteleftfont");
	case NOTELEFTSIZE:	return("noteleftsize");
	case NUMBERMRPT:	return("numbermrpt");
	case NUMBERMULTRPT:	return("numbermultrpt");
	case NUMSTAFF:		return("staffs");
	case ONTHELINE:		return("ontheline");
	case PACKEXP:		return("packexp");
	case PACKFACT:		return("packfact");
	case PAD:		return("pad");
	case PAGEHEIGHT:	return("pageheight");
	case PAGEWIDTH:		return("pagewidth");
	case PANELSPERPAGE:	return("panelsperpage");
	case PRINTKEYMAP:	return("printkeymap");
	case PRINTMULTNUM:	return("printmultnum");
	case REHEARSALKEYMAP:	return("rehearsalkeymap");
	case RELEASE:		return("release");
	case RESTCOMBINE:	return("restcombine");
	case RESTSYMMULT:	return("restsymmult");
	case RIGHTMARGIN:	return("rightmargin");
	case SCALE_FACTOR:	return("scale factor");
	case SIZE:		return("size");
	case SLASHESBETWEEN:	return("slashesbetween");
	case STAFFPAD:		return("staffpad");
	case STAFFSCALE:	return("staffscale");
	case STEMLEN:		return("stemlen");
	case SYLPOSITION:	return("sylposition");
	case TABWHITEBOX:	return("tabwhitebox");
	case TEXTKEYMAP:	return("textkeymap");
	case TOPMARGIN:		return("topmargin");
	case TUNING:		return("tuning");
	case TUPLETSLOPE:	return("tupletslope");
	case USEACCS:		return("useaccs");
	case VISIBLE:		return("visible");
	case WARN:		return("warn");
	case WITHFONT:		return("withfont");
	case WITHFAMILY:	return("withfontfamily");
	case WITHKEYMAP:	return("withkeymap");
	case WITHSIZE:		return("withsize");
	default:		return("");
	}
}


/* function to let other files get to the ADJUST2INCHES macro */

double
adjust2inches(value)

double value;

{
	ADJUST2INCHES(value);
	return(value);
}
