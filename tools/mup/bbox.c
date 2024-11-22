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

/* This program finds the bounding box information for Mup music characters,
 * and adds that information to the Mup PostScript prolog,
 * and generates in musfont.c file, which
 * Mup uses for character width/height/ascent data.
 *
 * This program reads the prolog.ps.in file and creates prolog.ps, which
 * consists of prolog.ps.in plus a dictionary of bounding box information
 * for all music characters. It also creates a file which
 * contains C structure initializations giving the height, width, and ascent
 * for all music characters.
 *
 * For each music character, it generates a PostScript program that consists
 * of prolog.ps.in and code to generate the specific character in the middle
 * of the page in 100 point size. It then runs Ghostscript on that program,
 * using the "bit" device. It then reads the bitmap output and determines the
 * bounding box that contains all the pixels generated. It uses that information
 * to generate the output files. For Mup width, height, and ascent purposes,
 * it adds a STDPAD of white space around all characters except for special
 * cases like note heads and flags.
 * Ghostscript now has a bbox device which could probably be used,
 * but this program was already written using bit, so not worth rewriting.
 */


#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>

#if defined(SIGCHLD) && ! defined(SIGCLD)
#define SIGCLD SIGCHLD
#endif

#include "defines.h"
/* page height in rows and width in bytes, with 8 bits per byte, rounded up */
#define PGHEIGHT	((int)(11.0 * PPI))
#define PGWIDTH		((int)(8.5 * PPI))
#define PGWIDTHBYTES	((PGWIDTH / 8) + 1)

/* to make sure we don't clip any "fuzz" from the edges of characters,
 * add a little white space all around. This is how many pixels extra to
 * add (for 100-pt character) */
#define FUZZ	1.5

/* character vital statistics */
float width[NUM_MFONTS][MAX_CHARS_IN_FONT];
float height[NUM_MFONTS][MAX_CHARS_IN_FONT];
float ascent[NUM_MFONTS][MAX_CHARS_IN_FONT];

char *name[NUM_MFONTS][MAX_CHARS_IN_FONT];	/* map character code to character name */

struct Map {
	char * charname;
	short code;
} Sym2code_table[NUM_MFONTS][MAX_CHARS_IN_FONT];

char buff[BUFSIZ];	/* general purpose buffer */
FILE *fullprolog;	/* generated prolog.ps */

/* names of temporary files */
char pstempfile[] = "mctemp.ps";
char gstempfile[] = "mctemp";

void make_sym_name_list P((void));
void setup_bbox_dict P((FILE *fullproglog, int font));
void do_a_char P((char *charname, int ch, int font));
int leftbit P((int byte));
int rightbit P((int byte));
void cleanup P((int status));
void adjust0 P((int ch));
void adjust2 P((int ch));
#ifdef linux
#define sigset(x, y) signal(x, y)
#endif


