
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

/* This file contains functions for printing things off of STAFF structs:
 * notes, stems, rests, flags, beams, etc.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"


/* This struct is used to build up a mesh that represents cross staff beams.
 * This is used to figure out how far from the stem end to offset
 * the end of a beam.
 * There are a row of these linked horizontally via "next" for each beam.
 * The stems are linked vertically via the above_p and below_p pointers.
 * To get the stem offset for a given beam,
 * the code finds the desired basictime on the appropriate stem,
 * and counts how many beams that is from the end of the stem.
 */
struct CSBINFO {
	struct CSBINFO *next;		/* for next group in same beam */
	struct CSBINFO *above_p;	/* beams above this beam */
	struct CSBINFO *below_p;	/* beams below this beam */
	struct GRPSYL *gs_p;		/* group this info is associated with.
					 * This is only used on the 8th beam,
					 * and is just for convenience,
					 * to save us from having to figure
					 * it out again later.
					 */
	int basictime;			/* 8, 16, 32, etc represented by beam */
};

/* static functions */
static void do_syl_joins P((char *syl, double west, double y));
static void pr_stuff P((struct STUFF *stufflist_p, int staffno,
		struct MAINLL *mll_p));
static int pr_grid P((struct STUFF *stuff_p, int staffnum));
static void pr_tieslur P((struct STUFF *stuff_p, struct MAINLL *mll_p,
		int staffno));
static int get_ts_style P((struct STUFF *stuff_p, struct MAINLL *mll_p));
static void pr_rest P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
static int is_ledgerless P((int restfont, int restcode));
static double mr_y_loc P((int staffno));
static void pr_note_dots P((struct NOTE *noteinfo_p, int numdots,
		double xdotr, double group_x, double group_y, int size));
static void pr_parens P((struct NOTE *note_p, struct GRPSYL *gs_p));
static void pr_stems P((struct GRPSYL *grpsyl_p));
static void pr_withside P((struct GRPSYL *gs_p, int side));
static void pr_withitems P((struct GRPSYL *gs_p, int side,
			double x, double y, double sign));
static void pr_flags P((struct GRPSYL *grpsyl_p, double x, double y));
static void pr_accidental P((struct NOTE *noteinfo_p, struct GRPSYL *grpsyl_p));
static void pr_leger P((struct NOTE *noteinfo_p, struct GRPSYL *gs_p,
		int staffno));
static int numlegers P((struct NOTE *noteinfo_p));
static double leger_length P((struct NOTE *noteinfo_p, struct GRPSYL *othergs_p,
		int lines, int other_is_prev, int is_intermediate));
static void pr_tupnums P((struct GRPSYL *gs_p, struct STAFF *staff_p));
static void pr_beams P((struct GRPSYL *grpsyl_p, int grpvalue, int grpsize));
static struct CSBINFO *mkcsbmesh P((struct GRPSYL *begin_p,
		struct GRPSYL *end_p));
static int draw_beams P((struct GRPSYL *gs_p, struct GRPSYL *endbeam_p,
		int basictime, int grpsize, int grpvalue));
static struct GRPSYL *neighboring_note_beam_group P((struct GRPSYL *gs_p,
		struct GRPSYL *first_p, int backwards, int rests_too) );
static struct GRPSYL *comm_next_beam_group P((struct GRPSYL *gs_p,
		struct GRPSYL *first_p, struct GRPSYL *endnext_p,
		int rests_too));
static struct GRPSYL *comm_prev_beam_group P((struct GRPSYL *gs_p,
		struct GRPSYL *first_p, int rests_too));
static int chkgroupings P((int *side_p, struct GRPSYL *thisgs_p));
static void pr_cresc P((struct STUFF *stuff_p));
static void extend P((struct STUFF *stuff_p));
static int tupdir1voice P((struct GRPSYL *gs_p));
static int mirror P((char *str, int ch, int font));
static double size2flagsep P((int size));


/* print things off of STAFF struct */

void
pr_staff(mll_p)

struct MAINLL *mll_p;	/* which main list struct holds the STAFF struct */

{
	struct STAFF *staff_p;	/* mll_p->u.staff_p */
	struct GRPSYL *grpsyl_p;/* current grpsyl */
	struct MAINLL *barmll_p;/* to find TIMEDSSVs */
	struct TIMEDSSV *tssv_p;/* for mid-measure parameter changes */
	struct TIMEDSSV *t_p;	/* walk through the mid-measure changes */
	RATIONAL now;		/* how far we are into measure */
	char *savedlyr;		/* saved copy of lyric syllable */
	double saved_east, saved_west;	/* to undo lyrics compensation */
	register int n;		/* index thru notes in a group */
	struct NOTE *noteinfo_p;/* current note */
	int otherstaff;		/* staff number for cross-staff stems */
	int v;			/* walk through voices or verses on the staff */
	int size;		/* to use for notes and such */
	int dotsize;		/* we use small dots only if entire group is cue */


	debug(512, "pr_staff file=%s lineno=%d staff=%d", mll_p->inputfile,
			mll_p->inputlineno, mll_p->u.staff_p->staffno);

	staff_p = mll_p->u.staff_p;

	if ( svpath(staff_p->staffno, VISIBLE)->visible == NO) {
		/* invisible staffs are easy to print... */
		return;
	}

	/* do any syllables */
	for (v = 0; v < staff_p->nsyllists; v++) {

		/* if bottom staff of "between" lyric is invisible,
		 * the lyric silently disappears from output */
		if (staff_p->sylplace[v] == PL_BETWEEN &&
				svpath(staff_p->staffno + 1, VISIBLE)->visible
				== NO) {
			continue;
		}

		if (staff_p->syls_p[v] != (struct GRPSYL *) 0 &&
					staff_p->syls_p[v]->inputlineno > 0) {
			/* tell PostScript about user input line reference */
			pr_linenum(staff_p->syls_p[v]->inputfile,
					staff_p->syls_p[v]->inputlineno);
		}

		/* do all syllables for current verse/place */
		for (grpsyl_p = staff_p->syls_p[v];
					grpsyl_p != (struct GRPSYL *) 0;
					grpsyl_p = grpsyl_p->next) {

			if ( grpsyl_p->syl != (char *) 0) {

				/* lyr_compensate may alter AW and AE,
				 * so we need to put them back when we are
				 * done, otherwise, if we do another print
				 * pass due to -o command line option,
				 * the values will be wrong */
				saved_west = grpsyl_p->c[AW];
				saved_east = grpsyl_p->c[AE];
				/* if <...> before or after syllable that
				 * were not used for placement, need to
				 * compensate for that */
				lyr_compensate(grpsyl_p);

				/* Extender printing can alter the lyrics
				 * string to get rid of the extender so it
				 * won't print with the syllable. But if we
				 * are printing pages using -o option we
				 * may need to have the original
				 * string preserved, because we may do this
				 * page again. So make a copy.
				 */
				if ((savedlyr = malloc(strlen(grpsyl_p->syl) + 1))
							== 0) {
					l_no_mem(__FILE__, __LINE__);
				}
				strcpy(savedlyr, grpsyl_p->syl);
				
				/* if syllable ends with a dash or underscore,
				 * they have to be spread between this syllable
				 * and the next */
				(void) spread_extender(grpsyl_p, mll_p,
						grpsyl_p->vno,
						staff_p->sylplace[v], YES);

				/* now print the syllable itself */
				pr_string(grpsyl_p->c[AW], grpsyl_p->c[AY],
						grpsyl_p->syl, J_LEFT,
						grpsyl_p->inputfile,
						grpsyl_p->inputlineno);

				/* handle multiple syllables on one chord */
				do_syl_joins(grpsyl_p->syl,
						(double) grpsyl_p->c[AW],
						(double) grpsyl_p->c[AY]);
				/* if string was altered, put original back */
				if (strcmp(grpsyl_p->syl, savedlyr) != 0) {
					FREE(grpsyl_p->syl);
					grpsyl_p->syl = savedlyr;
				}
				else {
					FREE(savedlyr);
				}
			
				/* undo any altering that lyr_compensate did */
				grpsyl_p->c[AW] = saved_west;
				grpsyl_p->c[AE] = saved_east;
			}
		}
	}

	/* Find the BAR that would point to any TIMEDSSVs for this measure. */
	for (barmll_p = mll_p->next; barmll_p->str != S_BAR; barmll_p = barmll_p->next) {
		;
	}
	t_p = tssv_p = barmll_p->u.bar_p->timedssv_p;

	/* do notes, etc for each voice on the staff */
	for (v = 0; v < MAXVOICES; v++) {

		if (staff_p->groups_p[v] == 0) {
			continue;
		}

		/* tab staff notes are handled differently */
		if (is_tab_staff(staff_p->staffno) == YES) {
			pr_tab_groups(staff_p->groups_p[v], mll_p);
			continue;
		}

		/* Set up to handle mid-measure changes, if any */
		if (tssv_p != 0) {
			setssvstate(mll_p);
		}
		t_p = tssv_p;
		now = Zero;

		/* for each GRPSYL in the list for current voice */
		for ( grpsyl_p = staff_p->groups_p[v];
					grpsyl_p != (struct GRPSYL *) 0;
					grpsyl_p = grpsyl_p->next) {

			/* Apply any timed SSVs */
			while (t_p != 0 && LE(t_p->time_off, now) ) {
				asgnssv(&t_p->ssv);
				t_p = t_p->next;
			}
			now = radd(now, grpsyl_p->fulltime);

			if (grpsyl_p->clef != NOCLEF) {
				float widthclef;
				int clefsize;
				int clefcode;
				int cleffont;

				clefsize = (3 * DFLT_SIZE) / 4;
				clefcode = clefchar(grpsyl_p->clef,
						grpsyl_p->staffno, &cleffont);
				widthclef = width(cleffont, clefsize, clefcode);
				pr_clef(grpsyl_p->staffno,
					grpsyl_p->c[AW] -
					(widthclef + CLEFPAD) * Staffscale,
					YES, clefsize);
			}
			if (grpsyl_p->grpcont == GC_SPACE) {
				/* very easy to print a space -- do nothing! */
				continue;
			}

			if (grpsyl_p->grpcont == GC_REST) {
				pr_rest(grpsyl_p, mll_p);
				pr_withlist(grpsyl_p);
				continue;
			}

			if (is_mrpt(grpsyl_p) == YES) {
				pr_mrpt(grpsyl_p, mll_p);
				continue;
			}

			/* If group has a cross-staff stem,
			 * figure out which is the other staff */
			if (grpsyl_p->stemto == CS_ABOVE) {
				for (otherstaff = grpsyl_p->staffno - 1;
						otherstaff >= 1; otherstaff--) {
					if (svpath(otherstaff, VISIBLE)->visible
							== YES) {
						break;
					}
				}
			}
			else if (grpsyl_p->stemto == CS_BELOW) {
				for (otherstaff = grpsyl_p->staffno + 1;
						otherstaff <= Score.staffs;
						otherstaff++) {
					if (svpath(otherstaff, VISIBLE)->visible
							== YES) {
						break;
					}
				}
			}
			else {
				otherstaff = grpsyl_p->staffno;
			}
			if (otherstaff < 1 || otherstaff > Score.staffs) {
				pfatal("failed to find other score for cross-staff stems for leger lines");
			}

			/* If all notes are cue size, we use cue size dots */
			/* Note that we don't have to be concerned with tiny
			 * here, because only grace can be tiny, and grace
			 * are not allowed to have dots. */
			dotsize = (allsmall(grpsyl_p, grpsyl_p) == YES ?
							SMALLSIZE : DFLT_SIZE);

			/* do each note in the group */
			for (n = 0; n < grpsyl_p->nnotes; n++) {

				size = size_def2font(grpsyl_p->notelist[n].notesize);

				/* we're going to need the NOTE info a lot;
				 * get its address */
				noteinfo_p = &(grpsyl_p->notelist[n]);

				/* do the note head */
				pr_muschar(noteinfo_p->c[AX],
						noteinfo_p->c[AY],
						noteinfo_p->headchar,
						size,
						noteinfo_p->headfont);
			
				/* do any accidental */
				pr_accidental(noteinfo_p, grpsyl_p);

				/* print noteleft string, if any */
				if (noteinfo_p->noteleft_string != 0) {
					pr_string(grpsyl_p->c[AX] + noteinfo_p->wlstring,
						noteinfo_p->c[AY] - Stepsize,
						noteinfo_p->noteleft_string,
						J_LEFT,
						grpsyl_p->inputfile,
						grpsyl_p->inputlineno);
				}

				/* do any dots */
				pr_note_dots(noteinfo_p, grpsyl_p->dots,
						grpsyl_p->xdotr,
						(double) grpsyl_p->c[AX],
						(double) grpsyl_p->c[AY],
						dotsize);

				/* print parentheses around note if any*/
				if (noteinfo_p->note_has_paren == YES) {
					pr_parens(noteinfo_p, grpsyl_p);
				}

				/* print small curve for 1/4 bends */
				if (noteinfo_p->smallbend == YES) {
					float adjust;

					/* may have to move slightly to avoid
					 * flag. This is true if group is an
					 * unbeamed, stem-up group of 8th note
					 * or shorter duration */
					if (grpsyl_p->basictime >= 8 &&
							grpsyl_p->stemdir == UP
							&& grpsyl_p->beamloc
							== NOITEM) {
						adjust = 2.0 * STEPSIZE;
					}
					else {
						adjust = STEPSIZE;
					}
					pr_sm_bend( (double)
						noteinfo_p->c[AE] + adjust,
						(double)
						noteinfo_p->c[AY] + 0.5 * STEPSIZE);
				}

				/* do any leger lines */
				if (grpsyl_p->stemto == CS_SAME ||
						(n >= FNNI(grpsyl_p) &&
						n <= LNNI(grpsyl_p) )) {
					pr_leger(noteinfo_p, grpsyl_p,
							grpsyl_p->staffno);
				}
				else {
					/* notes are on a different staff */
					pr_leger(noteinfo_p, grpsyl_p,
							otherstaff);
				}
			}

			/* do "with" lists */
			pr_withlist(grpsyl_p);

			/* do stems, flags, slash, and alt */
			pr_stems(grpsyl_p);

			/* print rolls */
			if (gets_roll(grpsyl_p, staff_p, v) == YES) {
				print_roll(grpsyl_p);
			}
		}

		/* assign anything that happened after start of last group */
		while (t_p != 0) {
			asgnssv(&t_p->ssv);
			t_p = t_p->next;
		}

		/* print tuplet numbers if any */
		pr_tupnums(staff_p->groups_p[v], staff_p);

		/* draw beams */
		pr_beams(staff_p->groups_p[v], GV_NORMAL, GS_NORMAL);
		pr_beams(staff_p->groups_p[v], GV_ZERO, GS_SMALL);
		pr_beams(staff_p->groups_p[v], GV_NORMAL, GS_SMALL);
		pr_beams(staff_p->groups_p[v], GV_ZERO, GS_TINY);
	}

	/* now do any associated STUFFs */
	pr_stuff(staff_p->stuff_p, staff_p->staffno, mll_p);
}


/* if two syllables are to be joined, draw a little curved line between them */

static void
do_syl_joins (syl, west, y)

char *syl;	/* syllable string */
double west;	/* where syllable was printed */
double y;	/* where syllable was printed */

{
	int font, size;
	char *p;			/* pointer into syllable string */
	float wid;			/* of syllable up to space */
	double x, east;			/* of curved line */
	double xinc, yinc;		/* increment to move when doing curve */
	double spacewid;		/* width of ' ' */


	int skipover = NO;
	
	/* skip past any <...> */
	font = syl[0];
	size = syl[1];
	for (p = syl + 2; *p != '\0'; p++) {
		switch ( (unsigned) *p & 0xff) {
		case STR_PRE:
		case STR_U_PRE:
		case STR_PST:
		case STR_U_PST:
			skipover = YES;
			break;
		case STR_PRE_END:
		case STR_PST_END:
			skipover = NO;
			break;
		case STR_MUS_CHAR:
		case STR_MUS_CHAR2:
		case STR_USERDEF1:
			p += 2;
			break;
		case STR_FONT:
			font = *(p+1);
			/*FALLTHRU*/
		case STR_SIZE:
		case STR_BACKSPACE:
		case STR_PAGENUM:
		case STR_NUMPAGES:
			p++;
			break;
		case STR_UNDER_END:
			p += 3;
			break;
		case ' ':
			if (skipover == NO && IS_STD_FONT(font)) {
				/* temporarily shorten string to just before
				 * the space to get width of string up to
				 * that point */
				*p = '\0';
				wid = strwidth(syl);
				*p = ' ';

				/* Calculate dimensions
				 * and location of curve to be drawn. */
				spacewid = width(font, size, ' ');
				xinc = spacewid * 0.3;
				yinc = spacewid * 0.2;
				x = west + wid;
				east = x + spacewid;

				do_linetype(L_NORMAL);
				y -= yinc;
				do_moveto(x, y);
				do_curveto(x + xinc, y - yinc,
					east - xinc, y - yinc, east, y);
				y += yinc;
			}
			break;
		case STR_KEYMAP:
			/* All STR_KEYMAPs should have been eaten up
			 * during string mapping that happens before
			 * placement, so we should not be here. */
			pfatal("STR_KEYMAP found unexpectedly during print phrase");
			/*NOTREACHED*/
			break;
		default:
			break;
		}
	}
}


