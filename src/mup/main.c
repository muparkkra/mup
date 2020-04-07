char *license_text =
" Copyright (c) 1995-2020  by Arkkra Enterprises.\n\
 All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms,\n\
 with or without modification, are permitted provided that\n\
 the following conditions are met:\n\
\n\
 1. Redistributions of source code must retain\n\
 the above copyright notice, this list of conditions\n\
 and the following DISCLAIMER.\n\
\n\
 2. Redistributions in binary form must reproduce the above\n\
 copyright notice, this list of conditions and\n\
 the following DISCLAIMER in the documentation and/or\n\
 other materials provided with the distribution.\n\
\n\
 3. Any additions, deletions, or changes to the original files\n\
 must be clearly indicated in accompanying documentation,\n\
 including the reasons for the changes,\n\
 and the names of those who made the modifications.\n\
\n\
	DISCLAIMER\n\
\n\
 THIS SOFTWARE IS PROVIDED \"AS IS\" AND ANY EXPRESS\n\
 OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,\n\
 THE IMPLIED WARRANTIES OF MERCHANTABILITY\n\
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.\n\
 IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,\n\
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,\n\
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n\
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n\
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,\n\
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE)\n\
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,\n\
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\
"
;

static char Copyright[] =
	"Copyright (c) 1995-2020 by Arkkra Enterprises.\nMup is free software. Use -l option to see license terms.\n";

/* The file contains the main function for the Mup music publication program,
 * along with some functions that handle commmand line arguments and such. */

/*
 *		Command line arguments 
 * -cN	Combine strings of N measures of rest into multirests (N > 1)
 * -C		Include comments in macro-processed output (with -E)
 * -dn   turns on debug level n
 *		1 = yydebug
 *		2 = parse phase high level trace
 *		4 = parse phase low level trace
 *		8 = reserved
 *		16 = placement phase high level trace
 *		32 = placement phase low level trace
 *		64 = reserved
 *		128 = print contents of main linked list
 *		256 = print phase high level trace
 *		512 = print phase low level trace
 *	This is a bitmap, so multiple levels can be on at once
 * -DMACRO[=def] define a macro
 * -e errfile	write error output into errfile instead of stderr
 * -E		just do macro expansion
 * -f file	write output to file instead of stdout
 * -F		write output to file, deriving the name
 * -m midifile  generate MIDI output into specified file instead of the
 *		usual PostScript output to stdout.
 * -l		print license and exit
 * -M		create MIDI file, deriving the file name
 * -olist	print only pages given in list
 * -pN		start numbering pages at N instead of from 1.
 *			optionally followed by a comma plus leftpage or rightpage
 * -slist	print only the staffs in list
 * -v    	print verion number and exit
 * -xN,M	extract just measures N through M.
 *	Negative values are relative to the end of the song.
 *	The comma and second number are optional.
 *
 * Expects zero or more input files. If no file specified, reads stdin
 *
 * Exit code is 0 on success, or the number of errors found, up to 254,
 * or 255 for internal error.
 */


#ifdef __WATCOMC__
#include <io.h>
#endif
#ifdef Mac_BBEdit
#include <Files.h>
#include <Folders.h>
#include <MupInterface.h>
#define main _mup	/* rename entry point to _mup */
#endif
#include <errno.h>
#include <fcntl.h>
#include "defines.h"
#include "globals.h"


/* List of valid command line options and their explanations */
struct Options {
	char	option_letter;
	char	*argument;	/* describes the arg if any, or "" if none */
	char	*explanation;
} Option_list[] = {
	{ 'c', " N",		"combine N or more measures of rests into multirests" },
	{ 'C',	"",		"include comments in macro preprocessor output" },
	{ 'd', " N",		"turn on debug level N" },
	{ 'D', " MACRO[=macro_def]", "define macro" },
	{ 'e', " errfile",	"write error messages to errfile" },
	{ 'E', "",		"run macro preprocessor only" },
	{ 'f', " outfile",	"write output to outfile" },
	{ 'F', "",		"write output to file with derived name" },
	{ 'l', "",		"show license and exit" },
	{ 'm', " midifile",	"generate MIDI output file" },
	{ 'M', "",		"generate MIDI output file, derive file name" },
	{ 'o', " pagelist",	"only print pages in pagelist" },
	{ 'p', " N[,side]",	"start numbering pages at N; leftpage or rightpage" },
	{ 'q', "",		"quiet - don't print copyright notice" },
	{ 's', " stafflist",	"print only staffs in stafflist" },
	{ 'v', "",		"print version number and exit" },
	{ 'x', " N[,M]",	"extract measures N through M" }
};


#ifndef _UNISTD_H
/* to process command line args */
extern int getopt P((int argc, char * const *argv, const char *optstr));
extern int optind;	/* set by getopt to point to current cmd line argument */
extern char *optarg;	/* set by getopt */
extern char *getenv();
#endif
extern FILE *yyout;	/* lex could try to write error output here */

static char **Arglist;		/* global pointer to argv */
static int Num_args;		/* global copy of argc */
static char Version[] = "6.8";	/* Mup version number */
static int Quiet = NO;		/* -q option */

/* The different kinds of things that can be argument to -o option.
 * User values of "odd" and "even" will map to PG_ODD and PG_EVEN.
 * If neither of them are given, it will be PG_ALL
 * If the user doesn't specify a list of pages ranges,
 * we will create one range, from first page to last.
 */
#define PG_ALL		0
#define PG_ODD		1
#define PG_EVEN		2
static int Pglist_filter = PG_ALL;

