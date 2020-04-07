
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

/* This file contains functions related to characters, and strings that are
 * comprised of characters. There are functions that return
 * height/width/ascent/descent about characters and strings.
 * There are functions that "normalize" strings into internal format,
 * handle backspacing, user defined and music symbols, etc.
 */

#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"


/* code for invalid music character */
#define BAD_CHAR	'\377'
/* code for invalid number in user input string */
#define BAD_NUMBER	-30000

/* Font Kind values. STD are the 7x4 standard PostScript fonts.
 * The EXT* fonts are fonts we create to access the "hidden" characters
 * in those standard fonts that are beyond the ASCII set.
 * MUS* are our Mup music fonts. USER1 is for user-defined symbols.
 * SYM is the PostScript symbol font, and Z* are the Zapf fonts.
 */
#define FK_STD		(0x1)
#define FK_EXT1		(0x2)
#define FK_EXT2		(0x4)
#define FK_EXT3		(0x8)
#define FK_MUS1		(0x10)
#define FK_MUS2		(0x20)
#define FK_USER1	(0x40)
#define FK_SYM		(0x80)
#define FK_ZI		(0x100)
#define FK_ZD1		(0x200)
#define FK_ZD2		(0x400)

/* What kinds of fonts the user can override. Can't easily allow overriding
 * STD or EXT* as there are actually 28 versions of each character, so we
 * wouldn't know which one(s) to override. We could probably allow overriding
 * SYM and Z*, but don't currently. */
#define FK_CAN_OVERRIDE	(FK_MUS1 | FK_MUS2 | FK_USER1)

/* Character names and how many there are for each font type */
extern char *FI_std_names[];
extern int FI_std_numchars;
extern char *FI_ext1_names[];
extern int FI_ext1_numchars;
extern char *FI_ext2_names[];
extern int FI_ext2_numchars;
extern char *FI_ext3_names[];
extern int FI_ext3_numchars;
extern char *FI_mus0_names[];
extern int FI_mus0_numchars;
extern char *FI_mus1_names[];
extern int FI_mus1_numchars;
extern char *FI_ZD1_names[];
extern int FI_ZD1_numchars;
extern char *FI_ZD2_names[];
extern int FI_ZD2_numchars;
extern char *FI_SYM_names[];
extern int FI_SYM_numchars;

/* Information about a PostScript character symbol. */
struct CHARINFO {
	char *name;		/* The PostScript CharStrings name */
	short fontkind;		/* FK_* value */
	unsigned char code;	/* 32 - 191 */
	struct CHARINFO *next;	/* for hash chain collisions */
};

/* Make the size of the hash table of character names a power of two,
 * so we can do cheap AND rather than modulo. There are on the order of
 * 1000 names, so make big enough for not too many collisions, but not
 * so big as to waste lots of space. */
#define CH_TBL_SIZE	(512)
#define CH_TBL_MASK	(CH_TBL_SIZE - 1)

struct CHARINFO *Char_table[CH_TBL_SIZE];


/* save information about characters in string as we go, in order to be
 * able to backspace back over them */
struct BACKSPACEINFO {
	int	font;
	char	code;
};

	/* Variables for handling processing of backspaces */
/* This is a malloc-ed array where we save info to be able to do backspacing */
static struct BACKSPACEINFO *BS_info_p = 0;
/* This is the current array element in BS_info_p */
static int BS_index;
/* This is how many allocated array elements are pointed to by BS_info_p */
static int BS_allocated_len;
/* This gets set to NO if we encounter something that terminates the
 * ability to backspace, like verticial motion of a newline. */
static short BS_is_allowed;

/* Font from which we are allocating user-defined characters */
static int Curr_usym_font = FONT_USERDEF1;
/* Font of the currently-being-defined character. This will be the same
 * as Curr_usym_font except when user is overriding a builtin. */
static int Usym_font = FONT_UNKNOWN;
/* the "ascii" code of the currently-being-defined character - FIRST_CHAR */
static int Usym_code;
/* YES/NO to tell us if current user-defined symbol is overriding an
 * existing symbol, since that needs slightly different handling. */
static int Usym_override;

#ifndef __STDC__
extern char *bsearch();	/* binary search library function */
#endif
extern long strtol();

/* static functions */
static char * com_fix_string P((char *string, int font, int size,
		char *fname, int lineno, int allow_tags, char *exclusion_type));
static char *get_font P((char *string, int *font_p, int curr_font,
		int prev_font, char *fname, int lineno));
static int family_base_of P((int font));
static char *get_num P((char *string, int *num_p));
static void add_char P((char *name, int fontkind, int code));
static double lwidth_common P((char *string, int consider_start_pile));
static int starts_piled P((char *string, int *font_p, int *size_p,
		char **pile_start_p_p));
static int str_cmd P((char *str, int *size_p, int *font_p, int *in_pile_p));
static int get_accidental P((unsigned char *str, char *accidental_p,
		int *acc_size_p, int trans_natural, int *escaped_p));
static int add_accidental P((char *buff, int acc_character, int acc_size,
		int escaped));
static int dim_tri P((unsigned char *str, char *replacement,
		int size, int is_chord));
static int smallsize P((int size));
static int accsize P((int size));
static char *migrate_font_size P((char *string, char *out_p, char *deststring));
static void validate_tag P((char *tag, char *fname, int lineno));


/* return the height (in inches) of a character of specified font and size */

double
height(font, size, ch)

int font;
int size;
int ch;		/* which character */

{
	int chval;

	chval = ch & 0xff;

	/* control characters have no height */
	if (chval < FIRST_CHAR) {
		return(0.0);
	}

	return((Fontinfo[ font_index(font) ].ch_height[ CHAR_INDEX(chval) ] 
		/ FONTFACTOR) * ((float)size / (float)DFLT_SIZE) );
}


/* return the width (in inches) of a character of specified font and size */

double
width(font, size, ch)

int font;
int size;
int ch;		/* which character */

{
	int chval;

	chval = ch & 0xff;

	/* control characters have no width */
	if (chval < FIRST_CHAR) {
		return(0.0);
	}

	return((Fontinfo[ font_index(font) ].ch_width[ CHAR_INDEX(chval) ] 
		/ FONTFACTOR) * ((float)size / (float)DFLT_SIZE) );
}


/* return the ascent (in inches) of a character of specified font and size */

double
ascent(font, size, ch)

int font;
int size;
int ch;		/* which character */

{
	int chval;

	chval = ch & 0xff;

	/* control characters have no ascent */
	if (chval < FIRST_CHAR) {
		return(0.0);
	}

	return((Fontinfo[ font_index(font) ].ch_ascent[ CHAR_INDEX(chval) ]
		/ FONTFACTOR) * ((float) size / (float)DFLT_SIZE) );
}


/* return the descent (in inches) of a character of specified font and size */

double
descent(font, size, ch)

int font;
int size;
int ch;		/* which character */

{
	return ( height(font, size, ch) - ascent(font, size, ch) );
}


/* given a user input string, normalize it. This means:
 * Put the default font in [0] and default size in [1] of the string.
 * Change backslashed things to internal format. Each starts with a
 * hyper-ASCII code byte and is followed by one or more data bytes.
 * Note that in all cases in internal format is no longer than the
 * incoming format.
 * Change any \f(XX) to	  STR_FONT font_number
 * Change any \s(NN) to   STR_SIZE actual_size
 *	Note that NN might have a sign to indicate relative size change.
 * Change any \v(NN) to   STR_VERTICAL vertical_offset
 *	Note that NN might have a sign to indicate direction,
 *	negative means downward.
 * Change any \m(XX) to    STR_KEYMAP map_number
 * Change any \/ to	  STR_SLASH
 * Change any \| to       STR_L_ALIGN (piled mode only)
 * Change any \^ to       STR_C_ALIGN (piled mode only)
 * Change any backslashed space to space when in piled mode
 * Change any space to newline while in piled mode
 * Change \(xxxx) to      STR_MUS_CHAR size mus_char_code
 * Change \% to           STR_PAGENUM %
 * Change \# to		  STR_NUMPAGES #
 * Change \n to newline
 * Change \b to           STR_BACKSPACE n
 *      where n is how much to back up for the
 *	default size, in BACKSP_FACTORths of an inch
 * Change backslashed backslash or double quote to just be themselves.
 * Reject any other control characters or illegal backslash escapes.
 * The string is null-terminated.
 *
 * The normalized string is put back into the original string buffer
 * that was passed in, and a pointer to it is returned.
 *
 * Note that some functions in lyrics.c, and prntdata.c
 * also have knowledge of the escape conventions,
 * so if these change, check there too. But it is intended
 * that all the rest of the code gets at strings indirectly
 * via functions in this file, so the details can be hidden here.
 *
 * Once there was just a fix_string function, but then we allowed tags
 * inside lyrics, so there is a fix_tagged_string function that allows that.
 * Then with keymaps, another variation was added.
 * All the work is done in a common function that just allows or disallows
 * tag processing.
 */

char *
fix_string(string, font, size, fname, lineno)

char *string;	/* original string */
int font;	/* default font for string */
int size;	/* default size for string */
char *fname;	/* file name, for error messages */
int lineno;	/* input line number, for error messages */

{
	return com_fix_string(string, font, size, fname, lineno, NO, 0);
}


char *
fix_tagged_string(string, font, size, fname, lineno)

char *string;	/* original string */
int font;	/* default font for string */
int size;	/* default size for string */
char *fname;	/* file name, for error messages */
int lineno;	/* input line number, for error messages */

{
	return com_fix_string(string, font, size, fname, lineno, YES, 0);
}

/* In keymap patterns and replacements, only \(xxx) characters and a few
 * other backslashed things are allowed. */

char *
fix_pat_rep_string(string, font, size, fname, lineno, str_type)

char *string;	/* original string */
int font;	/* default font for string */
int size;	/* default size for string */
char *fname;	/* file name, for error messages */
int lineno;	/* input line number, for error messages */
char *str_type;	/* something like "keymap pattern" or "keymap replacement" */

{
	return com_fix_string(string, font, size, fname, lineno, YES, str_type);
}

static char *
com_fix_string(string, font, size, fname, lineno, allow_tags, exclusion)

char *string;	/* original string */
int font;	/* default font for string */
int size;	/* default size for string */
char *fname;	/* file name, for error messages */
int lineno;	/* input line number, for error messages */
int allow_tags;	/* YES if \=(tag) things are allowed */
char *exclusion;/* If this is non-null, then almost all backslash escapes
		 * are disallowed, and this string should be used to explain
		 * why they are excluded if the user uses one of them */

