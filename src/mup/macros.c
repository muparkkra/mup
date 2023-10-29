
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

/* This file contains functions to support Mup macros.
 * When a macro is defined, its text is copied to a file. When a macro is
 * invoked, information about the current input file is pushed on a stack,
 * and the macro text is read from the macro file. This file also has the
 * code to handle include files. They are handled similarly. Info about the
 * current file is pushed on a stack, and text is read from the included
 * file. */

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#ifndef __DJGPP__
#include <fenv.h>
#endif
#include "globals.h"

#ifdef VMS
#define unlink delete
#endif

/* size of macro name hash table. should be prime, big enough to not have too
 * many collisions, small enough to not use too much memory. */
#define MTSIZE	(67)

/* how many bytes to allocate at a time when collecting macro arguments */
#define MAC_ARG_SZ	(512)

/* Maximum length of a macro name that is stitched together from
 * other macros. */
#define MAX_CONCAT_NAME_LEN 1000

/* States for "eval" macro: not an eval macro at all, parsing, or setting */
#define NOT_EXPR	(0)
#define PARSING_EXPR	(1)
#define SETTING_EXPR	(2)
/* A macro without parameters is defined without a following parenthesis,
 * and is different than a macro with zero parameters. This value must not be
 * a valid value for number of parameters, so must be negative. */
#define WITHOUT_PARAMS	(-1)

/* information about a macro parameter */
struct MAC_PARAM {
	char	*param_name;	/* name of the parameter */
	struct MAC_PARAM *next;	/* for linked list */
};

/* info about a macro */
struct MACRO {
	char	*macname;	/* name of macro */
	char	*filename;	/* file in which macro was defined */	
	int	lineno;		/* line in file where macro definition began */
	int	lineoffset;	/* how many lines we are into macro */
	long	offset;		/* offset in macro temporary file where the
				 * text of the macro is stored for later use */
	long	quoted_offset;	/* offset into macro temp file where the
				 * quoted version is stored, if any. */
	struct MACRO 	*next;	/* for hash collision list */
	int	recursion;	/* incremented each time the macro is called,
				 * and decremented on completion. If this gets
				 * above 1 we are in trouble and ufatal */
	int	num_params;	/* We need to distinguish macros
				 * with empty parameter lists
				 * (e.g., define X() @) from macros
				 * with no parameters (e.g., define X @),
				 * so we represent the latter by
				 * WITHOUT_PARAMS. */
	struct	MAC_PARAM *parameters_p;	/* list of macro parameter
				 * names. Null if this macro doesn't
				 * have parameters */
};

/* macro information hash table */
static struct MACRO *Mactable[MTSIZE];

/* This points to an array of pointers to saved macro hash tables.
 * Each time the user does savemacros, we realloc this array one bigger,
 * and create a new macro hash table.
 * Each such table is an array of pointers to hash collision chains.
 */
static struct MACRO ***Saved_mac_tables;
/* How many saved macros tables there are. If this is zero, we know
 * the user hasn't ever called savemacros. In that case we will
 * free memory hanging off of MACRO structs when a macro is redefined or
 * undefined, because we know there is no chance they will get restored.
 * Once it becomes nonzero, we don't do that, to be safe.  */
static int Num_mac_tables;

/* temporary file for saving text of macros. Need separate handles for reading
 * and writing */
static FILE *Mactmp_read = (FILE *) 0;
static FILE *Mactmp_write = (FILE *) 0;
#ifdef UNIX_LIKE_FILES
static char Macfile[] = "mupmacXXXXXX";		/* name of temp file */
#else
/* As last resort, we use a temp file name of 11 character length,
 * so make sure we have enough room for that. Usually L_tmpnam is
 * already longer than that, but better safe than core dump.
 */
#if L_tmpnam < 12
#undef L_tmpnam
#define L_tmpnam 12
#endif
static char Macfile[L_tmpnam];		/* name of temp file */
#endif

/* Some OSs require us to open files in binary mode. Most implementations
 * of fopen these days accept the 'b' suffix, even when they don't care
 * about it, but to be safe, we only add on systems that appear to have
 * binary mode defined. */
#ifdef O_BINARY
static char *Read_mode = "rb";
#ifndef UNIX_LIKE_FILES
static char *Write_mode = "wb";
static char *Append_mode = "ab";
#endif
#else
static char *Read_mode = "r";
#ifndef UNIX_LIKE_FILES
static char *Write_mode = "w";
static char *Append_mode = "a";
#endif
#endif

/* maximum number of files on file stack */
#ifndef FOPEN_MAX
#ifdef _NFILE
#define FOPEN_MAX	_NFILE
#else
#define FOPEN_MAX	(20)
#endif
#endif
/* The -5 is to account for stdin, stdout, stderr, and the 2 tmp file handles.
 * The +20 is to allow for nested macros. They can take 2 stack slots per call
 * since argument expansion also uses a slot, but they don't take extra
 * file descriptors, since all the macros are kept in one file. */
#define MAXFSTK	(FOPEN_MAX - 5 + 20)


struct FILESTACK {
	FILE	*file;
	char	*filename;
	long	fileoffset;		/* fseek position in file */
	int	lineno;			/* where we are in the file */
	int	lineno_inc;		/* saved Lineno_increment */
	struct MACRO	*mac_p;		/* if pushing because of a macro call,
					 * this will point to info about the
					 * macro, otherwise 0. */
};

static struct FILESTACK Filestack[MAXFSTK];

static int Fstkptr = -1;		/* stack pointer for Filestack */
static char quote_designator[] = "`";
/* We save information about an "eval" expression in a temporary macro
 * while evaluating it. This hold info about that temporary macro.
 * Since they cannot be nested, we can re-use this for each. */
static struct MACRO Curr_expr_macro;
/* This is the final result of current "eval" expression */
extern struct VALUE Expr_result;

extern int unlink();			/* to remove temp file */
extern char *tmpnam();		/* generate temp file name */

/* static function declarations */
static void pushfile P((FILE *file, char *filename, int lineno,
		struct MACRO *mac_p));
static int macro_call P((char *macname));
static char *path_combiner P((char *prefix));
static FILE * find_relative_file P((char **filename_p));
static FILE *find_with_suffix P((char **filename_p, char *searchpath,
		char *path_separator));
static int is_absolute_path P((char *filename));
static struct MACRO *findMacro P((char *macname));
static int hashmac P(( char *macname));
static struct MACRO *setup_macro P((char *macname, int has_params, int expr_state));
static void prepare_mac_write P((struct MACRO *mac_p));
static void finish_mac_write P((void));
static void begin_macro_read P((struct MACRO *mac_p, char *macname));
static void free_parameters P((struct MAC_PARAM *param_p, char *macname,
		int values_only));
static char *mkmacparm_name P((char *macname, char *param_name));
static struct MACRO *resolve_mac_name P((char *macname));
static int has_quote_designator P((char *macname));
static void clone_mac_table P((struct MACRO **src_tbl, struct MACRO **dest_tbl));
static void stringify P((struct MACRO *mac_p));




