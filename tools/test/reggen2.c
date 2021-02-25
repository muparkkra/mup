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

/* This program generate random Mup input files and runs Mup on them, as a
 * way to do regression testing. If the program runs to completion before
 * a timeout value, and without generating a core dump, the input is discarded.
 * Otherwise, the file is kept as a way to potentially reproduce a bug.
 * On a reasonably fast machine, it is possible to run over a million test
 * cases in less than a day.
 * Arguments:
 *	-d		use mupdisp instead of mup
 *	-i iterations	generate this many tests. Default is ITERATIONS
 *					(ITERATIONS is #defined below)
 *	-n		also save files that give non-zero exit codes
 *	-t timeout	timeout value in seconds. Default is TIMEOUT
 *					(TIMEOUT is #defined below)
 *	-p prefix	use this prefix for generated files
 *	-v		verbose
 * If compiled with GENONLY defined, it will only generate files, not run
 * mup, useful for non-UNIX-like systems, since this uses fork/exec and
 * expects to be able to run "sh -c" 
 * With GENONLY, -i and -p are the only valid options.
 * GENONLY gets defined if __TURBOC__ or __WATCOMC__ or __DOS__ are defined.
 *
 * Typically, more than 90% of the generated files are not completely legal
 * Mup input, and the rest tend to have things no sane musician would actually
 * input, but that is a very good thing,
 * because we have found that most of the bugs
 * that remain in Mup after normal functional testing with good examples
 * are in obscure error cases, or interactions of strange combinations of
 * features that we would never think of testing.
 * It even randomly throws in a few typos.
 * So if this program runs through a million test cases without triggering any
 * core dumps or infinite loops, that means the code is probably pretty solid.
 *
 * Each input is run both for PostScript and MIDI output, since those go
 * through somewhat different code, and a -x option is used a random percentage
 * of the time, since that tends to interact with many other things.
 */

#ifdef __TURBOC__
#define __DOS__ 1
#endif
#ifdef __WATCOMC__
#define __DOS__ 1
#endif

#ifdef __DOS__
#define GENONLY 1
#endif

#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#ifndef GENONLY
#include <sys/wait.h>
#endif
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include "rational.h"

/* default values */
#define ITERATIONS	100
#define TIMEOUT		10


#define YES	1
#define NO	0

/* special flag to mark octave. Must be different from YES and NO */
#define OCTSTR  2
/* special flag to mark chord with grid.
 * Must be different than YES, NO, and OCTSTR */
#define GRIDCHORD 3

/* Values to control how mup input is generated */
short use1;		/* % of time to explicitly state voice 1,
			 *  when it will default to 1 anyway */
short measrest;		/* % of time to use measure rest */
short measspace;	/* % of time to use measure space */
short measrpt;		/* % of time to use measure repeat */
short sylposition;	/* % of time to specific sylposition parameter */
short staffpad;		/* % of time to specific staffpad parameter */
short scorepad;		/* % of time to specific scorepad parameter */
short rehstyle;		/* % of time to specific rehstyle parameter */
short brack;		/* % of time to use [ ] things */
short whitebefore;	/* % of time to put white space before token */
short whiteafter;	/* % of time to put white space after token */
short extranl;		/* % of time to add extra newline */
short swingunit;	/* % of time to use swingunit */
short stemlen;		/* % of time to use stemlen */
short stemshorten;	/* % of time to use stemshorten */
short staffmult;	/* % of time to have an SSV apply to multiple staffs */
short crnl;		/* % of time to use \r\n  */
short cr_only;		/* % of time to use \r alone as line separator */
short midmeas;		/* % of time to make mid-measure param chgs */
short noteheads;	/* % of time to use noteheads parameter */
short typo;		/* % of time to add a "typo" to the input */
short scorefeed;	/* % of time to use newscore */
short pagefeed;		/* % of time to use newpage */
short marginoverride;	/* % of time to override margin on feeds */
short restart;		/* % of time to use restart */
short samescore;	/* % of time to use samescore zone */
short samepage;		/* % of time to use samepage zone */
short dotted;		/* % of time to use dotted notes */
short dbldotted;	/* % of time to use double dotted notes */
short timedflt;		/* % of time to use time default value if possible */
short wrongtime;	/* % of time to use incorrect amount of time in meas */
short rest;		/* % of time to use rest */
short space;		/* % of time to use space */
short uncompressible;	/* % of time use make space uncompressible */
short subdivide;	/* % of time to subdivide notes */
short accidental;	/* % of time to use accidental */
short rehlet;		/* % of time to use reh let */
short rehnum;		/* % of time to use reh num */
short rehmnum;		/* % of time to use reh mnum */
short rehstr;		/* % of time to use reh "string" */
short multirest;	/* % of time to use multirest */
short restsymmult;	/* % of time to use restsymmult */
short printedtime;	/* % of time to use printedtime */
short additive_time;	/* % of time to use additive time */
short cross_staff_stems; /* % of time to use cross-staff stems */
short phrase;		/* % of time to generate phrase marks */
short octavesbeyond;	/* how many octave beyond default to use for notes */
short alt;		/* % of time to use alt */
short numalt;		/* argument to alt */
short custombeam;	/* % of time to do custom beaming */
short autobeaming;	/* % of time to do abm / eabm */
short esbm;		/* % of time to do esbm */
short ph_eph;		/* % of time to do ph - eph */
short slope;		/* % of time to specify slope on beam */
short compoundts;	/* % of time to use time sig with additive numerator */
short comments;		/* % of time to add comments */
short vcombine;		/* % of time to use vcombine parameter */
short pagesize;		/* % of time to use pagesize parameter */
short pedal;		/* % of time to use pedal */
short endped;		/* % of time to use endped */
short pedpermeas;	/* max pedal per measure */
short margins;		/* % of time to set margin */
short dist;		/* % of time to set dist/chorddist/crescdist */
short aligntag;		/* % of time to use align tag on STUFF */
short division;		/* % of time to set division parameter */
short endingstyle;	/* % of time to set endingstyle */
short label;		/* % of time to set label */
short measnum;		/* % of time to specify measnum */
short maxmeasures;	/* % of time to specify maxmeasures */
short maxscores;	/* % of time to specify maxscores */
short slashesbetween;	/* % of time to specify slashesbetween */
short bracketrepeats;	/* % of time to specify bracketrepeats */
short repeatdots;	/* % of time to specify repeatdots */
short withfont;		/* % of time to specify with font/familly/size */
short pack;		/* % of time to specify packfact or packexp */
short transpose;	/* % of time to transpose */
short chordtranslation; /* % of time to use chordtranslation */
short useaccs;		/* % of time to use useaccs */
short carryaccs;	/* % of time to use carryaccs */
short emptymeas;	/* % of time to use emptymeas */
short alignped;		/* % of time to use alignped */
short alignlabels;	/* % of time to use alignlabels */
short extendlyrics;	/* % of time to use extendlyrics */
short tag;		/* % of time to add tag */
short abstag;		/* % of time to use absolute tag */
short prints;		/* % of time to generate print commands */
short dd_bar;		/* % of time to do dashed/dotted bar lines */
short string_escapes;	/* % of time to do things like \v \s \f etc */
short modifier;		/* % of time to use chord/analysis/figbass */
short box;		/* % of time to do boxed string */
short staffscale;	/* % of time to use staffscale */
short multi_brack;	/* % of time to do more than one [] on group */
short gtc;		/* % of time to use ... (good til canceled) */
short unsetparam;	/* % of time to use use unset */
short viswhereused;	/* % of time to use visible=whereused */
short paragraph;	/* % of time to use paragraph */
short block;		/* % of time to use block */
short restcombine;	/* % of time to use restcombine param */
short firstpage;	/* % of time to use firstpage param */
short fretx, freto, fretdash;	/* % of time to use x, o, and - in grids */
short grids;		/* % of time to generate grids */
short gridfret;		/* % of time to use gridfret param */
short gridscale;	/* % of time to use gridscale param */
short gridparen;	/* % of time to use parentheses in grid definitions */
short mingridheight;	/* % of time to use mingridheight param */
short minalignscale;	/* % of time to use minalignscale param */
short orderlist;	/* % of time to use aboveorder, etc */
short phrasemod;	/* % of time to use dotted or dashed phrase */
short xoption;		/* % of time to use -x comand line argument */
short print;		/* % of time to generate print (x,y) "xxx" */
short line;		/* % of time to generate lines */
short curve;		/* % of time to generate curves (not bulge) */
short bulgecurve;	/* % of time to generate curves (using bulge) */
short keymaps;		/* % of time generate keymaps */
short tuning;		/* % of time to use tuning parameter */
short midi;		/* % of time to put midi things */
short midi_to;		/* % of time to add "to" to midi things */
short control;		/* % of time to use control context */
short saveparms;	/* % of time to use saveparms */
short restoreparms;	/* % of time to use restoreparms */
short barstyle;		/* % of time to generate barstyle parameter */
short subbarstyle;	/* % of time to generate subbarstyle parameter */
short midlinestemfloat; /* % of the time to generate midlinestemfloat parameter */
short beamslope;	/* % of the time to generate beamslope parameter */
short tupletslope;	/* % of the time to generate tupletslope parameter */
short numbermultrpt;	/* % of the time to generate numbermultrpt parameter */
short defaultphraseside; /* % of the time to generate defaultphraseside param */

short maxnotes;		/* notes per voice */
short maxsize;		/* maxsize to actually use */


/* constants that control how mup input is generated */
#define BEAMSTYLE	60	/* % of time to use beamstyle */
#define BEAMRESTS	50	/* % of time to use "r" on beamstyle */
#define STAFFTHINGS	60	/* % of time to use staff contexts */
#define KEY		30	/* % of time to set key */
#define MAJMIN		50	/* % of time to set maj/min */
#define CLEF		30	/* % of time to set clef */
#define VSCHEME2o	20	/* % of time to use 2o */
#define VSCHEME2f	20	/* % of time to use 2f */
#define VSCHEME3o	20	/* % of time to use 3o */
#define VSCHEME3f	20	/* % of time to use 3f */
#define VSCHEME1	20	/* % of time to explicitly use 1 */
#define ABSOCT		15	/* % of time to use absolute octave */
#define LYRICS		20	/* % of time to generate lyrics */
#define LYRABOVE	15	/* % of time to use lyrics above */
#define LYRBETWEEN	15	/* % of time to use lyrics between */
#define LYRBELOW	15	/* % of time to use explicit lyrics below */
#define LYRSPACE	15	/* % of time to use s on lyrics */
#define LYRDASH		20	/* % of time to use - between syllables */
#define UNDERSCORE	10	/* % of time to use underscore on syllables */
#define SMALL		5	/* % of time to use ? (small) notes */
#define STUFFABOVE	40	/* % of time to put stuff above */
#define STUFFBELOW	25	/* % of time to put stuff below */
#define STUFFBETWEEN	20	/* % of time to put stuff between */
#define CRESC		5	/* % of time to do cresc */
#define TILSAMEMEAS	30	/* % of time to do til within same measure */
#define TILMEASONLY	20	/* % of time to do til with just # nmeasures */
#define FAMILY		25	/* % of time to use explicit font family */
#define FNTSIZE		20	/* % of time to specify font size */
#define TILTEXT		10	/* % of time to use til clause on text stuff */
#define TEXTSTUFF	15	/* % of time to add text stuff items */
#define OCTSTUFF	5	/* % of time to generate octave marks */
#define HDFTFONT	15	/* % of time to use font in header/footer */
#define HDFTSIZE	15	/* % of time to use size in header/footer */
#define FNT		20	/* % of time to specify font on titles */
#define TITLE2		25	/* % of time to generate 2nd title string */
#define TITLE3		20	/* % of time to generate 2nd title string */
#define HDFT		20	/* % of time to generate a header/footer */
#define GRACE		5	/* % of time to add grace notes */
#define GRACETIME	30	/* % of time to explicitly specify time value on grace groups after the first */
#define SLASHGRACE	25	/* % of time to use slash on grace notes */
#define DFLTGRP		30	/* % of time to default pitches if possible */
#define BARPAD		5	/* % of time to add bar padding */
#define ENDING		15	/* % of time to use ending */
#define ENDENDING	30	/* % of time to end ending */
#define ONELETTAG	15	/* % of time to generate 1-letter tags */
#define TABSTRING	25	/* % of time to use each tab string */


#define MAXMEAS		12	/* how many measures max to generate */
#define MAXSTAFFS	8	/* how many staffs max to generate */
#define MINNUMERATOR	1	/* minimum time sig numerator */
#define MAXNUMERATOR	24	/* maximum time sig numerator */
#define MAXREHSTR	10	/* # characters in rehearsal string */
#define MAXSTR		12	/* max chars in stuff string */
#define MAXPARASTR	200	/* max chars in a paragraph */
#define MINSIZE		1	/* min point size to use */
#define MAXSIZE		100	/* max point size to use */
#define MINHDFTITEMS	0	/* minimum number of items in header/footer */
#define MAXHDFTITEMS	10	/* maximum number of items in header/footer */
#define MAXTITLE	30	/* maximum chars in a title string */
#define MAXGRACE	4	/* max # of grace groups before a real group */
#define MAXBRACK	3	/* max number of items in [] before group */
#define MINPAD		1	/* minimum padding */
#define MAXPAD		10	/* maximum padding */
#define MAXENDSTR	5	/* max size for ending string */
#define MAXLABEL	9	/* max strlen of label */
#define MAXTAGS		100	/* max number of tags */
#define MAXTAGLEN	7	/* max length of a tag */
#define MAXSTRINGS	9	/* maximum number of tablature strings */
#define MAX_KEYMAPS     5	/* maximum number of keymap contexts to generate */
#define MAX_KEYMAP_NAME_LEN     10	/* longest keymap name */


/* kinds of groups */
typedef enum { None, Rest, Space, Uspace, Notes } GTYPE;

/* handy rational constants */
RATIONAL Three_halves = { 3, 2 };	/* for dotted notes */
RATIONAL Seven_fourths = { 7, 4 };	/* for double-dotted notes */
RATIONAL Three_fourths = { 3, 4 };
RATIONAL Five_fourths = { 5, 4 };
RATIONAL Eighth = { 1, 8 };
RATIONAL One = { 1, 1 };
RATIONAL Two = { 2, 1 };
RATIONAL Four = { 4, 1 };
RATIONAL Eight = { 8, 1 };
RATIONAL Zero = { 0, 1};
RATIONAL N64 = { 1, 64 };
RATIONAL N128 = { 1, 128 };

/* the time value for a group. Enough for a measure are linked in a list */
struct GRP {
	RATIONAL	basictime;
	int 		dots;
	int		do_alt;	/* 0 = no alt, 1 = first of alt, 2 = second */
	int		custbeam;	/* 0 = no custom beam, 1 = bm, 2 =
					 * inside, 3 = ebm, 4 = esbm */
	int		autobeam;	/* 0 = none, 1 = abm, 2 = eabm */
	struct GRP	*next;
};

/* struct for list of items to choose from */
struct LIST {
	char 	*str;		/* value to be printed */
	int	used;		/* set to the current "generation." If this
				 * field is the same as the current generation
				 * number, this item will not be used. This
				 * will ensure that a value will only get used
				 * once */
};

/* The following are various lists of tiny snippets of Mup input that
 * we randomly choose from when we want to generate a particular type
 * of token that can have various values. When the number of valid choices
 * is small, it will be an exhastive list of those, otherwise a sampling.
 */

