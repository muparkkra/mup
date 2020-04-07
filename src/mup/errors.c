
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

/* Thi file contains various functions for printing warning and error messages,
 * and exiting when things go wrong.
 */

#include "globals.h"
#include "rational.h"
#ifdef CORE_MESSAGE
#include <sys/resource.h>
#endif

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef Mac_BBEdit
#include <BBEdit_Streams.h>
#endif

extern void exit P((int status));

	int debug_on P((int level));

#ifdef __STDC__
	extern void abort P((void));
#else
	extern int abort();
#endif

static void error_header P((char *filename, int lineno, char * errtype));

/* Print a message for a user error, and exit with the value of Errorcount,
 * or of MAX_ERRORS if > MAX_ERRORS */

/*VARARGS1*/
#ifdef __STDC__

void
ufatal(char *format, ...)

#else

void
ufatal(format, va_alist)

char *format;	/* printf style format */
va_dcl

#endif

{
	va_list args;

	/* we now have one more error */
	Errorcount++;

	/* print the specified message with a newline */
#ifdef __STDC__
	va_start(args, format);
#else
	va_start(args);
#endif

#ifndef UNIX_LIKE_FILES
	mac_cleanup();
#endif

	(void) fprintf(stderr, "\n! Fatal user error: ");
	(void) vfprintf(stderr, format, args);
	va_end(args);
	(void) fprintf(stderr, "\n");

	error_exit();
}

/* Print a message with filename and linenumber for a user error,
 * and exit with the value of Errorcount, or of MAX_ERRORS if > MAX_ERRORS */

/*VARARGS3*/
#ifdef __STDC__

void
l_ufatal(char *filename, int lineno, char *format, ...)

#else

void
l_ufatal(filename, lineno, format, va_alist)

char *filename;
int lineno;
char *format;	/* printf style format */
va_dcl

#endif

{
	va_list args;

	/* we now have one more error */
	Errorcount++;

	/* print the specified message with a newline */
#ifdef __STDC__
	va_start(args, format);
#else
	va_start(args);
#endif

#ifndef UNIX_LIKE_FILES
	mac_cleanup();
#endif

	error_header(filename, lineno, "! Fatal user error");

	(void) vfprintf(stderr, format, args);
	va_end(args);
	(void) fprintf(stderr, "\n");

#ifdef Mac_BBEdit
	AppendError(filename, lineno);
#endif

	error_exit();
}


/* Print a message for a program internal error and exit with MAX_ERRORS */


/*VARARGS1*/

#ifdef __STDC__

void
pfatal(char *format, ...)

#else

void
pfatal(format, va_alist)

char *format;
va_dcl

#endif

{
	va_list args;
#ifdef CORE_MESSAGE
	struct rlimit limit;
#endif

#ifdef __STDC__
	va_start(args, format);
#else
	va_start(args);
#endif

#ifndef UNIX_LIKE_FILES
	mac_cleanup();
#endif

	/* print specified message with newline */
	(void) fprintf(stderr, "\n! Fatal internal error: ");
	(void) vfprintf(stderr, format, args);
	va_end(args);
	(void) fprintf(stderr, "\n");

	(void) fprintf(stderr, "Please report this error to support@arkkra.com,\nstating the text of the error, what Operating System you are using,\nand the Mup input that triggered the error, if possible.\n\n");
#ifdef CORE_MESSAGE
	if (getrlimit(RLIMIT_CORE, &limit) == 0 && limit.rlim_cur > 0) {
		(void) fprintf(stderr, "Attempting to create a core dump.\n");
	}
#endif
	abort();

	Errorcount = MAX_ERRORS;
	error_exit();
}


/* fatal error with line number and file name. Note that this should be the
 * line number and file in the program, not the input file, ie, __LINE__
 * and __FILE__. */

/*VARARGS3*/

#ifdef __STDC__

void
l_pfatal(char *filename, int lineno, char *format, ...)

#else

void
l_pfatal(filename, lineno, format, va_alist)

char *filename;		/* name of program file */
int lineno;		/* pgm line where error was discovered */
char *format;		/* printf format */
va_dcl

#endif

{
	va_list args;

#ifdef __STDC__
	va_start(args, format);
#else
	va_start(args);
#endif

	/* The actual header line with be printed by pfatal(),
	 * so we pass an empty string here. */
	error_header(filename, lineno, "");
	pfatal(format, args);
	va_end(args);
}


/* error exit for the common problem of malloc failures */

void
l_no_mem(filename, lineno)

char *filename;
int lineno;

{
	l_pfatal(filename, lineno, "memory allocation failed");
}


/* error exit for common error of not being able to open a specified file */

void
cant_open(filename)

char *filename;

