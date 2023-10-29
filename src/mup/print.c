
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

/* The file contains the more general functions that make the final pass
 * through the Mup main list, writing out the PostScript output,
 * for printing the music. More specialized functions are in other files.
 * In particular, functions that are associated with printing
 * things from S_STAFF structs are mostly in prntdata.c.
 * Tablature is printed via printtab.c.
 * Additional print functions are in prntmisc.c and utils.c.
 */

#include <time.h>
#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"


/* print only if flag is turned on. This allows printing selected pages */
static int Printflag = YES;
#define OUTP(x)	if (Printflag==YES){(void)printf x;}
#define OUTPCH(x) if (Printflag == YES){putchar x;}


/* the PostScript commands */
#define O_FONT		(1)
#define O_SIZE		(2)
#define O_LINEWIDTH	(3)
#define O_CURVETO	(4)
#define O_DOTTED	(5)
#define O_SHOWPAGE	(6)
#define O_SHOW 		(7)
#define O_STAFF		(8)
#define O_SETFONT	(9)
#define O_MOVETO	(10)
#define O_LINE		(11)		/* lineto stroke */
#define O_BRACE		(12)
#define O_BRACKET	(13)
#define O_ENDDOTTED	(14)
#define O_WAVY		(15)
#define O_DASHED	(16)
#define O_SAVE		(17)
#define O_RESTORE	(18)
#define O_FILL		(19)
#define O_LINETO	(20)
#define O_STROKE	(21)
#define O_NEWPATH	(22)
#define O_CLOSEPATH	(23)
#define O_GSAVE		(24)
#define O_GRESTORE	(25)
#define O_CONCAT	(26)
#define O_ARC		(27)
#define O_EOFILL	(28)
#define O_SCALE		(29)
#define O_TRANSLATE	(30)
#define O_ROTATE	(31)
#define O_WIDTHSHOW	(32)
#define O_ROLL		(33)
#define O_REPEATBRACKET	(34)

#ifdef __TURBOC__
#define SMALLMEMORY 1
#endif

/*
 * For debugging, it can be useful to display the "bounding box"
 * which is stored in the coordinate arrays of various entities.
 * This list tells which entities have coords.
 * These must match the indices in the Bbox_list array.
 */
#define BB_BAR		0
#define BB_CHORD	1
#define BB_FEED		2
#define BB_GRPSYL	3
#define BB_BLOCKHEAD	4
#define BB_NOTE		5
#define BB_STAFF	6
#define BB_STUFF	7
/* DEBUG is a special type used only for temporary debugging,
 * not for the real product. It only does something if
 * if there is a non-zero coord in the Debug_coords array.
 * If, while coding and debugging, it would be nice
 * to show the coordinates of something, you can temporarily make
 * that array bigger (if necessary--only needed if you want to show more than
 * one coordinate), and then fill it in with whatever you want.
 * The things get printed on every page.
 */
#define BB_DEBUG	8
/* Macros to turn on display of a coord type and to check if it is on */
#define BB_SET(x)	Do_bbox |= (1<<x)
#define BB_IS_SET(x)	(Do_bbox & (1<<x))

/* This struct holds information about how to display a coord bounding box.
 * We use the environment variable MUP_BB to turn on this displaying.
 * For now, we hard-code what color/dashing to use. MUP_BB could optionally
 * contain color info some day, but that seems like overkill flexibility.
 */
static struct Bbox {
	char	id;	/* character in $MUP_BB that says to draw these */
	char	red;	/* per cent of this color to use when drawing */
	char	green;	
	char	blue;
	char	dash_on;	/* for setdash */
	char	dash_off;
} Bbox_list[] = {
	{ 'b',	100,	50,	0,	5,	2 },	/* BAR */
	{ 'c',	0,	80,	0,	5,	2 },	/* CHORD */
	{ 'f',	50,	50,	0,	0,	0 },	/* FEED */
	{ 'g',	100,	0,	0,	0,	0 },	/* GRPSYL */
	{ 'h',	0,	80,	50,	0,	0 },	/* headings, etc */
	{ 'n',	0,	0,	100,	0,	0 },	/* NOTE */
	{ 's',	0,	50,	80,	5,	3 },	/* STAFF */
	{ 'u',	100,	0,	100,	5,	2 },	/* STUFF */
	{ 'd',  0,	0,	80,	0,	0 }	/* DEBUG */
};
static short Do_bbox = 0;

#ifdef SMALLMEMORY
/* if memory is scarce, we do each font in a separate save/restore context.
 * Need flag to keep track of whether we are in one of those */
static int Did_save = NO;
#endif

/* for header, to indicate when output file was generated */
static char *Dayofweek[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char *Month[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* This is the PostScript code used for creating extended fonts */
char *MakeExtendedFont =
"/makeExtendedFont {\n\
	/newencoding exch def\n\
	/newfont exch def\n\
	findfont\n\
	dup length dict /newdict exch def\n\
	{ 1 index /FID ne\n\
		{ newdict 3 1 roll put }\n\
		{ pop pop }\n\
		ifelse\n\
	} forall\n\
	newdict /Encoding newencoding put\n\
	newdict /UniqueID known {\n\
		newdict /UniqueID newdict /UniqueID get 1 add put\n\
	} if\n\
	newfont newdict definefont pop\n\
} def\n\n";

static int Pagesprinted = 0;		/* number of pages actually printed */
static int Feednumber;			/* how many pagefeeds we've handled */


static struct STAFF *Last_staff;	/* point to last STAFF we saw, to
					 * save having to back up to it
					 * later to check if it was a
					 * multirest. */

static int Last_linetype = -1;		/* keep track of last type. If same,
					 * this time, no reason to output */
static float Last_staffscale;		/* Change in staffscale also changes
					 * line width, so have to remember it */
static short Doing_dotted = NO;		/* keep track of whether last line
					 * was dotted. If it was, but current
					 * one isn't, need to tell PostScript */
static int Landscape;			/* how much to translate for landscape
					 * mode, or 0 if not in landscape */
static float Flipshift;			/* How far the printed are on the page
					 * has been shifted because of the
					 * flipmargins parameter. This will
					 * only be nonzero if flipmargins is
					 * YES, and even then only on
					 * alternate pages. */
int In_music = YES;			/* YES if musicscale is in effort,
					 * thus NO if in head/foot/top/bot */

/* static functions */
static void init4print P((void));
static void page1setup P((void));
static int use_landscape P((double pgwidth, double pgheight));
static void setup_user_fonts P((void));
static void print_paper_size P((char *format));
static void setup_extended_fonts P((void));
static void pr_line P((struct LINE *line_p, char *fname, int lineno));
static void dr_line P((double x1, double y1, double x2, double y2, int ltype));
static void pr_curve P((struct CURVE *curve_p, char *fname, int lineno));
static void outp_muschar P((double x, double y, int ch, int size, int font,
		double horzscale, int bold));
static void pr_sc_muschar P((double x, double y, int ch, int size, int font,
		double horzscale, int bold));
static void pr_sc_ital_muschar P((double x, double y, int ch,
		int size, int font, double horzscale, int bold));
static void pr_bar P((struct MAINLL *mll_p, double x, int is_pseudobar));
static int pr_bar_range P((struct BAR *bar_p, int topstaff,
		int botstaff, double x, int next_is_restart, int between,
		struct MAINLL *mll_p));
static void draw_bar P((int bartype, int linetype, double x, double y1,
		double y2));
static void pr_repeat_dots P((int bartype, int staff, double x, int between));
static void do_rdots P((double x, double y, double topoffset, double bottomosffset));
static void print_subbar P((struct BAR *bar_p));
static void pr1_subbar P((struct SUBBAR_APPEARANCE *sb_app_p,
		struct SUBBAR_LOC *sb_loc_p));
static double subbar_y P((int line_ref, double offset, int staff));
static void draw_bracketrepeats P((double x, double y1, double y2, int is_start));
static void pr_reh P((struct MAINLL *mll_p));
static void pr_box P((double x, double y, double boxheight, double boxwidth));
static void pr_topbot P((struct BLOCKHEAD *blockhead_p, double y));
static void pr_restarts P((struct MAINLL *mll_p, double y1, double y2,
		int need_vert_line));
static void outint P((int val));
static void pr_wstring P((double x, double y, char *string, int justify,
		double fullwidth, double horzscale, char * fname, int lineno));
static void outstring P((double x, double y, double fullwidth, double horzscale,
		char *string, char * fname, int lineno));
static int begin_string P((int in_string, double horzscale));
static int end_string P((int in_string, double space_adjust, double horzscale));
static void scrunch P((double horzscale));
static void unscrunch P((double horzscale));
static void outop P((int op));
static void pr_headfoot P((struct MAINLL *mll_p));
static void to_next_page P((struct MAINLL *mll_p));
static void pr_print P((struct PRINTDATA *printdata_p, int is_hook_call));
static void export P((struct VAR_EXPORT *export_p));
static void exportc P ((char *name, float *coord_p));
static void export1 P((char *name, float *coord_p, int alias_index, int tag_index));
static int expr_has_invis P((struct EXPR_NODE *expr_p));
static double pr_keysig P((int staffno, int sharps, int naturals, double x,
		int really_print));
static void draw_keysig P((int staffno, int muschar, int symbols, double x,
		double y, int *table, int offset, int skip));
static double pr_timesig P((int staffno, double x, int multnum,
		int really_print));
static double pr_arbitrary_tsig P((int staffno, double x, int really_print));
static double tsjam P((int num));
static int is_tsig_symbol P((char *str));
static void pr_tsnum P((double x, double y, char *str, double jam));
static void draw_circle P((double x, double y, double radius));
static void do_scale P((double xscale, double yscale));
static void pr_font P((int font, int size));
static void prfontname P((int font));
static void split_a_string P((double x, double y, char *string, int justify,
		double fullwidth, double horzscale, char *fname, int lineno));
static void j_outstring P((double x, double y, char *string, int justify,
		double fullwidth, double horzscale, char *fname, int lineno));
static void set_staff_y P((struct MAINLL *main_p));
static void pr_meas_num P((int staffno, double x));
static void setscale P((void));
static void show_coord P((float *coord_p, int index));
static void prep_bbox P((void));
static void show_bounding_boxes P((struct MAINLL *mll_p));
static void start_page P((void));
static void show_the_page P((void));
static void begin_non_music_adj P((void));
static void end_non_music_adj P((void));


/* main function of print phase. Walk through main list,
 * printing things as we go */

void
print_music()

{
	struct MAINLL *mll_p;	/* to walk through list */
	struct FEED *feed_p;


	debug(256, "print_music");
	prep_bbox();

	/* initialize for printing */
	init4print();

	/* walk down the list, printing as we go */
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) NULL;
						mll_p = mll_p->next) {

		{
			/* in debug mode, print out Postscript comments
			 * to make it easier to map output back to input */
			OUTP(("%%  %s\n", stype_name(mll_p->str)));
		}

		/* tell output program what the user input line was */
		/* STAFF structs have lots of things hung off them that
		 * could come from different input lines, so do that
		 * separately */
		if (mll_p->str != S_STAFF && mll_p->inputlineno > 0) {
			pr_linenum(mll_p->inputfile, mll_p->inputlineno);
		}

		/* call appropriate function(s) based on type */
		switch(mll_p->str) {

		case S_SSV:
			/* Assign the values from the SSV */
			asgnssv(mll_p->u.ssv_p);
			break;

		case S_STAFF:
			OUTP(("%% staff %d\n", mll_p->u.staff_p->staffno));
			outop(O_SAVE);
			set_staffscale(mll_p->u.staff_p->staffno);
			pr_staff(mll_p);
			outop(O_RESTORE);
			Curr_font = FONT_UNKNOWN;
			Curr_size = DFLT_SIZE;
			Last_staff = mll_p->u.staff_p;
			break;

		case S_BAR:
			pr_bar(mll_p, (double)mll_p->u.bar_p->c[AX],
								NO);
			/* reset the staffscale to its scorewide
			 * default value */
			set_staffscale(0);
			break;

		case S_CHHEAD:
			/* nothing to do here--we print notes when we
			 * hit the S_STAFF structs */
			break;

		case S_PRHEAD:
			pr_print(mll_p->u.prhead_p->printdata_p, NO);
			break;

		case S_LINE:
			pr_line(mll_p->u.line_p, mll_p->inputfile,
							mll_p->inputlineno);
			break;

		case S_CURVE:
			pr_curve(mll_p->u.curve_p, mll_p->inputfile,
							mll_p->inputlineno);
			break;

		case S_FEED:
			pr_feed(mll_p);
			pr_endings(mll_p);
			break;

		case S_CLEFSIG:
			(void) pr_clefsig(mll_p, mll_p->u.clefsig_p, YES);
			/* have to do rehearsal marks, because we only
			 * print a pseudo-bar if it is visible */
			if (mll_p->u.clefsig_p->bar_p != (struct BAR *) 0) {
				pr_reh(mll_p);
			}
			break;

		case S_BLOCKHEAD:
			if (mll_p->prev == 0 || mll_p->prev->str != S_FEED) {
				pfatal("blockhead without preceding feed");
			}
			feed_p = mll_p->prev->u.feed_p;
			set_win_coord(mll_p->u.blockhead_p->c);
			set_win(feed_p->c[AN], feed_p->c[AS],
					feed_p->c[AE], feed_p->c[AW]);
			set_cur(mll_p->prev->u.feed_p->c[AW],
					mll_p->prev->u.feed_p->c[AN]);
			pr_print(mll_p->u.blockhead_p->printdata_p, NO);
			set_win_coord(0);
			break;
		default:
			pfatal("unknown item in main list");
			break;
		}
	}

	/* do grid atend things if necessary */
	if (Atend_info.grids_used > 0) {
		if (Atend_info.separate_page == YES) {
			/* The only MUP_BB thing that matters on grids at end
			 * page is header/footer, and because of the order
			 * in which things are done in pr_atend()
			 * (i.e., when top/bottom are set relative to when the
			 * MUP_BB printing code is called),
			 * it's hard to find a way
			 * to make that work without breaking something else.
			 * So since grids at end is such a rare case,
			 * and MUP_BB is just for debugging,
			 * we just turn it off. */
			Do_bbox = 0;
		}
		pr_atend();
	}

	/* do final stuff for last page */
	pr_headfoot(Mainlltc_p);
}


/* do the things for starting a new page */
void
newpage(mll_p)

struct MAINLL *mll_p;

{
	pr_headfoot(mll_p);
	Pagenum++;
	to_next_page(mll_p);
}


/* print final trailer */

void
trailer()

{
	int f;			/* font index */

	if (PostScript_hooks[PU_BEFORETRAILER] != 0) {
		pr_print(PostScript_hooks[PU_BEFORETRAILER], YES);
	}

	Printflag = YES;
	printf("%%%%Trailer\n");
	printf("%%%%DocumentFonts: ");
	for (f = 1; f < MAXFONTS; f++) {
		if (Fontinfo[font_index(f)].was_used == YES) {
			prfontname(f);
		}
	}
	
	printf("\n%%%%Pages: %d\n", Score.panelsperpage == 1 ? Pagesprinted :
				((Pagesprinted + 1) / 2) );
}


/* initialize things for print pass through main list */

static void
init4print()

{
	struct tm *timeinfo_p;
	time_t clockinfo;
	struct MAINLL *mll_p;
	static int first_time = YES;

	if (first_time == NO) {
		page1setup();
		return;
	}
	first_time = NO;

	/* initialize the SSV data */
	initstructs();

	printf("%%!PS-Adobe-1.0\n");
	printf("%%%%Creator: Mup (Version 7.1)\n");
	printf("%%%%Title: music: %s from %s\n", Outfilename, Curr_filename);
	clockinfo = time((time_t *)0);
	timeinfo_p = localtime(&clockinfo);
	printf("%%%%CreationDate: %s %s %d %d:%02d:%02d %d\n",
			Dayofweek[timeinfo_p->tm_wday],
			Month[timeinfo_p->tm_mon], timeinfo_p->tm_mday,
			timeinfo_p->tm_hour, timeinfo_p->tm_min,
			timeinfo_p->tm_sec, 1900 + timeinfo_p->tm_year);
	printf("%%%%Pages: (atend)\n");
	printf("%%%%DocumentFonts: (atend)\n");
	/* we need to know the value of panelsperpage before setting up the
	 * first page, as well as the pagewidth and pageheight,
	 * so need to peek into main list up till the first non-SSV
	 * to get that. */
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) NULL;
						mll_p = mll_p->next) {
		if (mll_p->str == S_SSV) {
			asgnssv(mll_p->u.ssv_p);
		}
		else {
			/* as soon as we hit something other than SSV,
			 * we're past any page size changes */
			break;
		}
	}
	print_paper_size("%%%%BoundingBox: 0 0 %d %d\n");
	/* DocumentMedia  is actually for PostScript 3.0,
	 * whereas Mup tries to be backward compatible all the way to 1.0,
	 * but it's inside a comment, so shouldn't hurt anything, and might
	 * help. The "Default" is an arbitrary tag, 0 is for "weight",
	 * and the two empty strings for paper color and special feature. */
	print_paper_size("%%%%DocumentMedia: Default %d %d 0 () ()\n");
	printf("%%%%Orientation: %s\n", ((Landscape || Score.panelsperpage == 2)
					? "Landscape" : "Portrait"));
	printf("%%%%EndComments\n");
	ps_prolog();
	printf("/flagsep %.2f 300 mul def\t %% %.2f stepsizes\n",
				FLAGSEP / STEPSIZE, FLAGSEP / STEPSIZE);

	setup_user_fonts();
	setup_extended_fonts();

	/* At least the Mac OS X--and perhaps other--PostScript interpreters
	 * can fail to handle non-letter size paper properly.
	 * The recommended solution seems to be to add a setpagedevice
	 * section to the Prolog. As far as we can tell,
	 * this doesn't seem to hurt anything on systems
	 * that can get along okay without it.
	 */
	print_paper_size("%%%%BeginFeature: *PageSize Default\n<< /PageSize [ %d %d ] >> setpagedevice\n%%%%EndFeature\n");

	printf("%%%%EndProlog\n");

	if (PostScript_hooks[PU_AFTERPROLOG] != 0) {
		pr_print(PostScript_hooks[PU_AFTERPROLOG], YES);
		printf("%%EndAfterPrologHook\n");
	}
	/* init for first page */
	page1setup();
}


/* set thing up to print the first page */

static void
page1setup()

{
	struct MAINLL *mll_p;

	/* Peek into list to see if there is a PostScript atpagebegin hook
	 * and/or atscorebegin before the first BAR. If so, save them. */
	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		if (mll_p->str == S_BAR) {
			break;
		}
		if (mll_p->str == S_PRHEAD) {
			struct PRINTDATA *pr_p;

			for (pr_p = mll_p->u.prhead_p->printdata_p; pr_p != 0;
							pr_p = pr_p->next) {
				if (pr_p->ps_usage == PU_ATPAGEBEGIN) {
					PostScript_hooks[PU_ATPAGEBEGIN] = pr_p;
				}
				if (pr_p->ps_usage == PU_ATSCOREBEGIN) {
					PostScript_hooks[PU_ATSCOREBEGIN] = pr_p;
				}
			}
		}
	}

	Feednumber = 0;

	to_next_page(Mainllhc_p);

	/* Arrange to start at beginning of main list */
	initstructs();

	/* start the cursor at the top left corner of page */
	set_cur(0.0, PGHEIGHT);

	Meas_num = 1;
	Ped_snapshot[0] = NO;
	set_staffscale(0);
}


/* table of standard paper sizes, to be used to see if user specified
 * a landscape version of a standard size */
struct Papersize {
	int	width;
	int	height;
} Paper_size_table[] = { 
	{ 612, 792 },	/* letter */
	{ 540, 720},	/* note */
	{ 612, 1008},	/* legal */
	{ 595, 842},	/* a4 */
	{ 421, 595},	/* a5 */
	{ 297, 421},	/* a6 */
	{ 612, 936},	/* flsa */
	{ 396, 612},	/* halfletter */
	{ 0, 0}
};

/* how many points away from an exact match to consider a match. This is big
 * enough so that user can be off by a little and still get the desired
 * results, yet not so big as to give false matches. */