struct LIST Notelist[] = {
	{ "a0", 0 },
	{ "b0", 0 },
	{ "c0", 0 },
	{ "d0", 0 },
	{ "e0", 0 },
	{ "f0", 0 },
	{ "g0", 0 },
	{ "a1", 0 },
	{ "b1", 0 },
	{ "c1", 0 },
	{ "d1", 0 },
	{ "e1", 0 },
	{ "f1", 0 },
	{ "g1", 0 },
	{ "a2", 0 },
	{ "b2", 0 },
	{ "c2", 0 },
	{ "d2", 0 },
	{ "e2", 0 },
	{ "f2", 0 },
	{ "g2", 0 },
	{ "a3", 0 },
	{ "b3", 0 },
	{ "c3", 0 },
	{ "d3", 0 },
	{ "e3", 0 },
	{ "f3", 0 },
	{ "g3", 0 },
	{ "a4", 0 },
	{ "b4", 0 },
	{ "c4", 0 },
	{ "d4", 0 },
	{ "e4", 0 },
	{ "f4", 0 },
	{ "g4", 0 },
	{ "a5", 0 },
	{ "b5", 0 },
	{ "c5", 0 },
	{ "d5", 0 },
	{ "e5", 0 },
	{ "f5", 0 },
	{ "g5", 0 },
	{ "a6", 0 },
	{ "b6", 0 },
	{ "c6", 0 },
	{ "d6", 0 },
	{ "e6", 0 },
	{ "f6", 0 },
	{ "g6", 0 },
	{ "a7", 0 },
	{ "b7", 0 },
	{ "c7", 0 },
	{ "d7", 0 },
	{ "e7", 0 },
	{ "f7", 0 },
	{ "g7", 0 },
	{ "a8", 0 },
	{ "b8", 0 },
	{ "c8", 0 },
	{ "d8", 0 },
	{ "e8", 0 },
	{ "f8", 0 },
	{ "g8", 0 },
	{ "a9", 0 },
	{ "b9", 0 },
	{ "c9", 0 },
	{ "d9", 0 },
	{ "e9", 0 },
	{ "f9", 0 },
	{ "g9", 0 },
};


struct LIST Acclist[] = {
	{ "", 0 },
	{ "#", 0 },
	{ "&", 0 },
	{ "x", 0 },
	{ "&&", 0 },
	{ "n", 0 }
};

struct LIST Useaccslist[] = {
	{ "n", 0 },
	{ "y", 0 },
	{ "y none", 0 },
	{ "y all", 0 },
	{ "y nonnat", 0 },
};

struct LIST Barlist[] = {
	{ "bar", 0 },
	{ "dblbar", 0 },
	{ "invisbar", 0 },
	{ "repeatstart", 0 },
	{ "repeatend", 0 },
	{ "repeatboth", 0 },
	{ "endbar", 0 }
};

struct LIST Barstylelist[] = {
	{ "all", 0 },
	{ "between all", 0 },
	{ "1-2", 0 }
};

struct LIST Cleflist[] = {
	{ "treble", 0 },
	{ "bass", 0 },
	{ "soprano", 0 },
	{ "alto", 0 },
	{ "tenor", 0},
	{ "baritone", 0 },
	{ "mezzosoprano", 0 },
	{ "frenchviolin", 0 },
	{ "8treble", 0},
	{ "treble8", 0},
	{ "8bass", 0 },
	{ "bass8", 0 },
	{ "subbass", 0 },
};

struct LIST Familylist [] = {
	{ "times", 0 },
	{ "helvetica", 0 },
	{ "avantgarde", 0 },
	{ "newcentury", 0 },
	{ "courier", 0 },
	{ "bookman", 0 },
	{ "palatino", 0 }
};

struct LIST Fontlist[] = {
	{ "rom", 0 },
	{ "bold", 0 },
	{ "ital", 0 },
	{ "boldital", 0 }
};


struct LIST Stylelist[] = {
	{ "plain", 0 },
	{ "boxed", 0 },
	{ "circled", 0 }
};


struct LIST Headfootlist[] = {
	{ "top", 0 },
	{ "top2", 0 },
	{ "bottom", 0 },
	{ "bottom2", 0 },
	{ "header", 0 },
	{ "header2", 0 },
	{ "footer", 0 },
	{ "footer2", 0 }
};

struct LIST Pagesidelist[] = {
	{ "leftpage", 0 },
	{ "rightpage", 0 },
	{ "", 0 }
};

struct LIST Bracklist[] = {
	{ "cue", 0 },
	{ "xnote", 0 },
	{ "slash 1", 0 },
	{ "with .", 0 },
	{ "with \"sfz\"", 0 },
	{ "ho 2", 0 },
	{ "ho -", 0 },
	{ "hs \"semicirc\"", 0 },
	{ "hs \"allslash\"", 0 },
	{ "len 4" },
	{ "up", 0 },
	{ "down", 0 },
	{ "pad 2", 0 }
};


struct LIST Endingstylelist[] = {
	{ "top", 0 },
	{ "barred", 0 },
	{ "grouped", 0 }
};


struct LIST Xposelist[] = {
	{ "per 1", 0 },
	{ "min 2", 0 },
	{ "maj 2", 0 },
	{ "dim 2", 0 },
	{ "aug 2", 0 },
	{ "min 3", 0 },
	{ "maj 3", 0 },
	{ "dim 3", 0 },
	{ "aug 3", 0 },
	{ "perfect 4", 0 },
	{ "diminished 4", 0 },
	{ "augmented 4", 0 },
	{ "perfect 5", 0 },
	{ "diminished 5", 0 },
	{ "augmented 5", 0 },
	{ "min 6", 0 },
	{ "maj 6", 0 },
	{ "dim 6", 0 },
	{ "aug 6", 0 },
	{ "min 7", 0 },
	{ "maj 7", 0 },
	{ "dim 7", 0 },
	{ "aug 7", 0 }
};

struct LIST Chordtranslationlist[] = {
	{ "", 0 },
	{ "\"German\"", 0 },
	{ "\"do re mi fa sol la si\"", 0 },
	{ "\"foo Hjk zz89 345k+ o9ee MMMuy qw\"", 0 }
};

struct LIST Printcmdslist[] = {
	{ "print", 0 },
	{ "center", 0 },
	{ "left", 0 },
	{ "right", 0 }
};

struct LIST Paramlist[] = {
	{ "a4freq", 0 },
	{ "aboveorder", 0 },
	{ "acctable", 0 },
	{ "addtranspose", 0 },
	{ "alignlabels", 0 },
	{ "alignped", 0 },
	{ "barstyle", 0 },
	{ "beamslope", 0 },
	{ "beamstyle", 0 },
	{ "beloworder", 0 },
	{ "betweenorder", 0 },
	{ "bottommargin", 0 },
	{ "brace", 0 },
	{ "bracket", 0 },
	{ "bracketrepeats", 0 },
	{ "cancelkey", 0 },
	{ "chorddist", 0 },
	{ "chordtranslation", 0 },
	{ "clef", 0 },
	{ "cue", 0 },
	{ "defaultkeymap", 0 },
	{ "defaultphraseside", 0 },
	{ "defoct", 0 },
	{ "dist", 0 },
	{ "division", 0 },
	{ "dyndist", 0 },
	{ "emptymeas", 0 },
	{ "endingkeymap", 0 },
	{ "endingstyle", 0 },
	{ "extendlyrics", 0 },
	{ "firstpage", 0 },
	{ "flipmargins", 0 },
	{ "font", 0 },
	{ "fontfamily", 0 },
	{ "gridfret", 0 },
	{ "gridsatend", 0 },
	{ "gridswhereused", 0 },
	{ "gridscale", 0 },
	{ "key", 0 },
	{ "labelkeymap", 0 },
	{ "label2", 0 },
	{ "label", 0 },
	{ "leftmargin", 0 },
	{ "lyricsalign", 0 },
	{ "lyricsdist", 0 },
	{ "lyricsfont", 0 },
	{ "lyricsfontfamily", 0 },
	{ "lyricskeymap", 0 },
	{ "lyricssize", 0 },
	{ "maxmeasures", 0 },
	{ "maxscores", 0 },
	{ "measnum", 0 },
	{ "measnumfont", 0 },
	{ "measnumfontfamily", 0 },
	{ "measnumsize", 0 },
	{ "midlinestemfloat", 0 },
	{ "minalignscale", 0 },
	{ "mingridheight", 0 },
	{ "numbermrpt", 0 },
	{ "numbermultrpt", 0 },
	{ "ontheline", 0 },
	{ "packexp", 0 },
	{ "packfact", 0 },
	{ "pad", 0 },
	{ "pageheight", 0 },
	{ "pagewidth", 0 },
	{ "panelsperpage", 0 },
	{ "pedstyle", 0 },
	{ "printkeymap", 0 },
	{ "printmultnum", 0 },
	{ "rehearsalkeymap", 0 },
	{ "rehstyle", 0 },
	{ "release", 0 },
	{ "repeatdots", 0 },
	{ "restcombine", 0 },
	{ "rightmargin", 0 },
	{ "scale", 0 },
	{ "scorepad", 0 },
	{ "scoresep", 0 },
	{ "size", 0 },
	{ "slashesbetween", 0 },
	{ "stafflines", 0 },
	{ "staffpad", 0 },
	{ "staffs", 0 },
	{ "staffscale", 0 },
	{ "staffsep", 0 },
	{ "stemlen", 0 },
	{ "stemshorten", 0 },
	{ "subbarstyle", 0 },
	{ "swingunit", 0 },
	{ "sylposition", 0 },
	{ "tabwhitebox", 0 },
	{ "time", 0 },
	{ "timeunit", 0 },
	{ "topmargin", 0 },
	{ "transpose", 0 },
	{ "tuning", 0 },
	{ "tupletslope", 0 },
	{ "units", 0 },
	{ "useaccs", 0 },
	{ "carryaccs", 0 },
	{ "visible", 0 },
	{ "vcombine", 0 },
	{ "pagesize", 0 },
	{ "vscheme", 0 },
	{ "warn", 0 },
	{ "withfont", 0 },
	{ "withfontfamily", 0 },
	{ "withkeymap", 0 },
	{ "withsize", 0 }
};

struct LIST Vcombinelist[] = {
	{ "1,2", 0 },
	{ "1-3", 0 },
	{ "3, 2, 1 ", 0 },
	{ "2,1,3", 0 },
	{ "2", 0 }
};

struct LIST Vcombquallist[] = {
	{ "", 0 },
	{ "nooverlap", 0 },
	{ "shareone", 0 },
	{ "overlap", 0 },
	{ "restsonly", 0 }
};

struct LIST Pagesizelist[] = {
	{ "letter", 0 },
	{ "legal landscape", 0 },
	{ "legal portrait", 0 },
	{ "a4 landscape", 0 },
	{ "a5", 0 },
	{ "a6 portrait", 0 }
};

struct LIST Abovelist[] = {
	{ "mussym", 0 },
	{ "othertext", 0 },
	{ "chord", 0 },
	{ "ending", 0 },
	{ "rehearsal", 0 },
	{ "octave", 0 },
	{ "lyrics", 0 },
	{ "dyn", 0 },
};

struct LIST Belowlist[] = {
	{ "mussym", 0 },
	{ "othertext", 0 },
	{ "chord", 0 },
	{ "pedal", 0 },
	{ "octave", 0 },
	{ "lyrics", 0 },
	{ "dyn", 0 },
};

struct LIST Betweenlist[] = {
	{ "mussym", 0 },
	{ "othertext", 0 },
	{ "chord", 0 },
	{ "lyrics", 0 },
	{ "dyn", 0 },
};

struct LIST Keymaplist[] = {
	{ "defaultkeymap", 0 },
	{ "textkeymap", 0 },
	{ "lyricskeymap", 0 },
	{ "endingkeymap", 0 },
	{ "labelkeymap", 0 },
	{ "printkeymap", 0 },
	{ "rehearsalkeymap", 0 },
	{ "withkeymap", 0 },
};

struct LIST Tuninglist[] = {
	{ "equal", 0 },
	{ "meantone", 0 },
	{ "pythagorean", 0 }
};

struct LIST Noteheadlist[] = {
	{ "norm", 0 },
	{ "x", 0 },
	{ "allx", 0 },
	{ "diam", 0 },
	{ "blank", 0 },
	{ "righttri", 0 },
	{ "isostri", 0 },
	{ "rect", 0 },
	{ "pie", 0 },
	{ "semicirc", 0 },
	{ "slash", 0 },
	{ "allslash", 0 },
	{ "ILLEGAL", 0 }	/* to cause error cases */
};

struct LIST Noteshapeslist[] = {
	{ "octwhole", 0 },
	{ "quadwhole", 0 },
	{ "dblwhole", 0 },
	{ "1n", 0 },
	{ "2n", 0 },
	{ "4n", 0 },
	{ "xnote", 0 },
	{ "dwhdiamond", 0 },
	{ "diamond", 0 },
	{ "filldiamond", 0 },
	{ "dwhrighttriangle", 0 },
	{ "righttriangle", 0 },
	{ "fillrighttriangle", 0 },
	{ "udwhrighttriangle", 0 },
	{ "urighttriangle", 0 },
	{ "ufillrighttriangle", 0 },
	{ "dwhrectangle", 0 },
	{ "rectangle", 0 },
	{ "fillrectangle", 0 },
	{ "dwhisostriangle", 0 },
	{ "isostriangle", 0 },
	{ "fillisostriangle", 0 },
	{ "dwhpiewedge", 0 },
	{ "piewedge", 0 },
	{ "fillpiewedge", 0 },
	{ "dwhsemicircle", 0 },
	{ "semicircle", 0 },
	{ "fillsemicircle", 0 },
	{ "blankhead", 0 },
	{ "slashhead", 0 },
	{ "fillslashhead", 0 },
	{ "dwhslashhead", 0 },
	{ "INVALID", 0 }	/* to cause error cases */
};

struct LIST Midi_list[] = {
	{ "channel", 0 },
	{ "chanpressure", 0 },
	{ "cue", 0 },
	{ "hex", 0 },
	{ "instrument", 0 },
	{ "marker", 0 },
	{ "name", 0 },
	{ "offvelocity", 0 },
	{ "onvelocity", 0 },
	{ "parameter", 0 },
	{ "port", 0 },
	{ "program", 0 },
	{ "seqnum", 0 },
	{ "tempo", 0 },
	{ "text", 0 }
};

/* list of grid chords to use */
struct LIST *Gridlist;	/* malloc-ed */
int Numgrids;

char Keymapnames[MAX_KEYMAPS][MAX_KEYMAP_NAME_LEN + 1];
int Numkeymaps;

/* For save/restore. The names are for testing a typical name, a name with a
 * space, and a ridiculous but legal name. */
char *saveparms_names[] = { "xx", "a name", "!@,)&	+" };
#define MAX_SAVEPARMS 	(sizeof(saveparms_names) / sizeof(saveparms_names[0]))
short saveparms_used[MAX_SAVEPARMS];

/* info about each staff */
struct STAFFINFO {
	int	voices;		/* 1, 2, or 3 */
	int	defoct;		/* default octave */
} Staffinfo[MAXSTAFFS + 1];

struct STRINGDATA {
	char	pitch;
	char	accidental;
	short	nticks;
	short	octave;	/* greater than 9 means use default */
};

/* tag information */
char Tagtable[MAXTAGS][MAXTAGLEN+1] = {
	"_win", "_page", "_cur", "_score", "_staff.1" } ;
int Numtags = 3;

/* statistics */
int num_killed, num_core_dumped, num_exited_nonzero, num_midi_nonzero, num_nans;
int num_huge_nums;
int num_non_midi_tests, num_midi_tests;

short savenonzero = NO;	/* if to save files than cause non-zero exit */
int child;		/* PID of child process */
FILE *outf;		/* output file */
int In_ending;		/* if currently doing an ending */
short Pedstate[MAXSTAFFS + 1];	/* ped state, 0 off, 1 on */
short Samescore_inprog = NO;
short Samepage_inprog = NO;

/* argv processing globals */
extern int optind;
extern char *optarg;

