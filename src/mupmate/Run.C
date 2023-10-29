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

// This file includes methods related to the "Run" menu on the main toolbar.

#include "Run.H"
#include "Main.H"
#include "Preferences.H"
#include "globals.H"
#include "Config.H"
#include "defines.h"
#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef OS_LIKE_UNIX
#include <sys/wait.h>
#include <errno.h>
#endif

// Message for when Mup fails, but we can't figure out why.
const char * const Unknown_Mup_failure = "Mup failed. Reason unknown.";

// Describe use of numbers passed to -x option
#define EXTRACT_NUM_DESCRIPTION "0 is used for pickup measure.\n" \
		"Negative numbers are used to specify\n" \
		"number of measures from the end.\n"

// Tooltip is the same for all macro entries
const char * macro_definition_tip =
	"Your Mup input can use macros to vary characteristics\n"
	"of the output for different runs.\n"
	"Enter the name of a macro you want to define,\n"
	"optionally followed by an equals sign and macro definition.\n"
	"Macro names must consist of upper case letters,\n"
	"numbers, and underscores, starting with an upper case letter.";

//------ Class for user to set parameters to pass to Mup

Run_parameters_dialog::Run_parameters_dialog(void)
		: Fl_Double_Window(0, 0, 700, 350, "Mup Options")
{
	enable_combine_p = new Fl_Check_Button(20, 20, 165, 30,
						"Enable Auto Multirest");
	enable_combine_p->tooltip("Set whether to automatically combine\n"
			"consecutive measures of rests into a multirest.");
	enable_combine_p->callback(combine_cb, this);
	enable_combine_p->when(FL_WHEN_CHANGED);

	rest_combine_p = new Positive_Int_Input(370, 20, 40, 30,
						"Min Measures to Combine");
	rest_combine_p->tooltip("Minimum number of consecutive measures\n"
			"of rest that will be combined into a multirest.");
	// Only becomes active if auto-multirest combine becomes active
	rest_combine_p->deactivate();

	// -p firstpage
	first_page_p = new Positive_Int_Input(185, 60, 40, 30, "First Page's Page Number");
	first_page_p->tooltip("Set the page number to use\n"
			"for the first page of music output.");
	// Optional pageside modifier
	pageside_p = new Fl_Group(230, 60, 180, 30, "");
	pageside_p->box(FL_ENGRAVED_BOX);
	pageside_p->align(FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE);

	default_p = new Fl_Check_Button(233, 65, 65, 20, "Default");
	default_p->type(FL_RADIO_BUTTON);
	default_p->tooltip("Treat first page as right unless panelsperpage is 2");
	default_p->when(FL_WHEN_NEVER);

	right_p = new Fl_Check_Button(300, 65, 55, 20, "Right");
	right_p->type(FL_RADIO_BUTTON);
	right_p->tooltip("Use the rightpage headers and footers (if defined)\n"
			"for the first page (and every other page thereafter).");
	right_p->when(FL_WHEN_NEVER);

	left_p = new Fl_Check_Button(365, 65, 45, 20, "Left");
	left_p->type(FL_RADIO_BUTTON);
	left_p->tooltip("Use the leftpage headers and footers (if defined)\n"
			"for the first page (and every other page thereafter).");
	left_p->when(FL_WHEN_NEVER);
	pageside_p->end();


	// -o pagelist
	pages_p = new Fl_Group(30, 100, 380, 70, "Pages");
	pages_p->box(FL_ENGRAVED_BOX);
	pages_p->align(FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE);

	all_p = new Fl_Check_Button(90, 105, 50, 20, "All");
	all_p->type(FL_RADIO_BUTTON);
	all_p->tooltip("You can restrict to only odd or even\n"
			"pages of music, but this shows all pages.");

	odd_p = new Fl_Check_Button(150, 105, 55, 20, "Odd");
	odd_p->type(FL_RADIO_BUTTON);
	odd_p->tooltip("This restricts to displaying only\n"
			"odd numbered pages. This is generally\n"
			"most useful for printing to a\n"
			"single-sided printer.");
	even_p = new Fl_Check_Button(215, 105, 60, 20, "Even");
	even_p->type(FL_RADIO_BUTTON);
	even_p->tooltip("This restricts to displaying only\n"
			"even numbered pages. This is generally\n"
			"most useful for printing to a\n"
			"single-sided printer.");
	reversed_p = new Fl_Check_Button(315, 105, 90, 20, "Reversed");
	reversed_p->tooltip("This restricts to displaying \n"
			"pages in reversed order. This is generally\n"
			"most useful for printing to a printer\n"
			"that stacks in reverse order.");
	reversed_p->type(FL_TOGGLE_BUTTON);
	saved_pages_direction = FORWARD_ORDER;

	page_list_p = new Fl_Input(110, 130, 280, 30, "Selected:");
	page_list_p->tooltip("List the specific pages you want displayed.\n"
			"You can list individual pages separated by commas,\n"
			"and/or ranges of pages separated by dashes.\n"
			"The special value of blank can also be used.\n"
			"Pages may be listed more than once and in any order.");
	saved_page_list = 0;
	pages_p->end();

	// -s stafflist
	staff_list_p = new Fl_Input(150, 180, 260, 30, "Staffs to Display/Play");
	staff_list_p->tooltip("If you wish to display or play only a subset\n"
			"of the staffs, or voices on a staff, list them here.\n"
			"Staffs are specified by comma-separated numbers\n"
			"or ranges, like 1,4 or 1-3,5-6. Staff numbers can be\n"
			"followed by v1, v2, or v3 to limit to a single voice.\n");
	saved_staff_list = 0;

	// -x extract list
	extract_begin_p = new Int_Input(150, 220, 95, 30, "Extract Measures");
	extract_begin_p->tooltip("If you wish to display or play only selected\n"
			"measures, enter the starting measure here.\n"
			EXTRACT_NUM_DESCRIPTION);
	extract_begin_p->callback(extract_cb, this);
	extract_begin_p->when(FL_WHEN_CHANGED);
	extract_end_p = new Int_Input(315, 220, 95, 30, "through");
	extract_end_p->tooltip("If you wish to display or play only selected\n"
			"measures, enter the ending measure here.\n"
			EXTRACT_NUM_DESCRIPTION);
	extract_end_p->deactivate();

	// Macros
	macros_group_p = new Fl_Group(430, 20, 250, 40 + MAX_MACROS * 40, "Macro Definitions");
	macros_group_p->box(FL_ENGRAVED_BOX);
	macros_group_p->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
	int m;
	for (m = 0; m < MAX_MACROS; m++) {

		macro_definitions_p[m] = new Fl_Input(450, 50 + m * 40, 210, 30, "");
		macro_definitions_p[m]->tooltip(macro_definition_tip);

		saved_macro_definitions[m] = 0;
	}
	macros_group_p->end();

	apply_and_close_p = new Fl_Return_Button(50, h() - 50, 160, 30, "Apply and close");
	apply_and_close_p->callback(Apply_and_close_cb, this);
	apply_and_close_p->when(FL_WHEN_RELEASE);


	apply_p = new Fl_Button(250, h() - 50, 80, 30, "Apply");
	apply_p->callback(Apply_cb, this);
	apply_p->when(FL_WHEN_RELEASE);

	clear_form_p = new Fl_Button((w() - 320), h() - 50, 150, 30,
								"Clear Options");
	clear_form_p->callback(clear_form_cb, this);
	clear_form_p->when(FL_WHEN_RELEASE);

	cancel_p = new Fl_Button(w() - 130, h() - 50, 80, 30, "Cancel");
	cancel_p->shortcut(FL_Escape);
	cancel_p->callback(Cancel_cb, this);
	cancel_p->when(FL_WHEN_RELEASE);

	// Set everything to default values
	clear_form();
	
	// Arrange to clean up all the new-ed widgets in destructor.
	end();

	// Arrange for window manager closes to do Cancel.
	callback(Cancel_cb, this);
	when(FL_WHEN_NEVER);
}