static int Pages_reversed = NO;	/* YES if user said "reversed" in -o arg */

/* Pseudo page number for when user wants a blank page */
#define BLANK_PAGE	-999

#ifdef O_BINARY
static char * Read_mode = "rb";
#else
static char * Read_mode = "r";
#endif

/* If there is a list of pages to print using -o, the values are stored
 * in a list of RANGELIST structs. This points to that list. The "all" field
 * of the struct is unused.
 */
static struct RANGELIST *Page_range_p;

static void usage P((char **argv));	/* print usage message and exit */
static int ignore_option P((int opt));
static void notice P((void));
static char *derive_file_name P((char *suffix));
static int get_first_page P((int pagenum));
static void set_pagelist P((char *pagelist, int startpage));
static char *get_page_modifiers P((char *pagelist));
static char *get_pagelist P((char *pagelist, int startpage));
static struct RANGELIST **save_range P((int lower, int upper,
		struct RANGELIST **linkpoint_p_p));
static void prune_page_range P((int start_page));
static void vis_staffs P((char *stafflist));


int
main(argc, argv)

int argc;
char **argv;

{
	int a;			/* for command line args */
	char *midifilename = (char *) 0;
	int combine = NORESTCOMBINE;	/* number of measures to combine into
				 * multirests with -c option */
	int derive_out_name = NO;	/* YES is -F option is specified */
	char *vis_stafflist = (char *) 0;	/* -s list of visible staffs */
	int pagenum;
	int side = PGSIDE_NOT_SET;	/* optional second argument to -p */
	char *pagelist = 0;
	int start = 1, end = -1;	/* Arguments to -x option */
	int has_x_arg = NO;
	int outfile_args = 0;	/* we only allow one instance of [fFmM] options */
	int n, i;
	int num_options;
	char *getopt_string;
	/* The following three variables are to guard against infinite loops */
	struct RANGELIST *prev_page_range_p;
	int prev_begin = 0;
	int prev_end = 0;


	/* Initialize all the font metrics.
	 * This must happen before init_symtbl() */
	init_psfont_metrics();
	init_musfont_metrics();
	init_charinfo_table();

	/* must init head shapes table before first call to initstructs */
	init_symtbl();

	/* set initial page number to "not set" */
	pagenum = MINFIRSTPAGE - 1;
	initstructs();

	/* If run via mupmate, user may not understand error messages	
	 * about things like -c or -p, so we give different messages. */
	Mupmate = (getenv("MUPMATE") == 0 ? NO : YES);

	/* process command line arguments */
	/* create getopt string */
	num_options = NUMELEM(Option_list);
	/* allow for worst case of all requiring colon */
	MALLOCA(char, getopt_string, 2 * num_options + 1);
	for (n = i = 0; n < num_options; n++) {
		if (ignore_option( (int) Option_list[n].option_letter) == YES) {
			continue;
		}
		getopt_string[i] = Option_list[n].option_letter;
		if (Option_list[n].argument[0] != '\0') {
			getopt_string[++i] = ':';
		}
		i++;
	}
	getopt_string[i] = '\0';

	while ((a = getopt(argc, argv, getopt_string)) != EOF) {

		switch (a) {

		case 'c':
			combine = atoi(optarg);
			if (combine < MINRESTCOMBINE || combine > MAXRESTCOMBINE) {
				if (Mupmate == YES) {
					/* Should be impossible to get here,
					 * since mupmate refuses to accept
					 * out of range values. */
					l_yyerror(0, -1, "Run > Set Options > Min measures to combine: value must be between %d and %d.",
						MINRESTCOMBINE, MAXRESTCOMBINE);
				}
				else {
					l_yyerror(0, -1, "argument for %cc (number of measures to combine) must be between %d and %d",
						Optch, MINRESTCOMBINE, MAXRESTCOMBINE);
				}
			}
			break;

		case 'C':
			Ppcomments = YES;
			break;

		case 'd':
			Debuglevel = (int) strtol(optarg, (char **) 0, 0);
			break;

		case 'e':
			if (freopen(optarg, "w", stderr) == (FILE *) 0) {
				cant_open(optarg);
			}
			break;

		case 'E':
			Preproc = YES;
			break;

		case 'f':
			Outfilename = optarg;
			outfile_args++;
			break;

		case 'F':
			derive_out_name = YES;
			outfile_args++;
			break;

		case 'D':
			cmdline_macro(optarg);
			break;

		case 'l':
			printf("\nMup license:\n\n%s\n", license_text);
			exit(0);
			/*NOTREACHED*/
			break;

		case 'm':
			midifilename = optarg;
			/* FALLTHRU */
		case 'M':
			Doing_MIDI = YES;
			/* define "built-in" MIDI macro */
			cmdline_macro("MIDI");
			outfile_args++;
			break;

		case 'o':
			pagelist = optarg;
			break;

		case 'p':
			pagenum = atoi(optarg);
			if (pagenum < MINFIRSTPAGE || pagenum > MAXFIRSTPAGE) {
				if (Mupmate == YES) {
					/* Should be impossible to get here,
					 * since mupmate refuses to accept
					 * out of range values. */
					l_yyerror(0, -1, "Run > Set Options > First Page: value must be between %d and %d.",
						MINFIRSTPAGE, MAXFIRSTPAGE);
				}
				else {
					l_yyerror(0, -1, "argument for %cp (first page) must be between %d and %d",
						Optch, MINFIRSTPAGE, MAXFIRSTPAGE);
				}
			}

			/* Skip past the page number to check for optional side */
		 	for (n = 0; optarg[n] != '\0'; n++) {
				if ( ! isdigit(optarg[n]) ) {
					break;
				}
			}
			if (optarg[n] != '\0') {
				if (optarg[n] != ',') {
					l_yyerror(0, -1, "for -p, expecting comma between page number and side");
				}
				else {
					/* Skip past the comma and any spaces */
					n++;
					while (optarg[n] == ' ') {
						n++;
					}

					if (strcmp(optarg + n, "leftpage") == 0) {
						side = PGSIDE_LEFT;
					}
					else if (strcmp(optarg + n, "rightpage") == 0) {
						side = PGSIDE_RIGHT;
					}
					else {
						l_yyerror(0, -1, "-p side specification must be leftpage or rightpage");
					}
				}
			}
			break;

		case 'q':
			Quiet = YES;
			break;
		case 's':
			vis_stafflist = optarg;
			break;

		case 'v':
			notice();

			(void) fprintf(stderr,"Version %s\n", Version);
			exit(0);
			/*NOTREACHED*/
			break;

		case 'x':
			chk_x_arg(optarg, &start, &end);
			has_x_arg = YES;
			break;

		default:
			usage(argv);
			break;
		}
	}

	notice();

	if (Ppcomments == YES && Preproc == NO) {
		warning("-C only valid with -E; ignored");
	}

	if (Preproc == YES && vis_stafflist != 0) {
		warning("-s not valid with -E; ignored");
	}

	if (outfile_args > 1) {
		(void) fprintf(stderr, "Only one output file option (-f, -F, -m, -M) can be specified\n");
		exit(1);
	}

	/* turn on yacc debug flag if appropriate */
	if (Debuglevel & 1) {
		yydebug = 1;
		ifdebug = 1;
	}

	/* save info about arguments so yywrap can open additional input files
	 * if necessary */
	Arglist = argv;
	Num_args = argc;
	yyin = stdin;
	yyout = stderr;

	/* if file argument, open that, else use stdin */
	if (optind <= argc - 1) {
		(void) yywrap();
	}
	else {
#ifdef Mac_BBEdit
		Curr_filename  = _mup_input_filename;
#else
		Curr_filename = "stdin";
#if defined(unix) || defined(__WATCOM__)
		/* Sometimes people forget to give a file name,
		 * then wonder why Mup is "hanging," so let user
		 * know it isn't hanging... it's waiting for them
		 * to type something. But only if input is a terminal,
		 * and stderr is a terminal--if stdin is a pipe,
		 * user probably doesn't need a reminder. */
		if (isatty(0) && isatty(2)) {
			fprintf(stderr, "No input file specified; reading standard input.\n\n");
		}
#endif
#endif
	}

	/* initialize for parser */
	raterrfuncp = doraterr;
	initstructs();
	vis_staffs(vis_stafflist);
	reset_ped_state();

	/* parse the input */
	if (Preproc == YES) {
		preproc();
	}
	else {
		(void) yyparse();
	}
#ifndef UNIX_LIKE_FILES
	mac_cleanup();
#endif

	/* Apply keymaps. This has to happen before calc_block_heights so
	 * that that function is using the mapped strings */
	if (Errorcount == 0) {
		map_all_strings();
	}

	/* do final checks and cleanup of input data */
	/* check for missing endif */
	chk_ifdefs();	
	if (Preproc == YES) {
		error_exit();
	}

	/* Set Firstpageside, taking -p option and SSVs into account
	 * as appropriate. This needs to be done before calling
	 * calc_block_heights, to populate the left/right versions it needs. */
	Firstpageside = set_firstpageside(side);

	/* find height of headers and footers */
	/* Note: this has to be called when we are at the *end* of the main
	 * list with all SSVs applied, so that we know the margin settings */
	/* Skip this if there were errors, in case one of the errors involved
	 * an invalid expression. */
	if (Errorcount == 0) {
		calc_block_heights();
	}

	/* make sure there is a final barline */
	check4barline_at_end();
	/* make sure we go to new score if visibility changes */
	chk_vis_feed();

	/* derive tabnote staff data for tablature staffs. But if there
	 * have been errors found, don't bother, because we may have
	 * some incomplete/inconsistent data that tab2tabnote doesn't
	 * know how to deal with cleanly. */
	if (Errorcount == 0) {
		tab2tabnote();
	}

	/* Convert ph-eph pairs into phrase STUFFs */
	conv_ph_eph();

	/* do -c option or restcombine parameter */
	combine_rests(combine);

	/* make sure there aren't til clauses past end of song */
	chk4dangling_til_clauses("the end of the song");

	/* count how many verses */
	set_maxverses();

	/* process ties */
	tie();

	/* Verify that -o argument (and maybe -p or firstpage parameter)
	 * is valid. If not, this will ufatal. */
	pagenum = get_first_page(pagenum);
	set_pagelist(pagelist, pagenum);

	/* Do -x (extract) option if needed. But if there were errors before,
	 * skip this, because there could be empty measures and such,
	 * that could confuse it, and we're going to give up soon anyway. */
	if (has_x_arg == YES && Errorcount == 0) {
		extract(start, end);
	}

	debug(2, "finished with parsing, Errorcount is %d", Errorcount);

	if (Errorcount > 0) {
		(void) fprintf(stderr, "\nstopping due to previous error%s\n",
						Errorcount ? "s" : "");
		error_exit();
	}

	/* do the placement phase */

	/* initialize the Staffscale and related variables to default values */
	initstructs();
	set_staffscale(0);
	Ignore_staffscale = YES;

	/* transpose */
	transgroups();

	/* set up ties that carry into next measure */
	tie_carry();

	if (Doing_MIDI == YES) {
		/* For MIDI, expand any repeated sections as if the user
		 * had written them out fully without repeats. */
		expand_repeats();
	}

	/* line up chords */
	makechords();

	/* place notes relative to staff and set stem direction */
	setnotes();	
	/* find relative horizontal position of notes */
	setgrps();
	/* set coordinates of rests and syllables */
	restsyl();

	/* generate MIDI file if appropriate. We wait until here to
	 * do MIDI, so that chord widths have been established, so midi
	 * code can more easily figure out how to crunch all-space chords */
	if (Doing_MIDI == YES) {
		if (midifilename == (char *) 0) {
			/* -M option, so we have to derive the name */
			midifilename = derive_file_name(".mid");
		}
		gen_midi(midifilename);
		exit(0);
	}

	/* figure out absolute horizontal locations */
	abshorz();
	/* find lengths of beams, angles of beams, etc */
	beamstem();
	/* set up mussym, octave, rom, bold, pedal, etc */
	stuff();

	/* find vertical coordinates relative to staff */
	relvert();
	/* set absolute vertical coordinates */
	absvert();

	/* split lines and curves */
	fix_locvars();	

	/* If debugging bit 128 is on, dump the main list */
	print_mainll();

	if (derive_out_name == YES) {
		Outfilename = derive_file_name(".ps");
	}
	if (*Outfilename != '\0') {
		if (freopen(Outfilename, "w", stdout) == (FILE *) 0) {
			cant_open(Outfilename);
			exit(1);
		}
	}

	/* output PostScript for printing */
	prune_page_range(pagenum);

	/* Initialize infinite loop guards */
	prev_page_range_p = Page_range_p;
	if (Page_range_p != 0) {
		prev_begin = Page_range_p->begin;
		prev_end = Page_range_p->end;
	}
	
	do {
		Pagenum = (short) pagenum;
		print_music();
		/* As a guard against infinite loop bugs, we make sure
		 * that each time through this loop we make some progress.
		 * We need to either have moved on to a new range or have
		 * reduced the current range. */
		if (Page_range_p != 0) {
			if (prev_page_range_p == Page_range_p
					&& prev_begin == Page_range_p->begin
					&& prev_end == Page_range_p->end) {
				pfatal("infinite loop in finding pages to print");
			}
			else {
				prev_page_range_p = Page_range_p;
				prev_begin = Page_range_p->begin;
				prev_end = Page_range_p->end;
			}
		}
	} while (Page_range_p != 0);
	trailer();

	/* if we get to here, all is okay. If there was a problem,
	 * we would have exited where the problem occurred */
	return(0);
}


