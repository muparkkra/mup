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

/* This file contains functions for displaying Mup/Ghostscript output
 * under X windows.
 */

#include "mupdisp.h"

#ifdef XWINDOW

#include <X11/Xlib.h>
#include <X11/Xresource.h>

/* size for XLookupString buffer */
#define IBUFSIZ		8

/* X window icon to use when window is icon-ized.
 * Shows musical notes. This was generated using the bitmap tool */
#define Disp_icon_width 32
#define Disp_icon_height 32
static unsigned char Disp_icon_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0f, 0x00,
   0x00, 0xc0, 0x0b, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0xf8, 0x08, 0x00,
   0x00, 0x1e, 0x08, 0x00, 0x00, 0x0e, 0x08, 0x00, 0x00, 0x02, 0x08, 0x00,
   0xff, 0xff, 0xff, 0xff, 0x00, 0x02, 0x08, 0x14, 0x00, 0x02, 0x08, 0x22,
   0x00, 0x02, 0x08, 0x23, 0x00, 0x02, 0x08, 0x15, 0xff, 0xff, 0xff, 0xff,
   0x00, 0x02, 0x08, 0x01, 0x00, 0x02, 0x08, 0x01, 0x00, 0x82, 0x0f, 0x01,
   0x00, 0xc2, 0x0f, 0x01, 0xff, 0xff, 0xff, 0xff, 0x00, 0x82, 0x07, 0x01,
   0x00, 0x02, 0x00, 0x01, 0xe0, 0x03, 0x02, 0x01, 0xf0, 0x03, 0x01, 0x01,
   0xff, 0xff, 0xff, 0xff, 0xe0, 0x81, 0x00, 0x01, 0x00, 0x40, 0x00, 0x01,
   0x80, 0x31, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define MINHEIGHT	(240)	/* allow this short a window in X window mode */
#define MAXHEIGHT	LINES_PER_PAGE	/* tallest window we allow */

#define SMALL 1
#define OK 0

/* bitmap of whether control key is pressed */
#define LEFTC 	0x1
#define RIGHTC	0x2

extern unsigned char Waitmsg_bitmap[];	/* message to tell user to wait */
extern int Waitmsg_width, Waitmsg_height;

/* define X window screen and display things */
static Display *Display_p;
static int Xscreen;
static XImage *Image_p;
static XFontStruct *Font_info_p;
static GC gc;			/* X graphics context */
static Window Win;
static unsigned int Width, Height;
static XSizeHints Size_hints;	/* tell window manager what size we want */
static unsigned long Foreground; /* color */
static unsigned long Background; /* color */
static char Mupdisp[] = "mupdisp";

/* X resource manager things */
static XrmOptionDescRec Option_table[] = {
	{ "-geometry",	".geometry",	XrmoptionSepArg, (caddr_t) 0 },
	{ "-background","*background",	XrmoptionSepArg, (caddr_t) 0 },
	{ "-bg",	"*background",	XrmoptionSepArg, (caddr_t) 0 },
	{ "-foreground","*foreground",	XrmoptionSepArg, (caddr_t) 0 },
	{ "-fg",	"*foreground",	XrmoptionSepArg, (caddr_t) 0 },
};
static int Opt_table_size = sizeof(Option_table) / sizeof (XrmOptionDescRec);
static char Xoptions_usage[] =
"MUPADDOP=\
 Also the following X options:\n\
   -bg color            set background color\n\
   -fg color            set foreground color\n\
   -geometry XxY+N+M    set window size and location\n";
XrmDatabase Resource_db;


/* local functions */
static char * get_cmd_resource P((char *resource_name));
static unsigned long get_color P((char *resource_name,
		unsigned long default_value));
static int color_ok P((char *resource_name, char *value, XColor *color_p));
static void create_image P((int wid, int height));
static void get_GC P((Window win));
static void load_font P((void));
static void TooSmall P((Window win));
static int err_handler P((Display *disp_p));


/* parse any X-specific aguments */

void
parse_X_options()
{
	XrmInitialize();
	XrmParseCommand( &Resource_db, Option_table, Opt_table_size,
			Mupdisp, &Argc, Argv);
	/* tell Mup about these additional options */
	putenv(Xoptions_usage);
}


