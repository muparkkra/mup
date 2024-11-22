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

/* This file contains the Config table that defines which functions to call
 * for the mupdisp program that displays Mup output via Ghostscript,
 * as well as the init() function that figures out
 * which Config table entry to use, based on terminal type.
 * When adding support for additional terminal types, you will need to
 * add the declarations of relevant functions and add an entry to the
 * Config table.
 */


#include <string.h>
#include <stdlib.h>
#include "mupdisp.h"

/* declare the functions, etc for each supported terminal type */

#ifdef XWINDOW
extern void parse_X_options P((void));
extern void xterm_setup P((void));
extern void xterm_cleanup P((int status));
extern void xterm_draw P((int line, int small));
extern void xterm_user_interf P((void));
extern void xterm_error P((char *msg));
extern void xterm_raster P((unsigned char *bitmap, int width, int height));
#define XVIDLINES       (400)   /* request 400 lines when in 640x480 mode */
#endif

#if defined(AT386) && ! defined(linux) && ! defined(__DOS__) && ! defined(__APPLE__)
extern void at386_setup P((void));
extern void at386_cleanup P((int status));
extern void at386_draw P((int line, int small));
extern void at386_user_interf P((void));
extern void at386_error P((char *msg));
extern void at386_raster P((unsigned char *bitmap, int width, int height));
#define VIDLINES        (350)   /* for 640x350 display mode */
#endif

#if defined(linux) && ! defined(NO_VGA_LIB)
extern void vgalib_setup P((void));
extern void vgalib_cleanup P((int status));
extern void vgalib_draw P((int line, int small));
extern void vgalib_user_interf P((void));
extern void vgalib_error P((char *msg));
extern void vgalib_raster P((unsigned char *bitmap, int width, int height));
#define LINVGAVIDLINES        (480)   /* for 640x480 display mode */
#endif

#ifdef __WATCOMC__
extern void dos_setup P((void));
extern void dos_cleanup P((int status));
extern void dos_draw P((int line, int small));
extern void dos_user_interf P((void));
extern void dos_error P((char *msg));
extern void dos_raster P((unsigned char *bitmap, int width, int height));
/* video lines determined at run time */
#endif



/* list of supported $TERM types and which functions and parameters to use
 * to implement them. To support a new terminal type, write appropriate
 * functions, and add to the Config table.
 */
struct CONFIG Config [] = {

#if defined(AT386) && ! defined(__DOS__) && ! defined(__APPLE__)
	{
		"AT386",		/* terminal type name */
		at386_setup,		/* setup function */
		at386_cleanup,		/* cleanup function */
		at386_draw,		/* function for drawing a screen full of bits */
		at386_user_interf,	/* function to read user input and call do_cmd on it */
		at386_error,		/* error reporting function */
		at386_raster,		/* function to display a raster bitmap centered on screen */
		VIDLINES,		/* screen vertical lines */
		640,			/* max pixels wide */
		0.75			/* aspect ratio */
	},
#endif

#ifdef XWINDOW
	{
		"xterm",		/* terminal type name */
		xterm_setup,		/* setup function */
		xterm_cleanup,		/* cleanup function */
		xterm_draw,		/* function for drawing a screen full of bits */
		xterm_user_interf,	/* function to read user input and call do_cmd on it */
		xterm_error,		/* error reporting function */
		xterm_raster,		/* function to display a raster bitmap centered on screen */
		XVIDLINES,		/* screen vertical lines */
		0,			/* max pixels wide unlimited*/
		1.0			/* aspect ratio */
	},
#endif

#if defined(linux) && ! defined(NO_VGA_LIB)
	{
		"linux",		/* terminal type name */
		vgalib_setup,		/* setup function */
		vgalib_cleanup,		/* cleanup function */
		vgalib_draw,		/* function for drawing a screen full of bits */
		vgalib_user_interf,	/* function to read user input and call do_cmd on it */
		vgalib_error,		/* error reporting function */
		vgalib_raster,		/* function to display a raster bitmap centered on screen */
		LINVGAVIDLINES,		/* screen vertical lines */
		640,			/* max pixels wide */
		1.0			/* aspect ratio */
	},

#endif
#ifdef __WATCOMC__
	{
		"DOS",			/* terminal type name */
		dos_setup,		/* setup function */
		dos_cleanup,		/* cleanup function */
		dos_draw,		/* function for drawing a screen full of bits */
		dos_user_interf,	/* function to read user input and call do_cmd on it */
		dos_error,		/* error reporting function */
		dos_raster,		/* function to display a raster bitmap centered on screen */
		0,              	/* screen vertical lines--will get set at run time */
		640,			/* max pixels wide */
		0.0             	/* aspect ratio adjust--will get set at run time */
	}
#endif
};


/* initialize. Make sure TERM is supported, and set up for it. */

void
init()

{
	struct CONFIG *c_p;     /* terminal configuration info */
	char *termname;         /* $TERM */
	int numtypes;
#ifdef XWINDOW
	struct CONFIG *xwindow_conf_p = 0;	/* info for running under X */
#endif


#ifdef __WATCOMC__
	termname = "DOS";
#else
	if ((termname = getenv("TERM")) == NULL) {
		fprintf(stderr, "can't determine $TERM\n");
		generalcleanup(1);
	}
#endif

	/* find appropriate functions to use based on terminal type */
	numtypes = sizeof(Config) / sizeof(struct CONFIG);
	for (c_p = Config; numtypes > 0; c_p++, numtypes--) {
		if (strcmp(c_p->termname, termname) == 0) {
			Conf_info_p = c_p;
			break;
		}
#ifdef XWINDOW
		/* save the xwindow config. If we don't find a
		 * matching terminal type here, we're try some other
		 * tricks later to guess if we're running under X */
		if (strcmp(c_p->termname, "xterm") == 0) {
			xwindow_conf_p = c_p;
		}
#endif
	}

#ifdef XWINDOW
	if (Conf_info_p == (struct CONFIG *) 0) {
		/* There can be several variations on xterm, so
		 * if $TERM at least starts with xterm, consider that
		 * good enough to try. And to try even harder to
		 * recognize if we're probably running under X, and
		 * thus the 'xterm' type will probably work, check if
		 * $DISPLAY is set. Chances are, if it is, we're probably
		 * in X. If it turns out we're not, we'll fail eventually,
		 * but we will have at least tried pretty hard
		 * to find something that would work... */
		if (strncmp(termname, "xterm", 5) == 0 ||
				getenv("DISPLAY") != (char *) 0) {
			Conf_info_p = xwindow_conf_p;
		}
	}
#endif

	/* Make sure we managed to find a $TERM we can work with */
	if (Conf_info_p == (struct CONFIG *) 0 ) {
		fprintf(stderr, "$TERM type not supported\n");
		generalcleanup(1);
	}

#ifdef XWINDOW
	/* X has some extra options, so handle them */
	if (strcmp(Conf_info_p->termname, "xterm") == 0) {
		parse_X_options();
	}
#endif
}
