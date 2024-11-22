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
 *	structs.h
 *
 *	This file defines the structures needed for the Mup program.
 */

#ifndef _STRUCTS
#define _STRUCTS

#include <stdio.h>
#include "defines.h"
#include "rational.h"


/*
 * This struct stores information about a coordinate tag reference in
 * an expression, specifically, the c[] array being referenced and which
 * index into that array is of interest.
 */
struct TAG_REF {
	float *c;			/* a c[] location array */
	short c_index;			/* AX, AY, etc into the c[] array */
};

/*
 * Instances of this struct are linked together in a tree that
 * represents an expression, used in coordinates.
 * The valid operator/operand types are listed in defines.h,
 * along with comments that explain how the fields of the unions
 * in this struct are used by each operator/operand.
 */
struct EXPR_NODE {
	int	op;		/* an OP_* operator/operand type */
	union {
		struct EXPR_NODE *lchild_p;	/* a left tree branch */
		struct TAG_REF *ltag_p;		/* reference to a tag */
		double value;			/* a literal value */
	} left;
	union {
		struct EXPR_NODE *rchild_p;	/* a right tree branch */
		struct TAG_REF *rtag_p;		/* reference to another tag */
	} right;
};

/*
 * Define a structure for holding coordinates that user has in their input.
 * These are normally relative to something, either a something like a
 * bar, note, or group, or one of the builtin tags, namely,
 * _page (the whole sheet of paper), _win (this context's window:
 * header, footer, or center region), or _cur (the current position).
 * But we now allow very general expressions.
 *
 * Both the horizonal and vertical elements have a pointer to some
 * tag's c[], which serves as its "anchor." That is what is
 * used to determine which staff/score the coordinate is associated with.
 * For the horizontal, it also helps determine if the pseudo-bar
 * should be used, if the tag is for a bar.
 * This struct also contains pointers to parse trees
 * of the user's horizontal and vertical expressions,
 * and the results of evaluating those expressions. The evaluation is done
 * in locvar.c between placement and print phases. The hor and vert field
 * hold the reults, and are in scaled inches.
 */
struct INPCOORD {
	float *hor_p;			/* the horizontal anchor tag */
	struct EXPR_NODE *hexpr_p;	/* parse tree of user's hor expr */
	float hor;			/* the result of evaluating hexpr_p */

	float *vert_p;			/* The vertical anchor tag */
	struct EXPR_NODE *vexpr_p;	/* parse tree of user's vert expr */
	float vert;			/* the rsult of evaluating vexpr_p */
};

/*
 * We save a linked list of the addresses of tag references, so that if a tag
 * is moved, we can update its references.
 */
struct COORD_REF {
	float **ref_p_p;		/* address of tag reference */
	struct COORD_REF *next;		/* for linked list */
};

/*
 * For each coordinate that is pointed to by some location variable, we
 * need a bunch of information about it, such as whether it's from a NOTE,
 * BAR, builtin variable, or GRPSYL. If not a builtin variable, we'll need
 * to know what page, score, and (if GRPSYL) staff, it is associated with
 * in order to figure out how to split things that end up on different
 * scores and/or pages.
 */
struct COORD_INFO {
	float *coordlist_p;		/* address of the coordinate */
	short flags;			/* if CT_NOTE, CT_BAR, etc */
	short page;			/* which page it's on */
	short scorenum;			/* which score on the page */
	short staffno;			/* which staff (CT_GRPSYL only) */
	struct MAINLL *mll_feed_p;  /* the feed info about the score it's on */
	struct BAR *pseudo_bar_p;	/* if this is a CT_BAR that happens
					 * to fall at the end of a score,
					 * this field will contain a pointer
					 * to the pseudo bar at the beginning
					 * of the following score */
	struct COORD_REF *ref_list_p;	/* list of references to this coord */
	struct COORD_INFO *next;	/* linked list off hash table */
};

/* Value for an "if" clause, or a "set" expression */
struct VALUE {
	int type;		/* TYPE_* */
	int intval;		/* meaningless if type is not TYPE_INT */
	double floatval;	/* This will always be set in "set" expressions,
				 * even if the type is TYPE_INT, in case this
				 * value has to be promoted to double. */
};

/*
 * Define a structure to be used to hold two staff numbers, and pointers
 * to strings to be malloc'ed to hold labels, if desired.  This is to
 * be used for brace and bracket info.
 */
struct STAFFSET {
	short topstaff;		/* first staff joined by brace or bracket */
	short botstaff;		/* last staff joined by brace or bracket */
	char *label;		/* label to be used on first score */
	char *label2;		/* label to be used on later scores */
};

/*
 * Define a structure to be used to hold top and bottom of a range of staffs
 */
struct TOP_BOT {
	short top;
	short bottom;
	short between;		/* YES or NO */
	short all;		/* YES if barstyle=all or between all */
};

/*
 * Define headcell structure pointing off at linked list of structures
 * holding information about an instance of one of the contexts of the
 * C_BLOCKHEAD class.
 */
struct BLOCKHEAD {
	struct PRINTDATA *printdata_p;	/* point at first item in list */
	float c[NUMCTYPE];		/* for _win */
	float height;			/* of the context instance in inches */
	/*
	 * This flag is set to YES if the score/block before where this is and
	 * after where this is must be kept on the same page due to
	 * samepagebegin/end.
	 */
	short samepage;
};

/*
 * Define a structure to save information about Mup variables to export to
 * user's PostScript code.
 */
struct VAR_EXPORT {
	char *name;			/* name of Mup variable */
	float *tag_addr;		/* Address of the c[] array of the tag*/
	struct VAR_EXPORT *alias;	/* name to use in PostScript, or NULL */
	int index;			/* AX, AW, etc, or NUMCTYPE for all */
	struct VAR_EXPORT *next;	/* linked list */
};

/*
 * Define a structure for holding information about printing.  It can be used
 * in a linked list off of either a BLOCKHEAD or PRHEAD struct.
 */
struct PRINTDATA {
	struct INPCOORD location;	/* input coordinates */
	float width;			/* of string in inches */
	short justifytype;		/* J_LEFT, etc. */
	char *string;			/* malloc'ed string to print */
	short isPostScript;		/* is this for raw PostScript? */
	short isfilename;		/* is there a file name? */
	short ps_usage;			/* PU_* value */
	char *inputfile;        	/* file this print command came from */
	short inputlineno;      	/* line number in inputfile */
	struct VAR_EXPORT *export_p;	/* vars to export to user PostScript */
	struct PRINTDATA *next;		/* for linked list */
	struct PRINTDATA *mirror_p;	/* if mirroring is in effect, this
					 * points to the string to
					 * interchange with, else null */
};

/*
 * Information about a font. There is an array of these, one for
 * every font Mup knows about.
 */
struct FONTINFO {
	char	*ps_name;	/* PostScript name of font */
	short	numchars;	/* how many characters the font has */
	char	**charnames;	/* the /CharStrings names of the characters */

	/* The following 3 arrays give font metrics for each character in
	 * the font, in FONTFACTORs of an inch for DFLT_SIZE. Descent is not
	 * stored explicitly; it is (height - ascent). Different fonts have
	 * different numbers of characters, so these arrays are sized based on
	 * numchars. */
	short	*ch_width;
	short	*ch_height;
	short	*ch_ascent;

	float	maxheight;	/* largest height of any character */
	float	maxascent;	/* largest ascent of any character */
	FILE	*fontfile;	/* User's PostScript defn of font, if any  */
	short	is_bold;	/* is this a bold font? */
	short	is_ital;	/* is this an italic font? */
	short	was_used;	/* YES if this font is actually used */
};

/*
 * This struct stores information about a user-defined symbol.  This includes
 * the information necessary to generate the PostScript code for drawing it,
 * and stem offset information (if it is to be used as a note head).
 */
struct USER_SYMBOL {
	/* The user can refer to the symbol using \(xxxx) where xxxx is
	 * the string this points to. */
	char *name;

	/* User tells us what PostScript to supply for the symbols
	 * font entry. We copy as is, so they must do any necessary escaping. */
	char *postscript;

	/* The next 4 fields define the PostScript bounding box for the symbol,
	 * lower left and upper right corners relative to the symbol's
	 * "logical center".  This is in a 1000-unit character coordinate
	 * space, as is traditional for PostScript characters. */
	short llx;
	short lly;
	short urx;
	short ury;

	/* If the symbols is to be used as a note head, user must specify
	 * the y offset for stems (up and down). This is in the same units
	 * as the bounding box information. */
	short upstem_y;
	short downstem_y;

	/* This tells which attributes of the character have been defined.
	 * It is a bitmap of US_* values. This tells us if any required
	 * attributes are missing, and which optional ones are present. */
	unsigned char flags;
};

/* This stores information about a font worth of user-defined symbols. */
struct USERFONT {
	int num_symbols;	/* how many symbols actually defined */
	struct USER_SYMBOL symbols[MAX_CHARS_IN_FONT];	/* info about each */
};

/*
 * This is for mapping a single symbol. Where Mup would use the "from"
 * symbol by default, the user wants to substitute the "to" symbol.
 */
struct SHAPE_ENTRY {
	unsigned short from;
	unsigned short to;
};