/* add macro name to hash table and arrange to save its text in temp file */

void
define_macro(macname, is_expression)

char *macname;		/* name of macro to be defined */
int is_expression;	/* YES if is expression to be evaluated */

{
	int has_params = NO;
	char *mac_name;		/* copy of macro name, because what gets
				 * passed in can get overwritten by parameter
				 * names */
	struct MACRO *mac_p;

	debug (4, "define_macro macname=%s, is_expression=%d\n",
						macname, is_expression);

	/* there might be some leading white space in front of macro name,
	 * if so, ignore that */
	while (*macname == ' ' || *macname == '\t') {
		macname++;
	}

	/* if ends with a ( this macro has parameters */
	if (macname[strlen(macname) - 1] == '(') {
		/* this is a macro with parameters */
		macname[strlen(macname) - 1] = '\0';
		has_params = YES;
	}

	mac_p = setup_macro(macname, has_params,
			(is_expression ? PARSING_EXPR : NOT_EXPR) );
	mac_name = mac_p->macname;

	if (has_params == YES) {
		get_parameters(mac_name);
	}

	/* Copy the macro text to the macro temp file. We used to use the
	 * return value to decide whether to skip saving a quoted
	 * copy of the parameter when it is called, as a optimization.
	 * But it turned out that the optimization could be fooled into
	 * being used when it shouldn't be, namely by passing to another
	 * macro which then wanted quoting. Now we create a quoted copy
	 * when we discover we actually need it. */
	(void) save_macro(Mactmp_write);

	if (is_expression == YES) {
		/* Add special terminator to signal lex */
		fputs(":=:", Mactmp_write);
	}

	/* terminate the macro with a NULL, so that when lex hits it when
	 * the macro is called, it will think it hit EOF */
	putc('\0', Mactmp_write);

	finish_mac_write();

	if (is_expression == YES) {
		/* Make lex/exprgram evaluate it. */
		begin_macro_read(mac_p, mac_p->macname);
	}
}


/* save macro info in hash table and make sure macro temporary file is
 * set up and ready to use */

static struct MACRO *
setup_macro(macname, has_params, expr_state)

char *macname;		/* name of macro being defined */
int has_params;		/* YES or NO */
int expr_state;		/* *_EXPR value */

{
	struct MACRO *mac_p;	/* info about current macro */
	int h;			/* hash number */


	if (expr_state == PARSING_EXPR) {
		mac_p = &Curr_expr_macro;
		mac_p->macname = strdup(macname);
	}
	else {
		/* if macro has not been defined before, add to hash table */
		if ((mac_p = findMacro(macname)) == (struct MACRO *) 0) {

			MALLOC(MACRO, mac_p, 1);
			h = hashmac(macname);
			mac_p->next = Mactable[h];
			Mactable[h] = mac_p;

			mac_p->macname = strdup(macname);
			mac_p->recursion = 0;
		}
		else if (expr_state != SETTING_EXPR) {
			l_warning(Curr_filename, yylineno,
					"macro '%s' redefined", macname);
			/* Free space used if we are sure it is safe to do so */
			if (Num_mac_tables == 0) {
				free_parameters(mac_p->parameters_p, macname, NO);
			}
		}
	}

	mac_p->parameters_p = (struct MAC_PARAM *) 0;
	/* We don't know how many parameters there will be yet,
	 * only whether there are some. If there will be some,
	 * we mark how many we have so far (zero). Note that it
	 * is possible to have a macro with zero parameters (define X())
	 * which is different than one without parameters (define X).
	 */
	mac_p->num_params = (has_params == YES ? 0 : WITHOUT_PARAMS);

	/* save current filename and line number so they can be used to give
	 * useful error messages when the macro is called */
	if (Fstkptr >= 0 && Filestack[Fstkptr].mac_p != (struct MACRO *) 0) {
		/* if current expanding a macro, get file/line info relative
		 * to the macro */
		mac_p->filename = Filestack[Fstkptr].mac_p->filename;
		mac_p->lineno = Filestack[Fstkptr].mac_p->lineno +
				Filestack[Fstkptr].mac_p->lineoffset;
	}
	else {
		mac_p->filename = Curr_filename;
		mac_p->lineno = yylineno;
	}

	prepare_mac_write(mac_p);
	return(mac_p);
}


static void
prepare_mac_write(mac_p)

struct MACRO *mac_p;

{
	static int have_mac_file = NO;
#ifdef UNIX_LIKE_FILES
	int filedesc;		/* of tmp file for storing macros */
#endif

	/* if we don't yet have a temp file to store macro info, open one */
	if (have_mac_file == NO) {
		/* We need separate read/write file pointers in case of defining
		 * one macro inside another, so can't easily use tmpfile().
		 * The most straightforward way to do this is to use tmpnam.
		 * But recent versions of gcc complain loudly that tmpnam
		 * is dangerous--you should use mkstemp instead. While it's
		 * true tmpnam could have problems under certain conditions,
		 * Mup's use of it hardly qualifies as "dangerous." Yes,
		 * perhaps if you tried really hard by running lots and
		 * lots of instances of Mup simultaneously, and they all
		 * used macros, maybe you could get one of them to fail once
		 * in a while. But some systems may not support mkstemp,
		 * since it's more from BSD than System V, and it's unclear
		 * how it ought to function on a system that has DOS-style
		 * (8.3 character) filenames if you gave it a longer string
		 * than would be a legal file name.
		 * And, interestingly, the Solaris manual pages say
		 * that you should NOT use mkstemp, in direct conflict with
		 * gcc's opinion. They say to use tmpfile, which we can't do
		 * because that just gives a single file pointer for writing,
		 * but we want one for writing and one for reading.
		 * So it's very unclear what to do. For unix-ish systems
		 * we'll go with mkstemp, and for others use tmpnam.
		 * We get a temp file name, open it twice, once for writing,
		 * once for reading, then remove the file. This will make it a
 		 * hidden file that will disappear when the 2 file pointers
		 * are closed.
		 */
#ifdef UNIX_LIKE_FILES
		if ((filedesc = mkstemp(Macfile)) < 0) {
			ufatal("can't create temporary file for macro storage (possibly you don't have write permissions on directory/file?)");
		}
		if ((Mactmp_write = fdopen(filedesc, "w")) == (FILE *) 0) {
			pfatal("can't fdopen temporary file for macro storage");
		}
		if ((Mactmp_read = fopen(Macfile, "r")) == (FILE *) 0) {
			pfatal("can't open temporary file for reading macro");
		}

		/* arrange for file to vanish */
		(void) unlink(Macfile);
#else
		/* On non-UNIX systems, unlinking an open file may not have
		 * the desired effect, and some systems don't properly
		 * handle both a read and write FILE * on the same file.
		 * So for those we just create it here and have to open
		 * and close the file each time. */
		(void) tmpnam(Macfile);
		if ((Mactmp_write = fopen(Macfile, Write_mode)) == (FILE *) 0) {
			/* If tmpnam isn't implemented or fails,
			 * try a hard-coded temp name and hope
			 * for the best... */
			(void) strcpy(Macfile, "MupMacF.tmp");
			if ((Mactmp_write = fopen(Macfile, Write_mode))
							== (FILE *) 0) {
				ufatal("can't open temporary file for macro storage (possibly you don't have write permissions on directory/file?)");
			}
		}
#endif
		have_mac_file = YES;
	}
#ifndef UNIX_LIKE_FILES
	else {
		if ((Mactmp_write = fopen(Macfile, Append_mode)) == (FILE *) 0) {
			pfatal("can't open temporary file for macro storage (maybe out of memory?)");
		}
	}
#endif
	/* make sure we're at the end. Now that we have separate read/write
	 * file pointers, this should be unnecessary. However, on one non-UNIX
	 * system is seemed the opening for append didn't really work,
	 * requiring this fseek to actually go to the end. And in a previous
	 * variation of this code, this fseek was needed for the UNIX way too.
	 * In any case, it doesn't hurt to be sure we are where we want to be. */
	if (fseek(Mactmp_write, 0L, SEEK_END) != 0) {
		pfatal("fseek failed in setup_macro");
	}

