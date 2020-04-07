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
/*
 *	defines.h
 *
 *	This file defines the constants needed for the music publication
 *	program.
 */

#include <math.h>
#ifndef _DEFINES
#define _DEFINES
#include "muschar.h"

/*
 * Define the environment =============================================
 */

/* for all forms of DOS we've tried, make sure __DOS__ is defined */
#if defined(__TURBOC__) || defined(__WATCOMC__) || defined(__MINGW32__)
#ifndef __DOS__
#define __DOS__
#endif
#endif

/* if you are short on memory or know you'll never use the extended character
 * set (with accented characters, etc) you can undefine EXTCHAR
 */
#ifndef __TURBOC__
#define EXTCHAR
#endif

/* the following is needed for the DOS port of the GNU C compiler */
#ifdef __DJGPP__
#ifndef __DOS__
#define __DOS__
#endif
#undef unix
#endif

/*
 * Based on system type, #define or don't #define various symbols, as follows:
 *
 * UNIX_LIKE_FILES	If defined, files can be simultaneously read from and
 *			written to and invisibly unlinked.
 * UNIX_LIKE_PATH_RULES	If defined, use these rules for MUPPATH.
 * DOS_LIKE_PATH_RULES	If defined, use these rules for MUPPATH.
 * CORE_MESSAGE		If defined, then a creating core error message will be
 *			printed in debug mode.
 * NEED_GETOPT		If defined, the getopt suite is not available,
 *			therefore use the limited one in main.c.
 * OPTION_MARKER	The char which precedes command line options.
 */
#ifdef unix
#define UNIX_LIKE_FILES
#define	UNIX_LIKE_PATH_RULES
#define CORE_MESSAGE
#define OPTION_MARKER	'-'
#endif

#ifdef VMS
#define OPTION_MARKER	'-'
#endif

#ifdef AMIGA
#undef	UNIX_LIKE_FILES
#define NEED_GETOPT
#define OPTION_MARKER	'-'
#endif

#ifdef __DOS__
#define	DOS_LIKE_PATH_RULES
#define NEED_GETOPT
#define OPTION_MARKER	'/'
#endif

#ifdef Mac_BBEdit
#undef  UNIX_LIKE_FILES
#define NEED_GETOPT
#define OPTION_MARKER	'-'
#endif

#ifdef __EMX__
#undef	UNIX_LIKE_FILES
#define	DOS_LIKE_PATH_RULES
#define OPTION_MARKER	'-'
#endif

#ifndef OPTION_MARKER
/* use - as default if nothing has been defined for it */
#define OPTION_MARKER '-'
#endif

/*
 * Define ranges for variables =============================================
 */

/*
 * Define maximum value for exit code from Mup. Must be less than 128
 * to be able to distinguish from WAIT_ABANDONED value on Windows.
 */
#define	MAX_ERRORS	(127)

/* text point sizes */
#define MINSIZE	(1)
#define MAXSIZE	(100)

/* key signatures */
#define MINSHARPS	(-7)
#define MAXSHARPS	(7)
#define DEFSHARPS	(0)

/* octave */
#define MINOCTAVE	(0)
#define MAXOCTAVE	(9)
#define DEFOCTAVE	(4)

/* staves */
#define MINSTAFFS	(1)
#define MAXSTAFFS	(40)
#define DEFSTAFFS	(1)

/* voices */
#define MINVOICES	(1)
#define NORMVOICES	(2)
#define MAXVOICES	(3)

/* number of lines in a tablature staff */
#define	MINTABLINES	(1)
#define	DEFTABLINES	(6)	/* see Guitar[] in globals.c */
#define	MAXTABLINES	(9)

/* number of ticks on a string designation for tablature */
#define	MINTICKS	(0)
#define	MAXTICKS	(MAXTABLINES - 1)

#define TABDEFOCT	(4)

/* tablature fret numbers, including pseudo fret numbers */
#define MINFRET		(0)
#define MAXFRET		(99)
#define DEFFRET		(0)
#define NOFRET		(MAXFRET + 1)
#define IN_UPWARD	(MAXFRET + 2)	/* slur from nowhere, going up */
#define IN_DOWNWARD	(MAXFRET + 3)	/* slur from nowhere, going down */
#define OUT_UPWARD	(MAXFRET + 4)	/* slur to nowhere, going up */
#define OUT_DOWNWARD	(MAXFRET + 5)	/* slur to nowhere, going down */
#define IS_NOWHERE(fret) ((fret) >= IN_UPWARD && (fret) <= OUT_DOWNWARD)

/* range of frets that can be printed by a chord grid */
#define	MINGRIDFRET	(2)
#define	MAXGRIDFRET	(MAXFRET)
#define	DEFGRIDFRET	(4)
#define	NOGRIDFRET	(-1)	/* must be negative so user can't enter it */

/* USE_DFLT_OCTAVE has to be number that is not a valid octave, nor a */
/* valid fret number.  But it has to be positive and small enough to be */
/* able to add MAX_OCTAVE to it */
#define USE_DFLT_OCTAVE	(MAXFRET + 6)

/* min and max numbers for bend integers, numerators, and denominators */
#define BENDINTBITS	(4)
#define MINBENDINT	(0)
#define MAXBENDINT	((1 << BENDINTBITS) - 1)
#define BENDNUMBITS	(6)
#define MINBENDNUM	(0)
#define MAXBENDNUM	((1 << BENDNUMBITS) - 1)
#define BENDDENBITS	(6)
#define MINBENDDEN	(1)
#define MAXBENDDEN	((1 << BENDDENBITS) - 1)

/* time unit */
/* -1 means quadruple whole and 0 means double whole.  Actually, less than -1 */
/* is allowed, and it's minus the number of measures in a multirest */
#define MINBASICTIME	(-1)
#define MAXBASICTIME	(256)

/* the biggest number that is allowed on a multirest */
#define MAXMULTINUM	(1000)

/* limits for scaling the output */
#define MINSCALE	(0.1)
#define MAXSCALE	(10.0)
#define DEFSCALE	(1.0)

/* limits for scaling a staff relative to the output as a whole */
#define MINSTFSCALE	(0.1)
#define MAXSTFSCALE	(10.0)
#define DEFSTFSCALE	(1.0)

#define MINGRIDSCALE	(0.1)
#define MAXGRIDSCALE	(10.0)
#define DEFGRIDSCALE	(1.0)

/* limits on min distance required between a chord and its staff (stepsizes) */
#define	MINCHORDDIST	(0.0)
#define	MAXCHORDDIST	(50.0)
#define	DEFCHORDDIST	(3.0)

/* limits on min dist required between other stuff and its staff (stepsizes) */
#define	MINDIST		(0.0)
#define	MAXDIST		(50.0)
#define	DEFDIST		(2.0)

/* limits on min distance required between a cresc and its staff (stepsizes) */
#define	MINDYNDIST	(0.0)
#define	MAXDYNDIST	(50.0)
#define	DEFDYNDIST	(0.0)

/* limits on min distance required between lyrics and staff (stepsizes) */
#define	MINLYRICSDIST	(0.0)
#define	MAXLYRICSDIST	(50.0)
#define	DEFLYRICSDIST	(2.0)

/* vertical space between staffs and scores, in units of stepsize */
#define	MINMINSTSEP	(6.0)	/* min user-specified min between staffs */
#define	DEFMINSTSEP	(10.0)	/* default min between staffs */
#define	MINMINSCSEP	(6.0)	/* min user-specified min between scores */
#define	DEFMINSCSEP	(12.0)	/* default min between scores */
#define	MINMAXSCSEP	MINMINSCSEP /* min user-specified max between scores */
#define	DEFMAXSCSEP	(20.0)	/* default max between scores */
#define MAXSEPVAL	(PGHEIGHT / STEPSIZE) /* max for either one */
#define	MINSTPAD	(-MAXSEPVAL) /* min user-spec. white between staffs */
#define	MAXSTPAD	(MAXSEPVAL) /* max user white between staffs */
#define	DEFSTPAD	(0.0)	/* default white between staffs */
#define	MINMINSCPAD	(-MAXSEPVAL) /* min user-spec. white between scores */
#define	DEFMINSCPAD	(2.0)	/* default white between scores */
#define	MINMAXSCPAD	(-MAXSEPVAL) /* min user-spec. white between scores */
#define	DEFMAXSCPAD	(2.0)	/* default white between scores */
#define	MAXPADVAL	(MAXSEPVAL) /* max user white between scores */
#define INVSEPVAL	(-100000.0)	/* invalid value for sep */
/* use +1.0 in the following so that roundoff error doesn't cause a problem */
#define SEP_IS_VALID(sep)	((sep) > INVSEPVAL + 1.0)

/* distance from center line a rest should be forced to be */
#define MINRESTDIST	(-50)
#define MAXRESTDIST	(50)
#define	NORESTDIST	(11111)	/* this means it is not set */

/* alignment tag for STUFF */
#define	MINALIGNTAG	(0)
#define	MAXALIGNTAG	(10000)
#define	NOALIGNTAG	(-1)

