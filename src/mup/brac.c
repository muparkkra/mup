
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

/* This file contains functions to deal with brace/bracket lists.
 * This includes things like placing the labels to minimize the space used,
 * while making sure they don't overlap in bad ways, mostly called during
 * placement phase. It also includes print phase code to print the braces,
 * brackets, and their labels. 
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/* Fuzz to allow for roundoff error when checking for zero width labels */
#define FUDGE	(.0001)

/* Padding between labels in inches at default size */
#define LABELPAD	0.1

/* space to allow for a brace or bracket */
#define BRAC_WIDTH	(1.5 * STEPSIZE)

/* Space to allow for a nested bracket */
#define NEST_WIDTH	(2.0 * STDPAD)

/* information to be able to determine overlaps in the brace/bracket lists */
static struct BRAC_INFO {
	struct STAFFSET *staffset_p;	/* bracelist or bracklist item */
	int bractype;			/* BRACELIST or BRACKLIST */
	struct BRAC_INFO *nested_p;	/* pointer to another brace/bracket
					 * item, which has its top on the
					 * same staff, and presumably
					 * is nested inside this one */
	struct BRAC_INFO *nested_by_p;	/* if this one is nested, pointer
					 * to what it is nested by, else NULL */
	short nestlevel;		/* how many levels deep */
	short topvisstaff;		/* top visible staff in range */
	short botvisstaff;		/* bottom visible staff in range */
} *Brac_info_p [MAXSTAFFS + 1];


/* information about a label, either for a staff or group. */
struct LABELINFO {
	char 	*label;		/* text of the label */
	char	*printed_label;	/* either the same as label or a copy that
				 * has been resized based on staffscale. */
	float	width;		/* strwidth(label) */
	float	west;		/* relative distance of left edge of label
				 * from the line between the labels and the
				 * braces/brackets. This will be negative */
	int	is_staff_label;	/* YES for staff label, NO for group */
	int	top;		/* if not a staff label, group's top staff */
	int	bot;		/* if not a staff label, group's bottom staff */
	float	x;		/* when printing, this is actual x */
	float	y;		/* when printing, this is first the desired y,
				 * and eventually the actual y */
	struct LABELINFO *next;/* linked list of labels at same y location */
};

/* information about all the labels that end up being printed left of a
 * specific staff or between that staff and the one below it. */
struct LABELLIST {
	short staffno;			/* which staff */
	struct LABELINFO *label_p;	/* List of labels associated with
					 * this staff. This includes staff
					 * labels as well as group labels for
					 * any groups whose logical center
					 * is this staff or between this staff
					 * and the next. It might be
					 * possible for a group's
					 * geometric center to end up above
					 * this staff, which means it really
					 * should get associated with that
					 * staff instead, but using this staff
					 * is plenty good enough in almost
					 * any non-crazy scenario.
					 */
	struct LABELINFO *btwnlabel_p;	/* list of group labels whose logical
					 * center is between this staff
					 * and the staff below. */
	short pad;			/* how many levels of labels 
					 * have been put on this staff, either
					 * on the staff itself or on one or
					 * more other staffs that are
					 * grouped with this one */
};
static struct LABELLIST Labellist[MAXSTAFFS + 1];

/* This structure is used to save away information about labels
 * in a linked list, so that they can be left justified or centered
 * if the user wants that (as specified via the "alignlabels" parameter).
 * We need to know the widest width at each nesting level to be able to do that,
 * so have to wait till all are placed.
 */
struct LABEL_JUSTIFY_INFO {
	struct LABELINFO *label_p;		/* the label */
	int level;				/* nesting level */
	int is_group;				/* YES for brace/bracket, NO for staff */
	double scale;				/* staffscale or grpscale */
	struct LABEL_JUSTIFY_INFO *next;	/* linked list */
};
static struct LABEL_JUSTIFY_INFO *Label_justify_info_list;

static short Numvis;		/* how many staffs currently visible */
static short Maxlevels;		/* maximum number of nesting levels */


/* static functions */
static void free_brac_info P((struct BRAC_INFO *brac_info_p));
static void set_brac_info P((struct STAFFSET *staffset_p, int bractype));
static int check_brac_overlap P((struct BRAC_INFO *brac_info_p));
static void setnestlevel P((struct BRAC_INFO *brac_p,
		struct BRAC_INFO *nested_by_p));
static void place_labels P((struct MAINLL *mll_p,
		struct MAINLL *prev_feed_mll_p));
static void init_labellist P((void));
static void free_label P((struct LABELINFO *label_p));
static struct LABELINFO *newlabelinfo P((char *label, int is_staff_label));
static void grouplabel P((struct BRAC_INFO *brac_p, int do_nested,
		struct MAINLL *mll_p, struct MAINLL *prev_feed_mll_p));
static double width_of_labels P((struct MAINLL *mll_p,
		struct MAINLL *prev_feed_mll_p));
static void desired_loc P((struct LABELINFO *lab_p, int staffno, double adj));
static double grpscale P((struct LABELINFO *lab_p));
static void fix_collisions P((void));
static void uncollide P((struct LABELINFO *lab_p));
static void pr_lablist P((struct LABELINFO *lab_p));
static struct MAINLL *find_prev_feed_mll_p P((struct MAINLL *mll_p));
static char * label4staff P((struct MAINLL *mll_p, int s,
		struct MAINLL *prev_feed_mll_p));
static char * label4group P((struct MAINLL *mll_p, struct BRAC_INFO *brac_p,
		struct MAINLL *prev_feed_mll_p));
static double dflt_label_width P((struct MAINLL *mll_p,
		struct MAINLL *prev_feed_mll_p));
static void save_label_justify_info P((int level, struct LABELINFO *label_p,
		int is_group, double scale));