/* function templates */
void gen(void);
void genmeas(int num, int den, int staff, int voice, int staffs);
struct GRP * gen_music(int num, int den, int staff);
int sometimes(int percent);
int myrandom(int min, int max);
void out(char *str);
void outfmt(char *fmt, ...);
void newline(void);
void genbar(int);
struct GRP *picktimes(struct GRP *grplist_p, RATIONAL remtime);
struct GRP *scramble_times(struct GRP *grplist_p);
void freegrps(struct GRP *g_p);
void picknotes(int n, int defoct);
void genbrack(void);
char *picklist(struct LIST *list_p, int leng, int generation);
void genstring(int leng);
char * create_a_string(int minleng, int leng);
char *create_a_name(int minleng, int leng);
void prtime(struct GRP *g_p);
void usage(char *pname);
void run_test(int index, char *command, char *suffix, char *prefix, int timeout, int verbose);
int exectest(char *cmd, int timeout, int verbose, int doing_midi, int *ret_p);
void alarmhandler(int sig);
void genlyrics(struct GRP *grplist_p, int staff, int staffs);
void gensyl(int mid);
void scramble_lines(long foffset, char repchar);
void cresc(int staff, int staffs, int meas, int measures, int num);
void place(int staff, int staffs, int between_ok, int req);
void stuff(int num, int str, int til, int measrem);
void textstuff(int staff, int staffs, int meas, int measures, int num);
void phrasestuff(int staff, int staffs, int meas, int measures, int num);
void octstuff(int staff, int staffs, int meas, int measures, int num);
void pedalstuff(int staff, int num);
void betweenbars(void);
void headfoot(void);
void initvals(void);
char *create_tag(void);
void genprint(void);
char *gettag(void);
struct STRINGDATA *tabstafflines(int *nstrings_p);
void picktabnotes(int nstrings, struct STRINGDATA *stringdata_p);
void do_a_tab_note(struct STRINGDATA *stringdata_p, int n);
void gen_grid(int gridlist_index);
void adjust_defoct(char * clefname, int staff);
void gen_coord_things(void);
void gen_keymaps(void);
void gen_til(int count100, int num, int measrem);
void gen_to(int minval, int maxval, int start, int ts_num, int measrem, int multi);
void gen_midi(int staffs, int ts_num, int measrem);
void gen_barstyle(void);
void gen_subbarstyle(void);