	/* keep track of where this macro will begin in the temp file */
	mac_p->offset = ftell(Mactmp_write);
	/* We may make a quoted copy of it later, but only if needed */
	mac_p->quoted_offset = 0;
}


static void
finish_mac_write()

{
	(void) fflush(Mactmp_write);
#ifndef UNIX_LIKE_FILES
	fclose(Mactmp_write);
#endif
}


#ifndef UNIX_LIKE_FILES
/* on non-unix systems, unlinking an open file may produce undesired results,
 * so clean up the macro temp file after parsing is done. Unfortunately,
 * this leave a higher possibility of leaving an orphan temp file. */

void
mac_cleanup()

{
	if (Macfile[0] != '\0') {
		unlink(Macfile);
	}
	Macfile[0] =  '\0';
}
#endif


/* look up macro name in hash table and return info about it, or null if
 * not defined */

static struct MACRO *
findMacro(macname)

char *macname;		/* which macro to look up */

{
	struct MACRO *mac_p;	/* pointer to info about macro */


	/* search hash table and collision chain off the table for match
	 * of macro name */
	for (mac_p = Mactable[ hashmac(macname) ]; mac_p != (struct MACRO *) 0;
						mac_p = mac_p->next) {

		if (strcmp(mac_p->macname, macname) == 0) {
			/* found it! */
			return(mac_p);
		}
	}

	/* macro not defined */
	return( (struct MACRO *) 0);
}


/* Remove a macro definition. We just remove its entry from the current
 * hash table. Its text will still remain in the macro temp file,
 * because it seems like too much trouble to do storage management
 * on a temp file, and because if macros have been saved,
 * it could get restored later.
 * Note that if asked to undef a macro that isn't defined, it silently
 * does nothing.
 */

void
undef_macro(macname)

char *macname;		/* which macro to undefine */

{
	struct MACRO *mac_p;	/* to walk though list of info about macros */
	struct MACRO **mac_p_p;	/* to keep track of delete place */


	/* there might be some leading white space in front of macro name,
	 * if so, ignore that */
	while (*macname == ' ' || *macname == '\t') {
		macname++;
	}

	/* keep track of where to delete from linked list */
	for (mac_p_p = &(Mactable[ hashmac(macname) ]);
					*mac_p_p != (struct MACRO *) 0;
					mac_p_p = &((*mac_p_p)->next)) {
		mac_p = *mac_p_p;
		if (strcmp(mac_p->macname, macname) == 0) {
			
			/* found it--delete this entry from list */
			*mac_p_p = mac_p->next;
			/* Free space used if we are sure it is safe to do so */
			if (Num_mac_tables == 0) {
				FREE(mac_p->macname);
				free_parameters(mac_p->parameters_p, macname, NO);
				FREE(mac_p);
			}
			return;
		}
	}
}


/* generate hash number from macro name */

static int
hashmac(macname)

char *macname;

{
	int h;

	/* add up characters of name and take sum modulo hash table size */
	for (h = 0; *macname != '\0'; macname++) {
		h += *macname;
	}
	return(h % MTSIZE);
}


/* when macro is called, arrange to read its text from macro file */

void
call_macro(macname)

char *macname;		/* which macro to call */

{
	/* Originally no calls needed a return value, but then one did,
	 * so this now calls new version but ignores its return,
	 * so old callers are unaffected. */
	return (void) macro_call(macname);
}

/* Returns YES on success, NO on failure. */

static int
macro_call(macname)

char *macname;		/* which macro to call */

{
	struct MACRO *mac_p;	/* info about the macro */


	debug(4, "call_macro macname=%s\n", macname);

	if ((mac_p = resolve_mac_name(macname)) == (struct MACRO *) 0) {
		l_yyerror(Curr_filename, yylineno,
				"macro '%s' not defined", macname);
		return(NO);;
	}

	/* If macro has parameters, remove any previous arguments
	 * and gather the arguments for this call. */
	if (mac_p->num_params >= 0) {
		free_parameters(mac_p->parameters_p, macname, YES);
		if (get_mac_arguments(macname, mac_p->num_params) == NO) {
			/* something was wrong with argument. Don't bother
			 * trying to expand, because we'll probably just
			 * get lots more error messages */
			return(NO);;
		}
	}

	begin_macro_read(mac_p, macname);
	return(YES);
}


/* Prepare to read macro text from the macro temp file. */

static void
begin_macro_read(mac_p, macname)

struct MACRO *mac_p;
char *macname;

{
#ifndef UNIX_LIKE_FILES
	if ((Mactmp_read = fopen(Macfile, Read_mode)) == (FILE *) 0) {
		pfatal("can't open macro file for reading (maybe out of memory?)");
	}
#endif

	/* save old yyin value and make macro definition the input */
	pushfile(Mactmp_read, mac_p->filename, mac_p->lineno, mac_p);
	
	/* go to where macro definition begins */
	if (fseek(Mactmp_read, (has_quote_designator(macname)
			? mac_p->quoted_offset : mac_p->offset),
			SEEK_SET) != 0) {
		pfatal("fseek failed in macro_call");
	}
}


/* save info about current yyin and set yyin to specified file */

static void
pushfile(file, filename, lineno, mac_p)

FILE *file;		/* replace current file with this file */
char *filename;		/* name of new file to use */
int lineno;		/* current linenumber in new file */
struct MACRO *mac_p;	/* if switching to macro temp file because of a
			 * macro call, this is information about macro.
			 * Otherwise, it will be null. */