#define DEFHORZSCALE	(1.0)
/* REMOVE MINHORZSCALE  */
#define MINMINALIGNSCALE	(0.1)
#define MAXMINALIGNSCALE	(1.0)
#define DEFMINALIGNSCALE	(2.0 / 3.0)

/* number of mr needed to combine into a multirest (like -c option) */
#define	MINRESTCOMBINE	(2)
#define	MAXRESTCOMBINE	(1000)
#define	NORESTCOMBINE	(-1)	/* must be negative so user can't enter it */

/* page number to be put on the first page (like -p option) */
#define MINFIRSTPAGE	(1)
#define MAXFIRSTPAGE	(5000)
#define NOFIRSTPAGE	(0)	/* this means it is not set */

/* what fraction of the extra space around a chord should be put on its left
 * side, subject to the limitation of "leftmax" */
#define MINLEFTFACT	(0.0)
#define MAXLEFTFACT	(0.5)
#define DEFLEFTFACT	(0.15)

/* max number of stepsizes of extra room around a chord that are allowed to be
 * put on its left side */
#define MINLEFTMAX	(0.0)
#define MAXLEFTMAX	(100.0)
#define DEFLEFTMAX	(5.0)

/* number to multiply linear regression slope by to get beam's slope */
#define MINBEAMFACT	(0.0)
#define MAXBEAMFACT	(1.0)
#define DEFBEAMFACT	(0.7)

/* maximum beam angle allowed, in degrees */
#define MINBEAMMAX	(0.0)
#define MAXBEAMMAX	(45.0)
#define DEFBEAMMAX	(20.0)

/* angle of beam specified by the "slope" interchord attribute */
#define	MINBEAMANGLE	(-MAXBEAMMAX)
#define	MAXBEAMANGLE	(MAXBEAMMAX)
#define	NOBEAMANGLE	(2 * MAXBEAMANGLE)	/* something out of range */

/* padding to be applied on the left of every group (and the right of the */
/* last group in the measure), in stepsizes */
#define MINPAD		(-5.0)
#define MAXPAD		(50.0)
#define DEFPAD		(0.0)

/* for setting and testing unknown stem length */
#define	STEMLEN_UNKNOWN		(-1.0)
#define	IS_STEMLEN_KNOWN(x)	((x) >= 0.0)	/* avoid using "==" on floats */
#define	IS_STEMLEN_UNKNOWN(x)	((x) < 0.0)	/* avoid using "==" on floats */

/* other stem length definitions */
#define	MINSTEMLEN	(0.0)
#define	DEFSTEMLEN	(7.0)
#define	MAXSTEMLEN	(100.0)
#define	DEFSTEMLEN_LONG	(6.0)		/* for long (quad and oct) groups */
#define	SM_STEMFACTOR	(5.0 / 7.0)	/* grace/cue factor */
#define	TINY_STEMFACTOR	(4.0 / 7.0)	/* grace-cue versus normal factor */

/* number of stepsizes by which a beamed stem can be shortened */
#define	MINBEAMSHORT	(0.0)
#define	MAXBEAMSHORT	(2.0)
#define	DEFBEAMSHORT	(1.0)

/* max stepsizes to shorten a stem due to note protuding far from the staff */
#define	MINMAXPROSHORT	(0.0)
#define	MAXMAXPROSHORT	(7.0)
#define	DEFMAXPROSHORT	(2.0)

/* steps of a note towards stem side of staff where shortening has begun */
#define	MINBEGPROSHORT	(-4)
#define	MAXBEGPROSHORT	(50)
#define	DEFBEGPROSHORT	(1)

/* steps of a note towards stem side of staff where shortening reaches limit */
#define	MINENDPROSHORT	(-4)
#define	MAXENDPROSHORT	(50)
#define	DEFENDPROSHORT	(6)

/* min stem length as a fraction of the default */
#define MINSTEMLENFRAC	(0.10)

/* what fraction of each lyrics syllable goes left of the center of chord */
#define MINLYRICSALIGN	(0.0)
#define MAXLYRICSALIGN	(1.0)
#define DEFLYRICSALIGN	(0.25)

/* size of the paper */
#define MINPAGEHEIGHT	(2.0)
#define MAXPAGEHEIGHT	(24.0)
#define DEFPAGEHEIGHT	(11.0)
#define MINPAGEWIDTH	(2.0)
#define MAXPAGEWIDTH	(24.0)
#define DEFPAGEWIDTH	(8.5)

/* top and bottom margins */
#define MINVMARGIN	(0.0)
#define MAXVMARGIN	(3.0)
#define DEFVMARGIN	(0.5)

/* left and right margins */
#define MINHMARGIN	(0.0)
#define MAXHMARGIN	(3.0)
#define DEFHMARGIN	(0.5)

/* how close a subbar must be to a chord, to be printed (in counts) */
#define	SUBBARFUDGE	(0.001)

/* special values to be stored in margin fields in FEED */
#define MG_DEFAULT	(-1.0)	/* use Score.*margin parameter */
#define MG_AUTO		(-2.0)	/* placement should calculate (right) margin */

/* margin override type */
#define MOT_UNUSED	(0)	/* no margin override */
#define MOT_ABSOLUTE	(1)	/* replace default value */
#define MOT_RELATIVE	(2)	/* add to default value */
#define MOT_AUTO	(3)	/* placement should calculate (right) margin */

/* page side */
#define PGSIDE_NOT_SET	(0)
#define PGSIDE_LEFT	(1)
#define PGSIDE_RIGHT	(2)

/* panels per page */
#define	MINPANELSPERPAGE	(1)
#define	MAXPANELSPERPAGE	(2)
#define	DEFPANELSPERPAGE	(1)

/* max number of scores allowed on a panel */
#define	MINMAXSCORES	(1)
#define	MAXMAXSCORES	(1000)
#define	DEFMAXSCORES	(1000)

/* max number of measures allowed on a score */
#define	MINMAXMEASURES	(1)
#define	MAXMAXMEASURES	(1000)
#define	DEFMAXMEASURES	(1000)

/* how tightly to pack the stuff horizontally (default value) */
#define MINPACKFACT	(0.0)
#define DFLTPACKFACT	(1.0)
#define MAXPACKFACT	(10.0)

/* how much to expand long notes relative to short ones */
#define MINPACKEXP	(0.0)
#define DFLTPACKEXP	(0.8)
#define MAXPACKEXP	(1.0)

/* time signature */
#define MINNUMERATOR	(1)
#define MAXNUMERATOR	(99)

#define MINDENOMINATOR	(1)
#define MAXDENOMINATOR	(64)

#define MINDIVISION	(1)
#define DEFDIVISION	(192)
#define MAXDIVISION	(3 * 512)

/* define values for the "release" parameter, in milliseconds */
#define MINRELEASE	(0)
#define DEFRELEASE	(20)
#define MAXRELEASE	(500)

/* define values for the "sylposition" parameter, in points */
#define	MINSYLPOSITION	(-100)
#define	MAXSYLPOSITION	(100)
#define	DEFSYLPOSITION	(-5)
#define	NOSYLPOSITION	(11111)	/* this means it is not set */

/* define values for the "a4freq" parameter */
#define	MINA4FREQ	(100.0)
#define	MAXA4FREQ	(1000.0)
#define	DEFA4FREQ	(440.0)

/* define the min and max frequencies allowed by MIDI */
#define	MINFREQ		(8.1758)
#define	MAXFREQ		(13289.7300)

#define MAXMIDINOTES	(128)	/* number of notes MIDI allows */
#define MAXMIDIMAPS	(128)	/* number of map MIDI allows */



/*
 * Define sets of symbols which probably should have been done as enums,
 * but in ANSI C enums aren't much good anyhow.  =============================
 * Also define some macros for testing these enums.
 */

/*
 * To limit the number of terminal symbols so that yacc won't blow up,
 * several tokens map to the same terminal symbol, and we set a variable
 * (yylval) to say which particular one was actually seen.
 */
#define F_STAFFS	(1)
#define F_OCTAVE	(2)
#define F_LYRSIZE	(3)
#define F_VERTSPACE	(4)
#define F_SIZE		(5)

#define F_TOPMARGIN	(6)
#define F_BOTMARGIN	(7)
#define F_LEFTMARGIN	(8)
#define F_RIGHTMARGIN	(9)

#define F_TIMEUNIT	(10)
#define F_VSCHEME	(11)

#define F_BRACKET	(12)
#define F_BRACE		(13)
#define F_BARSTYLE	(14)

#define F_LABEL		(15)
#define F_LABEL2	(16)

#define F_FONT		(17)
#define F_LYRFONT	(18)

#define F_VISIBLE	(19)
#define F_VERSES	(20)

/*
 * Define all the types of context in the input.  Make each one a bit, so that
 * we can also define a couple groupings of them.
 */
