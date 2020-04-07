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

/* This file contains code for keymaps. A keymap lets the user specify
 * that a particular character or sequence of characters in input strings
 * be mapped to something else. The main intended use is to make it easier
 * to input text in some language that uses an alphabet other than Roman,
 * like Cyrillic or Greek. But it could potentially be used as sort of
 * a macro inside a string, so you could define a short string that gets
 * expanded to a longer string, to save typing.
 *
 * The user can define multiple maps, by putting multiple keymap contexts
 * in the input. Each has a user-defined name. The user can then say which
 * map to use for various things via a set of keymap parameters.
 * If a specific keymap parameter is not set, then the defaultkeymap is used.
 * So, for example,  the user could set defaultkeymap to a map for Cyrillic,
 * but then set textkeymap="" to not do any mapping for things like dynamics,
 * which they may still want to be in Italian.
 *
 * Internally, if the user ever defines a keymap, we set a flag to note
 * that fact, and we allocate a table for keymaps.
 * Otherwise they use almost no space.
 * Each entry in that table then contains an inner mapping table.
 * Each entry in that inner table maps an input string to an output string.
 * Then between parsing and placement, we go through and map strings.
 *
 * This is complicated by the fact that the patterns and
 * replacements need to be unaffected by font. E.g., if "a" is to be mapped
 * to "A" that is true whether the "a" happens to be in Times Roman or
 * Helvetica Bold or whatever. But since we internally also use "fonts"
 * to handle non-ASCII characters, we do need to distinguish
 * that kind of font. So while the character code
 * for something in one of the extended fonts or the music
 * font might be the same as the code for "a" in the base ASCII fonts,
 * a mapping of an "a" would not affect that. To deal with this, the patterns,
 * replacements, and target strings to be matched against patterns are put
 * into a normalized internal format, which consists of a STR_FONT followed
 * by a "character set" number that masked out the irrelevances in the font,
 * and then the character codes. So a pattern or replacement of "a" would be
 *   STR_FONT FONT_TR a
 * but would match an "a" string in all the base fonts FONT_TR through
 * FONT_PX. 
 *
 * Typically we expect both patterns and  replacements
 * to be one character long each, but allow either or both
 * to be of arbitrary length greater than zero.
 * Patterns and replacements are not allowed to have any STR_* things 
 * in them beyond the implicit STR_FONTs used for accessing
 * different character sets. When matching strings,
 * any explicit STR_* things will end the segment
 * that can potentially be mapped.
 */

#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"


/* We allocate space for entries in chunks. If a chunk fills up,
 * we reallocate with MAP_CHUNK more entries. We expect the most common
 * usage of keymaps will be for a non-ASCII alphabet. For a Western
 * European language, this could mean just a handfull to a few dozen
 * things added to ASCII. The most likely cases are something like
 * Greek or Cyrillic, which with upper and lower case would
 * likely be on the order of 48-72. So using 80, there should be a good
 * chance of never needing to realloc bigger after the first chunk,
 * but also not wasting too much.
 */
#define MAP_CHUNK	80

/* We have three normalized string types: patterns to be matched,
 * replacements strings for when those patterns are matched,
 * and strings that are candidate to be matched by some pattern. */
#define NST_PATTERN		1
#define NST_REPLACEMENT		2
#define	NST_MATCH_CANDIDATE	3

/* We sort of "hash" on the code of the first character of a normalized
 * string to get an index into a table that tells us where patterns
 * starting with that start in the map */
#define FIRST_CODE_INDEX(nstring) (nstring[2] - FIRST_CHAR)

/* Define how many keymaps there can be.
 * Since entry 0 is used for "no mapping," we add one
 * to the table size for that when allocating it.
 * And since we can't use zero as an argument to STR_KEYMAP,
 * we need to reserve an extra to avoid using zero for a handle.
 * This total (MAX_KEYMPAPS+2) needs to be less than MAX_CHARS_IN_FONT
 * so we can use the index in strings,
 * without having it mistaken for a STR_* thing.
 * We allow for 100, which seems like way more than enough,
 * as more than 2 or 3 should be very rare. */
#define MAX_KEYMAPS 100

/* This is the table that holds information about keymaps.
 * We malloc it the first time the user defines a keymap.
 * If they never do (the most common case), it will take no space,
 * except for this pointer.  */
static struct KEYMAP *Maplist_p;

/* How many entries in Maplist_p array are actually used */
static int Num_keymaps = 0;

/* This keeps track of which keymap is currently being built,
 * so that the yacc code doesn't have to constantly pass it back to us. */
static int Curr_keymap;

/* When chopping up a string into substrings that might need to be mapped,
 * no need to make a candidate substring any longer than the longest pattern,
 * so this keeps track of how long that is. */
static int Longest_pattern = 0;

/* We want to use a 1-byte "handle" as argument to STR_KEYMAP, but it can't
 * be a zero, because that would be a string terminator. Since zero is a
 * valid index, we adjust for that. Note that the parentheses around the h
 * and i in the body are critical to make sure precedences are correct! */
#define HANDLE2INDEX(h)		((h)-1)
#define INDEX2HANDLE(i)		((i)+1)

