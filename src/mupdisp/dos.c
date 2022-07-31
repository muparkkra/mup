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

/* This file contains functions for displaying Mup output on screen
 * via Ghostscript, using the Watcom C for Windows.
 */

#ifdef __WATCOMC__
#include "mupdisp.h"
#include <graph.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>

/* hard code X and Y sizes for the moment */
#define XSIZE		640
#define YSIZE		480

/* image to be calloc'ed has 6 header bytes, then each row consecutively */
#define BITS2BYTES(bits)	(((bits) + 7) / 8)	/* round upwards */
#define SIZEIMAGE	(6 + ((long)BITS2BYTES(XSIZE)*4) * (long)YSIZE)
static char *Image;

static void zapblock P((char *area, long size));


/* initialize graphics driver, etc */

void
dos_setup()

{
	struct videoconfig vidinfo;


	/* initalize best graphics mode */
	if (_setvideomode(_VRES16COLOR) == 0) {
		fprintf(stderr, "can't set graphics mode\n");
		generalcleanup(1);
	}

	_getvideoconfig(&vidinfo);

	if (vidinfo.numxpixels != XSIZE || vidinfo.numypixels != YSIZE) {
		_setvideomode(_DEFAULTMODE);
		fprintf(stderr, "video mode %dx%d not supported\n",
				vidinfo.numxpixels, vidinfo.numypixels);
		generalcleanup(0);
	}

	/*
	 * Allocate the image buffer.  Note that both parameters must be less
	 * than 65536, but the total size (product) can be greater.  The +1
	 * is to allow room for the header bytes (and then some).
	 */
	Image = (char *)calloc(BITS2BYTES(YSIZE)*4 + 1, XSIZE);
	if (Image == NULL) {
		_setvideomode(_DEFAULTMODE);
		fprintf(stderr, "can't allocate image buffer\n");
		generalcleanup(0);
	}

	/* set aspect ratio adjust */
	Conf_info_p->adjust = 1.375 * (double) vidinfo.numypixels
					/ (double) vidinfo.numxpixels;
	Conf_info_p->vlines = vidinfo.numypixels;

	/* set screen to all white */
	_setbkcolor(_BRIGHTWHITE);
	_clearscreen(_GCLEARSCREEN);
}


/* before exiting, clean up graphics, then call the general cleanup routine */

void
dos_cleanup(int status)
{
	 _setvideomode(_DEFAULTMODE);
	 generalcleanup(status);
}


/* draw a screen worth of the bitmap, starting at specified raster line */

void
dos_draw(line, small)

int line;       /* start at this raster line */
int small;      /* YES or NO for small or large page image */

{
	int r;				/* row index */
	long offset;                    /* into bitmap file */
	int fd;				/* file descriptor */
	int himage_bytes;		/* horizontal image bytes */
	char *row_ptr;			/* point at a row of the image */
	int n;				/* loop variable */


	/* make sure we have a valid page to draw */
	if (Currpage_p == (struct Pginfo *) 0) {
		( *(Conf_info_p->error) ) ("page # out of range");
		return;
	}

	/* figure out where in the bitmap file this page is */
	offset = Currpage_p->seqnum * BYTES_PER_PAGE;
	fd = gen1file(small);
	lseek(fd, offset + (long)line * BYTES_PER_LINE, SEEK_SET);

	/* zero out the image buffer */
	zapblock(Image, SIZEIMAGE);

	/* number of bytes representing one horizontal row in full page image */
	himage_bytes = BITS2BYTES(XSIZE) * 4;

	/* set the header bytes in the image */
	Image[0] = XSIZE % 256;
	Image[1] = XSIZE / 256;
	Image[2] = YSIZE % 256;
	Image[3] = YSIZE / 256;
	Image[4] = 4;
	Image[5] = 0;

	/* set screen to all white */
	_setbkcolor(_BRIGHTWHITE);
	_clearscreen(_GCLEARSCREEN);

	/* for each row */
	for (r = 0; r < Conf_info_p->vlines; r++) {
	    	/* read it directly into the image */
		row_ptr = &Image[ 6 + r * himage_bytes ];
		if (read(fd, row_ptr, BYTES_PER_LINE) != BYTES_PER_LINE) {
			break;
		}

		if (small) {
			/* black out the unused strip on the right */
			for (n = BITS_PER_LINE; n < XSIZE; n++)
				row_ptr[n/8] |= 1 << (7 - n%8);
		}
	}
	/* put at upper left corner, (0, 0) */
	_putimage(0, 0, Image, _GPSET);
}


