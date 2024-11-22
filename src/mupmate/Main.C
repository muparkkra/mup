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

// Code for the main window for Mupmate, a front end program for
// the Mup music publisher program from Arkkra Enterprises.
// It uses the FLTK toolkit for OS independence.

// This file contains code for the toolbar and editor window,
// as well as general startup and showing the license.

// We only support editing a single file at a time, so most classes
// as really effectively singletons, but the code is written to be
// able to support multiple instances, in case we ever want to do that.
// That means callback functions are always passed a pointer to
// a class instance as their second argument, and all they do is cast 
// that to the appropriate type, and call the corresponding class method.

// For the most part, widgets are only allocated when needed, but then
// stay around for the life of the process, in case they are needed again.

// Callbacks are named with _cb suffix.
// Pointers are named with _p suffix, except for (char *) types
// that are pointing to text strings, which don't have any special suffix.


#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Tooltip.H>

#include "globals.H"
#include "Main.H"
#include "Preferences.H"
#include "utils.H"
#include "defines.h"

#ifdef POINT
// Definition in defines.h conflicts with typedef in  win32.H
#undef POINT
#endif

#include <FL/x.H>
#ifdef OS_LIKE_WIN32
#include "resource.h"
#else
#include <FL/Fl_File_Icon.H>
#if defined(OS_LIKE_UNIX) && !defined(OS_LIKE_MACOSX)
#include <X11/xpm.h>
#include "mup32.xpm"
#endif
#endif

// Height of the tool bar on the main window
#define TOOLBAR_HEIGHT 30

// How often to blink the cursor.
#define BLINK_RATE 0.5

//----------------------------------------------------------------------

// Define the toolbar and its submenus.
//  Indented lines indicate the submenus.
// The & indicates shortcut key

const char * File_label = "&File";
 const char * New_label = "&New";
 const char * NewFromTemplate_label = "New From &Template";
 const char * Open_label = "&Open...";
 const char * Save_label = "&Save";
 const char * SaveAs_label = "Save &As...";
 const char * Exit_label = "E&xit";
const char * Edit_label = "&Edit";
 const char * Undo_label = "&Undo";
 const char * Cut_label = "Cu&t";
 const char * Copy_label = "&Copy";
 const char * Paste_label = "&Paste";
 const char * Delete_label = "&Delete";
 const char * Find_label = "&Find...";
 const char * FindNext_label = "Find &Next";
 const char * Replace_label = "&Replace...";
 const char * GoTo_label = "&Go To...";
 const char * SelectAll_label = "&Select All";
const char * Run_label = "&Run";
 const char * Display_label = "&Display";
 const char * Play_label = "&Play";
 const char * WritePostScript_label = "&Write PostScript File";
 const char * WriteMIDI_label = "Write &MIDI File";
 const char * ConvertToPDF_label = "&Convert output to PDF";
 const char * Options_label = "&Set Options...";
const char * Config_label = "&Config";
 const char * FileLocations_label = "&File Locations...";
 const char * Preferences_label = "&Preferences...";
 const char * RegistrationForm_label = "&Registration Form...";
 const char * RegistrationKey_label = "Registration &Key...";
const char * Help_label = "&Help";
 const char * UserGuide_label = "Mup &User's Guide";
 const char * StartupHints_label = "&Startup Hints";
 const char * AboutMupmate_label = "&About Mupmate";
 const char * License_label = "&License";