/* print copyright notice */

static void
notice()

{
	if (getenv("MUPQUIET") == (char *) 0 && Quiet == NO) {
		(void) fprintf(stderr, "Mup - Music Publisher   Version %s\n", Version);
		(void) fputs(Copyright, stderr);
	}
}


/* print usage message and exit */

static void
usage(argv)

char **argv;

{
	int num_options;	/* how many options */
	int n;
	char *whitespace;	/* for lining things up */
	int white_length;	/* strlen(whitespace) */
	int length;		/* of an argument item */
	char *extra_options;	/* parent process can ask us to print more */


	notice();
	/* print the usage summary */
	fprintf(stderr, "usage: %s ", argv[0]);
	num_options = NUMELEM(Option_list);
	for (n = 0; n < num_options; n++) {
		if (ignore_option( (int) Option_list[n].option_letter) == YES) {
			/* ignore this option */
			continue;
		}
		fprintf(stderr, "[%c%c%s] ", Optch,
			Option_list[n].option_letter, Option_list[n].argument);
	}
	(void) fputs("[file...]\n", stderr);

	/* We'll add as much of this whitespace string to each argument
	 * item as needed to line the explanations up nicely. */
	whitespace = "                  ";
	white_length = strlen(whitespace);

	/* print the explanations of each option */
	for (n = 0; n < num_options; n++) {

		if (ignore_option( (int) Option_list[n].option_letter) == YES) {
			continue;
		}

		fprintf(stderr, "   %c%c%s", Optch,
			Option_list[n].option_letter, Option_list[n].argument);

		/* add enough white space to line things up */
		if ((length = strlen(Option_list[n].argument)) < white_length) {
			fprintf(stderr, "%s", whitespace + length);
		}

		fprintf(stderr, " %s\n", Option_list[n].explanation);
	}
	/* If calling program tells us to add some options to the list,
	 * print those out too. */
	if ((extra_options = getenv("MUPADDOP")) != (char *) 0) {
		fprintf(stderr, "%s", extra_options);
	}

	exit(1);
}


