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

// Code for the File menu off the main toolbar

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#ifndef OS_LIKE_WIN32
#include <FL/Fl_File_Icon.H>
#endif
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_Input.H>
#include <FL/filename.H>
#include <FL/Fl_Bitmap.H>
#include <FL/Fl_File_Chooser.H>
#include "globals.H"
#include "File.H"
#include "Main.H"
#include "Preferences.H"
#include "utils.H"
#include <time.h>
#ifdef OS_LIKE_WIN32
#include <FL/x.H>
#include <commdlg.h>
#endif

// The file name filters
#define Mup_filter "*.mup"
#define All_filter "*.*"

extern char dir_separator();
extern const char * const template_text;


//-----------------Class to implement the File menu off the main menu bar

File::File()
{
	filename = 0;
	unsaved_changes = false;
}

File::~File()
{
	if (filename) {
		delete filename;
		filename = 0;
	}
}


//--- the "New" menu item -------------

CALL_BACK(File, New)
{
	// Ask user if they want to save any unsaved change on
	// current file first.
	switch (save_changes_check()) {
	default: // default case should be impossible
	case Save_confirm_dialog::Cancel:
		return;
	case Save_confirm_dialog::No:
		break;
	case Save_confirm_dialog::Yes:
		Save(false);
		break;
	}

	// Clear the current edit buffer
	Fl_Text_Buffer * buffer_p = editor_p->buffer();
	buffer_p->replace(0, buffer_p->length(), "");

	if (filename) {
		delete filename;
		filename = 0;
	}
	set_window_label();
	// Reset state information
	begin_new_file();
}


// --- the "New From Template" menu item

CALL_BACK(File, NewFromTemplate)
{
	New();
	editor_p->buffer()->append(template_text);
	unsaved_changes = false;
}

//--- the "Open" menu item -------------

CALL_BACK(File, Open)
{
	// If already editing a file, ask user if they want to
	// save any unsaved changes first
	switch (save_changes_check()) {
	default: // default case should be impossible
	case Save_confirm_dialog::Cancel:
		return;
	case Save_confirm_dialog::No:
		break;
	case Save_confirm_dialog::Yes:
		Save(false);
		break;
	}

	// Clear out label to "Untitled" in case the load fails
	set_window_label();

	// Ask user for filename. If they give one, load it into editor.
	const char * newfile = open_ask_user();
	if (newfile != 0 && newfile[0] != '\0') {
		load_file(newfile);
	}
}


//--- the "Save" menu item -------------

// Save the current buffer contents.
// If honor_auto_display is true and user has requested auto display,
// do the Run>Display action. If the save was due to finishing up an old
// file to start on a new or due to saving before exiting, we don't do that;
// it is only done if user did an explicit Save or Save As.

CALL_BACK_A(File, Save, bool honor_auto_display)
{
	if (filename == 0) {
		// No file name given yet, so change into a Save As
		SaveAs(honor_auto_display);
	}
	else {
		save_file(honor_auto_display);
	}
}


//--- the "SaveAs" menu item -------------

CALL_BACK_A(File, SaveAs, bool honor_auto_display)
{
	// Ask user for name of file to save to
	const char * newfile = save_as_ask_user();
	if (newfile != 0 && newfile[0] != '\0') {

		// If user didn't give a suffix, add .mup suffix.
		// If they used .ps or .mid or .err suffix, don't allow that.
		const char * suffix = fl_filename_ext(newfile);
		char * suffixed_filename = 0;
		if (*suffix == '\0') {
			// User did not supply a suffix, so we add .mup
			size_t sfleng = strlen(newfile) + 5;
			suffixed_filename = new char[sfleng];
			(void) snprintf(suffixed_filename, sfleng, "%s.mup", newfile);
			newfile = suffixed_filename;
		}
		else if (strcasecmp(suffix, ".ps") == 0
					|| strcasecmp(suffix, ".mid") == 0
					|| strcasecmp(suffix, ".err") == 0) {
			fl_alert("A filename extension of .ps .mid or .err\n"
				"is not allowed for Mup input files,\n"
				"since those extensions are used for\n"
				"PostScript, MIDI, and error output files.");
			return;
		}

		if (access(newfile, F_OK) == 0) {
			const char * ask_replace = " already exists. Do you want to replace it?";
			char question[strlen(newfile) + strlen(ask_replace) + 1];
			(void) snprintf(question, sizeof(question),
						"%s%s\n", newfile, ask_replace);
			switch (Save_confirm_dialog::confirm_save(question)) {
			default: // default case should be impossible.
			case Save_confirm_dialog::Cancel:
			case Save_confirm_dialog::No:
				if (suffixed_filename != 0) {
					delete suffixed_filename;
				}
				return;
			case Save_confirm_dialog::Yes:
				break;
			}
		}

		// Save the name of the new file
		if (filename != 0) {
			// forget previous file name
			delete filename;
		}
		if (suffixed_filename != 0) {
			filename = suffixed_filename;
		}
		else {
			filename = new char[strlen(newfile) + 1];
			(void) strcpy(filename, newfile);
		}

		set_window_label();
		save_file(honor_auto_display);
	}
}