/* print things in STUFF list */

static void
pr_stuff (stufflist_p, staffno, mll_p)

struct STUFF *stufflist_p;	/* which list of STUFF */
int staffno;			/* which staff the stuff is for */
struct MAINLL *mll_p;

{
	char lch;	/* last character in string */


	/* do each item in stuff list */
	for (   ; stufflist_p != (struct STUFF *) 0;
					stufflist_p = stufflist_p->next) {

		set_staffscale( (stufflist_p->all == YES) ? 0 : staffno);

		switch (stufflist_p->stuff_type) {

		case ST_MUSSYM:
		case ST_OCTAVE:
		case ST_ROM:
		case ST_BOLD:
		case ST_ITAL:
		case ST_BOLDITAL:
			/* do 'til' clause if any */
			extend(stufflist_p);

			/* if special case of ending in ~ or _, don't print the
			 * ~ or _ itself */
			if ((lch = last_char(stufflist_p->string)) == '~' ||
					lch == '_') {
				stufflist_p->string[strlen(stufflist_p->string)
						-1] = '\0';
			}

			/* print the string at specified place */
			if (stufflist_p->string != (char *) 0) {


				/* print grid if appropriate,
				 * otherwise just the string. */
				if (stufflist_p->modifier != TM_CHORD ||
						svpath(staffno, GRIDSWHEREUSED)
						->gridswhereused == NO ||
						pr_grid(stufflist_p,
						(stufflist_p->all == YES ?
						0 : staffno))
						== NO) {
					pr_scrunched_string (stufflist_p->c[AW],
						stufflist_p->c[AY],
						stufflist_p->string, J_LEFT,
						stufflist_p->horzscale,
						stufflist_p->inputfile,
						stufflist_p->inputlineno);
				}
			}

			break;

		case ST_CRESC:
		case ST_DECRESC:
			pr_cresc(stufflist_p);
			break;

		case ST_PEDAL:
			pr_ped_char(stufflist_p, staffno);
			break;

		case ST_PHRASE:
			pr_phrase(stufflist_p->crvlist_p, stufflist_p->modifier,
				(stufflist_p->modifier == L_NORMAL ? YES : NO),
				staffno);
			break;

		case ST_TIESLUR:
			pr_tieslur(stufflist_p, mll_p, staffno);
			break;
		
		case ST_BEND:
			pr_bend(stufflist_p->crvlist_p);
			break;

		case ST_TABSLUR:
			pr_tabslur(stufflist_p->crvlist_p,
					get_ts_style(stufflist_p, mll_p));
			break;

		case ST_MIDI:
			break;

		default:
			pfatal("unknown stuff type");
			break;
		}
	}
}


/* Print a guitar grid. Return YES if grid was found and printed, else NO. */

static int
pr_grid(stuff_p, staffnum)

struct STUFF *stuff_p;
int staffnum;

{
	struct GRID *grid_p;	/* info about the grid to print */
	double space;		/* distance between grid lines */
	float north, south;	/* bounding box of grid */
	float east, west;
	float namewidth;	/* width of just the name string */
	float gridwidth;	/* width of just the grid */
	float objectwidth;	/* width alloted to the grid object */
	double namescrunch;	/* horzscale to apply to name string */
	double gridscrunch;	/* horzscale to apply to grid */


	if ((grid_p = findgrid(stuff_p->grid_name)) == 0) {
		/* placement phase should have printed a warning already */
		return(NO);
	}

	/* If the grid object as a whole had to be scrunched to avoid
	 * colliding with an aligned STUFF, we may need to compress just
	 * the grid name, or just the grid itself, or both.
	 * We do them independently, to try to avoid squeezing
	 * either any more than absolutely necessary.
	 */
	objectwidth = stuff_p->c[AE] - stuff_p->c[AW];
	namewidth = strwidth(stuff_p->string);
	gridsize(grid_p, staffnum, &north, &south, &east, &west);
	gridwidth = east - west;

	namescrunch = gridscrunch = DEFHORZSCALE;
	if (stuff_p->horzscale != DEFHORZSCALE) {
		if (objectwidth < namewidth) {
			namescrunch = objectwidth / namewidth;
		}
		if (objectwidth < gridwidth) {
			gridscrunch = objectwidth / gridwidth;
		}
	}

	/* print the grid name */
	pr_scrunched_string(stuff_p->c[AX] - (namewidth * namescrunch) / 2.0,
			stuff_p->c[AY], stuff_p->string, J_LEFT,
			namescrunch,
			stuff_p->inputfile, stuff_p->inputlineno);

	space = gridspace(staffnum);

	do_grid(stuff_p->c[AX] - (space * gridscrunch) * (grid_p->numstr - 1) / 2.0,
			stuff_p->c[AS] - south,
			space, grid_p, staffnum, gridscrunch);
	return(YES);
}


/* print ties and slurs */

static void
pr_tieslur(stuff_p, mll_p, staffno)

struct STUFF *stuff_p;
struct MAINLL *mll_p;
int staffno;

{
	int ts_style;		/* tie/slur style (L_DOTTED or L_DASHED) */


	ts_style = get_ts_style(stuff_p, mll_p);

	/* If tabslur, do that */
	if ( stuff_p->curveno >= 0 && stuff_p->begnote_p->nslurto > 0
			&& IS_NOWHERE(stuff_p-> begnote_p->slurtolist
			[stuff_p->curveno].octave)) {
		pr_tabslur(stuff_p->crvlist_p, ts_style);
		return;
	}

	/* print a regular tie/slur curve */
	pr_phrase(stuff_p->crvlist_p, ts_style,
				(ts_style == L_NORMAL ? YES : NO), staffno );
}


/* given a TIESLUR STUFF, return the line type to use for it */

static int
get_ts_style(stuff_p, mll_p)

struct STUFF *stuff_p;
struct MAINLL *mll_p;

{
	struct GRPSYL *prevgrp_p;	/* for carryins */
	int n;				/* notelist index */


	if (stuff_p->carryin == YES) {
		prevgrp_p = prevgrpsyl(stuff_p->beggrp_p, &mll_p);
		if (stuff_p->curveno >= 0) {
			/* a carried-in slur. Need to find a note
			 * in previous group that is slurred to this one,
			 * and use its slurstyle. There is some chance
			 * that there could be more than one slur to this
			 * note from the same curveno
			 * and each slur could have a different style,
			 * in which case we no longer have enough information
			 * to know which to use, so we just use the first
			 * we find. */
			for (n = 0; n < prevgrp_p->nnotes; n++) {

				if (prevgrp_p->notelist[n].nslurto
							<= stuff_p->curveno) {
					/* couldn't have come from this grp */
					continue;
				}

				if (prevgrp_p->notelist[n].slurtolist
						[stuff_p->curveno].letter
						== stuff_p->begnote_p->letter
						&& prevgrp_p->notelist[n]
						.slurtolist[stuff_p->curveno].octave
						== stuff_p->begnote_p->octave) {

					return (prevgrp_p->notelist[n].
						slurtolist[stuff_p->curveno]
						.slurstyle);
				}
			}
		}
		else {
			/* a carried-in tie. Need to find matching note
			 * in previous group, and use its tiestyle. */
			for (n = 0; n < prevgrp_p->nnotes; n++) {
				if (prevgrp_p->notelist[n].letter ==
						stuff_p->begnote_p->letter &&
						prevgrp_p->notelist[n].octave
						== stuff_p->begnote_p->octave) {
					return(prevgrp_p->notelist[n].tiestyle);
				}
			}
		}
	}

	else {
		if (stuff_p->curveno >= 0) {
			/* a non-carried-in slur, use slurstyle */
			return(stuff_p->begnote_p->slurtolist
						[stuff_p->curveno].slurstyle);
		}
		else {
			/* a non-carried-in tie, use tiestyle */
			return(stuff_p->begnote_p->tiestyle);
		}
	}

	/* if none of those cases applied, use normal */
	return(L_NORMAL);
}


/* print a rest symbol */

static void
pr_rest(gs_p, mll_p)

struct GRPSYL *gs_p;	/* information about the rest to be printed */
struct MAINLL *mll_p;

{
	struct STAFF *staff_p;
	int musfont;
	int muschar;	/* which type of rest character to print */
	int d;		/* number of dots */
	float adjust;	/* to space dots properly */
	float y;	/* vertical location of rest */
	int size;


	if (gs_p->is_multirest == YES) {
		/* multirest are a special case */
		pr_multirest(gs_p, mll_p);
		return;
	}

	staff_p = mll_p->u.staff_p;

	/* draw the rest */
	muschar = restchar(gs_p, &musfont);
	/* Half and whole rests outside the staff need to use the version
	 * that includes a ledger line. So check for that case. 
	 * We used to use characters with ledgers all the time,
	 * but Ghostscript then sometimes seemed to misplace them
	 * by one pixel at certain magnifications, which looked bad. */
	if (is_ledgerless(musfont, muschar) == YES) {
		double halfst;
		if (svpath(staff_p->staffno, STAFFLINES)->stafflines > 1) {
			halfst = halfstaffhi(staff_p->staffno);
		}
		else {
			halfst = 0.0;
		}
		/* The adjustments to halfst are chosen so that both half
		 * and whole rests will properly get leger lines when they
		 * are outside the staff, but not when inside.
		 */
		if ( (gs_p->c[AN] > (staff_p->c[AY] + halfst + 1.7 * Stepsize)) ||
				(gs_p->c[AN] < (staff_p->c[AY] - halfst - Stdpad)) ) {
			muschar = (muschar == C_LL1REST ? C_1REST : C_2REST);
		}
	}
	size = size_def2font(gs_p->grpsize);
	if (gs_p->is_meas == YES) {
		/* measure rest is special case, have to move to middle */
		pr_muschar( (gs_p->c[AW] + gs_p->c[AE]) / 2.0,
				gs_p->restc[AY], muschar, size, musfont);
	}
	else {
		pr_muschar(gs_p->c[AX], gs_p->restc[AY], muschar, size, musfont);
	}

	/* get ready to print any dots */
	adjust = width(FONT_MUSIC, adj_size(size, Staffscale, (char *) 0,
				-1), C_DOT) / 2.0;
	y = _Cur[AY] + Stepsize;

	/* print any dots after the rest */
	for (d = 0; d < gs_p->dots; d++) {
		/* each time we print a dot, the current location will get
		 * moved to just beyond that one */
		pr_muschar(_Cur[AX] + adjust + (2.0 * Stdpad), y, C_DOT, size,
								FONT_MUSIC);
	}
}


/* Return YES if the given font/char is for a ledger-less rest symbol */

static int
is_ledgerless(restfont, restcode)

int restfont;
int restcode;

{
	if (restfont == FONT_MUSIC) {
		if ((restcode == C_LL1REST) || (restcode == C_LL2REST)) {
			return(YES);
		}
	}
	else if (restfont == FONT_MUSIC2) {
		if ((restcode == C_MENSURLL1REST)
					|| (restcode == C_MENSURLL2REST)) {
			return(YES);
		}
	}
	return(NO);
}


/* print a measure repeat, single, double, or quad */

void
pr_mrpt(gs_p, mainll_p)

struct GRPSYL *gs_p;
struct MAINLL *mainll_p;

{
	double x;		/* horizontal position of number string */
	double y, y_offset;	/* vertical location */
	double height, width;	/* of meas num string */
	char *numstr;		/* ASCII version of numbers of measures */
	int rptsym;		/* measure repeat music character */
	int rptfont;
	short print_number;	/* YES or NO */


	numstr = mr_num(mainll_p, &x, &y_offset, &height, &width);
	if (numstr == (char *) 0) {
		/* must be a measure of a double or quad that doesn't
		 * get a symbol at its ending bar line */
		return;
	}

	rptsym = mrptchar(gs_p, &rptfont);

	print_number = NO;
	if ( (gs_p->meas_rpt_type == MRT_SINGLE) &&
			(svpath(gs_p->staffno, NUMBERMRPT)->numbermrpt == YES )) {
		print_number = YES;
	}
	if ( (gs_p->meas_rpt_type != MRT_SINGLE) &&
			(svpath(gs_p->staffno, NUMBERMULTRPT)->numbermultrpt == YES )) {
		print_number = YES;
	}
	if (print_number == YES) {
		/* print number above the staff */
		y = Staffs_y[gs_p->staffno];
		pr_string(x, y + y_offset, numstr, J_LEFT, (char *) 0, -1);
	}
	if (gs_p->meas_rpt_type == MRT_SINGLE) {
		/* put symbol in middle of measure */
		x = (gs_p->c[AW] + gs_p->c[AE]) / 2.0;
	}
	else {
		/* x is currently left edge of number, so adjust to middle */
		x += strwidth(numstr) / 2.0;
	}
	pr_muschar(x, mr_y_loc(gs_p->staffno), rptsym, DFLT_SIZE, rptfont);
}


/* given a staff number, return the y at which to print the measure repeat
 * or multirest symbols. If the number of staff lines is odd, this is the
 * middle line, otherwise the line just above the middle. */

static double
mr_y_loc(staffno)

int staffno;

{
	return(Staffs_y[staffno] + Stepsize * mr_y_offset(staffno));
}


/* print the dots for dotted notes */

static void
pr_note_dots(noteinfo_p, numdots, xdotr, group_x, group_y, size)

struct NOTE *noteinfo_p;	/* which note to dot */
int numdots;		/* how many dots to print */
double xdotr;		/* relative x distance from note to print the dots */
double group_x;
double group_y;		/* coord of group, dots are relative to this */
int size;		/* DFLT_SIZE for normal or SMALLSIZE for cue */

{
	float adjust;	/* to place dots with proper spacing */


	/* if note isn't dotted, nothing to do */
	if (numdots <= 0) {
		return;
	}

	adjust = width(FONT_MUSIC, adj_size(size, Staffscale,
				(char *) 0, -1), C_DOT) / 2.0;

	/* go to where first dot belongs */
	set_cur(group_x + xdotr - adjust, group_y + noteinfo_p->ydotr);
	
	/* print as many dots as necessary */
	for (  ; numdots > 0; numdots--) {
		pr_muschar(_Cur[AX] + adjust + (2.0 * Stdpad),
				_Cur[AY], C_DOT, size, FONT_MUSIC);
	}
}


/* print parentheses around a note. Should only be called if note_has_paren
 * is YES */

static void
pr_parens(note_p, gs_p)

struct NOTE *note_p;
struct GRPSYL * gs_p;

{
	char paren_string[4];
	double y;


	/* make a parentheses string of proper size in internal string format */
	paren_string[0] = (char) FONT_TR;
	paren_string[1] = (char) adj_size(size_def2font(note_p->notesize), Staffscale, (char *) 0, -1);
	paren_string[2] = '(';
	paren_string[3] = '\0';

	/* center the parentheses vertically on the Y on the note */
	y = note_p->c[AY] - (strascent(paren_string)
					- (strheight(paren_string) / 2.0));

	/* print the left parenthesis */
	pr_string(gs_p->c[AX] + note_p->wlparen, y,
				paren_string, J_LEFT, (char *) 0, -1);

	/* now do the right parenthesis */
	paren_string[2] = ')';
	pr_string(gs_p->c[AX] + note_p->erparen - strwidth(paren_string), y,
				paren_string, J_LEFT, (char *) 0, -1);
}


/* print "with" lists */

void
pr_withlist(gs_p)

struct GRPSYL *gs_p;	/* GRPSYL that might have with lists */

{
	if (gs_p->nwith == 0) {
		return;
	}

	/* Rests are effectively spaces on tab staffs,
	 * so printing "with" items doesn't make sense.
	 * They will print on the tabnote staff */
	if (gs_p->grpcont != GC_NOTES && is_tab_staff(gs_p->staffno) == YES) {
		return;
	}

	/* There could be things both above and below; do both */
	pr_withside(gs_p, PL_ABOVE);
	pr_withside(gs_p, PL_BELOW);
}


/* Given a with list, print all the items on the specified side */

static void
pr_withside(gs_p, side)

struct GRPSYL *gs_p;
int side;	/* PL_ABOVE or PL_BELOW */

