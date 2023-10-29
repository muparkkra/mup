
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

// On Apple OS X, we set some magic path environment variables,
// so that Mupmate can be relocated and still find what it needs
// relative to wherever it gets moved.
// An OS X app has a particular
// directory structure. For Mupmate it looks like this:
//
//                       MupMate
//                          |
//           ---------------------------------
//           |              |                |
//     MupIncludes     MupMate.app       MupMusic
//			    |
//			Contents
//			    |
//     -----------------------------------------------------------
//     |                |           |           |                |
//  Info.plist        MacOS      PkgInfo    Resources    Resources Disabled
//                      |                       |
//                   MupMate                    |
//                                              |
//                                ------------------------------
//                                |             |              |
//                         MupMate*.icns       bin            doc
//                                              |              |
//                                             mup          packages
//                                                             |
//                                                            mup
//                                                             |
//                                                      -----------------
//                                                      |               |
//                                                    *.html, etc     uguide
//                                                                      |
//                                                              -------------
//                                                              |           |
//                                                             *.html    *.gif
//
// The original code was adapted from code provided by Michael Thies.
// A re-write was done in 7.0 to remove deprecated functions.
// For the moment, the old code is still here, but ifdef-ed out,
// so it could still be turned on if there are problems with the new.
//
// APPL is set to the top of the tree
// RSRC is set to $APPL/MupMate.app/Contents/Resources
// SUPP is set from
//   kApplicationSupportFolderType (old way)
//   NSApplicationSupportDirectory (new way)
// DOCS is set from
//   kDocumentsFolderType (old way)
//   NSDocumentDirectory (new way)
// HOME is set from
//   kCurrentUserFolderType (old way)
//  NSHomeDirectoryForUser(NSUserName())
//
// Note that
//   $APPL/MupMate.app/Contents/MacOS/MupMate
// is the actual Mupmate executable, and
//   $APPL/MupMate.app/Contents/Resources/bin/mup
// is the actual Mup executable that it calls.
//

#if defined (__APPLE__)
#include <string.h>
#include <limits.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

// The list of things we look up
enum MagicPath { MP_RSRC = 0, MP_APPL, MP_SUPP, MP_DOCS, MP_HOME };
static const char * magicPathTable[] =
{
        "RSRC",
        "APPL",
        "SUPP",
        "DOCS",
        "HOME",
        0
};

// This looks up the value of the given magic path, and returns it,
// or returns 0 if the path cannot be resolved.

static char *
getMagicPath (enum MagicPath mp)