Run_parameters_dialog::~Run_parameters_dialog(void)
{
	int m;
	for (m = 0; m < MAX_MACROS; m++) {
		if (saved_macro_definitions[m] != 0) {
			delete[] saved_macro_definitions[m];
		}
	}
}


//---- Callback for when user changes enablement of rest combining,
// to gray or ungray the field for how many measures to combine.

CALL_BACK(Run_parameters_dialog, combine)
{
	if (enable_combine_p->value()) {
		rest_combine_p->activate();
	}
	else {
		rest_combine_p->value("");
		rest_combine_p->deactivate();
	}
}


//---- Callback for when user changes setting of start of measure extraction,
// to gray or ungray the field for ending measure of extraction.

CALL_BACK(Run_parameters_dialog, extract)
{
	if (extract_begin_p->size() > 0) {
		extract_end_p->activate();
	}
	else {
		extract_end_p->value("");
		extract_end_p->deactivate();
	}
}


//----Callback for button for clearing the form

CALL_BACK(Run_parameters_dialog, clear_form)
{
	// Set everything to default values
	saved_enable_combine = false;
	enable_combine_p->value(saved_enable_combine);

	saved_combine_measures[0] = '\0';
	rest_combine_p->value(saved_combine_measures);

	(void) snprintf(saved_first_page, sizeof(saved_first_page),
						"%d", MINFIRSTPAGE);
	first_page_p->value(saved_first_page);
	default_p->value(1);
	right_p->value(0);
	left_p->value(0);
	saved_pageside = "";

	if (saved_page_list != 0) {
		delete[] saved_page_list;
	}
	saved_page_list = new char[1];
	saved_page_list[0] = '\0';
	page_list_p->value(saved_page_list);
	all_p->value(1);
	odd_p->value(0);
	even_p->value(0);
	reversed_p->value(0);
	saved_pages_set = ALL_PAGES;

	if (saved_staff_list != 0) {
		delete[] saved_staff_list;
	}
	saved_staff_list = new char[1];
	saved_staff_list[0] = '\0';
	staff_list_p->value(saved_staff_list);

	saved_extract_begin[0] = '\0';
	extract_begin_p->value(saved_extract_begin);
	saved_extract_end[0] = '\0';
	extract_end_p->value(saved_extract_end);

	int m;
	for (m = 0; m < MAX_MACROS; m++) {
		macro_definitions_p[m]->value("");
		if (saved_macro_definitions[m] != 0) {
			delete[] saved_macro_definitions[m];
			saved_macro_definitions[m] = 0;
		}
	}
}


//---- callback for when user clicks "Apply" on Set Options form

CALL_BACK(Run_parameters_dialog, Apply)
{
	do_apply(false);
}

CALL_BACK(Run_parameters_dialog, Apply_and_close)
{
	do_apply(true);
}

void
Run_parameters_dialog::do_apply(bool ok_to_hide)
{
	// -c rest combine option
	bool error = false;
	saved_enable_combine = enable_combine_p->value();
	int num_meas = (int) atoi(rest_combine_p->value());
	if (saved_enable_combine && (num_meas < MINRESTCOMBINE || num_meas > MAXRESTCOMBINE)) {
		fl_alert("\"Min Measures to Combine\" must be between\n"
			"%d and %d.", MINRESTCOMBINE, MAXRESTCOMBINE);
		error = true;
	}
	else {
		(void) strcpy(saved_combine_measures, rest_combine_p->value());
	}

	// -p first page option
	int page_num = (int) atoi(first_page_p->value());
	if (page_num < MINFIRSTPAGE || page_num > MAXFIRSTPAGE) {
		fl_alert("\"First Page\" number must be between\n"
		"%d and %d.", MINFIRSTPAGE, MAXFIRSTPAGE);
		error = true;
	}
	else {
		(void) strcpy(saved_first_page, first_page_p->value());
	}
	if (left_p->value() == 1) {
		saved_pageside = ",leftpage";
	}
	else if (right_p->value() == 1) {
		saved_pageside = ",rightpage";
	}
	else {
		saved_pageside = "";
	}
	

	// Validate the list of pages, if any.
	if (page_list_p->size() > 0) {
		const char * error_description;
		if ( (error_description = valid_page_list()) != 0 ) {
			fl_alert("\"Selected Pages\" value is not valid:\n%s",
						error_description);
			error = true;
		}
		else {
			// Free existing, if any, and save new value.
			if (saved_page_list != 0) {
				delete[] saved_page_list;
			}
			saved_page_list = new char[page_list_p->size() + 1];
			strcpy(saved_page_list, page_list_p->value());
		}
	}
	else if (saved_page_list[0] != '\0') {
		// Had a list before but not anymore.
		// Null out the existing list.
		delete[] saved_page_list;
		saved_page_list = new char[1];
		saved_page_list[0] = '\0';
	}
	if (all_p->value() == 1) {
		saved_pages_set = ALL_PAGES;
	}
	if (odd_p->value() == 1) {
		saved_pages_set = ODD_PAGES;
	}
	if (even_p->value() == 1) {
		saved_pages_set = EVEN_PAGES;
	}
	if (reversed_p->value() == 1) {
		saved_pages_direction = REVERSED_ORDER;
	}
	else {
		saved_pages_direction = FORWARD_ORDER;
	}

	// Similar for staff list (-s option)
	if (staff_list_p->size() > 0) {
		if (strspn(staff_list_p->value(), "0123456789,-v \t")
					!= (size_t)(staff_list_p->size())) {
			fl_alert("\"Staffs to Display/Play\" is not valid");
			error = true;
		}
		else {
			if (saved_staff_list != 0) {
				delete[] saved_staff_list;
			}
			saved_staff_list = new char[staff_list_p->size() + 1];
			strcpy(saved_staff_list, staff_list_p->value());
		}
	}
	else if (saved_staff_list[0] != '\0') {
		delete[] saved_staff_list;
		saved_staff_list = new char[1];
		saved_staff_list[0] = '\0';
	}

	// We can't really error check -x option values.
	// Widget will have already constrained to integer,
	// and we have no way to know what range is valid,
	// since we don't know how many measures the song has.
	// However, to keep things simple, we have fixed-size array of 8 bytes
	// for saving value, so can only allow up to 6 digits plus sign.
	if (abs(atoi(extract_begin_p->value())) >= 1000000) {
		fl_alert("\"Extract Measures\" start value is out of range.");
		error = true;
	}
	else {
		(void) strcpy(saved_extract_begin, extract_begin_p->value());
	}
	if (abs(atoi(extract_end_p->value())) >= 1000000) {
		fl_alert("\"Extract Measures\" end value is out of range.");
		error = true;
	}
	else {
		(void) strcpy(saved_extract_end, extract_end_p->value());
	}

	// Macros
	int m;
	int macsize;
	bool changed;
	for (m = 0; m < MAX_MACROS; m++) {
		changed = false;
		if ((macsize = macro_definitions_p[m]->size()) > 0) {
			if (macro_error(macro_definitions_p[m]->value())) {
				fl_alert("Macro %d has invalid format.", m + 1);
				error = true;
			}
			else if (saved_macro_definitions[m] == 0) {
				// no macro before, but is one now
				changed = true;
			}
			else if (strcmp(macro_definitions_p[m]->value(),
					saved_macro_definitions[m]) != 0) {
				// A different macro value than before
				delete[] saved_macro_definitions[m];
				changed = true;
			}
			if (changed) {
				saved_macro_definitions[m] = new char[macsize + 1];
				(void) strcpy(saved_macro_definitions[m],
					macro_definitions_p[m]->value());
			}
		}
		else if (macsize == 0 && saved_macro_definitions[m] != 0) {
			// Used to be a macro value, but not anymore
			delete[] saved_macro_definitions[m];
			saved_macro_definitions[m] = 0;
		}
	}


	// If there were user errors, we leave the window up for user to
	// correct the errors. User can, of course, cancel without correcting,
	// in which case if they try to run, Mup will complain about the
	// bad arguments.
	if ( ok_to_hide && ! error ) {
		hide();
	}
}


