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
/*
 * Name:	absvert.c
 *
 * Description:	This file contains functions for setting all absolute
 *		vertical coordinates.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/*
 * Define the maximum number of scores that could ever fit on a page, when all
 * staffs and scores are packed as tightly as possible.  The 8 * STEPSIZE is
 * the height of the five lines of a staff, and the other factor in the
 * denominator is the minimum distance between staffs or scores, whichever is
 * smaller.  If a staff has less than 5 lines, it is still given as much space
 * as a 5 line staff, so that's why we can use 8 * STEPSIZE here as the
 * smallest possible staff size.
 */
#define CURMAXSCORES	( (int)(PGHEIGHT /				\
	(MINSTFSCALE * STEPSIZE * (8 + MIN(MINMINSTSEP, MINMINSCSEP)))) + 1 )

#define	FUDGE	0.001	/* fudge factor for round off error */

/* determine what clef, if any, will be printed on a staff */
#define CLEF2PRINT(staffno)	\
	(svpath(staffno, STAFFLINES)->printclef == SS_NORMAL ?	\
			svpath(staffno, CLEF)->clef : NOCLEF)

/* define amount of horz and vert padding between at-end grids */
#define HPADGRID	(2.0 * STEPSIZE)
#define VPADGRID	(2.0 * STEPSIZE)

/* maximum length of a chord name that we care about for sorting purposes */
#define MAXCHNAME	(100)

static void relscore P((struct MAINLL *mllfeed_p, int measnum));
static void relstaff P((struct MAINLL *feed_p, int s1, int s2, double botoff,
		double betweendist));
static void posscores P((void));
static void abspage P((struct MAINLL *page_p, float cursep[], float maxsep[],
		short forcesep[], float curpad[], float maxpad[], int totscores,
		double remheight, double y_start));
static void absstaff P((struct FEED *feed_p, struct STAFF *staff_p));
static double grids_atend P((double vertavail, int physpage,
		struct FEED *mfeed_p, struct FEED *gfeed_p));
static int compgrids P((const void *g1_p_p, const void *g2_p_p));
static void proc_css P((void));
static void one_css P((struct STAFF *ts_p, struct STAFF *os_p,
		struct GRPSYL *tg_p, RATIONAL time));
static void horzavoid P((void));
static void avoidone P((struct MAINLL *mainll_p, struct GRPSYL *cssg_p,
		RATIONAL time));
static int acccollide P((struct GRPSYL *gs1_p, struct GRPSYL *gs2_p));
static void set_csb_stems P((void));
static void onecsb P((struct GRPSYL *gs1_p, struct GRPSYL *gs2_p));
static int calcline P((struct GRPSYL *start1_p, struct GRPSYL *end1_p,
		struct GRPSYL *start2_p, struct GRPSYL *end2_p,
		struct GRPSYL *first_p, struct GRPSYL *last_p,
		int topdir, int botdir,
		float *b0_p, float *b1_p));
static void samedir P((struct GRPSYL *first_p, struct GRPSYL *last_p,
		struct GRPSYL *start1_p, struct GRPSYL *start2_p,
		struct GRPSYL *end1_p, float *b0_p, float *b1_p,
		double deflen, int one_end_forced, int slope_forced,
		double forced_slope));
static void oppodir P((struct GRPSYL *first_p, struct GRPSYL *last_p,
		struct GRPSYL *start1_p, struct GRPSYL *start2_p,
		float *b0_p, float *b1_p, double deflen, int one_end_forced,
		int slope_forced, double forced_slope));
static struct GRPSYL *nextcsb P((struct GRPSYL *gs_p));
static struct GRPSYL *nxtbmnote P((struct GRPSYL *gs_p, struct GRPSYL *first_p,
		struct GRPSYL *endnext_p));
static int measnumdelta P((struct MAINLL *mll_p));

/*
 * Name:        absvert()
 *
 * Abstract:    Set all absolute vertical coordinates.
 *
 * Returns:     void
 *
 * Description: This function sets all absolute vertical coordinates.  First it
 *		calls relscore() for each score, to position the staffs in that
 *		score relative to the score.  Then it calls posscores() to
 *		decide how many scores to put on each page, and set all the
 *		absolute coordinates.  Finally it completes the work for
 *		cross staff stemming (CSS) and cross staff beaming (CSB).
 */

void
absvert()

{
	struct MAINLL *mainll_p;	/* point along main linked list */
	int measnum;			/* measure number */


	debug(16, "absvert");
	/*
	 * Find each section of the main linked list, delimited by FEEDs.  For
	 * each such section, call relscore() to fix the score internally
	 * (relative to itself, all staffs and between stuff).  Keep SSVs
	 * up to date so that we always know what the user requested
	 * separations are.
	 */
	initstructs();			/* clean out old SSV info */

	measnum = has_pickup() ? 0 : 1;
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		switch (mainll_p->str) {
		case S_SSV:
			asgnssv(mainll_p->u.ssv_p);
			break;

		case S_BAR:
			measnum += measnumdelta(mainll_p);
			break;

		case S_FEED:
			relscore(mainll_p, measnum);
			break;
		}
	}

	/*
	 * Position the scores on the pages, setting all absolute vertical
	 * coordinates.
	 */
	posscores();

	/*
	 * Process groups that have cross staff stemming, if there were any.
	 */
	if (CSSused == YES) {
		proc_css();
	}

	/*
	 * Set stem lengths for groups involved in cross staff beaming, if
	 * there were any.
	 */
	if (CSBused == YES) {
		set_csb_stems();
	}
}

/*
 * Name:        relscore()
 *
 * Abstract:    Set certain relative coords to be relative to score.
 *
 * Returns:     void
 *
 * Description: This function loops through the part of the main linked list
 *		for this score.  It adjusts the relative vertical coords of
 *		STAFFs, and also of GRPSYLs (syllables) and STUFFs of the
 *		things that are "between" staffs.  In the end, the STAFFs will
 *		be relative to the score (FEED), and the between things will
 *		be relative to the staff above them.  Yes, I suppose this
 *		belongs in relvert.c, but relvert.c has enough work to do.
 */

static void
relscore(mllfeed_p, measnum)

struct MAINLL *mllfeed_p;	/* FEED at start of this score */
int measnum;			/* measure number */

{
	struct MAINLL *mainll_p;/* point along main linked list */
	struct STAFF *cstaff_p;	/* point at current staff */
	struct STAFF *pstaff_p;	/* point at previous staff */
	struct FEED *feed_p;	/* point at FEED structure itself */
	float cstaffoffset;	/* current staff offset from score */
	float staffdist;	/* dist between prev & cur staff inner lines*/
	float halfnonbetween;	/* (staffdist - heightbetween) / 2 */
	float betweendist;	/* from prev staff center line to base line */
	float prevhalf;		/* half the height of previous staff */
	float curhalf;		/* half the height of current staff */
	float limit;		/* smallest dist allowed between inner lines */
	float needed;		/* dist between inner lines to avoid collis */
	int prevclef;		/* clef on the previous staff */
	float prevscale;	/* staffscale of the previous staff */
	float spad;		/* staffpad (inches) below previous staff */
	float clefroom;		/* room for clefs and/or measure numbers */
	static int first = YES;	/* is this the first score in the song? */


	debug(32, "relscore file=%s line=%d", mllfeed_p->inputfile,
			mllfeed_p->inputlineno);
	feed_p = mllfeed_p->u.feed_p;

	/*
	 * If this score is actually a block, all we have to do is set the
	 * relative vertical coords of the FEED.  We set RY to be the center.
	 */
	if (mllfeed_p->next != 0 && mllfeed_p->next->str == S_BLOCKHEAD) {
		feed_p->c[RN] = mllfeed_p->next->u.blockhead_p->height / 2.0;
		feed_p->c[RY] = 0;		/* RY is always 0 */
		feed_p->c[RS] = -feed_p->c[RN];
		feed_p->lastdist = 0.0;
		return;
	}

	/*
	 * Find the first STAFF in this score (will be in first measure).
	 */
	for (mainll_p = mllfeed_p->next; mainll_p != 0 &&
			mainll_p->str != S_FEED && mainll_p->str != S_STAFF;
			mainll_p = mainll_p->next)
		;
	if (mainll_p == 0 || mainll_p->str != S_STAFF)
		return;	/* ignore items when there's a feed at end of song */

	/* init variables for main loop */
	cstaffoffset = 0;		/* top staff Y == score Y */
	pstaff_p = 0;			/* there is no previous staff */
	prevclef = NOCLEF;
	prevscale = 1.0;
	spad = 0.0;		/* keep lint happy; will be set before used */

	/*
	 * Loop through all STAFF structures in the first measure of this
	 * score.  Skip invisible ones.  cstaff_p always points at the staff
	 * we are working on, and pstaff_p always points to the previous
	 * visible staff (so is 0 while we are working on the first visible
	 * staff of the score).  For each visible staff except the first, we
	 * figure out how far down it should be from the one above it, and
	 * set its relative vertical coords relative to the score.  Also, we
	 * figure out where to put the things that are "between" this staff
	 * and the one above, and set them relative to the above staff.
	 */
	for ( ; mainll_p->str == S_STAFF; mainll_p = mainll_p->next) {

		cstaff_p = mainll_p->u.staff_p;

		/*
		 * If this staff is invisible, ignore it completely.
		 */
		if (cstaff_p->visible == NO)
			continue;

		/*
		 * If it's the first visible staff, there are no coords to set,
		 * since its offset is 0 and the "between" objects below it
		 * will be handled by the next loop.  Also set first and last
		 * visible staff numbers in the FEED in this loop, and the
		 * relative vertical coords of the score.
		 */
		if (pstaff_p == 0) {
			/* set first visible staff number */
			feed_p->firstvis = cstaff_p->staffno;

			/* feed's RN is same as first visible staff's RN */
			feed_p->c[RN] = cstaff_p->c[RN];
			feed_p->c[RY] = 0;		/* RY is always 0 */

			/* these next 3 will be changed later if more staffs */
			feed_p->c[RS] = cstaff_p->c[RS];
			feed_p->lastvis = cstaff_p->staffno;
			feed_p->lastdist = cstaff_p->c[RY] - cstaff_p->c[RS] -
				staffvertspace(cstaff_p->staffno) / 2.0;

			pstaff_p = cstaff_p;	/* previous visible staff */
			prevclef = CLEF2PRINT(pstaff_p->staffno);
			prevscale = svpath(pstaff_p->staffno, STAFFSCALE)->
					staffscale;
			spad = svpath(pstaff_p->staffno, STAFFPAD)->staffpad
				* STEPSIZE * prevscale;
			continue;		/* no coords to set */
		}

		/* set half the height of the previous and current staffs */
		prevhalf = staffvertspace(pstaff_p->staffno) / 2.0;
		curhalf = staffvertspace(cstaff_p->staffno) / 2.0;

		/*
		 * The space needed between the bottom line of the previous
		 * staff and the top line of the current staff to avoid
		 * collisions is how far up from the current staff things
		 * stick, plus how far down from the previous staff things
		 * stick, plus the height of anything "between" the two.
		 * To this we add spad for extra padding (overlap if negative).
		 */
		needed = (cstaff_p->c[RN] - curhalf) +
			 ((pstaff_p->c[RY] - pstaff_p->c[RS]) - prevhalf) +
			 pstaff_p->heightbetween + spad;
		/*
		 * Set the distance between those two lines to be what the
		 * user requested, or what was calculated above as "needed",
		 * whichever is greater.  Set halfnonbetween to be half of
		 * this result, minus half the height of the "between" items.
		 */
		/* never closer than this */
		limit = svpath(pstaff_p->staffno,MINSTSEP)->minstsep * STEPSIZE;
		clefroom = clefspace(prevclef, prevscale,
			CLEF2PRINT(cstaff_p->staffno),
			svpath(cstaff_p->staffno, STAFFSCALE)->staffscale,
			(Score.measnum == MN_SCORE &&
			has_ending(cstaff_p->staffno)
			&& first == NO) ? measnum : 0);
		limit = MAX(limit, clefroom);

		staffdist = MAX(limit, needed);	/* between prev & current */

		/*
		 * Find half the room between the inner staff lines that is not
		 * going to be used by the "between" items.  But pretend that
		 * the "between" items are bigger by "spad" than they really
		 * are, so that half of staffpad will go on each side of them.
		 */
		halfnonbetween = (staffdist - (pstaff_p->heightbetween + spad))
				/ 2.0;

		/* set cstaffoffset for relative to score */
		cstaffoffset -= (prevhalf + staffdist + curhalf);

		/*
		 * The "between" items are currently placed relative to a base
		 * line that they were piled onto.  We would like to center
		 * them between the staffs, but if one staff sticks out more
		 * than the other, it may not be possible.  Center as close as
		 * possible.  betweendist is how far the base line is from the
		 * center line of the previous staff.
		 */
		if ((pstaff_p->c[RY] - pstaff_p->c[RS]) - prevhalf >
					halfnonbetween) {
			/*
			 * The top staff sticks down far enough that we have
			 * to put the "between" items below center.  Jam them
			 * against the top staff.
			 */
			betweendist = (pstaff_p->c[RY] - pstaff_p->c[RS]) +
						pstaff_p->heightbetween + spad;
		} else if (cstaff_p->c[RN] - curhalf > halfnonbetween) {
			/*
			 * The bottom staff sticks up far enough that we have
			 * to put the "between" items above center.  Jam them
			 * against the bottom staff.
			 */
			betweendist = (prevhalf + staffdist + curhalf) -
						cstaff_p->c[RN];
		} else {
			/*
			 * There is room to center the between items.
			 */
			betweendist = prevhalf + staffdist - halfnonbetween;
		}

		/* change baseline of padding to actual baseline */
		betweendist -= spad / 2.0;

		/*
		 * For all STAFF structures of these staff numbers in this
		 * score, change relative coords as described below.
		 */
		relstaff(mllfeed_p, pstaff_p->staffno, cstaff_p->staffno,
				cstaffoffset, betweendist);

		/* last loop iteration leaves right value in these variables */
		feed_p->lastvis = cstaff_p->staffno;
		feed_p->c[RS] = cstaff_p->c[RS];
		feed_p->lastdist = cstaff_p->c[RY] - cstaff_p->c[RS] - curhalf;

		pstaff_p = cstaff_p;
		prevclef = CLEF2PRINT(pstaff_p->staffno);
		prevscale = svpath(pstaff_p->staffno, STAFFSCALE)->staffscale;
		spad = svpath(pstaff_p->staffno, STAFFPAD)->staffpad
				* STEPSIZE * prevscale;
	}

	first = NO;	/* next score will not be the first */
}

/*
 * Name:        relstaff()
 *
 * Abstract:    Set certain relative coords to be relative to score.
 *
 * Returns:     void
 *
 * Description:	This function is given two staff structures for consecutive
 *		visible staffs.  For all STAFF structures of these staff
 *		numbers in this score, set the bottom staff's coords relative
 *		to the score, and set the "between" items' coords (for what's
 *		between top and bottom staff) relative to the top staff.
 */

static void
relstaff(feed_p, s1, s2, botoff, betweendist)

struct MAINLL *feed_p;		/* pointer to FEED for this score */
int s1;				/* number of top staff */
int s2;				/* number of bottom staff */
double botoff;			/* center line of bottom, relative to score */
double betweendist;		/* center line of top to base line of between*/

{
	struct MAINLL *mainll_p;/* point along main linked list */
	struct STAFF *staff_p;	/* pointer to a staff */
	struct GRPSYL *syl_p;	/* pointer to a syllable */
	struct STUFF *stuff_p;	/* pointer to stuff to draw */
	int n;			/* loop variable */


	debug(32, "relstaff file=%s line=%d s1=%d s2=%d botoff=%f betweendist=%f",
			feed_p->inputfile, feed_p->inputlineno, s1, s2,
			(float)botoff, (float)betweendist);
	/*
	 * Loop through the section of the main linked list for this score,
	 * looking for every STAFF for one of the two given staffs.
	 */
	for (mainll_p = feed_p->next; mainll_p != 0 && mainll_p->str != S_FEED;
				mainll_p = mainll_p->next) {

		if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno == s1) {

			staff_p = mainll_p->u.staff_p;

			/*
			 * Subtract betweendist from all relative coords of
			 * "between" items hanging off this staff, to make them
			 * relative to this staff instead of the base line.
			 */
			for (n = 0; n < staff_p->nsyllists; n++) {
				if (staff_p->sylplace[n] == PL_BETWEEN) {
					for (syl_p = staff_p->syls_p[n];
							syl_p != 0;
							syl_p = syl_p->next) {
						syl_p->c[RN] -= betweendist;
						syl_p->c[RY] -= betweendist;
						syl_p->c[RS] -= betweendist;
					}
				}
			}
			for (stuff_p = staff_p->stuff_p; stuff_p != 0;
					stuff_p = stuff_p->next) {
				if (stuff_p->place == PL_BETWEEN) {
					stuff_p->c[RN] -= betweendist;
					stuff_p->c[RY] -= betweendist;
					stuff_p->c[RS] -= betweendist;
				}
			}
		}

		if (mainll_p->str == S_STAFF &&
				mainll_p->u.staff_p->staffno == s2) {

			staff_p = mainll_p->u.staff_p;

			/*
			 * Make this staff relative to the score instead of
			 * relative to its own center line.
			 */
			staff_p->c[RN] += botoff;
			staff_p->c[RY] = botoff;
			staff_p->c[RS] += botoff;
		}
	}
}

/*
 * Name:        posscores()
 *
 * Abstract:    Place which scores on which pages, and set all vertical coords.
 *
 * Returns:     void
 *
 * Description: This function decides how many scores are going to fit on each
 *		page, based on how big they are and how much minimum space the
 *		user wants put between them, subject to the limit stated in the
 *		maxscores parameter.  It calls abspage() for each page to do
 *		final positioning and coordinate setting.
 */

static void
posscores()

