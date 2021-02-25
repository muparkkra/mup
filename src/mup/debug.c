
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


/* This file contains routines for printing info for debugging purposes.
 * Since this is for internal use by Mup developers, the format may be
 * subject to (possibly radical) changes without notice.
 * It basically dumps whatever we think might be useful,
 * in some reasonably human-readable form.
 * The -d option specifies a bitmap of what levels of debugging to turn on.
 */


#include "defines.h"
#include "structs.h"
#include "globals.h"


/* for number-to-English name translations, if we fall off the bottom of
 * a switch without a match, need to return something, so use "unknown" */
static char Unknown[] = "unknown";

/* Since user names can be "unlimited" in length,
 * we allow each name to be up to a max, and truncate after that. */
#define MAX_ACC_NAME_LEN	50

/* static functions */
static void print_ssv P((struct SSV *ssv_p));
static void print_markorder P((struct SSV *ssv_p, int place));
static void print_printdata P((struct PRINTDATA *printdata_p));
static void print_bar P((struct BAR *bar_p));
static void print_chhead P((struct CHORD *ch_p));
static void print_staff P((struct STAFF *staff_p));
static void print_grpsyl P((struct GRPSYL *g_p, char *gstype, int vno));
static void print_slurtolist P((struct NOTE *note_p));
static void pr_stuff P((struct STUFF *stuff_p));
static char *xlate_place P((int place));
static char *xlate_stufftype P((int stuff_type));
static char * xlate_linetype P((int ltype));
static char *xlate_gvalue P((int grpvalue));
static char *xlate_gsize P((int grpsize));
static char *xlate_gcont P((int gcont));
static char *xlate_item P((int item));
static char *xlate_dir P((int dir));
static void print_curve P((struct CURVE *curve_p));
static char *xlate_coordtype P((int coordtype));
static void print_line P((struct LINE *line_p));
static char * xlate_justify P((int justify_type));
static void show_headfoot P((struct BLOCKHEAD *headfoot_p, char *name));
static void print_binop P((struct EXPR_NODE *expr_p, char *op_name));
static void print_unop P((struct EXPR_NODE *expr_p, char *op_name));
static char * tag_name P((float *c_array));
static void show_parse_trees P((struct INPCOORD *inpc_p, char *label));
static void print_expr P((struct EXPR_NODE *expr_p));
static char *accs_as_string P((char *acclist));


/* tell what is in the main linked list. Walk down the list and print
 * things of interest. */

void
print_mainll()

{
	struct MAINLL *mll_p;		/* to walk through list */
	register int i;			/* count number of items in list */
	int s;				/* walk through staffs */


	/* only do this stuff if debug level 128 is on */
	if (debug_on(128) == 0) {
		return;
	}

	debug(128, "\n\t\t==== Contents of main linked list ====\n");

	/* walk down the main linked list, printing things about each struct
	 * in it */
	for (i = 0, mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
	    			mll_p = mll_p->next, i++) {

		debug(128, "\nmainll %d: type %s", i, stype_name(mll_p->str));
		if (mll_p->inputfile != (char *) 0 && mll_p->inputlineno > 0) {
			debug(128, "from file %s inputlineno %d",
				mll_p->inputfile, mll_p->inputlineno);
		}

		/* print info of interest based on struct type */
		switch (mll_p->str) {

		case S_SSV:
			print_ssv(mll_p->u.ssv_p);
			break;

		case S_BAR:
			print_bar(mll_p->u.bar_p);
			break;

		case S_CURVE:
			print_curve(mll_p->u.curve_p);
			break;

		case S_LINE:
			print_line(mll_p->u.line_p);
			break;

		case S_PRHEAD:
			print_printdata(mll_p->u.prhead_p->printdata_p);
			break;

		case S_FEED:
			debug(128, "\tfeed at (%f, %f) with%s pagefeed\n\tnorth %f, south %f, west %f, east %f",
				mll_p->u.feed_p->c[AX], mll_p->u.feed_p->c[AY],
				mll_p->u.feed_p->pagefeed ? "" : "out",
				mll_p->u.feed_p->c[AN], mll_p->u.feed_p->c[AS],
				mll_p->u.feed_p->c[AW], mll_p->u.feed_p->c[AE]);
			debug(128, "\tleftmargin %f rightmargin %f",
				mll_p->u.feed_p->leftmargin,
				mll_p->u.feed_p->rightmargin);
			if (mll_p->u.feed_p->top_p != 0) {
				debug(128, "\ttop block: height %f",
						mll_p->u.feed_p->top_p->height);
				print_printdata(mll_p->u.feed_p->top_p->printdata_p);
			}
			if (mll_p->u.feed_p->bot_p != 0) {
				debug(128, "\tbot block: height %f",
						mll_p->u.feed_p->bot_p->height);
				print_printdata(mll_p->u.feed_p->bot_p->printdata_p);
			}
			/* Note: after placment top2/bot2 are only meaningful
			 * on final FEED when doing gridsatend with more than
			 * one page worth of grids. */
			if (mll_p->u.feed_p->top2_p != 0) {
				debug(128, "\ttop2 block: height %f",
						mll_p->u.feed_p->top2_p->height);
				print_printdata(mll_p->u.feed_p->top2_p->printdata_p);
			}
			if (mll_p->u.feed_p->bot2_p != 0) {
				debug(128, "\tbot2 block: height %f",
						mll_p->u.feed_p->bot2_p->height);
				print_printdata(mll_p->u.feed_p->bot2_p->printdata_p);
			}
			break;

		case S_CLEFSIG:
			debug(128, "\twest at %f, prtimesig %d, hide %d",
				mll_p->u.clefsig_p->wclefsiga,
				mll_p->u.clefsig_p->prtimesig,
				mll_p->u.clefsig_p->hide);
			if (mll_p->u.clefsig_p->bar_p != (struct BAR *) 0) {
				debug(128, " clefsig includes pseudo bar:");
				print_bar(mll_p->u.clefsig_p->bar_p);
			}
			for (s = 1; s <= Score.staffs; s++) {
				debug(128, "  staff %d: prclef %d, %d sharps, %d naturals",
					s, mll_p->u.clefsig_p->prclef[s],
					mll_p->u.clefsig_p->sharps[s],
					mll_p->u.clefsig_p->naturals[s]);
			}
			break;

		case S_CHHEAD:
			print_chhead(mll_p->u.chhead_p->ch_p);
			break;

		case S_STAFF:
			print_staff(mll_p->u.staff_p);
			break;

		case S_BLOCKHEAD:
			debug(128, "height %f, samepage %d\n", mll_p->u.blockhead_p->height, mll_p->u.blockhead_p->samepage);
			print_printdata(mll_p->u.blockhead_p->printdata_p);
			break;
		default:
			break;
		}
	}

	/* Headers and footers aren't really part of the main list,
	 * but if someone wants to see the main list, they might like to
	 * see those too. */
	show_headfoot(&Header, "HEADER");
	show_headfoot(&Header2, "HEADER2");
	show_headfoot(&Footer, "FOOTER");
	show_headfoot(&Footer2, "FOOTER2");
}


