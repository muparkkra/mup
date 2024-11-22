
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

/*
 This file deals with location tags. It handles splitting lines and curves
 when their endpoints end up on different scores.
 It also evaulates expressions in INPCOORDs.

There are 3 classes of location variables:

	A. Those associated with a specific staff of a specific score.
	This includes those for GRPSYLs and NOTES.

	B. Those associated with a specific score. These are for bars.

	C. Those associated with the current page. These are the builtin
	variables such as _win and _page, or absolute coordinates.

Type B variables, associated with bars, are extra strange. The y of a bar
in not particularly useful. If the bar line happens to wind up at the end
of a score, special consideration applies, because the right side of the
bar is effectively at the beginning of the next score after the clefsig.
So here are the rules:

1. If a bar does not occur at the end of a score, the location variable
associated with it, if any, is handled normally.

2. If a bar does fall on the end of a score, if the x coordinate derived from
a location variable associated with that bar in
an INPCOORD, after offsetting, comes out to be either left of the x of
the bar, or equal to the x of the bar, it will be handled normally.

3. If a bar falls on the end of a score, and
if the x coordinate derived from an INPCOORD, after offsetting, comes out
to be right of the x of the bar,
the x coordinate will be recalculated using the pseudo
bar at the beginning of the following score,
and if the y coordinate of the same INPCOORD is also associated
with the same bar coordinate, it will also be recalculated from the pseudo bar.

4. If rule 3 would normally apply, but there is no following score, rule 3
will be ignored, and the coordinate will be used as is.

A PRHEAD contains only a single location variable, so it should always
be taken just as is.

Lines and curves are more exciting, since they can have multiple
coordinates, and thus may need to be split into 2 or more pieces
across scores and/or pages. Lines are a degenerate case of curves,
so if we can deal with curves, we've got it made.

For drawing a specific curve, here are the rules:

1. If any coordinate is associated with a staff that is invisible, the entire
curve will be ignored.

2. Type C variables are always used as is, never themselves causing splitting.
Taking any adjacent pair of points in a curve, if either of them is of type C,
the line segment between those 2 points will not be split.

3. If all variables of type A and B are on the same score of the same page,
then the curve can be printed as is, with no splitting needed.

4. If the x and y components of a single INPCOORD are associated with
different scores, this will be an error condition.

5. If the x and y components of a single INPCOORD are associated with
different staffs, but the same score, the point will be treated as if
it were associated with the staff associated with the y coordinate.

6. If 2 adjacent points of a curve are associated with different
scores, the line segment must be split. The number of segments that will
need to be generated will be equal to the number of FEEDs between
the coordinates plus one. 

7. Splitting will only be done to forward scores. In other words, if the
coordinates of a curve would require splitting part of the curve onto a
preceding score, that will be an error. This is to keep things simpler,
since I can't think of any times this restriction would cause a problem.

8. If a segment needs to be split, the first piece will extend in the
x direction from the first point to 0.1 inch left of the right edge of the
score associated with the first point.
However, if the starting x is already at the right edge of the score, a line of length 0.1 inches will be drawn instead.
The last piece of the split line segment will extend in
the x direction from the pseudo bar of the clefsig.

8a. If there are additional scores
between the one associated with the beginning point and the one associated
with the endpoint, for each intervening score a line will be drawn with
its x coordinates from the pseudo bar to the right margin.

9. To calculate the y coordinates of each piece of a split line segment,
there are several cases. First the easy case, where the y coordinates of
the beginning and ending point are both associated with the same staff.
Conceptionally, the scores are lined up on a single line without score
feeds. The slope of the line is then calculated. The y coordinates of
the derived points are then calculated using this slope. Thus, for example,
if the ending y coordinate would be A inches from of the beginning y coordinate
in the x direction (if they were on the same score),
and the line segment is split into 2 segments, with the first having
a length in the x direction of B and the second having a length in the x
direction of C, the y coordinate of the end of the first segment would be
y[begin] + (A/(B+C)) * B, and the y coordinate of the beginning of the second
piece would be y[end] - (A/(B+C)) * C.

10. If the y coordinates of the 2 points are associated with different staffs.
the slope is calculated based on the distance of the endpoints from their
respective staffs. Then for each segment, the slope and endpoints are adjusted
based on the ratio of the distance between the two staffs on the current score
relative to the widest distance.

11. For purposes of determining y coordinates, the y and n values of a bar
are considered to be associated with the top visible score, and the s value
is considered to be associated with the bottom visible score.
Then rules 9 and 10 above are applied as for with type A coordinates.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

static int Total_pages = 1;		/* how many pages of output */

/* if lines must be added for intervening scores, save info about them */
struct SEGINFO {
	struct MAINLL *mll_p;	/* FEED where line segment must go */
	double xlength;		/* x length prior to current score */
	struct SEGINFO *next;	/* linked list */
};
struct SEGINFO *Seginfo_p;

static void gather_coord_info P((void));
static void save_coord_info P((struct COORD_INFO *coord_info_p,
		int coordtype, int page, int score, int staff,
		struct MAINLL *mll_feed_p, int vis));
static void coord_staff P((struct STAFF *staff_p, int page,
		int score, struct MAINLL *mll_feed_p));
static void split_lines_and_curves P((void));
static void chkline P((struct MAINLL *mll_p));
static void coordcheck P((struct COORD_INFO *x_info_p,
		struct COORD_INFO *y_info_p, char *fname, int lineno));
static int valid_coord P((struct INPCOORD *coord_p));
static void add_segment P((struct SEGINFO *seginfo_p, double slope,
		double y_offset, int staffno1, int linetype));
static double find_effXlength P((double seg1xlen, double seg2xlen,
		struct COORD_INFO *x1info_p, struct COORD_INFO *x2info_p,
		int save_feed_info));
static void svfeed P((struct MAINLL *mll_feed_p, double xlength));
static int eff_staff P((struct COORD_INFO *yinfo_p));
static double getYstaff P((struct MAINLL *mll_p, int staffno));
static void chkcurve P((struct MAINLL *mll_p, double staffscale));
static int bulgedir P((struct CURVE *curve_p, int index, char *inputfile,
		int inputlineno));
static int cmpcoords P((struct CURVE *curve_p, int p1, int p2));
static void add_crv_seg P((struct SEGINFO *seginfo_p, double slope,
		double y_offset, int staffno1, int curvetype, int bulge_type,
		double staffscale, char *filename, int lineno));
static int is_invis P((struct COORD_INFO *cinfo_p));
static int is_builtin P((struct COORD_INFO *cinfo_p));
static void move2correct_page P((void));
static void move_it P((struct MAINLL *m_p, struct MAINLL *im_p, int page));
static void move2pseudo P((void));
static void to_pseudo P((struct INPCOORD *inpc_p, struct COORD_INFO *info_p,
		struct MAINLL *mll_p));
static struct EXPR_NODE *find_tag_ref P((struct EXPR_NODE *expr_p, float *c_p));
static void do_pseudo P((struct INPCOORD *inpc_p, struct MAINLL *mll_p));
static void fix_inpcoords P((struct MAINLL *mll_p));
static void adj_coord P((struct INPCOORD *coord_p, struct MAINLL *mll_p,
		struct INPCOORD *prev_coord_p));
static void calc_bulge P((struct CURVE *curve_p, char *fname, int lineno,
		int is_split, struct MAINLL *mll_p));
static double get_staffscale P((struct COORD_INFO *x1info_p,
		struct COORD_INFO *x2info_p, struct COORD_INFO *y1info_p,
		struct COORD_INFO *y2info_p));
static double find_slope P((double left_y, double right_y,
		struct MAINLL *left_mll_feed_p, struct MAINLL *right_mll_feed_p,
		int left_staffnum, int right_staffnum,
		struct MAINLL *of_interest_mll_feed_p, double xlength));
static void eval_all_exprs P((void));
static double eval_expr P((struct EXPR_NODE *node_p, char *inputfile,
		int inputlineno));


/* during parse phase, a table of coordinates associated with location
 * variables was built. After all the positioning has been done, we need
 * to go through the main list and stuff off of it checking each coordinate.
 * If the coordinate is pointed to by something else, we'll need to
 * save some info about it. Then we have to go through the main list
 * again and for each line and curve, see whether it needs to be split
 * into pieces. If so, add LINE or CURVE structs at appropriate places.
 */


void
fix_locvars()

{
	/* first get info about all coordinates with loc variables */
	gather_coord_info();

	/* Evaluate all the expressions, and replace the hor and vert fields
	 * with the results of their expressions. */
	eval_all_exprs();

	/* move things to pseudo-bar if necessary */
	move2pseudo();

	/* split any lines and curves that need to be split */
	split_lines_and_curves();

	/* move anything that is on the wrong page */
	move2correct_page();
}


/* go through everything looking for coordinates. For each one found, if
 * there is a location tag pointing at it, save info about what the coord
 * is associated with (bar, note, or group), what page, score and
 * staff it's on, etc. */

static void
gather_coord_info()

{
	struct MAINLL *mll_p;		/* to walk through list */
	short page = 1;			/* which page we're on */
	short score = 0;		/* which score on current page */
	struct MAINLL *mll_feed_p;	/* FEED info for current score */
	struct COORD_INFO *coord_info_p;
	struct COORD_INFO *last_bar_coord_info_p;	/* info about the
					 * most recent bar line, in case we
					 * need to attach information about
					 * the pseudo bar at the beginning
					 * of the following score */


	debug(32, "gather_coord_info");

	initstructs();
	last_bar_coord_info_p = (struct COORD_INFO *) 0;
	/* We know that because of how the main list is set up, we will never
	 * actually access mll_feed_p without setting it first, but compilers
	 * aren't smart enough to know that, and some picky compilers warn
	 * that mll_feed_p could be used without being set, so shut them up.
	 */
	mll_feed_p = (struct MAINLL *) 0;

	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {
		switch (mll_p->str) {

		case S_FEED:
			/* keep track of which page and score we're on */
			if (mll_p->u.feed_p->pagefeed == YES) {
				page++;
				score = 1;
				Total_pages++;
			}
			else {
				score++;
			}
			if (IS_CLEFSIG_FEED(mll_p)) {
				mll_feed_p = mll_p;
			}
			if ((coord_info_p = find_coord(mll_p->u.feed_p->c)) != 0) {
				save_coord_info(coord_info_p, CT_SCORE,
					page, score, 0, mll_feed_p, YES);
			}
			break;

		case S_BAR:
			/* if bar is pointed to, save info about it */
			if ((coord_info_p = find_coord(mll_p->u.bar_p->c))
						!= (struct COORD_INFO *) 0) {

				save_coord_info(coord_info_p, CT_BAR,
							page, score, 0,
							mll_feed_p, YES);
				last_bar_coord_info_p = coord_info_p;
			}

			else {
				/* no reference to this bar, so no need to
				 * attach pseudo bar info */
				last_bar_coord_info_p = (struct COORD_INFO *) 0;
			}
			break;

		case S_CLEFSIG:
			if (mll_p->u.clefsig_p->bar_p != (struct BAR *) 0) {
				if (last_bar_coord_info_p != (struct COORD_INFO *) 0) {
					/* point bar at end of previous score
					 * to the  pseudo bar on this score */
					last_bar_coord_info_p->pseudo_bar_p
						= mll_p->u.clefsig_p->bar_p;
				}

				/* always save info, because a split curve may
				 * need to refer to it */
				add_coord(mll_p->u.clefsig_p->bar_p->c, CT_BAR);
				coord_info_p = find_coord(mll_p->u.clefsig_p->bar_p->c);
				save_coord_info(coord_info_p, CT_BAR,
					page, score, 0, mll_feed_p, YES);
			}
			break;

		case S_STAFF:
			/* will have to get info for both GRPSYLs and NOTES. */
			coord_staff(mll_p->u.staff_p, page, score, mll_feed_p);
			break;

		case S_SSV:
			/* keep track of VISIBLE status */
			asgnssv(mll_p->u.ssv_p);
			break;

		default:
			/* nothing else is of interest at this point */
			break;
		}
	}
}


/* fill in the COORD_INFO table with information about a coordinate.  */

static void
save_coord_info(coord_info_p, coordtype, page, score, staff, mll_feed_p, vis)

struct COORD_INFO *coord_info_p;/* where to add -- assumed
				 * to be non-NULL */
int coordtype;			/* CT_BAR, CT_NOTE, etc */
int page;
int score;
int staff;
struct MAINLL *mll_feed_p;	/* MAINLL containing FEED
				 * associated with score */
int vis;			/* YES if visible */

{
	if (coord_info_p == (struct COORD_INFO *) 0) {
		pfatal("invalid coordinate information");
	}

	/* make sure this phase matches parse phase */
	if ((coord_info_p->flags & coordtype) == 0) {
		pfatal("coordinate type mismatch");
	}

	/* save relevant info */
	coord_info_p->page = (short) page;
	coord_info_p->scorenum = (short) score;
	coord_info_p->staffno = (short) staff;
	coord_info_p->mll_feed_p = mll_feed_p;
	if (vis == NO) {
		coord_info_p->flags |= CT_INVISIBLE;
	}
}


/* given a STAFF struct, save relevant info about all the GRPSYL
 * and NOTE coordinates */