/* setup for X-window operation. Basically copy things from the X manual,
 * and customize as appropriate. */


void
xterm_setup()

{
	int x = 0, y = 0;
	unsigned int border_width = 4;
	unsigned int display_height;
	char *window_name = Mupdisp;
	char *icon_name = Mupdisp;
	Pixmap icon_pixmap;
	char *display_name = NULL;
	XEvent event;
	char * value;
	unsigned int dummy_width;	/* we don't allow Width to change */
	XWindowAttributes attributes;
	int got_geometry = 0;


	/* If mupdisp is setuid to root so that libsvga can work on
	 * console devices, Ghostscript can fail under X.
	 * So relinquish our superuser-ism.
	 */
	if (getuid() != geteuid()) {
		if (seteuid(getuid()) != 0) {
			fprintf(stderr, "seteuid failed\n");
			generalcleanup(1);
		}
	}

	/* set up display */
	if ( (Display_p = XOpenDisplay(display_name)) == NULL) {
		fprintf(stderr, "%s: can't connect to X server %s\n", Argv[0],
				XDisplayName(display_name));
		generalcleanup(1);
	}

	Xscreen = DefaultScreen(Display_p);

	display_height = DisplayHeight(Display_p, Xscreen);

	/* this is our ideal size. Window manager may have other ideas */
	Width = BITS_PER_LINE;
	/* Use a height a little smaller than the screen height (to allow
	 * for window borders), or at a minimum, what's in Conf_info_p */
	if (display_height - 50 > Conf_info_p->vlines) {
		Height = display_height - 50;
	}
	else {
		Height = Conf_info_p->vlines;
	}

	/* If user specified colors, get those, otherwise use black on white */
	Background = get_color("background", WhitePixel(Display_p, Xscreen));
	Foreground = get_color("foreground", BlackPixel(Display_p, Xscreen));
	
	/* Now see if user specified geometry, either from command line,
	 * or failing that, from default database. */
	if ((value = get_cmd_resource("geometry")) != (char *) 0) {
		XParseGeometry(value, &x, &y, &dummy_width, &Height);
		got_geometry = 1;
	}
	else if ((value = XGetDefault(Display_p, Mupdisp, "geometry")) != 0) {
		XParseGeometry(value, &x, &y, &dummy_width, &Height);
		got_geometry = 1;
	}
	if (Height < MINHEIGHT) {
		Height = MINHEIGHT;
	}
	if (Height > MAXHEIGHT) {
		Height = MAXHEIGHT;
	}


	/* create window and icon */
	Win = XCreateSimpleWindow(Display_p, RootWindow(Display_p, Xscreen),
			x, y, Width, Height, border_width,
			Foreground, Background);

	icon_pixmap = XCreateBitmapFromData(Display_p, Win,
			(char *) Disp_icon_bits, Disp_icon_width,
			Disp_icon_height);

	/* we want the width to be exactly the width of a page. The height
	 * can vary because we scroll in that direction */
	if (got_geometry) {
		Size_hints.flags = PSize | PMinSize | PMaxSize | PPosition;
		Size_hints.x = x;
		Size_hints.y = y;
	}
	else {
		Size_hints.flags = PSize | PMinSize | PMaxSize;
	}
	Size_hints.width = Width;
	Size_hints.height = Height;
	Size_hints.min_width = Width;
	Size_hints.max_width = Width;
	Size_hints.min_height = MINHEIGHT;
	Size_hints.max_height = (display_height < MAXHEIGHT
					? display_height : MAXHEIGHT);

	XSetStandardProperties(Display_p, Win, window_name, icon_name,
			icon_pixmap, Argv, Argc, &Size_hints);

	XSelectInput(Display_p, Win, ExposureMask | KeyPressMask |
			KeyReleaseMask | ButtonPressMask | StructureNotifyMask);

	load_font();
	get_GC(Win);
	XMapWindow(Display_p, Win);

	/* determine what height we actually got */
	if (XGetWindowAttributes(Display_p, Win, &attributes) != 0) {
		Conf_info_p->vlines = Height = attributes.height;
	}
		
	create_image(Width, Height);

	/* Some window managers apparently just destroy the X connection,
	 * rather than sending a signal when the user closes the window,
	 * so arrange to clean up in that case. */
	XSetIOErrorHandler(err_handler);

	/* it seems we need to wait for an exposure event,(or maybe it's
	 * really MapNotify) before proceeding, or else the "wait" message
	 * doesn't get displayed, so wait for it */
	XWindowEvent(Display_p, Win, ExposureMask, &event);
}


