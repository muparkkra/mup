
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

/* This file contains functions for dealing with symbol tables.
 * There is a symbol table that maps the names of location variables
 * to the addresses of their array of coordinate info,
 * a symbol table to map headshape names to the list of shapes,
 * a table to map time signatures to beamstyle and/or timeunit values,
 * and a symbol table to map grid names to definitions of the grids.
 * Symbol names are hashed for fast lookup.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"


/* shapes for quarter and shorter, half, whole, double whole, quad, oct */
#define MIN_SHAPE_DURS  (4)
#define MAX_SHAPE_DURS	(6)
/* UP and DOWN */
#define MAX_STEM_DIRS	(2)

/* First index of headchar and headfont is based on stem direction;
 * also index of ystem_off. */
#define STEMINDEX(stemdir)		( (stemdir) == UP ? 0 : 1 )
/* Second index of headchar and headfont is based on basictime */
#define HCDUR(basictime)	( (basictime) > 2 ? 0 : 3 - (basictime) )
/* First index of Nhead_map */
#define FONTINDEX(font)		(font - FONT_MUSIC)
/* Second index of Nhead_map */
#define CHARINDEX(ch)		((ch - FIRST_CHAR) & 0xff)

struct HDSHAPEINFO {
	/* We give each headshape instance a unique index number,
	 * as a compact way to refer to it. */
	short	index;

	/* This says what notehead characters to use for this shape.
	 * The first dimension is for stem direction, the second for
	 * duration (quarter and shorter, half, whole, double whole).
	 */
	unsigned char	headchar[MAX_STEM_DIRS][MAX_SHAPE_DURS];
	/* This array parallels the notehead array, saying which music
	 * font the character is in. */
	char	headfont[MAX_STEM_DIRS][MAX_SHAPE_DURS];
};

/* How many "headshape" entries we allow. This must be small enough to
 * fit in the number of bits allowed for shape indexes in GRPSYL and NOTE.
 * Entry 0 isn't used since we need an "unknown" value.
 */
#define MAX_SHAPE_ENTRIES	(32)

/* These are the predefined headshape entries. Users can define more.
 * We define them here using the same syntax as user would, so we can
 * use the same code to add to our internal table. */
struct SHAPENAMES {
	char *clan_name;	/* name for the set of shapes */
	char *member_names;	/* The names of the 4 shapes in the set */
} Predef_shape_names[] = {
	{ "norm",	"4n 2n 1n dblwhole quadwhole octwhole" },
	{ "x",		"xnote diamond diamond dwhdiamond quadwhole octwhole" },
	{ "allx",	"xnote xnote xnote xnote xnote xnote" },
	{ "diam",	"filldiamond diamond diamond dwhdiamond quadwhole octwhole" },
	{ "blank",	"blankhead blankhead blankhead blankhead blankhead blankhead" },
	{ "righttri",	"u?fillrighttriangle u?righttriangle u?righttriangle u?dwhrighttriangle quadwhole octwhole" },
	{ "isostri",	"fillisostriangle isostriangle isostriangle dwhisostriangle quadwhole octwhole" },
	{ "rect",	"fillrectangle rectangle rectangle dwhrectangle quadwhole octwhole" },
	{ "pie",	"fillpiewedge piewedge piewedge dwhpiewedge quadwhole octwhole" },
	{ "semicirc",	"fillsemicircle semicircle semicircle dwhsemicircle quadwhole octwhole" },
	{ "allslash",	"fillslashhead fillslashhead fillslashhead fillslashhead fillslashhead fillslashhead" },
	{ "slash",	"fillslashhead slashhead slashhead dwhslashhead quadwhole octwhole" },
	{ 0, 0 }
};

/* Information about characters that are allowed to be noteheads */
struct HEADINFO {
	char ch;	/* code number 32-127 */
	char font;	/* FONT_MUSIC*  */
	float ystem_off[MAX_STEM_DIRS];	/* stepsizes from y to end stem */
};

/* Predefined note head music characters and their attributes. */
struct HEADDATA {
	char *name;
	struct HEADINFO info;
} Predef_headinfo[] = {
  { "octwhole",		{ C_OCTWHOLE,		FONT_MUSIC, { 0.0, 0.0 } } },
  { "quadwhole",	{ C_QUADWHOLE,		FONT_MUSIC, { 0.0, 0.0 } } },
  { "dblwhole",		{ C_DBLWHOLE,		FONT_MUSIC, { 0.0, 0.0 } } },
  { "altdblwhole",	{ C_ALTDBLWHOLE,	FONT_MUSIC, { 0.0, 0.0 } } },
  { "1n",		{ C_1N,			FONT_MUSIC, { 0.0, 0.0 } } },
  { "2n",		{ C_2N,			FONT_MUSIC, { 0.25, -0.25 } } },
  { "4n",		{ C_4N,			FONT_MUSIC, { 0.25, -0.25 } } },
  { "xnote",		{ C_XNOTE,		FONT_MUSIC, { 1.0, -1.0 } } },
  { "dwhdiamond",	{ C_DWHDIAMOND,		FONT_MUSIC, { 0.0, 0.0 } } },
  { "diamond",		{ C_DIAMOND,		FONT_MUSIC, { 0.0, 0.0 } } },
  { "filldiamond",	{ C_FILLDIAMOND,	FONT_MUSIC, { 0.0, 0.0 } } },
  { "dwhrighttriangle",	{ C_DWHRIGHTTRIANGLE,	FONT_MUSIC2, { 0.0, 0.0 } } },
  { "righttriangle",	{ C_RIGHTTRIANGLE,	FONT_MUSIC2, { 0.0, 0.9 } } },
  { "fillrighttriangle",{ C_FILLRIGHTTRIANGLE,	FONT_MUSIC2, { 0.0, 0.9 } } },
  { "udwhrighttriangle",{ C_UDWHRIGHTTRIANGLE,	FONT_MUSIC2, { 0.0, 0.9 } } },
  { "urighttriangle",	{ C_URIGHTTRIANGLE,	FONT_MUSIC2, { -0.9, 0.0 } } },
  { "ufillrighttriangle",{ C_UFILLRIGHTTRIANGLE,FONT_MUSIC2, { -0.9, 0.0 } } },
  { "dwhrectangle",	{ C_DWHRECTANGLE,	FONT_MUSIC2, { -0.9, 0.0 } } },
  { "rectangle",	{ C_RECTANGLE,		FONT_MUSIC2, { 0.0, 0.0 } } },
  { "fillrectangle",	{ C_FILLRECTANGLE,	FONT_MUSIC2, { 0.0, 0.0 } } },
  { "dwhisostriangle",	{ C_DWHISOSTRIANGLE,	FONT_MUSIC2, { -0.8, -0.8 } } },
  { "isostriangle",	{ C_ISOSTRIANGLE,	FONT_MUSIC2, { -0.8, -0.8 } } },
  { "fillisostriangle",	{ C_FILLISOSTRIANGLE,	FONT_MUSIC2, { -0.8, -0.8 } } },
  { "dwhpiewedge",	{ C_DWHPIEWEDGE,	FONT_MUSIC2, { 0.1, 0.2 } } },
  { "piewedge",		{ C_PIEWEDGE,		FONT_MUSIC2, { 0.1, 0.2 } } },
  { "fillpiewedge",	{ C_FILLPIEWEDGE,	FONT_MUSIC2, { 0.1, 0.2 } } },
  { "dwhsemicircle",	{ C_DWHSEMICIRCLE,	FONT_MUSIC2, { 0.8, 0.8 } } },
  { "semicircle",	{ C_SEMICIRCLE,		FONT_MUSIC2, { 0.8, 0.8 } } },
  { "fillsemicircle",	{ C_FILLSEMICIRCLE,	FONT_MUSIC2, { 0.8, 0.8 } } },
  { "blankhead",	{ C_BLANKHEAD,		FONT_MUSIC2, { 0.0, 0.0 } } },
  { "slashhead",	{ C_SLASHHEAD,		FONT_MUSIC2, { 1.8, -1.8 } } },
  { "fillslashhead",	{ C_FILLSLASHHEAD,	FONT_MUSIC2, { 1.8, -1.8 } } },
  { "dwhslashhead",	{ C_DWHSLASHHEAD,	FONT_MUSIC2, { 1.8, -1.8 } } },
  { 0,			{ 0,			0, 	     { 0.0, 0.0 } } }
};