Fl_Menu_Item Toolbar_menu[] = {
	{ File_label,	0,	0,	0,	FL_SUBMENU },
		{ New_label,	FL_CTRL + 'n',	File::New_cb },
		{ NewFromTemplate_label,	FL_CTRL + 't', File::NewFromTemplate_cb },
		{ Open_label,	FL_CTRL + 'o',	File::Open_cb },
		{ Save_label,	FL_CTRL + 's',	File::Save_cb },
		{ SaveAs_label,		0,	File::SaveAs_cb,	0,	FL_MENU_DIVIDER },
		{ Exit_label,	0,	File::Exit_cb },
		{ 0 },
	{ Edit_label,	0,	0,	0,	FL_SUBMENU },
		{ Undo_label,	FL_CTRL + 'z',	Edit::Undo_cb,	0,	FL_MENU_INACTIVE | FL_MENU_DIVIDER },
		{ Cut_label,	FL_CTRL + 'x',	Edit::Cut_cb,	0,	FL_MENU_INACTIVE },
		{ Copy_label,	FL_CTRL + 'c',	Edit::Copy_cb,	0,	FL_MENU_INACTIVE },
		{ Paste_label,	FL_CTRL + 'v',	Edit::Paste_cb,	0,	FL_MENU_INACTIVE },
		{ Delete_label,	FL_Delete,	Edit::Delete_cb,	0,	FL_MENU_INACTIVE | FL_MENU_DIVIDER },
		{ Find_label,	FL_CTRL + 'f',	Edit::Find_cb,	0,	FL_MENU_INACTIVE },
		{ FindNext_label,	FL_F + 3,	Edit::FindNext_cb,	0,	FL_MENU_INACTIVE },
		{ Replace_label,	FL_CTRL + 'h',	Edit::Replace_cb,	0,	FL_MENU_INACTIVE },
		{ GoTo_label,	FL_CTRL + 'g',	Edit::GoTo_cb,	0,	FL_MENU_INACTIVE | FL_MENU_DIVIDER },
		{ SelectAll_label,	FL_CTRL + 'a',	Edit::SelectAll_cb,	0, FL_MENU_INACTIVE },
		{ 0 },
	{ Run_label,	0,	0,	0,	FL_SUBMENU },
		{ Display_label,			0,	Run::Display_cb,	0,	FL_MENU_INACTIVE },
		{ Play_label,			0,	Run::Play_cb,	0,	FL_MENU_INACTIVE },
		{ WritePostScript_label,	0,	Run::WritePostScript_cb,	0, FL_MENU_INACTIVE },
		{ ConvertToPDF_label,		0,	Run::ConvertToPDF_cb,		0, FL_MENU_INACTIVE },
		{ WriteMIDI_label,		0,	Run::WriteMIDI_cb,	0,	 FL_MENU_DIVIDER | FL_MENU_INACTIVE },
		{ Options_label,		0,	Run::Options_cb },
		{ 0 },
	{ Config_label,	0,	0,	0,	FL_SUBMENU },
		{ FileLocations_label,		0,	Config::FileLocations_cb },
		{ Preferences_label,		0,	Config::Preferences_cb },
		{ 0 },
	{ Help_label,	0,	0,	0,	FL_SUBMENU },
		{ UserGuide_label,		0,	Help::Uguide_cb },
		{ StartupHints_label,		0,	Help::Startup_Hints_cb },
		{ AboutMupmate_label,		0,	Help::About_cb },
		{ License_label,		0,	Help::License_cb },
		{ 0 },
	{ 0 }
};


//----------------------------------------------------------------------

// Linked list of main windows, in case we ever support more than one
// at a time. (Currently we don't, because we're not sure if it
// might be more confusing than useful.)
Main * Main::list_p;


// Constructor for main window. It contains toolbar and editor window.

