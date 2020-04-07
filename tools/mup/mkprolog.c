
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

/* This program reads text from stdin and writes a C function
 * to stdout that will print the text, adding in the
 * proper escapes for %, etc. The function that is created
 * is void ps_prolog(). This is used for building the PostScript prolog used
 * by Mup into the Mup program itself.
 */

#include <stdio.h>

int
main()
{
	int c;
	int at_newline = 1;


	/* start up the function */
	(void) printf("/* Copyright 1995, 1996, 1997, 1998, 1999, 2000, 2003, 2004, 2007, 2008, 2009, 2012, 2016 by Arkkra Enterprises */\n\n");
	(void) printf("#include <stdio.h>\n#include \"globals.h\"\n\n");


	(void) printf("char *prolog_text[] = {\n");

	/* make a text statement for each line of input read */
	while ((c = getchar()) != EOF) {

		/* generate a separate text line for each line of input */
		if (at_newline == 1) {
			(void) printf("\t\"");
			at_newline = 0;
		}

		/* handle things that have to be escaped */
		switch (c) {
		case '"':
			putchar('\\');
			putchar('"');
			break;
		case '\\':
			putchar('\\');
			putchar('\\');
			break;
		case '\n':
			at_newline = 1;
			(void) printf("\",\n");
			break;
		default:
			putchar( (char) c);
			break;
		}
	}

	/* end the array of text lines */
	(void) printf("(char *) 0\n};\n\n");

	/* now generate the print function */
	(void) printf("/* generate the PostScript prolog */\n\n");
	(void) printf("void\nps_prolog()\n{\n");
	(void) printf("\tint line;\n\n");
	(void) printf("\tfor (line = 0; prolog_text[line] != (char *) 0; line++) {\n");
	(void) printf("\t\t(void) printf(\"%%s\\n\", prolog_text[line]);\n");
	(void) printf("\t}\n");

	/* close the function */
	(void) printf("}\n");
	return(0);
}