/* If Mup is being called by some other program, like mupdisp,
 * such that some of Mup's options should be disallowed, it
 * should set $MUPDELOP to the list of options to be deleted
 * from the list of valid options. This function will say, for the
 * given option, whether it should be disallowed. */

static int
ignore_option(opt)

int opt;	/* an option letter */

{
	static char *del_options = 0;	/* which options to delete from list */

	/* the first time we are called, get the list, if any */
	if (del_options == (char *) 0) {
		if ((del_options = getenv("MUPDELOP")) == (char *) 0) {
			del_options = "";
		}
	}

	return ((strchr(del_options, opt) != (char *) 0) ? YES : NO);
}


/* make our own yywrap rather than use the one in the lex library.
 * In case user specifies more than one file, open
 * each in turn, and return control to lex */

int
yywrap()

{
	int leng = 0;	/* Length of file name. Initialization done solely
			 * to avoid bogus "used before set" warning. */

	/* return from any macros or includes */
	if (popfile() == 1) {
		return(0);
	}

	/* if user specified more files, open the next one */
	for (  ; optind < Num_args; optind++) {
		if (yyin != NULL) {
			(void) fclose(yyin);
		}
		errno = 0;
		if ((yyin = fopen(Arglist[optind], Read_mode)) != NULL) {
			Curr_filename = Arglist[optind++];
			yylineno = 1;
			return(0);
		}
		/* If name doesn't already end with .mup or .MUP and the open
		 * failed because the file didn't exist, try the name with
		 * .mup appended. */
		else if (
#ifdef ENOENT
				errno == ENOENT &&
#endif
				( ((leng = strlen(Arglist[optind])) < 5) ||
				(strcmp(Arglist[optind] + leng - 4, ".mup") != 0 &&
				strcmp(Arglist[optind] + leng - 4, ".MUP") != 0
				)) ) {
			MALLOCA(char, Curr_filename, leng + 5);
			sprintf(Curr_filename, "%s.mup", Arglist[optind]);
			if ((yyin = fopen(Curr_filename, Read_mode)) != NULL) {
				yylineno = 1;
				optind++;
				return(0);
			}
			/* try upper case suffix before giving up */
			sprintf(Curr_filename, "%s.MUP", Arglist[optind]);
			if ((yyin = fopen(Curr_filename, Read_mode)) != NULL) {
				yylineno = 1;
				optind++;
				return(0);
			}
			FREE(Curr_filename);
		}
		cant_open(Arglist[optind]);
	}

	return(1);
}