static void
coord_staff(staff_p, page, score, mll_feed_p)

struct STAFF *staff_p;		/* get info from here */
int page;
int score;
struct MAINLL *mll_feed_p;	/* FEED associated with this score */

{
	struct GRPSYL *gs_p;
	struct COORD_INFO *coord_info_p;
	int vis;		/* YES if staff is visible */
	register int n;		/* to walk through NOTE lists */
	register int v;		/* walk through voices/verses */


	/* do for each voice */
	for (v = 0; v < MAXVOICES; v++) {

		vis = vvpath(staff_p->staffno, v + 1, VISIBLE)->visible;
		/* for each GRPSYL in the list */
		for (gs_p = staff_p->groups_p[v]; gs_p != (struct GRPSYL *) 0;
				gs_p = gs_p->next) {

			/* check its coordinate */
			if ((coord_info_p = find_coord(gs_p->c))
					!= (struct COORD_INFO *) 0) {
				save_coord_info(coord_info_p, CT_GRPSYL,
						page, score, gs_p->staffno,
						mll_feed_p, vis);
			}

			/* if has notes, check each note coordinate */
			for (n = 0; n < gs_p->nnotes; n++) {

				if ((coord_info_p = find_coord(gs_p->notelist[n].c))
						!= (struct COORD_INFO *) 0) {

					save_coord_info(coord_info_p, CT_NOTE,
							page, score,
							gs_p->staffno,
							mll_feed_p, vis);
				}
			}

			/* if a rest, save info about its restc */
			if (gs_p->restc != 0) {
				if ((coord_info_p = find_coord(gs_p->restc))
						!= (struct COORD_INFO *) 0) {
					save_coord_info(coord_info_p, CT_NOTE,
							page, score,
							gs_p->staffno,
							mll_feed_p, vis);
				}
			}
		}
	}

	/* Do each verse */
	for (v = 0; v < staff_p->nsyllists; v++) {
		for (gs_p = staff_p->syls_p[v]; gs_p != 0; gs_p = gs_p->next) {
			if ((coord_info_p = find_coord(gs_p->c)) != 0) {
				save_coord_info(coord_info_p, CT_GRPSYL,
						page, score, gs_p->staffno,
						mll_feed_p, vis);
			}
		}
	}
}


/* go down main list. For any lines and curves, see if they need to be
 * split */

static void
split_lines_and_curves()

{
	struct MAINLL *mll_p;		/* walk through main list */


	debug(16, "split_lines_and_curves");

	initstructs();
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {

		switch(mll_p->str) {

		case S_LINE:
			fix_inpcoords(mll_p);
			chkline(mll_p);
			break;

		case S_CURVE:
			fix_inpcoords(mll_p);
			chkcurve(mll_p, -1.0);
			break;

		case S_SSV:
			asgnssv(mll_p->u.ssv_p);
			break;

		default:
			/* ignore everything else */
			break;
		}
	}
}


/* check whether a LINE needs to be split. If so, split it */

static void
chkline(mll_p)

struct MAINLL *mll_p;	/* points to LINE */

{
	struct COORD_INFO *x1info_p, *y1info_p;	/* info about coordinates
				* referenced for the beginning of the line */
	struct COORD_INFO *x2info_p, *y2info_p;	/* same for end of line */
	struct LINE *line_p;			/* the line being processed */
	struct MAINLL *new_mll_p;		/* new main list struct to add
						 * if line has to be split */
	struct LINE *end_line_p;		/* new LINE struct to hang off
						 * of new_mll_p if the line
						 * has to be split */
	double offset;
	struct MAINLL *mll_clefsig_p;		/* clefsig before a continued
						 * line segment */
	struct MAINLL *m_p;			/* for finding BAR */
	char *fname;				/* file name for messages */
	int lineno;				/* line # for messages */
	double seg1xlen, seg2xlen;		/* lengths of split segments */
	double effective_x_len;			/* effective horizontal
						 * distance of line, adding
						 * the split segments */
	double slope;				/* of effective line */
	int p1staff, p2staff;			/* effective staff associated
						  * with y coord of line ends */
	struct SEGINFO *seg_p;			/* walk through segment list */
	struct SEGINFO *to_free_p;		/* which is to be freed */


	/* if we added this line internally, it's already split, so no
	 * more to check on it */
	if (mll_p->inputlineno <= 0) {
		return;
	}

	Seginfo_p = (struct SEGINFO *) 0;

	/* get relevant info about each referenced coordinate */
	line_p = mll_p->u.line_p;
	if ( (valid_coord( &(line_p->start) ) == NO)
			 || (valid_coord( &(line_p->end) ) == NO) ) {
		/* Probably a _staff.N where N is an invisible staff.
		 * Or in any case, we have nothing to reference, so need to
		 * skip this one.
		 */
		unlinkMAINLL(mll_p);
		return;
		/* We don't free the space, since that way the function that
		 * called us can still do mll_p->next to get to the next
		 * item in list.  The space will never get reclaimed, but
		 * this case will be hit so rarely anyway, who cares
		 * about a few extra bytes? */
	}

	x1info_p = find_coord(line_p->start.hor_p);
	y1info_p = find_coord(line_p->start.vert_p);
	x2info_p = find_coord(line_p->end.hor_p);
	y2info_p = find_coord(line_p->end.vert_p);

	if (x1info_p == (struct COORD_INFO *) 0
				|| y1info_p == (struct COORD_INFO *) 0
				|| x2info_p == (struct COORD_INFO *) 0
				|| y2info_p == (struct COORD_INFO *) 0) {
		/* must be an absolute coordinate */
		return;
	}

	fname = mll_p->inputfile;
	lineno = mll_p->inputlineno;

	/* rule 1: if any invisible, ignore */
	if ( is_invis(x1info_p) || is_invis(y1info_p) || is_invis(x2info_p)
						|| is_invis(y2info_p) ) {
		/* not to be printed, so remove from main list */
		unlinkMAINLL(mll_p);
		/* As above, don't free the space */
		return;
	}

	/* rule 2:
	 * if there are any references to a builtin variable (like _cur)
	 * then there will be no split */
	if ( is_builtin(x1info_p) || is_builtin(y1info_p)
			|| is_builtin(x2info_p) || is_builtin(y2info_p) ) {
		return;
	}

	/* rule 3:
	 * if all references are on same page and score, no split needed */
	if ( (x1info_p->scorenum == y1info_p->scorenum)
			&& (x1info_p->scorenum == x2info_p->scorenum)
			&& (x1info_p->scorenum == y2info_p->scorenum)
			&& (x1info_p->page == y1info_p->page)
			&& (x1info_p->page == x2info_p->page)
			&& (x1info_p->page == y2info_p->page) ) {
		return;
	}

	/* rule 4:
	 * If x and y of a single INPCOORD are associated with different
	 * scores, we give up. (coordcheck ufatals if x and y are on
	 * different scores.)
	 */
	coordcheck(x1info_p, y1info_p, fname, lineno);
	coordcheck(x2info_p, y2info_p, fname, lineno);

	/* rule 5:
	 * if x and y are associated with different staffs,
	 * make effective staff that of the y coordinate. */
	/* figure out which staff the beginning is associated with */
	p1staff = eff_staff(y1info_p);

	/* figure out which staff end of line is associated with */
	p2staff = eff_staff(y2info_p);

	/* rule 6:
	 * Arrrgh! The line will have to be split. No specific code to do
	 * for this rule...the mere fact that we are here indicates rule 6
	 * has been satisfied */

	/* rule 7:
	 *  Make sure x2 is not behind x1. */
	if (x2info_p->page < x1info_p->page ||
			(x2info_p->page == x1info_p->page &&
			x2info_p->scorenum < x1info_p->scorenum)) {
		l_ufatal(fname, lineno,
				"can't draw line backwards to previous score");
	}

	/* So... there will have to be at least 1 more LINE struct
	 * (more if the end is more than 1 score away) */
	new_mll_p = newMAINLLstruct(S_LINE, -1);
	new_mll_p->inputfile = mll_p->inputfile;
	end_line_p = new_mll_p->u.line_p;
	end_line_p->linetype = (short) line_p->linetype;

	/* the new LINE will have its end equal to what the original LINE had */
	end_line_p->end = line_p->end;

	/* Start out with end of first segment the same as its
	 * start. Later, we'll add appropriate x and y offsets. */
	line_p->end = line_p->start;

	/* start out with last segment's beginning the same as its end.
	 * In a bit, we'll adjust the x and y appropriately. */
	end_line_p->start = end_line_p->end;

	/* rule 8:
	 * finding the x's of the new pieces isn't too bad... */

	/* the end x of the first segment is just like the beginning x,
	 * but offset to the east far enough to
	 * reach the end of the score. */
	seg1xlen = EFF_PG_WIDTH - eff_rightmargin(mll_p)
				- inpc_x( &(line_p->start), fname, lineno );

	/* handle bizarre case of beginning being too far right to deal
	 * with properly */
	if (seg1xlen < 0.1) {
		seg1xlen = 0.1;
	}
	/* The X of the end is X of the beginning plus the length of the
	 * first segment. */
	line_p->end.hor += seg1xlen;

	/* the begin x of the last segment is at the pseudo-bar */
	/* The relevant clefsig should be immediately after the FEED
	 * associated with y2 */
	mll_clefsig_p = y2info_p->mll_feed_p->next;
	if (mll_clefsig_p->str != S_CLEFSIG) {
		pfatal("missing clefsig info after newscore");
	}

	/* fill in x of beginning of final segment based on the pseudo-bar */
	end_line_p->start.hor_p = mll_clefsig_p->u.clefsig_p->bar_p->c;
	end_line_p->start.hor = end_line_p->start.hor_p[AX];

	/* effective distance in x direction will be the sum of the lengths of
	 * the first and last line segments and any intervening. We already
	 * know the length of the first segment and and now
	 * determine the lengths of the last segment. */
	seg1xlen = inpc_x( &(line_p->end), fname, lineno)
			- inpc_x( &(line_p->start), fname, lineno);
	seg2xlen = inpc_x( &(end_line_p->end), fname, lineno)
			- inpc_x( &(end_line_p->start), fname, lineno);

	/* rule 8a */
	/* check for intervening scores and find the effective length in
	 * the X direction. */
	effective_x_len = find_effXlength(seg1xlen, seg2xlen, x1info_p,
							x2info_p, YES);

	/* now find y values */

	/* figure out the first segment y relative to the effective staff */
	for (m_p = x1info_p->mll_feed_p; m_p != (struct MAINLL *) 0;
							m_p = m_p->next) {
		if (m_p->str == S_STAFF &&
				m_p->u.staff_p->staffno == p1staff) {
			break;
		}
	}
	offset = inpc_y( &(line_p->start), fname, lineno)
			- m_p->u.staff_p->c[AY];

	/* rule 9:
	 * First we tackle the easy (relatively speaking) case of both
	 * coordinates being associated with the same staff. */
	if (p1staff == p2staff) {

		/* calculate y values based on slope */
		/* Slope is calculated by taking the offsets of the beginning
		 * and ending points relative to their staff, and using the
		 * difference between them as the "rise," and the length in
		 * the X direction as if it were an unbroken line on an
		 * infinite score as the "run." */
		slope = ((end_line_p->end.vert - getYstaff(y2info_p->mll_feed_p, p2staff))
			- (line_p->start.vert - getYstaff(y1info_p->mll_feed_p, p1staff)))
			/ effective_x_len;

		/* Use the slope to the end y of the first segment and
		 * begin y of the last segment. */
		line_p->end.vert += slope * seg1xlen;
		end_line_p->start.vert -= slope * seg2xlen;

		/* if need more than 2 line segments
		 * do the rest of them */
		for (seg_p = Seginfo_p; seg_p != (struct SEGINFO *) 0;  ) {

			add_segment(seg_p, slope, offset, p1staff,
							line_p->linetype);

			/* move on the next segment in list, if any. First
			 * remember current one so we can free it, then move
			 * to next, then free the one we just finished with */
			to_free_p = seg_p;
			seg_p = seg_p->next;
			FREE(to_free_p);
		}
	}

	else {
		/* The ends are associated with different staffs */

		/* Find the slope to use on the first segment.
		 * The Y of the end of the first segment is then
		 * the segment start Y plus the slope times the X length
		 * the first segment */
		slope = find_slope(line_p->start.vert, end_line_p->end.vert,
			y1info_p->mll_feed_p, y2info_p->mll_feed_p,
			p1staff, p2staff,
			y1info_p->mll_feed_p, effective_x_len);
		line_p->end.vert += slope * seg1xlen;

		/* Do the same for the last segment */
		slope = find_slope(line_p->start.vert, end_line_p->end.vert,
			y1info_p->mll_feed_p, y2info_p->mll_feed_p,
			p1staff, p2staff,
			y2info_p->mll_feed_p, effective_x_len);
		end_line_p->start.vert -= slope * seg2xlen;

		/* If need more than 2 line segments,
		 * loop through the intermediate scores, adding segments */
		for (seg_p = Seginfo_p; seg_p != (struct SEGINFO *) 0;  ) {
			slope = find_slope(line_p->start.vert, end_line_p->end.vert,
				y1info_p->mll_feed_p, y2info_p->mll_feed_p,
				p1staff, p2staff,
				seg_p->mll_p, effective_x_len);
			add_segment(seg_p, slope, offset, p1staff,
							line_p->linetype);

			/* move on the next segment in list, if any. First
			 * remember current one so we can free it, then move
			 * to next, then free the one we just finished with */
			to_free_p = seg_p;
			seg_p = seg_p->next;
			FREE(to_free_p);
		}
	}

