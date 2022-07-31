
/*
 Copyright (c) 1995-2022  by Arkkra Enterprises.
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

/* This file contains miscellaneous utility functions for the Mup
 * music publication program, mostly for the print phase.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/* Mapping of string() transform names to functions */
struct STRFUNC {
	char *name;		/* For string(num, name) ... */
	char * (*func)(int);	/* ...call (*func)(num) */
};

/* Table of supported string transform functions.
 * IMPORTANT: The table must be sorted on the first field, so bsearch will work!
 */
static char * num2uletter P((int num));
static char * num2lletter P((int num));
static char * num2uroman P((int num));
static char * num2lroman P((int num));
static struct STRFUNC Trans_table[] = {
	{ "LET", num2uletter },
	{ "ROM", num2uroman },
	{ "let", num2lletter },
	{ "rom", num2lroman }
};
static int Trans_elements = NUMELEM(Trans_table);

static int prev_has_tie P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
static void chk_tie_out_oct P((struct GRPSYL *gs_p, RATIONAL total_time,
	double oct_end_count, char *filename, int lineno));
static void grp_octave_adjust P((struct GRPSYL *gs_p, int adj, struct MAINLL *mll_p));
static void set_height_blockhead P((struct BLOCKHEAD *blockhead_p,
		UINT32B context, struct MAINLL *mll_p));



/* set _cur location variable to specified value */

void
set_cur(x, y)

float x, y;	/* x, east, and west get set to x value. y, north and south,
		 * get set to y value */

{
	float *cur_p;	/* coord array for _cur in current context */


	/* look up _cur symbol and fill in the values */
	cur_p = symval("_cur", (float **) 0);
	cur_p[AX] = cur_p[AE] = cur_p[AW] = x;
	cur_p[AY] = cur_p[AN] = cur_p[AS] = y;
}


/* set the values for location variable _win */

void
set_win(n, s, e, w)

float n, s, e, w;	/* north, south, east, and west */

{
	float *window;	/* coordinate info for _win */


	/* look up symbol and fill in values */
	window = symval("_win", (float **) 0);
	window[AN] = n;
	window[AS] = s;
	window[AE] = e;
	window[AW] = w;
	/* set x and y to midpoints of rectangle */
	window[AY] = s + (n - s)/2.0;
	window[AX] = w + (e - w)/2.0;
}


/* Return width of a bar line. Allow a couple pixels on either side
 * of the actual line(s) [and dots]. Does not include user padding.
 * Since an invisbar has no lines or dots, it has zero width.
 */

double
width_barline(bar_p)

struct BAR *bar_p;	/* return width of this bar line */

{
	if (bar_p == (struct BAR *) 0) {
		return(0.0);
	}

	switch(bar_p->bartype) {

	case SINGLEBAR:
		return(Score.staffscale * 7 * STDPAD);

	case DOUBLEBAR:
		return(Score.staffscale * 9 * STDPAD);

	case ENDBAR:
		return(Score.staffscale * 12 * STDPAD);

	case REPEATSTART:
	case REPEATEND:
		return(Score.staffscale * 16 * STDPAD);

	case REPEATBOTH:
		return(Score.staffscale * 19 * STDPAD);

	case RESTART:
		return(Score.staffscale * 2.0 * HALF_RESTART_WIDTH);

	case INVISBAR:
		return(0.0);

	default:
		pfatal("bad bar type");
		/*NOTREACHED*/
		break;
	}

	/*NOTREACHED*/
	return(0.0);
}

/* Similar to previous function, but for subbar */

double
width_subbar(subbar_app_p)

struct SUBBAR_APPEARANCE *subbar_app_p;

{
	/* Values are like for reqular bars except we add 3 STDPADs on
	 * each side for padding so they won't be right against notes. */
	if (subbar_app_p->bartype == SINGLEBAR) {
		return(13.0 *STDPAD);
	}
	else if (subbar_app_p->bartype == DOUBLEBAR) {
		return(15.0 *STDPAD);
	}
	else {
		pfatal("unknown subbar type %d", subbar_app_p->bartype);
		/*NOTREACHED*/
		return(0.0);
	}
}


/* Normally, we want some padding on both sides of a bar line,
 * but at the end of a staff, we don't want right padding.
 * This applies either if we are at the right
 * margin or if the next bar is a restart.
 * This function returns how much to adjust an end-of-score bar line
 * eastward to make it at the right edge of the score.
 */

double
eos_bar_adjust(bar_p)

struct BAR *bar_p;	/* the bar to adjust */

{
	double halfbarwidth;

	halfbarwidth = width_barline(bar_p) / 2.0;
	switch (bar_p->bartype) {
	case DOUBLEBAR:
		return(halfbarwidth - STDPAD - (Score.staffscale * W_NORMAL / PPI / 2.0));
	case SINGLEBAR:
		return(halfbarwidth - (Score.staffscale * W_NORMAL / PPI / 2.0));
	case REPEATEND:
		return(halfbarwidth - (4.0 * STDPAD) - (Score.staffscale * W_WIDE / PPI /  2.0));
	case ENDBAR:
		return(halfbarwidth - (2.0 * STDPAD) - (Score.staffscale * W_WIDE / PPI / 2.0));
	default:
		break;
	}
	return(0.0);
}


/* width of clef, keysig, timesig */

double
width_clefsig(mainll_p, clefsig_p)

struct MAINLL *mainll_p;
struct CLEFSIG *clefsig_p;	/* return width of this clefsig */

{
	/* we just call the routine to print a clefsig, but with
	 * flag to tell it to not really print */
	return(pr_clefsig(mainll_p, clefsig_p, NO));
}


/* Translate clef name to clef output character, accounting for shape overides */

int
clefchar(clef, staffno, font_p)

int clef;
int staffno;
int *font_p;	/* symbol's font is returned here */

{
	int clefcode;

	switch(clef) {

	case TREBLE:
	case TREBLE_8:
	case FRENCHVIOLIN:
	case TREBLE_8A:
		clefcode = C_GCLEF;
		break;

	case BASS:
	case BASS_8:
	case BASS_8A:
	case SUBBASS:
		clefcode = C_FCLEF;
		break;

	default:
		/* everything else uses the C clef */
		clefcode = C_CCLEF;
		break;
	}
	*font_p = FONT_MUSIC;
	get_shape_override(staffno, 0, font_p, &clefcode);
	return(clefcode);
}


/* Returns width of the given clef in inches in the default size.
 * If is_small is YES, give width of the 3/4 sized one used in mid-score,
 * rather than the full-sized used at beginning of line.
 * Caller must adjust by staffscale if they need that.
 * No padding is included beyond the padding of the clef music character
 * itself, so caller needs to add any they deem appropriate for aesthetics.
 */

double
clefwidth(clef, staffno, is_small)

int clef;	/* TREBLE, BASS, ALTO, FRENCHVIOLIN, etc */
int staffno;
int is_small;	/* If YES, assume mid-score clef, not full sized one */

{
	int clefsym;
	int cleffont;

	clefsym = clefchar(clef, staffno, &cleffont);
	return(width(cleffont, (is_small ? (3 * DFLT_SIZE) / 4 : DFLT_SIZE),
						clefsym));
}


/* Returns where the given clef's baseline should be
 * relative to the middle line of the staff, in stepsizes.
 * (Above the middle line is positive, below is negative).
 * If north_p and/or south_p are non-null, the relative north/south values
 * of the clef are returned via the pointers. These will be relative
 * to the middle line of the staff and  will be in inches
 * in the default staffscale, using default size
 * (unless is_small == YES, in which case it will be 3/4 size).
 * Note that this should not be called with TABCLEF or NOCLEF.
 */

int
clefvert(clef, staffno, is_small, north_p, south_p)

int clef;	/* TREBLE, BASS, ALTO, FRENCHVIOLIN, etc */
int staffno;
int is_small;	/* If YES, assume mid-score clef, not full sized one.
		 * Note that if both of the following arguments are null,
		 * this is_small argument's value is actually irrelevent. */
float *north_p;	/* if non-null, relative north will be returned here */
float *south_p;	/* if non-null, relative south will be returned here */

{
	int steps;	/* relative to middle line, to be returned */
	int cleffont;

	switch(clef) {

	case TREBLE:
	case TREBLE_8:
	case TREBLE_8A:
		steps = -2;
		break;

	case FRENCHVIOLIN:
	case SOPRANO:
		steps = -4;
		break;

	case MEZZOSOPRANO:
		steps = -2;
		break;

	case ALTO:
		steps = 0;
		break;

	case TENOR:
		steps = 2;
		break;

	case BARITONE:
		steps = 4;
		break;

	case BASS:
	case BASS_8:
	case BASS_8A:
		steps = 2;
		break;

	case SUBBASS:
		steps = 4;
		break;

	case TABCLEF:
	default:
		pfatal("clefvert called with invalid clef %d", clef);
		/*NOTREACHED*/
		steps = 0;		/* shut up bogus compiler warning */
		break;
	}

	/* If caller wants relative north/south values, calculate them */
	if (north_p != 0 || south_p != 0) {
		char muschar;	/* music character to print for the clef */
		int clefsize;
		char tr8str[4];	/* "8" of treble8 or 8treble */
		float value;	/* of north or south */

		muschar = clefchar(clef, staffno, &cleffont);
		clefsize = (is_small ? (3 * DFLT_SIZE) / 4 : DFLT_SIZE);
		tr8str[0] = FONT_TR;
		tr8str[1] = 9;
		tr8str[2] = '8';
		tr8str[3] = '\0';
		
		if (north_p != 0) {
			value = (float) ascent(cleffont, clefsize, muschar);
			if (clef == TREBLE_8A || clef == BASS_8A) {
				value += (float) strheight(tr8str);
			}
			*north_p = value + (float)(steps * STEPSIZE);
		}

		if (south_p != 0) {
			value = (float) descent(cleffont, clefsize, muschar);
			if (clef == TREBLE_8 || clef == BASS_8) {
				value += (float) strheight(tr8str);
			}
			*south_p = -value + (float)(steps * STEPSIZE);
		}
	}

	return(steps);
}


/* Given a BLOCKHEAD, fill in its height. BLOCKHEADs are a bit strange,
 * in that they are in a separate coordinate space of unknown size.
 * So we start out assuming it is infinitely thin. Then we check
 * each string in the list and keep track of the lowest one, by
 * pretending to go where it would be printed and adding the descent
 * of the string. At the end, the height must be the page height minus the
 * lowest, since upwards is positive */

static void
set_height_blockhead(blockhead_p, context, mll_p)

struct BLOCKHEAD *blockhead_p;	/* which block to get height of */
UINT32B context;			/* C_HEADER, etc */
struct MAINLL *mll_p;		/* for getting margin overrides */

{
	float distance;		/* of headfoot from bottom of page */
	float lowest;
	float x_offset;		/* because of justification */
	float yval;		/* y coordinate value */
	struct PRINTDATA *pr_p;	/* walk through list of things to print */
	float block_width;	/* page width minus margins */
	float s_descent;	/* strdescent() of the current string */
	float extra;		/* how much farther the string descended
				 * than would be expected of a normal,
				 * single line. */
	struct MAINLL *rightmargin_mll_p;	/* mll_p to use for
				 * finding the right margin */