// Validate a page subset list for Mup -o option.
// Returns error message if bad, 0 if good.

static const char * digits = "0123456789";
static const char * white = " \t";

const char *
Run_parameters_dialog::valid_page_list(void)
{
	const char * str;	// walk through the page list
	int num_digits;		// how many consecutive digits seen
	int start, end;		// of range


	for (str = page_list_p->value(); str != 0 && *str != '\0';  ) {

		// skip leading white space
		str += strspn(str, white);

		// "blank" is a special "page number"
		if (strncmp(str, "blank", 5)  == 0) {
			str += 5;
		}

		// Otherwise has to be a number
		else if ((num_digits = strspn(str, digits)) > 0) {
			// Skip past the number and any white space after it
			start = atoi(str);
			str += num_digits;
			str += strspn(str, white);

			// If followed by a dash, it is a range, so needs
			// another number.
			if (*str == '-') {
				// Skip past the dash and white space
				str++;
				str += strspn(str, white);

				// Next thing must be a number to end range
				if ((num_digits = strspn(str, digits)) < 1) {
					return("expecting digit");
				}

				// Check if range is backwards
				end = atoi(str);
				if (end < start) {
					return("range is backwards");
				}

				// Skip past the number
				str += num_digits;
			}
		}
		else {
			// Any else is a syntax error
			return("expecting page number or 'blank'");
		}

		// Skip any white space
		str += strspn(str, white);
		// If still something left, only valid thing here is a comma
		if (*str == ',') {
			str++;
		}
	}
	return (0);
}


//---- Callback for when user clicks "Cancel" on Set Options form

CALL_BACK(Run_parameters_dialog, Cancel)
{
	// Put back all the previous values
	enable_combine_p->value(saved_enable_combine);
	rest_combine_p->value(saved_combine_measures);
	first_page_p->value(saved_first_page);
	if (strlen(saved_pageside) != 0) {
		if (saved_pageside[1] == 'l') {
			left_p->value(1);
			right_p->value(0);
			default_p->value(0);
		}
		else {
			right_p->value(1);
			left_p->value(0);
			default_p->value(0);
		}
	}
	else {
		default_p->value(0);
		right_p->value(0);
		left_p->value(0);
	}

	// It seems setting a radio button via value(1) doesn't reset the
	// others, so clear all, then set the one we want
	all_p->value(0);
	odd_p->value(0);
	even_p->value(0);
	switch (saved_pages_set) {
	default:	// default should be impossible
	case ALL_PAGES:
		all_p->value(1);
		break;
	case ODD_PAGES:
		odd_p->value(1);
		break;
	case EVEN_PAGES:
		even_p->value(1);
		break;
	}
	reversed_p->value(saved_pages_direction);
	page_list_p->value(saved_page_list);
	staff_list_p->value(saved_staff_list);
	extract_begin_p->value(saved_extract_begin);
	extract_end_p->value(saved_extract_end);

	int m;
	for (m = 0; m < MAX_MACROS; m++) {
		macro_definitions_p[m]->value( saved_macro_definitions[m] == 0
					? "" : saved_macro_definitions[m]);
	}
	hide();
}

// Return true if check of macro finds an error.

bool
Run_parameters_dialog::macro_error(const char * macro)
{
	// First character must be an upper case letter.
	if ( ! isupper(macro[0])) {
		return(true);
	}
	// Everything up to = or end of string, whichever comes first,
	// must be upper case letter, digit, or underscore.
	const char * m_p;
	for (m_p = macro; *m_p != '\0' && *m_p != '='; m_p++) {
		if ( ! isupper(*m_p) && ! isdigit(*m_p) && *m_p != '_') {
			return(true);
		}
	}
	return(false);
}


//--------class for Run menu -----------------------------------------------

Run::Run()
{
	parameters_p = 0;
	report_p = 0;
#ifdef OS_LIKE_WIN32
	display_child.hProcess = 0;
	display_child.dwProcessId = 0;
	MIDI_child.hProcess = 0;
	MIDI_child.dwProcessId = 0;
#else
	display_child = 0;
	MIDI_child = 0;
#endif
}

Run::~Run()
{
	if (parameters_p != 0) {
		delete parameters_p;
		parameters_p = 0;
	}
	if (report_p != 0) {
		delete report_p;
		report_p = 0;
	}
	// Kill off any child processes
	clean_up();
}