static void free_label_justify_info_list P((void));
static void justify_labels P((void));



/* check for overlap between brace and bracket lists. Return YES if okay, NO
 * if there is something illegal */

int
brac_check (bracelist_p, nbrace, bracklist_p, nbrack)

struct STAFFSET *bracelist_p;
int nbrace;			/* how many items in bracelist_p */
struct STAFFSET *bracklist_p;
int nbrack;			/* how many items in bracklist_p */

{
	register int s;		/* staff index into Brac_info_p */
	register int n;		/* index into staffset */
	int retval = 0;		/* return from check_brac_overlap() */
	static int first_time = YES;	/* flag for if first time this function
					 * has been called */


	debug(4, "brac_check");

	/* initialize table */
	for (s = 1; s <= Score.staffs; s++) {
		if (first_time == NO) {
			/* only try to free if we know item has been properly
			 * initialized, in case this is ever run on some system
			 * that doesn't initialize pointer arrays to null ptrs */
			free_brac_info(Brac_info_p[s]);
		}
		Brac_info_p[s] = (struct BRAC_INFO *) 0;
	}
	first_time = NO;
	Maxlevels = 0;

	/* Go through each list, attaching each to table slot of its top staff.
	 */
	for (n = 0; n < nbrace; n++) {
		set_brac_info( &(bracelist_p[n]), BRACELIST);
	}
	for (n = 0; n < nbrack; n++) {
		set_brac_info( &(bracklist_p[n]), BRACKLIST);
	}

	/* now check each staff for possible overlap */
	for (s = 1; s <= Score.staffs; s++) {
		if (Brac_info_p[s] == (struct BRAC_INFO *) 0) {
			/* no braces or brackets, so can't be any overlap */
			continue;
		}

		retval += check_brac_overlap (Brac_info_p[s]);
	}

	return(retval == 0 ? YES : NO);
}


/* recursively free a linked list of BRAC_INFO structs */

static void
free_brac_info(brac_info_p)

struct BRAC_INFO *brac_info_p;	/* the list to free */

{
	if (brac_info_p == (struct BRAC_INFO *) 0) {
		return;
	}

	free_brac_info(brac_info_p->nested_p);
	FREE(brac_info_p);
}


/* save information about a brace/bracket STAFFSET and link onto list for its
 * top staff */

static void
set_brac_info (staffset_p, bractype)

struct STAFFSET *staffset_p;	/* staffs to group together */
int bractype;			/* BRACELIST or BRACKLIST */

{
	struct BRAC_INFO *new_p;	/* info to be saved */
	int s;				/* staff num of top staff of staffset */


	/* record information */
	MALLOC(BRAC_INFO, new_p, 1);
	new_p->staffset_p = staffset_p;
	new_p->bractype = bractype;
	new_p->nested_by_p = (struct BRAC_INFO *) 0;
	new_p->nestlevel = 0;

	/* link into list off of table */
	s = staffset_p->topstaff;
	new_p->nested_p = Brac_info_p[s];
	/* Note that if there is an actual nesting, the pointer in the other
	 * direction (nested_by_p) will get filled in later via a call
	 * to setnestlevel() from check_brac_overlap(). */
	Brac_info_p[s] = new_p;
}


/* check the brace/bracket information for one staff for overlap. Return
 * number of errors found */

static int
check_brac_overlap (brac_info_p)

struct BRAC_INFO *brac_info_p;

{
	register int s;


	if (brac_info_p == (struct BRAC_INFO *) 0) {
		/* end recursion */
		return(0);
	}

	/* if no nesting, don't need to do those checks */
	if (brac_info_p->nested_p != (struct BRAC_INFO *) 0) {

		/* braces can't have anything nested inside them */
		if (brac_info_p->bractype == BRACELIST) {
			yyerror("nesting inside a brace not allowed");
			return(1);
		}

		/* check that nested range is a proper subset */
		if (brac_info_p->nested_p->staffset_p->botstaff
					>= brac_info_p->staffset_p->botstaff) {
			yyerror("nested brackets must be subsets of other brackets");
			return(1);
		}

		setnestlevel(brac_info_p->nested_p, brac_info_p);
	}

	/* see if this one overlaps with groups
	 * defined previously */
	for (s = brac_info_p->staffset_p->topstaff + 1;
				s <= brac_info_p->staffset_p->botstaff; s++) {

		if (Brac_info_p[s] == (struct BRAC_INFO *) 0) {
			continue;
		}

		/* if brace is being nested by something else,
		 * overlap is illegal */
		if (brac_info_p->bractype == BRACELIST) {
			yyerror("brace overlap not allowed");
			return(1);
		}

		/* if bottom of this staffset is greater than bottom of the one
		 * we are checking, there is illegal overlap */
		if (Brac_info_p[s]->staffset_p->botstaff 
					> brac_info_p->staffset_p->botstaff) {
			yyerror("overlapping brackets are not nested");
			return(1);
		}

		/* remember who nests this one */
		setnestlevel(Brac_info_p[s], brac_info_p);
	}

	/* recurse */
	return (check_brac_overlap (brac_info_p->nested_p));
}


/* when one bracket is nested inside another, record that fact */

static void
setnestlevel(brac_p, nested_by_p)

struct BRAC_INFO *brac_p;	/* set nesting here */
struct BRAC_INFO *nested_by_p;	/* brac_p is nested by this one */

{
	brac_p->nested_by_p = nested_by_p;
	brac_p->nestlevel = nested_by_p->nestlevel + 1;

	/* keep track of deepest nesting level */
	if (brac_p->nestlevel > Maxlevels) {
		Maxlevels = brac_p->nestlevel;
	}
}


/*
 * for each label
 *	find which staff the label should go on based on visible
 *
 * Determine placement of staff labels, then nested, then outer.
 */