{
	double x, y;	/* where to start piling items */
	double sign;	/* 1.0 or -1.0 for direction for subsequent items */
	int nwith_side;	/* how many items on the current side */
	int normside;	/* YES/NO for being on note or stem end */
	int font, size;
	int index;	/* offset into with list */
	int first_char;		/* first char of string to print */
	char *str_p;		/* pointer into string to print */



	/* Most usual case of list being on the note side */
	if (gs_p->stemdir == UP && side == PL_BELOW) {
		normside = YES;
		if (gs_p->grpcont == GC_NOTES) {
			y = gs_p->notelist [gs_p->nnotes - 1] .c[AS];
			x = gs_p->notelist [gs_p->nnotes - 1] .c[AX];
		}
		else {
			y = gs_p->c[AS] + withheight(gs_p, side);
			x = gs_p->c[AX];
		}
		sign = -1.0;
	}
	else if (gs_p->stemdir == DOWN && side == PL_ABOVE) {
		normside = YES;
		if (gs_p->grpcont == GC_NOTES) {
			y = gs_p->notelist[0].c[AN];
			x = gs_p->notelist[0].c[AX];
		}
		else {
			y = gs_p->c[AN] - withheight(gs_p, side);
			x = gs_p->c[AX];
		}
		sign = 1.0;
	}

	/* not notes case, goes by the "stem" (which means by where the stem
	 * would be if there actually was one) */
	else if (gs_p->grpcont != GC_NOTES) {
		normside = NO;
		x = gs_p->c[AX];
		y = find_y_stem(gs_p);
		sign = (gs_p->stemdir == DOWN ? -1.0 : 1.0);
	}

	/* on the "not normal" or note head side */
	else {
		normside = NO;
		y = find_y_stem(gs_p);
		if (gs_p->stemdir == DOWN) {
			sign = -1.0;
			/* Adjust for any that have zero length stem.
			 * This could happen either because
			 * they are whole or double whole notes,
			 * or because user explicitly set zero length. */
			if (gs_p->stemlen <= 0.0) {
				y = gs_p->notelist[gs_p->nnotes - 1] .c[AS];
			}
			/* beamed notes stems effective stick out a little
			 * farther, so compensate for that */
			if (gs_p->beamloc != NOITEM) {
				y -= HALF_BEAM_THICKNESS(gs_p);
			}
		}
		else {
			sign = 1.0;
			if (gs_p->stemlen <= 0.0) {
				y = gs_p->notelist[0].c[AN];
			}
			if (gs_p->beamloc != NOITEM) {
				y += HALF_BEAM_THICKNESS(gs_p);
			}
		}
		x = gs_p->c[AX];
	}

	/* See how many "with" items on the current side */
	nwith_side = 0;
	for (index = 0; index < gs_p->nwith; index++) {
		if (gs_p->withlist[index].place == side) {
			nwith_side++;
		}
	}

	/* If a dot, wedge, and uwedge is the only item in the list,
	 * and it's on the stem side of a group with a stem, it is supposed
	 * to be aligned with the stem. */
	if (normside == NO && nwith_side == 1 &&
				(gs_p->basictime != 1 && gs_p->basictime != BT_DBL)
				&& gs_p->stemlen > 0.0 &&
				is_music_symbol(gs_p->withlist[0].string) == YES) {
		font = gs_p->withlist[0].string[0];
		size = gs_p->withlist[0].string[1];
		str_p = gs_p->withlist[0].string + 2;
		first_char = next_str_char(&str_p, &font, &size);
		if (first_char == C_DOT || first_char == C_WEDGE ||
					first_char == C_UWEDGE) {
			x = find_x_stem(gs_p);
		}
	}

	pr_withitems(gs_p, side, x, y, sign);
}


/* Given information about where to place a with list, print all its items */

static void
pr_withitems(gs_p, side, x, y, sign)

struct GRPSYL *gs_p;
int side;
double x;
double y;
double sign;

{
	char *string;
	int index;		/* through withlist */
	int font, size;
	double x_offset;	/* to center first character of item on note */
	double yposition;	/* y coordinate at which to print */
	double y_offset;
	double item_height;	/* height of with list item */
	int first_char;		/* first char of string to print */
	char *str_p;		/* pointer into string to print */
	int alternate;		/* upside version of music symbol */
	double ystaff;		/* y of middle of staff */
	double yline;		/* y value of staff line */
	double top, bot;	/* top and bottom of item to be printed */
	double pad;		/* vertical padding around short items */
	int sl;			/* staff line index */
	double adjusted_stepsize;	/* STEPSIZE or STEPSIZE * TABRATIO
			 	* depending on whether tab staff or not */
	int stafflines;		/* how many lines in current staff */
	double minwithheight;


	y_offset = 0.0;
	minwithheight = MINWITHHEIGHT * Staffscale;
	stafflines = svpath(gs_p->staffno, STAFFLINES)->stafflines;
	/* get stepsize distance based on whether it
	 * is a tab staff or not */
	adjusted_stepsize = (is_tab_staff(gs_p->staffno)
				== YES ? Stepsize * TABRATIO : Stepsize);

	/* find y of middle of staff */
	ystaff = Staffs_y[gs_p->staffno];

	/* do each item in with list */
	for (index = 0; index < gs_p->nwith; index++) {

		/* Skip things on the other side */
		if (gs_p->withlist[index].place != side) {
			continue;
		}

		string = gs_p->withlist[index].string;

		/* should center first character on x */
		font = string[0];
		size = string[1];
		str_p = string + 2;
		first_char = next_str_char(&str_p, &font, &size);

		/* get upside down version if necessary */
		if (sign == -1.0 && IS_MUSIC_FONT(font)) {
			if ((alternate = mirror(string, first_char, font))
							!= first_char) {
				*(str_p - 1) = (char) alternate;
			}
		}
		
		x_offset = left_width( &(string[0]) );

		/* get height of item to print */
		item_height = strheight(string);

		/* If string is so short vertically
		 * it could get swallowed up in a staff line,
		 * adjust to fall in a space. Placement phase will have
		 * allowed MINWITHHEIGHT, so put in middle of that area unless
		 * that would fall on a line, in which case move somewhat */
		if (item_height < minwithheight) {
			/* need to adjust this one. Start out by putting in
			 * middle vertically of reserved area */
			yposition = y + y_offset + sign * minwithheight / 2.0;

			/* no reason to adjust further for 1-line staffs */
			if (stafflines > 1) {

				/* take the extra vertical space alloted to this
				 * with list item, and add 1/4 of it on top
				 * and bottom as padding. If no staff line is
				 * in between the boundaries of the item after
				 * adding that padding, it's good enough where
				 * it is. Otherwise, if a staff line falls above
				 * the middle of the item, move the item
				 * down into space. Otherwise move it
				 * up into space.
				 */
				pad = (minwithheight - item_height) / 4.0;
				top = yposition + (item_height / 2.0) + pad;
				bot = yposition - (item_height / 2.0) - pad;

 				/* check each staff line for collisions, from
				 * bottom to top */
				for (sl = -(stafflines - 1);
						sl <= (stafflines - 1);
						sl += 2) {

					/* find y of current staff line */
					yline = ystaff + (sl * adjusted_stepsize);

					/* check if current staff line goes
					 * through the item
					 * as currently placed */
					if (yline < top && yline > bot) {
						/* collides--need to move */

						if ((top - yline) >
								(yline - bot)) {
							/* move up to area
							 * above the line */
							yposition += 2.0 * pad;
							/* if overdid the move,
							 * move back a bit */
							if (yposition - yline -
							(item_height / 2.0)
							> 0.7 * adjusted_stepsize) {
								yposition -=
								 0.4 * adjusted_stepsize;
							}
						}
						else {
							/* move down to area
							 * below the line */
							yposition -= 2.0 * pad;
							if (yline - yposition -
							(item_height / 2.0)
							> 0.7 * adjusted_stepsize) {
								yposition +=
								 0.4 * adjusted_stepsize;
							}
						}

						/* only 1 staff line can
						 * possibly interfere,
						 * and we've found that one, so
						 * can jump out of loop */
						break;
					}
				}
			}

			/* adjust y_offset to include the area taken by item */
			y_offset += minwithheight * sign;

			/* up to now, we've been using the center of the item,
			 * so now adjust to baseline */
			if (sign > 0.0) {
				yposition += (item_height / 2.0)
					- strascent(string);
			}
			else {
				yposition -= (item_height / 2.0)
					- strdescent(string);
			}
		}
		else {
			/* not too short, handle normally */
			y_offset += item_height * sign;
			yposition = y + y_offset;

			/* adjust to get to baseline of string from top or
			 * bottom that we've used up to this point */
			if (sign > 0.0) {
				yposition -= strascent(string);
			}
			else {
				yposition += strdescent(string);
			}
		}

		pr_string(x - x_offset, yposition, string,
				J_CENTER, gs_p->inputfile, gs_p->inputlineno);
	}
}


/* print note stems and flags. Also print any slashes and alt lines */

static void
pr_stems(grpsyl_p)

struct GRPSYL *grpsyl_p;	/* which group's stem to print */

{
	float x, y1, y2;
	float first_x, last_x;	/* x of stems at ends of beamed set */
	float sign;	/* 1 or -1 direction for moving to draw slashes */
	float y_offset, offset, spacing;	/* for where to draw slashes */
	float y_tilt;		/* how much to move in y direction to get
				 * proper tilt on slashes */
	float halfwidth;	/* half width of slash or alt line */
	struct GRPSYL *first_p, *last_p;	/* beginning and ending group
						 * of beam group */
	int grpsize;		/* grpsize field of grpsyl_p */
	int grpvalue;		/* grpvalue field of grpsyl_p */
	int slash;		/* to count number of slashes drawn */
	int eff_stemdir;	/* to account for quad/oct stem oddities */
	struct NOTE *note_p;


	/* if no stem, nothing to do */
	if ( grpsyl_p->stemlen <= 0 && grpsyl_p->slash_alt == 0) {
		return;
	}

	/* figure out x coordinate of stem */
	x = find_x_stem(grpsyl_p);

	/* if stem is up, start at bottom note, if down, at top */
	if (grpsyl_p->stemdir == UP) {
		note_p = &(grpsyl_p->notelist[ grpsyl_p->nnotes - 1]);
		y1 = note_p->c[AY];
		y2 = find_y_stem(grpsyl_p);
		sign = -1;
	}
	else {
		note_p = &(grpsyl_p->notelist [0]);
		y1 = note_p->c[AY];
		y2 = find_y_stem(grpsyl_p);
		sign = 1;
	}

	if (note_p->headchar != 0) {
		if (grpsyl_p->basictime == BT_QUAD
					|| grpsyl_p->basictime == BT_OCT) {
			/* Long notes have the stem on the right,
			 * so the place to put the stem is as if it was up,
			 * even if the stem is actually down. */
			eff_stemdir = UP;
		}
		else {
			eff_stemdir = grpsyl_p->stemdir;
		}
		y1 += stem_yoff(note_p->headchar, note_p->headfont, eff_stemdir)
				* (Stepsize * size2factor(note_p->notesize));
	}

	if (grpsyl_p->basictime != 1 && grpsyl_p->basictime != BT_DBL) {
		/* print the stem */
		do_linetype(L_NORMAL);

		if (STEMSIDE_CENTER(grpsyl_p) == YES) {
			int n;

			/* Draw a stem from the top of the top/bottom note */
			if (grpsyl_p->stemdir == UP) {
				y1 = grpsyl_p->notelist[0].c[AN] - Stdpad;
			}
			else {
				y1 = grpsyl_p->notelist[grpsyl_p->nnotes-1].c[AS]
						+ Stdpad;
			}
			draw_line(x, y1, x, y2);

			/* Now draw line segments between any notes pairs
			 * which are more than 2 steps apart. */
			for (n = 0; n < grpsyl_p->nnotes - 1; n++) {
				if ((grpsyl_p->notelist[n].stepsup -
					grpsyl_p->notelist[n+1].stepsup) > 2) {
					draw_line(x,
						grpsyl_p->notelist[n].c[AS] + Stdpad,
						x,
						grpsyl_p->notelist[n+1].c[AN] - Stdpad);
				}
			}
		}
		else {
			draw_line(x, y1, x, y2);
		}

		/* attach any flags as appropriate */
		pr_flags(grpsyl_p, (double) x, (double) y2);
	}

	/* print any slashes */
	if (grpsyl_p->slash_alt > 0) {

		/* adjust for flags or beams. */
		if (grpsyl_p->basictime >= 8) {
			offset = fabs(beam_offset(numbeams(grpsyl_p->basictime),
					grpsyl_p->grpsize, grpsyl_p->stemdir));
			if (grpsyl_p->beamloc == NOITEM) {
				offset = (numbeams(grpsyl_p->basictime) - 1)
					* size2flagsep(grpsyl_p->grpsize)
					* Staffscale;
				if (grpsyl_p->grpsize == GS_NORMAL) {
					offset += 8.0 * Stdpad;
				}
				else if (grpsyl_p->basictime != 16) {
					/* 16th small notes don't have any extra
					 * stem to account for extra flag */
					offset += 2.0 * Stdpad;
				}
			}
		}
		else {
			offset = 0.0;
		}

		if ( grpsyl_p->beamloc == NOITEM) {
			/* unbeamed things get hard-coded tilt value */
			if (grpsyl_p->grpvalue == GV_ZERO) {
				y_tilt = (grpsyl_p->stemdir == UP ? 3.5 : -3.5)
							* Stdpad;
			}
			else {
				y_tilt = 2.2 * Stdpad;
			}
		}

		else {
			/* beamed. Need to slant slashes the same as beam */

			grpsize = grpsyl_p->grpsize;
			grpvalue = grpsyl_p->grpvalue;

			/* find beginning and ending stems */
			for (first_p = grpsyl_p; (first_p->beamloc != STARTITEM)
					|| (first_p->grpsize != grpsize)
					|| (first_p->grpvalue != grpvalue);
					first_p = first_p->prev) {
				;
			}

			for (last_p = grpsyl_p; (last_p->beamloc != ENDITEM)
					|| (last_p->grpsize != grpsize)
					|| (last_p->grpvalue != grpvalue);
					last_p = last_p->next) {
				;
			}

			/* calculate slope from them. We find the ratio of
			 * y to x of the beam and apply that proportion to
			 * the known x length of the slash to get the y height
			 * of the slash, then divide by 2 to get the y distance
			 * on either side of the stem. */
			/* Guard again divide by zero */
			first_x = find_x_stem(first_p);
			last_x = find_x_stem(last_p);
			if (first_x == last_x) {
				pfatal("found zero length beam when drawing alt");
			}
			y_tilt = (((find_y_stem(last_p) - find_y_stem(first_p))
					* (2.0 * slash_xlen(grpsyl_p)))
					/ (last_x - first_x)) / 2.0;
		}

		/* draw the slashes */
		pr_slashes(grpsyl_p, (double) x, (double) y2, (double) sign,
				(double) offset, (double) y_tilt);
	}

	/* print alt group lines if any */
	if (grpsyl_p->slash_alt < 0) {
		struct GRPSYL *grpsyl2_p;
		float grp2x, grp2y;	/* stem of second group */
		float grp1y_offset, grp2y_offset;


		if (grpsyl_p->next == (struct GRPSYL *) 0) {
			pfatal("missing second group in alt pair");
		}

		/* figure out how wide to draw the lines and how far apart
		 * to make them */
		if (grpsyl_p->grpsize == GS_NORMAL) {
			halfwidth = W_WIDE * Staffscale / PPI / 2.0;
			spacing = 5.0 * Stdpad;
		}
		else if (grpsyl_p->grpsize == GS_SMALL) {
			halfwidth = W_MEDIUM * Staffscale / PPI / 2.0;
			spacing = 3.0 * Stdpad;
		}
		else { /* GS_TINY */
			/* Actually we don't allow alts on grace notes,
			 * so we'd never have tiny alts, but this should be
			 * about right if we ever did allow tiny alts. */
			halfwidth = W_NORMAL * Staffscale / PPI / 2.0;
			spacing = 2.0 * Stdpad;
		}

		/* find the stem coordinates of the second group */
		grpsyl2_p = grpsyl_p->next;
		grp2x = find_x_stem(grpsyl2_p);
		grp2y = find_y_stem(grpsyl2_p);

		/* on notes shorter than half note, the lines don't go all the
		 * way to the stems */
		if ( grpsyl_p->basictime >= 4) {
			/* figure out where the y of the end of the line is
			 * by multiplying the x value by the tangent of the
			 * angle of the line that would go all the way
			 * between the stems */
			grp2y_offset = (grp2x - x - (6.0 * Stdpad))
						* ((grp2y - y2) / (grp2x - x));
			grp1y_offset = (6.0 * Stdpad)
					* ((grp2y - y2) / (grp2x - x));
			/* if 8th notes or shorter, get out of way of beams */
			offset = numbeams(grpsyl_p->basictime) * spacing;
			x += (6.0 * Stdpad);
			grp2x -= (6.0 * Stdpad);
		}
		else {
			grp1y_offset = 0.0;
			grp2y_offset = grp2y - y2;
			offset = 0.0;
		}

		/* draw the alt lines */
		for (slash = -(grpsyl_p->slash_alt) - 1; slash >= 0; slash--) {
			y_offset = sign * slash * spacing + (sign * offset);
			do_newpath();
			do_moveto(x, y2 + y_offset + grp1y_offset - halfwidth);
			do_line(x, y2 + y_offset + grp1y_offset + halfwidth);
			do_line(grp2x, y2 + y_offset + grp2y_offset
								+ halfwidth);
			do_line(grp2x, y2 + y_offset + grp2y_offset
								- halfwidth);
			do_closepath();
			do_fill();
		}

		/* earlier phase wanted both groups in alt pair to have
		 * slash_alt set, but now we've printed this one, so clear
		 * the one on the following group, so it won't try to
		 * print another alt group */
		grpsyl2_p->slash_alt = 0;
	}
}


