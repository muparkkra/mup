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
/*
 * Name:	ssv.c
 *
 * Description:	This file defines the instances of score, staff, and voice
 *		structures that are used for viewpathing.  It also contains
 *		functions for accessing them.
 */

#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "rational.h"
#include "structs.h"
#include "globals.h"

/*
 * Define copies of the official fixed SSV structures that are in defines.c.
 * These copies are used to hold a snapshot of the official ones.
 * Note that there is only one copy of these.  It is used by code that is
 * looping through the main linked list, so that it can save (savessvstate),
 * loop some more, then restore (restoressvstate), to revert to the earlier
 * state.  This is not related to the user's saveparms and restoreparms.
 */
static struct SSV Saved_score;
static struct SSV Saved_staff[MAXSTAFFS];
static struct SSV Saved_voice[MAXSTAFFS][MAXVOICES];

/*
 * This flag is YES when the contents of the above SSVs are the same as the
 * official fixed SSVs.  At the start, they are all zeros, and therefore equal.
 */
static int Ssvs_equal = YES;

/* define the default order of stacking for marks */
static char Defmarkorder[] = {
	1,	/* MK_MUSSYM	*/
	2,	/* MK_OCTAVE	*/
	3,	/* MK_DYN	*/
	3,	/* MK_OTHERTEXT	*/
	3,	/* MK_CHORD	*/
	4,	/* MK_LYRICS	*/
	5,	/* MK_ENDING	*/
	6,	/* MK_REHEARSAL	*/
	7,	/* MK_PEDAL	*/
};

/* default timesig of 4/4 */
static char Dflt_timerep[] = { 4, TSR_SLASH, 4, TSR_END };


static struct SSV *svpath_parm P((int s, int field));

static void setorder P((int place, struct SSV *i_p, struct SSV *f_p));

/*
 * Name:        initstructs()
 *
 * Abstract:    Init the fixed instances of the SSV structures.
 *
 * Returns:     void
 *
 * Description: This function initializes all the fixed structures Score,
 *		Staff[s], and Voice[s][v].  This needs to be done before
 *		scanning through the main linked list of user input
 *		structures.
 */

void
initstructs()

{
	int s;		/* staff number */
	int v;		/* voice number */
	int n;		/* another loop variable */
	int p;		/* place (PL_*) */
	int m;		/* mark (MK_*) */
	int hs;		/* head shape number */


	Ssvs_equal = NO;	/* this will make the backup SSVs out of date */

	/*
	 * Call zapssv() to destroy current staff and voice info on all
	 * existing staffs.  It will mark all fields unused.  Note that the
	 * first time in, all this global data is zero, including Score.staffs,
	 * so the loop does nothing.  But that's okay, since the "used" fields
	 * will also already be zero (NO).
	 */
	for (s = 0; s < Score.staffs; s++) {
		zapssv(&Staff[s]);
		for (v = 0; v < MAXVOICES; v++) {
			zapssv(&Voice[s][v]);
		}
	}

	zapssv(&Score);		/* now zap the score itself */

	/*
	 * Fill the Score structure with the proper default values, and
	 * mark all its fields as used.
	 */
	/* score context */
	Score.scale_factor = DEFSCALE;
	Score.units = INCHES;
	Score.pageheight = DEFPAGEHEIGHT;
	Score.pagewidth = DEFPAGEWIDTH;
	Score.panelsperpage = DEFPANELSPERPAGE;
	Score.slashesbetween = NO;
	Score.maxscores = DEFMAXSCORES;
	Score.maxmeasures = DEFMAXMEASURES;
	Score.topmargin = DEFVMARGIN;
	Score.botmargin = DEFVMARGIN;
	Score.leftmargin = DEFHMARGIN;
	Score.rightmargin = DEFHMARGIN;
	Score.flipmargins = NO;
	Score.indentrestart = NO;
	Score.restcombine = NORESTCOMBINE;
	Score.firstpage = NOFIRSTPAGE;
	Score.firstside = PGSIDE_NOT_SET;
	Score.staffs = DEFSTAFFS;
	Score.minscsep = DEFMINSCSEP;
	Score.maxscsep = DEFMAXSCSEP;
	Score.minscpad = DEFMINSCPAD;
	Score.maxscpad = DEFMAXSCPAD;
	Score.nbrace = 0;
	Score.nbrack = 0;
	Score.nbarst = 0;
	Score.timerep = Dflt_timerep;
	Score.timenum = 4;
	Score.timeden = 4;
	Score.time.n = 1;
	Score.time.d = 1;
	Score.timevis = PTS_ONCE;
	Score.nsubbar = 0;
	Score.subbarlist = 0;
	Score.division = DEFDIVISION;
	Score.endingstyle = ENDING_TOP;
	Score.alignlabels = J_RIGHT;
	Score.gridsatend = NO;
	Score.measnum = MN_NONE;
	Score.measnumfamily = BASE_TIMES;
	Score.measnumfont = FONT_TR;
	Score.measnumsize = MNUM_SIZE;
	Score.measnumstyle = RS_PLAIN;
	Score.bracketrepeats = NO;
	Score.packfact = DFLTPACKFACT;
	Score.packexp = DFLTPACKEXP;
	Score.leftspacefact = DEFLEFTFACT;
	Score.leftspacemax = DEFLEFTMAX;
	Score.warn = YES;
	Score.printkeymap = 0;
	Score.a4freq = DEFA4FREQ;
	Score.tuning = TU_EQUAL;
	Score.acctable = 0;

	/* score and staff context */
	Score.staffscale = DEFSTFSCALE;
	Score.hidesilent = NO;
	Score.stafflines = 5;
	Score.strinfo = 0;
	Score.printclef = SS_NORMAL;
	Score.printtabclef = PTC_FIRST;
	Score.gridswhereused = NO;
	Score.gridscale = DEFGRIDSCALE;
	Score.gridfret = DEFGRIDFRET;
	Score.mingridheight = DEFGRIDFRET;
	Score.numbermrpt = YES;
	Score.numbermultrpt = YES;
	Score.printmultnum = YES;
	Score.restsymmult = NO;
	Score.vscheme = V_1;
	for (v = 0; v < MAXVOICES; v++) {
		Score.vcombine[v] = 0;
	}
	Score.vcombinequal = VC_NOOVERLAP;
	Score.vcombinemeas = NO;
	Score.prtime_str1 = 0;
	Score.prtime_str2 = 0;
	Score.prtime_is_arbitrary = NO;
	Score.sharps = DEFSHARPS;
	Score.is_minor = NO;
	Score.cancelkey = NO;
	Score.inttype = PERFECT;
	Score.intnum = 1;
	Score.addinttype = PERFECT;
	Score.addintnum = 1;
	Score.useaccs = UA_N;
	Score.carryaccs = YES;
	Score.clef = TREBLE;
	Score.rehstyle = RS_BOXED;
	Score.repeatdots = RD_STANDARD;
	Score.fontfamily = BASE_TIMES;
	Score.font = FONT_TR;
	Score.size = DFLT_SIZE;
	Score.lyricsfamily = BASE_TIMES;
	Score.lyricsfont = FONT_TR;
	Score.lyricssize = DFLT_SIZE;
	Score.lyricsalign = DEFLYRICSALIGN;
	Score.sylposition = DEFSYLPOSITION;
	Score.minalignscale = DEFMINALIGNSCALE;
	Score.minstsep = DEFMINSTSEP;
	Score.staffpad = DEFSTPAD;
	for (p = 0; p < NUM_PLACE; p++) {
		for (m = 0; m < NUM_MARK; m++) {
			Score.markorder[p][m] = Defmarkorder[m];
		}
	}
	Score.pedstyle = P_LINE;
	Score.alignped = YES;
	Score.chorddist = DEFCHORDDIST;
	Score.dist = DEFDIST;
	Score.dyndist = DEFDYNDIST;
	Score.lyricsdist = DEFLYRICSDIST;
	Score.chordtranslation = CT_NONE;
	Score.doremi_syls = NULL;
	Score.label = 0;
	Score.label2 = 0;
	Score.labelkeymap = 0;
	Score.endingkeymap = 0;
	Score.rehearsalkeymap = 0;
	Score.defaultkeymap = get_keymap("");	/* never allow null pointer */
	Score.withkeymap = 0;
	Score.textkeymap = 0;
	Score.lyricskeymap = 0;

	/* score, staff, and voice context */
	Score.visible = YES;
	Score.nbeam = 0;
	Score.nsubbeam = 0;
	Score.beamfact = DEFBEAMFACT;
	Score.beammax = DEFBEAMMAX;
	Score.tupletfact = DEFTUPLETFACT;
	Score.tupletmax = DEFTUPLETMAX;
	Score.pad = DEFPAD;
	Score.stemlen = DEFSTEMLEN;
	Score.beamshort = DEFBEAMSHORT;
	Score.maxproshort = DEFMAXPROSHORT;
	Score.begproshort = DEFBEGPROSHORT;
	Score.endproshort = DEFENDPROSHORT;
	Score.midlinestemfloat = NO;
	Score.defoct = DEFOCTAVE;
	Score.noteinputdir = UNKNOWN;
	Score.timeunit.n = 1;
	Score.timeunit.d = 4;
	Score.timelist_p = 0;
	Score.swingunit = Zero;
	Score.withfamily = BASE_TIMES;
	Score.withfont = FONT_TR;
	Score.withsize = DFLT_SIZE;
	Score.alignrests = NO;
	Score.release = DEFRELEASE;
	Score.ontheline = YES;
	Score.tabwhitebox = NO;
	hs = get_shape_num("norm");
	for (n = 0; n < 7; n++) {
		Score.noteheads[n] = hs;
	}
	Score.emptymeas = 0;
	Score.extendlyrics = NO;
	Score.cue = NO;
	Score.defaultphraseside = PL_UNKNOWN;

	for (n = 0; n < NUMFLDS; n++) {
		Score.used[n] = YES;	/* all items will be set in Score */
	}
}