#define	C_HEADER	(0x1)	/* first page header */
#define	C_FOOTER	(0x2)	/* first page footer */
#define	C_HEAD2		(0x4)	/* later page header */
#define	C_FOOT2		(0x8)	/* later page footer */
#define	C_TOP		(0x10)	/* initial block at top */
#define	C_BOT		(0x20)	/* initial block at bottom */
#define	C_TOP2		(0x40)	/* later block at top */
#define	C_BOT2		(0x80)	/* later block at bottom */
#define	C_BLOCK		(0x100)	/* other block */
#define	C_SCORE		(0x200)	/* the whole score */
#define	C_STAFF		(0x400)	/* a staff */
#define	C_VOICE		(0x800)	/* a voice */
#define	C_MUSIC		(0x1000) /* notes, etc. */
#define	C_GRIDS		(0x2000) /* chord grids */
#define	C_HEADSHAPES	(0x4000) /* head shapes for notehead characters */
#define	C_SYMBOL	(0x8000) /* user-defined symbol */
#define	C_KEYMAP	(0x10000) /* key mapping */
#define	C_ACCIDENTALS	(0x20000) /* pitch offsets of accidentals */
#define	C_CONTROL	(0x40000) /* control commands */

/* context classes--combining things that mostly have the same rules */
#define	C_BLOCKHEAD	(C_HEADER | C_FOOTER | C_HEAD2 | C_FOOT2 | \
			C_TOP | C_BOT | C_TOP2 | C_BOT2 | C_BLOCK)
#define	C_SSV		(C_SCORE | C_STAFF | C_VOICE)

/* define whether things are measured in inches or centimeters */
#define INCHES		(0)
#define CM		(1)

/*
 * Define the voice schemes.  V_1 means just one voice is on the staff;
 * the program will decide the stem direction of each group.  V_2OPSTEM
 * means there are two voices with opposing stems:  the first voice always
 * points up, and the second one always points down.  V_2FREESTEM means there
 * are two voices.  If, at any time, one has a space, the other one's stems
 * can point either way (as with one voice).  Otherwise, the first voice
 * points up and the second one down.
 * V_3OPSTEM and V_3FREESTEM are just like the "2" ones, except that a third
 * voice is allowed to exist.  Its stem direction is determined elsewhere.
 */
#define	V_1		(0)
#define	V_2OPSTEM	(1)
#define	V_2FREESTEM	(2)
#define	V_3OPSTEM	(3)
#define	V_3FREESTEM	(4)

/*
 * Define the modes of the voicecombine parameter.
 */
/* bottom note of high voice must be higher than top note of low voice */
#define VC_NOOVERLAP	(0)
/* bottom note of high voice must be no lower than top note of low voice */
#define VC_SHAREONE	(1)
/* no restrictions */
#define VC_OVERLAP	(2)
/* combine only rests */
#define VC_RESTSONLY	(3)
/* bot note of high voice at least 2 steps higher than top note of low voice */
#define VC_STEPSAPART	(4)

/*
 * When tied_to_voice or slurred_to_voice is not used, set it to this.
*/
#define	NO_TO_VOICE	(0)

/*
 * Define whether a tranposition should apply to only the notes, or only the
 * chords, or both.
 */
#define	TR_NOTES	(1 << 0)
#define	TR_CHORDS	(1 << 1)
#define	TR_BOTH		(TR_NOTES | TR_CHORDS)

/*
 * Define the staff style vis-a-vis printing of clefs, accidentals (in key
 * signatures and otherwise), and tranposing.  WARNING:  SS_NOTHING must be
 * defined to be 0 so that calloc inits the field to this.
 */
#define	SS_NOTHING	(0)	/* no transpose; print no clef, no accidentals*/
#define	SS_NORMAL	(1)	/* normal printing */
#define	SS_DRUM		(2)	/* like SS_NOTHING, but print a "drum clef" */

/*
 * Define values for printtabclef.  Ideally, we would have combined these in
 * some logical way with the SS_* symbols above (printclef), or organized the
 * information in some other way.  But this was introduced later, and it's not
 * worth the effort and risk of changing printclef and the SS_* symbols.
 */
#define	PTC_NEVER	(0)	/* never print "TAB" at the start of a staff */
#define	PTC_FIRST	(1)	/* print "TAB" only on the first score */
#define	PTC_ALWAYS	(2)	/* never print "TAB" */

/*
 * Define the time signature types.  NUMERIC is the usual two-number
 * signature.  INVISNUM is the same, except that it will not be printed.
 * Common and cut time are noted here separately.  But for all other
 * purposes they are treated as 4/4 and 2/2 respectively.
 */
#define	TS_NUMERIC	(0)
#define	TS_INVISNUM	(1)
#define	TS_COMMON	(2)
#define	TS_CUT		(3)

/*
 * Special bytes used inside time signature representation.
 * These values need to be outside the range of time signature numerators
 * and denominators, but small enough to fit in a byte.
 * Probably safer to keep them under 128 as well, to avoid any possible
 * issues with negative chars.
 *
 * Here's how the encoding works:
 * cut and common get stored as TSR_CUT and TSR_COMMON respectively.
 * If a + is used between a denominator of one fraction
 * and numerator of another, it is encoded as TSR_ADD.
 * When a + is used inside a numerator, nothing is stored at all,
 * since it is clear all numbers up to the slash
 * must be components of the numerator.
 * White space rather than + after a denominator
 * means alternating time signatures, marked internally by TSR_ALTERNATING.
 * Numbers are stored as their binary values.
 * A TSR_END is placed at the end of everything to mark the end.
 * A null is used for that, so one could use things like strlen or strcpy
 * on a representation.
 */
#define TSR_CUT		(MAXNUMERATOR + 1)
#define TSR_COMMON	(MAXNUMERATOR + 2)
#define TSR_SLASH	(MAXNUMERATOR + 3)
#define TSR_ADD		(MAXNUMERATOR + 4)
#define TSR_ALTERNATING	(MAXNUMERATOR + 5)
#define TSR_END		(0)

/*
 * This is the maximum length of an internal time signature representation.
 * Note that the internal form is more compact than the input form,
 * so even something so ridiculously complicated that it's doubtful any
 * human could keep it straight, like
 *   3+1+4/16 + cut + 7+11+41/2  9+2/64 + 5+7+31+29/1 + com
 * would only take 28 bytes, so 40 should be plenty!
 * The maximum number of numerator components 40 bytes could accomodate
 * would be 37, so maximum theoretical effective time signature
 * would be thirty-seven 99's or 3663 counts.
 */
#define MAXTSLEN	40

/*
 * Define when time signatures should be printed.  "Once" means when it
 * changes.  "Always" makes sense only when there is an alternating time sig,
 * and it means print it at each measure.
 */
#define PTS_NEVER	(0)	/* user input "n" */
#define PTS_ONCE	(1)	/* user input nothing */
#define PTS_ALWAYS	(2)	/* user input "y" */

/*
 * Define the clefs that are allowed.  The values are important, so that
 * the program can conveniently figure note positions without having a
 * separate case for each value.
 */
#define	TREBLE_8A	(-7)
#define	FRENCHVIOLIN	(-2)
#define	TREBLE		(0)
#define	SOPRANO		(2)
#define	MEZZOSOPRANO	(4)
#define	BASS_8A		(5)
#define	ALTO		(6)
#define	TREBLE_8	(7)
#define	TENOR		(8)
#define	BARITONE	(10)
#define	BASS		(12)
#define	SUBBASS		(14)
#define	BASS_8		(19)

#define	TABCLEF		(-1)	/* tablature "clef" */
#define NOCLEF		(-99)	/* no clef is present */

/*
 * Define what a GRPSYL structure can represent.
 */
#define	GS_GROUP	(0)
#define	GS_SYLLABLE	(1)

/*
 * Define the different contents a group can have.
 */
#define	GC_NOTES	(0)
#define	GC_REST		(1)
#define	GC_SPACE	(2)

/*
 * Define the time values a group can have.  Only note groups can be other than
 * normal (grace notes).  There are no grace rests or spaces.
 */
#define	GV_NORMAL	(0)
#define	GV_ZERO		(1)

/*
 * Define the unknown value for headshapes.  The other values aren't defined
 * here because they are established as headshapes are loaded at run time.
 * There is a built-in set of these that are loaded, and then the user can use
 * the "headshapes" context to load more.  Each headshape number corresponds to
 * a headshape name, such as "norm", "xnote", "rect", etc., and stands for the
 * set of music characters used to print note heads of the various basictimes.
 */
#define	HS_UNKNOWN	(0)

/*
 * Define basictime values for groups that are longer than a whole note.
 * Don't bother with shorter values, since they are obvious, 1 over the time.
 */
#define	BT_DBL		(0)	/* double whole */
#define	BT_QUAD		(-1)	/* quadruple whole, a.k.a. "longa" */
#define	BT_OCT		(-2)	/* octuple whole, a.k.a. "maxima" */

/*
 * Define the size of a group.  Spaces are always normal, but notes and rests
 * can have different sizes.  For note groups, this can vary on a per-note
 * basis.
 */
#define	GS_NORMAL	(0)
#define	GS_SMALL	(1)
#define	GS_TINY		(2)

/*
 * Define position that you can be in relative to a list of objects.
 */
#define	NOITEM		(0)	/* not within a list */
#define	STARTITEM	(1)	/* first item in a list */
#define	INITEM		(2)	/* interior item in a list */
#define	ENDITEM		(3)	/* last item in a list */
#define	LONEITEM	(4)	/* only item in a list */


/*
 * Define the directions for various items.
 */