/* This saves information about a named shape mapping. */
struct SHAPE_MAP {
	char *name;			/* to be used by shapes= parameter */
	short num_entries;		/* how many entries in the map */
	struct SHAPE_ENTRY *map;	/* the actual mapping */
	struct SHAPE_MAP *next;		/* for linked list */
};

/*
 * Store info about a string, for tablature.  The parse phase fills in an array
 * of these, one for each string, for each tablature staff.
 */
struct STRINGINFO {
	char letter;		/* 'a' to 'g' */
	char accidental;	/* '\0', '#', '&' */
	short nticks;		/* 0 or more */
	short octave;		/* MINOCTAVE to MAXOCTAVE */
};

/*
 * This struct saves information about one entry in a keymap.
 * An entry is a pair of strings. Whenever the first is encountered in
 * a string to be mapped, it is replaced by the second.
 */
struct KEYMAP_ENTRY {
	char *pattern;		/* The string to be replaced */
	char *replacement;	/* What to replace it with */
	short pat_len;		/* strlen(pattern) to avoid recalculating */
	short rep_len;		/* strlen(replacement) */
};

struct KEYMAP {
	char *name;			/* The user-supplied name for keymap */
	struct KEYMAP_ENTRY *map_p;	/* The key mapping table itself */
	int entries;			/* how many entries used */ 
	int code_index[MAX_CHARS_IN_FONT];/* For each code (e.g. ASCII value)
					 * this gives the index in map_p
					 * for the first entry that starts
					 * with that code, to know where to
					 * start looking for matches. */
	int is_sorted;			/* has map_p been sorted?  YES or NO */
};

/* This stores the information for one entry from an "accidentals" context. */
struct ACCINFO {
	int	font;	/* probably FONT_USERDEF1 or one of the MUSIC fonts */
	int	code;	/* Which character in the font to use
			 * for the accidental symbol. */
	float	offset[7];	/* How much to alter the pitch for each
			 * of the notes a through g. User may specify as
			 * cents or ratio, but we divide out ratios and
			 * convert cents to that format too */
};

/*
 * A linked list of these structs lets us map from the user's name for an
 * accidentals context to the internal representation of the context.
 */
struct ACCIDENTALS {
	char	*name;		/* User's name for the context instance */
	struct ACCINFO *info;	/* Internal table */
	int	size;		/* How many entries in info table */
	struct ACCIDENTALS *next;	/* linked list */
};

/*
 * For use in defining a table of paper sizes.
 */
struct PAPER_SIZES {
	int	width;
	int	height;
};

/*
 * Define structure holding information about these three contexts:
 *
 *	score		the whole score
 *	staff		a staff
 *	voice		a voice
 *
 * It also holds information for the "control" context.  Currently this is
 * used for only one thing: restoring all score/staff/voice parameters to the
 * state they were in at some earlier time when a "save" was done.
 *
 * Every field used by voice is also used by staff and score.  Every field
 * used by staff is also used by score.
 *
 * Except for the selector and "used" fields, every field is numbered.
 * Each item in the map "used" tells whether the corresponding field is
 * currently being used in this instance of the structure.  YES means it is,
 * so the value in this structure is meaningful; NO means it is not, and the
 * value will be ignored; and UNSET means the value formerly set should be
 * forgotten, allowing viewpathing to skip this level (voice or staff).
 * Fields that are always set at the same time may be numbered as a group.
 *
 * Instances of this structure are used in two ways.
 *
 * First, whenever the user's input contains a context of score, staff, or
 * voice, a "struct MAINLL", which contains one of these structures, is
 * allocated and put in the main linked list.  For each item the user sets,
 * the corresponding field is set in this structure and its "used" bit is
 * set to YES.  If the user sets a parameter during the course of the input for
 * a voice, using the <<....>> construct, the SSV will be put in a TIMEDSSV
 * structure and linked off the following BAR line.
 * When the user's input contains a control context, an SSV is allocated only
 * if there is a "restoreparms", containing only the last "restoreparms" in the
 * context.  That is the only command that matters after the parse phase.  For
 * each "saveparms" command, the parse phase saves the contents of all the
 * statically allocated SSVs into save areas.
 * 
 * Second, a structure is allocated statically (global variable) for each
 * possible score, staff, and voice.  Each time the program scans through
 * the main linked list, it starts off by populating the (one and only)
 * score structure with default values for everything, and setting all its
 * "used" bits to YES.  It sets all the "used" bits for the staff and voice
 * structures to NO.  As it scans through the main linked list, whenever it
 * finds one of these structures, it copies all the fields whose "used" bits
 * are YES in that structure, to the appropriate fixed structure, and sets
 * its "used" bit to YES there.  When it finds a "restoreparm", it copies all
 * the data from the appropriate save area into the statically allocated SSVs.
 * Note: The selector and linkage items are not used in these fixed structures.
 * 
 * Whenever the program needs to know the current value of a field for a
 * given voice, staff, and score, it uses a viewpath through the corresponding
 * three fixed structures, in that order, using the "used" bits to see if
 * the field is filled in.  Note that the score structure is always fully
 * populated, so the info will be found then, if not earlier.
 */

/* define the indices to the "used" field */
#include "ssvused.h"

struct SSV {
	/* ======== SELECTOR ITEMS (ONLY SET FOR USER INPUT STRUCTS) ======= */
	UINT32B context;/* which context is it for? (used by all) */
	short staffno;	/* staff no. (used by staff & voice); 1 to MAXSTAFFS */
	short voiceno;	/* voice no. (used by voice); 1 to MAXVOICES */

	/* ======== USED FLAGS ======== */
	char used[NUMFLDS];	/* map of which fields below are being used */

	/* ======== ITEMS FOR CONTROL CONTEXT ONLY ======== */
	short saved_ssv_index;	/* index into Saved_parms to restore from */

	/* ======== ITEMS FOR SCORE CONTEXT ONLY ======== */
	float scale_factor;	/* scale all output (except margins) by this */
	float musicscale;	/* scale only the music "window" by this */

	float units;		/* INCHES or CM */

	float pageheight;	/* size of the paper, in inches */
	float pagewidth;

	short panelsperpage;	/* print how many panels on each page of paper*/
	short slashesbetween;	/* print slashes between scores to separate? */
	short maxscores;	/* max scores allowed on any panel */
	short maxmeasures;	/* max measures allowed on any score */

	float topmargin;	/* the four margins, in inches */
	float botmargin;
	float leftmargin;
	float rightmargin;

	short flipmargins;	/* reverse left/right margin every other page?*/

	short indentrestart;	/* ...if the restart falls at end of a line */

	short restcombine;	/* min. no. of mr to combine, like -c option */
	short firstpage;	/* page number for first page, like -p option*/
	short firstside;	/* is the first page left or right? PGSIDE_* */

	short staffs;		/* no. of staffs, 1 to MAXSTAFFS */

	/*
	 * The following vertical distances are in units of stepsizes.
	 * They are as follows:  minscsep is the smallest distance ever allowed
	 * between the bottom line of the bottom staff of one score, and the
	 * top of the top of the next.  (The program distributes extra space
	 * on the page between scores.)  maxscsep is the farthest distance
	 * allowed here, unless things sticking out force it farther.
	 * scorepad is the minimum distance allowed between the outermost
	 * things on neighboring scores.
	 */
	float minscsep;
	float maxscsep;
	float minscpad;
	float maxscpad;

	/*
	 * A brace scheme is either "none" (nbrace == 0), or a list of pairs
	 * of staff numbers and optional labels.  Braces are to be drawn
	 * joining each pair of staffs at the start of each score.  The staffs
	 * in a pair may be the same; (3,3) means a brace is put on staff 3.
	 * If nbrace != 0, an array must be malloc'ed for bracelist to point
	 * at, so it can be treated as an array.  If labels are to be printed
	 * for the group of staffs, a place must be malloc'ed for them, and
	 * the pointers in the "struct STAFFSET" must be set.  Otherwise,
	 * those pointers must be set to null.
	 *
	 * A bracket scheme works exactly the same, using nbrack and bracklist.
	 * 
	 * Barcon refers to which staffs are to be connected by bar lines.
	 * This also works the same way, except that if staff N does not
	 * fall within any of the ranges, it still will have bar lines, as
	 * if (N,N) had been listed explicity.  Also, no labels are allowed.
	 */
	short nbrace;			/* brace ranges in list */
	struct STAFFSET *bracelist;	/* pointer to list */
	short nbrack;			/* bracket ranges in list */
	struct STAFFSET *bracklist;	/* pointer to list */
	short nbarst;			/* bar-connected ranges in list */
	struct TOP_BOT *barstlist;	/* pointer to list */

	short nsubbar;		/* how many subbars to draw per measure */
	struct SUBBAR_INSTANCE *subbarlist;	/* info about each subbar */

	short timenum;		/* time sig numerator, 1 to 99 */
	short timeden;		/* time sig denominator, power of 2 */
				/* from 1 to 64 */
	char *timerep;		/* representation of time signature */
	short timevis;		/* is time sig visible (PTS_NEVER) */
	RATIONAL time;		/* time signature in lowest terms */