/* create XImage for the display. If one already existed, free it first (which
 * would happen if window was re-sized */

static void
create_image(wid, height)

int wid;
int height;

{
	char *databuff;


	/* if already have one, free that and redo if different size */
	if (Image_p != (XImage *) 0) {
		if (Image_p->width == wid && Image_p->height == height) {
			/* already have one of correct size */
			return;
		}
		else {
			XDestroyImage(Image_p);
		}
	}

	/* create buffer for display image */
	if ((databuff = (char *) malloc(BYTES_PER_LINE * height)) == (char *) 0) {
		Exit_errmsg = "Could not allocate memory\n";
		( *(Conf_info_p->cleanup) )  (1);
	}

	/* On some systems (e.g., Easy Peasy with maximus), the window
	 * manager may ignore the width we requested. We really need this
	 * image to be exactly the width we expect, or the XCreateImage could
	 * fail and lead to core dump, not to mention not make the correct
	 * image that we want. So we have to not use the wid passed in */
	Image_p = XCreateImage(Display_p, DefaultVisual(Display_p, Xscreen),
			1, XYBitmap, 0, databuff, BITS_PER_LINE, height,
			8, BYTES_PER_LINE);
	Image_p->bitmap_unit = 8;
	Image_p->bitmap_bit_order = MSBFirst;
}


/* Look up the color resource named. If found, return its value,
 * otherwise return the default value. */

static unsigned long
get_color(resource_name, default_value)

char *resource_name;
unsigned long default_value;

{
	char *value;
	XColor color;

	/* First try looking up in command line resource database. */
	if ((value = get_cmd_resource(resource_name)) != (char *) 0) {
		if (color_ok(resource_name, value, &color)) {
			return(color.pixel);
		}
	}

	/* failing that, try looking in the default database */
	if ((value = XGetDefault(Display_p, Mupdisp, resource_name)) != 0) {
		if (color_ok(resource_name, value, &color)) {
			return(color.pixel);
		}
	}

	return(default_value);
}


/* look up a value from the command line database. Return it if found, else 0 */
static char *
get_cmd_resource(resource_name)

char *resource_name;

{
	XrmValue rm_value;
	char *str_type[20];
	char res_name[100];
	char class_name[100];
	int offset;


	/* create the resource and class names. Class name has initial caps */
	sprintf(res_name, "%s.%s", Mupdisp, resource_name);
	sprintf(class_name, "%s.%s", Mupdisp, resource_name);
	class_name[0] = toupper(class_name[0]);
	offset = strlen(Mupdisp) + 1;
	class_name[offset] = toupper(class_name[offset]);

	/* look it up in command line resource database. */
	if (XrmGetResource(Resource_db, res_name,
			class_name, str_type, &rm_value) == True) {
		return ((char *) rm_value.addr);
	}
	return((char *) 0);
}

/* Parse a color name and allocate it. If all goes well, fill in the
 * XColor and return 1. If something goes wrong, return 0. */

static int
color_ok(resource_name, value, color_p)

char *resource_name;
char *value;
XColor *color_p;

{
	if (XParseColor(Display_p,
			DefaultColormapOfScreen(
			DefaultScreenOfDisplay(Display_p)),
			value, color_p) != 0) {
		if (XAllocColor(Display_p,
				DefaultColormapOfScreen(
				DefaultScreenOfDisplay(Display_p)),
				color_p) != 0) {
			return (1);
		}
	}
	else {
		fprintf(stderr, "invalid %s color: %s\n",
				resource_name, value);
	}
	return(0);
}


/* get input in X windows mode. Handle all the events, including mouse,
 * resizing and exposure */
 
void
xterm_user_interf()