int
main(int argc, char **argv)
{
	int a;				/* cmd line arg */
	int iterations = ITERATIONS;	/* how many tests to run */
	int timeout = TIMEOUT;		/* how long to wait before assuming
					 * program is stuck in a loop */
	char *command = "mup";		/* what program to run and its args */
	char *suffix = "> RegGen2.out";	/* end of command after file name */
	char *prefix = "RGtest";	/* prefix for file names */
	int verbose = 0;
	int index;			/* loop index */
#ifdef __DOS__
	extern int _fmode;
#endif


#ifdef __DOS__
	_fmode = O_BINARY;
#endif

#ifdef GENONLY
	while ((a = getopt(argc, argv, "i:p:")) != EOF) {
#else
	while ((a = getopt(argc, argv, "di:np:t:v")) != EOF) {
#endif
		switch(a) {
#ifndef GENONLY
		case 'd':
			command = "mupdisp";
			suffix = "";
			timeout = 0;
			break;
#endif
		case 'i':
			iterations = atoi(optarg);
			break;
#ifndef GENONLY
		case 'n':
			savenonzero = YES;
			break;
#endif
		case 'p':
			prefix = optarg;
			break;
#ifndef GENONLY
		case 't':
			timeout = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usage(argv[0]);
			break;
		}
	}
	if (optind != argc) {
		usage(argv[0]);
	}

	/* seed random number generator */
	srand(time((long *) 0));

	/* Turn on malloc flag to make certain error blatant that might
	 * otherwise get through. */
	setenv("MALLOC_PERTURB_", "29", 1);


	/* run as many tests as specified */
	for (index = 1; index <= iterations; index++) {
		run_test(index, command, suffix, prefix, timeout, verbose);
		/* clean up */
		if (Numgrids > 0) {
			int g;
			for (g = 0; g < Numgrids; g++) {
				free(Gridlist[g].str);
			}
			free(Gridlist);
			Gridlist = 0;
			Numgrids = 0;
		}
		Numtags = 3;  /* _cur, _win, and _page */
	}

#ifndef GENONLY
	fprintf(stderr, "\n%d non-midi and %d midi tests run.\n %d core dumps, %d killed, %d contained 'nan', %d contained huge numbers,\n%d exited non-zero (%d non-midi / %d midi)\n",
		num_non_midi_tests, num_midi_tests, num_core_dumped,
		num_killed, num_nans, num_huge_nums, num_exited_nonzero,
		num_exited_nonzero - num_midi_nonzero, num_midi_nonzero);
#endif
	unlink("RegGen2.out");
	/* core dumps is a definite bug. Killed due to timeout is almost
	 * certainly a bug. Seeing a "nan" can have
	 * false positives, so we'll let them go. */
	return(num_core_dumped + num_killed);
}


/* Generate various score level SSV things */

void
gen_score_ssv()
{
	int val;

	if (sometimes(dist)) {
		out("dist=");
		if (sometimes(dist)) {
			outfmt("%d", myrandom(0, 10));
		}
		else {
			/* use floating point */
			outfmt("%d.%d", myrandom(0, 10), myrandom(0,999));
		}
		newline();
	}
	if (sometimes(dist)) {
		out("chorddist=");
		if (sometimes(dist)) {
			outfmt("%d", myrandom(0, 10));
		}
		else {
			/* use floating point */
			outfmt("%d.%d", myrandom(0, 10), myrandom(0,999));
		}
		newline();
	}
	if (sometimes(dist)) {
		out("dyndist=");
		if (sometimes(dist)) {
			outfmt("%d", myrandom(0, 10));
		}
		else {
			/* use floating point */
			outfmt("%d.%d", myrandom(0, 10), myrandom(0,999));
		}
		newline();
	}
	if (sometimes(dist)) {
		out("lyricsdist=");
		if (sometimes(dist)) {
			outfmt("%d", myrandom(0, 10));
		}
		else {
			/* use floating point */
			outfmt("%d.%d", myrandom(0, 10), myrandom(0,999));
		}
		newline();
	}

	if (sometimes(endingstyle)) {
		out("endingstyle=");
		out(picklist(Endingstylelist,
			sizeof(Endingstylelist) / sizeof(struct LIST), -1));
		newline();
	}

	if (sometimes(label)) {
		out("label=");
		genstring(MAXLABEL);
		newline();
	}
	if (sometimes(label)) {
		out("label2=");
		genstring(MAXLABEL);
		newline();
	}
	if (sometimes(measnum)) {
		out("measnum=");
		out(sometimes(50) ? "y" : "n");
		newline();
		if (sometimes(50)) {
			outfmt("measnumsize=%d", myrandom(1, 20));
			newline();
		}
		if (sometimes(50)) {
			out("measnumfont=");
			out(picklist(Fontlist, sizeof(Fontlist)
						/ sizeof(struct LIST), -1));
			newline();
		}
		if (sometimes(50)) {
			out("measnumfontfamily=");
			out(picklist(Familylist, sizeof(Familylist)
						/ sizeof(struct LIST), -1));
			newline();
		}
		if (sometimes(50)) {
			out("measnumstyle=");
			out(picklist(Stylelist, sizeof(Stylelist)
						/ sizeof(struct LIST), -1));
			newline();
		}
	}
	if (sometimes(maxmeasures)) {
		outfmt("maxmeasures=%d", myrandom(1, 10));
		newline();
	}
	if (sometimes(maxscores)) {
		outfmt("maxscores=%d", myrandom(1, 5));
		newline();
	}
	if (sometimes(slashesbetween)) {
		out("slashesbetween=");
		out(sometimes(50) ? "y" : "n");
		newline();
	}

	if (sometimes(bracketrepeats)) {
		out("bracketrepeats=");
		out(sometimes(50) ? "y" : "n");
		newline();
	}

	if (sometimes(repeatdots)) {
		out("repeatdots=");
		out(sometimes(50) ? "all" : "standard");
		newline();
	}

	if (sometimes(withfont)) {
		out("withfontfamily=");
		out(picklist(Familylist, sizeof(Familylist)
						/ sizeof(struct LIST), -1));
		newline();
	}

	if (sometimes(withfont)) {
		out("withfont=");
		out(picklist(Fontlist, sizeof(Fontlist)
						/ sizeof(struct LIST), -1));
		newline();
	}

	if (sometimes(withfont)) {
		outfmt("withsize=%d", myrandom(1, 20));
		newline();
	}

	if (sometimes(pack)) {
		out("packexp=");
		val = myrandom(0, 100);
		outfmt("%d.%02d", val / 100, val % 100);
		newline();
	}
	if (sometimes(pack)) {
		out("packfact=");
		val = myrandom(0, 100);
		outfmt("%d.%1d", val / 10, val % 10);
		newline();
	}

	if (sometimes(transpose)) {
		out("transpose=");
		out(sometimes(50) ? "up " : "down ");
		out(picklist(Xposelist, sizeof(Xposelist) / sizeof(struct LIST),
				-1));
		newline();
	}

	if (sometimes(chordtranslation)) {
		out("chordtranslation=");
		out(picklist(Chordtranslationlist,
			sizeof(Chordtranslationlist) / sizeof(struct LIST), -1));
		newline();
	}

	if (sometimes(useaccs)) {
		out("useaccs=");
		out(picklist(Useaccslist, sizeof(Useaccslist) / sizeof(struct LIST), -1));
		newline();
	}

	if (sometimes(carryaccs)) {
		out("carryaccs=");
		out(sometimes(50) ? "y" : "n");
		newline();
	}

	if (sometimes(alignped)) {
		out("alignped=");
		out(sometimes(50) ? "y" : "n");
		newline();
	}
	
	if (sometimes(alignlabels)) {
		out("alignlabels=");
		out(sometimes(50) ? "left" : "center");
		newline();
	}
	if (sometimes(extendlyrics)) {
		out("extendlyrics=");
		out(sometimes(75) ? "y" : "n");
		newline();
	}

	if (sometimes(orderlist)) {
		static int ordergeneration;
		int i;
		int items;
		int use;	/* how many items to actually use */

		ordergeneration++;
		if (sometimes(75)) {
			items = sizeof(Abovelist) / sizeof(struct LIST);
			use = myrandom(items - 3, items);
			outfmt("aboveorder=");
			for (i = 0; i < use; i++) {
				outfmt( "%s", picklist(Abovelist, items, ordergeneration));
				if (i < use - 1) {
					outfmt(",");
				}
			}
			newline();
		}
		ordergeneration++;
		if (sometimes(75)) {
			items = sizeof(Belowlist) / sizeof(struct LIST);
			use = myrandom(items - 2, items);
			outfmt("beloworder=");
			for (i = 0; i < use; i++) {
				outfmt( "%s", picklist(Belowlist, items, ordergeneration));
				if (i < use - 1) {
					outfmt(",");
				}
			}
			newline();
		}
		ordergeneration++;
		if (sometimes(75)) {
			items = sizeof(Betweenlist) / sizeof(struct LIST);
			use = myrandom(items - 1, items);
			outfmt("betweenorder=");
			for (i = 0; i < use; i++) {
				outfmt( "%s", picklist(Betweenlist, items, ordergeneration));
				if (i < use - 1) {
					outfmt(",");
				}
			}
			newline();
		}
	}

	if (sometimes(noteheads)) {
		int i;
		out("noteheads =\"");
		for (i = 0; i < 7; i++) {
			outfmt("%s ",picklist(Noteheadlist, sizeof(Noteheadlist) /
					sizeof(struct LIST), -1));
		}
		out("\"");
		newline();
	}

	if (sometimes(sylposition)) {
		outfmt("sylposition = %d", myrandom(0, 30) - 15);
		newline();
	}
	if (sometimes(staffpad)) {
		outfmt("staffpad = %d", myrandom(0, 12) - 6);
		newline();
	}
	if (sometimes(scorepad)) {
		if (sometimes(50)) {
			outfmt("scorepad = %d", myrandom(0, 12) - 6);
		}
		else {
			outfmt("scorepad = %d, %d", myrandom(0, 12) - 6,
						myrandom(0, 20) - 2);
		}
		newline();
	}
	if (sometimes(rehstyle)) {
		out("rehstyle = ");
		out(picklist(Stylelist, sizeof(Stylelist)
						/ sizeof(struct LIST), -1));
		newline();
	}

	if (sometimes(stemlen)) {
		outfmt("stemlen= %d", myrandom(0,10));
		if (sometimes(stemlen)) {
			outfmt(".%d", myrandom(0,9));
		}
		out("\n");
	}

	if (sometimes(stemshorten)) {
		outfmt("stemshorten=%d", myrandom(0,2));
		if (sometimes(stemshorten)) {
			outfmt(".%d", myrandom(0,9));
		}
		if (sometimes(stemshorten)) {
			outfmt(",%d", myrandom(0,6));
			if (sometimes(stemshorten)) {
				outfmt(".%d", myrandom(0,9));
			}
			if (sometimes(stemshorten)) {
				outfmt(",%d,%d", myrandom(-4, 50), myrandom(-4, 50));
			}
		}
		out("\n");
	}

	if (sometimes(swingunit)) {
		switch (myrandom(0, 3)) {
		case 0:
			outfmt("swingunit = 8\n");
			break;
		case 1:
			outfmt("swingunit = 4\n");
			break;
		case 2:
			outfmt("swingunit = 4.\n");
			break;
		case 3:
			outfmt("swingunit = 2\n");
			break;
		}
	}

	if (sometimes(barstyle)) {
		gen_barstyle();
	}
	if (sometimes(subbarstyle)) {
		gen_subbarstyle();
	}
	if (sometimes(midlinestemfloat)) {
		out("midlinestemfloat=");
		out(sometimes(50) ? "y" : "n");
		newline();
	}
	if (sometimes(beamslope)) {
		out("beamslope=");
		outfmt("0.%d, %d", myrandom(1, 9), myrandom(0, 45));
		newline();
	}
	if (sometimes(tupletslope)) {
		out("tupletslope=");
		outfmt("0.%d, %d", myrandom(1, 9), myrandom(0, 45));
		newline();
	}
	if (sometimes(numbermultrpt)) {
		out("numbermultrpt=");
		out(sometimes(50) ? "y" : "n");
		newline();
	}
	if (sometimes(defaultphraseside)) {
		out("defaultphraseside=");
		out(sometimes(50) ? "above" : "below");
		newline();
	}
}


/* generate a test file */

void
gen()
{
	int measures;			/* how many measures to generate */
	int staffs;			/* how many staffs */
	int voices;
	int num;			/* time signature */
	int den;
	int s;				/* staff index */
	int i;				/* index in saveparms_used */
	RATIONAL meastime;
	struct GRP *grplist_p, *g_p;
	long f_offset;			/* file offset */
	int meas;			/* current measure number */
	int marg;			/* margin in 100th of an inch */
	int val;			/* value for parameters */
	short otherstaffs[MAXSTAFFS+1];	/* for doing multiple SSVs */
	int extrastaff;			/* when doing multiple SSVs */
	int set_vscheme;		/* YES or NO */
	int ss;				/* another staff index */


	/* initialize to not having done any saves */
	for (i = 0; i < MAX_SAVEPARMS; i++) {
		saveparms_used[i] = NO;
	}

	/* decide some fundamental things */
	measures = myrandom(1, MAXMEAS);
	staffs = myrandom(1, MAXSTAFFS);
	num = myrandom(MINNUMERATOR, MAXNUMERATOR);
	den = (int) (pow(2.0, (double) myrandom(0, 6)));
	In_ending = NO;

	if (sometimes(noteheads)) {
		int i, j;
		/* generate a headshapes context */
		out("headshapes");
		newline();
		for (i = myrandom(0,4); i > 0; i--) {
			/* usually do user-defines,
			 * but sometimes override builtin */
			if (sometimes(70)) {
				genstring(30);
			}
			else {
				outfmt("%s ",picklist(Noteheadlist, sizeof(Noteheadlist) /
					sizeof(struct LIST), -1));
			}
			out("\"");
			for (j = 0; j < 4; j++) {
				out(picklist(Noteshapeslist, sizeof(Noteshapeslist) /
					sizeof(struct LIST), -1));
				if (j < 3) {
					out(" ");
				}
			}
			out("\"");
			newline();
		}
	}

	if (sometimes(keymaps)) {
		gen_keymaps();
	}
	else {
		Numkeymaps = 0;
	}

	/* do the score things */
	out("score");
	newline();
	out("staffs=");
	outfmt("%d\ntime=", staffs);
	if (sometimes(compoundts)) {
		int remaining, part;

		for (remaining = num; remaining > 2; remaining -= part) {
			part = myrandom(1, remaining - 1);
			outfmt("%d+", part);
		}
		outfmt("%d/%d\n", remaining, den);
	}
	else {
		outfmt("%d/%d\n", num, den);
	}

	/* save file offset for scrambling */
	f_offset = ftell(outf);

	out("scale=");
	outfmt("0.%d", myrandom(4,9));
	newline();

	/* establish a beamstyle */
	if (sometimes(BEAMSTYLE)) {
		meastime.n = num; meastime.d = den;
		rred ( &meastime);
		grplist_p = picktimes( (struct GRP *) 0, meastime);
		grplist_p = scramble_times(grplist_p);
		out("beamstyle=");
		for (g_p = grplist_p; g_p != (struct GRP *) 0; g_p = g_p->next) {
			prtime(g_p);
			while (sometimes(additive_time) && g_p->next != 0) {
				out("+");
				g_p = g_p->next;
				prtime(g_p);
			}
			if (g_p->next != (struct GRP *) 0) {
				out(",");
			}
		}
		if (sometimes(BEAMRESTS)) {
			out(" r");
		}
		newline();
		freegrps(grplist_p);
	}

	/* establish global vscheme */
	if (sometimes(VSCHEME2o)) {
		out("vscheme=");
		out("2o");
		newline();
		voices = 2;
	}
	else if (sometimes(VSCHEME2f)) {
		out("vscheme=");
		out("2f");
		newline();
		voices = 2;
	}
	else if (sometimes(VSCHEME1)) {
		out("vscheme=");
		out("1");
		newline();
		voices = 1;
	}
	else if (sometimes(VSCHEME3f)) {
		out("vscheme=");
		out("3f");
		newline();
		voices = 3;
	}
	else if (sometimes(VSCHEME3o)) {
		out("vscheme=");
		out("3o");
		newline();
		voices = 3;
	}
	else {
		voices = 1;
	}

	/* sometimes set margins */
	if (sometimes(margins)) {
		marg = myrandom(0, 110);
		out("topmargin=");
		outfmt("%d.%d", marg / 100, marg % 100);
		newline();
	}
	if (sometimes(margins)) {
		marg = myrandom(0, 110);
		out(sometimes(50) ? "bottommargin" : "botmargin=");
		outfmt("%d.%d", marg / 100, marg % 100);
		newline();
	}
	if (sometimes(margins)) {
		marg = myrandom(0, 110);
		out("leftmargin=");
		outfmt("%d.%d", marg / 100, marg % 100);
		newline();
	}
	if (sometimes(margins)) {
		marg = myrandom(0, 110);
		out("rightmargin=");
		outfmt("%d.%d", marg / 100, marg % 100);
		newline();
	}
	if (sometimes(margins)) {
		outfmt("flipmargins=%s", (sometimes(50) ? "y" : "n"));
		newline();
	}
	gen_score_ssv();

	if (sometimes(division)) {
		out("division=");
		outfmt("%d", 96 * myrandom(1, 4));
		newline();
	}

	if (Numkeymaps > 0) {
		while (sometimes(60)) {
			outfmt("%s=\"%s\"", picklist(Keymaplist,
				sizeof(Keymaplist) / sizeof(struct LIST), -1),
				Keymapnames[myrandom(0, Numkeymaps-1)]);
			newline();
		}
	}

	if (sometimes(tuning)) {
		outfmt("tuning = %s\n", 
			picklist(Tuninglist, sizeof(Tuninglist)
						/ sizeof(struct LIST), -1));
	}

	if (sometimes(emptymeas)) {
		struct GRP *grplist_p;
		out("emptymeas = \"");
		grplist_p = gen_music(num, den, 1);
		out("\"");
		newline();
		if (grplist_p != 0) {
			freegrps(grplist_p);
		}
	}

	if (sometimes(restsymmult)) {
		out("restsymmult=y");
		newline();
	}

	if (sometimes(printedtime)) {
		out("printedtime=");
		genstring(4);
		if (myrandom(1, 10) > 3) {
			genstring(4);
		}
		newline();
	}

	if (sometimes(vcombine)) {
		outfmt("vcombine = %s", picklist(Vcombinelist,
			sizeof(Vcombinelist) / sizeof(struct LIST), -1));
		outfmt(" %s ", picklist(Vcombquallist,
			sizeof(Vcombquallist) / sizeof(struct LIST), -1));
		if (sometimes(10)) {
			out(" bymeas");
		}
		newline();

	}

	if (sometimes(pagesize)) {
		outfmt("pagesize = %s", picklist(Pagesizelist,
			sizeof(Pagesizelist) / sizeof(struct LIST), -1));
		newline();
	}


	if (sometimes(restcombine)) {
		outfmt("restcombine=%d", myrandom(2, 10));
		newline();
	}

	if (sometimes(firstpage)) {
		outfmt("firstpage=%d", myrandom(1, 400));
		newline();
	}

	if (sometimes(gridfret)) {
		outfmt("gridfret=%d", myrandom(2, 30));
		newline();
	}

	if (sometimes(gridscale)) {
		outfmt("gridscale=%d.%d", myrandom(0, 1), myrandom(1,9));
		newline();
	}

	if (sometimes(mingridheight)) {
		outfmt("mingridheight=%d\n", myrandom(3,15));
		newline();
	}

	if (sometimes(minalignscale)) {
		outfmt("minalignscale=0.%d\n", myrandom(1,99));
		newline();
	}
	
	if (sometimes(grids)) {
		if (sometimes(75)) {
			outfmt("gridswhereused=y");
			newline();
		}
		if (sometimes(75)) {
			outfmt("gridsatend=y");
			newline();
		}
	}



	/* rearrange the score parameters */
	scramble_lines(f_offset, '\n');

	/* pick clefs, keys, etc for each staff */
	for (s = 1; s <= staffs; s++) {

		Staffinfo[s].voices = voices;
		Staffinfo[s].defoct = 4;

		if(sometimes(STAFFTHINGS)) {
			out("staff ");
			outfmt("%d", s);
			for (ss = 1; ss <= staffs; ss++) {
				otherstaffs[ss] = NO;
			}
			if (sometimes(staffmult)) {
				extrastaff = myrandom(1, staffs);
				outfmt(",%d", extrastaff);
				otherstaffs[extrastaff] = YES;
			}
			if (sometimes(staffmult)) {
				extrastaff = myrandom(1, staffs);
				outfmt(" & %d", extrastaff);
				otherstaffs[extrastaff] = YES;
			}
			newline();
			if(sometimes(KEY)) {
				out("key=");
				outfmt("%d", myrandom(0,7));
				out(sometimes(50) ? "#" : "&");
				if (sometimes(MAJMIN)) {
					out(sometimes(50) ? "maj" : "min");
				}
				newline();
			}
			if (sometimes(CLEF)) {
				char *str;

				out("clef=");
				str = picklist(Cleflist, sizeof(Cleflist) /
						sizeof(struct LIST), -1);
				out(str);
				adjust_defoct(str, s);
				for (ss = 1; ss <= staffs; ss++) {
					if (otherstaffs[ss] == YES) {
						adjust_defoct(str, ss);
					}
				}
				
				newline();
			}

			/* per staff vscheme */
			set_vscheme = NO;
			if (sometimes(VSCHEME2o)) {
				out("vscheme=");
				out("2o");
				newline();
				Staffinfo[s].voices = 2;
				set_vscheme = YES;
			}
			else if (sometimes(VSCHEME2f)) {
				out("vscheme=");
				out("2f");
				newline();
				Staffinfo[s].voices = 2;
				set_vscheme = YES;
			}
			else if (sometimes(VSCHEME1)) {
				out("vscheme=");
				out("1");
				newline();
				Staffinfo[s].voices = 1;
				set_vscheme = YES;
			}
			else if (sometimes(VSCHEME3f)) {
				out("vscheme=");
				out("3f");
				newline();
				Staffinfo[s].voices = 3;
				set_vscheme = YES;
			}
			else if (sometimes(VSCHEME3o)) {
				out("vscheme=");
				out("3o");
				newline();
				Staffinfo[s].voices = 3;
				set_vscheme = YES;
			}
			if (set_vscheme == YES) {
				for (ss = 1; ss <= staffs; ss++) {
					if (otherstaffs[ss] == YES) {
						Staffinfo[ss].voices = Staffinfo[s].voices;
					}
				}
			}

			if (sometimes(unsetparam)) {
				out("unset ");
				out(picklist(Paramlist, sizeof(Paramlist) / sizeof(struct LIST), -1));
				newline();
			}

			if (sometimes(viswhereused)) {
				out("visible=whereused");
				newline();
			}

			if (sometimes(label)) {
				out("label=");
				genstring(MAXLABEL);
				newline();
			}
			if (sometimes(label)) {
				out("label2=");
				genstring(MAXLABEL);
				newline();
			}
			if (sometimes(staffscale)) {
				out("staffscale=");
				outfmt("%d.%d", myrandom(0,1), myrandom(0, 9));
				newline();
			}
		}
	}

	/* generate some grids */
	if (sometimes(grids)) {
		int g;

		out("grids");
		newline();
		Numgrids = myrandom(0, 50);
		/* get space to save the names for use in chord statements */
		if ((Gridlist = (struct LIST *) malloc(Numgrids * sizeof(struct LIST))) == 0) {
			fprintf(stderr, "failed to malloc grid memory\n");
			exit(1);
		}

		for (g = 0; g < Numgrids; g++) {
			gen_grid(g);
		}
	}

	/* go into music context */
	out("music");
	newline();

	/* generate as many measures as needed for this test */
	for (meas = 1; meas <= measures; meas++) {

		if (sometimes(multirest)) {
			out("multirest ");
			outfmt("%d", myrandom(2, 100));
			newline();
		}

		else {
			/* save file offset for scrambling lines later */
			f_offset = ftell(outf);

			/* generate everything for this measure */
			for (s = 1; s <= staffs; s++) {
				genmeas(num, den, s, 1, staffs);
				if (Staffinfo[s].voices > 1) {
					genmeas(num, den, s, 2, staffs);
				}
				if (Staffinfo[s].voices > 2) {
					genmeas(num, den, s, 3, staffs);
				}
				if (sometimes(CRESC)) {
					cresc(s, staffs, meas, measures, num);
				}
				if (sometimes(TEXTSTUFF)) {
					textstuff(s, staffs, meas,
								measures, num);
				}
				if (sometimes(phrase)) {
					phrasestuff(s, staffs, meas,
								measures, num);
				}
				if (sometimes(OCTSTUFF)) {
					octstuff(s, staffs, meas,
								measures, num);
				}
				if (sometimes(pedal)) {
					pedalstuff(s, num);
				}
				if (sometimes(midi)) {
					gen_midi(staffs, num, measures - meas);
				}

				if (sometimes(prints)) {
					genprint();
				}
				if (sometimes(comments)) {
					out("//");
					genstring(100);
					newline();
				}
			}

			/* rearrange the lines */
			scramble_lines(f_offset, '\n');
		}
		gen_coord_things();
		genbar(meas == measures ? NO : YES);
		betweenbars();
	}
	newline();

	if (sometimes(typo)) {
		/* "mistype" one random byte in the file */
		fseek(outf, myrandom(0, ftell(outf) - 1), SEEK_SET);
		outfmt("%c", myrandom(32, 127));
	}
}


/* generate a measure for a given staff and voice */

void
genmeas(num, den, staff, voice, staffs)

int num;
int den;
int staff;
int voice;
int staffs;	/* total number of staffs */

{
	struct GRP *grplist_p, *g_p;

	/* generate stuff before colon */
	outfmt("%d", staff);
	if (voice == 1) {
		if( sometimes(use1)) {
			outfmt(" %d", voice);
		}
	}
	else {
		outfmt(" %d", voice);
	}
	out(":");

	grplist_p = gen_music(num, den, staff);
	newline();

	if (grplist_p == 0) {
		return;
	}

	/* for testing it might be nice to have lyrics with time unrelated to
	 * note times, but that very quickly makes thing too wide to fit, and
	 * is not normal, so for now, reuse note times */
	if (sometimes(LYRICS) && voice == 1) {

		/* put back the original time values of alt groups, since
		 * lyrics don't have alt */
		for (g_p = grplist_p; g_p != (struct GRP *) 0; g_p = g_p->next) {
			if (g_p->do_alt != 0) {
				g_p->basictime = rdiv(g_p->basictime, Two);
			}
		}
		
		genlyrics(grplist_p, staff, staffs);
	}

	/* free info */
	freegrps(grplist_p);
}


/* generate a measure worth of music, either for normal music input
 * of for an emptymeas parameter value. */

struct GRP *
gen_music(num, den, staff)

int num;
int den;
int staff;

{
	struct GRP *grplist_p, *g_p;
	RATIONAL meastime;
	struct GRP lastgrp;
	GTYPE type;
	int ngrace;		/* number of grace chords */
	int g;			/* grace loop counter */
	GTYPE lasttype = None;
	char * clefname;
	int need_comma;		/* if did an inter-group thing */
	int need_eph = 0;	/* did a ph that needs a matching eph */
	int doing_autobeam;	/* abm / eadm */
	

	/* sometimes do measure rest or space or rpt */
	if (sometimes(measrest)) {
		out("mr;");
		return(0);
	}
	if (sometimes(measspace)) {
		out("ms;");
		return(0);
	}
	if (sometimes(measrpt)) {
		if (sometimes(20)) {
			out("dbl");
		}
		else if (sometimes(15)) {
			out("quad");
		}
		out("mrpt;");
		return(0);
	}

	/* figure out time values first */
	meastime.n = num; meastime.d = den;
	rred ( &meastime);
	grplist_p = picktimes( (struct GRP *) 0, meastime);
	grplist_p = scramble_times(grplist_p);

	/* set up beginning default time value */
	/* TODO: really should be based on timeunit */
	lastgrp.basictime.n = 1;
	lastgrp.basictime.d = den;
	lastgrp.dots = 0;

	/* figure out which ones to make alt pairs. If consecutive groups
	 * have same time value, can make into alt */
	for (g_p = grplist_p; g_p != (struct GRP *) 0; g_p = g_p->next) {
		if (g_p->next != (struct GRP *) 0 &&
				EQ(g_p->basictime, g_p->next->basictime) &&
				g_p->dots == g_p->next->dots &&
				LT(g_p->basictime, Two) &&
				sometimes(alt)) {
			g_p->basictime = rmul(g_p->basictime, Two);
			g_p->next->basictime = g_p->basictime;
			g_p->do_alt = 1;
			g_p = g_p->next;
			g_p->do_alt = 2;
		}
		else {
			g_p->do_alt = 0;
		}
	}

	/* figure out which to connect with custom beams */
	for (g_p = grplist_p; g_p != (struct GRP *) 0; g_p = g_p->next) {
		/* assume no custom beaming */
		g_p->custbeam = 0;

		/* if there is a run of eighth or shorter, sometimes beam */
		if (LE(g_p->basictime, Eighth) && sometimes(custombeam)) {
			if (g_p->next != (struct GRP *) 0 &&
					LE(g_p->next->basictime, Eighth)) {
				g_p->custbeam = 1;
				for (g_p = g_p->next; g_p->next != (struct GRP *) 0
						&& LE(g_p->next->basictime, Eighth) &&
						sometimes(custombeam);
						g_p = g_p->next) {
					if (sometimes(esbm)) {
						g_p->custbeam = 4;
					}
					else {
						g_p->custbeam = 2;
					}
				}
				g_p->custbeam = 3;
			}
		}
	}

	doing_autobeam = NO;
	for (g_p = grplist_p; g_p != (struct GRP *) 0; g_p = g_p->next) {
		if (doing_autobeam == YES) {
			/* Need to stop if at end of measure, or will be
			 * hitting custom beam in next group,
			 * or sometimes stop, even if we could go on */
			if (g_p->next == 0 || g_p->next->custbeam != 0 || sometimes(20)) {
				g_p->autobeam = 2;
				doing_autobeam = NO;
			}
		}
		else if (sometimes(autobeaming)) {
			if (g_p->next != 0 && g_p->next->custbeam == 0) {
				/* We can do autobeaming */
				g_p->autobeam = 1;
				doing_autobeam = YES;
			}
		}
	}

	/* do each group */
	for (g_p = grplist_p; g_p != (struct GRP *) 0; g_p = g_p->next) {

		need_comma = 0;
		/* do mid-measure changes */
		if (sometimes(midmeas)) {
			char * contxt, param[100];
			switch (myrandom(0, 4)) {
			default:
			case 0:
			case 3:
				contxt = "score";
				break;
			case 1:
			case 4:
				contxt = "staff";
				break;
			case 2:
				/* clef can't be on voice, so use less often */
				contxt = "voice";
				break;
			}
			switch (myrandom(0, 2)) {
			default:
			case 0:
				clefname = picklist(Cleflist,
					sizeof(Cleflist) / sizeof(struct LIST), -1);
				sprintf(param, "clef=%s", clefname);
				if (strcmp(contxt, "score") == 0) {
					int st;
					for (st = 1; st <= MAXSTAFFS; st++) {
						adjust_defoct(clefname, st);
					}
				}
				else {
					adjust_defoct(clefname, staff);
				}
				break;
			case 1:
				/* This won't really work right for "voice"
				 * level changes to defoct. Oh well. */
				Staffinfo[staff].defoct = myrandom(0, 9);
				if (strcmp(contxt, "score") == 0) {
					int st;
					for (st = 1; st <= MAXSTAFFS; st++) {
						Staffinfo[st].defoct = Staffinfo[staff].defoct;
					}
				}
				sprintf(param, "defoct=%d", Staffinfo[staff].defoct);
				break;
			case 2:
				sprintf(param, "release=%d", myrandom(0,400));
				break;
			}
			outfmt("<<%s %s>>", contxt, param);
		}
		/* choose rest, space, or notes */
		if (sometimes(rest) && g_p->do_alt == 0 && g_p->custbeam == 0) {
			type = Rest;
		}
		else if (sometimes(space) && g_p->do_alt == 0
							&& g_p->custbeam == 0) {
			if (sometimes(uncompressible)) {
				type = Uspace;
			}
			else {
				type = Space;
			}
		}
		else {
			type = Notes;

			/* sometimes add some grace notes */
			if (sometimes(GRACE) && g_p->do_alt != 2) {
				ngrace = myrandom(1, MAXGRACE);
				for (g = 0; g < ngrace; g++) {
					out("[");
					/* explicitly say "grace" on first group,
					 * and half the time on
					 * subsequent grace groups.
					 * Otherwise just use [] */
					if (g == 0 || sometimes(50)) {
						out("grace");
						if (sometimes(SLASHGRACE) &&
								ngrace == 1) {
							out(";slash 1");
						}
					}
					/* sometime force stem direction */
					if (sometimes(GRACE)) {
						if (sometimes(50)) {
							out(";up");
						}
						else {
							out(";down");
						}
					}
					out("]");
					/* on first grace group and some others,
					 * give a time value from 8 to 256 */
					if (g == 0 || sometimes(GRACETIME)) {
						lastgrp.basictime.n = 1;
						lastgrp.basictime.d
							= (8 << myrandom(0, 5));
						lastgrp.dots = 0;
						outfmt("%d", lastgrp.basictime.d);
					}
					picknotes(myrandom(1, maxnotes),
								Staffinfo[staff].defoct);
					out(";");
				}
			}

			/* sometimes add [] things */
			if (sometimes(brack)) {
				genbrack();
			}
		}

		/* do time value  */
		if ( EQ(lastgrp.basictime, g_p->basictime)
				&& lastgrp.dots == g_p->dots
				&& sometimes(timedflt)) {
			/* use default value */
			out("");
		}
		else {
			prtime(g_p);
			while (sometimes(additive_time) && g_p->next != 0) {
				g_p = g_p->next;
				out("+");
				prtime(g_p);
			}
		}
		lastgrp = *g_p;

		switch (type) {
		case Rest:
			if (lasttype == Rest && sometimes(DFLTGRP)) {
				out("");
			}
			else {
				out("r");
			}
			break;
		case Uspace:
			if (lasttype == Uspace && sometimes(DFLTGRP)) {
				out("");
			}
			else {
				out("us");
			}
			break;
		case Space:
			if (lasttype == Space && sometimes(DFLTGRP)) {
				out("");
			}
			else {
				out("s");
			}
			break;
		default:
			if (lasttype == Notes && sometimes(DFLTGRP)) {
				out("");
			}
			else {
				/* do some random notes */
				picknotes(myrandom(1, maxnotes),
						Staffinfo[staff].defoct);
			}

			/* sometimes do alt */
			if (g_p->do_alt == 1) {
				out(" alt");
				outfmt("%d", myrandom(1, numalt));
				if (sometimes(slope)) {
					outfmt(", slope %d", myrandom(-30, 30));
				}
				need_comma = 1;
			}

			/* do any custom beaming */
			if (g_p->custbeam == 1) {
				if (need_comma == 1) {
					out(",");
				}
				need_comma = 1;
				out(" bm ");
				if (sometimes(slope)) {
					outfmt(", slope %d", myrandom(-30, 30));
				}
			}
			else if (g_p->custbeam == 3) {
				if (need_comma == 1) {
					out(",");
				}
				need_comma = 1;
				out(" ebm ");
			}
			else if (g_p->custbeam == 4) {
				if (need_comma == 1) {
					out(",");
				}
				need_comma = 1;
				out(" esbm ");
			}

			if (g_p->autobeam == 1) {
				out("abm");
			}
			else if (g_p->autobeam == 2) {
				out("eabm");
			}

			/* This will only test non-nested ph - eph inside a
			 * single measure, but is better than no test at all...
			 */
			if (need_eph && (g_p->next == 0 || sometimes(30))) {
				if (need_comma == 1) {
					out(",");
				}
				need_comma = 1;
				out("eph");
				need_eph = 0;
			}
			if (sometimes(ph_eph) && g_p->next != 0 && need_eph == 0) {
				char *phside;
				if (need_comma == 1) {
					out(",");
				}
				need_comma = 1;
				if (sometimes(30)) {
					phside = "above";
				}
				else if (sometimes(30)) {
					phside = "below";
				}
				else {
					phside = "";
				}
				outfmt("ph %s", phside);
				need_eph = 1;
			}
			/* TODO: could sometimes do tie */
		}

		out(";");
		lasttype = type;
	}
	return(grplist_p);
}


/* return YES or NO randomly according to percentage YES */

int
sometimes(percent)

int percent;

{
	return ((rand() % 100) < percent ? YES : NO);
}


/* return random number between min and max inclusive */

int
myrandom(min, max)

int min;
int max;

{
	return ((rand() % (max - min + 1)) + min);
}


/* output a string, with random white space on either side */

void
out(str)

char *str;

{
	if (sometimes(whitebefore)) {
		fprintf(outf, " ");
	}
	fprintf(outf, "%s", str);
	if (sometimes(whiteafter)) {
		fprintf(outf, " ");
	}
}


/* printf-like output, without white space padding */

void
outfmt(char *fmt, ...)

{
	va_list args;

	va_start(args, fmt);
	vfprintf(outf, fmt, args);
	va_end(args);
}


/* do newline, sometimes with an extra one */

void
newline()
{
	if (sometimes(crnl)) {
		out("\r\n");
	}
	else if (sometimes(cr_only)) {
		out("\r");
	}
	else {
		out("\n");
	}
	if (sometimes(extranl)) {
		out("\n");
	}
}


/* generate bar line */

void
genbar(int feed_ok)
{
	long f_off;	/* offset in file */


	if (sometimes(dd_bar)) {
		out( sometimes(50) ? " dashed " : " dotted " );
	}

	out(picklist(Barlist, sizeof(Barlist) / sizeof(struct LIST), -1));

	/* save file location for scrambling */
	f_off = ftell(outf);

	/* add optional things. End each with newline for scrambling */
	/* sometimes add a rehearsal mark */
	if (sometimes(rehlet)) {
		out(" reh ");
		outfmt("let\n");
	}
	else if (sometimes(rehnum)) {
		out(" reh ");
		outfmt("num\n");
	}
	else if (sometimes(rehmnum)) {
		out(" reh ");
		outfmt("mnum\n");
	}
	else if (sometimes(rehstr)) {
		out(" reh ");
		genstring(MAXREHSTR);
		outfmt("\n");
	}

	/* add padding sometimes */
	if(sometimes(BARPAD)) {
		outfmt(" pad %d\n", myrandom(MINPAD, MAXPAD));
	}

	/* sometimes do ending */
	if (sometimes(ENDING)) {
		if (In_ending == YES && sometimes(ENDENDING)) {
			outfmt(" endending\n");
			In_ending = NO;
		}
		else if (feed_ok) {
			out(" ending ");
			genstring(MAXENDSTR);
			outfmt("\n");
			In_ending = YES;
		}
	}

	scramble_lines(f_off, ' ');

	newline();

	if (feed_ok) {
		/* adds feeds and similar things sometimes */
		if (Samescore_inprog == YES && sometimes(55)) {
			out("samescoreend");
			newline();
			Samescore_inprog = NO;
		}
		if (Samepage_inprog == YES && sometimes(55)) {
			out("samepageend");
			newline();
			Samepage_inprog = NO;
		}
		else if (sometimes(scorefeed)) {
			out("newscore");
			if (sometimes(marginoverride)) {
				outfmt(" rightmargin %d.%d", myrandom(0, 2), myrandom(0,100) );
			}
			else if (sometimes(marginoverride)) {
				outfmt(" rightmargin = auto");
			}
			if (sometimes(marginoverride)) {
				outfmt(" leftmargin %d.%d", myrandom(0, 2), myrandom(0,100) );
			}
			newline();
		}
		else if (sometimes(pagefeed)) {
			out("newpage");
			if (sometimes(marginoverride)) {
				outfmt(" leftmargin %d.%d", myrandom(0, 2), myrandom(0,100) );
			}
			if (sometimes(marginoverride)) {
				outfmt(" rightmargin %d.%d", myrandom(0, 2), myrandom(0,100) );
			}
			else if (sometimes(marginoverride)) {
				outfmt(" rightmargin = auto");
			}
			newline();
		}
		else if (sometimes(restart)) {
			out("restart");
			newline();
		}
		else {
			if (sometimes(samescore)) {
				out("samescorebegin");
				newline();
				Samescore_inprog = YES;
			}
			if (sometimes(samepage)) {
				out("samepagebegin");
				newline();
				Samepage_inprog = YES;
			}
		}
	}

	if (sometimes(control)) {
		out("control");
		newline();

		if (sometimes(saveparms)) {
			int spi;

			spi = myrandom(0, MAX_SAVEPARMS - 1);
			outfmt("saveparms \"%s\"\n", saveparms_names[spi]);
			saveparms_used[spi] = YES;
		}
		if (sometimes(restoreparms)) {
			int spi;
			int count;

			/* Start at a random place in the saveparms list,
			 * and look for one that has been used. */
			spi = myrandom(0, MAX_SAVEPARMS - 1);
			for(count = 0; count < MAX_SAVEPARMS; count++) {
				if (saveparms_used[spi] == YES) {
					/* Found one. restore from it */
					outfmt("restoreparms \"%s\"\n", saveparms_names[spi]);
					break;
				}
				/* move to next in list, wrapping if needed */
				if (++spi >= MAX_SAVEPARMS) {
					spi = 0;
				}
			}
		}

		/* Change some parameters */
		if (sometimes(50)) {
			out("score");
			gen_score_ssv();
		}
		newline();

		/* return to music context */
		out("music");
		newline();
	}
}


/* create a list of GRPs that add up to remtime. Return pointer to list */

struct GRP *
picktimes(grplist_p, remtime)

struct GRP *grplist_p;
RATIONAL remtime;

{
	RATIONAL basictime;
	RATIONAL fulltime;
	int dots;
	struct GRP *grp_p;


	/* Start with oct whole note, optionally with dots.
	 * Go shorter from there if necessary to fit in the remaining
	 * time in the measure. */
	fulltime = basictime = Eight;
	dots = 0;
	if (sometimes(dotted)) {
		dots = 1;
		fulltime = rmul(basictime, Three_halves);
		if (sometimes(dbldotted)) {
			dots = 2;
			fulltime = rmul(basictime, Seven_fourths);
		}
	}

	/* keep shortening til no longer than remaining time */
	while (GT(fulltime, remtime)) {

		/* undo dots sometimes, if any, to shorten */
		/* also undo dots if they might cause us to need notes
		 * shorter than a 256th */
		if (dots == 2 && (sometimes(dbldotted) || LE(basictime, N64))) {
			dots = 1;
			fulltime = rmul(basictime, Three_halves);
		}
		else if (dots == 1 && (sometimes(dotted) || LE(basictime, N128))) {
			dots = 0;
			fulltime = basictime;
		}
		else {
			basictime = rdiv(basictime, Two);
			fulltime = rdiv(fulltime, Two);
		}
	}

	/* save info about the amount of time chosen */
	if ((grp_p = (struct GRP *) malloc(sizeof(struct GRP))) ==
				(struct GRP *) 0) {
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	grp_p->basictime = basictime;
	grp_p->dots = dots;
	grp_p->next = grplist_p;
	grplist_p = grp_p;

	/* find out how much time is left */
	remtime = rsub(remtime, fulltime);

	/* Sometimes introduce a "bug" in the input, with too much time,
	 * or use the auto padding feature with too little */
	if (sometimes(wrongtime) && remtime.d < 16) {
		if (GE(remtime, Three_fourths)) {
			remtime = rmul(remtime, Three_fourths);
		}
		else {
			remtime = rmul(remtime, Five_fourths);
		}
	}
	/* if still time left, recurse to create more groups */
	if (GT(remtime, Zero)) {
		grplist_p = picktimes(grplist_p, remtime);
	}

	return(grplist_p);
}


/* recursively free list of GRP structs */

void
freegrps(g_p)

struct GRP *g_p;

{
	if (g_p == (struct GRP *) 0) {
		return;
	}

	freegrps(g_p->next);
	free(g_p);
}


/* given a list of group times, rearrange them and subdivide some of the
 * times, etc  */

struct GRP *
scramble_times(grplist_p)

struct GRP *grplist_p;

{
	struct GRP *g_p;
	struct GRP *grp_p;


	/* go down list, take some groups and subdivide */
	for (g_p = grplist_p; g_p != (struct GRP *) 0; g_p = g_p->next) {
		if (sometimes(subdivide)) {
			if (g_p->basictime.d < 256) {
				g_p->basictime = rdiv(g_p->basictime, Two);
				if ((grp_p = (struct GRP *) malloc(sizeof(struct GRP))) ==
							(struct GRP *) 0) {
					fprintf(stderr, "malloc failed\n");
					exit(1);
				}

				grp_p->basictime = g_p->basictime;
				grp_p->dots = g_p->dots;
				grp_p->next = g_p->next;
				g_p->next = grp_p;
			}
		}
	}

	/* TODO: could shuffle them around */

	return(grplist_p);
}



/* pick random notes to make up a chord */

void
picknotes(n, defoct)

int n;
int defoct;

{
	static int notegeneration = 0;
	char *str;
	int octave;
	struct LIST *list_p;
	int listleng;
	int css_index;		/* cross-staff stem index */


	/* calculate subset of notelist to use, based on octave range. Constrain
	 * to be within valid range */
	octave = defoct - octavesbeyond;
	if (octave < 0) {
		octave = 0;
	}
	list_p = &(Notelist[octave * 7]);
	listleng = (octavesbeyond * 2 + 1) * 7;
	if (defoct + octavesbeyond > 9) {
		listleng -= 7 * (defoct + octavesbeyond - 9);
	}

	/* pick  appropriate number of notes, some with accidentals */
	notegeneration++;
	if (sometimes(cross_staff_stems)) {
		css_index = myrandom(1, n);
	}
	else {
		css_index = -1;
	}
	for (  ; n > 0; n--) {
		str = picklist(list_p, listleng, notegeneration);
		if (str != (char *) 0) {
			if (css_index == n) {
				outfmt(" with ");
			}
			outfmt("%c", str[0]);
		}
		else {
			break;
		}
		octave = str[1] - '0';

		if (sometimes(accidental)) {
			str = picklist(Acclist,
				sizeof(Acclist) / sizeof(struct LIST), -1);
			if (str != (char *) 0) {
				out(str);
			}
		}

		/* handle octave */
		if (sometimes(ABSOCT)) {
			outfmt("%d", octave);
		}
		else if (octave > defoct) {
			for (octave -= defoct; octave > 0; octave--) {
				out("+");
			}
		}
		else if (octave < defoct) {
			for (octave = defoct - octave; octave > 0; octave--) {
				out("-");
			}
		}

		/* sometimes mark as cue size */
		if (sometimes(SMALL)) {
			out("?");
		}
		/* add tag sometimes */
		if (sometimes(tag)) {
			out("=");
			out(create_tag());
			out(" ");
		}
		/* sometimes add head shape override */
		if (sometimes(noteheads) && sometimes(10)) {
			outfmt("hs \"%s\"", picklist(Noteheadlist,
				sizeof(Noteheadlist) / sizeof(struct LIST), -1));
		}
	}
	if (css_index > 0) {
		if (sometimes(50)) {
			outfmt("above");
		}
		else {
			outfmt("below");
		}
	}
}


/* generate things in [ ] before group */

void
genbrack()
{
	static int brackgen = 0;
	char *str;
	int i;


	out("[");
	brackgen++;
	for (i = myrandom(1, MAXBRACK); i > 0; i--) {
		str = picklist(Bracklist,
			sizeof(Bracklist) / sizeof (struct LIST), brackgen);
		if (str != (char *) 0) {
			out(str);
			if (i > 1) {
				/* it's legal to have more than one [ ] */
				if (sometimes(multi_brack)) {
					out("][");
				}
				else {
					out(";");
				}
			}
		}
	}
	if (sometimes(tag)) {
		out(";=");
		out(create_tag());
		out(" ");
	}
	out("]");
	if (sometimes(gtc)) {
		out("...");
	}
}


/* return a random item from given list. If generation is set to -1, return
 * any item. If not, return only items that haven't been used yet this
 * generation. */

char *
picklist(list_p, leng, generation)

struct LIST *list_p;
int leng;
int generation;

{
	int choice;
	int retries = 0;

	/* choose random item on list. If everything chosen is already used,
	 * give up eventually */
	do {
		choice = myrandom(0, leng - 1);	
		if (generation == -1) {
			return(list_p[choice].str);
		}
		if (list_p[choice].used != generation) {
			list_p[choice].used = generation;
			return(list_p[choice].str);
		}
	} while (++ retries < leng * 4);

	return(char *) 0;
}


/* print a string up to leng characters long, any printable character */

void
genstring(leng)

int leng;

{
	outfmt("%s", create_a_string(0, leng));
}

/* create a random string, returning it in static buffer */

char *
create_a_string(int minleng, int leng)
{
	char ch;
	int do_box;
	static char buff[BUFSIZ];
	int offset;

	if (leng > sizeof(buff) - 1) {
		leng = sizeof(buff) - 1;
	}
	leng = myrandom(minleng, leng);
	offset = 0;

	buff[offset++] = '"';
	/* Yes, I mean single = here */
	if ((do_box = sometimes(box)) != 0) {
		buff[offset++] = '\\';
		buff[offset++] = '[';
	}
	for (  ; leng > 0; leng--) {
		if (sometimes(string_escapes)) {
			switch (myrandom(1, 10)) {
			case 1:
				buff[offset++] = '\\';
				buff[offset++] = 'b';
				break;
			case 2:
				buff[offset++] = '\\';
				buff[offset++] = 'n';
				break;
			case 3:
				buff[offset++] = '\\';
				buff[offset++] = '/';
				break;
			case 4:
				buff[offset++] = '\\';
				buff[offset++] = ':';
				break;
			case 5:
				sprintf(buff+offset, "\\s(+%d)", myrandom(1,5));
				while (buff[offset] != '\0') {
					offset++;
				}
				break;
			case 6:
				sprintf(buff+offset, "\\v(%d)", myrandom(1,100));
				while (buff[offset] != '\0') {
					offset++;
				}
				break;
			case 7:
				buff[offset++] = '\\';
				buff[offset++] = '%';
				break;
			case 8:
				buff[offset++] = '\\';
				buff[offset++] = '#';
				break;
			case 9:
				if (Numkeymaps > 0) {
					int m;
					char *mapname;

					buff[offset++] = '\\';
					buff[offset++] = 'm';
					buff[offset++] = '(';
					mapname = Keymapnames[myrandom(0, Numkeymaps-1)];
					for (m = 0; mapname[m] != 0; m++) {
						buff[offset++] = mapname[m];
					} 
					buff[offset++] = ')';
					break;
				}
				/* Fall through to default if no keymaps */
			default:
				sprintf(buff+offset,
				"\\f(%s %s)", picklist(Familylist,
				sizeof(Familylist) / sizeof(struct LIST), - 1),
				picklist(Fontlist,
				sizeof(Fontlist) / sizeof(struct LIST), -1));
				while (buff[offset] != '\0') {
					offset++;
				}
				break;
			}
			continue;
		}

		if (sometimes(15)) {
			ch = ' ';	/* real text has a lot of spaces */
		}
		else {
			ch = myrandom((int)' ', (int)'~');
			/* backslash things that need that */
			if (ch == '\\' || ch == '"') {
				buff[offset++] = '\\';
			}
		}
		buff[offset++] = ch;
	}
	if (do_box) {
		buff[offset++] = '\\';
		buff[offset++] = ']';
	}
	buff[offset++] = '"';
	buff[offset] = '\0';
	return(buff);
}


/* Like create_a_string, except no string escapes and such */

char *
create_a_name(int minleng, int leng)
{
	char ch;
	static char buff[BUFSIZ];
	int offset;

	if (leng > sizeof(buff) - 1) {
		leng = sizeof(buff) - 1;
	}
	leng = myrandom(minleng, leng);
	offset = 0;
	for (  ; leng > 0; leng--) {
		ch = myrandom((int)' ', (int)'~');
		if (ch == '\\' || ch == '"') {
			buff[offset++] = '\\';
		}
		buff[offset++] = ch;
	}
	buff[offset] = '\0';
	return(buff);
}


/* print a time value */

void
prtime(g_p)

struct GRP *g_p;
{
	int d;


	if (EQ(Two, g_p->basictime)) {
		out("1/2");
	}
	else if (EQ(Four, g_p->basictime)) {
		out("1/4");
	}
	else if (EQ(Eight, g_p->basictime)) {
		out("1/8");
	}
	else {
		outfmt("%d", g_p->basictime.d);
	}
	/* print dots if any */
	for (d = g_p->dots; d > 0; d--) {
		out(".");
	}
}


/* print usage message and exit */

void
usage(char *pname)
{
#ifdef GENONLY
	fprintf(stderr, "usage: %s [-i iterations] [-p prefix]\n", pname);
#else
	fprintf(stderr, "usage: %s [-d] [-i iterations] [-n] [-p prefix] [-t timeout] [-v]\n", pname);
#endif
	exit(1);
}


/* run a test. Generate a test file, run the command on it. If it core dumps
 * or exhibits other nasty behavior, save the file, otherwise throw it away */

void
run_test(int index, char *command, char *suffix, char *prefix, int timeout, int verbose)
{
	char filename[100];	/* name of generated input file */
	char cmd[BUFSIZ];	/* the command to execute */
	int ret;		/* return code */
	int exitval;		/* exit value of execution */


	sprintf(filename, "%s%d", prefix, index);

	if ((outf = fopen(filename, "w+")) == (FILE *) 0) {
		fprintf(stderr, "can't open %s\n", filename);
		return;
	}

	initvals();
	gen();
	fclose(outf);

#ifndef GENONLY
	if (sometimes(xoption)) {
		int start, end;
		start = myrandom(-2, 2);
		end = myrandom(-2, 2);
		sprintf(cmd, "%s -x%d,%d %s %s", command, start, end,
							filename, suffix);
	}
	else {
		sprintf(cmd, "%s %s %s", command, filename, suffix);
	}

	ret = exectest(cmd, timeout, verbose, 0, &exitval);
	num_non_midi_tests++;

	if (WIFEXITED(exitval) && strcmp(command, "mup") == 0) {
		int ret2;
		/* run mup with MIDI option */
		if (sometimes(xoption)) {
			int start, end;
			start = myrandom(-2, 2);
			end = myrandom(-2, 2);
			sprintf(cmd, "mup -x%d,%d -m /dev/null %s", start, end, filename);
		}
		else {
			sprintf(cmd, "mup -m /dev/null %s", filename);
		}
		ret2 = exectest(cmd, timeout, verbose, 1, &exitval);
		num_midi_tests++;

		if (ret == YES && ret2 == YES) {
			unlink(filename);
		}
	}
#endif
}


#ifndef GENONLY

/* execute test. Return YES if should remove file, NO if not */

int 
exectest(char *cmd, int timeout, int verbose, int doing_midi, int *ret_p)
{
	int ret;		/* wait() exit code of child */
	int code;		/* exit code of child */
	long now;
	char *timestamp;
	struct stat statinfo;
	int wpid;		/* PID returned by wait() */


	if (verbose) {
		time(&now);
		timestamp = ctime(&now);
		timestamp[19] = '\0';
		fprintf(stderr, "\n========= %s: %s ===========\n",
							timestamp + 4, cmd);
	}

	unlink("core");
	signal(SIGALRM, alarmhandler);
	alarm(timeout);
	switch(child = fork()) {
	case 0:
		execlp("sh", "sh", "-c", cmd, (char *) 0);
		/*FALLTHRU*/
	case -1:
		fprintf(stderr, "failed to execute command\n");
		exit(1);
	default:
		/* wait for child to die */
		do {
			wpid = wait(&ret);
		} while (wpid != child);
		alarm(0);
		*ret_p = ret;

		/* we don't remove the file if
		 * 1) if produced a core dump
		 * 2) was killed
		 * 3) exited non-zero or output contained things that
		 *    look suspiciously like floating float overflows,
		 *    and user asked to save failures.
		 */
		if (WCOREDUMP(ret) || stat("core", &statinfo) == 0) {
			if (verbose) {
				fprintf(stderr, "\tcore dumped!\n");
			}
			num_core_dumped++;
			return(NO);
		}
		if (WIFEXITED(ret)) {
			code = WEXITSTATUS(ret);
			if (verbose) {
				fprintf(stderr, "\texit code was %d\n", code);
			}
			if (code != 0) {
				num_exited_nonzero++;
				if (doing_midi) {
					num_midi_nonzero++;
				}
			}
			else {
				/* Check for floating point errors in output */
				if ( ! doing_midi) {
					int f;
					struct stat info;
					char * addr;
					int n;

					if ((f = open("RegGen2.out", O_RDONLY)) >= 0) {
						fstat(f, &info);
						addr = (char *) mmap(0, info.st_size, PROT_READ,
							MAP_SHARED, f, 0);
						close(f);
						n = info.st_size;
						if (addr != 0) {
							for (n = 0; n < info.st_size - 3; n++) {
								if (addr[n] == 'n' &&
										strncmp(addr+n, "nan ", 4) == 0) {
									num_nans++;
									code = 1;
									break;
								}
								else if (isdigit(addr[n])) {
									if (strspn(addr+n, "0123456789") > 15) {
										num_huge_nums++;
										code = 1;
										break;
									}
								}
							}
							munmap(addr, info.st_size);
						}
						else {
							fprintf(stderr, "mmap failed: %s\n", strerror(errno));
						}
						if (n < info.st_size - 3) {
							/* treat as fail */
							*ret_p = 1;
							return(NO);
						}
					}
				}
			}

			if (savenonzero == YES && code != 0) {
				return(NO);
			}
		}
		else {
			if (verbose) {
				fprintf(stderr, "\twas killed\n");
			}
			num_killed++;
			return(NO);
		}
		return(YES);
		break;
	}
}


/* if alarm goes off, assume child is in an infinite loop, and put it
 * out of its misery */

void
alarmhandler(int sig)
{
	kill(child, SIGTERM);
}

#endif


/* generate lyrics */

void
genlyrics(grplist_p, staff, staffs)

struct GRP *grplist_p;
int staff;
int staffs;	/* how many total staffs */

{
	struct GRP *g_p;
	int syls;


	/* do stuff before colon */
	out("lyrics ");
	if (sometimes(LYRABOVE)) {
		out("above ");
		outfmt("%d", staff);
	}
	else if (sometimes(LYRBETWEEN) && staff < staffs) {
		out("between");
		outfmt("%d", staff);
		out("&");
		outfmt("%d", staff + 1);
	}
	else if (sometimes(LYRBELOW)) {
		out("below");
		outfmt("%d", staff);
	}
	else {
		outfmt("%d", staff);
	}
	out(":");

	/* print time values, some with space */
	for (syls = 0, g_p = grplist_p; g_p != (struct GRP *) 0; g_p = g_p->next) {
		prtime(g_p);
		if (sometimes(LYRSPACE)) {
			out("s");
		}
		else {
			syls++;
		}
		out(";");
	}

	/* print verse number */
	if (sometimes(use1)) {
		out("[1]");
	}

	/* generate lyric syllables */
	outfmt("\"");
	for (   ; syls > 0; syls--) {
		gensyl(syls > 1 ? YES : NO);
	}
	outfmt("\"");
	out(";");
	newline();
}


/* generate lyric syllables, some with dashes or underscores */

void
gensyl(mid)

int mid;	/* YES if in middle of string (not end) */
{
	int leng;

	if (sometimes(sylposition)) {
		if (sometimes(50)) {
			outfmt("%d", myrandom(0,16)-8);
		}
		outfmt("|");
	}

	for (leng = myrandom(1, 6); leng > 0; leng--) {
		putc(myrandom((int)'a', (int) 'z'), outf);
	}

	if (sometimes(LYRDASH)) {
		outfmt("-");
	}
	else if (sometimes(UNDERSCORE)) {
		outfmt("_");
	}
	else if (mid) {
		outfmt(" ");
	}
}


/* take all the lines starting from foffset and rearrange them. To rearrange
 * whole lines, repchar will be a newline. Otherwise, you can scramble part of
 * a line by putting each piece on a separate line, then using ' ' as the
 * repchar to put everything onto one line */

void
scramble_lines(long foffset, char repchar)

{
	long end;
	long leng, c;
	char *p;
	char *buff;	/* copy of text to be scrambled */
	char **lines_p;	/* malloc-ed array of pointers to lines in buff */
	int lines = 0;	/* how many lines in buff */
	int n;		/* index into lines_p */
	int index;	/* index into lines_p */
	int i;
	int lastwasnl;	/* YES if prev char was a newline */


	/* get a buffer and read in the part of the file to be scrambled */
	end = ftell(outf);
	fseek(outf, foffset, SEEK_SET);
	leng = end - foffset;
	if (leng <= 0) {
		fseek(outf, end, SEEK_SET);
		return;
	}
	if ((buff = (char *)malloc(leng)) == (char *) 0) {
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	/* read into buffer, counting the number of lines */
	for (p = buff; p < buff + leng; p++) {
		*p = getc(outf);
		if (*p == '\n') {
			lines++;
		}
	}

	/* sometimes the last newline may have white space after it, which
	 * effectively makes an additional line (albeit an unterminated one) */
	if ( *(p-1) != '\n') {
		lines++;
	}

	if (lines < 1) {
		/* nothing to scramble */
		free(buff);
		return;
	}

	/* now malloc array of pointers, one for each line */
	if ((lines_p = (char **) malloc(lines * sizeof(char *))) == (char **) 0) {
		fprintf(stderr, " malloc failed\n");
		exit(1);
	}
	/* go through buff again, saving a pointer to the beginning of each line */
	lastwasnl = YES;
	for (n = c = 0; c < leng; c++) {
		if (lastwasnl == YES) {
			lines_p[n++] = buff + c;
		}
		lastwasnl = (buff[c] == '\n' ? YES : NO);
	}

	if (n != lines) {
		fprintf(stderr, "bug: n != lines %d %d\n", n, lines);
		exit(2);
	}

	/* now go into loop. pick a random line and output it. collapse
	 * the array to not include that line. repeat till done */
	fseek(outf, foffset, SEEK_SET);
	for (c = lines; c > 0; c--) {
		index = myrandom(0, c - 1);

		/* output the line chosen */
		for (p = lines_p[index]; p < buff + leng; p++) {
			if (*p == '\n') {
				putc(repchar, outf);
				break;
			}
			else {
				putc(*p, outf);
			}
		}
		
		/* move others down */
		for (i = index; i < c - 1; i++) {
			lines_p[i] = lines_p[i+1];
		}
	}

	if (ftell(outf) != end) {
		fprintf(stderr, "bug: rewritten length doesn't match original\n");
		exit(2);
	}

	free(buff);
	free(lines_p);
}


/* do < or > statements */

void
cresc(staff, staffs, meas, measures, num)

int staff;
int staffs;	/* how many staffs */
int meas;	/* current measure number */
int measures;	/* total number of measures */
int num;	/* numerator of time signature */

{
	if (sometimes(50)) {
		out("<");
	}
	else {
		out(">");
	}
	place(staff, staffs, YES, NO);
	stuff(num, NO, YES, measures - meas);
	newline();
}


/* choose a place: above, below, between */

void
place(staff, staffs, between_ok, req)

int staff;
int staffs;
int between_ok;	/* YES if between is legal */
int req;	/* YES if required */

{
	if (sometimes(STUFFABOVE)) {
		out("above ");
		outfmt("%d", staff);
	}
	else if (between_ok == YES && sometimes(STUFFBETWEEN)
						&& staff < staffs) {
		out("between");
		outfmt("%d", staff);
		out("&");
		outfmt("%d", staff + 1);
	}
	else if (sometimes(STUFFBELOW) || req == YES) {
		out("below");
		outfmt("%d", staff);
	}
	else {
		outfmt("%d", staff);
	}
	if (sometimes(dist)) {
		outfmt(" dist %d.%d ", myrandom(-2, 2), myrandom(0, 999) );
	}
	if (sometimes(aligntag)) {
		outfmt(" align %d", myrandom(0,10000));
	}
	out(":");
}


/* generate a "til" clause */

void
gen_til(count100, num, measrem)

int count100;		/* where the STUFF started times 100 */
int num;		/* numerator of time sig */
int measrem;		/* measures left in the song */
{
	out("til");
	/* do some with only one part or the other, some with both */
	if (sometimes(TILSAMEMEAS) || measrem == 0) {
		count100 = myrandom(count100, (num + 1) * 100);
		outfmt("%d.%02d", count100 / 100, count100 % 100);
	}
	else if (sometimes(TILMEASONLY)) {
		outfmt("%d", myrandom(1, measrem));
		out("m");
	}
	else {
		outfmt("%d", myrandom(1, measrem));
		out("m");
		out("+");
		count100 = myrandom(0, (num + 1) * 100);
		outfmt("%d.%02d", count100 / 100, count100 % 100);
	}
}


/* generate a generic stuff item */

void
stuff(num, str, til, measrem)

int num;	/* numerator of time signature */
int str;	/* if YES, generate a string */
int til;	/* if YES generate a til clause */
int measrem;	/* measures remaining */

{
	int count;


	/* generate starting place. Get a random number 100 times too big,
	 * and make a float number with 2 decimal digits from that */
	count = myrandom(0, (num + 1) * 100);
	outfmt("%d.%d", count / 100, count % 100);

	if (str == YES) {
		genstring(myrandom(0, MAXSTR));
	}
	else if (str == OCTSTR) {
		out("\"8va\"");
	}
	else if (str == GRIDCHORD) {
		out(picklist(Gridlist, Numgrids, -1));
	}

	if (til == YES) {
		gen_til(count, num, measrem);
	}
	out(";");
}


/* generate rom, bold, etc stuffs */

void
textstuff(staff, staffs, meas, measures, num)

int staff;
int staffs;	/* how many staffs */
int meas;	/* current measure number */
int measures;	/* total number of measures */
int num;	/* numerator of time signature */

{
	int use_chord_grid = NO;

	if (sometimes(FAMILY)) {
		out(picklist(Familylist,
				sizeof(Familylist) / sizeof(struct LIST), - 1));
	}
	out(" ");
	out(picklist(Fontlist, sizeof(Fontlist) / sizeof(struct LIST), -1));
	if (sometimes(FNTSIZE)) {
		out("(");
		outfmt("%d", myrandom(MINSIZE, maxsize));
		out(")");
	}
	else {
		out(" ");
	}

	if (sometimes(modifier)) {
		switch(myrandom(1, 5)) {
		case 1:
		case 4:
		case 5:
			out("chord ");
			if (Numgrids > 0) {
				use_chord_grid = YES;
			}
			break;
		case 2:
			out("analysis ");
			break;
		case 3:
			out("figbass ");
			break;
		}
	}

	place(staff, staffs, YES, NO);
	if (use_chord_grid) {
		stuff(num, GRIDCHORD, NO, measures - meas);
	}
	else {
		stuff(num, YES, sometimes(TILTEXT), measures - meas);
	}
	newline();
}


/* generate phrase marks */

void
phrasestuff(staff, staffs, meas, measures, num)

int staff;
int staffs;	/* how many staffs */
int meas;	/* current measure number */
int measures;	/* total number of measures */
int num;	/* numerator of time signature */

{
	if (sometimes(phrasemod)) {
		if (sometimes(50)) {
			out("dotted ");
		}
		else {
			out("dashed ");
		}
	}
	out("phrase ");
	place(staff, staffs, NO, NO);
	stuff(num, NO, YES, measures - meas);
	newline();
}


/* generate octave marks */

void
octstuff(staff, staffs, meas, measures, num)

int staff;
int staffs;	/* how many staffs */
int meas;	/* current measure number */
int measures;	/* total number of measures */
int num;	/* numerator of time signature */

{
	out("octave ");
	place(staff, staffs, NO, YES);
	stuff(num, OCTSTR, sometimes(50), measures - meas);
	newline();
}


/* generate pedal commands */

void
pedalstuff(staff, num)

int staff;
int num;

{
	int n;
	int when;


	out("pedal ");
	if (sometimes(50)) {
		out("below");
	}
	outfmt("%d:", staff);

	when = 0;
	for (n = 0; n < pedpermeas; n++) {
		/* generate number 100 times bigger, then format
		 * as number with 2 decimal digits. */
		when += myrandom(10, num * 100);
		if (when > (num + 1) * 100) {
			break;
		}
		outfmt("%d.%02d", when / 100, when % 100);

		/* sometimes end pedal if appropriate */
		if (sometimes(endped) && Pedstate[staff] == 1) {
			out("*");
			Pedstate[staff] = 0;
		}
		else {
			Pedstate[staff] = 1;
		}
		out(";");
	}
	newline();
}


/* sometimes put things between bars */

void
betweenbars()

{
	int did_something = NO;


	if (sometimes(HDFT)) {
		headfoot();
		did_something = YES;
	}

	if (sometimes(block)) {
		int i;

		out("block ");
		newline();
		for (i = 0; i < myrandom(0, 4); i++) {
			genprint();
		}
		did_something = YES;
	}

	/* TODO: could sometimes do score/staff/voice things here */

	/* make sure we get back to music context */
	if (did_something == YES) {
		out("music");
		newline();
	}
}


/* generate header/footer */

void
headfoot()
{
	char *hdfttype;
	int i;


	/* generate a header/footer if we can find one that hasn't been
	 * generated before */
	if ((hdfttype = picklist(Headfootlist,
			sizeof(Headfootlist) / sizeof(struct LIST),
			1)) != (char *) 0) {

		/* output header, footer, header2, or footer2 */
		out(hdfttype);
		outfmt(" %s", picklist(Pagesidelist,
			sizeof(Pagesidelist) / sizeof(struct LIST), -1));
		newline();

		/* generate items for header/footer */
		for (i = MINHDFTITEMS; i < MAXHDFTITEMS; i++) {
			if (sometimes(HDFTFONT)) {
				out("font=");
				out(picklist(Fontlist, sizeof(Fontlist)
						/ sizeof(struct LIST), -1));
				newline();
			}
			else if (sometimes(HDFTSIZE)) {
				out("size=");
				outfmt("%d", myrandom(MINSIZE, maxsize));
			}

			/*TODO: could also sometimesdo print/left/right/center here */

			else {
				out("title ");
				if (sometimes(FAMILY)) {
					out(picklist(Familylist,
						sizeof(Familylist)
						/ sizeof(struct LIST), - 1));
					out(" ");
				}
				if (sometimes(FNT)) {
					out(picklist(Fontlist, sizeof(Fontlist)
						/ sizeof(struct LIST), -1));
					out(" ");
				}
				if (sometimes(FNTSIZE)) {
					out("(");
					outfmt("%d", myrandom(MINSIZE, maxsize));
					out(")");
				}

				/* generate one to three strings */
				genstring(MAXTITLE);
				if (sometimes(TITLE2)) {
					genstring(MAXTITLE);
				}
				else if (sometimes(TITLE3)) {
					genstring(MAXTITLE);
					genstring(MAXTITLE);
				}
			}

			newline();
		}
	}
}


/* for a given test, pick random values for the parameters that govern what
 * is generated */

void
initvals()
{
	int n;

	use1 = myrandom(0,100);
	measrest = myrandom(0, 75);
	measspace = myrandom(0, 45);
	measrpt = myrandom(0, 5);
	mingridheight = myrandom(0, 10);
	minalignscale = myrandom(0, 10);
	brack = myrandom(0, 10);
	whitebefore = myrandom(0, 100);
	whiteafter = myrandom(0, 100);
	extranl = myrandom(0, 25);
	swingunit = myrandom(0, 20);
	stemlen = myrandom(0, 15);
	stemshorten = myrandom(0, 45);
	staffmult = myrandom(0, 15);
	crnl = myrandom(0, 40);
	cr_only = myrandom(0, 10);
	midmeas = myrandom(0, 10);
	noteheads = myrandom(0, 10);
	typo = myrandom(0, 20);
	scorefeed = myrandom(0, 30);
	pagefeed = myrandom(0, 20);
	marginoverride = myrandom(0, 20);
	sylposition = myrandom(0, 30);
	staffpad = myrandom(0, 30);
	scorepad = myrandom(0, 30);
	rehstyle = myrandom(0, 50);
	restart = myrandom(0, 10);
	dotted = myrandom(5,25);
	dbldotted = myrandom(0, 20);
	timedflt = myrandom(0, 100);
	wrongtime = myrandom(0, 3);
	rest = myrandom(0, 65);
	space = myrandom(0, 35);
	uncompressible = myrandom(5, 50);
	subdivide = myrandom(2, 50);
	accidental = myrandom(0, 60);
	rehlet = myrandom(0, 20);
	rehnum = myrandom(0, 20);
	rehmnum = myrandom(0, 20);
	rehstr = myrandom(0, 20);
	multirest = myrandom(0, 15);
	restsymmult = myrandom(0, 15);
	printedtime = myrandom(0, 10);
	additive_time = myrandom(0, 8);
	cross_staff_stems = myrandom(0, 2);
	maxnotes = myrandom(1, 11);
	phrase = myrandom(0, 10);
	octavesbeyond = myrandom(0, 2);
	alt = myrandom(0,5);
	numalt = myrandom(1, 3);
	custombeam = myrandom(0, 25);
	esbm = myrandom(0, 15);
	autobeaming = myrandom(0, 2);
	ph_eph = myrandom(2, 15);
	slope = myrandom(0, 8);
	compoundts = myrandom(0, 5);
	comments = myrandom(0, 10);
	vcombine = myrandom(0, 10);
	pagesize = myrandom(0, 10);
	pedal = myrandom(0, 15);
	pedpermeas = myrandom(1, 5);
	endped = myrandom(0, 30);
	margins = myrandom(0, 25);
	dist = myrandom(0, 15);
	aligntag = myrandom(0,10);
	division = myrandom(0, 10);
	endingstyle = myrandom(0, 20);
	label = myrandom(0, 25);
	measnum = myrandom(0,40);
	maxmeasures = myrandom(0, 15);
	maxscores = myrandom(0, 15);
	slashesbetween = myrandom(0, 25);
	bracketrepeats = myrandom(0, 20);
	repeatdots = myrandom(0, 15);
	withfont = myrandom(0, 35);
	emptymeas = myrandom(0, 20);
	pack = myrandom(0, 40);
	transpose = myrandom(0, 20);
	chordtranslation = myrandom(0, 10);
	useaccs = myrandom(0, 8);
	carryaccs = myrandom(0, 7);
	alignped = myrandom(0, 20);
	alignlabels = myrandom(0, 10);
	extendlyrics = myrandom(0, 60);
	tag = 0;	/*** tags don't really work yet **/
	abstag = myrandom(0,20);
	prints = myrandom(0, 5);
	dd_bar = myrandom(0, 5);
	string_escapes = myrandom(0, 3);
	modifier = myrandom(0,35);
	box = myrandom(0,10);
	staffscale = myrandom(0, 7);
	multi_brack = myrandom(0, 10);
	gtc = myrandom(0, 7);
	unsetparam = myrandom(0, 35);
	viswhereused = myrandom(0, 30);
	paragraph = myrandom(0, 30);
	block = myrandom(0, 15);
	restcombine = myrandom(0, 50);
	firstpage = myrandom(0, 10);
	fretx = myrandom(0, 20);
	freto = myrandom(0, 20);
	fretdash = myrandom(0, 20);
	gridfret = myrandom(0, 50);
	gridscale = myrandom(0, 40);
	gridparen = myrandom(0, 50);
	grids = myrandom(15, 60);
	orderlist = myrandom(10, 50);
	phrasemod = myrandom(0, 25);
	xoption = myrandom(5, 35);
	maxsize = myrandom(15, MAXSIZE);
	print = myrandom(1, 8);
	line = myrandom(1, 8);
	curve = myrandom(1, 4);
	bulgecurve = myrandom(1, 4);
	keymaps = myrandom(1, 6);
	tuning = myrandom(2, 8);
	midi = myrandom(2, 25);
	midi_to = myrandom(2, 10);
	control = myrandom(5, 20);
	/* The next two only get used if control is used */
	saveparms = myrandom(50, 70);
	restoreparms = myrandom(40, 60);
	barstyle = myrandom(10, 25);
	subbarstyle = myrandom(5, 15);
	midlinestemfloat = myrandom(2, 10);
	beamslope = myrandom(3, 12);
	tupletslope = myrandom(3, 12);
	numbermultrpt = myrandom(3, 12);
	defaultphraseside = myrandom(5, 15);
	samescore = myrandom(2, 7);
	samepage = myrandom(2, 7);
	Samescore_inprog = NO;
	Samepage_inprog = NO;

	for (n = 1; n <= MAXSTAFFS; n++) {
		Pedstate[n] = 0;
	}
}


/* create a tag. */

char *
create_tag()
{
	int i;
	int tag;		/* index into Tagtable */
	int len;		/* strlen of tag */


	/* if tag table is full, reuse an existing tag */
	if (Numtags >= MAXTAGS) {
		tag = myrandom(0, MAXTAGS - 1);
	}

	else if (sometimes(ONELETTAG)) {
		Tagtable[Numtags][0] = myrandom('a', 'z');
		Tagtable[Numtags][1] = '\0';
		tag = Numtags++;
	}
	else {
		Tagtable[Numtags][0] = '_';
		len = myrandom(2, MAXTAGLEN);
		for (i = 1; i < len; i++) {
			Tagtable[Numtags][i] = myrandom('a' - 1, 'z');
			if (Tagtable[Numtags][i] == 'a' - 1) {
				Tagtable[Numtags][i] = '_';
			}
		}
		Tagtable[Numtags][i] = '\0';
		tag = Numtags++;
	}

	return(Tagtable[tag]);
}


/* generate print/left/right/center statements */

void
genprint()
{
	char *xtag, *ytag;


	if (sometimes(paragraph)) {
		if (sometimes(30)) {
			out("ragged ");
		}
		else if (sometimes(50)) {
			out("justified ");
		}
		out ("paragraph ");
		if (sometimes(30)) {
			out(picklist(Familylist, sizeof(Familylist)
						/ sizeof(struct LIST), -1));
			out(" ");
		}
		if (sometimes(30)) {
			out(picklist(Fontlist, sizeof(Fontlist)
						/ sizeof(struct LIST), -1));
		}
		if (sometimes(30)) {
			outfmt("(%d)", myrandom(3,20));
		}
		genstring(MAXPARASTR);
		newline();
		return;
	}

	out(picklist(Printcmdslist,
			sizeof(Printcmdslist) / sizeof(struct LIST), -1));
	xtag = gettag();
	ytag = gettag();

	out ("(");
	if (xtag == (char *) 0 || sometimes(abstag)) {
		outfmt("%d", myrandom(0, 200));
	}
	else {
		out(xtag);
		out(".");
		switch(myrandom(0,2)) {
		case 0:
			out("e");
			break;
		case 1:
			out("w");
			break;
		default:
			out("x");
			break;
		}
	}
	out(",");
			
	if (ytag == (char *) 0 || sometimes(abstag)) {
		outfmt("%d", myrandom(0, 250));
	}
	else {
		out(ytag);
		out(".");
		switch(myrandom(0,2)) {
		case 0:
			out("n");
			break;
		case 1:
			out("s");
			break;
		default:
			out("y");
			break;
		}
	}
	out(")");
	genstring(MAXSTR);
	newline();
}


/* Return a random entry from the list of tags */

char *
gettag()
{
	return(Tagtable[myrandom(0, Numtags - 1)]);
}


#if ! defined(unix) && ! defined(__APPLE__)
#define NEED_GETOPT
#endif

#ifdef NEED_GETOPT
/* for non-unix or other systems that don't have a getopt() function,
 * define one here. This is NOT a general purpose implementation of getopt(),
 * but something good enough to work with Mup */

int Optch = '-';
int optind = 1;
char *optarg;
static int argoffset;

#define NOARG 1
#define WITHARG	2
#define BADOPT  3

int
getopt(argc, argv, optstring)

int argc;
char **argv;
char *optstring;

{
	int option;


	if (optind >= argc) {
		return(EOF);
	}

	if (argoffset == 0) {
#ifdef __DOS__
		if (argv[optind][argoffset] == '-'
					|| argv[optind][argoffset] == '/') {
#else
		if (argv[optind][argoffset] == '-') {
#endif
			argoffset = 1;
		}
		else {
			return(EOF);
		}
	}

	/* determine if option is valid and if should have an argument */
	option = argv[optind][argoffset] & 0x7f;
	switch (opttype(option, optstring)) {
	case NOARG:
		/* valid option without argument. Keep track of where
		 * to look for next option */
		if (argv[optind][++argoffset] == '\0') {
			optind++;
			argoffset = 0;
		}
		break;

	case WITHARG:
		/* valid option with argument. */
		if (argv[optind][++argoffset] != '\0') {
			/* argument immediately follows in same argv */
			optarg = &(argv[optind][argoffset]);
			optind++;
		}
		else {
			/* white space. argument must be in next argv */
			optind++;
			if (optind >= argc) {
				fprintf(stderr, "missing argument to %c%c option\n", Optch, option);
				return('?');
			}
			optarg = &(argv[optind][0]);
			optind++;
		}
		argoffset = 0;
		break;

	default:
		fprintf(stderr, "invalid option %c%c\n", Optch, option);
		option = '?';
	}
	return(option);
}


/* look up option in optstring and return type of option */

int
opttype(option, optstring)

int option;
char *optstring;

{
	char *p;

	for (p = optstring; *p != '\0'; ) {
		if (*p++ == option) {
			return(*p == ':' ? WITHARG : NOARG);
		}
		if (*p == ':') {
			p++;
		}
	}
	return(BADOPT);
}

#endif



/* return a list of tab strings to use and output a corresponding stafflines,
 * and return the number of strings via the passed-in pointer */

struct STRINGDATA *
tabstafflines(int *nstrings_p)
{
	struct STRINGDATA *stringdata_p;
	int n;
	int t;


	/* decide how many strings to use */
	*nstrings_p = rand() % MAXSTRINGS + 1;

	if ((stringdata_p = (struct STRINGDATA *) malloc (sizeof(struct STRINGDATA) * *nstrings_p)) == (struct STRINGDATA *) 0) {
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	out("stafflines=tab(");
	for (n = 0; n < *nstrings_p; n++) {
		stringdata_p[n].pitch = myrandom((int) 'a', (int) 'g');
		outfmt("%c", stringdata_p[n].pitch);
		switch (myrandom(0, 2)) {
		case 0:
			stringdata_p[n].accidental = ' ';
			break;
		case 1:
			stringdata_p[n].accidental = '#';
			break;
		case 2:
			stringdata_p[n].accidental = '&';
			break;
		}

		if (stringdata_p[n].accidental != ' ') {
			outfmt("%c", stringdata_p[n].accidental);
		}

		stringdata_p[n].nticks = myrandom(0, 3);
		for (t = stringdata_p[n].nticks; t > 0; t--) {
			out("'");
		}

		if ((stringdata_p[n].octave = myrandom(0, 15)) < 9) {
			outfmt("%d", stringdata_p[n].octave);
		}

		/* note: sometimes may get duplicate strings. That's okay--
		 * it tests error conditions. */
	}

	out(")");
	newline();
	return(stringdata_p);
}


/* pick notes for tablature staff */

void
picktabnotes(int nstrings, struct STRINGDATA *stringdata_p)
{
	int n;
	int u;	/* how many strings actually used */

	for (u = n = 0; n < nstrings; n++) {
		if ( ! sometimes(TABSTRING)) {
			continue;
		}
		do_a_tab_note(stringdata_p, n);
		u++;
	}

	if (u == 0) {
		/* need to have at least one string used,
		 * so pick a random one */
		do_a_tab_note(stringdata_p, myrandom(0, nstrings - 1));
	}
}

/* do a single tab note, using string n */

void
do_a_tab_note(struct STRINGDATA *stringdata_p, int n)
{
	int t;

	outfmt("%c", stringdata_p[n].pitch);
	if (stringdata_p[n].accidental != ' ') {
		outfmt("%c", stringdata_p[n].accidental);
	}
	for (t = 0; t < stringdata_p[n].nticks; t++) {
		out("'");
	}

	/* pick a random fret */
	outfmt("%d", myrandom(0, 99));

	/* TODO: should test bends and slides */
}

/* generate a grid */

void
gen_grid(int gridlist_index)

{
	int s, i;
	int paren1, paren2;	/* strings on which to put parentheses */
	char *chordname;


	/* generate a random name for the grid */
	chordname = create_a_string(1, 12);
	outfmt("%s", chordname);

	/* save a copy of the name for later use */
	if ((Gridlist[gridlist_index].str
				= (char *) malloc(strlen(chordname)+1)) == 0) {
		fprintf(stderr, "failed to malloc for grid name\n");
		exit(1);
	}
	strcpy(Gridlist[gridlist_index].str, chordname);
	Gridlist[gridlist_index].used = 0;

	out("\"");
	/* generate for a random number of strings */
	s = myrandom(1, 9);
	if (sometimes(gridparen) && s > 1) {
		paren1 = myrandom(0, s-2);
		paren2 = myrandom(paren1+1, s-1);
	}
	else {
		paren1 = paren2 = -1;
	}

	for (i = 0; i < s; i++) {
		if (i == paren1) {
			out("(");
		}
		if (sometimes(fretx)) {
			out("x ");
		}
		else if (sometimes(freto)) {
			out("o ");
		}
		else if (sometimes(fretdash)) {
			out("- ");
		}
		else {
			outfmt("%d ", myrandom(1, 99));
		}

		if (i == paren2) {
			out(")");
		}
	}
	out("\"");
	newline();
}

/* Set the default octave based on the clef, to match what Mup does */

void
adjust_defoct(char * clefname, int staff)
{
	if( !strcmp(clefname, "bass")
				|| !strcmp(clefname, "treble8")
				|| !strcmp(clefname, "tenor")
				|| !strcmp(clefname, "baritone")) {
		Staffinfo[staff].defoct = 3;
	}
	if ( !strcmp(clefname, "frenchviolin")
				|| !strcmp(clefname, "8treble")) {
		Staffinfo[staff].defoct = 5;
	}
	if ( !strcmp(clefname, "bass8")
				|| !strcmp(clefname, "subbass")) {
		Staffinfo[staff].defoct = 2;
	}
}


/* The following bunch of stuff is to support generating random
 * coordinate expressions */

const char *Func1list[] = {
	"sqrt",
	"sin",
	"cos",
	"tan",
	"asin",	
	"acos",
	"atan"
};
const int Numfunc1 = sizeof(Func1list) / sizeof(Func1list[0]);

const char *Func2list[] = {
	"atan2",
	"hypot"
};
const int Numfunc2 = sizeof(Func2list) / sizeof(Func2list[0]);

typedef enum {
	E_LITERAL,
	E_TIME,
	E_TAGREF,
	E_FUNC1,
	E_FUNC2,
	MAXETYPES
} ETYPE;

const char Operators[] = "+-*/%";

const char Directions[] = "nsewxy";


/* Output a random number, often a floating point one */

void
outrandfloat()
{
	if (sometimes(50)) {
		/* a floating point number */
		outfmt("%d.%05d", myrandom(0, myrandom(1,100)),
				myrandom(0,10000000));
	}
	else {
		/* a whole number */
		outfmt("%d", myrandom(0, myrandom(1,100)));
	}
}

/* Choose and return a random expression type */

ETYPE
pick_etype()
{
	int e;

	e = myrandom(0, 100);

	if (e < 40) {
		return(E_LITERAL);
	}
	else if (e < 70) {
		return(E_TAGREF);
	}
	else if (e < 85) {
		return(E_TIME);
	}
	else if (e < 93) {
		return(E_FUNC1);
	}
	else {
		return(E_FUNC2);
	}
}

/* Generate a random expression */

void
gen_expr(int max_elem)
{
	int elements;

	/* generate a random number of elements for the expression */
	for (elements = myrandom(1, max_elem); elements > 0; elements--) {

		switch (pick_etype()) {
		case E_LITERAL:
			outrandfloat();
			break;
		case E_TIME:
			out("time ");
			outrandfloat();
			break;
		case E_TAGREF:
			out(Tagtable[myrandom(0, Numtags - 1)]);
			outfmt(".%c", Directions[myrandom(0, strlen(Directions) - 1)]);
			break;
		case E_FUNC1:
			outfmt("%s(", Func1list[myrandom(0, Numfunc1 - 1)]);
			gen_expr(2);
			out(")");
			break;
		case E_FUNC2:
			outfmt("%s(", Func2list[myrandom(0, Numfunc2 - 1)]);
			gen_expr(2);
			out(",");
			gen_expr(2);
			out(")");
			break;
		default:
			fprintf(stderr, "bad expression type\n");
			exit(1);
		}

		if (elements > 1) {
			outfmt("%c", Operators[myrandom(0, strlen(Operators) - 1)]);
		}
	}
}


/* Generate a (x, y), where x and y are expressions  */
void
coord()
{
	out("(");
	gen_expr(5);
	out(",");
	gen_expr(5);
	out(")");
}


/* Generate things that include coordinates */

void
gen_coord_things()
{
	if (sometimes(print)) {
		out("print");	
		coord();
		genstring(10);
		newline();
	}
	if (sometimes(line)) {
		out("line");
		coord();
		out("to");
		coord();
		newline();
	}
	if (sometimes(curve)) {
		int points;

		out("curve");
		for (points = myrandom(3, 5); points > 0; points--) {
			coord();
			if (points > 1) {
				out("to");
			}
		}
		newline();
	}
	if (sometimes(bulgecurve)) {
		out("curve");
		coord();
		out("to");
		coord();
		outfmt("bulge %d", myrandom(-4, 4));
		newline();
	}
}

/* Generate a keymap context */

void
gen_keymaps()
{

	int entries;
	char *name;
	int e;
	int n;

	Numkeymaps = myrandom(0, MAX_KEYMAPS);
	for (n = 0; n < Numkeymaps; n++) {
		strcpy(Keymapnames[n], create_a_name(1, MAX_KEYMAP_NAME_LEN));
		outfmt("keymap %s", Keymapnames[n]);
		newline();
		entries = myrandom(0, 20);
		for (e = 0; e < entries; e++) {
			out(create_a_string(1, 3));
			out(create_a_string(0, 4));
			newline();
		}
	}
}

/* Generate a "to" clause for a stuff */

void
gen_to(minval, maxval, start, ts_num, measrem, multi)

int minval;
int maxval;
int start;
int ts_num;	/* time sig numerator */
int measrem;
int multi;	/* YES for velocity */

{
	int items;
	int extra;

	if (sometimes(midi_to)) {
		for (items = myrandom(1, 4); items > 0; items--) {
			outfmt("%d", myrandom(minval, maxval));
			if (multi == YES) {
				for (extra = myrandom(0,3); extra > 0; extra--) {
					outfmt(",%d", myrandom(minval, maxval));
				}
			}
			out(items > 1 ? "to" : "\"");
		}
		gen_til(start * 100, ts_num, measrem);
		out(";");
	}
	else {
		outfmt("%d\";", myrandom(minval, maxval));
	}
}

/* Generate MIDI stuff */

void
gen_midi(staffs, ts_num, measrem)

int staffs;
int ts_num;
int measrem;

{
	int start;
	int n;
	char * mtype;

	for (n = myrandom(1, 3); n > 0; n--) {

		if (sometimes(20)) {
			out ("midi all:");
		}
		else {
			outfmt("midi %d:", myrandom(1, staffs)); 
		}

		start = myrandom(0, ts_num);
		mtype = picklist(Midi_list, sizeof(Midi_list)
						/ sizeof(struct LIST), -1);
		outfmt("%d \"%s=", start, mtype);
		if (strcmp(mtype, "channel") == 0 || 
					strcmp(mtype, "chanpressure") == 0 || 
					strcmp(mtype, "port") == 0 || 
					strcmp(mtype, "program") == 0) {
			gen_to(0, 127, start, ts_num, measrem, NO);
		}
		else if (strcmp(mtype, "tempo") == 0) {
			gen_to(10, 1000, start, ts_num, measrem, NO);
		}
		else if (strcmp(mtype, "parameter") == 0) {
			outfmt("%d,", myrandom(0,127));
			gen_to(0, 127, start, ts_num, measrem, NO);
		}
		else if (strcmp(mtype, "onvelocity") == 0
				|| strcmp(mtype, "offvelocity") == 0) {
			gen_to(0, 127, start, ts_num, measrem, YES);
		}
		else if (strcmp(mtype, "seqnum") == 0) {
			outfmt("%d\";", myrandom(0,65535));
		}
		else if (strcmp(mtype, "hex") == 0) {
			/*** TODO ***/
		}
		else {
			outfmt("%s", create_a_name(1,10));
			out("\";");
		}
		newline();
	}
}

void
gen_barstyle()

{
	out("barstyle=");
	out(picklist(Barstylelist, sizeof(Barstylelist) / sizeof(struct LIST), -1));
	newline();
}


void
gen_subbarstyle()

{
	out("subbarstyle=");
	switch(myrandom(0,2)) {
	case 0: out("dashed "); break;
	case 1: out("dotted "); break;
	case 2: break;
	}
	switch(myrandom(0, 1)) {
	case 0: out("bar "); break;
	case 1: out("dblbar "); break;
	}
	switch(myrandom(0, 2)) {
	case 0: out("between "); break;
	case 1: out("(top to bottom)"); break;
	case 2: out("(middle-1 to middle+1)"); break;
	}
	switch(myrandom(0, 2)) {
	case 0: out(" all "); break;
	case 1: out(" 1-2 " ); break;
	case 2: out(" 1 "); break;
	}
	out("time");
	switch(myrandom(0, 1)) {
	case 0: out("1.5"); break;
	case 1: out("2"); break;
	}
	newline();
}