/* declarations of static functions */
static void init_keymaps P((void));
static int charset P((int font));
#ifdef __STDC__
static int comp_keymap P((const void *item1_p, const void *item2_p));
#else
static int comp_keymap P((char *item1_p, char *item2_p));
#endif
static char *normalize P((char *str, int currfont, int nstrtype));
static char *unnormalize P((char *str, int currfont));
static int match P((char *candidate, int cand_len, char *pattern, int pat_len));
static char *find_match P((char *candidate, struct KEYMAP *map_p, int *used_p));
static char *map_string P((char *str, struct KEYMAP *map_p, char *fname, int linenum));
static struct KEYMAP *kpvp_map P((int staffno, int param));
static void map_bar P((struct BAR *bar_p, char *fname, int linenum));
static void prepare_maps_for_searching P((void));


/* Create a new keymap and make it the current one.  This is called from
 * the yacc parsing code when user starts a keymap context. */

void
new_keymap(name)

char *name;	/* Map name provided by user */

{
	int m;	/* Index through list of defined maps */


	debug(2, "in new_keymap for %s", name);

	/* First time through, this will set up Maplist_p */
	init_keymaps();

	/* Check for redefining existing map */
	for (m = 0; m < Num_keymaps; m++) {
		if (Maplist_p[m].name != 0 && strcmp(name, Maplist_p[m].name) == 0) {
			int e;		/* index through entries */

			/* We let last one override */
			l_warning(Curr_filename, yylineno,
				"Map name %s being redefined; using last", name);

			/* Free the existing map */
			for (e = 0; e < Maplist_p[m].entries; e++) {
				FREE(Maplist_p[m].map_p[e].pattern);
				FREE(Maplist_p[m].map_p[e].replacement);
			}
			FREE(Maplist_p[m].map_p);
			Maplist_p[m].map_p = 0;
			Maplist_p[m].entries = 0;
			/* Note that the code_index array is not used
			 * until mapping time, so we can ignore it here. */
			break;
		}
	}
	if (m >= MAX_KEYMAPS + 1) {
		l_ufatal(Curr_filename, yylineno,
				"Too many keymaps (%d maximum)", MAX_KEYMAPS);
	}
	if (m >= Num_keymaps) {
		/* Not redefining an existing, so have one more */
		Num_keymaps++;
	}

	/* Save the user's name for this map */
	Maplist_p[m].name = name;

	/* NOTE: the space for the map itself will be allocated when
	 * the first entry is defined, in add_to_keymap(). */

	/* Remember which we are working on, so yacc doesn't have to
	 * keep passing it back to us. */
	Curr_keymap = m;

	Maplist_p[m].entries = 0;
	Maplist_p[m].is_sorted = NO;
}


/* Add a
 *	pattern = replacement
 * pair to the current key map.
 * This is called from yacc parsing code for each keymap entry.
 */

void
add_to_keymap(pattern, replacement)

char * pattern;			/* what to map */
char *replacement;		/* what to replace the pattern with */

{
	struct KEYMAP_ENTRY *entry_p;		/* the new entry */
	int m;					/* index through map list */
	int p;					/* index through the pattern */

	/* Make sure the pattern contains only allowed characters.
	 * If not, provide error message, and don't process
	 * any further. There are characters that somethings have special
	 * meanings, like dashes and underscores in lyrics cause syllalbe
	 * break, so if we allowed using those in keymaps, it could
	 * at least be confusing for the user, and maybe cause undesired
	 * behavior, so we just avoid all that by limited the character set.
	 */
	for (p = 2; pattern[p] != '\0'; p++) {
		if ( ! isalpha(pattern[p]) && pattern[p] != '=') {
			yyerror("keymap pattern can only contain letters or =");
			return;
		}
	}

	/* If no space for this entry, allocate a chunk for entries */
	if (Maplist_p[Curr_keymap].entries == 0) {
		MALLOC(KEYMAP_ENTRY, Maplist_p[Curr_keymap].map_p, MAP_CHUNK);
	}
	else if (Maplist_p[Curr_keymap].entries % MAP_CHUNK == 0) {
		REALLOC(KEYMAP_ENTRY, Maplist_p[Curr_keymap].map_p, Maplist_p[Curr_keymap].entries + MAP_CHUNK);
	}

	/* Get shorter name to the entry we will fill in */
	entry_p = &(Maplist_p[Curr_keymap].map_p[ Maplist_p[Curr_keymap].entries ]);

	/* Get normalized version of pattern string */
	pattern = fix_pat_rep_string(pattern, pattern[0], pattern[1],
				Curr_filename, yylineno, "keymap pattern");
	entry_p->pattern = normalize(pattern + 2,  pattern[0], NST_PATTERN);
	entry_p->pat_len = strlen(entry_p->pattern);

	/* Convert and save the replacement */
	replacement = fix_pat_rep_string(replacement,
				replacement[0], replacement[1],
				Curr_filename, yylineno, "keymap replacement");
	entry_p->replacement = normalize(replacement + 2, replacement[0],
					NST_REPLACEMENT);
	entry_p->rep_len = strlen(entry_p->replacement);

	/* If this pattern is longer than any we'd seen before,
	 * make a note of that. Knowing the longest lets us optimize
	 * how much to attempt to match. */
	if (entry_p->pat_len > Longest_pattern) {
		Longest_pattern = entry_p->pat_len;
	}

	/* Check for duplicate of something we already had. Might be nice
	 * to check earlier, but seemed easier to check here, and not worth
	 * optimizing for something that should be rare. */
	for (m = 0; m < Maplist_p[Curr_keymap].entries; m++) {
		if (Maplist_p[Curr_keymap].map_p[m].pat_len == entry_p->pat_len
				&& strncmp(Maplist_p[Curr_keymap].map_p[m].pattern,
				entry_p->pattern, entry_p->pat_len) == 0) {
			l_yyerror(Curr_filename, yylineno,
				"redefinition of keymap named %s; using last", pattern + 2);

			/* Don't need the duplicate pattern */
			FREE(entry_p->pattern);

			/* Replace the replacement string and its length */
			FREE(Maplist_p[Curr_keymap].map_p[m].replacement);
			Maplist_p[Curr_keymap].map_p[m].replacement
							= replacement;
			Maplist_p[Curr_keymap].map_p[m].rep_len
							= entry_p->rep_len;

			/* Return here, so we don't increment the count of
			 * entries, because we are reusing an existing entry */
			return;
		}
	}

	(Maplist_p[Curr_keymap].entries)++;
}