Main::Main(const char * title, const int width, const int height)
	: Fl_Double_Window(width, height, title)
{
	xclass("mup");
#ifdef OS_LIKE_WIN32
	icon((char *)LoadIcon(fl_display, MAKEINTRESOURCE(IDI_ICON)));
#else
#if defined(OS_LIKE_UNIX) && !defined(OS_LIKE_MACOSX)
	fl_open_display();
	Pixmap p, mask;
	XpmCreatePixmapFromData(fl_display, DefaultRootWindow(fl_display),
                                 (char **) mup32_xpm, &p, &mask, NULL);
	icon((char *)p);
#endif
#endif
	// Try to use user's default foreground/background
	Fl::get_system_colors();

	void * data = 0;

	// Create class instances for each toolbar item
	filemenu_p = new File();
	editmenu_p = new Edit();
	configmenu_p = new Config();
	helpmenu_p = new Help();
	runmenu_p = new Run();

	// Add to list of windows 
	next = list_p;
	list_p = this;

	// Create the toolbar and populate its menu items
	toolbar_p = new Fl_Menu_Bar(0, 0, w(), TOOLBAR_HEIGHT);
	int numitems = NUMELEM(Toolbar_menu);
	for (int i = 0; i < numitems; i++) {
		if (Toolbar_menu[i].text != 0) {
			// As we move to each top-level menu item,
			// keep a pointer to that item, which is then
			// used as the argument to callback functions,
			// so they know what object to act on.
			if (strcmp(Toolbar_menu[i].text, File_label) == 0) {
				data = (void *) filemenu_p;
			}
			else if (strcmp(Toolbar_menu[i].text, Edit_label) == 0) {
				data = (void *) editmenu_p;
			}
			else if (strcmp(Toolbar_menu[i].text, Config_label) == 0) {
				data = (void *) configmenu_p;
			}
			else if (strcmp(Toolbar_menu[i].text, Run_label) == 0) {
				data = (void *) runmenu_p;
			}
			else if (strcmp(Toolbar_menu[i].text, Help_label) == 0) {
				data = (void *) helpmenu_p;
			}
		}
		Toolbar_menu[i].user_data(data);
	}
	toolbar_p->copy(Toolbar_menu);

	// Create and configure the editor window
	editor_p = new Fl_Text_Editor(0, TOOLBAR_HEIGHT, w(),
					h() - TOOLBAR_HEIGHT, "");
	editor_p->buffer( new Fl_Text_Buffer );

	// Fill in style table entries
	style_table[0].color = FL_BLACK;
	style_table[1].color = FL_RED;
	style_table[2].color = FL_BLUE;
	unsigned long s;
	for (s = 0; s < NUMELEM(style_table); s++) {
		style_table[s].font = editor_p->textfont();
		style_table[s].size = editor_p->textsize();
		style_table[s].attr = 0;
	}

	// Start with everything unhighlighted
	style_buff_p = 0;
	unhighlight_all();

	// Tell the editor to use the style data
	editor_p->highlight_data(style_buff_p, style_table,
			NUMELEM(style_table), 'A', style_unfinished_cb, 0);

	// Set font/size and arrange to be notified of changes in them
	font_change_reg_p = new Font_change_registration(font_change_cb,
								(void *) this);

	// Several objects need to be notified of changes in the
	// editor window, so they can do things like gray-ungray menu items.
	editor_p->buffer()->add_modify_callback(modify_cb, (void*) this);
	editor_p->buffer()->add_modify_callback(style_update_cb, (void*)this);
	editor_p->buffer()->add_modify_callback(File::modify_cb,
					(void*) filemenu_p);
	editor_p->buffer()->add_modify_callback(Edit::modify_cb,
					(void*) editmenu_p);

	// Initialize state information.
	have_selection = false;
	can_paste = false;
	prev_bufflength = 0;
	// Undo is inactive until user does something that can be undone.
	undo_active = false;
	undo_active_on_next_change = true;

	// Arrange to make cursor blink
	Fl::add_timeout(BLINK_RATE, blinker, this);
	cursor_state = 1;

	// Let editor take as much space as is available
	// if the user resizes the main window. 
	size_range(Min_width, Min_height, 0, 0);
	resizable((Fl_Widget *) editor_p);

	// Other classes need to have access to editor and such
	filemenu_p->set_editor(editor_p);
	filemenu_p->set_parent(this);
	editmenu_p->set_editor(editor_p);
	runmenu_p->set_file(filemenu_p);

	// Arrange for destructor to free the new-ed child widgets
	end();

	show();

	// Arrange for window manager closes to do Exit.
	callback(atclose_cb, this);
	when(FL_WHEN_NEVER);

#if defined(OS_LIKE_UNIX) && !defined(OS_LIKE_MACOSX)
	// Arrange for icon to be associated with window
	XWMHints hints;
	hints.flags = IconPixmapHint | IconMaskHint ;
	hints.icon_pixmap = p;
	hints.icon_mask = mask;
	XSetWMHints(fl_display, fl_xid((Fl_Window *)this), &hints);
#endif
}


// Destructor for main window

Main::~Main()
{
	// If user changed the window size, persist that
	int width, height;
	(void) Preferences_p->get(Main_window_width, width, Default_width);
	(void) Preferences_p->get(Main_window_height, height, Default_height);
	if (width != w() || height != h()) {
		Preferences_p->set(Main_window_width, w());
		Preferences_p->set(Main_window_height, h());
		Preferences_p->flush();
	}

	delete font_change_reg_p;
	font_change_reg_p = 0;
	Fl::remove_timeout(blinker, this);
	// Remove from list of Main windows 
	if (list_p == this) {
		list_p = next;
	}
	else {
		for (Main * m = list_p; m != 0; m = m->next) {
			if (m->next == this) {
				m->next = this->next;
				break;
			}
		}
	}
	delete filemenu_p;
	filemenu_p = 0;
	delete editmenu_p;
	editmenu_p = 0;
	delete configmenu_p;
	configmenu_p = 0;
	delete helpmenu_p;
	helpmenu_p = 0;
	delete runmenu_p;
	runmenu_p = 0;
}


