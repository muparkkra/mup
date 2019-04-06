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

/* This creates a template.C file that declares a string containing
 * the text of template.mup, that can them be compiled into Mupmate.
 * Building the template into Mupmate means
 * the template file does not have to exist at run time, and we don't
 * have to look for it. We used to do this with an "ed" script, but
 * now have this C program to try to be more portable.
 * It reads in the LICENSE (given as first argument), and outputs it
 * inside a comment. Then it prints an explanatory comment and declares
 * the string. Then it reads the template.mup file (second argument) and
 * outputs it with proper quoting: backslashes before backslashes and double
 * quote, and backslashed newlines at the end of each line.
 * It finishes off with the final double quote and semicolon.
 */

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
	FILE *in;
	char buff[BUFSIZ];
	int line;
	int c;

	if (argc != 3) {
		(void) fprintf(stderr, "usage: %s LICENSE template.mup\n", argv[0]);
		exit(1);
	}

	/* Open the LICENSE file */
	if ((in = fopen(argv[1], "r")) == 0) {
		(void) fprintf(stderr, "cannot open %s\n", argv[1]);
		exit(1);
	}

	/* Output the contents of the file inside a comment */
	(void) printf("/*\n");
	while (fgets(buff, sizeof(buff), in)) {
		printf("%s", buff);
	}
	(void) fclose(in);

	(void) printf(" */\n\n/* We build the template into Mupmate so we do not have to depend on the\n"
	" * template file existing at run time. */\n"
	"extern const char * const template_text =\n");

	/* Open template.mup file */
	if ((in = fopen(argv[2], "r")) == 0) {
		(void) fprintf(stderr, "cannot open %s\n", argv[2]);
		exit(1);
	}

	/* Copy template.mup file contents to output with appropriate quoting */
	line = 1;
	while (fgets(buff, sizeof(buff), in)) {
		(void) printf("%s", (line++ == 1 ? "\"" : "\\n\\\n"));
		for (c = 0; (buff[c] != '\n') && (buff[c] != '\0'); c++) {
			if ( (buff[c] == '\\') || (buff[c] == '"') ) {
				putchar('\\');
			}
			putchar(buff[c]);
		}
	}
	(void) fclose(in);

	/* End the generated string */
	(void) printf("\";\n");
	exit(0);
}