static void
place_labels(mll_p, prev_feed_mll_p)

struct MAINLL *mll_p;	/* current place in main list, used to determine
			 * whether to use label or label2. */
struct MAINLL *prev_feed_mll_p;	/* actual or proposed location of prev FEED */

{
	int s;			/* index through staffs */
	int count;		/* how many non-empty labels */
	char *label;		/* the label being processed */
	struct LABELINFO *lab_p;/* info about label */


	init_labellist();
	lab_p = (struct LABELINFO *) 0;

	/* put the staff labels on the label list. While we're at it, count
	 * up the number of staffs that are currently visible */
	for (count = Numvis = 0, s = 1; s <= Score.staffs; s++) {
		if (svpath(s, VISIBLE)->visible == NO) {
			continue;
		}

		/* use label or label2 as appropriate */
		if ((label = label4staff(mll_p, s, prev_feed_mll_p)) != 0) {
			lab_p = newlabelinfo(label, YES);
		}

		/* if there was a label, save info about it */
		if (lab_p != (struct LABELINFO *) 0) {

			/* Adjust by staffscale, but get from SSV, since
			 * Stepsize won't be up to date. */
			if (lab_p->width == 0.0) {
				/* If explicity the empty string, don't
				 * leave the padding for it either. */
				lab_p->west = 0.0;
			}
			else {
				double staffscale;

				staffscale = svpath(s, STAFFSCALE)->staffscale;
				lab_p->west = (-(lab_p->width) - STEPSIZE)
								* staffscale;
				save_label_justify_info(0, lab_p, NO, staffscale);
				count++;
			}

			/* link onto list */
			lab_p->next = Labellist[Numvis].label_p;
			Labellist[Numvis].label_p = lab_p;

			/* re-init for next trip through loop */
			lab_p = (struct LABELINFO *) 0;
		}

		Labellist[Numvis].staffno = (short) s;

		/* we now know there is one more staff visible */
		Numvis++;
	}

	/* if there were any labels, mark all staffs as needing padding
	 * before placing another label. If there were no staff labels,
	 * group labels will go as far east as possible, otherwise the
	 * group labels will be leftward a bit. */
	if (count > 0) {
		for (s = 0; s < Numvis; s++) {
			(Labellist[s].pad)++;
		}
	}

	/* do all nested group labels */
	for (s = 1; s <= Score.staffs; s++) {
		grouplabel(Brac_info_p[s], YES, mll_p, prev_feed_mll_p);
	}

	/* do all non-nested group labels */
	for (s = 1; s <= Score.staffs; s++) {
		grouplabel(Brac_info_p[s], NO, mll_p, prev_feed_mll_p);
	}

	/* Adjust for justification if necessary */
	justify_labels();
}


/* initialize label list. Free any information currently in the list and
 * mark everything as empty */

static void
init_labellist()

{
	register int s;		/* index through label list */


	for (s = 0; s <= Numvis; s++) {
		free_label(Labellist[s].label_p);
		free_label(Labellist[s].btwnlabel_p);
		Labellist[s].label_p = Labellist[s].btwnlabel_p
						= (struct LABELINFO *) 0;
		Labellist[s].pad = 0;
	}
	Numvis = 0;
}


/* recursively free linked list of LABELINFO structs */

static void
free_label(label_p)

struct LABELINFO *label_p;	/* free this list */

{
	if (label_p == (struct LABELINFO *) 0) {
		return;
	}

	free_label(label_p->next);
	FREE(label_p);
}


/* allocate a new LABELINFO struct and fill in the label and width. Initialize
 * west to zero */

static struct LABELINFO *
newlabelinfo(label, is_staff_label)

char *label;		/* text of the label */
int is_staff_label;	/* YES or NO */

{
	struct LABELINFO *new_p;	/* newly allocate place to save info */


	MALLOC(LABELINFO, new_p, 1);
	new_p->label = label;
	new_p->west = 0.0;
	new_p->width = strwidth(label);
	new_p->is_staff_label = is_staff_label;
	new_p->next = (struct LABELINFO *) 0;
	return(new_p);
}


/* do placement of group labels */

static void
grouplabel(brac_p, do_nested, mll_p, prev_feed_mll_p)

struct BRAC_INFO *brac_p;	/* info about group of staffs to do */
int do_nested;			/* if YES, process nested staff group. If NO,
				 * process non-nested */
struct MAINLL *mll_p;		/* used to decide if to use label or label2 */
struct MAINLL *prev_feed_mll_p;	/* actual or proposed previous FEED */

{
	struct STAFFSET *staffset_p;	/* staffs/label in group */
	char *label;			/* label for group */
	int index;			/* into Labellist */
	int topindex, botindex;		/* index into Labellist of where
					 * group range top & bottom visible
					 * staffs are */
	int labindex;			/* index into Labellist of staff where
					 * label should go */
	struct LABELINFO *lab_p;	/* information about group label */
	struct LABELINFO **lab_p_p;	/* where to insert label info */
	double scale;			/* effective staffscale for group */


	if (brac_p == (struct BRAC_INFO *) 0) {
		/* end recursion */
		return;
	}

	if (do_nested == YES) {
		/* recurse */
		grouplabel(brac_p->nested_p, do_nested, mll_p, prev_feed_mll_p);
		if (brac_p->nested_by_p == (struct BRAC_INFO *) 0) {
			return;
		}
	}
	else if (brac_p->nested_by_p != (struct BRAC_INFO *) 0) {
		return;
	}

	/* we'll probably need the staffset info a lot, so get pointer to it */
	staffset_p = brac_p->staffset_p;