{
	XEvent report;		/* what X event happened */
	char inpbuff[IBUFSIZ];
	KeySym keysym;
	XComposeStatus compose;
	int window_size = OK;
	static int control = 0;	/* non-zero if control key is pressed */


	while (1) {

		/* get an event and take appropriate action */
		XNextEvent(Display_p, &report);

		switch(report.type) {

		case Expose:
			/* repaint screen */
			while(XCheckTypedEvent(Display_p, Expose, &report))
				;
			if (window_size == SMALL) {
				TooSmall(Win);
			}
			else {
				do_cmd('r');
			}
			break;

		case ConfigureNotify:
			/* set up image of proper size */
			Width = report.xconfigure.width;
			Height = report.xconfigure.height;
			if ((Width < Size_hints.min_width) ||
					(Height < Size_hints.min_height)) {
				window_size = SMALL;
			}
			else {
				window_size = OK;
			}
			create_image(Width, Height);
			Conf_info_p->vlines = Height;
			break;

		case ButtonPress:
			/* mouse. left is forward, right is backward scroll */
			if (report.xbutton.button == 1) {
				do_cmd('f');
			}
			else if (report.xbutton.button == 3) {
				do_cmd('b');
			}
			break;

		case KeyPress:
			/* keyboard input. Do appropriate command */
			XLookupString(&(report.xkey), inpbuff, IBUFSIZ, &keysym,
						&compose);
			/* the linux version of isascii claims
			 * that isascii(0xff0d) is true! So I added the
			 * extra check for < 256 (which isascii should already
			 * be checking) */
			if (keysym < 256 && isascii(keysym)) {
				if (control != 0) {
					do_cmd(keysym & 0x1f);
				}
				else {
					do_cmd(keysym);
				}
			}
			else if (keysym == XK_Return) {
				do_cmd('\n');
			}
			else if (keysym == XK_BackSpace) {
				do_cmd('\b');
			}
			else if (keysym == XK_Up) {
				/* use up key as synonym for scrolling back */
				do_cmd('b');
			}
			else if (keysym == XK_Down) {
				/* use down key as synonym for scrolling forward */
				do_cmd('f');
			}
			else if (keysym == XK_Prior) {
				/* use page up key as synonym for previous page */
				do_cmd('p');
			}
			else if (keysym == XK_Next) {
				/* use page down key as synonym for next page */
				do_cmd('n');
			}
			else if (keysym == XK_Control_L || keysym == XK_Control_R) {
				control++;
			}
			break;

		case KeyRelease:
			/* just check for control key release */
			XLookupString(&(report.xkey), inpbuff, IBUFSIZ, &keysym,
						&compose);
			if (keysym == XK_Control_L || keysym == XK_Control_R) {
				if (control > 0) {
					control--;
				}
			}
			break;

		default:
			break;
		}
	}
}


/* create graphics context. Basically copy the example in the X book */

static void
get_GC(win)

Window win;

{
	unsigned long valuemask = 0;
	XGCValues values;
	unsigned int line_width = 6;
	int line_style = LineOnOffDash;
	int cap_style = CapRound;
	int join_style = JoinRound;
	int dash_offset = 0;
	static char dash_list[] = { 12, 24 };
	int list_length = 2;

	gc = XCreateGC(Display_p, Win, valuemask, &values);
	XSetFont(Display_p, gc, Font_info_p->fid);
	XSetForeground(Display_p, gc, Foreground);
	XSetBackground(Display_p, gc, Background);
	XSetLineAttributes(Display_p, gc, line_width, line_style, cap_style,
			join_style);
	XSetDashes(Display_p, gc, dash_offset, dash_list, list_length);
}


/* load a font. Copy example in X book */

static void
load_font()

{
	char *fontname = "9x15";

	if ((Font_info_p = XLoadQueryFont(Display_p, fontname)) == NULL) {
		fprintf(stderr, "can't open 9x15 font\n");
		generalcleanup(1);
	}
}


static void
TooSmall(win)

Window win;

{
	char *string1 = "Too small";

	XDrawString(Display_p, win, gc, 2, Font_info_p->max_bounds.ascent + 2,
			string1, strlen(string1));
	Exit_errmsg = "Window too small\n";
	( *(Conf_info_p->cleanup) )  (1);
}