#ifdef FUZZ
#undef FUZZ
#endif
#define FUZZ 24

/* given a paper size, determine if the paper
 * size appears to be the landscape version of a standard paper size.
 * If so, return the page height in points, otherwise return 0.
 * It return this rather than just a boolean
 *  since page height is needed for translate amount.
 */

static int
use_landscape(pgwidth, pgheight)

double pgwidth;		/* page width in inches */
double pgheight;	/* page height in inches */

{
	int pts_width, pts_height;	/* width and height in points */
	int i;


	/* convert dimension to points */
	pts_width = (int) (pgwidth * PPI);
	pts_height = (int) (pgheight * PPI);

	/* for each paper size table entry, see if by interchanging the
	 * width and height we would end up with something within FUZZ
	 * points of matching a landscape mode paper size */
	for (i = 0; Paper_size_table[i].width != 0; i++) {
		if (pts_width > Paper_size_table[i].height - FUZZ &&
				pts_width < Paper_size_table[i].height + FUZZ &&
				pts_height > Paper_size_table[i].width - FUZZ &&
				pts_height < Paper_size_table[i].width + FUZZ) {
			return(pts_height);
		}
	}

	/* not landscape */
	return(0);
}


/* Given a format string containing two %d items, print that format string,
 * filling in the page width and height in points.
 * Need to round to nearest point; gv won't recognize the size if it is
 * off by even 1 point.
 * This function also set the Landscape value as appropriate.
 */

void
print_paper_size(format)

char *format;

{
	if (Score.panelsperpage == 2) {
		/* Have to compensate for the fact that our page width/height
		 * internally are that of the panel, but here we need the
		 * physical paper size */
		printf(format, (int) (Score.pageheight * PPI + 0.5),
				(int) (Score.pagewidth * 2.0 * PPI + 0.5));
	}
	else if ((Landscape = use_landscape(Score.pagewidth, Score.pageheight))
								!= 0) {
		printf(format, (int) (Score.pageheight * PPI + 0.5),
				(int) (Score.pagewidth * PPI + 0.5));
	}
	else {
		printf(format, (int) (Score.pagewidth * PPI + 0.5),
				(int) (Score.pageheight * PPI + 0.5));
	}
}


/* for any user-defined fonts, if there was any PostScript that needs to
 * be output in order to use the font, output that.
 */

static void
setup_user_fonts()

{
	int f;
	char buffer[BUFSIZ];

	for (f = 0; f < MAXFONTS; f++) {
		if (Fontinfo[f].fontfile != (FILE *) 0) {
			while (fgets(buffer, BUFSIZ, Fontinfo[f].fontfile)
						!= (char *) 0) {
				printf("%s", buffer);
			}
			fclose(Fontinfo[f].fontfile);
		}
	}

	/* If user defined any symbols, output Postscript for those fonts. */
	/* Maybe we could we try to optimize to only do this if they actually
	 * *used* at least one character of the font, but they could have
	 * used it in any arbitrary postscript section.
	 * It is better to define it unnecessarily
	 * than fail to define it when needed.
	 */
	if (Userfonts != 0) {
		int f;
		for (f = 0; f < NUM_SYMFONTS; f++) {
			if (f < NUM_MFONTS) {
				user_overrides(f, &(Userfonts[f]));
			}
			else {
				user_symbols(f, &(Userfonts[f]));
			}
		}
	}
}



/* for each extended character set font that was used somewhere, output
 * the PostScript to get that font set up so that is can be used.
 */

/* We make an array with a slot for each set of character names.
 * Slot 0 is used for the symbol font. The next NUM_EXT_FONT_SETS
 * are used for the extneded fonts, The last 2 are used for
 * the Zapf Dingbats fonts. */
#define FONTS_USED_SLOTS	(NUM_EXT_FONT_SETS + 3)

static void
setup_extended_fonts()

{
	int i;			/* font index */
	int have_extended;	/* YES if extended character set was used
				 * somewhere, and thus we have to output
				 * PostScript to allow using the set */
	int extNused[FONTS_USED_SLOTS];	/* YES for each extended font
				 * that was used.  See definition of
				 * FONTS_USED_SLOTS above for more info. */
	char * suffix;		/* "sym" or "ext" */
	int numsuffix;		/* number to distiguish the extended fonts,
				 * 1, 2, or 3 for the extended font sets,
				 * 1 or 2 for Zapf Dingbats, and 0 for Symbol */
	int e;			/* index through NUM_EXT_FONT_SETS */
	int extfont;		/* font number of an extended font */
	int findex;		/* index into Fontinfo */
	int c;			/* index through characters in the font */


	/* First see if there are any extended characters used at all.
	 * If not, we don't have to do anything more here */
	have_extended = NO;
	/* init to none used */
	for (e = 0; e < FONTS_USED_SLOTS; e++) {
		extNused[e] = NO;
	}
	/* Check which extended font sets were used */
	for (i = FONT_TR; IS_STD_FONT(i); i++) {
		for (e = 1; e <= NUM_EXT_FONT_SETS; e++) {
			extfont = i + e * NUM_STD_FONTS;
			if (Fontinfo[font_index(extfont)].was_used == YES) {
				have_extended = YES;
				extNused[e] = YES;
			}
		}
	}
	/* Check if the symbol and/or Zapf fonts were used */
	if (Fontinfo[font_index(FONT_SYM)].was_used == YES) {
		have_extended = YES;
		extNused[0] = YES;
	}
	if (Fontinfo[font_index(FONT_ZD1)].was_used == YES) {
		have_extended = YES;
		extNused[NUM_EXT_FONT_SETS+1] = YES;
	}
	if (Fontinfo[font_index(FONT_ZD2)].was_used == YES) {
		have_extended = YES;
		extNused[NUM_EXT_FONT_SETS+2] = YES;
	}

	if (have_extended == NO) {
		return;
	}

	/* First output the generic code needed for all extended fonts */
	printf("\n%% Set up extended character set fonts\n");
	(void) printf("%s", MakeExtendedFont);

	/* Make encoding vector for each extended font that was used */
	for (e = 0; e < FONTS_USED_SLOTS; e++) {	
		if (extNused[e] == NO) {
			continue;
		}
		numsuffix = e;
		if (e == 0) {
			suffix = "sym";
			findex = font_index(FONT_SYM);
		}
		else if (e == NUM_EXT_FONT_SETS + 1) {
			suffix = "ZD";
			findex = font_index(FONT_ZD1);
			numsuffix = 1;
		}
		else if (e == NUM_EXT_FONT_SETS + 2) {
			suffix = "ZD";
			findex = font_index(FONT_ZD2);
			numsuffix = 2;
		}
		else {
			suffix = "ext";
			findex = font_index(FONT_TR + e * NUM_STD_FONTS);
		}
		printf("/encoding_%s%d StandardEncoding length array def\n",
							suffix, numsuffix);
		for (c = 0; c < Fontinfo[findex].numchars; c++) {
			printf("encoding_%s%d %d /%s put\n",
					suffix, numsuffix, c + FIRST_CHAR,
					Fontinfo[findex].charnames[c]);
		}
	}

	/* Generate call to makeExtendedFont for each one that was used */
	for (i = FONT_TR; IS_STD_FONT(i); i++) {
		for (e = 1; e <= NUM_EXT_FONT_SETS; e++) {	
			extfont = i + e * NUM_STD_FONTS;
			findex = font_index(extfont);
			if (Fontinfo[findex].was_used == YES) {
				/* arguments are:
				 *	 base_font new_font new_encoding */
				prfontname(i);
				prfontname(extfont);
				printf("encoding_ext%d ", e);
				printf("makeExtendedFont\n");
			}
		}
	}
	if (Fontinfo[font_index(FONT_SYM)].was_used == YES) {
		printf("/Symbol /Symbol encoding_sym0 makeExtendedFont\n");
	}
	if (Fontinfo[font_index(FONT_ZD1)].was_used == YES) {
		printf("/ZapfDingbats /ZapfDingbats1 encoding_ZD1 makeExtendedFont\n");
	}
	if (Fontinfo[font_index(FONT_ZD2)].was_used == YES) {
		printf("/ZapfDingbats /ZapfDingbats2 encoding_ZD2 makeExtendedFont\n");
	}
}


/* given a LINE struct, output commands to draw a line */

static void
pr_line(line_p, fname, lineno)

struct LINE *line_p;	/* info about what kind of line to draw and where */
char *fname;		/* file name for error messages */
int lineno;		/* line number for error messages */

{
	double x1, y1;	/* beginning of line */
	double x2, y2;	/* end of line */
	int uses_page;	/* YES if start or end references _page */

	x1 = inpc_x( &(line_p->start), fname, lineno);
	y1 = inpc_y( &(line_p->start), fname, lineno);
	x2 = inpc_x( &(line_p->end), fname, lineno);
	y2 = inpc_y( &(line_p->end), fname, lineno);

	/* If there is a string associated with the line,
	 * print that first.
	 */
	if (line_p->string != 0) {
		double line_len;	/* length of line in LINE struct */
		double str_x, str_y;	/* where string starts */

		/* First find length of line. */
		line_len = sqrt(SQUARED(x2 - x1) + SQUARED(y2 - y1));
		if (x2 < x1) {
			line_len = -line_len;
		}

		/* For now, pretend the line is horizontal, starting
		 * at (x1,y1). The horizontal middle of the string should then
		 * be at the midpoint of the line, and the left edge of the
		 * string should be half the string width left of that.
		 * The vertical is a STEPSIZE above the line.
		 */
		str_x = (line_len / 2.0) - (strwidth(line_p->string) / 2.0);
		str_y = STEPSIZE + strdescent(line_p->string);

		/* move effective origin of coordinate system to (x1,y1),
		 * then rotate by the appropriate angle and print string.
		 */
		outop(O_GSAVE);
		outcoord(x1);
		outcoord(y1);
		outop(O_TRANSLATE);
		/* calculate angle. If vertical line or nearly so,
		 * avoid division by zero */
		if (fabs(x2 - x1) < .001) {
			outint(90);
		}
		else {
			OUTP(("%.1f ", atan( (y2 - y1) / (x2 - x1) ) * 180.0 / PI));
		}
		
		outop(O_ROTATE);
		pr_string(str_x, str_y, line_p->string, J_LEFT, 0, -1);
		outop(O_GRESTORE);
	}

	/* wavy lines are special case */
	if (line_p->linetype == L_WAVY) {
		draw_wavy(x1, y1, x2, y2);
		return;
	}

	/* Set line width to specified width and type, then draw the line.
	 * But if _page was used, we want to cancel out any effect of
	 * musicscale. This is a bit ugly. We set Last_staffscale to
	 * invalid value to ensure do_linetype will output new value,
	 * then adjust Staffscale by musicscale. Then after drawing,
	 * we put Staffscale back to what it should be, and again set
	 * Last_staffscale invalid, to force the next call to do_linetype
	 * (if any) to output new value.
	 */
	uses_page = NO;
	if (Score.musicscale != DEFMUSICSCALE) {
		if ( (references_page(&(line_p->start)) == YES)
				|| (references_page(&(line_p->end)) == YES) ) {
			uses_page = YES;
			Last_staffscale  = -1.0;
			Staffscale /= Score.musicscale;
		}
	}
	
	do_linetype(line_p->linetype);
	draw_line (x1, y1, x2, y2);
	if (uses_page == YES) {
		Last_staffscale  = -1.0;
		Staffscale *= Score.musicscale;
	}

	/* make sure line type gets set back to solid */
	if (line_p->linetype == L_DASHED || line_p->linetype == L_DOTTED) {
		do_linetype(L_NORMAL);
	}
}


/* generate PostScript command to tell what kind of line to draw */

void
do_linetype(ltype)

int ltype;		/* L_WIDE, L_NORMAL, etc  */

{
	if (Last_linetype == ltype && Last_staffscale == Staffscale) {
		/* same as last time, no need to tell the printer again */
		return;
	}

	/* output command for proper width/type of line */
	switch(ltype) {

	case L_WIDE:
		OUTP(("%4.2f ", W_WIDE * Staffscale));
		outop(O_LINEWIDTH);
		break;

	case L_NORMAL:
		OUTP(("%4.2f ", W_NORMAL * Staffscale));
		outop(O_LINEWIDTH);
		break;

	case L_MEDIUM:
		OUTP(("%4.2f ", W_MEDIUM * Staffscale));
		outop(O_LINEWIDTH);
		break;

	case L_DOTTED:
		OUTP(("%4.2f ", Staffscale));
		outop(O_LINEWIDTH);
		outop(O_DOTTED);
		Doing_dotted = YES;
		break;

	case L_DASHED:
		OUTP(("%4.2f ", Staffscale));
		outop(O_LINEWIDTH);
		outop(O_DASHED);
		Doing_dotted = YES;
		break;

	default:
		pfatal("unknown line type");
		break;
	}

	/* remember current line type */
	Last_linetype = ltype;
	Last_staffscale = Staffscale;

	/* if was doing dotting but not anymore, tell PostScript */
	if (Doing_dotted && (ltype != L_DOTTED) && (ltype != L_DASHED)) {
		Doing_dotted = NO;
		outop(O_ENDDOTTED);
	}
}


/* output commands for drawing a line. Resulting output is:
 *	x1 y1 moveto x2 y2 lineto stroke */

void
draw_line(x1, y1, x2, y2)

double x1, y1;	/* draw line from here */
double x2, y2;	/* to here */

{
	dr_line( (double) x1, (double) y1, (double) x2, (double) y2, O_LINE);
}


/* output commands to draw a line whose width is proportional to the given
 * point size */

void
draw_prop_line(x1, y1, x2, y2, size, ltype)

double x1, y1;	/* draw line from here */
double x2, y2;	/* to here */
int size;	/* make width proportional to this */
int ltype;	/* O_LINE, etc */

{
	/* temporarily change the line width, then draw the line */
	outop(O_GSAVE);
	OUTP(("%.2f ", (double) size * 0.065 * Staffscale));
	outop(O_LINEWIDTH);
	dr_line(x1, y1, x2, y2, O_LINE);
	outop(O_GRESTORE);
}


/* draw a wavy line. Resulting output is:
 *	x1 y1 moveto x2 y2 wavy */

void
draw_wavy(x1, y1, x2, y2)

double x1;
double y1;	/* draw wavy line from x1,y1 */
double x2;
double y2;	/* to x2, y2 */

{
	dr_line((double) x1, (double) y1, (double) x2, (double) y2, O_WAVY);
}


/* actually draw line. Common function for drawing regular or wavy lines */

static void
dr_line(x1, y1, x2, y2, ltype)

double x1;
double y1;	/* draw line from x1,y1 */
double x2;
double y2;	/* to x2,y2 */
int ltype;	/* O_LINE, etc */

{

	/* output coordinates */
	outcoord(x1);
	outcoord(y1);
	outop(O_MOVETO);

	outcoord(x2);
	outcoord(y2);
	outop(ltype);

	/* current location is where line ended */
	set_cur(x2, y2);
}


/* Draw a parallelogram for a slant line, like for beams or a slash */

void
draw_parallelogram(x1, y1, x2, y2, halfwidth)

double x1, y1;		/* start parallelogram here */
double x2, y2;		/* end parallelogram here */
double halfwidth;	/* go this far up and down from y1 and y2 to get
			 * corners of parallelogram */

{
	do_newpath();
	do_moveto(x1, y1 + halfwidth);
	do_line(x2, y2 + halfwidth);
	do_line(x2, y2 - halfwidth);
	do_line(x1, y1 - halfwidth);
	do_closepath();
	do_fill();
}


/* output commands to draw a curve */

static void
pr_curve(curve_p, fname, lineno)

struct CURVE *curve_p;	/* which curve */
char *fname;		/* file name for error messages */
int lineno;		/* line number for error messages */

{
	struct INPCOORD *inpcoord_p;
	int n;
	float *xlist, *ylist;
	float cwid;		/* curve width */
	int taper;		/* YES or NO */
	int uses_page;		/* YES if a coord references _page */


	/* pr_allcurve() expects lists of X and Y coordinates, so
	 * get some space for those lists,  and fill them in.
	 * Call pr_allcurve() to print the curve, then free the lists */
	MALLOCA(float, xlist, curve_p->ncoord);
	MALLOCA(float, ylist, curve_p->ncoord);
	for (n = 0; n < curve_p->ncoord; n++) {
		inpcoord_p = &(curve_p->coordlist[n]);
		xlist[n] = inpc_x(inpcoord_p, fname, lineno);
		ylist[n] = inpc_y(inpcoord_p, fname, lineno);
	}
	switch(curve_p->curvetype) {
	case L_NORMAL:
		cwid = W_NORMAL / PPI;
		taper = YES;
		break;
	case L_MEDIUM:
		cwid = W_MEDIUM / PPI;
		taper = YES;
		break;
	case L_WIDE:
		cwid = W_WIDE / PPI;
		taper = YES;
		break;
	case L_DASHED:
		cwid = 1.0 / PPI;
		taper = NO;
		do_linetype(L_DASHED);
		break;
	case L_DOTTED:
		cwid = 1.0 / PPI;
		taper = NO;
		do_linetype(L_DOTTED);
		break;
	default:
		pfatal("unknown curve type");
		/*NOTREACHED*/
		return;
	}
	/* adjust for current staff scaling */
	cwid *= Staffscale;

	/* If any of the coordinates reference _page,
	 * then cancel out the effect of musicscale, since that is
	 * suppose to only affect the music window, and _page encompasses
	 * more than that. */
	uses_page = NO;
	for (n = 0; n < curve_p->ncoord; n++) {
		if (references_page(&(curve_p->coordlist[n])) == YES) {
			uses_page = YES;
			break;
		}
	}
	if (uses_page == YES) {
		cwid /= Score.musicscale;
	}

	pr_allcurve(xlist, ylist, curve_p->ncoord, cwid, taper);
	FREE(xlist);
	FREE(ylist);
	/* make sure line type gets set back to solid */
	if (curve_p->curvetype == L_DASHED || curve_p->curvetype == L_DOTTED) {
		do_linetype(L_NORMAL);
	}
}


/* functions to do common PostScript things */

void
do_moveto(x, y)

double x;
double y;

{
	outcoord(x);
	outcoord(y);
	outop(O_MOVETO);
}


void
do_line(x, y)

double x;
double y;

{
	outcoord(x);
	outcoord(y);
	outop(O_LINETO);
}

void
do_curveto(x1, y1, x2, y2, x3, y3)

double x1, y1;
double x2, y2;
double x3, y3;

{
	outcoord(x1);
	outcoord(y1);
	outcoord(x2);
	outcoord(y2);
	outcoord(x3);
	outcoord(y3);
	outop(O_CURVETO);
}


void
do_stroke()

{
	outop(O_STROKE);
}

void
do_fill()

{
	outop(O_FILL);
}

void
do_newpath()

{
	outop(O_NEWPATH);
}

void
do_closepath()

{
	outop(O_CLOSEPATH);
}

void
do_rotate(angle)

int angle;

{
	outint(angle);
	outop(O_ROTATE);
}

/* output a PostScript scale command */

static void
do_scale(xscale, yscale)

double xscale, yscale;

{
	OUTP(("%0.6f %0.6f ", xscale, yscale));
	outop(O_SCALE);
}

/* print a white box with the corners given */

void
do_whitebox(x1, y1, x2, y2)

double x1, y1;
double x2, y2;

{
	outcoord(x1);
	outcoord(y1);
	outcoord(x2);
	outcoord(y2);
	OUTP(("whitebox\n"));
}


/* output PostScript to draw a guitar grid */

void
do_grid(x, y, space, grid_p, staff, horzscale)

double x;
double y;
double space;	/* distance between lines of the grid */
struct GRID *grid_p;
int staff;
double horzscale;	/* how much to squeeze horizontally */

{
	int s;
	int fret, fretnum, numvert;
	int topfret;

	if (horzscale != DEFHORZSCALE) {
		/* We want everything to happen in scrunch coord space
		 * except the moveto x value, so compensate that value. */
		x /= horzscale;
		scrunch(horzscale);
	}
	outcoord(x);
	outcoord(y);
	outcoord(space);

	gridinfo(grid_p, staff, &fret, &fretnum, &numvert, &topfret);
	outint(fret);
	outint(fretnum);
	outint(numvert);

	/* the curve ends */
	outint(grid_p->curvel);
	outint(grid_p->curver);

	/* fret value for each string in a PostScript array */
	OUTP(("[ "));
	for (s = 0; s < grid_p->numstr; s++) {
		if (grid_p->positions[s] <= 0) {
			outint(grid_p->positions[s]);
		}
		else {
			outint(grid_p->positions[s] - topfret);
		}
	}
	OUTP(("] grid\n"));
	if (horzscale != DEFHORZSCALE) {
		unscrunch(horzscale);
	}
}