/*
 * This struct provides a mapping from a time signature to all the
 * beamstyle and timeunit values to be associated with that time signature.
 *
 * The [0][0] entry is used for the C_SCORE value.
 * The entries [0][1] through [0][MAXVOICES+1] are unused.
 * The [s][0] entries are used for C_STAFF values where 1 <= s <= MAXSTAFFS
 * The [s][v] entries are used for C_VOICE values where 1 <= s <= MAXSTAFFS
 *			and 1 <= v <= MAXVOICES
 * There will be one of these allocated for each time signature used in the
 * input, if and only if the user also specified at least one beamstyle
 * or timeunit while that time signature was in effect.
 */
struct SSVTABLES {
	struct SSV *beamstyle_table[MAXSTAFFS+1][MAXVOICES+1];
	struct SSV *timeunit_table[MAXSTAFFS+1][MAXVOICES+1];
};

/* This is used for saveparms, to save everything we need to be able
 * to do a restoreparms later. */
struct SAVEPARMS_INFO {
	struct MAINLL *mll_p;	/* at time of saveparms */
	int time2beamstyle_index;	/* index of saved mapping table */
	/* These next 3 allow us restore our state if the user is doing
	 * alternating time signatures, to reset to the first in the set. */
	char *saved_alt_timesig_list;
	char *saved_next_alt_timesig;
	short saved_tsig_visibility;
};
	

/* information about a symbol: its name and current value.
 * This is used for location tags, chord grids, headshapes, and time signatures.
 * Note that in the case of location tags,
 * if the same name is used in a later measure, just the coordlist_p will
 * change, and when the symbol table is queried for the value of a symbol,
 * it will get the current value for that symbol */
struct Sym {
	char	*symname;
	union {
		float	*coordlist_p;	/* when used for location tags, this is
					 * where its AX, RX, etc values are */
		struct GRID *grid_p;	/* when used for grids */
		struct HDSHAPEINFO *shapeinfo_p;/* when used for headshapes */
		struct HEADINFO *noteinfo_p;	/* when used for note head info */
		/* Info about beamstyles and/or timeunits associated with
		 * the time signature given by the symname. */
		struct SSVTABLES *ssvtables_p;
		int index;		/* index into an array of saved
					 * macro tables */
		struct SAVEPARMS_INFO *saveparms_info_p; /* for saveparms */
	} val;
	struct Sym *next;		/* for collision chain off hash table */
};

/* symbol hash table size-- if this changes, hash() has to change accordingly */
#define SYMTBLSIZE	(128)

/* this is the symbol table for location tags */
static struct Sym *Tag_table[SYMTBLSIZE];

/* this is the symbol table for guitar grids. It is malloc-ed at runtime
 * only if needed */
static struct Sym **Grid_table;

/* This is the symbol table for headshapes */
static struct Sym *Shape_table[SYMTBLSIZE];

/* This maps headshape indexes to the corresponding info.
 * Element 0 is unused, since index 0 means "unknown" shape.
 */
static struct Sym *Shape_map[MAX_SHAPE_ENTRIES];
/* How many Shape_map entries are actually used */
static short Shape_entries = 0;

/* This is the symbol table for noteheads, to get stem offsets */
static struct Sym  *Nhead_table[SYMTBLSIZE];

/* This maps notehead character codes to the stem offset info */
static struct HEADINFO *Nhead_map[NUM_SYMFONTS][MAX_CHARS_IN_FONT];

/* This is used to remember what beamstyle and/or timeunit values were
 * associated with time signatures. This is really only needed during parse,
 * and then only if user specifies beamstyle or timeunit somewhere.
 */
static struct Sym *Time_map[SYMTBLSIZE];
/* If user does saveparms, this points to a malloced array of saved copies
 * of the TimeMap, for doing restore. */
static struct Sym ***Saved_time_maps = 0;
/* This is how many saved time maps we have. */
static int Num_time_maps = 0;

/* This is the table for named saved macros */
static struct Sym *Saved_macs_table[SYMTBLSIZE];

/* This is the table for named saved parameters */
static struct Sym *Saved_parms_table[SYMTBLSIZE];

/* Internal name for tag used to store the virtual _win coords for blocks */
char Blockwin[] = "~blockwin";
/* This points to where in the symbol table
 * to save the pointer to the coord array for blocks.
 * Having this pointer is a speed optimization,
 * to save us from having to look it up every time. */
float **Blockcoord_p_p;


/* static functions */
static struct GRID *parse_grid P((char *griddef));
static struct Sym *add2tbl P((char *symname, struct Sym **table));
static struct Sym *findSym P((char *symname, struct Sym **table));
static int hash P((char *string));
static int coordhash P((float *key));
static void rep_ref P((float **old_ref_p_p, float **new_ref_p_p));
static void delete_coord P((float *coord_p));
static int is_valid_notehead P((int ch, int font));
static void add_head P((char *name, struct HEADINFO *info_p));
static int save_time2beamstyle P((void));


/* size of Coordinfo hash table. Should be prime */
#define COORDTBLSIZE  (271)

static struct COORD_INFO *Coord_table[COORDTBLSIZE];


/* Add predefined values to the symbol tables */

void
init_symtbl()

{
	struct Sym *sym_p;
	int i;

	addsym("_page", _Page, CT_BUILTIN);
	addsym("_cur", _Cur, CT_BUILTIN);
	addsym("_score", _Score, CT_BUILTIN);
	for (i = 1; i <= MAXSTAFFS; i++) {
		char sname[12];
		(void) sprintf(sname, "_staff.%d", i);
		addsym(sname, _Staff[i], CT_BUILTIN);
	}

	/* Blocks each have their own virtual _win,
	 * so we put a placeholder in the symbol table
	 * and save a pointer to its tag's coord pointer.
	 * Then we can update the coords via set_win_coords().
	 */
	addsym(Blockwin, 0, CT_BUILTIN);
	sym_p = findSym(Blockwin, Tag_table);
	if (sym_p == 0) {
		pfatal("couldn't find %s coord right after inserting it!",
							Blockwin);
	}
	Blockcoord_p_p = &(sym_p->val.coordlist_p);

	/* Put the predefined notehead shapes into table. */
	for (i = 0; Predef_headinfo[i].name != 0; i++) {
		add_head(Predef_headinfo[i].name, &(Predef_headinfo[i].info));
	}

	/* Put the predefined head shapes in the shapes table */
	for (i = 0; Predef_shape_names[i].clan_name != 0; i++) {
		add_shape(Predef_shape_names[i].clan_name,
					Predef_shape_names[i].member_names);
	}
}


/* add a symbol to the table if not already there and fill in its coordlist_p
 * in symbol table. */

void
addsym(symname, coordlist_p, coordtype)

char *symname;		/* what to add to table */
float *coordlist_p;	/* set of 13 coordinates associated with symbol */
int coordtype;		/* CT_BAR, CT_GRPSYL, etc */