/* translate S_* numbers used for the str field of MAINLL struct to names */

char *
stype_name(stype)

int stype;	/* the S_* number to translate */

{
	switch(stype) {
	case S_SSV:
		return("S_SSV");
	case S_CHHEAD:
		return("S_CHHEAD");
	case S_PRHEAD:
		return("S_PRHEAD");
	case S_LINE:
		return("S_LINE");
	case S_CURVE:
		return("S_CURVE");
	case S_BAR:
		return("S_BAR");
	case S_CLEFSIG:
		return("S_CLEFSIG");
	case S_FEED:
		return("S_FEED");
	case S_STAFF:
		return("S_STAFF");
	case S_BLOCKHEAD:
		return("S_BLOCKHEAD");
	default:
		return(Unknown);
	}
}


/* print some useful info from SSV struct */

static void
print_ssv(ssv_p)

struct SSV *ssv_p;		/* which struct to report on */

{
	debug(128, "\tcontext = %s", contextname(ssv_p->context));
	if (ssv_p->context == C_CONTROL) {
		/* control context is a special strange usage of SSV */
		debug(128, "\tsaved_ssv_index = %d\n", ssv_p->saved_ssv_index);
		return;
	}
	debug(128, "\tstaffno = %d", ssv_p->staffno);
	debug(128, "\tvoiceno = %d", ssv_p->voiceno);
	if (ssv_p->strinfo != (struct STRINGINFO *) 0) {
		int i;

		for (i = 0; i < ssv_p->stafflines; i++) {
			debug(128, "\t\ttab string %d: %c%c nticks=%d, oct=%d",
				i, ssv_p->strinfo[i].letter,
				ssv_p->strinfo[i].accidental == '\0'
				? ' ' : ssv_p->strinfo[i].accidental,
				ssv_p->strinfo[i].nticks,
				ssv_p->strinfo[i].octave);
		}
	}
	if (ssv_p->used[ABOVEORDER] == YES) {
		print_markorder(ssv_p, PL_ABOVE);
	}
	if (ssv_p->used[BELOWORDER] == YES) {
		print_markorder(ssv_p, PL_BELOW);
	}
	if (ssv_p->used[BETWEENORDER] == YES) {
		print_markorder(ssv_p, PL_BETWEEN);
	}
	if (ssv_p->used[TIME] == YES) {
		debug(128, "time sig %d/%d vis %d",
			ssv_p->timenum, ssv_p->timeden, ssv_p->timevis);
	}
}

static void
print_markorder(ssv_p, place)

struct SSV *ssv_p;
int place;

{
	int m;

	debug(128, "\tmarkorder %s: ", xlate_place(place));
	for (m = 0; m < NUM_MARK; m++) {
		debug(128, "\t\t%d ", ssv_p->markorder[place][m]);
	}
	debug(128, "\n");
}


/* print useful info from STAFF struct */

static void
print_staff(staff_p)

struct STAFF *staff_p;		/* which to report on */

{
	register int i;		/* index to walk down a list */


	debug(128, "\tstaffno = %d, visible = %s", staff_p->staffno,
					staff_p->visible == YES ? "y" : "n");
	debug(128, "\t\tmrptnum = %d, mult_rpt_measnum = %d",
					staff_p->mrptnum, staff_p->mult_rpt_measnum);

	/* print each group */
	for ( i = 0; i < MAXVOICES; i++) {
		if (staff_p->groups_p[i] != (struct GRPSYL *) 0) {
			print_grpsyl(staff_p->groups_p[i], "GROUP", i + 1);
		}
	}

	/* print each lyrics syllable */
	for (i = 0; i < staff_p->nsyllists; i++) {
		debug(128, "\n\tsylplace = %s",
					xlate_place(staff_p->sylplace[i]));
		print_grpsyl(staff_p->syls_p[i], "SYLLABLE",
					staff_p->syls_p[i]->vno);
	}

	pr_stuff(staff_p->stuff_p);
}