/*
 * Name:        zapssv()
 *
 * Abstract:    Init a fixed instance of the SSV structure to empty.
 *
 * Returns:     void
 *
 * Description: This function initializes a fixed SSV structure to say that
 *		all fields are unused.
 */

void
zapssv(s_p)

struct SSV *s_p;		/* pointer to the structure to be zapped */

{
	int n;		/* loop variable */


	/*
	 * Mark all fields unused.
	 */
	for (n = 0; n < NUMFLDS; n++) {
		s_p->used[n] = NO;
	}
}

/*
 * Name:        svpath()
 *
 * Abstract:    Find a field for a staff, using the viewpath.
 *
 * Returns:     pointer to structure containing correct field value
 *
 * Description: This function, given a staff number and a field number, looks
 *		down the viewpath to find the first structure where the field
 *		is set.  It returns a pointer to that structure.  (However, it
 *		it does special work for *keymap parameters and "visible".)
 *		Note:  0 is allowed for the staff number, and that means use
 *		the Score value.
 */

struct SSV *
svpath(s, field)

int s;		/* staff number, 1 to MAXSTAFFS; or 0, meaning score */
int field;	/* the defined symbol for the field desired */

{
	struct SSV *ssv_p;
	struct KEYMAP *keymap_p;


	/* first do the normal work for this parameter */
	ssv_p = svpath_parm(s, field);

	/* the non-default keymap parameters must check the value they got */
	switch (field) {
	case ENDINGKEYMAP:
		keymap_p = ssv_p->endingkeymap;
		break;
	case LABELKEYMAP:
		keymap_p = ssv_p->labelkeymap;
		break;
	case LYRICSKEYMAP:
		keymap_p = ssv_p->lyricskeymap;
		break;
	case PRINTKEYMAP:
		keymap_p = ssv_p->printkeymap;
		break;
	case REHEARSALKEYMAP:
		keymap_p = ssv_p->rehearsalkeymap;
		break;
	case TEXTKEYMAP:
		keymap_p = ssv_p->textkeymap;
		break;
	case WITHKEYMAP:
		keymap_p = ssv_p->withkeymap;
		break;
	default:
		/* DEFAULTKEYMAP and all other parameters return here */
		return (ssv_p);
	}

	/*
	 * If this non-default keymap parameter is null, viewpath on the
	 * default keymap parameter, and return that result instead.
	 */
	if (keymap_p == 0) {
		return (svpath_parm(s, DEFAULTKEYMAP));
	}

	/* otherwise return the keymap we got for the original parameter */
	return (ssv_p);
}

/*
 * Name:        svpath_parm()
 *
 * Abstract:    Do the real svpath work for a single parameter.
 *
 * Returns:     pointer to structure containing correct field value
 *
 * Description: This function, given a staff number and a field number, looks
 *		down the viewpath to find the first structure where the field
 *		is set.  It returns a pointer to that structure.  (However, see
 * 		below for a special kluge for the "visible" field.)
 */

static struct SSV *
svpath_parm(s, field)

int s;		/* staff number, 1 to MAXSTAFFS; or 0, meaning score */
int field;	/* the defined symbol for the field desired */

{
	static struct SSV phony;	/* phony SSV; see below */


	if (s == 0)
		return (&Score);

	/*
	 * For the "visible" field, special kluges are needed, for two reasons.
	 * One is that there is a command line option (-s) that overrides the
	 * visibility requests in the Mup input file.  The other is that, even
	 * though it is a score/staff/voice parameter, it is easier to manage
	 * visibility at the staff level than at the voice level.  Otherwise,
	 * to know if a staff should be drawn, we would have to check vscheme
	 * and the visibility state of each voice.
	 *
	 * The design is as follows:  in mkchords.c, if a voice is to be
	 * invisible, a measure space is put in place of its original GRPSYL
	 * list.  Users can use svpath() to check if a staff is visible
	 * instead of having to call vvpath() for its voice(s).  The field
	 * staff_p->visible is set by calling here, so that will also be
	 * consistent.
	 *
	 * If the -s option was used on the command line, only staffs/voices
	 * listed there are ever allowed to be visible.  So although SSVs are
	 * set as for other score/staff/voice parameters, the results from
	 * svpath() also consider what the -s option said.  In vvpath() this
	 * is also done.
	 */
	if (field == VISIBLE) {
		int num_voices;		/* how many voices on this staff */

		/*
		 * In case we are going to return the phony SSV, load the
		 * hidesilent field into it.  Both visible and hidesilent are
		 * controlled by VISIBLE, but hidesilent is not to be affected
		 * by the kluges needed for "visible".
		 */
		/* if the field is set in the staff structure, use that */
		if (Staff[s-1].used[VISIBLE] == YES) {
			phony.hidesilent = Staff[s-1].hidesilent;
		} else {
			/* use the score structure; it's always set there */
			phony.hidesilent = Score.hidesilent;
		}

		/* always return NO if the command line says staff invisible */
		if (Staff_vis[s] == NO) {
			/* return phony SSV with NO, ignore real SSV */
			phony.visible = NO;
			return (&phony);
		}

		num_voices = vscheme_voices(svpath(s, VSCHEME)->vscheme);

		/*
		 * If a voice that the command line is allowing to be visible
		 * was requested via an SSV to be visible, we must let the
		 * staff be visible.
		 */
		if ((Voice_vis[s][1] == YES &&
		     Voice[s-1][0].used[VISIBLE] == YES &&
		     Voice[s-1][0].visible == YES) ||

		    (num_voices >= 2 &&
		     Voice_vis[s][2] == YES &&
		     Voice[s-1][1].used[VISIBLE] == YES &&
		     Voice[s-1][1].visible == YES) ||

		    (num_voices >= 3 &&
		     Voice_vis[s][3] == YES &&
		     Voice[s-1][2].used[VISIBLE] == YES &&
		     Voice[s-1][2].visible == YES)) {

			/* return phony SSV with YES, ignore real SSV */
			phony.visible = YES;
			return (&phony);
		}

		/*
		 * If, for each voice that exists, either the command line is
		 * forcing it to be invisible or else it was requested via an
		 * SSV to be invisible, then the staff must be invisible.
		 */
		if ((Voice_vis[s][1] == NO ||
		    (Voice[s-1][0].used[VISIBLE] == YES &&
		     Voice[s-1][0].visible == NO)) &&

		    (num_voices < 2 ||
		     (Voice_vis[s][2] == NO ||
		     (Voice[s-1][1].used[VISIBLE] == YES &&
		      Voice[s-1][1].visible == NO))) &&

		    (num_voices < 3 ||
		     (Voice_vis[s][3] == NO ||
		     (Voice[s-1][2].used[VISIBLE] == YES &&
		      Voice[s-1][2].visible == NO)))) {

			/* return phony SSV with NO, ignore real SSV */
			phony.visible = NO;
			return (&phony);
		}

		/*
		 * The command line and the voice(s) aren't forcing the issue.
		 * So fall through to determine the staff's visibility the
		 * normal way.
		 */
	}

	/* if the field is set in the staff structure, use that */
	if (Staff[s-1].used[field] == YES)
		return (&Staff[s-1]);

	/* else use the score structure; it's always set there */
	return (&Score);
}

