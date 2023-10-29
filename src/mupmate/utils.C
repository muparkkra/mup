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

// This file contains code for miscellaneous things that don't seem to really
// belong with any particular menu.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "globals.H"
#include "utils.H"
#include <FL/fl_ask.H>
#include <FL/filename.H>
#ifdef OS_LIKE_WIN32
#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <ctype.h>
#include <sys/stat.h>
#endif

#if defined(__APPLE__)
extern void initMagicPaths(void);
#endif

// The FLTK Fl_Int_Input is almost what we want, but it allows
// octal and hex input via leading 0 and 0x respectively.
// So the Int_Input derived class intercepts the input and throws away
// characters from the set [xX] and leading zeros.
// Sometimes we also want to restrict to positive numbers,
// so the Positive_Int_Input class discards the - character as well.

Int_Input::Int_Input(int x, int y, int w, int h, const char * label)
	: Fl_Int_Input(x, y, w, h, label)
{
	allow_negative = true;
}


Positive_Int_Input::Positive_Int_Input(int x, int y, int w, int h, const char * label)
	: Int_Input(x, y, w, h, label)
{
	allow_negative = false;
}

int
Int_Input::handle(int event)
{
	if (event == FL_KEYBOARD) {
		int key = Fl::event_key();
		if (key == 'x' || key == 'X') {
			return(1);
		}
		if (key == '0' && position() == 0) {
			return(1);
		}
		if (key == '-' && ! allow_negative) {
			return(1);
		}
	}
	return(Fl_Int_Input::handle(event));
}



// Character used to separate items in $PATH and to separate directory names
#ifdef OS_LIKE_WIN32
	static const char path_sep = ';';
	static const char dir_sep = '\\';
#else
	static const char path_sep = ':';
	static const char dir_sep = '/';
#endif


// Return the native OS's directory separator character

char
dir_separator(void)
{
	return(dir_sep);
}

// Return the native OS's path separator character
char
path_separator(void)
{
	return(path_sep);
}

// Set the value of $MUPPATH. We "new" space for it and save a static
// pointer to that space.
// If it had been set before, we delete the old space.
// We use the value set in Preferences plus the value of $MUPPATH on
// entry to the program.

void
set_muppath(const char * new_muppath)
{
	// This is the original value of $MUPPATH on program startup
	static const char * orig_env_muppath = 0;
	// This is our most recent setting of MUPPATH
	static char * muppath = 0;

	// The first time we are called here, look up $MUPPATH
	if (orig_env_muppath == 0) {
		if ((orig_env_muppath = getenv("MUPPATH")) == 0) {
			// Set to empty string so we know we have looked it up
			orig_env_muppath = "";
		}
	}

	// If we had set a copy before, delete that copy
	if (muppath != 0) {
		delete(muppath);
	}

	// Need space for the new string plus MUPPATH= and null terminator
	int newlength = strlen(new_muppath) + 9;

	// If $MUPPATH was set on program entry, we want to append that.
	// If it wasn't set, this will add zero, which is correct.
	newlength += strlen(orig_env_muppath);

	// If there was both a non-empty  string being passed in
	// and a non-empty original $MUPPATH,
	// we need a delimiter between them. If either was a empty string ("")
	// then no delimiter is required.
	bool need_sep;
	if (new_muppath[0] != '\0' && orig_env_muppath[0] != '\0') {
		newlength += 1;
		need_sep = true;
	}
	else {
		need_sep = false;
	}

	// Get space for the new value and fill it in
	muppath = new char[newlength];
	if (need_sep) {
		(void) snprintf(muppath, newlength, "MUPPATH=%s%c%s",
			new_muppath, path_separator(), orig_env_muppath);
	}
	else {
		(void) snprintf(muppath, newlength, "MUPPATH=%s%s",
			new_muppath, orig_env_muppath);
	}
	(void) putenv(muppath);
}



// Given a path to a file in "location", and the length of that path,
// and a suffix, see if the location with the suffix added is an
// executable file. If so, return true, with the suffixed name left
// in location. Otherwise return false with location as it came in.
// Could also return false if path would be longer than FL_PATH_MAX,
// and therefore will not fit. (Better to fail than core dump.)