#define UNKNOWN	(0)
#define	UP	(1)
#define	DOWN	(2)


/*
 * Define values for controlling printing of tuplet numbers and brackets.
 * For PT_DEFAULT, the bracket will be printed unless all the notes of the
 * tuplet (and no other notes) form a beamed set.
 */
#define	PT_NEITHER	(0)	/* never print number or bracket */
#define	PT_DEFAULT	(1)	/* always print number; maybe print bracket */
#define	PT_BOTH		(2)	/* always print number and bracket */
#define	PT_NUMBER	(3)	/* always print just the number */


/*
 * Coordinate types that can have location variables associated with them.
 */
#define CT_BUILTIN	(1)
#define CT_GRPSYL	(2)
#define CT_BAR		(4)
#define CT_NOTE		(8)
#define	CT_SCORE	(16)
#define CT_INVISIBLE	(128)


/*
 * Define the types of STUFF structure.  "Stuff" is things that are to be
 * printed other than the actual music and lyrics.
 */
#define	ST_ROM		(0)
#define	ST_BOLD		(1)
#define	ST_ITAL		(2)
#define	ST_BOLDITAL	(3)
#define	ST_CRESC	(4)
#define	ST_DECRESC	(5)
#define	ST_MUSSYM	(6)
#define	ST_PEDAL	(7)
#define	ST_TIESLUR	(8)
#define	ST_TABSLUR	(9)
#define	ST_BEND		(10)
#define	ST_PHRASE	(11)
#define	ST_OCTAVE	(12)
#define	ST_MIDI		(13)

#define IS_TEXT(stype)	((stype) == ST_ROM || (stype) == ST_ITAL ||	\
			(stype) == ST_BOLD || (stype) == ST_BOLDITAL)

/* define a pseudo-stufftype, used for lyrics in a couple places */
#define ST_LYRICS	(-1)

/*
 * Define the places where the stuff can be printed, relative to staff(s).
 */
#define	PL_ABOVE	(0)
#define	PL_BELOW	(1)
#define	PL_BETWEEN	(2)
#define	PL_UNKNOWN	(3)
#define	NUM_PLACE	(3)	/* number of "good" places (exclude unknown) */

/*
 * Define the ways a user can specify the horizonal offset of a group from the
 * chord's X coordinate.
 */
#define	HO_NONE		(0)	/* not specified */
#define	HO_LEFT		(1)	/* "-" (next to other group(s), on the left) */
#define	HO_RIGHT	(2)	/* "+" (next to other group(s), on the right) */
#define	HO_VALUE	(3)	/* "+/-N" (offset given by GRPSYL.ho_value) */

/*
 * Define text modifiers:  different flavors of rom, bold, ital, and
 * boldital.  TM_DYN also applies automatically to the hairpin stuffs, < and >.
 */
#define	TM_NONE		(0)
#define	TM_CHORD	(1)
#define	TM_ANALYSIS	(2)
#define	TM_FIGBASS	(3)
#define	TM_DYN		(4)

#define IS_CHORDLIKE(x)	((x) == TM_CHORD || (x) == TM_ANALYSIS ||	\
			(x) == TM_FIGBASS)

/*
 * Define values for dist_usage in STUFF.
 */
#define	SD_NONE		(0)	/* user did not specify */
#define	SD_MIN		(1)	/* user specified minimum distance */
#define	SD_FORCE	(2)	/* user is forcing this distance */

/*
 * Define all the types of marks that the user can request to be stacked in
 * different orders.  This list is in increasing order of the default priority
 * of stacking, except that by default MK_DYN, MK_OTHERTEXT, and MK_CHORD are
 * of equal priority.  If this list changes, make sure that Defmarkorder[]
 * is updated too.  For each place (above, below, between), the user can
 * specify a different ordering of priority to be used.  There is a restriction
 * that the last ones (MK_LYRICS, MK_ENDING, MK_REHEARSAL, MK_PEDAL) must
 * have a different priority from each other and any of the other ones.  Also,
 * MK_PEDAL is allowed only below; MK_ENDING and MK_REHEARSAL are allowed only
 * above; and those three and MK_OCTAVE are not allowed between.
 */
#define	MK_MUSSYM	(0)	/* ST_MUSSYM */
#define	MK_OCTAVE	(1)	/* ST_OCTAVE */
#define	MK_DYN		(2)	/* ST_CRESC or ST_DECRESC; or ST_ROM, */
				/* ST_ITAL, ST_BOLD, ST_BOLDITAL with "dyn" */
				/* (TM_DYN is set for these and only these) */
#define	MK_OTHERTEXT	(3)	/* ST_ROM, ST_ITAL, ST_BOLD, ST_BOLDITAL */
				/* without chord/analysis/figbass or dyn */
#define	MK_CHORD	(4)	/* ST_ROM, ST_ITAL, ST_BOLD, ST_BOLDITAL */
				/* with chord, analysis, or figbass */
				/* (TM_CHORD is set for these and only these) */
#define	MK_LYRICS	(5)	/* lyrics */
#define	MK_ENDING	(6)	/* ending mark */
#define	MK_REHEARSAL	(7)	/* rehearsal mark */
#define	MK_PEDAL	(8)	/* ST_PEDAL */
#define NUM_MARK	(9)	/* this must follow the last MK */

/*
 * While parsing, we need to temporarily store some things as pseudo-pitches.
 * When the user does not specify any pitch for the first group of a measure,
 * we temporarily use PP_NO_PITCH, since not specifying a pitch is legal iff it
 * is for one or more 1-line staffs, but it's not easy to check on that until
 * later.  We also need to save rest, space, and rpt as pseudo notes until we
 * later map them to groups.
 */
#define PP_NO_PITCH	'h'
#define PP_REST		'r'
#define PP_SPACE	's'
#define PP_RPT		'p'

/*
 * Define the other staff that a given staff's groups are beamed or stemmed to.
 */
#define CS_SAME		(0)
#define CS_ABOVE	(1)
#define CS_BELOW	(2)

/*
 * Define symbols for indexing coordinate arrays.  The first 12 are X, Y,
 * north, south, east, and west, both relative and absolute.  Not all things
 * that have coordinates bother to set all 12 of these.  There should be
 * comments in structs.h saying which things get set for what.
 *
 * In GRPSYL, during the time that positions of phrase marks are being
 * figured out, AN and AS are used in a strange way.  But later, they get
 * set to their intended values.
 *
 * The 13th is a special number used by nongrace notes and GRPSYLs, and BARs
 * only.  For notes and GRPSYLs, it has the following meaning.  It indicates
 * how many inches of horizontal space would be allocated to this object
 * if it were a whole note instead of whatever it actually is, but were
 * allocated space proportionally.  That is, fulltime times this number
 * is the amount of space between the X coordinates of this GRPSYL and
 * the next one in the measure (or the bar line if this is the last GRPSYL
 * in the measure).  Notice that even for notes, it's the X coordinate of
 * the note's GRPSYL that count, even if the note head is on the "wrong"
 * side of the stem.  For BARs, this 13th number has a similiar use.  If
 * you pretend that a bar line is count 0 of the following measure, and
 * the first GRPSYL of the measure is at count 1 (whichever type of note
 * constitutes a "count"), this number is how many inches would be allocated
 * to a whole note instead of the one count of space that is there.
 */
#define	RX	(0)
#define	RY	(1)
#define	RN	(2)
#define	RS	(3)
#define	RE	(4)
#define	RW	(5)
#define	AX	(6)
#define	AY	(7)
#define	AN	(8)
#define	AS	(9)
#define	AE	(10)
#define	AW	(11)
#define	INCHPERWHOLE	(12)
#define NUMCTYPE	(13)

/* Must be something that is not a valid index of a c[] */
#define EXPORT_ALL_STAFFS	(NUMCTYPE+1)


/*
 * AN and AS of group coordinates are used temporarily to store the distance
 * from the group to any phrase marks, to be used to make sure nesting phrases
 * don't impinge on that space. But if a group has a phrase ending on it and
 * another beginning on it, those won't collide.  So use bitmap to keep track
 * of whether the AN and AS values apply to east side, west side, or both.
 * (phraseside in GRPSYL is a bit map.)
 */
#define EAST_SIDE	(1 << 0)
#define WEST_SIDE	(1 << 1)

/*
 * The following section lists the operators/operand types for expressions
 * used for location tag references in statements like print, line, curve, etc.
 * The values for the operators are arbitrary; they just need to be unique,
 * They are put into groups based on similarities, in case it can make coding
 * easier by treating similar operators with common code.
 */
/* Binary operators. The  lchild_p and rchild_p fields will be used. */
#define OP_BINARY	(0x10)
#define OP_ADD 		(OP_BINARY + 0)
#define OP_SUB		(OP_BINARY + 1)
#define OP_MUL		(OP_BINARY + 2)
#define OP_DIV		(OP_BINARY + 3)
#define OP_MOD		(OP_BINARY + 4)
#define OP_ATAN2	(OP_BINARY + 5)
#define OP_HYPOT	(OP_BINARY + 6)

/*
 * Unary operators.  These use only the lchild_p field.
 */