//------------callback for Display menu item

CALL_BACK(Run, Display)
{
	Run_Mup(false, Show_PS);
}


//-----Callback for menu item to play MIDI file

CALL_BACK(Run, Play)
{
	Run_Mup(true, Play_MIDI);
}


//---------callback for menu item for writing PostScript output

CALL_BACK(Run, WritePostScript)
{
	Run_Mup(false, No_action);
}

//---------callback for menu item for writing MIDI output

CALL_BACK(Run, WriteMIDI)
{
	Run_Mup(true, No_action);
}

//---------callback for menu item for writing PDF output

CALL_BACK(Run, ConvertToPDF)
{
	Run_Mup(false, Convert_to_PDF);
}

//---- callback for menu item to collect Mup command line parameters from user

CALL_BACK(Run, Options)
{
	if (parameters_p == 0) {
		parameters_p = new Run_parameters_dialog();
	}
	parameters_p->show();
}


//--------- This lets Run class know the file name and editor buffer,
// which it needs access to, so it can run Mup on its contents.

void
Run::set_file(File * file_info_p)
{
	file_p = file_info_p;
}


// Run Mup commands with user's parameters on the current file
// The action tells us what, if any action to take after running Mup:
// like run a viewer, player, or converter.

void
Run::Run_Mup(bool midi, Action action)
{

	// Remove any error highlighting from previous run
	file_p->get_main()->unhighlight_all();

	const char * mup_input = file_p->effective_filename();
	// Make sure file has been written.
	// The filename == 0 will be hit in the case where
	// user did New From Template but made no changes.
	if (file_p->unsaved_changes || file_p->filename == 0) {
		int auto_save;
		(void) Preferences_p->get(Auto_save_preference, auto_save,
							Default_auto_save);
		if (auto_save) {
			file_p->Save(false);
			if (file_p->unsaved_changes || file_p->filename == 0) {
				// User probably canceled a Save that got
				// turned into a SaveAs,
				// or maybe Save failed. If not properly
				// saved, we shouldn't try to process it.
				return;
			}
		}
 		else {
			bool hide_the_No;
			const char * extra_text;
			// If there is a previous version of the file,
			// allow user to select "No," which means use that
			// previous version rather than writing out current.
			if (access(mup_input, F_OK) == 0) {
				hide_the_No = false;
				extra_text = "(If you select \"No,\" "
					"the most recently saved version "
					"of the Mup input file will be used.)";
			}
			else {
				// No previous version
				hide_the_No = true;
				extra_text = "";
			}
			switch (file_p->save_changes_check(extra_text, hide_the_No)) {
			default:	// default case should be impossible
			case Save_confirm_dialog::Cancel:
				return;
			case Save_confirm_dialog::No:
				break;
			case Save_confirm_dialog::Yes:
				file_p->Save(false);
				if (file_p->unsaved_changes
						|| file_p->filename == 0) {
					// User probably canceled the Save,
					// or maybe it failed. If not properly
					// saved, we shouldn't try to process it.
					return;
				}
				break;
			}
		}
		// If was "Untitled" before, it is now saved under a good
		// name, so get what that is.
		mup_input = file_p->effective_filename();
	}

	// Get length of file name without the extension
	int base_length = strlen(mup_input)
					- strlen(fl_filename_ext(mup_input));

	// Create output file name
	char mup_output[base_length + (midi ? 5 : 4)];
	strncpy(mup_output, mup_input, base_length);
	strcpy(mup_output + base_length, (midi ? ".mid" : ".ps"));

	// Create error file name
	char mup_error[base_length + 5];
	strncpy(mup_error, mup_input, base_length);
	strcpy(mup_error + base_length, ".err");

	// Get Mup command to use.
	char * mup_command;
	(void) Preferences_p->get(Mup_program_location, mup_command,
					Default_Mup_program_location);
	char full_location[FL_PATH_MAX];
	if ( ! find_executable(mup_command, full_location)) {
		fl_alert("Mup command not found.\n"
				"Check Config > File Locations setting.");
		return;
	}
	bool wrong_command = false;
	int clength = strlen(full_location);
	if (clength > 7 && (strcmp(full_location + clength - 7, "mupmate") == 0)
#ifdef OS_LIKE_WIN32
			&& (full_location[clength-8] == '/' ||
			full_location[clength-8] == '\\')) {
#else
			&& full_location[clength-8] == '/') { 
#endif
		wrong_command = true;
	}
#ifdef OS_LIKE_WIN32
	else if (clength > 11 && (strcmp(full_location + clength - 11, "mupmate.exe") == 0)
			&& (full_location[clength-12] == '/' ||
			full_location[clength-12] == '\\')) { 
		wrong_command = true;
	}
#endif
	if (wrong_command == true) {
		fl_alert("Value for Config > File Locations > Mup Command Path is incorrect.\n"
			"(Should be path to mup, not to mupmate.) Please correct and retry.");
		return;
	}

	if (parameters_p == 0) {
		// User hasn't set any parameters, so we will use all
		// defaults. Creating the instance lets us deference it
		// below, so we don't have to care whether user ever
		// went to the Set Options page or not.
		parameters_p = new Run_parameters_dialog();
	}

	// Build up list of arguments.
	// array slots needed for args:
	//	1 for Mup command itself
	//	2 for -e and arg
	//	2 for -f or -m and arg
	//	2 for -c and arg
	//	2 for -p and arg
	//	2 for -o and arg
	//	2 for -s and arg
	//	2 for -x and arg
	//	1 for Mup input file name
	//	2 for each -D and its macro definition arg
	//	1 for null terminator
	const char * command[17 + 2 * MAX_MACROS];
	command[0] = full_location;
	command[1] = "-e";
	command[2] = mup_error;
	command[3] = (midi ? "-m" : "-f");
	command[4] = mup_output;
	int arg_offset = 5;

	// rest combine
	if (parameters_p->enable_combine_p->value() &&
			parameters_p->rest_combine_p->size() > 0) {
		command[arg_offset++] = "-c";
		command[arg_offset++] = parameters_p->rest_combine_p->value();
	}

	// first page
	char full_firstpage_param[20];	// first page, including optional side
	if (parameters_p->first_page_p->size() > 0) {
		command[arg_offset++] = "-p";
		(void) snprintf(full_firstpage_param,
					sizeof(full_firstpage_param), "%s%s",
					parameters_p->first_page_p->value(),
					parameters_p->saved_pageside);
		command[arg_offset++] = full_firstpage_param;
	}

	// page list
	// Add -o option and its argument, if needed.
	char ooption[parameters_p->page_list_p->size() + 15];
	if (parameters_p->saved_pages_set != Run_parameters_dialog::ALL_PAGES
			|| parameters_p->saved_pages_direction
			== Run_parameters_dialog::REVERSED_ORDER
			|| parameters_p->page_list_p->size() > 0) {
		command[arg_offset++] = "-o";
		// May need enough space for "even,reversed," plus
		// the selected pages list and null terminator.
		// Might not need that much, but not worth the time
		// to shorten a local variable by a few bytes.
		if (parameters_p->saved_pages_set ==
					Run_parameters_dialog::ODD_PAGES) {
			(void) snprintf(ooption, sizeof(ooption), "odd");
		}
		else if (parameters_p->saved_pages_set ==
					Run_parameters_dialog::EVEN_PAGES) {
			(void) snprintf(ooption, sizeof(ooption), "even");
		}
		else {
			ooption[0] = '\0';
		}

		if (parameters_p->saved_pages_direction ==
				Run_parameters_dialog::REVERSED_ORDER) {
			if (strlen(ooption) > 0) {
				(void) strcat(ooption, ",");
			}
			(void) strcat(ooption, "reversed");
		}
		if (parameters_p->page_list_p->size() > 0) {
			if (strlen(ooption) > 0) {
				(void) strcat(ooption, ",");
			}
			(void) strcat(ooption, parameters_p->page_list_p->value());
		}
		command[arg_offset++] = ooption;
	}

	// staff list
	if (parameters_p->staff_list_p->size() > 0) {
		command[arg_offset++] = "-s";
		command[arg_offset++] = parameters_p->staff_list_p->value();
	}

	// extract list
	char xoption[parameters_p->extract_begin_p->size()
				+ parameters_p->extract_end_p->size() + 2];
	if (parameters_p->extract_begin_p->size() > 0) {
		command[arg_offset++] = "-x";
		(void) strcpy(xoption, parameters_p->extract_begin_p->value());
		if (parameters_p->extract_end_p->size() > 0) {
			(void) strcat(xoption, ",");
			(void) strcat(xoption, parameters_p->extract_end_p->value());
		}
		command[arg_offset++] = xoption;
	}

	// -D options
	int m;
	for (m = 0; m < MAX_MACROS; m++) {
		if (parameters_p->saved_macro_definitions[m] != 0) {
			command[arg_offset++] = "-D";
			command[arg_offset++] = parameters_p->saved_macro_definitions[m];
		}
	}

	// Mup input file name and null terminator
	command[arg_offset++] = mup_input;
	command[arg_offset++] = 0;

	static bool set_mupquiet = false;
	if ( ! set_mupquiet ) {
		// make non-const copy of "MUPQUIET=1 for passing to putenv
		char * mupquiet = new char[11];
		(void) strcpy(mupquiet, "MUPQUIET=1");
		putenv(mupquiet);
		set_mupquiet = true;
	}

	// Look up the right (dis)player program to use.
	// On Windows we need this even if we are only writing the file,
	// not actually (dis)playing. To keep the code simpler,
	// we just always look it up.
	char * player_command;
	if (midi) {
		(void) Preferences_p->get(MIDI_player_location,
			player_command,
			Default_MIDI_player_location);
	}
	else if (action == Convert_to_PDF) {
		(void) Preferences_p->get(PS_to_PDF_converter_location,
			player_command,
			Default_PS_to_PDF_converter_location);
#ifdef OS_LIKE_WIN32
			// Try to ensure gswin32c or gswin64c is available
			find_gswinNNc(player_command);
#endif
	}
	else {
		(void) Preferences_p->get(Viewer_location,
			player_command,
			Default_viewer_location);
	}
#ifdef OS_LIKE_WIN32
	// Media player locks the file it plays, so if we had
	// run it before, we have to make it release the lock 
	// before we run Mup. Also, if it was playing a different file
	// before, it doesn't seem to want to play ours.
	// So if the MIDI player of choice is the media player,
	// we first try to make it close somewhat gracefully,
	// and wait a second for that to complete.
	// In any case we try to kill off any child MIDI players we
	// know of. If the graceful close already worked,
	// it should find the child already dead.
	// If they are using some other MIDI player,
	// we don't know what to do, so we'll just kill any child
	// MIDI player we know spawned previously, if any.
	// Would be nice to be able to do something less drastic than
	// kill the process, but we're not sure what else would be effective.
	if (midi) {
		if (is_mplayer(player_command)) {
			HWND window_handle;
			if ((window_handle = FindWindow(0, "Windows Media Player")) != 0) {
				SendMessage(window_handle, WM_CLOSE, 0, 0);
			}
			_sleep(1000);
		}
		if (has_MIDI_child()) {
			kill_process(&MIDI_child, "MIDI player");
		}
	}
	// Somewhat similarly, we kill off any prior displayer, unless
	// the displayer is GSview, which we know we can pass -e argument
	// to make it reuse existing instance, if any.
	else if ( ! is_gsview(player_command) && has_display_child()) {
		kill_process(&display_child, "display");
	}
#endif

	// Run the command.
	int ret = execute_command(command, 0, true);

	// Report the errors, if any.
	// First clear out any previous error window.
	if (report_p != 0) {
		report_p->hide();
	}
	struct stat info;
	if (stat(mup_error, &info) == 0) {
		if (info.st_size > 0) {
			// Highlight the input lines with errors reported
			show_errors(mup_error, base_length);
			if (report_p == 0) {
				report_p = new Error_report();
			}
			report_p->loadfile(mup_error);
			report_p->show();
		}
		unlink(mup_error);
	}
	else if (ret != 0) {
		// Exited with error, but left no error file.
		// Must have died badly (core dumped, execvp failed, etc)
#if defined(WIFEXITED) && defined(WIFSIGNALED)
		if (WIFEXITED(ret)) {
			// Did exit(), so most likely exec failed
			// due to bad path to Mup program,
			// although we should have caught that above.
			fl_alert("Mup exited with return code %d but no error output.\n"
				"Check Config > File Locations setting.",
				WEXITSTATUS(ret));
		}
		else if (WIFSIGNALED(ret)) {
			// Probably core dump :-(
			fl_alert("Mup exited due to signal %d.", WTERMSIG(ret));
		} else {
			fl_alert(Unknown_Mup_failure);
		}
#else // WIF... macros not defined
		if (ret == -1) {
#ifdef OS_LIKE_WIN32
			// Look up the error reason to include in message.
			DWORD format_retval;
			LPVOID error_string = 0;
			if (FormatMessage(
					FORMAT_MESSAGE_ALLOCATE_BUFFER
					| FORMAT_MESSAGE_FROM_SYSTEM
					| FORMAT_MESSAGE_ARGUMENT_ARRAY,
					0, GetLastError(),
					LANG_NEUTRAL, (LPTSTR)&error_string,
					0, 0)) {
				fl_alert("Failed to execute Mup program:\n%s"
					"Check settings in Config > File Locations.",
					(char *) error_string);
				LocalFree((HLOCAL)error_string);
			}
			else {
				fl_alert(Unknown_Mup_failure);
			}
		}
#else  // not Windows
			fl_alert(Unknown_Mup_failure);
		}
#endif
		else {
			fl_alert(Unknown_Mup_failure);
		}
#endif	// WIF... macros
	}

	if (ret != 0) {
		// Something went wrong. We should not go on to
		// display/play even if user had asked for that.
		return;
	}

	// We wrote the output file successfully.
	// Check if we need to (dis)play the results.
	if (action == No_action) {
		// User just asked to generate a file, not (dis)play it.
		fl_message("%s output file\n\n     %s\n\n"
			"has been successfully created.",
			(midi ? "MIDI" : "PostScript"), mup_output);
		return;
	}


	if ( ! find_executable(player_command, full_location)) {
		fl_alert("Unable to run %s command.\n"
			"Check Config > File Locations setting.",
			player_command);
		return;
	}