/* If user used -M or -F option, we need to derive the output file name.
 * Use the last input file name, strip off the trailing .mup if it is there,
 * add the suffix, and return the derived name.
 */

static char *
derive_file_name(suffix)

char *suffix;		/* ".mid" or ".ps" */

{
	int length;		/* of Curr_filename */
	char *file_name;	/* the name we derive */
	char *suffix_location;	/* where the suffix will go */


	length = strlen(Curr_filename);
	MALLOCA(char, file_name, length + strlen(suffix) + 1);

	/* start with the original Mup input file name */
	strcpy(file_name, Curr_filename);

	/* see if we need to strip off a .mup */
	if (length > 3) {
		/* find where the .mup would start if it is there */
		suffix_location = file_name + length - 4;

		/* If user used upper case, so will we */
		if (strcmp(suffix_location, ".MUP") == 0) {
			if (strcmp(suffix, ".mid") == 0) {
				suffix = ".MID";
			}
			else if (strcmp(suffix, ".ps") == 0) {
				suffix = ".PS";
			}
			else {
				pfatal("derive_file_name() called with unknown suffix '%s'", suffix);
			}
		}
		else if (strcmp(suffix_location, ".mup") != 0) {
			/* no .mup to strip off; just add to the end */
			suffix_location = file_name + length;
		}
	}
	else {
		suffix_location = file_name + length;
	}

	/* append the suffix and return the derived name */
	strcpy(suffix_location, suffix);
	return(file_name);
}


/* Determine the first page number. If user used -p option, use that,
 * otherwise get from first_page parameter, else use 1. */

static int
get_first_page(pagenum)

int pagenum;		/* from -p option */

{
	struct MAINLL *m_p;

	/* if there wasn't a -p value, figure out what to use for first page */
	if (pagenum < MINFIRSTPAGE) {
		/* default to page 1 */
		pagenum = 1;

		/* look for last setting of firstpage parameter before
		 * any STAFFs */
		initstructs();
		for (m_p = Mainllhc_p; m_p != 0; m_p = m_p->next) {
			if (m_p->str == S_SSV) {
				asgnssv(m_p->u.ssv_p);
				if (Score.firstpage != NOFIRSTPAGE) {
					pagenum = Score.firstpage;
				}
			}
			else if (m_p->str == S_STAFF) {
				break;
			}
		}
	}
	return(pagenum);
}


/* Parse the argument to -o, if there is a -o option specified, and
 * save the info away for later use. Gives error if argument is invalid.
 */

static void
set_pagelist(pagelist, startpage)

char *pagelist;
int startpage;	/* from -p option */

{
	if (pagelist == (char *) 0) {
		/* no -o option, print all pages */
		Pglist_filter = PG_ALL;
		Pages_reversed = NO;
		return;
	}

	/* We allow odd/even or reversed to be at either end of the list */
	pagelist = get_page_modifiers(pagelist);
	pagelist = get_pagelist(pagelist, startpage);
	pagelist = get_page_modifiers(pagelist);

	/* Make sure we got to the end with valid results. */
	if (*pagelist != '\0') {
		if (Mupmate == YES) {
			l_yyerror(0, -1, "Run > Set Options > Pages to display: value is invalid.");
		}
		else {
			l_yyerror(0, -1, "argument for -o (list of pages to display) is invalid");
		}
	}
}


/* Look for odd or even or reversed in the given page list.
 * Sets the appropriate globals if it finds them, checking for contradictions.
 * We allow these to be at the beginning or end of the -o argument,
 * so this gets called twice.
 * Each time it is passed where to start parsing and returns the point
 * where it found something it didn't know how to handle.
 */

static char *
get_page_modifiers(pagelist)

char *pagelist;