	/* Find index in Labellist of top
	 * and bottom visible staffs of the range */
	for (topindex = botindex = -1, index = 0;  index < Numvis; index++) {
		if (topindex == -1 && staffset_p->topstaff
						<= Labellist[index].staffno) {
			topindex = index;
		}
		if (staffset_p->botstaff >= Labellist[index].staffno) {
			botindex = index;
		}
	}

	/* see if there were some visible staffs in this group */
	if (topindex != -1 && botindex != -1 && botindex >= topindex) {

		brac_p->topvisstaff = Labellist[topindex].staffno;
		brac_p->botvisstaff = Labellist[botindex].staffno;

		/* figure out which label to use, if any */
		if ((label = label4group(mll_p, brac_p, prev_feed_mll_p))
							== (char *) 0) {
			return;
		}

		/* find index in list of visible staffs where label should
		 * go. If even number of visible staffs in range, label
		 * goes between two staffs */
		labindex = (topindex + botindex) / 2;
		if ((botindex - topindex) & 1) {
			lab_p_p = &(Labellist[labindex].btwnlabel_p);
		}
		else {
			lab_p_p = &(Labellist[labindex].label_p);
		}

		lab_p = newlabelinfo(label, NO);

		/* link onto list */
		lab_p->next = *lab_p_p;
		*lab_p_p = lab_p;

		/* add padding to all visible staffs in the group range */
		for (    ; topindex <= botindex; topindex++) {
			Labellist[topindex].pad++;
		}

		/* save which staff range this group is for */
		lab_p->top = brac_p->topvisstaff;
		lab_p->bot = brac_p->botvisstaff;

		/* Put as far east as possible, allowing space for padding
		 * for other labels if needed, then adjust by the
		 * effective staffscale factor. */
		lab_p->west = (-(lab_p->width) - STEPSIZE);
		lab_p->west -= Labellist[labindex].pad * LABELPAD;
		scale = grpscale(lab_p);
		lab_p->west *= scale;
		/* Save information for in case we need to justify */
		save_label_justify_info(brac_p->nestlevel + 1, lab_p, YES, scale);
	}
	else {
		/* all staffs in group are invisible */
		brac_p->topvisstaff = 0;
	}
}


/* determine total width of labels. This is how much to add to
 * relative west to get absolute location from left margin */

static double
width_of_labels(mll_p, prev_feed_mll_p)

struct MAINLL *mll_p;		/* actual or proposed FEED location,
				 * used to decide if to use label or label2 */
struct MAINLL *prev_feed_mll_p;	/* actual or proposed location of preceding
				 * FEED, used for label/label2 decision */

{
	register int s;		/* index */
	double minwest = 0.0;	/* farthest west distance */
	int num_staff_labels = 0;


	/* find westernmost label */
	for (s = 0; s < Numvis; s++) {
		if (Labellist[s].label_p != (struct LABELINFO *) 0) {
			num_staff_labels++;
			if (Labellist[s].label_p->west < minwest) {
				minwest = Labellist[s].label_p->west;
			}
		}
		if (Labellist[s].btwnlabel_p != (struct LABELINFO *) 0) {
			if (Labellist[s].btwnlabel_p->west < minwest) {
				minwest = Labellist[s].btwnlabel_p->west;
			}
		}
	}

	/* check for need to use default label on first score.
	 * If default label is needed, it creates an indent. */
	if (minwest == 0.0 && num_staff_labels < Numvis) {
		return(dflt_label_width(mll_p, prev_feed_mll_p));
	}

	return( - minwest);
}


/* return width of braces/brackets and their labels */

double
width_left_of_score(mll_p)

struct MAINLL *mll_p;	/* FEED, used to decide if to use label or label2 */

{
	return(pwidth_left_of_score(mll_p, find_prev_feed_mll_p(mll_p)));
}

double
pwidth_left_of_score(mll_p, prev_feed_mll_p)

struct MAINLL *mll_p;		/* actual or proposed location of current FEED,
				 * used to decide if to use label or label2 */
struct MAINLL *prev_feed_mll_p;	/* actual or proposed location of prev FEED */

{
	double labwidth;	/* width of labels */
	int n;			/* index through brac*lists */
	int s;			/* staff index */
	int hasbracs;		/* YES if there are visible brackets/braces */


	if (brac_check(Score.bracelist, Score.nbrace, Score.bracklist,
					Score.nbrack) == NO) {
		/* we should have exited before */
		pfatal("illegal brace/bracket ranges");
	}
	/* call functions to determine the placement of all labels and
	 * save that information in the Labellist, then determine how
	 * wide the labels plus braces and brackets are */
	place_labels(mll_p, prev_feed_mll_p);
	labwidth = width_of_labels(mll_p, prev_feed_mll_p);

	/* First see if there are any visible brackets/braces.
	 * If so, we'll need to allow space for them, otherwise not. */
	hasbracs = NO;
	for (n = 0; n < Score.nbrace && hasbracs == NO; n++) {
		for (s = Score.bracelist[n].topstaff;
					s <= Score.bracelist[n].botstaff; s++){
			if (svpath(s, VISIBLE)->visible == YES) {
				hasbracs = YES;
				break;
			}
		}
	}
	for (n = 0; n < Score.nbrack && hasbracs == NO; n++) {
		for (s = Score.bracklist[n].topstaff;
					s <= Score.bracklist[n].botstaff; s++){
			if (svpath(s, VISIBLE)->visible == YES) {
				hasbracs = YES;
				break;
			}
		}
	}