{
	ufatal("can't open '%s'", filename);
}


/* Exit with exit code being the number of errors, unless there were
 * too many of them to fit in an exit code, in which case MAX_ERRORS
 * is used. MAX_ERRORS is used for internal errors. */

void
error_exit()

{
	exit(Errorcount > MAX_ERRORS ? MAX_ERRORS : Errorcount);
}


/* print a warning message */

/*VARARGS1*/

#ifdef __STDC__

void
warning(char *format, ...)

#else

void
warning(format, va_alist)

char *format;
va_dcl

#endif

{
	va_list args;

	if (Score.warn == NO) {
		return;
	}
#ifdef __STDC__
	va_start(args, format);
#else
	va_start(args);
#endif

	(void) fprintf(stderr, "- Warning: ");
	(void) vfprintf(stderr, format, args);
	(void) fprintf(stderr, "\n");
	va_end(args);

	/* if doing macro expansion, also tell where macro was defined */
	mac_error();
	/* similar for when expanding emptymeas parameter */
	emptym_err("warning");

#ifdef Mac_BBEdit
	AppendWarning((char *) 0, -1);
#endif
}


/* warning message with file name and line number */

/*VARARGS3*/

#ifdef __STDC__

void
l_warning(char * filename, int lineno, char *format, ...)

#else

void
l_warning(filename, lineno, format, va_alist)

char *filename;	/* name of program file */
int lineno;		/* pgm line where error was discovered */
char *format;		/* printf format */
va_dcl

#endif

{
	va_list args;

	if (Score.warn == NO) {
		return;
	}
#ifdef __STDC__
	va_start(args, format);
#else
	va_start(args);
#endif

	error_header(filename, lineno, "- Warning");
	(void) vfprintf(stderr, format, args);
	(void) fprintf(stderr, "\n");
	va_end(args);

	/* if doing macro expansion, also tell where macro was defined */
	mac_error();
	/* similar for when expanding emptymeas parameter */
	emptym_err("warning");

#ifdef Mac_BBEdit
	AppendWarning(filename, lineno);
#endif
}


/* varargs version of yyerror, passing a file and linenumber (or -1 for the
 * lineno if you don't want a filename and linenumber printed) */

/*VARARGS3*/

#ifdef __STDC__

void
l_yyerror(char *fname, int lineno, char *format, ...)

#else

void
l_yyerror(fname, lineno, format, va_alist)

char *fname;
int lineno;
char *format;
va_dcl

#endif

{
	va_list args;


	/* if linenumber is zero or negative, assume this is special case of
	 * not being associated with a specific line, so don't print
	 * a line number */
	if (lineno > 0) {
		error_header(fname, lineno, "! Error");
	}

#ifdef __STDC__
	va_start(args, format);
#else
	va_start(args);
#endif

	(void) vfprintf(stderr, format, args);
	va_end(args);
	(void) fprintf(stderr, "\n");

	/* if doing macro expansion, also tell where macro was defined */
	mac_error();
	/* Similarly, tell them about emptymeas parameter expansions */
	emptym_err("error");

#ifdef Mac_BBEdit
	AppendError(fname, lineno);
#endif

	Errorcount++;
}




/* print a debugging message if corresponding debugging bit is on */

/*VARARGS2*/

#ifdef __STDC__

void
debug(int level, char *format, ...)

#else

void
debug(level, format, va_alist)

int level;		/* debugging flag bitmap */
char *format;		/* printf style format */
va_dcl

#endif

{
	va_list args;

	if (debug_on(level)) {
#ifdef __STDC__
		va_start(args, format);
#else
		va_start(args);
#endif
		(void) vfprintf(stderr, format, args);
		va_end(args);
		(void) fprintf(stderr, "\n");
	}
}


/* return AND of Debuglevel and argument. Useful for other debug functions
 * that want to see if a given debug level is currently turned on */

int
debug_on(level)

int level;

{
	return(Debuglevel & level);
}

/* if we get an error while doing rational arithmetic, we are in deep
 * trouble, so print message and get out. */

void
doraterr(code)

int code;

{
	switch (code) {

	case RATOVER:
		pfatal("rational overflow");
		/*NOTREACHED*/
		break;

	case RATDIV0:
		pfatal("rational division by zero");
		/*NOTREACHED*/
		break;

	case RATPARM:
		pfatal("invalid rational number parameter");
		/*NOTREACHED*/
		break;

	default:
		pfatal("error in rational arithmetic routines");
		/*NOTREACHED*/
		break;
	}
}


/* Print header for an error report. If the error is associated with a
 * particular line, the file name and line number and the text of the line
 * is printed, but only on the first of multiple errors for the same line.
 * If not associated with any line, a blank line is produced.
 */