/* Between parse and placement, this function is called
 * to go through the main list, mapping any strings that need mapping.
 */

void
map_all_strings()
{
	struct MAINLL *mll_p;
	struct STUFF *stuff_p;
	struct GRPSYL *gs_p;
	int v;			/* index through voices */
	int s;			/* index through syllables */
	int w;			/* index through "with" list items */
	int b;			/* index through brace/bracket lists */
	struct KEYMAP *map_p;	/* Which mapping table to use */


	/* If no maps were ever defined, we don't have to do anything here */
	if (Keymap_used == NO) {
		return;
	}

	debug(2, "doing keymapping of strings; %d maps were defined", Num_keymaps);

	prepare_maps_for_searching();

	/* Walk through main list, mapping strings as needed */
	initstructs();
	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		switch (mll_p->str) {

		case S_STAFF:
			/* Strings in STUFFs */
			for (stuff_p = mll_p->u.staff_p->stuff_p; stuff_p != 0;
						stuff_p = stuff_p->next)  {

				/* Some kinds of STUFFs do not get mapped,
				 * so skip those. */
				if (stuff_p->stuff_type == ST_MUSSYM
					|| stuff_p->stuff_type == ST_PEDAL
					|| stuff_p->stuff_type == ST_BEND
					|| stuff_p->stuff_type == ST_OCTAVE
					|| stuff_p->stuff_type == ST_MIDI
					|| stuff_p->modifier == TM_CHORD
					|| stuff_p->modifier == TM_ANALYSIS
					|| stuff_p->modifier == TM_FIGBASS) {
						continue;
				}

				if (stuff_p->string != 0) {
					stuff_p->string = map_string(
						stuff_p->string,
						kpvp_map(mll_p->u.staff_p->staffno, TEXTKEYMAP),
						mll_p->inputfile,
						mll_p->inputlineno);
				}
			}

			/* Lyrics */
			for (s = 0; s < mll_p->u.staff_p->nsyllists; s++) {
				for (gs_p = mll_p->u.staff_p->syls_p[s]; gs_p != 0; gs_p = gs_p->next) {
					if (gs_p->syl == 0) {
						continue;
					}
					gs_p->syl = map_string(
						gs_p->syl,
						kpvp_map(mll_p->u.staff_p->staffno, LYRICSKEYMAP),
						gs_p->inputfile,
						gs_p->inputlineno);
				}
			}

			/* "with" lists */
			for (v = 0; v < MAXVOICES; v++) {
				for (gs_p = mll_p->u.staff_p->groups_p[v]; gs_p != 0; gs_p = gs_p->next) {
					map_p = kpvp_map(mll_p->u.staff_p->staffno, WITHKEYMAP);
					for (w = 0; w < gs_p->nwith; w++) {
						gs_p->withlist[w].string = map_string(
							gs_p->withlist[w].string,
							map_p,
							gs_p->inputfile,
							gs_p->inputlineno);
					}
				}
			}

			break;

		case S_SSV:
			/* Note that none of the parameters that can be
			 * changed mid-measure are string parameters,
			 * so we don't have to deal with them. */
			asgnssv(mll_p->u.ssv_p);

			/* label */
			if (mll_p->u.ssv_p->used[LABEL] == YES) {
				mll_p->u.ssv_p->label = map_string(
					mll_p->u.ssv_p->label,
					kpvp_map(mll_p->u.ssv_p->staffno, LABELKEYMAP),
					mll_p->inputfile, mll_p->inputlineno);
			}

			/* label2 */
			if (mll_p->u.ssv_p->used[LABEL2] == YES) {
				mll_p->u.ssv_p->label2 = map_string(
					mll_p->u.ssv_p->label2,
					kpvp_map(mll_p->u.ssv_p->staffno, LABELKEYMAP),
					mll_p->inputfile, mll_p->inputlineno);
			}

			/* labels for brace and bracket groups */
			map_p = kpvp_map(mll_p->u.ssv_p->staffno, LABELKEYMAP);
			for (b = 0; b < mll_p->u.ssv_p->nbrace; b++) {
				if (mll_p->u.ssv_p->bracelist[b].label != 0) {
					mll_p->u.ssv_p->bracelist[b].label = map_string(
						mll_p->u.ssv_p->bracelist[b].label,
						map_p,
						mll_p->inputfile, mll_p->inputlineno);
				}
				if (mll_p->u.ssv_p->bracelist[b].label2 != 0) {
					mll_p->u.ssv_p->bracelist[b].label2 = map_string(
						mll_p->u.ssv_p->bracelist[b].label2,
						map_p,
						mll_p->inputfile, mll_p->inputlineno);
				}
			}

			for (b = 0; b < mll_p->u.ssv_p->nbrack; b++) {
				if (mll_p->u.ssv_p->bracklist[b].label != 0) {
					mll_p->u.ssv_p->bracklist[b].label = map_string(
						mll_p->u.ssv_p->bracklist[b].label,
						map_p,
						mll_p->inputfile, mll_p->inputlineno);
				}
				if (mll_p->u.ssv_p->bracklist[b].label2 != 0) {
					mll_p->u.ssv_p->bracklist[b].label2 = map_string(
						mll_p->u.ssv_p->bracklist[b].label2,
						map_p,
						mll_p->inputfile, mll_p->inputlineno);
				}
			}

			break;

		case S_BAR:
			map_bar(mll_p->u.bar_p, mll_p->inputfile, mll_p->inputlineno);
			break;

		case S_LINE:
			if (mll_p->u.line_p->string != 0) {
				mll_p->u.line_p->string = map_string(
					mll_p->u.line_p->string,
					kpvp_map(0, TEXTKEYMAP),
					mll_p->inputfile, mll_p->inputlineno);
			}
			break;

		case S_PRHEAD:
		case S_BLOCKHEAD:
		case S_FEED:
			/* The strings in these are processed during parse */
			break;

		case S_CLEFSIG:
			if (mll_p->u.clefsig_p->bar_p != 0) {
				map_bar(mll_p->u.clefsig_p->bar_p,
					mll_p->inputfile, mll_p->inputlineno);
			}
			break;

		default:
			break;
		}
	}
}