	/* Set _win in this context to zero height at top margin */
	Context = context;
	rightmargin_mll_p = (mll_p ? mll_p->next : 0);
	set_win_coord(blockhead_p->c);
	set_win(PGHEIGHT - EFF_TOPMARGIN, PGHEIGHT - EFF_TOPMARGIN,
		PGWIDTH - eff_rightmargin(rightmargin_mll_p),
		eff_leftmargin(mll_p));
	block_width = PGWIDTH - eff_rightmargin(rightmargin_mll_p) -
							eff_leftmargin(mll_p);

	if (blockhead_p->printdata_p != (struct PRINTDATA *) 0) {
		/* set current to left corner */
		set_cur(eff_leftmargin((struct MAINLL *)0),
						PGHEIGHT - EFF_TOPMARGIN);

		distance = _Cur[AY];

		/* Process each item in the list */
		for (pr_p = blockhead_p->printdata_p;
				pr_p != (struct PRINTDATA *) 0;
				pr_p = pr_p->next) {

			/* If this is a paragraph,
			 * split it into as many lines as needed. */
			if (pr_p->justifytype == J_JUSTPARA ||
					pr_p->justifytype == J_RAGPARA) {
				pr_p->string = split_string(pr_p->string,
								block_width);
			}
			pr_p->width = strwidth(pr_p->string);
			/* stretch justified paragraphs to full width */
			if (pr_p->justifytype == J_JUSTPARA &&
						pr_p->width < block_width) {
				pr_p->width = block_width;
			}

			/* adjust for justification */
			switch (pr_p->justifytype) {

			case J_RIGHT:
				x_offset = pr_p->width;
				break;

			case J_CENTER:
				x_offset = pr_p->width / 2.0;
				break;

			default:
				x_offset = 0.0;
				break;
			}

			/* set current to specified location */
			/* This can get called before placement phase is
			 * done, when we haven't evaluated any of the
			 * coord expressions yet, but we need to have this
			 * location evaluated now. Fortunately,
			 * prints inside blocks don't get affected by
			 * placement phase, so we can evaluate it now.
			 */
			if (mll_p != 0) {
				eval_coord(&(pr_p->location), mll_p->inputfile,
						mll_p->inputlineno);
			}
			else {
				eval_coord(&(pr_p->location), pr_p->inputfile,
						pr_p->inputlineno);
			}
			yval = inpc_y (&(pr_p->location), (char *) 0, -1);
			set_cur( inpc_x( &(pr_p->location), (char *) 0, -1 )
							- x_offset, yval);

			/* Before we had supported full expressions,
			 * there was code here with the description:
			 * 	If user said to go off of south or y of _win,
			 *	change to equivalent offset off of north
			 *	of _win. because the south and y could change.
			 * But I haven't been able to come up with any example
			 * where having that code or not having it
			 * actually changed the output.
			 * Nor can I find an example where the code to
			 * support full expressions would need anything here
			 * to make the results look like they did before full
			 * expression support. So since I am not sure what
			 * case the old code was intended to handle, I don't
			 * know what to do in its place, if anything. 
			 * So we do nothing here until and unless someone
			 * can demonstrate something is needed...
			 */

			/* determine lowest descent of current string */
			if (pr_p->isPostScript == YES) {
				s_descent = 0.0;
			}
			else {
				s_descent = strdescent(pr_p->string);
			}
			lowest = _Cur[AY] - s_descent;

			/* if lowest of anything found so far, note that */
			if ( lowest < distance) {
				distance = lowest;
				set_win(PGHEIGHT - EFF_TOPMARGIN, distance,
					PGWIDTH - eff_rightmargin((struct MAINLL *)0),
					eff_leftmargin((struct MAINLL *)0));
			}

			/* Set to end of string just "printed."
			 * If the string when down farther than a single line
			 * add in that extra.
			 */
			if (pr_p->isPostScript == YES) {
				extra = 0.0;
			}
			else {
				extra = s_descent - fontdescent(pr_p->string[0],
							pr_p->string[1]);
				if (extra < 0.0) {
					extra = 0.0;
				}
			}
			set_cur( _Cur[AX] + pr_p->width, _Cur[AY] - extra);
		}

		/* set height to lowest distance encountered */
		blockhead_p->height = (PGHEIGHT - distance - EFF_TOPMARGIN);

	}
	else {
		/* empty header/footer */
		blockhead_p->height = 0.0;
	}

	/* if was a footer, now we can set the actual _win coordinates,
 	 * by offsetting from bottom of page instead of top */
	if ( (context == C_FOOTER) || (context == C_FOOT2) ) {
		set_win(EFF_BOTMARGIN + blockhead_p->height, EFF_BOTMARGIN,
			PGWIDTH - eff_rightmargin((struct MAINLL *)0),
			eff_leftmargin((struct MAINLL *)0));
	}
	set_win_coord(0);
}


/* Calculate the height of all blocks, including headers and footers,
 * and fill in the height field of the struct. */

void
calc_block_heights()

{
	struct MAINLL *mll_p;
	double topheight = -1.0;	/* if > 0.0, is height of "top" */
	double botheight = -1.0;	/* if > 0.0, is height of "bottom" */

	debug(2, "calc_block_heights");

	set_height_blockhead(&Header, C_HEADER, 0);
	set_height_blockhead(&Footer, C_FOOTER, 0);
	set_height_blockhead(&Header2, C_HEAD2, 0);
	set_height_blockhead(&Footer2, C_FOOT2, 0);
	set_height_blockhead(&Leftheader, C_HEADER, 0);
	set_height_blockhead(&Leftfooter, C_FOOTER, 0);
	set_height_blockhead(&Leftheader2, C_HEAD2, 0);
	set_height_blockhead(&Leftfooter2, C_FOOT2, 0);
	set_height_blockhead(&Rightheader, C_HEADER, 0);
	set_height_blockhead(&Rightfooter, C_FOOTER, 0);
	set_height_blockhead(&Rightheader2, C_HEAD2, 0);
	set_height_blockhead(&Rightfooter2, C_FOOT2, 0);

	/* set main _win to space within margins and header/footer
	 * for first page. */
	Context = C_MUSIC;

	/* set the size of _page */
	_Page[AW] = _Page[AS] = 0.0;
	_Page[AE] = PGWIDTH;
	_Page[AN] = PGHEIGHT;
	_Page[AX] = PGWIDTH / 2.0;
	_Page[AY] = PGHEIGHT / 2.0;

	/* now calculate top/bot and any other blocks in the main list */
	initstructs();
	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		if (mll_p->str == S_SSV) {
			/* keep margins up to date */
			asgnssv(mll_p->u.ssv_p);
		}
		else if (mll_p->str == S_FEED) {
			if (mll_p->u.feed_p->top_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->top_p,
						C_TOP, 0);
				if (topheight < 0.0) {
					/* save for setting music _win */
					topheight = mll_p->u.feed_p->top_p->height;
				}
			}
			if (mll_p->u.feed_p->top2_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->top2_p,
						C_TOP2, 0);
			}
			if (mll_p->u.feed_p->bot_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->bot_p,
						C_BOT, 0);
				if (botheight < 0.0) {
					/* save for setting music _win */
					botheight = mll_p->u.feed_p->bot_p->height;
				}
			}
			if (mll_p->u.feed_p->bot2_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->bot2_p,
						C_BOT2, 0);
			}
			/* Now do the left and right versions */
			if (mll_p->u.feed_p->lefttop_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->lefttop_p,
						C_TOP, 0);
				if (topheight < 0.0 &&
						Firstpageside == PGSIDE_LEFT) {
					/* save for setting music _win */
					topheight = mll_p->u.feed_p->lefttop_p->height;
				}
			}
			if (mll_p->u.feed_p->lefttop2_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->lefttop2_p,
						C_TOP2, 0);
			}
			if (mll_p->u.feed_p->leftbot_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->leftbot_p,
						C_BOT, 0);
				if (botheight < 0.0 &&
						Firstpageside == PGSIDE_LEFT) {
					/* save for setting music _win */
					botheight = mll_p->u.feed_p->leftbot_p->height;
				}
			}
			if (mll_p->u.feed_p->leftbot2_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->leftbot2_p,
						C_BOT2, 0);
			}

			if (mll_p->u.feed_p->righttop_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->righttop_p,
						C_TOP, 0);
				if (topheight < 0.0 &&
						Firstpageside == PGSIDE_RIGHT) {
					/* save for setting music _win */
					topheight = mll_p->u.feed_p->righttop_p->height;
				}
			}
			if (mll_p->u.feed_p->righttop2_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->righttop2_p,
						C_TOP2, 0);
			}
			if (mll_p->u.feed_p->rightbot_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->rightbot_p,
						C_BOT, 0);
				if (botheight < 0.0 &&
						Firstpageside == PGSIDE_RIGHT) {
					/* save for setting music _win */
					botheight = mll_p->u.feed_p->rightbot_p->height;
				}
			}
			if (mll_p->u.feed_p->rightbot2_p != 0) {
				set_height_blockhead(mll_p->u.feed_p->rightbot2_p,
						C_BOT2, 0);
			}
		}
		else if (mll_p->str == S_BLOCKHEAD) {
			set_height_blockhead(mll_p->u.blockhead_p,
						C_BLOCK, mll_p);
		}
	}

	set_win(PGHEIGHT - EFF_TOPMARGIN - Header.height
			- (topheight > 0.0 ? topheight : 0.0),
			EFF_BOTMARGIN + Footer.height
			+ (botheight > 0.0 ? botheight : 0.0),
			PGWIDTH - eff_rightmargin((struct MAINLL *)0),
			eff_leftmargin((struct MAINLL *)0));
}


/* return number of beams or flags to use for a given basic time */

int
numbeams(btime)

int btime;	/* basic time of note to be checked */

{
	int n;

	/* no beams for long notes */
	if (btime <= 4) {
		return(0);
	}

	/* number of beams is equal to the number of bits 4 has to be
	 * shifted left in order to equal the given basic time */
	for (n = 1; (4 << n) <= MAXBASICTIME; n++) {
		if (btime == (4 << n)) {
			return(n);
		}
	}
	return(0);
}


/* Given an accidental (#, &, x, B, n) return its music character
 * C_SHARP, etc.  If not a valid accidental, return 0 */

int
acc2char(acc)

int acc;


{
	switch (acc) {

	case '&':
		return(C_FLAT);
	case '#':
		return(C_SHARP);
	case 'n':
		return(C_NAT);
	case 'x':
		return(C_DBLSHARP);
	case 'B':
		return(C_DBLFLAT);
	default:
		/* no accidental */
		return(0);
	}
}

/*
 * Name:        char2acc()
 *
 * Abstract:    Convert a std accidental character code to character symbol.
 *
 * Returns:     symbol, one of x # n & B, or \0 if not valid
 *
 * Description: This function is given the character code of a standard
 *		accidental.  It returns the symbol for it (see above),
 *		or null character if not one of these five.
 */

int
char2acc(acc_code)

int acc_code;			/* accidental as a code (C_SHARP, etc.) */

{
	switch (acc_code) {
	case C_NAT:
		return ('n');
	case C_SHARP:
		return ('#');
	case C_FLAT:
		return ('&');
	case C_DBLSHARP:
		return ('x');
	case C_DBLFLAT:
		return ('B');
	default:
		return ('\0');
	}
}


