/*
 Copyright (c) 1995-2019  by Arkkra Enterprises.
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

// Code for the Config menu item on the main toolbar

#include "globals.H"
#include "Preferences.H"
#include "Config.H"
#include "Main.H"
#include "utils.H"
#include <FL/fl_ask.H>
#include <FL/Fl_Tooltip.H>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>


// Window to ask user where files and tools are located

FileLocations_dialog::FileLocations_dialog(void)
	: Fl_Double_Window(620, 335, "Mupmate File Locations")
{
	mup_documentation_p = new Fl_Input(200, 30, 400, 30, "Mup Documentation Folder");
	mup_documentation_p->tooltip("Set where Mup documentation\n"
				"files are installed on your system.\n"
				"This folder must contain the \"uguide\"\n"
				"folder that contains the HTML version\n"
				"of the Mup User's Guide.");

	mup_program_p = new Fl_Input(200, 65, 400, 30, "Mup Command Path");
	mup_program_p->tooltip("Set where the Mup program\n"
				"is installed on your system.");

	music_files_p = new Fl_Input(200, 100, 400, 30, "Folder for Mup Files");
	music_files_p->tooltip("Set the default folder for storing\n"
				"your Mup files (.mup input files,\n"
				"and .ps and .mid output files).");

	muppath_p = new Fl_Input(200, 135, 400, 30, "Folder for Mup include Files");
	static char include_tip_text[200];
	(void) sprintf(include_tip_text,
			"Set the default folder (or list of folders,\n"
			"separated by %c characters) for\n"
			"storing your Mup \"include\" files.",
			path_separator());
	muppath_p->tooltip(include_tip_text);

	viewer_p = new Fl_Input(200, 170, 400, 30, "PostScript Viewer Path");
	viewer_p->tooltip("Set which PostScript viewing program\n"
				"to use for displaying Mup output.\n"
#ifdef OS_LIKE_WIN32
				"This is typically GSview32.exe\n"
				"which you can obtain from\n"
				"http://www.cs.wisc.edu/~ghost/gsview/"
#else
				"The \"gv\" program is a common choice."
#endif
			);

	player_p = new Fl_Input(200, 205, 400, 30, "MIDI Player Path");
	player_p->tooltip("Set which MIDI player program\n"
				"to use for playing Mup MIDI output.\n"
#ifdef OS_LIKE_WIN32
				"This is typically wmplayer.exe"
#else
				"Common choices include xplaymidi or timidity."
#endif
			);

	ps_to_pdf_converter_p = new Fl_Input(200, 240, 400, 30, "PS to PDF Converter Path");
	ps_to_pdf_converter_p->tooltip("Set program to use for converting\n"
				"Mup output to PDF.\n"
				"Common choices are ps2pdf or pstopdf"
			);

	apply_p = new Fl_Return_Button(50, 290, 100, 30, "Save");
	apply_p->when(FL_WHEN_RELEASE);
	apply_p->callback(Save_cb, this);

	cancel_p = new Fl_Button(w() - 150, 290, 100, 30, "Cancel");
	cancel_p->shortcut(FL_Escape);
	cancel_p->when(FL_WHEN_RELEASE);
	cancel_p->callback(Cancel_cb, this);

	// Populate the fields
	set_current_values();

	// Arrange for destructor to clean up new-ed widgets
	end();

	// Arrange for window manager closes to do Cancel.
	callback(Cancel_cb, this);
	when(FL_WHEN_NEVER);
}


FileLocations_dialog::~FileLocations_dialog()
{
}


//--- Callback for when user clicks "Save" on FileLocations dialog.
// Save values in preferences file.

CALL_BACK(FileLocations_dialog, Save)
{
	bool changes = false;		// if any changes made
	bool error = false;		// if any errors found
	char location[FL_PATH_MAX];
	char expanded_path[FL_PATH_MAX];
	char * val;

	// Documentation location
	if (mup_documentation_p->size() > 0) {
		(void) Preferences_p->set(Mup_documentation_location,
						mup_documentation_p->value());
		changes = true;
		// Documentation being wrong means User's Guide can't be
		// shown, which is bad, although not fatal.
		filename_expand(expanded_path, mup_documentation_p->value());
		if ( ! fl_filename_isdir(expanded_path) ) {
			fl_alert("Location for Mup documentation is not a valid folder.");
			error = true;
		}
		else {
			if (access(users_guide_index_file(
					mup_documentation_p->value()), F_OK)
					!= 0) {
				fl_alert("Folder specified for Mup documentation is not correct:\n"
					"it does not contain the Mup User's Guide.");
				error = true;
			}
		}
	}
	else {
		(void) Preferences_p->get(Mup_documentation_location, val,
					Default_Mup_documentation_location);
		mup_documentation_p->value(val);
		fl_message("The Mup documentation location value was empty.\nIt has been restored to its previous value.");
		changes = true;
	}

	// Location of Mup program
	if (mup_program_p->size() > 0) {
		if (find_executable(mup_program_p->value(), location)) {
			(void) Preferences_p->set(Mup_program_location,
						mup_program_p->value());
			changes = true;
		}
		else {
			fl_alert("Location specified for Mup program is not valid.");
			error = true;
		}
	}
	else {
		(void) Preferences_p->get(Mup_program_location, val,
					Default_Mup_program_location);
		mup_program_p->value(val);
		fl_message("The Mup program location value was empty.\nIt has been restored to its previous value.");
		changes = true;
	}

	// Default folder for Mup input files
	if (music_files_p->size() > 0) {
		filename_expand(expanded_path, music_files_p->value());
		if (chdir(expanded_path) != 0) {
			fl_alert("Value for \"Folder for Mup Files\" is not a valid folder.");
			error = true;
		}
		else {
			(void) Preferences_p->set(Music_files_location,
						music_files_p->value());
			changes = true;
		}
	}

	// $MUPPATH value
	if (muppath_p->size() > 0) {
		(void) Preferences_p->set(MUPPATH_location, muppath_p->value());
		// Set $MUPPATH
		set_muppath(muppath_p->value());
		changes = true;
		// Setting MUPPATH correctly is only important if user
		// actually uses it, which many people won't, but if it is set
		// to something invalid, we give a warning.
		// Since MUPPATH can be a list, we check each component
		// in the list.
		char pathcopy[muppath_p->size() + 1];
		(void) strcpy(pathcopy, muppath_p->value());
		char * component_p = pathcopy;
		char * sep_p;	// where path separator appears in list
		do {
			if ((sep_p = strchr(component_p, path_separator())) != 0) {
				*sep_p = '\0';
			}
			filename_expand(expanded_path, component_p);
			if (strlen(expanded_path) > 0 &&
					! fl_filename_isdir(expanded_path)) {
				fl_alert("Location for Mup include files\n"
					"\"%s\"\nis not a valid folder.",
					component_p);
				error = true;
			}
			component_p += strlen(component_p) + 1;
		} while (sep_p != 0);
	}

	// PostScript viewer program
	if (viewer_p->size() > 0) {
		if (find_executable(viewer_p->value(), location)) {
			(void) Preferences_p->set(Viewer_location, viewer_p->value());
			changes = true;
		}
		else {
			fl_alert("Location specified for PostScript viewer is not valid.");
			error = true;
		}
	}
	else {
		(void) Preferences_p->get(Viewer_location, val,
					Default_viewer_location);
		viewer_p->value(val);
		fl_message("The PostScript viewer value was empty.\nIt has been restored to its previous value.");
		changes = true;
	}

	// MIDI player
	if (player_p->size() > 0) {
		if (find_executable(player_p->value(), location)) {
			(void) Preferences_p->set(MIDI_player_location, player_p->value());
			changes = true;
		}
		else {
			fl_alert("Location specified for MIDI player is not valid.");
			error =  true;
		}
	}
	else {
		(void) Preferences_p->get(MIDI_player_location, val,
					Default_MIDI_player_location);
		player_p->value(val);
		fl_message("The MIDI player value was empty.\nIt has been restored to its previous value.");
		changes = true;
	}

	if (ps_to_pdf_converter_p->size() > 0) {
		if (find_executable(ps_to_pdf_converter_p->value(), location)) {
			(void) Preferences_p->set(PS_to_PDF_converter_location, ps_to_pdf_converter_p->value());
			changes = true;
		}
		else {
			fl_alert("Location specified for PostScript to PDF converter is not valid.");
			error =  true;
		}
	}
	else {
		(void) Preferences_p->get(PS_to_PDF_converter_location, val,
					Default_PS_to_PDF_converter_location);
		ps_to_pdf_converter_p->value(val);
		fl_message("The PostScript to PDF converter value was empty.\nIt has been restored to its previous value.");
		changes = true;
	}

	// If any changes, persist the data.
	if (changes) {
		Preferences_p->flush();
	}

	// If there were errors, leave form up so user can try to correct them.
	if ( ! error ) {
		hide();
	}
}


//--- callback for when user clicks "Cancel" on FileLocations dialog

CALL_BACK(FileLocations_dialog, Cancel)
{
	hide();
	// Put all the original settings back on the form
	set_current_values();
}	


// Populate form with the current default values from user's preferences.

void
FileLocations_dialog::set_current_values(void)
{
	char * val;
	(void) Preferences_p->get(Mup_documentation_location, val,
					Default_Mup_documentation_location);
	mup_documentation_p->value(val);

	(void) Preferences_p->get(Mup_program_location, val,
					Default_Mup_program_location);
	mup_program_p->value(val);

	(void) Preferences_p->get(Music_files_location, val,
					Default_music_files_location);
	music_files_p->value(val);
	(void) Preferences_p->get(MUPPATH_location, val,
					Default_MUPPATH_location);
	muppath_p->value(val);
	(void) Preferences_p->get(Viewer_location, val,
					Default_viewer_location);
	viewer_p->value(val);
	(void) Preferences_p->get(MIDI_player_location, val,
					Default_MIDI_player_location);
	player_p->value(val);
	(void) Preferences_p->get(PS_to_PDF_converter_location, val,
					Default_PS_to_PDF_converter_location);
	ps_to_pdf_converter_p->value(val);
}


//-----------------------------------------------------------------

// List of standard FLTK fonts, and info to map name to menu entry.
static struct Font_info {
	const char * name;
	Fl_Font	value;
	int menu_offset;
} Fontlist[] = {
	{ "Courier",			FL_COURIER },
	{ "Courier Bold",		FL_COURIER_BOLD },
	{ "Courier Italic",		FL_COURIER_ITALIC },
	{ "Courier Bold Italic",	FL_COURIER_BOLD_ITALIC },
	{ "Helvetica",			FL_HELVETICA },
	{ "Helvetica Bold",		FL_HELVETICA_BOLD },
	{ "Helvetica Italic",		FL_HELVETICA_ITALIC },
	{ "Helvetica Bold Italic",	FL_HELVETICA_BOLD_ITALIC },
	{ "Times",			FL_TIMES },
	{ "Times Bold",			FL_TIMES_BOLD },
	{ "Times Italic",		FL_TIMES_ITALIC },
	{ "Times Bold Italic",		FL_TIMES_BOLD_ITALIC },
};
static const int Fontlistlength = sizeof(Fontlist) / sizeof(Fontlist[0]);

// Window to ask user preferences, like editor font, size, etc.

Preferences_dialog::Preferences_dialog(void)
	: Fl_Double_Window(400, 280, "Mupmate Preferences")
{
	// Make widget for user's editor font choice.
	font_p = new Fl_Choice(20, 40, 210, 30, "Text Font");
	font_p->tooltip("Select the font to be used\n"
			"in the editor window where you\n"
			"type in Mup input. It is also used\n"
			"for the Help and error report text.");
	// Arrange to reset size menu if font selection changes
	font_p->callback(fontchg_cb, this);
	font_p->when(FL_WHEN_CHANGED);
	font_p->align(FL_ALIGN_TOP_LEFT);

	// Make widget for user's editor size choice.
	size_p = new Fl_Choice(270, 40, 100, 30, "Text Size");
	size_p->tooltip("Select the text size to be used\n"
			"in the editor window where you\n"
			"type in Mup input. It is also used\n"
			"for the Help and error report text.");
	size_p->align(FL_ALIGN_TOP_LEFT);

	auto_display_p = new Fl_Check_Button(20, 90, 180, 30,
						"Auto-Display on Save");
	auto_display_p->tooltip("Set whether your music\n"
			"is displayed automatically\n"
			"whenever you save your Mup file.");

	auto_save_p = new Fl_Check_Button(w() - 170, 90, 150, 30,
						"Auto-Save on Run");
	auto_save_p->tooltip("Set whether your music is saved\n"
			"automatically whenever you do Display, Play,\n"
			"Write PostScript or Write MIDI from the Run menu.");

	tooltips_delay_p = new Fl_Value_Input(150, 155, 100, 30, "Tool Tip Delay");
	tooltips_delay_p->minimum(0.0);
	tooltips_delay_p->precision(3);
	tooltips_delay_p->tooltip("Set how long to delay before showing\n"
			"tool tips, in seconds.\n");
	tooltips_delay_p->align(FL_ALIGN_TOP_LEFT);

	// Create and configure widget for Save button
	apply_p = new Fl_Return_Button(60, 215, 100, 30, "Save");
	apply_p->when(FL_WHEN_RELEASE);
	apply_p->callback(Save_cb, this);

	// Create and configure widget for Cancel button
	cancel_p = new Fl_Button(w() - 160, 215, 100, 30, "Cancel");
	cancel_p->shortcut(FL_Escape);
	cancel_p->when(FL_WHEN_RELEASE);
	cancel_p->callback(Cancel_cb, this);

	// Populate the fields
	set_current_values();

	// Arrange for destructor to clean up new-ed widgets
	end();

	// Arrange for window manager closes to do Cancel.
	callback(Cancel_cb, this);
	when(FL_WHEN_NEVER);
}

Preferences_dialog::~Preferences_dialog()
{
}


//---- Callback for when user changes font selection.
// This re-creates the size menu to be what sizes are available
// for that font, since each font could have a different set of sizes.

CALL_BACK(Preferences_dialog, fontchg)
{
	unsigned char size;
	if (size_p->mvalue() != 0) {
		size = atoi(size_p->mvalue()->text);
	}
	else {
		// Shouldn't really be possible to get here,
		// but better to be safe.
		size = (unsigned char) atoi(Default_editor_size);
	}

	set_size_list(Config::fontvalue(font_p->mvalue()->text), size);
}


//--- Callback for when user clicks Save in Preferences
// Save the new values.

CALL_BACK(Preferences_dialog, Save)
{
	Fl_Font font;
	int n;

	Preferences_p->set(Auto_display_preference, auto_display_p->value());
	Preferences_p->set(Auto_save_preference, auto_save_p->value());
	Preferences_p->set(Tooltips_delay_preference, tooltips_delay_p->value());
	Fl_Tooltip::delay(tooltips_delay_p->value());

	// Convert font menu selection into font value.
	for (n = 0; n < Fontlistlength; n++) {
		if (Fontlist[n].menu_offset == font_p->value()) {
			Preferences_p->set(Editor_font_preference, Fontlist[n].name);
			font = Fontlist[n].value;
			break;
		}
	}
	if (n >= Fontlistlength) {
		// Selection not valid. Fall back to using the default.
		char * fontname;
		(void) Preferences_p->get(Editor_font_preference, fontname,
							Default_editor_font);
		font = Config::fontvalue(fontname);
	}

	// Save size value.
	unsigned char size;
	if (size_p->text() != 0) {
		(void) Preferences_p->set(Editor_size_preference, size_p->text());
		size = (unsigned char) atoi(size_p->text());
	}
	else {
		size = (unsigned char) atoi(Default_editor_size);
	}

	// Persist the data.
	Preferences_p->flush();

	// Actually change the font/size in all relevant windows.
	// Windows that want to know about these changes register a callback,
	// so we call them.
	Font_change_registration::run_callbacks(font, size);

	hide();
}

//--- callback for when user clicks Cancel in Preferences

CALL_BACK(Preferences_dialog, Cancel)
{
	hide();
	// Put all the original settings back on the form
	set_current_values();
}


// Populate form with current values from user's preferences

void
Preferences_dialog::set_current_values(void)
{
	int auto_display;
	(void) Preferences_p->get(Auto_display_preference, auto_display,
						Default_auto_display);
	auto_display_p->value(auto_display);

	int auto_save;
	(void) Preferences_p->get(Auto_save_preference, auto_save,
						Default_auto_save);
	auto_save_p->value(auto_save);

	double tooltips_delay;
	(void) Preferences_p->get(Tooltips_delay_preference, tooltips_delay,
						Default_tooltips_delay);
	tooltips_delay_p->value(tooltips_delay);

	char * fontname;
	(void) Preferences_p->get(Editor_font_preference, fontname,
						Default_editor_font);
	Fl_Font font = Config::fontvalue(fontname);
	// Populate font menu
	font_p->clear();
	for (int i = 0; i < Fontlistlength; i++) {
		Fontlist[i].menu_offset =
			font_p->add(Fontlist[i].name, 0, 0, 0, 0);
		// Set the current value
		if (Fontlist[i].value == font) {
			font_p->value(Fontlist[i].menu_offset);
		}
	}

	char * sizename;
	(void) Preferences_p->get(Editor_size_preference, sizename,
						Default_editor_size);
	unsigned char size = (unsigned char) atoi(sizename);
	// Populate the size menu
	set_size_list(font, size);
}


// When font selection changes, re-create the size menu,
// because each font could have different sizes available.

void
Preferences_dialog::set_size_list(Fl_Font font, uchar curr_size)
{
	// Avoid really tiny sizes, or more importantly, zero, like if an atoi
	// failed, because otherwise FLTK may try to divide by zero.
	// Also limit to a maximum size.
	if (curr_size < Min_size || curr_size > Max_size) {
		curr_size = (unsigned char) atoi(Default_editor_size);
	}

	// Clean out the current menu if any
	size_p->clear();
	
	// Populate the menu
	int * sizelist;
	int numsizes = Fl::get_font_sizes(font, sizelist);
	
	// Set current value to ridiculous value, then find closest
	int currvalue = 5000;

	int i;	// index through sizelist
	int menu_index;	// index into menu
	for (i = menu_index = 0; i < numsizes; i++) {
		if (sizelist[i] == 0) {
			// This means font is scaleable 
			continue;
		}
		if (sizelist[i] > Max_size) {
			break;
		}
		char num_as_string[4];
		(void) sprintf(num_as_string, "%d", sizelist[i]);
		size_p->add(num_as_string, 0, 0, 0, 0);
		// If this is closest index to desired size, mark as current
		if ( abs(sizelist[i] - currvalue) > abs(sizelist[i] - curr_size) ) {
			currvalue = sizelist[i];
			size_p->value(menu_index);
		}
		menu_index++;
	}
	if (numsizes == 0 || (numsizes == 1 && sizelist[0] == 0)) {
		// Either no available sizes at all, or only
		// scaleable, with no special "good" sizes,
		// so we pick some and hope for the best.
		size_p->add("10", 0, 0, 0, 0);
		if (curr_size <= 11) {
			size_p->value(0);
		}
		size_p->add("12", 0, 0, 0, 0);
		if (curr_size >= 12 && curr_size <= 13) {
			size_p->value(1);
		}
		size_p->add("14", 0, 0, 0, 0);
		if (curr_size >= 14 && curr_size <= 15) {
			size_p->value(2);
		}
		size_p->add("16", 0, 0, 0, 0);
		if (curr_size >= 16 && curr_size <= 17) {
			size_p->value(3);
		}
		size_p->add("18", 0, 0, 0, 0);
		if (curr_size >= 18) {
			size_p->value(4);
		}
	}
}



//-------the Config menu item on main toolbar-------------------------------

Config::Config()
{
	locations_p = 0;
	preferences_p = 0;
}

Config::~Config()
{
	if (locations_p != 0) {
		delete locations_p;
		locations_p = 0;
	}
	if (preferences_p != 0) {
		delete preferences_p;
		preferences_p = 0;
	}
}


// Bring up the dialog for "File Locations" menu item

CALL_BACK(Config, FileLocations)
{
	if (locations_p == 0) {
		// first time, create widget
		locations_p = new FileLocations_dialog();
	}
	locations_p->show();
}


// Bring up the dialog for "Preferences" menu item

CALL_BACK(Config, Preferences)
{
	if (preferences_p == 0) {
		// first time, create widget
		preferences_p = new Preferences_dialog();
	}
	preferences_p->show();
}



// Translate font name to FL_Font value.

Fl_Font
Config::fontvalue(const char * fontname)
{
	int n;
	// Linear search of the list (it is short).
	for (n = 0; n < Fontlistlength; n++) {
		if (strcmp(Fontlist[n].name, fontname) == 0) {
			return(Fontlist[n].value);
		}
	}
	// Hmmm. Not found. Should not happen. Hunt for default
	for (n = 0; n < Fontlistlength; n++) {
		if (strcmp(Fontlist[n].name, Default_editor_font) == 0) {
			return(Fontlist[n].value);
		}
	}
	// Wow. Can't find default either. Punt.
	return(FL_COURIER);
}


//--------------------- class that lets other classes register a callback
// to be called for changes in font/size


// List of callbacks for when font/size change
Font_change_registration * Font_change_registration::list_p = 0;

Font_change_registration::Font_change_registration(Font_change_callback func, void * arg)
{
	// Save callback information.
	callback = func;
	callback_arg = arg;

	// Add to list of callbacks.
	next = list_p;
	list_p = this;

	// Set the font and size on this newly registered widget.
	// Look up the current values and call the newly registered callback.
	char * fontstr;
	(void) Preferences_p->get(Editor_font_preference, fontstr,
							Default_editor_font);
	Fl_Font font = Config::fontvalue(fontstr);

	char * sizestr;
	(void) Preferences_p->get(Editor_size_preference, sizestr,
							Default_editor_size);
	unsigned char size = (unsigned char) atoi(sizestr);
	if (size < Min_size) {
		size = (unsigned char) atoi(Default_editor_size);
	}
	(*func)(arg, font, size);
}

Font_change_registration::~Font_change_registration(void)
{
	// Remove callback from linked list
	if (list_p == this) {
		list_p = next;
	}
	else {
		Font_change_registration * fcr_p;
		for (fcr_p = list_p; fcr_p != 0; fcr_p = fcr_p->next) {
			if (fcr_p->next == this) {
				fcr_p->next = next;
				return;
			}
		}
	}
}


// Notify all classes that want to know about font/size changes,
// by calling the callback function they registered.

void
Font_change_registration::run_callbacks(Fl_Font font, unsigned char size)
{
	// Avoid unreadably small sizes and division by zero if
	// earlier atoi() of size failed due to bad data
	// (e.g., user hand-editing the preference file)
	if (size < Min_size || size > Max_size) {
		size = (unsigned char) atoi(Default_editor_size);
	}

	// Walk through list of registered callbacks, calling each.
	Font_change_registration * fcr_p;
	for (fcr_p = list_p; fcr_p != 0; fcr_p = fcr_p->next) {
		(*(fcr_p->callback))(fcr_p->callback_arg, font, size);
	}
}