{
	struct MAINLL *mainll_p;/* point along main LL */
	struct TIMEDSSV *tssv_p;/* point along timed SSV lists */
	struct MAINLL *page_p;	/* point at first FEED of a page */
	struct MAINLL *ppage_p;	/* point at first FEED of previous page */
	struct MAINLL *gridpage_p; /* point at FEED for grids-at-end */
	struct MAINLL *origpage_p; /* remember original page_p */
	struct MAINLL *cfeedmll_p; /* remember the current FEED */
	struct MAINLL *spfeedmll_p;/* FEED at start of current "samepage" zone*/
	struct FEED *cfeed_p;	/* point at current scorefeed */
	struct FEED *pfeed_p;	/* point at previous scorefeed */
	float availheight;	/* available height on page (middle window) */
	float remheight;	/* remaining height on page */
	float spremheight;	/* remaining height when samepage started */
	float prevremheight;	/* remaining after previous score */
	float y_start;		/* where y begins (at top of _win) */
	float limit;		/* smallest distance allowed between scores */
	int prevclef;		/* clef on last visible staff of prev score */
	float prevscale;	/* staffscale of last vis staff of prev score*/
	float clefroom;		/* room for clefs and/or measure numbers */
	float excess;		/* extra room needed for top score */
	float abovetopline;	/* dist from top line of score to top of score*/
	float ink;		/* distance ink extends between inner lines */
	float padding;		/* space between farthest extents */
	float scoreheight;	/* height of current score */
	float topheight, botheight;	/* height of a "top" or "bot" block */
	int firstpage;		/* 1st music page (don't count hdr-only page)?*/
	int physpage;		/* physical page number: counts the pages
				 * generated (before any -o is applied),
				 * starting from 1 */
	int pageside;		/* PGSIDE_* */
	int totscores;		/* number of scores on a page */
	int maxscores;		/* maximum number of scores allowed */
	int spscores;		/* number of scores in current samepage zone */
	int measnum;		/* measure number */

	/* the following are all in inches, unlike scorepad/scoresep parms */
	float curminpad;	/* current minscpad */
	float curmaxpad;	/* current maxscpad */
	float *curpad;		/* malloc: pad above each score */
	float *maxpad;		/* malloc: maxscpad above each score */
	float curminsep;	/* current minscsep */
	float curmaxsep;	/* current maxscsep */
	float *cursep;		/* malloc: sep above each score */
	float *maxsep;		/* malloc: maxscsep above each score */
	short *forcesep;	/* YES or NO: force cursep to be used */

	int is_block;		/* is there a block after this FEED? */
	int score_fits;		/* does the current score fit?  YES or NO */
	struct BLOCKHEAD *head_p;	/* point at currently relevant header */
	struct BLOCKHEAD *foot_p;	/* point at currently relevant footer */
	struct BLOCKHEAD *rememlefttop2_p, *rememrighttop2_p;
	struct BLOCKHEAD *rememleftbot2_p, *rememrightbot2_p;


	debug(16, "posscores");
	/*
	 * In each of these arrays, array[idx] refers to distance below score
	 * number idx on a page, numbering the scores from 1 to N.  For sep, 
	 * only indices 1 through N-1 are used.  For pad, indices 0 through N
	 * are used, where 0 means above the first score and N below the last.
	 * The "sep" arrays are for distances between the outermost staff lines
	 * of neighboring scores.  The "pad" arrays are for distances between
	 * the outermost thing sticking out of those scores.  The "above"
	 * arrays are for distance currently allocated.  The "max" arrays are
	 * for the max limits we impose (when we can).
	 * If forcesep is YES, we force cursep to be used, regardless of pad
	 * values, and not expanding it in abspage().
	 */
	MALLOCA(float, cursep, CURMAXSCORES);
	MALLOCA(float, curpad, CURMAXSCORES + 1);
	MALLOCA(float, maxsep, CURMAXSCORES);
	MALLOCA(float, maxpad, CURMAXSCORES + 1);
	CALLOCA(short, forcesep, CURMAXSCORES);		/* set all to NO */

	initstructs();		/* init SSVs */

	/* the following need to be initialized for the coming loop */
	curminsep = Score.minscsep * STEPSIZE;
	curminpad = Score.minscpad * STEPSIZE;
	curmaxsep = Score.maxscsep * STEPSIZE;
	curmaxpad = Score.maxscpad * STEPSIZE;
	maxscores = Score.maxscores;
	pfeed_p = 0;
	firstpage = YES;
	physpage = 1;
	mainll_p = Mainllhc_p;
	rememlefttop2_p = rememrighttop2_p = 0;
	rememleftbot2_p = rememrightbot2_p = 0;
	measnum = has_pickup() ? 0 : 1;

	/* the following don't really need to be initialized; we're doing it */
	/* just to prevent useless 'used before set' warnings */
	page_p = 0;
	ppage_p = 0;
	spfeedmll_p = 0;
	cfeedmll_p = 0;
	spremheight = 0;
	prevremheight = 0;
	spscores = 0;
	remheight = 0;
	y_start = 0;
	totscores = 0;
	prevclef = NOCLEF;
	prevscale = 1.0;
	botheight = 0.0;
	foot_p = 0;

	/*
	 * Loop through the main linked list, looking at each feed.  Assuming
	 * the scores are packed as tightly as allowed, see how many will fit
	 * on each page.  Whenever a page fills up, call abspage() to
	 * distribute the extra white space as well as possible and set all
	 * the absolute vertical coords for that page.  At the end, call it
	 * again for the last page.
	 */
	while (mainll_p != 0) {
		switch (mainll_p->str) {
		case S_FEED:
			break;	/* go handle this score */
		case S_SSV:
			/* apply, and reset vars in case some changed */
			asgnssv(mainll_p->u.ssv_p);
			curminsep = Score.minscsep * STEPSIZE;
			curmaxsep = Score.maxscsep * STEPSIZE;
			curminpad = Score.minscpad * STEPSIZE;
			curmaxpad = Score.maxscpad * STEPSIZE;
			maxscores = Score.maxscores;
			mainll_p = mainll_p->next;
			continue;
		case S_BAR:
			/* apply timed SSVs; they won't affect the above
			 * variables, but they could affect clef, which we
			 * will need later */
			for (tssv_p = mainll_p->u.bar_p->timedssv_p;
					tssv_p != 0; tssv_p = tssv_p->next) {
				asgnssv(&tssv_p->ssv);
			}
			measnum += measnumdelta(mainll_p);
			/*
			 * If this bar is to be in a samepage zone, and we have
			 * not already set spfeedmll_p, this will be the first
			 * score in the zone, so remember that the zone starts
			 * with the current score.
			 */
			if (mainll_p->u.bar_p->samepage == YES &&
					spfeedmll_p == 0) {
				spfeedmll_p = cfeedmll_p;
				spscores = 1;
				spremheight = prevremheight;
			} else if (mainll_p->u.bar_p->samepage == NO) {
				spfeedmll_p = 0;
			}
			mainll_p = mainll_p->next;
			continue;
		case S_BLOCKHEAD:
			/* same logic as for BAR */
			if (mainll_p->u.blockhead_p->samepage == YES &&
					spfeedmll_p == 0) {
				spfeedmll_p = cfeedmll_p;
				spscores = 1;
				spremheight = prevremheight;
			} else if (mainll_p->u.blockhead_p->samepage == NO) {
				spfeedmll_p = 0;
			}
			mainll_p = mainll_p->next;
			continue;
		default:
			mainll_p = mainll_p->next;
			continue;
		}

		/* if there is nothing after this FEED, break out */
		if (mainll_p->next == 0) {
			break;
		}

		cfeedmll_p = mainll_p;	/* remember for future loops */
		cfeed_p = mainll_p->u.feed_p;	/* set convenient pointer */

		if (spfeedmll_p == 0) {
			/*
			 * We are not in a samepage zone, as far as we know yet.
			 * But following this FEED, there could be a BAR or
			 * BLOCKHEAD that says a zone should start at this FEED.
			 * So we need to save the SSV states in case that
			 * happens and we later need to move the zone to the
			 * next page.  This could also be the FEED at the end
			 * of a zone, but in that case, we don't need the old
			 * saved version anymore, and we need to save here in
			 * case a new zone will be starting.
			 */
			spscores = 0;
			savessvstate();
		} else {
			spscores++;	/* found other FEED in samepage zone */
		}

		/*
		 * If firstpage is set, normally there would be no pagefeed,
		 * because the first FEED on that page is marked as a pagefeed
		 * only if the user requested it.  If they did, that means
		 * there was a title page with no music on it; and in that
		 * case, we are on physical page 2 now instead of 1.
		 */
		if (firstpage == YES && cfeed_p->pagefeed == YES) {
			physpage = 2;
		}

		pageside = page2side(physpage);

		/* see if there is a block after this feed */
		is_block = mainll_p->next != 0 &&
				mainll_p->next->str == S_BLOCKHEAD;

		scoreheight = cfeed_p->c[RN] - cfeed_p->c[RS];

		if (pfeed_p == 0) {
			/*
			 * We are at the top of a page.
			 */

			/*
			 * Point at the header and footer that apply.  Note
			 * that if the header or footer is unused, its height
			 * will be 0.
			 */
			if (physpage == 1) {
				if (pageside == PGSIDE_LEFT) {
					head_p = &Leftheader;
					foot_p = &Leftfooter;
				} else {
					head_p = &Rightheader;
					foot_p = &Rightfooter;
				}
			} else {
				if (pageside == PGSIDE_LEFT) {
					head_p = &Leftheader2;
					foot_p = &Leftfooter2;
				} else {
					head_p = &Rightheader2;
					foot_p = &Rightfooter2;
				}
			}

			/* if not the first page, set pagefeed */
			if (firstpage == NO) {
				cfeed_p->pagefeed = YES;
			}

			/*
			 * We do the following top/bot logic for both left and
			 * right, since, for one thing, when a user sets a
			 * non-left/right thing, it can affect both types of
			 * pages.
			 */

			/* "viewpath" to set left and right if not set */
			if (cfeed_p->lefttop_p == 0) {
				cfeed_p->lefttop_p = cfeed_p->top_p;
			}
			if (cfeed_p->righttop_p == 0) {
				cfeed_p->righttop_p = cfeed_p->top_p;
			}
			if (cfeed_p->leftbot_p == 0) {
				cfeed_p->leftbot_p = cfeed_p->bot_p;
			}
			if (cfeed_p->rightbot_p == 0) {
				cfeed_p->rightbot_p = cfeed_p->bot_p;
			}
			if (cfeed_p->lefttop2_p == 0) {
				cfeed_p->lefttop2_p = cfeed_p->top2_p;
			}
			if (cfeed_p->righttop2_p == 0) {
				cfeed_p->righttop2_p = cfeed_p->top2_p;
			}
			if (cfeed_p->leftbot2_p == 0) {
				cfeed_p->leftbot2_p = cfeed_p->bot2_p;
			}
			if (cfeed_p->rightbot2_p == 0) {
				cfeed_p->rightbot2_p = cfeed_p->bot2_p;
			}

			/* remember most recent settings of the "2" ones */
			if (cfeed_p->lefttop2_p != 0) {
				rememlefttop2_p = cfeed_p->lefttop2_p;
			}
			if (cfeed_p->righttop2_p != 0) {
				rememrighttop2_p = cfeed_p->righttop2_p;
			}
			if (cfeed_p->leftbot2_p != 0) {
				rememleftbot2_p = cfeed_p->leftbot2_p;
			}
			if (cfeed_p->rightbot2_p != 0) {
				rememrightbot2_p = cfeed_p->rightbot2_p;
			}

			/*
			 * Decide what is to be printed at the top and
			 * bottom (inside the header(2)/footer(2) if any).
			 * Do this for both the left and right versions.
			 * On the first page and at every pagefeed where top_p
			 * is set, that is to be used, so leave it alone.
			 * Otherwise use the most recent top2_p setting, so
			 * save the value into top_p.  Later in this function,
			 * and also in the print phase, top_p is used, not
			 * top2_p, with exception of grids-at-end pages.
			 * (Same goes for bot.)
			 */
			if (firstpage == NO && cfeed_p->lefttop_p == 0) {
				cfeed_p->lefttop_p  = rememlefttop2_p;
			}
			if (firstpage == NO && cfeed_p->righttop_p == 0) {
				cfeed_p->righttop_p  = rememrighttop2_p;
			}
			if (firstpage == NO && cfeed_p->leftbot_p == 0) {
				cfeed_p->leftbot_p  = rememleftbot2_p;
			}
			if (firstpage == NO && cfeed_p->rightbot_p == 0) {
				cfeed_p->rightbot_p  = rememrightbot2_p;
			}

			/*
			 * Finally, set top and bot to what we really need for
			 * this page, based on which side we are.
			 */
			if (pageside == PGSIDE_LEFT) {
				cfeed_p->top_p = cfeed_p->lefttop_p;
				cfeed_p->bot_p = cfeed_p->leftbot_p;
			} else {
				cfeed_p->top_p = cfeed_p->righttop_p;
				cfeed_p->bot_p = cfeed_p->rightbot_p;
			}

			/* set height of "top" & "bot" if they exist, else 0 */
			topheight = cfeed_p->top_p != 0 ?
					cfeed_p->top_p->height : 0.0;
			botheight = cfeed_p->bot_p != 0 ?
					cfeed_p->bot_p->height : 0.0;

			/*
			 * Remove these items' size from the space available
			 * for music, and set music's starting point.
			 */
			availheight = PGHEIGHT - EFF_TOPMARGIN - EFF_BOTMARGIN
				- head_p->height - foot_p->height
				- topheight - botheight;

			y_start = PGHEIGHT - EFF_TOPMARGIN
				- head_p->height - topheight;

			/* these vars need to be set for paqefeeds */
			cfeed_p->north_win = y_start;
			cfeed_p->south_win = y_start - availheight;

			/*
			 * If a header or top exists on this page, we need to
			 * have pad below it.  Since we're initially packing as
			 * tightly as possible, assume the minimum.  Reduce the
			 * available room by that amount.  Analogous for
			 * footer/bottom.
			 */
			if (head_p->height + topheight > 0.0) {
				availheight -= curminpad;
			}
			if (foot_p->height + botheight > 0.0) {
				availheight -= curminpad;
			}

			/* increase score's RN and scoreheight if need be */
			if (is_block) {
				/*
				 * Blocks have no clef or measure number, but
				 * clefspace() still will return a little
				 * something for padding, so add that in.
				 */
				excess = clefspace(NOCLEF, 1.0, NOCLEF, 1.0, 0);
				cfeed_p->c[RN] += excess;
				scoreheight += excess;
			} else {
				/*
				 * If clef (and measure number if that is to be
				 * printed) stick up higher than anything else,
				 * adjust the size of the score to allow for it.
				 */
				clefroom = clefspace(NOCLEF, 1.0,
					CLEF2PRINT(cfeed_p->firstvis),
					svpath(cfeed_p->firstvis, STAFFSCALE)
						->staffscale,
					(Score.measnum == MN_SCORE &&
					firstpage == NO)
					? measnum : 0);
				abovetopline = cfeed_p->c[RN] -
					staffvertspace(cfeed_p->firstvis) / 2.0;
				excess = clefroom - abovetopline;
				if (excess > 0.0) {
					cfeed_p->c[RN] += excess;
					scoreheight += excess;
				}
			}

			if (scoreheight > availheight) {
				if (Score.units == INCHES) {
					ufatal("score is too high (%.2f inches) to fit on one page (limit %.2f)",
					scoreheight * Score.scale_factor,
					availheight * Score.scale_factor);
				} else {
					ufatal("score is too high (%.2f cm) to fit on one page (limit %.2f)",
					scoreheight * Score.scale_factor *
					CMPERINCH, availheight *
					Score.scale_factor * CMPERINCH);
				}
			}

			/*
			 * Set pad above the top score.  If there is a header
			 * or top, use the values from scorepad.  If not, force
			 * both to 0, so that none will be allowed.
			 */
			if (head_p->height + topheight > 0.0) {
				curpad[0] = curminpad;
				maxpad[0] = curmaxpad;
			} else {
				curpad[0] = 0.0;
				maxpad[0] = 0.0;
			}

			remheight = availheight - scoreheight;
			totscores = 1;
			pfeed_p = cfeed_p;
			ppage_p = page_p;
			page_p = mainll_p;
			mainll_p = mainll_p->next;
			firstpage = NO;
			physpage++;
			if (is_block) {
				prevclef = NOCLEF;
				prevscale = 1.0;
			} else {
				prevclef = CLEF2PRINT(pfeed_p->lastvis);
				prevscale = svpath(pfeed_p->lastvis,
						STAFFSCALE)->staffscale;
			}

		} else {

			/*
			 * This will be the second or later score on this page,
			 * if it fits, and the user did not request a manual
			 * pagefeed.  Figure out what the minimum padding can
			 * be between this score and the previous.  "ink" is
			 * the distance things on the bottom visible staff of
			 * the previous score extend from its bottom line down,
			 * plus the distance things on the top visible staff of
			 * the current score extend from its top line up.
			 * curminpad is the minimum white space the user wants
			 * to allow between scores.
			 */
			if (is_block) {
				ink = pfeed_p->lastdist;
				clefroom = clefspace(prevclef, prevscale,
					NOCLEF, 1.0, 0);
			} else {
				ink = pfeed_p->lastdist + (cfeed_p->c[RN] -
					staffvertspace(cfeed_p->firstvis)/2.0);
				clefroom = clefspace(prevclef, prevscale,
					CLEF2PRINT(cfeed_p->firstvis),
					svpath(cfeed_p->firstvis, STAFFSCALE)
						->staffscale,
					Score.measnum == MN_SCORE ? measnum :0);
				/*
				 * clefspace() deals with clefs and measure
				 * numbers.  Add in slashes if requested, since
				 * this score and the preceding are not blocks.
				 * But if the previous thing was a block
				 * (prevclef == NOCLEF), never use slashes.
				 */
				if (Score.slashesbetween == YES &&
						prevclef != NOCLEF) {
					clefroom += Score.staffscale *
					   (SL_BET_Y_TOTAL * STEPSIZE + STDPAD);

					/* set negative as a flag to abspage();
					 * it shouldn't use the parameter
					 * because it may change by then */
					cfeed_p->sl_bet_top_offset = -1.0;
				}
			}
			limit = MAX(curminsep, clefroom);
			if (ink < limit - curminpad) {
				padding = limit - ink;
			} else {
				padding = curminpad;
			}
			score_fits = padding + scoreheight <= remheight;

			/* but if forcing a sep value, change score_fits */
			if (SEP_IS_VALID(cfeed_p->scoresep)) {
				score_fits = cfeed_p->scoresep * STEPSIZE - ink
						+ scoreheight <= remheight;
			}

			if (score_fits && totscores < maxscores &&
						cfeed_p->pagefeed == NO) {

				/* this score will go on this page */
				/* unless samepage later needs to undo it */

				prevremheight = remheight;

				if (SEP_IS_VALID(cfeed_p->scoresep)) {
					remheight -= cfeed_p->scoresep* STEPSIZE
						- ink + scoreheight;
					forcesep[totscores] = YES;
					cursep[totscores] = cfeed_p->scoresep
						* STEPSIZE;
					/* the others won't be used */
				} else {
					remheight -= padding + scoreheight;
					cursep[totscores] = ink + padding;
					maxsep[totscores] = curmaxsep;
					curpad[totscores] = padding;
					maxpad[totscores] = curmaxpad;
					forcesep[totscores] = NO;
				}
				totscores++;
				pfeed_p = cfeed_p;
				mainll_p = mainll_p->next;
				if (is_block)
					prevclef = NOCLEF;
				else
					prevclef = CLEF2PRINT(pfeed_p->lastvis);
			} else {
				/* the score must go on the following page */
				/* but if we are in a samepage zone we will */
				/* need to move the whole zone */
				if (spscores > 0) {
					if (spscores == 1) {
						pfatal("error in samepage logic");
					}
					if (spfeedmll_p == page_p) {
						ufatal("group of samepage scores does not fit on physical page %d",
							physpage - 1);
					}
					/*
					 * Restore everything to what it was
					 * before we entered the samepage zone.
					 */
					totscores -= spscores - 1;
					remheight = spremheight;
					mainll_p = spfeedmll_p;
					restoressvstate();
				}

				/*
				 * Set pad below the bottom score.  If there is
				 * a footer or bottom, use the values from
				 * scorepad.  If not, force both to 0, so that
				 * none will be allowed.
				 */
				if (foot_p->height + botheight > 0.0) {
					curpad[totscores] = curminpad;
					maxpad[totscores] = curmaxpad;
				} else {
					curpad[totscores] = 0.0;
					maxpad[totscores] = 0.0;
				}

				abspage(page_p, cursep, maxsep, forcesep,
						curpad, maxpad, totscores,
						remheight, y_start);
				pfeed_p = 0;
			}
		}
	}

	/* in case it changes, remember the original page_p */
	origpage_p = page_p;

	/* find out what is after the last FEED */
	if (page_p->next != 0 && (page_p->next->str == S_CLEFSIG ||
				  page_p->next->str == S_BLOCKHEAD)) {
		/*
		 * The last top-of-page feed has music/block(s) after it.  Let
		 * page_p continue to point at it, and for now let gridpage_p
		 * be null.
		 */
		gridpage_p = 0;
	} else {
		/*
		 * The last top-of-page feed is after all music/blocks.  Point
		 * page_p at the previous one, and use this one for gridpage_p.
		 */
		gridpage_p = page_p;
		page_p = ppage_p;
	}

	/*
	 * Before distributing the scores on the last page, if there are chord
	 * grids to be printed at the end, find whether they fit on this page
	 * (their height doesn't exceed remheight minus white).  If so, the
	 * subroutine places them at the bottom and returns their height.  If
	 * they don't fit, it returns zero and puts them on a separate page.
	 */
	if (Atend_info.grids_used > 0) {
		float gridheight;

		/*
		 * In case grids need to go on later page(s), we need to make
		 * sure there is a FEED at the end of the MLL.  Its top_p and
		 * bot_p will be used on the first grid page, and top2_p and
		 * bot2_p will be used on later pages.
		 */
		if (gridpage_p == 0) {
			/* find last thing in MLL that's not LINE/CURVE/PRHEAD*/
			for (mainll_p = Mainlltc_p;
					mainll_p->str == S_LINE ||
					mainll_p->str == S_CURVE ||
					mainll_p->str == S_PRHEAD;
					mainll_p = mainll_p->prev)
				;
			if (mainll_p->str == S_FEED) {
				/* FEED, so reuse for gridpage FEED */
				/* (it wasn't a top-of-page FEED before) */
				gridpage_p = mainll_p;
			} else {
				/* alloc new FEED to be used for grid pages */
				gridpage_p = newMAINLLstruct(S_FEED, -1);
				insertMAINLL(gridpage_p, Mainlltc_p);
			}

			/*
			 * Both the first and later grid pages should use what
			 * is currently remembered for top2 and bot2.
			 */
			gridpage_p->u.feed_p->lefttop_p   = rememlefttop2_p;
			gridpage_p->u.feed_p->lefttop2_p  = rememlefttop2_p;
			gridpage_p->u.feed_p->righttop_p  = rememrighttop2_p;
			gridpage_p->u.feed_p->righttop2_p = rememrighttop2_p;
			gridpage_p->u.feed_p->leftbot_p   = rememleftbot2_p;
			gridpage_p->u.feed_p->leftbot2_p  = rememleftbot2_p;
			gridpage_p->u.feed_p->rightbot_p  = rememrightbot2_p;
			gridpage_p->u.feed_p->rightbot2_p = rememrightbot2_p;
		} else {
			/* set pointers that are not already set */
			if (gridpage_p->u.feed_p->lefttop2_p == 0) {
				gridpage_p->u.feed_p->lefttop2_p =
					rememlefttop2_p;
			}
			if (gridpage_p->u.feed_p->righttop2_p == 0) {
				gridpage_p->u.feed_p->righttop2_p =
					rememrighttop2_p;
			}
			if (gridpage_p->u.feed_p->lefttop_p == 0) {
				gridpage_p->u.feed_p->lefttop_p =
					gridpage_p->u.feed_p->lefttop2_p;
			}
			if (gridpage_p->u.feed_p->righttop_p == 0) {
				gridpage_p->u.feed_p->righttop_p =
					gridpage_p->u.feed_p->righttop2_p;
			}
			if (gridpage_p->u.feed_p->leftbot2_p == 0) {
				gridpage_p->u.feed_p->leftbot2_p =
					rememleftbot2_p;
			}
			if (gridpage_p->u.feed_p->leftbot2_p == 0) {
				gridpage_p->u.feed_p->leftbot2_p =
					rememleftbot2_p;
			}
			if (gridpage_p->u.feed_p->leftbot_p == 0) {
				gridpage_p->u.feed_p->leftbot_p =
					gridpage_p->u.feed_p->leftbot2_p;
			}
			if (gridpage_p->u.feed_p->rightbot_p == 0) {
				gridpage_p->u.feed_p->rightbot_p =
					gridpage_p->u.feed_p->rightbot2_p;
			}
		}

		/*
		 * (remheight - curminpad) is how much space is available on the
		 * last page for grids.   physpage is needed to know whether
		 * to use Header or Header2 (etc.) in calculations, and to
		 * set up data for left pages versus right pages.   The next
		 * two parms are needed for finding the correct top and bottom
		 * sizes for the last music page, and any grid-only pages.
		 */
		gridheight = grids_atend(remheight - curminpad, physpage,
			page_p->u.feed_p, gridpage_p->u.feed_p);

		if (gridheight > 0.0) {
			/* reduce remaining height by grids and curminpad */
			remheight -= gridheight + curminpad;
		}
	}

	/*
	 * Set pad below the bottom score.  If there is a footer
	 * or bottom, use the values from scorepad.  If not, force
	 * both to 0, so that none will be allowed.
	 */
	if (foot_p->height + botheight > 0.0) {
		curpad[totscores] = curminpad;
		maxpad[totscores] = curmaxpad;
	} else {
		curpad[totscores] = 0.0;
		maxpad[totscores] = 0.0;
	}

	abspage(origpage_p, cursep, maxsep, forcesep, curpad, maxpad,
			totscores, remheight, y_start);

	FREE(cursep);
	FREE(maxsep);
	FREE(curpad);
	FREE(maxpad);
	FREE(forcesep);
}

