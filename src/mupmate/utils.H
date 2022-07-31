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

#ifndef _UTILS_H_
#define _UTILS_H_

// This file defines miscellaneous things that don't seem to really
// belong with any particular menu.

#include <FL/Fl.H>
#include <FL/Fl_Int_Input.H>

#ifdef OS_LIKE_WIN32
// For some reason mingw doesn't seem to properly define va_list anywhere.
// I guess it needs a compiler-specific header file.
// We don't actually use any functions that use va_list,
// so just need to try to keep the compiler happy.
#ifndef va_list
#define va_list __VALIST
#endif
#endif

// The FLTK Fl_Int_Input is almost what we want, but it allows
// octal and hex input via leading 0 and 0x respectively.
// So the Int_Input derived class intercepts the input and throws away
// characters from the set [xX] and leading zeros.
// Sometimes we also want to restrict to positive numbers,
// so the Positive_Int_Input class discards the - character as well.

class Int_Input : public Fl_Int_Input {
public:
	Int_Input(int x, int y, int w, int h, const char * label);
	int handle(int event);
protected:
	bool allow_negative;
};

class Positive_Int_Input : public Int_Input {
public:
	Positive_Int_Input(int x, int y, int w, int h, const char * label);
};


// Return the native OS's directory separator character
char dir_separator(void);
// Return the native OS's path separator character
char path_separator(void);

// Set the $MUPPATH value
extern void set_muppath(const char * new_muppath);

// Get value of PATH from third argument to main(), or failing that,
// from getenv(), and save it.
extern void get_path(const char ** const env_p);

// Return true if given path is an absoluate path
extern bool is_absolute(const char * const path);

// Find full path to an executable.
// If found, true is returned and full path is filled into location,
// which must be at least FL_PATH_MAX bytes long.
// If executable is not found, false is returned, and the contents of
// location is not defined.
extern bool find_executable(const char * const pgm_name, char * location);

// wrapper for fl_filename_expand that corrects directory separators
extern void filename_expand(char *expanded_path, const char *given_path);

#ifdef OS_LIKE_WIN32
// On Windows, we read the registry to try to determine the proper
// program to use for a given file type, like .mid or .ps files.
// This function will return the path to the appropriate file,
// if found, in a static area that may get overwritten on next call,
// so caller needs to make its own copy. If program is not found,
// returns null.
extern char * lookup_pgm_for_file_suffix(const char * file_suffix);

// Look for likely default directory for Mup files.
// Use the current user's "My Music" place if there is one,
// otherwise try their "My Documents" folder.
// Returns path in static area or null on failure.
extern char * find_music_folder(void);

// ps2pdf may not be able to find gswin32c (or gswin64c) in PATH.
// This tries to deduce where it is, and adds to PATH if found.
extern void find_gswinNNc(const char * const ps2pdf_location);

#endif

#endif