/* This function needs to be called before any other key map code is used.
 * Slot 0 is used for the default mapping (which is not to map anything).
 * We use "" (empty string) for its name, and null pointer for its map.
 * This function sets up the table and initializes slot 0.
 */

static void
init_keymaps()
{
	if (Maplist_p == 0) {
		/* Allocate 1 extra for the "default" in slot [0] */
		MALLOC(KEYMAP, Maplist_p, MAX_KEYMAPS + 1);
		Curr_keymap = 0;
		Maplist_p[0].name = "";
		Maplist_p[0].map_p = 0;
		Maplist_p[0].entries = 0;
		Num_keymaps++;
	}
}


/* Return the "handle" of the map associated with the given keymap name,
 * or 0 if no such map has been defined. */

int
keymap_handle(name)

char *name;	/* Look up map with this name */

{
	int m;	/* Index through list of defined maps */

	if (name == 0) {
		return(0);
	}

	/* Loop thorough list of maps. Note that the name
	 * of the default, Maplist_p[0], is "",
	 * so we can treat it like any other map here. */
	for (m = 0; m < Num_keymaps; m++) {
		if (Maplist_p[m].name != 0 && strcmp(name, Maplist_p[m].name) == 0) {
			return(INDEX2HANDLE(m));
		}
	}

	if (Num_keymaps > 0) {
		l_yyerror(Curr_filename, yylineno, "map %s has not been defined", name);
	}
	return(0);
}


/* Return the map associated with the given keymap name, or 0 if no match. */

struct KEYMAP *
get_keymap(name)

char *name;	/* Look up map with this name */

{
	int map;	/* map handle */

	if ((map = keymap_handle(name)) == 0) {
		return(0);
	}
	else {
		return( &(Maplist_p[HANDLE2INDEX(map)]) );
	}
}


/* Given a keymap index, return that map's name or "" if bad index */

char *
keymap_name(map)

int map;

{
	if (map < 0 || map >=  Num_keymaps) {
		return ((char *) "");
	}
	else {
		return(Maplist_p[map].name);
	}
}


/* To handle more than ASCII characters, Mup uses several "character sets."
 * We define a character set as all the fonts that have the same characters.
 * So all the BASE_ fonts are one set, which are all ASCII.
 * Then we have 3 "extended" character sets to cover the other few hundred
 * Standard PostScript characters outside of ASCII.
 * Then there are the miscellaneous fonts (ZI, ZD, ZD2, SYM),
 * the two music character sets, and the user defined character set.
 * For mapping things, we need to know which character set
 * a character belongs to.
 * We use set FONT_TR for the ASCII set. For the extended fonts, we use the set
 * number of the first font among those with the same characters.
 * This lets us add the offset from FONT_TR to get the right font within
 * any character set for a particular font/family.
 * For the others, we use the font number as is.
 */
 