	/* link end_line_p into proper place in main list */
	/* this will be right before the first BAR after the FEED associated
	 * with y2 */
	for (m_p = mll_clefsig_p->next; m_p->str != S_BAR; m_p = m_p->next) {
		;
	}
	insertMAINLL(new_mll_p, m_p->prev);
}


/* Returns YES if given INPCOORD appears to have valid parse trees for
 * both x and y, NO if not. If the parse tree is just a tag reference to
 * a null tag, that it considered invalid. That can happen for cases like
 * a reference to _staff.1 if staff 1 is invisible.
 */

static int
valid_coord(coord_p)

struct INPCOORD *coord_p;

{
	if ( (coord_p->hexpr_p != 0)
			&& (coord_p->hexpr_p->op == OP_TAG_REF)
			&& (coord_p->hexpr_p->left.ltag_p->c == 0) ) {
		return(NO);
	}
	if ( (coord_p->vexpr_p != 0)
			&& (coord_p->vexpr_p->op == OP_TAG_REF)
			&& (coord_p->vexpr_p->left.ltag_p->c == 0) ) {
		return(NO);
	}
	return(YES);
}

/* check if location variables associated with an x and y point to at least
 * the same score on the same page. If not, give up */

static void
coordcheck(x_info_p, y_info_p, fname, lineno)

struct COORD_INFO *x_info_p;
struct COORD_INFO *y_info_p;
char *fname;
int lineno;

{
	if (x_info_p == (struct COORD_INFO *) 0 ||
				y_info_p == (struct COORD_INFO *) 0) {
		pfatal("coordinate not in table\n");
	}

	if ( (x_info_p->flags & CT_BUILTIN) || (y_info_p->flags & CT_BUILTIN)) {
		/* if any reference to builtin tag, leave as is */
		return;
	}

	if ( (x_info_p->scorenum != y_info_p->scorenum)
			|| (x_info_p->page != y_info_p->page) ) {
		l_ufatal(fname, lineno,
			"x and y cannot be associated with different scores");
	}
}


/* given info about a coord, return its effective staff. This is the staff
 * associated with the info if any, otherwise the top visible staff */

static int
eff_staff(yinfo_p)

struct COORD_INFO *yinfo_p;

{
	int staff;


	if (yinfo_p->staffno != 0) {
		staff = yinfo_p->staffno;
	}
	else {
		/* use top visible staff as effective staff */
		for (staff = 1; staff <= Score.staffs; staff++) {
			if (svpath(staff, VISIBLE)->visible == YES) {
				break;
			}
		}
	}
	return(staff);
}


/* find the total effective length of a line or curve, accounting for all
 * intervening scores. For each intermediate score, if the save_feed_info
 * flag is set, save away information for use in adding
 * a line or curve for that score */

static double
find_effXlength(seg1xlen, seg2xlen, x1info_p, x2info_p, save_feed_info)

double seg1xlen;	/* length of first part */
double seg2xlen;	/* length of last part */
struct COORD_INFO *x1info_p;	/* info about beginning point */
struct COORD_INFO *x2info_p;	/* info about last point */
int save_feed_info;	/* if YES, do svfeed() call, otherwise not */

{
	double effective_x_len;
	struct MAINLL *m_p;	/* to search main list */


	/* start out with length of first segment */
	effective_x_len = seg1xlen;

	/* check if there might be one or more intervening scores. If the
	 * end point is on the next page, there might be. If both are on
	 * the same page, with the first having a scorenum greater than
	 * the first one plus one, then there is an intervening score
	 * for sure. */
	if (x2info_p->page > x1info_p->page ||
			(x2info_p->page == x1info_p->page &&
			x2info_p->scorenum > x1info_p->scorenum + 1)) {
		/* search forward in main list. Every time we find
		 * a matching newscore that isn't the one associated with
		 * the last segment, save info to be able to
		 * add an intervening line. Also add the length of that line
		 * to the effective x length. */
		for (m_p = x1info_p->mll_feed_p->next;
				m_p != (struct MAINLL *) 0; m_p = m_p->next) {
			if (IS_CLEFSIG_FEED(m_p)) {
				if (m_p == x2info_p->mll_feed_p) {
					/* hurray! We found the score with
					 * the last line segment. No more to
					 * add */
					break;
				}
				else {
					/* need to add another line segment */
					if (m_p->next != (struct MAINLL *) 0 &&
						m_p->next->str == S_CLEFSIG &&
						m_p->next->u.clefsig_p->bar_p
						!= (struct BAR *) 0) {
					   if (save_feed_info == YES) {
						svfeed(m_p, effective_x_len);
					   }
					   effective_x_len += EFF_PG_WIDTH
					      - eff_rightmargin(m_p) -
					      m_p->next->u.clefsig_p->bar_p->c[AX];
					}
					else {
						pfatal("error in main list while splitting lines");
					}
				}
			}
		}
	}

	/* add in length of final segment */
	effective_x_len += seg2xlen;

	return(effective_x_len);
}


/* allocate SEGINFO and fill it in  */

static void
svfeed(mll_feed_p, xlength)

struct MAINLL *mll_feed_p;
double xlength;

{
	struct SEGINFO *new_p;


	MALLOC(SEGINFO, new_p, 1);
	new_p->mll_p = mll_feed_p;
	new_p->xlength = xlength;

	/* link onto list */
	new_p->next = Seginfo_p;
	Seginfo_p = new_p;
}


/* add LINE for intervening scores */

static void
add_segment(seginfo_p, slope, y_offset, staffno1, linetype)

struct SEGINFO *seginfo_p;
double slope;
double y_offset;	/* offset from staff of beginning point */
int staffno1;	/* staff associated with y of beginning */
int linetype;

{
	struct MAINLL *m_p;		/* index through main list */
	struct MAINLL *new_mll_p;	/* points to new LINE */
	struct LINE *new_line_p;	/* LINE connected to new_mll_p */
	double xleng;			/* distance to end in x direction */


	/* create a new LINE */
	new_mll_p = newMAINLLstruct(S_LINE, -1);
	new_line_p = new_mll_p->u.line_p;
	new_line_p->linetype = linetype;

	/* x coords of the line are at the pseudobar and the rightmargin.
	 * We get to the right margin by adding the correct distance
	 * from the pseudobar */
	new_line_p->start.hor_p = seginfo_p->mll_p->next->u.clefsig_p->bar_p->c;
	new_line_p->start.hor = new_line_p->start.hor_p[AX];
	new_line_p->start.hexpr_p = 0;
	new_line_p->end.hor_p = new_line_p->start.hor_p;
	xleng = EFF_PG_WIDTH - eff_rightmargin(seginfo_p->mll_p)
					 - new_line_p->start.hor;
	new_line_p->end.hor = new_line_p->start.hor + xleng;
	new_line_p->end.hexpr_p = 0;

	/* find staff coord info */
	for (m_p = seginfo_p->mll_p; m_p != (struct MAINLL *) 0;
							m_p = m_p->next) {
		if (m_p->str == S_STAFF && m_p->u.staff_p->staffno == staffno1) {
			break;
		}
	}
	/* To find the vertical of the start, we take the Y of the staff
	 * associated with the first point of the line, add the offset between
	 * the first staff and the first point,
	 * and then add on another offset based on the slope,
	 * which gives us where the line should be at the current X offset
	 * into the line. */
	new_line_p->start.vert_p = m_p->u.staff_p->c;
	new_line_p->start.vert = m_p->u.staff_p->c[AY] + y_offset
			+ (slope * seginfo_p->xlength);
	new_line_p->start.vexpr_p = 0;
	/* The end is similar, except using the X value at the end of
	 * the current score rather than at the pseudo bar. */
	new_line_p->end.vert_p = m_p->u.staff_p->c;
	new_line_p->end.vexpr_p = 0;
	new_line_p->end.vert = m_p->u.staff_p->c[AY] + y_offset
			+ (slope * (xleng + seginfo_p->xlength));

	/* link into proper place in main list */
	/* this will be right before the first BAR after the FEED */
	for (m_p = seginfo_p->mll_p->next; m_p != (struct MAINLL *) 0;
						m_p = m_p->next) {
		if (m_p->str == S_BAR) {
			break;
		}
	}

	if (m_p == (struct MAINLL *) 0) {
		pfatal("couldn't find bar while adding line segment");
	}

	insertMAINLL(new_mll_p, m_p->prev);
}


/* given a MAINLL and a staff number, return the absolute Y of the staff
 * searching forward from the MAINLL */

static double
getYstaff(mll_p, staffno)

struct MAINLL *mll_p;
int staffno;

{
	for (    ; mll_p != (struct MAINLL *) 0; mll_p = mll_p->next) {
		if (mll_p->str == S_STAFF) {
			if (mll_p->u.staff_p->staffno == staffno) {
				return(mll_p->u.staff_p->c[AY]);
			}
		}
	}
	pfatal("couldn't find Y of staff");
	/*NOTREACHED*/
	return(0.0);
}


/* check whether a CURVE needs to be split. If so, split it */

static void
chkcurve(mll_p, staffscale)

struct MAINLL *mll_p;	/* points to CURVE */
double staffscale;	/* if negative calculate, otherwise use as is */