	/* To calculate total space, we start with space for the labels,
	 * which is the labwidth that was gotten above.
	 * Then if there are braces and/or brackets,
	 * we add space for the maximum number of them.
	 * That's BRAC_WIDTH for the first and NEST_WIDTH for
	 * each additional nested ones.
	 * Then if there were labels, we add one extra nest width
	 * to allow some padding between them and the labels.
	 * If you change this code, the matching code in pr_brac()
	 * will need to change similarly.
	 */
	if (hasbracs == YES) {
		if (labwidth < FUDGE) {
			/* Must not have been any labels--or at least
			 * they are so narrow we can ignore them! */
			return(BRAC_WIDTH + (Maxlevels - 1) * NEST_WIDTH);
		}
		else {
			return(labwidth + BRAC_WIDTH
					+ (Maxlevels * NEST_WIDTH));
		}
	}
	else {
		return(labwidth);
	}
}


/* Prepare to print braces/brackets and their labels, by figuring out
 * where all the labels go. Returns an altered x_offset that should then
 * be passed to pr_brac(), and via has_labels_p whether there are labels,
 * which should also be passed to pr_brac. This used to all be combined
 * in pr_brac() by with addition of support for _staffN tags, this code
 * needs to happen sooner. */

double
prep_brac(is_restart, x_offset, mll_p, has_labels_p)

int is_restart;		/* YES if being called due to restart */
double x_offset;	/* If is_restart == YES, this is the x location at
			 * which to print brace/bracket. Otherwise this
			 * function will ignore this passed in value and 
			 * calculate the correct value itself, based on
			 * left margin and width of the labels. */
struct MAINLL *mll_p;	/* for FEED for possible margin override, and to
			 * decide if to use label or label2 */
int *has_labels_p;	/* to return whether there are labels. This should
			 * then be passed to pr_brac() */

{
	register int li;		/* index into Labellist */
	struct MAINLL *prev_feed_mll_p;	/* previous FEED */


	debug(512, "prep_brac");

	/* figure out where to place everything */
	(void) brac_check(Score.bracelist, Score.nbrace, Score.bracklist,
								Score.nbrack);
	prev_feed_mll_p = find_prev_feed_mll_p(mll_p);
	place_labels(mll_p, prev_feed_mll_p);
	*has_labels_p = NO; /* assume till proven otherwise */
	if (is_restart == NO) {
		/* Calculate the correct x offset for printing.
		 * Start with the width of labels. */
		x_offset = width_of_labels(mll_p, prev_feed_mll_p);

		/* Make a note of whether there were any labels,
		 * so we know a bit later whether to add any
		 * padding between the labels and brace/bracket. */
		*has_labels_p =  (x_offset > FUDGE ? YES : NO);

		/* Add on the margin and allow a little space for the
		 * innermost brace/bracket to get the final correct x. */
		x_offset += eff_leftmargin(mll_p) - BRAC_WIDTH;

		/* Calulate desired location for printing
		 * labels associated with this staff. */
		for (li = 0; li < Numvis; li++) {
			desired_loc(Labellist[li].label_p,
					Labellist[li].staffno, x_offset);
			desired_loc(Labellist[li].btwnlabel_p,
					Labellist[li].staffno, x_offset);
		}

		/* Find any collisions that would result from printing labels
		 * at their ideal locations, and adjust to not collide */
		fix_collisions();
	}
	else {
		x_offset += HALF_RESTART_WIDTH - BRAC_WIDTH - 2.0 * NEST_WIDTH;
	}

	return(x_offset);
}


/* Print braces, brackets, and all the labels for them and individual staffs/
 * Return YES if there were braces or brackets, NO if not. */

int
pr_brac(is_restart, x_offset, has_label)

int is_restart;		/* YES if being called due to restart */
double x_offset;	/* this is the x location at which to print
			 * brackets/braces */
int has_label;		/* what prep_brac returned via has_label_p */