/* Returns YES if the given STUFF includes a "til" clause, NO if not */

int
has_til(stuff_p)

struct STUFF *stuff_p;

{
	return ((stuff_p->end.bars != 0
			|| stuff_p->end.count != 0.0
			|| stuff_p->end.steps != 0.0
			|| stuff_p->end.gracebackup != 0)
		? YES : NO);
}


/* Return the absolute x values for an INPCOORD in scaled inches.
 * This function used to calculate the value based on a c[] plus steps and
 * time offsets. Now that INPCOORD support much more complex expressions,
 * we calculate the value in locvar.c. So now this function just returns
 * that previously calculated value, after range checking.
 */

double
inpc_x(inpcoord_p, fname, lineno)

struct INPCOORD *inpcoord_p;	/* return the x value of this inpcoord */
char *fname;			/* filename, for error message */
int lineno;			/* for error message */

{
	if (inpcoord_p->hor < 0.0 || inpcoord_p->hor > PGWIDTH) {
		if (lineno > 0 && fname != (char *) 0) {
			l_warning(fname, lineno,
				"x value of %f is off the page",
				inpcoord_p->hor * Score.scale_factor);
		}
	}
	return(inpcoord_p->hor);
}


/* Like the preceding function, except for y */

double
inpc_y(inpcoord_p, fname, lineno)

struct INPCOORD *inpcoord_p;	/* return y value of this inpcoord */
char *fname;			/* filename, for error message */
int lineno;			/* for error message */

{
	if (inpcoord_p->vert < 0.0 || inpcoord_p->vert > PGHEIGHT) {
		if (lineno > 0 && fname != (char *) 0) {
			l_warning(fname, lineno,
				"y value of %f is off the page",
				inpcoord_p->vert * Score.scale_factor);
		}
	}
	return(inpcoord_p->vert);
}


/* return the y coordinate of the end of a note stem */
/* (the end farthest from the note head) */

double
find_y_stem(gs_p)

struct GRPSYL *gs_p;	/* which group to get the stem of */

{
	/* error checks */
	if (gs_p == (struct GRPSYL *) 0) {
		pfatal("null group passed to find_y_stem");
	}

	/* We pretend there are stems on rests for the purposes of
	 * placing "with" items */
	if (gs_p->grpcont == GC_REST) {
		if (gs_p->stemdir == UP) {
			return(gs_p->c[AN] - withheight(gs_p, PL_ABOVE));
		}
		else {
			return(gs_p->c[AS] + withheight(gs_p, PL_BELOW));
		}
	}

	if (gs_p->nnotes == 0) {
		pfatal("group with no notes passed to find_y_stem (from line %d, grpcont %d)",
				gs_p->inputlineno, gs_p->grpcont);
	}

	/* if stem is up, start at bottom note, if down at top */
	if (gs_p->stemdir == UP) {
		return(gs_p->notelist[ gs_p->nnotes - 1].c[AY] + gs_p->stemlen);
	}
	else {
		return(gs_p->notelist[0].c[AY] - gs_p->stemlen);
	}
}


/* Returns whether a note stem should be at the edge of center of note head.
 * For now, a mensural note head is centered, anything else not.
 */

int
stem_x_position(gs_p)

struct GRPSYL *gs_p;

{
	int n;
	int centered_count = 0;

	for (n = 0; n < gs_p->nnotes; n++) {
		if (gs_p->notelist[n].headfont == FONT_MUSIC2) {
			switch (gs_p->notelist[n].headchar) {
			case C_MENSURDIAMOND:
			case C_MENSURFILLDIAMOND:
			case C_MENSURDBLWHOLE:
				centered_count++;
				break;
			default:
				break;
			}
		}
	}
	if (centered_count > 0) {
		if (centered_count != gs_p->nnotes) {
			l_ufatal(gs_p->inputfile, gs_p->inputlineno,
				"Mixture of centered and edge stem note heads");
		}
		return(SP_CENTERED);
	}
	return(SP_EDGE);
}


/* return x coordinate of a note stem */

double
find_x_stem(gs_p)

struct GRPSYL *gs_p;	/* return x of stem of this group */

{
	double stem_adjust;	/* to overlap the note head */

	if ( gs_p == (struct GRPSYL *) 0) {
		pfatal("bad group passed to find_x_stem");
	}

	/* If called with a whole or double whole, then there
	 * is no real stem. We must be being called for printing slashes,
	 * so in that case, the x of the "stem" is the x of the group */
	if (STEMLESS(gs_p)) {
		return(gs_p->c[AX]);
	}

	if (stem_x_position(gs_p) == SP_CENTERED) {
		stem_adjust = 0.0;
	}
	else {
		/* Move stem by half of stem width so edge
		 * lines up with edge of note */
		stem_adjust = W_NORMAL / PPI / 2.0;
		if (gs_p->stemdir == UP || gs_p->basictime == BT_QUAD
					|| gs_p->basictime == BT_OCT) {
			stem_adjust = -stem_adjust;
		}
	}
	return(gs_p->c[AX] + (gs_p->stemx + stem_adjust) * Staffscale);
}


/* return the width of a key signature in inches */

double
width_keysig(sharps, naturals)

int sharps;	/* how many sharps to print, or if negative, how many flats. */
int naturals;	/* how many naturals to print for canceling previous key */

{
	double total_width = 0.0;
	int size;

	/* In keysig, things are drawn closer together than
	 * in other places, so to get the total width, we first
	 * multiply the width of the sharp, flat, or natural character
	 * by the number of times it is to be printed, then subtract off
	 * two points for each character printed, except for naturals,
	 * which are only jammed together by one point. */
	
	size = adj_size(DFLT_SIZE, Staffscale, (char *) 0, -1);
	if (sharps >= 1) {
		total_width = (width(FONT_MUSIC, size, C_SHARP) - 2.0 * Stdpad)
					* sharps;
	}
	else if (sharps <= -1) {
		/* negative sharps are flats */
		total_width = (width(FONT_MUSIC, size, C_FLAT) - 2.0 * Stdpad)
					* -sharps;
	}
	if (naturals != 0) {
		total_width += (width(FONT_MUSIC, size, C_NAT) - Stdpad)
				* abs(naturals) + 3.0 * Stdpad;
	}
	return(total_width);
}

/*
 * Name:        nextgrpsyl()
 *
 * Abstract:    Find next GRPSYL in this voice (same measure or not).
 *
 * Returns:     Pointer to the GRPSYL, or 0 if none.
 *
 * Description: This function, given a GRPSYL and the MLL structure it hangs
 *		off of, returns the next GRPSYL in this voice, even if it's in
 *		the next measure.  If it is in the next measure, *mll_p_p gets
 *		updated.  But if that next measure is a second or later ending,
 *		it's not considered to be a "next" measure, so return 0.
 */

struct GRPSYL *
nextgrpsyl(gs_p, mll_p_p)

struct GRPSYL *gs_p;	 /* the given GRPSYL */
struct MAINLL **mll_p_p; /* main linked list structure it is hanging off of */

{
	struct MAINLL *mll_p;	/* point at a MLL item */
	int endingloc;		/* of the following barline */


	/* if not at end of measure, just return the next GRPSYL */
	if (gs_p->next != 0) {
		return (gs_p->next);
	}

	mll_p = *mll_p_p;	/* save original MLL item */

	/*
	 * We hit the end of the measure.  We need to find the first group in
	 * the next measure.  Find the coming bar line, then the corresponding
	 * staff in the next measure.  We do this in case the number of staffs
	 * changes back and forth; we don't want to find the staff in some
	 * later measure.
	 */
	for (*mll_p_p = (*mll_p_p)->next; *mll_p_p != (struct MAINLL *) 0 &&
			(*mll_p_p)->str != S_BAR; *mll_p_p = (*mll_p_p)->next) {
		;
	}

	/* if we hit the end of the MLL, there is no next GRPSYL */
	if (*mll_p_p == (struct MAINLL *) 0) {
		return (struct GRPSYL *) 0;
	}

	/* we found a bar; get its endingloc */
	endingloc = (*mll_p_p)->u.bar_p->endingloc;

	/*
	 * Search for this staff in next measure.  If we find a pseudobar while
	 * doing this, save its endingloc in preference to the real bar's.
	 */
	for (*mll_p_p = (*mll_p_p)->next; *mll_p_p != (struct MAINLL *) 0 &&
			(*mll_p_p)->str != S_BAR &&
			((*mll_p_p)->str != S_STAFF ||
			(*mll_p_p)->u.staff_p->staffno != gs_p->staffno);
			*mll_p_p = (*mll_p_p)->next) {

		if ((*mll_p_p)->str == S_CLEFSIG && 
		    (*mll_p_p)->u.clefsig_p->bar_p != (struct BAR *) 0) {
			endingloc = (*mll_p_p)->u.clefsig_p->bar_p->endingloc;
		}
	}

	/* if we hit the end or another bar before finding our staff, return */
	if (*mll_p_p == (struct MAINLL *) 0 || (*mll_p_p)->str == S_BAR) {
		return (struct GRPSYL *) 0;
	}

	/*
	 * We found the appropriate staff in the next measure.  But if we have
	 * crossed into a second or later ending, this bar doesn't really
	 * "follow" the previous bar, and we must return null.  So if endingloc
	 * shows this is the case, we must search backwards to find out if we
	 * were already in an ending.
	 */
	if (endingloc == STARTITEM) {
		while (mll_p != 0 && mll_p->str != S_BAR && (mll_p->str !=
				S_CLEFSIG || mll_p->u.clefsig_p->bar_p == 0))
			mll_p = mll_p->prev;

		/* set endingloc of the previous measure */
		if (mll_p == 0) {
			endingloc = NOITEM;
		} else if (mll_p->str == S_BAR) {
			endingloc = mll_p->u.bar_p->endingloc;
		} else {
			endingloc = mll_p->u.clefsig_p->bar_p->endingloc;
		}

		/* if we were already in an ending, there's no next GRPSYL */
		if (endingloc == STARTITEM || endingloc == INITEM) {
			return (struct GRPSYL *) 0;
		}
	}

	/* return the first GRPSYL of the appropriate voice */
	return ((*mll_p_p)->u.staff_p->groups_p[ gs_p->vno - 1 ]);
}

/*
 * Name:        prevgrpsyl()
 *
 * Abstract:    Find previous GRPSYL in this voice (same measure or not).
 *
 * Returns:     Pointer to the GRPSYL, or 0 if none.
 *
 * Description: This function, given a GRPSYL and the MLL structure it hangs
 *		off of, returns the previous GRPSYL in this voice, even if it's
 *		in an earlier measure.  If we are at the start of an ending,
 *		it skips over any previous ending and goes to the measure
 *		preceding the first ending.  If the resulting GRPSYL is in a
 *		previous measure, *mll_p_p gets updated.
 */

struct GRPSYL *
prevgrpsyl(gs_p, mll_p_p)

struct GRPSYL *gs_p;	 /* the given GRPSYL */
struct MAINLL **mll_p_p; /* main linked list structure it is hanging off of */