{
	char *start_pagelist;	/* Where we were in pagelist at the top of
				 * the "do" loop, to tell if we parsed
				 * something, and should keep going,
				 * or should give up looking, and return. */

	do {
		start_pagelist = pagelist;
		if (strncmp(pagelist, "reversed", 8) == 0) {
			pagelist += 8;
			if (Pages_reversed == YES) {
				warning("reversed specified more than once to -o option");
			}
			Pages_reversed = YES;
		}

		else if (strncmp(pagelist, "odd", 3) == 0) {
			pagelist += 3;
			if (Pglist_filter == PG_ODD) {
				warning("odd specified more than once to -o");
			}
			if (Pglist_filter == PG_EVEN) {
				warning("both odd and even specified to -o; using last");
			}
			Pglist_filter = PG_ODD;
		}

		else if (strncmp(pagelist, "even", 4) == 0) {
			pagelist += 4;
			if (Pglist_filter == PG_EVEN) {
				warning("even specified more than once to -o");
			}
			if (Pglist_filter == PG_ODD) {
				warning("both odd and even specified to -o; using last");
			}
			Pglist_filter = PG_EVEN;
		}
		pagelist += strspn(pagelist, ", \t");
	/* Keep going till we get to something we don't recognize. */
	} while (pagelist > start_pagelist);
	return(pagelist);
}


/* Parse and save ranges arguments to -o */

static char *
get_pagelist(pagelist, startpage)

char *pagelist;
int startpage;

{
	struct RANGELIST **linkpoint_p_p;/* tail of Page_range_p list,
					 * for linking to the end of the list */
	char *p, *beyondnum_p;		/* for parsing the numbers */
	long lower, upper;		/* page number range */
	short value_as_short;		/* lower/upper ultimately get saved
					 * into shorts, so this is used to
					 * try to check if they will fit. */


	/* First set up where to link onto tail of list */
	linkpoint_p_p = &Page_range_p;

	/* walk through the -o argument */
	for (p = pagelist; *p != '\0';   ) {

		/* Skip any leading white space */
		while (isspace(*p)) {
			p++;
		}

		if (strncmp(p, "blank", 5) == 0) {
			linkpoint_p_p = save_range(BLANK_PAGE, BLANK_PAGE, linkpoint_p_p);
			p += 5;
			while (isspace(*p)) {
				p++;
			}
			if (*p == ',') {
				p++;
			}
			continue;
		}

		/* Get page number (which may or may not be the
		 * start of a range of numbers) */
		lower = strtol(p, &beyondnum_p, 10);
		value_as_short = (short) lower;
		if (beyondnum_p == p || lower <= 0 || lower < startpage
					|| lower != value_as_short ) {
			/* bad number from user, jump to error out */
			break;
		}

		p = beyondnum_p;
		/* Skip any white space */
		while (isspace(*p)) {
			p++;
		}

		if (*p == '-') {
			/* There is a range of page numbers.
			 * Get the upper limit of the range */
			upper = strtol(++p, &beyondnum_p, 10);
			value_as_short = (short) upper;
			if (beyondnum_p == p || upper <= 0 || upper < lower	
						|| upper != value_as_short) {
				/* bad value from user */
				break;
			}
			p = beyondnum_p;
			while (isspace(*p)) {
				p++;
			}
			if (*p == ',') {
				p++;
			}
			else if (*p != '\0') {
				break;
			}
		}
		else if (*p == ',') {
			/* not a range, so treat like range of n-n */
			upper = lower;
			p++;
		}
		else if (*p == '\0') {
			upper = lower;
		}
		else {
			/* Something other than dash, comma, or end of
			 * string, which is either a modifier or a user error.
			 * In either case, we are done here. */
			break;
		}

		/* save info about this page range */
		linkpoint_p_p = save_range((int)lower, (int)upper, linkpoint_p_p);
	}

	return(p);
}


/* Given a lower and upper page range, allocate a struct to save them in,
 * and link it onto the Page_range_p list at the given place.
 */

static struct RANGELIST **
save_range(lower, upper, linkpoint_p_p)

int lower;
int upper;
struct RANGELIST **linkpoint_p_p;

{
	struct RANGELIST *new_range;

	/* save info about this page range */
	MALLOC(RANGELIST, new_range, 1);
	new_range->begin = (short) lower;
	new_range->end = (short) upper;
	new_range->next = (struct RANGELIST *) 0;

	/* link onto tail of list */
	*linkpoint_p_p = new_range;
	return (&(new_range->next));
}


/* Calculate the page number for the final page and put it in Last_pagenum.
 * If there is a -o list, make sure all the
 * pages listed on the -o list are less than that. If they aren't remove them
 * from the list. Without this step, Mup could go into a loop trying to print
 * a page that doesn't exist. */

static void
prune_page_range(start_page)

int start_page;		/* number given to the first page via the -p option
			 * or via the firstpage parameter */

