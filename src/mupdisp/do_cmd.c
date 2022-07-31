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

/* This file contains command processing functions for the mupdisp
 * program that displays Mup output using Ghostscript.
 * Given an input input character, it does the appropriate command.
 */


/* The user interface commands are:
 *	+ or <space> or ^e or ^f	scroll forwards 1/8"
 *	- or <backsp> or ^y or ^b	scroll backwards 1/8"
 *	f or <enter> or ^d		scroll forwards 1"
 *	b or ^u				scroll backwards 1"
 *	h or ?		help
 *	m		toggle between full page and scrolled mode
 *	n		go to top of next page
 *	p		go to top of previous page
 *	q		quit
 *	r		repaint current page
 *	Num\n		go to page Num
 * Upper case versions work too.
 *	ZZ is synonym for quit
 */

#include "mupdisp.h"

/* Bitmap for the help screen. Have to make sure this is smaller
 * than the minimum screen/window size */
#include "help.bm"


/* command processing function for Mup/Ghostscript display program.
 * Given a input input character, does the appropriate command */

void
do_cmd(c)

int c;		/* character read from user */

{
	static int line = 0;	/* current line on page that
				 * is at top of screen */
	static int got1z = 0;	/* if got one Z for vi-like ZZ exit */
	static int pgnum = -1;	/* page number */
	static int count = 0;	/* how many times called */
	static int lasterr = 0;	/* value of count for last error */
	static int goofs = 0;	/* how many consecutive user errors */


	/* count of times called, to implement auto-help if user goofs too
	 * many times in a row */
	count++;

	c = tolower(c);

	switch(c) {

	case '0': case '1':
	case '2': case '3':
	case '4': case '5':
	case '6': case '7':
	case '8': case '9':
		/* gathering page number for specific page */
		if (pgnum < 0) {
			/* first digit, so initialize page number */
			pgnum = 0;
		}
		pgnum = (pgnum * 10) + c - '0';
		break;

	case '+':
	case ' ':
	case '\5':	/* control-E for vi users */
	case '\6':	/* control-F for emacs users */
		/* scroll forward a little bit */
		if (Fullpgmode) {
			( *(Conf_info_p->error) ) ("command invalid in full page mode");
		}
		else {
			line = scroll(line, 9);
		}
		break;

	case '-':
	case '\b':
	case '\31':	/* control-Y for vi users */
	case '\2':	/* control-B for emacs users */
		/* backward a little bit */
		if (Fullpgmode) {
			( *(Conf_info_p->error) ) ("command invalid in full page mode");
		}
		else {
			line = scroll(line, -9);
		}
		break;

	case '\n':
	case '\r':
		if (pgnum >= 0) {
			/* ending page number command */
			if (getpginfo(pgnum) == YES) {
				( *(Conf_info_p->draw) )  (0, Fullpgmode);
				line = 0;
			}
			else {
				( *(Conf_info_p->error) ) ("invalid page number");
			}
			pgnum = -1;
			break;
		}
		/* else fall through to moving forward on page*/
		/*FALLTHRU*/

	case 'f':
	case '\4':	/* control-D */
	case '\16':	/* control-N */
		/* forward an inch */
		if (Fullpgmode) {
			( *(Conf_info_p->error) ) ("command invalid in full page mode");
		}
		else {
			line = scroll(line, 72);
		}
		break;

	case 'b':
	case '\25':	/* control-U */
	case '\20':	/* control-P */
		/* back an inch */
		if (Fullpgmode) {
			( *(Conf_info_p->error) ) ("command invalid in full page mode");
		}
		else {
			line = scroll(line, -72);
		}
		break;

	case 'h':
	case '?':
		/* display help screen */
		( *(Conf_info_p->bitmap) ) (Help_bitmap, Help_width, Help_height);
		break;

	case 'n':
		/* go to next page */
		if (Currpage_p->next != (struct Pginfo *) 0) {
			Currpage_p = Currpage_p->next;
			line = 0;
			( *(Conf_info_p->draw) ) (line, Fullpgmode);
		}
		else {
			( *(Conf_info_p->error) ) ("already at last page");
		}
		break;

	case 'p':
		/* go to previous page */
		if (Currpage_p->prev != (struct Pginfo *) 0) {
			Currpage_p = Currpage_p->prev;
			line = 0;
			( *(Conf_info_p->draw) ) (line, Fullpgmode);
		}
		else {
			( *(Conf_info_p->error) ) ("already at first page");
		}
		break;

	case 'q':
		/* quit */
		( *(Conf_info_p->cleanup) )  (0);
		break;

	case 'r':
		/* repaint current screen */
		/* if redraw is because window was resized, make sure we don't
		 * run off the bottom */
		if (line + Conf_info_p->vlines > LINES_PER_PAGE * Conf_info_p->adjust) {
			line = LINES_PER_PAGE * Conf_info_p->adjust - Conf_info_p->vlines;
		}
		( *(Conf_info_p->draw) ) (line, Fullpgmode);
		break;
		
	case 'z':
		/* 2 Z's is symonym for quit, to match with common
		 * way to exit vi */
		if (got1z) {
			( *(Conf_info_p->cleanup) )  (0);
		}
		break;

	case 'm':
		/* switch between full page and scrolled modes */
		Fullpgmode = (Fullpgmode == YES ? NO : YES);
		line = 0;
		( *(Conf_info_p->draw) ) (line, Fullpgmode);
		break;

	default:
		( *(Conf_info_p->error) ) ("unknown command");
		/* auto-help feature:
		 * if user puts in something illegal too many times in a row,
		 * figure they're confused and desperate, so give them the
		 * help screen */
		if (lasterr == count - 1) {
			goofs++;
			if (goofs > 9) {
				( *(Conf_info_p->bitmap) ) (Help_bitmap,
						Help_width, Help_height);
				goofs = 0;
			}
		}
		else {
			goofs = 1;
		}
		lasterr = count;
		break;
	}

	/* set flag if got Z, so can check for Z consecutive Z's for
	 * vi-style exit */
	got1z = (c == 'Z' || c == 'z');
}