static int
charset(font)

int font;	/* like FONT_TR, FONT_HX, FONT_ZD1, or FONT_MUSIC,
		 * which is to be classified into a set */

{
	int s;

	for (s = 0; s <= NUM_EXT_FONT_SETS; s++) {
		if (font <= (s+1) * NUM_STD_FONTS) {
			return(s * NUM_STD_FONTS + 1);
		}
	}

	/* miscellaneous, music or user defined */ 
	return(font);
}


/* This is sort of like strcmp, except for KEYMAP_ENTRYs rather than strings,
 * and can be used as a comparision function for qsorting a KEYMAP_ENTRY array.
 * It returns negative, zero, or positive, based on whether the first
 * argument goes before the second, they are equal, or it goes after. */

static int
comp_keymap(item1_p, item2_p)

#ifdef __STDC__
const void *item1_p;
const void *item2_p;
#else
char *item1_p;
char *item2_p;
#endif

{
	int complength;	/* how many bytes to compare */
	int comp;	/* result of a strncmp comparison */
	struct KEYMAP_ENTRY *entry1_p;
	struct KEYMAP_ENTRY *entry2_p;


	/* Cast the opaque generic types to what args really are */
	entry1_p = (struct KEYMAP_ENTRY *) item1_p;
	entry2_p = (struct KEYMAP_ENTRY *) item2_p;

	/* If they are the same length, we can do a simple strncmp */
	if (entry1_p->pat_len == entry2_p->pat_len) {
		return(strncmp(entry1_p->pattern, entry2_p->pattern,
			entry1_p->pat_len));
	}

	complength = MIN(entry1_p->pat_len, entry2_p->pat_len);
	if ((comp = strncmp(entry1_p->pattern, entry2_p->pattern, complength))
							 != 0) {
		/* They differed within their common length. */
		return(comp);
	}

	/* Their common length was the same. Put longer before shorter,
	 * because we want longest matches. */
	if (entry1_p->pat_len > entry2_p->pat_len) {
		return(-1);
	}
	else {
		return(1);
	}
}


/* Given a string and its current font, put it in a normalized form that
 * removes the distinctions between rom/bold/ital/boldital and between
 * font families. Each STR_FONT argument is effectively replaced with
 * its "character set." There is a character set for all the fonts that contain
 * ASCII characters, and a character set for each group of extended fonts
 * that contains the same characters. E.g., if a font contains Aacute,
 * then all fonts that contain Aacute at that same code are members of
 * the same character set. The normalization ends at end of str or at the
 * first STR_* command other than STR_FONT, whichever comes first.
 * The normalized string is returned.
 * The normalized string is malloced; caller should free
 * it when they are finished with it.
 */

static char *
normalize(str, currfont, nstrtype)

char *str;	/* What to normalize */
int currfont;	/* The font when at the beginning of str (str can be in
		 * the middle of a longer string so we can't use the
		 * first byte) */
int nstrtype;	/* NST_PATTERN, NST_REPLACEMENT, or NST_MATCH_CANDIDATE */

{
	int length;		/* strlen(str) */
	char *normalized;	/* the result */
	int i;			/* walk through str */
	int ch;			/* one character from str */


	/* Malloc space for normalized version.
	 * If NST_MATCH_CANDIDATE, limit to length of longest pattern,
	 * because we know we can't match more than that.
	 * Add 3 for adding STR_FONT and character set value plus null terminator. */
	length = strlen(str);
	if (nstrtype == NST_MATCH_CANDIDATE && length > Longest_pattern) {
		length = Longest_pattern;
	}
	MALLOCA(char, normalized, length + 3);

	/* Set first two bytes of normalized to STR_FONT and the
	 * character set of currfont */
	normalized[0] = STR_FONT;
	normalized[1] = charset(currfont);

	/* Walk through str */
	for (i = 0; i < length; i++) {
		/* If STR_FONT, replace the font with its character set.
		 * If STR_MUS_CHAR, STR_MUS_CHAR2, or STR_USERDEF1, copy it.
		 * If hit any STR_* other than STR_FONT, jump out. */
		ch = str[i] & 0xff;
		if (IS_STR_COMMAND(ch)) {
			if ( ch == STR_FONT) {
				normalized[i+2] = STR_FONT;
				i++;
				normalized[i+2] = charset(str[i]);
			}
			else if (ch == STR_MUS_CHAR || ch == STR_MUS_CHAR2
						|| ch == STR_USERDEF1) {
				normalized[i+2] = ch;
				i++;
				normalized[i+2] = str[i];
			}
			else {
				break;
			}
		}
		else {
			/* Copy everything else as is */
			normalized[i+2] = str[i];
		}
	}

	normalized[i+2] = '\0';
	/* Could realloc if we have more than we need, but it should be rare
	 * that we could ever save more than a few bytes, if any at all,
	 * and often these are only temporary strings anyway,
	 * so probably not worth it */
	return(normalized);
}