{
	char *tmpbuff;			/* for normalized string */
	int leng;			/* strlen(string) + 1 */
	char *inp_p, *out_p;		/* walk thru orig & normalized string */
	int nsize;			/* new size */
	int prevsize;			/* previous size */
	int msize;			/* size for music character */
	int vert;			/* argument to \v without sign */
	int vertval = 0;		/* signed argument to \v */
	int has_vertical = NO;		/* YES if \v or pile found */
	int has_newline = NO;		/* YES if \n somewhere in string */
	int pile_mode = NO;
	int align_points = 0;		/* how many alignment points found */
	int np_align_points = 0;	/* how many non-pile alignment points */
	int error;			/* YES if have found an error that 
					 * caused us to have a half-finished
					 * STR_* command that we will need
					 * to clean up. */
	char spec_name[100], *sc_p;	/* name of special music character, or
					 * extended character set character */
	unsigned char extchar;		/* value for extended character */
	int spec_font;			/* The font of non-standard chars */
	int is_small;			/* If a music char is "small" */
	int now_font;			/* current font */
	int newfont;			/* proposed new font */
	int prevfont;			/* previous font */
	int backupval;			/* value to store for backspace
					 * distance */


	/* fill in default font and size */
	string[0] = (char) font;
	if (rangecheck(size, MINSIZE, MAXSIZE, "size") == NO) {
		size = MAXSIZE;
	}
	string[1] = (char) size;
	now_font = prevfont = font;
	Fontinfo[font_index(font)].was_used = YES;
	prevsize = size;

	leng = strlen(string) + 1;
	MALLOCA(char, tmpbuff, leng);
	bs_init(leng);
	/* walk through incoming string, creating normalized string */
	for (error = NO, out_p = tmpbuff + 2, inp_p = string + 2;
					(error == NO) && (*inp_p != '\0');
					inp_p++, out_p++) {

		/* handle backslashed stuff */
		if (*inp_p == '\\') {

			/* skip past the backslash */
			inp_p++;	

			/* If exclusion is set, then only a small subset of
			 * backslash escapes are allowed. */
			if (exclusion != 0) {
				switch (*inp_p) {
				case 'n':	/* line continuation */
				case 'r':	/* line continuation */
				case '(':	/* symbol name */
				case '\\':	/* literal backslash */
				case '"':	/* embedded quote */
					/* The preceding list contains the
					 * only things allowed */
					break;
				default:
					l_yyerror(fname, lineno, "\\%c not allowed in a %s", *inp_p, exclusion);
					break;
				}
			}
			switch( *inp_p) {

			case '\n':
				/* ignore the backslashed newline */
				out_p--;
				break;

			case '\r':
				if (*(inp_p) == '\n') {
					inp_p++;
				}
				out_p--;
				break;

			case 'f':
				/* font change */
				inp_p = get_font(++inp_p, &newfont, now_font,
						prevfont, fname, lineno);
				if (newfont == FONT_UNKNOWN) {
					error = YES;
				}
				else {
					*out_p++ = (char) STR_FONT;
					*out_p = (char) newfont;
					prevfont = now_font;
					now_font = newfont;
					Fontinfo[font_index(newfont)].was_used = YES;
				}
				break;

			case 's':
				/* size change */
				if (*++inp_p == '(') {
					switch (*++inp_p) {
					case '+':
						inp_p = get_num(++inp_p, &nsize);
						if (nsize > 0) {
							nsize += size;
						}
						break;
					case '-':
						inp_p = get_num(++inp_p, &nsize);
						if (nsize > 0) {
							nsize = size - nsize;
						}
						break;
					case 'P':
						if (strncmp(inp_p, "PV)", 3) == 0) {
							nsize = prevsize;
							inp_p += 2;
						}
						else {
							nsize = BAD_NUMBER;
						}
						break;
					case 'p':
						if (strncmp(inp_p, "previous)", 9) == 0) {
							nsize = prevsize;
							inp_p += 8;
						}
						else {
							nsize = BAD_NUMBER;
						}
						break;
					default:
						inp_p = get_num(inp_p, &nsize);
						break;
					}
				}
				else {
					nsize = BAD_NUMBER;
				}

				/* if got valid size, store it */
				if (nsize == BAD_NUMBER) {
					l_yyerror(fname, lineno,
						"Invalid format for size value");
					error = YES;
				}
				else if (rangecheck(nsize, MINSIZE,
						MAXSIZE, "size") == YES) {
					*out_p++ = (char) STR_SIZE;
					*out_p = (char) nsize;
					/* save new size */
					prevsize = size;
					size = nsize;
				}
				else {
					error = YES;
				}

				break;

			case 'v':
				/* vertical motion */
				if (*++inp_p == '(') {
					switch (*++inp_p) {
					case '-':
						inp_p = get_num(++inp_p, &vert);
						if (vert >= 0) {
							vertval = -vert;
						}
						break;

					case '+':
						++inp_p;
						/* fall through */
					default:
						inp_p = get_num(inp_p, &vert);
						if (vert >= 0) {
							vertval = vert;
						}
						break;
					}
				}
				else {
					vert = BAD_NUMBER;
				}

				if (vert == BAD_NUMBER) {
					l_yyerror(fname, lineno,
						"Invalid format for vertical motion value");
					error = YES;
				}
				else if (rangecheck(vertval, -100, 100,
						"vertical") == YES) {
					/* if motion is zero, don't even bother
					 * to save it, else do */
					if (vertval != 0) {
						/* convert percentage to 
						 * STR_VERTICAL units */
						if (vertval > 0) {
							vertval = vertval *
								MAXVERTICAL/100;
						}
						else {
							vertval = -vertval *
								MINVERTICAL/100;
						}
						*out_p++ = (char) STR_VERTICAL;
						*out_p = (char) ENCODE_VERT(
							vertval );
					}
				}
				else {
					error = YES;
				}

				/* we don't allow backspacing to something
				 * before a vertical motion--this is almost
				 * like a newline. */
				bs_push(STR_VERTICAL, now_font);

				has_vertical = YES;
			
				break;

			case 'm':
				Keymap_used = YES;
				if (*++inp_p == '(') {
					char *start_p;

					*out_p++ = STR_KEYMAP;
					/* extract the keymap name */
					start_p = ++inp_p;
					/* Skip leading white space */
					while (*inp_p == ' ' || *inp_p == '\t') {
						start_p = ++inp_p;
					}
					for ( ; *inp_p != '\0' && *inp_p != ')';
							inp_p++) {
					}

					if (*inp_p != ')') {
						l_yyerror(fname, lineno,
						"missing ) to end \\m( keymap name");
						/* Index 1 is the "no mapping"
						 * default entry, so is safe
						 * to use in error cases. */
						*out_p = 1;
					}
					else {
						char *term_p;	/* termination of name */
						char saved_char;/* char at *term_p */

						/* Temporarily add terminator,
						 * for looking up name. Skip
						 * any trailing white space. */
						term_p = inp_p;
						while (term_p > start_p &&
							(*(term_p-1) == ' ' ||
							*(term_p-1) == '\t')) {
							term_p--;
						}
						saved_char = *term_p;
						*term_p = '\0';

						if ((*out_p = (char) keymap_handle(start_p)) == 0) {
							/* keymap_handle() will
							 * have printed error */
							*out_p = 1;
						}
						*term_p = saved_char;
					}
				}
				else {
					l_yyerror(fname, lineno,
						"expecting ( after \\m");
					*out_p = 1;
				}
				break;
				
			case ':':
				/* If this begins a pile, and the next thing
				 * in input ends the pile, just ignore them
				 * both to keep things simpler later. */
				if (pile_mode == NO && *(inp_p+1) == '\\'
							&& *(inp_p+2) == ':') {
					inp_p += 2;
					/* no output character */
					out_p--;
				}
				else {
					*out_p = (char) STR_PILE;
					has_vertical = YES;
					pile_mode = (pile_mode == YES ? NO : YES);
				}
				align_points = 0;
				break;

			case '|':
			case '^':
				if (pile_mode == NO) {
					if (++np_align_points > 1) {
						l_yyerror(fname, lineno,
						"only one non-pile alignment point allowed");
					}
					*out_p = (char) (*inp_p == '^' ? STR_C_ALIGN : STR_L_ALIGN);
				}

				else if (++align_points > 1) {
					l_yyerror(fname, lineno,
						"only one alignment point allowed per line");
					*out_p =  *inp_p;
				}

				else if (*inp_p == '^') {
					*out_p = (char) STR_C_ALIGN;
				}
				else {
					*out_p = (char) STR_L_ALIGN;
				}
				has_vertical = YES;
				break;

			case ' ':
				if (pile_mode == NO) {
					l_yyerror(fname, lineno,
						"backslashed space only allowed in piled mode");
				}
				*out_p = ' ';
				break;

			case '/':
				/* This is only allowed after one
				 * or more digits */
				if ( inp_p - string < 4 ||
						! isdigit( *(inp_p - 2)) ) {
					l_yyerror(fname, lineno,
						"slash can only be used after digit(s)");
					*out_p = '/';
				}
				else {
					*out_p = (char) STR_SLASH;
				}
				break;
			case '\\':
			case '"':
				/* real backslash or embedded quote, copy it */
				bs_push(*inp_p, now_font);
				*out_p = *inp_p;
				break;

			case '(':
				/* special music character or extended
				 * character set character */
				/* make copy of name */
				/* First skip any leading white space */
				while (*(inp_p + 1) == ' '
						|| *(inp_p + 1) == '\t') {
					inp_p++;
				}
				for ( sc_p = spec_name, inp_p++;
						*inp_p != ' ' && *inp_p != '\t' &&
						*inp_p != ')' && *inp_p != '\0';
						sc_p++, inp_p++) {
					*sc_p = *inp_p;
				}
				/* Skip trailing white space */
				while (*inp_p == ' ' || *inp_p == '\t') {
					inp_p++;
				}
				if (*inp_p != ')') {
					l_yyerror(fname, lineno,
						"missing ) to end character name");
					error = YES;
				}
				*sc_p = '\0';

				spec_font = now_font;
				is_small = NO;
				if (error == NO &&
						(extchar = find_char(spec_name, &spec_font,
						&is_small, YES))
						!= (unsigned char) BAD_CHAR) {
					/* For a music character, output
					 * its command, size, and code.
					 * For an extended character, change
					 * to its font, output its code,
					 * and put back the original font.
					 */
					if (IS_MUSIC_FONT(spec_font)) {
						int strcmd;
						switch(spec_font) {
						case FONT_MUSIC:
							strcmd = STR_MUS_CHAR;
							break;
						case FONT_MUSIC2:
							strcmd = STR_MUS_CHAR2;
							break;
						case FONT_USERDEF1:
							strcmd = STR_USERDEF1;
							break;
						default:
							strcmd = 0; /* for lint */
							pfatal("missing case for music font %d", spec_font);
							break;
						}
						*out_p++ = (char) strcmd;
						if (is_small == YES) {
							msize = smallsize(size);
						}
						else {
							msize = size;
						}
						*out_p++ = (char) msize;
					}
					else {
						*out_p++ = (char) STR_FONT;
						*out_p++ = (char) spec_font;
					}
					*out_p = extchar;
					if ( ! IS_MUSIC_FONT(spec_font) ) {
						out_p++;
						*out_p++ = (char) STR_FONT;
						*out_p = (char) now_font;
					}

					bs_push(extchar, spec_font);

					/* mark that this extended character
					 * set font has been used */
					Fontinfo[font_index(spec_font)].was_used = YES;
				}
				else {
					error = YES;
				}
				break;

			case '[':
				/* Start of boxed text. We only allow this at
				 * the beginning of a string (after optional
				 * font/size changes). */
				out_p = migrate_font_size(tmpbuff, out_p, string);
				if (out_p != tmpbuff + 2) {
					l_yyerror(fname, lineno,
						"\\[ only allowed at beginning of string (after optional font/size changes)");
					error = YES;
				}
				else {
					*out_p = (char) STR_BOX;
				}
				break;

			case ']':
				/* end of boxed text. Only allowed at end of
				 * string, and only if the string began
				 * with a box start. */
				if (*(inp_p + 1) != '\0') {
					l_yyerror(fname, lineno,
						"\\] only allowed at end of string");
					error = YES;
				}
				else if (IS_BOXED(tmpbuff) == NO) {
					l_yyerror(fname, lineno,
						"no matching \\[ for \\]");
					error = YES;
				}
				else {
					*out_p = (char) STR_BOX_END;
				}
				break;

			case '{':
				/* Start of circled text. We only allow this at
				 * the beginning of a string (after optional
				 * font/size changes) */
				out_p = migrate_font_size(tmpbuff, out_p, string);
				if (out_p != tmpbuff + 2) {
					l_yyerror(fname, lineno,
						"\\{ only allowed at beginning of string (after optional font/size changes)");
					error = YES;
				}
				else {
					*out_p = (char) STR_CIR;
				}
				break;

			case '}':
				/* end of circled text. Only allowed at end of
				 * string, and only if the string began
				 * with a circle start. */
				if (*(inp_p + 1) != '\0') {
					l_yyerror(fname, lineno,
						"\\} only allowed at end of string");
					error = YES;
				}
				else if (IS_CIRCLED(tmpbuff) == NO) {
					l_yyerror(fname, lineno,
						"no matching \\{ for \\}");
					error = YES;
				}
				else {
					*out_p = (char) STR_CIR_END;
				}
				break;

			case '=':
				if (allow_tags == NO) {
					l_yyerror(fname, lineno,
					"tags only allowed inside lyric strings ");
					error = YES;
				}
				if (*++inp_p == '(') {
					char *start_p;

					*out_p++ = STR_TAG;
					start_p = out_p;
					/* Skip leading white space */
					while (*(inp_p + 1) == ' ' ||
							*(inp_p + 1) == '\t') {
						inp_p++;
					}
					while (*++inp_p != '\0') {
						if (*inp_p == ')'
							    || *inp_p == ' '
							    || *inp_p == '\t') {
							break;
						}
						*out_p++ = *inp_p;
					}
					/* Skip trailing white space */
					while (*inp_p == ' ' ||
							*inp_p == '\t') {
						inp_p++;
					}
					if (*inp_p != ')') {
						l_yyerror(fname, lineno,
						"missing ) to end \\=( tag name");
						error = YES;
					}
					*out_p = STR_END_TAG;
					validate_tag(start_p, fname, lineno);
				}
				else {
					l_yyerror(fname, lineno,
						"expecting ( after \\= for tag");
					error = YES;
				}
				break;

			case '%':
				/* too hard to deal with inside a pile... */
				if (pile_mode == YES) {
					l_yyerror(fname, lineno,
						"\\%c not allowed inside a pile\n", '%');
				}

				/* page number -- change to STR_PAGENUM-% */
				*out_p++ = (char) STR_PAGENUM;
				*out_p = '%';
				/* we really don't know at this point how far
				 * to backspace over pagenum because we don't
				 * know yet how many digits it is, etc, so we
				 * punt and just use the % character
				 * for width */
				bs_push('%', now_font);
				break;

			case '#':
				/* code basically the same as for % */
				if (pile_mode == YES) {
					l_yyerror(fname, lineno,
						"\\# not allowed inside a pile\n");
				}

				/* number of pages -- change to STR_NUMPAGES-# */
				*out_p++ = (char) STR_NUMPAGES;
				*out_p = '#';
				/* We really don't know at this point how far
				 * to backspace, because we don't know yet
				 * how many digits it is, etc, so we punt
				 * and just use the # character for width. */
				bs_push('#', now_font);
				break;

			case 'n':
				/* newline */
				*out_p = '\n';
				/* can't back up to previous line */
				bs_push('\n', now_font);
				has_newline = YES;
				break;

			case 'b':
				backupval = bs_pop(fname, lineno, &error);
				if (error == NO) {
					*out_p++ = (char) STR_BACKSPACE;
					*out_p = backupval;
				}
				break;

			default:
				yyerror("illegal \\ escape");
				error = YES;
				break;
			}
		}

		else if (iscntrl(*inp_p) ) {
			if (*inp_p == '\n') {
				bs_push('\n', now_font);
				has_newline = YES;
				*out_p = *inp_p;
			}
			else if (*inp_p == '\r' && *(inp_p+1) == '\n') {
				/* ignore DOS's extraneous \r */
				out_p--;
			}
			else {
				/* We don't support any other control
				 * characters, but just convert others to
				 * space and continue. That way user at least
				 * gets something. Tab is something user may
				 * expect to work, so we give a more clear
				 * and specific error for that.
				 */
				l_warning(fname, lineno,
					"unsupported control character '\\0%o' %sin string replaced with space",
					*inp_p, *inp_p =='\t' ? "(tab) ": "");
				*out_p = ' ';
			}
		}
		else if  (pile_mode == YES && *inp_p == ' ') {
			/* in piled mode, space means move down for next
			 * item in pile. */
			*out_p = '\n';
			
			align_points = 0;
			bs_push('\n', now_font);
		}
		else {
			/* normal character -- copy as is */
			*out_p = *inp_p;
			bs_push(*inp_p, now_font);
		}
	}
	/* If we got an error, we would not have put anything into the
	 * final output position before incrementing out_p in the 'for' loop,
	 * so compensate, so we don't leave a garbage character. */
	if (error == YES) {
		out_p--;
	}
	*out_p = '\0';

	if (error == NO && IS_BOXED(tmpbuff) == YES &&
				(*(out_p - 1) & 0xff) != (STR_BOX_END & 0xff)) {
		l_yyerror(fname, lineno, "no matching \\] for \\[");
	}

	if (error == NO && IS_CIRCLED(tmpbuff) == YES &&
				(*(out_p - 1) & 0xff) != (STR_CIR_END & 0xff)) {
		l_yyerror(fname, lineno, "no matching \\} for \\{");
	}

	/* to keep things simple, we don't allow
	 * mixing newlines with vertical motion */
	if (has_vertical == YES && has_newline == YES) {
		l_yyerror(fname, lineno,
			"can't have newline in same string with vertical motion or alignment");
	}

	/* now copy normalized string back onto original */
	(void) strcpy(string + 2, tmpbuff + 2);
	FREE(tmpbuff);
	return(string);
}


/* given pointer into a string, read a font name exclosed in parentheses.
 * Return the corresponding font number, or
 * FONT_UNKNOWN if name is invalid. Return pointer to last character
 * processed in string */

static char *
get_font(string, font_p, curr_font, prev_font, fname, lineno)

char *string;	/* get font from this string */
int *font_p;	/* return new font via this pointer */
int curr_font;	/* current font */
int prev_font;	/* previous font */
char *fname;	/* file name for errors */
int lineno;	/* line number, for error messages */

{
	char fontname[BUFSIZ];
	int font = FONT_UNKNOWN;
	char *endparen;		/* where ')' is in string */
	int length;		/* of font name */


	if (*string == '(') {
		string++;
		/* Skip leading space */
		while (*string == ' ' || *string == '\t') {
			string++;
		}
	 	if ((endparen = strchr(string, ')')) != (char *) 0) {
			length = endparen - string;
			/* Trim trailing space */
			while (length > 0 && (string[length - 1] == ' ' ||
						string[length - 1] == '\t')) {
				length--;
			}
			(void) strncpy(fontname, string, (unsigned) length);
			fontname[length] = '\0';
			string = endparen;
			if (strcmp(fontname, "PV") == 0
					|| strcmp(fontname, "previous") == 0) {
				/* special case of "previous" font */
				font = prev_font;
			}

			/* special cases of R, I, B, or X, to mean roman
			 * italic, bold, or boldital in the current family */
			else if (strcmp(fontname, "R") == 0 ||
					strcmp(fontname, "rom") == 0) {
				font = family_base_of(curr_font) + FONT_TR;
			}
			else if (strcmp(fontname, "I") == 0 ||
					strcmp(fontname, "ital") == 0) {
				font = family_base_of(curr_font) + FONT_TI;
			}
			else if (strcmp(fontname, "B") == 0 ||
					strcmp(fontname, "bold") == 0) {
				font = family_base_of(curr_font) + FONT_TB;
			}
			else if (strcmp(fontname, "X") == 0 ||
					strcmp(fontname, "boldital") == 0) {
				font = family_base_of(curr_font) + FONT_TX;
			}
			
			else {
				font = lookup_font(fontname);
			}
		}
	}

	*font_p = font;
	if (font == FONT_UNKNOWN) {
		l_yyerror(fname, lineno, "unknown font specified");
	}
	return(string);
}