{
	debug(4, "pushfile file=%s", filename);

	/* do error checks */
	if (++Fstkptr >= MAXFSTK) {
		l_ufatal(filename, lineno,
			"too many nested files or macros (%d levels maximum)\n",
			MAXFSTK);
	}
	
	if (mac_p != (struct MACRO *) 0) {
		if ( ++(mac_p->recursion) != 1) {
			l_ufatal(Curr_filename, yylineno,
				"macro '%s' called recursively",
				mac_p->macname);
		}
	}

	/* save current info */
	Filestack[Fstkptr].file = yyin;
	Filestack[Fstkptr].fileoffset = (yyin ? ftell(yyin) : -1);
	Filestack[Fstkptr].filename = Curr_filename;
	Filestack[Fstkptr].lineno = yylineno;
	Filestack[Fstkptr].lineno_inc = Lineno_increment;
	Lineno_increment = 0;
	Filestack[Fstkptr].mac_p = mac_p;

	/* arrange to use the new file */
	new_lexbuff(file);

	/* if we are now expanding a macro, we don't change the input line
	 * number.  If doing an include, then we do. */
	if (Filestack[Fstkptr].mac_p == (struct MACRO *) 0) {
		yylineno = lineno;
		Curr_filename = filename;
	}
	else {
		Filestack[Fstkptr].mac_p->lineoffset = 0;
	}
}


/* if there are any files on the Filestack, go back to the previous one
 * on the stack. Return 1 if something was popped, 0 if stack was empty */

int
popfile()

{
	debug(4, "popfile");

	/* if nothing on file stack, nothing to do  except return 0 */
	if (Fstkptr < 0) {
		return(0);
	}

	if (Filestack[Fstkptr].mac_p != (struct MACRO *) 0) {
		/* returning from macro call */
		Filestack[Fstkptr].mac_p->recursion = 0;
#ifndef UNIX_LIKE_FILES
		if (yyin != 0) {
			(void) fclose(yyin);
		}
#endif
	}
	else {
		/* this is an include rather than a macro file, so close it */
		if (yyin != 0) {
			(void) fclose(yyin);
		}
	}

	/* set things back to the previous file */
	yyin = Filestack[Fstkptr].file;
	Curr_filename = Filestack[Fstkptr].filename;
	if ( (yyin != 0) && (Filestack[Fstkptr].fileoffset >= 0) ) {
		if (fseek(yyin, Filestack[Fstkptr].fileoffset, SEEK_SET) != 0) {
			pfatal("fseek failed in popfile");
		}
	}
	yylineno = Filestack[Fstkptr].lineno;
	Lineno_increment = Filestack[Fstkptr].lineno_inc;

	/* go back to previous file */
	del_lexbuff();

	/* decrement stackpointer */
	Fstkptr--;

	return(1);
}


/* return 1 if we are NOT currently expanding a macro, 0 if we are.
 * This backwards logic is used because when we ARE doing a macro, we
 * should NOT muck with yylineno, and vice versa. If in a macro, adjust
 * the line offset within the macro by the specified amount. */

int
not_in_mac(inc_dec)

int inc_dec;	/* how much to increment/decrement the in-macro line offset */
{
	if (Fstkptr >= 0 && Filestack[Fstkptr].mac_p != (struct MACRO *) 0) {
		/* we are in a macro */
		Filestack[Fstkptr].mac_p->lineoffset += inc_dec;
		return(0);
	}
	else {
		return(1);
	}
}


/* if an error occurs while expanding a macro, give additional help */

void
mac_error()

{
	struct MACRO *mac_p;

	/* Don't want this while evaluating an "eval" expression */
	if (Curr_expr_macro.macname != 0) {
		return;
	}
	if (Fstkptr >= 0 && (mac_p = Filestack[Fstkptr].mac_p)
						!= (struct MACRO *) 0) {
		(void) fprintf(stderr, "note: previous error found while expanding macro %s'%s' from %s: line %d:\n",
				(strchr(mac_p->macname, '(') ? "parameter " : ""),
				mac_p->macname,
				mac_p->filename,
				mac_p->lineno + mac_p->lineoffset);
		print_offending_line(mac_p->filename, mac_p->lineno + mac_p->lineoffset);
	}
}


/* process an included file */

void
includefile(fname)

char *fname;	/* name of file to include */

{
	FILE *file;		/* the included file */
	char *fnamecopy;


	/* attempt to open file. Give message if fail */
	/* Note that if we find the file somewhere up the MUPPATH
	 * rather than directly, fname will be updated to contain the
	 * actual path of the file we are using, so we can tell that
	 * to the user. That way if they have several files by the same
	 * name but in different directories, and we're picking up a different
	 * one than they had intended, we can give them a clue of what's
	 * going on... */
	if ((file = find_file(&fname)) == (FILE *) 0) {

		l_ufatal(Curr_filename, yylineno,
				"can't open include file '%s'", fname);
	}

	/* need to make copy of file name */
	fnamecopy = strdup(fname);

	/* arrange to connect yyin to the included file, save info, etc */
	pushfile(file, fnamecopy, 1, (struct MACRO *) 0);
}


/* Find a file to be included. First look using file name as is.
 * If not found, check if is absolute path name, and if so, give up.
 * Otherwise, try prepending each component of $MUPPATH in turn,
 * and trying that as a path. If a file is found, return that.
 * If still not found, try relative to the including file,
 * before giving up for good.
 * Giving up means returning 0. If we find it somewhere up the MUPPATH
 * rather than directly, update filename to point to the actual path used.
 */

FILE *
find_file(filename_p)

char **filename_p;

{
	char *filename;
	FILE *file;
	char *envmuppath;		/* from getenv("MUPPATH") */
	char envbuff[BUFSIZ];		/* in case MUPPATH is not set */
	char *path_separator = "\0";	/* between components in $MUPPATH */


	/* first try name just as it is. */
	filename = *filename_p;
	if ((file = fopen(filename, Read_mode)) != (FILE *) 0) {
		return(file);
	}

	/* If it's an absolute path, we have to give up */
	if (is_absolute_path(filename)) {
		return ((FILE *) 0);
	}

	if ((envmuppath = getenv("MUPPATH")) != (char *) 0) {

#ifdef UNIX_LIKE_PATH_RULES
		path_separator = ":";
#endif
#ifdef DOS_LIKE_PATH_RULES
		path_separator = ";";
#endif
		if (*path_separator == '\0') {
			/* If user went to the trouble of setting $MUPPATH,
			 * they probably think it will work, so we better
			 * let them know it doesn't. Hopefully they will
			 * tell us what path rules their operating system
			 * uses, so we support it in the next release. */
			warning("MUPPATH facility not implemented for this operating system");
			/* We can still try adding suffixes, so keep going. */
		}
	}
	else {
		/* Create an effective MUPPATH */
		if (getcwd(envbuff, sizeof(envbuff)) == 0) {
			ufatal("Cannot obtain current working directory");
		}
		envmuppath = envbuff;
	}
	if ((file = find_with_suffix(filename_p, envmuppath, path_separator)) != 0) {
		return(file);
	}

	/* try relative to including file */
	return(find_relative_file(filename_p));
}


/* Given a file name, try to find it relative to the current file (the
 * one that is including it). If found, return FILE * to it, and update
 * what filename_p points to with its path. */

static FILE *
find_relative_file(filename_p)

char **filename_p;	/* what to find; will be updated with path if found */