{
	register int li;		/* label index */
	register int s;			/* staff index */
	struct BRAC_INFO *brac_p;	/* info about brace or bracket */
	double x, y, y1;
	int eff_stafflines;		/* how many stafflines there effectively
					 * are, counting the extra space around
					 * staffs with a very small number
					 * of stafflines */
	double tab_adjust;		/* to adjust for TABRATIO */
	double eff_stepsize;		/* STEPSIZE adjusted for staffscale */
	int printed_brac = NO;		/* if printed any braces/brackets */

	if (is_restart == NO) {
		/* Loop through and print the labels */
		for (li = 0; li < Numvis; li++) {
			pr_lablist(Labellist[li].label_p);
			pr_lablist(Labellist[li].btwnlabel_p);
		}
	}

	/* print the braces and brackets themselves */
	for (s = 1; s <= Score.staffs; s++) {
		for (brac_p = Brac_info_p[s]; brac_p != (struct BRAC_INFO *) 0;
						brac_p = brac_p->nested_p) {
			x = x_offset + BRAC_WIDTH +
				(Maxlevels - brac_p->nestlevel) * NEST_WIDTH;
			if (has_label == YES) {
				/* There was a label, so add space
				 * to make padding between the label and
				 * the brace/bracket. This is to match the
				 * code in pwidth_left_of_score() */
				x += NEST_WIDTH;
			}
			if (brac_p->bractype == BRACELIST) {
				if (brac_p->nested_by_p == 0) {
					x += (0.5 * STEPSIZE);
				}
				else {
					x -= (0.5 * STEPSIZE);
				}
			}
			if (brac_p->topvisstaff > 0) {
				/* figure out y (the top). Start at the y
				 * of the top staff, then adjust as needed. */
				y = Staffs_y [brac_p->topvisstaff];

				/* figure out how tall the staff is effectively.
				 * Staffs with only a few stafflines are
				 * effectively taller than the number of
				 * stafflines. */
				if ((eff_stafflines = svpath(
						brac_p->topvisstaff, STAFFLINES)						->stafflines) < 3) {
					eff_stafflines = 3;
				}
				/* stepsizes are taller on tab staffs */
				tab_adjust = (is_tab_staff(brac_p->topvisstaff)
							? TABRATIO : 1.0);

				/* adjust for height of staff */
				eff_stepsize = svpath(brac_p->topvisstaff,
						STAFFSCALE)->staffscale
						* STEPSIZE;
				y += (eff_stafflines - 1) * eff_stepsize
						* tab_adjust;

				/* nested brackets should be a little shorter
				 * vertically to fit inside their parent.
				 * But beyond about 4 levels, if there is
				 * only a single staff, things look
				 * pretty bad, so limit to 4. */
				y -= (eff_stepsize * (brac_p->nestlevel < 5
					? brac_p->nestlevel : 4));

				/* brackets are 1 stepsize taller than braces */
				if (brac_p->bractype == BRACKLIST) {
					y += eff_stepsize;
				}

				/* now calculate y1 (the bottom) by similar
				 * means */
				y1 = Staffs_y [brac_p->botvisstaff];
		
				/* figure out how tall the staff is effectively.
				 * Staffs with only a few stafflines are
				 * effectively taller than the number of
				 * stafflines. */
				if ((eff_stafflines = svpath(
						brac_p->botvisstaff, STAFFLINES)						->stafflines) < 3) {
					eff_stafflines = 3;
				}
				/* stepsizes are taller on tab staffs */
				tab_adjust = (is_tab_staff(brac_p->botvisstaff)
							? TABRATIO : 1.0);

				/* adjust for height of staff */
				eff_stepsize = svpath(brac_p->botvisstaff,
						STAFFSCALE)->staffscale
						* STEPSIZE;
				y1 -= (eff_stafflines - 1) * eff_stepsize
						* tab_adjust;

				/* nested brackets should be a little shorter
				 * vertically to fit inside their parent.
				 * But beyond about 4 levels, if there is
				 * only a single staff, things look
				 * pretty bad, so limit to 4. */
				y1 += (eff_stepsize * (brac_p->nestlevel < 5
					? brac_p->nestlevel : 4));

				/* brackets are 1 stepsize taller than braces */
				if (brac_p->bractype == BRACKLIST) {
					y1 -= eff_stepsize;
				}

				/* now do the actual printing */
				do_pr_brac(x, y, y1, brac_p->bractype);
				printed_brac = YES;
			}
		}
	}

	return(printed_brac);
}


/* Given a list of LABELINFOs, walk down the list, and for each,
 * calculate the y at which we would like the print the label.
 * For a staff label, y will be centered on the staff.
 * For a group label, y will be centered on the brace or bracket.
 * Later we may discover using these ideal y locations will cause collisions,
 * so we may have to move them.
 * The x is always the label's west plus the passed in adjustment.
 */

static void
desired_loc(lab_p, staffno, adj)

struct LABELINFO * lab_p;	/* List to process */
int staffno;			/* Labels are associated with this staff */
double adj;			/* To adjust for margin and all label widths */

{
	double factor;		/* staffscale adjustment factor */


	for (  ; lab_p != 0; lab_p = lab_p->next) {
		/* Have to adjust by staffscale.
		 * We can't change the label in the SSV itself because that
		 * would cause problems, so make a copy and adjust that,
		 * then free it when we are done with it.
		 * Have to get size out of SSV, because Staffscale won't be
		 * up to date. */
		if (lab_p->is_staff_label == YES) {
			factor = svpath(staffno, STAFFSCALE)->staffscale;
		}
		else {
			factor = grpscale(lab_p);
		}
		MALLOCA(char, lab_p->printed_label,
						strlen(lab_p->label) + 1);
		memcpy(lab_p->printed_label, lab_p->label,
						strlen(lab_p->label) + 1);
		resize_string(lab_p->printed_label, factor, (char *) 0, -1);

		lab_p->x = lab_p->west + adj;

		/* Staff labels should be centered
		 * on the staff. Group labels should be
		 * centered on their visible staffs */
		if (lab_p->is_staff_label == YES) {
			lab_p->y = Staffs_y[staffno];
		}
		else {
			lab_p->y = (Staffs_y[lab_p->top] +
				Staffs_y[lab_p->bot]) / 2.0;
		}
		/* Adjust y to be the baseline */
		lab_p->y += (strheight(lab_p->printed_label) / 2.0)
				- strascent(lab_p->printed_label);
	}
}


/* Calculate and return the effective staffscale for group labels.
 * We use the largest staffscale of any visible staff in the group.
 * It's not entirely clear what is the "right"  factor to use in that case,
 * but it is rare for grouped staffs to have different staffscale values,
 * and when they do, the "small" ones are probably lesser parts,
 * so using the largest value seems reasonable.
 */

static double
grpscale(lab_p)

struct LABELINFO *lab_p;	/* return how much to scale this label */

{
	int s;
	double factor;

	factor = Score.staffscale;
	for (s = lab_p->top; s <= lab_p->bot; s++) {
		if (svpath(s, VISIBLE)->visible == NO) {
			continue;
		}
		if (svpath(s, STAFFSCALE)->staffscale > factor) {
			factor = svpath(s, STAFFSCALE)->staffscale;
		}
	}
	return(factor);
}


/* Find any labels that collide, and move one or both to not collide. */

static void
fix_collisions()

{
	int li;				/* list index */

	for (li = 0; li < Numvis; li++) {
		uncollide(Labellist[li].label_p);
		uncollide(Labellist[li].btwnlabel_p);
	}
}

/* Check for possible collisions between labels, and if found, adjust to
 * not collide. We only check between the labels on a single list,
 * so this will not catch or correct cases of labels for one staff colliding
 * with those for another staff, or those between 2 staffs colliding with
 * those for the staff above or below.
 * We also only stack upwards from inner to outer labels,
 * and always leave staff labels fixed, so there could be cases 
 * where a human could see a better way to lay things out.
 * However, any non-optimal cases should only arise
 * if there are very tall labels, and/or deep nesting of labels, and/or
 * wildly disparate distances between scores. Those cases should be rare,
 * and are likely to be nearly hopeless anyway, and too confusing for the
 * musician to read even if we could place things more perfectly.
 */