/* Given a font, return the font number of its family base, to which
 * one could add FONT_TR, FONT_TI, FONT_TB, or FONT_TX to get the
 * corresponding face in the same family as the given font.
 * This uses a fairly brute force approach; it doesn't seem worth
 * trying to be overly clever for something that will rarely be used.
 */

static int
family_base_of(font)

int font;

{
	if (font >= FONT_TR && font <= FONT_TX) {
		return(BASE_TIMES);
	}
	if (font >= FONT_AR && font <= FONT_AX) {
		return(BASE_AVANTGARDE);
	}
	if (font >= FONT_CR && font <= FONT_CX) {
		return(BASE_COURIER);
	}
	if (font >= FONT_HR && font <= FONT_HX) {
		return(BASE_HELVETICA);
	}
	if (font >= FONT_BR && font <= FONT_BX) {
		return(BASE_BOOKMAN);
	}
	if (font >= FONT_NR && font <= FONT_NX) {
		return(BASE_NEWCENTURY);
	}
	if (font >= FONT_PR && font <= FONT_PX) {
		return(BASE_PALATINO);
	}
	pfatal("family_base_of() passed unknown font %d", font);
	/*NOTREACHED*/
	/* but keep lint happy */
	return(0);
}


/* given a pointer into a string, get a number followed by close parenthesis.
 * Return the number via pointer, or BAD_NUMBER on error.
 * Return pointer to the last character processed
 * in the incoming string */

static char *
get_num(string, num_p)

char *string;	/* get number from this string */
int *num_p;	/* return number via this pointer, or -1 on error */

{
	/* Skip leading white space */
	while (*string == ' ' || *string == '\t') {
		string++;
	}
	if (isdigit(*string)) {
		*num_p = strtol(string, &string, 10);
		/* Skip trailing white space */
		while (*string == ' ' || *string == '\t') {
			string++;
		}
		if (*string != ')') {
			*num_p = BAD_NUMBER;
		}
	}
	else {
		*num_p = BAD_NUMBER;
	}
	return(string);
}


/* return YES if string passed in consists solely of a music symbol, otherwise
 * return NO */

int
is_music_symbol(str)

char *str;		/* which string to check */

{
	char *string;
	int font;
	int size;


	if (str == (char *) 0) {
		return(NO);
	}

	font = str[0];
	size = str[1];
	string = str + 2;

	/* has to be music char followed by null to be YES */
	if (next_str_char(&string, &font, &size) == '\0') {
		return(NO);
	}
	if ( ! IS_MUSIC_FONT(font)) {
		return(NO);
	}
	if (next_str_char(&string, &font, &size) == '\0') {
		return(YES);
	}
	return(NO);
}


/* The next few functions implement backspace processing */


/* Call this when ready to start processing a string, to be able to save
 * away information for baskspacing. */

void
bs_init(len)

int len;	/* How long the string to be processed is. It is okay if
		 * the result ends up longer than this; this is just used
		 * as the first guess, to make it unlikely we should
		 * need to realloc. */
{
	/* If had a backspace buffer already allocated from some
	 * previous call, but too short to reuse, then free the one we have. */
	if (BS_info_p != 0 && len > BS_allocated_len) {
		FREE(BS_info_p);
		BS_info_p = 0;
	}

	/* If don't already have a buffer, get one */
	if (BS_info_p == 0) {
		BS_allocated_len = len;
		MALLOC(BACKSPACEINFO, BS_info_p, BS_allocated_len);
	}

	/* Initialize values */
	BS_index = 0;
	BS_is_allowed = YES;
}


/* Call this when processing a character. This saves it away, so that if
 * we later need to backspace over it, we know what it was.
 * It also handles some special cases: if you pass it a STR_VERTICAL or
 * a newline, that tell us that we can no longer backspace in this string.
 */

void
bs_push(code, font)

int code;
int font;

{
	/* There are some special characters that tell us we need to
	 * disallow backspacing. */
	if ( ((code & 0xff) == STR_VERTICAL) || (code == '\n') ) {
		BS_is_allowed = NO;
		BS_index = 0;
		return;
	}

	/* If index would overflow, alloc some more.  Currently this should
	 * only happen when mapping a string when a replacement is longer
	 * than its pattern, so should be rare. */
	if (BS_index >= BS_allocated_len) {
		BS_allocated_len += 16;
		REALLOC(BACKSPACEINFO, BS_info_p, BS_allocated_len);
	}

	BS_info_p[BS_index].code = code;
	BS_info_p[BS_index].font = font;
	BS_index++;
}


/* Call this when a backspace is encountered to be processed. It returns
 * the appropriate value to place after the STR_BACKSPACE. If error_p is
 * non-null, what it points to will be set to YES if an error occurs.
 */

int
bs_pop(fname, linenum, error_p)

char *fname;	/* input file name, for error messages */
int linenum;	/* input line number, for error messages */
int *error_p;	/* If non-null, what is points to will be set to YES if
		 * an error occurs. */

{
	double backup_width;	/* backspace amount in inches */
	int backupval;		/* amount converted via BACKSP_FACTOR */

	if (BS_index == 0) {
		/* Can't back up past beginning of string or if we have
		 * encountered things that stop backspacing.*/
		if (BS_is_allowed == NO) {
			l_yyerror(fname, linenum,
				"can't backspace before newline or vertical motion");
		}
		else {
			l_yyerror(fname, linenum,
				"can't backspace before beginning of line");
		}
		if (error_p != 0) {
			*error_p = YES;
		}
		/* Return a safe value. Can't use zero, since that would
		 * terminate string. */
		return(1);
	}
	else {
		BS_index--;
		backup_width = width(BS_info_p[BS_index].font,
					DFLT_SIZE, BS_info_p[BS_index].code);
		backupval = (int) (backup_width * BACKSP_FACTOR);
		/* Clamp to legal values */
		if (backupval < 1) {
			backupval = 1;
		}
		else if (backupval >= MAX_CHARS_IN_FONT + FIRST_CHAR) {
			backupval = MAX_CHARS_IN_FONT + FIRST_CHAR;
		}
		return(backupval);
	}
}


/* return the ascent of a string in inches. This is the largest ascent of any
 * character in the string */

double
strascent(str)

char *str;	/* which string to process */

{
	float max_ascent, a;	/* tallest and current ascent */
	char *s;		/* to walk through string */
	int font, size, code;
	int textfont;
	double vertical, horizontal;
	float baseline_offset;	/* to account for vertical motion */
	int in_pile;
	int only_mus_sym;	/* YES if string consists solely
				 * of a music char */


	if (str == (char *) 0) {
		return(0.0);
	}

	only_mus_sym = is_music_symbol(str);

	/* first 2 bytes are font and size. */
	font = str[0];
	size = str[1];

	/* Walk through the string. */
	for (max_ascent = 0.0, baseline_offset = 0.0, s = str + 2;
			(code = nxt_str_char(&s, &font, &size, &textfont,
			&vertical, &horizontal, &in_pile, NO)) > 0;    ) {

		/* A newline goes to following line, so we probably won't
		 * get any higher ascent than we have so far, but if
		 * user gives enough vertical motion, we might, so continue. */
		if (code == '\n') {
			baseline_offset -= fontheight(font, size);
		}

		/* adjust for any vertical motion */
		if (vertical != 0.0) {
			baseline_offset += vertical;
		}

		/* music characters inside strings get moved up to the baseline,
		 * so use their height as ascent.
		 * Regular characters use the
		 * ascent of the character */
		if ((IS_MUSIC_FONT(font))  && (only_mus_sym == NO)) {
			a = height(font, size, code);
		}
		else {
			a = ascent(font, size, code);
		}
		a += baseline_offset;

		/* if tallest seen save this height */
		if (a > max_ascent) {
			max_ascent = a;
		}
	}

	/* if boxed, allow space for that */
	if (IS_BOXED(str) == YES) {
		max_ascent += 2.5 * STDPAD;
	}
	/* similarly, allow space for circle */
	if (IS_CIRCLED(str) == YES) {
		float ascent_adjust;
		max_ascent += circled_dimensions(str, (float *) 0, (float *) 0,
					&ascent_adjust, (float *) 0);
		max_ascent += ascent_adjust;
	}
	return(max_ascent);
}


/* return the descent of a string in inches. This is the largest descent of any
 * character in the string */

double
strdescent(str)

char *str;	/* which string to process */

{
	float max_descent, d;	/* largest and current descent */
	float line_descent;	/* descent caused by newlines */
	double vertical, horizontal;
	int in_pile;
	char *s;		/* to walk through string */
	int font, size, code;
	int textfont;
	int only_mus_sym;	/* YES if string consists solely
				 * of a music char */


	if (str == (char *) 0) {
		return(0.0);
	}

	only_mus_sym = is_music_symbol(str);

	/* first 2 bytes are font and size. */
	font = str[0];
	size = str[1];

	/* walk through the string. */
	for (max_descent = line_descent = 0.0, s = str + 2;
			(code = nxt_str_char(&s, &font, &size, &textfont,
			&vertical, &horizontal, &in_pile, NO)) > 0
			|| vertical != 0.0;  ) {

		/* Adjust for vertical motion. Since line_descent is
		 * measured downward and vertical is upward, have to
		 * substract the vertical, then adjust max_descent
		 * to compensate. */
		if (vertical != 0.0) {
			line_descent -= vertical;
			max_descent += vertical;
			if (code == 0) {
				/* motion only */
				continue;
			}
		}

		if (code == '\n') {
			/* at newline, descent goes down to next baseline,
			 * which will be down from current baseline
			 * by height of font */
			line_descent += fontheight(font, size);
			max_descent = 0.0;
			continue;
		}

		/* music characters inside strings get moved up to the
		 * baseline, so have no descent. */
		if ( ! (IS_MUSIC_FONT(font)) || (only_mus_sym == YES)) {
			d = descent(font, size, code);
		}
		else {
			d = 0.0;
		}

		/* if largest descent seen, save this descent */
		if (d > max_descent) {
			max_descent = d;
		}
	}

	/* if boxed, allow space for that */
	if (IS_BOXED(str) == YES) {
		max_descent += 3.5 * STDPAD;
	}
	/* similarly, allow space for circle */
	if (IS_CIRCLED(str) == YES) {
		max_descent += circled_dimensions(str, (float *) 0, (float *) 0,
						(float *) 0, (float *) 0);
	}
	return(max_descent + line_descent);
}


/* return the height of a string in inches. This is the maximum ascent plus the
 * maximum descent */

double
strheight(str)

char *str;		/* which string to process */
{
	/* Since letters may not
	 * align because of ascent/descent, we get the tallest extent
	 * by adding the largest ascent to the largest descent */
	return( strascent(str) + strdescent(str));
}


/* return the width of a string. This is the sum of the widths of the
 * individual characters in the string */

double
strwidth(str)
char *str;
{
	float tot_width;
	float widest_line;	/* for multi-line strings */
	float curr_width;
	double horizontal, vertical;
	int was_in_pile;	/* if in pile last time through loop */
	int in_pile_now;	/* if current character is inside a pile */
	char *s;		/* to walk through string */
	int font, size, code;
	int textfont;


	if (str == (char *) 0) {
		return(0.0);
	}

	/* first 2 bytes are font and size. */
	font = str[0];
	size = str[1];

 	/* walk through string */
	was_in_pile = NO;
	for (curr_width = tot_width = widest_line = 0.0, s = str + 2;
			(code = nxt_str_char(&s, &font, &size, &textfont,
			&vertical, &horizontal, &in_pile_now, NO)) > 0;
			was_in_pile = in_pile_now) {

		/* Piles are handled specially. As soon as we enter a pile,
		 * we call the function to get its entire width. Then for
		 * the rest of the pile, we just skip past everything */
		if (in_pile_now == YES) {
			if (was_in_pile == NO) {
				curr_width += pile_width();
				if (curr_width > tot_width) {
					tot_width = curr_width;
				}
			}
			continue;
		}

		/* the horizontal movement coming out of a pile doesn't count,
		 * since it was included in the pile, otherwise it does */
		if (was_in_pile == NO) {
			curr_width += horizontal;
		}
		if (curr_width > tot_width) {
			tot_width = curr_width;
		}

		if (code == '\n') {
			/* keep track of width line of multi-line string */
			if (tot_width > widest_line) {
				widest_line = tot_width;
			}
			tot_width = 0.0;
			curr_width = 0.0;
			continue;
		}

		if (code == '\b') {
			/* backspace */
			tot_width -= backsp_width(size);
			curr_width -= backsp_width(size);
			continue;
		}

		/* If we have the special "page number" character,
		 * or special "total number of pages" character,
		 * we deal with that here. */
		if ( (code == '%' || code == '#') && (s > str + 3)
					&& ( (*(s-2) & 0xff) == STR_PAGENUM
					|| (*(s-2) & 0xff) == STR_NUMPAGES) ) {

			char pgnumbuff[8], *pgnum_p;

			/* convert page number to a string and
			 * add the width of each character in
			 * that string. */
			(void) sprintf(pgnumbuff, "%d",
					code == '%' ? Pagenum : Last_pagenum);

			for ( pgnum_p = pgnumbuff; *pgnum_p != '\0';
								pgnum_p++) {
				curr_width += width(font, size, *pgnum_p);
			}
		}

		else {
			/* Oh good. This is a normal case. Just add
			 * width of this character to width so far */
			curr_width += width(font, size, code);
		}

		if (curr_width > tot_width) {
			tot_width = curr_width;
		}
	}
	if (tot_width < widest_line) {
		tot_width = widest_line;
	}
	/* if string is boxed, allow space for the box */
	if (IS_BOXED(str) == YES) {
		tot_width += 6.0 * STDPAD;
	}
	/* similarly, allow space for circled */
	if (IS_CIRCLED(str) == YES) {
		(void) circled_dimensions(str, (float *) 0, &tot_width,
						(float *) 0, (float *) 0);
	}
	return(tot_width);
}


/* If the last character of the given string is a space,
 * returns its width, else returns 0.0 */

double
endspace_width(str)

char * str;

{
	int font;
	int size;
	int ch;
	int last_was_space = NO;

	if (str == 0) {
		return(0.0);
	}

	/* Walk through the string to the last character.
	 * If it is a space, return its width. */
	font = str[0];
	size = str[1];
	str += 2;

	while ((ch = next_str_char(&str, &font, &size)) > 0) {
		last_was_space = (ch == ' ' && IS_STD_FONT(font) ? YES : NO);
	}
	return(last_was_space == YES ? width(font, size, ' ') : 0.0);
}


/* Return YES if given string contain alignment point \| or \^ that
 * is not inside a pile. */

int
has_align_point(str)

char *str;