#define OP_UNARY	(0x20)
#define OP_SQRT		(OP_UNARY + 0)
#define OP_SIN		(OP_UNARY + 1)
#define OP_COS		(OP_UNARY + 2)
#define OP_TAN		(OP_UNARY + 3)
#define OP_ASIN		(OP_UNARY + 4)
#define OP_ACOS		(OP_UNARY + 5)
#define OP_ATAN		(OP_UNARY + 6)

/* Operands */
#define OP_OPERAND	(0x40)
/* A literal number (int or float) would be converted to double
 * and stored in left.value */
#define OP_FLOAT_LITERAL	(OP_OPERAND + 0)
/* A reference to a tag c[] thing would have the address
 * of the appropriate TAG_REF in its left.ltag_p */
#define OP_TAG_REF	(OP_OPERAND + 1)
/* A time offset. The offset in counts is stored in left.value
 * and the c[] to use for INCHPERWHOLE is pointed to by right.rtag_p.
 * The right.rtag_p->c_index is ignored. */
#define OP_TIME_OFFSET	(OP_OPERAND + 2)


/*
 * Define the types of bar line.
 */
#define	INVISBAR	(0)
#define	SINGLEBAR	(1)
#define	DOUBLEBAR	(2)
#define	REPEATSTART	(3)
#define	REPEATEND	(4)
#define	REPEATBOTH	(5)
#define	ENDBAR		(6)
#define	RESTART		(7)

/*
 * When specifying vertical endpoints of subbars, these say whether the
 * reference point is the top, middle, or bottom line of the staff.
 */
#define LR_TOP		(0)
#define LR_MIDDLE	(1)
#define LR_BOTTOM	(2)

/*
 * Define the types of font, leaving 0 unused to avoid ending up having
 * null characters appear in strings where this number is stored,
 * and to have something convenient to use to flag an unknown font.
 * WARNING:  the getfontinfo.c program depends on all the font names
 * beginning with "FONT_", and on MAXFONTS being at the end of the list.
 */
#define FONT_UNKNOWN	(0)
#define FAMILY_DFLT	(-1)

#define BASE_TIMES	(0)
#define	FONT_TR		(1)
#define	FONT_TI		(2)
#define	FONT_TB		(3)
#define	FONT_TX		(4)

#define BASE_AVANTGARDE	(FONT_TX)
#define FONT_AR		(BASE_AVANTGARDE + FONT_TR)
#define FONT_AI		(BASE_AVANTGARDE + FONT_TI)
#define FONT_AB		(BASE_AVANTGARDE + FONT_TB)
#define FONT_AX		(BASE_AVANTGARDE + FONT_TX)

#define BASE_COURIER	(FONT_AX)
#define FONT_CR		(BASE_COURIER + FONT_TR)
#define FONT_CI		(BASE_COURIER + FONT_TI)
#define FONT_CB		(BASE_COURIER + FONT_TB)
#define FONT_CX		(BASE_COURIER + FONT_TX)

#define BASE_HELVETICA	(FONT_CX)
#define FONT_HR		(BASE_HELVETICA + FONT_TR)
#define FONT_HI		(BASE_HELVETICA + FONT_TI)
#define FONT_HB		(BASE_HELVETICA + FONT_TB)
#define FONT_HX		(BASE_HELVETICA + FONT_TX)

#define BASE_BOOKMAN	(FONT_HX)
#define FONT_BR		(BASE_BOOKMAN + FONT_TR)
#define FONT_BI		(BASE_BOOKMAN + FONT_TI)
#define FONT_BB		(BASE_BOOKMAN + FONT_TB)
#define FONT_BX		(BASE_BOOKMAN + FONT_TX)

#define BASE_NEWCENTURY	(FONT_BX)
#define FONT_NR		(BASE_NEWCENTURY + FONT_TR)
#define FONT_NI		(BASE_NEWCENTURY + FONT_TI)
#define FONT_NB		(BASE_NEWCENTURY + FONT_TB)
#define FONT_NX		(BASE_NEWCENTURY + FONT_TX)

#define BASE_PALATINO	(FONT_NX)
#define FONT_PR		(BASE_PALATINO + FONT_TR)
#define FONT_PI		(BASE_PALATINO + FONT_TI)
#define FONT_PB		(BASE_PALATINO + FONT_TB)
#define FONT_PX		(BASE_PALATINO + FONT_TX)

#define IS_STD_FONT(f)	(f >= FONT_TR && f <= FONT_PX)
#define NUM_STD_FONTS	(FONT_PX - BASE_TIMES)
#define NUM_EXT_FONT_SETS  (3)
#define BASE_MISC	((1 + NUM_EXT_FONT_SETS) * NUM_STD_FONTS) + BASE_TIMES
#define FONT_ZI		(BASE_MISC + 1)
#define FONT_ZD1	(BASE_MISC + 2)
#define FONT_ZD2	(BASE_MISC + 3)
#define	FONT_SYM	(BASE_MISC + 4)
/* number of music fonts */
#define NUM_MFONTS      (2)
#define	FONT_MUSIC	(FONT_SYM + 1)
#define	FONT_MUSIC2	(FONT_MUSIC + 1)
/*
 * How many fonts worth of user-defined symbols we support.  At the moment,
 * we only support 1, and it would take more work than just changing this
 * to support more, but having this helps make some code support multiple.
 */
#define NUM_UFONTS      (1)
#define FONT_USERDEF1	(FONT_MUSIC + NUM_MFONTS)
/* The builtin music fonts and fonts of user defined symbols are sometimes
 * handled the same, so it is convenient to know how many there are. */
#define NUM_SYMFONTS	(NUM_MFONTS + NUM_UFONTS)
/* For arrays that are for both music symbols and user-defined symbols,
 * this gives the index into the array. */
#define SYMFONT_INDEX(f)	((f) - FONT_MUSIC)
#define MAXFONTS	(FONT_MUSIC + NUM_SYMFONTS)

/* first printable character in a font */
#define	FIRST_CHAR	(32)

/*
 * We use character code from 32-191. 0-31 are reserved for control characters,
 * and we reserve 192-255 for STR_* commands.
 */
#define MAX_CHARS_IN_FONT	(192-FIRST_CHAR)

/*
 * Find array offset into height, width, and ascent tables.  This is character
 * code -FIRST_CHAR, to skip ASCII control characters.
 */
#define CHAR_INDEX(c)	((c) - FIRST_CHAR)

/*
 * Test if a font is a "music" font.
 */
#define IS_MUSIC_FONT(f) ((f) >= FONT_MUSIC && (f) < FONT_MUSIC + NUM_SYMFONTS)

/*
 * For finding font-wide bounding box of user-defined symbols, we assume way
 * off in the wrong direction and then find the real values.  We also want to
 * make sure any sizes will fit even in a 16-bit int, just in case anyone still
 * has that.  So we use 30000. This allows for 100 stepsizes in each direction,
 * or 200 stepsizes square, which should be way bigger than any sane single
 * symbol.
 */
#define MAX_USYM_UNITS	(30000)

/*
 * These are used in a bitmap to record what attributes the user has supplied
 * for a user-defined symbol.  They tell us if any required attributes are
 * missing, and which optional ones are present.
 */
#define US_POSTSCRIPT	(0x1)
#define US_BBOX		(0x2)
#define US_STEMOFFSET	(0x4)


/*
 * Special commands to use inside internal format of strings.
 * All use high-bit==1 to indicate a command.
 * Note that the low order bits of STR_MUS_CHAR* specify which music font it is.
 */
#define STR_MUS_CHAR		0xd0	/* followed by 1-byte size and 1-byte \
					 * music character code */
#define STR_MUS_CHAR2		0xd1	/* same rules as above */
#define STR_USERDEF1		0xd2
#define STR_FONT		0xe0	/* followed by 1-byte font number */
#define STR_SIZE		0xe1	/* followed by 1-byte size */
#define STR_PAGENUM		0xe2	/* followed by '%' */
#define STR_NUMPAGES		0xe3	/* followed by '#' */
#define STR_PRE			0xe4	/* pre-syl <--->, ignored in spacing */
#define STR_U_PRE		0xe5	/* pre-syl <...>, used in spacing */
#define STR_PRE_END		0xe6	/* end of pre-syllable <...> */
#define STR_PST			0xe7	/* post-syl <...>, ignored in spacing */
#define STR_U_PST		0xe8	/* post-syl <...>, used in spacing */
#define STR_PST_END		0xe9	/* end of post-syllable <...> */
/*
 * Backspace is a bit strange.  The amount to back up varies because of
 * proportionally spaced fonts.  So we save the distance in terms of the width
 * in 250ths of an inch in the default size.  The actual distance to back up is
 * then that number times the ratio of the current actual size to the default
 * size divided by 250.  250 was chosen because that means a range of 1-127
 * (to make sure it is non-zero and with high-bit of zero)
 * will give a range of .004 to 0.508 inches, which should cover all
 * characters we have today (the widest of which is currently just over 1/4").
 * This is done in floating point, so no reason to use a power of 2 like 256.
 */
#define STR_BACKSPACE		0xea	/* followed by how much to back up */

#define BACKSP_FACTOR		(250.0)