{
	struct GRPSYL *gs2_p;	/* for looping through prev measure's list */
	struct BAR *bar_p;	/* point at a bar line */
	struct MAINLL *mll_p;	/* point at a MLL item */
	int pseudo;		/* was the last thing we saw a pseudobar? */
	int barcount;		/* how many bar lines we looped backward thru*/
	int safmoae;		/* "started at first measure of an ending" */


	/* if not at start of measure, just return the previous GRPSYL */
	if (gs_p->prev != (struct GRPSYL *) 0) {
		return (gs_p->prev);
	}

	/*
	 * We hit the start of the measure.  Loop backwards through the MLL
	 * looking for the bar line at the start of the "previous" measure.
	 * If our measure is not the first measure of an ending, this is
	 * simply the bar at the start of the previous measure.  Otherwise,
	 * this is the bar before the measure before the first ending.  Also
	 * handle the cases where we fall off the start of the MLL.
	 */
	bar_p = 0;
	mll_p = *mll_p_p;
	pseudo = NO;
	barcount = 0;
	safmoae = NO;
	do {
		/* find preceding bar or pseudobar, if either exists */
		for (mll_p = mll_p->prev; mll_p != (struct MAINLL *) 0 &&
				mll_p->str != S_BAR &&
				(mll_p->str != S_CLEFSIG ||
				mll_p->u.clefsig_p->bar_p == 0);
				mll_p = mll_p->prev) {
			;
		}

		/*
		 * If we hit the start of the MLL without crossing any bars, or
		 * just a pseudobar, there is no preceding GRPSYL, so return 0.
		 * (Depending on who is calling this function, the pseudobar at
		 * the start of the song may or may not exist.)  Otherwise, it
		 * must be that we started in the second measure, or an ending
		 * that we skipped over started there.  Point at the start of
		 * the MLL, and get out of the loop.  Note that we can't still
		 * be in the process of skipping over endings:  no ending can
		 * start at the first measure of the song, because there is no
		 * bar line there.
		 */
		if (mll_p == 0) {
			if (barcount == 0 || (barcount == 1 && pseudo == YES)) {
				return (struct GRPSYL *) 0;
			}
			mll_p = Mainllhc_p;
			break;
		}

		barcount++;

		/*
		 * Point bar_p at the relevant bar/pseudobar for checking the
		 * endingloc.  If this is a pseudobar, it's relevant.  If this
		 * is a bar, it's relevant only if it's the first thing we've
		 * seen, or the thing we saw last was not a pseudobar.  That
		 * is, the endingloc of the bar at the end of a score is to be
		 * ignored, because the true endingloc has been moved to the
		 * next score's pseudobar.  We need to worry about this because
		 * of the case where a second ending starts at the start of the
		 * new score (STARTITEM) and the previous score ends with
		 * ENDITEM, which should be ignored.
		 */
		if (mll_p->str == S_BAR) {
			if (bar_p == 0 || pseudo == NO) {
				bar_p = mll_p->u.bar_p;
			}
			if (pseudo == YES) {
				barcount--;	/* forget this bar */
				pseudo = NO;
			}
		} else {
			bar_p = mll_p->u.clefsig_p->bar_p;
			pseudo = YES;
		}

		/*
		 * If this is the first measure we're backing into, and this
		 * first bar we hit shows that our GRPSYL was in the first
		 * measure of an ending, remember that fact.
		 */
		if (barcount == 1 && bar_p->endingloc == STARTITEM) {
			safmoae = YES;
		}

	/*
	 * Get out, when, in the normal case, we've hit the second meaningful
	 * bar; or in the safmoae case, we've skipped back to the back before
	 * the first bar of the first ending.
	 */
	} while (! ( (safmoae == NO && barcount == 2) || (safmoae == YES &&
		(bar_p->endingloc == NOITEM || bar_p->endingloc == ENDITEM))));

	/*
	 * Search forward to the next bar, which is the bar before which we
	 * want to find a GRPSYL.  We don't care about pseudobars here.
	 */
	for (mll_p = mll_p->next; mll_p->str != S_BAR; mll_p = mll_p->next) {
		;
	}

	/*
	 * Now mll_p is the bar before which we want to find the GRPSYL.
	 * Find the corresponding staff in the previous measure.  We do
	 * this in case the number of staffs changes back and forth; we
	 * don't want to find the staff in some earlier measure.
	 */

	/* search for this staff in previous measure */
	for (*mll_p_p = mll_p->prev; *mll_p_p != (struct MAINLL *) 0 &&
			(*mll_p_p)->str != S_BAR &&
			((*mll_p_p)->str != S_STAFF ||
			(*mll_p_p)->u.staff_p->staffno != gs_p->staffno);
			*mll_p_p = (*mll_p_p)->prev) {
		;
	}

	/* if we hit the start or another bar before finding our staff, return*/
	if (*mll_p_p == (struct MAINLL *) 0 || (*mll_p_p)->str == S_BAR) {
		return (struct GRPSYL *) 0;
	}

	/* return the last GRPSYL of the appropriate voice */
	gs2_p = (*mll_p_p)->u.staff_p->groups_p[ gs_p->vno - 1 ];
	if (gs2_p == (struct GRPSYL *) 0) {
		return(gs2_p);
	}
	while (gs2_p->next != (struct GRPSYL *) 0) {
		gs2_p = gs2_p->next;
	}

	return (gs2_p);
}


/* if user asked for octave marks, we need to transpose any affected notes
 * by the appropriate number of octaves. This should be called for a measure
 * at a time. It will handle all the octave marks
 * within the measure for the current voice, both those carrying over into
 * this measure and ones starting there. If an octave mark spills beyond
 * the end of the measure, the amount to transpose and how long to do so
 * is saved away for use in the next measure. Checks for things becoming
 * out of range because of the transposition are not done here; caller
 * must do that if they care. */

void
octave_transpose(staff_p, mll_p, vno, normdir)

struct STAFF *staff_p;
struct MAINLL *mll_p;	/* staff_p connects here, used for bends */
int vno;		/* voice number */
int normdir;		/* YES if should move note pitches up for above and
			 * down for below. NO if the inverse should be done */

{
	struct GRPSYL *gs_p;	/* walk through list of GRPSYLs */
	struct GRPSYL *gs_roll_p;	/* group generated for a roll */
	struct STUFF *stuff_p;	/* to look for octave marks */
	RATIONAL total_time;	/* accumulated time in measure */
	float float_total_time;	/* for comparing with count */
	int carry_adjust;	/* adjust to carry into next measure */
	int carry_bars;		/* how many bars to carry over */
	float carry_counts;	/* how many counts in final bar */
	int staffno;		/* which staff we are working with */
	int first;		/* first group needs extra error check */


	staffno = staff_p->staffno;
	carry_adjust = carry_bars = 0;
	carry_counts = 0.0;

	/* if currently have octave mark */
	if (Octave_adjust[staffno] != 0) {

		/* if bar count > 0, transpose all notes in measure */
		if (--(Octave_bars[staffno]) > 0) {

			for (gs_p = staff_p->groups_p[vno];
					gs_p != (struct GRPSYL *) 0;
					gs_p = gs_p->next) {

				grp_octave_adjust(gs_p, Octave_adjust[staffno], mll_p);
			}
			carry_adjust = Octave_adjust[staffno];
			carry_bars = Octave_bars[staffno];
			carry_counts = Octave_count[staffno];
		}

		/* otherwise just transpose until specified time */
		else {
			total_time = Zero;
			for (gs_p = staff_p->groups_p[vno];
						gs_p != (struct GRPSYL *) 0;
						gs_p = gs_p->next) {

				float_total_time = RAT2FLOAT(total_time)
						* Score.timeden + 1.0;

				if (float_total_time <= Octave_count[staffno]) {
					grp_octave_adjust(gs_p,
						Octave_adjust[staffno], mll_p);
				}
				else {
					break;
				}
				total_time = radd(total_time, gs_p->fulltime);
				chk_tie_out_oct(gs_p, total_time,
						Octave_count[staffno],
						mll_p->inputfile,
						mll_p->inputlineno);
			}

			Octave_adjust[staffno] = 0;
			carry_adjust = 0;
			carry_bars = 0;
			carry_counts = 0.0;
		}
	}

	/* go through stuff list. If any octave marks, transpose appropriate
	 * notes in this measure. If extends to next measure, remember for
	 * next time. If user put in more than one octave
	 * mark going over the bar, catch that. If there are overlaps within
	 * a measure, it seems like too much trouble to catch this, so just
	 * transpose as often as they say and let them figure out why it
	 * sounds funny. */
	for (stuff_p = staff_p->stuff_p; stuff_p != (struct STUFF *) 0;
						stuff_p = stuff_p->next) {

		if (stuff_p->stuff_type == ST_OCTAVE) {

			if (Octave_adjust[staffno] != 0) {
				l_warning(stuff_p->inputfile,
						stuff_p->inputlineno,
						"overlapping octave marks");
			}

			/* figure out how many octaves to move */
			Octave_adjust[staffno] = parse_octave(stuff_p->string,
					stuff_p->place, stuff_p->inputfile,
					stuff_p->inputlineno);

			/* if this call is for inverse transposition, adjust
			 * to go in opposite direction */
			if (normdir == NO) {
				Octave_adjust[staffno] *= -1;
			}

			/* figure out to which count the octave mark applies
			 * within this measure. */
			if (stuff_p->end.bars > 0) {
				Octave_count[staffno] = 1.0 +
					RAT2FLOAT(Score.time) * Score.timeden;
			}
			else if (has_til(stuff_p) == NO) {
				/* No til clause, so end is same as start */
				Octave_count[staffno] = stuff_p->start.count;
			}
			else {
				Octave_count[staffno] = stuff_p->end.count;
			}
			
			total_time = Zero;
			first = YES;
			for (gs_p = staff_p->groups_p[vno];
						gs_p != (struct GRPSYL *) 0;
						gs_p = gs_p->next) {

				float_total_time = RAT2FLOAT(total_time) *
						Score.timeden + 1.0;
				if (float_total_time > Octave_count[staffno]) {
					/* past end; done with this one */
					break;
				}
				if (float_total_time >= stuff_p->start.count) {

					if (first == YES) {
						if (prev_has_tie(gs_p, mll_p) == YES) {
							l_ufatal(stuff_p->inputfile,
								stuff_p->inputlineno,
								"tie into octave mark not allowed");
						}

						first = NO;
					}

					/* is within the mark, so move */
					grp_octave_adjust(gs_p,
							Octave_adjust[staffno],
							mll_p);

					/* special case. If we have a rolled
					 * chord, and user specified
					 * an octave mark without a til clause,
					 * all the chords that got
					 * generated internally to cause the
					 * "roll" effect need to be transposed,
					 * even though they have been moved
					 * in time */
					if (float_total_time == stuff_p->start.count
						&& stuff_p->end.bars == 0
						&& stuff_p->end.count == 0.0
						&& gs_p->inputlineno < 0) {

					    /* adjust all the groups generated
					     * to create the roll effect */
					    for (gs_roll_p = gs_p->next;
							gs_roll_p != (struct GRPSYL *) 0;
							gs_roll_p = gs_roll_p->next) {

					    	    grp_octave_adjust(gs_roll_p,
							Octave_adjust[staffno],
							mll_p);
						    /* stop when we hit the
						     * end of the roll */
						    if (gs_roll_p->inputlineno > 0) {
							break;
						    }
					    }
					}
				}

				total_time = radd(total_time, gs_p->fulltime);
				if (stuff_p->end.bars < 1) {
					chk_tie_out_oct(gs_p, total_time,
						Octave_count[staffno],
						mll_p->inputfile,
						mll_p->inputlineno);
				}
			}

			/* if octave mark carried over into subsequent
			 * measure(s), make a note of that for future use */
			if (stuff_p->end.bars > 0) {
				if (carry_adjust != 0) {
					l_warning(stuff_p->inputfile,
						stuff_p->inputlineno,
						"overlapping octave marks");
				}
				carry_bars = stuff_p->end.bars;
				carry_counts = stuff_p->end.count;
				carry_adjust = Octave_adjust[staffno];
			}

			Octave_adjust[staffno] = 0;
		}
	}

	if ( (staff_p->groups_p[vno] != 0) &&
				(staff_p->groups_p[vno]->is_multirest == YES) ) {
		/* Multirest. Since the basictime is the negative of
		 * the number of measure of multirest,
		 * and we need to reduce the number of measures
		 * remaining in the octave mark, we do that by
		 * adding that negative value, adjusted by one,
		 * because the bar at the end of the multirest
		 * counts as one.
		 */
		carry_bars += staff_p->groups_p[vno]->basictime + 1;
		if (carry_bars <= 0) {
			/* ended inside the multirest */
			Octave_adjust[staffno] = 0;
			Octave_bars[staffno] = 0;
			Octave_count[staffno] = 0.0;
		}
	}

	/* above octave marks that were put in on the same input line will
	 * be in backwards order (because above stuff needs to be that
	 * way to pile on correctly). However, if the last octave in the
	 * stuff input line carried over a bar line, we would see it
	 * first and lose the carryover information when handling the earlier
	 * octave marks (which are later in the stuff list). That's why the
	 * carryover information had to be saved. Now we can assign it */
	if (carry_bars > 0) {
		Octave_adjust[staffno] = carry_adjust;
		Octave_bars[staffno] = carry_bars;
		Octave_count[staffno] = carry_counts;
	}
}