{
	struct Sym *sym_p;	/* pointer to info about symbol in hash tbl */


	debug(4, "addsym(symname=%s coordlist_p=0x%lx, coordtype=%d)",
					symname, coordlist_p, coordtype);

	/* error if user tries to set a builtin */
	if (coordtype != CT_BUILTIN) {
		if ((strcmp(symname, "_win") == 0) ||
				((sym_p = findSym(symname, Tag_table)) != 0 &&
				is_builtin_tag(sym_p->val.coordlist_p) == YES)) {
			l_yyerror(Curr_filename, yylineno,
				"Cannot set a builtin tag '%s'", symname);
			return;
		}
	}

	/* find in symbol table or add if not yet there */
	sym_p = add2tbl(symname, Tag_table);

	/* fill in coordlist pointer */
	sym_p->val.coordlist_p = coordlist_p;

	/* put entry in coord table */
	add_coord(coordlist_p, coordtype);
}


/* add a symbol to the specified hash table */

static struct Sym *
add2tbl(symname, table)

char *symname;		/* what to add */
struct Sym **table;	/* which table to add to */

{
	struct Sym *sym_p;
	int h;			/* hash number of symbol */

	if ((sym_p = findSym(symname, table)) == (struct Sym *) 0) {

		/* not in list before. Add it */
		MALLOC(Sym, sym_p, 1);
		MALLOCA(char, sym_p->symname, strlen(symname) + 1);
		(void) strcpy(sym_p->symname, symname);
		h = hash(symname);

		/* link onto front of list off hash table */
		sym_p->next = table[h];
		table[h] = sym_p;
	}
	return(sym_p);
}


/* given a symbol name, return pointer to its symbol table entry, or NULL
 * if none */

static struct Sym *
findSym(symname, table)

char *symname;		/* which symbol to look for */
struct Sym **table;	/* which table to look in */

{
	struct Sym *sym_p;	/* symbol info currently being checked
				 * for match with symname */
	int h;			/* hash number */


	h = hash(symname);
	/* go down the linked list (of hash collisions) off the table
	 * searching for match */
	for (sym_p = table[h]; sym_p != (struct Sym *) 0;
						sym_p = sym_p->next) {
		if (strcmp(sym_p->symname, symname) == 0) {
			return(sym_p);
		}
	}
	return((struct Sym *) 0);
}


/* For top/bot/top2/bot2/block we temporarily set the ~blockwin tag to point
 * to the appropriate blockhead's coord array. Before doing a set_win for
 * any of those, this function should be called to set things up, and
 * it should be called again afterwards with 0 to mark we are no longer
 * inside a block.
 */

void
set_win_coord(coord_p)

float *coord_p;

{
	*Blockcoord_p_p = coord_p;
}


/* The _score and _staff values change on every score,
 * so this function needs to be called at the beginning of each score
 * to set them to the correct "current" values.
 * Since user's _score  references are saved just as pointers at
 * parse time, we set all those pointers to the static _Score and then
 * update what it points to a print time.
 * The _Staff array gets updated with the staff values.
 */

void
set_score_coord(mll_p)

struct MAINLL *mll_p;	/* expected to point to a FEED */

{
	float pseudo_bar_x;
	float *staff_coord_p;	/* point to a STAFF c[] */
	struct MAINLL *m_p;
	int staff;


	/* Find the X of the pseudo bar. That will be staff's X */
	if (mll_p->next != 0 && mll_p->next->str == S_CLEFSIG &&
				mll_p->next->u.clefsig_p->bar_p != 0) {
		pseudo_bar_x = mll_p->next->u.clefsig_p->bar_p->c[AX];
	}
	else {
		/* Must be a BLOCK */
		return;
	}

	memcpy(_Score, mll_p->u.feed_p->c, sizeof(_Score));
	
	for (m_p = mll_p->next, staff = 0; staff <= MAXSTAFFS && m_p != 0;
							m_p = m_p->next) {
		switch (m_p->str) {
		case S_BAR:
			/* Must be past all visible staffs */
			staff = MAXSTAFFS+1;
			break;
		case S_STAFF:
			staff = m_p->u.staff_p->staffno;
			if (svpath(staff, VISIBLE)->visible == YES) {
				staff_coord_p = m_p->u.staff_p->c;
				_Staff[staff][AN] = staff_coord_p[AN];
				_Staff[staff][AS] = staff_coord_p[AS];
				_Staff[staff][AY] = staff_coord_p[AY];
				/* staff AE matches score's */
				_Staff[staff][AE] = _Score[AE];
				/* staff AX is pseudobar */
				_Staff[staff][AX] = pseudo_bar_x;
				/* staff AW is same as score's AX
				 * when there is no label, so init
				 * to that. If there is a label,
				 * set_staff_x() call later will fix */
				_Staff[staff][AW] = _Score[AX];
			}
			break;
		default:
			/* something irrelevant */
			break;
		}
	}

	/* adjust _Staff AX values for staffs that have labels */
	set_staff_x();
}


/* Given a tag name, return its value (a pointer to the coordinate array
 * containing the AX, AY, etc of the variable).
 * If the tag is not found, an error is printed and 0 is returned.
 * If the ref_p_p is non-null,
 * save that value as something that references the tag. That way if the
 * tag gives moved somewhere else, we can update all the references.
 * If referencing the value of a coord that can never move (a builtin coord
 * like _win), ref_p_p may be null.
 */

float *
symval(symname, ref_p_p)

char *symname;		/* which symbol to look up */
float **ref_p_p;	/* address of reference to the tag */

{
	struct Sym *sym_p;	/* symbol info currently being checked */


	/* _win is a special case: its actual symbol depends on context */
	if (strcmp(symname, "_win") == 0) {
		/* If we're inside a block, use its coord,
		 * else use the global _Win */
		if (*Blockcoord_p_p != 0) {
			return(*Blockcoord_p_p);
		}
		else {
			return(_Win);
		}
	}
	
	/* If an _staff.N tag, validate that that staff is currently valid */
	if (strncmp(symname, "_staff.", 7) == 0) {
		char *endptr;
		int s;

		s = strtol(symname+7, &endptr, 0);
		if (*endptr == '\0' && endptr != symname + 7) {
			if (s < 1 || s > Score.staffs) {
				l_yyerror(Curr_filename, yylineno,
				"%s reference not allowed; staff number must between 1 and %d", symname, Score.staffs);
				return((float *) 0);
			}
			else if (svpath(s, VISIBLE)->visible == NO) {
				return((float *) 0);
			}
		}
	}
	/* find the symbol table entry */
	if ((sym_p = findSym(symname, Tag_table)) != (struct Sym *) 0) {
		if (sym_p->val.coordlist_p != (float *) 0) {
			/* save reference information */
			if (ref_p_p != 0) {
				save_tag_ref(sym_p->val.coordlist_p, ref_p_p);
			}
			return(sym_p->val.coordlist_p);
		}
	}

	/* whoops! not in table */
	l_yyerror(Curr_filename, yylineno,
			"reference to uninitialized location tag '%s'",
			symname);
	return((float *) 0);
}	


/* Given a tag and a reference to it, add the reference to that tag's list
 * of references. */

void
save_tag_ref(c_p, tag_ref_p_p)

float *c_p;		/* A tag's c[] */
float **tag_ref_p_p;	/* A reference to c_p */

