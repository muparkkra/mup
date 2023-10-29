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

/*
 * This file contains functions for shape maps. These allow users to
 * override various music symbol shapes on a per-staff or even per-voice
 * basis. This might be useful, for example, if one wanted a different-looking
 * C clef on some staffs than others, or wants to provide their own rest
 * symbols.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"

/* By convention, we pass font/code to functions as ints, even though they are
 * really only a byte (unsigned char), just to avoid any possible issues with
 * casting and argument passing. But in this file, we compress them into
 * a single unsigned short to save space. These macros are for encoding and
 * decoding that compression.
 */
#define CHAR_VALUE(font, code)   ( ((code & 0xff) << 8) | (font & 0xff) )
/* Extract font/code from compressed version */
#define FONT_VALUE(value)	(value & 0xff)
#define CODE_VALUE(value)	((value >> 8) & 0xff)

/* This is the list of shapes contexts the user has defined. When defining,
 * the one being filled in is at the head of the list. */
struct SHAPE_MAP *Shape_map_list;

/* How many characters there are which can be overridden */
static int Max_shape_overrides;


/* For fast lookup, we have a table that says, for a given character code,
 * whether it can be overridden in shapes context.
 * For each music font, there is one bit
 * that says if it can be overridden at all, and then one bit to say if it
 * can be overridden in voice context. This could support up to 4 music fonts
 * in a table of bytes. We initialize the fast lookup table
 * at runtime from hard-coded lists.
 */
#define OVERRIDE_BIT(mfont)   (1 << (2*(mfont)-2))
#define VV_BIT(mfont)		(1 << (2*(mfont)-1))
static unsigned char Can_override[MAX_CHARS_IN_FONT];
/* characters in MFONT that can be overridden via svpath */
static unsigned char Mfont_SV_overrides[] = {
	C_FCLEF,
	C_GCLEF,
	C_CCLEF,
	C_COM,
	C_CUT,
	C_MEASRPT,
	C_SHARP,
	C_FLAT,
	C_NAT,
	C_DBLSHARP,
	C_DBLFLAT,
	0
};
/* characters in MFONT that can be overridden via vvpath */
static unsigned char Mfont_VV_overrides[] = {
	C_UPFLAG,
	C_DNFLAG,
	C_OWHREST,
	C_QWHREST,
	C_DWHREST,
	C_1REST,
	C_2REST,
	C_4REST,
	C_8REST,
	C_16REST,
	C_32REST,
	C_64REST,
	C_128REST,
	C_256REST,
	C_LL1REST,
	C_LL2REST,
	0
};
/* characters in MFONT2 that can be overridden via svpath */
static unsigned char Mfont2_SV_overrides[] = {
	C_DBLMEASRPT,
	C_QUADMEASRPT,
	0
};
/* characters in MFONT2 that can be overridden via vvpath */
static unsigned char Mfont2_VV_overrides[] = {
	0
};

unsigned char *SV_overrides[NUM_MFONTS] = {
	Mfont_SV_overrides,
	Mfont2_SV_overrides
};
unsigned char *VV_overrides[NUM_MFONTS] = {
	Mfont_VV_overrides,
	Mfont2_VV_overrides
};

/* This function initializes the fast lookup table of which characters
 * are legal to override, from hard-coded lists */

static void
init_can_override()

{
	int f;
	int i;
	int index;

	for (f = 0; f < NUM_MFONTS; f++) {
		for (i = 0; SV_overrides[f][i] != 0; i++) {
			index = SV_overrides[f][i] - FIRST_CHAR;
			if ((index < 0) || (index >= MAX_CHARS_IN_FONT)) {
				pfatal("out of range index in SV_overrides");
			}
			Can_override[index] |= OVERRIDE_BIT(f+1);
			Max_shape_overrides++;
		}
		for (i = 0; VV_overrides[f][i] != 0; i++) {
			index = VV_overrides[f][i] - FIRST_CHAR;
			if ((index < 0) || (index >= MAX_CHARS_IN_FONT)) {
				pfatal("out of range index in VV_overrides");
			}
			Can_override[index] |= (OVERRIDE_BIT(f+1) | VV_BIT(f+1));
			Max_shape_overrides++;
		}
	}
}