{
	struct CURVE *curve_p;
	struct COORD_INFO *x1info_p, *y1info_p, *x2info_p, *y2info_p;
	int bulge;		/* 1 for UP or -1 for DOWN */
	register int n;
	int j;
	int curscore, curpage;	/* current score and page */
	int is_split = NO;
	int p1staff, p2staff;	/* staff associate with each endpoint */
	struct MAINLL *new_mll_p;	/* place for 2nd part of split curve */
	struct MAINLL *m_p;		/* to find place in list to insert */
	struct CURVE *new_crv_p;	/* points for second part */
	int ncoord1, ncoord2;	/* number of coords in each piece */
	int add1, add2;		/* 1 if need to add another point to the
				 * first of second piece of the curve,
				 * 0 if not */
	float offset;
	struct MAINLL *mll_clefsig_p;	/* clefsig for score where part of
					 * a curve goes */
	double seg1xlen, seg2xlen;	/* length of begin and end parts */
	double effective_x_len;		/* total length in X direction */
	double slope;			/* of line */
	int index1, index2;		/* into coordlist array */
	char *fname;
	int lineno;
	struct SEGINFO *seg_p, *to_free_p;	/* to deal with curves for
					 * intermediate scores */
	double addedx;		/* x of endpoint that we added */
	double userx;		/* x of nearest user-defined point */
	double y1offset;	/* Y distance of first point from its staff */
	double y2offset;	/* Y distance of second point from its staff */
	double staff_1_y;	/* Y of staff associated with first point */
	double staff_2_y;	/* Y of staff associated with second point */


	curve_p = mll_p->u.curve_p;
	if (staffscale < 0.0) {
		staffscale = get_staffscale(find_coord(curve_p->coordlist[0].hor_p),
			find_coord(curve_p->coordlist[curve_p->ncoord - 1].hor_p),
			find_coord(curve_p->coordlist[0].vert_p),
			find_coord(curve_p->coordlist[curve_p->ncoord - 1].vert_p));
	}

	bulge = 0;
	fname = mll_p->inputfile;
	lineno = mll_p->inputlineno;
	curscore = curpage = -1;

	Seginfo_p = (struct SEGINFO *) 0;

	for (n = 0; n < curve_p->ncoord; n++) {

		if ( valid_coord( &(curve_p->coordlist[n]) ) == NO) {
			/* As with lines, unlink so we don't print, but
			 * don't free, so that calling function can still
			 * follow the ->next pointer */
			unlinkMAINLL(mll_p);
			return;
		}

		x1info_p = find_coord( curve_p->coordlist[n].hor_p);
		y1info_p = find_coord( curve_p->coordlist[n].vert_p);

		if (x1info_p == (struct COORD_INFO *) 0 ||
				y1info_p == (struct COORD_INFO *) 0) {
			/* must be an absolute coordinate */
			continue;
		}

		/* rule 1: if any coordinate on the list
		 * is associated with something
		 * invisible, ignore the whole curve. */
		if ( is_invis(x1info_p) || is_invis(y1info_p) ) {

			/* as with lines, unlink so we don't print, but
			 * don't free, so that calling function can still
			 * follow the ->next pointer */
			unlinkMAINLL(mll_p);
			return;
		}

		/* rule 4. Check that x and y are on same score */
		coordcheck(x1info_p, y1info_p, fname, lineno);

		/* rule 3 checking. See if all on same score/page */
		if (curpage == -1) {
			curscore = x1info_p->scorenum;
			curpage = x1info_p->page;
		}
		else {
			if (curscore != x1info_p->scorenum ||
					curpage != x1info_p->page) {
				is_split = YES;
			}
		}
	}

	/* If this curve was specified using bulge parameters, go calculate
	 * the intermediate points */
	if (curve_p->nbulge > 0) {
		/* Since user specified a bulge, we should be able to deduce
		 * a bulge direction from that more accurately than the
		 * bulgedir() function can. If the sum of the bulge values is
		 * positive, that indicates it generally bulges UP;
		 * if negative, then DOWN. If equal, we don't know.
		 * It's not clear if this code actually is significantly
		 * more accurate than bulgedir, but it seems unlikely it
		 * would ever be worse, and since it is more algorithmic and
		 * less heuristic than bulgedir, it seems easier to defend
		 * the answer it gives us.
		 */
		int bulge_sum;
		for (bulge_sum = n = 0; n < curve_p->nbulge; n++) {
			bulge_sum += curve_p->bulgelist[n];
		}
		if (bulge_sum > 0) {
			bulge = 1;
		}
		else if (bulge_sum < 0) {
			bulge = -1;
		}

		calc_bulge(curve_p, fname, lineno, is_split, mll_p);
		/* some INPCOORDs might well point off the page, so fix those */
		fix_inpcoords(mll_p);
	}

	/* finish rule 3 checking. If all were on same score, we are done */
	if (is_split == NO) {
		return;
	}

	/* Loop through the list of points in the curve a pair at a time.
	 * For each pair, we will check if those points
	 * are on different scores. If so, that means that portion
	 * of the curve needs to be split into two or more sub-curves.
	 */
	for (n = 0; n < curve_p->ncoord - 1; n++) {
		/* Look up the information that will tell us which
		 * page/score each of the points is associated with. */
		x1info_p = find_coord( curve_p->coordlist[n].hor_p);
		y1info_p = find_coord( curve_p->coordlist[n].vert_p);
		x2info_p = find_coord( curve_p->coordlist[n + 1].hor_p);
		y2info_p = find_coord( curve_p->coordlist[n + 1].vert_p);

		if (x1info_p == (struct COORD_INFO *) 0
				|| y1info_p == (struct COORD_INFO *) 0
				|| x2info_p == (struct COORD_INFO *) 0
				|| y2info_p == (struct COORD_INFO *) 0) {
			/* We don't split any curve
			 * that contains any absolute location. */
			continue;
		}

		/* rule 2. If any builtin variable used,
		 * no split of this segment */
		if ( is_builtin(x1info_p) || is_builtin(y1info_p) ||
				is_builtin(x2info_p) || is_builtin(y2info_p) ) {
			continue;
		}

		/* rule 6. If both ends of segment are on same page/score
		 * no split of this segment */
		if ( (x1info_p->scorenum == y1info_p->scorenum)
				&& (x1info_p->scorenum == x2info_p->scorenum)
				&& (x1info_p->scorenum == y2info_p->scorenum)
				&& (x1info_p->page == y1info_p->page)
				&& (x1info_p->page == x2info_p->page)
				&& (x1info_p->page == y2info_p->page) ) {
			continue;
		}

		/* rule 7. Only split to forward score */
		if (x2info_p->page < x1info_p->page ||
				(x2info_p->page == x1info_p->page &&
				x2info_p->scorenum < x1info_p->scorenum)) {
			l_ufatal(mll_p->inputfile, mll_p->inputlineno,
				"can't draw curve backwards to previous score");
		}

		/* if we're here, segment must be split */

		/* figure out if curve generally bulges up or down */
		if (bulge == 0) { 
			bulge = bulgedir(curve_p, n, mll_p->inputfile,
							mll_p->inputlineno);
		}

		/* get effective staffs */
		p1staff = eff_staff(y1info_p);
		p2staff = eff_staff(y2info_p);
		if (y2info_p->flags & CT_BAR) {
			p2staff = p1staff;
		}

		/* set up first part of split curve. It will have as many
		 * coords as we have so far, unless that is only 2, in which
		 * case we have to add another, because a curve must have at
		 * least three points */
		if (n == 0) {
			ncoord1 = 3;
			add1 = 1;
		}
		else {
			ncoord1 = n + 2;
			add1 = 0;
		}
		/* similarly, the second portion has as many points as are
		 * left, or a minimum of 3 */
		if (curve_p->ncoord - n == 2) {
			ncoord2 = 3;
			add2 = 1;
		}
		else {
			ncoord2 = curve_p->ncoord - n;
			add2 = 0;
		}

		/* Split off the second part into a separate curve */
		new_mll_p = newMAINLLstruct(S_CURVE, mll_p->inputlineno);
		new_mll_p->inputfile = mll_p->inputfile;
		new_crv_p = new_mll_p->u.curve_p;
		new_crv_p->curvetype = curve_p->curvetype;
		new_crv_p->ncoord = (short) ncoord2;
		MALLOC (INPCOORD, new_crv_p->coordlist, ncoord2);

		/* copy second part into second curve. Copy backwards from
		 * the end, but don't fill in the first point of it, because
		 * we still need to calculate that */
		for (ncoord2--, j = curve_p->ncoord - 1; ncoord2 > 0 + add2;
							ncoord2--, j--) {
			new_crv_p->coordlist[ncoord2] = curve_p->coordlist[j];
		}

		/* Realloc space for first part of curve, with just the
		 * points for the curve's first score. */
		REALLOC(INPCOORD, curve_p->coordlist, ncoord1);
		curve_p->ncoord = (short) ncoord1;

		/* Now we need to find new endpoints for the split ends. */

		/* For the end x of the first segment of the portion of the
		 * curve that we are splitting, we will start with the same
		 * coordinate as the beginning of the portion, but then
		 * add enough horizontal offset to
		 * reach the end of the score.
		 */
		curve_p->coordlist[ncoord1 - 1] = curve_p->coordlist[0];
		offset = EFF_PG_WIDTH - eff_rightmargin(mll_p)
				- inpc_x( &(curve_p->coordlist[0]),
				fname, lineno );

		/* Handle bizarre case of beginning being too far right to deal
		 * with properly. */
		if (offset < 0.1) {
			offset = 0.1;
		}

		/* Add enough to the beginning to reach the right margin */
		curve_p->coordlist[ncoord1 - 1].hor += offset;
		curve_p->coordlist[ncoord1 - 1].hexpr_p = 0;

		/* the begin x of the last segment is at the pseudo-bar */
		/* The relevant clefsig should be immediately after the FEED
	 	 * associated with y2 */
		mll_clefsig_p = y2info_p->mll_feed_p->next;
		if (mll_clefsig_p->str != S_CLEFSIG) {
			pfatal("missing clefsig info after newscore");
		}


		/* fill in x of beginning of final part based
		 * on the pseudo-bar */
		new_crv_p->coordlist[0].hor_p
				= mll_clefsig_p->u.clefsig_p->bar_p->c;
		/* The X of the beginning is at the X of the pseudo-bar */
		new_crv_p->coordlist[0].hor = new_crv_p->coordlist[0].hor_p[AX];
		new_crv_p->coordlist[0].hexpr_p = 0;

		/* If the first user defined point on the subsequent score
		 * is extremely close to the pseudo-bar where we want to
		 * start this segment, or worse yet, is west of it
		 * (because they specified a negative offset that makes the
		 * curve bend back into the preceding measure),
		 * we move the beginning point that we added,
		 * to make it 0.1 inch west of the user's point.
		 * This is not the same remedial action as we take later for
		 * the somewhat similar case at the end of the preceding score.
		 * The argument for this lack of symmetry is that when
		 * carrying out from the end of a score, we don't want to
		 * spill out into the margin--it's better to end the curve
		 * a tiny bit too early. On the other hand, at the beginning
		 * of a score, there is probably some room in the
		 * clef/key/time area to allow starting somewhat earlier
		 * than the pseudo-bar and still look okay.
		 * Also, if the user did do some crazy curve that would
		 * bend back into the preceding measure, it's just too
		 * hard to try to do anything about that on the preceding
		 * score, but we can make it bend back prior to the
		 * pseudobar, which can sort of honor what they asked for.
		 */
		addedx = inpc_x( &(new_crv_p->coordlist[0]), fname, lineno );
		userx = inpc_x( &(new_crv_p->coordlist[1+add2]), fname, lineno );
		if (userx - addedx < 0.1) {
			new_crv_p->coordlist[0].hor =
					new_crv_p->coordlist[1+add2].hor - 0.1;
		}

		/* use the last user defined point (the one immediately
		 * before the one or two points we just added to the
		 * first part) as a reference point */
		index1 = (add1 ? 0 : curve_p->ncoord - 2);
		/* similarly, use first user-defined point of last part */
		index2 = (add2 ? 2 : 1);

		/* find y values for split ends  */

		/* Get the first approximation of the correct y values for
		 * the new endpoints we are creating,
		 * by copying the vertical information
		 * from their neighboring user-defined points.
		 * We will adjust these values later as needed based on slope.
		 * We are filling in the last point of the first segment
		 * (i.e., its east end, at the right margin),
		 * and the first point the last segment
		 * (its west end, at the pseudo-bar).
		 */
		curve_p->coordlist[ncoord1 - 1].vert_p =
				curve_p->coordlist[index1].vert_p;
		curve_p->coordlist[ncoord1 - 1].vert =
					curve_p->coordlist[index1].vert;
		curve_p->coordlist[ncoord1 - 1].vexpr_p = 0;
		new_crv_p->coordlist[0].vert_p =
				new_crv_p->coordlist[index2].vert_p;
		new_crv_p->coordlist[0].vert =
				new_crv_p->coordlist[index2].vert;
		new_crv_p->coordlist[0].vexpr_p = 0;

		/* We need to determine the effective horizontal length
		 * of the portion of the curve we are splitting,
		 * in order to determine the slope.
		 * seg1xlen will be the length of the segment
		 * on the first score, seg2xlen the length on the final score.
		 * effective_x_len will the sum of those,
		 * plus any intervening scores.
		 */
		seg1xlen = offset;
		seg2xlen = inpc_x( &(new_crv_p->coordlist[index2]), fname, lineno)
			- inpc_x( &(new_crv_p->coordlist[0]), fname, lineno);
		effective_x_len = find_effXlength(seg1xlen, seg2xlen, x1info_p,
						x2info_p, YES);

		/* Get the Y of the staffs associated with each point. */
		staff_1_y = getYstaff(y1info_p->mll_feed_p, p1staff);
		staff_2_y = getYstaff(y2info_p->mll_feed_p, p2staff);

		/* Find the Y offsets of each point relative to its staff */
		y1offset = curve_p->coordlist[index1].vert - staff_1_y;
		y2offset = new_crv_p->coordlist[index2].vert - staff_2_y;

		/* Now we will find two slopes,
		 * one for the beginning line segment,
		 * and one for the end.
		 */

		/* To get a slope to use,
		 * we pretend the first segment's score is infinitely long,
		 * and figure out what the absolute Y of the second point
		 * would be, if it were on that score, rather than the
		 * score it's really on, by adding its Y offset from its
		 * real score to the absolute Y of this pretend score.
		 * Then we subtract the Y of the beginning of the segment
		 * from that, to give us the "rise."
		 * The effective X length is the "run."
		 */
		slope = ((getYstaff(y1info_p->mll_feed_p, p2staff) + y2offset)
				 - (staff_1_y + y1offset)) / effective_x_len;

		/* The vertical of the endpoint of the first segment
		 * was initialized earlier to be the same at the
		 * first point of the segment,
		 * so we now adjust it to the proper place,
		 * based on the calculated slope, and the horizontal
		 * length of the segment.
		 */
		curve_p->coordlist[ncoord1 - 1].vert += slope * seg1xlen;

		/* To find the vertical position of the beginning point of
		 * the final segment, we do similar to how we found the end
		 * of the first segment. This time we pretend the infinite
		 * staffs are spaced like they are for the final point's,
		 * and conceptually place the first point relative to its
		 * staff, and calculate the slope, and then set the beginning
		 * point of the final segment based on that slope.
		 */
		slope = ((staff_2_y + y2offset)
			- (getYstaff(y2info_p->mll_feed_p, p1staff) + y1offset))
			/ effective_x_len;
		new_crv_p->coordlist[0].vert -= slope * seg2xlen;

		/* if need more than 2 curve segments
		 * do the rest of them */
		for (seg_p = Seginfo_p; seg_p != (struct SEGINFO *) 0;  ) {

			/* For these intermediate segments, we calculate a
			 * slope similarly to how it was done for the beginning
			 * and ending segements, but this time we use a score
			 * with staffs spread like they are on the score where
			 * we are adding the segment. So we conceptually place
			 * the first and last points relative to their staffs,
			 * as if they were on the current intermediate score,
			 * and calculate the slope of the line between them.
			 * Then we call a function to handle the complexities
			 * of adding the segment, since we need to actually
			 * draw a curve with some bulge to it,
			 * rather than just a simple line */
			slope = ((getYstaff(seg_p->mll_p, p2staff) + y2offset)
				- (getYstaff(seg_p->mll_p, p1staff) + y1offset))
				/ effective_x_len;
			add_crv_seg(seg_p, slope, y1offset,
				p1staff, curve_p->curvetype, bulge,
				staffscale, fname, lineno);

			/* move on the next segment in list, if any.
			 * First remember current one so we can free it,
			 * then move to next, then free the one
			 * we just finished with */
			to_free_p = seg_p;
			seg_p = seg_p->next;
			FREE(to_free_p);
		}

		/* If there was a user-defined point extremely close to
		 * where we did the split (which is moderately likely,
		 * since they may well specific a point at a bar line),
		 * or even worse, if the one we added somehow came out
		 * to the left of the user-defined point,
		 * the curve could end up looking very strange since it
		 * contains a very tiny segment. So in that case we discard
		 * the extra point we added as the end of the split place,
		 * and just use the user-defined point.
		 */
		addedx = inpc_x( &(curve_p->coordlist[ncoord1 - 1]), fname, lineno);
		userx = inpc_x( &(curve_p->coordlist[ncoord1 - 2]), fname, lineno);
		if (add1 == 0 && (fabs(addedx - userx) < 0.1 || userx > addedx)) {
			if (ncoord1 == 3) {
				/* If discarding a point would get us down
				 * to only two points, we'll discard the
				 * user's point, by marking that we need to
				 * fill in an extra point in the middle
				 * (at subscript [1]). The ending point we added
				 * at [2] is so close that no one should
				 * notice. */
				add1 = 1;
			}
			else {
				/* We already had more than 3 points,
				 * so we'll just ignore the extra one we
				 * added. The previous user-defined point
				 * is close enough to where it should end.
				 * It isn't worth the trouble to reclaim
				 * the extra array element; just let it leak.
				 */
				ncoord1--;
				curve_p->ncoord = ncoord1;
			}
		}

		/* if first part of curve ended up with only a single segment,
		 * need to add another point in the middle to make the
		 * required minimum of 3 points for a curve. So copy the
		 * first point, adjust the x to be halfway between the first
		 * and last point, and adjust the y to be halfway between the
		 * the y's of the endpoint, offset by a little bit to get
		 * a bend in the curve. */
		if (add1 == 1) {
			curve_p->coordlist[1] = curve_p->coordlist[0];
			curve_p->coordlist[1].hor =
					(curve_p->coordlist[0].hor
					+ curve_p->coordlist[2].hor) / 2.0;
			curve_p->coordlist[1].hexpr_p = 0;
			/* the square root is to make the amount of bulge
			 * proportional to the x length, 1 stepsize for a
			 * piece 1 inch long, less for shorter pieces,
			 * more for longer pieces */
			curve_p->coordlist[1].vert =
					(curve_p->coordlist[0].vert
					+ curve_p->coordlist[2].vert) / 2.0
					+ (bulge * STEPSIZE * sqrt(seg1xlen * Score.scale_factor));
			curve_p->coordlist[1].vexpr_p = 0;
		}

		/* similarly for the ending part of curve */
		if (add2 == 1) {
			new_crv_p->coordlist[1] = new_crv_p->coordlist[0];
			new_crv_p->coordlist[1].hor =
				(seg2xlen / 2.0) + new_crv_p->coordlist[0].hor;
			new_crv_p->coordlist[1].hexpr_p = 0;
			/* the square root is to make the amount of bulge
			 * proportional to the x length, 1 stepsize for a
			 * piece 1 inch long, less for shorter pieces,
			 * more for longer pieces */
			new_crv_p->coordlist[1].vert =
					(new_crv_p->coordlist[0].vert
					+ new_crv_p->coordlist[2].vert) / 2.0
					+ (bulge * sqrt(seg2xlen * Score.scale_factor) * STEPSIZE);
			new_crv_p->coordlist[1].vexpr_p = 0;
		}


		/* link new_mll_p into proper place in main list */
		/* this will be right before the first BAR after
		 * the FEED associated with y2 */
		for (m_p = mll_clefsig_p->next; m_p->str != S_BAR;
							m_p = m_p->next) {
			;
		}
		insertMAINLL(new_mll_p, m_p->prev);

		/* If the rest of the curve requires further splitting,
		 * we do that now, then break out of this loop */
		chkcurve(new_mll_p, staffscale);
		break;
	}
}