//--- the "Exit" menu item -------------

CALL_BACK(File, Exit)
{
	switch (save_changes_check()) {
	default: // default case should be impossible
	case Save_confirm_dialog::Cancel:
		return;
	case Save_confirm_dialog::No:
		break;
	case Save_confirm_dialog::Yes:
		Save(false);
		break;
	}
	Main::clean_exit();
}

//------------------------------------------------------------------------

// This class needs access to editor window, so whoever creates an instance
// of this class needs to call this function to give it access.

void
File::set_editor(Fl_Text_Editor * ed)
{
	editor_p = ed;
}

 
// We need to set parent's label, etc. Save info about parent.

void
File::set_parent(Main * win_p)
{
	parent_window_p = win_p;
}

// Called when user makes a change in editor window.
// Let's us know if we need to prompt user about unsaved changes.

void
File::modify_cb(int, int num_inserted, int num_deleted, int, const char *, void * data)
{
	if (num_inserted > 0 || num_deleted > 0) {
		((File *)data)->unsaved_changes = true;
	}
}


// Utility method that does the details of saving the current file

void
File::save_file(bool honor_auto_display)
{
	if (editor_p->buffer()->savefile(filename) != 0) {
		fl_alert("failed to save file %s", filename);
	}
	else {
		// All unsaved changes have now been saved
		unsaved_changes = false;

		// If user wants auto-display on save, do that
		if (honor_auto_display) {
			int auto_display;
			(void) Preferences_p->get(Auto_display_preference, auto_display,
							Default_auto_display);
			if (auto_display) {
				parent_window_p->runmenu_p->Display();
			}
		}
	}
}


// Utility method to ask user if they want to save changes.
// Returns:
//	Cancel	don't do anything
//	No	do action without saving changes
//	Yes 	save changes before doing action
// The extra_text argument allows caller to enhance the question
// asked of the user. The "No" button can be hidden.

Save_confirm_dialog::Answer
File::save_changes_check(const char * extra_text, bool hide_the_No)
{
	if ( ! unsaved_changes ) {
		return Save_confirm_dialog::No;
	}
	const char * name = effective_filename();
	char question[200 + strlen(name) + strlen(extra_text)];
	(void) snprintf(question, sizeof(question),
			"The text in the %s file has changed. "
			"Do you want to save the changes? %s", name, extra_text);
	return(Save_confirm_dialog::confirm_save(question, hide_the_No));
}


// Read the given file into the editor window.

void
File::load_file(const char * name)
{
	// Free up existing file name, if any
	if (filename != 0) {
		delete filename;
		filename = 0;
	}

	// If name supplied doesn't have .mup suffix,
	// try again with that suffix added.
	char * newname = 0;
	if (access(name, F_OK) != 0) {
		if (strlen(name) < 5 ||
				strcasecmp(name + strlen(name) - 4, ".mup") != 0) {
			size_t nnleng = strlen(name) + 5;
			newname = new char[nnleng];
			(void) snprintf(newname, nnleng, "%s.mup", name);
		}
	}
	if (newname == 0) {
		newname = new char[strlen(name) + 1];
		(void) strcpy(newname, name);
	}

	// Mup files are typically only a few Kbytes,
	// so we probably don't need the default 128 K buffer,
	// but memory is cheap enough these days, so just go with that.
	if (editor_p->buffer()->loadfile(newname) != 0) {
		fl_alert("Unable to load file \"%s\"", newname);
		delete newname;
	}
	else {
		filename = newname;
	}

	// Reset state information for a new file
	begin_new_file();

	// add file name to window label
	set_window_label();
}

void
File::set_window_label()
{
	if (parent_window_p == 0) {
		return;
	}
	const char * name = effective_filename();
	char label[strlen(name) + 11];
	(void) snprintf(label, sizeof(label), "%s - Mupmate", name);
	parent_window_p->copy_label(label);
}


const char *
File::effective_filename(void)
{
	return(filename == 0 ? "Untitled.mup" : filename);
}

