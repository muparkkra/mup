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

#ifndef _MAIN_H_
#define _MAIN_H_

// Header file for the main Mupmate window, containing toolbar and editor.

#include <FL/Fl_Widget.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Editor.H>

#include "File.H"
#include "Edit.H"
#include "Run.H"
#include "Config.H"
#include "Help.H"


// Class for main window, with editor and toolbar

class Main : public Fl_Double_Window {

public:
	Main(const char * title, const int width, const int height);
	~Main();

	// Callbacks
	static void modify_cb(int, int, int, int, const char *, void * data);
	static void font_change_cb(void * data, Fl_Font font, unsigned char size);
	static void atclose_cb(Fl_Widget *, void * data);
	static void apple_open_cb(const char *c);
	// For highlighting error lines
	static void style_unfinished_cb(int, void *);
	static void style_update_cb(int pos, int inserted, int deleted, int,
			const char *, void * data);


	// We don't want Escape to cause main window to close,
	// and need to handle Paste ungraying specially.
	// This event handler takes care of those things.
	static int handle_events(int);

	// Highlight the specified line in red
	void highlight_error_line(const int linenum, const char index);
	// Unhighlight everything
	void unhighlight_all(void);

	// Show user hints the first time they bring up Mupmate
	void hints(void);

	// Reset state information when user opens a new file.
	void begin_new_file();

	// Pointers to the classes for each toolbar menu item widgets
	File * filemenu_p;
	Edit * editmenu_p;
	Config * configmenu_p;
	Help * helpmenu_p;
	Run * runmenu_p;

	// Clean up all windows 
	static void clean_exit(int exitval = 0);

private:
	// called when input text is modified
	void modify(void);

	// Sets font/size of editor
	void font_change(Fl_Font font, unsigned char size);

	// Make window manager close like Exit
	void atclose(void);

	// Cursor blinker
	static void blinker(void *);

	// Ponters to the top level widgets
	Fl_Menu_Bar * toolbar_p;
	Fl_Text_Editor * editor_p;
	Fl_Text_Buffer * style_buff_p;

	// For highlighting error lines
	Fl_Text_Display::Style_Table_Entry style_table[3];


	// For font/size change notification
	Font_change_registration * font_change_reg_p;

	// True if there is a selected area in editor window
	bool have_selection;

	// True if something in clipboard
	bool can_paste;

	// For knowing when to gray out Undo menu item
	bool undo_active;
	bool undo_active_on_next_change;
	int prev_bufflength;

	// Current cursor on/off state, for blinker
	int cursor_state;

	// We currently only have one main window, but code is
	// general to allow multiple windows. We keep a linked list
	// of all the windows.
	static Main * list_p;
	Main * next;
};

#endif
