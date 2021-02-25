/*
 Copyright (c) 1995-2021  by Arkkra Enterprises.
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

#ifndef _CONFIG_H_
#define _CONFIG_H_

// Classes for Config menu item off of main toolbar

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Value_Input.H>
#include "utils.H"


// Class for window that asks user for file locations

class FileLocations_dialog : Fl_Double_Window {

friend class Config;

public:
	FileLocations_dialog(void);
	~FileLocations_dialog();

	// Callbacks
	static void Save_cb(Fl_Widget *, void *);
	static void Cancel_cb(Fl_Widget *, void *);

private:
	// Callbacks
	void Save(void);
	void Cancel(void);

	// Populuate fields with current values
	void set_current_values(void);

	// Widgets
	Fl_Input * mup_program_p;
	Fl_Input * mup_documentation_p;
	Fl_Input * music_files_p;
	Fl_Input * muppath_p;
	Fl_Input * viewer_p;
	Fl_Input * player_p;
	Fl_Input * ps_to_pdf_converter_p;
	Fl_Return_Button * apply_p;
	Fl_Button * cancel_p;
};


// Class for window that asks user for preferences, like editor font, size, etc.

class Preferences_dialog : Fl_Double_Window {

friend class Config;

public:
	Preferences_dialog(void);
	~Preferences_dialog();

	// Callbacks
	static void Save_cb(Fl_Widget *, void * data);
	static void Cancel_cb(Fl_Widget *, void * data);
	static void fontchg_cb(Fl_Widget *, void * data);

private:
	// Callbacks
	void fontchg(void);
	void Save(void);
	void Cancel(void);

	// Populate fields with current values
	void set_current_values(void);

	// Populate size menu as appropriate for the given font
	void set_size_list(Fl_Font font, uchar curr_size);

	// Widgets
	Fl_Choice * font_p;
	Fl_Choice * size_p;
	Fl_Check_Button * auto_display_p;
	Fl_Check_Button * auto_save_p;
	Fl_Value_Input * tooltips_delay_p;
	Fl_Return_Button * apply_p;
	Fl_Button * cancel_p;
};


// Class for the Config menu item on main toolbar

class Config {
public:
	Config();
	~Config();

	// Callbacks
	static void FileLocations_cb(Fl_Widget *, void *);
	static void Preferences_cb(Fl_Widget *, void *);

	// Convert font name to Fl_Font value
	static Fl_Font fontvalue(const char *);

private:
	// Callbacks
	void FileLocations(void);
	void Preferences(void);
	void RegistrationForm(void);
	void RegistrationKey(void);

	// Widgets
	FileLocations_dialog * locations_p;
	Preferences_dialog * preferences_p;
};


// Any class that wants to be notified of changes in user preference
// for font and size should register a callback function of this type
// using the Font_change_registration class. The first arg will be user
// data the registrant wants to have passed to it. The second argument
// is the new font, and the third argument is the new size.
typedef void (*Font_change_callback)(void *, Fl_Font, unsigned char);

// Any class that wants to be notified of changes in user preference
// in font/size should instantiate an instance of this class.
class Font_change_registration {
public:
	Font_change_registration(Font_change_callback func, void * arg);
	~Font_change_registration(void);

	// This runs all the registered callbacks
	static void run_callbacks(Fl_Font font, unsigned char size);

private:
	// Linked list of callbacks
	static Font_change_registration * list_p;
	Font_change_registration * next;

	// What function to call back, and what to pass it.
	Font_change_callback callback;
	void * callback_arg;
};

#endif