	short division;		/* clock ticks per 1/4 note (used by midi) */
	short endingstyle;	/* where ending brackets are to be drawn */
	short alignlabels;	/* J_* (LEFT, RIGHT, CENTER) */
	short gridsatend;	/* print chord grids at end of song? */
	short measnum;		/* should measure numbers be printed? */
	short measnumfamily;	/* font family for measnum */
	short measnumfont;	/* font for measnum */
	short measnumsize;	/* point size for measnum */
	short measnumstyle;	/* style of measnum: RS_* */
	float pilescale;	/* multiply font size by this in piles */
	short bracketrepeats;	/* should repeat bars have brackets drawn? */
	float packfact;		/* horizontal packing factor */
	float packexp;		/* horizontal packing expansion (exponent) */
	float leftspacefact;	/* fraction of room on left side of chord */
	float leftspacemax;	/* max stepsizes of room on left side of chord*/

	short warn;		/* should warnings be printed (YES/NO)? */
	struct KEYMAP *printkeymap;	/* keymap for print etc. statements */
	float a4freq;		/* frequency of the note a4 */
	short tuning;		/* the tuning system being used: TU_* */
	char *acctable;		/* name of the table of accidentals to use */

	/* ======== ITEMS FOR SCORE AND STAFF CONTEXT ======== */
	float staffscale;	/* scale staff by this, relative to score */
	short stafflines;	/* number of lines in staff (normally 5) */
	struct STRINGINFO *strinfo;	/* iff tab staff, malloc'ed array */
	short printclef;	/* SS_* (shares staffline's "used" flag) */
	short printtabclef;	/* PTC_* (shares staffline's "used" flag) */
	short gridswhereused;	/* print grids by chords where used? */
	float gridscale;	/* scale chord grids by this times staffscale*/
	short gridfret;		/* min fret to print next to chord grid */
	short mingridheight;	/* min number of frets to print in chord grid*/
	short numbermrpt;	/* should mrpt have number printed above? */
	short numbermultrpt;	/* should multiple meas rpt have number above?*/
	short printmultnum;	/* should multirests have no. printed above? */
	short restsymmult;	/* draw multirests using rest chars? (YES/NO)*/
	short vscheme;		/* voice scheme */
	short vcombine[MAXVOICES]; /* voices to be combined if possible, in the
				    * order to try the combining */
	short vcombinequal;	/* vcombine qualifer (see definition of VC_*) */
	short vcombinemeas;	/* vcombine qualifer, consistent across meas? */
	char *prtime_str1;	/* printedtime: first arbitrary string */
	char *prtime_str2;	/* printedtime: second arbitrary string */
	short prtime_is_arbitrary; /* YES = arbitrary, NO = use timerep */
	short sharps;		/* no. of sharps, -7 to 7 */
	short is_minor;		/* minor key (YES/NO)? (used by MIDI) */
	short cancelkey;	/* should old key sig be canceled with nats? */
	short inttype;		/* transpose: interval type (MINOR, ...) */
	short intnum;		/* transpose: interval number, neg means down*/
	short trans_usage;	/* what the transposition applies to, TR_* */
	short addinttype;	/* same as inttype but for addtranspose */
	short addintnum;	/* same as intnum but for addtranspose */
	short addtrans_usage;	/* what the addtransposition applies to, TR_* */
	short useaccs;		/* use accidentals instead of a key sig */
	short carryaccs;	/* should accs apply until the next barline? */
	short clef;		/* which clef is it? */
	short forceprintclef;	/* print clef even if it didn't change? */
	short rehstyle;		/* what should reh marks be enclosed in? */
	short repeatdots;	/* what dots are to be drawn in repeat signs? */
	short fontfamily;	/* font family for text other than lyrics */
	short font;		/* font for text other than lyrics */
	short size;		/* point size for text other than lyrics */
	short lyricsfamily;	/* font family for lyrics */
	short lyricsfont;	/* font for lyrics */
	short lyricssize;	/* point size for lyrics */
	float lyricsalign;	/* fraction of syl to the left of chord center*/
	short sylposition;	/* points left of chord center to start syl
				 * (overrides lyricsalign if used) */
	float minalignscale;	/* min scale aligned "stuff" can be shrunk to */

	/*
	 * minstsep is the distance between bottom line of this staff and top
	 * line of the first visible staff below it, unless things sticking out
	 * force them to be farther apart.
	 */
	float minstsep;

	/*
	 * staffpad is the minimum distance allowed between the outermost
	 * things on neighboring staffs.  It can be negative, to allow
	 * overlapping.
	 */
	float staffpad;

	/*
	 * markorder, for each "place", contains the stacking priority of each
	 * MK_* (mark type).  Priority 1 get stack first, then 2, etc.  This
	 * should really be "short" (a number, not a char), but it would waste
	 * too much space.
	 */
	char markorder[NUM_PLACE][NUM_MARK];

	short pedstyle;		/* type of pedal marks to draw */
	short alignped;		/* align all pedal marks on a score? */
	float chorddist;	/* min dist between chord & staff, STEPSIZEs */
	float dist;		/* min dist between stuff & staff, STEPSIZEs */
	float dyndist;		/* min dist between dyn & staff, STEPSIZEs */
	float lyricsdist;	/* min dist between lyrics & staff, STEPSIZEs */

	short chordtranslation;	/* CT_* */
	char **doremi_syls;	/* map C D E F G A B to these syllables */

	/* must malloc a place to store these strings, else set to null */
	char *label;		/* label on first score */
	char *label2;		/* label on later scores */

	struct KEYMAP *labelkeymap;	/* keymap for labels */
	struct KEYMAP *endingkeymap;	/* keymap for endings */
	struct KEYMAP *rehearsalkeymap;/* keymap for rehearsal marks */
	struct KEYMAP *defaultkeymap;	/* use if specific keymap is not set */
	struct KEYMAP *withkeymap;	/* keymap for with lists */
	struct KEYMAP *textkeymap;	/* keymap for text statements */
	struct KEYMAP *lyricskeymap;	/* keymap for lyrics */

	/* ======== ITEMS FOR SCORE, STAFF, AND VOICE CONTEXT ======== */
	/*
	 * Although "visible" applies to score, staff, and voice, it is handled
	 * specially; see svpath() in ssv.c.  The "visible" parameter sets both
	 * the "visible" and "hidesilent" fields, as follows:
	 *	visible parameter	visible field		hidesilent field
	 *		"n"			NO			NO
	 *		"whereused"		YES			YES
	 *		"y"			YES			NO
	 * The whereused value is not allowed for voice.  As soon as the all
	 * the scorefeeds are known (after abshorz.c), new SSVs are inserted to
	 * make the appropriate staffs invisible "for real" (using "visible").
	 */
	short visible;		/* is the voice visible? */
	short hidesilent;	/* if normally visible, hide when not used? */
	/*
	 * A beam scheme is either "none" (nbeam == 0), or a list of note
	 * durations adding up to a full measure.  nbeam is how many durations
	 * in the list.  If nbeam != 0, an array must be malloc'ed for
	 * beamstlist to point at, so it can be treated as an array.
	 * Some of these durations can be parenthesized sublists of durations.
	 * If not, then nsubbeam == nbeam and subbeamstlist is the same as
	 * beamstlist.  But if so, the "sub" items count and list each
	 * individual duration, regardless of whether it's a stand-alone
	 * duration, or a member of a sublist; and nbeam and beamstlist give
	 * the combined version, where the durations of each sublist are added
	 * up and are regarded as a single duration.
	 * When nbeam != 0, beamrests tells whether the "r" flag was given;
	 * otherwise, beamrests is garbage.
	 * When nbeam != 0, beamspaces tells whether the "s" flag was given;
	 * otherwise, beamspaces is garbage.
	 */
	short nbeam;		/* durations in list */
	RATIONAL *beamstlist;	/* pointer to list */
	short beamrests;	/* YES or NO:  beam across rests? */
	short beamspaces;	/* YES or NO:  beam across spaces? */
	short nsubbeam;		/* durations in list */
	RATIONAL *subbeamstlist; /* pointer to list */

	/* these are controlled by BEAMSLOPE */
	float beamfact;		/* beam angle = this * regression angle */
	float beammax;		/* maximum beam angle allowed (degrees) */

	/* these are controlled by TUPLETSLOPE */
	float tupletfact;	/* bracket angle = this * regression angle */
	float tupletmax;	/* maximum bracket angle allowed (degrees) */

	float pad;		/* apply on left of each group (stepsizes) */
				/* internal value = external - 1/3 */
	float stemlen;		/* stem length in inches */
	/*
	 * The following items are based off the "stemshorten" parm, and are
	 * all measured in stepsizes.   beamshort is how much shortening can be
	 * applied to a stem because of its being in a beamed set.  The other
	 * three deal with shortening because of a stem protruding from the
	 * staff.  Shortening begins when the note is "begproshort" distance
	 * from the center line towards the side of the staff where the stem
	 * points.  It is zero at one step less that that, and increases
	 * linearly up to the point given by endproshort, where it attains the
	 * maximum amount of shortening, maxproshort.
	 */
	float beamshort;
	float maxproshort;
	short begproshort;
	short endproshort;

