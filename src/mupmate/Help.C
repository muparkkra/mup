static const char * const license_text =
" Copyright (c) 1995-2019  by Arkkra Enterprises.\n\
 All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms,\n\
 with or without modification, are permitted provided that\n\
 the following conditions are met:\n\
\n\
 1. Redistributions of source code must retain\n\
 the above copyright notice, this list of conditions\n\
 and the following DISCLAIMER.\n\
\n\
 2. Redistributions in binary form must reproduce the above\n\
 copyright notice, this list of conditions and\n\
 the following DISCLAIMER in the documentation and/or\n\
 other materials provided with the distribution.\n\
\n\
 3. Any additions, deletions, or changes to the original files\n\
 must be clearly indicated in accompanying documentation,\n\
 including the reasons for the changes,\n\
 and the names of those who made the modifications.\n\
\n\
	DISCLAIMER\n\
\n\
 THIS SOFTWARE IS PROVIDED \"AS IS\" AND ANY EXPRESS\n\
 OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,\n\
 THE IMPLIED WARRANTIES OF MERCHANTABILITY\n\
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.\n\
 IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,\n\
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,\n\
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n\
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n\
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,\n\
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE)\n\
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,\n\
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\
"
;

// Code for Help menu item from main toolbar

#include "globals.H"
#include "Preferences.H"
#include "Config.H"
#include "Help.H"
#include <FL/fl_ask.H>
#include <string.h>
#include <stdlib.h>


// Window for browsing Mup User's Guide

Uguide_browser::Uguide_browser(const int width, const int height)
	: Fl_Double_Window(0, 0, width, height, "Mup User's Guide")
{
	browser_p = new Fl_Help_View(x(), y(), w(), h(), "");
	// Set font/size and arrange to get notified of changes in them
	font_change_reg_p = new Font_change_registration(font_change_cb, (void *) this);

	// Haven't loaded the User's Guide yet
	loaded = false;

	// Allow browser window to be made as big as the user wants
	size_range(Min_width, Min_height, 0, 0);
	resizable((Fl_Widget *)browser_p);

	// Fix problem with following relative links properly.
	browser_p->link(resolve_link);

	// Arrange for destructor to clean up new-ed widgets
	end();
}


Uguide_browser::~Uguide_browser(void)
{
	delete font_change_reg_p;
	font_change_reg_p = 0;

	// If user changed the window size, persist that
	int width, height;
	(void) Preferences_p->get(Help_window_width, width, Default_width);
	(void) Preferences_p->get(Help_window_height, height, Default_height);
	if (width != w() || height != h()) {
		Preferences_p->set(Help_window_width, w());
		Preferences_p->set(Help_window_height, h());
		Preferences_p->flush();
	}
}


// Load Mup User's Guide into browser, if haven't already done so.

void
Uguide_browser::load_uguide(void)
{
	if (!loaded) {
		// Entry under File Locations tells the base directory
		// for Mup documentation. We concatenate the uguide/index.file
		// to that directory to get top level file for User's Guide.
		char * base_url;
		(void) Preferences_p->get(Mup_documentation_location, base_url,
				Default_Mup_documentation_location);
		const char * url = users_guide_index_file(base_url);

		// Fltk has a bug (at least in some versions):
		// the documentation claims load()
		// returns -1 on failure, but in fact it always returns 0.
		// So the strncmp attempts to deduce if it failed.
		if (browser_p->load(url) != 0 ||
				strstr(browser_p->value(), "Mup User's Guide")
				== 0) {
			fl_alert("Unable to load User's Guide.\n  %s\n"
				"Check settings in Config > File Locations", url);
		}
		else {
			loaded = true;
		}
	}
}


// Callback for when user changes font/size

void
Uguide_browser::font_change_cb(void * data, Fl_Font font, unsigned char size)
{
	((Uguide_browser *)data)->font_change(font, size);
}