// Callback for when user changes font/size

void
Main::font_change_cb(void * data, Fl_Font font, unsigned char size)
{
	((Main *)data)->font_change(font, size);
}


void
Main::font_change(Fl_Font font, unsigned char size)
{
	// Get shorter name for buffer, as we'll be using it a lot.
	Fl_Text_Buffer * buffer_p = editor_p->buffer();

	// Don't want this change to count as something that can be undone
	buffer_p->canUndo(false);

	// We want to change the entire text buffer, so need to
	// select its whole contents. If there was already a selection,
	// save that and put it back when we are done
	int sel_start, sel_end;
	int had_selection;
	int cursorplace = editor_p->insert_position();
	if ((had_selection = buffer_p->selected()) != 0) {
		buffer_p->selection_position(&sel_start, &sel_end);
		buffer_p->unselect();
	}

	// set new font and size
	buffer_p->select(0, editor_p->buffer()->length() - 1);
	editor_p->textfont(font);
	editor_p->textsize(size);
	buffer_p->unselect();

	// Put selection and cursor back as they were before font change
	if (had_selection) {
		buffer_p->select(sel_start, sel_end);
	}
	editor_p->insert_position(cursorplace);
	buffer_p->canUndo(true);

	// Fix style array
	unsigned long s;
	for (s = 0; s < NUMELEM(style_table); s++) {
		style_table[s].font = font;
		style_table[s].size = size;
	}
	editor_p->highlight_data(style_buff_p, style_table,
			NUMELEM(style_table), 'A', style_unfinished_cb, 0);
}


// Callback for when editor window changes.
// This arranges to gray/ungray toolbar menu items.

void
Main::modify_cb(int, int, int, int, const char *, void * data)
{
	((Main *)data)->modify();
}

void
Main::modify()
{
	int bufflength = editor_p->buffer()->length();
	// See if what changed is something we might care about.
	if (editor_p->buffer()->selected() != have_selection ||
			editmenu_p->can_paste() != can_paste
			|| undo_active != undo_active_on_next_change
			|| bufflength < 2 || prev_bufflength == 0) {

		// Something changed, and we may need to
		// gray or ungray menu items in response.
		have_selection = editor_p->buffer()->selected();
		const Fl_Menu_Item * menu_p = toolbar_p->menu();
		Fl_Menu_Item * item_p;
		// Walk through toolbar and submenus, checking
		// if anything needs to be grayed/ungrayed.
		for (int i = 0; i < toolbar_p->size(); i++) {
			const char * mtext = toolbar_p->text(i);
			if (mtext == 0) {
				continue;
			}
			// Can only Copy, Cut, and Delete if something
			// is selected.
			if (strcmp(mtext, Copy_label) == 0 ||
					strcmp(mtext, Cut_label) == 0 ||
					strcmp(mtext, Delete_label) == 0) {
				// have to un-const so we can (de)activate
				item_p = (Fl_Menu_Item *) &(menu_p[i]);
				if (have_selection) {
					item_p->activate();
				}
				else {
					item_p->deactivate();
				}
			}

			// Paste is different. It becomes active when there
			// is something in clipboard, and never again becomes
			// inactive.
			if (strcmp(mtext, Paste_label) == 0 &&
						editmenu_p->can_paste()) {
				((Fl_Menu_Item *)&(menu_p[i]))->activate();
				can_paste = true;
			}

			// Undo is also different. On first change of any
			// kind it become active, and stays that way,
			// except it gets reset on new file.
			if (strcmp(mtext, Undo_label) == 0) {
				if (undo_active && ! undo_active_on_next_change) {
					((Fl_Menu_Item *)&(menu_p[i]))->deactivate();
					undo_active = false;
					undo_active_on_next_change = true;
				}
				else if ( ! undo_active && undo_active_on_next_change) {
					((Fl_Menu_Item *)&(menu_p[i]))->activate();
					undo_active = true;
				}
			}

			// Find and FindNext are inactive when file is empty,
			// because obviously there is nothing to find.
			// Similar for Replace, Select All, and GoTo.
			// Also for all the Run things
			if (strcmp(mtext, Find_label) == 0 ||
					strcmp(mtext, FindNext_label) == 0 ||
					strcmp(mtext, Replace_label) == 0 ||
					strcmp(mtext, SelectAll_label) == 0 ||
					strcmp(mtext, GoTo_label) == 0 ||
					strcmp(mtext, Display_label) == 0 ||
					strcmp(mtext, Play_label) == 0 ||
					strcmp(mtext, WritePostScript_label) == 0 ||
					strcmp(mtext, ConvertToPDF_label) == 0 ||
					strcmp(mtext, WriteMIDI_label) == 0) {
				if (bufflength == 0) {
					((Fl_Menu_Item *)&(menu_p[i]))->deactivate();
				}
				else {
					((Fl_Menu_Item *)&(menu_p[i]))->activate();
				}
			}
		}
	}
	prev_bufflength = bufflength;
}