static void
uncollide(lab_p)

struct LABELINFO *lab_p;	/* list of labels to process */

{
	double this_south;	/* south boundary of current label */
	double next_north;	/* north boundary of label below current */
	double overlap;		/* how much curr overlaps with label below */

	if (lab_p == 0 || lab_p->next == 0) {
		/* End recursion. Either no list at all, of we are at the
		 * innermost label, which will be left
		 * at its desired locations. If outer labels collide,
		 * they will be fixed in the caller. */
		return;
	}

	/* recurse */
	uncollide(lab_p->next);

	/* See if this collides with the one below */
	this_south = lab_p->y - strdescent(lab_p->printed_label);
	next_north = lab_p->next->y + strascent(lab_p->next->printed_label)
								+ STDPAD;
	overlap = next_north - this_south;

	if (overlap > 0.0) {
		/* We never move staff labels. But if both are group labels,
		 * we might be able to move both. We can do this
		 * if the next is the innermost label. In theory,
		 * we could also split the overlap if there is any leeway
		 * below any of the other group labels, by moving all of
		 * them down as far as possible. But that could only be helpful
		 * if there are at least 3 levels of nesting, and if the
		 * distance between the geometric center of the group is
		 * wildly different than its logical center, both of which
		 * are very rare, so it doesn't seem worth trying to be
		 * that clever and risk breaking something.
		 */
		if (lab_p->next->is_staff_label == NO) {
			if (lab_p->next->next == 0) {
				lab_p->y += 0.5 * overlap;
				lab_p->next->y -= 0.5 * overlap;
				return;
			}
		}

		/* We'll have to do the entire adjustment to this group */
		lab_p->y += overlap;
	}
}


/* Go through given list of labels and print them, at the position
 * specified in the lab_p struct. If a temporary copy of the label had been
 * made to resize for staffscale, free it. */

static void
pr_lablist(lab_p)

struct LABELINFO *lab_p;

{
	for (   ; lab_p != 0; lab_p = lab_p->next) {
		pr_string(lab_p->x, lab_p->y, lab_p->printed_label,
					J_CENTER, (char *) 0, -1);
		if (lab_p->printed_label != lab_p->label) {
			FREE(lab_p->printed_label);
		}
	}
}


/* Given one MAINLL pointing to a FEED, find the previous one.
 * Many functions in this file need the previous feed. At abshorz time,
 * there may not be an actual FEED yet, it might just be proposed,
 * so functions at that time need to provide that proposed FEED place.
 * Once all the FEEDs are determined, we can use this function to
 * find the previous one.
 */

static struct MAINLL *
find_prev_feed_mll_p(mll_p)

struct MAINLL *mll_p;

{
	for (mll_p = mll_p->prev; mll_p != 0; mll_p = mll_p->prev) {
		if (IS_CLEFSIG_FEED(mll_p)) {
			break;
		}
	}
	return(mll_p);
}


/* Determine which label to use for a given staff.
 * Goes backwards from mll_p, finding if label has been changed more recently
 * than the previous feed. If so, use that label, else use label2.
 */

static char *
label4staff(mll_p, s, prev_feed_mll_p)

struct MAINLL *mll_p;	/* should point to an actual or proposed FEED location */
int s;
struct MAINLL *prev_feed_mll_p;	/* should point to an actual or proposed FEED location */

{
	for (mll_p = mll_p->prev; mll_p != 0; mll_p = mll_p->prev) {
		if (mll_p == prev_feed_mll_p) {
			break;
		}
		if (mll_p->str == S_SSV) {
			struct SSV *ssv_p = mll_p->u.ssv_p;

			/* If user changed label for this staff in staff
			 * context more recently that the previous feed,
			 * then that's the label we need. */
			if (ssv_p->context == C_STAFF && ssv_p->staffno == s
						&& ssv_p->used[LABEL] == YES) {
				return(ssv_p->label);
			}

			/* If user changed the score-wide label
			 * more recently than the previous feed,
			 * but there isn't any label set in staff context for
			 * this staff to override the score level label,
			 * then the score level label is the one we need. */
			if (ssv_p->context == C_SCORE &&
						ssv_p->used[LABEL] == YES &&
						Staff[s-1].used[LABEL] == NO) {
				return(ssv_p->label);
			}
		}
	}
	if (mll_p != 0) {
		/* Hit another feed before any relevent label changes,
		 * so we need to use label2 */
		return(svpath(s, LABEL2)->label2);
	}
	/* Ran off the top of the song. Use label */
	return(svpath(s, LABEL)->label);
}


/* Given information about a set of grouped staffs,
 * return the appropriate label to use: label or label2.
 */

static char *
label4group(mll_p, brac_p, prev_feed_mll_p)

struct MAINLL *mll_p;
struct BRAC_INFO *brac_p;
struct MAINLL *prev_feed_mll_p;

{
	for (mll_p = mll_p->prev; mll_p != 0; mll_p = mll_p->prev) {
		if (mll_p == prev_feed_mll_p) {
			/* Hasn't changed since previous feed, so label2 */
			return(brac_p->staffset_p->label2);
		}
		if (mll_p->str == S_SSV && mll_p->u.ssv_p->context == C_SCORE &&
				mll_p->u.ssv_p->used[brac_p->bractype] == YES) {
			/* found SSV where brace/bracket was changed */
			break;
		}
	}
	/* Either changed since previous feed or is the first feed in song,
	 * so use label. */
	return(brac_p->staffset_p->label);
}