{
#ifdef OLDWAY
	static char path[PATH_MAX];
	FSRef myFSRef;

	if (mp == MP_RSRC) {
		CFBundleRef myAppBundle = CFBundleGetMainBundle();
		if (myAppBundle == NULL) {
			return(0);
		}
		CFURLRef myBundleURL;
		myBundleURL = CFBundleCopyResourcesDirectoryURL(myAppBundle);
		if (myBundleURL == NULL) {
			return(0);
		}
		if (CFURLGetFileSystemRepresentation(myBundleURL, true, (UInt8*) path, sizeof(path)) == true) {
		}
		CFRelease(myBundleURL);
	}
	// $RSRC and $APPL are looked up in similar way
	if (mp == MP_RSRC || mp == MP_APPL) {

		// Get reference to the application bundle
		CFBundleRef myAppBundle = CFBundleGetMainBundle();
		if (myAppBundle == NULL) {
			return(0);
		}

		// Get the specific directory of interst inside the bundle
		CFURLRef myBundleURL;
		if (mp == MP_RSRC) {
			myBundleURL = CFBundleCopyResourcesDirectoryURL(myAppBundle);
		}
		else {
			myBundleURL = CFBundleCopyBundleURL(myAppBundle);
		}
		if (myBundleURL == NULL) {
			return(0);
		}

		Boolean ok;
		if (mp == MP_RSRC) {
               		ok = CFURLGetFSRef(myBundleURL, &myFSRef);
		}
		else {
			// parent folder of application bundle
			FSRef tmpFSRef;
			ok = CFURLGetFSRef(myBundleURL, &tmpFSRef);
			if (ok) {  // get parent folder
				ok = (FSGetCatalogInfo(&tmpFSRef, kFSCatInfoNone, 0, 0, 0, &myFSRef) == noErr);
			}
		}
		CFRelease(myBundleURL);
		if (!ok) {
			return 0;
		}
	}
	else {
		// Look up $SUPP, $DOCS, or $HOME
		OSType folderType;
		if (mp == MP_SUPP) {
			 folderType = kApplicationSupportFolderType;
		}
		else if (mp == MP_DOCS) {
			folderType = kDocumentsFolderType;
		}
		else {
			folderType = kCurrentUserFolderType;
		}
		if (FSFindFolder(kUserDomain, folderType, TRUE, &myFSRef)
								!= noErr) {
			return(0);
		}
	}

	// Translate the reference to a path. 
	if (FSRefMakePath(&myFSRef, (UInt8*) path, PATH_MAX) == noErr) {
		return(path);
	}
	return 0;
#else
	// This newer way attempts to use APIs which have been available
	// for a very long time for backward compatibility,
	// but without using ones which are deprecated.
	static char path[PATH_MAX];
	NSString *pathAsNSString = 0;

	// APPL and RSRC are looked up similarly
	if ( (mp == MP_RSRC) || (mp == MP_APPL) ) {
		// Get reference to the application bundle
		CFBundleRef myAppBundle = CFBundleGetMainBundle();
		if (myAppBundle == NULL) {
			return(0);
		}

		// Look up the value of interest.
		CFURLRef myBundleURL;
		if (mp == MP_RSRC) {
			myBundleURL = CFBundleCopyResourcesDirectoryURL(myAppBundle);
		}
		else {
			myBundleURL = CFBundleCopyBundleURL(myAppBundle);
		}
		if (myBundleURL == NULL) {
			return(0);
		}

		// Convert to C string, for passing to setenv().
		Boolean ok;
		ok = CFURLGetFileSystemRepresentation(myBundleURL, true,
						(UInt8*) path, sizeof(path));
		CFRelease(myBundleURL);
		if ( ! ok ) {
			return(0);
		}

		if (mp == MP_APPL) {
			// This historically got set to the parent directory of
			// the application, so we chop off the last component.
			char *lastSlash = strrchr(path, '/');
			if ((lastSlash != 0) && (lastSlash > path) &&
					(lastSlash < (path + sizeof(path) - 2))) {
				*(lastSlash+1) = '\0';
			}
		}
		return(path);
	}

	// The other three are done in somewhat similar ways,
	// using a function that returns the desired item as an NSString.
	if (mp == MP_DOCS) {
		NSArray *dirList = NSSearchPathForDirectoriesInDomains(
				NSDocumentDirectory, NSUserDomainMask, YES);
		if (dirList.count != 0) {
			pathAsNSString = [dirList objectAtIndex: 0];
		}
	}

	else if (mp == MP_SUPP) {
		NSArray *dirList = NSSearchPathForDirectoriesInDomains(
			NSApplicationSupportDirectory, NSUserDomainMask, YES);
		if (dirList.count != 0) {
			pathAsNSString = [dirList objectAtIndex: 0];
		}
	}

	else if (mp == MP_HOME) {
		pathAsNSString = NSHomeDirectoryForUser(NSUserName());
	}

	if (pathAsNSString == 0) {
		return(0);
	}

	// Convert to a C-style string, so we can pass it to setenv()
	strncpy(path, pathAsNSString.fileSystemRepresentation, sizeof(path));
	path[sizeof(path)-1] = '\0';
	return(path);
#endif
}


// This loops through the items to be looked up, and does setenv()
// on the values that are found.

void
initMagicPaths(void)

{
	int i;

	// Arrange to avoid "leak" messages on exit 
	void *autoreleasePool = [[NSAutoreleasePool alloc] init];

	for (i = 0; magicPathTable[i] != 0; i++) {

		// Only look up if not already an environment variable
		// by this name. $HOME in particular might well be set,
		// although probably to the same as what we will set it to.
		if (getenv(magicPathTable[i]) != 0) {
			continue;
		}

		size_t len;
		char *value;
		char *p = getMagicPath((MagicPath) i);

		if (p != 0 && *p != 0) {
			// Normalize path value to end with slash.
			// This shouldn't really be necessary,
			// but shouldn't hurt either, as extra slashes
			// are harmless.
			len = strlen(p);
			if (p[len - 1] != '/') {
				 len++;
			}
			value = (char *) malloc(len + 1);
			(void) strcpy(value, p);
			if (p[len - 1] != '/') {
				// append slash and terminator
				value[len - 1] = '/';
				value[len] = 0;
			}
		}
		else {
			// fallback default initialization
			value = strdup("./");
		}
		// We put into the environment, so that the code that
		// handles environment variable expansion will use them,
		// without having to know or care that they were "magic."
		setenv(magicPathTable[i], value, 0);
	}
	[(NSAutoreleasePool*)autoreleasePool release];
}
#endif
