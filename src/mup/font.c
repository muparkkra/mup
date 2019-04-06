
/*
 Copyright (c) 1995-2019  by Arkkra Enterprises.
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

/* This file contains functions related to fonts,
 * both user defined and native PostScript fonts.
 */

#include <string.h>
#include "defines.h"
#include "structs.h"
#include "globals.h"


/* map font names to numbers. */
static struct FONTMAP {
	char *fontname;		/* abbreviated (similar to troff-style)
				 * or full font name */
	int	findex;		/* font number */
} Font_table[] = {
	/*====== this table must be sorted alphabetically to allow
	 *====== binary search!!!!!  */
	{ "AB", FONT_AB },
	{ "AI", FONT_AI },
	{ "AR", FONT_AR },
	{ "AX", FONT_AX },
	{ "BB", FONT_BB },
	{ "BI", FONT_BI },
	{ "BR", FONT_BR },
	{ "BX", FONT_BX },
	{ "CB", FONT_CB },
	{ "CI", FONT_CI },
	{ "CR", FONT_CR },
	{ "CX", FONT_CX },
	{ "HB", FONT_HB },
	{ "HI", FONT_HI },
	{ "HR",	FONT_HR },
	{ "HX", FONT_HX },
	{ "NB", FONT_NB },
	{ "NI", FONT_NI },
	{ "NR", FONT_NR },
	{ "NX", FONT_NX },
	{ "PB", FONT_PB },
	{ "PI", FONT_PI },
	{ "PR", FONT_PR },
	{ "PX", FONT_PX },
	{ "TB",	FONT_TB },
	{ "TI",	FONT_TI },
	{ "TR",	FONT_TR },
	{ "TX",	FONT_TX },
	{ "avantgarde bold", FONT_AB },
	{ "avantgarde boldital", FONT_AX },
	{ "avantgarde ital", FONT_AI },
	{ "avantgarde rom", FONT_AR },
	{ "bookman bold", FONT_BB },
	{ "bookman boldital", FONT_BX },
	{ "bookman ital", FONT_BI },
	{ "bookman rom", FONT_BR },
	{ "courier bold", FONT_CB },
	{ "courier boldital", FONT_CX },
	{ "courier ital", FONT_CI },
	{ "courier rom", FONT_CR },
	{ "helvetica bold", FONT_HB },
	{ "helvetica boldital", FONT_HX },
	{ "helvetica ital", FONT_HI },
	{ "helvetica rom", FONT_HR },
	{ "newcentury bold", FONT_NB },
	{ "newcentury boldital", FONT_NX },
	{ "newcentury ital", FONT_NI },
	{ "newcentury rom", FONT_NR },
	{ "palatino bold", FONT_PB },
	{ "palatino boldital", FONT_PX },
	{ "palatino ital", FONT_PI },
	{ "palatino rom", FONT_PR },
	{ "times bold", FONT_TB },
	{ "times boldital", FONT_TX },
	{ "times ital", FONT_TI },
	{ "times rom", FONT_TR }
};

/* Strings to look for in a fontfile */
char *Mup_name = "Mup font name:";
char *PostScript_name = "PostScript font name:";
char *PS_definition = "PostScript:";
char *Size_data = "Size data:";

/* static functions */
static int fncmp P((const void *fn1, const void * fn2));
static char *get_expected P((FILE *fontfile_p, char *filename, char *expected,
		int *lineno_p));
static char *get_noncomment P((FILE *fontfile_p, int *lineno_p));


/* given a font number, return its index into Fontinfo table. FONT_TR
 * is the first entry in the table.
 * If the font number given is out of range, pfatal.
 */

int
font_index(font)

int font;	/* which font */

{
	if ((font < 0) || (font >= MAXFONTS)) {
		pfatal("font %d out of range", font);
	}
	/* offset relative to first valid font */
	font = font - FONT_TR;

	return(font);
}


/* given a fontname, return its number, or FONT_UNKNOWN */