/*
 * Name:        vvpath()
 *
 * Abstract:    Find a field for a voice, using the viewpath.
 *
 * Returns:     pointer to structure containing correct field value
 *
 * Description: This function, given a staff number, voice number on that
 *		staff, and a field number, looks down the viewpath to find
 *		the first structure where the field is set.  It returns a
 *		pointer to that structure.  (However, see below for a special
 *		kluge for the "visible" field.)
 *		Note:  0 is allowed for the voice number, and that means use
 *		the staff's value.  If staff is 0, the Score is used,
 *		regardless of the voice number.
 */

struct SSV *
vvpath(s, v, field)

int s;		/* staff number, 1 to MAXSTAFFS; or 0, meaning score */
int v;		/* voice number, 1 to MAXVOICES; or 0, meaning staff */
int field;	/* the defined symbol for the field desired */

{
	static struct SSV phony;	/* phony SSV; see below */


	if (s == 0 || v == 0)
		return (svpath(s, field));

	/*
	 * See the comment in svpath() regarding the "visible" field.  There's
	 * probably no need to call vvpath() for "visible" after mkchords.c has
	 * run, since voices that are to be invisible are changed to measure
	 * spaces there.  But in mkchords.c itself, and earlier, there is
	 * sometimes a need.
	 *
	 * For the "visible" field, first check the command line to see if this
	 * voice or its staff must always be invisible.  If so, return a phony
	 * SSV that says that.  Otherwise fall through to handle the normal way.
	 */
	if (field == VISIBLE && (Staff_vis[s] == NO || Voice_vis[s][v] == NO)) {
		/*
		 * Since we are going to force visible to NO, it's irrelevant
		 * what hidesilent is, so don't bother setting it, unlike what
		 * we had to do in svpath().
		 */
		phony.visible = NO;
		return (&phony);
	}

	/* if the field is set in the voice structure, use that */
	if (Voice[s-1][v-1].used[field] == YES)
		return (&Voice[s-1][v-1]);

	/* else if the field is set in the staff structure, use that */
	if (Staff[s-1].used[field] == YES)
		return (&Staff[s-1]);

	/* else use the score structure; it's always set there */
	return (&Score);
}

/*
 * Name:        asgnssv()
 *
 * Abstract:    Assign fields from an input SSV to a fixed one.
 *
 * Returns:     void
 *
 * Description: This function is passed an input SSV structure (from an input
 *		context).  For each field where "used" is YES in the input SSV,
 *		it copies it to the appropriate fixed SSV and sets its "used"
 *		flag to YES.  (For the *keymap parameters, there is an exception
 *		to this.)  For each field where "used" is UNSET in the input
 *		SSV, it sets "used" to NO in the appropriate fixed SSV.
 *		In some cases, there are side effects, where it also
 *		alters a lower level structure.  E.g., changing the number of
 *		voices of a staff inits its voice structures.  In the case
 *		of stafflines, setting a staff to be a tablature staff
 *		automatically automatically forces some other fields to be set
 *		not only in the given staff, but also in the preceding tabnote
 *		staff.  Note also that the Score "used" flags are already
 *		always set and don't need to be set here.  And Score fields can
 *		never be unset.
 */

void
asgnssv(i_p)

struct SSV *i_p;	/* input SSV structure to be copied from */

{
	struct SSV *f_p;	/* ptr to fixed SSV structure to copy into */
	int s, v;		/* used for looping through staffs & voices */
	int start, stop;	/* loop limits */
	int n;			/* another loop variable */


	f_p = 0;	/* to prevent "uninitialized variable" warnings */

	/*
	 * Using the selector fields in the input structure, set a pointer to
	 * the fixed structure that is to be populated.
	 */
	switch (i_p->context) {
	case C_SCORE:
		f_p = &Score;
		break;
	case C_STAFF:
		/* silently ignore bogus staff no.; it is caught elsewhere */
		if (i_p->staffno < 1 || i_p->staffno > MAXSTAFFS) {
			return;
		}
		f_p = &Staff[ i_p->staffno - 1 ];
		break;
	case C_VOICE:
		/* silently ignore bogus staff/voice; it is caught elsewhere */
		if (i_p->staffno < 1 || i_p->staffno > MAXSTAFFS ||
		    i_p->voiceno < 1 || i_p->voiceno > MAXVOICES) {
			return;
		}
		f_p = &Voice[ i_p->staffno - 1 ][ i_p->voiceno - 1 ];
		break;
	default:
		pfatal("invalid context %d\n", i_p->context);
	}

	Ssvs_equal = NO;	/* this will make the backup SSVs out of date */

	/*
	 * ========== ITEMS FOR SCORE CONTEXT ONLY ===========
	 * There's no need to set f_p->used[] = YES here; since this is the
	 * score, those bits are already always YES.
	 */
	if (i_p->used[SCALE_FACTOR] == YES) {
		f_p->scale_factor = i_p->scale_factor;
	}

	if (i_p->used[UNITS] == YES) {
		f_p->units = i_p->units;
	}

	/*
	 * PAGEHEIGHT, PAGEHEIGHT, and PANELSPERPAGE interact, because when the
	 * user sets PAGE*, they are referring to the paper, but internally we
	 * want page* to refer to one "panel" of music, which is a 90 degree
	 * rotated half of the sheet of paper when panelsperpage is 2.
	 */
	if (i_p->used[PAGEHEIGHT] == YES) {
		if (f_p->panelsperpage == 1) {
			f_p->pageheight = i_p->pageheight;
		} else {
			f_p->pagewidth = i_p->pageheight / 2.0;
		}
	}