/* try to determine whether a user-defined curve generally bulged upward
 * or downward and return 1 for up or -1 for down as appropriate.
 * If intermediate points
 * seem to be mainly higher than the endpoints it is probably up, if they
 * tend to be below the endpoints, it is probably down. */

static int
bulgedir(curve_p, index, inputfile, inputlineno)

struct CURVE *curve_p;
int index;		/* check bulge dir between this point in array and
			 * the next one */
char *inputfile;	/* where curve was defined */
int inputlineno;

{
	int retval = 0;

	
	if (index == 0 || index == curve_p->ncoord - 2) {
		/* if checking an end of the curve, we use the two end
		 * segments to guess the direction */
		retval += cmpcoords(curve_p, 0, 1);
		retval += cmpcoords(curve_p, curve_p->ncoord - 1,
					curve_p->ncoord - 2);
		/* if more than 3 points in curve, we can use the adjacent
		 * segment on the one side where there is an adjacent segment
		 * as another reference point */
		if (curve_p->ncoord > 3) {
			if (index == 0) {
				retval += cmpcoords(curve_p, 1, 2);
			}
			else {
				retval += cmpcoords(curve_p,
						curve_p->ncoord - 2,
						curve_p->ncoord - 3);
			}
		}
	}
	else {
		/* for a segment in the middle, use the segments on
		 * either side for reference */
		retval += cmpcoords(curve_p, index - 1, index);
		retval += cmpcoords(curve_p, index + 2, index + 1);
		/* if that was inconclusive, try using the endpoints */
		if (retval == 0) {
			retval += cmpcoords(curve_p, 0, 1);
			retval += cmpcoords(curve_p, curve_p->ncoord - 1,
					curve_p->ncoord - 2);
		}
	}

	if (retval == 0) {
		/**** eventually try more drastic measures to try to deduce
		 *** the direction??? It's debatable about whether this should
		 * be a ufatal or pfatal. The program should be smart enough
		 * to figure out the direction, but probably can't be that
		 * smart for just any arbitrary curve shape
		 * the user tries to throw at it, and
		 * user can probably always manage to get what they want by
		 * specifying enough points, so make ufatal. */
		l_ufatal(inputfile, inputlineno,
			"can't determine curve bend direction; try specifying more points");
	}
	return (retval > 0 ? 1 : -1);
}


/* return 1 if point p1 appears to be below point p2. Return -1 if p1 appears
 * to be above point p2. Return 0 if can't tell */

static int
cmpcoords(curve_p, p1, p2)

struct CURVE *curve_p;
int p1;
int p2;

{
	struct COORD_INFO *y1info_p, *y2info_p;
	int staff1, staff2;
	double y1, y2;


	/* check the two points */
	y1info_p = find_coord(curve_p->coordlist[p1].vert_p);
	y2info_p = find_coord(curve_p->coordlist[p2].vert_p);

	if ((y1info_p == (struct COORD_INFO *) 0)
				|| (y2info_p == (struct COORD_INFO *) 0)) {
		pfatal("couldn't find coord info in cmpcoords");
	}

	/* if on same score, can compare the absolute Y values */
	if (y1info_p->mll_feed_p == y2info_p->mll_feed_p) {
		y1 = inpc_y( &(curve_p->coordlist[p1]), (char *) 0, -1);
		y2 = inpc_y( &(curve_p->coordlist[p2]), (char *) 0, -1);
		if (y1 < y2) {
			return(1);
		}
		else if (y2 < y1) {
			return(-1);
		}
	}
	else {
		/* weren't on same score. See if associated with same staff.
		 * If so, we can compare the relative Y values. If associated
		 * with different staffs, if second point is with lower staff
		 * it probably bulges downward. */
		staff1 = eff_staff(y1info_p);
		staff2 = eff_staff(y2info_p);
		if (staff1 == staff2) {
			/* Calculate the offset of each point from the staff */
			y1 = curve_p->coordlist[p1].vert
				- getYstaff(y1info_p->mll_feed_p, staff1);
			y2 = curve_p->coordlist[p2].vert
				- getYstaff(y2info_p->mll_feed_p, staff2);
			if (y1 < y2) {
				return(1);
			}
			else if (y2 < y1) {
				return(-1);
			}
		}
		else if (staff1 < staff2) {
			/* first point higher, bends down */
			return(-1);
		}
		else {
			return(1);
		}
	}
	return(0);
}


/* When a curve needs to be split across three or more scores,
 * this function is used to add a CURVE struct for each score other than
 * the first and the last. It is given the staff number of the initial point,
 * and the y offset of that first point from that staff, along with the slope
 * and bulge direction to use. It creates a CURVE struct with 3 points, and
 * links it into the main list at the appropriate place, based on the info
 * in the SEG_INFO that is passed in. It will be a shallow curve (2 STEPSIZES
 * of bulge) slanted according to the slope. So imagine a line drawn in the
 * horizontal dimension from the pseudo bar to the right margin,
 * at the given slope. Then bow the line by pulling the endpoint one STEPSIZE
 * in one direction and the midpoint one STEPSIZE in the opposite direction. */

static void
add_crv_seg(seginfo_p, slope, y_offset, staffno1, curvetype, bulge_type,
	staffscale, filename, lineno)

struct SEGINFO *seginfo_p;
double slope;
double y_offset;	/* offset from staff of beginning point */
int staffno1;	/* staff associated with y of beginning */
int curvetype;
int bulge_type;	/* 1 for bulge up, -1 for bulge down */
double staffscale;
char *filename;	/* where original curve was defined */
int lineno;	/* where original curve was defined */

{
	struct MAINLL *m_p;		/* index through main list */
	struct MAINLL *new_mll_p;	/* points to new LINE */
	struct CURVE *new_crv_p;	/* CURVE connected to new_mll_p */
	double bulge;			/* how much to adjust points to
					 * make a slight bow in the curve */
	double y;			/* vertical location of the curve seg */
	double xleng;			/* distance to end in x direction */


	/* create a new CURVE */
	new_mll_p = newMAINLLstruct(S_CURVE, lineno);
	new_mll_p->inputfile = filename;
	new_crv_p = new_mll_p->u.curve_p;
	new_crv_p->curvetype = (short) curvetype;
	new_crv_p->ncoord = 3;
	MALLOC (INPCOORD, new_crv_p->coordlist, 3);

	/* x coords of the curve ends are at the pseudobar and the rightmargin.
	 * The middle point, appropriately enough, is in the middle. We
	 * get to the right margin by adding the correct number of stepsizes
	 * from the pseudobar */
	new_crv_p->coordlist[0].hor_p = seginfo_p->mll_p->next->u.clefsig_p->bar_p->c;
	/* Start at pseudo bar. */
	new_crv_p->coordlist[0].hor = new_crv_p->coordlist[0].hor_p[AX];
	new_crv_p->coordlist[0].hexpr_p = 0;

	/* End at right margin. */
	xleng = EFF_PG_WIDTH - eff_rightmargin(seginfo_p->mll_p)
					- new_crv_p->coordlist[0].hor;
	new_crv_p->coordlist[2].hor = new_crv_p->coordlist[0].hor_p[AX] + xleng;
	new_crv_p->coordlist[2].hexpr_p = 0;

	/* Place middle point in the middle. */
	new_crv_p->coordlist[1].hor_p = new_crv_p->coordlist[0].hor_p;
	new_crv_p->coordlist[1].hexpr_p = 0;
	new_crv_p->coordlist[1].hor = (new_crv_p->coordlist[0].hor + new_crv_p->coordlist[2].hor) / 2.0;

	/* find staff coord info */
	for (m_p = seginfo_p->mll_p; m_p != (struct MAINLL *) 0;
							m_p = m_p->next) {
		if (m_p->str == S_STAFF && m_p->u.staff_p->staffno == staffno1) {
			break;
		}
	}
	/* y coords are determined from the slope. Offset the endpoint by 1
	 * STEPSIZE and the middle point by 1 STEPSIZE in the opposite
	 * direction to get a little bulge */
	new_crv_p->coordlist[0].vert_p = m_p->u.staff_p->c;
	/* First point goes at the passed-in y_offset from its staff,
	 * with an extra STEPSIZE adjustment to get some bulge. */
	y = m_p->u.staff_p->c[AY] + y_offset;
	bulge = bulge_type * STEPSIZE * staffscale;
	new_crv_p->coordlist[0].vert = y - bulge
			+ (slope * seginfo_p->xlength);
	new_crv_p->coordlist[0].vexpr_p = 0;

	/* End point is like first, except adjusted based on the slope */
	new_crv_p->coordlist[2].vert_p = m_p->u.staff_p->c;
	new_crv_p->coordlist[2].vert = y - bulge
			+ (slope * (xleng + seginfo_p->xlength));
	new_crv_p->coordlist[2].vexpr_p = 0;

	/* add middle point, bulging in opposite direction as the endpoints */
	new_crv_p->coordlist[1].vert_p = m_p->u.staff_p->c;
	new_crv_p->coordlist[1].vert = y + bulge
			+ (slope * (seginfo_p->xlength + (xleng / 2.0)) );
	new_crv_p->coordlist[1].vexpr_p = 0;

	/* link into proper place in main list */
	/* this will be right before the first BAR after the FEED */
	for (m_p = seginfo_p->mll_p->next; m_p != (struct MAINLL *) 0;
							m_p = m_p->next) {
		if (m_p->str == S_BAR) {
			break;
		}
	}

	if (m_p == (struct MAINLL *) 0) {
		pfatal("couldn't find bar when adding curve segment");
	}

	insertMAINLL(new_mll_p, m_p->prev);
}