int
main()
{
	int f;		/* font index */
	int i;		/* character index */
	FILE *outf;	/* c file with metrics */
	char * suffix;	/* "" for FONT_MUSIC, or "2" for FONT_MUSIC2 */
	int numch[NUM_MFONTS];	/* how many music characters */
	FILE *prolog;	/* prolog.ps.in */


	/* arrange to clean up */
	for (i = 1; i < NSIG; i++) {
		(void) sigset(i, cleanup);
	}
	(void) sigset(SIGCLD, SIG_DFL);
	(void) sigset(SIGWINCH, SIG_DFL);

	/* open appropriate files */
	if ((prolog = fopen("prolog.ps.in", "r")) == NULL) {
		(void) fprintf(stderr, "can't open prolog.ps.in\n");
		exit(1);
	}

	if ((fullprolog = fopen("prolog.ps", "w")) == NULL) {
		(void) fprintf(stderr, "can't open prolog.ps\n");
		exit(1);
	}

	if ((outf = fopen("musfont.c", "w")) == NULL) {
		(void) fprintf(stderr, "can't open musfont.c\n");
		exit(1);
	}

	/* copy prolog.ps.in to prolog.ps */
	while (fgets(buff, BUFSIZ, prolog)) {
		(void) fprintf(fullprolog, "%s", buff);
	}
	(void) fclose(prolog);

	make_sym_name_list();

	/* figure out bounding box info for all characters and fill into
	 * prolog.ps bounding box dictionary */
	for (f = 0; f < NUM_MFONTS; f++) {
		setup_bbox_dict(fullprolog, f);
		numch[f] = 0;
		for (i = 0; i < MAX_CHARS_IN_FONT; i++) {
			if (Sym2code_table[f][i].charname == 0) {
				break;
			}
			do_a_char(Sym2code_table[f][i].charname,
				CHAR_INDEX(Sym2code_table[f][i].code), f);
			(numch[f])++;
		}
		/* finish off font definition in prolog.ps */
		(void) fprintf(fullprolog, "\nend\nend\n\n/Mfont%d mfont%d definefont\n\n", f, f);
	}

	(void) fclose(fullprolog);
	(void) fprintf(stderr, "\n");
	(void) fflush(stderr);

	/* adjust to put a little white space around most characters */
	adjust0(numch[0]);
	adjust2(numch[1]);

	/* put width/height/ascent values into file for Mup use */
	(void) fprintf(outf, "/* Copyright (c) 2009 by Arkkra Enterprises */\n/* All rights reserved */\n");
	(void) fprintf(outf, "\n/* Machine generated metrics information about the music symbol fonts */\n\n");
	(void) fprintf(outf, "#include \"defines.h\"\n");
	(void) fprintf(outf, "#include \"structs.h\"\n");
	(void) fprintf(outf, "#include \"globals.h\"\n\n");

	for (f = 0; f < NUM_MFONTS; f++) {
		(void) fprintf(outf, "char *FI_mus%d_names[] = {", f);
		for (i = 0; i < numch[f]; i++) {
			if (i % 4 == 0) {
				(void) fprintf(outf, "\n");
			}
			(void) fprintf(outf, " \"%s\"",
					Sym2code_table[f][i].charname);
			if (i < numch[f] - 1) {
				(void)fprintf(outf, ",");
			}
		}
		(void) fprintf(outf, "};\n");

		(void) fprintf(outf, "short FI_M%d_widths[] = {", f);
		for (i = 0; i < numch[f]; i++) {
			if (i % 4 == 0) {
				(void) fprintf(outf, "\n");
			}
			(void) fprintf(outf, " %d",
				(int) (FONTFACTOR * width[f][i]));
			if (i < numch[f] - 1) {
				(void)fprintf(outf, ",");
			}
		}
		(void) fprintf(outf, "};\n");

		(void) fprintf(outf, "short FI_M%d_heights[] = {", f);
		for (i = 0; i < numch[f]; i++) {
			if (i % 4 == 0) {
				(void) fprintf(outf, "\n");
			}
			(void) fprintf(outf, " %d",
				(int) (FONTFACTOR * height[f][i]));
			if (i < numch[f] - 1) {
				(void)fprintf(outf, ",");
			}
		}
		(void) fprintf(outf, "};\n");

		(void) fprintf(outf, "short FI_M%d_ascents[] = {", f);
		for (i = 0; i < numch[f]; i++) {
			if (i % 4 == 0) {
				(void) fprintf(outf, "\n");
			}
			(void) fprintf(outf, " %d",
				(int) (FONTFACTOR * ascent[f][i]));
			if (i < numch[f] - 1) {
				(void)fprintf(outf, ",");
			}
		}
		(void) fprintf(outf, "};\n");
		(void) fprintf(outf, "int FI_mus%d_numchars = %d;\n", f, numch[f]);
	}

	suffix = "";
	(void) fprintf(outf, "\n\nvoid\ninit_musfont_metrics()\n{\n\tint index;\n\n");
	for (f = 0; f < NUM_MFONTS; f++) {
		(void) fprintf(outf, "\tindex = FONT_MUSIC%s - 1;\n", suffix);
		(void) fprintf(outf, "\tFontinfo[index].ps_name = \"Mfont%d\";\n", f);
		(void) fprintf(outf, "\tFontinfo[index].numchars = FI_mus%d_numchars;\n", f);
		(void) fprintf(outf, "\tFontinfo[index].charnames = FI_mus%d_names;\n", f);
		(void) fprintf(outf, "\tFontinfo[index].ch_width = FI_M%d_widths;\n", f);
		(void) fprintf(outf, "\tFontinfo[index].ch_height = FI_M%d_heights;\n", f);
		(void) fprintf(outf, "\tFontinfo[index].ch_ascent = FI_M%d_ascents;\n", f);
		(void) fprintf(outf, "\tFontinfo[index].is_ital = NO;\n");

		/* prepare for second music font */
		suffix = "2";
	}
	(void) fprintf(outf, "}\n");
	(void) fclose(outf);

	cleanup(0);
	/*NOTREACHED*/
	return(0);
}