/* Given a music font and code within that,
 * if that character can be overridden,
 * return the shape map that would contain such a override, if it exists.
 * This looked up the shapes parameter value using either svpath or vvpath
 * as appropriate for the particular character. If there is no such map,
 * 0 is returned. Note that a non-zero result only means that a map exists;
 * the particular font/code might or might not be overridden in that map.
 */
 
static struct SHAPE_MAP *
get_override_map(staffno, vno, font, code)

int staffno;
int vno;
int font;
int code;

{
	int index;
	int font_index;

	index = code - FIRST_CHAR;
	if ( (index < 0) || (index >= MAX_CHARS_IN_FONT) ) {
		pfatal("out of range index in get_override_map()");
	}

	font_index = font - FONT_MUSIC + 1;
	if (Can_override[index] & OVERRIDE_BIT(font_index)) {
		if (Can_override[index] & VV_BIT(font_index)) {
			return(vvpath(staffno, vno, SHAPES)->shapes);
		}
		else {
			return(svpath(staffno, SHAPES)->shapes);
		}
	}
	return(0);
}


/* This comparison function is used by qsort and bsearch, for quickly
 * determining if a particular characters is being overridden. */

static int
shape_compare(item1, item2)

const void *item1;
const void *item2;

{
	int key;
	int entry;

	key = ((struct SHAPE_ENTRY *)item1)->from;
	entry = ((struct SHAPE_ENTRY *)item2)->from;
	if ( key < entry) {
		return(-1);
	}
	else if (key > entry) {
		return(1);
	}
	else {
		return(0);
	}
}


/* Given a "normal" music font and code in that font for some character,
 * like a clef, return, via pointers,
 * the music font and code to use for the specified staff/voice.
 * Returns YES if there was an override, NO if not.
 * If there was no override, the values returned via the pointers will
 * be the same as were passed in, so if the caller doesn't actually care if
 * there was an override or not, just wants the values, they can ignore the
 * function return value.
 */
 
int
get_shape_override(staffno, vno, font_p, code_p)

int staffno;
int vno;
int *font_p;
int *code_p;

{
	struct SHAPE_MAP *shape_map;

	if ( (Shape_map_list != 0) &&
			(shape_map = get_override_map(staffno, vno,
			*font_p, *code_p)) != 0) {
		struct SHAPE_ENTRY key;
		struct SHAPE_ENTRY *replacement_p;

		key.from = CHAR_VALUE(*font_p, *code_p);
		if ((replacement_p = (struct SHAPE_ENTRY *) bsearch(&key,
				shape_map->map, shape_map->num_entries,
				sizeof(shape_map->map[0]), shape_compare)) != 0) {
			*font_p = FONT_VALUE(replacement_p->to);
			*code_p = CODE_VALUE(replacement_p->to);
			return(YES);
		}
	}

	/* No override; return unchanged */
	return(NO);
}


/* Start defining a new shape map. Allocates a struct for it and adds it
 * to the list of known maps. Note that if one by that name already exists,
 * we silently make a new one one, which since it will be nearer the head of
 * the list, will now be found first. */

void
init_new_shape_map(name)

char *name;	/* name the user gave to this shapes context */

{
	struct SHAPE_MAP *new_map_p;

	/* First time called, initialize the override fast lookup table */
	if (Shape_map_list == 0) {
		init_can_override();
	}

	MALLOC(SHAPE_MAP, new_map_p, 1);
	new_map_p->name = strdup(name);
	new_map_p->num_entries = 0;
	/* Allocate enough space to override everything we allow.
	 * If user ovverides less, we will shed the excess later */
	MALLOC(SHAPE_ENTRY, new_map_p->map, Max_shape_overrides);
	/* Link to head of list */
	new_map_p->next = Shape_map_list;
	Shape_map_list = new_map_p;
}


/* Add the supplied mapping into the shape map currently being defined. */

void
add_shape_map_entry(from_sym_name, to_sym_name)

char *from_sym_name;
char *to_sym_name;