/* string commands for boxed text */
#define STR_BOX			0xeb
#define STR_BOX_END		0xec
#define IS_BOXED(string)   (((string[2] & 0xff) == (STR_BOX & 0xff)) ? YES : NO)

#define	STR_VERTICAL		0xed	/* vert move followed by 1-byte dist */
#define	STR_L_ALIGN		0xee	/* align at left edge of next char */
#define	STR_C_ALIGN		0xef	/* align at center of next char */
#define STR_PILE		0xf0	/* toggle for piling, \: */
#define STR_SLASH		0xf1	/* slash used by figbass, \/ */

/* string commands for circled text */
#define STR_CIR			0xf2
#define STR_CIR_END		0xf3
#define IS_CIRCLED(string) (((string[2] & 0xff) == (STR_CIR & 0xff)) ? YES : NO)

#define STR_TAG			0xf4
#define STR_END_TAG		0xf5

#define STR_KEYMAP		0xf6

/* chord translation */
#define	CT_NONE			(0)	/* no translation */
#define	CT_DOREMI		(1)	/* translate letters to syllables */
#define	CT_GERMAN		(2)	/* translate B -> H, B& -> B */

#ifndef DOREMI /* the old way */
/* chord translation modifier, used when chord translation is CT_DOREMI */
#define	CTM_NOCAPS		(0)	/* example:  C -> do */
#define	CTM_INITIALCAP		(1)	/* example:  C -> Do */
#define	CTM_ALLCAPS		(2)	/* example:  C -> DO */
#endif

/*
 * Define macros relating to vertical movement within a string.  It is stored
 * in points, offset by a bias so that the number will always be positive when
 * stored in the byte after the STR_VERTICAL in a string.
 */
#define	MINVERTICAL	(-50)
#define	MAXVERTICAL	(50)
#define	VERT_BIAS	(1 - MINVERTICAL)
#define	ENCODE_VERT(x)	(x + VERT_BIAS)
#define	DECODE_VERT(x)	(x - VERT_BIAS)

/*
 * Macro to determine if a character in an internal-format string is a
 * command character.
 */
#define IS_STR_COMMAND(c)	(((c) & 0xc0) == 0xc0)


#define DFLT_SIZE	(12)
#define SMALLSIZE	((int)(DFLT_SIZE * SM_FACTOR))
#define TINYSIZE	((int)(DFLT_SIZE * TINY_FACTOR))

/*
 * Define the default font size of a measure number.
 */
#define MNUM_SIZE	(11)

/*
 * Define types of lines and curves.
 */
#define	L_NORMAL	(0)
#define	L_MEDIUM	(1)
#define	L_WIDE 		(2)
#define	L_WAVY		(3)
#define	L_DOTTED	(4)
#define	L_DASHED	(5)

/*
 * Define the kinds of justification.
 */
#define	J_LEFT		(1)
#define	J_RIGHT		(2)
#define	J_CENTER	(3)
#define	J_NONE		(4)
#define	J_RAGPARA	(5)	/* ragged-right paragraph */
#define	J_JUSTPARA	(6)	/* justified paragraph */

/*
 * Define the ending styles (which staffs 1st, 2nd, etc. endings are to
 * be drawn on).
 */
#define ENDING_TOP	(0)	/* only above top visible staff */
#define ENDING_BARRED	(1)	/* above each group of staffs barred together*/
#define ENDING_GROUPED	(2)	/* above each group of staffs grouped */
				/*  together by braces or brackets */

/*
 * Define values for endending_type.
 */
#define	EE_DEFAULT	(0)	/* open/closed based on Mup's algorithm */
#define	EE_OPEN		(1)	/* always open */
#define	EE_CLOSED	(2)	/* always closed */

/*
 * Define where to print measure numbers.
 */
#define	MN_NONE		(0)	/* no measure numbers ("n")*/
#define	MINEVERYMEASNUM	(1)	/* every measure (the minimum) */
#define	MAXEVERYMEASNUM	(10000)	/* every 10000th measure (the maximum) */
#define	MN_SCORE	(MAXEVERYMEASNUM + 1) /* at start of each score ("y") */
/* MN_NONE and MN_SCORE have to have values as defined above for the
 * following range values to work properly. */
#define MINMEASNUM	MN_NONE
#define MAXMEASNUM	MN_SCORE
/* numbers allowed for "m" in "measnum = every m" */
#define IS_EVERY(m)     ((m) >= MINEVERYMEASNUM && (m) <= MAXEVERYMEASNUM)

/*
 * Define types of rehearsal letters/numbers.
 */
#define REH_NONE	(0)	/* none at all */
#define REH_STRING	(1)	/* user-supplied string */
#define REH_NUM		(2)	/* consecutive numbers 1, 2, 3, . . . */
#define REH_LET		(3)	/* consecutive letters A, B, C, . . . */
#define REH_MNUM	(4)	/* use current measure number */
#define REH_BAR_MNUM	(5)	/* mnum using measnumstyle, (measnum not y/n) */

/* Values for the ps_usage (PostScript usage) field of PRINTDATA struct */
#define PU_NORMAL	(0)
#define PU_AFTERPROLOG	(1)
#define PU_BEFORETRAILER (2)
#define PU_ATPAGEBEGIN	(3)
#define PU_ATPAGEEND	(4)
#define PU_ATSCOREBEGIN	(5)
#define PU_ATSCOREEND	(6)
#define PU_MAX (7)  /* must be at least one more than largest used PU_ value */

/*
 * Define the style of (ways of drawing) rehearsal letters/numbers.
 */
#define	RS_PLAIN	(0)	/* just the letter/number */
#define	RS_BOXED	(1)	/* enclosed in a box */
#define	RS_CIRCLED	(2)	/* enclosed in a circle */

/*
 * Define where dots are to be drawn in repeatstart/repeatend/repeatboth.
 */
#define	RD_STANDARD	(0)	/* two dots, surrounding center staff line */
#define RD_ALL		(1)	/* dots in all the spaces between staff lines */

/*
 * Define the ways to print pedal markings.
 */
#define P_LINE		(0)	/* draw a line under where the pedal is down */
#define P_PEDSTAR	(1)	/* print "Ped." and the start and "*" at end */
#define P_ALTPEDSTAR	(2)	/* like P_PEDSTAR but only "Ped." when bounce*/

/*
 * Define the side of a stem that a partial beam can be on.  There is code in
 * prntdata.c that depends on these values, -1 and 1.
 */
#define PB_LEFT		(-1)
#define PB_RIGHT	(1)

/*
 * Define types of intervals.  There is code that depends on these being in
 * this order, starting from 0, counting up (like in trnspose.c).
 */
#define	DIMINISHED	(0)
#define	MINOR		(1)
#define	PERFECT		(2)
#define	MAJOR		(3)
#define	AUGMENTED	(4)

/*
 * Define values for the "useaccs" parameter.  The first value says don't do
 * anything; keep the key sig and don't add any accidentals to what the user
 * already used.  The other values throw the key signature away and use
 * accidentals to indicate the correct notes.  They differ only in whether to
 * use extra (technically redundant) accidentals, and/or remove unneeded
 * user accidentals.
 */
#define UA_N		(0)	/* don't change anything */
#define UA_Y_NONE	(1)	/* use only the accs standard notation needs */
#define UA_Y_NONNAT	(2)	/* every non-nat note, nats only if needed */
#define UA_Y_ALL	(3)	/* accs on every note */
#define UA_Y_NONEREMUSER   (4)	/* like NONE but remove unneeded user accs */
#define UA_Y_NONNATREMUSER (5)	/* like NONNAT but remove unneeded user accs */

/*
 * Define the supported tuning systems.
 */
#define	TU_EQUAL	(0)	/* equal temperament */
#define	TU_PYTHAGOREAN	(1)	/* Pythagorean: fifths are 3/2 */
#define	TU_MEANTONE	(2)	/* meantone: thirds are 5/4 */

/*
 * Define the input style.
 */
#define IS_VOICE_INPUT  (0)     /* voice-at-a-time (must be zero) */
#define IS_CHORD_INPUT  (1)	/* chord-at-a-time */

/*
 * Define the valid page sizes. The values must match the indexes of
 * pagesztbl in gram.y.
 */
#define PS_LETTER	(0)
#define PS_LEGAL	(1)
#define PS_FLSA		(2)
#define PS_HALFLETTER	(3)
#define PS_A4		(4)
#define PS_A5		(5)
#define PS_A6		(6)

/* Define page orientations */
#define O_PORTRAIT	(0)
#define O_LANDSCAPE	(1)

/*
 * Define bits in the map Gotheadfoot, to keep track of which of the
 * header/footer contexts have been defined.
 */
#define GOT_HEADER	(0x1)                
#define GOT_LHEADER	(0x2)
#define GOT_RHEADER	(0x4)
#define GOT_HEADER2	(0x8)
#define GOT_LHEADER2	(0x10)             
#define GOT_RHEADER2	(0x20)             
#define GOT_FOOTER	(0x40)               
#define GOT_LFOOTER	(0x80)              
#define GOT_RFOOTER	(0x100)
#define GOT_FOOTER2	(0x200)
#define GOT_LFOOTER2	(0x400)
#define GOT_RFOOTER2	(0x800)