/* Given a string and a font, change all the normalized character set values
 * to match that font. So like if the currfont is TX, any STR_FONT commands
 * with the ASCII character set as argument would get changed to TX, any such
 * commands with argument of the first extended character set font would
 * get changed to the TX version of the first extended character set, etc.
 * The space is malloced; user should free when finished with it.
 */

static char *
unnormalize(str, currfont)

char *str;
int currfont;

{
	char *newstr;		/* to be returned */
	int font_offset;	/* relative to character set */
	int s;			/* index through str */


	/* Calculate font_offset from currfont */
	if (currfont <= BASE_MISC) {
		font_offset = currfont - charset(currfont);
	}
	else {
		/* a one-of-a-kind font, so nothing to translate */
		font_offset = 0;
	}

	/* Malloc space for a new string same length as incoming */
	MALLOCA(char, newstr, strlen(str) + 1);

	/* Walk through, add font_offset to any STR_FONT args */
	for (s = 0; str[s] != '\0'; s++) {
		if ((str[s] & 0xff) == STR_FONT) {
			newstr[s] = str[s];
			s++;
			if ( (str[s] & 0xff) <= BASE_MISC) {
				int newfont;
				newfont = str[s] + font_offset;
				newstr[s] = newfont;
				Fontinfo[font_index(newfont)].was_used = YES;
			}
			else {
				newstr[s] = str[s];
			}
		}
		else {
			newstr[s] = str[s];
		}
	}
	newstr[s] = '\0';

	/* Return new string */
	return(newstr);
}


/* This is vaguely like strncmp, but tells if the initial part of a
 * normalized candidate string matches a normalized pattern.
 * If they match, 0 is returned. If they don't match, but some future
 * pattern in a sorted list of pattern might match, it returns -1.
 * If it doesn't match, and it's already past any pattern in a sorted list
 * that could match, it returns 1. In the sorted list, patterns with
 * overlapping initial values must be sorting longest to shortest,
 * because we want to match the longest pattern.
 */

static int
match(candidate, cand_len, pattern, pat_len)

char *candidate;	/* the string to try to match */
int cand_len;		/* expected to be strlen(candidate) */
char *pattern;		/* the pattern to check */
int pat_len;		/* expected to be strlen(pattern) */

{
	if ((candidate[0] & 0xff) != STR_FONT
					|| (pattern[0] & 0xff) != STR_FONT) {
		pfatal("in match() normalized string doesn't start with font as expected; got 0x%x and 0x%x",
					candidate[0] & 0xff, pattern[0] & 0xff);
	}

	/* The entire pattern must match (although the entire candidate does
	 * not, just an initial substring), so if too short,
	 * this is not a match. */
	if (cand_len < pat_len) {
		return(-1);
	}

	return (strncmp(pattern, candidate, pat_len));
}


/* Given a candidate string and a keymap, find the longest matching pattern 
 * in the map, if any. If found, return the replacement, else zero.
 */

static char *
find_match(candidate, keymap_p, used_p)

char *candidate;		/* try to find something matching this... */
struct KEYMAP *keymap_p;	/* ... in this keymap */
int *used_p;			/* how many bytes of candidate matched is
				 * returned here */

{
	int index;		/* of pattern to try to match */
	int result;		/* return code of match() */
	int cand_length;	/* strlen(candidate) */


	if (keymap_p == 0) {
		return(0);
	}

	*used_p = 0;

	/* Based on the first character code of the candidate string,
	 * we look up the index of the first entry in the map that starts
	 * with that code. */
	if ((index = keymap_p->code_index[FIRST_CODE_INDEX(candidate)]) < 0) {
		/* No patterns exist that start like the candidate string
		 * so can't be any patterns that match */
		return(0);
	}

	for (cand_length = strlen(candidate); cand_length > 0; cand_length--) {
		for (    ; index < keymap_p->entries; index++) {
			result = match(candidate, cand_length,
					keymap_p->map_p[index].pattern,
					keymap_p->map_p[index].pat_len);
			if (result == 0) {
				/* subtract off the leading STR_FONT */
				*used_p = keymap_p->map_p[index].pat_len - 2;
				return(keymap_p->map_p[index].replacement);
			}
			else if (result > 0) {
				/* give up on inner loop. Outer loop
				 * may then try a shorter pattern. */
				break;
			}
		}
	}
	return(0);
}


/* Do key mapping of given string using given map.
 * The mapped string is returned. It will be the original str if no mapping
 * was needed, otherwise the original str will have been freed, and
 * a malloc-ed mapped replacement is returned. */

static char *
map_string(str, map_p, fname, linenum)

char *str;			/* The string to be mapped */
struct KEYMAP *map_p;		/* which map to use */
char *fname;			/* input file name, for error messages */
int linenum;			/* input line number, for error messages */