/* output commands for printing one music character */

void
pr_muschar(x, y, ch, size, font)

float x, y;	/* where to print */
int ch;		/* which music character to print */
int size;
int font;	/* FONT_MUSIC*   */

{
	pr_sc_muschar(x, y, ch, size, font, DEFHORZSCALE, NO);
	Fontinfo[font_index(font)].was_used = YES;
}

static void
pr_sc_muschar(x, y, ch, size, font, horzscale, bold)

float x, y;	/* where to print */
int ch;		/* which music character to print */
int size;
int font;	/* FONT_MUSIC*   */
double horzscale;	/* how much to squeeze horizontally */
int bold;	/* YES or NO */

{
	outp_muschar(x, y, ch, size, font, horzscale, bold);

	/* x of music char is in middle, so set current to right edge */
	size = adj_size(size, Staffscale, (char *) 0, -1);
	set_cur(x + width(font, size, ch) * horzscale / 2.0, y);
}


/* Output PostScript to print a music character. Common part for
 * normal and italic versions of the character. */

static void
outp_muschar(x, y, ch, size, font, horzscale, bold)

double x, y;
int ch;
int size;
int font;
double horzscale;	/* how much to squeeze horizontally */
int bold;		/* YES or NO */

{
	double scaling;
	double bold_offset;	/* how much to move overstrikes to make bold */
	int overstrikes;	/* count of overstrikes for bold */
	char *name;	/* name of music character */
	char *prefix;	/* to avoid name collisions */


	if (bold == YES) {
		/* Try to not bleed over too much. We'll make bold about
		 * 4% wider with overstrikes, so scrunch by a similar amount */
		horzscale *= 0.96;
	}

	/* Since the x,y location are arguments
	 * to the character drawing procedure,
	 * we have to apply the horzscale first and then compensate. */
	scrunch(horzscale);

	if (size == DFLT_SIZE) {
		scaling = Staffscale;
	}
	else {
		scaling = (double) size * Staffscale / (double) DFLT_SIZE;
	}
	bold_offset = 0.02 * size * POINT;

	/* Determine the symbolic name of the music character.
	 * For user-defined names, we add a prefix, so they can't
	 * accidentally collide with some other PostScript symbol. */
	prefix = (font >= FONT_USERDEF1 ? "UDS_" : "");
	name = get_charname(ch, font);

	/* For bold we print 5 times: first a bit southwest, then northwest,
	 * then northeast, then southeast, then at home position.
	 * For non-bold we just print once at normal position.
	 */
	for (overstrikes = 0; overstrikes < 5; overstrikes++) {
		if (bold == YES) {
			switch (overstrikes) {
			case 0: x -= bold_offset;
				y -= bold_offset;
				break;
			case 1: y += 2.0 * bold_offset;
				break;
			case 2: x +=  2.0 * bold_offset;
				break;
			case 3: y -= 2.0 * bold_offset;
				break;
			case 4: x -= bold_offset;
				y += bold_offset;
				break;
			}
		}
		outcoord( (double) x / horzscale);
		outcoord( (double) y);
		OUTP(("%f %s%s\n", scaling, prefix, name));
		if (bold == NO) {
			break;
		}
	}

	unscrunch(horzscale);
}


/* Print an italic music character. We do this by constructing an
 * appropriate PostScript transform matrix and then printing the character.
 * The transform matrix takes the rectangle that bounds the character,
 * widens it slightly, and and turns it into a parallelogram
 * slanted by 15 degrees.
 *    ----------          ---------
 *    |        |  -->    /       /
 *    |        |        /       /
 *    ----------        --------
 * But then it is scrunched slightly to somewhat counteract the extra width.
 */

static void
pr_sc_ital_muschar(x, y, ch, size, font, horzscale, bold)

double x, y;	/* where to print */
int ch;		/* which music character to print */
int size;
int font;	/* MUSIC_FONT*  */
double horzscale;	/* how much to squeeze horizontally */
int bold;

{
	float chwidth, chheight;
	float adj;		/* distance top left is moved to get slant */
	float inc;		/* increment on width */
	float a, c;		/* for transform equation  x' = ax + cy + t  */
	int eff_size;


	eff_size = adj_size(size, Staffscale, (char *) 0, -1);
	chheight = height(font, eff_size, ch);
	chwidth = width(font, eff_size, ch);
	/* Widen some so doesn't look so cramped. This may 
	 * encroach on neighboring characters, but if they are italic
	 * too--which they probably are--they probably slant enough
	 * to stay out of the way. */
	inc = MIN(chwidth, chheight * 0.8) / 4.0;

	/* we want to slant by 15 degrees, so use tangent of 15 degrees */
	adj = chheight * 0.27;
	/* if character is really narrow, don't slant so much--
	 * don't squeeze character to less than half its original width */
	if (adj > chwidth / 2.0) {
		adj = chwidth / 2.0;
	}
	
	/* Temporarily change the transform matrix.
	 * The y value is unchanged by the transform.
	 * The new x is
	 *	x' = ax + cy + t
	 * where t is 0, and a and c are as stated below.
	 */
	a = (chwidth + 2 * inc - adj) / chwidth;
	c = adj / chheight;

	outop(O_GSAVE);
	OUTP(("[ %f 0.0 %f 1.0 0.0 0.0 ] ", a, c));
	outop(O_CONCAT);

	/* The x location will get adjusted by the new transform matrix,
	 * so we have to compensate so it will appear where it should.
	 * We take the PostScript transform matrix equation given above,
	 * then set x' to the x value that was passed in to us,
	 * and rearrange to solve for x.
	 */
	/* We scrunch a little more than a non-ital would be,
	 * to somewhat compensate for having made the character wider. */
	outp_muschar((x - c * y) / a, y, ch, size, font, horzscale * 0.95, bold);

	/* return to previous transform matrix */
	outop(O_GRESTORE);

	/* x of music char is in middle, so set current to right edge */
	set_cur(x + width(font, eff_size, ch) * horzscale / 2.0, y);
}


/* print bar line */

static void
pr_bar(mll_p, x, is_pseudobar)

struct MAINLL *mll_p;	/* print bar connected here */
double x;		/* x coordinate */
int is_pseudobar;	/* YES if is pseudobar at beginning of score */

{
	register int s;		/* staff number */
	register int n;		/* index into range list */
	struct BAR *bar_p;	/* info about the bar */
	struct MAINLL *m_p;	/* to walk through main list */
	int next_is_restart = NO;	/* if following bar is a restart */
	short at_end_of_score;	/* YES or NO */


	debug(512, "pr_bar");

	if (is_pseudobar == YES) {
		bar_p = mll_p->u.clefsig_p->bar_p;
	}
	else {
		bar_p = mll_p->u.bar_p;
	}

	/* We need to know if the following bar (if any) is a restart,
	 * because then this one will have to be handled like it is at
	 * the right margin, so find out. */
	for (m_p = mll_p->next; m_p != (struct MAINLL *) 0; m_p = m_p->next) {
		if (m_p->str == S_FEED) {
			/* If there was a restart, it's been moved to this
			 * feed and is thus now irrelevant. */
			break;
		}
		/* If there is a clefsig, then even if there is a restart
		 * we should not remove this bar's right padding--
		 * there is still some staff after it for the
		 * clef/keysig/time (whatever subset is specified by clefsig),
		 * and moving the bar would cause them to get too close. */
		if (m_p->str == S_CLEFSIG) {
			break;
		}
		if (m_p->str == S_BAR) {
			if (m_p->u.bar_p->bartype == RESTART) {
				next_is_restart = YES;
			}
			/* we've looked ahead far enough */
			break;
		}
	}

	at_end_of_score = NO;
	/* go down the bar list and the list of staffs */
	for (s = 1, n = 0; n < Score.nbarst; n++) {

		/* everything up to next range is barred individually */
		for (   ; s < Score.barstlist[n].top; s++) {
			at_end_of_score = pr_bar_range(bar_p, s, s, (double) x,
				next_is_restart, Score.barstlist[n].between,
				mll_p);
		}

		/* everything in the range is barred together */
		at_end_of_score = pr_bar_range(bar_p, Score.barstlist[n].top,
			Score.barstlist[n].bottom, x,
			next_is_restart, Score.barstlist[n].between, mll_p);
		s = Score.barstlist[n].bottom + 1;
	}

	/* any remaining are barred individually */
	for (   ; s <= Score.staffs; s++) {
		/* Note that since not listed in a "between" range,
		 * it will always be the normal way of on the staff itself. */
		at_end_of_score = pr_bar_range(bar_p, s, s, (double) x,
						next_is_restart, NO, mll_p);
	}

	/* If user specified a measure number use that */
	if (bar_p->mnum > 0) {
		Meas_num = bar_p->mnum;
	}
	/* It the last STAFF we saw was a multirest, the measure number
	 * needs to be incremented by the number of measures of multirest.
	 * Since this is stored as a negative, we subtract the
	 * negative to get the same effect as adding the absolute
	 * value */
	else if ( (Last_staff != (struct STAFF *) 0)
			&& (is_pseudobar == NO)
			&& (Last_staff->groups_p[0] != (struct GRPSYL *) 0)
			&& (Last_staff->groups_p[0]->is_multirest == YES) ) {
		Meas_num -= Last_staff->groups_p[0]->basictime;
	}
	else if ((bar_p->bartype != INVISBAR) && (bar_p->bartype != RESTART)
						&& (is_pseudobar == NO)) {
		/* normal case, not multirest; just increment measure number */
		Meas_num++;
	}

	if (is_pseudobar == NO) {
		print_subbar(bar_p);
	}

	/* print rehearsal mark if any */
	if (is_pseudobar == NO) {
		pr_reh(mll_p);
	}

	/* take care of pedal marks for the measure */
	if (is_pseudobar == NO) {
		pr_ped_bar(mll_p, bar_p);
	}

	if (at_end_of_score == YES && PostScript_hooks[PU_ATSCOREEND] != 0) {
		set_cur(Score_location_p[AE], Score_location_p[AS]);
		pr_print(PostScript_hooks[PU_ATSCOREEND], YES);
	}
}


/* given a range of staffs to bar together, find which are visible and from
 * that, the y-coordinates of the ends of the bar line, and draw it */
/* Return whether this is the last bar line of the current score or not. */

static int
pr_bar_range(bar_p, topstaff, botstaff, x, next_is_restart, between, mll_p)

struct BAR *bar_p;	/* info about bar */
int topstaff;		/* top staff to be barred together */
int botstaff;		/* bottom staff to be barred together */
double x;		/* x coordinate of where to draw the bar */
int next_is_restart;	/* YES if following bar is RESTART */
int between;		/* YES if should draw between staffs */
struct MAINLL *mll_p;	/* to get effective margin */

{
	float y1, y2;		/* top and bottom of bar line */
	float halfbarwidth;	/* half the width of the bar line */
	int staffno;
	int stafflines;
	int at_end_of_score = NO;


	/* check for null pointer to avoid core dumps */
	if (Score_location_p == (float *) 0) {
		pfatal("can't print bar: no feed");
		return(at_end_of_score);
	}

	/* Normally, we want some padding on both sides of a bar line,
	 * but at the end of a staff, we don't want right padding.
	 * This applies either if we are at the right
	 * margin or if the next bar is a restart. */
	halfbarwidth = width_barline(bar_p) / 2.0;
	/* Make sure bars line at end of score are precisely at the end */
	if (EFF_PG_WIDTH - eff_rightmargin(mll_p) - x <= halfbarwidth + 3.0 * STDPAD) {
		/* Should only hit this now if there is a bug in placement
		 * of last bar line in a score, but since we changed how
		 * that is determined, better safe than sorry. */
		x = EFF_PG_WIDTH - eff_rightmargin(mll_p) - halfbarwidth
							+ eos_bar_adjust(bar_p);
		at_end_of_score = YES;
	}

	/* Similarly, make sure bars line just before a restart
	 * are precisely at the point where the restart whitebox starts. */
	if (next_is_restart) {
		struct MAINLL *m_p;

		/* find the restart */
		for (m_p = mll_p; m_p != 0; m_p = m_p->next) {
			if (m_p->str == S_BAR && m_p->u.bar_p->bartype == RESTART) {
				if (m_p->u.bar_p->c[AX] - HALF_RESTART_WIDTH
						- m_p->u.bar_p->padding - x
						<= halfbarwidth + 2.0 * STDPAD) {
					x = m_p->u.bar_p->c[AX]
							- HALF_RESTART_WIDTH
							- m_p->u.bar_p->padding
							- halfbarwidth
							+ eos_bar_adjust(bar_p);
				}
				break;
			}
		}
	}

	/* If user said barstyle=all, bottom will be MAXSTAFFS, so
	 * limit to actual number of staffs */
	if (botstaff > Score.staffs) {
		botstaff = Score.staffs;
	}

	/* go through the range of staffs */
	/* Note: y2 doesn't really need to be set here, it's just to shut up
	 * compilers that think it could be used without being set. */
	for (y1 = y2 = -1.0, staffno = topstaff; staffno <= botstaff; staffno++) {
		/* only worry about visible staffs */
		if ( (svpath(staffno, VISIBLE))->visible == YES) {

			stafflines = svpath(staffno, STAFFLINES)->stafflines;
			set_staffscale(staffno);

			/* if hadn't found any staff yet to bar, now we have */
			if (y1 < 0.0) {
				/* Handle bar lines between staffs */
				if (between == YES) {
					y1 = Staffs_y[staffno] -
						(stafflines - 1) * Stepsize
						* (is_tab_staff(staffno) ?
						TABRATIO : 1.0);
				}
				else if (stafflines < 2) {
					y1 = Staffs_y[staffno] +
							(2.0 * Stepsize);
				}
				else {
					y1 = Staffs_y[staffno] +
						(stafflines - 1) * Stepsize
						* (is_tab_staff(staffno) ?
						TABRATIO : 1.0);
					/* 2-line staffs get a bit more, so
					 * repeat sign dots have something
					 * to be next to */
					if (stafflines == 2) {
						y1 += 2 * Stepsize;
					}
				}
			}
			else if (between == YES) {
				y2 = Staffs_y[staffno] +
					(stafflines - 1) * Stepsize *
					(is_tab_staff(staffno) ? TABRATIO : 1.0);
				draw_bar(bar_p->bartype, bar_p->linetype,
					(double) x, (double) y1, (double) y2);
				if (staffno < botstaff) {
					pr_repeat_dots(bar_p->bartype, staffno, (double) x, between);
				}
				y1 = Staffs_y[staffno] -
					(stafflines - 1) * Stepsize
					* (is_tab_staff(staffno) ?
					TABRATIO : 1.0);
				continue;
			}

			/* this is the bottom one found so far */
			if (stafflines < 2) {
				y2 = Staffs_y[staffno] - (2.0 * Stepsize);
			}
			else {
				y2 = Staffs_y[staffno] -
					(stafflines - 1) * Stepsize *
					(is_tab_staff(staffno) ? TABRATIO : 1.0);
				if (stafflines == 2) {
					y2 -= 2 * Stepsize;
				}
			}

			/* if repeat, print the dots */
			pr_repeat_dots(bar_p->bartype, staffno, (double) x, between);
		}
	}

	/* if any were visible, we draw the bar line now */
	if (y1 > 0.0 && between == NO) {
		draw_bar(bar_p->bartype, bar_p->linetype,
					(double) x, (double) y1, (double) y2);
	}

	return(at_end_of_score);
}


/* actually draw a bar line of the proper type at specified place */

/*--- Note: any changes in width made here have to be reflected in
 * pr_bar_range() for adjustment when at right edge of page, and
 * in width_barline() */

static void
draw_bar(bartype, linetype, x, y1, y2)

int bartype;	/* info about single, double, repeat, etc */
int linetype;
double x;
double y1;	/* top of bar line */
double y2;	/* bottom of bar line */

{
	/* always use default staffscale for bar lines since they are
	 * not associated with any particular staff */
	set_staffscale(0);
	if (bartype == SINGLEBAR || bartype == DOUBLEBAR) {
		do_linetype(linetype);
	}
	/* dashed/dotted look better if we offset them slightly */
	if (linetype == L_DASHED || linetype == L_DOTTED) {
		y1 -= Stepsize * 0.375;
		y2 += Stepsize * 0.1;
	}

	switch (bartype) {

	case DOUBLEBAR:
		draw_line(x - 2.0 * STDPAD, y1, x - 2.0 * STDPAD, y2);
		draw_line(x + STDPAD, y1, x + STDPAD, y2);
		break;

	case SINGLEBAR:
		draw_line(x, y1, x, y2);
		break;

	case REPEATSTART:
		draw_line(x + STDPAD, y1, x + STDPAD, y2);
		do_linetype(L_WIDE);
		draw_line(x - (3.0 * STDPAD), y1, x - (3.0 * STDPAD), y2);
		draw_bracketrepeats(x, y1, y2, YES);
		break;

	case REPEATEND:
		draw_line(x, y1, x, y2);
		do_linetype(L_WIDE);
		draw_line(x + (4.0 * STDPAD), y1, x + (4.0 * STDPAD), y2 );
		draw_bracketrepeats(x + STDPAD, y1, y2, NO);
		break;

	case REPEATBOTH:
		do_linetype(L_WIDE);
		draw_line(x - (2.5 * STDPAD), y1, x - (2.5 * STDPAD), y2);
		draw_line(x + (2.5 * STDPAD), y1, x + (2.5 * STDPAD), y2);
		draw_bracketrepeats(x + 3.5 * STDPAD, y1, y2, YES);
		draw_bracketrepeats(x - 3.5 * STDPAD, y1, y2, NO);
		break;

	case ENDBAR:
		draw_line(x - (2.0 * STDPAD), y1, x - (2.0 * STDPAD), y2);
		do_linetype(L_WIDE);
		draw_line(x + (2.0 * STDPAD), y1, x + (2.0 * STDPAD), y2);
		break;

	case RESTART:
		/* This is a "funny" bar that is drawn when the staff lines
		 * are printed, so there isn't anything to be done here. */
		break;

	case INVISBAR:
		/* nothing to do! */
		break;

	default:
		pfatal("bad bar type");
	}
	do_linetype(L_NORMAL);
}


/* print the dots for a repeat sign */

static void
pr_repeat_dots(bartype, staff, x, between)

int bartype;	/* repeatstart, repeatend, repeatboth, etc */
int staff;	/* which staff to print on */
double x;	/* horizontal position */
int between;	/* YES or NO */