void
pr_slashes(grpsyl_p, x, y, sign, offset, y_tilt)

struct GRPSYL *grpsyl_p;
double x;
double y;
double sign;
double offset;
double y_tilt;

{
	int slash;
	double xlen;
	float y_offset;
	float spacing;
	float halfwidth;


	/* get length based on note head size */
	xlen = slash_xlen(grpsyl_p);

	/* figure out how wide to make the slashes and how far apart
	* to space them */
	if (grpsyl_p->grpsize == GS_NORMAL) {
		halfwidth = W_WIDE * Staffscale / PPI / 2.0;
		spacing = 5 * Stdpad;
	}
	else if (grpsyl_p->grpsize == GS_SMALL) {
		halfwidth = W_MEDIUM * Staffscale / PPI / 2.0;
		spacing = 4 * Stdpad;
	}
	else { /* GS_TINY */
		halfwidth = W_NORMAL * Staffscale / PPI / 2.0;
		spacing = 3 * Stdpad;
	}

	/* If stem is so short that slashes would end up hitting the end note,
	 * or even ending up on the wrong side of it, fix that to put them
	 * where the stem would be if it was long enough.
	 * But if there are flags/beams, just leave it as is, because we
	 * want to put them inside the beams.
	 */
	if ((numbeams(grpsyl_p->basictime) == 0) &&
			(spacing * grpsyl_p->slash_alt + 2.0 * Stepsize
			> grpsyl_p->stemlen)) {
		offset -= spacing * grpsyl_p->slash_alt
			+ 2.0 * Stepsize - grpsyl_p->stemlen;
	}


	for (slash = grpsyl_p->slash_alt; slash > 0; slash--) {
		y_offset = y + sign * (offset + (spacing * slash));

		/* draw filled parallelogram */
		draw_parallelogram(x - xlen, y_offset - y_tilt,
				x + xlen, y_offset + y_tilt, halfwidth);
	}
}


/* print flags on 8th and shorter notes */

static void
pr_flags(grpsyl_p, x, y)

struct GRPSYL *grpsyl_p;	/* group for which to draw flags */
double x;
double y;			/* coord of end of stem */

{
	int muschar;	/* what kind of flag to print */
	int flagfont;
	float y_spacing;	/* distance to next flag */
	float y_offset;	/* from end of stem */
	int f;		/* how many flags */
	int size;


	/* only 8th and shorter notes might have flags */
	if (grpsyl_p->basictime < 8) {
		return;
	}

	/* if not a note, no flag */
	if (grpsyl_p->grpcont != GC_NOTES) {
		return;
	}

	/* if beamed, no flag */
	if (grpsyl_p->beamloc != NOITEM) {
		return;
	}

	/* figure out whether small/reg  and if up/down */
	size = size_def2font(grpsyl_p->grpsize);
	if (grpsyl_p->stemdir == UP) {
		muschar = C_DNFLAG;
		y_spacing = - size2flagsep(grpsyl_p->grpsize);
	}
	else {
		muschar = C_UPFLAG;
		y_spacing = size2flagsep(grpsyl_p->grpsize);
	}

	/* Handle override of flag character, if any.
	 * We have to assume that if the user overrode the symbol,
	 * they made it similar enough that our placement
	 * will look good. */
	flagfont = FONT_MUSIC;
	(void) get_shape_override(grpsyl_p->staffno, grpsyl_p->vno,
					&flagfont, &muschar);


	/* do for each flag. f == 1 less than the number of flags, and is
	 * how much to multiply the y_offset by for each flag */
	for ( f = numbeams(grpsyl_p->basictime) - 1; f >= 0; f--) {
		y_offset = f * y_spacing * Staffscale;
		pr_muschar(x + width(flagfont,
				adj_size(size, Staffscale, (char *) 0, -1),
				muschar) / 2.0,
				y + y_offset, muschar, size, flagfont);
	}
}


/* Print any accidentals */

static void
pr_accidental(noteinfo_p, grpsyl_p)

struct NOTE *noteinfo_p;	/* info about the note being printed */
struct GRPSYL *grpsyl_p;	/* info about the group conatining the note */

{
	int font;
	int size;
	int a_size;		/* size adjusted for Staffscale */
	int code;
	int a;			/* index through accidentals */
	char paren_string[4];	/* "(" or ")" in internal format */
	double y_offset = 0.0;	/* y adjustment of ( ) */
	double acc_width;	/* width of accidental */
	double acc_offset = 0.0;	/* from waccr */


	size = size_def2font(noteinfo_p->notesize);
	a_size = adj_size(size, Staffscale, (char *) 0, -1);

	if (noteinfo_p->acc_has_paren == YES) {
		/* create string for "(" */
		paren_string[0] = (char) FONT_TR;
		paren_string[1] = (char) a_size;
		paren_string[2] = '(';
		paren_string[3] = '\0';

		/* To center things vertically on the note, need to
		 * adjust parentheses downward by difference between
		 * the ascent and half the height of the parenthesis. */
		y_offset = strascent(paren_string) -
					(strheight(paren_string) / 2.0);
		pr_string(grpsyl_p->c[AX] + noteinfo_p->waccr,
					noteinfo_p->c[AY] - y_offset,
					paren_string, J_LEFT,
					grpsyl_p->inputfile,
					grpsyl_p->inputlineno);
		acc_offset += strwidth(paren_string);
	}

	for (a = 0; a < MAX_ACCS * 2; a += 2) {
		if (noteinfo_p->acclist[a] == 0) {
			/* end of list */
			break;
		}
		font = noteinfo_p->acclist[a];
		code = noteinfo_p->acclist[a+1];
		acc_width = width(font, a_size, code);
		pr_muschar(grpsyl_p->c[AX] + noteinfo_p->waccr
				+ acc_offset + acc_width / 2.0,
				noteinfo_p->c[AY], code, size, font);
		acc_offset += acc_width;
	}

	if (noteinfo_p->acc_has_paren == YES) {
		paren_string[0] = (char) FONT_TR;
		paren_string[1] = (char) a_size;
		paren_string[2] = ')';
		paren_string[3] = '\0';
		pr_string(_Cur[AX], noteinfo_p->c[AY] - y_offset,
				paren_string, J_LEFT,
				grpsyl_p->inputfile, grpsyl_p->inputlineno);
	}
}


/* print appropriate number of leger lines */

static void
pr_leger(noteinfo_p, gs_p, staffno)

struct NOTE *noteinfo_p;	/* info about current note */
struct GRPSYL *gs_p;		/* which group contains the note */
int staffno;			/* which staff to draw relative to */

{
	register int lines2draw;	/* how many leger lines are needed */
	float sign;			/* 1 for above or -1 for below staff */
	float y;			/* vertical position */
	float left_leger, right_leger;	/* how far legers stick out from note */
	int is_intermediate;		/* YES if inner, NO if outermost */
	int on_other_side;		/* YES if on "wrong" side of stem */


	if ((lines2draw = numlegers(noteinfo_p)) < 1) {
		/* No legers needed for this note */
		return;
	}

	/* Is note above or below the middle of the staff? */
	sign = noteinfo_p->stepsup > 0.0 ? 1.0 : -1.0;

	/* For notes on the "wrong" side of the stem, we will only need
	 * to draw the outermost leger. */
	if ( (gs_p->stemdir == DOWN && noteinfo_p->c[AE] < gs_p->c[AX]) ||
			(gs_p->stemdir == UP && noteinfo_p->c[AW] > gs_p->c[AX])) {
		on_other_side = YES;
	}
	else {
		on_other_side = NO;
	}

	/* Draw the legers */
	do_linetype(L_NORMAL);
	is_intermediate = NO;
	for (    ; lines2draw > 0; lines2draw--) {

		/* Find the y location for the leger line.
		 * They are 2 Stepsizes apart,
		 * beginning at the edge of the staff  */
		y = Staffs_y[staffno]
				+ (sign * (2 + lines2draw) * (2 * Stepsize));

		/* If things are packed really close together, leger lines
		 * could bleed into leger lines of the neighboring chord.
	 	 * We need to see if there are any potentially
		 * troublesome leger lines on either side, and shorten
		 * this leger if necessary to avoid them.
		 */
		left_leger = leger_length(noteinfo_p, gs_p->prev, lines2draw,
				YES, is_intermediate);
		right_leger = leger_length(noteinfo_p, gs_p->next, lines2draw,
				NO, is_intermediate);

		draw_line( noteinfo_p->c[AW] - left_leger, y,
			noteinfo_p->c[AE] + right_leger, y);
		is_intermediate = YES;

		/* For notes on the "wrong" side of the stem, we only need
		 * to draw the outermost leger */
		if (on_other_side == YES) {
			break;
		}
	}
}


/* How many legers to draw is absolute value of stepsup divided
 * by 2 minus the 2 lines that are already in the staff.
 * Note that we only do legers on normal 5-line staffs. */

static int
numlegers(noteinfo_p)

struct NOTE *noteinfo_p;

{
	return (abs(noteinfo_p->stepsup) / 2) - 2;
}


/* If things are packed really close together, leger lines
 * could bleed into leger lines of the neighboring chord.
 * This function will detect that and shorten them if necessary.
 * To be completely correct, it should check all the voices on the
 * staff, but that would be quite a bit more work, and chances of colliding
 * with another voice's notes is not very high, so we just check
 * the voice of the note in question.
 */

static double
leger_length(noteinfo_p, othergs_p, lines, other_is_prev, is_intermediate)

struct NOTE *noteinfo_p;	/* we are finding leger length for this note */
struct GRPSYL *othergs_p;	/* check this group for a too close note */
int lines;			/* how many leger lines to draw */
int other_is_prev;		/* YES if othergs_p is ->prev, NO if ->next */
int is_intermediate;		/* YES if interior, NO is outermost leger */

{
	int n;				/* note index */
	double distance;		/* between 2 notes */
	double length = 2.2 * Stdpad;	/* length of leger. Init to default */
	double adjust;			/* inners can be shortened extra */


	if (othergs_p == 0) {
		/* No group to collide with */
		return(length);
	}
	if (othergs_p->grpcont != GC_NOTES) {
		/* Can't have leger lines */
		return(length);
	}

	/* Legers that are not through or right next to the note
	 * can be shortened a bit more to make their gap show up better.
	 */
	adjust = (is_intermediate ? 0.5 * Stdpad : 0.0);

	/* See if othergs_p has any notes that are too close */
	for (n = 0; n < othergs_p->nnotes; n++) {
		if (numlegers( &(othergs_p->notelist[n]) ) < lines) {
			/* Neighboring note has fewer legers; not relevant */
			continue;
		}
		if (noteinfo_p->stepsup > 0 &&
					othergs_p->notelist[n].stepsup < 0) {
			/* Neighboring note's legers are below, ours above.
			 * The remaining neighboring notes are irrelevant. */
			break;
		}
		if (noteinfo_p->stepsup < 0 &&
					othergs_p->notelist[n].stepsup > 0) {
			/* Neighboring note's legers are above, ours below.
			 * Haven't gotten to any potentially relevant
			 * notes yet. */
			continue;
		}

		/* We have a pair of notes whose leger lines might collide.
		 * See how far apart they are. */
		if (other_is_prev == YES) {
			distance = noteinfo_p->c[AW] - othergs_p->notelist[n].c[AE];
		}
		else {
			distance = othergs_p->notelist[n].c[AW] - noteinfo_p->c[AE];
		}

		/* Ideally, we try to make leger lines 2.2 Stdpads on each side,
		 * but if that leaves less than 2.0 Stdpads between them,
		 * we shorten them until they get down to 0.7 Stdpads.
		 * After that we let them join. That should only happen
		 * if things are really tightly packed.
		 * The 6.4 is from two legers of 2.2 each with 2.0 between.
		 */
		if (distance < 6.4 * Stdpad) {
			/* Too close. Will have to shorten */
			length = (distance - (2.0 * Stdpad)) / 2.0 - adjust;
			if (length < 0.7 * Stdpad - adjust) {
				/* No shorter than minimum */
				length = 0.7 * Stdpad - adjust;
			}
		}
	}
	return (length);
}


/* given the first group of a tuplet, return, via pointers, the x coords of
 * the left and right boundaries of the tuplet number and its height.
 * Return pointer to static string containing the tuplet number itself in
 * internal string format */

char *
tupnumsize(gs_p, west_p, east_p, height_p, staff_p)

struct GRPSYL *gs_p;
float *west_p;		/* west coord returned here */
float *east_p;		/* east coord returned here */
float *height_p;	/* string height returned here */
struct STAFF *staff_p;	/* staff pointing at gs_p */

{
	char *numstr;			/* tuplet number as internal string */
	struct GRPSYL *last_gs_p;	/* last group in tuplet */
	float num_x;			/* x coord of number */
	float halfnumwidth;		/* half the width of numstr */
	int tupside;
	int all_cue;


	/* assume all cue till proven otherwise */
	all_cue = YES;

	/* find x of middle of tuplet number */
	if (gs_p->tuploc == LONEITEM) {
		if (gs_p->grpsize != GS_SMALL) {
			all_cue = NO;
		}
		num_x = gs_p->c[AX];
	}
	else {
		for (last_gs_p = gs_p->next; last_gs_p != (struct GRPSYL *) 0;
					last_gs_p = last_gs_p->next) {
			if (gs_p->grpsize != GS_SMALL) {
				all_cue = NO;
			}
			if (last_gs_p->tuploc == ENDITEM) {
				break;
			}
		}
		if (last_gs_p == (struct GRPSYL *) 0) {
			pfatal("missing end tuplet in tupnumsize");
		}

		/* Usually, the x location of tuplet number is average of
		 * beginning and end group x coords. But if there is a beam
		 * and the number is being printed on the beam side,
		 * and there is no bracket being printed,
		 * it generally looks better to center between the stems.
		 */
		tupside = tupdir(gs_p, staff_p);
		if (gs_p->beamloc == STARTITEM && last_gs_p->beamloc == ENDITEM
				&& ((tupside == PL_ABOVE && gs_p->stemdir == UP)
				|| (tupside == PL_BELOW && gs_p->stemdir == DOWN))
				&& tupgetsbrack(gs_p) == NO) {
			num_x = (find_x_stem(last_gs_p) + find_x_stem(gs_p)) / 2.0;
		}
		else {
			num_x = (last_gs_p->c[AX]  + gs_p->c[AX]) / 2.0;
		}
	}

	/* prepare the string to print */
	numstr = num2str(gs_p->tupcont);
	/* force to 11-point newcentury bold-italics, unless all cue,
	 * then smaller */
	numstr[0] = FONT_NX;
	numstr[1] = (char) adj_size((all_cue == YES ? 9 : 11), Staffscale,
				(char *) 0, -1);
	halfnumwidth = strwidth(numstr) / 2.0;

	/* return the values */
	*west_p =  num_x - halfnumwidth - Stdpad;
	*east_p = num_x + halfnumwidth + Stdpad;
	*height_p = strheight(numstr);
	return(numstr);
}


/* go through measure. If there are any tuplets, print a number by them,
 * along with bracket if appropriate. */

static void
pr_tupnums(gs_p, staff_p)

struct GRPSYL *gs_p;	/* start from here to walk through list of groups */
struct STAFF *staff_p;	/* staff pointing to gs_p */

{
	struct GRPSYL *first_gs_p = 0;	/* where to begin tuplet label.
				 * Initialization is just to shut up bogus
				 * compiler warning. */
	struct GRPSYL *g_p;	/* to check for all spaces */
	float x1, x2;		/* where tuplet bracket begins & ends */
	float num_y;		/* y of tuplet number */
	float y1, y2;		/* y coord of ends of bracket */
	char *numstr;		/* ASCII version of tuplet number */
	float numeast, numwest;	/* boundaries of tuplet number */
	float height;		/* of tuplet number */
	float y_adjust;		/* adjustment for space taken by number */
	float x_adjust;		/* from group x to where bracket goes */
	int num_notes = 0;	/* how many notes in tuplet */
	int need_brack = NO;	/* set to YES if the beaming of the notes
				 * doesn't match the tuplet boundaries */
	float brackdir;		/* how far in y direction to draw bracket ends
				 * (positive or negative depending on the
				 * direction that the bracket points) */
	int size;
	int rfont;		/* rest symbol font */
	int rcode;		/* rest symbol code */


