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

// This file contains code for defining and using things
// related to user configuration settings and preferences.

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "globals.H"
#include "utils.H"
#include "Preferences.H"
#include <FL/filename.H>
#include <FL/fl_ask.H>

// Access to the user preferences file
Fl_Preferences * Preferences_p;

// Preference names and default values
const char * const Editor_font_preference = "editor_font";
const char * const Default_editor_font = "Courier";

const char * const Editor_size_preference = "editor_size";
const char * const Default_editor_size = "12";

const char * const Auto_display_preference = "auto_display";
const int Default_auto_display = 0;

const char * const Auto_save_preference = "auto_save";
const int Default_auto_save = 1;

const char * const Tooltips_delay_preference = "tooltips_delay";
const double Default_tooltips_delay = 1.0;

const char * const Mup_program_location = "Mup_program";
#if defined(OS_LIKE_MACOSX)
const char * const Default_Mup_program_location = "$RSRC/bin/mup";
#else
const char * const Default_Mup_program_location = "mup";
#endif

const char * const Mup_documentation_location = "Mup_documentation";
#if defined(__WIN32)
const char * const Default_Mup_documentation_location = "C:\\Program Files\\mupmate";
#elif defined(OS_LIKE_MACOSX)
const char * const Default_Mup_documentation_location = "$RSRC/doc/packages/mup";
#else
const char * const Default_Mup_documentation_location = "/usr/share/doc/packages/mup";
#endif
// Debian, and possibly others, use /usr/share/doc/mup without the "packages"
const char * const Alternate_Mup_documentation_location = "/usr/share/doc/mup";

const char * const Music_files_location = "Music_folder";
#if defined(OS_LIKE_MACOSX)
const char * const Default_music_files_location = "$APPL/MupMusic";
#else
const char * const Default_music_files_location = ".";
#endif

const char * const MUPPATH_location = "MUPPATH";
#if defined(OS_LIKE_MACOSX)
const char * const Default_MUPPATH_location = "$APPL/MupIncludes";
#else
const char * const Default_MUPPATH_location = ".";
#endif

const char * const Viewer_location = "Viewer";
#if defined(__WIN32)
const char * const Default_viewer_location = "C:\\Program Files\\Ghostgum\\gsview\\gsview32.exe";
#elif defined(OS_LIKE_MACOSX)
const char * const Default_viewer_location = "open";
#else
const char * const Default_viewer_location = "gv";
#endif

const char * const MIDI_player_location = "MIDI_player";
#if defined(__WIN32)
const char * const Default_MIDI_player_location = "c:\\Program Files\\Windows Media Player\\wmplayer.exe";
#elif defined(OS_LIKE_MACOSX)
const char * const Default_MIDI_player_location = "open";
#else
const char * const Default_MIDI_player_location = "xplaymidi";
#endif

const char * const PS_to_PDF_converter_location = "PS_to_PDF_converter";
const char * const Default_PS_to_PDF_converter_location = "ps2pdf";

const char * const Showed_startup_hints = "showed_startup_hints";
const int Default_startup_hints_flag = 0;

const char * const Migration_status = "migration_status";
const int Default_migration_status = Not_migrated;

const char * const Main_window_width = "main_window_width";
const char * const Main_window_height = "main_window_height";
const char * const Help_window_width = "help_window_width";
const char * const Help_window_height = "help_window_height";

// Name of User's Guide directory and index file
// relative to Mup documentation directory.
const char * uguide_directory = "uguide";
const char * uguide_filename = "index.html";

// Don't use un-readable tiny font, but especially avoid size of zero,
// which could happen if preferences file contains a bad number, so that
// atoi() returns zero. Keep max small enough to fit reasonable number
// of characters in a window.
const unsigned char Min_size = 5;
const unsigned char Max_size = 30;

// Minimum and default sizes for Main and Help browser windows.
const int Min_width = 600;
const int Default_width = 720;
const int Min_height = 400;
const int Default_height = 480;


// If user hasn't specified any program yet to use for PostScript viewer
// and/or MIDI file player, we try to deduce what they have available
// that could be used, and set one of those as the default.

// List of likely programs to use as PostScript viewers
static const char * const viewer_candidates[] = {
#if defined(__WIN32)
	"GSview32",
	"GSview64",
	"GSview",
#else
#ifdef OS_LIKE_UNIX
	"gv",
	"ghostview",
#if defined(OS_LIKE_MACOSX)
	"open",
#endif
#endif
#endif
	0
};

// List of likely programs to use as MIDI file players
static const char * const player_candidates[] = {
#if defined(__WIN32)
	"wmplayer",
	"mplayer",
#else
#ifdef OS_LIKE_UNIX
	"xplaymidi",
	"timidity",
	"pmidi",
	"playmidi",
#if defined(OS_LIKE_MACOSX)
	"open",
#endif
#endif
#endif
	0
};