{
	double y;	/* vertical position of middle of staff */
	double topoffset, bottomoffset;		/* dot offset */
	double x_offset;	/* X distance from bar line */
	double adjust;		/* adjustment for tablature and/or staffscale */
	int stafflines;
	

	/* If no dots, don't bother */
	if (bartype != REPEATSTART && bartype != REPEATEND
						&& bartype != REPEATBOTH) {
		return;
	}

	if (Score_location_p == (float *) 0) {
		/* this should never be hit--we already checked earlier */
		pfatal("can't do repeat: no feed");
		return;
	}

	/* get y offset based on staff */
	y = Staffs_y[staff];
	stafflines = svpath(staff, STAFFLINES)->stafflines;
	adjust = Stepsize * (is_tab_staff(staff) ? TABRATIO : 1.0);
	/* 1 and 2 line tab staffs need to have the dots moved closer to the
	 * line than normal if bracketrepeats is in effect, to look better. */
	if (stafflines < 3 && is_tab_staff(staff) == YES &&
						Score.bracketrepeats == YES) {
		adjust -= Stdpad;
	}

	x_offset = (bartype == REPEATBOTH ? 7.0 : 4.0) * STDPAD;

	if (between == YES) {
		if (bartype != REPEATEND) {
			do_rdots(x + x_offset,
				(Staffs_y[staff] + Staffs_y[staff+1]) / 2.0,
				STEPSIZE, STEPSIZE);
		}
		if (bartype != REPEATSTART) {
			do_rdots(x - x_offset,
				(Staffs_y[staff] + Staffs_y[staff+1]) / 2.0,
				STEPSIZE, STEPSIZE);
		}
		return;
	}

	/* For a 1 line staff, the setting of repeatdots doesn't
	 * matter, since we will print 2 dots in the same place in either
	 * case; otherwise we have to have special code for the "all" case. */
	if (svpath(staff, REPEATDOTS)->repeatdots == RD_ALL &&
						stafflines > 1) {
		int ndots;

		/* 2 line staff is strange. There is only 1 space for putting
		 * a dot, so even with standard we add a second dot above
		 * the top line. It is not clear what we should do in "all"
		 * case, but to make different from standard and to look
		 * more symmetrical, we put dot below bottom line as well. */
		if (stafflines == 2) {
			ndots = 3;
			topoffset = 2.0 * adjust;
		}
		else {
			ndots = stafflines - 1;
			topoffset = adjust * (ndots - 1);
		}

		/* print dots top to bottom */
		for (   ; ndots > 0; ndots--, topoffset -= adjust * 2.0) {
			if (bartype != REPEATEND) {
				pr_muschar(x + x_offset, y + topoffset,
						C_DOT, DFLT_SIZE, FONT_MUSIC);
			}
			if (bartype != REPEATSTART) {
				pr_muschar(x - x_offset, y + topoffset,
						C_DOT, DFLT_SIZE, FONT_MUSIC);
			}
		}
		return;
	}

	/* Do the "standard" case */
	bottomoffset = topoffset = adjust;
	/* if even number of staff lines, compensate by moving up */
	if ( (stafflines & 1) == 0) {
		y += adjust;
	}


	/* if more than 5 lines on staff, leave one blank space between
	 * the dots */
	if (stafflines > 5) {
		if ( (stafflines & 1) == 0) {
			/* even number of staff lines, move bottom down */
			bottomoffset = 3 * adjust;
		}
		else {
			/* odd number of lines, move top up */
			topoffset = 3 * adjust;
		}
	}
	

	/* print dots at appropriate locations */
	if (bartype != REPEATEND) {
		do_rdots((double) (x + x_offset), y, topoffset,
					bottomoffset);
	}
	if (bartype != REPEATSTART) {
		do_rdots((double) (x - x_offset), y, topoffset,
					bottomoffset);
	}
}


/* print the 2 dots for a repeat sign */

static void
do_rdots(x, y, topoffset, bottomoffset)

double x;
double y;	/* y is a middle of staff, so offset from there */
double topoffset, bottomoffset;		/* offset from y in each direction */

{
	pr_muschar(x, y + topoffset, C_DOT, DFLT_SIZE, FONT_MUSIC);
	pr_muschar(x, y - bottomoffset, C_DOT, DFLT_SIZE, FONT_MUSIC);
}


/* Print subbars -- subdivisions of measures. Loops through the list
 * of subbars for the current measure. */

static void
print_subbar(bar_p)

struct BAR *bar_p;

{
	int n;

	for (n = 0; n < Score.nsubbar; n++) {
		pr1_subbar(Score.subbarlist[n].appearance_p,
				&(bar_p->subbar_loc[n]));
	}
}


/* Print one subbar */

static void
pr1_subbar(sb_app_p, sb_loc_p)

struct SUBBAR_APPEARANCE *sb_app_p;	/* What subbar looks like */
struct SUBBAR_LOC *sb_loc_p;		/* AX at which to draw and
					 * array of what staffs to print on */

{
	int r;			/* range index */
	int s;			/* staff index */
	int top, bottom;	/* staffs in range */
	int linetype;		/* L_NORMAL, L_DOTTED, or L_DASHED */
	int bartype;		/* SINGLEBAR, DOUBLEBAR */
	double x;
	double top_y, bottom_y;


	linetype = sb_app_p->linetype;
	bartype = sb_app_p->bartype;
	x = sb_loc_p->ax;

	for (r = 0; r < sb_app_p->nranges; r++) {
		if (sb_app_p->ranges_p[r].all == YES) {
			top = 1;
			bottom = Score.staffs;
		}
		else {
			top = sb_app_p->ranges_p[r].top;
			bottom = sb_app_p->ranges_p[r].bottom;
		}

		/* constrain top and bottom to visible and to-be-printed staffs */
		while (top <= bottom && 
				((svpath(top, VISIBLE)->visible == NO) ||
				( (sb_loc_p->pr_subbars[top] == NO) &&
				(sb_app_p->ranges_p[r].between == NO)) ) ) {
			top++;
		}
		while (bottom >= top &&
				((svpath(bottom, VISIBLE)->visible == NO) ||
				( (sb_loc_p->pr_subbars[bottom] == NO) &&
				(sb_app_p->ranges_p[r].between == NO)) ) ) {
			bottom--;
		}
		if (bottom < top) {
			/* Must have all been invisible and/or not to print */
			continue;
		}

		if (sb_app_p->ranges_p[r].between == YES) {
			for (s = top; s < bottom; s++) {
				int below;	/* next lower visible staff */

				/* Skip past invisible */
				if (svpath(s, VISIBLE)->visible == NO) {
					continue;
				}

				for (below = s + 1; below <= bottom; below++) {
					if (svpath(below, VISIBLE)->visible == YES) {
						break;
					}
				}
				if (below > bottom) {
					break;
				}

				/* If not to be printed on either staff,
				 * skip this pair. */
				if (sb_loc_p->pr_subbars[s] == NO &&
						sb_loc_p->pr_subbars[below] == NO) {
					continue;
				}

				top_y = subbar_y(LR_BOTTOM, 0.0, s);
				bottom_y = subbar_y(LR_TOP, 0.0, below);
				draw_bar(bartype, linetype, x, top_y, bottom_y);
			}
		}
		else {
			/* The range may be interrupted by staffs where
			 * we should not print the subbar. So chop the
			 * range into groups to actually get drawn together.
			 * Find the top and bottom staff in each such group. */
			int group_top, group_bottom;

			group_bottom = 0;
			for (group_top = top; group_top <= bottom;
						group_top = group_bottom + 1) {
				while (group_top <= bottom && sb_loc_p->pr_subbars[group_top] == NO) {
					group_top++;
				}
				if (group_top > bottom) {
					/* We are done with this range */
					break;
				}

				for (group_bottom = group_top;
						group_bottom < bottom;
						group_bottom++) {
					if (sb_loc_p->pr_subbars[group_bottom+1] == NO) {
						break;
					}
				}

				top_y = subbar_y(sb_app_p->upper_ref_line,
	                        	sb_app_p->upper_offset, group_top);
				bottom_y = subbar_y(sb_app_p->lower_ref_line,
	                        	sb_app_p->lower_offset, group_bottom);
				draw_bar(bartype, linetype, x, top_y, bottom_y);
			}
		}
	}
}


/* Return the Y at which to place one end of a subbar.
 * It is given a staff number, which line of that staff to go relative to,
 * and how much to offset from that line.
 */
static double
subbar_y(line_ref, offset, staff)

int line_ref;	/* LR_* */
double offset;	/* from the reference line */
int staff;

{
	double staffstepsize;

	staffstepsize = svpath(staff, STAFFSCALE)->staffscale * STEPSIZE
			* (is_tab_staff(staff) ? TABRATIO : 1.0);

	switch (line_ref) {

	case LR_MIDDLE:
		return(Staffs_y[staff] + (offset * staffstepsize));

	case LR_TOP:
		return(Staffs_y[staff] + halfstaffhi(staff) + offset * staffstepsize);

	case LR_BOTTOM:
		return(Staffs_y[staff] - halfstaffhi(staff) + offset * staffstepsize);

	default:
		pfatal("unknown line_ref type in subbar_y()");
		/*NOTREACHED*/
		break;
	}
	/* Keep lint happy */
	/*NOTREACHED*/
	return(Staffs_y[staff]);
}


/* Draw bracket for a repeat to make it stand out more, if the score
 * parameter 'bracketrepeats' is set. */

static void
draw_bracketrepeats(x, y1, y2, is_start)

double x;
double y1, y2;
int is_start;	/* YES if is starting a repeat, NO if ending it */

{
	if (Score.bracketrepeats == YES) {
		outcoord(x);
		outcoord(y1);
		outcoord(y2);
		OUTP(("%d ", is_start ? 1 : 0));
		outop(O_REPEATBRACKET);
	}
}


/* print any rehearsal marks associated with bar line */

static void
pr_reh(mll_p)

struct MAINLL *mll_p;	/* current bar line is off of here */

{
	struct MARKCOORD *mark_p;	/* where to put rehearsal mark */
	float y;			/* vertical location */
	struct BAR *bar_p;
	char *str;			/* the string, with box or circle
					 * or nothing as appropriate for
					 * the associated staff */


	if (mll_p->str == S_BAR) {
		bar_p = mll_p->u.bar_p;
	}
	else {
		bar_p = mll_p->u.clefsig_p->bar_p;
	}

	for (mark_p = bar_p->reh_p; mark_p != (struct MARKCOORD *) 0;
					mark_p = mark_p->next) {

		/* print rehearsal mark if any */
		if (bar_p->reh_string != (char *) 0) {

			y = Staffs_y[mark_p->staffno] + mark_p->ry;

			/* get boxed or circled version if appropriate */
			str = get_reh_string(bar_p, mark_p->staffno);
			pr_string((double) bar_p->c[AX] - left_width(str),
					(double) y, str, J_LEFT,
					mll_p->inputfile, mll_p->inputlineno);
		}
	}
}


/* draw a box of given size at given x,y */

static void
pr_box(x, y, boxheight, boxwidth)

double x, y;
double boxheight, boxwidth;

{
	do_linetype(L_NORMAL);
	do_newpath();
	do_moveto(x, y);
	do_line(x, y + boxheight);
	do_line(x + boxwidth, y + boxheight);
	do_line(x + boxwidth, y);
	do_closepath();
	do_stroke();
}


/* do a feed (newscore and maybe newpage) */

void
pr_feed(main_feed_p)

struct MAINLL *main_feed_p;	/* MAINLL struct pointing to FEED */

{
	register int s;		/* walk through staffs */
	float lowest_y = 0.0;	/* y coord of bottom staff. Initialization is
				 * solely to shut up bogus compiler warning */
	float highest_y = 0.0;
	float headfoot_height;	/* for this page */
	int printed;		/* How many staffs printed so far */
	int had_br_br;		/* YES if had braces and/or brackets printed */
	double brac_x_offset;	/* brace/bracket offset from prep_brac() */
	int has_labels;		/* YES if there are labels */
	int need_vert_line = NO;	/* if need line at left edge */
	struct FEED *feed_p;
	int stafflines;
	double y;


	debug(256, "pr_feed lineno=%d", main_feed_p->inputlineno);

	feed_p = main_feed_p->u.feed_p;

	/* If user put top/bottom or newpage at the very end of the file,
	 * we could end up with a page with nothing but header/footer.
	 * So if there is no good reason to do another page, we don't. */
	if (Atend_info.separate_page == NO && main_feed_p->next == 0) {
		/* Nothing at all after the feed,
		 * so no need to make another page. */
		return;
	}

	/* if doing a page feed, print the headers and footers on the
	 * current page and move on to the next one */
	if (feed_p->pagefeed == YES) {
		newpage(main_feed_p);
	}

	/* If there is a top and/or bot block, print those.
	 * Even though from user's viewpoint the current page may
	 * use top2/bot2, placement phase will have set top_p/bot_p
	 * to whatever is appropriate for this page.
	 */
	if (feed_p->top_p != 0) {
		if (Feednumber == 1) {
			if (Curr_pageside == PGSIDE_LEFT) {
				headfoot_height = Leftheader.height;
			}
			else {
				headfoot_height = Rightheader.height;
			}
		}
		else {
			if (Curr_pageside == PGSIDE_LEFT) {
				headfoot_height = Leftheader2.height;
			}
			else {
				headfoot_height = Rightheader2.height;
			}
		}
		y = PGHEIGHT - EFF_TOPMARGIN - headfoot_height;
		pr_topbot(feed_p->top_p, y);
	}
	if (feed_p->bot_p != 0) {
		if (Feednumber == 1) {
			if (Curr_pageside == PGSIDE_LEFT) {
				headfoot_height = Leftfooter.height;
			}
			else {
				headfoot_height = Rightfooter.height;
			}
		}
		else {
			if (Curr_pageside == PGSIDE_LEFT) {
				headfoot_height = Leftfooter2.height;
			}
			else {
				headfoot_height = Rightfooter2.height;
			}
		}
		y = EFF_BOTMARGIN + feed_p->bot_p->height + headfoot_height;
		pr_topbot(feed_p->bot_p, y);
	}

	if (main_feed_p->next == 0) {
		/* Feed at end of piece, presumably to force
		 * gridsatend onto separate page or something like that */
		return;
	}
	if (main_feed_p->next->str != S_CLEFSIG) {
		/* Must be BLOCKHEAD or lines/curves at end of file.
		 * In any case, no actual music staffs to print. */
		return;
	}

	/* now do score feed stuff */
	/* keep track of where the staffs are: we need this for
	 * drawing lots of other things relative to the staffs */
	Score_location_p = feed_p->c;
	set_staff_y(main_feed_p);

	if (Feednumber == 1 && Meas_num == 1) {
		/* first time through. See if the song begins with a
		 * "pickup" measure, i.e., its first chord is all spaces.
		 * If so, don't count that measure in measure number. */
		if (has_pickup() == YES) {
			Meas_num--;
		}
	}

	brac_x_offset = prep_brac(NO, 0.0, main_feed_p, &has_labels);
	set_score_coord(main_feed_p);
	if (PostScript_hooks[PU_ATSCOREBEGIN] != 0) {
		set_cur(Score_location_p[AW], Score_location_p[AN]);
		pr_print(PostScript_hooks[PU_ATSCOREBEGIN], YES);
	}

	/* for each staff */
	for ( printed = 0, s = 1; s <= Score.staffs; s++) {

		/* print if visible */
		if ( (svpath(s, VISIBLE))->visible == YES) {

			stafflines = svpath(s, STAFFLINES)->stafflines;
			set_staffscale(s);
			if (stafflines < 3) {
				lowest_y = Staffs_y[s] - (2.0 * Stepsize)
					* (is_tab_staff(s) ? TABRATIO : 1.0);
			}
			else {
				lowest_y = Staffs_y[s] - (stafflines - 1)
					* Stepsize * (is_tab_staff(s) ?
					TABRATIO : 1.0);
			}

			/* find the top of the score */
			if (printed == 0) {
				if (stafflines < 3) {
					highest_y = Staffs_y[s]
					+ (2.0 * Stepsize)
					* (is_tab_staff(s) ? TABRATIO : 1.0);
				}
				else {
					highest_y = Staffs_y[s]
					+ (stafflines - 1)
					* Stepsize * (is_tab_staff(s) ?
					TABRATIO : 1.0);
				}
			}

			printed++;

			outcoord( (double) Score_location_p[AX]);
			outcoord( (double) Staffs_y[s]);
			outcoord( (double) Score_location_p[AE]);
			OUTP(("%d %f %f ", svpath(s, STAFFLINES)->stafflines,
				(is_tab_staff(s) == YES ? TABRATIO : 1.0),
				Staffscale));
			outop(O_STAFF);

			/* print measure number at beginning of staff if
			 * necessary */
			pr_meas_num(s, Score_location_p[AX]);
		}
	}

	/* print brace/bracket and group label */
	had_br_br = pr_brac(NO, brac_x_offset, has_labels);

	if (printed == 0) {
		/* we check for this earlier, so should never hit this */
		pfatal("no staffs visible");
	}

	/* draw vertical line at left edge of staffs */
	/* but don't draw if only one staff and no brace/bracket */
	if ((printed > 1) || (had_br_br != NO)) {
		need_vert_line = YES;
		do_linetype(L_NORMAL);
		draw_line(Score_location_p[AX], highest_y,
					Score_location_p[AX],  lowest_y);
	}

	/* If there should be slashes above the score to make it clearer
	 * where the scores are, do that. */
	if (main_feed_p->u.feed_p->sl_bet_top_offset != 0.0) {
		double half_width;
		double half_height;
		double half_thickness;
		double space;
		double x, y;

		half_width = SL_BET_X_TOTAL * Score.staffscale * STEPSIZE / 2.0;
		half_height = SL_BET_Y_TOTAL * Score.staffscale * STEPSIZE / 2.0;
		half_thickness = SL_BET_Y_LINE * Score.staffscale * STEPSIZE / 2.0;
		space = SL_BET_Y_SPACE * Score.staffscale *STEPSIZE;
		x = main_feed_p->u.feed_p->c[AX];
		y = main_feed_p->u.feed_p->c[AY]
			+ main_feed_p->u.feed_p->sl_bet_top_offset
			- half_height;
		draw_parallelogram(x - half_width,
			y - half_height + half_thickness,
			x + half_width,
			y + half_height - 3.0 * half_thickness - space,
			half_thickness);
		draw_parallelogram(x - half_width,
			y - half_height + 3.0 * half_thickness + space,
			x + half_width,
			y + half_height - half_thickness,
			half_thickness);
	}

	pr_restarts(main_feed_p, highest_y, lowest_y, need_vert_line);

	/* set current to x,y of score */
	set_cur(Score_location_p[AX], Score_location_p[AY]);
}


/* Given a BLOCKHEAD for a top/bottom and a y location, print the
 * contents of the BLOCKHEAD at that location.
 */

static
void pr_topbot(blockhead_p, y)

struct BLOCKHEAD *blockhead_p;
double y;

{
	double x;

	/* Note cannot use eff_*margin() in thus function because they are only
	 * valid in music window.  */
	x = Score.rightmargin / Score.scale_factor;
	/* Set up window coordinates, go to upper left of window, and print */
	set_win_coord(blockhead_p->c);
	set_win(y, y - blockhead_p->height,
			PGWIDTH - (Score.rightmargin / Score.scale_factor), x);
	set_cur(x, y);
	begin_non_music_adj();
	pr_print(blockhead_p->printdata_p, NO);
	end_non_music_adj();
	set_win_coord(0);
}


/* We want to print all the "restart" bars right after the staff lines,
 * so in case anything spills into the white space we write over the staffs,
 * it won't get obliterated. So find any restarts till the next feed and
 * put out a whitebox and do and brace/backets and vertical line needed.
 */

static void
pr_restarts(mll_p, y1, y2, need_vert_line)

struct MAINLL *mll_p;
double y1;
double y2;
int need_vert_line;

{
	double x;
	double x_off;	/* for braces/ brackets */
	int has_labels;	/* YES or NO */

	for (mll_p = mll_p->next; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->next) {
		if (mll_p->str == S_FEED) {
			/* we went far enough */
			return;
		}

		if (mll_p->str == S_BAR && mll_p->u.bar_p->bartype == RESTART) {
			x = mll_p->u.bar_p->c[AX];
			/* Expand the y dimensions to make sure we completely
			 * erase the top and bottom staff lines. */
			do_whitebox(x - HALF_RESTART_WIDTH
					- mll_p->u.bar_p->padding,
					y1 + POINT,
					x + HALF_RESTART_WIDTH, y2 - POINT);

			/* print braces/brackets */
			x_off = prep_brac(YES, x, mll_p, &has_labels);
			pr_brac(YES, x_off, has_labels);

			/* draw vertical line, if needed */
			x += HALF_RESTART_WIDTH - (W_NORMAL / PPI) / 2.0;
			if (need_vert_line == YES) {
				do_linetype(L_NORMAL);
				draw_line(x, y1, x,  y2);
			}

		}
	}
}


/* print a brace or bracket */

void
do_pr_brac(x, y1, y2, which)

double x;	/* coordinates at which to draw brace or bracket */
double y1;
double y2;
int which;	/* BRACELIST or BRACKLIST */

{
	outcoord(x);
	outcoord(y1);
	outcoord(y2);
	outop( which == BRACELIST ? O_BRACE : O_BRACKET);
}


/* output a coordinate. Convert from inches to points, to 0.01 point accuracy */

void
outcoord(val)

double val;		/* an x or y value */