{
	int in_pile = NO;	/* this tracks pile toggle state */

	for (str += 2; *str != '\0'; str++) {
		switch(*str & 0xff) {
		case STR_PILE:
			in_pile = (in_pile == YES ? NO : YES);
			break;
		case STR_C_ALIGN:
		case STR_L_ALIGN:
			if (in_pile == NO) {
				return(YES);
			}
			break;
		default:
			break;
		}
	}
	return(NO);
}


/* Return the width to the "anchor" point of a string. For most strings,
 * this will be half the width of the first character. But for a string
 * that begins with things piled atop one another, it is the alignment point.
 * And for boxed or circled strings, the box or circle must be considered.
 */

double
left_width(string)

char *string;

{
	return(lwidth_common(string, YES));
}

/* This is a variation that is used for "center" statements.  For STUFFs,
 * if the string starts with a pile, we align on the pile, because it might
 * be something like figured bass. So that's what the original left_width did.
 * But when we added support for "center" statements having an alignment
 * point, it doesn't seem like it makes sense to have an initial pile
 * cause alignment. For one thing, it a user used to have a string starting
 * with a pile, they wouldn't want it suddenly centered on that. They really
 * would most likely want to align the entire string. So this variation
 * only considers non-pile aligment.
 */

double
center_left_width(string)

char * string;

{
	return(lwidth_common(string, NO));
}


/* Now the common function that the two variation above call */

static double
lwidth_common(string, consider_start_pile)

char *string;
int consider_start_pile;	/* If YES, consider initial piles */

{
	int font;
	int size;
	char *pile_start_p;	/* where pile begins, if any */
	char *str;		/* walk string looking for alignment */
	char *s;		/* for finding char after \^ */
	double wid;		/* when there is alignment point */
	int in_pile = NO;	/* tracks current toggle state */
	int ch;
	float extra;		/* space for box or circle, if any */


	/* For boxed or circled strings,
	 * the space for the box or circle must be added in */
	if (IS_BOXED(string) == YES) {
		extra = 3.5 * STDPAD;
	}
	else if (IS_CIRCLED(string) == YES) {
		(void) circled_dimensions(string, (float *) 0,
				(float *) 0, (float *) 0, &extra);
	}
	else {
		extra = 0.0;
	}

	/* If there is an alignment point, use left of that */
	font = string[0];
	size = string[1];
	for (str = string + 2; *str != '\0'; str++) {
		if (IS_STR_COMMAND(*str) == YES) {
			switch (*str & 0xff) {

			/* thing with one byte argument */
			case STR_FONT:
			case STR_SIZE:
			case STR_PAGENUM:
			case STR_NUMPAGES:
			case STR_BACKSPACE:
			case STR_VERTICAL:
			case STR_KEYMAP:
				str++;
				break;

			/* things with 2 argument bytes */
			case STR_MUS_CHAR:
			case STR_MUS_CHAR2:
			case STR_USERDEF1:
				str += 2;
				break;

			case STR_PILE:
				in_pile = (in_pile == YES ? NO : YES);
				break;
			case STR_C_ALIGN:
				if (in_pile == YES) {
					/* This one doesn't count */
					break;
				}

				/* walk through string to
				 * get the next character */
				font = string[0];
				size = string[1];
				s = string+2; 
				while ((ch = next_str_char(&s, &font, &size)) != '\0') {
					if (s == str) {
						/* got to where we were
						 * in the more simple
						 * way through the str,
						 * so we want the char
						 * right after this. */
						ch = next_str_char(&s, &font, &size);
						if (ch == '\0') {

							/* This is probably impossible,
							 * since we pad strings with a space,
							 * and if it is possible,
							 * is really a user error,
							 * center with no following char.
							 * We'll figure half the width
							 * of a non-existent character
							 * is nothing. */
							break;
						}
						/* Add half the width
						 * of this char */
						extra += width(font, size, ch) / 2.0;
						break;
					}
				}

				*str =  '\0';
				wid = strwidth(string);
				*str = STR_C_ALIGN;
				return(wid + extra);
				break;
			case STR_L_ALIGN:
				if (in_pile == YES) {
					/* This one doesn't count */
					break;
				}

				*str =  '\0';
				wid = strwidth(string);
				*str = STR_L_ALIGN;
				return(wid + extra);
				break;

			/* others have no argument bytes to deal with */
			default:
				break;
			}
		}
	}

	/* If we are here, we did not find a non-pile alignment,
	 * so if we are supposed to consider initial pile, do that now */
	if (consider_start_pile == YES &&
			starts_piled(string, &font, &size, &pile_start_p) == YES) {
		return(align_distance(pile_start_p, font, size));
	}

	/* No special aligment cases were found,
	 * so use half the width of the first character in the string. */
	font = *string++;
	size = *string++;
	ch = next_str_char(&string, &font, &size);
	return(width(font, size, ch) / 2.0 + extra);
}


/* If string begins with piled text, return YES, otherwise NO,
 * If YES, also return via pointers the start of the pile and the
 * font and size at that point. */

static int
starts_piled(string, font_p, size_p, pile_start_p_p)

char *string;
int *font_p;
int *size_p;
char **pile_start_p_p;

{
	*font_p = *string++;
	*size_p = *string++;

	/* walk through string, skipping any leading box/size/font */
	for (  ; *string != '\0'; string++) {
		if (IS_STR_COMMAND(*string)) {
			switch(*string & 0xff) {

			case STR_FONT:
				*font_p = *(++string);
				break;

			case STR_SIZE:
				*size_p = *(++string);
				break;

			case STR_BOX:
			case STR_CIR:
				break;

			case STR_PILE:
				/* The first thing we found that was not to be
				 * ignored is the beginning of a pile */
				*pile_start_p_p = string;
				return(YES);
		
			default:
				return(NO);
			}
		}
		else {
			break;
		}
	}
	return(NO);
}


/* given a string representing a chord mark, transpose it. For each letter
 * 'A' to 'G' optionally followed by an accidental, call function to
 * get transposed value. Build new string with transposed values. Free up
 * the old string and return the new one. Also, if the accidental was
 * of the form &, #, x, or && instead of \(smflat) etc, change to proper
 * music symbol. Also handles translation of o, o/ and ^ to dim, halfdim,
 * and triangle symbols, and does translation of unescaped accidentals. */

char *
tranchstr(chordstring, staffno)

char *chordstring;	/* untransposed string */
int staffno;		/* which staff it is associated with */
			/* A staffno of -1 means no transpose, just translate */

{
	char tmpbuff[128];	/* temporary copy of transposed string */
	char replacement[4];	/* for dim/halfdim/triangle */
	short i;		/* index into tmpbuff */
	unsigned char *str;	/* walk through chordstring */
	char *transposed;	/* new version of letter[accidental] */
	char tranbuff[4];	/* to point 'transposed' at if not really
				 * transposing */
	char letter;		/* A to G */
	char accidental;
	int escaped;		/* YES if accidental was escaped */
	int chordtranslation;	/* none, do/re/mi, or german */
	char literal_accidental;	/* what would normally be translated */
	int nprocessed;		/* how many characters processed by subroutine */
	char *newstring;	/* final copy of transposed string */
	int n;
	int size;
	int font;
	int in_pile;		/* YES if inside a pile */
	int acc_size;		/* size for accidentals */


	/* get font/size info */
	tmpbuff[0] = chordstring[0];
	tmpbuff[1] = chordstring[1];
	font = chordstring[0];
	size = chordstring[1];
	in_pile = NO;
	str = (unsigned char *) (chordstring + 2);
	literal_accidental = '\0';  /* avoids bogus "used before set" warning */

	/* walk through original string */
	for (i = 2; *str != '\0'; str++) {

		/* Be safe. Bail out a little before we reach end,
		 * because some things take several bytes,
		 * and it's easiest to just check once per loop. */
		if (i > sizeof(tmpbuff) - 8) {
			ufatal("chord string too long: '%s'", chordstring + 2);
		}

		acc_size = (in_pile == YES ? size : accsize(size));

		/* If a STR_*, deal with that */
		if ((n = str_cmd((char *) str, &size, &font, &in_pile)) > 0) {
			strncpy(tmpbuff + i, (char *) str, (unsigned) n);
			i += n;
			str += n - 1;
		}

		/* handle backslashed o and ^ */
		else if (*str == '\\' && ( *(str+1) == 'o' || *(str+1) == '^' ) ) {
			str++;
			tmpbuff[i++] = *str;
		}

		/* Transpose A-G anywhere.
		 * Transpose a-g only if first character of the string. */
		else if ( IS_STD_FONT(font) &&
				( (*str >= 'A' && *str <= 'G') ||
				(i == 2 && *str >= 'a' && * str <= 'g') ) ) {

			/* Aha! Something to transpose. */
			letter = *str;

			str += get_accidental( (unsigned char *) (str + 1),
					&accidental, &acc_size, NO, &escaped);
			if (escaped == YES) {
				/* not *really* an accidental, so save to
				 * print later. */
				literal_accidental = accidental;
				accidental = '\0';
			}
			if (staffno == -1) {
				/* not to be transposed, so make a string
				 * that would be like what tranchnote() would
				 * return, but with no transposition. */
				tranbuff[0] = letter;
				tranbuff[1] = accidental;
				tranbuff[2] = '\0';
				transposed = tranbuff;
			}
			else {
				/* get the transposed value */
				transposed = tranchnote(letter, accidental, staffno);
			}

			chordtranslation = svpath(staffno, CHORDTRANSLATION)
						->chordtranslation;
			if (chordtranslation == CT_NONE) {
				/* put transposed letter into output */
				tmpbuff[i++] = *transposed;
			}
			else if (chordtranslation == CT_DOREMI) {
				int trans_index;	/* into deremi_syls[] */
				char *replacement;	/* e.g. "do" for "C" */
				int prevsize;
				int prevfont;

				/* Map letter to do/re/mi style name */
				trans_index = (int)(toupper(*transposed)) - (int) 'A';
				if (trans_index < 0 || trans_index >= 7) {
					pfatal("invalid pitch letter '%c' returned for do/re/mi translation", *transposed);
				}
				/* compensate for array order starting at C */
				trans_index = (trans_index + 5) % 7;
				/* Look up the replacement syllable */
				replacement = svpath(staffno, CHORDTRANSLATION)->doremi_syls[trans_index];
				/* copy, skipping the font/size bytes */
				prevfont = font;
				prevsize = size;
				for (replacement += 2; *replacement != '\0';
							replacement++) {
					/* Reserve 6 in case we need to reset
					 * size/font, plus a couple to spare */
					if (i > sizeof(tmpbuff) - 6) {
						ufatal("chord string too long after translation: '%s'", chordstring + 2);
					}
					if ((n = str_cmd((char *) replacement, &size, &font, &in_pile)) > 0) {
						strncpy(tmpbuff + i, (char *) replacement, (unsigned) n);
						i += n;
						replacement += n - 1;
					}
					else {
						tmpbuff[i++] = *replacement;
					}
				}
				if (font != prevfont) {
					tmpbuff[i++] = STR_FONT;
					tmpbuff[i++] = prevfont;
				}
				if (size != prevsize) {
					tmpbuff[i++] = STR_SIZE;
					tmpbuff[i++] = prevsize;
				}
			}
			else if (chordtranslation == CT_GERMAN) {
				int translated;

				translated = *transposed;
				if ( toupper(translated) == 'B') {
					switch (accidental) {
					case '\0':
					case '#':
					case 'x':
						/* replace with H */
						translated = (isupper(*transposed)
							? 'H' : 'h');
						break;
					case '&':
						/* B& becomes just B */
						transposed[1] = '\0';
						break;
					case 'B':
						/* Change B&& to B& */
						transposed[1] = '&';
						transposed[2] = '\0';
						break;
					}
				}
				tmpbuff[i++] = translated;
			}
			else {
				pfatal("Invalid chordtranslation value %d",
						chordtranslation);
			}
			/* now add accidental if any */
			i += add_accidental(tmpbuff + i, (int) *++transposed,
							acc_size, NO);

			/* add on any escaped pseudo-accidental */
			if (escaped == YES) {
				i += add_accidental(tmpbuff + i,
					(int) literal_accidental,
					acc_size, YES);
				escaped = NO;
			}

			/* handle dim/halfdim/triangle transformations */
			if ((n = dim_tri(str + 1, replacement, size, YES)) > 0) {
				strcpy(tmpbuff + i, replacement);
				i += strlen(replacement);
				str += n;
			}
		}
		else {
			/* Originally we only translated things like # and &
			 * in chords to musical accidental symbols if they
			 * immediately followed a letter A-G. But due to
			 * popular demand, they are now translated everywhere,
			 * unless escaped. */
			nprocessed = get_accidental( (unsigned char *) str,
					&accidental, &acc_size, NO, &escaped);
			if (nprocessed > 0) {
				i += add_accidental(tmpbuff + i,
					(int) accidental,
					acc_size, escaped);
				/* the -1 is because str will get incremented
				* at the top of the 'for' */
				str += nprocessed - 1;
			}
			else {
				/* something boring. Just copy */
				tmpbuff[i++] = *str;
			}
		}
	}

	/* need to make permanent copy of new string */
	tmpbuff[i++] = '\0';
	MALLOCA(char, newstring, i);
	(void) memcpy(newstring, tmpbuff, (unsigned) i);

	/* free original version */
	FREE(chordstring);

	/* return new, transposed version */
	return(newstring);
}


/* If there is a STR_* command in chord/analysis/figbass, return how
 * many characters long it is. Also update the size or font if the
 * command was one to change those, and update pile status if necessary. */

static int
str_cmd(str, size_p, font_p, in_pile_p)

char *str;	/* check string starting here */
int *size_p;
int *font_p;
int *in_pile_p;	/* YES if in pile, may be updated */

{
	if (IS_STR_COMMAND(*str)) {
		switch(*str & 0xff) {

		case STR_SIZE:
			/* update size */
			*size_p = *(str + 1);
			/* command plus 1 argument byte */
			return(2);

		case STR_FONT:
			/* update font */
			*font_p = *(str + 1);
			return(2);
		case STR_PAGENUM:
		case STR_NUMPAGES:
		case STR_BACKSPACE:
		case STR_VERTICAL:
		case STR_KEYMAP:
			/* command plus 1 argument byte */
			return(2);

		case STR_MUS_CHAR:
		case STR_MUS_CHAR2:
		case STR_USERDEF1:
			/* command plus 2 argument bytes */
			return(3);

		case STR_PILE:
			/* entering/leaving a pile alters the size */
			*size_p = pile_size(*size_p, *in_pile_p);
			*in_pile_p = (*in_pile_p ? NO : YES);
			break;

		default:
			/* others have no argument bytes */
			return(1);
		}
	}
	return(0);
}


/* Check the first character of the given string to see if it is an accidental
 * or something that should be translated to an accidental (# & x && and
 * maybe n). If so, fill in the accidental. If the accidental was specified
 * via a STR_MUS_CHAR, also update the accidental size.
 * If no accidental, accidental_p will will filled in
 * with '\0'. In any case return how many bytes were processed.
 */

static int
get_accidental(string, accidental_p, acc_size_p, trans_natural, escaped_p)