/* Populate the Sym2code_table */

void
make_sym_name_list()
{
	FILE *mc;		/* muschar.h */
	char line[BUFSIZ];	/* for lines read from mc */
	int fontindex;		/* 0 for FONT_MUSIC, 1 for FONT_MUSIC2 */
	/* Lines are of the form:   C_xxx (N) */ 
	char * name;		/*    ^        - start of character name */
	char * endname;		/*      ^      - end of name*/
	char * valuestr;	/*         ^   - the character number */
	int value;		/* atoi(N) */
	int n;			/* index through name to convert to lower */

	if ((mc = fopen("muschar.h", "r")) == 0) {
		/* try new location */
		if ((mc = fopen("../include/muschar.h", "r")) == 0) {
			(void) fprintf(stderr, "can't open muschar.h or ../include/muschar.h\n");
			exit(1);
		}
	}
	fontindex = 0;
	while (fgets(line, sizeof(line), mc) != 0) {
		if (strstr(line, "FONT_MUSIC2") != 0) {
			fontindex = 1;
		}
		else if (strncmp(line, "#define", 7) != 0) {
			continue;
		}
		else {
			if ((name = strchr(line, '_')) == 0) {
				(void) fprintf(stderr, "expecting underscore: %s", line);
				exit(1);
			}
			name++;
			if ((endname = strpbrk(name, " \t")) == 0) {
				(void) fprintf(stderr, "expecting white space: %s", line);
				exit(1);
			}
			if ((valuestr = strchr(endname, '(')) == 0) {
				(void) fprintf(stderr, "expecting paren: %s", line);
				exit(1);
			}
			valuestr++;
			*endname = '\0';
			value = atoi(valuestr);
			for (n = 0; name[n] != '\0'; n++) {
				name[n] = tolower(name[n]);
			}
			Sym2code_table[fontindex][value - FIRST_CHAR].charname = strdup(name);
			Sym2code_table[fontindex][value - FIRST_CHAR].code = value;
		}
	}
	fclose(mc);
}


/* Output the beginning of a music font bounding box dictionary */

void
setup_bbox_dict(fullprolog, font)

FILE *fullprolog;	/* prolog.ps file */
int font;		/* which font number to do */

{
	/* add prolog information to set up bounding box dictionary */
	(void) fprintf(fullprolog,
			"\n%% find size of Encoding and make a dictionary\n");
	(void) fprintf(fullprolog,
			"%% that size for bounding box information\n");
	(void) fprintf(fullprolog, "mfont%d begin\n", font);
	(void) fprintf(fullprolog, "/dictsize Encoding length def\n");
	(void) fprintf(fullprolog, "mfont%d /Mcbbox%d dictsize dict put\n\n",
			font, font);
	(void) fprintf(fullprolog,
			"%% temporarily redefine printmchar%d to get code\n", font);
	(void) fprintf(fullprolog,
			"5 dict begin\n/printmchar%d { {} forall } def\n\n", font);
}


/* find bounding box information for a character. We do this by running
 * ghostscript on a program that consists of prolog.ps.in and instructions
 * to print the character in 100-point size, with origin in the middle
 * of the page, using the "bit" device. Then take the output, figure out
 * the topmost, bottommost, leftmost and rightmost places where there are
 * pixels filled in. Convert to appropriate units for PostScript bounding
 * box and for music character metrics information. */