	short midlinestemfloat;	/* stems of notes on middle line float? YES/NO*/
	short defoct;		/* default octave number, 0 to 9 */
	short noteinputdir;	/* UP/DOWN/UNKNOWN: how to set notes' octaves */
	RATIONAL timeunit;	/* note length to use when none specified */
	struct TIMELIST *timelist_p;	/* LL of additional times for timeunit*/
	RATIONAL swingunit;	/* duration within which notes are to "swing" */
	short withfamily;	/* font family for "with" lists */
	short withfont;		/* font for "with" lists */
	short withsize;		/* point size for "with" lists */
	short noteleftfamily;	/* font family for strings left of notes */
	short noteleftfont;	/* font for strings left of notes */
	short noteleftsize;	/* point size for strings left of notes */
	short alignrests;	/* align rests vertically with notes? */
	short release;		/* internote space for MIDI, in milliseconds */
	short ontheline;	/* put notes on the one-line staff line? */
	short tabwhitebox;	/* print white rectangle under fret numbers? */
	short noteheads[7];	/* headshapes to be used for each scale degree*/
	struct SHAPE_MAP *shapes;	/* point at shape map context */
	char *emptymeas;	/* input to use when no voice info is given */
	short extendlyrics;	/* automatically put "_" on syllables? */
	short cue;		/* should this voice be all cues? */
	short defaultphraseside;/* PL_ABOVE/PL_BELOW/PL_UNKNOWN */
};

/*
 * Define a structure for timed SSVs.  These represent parameters that are set
 * in the course of the input for a voice, using the <<....>> construct rather
 * than being set the usual way in a score, staff, or voice context.  They are
 * put in a linked list hanging off the next BAR structure.  They are stored in
 * user input order, except that they are sorted by time_off.
 */
struct TIMEDSSV {
	struct SSV ssv;		/* all the normal contents of an SSV */
	RATIONAL time_off;	/* time offset into the measure where it is */
	struct GRPSYL *grpsyl_p;/* the group before which the <<....>> was */
	struct TIMEDSSV *next;	/* link to the next one */
};

/*
 * If the user enters times to be added together, like 2.+16, a linked list of
 * these structs keeps track of the added times.
 */
struct TIMELIST {
	int		basictime;
	RATIONAL        fulltime;	/* like fulltime in struct GRPSYL */
	struct TIMELIST *next;		/* for linked list */
};

/*
 * Define structure pointing to the list of chords in a measure.
 */
struct CHHEAD {
	struct CHORD *ch_p;	/* point at a linked list of chords */
};

/*
 * Define structure pointing a list of things to print.  It is used for
 * prints that occur in the "music" context.
 */
struct PRHEAD {
	struct PRINTDATA *printdata_p;	/* point at first item in list */
};

/*
 * Define a structure containing a coordinate for an ending or rehearsal mark.
 * The coordinate given is the south edge of the item's rectangle.
 */
struct MARKCOORD {
	short staffno;		/* which staff has the ending and/or rehear */
	float ry;		/* vertical coord rel to center line of staff */
	struct MARKCOORD *next;	/* for linked list */
};

/*
 * Define structure holding info about a bar line.
 */
struct BAR {
	short bartype;		/* type of bar line */
	short linetype;		/* type of line (L_*) */

	/*
	 * When a repeatstart occurs at the end of a line, it gets moved to the
	 * pseudobar on the next line.  Where it used to be, a new bar line is
	 * supplied.  The type of that new bar line is specified by precbartype.
	 * It defaults to singlebar.
	 */
	short precbartype;

	/*
	 * The bar line's coordinates have the following meanings.
	 * X is the middle of the bar line (even if it's a repeat sign, etc.).
	 * Y is the middle line of the top staff.
	 * W and E allow standard padding.  (The leftmost "bar line" on a score
	 * is not considered to be a bar line at all, but is just drawn as
	 * part of the score.)
	 * N is the top line of the top staff; S is the bottom line of the
	 * bottom staff.
	 */
	float c[NUMCTYPE];	/* coordinates */

	float padding;		/* extra space to allow */

	/*
	 * Define whether the user used "hidechanges" on this bar, which
	 * prevents changes of clef, key, and time from printing out after this
	 * bar if it ends up being the last bar on a score.
	 */
	short hidechanges;	/* YES or NO */

	/*
	 * Define position (*ITEM) relative to an ending, and what the label
	 * should say.  The label is meaningful only for the first bar of
	 * the ending (STARTITEM).  ENDITEM is used for the bar after the
	 * last measure of the ending.
	 */
	short endingloc;	/* position within (or not) an ending */
	char *endinglabel;	/* malloc'ed string to label the ending */

	short reh_type;		/* REH_* */
	char *reh_string;	/* string to print as rehearsal mark; should */
				/* be null if reh_type != REH_STRING */
	float dist;		/* overrides SSV dist for the reh mark */
	short dist_usage;	/* was dist used, and was it forced? (SD_*) */

	short mnum;		/* measure number, 0 unless set by the user */

	short endending_type;	/* closed or open at end: values are EE_* */

	/*
	 * This flag is set to YES if the measures before and after this bar
	 * line must be kept on the same score due on samescorebegin/end.
	 */
	short samescore;

	/*
	 * This flag is set to YES if the score/block before where this is and
	 * after where this is must be kept on the same page due to
	 * samepagebegin/end.
	 */
	short samepage;

	/*
	 * These start linked lists, one structure for each staff at this
	 * bar line that needs to have coordinates stored for an ending mark
	 * or a rehearsal mark above it.  A coord is given for an ending
	 * mark only at the bar line where it begins.
	 */
	struct MARKCOORD *ending_p;	/* LL for ending marks */
	struct MARKCOORD *reh_p;	/* LL for rehearsal marks */

	/*
	 * If Score.nsubbar > 0, this points to a malloc-ed array of that size,
	 * one of these per each subbar in the preceding measure; else 0.
	 */
	struct SUBBAR_LOC {
		float ax;		/* position of the subbar */
		short *pr_subbars;	/* print subbar? YES/NO for each staff*/
	} *subbar_loc;


	/*
	 * There is also a linked list holding SSVs for parameters that were
	 * set during the input for a voice rather than in their own context.
	 */
	struct TIMEDSSV *timedssv_p;
};

/* This describes how a subbar looks */
struct SUBBAR_APPEARANCE {
	short bartype;          /* BAR or DBLBAR */
	short linetype;         /* L_NORMAL, L_DASHED, or L_DOTTED */
	short upper_ref_line;   /* LR_TOP, LR_MIDDLE, or LR_BOTTOM */
	float upper_offset;     /* stepsizes away for the reference line */
	short lower_ref_line;	/* similar for lower */
	float lower_offset;	/*    "              */
	short nranges;          /* how many staffs ranges */
	struct TOP_BOT *ranges_p; /* staffs to include in subbars */
};

/*
 * This describes a single subbar, saying at which count in the measure
 * it is to be drawn, and with a pointer to the information about what
 * it should look like.
 */
struct SUBBAR_INSTANCE
{
	float count;		/* draw at this beat in the measure */
	struct SUBBAR_APPEARANCE *appearance_p;	/* make it look like this */
};

/*
 * Define structure for a line.
 */
struct LINE {
	short linetype;			/* type of line */
	struct INPCOORD start, end;	/* start and end points */
	char *string;			/* malloc; to be printed by the line */
	int side;			/* PL_ABOVE or PL_BELOW */
};

/*
 * Define structure for a curve.  There are two input formats for a curve.  The
 * first type gives 3 or more coordinates, but no bulge distances.  For that
 * type ncoord tells how many coordinates, the coordinates are given by
 * coordlist, nbulge is zero, and bulgelist is not allocated.  For the other
 * type of curve, 2 points (the endpoints) are given, and 1 or more bulge
 * distances are given, in stepsize units.
 */
struct CURVE {
	short curvetype;		/* type of curve */
	short ncoord;			/* number of coords in the following */
	struct INPCOORD *coordlist;	/* array of coords to be malloc'ed */
	short nbulge;			/* number of bulge distances given */
	float *bulgelist;		/* array of them to be malloc'ed */
};

/*
 * Define a structure for score and page feeds.  The parser puts these in
 * the main linked list when it sees the user's "newscore" and "newpage"
 * directives, and the placement phase puts additional ones there as needed.
 */
struct FEED {
	short pagefeed;		/* YES=score & page feed, NO=scorefeed only */

	/*
	 * The following are margin overrides, and flags for how/whether they
	 * are being used (*_mot == margin override type).  They are for left
	 * and right margins respectively.  Left margins apply to the score
	 * after this feed.  Right margins apply to the score before this feed.
	 * A mot of MOT_UNUSED means that margin is not being overridden.
	 * MOT_RELATIVE can be used initially, but later the code changes it
	 * to MOT_ABSOLUTE and changes the value to be absolute.  MOT_AUTO
	 * means placement has to determine a value.
	 */
	short left_mot, right_mot;
	float leftmargin;	/* override param on score after the feed */
	float rightmargin;	/* override param on score before the feed */