	if (i_p->used[PAGEWIDTH] == YES) {
		if (f_p->panelsperpage == 1) {
			f_p->pagewidth = i_p->pagewidth;
		} else {
			f_p->pageheight = i_p->pagewidth;
		}
	}

	if (i_p->used[PANELSPERPAGE] == YES) {
		/* depending on how this is changing, flip height and width */
		float oldwidth;
		if (f_p->panelsperpage == 1 && i_p->panelsperpage == 2) {
			oldwidth = f_p->pagewidth;
			f_p->pagewidth = f_p->pageheight / 2.0;
			f_p->pageheight = oldwidth;
		} else if (f_p->panelsperpage == 2 && i_p->panelsperpage == 1) {
			oldwidth = f_p->pagewidth;
			f_p->pagewidth = f_p->pageheight;
			f_p->pageheight = oldwidth * 2.0;
		}
		f_p->panelsperpage = i_p->panelsperpage;
	}

	if (i_p->used[SLASHESBETWEEN] == YES) {
		f_p->slashesbetween = i_p->slashesbetween;
	}

	if (i_p->used[MAXSCORES] == YES) {
		f_p->maxscores = i_p->maxscores;
	}

	if (i_p->used[MAXMEASURES] == YES) {
		f_p->maxmeasures = i_p->maxmeasures;
	}

	if (i_p->used[TOPMARGIN] == YES) {
		f_p->topmargin = i_p->topmargin;
	}

	if (i_p->used[BOTMARGIN] == YES) {
		f_p->botmargin = i_p->botmargin;
	}

	if (i_p->used[LEFTMARGIN] == YES) {
		f_p->leftmargin = i_p->leftmargin;
	}

	if (i_p->used[RIGHTMARGIN] == YES) {
		f_p->rightmargin = i_p->rightmargin;
	}

	if (i_p->used[FLIPMARGINS] == YES) {
		f_p->flipmargins = i_p->flipmargins;
	}

	if (i_p->used[INDENTRESTART] == YES) {
		f_p->indentrestart = i_p->indentrestart;
	}

	if (i_p->used[RESTCOMBINE] == YES) {
		f_p->restcombine = i_p->restcombine;
	}

	if (i_p->used[FIRSTPAGE] == YES) {
		f_p->firstpage = i_p->firstpage;
		f_p->firstside = i_p->firstside;
	}

	if (i_p->used[NUMSTAFF] == YES) {
		/* destroy current staff & voice info on all existing staffs */
		for (s = 0; s < f_p->staffs; s++) {
			zapssv(&Staff[s]);
			for (v = 0; v < MAXVOICES; v++) {
				zapssv(&Voice[s][v]);
			}
		}

		f_p->staffs = i_p->staffs;
	}

	if (i_p->used[MINSCSEP] == YES) {
		f_p->minscsep = i_p->minscsep;
	}

	if (i_p->used[MAXSCSEP] == YES) {
		f_p->maxscsep = i_p->maxscsep;
	}

	if (i_p->used[MINSCPAD] == YES) {
		f_p->minscpad = i_p->minscpad;
	}

	if (i_p->used[MAXSCPAD] == YES) {
		f_p->maxscpad = i_p->maxscpad;
	}

	if (i_p->used[BRACELIST] == YES) {
		f_p->nbrace = i_p->nbrace;
		f_p->bracelist = i_p->bracelist;	/* just copy pointer */
	}

	if (i_p->used[BRACKLIST] == YES) {
		f_p->nbrack = i_p->nbrack;
		f_p->bracklist = i_p->bracklist;	/* just copy pointer */
	}

	if (i_p->used[BARSTLIST] == YES) {
		f_p->nbarst = i_p->nbarst;
		f_p->barstlist = i_p->barstlist;	/* just copy pointer */
	}

	if (i_p->used[TIME] == YES) {
		f_p->timenum  = i_p->timenum;
		f_p->timeden  = i_p->timeden;
		f_p->timevis  = i_p->timevis;
		f_p->timerep  = i_p->timerep;
		f_p->time     = i_p->time;

		/* changing time sig removes any subbar style */
		f_p->nsubbar = 0;
		f_p->subbarlist = 0;	/* null pointer */

		/*
		 * Changing the time sig forces a change in default time unit.
		 * Set it to one "beat" for the score, and unset it for all
		 * staffs and voices for staffs that exist.
		 */
		f_p->timeunit.n = 1;
		f_p->timeunit.d = f_p->timeden;
		f_p->timelist_p = 0;
		for (s = 0; s < Score.staffs; s++) {
			Staff[s].used[TIMEUNIT] = NO;
			for (v = 0; v < MAXVOICES; v++)
				Voice[s][v].used[TIMEUNIT] = NO;
		}

		/*
		 * Changing the time also destroys all beamstyle lists.
		 * However, the special empty beamstyle list that was set up
		 * for a tablature staff must be retained, so that it will
		 * continue to override any score beamstyle list that may be
		 * set up later on.
		 */
		if (Score.used[BEAMSTLIST] == YES) {
			if (Score.nbeam != 0) {
				Score.beamstlist = 0;
				Score.nbeam = 0;
				Score.subbeamstlist = 0;
				Score.nsubbeam = 0;
			}
		}
		for (s = 0; s < Score.staffs; s++) {
			if (Staff[s].used[BEAMSTLIST] == YES &&
			    ! (Staff[s].used[STAFFLINES] == YES &&
			       Staff[s].strinfo != 0)) { /* not tablature */
				if (Staff[s].nbeam != 0) {
					Staff[s].beamstlist = 0;
					Staff[s].nbeam = 0;
					Staff[s].subbeamstlist = 0;
					Staff[s].nsubbeam = 0;
				}
				Staff[s].used[BEAMSTLIST] = NO;
			}
			for (v = 0; v < MAXVOICES; v++) {
				if (Voice[s][v].used[BEAMSTLIST] == YES) {
					if (Voice[s][v].nbeam != 0) {
						Voice[s][v].beamstlist = 0;
						Voice[s][v].nbeam = 0;
						Voice[s][v].subbeamstlist = 0;
						Voice[s][v].nsubbeam = 0;
					}
					Voice[s][v].used[BEAMSTLIST] = NO;
				}
			}
		}
	}

	/* has to be after TIME, since TIME blows it away */
	if (i_p->used[SUBBARSTYLE] == YES) {
		f_p->nsubbar = i_p->nsubbar;
		f_p->subbarlist = i_p->subbarlist;	/* just copy pointer */
	}

	if (i_p->used[DIVISION] == YES) {
		f_p->division = i_p->division;
	}

	if (i_p->used[ENDINGSTYLE] == YES) {
		f_p->endingstyle = i_p->endingstyle;
	}

	if (i_p->used[ALIGNLABELS] == YES) {
		f_p->alignlabels = i_p->alignlabels;
	}

	if (i_p->used[MEASNUM] == YES) {
		f_p->measnum = i_p->measnum;
	}

	if (i_p->used[MEASNUMFAMILY] == YES) {
		f_p->measnumfamily = i_p->measnumfamily;
	}

	if (i_p->used[MEASNUMFONT] == YES) {
		f_p->measnumfont = i_p->measnumfont;
	}

	if (i_p->used[MEASNUMSIZE] == YES) {
		f_p->measnumsize = i_p->measnumsize;
	}

	if (i_p->used[MEASNUMSTYLE] == YES) {
		f_p->measnumstyle = i_p->measnumstyle;
	}

	if (i_p->used[BRACKETREPEATS] == YES) {
		f_p->bracketrepeats = i_p->bracketrepeats;
	}