{
	struct COORD_INFO *coordinfo_p;	/* Info about tag, including its
					 * reference list that we will add to */
	struct COORD_REF *ref_p;	/* To hold the saved info */


	if ((coordinfo_p = find_coord(c_p)) == 0) {
		/* Must be an internal tag like _win. Those never move,
		 * so we don't need to save information about references
		 * to them. The caller may not realize what they are dealing
		 * with is an internal tag, and rather than make all the
		 * callers check for that, we let deal with it here. */
		return;
	}
	/* Get space to save the reference */
	CALLOC(COORD_REF, ref_p, 1);
	ref_p->ref_p_p = tag_ref_p_p;

	/* Link onto reference list */
	ref_p->next = coordinfo_p->ref_list_p;
	coordinfo_p->ref_list_p = ref_p;
}


/* add item to grid table if not already there. */

void
add_grid(name, griddef)

char *name;		/* chord name */
char *griddef;		/* user's definition of the grid */

{
	char *internal_name;	/* internal format name */
	char *grid_name;	/* permanent copy of internal_name */
	char *symname;		/* ASCII-ized name */
	char *key;		/* permanent copy of symname, key into hash tbl */
	struct Sym *sym_p;
	struct GRID *grid_p;


	/* if table doesn't exist yet, create it */
	if (Grid_table == 0) {
		int g;

		MALLOCA(struct Sym *, Grid_table, SYMTBLSIZE);
		for (g = 0; g < SYMTBLSIZE; g++) {
			Grid_table[g] = 0;
		}
	}

	/* Do all the transforms to get into internal form with
	 * accidentals as music characters. For historical reasons,
	 * this has to be done in several steps, in just the right order. */
	internal_name = modify_chstr(name, TM_CHORD);
	internal_name = fix_string(internal_name, Score.font, Score.size,
						Curr_filename, yylineno);
	/* convert accidentals, then convert to all ASCII */
 	grid_name = tranchstr(internal_name, -1);
	symname = ascii_str(grid_name, YES, NO, TM_CHORD);

	if (strlen(symname) == 0) {
		yyerror("empty grid name not allowed");
		return;
	}

	if ((sym_p = findSym(symname, Grid_table)) != 0) {
		l_warning(Curr_filename, yylineno,
			"duplicate definition of grid for '%s', discarding previous",
			symname);
		/* discard old definition, use new one */
		if (sym_p->val.grid_p != 0) {
			if (sym_p->val.grid_p->name != 0) {
				FREE(sym_p->val.grid_p->name);
			}
			FREE(sym_p->val.grid_p);
			sym_p->val.grid_p = 0;
		}
		key = sym_p->symname;
	}
	else {
		/* make permanent copy of key */
		MALLOCA(char, key, strlen(symname) + 1);
		(void) strcpy(key, symname);
	}

	if ((grid_p = parse_grid(griddef)) != 0) {
		/* it's good, so put in hash table */
		sym_p = add2tbl(key, Grid_table);

		/* fill in the name and grid pointer */
		grid_p->name = grid_name;
		sym_p->val.grid_p = grid_p;;
		{
			/* buffer--2 digits and a space for each string, plus null */
			char fretlist[3 * MAXTABLINES + 1];
			int f;
			for (f = 0; f < grid_p->numstr; f++) {
				(void) sprintf(fretlist + 3 * f, "%3d",
							 grid_p->positions[f]);
			}
			fretlist[sizeof(fretlist) - 1] = '\0';
			debug(4, "added grid '%s' key '%s' with %d strings: %s curve %d to %d",
					name, key,
					grid_p->numstr, fretlist,
					grid_p->curvel, grid_p->curver);
		}
	}
}


/* take the user's grid definition, like "2 (3 1) o x -"
 * and populate a GRID struct with the info. If not parse-able,
 * return 0 */

static struct GRID *
parse_grid(griddef)

char *griddef;	/* user's definition of the grid */

{
	struct GRID *grid_p;	/* the malloc-ed grid to populate & return */
	int error = NO;
	int value;

	MALLOC(GRID, grid_p, 1);
	grid_p->numstr = 0;
	grid_p->curvel = grid_p->curver = 0;
	grid_p->used = NO;
	/* the +2 is to skip the font/size bytes */
	for (griddef += 2; *griddef != '\0' && error == NO; griddef++) {
		/* init to something illegal */
		value = -1000;

		while (*griddef == ' ' || *griddef == '\t') {
			griddef++;
		}
		if (*griddef == '\0') {
			break;
		}

		if ( isdigit(*griddef) ) {
			value = *griddef - '0';
			if (isdigit(*(griddef+1))) {
				value *= 10;
				griddef++;
				value += *griddef - '0';
			}
			if (value == 0) {
				yyerror("fret of zero not allowed; use 'o' for open or '-' for nothing");
				error = YES;
			}
		}
		else if (*griddef == '(') {
			if (grid_p->curvel != 0) {
				yyerror("only one '(' allowed in grid definition");
				error = YES;
			}
			else {
				grid_p->curvel = grid_p->numstr + 1;
			}
		}
		else if (*griddef == ')') {
			if (grid_p->curver != 0) {
				yyerror("only one ')' allowed in grid definition");
				error = YES;
			}
			else if (grid_p->curvel == 0) {
				yyerror("missing '(' in grid definition");
				error = YES;
			}
			else if (grid_p->curvel == grid_p->numstr) {
				yyerror("curve in grid definition must encompass more than one string");
				error = YES;
			}
			else {
				grid_p->curver = grid_p->numstr;
			}
		}
		else if (*griddef == 'o') {
			value = 0;
		}
		else if (*griddef == 'x') {
			value = -1;
		}
		else if (*griddef == '-') {
			value = -2;
		}
		else {
			yyerror("invalid grid specification");
			FREE(grid_p);
			return (0);
		}

		if (value >= -2) {
			/* We found a fret value (not parentheses).
			 * Next better be white space sort of thing */
			char c;
			c = *(griddef + 1);
			if (c != ' ' && c != '\t' && c != '(' && c != ')'
							&& c != '\0') {
				yyerror("missing white space in grid specification");
				error = YES;
			}
			else if (grid_p->numstr < MAXTABLINES) {
				/* all is well; save info for current string */
				grid_p->positions[grid_p->numstr] = value;
				(grid_p->numstr)++;
			}
			else {
				yyerror("too many strings in grid specification");
				error = YES;
			}
		}
	}

	if (error == NO && grid_p->curvel != 0 && grid_p->curver == 0) {
		yyerror("missing ')' in grid specification");
		error = YES;
	}
	
	if (grid_p->numstr < 1) {
		yyerror("grid must include at least one string");
		error = YES;
	}

	/* every curve must have at least one real fret in it */
	if (grid_p->curvel != 0) {
		int s;
		/* the -1 is because curves start at 1, but positions at 0 */
		for (s = grid_p->curvel - 1; s <= grid_p->curver - 1; s++) {
			if (grid_p->positions[s] > 0) {
				break;
			}
		}
		if (s == grid_p->curver) {
			yyerror("grid curve must include at least one fret number, not just x, o, and -");
			error = YES;
		}
	}

	if (error == YES) {
		/* clean up */
		FREE(grid_p);
		grid_p = 0;
	}
	return(grid_p);
}


/* Locate a named GRID in the grid hash table. Returns 0 if not found,
 * else pointer to desired GRID. */

struct GRID *
findgrid(name)

char *name;	/* find GRID with this chord name */

{
	struct Sym *sym_p;
	char *ascii_name;
	int length;
	int first_char;

	if (Grid_table == 0) {
		/* no grids defined */
		return((struct GRID *) 0);
	}

	ascii_name = ascii_str(name, YES, NO, TM_CHORD);
	/* Chord names in STUFF have a space padding at the end of them
	 * unless they are in a box or circle, so strip that off for matching
	 * the name. We store the grid name without the space, because for
	 * grid printing, we want the name centered without end padding. */
	length = strlen(ascii_name);
	first_char = ((int)*(name+2)) & 0xff;
	if (first_char != STR_BOX && first_char != STR_CIR
					&& ascii_name[length-1] == ' ') {
		ascii_name[length-1] = '\0';
	}

	/* look it up */
	sym_p = findSym(ascii_name, Grid_table);

	return (sym_p ? sym_p->val.grid_p : 0);
}