/*
 * Define various constants =============================================
 */

/* the number of PostScript points in one inch */
#define PPI		(72.0)

/* the size of a point in inches */
#define POINT		(1.0 / PPI)

/* the distance between a note and the next higher note */
#define STEPSIZE	(3.0 * POINT)

/* size of white space for surrounding things that should be padded thus */
#define	STDPAD		(STEPSIZE / 3.0)

/* (distance between tab staff lines) / (distance between normal staff lines)*/
#define TABRATIO	(1.735)

/* scaling factor giving size of "sm" (small) music chars relative to normal */
#define SM_FACTOR	(0.65)

/* scaling factor giving size of "sm" (small) music chars relative to normal */
#define TINY_FACTOR	(0.45)

/* scaling factor giving size of a grace fret number relative to normal */
#define SMFRETSIZE	((int)(0.80 * DFLT_SIZE))
#define TINYFRETSIZE	((int)(0.60 * DFLT_SIZE))

/* font character size information is stored in FONTFACTORs of an inch */
#define FONTFACTOR	(1000.0)

/* widths of various types of lines, in points */
#define	W_NORMAL	(0.7)
#define	W_MEDIUM	(1.5)
#define	W_WIDE 		(3.0)

/* dimensions for slashesbetween, in stepsizes */
#define SL_BET_X_TOTAL	(8.0)	/* total width */
#define SL_BET_Y_TOTAL	(8.0)	/* total height */
#define SL_BET_Y_LINE	(1.5)	/* vertical thickness of one line */
#define SL_BET_Y_SPACE	(1.0)	/* vertical thickness of space between lines */

/* vertical distance between two flags or two beams */
#define	FLAGSEP		(1.6 * STEPSIZE)	/* for normal notes */
#define	SMFLAGSEP	(FLAGSEP * SM_FACTOR)	/* for cue or grace notes */
#define TINYFLAGSEP	(FLAGSEP * TINY_FACTOR)	/* grace note before a cue */

/* minimum width to be given to a multirest */
#define MINMULTIWIDTH	(0.7)

/* temporary width for a measure rest, serves as a marker */
#define	TEMPMRPTWIDTH	(0.02)

/* half the width of a restart bar */
#define HALF_RESTART_WIDTH	(12 * POINT)

/* padding provided to force more room between groups for various reasons */
#define	TIESLURPAD	(0.12)	/* ties and slurs */
#define	SLASHPAD	(0.06)	/* slashes through stems */
#define	ALTPAD		(0.12)	/* alternation beams between groups */
#define ROLLPADDING	(10 * STDPAD)	/* a roll to the left of a group */

/* padding to be put after a clef */
#define CLEFPAD		(2.0 * STDPAD)

/* how high certain things are */
#define TUPHEIGHT	(10.0/3.0 * STEPSIZE)	/* tuplet bracket */
#define OCTHEIGHT	(10.0/3.0 * STEPSIZE)	/* octave bracket */
#define ENDINGHEIGHT	(13.0/3.0 * STEPSIZE)	/* ending bracket */
#define MULTIHEIGHT	(12.0/3.0 * STEPSIZE)	/* multirest number */
#define MINWITHHEIGHT	(2.2 * STEPSIZE)	/* minimum "with" list item */

/* horizontal extent of a slash, in stepsizes */
#define SLASHHORZ	(5.0 / 3.0)

/*
 * The maximum number of notes there could ever be in a "hand" (the notes of a
 * chord that occur on a given staff) is the number of unique notes that can
 * exist, C0 through B9.
 */
#define MAXHAND		((MAXOCTAVE - MINOCTAVE + 1) * 7)

/* the length in the X direction of a tabslur to or from nowhere */
#define	SLIDEXLEN	(3.0 * STEPSIZE)

/* define the default distance, in stepsizes, between the lines of a grid */
#define	WHEREUSED_GS	(1.6)	/* when printed by the music */
#define	ATEND_GS	(2.0)	/* when printed at the end of the song */

/* define size of the bit field for phcount and thus the max number it can be */
#define PH_BITS		(3)
#define PH_COUNT	((1 << PH_BITS) - 1)

#define	NO		(0)
#define	YES		(1)

/* the "used" field in input SSVs uses the following in addition to YES/NO */
#define	UNSET		(2)

#ifndef PI
#define PI	(3.141592653589793)
#endif

#define	CMPERINCH	(2.54)		/* centimeters per inch */

/* number of rectangles to allocate at a time in Rectab */
#define RECTCHUNK       (100)

/*
 * Define miscellaneous macros =============================================
 */

/* number of elements in an array */
#define NUMELEM(a)	(sizeof(a) / sizeof((a)[0]))

/*
 * If memory debugging is turned on, include the header file for it, and define
 * macros to support memory debugging.  
 */
#ifdef MUP_ALLOC_DEBUG
#include "allocdebug.h"
#define STRINGIFY(s) #s
#define MALLOC_DEBUG_START(structtype, numelem, new_p) alloc_debug(AI_MALLOC, sizeof(struct structtype), numelem, 0, STRINGIFY(structtype), STRINGIFY(new_p));
#define MALLOC_DEBUG_END(new_p) alloc_debug_ret_value(new_p);
#define CALLOC_DEBUG_START(structtype, numelem, new_p) alloc_debug(AI_CALLOC, sizeof(struct structtype), numelem, 0, STRINGIFY(structtype), STRINGIFY(new_p));
#define CALLOC_DEBUG_END(new_p) alloc_debug_ret_value(new_p);
#define REALLOC_DEBUG_START(structtype, numelem, new_p) alloc_debug(AI_REALLOC, sizeof(struct structtype), numelem, new_p, STRINGIFY(structtype), STRINGIFY(new_p));
#define REALLOC_DEBUG_END(new_p) alloc_debug_ret_value(new_p);
#define MALLOCA_DEBUG_START(type, numelem, new_p) alloc_debug(AI_MALLOCA, sizeof(type), numelem, 0, STRINGIFY(type), STRINGIFY(new_p));
#define MALLOCA_DEBUG_END(new_p) alloc_debug_ret_value(new_p);
#define CALLOCA_DEBUG_START(type, numelem, new_p) alloc_debug(AI_CALLOCA, sizeof(type), numelem, 0, STRINGIFY(type), STRINGIFY(new_p));
#define CALLOCA_DEBUG_END(new_p) alloc_debug_ret_value(new_p);
#define REALLOCA_DEBUG_START(type, numelem, new_p) alloc_debug(AI_REALLOCA, sizeof(type), numelem, new_p, STRINGIFY(type), STRINGIFY(new_p));
#define REALLOCA_DEBUG_END(new_p) alloc_debug_ret_value(new_p);
#define FREE_DEBUG(mem_p) free_debug(mem_p)
#else
#define MALLOC_DEBUG_START(type, numelem, new_p)
#define MALLOC_DEBUG_END(new_p)
#define CALLOC_DEBUG_START(type, numelem, new_p)
#define CALLOC_DEBUG_END(new_p)
#define REALLOC_DEBUG_START(type, numelem, new_p)
#define REALLOC_DEBUG_END(new_p)
#define MALLOCA_DEBUG_START(type, numelem, new_p)
#define MALLOCA_DEBUG_END(new_p)
#define CALLOCA_DEBUG_START(type, numelem, new_p)
#define CALLOCA_DEBUG_END(new_p)
#define REALLOCA_DEBUG_START(type, numelem, new_p)
#define REALLOCA_DEBUG_END(new_p)
#define FREE_DEBUG(mem_p)  free(mem_p)
#endif

/*
 * Define macros for allocating structures.
 */
#define	MALLOC(structtype, new_p, numelem) {				\
	MALLOC_DEBUG_START(structtype, numelem, new_p)			\
	if ((new_p = (struct structtype *)malloc((unsigned)		\
			(((numelem) == 0 ? 1 : (numelem)) *		\
			sizeof(struct structtype)))) == 0)		\
		l_no_mem(__FILE__, __LINE__);				\
	MALLOC_DEBUG_END(new_p) \
}
#define	CALLOC(structtype, new_p, numelem) {				\
	CALLOC_DEBUG_START(structtype, numelem, new_p)			\
	if ((new_p = (struct structtype *)calloc(			\
			(numelem) == 0 ? 1 : (numelem),			\
			(unsigned)sizeof(struct structtype)))  == 0)	\
		l_no_mem(__FILE__, __LINE__);				\
	CALLOC_DEBUG_END(new_p) \
}
#ifndef __STDC__
#define	REALLOC(structtype, new_p, numelem) {				\
	REALLOC_DEBUG_START(structtype, numelem, new_p)			\
	if ((new_p = (struct structtype *)realloc((char *)(new_p),	\
			(unsigned)(((numelem) == 0 ? 1 : (numelem)) *	\
			sizeof(struct structtype)))) == 0) 		\
		l_no_mem(__FILE__, __LINE__);				\
	REALLOC_DEBUG_END(new_p) \
}
#else
#define	REALLOC(structtype, new_p, numelem) {				\
	REALLOC_DEBUG_START(structtype, numelem, new_p)			\
	if ((new_p = (struct structtype *)realloc((void *)(new_p),	\
			(unsigned)(((numelem) == 0 ? 1 : (numelem)) *	\
			sizeof(struct structtype)))) == 0) 		\
		l_no_mem(__FILE__, __LINE__);				\
	REALLOC_DEBUG_END(new_p) \
}
#endif