void
Main::style_unfinished_cb(int, void *)
{
}

void
Main::style_update_cb(int pos, int inserted, int deleted, int restyled, const char *, void * data)
{
	Main * main_p;

	main_p = (Main *) data;

	if (inserted == 0 && deleted == 0) {
		// This happens, for example, when initially reading in
		// and file and unhighlighting all of it. I don't think
		// We really need to do the unselect in our case, but
		// that's what the examples do in this case, and it
		// shouldn't hurt.
		main_p->style_buff_p->unselect();
		return;
	}

	// This next section is basically like the FLTK example,
	// to start any new characters as normal, and bring the style
	// buffer in sync with the text buffer.
	if (inserted > 0) {
		char * style = new char[inserted + 1];
		memset(style, 'A', inserted);
		style[inserted] = '\0';
		main_p->style_buff_p->replace(pos, pos + deleted, style);
		delete[] style;
	}
	else {
		main_p->style_buff_p->remove(pos, pos + deleted);
	}

	main_p->editor_p->redisplay_range(pos, pos + inserted - deleted);

	// When user makes any change in an error line, we unhighlight
	// that line. We can't know if they really fixed anything, but
	// assume they at least tried. This looks better than mixture of
	// highlighted and not on the line.
	int begin = main_p->editor_p->buffer()->line_start(pos + inserted - deleted);
	int end = main_p->editor_p->buffer()->line_end(pos + inserted - deleted);
	int length = end - begin + 1;
	char * style = new char[length + 1];
	memset(style, 'A', length);
	style[length] = '\0';
	main_p->style_buff_p->replace(begin, end, style);
	delete[] style;

	main_p->editor_p->redisplay_range(begin, end);
}


// Make the style of the given line the style of the given index
// in the style array.

void
Main::highlight_error_line(const int linenum, const char index)
{
	int start;
	int end;

	// Figure out where in the text buffer the error line starts/ends
	start = editor_p->buffer()->skip_lines(0, linenum - 1);
	if (start >= editor_p->buffer()->length()) {
		// Is off the end of the file; ignore
		return;
	}
	end = editor_p->buffer()->line_end(start);

	// Mark those characters in the requested style
	char * style = new char[end - start + 2];
	memset(style, index, end - start + 1);
	style[end - start + 1] = '\0';
	style_buff_p->replace(start, end, style);
	delete[] style;
	editor_p->redisplay_range(start, end);
}


// Make all lines print in normal color

void
Main::unhighlight_all()
{
	// Allocate a style buffer. Create a temporary style array
	// and copy its contents into the buffer.
	int style_length = editor_p->buffer()->length();
	if (style_buff_p == 0) {
		style_buff_p = new Fl_Text_Buffer(style_length);
	}
	char * style = new char[style_length + 1];
	memset(style, 'A', style_length);
	style[style_length] = '\0';
	style_buff_p->text(style);
	delete[] style;
	editor_p->redisplay_range(0, style_length);
}


// This method gets called when user starts working on a new file.
// It sets state information so we can know how to gray menu items properly.

void
Main::begin_new_file()
{
	// transition Undo-ability state from true to false
	undo_active = true;
	undo_active_on_next_change = false;
	editor_p->buffer()->call_modify_callbacks();
	runmenu_p->clean_up();
}