/*
 * Name:        abspage()
 *
 * Abstract:    Set all absolute vertical coordinates on a page.
 *
 * Returns:     void
 *
 * Description: This function positions the scores on this page as well as
 *		possible, and then sets all the absolute vertical coordinates
 *		for the scores and everything in them.
 */

static void
abspage(page_p, cursep, maxsep, forcesep, curpad, maxpad, totscores, remheight,
		y_start)

struct MAINLL *page_p;	/* point at first FEED for this page */
float cursep[];		/* this score's top line to above score's bottom line */
float maxsep[];		/* the max we'd like to expand cursep to */
short forcesep[];	/* force cursep to be used without change? */
float curpad[];		/* white pad between this score and above score */
float maxpad[];		/* the max we'd like to expand curpad to */
int totscores;		/* number of scores on this page */
double remheight;	/* extra vertical space available, to be distributed */
double y_start;		/* Y coord of top of first score (before padding) */

{
	struct MAINLL *mainll_p;/* point along main LL */
	struct FEED *feed_p;	/* point at a score feed on this page */
	struct TIMEDSSV *tssv_p;/* point along timed SSV lists */
	struct CHORD *ch_p;	/* point at a chord on this page */
	struct STAFF *staff_p;	/* point at a staff on this page */
	float min;		/* smallest number in curpad or cursep */
	float min2;		/* second smallest number in curpad or sep */
	float share;		/* space to add to the min numbers each loop */
	int mins;		/* how many numbers are tied for min */
	int n;			/* loop variable */
	int *is_min;		/* pointer to array malloc'ed below */
	int *hit_max;		/* pointer to array malloc'ed below */
	int allmax;		/* have all scores used the max sep allowed? */
	int prevclef;		/* clef on last visible staff of prev score */
	float prevscale;	/* staffscale of last vis staff of prev score*/
	float pstaffbot;	/* abs Y of bot line of prev score bot staff */


	debug(32,"abspage file=%s line=%d totscores=%d remheight=%f y_start=%f",
			page_p->inputfile, page_p->inputlineno, totscores,
			(float)remheight, (float)y_start);
	/*
	 * Array to hold which of the distances in curpad or cursep are
	 * minimal.
	 */
	MALLOCA(int, is_min, CURMAXSCORES + 1);
	/*
	 * Malloc an array to hold YES or NO as to whether this score's
	 * curpad or cursep has reached the maximum allowed.
	 */
	MALLOCA(int, hit_max, CURMAXSCORES + 1);

	/*
	 * The current values in curpad[] and cursep[] are for the case of
	 * the scores being packed as tightly as the stuff sticking out of them
	 * and the user's specification of minscpad and minscsep allow.
	 * (And also considering any "scoresep"s on "newscore" commands.)
	 * maxpad[] and maxsep[] have the values of maxscpad and maxscsep
	 * above each.  Now we need to spread the score out, distributing
	 * remheight appropriately.
	 */
	/*
	 * First, "smooth out" curpad[], so that the numbers in it will be as
	 * equal as possible, subject to maxpad[], but ignoring maxsep[].
	 * Anywhere where "newscore scoresep" was used, we ignore the padding.
	 */
	while (remheight > FUDGE) {
		/*
		 * For each score, remember in hit_max whether its curpad
		 * meets or exceeds the max pad allowed.  The fudge factor is
		 * so we'll pretend we made it, even if there is roundoff
		 * error.  If all scores' curpads have reached that, we're
		 * done, so break out.
		 */
		allmax = YES;
		for (n = 0; n <= totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (curpad[n] >= maxpad[n] - FUDGE) {
				hit_max[n] = YES;
			} else {
				hit_max[n] = NO;
				allmax = NO;
			}
		}
		if (allmax == YES) {
			break;
		}

		/*
		 * Find the smallest curpad among scores that haven't hit
		 * their max.
		 */
		min = 1000;
		for (n = 0; n <= totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (hit_max[n] == NO && curpad[n] < min)
				min = curpad[n];
		}

		mins = 0;	/* number of curpads tied for min */
		min2 = 1000;	/* second smallest curpad value */

		/*
		 * In this loop, mark which of the curpads are tied for the
		 * "min" value, and count how many are tied (mins).  Also, find
		 * the second smallest value (min2).  All this is done only for
		 * scores that haven't hit their max.
		 */
		for (n = 0; n <= totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (hit_max[n] == NO) {
				if (curpad[n] == min) {
					is_min[n] = YES;
					mins++;
				} else {
					is_min[n] = NO;
					if (curpad[n] < min2) {
						min2 = curpad[n];
					}
				}
			}
		}

		/*
		 * Don't let min2 exceed the maxpad of any eligible score.
		 * That way, when we spread the scores out to min2, we won't be
		 * spreading any of them beyond where they are allowed to go.
		 * In the next loop, ones that have reached their limit will
		 * get hit_max[] == YES, while other scores can continue to be
		 * spread more.
		 */
		for (n = 0; n <= totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (hit_max[n] == NO && min2 > maxpad[n]) {
				min2 = maxpad[n];
			}
		}

		/*
		 * We're going to add to all those minimum curpads, either
		 * using up all of remheight, or bringing them up equal to
		 * min2, whichever is lower.  We add the same amount to the
		 * curseps, since they change by the same amount as we move
		 * a score.
		 */
		share = remheight / mins;
		if (share > min2 - min) {
			share = min2 - min;
		}
		for (n = 0; n <= totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (hit_max[n] == NO && is_min[n] == YES) {
				curpad[n] += share;
				cursep[n] += share;
			}
		}

		/* decrement remheight by the amount we just used */
		remheight -= mins * share;
	}

	/*
	 * "Smooth out" cursep[], so that the numbers in it will be as
	 * equal as possible, subject to maxsep[], but ignoring maxpad[].
	 * Anywhere where "newscore scoresep" was used, we don't change.
	 * If there is only one score, the first "for" loop won't execute, and
	 * we'll break out.
	 */
	while (remheight > FUDGE) {
		/*
		 * For each score, remember in hit_max whether its cursep
		 * meets or exceeds the max sep allowed.  The fudge factor is
		 * so we'll pretend we made it, even if there is roundoff
		 * error.  If all scores' curseps have reached that, we're
		 * done, so break out.
		 */
		allmax = YES;
		for (n = 1; n < totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (cursep[n] >= maxsep[n] - FUDGE) {
				hit_max[n] = YES;
			} else {
				hit_max[n] = NO;
				allmax = NO;
			}
		}
		if (allmax == YES) {
			break;
		}

		/*
		 * Find the smallest cursep among scores that haven't hit
		 * their max.
		 */
		min = 1000;
		for (n = 1; n < totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (hit_max[n] == NO && cursep[n] < min)
				min = cursep[n];
		}

		mins = 0;	/* number of curseps tied for min */
		min2 = 1000;	/* second smallest cursep value */

		/*
		 * In this loop, mark which of the curseps are tied for the
		 * "min" value, and count how many are tied (mins).  Also, find
		 * the second smallest value (min2).  All this is done only for
		 * scores that haven't hit their max.
		 */
		for (n = 1; n < totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (hit_max[n] == NO) {
				if (cursep[n] == min) {
					is_min[n] = YES;
					mins++;
				} else {
					is_min[n] = NO;
					if (cursep[n] < min2) {
						min2 = cursep[n];
					}
				}
			}
		}

		/*
		 * Don't let min2 exceed the maxsep of any eligible score.
		 * That way, when we spread the scores out to min2, we won't be
		 * spreading any of them beyond where they are allowed to go.
		 * In the next loop, ones that have reached their limit will
		 * get hit_max[] == YES, while other scores can continue to be
		 * spread more.
		 */
		for (n = 1; n < totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (hit_max[n] == NO && min2 > maxsep[n]) {
				min2 = maxsep[n];
			}
		}

		/*
		 * We're going to add to all those minimum curseps, either
		 * using up all of remheight, or bringing them up equal to
		 * min2, whichever is lower.
		 */
		share = remheight / mins;
		if (share > min2 - min) {
			share = min2 - min;
		}
		for (n = 1; n < totscores; n++) {
			if (forcesep[n] == YES) {
				continue;
			}
			if (hit_max[n] == NO && is_min[n] == YES) {
				cursep[n] += share;
			}
		}

		/* decrement remheight by the amount we just used */
		remheight -= mins * share;
	}

	/* move to top of first score */
	y_start -= curpad[0];

	feed_p = 0;	/* flag that we haven't seen the first FEED yet */
	prevclef = NOCLEF;	/* prevent bogus "used before set" */
	prevscale = 1.0;	/* prevent bogus "used before set" */
	pstaffbot = 0.0;	/* prevent bogus "used before set" */

	/* we've applied SSVs up to the end of this page; need to back up */
	setssvstate(page_p);

	/*
	 * Loop through the main linked list for this page, setting all
	 * absolute vertical coordinates.
	 */
	for (mainll_p = page_p, n = 0; mainll_p != 0 && ! (n == totscores &&
			mainll_p->str == S_FEED); mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_SSV:
			/* by end of page, SSVs will be up to date for there */
			asgnssv(mainll_p->u.ssv_p);
			break;

		case S_FEED:
			/*
			 * If this is the first FEED on the page, and what
			 * follows is music (not a block), move to the top line
			 * of the first score.
			 */
			if (feed_p == 0 && IS_CLEFSIG_FEED(mainll_p)) {
				y_start = y_start - page_p->u.feed_p->c[RN] +
				 staffvertspace(page_p->u.feed_p->firstvis)/2.0;
			}

			/*
			 * Set the score's absolute coordinates.  The feed_p
			 * pointer will be used by other cases in later loops.
			 */
			feed_p = mainll_p->u.feed_p;

			/* if next is 0, this is a trailing feed, and it */
			/*  really has no meaningful coords */
			if (mainll_p->next == 0)
				continue;

			if (mainll_p->next->str == S_BLOCKHEAD) {
				/* move from top of block to middle of block */
				y_start -= feed_p->c[RN];
			} else {
				/* move from top line of score to middle of
				 * first staff */
				y_start -= staffvertspace(feed_p->firstvis)/2.0;
			}

			feed_p->c[AN] = y_start + feed_p->c[RN];
			feed_p->c[AY] = y_start;
			feed_p->c[AS] = y_start + feed_p->c[RS];

			/*
			 * If posscores() set this negative as a flag, set now
			 * to correct value: the position of slashesbetween.
			 * We try to center it between the outer staff lines,
			 * but we must not let it collide with clefs or measure
			 * numbers (hence the floor and ceiling).
			 */
			if (feed_p->sl_bet_top_offset < 0.0) {
				/* these Y values relative to current FEED AY */
				float curline;	/*cur FEED top staff top line*/
				float prevline;	/*prev FEED bot staff bot line*/
				float floor;	/* lowest the slashes can go */
				float ceiling;	/* highest the slashes can go */
				float slashhigh;/* vert extent of slashes */
				float offset;	/* store in sl_bet_top_offset */

				curline = halfstaffhi(feed_p->firstvis);
				floor = curline + clefspace(NOCLEF, 1.0,
					CLEF2PRINT(feed_p->firstvis),
					svpath(feed_p->firstvis, STAFFSCALE)
						->staffscale,
					(Score.measnum == MN_SCORE ? 1 : 0));
				prevline = pstaffbot - y_start;
				ceiling = prevline - clefspace(prevclef,
					prevscale, NOCLEF, 1.0, 0);
				slashhigh = Score.staffscale *
					(SL_BET_Y_TOTAL * STEPSIZE + STDPAD);

				/* try setting to exact middle between staffs */
				offset = (prevline + curline) / 2.0 +
					slashhigh / 2.0;
				/* but move if it would collide */
				if (offset > ceiling) {
					offset = ceiling;
				} else if (offset - slashhigh < floor) {
					offset = floor + slashhigh;
				}
				/* make half of the padding be above it */
				feed_p->sl_bet_top_offset = offset -
					Score.staffscale * STDPAD / 2.0;
			}

			/* unless last score, set up y_start for next one */
			if (n < totscores - 1) {
				/* top line of next score */
				y_start = y_start + feed_p->c[RS] +
					feed_p->lastdist - cursep[n + 1];
			}

			n++;

			/*
			 * Remember prev values for next time.  We're doing this
			 * for the benefit of slashesbetween, so we don't care
			 * about the "block" case.
			 */
			if (mainll_p->next->str == S_CLEFSIG) {
				prevclef = CLEF2PRINT(feed_p->lastvis);
				prevscale = svpath(feed_p->lastvis,
						STAFFSCALE)->staffscale;
			}
			break;

		case S_CHHEAD:
			/*
			 * Set each chord's absolute coordinates the same as
			 * the feed.  These are pretty arbitrary, since they
			 * are using only for drawing boxes with the MUP_BB
			 * environment variable.
			 */
			for (ch_p = mainll_p->u.chhead_p->ch_p; ch_p != 0;
					ch_p = ch_p->ch_p) {
				ch_p->c[AN] = feed_p->c[AN];
				ch_p->c[AY] = feed_p->c[AY];
				ch_p->c[AS] = feed_p->c[AS];
			}
			break;

		case S_BAR:
			/* apply timed SSVs to keep the clefs accurate */
			for (tssv_p = mainll_p->u.bar_p->timedssv_p;
					tssv_p != 0; tssv_p = tssv_p->next) {
				asgnssv(&tssv_p->ssv);
			}

			/*
			 * Set absolute N, Y, and S for the bar line.  Y can be
			 * copied from the score's Y; they are both the center
			 * line of the top visible staff.  But the score's N
			 * S can stick out, based on the groups present,
			 * whereas the bar line's N is the top line of the top
			 * staff, and its S is the bottom line of the bottom
			 * staff.
			 */
			mainll_p->u.bar_p->c[AN] = feed_p->c[AY] +
				halfstaffhi(feed_p->firstvis);
			mainll_p->u.bar_p->c[AY] = feed_p->c[AY];
			mainll_p->u.bar_p->c[AS] = feed_p->c[AS] +
					feed_p->lastdist;
			break;

		case S_CLEFSIG:
			/*
			 * If the clefsig doesn't contain a pseudo bar, just
			 * break.  But otherwise, set this bar's coords just
			 * like a normal bar.
			 */
			if (mainll_p->u.clefsig_p->bar_p == 0)
				break;
			mainll_p->u.clefsig_p->bar_p->c[AN] = feed_p->c[AY] +
				halfstaffhi(feed_p->firstvis);
			mainll_p->u.clefsig_p->bar_p->c[AY] = feed_p->c[AY];
			mainll_p->u.clefsig_p->bar_p->c[AS] = feed_p->c[AS] +
				feed_p->lastdist - halfstaffhi(feed_p->lastvis);
			break;

		case S_STAFF:
			/* if visible, set all abs vertical coords on staff */
			staff_p = mainll_p->u.staff_p;
			if (staff_p->visible == YES) {
				absstaff(feed_p, staff_p);

				/* this will remember the last visible staff's
				 * bottom line's AY */
				pstaffbot = staff_p->c[AY] -
						halfstaffhi(staff_p->staffno);
			}
			break;
		}

	}

	FREE(is_min);
	FREE(hit_max);
}