/* Return width of default label if the default label is needed (for
 * indent of first score. Returns 0.0 if default label should not be used.
 */

static double
dflt_label_width(mll_p, prev_feed_mll_p)

struct MAINLL *mll_p;		/* points to FEED or proposed place
				 * where current FEED will be */
struct MAINLL *prev_feed_mll_p;	/* points to previous FEED, or proposed
				 * place where prev FEED will be */

{
	char dfltlabel[16];
	int size;		/* adjusted size of default label */


	for (mll_p = mll_p->prev; mll_p != 0; mll_p = mll_p->prev) {
		if (mll_p == prev_feed_mll_p) {
			/* not the first; so don't use default for first */
			return(0.0);
		}

		if (mll_p->str == S_SSV && mll_p->u.ssv_p->context == C_SCORE &&
					mll_p->u.ssv_p->used[LABEL] == YES) {
			/* explicit label for first, so don't use default */
			return(0.0);
		}
	}
	/* Since the default label is based on the fact there was no label
	 * specified in score context, we probably should use the staffscale
	 * of score context to adjust the default label's size
	 * to be proportional to Score.staffscale.
	 * But with really big staffscale values, the indent could look
	 * ridiculous, taking more than half the page. And in the other
	 * direction, a really small staffscale can make the indent
	 * hardly big enough to see. So we compromise, and only adjust
	 * between half and double size. User can always override by
	 * setting whatever they want if they don't like what they get. */
	size = adj_size(DFLT_SIZE, Score.staffscale, (char *) 0, -1);
	if (size < DFLT_SIZE / 2) {
		size = DFLT_SIZE / 2;
	}
	else if (size > DFLT_SIZE * 2) {
		size = DFLT_SIZE * 2;
	}
	(void) sprintf(dfltlabel, "%c%c            ", FONT_TR, size);
	return(strwidth(dfltlabel));
}


/* Adjusts the AW values in _Staff to be the left edge of staff labels.
 * Any that do not have staff labels are left as they are.
 * prep_brac() must be called before this is called.
 */

void
set_staff_x()

{
	int s;
	struct LABELINFO *lab_p;

	for (s = 0; s < Numvis; s++) {
		for (lab_p = Labellist[s].label_p; lab_p != 0; lab_p = lab_p->next) {
			if (lab_p->is_staff_label == YES) {
				_Staff[Labellist[s].staffno][AW] = lab_p->x;
			}
		}
	}
}


/* When place_labels() or grouplabel() add a label, 
 * they call this to save away information about the label,
 * so that when all have been placed,
 * that information can be used to justify the labels.
 */

static void
save_label_justify_info(level, label_p, is_group, scale)

int level;			/* nesting level */
struct LABELINFO *label_p;	/* the label to save info about */
int is_group;			/* YES or NO */
double scale;			/* staffscale or grpscale */

{
	struct LABEL_JUSTIFY_INFO *jinfo_p;

	/* We only actually need this if not the default right justification */
	if (Score.alignlabels == J_RIGHT) {
		return;
	}

	MALLOC(LABEL_JUSTIFY_INFO, jinfo_p, 1);
	jinfo_p->level = level;
	jinfo_p->label_p = label_p;
	jinfo_p->is_group = is_group;
	jinfo_p->scale = scale;
	jinfo_p->next = Label_justify_info_list;
	Label_justify_info_list = jinfo_p;
}


/* This function frees up the current list of information that is saved
 * regarding labels, to be able to justify them.
 */

static void
free_label_justify_info_list()
{
	struct LABEL_JUSTIFY_INFO *jinfo_p;
	struct LABEL_JUSTIFY_INFO *tmp_jinfo_p;

	for (jinfo_p = Label_justify_info_list; jinfo_p != 0;
						jinfo_p = tmp_jinfo_p) {
		tmp_jinfo_p = jinfo_p->next;
		FREE(jinfo_p);
	}
	Label_justify_info_list = 0;
}


/* Once all the labels have been placed, this function is called to justify
 * them, if "center" or "left" is desired (default is "right").
 */

static void
justify_labels()

{
	struct LABEL_JUSTIFY_INFO *jinfo_p;
	int level;			/* nesting level */
	double width;
	double widest;
	double extra;			/* widest minus current label's width */
	int more_to_do;			/* YES or NO */


	/* If justification is center or left, go through all the group labels
	 * for each nesting level, adjusting the west appropriately.
	 */
	if (Score.alignlabels != J_RIGHT) {
		more_to_do = YES;
		for (level = 0; more_to_do == YES; level++) {
			/* Assume for now that this is the last time
			 * we'll need to go through this loop. */
			more_to_do = NO;

			/* Find widest at this nesting level */
			widest = 0.0;
			for (jinfo_p = Label_justify_info_list; jinfo_p != 0;
						jinfo_p = jinfo_p->next) {
				if (jinfo_p->level != level) {
					continue;
				}
				width = jinfo_p->label_p->width * jinfo_p->scale;
				if (width > widest) {
					widest = width;
				}
				more_to_do = YES;
			}

			if (more_to_do == NO) {
				break;
			}

			/* Adjust all labels at this nesting level */
			for (jinfo_p = Label_justify_info_list; jinfo_p != 0;
						jinfo_p = jinfo_p->next) {
				if (jinfo_p->level != level) {
					continue;
				}
				extra = widest - (jinfo_p->label_p->width * jinfo_p->scale);
				if (Score.alignlabels == J_LEFT) {
					jinfo_p->label_p->west -= extra;
				}
				if (Score.alignlabels == J_CENTER) {
					jinfo_p->label_p->west -= extra / 2.0;
				}
			}
		}
	}
	/* We won't need the information list anymore. */
	free_label_justify_info_list();
}