#ifdef OS_LIKE_UNIX
	// For MIDI, we try to kill the previous player
	// if any, since we'll probably need the same
	// sound card. Note that for Windows, we did this
	// even before running Mup, since the player may lock
	// the file so Mup would be unable to write it.
	if (midi) {
		kill_process(&MIDI_child, "MIDI player");
	}

	// If using gv as displayer and one already running on
	// this file, it would be nice to get send it SIGHUP
	// to make it re-read the file
	// rather than starting a new one.
	// But for other displayers, we don't know what signal
	// might work, if any. And even with gv, if we send
	// the signal, we have no way of knowing whether it
	// actually worked. And there are lots of cases to
	// consider. Suppose the displayer was gv,
	//  and they brought up an instance,
	// then they changed to some other displayer,
	// and then back to gv. Should we have killed off
	// the first gv when bringing up the other viewer,
	// of should we keep it up and reuse it after they
	// change back?  What if they kill the current
	// instance just after we decided to re-use it?
	// So just keep things simple for now. Always
	// kill off any existing viewer we know of and
	// start up a new one.
	else if (action == Show_PS) {
		kill_process(&display_child, "display");
	}
#endif

	// Fill in the argv array for displayer/player
	command[0] = full_location;
#ifdef OS_LIKE_WIN32
	// Media player appears to ignore its argument
	// if it isn't a full path name, including drive.
	// So ensure we have everything.
	char fullpath[FL_PATH_MAX];
	if (mup_output[1] != ':' ) {
		// We don't have complete path.
		// Get current directory including drive.
		if (getcwd(fullpath, sizeof(fullpath)) == 0) {
			fl_alert("Unable to determine current folder.");
			return;
		}
		if (mup_output[0] != dir_separator()) {
			// Relative path
			(void) snprintf(fullpath + strlen(fullpath),
				sizeof(fullpath) - strlen(fullpath),
				"%c%s", dir_separator(),
				mup_output);
		}
		else {
			// Was full path except for drive.
			(void) strcpy(fullpath + 2, mup_output);
		}
	}
	else {
		// Can use existing path as is.
		(void) strcpy(fullpath, mup_output);
	}

	if ( ! midi && is_gsview(player_command)) {
		// For gsview, use -e to make it use
		// existing instance if any.
		command[1] = "-e";
		command[2] = fullpath;
		command[3] = 0;
	}
	else {
		command[1] = fullpath;
		command[2] = 0;
		
	}