/*
 * Name:        absstaff()
 *
 * Abstract:    Set all absolute vertical coordinates for a STAFF structure.
 *
 * Returns:     void
 *
 * Description: This function sets all the absolute vertical coords for a
 *		STAFF structure; those of the staff itself, and those of
 *		everything hanging off it.
 */

static void
absstaff(feed_p, staff_p)

struct FEED *feed_p;		/* FEED for the score we're on */
struct STAFF *staff_p;		/* the staff to be set */

{
	struct GRPSYL *gs_p;	/* point at a group of syllable */
	struct STUFF *stuff_p;	/* point at a STUFF structure */
	struct CRVLIST *pp_p;	/* point at a coord for phrase point */
	int v;			/* index to voices or verses */
	int n;			/* loop variable */


	debug(32, "absstaff file=%s line=%d", staff_p->groups_p[0]->inputfile,
			staff_p->groups_p[0]->inputlineno);
	/* set the staff's own coords */
	staff_p->c[AN] = feed_p->c[AY] + staff_p->c[RN];
	staff_p->c[AY] = feed_p->c[AY] + staff_p->c[RY];
	staff_p->c[AS] = feed_p->c[AY] + staff_p->c[RS];

	/* do the voice(s) */
	for (v = 0; v < MAXVOICES; v++) {
		for (gs_p = staff_p->groups_p[v]; gs_p != 0; gs_p = gs_p->next){
			gs_p->c[AY] = staff_p->c[AY] + gs_p->c[RY];
			gs_p->c[AN] = staff_p->c[AY] + gs_p->c[RN];
			gs_p->c[AS] = staff_p->c[AY] + gs_p->c[RS];

			switch (gs_p->grpcont) {
			case GC_NOTES:
				for (n = 0; n < gs_p->nnotes; n++) {
					gs_p->notelist[n].c[AY] = staff_p->c[AY]
						+ gs_p->notelist[n].c[RY];
					gs_p->notelist[n].c[AN] = staff_p->c[AY]
						+ gs_p->notelist[n].c[RN];
					gs_p->notelist[n].c[AS] = staff_p->c[AY]
						+ gs_p->notelist[n].c[RS];
				}
				break;

			case GC_REST:
				gs_p->restc[AY] =
					staff_p->c[AY] + gs_p->restc[RY];
				gs_p->restc[AN] =
					staff_p->c[AY] + gs_p->restc[RN];
				gs_p->restc[AS] =									staff_p->c[AY] + gs_p->restc[RS];
				break;
			}
		}
	}

	/* do the verse(s) */
	for (v = 0; v < staff_p->nsyllists; v++) {
		for (gs_p = staff_p->syls_p[v]; gs_p != 0; gs_p = gs_p->next){
			gs_p->c[AY] = staff_p->c[AY] + gs_p->c[RY];
			gs_p->c[AN] = staff_p->c[AY] + gs_p->c[RN];
			gs_p->c[AS] = staff_p->c[AY] + gs_p->c[RS];
		}
	}

	/* do the stuff */
	for (stuff_p = staff_p->stuff_p; stuff_p != 0; stuff_p = stuff_p->next){
		stuff_p->c[AY] = staff_p->c[AY] + stuff_p->c[RY];
		stuff_p->c[AN] = staff_p->c[AY] + stuff_p->c[RN];
		stuff_p->c[AS] = staff_p->c[AY] + stuff_p->c[RS];

		/* if it's a phrase/tie/slur, do the phrase points too */
		if (stuff_p->stuff_type == ST_PHRASE ||
		    stuff_p->stuff_type == ST_TIESLUR ||
		    stuff_p->stuff_type == ST_TABSLUR ||
		    stuff_p->stuff_type == ST_BEND) {
			for (pp_p = stuff_p->crvlist_p; pp_p != 0;
						pp_p = pp_p->next)
				pp_p->y += staff_p->c[AY];
		}
	}
}

/*
 * Name:        grids_atend()
 *
 * Abstract:    Determine placement of chord grids to be printed at the end.
 *
 * Returns:     height of all the grids printed on this page
 *
 * Description: This function determines the placement of chord grids that are
 *		to be printed at the end of the song, and sets up the data in
 *		Atend_info accordingly.
 */

static double
grids_atend(vertavail, physpage, mfeed_p, gfeed_p)

double vertavail;	/* space available for grids and spreading out scores*/
int physpage;		/* physical page number if grids go on a new page */
struct FEED *mfeed_p;	/* FEED at start of last music page */
struct FEED *gfeed_p;	/* FEED applying to grid-only pages (may be same) */

{
	int pageside;			/* PGSIDE_* */
	struct GRID *grid_p;		/* point at a grid */
	int ngrids;			/* no. of grids used */
	float north, south, east, west;	/* coords for one grid */
	float farnorth, farsouth, fareast, farwest; /* farthest for any grid */
	float hstrwid;			/* half the width of chord string */
	float havail;			/* horizonal space available */
	int inrow;			/* no. of grids in one row */
	int nrows;			/* no. of rows of grids */
	float totalheight;		/* of all the rows */
	float white;			/* scorepad in inches */
	float upheight;			/* height of header + top */
	float downheight;		/* height of bottom + footer */


	debug(32, "grids_atend vertavail=%f", (float)vertavail);

	/* assume for now that the grids won't start on the last music page */
	pageside = page2side(physpage);

	/* malloc array of pointers to the grids that were used */
	MALLOCA(struct GRID *, Atend_info.grid_p, Atend_info.grids_used);

	/*
	 * Set pointers to the grids that were used.  While doing this, find
	 * the farthest extent of any grid, for each of the 4 directions.  The
	 * size of the chord string must also be considered in this.
	 */
	ngrids = 0;
	farnorth = farsouth = fareast = farwest = 0.0;
	for (grid_p = 0; (grid_p = nextgrid(grid_p)) != 0;  ) {
		if (grid_p->used == NO)
			continue;
		Atend_info.grid_p[ngrids++] = grid_p;
		gridsize(grid_p, -1, &north, &south, &east, &west);
		north += strheight(grid_p->name);
		hstrwid = strwidth(grid_p->name) / 2.0;
		if (north > farnorth)
			farnorth = north;
		if (south < farsouth)
			farsouth = south;
		if (hstrwid > east)
			east = hstrwid;
		if (east > fareast)
			fareast = east;
		if (-hstrwid < west)
			west = -hstrwid;
		if (west < farwest)
			farwest = west;
	}

	/* sort the pointers by grid name */
	qsort((char *)Atend_info.grid_p, ngrids, sizeof (struct GRID *),
			compgrids);

	/* horizontal available width to use */
	havail = PGWIDTH - eff_leftmargin((struct MAINLL *)0)
			 - eff_rightmargin((struct MAINLL *)0);

	/*
	 * Find max we could put in one row, allowing padding.  Note that we do
	 * not try to optimize the packing at all:  the biggest grid coord in
	 * any direction is what we use.  The "padding" to the right of the
	 * rightmost grid is not needed, so let it hang into the margin.
	 */
	inrow = (havail + HPADGRID) / (fareast - farwest + HPADGRID);
	if (inrow == 0) {
		ufatal("chord grid is too wide to fit on a page");
	}

	/* this determines how many rows there will be; it will not change */
	nrows = (ngrids + inrow - 1) / inrow;

	/*
	 * It could be that the last row would be far from full.  So attempt to
	 * spread the grids more equally between rows.
	 */
	while (nrows > 1 && inrow > 1) {
		inrow--;		/* try one less grid per row */
		if ((ngrids + inrow - 1) / inrow > nrows) {
			/* whoops, no. of rows increased, so undo last decr. */
			inrow++;
			break;
		}
	}

	Atend_info.grids_per_row = inrow;

	/* spread them out appropriately */
	Atend_info.horz_sep = havail / (nrows == 1 ? ngrids : inrow);

	/*
	 * Normally, the first grid's X is as far from the left margin as the
	 * last (on that line) grid's X is from the right margin.  But if any
	 * grids have "N fr", fareast may be bigger than -farwest.  So move
	 * everything to the left by half the difference.
	 */
	Atend_info.firstgrid_x = eff_leftmargin((struct MAINLL *)0) +
			Atend_info.horz_sep / 2.0 - (fareast + farwest) / 2.0;

	/*
	 * Base the vertical separation on the maximum case plus padding.  Of
	 * course, no padding is needed below the bottom row, so subtract it.
	 * The left and right pages' vert seps are the same right now (the
	 * minimum they can be), but later may be expanded differently.
	 */
	Atend_info.left.vert_sep = Atend_info.right.vert_sep =
			farnorth - farsouth + VPADGRID;
	totalheight = nrows * Atend_info.left.vert_sep - VPADGRID;

	white = Score.minscpad * STEPSIZE;

	if (totalheight <= vertavail && gfeed_p->pagefeed == NO) {
		/*
		 * It fits on the last page of music.  Set the absolute coord
		 * so that it rests above the footer and/or bottom block (if
		 * any) and bottom margin.  The page side of last music page is
		 * opposite of what the next page would have been.
		 */
		if (pageside == PGSIDE_RIGHT) {	/* music page is PGSIDE_LEFT */
			Atend_info.left.firstgrid_y = EFF_BOTMARGIN +
					totalheight - farnorth;

			/* if music page's number is 1, use the first footer */
			downheight = (physpage == 2 ?
					&Leftfooter : &Leftfooter2)->height +
				(mfeed_p->leftbot_p != 0 ?
					mfeed_p->leftbot_p->height : 0.0);
			if (downheight > 0) {
				Atend_info.left.firstgrid_y +=
						downheight + white;
			}

			Atend_info.left.rows_per_page = nrows;
		} else {	/* music page is PGSIDE_RIGHT */
			Atend_info.right.firstgrid_y = EFF_BOTMARGIN +
					totalheight - farnorth;

			/* if music page's number is 1, use the first footer */
			downheight = (physpage == 2 ?
					&Rightfooter : &Rightfooter2)->height +
				(mfeed_p->rightbot_p != 0 ?
					mfeed_p->rightbot_p->height : 0.0);
			if (downheight > 0) {
				Atend_info.right.firstgrid_y +=
						downheight + white;
			}

			Atend_info.right.rows_per_page = nrows;
		}

		return (totalheight);
	}

	/*
	 * All grids must go on later page(s).  Find how much height must be
	 * reserved for header/top and bottom/footer on those pages, both for
	 * left pages and right pages.  Since this cannot be the first page,
	 * we always use the "2" versions of headers, etc.
	 */
	/* make the grid page FEED a pagefeed, in case it isn't already */
	gfeed_p->pagefeed = YES;
	Atend_info.separate_page = YES;

	/* ---- do the work for left pages ---- */

	upheight = Leftheader2.height + (gfeed_p->lefttop2_p != 0 ?
				gfeed_p->lefttop2_p->height : 0.0);
	downheight = Leftfooter2.height + (gfeed_p->leftbot2_p != 0 ?
				gfeed_p->leftbot2_p->height : 0.0);

	/*
	 * It will have to go on other page(s).  Set the absolute coord to put
	 * it at the top.
	 */
	Atend_info.left.firstgrid_y = PGHEIGHT - EFF_TOPMARGIN -
			upheight - farnorth;
	if (upheight > 0) {
		Atend_info.left.firstgrid_y -= white;
	}

	/* reset vertavail to the amount of space on a whole page */
	vertavail = PGHEIGHT - EFF_TOPMARGIN - EFF_BOTMARGIN;
	if (upheight > 0)
		vertavail -= upheight + white;
	if (downheight > 0)
		vertavail -= downheight + white;

	/* find number of rows per page; must be at least 1 */
	Atend_info.left.rows_per_page = (vertavail + VPADGRID) /
			Atend_info.left.vert_sep;
	if (Atend_info.left.rows_per_page == 0)
		ufatal("chords grids are too high to fit on a page");

	/*
	 * If there is at least 1 full page, spread the rows out evenly.  The
	 * same spacing will be used on later pages of this side, even though
	 * the last page may not be full.  That's okay.
	 */
	if (nrows >= Atend_info.left.rows_per_page) {
		Atend_info.left.vert_sep = (vertavail + VPADGRID) /
				Atend_info.left.rows_per_page;
	}

	/* ---- do the work for right pages ---- */

	upheight = Rightheader2.height + (gfeed_p->righttop2_p != 0 ?
				gfeed_p->righttop2_p->height : 0.0);
	downheight = Rightfooter2.height + (gfeed_p->rightbot2_p != 0 ?
				gfeed_p->rightbot2_p->height : 0.0);

	/*
	 * It will have to go on other page(s).  Set the absolute coord to put
	 * it at the top.
	 */
	Atend_info.right.firstgrid_y = PGHEIGHT - EFF_TOPMARGIN -
			upheight - farnorth;
	if (upheight > 0) {
		Atend_info.right.firstgrid_y -= white;
	}

	/* reset vertavail to the amount of space on a whole page */
	vertavail = PGHEIGHT - EFF_TOPMARGIN - EFF_BOTMARGIN;
	if (upheight > 0)
		vertavail -= upheight + white;
	if (downheight > 0)
		vertavail -= downheight + white;

	/* find number of rows per page; must be at least 1 */
	Atend_info.right.rows_per_page = (vertavail + VPADGRID) /
			Atend_info.right.vert_sep;
	if (Atend_info.right.rows_per_page == 0)
		ufatal("chords grids are too high to fit on a page");