void
do_a_char(charname, ch, font)

char *charname;
int ch;			/* code */
int font;		/* font number */

{
	int top, bottom, leftmost, rightmost;
	int leftmostbit, rightmostbit;		/* bit within byte */
	int x;					/* temporary for bit position */
	float urx, ury, llx, lly;		/* bounding box */
	int i, j;				/* row/col loop counters */
	int ret;				/* return code */
	int t;					/* temp file */
	FILE *psf;				/* PostScript input file */
	FILE *prolog;				/* prolog.ps.in */
	int special_case = 0;			/* if true, add tiny dot
						 * at baseline, to make height
						 * and ascent come out right. */
	int special_case2 = 0;			/* for blankhead */


	if (charname == (char *) 0) {
		/* element to end array */
		return;
	}

	(void) fprintf(stderr, "%s ", charname);
	(void) fflush(stderr);

	/* put PostScript program into temp file */
	if ((psf = fopen(pstempfile, "w")) == NULL) {
		(void) fprintf(stderr, "can't open temp PS file\n");
		cleanup(1);
	}

	/* copy prolog.ps.in to temp file */
	if ((prolog = fopen("prolog.ps.in", "r")) == NULL) {
		(void) fprintf(stderr, "can't open prolog.ps.in\n");
		cleanup(1);
	}

	while (fgets(buff, BUFSIZ, prolog)) {
		/* For dim and halfdim we have to pretend they go all the
		 * way down to the baseline to get height and ascent correct
		 * so they won't get misplaced in strings. So massage the
		 * PostScript definitions to add a tiny dot at the baseline.
		 * 1rest and ll1rest also have to be handled that way.
		 */
		if (strcmp(buff, "\t\t% dim\n") == 0
				|| strcmp(buff, "\t\t% halfdim\n") == 0
				|| strcmp(buff, "\t\t% 1rest\n") == 0
				|| strcmp(buff, "\t\t% ll1rest\n") == 0) {
			special_case = 1;
		}
		else if (special_case && strcmp(buff, "\t\t} def\n") == 0) {
			fprintf(psf, "\t\tnewpath 0 0 5 0 360 arc fill\n");
			special_case = 0;
		}

		/* blankhead is another special case. We make it do
		 * a rectangle note head for the purposes of bounding box. */
		if (strcmp(buff, "\t\t% blankhead\n") == 0) {
			special_case2 = 1;
		}
		else if (special_case2 && strcmp(buff, "\t\t} def\n") == 0) {
			fprintf(psf, "\t\t\tdo_rectangle\n");
			special_case2 = 0;
		}

		(void) fprintf(psf, "%s", buff);
	}
	(void) fclose(prolog);

	/* add flagsep definition */
	(void) fprintf(psf, "/flagsep %.2f 300 mul def\n", FLAGSEP / STEPSIZE);

	/* change printmchar to leave the code on the stack */
	(void) fprintf(psf, "/printmchar%d { { } forall } def\n", font);

	/* move to center of page */
	(void) fprintf(psf, "%d %d translate\n", PGWIDTH / 2, PGHEIGHT / 2);

	/* compensate from 1000 unit font scale to 100-point size
	 *  character */
	(void) fprintf(psf, "0.1 0.1 scale\n");

	/* emit code to look up character in font dictionary and draw it */
	(void) fprintf(psf, "mfont%d begin Encoding ", font);
	(void) fprintf(psf, "%s", charname);
	(void) fprintf(psf, " get CharStrings exch get exec end\n");
	/* display and quit */
	(void) fprintf(psf, "showpage\n");
	(void) fprintf(psf, "quit\n");
	(void) fclose(psf);


	/* run ghostscript */
	switch(fork()) {
	case 0:
		(void) sprintf(buff, "-sOutputFile=%s", gstempfile);
		(void) execlp("gs", "gs", "-sDEVICE=bit", "-dNOPAUSE",
						"-sPAPERSIZE=letter",
						"-dQUIET", buff,
						 pstempfile, (char *) 0);
		/*FALLTHRU*/
	case -1:
		(void) fprintf(stderr, "failed to exec gs\n");
		cleanup(1);
	default:
		(void) wait(&ret);
		
		if (ret != 0) {
			(void) fprintf(stderr, "gs failed\n");
			cleanup(1);
		}

		/* read output from ghostscript */
		if ((t = open(gstempfile, O_RDONLY)) < 0) {
			(void) fprintf(stderr, "can't open temp file\n");
			cleanup(1);
		}

		/* find bounding box info. For left/right, work inwards till
		 * find a non-zero byte. For top/bottom find rows
		 * containing non-zero bytes */
		leftmost = PGWIDTHBYTES - 1;
		rightmost = 0;
		/* the following line is to keep lint happy */
		leftmostbit = rightmostbit = bottom = 0;
		top = -1;
		for (i = 0; i < PGHEIGHT; i++) {
			/* read a row of bits */
			if (read(t, buff, (unsigned) PGWIDTHBYTES)
							!= PGWIDTHBYTES) {
				(void) fprintf(stderr, "read error\n");
				cleanup(-1);
			}

			/* see if any farther left than we found before */
			for (j = 0; j <= leftmost; j++) {
				if (buff[j]) {
					if (j == leftmost) {
						if ((x = leftbit(buff[j])) <
								leftmostbit) {
							leftmostbit = x;
						}
					}
					else {
						leftmost = j;
						leftmostbit = leftbit(buff[j]);
					}
					break;
				}
			}

			/* see if any farther right */
			for (j = PGWIDTHBYTES - 1; j >= rightmost; j--) {
				if (buff[j]) {
					if (j == rightmost) {
						if ((x = rightbit(buff[j])) >
								rightmostbit) {
							rightmostbit = x;
						}
					}
					else {
						rightmost = j;
						rightmostbit = rightbit(buff[j]);
					}
					break;
				}
			}

			/* check if anything in row, for finding top/bottom */
			for (j = 0; j < PGWIDTHBYTES; j++) {
				if (buff[j]) {
					if (top == -1) {
						top = i;
					}
					bottom = i;
					break;
				}
			}
		}
		if (top < 0) {
			(void) fprintf(stderr, "no output generated\n");
			cleanup(-1);
		}

		/* compensate for middle-of-page origin */
		llx = (leftmost * 8) + leftmostbit - (PGWIDTH / 2);
		urx = (rightmost * 8) + rightmostbit + 1 - (PGWIDTH / 2);
		/* Postscript goes up positive */
		lly = PGHEIGHT - bottom - (PGHEIGHT / 2);
		ury = PGHEIGHT - top - (PGHEIGHT / 2);

		/* fill in values. characters are in 1000 units,
		 * output in 72nds of an inch. */
		width[font][ch] = (urx - llx + 1) / 720.0;
		height[font][ch] = (ury - lly + 1) / 720.0;
		ascent[font][ch] = ury / 720.0;
		name[font][ch] = charname;
	
		/* do PostScript bounding box dictionary entry for this
		 * character. Convert from 100-point size character to
		 * 1000 unit character. Add an extra FUZZ all around to make
		 * sure we don't chop anything off. */
		(void) fprintf(fullprolog,
			"Mcbbox%d Encoding %s get [ %.1f 0 %.1f %.1f %.1f %.1f ] put\n",
				font, charname, (urx - llx + 4) * 10.0,
				(llx - FUZZ) * 10.0, (lly - FUZZ) * 10.0,
				(urx + FUZZ) * 10.0, (ury + FUZZ) * 10.0);

		(void) close(t);
	}
}


