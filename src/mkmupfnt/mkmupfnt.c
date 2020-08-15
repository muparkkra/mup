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

char Copyright[] =
	"Copyright (c) 1999-2013 by Arkkra Enterprises\nAll rights reserved\n\n";

/* This program generates a Mup fontfile, that will let you override a
 * Mup font with one of your own. It is done in C rather than with
 * a "shell script" to be portable to systems without a Unix-like shell.
 * See the "usage_message" for how to invoke this program.
 * It creates a PostScript program to print each character in the font
 * and print out its width, height, and ascent.
 * It runs Ghostscript on that program.
 */

#ifdef __DJGPP__
#define __DOS__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#ifdef __DOS__
#include <process.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifdef O_BINARY
#define READ_FLAGS (O_RDONLY | O_BINARY)
#define WRITE_FLAGS (O_WRONLY | O_BINARY | O_CREAT | O_TRUNC)
#else
#define READ_FLAGS (O_RDONLY)
#define WRITE_FLAGS (O_WRONLY | O_CREAT | O_TRUNC)
#endif

/* temp file used for PostScript program */
char *PS_script_file = "mkmupfnt.ps";
#ifdef __DOS__
char *GS_output = "mkmupfnt.tmp";
#endif

char Version[] = "6.8.1";

void usage(char *program_name);
void verify_valid_Mup_name(char *Mup_name);
int name_matches(char *namelist[], char *name, int namelength);
void run_Ghostscript(char *PostScript_name, char *Mup_name);
char * make_string(char *first_part, char *second_part);
void generate_PostScript_program(char *PS_file);
void pswrite(int file, char *data, int length);
void cleanup(int exitcode);

int
main(int argc, char **argv)
{
	char *PostScript_name;
	char *Mup_name;
	char *outfile;

	fprintf(stderr, "%s Version %s\n%s", argv[0], Version, Copyright);

	if (argc < 4 || argc > 5) {
		usage(argv[0]);
	}

	PostScript_name = argv[1];
	Mup_name = argv[2];
	outfile = argv[3];

	verify_valid_Mup_name(Mup_name);

	if ((freopen(outfile, "w", stdout)) == (FILE *) 0) {
		fprintf(stderr, "Can't open '%s'\n", outfile);
		exit(1);
	}

	/* Generate a PostScript program to run, and redirect that program
	 * into Ghostscript. */
	generate_PostScript_program(argc == 5 ? argv[4] : (char *) 0);
	if ((freopen(PS_script_file, "r", stdin)) == (FILE *) 0) {
		fprintf(stderr, "Can't open '%s'\n", PS_script_file);
		cleanup(1);
	}
	run_Ghostscript(PostScript_name, Mup_name);

	/* If there is a PostScript file to add to the output, copy that */
	if (argc == 5) {
		int file;
		int n;
		char buff[BUFSIZ];

		if ((file = open(argv[4], READ_FLAGS)) < 0) {
			fprintf(stderr, "Can't open '%s'\n", argv[4]);
			cleanup(1);
		}
		while ((n = read(file, buff, BUFSIZ)) > 0) {
			if (write(1, buff, n) != n) {
				(void) fprintf(stderr, "write failed\n");
				cleanup(1);
			}
		}
		close(file);
	}

	cleanup(0);
	/* This line is not reached, since cleanup() exits,
	 * but some compilers complain if function doesn't have a
	 * return or exit, so the return is here to appease them. */
	return(0);
}


char *usage_message =
	"PostScript_font_name Mup_font_name outfile [file]\n\n"
	" Generates a fontfile for Mup to use, to override a Mup font.\n"
	" Arguments are:\n\n"
	"    PostScript_font_name   the name of the font you want to add to Mup,\n"
	"                                  like 'Helvetica-Narrow'\n\n"
	"    Mup_font_name          the name of the Mup font you want to replace,\n"
	"                                  like 'HR' or 'helvetica rom'\n\n"
	"    outfile                the generated Mup fontfile\n\n"
	"    file                   can contain PostScript to be added to Mup prolog,\n"
	"                                  if needed to use the font.\n";

void
usage(char *program_name)
{
	fprintf(stderr, "usage: %s %s", program_name, usage_message);
	exit(1);
}

/* verify Mup font name is a valid one, give error and exit if not */

char *family_names[] = {
	"avantegarde",
	"bookman",
	"courier",
	"helvetica",
	"newcentury",
	"palatino",
	"times",
	(char *) 0
};

char *font_names[] = {
	"rom",
	"bold",
	"ital",
	"boldital",
	(char *) 0
};