static bool
access_with_suffix(char * location, int length, const char * suffix)
{
	if (length + strlen(suffix) + 1 > FL_PATH_MAX) {
		// Too long to store
		return(false);
	}

	// Add suffix and see if it is an executable file
	(void) strcpy(location + length, suffix);
#ifdef OS_LIKE_WIN32
	/* On Vista, X_OK always returns false, even when
	 * file is a program that can be run */
	if(access(location, F_OK) == 0) {
#else
	if(access(location, X_OK) == 0) {
#endif
		return(true);
	}
	else {
		// This suffix didn't work. Remove it before returning
		location[length] = '\0';
		return(false);
	}
}


// Given a file location, see if it exists as an executable file,
// taking into account the DOS/Windows strangeness of implicit suffixes.
// $variables in the path will be expanded.

static bool
check_access(char * location)
{
	filename_expand(location, location);
	int len = strlen(location);
#ifdef __WIN32
	// If doesn't have a suffix, try with .com, .exe, and .bat suffix
	if (len < 5 || strchr(location + len - 4, '.') == 0) {
		// This is the precedence order for executable suffixes
		if (access_with_suffix(location, len, ".com")) {
			return(true);
		}
		if (access_with_suffix(location, len, ".exe")) {
			return(true);
		}
		if (access_with_suffix(location, len, ".bat")) {
			return(true);
		}
		return(false);
	}
	// If did have a suffix, go ahead and try name as is
#endif
	return access_with_suffix(location, len, "");
}


// Find the value of PATH. First try in third arg of main()
// since that seems more reliable on some OSs. Failing that, try getenv().

// We cache the value so we only have to search for it one time.
// This also rescues us in case env_p becomes invalid due to setting
// new environment variable values.

static const char * Path = 0;

void
get_path(const char ** const env_p)
{
	if (Path != 0) {
		// Already did it before
		return;
	}

	if (env_p != 0) {
		// Find $PATH in the environment variable list
		int e;
		for (e = 0; env_p[e] != 0; e++) {
			if (strncmp(env_p[e], "PATH=", 5) == 0) {
				Path = strdup(env_p[e] + 5);
				break;
			}
		}
	}
	if (Path == 0) {
		// Not found in the arge, so try looking up directly
		Path = getenv("PATH");
	}

#if defined (__APPLE__)
	// On Apple OS X, we look up some special magic paths,
	// so the MupMate.app can be relocated.
	initMagicPaths();
#endif
}


// Return true if given path is an absolute path

bool
is_absolute(const char * const path)
{
#ifdef OS_LIKE_WIN32
	if ((path[0] != '\0' && path[1] == ':') || path[0] == dir_sep) {
#else
	if (path[0] == dir_sep) {
#endif
		return(true);
	}
	return(false);
}


// Given the name of a executable program, find the directory from
// which it comes, and put the full path into "location,"
// which is expected to be at least FL_PATH_MAX bytes long.
// The incoming pgm_name is expected to be no more than FL_PATH_MAX long.
// It uses the components of PATH to try to find the executable.
// For Windows, if the program name doesn't have a suffix,
// it tries to find a .com, .exe, or .bat file with the pgm_name.
// It returns true on success. On failure, it returns false,
// and the contents of location are not defined.

bool
find_executable(const char * const pgm_name, char * location)
{
	// If pgm_name is already absolute path,
	// just check if it exists and is executable
	if (is_absolute(pgm_name)) {
		(void) strcpy(location, pgm_name);
		return (check_access(location));
	}

	if (Path == 0) {
		// Should have already looked up $PATH,
		// but make another attempt, just in case...
		get_path(0);
		if (Path == 0) {
			return(false);
		}
	}

	// We'll try the program name added to each PATH component
	// until we find it or have to give up.
#ifdef OS_LIKE_WIN32
	// DOS/Windows implicitly adds current working directory first
	bool add_implicit_cwd = true;
#else
	bool add_implicit_cwd = false;
#endif
	const char * component;		// current component of PATH
	const char * next_component;	// next component of PATH
	const char * sep_p;		// location of PATH separator
	int len;			// length of component
	for (component = Path; *component != '\0'; component = next_component) {
		if (add_implicit_cwd) {
			// DOS/Windows implicitly adds current directory
			// as first PATH component.
			len = 0;
			next_component = component;
			add_implicit_cwd = false;
		}

		else if ((sep_p = strchr(component, path_sep)) != 0) {
			// Not the last component in the PATH
			len = sep_p - component;
			next_component = sep_p + 1;
		}
		else {
			// Is the last component in the PATH
			len = strlen(component);
			next_component = component + len;
		}

		if (len == 0) {
			// Empty path component means current directory.
			// Allow enough room for directory separator,
			// pgm_name, suffix, and null terminator
			if (getcwd(location, FL_PATH_MAX
					- strlen(pgm_name) - 6) == 0) {
				// Current directory unobtainable or too long
				return(false);
			}
			len = strlen(location);
		}
		else {
			strncpy(location, component, len);
		}

		// If PATH component didn't already add a directory
		// separator, we add one. In some OSs it doesn't hurt
		// to add another, but no reason to use an extra byte.
		if (location[len-1] != dir_sep) {
			location[len++] = dir_sep;
		}

		// Now add the progam name itself and see if it exists.
		// The check_access() will add implied suffix if necessary.
		(void) strcpy(location + len, pgm_name);
		if (check_access(location)) {
			return(true);
		}
	}
	return(false);
}



#ifdef OS_LIKE_WIN32

// On Windows, we read the registry to try to determine the proper
// program to use for a given file type, like .mid or .ps files.
// This function will return the path to the appropriate file,
// if found, in a static area that may get overwritten on next call,
// so caller needs to make its own copy. If program is not found,
// returns null.


char *
lookup_pgm_for_file_suffix(const char * file_suffix)
{
	static char data[512];
	char name[512]; 
	long len = sizeof(data);
	// First find entry for file suffix mapping to a class
	(void) snprintf(name, sizeof(name), "Software\\Classes\\%s", file_suffix);
	HRESULT result = RegQueryValue(HKEY_LOCAL_MACHINE, name, data, &len);
	if (result != ERROR_SUCCESS) {
		return(0);
	}

	// Next look up the program associated with that class
	(void) snprintf(name, sizeof(name),
			"Software\\Classes\\%s\\shell\\open\\command", data);
	len = sizeof(data);
	result = RegQueryValue(HKEY_LOCAL_MACHINE, name, data, &len);
	if (result != ERROR_SUCCESS) {
		return(0);
	}

	// We might get multiple strings back,
	// giving the program itself plus arguments.
	// We only want the program itself. So if the first string is quoted,
	// strip the quotes and anything after it.
	char * d;
	if (*data == '"') {
		for (d = data + 1; *d != '\0'; d++) {
			if (*d == '"') {
				*(d-1) = '\0';
				break;
			}
			*(d-1) = *d;
		}
	}
	if (access(data, F_OK) == 0) {
		return(data);
	}
	return(0);
}


// Look up the given name in the CURRENT_USER area of registry
// and if found, fill in the data and return true, except return false.

static bool
reg_dir_found(char * name, char * data, DWORD len)
{
	HRESULT result = RegQueryValueEx(HKEY_CURRENT_USER, name, 0, 0,
					(LPBYTE)data, &len);
	if (result == ERROR_SUCCESS) {
		struct stat info;
		if (stat(data, &info) == 0 && (info.st_mode & S_IFDIR)) {
			return(true);
		}
	}
	return(false);
}


// Look for likely default folder for Mup files.
// Use the current user's "My Music" folder if there is one,
// otherwise try their "Personal" folder.
// Returns path in static area or null on failure.

char *
find_music_folder(void)
{
	static char best_value[FL_PATH_MAX];

	// Get the registry info about folders
	HKEY key = 0;
	// Win'98 uses "User Shell Folders" but newer versions use just
	// "Shell Folders," so we check for both, newer first, since that
	// is probably more likely to work.
	if ((RegOpenKeyEx(HKEY_CURRENT_USER,
			"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
			0, KEY_READ, &key) == ERROR_SUCCESS) ||
			(RegOpenKeyEx(HKEY_CURRENT_USER,
			"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders",
			0, KEY_READ, &key) == ERROR_SUCCESS)) {
		DWORD max_key_length;
		DWORD count;
		DWORD max_name_length;
		DWORD max_value_length;
		// Find out how many subkeys there are, and max lengths.
		if (RegQueryInfoKey(key, 0, 0, 0, 0, &max_key_length, 0, &count,
				&max_name_length, &max_value_length, 0, 0)
				== ERROR_SUCCESS) {
			TCHAR name[max_name_length + 1];
			DWORD name_length;
			DWORD value_type;
			BYTE value[max_value_length + 1];
			DWORD value_length;
			int i;
			best_value[0] = '\0';
			// Look for "My Music" and "Personal" subkeys.
			// There's probably a better way to query for specific
			// subkey than linear search, but this works...
			for (i = 0; i < count; i++) {
				name_length = sizeof(name);
				value_length = sizeof(value);
				if (RegEnumValue(key, i, name, &name_length, 0,
						&value_type, value,
						&value_length)
						== ERROR_SUCCESS) {
					if (value_type != REG_SZ) {
						continue;
					}
					if (strcasecmp(name, "My Music") == 0) {
						// Found the ones we want.
						strcpy(best_value, (char *) value);
						break;
					}
					if (strcasecmp(name, "Personal") == 0) {
						// This is our second choice.
						// Save as best so far.
						strcpy(best_value, (char *) value);
					}
				}
			}
		}
	}
	if (key != 0) {
		RegCloseKey(key);
	}
	return(best_value[0] == '\0' ? 0 : best_value);
}

#endif

// This function is a wrapper for fl_filename_expand().  For Windows,
// fl_filename_expand returns a path using '/' to separate directories.
// This function restores the Windows separator '\'.

void
filename_expand(char *expanded_path, const char *given_path)
{
	(void)fl_filename_expand(expanded_path, given_path);
#ifdef OS_LIKE_WIN32
	for (int n = 0; expanded_path[n] != '\0'; n++){
		if (expanded_path[n] == '/') {
			expanded_path[n] = dir_separator();
		}
	}
#endif
}

#ifdef OS_LIKE_WIN32

// Add the given additional_bin to $PATH,
// using the initial len bytes of it.

static void
add_to_path(char *additional_bin, int len)

{
	char *old_path;
	char *new_path;

	// Look up the current $PATH value
	if ((old_path = getenv("PATH")) == 0) {
		// No PATH; don't bother
		return;
	}
	// Truncate to just the specified len, to get just the directory
	additional_bin[len] = '\0';

	// Allocate space for new PATH
	int newlen = strlen(old_path) + len + 7;
	new_path = new char[newlen];
	// Create the new PATH
	snprintf(new_path, newlen, "PATH=%s%c%s", old_path,
				path_separator(), additional_bin);

	// Set the new PATH
	putenv(new_path);
}


// Check if gswin32c or gswin64c exists at the candidate location.
// The size is the total length of the candidate_location array.
// The len is where to append the gswinNNc for checking.
// Returns true if was found, false if not.

static int
check_gswin_location(char *candidate_location, int size, int len)

{
	// Try the location with gswin32 appended
	strncpy(candidate_location + len, "gswin32c",
				size - len - 1);
	candidate_location[len + 8] = '\0';
	if (check_access(candidate_location)) {
		// Found it
		add_to_path(candidate_location, len - 1);
		return(true);
	}

	// Try with gswin64c
	candidate_location[len+5] = '6';
	candidate_location[len+6] = '4';
	if (check_access(candidate_location)) {
		// Found it
		add_to_path(candidate_location, len - 1);
		return(true);
	}
	return(false);
}


// With recent versions of Ghostscript, ps2pdf may not be able to find
// gswin32c or gswin64c. So this tries to deduce where it likely is,
// and if found, adds the location to PATH.
// If we don't find it, we just go on and hope for the best.

void
find_gswinNNc(const char * const ps2pdf_location)

{
	static bool already_called = false;

	if (already_called) {
		return;
	}
	already_called = true;

	char gswin_location[FL_PATH_MAX];
	gswin_location[0] = '\0';
	if ( ! find_executable("gswin64c", gswin_location)) {
		if ( ! find_executable("gswin32c", gswin_location) ) {
			gswin_location[0] = '\0';
		}
	}
	if (gswin_location[0] != '\0') {
		// Was already found in the PATH; no need to deduce.
		return;
	}

	// Neither gswin32c or gswin64c was found in the PATH
	// Try looking in same directory as ps2pdf. (This may work if
	// user gave full path, but it wouldn't have been found via PATH.)
	char *last_sep;
	if ((last_sep = strrchr(ps2pdf_location, dir_separator())) == 0) {
		// Have to give up looking
		return;
	}

	// Create the path to be checked
	char candidate_location[FL_PATH_MAX];
	int len;
	len = last_sep - ps2pdf_location + 1;
	strncpy(candidate_location, ps2pdf_location, len); 
	candidate_location[len] = '\0';

	if (check_gswin_location(candidate_location, sizeof(candidate_location),
							len)) {
		// Found it
		return;
	}

	// If the last component was not "bin," try backing up one level
	// and seeing if there is in a bin directory at the level,
	// and if so, try looking in there.
	// This is the place most likely to work.
	candidate_location[len-1] = '\0';
	if ((last_sep = strrchr(candidate_location, dir_separator())) == 0) {
		// Have to give up looking
		return;
	}
	if (strcmp(last_sep + 1, "bin") == 0) {
		// Already was bin, so no help 
		return;
	}
	len = last_sep - candidate_location + 1;
	strncpy(candidate_location + len, "bin",
					sizeof(candidate_location) - len - 1);
	len += 3;
	candidate_location[len++] = dir_separator();
	candidate_location[len] = '\0';
	(void) check_gswin_location(candidate_location,
					sizeof(candidate_location), len);
}
#endif