/* Function to iterate through all the GRIDs. First time, call it with
 * argument of zero, and it returns the first GRID it finds. To walk
 * through the list, call it repeatedly, each time passing the GRID
 * you got the last time.  When you get back a zero, the list is done.
 * Restrictions: you must either call with zero or the last one you got.
 * You can't remember some previous value and use that to try to jump
 * to elsewhere on the list, or have multiple walks going on at once.
 * Caller should assume values are returned in arbitrary order.
 */

struct GRID *
nextgrid(grid_p)

struct GRID *grid_p;

{
	static int tbl_index = -1;
	static struct Sym *last_sym_p = 0;	/* remember where we were the
						 * last time we were called */

	if (grid_p == 0) {
		/* starting at beginning */
		tbl_index = -1;
		last_sym_p = 0;
	}
	else if (last_sym_p == 0 || grid_p != last_sym_p->val.grid_p) {
		pfatal("nextgrid called incorrectly");
	}

	/* if there is another on the current collision chain, use that */
	if (last_sym_p != 0) {
		last_sym_p = last_sym_p->next;
	}

	/* if none, either if just starting, or if ran off the end
	 * of a collision chain, find next chain. */
	if (last_sym_p == 0) {
		for (tbl_index++; tbl_index <  SYMTBLSIZE; tbl_index++) {
			if (Grid_table[tbl_index] != 0) {
				/* found a populated chain */
				last_sym_p = Grid_table[tbl_index];
				break;
			}
		}
	}

	return(last_sym_p == 0 ? 0 : last_sym_p->val.grid_p);
}


/* return a hash number from a string. XOR the bytes together */

static int
hash(string)

char *string;	/* hash this string */

{
	int h;	/* hash number */

	for (h = 0; *string != '\0'; string++) {
		h ^= *string;
	}
	/* return hash number between 0 and 127 */
	return(h & 0x7f);
}


/* add entry to COORD_INFO table */

void
add_coord(coordlist_p, coordtype)

float *coordlist_p;	/* address of set of 13 coordinates to add to tbl */
int coordtype;		/* CT_GRPSYL, etc */

{
	struct COORD_INFO *new_p;	/* space for saving coord info */
	int h;				/* hash number */


	/* if not already in table, add it */
	if (find_coord(coordlist_p) == (struct COORD_INFO *) 0) {

		/* get space, fill in coord type, and link into hash table */
		CALLOC(COORD_INFO, new_p, 1);

		new_p->coordlist_p = coordlist_p;
		new_p->flags = (short) coordtype;

		h = coordhash(coordlist_p);
		new_p->next = Coord_table[h];
		Coord_table[h] = new_p;
	}
}


/* Given an address of an array of floats (a coordinate array), return a hash
 * number, which is modulo of the Coord_table table size */

static int
coordhash(key)

float *key;	/* hash this number */

{
	return ((int) ( (unsigned long) key % COORDTBLSIZE));
}


/* Given a coordinate (pointer to array of floats), return the location of
 * the info about it in the Coord_table, or 0 if not in table */

struct COORD_INFO *
find_coord(key)

float *key;		/* look up this key in hash table */

{
	int h;			/* hash number */
	struct COORD_INFO *c_p;


	/* search hash table for matching entry */
	h = coordhash(key);
	for (c_p = Coord_table[h]; c_p != (struct COORD_INFO *) 0;
							c_p = c_p->next) {
		if (key == c_p->coordlist_p) {
			return(c_p);
		}
	}
	return( (struct COORD_INFO *) 0);
}


/* Given an existing INPCOORD, and a new INPCOORD that is to replace it,
 * adjust any tag references (hor_p or vert_p) to point to the new one. */
/* Note that we do not have to deal with any tag references inside
 * coordinate expressions here (i.e., any OP_TAG_REF nodes inside of
 * the expressions pointed to by hexpr_p and vexpr_p). That is
 * because the new INPCOORD will just point to the same parse trees as the
 * old--the parse trees are not copied, just the pointers to them.
 */

void
rep_inpcoord(old_inpcoord_p, new_inpcoord_p)

struct INPCOORD *old_inpcoord_p;
struct INPCOORD *new_inpcoord_p;

{
	rep_ref( &(old_inpcoord_p->hor_p), &(new_inpcoord_p->hor_p) );
	rep_ref( &(old_inpcoord_p->vert_p), &(new_inpcoord_p->vert_p) );
	
}

/* Given an existing reference to a tag in an INPCOORD, replace that
 * reference with the new reference. This is for when transferring a
 * temporary INPCOORD to a permanent one.
 */

static void
rep_ref(old_ref_p_p, new_ref_p_p)

float **old_ref_p_p;
float **new_ref_p_p;

{
	struct COORD_INFO *coordinfo_p;
	struct COORD_REF *ref_p;

	if (*old_ref_p_p == 0) {
		/* This can happen if user references uninitialized tag.
		 * We already give error message elsewhere for that. */
		return;
	}
	if (*new_ref_p_p == 0) {
		pfatal("attempt to replace tag reference with null.");
		return;
	}

	/* Find this information about the coordinate */
	if ((coordinfo_p = find_coord(*old_ref_p_p)) == 0) {
		/* Must have been earlier user error. */
		return;
	}

	/* Find any references matching the old and update them.
	 * Really should be only one, but seems safer to check all.
	 */
	for (ref_p = coordinfo_p->ref_list_p; ref_p != 0; ref_p = ref_p->next) {
		if (ref_p->ref_p_p == old_ref_p_p) {
			ref_p->ref_p_p = new_ref_p_p;
		}
	}
}


/* Given an existing coord array address and an address it is being moved to,
 * update all references to the old to point to the new, and update our
 * hash table to insert the old and delete the old.
 */

void
upd_ref(oldcoord_p, newcoord_p)

float * oldcoord_p;
float * newcoord_p;

{
	struct COORD_INFO *coordinfo_p;	/* existing info about oldcoord_p */
	struct COORD_INFO *newcoordinfo_p;	/* for info about newcoord_p */
	struct COORD_REF *ref_p;	/* for walking through ref list */

	if ((coordinfo_p = find_coord(oldcoord_p)) == 0) {
		/* apparently no tags associated with this coord array */
		return;
        }
	/* update all the references to the tag with the new value */
        for (ref_p = coordinfo_p->ref_list_p; ref_p != 0; ref_p = ref_p->next) {
                *(ref_p->ref_p_p) = newcoord_p;
        }

	/* Now need to create new entry for the new coord, and delete old */
	add_coord(newcoord_p, coordinfo_p->flags);
	/* Append reference list to new coord info */
	if ((newcoordinfo_p = find_coord(newcoord_p)) != 0) {
		/* Should always get here; the 'if' is to just paranoia
		 * to be sure to avoid null pointer deference. */
		/* If coord already existed before, because one coord is
		 * being combined with another, we want to concatenate the
		 * reference list, so find end to append to. */
		struct COORD_REF **append_p_p;
		for (append_p_p = &(newcoordinfo_p->ref_list_p);
					*append_p_p != 0;
					append_p_p = &((*append_p_p)->next)) {
			;
		}

		*append_p_p = coordinfo_p->ref_list_p;
		coordinfo_p->ref_list_p = 0;
	}
	delete_coord(oldcoord_p);
}


