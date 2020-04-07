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


/* Program to display ghostscript bitmap output on screen.
 * Works either on an AT386 or linux console
 * or under X-windows from an xterm window or under DOS (with Watcom C).
 * It could be extended to other
 * TERM types by writing appropriate functions and adding to the Config
 * array. Use the existing functions as models.
 * Passes all arguments on to Mup.
 */

/* for compiling under UNIX on x86, try
 *      cc -DSYSV -D_USHORT_H -s -O -o mupdisp *.c -lX11 -lnsl_i
 * If you compile without XWINDOW, you can get by with just:
 *      cc -s -O -o mupdisp *.c
 *
 * For Watcom C under DOS,
 * Put the following 4 lines in a batch script and execute it
 * 
 * 	for %%f in (*.c) do wcc386 %%f -dDEBUG -on -4r
 * 	echo NAME mup > mup.lnk
 * 	for %%f in (*.obj) do echo FIL %%f >> mup.lnk
 * 	wlink sys dos4g op st=32k @mup.lnk
 * 
 * Note that all the mupdisp *.c and *.h files should be in the
 * current directory, but there must be no other *.c files there, and no *.obj
 * files (except that *.obj files from a previous attempt would be okay).
 *
 * For Linux,
 *	cc -L/usr/X11/lib -o mupdisp *.c -lvga -lX11 -lm
 * Depending on the versions of libraries you have,
 * you might not really need the -lm, but it doesn't hurt.
 * If you don't have libvga on your system, and only intend to use the X11
 * mode, not the console mode, you can use
 *	cc -L/usr/X11/lib -o mupdisp -DNO_VGA_LIB *.c -lX11
 *
 * Other environments may require different options
 *
 * Note that using mupdisp in non-X-window mode on Linux requires that it
 * can write to the console device. To allow this, make mupdisp setuid to root:
 *	chown root mupdisp
 *	chmod 4755 mupdisp
 */


#include "mupdisp.h"
#ifdef __WATCOMC__
#include <io.h>
#endif

#if defined(SIGCHLD) && ! defined(SIGCLD)
#define SIGCLD SIGCHLD
#endif


struct CONFIG *Conf_info_p; /*  the info for the actual $TERM */
struct Pginfo *Pagehead = (struct Pginfo *) 0;  /* all page's bitmap info */
struct Pginfo *Pagetail = (struct Pginfo *) 0;  /* where to add to list */
struct Pginfo *Currpage_p;      /* current page */
int Pagenum;            /* current page number */
long Beginprolog;       /* where in PostScript file the prolog begins */
long Endprolog;         /* where in PostScript file the prolog ends */
long Begin_offset;      /* offset in file where current page begins */
int Psfile;             /* PostScript temp file, file descriptor */
FILE *PS_file;          /* PostScript temp file */
int Fullbitmaps;        /* temp file of full page bitmaps */
int Partbitmaps;        /* temp file for bitmaps for scrollable pages */
int Nulldev;            /* /dev/null */
#ifdef linux
char Fullfile[] = "mupdispfXXXXXX"; /* name of gs output tmp file, full page */
char Partfile[] = "mupdisppXXXXXX"; /* name of gs output tmp file, partial page */
char Mupfile[] = "mupdispmXXXXXX"; /* Mup output temp file */
#else
char Fullfile[L_tmpnam];        /* name of gs output tmp file, full page */
char Partfile[L_tmpnam];        /* name of gs output tmp file, partial page */
char Mupfile[L_tmpnam]; /* Mup output temp file */
#endif
char **Argv;            /* global version of argv */
int Argc;               /* global version of argc */
int Quiet;		/* set via -q option or $MUPQUIET */
int Fullpgmode = DFLT_MODE;     /* full page or partial page mode, YES if full */
char *Exit_errmsg = (char *) 0; /* error message to print upon exit, if any */
char *Gs_errfile = "mupdispg.err";      /* ghostscript error file */
int Bits_per_line = 612; 	/* pixels per line */
int Bytes_per_line = 77;	/* pixels per line divided by 8 rounded up */
int Lines_per_page = 792;	/* vertical pixels */
char *Version = "6.8";

/* misc function declarations */
static void parsePS P((FILE *file));
static void save_endpage P((void));



/* main function. Run Mup, run ghostscript, then do user interface */

int
main(argc, argv)

int argc;
char **argv;