/* Returns YES/NO for whether the group before the group contains a tie */

static int
prev_has_tie(gs_p, mll_p)

struct GRPSYL *gs_p;	/* check this group */
struct MAINLL *mll_p;	/* where gs_p is connected; prevgrpsyl() needs this */

{
	int n;		/* loop through notes */

	if ((gs_p = prevgrpsyl(gs_p, &mll_p)) == 0) {
		return(NO);
	}
	for (n = 0; n < gs_p->nnotes; n++) {
		if (gs_p->notelist[n].tie == YES) {
			return(YES);
		}
	}
	return(NO);
}


/* Do ufatal if given group has tie and extends to or beyond octave mark */

static void
chk_tie_out_oct(gs_p, total_time, oct_end_count, filename, lineno)

struct GRPSYL *gs_p;	/* check this group */
RATIONAL total_time;	/* count at which the group ends */
double oct_end_count;	/* count at which the octave mark ends */
char *filename;		/* for error message */
int lineno;		/* for error message */

{
	float float_total_time;
	int n;

	float_total_time = RAT2FLOAT(total_time) * Score.timeden + 1.0;
	if (float_total_time >= oct_end_count) {
		for (n = 0; n < gs_p->nnotes; n++) {
			if (gs_p->notelist[n].tie == YES) {
				l_ufatal(filename, lineno, "not allowed to tie beyond octave mark; could try doing without using octave mark, inside ifdef MIDI");
			}
		}
	}
}


/* transpose all the notes in a group by the specified octave adjustment */

static void
grp_octave_adjust(gs_p, adj, mll_p)

struct GRPSYL *gs_p;		/* adjust this group */
int adj;			/* add this (potentially negative) value to
				 * each notelist octave */
struct MAINLL *mll_p;		/* STAFF of gs_p for finding prev grp for bends */

{
	register int n;
	struct GRPSYL *prevgs_p;	/* previous group, for bends */

	for (n = 0; n < gs_p->nnotes; n++) {
		gs_p->notelist[n].octave += adj;
		if (gs_p->notelist[n].octave < MINOCTAVE ||
				gs_p->notelist[n].octave > MAXOCTAVE) {
			l_ufatal(gs_p->inputfile, gs_p->inputlineno,
					"octave %d (after octave transposition) is out of range",
					gs_p->notelist[n].octave);
		}
	}
	/* Transpose any bends going to this group */
	prevgs_p = prevgrpsyl(gs_p, &mll_p);
	if (prevgs_p == 0) {
		/* If there is no previous group, nothing to transpose. */
		return;
	}
	for (n = 0; n < prevgs_p->nnotes; n++) {
		if (prevgs_p->notelist[n].is_bend == YES) {
			/* Note that when is_bend is YES, nslurto will always
			 * be 1, but we loop through as many as there are
			 * (all 1 of them!), to not be dependent
			 * on that piece of inside information.
			 */
			int s;
			for (s = 0; s < prevgs_p->notelist[n].nslurto; s++) {
				prevgs_p->notelist[n].slurtolist[s].octave += adj;
			}
		}
	}
}


/* Given a group and note value, return the effective accidental
 * on that note, (-2 to +2) taking key signature
 * and previous accidentals into account.
 * There are certain pathological cases that this doesn't
 * handle right, notably if the user changes the key signature at a bar
 * line, and the note in question is tied to from before that bar line,
 * and the key signature change is such that it changes what the accidental
 * should be. But it should handle anything less convoluted than that.
 */

int
eff_acc(gs_p, note_p, mll_p)

struct GRPSYL *gs_p;	/* get effective accidental for note in this group */
struct NOTE *note_p;	/* get for this note */
struct MAINLL *mll_p;	/* main list item that points to gs_p */

{
	struct MAINLL *orig_mll_p;
	struct GRPSYL *pgs_p;		/* previous group */
	int n;
	int tie_break;			/* YES if there has been a break
					 * in the chain of notes
					 * tied together */
	int stdacc;			/* standard version of acclist */
	int eff_accidental;		/* effective accidental so far */


	/* if this group has an explicit accidental, that's also the
	 * effective accidental */
	if ((stdacc = standard_acc(note_p->acclist)) != '\0') {
		/* note: the - 2 is to adjust for natural being element 2
		 * of the Circle array */
		return( (int) (strchr(Acclets, stdacc) - Acclets) - 2);
	}

	/* remember which measure we are starting in, so we know when we cross
	 * a bar line */
	orig_mll_p = mll_p;

	/* init to assume we might have ties */
	tie_break = NO;

	/* if all else fails, we will use the accidental from the key sig */
	eff_accidental = acc_from_keysig(note_p->letter, gs_p->staffno,
						mll_p);

	/* back up until we figure out the effective accidental */
	for (pgs_p = prevgrpsyl(gs_p, &mll_p); pgs_p != (struct GRPSYL *) 0;
				pgs_p = prevgrpsyl(pgs_p, &mll_p)) {


		/* see if this group contains the note in question */
		for (n = 0; n < pgs_p->nnotes; n++) {
			if (pgs_p->notelist[n].letter == note_p->letter &&
						pgs_p->notelist[n].octave
						== note_p->octave) {
				/* it does have the note: it's notelist[n] */
				break;
			}
		}

		/* see if this is end of ties to the note, working backwards */
		if (n == pgs_p->nnotes) {
			/* no note at all, so clearly no tied note */
			tie_break = YES;
		}
		else if (pgs_p->notelist[n].tie == NO) {
			/* end of chain of tied notes */
			tie_break = YES;
		}

		if (orig_mll_p == mll_p) {
			/* we're still in same measure. If we have a matching
			 * note, see if it has an accidental */
			if (n < pgs_p->nnotes) {
				if ((stdacc = standard_acc(pgs_p->notelist[n].acclist)) != '\0') {
					return( (int) (strchr(Acclets, stdacc)
						- Acclets) - 2);
				}
			}

			/* have to keep backing up */
		}
		else {
			/* Now in previous measure. If no matching note,
			 * we use the effective accidental found so far */
			if (n == pgs_p->nnotes) {
				return(eff_accidental);
			}

			/* if the note isn't tied, then use the most recent
			 * effective accidental we found */
			if (pgs_p->notelist[n].tie == NO || tie_break == YES) {
				return(eff_accidental);
			}

			/* if there is an accidental on this note,
			 * then it's the one we're looking for. */
			if ((stdacc = standard_acc(pgs_p->notelist[n].acclist)) != '\0') {
				return( (int) (strchr(Acclets, stdacc)
					- Acclets) - 2);
			}

			/* need to continue working backwards toward
			 * the beginning of this new measure */
			orig_mll_p = mll_p;
			eff_accidental = acc_from_keysig(note_p->letter,
					gs_p->staffno, mll_p);
		}
	}

	/* backed up all the way to the beginning of the song, use last we
	 * found */
	return(eff_accidental);
}


/* given a letter and staff number, return 1 if the pitch with that letter
 * gets a sharps according to the key signature,  or -1 if it gets a flat,
 * or 0 if it gets neither. */

int
acc_from_keysig(letter, staffno, mll_p)

int letter;		/* which pitch */
int staffno;		/* which staff to get the key signature from */
struct MAINLL *mll_p;	/* pitch is from the staff hanging off of here */

{
	int index;	/* where the letter is in circle of fifths */
	int sharps;	/* sharps in key sig for the given staff */
	struct SSV *ssv_p;	/* to get key signature */


	/* find letter in circle of fifths */
	index = strchr(Circle, letter) - Circle + 1;

	/* get key signature. Unfortunately, the SSVs may not be
	 * accurate at the time we are called, so we have to search
	 * backwards in the main list for the most recent relevant
	 * key signature change. */
	for (sharps = 0; mll_p != (struct MAINLL *) 0; mll_p = mll_p->prev) {
		if (mll_p->str == S_SSV) {
			ssv_p = mll_p->u.ssv_p;

			/* does this SSV have a key signature change in it? */
			if (ssv_p->used[SHARPS] == YES) {
				if (ssv_p->context == C_STAFF &&
						ssv_p->staffno == staffno) {
					/* aha! found the most recent
					 * key signature
					 * for the relevant staff */
					sharps = ssv_p->sharps;
					break;
				}
				else if (ssv_p->context == C_SCORE) {
					/* this will be the score-wide default
					 * if we don't find a staff-specific
					 * value, so save this as our default */
					sharps = ssv_p->sharps;
				}
			}
		}
	}

	if (sharps > 0) {
		/* key signature is one with sharps */
		if (index <= sharps) {
			/* this letter gets a sharp */
			return(1);
		}
	}
	else if (sharps < 0) {
		/* key signature is one with flats */
		if ( (8 - index) <= -sharps) {
			/* this letter gets a flat */
			return(-1);
		}
	}

	/* must be a natural */
	return(0);
}

/* Set Staffscale, Stepsize, Stdpad, and other similar values based on the
 * specified staff number. If staff of 0 is given, set Staffscale to the
 * score value
 */

void
set_staffscale(s)

int s;			/* which staff */

{
	Staffscale = (s == 0 ? Score.staffscale
					: svpath(s, STAFFSCALE)->staffscale);
	Stepsize = STEPSIZE * Staffscale;
	Stdpad = STDPAD * Staffscale;
	Flagsep = FLAGSEP * Staffscale;
	Smflagsep = SMFLAGSEP * Staffscale;
	Tupheight = TUPHEIGHT * Staffscale;
}