void
Uguide_browser::font_change(Fl_Font font, unsigned char size)
{
	browser_p->textfont(font);
	browser_p->textsize(size);
}

// On Windows, Fl_Help_View doesn't seem to properly follow relative
// URLs (it goes relative to current working directory rather than
// relative to the file containing the link),
// so we use the link() callback to prepend the proper directory.
// On Linux or Mac, things work fine without this kludge, but things also
// can work with it, so to keep code common, we do it always.

// It seems fltk (at least some versions) gets confused if we use a static
// area to store the resolved name, and return a pointer to that area
// for everything. While file URIs work okay, once this is called on an image,
// the Help_view just prints GIF87 and some garbage at the top of the window,
// and you are then stuck and can't use the Help anymore. It works if
// we return a malloced string every time, but that could potentially be
// a huge memory leak. So we make a unique resolved string
// for each unique URI this is called with. 
// One benefit is that the caching may improve speed.

// Given this is for a human interaction,
// even a linear search of a cache would probably be plenty fast enough,
// but we may as well hash.
// As of this writing, there are approximately 200 files
// making up the user's guide, although many of them have internal tags.
// So there are on the order of a few hundred unique URIs.
// But in a typical Mupmate run, a user will probably only look at
// a small fraction of those. So even allowing for growth,
// and the worst case of user looking at every page, this hash table size
// should hardly ever give more than a few hash collisions per search.
// A modulo hash is used, so the size should be prime.
const int URI_tsize = 117;

// This class is used to implement a hash table that chaces a mapping
// from a possibly relative URIs to full path URIs.
class URI_Cache {
public:
	// Establish mapping
	URI_Cache(const char * uri, const char * resolved_uri);
	// Given an instance, return its mapped value
	const char * get() { return(resolved); }
	// Given an unmapped name, return its mapped name
	static const char * get(const char * uri);

private:
	static int	hash(const char * uri);
	// Destructor is actually unused
	~URI_Cache();

	int		hashnum;
	const char *	name;		// unmapped
	const char *	resolved;	// mapped
	URI_Cache *	next;		// hash collion chain

	// The hash table
	static URI_Cache *known_uris[URI_tsize];
};

// The hash table
URI_Cache * URI_Cache::known_uris[URI_tsize];


// Constructor establishes a mapping
URI_Cache::URI_Cache(const char * uri, const char * resolved_uri)
{
	name = strdup(uri);
	resolved = strdup(resolved_uri);
	hashnum = hash(uri);
	// Link into hash table
	next = known_uris[hashnum];
	known_uris[hashnum] = this;
}

URI_Cache::~URI_Cache()
{
	// We never actually call this, except maybe on program termination
	// when we no longer care, so we don't bother to clean up anything.
	// If it is ever needed, it would free name and resolved and
	// unlink itself from the hash table.
	// Actually, I supppose we should clean up the hash table if the
	// user changes the documentation path, but there should be no good
	// reason for them to do that often enough to waste enough memory
	// to care about.
}

// Given an unmapped name, return the mapped name, or null if not mapped yet.

const char *
URI_Cache::get(const char * uri)
{
	URI_Cache *uri_cache_p;

	for (uri_cache_p = known_uris[hash(uri)]; uri_cache_p != 0;
					uri_cache_p = uri_cache_p->next) {
		if (strcmp(uri, uri_cache_p->name) == 0) {
			return(uri_cache_p->resolved);
		}
	}
	return(0);
}


int
URI_Cache::hash(const char * name)
{
	int h = 0;
	const char *n;

	for (n = name  ; *n != 0; n++) {
		h <<= 1;
		h ^= *n;
	}
	return(abs(h) % URI_tsize);
}