#else
	command[1] = mup_output;
	command[2] = 0;
#endif

	if (action == Convert_to_PDF) {
		char *pdf_filename;
		char *working_dir;
		char *dir_end;
		int len;

		// Deduce the name of the pdf file that will get generated
		len = strlen(mup_output);
		// Need one byte for null and one because pdf is longer than ps
		pdf_filename = (char *) malloc(len + 2);
		/* Normally would do a strncpy here, but some compilers warn
		 * that the len is based on the source size rather than the
		 * destination size. While that is true, is it done in a way
		 * that is safe. The author of the warning even agrees
		 * there are cases that are actually safe, but difficult
		 * for the warning code to realize they are safe.
		 * So use memcpy to avoid the warning.
		 */
		memcpy(pdf_filename, mup_output, len - 2);
		strcpy(pdf_filename + len - 2, "pdf");

		// Get the name of the directory where the input file is.
		// The PDF converters put their output in their current
		// directory, so we need to go there to ensure the output
		// appears in the same directory as the input.
		dir_end = strrchr(mup_output, dir_separator());
		if (dir_end != 0) {
			size_t dir_len;
			dir_len = dir_end - mup_output;
			working_dir = (char *) malloc(dir_len + 1);
			strncpy(working_dir, mup_output, dir_len);
			working_dir[dir_len] = '\0';
		}
		else {
			// Should never happen, but if it does,
			// this way the worst that should happen is the file
			// may get put in the wrong directory,
			// and the conversion would then appear to fail.
			working_dir = 0;
		}

		// Run the converter and report how it went.
		if (execute_command(command, 0, false, working_dir) == 0) {
			fl_message("PDF output file\n\n     %s\n\n"
				"has been successfully created.",
				pdf_filename);
		}
		else {
			fl_alert("running of '%s %s'\nto create %s failed",
					command[0], command[1], pdf_filename);
		}
		if (working_dir != 0) {
			free(working_dir);
		}
		free(pdf_filename);
		return;
	}

	if (execute_command(command,
			(midi ? &MIDI_child : &display_child)) != 0) {
		fl_alert("Unable to run %s command.\n"
			"Check settings under Config > File Locations.",
			command[0]);
	}
}


// Execute given command with the given argv.
// If proc_info_p is zero, wait for the process to complete,
// otherwise save information about the spawned process in what it points to,
// so that the caller can keep track of it while it runs independently.
// The hide_window parameter is only used for Windows and causes the
// spawned process to be created with flags to not create a window,
// This lets us use a console mode version of Mup, so traditional users
// can continue to run Mup in a console without mupmate, but we can
// run the same .exe without the annoyance of a console popping up.
// If working_dir is not NULL, run the command with that as its current
// working directory.
// Returns 0 on success, -1 on failure to create process,
// or what the process returned if it had non-zero exit code,
// and was waited for.