/* print info from a list of GRPSYL structs */

static void
print_grpsyl(g_p, gstype, vno)

struct GRPSYL *g_p;	/* which GRPSYL */
char *gstype;		/* "GROUP" or "SYLLABLE" */
int vno;		/* voice number or verse number */

{
	register int i;		/* index through list */
	char *sylbuff;		/* syllable with all ASCII characters */


	if (debug_on(128) == 0) {
		return;
	}

	(void) fprintf(stderr, "\tgrpsyl = %s, vno = %d\n", gstype, vno);
	if (g_p != 0) {
		(void) fprintf(stderr, "\tinputfile %s, lineno %d\n", g_p->inputfile, g_p->inputlineno);
	}

	/* print info about the stuff in GRPSYL structs */
	for (   ; g_p != (struct GRPSYL *) 0; g_p = g_p->next) {

/* This ifdef has to match rational.h */
#if __STDC_VERSION__ <= 199409L & ! __LP64__
		(void) fprintf(stderr, "\n\t\tbasictime = %d, dots = %d, fulltime = %ld/%ld\n",
#else
		(void) fprintf(stderr, "\n\t\tbasictime = %d, dots = %d, fulltime = %d/%d\n",
#endif
					g_p->basictime, g_p->dots,
					g_p->fulltime.n, g_p->fulltime.d);
		(void) fprintf(stderr, "\t\tis_meas = %d, is_multirest = %d\n",
					g_p->is_meas, g_p->is_multirest);
		(void) fprintf(stderr, "\t\tmeas_rpt_type = %d\n",
					g_p->meas_rpt_type);
		(void) fprintf(stderr, "\t\tc[AN] = %f, c[RN] = %f\n",
					g_p->c[AN], g_p->c[RN]);
		(void) fprintf(stderr, "\t\tc[AY] = %f, c[RY] = %f\n",
					g_p->c[AY], g_p->c[RY]);
		(void) fprintf(stderr, "\t\tc[AS] = %f, c[RS] = %f\n",
					g_p->c[AS], g_p->c[RS]);
		(void) fprintf(stderr, "\t\tc[AW] = %f, c[AX] = %f, c[AE] = %f\n",
					g_p->c[AW], g_p->c[AX], g_p->c[AE]);
		(void) fprintf(stderr, "\t\tc[RW] = %f, c[RX] = %f, c[RE] = %f\n",
					g_p->c[RW], g_p->c[RX], g_p->c[RE]);
		if (g_p->restc != 0) {
			(void) fprintf(stderr, "\t\trestc[AX] = %f, restc[AY] = %f\n",
					g_p->restc[AX], g_p->restc[AY]);
		}
		if (g_p->ho_usage != HO_NONE) {
			(void) fprintf(stderr, "\t\tho_usage = %d", g_p->ho_usage);
			if (g_p->ho_usage == HO_VALUE) {
				(void) fprintf(stderr, ", ho_value = %f",
							g_p->ho_value);
			}
			(void) fprintf(stderr, "\n");
		}

		/* if group, print info about it, including the list of
		 * notes if any */
		if (g_p->grpsyl == GS_GROUP) {

			(void) fprintf(stderr, "\t\tclef = %d, xdotr = %f, stemx = %f\n",
				g_p->clef, g_p->xdotr, g_p->stemx);
			(void) fprintf(stderr, "\t\tgrpvalue = %s, grpsize = %s, headshape = %d,\n\t\tgrpcont = %s, beamloc = %s, beamslope=%f\n",
				xlate_gvalue(g_p->grpvalue),
				xlate_gsize(g_p->grpsize),
				g_p->headshape,
				xlate_gcont(g_p->grpcont),
				xlate_item(g_p->beamloc),
				g_p->beamslope);
			if (g_p->roll != NOITEM) {
				(void) fprintf(stderr, "\t\troll = %s, rolldir = %s\n",
				xlate_item(g_p->roll),
				xlate_dir(g_p->rolldir));
			}

			if( g_p->padding != 0.0) {
				(void) fprintf(stderr, "\t\tpadding=%f\n", g_p->padding);
			}
			(void) fprintf(stderr, "\t\tautobeam=%d, uncompressible=%d\n",
				g_p->autobeam, g_p->uncompressible);

			(void) fprintf(stderr, "\t\tstemlen=%f, stemdir=%s\n",
				g_p->stemlen, xlate_dir(g_p->stemdir));
			(void) fprintf(stderr, "\t\ttie=%d, slash_alt=%d\n",
				g_p->tie, g_p->slash_alt);

			/* if part of tuplet, print info about that */
			if (g_p->tuploc != NOITEM) {
				(void) fprintf(stderr, "\t\ttuploc=%s, tupcont=%d\n",
					xlate_item(g_p->tuploc), g_p->tupcont);
				(void) fprintf(stderr, "\t\tprinttup=%d, tupside=%s, tupextend=%f\n",
					g_p->printtup, xlate_place(g_p->tupside),
					g_p->tupextend);
				if (g_p->tupletslope != NOTUPLETANGLE) {
					(void) fprintf(stderr, "\t\ttupletslope=%f\n", g_p->tupletslope);
				}
			}

			/* print a bit about "with" lists */
			if (g_p->nwith) {
				int w;
				(void) fprintf(stderr, "\t\t%d items in 'with' list:\n",
						g_p->nwith);
				for (w = 0; w < g_p->nwith; w++) {
					(void) fprintf(stderr, "\t\t\t\"%s\" %s\n",
						ascii_str(g_p->withlist[w].string,
						YES, NO, TM_NONE), xlate_place(g_p->withlist[w].place));
				}
			}

			(void) fprintf(stderr, "\t\tnnotes = %d, beamto = %d, stemto = %d, stemto_idx = %d\n",
					g_p->nnotes, g_p->beamto,
					g_p->stemto, g_p->stemto_idx);
			for (i = 0; i < g_p->nnotes; i++) {
				if (is_tab_staff(g_p->staffno) == YES) {
					(void) fprintf(stderr, "\t\t\tstring %d, fret %d, bend %d %d/%d, fret_paren %d\n",
						g_p->notelist[i].STRINGNO,
						g_p->notelist[i].FRETNO,
						BENDINT(g_p->notelist[i]),
						BENDNUM(g_p->notelist[i]),
						BENDDEN(g_p->notelist[i]),
						g_p->notelist[i].FRET_HAS_PAREN);
					continue;
				}
				(void) fprintf(stderr, "\t\t\t%c%s %d steps %d",
					g_p->notelist[i].letter,
					accs_as_string(g_p->notelist[i].acclist),
					g_p->notelist[i].octave,
					g_p->notelist[i].stepsup);
				if (g_p->notelist[i].note_has_paren == YES) {
					(void) fprintf(stderr, " note_paren (%f, %f)",
						g_p->notelist[i].wlparen,
						g_p->notelist[i].erparen);
				}
				if (g_p->notelist[i].acc_has_paren == YES) {
					(void) fprintf(stderr, " acc_paren");
				}
				if (g_p->notelist[i].tie == YES) {
					(void) fprintf(stderr, " tie (style %d, dir %d, to_voice %d)",
						g_p->notelist[i].tiestyle,
						g_p->notelist[i].tiedir,
						g_p->notelist[i].tied_to_voice);
				}

				(void) fprintf(stderr, "   (headshape %d, headchar %d, headfont %d)",
					g_p->notelist[i].headshape,
					g_p->notelist[i].headchar,
					g_p->notelist[i].headfont);
				if (g_p->notelist[i].notesize != GS_NORMAL) {
					(void) fprintf(stderr, " (size %s)",
						xlate_gsize(g_p->notelist[i]
						.notesize));
				}
				if (g_p->notelist[i].is_bend == YES) {
					fprintf(stderr, " is_bend");
				}
				if (g_p->notelist[i].smallbend == YES) {
					fprintf(stderr, " smallbend");
				}
				if (g_p->notelist[i].tied_from_other == YES) {
					fprintf(stderr, " tied_from_other");
				}
				if (g_p->notelist[i].slurred_from_other == YES) {
					fprintf(stderr, " slurred_from_other");
				}
				print_slurtolist( &(g_p->notelist[i]) );
				(void) fprintf(stderr, "\n");
			}
		}


		/* if syllable, print it */
		if (g_p->syl != (char *) 0) {
			sylbuff = ascii_str(g_p->syl, YES, NO, TM_NONE);
			(void) fprintf(stderr, "\t\tsyllable = '%s', font %d, size %d\n",
				sylbuff, g_p->syl[0], g_p->syl[1]);
		}
	}
}


/* print any slurtolist entries */

static void
print_slurtolist(note_p)

struct NOTE *note_p;

{
	register int n;

	if ( (note_p->nslurto == 0) || (debug_on(128) == 0) ) {
		return;
	}

	(void) fprintf(stderr, "    slurred to:");
	for (n = note_p->nslurto - 1; n >= 0; n--) {
		switch (note_p->slurtolist[n].octave) {
		case IN_UPWARD:
			(void) fprintf(stderr, " IN_UPWARD");
			break;
		case IN_DOWNWARD:
			(void) fprintf(stderr, " IN_DOWNWARD");
			break;
		case OUT_UPWARD:
			(void) fprintf(stderr, " OUT_UPWARD");
			break;
		case OUT_DOWNWARD:
			(void) fprintf(stderr, " OUT_DOWNWARD");
			break;
		default:
			(void) fprintf(stderr, " %c%d (to_voice %d)",
					note_p->slurtolist[n].letter,
					note_p->slurtolist[n].octave,
					note_p->slurtolist[n].slurred_to_voice);
		}
	}
}


/* given a GV_* value, return its English name */

static char *
xlate_gvalue(grpvalue)

int grpvalue;

{
	switch (grpvalue) {
	case GV_NORMAL:
		return("NORMAL");
	case GV_ZERO:
		return("ZERO");
	default:
		return(Unknown);
	}
}


/* given a GS_* value, return its English name */

static char *
xlate_gsize(grpsize)

int grpsize;

{
	switch(grpsize) {
	case GS_NORMAL:
		return("NORMAL");
	case GS_SMALL:
		return("SMALL");
	case GS_TINY:
		return("TINY");
	default:
		return(Unknown);
	}
}


/* given a GC_* grpcont value, return its English name */

static char *
xlate_gcont(grpcont)

int grpcont;

{
	switch(grpcont) {
	case GC_NOTES:
		return("NOTES");
	case GC_REST:
		return("REST");
	case GC_SPACE:
		return("SPACE");
	default:
		return(Unknown);
	}
}


/* given an "ITEM" value, return its English name */

static char *
xlate_item(item)

int item;

{
	switch(item) {
	case NOITEM:
		return("NOITEM");
	case INITEM:
		return("INITEM");
	case STARTITEM:
		return("STARTITEM");
	case ENDITEM:
		return("ENDITEM");
	case LONEITEM:
		return("LONEITEM");
	default:
		return(Unknown);
	}
}


/* translate direction to name */

static char *
xlate_dir(dir)

int dir;

{
	switch(dir) {
	case UP:
		return("UP");
	case DOWN:
		return("DOWN");
	default:
		return("UNKNOWN");
	}
}


/* recursively print info from a list of PRINTDATA structs */

static void
print_printdata(printdata_p)

struct PRINTDATA *printdata_p;

{
	char *buff;	/* for all-ASCII version. */


	if (printdata_p == (struct PRINTDATA *) 0) {
		/* we're at the end of the line... */
		return;
	}

	buff = ascii_str(printdata_p->string, YES, NO, TM_NONE);
	debug(128, "\tprint (isPostScript %d, x %f, y %f, justify %s, width %.2f, font %d, size %d)\n\t\t'%s'",
		printdata_p->isPostScript,
		printdata_p->location.hor, printdata_p->location.vert,
		xlate_justify(printdata_p->justifytype),
		printdata_p->width, (int) printdata_p->string[0],
		(int) printdata_p->string[1], buff);
	show_parse_trees( &(printdata_p->location), "");

	/* recurse down the list */
	print_printdata(printdata_p->next);
}


/* print useful info from a bar struct */

static void
print_bar(bar_p)

struct BAR *bar_p;

{
	char *type;
	struct TIMEDSSV *tssv_p;	/* list of mid-meas param changes */

	switch(bar_p->bartype) {
	case INVISBAR:
		type = "INVISBAR";
		break;
	case SINGLEBAR:
		type = "SINGLEBAR";
		break;
	case DOUBLEBAR:
		type = "DOUBLEBAR";
		break;
	case REPEATSTART:
		type = "REPEATSTART";
		break;
	case REPEATEND:
		type = "REPEATEND";
		break;
	case REPEATBOTH:
		type = "REPEATBOTH";
		break;
	case ENDBAR:
		type = "ENDBAR";
		break;
	case RESTART:
		type = "RESTART";
		break;
	default:
		type = Unknown;
		break;
	}

	debug(128, "\tbartype = %d (%s), endingloc=%d, samescore=%d, samepage=%d",
				bar_p->bartype, type,
				bar_p->endingloc, bar_p->samescore,
				bar_p->samepage);
	debug(128, "\tx = %f, y = %f, mnum = %d",  bar_p->c[AX], bar_p->c[AY],
				bar_p->mnum);
	if (bar_p->reh_string != 0) {
		debug(128, "\treh_string = '%s'",
				ascii_str(bar_p->reh_string, YES, NO, TM_NONE));
	}

	for (tssv_p = bar_p->timedssv_p; tssv_p != 0; tssv_p = tssv_p->next) {
		debug(128, "\tTimed SSV, time_off %d/%d\n", tssv_p->time_off.n,
						tssv_p->time_off.d);
		print_ssv(&tssv_p->ssv);
	}
}


/* recursively print coord info about chords in chord list */

static void
print_chhead(ch_p)

struct CHORD *ch_p;

{
	struct GRPSYL * gs_p;


	if (ch_p != (struct CHORD *) 0) {
		debug(128, "\tchord at (%f, %f), width %f, fullwidth %f",
			ch_p->c[AX], ch_p->c[AY], ch_p->width, ch_p->fullwidth);
		debug(128, "\tc[RW] = %f, c[RE] = %f", ch_p->c[RW], ch_p->c[RE]);
		debug(128, "\tstarttime %d/%d, duration %d/%d, pseudodur %f",
			ch_p->starttime.n, ch_p->starttime.d,
			ch_p->duration.n, ch_p->duration.d,
			ch_p->pseudodur);
		for (gs_p = ch_p->gs_p; gs_p != (struct GRPSYL *) 0;
							gs_p = gs_p->gs_p) {
			debug(128, "\t\t%s, staff %d, vno %d",
					gs_p->grpsyl == GS_GROUP ? "GROUP"
					: "SYLLABLE", gs_p->staffno,
					gs_p->vno);
		}
		print_chhead(ch_p->ch_p);
	}
}


/* print information about a STUFF list */

static void
pr_stuff(stuff_p)

struct STUFF *stuff_p;

{
	char *buff;	/* for all-ASCII version */


	for (   ; stuff_p != (struct STUFF *) 0; stuff_p = stuff_p->next) {

		debug(128, "\nSTUFF: %s %s %s: start=%f (%d) [%f], end=%dm+%f(%d)[%f], all=%d, x=%f, y=%f",
			xlate_stufftype(stuff_p->stuff_type),
			(stuff_p->stuff_type == ST_PHRASE
			  ? xlate_linetype(stuff_p->modifier)
			  : stuff_modifier(stuff_p->modifier)),
			xlate_place(stuff_p->place), stuff_p->start.count,
			stuff_p->start.gracebackup, stuff_p->start.steps,
			stuff_p->end.bars, stuff_p->end.count,
			stuff_p->end.gracebackup,
			stuff_p->end.steps, stuff_p->all,
			stuff_p->c[AX], stuff_p->c[AY]);

		if (stuff_p->aligntag != NOALIGNTAG) {
			debug(128, "\taligntag = %d\n", stuff_p->aligntag);
		}

		if (stuff_p->string != (char *) 0) {
			buff = ascii_str(stuff_p->string, YES, NO, stuff_p->modifier);
			debug(128, "\tstring = \"%s\"", buff);
		}
	}
}


/* given a PL_* place value, return its English name */

static char *
xlate_place(place)

int place;

{
	switch(place) {
	case PL_ABOVE:
		return("above");
	case PL_BELOW:
		return("below");
	case PL_BETWEEN:
		return("between");
	case PL_UNKNOWN:
		return("unknown");
	default:
		return("invalid place");
	}
}


/* given a ST_* stufftype, return its English name */


static char *
xlate_stufftype(stuff_type)

int stuff_type;

{
	switch (stuff_type) {
	case ST_ROM:
		return("rom");
	case ST_BOLD:
		return("bold");
	case ST_ITAL:
		return("ital");
	case ST_BOLDITAL:
		return("boldital");
	case ST_CRESC:
		return("cresc");
	case ST_DECRESC:
		return("decresc");
	case ST_MUSSYM:
		return("mussym");
	case ST_PHRASE:
		return("phrase");
	case ST_PEDAL:
		return("pedal");
	case ST_OCTAVE:
		return("octave");
	case ST_MIDI:
		return("MIDI");
	case ST_TIESLUR:
		return("TIESLUR");
	case ST_TABSLUR:
		return("TABSLUR");
	case ST_BEND:
		return("BEND");
	default:
		return("unknown stuff_type");
	}
}


/* Translate L_ line type to name */

static char *
xlate_linetype(ltype)

int ltype;	/* L_* value */

{
	switch(ltype) {
	case L_NORMAL:
		return("");
	case L_MEDIUM:
		return("medium");
	case L_WIDE:
		return("wide");
	case L_DOTTED:
		return("dotted");
	case L_DASHED:
		return("dashed");
	case L_WAVY:
		return("wavy");
	default:
		return("unknown");
	}
}


static void
print_curve(curve_p)

struct CURVE *curve_p;

{
	struct COORD_INFO *cinfo_p;
	int n;

	if (debug_on(128) == 0) {
		return;
	}

	for (n = 0; n < curve_p->ncoord; n++) {
		debug(128, "\thor = %f", curve_p->coordlist[n].hor);
		if ((cinfo_p = find_coord(curve_p->coordlist[n].hor_p)) != 0) {
			debug(128, "\t\tpage = %d, score = %d",
				cinfo_p->page, cinfo_p->scorenum);
			debug(128, "\t\thexpr_p parse tree:");
			print_expr(curve_p->coordlist[n].hexpr_p);
		}

		debug(128, "\tvert = %f", curve_p->coordlist[n].vert);
		if ((cinfo_p = find_coord(curve_p->coordlist[n].vert_p)) != 0) {
			debug(128, "\t\tpage = %d, score = %d",
				cinfo_p->page, cinfo_p->scorenum);
			debug(128, "\t\tvexpr_p parse tree:");
			print_expr(curve_p->coordlist[n].vexpr_p);
		}
	}
}



static void
print_line(line_p)

struct LINE *line_p;

{
	debug(128, "\t%s (%f, %f) to (%f, %f)\n",
			xlate_linetype(line_p->linetype),
			line_p->start.hor, line_p->start.vert,
			line_p->end.hor, line_p->end.vert);
	show_parse_trees( &(line_p->start), " (start)");
	show_parse_trees( &(line_p->end), " (end)");
}


/* given an absolute coordinate type like AX, return its name */

static char
*xlate_coordtype(coordtype)

int coordtype;

{
	switch(coordtype) {
	case AX:
		return("AX");
	case AY:
		return("AY");
	case AN:
		return("AN");
	case AS:
		return("AS");
	case AE:
		return("AE");
	case AW:
		return("AW");
	default:
		return(Unknown);
	}
}


/* Translate justify to to a human readable form */

static char *
xlate_justify(justify_type)

int justify_type;

{
	switch (justify_type) {
	case J_LEFT:
		return("left");
	case J_RIGHT:
		return("right");
	case J_CENTER:
		return("center");
	case J_NONE:
		return("none");
	case J_RAGPARA:
		return("ragged paragraph");
	case J_JUSTPARA:
		return("justified paragraph");
	default:
		return(Unknown);
	}
}



/* Print information about what is in a header/footer */

static void
show_headfoot(headfoot_p, name)

struct BLOCKHEAD *headfoot_p;
char *name;

{
	if (headfoot_p->printdata_p != 0) {
		debug(128, "\n%s: height %f", name, headfoot_p->height);
		print_printdata(headfoot_p->printdata_p);
	}
}


/* Helper function for printing an expression. This prints the information
 * about a binary operator node. */

static void
print_binop(expr_p, op_name)

struct EXPR_NODE *expr_p;
char *op_name;

{
	print_expr(expr_p->left.lchild_p);
	print_expr(expr_p->right.rchild_p);
	fprintf(stderr, "\t\t\tOP_%s\n", op_name);
}


/* Helper function for printing an expression. This prints the information
 * about a unary operator node. */

static void
print_unop(expr_p, op_name)

struct EXPR_NODE *expr_p;
char *op_name;

{
	print_expr(expr_p->left.lchild_p);
	fprintf(stderr, "\t\t\tOP_%s\n", op_name);
}


/* Helper function for printing an expression. For those few tags that we
 * can figure out what their names are, return that name, end null string. */

static char *
tag_name(c_array)

float *c_array;

{
	if (c_array == Header.c) {
		return(" (Header)");
	}
	if (c_array == Footer.c) {
		return(" (Footer)");
	}
	if (c_array == Header2.c) {
		return(" (Header2)");
	}
	if (c_array == Footer2.c) {
		return(" (Footer2)");
	}
	if (c_array == _Page) {
		return(" (Page)");
	}
	if (c_array == _Cur) {
		return(" (Cur)");
	}
	if (c_array == _Win) {
		return(" (Win)");
	}
	return("");
}


/* Print the horizontal and vertical expression parse trees of an INPCOORD */

static void
show_parse_trees(inpc_p, label)

struct INPCOORD *inpc_p;	/* which thing to print info about */
char * label;			/* optional string to describe the coord */

{
	if (inpc_p->hexpr_p != 0) {
		fprintf(stderr, "\tparse tree of hexpr%s:\n", label);
		print_expr(inpc_p->hexpr_p);
	}
	if (inpc_p->vexpr_p != 0) {
		fprintf(stderr, "\tparse tree of vexpr%s:\n", label);
		print_expr(inpc_p->vexpr_p);
	}
}


/* Print the parse tree of an expression, by recursively walking the tree. */

static void
print_expr(expr_p)

struct EXPR_NODE * expr_p;	/* The expression to show */

{
	if (expr_p == 0) {
		/* end of recursion */
		return;
	}

	switch (expr_p->op) {
	case OP_ADD:
		print_binop(expr_p, "ADD");
		break;
	case OP_SUB:
		print_binop(expr_p, "SUB");
		break;
	case OP_MUL:
		print_binop(expr_p, "MUL");
		break;
	case OP_DIV:
		print_binop(expr_p, "DIV");
		break;
	case OP_MOD:
		print_binop(expr_p, "MOD");
		break;
	case OP_ATAN2:
		print_binop(expr_p, "ATAN2");
		break;
	case OP_HYPOT:
		print_binop(expr_p, "HYPOT");
		break;
	case OP_SQRT:
		print_unop(expr_p, "SQRT");
		break;
	case OP_SIN:
		print_unop(expr_p, "SIN");
		break;
	case OP_COS:
		print_unop(expr_p, "COS");
		break;
	case OP_TAN:
		print_unop(expr_p, "TAN");
		break;
	case OP_ASIN:
		print_unop(expr_p, "ASIN");
		break;
	case OP_ACOS:
		print_unop(expr_p, "ACOS");
		break;
	case OP_ATAN:
		print_unop(expr_p, "ATAN");
		break;
	case OP_FLOAT_LITERAL:
		fprintf(stderr, "\t\t\t%f\n", expr_p->left.value);
		break;
	case OP_TAG_REF:
		fprintf(stderr, "\t\t\tc[%s] where c is at %p%s\n",
			xlate_coordtype(expr_p->left.ltag_p->c_index),
			expr_p->left.ltag_p->c,
			tag_name(expr_p->left.ltag_p->c));
		break;
	case OP_TIME_OFFSET:
		fprintf(stderr, "\t\t\t%f (with INCHPERWHOLE of %f)\n",
			expr_p->left.value,
			expr_p->right.rtag_p->c[INCHPERWHOLE]);
		break;
	default:
		pfatal("Unknown expr op %d\n", expr_p->op);
	}
}


/* Returns string representation of a list of accidentals, from static
 * area overwritten on each call. */

static char *
accs_as_string(acclist)

char *acclist;

{
	/* Since user names can be "unlimited" in length,
	 * we allow each name to be up to a max. Also allow one char after
	 * each, for a space or the null. */
	static char result[MAX_ACCS * (MAX_ACC_NAME_LEN+1)];
	int a;		/* index through acclist */
	int i;		/* index through result */
	char *name;	/* name of symbol */
	int len;	/* length of name */


	for (i = a = 0; a < MAX_ACCS * 2; a += 2) {

		if (acclist[a] == 0) {
			/* end of list */
			break;
		}

		/* Put blank between each */
		if (i > 0) {
			result[i++] = ' ';
		}

		if (acclist[a] == FONT_MUSIC) {
			switch (acclist[a+1]) {
			case C_SHARP:
				result[i++] = '#';
				continue;
			case C_FLAT:
				result[i++] = '&';
				continue;
			case C_DBLSHARP:
				result[i++] = 'x';
				continue;
			case C_DBLFLAT:
				result[i++] = '&';
				result[i++] = '&';
				continue;
			case C_NAT:
				result[i++] = 'n';
				continue;
			default:
				break;
			}
		}

		name = get_charname(acclist[a+1], acclist[a]);
		/* Normally would do a strncpy here, but some compilers warn
		 * that the len is based on the source size rather than the
		 * destination size. While that is true, is it done in a way
		 * that is safe. The author of the warning even agrees
		 * there are cases that are actually safe, but difficult
		 * for the warning code to realize they are safe.
		 * So use memcpy to avoid the warning.
		 */
		len = strlen(name);
		if (len > MAX_ACC_NAME_LEN) {
			len = MAX_ACC_NAME_LEN;
		}
		memcpy(result + i, name, len);
		i += len;
	}
	result[i] = '\0';
	return(result);
}


#ifdef MUP_ALLOC_DEBUG

/* This is code to help dubug cases where malloc's data structures get
 * compromised by things like buffer overflows. You should only
 * compile this in if you actually need to debug such an issue.
 * To debug, in addition to compiling this in, you need to create a file
 * named allocdebug, and make it big enough to hold the data for your test case,
 * typically by using dd if=/dev/zero of=allocdebug with appropriate
 * bs and count arguments. (A file size of 500K or so might suffice for
 * a fairly short and simple Mup file, so you could start there and then
 * make it bigger if that runs out of slots.)
 * If this code sees that file, it will mmap it in,
 * and save data about every malloc and free done via the MALLOC and similar
 * macros. An offline program (smadi) can then render that information human
 * readable, to help gives clues on where the memory corruption
 * might be happening.
 *
 * It is also possible to set environment variable MUP_FREE_SKIP to a
 * number, or a range (a pair of numbers, comma or dash separated) to make Mup
 * skip actually freeing certain calls to free().
 * The idea is that if skipping the Nth free causes a problem to go away,
 * it could be that some code is still using that space after it was
 * supposedly freed. But there could be lots of other reasons for similar
 * symptoms, so it may or may not provide useful clues in a particular case.
 *
 * This code has to not malloc/free any memory itself for its own purposes,
 * to avoid recursion or affecting the results.
 */

#include <sys/stat.h>
#include <sys/mman.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <errno.h>
#include "allocdebug.h"

/* where the data is stored; points at memory-mapped allocdebug file */
static struct ALLOC_INFO * Alloc_info;
/* current slot in Alloc_info */
static int Alloc_index = 0;
/* how many mallocs and frees so far */
static int Alloc_count = 1;
static int Free_count = 1;
/* number of slots available in the mem-mapped file */
static int Alloc_slots = 0;
/* flag to remember if we've initialized things yet */
static int Alloc_init_done = 0;
/* start/end for $MUP_FREE_SKIP */
static int Free_skip_start = -1;
static int Free_skip_end = -1;

/* Function to set start/end value requested by an environment variable.
 * So for something like MUP_FREE_SKIP=20,30 it would return 20 and 30
 * via the pointers, or for MUP_FREE_SKIP=5 it would return 5 for both. */

static void
get_skip(varname, start_p, end_p)

char *varname;  /* name of the environment variable, e.g., MUP_FREE_SKIP */
int *start_p;	/* start is returned via this pointer */
int *end_p;	/* end is returned via this pointer */

{
	char *skip;
	char *delim;

	if ((skip = getenv(varname)) != 0) {
		*start_p = atoi(skip);
		if ((delim = strpbrk(skip, ",-")) != 0) {
			*end_p = atoi(delim+1);
		}
		else {
			*end_p = *start_p;
		}
	}
}

/* This gets called on the first malloc or free, to memory-map in the
 * place to store data. */

void
alloc_debug_init()
{
	int file;
	struct stat info;


	if (Alloc_init_done > 0) {
		return;
	}

	if ((file = open("allocdebug", O_RDWR, 0660)) < 0) {
		return;
	}
	if( fstat(file, &info) != 0) {
		fprintf(stderr, "cannot stat allocdebug file\n");
		exit(1);
	}
	Alloc_info = mmap(0, info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
	if (Alloc_info == (void*)-1) {
		fprintf(stderr, "cannot map allocdebug file, error %s\n",
						strerror(errno));
		exit(1);
	}
	memset(Alloc_info, 0, info.st_size);
	Alloc_slots = info.st_size / sizeof(struct ALLOC_INFO);
	Alloc_init_done = 1;

	get_skip("MUP_FREE_SKIP", &Free_skip_start, &Free_skip_end);
}

/* This returns where the malloc/free was called from. It relies on
 * a gcc extension, so probably won't work with other compilers.
 * A post processor could potentially do symbol table lookup to get the actual
 * function, or user can figure it out by grepping for the first part
 * of the address in nm output.
 */

static void *
get_call_address()
{
	return(__builtin_return_address(0));
}

/* This should be called on return from  malloc/calloc/realloc,
 * to fill in the return value, and increment the index for the next item. */

void
alloc_debug_ret_addr(ret_addr)

void *ret_addr;

{
	if (Alloc_info != 0) {
		Alloc_info[Alloc_index].ret_addr = ret_addr;
		Alloc_count++;
		Alloc_index++;
	}
}

/* This should be called before a call to malloc/calloc/realloc/free to
 * save away the call info. */

void
alloc_debug(ai_type, size, num_elem, arg_addr, type, newp)

short ai_type;		/* AI_* */
size_t size;		/* arg to alloc/calloc/realloc */
size_t num_elem;	/* arg to calloc */
void *arg_addr;		/* arg to realloc/free */
char *type;		/* *LLOC* macro type field */
char *newp;		/* *LLOC* macro new_p field */

{
	if (Alloc_init_done == 0) {
		alloc_debug_init();
	}
	if (Alloc_info == 0) {
		return;
	}
	if (Alloc_index >= Alloc_slots) {
		pfatal("ran out of allocdebug slots");
	}
	Alloc_info[Alloc_index].ai_type = ai_type;
	Alloc_info[Alloc_index].size = size;
	Alloc_info[Alloc_index].num_elem = num_elem;
	Alloc_info[Alloc_index].arg_addr = arg_addr;
	if (type != 0) {
		strncpy(Alloc_info[Alloc_index].type, type, CSIZE-1);
	}
	if (newp != 0) {
		strncpy(Alloc_info[Alloc_index].newp, newp, CSIZE-1);
	}
	Alloc_info[Alloc_index].call_addr = get_call_address();
	if (ai_type == AI_FREE || ai_type == AI_FREE_SKIPPED) {
		Alloc_index++;
	}
}

/* This should be called from user code instead of the real free.
 * It saves the appropriate info, and does the real free unless
 * MUP_FREE_SKIP value says not to.
 */

void
free_debug(void *addr)
{
	Free_count++;
	if (Free_count >=  Free_skip_start && Free_count <= Free_skip_end) {
		alloc_debug(AI_FREE_SKIPPED, 0, Free_count, addr, 0, 0);
		return;
	}
	alloc_debug(AI_FREE, 0, Free_count, addr, 0, 0);
	free(addr);
}
#endif