/* Determine the space between lines of a grid. A staff of 0 means use
 * the score size. A staff of -1 means ATEND. Return distance in inches. */

double
gridspace(staff)

int staff;

{
	double space;

	if (staff == -1) {
		space = ATEND_GS * STEPSIZE * Score.gridscale;
	}
	else {
		space = WHEREUSED_GS * STEPSIZE
				* svpath(staff, STAFFSCALE)->staffscale
				* svpath(staff, GRIDSCALE)->gridscale;
	}
	return(space);
}


/* Given a grid, return (via pointer) the items needed for the PostScript
 * plus the top fret.
 */

void
gridinfo(grid_p, staff, frets_p, fretnum_p, numvert_p, topfret_p)

struct GRID *grid_p;
int staff;	/* 0 == score, -1 = ATEND, otherwise the staff number */
int *frets_p;	/* how many frets high the grid should be */
int *fretnum_p;	/* the N of "N fr" */
int *numvert_p;	/* how many frets from the top to print the "N fr" */
int *topfret_p; /* the fret number of the top line of the grid */

{
	int minfret, maxfret;	/* smallest and largest frets used */
	int rightmost_fret;	/* the 'N' of 'N fr" if any */
	int right_stringnum;	/* string number having rightmost_fret */
	int mincurvefret;	/* smallest fret number inside curve */
	int has_o;		/* if there are 'o' items */
	int gridfret;
	int mingridheight;
	int s;


	/* Go through strings finding min/max/rightmost */
	minfret = MAXFRET + 1;
	maxfret = MINFRET - 1;
	mincurvefret = MAXFRET + 1;
	has_o = NO;
	rightmost_fret = right_stringnum = 0;  /* avoids bogus warnings */
	for (s = 0; s < grid_p->numstr; s++) {
		/* x o and - things don't count */
		if (grid_p->positions[s] > 0) {
			if (grid_p->positions[s] < minfret) {
				minfret = grid_p->positions[s];
			}
			if (grid_p->positions[s] > maxfret) {
				maxfret = grid_p->positions[s];
			}
			rightmost_fret = grid_p->positions[s];
			right_stringnum = s;

			/* find smallest fret inside curve */
			if (grid_p->curvel != 0 && s >= grid_p->curvel - 1 &&
					s <= grid_p->curver - 1 &&
					grid_p->positions[s] < mincurvefret) {
				mincurvefret = grid_p->positions[s];
			}
		}
		else if (grid_p->positions[s] == 0) {
			has_o = YES;
		}
	}

	/* set the values to defaults, then calculate actuals if needed */
	*frets_p = mingridheight = svpath(staff == -1 ? 0 : staff, MINGRIDHEIGHT)->mingridheight;
	*fretnum_p = 0;
	*numvert_p = 0;
	if (minfret <= MAXFRET) {
		/* at least one fret was used */

		/* figure out how many frets tall to make the grid */
		*frets_p = maxfret + 1;

		/* see if gridfret is set */
		gridfret = svpath(staff == -1 ? 0 : staff, GRIDFRET)->gridfret;
		if (gridfret != NOGRIDFRET) {
			/* gridfret is set; see if all frets larger than that.
			 * But we only use "N fr" if there are no 'o' items. */
			if (has_o == NO && minfret >= gridfret) {
				/* We will need "N fr"
				 * Usually we use the rightmost string
				 * that has a fret on it,
				 * but there is one special case:
				 * if the curve comes at least that far right,
				 * and the minimum fret inside the curve is
				 * smaller than the rightmost_fret, then we
				 * put the "N fr" by the curve minimum.
				 */
				if (grid_p->curver - 1 >= right_stringnum
						&& mincurvefret < rightmost_fret) {
					rightmost_fret = mincurvefret;
				}
				*fretnum_p = rightmost_fret;
				*numvert_p = rightmost_fret - minfret + 1;
				*frets_p = maxfret - minfret + 2;
			}
		}

		/* If less frets than minimum, change to minimum.
		 * Add one for the top line. */
		if (*frets_p < mingridheight + 1) {
			*frets_p = mingridheight + 1;
		}
	}
	*topfret_p = (*fretnum_p == 0 ? 0 : *fretnum_p - *numvert_p);
}


/* Determine the dimensions of a grid, relative to a point in the middle
 * of the top line of the grid, and return them via pointers.
 * If pointers are 0, don't bother; caller doesn't care about these things.
 * Things in this function must be kept in sync with the PostScript prolog
 * definition of grids.
 */

void
gridsize(grid_p, staff, north_p, south_p, east_p, west_p)

struct GRID *grid_p;	/* find the size of this grid */
int staff;		/* use this staff for scaling. 0 means score,
			 * -1 means ATEND */
float *north_p;		/* return values... */
float *south_p;
float *east_p;
float *west_p;

{
	double space;	/* distance between adjacent line of the grid */
	int frets;
	int fretnum;
	int numvert;
	int topfret;
	int s;		/* string index */

	if (grid_p == 0) {
		pfatal("gridsize() was passed a null pointer");
	}

	/* determine distance between grid lines and other needed info */
	space = gridspace(staff);
	gridinfo(grid_p, staff, &frets, &fretnum, &numvert, &topfret);

	/* Start with minimum. East and west are equal, at half the
	 * total grid width, based on number of strings. The number of
	 * spaces is the number of strings minus one, but dots, X's,
	 * and O's will hang over the sides, so use the number of strings.
	 * Then adjust east for "N fr" if needed. */
	if (west_p != 0) {
		*west_p = -((space * grid_p->numstr) / 2.0);
	}
	if (east_p != 0) {
		*east_p = (space * grid_p->numstr) / 2.0;
		if (fretnum > 0) {
			/* We will need "N fr".
			 * So we need enough space to hold
			 * font, size, 2 digits, space, "fr", null.
			 * But super-safe compilers don't know the number
			 * will never be more than 2 digits, and insist
			 * on more. */
			char tmp[16];

			/* this is printed in Palatino Roman */
			tmp[0] = (char) FONT_PR;

			/* Between staffscale and gridscale,
			 * we could get a size that we can't represent
			 * in internal format, so get string width
			 * in default size and adjust afterwards. */
			tmp[1] = (char) DFLT_SIZE;

			/* since we know there are no funny characters
			 * in this string, we can cheat and not bother
			 * to call the string normalizer. */
			sprintf(tmp + 2, "%d fr", fretnum);

			*east_p += strwidth(tmp) *
						(space * PPI * 1.9)/ DFLT_SIZE;
		}
	}

	if (north_p != 0) {
		/* Always put almost one space of padding on top, which allows
		 * room for x's and o's and curves. Even if this particular grid
		 * doesn't have those, many do, so it's nice to line
		 * all of them up as much as possible */
		*north_p = 0.85 * space;
		/* If there is a curve above the top fret,
		 * with x's or o's above it, leave some more space */
		if (grid_p->positions[grid_p->curvel - 1] == topfret + 1 
					|| grid_p->positions[grid_p->curver - 1]
					== topfret + 1) {
			for (s = grid_p->curvel; s <= grid_p->curver; s++) {
				if (grid_p->positions[s-1] == 0 ||
						grid_p->positions[s-1]
						== -1) {
					*north_p += 0.7 * space;
					break;
				}
			}
		}
	}

	/* Grid is always at least 4 boxes high, more if needed,
	 * plus 1/2 space of padding at bottom */
	if (south_p != 0) {
		*south_p = -(frets - 0.5) * space;
	}
}


/* This function returns the minimum distance needed between the current 
 * staff and previous one, given their clefs and allowing for a measure
 * number. The clefs might be NOCLEF, like if this is for the top
 * staff of a page or a staff where printclef is false.
 */
 
double
clefspace(prevclef, prevscale, curclef, curscale, measnum)

int prevclef;		/* clef on staff above */
double prevscale;	/* staffscale for the staff above */
int curclef;		/* clef on staff below */
double curscale;	/* staffscale for the staff below */
int measnum;		/* If > 0, the measure number to be printed.
			 * If 0, then no measure number is to be printed. */

{
	double cur_extend;	/* space needed for current clef */
	double prev_extend;	/* space needed for clef above */
	double space_needed;	/* total for both clefs and measure number */


	/* Figure out how much the clef on current staff sticks up,
	 * Do in approximate STEPSIZEs here, and adjust later for scale. */
	switch (curclef) {
	case TREBLE_8A:
		cur_extend = 6.0;
		break;
	case TREBLE:
	case TREBLE_8:
		cur_extend = 4.0;
		break;
	case BARITONE:
		cur_extend = 4.2;
		break;
	case TENOR:
		cur_extend = 2.2;
		break;
	case FRENCHVIOLIN:
		cur_extend = 2.0;
		break;
	case BASS_8A:
		cur_extend = 2.0;	
		break;
	case SUBBASS:
		cur_extend = 2.0;
		break;
	default:
		cur_extend = 0.0;
		break;
	}

	/* Similar for the clef above, only how much it sticks down
	 * rather than up */
	switch (prevclef) {
	case TREBLE:
	case TREBLE_8A:
		prev_extend = 3.2;
		break;
	case MEZZOSOPRANO:
		prev_extend = 2.2;
		break;
	case SOPRANO:
		prev_extend = 4.2;
		break;
	case FRENCHVIOLIN:
	case TREBLE_8:
		prev_extend = 5.2;
		break;
	case BASS_8:
		prev_extend = 2.0;
		break;
	default:
		prev_extend = 0.0;
		break;
	}

	/* Add top and bottom together, adjusting for scale factors,
	 * and adding a little padding */
	space_needed = prev_extend * STEPSIZE * prevscale +
			cur_extend * STEPSIZE * curscale +
			STDPAD * curscale;

	/* Add on the space for the measure number, if necessary. */
	if (measnum > 0) {
		char mnumstring[16];
		mnum_string(mnumstring, measnum);
		space_needed += strheight(mnumstring) + STDPAD;
	}
	return(space_needed);
}


/* Convert measure number into internal-format string,
 * adding box or circle if called for by measnumstyle.
 */

void
mnum_string(dest_string, measnum)

char *dest_string;	/* Write the string here. Assumed to be long enough */
int measnum;		/* Write the measure number into dest_string */

{
	if (Score.measnumstyle == RS_PLAIN) {
		(void) sprintf(dest_string, "%c%c%d",
			(char)(Score.measnumfamily + Score.measnumfont),
			(char) Score.measnumsize,
			measnum);
	}
	else {
		(void) sprintf(dest_string, "%c%c\\%c%d\\%c",
			(char)(Score.measnumfamily + Score.measnumfont),
			(char) Score.measnumsize,
			(Score.measnumstyle == RS_BOXED ? '[' : '{'),
			measnum,
			(Score.measnumstyle == RS_BOXED ? ']' : '}'));
	}
	fix_string(dest_string, dest_string[0], dest_string[1], (char *) 0, -1);
}