{
	char *dir_separator; 	/* what is between a directory and file in path name */
	char *curr_dir_separator = 0;	/* current instance of dir_separator */
	char *last_dir_separator = 0;	/* the rightmost dir_separator */
	char *filepath;			/* points into Curr_filename */
	char *includer_dir;		/* copy of Curr_filename's directory */
	int dir_name_length;		/* strlen of includer_dir */
	FILE *file;			/* what to return */


	/* Get what would separate directory from file in file path */
	dir_separator = path_combiner(".");

	/* Find the last instance of that separator in the "including"
	 * file path. The current file is that includer. */
	for (filepath = Curr_filename;
			(curr_dir_separator = strstr(filepath, dir_separator)) != 0;
			last_dir_separator = curr_dir_separator) {
		filepath = curr_dir_separator + strlen(dir_separator);
	}
	if (last_dir_separator == 0) {
		/* Just a filename with no directory leading
		 * up to it. We've already tried relative to the
		 * current directory, so no point in trying that again. */
		return(0);
	}

	/* Make a pseudo $MUPPATH which consists of the directory of 
	 * the including file, and try looking for the include file in there. */
	dir_name_length = last_dir_separator - Curr_filename;
	MALLOCA(char, includer_dir, dir_name_length + 1);
	strncpy(includer_dir, Curr_filename, dir_name_length);
	includer_dir[dir_name_length] = '\0';
	file = find_with_suffix(filename_p, includer_dir, "");
	FREE(includer_dir);
	return(file);
}


/* Try to find the given filename, by looping though a $PATH like search path,
 * and trying various possible suffixes for each component of that path.
 * Returns the FILE * of the file if found, or 0 if not found.
 * If found, what filename_p points to is updated to the found name.
 */

static FILE *
find_with_suffix(filename_p, searchpath, path_separator)

char **filename_p;	/* look for this file name */
char *searchpath;	/* search directories in this MUPPATH like string */
char *path_separator;

{
	FILE *file;			/* what to return */
	char *filename;			/*  *filename_p */
	char *muppath;			/* copy of search path */
	char *fullpath;			/* constructed full path */
	char *combiner;			/* what goes between path components */
	char *prefix;			/* component of the search path
					 * to prepend to filename */
	static char *suffixes[] = { "", ".mup", ".MUP", 0 }; /* List of
					 * suffixes to try adding.
					 * First we try without a suffix
					 * then with .mup and MUP before
					 * giving up. Some OS's have case
					 * insensitive names, so the .MUP
					 * will be fruitless there, but
					 * worth trying on those that are
					 * case sensitive, and not worth
					 * trying to figure out if worth it.
					 * If they use .Mup or something on
					 * such a system, too bad. */
	char **suffix_p;

	filename = *filename_p;
	/* Loop through the search path multiple times if necessary,
	 * trying with a different suffix each time, before giving up. */
	for (suffix_p = suffixes; *suffix_p != 0; suffix_p++) {

		/* If already has suffix, don't bother trying adding */
		int sufflen = strlen(*suffix_p);
		int namelen = strlen(filename);
		if (sufflen > 0 && namelen > sufflen
				&& strcasecmp(*suffix_p,
				filename + namelen - sufflen) == 0) {
			continue;
		}

		/* Make a copy of the search path,
		 * so strtok can overwrite the separator */
		MALLOCA(char, muppath, strlen(searchpath) + 1);
		(void) strcpy(muppath, searchpath);

		/* walk through the search path */
		for (prefix = strtok(muppath, path_separator);
				prefix != (char *) 0;
				prefix = strtok(0, path_separator)) {
	
			combiner = path_combiner(prefix);

			/* get enough space for the full name */
			MALLOCA(char, fullpath, strlen(prefix) + namelen +
				+ strlen(combiner) + sufflen + 1);

			/* create full path */
			sprintf(fullpath, "%s%s%s%s", prefix, combiner,
							filename, *suffix_p);

			/* See if this file exists */
			debug(4, "checking '%s' for include, using search path",
							fullpath);
			if ((file = fopen(fullpath, Read_mode)) != (FILE *) 0) {
				*filename_p = fullpath;
				FREE(muppath);
				return(file);
			}

			/* no file here, no need to save this path */
			FREE(fullpath);
		}
		FREE(muppath);
	}

	return (FILE *) 0;
}


/* return true if given filename is an absolute path name */

static int
is_absolute_path(filename)

char *filename;

{

#ifdef UNIX_LIKE_PATH_RULES
	/* For Unix, a pathname is absolute if it starts with a slash */
	return (*filename == '/');
#endif

#ifdef DOS_LIKE_PATH_RULES
	/* If second character is a colon, then absolute */
	if (*filename != '\0' && *(filename + 1) == ':') {
		return(1);
	}
	else {
		return(0);
	}
#endif

#if ! defined(UNIX_LIKE_PATH_RULES) && ! defined(DOS_LIKE_PATH_RULES)
	/* Not implemented for this operating system. We'll pretend
	 * it's not, which will make it fall through and fail later. */
	return(0);
#endif
}


/* What to use to glue together a prefix and relative path to get a
 * full path. */

static char *
path_combiner(prefix)

char *prefix;

{

#ifdef UNIX_LIKE_PATH_RULES
	/* Unix separator is slash. If there was already a slash at the
	 * end of the prefix, no problem: multiples slashes are like one */
	return ("/");
#endif

#ifdef DOS_LIKE_PATH_RULES
	/* Use backslash, unless prefix ended with slash or backslash,
	 * in which case we don't need anything. */
	char last_ch;
	last_ch = prefix[strlen(prefix) - 1];
	return ((last_ch == '\\' || last_ch == '/') ? "" : "\\");
#endif

#if ! defined(UNIX_LIKE_PATH_RULES) && ! defined(DOS_LIKE_PATH_RULES)
	/* Shouldn't be here. Unimplemented for this operating system */
	return ("");
#endif
}


/* return YES if macro is currently defined, NO if it isn't */

int
is_defined(macname, paramtoo)

char *macname;
int paramtoo;	/* if YES, also look for macro parameter by this name,
		 * otherwise just macro */

{
	if (paramtoo) {
		return(resolve_mac_name(macname) == 0 ? NO : YES);
	}
	else {
		return(findMacro(macname) == 0 ? NO : YES);
	}
}


/* when the -D option is used on the command line, save the macro definition. */

void
cmdline_macro(macdef)

char *macdef;	/* MACRO=definition */

