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

#ifndef _HELP_H_
#define _HELP_H_

// Classes for Help menu item off of main toolbar

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/Fl_Help_View.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Text_Display.H>
#include <FL/filename.H>
#include "Config.H"


// Class for window to let user browse the HTML version of Mup User's Guide

class Uguide_browser : Fl_Double_Window {

friend class Help;

public:
	Uguide_browser(const int width, const int height);
	~Uguide_browser(void);

	// User can change font/size preference. This callback notifies
	// of such changes, so we can adjust the output accordingly 
	static void font_change_cb(void * data, Fl_Font font, unsigned char size);

	// Load the User's Guide top level index.html file
	void load_uguide(void);

private:
	void font_change(Fl_Font font, unsigned char size);

	// On Windows, Fl_Help_View doesn't seem to properly follow relative
	// URLs, so we use the link() callback to prepend the proper directory.
	static const char * resolve_link(Fl_Widget *, const char * uri);

	Fl_Help_View *browser_p;
	Font_change_registration * font_change_reg_p;
	bool loaded;
};


// Class to give first time user some hints.

class StartupHints : Fl_Double_Window {

friend class Help;

public:
	StartupHints(void);
	~StartupHints(void);

	static void OK_cb(Fl_Widget *, void * data);
	// User can change font/size preference. This callback notifies
	// of such changes, so we can adjust the output accordingly 
	static void font_change_cb(void * data, Fl_Font font, unsigned char size);
private:
	void OK(void);
	void font_change(Fl_Font font, unsigned char size);

	Font_change_registration * font_change_reg_p;
	Fl_Text_Display * text_p;	// The text of the hints
	Fl_Return_Button * OK_p;
};


// Class for "About" window

class About_dialog : Fl_Double_Window {

friend class Help;

public:
	About_dialog(void);
	~About_dialog(void);

	static void OK_cb(Fl_Widget *, void * data);

private:
	void OK(void);

	Fl_Multiline_Output * message_p;
	Fl_Return_Button * ok_p;
};


// Class for showing License

class License_Display : public Fl_Double_Window {

friend class Help;

public:
	License_Display(void);
	~License_Display(void);

	static void font_change_cb(void * data, Fl_Font font, unsigned char size);
	static void OK_cb(Fl_Widget *, void * data);
private:
	void OK(void);
	void font_change(Fl_Font font, unsigned char size);

	Font_change_registration * font_change_reg_p;
	Fl_Text_Display * text_p;
	Fl_Return_Button * ok_p;
};


// Class for Help menu item from main toolbar

class Help {
public:
	Help(void);
	~Help(void);

	static void Uguide_cb(Fl_Widget *, void * data);
	static void Startup_Hints_cb(Fl_Widget *, void * data);
	static void About_cb(Fl_Widget *, void * data);
	static void License_cb(Fl_Widget *, void * data);

private:
	void Uguide(void);
	void Startup_Hints(void);
	void About(void);
	void License(void);

	About_dialog * about_p;
	StartupHints * startup_hints_p;
	Uguide_browser * uguide_p;
	License_Display * license_p;
};

#endif