/* return YES if given coordinate is invisible */

static int
is_invis(cinfo_p)

struct COORD_INFO *cinfo_p;

{
	/* It is invisible if explictly marked as such, or if
	 * it is not a builtin, but never had its page set differently
	 * than the initial default of zero. */
	if ((cinfo_p->flags & CT_INVISIBLE) ||
			(cinfo_p->page == 0 && is_builtin(cinfo_p) == NO)) {
		return(YES);
	}
	else if ((cinfo_p->flags & CT_BUILTIN) && (cinfo_p->staffno != 0)
			&& (svpath(cinfo_p->staffno, VISIBLE)->visible == NO)) {
		return(YES);
	}
	else {
		return(NO);
	}
}


/* return YES if given coordinate is a builtin location variable */

static int
is_builtin(cinfo_p)

struct COORD_INFO *cinfo_p;

{
	return((cinfo_p->flags & CT_BUILTIN) ? YES : NO); 
}


/* go through list and see if any variables were defined on one page and
 * used on another. If so, move them. */

static void
move2correct_page()

{
	int page = 1;				/* current page */
	struct MAINLL *m_p;			/* index through main list */
	struct MAINLL **insertp_p;		/* where, on each page, to
						 * insert items moved from
						 * other pages */
	struct COORD_INFO *info_p, *info1_p;	/* to see what page the item
						 * is supposed to be on */
	struct PRINTDATA *pr_p;			/* index through print list */
	struct PRINTDATA **pr_del_p_p;		/* where to delete item from
						 * list when moving */
	struct MAINLL *next_p;			/* which to check next */
	int xabs, yabs;				/* YES if x or y is absolute or
						 * builtin-relative coord */


	/* allocate array for saving where to insert things to be moved.
	 * There is no page 0, so leave extra element for that */
	CALLOC(MAINLL *, insertp_p, Total_pages + 1);


	for (m_p = Mainllhc_p; m_p != (struct MAINLL *) 0; ) {

		/* save what will be the next to check, in case the current
		 * one gets moved */
		next_p = m_p->next;

		switch(m_p->str) {

		case S_BAR:
			if (insertp_p[page] == (struct MAINLL *) 0) {
				/* find first bar on that page and save the
				 * main list struct right before that. That
				 * is where we will move anything that has to
				 * be moved to this page */
				insertp_p[page] = m_p->prev;
			}
			break;

		case S_FEED:
			if (m_p->u.feed_p->pagefeed == YES) {
				page++;
			}
			break;

		case S_LINE:
			/* only check user defined lines */
			if (m_p->inputlineno != -1) {
				info_p = find_coord(m_p->u.line_p->start.hor_p);
				if (info_p != (struct COORD_INFO *) 0 &&
						info_p->page != page &&
						((info_p->flags & CT_BUILTIN) == 0)) {
					move_it(m_p, insertp_p[info_p->page],
							info_p->page);
				}
			}
			break;

		case S_CURVE:
			/* only check user defined curves */
			if (m_p->inputlineno != -1) {
				info_p = find_coord(m_p->u.curve_p->
							coordlist[0].hor_p);
				if (info_p != (struct COORD_INFO *) 0 &&
						info_p->page != page &&
						((info_p->flags & CT_BUILTIN) == 0)) {
					move_it(m_p, insertp_p[info_p->page],
							info_p->page);
				}
			}
			break;

		case S_PRHEAD:
			for (pr_p = m_p->u.prhead_p->printdata_p, pr_del_p_p =
						&(m_p->u.prhead_p->printdata_p);
						pr_p != (struct PRINTDATA *) 0;
						pr_del_p_p = &(pr_p->next)) {

				/* find out about x and y portions */
				info_p = find_coord(pr_p->location.hor_p);
				info1_p = find_coord(pr_p->location.vert_p);

				/* figure out if x and y are absolute or
				 * associated with builtins */
				xabs = yabs = NO;
				if (info_p == (struct COORD_INFO *) 0) {
					xabs = YES;
				}
				else if (info_p->flags & CT_BUILTIN) {
					xabs = YES;
				}
				if (info1_p == (struct COORD_INFO *) 0) {
					yabs = YES;
				}
				else if (info1_p->flags & CT_BUILTIN) {
					yabs = YES;
				}
				/* if both x and y are absolute coordinates,
				 * don't move it */
				if ((xabs == YES) && (yabs == YES)) {
					pr_p = pr_p->next;
					continue;
				}

				/* if both x and y are not absolute, make sure
				 * they are associated with same staff */
				if ((xabs == NO) && (yabs == NO)) {
					coordcheck(info_p, info1_p,
							pr_p->inputfile,
							pr_p->inputlineno);
				}

				/* normally we'll check for moving based on x.
				 * (most of the time x and y will be on the same
				 * page, so we can use either.) However, if
				 * x happens to be the one that is absolute,
				 * use y instead */
				if ((xabs == YES) && (yabs == NO)) {
					info_p = info1_p;
				}

				if (info_p->page != page ) {
					struct MAINLL *new_mll_p;
					struct PRINTDATA *save_p;

					/* moving a PRINTDATA is harder than
					 * moving a line or curve, because,
					 * there could be a list, so we have
					 * surgically remove this one from the
					 * list and graft onto a new PRHEAD */
					new_mll_p = newMAINLLstruct(S_PRHEAD,
						m_p->u.prhead_p->printdata_p->
						inputlineno);
					new_mll_p->inputfile = m_p->u.prhead_p->							printdata_p->inputfile;
					new_mll_p->u.prhead_p->printdata_p
							= pr_p;

					/* save link for continuing for loop */
					save_p = pr_p->next;

					/* patch up linked list */
					*pr_del_p_p = pr_p->next;
					pr_p->next = (struct PRINTDATA *) 0;

					/* If there is a page to move it to,
					 * move it there. If page is zero,
					 * it must be associated with something
					 * invisible, so discard it. */
					if (info_p->page != 0) {
						/* move to correct page */
						if (insertp_p[info_p->page] ==
									0) {
							l_ufatal(pr_p->inputfile,
								pr_p->inputlineno,
								"forward reference to location tag");
						}
						insertMAINLL(new_mll_p,
							insertp_p[info_p->page]); 
					}
					else {
						FREE(new_mll_p);
					}

					/* prepare for next time through loop */
					pr_p = save_p;
				}
				else {
					pr_p = pr_p->next;
				}
			}

			/* if all moved, can discard */
			if (m_p->u.prhead_p->printdata_p
						== (struct PRINTDATA *) 0) {
				unlinkMAINLL(m_p);
			}
			break;

		default:
			break;
		}

		m_p = next_p;
	}
	FREE(insertp_p);
}


/* move given MAINLL to specified place */

static void
move_it(m_p, im_p, page)

struct MAINLL *m_p;	/* move this */
struct MAINLL *im_p;	/* insert here */
int page;		/* if page 0, move to oblivion */

{
	unlinkMAINLL(m_p);
	if (page == 0) {
		/* must be invisible, so discard it */
		FREE(m_p);
		return;
	}
	if (im_p == (struct MAINLL *) 0) {
		l_ufatal(m_p->inputfile, m_p->inputlineno,
				"forward reference to location tag");
	}
	insertMAINLL(m_p, im_p);
}


/* The first tag reference in the horizontal expression of an INPCOORD is
 * considered its "anchor" tag. If that anchor tag is associated with a bar
 * which is at the end of a score, but not the final bar,
 * then if the result of evaluating the horizontal expression
 * results in being to the right of that bar,
 * we replace the tag reference with its pseudo bar, and re-evaluate
 * the expression. This function loops through the main list, doing that
 * task on the INPCOORDs that need it.
 */

static void
move2pseudo()

{
	struct MAINLL *m_p;	/* walk through main list */
	struct PRINTDATA *pr_p;	/* walk through list of print commands */
	int n;			/* index through curve coordinates */


	/* go through main list */
	for (m_p = Mainllhc_p; m_p != (struct MAINLL *) 0; m_p = m_p->next) {

		if (m_p->str == S_LINE) {
			/* handle start and end points of line */
			do_pseudo( &(m_p->u.line_p->start), m_p );
			do_pseudo( &(m_p->u.line_p->end), m_p );
		}

		else if (m_p->str == S_CURVE) {
			/* do each point of curve */
			for (n = m_p->u.curve_p->ncoord - 1; n >= 0; n--) {
				do_pseudo( &(m_p->u.curve_p->coordlist[n]), m_p);
			}
		}

		else if (m_p->str == S_PRHEAD) {
			/* do each print command */
			for (pr_p = m_p->u.prhead_p->printdata_p;
					pr_p != (struct PRINTDATA *) 0;
					pr_p = pr_p->next) {
				do_pseudo( &(pr_p->location), m_p );
			}
		}
	}
}


/* Given an INPCOORD, if it has a hor_p of a bar that is at the end of
 * a score but is not the final bar, and evaluating the
 * horizontal expression resulting in something to the right of the bar,
 * this moves the hor_p and its usage to point to the pseudo-bar instead.
 * If the y of the same INPCOORD also pointed to the same bar before, move it
 * as well, otherwise leave it as is. */

static void
do_pseudo(inpc_p, mll_p)

struct INPCOORD *inpc_p;
struct MAINLL *mll_p;

{
	struct COORD_INFO *info_p;


	if ((info_p = find_coord(inpc_p->hor_p)) == (struct COORD_INFO *) 0) {
		/* probably an absolute coordinate */
		return;
	}

	/* if x is associated with a bar... */
	if (info_p->flags & CT_BAR) {
		/* and that bar has an associated pseudo bar... */
		if (info_p->pseudo_bar_p != (struct BAR *) 0) {
			/* ...and the value of the horizontal expression is
			 * in the right margin or beyond...  */
			if (inpc_x(inpc_p, (char *) 0, -1)
				> EFF_PG_WIDTH - eff_rightmargin(mll_p)) {
				to_pseudo(inpc_p, info_p, mll_p);
			}
		}
	}
}


/* Do the actual move of INPCOORD tag reference from bar to pseudo bar */

static void
to_pseudo(inpc_p, info_p, mll_p)

struct INPCOORD *inpc_p;
struct COORD_INFO *info_p;
struct MAINLL *mll_p;

{
	struct EXPR_NODE *tag_ref_p;	/* node in expression that
					 * references the anchor tag
					 * that is to be replaced */


	/* If the y of the INPCOORD was also associated with
	 * the same bar, replace its anchor tag with the pseudo-bar.
	 * We do the y first, because the hor_p we are comparing with
	 * will be changing. */
	if (inpc_p->hor_p == inpc_p->vert_p) {
		/* Locate the reference to the anchor tag in the expression and
		 * replace it with the pseudo bar. */
		if ((tag_ref_p = find_tag_ref(inpc_p->vexpr_p, inpc_p->vert_p)) != 0) {
			tag_ref_p->left.ltag_p->c = info_p->pseudo_bar_p->c;
		}
		/* Replace the y anchor tag with the pseudo bar */
		inpc_p->vert_p = info_p->pseudo_bar_p->c;
	}

	/* Locate the reference to the anchor tag in the expression and
	 * replace it with the pseudo bar. */
	if ((tag_ref_p = find_tag_ref(inpc_p->hexpr_p, inpc_p->hor_p)) != 0) {
		tag_ref_p->left.ltag_p->c = info_p->pseudo_bar_p->c;
	}
	/* Replace the x anchor tag with the pseudo bar */
	inpc_p->hor_p = info_p->pseudo_bar_p->c;

	/* Now re-evaluate the expressions based on the moved tag reference */
	eval_coord(inpc_p, mll_p->inputfile, mll_p->inputlineno);
}


/* Walk the given expression parse tree, looking for a tag reference to the
 * given c[]. Returns that node if found, or null if not found. */

static struct EXPR_NODE *
find_tag_ref(expr_p, c_p)

struct EXPR_NODE *expr_p;
float *c_p;			/* the c[] reference to find in the expr */

{
	struct EXPR_NODE *result_p;


	if (expr_p == 0) {
		return(0);
	}

	if ((expr_p->op & OP_BINARY) == OP_BINARY) {
		if ((result_p = find_tag_ref(expr_p->left.lchild_p, c_p)) != 0) {
			return(result_p);
		}
		if ((result_p = find_tag_ref(expr_p->right.rchild_p, c_p)) != 0) {
			return(result_p);
		}
	}
	if ((expr_p->op & OP_UNARY) == OP_UNARY) {
		if ((result_p = find_tag_ref(expr_p->left.lchild_p, c_p)) != 0) {
			return(result_p);
		}
	}
	if (expr_p->op == OP_TAG_REF) {
		if (expr_p->left.ltag_p->c == c_p) {
			/* Found it */
			return(expr_p);
		}
	}
	return(0);
}


/* Given a line or curve, fix any INPCOORD that end up off the margin */

static void
fix_inpcoords(mll_p)

struct MAINLL *mll_p;

{
	int n;		/* index through curve points */