static void
error_header(filename, lineno, errtype)

char * filename;
int lineno;
char * errtype;		/* "warning" or "error" etc */

{
	static char *cached_filename = 0;
	static int cached_lineno = -1;

	if (filename == 0 || lineno <= 0) {
		(void) fprintf(stderr, "\n");
		return;
	}

	/* We print the text of the offending line, unless it is the
	 * same as the last error, in which case we could have already
	 * printed it, so no need to print again. */
	if (cached_filename != filename || cached_lineno != lineno) {
		(void) fprintf(stderr,"\n%s: line %d:\n", filename, lineno);
		print_offending_line(filename, lineno);
	}
	if (strlen(errtype) > 0) {
		(void) fprintf(stderr,"%s: ", errtype);
	}
	cached_filename = filename;
	cached_lineno = lineno;
}


/* Print the text of input line where error was found. */

void
print_offending_line(filename, lineno)

char *filename;
int lineno;

{
	/* We cache file info to save time when multiple errors */
	static FILE *f = 0;
	static char *prev_filename = 0;
	static int prev_lineno = 0;
	int inp;	/* a byte read from file */
	int skipcount;	/* how many lines to skip past */
	int count = 0;	/* how many characters printed */

	/* We try to reuse already opened file to save time,
	 * but need to open new file when necessary. */
	if (f == 0 || prev_filename != filename) {
		/* close any previously open file */
		if (f != 0) {
			(void) fclose(f);
		}
		/* Note that if Mup is reading from stdin, we will try to
		 * open a file entitled "stdin" here. That will almost
		 * certainly fail, but we can't easily get the line from
		 * stdin anyway, so user just won't get the context in that
		 * case. People who are having Mup read from stdin are probably
		 * savvy enough that this isn't a big problem. (We never used
		 * to print this context ever, and no one complained.)
		 * So the only weird case is if the user happens to have a
		 * file whose name is literally "stdin" but the actual stdin
		 * being read is some other file, in which case they'll get
		 * garbage. But anyone using stdin for a file name is probably
		 * smart enough to figure out the strange results in that case.
		 * In the case of include files found via $MUPPATH,
		 * filename will have already been expanded to a full path,
		 * so we don't have to do anything special here for them.
		 */
		if ((f = fopen(filename, "r")) == 0) {
			return;
		}
		skipcount = lineno - 1;
		prev_filename = filename;
	}
	else if (lineno > prev_lineno) {
		/* We can continue where we left off in the file */
		skipcount = lineno - prev_lineno - 1;
	}
	else {
		/* Earlier line in same file; easiest to just start over.
		 * This could happen, because sometimes we don't realize
		 * there is an error until a later line (E.g., only when
		 * getting to bar line we find inconsistency in the 
		 * contents of the bar.) */
		rewind(f);
		skipcount = lineno - 1;
	}

	/* Skip to the line of interest and print it. */
	for (inp = 0; skipcount > 0; skipcount--) {
		/* We read byte-by-byte so we don't need to guess
		 * how big a buffer to use to hold a line.
		 */
		while ((inp = getc(f)) != '\n' && inp != EOF) {
			;
		}
		if (inp == EOF) {
			break;
		}
	}
	if (inp != EOF) {
		(void) fprintf(stderr, "    ");
		while ((inp = getc(f)) != '\n' && inp != EOF) {
			/* If Mup is run on a binary file, what appears to
			 * be a "line" of input could be really long garbage,
			 * so limit to about 250 bytes. Limit is arbitrary,
			 * but valid Mup input is typically very rarely over
			 * 100, and this should be more than enough to tell
			 * user the approximate context. We could also try
			 * to filter out non-printable stuff, but it is unclear
			 * what can safely be filtered and what to do with
			 * the filtered characters.
			 * Latin-1 characters are outside of ASCII range,
			 * so should be left as is. Null could
			 * probably be discarded, but maybe printing \0 would
			 * be better? Or something like "<null>"? Or "^@"?
			 * For now, if user runs Mup on garbage,
			 * they get garbage. */
			if (count++ > 250) {
				fprintf(stderr, " ...more...");
				break;
			}
			/* a bare carriage return with no newline
			 * gets translated to \r to be easier to see */
			if (inp == '\r') {
				int peek;
				peek = getc(f);
				if (peek != '\n') {
					putc('\\', stderr);
					putc('r', stderr);
				}
				else {
					putc(inp, stderr);
				}
				ungetc(peek, f);
			}
			else {
				putc(inp, stderr);
			}
		}
		putc('\n', stderr);
	}
	prev_lineno = lineno;
	/* Note that we leave the file open, to save time
	 * in case we need to read more from it
	 * due to additional errors from the same file.
	 */
}