void
File::begin_new_file(void)
{
	parent_window_p->begin_new_file();
	unsaved_changes = false;
}

#ifndef OS_LIKE_WIN32
// Add Icon for Mup files.

// Figure out note polygon one time and just add these offsets for the other...
#define RIGHT_NOTE_X	5800
#define RIGHT_NOTE_Y	-1200

static short Mup_icon_data[] = {
	Fl_File_Icon::COLOR,	0, FL_RED,

	// left stem
	Fl_File_Icon::POLYGON,
	Fl_File_Icon::VERTEX, 600, 1200, 
	Fl_File_Icon::VERTEX, 600, 8200,
	Fl_File_Icon::VERTEX, 1200, 8200,
	Fl_File_Icon::VERTEX, 1200, 1200,
	Fl_File_Icon::VERTEX, 600, 1200,
	Fl_File_Icon::END,

	// right stem
	Fl_File_Icon::POLYGON,
	Fl_File_Icon::VERTEX, 6400, 600, 
	Fl_File_Icon::VERTEX, 6400, 7000,
	Fl_File_Icon::VERTEX, 7000, 7000,
	Fl_File_Icon::VERTEX, 7000, 600,
	Fl_File_Icon::VERTEX, 6400, 600,
	Fl_File_Icon::END,

	// beam
	Fl_File_Icon::POLYGON,
	Fl_File_Icon::VERTEX, 600, 1200, 
	Fl_File_Icon::VERTEX, 600, 2700,
	Fl_File_Icon::VERTEX, 7000, 2100,
	Fl_File_Icon::VERTEX, 7000, 600,
	Fl_File_Icon::VERTEX, 600, 2000,
	Fl_File_Icon::END,

	// left note
	Fl_File_Icon::POLYGON,
	Fl_File_Icon::VERTEX, 600, 8200, 
	Fl_File_Icon::VERTEX, 1600, 9400,
	Fl_File_Icon::VERTEX, 3200, 9400,
	Fl_File_Icon::VERTEX, 3800, 8800,
	Fl_File_Icon::VERTEX, 3800, 8000,
	Fl_File_Icon::VERTEX, 3000, 7200,
	Fl_File_Icon::VERTEX, 1600, 7200,
	Fl_File_Icon::VERTEX, 800, 7900,
	Fl_File_Icon::VERTEX, 800, 8200,
	Fl_File_Icon::VERTEX, 500, 9000,
	Fl_File_Icon::END,

	// right note
	Fl_File_Icon::POLYGON,
	Fl_File_Icon::VERTEX, 600 + RIGHT_NOTE_X, 8200 + RIGHT_NOTE_Y, 
	Fl_File_Icon::VERTEX, 1600 + RIGHT_NOTE_X, 9400 + RIGHT_NOTE_Y,
	Fl_File_Icon::VERTEX, 3200 + RIGHT_NOTE_X, 9400 + RIGHT_NOTE_Y,
	Fl_File_Icon::VERTEX, 3800 + RIGHT_NOTE_X, 8800 + RIGHT_NOTE_Y,
	Fl_File_Icon::VERTEX, 3800 + RIGHT_NOTE_X, 8000 + RIGHT_NOTE_Y,
	Fl_File_Icon::VERTEX, 3000 + RIGHT_NOTE_X, 7200 + RIGHT_NOTE_Y,
	Fl_File_Icon::VERTEX, 1600 + RIGHT_NOTE_X, 7200 + RIGHT_NOTE_Y,
	Fl_File_Icon::VERTEX, 800 + RIGHT_NOTE_X, 7900 + RIGHT_NOTE_Y,
	Fl_File_Icon::VERTEX, 800 + RIGHT_NOTE_X, 8200 + RIGHT_NOTE_Y,
	Fl_File_Icon::VERTEX, 500 + RIGHT_NOTE_X, 9000 + RIGHT_NOTE_Y,
	Fl_File_Icon::END,

	Fl_File_Icon::END
};

void
File::add_mup_icon(void)
{
	new Fl_File_Icon("*.mup", Fl_File_Icon::PLAIN,
			sizeof(Mup_icon_data) / sizeof(Mup_icon_data[0]),
			Mup_icon_data);
}
#endif



// Ask user for name of file to open, and return their choice