{
	int from_font;
	int from_code;
	int to_font;
	int to_code;
	int index;
	int char_index;
	int font_index;
	int from_value;
	int to_value;
	int is_small;
	int i;

	/* Do error checks */
	if (Shape_map_list == 0) {
		pfatal("add_shape_map() called with null Shape_map_list");
	}

	/* Translate the 'from' symbol name to font/code */
	from_font = FONT_MUSIC;
	is_small = NO;
	from_code = find_char(from_sym_name, &from_font, &is_small, YES);
	if (is_bad_char(from_code)) {
		/* find_char should already have printed an error message */
		return;
	}
	if (is_small == YES) {
		l_yyerror(Curr_filename, yylineno, "from symbol in shape map cannot be a 'small' symbol");
		return;
	}

	/* Translate the 'to' symbol name to font/code */
	to_font = FONT_MUSIC;
	is_small = NO;
	to_code = find_char(to_sym_name, &to_font, &is_small, YES);
	if (is_bad_char(to_code)) {
		return;
	}
	if (is_small == YES) {
		l_yyerror(Curr_filename, yylineno, "to symbol in shape map cannot be a 'small' symbol");
		return;
	}

	/* Compress the font/code */
	from_value = CHAR_VALUE(from_font, from_code);
	to_value = CHAR_VALUE(to_font, to_code);

	/* If already in the table, overwrite the old entry.
	 * This is a linear search, but the list can never be longer
	 * than the number of characters that can be overridden,
	 * and will often be only a few, and this is a one-time search.
	 */
	for (i = 0; i < Shape_map_list->num_entries; i++) {
		if (Shape_map_list->map[i].from == from_value ) {
			l_warning(Curr_filename, yylineno, "%s shape already overridden; using last",
					from_sym_name);
			Shape_map_list->map[i].to = to_value;
			return;
		}
	}

	char_index = from_code - FIRST_CHAR;
	font_index = from_font - FONT_MUSIC + 1;
	if ( ! (Can_override[char_index] & OVERRIDE_BIT(font_index)) ) {
		l_yyerror(Curr_filename, yylineno, "Cannot override %s symbol in shapes context",
				from_sym_name);
		return;
	}

	index = Shape_map_list->num_entries;
	if (index >= Max_shape_overrides) {
		pfatal("add_shape_map() table overflow");
	}

	Shape_map_list->map[index].from = from_value;
	Shape_map_list->map[index].to = to_value;
	Shape_map_list->num_entries++;
}


/* This function is to be called at the end of defining a shape map.
 * It sorts the map for fast lookup, and frees any excess space that was
 * allocated. (To keep thing simple, we allocate assuming worse case
 * of user overriding every symbol they can, but they often won't.)
 */

void
finish_shape_map()

{
	/* Sort the list, so we can binary search it */
	qsort(Shape_map_list->map, Shape_map_list->num_entries,
			sizeof(struct SHAPE_ENTRY), shape_compare);
	/* Shed any excess space in the map */
	REALLOC(SHAPE_ENTRY, Shape_map_list->map, Shape_map_list->num_entries);
}


static struct SHAPE_MAP *
create_mensural_shape_map()

{
	init_new_shape_map("mensural");
	add_shape_map_entry("dwhrest", "mensurdwhrest");
	add_shape_map_entry("1rest", "mensur1rest");
	add_shape_map_entry("ll1rest", "mensurll1rest");
	add_shape_map_entry("2rest", "mensur2rest");
	add_shape_map_entry("ll2rest", "mensurll2rest");
	add_shape_map_entry("4rest", "mensur4rest");
	add_shape_map_entry("8rest", "mensur8rest");
	add_shape_map_entry("16rest", "mensur16rest");
	add_shape_map_entry("upflag", "mensurupflag");
	add_shape_map_entry("dnflag", "mensurdnflag");
	finish_shape_map();
	return(Shape_map_list);
}


/* Look up and return the shape map with the given name, or 0 if no map
 * by that name has been defined. */

struct SHAPE_MAP *
get_shape_map(name)

char *name;

{
	struct SHAPE_MAP *smap_p;

	/* Linear search, but should be rare to have more than a few */
	for (smap_p = Shape_map_list; smap_p != 0; smap_p = smap_p->next) {
		if (strcmp(name, smap_p->name) == 0) {
			return(smap_p);
		}
	}

	/* Create mensural shapes context if user has asked for that. */
	if (strcmp(name, "mensural") == 0) {
		return(create_mensural_shape_map());
	}

	return(0);
}