/* X cleanup function */

void
xterm_cleanup(status)

int status;

{
	/* free all X resources */
	XUnloadFont(Display_p, Font_info_p->fid);
	XFreeGC(Display_p, gc);
	XCloseDisplay(Display_p);

	/* call non-terminal-type specific cleanup */
	generalcleanup(status);
}

/* Handler for XIO. Some window managers apparently destroy the X connection
 * when the user closes the window, in a way that we don't get a signal,
 * so we set up this handler for that case.
 * Unfortunately we can't distinguish this "normal" case of the
 * user closing the window, resulting in a broken X connection,
 * from some other "real" IO errror, so we have to hope we don't
 * hit any real errors that would then fail silently.
 * This function is just a wrapper for the generic cleanup function,
 * to get the function signature that XSetIOErrorHandler() expects.
 */

static int
err_handler(disp_p)

Display * disp_p;

{
	generalcleanup(0);
	/* We never actually gets here, because generalcleanup() will exit
	 * like XSetIOErrorHandler man page says we should, but we say to
	 * return something so compilers won't complain.
	 */
	return(0);
}


/* draw screen in X mode */

void
xterm_draw(line, small)

int line;	/* start drawing at this raster line */
int small;	/* if YES, use small, full-page mode */

{
	register int i;
	long offset;			/* offset into file where page begins */
	int fd;				/* file descriptor of bitmap file */


	/* make sure we have a valid page */
	if (Currpage_p == (struct Pginfo *) 0) {
		( *(Conf_info_p->error) ) ("page # out of range");
		return;
	}

	/* find data in bitmap file */
	offset = Currpage_p->seqnum * BYTES_PER_PAGE;
	fd = gen1file(small);
	lseek(fd, offset + line * BYTES_PER_LINE, SEEK_SET);

	/* copy into memory and display it */
	for (i = 0; i < Conf_info_p->vlines; i++) {
		if (read(fd, Image_p->data + i * BYTES_PER_LINE, BYTES_PER_LINE) != BYTES_PER_LINE) {
			( *(Conf_info_p->error) ) ("can't read image data");
			return;
		}
	}
	XPutImage(Display_p, Win, gc, Image_p, 0, 0, 0, 0, Width, Height);
	XFlush(Display_p);
}



/* Error handler. Beep, and write error to stderr. */

void
xterm_error(msg)

char *msg;

{
	putc('\7', stderr);
	fprintf(stderr, "%s\n", msg);
}


/* draw a raster bitmap, centered on the window */

void
xterm_raster(bitmap, width, height)

unsigned char *bitmap;	/* what to display */
int width, height;	/* of bitmap, width is in bytes */

{
	register int b;
	int x, y;	/* upper left corner of where to put bitmap, x in bytes */
	XImage *bm_image;
	char *bmap;


	/* figure out how to center on screen */
	x = (BYTES_PER_LINE - width) / 2;
	y = (Conf_info_p->vlines - height) / 2;

	/* get space to image, copy, inverting to white on black. Display,
	 * then release the storage */
	if ((bmap = (char *) malloc(width*height)) == (char *) 0) {
		Exit_errmsg = "Could not allocate memory\n";
		( *(Conf_info_p->cleanup) )  (1);
	}
	for (b = width * height - 1; b >= 0; b--) {
		bmap[b] = bitmap[b] ^ 0xff;
	}
	bm_image = XCreateImage(Display_p, DefaultVisual(Display_p, Xscreen),
			1, XYBitmap, 0, bmap, width * 8, height,
			8, width);
	bm_image->bitmap_unit = 8;
	bm_image->bitmap_bit_order = MSBFirst;
	XPutImage(Display_p, Win, gc, bm_image, 0, 0, x * 8, y, width * 8, height);
	XDestroyImage(bm_image);
	XFlush(Display_p);
}

#else

/* some compilers complain about files that are effectively empty,
 * so put in something even when entire file is effectively ifdef-ed out */
/* Originally used:
 *       static short dummy;
 * but then gcc stated complaining that was unused.
 * So try this to try to keep both happy...
 */
void dummy_xterm_func_to_avoid_empty_file_warning()
{ 
}

#endif

