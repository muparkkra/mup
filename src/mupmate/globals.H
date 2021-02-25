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

#ifndef _GLOBALS_H
#define _GLOBALS_H

// Try to consolidate OS-specific ifdef complicated clauses to this
// header file, so code can use descriptive ifdefs
#if defined(__WIN32) && ! defined(__CYGWIN__)
#define OS_LIKE_WIN32
#endif

#if defined(__linux) || defined(unix)
#define OS_LIKE_UNIX
#endif

#if defined(__APPLE__)
// Mac OS X is a special flavor of UNIX
// so simply treat it like UNIX at most places
#define OS_LIKE_UNIX
#define OS_LIKE_MACOSX
#endif

// We use a class static method for callbacks which cast their "void * data"
// argument to an instance of the class, and call the class method.
// So we use these macros to enforce consistent usage of that technique. 
// Most places will use CALL_BACK. The CALL_BACK_A is only needed in code
// when the object method takes an argument.
// Can't use CALLBACK as single word since under mingw that is already
// defined as something completely different.
#define CALL_BACK_A(classname, cb_name, arg) \
	void \
	classname::cb_name##_cb(Fl_Widget *, void * data) \
	{ \
		((classname *)data)->cb_name(); \
	} \
	void \
	classname::cb_name(arg)
#define CALL_BACK(classname, cb_name) CALL_BACK_A(classname, cb_name, void)

#endif