{
	if (isnan(val)) {
		pfatal("got invalid number for a coordinate");
	}
	OUTP(("%.2f ", val * PPI));
}



/* output an integer value */

static void
outint(val)

int val;

{
	OUTP(("%d ", val));
}


/* output a string to be printed. Have to walk through the string
 * one character at a time, possibly breaking into several strings
 * if there are font/size changes or music characters in the middle */

static void
outstring(x, y, fullwidth, horzscale, string, fname, lineno)

double x;	/* where to print string */
double y;
double fullwidth;	/* If bigger than the string's intrinsic width,
			 * this is how much territory the string should take.
			 * This is for creating a right justified paragraph.
			 * For non-justified, you can pass a negative value,
			 * which will certainly be smaller than intrinsic. */
double horzscale;	/* how much to squeeze horizontally */
char *string;	/* what to print */
char *fname;	/* file name for error messages */
int lineno;	/* line number for error messages */

{
	int font, size, code;	/* for current character to print */
	int code2;		/* another character in the string */
	int textfont;		/* font disregarding music characters */
	double vertical, horizontal;
	double slash_x = 0.0, slash_y = 0.0; /* For slash through number.
				 * Initialization is just to avoid bogus
				 * "used before set" warning. It will be
				 * set to a valid value before being used. */
	double space_adjust = 0.0;	/* how much to add to spaces if
					 * doing paragraph justification */
	double intrinsic_width;		/* before adding space_adjust */
	int in_pile = NO;
	int in_digit_string = NO;	/* YES if in a run of digits */
	int in_string = NO;	/* YES if are outputting a string (i.e.,
				 * have printed '(' and have not printed
				 * matching ')'  */
	char pgnumstr[12];	/* page number as a string. Make big enough
				 * to allow some crazy person to use a page
				 * number of 2^31. Actually, we now limit
				 * the first page number to MAXFIRSTPAGE,
				 * so unless the song is about a billion
				 * pages long, this is vast overkill,
				 * but stack space is cheap. */
	float save_y;		/* temporarily remember y value */
	int only_mus_sym;	/* YES if string is solely a music sym */
	float mussym_compensation;	/* inside strings, music symbols
				 * get moved up to the baseline */
	float save_staffscale;


	/* go to starting point of string */
	outcoord( (double) x);
	outcoord( (double) y);
	outop(O_MOVETO);
	set_cur(x, y);

	/* check if consists solely of music character */
	only_mus_sym = is_music_symbol(string);

	intrinsic_width = strwidth(string);
	if (lineno > 0) {
		double flip_adjusted_x;
		double pgwidth;

		flip_adjusted_x = x + Flipshift;
		pgwidth = (In_music ? EFF_PG_WIDTH : PGWIDTH);
		if ( (flip_adjusted_x + intrinsic_width * horzscale > pgwidth)
					|| (flip_adjusted_x < 0.0) ) {
			l_warning(fname, lineno,
					"string extends beyond edge of page");
		}
	}
	/* If we need to right justify, figure out how much to add to spaces */
	if (fullwidth > intrinsic_width) {
		char *s;	/* to walk through string */
		int count;	/* number of space chars */

		/* count how many spaces there are that we can stretch */
		count = 0;
		font = *string;
		size = *(string + 1);
		s = string + 2;
		while ((code = next_str_char(&s, &font, &size) & 0xff) > 0) {
			if (code == ' ' && ! IS_MUSIC_FONT(font)) {
				count++;
			}
		}
		if (count > 0) {
			/* We have at least one space. Apportion needed
			 * padding among the number of space chars. */ 
			space_adjust = (fullwidth - intrinsic_width) / 
						(double) count;
			if (space_adjust < 0.0) {
				/* Hmmm. Apparently string is already
				 * wider than it should be, so leave as is. */
				space_adjust = 0.0;
			}
		}
	}

#ifdef SMALLMEMORY
	/* to make sure string space is cleaned up as soon as possible,
	 * output the string inside a save/restore */
	outop(O_SAVE);
#endif

	/* walk through and output chars one at a time */
	font = *string;
	size = *(string + 1);
	string += 2;
	while( (code = nxt_str_char(&string, &font, &size, &textfont, &vertical,
				&horizontal, &in_pile, YES) & 0xff) > 0) {
		/* do motion, if needed */
		if (vertical != 0.0 || horizontal != 0.0) {
			in_string = end_string(in_string, space_adjust, horzscale);
			set_cur(_Cur[AX] + horizontal * horzscale,
							_Cur[AY] + vertical);
			outcoord( _Cur[AX] );
			outcoord( _Cur[AY] );
			outop(O_MOVETO);
			in_digit_string = NO;
		}

		if ( (code & 0xff) == STR_SLASH) {
			if (in_digit_string == NO) {
				/* this should have been caught... */
				pfatal("STR_SLASH not after digits");
			}

			/* draw the slash */
			in_string = end_string(in_string, space_adjust, horzscale);
			/* Note that since the slash is drawn relative to
			 * x,y values that have already had horzscale applied,
			 * draw_prop_line does not need horzscale adjustment.
			 */ 
			save_y = _Cur[AY];
			draw_prop_line(slash_x - (0.15 * size * Stdpad), slash_y,
				_Cur[AX] + (0.15 * size * Stdpad),
				_Cur[AY] + 0.6 * fontascent(font, size),
				size, O_LINE);
			set_cur(_Cur[AX], save_y);
			outcoord( _Cur[AX] );
			outcoord( _Cur[AY] );
			outop(O_MOVETO);
			in_digit_string = NO;
			continue;
		}

		/* in case we need to draw a slash through digits
	 	 * (most likely for figured bass), keep track of where
		 * a run of digits begins */
		if (isdigit(code) && IS_STD_FONT(font)) {
			if (in_digit_string == NO) {
				in_digit_string = YES;
				/* calculate where to begin the slash
				 * if we need to do one */
				slash_x = _Cur[AX];
				slash_y = _Cur[AY] +
					0.35 * fontascent(font, size);
			}
		}
		else {
			in_digit_string = NO;
		}

		if (IS_MUSIC_FONT(font) ) {
			/* special music character */
			/* end this string, do the music character,
			 * and start a new string */
			in_string = end_string(in_string, space_adjust, horzscale);

			/* music characters are strange--their x
			 * is in the middle instead of the
			 * left edge, so compensate for that. Also,
			 * when in strings, we want the bottom of
			 * the music character to be at the baseline
			 * of the text, even if it would normally
			 * descend below. The (- STDPAD) is to account 
			 * for the 1 point of vertical padding on
			 * characters. */
			save_y = _Cur[AY];
			if (only_mus_sym == YES) {
				mussym_compensation = 0.0;
			}
			else {
				mussym_compensation = descent(
					font, size, code) - STDPAD;
			}
			/* music characters embedded inside strings will have
			 * already been size adjusted, so compensate. */
			save_staffscale = Staffscale;
			Staffscale = 1.0;
			if (is_ital_font(textfont) == YES)  {
				pr_sc_ital_muschar(_Cur[AX] +
					width(font, size, code) * horzscale/2.0,
					_Cur[AY] + mussym_compensation,
					code, size, font, horzscale,
					is_bold_font(textfont));
			}
			else {
				pr_sc_muschar(_Cur[AX] +
					width(font, size, code) * horzscale/2.0,
					_Cur[AY] + mussym_compensation,
					code, size, font, horzscale,
					is_bold_font(textfont));
			}
			Staffscale = save_staffscale;

			set_cur(_Cur[AX], save_y);
			outcoord( _Cur[AX] );
			outcoord( _Cur[AY] );
			outop(O_MOVETO);
			continue;
		}

		/* if font or size changed, do that */
		if ( (font != Curr_font) || (size != Curr_size) ) {
			in_string = end_string(in_string, space_adjust, horzscale);
			pr_font(font, size);
			outcoord( _Cur[AX] );
			outcoord( _Cur[AY] );
			outop(O_MOVETO);
		}

		switch (code) {

		case '(':
		case ')':
		case '\\':
			/* things that have to be backslashed */
			in_string = begin_string(in_string, horzscale);
			OUTP(("\\%c", code));
			set_cur(_Cur[AX] + width(font, size, code) * horzscale,
							_Cur[AY]);
			break;

		case '\b':
			/* backspace just changes position */
			in_string = end_string(in_string, space_adjust,
							horzscale);
			set_cur(_Cur[AX] - backsp_width(size) * horzscale,
							_Cur[AY]);
			outcoord( _Cur[AX] );
			outcoord( _Cur[AY] );
			outop(O_MOVETO);
			break;

		case '%':
		case '#':
			/* If this is the special page number char,
			 * of number of pages character, print the
			 * appropriate page number. Have to back up by 2,
			 * because string is already incremented beyond
			 * the % or #. */
			code2 = *(string - 2) & 0xff;
			if ((code == '%' && code2 == STR_PAGENUM) ||
					(code == '#' && code2 == STR_NUMPAGES)) {
				in_string = begin_string(in_string, horzscale);
				OUTP(("%d", (code == '%'
						? Pagenum : Last_pagenum)));

				/* Figure out width and
				 * set current location appropriately */
				pgnumstr[0] = (char) font;
				pgnumstr[1] = (char) size;
				(void) sprintf(pgnumstr + 2, "%d",
					(code == '%' ? Pagenum : Last_pagenum));
				set_cur(_Cur[AX] + strwidth(pgnumstr) * horzscale,
								_Cur[AY]);
				break;
			}
			/* otherwise fall through to normal default case */
			/*FALLTHRU*/

		default:
			if (code != '\n') {
				/* ordinary character */
				in_string = begin_string(in_string, horzscale);
				if ((code & 0x80) || (code == 0x7f)) {
					OUTP(("\\%3o", (unsigned char) code));
				}
				else {
					OUTPCH(((unsigned char)code));
				}
				set_cur(_Cur[AX] + width(font, size, code) * horzscale,
						_Cur[AY]);
				if (code == ' ' && IS_STD_FONT(font) && space_adjust > 0.0) {
					set_cur(_Cur[AX] + space_adjust, _Cur[AY]);
				}
			}
			break;
		}
	}

	(void) end_string(in_string, space_adjust, horzscale);
#ifdef SMALLMEMORY
	outop(O_RESTORE);
#endif
}


/* if haven't started a string yet, start one now, if already doing
 * a string, just return */
/* return YES to say we are inside doing a string */

static int
begin_string(in_string, horzscale)

int in_string;	/* NO if not currently inside a string */
double horzscale;	/* how much to squeeze horizontally */

{
	if (in_string == NO) {
		scrunch(horzscale);
		OUTPCH(('('));
	}
	return(YES);
}


/* if currently doing a string, end it. If not, just return */
/* return NO to say we are no longer inside doing a string */

static int
end_string(in_string, space_adjust, horzscale)

int in_string;		/* YES if currently inside a string */
double space_adjust;	/* if non-zero, use widthshow rather than show,
			 * and use this as the x adjust for spaces */
double horzscale;	/* how much to squeeze horizontally */

{
	if (in_string == YES) {
		OUTP((") "));
		if (fabs(space_adjust) < .001) {
			/* Close enough to zero. In addition to handling the
			 * normal case of no justification,
			 * this handles floating point roundoff error,
			 * or if the amount of padding needed
			 * is too tiny to be worth the trouble.
			 * Use regular show. */
			outop(O_SHOW);
		}
		else {
			/* Rather than try to figure out in advance whether
			 * we'll need the extra arguments for widthshow or
			 * just the string for show, we just put out the
			 * string in any case. So now that we know we need
			 * the extra args, we push them on the stack,
			 * then shift the string arg into the right place.
			 * Note that no horzscale adjustment is needed here,
			 * because that is only used on STUFF, whereas this
			 * code is only called for paragraphs.
			 */
			outcoord(space_adjust);	/* x adjust for spaces */
			outcoord(0.0);		/* y adjust for spaces */
			outint(32);		/* ASCII space */
			outint(4);		/* 4 items involved in roll */
			outint(-1);		/* roll 1 item down */
			outop(O_ROLL);
			outop(O_WIDTHSHOW);
		}
		unscrunch(horzscale);
	}
	return(NO);
}


/* output a postscript operator */

static void
outop(op)

int op;		/* which operator */

{
	switch (op) {

	case O_FONT:
		OUTP(("findfont\n"));
		break;

	case O_SETFONT:
		OUTP(("setfont\n"));
		break;

	case O_SIZE:
		OUTP(("scalefont\n"));
		break;

	case O_LINE:
		OUTP(("lineto stroke\n"));
		break;

	case O_WAVY:
		OUTP(("%f wavy\n", Staffscale));
		break;

	case O_CURVETO:
		OUTP(("curveto\n"));
		break;

	case O_LINEWIDTH:
		OUTP(("setlinewidth\n"));
		break;

	case O_DOTTED:
		OUTP(("[0.1 5] 0 setdash\n"));
		OUTP(("1 setlinecap\n"));
		OUTP(("1 setlinejoin\n"));
		break;

	case O_DASHED:
		OUTP(("[%4.2f %4.2f] 0 setdash\n", 3.0 * Staffscale, 3.0 * Staffscale));
		break;

	case O_ENDDOTTED:
		OUTP(("[] 0 setdash\n"));
		OUTP(("0 setlinecap\n"));
		OUTP(("0 setlinejoin\n"));
		break;

	case O_LINETO:
		OUTP(("lineto\n"));
		break;

	case O_SHOWPAGE:
		OUTP(("showpage\n"));
		break;

	case O_SHOW:
		OUTP(("show\n"));
		break;

	case O_WIDTHSHOW:
		OUTP(("widthshow\n"));
		break;

	case O_ROLL:
		OUTP(("roll\n"));
		break;

	case O_STAFF:
		OUTP(("staff\n"));
		break;

	case O_MOVETO:
		OUTP(("moveto\n"));
		break;

	case O_BRACE:
		OUTP(("brace\n"));
		break;

	case O_BRACKET:
		OUTP(("bracket\n"));
		break;

	case O_REPEATBRACKET:
		OUTP(("repeatbracket\n"));
		break;

	case O_SAVE:
		OUTP(("save\n"));
		break;

	case O_RESTORE:
		OUTP(("restore\n"));
		Last_linetype = -1;
		break;

	case O_GSAVE:
		OUTP(("gsave\n"));
		break;

	case O_GRESTORE:
		OUTP(("grestore\n"));
		Last_linetype = -1;
		break;

	case O_CONCAT:
		OUTP(("concat\n"));
		break;

	case O_TRANSLATE:
		OUTP(("translate\n"));
		break;

	case O_ROTATE:
		OUTP(("rotate\n"));
		break;

	case O_SCALE:
		OUTP(("scale\n"));
		break;

	case O_ARC:
		OUTP(("arc\n"));
		break;

	case O_EOFILL:
		OUTP(("eofill\n"));
		break;

	case O_FILL:
		OUTP(("fill\n"));
		break;

	case O_STROKE:
		OUTP(("stroke\n"));
		break;

	case O_NEWPATH:
		OUTP(("newpath\n"));
		break;

	case O_CLOSEPATH:
		OUTP(("closepath\n"));
		break;

	default:
		pfatal("unknown output operator %d", op);
		break;
	}
}


/* print the header and footer on current page. If first page, use header/footer
 * otherwise use header2 and footer2. Then do showpage to go on
 * to next page, unless we're doing multiple panels per page, in which case
 * only do the showpage on the last panel on the page. */

static void
pr_headfoot(mll_p)

struct MAINLL *mll_p;

{
	struct BLOCKHEAD *header_p;
	struct BLOCKHEAD *footer_p;


	OUTP(("%%  Printing header/footer\n"));
	if (Do_bbox && mll_p != 0) {
		show_bounding_boxes(mll_p);
	}

	/* figure out which header to use */
	if (Feednumber == 1) {
		header_p = (Curr_pageside == PGSIDE_LEFT
					? &Leftheader : &Rightheader);
		Context = C_HEADER;
	}
	else {
		header_p = (Curr_pageside == PGSIDE_LEFT
					? &Leftheader2 : &Rightheader2);
		Context = C_HEAD2;
	}

	/* if there is a header, print it */
	if (header_p->height > 0.0 ||
			(header_p->printdata_p != 0 &&
			header_p->printdata_p->isPostScript == YES)) {
		pr_topbot(header_p, PGHEIGHT - EFF_TOPMARGIN);
	}

	/* figure out which footer to use */
	if (Feednumber == 1) {
		footer_p = (Curr_pageside == PGSIDE_LEFT
					? &Leftfooter : &Rightfooter);
		Context = C_FOOTER;
	}
	else {
		footer_p = (Curr_pageside == PGSIDE_LEFT
					? &Leftfooter2 : &Rightfooter2);
		Context = C_FOOT2;
	}

	/* if there is a footer, print it */
	if (footer_p->height > 0.0 ||
			(footer_p->printdata_p != 0 &&
			footer_p->printdata_p->isPostScript == YES)) {
		pr_topbot(footer_p, EFF_BOTMARGIN + footer_p->height);
	}

	Context = C_MUSIC;

	/* end this page */
#ifdef SMALLMEMORY
	if (Did_save == YES) {
		outop(O_RESTORE);
		Did_save = NO;
	}
#endif
	/* Actual print everything drawn on the page */
	show_the_page();
}


/* go to next page */

static void
to_next_page(mll_p)

struct MAINLL *mll_p;

{

	Feednumber++;
	/* Figure out which page side this is */
	Curr_pageside = page2side(Feednumber);

	/* Set _win for this page */
	for (   ; mll_p != 0 && mll_p->str != S_FEED; mll_p = mll_p->next) {
		;
	}
	if (mll_p == 0) {
		pfatal("unable to find feed in to_next_page");
	}
	set_win(mll_p->u.feed_p->north_win,
			mll_p->u.feed_p->south_win,
			PGWIDTH - eff_rightmargin((struct MAINLL *)0),
			eff_leftmargin((struct MAINLL *)0));

	if ((Printflag = onpagelist(Pagenum)) == YES) {
		start_page();

		/* To help debugging, sometimes it would be nice to have a
		 * grid printed so if we print out the value of some location,
		 * it is easier to tell whether that looks right or not. */
		if (getenv("MUP_GRID") != 0) {
			int x, y;

			outop(O_SAVE);
			OUTP(("0.0  0.5 0.0 setrgbcolor\n"));
			do_linetype(L_DOTTED);
			for (x = 1.0; x < Score.pagewidth; x++) {
				draw_line(x, 0, x, Score.pageheight);
			}
			for (y = 1.0; y < Score.pageheight; y++) {
				draw_line(0, y, Score.pagewidth, y);
			}
			outop(O_RESTORE);
		}

		outop(O_SAVE);
		if (Landscape != 0) {
			OUTP(("%% set up landscape mode\n"));
			outint(Landscape);
			outint(0);
			outop(O_TRANSLATE);
			outint(90);
			outop(O_ROTATE);
		}

		/* handle 2-on-1 page printing. Translate and rotate each
		 * page as needed. Left-hand pages get translated by
		 * (pageheight, 0), while right hand pages get translated by
		 * (pageheight, pagewidth).  Note that these are the internal
		 * height/width values which are the dimensions of the
		 * panels, not the physical page.
		 * Both get rotated 90 degrees. */
		if (Score.panelsperpage == 2) {
			outcoord(Score.pageheight);
			outcoord( (Pagesprinted & 1) ?
					0.0 : Score.pagewidth);
			outop(O_TRANSLATE);
			outint(90);
			outop(O_ROTATE);
		}
		setscale();

		if ((Feednumber & 0x1) == 0 && (Score.flipmargins == YES)
				&& (Score.rightmargin != Score.leftmargin) ) {
			Flipshift = Score.rightmargin - Score.leftmargin;
			outcoord(Flipshift);
			outcoord(0.0);
			outop(O_TRANSLATE);
		}
		else {
			Flipshift = 0.0;
		}

		/* make sure things are reset to default values */
		Last_linetype = -1;
		Doing_dotted = NO;
		Curr_font = FONT_UNKNOWN;
		Curr_size = DFLT_SIZE;
		if (PostScript_hooks[PU_ATPAGEBEGIN] != 0) {
			set_cur(_Page[AW], _Page[AN]);
			begin_non_music_adj();
			pr_print(PostScript_hooks[PU_ATPAGEBEGIN], YES);
			end_non_music_adj();
		}
	}
}