/* for now we just beep on errors */
/*ARGSUSED*/
void
dos_error(msg)

char *msg;

{
	putc('\7', stderr);
}


/* for user interface, call command processor for each character read */

void
dos_user_interf()

{
	int c;
	int special = 0;	/* 1 = got a null, which is first character
				 * of special key sequence */
	for ( ; ; ) {
		c = getch();
		if (c == '\0') {
			special = 1;
			continue;
		}
		if (special == 1) {
			switch (c) {
			case 0x49:
				/* PgUp key */
				c = 'p';
				break;
			case 0x51:
				/* PgDown key */
				c = 'n';
				break;
			case 0x48:
				/* Up arrow key */
				c = 'b';
				break;
			case 0x50:
				/* Down arrow key */
				c = 'f';
				break;
			default:
				special = 0;
				continue;
			}
		}
		do_cmd(c);
		special = 0;
	}
}


/* display a raster centered on window */

void
dos_raster(bitmap, width, height)

unsigned char *bitmap;  /* what to display */
int width, height;      /* of bitmap, width is in bytes */

{
	int r, c;       /* row and column indices */
	int b;          /* index through bits */
	int x, y;       /* upper left corner of where to put bitmap,
			 * x in bytes */
	int himage_bytes;	/* bytes needed for one row in image */
	int n;		/* loop variable */
	char *row_ptr;	/* point at a row of the image */


	himage_bytes = 4 * width;

	/* figure out how to center on screen */
	x = (BYTES_PER_LINE - width) / 2 * 8;
	y = (Conf_info_p->vlines - height) / 2;

	/* zero out the image buffer */
	zapblock(Image, SIZEIMAGE);

	/* set the header bytes in the image */
	Image[0] = (width * 8) % 256;
	Image[1] = (width * 8) / 256;
	Image[2] = height % 256;
	Image[3] = height / 256;
	Image[4] = 4;
	Image[5] = 0;

	/* for each row */
	for (r = 0; r < height; r++) {
		row_ptr = &Image[ 6 + r * himage_bytes ];
		for (c = 0; c < width; c++) {
			for (b = 0; b < 8; b++) {
				if (bitmap[r*width+c] & (1 << (7-b))) {
					/* white (15); set 4 copies of bit */
					for (n = 0; n < 4; n++)
						row_ptr[n*width + c] |=
							(1 << (7-b));
				} else {
					/* black (8); set only the first */
					row_ptr[c] |= (1 << (7-b));
				}
			}
		}
	}

	_putimage(x, y, Image, _GPSET);
}

/* zero out a block of memory that may be bigger than 32K */

#define	BLOCKSIZE	0x3fff

static void
zapblock(area, size)

char *area;
long size;

{
	long k;

	/*
	 * memset's third parm is "unsigned int", so we can't do the whole
	 * area at once.
	 */
	for (k = 0; k < size; k += BLOCKSIZE) {
		if (size - k >= BLOCKSIZE)
			(void)memset(&area[k], 0, BLOCKSIZE);
		else
			(void)memset(&area[k], 0, size - k);
	}
}

#else

/* some compilers don't like empty files, so if __WATCOMC__ isn't defined,
 * put something here to keep those compilers happy */
/* Originally used:
 *       static short dummy;
 * but then gcc stated complaining that was unused.
 * So try this to try to keep both happy...
 */
void dummy_dos_func_to_avoid_empty_file_warning()
{ 
}

#endif
