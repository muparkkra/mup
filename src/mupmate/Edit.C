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

// Code for Edit menu on main toolbar


#include "globals.H"
#include "Edit.H"
#include "utils.H"
#include <FL/Enumerations.H>
#include <FL/fl_ask.H>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


//-------------------Find_dialog class--------------------------------
// This class is for the window that pops up when user does a "Find"

// Constructor creates the window and all the widgets inside it

Find_dialog::Find_dialog(void)
	: Fl_Double_Window(430, 170, "Find")
{
	pattern_p = new Fl_Input(90, 20, 175, 20, "Find what:");
	// We have ungray the "Find Next" when user enters a pattern,
	// so set up callback for that
	pattern_p->callback(Pattern_cb, this);
	pattern_p->when(FL_WHEN_CHANGED);
	pattern_p->tooltip("Enter the pattern you want to\n"
			"search for in your Mup input.");

	replace_with_p = new Fl_Input(90, 60, 175, 20, "Replace with");
	replace_with_p->tooltip("Enter the replacement text.");
	replace_with_p->hide();

	casematch_p = new Fl_Check_Button(10, 85, 100, 20, "Match case");
	casematch_p->tooltip("If checked, upper/lower case must match.\n"
			"If not checked, case is ignored.");

	direction_p = new Fl_Box(FL_ENGRAVED_BOX, 120, 55, 125, 35, "");
	up_p = new Fl_Round_Button(130, 60, 40, 20, "Up");
	up_p->type(FL_RADIO_BUTTON);
	up_p->value(0);
	up_p->tooltip("Search upward from current place.");
	up_p->callback(change_cb, this);
	down_p = new Fl_Round_Button(175, 60, 60, 20, "Down");
	down_p->type(FL_RADIO_BUTTON);
	down_p->value(1);
	down_p->tooltip("Search downward from current place.");
	down_p->callback(change_cb, this);

	next_p = new Fl_Return_Button(285, 10, 125, 30, "Find Next");
	next_p->when(FL_WHEN_RELEASE);
	next_p->callback(FindNext_cb, this);
	next_p->deactivate();

	replace_p = new Fl_Button(285, 45, 125, 30, "Replace");
	replace_p->tooltip("Replace the current instance of the pattern\n"
			"with the replacement text.");
	replace_p->callback(Replace_cb, this);
	replace_p->when(FL_WHEN_RELEASE);
	replace_p->deactivate();
	replace_p->hide();

	replace_all_p = new Fl_Button(285, 80, 125, 30, "Replace All");
	replace_all_p->tooltip("Replace all instances of the pattern\n"
			"with the replacement text.");
	replace_all_p->callback(ReplaceAll_cb, this);
	replace_all_p->when(FL_WHEN_RELEASE);
	replace_all_p->deactivate();
	replace_all_p->hide();

	cancel_p = new Fl_Button(285, 115, 125, 30, "Cancel");
	cancel_p->shortcut(FL_Escape);
	cancel_p->when(FL_WHEN_RELEASE);
	cancel_p->callback(Cancel_cb, this);

	// Arrange for destructor to clean up new-ed widgets
	end();

	// Arrange for window manager closes to do Cancel.
	callback(Cancel_cb, this);
	when(FL_WHEN_NEVER);
}

Find_dialog::~Find_dialog()
{
}


// The class can be used for either Find or Replace.
// These next two methods set which of those two personalities
// the window has.

void
Find_dialog::as_Find()
{
	label("Find");
	cancel_p->resize(cancel_p->x(), 55, cancel_p->w(), cancel_p->h());
	casematch_p->resize(casematch_p->x(), 65, casematch_p->w(), casematch_p->h());
	resize(x(), y(), w(), 120);
	direction_p->show();
	up_p->show();
	down_p->show();
	replace_with_p->hide();
	replace_p->hide();
	replace_all_p->hide();
	is_replace = false;
}

void
Find_dialog::as_Replace()
{
	label("Replace");
	cancel_p->resize(cancel_p->x(), 115, cancel_p->w(), cancel_p->h());
	casematch_p->resize(casematch_p->x(), 115, casematch_p->w(), casematch_p->h());
	resize(x(), y(), w(), 155);
	direction_p->hide();
	up_p->hide();
	down_p->hide();
	replace_with_p->show();
	replace_p->show();
	replace_all_p->show();
	is_replace = true;
}


// Callback for when user clicks "Find Next" button after filling in
// the dialog.