int
lookup_font(fontname)

char *fontname;

{
	struct FONTMAP *finfo_p;


	if ((finfo_p = (struct FONTMAP *) bsearch(fontname, Font_table,
			NUMELEM(Font_table), sizeof(struct FONTMAP), fncmp))
			!= (struct FONTMAP *) 0) {
		return(finfo_p->findex);
	}
	else {
		return(FONT_UNKNOWN);
	}
}



/* font name comparision function for use by bsearch() */

static int
fncmp(fn1, fn2)

#ifdef __STDC__
const void *fn1;	/* font name to check */
const void *fn2;	/* pointer to FONTMAP to compare with. declare as char *
			 * since that's what bsearch() thinks it gives us,
			 * then we cast appropriately */
#else
char *fn1;	/* font name to check */
char *fn2;	/* pointer to FONTMAP to compare with. declare as char *
		 * since that's what bsearch() thinks it gives us, then we
		 * cast appropriately */
#endif

{
	return(strcmp(fn1, ((struct FONTMAP *) fn2)->fontname));
}


/* Given a font number, return its name. We don't have to do this too often,
 * it's a simple int compare, and we only need to look through
 * abbreviated names, so just do linear search. */

char *
fontnum2name(font)

int font;

{
	int f;
	int elements;

	/* divide by 2 because only need to check abbreviations */
	elements = NUMELEM(Font_table) / 2;
	for (f = 0; f < elements; f++) {
		if (Font_table[f].findex == font) {
			return(Font_table[f].fontname);
		}
	}
	return("unknown");
}


/* This handles a fontfile, reading it in, validating its contents,
 * and saving the information in the Fontinfo array.
 */

void
parse_font_file(filename)

char *filename;

{
	FILE *fontfile_p;
	char *name;	/* Mup font name */
	char *ps_name;	/* PostScript font name */
	int findex;	/* which font is being defined */
	int c;		/* character index */
	int code;	/* "ASCII" code value */
	int width, height, ascent;
	char *buffer;	/* line read from file */
	int lineno;
	int max_height, max_ascent;


	debug(2, "parse_font_file(%s)", filename);

	if ((fontfile_p = find_file(&filename)) == (FILE *) 0) {
		l_yyerror(Curr_filename, yylineno, "can't open '%s'", filename);
		return;
	}

	/* first line of file is expected to contain the Mup font name */
	lineno = 0;
	if ((name = get_expected(fontfile_p, filename, Mup_name, &lineno))
							!= (char *) 0) {
		if ((findex = lookup_font(name)) == FONT_UNKNOWN) {
			l_yyerror(filename, lineno,
				"'%s' is not a valid Mup font name", name);
			return;
		}
		findex = font_index(findex);

		if (Fontinfo[findex].fontfile != (FILE *) 0) {
			l_yyerror(filename, lineno,
				"Font '%s' redefined more than once", name);
			return;
		}
		/* Save the file pointer, since we'll need to read the rest of
		 * the file to put into the Mup output */
		Fontinfo[findex].fontfile = fontfile_p;
	}
	else {
		return;
	}

	/* Next line of file is expected to contain the PostScript font name */
	if ((ps_name = get_expected(fontfile_p, filename, PostScript_name, &lineno))
							!= (char *) 0) {
		if (strlen(ps_name) == 0 ) {
			l_yyerror(filename, lineno,
					"No PostScript font name value given");
			return;
		}
		Fontinfo[findex].ps_name = ps_name;
	}
	else {
		return;
	}

	/* Next line of file is expected to contain the Size data line */
	if ((ps_name = get_expected(fontfile_p, filename, Size_data, &lineno))
							== (char *) 0) {
		return;
	}

	max_height = max_ascent = 0;
	for (c = FIRST_CHAR; c < FIRST_CHAR + 95; c++) {
		buffer = get_noncomment(fontfile_p, &lineno);
		if ( sscanf(buffer, "%d %d %d %d",
					&code, &width, &height, &ascent) != 4) {
			buffer[strlen(buffer) - 1] =  '\0';
			l_yyerror(filename, lineno,
				"size data line has incorrect format: '%s'",
				buffer);
			return;
		}
		if (c != code) {
			l_yyerror(filename, lineno,
				"expecting size data for character %d, but got %d instead",
				c, code);
			return;
		}

		/* Because of how backspace works (see comment in defines.h)
		 * we need to limit width to 0.5 inch for a DEFAULT_SIZE
		 * character. */
		if (width > 500) {
			l_yyerror(filename, lineno, "width must be less than 500");
			return;
		}

		/* save size in table */
		code = CHAR_INDEX(code);
		Fontinfo[findex].ch_height[code] = height;
		Fontinfo[findex].ch_width[code] = width;
		Fontinfo[findex].ch_ascent[code] = ascent;

		if (height > max_height) {
			max_height = height;
		}
		if (ascent > max_ascent) {
			max_ascent = ascent;
		}
	}

	Fontinfo[findex].maxheight =  (double) max_height / (double) FONTFACTOR;
	Fontinfo[findex].maxascent =  (double) max_ascent / (double) FONTFACTOR;

	/* Next line of file is expected to contain the PostScript: line */
	if ((ps_name = get_expected(fontfile_p, filename, PS_definition, &lineno))
							== (char *) 0) {
		return;
	}
}