	/*
	 * If this is a pagefeed, there may be blockhead contexts of some or
	 * all of these 12 types that we need to point at.  When the user
	 * uses one of these contexts, it forces a pagefeed (whether they
	 * explicitly requested it or not), and the parse phase sets the
	 * pointer to a malloc'ed area.  The parse phase's use or non-use of a
	 * "2" suffix, and "left" and "right" prefix, is like for Header etc.
	 * (see globals.c).  Later, for each FEED, absvert.c says, for each
	 * set of 3 below (<null>, left, right), if left is not set, copy the
	 * <null> one to it, and likewise for right.  And for the "2" ones, if
	 * left or right is still not set, carry forward the value from the
	 * previous FEED.  And finally, based on left/right page, copy the
	 * relevant value to top_p and bot_p.  Thus, the print phase only needs
	 * to look at the top_p and bot_p.  But grids-at-end is a special case;
	 * see the code at the end of posscores().
	 */
	struct BLOCKHEAD *top_p,  *lefttop_p,  *righttop_p;
	struct BLOCKHEAD *bot_p,  *leftbot_p,  *rightbot_p;
	struct BLOCKHEAD *top2_p, *lefttop2_p, *righttop2_p;
	struct BLOCKHEAD *bot2_p, *leftbot2_p, *rightbot2_p;

	short firstvis;		/* first visible staff number in this score */
	short lastvis;		/* last visible staff number in this score */
	float lastdist;		/* distance from bottom line of last visible */
				/*  staff to the southernmost extent of score*/
	/*
	 * If used, this overrides the scoresep and scorepad parameters and
	 * forces the previous and next scores to be this distance apart.  As
	 * for the scoresep parameter, the distance is measured from the bottom
	 * staff line of the score above to the top line of the score below.
	 */
	float scoresep;		/* in stepsizes */

	/*
	 * The following are the coordinates of the score that follows.
	 * W and E are the margins, which it always extends out to when you
	 * consider labels and everything (no padding).
	 * N and S are just far enough out to include every rectangle,
	 * every staff, and every clef, with standard padding.
	 * X is the the X of the line left of the clefs.
	 * Y is the middle line of the top visible staff.
	 */
	float c[NUMCTYPE];	/* coordinates of the score that follows */

	/*
	 * North and south of the music window's _win.  They are used only if
	 * this is a pagefeed, or is the first page of music.
	 */
	float north_win, south_win;

	/* offset of top edge of slashesbetween from FEED's Y, zero if none */
	float sl_bet_top_offset;
};


/*
 * Define structure telling whether to print clef, key signatures, or
 * time signatures.
 */
struct CLEFSIG {
	short prclef[MAXSTAFFS + 1];	/* print clef this staff? (YES/NO) */
	short clefsize;			/* print them DFLT_SIZE or SMALLSIZE */
	/*
	 * sharps tells how many sharps to print in the key sig.  If negative,
	 * it means flats.  naturals means how many sharps are to be cancelled
	 * with natural signs.  If negative, it means we are cancelling flats.
	 * Based on these numbers and on music theory, the print phase knows
	 * which ones to print.  If both are zero, no key sig is to be printed.
	 */
	short sharps[MAXSTAFFS + 1];
	short naturals[MAXSTAFFS + 1];
	short prtimesig;		/* print time signature? (YES/NO) */
	float wclefsiga;		/* absolute west coord of clefsig */
	float effwidth;			/* width that can't overlap chords */
					/* (used only by user clefsigs) */
	float widestclef;		/* width of widest clef to print */
					/* (used only by user clefsigs) */
	short hide;			/* should be hidden (hidechanges)? */
	short multinum;			/* number of measures in the multirest
					 * that follows, 0 if no multirest */

	/*
	 * The following is a pointer to a BAR that gets malloc'ed for
	 * CLEFSIGs that occur after FEEDs (at the start of a score).  This
	 * represents the special, pseudo bar line at the start of a score.
	 * This pseudo bar comes immediately after the other clefsig items.
	 * It actually gets drawn only if it happens to be a REPEATSTART.
	 */
	struct BAR *bar_p;
};


/*
 * Define a structure describing a staff for one measure, which points off to
 * linked lists of GRPSYLs and STUFFs.
 */
struct STAFF {
	short staffno;			/* staff number */
	short visible;			/* is this staff visible? */

	/*
	 * Coordinates for the location of the staff.  The relative horizontal
	 * ones are never set, but the absolute horizonal ones are set to meet
	 * the surrounding bar lines.  The vertical ones are the same for
	 * all STAFFs for the same staff number for a given score.  (For each
	 * staff number, the packing of rectangles is done across the whole
	 * score, and the same resulting vertical coords are stored in each
	 * measure's STAFF.)  The relative vertical coords start out relative
	 * to the center line of the staff, so at that time RY is 0.  Later,
	 * they are changed to be relative to the score.
	 */
	float c[NUMCTYPE];		/* location of staff */

	struct GRPSYL *groups_p[MAXVOICES]; /* linked list(s) of voices */

	/*
	 * Following is syls_p, a malloc'ed array of headcells of linked lists
	 * of GRPSYLs for verses.  There are "nsyllists" lists, which is
	 * "Maxverses" or less.  The parallel array sylplace tells whether
	 * each list of syllables is above this staff, below it, or centered
	 * between this staff and the next staff number.  The verse numbers do
	 * not have to equal the index into syls_p.  Lists for the three
	 * places can be mixed together, alternating or whatever.  But the
	 * verse numbers of each given place are in increasing order.  Any
	 * verses may be missing, but then they won't have entries in syls_p.
	 */
	short nsyllists;
	short *sylplace;
	struct GRPSYL **syls_p;

	/*
	 * Following is the headcell for the linked list of other "stuff"
	 * associated with this staff; above, below, and between mixed together
	 * any which way.  Actually, they are in user input order for below,
	 * and reversed for above and between, and that's the order in which
	 * their surrounding rectangles will be packed together.  Thus, on the
	 * page things will end up placed in agreement with user input order.
	 */
	struct STUFF *stuff_p;

	/*
	 * Centered between this staff and the next we may have lyrics and/or
	 * "stuff".  The rectangles for all this are packed together against
	 * a base line, and then the total height of all that is stored here.
	 */
	float heightbetween;

	/*
	 * In an mrpt measure, this holds the number that is to be printed
	 * above the measure (2 at the first mrpt, then increment).  For other
	 * measures it is 0.
	 */
	short mrptnum;

	short mult_rpt_measnum;	/* counts which measure of dblmrpt or quadmrpt*/
};


/*
 * Define a structure that hold information about a chord grid.
 */
struct GRID {
	char *name;		/* internal chord name string (malloc) */

	/*
	 * positions[0] is the fret for the first string, positions[1] is the
	 * second, etc.  0 means draw an "o" above this string, -1 means draw
	 * an "x".  -2 means draw nothing.
	 */
	short positions[MAXTABLINES];	/* slot for each string possible */
	short numstr;			/* number of strings used */

	/*
	 * Numbers of the left and right strings to which the curved line
	 * extends.  The first string is 1, not 0.  0 means no curved line.
	 */
	short curvel, curver;

	short used;			/* was this grid used in this song? */
};


/*
 * Define a structure that describes an item to be drawn other than music and
 * lyrics.  A linked list of these can hang off a STAFF structure.
 */
struct STUFF {

	short inputlineno;	/* which input line this structure came from */
	char *inputfile;	/* which file this came from (malloc'ed) */
	char *string;		/* usual convention of 1st 2 bytes = font/size*/
	short all;		/* does this STUFF actually belong to "all" */
				/* (the score), not a particular staff? YES/NO*/
	char *grid_name;	/* For TM_CHORD, the actual grid name; otherwise
				 * unused. The "string" field will typically be
				 * the same, but if user provided an alternate
				 * label to be printed, that will be in "string"
				 */

	/*
	 * Define start and end times for the stuff.  "start" and "end.count"
	 * range from 0 to (numerator_of_time_sig + 1).  They can be any float
	 * within that range; they don't have to line up with any group.
	 * However, if gracebackup is not 0, it means the stuff is to start at
	 * that many grace notes before the normal group that is closest to
	 * "start".  Also, if the stuff is a phrase mark, both ends of it are
	 * set to the nearest group that is not rest or space, even if grace-
	 * backup is 0.  (If it is nonzero, it then works the same way.)
	 * In any case, after the start position is determined as described
	 * above, the "steps" offset is applied to it, which can be positive,
	 * negative, or (usually) zero.  And similarly, the other "steps"
	 * offset and other gracebackup is applied to the end position.
	 */
	struct {
		float count;	/* counts into measure where the thing begins*/
		float steps;	/* offset in stepsizes */
		short gracebackup; /* how many graces before count to start at*/
	} start;
	struct {
		int bars;	/* how many bar lines it crosses */
		float count;	/* count (in whichever measure) where it ends */
		float steps;	/* offset in stepsizes */
		short gracebackup; /* how many graces before count to end at */
	} end;			/* all are 0 if no "til" clause */

	short stuff_type;	/* ST_CRESC, etc. */
	short modifier;		/* if text, is it chord, etc. (TM_*)? */
				/* if phrase, what type of line (L_*)? */
	short place;		/* PL_ABOVE, etc. */
	float dist;		/* overrides SSV dist/chorddist/dyndist */
	short dist_usage;	/* was dist used, and was it forced? (SD_*) */
	short aligntag;		/* tag for alignment with other STUFFs */
	float horzscale;	/* horz scaling of a STUFF */
	short carryin;		/* is this a continuation from last score? */
	short carryout;		/* does this continue onto the next score? */
	struct STUFF *costuff_p;/* for tie/slur/bend carryin stuff, point at 
				 * corresponding carryout stuff */