	if (i_p->used[PACKFACT] == YES) {
		f_p->packfact = i_p->packfact;
	}

	if (i_p->used[PACKEXP] == YES) {
		f_p->packexp = i_p->packexp;
	}

	switch (i_p->used[LEFTSPACE]) {
	case YES:
		f_p->leftspacefact = i_p->leftspacefact;
		f_p->leftspacemax = i_p->leftspacemax;
		f_p->used[LEFTSPACE] = YES;
		break;
	case UNSET:
		f_p->used[LEFTSPACE] = NO;
		break;
	}

	if (i_p->used[WARN] == YES) {
		f_p->warn = i_p->warn;
	}

	if (i_p->used[PRINTKEYMAP] == YES) {
		f_p->printkeymap = i_p->printkeymap;
	}

	if (i_p->used[A4FREQ] == YES) {
		f_p->a4freq = i_p->a4freq;
	}

	if (i_p->used[TUNING] == YES) {
		f_p->tuning = i_p->tuning;
	}

	if (i_p->used[ACCTABLE] == YES) {
		f_p->acctable = i_p->acctable;
	}

	/*
	 * ========== ITEMS FOR SCORE AND STAFF CONTEXT ===========
	 */
	/*
	 * Most parameters involve just a single field, and have no side
	 * effects.  For this, we can use the following switch statement to
	 * do the work, for parameters that can exist on staff or voice.
	 * (Score-only ones don't need it, since that can't be unset.)
	 */
#define	SETPARM(name, NAME)			\
	switch (i_p->used[NAME]) {		\
	case YES:				\
		f_p->name = i_p->name;		\
		f_p->used[NAME] = YES;		\
		break;				\
	case UNSET:				\
		f_p->used[NAME] = NO;		\
		break;				\
	/* default is NO; do nothing */		\
	}

	/*
	 * During the first part of the placement phase we need it ignore
	 * staffscale.  See the comment about Ignore_staffscale in globals.c.
	 */
	if (Ignore_staffscale == NO) {
		SETPARM(staffscale, STAFFSCALE)
	}

	switch (i_p->used[STAFFLINES]) {
	case YES: {
		struct SSV *tabnote_p;	/* ptr to tabnote fixed SSV */
		f_p->stafflines = i_p->stafflines;

		if (i_p->strinfo != 0) {	/* tablature */
			struct SSV *voice_p;	/* ptr to a voice's fixed SSV*/

			/*
			 * This is a tablature staff.  Set printclef to normal
			 * (even though tab isn't particularly "normal").
			 * Set printtabclef as requested by the user.
			 * Point f_p->strinfo at the same array that
			 * i_p->strinfo points at.
			 */
			f_p->printclef = SS_NORMAL;
			f_p->printtabclef = i_p->printtabclef;
			f_p->strinfo = i_p->strinfo;

			/*
			 * Force some other score/staff items to fixed values
			 * for tab.  The parser blocks the user from setting
			 * these.  We need to set them here in the staff SSV to
			 * override whatever may be in the score SSV.  This
			 * will make it possible to avoid special checks for
			 * tablature in many places; the right values will be
			 * set for this staff.  Also force score/staff/voice
			 * items here.
			 */
			f_p->sharps = 0;
			f_p->is_minor = NO;
			f_p->used[SHARPS] = YES;

			f_p->inttype = PERFECT;
			f_p->intnum = 1;
			f_p->trans_usage = TR_BOTH;
			f_p->used[TRANSPOSITION] = YES;

			f_p->addinttype = PERFECT;
			f_p->addintnum = 1;
			f_p->addtrans_usage = TR_BOTH;
			f_p->used[ADDTRANSPOSITION] = YES;

			f_p->clef = TABCLEF;
			f_p->forceprintclef = NO;
			f_p->used[CLEF] = YES;

			f_p->nbeam = 0;
			f_p->beamstlist = 0;
			f_p->nsubbeam = 0;
			f_p->subbeamstlist = 0;
			f_p->used[BEAMSTLIST] = YES;

			f_p->defoct = 4;
			f_p->used[DEFOCT] = YES;

			/* blow away the following in tab staff's voices */
			for (v = 0; v < MAXVOICES; v++) {
				voice_p = &Voice[ i_p->staffno - 1][ v ];

				if (voice_p->used[BEAMSTLIST] == YES) {
					voice_p->used[BEAMSTLIST] = NO;
				}
				voice_p->used[DEFOCT] = NO;
			}

			/*
			 * Force fields on the tabnote staff above this tab
			 * staff.
			 */
			tabnote_p = &Staff[ i_p->staffno - 2 ];

			/*
			 * The parse phase wouldn't let this be another tab
			 * staff, so we don't need to check for that.  But it
			 * might be a 1-line staff.  If so, override it to 5
			 * line.  If this parameter wasn't set, force printclef
			 * to normal, but if it was, keep the old value.  (We
			 * might as well allow 5n and 5 drum as well as 5,
			 * though that would be a weird usage.)
			 */
			tabnote_p->stafflines = 5;
			if (tabnote_p->used[STAFFLINES] == NO)
				tabnote_p->printclef = SS_NORMAL;
			tabnote_p->used[STAFFLINES] = YES;

		} else {	/* not tablature */
			/*
			 * If this staff used to be tablature, we need to unset
			 * some "used" fields that were forced.
			 */
			if (f_p->used[STAFFLINES] == YES && f_p->strinfo != 0) {
				f_p->used[SHARPS] = NO;
				f_p->used[TRANSPOSITION] = NO;
				f_p->used[ADDTRANSPOSITION] = NO;
				f_p->used[CLEF] = NO;
				f_p->used[BEAMSTLIST] = NO;
				f_p->used[DEFOCT] = NO;
			}

			/* make it non-tablature */
			f_p->strinfo = 0;
			f_p->printclef = i_p->printclef;
			/* not tab, so printtabclef is irrelevant */
		}
		f_p->used[STAFFLINES] = YES;
		} break;
	case UNSET:
		f_p->used[STAFFLINES] = NO;
		break;
	}

	SETPARM(gridswhereused, GRIDSWHEREUSED)

	SETPARM(gridsatend, GRIDSATEND)

	SETPARM(gridscale, GRIDSCALE)

	SETPARM(gridfret, GRIDFRET)

	SETPARM(mingridheight, MINGRIDHEIGHT)

	SETPARM(numbermrpt, NUMBERMRPT)

	SETPARM(numbermultrpt, NUMBERMULTRPT)

	SETPARM(printmultnum, PRINTMULTNUM)

	SETPARM(restsymmult, RESTSYMMULT)

	switch (i_p->used[VSCHEME]) {
	case YES:
		/*
		 * If the vscheme change changes the *number* of voices, we
		 * have to wipe out the voice information, but otherwise not.
		 */
		if (i_p->context == C_SCORE) {
			start = 0;		/* if score, do test for */
			stop = Score.staffs - 1;  /* all existing staffs */
		} else {	/* C_STAFF */
			start = stop = i_p->staffno - 1; /* do just this one */
		}

		/* for each staff affected by this change . . . */
		for (n = start; n <= stop; n++) {
			int oldvoices, newvoices; /* how many before & after */

			oldvoices = vscheme_voices(svpath(n + 1,
					VSCHEME)->vscheme);
			newvoices = vscheme_voices(i_p->vscheme);

			if (oldvoices != newvoices) {

				for (v = 0; v < MAXVOICES; v++)
					zapssv(&Voice[n][v]);
			}
		}

		f_p->vscheme = i_p->vscheme;
		f_p->used[VSCHEME] = YES;
		break;
	case UNSET:
		f_p->used[VSCHEME] = NO;
		break;
	}