/* Read from given file. If next non-comment line starts as expected,
 * return a copy of the rest of the line after any white space.
 * Otherwise print an error and return 0.
 */

static char *
get_expected(fontfile_p, filename, expected, lineno_p)

FILE *fontfile_p;
char *filename;
char *expected;	/* line read is expected to start with this */
int *lineno_p;	/* line number where line was found is returned here */

{
	char *buffer;
	char *newstring;
	char *p;

	buffer = get_noncomment(fontfile_p, lineno_p);
	if (strncmp(buffer, expected, strlen(expected)) != 0) {
		l_yyerror(filename, *lineno_p,
			"Expecting '%s' in font_file '%s'", expected, filename);
		return((char *) 0);
	}

	/* skip any leading white space */
	for (buffer += strlen(expected); isspace(*buffer); buffer++) {
		;
	}

	/* trim any white space from the end of the string */
	for (p = buffer + strlen(buffer) - 1; p >= buffer; p--) {
		if (isspace(*p)) {
			*p = '\0';
		}
		else {
			break;
		}
	}

	/* make a copy and return it */
	MALLOCA(char, newstring, strlen(buffer) + 1);
	strcpy(newstring, buffer);
	return(newstring);
}


/* Read lines from given file until a non-comment line is found, and
 * return that line. A comment is a line that has # in its first column.
 * Returns a null string on end of file. The line returned is in a static
 * buffer overwritten on each call, so caller must save if they need a copy.
 */

static char *
get_noncomment(file, lineno_p)

FILE *file;
int *lineno_p;	/* line number gets sents in and returned here */

{
	static char buffer[128];

	while (fgets(buffer, sizeof(buffer), file) != (char *) 0) {
		(*lineno_p)++;
		if ( *buffer != '#') {
			/* not a comment, so return it */
			return(buffer);
		}
	}
	buffer[0] = '\0';
	return(buffer);
}


/* return the height for a font in inches for a given size */

double
fontheight(font, size)

int font;
int size;

{
	return( (double) Fontinfo[font_index(font)].maxheight *
			((double) size / (double) DFLT_SIZE) );
}



/* return the ascent for a font in inches for a given size */

double
fontascent(font, size)

int font;
int size;

{
	return( (double) Fontinfo[font_index(font)].maxascent *
			((double) size / (double) DFLT_SIZE) );
}



/* return the descent for a font in inches for a given size */

double
fontdescent(font, size)

int font;
int size;

{
	return( fontheight(font, size) - fontascent (font, size) );
}