static const char * const ps_to_pdf_converter_candidates[] = {
	"ps2pdf",
	"pstopdf",
	0
};


// If the default preference value for a program location is no good,
// try to find where it is.
// pgm_location = the preference name of the program of interest
// default_location = the default path to the program
// file_suffix = .ps or .mid or .mup or null
// Returns true if the preference value was updated.

static bool
deduce_location(const char * pgm_location, const char * default_location,
			const char * file_suffix, const char * const * candidates)
{
	char * place;			// a path to try for finding the pgm
	char location[FL_PATH_MAX];	// full path to pgm when it is found

	if (Preferences_p->get(pgm_location, place, default_location) != 0) {
		// There was a value already set. Make sure it is good.
		if (find_executable(place, location)) {
			if (strcmp(place, location) != 0) {
				// Must have started as relative path
				// and now we know the full path.
				(void) Preferences_p->set(pgm_location, location);
				return(true);
			}
			else {
				// Existing setting was okay as is.
				return(false);
			}
		}
	}

#ifdef OS_LIKE_WIN32
	// Try looking in the registry for what to use for
	// files of this type.
	if ((file_suffix != 0) && ((place = lookup_pgm_for_file_suffix(file_suffix)) != 0)) {
		(void) Preferences_p->set(pgm_location, place);
		return(true);
	}
#endif

	// If there had been nothing set, see if the default location is good.
	if (find_executable(default_location, location)) {
		(void) Preferences_p->set(pgm_location, location);
		return(true);
	}

	// Try looking in the various educated guess locations.
	if (candidates != 0) {
		int i;
		for (i = 0; candidates[i] != 0; i++) {
			if (find_executable(candidates[i], location)) {
				(void) Preferences_p->set(pgm_location, location);
				return(true);
			}
		}
	}

	return(false);
}


// Returns true is the Mup User's Guide can be found at the given location.

static bool
doc_found(const char * location)
{
	return(access(users_guide_index_file(location), F_OK) == 0);
}


// Try to deduce where the Mup documentation is.
// Returns true if updated the Preferences to point to new location.

static bool
deduce_documentation_location(const char * doc_location, const char * default_doc_location)
{
	char * place;
	// First try the stored location, if any
	if (Preferences_p->get(doc_location, place, default_doc_location) != 0) {
		if (doc_found(place)) {
			// Existing preference is good. No update needed.
			return(false);
		}
	}

	// Try default location 
	if (doc_found(default_doc_location)) {
		Preferences_p->set(doc_location, default_doc_location);
		return(true);
	}

	// Try alternate location
	if (doc_found(Alternate_Mup_documentation_location)) {
		Preferences_p->set(doc_location, Alternate_Mup_documentation_location);
		return(true);
	}

	// Try relative to where Mup executable is.
	char *muploc;
	(void) Preferences_p->get(Mup_program_location, muploc,
					Default_Mup_program_location);
	char * basename_p;
	if ((basename_p = strrchr(muploc, dir_separator())) != 0) {
		int len = basename_p - muploc;
		char mupdir[len + 1];
		(void) strncpy(mupdir, muploc, len);
		mupdir[len] = '\0';
		if (doc_found(mupdir)) {
			Preferences_p->set(doc_location, mupdir);
			return(true);
		}

		// Try at ../docs from where mup executable was,
		// since it could be there from unpacking tar file.
		char * parentdir_p;
		if ((parentdir_p = strrchr(mupdir, dir_separator())) != 0) {
			len = parentdir_p - mupdir + 1;
			// Add room for doc and terminator
			char mupdocdir[len + 5];
			(void) strncpy(mupdocdir, muploc, len);
			(void) strcpy(mupdocdir + len, "docs");
			if (doc_found(mupdocdir)) {
				Preferences_p->set(doc_location, mupdocdir);
				return(true);
			}
		}
	}
	return(false);
}


// This function attempts to find the locations of what we need,
// like a PostScript viewer and MIDI player.