const char *
File::open_ask_user(void)
{
#ifdef OS_LIKE_WIN32
	OPENFILENAME openfilename;
	static CHAR path[FL_PATH_MAX] = "*.mup";
	CHAR dir[FL_PATH_MAX];
	memset(&openfilename, 0, sizeof(openfilename));
	GetCurrentDirectory(sizeof(dir), dir);
	openfilename.lStructSize = sizeof(openfilename);
	parent_window_p->make_current();
	openfilename.hwndOwner = fl_window;
	openfilename.hInstance = fl_display;
	openfilename.lpstrFilter = "Mup files (*.mup)\0*.mup\0All files (*.*)\0*.*\0";
	openfilename.lpstrFile = path;
	openfilename.nMaxFile = sizeof(path);
	openfilename.lpstrInitialDir = dir;
	openfilename.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	openfilename.lpstrDefExt = ".mup";

	if (GetOpenFileName(&openfilename)) {
		return(path);
	}
	else {
		return(0);
	}

#else
	fl_file_chooser_ok_label("Open");
	return (fl_file_chooser("Open Mup file", "Mup files (*.mup)\tAll files (*)", "*.mup"));
#endif
}


// Ask user for name of file to save to, and return their choice

const char *
File::save_as_ask_user(void)
{
#ifdef OS_LIKE_WIN32
	OPENFILENAME openfilename;
	static CHAR path[FL_PATH_MAX] = "*.mup";
	CHAR dir[FL_PATH_MAX];
	memset(&openfilename, 0, sizeof(openfilename));
	GetCurrentDirectory(sizeof(dir), dir);
	openfilename.lStructSize = sizeof(openfilename);
	parent_window_p->make_current();
	openfilename.hwndOwner = fl_window;
	openfilename.hInstance = fl_display;
	openfilename.lpstrFilter = "Mup files (*.mup)\0*.mup\0All files (*.*)\0*.*\0";
	openfilename.lpstrFile = path;
	openfilename.nMaxFile = sizeof(path);
	openfilename.lpstrInitialDir = dir;
	openfilename.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	openfilename.lpstrDefExt = ".mup";
	if (GetSaveFileName(&openfilename)) {
		return(path);
	}
	else {
		return(0);
	}
#else
	fl_file_chooser_ok_label("Save");
	return (fl_file_chooser("Save Mup file", "Mup files (*.mup)\tAll files (*)", "*.mup"));
#endif
}


//---------------------------------------------------------------------
// The fl_choice function has the middle button as the default,
// but we want the left button (Yes) to be the default for whether
// to save before exiting, so we have a special class for it.
// It is based on the fl_choice code.

Save_confirm_dialog::Save_confirm_dialog(const char * text)
	: Fl_Double_Window(500, 130, "Confirm")
{
	// Make question mark "icon"
	icon_p = new Fl_Box(10, 10, 50, 50);
	icon_p->box(FL_THIN_UP_BOX);
	icon_p->labelfont(FL_TIMES_BOLD);
	icon_p->labelsize(30);
	icon_p->color(FL_WHITE);
	icon_p->labelcolor(FL_BLUE);
	icon_p->label("?");

	// Print the question
	message_p = new Fl_Box(90, 20, 400, 40);
	message_p->label(text);
	message_p->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_WRAP);

	yes_p = new Fl_Return_Button(210, h() - 50, 70, 30, "Yes");

	no_p = new Fl_Button(300, h() - 50, 70, 30, "No");

	cancel_p = new Fl_Button(390, h() - 50, 70, 30, "Cancel");
	cancel_p->shortcut(FL_Escape);
	show();

	// Arrange for destructor to free new-ed widgets
	end();
}

Save_confirm_dialog::~Save_confirm_dialog()
{
}


// Method to bring up dialog to ask user if they want to save changes.
// Returns:
//	Cancel 	don't do anything
//	No	do action without saving changes
//	Yes	save changes before doing action

Save_confirm_dialog::Answer
Save_confirm_dialog::confirm_save(const char * text, bool hide_the_No)
{
	// Create dialog window
	Save_confirm_dialog * confirm_p = new Save_confirm_dialog(text);

	// Only show desired buttons
	if (hide_the_No) {
		confirm_p->no_p->hide();
	}

	// Wait for user to select a button
	Answer ret;
	for ( ; ; ) {
		Fl_Widget *widget_p = Fl::readqueue();
		if (widget_p == 0) {
			Fl::wait();
		}
		else if (widget_p == confirm_p->cancel_p ||
				widget_p == confirm_p) {
			ret = Cancel;
			break;
		}
		else if (widget_p == confirm_p->no_p) {
			ret = No;
			break;
		}
		else if (widget_p == confirm_p->yes_p) {
			ret = Yes;
			break;
		}
	}

	// Clean up the dialog window
	confirm_p->hide();
	delete confirm_p;

	return ret;
}