{
	int n;


	/* initialize */
	Argv = argv;
	Argc = argc;
	init();

#ifdef unix
	/* arrange to clean up temp files. Note that the user interface
	 * will probably have its own cleanup */
	for (n = 0; n < NSIG; n++) {
		if (n != SIGKILL && n != SIGCLD && n != SIGWINCH) {
			signal(n, generalcleanup);
		}
	}
	signal(SIGWINCH, SIG_IGN);
#endif

	Quiet = NO;
	if (getenv("MUPQUIET") != 0) {
		Quiet = YES;
	}
	else {
		int a;
#ifdef __DOS__
		char option_marker = '/';
#else
		char option_marker = '-';
#endif
		/* Peek into the arguments to look for -q */
		for (a = 1; a < argc; a++) {
			if (argv[a][0] == option_marker && argv[a][1] == 'q') {
				Quiet = YES;
			}
		}
	}

	if (Quiet == NO) {
		fprintf(stderr, "Mupdisp - Version %s\n", Version);
	}

	/* There are several Mup options we want to turn off, because
	 * they don't produce PostScript output. This magic environment
	 * variable tells Mup to make the listed options illegal. */
	putenv("MUPDELOP=CEfFlmMv");

	/* make a temp file for PostScript */
	Psfile = create_tmpfile(Mupfile);
#ifdef linux
	/* The assignment is just to appease picky compiler that complains
	 * the return from chown is ignored. */
	n = chown(Mupfile, getuid(), getgid());
#endif

	/* run Mup with given arguments */
	run_mup(Argv);
	if ((PS_file = fopen(Mupfile, "r")) == (FILE *) 0) {
		fprintf(stderr, "can't open Mup output file\n");
		generalcleanup(1);
	}

	/* find where pages begin in PostScript file */
	parsePS(PS_file);

#ifdef unix
	if ((Nulldev = open("/dev/null", O_WRONLY, 0)) < 0) {
		fprintf(stderr, "can't open /dev/null\n");
		generalcleanup(1);
	}
#endif

	/* if environment variable MUPDISPMODE is set, use the small full page
	 * mode as the default */
	if (getenv("MUPDISPMODE") != (char *) 0) {
		Fullpgmode = YES;
	}

	/* do user interface */
	user_interf();
	return(0);
}


/* given a page number, set Currpage_p to the info for that page
 * and return YES. Else leave as is and return NO */

int
getpginfo(pgnum)

int pgnum;      /* which page */

{
	struct Pginfo *pginfo_p;


	/* use -1 as special page number to mean first on list */
	if (pgnum == -1) {
		Currpage_p = Pagehead;
		return(Currpage_p == (struct Pginfo *) 0 ? NO : YES);
	}

	/* search list for requested page */
	for (pginfo_p = Pagehead; pginfo_p != (struct Pginfo *) 0;
				pginfo_p = pginfo_p->next) {
		if (pginfo_p->pagenum == pgnum) {
			/* found it */
			Currpage_p = pginfo_p;
			return(YES);
		}
	}

	/* page not on list */
	return (NO);
}


/* set up and call appropriate user interface routine */

void
user_interf()

{
	/* init, draw first page, do user interface */
	( *(Conf_info_p->setup) )  ();
	getpginfo(-1);
	( *(Conf_info_p->draw) )  (0, Fullpgmode);
	( *(Conf_info_p->user_interf) )  ();
	( *(Conf_info_p->cleanup) )  (0);
}


/* check if scrolling by specified distance from line would leave the
 * whole screen area within the page. If so, redraw the screen with that
 * much of a scroll and return the new line number. Otherwise just return
 * the original line number.
 */

int
scroll(line, distance)

int line;
int distance;

{
	int newlineno;		/* line number after scrolling */
	int pagebotline;	/* bottom line of page to display */


	newlineno = line + (int)(distance * Conf_info_p->adjust);
	if (newlineno < 0 && line > 0) {
		newlineno = 0;
	}
	pagebotline = Conf_info_p->adjust * LINES_PER_PAGE - 1;
	if (newlineno + Conf_info_p->vlines - 1 > pagebotline) {
		newlineno = pagebotline - Conf_info_p->vlines + 1;
	}
	
	if ( (newlineno != line) && (newlineno >= 0)
			&& (newlineno + Conf_info_p->vlines - 1
			<= pagebotline) ) {
		( *(Conf_info_p->draw) )  (newlineno, Fullpgmode);
		return(newlineno);
	}
	if (Quiet == NO) {
		/* some people don't want to be beeped when hitting end of
		 * page, so only exclaim if quiet flag is off */
		( *(Conf_info_p->error) )  ("can't scroll any farther");
	}
	return(line);
}


/* general cleanup function to delete temp files. All other cleanup functions
 * should call this function last, since it exits */

void
generalcleanup(status)

int status;

{
	if (Mupfile[0]) {
		close(Psfile);
		if (PS_file != 0) {
			fclose(PS_file);
		}
		unlink(Mupfile);
	}
	if (Fullfile[0]) {
		close(Fullbitmaps);
		unlink(Fullfile);
	}
	if (Partfile[0]) {
		close(Partbitmaps);
		unlink(Partfile);
	}
	/* if there is an error message to print, do so */
	if (Exit_errmsg != (char *) 0) {
		fprintf(stderr, "%s", Exit_errmsg);
		
		/* if there is a ghostscript error file, print it */
		if (status != 0) {
			FILE *f;
			char buff[BUFSIZ];

			if ((f = fopen(Gs_errfile, "r")) != NULL) {
				while (fgets(buff, BUFSIZ, f)) {
					fprintf(stderr, "%s", buff);
				}
				fclose(f);
				unlink(Gs_errfile);
			}
		}
	}
	exit(status);
}