	/* go through all the groups */
	for (   ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {

		switch (gs_p->tuploc) {

		case NOITEM:
			break;

		case STARTITEM:
			/* remember beginning for later use */
			first_gs_p = gs_p;
			num_notes = 1;
			break;

		case INITEM:
			num_notes++;
			break;

		case LONEITEM:
			first_gs_p = gs_p;
			/*FALLTHRU*/

		case ENDITEM:
			num_notes++;
			
			/* if not to be printed, nothing to do except reinit */
			if (gs_p->printtup == PT_NEITHER) {
				num_notes = 0;
				break;
			}

			/* we don't do tuplet numbers on cross-staff beams--
			 * it's virtually impossible to know where to put them
			 */
			if (gs_p->beamto != CS_SAME) {
				num_notes = 0;
				break;
			}

			/* If the tuplet is all spaces,
			 * there is nothing to draw a bracket over,
			 * and trying to do so causes problems,
			 * so don't try. */
			for (g_p = first_gs_p; g_p->tuploc != NOITEM;
							g_p = g_p->next) {
				if (g_p->grpcont != GC_SPACE) {
					/* good--it has something
					 * other than spaces */
					break;
				}

				if (g_p->tuploc == ENDITEM
						|| g_p->tuploc == LONEITEM) {
					/* reached end of all-space tuplet */
					break;
				}
			}
			if (g_p->grpcont == GC_SPACE) {
				/* must have been all spaces */
				num_notes = 0;
				break;
			}

			/* if tuplet doesn't match beaming, need bracket */
			need_brack = tupgetsbrack(first_gs_p);

			if (num_notes == 0) {
				pfatal("no notes in tuplet");
			}

			numstr = tupnumsize(first_gs_p, &numwest, &numeast,
							&height, staff_p);

			if (tupdir(first_gs_p, staff_p) == PL_ABOVE) {
				y_adjust = strascent(numstr);
				y1 = first_gs_p->c[AN] - y_adjust;
				y2 = gs_p->c[AN] - y_adjust;
				brackdir = -3.0 * Stdpad;
			}
			else {
				/* print below */
				y1 = first_gs_p->c[AS];
				y2 = gs_p->c[AS];
				brackdir = 3.0 * Stdpad;
			}

			/* print tuplet number at correct place */
			y1 += first_gs_p->tupextend;
			y2 += gs_p->tupextend;
			num_y = (y1 + y2) / 2.0;
			pr_string(numwest + Stdpad, num_y, numstr, J_LEFT,
					gs_p->inputfile, gs_p->inputlineno);

			/* add tuplet bracket if necessary */
			if (need_brack == YES) {
				do_linetype(L_NORMAL);
				
				/* adjust to reach edge of note head */
				size = (first_gs_p->grpsize == GS_NORMAL ?
						DFLT_SIZE : SMALLSIZE)
						* Staffscale;
				if (first_gs_p->grpcont == GC_NOTES) {
					x_adjust = widest_head(first_gs_p)
						* Staffscale / 2.0;
				}
				else if (first_gs_p->grpcont == GC_REST) {
					rcode = restchar(first_gs_p, &rfont);
					x_adjust = width(rfont, size, rcode) / 2.0;
				}
				else {
					x_adjust = 0.0;
				}
				x1 = first_gs_p->c[AX] - x_adjust;

				size = (gs_p->grpsize == GS_NORMAL ?
						DFLT_SIZE : SMALLSIZE)
						* Staffscale;
				if (gs_p->grpcont == GC_NOTES) {
					x_adjust = widest_head(gs_p)
						* Staffscale / 2.0;
				}
				else if (gs_p->grpcont == GC_REST) {
					rcode = restchar(gs_p, &rfont);
					x_adjust = width(rfont, size, rcode) / 2.0;
				}
				else {
					x_adjust = 0.0;
				}
				x2 = gs_p->c[AX] + x_adjust;

				/* move the bracket line up from the baseline
				 * of the number */
				y1 += (4.0 * Stdpad);
				y2 += (4.0 * Stdpad);
				num_y += (4.0 * Stdpad);

				/* figure out how much to adjust y from num_y
				 * to account for the space taken up by the
				 * number. Use ratio of similar triangles. */
				if (numwest - x1 == 0.0) {
					/* avoid any chance of divide by 0 */
					y_adjust = 0.0;
				}
				else {
					y_adjust = (((numeast - numwest
						+ (Stdpad * 2.0)) *
						(num_y - y1)) / (numeast - x1))
						/ 2.0;
				}

				draw_line(x1, y1, numwest - Stdpad,
						num_y - y_adjust); 
				draw_line(numeast + Stdpad,
						num_y + y_adjust, x2, y2); 
				draw_line(x1, y1, x1, y1 + brackdir);
				draw_line(x2, y2, x2, y2 + brackdir); 
			}

			/* re-init in case other tuplets in same measure */
			num_notes = 0;

			break;

		default:
			pfatal("bad tuplet type");
			break;
		}
	}
}


/* utility function. Given the first group in a tuplet, return YES if it
 * is to have a bracket printed. It does if the tuplet itself is to be printed,
 * and if not a LONEITEM and if any of the beamlocs do not match the tuploc */

int
tupgetsbrack(gs_p)

struct GRPSYL *gs_p;	/* first group of tuplet */

{
	/* If nothing is to be printed or number only, no bracket */
	if (gs_p->printtup == PT_NEITHER || gs_p->printtup == PT_NUMBER) {
		return(NO);
	}

	/* single chord tuplets never get a bracket -- not enough room
	 * to draw one */
	if (gs_p->tuploc == LONEITEM) {
		return(NO);
	}

	/* if user insists on a bracket, we oblige */
	if (gs_p->printtup == PT_BOTH) {
		return(YES);
	}

	/* check for mismatches between beamloc and tuploc. */
	for (  ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {
		/* grace notes don't count */
		if (gs_p->grpvalue == GV_ZERO) {
			continue;
		}

		if (gs_p->tuploc != gs_p->beamloc) {
			return(YES);
		}
		if (gs_p->tuploc == ENDITEM) {
			/* matched beam everywhere, so no bracket needed */
			return(NO);
		}
	}
	pfatal("missing end tuplet");

	/*NOTREACHED*/
	return(NO);
}


/* utility function to return PL_ABOVE  or PL_BELOW
 * depending on whether the number for
 * the given tuplet should get printed above or below the groups */
/* Can be passed any group in the tuplet. If not the first, it will find the
 * first and go from there */

int
tupdir(gs_p, staff_p)

struct GRPSYL *gs_p;	/* group in tuplet */
struct STAFF *staff_p;	/* staff pointing to gs_p */

{
	RATIONAL starttime, endtime;	/* begin & end time of tuplet */
	struct GRPSYL *save_gs_p;	/* temporarily save value of gs_p */
	int othervoice;			/* array subscript in staff_p->groups_p
					 * of the other voice on this staff */
	int vscheme;			/* V_* value */
	RATIONAL smalltime;


	smalltime.n = 1;
	smalltime.d = 2 * MAXBASICTIME;


	switch (gs_p->tuploc) {

	case LONEITEM:
	case STARTITEM:
		/* this is the one we want */
		break;

	case NOITEM:
		pfatal("arg of tupdir is not in a tuplet");
		/*NOTREACHED*/
		break;
	default:
		/* have to back up to beginning of tuplet first */
		for (    ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->prev) {
			if (gs_p->tuploc == STARTITEM) {
				break;
			}
		}
		if (gs_p == (struct GRPSYL *) 0) {
			pfatal("can't find beginning of tuplet");
		}
		break;
	}

	/* figure out which side. First determine vscheme */

	/* There used to be  a circumstance where we looked at an entire score
	 * in relvert, and if some of the score had V_1 and some of it
	 * didn't, it was possible for Mup to get confused and think something
	 * wasn't V_1 when it was. We would then try to look at the other
	 * voice, which is null, and would blow up. To avoid this, if one
	 * voice is null, we treat measure as V_1 regardless of what vscheme
	 * might lead us to believe. That case has since been fixed in relvert,
	 * so this "if" statement should never be true anymore,
	 * but it doesn't hurt to have it, as defensive code, just in case.
	 */
	if (staff_p->groups_p[1] == (struct GRPSYL *) 0) {
		return(tupdir1voice(gs_p));
	}

	/* voice 3 pays no attention to any other voices. */
	if (gs_p->vno == 3) {
		return(tupdir1voice(gs_p));
	}

	if ((vscheme = svpath(staff_p->staffno, VSCHEME)->vscheme) == V_1) {
		return(tupdir1voice(gs_p));
	}
	else if (vscheme == V_2OPSTEM) {
		/* 2 opposing stem voices, always put tuplet above voice 1 and
		 * below voice 2 */
		if (gs_p->tupside != PL_UNKNOWN) {
			l_warning(gs_p->inputfile, gs_p->inputlineno,
					"tuplet side specification not valid when vscheme=2o");
			/* fix so we don't print error again if called
			 * again on this tuplet */
			gs_p->tupside = PL_UNKNOWN;
		}
		return(gs_p->vno == 1 ? PL_ABOVE : PL_BELOW);
	}
	else {
		/* find the time period taken by tuplet */
		save_gs_p = gs_p;
		starttime = Zero;
		/* find time to where tuplet begins */
		for (gs_p = gs_p->prev; gs_p != (struct GRPSYL *) 0;
							gs_p = gs_p->prev) {
			starttime = radd(starttime, gs_p->fulltime);
		}
		/* find time up to last note of tuplet */
		endtime = starttime;
		for (gs_p = save_gs_p; gs_p->tuploc != ENDITEM
					&& gs_p->tuploc != LONEITEM;
					gs_p = gs_p->next) {
			endtime = radd(endtime, gs_p->fulltime);
		}
		/* add on a little bit for the final group of the tuplet */
		endtime = radd(endtime, smalltime);

		/* now check if other voice has space or not */
		othervoice = (gs_p->vno == 1 ? 1 : 0);
		if (hasspace(staff_p->groups_p [othervoice], starttime, endtime)
						== YES) {
			/* other voice is space: treat like V_1 */
			return(tupdir1voice(save_gs_p));
		}
		else {
			/* other voice not space: treat like V_2OPSTEM */
			if (gs_p->tupside != PL_UNKNOWN) {
				l_warning(gs_p->inputfile, gs_p->inputlineno,
					"tuplet side specification not valid when there are two voices");
				/* fix so we don't print error again if called
				 * again on this tuplet */
				gs_p->tupside = PL_UNKNOWN;
			}
			return(gs_p->vno == 1 ? PL_ABOVE : PL_BELOW);
		}
	}
}


/* return PL_ABOVE or PL_BELOW for tup location assuming a single voice */

static int
tupdir1voice(gs_p)

struct GRPSYL *gs_p;	/* first group of tuplet */

{
	int stemdirsum;	/* sum of stem directions to see if mostly up or down */


	/* if user specified a direction, the answer is easy */
	if (gs_p->tupside != PL_UNKNOWN) {
		return(gs_p->tupside);
	}