	/*
	 * The following group of variables is used only for ST_PHRASE and/or
	 * ST_TIESLUR/ST_TABSLUR/ST_BEND.
	 *
	 * Phrases use them as follows:
	 * A phrase must be assigned to apply to a particular voice on the
	 * staff.  Unlike other STUFF, the endpoints of a phrase mark need to
	 * get associated with GRPSYLs.  These pointers get set to point at
	 * them.  If it crosses score feeds, carry ins and outs will have a
	 * pointer to the first and last GRPSYL of the score, respectively.
	 * The crvlist_p is the headcell of the linked list of points forming
	 * a phrase mark.  begnote_p is not used.
	 *
	 * Ties and slurs use them as follows:
	 * A tie/slur is not input the same as other STUFF.  The user's input
	 * sets its starting note.  The STUFF structure is not allocated until
	 * stuff.c.  At that point, vno, beggrp_p, and begnote_p are set, and
	 * crvlist_p is set up like for phrases.  If it crosses a scorefeed,
	 * carry in and out are used, and the "beg" pointers for the second
	 * half of the tie/slur point at the end group and note.  In any case,
	 * curveno tells which tie/slur this STUFF refers to, since a tie and
	 * multiple slurs can start at the same note.  Normally, endgrp_p isn't
	 * used.  However, if the user specified the voice of the note tied or
	 * slurred to, and the tie/slur doesn't cross a scorefeed, endgrp_p is
	 * used:  It is set to the group where the tie/slur ends.
	 */
	short vno;			/* voice phrase applies to (1 or 2) */
	struct GRPSYL *beggrp_p;	/* beginning GRPSYL */
	struct GRPSYL *endgrp_p;	/* ending GRPSYL */
	struct CRVLIST *crvlist_p;	/* headcell of linked list of coords */
	struct NOTE *begnote_p;		/* beginning NOTE */
	short curveno;			/* idx into slurtolist; -1 for tie */

	/*
	 * For above and below stuff, the relative vertical coords are
	 * relative to the staff's center line.  For between stuff, they end
	 * up being relative to the center line of the above staff, but at
	 * first they are relative to the base line that the between stuff is
	 * piled on.  (Not used for phrase marks.)
	 */
	float c[NUMCTYPE];	/* where item is placed */

	struct STUFF *next;	/* for linked list */
};

/*
 * Define a structure for forming linked lists of coordinates making up the
 * curve of a phrase mark.  Each structure has the coordinates of one point.
 */
struct CRVLIST {
	float x;		/* absolute X */
	float y;		/* Y initially rel to staff, later absolute */
	struct CRVLIST *next;	/* doubly linked list */
	struct CRVLIST *prev;
};

/*
 * Define struct to save lists of staff number or vno (voice or verse) ranges.
 */
struct RANGELIST {
	short	begin;		/* first number in range */
	short	end;		/* last number in range. Must be >= begin */
	short	all;		/* is this staff no. actually "all" (the */
				/* score), not a particular staff? YES/NO */
	short	place;		/* PL_*   */
	struct RANGELIST *next;	/* for linked list */
};

/*
 * Define a struct to save a list of pairs of staff and voice range lists.
 */
struct SVRANGELIST {
	struct RANGELIST *stafflist_p;
	struct RANGELIST *vnolist_p;
	struct SVRANGELIST *next;	/* linked list */
};

/*
 * Define a structure for stating the note that a given note is slurred to.
 */
struct SLURTO {
	char letter;		/* a to g */
	short octave;		/* 0 to 9 */
	short slurstyle;	/* what type slur: L_[NORMAL|DOTTED|DASHED] */
	short slurdir;		/* should slur bulge UP or DOWN? */
	short slurred_to_voice;	/* voice number of the note we are slurring to*/
};

/*
 * Define the structure for holding information concerning a note.  Much of
 * the info you might expect to be here actually applies to the whole "group",
 * and so is in the group/syllable structure below.
 * NOTE:  When adding fields to this structure, update function map1note().
 */
struct NOTE {
	/*
	 * Define the coords x, y, north, south, east, west, both relative
	 * and absolute.  The relative coords are relative to the group's
	 * (x, y). The NSEW coords define a rectangle surrounding the note
	 * head.  XY are the center of the note head.
	 */
	float *c;		/* must malloc array; see comment in */
				/*  grpsyl.c, add_note() for why */

	float waccr;		/* relative coord:  w(accidental)-x(group) */
	float ydotr;		/* relative coord:  y(dot)-y(group) */

	/* these next two are used when note_has_paren is YES */
	float wlparen;		/* relative coord:  w(left paren)-x(group) */
	float erparen;		/* relative coord:  e(right paren)-x(group) */

	/* wlstring is 0 if and only if noteleft_string is 0 (null) */
	float wlstring;		/* relative coord:  w(string)-x(group) */
	char *noteleft_string;	/* malloc: string to print left of the note */

	/*
	 * nslurto says how many notes of the following group this note is
	 * slurred to.  If it is greater than 0, an array of that many SLURTO
	 * structures must be malloc'ed and slurtolist set to point at it.
	 */
	struct SLURTO *slurtolist;
	short nslurto;

	char letter;		/* a to g */
	char acclist[MAX_ACCS * 2]; /* font/char bytes for each accidental */
	short octave;		/* 0 to 9 */
	short stepsup;		/* how many steps above middle line is note? */
	char headfont;		/* music char font of this note head */
	char headchar;		/* music char number of this note head */
	short headshape;	/* shape type of this note head */
	short notesize;		/* size of the note head */
	short tie;		/* if YES, tie this note to the same note in
				 * the next note group */
	short tiestyle;		/* what type of tie: L_[NORMAL|DOTTED|DASHED] */
	short tiedir;		/* should tie bulge UP or DOWN? */
	short tied_to_voice;	/* voice number of the note we are tying to */
	short tied_from_other;	/* is a tie coming from a different voice? */
	short slurred_from_other; /* is a slur coming from a different voice? */
	short acc_has_paren;	/* does the accidental have () around it? */
	short note_has_paren;	/* does the entire note have () around it? */
	short is_bend;		/* is last item in slurto list really "bend"? */

	/*
	 * On a tabnote staff, when there is a bend of <= 1/4 steps, the bent-
	 * to note isn't drawn.  Instead, smallbend is set to YES, and a small,
	 * curved line gets drawn.  In the input, the user specifies this by
	 * saying ^/ after the note.
	 */
	short smallbend;

	/*
	 * On a tablature staff, when a note is tied to the following group,
	 * the second note normally should not be printed.  (Its corresponding
	 * tabnote note will be, though.)  This flag says not to print this
	 * note.
	 */
	short inhibitprint;
};
/*
 * For tablature, the items above are used differently from the usual meaning.
 * Accidentals are never printed, so their coordinates are not used.  letter
 * is used to store the string number.  accidental is used to store the fret
 * number.  Inside the octave field three bit fields are used to store the bend
 * distance, as follows from high bits to low bits:
 *	integer part of bend; if no integer, store 0.
 *	numerator part of bend; if no fraction, store 0.
 *	denominator part of bend; if no fraction, store 0.
 *	If "full", store as if the integer 1 were given for bend (1-0-0).
 * The following macros are used for accessing these fields.  acc_has_paren is
 * used to indicate parentheses around the fret number, so an alternate name is
 * defined for it.  stepsup is used, in the parse phase only, to store number
 * of tick marks and fret values; see grpsyl.c for the details.  In later
 * phases it is set to its usual meaning, but note that middle of the staff
 * will not be a line if there are an even number of lines.
 */
#define	STRINGNO		letter
#define	FRETNO			acclist[0]
#define	BEND			octave
#define	BENDINT(note)		(((note).BEND >> \
				(BENDNUMBITS + BENDDENBITS)) & MAXBENDINT)
#define	BENDNUM(note)		(((note).BEND >> BENDDENBITS) & MAXBENDNUM)
#define	BENDDEN(note)		(((note).BEND >> 0) & MAXBENDDEN)
#define	TABOCT(inte, num, den)	(((inte) << (BENDNUMBITS + BENDDENBITS)) | \
				((num) << BENDDENBITS) | ((den) << 0))
#define HASBEND(note)		((note).BEND != 0)
#define HASNULLBEND(note)	((note).BEND == 1)
#define HASREALBEND(note)	(HASBEND(note) && ! HASNULLBEND(note))
#define FRET_HAS_PAREN		acc_has_paren
/*
 * During parsing, we temporarily save nticks and fret in the stepsup field.
 * Bits 0-8 hold the fret, bits 9-12 hold the nticks.  Any changes to MAXFRET
 * or MAXTICKS would have to be coordinated here.  These things are stored
 * temporarily in these fields since when we are doing parsing, we still need
 * to remember the pitch and accidental information, so can't put them in their
 * final place till a bit later.
 */
#define TMP_SAVE(note_p, nticks, fret)  \
		(note_p)->stepsup = (((nticks) & 0xf) << 8) | ((fret) & 0xff)