int
Run::execute_command(const char ** argv, Proc_Info *proc_info_p, bool hide_window, const char * const working_dir)
{
	int ret = -1;

#ifdef OS_LIKE_UNIX
	pid_t child;
	switch (child = fork()) {
	case 0:
		if (working_dir != 0) {
			if (chdir(working_dir) != 0) {
				fl_alert("unable to chdir to %s", working_dir);
			}
		}
		execvp(argv[0], (char * const *)argv);
		// If here, the exec failed. Child must die.
		exit(1);
	case -1:
		// failed
		if (proc_info_p != 0) {
			*proc_info_p = 0;
		}
		break;
	default:
		if (proc_info_p == 0) {
			// wait for child to complete
			if (waitpid(child, &ret, 0) < 0) {
				ret = -1;
			}
		}
		else {
			*proc_info_p = child;
			ret = 0;
		}
		break;
	}
#else

#ifdef OS_LIKE_WIN32

	// Convert the argv array into a string with each argument quoted.
	// First calculate how much space is needed.
	int a;
	int length = 0;
	bool has_quote = false;		// to optimize normal case
	for (a = 0; argv[a] != 0; a++) {
		length += strlen(argv[a]);
		// A quoted argv[0] won't work; just quote the other args.
		if (a > 0) {
			// Add space before the arg and quotes on each end.
			length += 3;
			const char * s;
			// If embedded quote, add space for escaping it
			for (s = argv[a]; *s != '\0'; s++) {
				if (*s == '"') {
					length++;
					has_quote = true;
				}
			}
		}
	}

	// Get space and fill in all the arguments, properly quoted.
	char command[length + 1];
	(void) strcpy(command, argv[0]);
	char * dest = command + strlen(command);
	for (a = 1; argv[a] != 0; a++) {
		*dest++ = ' ';
		*dest++ = '"';
		if (has_quote) {
			int i;
			for (i = 0; argv[a][i] != '\0'; i++) {
				if (argv[a][i] == '"') {
					*dest++ = '\\';
				}
				*dest++ = argv[a][i];
			}
		}
		else {
			(void) strcpy(dest, argv[a]);
			dest += strlen(dest);
		}
		*dest++ = '"';
	}
	*dest = '\0';

	// Fill in information for starting up the process
	PROCESS_INFORMATION process_info;
	STARTUPINFO         startup_info;
	memset( &startup_info, 0, sizeof(startup_info));
	startup_info.cb = sizeof(startup_info);
	DWORD create_flags;	// flags to control creation aspects
	if (hide_window) {
		startup_info.dwFlags = STARTF_USESHOWWINDOW;
		startup_info.wShowWindow = SW_HIDE;
		create_flags = CREATE_NO_WINDOW;
	}
	else {
		create_flags = 0;
	}

	// Run the process
	BOOL proc = CreateProcess(NULL, command, NULL, NULL, 
			TRUE, create_flags, NULL, working_dir,
			&startup_info, &process_info);

	if (proc) {
		// It was successfully created.
		if (proc_info_p == 0) {
			// wait for child to complete
			DWORD result = WaitForSingleObject(
					process_info.hProcess, INFINITE);
			switch (result) {
			case WAIT_FAILED:
			case WAIT_ABANDONED:
			case WAIT_TIMEOUT:
				ret = -1;
				break;
			default:
				GetExitCodeProcess(process_info.hProcess, &result);
				ret = (int) result;
			}
		}
		else {
			*proc_info_p = process_info;
			ret = 0;
		}
	}
	else {
		proc_info_p->hProcess = 0;
		proc_info_p->dwProcessId = 0;
		ret = -1;
	}

#else
	fl_alert("Process execution only implemented\n"
		"for Linux (and similar) and Windows so far...");
	ret = -1;
#endif

#endif
	return(ret);
}


// Kill the specified process, if it exists.
// The description is used in error messages

void
Run::kill_process(const Proc_Info * const proc_info_p, const char * const description)
{
#ifdef OS_LIKE_UNIX
	int exitstatus;
	if (*proc_info_p == 0 || waitpid(*proc_info_p, &exitstatus, WNOHANG)
							== *proc_info_p) {
		// No process spawned or the one we had already died
		return;
	}
	// Not clear how hard to try to kill process.
	// SIGTERM should usually work, and is preferable if it works.
	// We wait a little while and try SIGKILL if it hasn't died yet.
	// We don't check for errors, because the
	// only errors that should be possible are bad signal (should be
	// impossible, since we hard-code SIGTERM), process doesn't exist
	// (already dead, so no need to kill it), or bad permission (we
	// spawned it ourself, so ought to have permission to kill it).
	(void) kill(*proc_info_p, SIGTERM);
	// Wait for up to 3 seconds for it to die
	int w;
	int ret;
	for (w = 0; w < 3; w++) {
		if ((ret = waitpid(*proc_info_p, &exitstatus, WNOHANG))
							== *proc_info_p) {
			// It died.
			// *** Especially in the case of MIDI, there is a
			// chance the player has already written stuff out
			// to the device driver that it won't clear out when
			// it dies, so that if we try to start a new one,
			// the device will still be busy. But we have no way
			// to know how long to wait, if at all, and don't
			// want to always sleep a long time just to cover
			// the case of trying to play again while in the middle
			// of playing.
			return;
		}
		if (ret == -1 && errno == ECHILD) {
			// Child doesn't exist, so nothing to kill
			return;
		}
		sleep(1);
	}
	// Okay. Resort to SIGKILL and wait 1 more second to let it die.
	kill(*proc_info_p, SIGKILL);
	sleep(1);
#endif
#ifdef OS_LIKE_WIN32
	if (proc_info_p->hProcess == 0 || WaitForSingleObject(
				proc_info_p->hProcess, 0) != WAIT_TIMEOUT) {
		// No process spawned or the one we had must have already died.
		return;
	}
	HANDLE handle;
	if ( (handle = OpenProcess(PROCESS_TERMINATE,
			FALSE, proc_info_p->dwProcessId)) == 0 ||
			TerminateProcess(handle, 0) == 0) {
		// Warn user we were unable to kill the old child.
		DWORD format_retval;
		LPVOID error_string = 0;
		if (FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER
					| FORMAT_MESSAGE_FROM_SYSTEM
					| FORMAT_MESSAGE_ARGUMENT_ARRAY,
					0, GetLastError(),
					LANG_NEUTRAL, (LPTSTR)&error_string,
					0, 0)) {
			fl_alert("Unable to terminate %s process.\n%s",
					description, (char *) error_string);
			LocalFree((HLOCAL)error_string);
		}
	}
	if (handle != 0) {
		CloseHandle(handle);
	}
#endif
}


#ifdef OS_LIKE_WIN32
// Return true if the given command is for Windows Media Player.

bool
Run::is_mplayer(const char * command)
{
	int length = strlen(command);
	if (strcasecmp(command + length - 12, "wmplayer.exe") == 0 ||
			strcasecmp(command + length - 11, "mplayer.exe") == 0 ||
			strcasecmp(command + length - 8, "wmplayer") == 0 ||
			strcasecmp(command + length - 7, "mplayer") == 0) {
		return(true);
	}
	return(false);
}


// Return true if the given command is for GSview.