{
	int curr_font;
	char *segment;		/* piece of str that is a candidate
				 * for maybe being replaced */
	char *replacement;	/* normalized replacement */
	char *realrep;		/* unnormalized replacement */
	int rep_len;		/* length of realrep */
	int used;		/* how many bytes of str were used by the
				 * most recent processing, either the length
				 * that was replaced by a mapping, or the
				 * length of what was copied as is because
				 * it didn't match anything to be mapped. */
	char *result;		/* the complete mapped string */
	int result_len;		/* allocated length of result */
	int s;			/* index of current place in str */
	int r;			/* index of current place in result */
	int i;			/* index into replacement */
	int mapped_something;	/* NO until we find something to map */


	if (map_p == 0) {
		/* Probably no mapping needed on this string, but there
		 * could be--if there was a \m in it, */
		for (s = 0; str[s] != '\0'; s++) {
			if ((str[s] & 0xff) == STR_KEYMAP) {
				break;
			}
		}
		if (str[s] == '\0') {
			/* As suspected; no mapping needed */
			return(str);
		}
	}

	curr_font = str[0];
	mapped_something = NO;

	/* We start with an empty result (mapped) string, guessing the
	 * same size as incoming. We might need to grow it later. */
	result_len = strlen(str) + 1;
	MALLOCA(char, result, result_len);
	/* set up to handle backspaces, if there are any */
	bs_init(result_len);

	/* Copy the font/size header byte, and set index to just after that */
	result[0] = str[0];
	result[1] = str[1];
	r = 2;

	/* Chop string into normalized potential matches */
	for (s = 2; str[s] != '\0'; s += used) {
		segment = normalize(str + s, curr_font, NST_MATCH_CANDIDATE);

		/* If there is a match, get the normalized replacement */
		if ((replacement = find_match(segment, map_p, &used)) != 0) {

			/* This is to be mapped, so get the replacement
			 * that is unnormalized to account for the
			 * current font */
			realrep = unnormalize(replacement, curr_font);

			/* Adjust size of result string if needed */
			rep_len = strlen(realrep);
			if (rep_len > used) {
				result_len += (rep_len - used);
				REALLOCA(char, result, result_len);
			}

			/* Do replacement, saving away information to
			 * be able to do backspace processing, if needed. */
			for (i = 0; i < rep_len; i++, r++) {
				result[r] = realrep[i];
				bs_push(realrep[i], curr_font);
			}

			/* We don't need the intermediate anymore */
			FREE(realrep);

			/* We now will have to eventually return the
			 * new mapped result rather than keeping the orig */
			mapped_something = YES;
		}

		/* If no match, skip past this "character" (which could
		 * be a STR_* and its argument) */
		else {
			/* Always copy the current byte as is */
			result[r++] = str[s];

			/* If that was a special STR_* command, handle it */
			switch (str[s] & 0xff) {

			case STR_FONT:
				/* copy the font value, but also remember it */
				result[r++] = str[s+1];
				curr_font = str[s+1];
				used = 2;
				break;

			case STR_MUS_CHAR:
			case STR_MUS_CHAR2:
			case STR_USERDEF1:
			case STR_SIZE:
			case STR_PAGENUM:
			case STR_NUMPAGES:
				/* Just copy */
				result[r++] = str[s+1];
				used = 2;
				break;
			case STR_VERTICAL:
				/* Copy like those above, but this one
				 * also means we can't backspace before
				 * here, so transmit that info to backspace
				 * processing code. */
				bs_push(STR_VERTICAL, curr_font);
				result[r++] = str[s+1];
				used = 2;
				break;

			case STR_BACKSPACE:
				result[r++] = bs_pop(fname, linenum, 0);
				used = 2;
				break;

			case STR_TAG:
				/* Copy the entire tag as is */
				for (used = 1; ; used++) {
					result[r++] = str[s+used];
					if ((str[s+used] & 0xff) == (STR_END_TAG & 0xff)) {
						used++;
						break;
					}
				}
				break;

			case STR_KEYMAP:
				/* switch to a different keymap */
				map_p = &(Maplist_p[ HANDLE2INDEX(str[s+1] & 0xff) ]);
				used = 2;
				/* Oops. We really shouldn't have copied the
				 * STR_KEYMAP to the result.  More efficient
				 * to just undo that mistake here than to
				 * check for it on every byte, since this is
				 * probably going to be very rare. */ 
				r--;
				/* We are discarding the STR_KEYMAP, so that
				 * means we can't use the original string
				 * anymore. */
				mapped_something = YES;
				break;

			case STR_PRE:
			case STR_U_PRE:
			case STR_PRE_END:
			case STR_PST:
			case STR_U_PST:
			case STR_PST_END:
			case STR_BOX:
			case STR_BOX_END:
			case STR_CIR:
			case STR_CIR_END:
			case STR_L_ALIGN:
			case STR_C_ALIGN:
			case STR_PILE:
			case STR_SLASH:
				/* These have no arguments, so nothing to do
				 * beyond the normal default. */
			default:
				used = 1;
				break;
			}
		}
		FREE(segment);
	}

	if (mapped_something == YES) {
		/* We don't need the original string anymore; it is being
		 * replaced by the result */
		FREE(str);

		/* Terminate the result string */
		result[r] = '\0';

		/* If it turned out the result was shorter than the original,
		 * shed the excess. */
		if (strlen(result) + 1 < result_len) {
			REALLOCA(char, result, strlen(result) + 1);
		}

		return(result);
	}
	else {
		/* Nothing was mapped, so all the work this function did
		 * was for naught. We can just use the original. */
		FREE(result);
		return(str);
	}
}