{
	char *def;


	/* separate out the macro name */
	if (*macdef == '_' || isdigit(*macdef) ) {
		if (Mupmate == YES) {
			l_yyerror(0, -1, "Run > Set Options > Define Macros: macro name cannot start with underscore or digit.");
		}
		else {
			l_yyerror(0, -1, "argument for %cD is invalid: macro name cannot start with underscore or digit", Optch);
		}
		return;
	}
	for (def = macdef; *def != '\0'; def++) {
		if ( ! isupper(*def) && ! isdigit(*def) && *def != '_') {
			break;
		}
	}

	/* make sure has form XXX=definition */
	if (def == macdef) {
		if (Mupmate == YES) {
			l_yyerror(0, -1, "Run > Set Options > Define Macros: macro name must contain only upper case letters, numbers, and underscores, starting with a letter.");
		}
		else {
			l_yyerror(0, -1, "argument for %cD is missing or wrong format; macro name must contain only upper case letters, numbers, and underscores, starting with a letter", Optch);
		}
		return;
	}

	if (*def == '=') {
		*def++ = '\0';
	}
	else if (*def != '\0') {
		if (Mupmate == YES) {
			l_yyerror(0, -1, "Run > Set Options > Define Macros: macro name is invalid or missing '=' on macro definition.");
		}
		else {
			l_yyerror(0, -1, "argument for %cD had invalid name or missing '=' on macro definition", Optch);
		}
		return;
	}

	Curr_filename = "Command line argument";
	/* command line macros can never have parameters or be expressions */
	(void) setup_macro(macdef, NO, NOT_EXPR);

	/* copy the macro to the macro temp file */
	do {
		putc(*def, Mactmp_write);
	} while ( *def++ != '\0');
	finish_mac_write();
}


/* recursively free a list of macro parameters */

static void
free_parameters(param_p, macname, values_only)

struct MAC_PARAM *param_p;	/* what to free */
char *macname;			/* name of macro having this parameter */
int values_only;		/* if YES, just get rid of current
				 * argument values, otherwise dispose of
				 * the entire parameters list */

{
	char *mp_name;		/* internal name of macro parameter */


	if (param_p == (struct MAC_PARAM *) 0) {
		/* end of list */
		return;
	}

	/* recurse */
	free_parameters(param_p->next, macname, values_only);

	/* need to undef the internal name */
	mp_name = mkmacparm_name(macname, param_p->param_name);
	undef_macro(mp_name);
	FREE(mp_name);

	if (values_only == NO) {

		/* release space */
		if (param_p->param_name != (char *) 0) {
			FREE(param_p->param_name);
		}
		FREE(param_p);
	}
}


/* given a macro name and a parameter name, add the parameter name to the
 * list of parameters for the macro */

void
add_parameter(macname, param_name)

char *macname;
char *param_name;		/* name of parameter to add */

{
	struct MACRO *macinfo_p;	/* which macro to add to */
	struct MAC_PARAM *new_p;	/* new parameter */
	struct MAC_PARAM *param_p;	/* to walk through parameter list */


	/* get space to store info about the parameter */
	MALLOC(MAC_PARAM, new_p, 1);

	/* get the macro information to know where to attach */
	if ((macinfo_p = findMacro(macname)) == (struct MACRO *) 0) {
		pfatal("add_parameter unable to find macro %s", macname);
	}

	/* if this is first parameter, link directly to macro, otherwise
	 * to the end of the parameter linked list */
	if (macinfo_p->parameters_p == (struct MAC_PARAM *) 0) {
		macinfo_p->parameters_p = new_p;
	}
	else {
		/* walk down current parameter list */
		for (param_p = macinfo_p->parameters_p;
				param_p != (struct MAC_PARAM *) 0;
				param_p = param_p->next) {

			/* check for duplicate name */
			if (strcmp(param_name, param_p->param_name) == 0) {
				l_yyerror(Curr_filename, yylineno,
					"duplicate parameter name %s for macro %s",
					param_name, macname);
			}

			/* link onto end of list */
			if (param_p->next == (struct MAC_PARAM *) 0) {
				param_p->next = new_p;
				break;
			}
		}
	}

	/* fill in the info */
	new_p->param_name = strdup(param_name);
	new_p->next = (struct MAC_PARAM *) 0;

	(macinfo_p->num_params)++;
}


/* save the value of a macro argument by making a macro out of it */

void
set_parm_value(macname, argbuff, argnum)

char *macname;	/* name of macro */
char *argbuff;	/* value of argument */
int argnum;	/* which argument. 1 for the first, 2 for second, etc */

{
	static struct MAC_PARAM *param_p;	/* keep track of current
			 * parameter. The first time we are called for a
			 * given macro, we look up the macro and get its
			 * first parameter. After that, we just follow
			 * the linked list of parameters */
	struct MACRO *mac_p;	/* info about macro */
	char *mp_name;	/* pointer to malloc-ed space containing internal
			 * name of MACRO(PARAMETER) */

	if (argnum == 1) {
		/* this is the first argument, so we have to look up the
		 * macro */
		if ((mac_p = findMacro(macname)) == (struct MACRO *) 0) {
			pfatal("set_parm_value can't find macro");
		}

		/* point to head of parameters list */
		param_p = mac_p->parameters_p;
	}
	else {
		/* just advance to the next parameter */
		if (param_p != (struct MAC_PARAM *) 0) {
			param_p = param_p->next;
		}
		if (param_p == (struct MAC_PARAM *) 0) {
			/* no next parameter. Error msg is printed elsewhere,
			 * so just clean up and return */
			FREE(argbuff);
			return;
		}
	}

	/* if argbuff is null, there is no argument, which really means the
	 * argument is the null string. */
	if (argbuff == (char *) 0) {
		argbuff = strdup("");
	}

	/* now associate the value with the parameter */
	mp_name = mkmacparm_name(macname, param_p->param_name);
	/* A parameter cannot itself have parameters */
	(void)setup_macro(mp_name, NO, NOT_EXPR);

	/* Copy the parameter text to the macro temp file. */
	fprintf(Mactmp_write, "%s", argbuff);
	putc('\0', Mactmp_write);

	finish_mac_write();

	/* temp space no longer needed */
	FREE(mp_name);
	FREE(argbuff);
}


/* make an internal macro name for a macro parameter name. The internal
 * name is MACRO(PARAMETER). Space for name is malloc-ed, caller must free */

static char *
mkmacparm_name(macname, param_name)

char *macname;
char *param_name;

{
	char *internal_name;

	/* add 3 for the 2 parentheses and the null */
	MALLOCA(char, internal_name,
			strlen(macname) + strlen(param_name) + 3);

	(void) sprintf(internal_name, "%s(%s)", macname, param_name);
	return(internal_name);
}


/* add a character to the current macro argument buffer. */

char *
add2argbuff(argbuff, c)

char *argbuff;		/* the argument buffer so far */
int c;			/* a character to add to the buffer */

{
	static int offset;	/* where in argbuff to put character */
	static int length;	/* how many characters we have allocated */


	if (argbuff == (char *) 0) {
		/* first time we were called, so malloc some space */
		MALLOCA(char, argbuff, MAC_ARG_SZ);
		offset = 0;
		length = MAC_ARG_SZ;
	}
	else if (offset == length - 1) {
		/* need more space */
		length += MAC_ARG_SZ;
		REALLOCA(char, argbuff, length);
	}

	/* put character in the buffer and null terminate it */
	argbuff[offset++] = (char) c;
	argbuff[offset] = '\0';

	return(argbuff);
}


/* given a macro name that might be either a macro parameter or a regular
 * macro, return pointer to the proper MACRO struct. We do this by
 * searching up the stack of macros being expanded. If the name matches
 * that of a macro parameter, use that, otherwise treat as a normal macro.
 * Return null if can't resolve */

static struct MACRO *
resolve_mac_name(macname)

