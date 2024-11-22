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

/* This is the main include file for the mupdisp program that displays
 * Mup output using Ghostscript.
 */


#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#ifndef __APPLE__
#include <malloc.h>
#endif
#if !defined(linux) && !defined(__EMX__) && !defined(__APPLE__)
/* undef SIGCHLD to avoid conflict with Xos.h */
#undef SIGCHLD
#endif
#include <errno.h>

#ifdef __WATCOMC__
#ifndef __DOS__
#define __DOS__ 1
#endif
#endif

#ifdef DJGPP
#ifndef __DOS__
#define __DOS__
#endif
#endif

#if defined(__DOS__) || defined (__EMX__)
#include <io.h>
#endif

#ifdef __APPLE__
#ifndef unix
#define unix
#endif
#endif

#ifdef __EMX__
#define unix	/* not really unix, but acts like it */
#endif
#include "dispttyp.h"

#ifdef XWINDOW
#ifdef linux
#undef SYSV
#undef _USHORT_H
#endif
/* X window includes */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#ifndef __APPLE__
/* define XK_MISCELLANY so we can use XK_Return, etc */
#ifndef XK_MISCELLANY
#define XK_MISCELLANY 1
#endif
#endif
#include <X11/keysymdef.h>
#endif

/* macro for function templates if using ANSI C */
#ifdef __STDC__
#define	P(parms)	parms
#else
#define	P(parms)	()
#endif

#define YES     1
#define NO      0

/* define screen and page dimensions */
/* Supported page sizes are
 * letter, note, legal, a3, a4, a5, a6, flsa, flse, and halfletter,
 * portrait or landscape  */
#define MAX_BYTES_PER_LINE	149	/* 1190/8 rounded up for a3 landscape */
#define MAX_LINES_PER_PAGE	1190	/* to handle a3 size */

#define BITS_PER_LINE   Bits_per_line    /* horizontal pixels */
#define BYTES_PER_LINE  Bytes_per_line   /* Bits_per_line / 8 rounded up */
#define LINES_PER_PAGE  Lines_per_page   /* vertical pixels */
#define BYTES_PER_PAGE  (BYTES_PER_LINE * LINES_PER_PAGE)


/* default mode is not full page */
#define DFLT_MODE       NO

typedef void (*FUNC)();

/* list of supported $TERM types and which functions and parameters to use
 * to implement them. To support a new terminal type, write appropriate
 * functions, and add to the Config table in init.c.
 */
struct CONFIG {
	char    *termname;      /* $TERM value */
	FUNC    setup;          /* call this to initialize */
	FUNC    cleanup;        /* call this to clean up and exit */
	FUNC    draw;           /* call this to draw a screen of the page */
	FUNC    user_interf;    /* call this to read user input */
	FUNC    error;          /* call this on bad user input */
	FUNC    bitmap;         /* call this to display a bitmap */
	int     vlines;         /* number of lines vertically on screen */
	int	max_width;	/* if  > 320, limit to no more than this many pixels wide */
	float   adjust;         /* aspect ratio adjustment */
};


/* information about a particular bitmap-ed page. */
struct Pginfo {
	int     pagenum;        /* actual designated page number. If the mup
				 * -p option is used, this may start somewhere
				 * other than 1, and if -o is used, there
				 * may be gaps in the list */
	int     seqnum;         /* page number from 0 to n-1. Multiplying this
				 * by the number of bytes per page in a bitmap
				 * gives the file offset for the page. */
	long    begin;          /* where page begins in input */
	long    end;            /* where page ends in input */
	struct Pginfo   *next;  /* linked list link */
	struct Pginfo   *prev;
};

/* globals */
extern struct CONFIG *Conf_info_p;
extern struct Pginfo *Pagehead; /* all page bitmaps */
extern struct Pginfo *Pagetail; /* where to add to list */
extern struct Pginfo *Currpage_p;       /* current page */
extern int Fullpgmode;  /* full page or partial page */
extern long Beginprolog;/* where in PostScript file the prolog begins */
extern long Endprolog;  /* where in PostScript file the prolog ends */
extern long Begin_offset;/* offset in file where current page begins */
extern int Pagenum;     /* current page number */
extern int Psfile;      /* PostScript temp file, file descriptor */
extern FILE *PS_file;   /* PostScript temp file */
extern int Fullbitmaps; /* temp file of full page bitmaps */
extern int Partbitmaps; /* temp file for bitmaps for scrollable pages */
extern int Nulldev;     /* /dev/null */
extern char Fullfile[]; /* name of gs output tmp file */
extern char Partfile[]; /* name of gs output tmp file */
extern char Mupfile[];  /* mup output temp file */
extern char **Argv;     /* global version of argv */
extern int Argc;        /* global version of argc */
extern char *Exit_errmsg;/* error message to print upon exit */
extern char *Gs_errfile;/* Ghostscript error file */
extern int Bits_per_line; /* pixels per line */
extern int Bytes_per_line;/* pixels per line divided by 8 and rounded up */
extern int Lines_per_page;/* vertical pixels */

/* misc functions */
extern int getpginfo P((int pgnum));
extern void user_interf P((void));
extern int scroll P((int line, int distance));
extern void generalcleanup P((int status));
extern int create_tmpfile P((char *tmpfname));
extern void set_gs_params P((void));
extern int gen1file P((int fullpgmode));
extern void get_paper_size P((int x, int y));
extern void landscape P((void));
extern void run_mup P((char **argv));
extern void init P((void));
extern void do_cmd P((int c));

extern char *getenv();
extern long ftell();
#ifdef __STDC__
#include <unistd.h>
#else
extern long lseek();
#endif