unsigned char *string;	/* check this for an accidental */
char *accidental_p;	/* return the accidental here, or \0 if none */
int *acc_size_p;	/* return the accidental size here */
int trans_natural;	/* if YES, translate n to natural, else leave as n */
int *escaped_p;		/* Return value: YES if the symbol was backslashed */

{
	unsigned char *str_p;

	str_p = string;
 
	/* assume no accidental */
	*accidental_p = '\0';

	/* check if escaped */
	if (*str_p == '\\') {
		*escaped_p = YES;
		str_p++;
	}
	else {
		*escaped_p = NO;
	}

	/* See if the following character is an accidental */
	switch (*str_p) {

	case '#':
	case 'x':
		*accidental_p = *str_p++;
		break;
	case '&':
		*accidental_p = *str_p++;
		/* have to peek ahead to check for double flat,
		 * but not if escaped, so person can get a literal
		 * ampersand followed by a flat. */
		if (*escaped_p == NO && *str_p == '&') {
			/* double flat is 'B' internally */
			*accidental_p = 'B';
			str_p++;
		}
		break;

	case 'n':
		/* naturals are not translated in chords, but are
		 * in analysis and figbass */
		if (trans_natural == YES) {
			*accidental_p = *str_p++;
		}
		break;

	case STR_MUS_CHAR:
		if (*escaped_p == YES) {
			break;
		}
		/* Check if user put in \(flat) or something
		 * similar. If so, use that. */
		switch (*(str_p + 2)) {
		case C_FLAT:
			*acc_size_p = *(str_p + 1);
			*accidental_p = '&';
			str_p += 3;
			break;

		case C_SHARP:
			*acc_size_p = *(str_p + 1);
			*accidental_p = '#';
			str_p += 3;
			break;

		case C_DBLFLAT:
			*acc_size_p = *(str_p + 1);
			*accidental_p = 'B';
			str_p += 3;
			break;

		case C_DBLSHARP:
			*acc_size_p = *(str_p + 1);
			*accidental_p = 'x';
			str_p += 3;
			break;

		case C_NAT:
			/* Always translate the natural symbol,
			 * even when trans_natural is NO. That really
			 * applies just to the use of 'n' which is
			 * likely to be wanted as a real n, whereas
			 * a music symbol natural is unambiguous. */
			*acc_size_p = *(str_p + 1);
			*accidental_p = 'n';
			str_p += 3;
			break;

		default:
			/* false alarm. Some other
			 * music character. */
			break;
		}
		break;

	default:
		/* nothing special */
		break;
	}

	/* If all we saw was a backslash,
	 * then there wasn't really an accidental */
	if (*escaped_p == YES && str_p == string + 1) {
		*escaped_p = NO;
		str_p = string;
	}

	return(str_p - string);
}


/* Write the given accidental in the given size to the given string.
 * Return how many bytes were added. */

static int
add_accidental(buff, acc_character, acc_size, escaped)

char *buff;		/* write into this buffer */
int acc_character;	/* write this accidental */
int acc_size;		/* make accidental this big */
int escaped;		/* if YES, was escaped, so not really an accidental;
			 * print it as a normal character */

{
	if (acc_character != '\0') {

		/* if escaped, just treat like normal character. */
		if (escaped == YES) {
			buff[0] = acc_character;
			return(1);
		}

		/* sharps and naturals are tall enough that they can
		 * make things not line up, so move them down some */
		if (acc_character == '#' || acc_character == 'n') {
			buff[0] = (char) STR_VERTICAL;
			buff[1] = (char) ENCODE_VERT(-4);
			buff += 2;
		}
		else if (acc_character == 'x') {
			buff[0] = (char) STR_VERTICAL;
			buff[1] = (char) ENCODE_VERT(4);
			buff += 2;
		}
		/* has accidental. Add STR_MUS_CHAR-size-code */
		buff[0] = (char) STR_MUS_CHAR;

		/* double sharp is special. It is too small,
		 * so make it bigger */
		if (acc_character == 'x') {
			acc_size = (int) ( (float) acc_size
							* 1.25);
		}
		buff[1] = (char) acc_size;

		/* use accidental of appropriate type */
		switch (acc_character) {

		case '#':
			buff[2] = C_SHARP;
			break;

		case '&':
			buff[2] = C_FLAT;
			break;

		case 'x':
			buff[2] = C_DBLSHARP;
			break;

		case 'B':
			buff[2] = C_DBLFLAT;
			break;

		case 'n':
			buff[2] = C_NAT;
			break;

		default:
			pfatal("illegal accidental on transposed chord");
			break;
		}
		if (acc_character == '#' || acc_character == 'n') {
			buff[3] = (char) STR_VERTICAL;
			buff[4] = (char) ENCODE_VERT(4);
			/* We added 3 bytes for the accidental, plus
			 * 2 bytes before and after for vertical motion. */
			return(7);
		}
		else if (acc_character == 'x') {
			buff[3] = (char) STR_VERTICAL;
			buff[4] = (char) ENCODE_VERT(-4);
			return(7);
		}
		else {
			return(3);	/* we added 3 bytes */
		}
	}

	return (0);
}


/* In chords and such, "o" becomes \(dim), "o/" becomes \(halfdim)
 * unless followed by [A-G] in which case it becomes "\(dim)/",
 * and "^" becomes \(triangle). Return number of characters processed.
 */

static int
dim_tri(str_p, replacement, size, is_chord)

unsigned char *str_p;		/* check string at this point */
char *replacement;		/* return the replacement in this buffer,
				 * which needs to be at least 4 bytes long */
int size;			/* use this size for music character */
int is_chord;			/* YES for chord, NO for analysis/figbass */

{
	if (*str_p == '^') {
		replacement[0] = (char) STR_MUS_CHAR;
		replacement[1] = size;
		replacement[2] = C_TRIANGLE;
		replacement[3] = '\0';
		return(1);
	}
	else if (*str_p == 'o') {
		replacement[0] = (char) STR_MUS_CHAR;
		replacement[1] = size;
		replacement[3] = '\0';
		if ( *(str_p+1) == '/' && (is_chord == NO ||
				(*(str_p+2) < 'A' || *(str_p+2) > 'G'))) {
			replacement[2] = C_HALFDIM;
			return(2);
		}
		else {
			replacement[2] = C_DIM;
			return(1);
		}
	}
	return(0);
}


/* Given a string for analysis or figbass, transform the accidentals
 * & # && x n to their music characters.
 */

char *
acc_trans(string)

char *string;

{
	char buffer[128];	/* output buffer for transformed string */
	char *out_p;		/* current location in output buffer */
	char replacement[4];	/* space for dim, halfdim, etc */
	int n;
	int size, acc_size;
	int font;
	char accidental;	/* #, &, x, etc */
	int escaped;		/* YES is accidental was escaped */
	int in_pile;		/* YES if inside a pile */


	buffer[0] = string[0];
	buffer[1] = string[1];
	font = string[0];
	size = string[1];
	in_pile = NO;

	/* walk through string, transforming any accidentals along the way */
	for ( string += 2, out_p = buffer + 2; *string != '\0'; ) {
		/* Be safe. Bail out a little before we reach end,
		 * because some things take several bytes,
		 * and it's easiest to just check once per loop. */
		if (out_p - buffer > sizeof(buffer) - 8) {
			l_ufatal(Curr_filename, yylineno,
				"analysis or figbass string too long");
		}

		acc_size = (in_pile == YES ? size : accsize(size));
		if ((n = get_accidental((unsigned char *) string,
				&accidental, &acc_size, YES, &escaped)) > 0 ) {
			out_p += add_accidental(out_p, (int) accidental,
						acc_size, escaped);
			string += n;
		}
		else if (*string == '\\' && ( *(string+1) == 'o' || *(string+1) == '^') ) {
			*out_p++ = *++string;
			string++;
		}
		else if ((n = dim_tri((unsigned char *) string, replacement,
							size, NO)) > 0) {
			strcpy(out_p, replacement);
			out_p += strlen(replacement);
			string += n;
		}
		else if ((n = str_cmd(string, &size, &font, &in_pile)) > 0) {
			strncpy(out_p, string, (unsigned) n);
			out_p += n;
			string += n;
		}
		else {
			*out_p++ = *string++;
		}
	}
	*out_p = '\0';

	return(copy_string(buffer + 2, buffer[0], buffer[1]));
}

/* Given a chord, analysis or figbass string,
 * transform according to their special rules:
 *	- : gets translated to \: and vice-versa
 *	- figbass starts in piled mode
 *	- in figbass, a / gets translated to \/ and vice-versa
 * This string will be in half transformed state: the first 2 bytes
 * are font/size, but the rest is still all ASCII, not internal format.
 */

char *
modify_chstr(string, modifier)

char *string;
int modifier;

{
	int length;	/* of modified string */
	char *s;	/* walk through string */
	char *newstring;
	char *new_p;	/* walk through newstring */
	int need_new;	/* if we need to make a new string */


	length = strlen(string);
	if (modifier == TM_FIGBASS) {
		/* We'll need two extra bytes for
		 * the leading \: for pile mode. */
		length += 2;
		need_new = YES;
	}
	else {
	 	/* Only need a new string if the original has colons,
		 * so assume for now we won't need a new string */
		need_new = NO;
	}

	/* Figure out how much space we'll need for the modified string.
	 * Any unbackslashed colons will take up an extra byte once
	 * we backslash it. But any backslashed one will take up one
	 * less when we unescape it. Similar for slashes in figbass. */
	for (s = string + 2; *s != '\0'; s++) {
		if (*s == ':') {
			length++;
			need_new = YES;
		}
		else if (modifier == TM_FIGBASS && *s == '/') {
			/* o/ means half diminished so that doesn't count */
			if (s > string + 2 && *(s-1) == 'o') {
				continue;
			}
			length++;
			need_new = YES;
		}
		else if (*s == '\\') {
			s++;
			/* things that occur inside \(...) don't count */
			if (*s == '(') {
				for (s++; *s != '\0' && *s != ')'; s++) {
					;
				}
				/* If no closing parenthesis, return as is;
				 * later code will catch that */
				if (*s == '\0') {
					return(string);
				}
			}
			else if (*s == ':') {
				length--;
				need_new = YES;
			}
			else if (modifier == TM_FIGBASS && *s == '/') {
				length--;
				need_new = YES;
			}
		}
	}

	/* If string is okay as is, we are done here */
	if (need_new == NO) {
		return(string);
	}

	/* get enough space for new string */
	MALLOCA(char, newstring, length + 1);

	/* copy font/size */
	newstring[0] = string[0];
	newstring[1] = string[1];

	new_p = newstring + 2;
	s = string + 2;
	if (modifier == TM_FIGBASS) {
		/* add \: but after box, if any */
		if (string[2] == '\\' && string[3] == '[') {
			*new_p++ = *s++;
			*new_p++ = *s++;
		}
		*new_p++ = '\\';
		*new_p++ = ':';
	}

	/* walk through rest of string, copying, but transforming
	 * any slashes and colons along the way */
	for (  ; *s != '\0'; s++, new_p++) {

		/* handle colons */
		if (*s == ':') {
			/* add a backslash */
			*new_p++ = '\\';
		}
		else if (*s == '\\' && *(s+1) == ':') {
			/* skip past the backslash */
			s++;
		}

		/* handle slashes in figbass */
		else if (modifier == TM_FIGBASS) {
			if (*s == '/') {
				/* o/ means half diminished
				 * so that doesn't count */
				if (s <= string + 2 || *(s-1) != 'o') {
					/* add a backslash */
					*new_p++ = '\\';
				}
			}
			else if (*s == '\\' && *(s+1) == '/') {
				/* skip past the backslash */
				s++;
			}
		}

		/* copy from original string to new one */
		*new_p = *s;
	}

	/* original is now no longer needed */
	FREE(string);

	/* terminate and return the modified string */
	*new_p = '\0';
	return(newstring);
}


/* given an integer point size, return the integer point size appropriate
 * for a "small" version. This is SM_FACTOR times the size, rounded, but
 * not less than 1. */

static int
smallsize(size)

int size;

{
	size = (int) ( (float) size * SM_FACTOR);
	if (size < 1) {
		size = 1;
	}
	return(size);
}


/* accidentals in chords need to be scaled. Given a size, return the size
 * that an accidental should be. This is 60% of given size, rounded to
 * an integer, but no smaller than 1. */

static int
accsize(size)

int size;

{
	size = (int) ( (float) size * 0.6);
	if (size < 1) {
		size = 1;
	}
	return(size);
}


/* return which character to use for rest, based on basictime */

int
restchar(basictime)

int basictime;

{
	if (basictime == BT_DBL) {
		/* double whole rest */
		return (C_DWHREST);
	}
	else if (basictime == BT_QUAD) {
		/* quad rest */
		return (C_QWHREST);
	}
	else if (basictime == BT_OCT) {
		/* quad rest */
		return (C_OWHREST);
	}


	else {
		/* other non-multirest */
		return (Resttab [ drmo(basictime) ] );
	}
}


/* return YES if given font is an italic font (includes boldital too) */

int
is_ital_font(font)

int font;

{
	return(Fontinfo[ font_index(font) ].is_ital);
}


/* return YES if given font is a bold font (includes boldital too) */

int
is_bold_font(font)

int font;

{
	return(Fontinfo[ font_index(font) ].is_bold);
}



/* given a string, return, via pointers the font and size in effect at the
 * end of the string */

void
end_fontsize(str, font_p, size_p)

char *str;		/* check this string */
int *font_p;		/* return font at end of str via this pointer */
int *size_p;		/* return size at end of str via this pointer */

{
	if (str == (char *) 0) {
		/* empty string, use defaults */
		*font_p = FONT_TR;
		*size_p = DFLT_SIZE;
		return;
	}

	/* find the font/size in effect at end of given string */
	*font_p = *str++;
	*size_p = *str++;
	while (next_str_char(&str, font_p, size_p) != '\0') {
		;
	}
}


/* given a string, return a string made up of a dash in the font and size
 * of the end of the given string. However, if the string ends with a ~ or _
 * return a string containing that instead */

char *
dashstr(str)

char *str;	/* return dash with same font/size as end of this string */

{
	int font, size;
	char *newstring;
	int ch;		/* character to use */


	end_fontsize(str, &font, &size);
	ch = last_char(str);
	if (ch != '~' && ch != '_') {
		ch = '-';
	}

	/* allocate space for dash string and fill it in */
	MALLOCA(char, newstring, 4);
	newstring[0] = (char) font;
	newstring[1] = (char) size;
	newstring[2] = (char) ch;
	newstring[3] = '\0';
	return(newstring);
}


/* Given an internal format string, create an ASCII-only string. Flags
 * tell how complete a conversion to do. If verbose is YES, try to convert
 * everything back to user's original input, otherwise ignore special things
 * other than music characters, extended characters, and backspace.
 * If pagenum is YES, interpolate the current page number rather than using %.
 *
 * Recreating the original user string is not perfect, but is usually right.
 * Where there are shortcuts, we can't tell if user used them or not.
 * Extended characters are output by name even if user put them in as single
 * Latin-1 characters. But we couldn't use the Latin-1 hyper-ASCII in midi
 * anyway, because they have high bit set.
 *
 * Returns the ASCII-ized string, which is stored in an area that will get
 * overwritten on subsequent calls, so if caller needs a permanent copy,
 * they have to make it themselves.
 */