	if (mll_p->str == S_CURVE) {
		for (n = 0; n < mll_p->u.curve_p->ncoord; n++) {
			adj_coord( & (mll_p->u.curve_p->coordlist[n]), mll_p,
				((n > 0) ? &(mll_p->u.curve_p->coordlist[n-1])
				: (struct INPCOORD *) 0) );
		}
	}
	else if (mll_p->str == S_LINE) {
		adj_coord( &(mll_p->u.line_p->start), mll_p,
				(struct INPCOORD *) 0);
		adj_coord( &(mll_p->u.line_p->end), mll_p,
				&(mll_p->u.line_p->start) );
	}
}


/* If x of INPCOORD ends up off the page, change the INPCOORD
 * to be on the following score, using that score's pseudo-bar
 * as the reference. */

static void
adj_coord(coord_p, mll_p, prev_coord_p)

struct INPCOORD *coord_p;	/* what to potentially adjust */
struct MAINLL *mll_p;	/* points to the line or curve containing coord_p */
struct INPCOORD *prev_coord_p;	/* previous coord if any, else NULL */

{
	struct MAINLL *m_p;		/* for finding thing in main list */
	float x, y;
	float prev_x, prev_y;		/* location of prev_coord_p */
	struct INPCOORD temp_coord;	/* reference if prev_coord_p is NULL */
	float right_margin_x;		/* EFF_PG_WIDTH - eff_rightmargin */
	float staff_y = 0.0;
	struct COORD_INFO *xinfo_p, *yinfo_p;	/* for finding which staff,
					 * clefsig, etc is associated with
					 * the point */
	struct BAR *bar_p;		/* pseudo-bar */
	int staffno;


	/* don't bother with invisible points. */
	if (prev_coord_p != (struct INPCOORD *) 0) {
		xinfo_p = find_coord(prev_coord_p->hor_p);
		yinfo_p = find_coord(prev_coord_p->vert_p);
		if (xinfo_p == (struct COORD_INFO *) 0
				|| yinfo_p == (struct COORD_INFO *) 0) {
			return;
		}

		if (is_invis(xinfo_p) == YES || is_invis(yinfo_p) == YES) {
			/* things with invisible points are ignored */
			return;
		}
	}

	xinfo_p = find_coord(coord_p->hor_p);
	yinfo_p = find_coord(coord_p->vert_p);
	if (xinfo_p == (struct COORD_INFO *) 0
				|| yinfo_p == (struct COORD_INFO *) 0) {
		return;
	}

	if (is_invis(xinfo_p) == YES || is_invis(yinfo_p) == YES) {
		return;
	}

	x = inpc_x(coord_p, (char *) 0, -1);
	y = inpc_y(coord_p, mll_p->inputfile, mll_p->inputlineno);
	prev_x = prev_y = 0.0;  /* avoid bogus "used before set" warning */

	/* Check for points being too close together. If user specifies the
	 * same point for both endpoints of a line, or something like that,
	 * PostScript might get asked to divide by zero. */
	if (prev_coord_p != (struct INPCOORD *) 0) {
		prev_x = inpc_x(prev_coord_p, mll_p->inputfile, mll_p->inputlineno);
		prev_y = inpc_y(prev_coord_p, mll_p->inputfile, mll_p->inputlineno);

		if ( (fabs(x - prev_x) < .0001) && (fabs(y - prev_y) < .0001)) {
			l_ufatal(mll_p->inputfile, mll_p->inputlineno,
				"points too close together");
		}
	}

	/* Find the x value and see if it is beyond
	 * where the right margin begins.
	 * Pretend we don't know the file/lineno, because that way if it is,
	 * no error message will be printed, which is what we
	 * want, since we hope to be able to patch things up so it isn't
	 * off the page anymore. */
	if (x < EFF_PG_WIDTH - eff_rightmargin(mll_p)) {
		/* this one is okay as is */
		return;
	}

	/* Get the staff associated with the y */
	staffno = eff_staff(yinfo_p);

	/* Make sure we don't core dump on the following "for" loop init */
	if (xinfo_p->mll_feed_p == 0) {
		/* No feed we can move to, so give up */
		return;
	}

	/* Find the y of the current staff, and the pseudo bar of the next
	 * score, where we are going to move the point to. */
	for (m_p = xinfo_p->mll_feed_p->next; m_p != (struct MAINLL *) 0;
						m_p = m_p->next) {

		if (m_p->str == S_STAFF && m_p->u.staff_p->staffno == staffno) {
			staff_y = m_p->u.staff_p->c[AY];
		}

		if (IS_CLEFSIG_FEED(m_p)) {
			/* pseudo-bar will be in CLEFSIG right after this */
			break;
		}
	}
	if (m_p == (struct MAINLL *) 0) {
		/* no future score. Give up trying to fix this one */
		return;
	}

	/* Use the pseudo-bar as reference */
	bar_p = m_p->next->u.clefsig_p->bar_p;

	/* If there was a previous point, we will use that as a reference
	 * point, otherwise make a temporary point that is the same
	 * as the current point but with no x or y offset. */
	if (prev_coord_p == (struct INPCOORD *) 0) {
		temp_coord = *coord_p;
		temp_coord.hor = temp_coord.hor_p[AX];
		temp_coord.hexpr_p = 0;
		temp_coord.vert = temp_coord.vert_p[AY];
		temp_coord.vexpr_p = 0;
		prev_coord_p = &temp_coord;
		prev_x = inpc_x(prev_coord_p, mll_p->inputfile, mll_p->inputlineno);
		prev_y = inpc_y(prev_coord_p, mll_p->inputfile, mll_p->inputlineno);
	}

	/* Use the pseudo-bar for y */
	coord_p->vert_p = bar_p->c;

	/* On the following score, where we are moving the INPCOORD,
	 * find the Y of the appropriate staff, and set the vert of the
	 * moved point to that Y offset by the same distance as the
	 * point was from its original staff.
	 */
	for (m_p = m_p->next; m_p != (struct MAINLL *) 0; m_p = m_p->next) {
		if (m_p->str == S_STAFF && m_p->u.staff_p->staffno == staffno) {
			/* The new y for the INPCOORD is the staff's y
			 * plus the relative offset. */
			coord_p->vert = m_p->u.staff_p->c[AY] + (y - staff_y);
			break;
		}
		if (m_p->str == S_BAR) {
			l_ufatal(mll_p->inputfile, mll_p->inputlineno,
				"curve is associated with staff %d, which does not exists", staffno);
		}
	}

	/* change the INPCOORD x to point to the pseudo-bar's coord array */
	coord_p->hor_p = bar_p->c;
	/* To get the x to use on the score we are moving to,
	 * we take the excess that would stick beyond the
	 * right edge of the original score, and add that to that pseudo bar
	 * X of the score we are moving to.
	 */
	right_margin_x = EFF_PG_WIDTH - eff_rightmargin(m_p);
	coord_p->hor = (x - right_margin_x) + bar_p->c[AX];

	/* If the original was really, really far off the page, even the
	 * moved version may still be off the page, so try again. Eventually
	 * either we should get within the current score or run off the end
	 * of the song and have to give up. */
	adj_coord(coord_p, mll_p, prev_coord_p);
}


/* For manual curves that use "bulge" values, figure out the intermediate
 * points for curves, and put them in the coordlist, getting rid of the
 * bulgelist.
 */

static void
calc_bulge(curve_p, fname, lineno, is_split, mll_p)

struct CURVE *curve_p;	/* curve defined using bulge */
char *fname;
int lineno;
int is_split;	/* YES if goes across at least one FEED */
struct MAINLL *mll_p;	/* for finding effective margin */

{
	double x1, y1;			/* start point location */
	double x2, y2;			/* end point location */
	double xlen, ylen;		/* distances between endpoints */
	double seg1xlen, seg2xlen;	/* lengths of parts of curve on
					 * first and last score when split */
	double sintheta, costheta;	/* for rotation */
	double length;			/* between endpoints */
	double segX, segY;		/* distance to intermediate point */
	int p1staff, p2staff;		/* staff associated with each point */
	struct INPCOORD *coordlist_p;	/* the new calculated curve */
	int n;				/* index through bulge points */
	int nbulge;			/* how many bulge points specified */
	double staffscale;
	struct COORD_INFO *x1info_p, *y1info_p;	/* to get staff info, etc */
	struct COORD_INFO *x2info_p, *y2info_p;


	nbulge = curve_p->nbulge;

	/* The calculated curve will have the 2 endpoints plus nbulge
	 * intermediate points */
	MALLOC (INPCOORD, coordlist_p, 2 + nbulge);

	/* The endpoints just get copied. All the inner points will
	 * be calculated relative to the first, so for now we copy all of
	 * the INPCOORD data from the first point into them, then later
	 * we will overwrite the offset values appropriately. */
	for (n = 0; n < nbulge + 1; n++) {
		coordlist_p[n] = curve_p->coordlist[0];
	}
	coordlist_p[nbulge + 1] = curve_p->coordlist[1];

	/* Find relevant information about the endpoints */
	x1 = inpc_x( &(curve_p->coordlist[0]), fname, lineno);
	y1 = inpc_y( &(curve_p->coordlist[0]), fname, lineno);
	x2 = inpc_x( &(curve_p->coordlist[1]), fname, lineno);
	y2 = inpc_y( &(curve_p->coordlist[1]), fname, lineno);
	
	x1info_p = find_coord( curve_p->coordlist[0].hor_p);
	y1info_p = find_coord( curve_p->coordlist[0].vert_p);
	x2info_p = find_coord( curve_p->coordlist[1].hor_p);
	y2info_p = find_coord( curve_p->coordlist[1].vert_p);

	staffscale = get_staffscale(x1info_p, x2info_p, y1info_p, y2info_p);

	/* Find the length of the line segment
	 * that would go straight between the two endpoints. To do this,
	 * we get the x and y distances to use with Pythagorean theorem */
	if (is_split == NO) {
		/* If all on same score, it is easy to find x and y */
		xlen = (x2 - x1);
		ylen = (y2 - y1);
	}
	else {
		/* Split curves take more work. First find x length
		 * on the score containing the first part of the curve */
		seg1xlen = EFF_PG_WIDTH - eff_rightmargin(mll_p) -
			inpc_x( &(curve_p->coordlist[0]), fname, lineno );


		/* match what chkcurve() does when curves are too short
		 * or backwards */
		if (seg1xlen < 0.1) {
			seg1xlen = 0.1;
		}

		/* Find the x length of the score containing the last part.
		 * To do this, have to find the pseudo-bar inside the
		 * appropriate CLEFSIG, which should be on the main list
		 * immediately following the FEED
		 * of the score containing the ending x coordinate. */
		seg2xlen = inpc_x( &(curve_p->coordlist[1]), fname, lineno) -
			x2info_p->mll_feed_p->next->u.clefsig_p->bar_p->c[AX];
		/* match what chkcurve() does when curves are too short
		 * or backwards */
		if (seg2xlen < 0.1) {
			seg2xlen = 0.1;
		}

		/* Finally, add in the x lengths of any intervening scores */
		xlen = find_effXlength(seg1xlen, seg2xlen,
						x1info_p, x2info_p, NO);

		/* Now we need the distance in the y direction. First, the
		 * easy case, when both endpoints are associated with the
		 * same staff */
		p1staff = eff_staff(y1info_p);
		p2staff = eff_staff(y2info_p);
		if (p1staff == p2staff) {
			/* To get the length in the y direction,
			 * for each endpoint, we take its absolute location
			 * and subtract the Y of its staff, to get the
			 * the Y offset from its staff. Then we subtract
			 * the end from the start.
			 */
			ylen = ((y1info_p->mll_feed_p->u.feed_p->c[AY]
				- curve_p->coordlist[0].vert)
				- (y2info_p->mll_feed_p->u.feed_p->c[AY]
				- curve_p->coordlist[1].vert)) * staffscale;
		}
		else {
			/* The endpoints are associated with different staffs.
			 * The y distance between these two staffs may vary
			 * from score to score, so to get things really
			 * accurate, we'd have to adjust the y proportionally
			 * on each intervening score, which may require adding
			 * lots of intermediate points and lots of complicated
			 * calculations which may or may not look much better
			 * than doing something more simple. So just do
			 * something fairly simple: Find the distance between
			 * each endpoint's y and its staff's y, and subtract
			 * those two distances to get an approximate ylen.
			 * As long as the distance between the two staffs is
			 * somewhat similar on both scores,
			 * which is likely to be the case,
			 * the results should be pretty good. */
			ylen = (y1info_p->mll_feed_p->next->u.clefsig_p->bar_p->c[AY] - y1) -
				(y2info_p->mll_feed_p->next->u.clefsig_p->bar_p->c[AY] - y2);
		}
	}

	/* Find distance between the endpoints */
	length = sqrt( SQUARED(xlen) + SQUARED(ylen) );

	/* Guard again divide by zero */
	if (length < 0.0001) {
		l_ufatal(fname, lineno, "curve endpoints too close together");
	}

	/* We find the intermediate points as if the line were horizontal,
	 * then rotate it, so need the sine and cosine for rotation.
	 */
	sintheta = ylen / length;
	costheta = xlen / length;