// Handle some special cases.
// 1. By default fltk will exit the main window upon getting escape.
// That seems bad, since a vi user will be used to hitting escape all the
// time when editing, because that is always a "safe" thing to do,
// and if they did it here by mistake, they would lose all
// their text entry since the last save. So we ignore the escape in this window.
// I suppose we could ask if they really want to quit...
// 2. If user does cut or copy via keyboard accelerator, the normal code
// for ungraying the Paste button doesn't get called, so we catch that case
// here and ungray it.

int
Main::handle_events(int e)
{
	// If escape is received while on main window,
	// return 1 to show that we consumed the event.
	if (e == FL_SHORTCUT && Fl::event_key() == FL_Escape) {
		for (Main * m_p = list_p; m_p != 0; m_p = m_p->next) {
			if (Fl::first_window() == m_p) {
				return(1);
			}
		}
	}

	// If user did cut or copy via cntl-c or cntl-x,
	// arrange to ungray Paste.
	if (e == FL_KEYUP && (Fl::event_state() & FL_CTRL)  &&
			(Fl::event_key() == 'v' || Fl::event_key() == 'x')) {
		for (Main * m_p = list_p; m_p != 0; m_p = m_p->next) {
			if (Fl::first_window() == m_p) {
				m_p->editmenu_p->set_can_paste();
				break;
			}
		}
	}

	return(0);
}


// If user tries to close the main window via the window manager
// while having unsaved changes, we ask user if they want to save
// the changes first. 

CALL_BACK(Main, atclose)
{
	File::Exit_cb(0, filemenu_p);
}


// Blink the cursor. It can be hard to see if next to selected text if not
// blinking. We could potentially optimize to only do this while
// the window has focus, but it doesn't seem worth the complication...

void
Main::blinker(void * data)
{
	Main * obj_p = (Main *) data;
	// Put cursor into opposite of its current state
	obj_p->cursor_state ^= 1;
	obj_p->editor_p->show_cursor(obj_p->cursor_state);
	// Reset timer to call ourselves again.
	Fl::repeat_timeout(BLINK_RATE, blinker, data);
}


// Show the user license and startup hints
// if we haven't already done that before.

void
Main::hints(void)
{
	int did_startup;
	(void) Preferences_p->get(Showed_startup_hints, did_startup,
					Default_startup_hints_flag);

	if ( ! did_startup ) {
		Help::License_cb(0, (void *) helpmenu_p);
	}

	int migration_status;
	(void) Preferences_p->get(Migration_status, migration_status,
					Default_migration_status);
	if ( ! did_startup || migration_status == Data_migrated) { 
		Help::Startup_Hints_cb(0, (void *) helpmenu_p);
	}
}

#ifdef OS_LIKE_MACOSX

void
Main::apple_open_cb(const char *filename)
{
	// File name passed in is full Unix path
	list_p->filemenu_p->load_file(filename);
}
#endif
//----------------------------------------------------------------------

