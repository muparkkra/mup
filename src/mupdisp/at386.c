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

/* This file contains functions to support displaying multipage bitmap
 * (as from Ghostscript -sDEVICE=bit) to AT386 console.
 */

#include "mupdisp.h"

#if defined(AT386) && ! defined(linux) && ! defined(__DOS__) && ! defined(__APPLE__)

#include <sys/kd.h>
#include <termio.h>

#define BPL		(80)	/* bytes per line on screen, AT386 mode */

unsigned char *Video_memory;
int Orig_video_mode;		/* to put video back the way it was */
struct termio Orig_ttyinfo;	/* to put keyboard back from raw mode */

static void setup_keyboard P((void));


/* set up for TERM=AT386. Put video and keyboard in proper mode */

void
at386_setup()

{
	register int n;			/* for setting signal catching */


	/* get current video mode, so we can put it back when we're done */
	if ((Orig_video_mode = ioctl(1, CONS_GET, 0)) < 0) {
		fprintf(stderr, "failed to determine current video mode\n");
		generalcleanup(1);
	}

	/* need to put keyboard into raw mode, save current state */
	if (ioctl(0, TCGETA, &Orig_ttyinfo) < 0) {
		fprintf(stderr, "failed to get tty info\n");
		generalcleanup(1);
	}

	/* make sure we always clean up, so user isn't left stuck in raw and/or
	 * graphics mode. */
	for (n = 1; n < NSIG; n++) {
		if ( n != SIGKILL && n != SIGCLD) {
			sigset(n, Conf_info_p->cleanup);
		}
	}
	sigset(SIGWINCH, SIG_IGN);

	/* put keyboard into raw mode */
	setup_keyboard();

	/* put screen into graphics mode */
	if (ioctl(1, SW_CG640x350, 0) < 0) {
		generalcleanup(1);
	}

	/* get access to video memory */
	Video_memory = (unsigned char *) ioctl(1, MAPCONS, 0);
}


/* draw stuff onto screen. Draw starting at specified line of page */

void
at386_draw(line, small)

int line;	/* draw starting from this raster line of page */
int small;	/* YES if should draw small view of full page */

{
	register int i;
	register int j;
	unsigned char buff[MAX_BYTES_PER_LINE]; /* a row of bits to display */
	int extra;		/* how many unused bits in rightmost byte */
	int mask;		/* to clear out unused bits */
	unsigned char *v;	/* pointer into video memory */
	long offset;		/* into bitmap file */
	int fd;			/* file to read bitmap from */


	/* make sure we have a valid page to draw */
	if (Currpage_p == (struct Pginfo *) 0) {
		( *(Conf_info_p->error) ) ("page # out of range");
		return;
	}

	/* figure out where in the bitmap file this page is */
	offset = Currpage_p->seqnum * BYTES_PER_PAGE;
	fd = gen1file(small);
	lseek(fd, offset + line * BYTES_PER_LINE, SEEK_SET);

	/* read from file and put into video memory, inverting to
	 * black on white */
	for (i = 0; i < Conf_info_p->vlines; i++) {
		read(fd, buff, BYTES_PER_LINE);

		/* if the page width is not on a byte boundary, blank
		 * out the partial byte at the edge */
		mask = 1;
		for (mask = 1, extra = BYTES_PER_LINE & 0x7;
					extra > 0; mask <<= 1, extra--) {
			buff[BYTES_PER_LINE - 1] |= mask;
		}

		/* point to row in video memory and transfer bitmap row */
		v = Video_memory + i * BPL;
		for (j = 0; j < BYTES_PER_LINE; j++) {
			v[j] = buff[j] ^ 0xff;
		}
	}
}


/* AT386 cleanup function.
 * Put screen back into previous mode. Some day maybe we should save the data
 * on the original screen and put it back when we're done... */

void
at386_cleanup(status)

int status;

{
	/* put video back to normal */
	ioctl(1, MODESWITCH | Orig_video_mode, 0);

	/* put keyboard back to normal */
	ioctl(0, TCSETA, &Orig_ttyinfo);
	
	/* call the non-terminal-type specific cleanup */
	generalcleanup(status);
}


/* read from keyboard and call do_cmd for each key read.
 * Commands are described in
 * the comment at the beginning of do_cmd() */

void
at386_user_interf()

{
	int c;			/* char read from keyboard */
	int special = 0;	/* 1 = got an escape, 2 = got escape followed
				 * by [, 0 = not doing any special processing.
				 * This is to handle special function keys. */

	while ( (c = getchar() ) != EOF) {
		if (c == 0x1b) {
			/* got ESC, could be a special function key */
			special = 1;
			continue;
		}
		else if (special == 1 && c == '[') {
			/* got ESC-[ */
			special = 2;
			continue;
		}
		else if (special == 2) {
			/* map special functions to their equivalent commands */
			if (c == 'V') {
				c = 'p';	/* Page Up key ==> previous */
			}
			else if (c == 'U') {
				c = 'n';	/* Page Down key ==> next */
			}
			else if (c == 'A') {
				c = 'b';	/* Up key ==> backwards */
			}
			else if (c == 'B') {
				c = 'f';	/* Down key ==> forwards */
			}
		}
		special = 0;
		do_cmd(c);
	}
}


/* Error handler.
 * For now just beep. Maybe eventually pop up an error message */

void
at386_error(msg)

char *msg;

{
	ioctl(1, KDMKTONE, (150L << 16) | 3600L);
}


/* overlay a raster centered on the window */

void
at386_raster(bitmap, width, height)

unsigned char *bitmap;	/* what to display */
int width, height;	/* of bitmap, width is in bytes */

{
	int i, j;
	int x, y;	/* upper left corner of where to put bitmap, x in bytes */


	/* figure out how to center on screen */
	x = (BYTES_PER_LINE - width) / 2;
	y = (Conf_info_p->vlines - height) / 2;

	/* copy bitmap to screen */
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			Video_memory[(y + i) * BPL + (x + j)] =
				bitmap[ (i * width) + j];
		}
	}
}


/* put keyboard in raw mode */

static void
setup_keyboard()

{
	struct termio ttyinfo;


	if (isatty(0) != 1) {
		fprintf(stderr, "stdin is not a tty\n");
		generalcleanup(1);
	}

	if (ioctl(0, TCGETA, &ttyinfo) < 0) {
		fprintf(stderr, "failed to get tty info\n");
		generalcleanup(1);
	}

	/* turn off echo and canonical */
	ttyinfo.c_lflag &= ~(ICANON | ECHO);
	ttyinfo.c_cc[VMIN] = 1;
	ttyinfo.c_cc[VTIME] = 3;
	if (ioctl(0, TCSETA, &ttyinfo) < 0) {
		fprintf(stderr, "failed to set keyboard modes, errno %d\n", errno);
		generalcleanup(1);
	}
}


#else

/* some compilers complain about files that are effectively empty,
 * so put in something even when entire file is effectively ifdef-ed out */
/* Originally used:
 * 	 static short dummy;
 * but then gcc stated complaining that was unused.
 * So try this to try to keep both happy...
 */
void dummy_at386_func_to_avoid_empty_file_warning()
{
}

#endif