	/*
	 * If there is at least 1 full page, spread the rows out evenly.  The
	 * same spacing will be used on later pages of this side, even though
	 * the last page may not be full.  That's okay.
	 */
	if (nrows >= Atend_info.right.rows_per_page) {
		Atend_info.right.vert_sep = (vertavail + VPADGRID) /
				Atend_info.right.rows_per_page;
	}

	return (0.0);	/* nothing goes on the last page of music */
}

/*
 * Name:        compgrids()
 *
 * Abstract:    Compare grid names; used by qsort.
 *
 * Returns:     negative or positive
 *
 * Description: This function returns its result based on whether the grid
 *		pointed to by g1_p should precede or follow g2_p.  It uses
 *		their names in alphabetical order, basically, but it also
 *		understands accidentals.  They will never be equal because the
 *		grids are all unique.
 */

static int
compgrids(g1_p_p, g2_p_p)

#ifdef __STDC__
const void *g1_p_p;	/* the two grid pointers to compare */
const void *g2_p_p;
#else
char *g1_p_p;		/* the two grid pointers to compare */
char *g2_p_p;
#endif

{
	char *name[2];		/* pointers into first and second names */
	char *asc_ptr;		/* point at the first name in ASCII */
	char chbuff[MAXCHNAME];	/* hold the ASCII name of the first chord */
	int accnum[2];		/* accidental number, -2 to 2  (&& to x) */
	int ridx[2];		/* index to rest of string */
	int k;			/* loop variable */


	/*
	 * Translate the chords names to the way the user entered them (as
	 * closely as possible).  Since ascii_str() overwrites the same static
	 * area each time, we have to copy the first name to our own buffer.
	 * Rather than wasting time using malloc(), just put it in a fixed
	 * buffer.  If someone has an absurd name longer than MAXCHNAME, just
	 * cut it off.
	 */
	asc_ptr = ascii_str((*(struct GRID **)g1_p_p)->name, YES, NO, TM_CHORD);
	if ((int)strlen(asc_ptr) < MAXCHNAME) {
		(void)strcpy(chbuff, asc_ptr);
	} else {
		(void)strncpy(chbuff, asc_ptr, MAXCHNAME - 1);
		chbuff[MAXCHNAME - 1] = '\0';
	}
	name[0] = chbuff;
	name[1] = ascii_str((*(struct GRID **)g2_p_p)->name, YES, NO, TM_CHORD);

	/*
	 * First deal with the bizarre case where one or both of the chord
	 * strings are empty.  That will make later code easier.
	 */
	if (name[0][0] == '\0' || name[1][0] == '\0') {
		return (name[0][0] - name[1][0]);
	}

	/*
	 * Set the accidental values.
	 */
	for (k = 0; k < 2; k++) {
		switch (name[k][1]) {
		case '&':
			if (name[k][2] == '&') {
				accnum[k] = -2;	/* double flat */
				ridx[k] = 3;
			} else {
				accnum[k] = -1;	/* flat */
				ridx[k] = 2;
			}
			break;
		case '#':
			accnum[k] = 1;		/* sharp */
			ridx[k] = 2;
			break;
		case 'x':
			accnum[k] = 2;		/* double sharp */
			ridx[k] = 2;
			break;
		default:
			accnum[k] = 0;		/* no acc is like a natural */
			ridx[k] = 1;
			break;
		}
	}

	/*
	 * Usually we expect chord letters to be upper case, but some people
	 * use small letters for minor chords.  We want to keep chords like
	 * C and Cm next to each other, even if someone spells Cm as cm.  So
	 * we start out trying to ignore case.  It seems like a good idea to
	 * try ignoring it in any characters that come after the letter and
	 * accidental, too.  But if our two chords are found to be equal that
	 * way, we have to regard the case.  That could happen; for example,
	 * some people may write c for Cm.
	 */

	if (toupper(name[0][0]) != toupper(name[1][0])) {
		/* chord letters differ even when disregarding case */
		return (toupper(name[0][0]) - toupper(name[1][0]));
	}

	if (accnum[0] != accnum[1]) {
		/* accidentals differ */
		return (accnum[0] - accnum[1]);
	}

	if (strcasecmp(&name[0][ridx[0]], &name[1][ridx[1]]) != 0) {
		/* trailing characters differ even when disregarding case */
		return (strcasecmp(&name[0][ridx[0]], &name[1][ridx[1]]));
	}

	/*
	 * Everything was equal when we ignored case.  But because of the way
	 * the chords were loaded into the list, we know they differ.  So we
	 * stop ignoring case, first in the characters at the end, and
	 * finally in the chord letters.  It's our best guess as to what
	 * people would want.
	 */

	if (strcmp(&name[0][ridx[0]], &name[1][ridx[1]]) != 0) {
		return (strcmp(&name[0][ridx[0]], &name[1][ridx[1]]));
	}

	/* the chord letters must differ in case; return that difference */
	return (name[0][0] - name[1][0]);
}

/*
 * Name:        proc_css()
 *
 * Abstract:    Process groups involved with cross staff stemming.
 *
 * Returns:     void
 *
 * Description: This function does all the remaining work necessary for groups
 *		involved in cross staff stemming.
 */

static void
proc_css()

{
	struct MAINLL *mainll_p;	/* point along main LL */
	struct MAINLL *prevvis_p;	/* previous visible staff */
	struct MAINLL *nextvis_p;	/* next visible staff */
	struct TIMEDSSV *tssv_p;	/* point along a timed SSV list */
	struct STAFF *thisstaff_p;	/* point at a staff */
	struct GRPSYL *thisg_p;		/* point at a group */
	struct STUFF *stuff_p;		/* point at a stuff structure */
	struct CRVLIST *pp_p;		/* point at a coord for phrase point */
	RATIONAL vtime;			/* start time of groups */
	int vidx;			/* voice index */


	debug(16, "proc_css");
	initstructs();			/* clean out old SSV info */

	/*
	 * Loop through the whole MLL, looking for visible staffs, and keeping
	 * SSVs up to date (including midmeasure SSVs, since CSS notes are
	 * affected by clef changes).
	 */
	prevvis_p = 0;
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {

		switch (mainll_p->str) {
		case S_STAFF:
			thisstaff_p = mainll_p->u.staff_p;
			/* if staff is invisible, skip it */
			if (thisstaff_p->visible == NO) {
				continue;
			}
			break;		/* go handle this visible staff */
		case S_SSV:
			/* assign normal SSV */
			asgnssv(mainll_p->u.ssv_p);
			continue;
		case S_BAR:
			/* assign preceding measure's timed SSVs */
			for (tssv_p = mainll_p->u.bar_p->timedssv_p;
					tssv_p != 0;
					tssv_p = tssv_p->next) {
				asgnssv(&tssv_p->ssv);
			}
			/* FALL THROUGH */
		default:
			/* set prev to null in preparation for next measure */
			prevvis_p = 0;
			continue;
		}

		/* look for next visible staff, skipping invisible */
		for (nextvis_p = mainll_p->next; nextvis_p != 0 &&
				nextvis_p->str == S_STAFF &&
				nextvis_p->u.staff_p->visible == NO;
				nextvis_p = nextvis_p->next) {
			;
		}
		/* if no more visible staffs in score, set next to null */
		if (nextvis_p != 0 && nextvis_p->str != S_STAFF) {
			nextvis_p = 0;
		}

		/*
		 * thisstaff_p is a visible staff, and prevvis_p and nextvis_p
		 * are the MLL structs for the previous and next visible staffs,
		 * if they exist.  Loop through the voices on the this staff.
		 */
		for (vidx = 0; vidx < MAXVOICES; vidx++) {
			/*
			 * Loop through the groups of this voice, keeping track
			 * of the elapsed time, looking for groups that have
			 * CSS, and calling one_css() for them.
			 */
			vtime = Zero;
			for (thisg_p = thisstaff_p->groups_p[vidx]; thisg_p !=0;
					vtime = radd(vtime, thisg_p->fulltime),
					thisg_p = thisg_p->next) {

				switch (thisg_p->stemto) {
				case CS_SAME:
					continue;
				case CS_ABOVE:
					if (prevvis_p == 0) {
						l_ufatal(mainll_p->inputfile,
						mainll_p->inputlineno,
						"cannot cross staff stem 'with above' from top visible staff");
					}
					one_css(thisstaff_p,
						prevvis_p->u.staff_p,
						thisg_p, vtime);
					break;
				case CS_BELOW:
					if (nextvis_p == 0) {
						l_ufatal(mainll_p->inputfile,
						mainll_p->inputlineno,
						"cannot cross staff stem 'with below' from bottom visible staff");
					}
					one_css(thisstaff_p,
						nextvis_p->u.staff_p,
						thisg_p, vtime);
					break;
				}
			}
		}

		prevvis_p = mainll_p;
	}

	/*
	 * Now we have to call beamstem() again, to do the work that it
	 * couldn't do before on groups affected by CSS.
	 */
	CSSpass = YES;
	beamstem();

	/*
	 * Do "horizontal avoidance": moving CSS groups sideways if necessary
	 * because they would collide with groups on the other staff.
	 */
	horzavoid();

	/*
	 * Back in relvert.c, we skipped placing tie/slur/bend/phrases whose
	 * endpoint groups were affected by CSS.  Now that we know where the
	 * final group boundaries are, we set up the coords for these items.
	 * tieslur_points and phrase_points destroy groups' AN and AS, and
	 * depends on them starting out as zero.  So zero them now and restore
	 * them later.  Because these items can cross bar lines, we need
	 * to zap all of these coords in this first loop, and have a separate
	 * loop to do the main work (and restore the groups' coords).
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		if (mainll_p->str != S_STAFF) {
			continue;
		}
		thisstaff_p = mainll_p->u.staff_p;

		for (vidx = 0; vidx < MAXVOICES; vidx++) {
			for (thisg_p = thisstaff_p->groups_p[vidx];
					thisg_p != 0; thisg_p = thisg_p->next) {
				thisg_p->c[AN] = 0.0;
				thisg_p->c[AS] = 0.0;
			}
		}
	}

	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		if (mainll_p->str != S_STAFF) {
			continue;
		}
		thisstaff_p = mainll_p->u.staff_p;

		/*
		 * Find and handle every tie/slur/bend/phrase starting in this
		 * staff.
		 */
		for (stuff_p = thisstaff_p->stuff_p;
				stuff_p != 0; stuff_p = stuff_p->next) {
			switch (stuff_p->stuff_type) {
			case ST_PHRASE:
				if (css_affects_phrase(stuff_p,
							mainll_p) == YES) {
					phrase_points(mainll_p, stuff_p);

					stuff_p->c[AY] = thisstaff_p->c[AY]
						       + stuff_p->c[RY];
					stuff_p->c[AN] = thisstaff_p->c[AY]
						       + stuff_p->c[RN];
					stuff_p->c[AS] = thisstaff_p->c[AY]
						       + stuff_p->c[RS];

					/* do the phrase points too */
					for (pp_p = stuff_p->crvlist_p;
					     pp_p != 0; pp_p = pp_p->next) {

						pp_p->y += thisstaff_p->c[AY];
					}
				}
				break;
			case ST_TIESLUR:
			case ST_BEND:
				if (css_affects_tieslurbend(stuff_p,
							mainll_p) == YES) {
					if (stuff_p->stuff_type == ST_TIESLUR) {
						tieslur_points(mainll_p, stuff_p);
					} else {
						bend_points(mainll_p, stuff_p);
					}

					stuff_p->c[AY] = thisstaff_p->c[AY]
						       + stuff_p->c[RY];
					stuff_p->c[AN] = thisstaff_p->c[AY]
						       + stuff_p->c[RN];
					stuff_p->c[AS] = thisstaff_p->c[AY]
						       + stuff_p->c[RS];

					/* do the tie/slur/bend points too */
					for (pp_p = stuff_p->crvlist_p;
					     pp_p != 0; pp_p = pp_p->next) {

						pp_p->y += thisstaff_p->c[AY];
					}
				}
				break;
			}
		}

		/*
		 * phrase_points destroys groups' AN and AS.  And some code in
		 * the second pass of beamstem.c doesn't set the absolute
		 * coords of groups.  So go through now and set the absolute
		 * coords of all groups.
		 */
		for (vidx = 0; vidx < MAXVOICES; vidx++) {
			for (thisg_p = thisstaff_p->groups_p[vidx];
					thisg_p != 0; thisg_p = thisg_p->next) {
				thisg_p->c[AN] = thisstaff_p->c[AY]
						+ thisg_p->c[RN];
				thisg_p->c[AY] = thisstaff_p->c[AY]
						+ thisg_p->c[RY];
				thisg_p->c[AS] = thisstaff_p->c[AY]
						+ thisg_p->c[RS];
			}
		}
	}
}

/*
 * Name:        one_css()
 *
 * Abstract:    Process one group involved with cross staff stemming.
 *
 * Returns:     void
 *
 * Description: This function processes one CSS group.  It moves the CSS notes
 *		in the group to fall into the correct place on the other staff.
 *		When necessary, it also adjusts the group boundary.
 */

static void
one_css(ts_p, os_p, tg_p, time)

struct STAFF *ts_p;		/* This Staff, the normal one for the grpsyl */
struct STAFF *os_p;		/* Other Staff that the grpsyl has notes on */
struct GRPSYL *tg_p;		/* This Grpsyl */
RATIONAL time;			/* time offset of this grpsyl */

{
	struct GRPSYL *og_p;	/* Other Grpsyl (some grpsyl on other staff) */
	int foundclef;		/* found a clef change on other staff? */
	RATIONAL cleftime;	/* time at which the last clef change happens*/
	RATIONAL tt;		/* temporary time variable */
	float offset;		/* distance from old note position to new */
	int upfromc4;		/* steps up from middle C */
	int clef;		/* clef in force on other staff */
	int vidx;		/* voice index */
	int n;			/* loop variable */


	/*
	 * Set globals like Staffscale according our staff.  The parse phase
	 * ensures that the two staffs have the same staffscale.
	 */
	set_staffscale(ts_p->staffno);

	/*
	 * We need to find out what clef is in force on the other staff.  We
	 * start with the current value; but it may change midmeasure.  We
	 * can't just use the timed SSVs, because there are weird cases
	 * where the clef got put farther to the right (because the clef was
	 * changed before rests or spaces).  So we have to search all the
	 * voices for clefs.  We look for the rightmost clef that does not
	 * exceed the given time value.
	 */
	/* find clef in force on other staff at start of this measure */
	clef = svpath(os_p->staffno, CLEF)->clef;
	foundclef = NO;
	cleftime = Zero;
	for (vidx = 0; vidx < MAXVOICES; vidx++) {
		tt = Zero;
		for (og_p = os_p->groups_p[vidx]; og_p != 0 && LE(tt, time);
				og_p = og_p->next) {
			/* if group has a clef, and either it's the first group
			 * found to have one or it's later than the latest such
			 * group found so far . . . */
			if (og_p->clef != NOCLEF &&
					(foundclef == NO || GT(tt, cleftime))) {
				foundclef = YES;
				clef = og_p->clef;	/* remember this clef*/
				cleftime = tt;		/* and when it was */
			}
			tt = radd(tt, og_p->fulltime);
		}
	}

	/*
	 * Everything that has to move will move by the same offset.  Calculate
	 * it, using the first CSS note.  First find its stepsup on the new
	 * staff, like setnotes.c does for the normal staff.  Subtract new
	 * minus old vertical positions.
	 */
	n = FCNI(tg_p);
	upfromc4 = (tg_p->notelist[n].octave - 4) * 7 +
		Letshift[ tg_p->notelist[n].letter - 'a' ];
	tg_p->notelist[n].stepsup = upfromc4 + clef - ALTO;
	offset = (os_p->c[AY] + tg_p->notelist[n].stepsup * Stepsize) -
		tg_p->notelist[n].c[AY];

	/* move all the CSS notes and their dots */
	for ( ; n <= LCNI(tg_p); n++) {
		upfromc4 = (tg_p->notelist[n].octave - 4) * 7 +
			Letshift[ tg_p->notelist[n].letter - 'a' ];
		tg_p->notelist[n].stepsup = upfromc4 + clef - ALTO;
		tg_p->notelist[n].c[RN] += offset;
		tg_p->notelist[n].c[RY] += offset;
		tg_p->notelist[n].c[RS] += offset;
		tg_p->notelist[n].c[AN] += offset;
		tg_p->notelist[n].c[AY] += offset;
		tg_p->notelist[n].c[AS] += offset;
		if (tg_p->dots > 0) {
			tg_p->notelist[n].ydotr += offset;
		}
	}

	/*
	 * If the CSS note(s) were not on the stemside, stemlen and group
	 * boundaries were set already in beamstem.c, but we need to fix them
	 * here to account for moving the CSS notes.
	 */
	if (STEMSIDE_CSS(tg_p) == NO) {
		/* but change stemlen only under certain circumstances */
		if (tg_p->stemlen != 0.0 &&
				(NNN(tg_p) != 0 || tg_p->beamloc != NOITEM)) {
			tg_p->stemlen += fabs(offset);
		}
		if (tg_p->stemdir == UP) {
			tg_p->c[RS] = tg_p->notelist[tg_p->nnotes - 1].c[RS]
					- Stdpad;
			tg_p->c[AS] = tg_p->notelist[tg_p->nnotes - 1].c[AS]
					- Stdpad;
		} else {
			tg_p->c[RN] = tg_p->notelist[0].c[RN] + Stdpad;
			tg_p->c[AN] = tg_p->notelist[0].c[AN] + Stdpad;
		}
	}
}

/*
 * Name:        horzavoid()
 *
 * Abstract:    Move CSS groups horizontally to avoid collisions on other staff.
 *
 * Returns:     void
 *
 * Description: This function goes through the MLL, and for each CSS group,
 *		calls a function to do horizontal avoidance.
 */

static void
horzavoid()

{
	struct MAINLL *mainll_p;	/* point along main LL */
	struct GRPSYL *gs_p;		/* point at a group */
	int vidx;			/* voice index */
	RATIONAL time;			/* start time of a group */


	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		if (mainll_p->str != S_STAFF) {
			continue;
		}

		for (vidx = 0; vidx < MAXVOICES; vidx++) {
			if (svpath(mainll_p->u.staff_p->staffno, VISIBLE)
					->visible == NO) {
				continue;
			}
			time = Zero;
			for (gs_p = mainll_p->u.staff_p->groups_p[vidx];
					gs_p != 0; gs_p = gs_p->next) {
				if (gs_p->stemto != CS_SAME) {
					avoidone(mainll_p, gs_p, time);
				}
				time = radd(time, gs_p->fulltime);
			}
		}
	}
}