	/* Calculate the position of each inner point.  */
	for (n = 1; n <= nbulge; n++) {
		/* horizontal offset is based on a fraction of the length
		 * between endpoints: 1/2 if there is one bulge value,
		 * 1/3 and 2/3 if there are two values, etc, so use
		 * n/(nbulge + 1) to get the unrotated x lengths.
		 * Use the bulge value (which is in stepsizes)
		 * for the unrotated y length.
		 * Then to do the rotation, use
		 * x' = x costheta - y sintheta
		 * y' = y costheta + x sintheta
		 */
		segX = length * ((double) n / (double)(nbulge + 1));
		segY = curve_p->bulgelist[n-1] * STEPSIZE * staffscale;
		coordlist_p[n].hor += ((segX * costheta) - (segY * sintheta));
		coordlist_p[n].vert += ((segY * costheta) + (segX * sintheta));
	}

	/* free the old coord list, which just had the endpoints */
	FREE(curve_p->coordlist);

	/* replace the old coordlist with the newly calculated one */
	curve_p->coordlist = coordlist_p;
	curve_p->ncoord = 2 + nbulge;

	/* don't need bulgelist anymore, since it has been converted to
	 * regular curve. */
	FREE(curve_p->bulgelist);
	curve_p->bulgelist = 0;
	curve_p->nbulge = 0;
}


/* If all given coordinates information structs are associated
 * with staffs having the same staffscale value, return that,
 * otherwise return the score staffscale value. */

static double
get_staffscale(x1info_p, x2info_p, y1info_p, y2info_p)

struct COORD_INFO *x1info_p;
struct COORD_INFO *x2info_p;
struct COORD_INFO *y1info_p;
struct COORD_INFO *y2info_p;

{
	double staffscale;

	if (x1info_p == 0 || x2info_p == 0 || y1info_p == 0 || y2info_p == 0) {
		/* At least one is not associated with any staff */
		return(Score.staffscale);
	}
	if ((staffscale = svpath(x1info_p->staffno, STAFFSCALE)->staffscale) !=
			svpath(y1info_p->staffno, STAFFSCALE)->staffscale
			|| staffscale !=
			svpath(x2info_p->staffno, STAFFSCALE)->staffscale
			|| staffscale !=
			svpath(y2info_p->staffno, STAFFSCALE)->staffscale) {
		staffscale = Score.staffscale;
	}
	return(staffscale);
}


/* Find the slope to use for a line segment that is part of a line
 * that has been split across scores.
 *
 * We are given the full line's begin and end Y values and information about
 * the FEEDs of the scores they are on, and which staff each is associated with.
 * Conceptually we put the two scores next to each other.
 * The actual distances between the staff may be different.
 * Each point might be above, below, or on its associated staff.
 *
 *   ===========             =============     point 1's staff
 *
 *       1
 *
 *   ===========
 *                                2
 *                            =============    point 2's staff
 *
 * Then given the FEED of a score of interest (which might be the same
 * as the begin or end or might be some score between them) we calculate
 * what slope to use on that score. The slope will vary depending on how
 * far apart the two staffs of interest happen to be on the score of interest,
 * so the farther apart they are, the steeper the slope.
 */


static double
find_slope(left_y, right_y, left_mll_feed_p, right_mll_feed_p,
	left_staffnum, right_staffnum, of_interest_mll_feed_p, xlength)

double left_y;		/* vertical position of the left point */
double right_y;		/* vertical position of the right point */
struct MAINLL *left_mll_feed_p;	/* points to FEED associated with left point */
struct MAINLL *right_mll_feed_p; /* similar for right point */
int left_staffnum;	/* Which staff the left point is associated with */
int right_staffnum;	/* similar for right point */
struct MAINLL *of_interest_mll_feed_p;	/* we want the slope to use for this score */
double xlength;		/* The horizonal length of the line; thus the "run"
			 * part of "rise over run" for the slope */

{
	double y_offset_of_left_point_from_its_staff;
	double y_offset_of_right_point_from_its_staff;
	double where_right_y_would_be_if_on_score_of_interest;
	double where_left_y_would_be_if_on_score_of_interest;
	double rise;


	/* Find the vertical distance between the points and their staffs */
	y_offset_of_left_point_from_its_staff =
			left_y - getYstaff(left_mll_feed_p, left_staffnum);
	y_offset_of_right_point_from_its_staff =
			right_y - getYstaff(right_mll_feed_p, right_staffnum);

	/* Next we pretend that point 1 (left) is on the score of interest,
	 * and find where it would be, by placing it the same distance
	 * from the score of interest as it was from its own score. */
	where_left_y_would_be_if_on_score_of_interest =
			getYstaff(of_interest_mll_feed_p, left_staffnum)
			+ y_offset_of_left_point_from_its_staff;

	/* Similar for point 2 (right) */
	where_right_y_would_be_if_on_score_of_interest =
			getYstaff(of_interest_mll_feed_p, right_staffnum)
			+ y_offset_of_right_point_from_its_staff;

	/* Then we calculate the "rise" between our pretend points */
	rise = where_right_y_would_be_if_on_score_of_interest -
			where_left_y_would_be_if_on_score_of_interest;

	/* And finally calculate and return the "rise over the run"
	 * as the slope. */
	return(rise / xlength);
}


/* Go through the main list, and for every INPCOORD, evaluate the horizontal
 * and vertical expressions and set the hor and vert fields to their results.
 */

static void
eval_all_exprs()

{
	struct MAINLL *m_p;	/* walk through main list */
	struct PRINTDATA *pr_p;	/* walk through list of print commands */
	int n;			/* index through curve coordinates */


	/* go through main list */
	initstructs();
	for (m_p = Mainllhc_p; m_p != (struct MAINLL *) 0; m_p = m_p->next) {

		if (m_p->str == S_FEED) {
			/* _score values change at each score feed */
			int has_labels;
			prep_brac(NO, 0.0, m_p, &has_labels);
			set_score_coord(m_p);
		}
		if (m_p->str == S_FEED && m_p->u.feed_p->pagefeed == YES) {
			/* Re-evaluate _win. It may have changed if
			 * top/bot have changed, or if there are different
			 * things for left/right pages. */
			set_win(m_p->u.feed_p->north_win,
					m_p->u.feed_p->south_win,
					EFF_PG_WIDTH - eff_rightmargin(0),
					eff_leftmargin(0));
		}

		if (m_p->str == S_LINE) {
			/* handle start and end points of line */
			eval_coord( &(m_p->u.line_p->start), m_p->inputfile,
							m_p->inputlineno );
			eval_coord( &(m_p->u.line_p->end), m_p->inputfile,
							m_p->inputlineno );
		}

		else if (m_p->str == S_CURVE) {
			/* do each point of curve */
			for (n = 0; n < m_p->u.curve_p->ncoord; n++) {
				eval_coord( &(m_p->u.curve_p->coordlist[n]),
					m_p->inputfile, m_p->inputlineno );
			}
		}

		else if (m_p->str == S_PRHEAD) {
			/* do each print command */
			for (pr_p = m_p->u.prhead_p->printdata_p;
					pr_p != (struct PRINTDATA *) 0;
					pr_p = pr_p->next) {
				eval_coord( &(pr_p->location), m_p->inputfile,
						m_p->inputlineno );
			}
		}
		else if (m_p->str == S_SSV) {
			/* In case things like staffscale change */
			asgnssv(m_p->u.ssv_p);
		}
	}
}


/* Evaluate the horizontal/vertical expression in the given INPCOORD,
 * and set hor/vert fields to their results.
 */

void
eval_coord(inpcoord_p, inputfile, inputlineno)

struct INPCOORD *inpcoord_p;
char *inputfile;
int inputlineno;

{
	inpcoord_p->hor = eval_expr(inpcoord_p->hexpr_p, inputfile, inputlineno)  * STEPSIZE;
	inpcoord_p->vert = eval_expr(inpcoord_p->vexpr_p, inputfile, inputlineno) * STEPSIZE;
	if ( (inpcoord_p->hor_p == _Page) || (inpcoord_p->hor_p == 0) ) {
		inpcoord_p->hor /= Score.musicscale;
	}
	if ( (inpcoord_p->vert_p == _Page) || (inpcoord_p->vert_p == 0) ) {
		inpcoord_p->vert /= Score.musicscale;
	}
}


/* This function evaluates an expression for tag coords, as used in line,
 * curve, print, and similar statements. It is initially passed the root of
 * the parse tree, and it recusively evaluates the expression, returning
 * the result. The original caller should then set the hor or vert
 * field of the INPCOORD to the result.
 */

static double
eval_expr(node_p, inputfile, inputlineno)

struct EXPR_NODE *node_p;	/* to expression to evaluate */
char *inputfile;
int inputlineno;

{
	double value;	/* Temporarily holds a value in cases where we need
			 * to check for overflow or division by zero */
 
	if (node_p == 0) {
		l_pfatal(inputfile, inputlineno,
				"eval_expr() called on a null expression");
	}

	switch (node_p->op) {

	case OP_ADD:
		value = eval_expr(node_p->left.lchild_p, inputfile, inputlineno)
			+ eval_expr(node_p->right.rchild_p, inputfile, inputlineno);
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"addition resulted in out of range value");
		}
		return(value);
	case OP_SUB:
		value = eval_expr(node_p->left.lchild_p, inputfile, inputlineno)
			- eval_expr(node_p->right.rchild_p, inputfile, inputlineno);
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"subtraction resulted in out of range value");
		}
		return(value);
	case OP_MUL:
		value = eval_expr(node_p->left.lchild_p, inputfile, inputlineno)
			* eval_expr(node_p->right.rchild_p, inputfile, inputlineno);
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"multiplication resulted in out of range value");
		}
		return(value);
	case OP_DIV:
		value = eval_expr(node_p->right.rchild_p, inputfile, inputlineno);
		if (value == 0.0) {
			l_ufatal(inputfile, inputlineno,
				"attempt to divide by zero");
		}
		value = eval_expr(node_p->left.lchild_p, inputfile, inputlineno) / value;
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"division resulted in out of range value");
		}
		return(value);
	case OP_MOD:
		value = eval_expr(node_p->right.rchild_p, inputfile, inputlineno);
		if (value == 0.0) {
			l_ufatal(inputfile, inputlineno,
				"attempt to modulo by zero");
		}
		value = fmod(eval_expr(node_p->left.lchild_p, inputfile, inputlineno), value);
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"modulo resulted in out of range value");
		}
		return(value); 
	case OP_ATAN2:
		value = atan2(eval_expr(node_p->left.lchild_p, inputfile, inputlineno),
			eval_expr(node_p->right.rchild_p, inputfile, inputlineno));
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"out of range value for atan2");
		}
		return(RAD2DEG(value));
	case OP_HYPOT:
		value = hypot(eval_expr(node_p->left.lchild_p, inputfile, inputlineno),
			eval_expr(node_p->right.rchild_p, inputfile, inputlineno));
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"out of range value for hypot");
		}
		return(value);

	case OP_SQRT:
		value = eval_expr(node_p->left.lchild_p, inputfile, inputlineno);
		if (value < 0.0) {
			l_ufatal(inputfile, inputlineno,
				"cannot take square root of a negative number");
		}
		value = sqrt(value);
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"out of range value for sqrt");
		}
		return(value);

	case OP_SIN:
		value = sin(DEG2RAD(eval_expr(node_p->left.lchild_p, inputfile, inputlineno)));
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"attempt to get sine of infinity");
		}
		return(value);
	case OP_COS:
		value = cos(DEG2RAD(eval_expr(node_p->left.lchild_p, inputfile, inputlineno)));
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"attempt to get cosine of infinity");
		}
		return(value);
	case OP_TAN:
		value = tan(DEG2RAD(eval_expr(node_p->left.lchild_p, inputfile, inputlineno)));
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"attempt to get tangent of infinity");
		}
		if (value == HUGE_VAL) {
			l_ufatal(inputfile, inputlineno,
				"attempt to get tangent with value too big to handle");
		}
		return(value);
	case OP_ASIN:
		value = asin(eval_expr(node_p->left.lchild_p, inputfile, inputlineno));
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"attempt to get asin of invalid value");
		}
		return(RAD2DEG(value));
	case OP_ACOS:
		value = acos(eval_expr(node_p->left.lchild_p, inputfile, inputlineno));
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"attempt to get acos of invalid value");
		}
		return(RAD2DEG(value));
	case OP_ATAN:
		value = atan(eval_expr(node_p->left.lchild_p, inputfile, inputlineno));
		if (isnan(value)) {
			l_ufatal(inputfile, inputlineno,
				"attempt to get atan of invalid value");
		}
		return(RAD2DEG(value));

	case OP_FLOAT_LITERAL:
		return(node_p->left.value);
	case OP_TAG_REF:
		if (node_p->left.ltag_p->c == 0) {
			/* This would indicate a reference to an
			 * uninitialized tag, which we would have already
			 * reported as an error. So we are only here
			 * because we are trying to get report as many
			 * errors as possible before quitting. The value
			 * we are calculating won't be used,
			 * so just return zero.
			 */
			return(0.0);
		}
		return(node_p->left.ltag_p->c[node_p->left.ltag_p->c_index] / STEPSIZE);
	case OP_TIME_OFFSET:
		return(node_p->left.value / Score.timeden
			* node_p->right.rtag_p->c[INCHPERWHOLE] / STEPSIZE);
	default:
		l_pfatal(inputfile, inputlineno,
			"unknown coordinate arithmetic operator %d", node_p->op);
	}
	/*NOTREACHED*/
	/* but keep lint happy */
	return(0.0);
}