void
verify_valid_Mup_name(char *Mup_name)
{
	char *space_loc;

	if (strlen(Mup_name) == 2 && strchr("ABCHNPT", Mup_name[0])
					&& strchr("RBIX", Mup_name[1]) ) {
		/* name is okay, an abbreviated name */
		return;
	}

	/* check long names */
	if ((space_loc = strchr(Mup_name, ' ')) != 0 &&
		name_matches(family_names, Mup_name, space_loc - Mup_name) &&
		name_matches(font_names, space_loc + 1, strlen(space_loc + 1))) {
		return;
	}

	fprintf(stderr, "'%s' is not a valid Mup font name\n", Mup_name);
	exit(1);
}

/* verify name given matches one on the list of valid names */

int
name_matches(char *namelist[], char *name, int namelength)
{
	int i;

	for (i = 0; namelist[i] != (char *) 0; i++) {
		if (strncmp(namelist[i], name, namelength) == 0 &&
					namelength == strlen(namelist[i])) {
			/* matches */
			return(1);
		}
	}
	return(0);
}

/* Run Ghostscript to write width/ascent/descent information for all
 * characters. */

#ifdef __DOS__
/* List of possible names for Ghostscript to try to run */
char *gs_exe[] = {
	"gs386",
	"gs",
	"gswin32c",
	"gswin64c",
	0
};
#endif

void
run_Ghostscript(char *PostScript_name, char *Mup_name)
{
	char *	PS_option;
	char *	Mup_option;
#ifdef __DOS__
	char *	output_option;
	int	ret;
	int	e;
#endif
	int status;

	/* pass the arguments on to Ghostscript */
	PS_option = make_string("-sPostScript_name=", PostScript_name);
	Mup_option = make_string("-sMup_name=", Mup_name);
#ifdef __DOS__
	/* use temp file as a /dev/null */
	output_option = make_string("-sOutputFile=", GS_output);

	for (e = 0; gs_exe[e] != 0; e++) {
		if ((ret = spawnlp(P_WAIT, gs_exe[e], gs_exe[e], "-sDEVICE=bit",
				"-dQUIET", output_option, Mup_option,
				PS_option, "-", (char *) 0)) == 0) {
			break;
		}
	}
	unlink(GS_output);
	if (ret != 0) {
		fprintf(stderr, "failed to execute gs\n");
		cleanup(1);
	}
#else
	switch (fork()) {
	case 0:
		execlp("gs", "gs", "-sDEVICE=bit", "-dQUIET",
			"-sOutputFile=/dev/null", Mup_option,
			PS_option, "-", (char *) 0);
		/* FALL THROUGH */
	case -1:
		fprintf(stderr, "failed to execute gs\n");
		cleanup(1);
	default:
		wait( &status);
		if (status != 0) {
			fprintf(stderr, "Ghostscript failed\n");
			cleanup(1);
		}
	}
#endif
}

/* given two strings, get enough space to concatenate them,
 * write them into the malloc-ed string, and return it. */

char *
make_string(char *first_part, char *second_part)
{
	char *new_string;

	if ((new_string = (char *) malloc(strlen(first_part)
					+ strlen(second_part) + 1)) == 0) {
		fprintf(stderr, "malloc failed\n");
		cleanup(1);
	}
	sprintf(new_string, "%s%s", first_part, second_part);
	return(new_string);
}

/* This is the PostScript program that actually extracts the
 * font size information. It is included here and generated as needed,
 * so that this can be a standalone program, and not have to search
 * for another file in order to run. In Unix, we could just pipe this
 * directly into Ghostscript, but on systems that don't have pipes,
 * a temp file would need to be used, so we do it that way everywhere
 * for consistency.
 */

