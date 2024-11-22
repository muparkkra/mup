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

/* This file contains functions to support displaying multipage bitmaps
 * (as from Ghostscript -sDEVICE=bit) using Linux svgalib.
 * This was derived from the AT386 version, so maybe some things
 * would have been done a bit differently if it were written from
 * scratch for Linux, but this seems to work fine and to be
 * plenty fast enough (at least on a Pentium or better ;-)
 *
 * Note that using mupdisp in non-X-window mode on Linux requires that it
 * can write to the console device. To allow this, make mupdisp setuid to root:
 *	chown root mupdisp
 *	chmod 4755 mupdisp
 */

#if defined(linux) && ! defined(NO_VGA_LIB)


#include "mupdisp.h"

#include <stdio.h>
#include <vga.h>
#include <termio.h>
#include <sys/kd.h>
#include <sys/ioctl.h>

#define BPL		(80)	/* bytes per line on screen */

int Orig_video_mode;
struct termio Orig_ttyinfo;	/* to put keyboard back from raw mode */
unsigned char Savefont[8192];	/* to put font back when we are done */
int Console;

static char *explanation =
"\n\
Note: The libvga used by this program\n\
requires write permissions to /dev/console.\n\
\n\
The best way to enable that is to do the following (as root):\n\
\tchown root mupdisp\n\
\tchmod 4755 mupdisp\n\
This makes mupdisp \"set user id\" to root.\n\
\n\
An alternate method would be to make /dev/console writeable by all:\n\
\tchmod 666 /dev/console\n\
but that method would pose a security risk, so it is not recommended.\n\n";
static void setup_keyboard P((void));
static void fix_keyboard P((void));


/* set up for svgalib. Put video and keyboard in proper mode */

void
vgalib_setup()

{
	register int n;			/* for setting signal catching */


	/* will need to put keyboard into raw mode, save current state */
	if (ioctl(0, TCGETA, &Orig_ttyinfo) < 0) {
		(void) fprintf(stderr, "failed to get tty info\n");
		generalcleanup(1);
	}

	/* some version of vgalib apparently do an atexit() call that causes
	 * the stty to be put in noecho mode. So arrange to undo that. */
	atexit(fix_keyboard);

	/* For some reason, vga_puttextmode doesn't seem to work on my
	 * system, but using the *IO_FONT ioctls does, so go with that.
	 * Save the current console font. */
	if ((Console = open("/dev/console", 0)) < 0) {
		(void) fprintf(stderr, "can't open /dev/console\n");
		(void) fprintf(stderr, explanation);
		generalcleanup(1);
	}
	if (ioctl(Console, GIO_FONT, Savefont) < 0) {
		(void) fprintf(stderr, "unable to save console font\n");
		generalcleanup(1);
	}

	vga_init();

	/* get current video mode, so we can put it back when we're done */
	Orig_video_mode = vga_getcurrentmode();

	/* make sure we always clean up, so user isn't left stuck in raw and/or
	 * graphics mode. */
	for (n = 1; n < NSIG; n++) {
		if ( n != SIGKILL && n != SIGCLD) {
			(void) signal(n, Conf_info_p->cleanup);
		}
	}
	(void) signal(SIGWINCH, SIG_IGN);

	/* put keyboard into raw mode */
	setup_keyboard();

	/* put screen into graphics mode */
	vga_setmode(G640x480x16);
}


/* draw stuff onto screen. Draw starting at specified line of page */

void
vgalib_draw(line, small)

int line;	/* draw starting from this raster line of page */
int small;	/* YES if should draw small view of full page */

{
	register int i;
	register int j;
	unsigned char buff[MAX_BYTES_PER_LINE]; /* a row of bits to display */
	int extra;		/* how many unused bits in rightmost byte */
	int mask;		/* to clear out unused bits */
	long offset;		/* into bitmap file */
	int fd;			/* file to read bitmap from */
	unsigned char vbuff[BPL * 8]; /* for one video scan line */
	int jx8;		/* j times 8 (to convert bits to bytes) */
	int vbytes;


	/* make sure we have a valid page to draw */
	if (Currpage_p == (struct Pginfo *) 0) {
		( *(Conf_info_p->error) ) ("page # out of range");
		return;
	}

	/* figure out where in the bitmap file this page is */
	offset = Currpage_p->seqnum * BYTES_PER_PAGE;
	fd = gen1file(small);
	(void) lseek(fd, offset + line * BYTES_PER_LINE, SEEK_SET);

	/* vgalib wants 1 byte per pixel, we have 1 bit per pixel,
	 * so multiply by 8 */
	vbytes = BYTES_PER_LINE << 3;

	/* read from file and put into form for vga library to use */
	for (i = 0; i < Conf_info_p->vlines; i++) {
		read(fd, buff, BYTES_PER_LINE);

		/* if the page width is not on a byte boundary, blank
		 * out the partial byte at the edge */
		for (mask = 1, extra = BYTES_PER_LINE & 0x7;
					extra > 0; mask <<= 1, extra--) {
			buff[BYTES_PER_LINE - 1] |= mask;
		}

		/* set line to all white except for area beyond right edge,
		 * which is set to all black */
		memset(vbuff, 0xf, vbytes);
		memset(vbuff + vbytes, 0, (BPL - BYTES_PER_LINE) << 3);

		/* transfer bitmap row to vbuff, 1 bit of bitmap to 1 byte
		 * of vbuff. */
		for (j = 0; j < BYTES_PER_LINE; j++) {
			/* get j times 8 bits per byte */
			jx8 = j << 3;

			/* for each black bit, set appropriate
			 * vbuff byte to 0 */
			for (mask = 0; mask < 8; mask++) {
				if (buff[j] & (0x80 >> mask)) {
					vbuff[jx8 + mask] = 0;
				}
			}
		}

		/* display this line */
		vga_drawscanline(i, vbuff);
	}
}