{
	struct MAINLL *mll_p;	/* to count page feeds */
	struct RANGELIST **range_p_p;
	int pruned;		/* if we removed anything from list */

	/* find the largest page number */
	Last_pagenum = start_page;
	for (mll_p = Mainllhc_p; mll_p != (struct MAINLL *) 0; mll_p = mll_p->next) {
		if (mll_p->str == S_FEED && mll_p->u.feed_p->pagefeed == YES
						&& mll_p->next != 0) {
			Last_pagenum++;
		}
	}

	/* If there are extra pages for gridsatend, add those on */
	if (Atend_info.separate_page == YES) {
		int side;		/* PGSIDE_* value */
		int grids_so_far;	/* how many grids have been accounted
					 * for so far */
		int grids_this_page;	/* How many grids on current page */

		/* Figure out whether the first pages of atend grids is
		 * a left or right page. If the odd/even-ness of the
		 * first page of grids is the same as that of the start
		 * page, then the side is also the same as that of the start,
		 * otherwise it is the opposite. */
		if ( ((Last_pagenum + 1) & 0x1) == (start_page & 0x1)) {
			side = Firstpageside;
		}
		else {
			side = (Firstpageside == PGSIDE_LEFT
					? PGSIDE_RIGHT : PGSIDE_LEFT);
		}

		/* Keep pretending to fill pages with grids until we run out.
		 * That will tell us how many pages are needed for them. */
		for (grids_so_far = 0; grids_so_far < Atend_info.grids_used;
					grids_so_far += grids_this_page) {
			Last_pagenum++;
			if (side == PGSIDE_LEFT) {
				grids_this_page = Atend_info.grids_per_row *
						Atend_info.left.rows_per_page;
				/* The next page, if any,  will be a right, */
				side = PGSIDE_RIGHT;
			}
			else {
				grids_this_page = Atend_info.grids_per_row *
						Atend_info.right.rows_per_page;
				side = PGSIDE_RIGHT;
			}
		}
	}

	/* If no page ranges specified by user, create a page range that
	 * goes from first page to last page. */
	if (Page_range_p == 0) {
		save_range(start_page, Last_pagenum, &Page_range_p);
	}
	/* see if any items in Page_range are bigger
	 * than the biggest page number */
	pruned = NO;
	for (range_p_p = &Page_range_p; *range_p_p != (struct RANGELIST *) 0;
					range_p_p = &((*range_p_p)->next) ) {
		/* If only doing every other page, adjust the range ends.
		 * If the begin is the wrong type, we round it up.
		 * If the end is the wrong type, we round it down.
		 * That's because we want to stay within the range.
		 */
		if (Pglist_filter == PG_ODD) {
			(*range_p_p)->begin |= 01;
			(*range_p_p)->end = ((*range_p_p)->end - 1) | 01;
		}
		else if (Pglist_filter == PG_EVEN) {
			(*range_p_p)->begin = ((*range_p_p)->begin + 1) & ~01;
			(*range_p_p)->end &= ~01;
		}
		/* That may have made the whole range unnecessary */
		if ((*range_p_p)->begin > (*range_p_p)->end) {
			if ((*range_p_p = (*range_p_p)->next) == 0) {
				/* last one on the list */
				break;
			}
		}

		if ((*range_p_p)->begin > Last_pagenum) {
			/* need to get rid of this entire entry, because none
			 * of the pages listed actually exist */
			pruned = YES;
			if ((*range_p_p = (*range_p_p)->next)
						== (struct RANGELIST *) 0) {
				/* last one on the list */
				break;
			}
		}
		else if ((*range_p_p)->end > Last_pagenum) {
			/* just need to shorten this range */
			(*range_p_p)->end = Last_pagenum;
			pruned = YES;
		}
	}

	if (pruned == YES) {
		l_warning( (char *) 0, -1, "-o list included one or more pages that don't exist");
	}

	if (Page_range_p == 0) {
		warning("-o option resulted in no pages to print");
	}

	if (Pages_reversed == YES) {
		/* Reverse the order of the Page_range_p list,
		 * by pointing each ->next to what had pointed to it. */
		struct RANGELIST *next, * prev;
		struct RANGELIST *curr_p;		
		prev = 0;
		next = 0;
		for (curr_p = Page_range_p; curr_p != 0; curr_p = next) {
			next = curr_p->next;
			curr_p->next = prev;
			prev = curr_p;
		}
		Page_range_p = prev;
	}
}


/* given a page number, return YES if that page should be printed now, NO
 * if not. If user gave a list of pages to print using -o, we print the page
 * only if it is the very first thing on the list. If there is a smaller
 * number further on in the list, we'll do that page later on another pass.
 * The print phase has to keep making multiple passes until the list is
 * empty. This allows user to print things out in random order, which may
 * be useful especially for 2-on-1 printing, where for example, you may
 * want a 4-page "booklet", printing page 4 then page 1 on one side and
 * pages 2 and 3 on the other side.
 */

int
onpagelist(pagenum)

int pagenum;

{
	struct RANGELIST *old_range;	/* to keep track of item to free */

	/* Print any blank pages. */
	while (Page_range_p != 0 && Page_range_p->begin == BLANK_PAGE) {
		old_range = Page_range_p;
		Page_range_p = Page_range_p->next;
		FREE(old_range);
		print_blank_page();
	}

	if (Page_range_p == 0) {
		return(NO);
	}

	/* If this page is the next on the list, print it.
	 * But first, fix up the list, by adjusting
	 * the current range, and if that range is all
	 * handled, by freeing that one and pointing to the next.
	 */
	if (Pages_reversed == NO && Page_range_p->begin == pagenum) {
		/* Go to next page to print. If we are only printing odd
		 * or even, that requires skipping a page. */
		Page_range_p->begin += (Pglist_filter == PG_ALL ? 1 : 2);

		if (Page_range_p->begin > Page_range_p->end) {
			old_range = Page_range_p;
			Page_range_p = Page_range_p->next;
			FREE(old_range);
		}
		return(YES);
	}
	else if (Pages_reversed == YES && Page_range_p->end == pagenum) {
		Page_range_p->end -= (Pglist_filter == PG_ALL ? 1 : 2);
		if (Page_range_p->end < Page_range_p->begin) {
			old_range = Page_range_p;
			Page_range_p = Page_range_p->next;
			FREE(old_range);
		}
		return(YES);
	}
	return(NO);
}


/* Return YES if we have printed all the page the user asked us to. */