/* This is how much to malloc at a time to hold the ASCII-ized string */
#define ASCII_BSIZE	512

char *
ascii_str(string, verbose, pagenum, textmod)

char *string;	/* internal format string to convert */
int verbose;	/* If YES, try to reproduce user's original input */
int pagenum;	/* YES (interpolate number for \%) or NO (leave \% as is) */
int textmod;	/* TM_ value */

{
	static char *buff = 0;		/* for ASCII-ized string */
	static unsigned buff_length = 0;/* how much is malloc-ed */
	int i;				/* index into ASCII-ized string */
	char *musname;			/* music character name */
	int in_pile = NO;
	char *str;			/* walk through string */
	int musfont;			/* FONT_MUSIC*    */


	/* first time, get some space */
	if (buff_length == 0) {
		buff_length = ASCII_BSIZE;
		MALLOCA(char, buff, buff_length);
	}

	/* walk through string */
	i = 0;
	/* special case: normally we implicitly begin a figbass with a
	 * pile start, but if users cancels that, it won't be there */
	if (textmod == TM_FIGBASS &&
			(((unsigned char) *(string+2)) & 0xff) != STR_PILE) {
		buff[i++] = ':';
	}
	for (str = string + 2;  *str != '\0'; str++) {
		switch ( ((unsigned char) *str) & 0xff) {

		case STR_FONT:
			str++;
			if ( ! IS_STD_FONT((int) *str) ) {
				/* translate to Mup name */
				(void) sprintf(buff + i, "\\(%s)",
					get_charname((int)*(str+1), (int)*str));
				str++;
				while (buff[i] != '\0') {
					i++;
				}
				/* skip past the return to original font */
				str += 2;
			}
			else if (verbose == YES) {
				(void) sprintf(buff + i, "\\f(%s)",
						fontnum2name((int) *str));
				while (buff[i] != '\0') {
					i++;
				}
			}
			break;

		case STR_SIZE:
			str++;
			if (verbose == YES) {
				(void) sprintf(buff + i, "\\s(%d)", (int) *str);
				while (buff[i] != '\0') {
					i++;
				}
			}
			break;

		case STR_VERTICAL:
			str++;
			if (verbose == YES) {
				(void) sprintf(buff + i, "\\v(%d)",
						DECODE_VERT((int) *str) * 100
						/ MAXVERTICAL);
				while (buff[i] != '\0') {
					i++;
				}
			}
			break;
		
		case STR_MUS_CHAR:
		case STR_MUS_CHAR2:
		case STR_USERDEF1:
			musfont = str2mfont( ((unsigned char) *str) & 0xff);

			/* skip past the size byte,
			 * and on to the character code. */
			str += 2;
			/* In chordlike stuffs, we translate things like
			 * # and &&, so translate them back. It's possible
			 * the user used the names explicitly rather than us
			 * translating, in which case this won't be
			 * strictly what they put in, but it will be
			 * consistent, so that a caller of this function
			 * can easily sort or compare values
			 * without having to know (for example)
			 * that '#' and \(smsharp) are the same thing.  */
			musname = 0;
			if (IS_CHORDLIKE(textmod) == YES
						&& musfont == FONT_MUSIC) {
				switch( ((unsigned char) *str) & 0xff) {
				case C_SHARP:
					musname = "#";
					break;
				case C_FLAT:
					musname = "&";
					break;
				case C_DBLSHARP:
					musname = "x";
					break;
				case C_DBLFLAT:
					musname = "&&";
					break;
				case C_NAT:
					if (textmod != TM_CHORD) {
						musname = "n";
					}
					break;
				case C_DIM:
					musname = "o";
					break;
				case C_HALFDIM:
					musname = "o/";
					break;
				case C_TRIANGLE:
					musname = "^";
					break;
				default:
					break;
				}
			}
			if (musname != 0) {
				(void) sprintf(buff + i, "%s", musname);
			}
			else {
				(void) sprintf(buff + i, "\\(%s)", 
					get_charname((int)*str, musfont));
			}
			while (buff[i] != '\0') {
				i++;
			}
			
			break;

		case STR_BACKSPACE:
			if (verbose == YES) {
				buff[i++] = '\\';
				buff[i++] = 'b';
			}
			/* ignore this and following char */
			str++;
			break;

		case STR_PRE:
		case STR_PST:
			if (verbose == YES) {
				buff[i++] = '<';
			}
			break;

		case STR_U_PRE:
		case STR_U_PST:
			if (verbose == YES) {
				buff[i++] = '<';
				buff[i++] = '^';
			}
			break;

		case STR_PRE_END:
		case STR_PST_END:
			if (verbose == YES) {
				buff[i++] = '>';
			}
			break;

		case STR_BOX:
			if (verbose == YES) {
				buff[i++] = '\\';
				buff[i++] = '[';
			}
			break;

		case STR_BOX_END:
			if (verbose == YES) {
				buff[i++] = '\\';
				buff[i++] = ']';
			}
			break;

		case STR_CIR:
			if (verbose == YES) {
				buff[i++] = '\\';
				buff[i++] = '{';
			}
			break;

		case STR_CIR_END:
			if (verbose == YES) {
				buff[i++] = '\\';
				buff[i++] = '}';
			}
			break;

		case STR_C_ALIGN:
			if (verbose == YES) {
				buff[i++] = '\\';
				buff[i++] = '^';
			}
			break;

		case STR_L_ALIGN:
			if (verbose == YES) {
				buff[i++] = '\\';
				buff[i++] = '|';
			}
			break;

		case STR_PILE:
			if (verbose == YES) {
				/* On figbass, we implictly add a pile start */
				if (textmod == TM_FIGBASS && string + 2 == str) {
					;
				}
				/* if this is at the end of a padded string,
				 * there is a high probability it is one
				 * we added implicitly, so skip it */
				else if (in_pile == YES && *(str+1) == ' ' &&
						*(str+2) == '\0') {
					;
				}
				else {
					/* in chordlike things, user didn't
					 * use a backslash, else they did */
					if (IS_CHORDLIKE(textmod) == NO) {
						buff[i++] = '\\';
					}
					buff[i++] = ':';
				}
			}
			/* keep track of toggle state */
			in_pile = (in_pile == YES ? NO : YES);
			break;

		case STR_SLASH:
			if (verbose == YES && textmod != TM_FIGBASS) {
				buff[i++] = '\\';
			}
			buff[i++] = '/';
			break;

		case STR_PAGENUM:
		case STR_NUMPAGES:
			if (pagenum == YES) {
				/* Write page number and update length.
				 * Actually, we don't have the correct values
				 * for this until late in program execution,
				 * and for MIDI, there are no pages at all,
				 * and this can be called from MIDI, so
				 * this is probably not really very useful,
				 * but this is the best we can do... */
				(void) sprintf(buff + i, "%d",
					(((unsigned char) *str) & 0xff)
					== STR_PAGENUM ?
					Pagenum : Last_pagenum);
				while (buff[i] != '\0') {
					i++;
				}
			}
			else {
				buff[i++] = '\\';
				buff[i++] = *(str+1);
			}
			str++;
			break;

		case STR_KEYMAP:
			str++;
			if (verbose == YES) {
				char *name;
				int len;

				/* Output the \m(  */
				buff[i++] = '\\';
				buff[i++] = 'm';
				buff[i++] = '(';
				/* Look up the keymap name */
				name = keymap_name(*str & 0xff);
				/* Keymap names can be of arbitrary length,
				 * so if we are running out, make sure we
				 * have plenty before copying in. */
				len = strlen(name);
				if (i + len > buff_length - 20) {
					buff_length = i + len + 20;
					REALLOCA(char, buff, buff_length);
				}
				(void) sprintf(buff + i, "%s", name);
				i += len;
				buff[i++] = ')';
			}
			break;

		case '\\':
			buff[i++] = '\\';
			buff[i++] = '\\';
			break;

		default:
			if (*str == '\n') {
				if (in_pile == YES) {
					if ( *(str+1) != '\0') {
						buff[i++] = ' ';
					}
				}
				else {
					buff[i++] = '\\';
					buff[i++] = 'n';
				}
			}
			else if (IS_CHORDLIKE(textmod) == YES && *str == ':') {
				buff[i++] = '\\';
				buff[i++] = ':';
			}
			else if (textmod == TM_FIGBASS && *str == '/') {
				buff[i++] = '\\';
				buff[i++] = '/';
			}
			else if (*str == ' ' && *(str+1) == '\0') {
				/* This is probably a space padding
				 * that we added implicitly,
				 * so don't print it. If this is
				 * called on a 'with' item or 'print' item
				 * where user explicitly added a space,
				 * this will strip that off, which, strictly
				 * speaking, it shouldn't. But that would
				 * only be for debugging anyway, and a
				 * strange case, so don't worry about it. */
				;
			}
			else {
				/* ordinary character */
				buff[i++] = *str;
			}
		}

		/* If running low on space, get some more. Could probably
		 * just truncate, since this is used for things like error
		 * messages, but alloc-ing more is easy enough. */
		if (i > buff_length - 20) {
			buff_length += ASCII_BSIZE;
			REALLOCA(char, buff, buff_length);
		}
	}
	buff[i++] = '\0';

	return(buff);
}


/*
 * Given a text string and a maximum desired width, try adding newlines at
 * white space to bring the width down under the desired width. If that's
 * not possible, do the best we can. Return pointer to the possibly
 * altered string.
 */

char *
split_string(string, desired_width)

char *string;
double desired_width;

{
	char *last_white_p;	/* where last white space was */
	char *curr_white_p;	/* white space we're dealing with now */
	char *str;		/* to walk through string */
	double proposed_width;	/* width of string so far */
	int font, size;
	int c;			/* the current character in string */
	int save_c;		/* temporary copy of c */
	int save_str;		/* temporary copy of character from string */


	/* Piles are incompatible with newlines, so we don't want to
	 * even attempt to split a string with a pile in it. */
	for (str = string + 2; *str != '\0'; str++) {
		if ((*str & 0xff) == STR_PILE) {
			/* string has a pile, so return it as is */
			return(string);
		}
	}

	/* Go through the string, until we hit white space. */
	last_white_p = (char *) 0;
	font = string[0];
	size = string[1];
	str = string + 2;
	while ((c = next_str_char(&str, &font, &size)) != '\0') {

		/* Are we at white space? */
		if ( ! IS_MUSIC_FONT(font) && (c == ' ' || c == '\t')) {

			if ( *(str+1) == '\0') {
				/* The white space is at the very end of
				 * the string, so replacing with a newline
				 * will not help; it will just make the
				 * string artifically taller. */
				break;
			}
			/* Temporarily replace with newline, and terminate
			 * to get width so far if we were to add a newline */
			curr_white_p = str - 1;
			save_c = c;
			save_str = *str;
			*curr_white_p = '\n';
			*str = '\0';
			proposed_width = strwidth(string);
			*curr_white_p = save_c;
			*str = save_str;

			if (proposed_width > desired_width) {
				if (last_white_p != (char *) 0) {
					/* reduce the width of the string by
					 * changing the most recent white space
					 * to a newline */
					*last_white_p = '\n';

					/* if the overall string is now short
					 * enough, we are done */
					if (strwidth(string) <= desired_width) {
						return(string);
					}
					last_white_p = curr_white_p;
				}
				else {
					/* No previous white space, so we
					 * can't make it short enough. So change
					 * this current white space to a
					 * newline, since that's the best we
					 * can do. But also set the desired
					 * width to our current width,
					 * because we know we're
					 * going to have to be at least this
					 * wide anyway, so we might as well use
					 * this much space on future lines */
					*curr_white_p = '\n';
					desired_width = proposed_width;

					/* no longer have a previous
					 * white space on the current line,
					 * because we just started a new
					 * line */
					last_white_p = (char *) 0;
				}

			}
			else {
				/* not too wide yet. Remember where this white
				 * space is, in case the next word makes us
				 * too wide and we have to change it to a
				 * newline */
				last_white_p = curr_white_p;
			}
		}
	}

	/* If last word went over the edge, move to next line if possible. */
	if (strwidth(string) > desired_width && last_white_p != (char *) 0) {
		*last_white_p = '\n';
	}

	/* Return the (possibly altered) string */
	return(string);
}


/* Given a point size and an adjustment factor, return a new point size.
 * If size would be less than MINSIZE, return MINSIZE.
 * If it would be greater than MAXSIZE, print error and return MAXSIZE.
 * Since we only use integer sizes, there may be some roundoff error.
 * While it would be possible to dream up a pathological case
 * where this roundoff might be big enough to notice,
 * for any sane scenario you would probably need
 * an extremely high resolution printer and a microscope to notice.
 */

int
adj_size(size, scale_factor, filename, lineno)

int size;		/* original point size */
double scale_factor;	/* multiply original size by this factor */
char *filename;		/* filename and lineno are for error messages */
int lineno;

{
	size = (int) ((double) size * scale_factor + 0.5);
	if (size < MINSIZE) {
		return(MINSIZE);
	}
	if (size > MAXSIZE) {
		l_warning(filename, lineno,
			"Adjusted size of string would be bigger than %d", MAXSIZE);
		return(MAXSIZE);
	}
	return(size);
}


/* Given a string that is in internal format, and a scale factor by which to
 * resize that string, adjust all size bytes in the string.
 */

char *
resize_string(string, scale_factor, filename, lineno)

char *string;		/* this is the string to adjust */
double scale_factor;	/* adjust sizes in string by this factor */
char *filename;		/* for error messages */
int lineno;		/* for error messages */

{
	char *s;	/* to walk through string */


	/* if string is empty, nothing to do */
	if (string == (char *) 0 || *string == '\0') {
		return(string);
	}

	/* if factor is sufficiently close to 1.0 that it's very clear
	 * we won't be making any changes (since we only use integer
	 * point sizes), don't bother */
	if ( fabs( (double) (scale_factor - 1.0)) < 0.01) {
		return(string);
	}

	/* second byte is size byte, so adjust that */
	string[1] = (char) adj_size( (int) string[1], scale_factor,
							filename, lineno);

	/* Go through the string. For each size byte, replace it with an
	 * adjusted size. Size bytes occur immediately after STR_SIZE
	 * and STR_MUS_CHAR commands. Everything else can get copied as
	 * is: STR_BACKSPACE is in terms of the default size, so it is
	 * unaffected by this resizing, and the other special string commands
	 * are unrelated to size and thus unaffected. */
	for (s = string + 2; *s != '\0'; s++) {
		switch ( (unsigned char) *s ) {
		case STR_SIZE:
		case STR_MUS_CHAR:
		case STR_MUS_CHAR2:
		case STR_USERDEF1:
			s++;
			*s = (char) adj_size( (int) *s, scale_factor,
							filename, lineno);
			break;
		default:
			break;
		}
	}

	return(string);
}


/* Given a circled string, return how much to add to its ascent and
 * descent to give room for the circle.  If pointer arguments are non-zero,
 * return additional values via those pointers.
 */

double
circled_dimensions(str, height_p, width_p, ascent_adjust, x_offset_p)

char *str;			/* a circled string */
float *height_p;		/* if non-zero, return circled height here */
float *width_p;			/* if non-zero, return circled width here */
float *ascent_adjust;		/* if non-zero, return amount we added to
				 * ascent to bring up to minimum height */