/* print everything in list of PRINTDATAs, relative to specified offsets */

static void
pr_print(printdata_p, is_hook_call)

struct PRINTDATA *printdata_p;	/* list of things to print */
int is_hook_call;

{
	float x, y;	/* coordinate */
	struct COORD_INFO *coordinfo_p;	/* to find out if coord is associated
			 * with something that is invisible */
	/* string_data_p is usually same as printdata_p,
	 * but for mirrored titles, is the mirror */
	struct PRINTDATA *string_data_p;


	/* walk down list of things to print */
	for (  ; printdata_p != (struct PRINTDATA *) 0;
					printdata_p = printdata_p->next) {

		/* When not being called from a PostScript hook,
		 * a PRINTDATA with a ps_usage value of something other than
		 * PU_NORMAL means the users wants to change
		 * the current value of a hook, so we just update
		 * the hook table. The actual processing will then occur
		 * later, when the hook is called. */
		if (is_hook_call == NO && printdata_p->ps_usage != PU_NORMAL) {
			if (printdata_p->ps_usage < 0
					|| printdata_p->ps_usage >= PU_MAX) {
				pfatal("Out of range ps_usage value %d",
							printdata_p->ps_usage);
			}
			PostScript_hooks[printdata_p->ps_usage] = printdata_p;
			continue;
		}

		/* if x or y is associated with something that is invisible,
		 * then don't print this item */
		/* Note that now that we support arithmetic expressions,
		 * the expr_has_invis() check a few lines down from here
		 * really should cover everything, making these older checks
		 * on hor_p and vert_p redundant. But just in case there are
		 * some cases where we created an inpcoord internally and
		 * didn't make an expression for it, we'll leave them.
		 * They shouldn't hurt anything, other than taking a
		 * tiny amount of time. For cases they catch, they will
		 * actually be faster than the expr_has_invis() check.
		 */
		if ( (coordinfo_p = find_coord(printdata_p->location.hor_p))
						!= (struct COORD_INFO *) 0) {
			if (coordinfo_p->flags & CT_INVISIBLE) {
				continue;
			}
		}
		if ( (coordinfo_p = find_coord(printdata_p->location.vert_p))
						!= (struct COORD_INFO *) 0) {
			if (coordinfo_p->flags & CT_INVISIBLE) {
				continue;
			}
		}

		/* If any tag referenced anywhere in either the hor or vert
		 * expression is invisible, skip printing */
		if (expr_has_invis(printdata_p->location.hexpr_p) ||
				expr_has_invis(printdata_p->location.vexpr_p)) {
			continue;
		}

		/* Headers and footers aren't in the main list, and thus their
		 * expression don't get evaluated during locvar.c, so make
		 * sure that has been done. */
		eval_coord( &(printdata_p->location), printdata_p->inputfile,
					printdata_p->inputlineno);

		/* get coordinate of string */
		x = inpc_x( &(printdata_p->location),
			printdata_p->inputfile, printdata_p->inputlineno );
		y = inpc_y( &(printdata_p->location),
			 printdata_p->inputfile, printdata_p->inputlineno );

		/* Check if to use the mirrored string */
		if (printdata_p->mirror_p != 0 && Curr_pageside == PGSIDE_LEFT) {
			string_data_p = printdata_p->mirror_p;
		}
		else {
			string_data_p = printdata_p;
		}

		/* justify as specified */
		switch (printdata_p->justifytype) {

		case J_RIGHT:
			x -= string_data_p->width;
			break;

		case J_CENTER:
			if (has_align_point(string_data_p->string) == YES) {
				x -= center_left_width(string_data_p->string);
			}
			else {
				x -= string_data_p->width / 2.0;
			}
			break;

		default:
			break;
		}

		/* Note that mirroring is only done for "title,"
		 * which can never have isPostScript set, so we don't have to
		 * deal with mirroring in the special PostScript code.  */
		if (printdata_p->isPostScript) {
			outop(O_SAVE);
			do_moveto(x, y);
			/* export any requested Mup variables */
			export(printdata_p->export_p);
			
			if (printdata_p->isfilename) {
				FILE *psfile;
#ifdef O_BINARY
				char *mode = "rb";
#else
				char *mode = "r";
#endif

				if ((psfile = fopen(printdata_p->string + 2, mode)) == NULL) {
					l_yyerror(printdata_p->inputfile,
					printdata_p->inputlineno,
					"cannot open file '%s'",
					printdata_p->string);
				}
				else {
					char buff[BUFSIZ];

					while (fgets(buff, sizeof(buff), psfile) != 0) {
						OUTP(("%s", buff));
					}
					fclose(psfile);
				}
			}
			else {
				OUTP(("%s\n", printdata_p->string + 2));
			}
			outop(O_RESTORE);
			do_moveto(x, y);
			continue;
		}

		/* print the string at proper place */
		pr_wstring(x, y, string_data_p->string,
					printdata_p->justifytype,
					string_data_p->width,
					DEFHORZSCALE,
					string_data_p->inputfile,
					string_data_p->inputlineno);
	}
}


/* Export Mup variables to user's PostScript code */

static void
export(export_p)

struct VAR_EXPORT *export_p;	/* list of Mup variables to export */

{
	float *coord_p;	/* the c[] associated with the current variable */
	char *name;	/* name to use for PostScript */

	for (   ; export_p != 0; export_p = export_p->next) {
		name = export_p->alias->name;
		if (export_p->index == EXPORT_ALL_STAFFS) {
			int s;
			char *staffname;

			/* Add room for dot, 2 digit number, null, and
			 * then a few more to be extra safe. */
			MALLOCA(char, staffname, strlen(name) + 8);
			for (s = 1; s <= Score.staffs; s++) {
				/* Skip invisibles */
				if (svpath(s, VISIBLE)->visible == NO) {
					continue;
				}
				(void) sprintf(staffname, "_staff.%d", s);
				if ((coord_p = symval(staffname, 0)) == 0) {
					/* Should have been caught at parse time */
					pfatal("unable to find symbol %s", staffname);
				}
				(void) sprintf(staffname, "%s.%d", name, s);
				exportc(staffname, coord_p);
			}
			FREE(staffname);
			continue;
		}
		if ((coord_p = export_p->tag_addr) == 0) {
			/* Should have been caught at parse time */
			pfatal("unable to find symbol %s", export_p->name);
		}

		/* NUMCTYPE means export all six [nsewxy] */
		if (export_p->index == NUMCTYPE) {
			exportc(name, coord_p);
		}
		else {
			export1(name, coord_p, export_p->alias->index, export_p->index);
		}
	}
}

/* Export all 6 values of given coord, using the given name */

static void
exportc(name, coord_p)

char *name;
float *coord_p;

{
	export1(name, coord_p, AN, AN);
	export1(name, coord_p, AS, AS);
	export1(name, coord_p, AE, AE);
	export1(name, coord_p, AW, AW);
	export1(name, coord_p, AX, AX);
	export1(name, coord_p, AY, AY);
}


/* Export one variable. _tag.x becomes /Mup_tag.x etc. Or if they did an
 * alias, like _foo.x = _staff1.w that becomes /Mup_foo.x but contains
 * the value of _staff1.w */

static void
export1(name, coord_p, alias_index, tag_index)

char *name;	/* Mup tag name */
float *coord_p;	/* a c[] array */
int alias_index;	/* AX, AW, etc  that is to be used for the .direction */
int tag_index;		/* AX, AW, etc that is to be used in c[] */

{
	char * label;

	switch (alias_index) {
	case AN: label = "n"; break;
	case AS: label = "s"; break;
	case AE: label = "e"; break;
	case AW: label = "w"; break;
	case AX: label = "x"; break;
	case AY: label = "y"; break;
	default: label= "unknown"; /* to keep lint happy */
		pfatal("invalid index %d in export1", alias_index);
		/*NOTREACHED*/
		break;
	}
	OUTP(("/Mup%s.%s ", name, label));
	outcoord(coord_p[tag_index]);
	OUTP((" def\n"));
}


/* Returns YES if the given expression contains at least one reference to
 * a tag that is associated with something that is invisible, else NO. */

static int
expr_has_invis(expr_p)

struct EXPR_NODE *expr_p;	/* check this expression for invisible tags */

{
	struct COORD_INFO *info_p;


	if (expr_p == 0) {
		/* end recursion */
		return(NO);
	}

	/* For binary and unary operators, follow their children down
	 * to leaf nodes. */
	if ( (expr_p->op & OP_BINARY) == OP_BINARY) {
		if (expr_has_invis(expr_p->left.lchild_p) == YES ||
				expr_has_invis(expr_p->right.rchild_p) == YES) {
			return(YES);
		}
	}
	if ( (expr_p->op & OP_UNARY) == OP_UNARY) {
		if (expr_has_invis(expr_p->left.lchild_p) == YES) {
			return(YES);
		}
	}

	/* The only leaf node type we care about here is a tag reference */
	if (expr_p->op != OP_TAG_REF) {
		return(NO);
	}

	if ((info_p = find_coord(expr_p->left.ltag_p->c)) != 0 &&
			(info_p->flags & CT_INVISIBLE) ) {
		return(YES);
	}
	return(NO);
}


/* Print clefs, time signature and key signatures, and
 * return widest width of everything printed. If really_print == NO,
 * just pretend to print; this is used to obtain the width.
 * Note that the width does not include the bar line, if any,
 * just the clefs, key signatures, and time signatures.
 * If really_print == NO then mll_p is allowed to be null.
 */

double
pr_clefsig(mll_p, clefsig_p, really_print)

struct MAINLL *mll_p;	/* clefsig is connected here */
struct CLEFSIG *clefsig_p;	/* which clef, etc to print */
int really_print;	/* if YES actually print, otherwise just being called to
			 * see how wide the stuff would be if we printed it */

{
	register int s;		/* walk through staffs */
	float itemwidth;	/* width of item just printed */
	float maxclefwidth, maxkswidth;	/* width of clef & time sig */
	float tsigwidth;	/* width of time signature */
	float curr_tsigwidth;	/* width of current time signature */
	float total_width;	/* with of clef + time sig + barline */
	float bar_width;	/* if mid-score clefsig, the clef goes before
				 * the bar line */
	float stscale;		/* staffscale of current staff */
	float biggest_stscale;	/* padding for various things should be based
				 * on the largest staffscale of any staff */
	struct MAINLL *m_p;	/* for finding preceding bar */
	int clefsize;		/* mid-score clefs are 3/4 normal size */
	int looked_ahead = NO;	/* If looked ahead for SSVs */
	double clefx;		/* where to place clef */



	if ((Score_location_p == (float *) 0) && (really_print == YES) ) {
		pfatal("can't do clef/key/time: no feed");
	}

	/* have to print clefs, time sigs and key sigs in separate
	 * loops since we need to find the widest of each and start
	 * the next after that on all staffs so things line up nicely */

	/* if this clefsig is hidden because user specified "hidechanges,"
	 * there is nothing to print, and the width of what was printed is 0.0 */
	if (clefsig_p->hide == YES) {
		return(0.0);
	}

	/* init bar_width for now; if needed we will calculate a
	 * value below */
	bar_width = 0.0;

	if (clefsig_p->clefsize == SMALLSIZE) {
		/* Back up looking for bar and get its width. */
		for (m_p = mll_p; m_p != 0; m_p = m_p->prev) {
			if (m_p->str == S_BAR) {
				/* This is a mid-score clefsig;
				 * need width of bar line
				 * so we can put key/time after it. */
				bar_width = width_barline(m_p->u.bar_p);
				break;
			}
		}
	}

	/* Go through all the staffs, printing clefs. Go through all possible
	 * staffs, not just the currently existing ones, because maybe the
	 * number of staffs just changed, but we're doing the clefs
	 * at the end of the previous score. */
	biggest_stscale = MINSTFSCALE;
	for (s = 1, maxclefwidth = 0.0; s <= MAXSTAFFS; s++) {

		/* if staff is invisible, nothing to do */
		if ( (svpath(s, VISIBLE))->visible == NO) {
			continue;
		}

		if ((stscale = svpath(s, STAFFSCALE)->staffscale)
							> biggest_stscale) {
			biggest_stscale = stscale;
		}

		if (really_print == YES && Staffs_y[s] == 0.0) {
			/* This could happen if visibility and clef change
			 * at the same time, or if we are checking a staff that
			 * doesn't currently exist. (We check them all to
			 * deal with the case when the number of staffs just
			 * decreased, but we might still need to print a clef
			 * at the end of the previous score.)
			 * Without this continue, a clef
			 * will appear halfway off the bottom of the page */
			continue;
		}

		/* if no clef is to be printed, don't print one */
		if ( (svpath(s, STAFFLINES))->printclef == SS_NOTHING) {
			continue;
		}

		/* If there is a BLOCK, there could be clefsig changes
		 * following that that could apply to courtesy clefsigs,
		 * so look ahead for those. */
		if (mll_p != 0 && mll_p->next != 0) {
			m_p = 0;
			if (mll_p->next->str == S_BLOCKHEAD) {
				/* This can happen if placement is trying
				 * to add a FEED but hasn't yet */
				m_p = mll_p->next->next;
			}
			else if (mll_p->next->str == S_FEED
					&& mll_p->next->next != 0
					&& mll_p->next->next->str
					== S_BLOCKHEAD) {
				m_p = mll_p->next->next->next;
			}
			for (   ; m_p != 0; m_p = m_p->next) {
				if (m_p->str == S_SSV) {
					asgnssv(m_p->u.ssv_p);
					looked_ahead = YES;
				}
				else {
					break;
				}
			}
		}
		/* print clef if necessary */
		if (clefsig_p->prclef[s] == YES) {
			set_staffscale(s);
			/* mid-staff clefs should be 3/4 as big as normal */
			if (clefsig_p->clefsize == SMALLSIZE) {
				int clefcode;
				int cleffont;

				clefsize = (3 * DFLT_SIZE) / 4;
				/* right justify mid-score clefs */
				clefcode = clefchar(svpath(s, CLEF)->clef,
							s, &cleffont);
				clefx = clefsig_p->wclefsiga +
						(clefsig_p->widestclef -
						Staffscale *
						width(cleffont, clefsize,
						clefcode));
			}
			else {
				clefsize = DFLT_SIZE;
				clefx = clefsig_p->wclefsiga;
			}
			itemwidth = pr_clef(s, clefx, really_print, clefsize);
			if (itemwidth > maxclefwidth) {
				maxclefwidth = itemwidth;
			}
		}
	}

	/* allow a little space before key/time signature */
	if (maxclefwidth > 0.0 && clefsig_p->clefsize != SMALLSIZE) {
		maxclefwidth += CLEFPAD * biggest_stscale;
	}

	/* print key sig if necessary */
	for (s = 1, maxkswidth = 0.0; s <= MAXSTAFFS; s++) {

		/* if staff is invisible, nothing to do */
		if ( (svpath(s, VISIBLE))->visible == NO) {
			continue;
		}

		/* if no clef is to be printed, don't print key sig either */
		if ( (svpath(s, STAFFLINES))->printclef != SS_NORMAL) {
			continue;
		}

		if (really_print == YES && Staffs_y[s] == 0.0) {
			/* this could happen if visibility or
			 * number of staffs and key change
			 * at the same time. Without this continue, a keysig
			 * will appear halfway off the bottom of the page */
			continue;
		}

		if (clefsig_p->sharps[s] != 0 || clefsig_p->naturals[s] != 0) {
			set_staffscale(s);
			itemwidth = pr_keysig(s, clefsig_p->sharps[s],
				clefsig_p->naturals[s],
				(double) (clefsig_p->wclefsiga + maxclefwidth
				+ bar_width), really_print);
			if (itemwidth > maxkswidth) {
				maxkswidth = itemwidth;
			}
		}
	}
	/* If there was a keysig, add some padding after it */
	if (maxkswidth > 0.0) {
		maxkswidth += 2.0 * STDPAD * biggest_stscale;
	}

	total_width = maxclefwidth + maxkswidth;

	/* print time sig if necessary */
	tsigwidth = 0.0;
	if (clefsig_p->prtimesig == YES) {

		for (s = 1; s <= MAXSTAFFS; s++) {

			/* if staff is invisible, nothing to do */
			if ( (svpath(s, VISIBLE))->visible == NO) {
				continue;
			}

			if (really_print == YES && Staffs_y[s] == 0.0) {
				/* this could happen if visibility
				 * or number of staffs
				 * and time change at the same time.
				 * Without this continue, a time signature
				 * will appear halfway off the bottom
				 * of the page */
				continue;
			}

			set_staffscale(s);
			curr_tsigwidth = pr_timesig(s,
				(double) (clefsig_p->wclefsiga + bar_width +
				+ total_width), clefsig_p->multinum,
				really_print);

			/* if widest time signature found so far,
			 * save its width */
			if (curr_tsigwidth > tsigwidth) {
				tsigwidth = curr_tsigwidth;
			}
		}

		/* Add up width so far. Add 2 STDPADs after time sig */
		if ( tsigwidth > 0.0) {
			total_width += tsigwidth +
					(2.0 * STDPAD * biggest_stscale);
		}
	}

	/* do pseudo-bar things */
	if (clefsig_p->bar_p != (struct BAR *) 0) {

		if (clefsig_p->bar_p->bartype != INVISBAR) {

			if (really_print == YES) {
				pr_bar(mll_p, (double)
					(clefsig_p->wclefsiga + total_width
					+ (width_barline(clefsig_p->bar_p) / 2.0
					)), YES);
			}
			total_width += width_barline(clefsig_p->bar_p);
		}
		if (really_print == YES) {
			/* save pedal info needed to deal with endings */
			saveped(mll_p, clefsig_p->bar_p);
		}
	}

	if (looked_ahead == YES) {
		/* If we had to look ahead and assign SSVs to get proper
		 * courtesy clef/time sig before a block,
		 * make sure the SSVs are right. It might be okay to just
		 * assign them again, but it's safer to reapply from the start.
		 * This is an extremely rare case, so the extra time is okay.
	 	 */
		setssvstate(mll_p);
	}

	return(total_width);
}


/* print a clef on specified staff */
/* return the width of what was printed */

double
pr_clef(staffno, x, really_print, size)

int staffno;		/* which staff to print clef on */
double x;		/* x coord */
int really_print;	/* if YES, actually print, else just return width */
int size;		/* point size of clef */

{
	int muschar;	/* clef character */
	int cleffont;
	float y_offset;	/* where to place clef vertical relative to staff */
	int clef;
	float y;


	/* the "drum" clef is handled specially */
	if (svpath(staffno, STAFFLINES)->printclef == SS_DRUM) {
		if (really_print == YES) {
			/* draw 2 vertical medium lines */
			do_linetype(L_NORMAL);
			y = Staffs_y[staffno];
			y_offset = 2.5 * Stepsize;
			x += 2.0 * Stepsize;
			draw_line(x, y - y_offset, x, y + y_offset);
			x += 0.7 * Stepsize;
			draw_line(x, y  - y_offset, x, y + y_offset);
		}
		return (5.0 * Stepsize);
	}

	/* figure out which clef to use */
	clef = svpath(staffno, CLEF)->clef;
	muschar = clefchar(clef, staffno, &cleffont);

	/* figure out vertical placement */
	if (clef == TABCLEF) {
		return(pr_tabclef(staffno, x, really_print, size));
	}

	y_offset = clefvert(clef, staffno, NO, 0, 0) * STEPSIZE;

	/* print the clef */
	if (really_print) {
		x += (width(cleffont, size, muschar) / 2.0
						+ CLEFPAD) * Staffscale;
		y = Staffs_y[staffno] + y_offset * Staffscale;
		/* Print 8 below or above a G clef clef in 9-point
		 * for treble8 or 8treble. Similarly, print 8 below
		 * or above an F clef for bass8 or 8bass. */
		if (clef == TREBLE_8 || clef == TREBLE_8A ||
				clef == BASS_8 || clef == BASS_8A) {
			double y8;
			char tr8str[4];
			double x_adj;

			tr8str[0] = FONT_TB;
			/* 10-point, but adjusted by staffscale */
			tr8str[1] = (char) adj_size(10, Staffscale,
								(char *) 0, -1);
			tr8str[2] = '8';
			tr8str[3] = '\0';
			/* Figure out correct y offset */
			if (clef == TREBLE_8) {
				y8 = y - descent(cleffont, size, muschar)
					* Staffscale
					- strascent(tr8str) + (2.0 * Stdpad);
			}
			else if (clef == BASS_8) {
				y8 = y - descent(cleffont, size, muschar)
					* Staffscale
					- strascent(tr8str) + (0.5 *Stdpad);
			}
			else {
				y8 = y + ascent(cleffont, size, muschar)
						* Staffscale - Stdpad;
			}

			/* Figure out correct x offset */
			switch (clef) {
			case TREBLE_8:
				x_adj = - Stepsize;
				break;
			case TREBLE_8A:
				x_adj = 0.0;
				break;
			case BASS_8:
			case BASS_8A:
				x_adj = -1.5 * Stepsize;
				break;
			default:
				/* This will never be hit, but silences bogus
				 * warning for "may be used
				 * without being set. */
				x_adj = 0.0;
				break;
			}
			j_outstring(x + x_adj, y8, tr8str, J_CENTER, strwidth(tr8str),
					DEFHORZSCALE, (char *) 0, -1);
		}
		pr_muschar(x, y, muschar, size, cleffont);
	}

	return (width(cleffont, size, muschar) + CLEFPAD) * Staffscale;
}