        /* Count up stem directions. Whichever side
         * has more stems, put it on that side. In case of tie,
         * arbitrarily choose above. */
	stemdirsum = 0;
	for (   ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {
		if (gs_p->grpcont == GC_NOTES && gs_p->grpvalue != GV_ZERO) {
			stemdirsum += (gs_p->stemdir == UP ? 1 : -1);
		}
		if (gs_p->tuploc == LONEITEM || gs_p->tuploc == ENDITEM) {
			break;
		}
	}

	return(stemdirsum >= 0 ? PL_ABOVE : PL_BELOW);
}


/* Go through measure, printing any beams. Gets called once
 * for normal sized notes, once for cue notes,
 * once for grace notes, and once for grace cue notes.
 */

static void
pr_beams(gs_p, grpvalue, grpsize)

struct GRPSYL *gs_p;	/* list of grpsyls for current measure
			 * of current voice */
int grpvalue;		/* GV_NORMAL, GV_ZERO */
int grpsize;		/* GS_NORMAL, GS_SMALL */

{
	struct GRPSYL *startbeam_p;	/* first in beam group */
	int t;				/* 8, 16, etc for basictimes */


	/* go through all the grpsyls in measure */
	for (   ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {

		/* skip until we find a STARTITEM
		 * on the relevant kind of group */
		if (gs_p->beamloc != STARTITEM || gs_p->grpvalue != grpvalue
					|| gs_p->grpsize != grpsize) {
			continue;
		}

		/* when there are cross-staff beams, we will find the beam
		 * on both staffs, but only need to draw it once. So skip
		 * it the second time */
		if (gs_p->beamto == CS_ABOVE) {
			continue;
		}

		/* find the matching ENDITEM */
		for (startbeam_p = gs_p; gs_p != 0 && (gs_p->beamloc != ENDITEM
					|| gs_p->grpvalue != grpvalue
					|| gs_p->grpsize != grpsize);
					gs_p = gs_p->next) {

		}
		if (gs_p == 0) {
			pfatal("pr_beams couldn't find end of beam group");
		}

		/* now go through beam group drawing beams for 8th notes,
		 * then 16th, etc */
		for (t = 8; t <= MAXBASICTIME; t <<= 1) {
			if (draw_beams(startbeam_p, gs_p, t, grpsize, grpvalue)
								<= 0) {
				break;
			}
		}
	}
}


/* In the case of cross-staff beams with the above staff's stems down,
 * and the below staff's stems up, we need to do extra work.
 * This function builds up a mesh of structs that represent the beams,
 * with a row of CSBINFO structs linked horizontally for each beam,
 * and vertical links at each stem. end_bm_offset() then uses this information
 * to figure out where along the stem a beam ends.
 * This function returns a pointer to the beginning of the 8th note beam.
 *
 * As an example, consider this input:
 *   1: 8.c; 64f beam with staff below; 32.s; 16e; 8s; 8e; 16s; 32.f; 64s ebm;
 *   2: 8.e; 64s beam with staff above; 32.a; 16s; 8g; 8s; 16g; 32.s; 64a ebm;
 * The resulting mesh will look like this:
 *                    .           .           .           .
 *                    .           .           .           .
 *       (64th)       X           .           .           .
 *                    |           .           .           .
 *       (32nd)       X --> X     .           .           .
 *                    |     |     .           .           .
 *       (16th)       X --> X --> X           .           X --> X   (32nd)
 *                    |     |     |           .           |     |
 *   return_value --> X --> X --> X --> X --> X --> X --> X --> X   (8th)
 *                          .           .           |     |     |
 *                          .           .           X --> X --> X   (16th)
 *                          .           .           .           |
 *                          .           .           .           X   (64th)
 *                          .           .           .           .
 *                          .           .           .           .
 *
 * Each X in the diagram represents a CSBINFO struct.
 * Each row represents a beam. The --> is the "next" field.
 * Each column represents a stem. It is a doubly-linked list,
 * using above_p and below_p fields.
 * The dots show the stem direction.
 */

static struct CSBINFO *
mkcsbmesh(begin_p, end_p)

struct GRPSYL *begin_p;	/* first group of cross-staff beam on upper staff */
struct GRPSYL *end_p;	/* 8th note beam goes from begin_p to end_p.
			 * There may be zero or more additional beams
			 * for shorter durations that span part or all
			 * of this list.
			 */

{
	struct CSBINFO *csbi_list_p;	/* this points to the 8th note beam
					 * list, which is what will
					 * ultimately be returned */
	struct CSBINFO *csbi_p;		/* the current information */
	struct CSBINFO *csbi8_p;	/* to walk through 8th list */
	struct CSBINFO *prevcsbi_p;	/* previous in horizontal list */
	struct CSBINFO *c_p;		/* for walking vertical lists */
	struct GRPSYL *gs_p;		/* to walk through beamed groups */
	int basictime;			/* 8, 16, 32, etc */
	int stemdir;			/* stem direction where beam starts */
	int shortest;			/* shortest basictime (8, 16, ...) */


	/* There is always at least an 8th note beam that goes the
	 * entire length, so make a list for that. */
	csbi_list_p = prevcsbi_p = 0;
	shortest = 8;
	for (gs_p = begin_p; gs_p != end_p->next; gs_p = nextbmgrp(gs_p,
						begin_p, end_p->next)) {
		MALLOC(CSBINFO, csbi_p, 1);

		/* set horizontal list links */
		if (csbi_list_p == 0) {
			/* first item on the horizontal list */
			csbi_list_p = csbi_p;
		}
		else {
			/* link from previous horizontally */
			prevcsbi_p->next = csbi_p;
		}
		prevcsbi_p = csbi_p;
		csbi_p->next = 0;

		/* init vertical list links */
		csbi_p->above_p = csbi_p->below_p = 0;

		/* this is for the 8th note beam */
		csbi_p->basictime = 8;
		/* save what group this is for, for later convenience */
		csbi_p->gs_p = gs_p;

		/* remember the shortest basictime anywhere in the beam */
		if (gs_p->basictime > shortest) {
			shortest = gs_p->basictime;
		}
	}

	/* For each additional beam, build up a row of structs representing
	 * that beam, and link it vertically to the row below or above it,
	 * depending on whether the first group of the beam is on the staff
	 * above or below the 8th beam.
	 */
	for (basictime = 16; basictime <= shortest; basictime <<= 1) {
		stemdir = UNKNOWN;	/* Init to keep lint happy;
					 * this will get set to appropriate
					 * value before it is actually used. */
		prevcsbi_p = 0;		/* No run of groups found yet */

		/* Walk through list, finding any runs of groups that are
		 * at least as short in duration as the current basictime
		 * we are looking for. Note this could be as little as a single
		 * group in the case of a partial beam.
		 * We walk through the GRPSYLs and their
		 * corresponding CSBINFO structs in parallel.
		 */
		for (gs_p = begin_p, csbi8_p = csbi_list_p;
				gs_p != end_p->next;
				gs_p = nextbmgrp(gs_p, begin_p, end_p->next),
				csbi8_p = csbi8_p->next) {

			/* nextbmgrp() always gives us the non-space group,
			 * but the begin_p (the first time through the 'for')
			 * could be a space, so we should use the other group
			 * in that case. */
			if (gs_p == begin_p && gs_p->grpcont == GC_SPACE) {
				do {
					if ((gs_p = gs_p->gs_p) == 0) {
						pfatal("can't find matching beam chord");
					}

				} while (gs_p->beamto != CS_ABOVE);
			}

			if (gs_p->basictime >= basictime) {
				/* this group is part of a beam of at least
				 * as short as the basictime of interest. */
				MALLOC(CSBINFO, csbi_p, 1);
				csbi_p->next = 0;
				csbi_p->basictime = basictime;

				/* If not first group in this beam,
				 * link from previous. If is first,
				 * save its stem direction. That determines
				 * which side of the 8th beam it goes on. */
				if (prevcsbi_p != 0) {
					prevcsbi_p->next = csbi_p;
				}
				else {
					stemdir = gs_p->stemdir;
				}
				/* Prepare to link more on horizonally,
				 * if beam goes further. */
				prevcsbi_p = csbi_p;

				/* set vertical links */
				if (stemdir == DOWN) {
					/* Must be from staff above.
					 * Find current top, and add
					 * above there */
					for (c_p = csbi8_p; c_p->above_p != 0;
							c_p = c_p->above_p) {
						;
					}
					c_p->above_p = csbi_p;
					csbi_p->below_p = c_p;
					csbi_p->above_p = 0;
				}
				else {
					/* similar for from staff below */
					for (c_p = csbi8_p; c_p->below_p != 0;
							c_p = c_p->below_p) {
						;
					}
					c_p->below_p = csbi_p;
					csbi_p->above_p = c_p;
					csbi_p->below_p = 0;
				}
			}
			else {
				/* If we were doing a beam before,
				 * it's done now */
				prevcsbi_p = 0;
			}
		}
	}
	return(csbi_list_p);
}


/* draw beams in a beam group for a particular time value, 8th, 16th, etc */
/* this gets called repeatedly, first for 8th, then 16ths, etc, until
 * there are no more shorter notes.
 * It returns the number of beams drawn (including partials) */


static int
draw_beams(gs_p, endbeam_p, basictime, grpsize, grpvalue)

struct GRPSYL *gs_p;			/* start of beam group */
struct GRPSYL *endbeam_p;		/* end of beam group */
int basictime;				/* draw beam for this basic time:
					 * 8, 16, 32, 64, etc */
int grpsize;				/* GS_NORMAL, GS_SMALL, or GS_TINY */
int grpvalue;				/* GV_NORMAL, GV_ZERO */

{
	int found = 0;		/* how many beams found to be drawn */
	int ngrps;		/* how many groups to beam together */
	struct GRPSYL *first_p = 0;/* first group in beam (the one on the
				 * above staff while doing cross-staff beams) */
	struct GRPSYL *begin_p = 0, *end_p;	/* the initialization is
				 * to shut up bogus compiler warning */
	struct GRPSYL *other_p;	/* other note that must be used to calculate
				 * slope of partial beam */
	float y_offset;		/* from end of stem to draw beam */
	int side;		/* left or right for partial beam */
	float x_begin, y_begin, x_other, y_other;	/* partial beam
							 * coordinates */
	double halfwidth;	/* half width of a beam */
	double end_y_offset;	/* to deal with cross staff beams */
	double slope;		/* of partial beam */
	double halfstem;
	double stemdist;	/* distance between stems */
	double pbeam_len;	/* length of partial beam */


	/* get relevant group, accounting for cross-staff beams */
	first_p = gs_p;
	gs_p = neighboring_note_beam_group(gs_p, first_p, NO, NO);

	/* go through the list */
	while ( gs_p != endbeam_p->next) {

		/* find however many in a row deserve to get another beam */
		for (end_p = (struct GRPSYL *) 0, ngrps = 0;
						(gs_p != endbeam_p->next);
						gs_p = nextbmgrp(gs_p,
						first_p, endbeam_p->next)) {

			/* if wrong type (e.g a grace inside of
			 * a set of normal notes), skip over */
			if (gs_p->grpsize != grpsize
					|| gs_p->grpvalue != grpvalue ) {
				continue;
			}

			/* if not beamed, skip */
			if ( gs_p->beamloc == NOITEM) {
				pfatal("non-beam inside beam group\n");
			}

			/* if this one deserves another beam,
			 * keep track of that. If not, break out */
			if (gs_p->basictime >= basictime) {
				end_p = gs_p;
				found++;
				if (ngrps == 0) {
					begin_p = gs_p;
				}
				ngrps++;
				if (gs_p->breakbeam == YES && basictime > 8) {
					break;
				}
			}
			else {
				break;
			}
		}

		/* prepare to do next one */
		if (gs_p != endbeam_p->next) {
			gs_p = nextbmgrp(gs_p, first_p, endbeam_p->next);
		}

		/* if none we looked at deserved a beam, keep looking */
		if (end_p == (struct GRPSYL *) 0) {
			continue;
		}

		/* calculate where on stem the beam should start */
		y_offset = beam_offset(numbeams(basictime),
					begin_p->grpsize, begin_p->stemdir);

		if (end_p->grpsize == GS_NORMAL) {
			halfwidth = W_WIDE * Staffscale / PPI / 2.0;
			halfstem = W_NORMAL * Staffscale / PPI / 2.0;
		}
		else if (end_p->grpsize == GS_SMALL) {
			halfwidth = W_WIDE * Staffscale * SM_FACTOR / PPI / 2.0;
			halfstem = W_NORMAL * Staffscale * SM_FACTOR / PPI / 2.0;
		}
		else {
			halfwidth = W_WIDE * Staffscale * TINY_FACTOR / PPI / 2.0;
			halfstem = W_NORMAL * Staffscale * TINY_FACTOR / PPI / 2.0;
		}

		/* check if single group.
		 * If so, need to do a partial beam, otherwise full beam */
		if (ngrps == 1) {
			/* rests and spaces don't get beams,
			 * so don't get partial ones */
			if (end_p->grpcont != GC_NOTES) {
				continue;
			}

			side = pbeamside(end_p, first_p);

			/* Now that we decided where the
			 * partial beam goes, we can draw it */

			/* in order to figure out the end point of the partial
			 * beam, we have to calculate the slope of the beam as
			 * if it were a full beam and derive from that where
			 * the partial beam will end. */
			/* determine whether to use prev or next note, and
			 * skip any notes of the wrong type! */
			if (side == PB_LEFT) {
				other_p = prevbmgrp(end_p, first_p);
			}
			else {
				other_p = nextbmgrp(end_p, first_p, end_p->next);
			}

			/* the line then goes from the stem (at y_offset) to
			 * a notehead width east or west of the stem,
			 * with the y coordinate calculated from the slope
			 * of what a full length beam would have been, unless
			 * stems are too close, in which case shorten it
			 * somewhat. */
			x_begin = find_x_stem(end_p);
			y_begin = find_y_stem(end_p);
			x_other = find_x_stem(other_p);
			y_other = find_y_stem(other_p);

			/* Guard against divide by zero */
			if (x_begin == x_other) {
				pfatal("found zero length beam when doing partial beams");
			}

			/* if cross-staff and the two stems are in opposite
			 * directions, have to compensate for that */
			if (end_p->stemdir == other_p->stemdir) {
				/* in same direction */
				slope = (y_other - y_begin)
							/ (x_other - x_begin);
			}
			else {
				double opp_adj;

				opp_adj = beam_offset(
					numbeams(other_p->basictime),
					other_p->grpsize, other_p->stemdir);
				slope = (y_other - (y_begin - opp_adj))
							/ (x_other - x_begin);
			}

			/* adjust to overlap stem */
			x_begin += halfstem * side;

			/* determine partial beam length */
			/* find distance between stems */
			if (x_begin < x_other) {
				stemdist = x_other - x_begin;
			}
			else {
				stemdist = x_begin - x_other;
			}
			/* if wide enough, use note head width, else less */
			if (stemdist < 5.0 * Stepsize) {
				pbeam_len = 0.4 * stemdist;
			}
			else {
				pbeam_len = widest_head(end_p) * Staffscale;
			}

			/* draw the partial beam */
			draw_parallelogram(x_begin, y_begin + y_offset,
				x_begin + pbeam_len * side,
				y_begin + y_offset + side *
				pbeam_len * slope, halfwidth);
		}

		else {
			/* draw a normal beam */

			/* For regular beams, can use y_offset directly,
			 * but with cross-staff beam, stems may be in opposite
			 * directions, so have to call a function to get
			 * appropriate offset.
			 */
			if (begin_p->beamto == CS_SAME) {
				end_y_offset = y_offset;
			}
			else {
				end_y_offset = end_bm_offset(first_p, end_p,
								basictime);
			}

			/* If the stems on both ends of the beam
			 * are zero length, don't draw any beams.
			 * If user really wants the beams,
			 * they can make one of the ends
			 * barely longer than zero.
			 */
			if (begin_p->stemlen <= 0.0 && end_p->stemlen <= 0.0) {
				continue;	
			}

			/* find end of first stem and last stem and draw
			 * the beam at proper offset from there */
			draw_parallelogram(find_x_stem(begin_p) - halfstem,
					find_y_stem(begin_p) + y_offset,
					find_x_stem(end_p) + halfstem,
					find_y_stem(end_p) + end_y_offset,
					halfwidth);
		}
	}
	return(found);
}


/* Figure out how far from the end of a stem a beam should be in the
 * case of a cross-staff beam with opposite-direction stems at its ends.
 * Will return some multiple (possibly 0) of the distance between beams,
 * with the proper sign to account for stem direction.
 * Should only be called if the beam in question is a cross-staff beam.
 */

double
end_bm_offset(top_first_p, end_p, basictime)

struct GRPSYL *top_first_p;	/* the group that has "bm with staff below" */
struct GRPSYL *end_p;		/* the group where a beam ends. This could be
				 * either a group with ebm or some intermediate
				 * group that happens to end a beam segment
				 * that is shorter. */
int basictime;			/* the basictime of the beam currently under
				 * consideration. The first beam drawn will
				 * be 8, the next 16, then 32, etc. */

{
	static struct CSBINFO *csbi_list_p = 0;/* Info about the cross beams */
	static struct GRPSYL *cached_gs_p = 0;	/* Each time we get a different
				 * top_first_p, we calculate its csbi_list
				 * and cache it for future calls. This lets
				 * us know if we can re-use the cached value. */
	struct CSBINFO *csbi_p;	/* for walking 8th note beam ->next links */
	struct CSBINFO *c_p;	/* for walking vertical links of mesh */
	int nbeams;		/* how many beams from the stem end */

	if (cached_gs_p != top_first_p) {
		/* Cached one is no good; need to recalculate */
		if (csbi_list_p != 0) {
			/* We had a list before; need to clean it up */
			struct CSBINFO *nextvert_p;	/* to free vert list */
			struct CSBINFO *nexthor_p;	/* to free hor list */
			/* walk horizontal list */
			for (csbi_p = csbi_list_p; csbi_p != 0;
							csbi_p = nexthor_p) {
				/* clean up vert list, both directions */
				for (c_p = csbi_p->above_p; c_p != 0;
							c_p = nextvert_p) {
					nextvert_p = c_p->above_p;
					FREE(c_p);
				}
				for (c_p = csbi_p->below_p; c_p != 0;
							c_p = nextvert_p) {
					nextvert_p = c_p->below_p;
					FREE(c_p);
				}
				nexthor_p = csbi_p->next;
				FREE(csbi_p);
			}
		}
		/* Calculate everything for current beam */
		csbi_list_p = mkcsbmesh(top_first_p, end_p);
		cached_gs_p = top_first_p;
	}

	/* First follow the 8th note CSBINFO list across till we find
	 * the one matching the end group. */
	for (csbi_p = csbi_list_p; csbi_p != 0 && csbi_p->gs_p != end_p;
							csbi_p = csbi_p->next) {
		;
	}
	if (csbi_p == 0) {
		pfatal("couldn't find beam end group in end_bm_offset()");
	}

	/* Now follow the vertical links until we find the right basic time.
	 * It could be on either side of the 8th beam.
	 * First we find the end of the stem, then count the number of
	 * links we have to follow to get to the one with the right basictime.
	 */
	if (end_p->stemdir == DOWN) {
		/* Must be from staff above, so end of stem is all the way			 * down the below_p list.
		 */
		for (c_p = csbi_p; c_p->below_p != 0; c_p = c_p->below_p) {
			;
		}
		/* Now count the number of beams till the one we want */
		for (nbeams = 1; c_p->basictime != basictime;
							c_p = c_p->above_p) {
			nbeams++;
		}
		if (c_p == 0) {
			pfatal("failed to find cross staff beam info go up");
		}
	}
	else {
		/* similar for staff below groups */
		for (c_p = csbi_p; c_p->above_p != 0; c_p = c_p->above_p) {
			;
		}
		/* Now count the number of beams till the one we want */
		for (nbeams = 1; c_p->basictime != basictime;
							c_p = c_p->below_p) {
			nbeams++;
		}
		if (c_p == 0) {
			pfatal("failed to find cross staff beam info go up");
		}
	}
	return (beam_offset(nbeams, end_p->grpsize, end_p->stemdir));
}


/* Given a group inside a beam, return the next group. Usually this will
 * be gs_p->next, but in the case of a cross-staff beam, it might be a
 * group on the other staff */

struct GRPSYL *
nextbmgrp(gs_p, first_p, endnext_p)

struct GRPSYL *gs_p;		/* find the beam group after this one */
struct GRPSYL *first_p;		/* The first group in the top staff of the
				 * beam */
struct GRPSYL *endnext_p;	/* what to return upon reaching the end of
				 * the beam. This will be the ->next field of
				 * the last group in the beam on the top staff
				 * of a cross-staff beam. Returning this lets
				 * legacy code (code before we supported
				 * cross-staff beams) keep working with minimal
				 * changes. */

{
	return(comm_next_beam_group(gs_p, first_p, endnext_p, NO));
}

/* Variation of the previous function. This one also returns embedded rests,
 * whereas the one above skips over rests. */

struct GRPSYL *
next_bm_grp_w_rests(gs_p, first_p, endnext_p)

struct GRPSYL *gs_p;		/* find the beam group after this one */
struct GRPSYL *first_p;		/* The first group in the top staff of the
				 * beam */
struct GRPSYL *endnext_p;	/* what to return upon reaching the end of
				 * beam */

{
	return(comm_next_beam_group(gs_p, first_p, endnext_p, YES));
}


/* The common code for the previous two functions */

static struct GRPSYL *
comm_next_beam_group(gs_p, first_p, endnext_p, rests_too)

struct GRPSYL *gs_p;		/* find the beam group after this one */
struct GRPSYL *first_p;		/* The first group in the top staff of the
				 * beam */
struct GRPSYL *endnext_p;	/* what to return upon reaching the end of
				 * the beam */
int rests_too;			/* If YES, return rests, else skip them */

{
	int grpsize, grpvalue;

	/* If we are passed the first group, it could be a space,
	 * in which case we need to use the below staff's group instead.
	 */
	if (gs_p->grpcont == GC_SPACE && gs_p->beamto != CS_SAME) {
		/* Need to hop to below staff. Go down the chord to find
		 * the matching cross-staff beam group. */
		do {
			if ((gs_p = gs_p->gs_p) == (struct GRPSYL *) 0) {
				pfatal("can't find matching beam chord");
			}

		/* skip any lyrics and such till we find the beamed-to group */
		} while (gs_p->beamto != CS_ABOVE);
	}

	/* need to skip past any groups of the wrong kind */
	grpsize = first_p->grpsize;
	grpvalue = first_p->grpvalue;
	do {
		/* Move to next group. If that gets us to the end
		 * of the measure, report that we're done. */
		if ((gs_p = gs_p->next) == (struct GRPSYL *) 0) {
			return(endnext_p);
		}
	} while (gs_p->grpsize != grpsize || gs_p->grpvalue != grpvalue);

	/* if past end of beam group, report that we're done */
	if (gs_p->beamloc != INITEM && gs_p->beamloc != ENDITEM) {
		return(endnext_p);
	}

	return(neighboring_note_beam_group(gs_p, first_p, NO, rests_too));
}


/* Given a group inside a beam (not the first),
 * return the previous group. Usually this will
 * be gs_p->prev, but in the case of a cross-staff beam, it might be a
 * group on the other staff */

struct GRPSYL *
prevbmgrp(gs_p, first_p)

struct GRPSYL *gs_p;		/* find the beam group after this one */
struct GRPSYL *first_p;		/* The first group in the top staff of the
				 * beam */

{
	return(comm_prev_beam_group(gs_p, first_p, NO));
}

/* Variation of the previous function. This one returns rests too,
 # whereas the one above skips over them */

struct GRPSYL *
prev_bm_grp_w_rests(gs_p, first_p)

struct GRPSYL *gs_p;		/* find the beam group after this one */
struct GRPSYL *first_p;		/* The first group in the top staff of the
				 * beam */

{
	return(comm_prev_beam_group(gs_p, first_p, YES));
}

/* The common code for the two functions above. */

struct GRPSYL *
comm_prev_beam_group(gs_p, first_p, rests_too)

struct GRPSYL *gs_p;		/* find the beam group after this one */
struct GRPSYL *first_p;		/* The first group in the top staff of the
				 * beam */
int rests_too;			/* If YES, return rests too, otherwise
				 * skip over them */

{
	int grpsize, grpvalue;
	int staffno;

	staffno = gs_p->staffno;

	/* need to skip past any groups of the wrong kind */
	grpsize = first_p->grpsize;
	grpvalue = first_p->grpvalue;
	do {
		/* Move to prev group. */
		if ((gs_p = gs_p->prev) == (struct GRPSYL *) 0) {
			pfatal("prevbmgrp couldn't find prev group");
		}
	} while (gs_p->grpsize != grpsize || gs_p->grpvalue != grpvalue);

	gs_p = neighboring_note_beam_group(gs_p, first_p, YES, rests_too);

	/* if we hopped staffs, then the space on the original staff might
	 * have been a long note, in which case the group we have isn't
	 * really the one we want. So we have to go forward on this new staff
	 * until we find the space that corresponds to the groups we started
	 * with, then back up one group from there. That's the one we want */
	if (staffno != gs_p->staffno) {
		/* we hopped staffs. Go forward to the next space */
		for (gs_p = gs_p->next; gs_p->grpcont != GC_SPACE;
							gs_p = gs_p->next) {
			;
		}
		/* now take the group right before the space */
		if (rests_too == YES) {
			gs_p = gs_p->prev;
		}
		else {
			do {
				gs_p = gs_p->prev;
			} while (gs_p->grpcont == GC_REST);
		}
	}
	return(gs_p);
}


/* Given a group in a beam, skip over any embedded rests.
 * Then if the group is not a space, return it as it is.
 * If it is a space, return the corresponding group on the staff
 * that this group is beamed to */

static struct GRPSYL *
neighboring_note_beam_group(gs_p, first_p, backwards, rests_too)

struct GRPSYL *gs_p;		/* find the beam group neighboring this one */
struct GRPSYL *first_p;		/* The first group in the top staff of the
				 * beam */
int backwards;			/* if YES, go backwards (find the previous
				 * group rather than the following) */
int rests_too;			/* If YES, also return any rests we find,
				 * otherwise skip over them */

{
	struct GRPSYL *tgs_p;	/* as we walk down a chord to try to find
				 * the group we're looking for, this keeps
				 * track of where we are */


	if (rests_too == NO) {
		while (gs_p->grpcont == GC_REST) {
			if (backwards == YES) {
				gs_p = gs_p->prev;
			}
			else {
				gs_p = gs_p->next;
			}
		}
	}
	if (gs_p == 0) {
		pfatal("neighboring_note_beam_group didn't find note group");
	}

	/* If this is a cross-staff beam, we may need to hop from
	 * staff to staff sometimes. If this group is a space
	 * group, then we have to hop now. */
	if (gs_p->grpcont == GC_SPACE) {
		if (gs_p->beamto == CS_SAME) {
			do {
				if (backwards == YES) {
					gs_p = gs_p->prev;
				} else {
					gs_p = gs_p->next;
				}
			} while (gs_p != 0 &&
				((rests_too == NO && gs_p->grpcont != GC_NOTES)
				|| (rests_too == YES && gs_p->grpcont == GC_SPACE)));
		}

		else if (gs_p->staffno == first_p->staffno) {
			/* Need to hop to below staff.
			 * Go down the chord to find
			 * the matching cross-staff beam group */
			do {
				if ((gs_p = gs_p->gs_p) ==
						(struct GRPSYL *) 0) {
					pfatal("can't find matching beam chord");
				}

			/* skip any lyrics and such till we find the
			 * group beamed to us */
			} while (gs_p->beamto != CS_ABOVE);
			if (gs_p->grpcont == GC_REST && rests_too == NO) {
				return(neighboring_note_beam_group(
					gs_p, first_p, backwards, rests_too));
			}
		}
		else {
			/* Need to jump back to staff above.
			 * Since the chord linked list is only one way (down)
			 * and we need to look up the chord, this is a
			 * little harder. Start at the first_p group, which
			 * is the first group in the beam on the above staff.
			 * Keep going down that staff until we find a chord
			 * linked down to gs_p. */
			struct GRPSYL * topgs_p;

			for (topgs_p = first_p; topgs_p != (struct GRPSYL *) 0;
						topgs_p = topgs_p->next) {

				/* walk down the chord */
				for (tgs_p = topgs_p->gs_p;
						tgs_p != (struct GRPSYL *) 0;
						tgs_p = tgs_p->gs_p) {

					if (tgs_p == gs_p) {
						/* Aha! We found it! */
						/* But if this is a rest,
						 * and we are skipping rests,
						 * have to keep looking */
						if (topgs_p->grpcont == GC_REST
								&& rests_too == NO) {
							return(neighboring_note_beam_group(
								topgs_p,
								first_p,
								backwards,
								rests_too));
						}
						return(topgs_p);
					}

					if (tgs_p->staffno > gs_p->staffno) {
						/* we're past the staff we care
						 * about, so this chord can't
						 * be the right one. */
						break;
					}
				}
			}

			pfatal("failed to find group when jumping back to above staff");
		}
	}

	return(gs_p);
}


/* given a GRPSYL that deserves a partial beam, return PB_LEFT if the beam
 * goes on the left or PB_RIGHT if is goes on the right.  */

int
pbeamside(gs_p, first_p)

struct GRPSYL *gs_p;
struct GRPSYL *first_p;

{
	int side;
	int beams2left, beams2right;	/* how many beams or dots for notes on
					 * either side of current group */
	struct GRPSYL *prevgs_p, *nextgs_p;


	/* need to figure out which side of stem to draw the
	 * partial beam. First the easy cases: if is STARTITEM,
	 * then it has to go on the right, if ENDITEM, it
	 * has to go on the left */
	switch (gs_p->beamloc) {
	case STARTITEM:
		side = PB_RIGHT;
		break;

	case ENDITEM:
		side = PB_LEFT;
		break;

	case INITEM:
		/* Hmmm. Will have to be more clever. Check the
		 * note on either side. If we're at a breakbeam,
		 * it's easy to know. Otherwise, if one should have more
		 * beams than the other, put the partial on that
		 * side */
		prevgs_p = prevbmgrp(gs_p, first_p);
		nextgs_p = nextbmgrp(gs_p, first_p, gs_p->next);
		beams2left = numbeams(prevgs_p->basictime);
		beams2right = numbeams(nextgs_p->basictime);
		if (gs_p->breakbeam == YES) {
			side = PB_LEFT;
		}
		else if (prevgs_p != 0 && prevgs_p->breakbeam == YES) {
			side = PB_RIGHT;
		}
		else if (beams2left > beams2right) {
			side = PB_LEFT;
		}
		else if (beams2right > beams2left) {
			side = PB_RIGHT;
		}

		/* That was inconclusive.  So now we're going to try to decide
		 * based on logical groupings of notes; that is, notes grouped
		 * according to what the accents should be. */
		else if (chkgroupings(&side, gs_p) == YES) {
			/* it found an answer and set "side" for us */
			;
		}
		else {
			/* ok. that didn't help.
			 * See if the notes on either side
			 * have more dots than the other.
			 * If so, put the partial towards
			 * that one. If they are the same, then
			 * throw in the towel and just stick it
			 * on the left */
			beams2left = prevgs_p->dots;
			beams2right = nextgs_p->dots;
			if (beams2right > beams2left) {
				side = PB_RIGHT;
			}
			else {
				side = PB_LEFT;
			}
		}
		break;

	default:
		pfatal("invalid beamloc passed to pbeamside");
		/*NOTREACHED*/
		return(PB_LEFT);	/* to shut up bogus compiler warning */
	}

	return(side);
}

/*
 * Name:        chkgroupings()
 *
 * Abstract:    Decide partial beam side based on groupings of notes.
 *
 * Returns:     YES if it found an answer (stored in *side_p), NO if not
 *
 * Description: This function breaks the measure down into successively
 *		smaller pieces based on where the accents should be, trying to
 *		find a piece where the current GRPSYL falls at the beginning or
 *		end of the piece.  If the GRPSYL falls at the start of a piece,
 *		its partial beam should point right; if end, left.  If we get
 *		to the point where the pieces are shorter than the GRPSYL
 *		itself, we have failed.
 */

static int
chkgroupings(side_p, thisgs_p)

int *side_p;			/* where to put the answer, if found */
struct GRPSYL *thisgs_p;	/* the GRPSYL we are working on */

{
	struct GRPSYL *gs_p;	/* point along GRPSYL list */
	short *factors;         /* array to be malloc'ed */
	int n;			/* loop variable */
	RATIONAL thisstart;	/* time offset in measure of thisgs_p */
	RATIONAL nextstart;	/* time offset in measure of next GRPSYL */
	RATIONAL quotient;	/* temp variable for dividing */
	RATIONAL grouplen;	/* time length of a grouping */
	RATIONAL tupstart;	/* time offset where tuplet starts */
	RATIONAL tupdur;	/* time length of a tuplet */
	int counts;		/* count in the current grouplen */
	int fraction;		/* is grouplen a fraction of a count? */
	int fact;		/* a factor */


	/*
	 * If we're doing grace beams, skip this whole thing, since we're
	 * dealing with time values, and they are all zero.
	 */
	if (thisgs_p->grpvalue == GV_ZERO) {
		return (NO);
	}

	/* find the time offset of thisgs_p by adding up all previous GRPSYLs*/
	thisstart = Zero;
	for (gs_p = thisgs_p->prev; gs_p != 0; gs_p = gs_p->prev) {
		thisstart = radd(thisstart, gs_p->fulltime);
	}

	/* find offset of GRPSYL following thisgs_p */
	nextstart = radd(thisstart, thisgs_p->fulltime);

	/*
	 * Interior notes of tuplets are dealt with in a special way.
	 */
	if (thisgs_p->tuploc == INITEM) {
		/*
		 * Find the duration of the tuplet by adding up all the
		 * previous GRPSYLs in the tuplet and this GRPSYL and all the
		 * later GRPSYLs.  (The loops stop when they hit a NOITEM
		 * that's not grace.)
		 */
		tupdur = Zero;
		for (gs_p = thisgs_p->prev; gs_p != 0 &&
				(gs_p->grpvalue == GV_ZERO ||
				gs_p->tuploc != NOITEM); gs_p = gs_p->prev) {
			tupdur = radd(tupdur, gs_p->fulltime);
		}
		/* remember where tuplet starts */
		tupstart = rsub(thisstart, tupdur);
		for (gs_p = thisgs_p; gs_p != 0 &&
				(gs_p->grpvalue == GV_ZERO ||
				gs_p->tuploc != NOITEM); gs_p = gs_p->next) {
			tupdur = radd(tupdur, gs_p->fulltime);
		}

		/*
		 * If the starting point of this tuplet is not at a multiple of
		 * its duration, we consider the tuplet synchopated.  This is
		 * pretty bizarre and not worth trying to deal with.
		 */
		quotient = rdiv(tupstart, tupdur);
		if (quotient.d != 1) {
			return (NO);
		}

		/* the first group length to consider is tupdur/tupcont */
		grouplen = tupdur;
		grouplen.d *= thisgs_p->tupcont;
		rred(&grouplen);

		/* loop until an answer is found, or we give up */
		for (;;) {
			/*
			 * If the group length is not longer than our note, it
			 * makes no sense to try to see if our note is at the
			 * start or end of such a group.  Maybe we never hit a
			 * match because our note is syncopated.  Whatever the
			 * reason, we have to give up.
			 */
			if (LE(grouplen, thisgs_p->fulltime)) {
				return (NO);
			}

			/*
			 * If thisstart/grouplen is an integer, it means
			 * thisgs_p is on a grouping boundary; that is, it is
			 * the first GRPSYL in a grouping.  So point right.
			 */
			quotient = rdiv(thisstart, grouplen);
			if (quotient.d == 1) {
				*side_p = PB_RIGHT;
				return (YES);
			}

			/*
			 * If nextstart/grouplen is an integer, it means the
			 * GRPSYL after thisgs_p is on a grouping boundary,
			 * which means that thisgs_p is the last GRPSYL in a
			 * grouping.  So point left.
			 */
			quotient = rdiv(nextstart, grouplen);
			if (quotient.d == 1) {
				*side_p = PB_LEFT;
				return (YES);
			}

			/* divide grouplen by 2 and try again */
			grouplen = rdiv(grouplen, Two);
		}
	}

	/*
	 * This is the normal case, not the interior of a tuplet.
	 */

	/* get all the prime factors of the time sig's numerator */
	factors = factor(Score.timenum);

	grouplen = Score.time;		/* first group is the whole measure */
	counts = Score.timenum;		/* number of counts in measure */

	/*
	 * Loop until we find an answer, or until we have to give up.  Each
	 * time through the loop, we reduce the grouping length.  At first, we
	 * divide out prime factors from the number of counts in the measure.
	 * Once we get down to one count, we start dividing by 2 all the time.
	 */
	for (;;) {
		fraction = YES;		/* default to "fraction of a count" */

		/* if there are still multiple counts, divide out a prime */
		if (counts > 1) {
			/*
			 * See if there are any prime factors greater than 4.
			 * This only happens with funny timesigs like 10/8 or
			 * 7/4.  We work down from the top, because the
			 * likelyhood is that the highest level grouping is by
			 * the biggest factor, when these funny numbers are
			 * involved.  At least 10/8, for example, is normally
			 * 5 groups of 2, not 2 groups of 5.
			 */
			for (n = Score.timenum; n > 4 && factors[n] == 0; n--)
				;
			/* if we found a 5 or greater, use it */
			if (n > 4) {
				factors[n]--;
				fact = n;
			/*
			 * There are no funny factors (5 or more) left.  Next,
			 * we need to look for 2s, not 3s yet, because, for
			 * example, 6/8 is 2 groups of 3, not 3 groups of 2.
			 */
			} else if (counts % 2 == 0) {
				factors[2]--;
				fact = 2;
			/* no 2s either, so look for 3s */
			} else if (counts % 3 == 0) {
				factors[3]--;
				fact = 3;
			} else {
			/* no factors left, so flag it by setting fact to 1 */
				fact = 1;
			}

			/*
			 * If a factor was found, divide it out, and remember
			 * that we are not yet dealing with fractions of a
			 * single count.
			 */
			if (fact > 1) {
				counts /= fact;
				fraction = NO;
			}
		}

		if (fraction == YES) {
			/*
			 * We are dealing with fractions of a count, so divide
			 * by 2 from now on.
			 */
			grouplen = rdiv(grouplen, Two);
		} else {
			/*
			 * Using the number of counts remaining, form the
			 * length in lowest terms.
			 */
			grouplen.n = counts;
			grouplen.d = Score.timeden;
			rred(&grouplen);
		}

		/*
		 * If the group length is not longer than our note, it makes no
		 * sense to try to see if our note is at the start or end of
		 * such a group.  Maybe we never hit a match because our note
		 * is syncopated.  Whatever the reason, we have to give up.
		 */
		if (LE(grouplen, thisgs_p->fulltime)) {
			return (NO);
		}

		/*
		 * If thisstart/grouplen is an integer, it means thisgs_p is on
		 * a grouping boundary; that is, it is the first GRPSYL in a
		 * grouping.  So point right.
		 */
		quotient = rdiv(thisstart, grouplen);
		if (quotient.d == 1) {
			*side_p = PB_RIGHT;
			return (YES);
		}

		/*
		 * If nextstart/grouplen is an integer, it means the GRPSYL
		 * after thisgs_p is on a grouping boundary, which means that
		 * thisgs_p is the last GRPSYL in a grouping.  So point left.
		 */
		quotient = rdiv(nextstart, grouplen);
		if (quotient.d == 1) {
			*side_p = PB_LEFT;
			return (YES);
		}
	}

	return (NO);		/* we can never get here; this is for lint */
}


/* print a multirest */

void
pr_multirest(gs_p, mll_p)

struct GRPSYL *gs_p;	/* info about the multirest */
struct MAINLL *mll_p;

{
	double x;		/* horizontal position of number string */
	double y, y_offset;	/* vertical location */
	double height, width;	/* of meas num string */
	float east, west;	/* edges of the multirest */
	char *numstr;		/* ASCII version of numbers of measures */
	struct STAFF *staff_p;


	/* avoid core dumps */
	if (Score_location_p == (float *) 0) {
		pfatal("can't do multirest: no feed");
		return;
	}

	/* Avoid re-printing multirest on voice 2 or 3
	 * if it was already printed on a earlier voice. */
	if ( (gs_p->vno > 1) &&
			(vvpath(gs_p->staffno, 1, VISIBLE)->visible == YES)) {
		return;
	}
	if ( (gs_p->vno > 2) &&
			(vvpath(gs_p->staffno, 2, VISIBLE)->visible == YES)) {
		return;
	}

	staff_p = mll_p->u.staff_p;

	/* determine where to place the multirest */
	y = mr_y_loc(gs_p->staffno);
	east = gs_p->c[AE];
	west = gs_p->c[AW];

	/* If user wants us to use the alternative multirest style of using
	 * rest symbols (often used in orchestral music), and the length of
	 * the multirest is 8 or less, we use that alternate style,
	 * otherwise draw horizontal line along middle staff and two
	 * vertical lines near the bar lines. Note that the basictime is
	 * the negative of the number of measures of multirest.	 
	 * We have seen rare examples of using the alternate style for all the
	 * way up to 11 measures, but consider normal usage only up to 8.
	 */
	if (svpath(staff_p->staffno, RESTSYMMULT)->restsymmult == YES &&
						gs_p->basictime > -9) {
		double center;	/* can't use AX, must use avg of AE and AW */
		int size;	/* actually will always be normal size,
				 * but may as well make code be able to handle
				 * small size just in case... */

		center = (gs_p->c[AE] + gs_p->c[AW]) / 2.0;
		size = size_def2font(gs_p->grpsize);

		switch (gs_p->basictime) {
		case -2:
			pr_muschar(center, gs_p->c[AY], C_DWHREST, size, FONT_MUSIC);
			break;
		case -3:
			pr_muschar(gs_p->c[AW], gs_p->c[AY], C_DWHREST, size, FONT_MUSIC);
			pr_muschar(gs_p->c[AE], gs_p->c[AY], C_1REST, size, FONT_MUSIC);
			break;
		case -4:
			pr_muschar(center, gs_p->c[AY], C_QWHREST, size, FONT_MUSIC);
			break;
		case -5:
			pr_muschar(gs_p->c[AW], gs_p->c[AY], C_QWHREST, size, FONT_MUSIC);
			pr_muschar(gs_p->c[AE], gs_p->c[AY], C_1REST, size, FONT_MUSIC);
			break;
		case -6:
			pr_muschar(gs_p->c[AW], gs_p->c[AY], C_QWHREST, size, FONT_MUSIC);
			pr_muschar(gs_p->c[AE], gs_p->c[AY], C_DWHREST, size, FONT_MUSIC);
			break;
		case -7:
			pr_muschar(gs_p->c[AW], gs_p->c[AY], C_QWHREST, size, FONT_MUSIC);
			pr_muschar(center, gs_p->c[AY], C_DWHREST, size, FONT_MUSIC);
			pr_muschar(gs_p->c[AE], gs_p->c[AY], C_1REST, size, FONT_MUSIC);
			break;
		case -8:
			pr_muschar(gs_p->c[AW], gs_p->c[AY], C_QWHREST, size, FONT_MUSIC);
			pr_muschar(gs_p->c[AE], gs_p->c[AY], C_QWHREST, size, FONT_MUSIC);
			break;
		default:
			pfatal("restsymmult with illegal number of measures (%d)",
					-(gs_p->basictime) );
			break;
		}
	}
	else {
		/* draw vertical lines at each end */
		do_linetype(L_MEDIUM);

		draw_line(west, y - (2.0 * Stepsize), west, y + (2.0 * Stepsize));
		draw_line(east, y - (2.0 * Stepsize), east, y + (2.0 * Stepsize));

		/* draw heavy horizontal */
		do_linetype(L_WIDE);
		draw_line(west, y, east, y);
	}

	if (svpath(staff_p->staffno, PRINTMULTNUM)->printmultnum == YES) {
		/* print number of measures */
		numstr = mr_num(mll_p, &x, &y_offset, &height, &width);
		pr_string(x, y + y_offset, numstr, J_LEFT, (char *) 0, -1);
	}
}


/* Given a MAINLL that point to a STAFF that  points to a multirest
 * or single/double/quad measure repeat GRPSYL,
 * return a string for its number of measures, to be printed,
 * and return via pointers its x, relative y, height, and width
 * In the case of dbl or quad measure repeats, a NULL is returned if
 * the STAFF for a measure which is not the one for which the following
 * bar line gets the symbol, and the pointer values are not valid.
 */

char *
mr_num(mainll_p, x_p, y_offset_p, height_p, width_p)

struct MAINLL *mainll_p;
double *x_p;		/* return where number starts horizontally */
double *y_offset_p;	/* return y relative to staff */
double *height_p;	/* return height of string */
double *width_p;	/* return width of string */

{
	struct STAFF *staff_p;
	struct GRPSYL *gs_p = 0;/* initialize to avoid compiler warning */
	char *numstr;		/* ASCII version of number of measures */
	int v;			/* voice index */


	if (mainll_p->str != S_STAFF) {
		pfatal("mr_num passed incorrect struct type %d", mainll_p->str);
	}
	staff_p = mainll_p->u.staff_p;

	/* skip over invisible voices */
	for (v = 0; v < MAXVOICES; v++) {
		if (vvpath(staff_p->staffno, v + 1, VISIBLE)->visible == YES) {
			gs_p = staff_p->groups_p[v];
			break;
		}
	}
	if (v == MAXVOICES) {
		pfatal("no visible voice found by mr_num");
	}

	/* Return null for multi repeat bars not having the symbol */
	if ( (gs_p->meas_rpt_type == MRT_DOUBLE)
				&& (staff_p->mult_rpt_measnum != 1) ) {
		return (char *) 0;
	}
	if ( (gs_p->meas_rpt_type == MRT_QUAD)
				&& (staff_p->mult_rpt_measnum != 2) ) {
		return (char *) 0;
	}

	if (gs_p->meas_rpt_type != MRT_NONE) {
		/* this is a measure repeat */
		if (gs_p->meas_rpt_type == MRT_SINGLE) {
			numstr = num2str(staff_p->mrptnum);
			numstr[0] = FONT_TR;
			numstr[1] = 11;
		}
		else {
			numstr = num2str(gs_p->meas_rpt_type == MRT_DOUBLE ? 2 : 4);
			numstr[0] = FONT_NB;
			numstr[1] = 16;
		}
	}
	else if (gs_p->grpcont == GC_REST) {
		/* this is a multi-rest */
		numstr = num2str( -(gs_p->basictime) );
		/* want these in bigger size */
		/* Essential Dictionary of Music Notation says this
		 * should be in the same size and font as time signature. */
		numstr[0] = FONT_NB;
		numstr[1] = 16;
	}
	else {
		pfatal("wrong group type (%d) passed to mr_num, line %d, staff %d, voice %d", gs_p->grpcont, gs_p->inputlineno, gs_p->staffno, gs_p->vno);
		/*NOTREACHED*/
		return (char *) 0;	/* to shut up bogus compiler warning */
	}
	numstr[1] = (char) adj_size((int) numstr[1], Staffscale, (char *) 0, -1);

	*width_p = strwidth(numstr);
	*height_p = strheight(numstr);

	if ( (gs_p->meas_rpt_type == MRT_DOUBLE)
				|| (gs_p->meas_rpt_type == MRT_QUAD) ) {
		struct MAINLL *m_p;	/* for finding next bar line */

		/* search forward in main list for next bar line */
		for (m_p = mainll_p->next; m_p != 0; m_p = m_p->next) {
			if (m_p->str == S_BAR) {
				break;
			}
		}
		if (m_p == 0) {
			pfatal("could not find the following bar in mr_num");
		}

		/* x is the x of the next bar line,
		 *  minus 1/2 of number string width */
		*x_p = m_p->u.bar_p->c[AX] - (*width_p / 2.0);
	}
	else {
		/* x is middle of measure minus 1/2 of number string width */
		*x_p = ((gs_p->c[AE] + gs_p->c[AW]) / 2.0) - (*width_p / 2.0);
	}

	/* y offset is just above staff */
	*y_offset_p = halfstaffhi(gs_p->staffno) + Stepsize;
	return(numstr);
}


/* given a number, return pointer to string version (with font/size in first
 * 2 bytes. Points to static area overwritten on each call, so if you need a
 * unique copy of it, use copy_string(). Always makes a string in Roman in
 * the default size. */

char *
num2str(num)

int num;	/* the number to convert */

{
	static char numstr[12];

	/* get ASCII version of number */
	(void) sprintf(numstr, "%c%c%d", FONT_TR,
			adj_size(DFLT_SIZE, Staffscale, (char *) 0, -1), num);
	return(numstr);
}


/* print cresc or decresc */

static void
pr_cresc(stuff_p)

struct STUFF *stuff_p;	/* info about what to print and where */

{
	float x1, x2;			/* x coords of west and east points */
	float line1y1, line2y1;		/* y coords of west points */
	float line1y2, line2y2;		/* y coords of east points */


	do_linetype(L_NORMAL);

	/* get coords for point and midpoint of open end */
	x1 = stuff_p->c[AW];
	x2 = stuff_p->c[AE];
	/* Note that since any horzscale effect has already been applied,
	 * reducing the east-west box dimension as appropriate,
	 * we don't need to worry about horzscale here. */
	if (stuff_p->stuff_type == ST_CRESC) {
		line1y1 = line2y1 = (stuff_p->c[AN] + stuff_p->c[AS]) / 2.0;
		/* adjust by 1 point to allow some vertical padding */
		line1y2 = stuff_p->c[AN] - (1.0 * Stdpad);
		line2y2 = stuff_p->c[AS] + (1.0 * Stdpad);
	}
	else if (stuff_p->stuff_type == ST_DECRESC) {
		line1y2 = line2y2 = (stuff_p->c[AN] + stuff_p->c[AS]) / 2.0;
		line1y1 = stuff_p->c[AN] - (1.0 * Stdpad);
		line2y1 = stuff_p->c[AS] + (1.0 * Stdpad);
	}
	else {
		pfatal("pr_cres called for something other than cresc/decresc");
		/*NOTREACHED*/
		return;	/* to shut up bogus compiler warning about unused variables */
	}

	/* draw the two sides of the hairpin */
	draw_line(x1, line1y1, x2, line1y2);
	draw_line(x1, line2y1, x2, line2y2);
}


/* if a STUFF has a til clause, may need to extend out. If a trill, extend
 * with wavy line. If octave, use dashed line.  If strings ends with a ~,
 * use a wavy line. If ends with an underscore or is figbass, use
 * underline. For everything else, put out periodic dashed. */

static void
extend(stuff_p)

struct STUFF *stuff_p;	/* a stuff which may have a til clause */

{
	float extlen;		/* length of extension */
	float y;		/* vertical position */
	float x;		/* horizontal position */
	float segment;		/* length of dash + white space */
	char *dash;		/* dash in proper font/size */
	char lch;		/* last character of string */
	float adjusted_staffscale;	/* If string ends with something
				 * other than default size, we pretend the
				 * staffscale has changed, so the extender
				 * will get adjusted appropriately. */
	float saved_staffscale;
	int font, size;		/* at end of string */


	/* Find out how long an extender we need, if any at all */
	if ((extlen = extwidth(stuff_p)) <= 0.0) {
		return;
	}

	y = stuff_p->c[AY];

	end_fontsize(stuff_p->string, &font, &size);
	saved_staffscale = Staffscale;
	/* If Staffscale is not 1.0, then the string size will have
	 * been adjusted to be right for that scale. So if we divide
	 * the size we get got by Staffscale, that should tell us
	 * approximately what size the user specified. Then for the purposes
	 * of drawing lines, we adjust the effective staffscale by
	 * the ratio of what they specified differs to the default.
	 */
	adjusted_staffscale = Staffscale * ((float) size / Staffscale) / (float) DFLT_SIZE;

	if (string_is_sym(stuff_p->string, C_TR, FONT_MUSIC) == YES) {
		/* special case of a trill */
		if ( extlen < Stepsize) {
			/* too short to bother */
			return;
		}

		y += (0.5 * strascent(stuff_p->string));
		Staffscale = adjusted_staffscale;
		draw_wavy(stuff_p->c[AE] - extlen, y, stuff_p->c[AE], y);
		Staffscale = saved_staffscale;
		return;
	}

	else if ((lch = last_char(stuff_p->string)) == '~') {
		y += strascent(stuff_p->string) / 2.0;
		Staffscale = adjusted_staffscale;
		draw_wavy(stuff_p->c[AE] - extlen, y, stuff_p->c[AE], y);
		Staffscale = saved_staffscale;
		return;
	}

	else if (lch == '_' || stuff_p->modifier == TM_FIGBASS) {
		Staffscale = adjusted_staffscale;
		do_linetype(L_NORMAL);
		draw_line(stuff_p->c[AE] - extlen, y, stuff_p->c[AE], y);
		Staffscale = saved_staffscale;
		return;
	}

	else if (stuff_p->stuff_type == ST_OCTAVE) {

		if ( extlen < (4.0 * Stepsize)) {
			/* too short to bother */
			return;
		}

		y += (1.5 * Stepsize);
		do_linetype(L_DASHED);
		/* Note that octave size is fixed, so we don't need to
		 * compensate for some other size. */
		draw_line(stuff_p->c[AE] - extlen + (2.0 * Stepsize), y,
						stuff_p->c[AE], y);

		/* vertical line at end unless carried to next score */
		if (stuff_p->carryout == NO) {
			draw_line(stuff_p->c[AE], y, stuff_p->c[AE],
				y + (3.0 * Stepsize *
				(stuff_p->place == PL_ABOVE ? -1.0 : 1.0)));
		}

		/* put linetype back to solid so some other music character that
		 * uses a line won't get messed up */
		do_linetype(L_NORMAL);
		return;
	}

	dash = dashstr(stuff_p->string);
	segment = (3.0 * strwidth(dash));
	for ( x = stuff_p->c[AE] - extlen + (2.0 * segment) / 3.0;
					x < stuff_p->c[AE]; x += segment) {
		/* Note that dashstr adjusts for size, so we don't need
		 * to adjust Staffscale for that */
		pr_string(x, y, dash, J_LEFT, (char *) 0, -1);
	}
	FREE(dash);
}


/* Calculate how wide the "extender" is for a STUFF string. This is the
 * line or dashes that go from end of string to end of "til." Note that
 * this should not be called prior to restsyl(), because Staffscale doesn't
 * gets applied to STUFFs until then. */

double
extwidth(stuff_p)

struct STUFF *stuff_p;

{
	if (stuff_p->string == 0 ||
			(stuff_p->end.bars == 0 && stuff_p->end.count == 0.0
			&& stuff_p->end.steps == 0.0)) {
		/* no string or no til clause, so no extender */
		return(0.0);
	}

	return(stuff_p->c[AE] - stuff_p->c[AW]
			- strwidth(stuff_p->string) * stuff_p->horzscale);
}


/* Some characters have upside-down versions that are used
 * if stem is down. This table maps such characters to their flipped versions.
 */

static struct MIRRCHAR {
	int	font;		/* Which music font. Note that both the
				 * normal and inverted characters must be
				 * in the same font. (We could relax
				 * this constraint by storing a font for each,
				 * and returning both character and font,
				 * but there's no hardship in this simple way.)
				 */
	char	norm;
	char	inverted;
} mirrtbl[] = {
	{ FONT_MUSIC, C_FERM, C_UFERM },
	{ FONT_MUSIC, C_ACC_HAT, C_ACC_UHAT },
	{ FONT_MUSIC, C_WEDGE, C_UWEDGE },
	{ 0, 0 }
};


/* Given a string and the first character in it, if it is a music symbol
 * that has a mirrored version, return that, otherwise, return
 * it as it was.
 */

static int
mirror(str, ch, font)

char *str;	/* the string to check */
int ch;		/* the first character (which better be a music character) */
int font;	/* FONT_MUSIC or some other music font */

{
	int i;

	for (i = 0; mirrtbl[i].norm != '\0'; i++) {
		if (string_is_sym(str, mirrtbl[i].norm, mirrtbl[i].font) == YES) {
			return((int) mirrtbl[i].inverted);
		}
	}
	return(ch);
}


static double
size2flagsep(size)

int size;	/* GS_NORMAL, GS_SMALL, or GS_TINY */

{
	switch(size) {
	default:
		pfatal("unexpected grpsize %d in size2flagsep", size);
		/*NOTREACHED*/
		/*FALLTHRU*/
	case GS_NORMAL:
		return(FLAGSEP);
	case GS_SMALL:
		return(SMFLAGSEP);
	case GS_TINY:
		return(TINYFLAGSEP);
	}
}