float *x_offset_p;		/* if non-zero, return where to print the
				 * actual string relative to circle edge */

{
	int font, size;
	float min_height;
	float adjust;			/* amount to bring up to min height */
	float uncirc_height, uncirc_width;/* dimensions of uncircled str */
	float circ_height;		/* height including circle */
	float circ_width;		/* width including circle */
	float circ_extra;		/* how much to add to top and
					 * bottom to allow space for circle */


	/* temporarily make the string uncircled */
	size = str[2] = str[1];
	font = str[1] = str[0];
	/* Note that there is at least one circumstance (in split_string())
	 * where a circled string is temporarily lacking the trailing END_CIR,
	 * and strheight and strwidth don't need it, so we don't need
	 * to blank that out. */

	/* get the dimensions of the uncircled version */
	uncirc_height = strheight(str+1);
	uncirc_width = strwidth(str+1);

	/* put the circle back */
	str[1] = str[2];
	str[2] = (char) STR_CIR;

	/* If string is unusually short vertically, treat as at least as tall
	 * as the font's ascent. That way if there are a bunch of
	 * circled things and one is tiny, like a dot, that circle
	 * won't be vastly smaller than the others. */
	min_height = fontascent(font, size);
	if (uncirc_height < min_height) {
		adjust = min_height - uncirc_height;
		uncirc_height = min_height;
	}
	else {
		adjust = 0.0;
	}

	/* Allow 25% of the height above and below as space for the circle. */
	circ_extra = 0.25 * uncirc_height;
	circ_height = 2.0 * circ_extra + uncirc_height;

	/* If width is up to 110% of the height, use the circled
	 * height as the circled width as well. */
	if (uncirc_width <= 1.1 * uncirc_height) {
		circ_width = circ_height;
	}
	else {
		/* make a little taller to compensate for the width */
		circ_extra += circ_height * .03 * (uncirc_width / uncirc_height);
		circ_height = 2.0 * circ_extra + uncirc_height;

		/* Use 50% of the circled height as the amount to add
		 * to the width, 25% on each end. */
		circ_width = uncirc_width + 0.5 * circ_height;
	}
	if (height_p != 0) {
		*height_p = circ_height;
	}
	if (width_p != 0) {
		*width_p = circ_width;
	}
	if (x_offset_p != 0) {
		*x_offset_p = (circ_width - uncirc_width) / 2.0;
	}
	if (ascent_adjust != 0) {
		*ascent_adjust = adjust;
	}

	return(circ_extra);
}


/* Return proper version of rehearsal mark string, based on staff number.
 * It may be circled, boxed, or plain. If circled or boxed, a new string
 * is returned. If plain, the string is returned as is.
 */

char *
get_reh_string(bar_p, staffnum)

struct BAR *bar_p;	/* the bar we want the rehearsal mark for */
int staffnum;	/* which staff it is for */

{
	char reh_buffer[100];	/* if not okay as it is, copy is put here */
	int style;
	char *string;		/* bar_p->reh_string */

	if (bar_p->reh_type == REH_BAR_MNUM) {
		style = svpath(staffnum, MEASNUMSTYLE)->measnumstyle;
	}
	else {
		style = svpath(staffnum, REHSTYLE)->rehstyle;
	}
	string = bar_p->reh_string;

	if (style == RS_PLAIN) {
		return(string);
	}

	if (strlen(string) + 3 > sizeof(reh_buffer)) {
		/* Usually reh marks are very short,
		 * so if this one is really long, too bad.
		 */
		ufatal("rehearsal mark is too long");
	}

	(void) sprintf(reh_buffer, "%c%s%c",
		style == RS_CIRCLED ? STR_CIR : STR_BOX,
		string + 2,
		style == RS_CIRCLED ? STR_CIR_END : STR_BOX_END);
	return(copy_string(reh_buffer, string[0], string[1]));
}


/* Map STR_MUS_CHAR* to FONT_MUSIC*  */

int
str2mfont(str)

int str;	/* STR_MUS_CHAR*  */

{
	switch (str) {
	case STR_MUS_CHAR:
		return(FONT_MUSIC);
	case STR_MUS_CHAR2:
		return(FONT_MUSIC2);
	case STR_USERDEF1:
		return(FONT_USERDEF1);
	default:
		pfatal("impossible str 0x%x in str2mfont", str);
		/*NOTREACHED*/
		return(FONT_MUSIC);
	}
}

/* Map FONT_MUSIC* to STR_MUS_CHAR*  */

int
mfont2str(mfont)

int mfont;	/* FONT_MUSIC*  */

{
	switch (mfont) {
	case FONT_MUSIC:
		return(STR_MUS_CHAR);
	case FONT_MUSIC2:
		return(STR_MUS_CHAR2);
	case FONT_USERDEF1:
		return(STR_USERDEF1);
	default:
		pfatal("impossible mfont %d in mfont2str", mfont);
		/*NOTREACHED*/
		return(STR_MUS_CHAR);
	}
}


/* Convert PostScript font character units (1000 unit scale where
 * 1 stepsize == 300 units) to Mup FONTFACTORs of an inch units. */

static int
fcu2ff(value)

int value;

{
	/* STEPSIZE * FONTFACTOR / 300 is the conversion facter. The
	 * 0.5 is to round up to next integer. */
	return((int)((value * STEPSIZE * FONTFACTOR / 300.0) + 0.5));
}


/* This function checks the name of a proposed user-defined symbol.
 * If it is that of a builtin music symbol, it allocated the Userfonts
 * entry for the override. Otherwise, it allocates
 * the next available slot for a user-defined symbol
 * and returns a pointer to it.
 * For now, we only support putting these user-defined symbols
 * in FONT_USERDEF1, from FIRST_CHAR till it fills up.
 * If we ever support more, we should just need to change a little code
 * in this function and some #defines and most of the rest of the
 * code should work as is. */

struct USER_SYMBOL *
alloc_usym(symname)

char *symname;	/* name for symbol to be used via \(....) */

{
	int ufont_index;	/* index into Userfonts */
	int is_small;		/* Will be YES for \(smXXXX) names */
	int font;
	int findex;		/* index into Fontinfo[] */
	struct USER_SYMBOL *user_symbol_p;	/* the allocated entry */
	char *p;		/* pointer into user's name */
	int alphas;		/* how many alphabetic found in user's name */
	int underscores;	/* how many underscores found in name */
	char *e_p;		/* location of 'e' or 'E' if any in name */


	/* First time, we allocate the Userfonts array.
	 * That way if user doesn't define any symbols (the most common case),
	 * we only use the space for the pointer. */
	if (Userfonts == 0) {
		CALLOC(USERFONT, Userfonts, NUM_SYMFONTS);
	}

	/* Check if user is overriding a builtin music character,
	 * or a user-defined they had already defined earlier. */
	/* There isn't really any "current" font, so we just use TR */
	font = FONT_TR;
	if ((Usym_code = find_char(symname, &font, &is_small, NO))
						!= (unsigned char) BAD_CHAR) {
		if ( ! IS_MUSIC_FONT(font)) {
			l_yyerror(Curr_filename, yylineno,
				"'%s' cannot be redefined; only music symbols and user-defined symbols can be redefined", symname);
			/* Return a junk struct that caller can fill in,
			 * so caller doesn't have to care if this failed. */
			CALLOC(USER_SYMBOL, user_symbol_p, 1);
			user_symbol_p->name = symname;
			return(user_symbol_p);
		}
		Usym_font = font;

		Usym_code = CHAR_INDEX(Usym_code);
		user_symbol_p = &(Userfonts[SYMFONT_INDEX(Usym_font)].symbols[Usym_code]);
		user_symbol_p->name = symname;
		if (user_symbol_p->postscript != 0) {
			l_warning(Curr_filename, yylineno,
				"Symbol '%s' has already been defined before; using last",
				symname);
		}
		else {
			(Userfonts[SYMFONT_INDEX(Usym_font)].num_symbols)++;
		}
		Usym_override = YES;
		user_symbol_p->flags = 0;
		return(user_symbol_p);
	}
	Usym_override = NO;
	Usym_font = FONT_UNKNOWN;


	/* Error check the name. Rules: must contain only
	 * letters/numbers/underscores and must contain
	 * at least one alphabetic.  PostScript would allow us to be more
	 * generous on rules, but it seems safer to restrict somewhat.
	 * It is much easier to allow more later if needed
	 * than try to restrict later. The one alphabetic rule
	 * is to ensure PostScript doesn't confuse with a number.
	 */
	for (alphas = underscores = 0, e_p = 0, p = symname; *p != '\0'; p++) {
		if ( ! isalnum(*p)  && *p != '_') {
			break;
		}
		if (isalpha(*p)) {
			alphas++;
			if (*p == 'e' || *p == 'E') {
				/* special case for "exponential notation" */
				e_p = p;
			}
		}
		if (*p == '_') {
			underscores++;
		}
	}
	if (*p != '\0' || alphas == 0) {
		l_yyerror(Curr_filename, yylineno,
			"'%s' is not a valid name; must contain only letters/numbers/underscores and contain at least one alphabetic.", symname);
		/* Return a junk struct that caller can fill in,
		 * so caller doesn't have to care if this failed. */
		CALLOC(USER_SYMBOL, user_symbol_p, 1);
		user_symbol_p->name = symname;
		return(user_symbol_p);
	}

	/* We will use the name as a PostScript name. A PostScript name
	 * must be distinguishable from a number, and a number can be in
	 * exponential notation, like 3e6, so we can't accept anything matching
	 * 	^[0-9]+[eE][0-9]+$
	 * because that would result in a PostScript error that would almost
	 * certainly baffle the user.
	 * This is such an unlikely case, we just apologetically
	 * force the user to pick a different name.
	 * Actually, now we add a prefix, so this check should not be
	 * necessary, but it's such a small restriction, we leave it,
	 * in case some day we need it back.
	 */
	if (alphas == 1 && e_p != 0 && underscores == 0 && e_p > symname
						&& *(e_p + 1) != '\0') {
		l_yyerror(Curr_filename, yylineno,
			"Sorry, a name consisting of digit(s) followed by e or E followed by digit(s) cannot be used as a symbol name; please choose another name");
		CALLOC(USER_SYMBOL, user_symbol_p, 1);
		user_symbol_p->name = symname;
		return(user_symbol_p);
	}

	/* We add symbols one at a time, so the one being defined will be
	 * at the index matching how many symbols previously defined */
	Usym_font = Curr_usym_font;
	ufont_index = SYMFONT_INDEX(Usym_font);
	Usym_code = Userfonts[ufont_index].num_symbols;

	/* The very first time, we create all the arrays and such.
	 * If user doesn't define any characters,
	 * then we don't need the space at all. */
	findex = font_index(FONT_USERDEF1);
	if (Usym_code == 0) {
		/* 8 bytes gives enough space for name mfontNN. Since we only
		 * have one byte for font numbers in strings, and have already
		 * used a big portion of the the 255 slots for other things,
		 * room for two digits should be plenty. */
		MALLOCA(char, Fontinfo[findex].ps_name, 8);
		sprintf(Fontinfo[findex].ps_name, "Mfont%d", ufont_index);
		Fontinfo[findex].numchars = MAX_CHARS_IN_FONT;
		MALLOCA(char *, Fontinfo[findex].charnames,
						Fontinfo[findex].numchars);
		MALLOCA(short, Fontinfo[findex].ch_width,
						Fontinfo[findex].numchars);
		MALLOCA(short, Fontinfo[findex].ch_height,
						Fontinfo[findex].numchars);
		MALLOCA(short, Fontinfo[findex].ch_ascent,
						Fontinfo[findex].numchars);
		Fontinfo[findex].fontfile = 0;
		Fontinfo[findex].is_ital = NO;
		Fontinfo[findex].was_used = YES;
	}

	/* Make sure there is room in the font. Note that the Usym_code
	 * starts from zero, not FIRST_CHAR. */
	if (Usym_code >= Fontinfo[findex].numchars) {
		ufatal("Too many user-defined symbols. (%d max)\n",
					Fontinfo[findex].numchars);
	}

	/* Prepare to save info in USERFONT, for use in PostScript prolog */
	(Userfonts[ufont_index].num_symbols)++;
	user_symbol_p = &(Userfonts[ufont_index].symbols[Usym_code]);
	user_symbol_p->name = symname;

	debug(4, "defining user symbol '%s' in font %d code %d",
			symname, Usym_font, Usym_code);

	return(user_symbol_p);
}


/* This function is called after all information about a user-defined symbol
 * has been gathered from the user. It
 *	- calculates height/width/ascent info from the bounding box
 *	- registers the symbol as a note head if user says to
 *	- adds the symbol name to the name-to-number map (unless it is an
 *	override of something already in the table)
 */


void
define_usym()