/* Return the keymap parameter associated with the given staff
 * via viewpathing. */

static struct KEYMAP *
kpvp_map(staffno, param)

int staffno;		/* staff, could be 0 to get score value */
int param;		/* Which keymap parameter is of interest */

{
	struct KEYMAP *keymap_p;


	switch(param) {
	case DEFAULTKEYMAP:
		return(svpath(staffno, DEFAULTKEYMAP)->defaultkeymap);
		break;
	case ENDINGKEYMAP:
		keymap_p = svpath(staffno, ENDINGKEYMAP)->endingkeymap;
		break;
	case LABELKEYMAP:
		keymap_p = svpath(staffno, LABELKEYMAP)->labelkeymap;
		break;
	case LYRICSKEYMAP:
		keymap_p = svpath(staffno, LYRICSKEYMAP)->lyricskeymap;
		break;
	case PRINTKEYMAP:
		keymap_p = svpath(staffno, PRINTKEYMAP)->printkeymap;
		break;
	case REHEARSALKEYMAP:
		keymap_p = svpath(staffno, REHEARSALKEYMAP)->rehearsalkeymap;
		break;
	case TEXTKEYMAP:
		keymap_p = svpath(staffno, TEXTKEYMAP)->textkeymap;
		break;
	case WITHKEYMAP:
		keymap_p = svpath(staffno, WITHKEYMAP)->withkeymap;
		break;
	default:
		pfatal("svp_map passed invalid parameter %d", param);
		/*NOTREACHED*/
		keymap_p = 0;	/* for lint */
		break;
	}
	if (keymap_p == 0) {
		/* User must not have set the specific keymap parameter,
		 * so fall back to the default */
		keymap_p = svpath(staffno, DEFAULTKEYMAP)->defaultkeymap;
	}
	return(keymap_p);
}


/* Do key mapping of strings associated with a bar line, namely rehearsal
 * strings (as specified by user, not internally generated ones),
 * and endings. This could be a normal bar or a pseudo-bar. */

static void
map_bar(bar_p, fname, linenum)

struct BAR *bar_p;
char *fname;			/* input file name, for error messages */
int linenum;			/* input line number, for error messages */

{
	/* rehearsal string */
	if (bar_p->reh_type == REH_STRING && bar_p->reh_string != 0) {
		bar_p->reh_string = map_string(bar_p->reh_string,
			kpvp_map(0, REHEARSALKEYMAP), fname, linenum);
	}

	/* ending string */
	if (bar_p->endinglabel != 0) {
		bar_p->endinglabel = map_string(bar_p->endinglabel,
			kpvp_map(0, ENDINGKEYMAP), fname, linenum);
	}
}


/* Go through all the key maps. For each, sort the entries. Then make an
 * index of the first entries starting with each code, for quick lookup.
 */

static void
prepare_maps_for_searching()

{
	int m;		/* index through maps */
	int i;		/* index through code_index */
	int e;		/* index through map entries */
	int startchar_index;	/* index of entry in map of the first entry
				 * starting with a given code */

	for (m = 0; m < Num_keymaps; m++) {

		if (Maplist_p[m].is_sorted == YES) {
			/* already handled earlier on some previous call */
			continue;
		}

		qsort(Maplist_p[m].map_p, Maplist_p[m].entries,
				sizeof(struct KEYMAP_ENTRY), comp_keymap);
		/* For each character that has patterns that start with it,
		 * save the index to the first such pattern, so we can
		 * jump immediately to the first possible match. 
		 * First init all slot to negative value to mark as unused. */
		for (i = 0; i < MAX_CHARS_IN_FONT; i++) {
			Maplist_p[m].code_index[i] = -1;
		}
		for (e = 0; e < Maplist_p[m].entries; e++) {
			/* The first two bytes of pattern will have been
			 * normalized to STR_FONT and some character set,
			 * so the code will be in byte [2] */
			startchar_index = FIRST_CODE_INDEX(Maplist_p[m].map_p[e].pattern);
			if (Maplist_p[m].code_index[startchar_index] < 0) {
				/* Hadn't seen this code yet, so this is the
				 * first entry for the code; save it. */
				Maplist_p[m].code_index[startchar_index] = e;
			}
		}
		Maplist_p[m].is_sorted = YES;

		/* NOTE: It is likely we have more entries allocated than were
		 * actually used, so we could potentially REALLOC them here
		 * (if not somewhere else earlier).
		 * But with current settings, it would be less than 1K
		 * worst case on a 32-bit machine, 2K on 64-bit,
		 * and frequently a lot less than worst case,
		 * so probably not worth the effort. If KEYMAP_ENTRY and/or
		 * MAX_KEYMAPS get a lot bigger, could reconsider.
		 */
	}
}


/* This gets called from parse phase to map strings that are in blocks.
 * They have to be done at that time, because it is kind of doing
 * "placement" types things, by calculating how tall strings are,
 * so it needs to have to actual strings, after mapping.
 */

char *
map_print_str(str, fname, linenum)

char *str;
char *fname;
int linenum;

{
	prepare_maps_for_searching();
	return(map_string(str, kpvp_map(0, PRINTKEYMAP), fname, linenum));
}