const char *
Uguide_browser::resolve_link(Fl_Widget * help_browser_p, const char * uri)
{
	char link_path[FL_PATH_MAX];

	if (strncmp(uri, "http:", 5) == 0) {
		// Uguide has a few links to arkkra.com.
		// They won't actually work, because we don't handle
		// http, but it's better to let them through than to
		// change the path and really confuse the user.
		return(uri);
	}

	// Return from cache if it is there.
	const char * resolved;
	if ((resolved = URI_Cache::get(uri)) != 0) {
		return(resolved);
	}

	// Not in cache. Will have to add it.
	char * base_url;
	(void) Preferences_p->get(Mup_documentation_location, base_url,
				Default_Mup_documentation_location);
	char expanded_url[FL_PATH_MAX];
	filename_expand(expanded_url, base_url);
	(void) snprintf(link_path, sizeof(link_path),
				"%s%c%s%c%s", expanded_url, dir_separator(),
				uguide_directory, dir_separator(),
				fl_filename_name(uri));

	URI_Cache *uri_cache_p;
	uri_cache_p = new URI_Cache(uri, link_path);
	return(uri_cache_p->get());
}

//---- Hints for user when they use Mupmate for the first time
// (or they can ask to see it later too)

const char * Welcome_message =
	"Welcome to Mupmate. Mupmate provides an interface to the\n"
	"Mup music publication program,\n"
	"making it easy to edit, display, and print musical scores.\n"
	"\n"
	"If you have not used Mup before, you should begin by selecting\n"
	"   Help > Mup User's Guide\n"
	"for information on how to use Mup.\n"
	"\n"
	"You may also want to verify that the settings under\n"
	"   Config > File Locations   and    Config > Preferences\n"
	"are what you want, and adjust them if you wish.\n";

const char * Migration_message =
	"Thank you for upgrading Mupmate. Mupmate provides an interface\n"
	"to the Mup music publication program,\n"
	"making it easy to edit, display, and print musical scores.\n"
	"\n"
	"Your preferences from the previous version have been migrated,\n"
	"but you should check all the settings under\n"
	"   Config > File Locations   and    Config > Preferences\n"
	"to make sure they are correct, and adjust them if you wish.\n";

StartupHints::StartupHints(void)
	: Fl_Double_Window(Default_width - 100, Default_height - 150,
	"Mup Startup Hints")
{
	text_p = new Fl_Text_Display(20, 20, w() - 40, h() - 90);
	resizable((Fl_Widget *) text_p);
	text_p->buffer( new Fl_Text_Buffer () );
	font_change_reg_p = new Font_change_registration(font_change_cb, (void *) this);
	int migration_status;
	(void) Preferences_p->get(Migration_status, migration_status,
						Default_migration_status);
	if (migration_status == Data_migrated) {
		text_p->buffer()->text(Migration_message);
		Preferences_p->set(Migration_status, Migration_complete);
	}
	else {
		text_p->buffer()->text(Welcome_message);
	}
	char * doc_dir;
	(void) Preferences_p->get(Mup_documentation_location, doc_dir,
			Default_Mup_documentation_location);
	text_p->buffer()->append("\nAdditional documentation is available in the folder:\n  ");
	text_p->buffer()->append(doc_dir);

	OK_p = new Fl_Return_Button(w() / 2 - 50, h() - 50, 100, 30,
								"OK");
	OK_p->callback(OK_cb, this);
	show();
	end();

	Preferences_p->set(Showed_startup_hints, 1);
	Preferences_p->flush();
}

StartupHints::~StartupHints()
{
	delete font_change_reg_p;
	font_change_reg_p = 0;
}

// Callback for user clicking OK when done reading startup hints

CALL_BACK(StartupHints, OK)
{
	hide();
}


// Callback for when user changes font/size

void
StartupHints::font_change_cb(void * data, Fl_Font font, unsigned char size)
{
	((StartupHints *)data)->font_change(font, size);
}

void
StartupHints::font_change(Fl_Font font, unsigned char size)
{
	text_p->textfont(font);
	text_p->textsize(size);
	text_p->redisplay_range(0, text_p->buffer()->length());
}

//-----the "About" window-----------------------------------------------