/* Create a temporary file. It is passed an array in which the filename is
 * stored. Newer versions of gcc claim that tmpnam is dangerous and that you
 * should use mkstemp instead. But some other systems say using mkstemp
 * is discouraged. Being unable to please everyone, we use mkstemp on linux
 * and tmpnam elsewhere. So on linux, the argument should be a character
 * array initialized to end with "XXXXXX" as per what mkstemp wants,
 * On other systems it should be a character array at least L_tmpnam bytes long.
 * Returns the file descriptor, opened read/write. */

int
create_tmpfile(char *tmpfname)

{
	int fd;


#ifdef linux
	if ((fd = mkstemp(tmpfname)) < 0) {
		fprintf(stderr, "can't create temp file\n");
		generalcleanup(1);
	}
#else
	/* create the file name */
	if (tmpnam(tmpfname) == (char *) 0) {
		fprintf(stderr, "can't create temp file\n");
		generalcleanup(1);
	}

	/* open the file */
#ifdef O_BINARY
	if ((fd = open(tmpfname, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0644)) < 0) {
#else        
	if ((fd = open(tmpfname, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
#endif
		fprintf(stderr, "can't open temp file\n");
		generalcleanup(1);
	}
#endif

	return(fd);
}


/* 
 * Read a PostScript file and save pointer to where the description of
 * each page begins.
 * Go through input file. Skip to %%EndProlog. Then for each page,
 * save file offset where page begins.
 * This function know about how Mup formats parts of its output. If Mup
 * ever changes to add extra white space or comments or something in
 * the specific lines this function cares about, this function will
 * have to change too.
 */

static void
parsePS(file)

FILE *file;

{
	char buff[BUFSIZ];
	long linebegin;		/* where in file current line begins */


	/* read whole file */
	linebegin = ftell(file);
	while (fgets(buff, BUFSIZ, file)) {

		if (strncmp(buff, "%!PS-Adobe", 10) == 0) {
			/* remember where prolog begins. Because of how DOS
			 * deals with cr/nl we can't just back up the length
			 * of the string from the current ftell position and
			 * be assured of being at the %, so that's why we
			 * save the current beginning of each line and then
			 * assign it here. Normally, the %!Adobe line will
			 * be the first line anyway and we wouldn't need this
			 * code at all, but some versions of dos4gw write stuff
			 * to stdout, which ends up in the PostScript file,
			 * which then confuses us, so we want to throw that
			 * away if it is present. */
			Beginprolog = linebegin;
		}
		else if (strncmp(buff, "%%BoundingBox:", 14) == 0) {
			int x, y;

			/* adjust for page size */
			if (sscanf(buff + 14, "%*d %*d %d %d", &x, &y) == 2) {
				if ( (x <= MAX_BYTES_PER_LINE * 8)
						&& (x > 0) && (y > 0) &&
						(y <= MAX_LINES_PER_PAGE) ) { 
					get_paper_size(x, y);
				}
				else {
					fprintf(stderr, "Page is too big to display completely\n");
				}
			}
		}
		else if (strncmp(buff, "%%Orientation: Landscape", 24) == 0) {
			landscape();
		}
		else if (strncmp(buff, "%%EndProlog", 11) == 0) {
			/* remember where prolog ends */
			Endprolog = ftell(PS_file);
		}

		else if (strncmp(buff, "%%Page: ", 8) == 0) {
			Pagenum = atoi(buff + 8);
		
			/* Page followed by save, save info about page */
			if (fgets(buff, BUFSIZ, file) == 0) {
				fprintf(stderr, "failed to read information about page %d\n", Pagenum);
				generalcleanup(1);
			}
			if (strncmp(buff, "save", 4) == 0) {
				Begin_offset = linebegin;
			}
		}

		else if (strncmp(buff, "showpage", 8) == 0) {
			/* for showpage, save info about where page ends */
			save_endpage();
		}
		linebegin = ftell(file);
	}
	
	/* file was not valid -- something went wrong */
	if (Endprolog == 0) {
		generalcleanup(1);
	}
}


/* at the end of a PostScript page, save info about it */

static void
save_endpage()

{
	struct Pginfo *new_p;   /* newly allocated struct for info about page */
	static int seqnum = 0;      /* sequential count of pages */


	/* allocate space to save info about page */
	if ((new_p = (struct Pginfo *) malloc (sizeof(struct Pginfo)))
						== (struct Pginfo *) 0) {
		fprintf(stderr, "malloc failed\n");
		generalcleanup(1);
	}

	/* fill in info */
	new_p->pagenum = Pagenum;
	new_p->seqnum = seqnum++;
	new_p->begin = Begin_offset;
	new_p->end = ftell(PS_file);
	new_p->prev = Pagetail;
	new_p->next = (struct Pginfo *) 0;

	/* link onto list */
	if (Pagehead == (struct Pginfo *) 0) {
		Pagehead = new_p;
	}
	if (Pagetail != (struct Pginfo *) 0) {
		Pagetail->next = new_p;
	}
	Pagetail = new_p;
}