	switch (i_p->used[VCOMBINE]) {
	case YES:
		for (v = 0; v < MAXVOICES; v++) {
			f_p->vcombine[v] = i_p->vcombine[v];
		}
		f_p->vcombinequal = i_p->vcombinequal;
		f_p->vcombinemeas = i_p->vcombinemeas;
		f_p->used[VCOMBINE] = YES;
		break;
	case UNSET:
		f_p->used[VCOMBINE] = NO;
		break;
	}

	switch (i_p->used[PRINTEDTIME]) {
	case YES:
		f_p->prtime_str1 = i_p->prtime_str1;
		f_p->prtime_str2 = i_p->prtime_str2;
		f_p->prtime_is_arbitrary = i_p->prtime_is_arbitrary;
		f_p->used[PRINTEDTIME] = YES;
		break;
	case UNSET:
		f_p->used[PRINTEDTIME] = NO;
		break;
	}

	switch (i_p->used[SHARPS]) {
	case YES:
		f_p->sharps = i_p->sharps;
		f_p->is_minor = i_p->is_minor;
		f_p->used[SHARPS] = YES;
		break;
	case UNSET:
		f_p->used[SHARPS] = NO;
		break;
	}

	SETPARM(cancelkey, CANCELKEY)

	switch (i_p->used[TRANSPOSITION]) {
	case YES:
		f_p->inttype = i_p->inttype;
		f_p->intnum = i_p->intnum;
		f_p->trans_usage = i_p->trans_usage;
		f_p->used[TRANSPOSITION] = YES;
		break;
	case UNSET:
		f_p->used[TRANSPOSITION] = NO;
		break;
	}

	switch (i_p->used[ADDTRANSPOSITION]) {
	case YES:
		f_p->addinttype = i_p->addinttype;
		f_p->addintnum = i_p->addintnum;
		f_p->addtrans_usage = i_p->addtrans_usage;
		f_p->used[ADDTRANSPOSITION] = YES;
		break;
	case UNSET:
		f_p->used[ADDTRANSPOSITION] = NO;
		break;
	}

	SETPARM(useaccs, USEACCS)

	SETPARM(carryaccs, CARRYACCS)

	switch (i_p->used[CLEF]) {
	case YES:
		f_p->clef = i_p->clef;
		f_p->forceprintclef = i_p->forceprintclef;
		f_p->used[CLEF] = YES;

		/*
		 * Reset the default octave so that the middle line of the
		 * staff lies within it.  If the user also set octave in
		 * this context, this will get changed again later in this
		 * function.
		 */
		f_p->defoct = (34 - f_p->clef) / 7;
		f_p->used[DEFOCT] = YES;
		break;
	case UNSET:
		f_p->used[DEFOCT] = NO;
		break;
	}

	SETPARM(rehstyle, REHSTYLE)

	SETPARM(repeatdots, REPEATDOTS)

	SETPARM(fontfamily, FONTFAMILY)

	SETPARM(font, FONT)

	SETPARM(size, SIZE)

	SETPARM(lyricsfamily, LYRICSFAMILY)

	SETPARM(lyricsfont, LYRICSFONT)

	SETPARM(lyricssize, LYRICSSIZE)

	SETPARM(lyricsalign, LYRICSALIGN)

	SETPARM(sylposition, SYLPOSITION)

	SETPARM(minalignscale, MINALIGNSCALE)

	SETPARM(minstsep, MINSTSEP)

	SETPARM(staffpad, STAFFPAD)

	switch (i_p->used[ABOVEORDER]) {
	case YES:
		setorder(PL_ABOVE, i_p, f_p);
		f_p->used[ABOVEORDER] = YES;
		break;
	case UNSET:
		f_p->used[ABOVEORDER] = NO;
		break;
	}

	switch (i_p->used[BELOWORDER]) {
	case YES:
		setorder(PL_BELOW, i_p, f_p);
		f_p->used[BELOWORDER] = YES;
		break;
	case UNSET:
		f_p->used[BELOWORDER] = NO;
		break;
	}

	switch (i_p->used[BETWEENORDER]) {
	case YES:
		setorder(PL_BETWEEN, i_p, f_p);
		f_p->used[BETWEENORDER] = YES;
		break;
	case UNSET:
		f_p->used[BETWEENORDER] = NO;
		break;
	}

	SETPARM(pedstyle, PEDSTYLE)

	SETPARM(alignped, ALIGNPED)

	SETPARM(chorddist, CHORDDIST)

	SETPARM(dist, DIST)

	SETPARM(dyndist, DYNDIST)

	SETPARM(lyricsdist, LYRICSDIST)

	switch (i_p->used[CHORDTRANSLATION]) {
	case YES:
		f_p->chordtranslation = i_p->chordtranslation;
		f_p->doremi_syls = i_p->doremi_syls;  /* needed for CT_DOREMI */
		f_p->used[CHORDTRANSLATION] = YES;
		break;
	case UNSET:				\
		f_p->used[CHORDTRANSLATION] = NO;
		break;
	}

	SETPARM(label, LABEL)

	SETPARM(label2, LABEL2)

	/*
	 * The *keymap parameters work a little differently.  When they are set
	 * to null pointer not at the score level, we want the viewpathing to
	 * continue; so we treat that like "unset".
	 */
#define	SETKEYMAPPARM(name, NAME)					\
	switch (i_p->used[NAME]) {					\
	case YES:							\
		if (i_p->context != C_SCORE && i_p->name == 0) {	\
			f_p->used[NAME] = NO;				\
		} else {						\
			f_p->name = i_p->name;				\
			f_p->used[NAME] = YES;				\
		}							\
		break;							\
	case UNSET:							\
		f_p->used[NAME] = NO;					\
		break;							\
	/* default is NO; do nothing */					\
	}

	SETKEYMAPPARM(labelkeymap, LABELKEYMAP)

	SETKEYMAPPARM(endingkeymap, ENDINGKEYMAP)

	SETKEYMAPPARM(rehearsalkeymap, REHEARSALKEYMAP)

	SETKEYMAPPARM(defaultkeymap, DEFAULTKEYMAP)

	SETKEYMAPPARM(withkeymap, WITHKEYMAP)

	SETKEYMAPPARM(textkeymap, TEXTKEYMAP)

	SETKEYMAPPARM(lyricskeymap, LYRICSKEYMAP)

	/*
	 * ========== ITEMS FOR SCORE, STAFF, AND VOICE CONTEXT ===========
	 */
	switch (i_p->used[VISIBLE]) {
	case YES:
		f_p->visible = i_p->visible;
		f_p->hidesilent = i_p->hidesilent;
		f_p->used[VISIBLE] = YES;
		break;
	case UNSET:
		f_p->used[VISIBLE] = NO;
	}

	switch (i_p->used[BEAMSTLIST]) {
	case YES:
		/* if it was already used, free old list if present */
		if (f_p->used[BEAMSTLIST] == YES && f_p->nbeam != 0) {
			f_p->beamstlist = 0;
			f_p->subbeamstlist = 0;
		}

		/* set up new list */
		f_p->nbeam = i_p->nbeam;
		f_p->beamrests = i_p->beamrests;
		f_p->beamspaces = i_p->beamspaces;
		f_p->nsubbeam = i_p->nsubbeam;
		f_p->beamstlist = i_p->beamstlist;	/* just copy pointer */
		f_p->subbeamstlist = i_p->subbeamstlist;/* just copy pointer */

		f_p->used[BEAMSTLIST] = YES;
		break;
	case UNSET:
		/* if it was already used, free old list if present */
		if (f_p->used[BEAMSTLIST] == YES && f_p->nbeam != 0) {
			f_p->beamstlist = 0;
			f_p->subbeamstlist = 0;
		}
		f_p->used[BEAMSTLIST] = NO;
		break;
	}