char *PostScript_program =
"%% This PostScript program generates a fontfile for use by Mup.\n"
"% PostScript_name and Mup_name must be defined as strings\n"
"% when this is called.\n"
"% PostScript_name is the font you want to add to Mup, while\n"
"% Mup_name is name of the Mup font you want to replace.\n"
"% So, for example, if you want to replace the Mup Helvetica roman\n"
"% font with the PostScript Helvetica-Narrow font, these strings would be\n"
"% (Helvetica-Narrow) and (helvetica rom).\n"
"% These can be passed in using the Ghostscript -s option.\n"
"\n"
"1 setflat		% make bounding box very accurate\n"
"\n"
"/buff 50 string def	% number to string conversion buffer\n"
"/character 1 string def	% buffer for a character to get the bbox of\n"
"\n"
"%------------------------------------------------------------------\n"
"\n"
"\n"
"% Usage\n"
"%	given a one-character string in \"character\",\n"
"%	outputs its width in 1/1000ths of an inch\n"
"\n"
"/getwidth {\n"
"	% get width of character\n"
"	character stringwidth\n"
"\n"
"	% convert x to 1/1000th of an inch\n"
"	pop 1000 mul 72 div round cvi\n"
"\n"
"	% print results\n"
"	buff cvs (\\t) print print\n"
"} def\n"
"\n"
"%-----------------------------------\n"
"% Usage\n"
"%	given a one-character string in \"character\",\n"
"%	outputs its height in 1/1000ths of an inch\n"
"\n"
"/getheight {\n"
"	% place character at (100, 100) and get its pathbbox\n"
"	newpath\n"
"	100 100 moveto\n"
"	character true charpath flattenpath pathbbox\n"
"\n"
"	% save the top and bottom y coordinates of the bbox\n"
"	/top exch def pop\n"
"	/bot exch def pop\n"
"\n"
"	% if bot is above the baseline, the height is (top - baseline)\n"
"	 % otherwise it is (top - bot)\n"
"	bot 100 gt { top 100 sub } { top bot sub } ifelse\n"
"\n"
"	% space is special, use 9 points for height\n"
"	character ( ) eq { 9 add } if\n"
"\n"
"	% add 2 point of padding, one for top and one for bottom white space,\n"
"	% and convert to 1/1000ths of an inch\n"
"	2 add 1000 mul 72 div round cvi\n"
"\n"
"	% print the results\n"
"	buff cvs (\\t) print print\n"
"} def\n"
"\n"
"%----------------------------------\n"
"% Usage\n"
"%	given a one-character string in \"character\",\n"
"%	outputs its ascent in 1/1000ths of an inch\n"
"\n"
"/getascent {\n"
"	% place character at (100, 100) and get its pathbbox\n"
"	newpath\n"
"	100 100 moveto\n"
"	character true charpath flattenpath pathbbox\n"
"\n"
"	% save the top y coordinate of the bbox\n"
"	/top exch def pop pop pop\n"
"\n"
"	% ascent is top minus baseline\n"
"	top 100 sub\n"
"\n"
"	% space is special, use 6.8 points for ascent\n"
"	character ( ) eq { 6.8 add } if\n"
"\n"
"	% add 1 point of padding and convert to 1/1000ths of an inch\n"
"	1 add 1000 mul 72 div round cvi\n"
"\n"
"	% print results\n"
"	buff cvs (\\t) print print\n"
"} def\n"
"\n"
"\n"
"%-----------------------------------\n"
"\n"
"% generate width, height an ascent for a font.\n"
"% Usage:\n"
"%	fname mupfname do_a_font\n"
"\n"
"/do_a_font {\n"
"	% save arguments for later use\n"
"	/mupfname exch def\n"
"	/fname exch def\n"
"\n"
"	% Outut heading\n"
"	(# This is a Mup font file\\n) print\n"
"	(Mup font name: ) print mupfname print (\\n) print\n"
"	(PostScript font name: ) print fname buff cvs print (\\n) print\n"
"	(Size data:\\n) print\n"
"\n"
"	% Set up to use the desired font\n"
"	fname findfont\n"
"	12 scalefont setfont\n"
"\n"
"	% Mup uses ASCII character codes from 32 through 126\n"
"	32 1 126 {\n"
"		dup buff cvs print\n"
"		/val exch def character 0 val put\n"
"		getwidth\n"
"		getheight\n"
"		getascent\n"
"		(\\t# ') print character print ('\\n) print\n"
"	} for\n"
"\n"
"	(PostScript:\\n) print\n"
"} def\n"
"\n"
"%-----------------------------------\n"
"\n"
"% generate the output\n"
"PostScript_name cvn Mup_name do_a_font\n"
"\n"
"quit\n";

void
generate_PostScript_program(char *PS_file)
{
	int file;
	int length;

	if ((file = open(PS_script_file, WRITE_FLAGS, 0644)) < 0) {
		fprintf(stderr, "Can't generate '%s'\n", PS_script_file);
		exit(1);
	}

	/* If user gave a PostScript file, that probably implements the
	 * font, so include that in the script */
	if (PS_file != (char *) 0) {
		pswrite(file, "(", 1);
		pswrite(file, PS_file, strlen(PS_file));
		pswrite(file, ") run\n", 6);
	}

	length = strlen(PostScript_program);
	pswrite(file, PostScript_program, length);
	close(file);
}

/* Write to PostScript file, checking return code, so compiler won't
 * complain about unused result (and to catch any errors).
 * Exits on write failure.
 */

void
pswrite(int file, char *data, int length)
{
	if (write(file, data, length) != length) {
		fprintf(stderr, "generation of PostScript program failed\n");
		cleanup(1);
	}
}

/* remove the temp file and exit */
 
void
cleanup(int exitcode)
{
	unlink(PS_script_file);
	exit(exitcode);
}