/*
 * Name:        eff_rightmargin()
 *
 * Abstract:    Return the effective right margin for this score.
 *
 * Returns:     the margin in inches
 *
 * Description: There are two reason that code can't just use Score.rightmargin
 *		but must call this function.  First, the way the "scale" param
 *		works, we pretend the paper is smaller by that amount and then
 *		in PostScript magnify it back to the real size; but margins are
 *		not to be scaled, so we have to fake out the code by dividing
 *		out the scale here.  Second, the user can override the param
 *		on a "newscore" or "newpage".  To ignore a user override, pass
 *		0 for mainll_p, else pass some MLL on that score.
 *		Oh, and third, we have to handle the MG_AUTO case.
 */

double
eff_rightmargin(mainll_p)

struct MAINLL *mainll_p; /* MLL struct on some score, or 0 for normal margin */

{
	struct MAINLL *mll_p;


	/* if not already at a FEED, find FEED at right end of this score */
	while (mainll_p != 0 && mainll_p->str != S_FEED)
		mainll_p = mainll_p->next;

	/* if there is none, use the parameter */
	if (mainll_p == 0)
		return (Score.rightmargin / Score.scale_factor);

	/* handle the case where placement needs to decide it */
	if (mainll_p->u.feed_p->right_mot == MOT_AUTO) {
		/*
		 * If this FEED is after a block, use the parameter, since
		 * it makes no sense to move the margin in this case.
		 */
		for (mll_p = mainll_p->prev; mll_p != 0 && mll_p->str != S_FEED;
						mll_p = mll_p->prev)
			;
		if (mll_p == 0) {
			pfatal("eff_rightmargin fell off main linked list");
		}
		if (mll_p->next->str == S_BLOCKHEAD) {
			/* overwrite the senseless MOT_AUTO setting */
			mainll_p->u.feed_p->right_mot = MOT_UNUSED;
			return (Score.rightmargin / Score.scale_factor);
		}

		/* We are not after a block. We have to return something,
		 * so we return a special (negative) value, that the caller
		 * can use to recognize as meaning
		 * that the user specified "auto" */
		return (MG_AUTO);
	}

	/* if there is no override, use the parameter */
	if (mainll_p->u.feed_p->right_mot == MOT_UNUSED)
		return (Score.rightmargin / Score.scale_factor);

	/* use this override value */
	return (mainll_p->u.feed_p->rightmargin / Score.scale_factor);
}

/*
 * Name:        eff_leftmargin()
 *
 * Abstract:    Return the effective left margin for this score.
 *
 * Returns:     the margin in inches
 *
 * Description: There are two reason that code can't just use Score.leftmargin
 *		but must call this function.  First, the way the "scale" param
 *		works, we pretend the paper is smaller by that amount and then
 *		in PostScript magnify it back to the real size; but margins are
 *		not to be scaled, so we have to fake out the code by dividing
 *		out the scale here.  Second, the user can override the param
 *		on a "newscore" or "newpage".  To ignore a user override, pass
 *		0 for mainll_p, else pass some MLL on that score.
 */

double
eff_leftmargin(mainll_p)

struct MAINLL *mainll_p; /* MLL struct on some score, or 0 for normal margin */

{
	/* if not already at a FEED, find FEED at left end of this score */
	while (mainll_p != 0 && mainll_p->str != S_FEED)
		mainll_p = mainll_p->prev;

	/* if there is none, or there is no override, use the parameter */
	if (mainll_p == 0 || mainll_p->u.feed_p->left_mot == MOT_UNUSED)
		return (Score.leftmargin / Score.scale_factor);

	/* use this override value */
	return (mainll_p->u.feed_p->leftmargin / Score.scale_factor);
}

/*
 * Name:        findprimes()
 *
 * Abstract:    Find all the prime numbers up to the given number ("max").
 *
 * Returns:     array indexed 0 to max, each element YES or NO (is index prime?)
 *
 * Description: This function mallocs and returns an array of shorts, indexed
 *		from 0 to max.  Each element is YES or NO, telling whether its
 *		index is a prime number.  The first time it is called, or if
 *		max is greater than the previous time, it calculates all this,
 *		but on other calls it just returns the answer from before.
 */

short *
findprimes(max)

int max;		/* max integer we need to consider */

{
	static short *isprime = 0;	/* array to be malloc'ed */
	static int oldmax = 0;		/* the max passed in previously */
	int stop;			/* where to stop looking */
	int prime;			/* a prime number */
	int n;				/* loop index */


	/* if we've already been here ... */
	if (isprime != 0) {
		/* if we've already done the same or more, just return answer*/
		if (max <= oldmax) {
			return (isprime);
		}
		/* max increased; free the old array */
		FREE(isprime);
	}
	oldmax = max;		/* remember if for next time */

	MALLOCA(short, isprime, max + 1);

	/* 0 and 1 are not primes */
	isprime[0] = isprime[1] = NO;

	/*
	 * We're going to use the Sieve of Eristosthenes.  We start out by
	 * assuming everything 2 and greater is prime.
	 */
	for (n = 2; n <= max; n++) {
		isprime[n] = YES;
	}

	/* the following loop can stop when it gets to this point */
	stop = sqrt((double)max) + 1;

	prime = 2;
	while (prime <= stop) {
		/* knock out all multiples of this prime number */
		for (n = 2 * prime; n <= max; n += prime) {
			isprime[n] = NO;
		}
		/* find the next prime */
		for (n = prime + 1; n <= stop && isprime[n] == NO; n++)
			;
		prime = n;
	}

	return (isprime);
}

/*
 * Name:        factor()
 *
 * Abstract:    Factor the given number.
 *
 * Returns:     array indexed 0 to num, giving the prime factors
 *
 * Description: This function mallocs and returns an array of shorts, indexed
 *		from 0 to max.  Each element indexed by a prime number tells
 *		how many times that prime factor occurs in num.  All other
 *		elements are 0.  The first time it is called, or if num is
 *		different from the previous time, it calculates all this,
 *		but on other calls it just returns the answer from before.
 */

short *
factor(num)

int num;		/* the integer to be factored */

{
	static short *factors = 0;	/* array to be malloc'ed */
	static int oldnum = 0;		/* the number passed in previously */
	short *isprime;			/* list of which numbers are prime */
	int orignum;			/* remember original num */
	int n;				/* loop index */


	/* if we've just done the same number, just return the answer */
	if (factors != 0) {
		/* if we've already done the same or more, just return answer*/
		if (num == oldnum) {
			return (factors);
		}
		/* num changed; free the old array */
		FREE(factors);
	}
	oldnum = num;		/* remember it for next time */

	CALLOCA(short, factors, num + 1);

	/* find which numbers up to num are primes */
	isprime = findprimes(num);

	/*
	 * For every prime number until "num" is used up, divide it into num
	 * as many times as possible, keeping track of how many times.
	 */
	orignum = num;
	for (n = 2; n <= orignum && num > 1; n++) {
		if (isprime[n] == YES) {
			while (num % n == 0) {
				num /= n;
				factors[n]++;
			}
		}
	}

	return (factors);
}


/* Return the width of the widest note head in the given GRPSYL. */

double
widest_head(gs_p)

struct GRPSYL *gs_p;

{
	double widest;		/* widest note head in the group */
	double thiswidth;	/* width of current note */
	int n;			/* note index */

	widest = 0.0;
	for (n = 0; n < gs_p->nnotes; n++) {
		thiswidth = width(gs_p->notelist[n].headfont,
			size_def2font(gs_p->notelist[n].notesize),
			gs_p->notelist[n].headchar);
		if (thiswidth > widest) {
			widest = thiswidth;
		}
	}
	return(widest);
}


/* Returns height of "with" list on the specified group, in inches.
 * If there are no "with" items, it will return 0.0.
 * Always returns the absolute value (i.e., positive) regardless of the side.
 * Staffscale is expected to be set properly.
 */

double
withheight(gs_p, place)

struct GRPSYL *gs_p;
int place;

{
	int n;
	double hi = 0.0;
	double answer = 0.0;

	for (n = 0; n < gs_p->nwith; n++) {
		if (gs_p->withlist[n].place != place) {
			continue;
		}
		hi = strheight(gs_p->withlist[n].string);
		hi = MAX(hi, Staffscale * MINWITHHEIGHT);
		answer += hi;
	}
	return(answer);
}


/* Return the offset from the middle of the staff at which to place a
 * measure rest. The caller must multiply this by their effective stepsize,
 * since this is used both in places where Stepsize is valid and when
 * it is not. The offset is always the middle of the staff unless the
 * number of staff lines is even, in which case it is the line above that,
 * adjusted by TABRATIO if the staff is a tablature staff. */

double
mr_y_offset(staffno)

int staffno;

{
	if ( (svpath(staffno, STAFFLINES)->stafflines & 1) == 0) {
		return (is_tab_staff(staffno) ? TABRATIO : 1.0);
	}
	return(0.0);
}


/* Given an index into a c[], like AX, or RE, return a value that indicates
 * what kind of index it is. Specifically:
 *   AX | AW | AE   ->  AX (i.e., absolute in X dimension)
 *   AY | AN | AS   ->  AY (i.e., absolute in Y dimension)
 *   RX | RW | RE   ->  RX (i.e., relative in X dimension)
 *   RY | RN | RS   ->  RY (i.e., relative in Y dimension)
 *   INCHPERWHOLE ->  INCHPERWHOLE
 */

int
index_type(index)

int index;

{
	switch (index) {

	case AX: case AW: case AE:
		return(AX);
	case AY: case AN: case AS:
		return(AY);
	case RX: case RW: case RE:
		return(RX);
	case RY: case RN: case RS:
		return(RY);
	case INCHPERWHOLE:
		return(INCHPERWHOLE);
	}
	pfatal("bad index type %d pass to index_type()", index);
	/*NOTREACHED*/
	/* but keep lint happy */
	return(0);
}

/* Convert a cents value to a divided out ratio */

double
cents2value(as_cents)

double as_cents;

{
	return(pow(2.0, as_cents / 1200.0));
}


/* For a "standard" accidental, set any offsets that the user didn't set
 * to their default values. 
 */

void
set_default_acc_offsets(accinfo_p)

struct ACCINFO *accinfo_p;

{
	char *name;			/* acc symbol name */
	float value = -1.0;		/* default value */
	int i;				/* index through a-g offset array */


	name = get_charname(accinfo_p->code, accinfo_p->font);

	if (strcmp(name, "flat") == 0) {
		switch (Score.tuning) {
		case TU_EQUAL:
			value = (float) cents2value( (double) -100.0);
			break;
		case TU_MEANTONE:
			value =  (float) cents2value( (double) -76.049014);
			break;
		case TU_PYTHAGOREAN:
			value = 2048.0 / 2187.0;
			break;
		}
	}
	else if (strcmp(name, "sharp") == 0) {
		switch (Score.tuning) {
		case TU_EQUAL:
			value = (float) cents2value( (double) 100.0);
			break;
		case TU_MEANTONE:
			value = (float) cents2value( (double) 76.049014);
			break;
		case TU_PYTHAGOREAN:
			value = 2187.0 / 2048.0;
			break;
		}
	}
	else if (strcmp(name, "nat") == 0) {
		value = 1.0;
	}
	else if (strcmp(name, "dblflat") == 0) {
		switch (Score.tuning) {
		case TU_EQUAL:
			value = (float) cents2value( (double) -200.0);
			break;
		case TU_MEANTONE:
			value = (float) cents2value( (double) -152.098028);
			break;
		case TU_PYTHAGOREAN:
			value = 4194304.0 / 4782969.0;
			break;
		}
	}
	else if (strcmp(name, "dblsharp") == 0) {
		switch (Score.tuning) {
		case TU_EQUAL:
			value = (float) cents2value( (double) 200.0);
			break;
		case TU_MEANTONE:
			value = (float) cents2value( (double) 152.098028);
			break;
		case TU_PYTHAGOREAN:
			value = 4782969.0 / 4194304.0;
			break;
		}
	}