bool
Run::is_gsview(const char * command)
{
	int length = strlen(command);
	if (strcasecmp(command + length - 12, "gsview32.exe") == 0 ||
			strcasecmp(command + length - 10, "gsview.exe") == 0 ||
			strcasecmp(command + length - 8, "gsview32") == 0 ||
			strcasecmp(command + length - 6, "gsview") == 0) {
		return(true);
	}
	return(false);
}
#endif



// Mark an input line as warning or error

void
Run::mark_line(const int line, const Run::Severity severity)
{
	if (line < 1 || severity == Run::No_error) {
		return;
	}
	file_p->get_main()->highlight_error_line(line,
						severity == Error ? 'B': 'C');
}


// Highlight errors in the input by parsing the error file

void
Run::show_errors(const char * errfile, const int base_length)
{
	FILE * file;
	char buff[BUFSIZ];
	int num_lines;
	int line;

	if ((file = fopen(errfile, "r")) == 0) {
		return;
	}

	// We want to mark lines by the most severe problem they have--
	// if a line has both warning and error, we will mark as error.
	// In some scenarios the messages may come out in the different parts
	// of the error output. So allocate a buffer for every line in the
	// file, initialize to no errors, then go through and mark bad lines.
	// If a line already has a higher severity, leave as is.
	// Note that the very last line in the file doesn't have a newline,
	// so count_lines() actually returns one less than the real number
	// of lines, so we need to add one.
	num_lines = file_p->get_editor()->buffer()->count_lines(0,
				file_p->get_editor()->buffer()->length()) + 1;
	// Lines start at 1 and array index at 0, so allocate extra and
	// leave element zero empty.
	Run::Severity * severities = new Run::Severity[num_lines + 1];
	for (line = 1; line <= num_lines; line++) {
		severities[line] = Run::No_error;
	}

	line = -1;
	const char *mac_err_message = "note: previous error found while expanding macro ";
	int mac_err_msg_length = strlen(mac_err_message);
	while (fgets(buff, sizeof(buff), file) != 0) {

		// If matches filename: line N: we have a line to mark
		if (strncmp(buff, errfile, base_length) == 0) {
			char * linematch;
			if ((linematch = strstr(buff + base_length, ": line ")) != 0) {
				line = atoi(linematch + 7);
				continue;
			}
		}

		// If matches the message for an error when expanding a
		// macro, find the line number in that, and highlight it.
		// Although it is probably more common for the "real" error
		// to be in the definition than the call, for any given case
		// we can't really know whether the "real" error is in
		// the macro definition or the macro call or both,
		// so we will highlight the call from the code above
		// and the definition from this code.
		if (strncmp(buff, mac_err_message, mac_err_msg_length) == 0) {
			char * from_match;
			if ((from_match = strstr(buff + mac_err_msg_length + 2, "' from ")) == 0) {
				continue;
			}
			if (strncmp(from_match + 7, errfile, base_length) != 0) {
				continue;
			}
			if (strncmp(from_match + base_length + 8, ": line ", 7) != 0) {
				int mac_line = atoi(from_match + base_length + 14);
				// If the severity reported at the call site
				// is Error, mark the definition line as Error
				// as well. Otherwise, if the call site is
				// only a warning, and the definition line
				// is not already more severve than Warning,
				// then mark definition as Warning.
				if (severities[line] == Run::Error) {
					severities[mac_line] = Run::Error;
				}
				else if (severities[line] == Run::Warning &&
							severities[mac_line]
							!= Run::Error) {
					severities[mac_line] = Run::Warning;
				}
				continue;
			}
		}

		if (buff[0] == '\r' || buff[0] == '\n') {
			// At the end of each list of errors for a line,
			// reset for the next.
			line = -1;
			continue;
		}

		if (line < 0) {
			// Probably error not associated with any line
			continue;
		}

		if (line > num_lines) {
			// Probably error detected at EOF
			continue;
		}

		// If "! Error"  most recent should be marked in red
		if (strncmp(buff, "! Error", 7) == 0) {
			severities[line] = Run::Error;
		}

		// If "- Warning" should mark, unless already has error
		else if ( severities[line] != Run::Error
					&& strncmp(buff, "- Warning", 9) == 0) {
			severities[line] = Run::Warning;
		}
	}
	int errors = 0;
	for (line = 1; line <= num_lines; line++) {
		if (severities[line] != Run::No_error) {
			mark_line(line, severities[line]);
			// Jump to the first error line
			if (++errors == 1) {
				Edit::do_goto(file_p->editor_p, line, false);
			}
		}
	}
	delete[] severities;
}


// Called when user begins a new file, or when exiting,
// to kill off child proceses that were spawned.
// We start new processes for new file,
// even if one already up for current file.
// Not sure if that's what user wants, but...

void
Run::clean_up(void)
{
	kill_process(&display_child, "display");
	kill_process(&MIDI_child, "MIDI player");
}


// Report whether we spawned a display child and think it is still alive.

bool
Run::has_display_child(void)
{
#ifdef OS_LIKE_WIN32
	return(display_child.hProcess != 0);
#else
	return(display_child != 0);
#endif
}


// Report whether we spawned a MIDI child and think it is still alive.

bool
Run::has_MIDI_child(void)
{
#ifdef OS_LIKE_WIN32
	return(MIDI_child.hProcess != 0);
#else
	return(MIDI_child != 0);
#endif
}


//------------ Class for displaying errors from Mup

Error_report::Error_report(void)
	: Fl_Double_Window(Default_width, Default_height, "Error report")
{
	text_p = new Fl_Text_Display(20, 20, w() - 40, h() - 90);
	resizable((Fl_Widget *) text_p);
	text_p->buffer( new Fl_Text_Buffer() );

	// Set font/size and arrange to get notified of changes in them
	font_change_reg_p = new Font_change_registration(
					font_change_cb, (void *) this);

	ok_p = new Fl_Return_Button(w() / 2 - 40, h() - 50, 80, 30, "OK");
	ok_p->callback(OK_cb, this); 
	show();

	// Arrange to clean up new-ed widgets in destructor.
	end();
}

Error_report::~Error_report()
{
}


// Load the error file (from -e of Mup) into window to show user.

int
Error_report::loadfile(const char * filename)
{
	return text_p->buffer()->loadfile(filename);
}


// Callback for when user clicks OK after reading Mup error report

CALL_BACK(Error_report, OK)
{
	hide();
}


// Callback for change in font/size

void
Error_report::font_change_cb(void * data, Fl_Font font, unsigned char size)
{
	((Error_report *)data)->font_change(font, size);
}

void
Error_report::font_change(Fl_Font font, unsigned char size)
{
	text_p->textfont(font);
	text_p->textsize(size);
	text_p->redisplay_range(0, text_p->buffer()->length());
}