/* Delete the given coordinate information */

static void
delete_coord(coord_p)

float * coord_p;

{
	int h;			/* hash number */
	struct COORD_INFO **c_p_p;

	/* search hash table for matching entry and delete it */
	h = coordhash(coord_p);
	for (c_p_p = &(Coord_table[h]); *c_p_p != 0; *(c_p_p) = (*c_p_p)->next) {
		if ((*c_p_p)->coordlist_p == coord_p) {
			struct COORD_INFO * to_delete_p;
			to_delete_p = *c_p_p;
			*c_p_p = (*c_p_p)->next;
			FREE(to_delete_p);
			return;
		}
	}
}


/* Given a shape name and string containing the 4 to 6 note heads to use for it,
 * as would exist in a line of "headshapes" context, parse the
 * string with the noteheads and save all the information
 * in the shapes hash table.
 */

void
add_shape(name, shapes)

char *name;	/* name of the list of shapes */
char *shapes;	/* list of 4 to 6 shape names */

{
	struct Sym *sym_p;		/* where added into Shape_table */
	struct HDSHAPEINFO *shapeinfo_p;/* internal format for shape data */
	int i;			/* index through shapes */
	int d;			/* index through durations */
	int nameleng;		/* length of one name in shapes list */
	char namebuff[40];	/* one of the names in shapes list.
				 * Needs to be big enough to hold longer
				 * name of any valid note head character. */
	unsigned char ch;	/* music character corresponding to head name */
	int font;		/* which font FONT_MUSIC*  */
	int is_small;		/* not really needed here, but function we
				 * call expects it */
	short flips;		/* YES if stem down uses upside down version */

	debug(4, "add_shape name='%s' shapes='%s'", name, shapes);

	/* Add to symbol table */
	sym_p = add2tbl(name, Shape_table);
	MALLOC(HDSHAPEINFO, shapeinfo_p, 1);
	sym_p->val.shapeinfo_p = shapeinfo_p;

	/* Allocate an index and fill in the index-to-info mapping array */
	shapeinfo_p->index = ++Shape_entries;
	if (Shape_entries >= MAX_SHAPE_ENTRIES) {
		yyerror("Too many headshapes");
		return;
	}
	else {
		Shape_map[Shape_entries] = sym_p;
	}

	/* Parse the list of shapes and save their info */
	for (d = i = 0; shapes[i] != '\0';  ) {
		/* Skip white space */
		if (isspace(shapes[i])) {
			i++;
			continue;
		}

		/* Make sure user didn't give too many shapes */
		if (d >= MAX_SHAPE_DURS) {
			l_yyerror(Curr_filename, yylineno,
				"Too many shapes for headshape '%s' (max of %d expected)\n",
				name, MAX_SHAPE_DURS);
			return;
		}

		/* Check if stem down gets a flipped character */
		if (shapes[i] == 'u' && shapes[i+1] == '?') {
			flips = YES;
			i += 2;
		}
		else {
			flips = NO;
		}

		/* get copy of current head name, and look up its character */
		nameleng = strcspn(shapes + i, " \t\r\n");
		/* leave room for null and 'u' for upsidedown */
		if (nameleng > sizeof(namebuff) - 2) {
			ufatal("head shape name too long");
		}
		if (nameleng == 0 && flips == YES) {
			l_yyerror(Curr_filename, yylineno,
				"'u?' must be followed immediately by a note head character name");
			return;
		}
		strncpy(namebuff, shapes + i, nameleng);
		namebuff[nameleng] = '\0';
		/*** !!! check this, and deal with small. also check instance below ***/
		/* The "current" font is actually irrelevent here, so
		 * just pass the first one */
		font = FONT_TR;
		ch = find_char(namebuff, &font, &is_small, YES);
		if (is_valid_notehead(ch, font) == NO) {
			l_yyerror(Curr_filename, yylineno,
				"'%s' is not a valid note head name", namebuff);
			return;
		}
		if (strncmp(namebuff, "sm", 2) == 0) {
			l_warning(Curr_filename, yylineno,
				"the 'sm' prefix (indicating 'small') is being ignored");
		}
			
		shapeinfo_p->headchar[STEMINDEX(UP)][d] = ch;
		shapeinfo_p->headfont[STEMINDEX(UP)][d] = font;

		/* If flips, get upside down version. Else use same again */
		if (flips == YES) {
			namebuff[0] = 'u';
			strncpy(namebuff + 1, shapes + i, nameleng);
			namebuff[nameleng+1] = '\0';
			ch = find_char(namebuff, &font, &is_small, YES);
			if (is_valid_notehead(ch, font) == NO) {
				l_yyerror(Curr_filename, yylineno,
					"'%s' is not a valid note head name", namebuff);
				return;
			}
		}
		shapeinfo_p->headchar[STEMINDEX(DOWN)][d] = ch;
		shapeinfo_p->headfont[STEMINDEX(DOWN)][d] = font;

		/* Prepare for next in the list, if any */
		d++;
		i += nameleng;
	}
	if (d < MIN_SHAPE_DURS) {
		l_yyerror(Curr_filename, yylineno,
			"Too few shapes for headshape '%s' (at least %d expected, %d found)\n",
			name, MIN_SHAPE_DURS, d);
	}
	if (d < MAX_SHAPE_DURS) {
		ch = find_char("octwhole", &font, &is_small, YES);	
		shapeinfo_p->headchar[STEMINDEX(UP)][MAX_SHAPE_DURS-1] = ch;
		shapeinfo_p->headfont[STEMINDEX(UP)][MAX_SHAPE_DURS-1] = font;
		shapeinfo_p->headchar[STEMINDEX(DOWN)][MAX_SHAPE_DURS-1] = ch;
		shapeinfo_p->headfont[STEMINDEX(DOWN)][MAX_SHAPE_DURS-1] = font;
	}
	if (d < MAX_SHAPE_DURS - 1) {
		ch = find_char("quadwhole", &font, &is_small, YES);	
		shapeinfo_p->headchar[STEMINDEX(UP)][MAX_SHAPE_DURS-2] = ch;
		shapeinfo_p->headfont[STEMINDEX(UP)][MAX_SHAPE_DURS-2] = font;
		shapeinfo_p->headchar[STEMINDEX(DOWN)][MAX_SHAPE_DURS-2] = ch;
		shapeinfo_p->headfont[STEMINDEX(DOWN)][MAX_SHAPE_DURS-2] = font;
	}
}


/* Given a head shape index, stemdir, and basictime, return the notehead
 * character to use and (via font_p pointer)
 * which music font that notehead character is in.
 */

int
nheadchar(headshape, basictime, stemdir, font_p)

int headshape;		/* head shape index */
int basictime;		/* 8 for eighth, 2 for half, etc */
int stemdir;		/* UP or DOWN */
int *font_p;		/* FONT_MUSIC* is returned here */

{
	struct Sym *info_p;	/* shape to character map */
	int dir;		/* first index into headchar */
	int dur;		/* second index into headchar */

	if (headshape == HS_UNKNOWN || headshape > Shape_entries) {
		pfatal("illegal headshape index to nheadchar (%d)", headshape);
	}

	info_p = Shape_map[headshape];

	dir = STEMINDEX(stemdir);
	dur = HCDUR(basictime);
	*font_p = info_p->val.shapeinfo_p->headfont[dir][dur];
	return(info_p->val.shapeinfo_p->headchar[dir][dur]);
}


/* Given a head shape name, return the internal index number we use
 * to refer to that shape.
 */

int
get_shape_num(shapename)

char *shapename;

{
	struct Sym *sym_p;

	if ((sym_p = findSym(shapename, Shape_table)) != 0) {
		return(sym_p->val.shapeinfo_p->index);
	}
	return(HS_UNKNOWN);
}