#define TMP_NTICKS(note_p)	(((note_p)->stepsup >> 8) & 0xf)
#define TMP_FRET(note_p)	((note_p)->stepsup & 0xff)

/* an array of these is allocated, for GRPSYL.withlist to point at */
struct WITH_ITEM {
	char *string;		/* "with" list item */
	short place;		/* where it is: PL_ABOVE/PL_BELOW/PL_UNKNOWN */
};

/*
 * Define the structure for holding information concerning a "group", which
 * consists of either a space, a rest, or a list of notes stemmed together
 * (or which would be stemmed together if they were shorter than whole notes);
 * or a syllable of lyrics.
 * NOTE:  When adding fields to this structure, update function map1note().
 */
struct GRPSYL {

	/* ======== ITEMS FOR GROUPS AND SYLLABLES ======== */
	short inputlineno;	/* which input line this structure came from */
	char *inputfile;	/* which file this came from (malloc'ed) */
	short staffno;		/* staff number */
	short vno;		/* voice (1 to MAXVOICES) or verse number */
	short grpsyl;		/* is it group or syllable? */

	/*
	 * Define the coords x, y, north, south, east, west, both relative
	 * and absolute.  The vertical relative coords are relative to the
	 * center line of the staff.  The horizontal relative coords are
	 * relative to the chord's x.  The NSEW coords define a rectangle
	 * surrounding the group; for groups, X goes through the center of
	 * the (normal) note heads; Y is the middle line of the staff.
	 * For syllables, Y is the baseline and X is the place that should
	 * line up with the chord, which is part way from the left edge toward
	 * the right edge based on lyricsalign, not counting any characters in
	 * <angle brackets> that precede or follow the real syllable.
	 *
	 * WARNING:  for groups, during the time when positions of phrase
	 * marks are being figured out, AN and AS are used in a strange way,
	 * denoting the offset from RN or RS where the phrase is.  But later
	 * they get set to their normal values.
	 */
	float c[NUMCTYPE];	/* coordinates */

	/*
	 * When grace groups exist, the following nongrace's west is extended
	 * leftwards to encompass the grace groups.  The following is the
	 * initial value of c[RW], before it is extended.
	 */
	float orig_rw;

	/*
	 * For multirests, basictime is negative the number of measures.
	 *
	 * For other is_meas==NO groups, whole is 1, half is 2, quarter is 4,
	 * etc.; BT_DBL (0) is double whole, BT_QUAD (-1) is quadruple whole
	 * (longa), and BT_OCT (-2) is octuple whole (maxima).
	 * 
	 * For is_meas==YES groups, basictime is the same as the preceding for
	 * measure rests, where it just tells which rest to draw, but for ms
	 * and all measures of measure repeats of any kind it is arbitrarily
	 * set to -1.
	 */
	short basictime;

	/*
	 * is_meas tells whether an "m" was used with the time in the input.
	 * This is used for "measure" rests, spaces, or repeats (mr, ms, mrpt).
	 * It is also used for all measures of a dblmrpt and quadmrpt.
	 * (Only mr can have a normal time value in addition to the "m"; it is
	 * stored as the basictime (defaults to 1) and tells which rest to
	 * draw.)  Their fulltime is the time signature.  mr and mrpt are
	 * centered in the measure.
	 */
	short is_meas;

	/*
	 * MRT_NONE, MRT_SINGLE, MRT_DOUBLE, MRT_QUAD.  Note that in the
	 * double and quad cases, it is set to that in every measure of it.
	 */
	short meas_rpt_type;	/* MRT_* */

	short is_multirest;	/* is this a multirest, YES or NO */

	short dots;		/* number of dots applied to time value */

	short tuploc;		/* none, start, inner, end, lone (for tuplet) */
	short tupcont;		/* number to print for the tuplet */

	/*
	 * Full time is basic time modified by dots, tuplets, etc.  It's the
	 * actual time duration, and thus for grace it's always 0.
	 */
	RATIONAL fulltime;

	float padding;		/* extra space to allow */

	/* ======== ITEMS FOR GROUPS ONLY ======== */
	short pvno;		/* pseudo voice number: normally equals vno,
				 * but when voice 3 is treated like voice 1 or
				 * or 2, that number is stored here */
				/* also used as scratch area in mkchords.c */
	short grpcont;		/* note(s), rest, or space; although normally
				 * meaningful only for groups, gram.y uses it
				 * as scratch while processing syllables */
	short grpvalue;		/* normal time value; or zero for grace group
				 * or for all-space chords in MIDI */
	short grpsize;		/* size of items in group */
	short headshape;	/* default shape of noteheads in group */
	short uncompressible;	/* is this space a "us" (used for space only)*/

	short beamloc;		/* none, start, inner, end (only note groups)*/
	float beamslope;	/* user specified angle of beam in degrees */
	float tupletslope;	/* user specified angle of bracket in degrees */
	short autobeam;		/* if autobeaming applies to this group, value
				 * can be NOITEM, STARITEM, INITEM, or ENDITEM*/

	/*
	 * If this GRPSYL is not for a rest, this will be NULL.  Otherwise, this
	 * points to an allocated array, similar to "c[]" above, except it is
	 * for the rest itself rather than the group.  In practice, the only
	 * difference is in the Y value:  for the group it is the middle staff
	 * line, but in restc it is the "logical" center of the rest.
	 */
	float *restc;

	/*
	 * Stem length applies to groups shorter than a whole note and groups
	 * joined by "alternation" beams.  It starts out based only on
	 * basictime and grpsize, but due to a beam or override, it could be
	 * changed.  Direction is up or down, and applies to all groups, even
	 * whole note groups (useful for figuring ties).   stemx is the
	 * horizontal position of the stem, relative to the X of the GRPSYL.
	 * These fields are valid only for note groups.
	 */
	float stemlen;
	short stemdir;		/* up or down */
	float stemx;

	/*
	 * beamto is always CS_SAME, except when this group is a notes or
	 * space group and is involved in cross staff beaming.  It then tells
	 * whether we are beamed with the staff above us or below us.  It is
	 * set for all the note and space groups on both staffs in the set.
	 * So on the top staff it's set to CS_BELOW, and on the bottom staff
	 * it's set to CS_ABOVE.
	 */
	short beamto;

	/*
	 * See setbeam(): this flag is used only by the first group in a
	 * beamed set.  It is needed because with cross staff stemming,
	 * beamstem is called on both passes.
	 */
	short ran_setbeam;

	/* see csbstempad(): this flag remembers if we did it */
	short padded_csb_stem;

	/*
	 * stemto is always CS_SAME, except when this group is a notes group
	 * and is involved in cross staff steming.  It then tells whether we
	 * are stemmed with the staff above us or below us.  When stemto is not
	 * CS_SAME, stemto_idx is an index into notelist[].  For CS_ABOVE,
	 * it indexes to the last note that is on the above staff.  For
	 * CS_BELOW, it indexes to the first note that is on the below staff.
	 */
	short stemto;
	short stemto_idx;

	/* YES if the last group in a subbeam */
	short breakbeam;

	/*
	 * printtup tells whether the user wants a tuplet number and bracket to
	 * be printed next to a note group.  If PT_NEITHER, neither will be.
	 * If PT_BOTH, both will be.  If PF_DEFAULT, at least the number will
	 * be.  The bracket will be too, unless the tuplet contains only one
	 * group, or if all the groups' beamlocs are equal to their tuplocs
	 * (the groups are already beamed as a unit).  If PT_NUMBER, the number
	 * (and only the number) will be printed.  In any case, printtup
	 * is set for each group in the tuplet.
	 *
	 * tupextend is set only for the groups of a tuplet of notes that has
	 * printtup == Y.  It is the vertical offset of where the tuplet
	 * bracket would be, from the AN or AS of the groups, as the case may
	 * be.  If the bracket would be above the groups, it is positive and
	 * relative to AN; else it is negative and relative to AS.  It is set
	 * even for the case where the bracket is not going to be printed, so
	 * that the tuplet number can still be placed as halfway between the
	 * invisible bracket's endpoints.
	 */
	short printtup;
	float tupextend;
	short tupside;		/* should number & bracket be above or below?*/

	short phraseside;	/* relevant side(s) for phrase mark space */

	/*
	 * nnotes says how many notes there are in the group.  An array of
	 * that many note structures must be malloc'ed and notelist set to
	 * point at it.  The notes are stored in order of descending pitch,
	 * regardless of the user's input ordering.  These fields are valid
	 * only for note groups.
	 * But for measure repeats (mrpt), even though they are GC_NOTES,
	 * nnotes is 0 and notelist is a null pointer.
	 */
	short nnotes;		/* no. of notes in group */
	struct NOTE *notelist;	/* list of notes in group */

	/* how many phrase start or end at this group */
	short phcount;		/* start */
	short ephcount;		/* end */

	/* whether each phrase is above or below */
	char phplace[PH_COUNT];
	/* whether each phrase is normal, dotted, or dashed */
	char phlinetype[PH_COUNT];

	/*
	 * If tie is set to YES, all notes in the group are to be tied to
	 * corresponding notes in the following group. The "tie" flag will
	 * also be set on each individual note in its NOTE struct, but it
	 * turns out to be handy to have the whole group marked here too.
	 * This field is valid only for note groups.
	 */
	short tie;