	switch (i_p->used[BEAMSLOPE]) {
	case YES:
		f_p->beamfact = i_p->beamfact;
		f_p->beammax = i_p->beammax;
		f_p->used[BEAMSLOPE] = YES;
		break;
	case UNSET:
		f_p->used[BEAMSLOPE] = NO;
		break;
	}

	switch (i_p->used[TUPLETSLOPE]) {
	case YES:
		f_p->tupletfact = i_p->tupletfact;
		f_p->tupletmax = i_p->tupletmax;
		f_p->used[TUPLETSLOPE] = YES;
		break;
	case UNSET:
		f_p->used[TUPLETSLOPE] = NO;
		break;
	}

	SETPARM(pad, PAD)

	SETPARM(stemlen, STEMLEN)

	SETPARM(beamshort, BEAMSHORT)

	SETPARM(maxproshort, MAXPROSHORT)

	SETPARM(begproshort, BEGPROSHORT)

	SETPARM(endproshort, ENDPROSHORT)

	SETPARM(midlinestemfloat, MIDLINESTEMFLOAT)

	SETPARM(defoct, DEFOCT)

	SETPARM(noteinputdir, NOTEINPUTDIR)

	switch (i_p->used[TIMEUNIT]) {
	case YES:
		f_p->timeunit = i_p->timeunit;
		f_p->timelist_p = i_p->timelist_p;
		f_p->used[TIMEUNIT] = YES;
		break;
	case UNSET:
		f_p->used[TIMEUNIT] = NO;
	}

	SETPARM(swingunit, SWINGUNIT)

	SETPARM(withfamily, WITHFAMILY)

	SETPARM(withfont, WITHFONT)

	SETPARM(withsize, WITHSIZE)

	SETPARM(alignrests, ALIGNRESTS)

	SETPARM(release, RELEASE)

	SETPARM(ontheline, ONTHELINE)

	SETPARM(tabwhitebox, TABWHITEBOX)

	switch (i_p->used[NOTEHEADS]) {
	case YES:
		for (n = 0; n < 7; n++) {
			f_p->noteheads[n] = i_p->noteheads[n];
		}
		f_p->used[NOTEHEADS] = YES;
		break;
	case UNSET:
		f_p->used[NOTEHEADS] = NO;
		break;
	}

	SETPARM(emptymeas, EMPTYMEAS)

	SETPARM(extendlyrics, EXTENDLYRICS)

	SETPARM(cue, CUE)

	SETPARM(defaultphraseside, DEFAULTPHRASESIDE)
}

/*
 * Name:        restoreparms()
 *
 * Abstract:    Restore the fixed SSVs from how they were when user did a save
 *
 * Returns:     address of the last SSV added
 *
 * Description: This function clones all the SSVs from the beginning of the
 *		main list to the specified place, ordinary and timed.
 */

struct MAINLL *
restoreparms(save_p, insert_p)

struct MAINLL *save_p;		/* clone till here */
struct MAINLL *insert_p;	/* insert the cloned SSVs here */

{
	struct MAINLL *mll_p;		/* for going through main list */
	struct MAINLL *new_mll_p;	/* points to current copy */
	struct MAINLL *old_next_p;	/* to detect if called function
					 * updated our next pointer */
	struct TIMEDSSV *tssv_p;	/* timed SSV being processed */
	short staff_used[MAXSTAFFS];	/* YES if there was a user SSV */
	short voice_used[MAXSTAFFS][MAXVOICES]; /* Yes if there was a user SSV */
	int s;				/* staff index */
	int v;				/* voice index */


	/* If nothing in the main list, there must not have been any (valid)
	 * input read yet, or at least any that was sufficiently free of
	 * errors to cause something to get put in the main list.
	 * That means nothing has changed from initial state,
	 * so nothing to do.
	 */
	if (Mainllhc_p == 0) {
		return(insert_p);
	}

	/* init to no staff or voice SSVs having anything set in them */
	for (s = 0; s < MAXSTAFFS; s++) {
		staff_used[s] = NO;
		for (v = 0; v < MAXVOICES; v++) {
			voice_used[s][v] = NO;
		}
	}

	/* Put SSVs as if it is the beginning of the song */
	initstructs();
	if (save_p == 0) {
		/* This means the user did a save at the very beginning
		 * of their input, and the initstructs has restored that state,
		 * so we are done. */
		return(insert_p);
	}

	/* Assign all the SSVs from the beginning of the main list
	 * to the save point, both ordinary and timed. */
	for (mll_p = Mainllhc_p; mll_p != 0; mll_p = mll_p->next) {
		if (mll_p->str == S_SSV) {
			asgnssv(mll_p->u.ssv_p);
			if (mll_p->u.ssv_p->context == C_STAFF) {
				staff_used[mll_p->u.ssv_p->staffno-1] = YES;
			}
			else if (mll_p->u.ssv_p->context == C_VOICE) {
				voice_used[mll_p->u.ssv_p->staffno-1][mll_p->u.ssv_p->voiceno-1] = YES;
			}
		}
		else if (mll_p->str == S_BAR) {
			for (tssv_p = mll_p->u.bar_p->timedssv_p; tssv_p != 0;
						tssv_p = tssv_p->next) {
				asgnssv( &(tssv_p->ssv) );
				if (tssv_p->ssv.context == C_STAFF) {
					staff_used[tssv_p->ssv.staffno-1] = YES;
				}
				else if (tssv_p->ssv.context == C_VOICE) {
					voice_used[tssv_p->ssv.staffno-1][tssv_p->ssv.voiceno-1] = YES;
				}
			}
		}

		if (mll_p == save_p) {
			/* We have cloned enough to finish the restore */
			/* Always copy the Score */
			new_mll_p = newMAINLLstruct(S_SSV, yylineno);
			memcpy(new_mll_p->u.ssv_p, &Score, sizeof(Score));
			/* The fixed SSV don't have context set,
			 * so fix the clone */
			new_mll_p->u.ssv_p->context = C_SCORE;
			insertMAINLL(new_mll_p, insert_p);
			insert_p = new_mll_p;
			old_next_p = new_mll_p->next;
			remember_tsig_params(new_mll_p);
			/* remember_tsig_params sometimes may add SSVs,
			 * so if we detect that happened, move our insert
			 * pointer to the last of those added structs. */
			while (insert_p->next != old_next_p) {
				insert_p = insert_p->next;
			}

			/* The "initial" SSV that we just created will have
			 * all the TUNEPARMSSV fields used[] set.
			 * Normally, if any of those are set,
			 * then Tuning_used will be YES,
			 * and other code depends on that.
			 * So we could just set it to YES here.
			 * But if the user hasn't really used them anywhere,
			 * it seems a shame to pull in all the tuning code
			 * unnecessarily. So we check for that case,
			 * and turn off the used fields in that case, because
			 * the current values will already be correct.
			 * THIS CODE MUST BE KEPT IN SYNC
			 * WITH THE DEFINITION OF TUNEPARMSSV.
			 */
			if (Tuning_used == NO) {
				new_mll_p->u.ssv_p->used[A4FREQ] = NO;
				new_mll_p->u.ssv_p->used[TUNING] = NO;
				new_mll_p->u.ssv_p->used[ACCTABLE] = NO;
			}

			/* Now create SSVs that are the result of all the
			 * SSVs up to this point, for any staffs / voices
			 * where the user set anything. Do in reverse order,
			 * so tabnotes don't get wiped out. */
			for (s = MAXSTAFFS - 1; s >= 0; s--) {
				if (staff_used[s] == YES) {
					new_mll_p = newMAINLLstruct(S_SSV, yylineno);
					memcpy(new_mll_p->u.ssv_p, &(Staff[s]), sizeof(struct SSV));
					new_mll_p->u.ssv_p->context = C_STAFF;
					new_mll_p->u.ssv_p->staffno = s+1;
					insertMAINLL(new_mll_p, insert_p);
					insert_p = new_mll_p;
					old_next_p = new_mll_p->next;
					remember_tsig_params(new_mll_p);
					while (insert_p->next != old_next_p) {
						insert_p = insert_p->next;
					}
				}
				for (v = 0; v < MAXVOICES; v++) {
					if (voice_used[s][v] == YES) {
						new_mll_p = newMAINLLstruct(S_SSV, yylineno);
						memcpy(new_mll_p->u.ssv_p, &(Voice[s][v]), sizeof(struct SSV));
						new_mll_p->u.ssv_p->context = C_VOICE;
						new_mll_p->u.ssv_p->staffno = s+1;
						new_mll_p->u.ssv_p->voiceno = v+1;
						insertMAINLL(new_mll_p, insert_p);
						insert_p = new_mll_p;
					}
				}
			}

			return(insert_p);
		}
	}
	pfatal("restoreparms: unable to find saved point");
	/* for lint */
	return(insert_p);
}