{
	struct USER_SYMBOL *usym_p;	/* which symbol to finish defining */
	int f_index;			/* index into Fontinfo */
	int sym_index;			/* index into symbol arrays (music and user) */
	int lineno;			/* for error messages */

	/* This function is not called until we have gone to the next
	 * context, so yylineno is at least one too high. It may be even
	 * more if there are blank lines or comments, and/or the error
	 * was in an earlier line of the symbol context, but we can't easily
	 * know how many more. This will at least give us a line number
	 * that is still within the symbol context, rather than the next one.
	 */
	lineno = yylineno - 1;


	if (Usym_font == FONT_UNKNOWN) {
		/* Must have been an earlier error */
		return;
	}

	/* Figure out indexes into the various arrays. Some arrays contain all
	 * valid fonts, like Fontinfo. Those use f_index.
	 * Others contain all symbol fonts (builtin and user),
	 * like Sym2code_table. Those use sym_index.
	 */
	f_index = font_index(Usym_font);
	sym_index = SYMFONT_INDEX(Usym_font);
	usym_p = &(Userfonts[sym_index].symbols[Usym_code]);

	if ((usym_p->flags & US_POSTSCRIPT) == 0) {
		l_yyerror(Curr_filename, lineno,
			"Missing postscript definition for symbol '%s'",
			usym_p->name);
	}

	if ((usym_p->flags & US_BBOX) == 0) {
		l_yyerror(Curr_filename, lineno,
			"Missing bbox definition for symbol '%s'",
			usym_p->name);
	}
	else {
		debug(4, "defining user symbol %s: font %d, code %d, bbox(%d %d %d %d), stemoffset(%d %d)",
				usym_p->name, Usym_font, Usym_code,
				usym_p->llx, usym_p->lly,
				usym_p->urx, usym_p->ury,
				usym_p->upstem_y, usym_p->downstem_y);

		/* Check for too big */
		if (usym_p->llx < -MAX_USYM_UNITS || usym_p->lly < -MAX_USYM_UNITS
				|| usym_p->urx > MAX_USYM_UNITS
				|| usym_p->ury > MAX_USYM_UNITS) {
			l_yyerror(Curr_filename, lineno,
				"User defined character too large; limit %d units in any direction", MAX_USYM_UNITS);
		}

		/* Derive width/height/ascent and store in FONTINFO */
		/* Note that we allow zero width/height for "invisible"
		 * accidentals, but disallow negative values */
		if (usym_p->urx - usym_p->llx < 0.0) {
			l_yyerror(Curr_filename, lineno,
					"symbol width is negative");
		}
		Fontinfo[f_index].ch_width[Usym_code]
					= fcu2ff(usym_p->urx - usym_p->llx);

		if (usym_p->ury - usym_p->lly < 0.0) {
			l_yyerror(Curr_filename, lineno,
					"symbol height is negative");
		}
		Fontinfo[f_index].ch_height[Usym_code]
					= fcu2ff(usym_p->ury - usym_p->lly);

		if ((Fontinfo[f_index].ch_ascent[Usym_code]
					= fcu2ff(usym_p->ury)) < 0) {
			/* It is legal to have no ascent (e.g. an underscore),
			 * but we treat that as zero ascent, not negative. */
			Fontinfo[f_index].ch_ascent[Usym_code] = 0;
		}

		debug(4, "deduced height %d, width %d, ascent %d, for font %d, index %d",
			Fontinfo[f_index].ch_height[Usym_code],
			Fontinfo[f_index].ch_width[Usym_code],
			Fontinfo[f_index].ch_ascent[Usym_code],
			Usym_font, Usym_code);

		/* Adjust font-wide max ascent/height info */
		if (Fontinfo[f_index].maxheight < Fontinfo[f_index].ch_height[Usym_code]) {
			Fontinfo[f_index].maxheight = Fontinfo[f_index].ch_height[Usym_code];
		}
		if (Fontinfo[f_index].maxascent < Fontinfo[f_index].ch_ascent[Usym_code]) {
			Fontinfo[f_index].maxascent = Fontinfo[f_index].ch_ascent[Usym_code];
		}

		/* If is a notehead, add to table of valid noteheads */
		if (usym_p->flags & US_STEMOFFSET) {
			if (usym_p->upstem_y < usym_p->lly ||
						usym_p->upstem_y > usym_p->ury ||
						usym_p->downstem_y < usym_p->lly ||
						usym_p->downstem_y > usym_p->ury) {
				/* Mup can handle this just fine,
				 * but it seems dubious the user really meant this. */
				l_warning(Curr_filename, lineno,
					"stem offset is outside the bounding box");
			}
			add_user_head(usym_p->name, Usym_font,
				Usym_code + FIRST_CHAR,
				usym_p->upstem_y, usym_p->downstem_y);
		}
	}

	/* If override of existing symbol, we are done; others need more work */
	if (Usym_override == YES) {
		return;
	}

	Fontinfo[f_index].charnames[Usym_code] = usym_p->name;
	add_char(usym_p->name, FK_USER1, Usym_code + FIRST_CHAR);
}


/* Return a hash number for a character name. XOR the bytes and mask
 * by the symbol table size. Size assumed to be power of 2 */
int
chhash(name)

char *name;

{
	int h;

	for (h = 0; *name != '\0'; name++) {
		h ^= *name;
	}
	return(h & CH_TBL_MASK);
}


/* Add the name of a character to hash table of names. This is the PostScript
 * CharStrings name used in the Encoding vector. */

static void
add_char(name, fontkind, code)

char *name;	/* The name, like "Aacute" or "questiondown" */
int fontkind;	/* FK_* value */
int code;	/* like the ASCII code, or really the index into Encoding */

{
	int h;				/* name hash */
	struct CHARINFO *ci_p;		/* character information */
	struct CHARINFO *newchar_p;	/* info to add to table */


	/* See if already in table */
	h = chhash(name);
	for (ci_p = Char_table[h]; ci_p != 0; ci_p = ci_p->next) {
		if (strcmp(ci_p->name, name) == 0) {
			if (fontkind == FK_USER1) {
				if ( ! (ci_p->fontkind & FK_CAN_OVERRIDE) ) {
					l_yyerror(Curr_filename, yylineno,
					"you can only override music and user-defined symbols");
				}
			}
			else {
				pfatal("multiple definitions of symbol %s", name);
			}
			return;
		}
	}

	MALLOC(CHARINFO, newchar_p, 1);
	newchar_p->name = name;
	newchar_p->fontkind = fontkind;
	newchar_p->code = code;
	newchar_p->next = Char_table[h];
	Char_table[h] = newchar_p;
}


/* Initialize the hash table of character names with all the known chars */

void
init_charinfo_table()
{
	int i;

	/* For each character in standard font that is beyond the ascii set,
	 * add to table. (We don't ever need to look up the ascii ones,
	 * and skipping them lets the user define their own by those names
	 * that won't collide. */
	for (i = 96; i < FI_std_numchars; i++) {
		add_char(FI_std_names[i], FK_STD, i + FIRST_CHAR);
	}

	/* for each extended font class, add its elements */
	for (i = 0; i < FI_ext1_numchars; i++) {
		add_char(FI_ext1_names[i], FK_EXT1, i + FIRST_CHAR);
	}
	for (i = 0; i < FI_ext2_numchars; i++) {
		add_char(FI_ext2_names[i], FK_EXT2, i + FIRST_CHAR);
	}
	for (i = 0; i < FI_ext3_numchars; i++) {
		add_char(FI_ext3_names[i], FK_EXT3, i + FIRST_CHAR);
	}

	/* add the music characters */
	for (i = 0; i < FI_mus0_numchars; i++) {
		add_char(FI_mus0_names[i], FK_MUS1, i + FIRST_CHAR);
	}
	for (i = 0; i < FI_mus1_numchars; i++) {
		add_char(FI_mus1_names[i], FK_MUS2, i + FIRST_CHAR);
	}

	for (i = 0; i < FI_SYM_numchars; i++) {
		add_char(FI_SYM_names[i], FK_SYM, i + FIRST_CHAR);
	}
	for (i = 0; i < FI_ZD1_numchars; i++) {
		add_char(FI_ZD1_names[i], FK_ZD1, i + FIRST_CHAR);
	}
	for (i = 0; i < FI_ZD2_numchars; i++) {
		add_char(FI_ZD2_names[i], FK_ZD2, i + FIRST_CHAR);
	}

	/* Note that user defined will get added as they are defined ... */
}


/* Given the name of a character, look it up and return its code. Also
 * return (via font_p) what font it is in, and (and is_small_p) whether
 * is should be made "small."  On error, (an unknown character), returns
 * BAD_CHAR and the values of font_p and is_small_p are meaningless. */

unsigned char
find_char(name, font_p, is_small_p, errmsg)

char *name;	/* the \(xxxxx) name to look up */
int *font_p;	/* The current standard font should be passed in. If the char
		 * being requested is in a different font, this will be
		 * updated to the value of the appropriate font. */
int *is_small_p;/* When called from elsewhere, should point to NO;
		 * when called recursively, will point to YES.
		 * On return will have set to YES or NO, but is meaningless
		 * if the function return is BAD_CHAR. */
int errmsg;	/* If YES, do a yyerror. Otherwise caller just wants to
		 * know if it is valid, but okay if it isn't. */
{
	int h;			/* has number */
	struct CHARINFO *ci_p;	/* the info we are looking for */
	char expanded[24];	/* normal versions of shortcuts */
	char *charname;		/* points to either name or expanded */

	/* First assume we can use name as is */
	charname = name;

	/* Do shortcut translations.
	 * A letter followed by one of '`^~:/,vo(-.c
	 * represents acute, grave, circumflex, tilde, dieresis,
	 * slash, cedilla, caron, ring, breve, macron, dotaccent, ogonek
	 * And as a special case, ss represents germandbls.
	 * Note that historically, when we supported less than these,
	 * we used comma for cedilla, so have to keep that for backward
	 * compatibility, although in retrospect maybe comma should have
	 * been used for commaaccent.
	 */
	if (strlen(name) == 2 && isalpha(name[0])) {
		switch (name[1]) {
		case '\'':
			(void) sprintf(expanded, "%cacute", name[0]);
			charname = expanded;
			break;
		case '`':
			(void) sprintf(expanded, "%cgrave", name[0]);
			charname = expanded;
			break;
		case '^':
			(void) sprintf(expanded, "%ccircumflex", name[0]);
			charname = expanded;
			break;
		case '~':
			(void) sprintf(expanded, "%ctilde", name[0]);
			charname = expanded;
			break;
		case ':':
			(void) sprintf(expanded, "%cdieresis", name[0]);
			charname = expanded;
			break;
		case '/':
			(void) sprintf(expanded, "%cslash", name[0]);
			charname = expanded;
			break;
		case ',':
			(void) sprintf(expanded, "%ccedilla", name[0]);
			charname = expanded;
			break;
		case 'v':
			(void) sprintf(expanded, "%ccaron", name[0]);
			charname = expanded;
			break;
		case 'o':
			(void) sprintf(expanded, "%cring", name[0]);
			charname = expanded;
			break;
		case '(':
			(void) sprintf(expanded, "%cbreve", name[0]);
			charname = expanded;
			break;
		case '-':
			(void) sprintf(expanded, "%cmacron", name[0]);
			charname = expanded;
			break;
		case '.':
			(void) sprintf(expanded, "%cdotaccent", name[0]);
			charname = expanded;
			break;
		case 'c':
			(void) sprintf(expanded, "%cogonek", name[0]);
			charname = expanded;
			break;
		case 's':
			if (name[0] == 's') {
				(void) sprintf(expanded, "germandbls");
				charname = expanded;
			}
			break;
		default:
			/* not a special shortcut, leave as is */
			break;
		}
	}
	/* Some more special case shortcuts: `` and '' are shortcuts for
	 * quotedblleft and quotedblright, and << and >> for guillemots */
	if (strcmp(name, "``") == 0) {
		charname = "quotedblleft";
	}
	else if (strcmp(name, "''") == 0) {
		charname = "quotedblright";
	}
	else if (strcmp(name, "<<") == 0) {
		charname = "guillemotleft";
	}
	else if (strcmp(name, ">>") == 0) {
		charname = "guillemotright";
	}

	h = chhash(charname);

	for (ci_p = Char_table[h]; ci_p != 0; ci_p = ci_p->next) {
		if (strcmp(ci_p->name, charname) == 0) {
			/* Found it, fill in the font */
			if (ci_p->fontkind == FK_MUS1) {
				*font_p = FONT_MUSIC;
			}
			else if (ci_p->fontkind ==  FK_MUS2) {
				*font_p = FONT_MUSIC2;
			}
			else if (ci_p->fontkind == FK_EXT1 && *is_small_p == NO) {
				*font_p = *font_p + NUM_STD_FONTS;
			}
			else if (ci_p->fontkind == FK_EXT2 && *is_small_p == NO) {
				*font_p = *font_p + 2 * NUM_STD_FONTS;
			}
			else if (ci_p->fontkind == FK_EXT3 && *is_small_p == NO) {
				*font_p = *font_p + 3 * NUM_STD_FONTS;
			}
			else if (ci_p->fontkind == FK_USER1) {
				*font_p = FONT_USERDEF1;
			}
			else if (ci_p->fontkind == FK_SYM && *is_small_p == NO) {
				*font_p = FONT_SYM;
			}
			else if (ci_p->fontkind == FK_ZI && *is_small_p == NO) {
				*font_p = FONT_ZI;
			}
			else if (ci_p->fontkind == FK_ZD1 && *is_small_p == NO) {
				*font_p = FONT_ZD1;
			}
			else if (ci_p->fontkind == FK_ZD2 && *is_small_p == NO) {
				*font_p = FONT_ZD2;
			}
			else {
				break;
			}
			return(ci_p->code);
		}
	}
	if (strncmp(charname, "sm", 2) == 0 && *is_small_p == NO) {
		*is_small_p = YES;
		return(find_char(charname + 2, font_p, is_small_p, errmsg));
	}
	
	if (errmsg == YES) {
		l_yyerror(Curr_filename, yylineno, "'%s' is not a valid character name", charname);
	}
	return(BAD_CHAR & 0xff);
}


/* Return YES if given character code is not a legal code */

int
is_bad_char(ch)

int ch;

{
	ch &= 0xff;
	if (ch < FIRST_CHAR || ch >= MAX_CHARS_IN_FONT + FIRST_CHAR) {
		return(YES);
	}
	return(NO);
}


/* Given a font and a code of a character in that font, return its name */

char *
get_charname(code, font)

int code;
int font;

{
	/* cancel out any sign extension that may have happened */
	code &= 0xff;

	if (code < FIRST_CHAR || code >= Fontinfo[font_index(font)].numchars
							+ FIRST_CHAR) {
		pfatal("get_charname passed invalid code of %d for font %d",
					code, font);
	}
	return(Fontinfo[font_index(font)].charnames[code - FIRST_CHAR]);
}


/* This function is somewhat of a kludge. We require that
 * boxed or circled text be the only thing in a string, because
 * that makes some things simpler, but we do want to allow changing
 * font and size before that. So when we find the start of boxing or circling,
 * we call this function. It goes through the internal-format string being
 * generated, applying any font or size changes it finds, by migrating their
 * effects to the font/size bytes at the beginning of the string.
 * Anything else found is an error. If it finds things to process,
 * it returns &(string[2]) to tell the caller that is its "current" place.
 * Otherwise it will return the original place that was passed in.
 */

static char *
migrate_font_size(string, out_p, deststring)

char *string;	/* The string to process which is already in internal form
		 * but only filled in as far as out_p so far */
char *out_p;
char *deststring;	/* If we need to migrate font/size, migrate them
			 * to the first two bytes of this string. */

{
	char *s_p;		/* index through the string */

	if (out_p == string + 2) {
		/* The [ or { must be  at the right place
		 * (immedidately after the font and size)
		 * so we don't have to do anything here. */
		return(out_p);
	}

	/* Walk through the bytes of the output string, after the
	 * font/size bytes, to just before where we are.
	 * For any font/size changes (which are already in internal form)
	 * move that information to the font/size bytes of the string.
	 * If all goes well (no invalid things found), then return the
	 * altered out_p to make the caller overwrite the bytes we processed.
	 */
	for (s_p = string + 2; s_p < out_p; s_p++) {
		switch ( ((unsigned char)*s_p) & 0xff ) {
		case STR_FONT:
			deststring[0] = *++s_p;
			break;
		case STR_SIZE:
			deststring[1] = *++s_p;
			break;
		default:
			return(out_p);
		}
	}

	return(string + 2);
}


/* Make sure tag name embedded in lyric string follows the naming rules.
 * Must be either a single lower case letter, or an underscore followed
 * by zero or more letters, digits, or underscores.  Give error message
 * if bad.
 */

static void
validate_tag(tag, fname, lineno)

char *tag;	/* The tag. Must end with STR_END_TAG */
char *fname;	/* input file name for error messages */
int lineno;	/* input line number for error messages */

{
	if ( (tag[1] & 0xff) == STR_END_TAG) {
		/* Single byte lone, must be lower case alphabetic */
		if ( ! islower(tag[0]) ) {
			l_yyerror(fname, lineno, "single character tags must be a lower case letter");
		}
	}
	else {
		if (tag[0] != '_') {
			l_yyerror(fname, lineno, "multi-character tags must begin with an underscore");
			return;
		}
		for (  ; (*tag & 0xff) != STR_END_TAG; tag++) {
			if (  ! isalnum(*tag) && *tag != '_') {
				l_yyerror(fname, lineno, "multi-character tags must contain only letters, numbers and underscores");
			return;
			}
		}
	} 
}