	/*
	 * The slash_alt field is used for slashes on a group or between
	 * pairs of groups (for tremolo, or just dividing the time value of
	 * the group(s).  0 means normal group; >0 means draw that many
	 * slashes through the stem of this group (or, if basictime < 2, where
	 * the stem would have been); <0 means draw negative that many slashes
	 * between this and the next group.  This field is valid only for note
	 * groups.
	 */
	short slash_alt;

	/*
	 * Record whether accidentals must be printed separately before each
	 * group in this chord on this staff.  That is the case when some
	 * common note(s) in them have contradictory accidentals.  Otherwise
	 * they are all printed to the left of all the groups.
	 */
	short sep_accs;
	/*
	 * This is used for rests only.  If not used, it is NORESTDIST.
	 * Otherwise, it is the vertical offset of the "center" of the rest
	 * (the part that normally is on the center line) from the center line.
	 * The rest is forced there.
	 */
	short restdist;

	/*
	 * Are we in a region of this voice where rests are to be aligned?
	 * (This is the current value of the alignrests parameter.  Because of
	 * the way we have to search forward and backwards from each rest, over
	 * multiple measures, it is simpler to store this value here on one
	 * pass, and then go through again and use it.)
	 */
	short alignrests;

	/*
	 * These are for the user-specified horizontal offset of the group from
	 * the chord's X.  The value is in stepsizes; negative to the left, and
	 * positive to the right.
	 */
	short ho_usage;		/* HO_* */
	float ho_value;		/* value to use when ho_usage is HO_VALUE */


	/* the X positions of dots are the same for every note in the group */
	float xdotr;		/* relative coord of dots:  x(dot)-x(group) */
	/*
	 * When symbols are to be drawn "with" a group, they are stored in the
	 * list below in order, starting from the group and moving outwards,
	 * in either or both directions.  When nwith is 0, there is no list.
	 * Otherwise, a list must be malloc'ed and the pointer must be set to
	 * point at it.  Each item in the list is a pointer to a structure,
	 * which contains a string of the item (also be malloc'ed) and the side
	 * it should go on (above, below, or unknown).  The user can choose
	 * above or below; otherwise parsing sets it to unknown and placement
	 * decides.
	 * These fields are not valid for space groups.
	 */
	short nwith;			/* number of symbols with group */
	struct WITH_ITEM *withlist;	/* list of symbols with group */

	short roll;		/* where is this group in a roll, if at all? */
	short rolldir;		/* is the roll's arrow UP, DOWN, or UNKNOWN? */
				/*  (with UNKNOWN, roll is up, but no arrow) */

	short clef;		/* clef to be printed with this group */
	short clef_vert;	/* pile clef vertically with the groups */

	short with_was_gtc;	/* "with" list was good til cancelled */

	/* ======== ITEMS FOR SYLLABLES ONLY ======== */
	char *syl;		/* malloc a place for the syllable */
	short sylposition;	/* points left of chord's X to start syl */

	/* ======== LINKAGE ======== */
	struct GRPSYL *prev;	/* point at previous group/syl in voice/verse*/
	struct GRPSYL *next;	/* point at next group/syl in voice/verse */
	struct GRPSYL *gs_p;	/* point at next group/syl in chord */
	struct GRPSYL *vcombdest_p; /* if vcombine made this a space, point at
				     * the destination GRPSYL */
};

/*
 * Define a structure for stacking rectangles.  It is used for STUFF,
 * accidentals, etc.  This same structure is used whether the stacking is from
 * north, south, east, or west.
 */
struct RECTAB {
	float n, s, e, w;	/* boundaries of a rectangle */
	short relevant;		/* is rectangle relevant? */
	short tried;		/* have we tried this one yet? */
};

/*
 * Define the structure for a chord.
 */
struct CHORD {
	/*
	 * Define the coords of a CHORD.  For vertical, only the absolute ones
	 * are set.  They are set the same as the FEED for this score, and they
	 * are used only for drawing bounding boxes with MUP_BB.  As for
	 * horizontal, the relative coords are relative to the score's
	 * (x, y).  Basically, west and east are set out just far enough to
	 * hold the biggest GRPSYL in the CHORD.  However, for both groups and
	 * syllables there is special code that allows parts of them to stick
	 * out, when the overlap would be harmless even if the CHORDs end up
	 * being packed tightly together.
	 */
	float c[NUMCTYPE];	/* coordinates */
	float width;		/* c[RE] - c[RW], which equals c[AE] - c[AW] */
	float fullwidth;	/* dist from c[RX] of this chord to c[RX] of */
				/*  next (or to bar line if last in measure) */

	RATIONAL starttime;	/* starting time of chord within its measure */
	RATIONAL duration;	/* duration of the chord */
	float pseudodur;	/* a function of duration; proportional to */
				/* width this chord will "deserve" */
	short uncollapsible;	/* YES or NO:  NO means that every GPRSYL
				 * passing through this time duration (whether
				 * or not in this chord) consists of
				 * collapsible spaces */

	struct CHORD *ch_p;	/* point at next chord in list */
	struct GRPSYL *gs_p;	/* point at first group or syllable in chord */
};

/*
 * Define the structure that contains info concerning chord grids that are
 * going to be printed at the end of the song.  Only one instance of this
 * structure exists.
 */
struct ATEND_INFO {
	/* number of different grids actually used in the song */
	int grids_used;

	/*
	 * Must grid dictionary be put on a separate page, following the last
	 * page of music (because it doesn't fit with the music)?  YES or NO.
	 */
	int separate_page;

	/*
	 * This is a malloc'ed array of pointers to the grids to be printed.
	 * Placement sets it up and sorts it.
	 */
	struct GRID **grid_p;

	int grids_per_row;	/* no. of grids to print per row */
	float firstgrid_x;	/* X coord of grids in the first column */
	float horz_sep;		/* dist between X of neighboring grids */

	/*
	 * Other variables, which need versions for left and right pages.
	 */
	struct ATEND_VARS {
		int rows_per_page;	/* no. of rows of grids on one page */
		float firstgrid_y;	/* Y coord of grids in the first row */
		float vert_sep;		/* dist between Y of neighboring grids*/
	} left, right;
};

/*
 * Define a symbol for each structure type that can be inside a union, so
 * that we can record which member of the union is being used.
 */
#define	S_SSV		(0)
#define	S_FEED		(1)
#define	S_CLEFSIG	(2)
#define	S_PRHEAD	(3)
#define	S_CHHEAD	(4)
#define	S_STAFF		(5)
#define S_LINE		(6)
#define S_CURVE		(7)
#define	S_BAR		(8)
#define	S_BLOCKHEAD	(9)

/*
 * The following contains a union of all the structures that can occur in the
 * main linked list, set up by yyparse().
 */
struct MAINLL {
	short str;	/* which structure in the union is now being used? */
	short inputlineno; /* which input line this structure came from */
	char *inputfile;	/* which file this came from (malloc'ed) */
	union {			/* malloc'ed structures to be pointed at */
		struct SSV *ssv_p;	/* score/staff/voice context info */
		struct FEED *feed_p;	/* score and/or page feed */
		struct CLEFSIG *clefsig_p;    /* print clef and/or sigs? */
		struct PRHEAD *prhead_p;/* head of list to print things */
		struct CHHEAD *chhead_p;/* head of chord list for a measure */
		struct STAFF *staff_p;	/* staff info from data context */
		struct LINE *line_p;	/* line info */
		struct CURVE *curve_p;	/* curve info */
		struct BAR *bar_p;	/* bar line info from data context */
		struct BLOCKHEAD *blockhead_p;	/* info for a "block" context */
	} u;
	struct MAINLL *prev;	/* previous structure in linked list */
	struct MAINLL *next;	/* next structure in linked list */
};
/*
 * The structures in the main linked list must occur in the order as shown
 * below, by the end of the placement phase.  There are optional initial SSVs,
 * then two alternative sets of structures that repeat, then some optional
 * final structures.  The list shows [in brackets] which phase of Mup inserts
 * the structure:  parse or placement.  (The third phase, print, doesn't insert
 * any structures; nor does the midi phase, which can replace the print phase.)
 *
 * 0 or more SSVs [parse]
 * LOOP 1 or more times:
 *    EITHER a measure:
 *       0 or 1 FEED; 1 is required in 1st measure & after block[parse or place]
 *       0 or 1 CLEFSIG (at start of a score); 1 iff a FEED precedes [place]
 *       1 CHHEAD [place]
 *       1 or more STAFFs.  They are ordered by staff number. [parse]
 *       0 or more LINEs and/or CURVEs and/or PRHEADs, any order[parse or place]
 *       1 BAR [parse]
 *       0 or more SSVs [parse or place]
 *       0 or 1 CLEFSIG (after a bar line, not at start of a score) [place]
 *    OR a block:
 *       1 FEED [parse]
 *       1 BLOCKHEAD [parse]
 *       0 or more SSVs [parse]
 *    END_EITHER
 * END_LOOP
 * 0 or more LINEs and/or CURVEs and/or PRHEADs and 1 optional FEED; the FEED
 *			is required if a block precedes [parse]
 */
#endif