/*
 * Name:        setssvstate()
 *
 * Abstract:    Set the static SSVs to the state for a given place in the MML.
 *
 * Returns:     void
 *
 * Description: This function, given any structure from the main linked list,
 *		initializes the static SSVs, and then runs through the MLL up
 *		to just before that point, assigning SSVs.  It assigns not only
 *		the SSVs in the MLL, but also the timed SSVs hanging off
 *		barlines.  You can pass a null pointer, and then it will go
 *		through the whole MLL.
 */

void
setssvstate(mainll_p)

struct MAINLL *mainll_p;	/* place in the MLL to stop */

{
	struct MAINLL *mll_p;		/* for looping through MLL */
	struct TIMEDSSV *tssv_p;	/* for looping through TIMEDSSV lists*/


	initstructs();
	for (mll_p = Mainllhc_p; mll_p != 0 && mll_p != mainll_p;
			mll_p = mll_p->next) {
		switch (mll_p->str) {
		case S_SSV:
			/* assign this normal input SSV */
			asgnssv(mll_p->u.ssv_p);
			break;
		case S_BAR:
			/* assign each timed SSV, if any */
			for (tssv_p = mll_p->u.bar_p->timedssv_p; tssv_p != 0;
					tssv_p = tssv_p->next) {
				asgnssv(&tssv_p->ssv);
			}
			break;
		}
	}
}

/*
 * Name:        savessvstate()
 *
 * Abstract:    Save the current SSV states into the Saved_* copies.
 *
 * Returns:     void
 *
 * Description: This function makes backup copies of the fixed SSVs by copying
 *		them into the "Saved_*" copies.
 */

void
savessvstate()

{
	/* if the backups are already the same, there is no need to copy */
	if (Ssvs_equal == YES) {
		return;
	}

	/*
	 * For staff and voice, save only for the staffs that exist.  It
	 * doesn't matter if there is garbage in later staffs.
	 */
	Saved_score = Score;
	(void)memcpy(Saved_staff, Staff, Score.staffs * sizeof (Staff[0]));
	(void)memcpy(Saved_voice, Voice, Score.staffs * sizeof (Voice[0]));

	Ssvs_equal = YES;	/* now they are equal */
}

/*
 * Name:        restoressvstate()
 *
 * Abstract:    Restore the current SSV states from the Saved_* copies.
 *
 * Returns:     void
 *
 * Description: This function restores the fixed SSVs to what they were when
 *		the last save was done, by copying them into the "Saved_*"
 *		copies.
 */

void
restoressvstate()

{
	/* if the backups are already the same, there is no need to copy */
	if (Ssvs_equal == YES) {
		return;
	}

	/*
	 * For staff and voice, restore only for the staffs that will exist.
	 * It doesn't matter if there is garbage in later staffs.
	 */
	Score = Saved_score;
	(void)memcpy(Staff, Saved_staff, Saved_score.staffs * sizeof(Staff[0]));
	(void)memcpy(Voice, Saved_voice, Saved_score.staffs * sizeof(Voice[0]));

	Ssvs_equal = YES;	/* now they are equal */
}

/*
 * Name:        staff_field_used()
 *
 * Abstract:    Is the given field used in this staff SSV?
 *
 * Returns:     YES or NO
 *
 * Description: This function returns the given "used" field of a static staff
 *		SSV.
 */

int
staff_field_used(field, staffno)

int field;		/* field as in ssvused.h */
int staffno;		/* staff number */

{
	if (field < 0 || field >= NUMFLDS) {
		pfatal("staff_field_used: field not valid", field);
	}
	if (staffno < 1 || staffno > Score.staffs) {
		pfatal("staff_field_used: staffno %d not valid", staffno);
	}
	return (Staff[staffno - 1].used[field]);
}

/*
 * Name:        voice_field_used()
 *
 * Abstract:    Is the given field used in this voice SSV?
 *
 * Returns:     YES or NO
 *
 * Description: This function returns the given "used" field of a static voice
 *		SSV.
 */

int
voice_field_used(field, staffno, voiceno)

int field;		/* field as in ssvused.h */
int staffno;		/* staff number */
int voiceno;		/* voice number */

{
	if (field < 0 || field >= NUMFLDS) {
		pfatal("voice_field_used: field not valid", field);
	}
	if (staffno < 1 || staffno > Score.staffs) {
		pfatal("staff_field_used: staffno %d not valid", staffno);
	}
	if (voiceno < 1 ||
	voiceno > vscheme_voices(svpath(staffno, VSCHEME)->vscheme)) {
		pfatal("voice_field_used: voiceno %d not valid", voiceno);
	}
	return (Voice[staffno - 1][voiceno - 1].used[field]);
}

/*
 * Name:        setorder()
 *
 * Abstract:    Assign an "order" field from an input SSV to a fixed one.
 *
 * Returns:     void
 *
 * Description: This function is called by asgnssv() to assign to the
 *		appropriate part of the markorder array, based on above, below,
 *		or between.
 */

static void
setorder(place, i_p, f_p)

int place;		/* PL_* */
struct SSV *i_p;	/* input SSV structure to be copied from */
struct SSV *f_p;	/* ptr to fixed SSV structure to copy into */

{
	int m;		/* mark (MK_*) */
	int stk;	/* stacking order */


	/*
	 * First assign all the marks' stacking orders as given.  Keep track of
	 * the highest stacking order number found.
	 */
	stk = 0;
	for (m = 0; m < NUM_MARK; m++) {
		f_p->markorder[place][m] = i_p->markorder[place][m];

		if (f_p->markorder[place][m] > stk)
			stk = f_p->markorder[place][m];
	}

	/*
	 * For every mark type that the user didn't list, the stacking order is
	 * now 0.  Set all these to default settings, higher than all the ones
	 * the user listed, but in the same order as Defmarkorder.  They will
	 * all be separate numbers, none set equal, unlike Defmarkorder, where
	 * some are equal.
	 */
	for (m = 0; m < NUM_MARK; m++) {
		if (f_p->markorder[place][m] == 0) {
			f_p->markorder[place][m] = ++stk;
		}
	}
}