/* Return YES if given character is a valid note head character, else NO */

static int
is_valid_notehead(ch, font)

int ch;		/* character code */
int font;	/* FONT_MUSIC*  */

{
	if ( IS_MUSIC_FONT(font) == NO || is_bad_char(ch) == YES) {
		/* If caller passes us the return from find_char(),
		 * that could return BAD_CHAR, so we don't pfatal here,
		 * but do return right away, so we don't do an illegal
		 * array index below.
		 */
		return(NO);
	}
	return (Nhead_map[FONTINDEX(font)][CHARINDEX(ch)] == 0 ? NO : YES);
}


/* Save note head information in table */
/* Note that this could be either a built-in head character, or a
 * user-defined symbol that they declared as being a head character. */

static void
add_head(name, info_p)

char *name;	/* music character name */
struct HEADINFO *info_p;	/* char/font/stem offset */

{
	struct Sym *sym_p;

	
	sym_p = add2tbl(name, Nhead_table);
	sym_p->val.noteinfo_p = info_p;

	/* Fill in reverse lookup map */
	Nhead_map[FONTINDEX(info_p->font)][CHARINDEX(info_p->ch)] = info_p;
}


/* Add a user-defined symbol to the list of valid note head characters. */

void
add_user_head(name, fontnumber, code, upstem_yoffset, downstem_yoffset)

char *name;		/* user's name, to be used as \(name) */
int fontnumber;		/* which "music" font the character is in */
int code;		/* which "ascii" code to associate with the symbol */
int upstem_yoffset;	/* these are in 1000-unit character coord space */
int downstem_yoffset;

{
	struct HEADINFO * info_p;


	/* Do error checking */
	if (is_bad_char(code) == YES) {
		pfatal("invalid code (%d) for user-defined note heads", code);
	}
	if (IS_MUSIC_FONT(fontnumber) == NO) {
		pfatal("invalid font (%d) for use as a note head", fontnumber);
	}

	/* Store info about the note head symbol */
	MALLOC(HEADINFO, info_p, 1);
	info_p->ch = code;
	info_p->font = fontnumber;
	/* Stem offsets are passed in 1000-unit character coordinate space.
	 * We convert to stepsizes, where 1 stepsize = 300 units. */
	info_p->ystem_off[STEMINDEX(UP)] = upstem_yoffset / 300.0;
	info_p->ystem_off[STEMINDEX(DOWN)] = downstem_yoffset / 300.0;

	/* Add the info to table of valid note heads */
	add_head(name, info_p);
}


/* Given a note head character and stem direction,
 * return the y offset for the stem to end, in stepsizes.
 */

double
stem_yoff(headch, font, stemdir)

int headch;	/* music character code */
int font;	/* FONT_MUSIC*   */
int stemdir;	/* UP or DOWN */

{
	struct HEADINFO *info_p;

	headch &= 0xff;
	if ( IS_MUSIC_FONT(font) == NO || is_bad_char(headch) == YES) {
		pfatal("invalid argument: stem_yoffset(%d, %d, stemdir)",
							headch, font, stemdir);
	}
	info_p = Nhead_map[FONTINDEX(font)][CHARINDEX(headch)];
	if (info_p == 0) {
		pfatal("No notehead map for ch=%d, font=%d", headch, font);
	}
	if (stemdir == UNKNOWN) {
		pfatal("stem_yoff called with unknown stemdir");
	}
	return(info_p->ystem_off[STEMINDEX(stemdir)]);
}


/* This should be called when an SSV has been collected by the user.
 * If the user set beamstyle and/or timeunit in the SSV,
 * create a mapping between the current time signature and that beamstyle
 * and/or timeunit, so that if the user later sets the same time signature,
 * they don't have to set the other things too.
 * If they set time signature, see if we have a mapping
 * for that time signature. If so, set the beamstyles and timeunits
 * from that mapping.
 */

void
remember_tsig_params(mll_p)

struct MAINLL *mll_p;	/* contains SSV */

{
	struct SSV *ssv_p;	/* the SSV to process */
	char *timesig;		/* current time signature representation */
	struct Sym *entry;	/* entry in time sig info mapping table */


	if (mll_p->str != S_SSV) {
		pfatal("remember_tsig_params got bad str value %d", mll_p->str);
	}
	ssv_p = mll_p->u.ssv_p;

	if (ssv_p->used[TIME] == NO && ssv_p->used[BEAMSTLIST] == NO &&
					ssv_p->used[TIMEUNIT] == NO) {
		/* nothing of interest in this SSV */
		return;
	}

	/* If user set time signature in this SSV, that's the time sig
	 * of interest, otherwise use the current time signature */
	timesig = (ssv_p->used[TIME] == YES ? ssv_p->timerep : Score.timerep);
	if ((entry = findSym(timesig, Time_map)) == 0) {
		entry = add2tbl(timesig, Time_map);
		/* We'll only allocate the actual table if user gives a
		 * beamstyle or timeunit somewhere. If they never do,
		 * this will avoid wasting memory.
		 */
		entry->val.ssvtables_p = 0;
	}

	/* If beamstyle or timeunit are set in this SSV, associate them
	 * with the current time signature. */
	if (ssv_p->used[BEAMSTLIST] == YES) {
		if (entry->val.ssvtables_p == 0) {
			CALLOC(SSVTABLES, entry->val.ssvtables_p, 1);
		}
		/* Note that when staffno and voiceno are zero,
		 * that's actually a score entry, and when voiceno is zero,
		 * but staffno is non-zero, that is actually a staff entry. */
		entry->val.ssvtables_p->beamstyle_table[ssv_p->staffno][ssv_p->voiceno] = ssv_p;
	}
	if (ssv_p->used[TIMEUNIT] == YES) {
		if (entry->val.ssvtables_p == 0) {
			CALLOC(SSVTABLES, entry->val.ssvtables_p, 1);
		}
		entry->val.ssvtables_p->timeunit_table[ssv_p->staffno][ssv_p->voiceno] = ssv_p;
	}