	if (value < 0.0) {
		pfatal("%s didn't match any standard acc when setting default values", name);
	}
	for (i = 0; i < 7; i++) {
		if (accinfo_p->offset[i] < 0.0) {
			accinfo_p->offset[i] = value;
		}
	}
}


/* find y offset on stem based on number of beams, whether normal or small
 * notes, and stem direction */

double
beam_offset(nbeams, gsize, stemdir)

int nbeams;	/* how many beams */
int gsize;	/* GS_NORMAL, GS_SMALL, or GS_TINY */
int stemdir;	/* UP or DOWN */

{
	/* for consistency, it would be nice to use FLAGSEP and SMFLAGSEP
	 * for beam separation too, but when we tried that, beams looked too
	 * close together, especially on certain low-resolution devices,
	 * so that's why we're using 5 and 4 points. */
	double space;

	switch (gsize) {
	case GS_NORMAL:
		space = 5;
		break;
	case GS_SMALL:
		space = 4;
		break;
	case GS_TINY:
		space = 3;
		break;
	default:
		pfatal("bad GS_ value of %d in beam_offset()", gsize);
		/*NOTREACHED*/
		space = 5;
		break;
	}
	return ( (nbeams - 1) * space * Staffscale
					* (stemdir == UP ? -POINT : POINT) );
}

/* Given a "physical" page number, return which PGSIDE_* it is.
 * The first page of output is physical page 1, regardless of what
 * page number it happens to have.
 */

int
page2side(phys_page_num)

int phys_page_num;

{
	if ( ((phys_page_num & 0x1) == 0x1) == (Firstpageside == PGSIDE_LEFT)) {
		return(PGSIDE_LEFT);
	}
	else {
		return(PGSIDE_RIGHT);
	}
}


/* returns YES if given group has any "with" items
 * on the "normal" (notehead) side */

int
has_normwith(gs_p)

struct GRPSYL *gs_p;

{
	int n;
	int normside;

	normside = (gs_p->stemdir == UP ? PL_BELOW : PL_ABOVE);

	for (n = 0; n < gs_p->nwith; n++) {
		if (gs_p->withlist[n].place == normside) {
			return(YES);
		}
	}
	return(NO);
}

/* Same as above for non-normal (stem) side */

int
has_nonnormwith(gs_p)

struct GRPSYL *gs_p;

{
	int n;
	int normside;

	normside = (gs_p->stemdir == UP ? PL_BELOW : PL_ABOVE);

	for (n = 0; n < gs_p->nwith; n++) {
		if (gs_p->withlist[n].place != normside) {
			return(YES);
		}
	}
	return(NO);
}


/* Add a space padding to a string (except if is it boxed or circled).
 * If padding was added, free the passed-in string and return the padded string,
 * else return the string as is. The incoming string
 * is expected to already be converted to font/size/string
 * internal format by this time, although still in input ASCII form.
 */

char *
pad_string(string, modifier)

char *string;
int modifier;	/* TM_* */

{
	char *padded_string;		/* string with 1-space padding at end */
	char *str_p;			/* walk through padded_string */
	int len;			/* length of string */
	int last_was_backslash;		/* YES/NO */
	int count_backslashed;		/* YES/NO if to count backslashed or
					 * unbackslashed colons */
	int colons;			/* how many colons found */
	int extra;			/* how many extra bytes to malloc */


	len = strlen(string);

	/* Boxed and circled strings don't get any extra padding,
	 * so we can use what we have. We check from the end of the string,
	 * because we allow font/size changes at the beginning. */
	if (len > 5 && string[len - 2] == '\\'
			&& (string[len-1] == ']' || string[len-1] == '}')) {
		return(string);
	}

	/* Make a new copy with a space at the end.
	 * But if the string ends in the middle of a pile,
	 * we need to implicitly end the pile before adding the space.
	 * Since the string is still in ASCII form,
	 * we have to count up the number of colons
	 * to see if we are mid-pile. In chord/analysis/figbass
	 * we need to count unbackslashed colon,
	 * otherwise backslashed.*/
	count_backslashed = (IS_CHORDLIKE(modifier) ? NO : YES);
	/* figbass implicitly begins with a pile */
	colons = (modifier == TM_FIGBASS ? 1 : 0);
	last_was_backslash = NO;
	for (str_p = string + 2; *str_p != '\0'; str_p++) {
		if (last_was_backslash == YES) {
			if (*str_p == ':' && count_backslashed == YES) {
				colons++;
			}
			last_was_backslash = NO;
		}
		else {
			if (*str_p ==  ':' && count_backslashed == NO) {
				colons++;
			}
			last_was_backslash = (*str_p == '\\' ? YES : NO);
		}
	}

	/* If odd number of colons, we are mid-pile.  Will need
	 * add extra byte to hold the colon to implicitly end the
	 * pile, and if it needs to be a backslashed colon,
	 * another extra byte for that. */
	if (colons & 1) {
		extra = (count_backslashed == YES ? 2 : 1);
	}
	else {
		extra = 0;
	}

	/* +2 is for space/null at end */
	MALLOCA(char, padded_string, len + 2 + extra);
	(void) memcpy(padded_string, string, len);
	str_p = padded_string + len;

	/* add implicit end-pile if needed */
	if (extra == 2) {
		*str_p++ = '\\';
	}
	if (extra > 0) {
		*str_p++ = ':';
	}

	/* now add space padding */
	*str_p++ = ' ';
	*str_p = '\0';
	FREE(string);
	return(padded_string);
}


/* Returns a malloc-ed letter string for the passed in number.
 * 1 -> "A", 2 -> "B" ... up to 702 -> "ZZ" if baseletter is 'A'
 * or corresponding lower case if baseletter is 'a' */

#define MIN_LET_VALUE (1)
#define MAX_LET_VALUE (26+(26*26))

static char *
num2letter(num, baseletter)

int num;
int baseletter;

{
	char *ret;

	if (num < MIN_LET_VALUE || num > MAX_LET_VALUE) {
		l_yyerror(Curr_filename, yylineno,
			"Number %d is out of range (%d-%d) for conversion to letter",
			num, MIN_LET_VALUE, MAX_LET_VALUE);
		/* Clamp to nearest valid value */
		num = (num < MIN_LET_VALUE ? MIN_LET_VALUE : MAX_LET_VALUE);
	}
	/* Easier to deal with 0-25 */
	num--;
	if (num < 26) {
		MALLOCA(char, ret, 2);
		ret[0] = num + baseletter;
		ret[1] = '\0';
	}
	else {
		MALLOCA(char, ret, 3);
		ret[0] = (num / 26) + baseletter - 1;
		ret[1] = (num % 26) + baseletter;
		ret[2] = '\0';
	}
	return(ret);
}

static char *
num2uletter(num)

int num;

{
	return(num2letter(num, 'A'));
}

static char *
num2lletter(num)

int num;

{
	return(num2letter(num, 'a'));
}


/* Returns a malloc-ed roman numeral string for the passed in number.
 * 1 -> "I", 2 -> "II" ... up to 3999 -> "MMMCMXCIX" */

#define MIN_ROM_VALUE (1)
#define MAX_ROM_VALUE (3999)

static char *
num2uroman(num)

int num;

{
	int digit;
	char *roman;

	if (num < MIN_ROM_VALUE || num > MAX_ROM_VALUE) {
		l_yyerror(Curr_filename, yylineno,
			"Number %d is out of range (%d-%d) for conversion to Roman numeral",
			num, MIN_ROM_VALUE, MAX_ROM_VALUE);
		/* Clamp to nearest valid value */
		num = (num < MIN_ROM_VALUE ? MIN_ROM_VALUE : MAX_ROM_VALUE);
	}

	/* the longest possible result is MMMDCCCLXXXVIII */
	MALLOCA(char, roman, 16);

	roman[0] = '\0';

	/* convert the decimal digits one at a time, appending the results */

	static char *thou_str[] = { "", "M", "MM", "MMM" };
	digit = num / 1000;
	strcat(roman, thou_str[digit]);

	static char *hund_str[] = { "", "C", "CC", "CCC", "CD", 
				"D", "DC", "DCC", "DCCC", "CM" };
	digit = (num % 1000) / 100;
	strcat(roman, hund_str[digit]);

	static char *ten_str[] = { "", "X", "XX", "XXX", "XL", 
				"L", "LX", "LXX", "LXXX", "XC" };
	digit = (num % 100) / 10;
	strcat(roman, ten_str[digit]);

	static char *one_str[] = { "", "I", "II", "III", "IV", 
				"V", "VI", "VII", "VIII", "IX" };
	digit = num % 10;
	strcat(roman, one_str[digit]);

	return roman;
}

static char *
num2lroman(num)

int num;

{
	char *roman;
	int i;

	roman = num2uroman(num);
	for (i = 0; roman[i] != '\0'; i++) {
		roman[i] = tolower(roman[i]);
	}
	return(roman);
}


/* Comparision function for bsearch of Trans_table. The first arg in the
 * string to match, and the second is a pointer to an entry in the table */

static int
trans_compare(item1, item2)

const void *item1;
const void *item2;

{
	return( strcmp( (char*)(item1),  ((struct STRFUNC *)item2)->name) );
}


/* This implements the "string()" functions, which take a number and
 * a string that describes what kind of transform to do the the number
 * and returns a malloc-ed string that is the result of the transform
 * on the number.
 */

char *
string_func(num, transform)

int num;
char * transform;

{
	struct STRFUNC *entry;

	/* Look up the transform in the table of known ones, and if found,
	 * call its corresponding function on the number passed in. */
	if ( ( entry = (struct STRFUNC *) bsearch( transform, Trans_table,
				Trans_elements, sizeof(struct STRFUNC),
				trans_compare) ) == 0) {
		l_yyerror(Curr_filename, yylineno,
			"Unrecognized string transform %s", transform);
		return(strdup("unknown"));
	}
	return (*(entry->func))(num);
}


/* Return the width of a slash that goes through a stem, accounting for
 * current Stepsize value and size of the group. */

double
slash_xlen(grpsyl_p)

struct GRPSYL *grpsyl_p;

{
	return (SLASHHORZ * Stepsize * size2factor(grpsyl_p->grpsize));
}



/* Given a GS_* value, return the factor by which to multiply normal sized
 * things to match that size. */

double
size2factor(size)

int size;	/* GS_NORMAL, GS_SMALL, or GS_TINY */

{
	switch (size) {
	default:
		pfatal("unexpected size %d in size2factor", size);
		/*NOTREACHED*/
		/*FALLTHRU*/
	case GS_NORMAL:
		return(1.0);
	case GS_SMALL:
		return(SM_FACTOR);
	case GS_TINY:
		return(TINY_FACTOR);
	}
}