int
main(int argc, char **argv, const char **arge)
{
	// The arge value may get changed when we set new environment
	// variables, so look up PATH first thing.
	get_path(arge);

	// Uguide browser needs to show images
	fl_register_images();

#ifndef OS_LIKE_WIN32
	// On Windows we use the native Open/Save As dialogs.
	// On other platforms we use FLTK's, but add icon for Mup files.
	Fl_File_Icon::load_system_icons();
	File::add_mup_icon();
#endif

	// Try to get best hardware support for graphics
	Fl::visual(FL_DOUBLE|FL_INDEX);

	// Get the user's preferences that persists across sessions
	Preferences_p = new Fl_Preferences(Fl_Preferences::USER,
					"arkkra.com", "mupmate72");

	// If user is upgrading from earlier version,
	// we want to migrate their non-version-specific preferences.
	migrate_preferences();

	// Enable tips when user hovers their mouse over a widget.
	// If user doesn't like them, they can set delay to huge value.
	Fl_Tooltip::enable();
	double tooltips_delay;
	(void) Preferences_p->get(Tooltips_delay_preference, tooltips_delay,
					Default_tooltips_delay);
	Fl_Tooltip::delay(tooltips_delay);

	// Set $MUPPATH
	char * val;
	(void) Preferences_p->get(MUPPATH_location, val,
						Default_MUPPATH_location);
	set_muppath(val);

	// Tell Mup that it is being run via mupmate,
	// so it can give more appropriate error messages.
	// Make non-const copy for passing to putenv.
	char * mupmate_flag = new char[10];
	strcpy(mupmate_flag, "MUPMATE=1");
	putenv(mupmate_flag);

	// Create main window
	int width, height;
	(void) Preferences_p->get(Main_window_width, width, Default_width);
	(void) Preferences_p->get(Main_window_height, height, Default_height);
	Main *main_p = new Main("Mupmate", width, height);

	// Ensure "escape" key doesn't kill main window,
	// and make sure Paste ungraying works
	Fl::add_handler(Main::handle_events);

	// Try to find some reasonable defaults for configuration items
	// that aren't already set.
	deduce_helper_locations();

	// Display the main window.
	main_p->show(1, argv);

	// The first time, we show user some hints.
	main_p->hints();

#ifdef OS_LIKE_MACOSX
	fl_open_callback(Main::apple_open_cb);
    // skip argument processing, if launched with process serial number by Finder
    if (argc < 2 || strncmp(argv[1], "-psn", 4) != 0) {
#endif
	// Expect 0 or 1 args. If 1, should be name of file to load
	if (argc > 1) {
		// We may be changing directory to user's Mup folder,
		// but if they gave file as a command line argument, we want
		// that to be used from current directory.
		if (is_absolute(argv[1])) {
			main_p->filemenu_p->load_file(argv[1]);
		}
		else {
			char fullname[FL_PATH_MAX + strlen(argv[1]) + 2];
			if (getcwd(fullname, FL_PATH_MAX) == 0) {
				fl_alert("Unable to obtain value of current folder.");
				// We'll use path as is and hope for the best.
				// If the file is good, we should be able to
				// read it, so the only problem should be if
				// they specified a different Mup folder, and
				// we cd there and then they write out this
				// file. In that case, the file would get
				// written to their Mup folder instead of
				// the original folder, so user may not be
				// able to find it and/or it could overwrite
				// a file, if they already had a file by that
				// name in that directory. But this is
				// something that should "never" happen,
				// so we'll live with it. (Could happen if
				// they have a really long path.)
				main_p->filemenu_p->load_file(argv[1]);
			}
			else {
				(void) snprintf(fullname + strlen(fullname),
					sizeof(fullname) - strlen(fullname),
					"%c%s", dir_separator(), argv[1]);
				main_p->filemenu_p->load_file(fullname);
			}
		}
	}
	if (argc > 2) {
		fl_alert("Only expecting one file; extra arguments are being ignored.");
	}
#ifdef OS_LIKE_MACOSX
    }
#endif

	// Go to where user said they want to store their Mup files by default.
	// Need to wait to do this until after we have deduced locations
	// of executable, in case they were in current directory.
	char * mup_dir;
	(void) Preferences_p->get(Music_files_location, mup_dir,
						Default_music_files_location);
	if (strcmp(mup_dir, ".") != 0) {
		char expanded_path[FL_PATH_MAX];
		filename_expand(expanded_path, mup_dir);
		if (chdir(expanded_path) != 0) {
			char curr_dir[FL_PATH_MAX] = "current";
			char message[2 * FL_PATH_MAX + 100];
			if (getcwd(curr_dir, sizeof(curr_dir)) == 0) {
				strcpy(curr_dir, "<Unknown folder>");
			}
			(void) snprintf(message, sizeof(message),
				"Unable to change to folder\n"
				"\"%s.\"\nStaying in \"%s\" folder.\n"
				"Fix setting of \"Folder for Mup Files\"\n"
				"in Config->File Locations.",
				mup_dir, curr_dir);
			fl_alert("%s", message);
		}
	}

	// Go into main event-handler loop
	int exitvalue = Fl::run();
	Main::clean_exit(exitvalue);
	/*NOTREACHED*/
	return(exitvalue);
}


// Clean up all the windows and their children and exit.

void
Main::clean_exit(int exitval)
{
	Main * m_p;
	Main * nextwin_p;

	for (m_p = list_p; m_p != 0; m_p = nextwin_p) {
		nextwin_p = m_p->next;
		delete m_p;
	}
	exit(exitval);
}