/* given a byte with at least 1 bit set to 1, return the bit position of the
 * leftmost 1, numbering left to right from 0 */

int
leftbit(byte)

int byte;

{
	int i;


	for (i = 0; i < 8; i++) {
		if ( byte & (0x80 >> i) ) {
			return(i);
		}
	}
	(void) fprintf(stderr, "bug -- leftbit was passed 0\n");
	cleanup(-1);
	/*NOTREACHED*/
	return(0);
}

/* similarly for finding rightmost bit */

int
rightbit(byte)

int byte;
{
	int i;

	for (i = 7; i >= 0; i--) {
		if (byte & (0x80 >> i) ) {
			return(i);
		}
	}
	(void) fprintf(stderr, "bug -- rightbit was passed 0\n");
	cleanup(-1);
	/*NOTREACHED*/
	return(0);
}


/* remove temporary files and exit */

void
cleanup(status)

int status;

{
	(void) unlink(pstempfile);
	(void) unlink(gstempfile);

	exit(status);
}


/* adjust to put a little white space around characters except for some
 * special cases, like note heads and flags */

void
adjust0(nch)

int nch;		/* how many music characters */

{
	float pad = 2.0 * STDPAD;


	for (  ; nch >= 0; nch--) {

		switch(nch) {

		case CHAR_INDEX(C_QUADWHOLE):
		case CHAR_INDEX(C_OCTWHOLE):
		case CHAR_INDEX(C_DBLWHOLE):
		case CHAR_INDEX(C_1N):
		case CHAR_INDEX(C_2N):
		case CHAR_INDEX(C_4N):
		case CHAR_INDEX(C_XNOTE):
		case CHAR_INDEX(C_DIAMOND):
		case CHAR_INDEX(C_FILLDIAMOND):
		case CHAR_INDEX(C_UPFLAG):
		case CHAR_INDEX(C_DNFLAG):
		case CHAR_INDEX(C_PEDAL):
			/* these are the special cases that get no padding */
			break;

		case CHAR_INDEX(C_BEGPED):
		case CHAR_INDEX(C_ENDPED):
			/* add padding in vertical direction only */
			height[0][nch] += pad;
			ascent[0][nch] += STDPAD;
			break;

		default:
			/* add padding */
			width[0][nch] += pad;
			height[0][nch] += pad;
			ascent[0][nch] += STDPAD;
			break;
		}
	}
}