CALL_BACK(Find_dialog, FindNext)
{
	int start = editor_p->insert_position();
	bool found;
	int where;
	if (down_p->value() == 1 || is_replace) {
		found = editor_p->buffer()->search_forward(
				start + 1, pattern_p->value(),
				&where, casematch_p->value());
	}
	else {
		found = editor_p->buffer()->search_backward(
				start - 1, pattern_p->value(),
				&where, casematch_p->value());
	}

	if ( ! found ) {
		// It seems fltk does not find a pattern if it is exactly
		// at the beginning for an upward search or exactly
		// at the end for a downward search. That surely can't
		// be right, so add special checks for that. If fltk
		// fixes that some day, we'll never hit this case,
		// so this should still be compatible.
		int patlength = pattern_p->size();
		int bufflength = editor_p->buffer()->length();
		where = (down_p->value() ? bufflength - patlength : 0);
		// If pattern is longer than buffer, then no match possible.
		// If already at pattern,
		// we already found the end one last time.
		if (patlength <= bufflength &&
				where != editor_p->insert_position() - patlength) {
			if (casematch_p->value()) {
				if (strncmp(pattern_p->value(),
						editor_p->buffer()->text()
						+ where, patlength) == 0) {
					found = 1;
				}
			}
			else {
				if (strncasecmp(pattern_p->value(),
						editor_p->buffer()->text()
						+ where, patlength) == 0) {
					found = 1;
				}
			}
		}

		if ( ! found ) {
			fl_alert("Cannot find \"%s\"", pattern_p->value());
			gray_out();

			// The main editor window should now
			// be made active, rather than the Find window.
			editor_p->take_focus();
		}
	}
	if (found) {
		editor_p->buffer()->highlight(where, where + pattern_p->size());
		editor_p->insert_position(where + pattern_p->size());
		editor_p->show_insert_position();
	}
}


// Callback for when user clicks "Replace"

