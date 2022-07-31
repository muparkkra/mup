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

#ifndef _EDIT_H_
#define _EDIT_H_

// Classes for Edit menu item off of main toolbar

#include <FL/Fl_Widget.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Return_Button.H>



// Class for window that pops up for "Find" or "Replace"

class Find_dialog : public Fl_Double_Window {

friend class Edit; 	// so it can call FindNext()

public:
	Find_dialog(void);
	~Find_dialog();

	// Callbacks
	static void FindNext_cb(Fl_Widget *, void * data);
	static void Replace_cb(Fl_Widget *, void * data);
	static void ReplaceAll_cb(Fl_Widget *, void * data);
	static void Cancel_cb(Fl_Widget *, void * data);
	static void Pattern_cb(Fl_Widget *, void * data);
	static void change_cb(Fl_Widget *, void * data);
	static void cursor_change_check(void * data);

	// Returns current search pattern
	const char * get_pattern(void);

	// Tells class which edit buffer to search in
	void set_editor(Fl_Text_Editor *);

	// Class instance can be either a Find or a Replace.
	// These change the personality.
	void as_Find(void);
	void as_Replace(void);

	// when not allowed to search/replace
	void gray_out(void);

private:
	// Callbacks
	void FindNext(void);
	void Replace(void);
	void ReplaceAll(void);
	void Cancel(void);
	void Pattern(void);
	void change(void);

	// Widgets
	Fl_Input * pattern_p;
	Fl_Input * replace_with_p;
	Fl_Button * replace_p;
	Fl_Button * replace_all_p;
	Fl_Check_Button * casematch_p;
	Fl_Box * direction_p;
	Fl_Round_Button * up_p;
	Fl_Round_Button * down_p;
	Fl_Return_Button * next_p;
	Fl_Button * cancel_p;

	// The text editor widget we are editing
	Fl_Text_Editor * editor_p;

	// true if current personality is "Replace," false if "Find."
	bool is_replace;

	// Where cursor was at last check
	int last_cursor_position;
};


// Class for window that pops up for Go To

class GoTo_dialog : Fl_Double_Window {

friend class Edit;

public:
	GoTo_dialog(void);
	~GoTo_dialog();

	// Callbacks
	static void OK_cb(Fl_Widget *, void * data);
	static void Cancel_cb(Fl_Widget *, void * data);

	// Tells us which editor instance to use
	void set_editor(Fl_Text_Editor * ed);

	// Initialize contents on GoTo field to the current line number
	void set_current_line();

private:
	// Callbacks
	void OK(void);
	void Cancel(void);

	// Widgets
	Fl_Int_Input * linenum_p;
	Fl_Return_Button * ok_p;
	Fl_Button * cancel_p;
	Fl_Text_Editor * editor_p;
};


// Class for Edit Menu from main toolbar

class Edit {
public:
	Edit();
	~Edit();

	// Callbacks
	static void Undo_cb(Fl_Widget *, void * data);
	static void Cut_cb(Fl_Widget *, void * data);
	static void Copy_cb(Fl_Widget *, void * data);
	static void Paste_cb(Fl_Widget *, void * data);
	static void Delete_cb(Fl_Widget *, void * data);
	static void Find_cb(Fl_Widget *, void * data);
	static void FindNext_cb(Fl_Widget *, void * data);
	static void Replace_cb(Fl_Widget *, void * data);
	static void GoTo_cb(Fl_Widget *, void * data);
	static void SelectAll_cb(Fl_Widget *, void * data);
	static void modify_cb(int, int, int, int, const char *, void * data);

	// Move cursor to specified line.
	// If show-range_err is true, show error message if line is out
	// of range. 
	static void do_goto(Fl_Text_Editor * ed_p, int linenum, bool show_range_err);

	// Tells us which editor instance to use
	void set_editor(Fl_Text_Editor * ed);

	// true if there is something in the clipboard that can be pasted
	bool can_paste()	{ return wrote_to_clipboard; }
	void set_can_paste();

private:
	// Callbacks
	void Undo(void);
	void Cut(void);
	void Copy(void);
	void Paste(void);
	void Delete(void);
	void Find(void);
	void FindNext(void);
	void Replace(void);
	void GoTo(void);
	void SelectAll(void);

	// Widgets
	Fl_Text_Editor * editor_p;
	Fl_Text_Buffer * buffer_p;
	Find_dialog * find_p;
	GoTo_dialog * goto_p;

	// if wrote something to the cut/copy buffer
	bool wrote_to_clipboard;
};

#endif