/* adjust to put a little white space around appropriate characters in MFONT2 */

void
adjust2(nch)

int nch;		/* how many music characters */

{
	float pad = 2.0 * STDPAD;


	for (  ; nch >= 0; nch--) {
		switch(nch) {
		case CHAR_INDEX(C_DWHRIGHTTRIANGLE):
		case CHAR_INDEX(C_RIGHTTRIANGLE):
		case CHAR_INDEX(C_FILLRIGHTTRIANGLE):
		case CHAR_INDEX(C_UDWHRIGHTTRIANGLE):
		case CHAR_INDEX(C_URIGHTTRIANGLE):
		case CHAR_INDEX(C_UFILLRIGHTTRIANGLE):
		case CHAR_INDEX(C_DWHRECTANGLE):
		case CHAR_INDEX(C_RECTANGLE):
		case CHAR_INDEX(C_FILLRECTANGLE):
		case CHAR_INDEX(C_DWHISOSTRIANGLE):
		case CHAR_INDEX(C_ISOSTRIANGLE):
		case CHAR_INDEX(C_FILLISOSTRIANGLE):
		case CHAR_INDEX(C_DWHPIEWEDGE):
		case CHAR_INDEX(C_PIEWEDGE):
		case CHAR_INDEX(C_FILLPIEWEDGE):
		case CHAR_INDEX(C_DWHSEMICIRCLE):
		case CHAR_INDEX(C_SEMICIRCLE):
		case CHAR_INDEX(C_FILLSEMICIRCLE):
		case CHAR_INDEX(C_DWHSLASHHEAD):
		case CHAR_INDEX(C_SLASHHEAD):
		case CHAR_INDEX(C_FILLSLASHHEAD):
		case CHAR_INDEX(C_BLANKHEAD):
		case CHAR_INDEX(C_MENSURDIAMOND):
		case CHAR_INDEX(C_MENSURFILLDIAMOND):
		case CHAR_INDEX(C_MENSURDBLWHOLE):
		case CHAR_INDEX(C_MENSURUPFLAG):
		case CHAR_INDEX(C_MENSURDNFLAG):
			/* these are the special cases that get no padding */
			break;
		default:
			/* add padding */
			width[1][nch] += pad;
			height[1][nch] += pad;
			ascent[1][nch] += STDPAD;
			break;
		}
	}
}