	/* If time signature is set in this SSV, see if we have any
	 * beamstyles or timeunits associated with that time signature.
	 * If so, restore their values. */
	if (ssv_p->used[TIME] == YES && entry->val.ssvtables_p != 0) {
		/* Make new SSVs and copy the relevant fields for any
		 * remembered beamstyles and/or timesunits associated 
		 * with this time signature. */
		struct SSV *beamstyle_ssv_p;	/* SSV having beamstyle info */
		struct SSV *timeunit_ssv_p;	/* SSV having timeunit info */
		struct MAINLL *mll_ssv_p;	/* new SSV to add to list */
		struct SSV *nssv_p;		/* new SSV to add to list */
		int s;				/* staff */
		int v;				/* voice */

		/* Check all SSV contexts. The [0][0] entry is for Scoreo			 * The rest of row [0] is unused.
		 * The [0] column is for staffs, for s > 0.
		 */
		for (s = 0; s <= MAXSTAFFS; s++) {
			for (v = 0; v <= MAXVOICES; v++) {

				/* Check if we need to create an SSV for
				 * this staff/voice */
				beamstyle_ssv_p = entry->val.ssvtables_p->beamstyle_table[s][v];
				timeunit_ssv_p = entry->val.ssvtables_p->timeunit_table[s][v];
				if (beamstyle_ssv_p == 0 &&
						timeunit_ssv_p == 0) {
					/* nothing to do for this one */
					continue;
				}

				/* If both saved SSVs are either zero or
				 * the same SSV as where the time was just
				 * set, no need to make another SSV */
				if ( (beamstyle_ssv_p == 0 ||
						beamstyle_ssv_p == ssv_p) &&
						(timeunit_ssv_p == 0 ||
						timeunit_ssv_p == ssv_p) ) {
					continue;
				}

				/* need to create an SSV */
				mll_ssv_p = newMAINLLstruct(S_SSV, -1);
				insertMAINLL(mll_ssv_p, mll_p);
				mll_p = mll_ssv_p;

				/* populate the new SSV */
				nssv_p = mll_ssv_p->u.ssv_p;
				if (beamstyle_ssv_p != 0) {
					nssv_p->nbeam = beamstyle_ssv_p->nbeam;
					nssv_p->beamstlist = beamstyle_ssv_p->beamstlist;
					nssv_p->beamrests = beamstyle_ssv_p->beamrests;
					nssv_p->beamspaces = beamstyle_ssv_p->beamspaces;
					nssv_p->nsubbeam = beamstyle_ssv_p->nsubbeam;
					nssv_p->subbeamstlist = beamstyle_ssv_p->subbeamstlist;
					nssv_p->used[BEAMSTLIST] = YES;
				}
				if (timeunit_ssv_p != 0) {
					nssv_p->timeunit = timeunit_ssv_p->timeunit;
					nssv_p->used[TIMEUNIT] = YES;
				}

				/* fill in the SSV header */
				if (v != 0) {
					nssv_p->context = C_VOICE;
				}
				else if (s != 0) {
					/* The [s][0] entry is for staff s
					 * when s > 0 */
					nssv_p->context = C_STAFF;
				}
				else {
					/* The [0][0] entry is actually score */
					nssv_p->context = C_SCORE;
				}
				nssv_p->staffno = s;
				nssv_p->voiceno = v;
				asgnssv(nssv_p);
			}
		}
	}
}


/* Save the current table that maps time signatures to beamstyles. 
 * This returns the index of the saved table, or -1 if the table was empty,
 * so we didn't need to save anything. */

static int
save_time2beamstyle()

{
	int i;
	int index;

	/* If there are no entries, no need to actually save the table */
	for (i = 0; i < SYMTBLSIZE; i++) {
		if (Time_map[i] != 0) {
			/* There is an entry; we will have to save */
			break;
		}
	}
	if (i == SYMTBLSIZE) {
		return(-1);
	}

	/* Okay. We do need to save a copy. Make array of saved table one larger. */
	Num_time_maps++;
	if (Num_time_maps == 1) {
		MALLOCA(struct Sym **, Saved_time_maps, 1);
	}
	else {
		REALLOCA(struct Sym **, Saved_time_maps, Num_time_maps);
	}

	/* Allocate the table to save to */
	index = Num_time_maps  - 1;
	MALLOCA(struct Sym *, Saved_time_maps[index], SYMTBLSIZE);

	/* Save a copy */
	for (i = 0; i < SYMTBLSIZE; i++) {
		Saved_time_maps[index][i] = Time_map[i];
	}
	return(index);
}


/* Restore the mapping between time signatures and beamstyles as it had
 * been at the time of a saveparms.
 */

void
restore_time2beamstyle(index)

int index;	/* which saved instance to restore */

{
	int i;


	for (i = 0; i < SYMTBLSIZE; i++) {
		if (index == -1) {
			/* Table was empty at save time, so we didn't really
			 * save, we can just set each entry to empty */
			Time_map[i] = 0;
		}
		else {
			Time_map[i] = Saved_time_maps[index][i];
		}
	}
}


/* If the c[] given as argument is for a builtin tag (_page, _win, or _cur),
 * return true, otherwise return false */

int
is_builtin_tag(coord)

float *coord;

{
	struct COORD_INFO *coord_info_p;

	/* _win has to be handled specially, since it depends on context,
	 * and cannot be found via find_coord() */
	if (Blockcoord_p_p != 0 && coord == *Blockcoord_p_p) {
		return(YES);
	}
	if (coord == _Win) {
		return(YES);
	}

	if ((coord_info_p = find_coord(coord)) == 0) {
		pfatal("is_builtin_tag unable to find tag %p", coord);
	}
	return (coord_info_p->flags & CT_BUILTIN);
}


/* Add the given savemacros name to the table of such names,
 * or update the index value if already there.
 */

void
add_savemacs(name, index)

char *name;	/* user's name to save to */
int index;	/* index that we map that name to */

{
	struct Sym *sym_p;	/* the entry */

	sym_p = add2tbl(name, Saved_macs_table);
	sym_p->val.index = index;
}


/* Look up the given name in the table of savemacros names and
 * return the index associated with that name, or -1 if no such name.
 */

int
find_savemacs(name)

char *name;

{
	struct Sym *sym_p;

	if ((sym_p = findSym(name, Saved_macs_table)) == 0) {
		return(-1);
	}
	else {
		return(sym_p->val.index);
	}
}


/* Add entry to the table of saved parameter names. We associate the name
 * with what is currently the last main list entry, and also save away various
 * other state information we need, regarding mapping of time signatures to
 * beamstyles, and about alternating time signatures. */

void
add_saveparms(name)

char *name;		/* user's saveparms string */

{
	struct Sym *sym_p;	/* the entry */

	sym_p = add2tbl(name, Saved_parms_table);
	MALLOC(SAVEPARMS_INFO, sym_p->val.saveparms_info_p, 1);
	sym_p->val.saveparms_info_p->mll_p = Mainlltc_p;
	sym_p->val.saveparms_info_p->time2beamstyle_index = save_time2beamstyle();
	sym_p->val.saveparms_info_p->saved_alt_timesig_list = Alt_timesig_list;
	sym_p->val.saveparms_info_p->saved_next_alt_timesig = Next_alt_timesig;
	sym_p->val.saveparms_info_p->saved_tsig_visibility = Tsig_visibility;
}


/* Look up the given name in the table of saveparms names and
 * do a restoreparms. Returns YES if successful, NO if there had not
 * been a saveparms to that name (after printing error message).
 */

int
do_restoreparms(name)

char *name;	/* the name of the saveparms that we should restore from */

{
	struct Sym *sym_p;

	if ((sym_p = findSym(name, Saved_parms_table)) == 0) {
		l_yyerror(Curr_filename, yylineno,
			"no saveparms \"%s\" was done, so cannot restoreparms from it", name);
		return(NO);
	}
	(void) restoreparms(sym_p->val.saveparms_info_p->mll_p, Mainlltc_p);
	restore_time2beamstyle(sym_p->val.saveparms_info_p->time2beamstyle_index);
	Alt_timesig_list = sym_p->val.saveparms_info_p->saved_alt_timesig_list;
	Next_alt_timesig = sym_p->val.saveparms_info_p->saved_next_alt_timesig;
	Tsig_visibility = sym_p->val.saveparms_info_p->saved_tsig_visibility;
	/* Note that if the saved_tsig_visiblity is PTS_NEVER, we don't know
	 * if there had actually been a PTS_ONCE at the beginning.
	 * If they do a restoreparms in the middle of a set
	 * of alternating time signatures, and they had not originally added
	 * a "y" or "n" flag, we really ought to print the time signature
	 * again, but won't. E.g., if they were doing 4/4 3/4
	 * and had just finished a 4/4 measure when doing a restore,
	 * there will be two 4/4 measures in a row, so we should print the
	 * 4/4 3/4 time signature again to make it clear the pattern
	 * is restarting from the beginning.
	 * But that would be hard enough to do that it just
	 * doesn't seem worth the trouble. If someone really wants that,
	 * the workaround is to add a nonprinting different time signature
	 * followed by a measure space and invis bar, to force the printing.
	 */ 

	return(YES);
}