CALL_BACK(Find_dialog, Replace)
{
	// See if we are already at the pattern to replace due to
	// a previous Replace/Find Next
	int start, end;
#ifdef FLTK_1_1
	int isRect, rectStart, rectEnd;
#endif
	bool at_pattern = false;
#ifdef FLTK_1_1
	if (editor_p->buffer()->highlight_position(&start, &end, &isRect,
						&rectStart, &rectEnd)) {
#else
	if (editor_p->buffer()->highlight_position(&start, &end)) {
#endif
		int place = editor_p->insert_position();
		if (place == end && (end - start == pattern_p->size())) {
			if (casematch_p->value()) {
			
				at_pattern = (strncmp(pattern_p->value(),
					editor_p->buffer()->text() + start,
					pattern_p->size()) == 0);
			}
			else {
				at_pattern = (strncasecmp(pattern_p->value(),
					editor_p->buffer()->text() + start,
					pattern_p->size()) == 0);
			}
		}
	}

	if (at_pattern) {
		editor_p->buffer()->unhighlight();
		editor_p->buffer()->replace(start, end, replace_with_p->value());
	}

	FindNext();
}


// Callback for when use clicks "Replace All"

CALL_BACK(Find_dialog, ReplaceAll)
{
	// We want to be able to "undo" the entire "Replace All"
	// so we make a copy of the buffer, make all the changes in the
	// copy and then replace the original with the altered copy.
	Fl_Text_Buffer altered_buff;
	altered_buff.copy(editor_p->buffer(), 0, editor_p->buffer()->length(), 0);
	int start;	// where to begin each search
	bool found = true;	// if matching pattern found on current search
	int where;	// offset into buffer where match occurred
	int new_cursor_pos;
	bool replaced_something = false;	// if any matches found at all

	new_cursor_pos = editor_p->insert_position();
	for (start = 0; found; start = where + replace_with_p->size()) {
		if ((found = altered_buff.search_forward(
						start, pattern_p->value(),
						&where, casematch_p->value()))
						!= 0) {
			altered_buff.replace(where, where + pattern_p->size(),
						replace_with_p->value());
			new_cursor_pos = where + replace_with_p->size();
			replaced_something = true;
		}
	}
	// Kludge because pattern at very end is not found.
	// See more complete explanation in FindNext().
	where = altered_buff.length() - pattern_p->size();
	if (where >= 0) {
		found = false;
		if (casematch_p->value()) {
			if (strcmp(pattern_p->value(),
					altered_buff.text() + where) == 0) {
				found = true;
			}
		}	
		else {
			if (strcasecmp(pattern_p->value(),
					altered_buff.text() + where) == 0) {
				found = true;
			}
		}
		if (found) {
			altered_buff.replace(where, where + pattern_p->size(),
						replace_with_p->value());
			new_cursor_pos = where + replace_with_p->size();
			replaced_something = true;
		}
	}

	if (replaced_something) {
		editor_p->buffer()->replace(0, editor_p->buffer()->length(),
						altered_buff.text());
		editor_p->insert_position(new_cursor_pos);
	}
	else {
		fl_alert("No instances of pattern to replace.");
	}
	replace_all_p->deactivate();
}


// Callback for when user clicks "Cancel" in the "Find" window.
// Hides the window.

CALL_BACK(Find_dialog, Cancel)
{
	editor_p->buffer()->unhighlight();
	hide();
}


// If user did Find Next until no more instances found,
// the Find Next and Replace buttons will get grayed out,
// But if they then change search direction, we need to reactivate them
// since the pattern might be found in that direction.
// They must also be ungrayed if the contents of the editor buffer change,
// since the new text might contain the pattern.

CALL_BACK(Find_dialog, change)
{
	next_p->activate();
	replace_p->activate();
	replace_all_p->activate();
}


// Callback for when user changes pattern, to know whether to gray or ungray
// Find Next button or not.

CALL_BACK(Find_dialog, Pattern)
{
	if (pattern_p->size() > 0) {
		next_p->activate();
		replace_p->activate();
		replace_all_p->activate();
	}
	else {
		gray_out();
	}
}

// Horrible kludge. If the "Find" or "Replace" is grayed out because there
// are no more instances of the pattern in the current direction,
// and then user moves the cursor somewhere else, it's possible the pattern
// may then be findable. But modify_callback does not get called for
// a change in cursor position. So we poll to see if the cursor position
// changed since the last check, and if so, ungray.
// Fortunately, we can limit this to only when the button are grayed,
// which shouldn't be too often.

void
Find_dialog::gray_out(void)
{
	next_p->deactivate();
	replace_p->deactivate();
	replace_all_p->deactivate();
	// Remember where we are and set up to poll for changes
	last_cursor_position = editor_p->insert_position();
	Fl::add_timeout(0.5, cursor_change_check, this);
}

void
Find_dialog::cursor_change_check(void * data)
{
	Find_dialog * obj_p = (Find_dialog *) data;
	if (obj_p->editor_p->insert_position() != obj_p->last_cursor_position
				&& obj_p->pattern_p->size() > 0) {
		obj_p->change();
	}
	else {
		Fl::repeat_timeout(0.5, cursor_change_check, data);
	}
}


// Class needs access to the editor; this lets it know which editor
// instance to use, and which main window it is associated with.

void
Find_dialog::set_editor(Fl_Text_Editor * ed)
{
	editor_p = ed;
}


// Returns the current "Find" pattern entered by user,
// or "" if they have not yet entered any such pattern

const char *
Find_dialog::get_pattern()
{
	if (pattern_p->value() == 0) {
		return("");
	}
	else {
		return(pattern_p->value());
	}
}


//---------------- GoTo class---------------------------------------------

GoTo_dialog::GoTo_dialog(void)
	: Fl_Double_Window(225, 95, "Goto line")
{
	linenum_p = new Positive_Int_Input(115, 10, 60, 30, "Line Number:");
	linenum_p->tooltip("Enter the line number of the line\n"
			"you want to make the current line.");

	ok_p = new Fl_Return_Button(25, 50, 75, 30, "OK");
	ok_p->when(FL_WHEN_RELEASE);
	ok_p->callback(OK_cb, this);

	cancel_p = new Fl_Button(125, 50, 75, 30, "Cancel");
	cancel_p->shortcut(FL_Escape);
	cancel_p->when(FL_WHEN_RELEASE);
	cancel_p->callback(Cancel_cb, this);

	// Arrange for destructor to clean up new-ed widgets
	end();

	// Arrange for window manager closes to do Cancel.
	callback(Cancel_cb, this);
	when(FL_WHEN_NEVER);
}

GoTo_dialog::~GoTo_dialog()
{
}


// Callback for when user clicks "OK" in GoTo dialog

CALL_BACK(GoTo_dialog, OK)
{
	Edit::do_goto(editor_p, (int)strtol(linenum_p->value(), 0, 0), true);
	hide();
}


// Callback if user cancel Go To

CALL_BACK(GoTo_dialog, Cancel)
{
	hide();
}


// Code that calls constructor should then call this to tell
// us which editor instance to act upon.

void
GoTo_dialog::set_editor(Fl_Text_Editor * ed)
{
	editor_p = ed;
}


// Initialize contents on GoTo field to the current line number

void
GoTo_dialog::set_current_line()
{
	char num_as_string[16];
	// fltk numbers lines from 0, so add 1 to get what user expects
	(void) sprintf(num_as_string, "%d",
		editor_p->buffer()->count_lines(0, editor_p->insert_position()) + 1);
	linenum_p->value(num_as_string);
}


//------------------Edit class-----------------------------------------------
// Implements the items in the Edit menu on the main toolbar


Edit::Edit()
{
	find_p = 0;
	goto_p = 0;
	wrote_to_clipboard = false;
}


Edit::~Edit()
{
	if (find_p != 0) {
		delete find_p;
		find_p = 0;
	}
	if (goto_p != 0) {
		delete goto_p;
		goto_p = 0;
	}
}


//---Undo menu item---------------

CALL_BACK(Edit, Undo)
{
	buffer_p->undo();
}


//---Cut menu item---------------

CALL_BACK(Edit, Cut)
{
	Fl_Text_Editor::kf_cut('x', editor_p);
	set_can_paste();
}


//---Copy menu item---------------

CALL_BACK(Edit, Copy)
{
	Fl_Text_Editor::kf_copy('c', editor_p);
	set_can_paste();
}


//---Paste menu item---------------

CALL_BACK(Edit, Paste)
{
	Fl_Text_Editor::kf_paste('v', editor_p);
}


//---Delete menu item---------------

CALL_BACK(Edit, Delete)
{
	buffer_p->remove_selection();
	buffer_p->unselect();
}


//---Find menu item---------------

CALL_BACK(Edit, Find)
{
	if (find_p == 0) {
		// First time, create widget
		find_p = new Find_dialog();
		find_p->set_editor(editor_p);
	}
	find_p->as_Find();
	find_p->show();
}


//---Find Next menu item---------------

CALL_BACK(Edit, FindNext)
{
	if (find_p == 0 || strlen(find_p->get_pattern()) == 0) {
		// No pattern specified yet; turn into Find
		Find();
		return;
	}
	find_p->FindNext();
}


//---Replace menu item---------------

CALL_BACK(Edit, Replace)
{
	if (find_p == 0) {
		// First time, create widget
		find_p = new Find_dialog();
		find_p->set_editor(editor_p);
	}
	find_p->as_Replace();
	find_p->show();
}


//---Go To menu item---------------

CALL_BACK(Edit, GoTo)
{
	if (goto_p == 0) {
		// First time, create widget
		goto_p = new GoTo_dialog();
		goto_p->set_editor(editor_p);
	}
	goto_p->set_current_line();
	goto_p->show();
}


//---Select All menu item---------------

CALL_BACK(Edit, SelectAll)
{
	buffer_p->select(0, buffer_p->length());
}


//---- Callback for when editor window is modified.
// Grayed out find/replace should be ungrayed, because the modified
// text might now match.

void
Edit::modify_cb(int, int, int, int, const char *, void * data)
{
	if ( ((Edit *)data)->find_p != 0) {
		((Edit *)data)->find_p->change();
	}
}


// Class needs access to the editor; this lets it know which editor
// instance to use.

void
Edit::set_editor(Fl_Text_Editor * ed)
{
	editor_p = ed;
	buffer_p = editor_p->buffer();
}

// Ungray Paste button

void
Edit::set_can_paste(void)
{
	wrote_to_clipboard = true;
	editor_p->buffer()->call_modify_callbacks();
}

// Goto the specified line.

void
Edit::do_goto(Fl_Text_Editor * ed_p, int linenum, bool show_range_err)
{
	// Find end of valid range
	int last_line = ed_p->buffer()->count_lines(0,
			ed_p->buffer()->length());
	// FLTK line numbers start at 0, so we have to subtract 1 from
	// number supplied by user.
	int desired_line = linenum - 1;
	if (desired_line < 0 || desired_line > last_line) {
		if (show_range_err) {
			fl_alert("Line number out of range");
		}
		return;
	}

	// Find appropriate new cursor position and move cursor there
	int newposition = ed_p->buffer()->skip_lines(0, desired_line);
	ed_p->insert_position(newposition);
	ed_p->show_insert_position();
}