/* print key signature on specified staff */
/* return the width of what was printed */

/* below is a table for relative y location of sharp/flat/natural symbols. For
 * each clef type, tell how many steps up or down to put each */

/* Std_* is the standard pattern for treble clef and is also the basic
 * pattern for several other clefs although shifted vertically */
static int Std_sharps_pattern[] = { 4, 1, 5, 2, -1, 3, 0 };
static int Std_flats_pattern[] = { 0, 3, -1, 2, -2, 1, -3 };

/* for some clefs, the standard patterns don't work, so use alternate */
static int Alt_sharps_pattern[] = { -1, 3, 0, 4, 1, 5, 2 };
static int Alt_flats_pattern[] = { 4, 0, 3, -1, 2, -2, 1 };
/* special version for baritone and soprano clef */
static int Alt2_sharps_pattern[] = { 0, 4, 1, -2, 2, -1, -4 };


static double
pr_keysig(staffno, sharps, naturals, x, really_print)

int staffno;		/* which staff to print on */
int sharps;		/* how many sharps in key signature */
int naturals;		/* how many naturals to print to cancel previous key */
double x;		/* coordinate */
int really_print;	/* if YES, actually print, else just return width */

{
	float y;			/* vertical location */
	int *sharptbl, *flattbl;	/* table of physical offsets */
	int offset;			/* to compensate for clef */


	if (sharps == 0 && naturals == 0) {
		return(0.0);
	}

	/* if just getting width, just calculate that */
	if (really_print == NO) {
		return(width_keysig(sharps, naturals));
	}

	y = Staffs_y[staffno];

	/* start out assuming standard patterns at standard place for
	 * treble clef. If a different clef, may have to use an
	 * alternate pattern and/or an additional offset */
	sharptbl = Std_sharps_pattern;
	flattbl = Std_flats_pattern;

	switch ( (svpath(staffno, CLEF))->clef ) {

	case TREBLE:
	case TREBLE_8:
	case TREBLE_8A:
	case SUBBASS:
		offset = 0;
		break;

	case FRENCHVIOLIN:
	case BASS:
	case BASS_8:
	case BASS_8A:
		offset = -2;
		break;

	case SOPRANO:
		if (sharps > 0 || naturals > 0) {
			sharptbl = Alt2_sharps_pattern;
			offset = -1;
		}
		if (sharps < 0 || naturals < 0) {
			flattbl = Alt_flats_pattern;
			offset = -2;
		}
		break;

	case MEZZOSOPRANO:
		if (sharps < 0 || naturals < 0) {
			flattbl = Alt_flats_pattern;
			offset = 0;
		}
		else {
			offset = -3;
		}
		break;

	case ALTO:
		offset = -1;
		break;

	case TENOR:
		if (sharps > 0 || naturals > 0) {
			sharptbl = Alt_sharps_pattern;
			offset = -1;
		}
		else {
			offset = 1;
		}
		break;

	case BARITONE:
		if (sharps < 0 || naturals < 0) {
			flattbl = Alt_flats_pattern;
			offset = -1;
		}
		if (sharps > 0 || naturals > 0) {
			sharptbl = Alt2_sharps_pattern;
			offset = 0;
		}
		break;

	case TABCLEF:
		return(0.0);

	default:
		pfatal("unknown clef %d in pr_keysig", (svpath(staffno, CLEF))->clef);
		/*NOTREACHED*/
		offset = 0;	/* to shut up bogus compiler warning */
		break;
	}

	set_cur(x, y);
	/* cancel a previous key signature of flats */
	if (naturals < 0) {
		draw_keysig(staffno, C_NAT, - naturals, (double) x, (double) y,
				flattbl, offset, (sharps < 0 ? -sharps : 0));
	}

	/* cancel a previous key signature of sharps */
	else if (naturals > 0 ) {
		draw_keysig(staffno, C_NAT, naturals, (double) x, (double) y,
				sharptbl, offset, (sharps > 0 ? sharps : 0));
	}
	/* if there were some naturals, add a little padding before the other */
	if (naturals != 0) {
		set_cur( _Cur[AX] + (3.0 * Stdpad), y);
	}

	/* do key signatures with sharps */
	if (sharps > 0) {
		draw_keysig(staffno, C_SHARP, sharps,
			(double) _Cur[AX], (double) y, sharptbl, offset, 0);
	}

	/* do key signatures with flats */
	else if (sharps < 0) {
		draw_keysig(staffno, C_FLAT, -sharps,
			(double) _Cur[AX], (double) y, flattbl, offset, 0);
	}

	/* return the width of what we printed */
	return( _Cur[AX] - x);
}


/* actually draw a key signature, given all the info about what and where
 * to do it */

static void
draw_keysig(staffno, muschar, symbols, x, y, table, offset, skip)

int staffno;
int muschar;	/* what to draw: C_SHARP, C_FLAT, or C_NAT */
int symbols;	/* how many to draw */
double x;	/* where to start putting them */
double y;	/* middle of staff */
int *table;	/* which pattern to use for drawing symbols */
int offset;	/* to compensate for clef */
int skip;	/* how many symbols to skip in pattern (for canceling key) */

{
	float compensation;	/* because mus char's x are in their middle */
	register int s;		/* index through number of symbols */
	float jam_factor;	/* how much to adjust to push things closer
				 * together. (Key signatures should be packed
				 * tighter than normal accidentals) */
	int font;


	_Cur[AX] = x;

	font = FONT_MUSIC;
	(void) get_shape_override(staffno, 1, &font, &muschar);

	/* have to compensate for music char's x being in its middle */
	compensation = width(font, DFLT_SIZE, muschar) * Staffscale / 2.0;

	/* just put each sharp or flat next to the previous one in the
 	 * x direction, except squeeze flats and sharps together by two points,
	 * and naturals by one point. */
	jam_factor = (muschar == C_NAT ? Stdpad : 2.0 * Stdpad);
	/* We only want the jam facter to happen *after* the first one,
	 * so cancel out the jam_factor on the first symbol by adding
	 * the value that will be subtracted in the loop below.
	 */
	if (symbols > 0) {
		set_cur(_Cur[AX] + jam_factor, y);
	}
	for (s = 0; s < symbols; s++) {
		pr_muschar( _Cur[AX] + compensation - jam_factor,
			y + ((table[s + skip] + offset) * Stepsize),
			muschar, DFLT_SIZE, font);
	}
}


/* print time signature on specified staff */
/* return width of what was printed */

static double
pr_timesig(staffno, x, multnum, really_print)

int staffno;		/* which staff to print on */
double x;		/* coordinate */
int multnum;		/* number of measures of multirest that follow */
int really_print;	/* if YES, actually print, else just return width */

{
	char numstr[MAXTSLEN * 3];	/* numerator as a string */
	char denstr[8];			/* denominator as a string */
	char plusstr[4];		/* plus sign as a string */
	char *timerep;			/* time representation to print */
	float numwidth, denwidth;	/* width of numstr and denstr */
	double thiswidth;		/* width of current fraction */
	double totalwidth;		/* width of entire time signature */
	double numjam, denjam;		/* certain 2-digit number look better
					 * if jammed together somewhat */
	char *t;			/* walk through timerep */
	double y;			/* y coordinate */


	if (is_tab_staff(staffno) == YES) {
		/* tab staffs never have a time signature */
		return(0.0);
	}

	if ( Score.timevis == PTS_NEVER ) {
		/* not visible */
		return(0.0);
	}

	numwidth = denwidth = thiswidth = totalwidth = numjam = denjam = 0.0;

	/* string version of numbers for time sig */
	numstr[0] = denstr[0] = plusstr[0] = FONT_NB;
	numstr[1] = denstr[1] = plusstr[1] = adj_size(16, Staffscale, (char *) 0, -1);
	numstr[2] = '\0';
	plusstr[2] = '+';
	plusstr[3] = '\0';

	if (svpath(staffno, PRINTEDTIME)->prtime_is_arbitrary == YES) {
		return(pr_arbitrary_tsig(staffno, x, really_print));
	}
	timerep = svpath(staffno, PRINTEDTIME)->prtime_str1;
	if (timerep == 0) {
		timerep = Score.timerep;
	}
	for (t = timerep; *t != TSR_END; t++) {

		if (*t == TSR_CUT || *t == TSR_COMMON) {
			int tschar;
			int tsfont;

			tschar = (*t == TSR_CUT ? C_CUT : C_COM);
			tsfont = FONT_MUSIC;
			(void) get_shape_override(staffno, 0, &tsfont, &tschar);
			thiswidth = width(tsfont, DFLT_SIZE, tschar) * Staffscale;
			totalwidth += thiswidth;
			if (really_print) {
				pr_muschar( x + totalwidth - (thiswidth / 2.0),
						Staffs_y[staffno], tschar,
						DFLT_SIZE, tsfont);
			}
		}

		else if (*t == TSR_SLASH) {
			t++;
			(void) sprintf(denstr + 2, "%d", *t);
			denjam = tsjam(*t);
			denwidth = strwidth(denstr) - denjam;
			numwidth = strwidth(numstr) - numjam;
			thiswidth = MAX(numwidth, denwidth);
			if (really_print) {
				double xx;
				char onenum[8];	/* one component of numerator */
				int n;		/* index into numstr */

				/* print numerator */
				xx = x + totalwidth +
						(thiswidth - numwidth)/2.0;
				y = Staffs_y[staffno];
				onenum[0] = numstr[0];
				onenum[1] = numstr[1];
				for (n = 2; numstr[n] != '\0'; n++) {

					if (numstr[n] == '+') {
						pr_string(xx, y + 2.0 * Stdpad,
							plusstr, J_LEFT,
							(char *) 0, -1);
						xx = _Cur[AX];
						continue;
					}

					onenum[2] = numstr[n];
					if (isdigit(numstr[n+1])) {
						onenum[3] = numstr[++n];
						onenum[4] = '\0';
					}
					else {
						onenum[3] = '\0';
					}
					pr_tsnum(xx, y, onenum, tsjam(atoi(onenum + 2)));
					xx = _Cur[AX];
				}

				/* print denominator */
				y = Staffs_y[staffno] - strheight(denstr)
							+ (2.0 * Stdpad);
				pr_tsnum(x + totalwidth +
						(thiswidth - denwidth)/2.0, y,
						denstr, denjam);

			}
			totalwidth += thiswidth;

			/* Reset things in case there is another
			 * time signature component */
			numwidth = denwidth = 0.0;
			numstr[2] = denstr[2] = '\0';
			numjam = 0.0;
		}

		else if (*t == TSR_ALTERNATING) {
			if (Score.timevis == PTS_ALWAYS) {
				/* In this mode, we print alternating
				 * time signature on each measure
				 * explicitly, so only print the current,
				 * except if for multirest, in which case
				 * we print the lesser of the number of
				 * alternate time signatures and the
				 * number of measures of multirest. */
				if (--multnum <= 0) {
					break;
				}
			}

			/* add some space */
			/* reuse the numstr  */
			numstr[2] = ' ';
			numstr[3] = '\0';
			numwidth = strwidth(numstr);
			if (really_print) {
				pr_string(x + totalwidth,
					Staffs_y[staffno] - strheight(numstr)/2.0,
					numstr, J_LEFT, (char *) 0, -1);
			}
			totalwidth += numwidth;
			/* reset for the next numerator */
			numstr[2] = '\0';
			numwidth = 0.0;
		}

		else if (*t == TSR_ADD) {
			if (really_print) {
				pr_string(x + totalwidth,
					Staffs_y[staffno]
					- strheight(plusstr)/2.0 + 1.5 * Stdpad,
					plusstr, J_LEFT, (char *) 0, -1);
			}
			totalwidth += strwidth(plusstr);
		}

		else {
			/* If first denominator number, use as is,
			 * otherwise have to add a plus sign first */
			if (numstr[2] != '\0') {
				(void) strcat(numstr, "+");
			}
			(void) sprintf(numstr + strlen(numstr), "%d", *t);
			numjam += tsjam(*t);
		}
	}

	return (totalwidth);
}


static double
pr_arbitrary_tsig(staffno, x, really_print)

int staffno;		/* which staff to print on */
double x;		/* coordinate */
int really_print;	/* if YES, actually print, else just return width */

{
	char *str1;
	char *str2;
	double adjust;	/* vertical from staff center line to string baseline */
	int size;

	if ((str1 = svpath(staffno, PRINTEDTIME)->prtime_str1) == 0) {
		pfatal("null printed time string");
	}

	if ((str2 = svpath(staffno, PRINTEDTIME)->prtime_str2) == 0) {
		double width;

		/* May need to adjust for staffscale, so make a copy
		 * and normalize that based on staffscale. */
		str1 = strdup(str1);
		if (is_tsig_symbol(str1) == YES) {
			size = adj_size(str1[1], Staffscale, (char *) 0, -1);
			str1 = fix_string(str1, str1[0], size, (char *) 0, -1);
		}
		else {
			size = adj_size(24, Staffscale, (char *) 0, -1);
			str1 = fix_string(str1, FONT_NB, size, (char *) 0, -1);
		}
		
		/* only one string, center it vertically */
		if (really_print == YES) {
			adjust = (strheight(str1) / 2.0) - strdescent(str1);
			pr_string(x, Staffs_y[staffno] - adjust, str1,
						J_LEFT, (char *) 0, -1);
		}
		width = strwidth(str1);
		free(str1);
		return(width);
	}
	else {
		double width1;
		double width2;
		double widest;

		str1 = strdup(str1);
		size = adj_size(14, Staffscale, (char *) 0, -1);
		str1 = fix_string(str1, FONT_NB, size, (char *)0, -1);
		str2 = strdup(str2);
		size = adj_size(14, Staffscale, (char *) 0, -1);
		str2 = fix_string(str2, FONT_NB, size, (char *)0, -1);

		/* two strings; stack them vertically */
		width1 = strwidth(str1);
		width2 = strwidth(str2);
		widest = MAX(width1, width2);
		if (really_print == YES) {
			adjust = strdescent(str1);
			pr_string(x + (widest - width1) / 2.0,
				Staffs_y[staffno] + adjust,
				str1, J_LEFT, (char *) 0, -1);
			adjust = strascent(str2);
			pr_string(x + (widest - width2) / 2.0,
				Staffs_y[staffno] - adjust - STDPAD,
				str2, J_LEFT, (char *) 0, -1);
		}
		free(str1);
		free(str2);
		return(widest);
	}
	return(0.0);
}


/* Returns YES if the given string consists solely of something that is
 * normally used as a time signature symbol.
 */

/* The remainder of the string to be matched if we see it starts with \( */
static char *Tsig_symbols[] = {
	"cut)",
	"com)",
	"perfminor)",
	"perfmaior)",
	"imperfminor)",
	"imperfmaior)",
	"perfminordim)",
	"perfmaiordim)",
	"imperfminordim)",
	"imperfmaiordim)"
};

static int
is_tsig_symbol(str)

char *str;

{
	int s;

	if (str[2] != '\\' || str[3] != '(') {
		return(NO);
	}

	for (s = 0; s < NUMELEM(Tsig_symbols); s++) {
		if (strcmp(str+4, Tsig_symbols[s]) == 0) {
			return(YES);
		}
	}
	return(NO);
}


/* Return the amount by which to jam the digits of a time signature number
 * together. Could be zero (if a 1-digit number or a number that doesn't
 * need jamming).
 */

static double
tsjam(num)

int num;

{
        if (num >= 10 && num < 20) {
		if (num == 12) {
			return(Stdpad);
		}
		else {
			return(2.0 * Stdpad);
		}
	}
        return (0.0);
}



/* print a number that is part of a time signature. The number is passed
 * as a string in str, and is to be printed as the given (x,y). Some 2-digit
 * numbers look better if jammed together somewhat, so if jam is non-zero,
 * jam them by that much, else just print the str as is.
 */

static void
pr_tsnum(x, y, str, jam)

double x;
double y;
char *str;
double jam;

{
	char save;

	if (jam > 0.0) {
		/* split and print 1 digit at a time */
		save = str[3];
		str[3] = '\0';
		pr_string(x, y, str, J_LEFT, (char *) 0, -1);
		str[2] = save;
		pr_string(_Cur[AX] - jam, y, str, J_LEFT, (char *) 0, -1);
	}
	else {
		pr_string(x, y, str, J_LEFT, (char *) 0, -1);
	}
}


/* print a string */

void
pr_string(x, y, string, justify, fname, lineno)

double x, y;	/* where to put it */
char *string;	/* what to print */
int justify;	/* J_LEFT, etc */
char *fname;	/* file name for error messages */
int lineno;	/* line number for error messages */

{
	/* This function is now just a wrapper that passes its arguments
	 * pass to a more general function. The added -1.0 argument says
	 * to not spread out for right justified paragraph. */
	pr_wstring(x, y, string, justify, -1.0, DEFHORZSCALE, fname, lineno);
}

/* This print a horizontally scrunched string */

void
pr_scrunched_string(x, y, string, justify, horzscale, fname, lineno)

double x, y;	/* where to put it */
char *string;	/* what to print */
int justify;	/* J_LEFT, etc */
double horzscale;	/* how much to scrunch */
char *fname;	/* file name for error messages */
int lineno;	/* line number for error messages */

{
	pr_wstring(x, y, string, justify, -1.0, horzscale, fname, lineno);
}
/* more general string printing function that handles right justified paragraphs */

static void
pr_wstring(x, y, string, justify, fullwidth, horzscale, fname, lineno)

double x, y;	/* where to put it */
char *string;	/* what to print */
int justify;	/* J_LEFT, etc */
double fullwidth;	/* width to use, or negative value to use strwidth */
double horzscale;	/* how much to squeeze horizontally */
char *fname;	/* file name for error messages */
int lineno;	/* line number for error messages */

{
	/* skip any empty strings */
	if ( ( string == (char *) 0) || (*string == '\0') ) {
		return;
	}

	/* set font and size */
	pr_font( (int) string[0], (int) string[1]);

	if (IS_BOXED(string) == YES) {
		/* The strheight and width already include the box dimension,
		 * so print the box of that size. Then adjust the x of
		 * the string so it will print at the right place
		 * inside the box. */
		pr_box(x + 1.5 * STDPAD, y - strdescent(string) + 3.0 * STDPAD,
				strheight(string) - 5.0 * STDPAD,
				strwidth(string) - (1.5 * STDPAD));


		x += 3.5 * STDPAD;
	}
	if (IS_CIRCLED(string) == YES) {
		float circ_height;
		float circ_width;
		float elongation_factor;
		float x_offset;
		float radius;
		float x_center, y_center;

		/* determine where to place the circle and its contents */
		(void) circled_dimensions(string, &circ_height, &circ_width,
						(float *) 0, &x_offset);
		x_center = x + circ_width / 2.0;
		y_center = y + strascent(string) - strheight(string) / 2.0;

		/* we will fiddle with the transform matrix so do inside
		 * save/restore */
		outop(O_GSAVE);
		outop(O_NEWPATH);

		/* draw the outer elipse */
		elongation_factor = circ_width / circ_height;
		radius = strheight(string) / 2.0;
		do_scale(elongation_factor, 1.0);
		draw_circle(x_center / elongation_factor, y_center, radius);

		/* undo the outer elongation, and set for inner */
		do_scale(1.0 / elongation_factor, 1.0);
		elongation_factor = (circ_width - 1.5 * Stdpad)
					/ (circ_height - 1.5 * Stdpad);
		do_scale(elongation_factor, 1.0);

		/* the inner circle's radius is smaller than outer */
		radius = radius - 0.5 * Stdpad;
		draw_circle(x_center / elongation_factor, y_center, radius);

		/* fill in the area between the inner and outer elipses */
		outop(O_EOFILL);
		outop(O_GRESTORE);

		/* adjust x for where text should be printed, if needed */
		switch (justify) {
		case J_LEFT:
			x += x_offset;
			break;
		case J_RIGHT:
			x -= x_offset;
			break;
		}
	}

	split_a_string(x, y, string, justify, fullwidth, horzscale,
							fname, lineno);
}


