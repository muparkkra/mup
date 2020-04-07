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

#ifndef _FILE_H_
#define _FILE_H_

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_File_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/filename.H>

class Main;


// Class used to ask user if they want to save changes.
// The FLTK fl_choice doesn't do quite what we want, so we make our own...

class Save_confirm_dialog : Fl_Double_Window {
public:
	enum Answer { Cancel, No, Yes };

	// This brings up the dialog
	static Answer confirm_save(const char * text, bool hide_the_No = false);

private:
	Save_confirm_dialog(const char * text);
	~Save_confirm_dialog(void);

	// Widgets
	Fl_Box * icon_p;
	Fl_Box * message_p;
	Fl_Return_Button * yes_p;
	Fl_Button * no_p;
	Fl_Button * cancel_p;
};



// Class for the items off of "File" on the main menu bar

class File {

friend class Run;

public:
	File();
	~File();

	// Callbacks
	static void modify_cb(int, int, int, int, const char *, void * data);
	static void New_cb(Fl_Widget *, void * data);
	static void NewFromTemplate_cb(Fl_Widget *, void * data);
	static void Open_cb(Fl_Widget *, void * data);
	static void Save_cb(Fl_Widget *, void * data);
	static void SaveAs_cb(Fl_Widget *, void * data);
	static void Exit_cb(Fl_Widget *, void * data);

	// Load a file into the text editor
	void load_file(const char * name);

	// Give access to the text editor buffer
	Fl_Text_Buffer * get_buffer() { return editor_p->buffer(); }

	// Gives real filename, or "Untitled.mup" if there is none
	const char * effective_filename();

	// This class needs access to Main class and its text editor.
	// Creator should call these to point to the proper instances 
	void set_editor(Fl_Text_Editor * ed_p);
	void set_parent(Main * main_p);
	Fl_Text_Editor * get_editor(void) { return editor_p; }
	Main * get_main(void) { return parent_window_p; }

	// These show the dialog and return the file name the user enters.
	const char * open_ask_user(void);
	const char * save_as_ask_user(void);

	// Ask user if they want to save the currently unsaved changes.
	Save_confirm_dialog::Answer save_changes_check(const char * extra_text = "",
						bool hide_the_No = false);

#ifndef OS_LIKE_WIN32
	// Make icon for Mup files.
	static void add_mup_icon(void);
#endif


private:
	// Callbacks
	void New(void);
	void Open(void);
	void NewFromTemplate(void);
	void Save(bool honor_auto_display = true);
	void SaveAs(bool honor_auto_display = true);
	void Exit(void);

	// Write out the current file
	void save_file(bool honor_auto_display);

	// Put current file name in window label
	void set_window_label(void);

	// Reset things for starting to edit a different file
	void begin_new_file(void);

	// The name of file being edited
	char * filename;

	// If buffer has changed since last being saved
	bool unsaved_changes;

	// Pointers to other class instances we need
	Fl_Text_Editor * editor_p;
	Main * parent_window_p;
};

#endif