About_dialog::About_dialog(void)
	: Fl_Double_Window(270, 200, "About Mupmate")
{
	message_p = new Fl_Multiline_Output(20, 20, w() - 40, 120, "");
	message_p->value("\n   Mupmate is a user interface\n"
			"   for the Mup music publisher\n"
			"   program.\n"
			"\n"
			"     This is Version 6.7");
	// Hide the cursor by making same color as background
	message_p->cursor_color(message_p->color());

	ok_p = new Fl_Return_Button((w() - 100) / 2, 155, 100, 30, "OK");
	ok_p->callback(OK_cb, this);

	// Arrange for destructor to clean up new-ed widgets
	end();
}

About_dialog::~About_dialog(void)
{
}


CALL_BACK(About_dialog, OK)
{
	hide();
}

//------the Mup license window -----------------------------

License_Display::License_Display()
	: Fl_Double_Window(Default_width, Default_height, "Mup License")
{
	// widget for displaying the license text
	text_p = new Fl_Text_Display(20, 20, w() - 40, h() - 90);
	resizable((Fl_Widget *) text_p);
	text_p->buffer( new Fl_Text_Buffer () );
	font_change_reg_p = new Font_change_registration(font_change_cb, (void *) this);
	text_p->buffer()->text(license_text);

	ok_p = new Fl_Return_Button(w() / 2 - 50, h() - 50, 100, 30, "OK");
        ok_p->callback(OK_cb, this);

	end();
}

License_Display::~License_Display()
{
	delete font_change_reg_p;
	font_change_reg_p = 0;
}

CALL_BACK(License_Display, OK)
{
	hide();
}

// Callback for when user changes font/size

void
License_Display::font_change_cb(void * data, Fl_Font font, unsigned char size)
{
	((License_Display *)data)->font_change(font, size);
}

void
License_Display::font_change(Fl_Font font, unsigned char size)
{
	text_p->textfont(font);
	text_p->textsize(size);
	text_p->redisplay_range(0, text_p->buffer()->length());
}


//------the Help menu item from main toolbar-----------------------------

Help::Help(void)
{
	uguide_p = 0;
	startup_hints_p = 0;
	about_p = 0;
	license_p = 0;
}

Help::~Help(void)
{
	if (about_p != 0) {
		delete about_p;
		about_p = 0;
	}
	if (startup_hints_p != 0) {
		delete startup_hints_p;
		startup_hints_p = 0;
	}
	if (uguide_p != 0) {
		delete uguide_p;
		uguide_p = 0;
	}
	if (license_p != 0) {
		delete license_p;
		license_p = 0;
	}
}


// Callback for when user requests viewing the Mup User's Guide

CALL_BACK(Help, Uguide)
{
	if (uguide_p == 0) {
		// first time, create widget
		int width, height;
		(void) Preferences_p->get(Help_window_width, width, Default_width);
		(void) Preferences_p->get(Help_window_height, height, Default_height);
		uguide_p = new Uguide_browser(width, height);
	}
	// Always attempt to load in case URL was bad last time but okay now.
	// If already loaded, this will be a no-op.
	uguide_p->load_uguide();
	uguide_p->show();
}


// Callback for when user clicks "Startup Hints" button

CALL_BACK(Help, Startup_Hints)
{
	// We delete any existing instance and start over,
	// just in case the path to the documentation has changed.
	if (startup_hints_p != 0) {
		delete startup_hints_p;
	}
	startup_hints_p = new StartupHints();
	startup_hints_p->show();
}


// Callback for when user clicks "About" button

CALL_BACK(Help, About)
{
	if (about_p == 0) {
		// first time, create widget
		about_p = new About_dialog();
	}
	about_p->show();
}


// Callback for when user selects "License" menu item

CALL_BACK(Help, License)
{
	if (license_p == 0) {
		// first time, create widget
		license_p = new License_Display();
	}
	license_p->show();
}