/* cleanup function.
 * Put screen back into previous mode.
 */

void
vgalib_cleanup(status)

int status;

{
	/* put video back to normal */
	vga_setmode(Orig_video_mode);
	(void) ioctl(Console, PIO_FONT, Savefont);
	close(Console);

	/* put keyboard back to normal */
	(void) ioctl(0, TCSETA, &Orig_ttyinfo);
	
	/* call the non-terminal-type specific cleanup */
	generalcleanup(status);
}

/* some versions of vgalib seem to put things into noecho mode. So undo that. */

static void
fix_keyboard()
{
	(void) ioctl(0, TCSETA, &Orig_ttyinfo);
}


/* read from keyboard and call do_cmd for each key read.
 * Commands are described in
 * the comment at the beginning of do_cmd() */

void
vgalib_user_interf()

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
			if (c == '5') {
				if ((c = getchar()) == '~') {
					c = 'p'; /* Page Up key ==> previous */
				}
			}
			else if (c == '6') {
				if ((c = getchar()) == '~') {
					c = 'n'; /* Page Down key ==> next */
				}
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
vgalib_error(msg)

char *msg;

{
	(void) ioctl(Console, KDMKTONE, (150L << 16) | 3600L);
}


/* overlay a raster centered on the window */

void
vgalib_raster(bitmap, width, height)

unsigned char *bitmap;	/* what to display */
int width, height;	/* of bitmap, width is in bytes */

{
	int i, j;
	int x, y;	/* upper left corner of where to put bitmap,
			 * x in bytes */
	unsigned char vbuff[BPL * 8];
	int mask;
	int byte;
	int jx8;	/* j times 8 */
	int width8;	/* width times 8 */
	int ixwidth;	/* i times width */


	/* figure out how to center on screen */
	x = ((BYTES_PER_LINE - width) / 2) * 8;
	y = (Conf_info_p->vlines - height) / 2;

	/* width translating bits to bytes */
	width8 = width << 3;

	/* copy bitmap to screen */
	for (i = 0; i < height; i++) {
		memset(vbuff, 0, width8);
		ixwidth = i * width;

		for (j = 0; j < width; j++) {
			byte = bitmap [ ixwidth + j ];
			jx8 = j << 3;
			for (mask = 0; mask < 8; mask++) {
				if (byte & (0x80 >> mask)) {
					vbuff[jx8 + mask] = 0xf;
				}
			}
		}
		vga_drawscansegment(vbuff, x, y + i, width8);
	}
}


/* put keyboard in raw mode */
/* ported without change from the AT386 version. */

static void
setup_keyboard()

{
	struct termio ttyinfo;


	if (isatty(0) != 1) {
		(void) fprintf(stderr, "stdin is not a tty\n");
		generalcleanup(1);
	}

	if (ioctl(0, TCGETA, &ttyinfo) < 0) {
		(void) fprintf(stderr, "failed to get tty info\n");
		generalcleanup(1);
	}

	/* turn off echo and canonical */
	ttyinfo.c_lflag &= ~(ICANON | ECHO);
	ttyinfo.c_cc[VMIN] = 1;
	ttyinfo.c_cc[VTIME] = 3;
	if (ioctl(0, TCSETA, &ttyinfo) < 0) {
		(void) fprintf(stderr,
			"failed to set keyboard modes, errno %d\n", errno);
		generalcleanup(1);
	}
}


#else

/* some compilers complain about files that are effectively empty,
 * so put in something even when entire file is effectively ifdef-ed out */
/* Originally used:
 *	static short dummy;
 * but then gcc stated complaining that was unused.
 * So try this to try to keep both happy...
 */
void dummy_linvga_func_to_avoid_empty_file_warning()
{ 
}

#endif