/*
 * Define macros for allocating other arrays.
 */
#define	MALLOCA(type, new_p, numelem) {					\
	MALLOCA_DEBUG_START(type, numelem, new_p)			\
	if ((new_p = (type *)malloc((unsigned)				\
			(((numelem) == 0 ? 1 : (numelem)) *		\
			sizeof(type)))) == 0)				\
		l_no_mem(__FILE__, __LINE__);				\
	MALLOCA_DEBUG_END(new_p) \
}
#define	CALLOCA(type, new_p, numelem) {					\
	CALLOCA_DEBUG_START(type, numelem, new_p)			\
	if ((new_p = (type *)calloc((numelem) == 0 ? 1 : (numelem),	\
			(unsigned)sizeof(type)))  == 0)			\
		l_no_mem(__FILE__, __LINE__);				\
	CALLOCA_DEBUG_END(new_p) \
}
#ifndef __STDC__
#define	REALLOCA(type, new_p, numelem) {				\
	REALLOCA_DEBUG_START(type, numelem, new_p)			\
	if ((new_p = (type *)realloc((char *)(new_p),			\
			(unsigned)(((numelem) == 0 ? 1 : (numelem)) *	\
			sizeof(type)))) == 0)				\
		l_no_mem(__FILE__, __LINE__);				\
	REALLOCA_DEBUG_END(new_p) \
}
#else
#define	REALLOCA(type, new_p, numelem) {				\
	REALLOCA_DEBUG_START(type, numelem, new_p)			\
	if ((new_p = (type *)realloc((void *)(new_p),			\
			(unsigned)(((numelem) == 0 ? 1 : (numelem)) *	\
			sizeof(type)))) == 0)				\
		l_no_mem(__FILE__, __LINE__);				\
	REALLOCA_DEBUG_END(new_p) \
}
#endif

/* define macro for freeing memory */
#ifdef __STDC__
#define	FREE(mem_p)	FREE_DEBUG((void*)mem_p)
#else
#define	FREE(mem_p)	FREE_DEBUG((char*)mem_p)
#endif

/* convert a RATIONAL to a float */
#define RAT2FLOAT(rat)	( (float)(rat).n / (float)(rat).d )

/* the usual minimum and maximum macros */
#define MAX(a, b)	( (a) > (b) ? (a) : (b) )
#define MIN(a, b)	( (a) < (b) ? (a) : (b) )

/* absolute value of the difference of a and b */
#define ABSDIFF(a, b)	( (a) > (b) ? (a) - (b) : (b) - (a) )

/* even and odd numbers (positive, zero, or negative; works on any compiler) */
#define	EVEN(a)		(abs(a) % 2 == 0)
#define	ODD(a)		(abs(a) % 2 == 1)

#define SQUARED(x)	((x) * (x))
#define NEARESTQUARTER(x) ( (int)((x) * 4.0 + 0.5) / 4.0 )

/* half the height of a staff in stepsizes; use 1 for 1-line staffs */
#define HALFSTAFF(s)	((svpath(s, STAFFLINES)->stafflines == 5) ? 4 : 1)

/* given ptr to a group, return the NOTE struct for note nearest to the beam */
#define BNOTE(gs_p)	\
	(gs_p)->notelist[ (gs_p)->stemdir == UP ? 0 : (gs_p)->nnotes - 1 ]

#define MAX_ACCS	(4)	/* max number of accidentals on one note */
#define NO_DEFERRED_ACC (-100)	/* used in Deferred_acc */
#define BAD_ACCS_OFFSET	(-100)	/* return value from accs_offset() */

#define EQ_ACCS(list1, list2)	(strncmp(list1, list2, MAX_ACCS * 2) == 0)
#define NE_ACCS(list1, list2)	(strncmp(list1, list2, MAX_ACCS * 2) != 0)
#define COPY_ACCS(list1, list2)	strncpy(list1, list2, MAX_ACCS * 2)
#define CLEAR_ACCS(list)	memset(list, 0, MAX_ACCS * 2)
#define ACC_IS_VISIBLE(acc)	(width((acc)[0], DFLT_SIZE, (acc)[1]) != 0.0)

/* does the given SSV set any of the 3 parameters concerned with tuning? */
/* THIS CODE MUST BE KEPT IN SYNC WITH THE CODE IN restoreparms() IN ssv.c */
#define TUNEPARMSSV(ssv_p)	(		\
	ssv_p->used[A4FREQ] == YES ||		\
	ssv_p->used[TUNING] == YES ||		\
	ssv_p->used[ACCTABLE] == YES		\
)
/*
 * Define indices into GRPSYL.notelist for the first (FNNI) and last (LNNI)
 * non-cross-staff-stemmed notes.  Also define indices for the first (FCNI)
 * and last (LCNI) cross-staff-stemmed notes.
 *
 * For FNNI and LNNI to work, there must be non-CSS notes, although even if
 * not, it's still okay to have loops like 
 *	for (n = FNNI(gs_p); n <= LNNI(gs_p); n++)
 * and the NNN macro, which uses them, always works.
 *
 * LCNI and FCNI only work if there are CSS notes.
 */
#define	FNNI(gs_p)	((gs_p)->stemto == CS_ABOVE ?	\
			(gs_p)->stemto_idx + 1 : 0)
#define	LNNI(gs_p)	((gs_p)->stemto == CS_BELOW ?	\
			(gs_p)->stemto_idx - 1 : (gs_p)->nnotes - 1)
#define	FCNI(gs_p)	((gs_p)->stemto == CS_ABOVE ?	\
			0 : (gs_p)->stemto_idx)
#define	LCNI(gs_p)	((gs_p)->stemto == CS_BELOW ?	\
			(gs_p)->nnotes - 1 : (gs_p)->stemto_idx)

/* define the Number of Normal Notes (non-CSS notes) in a group */
#define NNN(gs_p)	(LNNI(gs_p) - FNNI(gs_p) + 1)

/* test whether the note given via the index is CSS */
#define IS_CSS_NOTE(gs_p, idx)	(					\
	(((gs_p)->stemto == CS_ABOVE && idx <= (gs_p)->stemto_idx) ||	\
	 ((gs_p)->stemto == CS_BELOW && idx >= (gs_p)->stemto_idx))	\
)

/* test whether any CSS notes that exist would be on stem side of the group */
#define STEMSIDE_CSS(gs_p)						\
	(((gs_p)->stemto == CS_ABOVE && (gs_p)->stemdir == UP) ||	\
	 ((gs_p)->stemto == CS_BELOW && (gs_p)->stemdir == DOWN))

/*
 * Define CSS_STEPS to be an offset to be applied to "stepsup" when the note is
 * on the other staff.  It needs to be at twice as big as the interval between
 * the highest and lowest notes so that stepsup plus or minus half of it  will
 * become a value that it could never be for a normal note (see doacc()), and
 * it has to be an even number, because, for example, that matters for vertical
 * dot placement.
 */
#define CSS_STEPS	(MAXHAND * 2)

/* is this FEED followed by a CLEFSIG (music), not a block? */
#define IS_CLEFSIG_FEED(mll_p) ((mll_p)->str == S_FEED && \
		(mll_p)->next != 0 && (mll_p)->next->str == S_CLEFSIG)

/*
 * For the given note group (but not mrpt), check the side the stem is on, or
 * would be on.  Whole and dblwhole have a stemside even though no stem.  So
 * do groups with stemlen = 0.
 */
#define	STEMSIDE_RIGHT(gs_p)					\
	(gs_p->stemdir == UP || gs_p->basictime <= BT_QUAD)
#define STEMSIDE_LEFT(gs_p)	( ! STEMSIDE_RIGHT(gs_p) )

/*
 * For the given note group (but not mrpt), check whether it has a stem or not.
 * Stem length = 0 still counts as there being a stem.
 */
#define STEMLESS(gs_p)  ((gs_p)->basictime == 1 || (gs_p)->basictime == BT_DBL)
#define STEMMED(gs_p)   ( ! STEMLESS(gs_p) )

/*
 * For the given note group (but not mrpt), check the side the stem is on.
 * These are true only when there is a stem (so not for whole/dblwhole),
 * although stemlen = 0 still counts as there being a stem.
 */
#define	HAS_STEM_ON_RIGHT(gs_p)	( STEMSIDE_RIGHT(gs_p) && STEMMED(gs_p) )
#define	HAS_STEM_ON_LEFT(gs_p)	( STEMSIDE_LEFT(gs_p)  && STEMMED(gs_p) )

/*
 * This macro is to be used as in this example:
 *	extern int func P((int parm1, float parm2));
 * The ANSI version will result in a function prototype.  The other version
 * will result in a function declaration (no parameters).
 */
#ifdef __STDC__
#define	P(parms)	parms
#else
#define	P(parms)	()
#endif

#endif