char *macname;

{
	struct MACRO *mac_p;	/* macro that matches the macname */
	int i;			/* index through file stack */
	char *mp_name;		/* macro parameter internal name */
	int quoted;		/* if has quote designator */
	char *basename;		/* macname not counting quote designator */


	if ((quoted = has_quote_designator(macname)) == YES) {
		basename = macname + strlen(quote_designator);
	}
	else {
		basename = macname;
	} 
		
	/* first go up the stack of macro calls, seeing if the macro
	 * name matches the parameter name of any macro. If so, that's
	 * the correct macro to use. Failing that, we look for the macro
	 * name just as is. If that fails too, we have to admit defeat
	 * and return null. */
	for (i = 0; i <= Fstkptr; i++) {
		if (Filestack[i].mac_p != (struct MACRO *) 0) {
			/* we are expanding a macro. See if that macro
			 * has a parameter by the name we are trying to
			 * resolve. If so, that's what we want */
			mp_name = mkmacparm_name(Filestack[i].mac_p->macname,
					basename);
			mac_p = findMacro(mp_name);
			FREE(mp_name);
			if (mac_p != 0) {
				/* Eureka! We found it */
				if (quoted == YES && mac_p->quoted_offset == 0) {
					stringify(mac_p);
				}
				return(mac_p);
			}
		}
	}

	/* It wasn't a macro parameter.
	 * Try treating as just an ordinary macro name */
	mac_p = findMacro(basename);
	if (mac_p != 0 && quoted == YES && mac_p->quoted_offset == 0) {
		stringify(mac_p);
	}
	return(mac_p);
}


/* Return YES if macro name reference includes designator denoting
 * it is to be quoted (like usage of # in ANSI C preprocessor) */

static int
has_quote_designator(macname)

char *macname;

{
	return (strncmp(macname, quote_designator, strlen(quote_designator))
							== 0 ? YES : NO );
}


/* For "preprocessor" option, similar to the C compiler option to
 * just run the macro preprocessor, instead of the usual yacc-generated
 * yyparse(), we have this function that simply writes tokens out.
 */

void
preproc()
{
	while (yylex() != 0) {
		/* In strings, the backslashes before any embedded quotes
		 * will have been swallowed, so we have to recreate them. */
		if (yytext[0] == '"') {
			char *t;
			putchar('"');
			for (t = yytext + 1; *t != '\0'; t++) {
				if (*t == '"' && *(t+1) != '\0') {
					putchar('\\');
				}
				putchar(*t);
			}
		}
		else {
			printf("%s", yytext);
		}
	}
}


/*
 * This is called whenever the user does savemacros.
 * It increases the size of the array of pointers to saved macro hash tables,
 * and clones the current contents of Mactable into a new table
 * pointed to by the new entry.
 */

void
mac_saveto(name)

char *name;	/* name user will use for "restoremacros" */

{
	int new_index;	/* into array of saved macro tables */


	debug(4, "saving macros to %s\n", name);
	/* Make the array of pointers to the saved macros tables one larger */
	Num_mac_tables++;
	if (Num_mac_tables == 1) {
		/* This is the first save, so create the array */
		MALLOCA(struct MACRO **, Saved_mac_tables, 1);
	}
	else {
		REALLOCA(struct MACRO **, Saved_mac_tables, Num_mac_tables);
	}

	new_index = Num_mac_tables - 1;

	/* Allocate the new table itself */
	CALLOCA(struct MACRO *, Saved_mac_tables[new_index], MTSIZE);

	/* Associate the index number in the array
	 * with the user's "save to" name, so that if they later
	 * ask to restore from that, we know where to restore from.
	 */
	add_savemacs(name, new_index);

	/* Clone the existing table */
	clone_mac_table(Mactable, Saved_mac_tables[new_index]);
}


/* This is called when user does restoremacros.
 * It restores the "real" macro table to the saved one matching the name
 * they specify. If the name was not previous saved to, it prints an error
 * and leaves the table unchanged.
 */

void
mac_restorefrom(name)

char *name; /* which "saveto" to restore */
 
{
	int new_index;	/* our array slot associated with the user's name */

	debug(4, "restoring macros from %s\n", name);
	new_index = find_savemacs(name);
	if (new_index < 0) {
		l_yyerror(Curr_filename, yylineno,
			"no savemacros \"%s\" was done, so cannot do restoremacros from it", name);
		return;
	}

	/* Copy the correct saved table to be the "real" table */
	clone_mac_table(Saved_mac_tables[new_index], Mactable);
}


/* Copy one macro table to another. When saving, this will be called to
 * copy from the "real" table to a saved table. When restoring, it will be
 * called to copy from a saved table to the "real" table.
 */

static void
clone_mac_table(src_tbl, dest_tbl)

struct MACRO **src_tbl;		/* copy from this table... */
struct MACRO **dest_tbl;	/* ... to this table. */

{
	int t;		/* index through current table to be copied */
	struct MACRO *src_entry_p;
	struct MACRO *dest_entry_p;
	struct MACRO **dest_entry_p_p;	/* where to link in new copy */
	struct MACRO *next;	/* to save ->next before deleting */


	/* Loop though table, processing each hash collision chain */
	for (t = 0; t < MTSIZE; t++) {
		/* If we are doing a restore, the dest table could have
		 * existing entries, which we should free. */
		for (dest_entry_p = dest_tbl[t]; dest_entry_p != 0;
					dest_entry_p = next) {
			next = dest_entry_p->next;
			FREE(dest_entry_p);
		}
		dest_tbl[t] = 0;

		/* Now copy from source to destination */
		for (src_entry_p = src_tbl[t],
				dest_entry_p_p = &(dest_tbl[t]);
				src_entry_p != 0;
				src_entry_p = src_entry_p->next,
				dest_entry_p_p = &((*dest_entry_p_p)->next) ) {
			MALLOC(MACRO, *dest_entry_p_p, 1);
			memcpy(*dest_entry_p_p, src_entry_p, sizeof(struct MACRO));
		}
	}
}


/* If the user input including something like A##B, we look up both A and
 * B as macro names or parameter names, concatenate the contents of those
 * macros, which is expected to yield a valid macro name, and then use the
 * content of that macro. The whole thing can optionally be inside ` `
 * which causes those final contents to be turned into a double quoted string.
 * There can be multiple ## operators in a single item.
 * Example:
 *	define A RE@
 *	define B SU@
 *	define C LT@
 *	define RESULT This is the final result.@
 *	print `A##B##C`
 * would  yield
 *	print "This is the final result."
 */

void
macro_concat(concat_name)

char *concat_name; /* the A##B style name to be concatenated. */