/*
 * Name:        avoidone()
 *
 * Abstract:    Move CSS group horizontally to avoid collisions on other staff.
 *
 * Returns:     void
 *
 * Description: This function finds whether the given group collides with any
 *		groups on the other staff.  If so, it moves that group, along
 *		with all other groups on its staff and their preceding grace
 *		groups, to the right enough so that the group no longer
 *		collides.  But it won't move it so far that it would collide
 *		with a later group on its own staff.
 */

static void
avoidone(mainll_p, cssg_p, time)

struct MAINLL *mainll_p;	/* the MLL for our group's staff */
struct GRPSYL *cssg_p;		/* the CSS group we are working on */
RATIONAL time;			/* time offset of this group */

{
	struct MAINLL *mll_p;	/* point along main LL */
	int otherstaffno;	/* staff where the CSS notes are */
	struct GRPSYL *gs_p;	/* point along grpsyl lists */
	struct GRPSYL *gs2_p;	/* another pointer along grpsyl lists */
	struct CHORD *ch_p;	/* point at chord we're in */
	float movedist;		/* distance to move groups */
	float otherhorz;	/* east boundary of groups on other staff */
	float slope;		/* slope of a beam */
	float deltax;		/* change in X coord of stem tip */
	int gotone;		/* flag variable */
	int n;			/* loop variable */


	/* never move the group if the user is forcing it with "ho" */
	if (cssg_p->ho_usage != HO_NONE) {
		return;
	}

	/*
	 * Find the other staff's number.
	 */
	if (cssg_p->stemto == CS_ABOVE) {
		for (mll_p = mainll_p->prev; mll_p != 0 && mll_p->str == S_STAFF
		&& mll_p->u.staff_p->visible == NO; mll_p = mll_p->prev) {
			;
		}
	} else {
		for (mll_p = mainll_p->next; mll_p != 0 && mll_p->str == S_STAFF
		&& mll_p->u.staff_p->visible == NO; mll_p = mll_p->next) {
			;
		}
	}
	if (mll_p == 0 || mll_p->str != S_STAFF) {
		pfatal("missing staff in avoidone");
	}
	otherstaffno = mll_p->u.staff_p->staffno;

	/*
	 * Find what groups, if any, the other staff has at this time value.
	 * First we find the GPRSYL at which the search begins.
	 */
	if (cssg_p->stemto == CS_ABOVE) {
		/*
		 * We will start the search at this first grpsyl in the chord.
		 */
		ch_p = gs2ch(mainll_p, cssg_p);
		gs_p = ch_p->gs_p;
	} else {
		/*
		 * We will start the search at our group, or if it is grace,
		 * the main group that follows.
		 */
		for (gs_p = cssg_p; gs_p->grpvalue == GV_ZERO;
				gs_p = gs_p->next) {
			;
		}
		ch_p = 0;	/* remember we don't know the chord */
	}

	/* find the first GRPSYL, if any, on the other staff at this time */
	for ( ; gs_p != 0 && gs_p->staffno < otherstaffno; gs_p = gs_p->gs_p) {
		;
	}

	/* if no groups on the other staff, there is no need to move anything */
	if (gs_p == 0 || gs_p->grpsyl == GS_SYLLABLE ||
			gs_p->staffno > otherstaffno) {
		return;
	}

	/*
	 * Find the easternmost extent of any group on the other staff that
	 * extends far enough vertically to run into our group.  We don't care
	 * about grace groups, because they are on the west side, and we are
	 * going to move our group to the east side.
	 * Accidentals need special consideration, because their vertical
	 * extents are not included in the group boundaries.
	 */
	gotone = NO;
	otherhorz = 0.0;	/* avoid "used before set" warning */
	for ( ; gs_p != 0 && gs_p->grpsyl == GS_GROUP &&
			gs_p->staffno == otherstaffno; gs_p = gs_p->gs_p) {
		/* spaces never interfere; mr and mrpt rarely do, and their
		 * coords make them seem really wide, so ignore them too */
		if (gs_p->grpcont == GC_SPACE || gs_p->is_meas == YES) {
			continue;
		}

		/* if any accs collide, declare a problem */
		if (acccollide(cssg_p, gs_p) == YES) {
			otherhorz = gs_p->c[AE];
			gotone = YES;
			continue;
		}

		/*
		 * "continue" if the groups don't overlap vertically.  Also
		 * "continue" if the lowest note of the top group is at least
		 * two steps above the highest note of the bottom group, and
		 * the stems point away from the other group.  We want to
		 * allow that case, even though the groups overlap slightly
		 * because of padding.
		 */
		if (cssg_p->c[AN] <= gs_p->c[AS]) {
			continue;
		}
		if (cssg_p->c[AS] >= gs_p->c[AN]) {
			continue;
		}
		if (cssg_p->stemdir == DOWN && gs_p->stemdir == UP) {
			if (gs_p->grpcont == GC_NOTES &&
			    cssg_p->notelist[0].stepsup + 2 <=
			    gs_p->notelist[gs_p->nnotes - 1].stepsup) {
				continue;
			}
		}
		if (cssg_p->stemdir == UP && gs_p->stemdir == DOWN) {
			if (gs_p->grpcont == GC_NOTES &&
			    cssg_p->notelist[cssg_p->nnotes - 1].stepsup >=
			    gs_p->notelist[0].stepsup + 2) {
				continue;
			}
		}
		if (gotone == NO || gs_p->c[AE] > otherhorz) {
			otherhorz = gs_p->c[AE];
			gotone = YES;
		}
	}

	/*
	 * If our group doesn't reach the other staff's groups vertically,
	 * and there were no cases of accs in the two groups colliding,
	 * there is no need to move anything.
	 */
	if (gotone == NO) {
		return;
	}

	/*
	 * Find how far we'd need to move our group to the right to be beyond
	 * any of the other staff's groups.  If somehow that is not positive,
	 * there is no need to move.
	 */
	movedist = otherhorz - cssg_p->c[AW];
	if (movedist <= 0.0) {
		return;
	}

	/* find the first nongrace group at this time on our staff */
	if (cssg_p->vno == 1) {
		for (gs_p = cssg_p; gs_p->grpvalue == GV_ZERO;
				gs_p = gs_p->next) {
			;
		}
	} else {
		if (ch_p == 0) {
			ch_p = gs2ch(mainll_p, cssg_p);
		}
		/* find the first GRPSYL, if any, on our staff at this time */
		for (gs_p = ch_p->gs_p; gs_p != 0 && gs_p->staffno <
				cssg_p->staffno; gs_p = gs_p->gs_p) {
			;
		}
	}

	/*
	 * For each group on this staff in this chord, see whether moving it to
	 * the right this far is going to make its AX almost equal or exceed
	 * the following group, if any.  That would be intolerable.  If so,
	 * reduce movedist to keep that from happening.  Better to overlap than
	 * to have this happen.
	 */
	set_staffscale(gs_p->staffno);
	for (gs2_p = gs_p; gs2_p != 0 && gs2_p->grpsyl == GS_GROUP &&
			gs2_p->staffno == cssg_p->staffno; gs2_p = gs2_p->gs_p){
		if (gs2_p->next != 0 && gs2_p->c[AX] + movedist + Stepsize >=
					gs2_p->next->c[AX]) {
			movedist = gs2_p->next->c[AX] - gs2_p->c[AX] - Stepsize;
		}
	}
	if (movedist <= 0.0) {
		return;
	}

	/*
	 * For each group on this staff in this chord, and for all their
	 * preceding grace groups, move them to the east.  Adjust stem lengths
	 * of beamed groups.
	 */
	for ( ; gs_p != 0 && gs_p->grpsyl == GS_GROUP &&
			gs_p->staffno == cssg_p->staffno; gs_p = gs_p->gs_p) {

		/* never move the group if the user is forcing it with "ho" */
		if (gs_p->ho_usage != HO_NONE) {
			continue;
		}

		/*
		 * If the group is beamed and the beam is not horizontal, the
		 * stem length needs to be changed so it will meet the beam.
		 */
		if (gs_p->beamloc != NOITEM && gs_p->grpcont == GC_NOTES) {
			/*
			 * Find a neighboring group in the beamed set so we can
			 * find the beam's slope.  The prev group is already
			 * corrected; our group and the next group haven't been
			 * moved yet; so the stems of all 3 are currently
			 * touching the beam and are valid for finding slope.
			 */
			if (gs_p->beamloc == STARTITEM) {
				gs2_p = nextsimilar(gs_p);
			} else {
				gs2_p = prevsimilar(gs_p);
			}
			slope = (find_y_stem(gs2_p) - find_y_stem(gs_p)) /
				(find_x_stem(gs2_p) - find_x_stem(gs_p));

			deltax = slope * movedist;

			if (gs_p->stemdir == UP) {
				gs_p->stemlen += deltax;
				gs_p->c[RN] += deltax;
				gs_p->c[AN] += deltax;
			} else {
				gs_p->stemlen -= deltax;
				gs_p->c[RS] += deltax;
				gs_p->c[AS] += deltax;
			}
		}

		/*
		 * Always do our group (a nongrace group), then loop
		 * additionally for all preceding graces.
		 */
		gs2_p = gs_p;
		do {
			gs2_p->c[AW] += movedist;
			gs2_p->c[AX] += movedist;
			gs2_p->c[AE] += movedist;

			/* if it's a group with notes, do the notes too */
			if (gs2_p->grpcont == GC_NOTES) {
				for (n = 0; n < gs2_p->nnotes; n++) {
					gs2_p->notelist[n].c[AW] += movedist;
					gs2_p->notelist[n].c[AX] += movedist;
					gs2_p->notelist[n].c[AE] += movedist;
				}
			}

			gs2_p = gs2_p->prev;
		} while (gs2_p != 0 && gs2_p->grpvalue == GV_ZERO);
	}
}

/*
 * Name:        acccollide()
 *
 * Abstract:    Decide whether the accs of two groups collide.
 *
 * Returns:     YES if there is a collision, else NO.
 *
 * Description: This function goes through the notes in both the groups,
 *		checking whether any of their accidentals collide.
 */

static int
acccollide(gs1_p, gs2_p)

struct GRPSYL *gs1_p;
struct GRPSYL *gs2_p;

{
	int idx1;
	int idx2;
	float accasc, accdesc, accwidth;
	float acc1n, acc1s, acc1e, acc1w;
	float acc2n, acc2s, acc2e, acc2w;
	float staffscale;


	/* because of CSS, both groups must have the same staffscale */
	staffscale = svpath(gs1_p->staffno, STAFFSCALE)->staffscale;

	for (idx1 = 0; idx1 < gs1_p->nnotes; idx1++) {
		if ( ! has_accs(gs1_p->notelist[idx1].acclist) ) {
			continue;
		}

		accdimen(gs1_p->staffno, &gs1_p->notelist[idx1],
				&accasc, &accdesc, &accwidth);
		acc1n = gs1_p->notelist[idx1].c[AY] + accasc * staffscale;
		acc1s = gs1_p->notelist[idx1].c[AY] - accdesc * staffscale;
		acc1w = gs1_p->c[AX] + gs1_p->notelist[idx1].waccr;
		acc1e = acc1w + accwidth * staffscale;
	
		for (idx2 = 0; idx2 < gs2_p->nnotes; idx2++) {
			if ( ! has_accs(gs2_p->notelist[idx2].acclist) ) {
				continue;
			}

			accdimen(gs2_p->staffno, &gs2_p->notelist[idx2],
					&accasc, &accdesc, &accwidth);
			acc2n = gs2_p->notelist[idx2].c[AY] +
					accasc * staffscale;
			acc2s = gs2_p->notelist[idx2].c[AY] -
					accdesc * staffscale;
			acc2w = gs2_p->c[AX] + gs2_p->notelist[idx2].waccr;
			acc2e = acc2w + accwidth * staffscale;

			if (acc1n > acc2s && acc1s < acc2n &&
			    acc1e > acc2w && acc1w < acc2e) {

				return (YES);
			}
		}
	}

	return (NO);
}

/*
 * Name:        set_csb_stems()
 *
 * Abstract:    Set stem lengths for groups involved in cross staff beaming.
 *
 * Returns:     void
 *
 * Description: This function searches the MLL for cross staff beaming places.
 *		For each one, it calls onecsb() to set the stem lengths.
 */

static void
set_csb_stems()

{
	struct MAINLL *mainll_p;	/* point along main LL */
	struct MAINLL *mll_p;		/* point along main LL again */
	struct STAFF *staff1_p, *staff2_p; /* point at top and bottom staffs */
	struct GRPSYL *gs1_p, *gs2_p;	/* point at top and bottom groups */
	int v, bv;			/* loop thru voices, top and bottom */
	RATIONAL vtime1, vtime2;	/* start time of groups */


	debug(16, "set_csb_stems");
	initstructs();			/* clean out old SSV info */

	/*
	 * Loop through the whole MLL, looking for visible staffs that are
	 * not the last visible staff in their score.  Then find cross staff
	 * beamings and call a function to set stem lengths.
	 */
	for (mainll_p = Mainllhc_p; mainll_p != 0; mainll_p = mainll_p->next) {
		/* apply SSVs to keep staffscale up to date */
		if (mainll_p->str == S_SSV) {
			asgnssv(mainll_p->u.ssv_p);
			continue;
		}

		if (mainll_p->str != S_STAFF)
			continue;

		/* if staff is invisible, skip it */
		staff1_p = mainll_p->u.staff_p;
		if (staff1_p->visible == NO)
			continue;

		/* look for next visible staff, skipping invisible */
		for (mll_p = mainll_p->next; mll_p != 0 && mll_p->str ==
				S_STAFF && mll_p->u.staff_p->visible == NO;
				mll_p = mll_p->next)
			;
		/* if no more visible staffs in score, skip */
		if (mll_p == 0 || mll_p->str != S_STAFF)
			continue;

		staff2_p = mll_p->u.staff_p;

		/*
		 * staff1_p and staff2_p are two neighboring visible staffs
		 * (possibly with invisible ones in between).  Loop through the
		 * voices on the top staff.  For ones that don't exist, their
		 * pointers will be 0 and the inside loop will do nothing.
		 */
		for (v = 0; v < MAXVOICES; v++) {
			/*
			 * Loop through the groups of this voice, keeping track
			 * of the elapsed time, looking for the first group of
			 * each CSB set that is joined with the staff below.
			 * It could be any of the voices on the staff below.
			 * The parser deals with any checks concerning voices
			 * being in the way of each other.
			 */
			vtime1 = Zero;
			for (gs1_p = staff1_p->groups_p[v]; gs1_p != 0;
					vtime1 = radd(vtime1, gs1_p->fulltime),
					gs1_p = gs1_p->next) {

				if (gs1_p->beamto != CS_BELOW ||
				    gs1_p->beamloc != STARTITEM)
					continue;

				for (bv = 0; bv < MAXVOICES; bv++) {
					vtime2 = Zero;
					for (gs2_p = staff2_p->groups_p[bv];
							gs2_p != 0 &&
							(LT(vtime2, vtime1) ||
							gs2_p->grpvalue ==
								GV_ZERO);
							gs2_p = gs2_p->next) {
						vtime2 = radd(vtime2,
							gs2_p->fulltime);
					}
					if (gs2_p != 0 && EQ(vtime2, vtime1) &&
					    gs2_p->beamto == CS_ABOVE &&
					    gs2_p->beamloc == STARTITEM) {

						onecsb(gs1_p, gs2_p);
					}
				}
			}
		}
	}
}

/*
 * Name:        onecsb()
 *
 * Abstract:    Set stem lengths for one instance of cross staff beaming.
 *
 * Returns:     void
 *
 * Description: This function finds the stem directions on the two staffs of
 *		a CSB and the first and last groups of it that are note groups.
 *		If the user didn't specify the stem lengths for those outer
 *		groups (which determines the equation of the beams), it calls a
 *		function to decide what the equation should be; otherwise it
 *		finds the equation in-line.  Then it sets all the groups' stem
 *		lengths.
 */

/*
 * Given the STARTITEM group of a CSB (whether notes or space), return the
 * first CSB group that is notes.  Embedded grace groups are not part of CSB.
 */
#define FIRSTCSB(gs_p)	(gs_p->grpcont == GC_NOTES ? gs_p : nextcsb(gs_p))

static void
onecsb(start1_p, start2_p)

struct GRPSYL *start1_p;	/* first GRPSYL on top staff */
struct GRPSYL *start2_p;	/* first GRPSYL on bottom staff */

{
	struct GRPSYL *gs_p;	/* point at a group */
	int topdir, botdir;	/* stem directions of the two lists */
	struct GRPSYL *end1_p, *end2_p;	/* ending group in each list */
	struct GRPSYL *first_p, *last_p;/* first and last note groups in CSB */
	float firstx, lastx;	/* x coords of end of stems */
	float firsty, lasty;	/* y coords of stems */
	float b0, b1;		/* y intercept and slope of the beam */
	float stemshift;	/* x distance of stem from center of note */
	float x;		/* x coord of a stem */
	float outstem;	/* the part of the stemlen outside notes of group */


	/*
	 * Set globals like Staffscale for use by the rest of the file.  The
	 * parse phase ensures that the two staffs have the same staffscale.
	 */
	set_staffscale(start1_p->staffno);

	topdir = botdir = UP;	/* prevent useless 'used before set' warnings */