int
last_page()
{
	return ((Page_range_p == 0) ? YES : NO);
}


/* handle the argument to -s (list of staffs to make visible). For each
 * visible staff, make an SSV marking it visible */

static void
vis_staffs(stafflist)

char *stafflist;
{
	int s;				/* staff index */
	int v;				/* voice index */
	long start, end;		/* staff range */


	if (stafflist == (char *) 0) {
		/* user didn't use -s, so set to all visible */
		for (s = 1; s <= MAXSTAFFS; s++) {
			Staff_vis[s] = YES;
			for (v = 1; v <= MAXVOICES; v++) {
				Voice_vis[s][v] = YES;
			}
		}
		return;
	}

	/* init to all invisible */
	for (s = 1; s <= MAXSTAFFS; s++) {
		Staff_vis[s] = NO;
		for (v = 1; v <= MAXVOICES; v++) {
			Voice_vis[s][v] = NO;
		}
	}

	for (  ; *stafflist != '\0';   ) {
		/* get first staff number in list. Will error check below */
		start = strtol(stafflist, &stafflist, 10);

		if (*stafflist == '-') {
			/* we have a range. Get end of range */
			end = strtol(stafflist + 1, &stafflist, 10);
		}
		else {
			/* single number, use end same as start */
			end = start;
		}

		/* error check */
		if (start < 1 || start > MAXSTAFFS || end < 1 ||
					end > MAXSTAFFS || end < start) {
			if (Mupmate == YES) {
				l_yyerror(0, -1, "Run > Set Options > Staffs to display/play: value is invalid.");
			}
			else {
				l_yyerror(0, -1, "invalid argument for %cs option (staffs to make visible)", Optch);
			}
			return;
		}

		/* see if there is a voice qualifier */
		if (*stafflist == 'v') {
			stafflist++;
			switch (*stafflist) {
			case '1':
				v = 1;
				break;
			case '2':
				v = 2;
				break;
			case '3':
				v = 3;
				break;
			default:
				if (Mupmate == YES) {
					l_yyerror(0, -1, "Run > Set Options > Staffs to display/play: voice qualifier must be 1, 2, or 3.");
				}
				else {
					l_yyerror(0, -1, "voice qualifier for -s option must be 1, 2, or 3");
				}
				return;
			}
			stafflist++;
			if (*stafflist != '\0' && *stafflist != ',') {
				if (Mupmate == YES) {
					l_yyerror(0, -1, "Run > Set Options > Staffs to display/play: invalid voice qualifier. (Maybe missing comma?)");
				}
				else {
					l_yyerror(0, -1, "invalid voice qualifier for -s option (missing comma?)");
				}
				return;
			}
		}
		else {
			/* no voice qualifier */
			v = 0;
		}

		/* mark all staffs in range as visible */
		for (  ; start <= end; start++) {
			Staff_vis[start] = YES;
			if (v != 0) {
				Voice_vis[start][v] = YES;
			}
			else {
				/* no voice qualifier, so all voices are visible */
				int vn;
				for (vn = 1; vn <= MAXVOICES; vn++) {
					Voice_vis[start][vn] = YES;
				}
			}
		}

		/* if comma for another range, skip past it */
		if (*stafflist == ',') {
			stafflist++;
		}
	}
}


#ifdef NEED_GETOPT
/* for non-unix or other systems that don't have a getopt() function,
 * define one here. This is NOT a general purpose implementation of getopt(),
 * but something good enough to work with Mup */

int optind = 1;
char *optarg;
static int argoffset;
int opttype P((int option, char *optstring));

#define NOARG 1
#define WITHARG	2
#define BADOPT  3

int
getopt(argc, argv, optstring)

#ifdef __STDC__
int argc;
char * const *argv;
const char *optstring;
#else
int argc;
char **argv;
char *optstring;
#endif

{
	int option;


	if (optind >= argc) {
		return(EOF);
	}

	if (argoffset == 0) {
#ifdef __DOS__
		if (argv[optind][argoffset] == '-'
					|| argv[optind][argoffset] == '/') {
#else
		if (argv[optind][argoffset] == '-') {
#endif
			argoffset = 1;
		}
		else {
			return(EOF);
		}
	}

	/* determine if option is valid and if should have an argument */
	option = argv[optind][argoffset] & 0x7f;
	switch (opttype(option, (char *) optstring)) {
	case NOARG:
		/* valid option without argument. Keep track of where
		 * to look for next option */
		if (argv[optind][++argoffset] == '\0') {
			optind++;
			argoffset = 0;
		}
		break;

	case WITHARG:
		/* valid option with argument. */
		if (argv[optind][++argoffset] != '\0') {
			/* argument immediately follows in same argv */
			optarg = &(argv[optind][argoffset]);
			optind++;
		}
		else {
			/* white space. argument must be in next argv */
			optind++;
			if (optind >= argc) {
				fprintf(stderr, "missing argument to %c%c option\n", Optch, option);
				return('?');
			}
			optarg = &(argv[optind][0]);
			optind++;
		}
		argoffset = 0;
		break;

	default:
		fprintf(stderr, "invalid option %c%c\n", Optch, option);
		option = '?';
	}
	return(option);
}


/* look up option in optstring and return type of option */

int
opttype(option, optstring)

int option;
char *optstring;

{
	char *p;

	for (p = optstring; *p != '\0'; ) {
		if (*p++ == option) {
			return(*p == ':' ? WITHARG : NOARG);
		}
		if (*p == ':') {
			p++;
		}
	}
	return(BADOPT);
}

#endif
