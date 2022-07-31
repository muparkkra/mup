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

/* Program to generate Mup files for each page of the table of
 * extended characters. It expects a file to exist named "charlist"
 * that contains a list, one per line, of the characters.
 * It produces as many Mup files named ext_X.mup as needed to
 * show them all, where X starts at 1.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLS_PER_LINE 4

int
main()
{
	FILE * f;	/* charlist file, list character names */
	FILE * out;	/* Mup output file being generated */
	char outfilename[32];	/* name of output file */
	int len;	/* strlen of current character name */
	int maxlen;	/* widest strlen of any character name */
	char buff[BUFSIZ];	/* for a line of input */
	int page;	/* which page of output being generated */
	int line;	/* current line number */
	int col;	/* current column */

	if ((f = fopen("charlist", "r")) == 0) {
		exit(1);
	}
	/* First go through and find the widest name,
	 * so we know how to space the columns */
	maxlen = 0;
	while (fgets(buff, sizeof(buff), f) != 0) {
		len = strlen(buff) - 1;
		if (len > maxlen) {
			maxlen = len;
		}
	}
	rewind(f);

	/* Loop through, print the characters and their names. */
	out = 0;
	page = 1;
	line = 1;
	col = 1;
	while (fgets(buff, sizeof(buff), f) != 0) {
		buff[strlen(buff)-1] = '\0';
		if (line == 1 && col == 1) {
			/* begin a new page */
			if (out != 0) {
				(void) fclose(out);
			}
			(void) sprintf(outfilename, "ext_%d.mup", page);
			if ((out = fopen(outfilename, "w")) == 0) {
				(void) fprintf(stderr, "can't open %s for writing\n", outfilename);
				exit(1);
			}
			(void) fprintf(out, "score\n  fontfamily=courier\n size = 10\n");
			(void) fprintf(out, "pagewidth=7.8;pageheight=9;topmargin=0;botmargin=0;leftmargin=0.4\n");
			(void) fprintf(out, "block\n");
		}
		if (col == 1) {
			(void) fprintf(out, "left nl \"");
		}
		(void) fprintf(out, "\\(%s)\\b   %-*s", buff, maxlen, buff);
		if (col < COLS_PER_LINE) {
			(void) fprintf(out, "   ");
			col++;
		}
		else {
			/* Start a new line */
			(void) fprintf(out, "\";\n");
			col = 1;
			if (++line >= 51) {
				/* go to the next page */
				page++;
				line = 1;
			}
		}
	}
	/* finish last line if not multiple of 4 */
	if (col <= COLS_PER_LINE) {
		(void) fprintf(out, "\";\n");
	}
	(void) fclose(f);
	(void) fclose(out);
	exit(0);
}