	/*
	 * Find stemdir of the top groups.  (They will be consistent; that was
	 * enforced in dobunch().)  Set end1_p to the last group.
	 */
	for (gs_p = FIRSTCSB(start1_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		if (gs_p->grpcont == GC_NOTES)
			topdir = gs_p->stemdir;
	}
	for (end1_p = start1_p; end1_p != 0 && end1_p->beamloc != ENDITEM;
			end1_p = nextnongrace(end1_p))
		;
	if (end1_p == 0)
		pfatal("no ENDITEM in beamed set (onecsb[1])");

	/* do the same for the bottom groups */
	for (gs_p = FIRSTCSB(start2_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		if (gs_p->grpcont == GC_NOTES)
			botdir = gs_p->stemdir;
	}
	for (end2_p = start2_p; end2_p != 0 && end2_p->beamloc != ENDITEM;
			end2_p = nextnongrace(end2_p))
		;
	if (end2_p == 0)
		pfatal("no ENDITEM in beamed set (onecsb[2])");

	if (topdir == UP && botdir == DOWN) {
		l_ufatal(start2_p->inputfile, start2_p->inputlineno,
		"when beaming across staffs, cannot have stems up on top staff and down on bottom");
	}

	/*
	 * Set first_p and last_p to the first and last note groups, whichever
	 * staff(s) they are on.
	 */
	first_p = start1_p->grpcont == GC_NOTES ? start1_p : start2_p;
	last_p = end1_p->grpcont == GC_NOTES ? end1_p : end2_p;

	/*
	 * Find half the width of a note head; the stems will need to be
	 * shifted by that amount from the center of the notes so that they
	 * will meet the edge of the notes properly.
	 */
	stemshift = getstemshift(first_p);


	/*
	 * The user must either specify a stem length for both first and last
	 * groups, or neither.  (The parse phase enforces that.)  If neither,
	 * call a function to determine a line for a beam.  It sets b0 and b1
	 * for that line.
	 */
	if (IS_STEMLEN_UNKNOWN(first_p->stemlen) ||
	    IS_STEMLEN_UNKNOWN(last_p->stemlen)) {
		/*
		 * User did not provide both outer stem lengths.  Find the best
		 * line.  But if the stemlen parm was zero, we get back "NO",
		 * and we set all stems to zero.
		 */
		if (calcline(start1_p, end1_p, start2_p, end2_p, first_p,
				last_p, topdir, botdir, &b0, &b1) == NO) {
			for (gs_p = first_p; gs_p != end1_p->next;
			     gs_p = nxtbmnote(gs_p, start1_p, end1_p->next)) {
				gs_p->stemlen = 0.0;
			}
			return;
		}
	} else {
		/*
		 * User provided outer stem lengths.  If they are zero, force
		 * all groups to zero and get out.  There will be no stems and
		 * no beams.
		 */
		if (first_p->stemlen == 0.0 && last_p->stemlen == 0.0) {
			for (gs_p = first_p; gs_p != end1_p->next;
			     gs_p = nxtbmnote(gs_p, start1_p, end1_p->next)) {
				gs_p->stemlen = 0.0;
			}
			return;
		}

		/*
		 * User provided outer stem lengths; calculate b0 and b1.
		 * First get Y coords of endpoints of first and last stems.
		 */
		first_p->stemlen *= Staffscale;
		last_p->stemlen *= Staffscale;
		firsty = first_p->stemdir == UP ?
			first_p->notelist[0].c[AY] + first_p->stemlen :
			first_p->notelist[ first_p->nnotes - 1 ].c[AY]
				- first_p->stemlen;
		lasty = last_p->stemdir == UP ?
			last_p->notelist[0].c[AY] + last_p->stemlen :
			last_p->notelist[ last_p->nnotes - 1 ].c[AY]
				- last_p->stemlen;
		/*
		 * If first and last are opposite, adjust the right end of
		 * the line.
		 */
		if (first_p->stemdir != last_p->stemdir)
			lasty += end_bm_offset(start1_p, last_p, 8);

		/* get X coords; calculate b0 and b1 */
		firstx = first_p->c[AX] + stemshift *
				(first_p->stemdir == DOWN ? -1 : 1);
		lastx = last_p->c[AX] + stemshift *
				(last_p->stemdir == DOWN ? -1 : 1);
		b1 = (lasty - firsty) / (lastx - firstx); /* slope */
		b0 = firsty - b1 * firstx;		  /* y intercept */
	}


	/*
	 * At this point we know the equation for the beams.  Figure out and
	 * set the correct stem lengths for all of these beamed groups.
	 */
	if (topdir == botdir) {		/* all stems have the same direction */
		if (first_p->stemdir == DOWN)
			stemshift = -stemshift;

		/* loop through the top staff's groups */
		for (gs_p = FIRSTCSB(start1_p); gs_p != 0; gs_p=nextcsb(gs_p)){
			x = gs_p->c[AX] + stemshift;

			/* first set stemlen to beam's Y coord minus note's */
			gs_p->stemlen = (b0 + b1 * x) - BNOTE(gs_p).c[AY];

			/* if stems are down, reverse it */
			if (gs_p->stemdir == DOWN)
				gs_p->stemlen = -(gs_p->stemlen);

			finalstemadjust(gs_p);
		}
		/* loop through the bottom staff's groups */
		for (gs_p = FIRSTCSB(start2_p); gs_p != 0; gs_p=nextcsb(gs_p)){
			x = gs_p->c[AX] + stemshift;

			/* first set stemlen to beam's Y coord minus note's */
			gs_p->stemlen = (b0 + b1 * x) - BNOTE(gs_p).c[AY];

			/* if stems are down, reverse it */
			if (gs_p->stemdir == DOWN)
				gs_p->stemlen = -(gs_p->stemlen);

			/* if negative (note on wrong side of beam), error */
			if (gs_p->stemlen < 0) {
				l_ufatal(gs_p->inputfile, gs_p->inputlineno,
					"stem length was forced negative");
			}

			finalstemadjust(gs_p);
		}

		/* adjust rest positions on the beamside staff */
		embedrest(first_p, start1_p, start2_p, b1, b0);

	} else {	/* topdir != botdir; some stems have different dir */

		struct GRPSYL *prev_p;		/* previous CSB group */
		struct GRPSYL *firstsub_p;	/* first group of a subbeam */
		struct GRPSYL *lastsub_p;	/* last group of a subbeam */
		struct GRPSYL *sub_p;		/* a group in a subbeam */
		int minbeams;			/* no. of beams all share */
		int beams;			/* no. of beams of a group */
		int slowbasic;			/* slowest basictime in CSB */
		int fastbasic;			/* fastest basictime in CSB */
		int basic;			/* a basictime value */
		float bhigh;			/* height of beams */
		float extra;		/* amount to lengthen all stems by */


		/*
		 * Find the minimum number of beams of the groups in the CSB
		 * set.  That will be the number of beams that they all share.
		 */
		minbeams = 999;		/* way more than there could ever be */
		for (gs_p = first_p; gs_p != end1_p->next;
				gs_p = nxtbmnote(gs_p, start1_p, end1_p->next)){
			beams = drmo(gs_p->basictime) - 2;
			if (beams < minbeams)
				minbeams = beams;
		}

		/*
		 * Find height of all the beams: the distance between the
		 * centers of the outer beams.  This should agree with 
		 * the numbers in prntdata.c.
		 */
		bhigh = (minbeams - 1) * Staffscale *
			(first_p->grpsize == GS_NORMAL ? FLAGSEP : 4.0 * POINT);

		/*
		 * Change the y intercept such that the first stem is lengthened
		 * by half of this height.  The line is at the outer beam, from
		 * the perspective of the first group.
		 */
		b0 += first_p->stemdir == UP ? bhigh / 2.0 : -bhigh / 2.0;

		/*
		 * First set stem lengths to reach the line of the main beam.
		 * At this point, we don't yet include the distance between the
		 * notes of multinote groups.  While we're at it, find the
		 * slowest basictime of any group in the CSB set.
		 * Also find the fastest basictime.
		 */
		slowbasic = 1024;	/* faster than any could be */
		fastbasic = 8;		/* slowest that any could be */
		/* loop through the top staff's groups: all stems down */
		for (gs_p = FIRSTCSB(start1_p); gs_p != 0; gs_p=nextcsb(gs_p)){
			x = gs_p->c[AX] - stemshift;

			/* first set stemlen to note's Y coord minus beam's */
			gs_p->stemlen = gs_p->notelist[ gs_p->nnotes - 1 ].
					c[AY] - (b0 + b1 * x);

			slowbasic = MIN(slowbasic, gs_p->basictime);
			fastbasic = MAX(fastbasic, gs_p->basictime);
		}
		/* loop through the bottom staff's groups; all stems up */
		for (gs_p = FIRSTCSB(start2_p); gs_p != 0; gs_p=nextcsb(gs_p)){
			x = gs_p->c[AX] + stemshift;

			/* first set stemlen to beam's Y coord minus note's */
			gs_p->stemlen = (b0 + b1 * x) - gs_p->notelist[0].c[AY];

			slowbasic = MIN(slowbasic, gs_p->basictime);
			fastbasic = MAX(fastbasic, gs_p->basictime);
		}

		/*
		 * Find the minimum number of beams (based on the slowest
		 * basictime) and subtract 1 to find the number of additional
		 * beams that all groups share beyond the first beam.  Multiply
		 * by the distance the centers of neighboring beams.
		 */
		extra = ((drmo(slowbasic) - 2) - 1) * Staffscale *
			(first_p->grpsize == GS_NORMAL ? FLAGSEP : 4.0 * POINT);

		/*
		 * For each group with stemdir opposite to that of the first
		 * group, lengthen its stemlen by that amount.
		 */
		for (gs_p = first_p; gs_p != end1_p->next; gs_p =
				nxtbmnote(gs_p, start1_p, end1_p->next)) {

			if (gs_p->stemdir != first_p->stemdir)
				gs_p->stemlen += extra;
		}

		/*
		 * Loop for each basictime being used that is shorter than the
		 * longest one; that is, for each level of subbeam that is
		 * needed anywhere.
		 */
		for (basic = slowbasic * 2; basic <= fastbasic; basic *= 2) {

			/* loop through all note groups in the CSB */
			for (prev_p = 0, gs_p = first_p;
			     gs_p != end1_p->next;
			     prev_p = gs_p, gs_p = nxtbmnote(gs_p, start1_p,
					end1_p->next)) {
				/*
				 * If this group has at least as fast a basic-
				 * time as the one we're now dealing with, and
				 * the previous group doesn't (or there is no
				 * previous group), a new subbeam must begin
				 * here (or it could be just a partial beam).
				 * If not, "continue" here.
				 */
				if (gs_p->basictime < basic || (gs_p != first_p
						&& prev_p->basictime >= basic)){
					continue;
				}

				/* point at the start of this subbeam */
				firstsub_p = gs_p;

				/*
				 * Set lastsub_p to right end of the subbeam,
				 * the group right before the basictime becomes
				 * slower than the level we are dealing with.
				 */
				for (lastsub_p = sub_p = firstsub_p; sub_p !=
				     end1_p->next; sub_p = nxtbmnote(sub_p,
				     start1_p, end1_p->next)) {

					if (sub_p == 0 ||
					    sub_p->basictime < basic) {
						break;
					}
					lastsub_p = sub_p;
				}

				/*
				 * Loop through subbeam, lengthening the stems
				 * of all the note groups whose stem direction
				 * is opposite to the first group's.  Lengthen
				 * them enough for one more beam.
				 */
				for (sub_p = firstsub_p; sub_p != end1_p->next;
				     sub_p = nxtbmnote(sub_p, start1_p,
				     end1_p->next)) {

					if (sub_p->stemdir != firstsub_p->
							stemdir) {
						sub_p->stemlen +=
						(sub_p->grpsize == GS_NORMAL ?
						FLAGSEP : 4.0 * POINT) *
						Staffscale;
					}

					if (sub_p == lastsub_p) {
						break;
					}
				}
			}
		}

		/* adjust all stems in the CSB */
		for (gs_p = first_p;
		     gs_p != end1_p->next;
		     gs_p = nxtbmnote(gs_p, start1_p, end1_p->next)) {

			/* if negative (note on wrong side of beam), error */
			if (gs_p->stemlen < 0) {
				l_ufatal(gs_p->inputfile, gs_p->inputlineno,
					"stem length was forced negative");
			}

			/* add distance between outer notes of group */
			gs_p->stemlen += (gs_p->notelist[0].stepsup -
			gs_p->notelist[ gs_p->nnotes - 1 ].stepsup) * Stepsize;
		}

	}

	/*
	 * In beamstem.c, setgroupvert() expanded the north and south
	 * boundaries of groups to allow for stems (except for CSB groups) and
	 * "with" items (except for CSB with "with" on stem end). The exceptions
	 * were because in those cases we needed to know the stem lengths and
	 * we didn't yet.  Well, now we know.  So do the job here.
	 *
	 * The extension for the stem is the length of the exterior part of it
	 * minus half the size of the stem side note (about a STEPSIZE), since
	 * the note itself is already included in the group boundary.  Each
	 * "with" item is allowed enough space for its height, or MINWITHHEIGHT,
	 * whichever is greater.  In the print phase, items of height less than
	 * MINWITHHEIGHT will be placed so as to avoid staff lines as much as
	 * possible.
	 */
	for (gs_p = first_p; gs_p != end1_p->next; gs_p = nxtbmnote(gs_p,
			start1_p, end1_p->next)) {
		outstem = gs_p->stemlen
			- (gs_p->notelist[0].c[RY]
			- gs_p->notelist[ gs_p->nnotes - 1 ].c[RY]);
		if (gs_p->stemdir == UP)
			gs_p->c[AN] += outstem - Stepsize;
		else
			gs_p->c[AS] -= outstem - Stepsize;

		/* add width of "with" items that are on the stem end */
		if (gs_p->stemdir == UP) {
			gs_p->c[AN] += withheight(gs_p, PL_ABOVE);
		} else if (gs_p->stemdir == DOWN) {
			gs_p->c[RS] -= withheight(gs_p, PL_BELOW);
		}
	}
}

/*
 * Name:        calcline()
 *
 * Abstract:    Calculate the equation of the line for the beams of a CSB set.
 *
 * Returns:     YES if an equation was calculated, NO if there are no stems.
 *
 * Description: This function uses linear regression to figure out where the
 *		best place to put the beam is, for a CSB set.  Then, based on
 *		whether the stems on the two staffs have the same direction, it
 *		calls the appropriate function to adjust the results of the
 *		linear regression as needed.
 */

static int
calcline(start1_p, end1_p, start2_p, end2_p, first_p, last_p, topdir, botdir,
		b0_p, b1_p)

struct GRPSYL *start1_p;	/* first group in first voice */
struct GRPSYL *start2_p;	/* first group in second voice */
struct GRPSYL *end1_p;		/* last group in first voice */
struct GRPSYL *end2_p;		/* last group in second voice */
struct GRPSYL *first_p;		/* first note group in either voice */
struct GRPSYL *last_p;		/* last note group in either voice */
int topdir, botdir;		/* stem directions of top and bottom voices */
float *b0_p, *b1_p;		/* y intercept and slope to return */

{
	float defstemsteps;	/* default stem length */
	int one_end_forced;	/* is stem len forced on one end only? */
	int slope_forced;	/* is the slope of the beam forced? */
	float forced_slope;	/* slope that the user forced */
	struct GRPSYL *gs_p;	/* loop through the groups in the beamed set */
	float sx, sy;		/* sum of x and y coords of notes */
	float xbar, ybar;	/* average x and y coords of notes */
	float top, bottom;	/* numerator & denominator for finding b1 */
	float temp;		/* scratch variable */
	float b0, b1;		/* y intercept and slope */
	float deflen;		/* default len of a stem, based on basictime */
	int num;		/* number of notes */


	if (fabs(first_p->beamslope - NOBEAMANGLE) < 0.001) {
		slope_forced = NO;
		forced_slope = 0.0;	/* not used, keep lint happy */
	} else {
		slope_forced = YES;
		forced_slope = tan(first_p->beamslope * PI / 180.0);
	}
	one_end_forced = IS_STEMLEN_KNOWN(first_p->stemlen) !=
			 IS_STEMLEN_KNOWN(last_p->stemlen);

	/*
	 * Find how long we'd like stems to be, ignoring for the moment groups
	 * that need to be longer due to multiple beams.
	 */
	/* average default stems lengths of the two voices */
	defstemsteps = (vvpath(start1_p->staffno, start1_p->vno, STEMLEN)->
			stemlen + 
			vvpath(start2_p->staffno, start2_p->vno, STEMLEN)->
			stemlen) / 2.0;
	/* if this is zero, both stemlens must be zero, so no stems */
	if (defstemsteps == 0.0 && ! slope_forced && ( ! one_end_forced ||
			first_p->stemlen == 0.0 || last_p->stemlen == 0.0)) {
		return (NO);
	}
	if (allsmall(start1_p, end1_p) == NO ||
				allsmall(start2_p, end2_p) == NO) {
		/* at least one group has a normal size note */
		deflen = defstemsteps * Stepsize;
	} else {
		/* all groups have all small notes */
		deflen = defstemsteps * SM_STEMFACTOR * Stepsize;
	}

	/*
	 * Use linear regression to find the best-fit line through where the
	 * ends of the stems would be if they were the standard length.  In
	 * setbeam() where a similar thing was done for non-CSB beams, we used
	 * the centers of the notes, which was okay because at this point in
	 * the game we're really just interested in finding the slope.  But
	 * in CSB, sometimes the stems of the two staffs go in opposite
	 * directions, so we really need to consider the ends of the stems.
	 *
	 * In this function, we will always be concerned with the X coord of
	 * the group as a whole (disregarding any notes that are on the "wrong"
	 * side of the stem) but the Y coord of the note of the group that's
	 * nearest to the beam (thus the BNOTE macro).
	 *
 	 * First get sum of x and y coords, to find averages.
	 */
	sx = sy = 0;
	num = 0;
	for (gs_p = FIRSTCSB(start1_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		sx += gs_p->c[AX];
		sy += BNOTE(gs_p).c[AY] + (topdir == UP ? deflen : -deflen);
		num++;			/* count number of notes */
	}
	for (gs_p = FIRSTCSB(start2_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		sx += gs_p->c[AX];
		sy += BNOTE(gs_p).c[AY] + (botdir == UP ? deflen : -deflen);
		num++;			/* count number of notes */
	}

	xbar = sx / num;
	ybar = sy / num;

	/* accumulate numerator & denominator of regression formula for b1 */
	top = bottom = 0;
	for (gs_p = FIRSTCSB(start1_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		temp = gs_p->c[AX] - xbar;
		top += temp * (BNOTE(gs_p).c[AY] +
				(topdir == UP ? deflen : -deflen) - ybar);
		bottom += temp * temp;
	}
	for (gs_p = FIRSTCSB(start2_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		temp = gs_p->c[AX] - xbar;
		top += temp * (BNOTE(gs_p).c[AY] +
				(botdir == UP ? deflen : -deflen) - ybar);
		bottom += temp * temp;
	}

	b1 = top / bottom;		/* slope */
	b0 = ybar - b1 * xbar;		/* y intercept */

	/* equation of regression line:  y = b0 + b1 * x   */

	if (topdir == botdir) {
		samedir(first_p, last_p, start1_p, start2_p, end1_p, &b0, &b1,
				deflen, one_end_forced, slope_forced,
				forced_slope);
	} else {
		oppodir(first_p, last_p, start1_p, start2_p, &b0, &b1, deflen,
				one_end_forced, slope_forced, forced_slope);
	}

	/* return the calculated slope and intercept */
	*b0_p = b0;
	*b1_p = b1;

	return (YES);
}

/*
 * Name:        samedir()
 *
 * Abstract:    Adjust b0 and b1 when stems are all the same direction.
 *
 * Returns:     void
 *
 * Description: This function is used in the case that the stems on the two
 *		staffs of the CSB have the same direction.  It is given the
 *		y intercept and slope of the beam as calculated by linear
 *		regression.  It adjusts these values if need be.  The algorithm
 *		is similar to the one in setbeam() in beamstem.c.  But here we
 *		have to deal with two linked lists of groups, and we don't have
 *		to deal with grace notes or alternations.
 */

static void
samedir(first_p, last_p, start1_p, start2_p, end1_p, b0_p, b1_p, deflen,
		one_end_forced, slope_forced, forced_slope)

struct GRPSYL *first_p, *last_p;	/* first and last note groups in CSB */
struct GRPSYL *start1_p, *start2_p;	/* first groups of 1st & 2nd voices */
struct GRPSYL *end1_p;		/* last group of 1st voice */
float *b0_p, *b1_p;		/* y intercept and slope */
double deflen;			/* default len of a stem, based on group size*/
int one_end_forced;		/* is stem len forced on one end only? */
int slope_forced;		/* is the slope of the beam forced? */
double forced_slope;		/* slope that the user forced */

{
	struct GRPSYL *gs_p;	/* loop through the groups in the beamed set */
	float firstx, lastx;	/* x coord of first & last note (end of stem)*/
	float firsty, lasty;	/* y coord of first & last note (end of stem)*/
	float maxb0, minb0;	/* max and min y intercepts */
	float stemshift;	/* x distance of stem from center of note */
	float b0, b1;		/* working copy of y intercept and slope */
	float temp;		/* temp variable */
	float shortdist;	/* amount of stem shortening allowed (inches)*/
	int bf;			/* number of beams/flags */
	int shortest;		/* basictime of shortest note in group */


	/* set working copies from the original values */
	b0 = *b0_p;
	b1 = *b1_p;

	/*
	 * Find half the width of a note head; the stems will need to be
	 * shifted by that amount from the center of the notes so that they
	 * will meet the edge of the notes properly.  If the stems are up,
	 * they will be on the right side of (normal) notes, else left.  Set
	 * the X positions for the first and last stems.
	 */
	stemshift = getstemshift(first_p);
	if (first_p->stemdir == DOWN)
		stemshift = -stemshift;
	firstx = first_p->c[AX] + stemshift;	/* first group's stem */
	lastx = last_p->c[AX] + stemshift;	/* last group's stem */

	/*
	 * The original line derived by linear regression must be adjusted in
	 * certain ways.  First, override it if the user wants that; otherwise
	 * adjust according to the beamslope parameter.
	 */
	if (slope_forced) {
		b1 = forced_slope;
	} else {
		b1 = adjslope(start1_p, b1, NO, BEAMSLOPE);
	}

	/*
	 * Calculate a new y intercept (b0).  First pass parallel lines
	 * through each note, and record the maximum and minimum y intercepts
	 * that result.
	 */
	b0 = BNOTE(first_p).c[AY] - b1 * first_p->c[AX];
	maxb0 = minb0 = b0;		/* init to value for first note */
	/* look at rest of them on each of the two staffs */
	for (gs_p = FIRSTCSB(start1_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		b0 = BNOTE(gs_p).c[AY] - b1 * gs_p->c[AX];
		if (b0 > maxb0)
			maxb0 = b0;
		else if (b0 < minb0)
			minb0 = b0;
	}
	for (gs_p = FIRSTCSB(start2_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		b0 = BNOTE(gs_p).c[AY] - b1 * gs_p->c[AX];
		if (b0 > maxb0)
			maxb0 = b0;
		else if (b0 < minb0)
			minb0 = b0;
	}

	/*
	 * Find the basictime of the shortest note in the CSB set, considering
	 * also any slashes on it.  Then update the default stem length based
	 * on that.
	 */
	shortest = 0;
	for (gs_p = first_p; gs_p != end1_p->next; gs_p = nxtbmnote(gs_p,
			start1_p, end1_p->next)) {
		bf = drmo(gs_p->basictime) - 2;	/* no. of beams/flags */
		bf += abs(gs_p->slash_alt);	/* slashes */
		if (bf > shortest)
			shortest = bf;
	}

	if (shortest > 2) {
		/* don't use "==" due to floating point roundoff error */
		if (deflen > 6 * Stepsize) {
			/* at least one group has a normal size note */
			deflen += (shortest - 2) * Flagsep;
		} else {
			/* all groups have all small notes */
			deflen += (shortest - 2) * 4.0 * POINT * Staffscale;
		}
	}

	/*
	 * The outer edge of the beam should be deflen steps away from the
	 * average position of the notes, as defined by the linear regression
	 * line.  But don't allow any note to be closer than a certain number
	 * of steps less than that, the number as given by the stemshorten parm.
	 * We use the average of the two stemshorten values for the two voices.
	 */
	shortdist = (vvpath(start1_p->staffno, start1_p->vno, BEAMSHORT)
			->beamshort +
		     vvpath(start2_p->staffno, start2_p->vno, BEAMSHORT)
			->beamshort) / 2.0 * Stepsize;
	if (first_p->stemdir == UP) {
		if (maxb0 - minb0 > shortdist)
			b0 = maxb0 + deflen - shortdist;
		else
			b0 += deflen;
	} else { /* DOWN */
		if (maxb0 - minb0 > shortdist)
			b0 = minb0 - deflen + shortdist;
		else
			b0 -= deflen;
	}

	for (gs_p = first_p; gs_p != end1_p->next; gs_p = nxtbmnote(gs_p,
			start1_p, end1_p->next)) {
		/*
		 * In certain cases where there are accidentals, we need to
		 * move the beam farther away, to avoid collisions.
		 */
		if (gs_p == first_p) {
			continue;	/* accs are left of beam; no problem */
		}

		bf = drmo(gs_p->basictime) - 2;	/* no. of beams/flags */
		bf += abs(gs_p->slash_alt);	/* slashes */

		b0 = acc_beam(gs_p, b1, b0, AY, bf);
	}

	firsty = b0 + b1 * firstx;	/* y coord near left end of beam */
	lasty = b0 + b1 * lastx;	/* y coord near right end of beam */

	/*
	 * At this point, like setbeam(), we could force the stems of notes
	 * that are pointing to the center of their staffs to reach that center
	 * line.  But it's questionable whether that should be done in cross
	 * staff beaming situations.  We choose not to.
	 */

	/*
	 * If y at the ends of the beam differs by less than a step (allowing a
	 * fudge factor for roundoff error), force the beam horizontal by
	 * setting one end farther away from the notes.  But don't do it if the
	 * user is forcing a particular slope.
	 */
	if ( ! slope_forced && fabs(firsty - lasty) < Stepsize - 0.001) {
		if (first_p->stemdir == UP) {
			if (firsty > lasty) {
				lasty = firsty;
			} else {
				firsty = lasty;
			}
		} else {	/* DOWN */
			if (firsty < lasty) {
				lasty = firsty;
			} else {
				firsty = lasty;
			}
		}
	}

	/* recalculate slope and y intercept from (possibly) new endpoints */
	b1 = (lasty - firsty) / (lastx - firstx);	/* slope */
	b0 = firsty - b1 * firstx;			/* y intercept */

	/*
	 * At this point, like setbeam(), we could do the equivalent of
	 * embedgrace() and avoidothervoice().  But those functions themselves
	 * wouldn't work here as they are, and/or we don't have the necessary
	 * info handy for calling them.  These problems are fairly rare, on top
	 * of cross staff beaming already being fairly rare.  If something
	 * collides, the user can always manually set the stem lengths.
	 */

	/*
	 * If one end's stem len was forced but not the other, now is the time
	 * to apply that forcing.  So in effect, we have taken the beam as
	 * determined by the normal algorithm and now we change the vertical
	 * coord of this end.  If the slope was also forced, move the other
	 * end by the same amount so that the slope won't change.
	 */
	if (one_end_forced) {
		if (IS_STEMLEN_KNOWN(first_p->stemlen)) {
			first_p->stemlen *= Staffscale;
			temp = firsty;
			firsty = BNOTE(first_p).c[AY] + first_p->stemlen *
					(first_p->stemdir == UP ? 1.0 : -1.0);
			if (slope_forced) {
				lasty += firsty - temp;
			}
		} else {
			last_p->stemlen *= Staffscale;
			temp = lasty;
			lasty = BNOTE(last_p).c[AY] + last_p->stemlen *
					(last_p->stemdir == UP ? 1.0 : -1.0);
			if (slope_forced) {
				firsty += lasty - temp;
			}
		}

		/* recalculate */
		b1 = (lasty - firsty) / (lastx - firstx); /* slope */
		b0 = firsty - b1 * firstx;		/* y intercept */
	}

	/* send back the newly calculated values */
	*b0_p = b0;
	*b1_p = b1;
}

/*
 * Name:        oppodir()
 *
 * Abstract:    Adjust b0 and b1 when stems are in opposite directions.
 *
 * Returns:     void
 *
 * Description: This function is used in the case that the stems on the two
 *		staffs of the CSB all have opposite directions.  It is given
 *		the y intercept and slope of the beam as calculated by linear
 *		regression.  It adjusts these values if need be.
 */

static void
oppodir(first_p, last_p, start1_p, start2_p, b0_p, b1_p, deflen,
		one_end_forced, slope_forced, forced_slope)

struct GRPSYL *first_p, *last_p;	/* first and last note groups in CSB */
struct GRPSYL *start1_p, *start2_p;	/* first groups of 1st & 2nd voices */
float *b0_p, *b1_p;		/* y intercept and slope */
double deflen;			/* default len of a stem, based on group size*/
int one_end_forced;		/* is stem len forced on one end only? */
int slope_forced;		/* is the slope of the beam forced? */
double forced_slope;		/* slope that the user forced */

{
	struct GRPSYL *gs_p;	/* loop through the groups in the beamed set */
	float firstx, lastx;	/* x coord of first & last note (end of stem)*/
	float firsty, lasty;	/* y coord of first & last note (end of stem)*/
	float maxb0, minb0;	/* max and min y intercepts */
	float stemshift;	/* x distance of stem from center of note */
	float b0, b1;		/* working copy of y intercept and slope */
	float temp;		/* temp variable */


	/* set working copies from the original values */
	b0 = *b0_p;
	b1 = *b1_p;

	/*
	 * Find half the width of a note head; the stems will need to be
	 * shifted by that amount from the center of the notes so that they
	 * will meet the edge of the notes properly.  If the stems are up,
	 * they will be on the right side of (normal) notes, else left.  Set
	 * the X positions for the first and last stems.
	 */
	stemshift = getstemshift(first_p);
	if (first_p->stemdir == DOWN)
		stemshift = -stemshift;
	firstx = first_p->c[AX] + stemshift;	/* first group's stem */
	lastx = last_p->c[AX] + stemshift;	/* last group's stem */

	/*
	 * The original line derived by linear regression must be adjusted in
	 * certain ways.  First, override it if the user wants that; otherwise
	 * adjust according to the beamslope parameter.
	 */
	if (slope_forced) {
		b1 = forced_slope;
	} else {
		b1 = adjslope(start1_p, b1, YES, BEAMSLOPE);
	}

	/*
	 * Calculate a new y intercept (b0).  First pass parallel lines
	 * through each note, and record the minimum y intercept for the top
	 * staff and the maximum for the bottom staff that result.
	 */
	minb0 = 1000.0;		/* init way positive */
	/* look at rest of them on each of the two staffs */
	for (gs_p = FIRSTCSB(start1_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		b0 = BNOTE(gs_p).c[AY] - b1 * gs_p->c[AX];
		if (b0 < minb0)
			minb0 = b0;
	}
	maxb0 = -1000.0;	/* init way negative */
	for (gs_p = FIRSTCSB(start2_p); gs_p != 0; gs_p = nextcsb(gs_p)) {
		b0 = BNOTE(gs_p).c[AY] - b1 * gs_p->c[AX];
		if (b0 > maxb0)
			maxb0 = b0;
	}

	/*
	 * Make the y intercept be the average of these.  That means the top
	 * staff's shortest stem will be equal in length to the bottom staff's.
	 */
	b0 = (maxb0 + minb0) / 2.0;

	firsty = b0 + b1 * firstx;	/* y coord near left end of beam */
	lasty = b0 + b1 * lastx;	/* y coord near right end of beam */

	/*
	 * If y at the ends of the beam differs by less than a step (allowing a
	 * fudge factor for roundoff error), force the beam horizontal,
	 * averaging the two values.
	 */
	if ( ! slope_forced && fabs(firsty - lasty) < Stepsize - 0.001) {
		lasty = (firsty + lasty) / 2.;
		firsty = lasty;
	}

	/* recalculate slope and y intercept from (possibly) new endpoints */
	b1 = (lasty - firsty) / (lastx - firstx);	/* slope */
	b0 = firsty - b1 * firstx;			/* y intercept */

	/*
	 * If one end's stem len was forced but not the other, now is the time
	 * to apply that forcing.  So in effect, we have taken the beam as
	 * determined by the normal algorithm and now we change the vertical
	 * coord of this end.  If the slope was also forced, move the other
	 * end by the same amount so that the slope won't change.
	 */
	if (one_end_forced) {
		if (IS_STEMLEN_KNOWN(first_p->stemlen)) {
			first_p->stemlen *= Staffscale;
			temp = firsty;
			firsty = BNOTE(first_p).c[AY] + first_p->stemlen *
					(first_p->stemdir == UP ? 1.0 : -1.0);
			if (slope_forced) {
				lasty += firsty - temp;
			}
		} else {
			last_p->stemlen *= Staffscale;
			temp = lasty;
			lasty = BNOTE(last_p).c[AY] + last_p->stemlen *
					(last_p->stemdir == UP ? 1.0 : -1.0);
			if (slope_forced) {
				firsty += lasty - temp;
			}
		}

		/* recalculate */
		b1 = (lasty - firsty) / (lastx - firstx); /* slope */
		b0 = firsty - b1 * firstx;		/* y intercept */
	}

	/* send back the newly calculated values */
	*b0_p = b0;
	*b1_p = b1;
}

/*
 * Name:        nextcsb()
 *
 * Abstract:    Find the next note group on this staff in this CSB.
 *
 * Returns:     pointer to next note group in CSB on this staff, 0 if none
 *
 * Description: This function looks for the next group on this staff that is
 *		still in this CSB set (therefore nongrace), and contains notes
 *		(not a space).
 */

static struct GRPSYL *
nextcsb(gs_p)

struct GRPSYL *gs_p;		/* current group, must be in a CSB */

{
	/* if we are already at the last group in the set, no next group */
	if (gs_p->beamloc == ENDITEM)
		return (0);

	/* loop forward, considering only nongrace groups */
	for (gs_p = nextnongrace(gs_p); gs_p != 0; gs_p = nextnongrace(gs_p)) {
		/* if we find a note group, return it */
		if (gs_p->grpcont == GC_NOTES)
			return (gs_p);
		/* must be a space (rests not allowed); if enditem, give up */
		if (gs_p->beamloc == ENDITEM)
			return (0);
	}

	return (0);	/* hit the end of the measure (shouldn't happen) */
}

/*
 * Name:        nxtbmnote()
 *
 * Abstract:    Find the next note group in this CSB (this staff or the other).
 *
 * Returns:     pointer to next note group in CSB, endnext_p if none
 *
 * Description: This function looks for the next group that is still in this
 *		CSB set (therefore nongrace), and contains notes (not a space
 *		or a rest), whichever staff it may be on.
 */

static struct GRPSYL *
nxtbmnote(gs_p, first_p, endnext_p)

struct GRPSYL *gs_p;		/* current group, must be in a CSB */
struct GRPSYL *first_p;		/* first group in top staff of the CSB */
struct GRPSYL *endnext_p;	/* what to return if we hit the end */

{
	/*
	 * Keep finding the next nonspace group, until we hit the end or we
	 * find one that is not a rest.
	 */
	do {
		gs_p = nextbmgrp(gs_p, first_p, endnext_p);
	} while (gs_p != endnext_p && gs_p->grpcont != GC_NOTES);
	return (gs_p);
}

/*
 * Name:        measnumdelta()
 *
 * Abstract:    Given a bar line, return what to add to measure number.
 *
 * Returns:     the amount to add
 *
 * Description: Usually, at each bar line, 1 should be added to the measure
 *		number.  But there are special cases.  This function returns
 *		what should be added.
 */
static int
measnumdelta(mll_p)

struct MAINLL *mll_p;		/* of a bar line */

{
	struct MAINLL *mll2_p;


	/* find the last staff preceding this bar */
	for (mll2_p = mll_p; mll2_p != 0 && mll2_p->str != S_STAFF;
			mll2_p = mll2_p->prev) {
		;
	}
	if (mll2_p == 0) {
		pfatal("no staffs in measure");
	}

	/* if multirest, return the number of measures it has */
	if (mll2_p->u.staff_p->groups_p[0]->is_multirest) {
		return (-mll2_p->u.staff_p->groups_p[0]->basictime);
	}

	/* for these, nothing should be added */
	if (mll_p->u.bar_p->bartype == INVISBAR ||
	    mll_p->u.bar_p->bartype == RESTART) {
		return (0);
	}

	/* the normal case */
	return (1);
}