{
	char *start_p;		/* points into concat_name to the beginning
				 * of the currently-being-processed segment */
	char *end_p;		/* end of current segment */
	char *white_p;		/* location of white space, if any */
	char concated_name[MAX_CONCAT_NAME_LEN];	/* resulting macro name
				 * after stitching together all the contents
				 * of the individual segment. */
	int index;		/* cursor into the concated_name.
				 * This is where to write the
				 * next character as we are filling it in. */
	char save;		/* original character at a point where we
				 * want to temporarily put a NULL to 
				 * end a segment for processing. */
	int c;			/* character read from macro file */
	int quote_count = 0;	/* how many ` seen */

	index = 0;
	/* Split into individual macro names at each ## */
	for (start_p = end_p = concat_name;  ; end_p++) {

		if (*end_p == '`' && start_p == concat_name) {
			/* Starts with stringify marker, so carry that through
			 * to the macro name we are building, but for purposes
			 * of looking up the macro name of this first segment,
			 * we don't want that included. */
			concated_name[index++] = '`';
			start_p++;
			quote_count++;
			continue;
		}

		switch (*end_p) {
		case '#':
		case '\0':
		case '`':
			/* We have gotten to the end of the current segment.
			 * In the latter two cases, We're also at the end of
			 * the entire item, but that isn't really relevant
			 * for this switch statement; that will be handled
			 * in the following switch.
			 * We temporarily add a null to terminate this segment,
			 * then invoke macro_call on this segment, which will
			 * arrange to read that macro's content from the
			 * macro file.
			 */
			save = *end_p;
			*end_p = '\0';
			/* Snip off any leading and trailing white space */
			start_p += strspn(start_p, " \t");
			if ((white_p = strpbrk(start_p, " \t")) != 0) {
				*white_p = '\0';
			}
			if (macro_call(start_p) == NO) {
				/* something went wrong */
				return;
			}

			/* We read the contents on the macro, and transfer that
			 * to what will be the ultimate macro name we need.
			 */
			for ( ; ; ) {
				c = getc(yyin);
				if (c == EOF) {
					pfatal("unexpected end of file in macro");
				}
				if (isspace(c)) {
					/* ignore any white space */
					continue;
				}

				/* We need to end up with a valid macro name,
				 * so anything we find better be legal
				 * for a macro name. */
				if ( ! isupper(c) && ! isdigit(c) && c != '_' && c != '\0') {
					l_yyerror(Curr_filename, yylineno,
						"Contents of %s portion of concatenated macro contains character '%c' which is not legal for a macro name.",
						start_p, c);
					/* Put input back the way it was,
					 * to recover as best we can, */
					popfile();
					return;
				}

				/* -2 to be safe, especially for the ` case */
				if (index >= MAX_CONCAT_NAME_LEN - 2) {
					pfatal("result of ## is too long");
				}

				concated_name[index] = c;

				if (concated_name[index] == '\0') {
					/* We have reached the end of the
					 * last segment, so time to jump out
					 * and look up the newly constructed
					 * concatenated name. */
					break;
				}
				index++;
			} 
			/* Done reading the current segment's macro contents */
			popfile();

			*end_p = save;
			switch (save) {
			case '#':
				/* There is another segment, so move our
				 * pointers to be ready to process it. */
				start_p = end_p + 2;
				end_p++;
				break;
			case '`':
				/* Done with stringified */
				concated_name[index] = '\0';
				quote_count++;
				break;
			case '\0':
				concated_name[index] = '\0';
				break;
			default:
				pfatal("impossible case in macro_concat");
				break;
			}
			break;
		default:
			break;
		}
		if (*end_p == '\0' || *end_p == '`') {
			break;
		}
	}

	if (quote_count != 0 && quote_count != 2) {
		l_yyerror(Curr_filename, yylineno,
			"unmatched ` on concatenated macro name");
	}

	/* We have the actual final macro that the user wants, so call it. */
	call_macro(concated_name);
}


/* Given info about a macro, created a "string-ified" version of it.
 * I.e, put double quotes around it,
 * and add backslashes to embedded double quotes.
 */
static void
stringify(mac_p)

struct MACRO *mac_p;

{
	int c;	/* character read from the unquoted macro value */
	int in_string = 0;	/* YES if inside a string that is inside the
				 * thing we are string-izing. */
	int escaped = NO;	/* If we have seen a backslash to escape the
				 * following character. */


#ifndef UNIX_LIKE_FILES
	if ((Mactmp_write = fopen(Macfile, Append_mode)) == (FILE *) 0) {
		pfatal("can't open temporary file for macro storage (even though could earlier)");
	}
#endif

	/* Remember where we are stashing this quoted copy */
	fseek(Mactmp_write, 0L, SEEK_END);
	mac_p->quoted_offset = ftell(Mactmp_write);

	/* Add the initial quote */
	putc('"', Mactmp_write);

	/* Arrange to read the unquoted macro value from the macro file */
	call_macro(mac_p->macname);

	/* Have to copy a character at a time, because we
	 * need to backslash any embedded quotes. This follows
	 * rules like the ANSI C preprocessor, except we only have
	 * to worry about strings, not character constants, because
	 * Mup doesn't have character constants. Also, we don't
	 * squeeze not-in-string white space runs to a single space,
	 * because this keeps the code simpler and there's no
	 * particular benefit in squeezing, other than perhaps
	 * saving a few bytes in the macro temp file. This code
	 * is similar to the gcc implementation of cpp. */
	in_string = escaped = NO;
	while ((c = getc(Mactmp_read)) != '\0') {
		if (escaped == YES) {
			escaped = NO;
		}
		else {
			if (c == '\\') {
				escaped = YES;
			}
			if (in_string == YES) {
				if (c == '"') {
					/* reached end of string */
					in_string = NO;
				}
			}
			else if (c == '"') {
				/* starting a string */
				in_string = YES;
			}
		}

		/* Escape quotes always; escape backslashes
		 * when they are inside strings. */
		if (c == '"' || (in_string == YES && c == '\\')) {
			putc('\\', Mactmp_write);
		}
		putc(c, Mactmp_write);
	}

	/* Add the final quote */
	putc('"', Mactmp_write);
	putc('\0', Mactmp_write);
	finish_mac_write();

	/* put the input back where it was */
	popfile();
}


/* Save the result of an "eval" macro. */

void
do_eval()

{
	/* Write the evaluated result into the macro tmp file */
	setup_macro(Curr_expr_macro.macname, NO, SETTING_EXPR);
	if (Expr_result.type == TYPE_INT) {
		fprintf(Mactmp_write, "%d", Expr_result.intval);
	}
	else if (Expr_result.type == TYPE_FLOAT) {
		if ( isnan(Expr_result.floatval)
				|| isinf(Expr_result.floatval)
#ifndef __DJGPP__
		 		|| fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW)
#endif
				) {
			yyerror("expression resulted in invalid floating point number");
			/* Force to something valid to try to avoid any
			 * future issues. */
			Expr_result.floatval = 1.0;
		}
		fprintf(Mactmp_write, "%f", Expr_result.floatval);
	}
	else {
		pfatal("invalid value type %d\n", Expr_result.type);
	}
	putc('\0', Mactmp_write);
	finish_mac_write();
	/* We no longer need the "temporary" macro that was the expression
	 * before evaluation. The text of the expression will still in the
	 * temp file, but we never bother to try to reclaim anything in there.
	 */
	FREE(Curr_expr_macro.macname);
	Curr_expr_macro.macname = 0;
}