/* Draw a circle (or maybe elipse, if scaling is in effect) */

static void
draw_circle(x, y, radius)

double x, y;	/* of circle center */
double radius;

{
	outcoord(x);
	outcoord(y);
	outcoord(radius);
	outint(0);
	outint(360);
	outop(O_ARC);
}


/* output instructions for setting font and size */

static void
pr_font(font, size)

int font;
int size;

{
#ifdef SMALLMEMORY
	/* if memory is scarce, every time we do a new font,
	 * do it in a separate save context */
	if (Did_save == YES) {
		outop(O_RESTORE);
	}
	outop(O_SAVE);
	Did_save = YES;
#endif

	Curr_font = font;
	Curr_size = size;

	prfontname(font);

	outop(O_FONT);

	outint(size);
	outop(O_SIZE);
	outop(O_SETFONT);

	Fontinfo[font_index(font)].was_used = YES;
}


/* print font name */

static void
prfontname(font)

int font;
{
	OUTP(("/%s ", Fontinfo[font_index(font)].ps_name));
}


/* Functions to turn scrunching on and off. This makes things narrower, used
 * in an effort to shoehorn something into a space it is too wide for.
 * This only adjusts horizontal scaling by the given factor. */

static void
scrunch(horzscale)

double horzscale;	/* how much to scale horizontally */

{
	if (horzscale != DEFHORZSCALE) {
		outop(O_GSAVE);
		do_scale(horzscale, 1.0);
	}
}


static void
unscrunch(horzscale)

double horzscale;	/* how much to scale horizontally when scrunching */
 
{
	if (horzscale != DEFHORZSCALE) {
		outop(O_GRESTORE);
	}
}


/* split a string into lines and print each line */

static void
split_a_string(x, y, string, justify, fullwidth, horzscale, fname, lineno)

double x;		/* coordinate at which to print string */
double y;
char *string;		/* what string to print */
int justify;		/* J_LEFT, etc */
double fullwidth;	/* width of (possibly multi-line) string, or -1.0
			 * if the string width should be used. */
double horzscale;	/* how much to squeeze horizontally */
char *fname;		/* file name for error messages */
int lineno;		/* line number for error messages */

{
	int font, size;		/* current font and size */
	int origfont, origsize;	/* font & size at beginning of current line
				 * of text */
	char *text;		/* beginning of text of current line */
	char *p;		/* pointer to current place in string */
	int c;			/* character read from string */
	char *buff;		/* temporary copy of one line of string */


	origfont = font = string[0];
	origsize = size = string[1];
	text = string + 2;

	/* if centering or right justifying, will need width of entire
	 * (possibly multi-line) string, to adjust lines within the string */
	if (fullwidth < 0.0) {
		fullwidth = strwidth(string);
	}

	if (IS_BOXED(string) == YES) {
		/* The box printing is dealt with in pr_string(), so we
		 * can ignore the BOX commands here (and need to, in order
		 * to make things align properly). */
		text++;
		fullwidth -= 7.0 * STDPAD;
	}
	if (IS_CIRCLED(string) == YES) {
		/* Similarly, skip past the circle indicator, so it
		 * doesn't affect the string width calculation. */
		text++;
	}
	p = text;
	MALLOCA(char, buff, strlen(string) + 1);
	do {
		c = next_str_char(&p, &font, &size);
		if (c == '\n' || c == '\0') {
			/* end of line. Print this line. Put into
			 * temporary buffer in case more than one line */
			buff[0] = (char) origfont;
			buff[1] = (char) origsize;
			(void) memcpy(buff + 2, text, (unsigned) (p - text));
			buff[p - text + 2] = '\0';
			/* On final line of a justified paragraph, we don't
			 * want to stretch that line out, because it might
			 * only contain a couple words. */
			if (justify == J_JUSTPARA && c == '\0') {
				justify = J_LEFT;
			}

			j_outstring(x, y, buff, justify, fullwidth,
						horzscale, fname, lineno);

			/* prepare for next line, if any */
			origfont = font;
			origsize = size;
			text = p;
			y -= fontheight(font, size);
		}
	} while (c != '\0');
	FREE(buff);
}


/* output a string segment with specified justification. If J_LEFT, just
 * print given string at given x, y location. If J_CENTER, put half way
 * between x and (x + fullwidth). If J_RIGHT, print such that right edge
 * of string will be at (x + fullwidth) */

static void
j_outstring(x, y, string, justify, fullwidth, horzscale, fname, lineno)

double x;
double y;
char *string;		/* which string to print */
int justify;		/* J_LEFT, etc */
double fullwidth;	/* full width to allocate to string */
char *fname;		/* file name for error messages */
int lineno;		/* line number for error messages */
double horzscale;	/* how munch to squeeze horizontally */

{
	switch (justify) {
	case J_NONE:
		/* NONE is effectively the same as LEFT */
		/*FALLTHRU*/
	case J_LEFT:
	case J_RAGPARA:
		outstring(x, y, -1.0, horzscale, string, fname, lineno);
		break;
	case J_JUSTPARA:
		outstring(x, y, fullwidth, horzscale, string, fname, lineno);
		break;
	case J_CENTER:
		outstring(x + (fullwidth - strwidth(string)) / 2.0, y,
				-1.0, horzscale, string, fname, lineno);
		break;
	case J_RIGHT:
		outstring(x + fullwidth - strwidth(string), y, -1.0,
				horzscale, string, fname, lineno);
		break;
	default:
		pfatal("bad justification type");
		/*NOTREACHED*/
		break;
	}
}


/* given a MAINLL struct, find all the STAFF structs from there to the next
 * BAR, and fill in a table of the staff Y coordinates */

static void
set_staff_y(main_p)

struct MAINLL *main_p;

{
	int s;

	/* First initialize all to 0.0. This is so that if the Staffs_y
	 * array is accessed for a non-existent staff, we will be sure
	 * that it will be set to 0.0, which is the special value to mean
	 * non-existent. Otherwise, when the number of staffs decreases,
	 * an old value could get left around in the staff that went away. */
	for (s = 1; s <= MAXSTAFFS; s++) {
		Staffs_y[s] = 0.0;
	}

	for (   ; main_p != (struct MAINLL *) 0; main_p = main_p->next) {

		if (main_p->str == S_BAR) {
			/* reached end of list of staffs in this measure */
			return;
		}

		if (main_p->str == S_STAFF) {
			/* save y value of staff */
			Staffs_y[main_p->u.staff_p->staffno] = 
					main_p->u.staff_p->c[AY];
		}
	}
}


/* print measure number at beginning of score if user wants them */

static void
pr_meas_num(staffno, x)

int staffno;	/* which staff to possible put measure number on */
double x;	/* where to put measure number */

{
	float y_adj;	/* to avoid clefs */
	int clef;


	/* measure numbers only put on those staffs that have endings */
	if (has_ending(staffno) ) {

		/* print measure number if user wants them */
		if ( (svpath(staffno, MEASNUM)->measnum == MN_SCORE)
						&& (Meas_num > 1)) {

			/* construct the measure number string */
			char mnumstr[16];
			mnum_string(mnumstr, Meas_num);

			/* print it */
			if (is_tab_staff(staffno) == YES) {
				clef = TABCLEF;
			}
			/* If clef is not to be printed, use NOCLEF.
			 * (printclef shares the STAFFLINES used flag) */
			else if (svpath(staffno, STAFFLINES)->printclef == NO) {
				clef = NOCLEF;
			}
			else {
				clef = svpath(staffno, CLEF)->clef;
			}
			/* Figure out where to place the measure number
			 * vertically by calling clefspace to get
			 * the height of the clef on the current staff plus
			 * the height of the measure number, but ignoring
			 * the height of the clef above (if any), then
			 * subtract the ascent of the measure number to
			 * get the right baseline. */
			y_adj = halfstaffhi(staffno) +
				clefspace(NOCLEF, 1.0, clef, Staffscale, Meas_num)
				- strascent(mnumstr);
			pr_string(x + 1.5 * Stepsize,
					Staffs_y[staffno] + y_adj,
					mnumstr, J_LEFT, (char *) 0, -1);
		}
	}
}


/* tell PostScript about file and linenumber */

void
pr_linenum (inputfile, inputlineno)

char *inputfile;
int inputlineno;

{
	static char *fname = "";		/* keep track of current file
						 * name to only output it when
						 * it changes */
	char *str;				/* walk thru file name to
						 * add backslashes if needed */

	
	if (strcmp(fname, inputfile) != 0) {
		OUTPCH(('('));
		for (str = inputfile; *str != 0; str++) {
			switch(*str) {
			case '\\':
			case '(':
			case ')':
				OUTPCH(('\\'));
				/*FALLTHRU*/
			default:
				OUTPCH((*str));
				break;
			}
		}
		OUTP((") inputfile\n"));
		fname = inputfile;
	}
	OUTP(("%d linenum\n", inputlineno));
}


/* output the current scale factor */

static void
setscale()

{
	double adjust;

	adjust = Score.scale_factor * Score.musicscale;
	OUTP(("%f %f scale\n", adjust, adjust));
}


/* For debugging, this generates PostScript to draw colored bounding boxes
 * representing coordinates (the 13-element arrays called "c"
 * in various structs).
 */

static void
show_coord(coord_p, index)

float *coord_p;		/* which one to draw */
int index;		/* index into Bbox_list, to get colors to use */

{
	struct Bbox *bb_p;

	bb_p = &(Bbox_list[index]);
	outop(O_GSAVE);
	OUTP(("%d.%d %d.%d %d.%d setrgbcolor\n",
		bb_p->red / 100, bb_p->red % 100,
		bb_p->green / 100, bb_p->green % 100,
		bb_p->blue / 100, bb_p->blue % 100));
	if (bb_p->dash_on && bb_p->dash_off) {
		OUTP(("[%d %d] 0 setdash\n", bb_p->dash_on, bb_p->dash_off));
	}
	pr_box(coord_p[AW], coord_p[AS], coord_p[AN] - coord_p[AS], coord_p[AE] - coord_p[AW]);
	outop(O_GRESTORE);
}


/* The environment variable MUP_BB turns on drawing of bounding boxes around
 * things, for debugging.  This function checks if the variable is set,
 * and if so, parses it to see which subset of things to draw boxes for.
 * For example, MUP_BB=g just does grpsyls, whereas MUP_BB=gnc does grpsyls,
 * notes, and chords. Bbox_list gives the full list of possibilities.
 */

static void
prep_bbox()
{
	char *bb;
	int i;

	if ((bb = getenv("MUP_BB")) == 0) {
		/* user doesn't want bounding box debugging */
		return;
	}

	/* If a coordinate type's id is set in MUP_BB, set its corresponding
	 * flag bit */
	for (i = 0; i < NUMELEM(Bbox_list); i++) {
		if (strchr(bb, Bbox_list[i].id) != 0) {
			BB_SET(i);
		}
	}
}


/* To help with debugging the placement phase of Mup,
 * or just to help a user see why things are laid out as they are,
 * this function will draw colored bounding boxes around things that
 * have "coordinates." The environment variable MUP_BB control which,
 * if any, kinds of things this is done for. This function is given the
 * main list item at the end of a page. It backs up through the list, back to
 * the beginning of the page, generating PostScript code to cause
 * printing of relevant boxes. Doing this last on a page ensures the boxes
 * are drawn on top of other things, and is at least as easy as going forwards.
 */

static void
show_bounding_boxes(mll_p)

struct MAINLL *mll_p;	/* FEED for end of current page, or could be
			 * Mainlltc_p if at end of song. */

{
	int v;			/* voice/verse */
	int n;			/* NOTE index */
	struct GRPSYL *gs_p;
	struct CHORD *chord_p;
	struct STUFF *stuff_p;

	/* We are at bottom of main list for current page. Work upwards,
	 * printing any coords we find. */
	if (mll_p->str == S_FEED) {
		/* Skip this feed.
		 * We want to back up to previous page feed, if any */
		mll_p = mll_p->prev;
	}

	for (   ; mll_p != 0; mll_p = mll_p->prev) {
		switch (mll_p->str) {

		case S_BAR:
			if (BB_IS_SET(BB_BAR)) {
				show_coord(mll_p->u.bar_p->c, BB_BAR);
			}
			break;

		case S_FEED:
			if (BB_IS_SET(BB_FEED)) {
				show_coord(mll_p->u.feed_p->c, BB_FEED);
			}
			if (mll_p->u.feed_p->pagefeed == YES
							|| Feednumber == 1) {
				if (BB_IS_SET(BB_BLOCKHEAD)) {
					begin_non_music_adj();
					/* Do header/footer */
					if (Curr_pageside == PGSIDE_LEFT) {
						if (Feednumber == 1) {
							show_coord(Leftheader.c, BB_BLOCKHEAD);
							show_coord(Leftfooter.c, BB_BLOCKHEAD);
						}
						else {
							show_coord(Leftheader2.c, BB_BLOCKHEAD);
							show_coord(Leftfooter2.c, BB_BLOCKHEAD);
						}
						if (mll_p->u.feed_p->lefttop_p != 0){
							set_win_coord(mll_p->u.feed_p->lefttop_p->c);
							show_coord(mll_p->u.feed_p->lefttop_p->c, BB_BLOCKHEAD);
							set_win_coord(0);
						}
						if (mll_p->u.feed_p->leftbot_p != 0){
							set_win_coord(mll_p->u.feed_p->leftbot_p->c);
							show_coord(mll_p->u.feed_p->leftbot_p->c, BB_BLOCKHEAD);
							set_win_coord(0);
						}
					}

					else { /* right page */
						if (Feednumber == 1) {
							show_coord(Rightheader.c, BB_BLOCKHEAD);
							show_coord(Rightfooter.c, BB_BLOCKHEAD);
						}
						else {
							show_coord(Rightheader2.c, BB_BLOCKHEAD);
							show_coord(Rightfooter2.c, BB_BLOCKHEAD);
						}
						if (mll_p->u.feed_p->righttop_p != 0){
							set_win_coord(mll_p->u.feed_p->righttop_p->c);
							show_coord(mll_p->u.feed_p->righttop_p->c, BB_BLOCKHEAD);
							set_win_coord(0);
						}
						if (mll_p->u.feed_p->rightbot_p != 0){
							set_win_coord(mll_p->u.feed_p->rightbot_p->c);
							show_coord(mll_p->u.feed_p->rightbot_p->c, BB_BLOCKHEAD);
							set_win_coord(0);
						}
					}
					end_non_music_adj();
				}
				if (mll_p->u.feed_p->pagefeed == YES) {
					/* reached top of current page; we're done */
					return;
				}
			}
			break;

		case S_STAFF:
			if (mll_p->u.staff_p->visible == NO) {
				break;
			}

			/* show the staff itself */
			if (BB_IS_SET(BB_STAFF)) {
				show_coord(mll_p->u.staff_p->c, BB_STAFF);
			}

			/* Do groups and notes */
			if (BB_IS_SET(BB_GRPSYL) || BB_IS_SET(BB_NOTE)) {
				for (v = 0; v < MAXVOICES; v++) {

					if (vvpath(mll_p->u.staff_p->staffno,
							v+1, VISIBLE)->visible
							== NO) {
						/* Skip invisible voices */
						continue;
					}

					for (gs_p = mll_p->u.staff_p->groups_p[v];
							gs_p != 0;
							gs_p = gs_p->next) {
						if (BB_IS_SET(BB_GRPSYL)) {
							show_coord(gs_p->c, BB_GRPSYL);
						}
						if (gs_p->nnotes > 0 &&
								BB_IS_SET(BB_NOTE)) {
							for (n = 0; n < gs_p->nnotes; n++) {
								show_coord(gs_p->notelist[n].c, BB_NOTE);
							}
						}
					}
				}
			}

			/* now do lyrics */
			if (BB_IS_SET(BB_GRPSYL)) {
				for (n = 0; n < mll_p->u.staff_p->nsyllists; n++) {
					for  (gs_p = mll_p->u.staff_p->syls_p[n];
							gs_p != 0;
							gs_p = gs_p->next) {
						show_coord(gs_p->c, BB_GRPSYL);
					}
				}
			}

			/* do the other "stuff" */
			if (BB_IS_SET(BB_STUFF)) {
				for (stuff_p = mll_p->u.staff_p->stuff_p;
						stuff_p != 0;
						stuff_p = stuff_p->next) {
					show_coord(stuff_p->c, BB_STUFF);
				}
			}
			break;

		case S_CHHEAD:
			if (BB_IS_SET(BB_CHORD)) {
				for (chord_p = mll_p->u.chhead_p->ch_p; chord_p != 0;
						chord_p = chord_p->ch_p) {
					show_coord(chord_p->c, BB_CHORD);
				}
			}
			break;

		default:
			break;
		}
	}

	/* Now do the special coords just for debugging, if any */
	if (BB_IS_SET(BB_DEBUG)) {
		for (n = 0; Debug_coords[n][AN] != 0.0; n++) {
			show_coord(Debug_coords[n], BB_DEBUG);
		}
	}
}


/* Increment the count of pages printed,
 * and print a PostScript %%Page: comment. When panelsperpage is 2,
 * we only do this on leftside panels and adjust the page count appropriately.
 */

static void
start_page()

{
	Pagesprinted++;

	if (Score.panelsperpage < 2) {
		OUTP(("%%%%Page: %d %d\n", Pagenum, Pagesprinted));
	}
	else if ((Pagesprinted & 1) == 1) {
		OUTP(("%%%%Page: %d %d\n", Pagenum, (Pagesprinted + 1) / 2));
	}
}


/* This is called at the end of pages. It will output a showpage to make the
 * page actually get printed, except in the case of a left side panel
 * when panelsperpage is 2 and this isn't the last page, because in that case,
 * we need to wait until the right panel is rendered.
 */

static void
show_the_page()

{
	if (PostScript_hooks[PU_ATPAGEEND] != 0) {
		set_cur(_Page[AE], _Page[AS]);
		begin_non_music_adj();
		pr_print(PostScript_hooks[PU_ATPAGEEND], YES);
		end_non_music_adj();
	}

	outop(O_RESTORE);
	if ( (Score.panelsperpage < 2) || ((Pagesprinted & 1) == 0) ||
					(last_page() == YES) ) {
		outop(O_SHOWPAGE);
	}
}

/* Print a completely blank page, when user specifies that's what they want,
 * via the -o command line list. */

void
print_blank_page()
{
	start_page();
	outop(O_SAVE);
	if (PostScript_hooks[PU_ATPAGEBEGIN] != 0) {
		begin_non_music_adj();
		pr_print(PostScript_hooks[PU_ATPAGEBEGIN], YES);
		end_non_music_adj();
	}
	OUTP(("%% Intentionally blank page\n"));
	show_the_page();
}


/* Compensate for musicscale if necessary, for header/foot/top/bottom */

static void
begin_non_music_adj()
{
	if (Score.musicscale != DEFMUSICSCALE) {
		double adjust;

		outop(O_GSAVE);
		adjust = (1.0 / Score.musicscale);
		do_scale(adjust, adjust);
	}
	In_music = NO;
}


static void
end_non_music_adj()
{
	if (Score.musicscale != DEFMUSICSCALE) {
		outop(O_GRESTORE);
	}
	In_music = YES;
}