void
deduce_helper_locations(void)
{
	bool updated = false;

	// First try to find a PostScript viewer
	if (deduce_location(Viewer_location, Default_viewer_location, ".ps",
					viewer_candidates)) {
		updated = true;
	}

	// Next do MIDI player
	if (deduce_location(MIDI_player_location, Default_MIDI_player_location,
				".mid", player_candidates)) {
		updated = true;
	}

	// PostScript to PDF converter
	if (deduce_location(PS_to_PDF_converter_location, Default_PS_to_PDF_converter_location,
				0, ps_to_pdf_converter_candidates)) {
		updated = true;
	}

	// Find the Mup command itself
	if (deduce_location(Mup_program_location, Default_Mup_program_location, ".mup", 0)) {
		updated = true;
	}

	if (deduce_documentation_location(Mup_documentation_location,
				Default_Mup_documentation_location)) {
		updated = true;
	}

#ifdef OS_LIKE_WIN32
	// Try to guess a good default place for Mup input files.
	char * place;
	if (Preferences_p->get(Music_files_location, place,
					Default_music_files_location) == 0) {
		if ((place = find_music_folder()) != 0) {
			Preferences_p->set(Music_files_location, place);
			updated = true;
		}
	}
	// The same place is probably a good guess for Mup include files.
	if (Preferences_p->get(MUPPATH_location, place,
					Default_MUPPATH_location) == 0) {
		if ((place = find_music_folder()) != 0) {
			Preferences_p->set(MUPPATH_location, place);
			updated = true;
		}
	}
#endif

	// If at least one better choice was found, update the persistent data.
	if (updated) {
		Preferences_p->flush();
	}
}


// Given a path to Mup's documentation directory,
// add on the name of the User's Guide index file.
// Return that in a static area.

const char *
users_guide_index_file(const char * const doc_dir)
{
	static char file_path[FL_PATH_MAX];
	char expanded_doc_dir[FL_PATH_MAX];

	filename_expand(expanded_doc_dir, doc_dir);
	if (snprintf(file_path, sizeof(file_path),
			"%s%c%s%c%s", expanded_doc_dir,
			dir_separator(), uguide_directory,
			dir_separator(), uguide_filename) > (int) sizeof(file_path)) {
		fl_message("Path too long; truncated");
	}
	return((const char *) file_path);
}


// List of old versions of Mupmate preference files to potentially check,
// listed most recent to oldest, and ending with null.
const char * const oldversions [] = {
	"mupmate70",
	"mupmate69",
	"mupmate681",
	"mupmate68",
	"mupmate67",
	"mupmate66",
	"mupmate65",
	"mupmate64",
	"mupmate63",
	"mupmate62",
	"mupmate61",
	"mupmate60",
	"mupmate58",
	"mupmate57",
	"mupmate56",
	"mupmate55",
	"mupmate55beta",
	"mupmate54",
	"mupmate54beta",
	"mupmate",
	0
};

// If preference was set in old preferences file, copy to current
#define COPY_PREF(value_var, value_name, value_default) \
	if (old_preferences_p->get(value_name, value_var, value_default) != 0) { \
		Preferences_p->set(value_name, value_var); \
	}

void
migrate_preferences( )
{
	int migrated;
	(void) Preferences_p->get(Migration_status, migrated, Default_migration_status);
	if (migrated != Migration_complete) {

		Fl_Preferences * old_preferences_p;
		int i;
		for (i = 0; oldversions[i] != 0; i++) {

			old_preferences_p = new Fl_Preferences(
					Fl_Preferences::USER,
                                        "arkkra.com", oldversions[i]);

			if (old_preferences_p->entries() == 0) {
				// No preferences for this old release,
				// but continue looking for even older
				// release, in case they skipped.
				delete old_preferences_p;
				continue;
			}

			// Copy old preferences, unless they are things
			// that might be different for each release.
			// We need to know the type of the preference value,
			// to know what type to pass to the get() method,
			// so we can't just loop through whatever is there.
			int intval;
			char * stringval;
			double doubleval;

			COPY_PREF(stringval, Editor_font_preference,
						Default_editor_font);

			// Note that size is stored as string, although it
			// will always be a string representation of an int
			COPY_PREF(stringval, Editor_size_preference,
						Default_editor_size);

			COPY_PREF(intval, Auto_display_preference,
						Default_auto_display);

			COPY_PREF(intval, Auto_save_preference,
						Default_auto_save);

			COPY_PREF(doubleval, Tooltips_delay_preference,
						Default_tooltips_delay);

			COPY_PREF(stringval, Music_files_location,
						Default_music_files_location);

			COPY_PREF(stringval, MUPPATH_location,
						Default_MUPPATH_location);

			COPY_PREF(stringval, Viewer_location,
						Default_viewer_location);

			COPY_PREF(stringval, MIDI_player_location,
						Default_MIDI_player_location);

			COPY_PREF(stringval, PS_to_PDF_converter_location,
						Default_PS_to_PDF_converter_location);

			COPY_PREF(intval, Showed_startup_hints,
						Default_startup_hints_flag);

			// We found a version to upgrade from,
			// so no need to look further.
			delete old_preferences_p;
			// Mark that we migrated data, so we won't do again,
			// but also mark to give startup hints for migration.
			Preferences_p->set(Migration_status, Data_migrated);
			return;
		}
		// Didn't find anything to migrate from,
		// so we have completed migrating the nothing to migrate.
		Preferences_p->set(Migration_status, Migration_complete);

	}
}
