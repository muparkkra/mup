
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

/* This file contains functions called by the Mup parse phase,
 * to deal with lyrics.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif


/* information about a measure-worth of lyrics */
struct LYRINFO {
	short	place;		/* PL_ABOVE, etc */
	short	vno;		/* verse number */
	struct	GRPSYL	*gs_p;	/* the lyrics themselves */
	struct LYRINFO	*next;	/* for linked list of other verses on the
				 * same staff */
};

/* current font/size for each staff/verse/place combination */
struct LYRFONTSIZE {
	short	staffno;
	short	verse;
	char	place;			/* PL_ABOVE, etc */
	char	font;
	char	size;
	char	all;			/* YES if "above all" or "below all".
					 * In that case, the staffno is
					 * irrelevant */
	struct LYRFONTSIZE *next;	/* linked list */
};

/* we keep the font/size info independently for each staff/verse/place combo
 * and carry it forward from measure to measure. Info is stored here */
struct LYRFONTSIZE *Lyrfontsizeinfo_p [MAXSTAFFS + 1];

/* for each staff, keep a list of all the GRPSYL lyric lists that will be
 * associated with that staff. */
static struct LYRINFO *Lyr_tbl[MAXSTAFFS + 1];

/* need to keep a list of all verse numbers ever used, for setting Maxverses */
static struct RANGELIST *Above_vnumbers_used;
static struct RANGELIST *Below_vnumbers_used;
static struct RANGELIST *Between_vnumbers_used;

/* for auto-numbering verses, keep track of the last verse number used
 * for each staff/place */
static short Prev_verse_num[MAXSTAFFS+1][NUM_PLACE];

/* We remember what staff/voice were used for time derivation,
 * to be able to give better error messages. */
static int Derive_staff;
static int Derive_voice;
#define NOT_DERIVED (-1)

/* This malloced array keeps track of where we deduced there should be
 * extenders, so we can add underscore anywhere user didn't but should have. */
static int *Need_extender;
static int Num_need_extender;

/* static function declarations */
static void free_lyrinfo P((struct LYRINFO *lyrinfo_p));
static void assoc_lyr2staff P((int staffno, int verse, struct GRPSYL *gs_p,
		char *lyrstring, int all, int place));
static struct LYRFONTSIZE *getlyrfontsize P((int staffno, int verse,
		int place, int all));
static void updlyrfontsize P((struct LYRFONTSIZE *lyrfontsize_p,
		int font, int size));
static char *next_syl P((char **sylstring_p, int *font_p, int *size_p,
			short *position_p, int staffno, float *c_p));
static void do_sylwidth P((char *lyrstring, float *wid_b4_syl_p,
		float *wid_real_syl_p, float *wid_after_syl_p,
		int compensating));
static void count_verses P((struct RANGELIST *used_p));
static void vno_used P((int vno));
static void lyr_mismatch P((char *which));
static void clear_need_extender P((void));
#ifdef __STDC__
static void lyr_error P((char *filename, int lineno, char * format, ...));
#else
static void lyr_error P((char *filename, int lineno, char * format, va_alist));
#endif


/* given a range of verses, save the range for later use */

void
lyr_verse(begin, end)

int begin;	/* first verse in range */
int end;	/* last verse number in range */

{
	/* error check */
	/* Note that -1 is a special value meaning "one more than previous."
	 * Zero is also special, meaning "centered," but that doesn't
	 * get handled by this function, since that doesn't need the error
	 * checks that are done here, since the parser guarantees good values.
 	 * If a zero does get passed in, the user must have explicitly
	 * specified it, which is a user error.
	 */
	if (begin < 1 && begin != -1) {
		lyr_error(Curr_filename, yylineno, "verse number must be >= 1");
		return;
	}

	if (end < begin) {
		lyr_error(Curr_filename, yylineno,
				"end of verse range smaller than beginning");
		return;
	}

	save_vno_range(begin, end);
}


/* once all the information about a measure-worth of lyrics has been
 * gathered, process it. Break up the lyrics string into syllables and
 * store them in the appropriate GRPSYL structs. Then clone the list as
 * many times as necessary for each staff/verse, and associate with
 * appropriate staffs.
 */

void
proc_lyrics(grpsyl_p, lyrstring)

struct GRPSYL *grpsyl_p;	/* list of GRPSYLs with time values */
char *lyrstring;		/* string containing the lyrics */

{
	struct RANGELIST *slist_p;	/* walk through list of staffs */
	struct RANGELIST *vlist_p;	/* walk through list of verses */
	int staffno;
	int verse;
	int place;


	if (grpsyl_p == (struct GRPSYL *) 0 || lyrstring == (char *) 0) {
		return;
	}

	debug(2, "proc_lyrics file=%s lineno=%d", grpsyl_p->inputfile,
			grpsyl_p->inputlineno);

	/* for each staff and verse being defined, clone the GRPSYL
	 * list and associate with appropriate staff */
	/* do each appropriate staff */
	for (slist_p = Staffrange_p; slist_p !=  (struct RANGELIST *) 0;
				slist_p = slist_p->next) {
		place = (slist_p->place == PL_UNKNOWN ? PL_BELOW : slist_p->place);
		for (staffno = slist_p->begin; staffno <= slist_p->end;
							staffno++) {

			/* do each appropriate verse */
			for (vlist_p = Vnorange_p;
					vlist_p != (struct RANGELIST *) 0;
					vlist_p = vlist_p->next) {

				for (verse = vlist_p->begin;
							verse <= vlist_p->end;
							verse++) {

					/* -1 means one more than previous */
					if (verse == -1) {
		
						verse = Prev_verse_num[staffno][place] + 1;
					}
					Prev_verse_num[staffno][place] = verse;
					/* connect to proper staff */
					assoc_lyr2staff(staffno, verse,
							grpsyl_p, lyrstring,
							slist_p->all, place);

					/* mark that we've use this verse # */
					vno_used(verse);
				}
			}
			/* The ranges are linked onto the head of Vnorange_p,
			 * so they end up in backwards order from user input.
			 * We want the "previous" verse to be the last that
			 * the user put in, so if the last thing the user put
			 * in was a a "deduce from previous," that should
			 * be the end of the range of the first struct on
			 * the Vnorrange_p list, since that is that last
			 * one the user put in. */
			if (Vnorange_p != 0 && Vnorange_p->begin != -1 ) {
				Prev_verse_num[staffno][place] = Vnorange_p->end;
			}
		}
	}

	/* free the vno rangelist.  Don't free the staff info yet, because
	 * there may be more verses */
	free_vnorange();

	/* the lyrics string has been broken into syllables. Don't need
	 * the original string anymore */
	FREE(lyrstring);
}


/* connect a list of syllable to specified STAFF */

static void
assoc_lyr2staff(staffno, verse, gs_p, lyrstring, all, place)

int staffno;		/* which staff lyrics belong to */
int verse;		/* verse number of lyrics */
struct GRPSYL *gs_p;	/* list of GRPSYLs with time information */
char *lyrstring;	/* string of lyrics syllables in this measure/verse */
int all;		/* YES if associated with "all" */
int place;		/* PL_*   */

{
	struct LYRINFO *new_p;			/* to store info about
						 * current staff/verse lyrics */
	struct LYRINFO *lyrinfo_p;		/* used when searching for
						 * where to insert this verse */
	struct LYRINFO **insert_place_p_p;	/* address of where to add
						 * lyric info when doing
						 * insertion sort */
	char *lyr_copy;				/* copy of lyrics */
	int font;
	int size;
	struct LYRFONTSIZE *lyrfontsize_p;	/* info about font/size for
						 * current staff/verse/place */
	int n;					/* Need_extender[] index */
	char *newsyl;				/* syl with underscore added */


	debug(4, "assoc_lyr2staff file=%s lineno=%d staffno=%d, verse=%d, all=%d, place=%d",
			gs_p->inputfile, gs_p->inputlineno, staffno, verse,
			all, place);

	if (staffno > Score.staffs) {
		lyr_error(gs_p->inputfile, gs_p->inputlineno,
			"can't have lyrics for staff %d when there aren't that many staffs",
			staffno);
		return;
	}

	/* make a copy of the grpsyl list for this staff/verse */
	gs_p = clone_gs_list(gs_p, NO);

	/* allocate space to save info, and fill it in */
	CALLOC(LYRINFO, new_p, 1);
	new_p->vno = (short) verse;
	new_p->place = place;
	new_p->gs_p = gs_p;

	/* insertion sort into list of lyrics for this staff */
	for (insert_place_p_p = & (Lyr_tbl[staffno]);
			*insert_place_p_p != (struct LYRINFO *) 0;
			insert_place_p_p = & ((*insert_place_p_p)->next) ) {

		lyrinfo_p = (*insert_place_p_p);

		if ( lyrinfo_p->vno > verse) {
			/* need to insert new one right before this one */
			break;
		}

		if ( (lyrinfo_p->vno == verse)
					&& (lyrinfo_p->place == new_p->place)) {
			lyr_error(Curr_filename, yylineno,
					"verse %d, staff %d multiply defined",
					verse, staffno);
			return;
		}
	}
	new_p->next = *insert_place_p_p;
	*insert_place_p_p = new_p;

	/* we need to set font/size on a per-staff basis. That means we
	 * have to make a temporary copy for each instance and get the
	 * syllables each time */
	lyrfontsize_p = getlyrfontsize(staffno, verse, Place, all);
	font = lyrfontsize_p->font;
	size = lyrfontsize_p->size;

	lyr_copy = lyrstring = copy_string(lyrstring + 2, font, size);

	/* get into internal format */
	fix_tagged_string(lyrstring, font, size, Curr_filename, yylineno);

	/* skip past the font/size */
	lyrstring += 2;

	/* fill in vno, staffno, and syllables */
	for (n = 0; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next, n++) {

		gs_p->staffno = (short) staffno;
		gs_p->vno = (short) verse;

		/* if this GRPSYL is a space, don't fill in a syllable */
		if (gs_p->grpcont == GC_SPACE) {
			continue;
		}

		/* otherwise, find the next syllable, and store it in
		 * the GRPSYL */
		gs_p->syl = next_syl(&lyrstring, &font, &size,
				&(gs_p->sylposition), staffno, gs_p->c);

		if (Need_extender != 0 && n < Num_need_extender
					&& Need_extender[n] == YES
					&& has_extender(gs_p->syl) == NO) {
			MALLOCA(char, newsyl, strlen(gs_p->syl) + 2);
			(void) sprintf(newsyl, "%s_", gs_p->syl);
			FREE(gs_p->syl);
			gs_p->syl = newsyl;
		}
	}

	/* keep track of font/size at end of measure in case we have more
	 * lyrics in this same staff/verse/place on a later measure */
	updlyrfontsize(lyrfontsize_p, font, size);

	/* skip over any trailing white space */
	while (*lyrstring == ' ' || *lyrstring == '\t') {
		lyrstring++;
	}

	if (*lyrstring != '\0') {
		lyr_mismatch("many");
	}

	/* the copy has been broken into syllables. Free the copy */
	FREE(lyr_copy);
}


/* given a staffno, verse, and place, return (via pointer to struct of info)
 * the font and size that should be used */

static struct LYRFONTSIZE *
getlyrfontsize(staffno, verse, place, all)

int staffno;
int verse;
int place;
int all;	/* YES if associated with "all" */

{
	struct LYRFONTSIZE *lfs_p;	/* walk through linked list */


	/* if this staff/verse/place is on the list, use the values stored
	 * there. */
	for (lfs_p = Lyrfontsizeinfo_p [staffno];
					lfs_p != (struct LYRFONTSIZE *) 0;
					lfs_p = lfs_p->next) {

		if (lfs_p->staffno == staffno && lfs_p->verse == verse
				&& lfs_p->place == place && lfs_p->all == all) {
			return(lfs_p);
		}
	}

	/* wasn't on list yet. Add to list, using default values from SSV */
	MALLOC(LYRFONTSIZE, lfs_p, 1);
	lfs_p->staffno = (short) staffno;
	lfs_p->verse = (short) verse;
	lfs_p->place = (char) place;
	lfs_p->all = (char) all;
	if (all == YES) {
		lfs_p->font = (char) (Score.lyricsfamily + Score.lyricsfont);
		lfs_p->size = (char) Score.lyricssize;
	}
	else {
		lfs_p->font = svpath(staffno, LYRICSFAMILY)->lyricsfamily
				+ svpath(staffno, LYRICSFONT)->lyricsfont;
		lfs_p->size = svpath(staffno, LYRICSFONT)->lyricssize;
	}
	lfs_p->next = Lyrfontsizeinfo_p [ staffno ];
	Lyrfontsizeinfo_p [ staffno ] = lfs_p;

	return(lfs_p);
}


/* update values of font/size for a given LYRFONTSIZE struct */

static void
updlyrfontsize(lyrfontsize_p, font, size)

struct LYRFONTSIZE *lyrfontsize_p;
int font;
int size;

{
	lyrfontsize_p->font = (char) font;
	lyrfontsize_p->size = (char) size;
}


/* when user sets lyricsfont via an SSV, that overrides whatever fonts are
 * currently in effect, so change them. */

void
setlyrfont(staffno, font)

int staffno;
int font;

{
	int s;				/* index through staffs */
	struct LYRFONTSIZE *lfs_p;	/* info about font/size */


	if (Context == C_SCORE) {
		Context = C_STAFF;
		/* reset all staffs */
		for (s = 1; s <= Score.staffs; s++) {
			setlyrfont(s, font);
		}
		Context = C_SCORE;
	}
	else {
		/* set all place/verse combinations of given staff */
		for (lfs_p = Lyrfontsizeinfo_p [staffno];
					lfs_p != (struct LYRFONTSIZE *) 0;
					lfs_p = lfs_p->next) {
			lfs_p->font = (lfs_p->all == YES ? Score.lyricsfamily
				+ Score.lyricsfont
				: svpath(staffno, LYRICSFAMILY)->lyricsfamily
				+ font);
		}
	}
}


/* when user sets lyricsize via an SSV, that overrides whatever fonts are
 * currently in effect, so change them. */

void
setlyrsize(staffno, size)

int staffno;
int size;

{
	int s;				/* index through staffs */
	struct LYRFONTSIZE *lfs_p;	/* font/size info */


	if (Context == C_SCORE) {
		/* reset all staffs */
		Context = C_STAFF;
		for (s = 1; s <= Score.staffs; s++) {
			setlyrsize(s, size);
		}
		Context = C_SCORE;
	}
	else {
		/* set all place/verse combinations of given staff */
		for (lfs_p = Lyrfontsizeinfo_p [staffno];
					lfs_p != (struct LYRFONTSIZE *) 0;
					lfs_p = lfs_p->next) {
			lfs_p->size = (char) (lfs_p->all == YES
					? Score.lyricssize : size);
		}
	}
}


/* This function is called when an entire measure has been collected, to
 * take care of all the lyrics.
 * For each staff, allocate the proper number of elements for the syls_p
 * list and fill them in.
 * Then free all the LYRINFO structs and clean out the Lyr_tbl.
 */

void
attach_lyrics2staffs(mll_staffs_p)

struct MAINLL *mll_staffs_p;	/* the list of staffs to attch to */

{
	struct STAFF *staff_p;		/* the staff we are currently dealing
					 * with */
	int verses;			/* how many verses for staff */
	struct LYRINFO *lyrinfo_p;	/* where lyrics info is stored */


	debug(4, "attach_lyrics2staffs");

	/* do each staff */
	for (   ; mll_staffs_p != (struct MAINLL *) 0;
					mll_staffs_p = mll_staffs_p->next) {

		if (mll_staffs_p->str != S_STAFF) {
			/* must have gotten to end of staffs; done */
			break;
		}

		/* need staff pointer a lot. Get shorter name for it */
		staff_p = mll_staffs_p->u.staff_p;

		/* count up how many verses there are associated with this
		 * staff */
		for ( verses = 0, lyrinfo_p = Lyr_tbl[staff_p->staffno];
					lyrinfo_p != (struct LYRINFO *) 0;
					lyrinfo_p = lyrinfo_p->next) {
			verses++;
		}

		/* if no verses, nothing to do */
		if (verses == 0) {
			continue;
		}

		/* alloc space for the appropriate number of verses */
		MALLOC(GRPSYL *, staff_p->syls_p, verses);
		if ((staff_p->sylplace = (short *) malloc
				(sizeof(short) * verses)) == (short *) 0) {
			l_no_mem(__FILE__, __LINE__);
		}
		staff_p->nsyllists = (short) verses;

		/* now attach the GRPSYLs and set the places for each */
		for (verses = 0, lyrinfo_p = Lyr_tbl[staff_p->staffno];
					lyrinfo_p != (struct LYRINFO *) 0;
					lyrinfo_p = lyrinfo_p->next, verses++) {

			staff_p->syls_p[verses] = lyrinfo_p->gs_p;
			staff_p->sylplace[verses] = lyrinfo_p->place;
		}

		free_lyrinfo(Lyr_tbl[staff_p->staffno]);
		Lyr_tbl[staff_p->staffno] = (struct LYRINFO *) 0;
	}
}


/* recursively free list of LYRINFO structs */

static void
free_lyrinfo(lyrinfo_p)

struct LYRINFO *lyrinfo_p;	/* the list to free */

{
	if (lyrinfo_p == (struct LYRINFO *) 0) {
		return;
	}

	free_lyrinfo(lyrinfo_p->next);
	FREE(lyrinfo_p);
}


/* At each bar line, reset previous verse number for auto-verse-numbering */

void
lyr_new_bar()
{
	int staff_index;
	int place_index;

	for (staff_index = 1; staff_index <= MAXSTAFFS; staff_index++) {
		for (place_index = 0; place_index < NUM_PLACE; place_index++) {
			Prev_verse_num[staff_index][place_index] = 0;
		}
	}
}



/* in several places we need to copy part of a lyric from one string to
 * another while keeping track of the font and size changes along the way.
 * Doing this with a function gets hard to think about because you'd
 * have to pass around a bunch of pointers to pointers and keep all the
 * levels of indirection straight, so I opted for a macro.
 * destbuff is the char array into which to copy. destindex is the subscript
 * into that array. src_p is a char * from which to copy. delimiter is the
 * character at which to stop copying. fnt_p and sz_p are pointer to font
 * and size which need to be updated if the font or size change.
 */
#define COPY_LYR(destbuff, destindex, src_p, delimiter, fnt_p, sz_p) \
	for (   ; *src_p != '\0' && *src_p != delimiter; src_p++) { \
		destbuff[destindex++] = *src_p; \
						\
		switch ( (unsigned) *src_p & 0xff) { \
		case STR_FONT: \
			*fnt_p = *++src_p; \
			destbuff[destindex++] = *src_p; \
			break; \
		case STR_SIZE: \
			*sz_p = *++src_p; \
			destbuff[destindex++] = *src_p; \
			break; \
		case STR_MUS_CHAR: \
		case STR_MUS_CHAR2: \
		case STR_USERDEF1: \
			destbuff[destindex++] = *++src_p; \
			destbuff[destindex++] = *++src_p; \
			break; \
		case STR_BACKSPACE: \
			destbuff[destindex++] = *++src_p; \
			break; \
		default: \
			break; \
		} \
	}



/* break a string of lyrics into individual syllables. The following
 * characters are special:
 *	<space>		marks end of syllable
 *	<tab>		same as space
 *	-		end of syllable
 *	_		end of syllable
 *	'<'		beginning of non-lyric item
 *	'>'		end of non-lyric item
 */
/* Each syllable will be returned as a string of the form:
 *	<font> <size> [<STR_PRE> pre-item <STR_PRE_END>] syllable [<STR_PST> post-item <STR_PST_END>]
 * where
 *	<font> is a 1-byte font identifier, FONT_TR, FONT_TI etc
 *	<size> is a 1-byte size, in points
 *	if there is non-lyrics stuff to be put before the lyric syllable, it
 *		will be surrounded by <STR_PRE> and <STR_PRE_END> characters.
 *		It may contain other commands for font changes, etc.
 *		This field is optional. If present it may be of arbitrary
 *		length, including zero length
 *		If this text is to be used for syllable
 *		placement, the header will be <STR_U_PRE> instead
 *	The syllable follows. It may contain any of the commands found in
 *		any other string. It can be of zero length.
 *	Optionally, there may there something to print after the syllable
 *		that isn't part of the syllable. If there is such a thing,
 *		it will be surrounded by <STR_PST> and <STR_PST_END> characters.
 *		If this text is to be used for syllable placement, the
 *		header will be <STR_U_PST> instead.
 *	Any of the fields can be of zero length, but at least one of them
 *		must be of non-zero length for each syllable to be sensible.
 * This function should be called after the string has been transformed
 * to internal format, with font/size changes stored as commands.
 * If a syllable starts with a positioning value, that is updated.
 * If there are any tags defined, that tag is added to the tag table,
 * and associated with the passed-in c[] array.
 */

static char *
next_syl(sylstring_p, font_p, size_p, position_p, staffno, c_p)

char **sylstring_p;	/* address of pointer to current spot in lyric string
			 * of syllables. At entry, its contents should be
			 * the address of the first character of the syllable
			 * to be returned. On return, its contents will be
			 * updated to point to the first character of the
			 * following syllable, to be ready for the next call
			 * to this function.
			 */
int *font_p;		/* pointer to current font value, will be updated
			 * with new font if it changes. */
int *size_p;		/* pointer to current size value; will be updated
			 * with new size if it changes */
short *position_p;	/* pointer to sylposition field, will be updated
			 * with the user-specified position value, if
			 * they specified one, otherwise will be left as is. */
int staffno;		/* which staff this syllable is for */
float *c_p;		/* If the user wants a tag associated with the
			 * current syllable, this is the c[] to associate
			 * with that tag. If no tag, this is ignored. */

{
	char *p;		/* pointer into lyrics string */
	int done;		/* boolean to keep track of if we are done */
	int font, size;		/* original font and size */
	int i;			/* index into sylbuff */
	char *t;		/* to walk through tag to find its end. */
	char sylbuff[BUFSIZ];	/* temporary storage for a syllable */


	/* initialize */
	sylbuff[0] = '\0';
	i = 0;
	font = *font_p;
	size = *size_p;

	/* skip any leading white space */
	for (p = *sylstring_p; *p != '\0'; p++) {
		if (*p != ' ') {
			break;
		}
	}

	/* see if we have a non-lyrics before the lyric */
	if ( *p == '<') {

		/* we do have a non-lyric. Put in the header, and copy up
		 * to the '>', keeping track of any font/size changes
		 * along the way */
		if (*(p+1) == '^') {
			sylbuff[i++] = (char) STR_U_PRE;
			p++;
		}
		else {
			sylbuff[i++] = (char) STR_PRE;
		}
		p++;
		COPY_LYR(sylbuff, i, p, '>', font_p, size_p);

		/* add in the > marker */
		sylbuff[i++] = (char) STR_PRE_END;
		if (*p == '\0') {
			lyr_error(Curr_filename, yylineno,
						"missing '>' in lyric");
		}
		else {
			p++;
		}
	}

	/* see if user gave a position value */
	if (*p == '|') {
		/* use default value from parameter */
		*position_p = svpath(staffno, SYLPOSITION)->sylposition;
		p++;
	}
	else if ( isdigit(*p) ||
			( (*p == '-' || *p == '+') && isdigit(*(p+1)) ) ) {
		char *after_num_p;
		int value;

		/* if there is a pipe after the number, it's a position */
		value = strtol(p, &after_num_p, 10);
		if ( *after_num_p == '|') {
			*position_p = value;
			p = after_num_p + 1;
		}
	}

	/* now collect the lyric syllable itself */
	for ( done = NO; *p != '\0' && i < sizeof(sylbuff); p++) {

		switch ( (unsigned) *p & 0xff) {

		case ' ':
		case '\t':
			/* definitely end of this lyric syllable */
			done = YES;
			break;


		case '~':
			/* two syllables joined into one. Replace the ~
			 * by a space */
			sylbuff[i++] = ' ';
			p++;
			break;

		case '-':
		case '_':
			/* end of syllable */
			/* need to peek ahead to check for non-lyric following */
			sylbuff[i++] = *p++;
			if ( *p != '<') {
				done = YES;
				break;
			}
			/*FALLTHRU*/

		case '<':
			if (*(p+1) == '^') {
				sylbuff[i++] = (char) STR_U_PST;
				p++;
			}
			else {
				sylbuff[i++] = (char) STR_PST;
			}
			p++;
			COPY_LYR(sylbuff, i, p, '>', font_p, size_p);

			sylbuff[i++] = (char) STR_PST_END;
			if (*p == '\0') {
				/* User forgot the >, so get out now,
				 * to avoid doing a *(p+1) below */
				continue;
			}
			/* Peek ahead. If next thing is a tag,
			 * then this syllable isn't over yet,
			 * so need to continue processing it */
			if ( (*(p+1) & 0xff) == STR_TAG) {
				continue;
			}

			p++;
			/* A ~ joins this syllable with the next to make
			 * them effectively one, but anything else ends
			 * the syllable. But copy - or _ if they are there */
			if ( *p != '~') {
				if (*p == '-' || *p == '_') {
					sylbuff[i++] = *p++;
				}
				done = YES;
			}
			break;

		case STR_FONT:
			sylbuff[i++] = *p++;
			*font_p = *p;
			if ( ! IS_STD_FONT(*font_p) ) {
				sylbuff[i++] = *p++;
			}
			break;

		case STR_SIZE:
			sylbuff[i++] = *p++;
			*size_p = *p;
			break;

		case STR_BACKSPACE:
			sylbuff[i++] = *p++;
			break;

		case STR_MUS_CHAR:
		case STR_MUS_CHAR2:
		case STR_USERDEF1:
			sylbuff[i++] = *p++;
			sylbuff[i++] = *p++;
			break;

		case STR_BOX:
			lyr_error(Curr_filename, yylineno,
					"boxed text not allowed in lyrics");
			break;

		case STR_BOX_END:
			break;

		case STR_CIR:
			lyr_error(Curr_filename, yylineno,
					"circled text not allowed in lyrics");
			break;

		case STR_CIR_END:
			break;

		case STR_PILE:
			lyr_error(Curr_filename, yylineno,
					"\\: not allowed in lyric syllable");
			break;

		case STR_C_ALIGN:
		case STR_L_ALIGN:
			lyr_error(Curr_filename, yylineno,
				"alignment not allowed in lyric syllable");
			break;

		case STR_TAG:
			/* Find the end of the tag */
			for (t = p + 1; (*t & 0xff) != STR_END_TAG; t++) {
				;
			}
			/* Temporary replace the STR_END_TAG with null so
		 	 * we can pass the tag name to addsym,
			 * then put it back. */ 
			*t = '\0';
			addsym(p + 1, c_p, CT_GRPSYL);
			*t = STR_END_TAG;

			/* Special case. If this is after a <> at the
			 * end of the syllable, we have to jump out */
			if ( (sylbuff[i-1] & 0xff) == STR_PST_END) {
				done = YES;
			}

			/* Move our position to end of the tag name */
			p = t;
			continue;

		default:
			break;
		}

		if (done == NO) {
			sylbuff[i++] = *p;
		}
		else {
			break;
		}
	}

	sylbuff[i] = '\0';

	if (i == 0) {
		/* Too few syllables */
		lyr_mismatch("few");
	}

	/* prepare for next call to this function */
	*sylstring_p = p;

	/* return a copy of the extracted syllable */
	return(copy_string(sylbuff, font, size));
}


/* given a syllable string, return the width (in inches) of the non-lyric
 * parts and of the actual syllable */

void
sylwidth(lyrstring, wid_b4_syl_p, wid_real_syl_p, wid_after_syl_p)

char *lyrstring;
float *wid_b4_syl_p;	/* width of leading non-lyrics will be put here */
float *wid_real_syl_p;	/* width of actual syllable will be put here */
float *wid_after_syl_p;	/* width of trailing non-lyric will be put here */

{
	do_sylwidth(lyrstring, wid_b4_syl_p, wid_real_syl_p, wid_after_syl_p,
							NO);
}


/* find width of syllable and <...> things. When called in placement phase
 * we count the <^...> and don't count <...>. When called in print phase
 * we do the opposite to compensate.
 */

static void
do_sylwidth(lyrstring, wid_b4_syl_p, wid_real_syl_p, wid_after_syl_p,
		compensating)

char *lyrstring;
float *wid_b4_syl_p;	/* width of leading non-lyrics will be put here */
float *wid_real_syl_p;	/* width of actual syllable will be put here */
float *wid_after_syl_p;	/* width of trailing non-lyric will be put here */
int compensating;	/* YES if being called from print phrase when we
			 * have to compensate for the fact that <...> may
			 * not have been used in placement. */

{
	int pre_counts = NO;	/* if pre width should be included */
	int post_counts = NO;	/* if post width should be included */
	int had_post = NO;	/* if had post <...> */
	float w1 = 0.0;		/* width of pre */
	float w2 = 0.0;		/* width of pre + syllable */
	float w3;		/* width of entire string */
	char *p;
	char save;		/* temp storage */
	int font, size;
	int ch;			/* one character of the syllable string */
	char *syl;		/* for stepping through syllable */
	float underscore_adjust;/* if ends with underscore,
				 * we don't count that, because it can
				 * be squeezed out */
	

	if (lyrstring == (char *) 0) {
		*wid_b4_syl_p = *wid_real_syl_p = *wid_after_syl_p = 0.0;
		return;
	}

	for (p = lyrstring; *p != '\0'; p++) {
		switch ( (unsigned) *p & 0xff) {
		case STR_PRE:
			if (compensating == YES) {
				pre_counts = YES;
			}
			break;
		case STR_U_PRE:
			if (compensating == NO) {
				pre_counts = YES;
			}
			break;
		case STR_PRE_END:
			/* get width of string so far. Temporarily shorten
			 * string to this point and get its length. This
			 * will be the length of the preceding <...> */
			*p = '\0';
			w1 = strwidth(lyrstring);
			*p = (char) STR_PRE_END;
			break;
		case STR_U_PST:
			/* get width of string so far. Temporarily shorten
			 * string to this point and get its length. This
			 * will be the length of the syllable and its
			 * preceding <...> */
			if (compensating == NO) {
				post_counts = YES;
			}
			save = *p;
			*p = '\0';
			w2 = strwidth(lyrstring);
			*p = save;
			had_post = YES;
			break;
		case STR_PST:
			if (compensating == YES) {
				post_counts = YES;
			}
			save = *p;
			*p = '\0';
			w2 = strwidth(lyrstring);
			*p = save;
			had_post = YES;
			break;
		default:
			break;
		}
	}

	/* get length of entire string */
	w3 =  strwidth(lyrstring);

	/* If the last thing in the string is an underscore, we don't
	 * count it in the width */
	font = lyrstring[0];
	size = lyrstring[1];
	syl = lyrstring + 2;

	/* see if last character is an underscore.
	 * This is somewhat similar to code in spread_extender(). */
	underscore_adjust = 0.0;
	while ( (ch = next_str_char( &syl, &font, &size)) != '\0') {
		if ( ch == '_' && IS_STD_FONT(font) ) {
			underscore_adjust = width(font, size, ch);
		}
		else {
			underscore_adjust = 0.0;
		}
	}
	w3 -= underscore_adjust;

	/* if there wasn't a post <...> we didn't yet get the width of
	 * the syllable plus preceding <...> */
	if (had_post == NO) {
		w2 = w3;
	}

	/* now calculate and return the widths */
	*wid_b4_syl_p = (pre_counts == YES ? w1 : 0.0);
	*wid_real_syl_p = w2 - w1;
	*wid_after_syl_p = (post_counts == YES ? w3 - w2 : 0.0);
}


/* during placement phase, <...> things were used or not used as appropriate
 * for setting placement. During print phrase, we do the opposite to
 * compensate, and update the group east and west boundaries as necessary */
void
lyr_compensate(gs_p)

struct GRPSYL *gs_p;

{
	float pre_wid, syl_wid, post_wid;

	do_sylwidth(gs_p->syl, &pre_wid, &syl_wid, &post_wid, YES);
	gs_p->c[AW] -= pre_wid;
	gs_p->c[AE] += post_wid;
}


/* every time a verse number is used, see whether we've already had that
 * verse number before. if not, add it to the list of verse numbers used. */
/* We could keep each range of defined verses in a RANGELIST, so that,
 * for example, 1-3 could be keep in a single struct. However, trying to
 * coalese the list could get very hairy, so do the simple-minded way of
 * saving each unique verse number in its own struct. We could also
 * insertion sort the list, but rarely will there be more than a couple
 * verses, and inserting into a singly linked list takes a slight amount
 * of work, so be very lazy and just search the whole list each time and
 * insert at beginning if need to add to list. */

static void
vno_used(vno)

int vno;	/* the verse number to check */

{
	struct RANGELIST *v_p;	/* to walk through list of verse nums used */
	struct RANGELIST **list_p_p;	/* which list to check */


	switch (Place) {
	case PL_ABOVE:
		list_p_p = &Above_vnumbers_used;
		break;
	case PL_BETWEEN:
		list_p_p = &Between_vnumbers_used;
		break;
	case PL_BELOW:
	case PL_UNKNOWN:
		list_p_p = &Below_vnumbers_used;
		break;
	default:
		pfatal("illegal place in vno_used");
		/*NOTREACHED*/
		return;
	}

	/* see if this verse is already on the list */
	for (v_p = *list_p_p; v_p != (struct RANGELIST *) 0; v_p = v_p->next) {
		if (v_p->begin == vno) {
			/* already had this verse before */
			return;
		}
	}

	/* must not have seen this verse number before, so add it */
	CALLOC(RANGELIST, v_p, 1);
	v_p->begin = (short) vno;
	/* ->end is not used in this list */
	v_p->next = *list_p_p;
	*list_p_p = v_p;
}


/* when parsing is complete, we can find out how many unique verse numbers
 * there were in the input */

void
set_maxverses()
{
	debug(4, "set_maxverses");

	count_verses(Above_vnumbers_used);
	count_verses(Below_vnumbers_used);
	count_verses(Between_vnumbers_used);
}

/* recursively walk down list of verse numbers used. Count them up and
 * free them while unwinding */

static void
count_verses(used_p)

struct RANGELIST *used_p;

{
	if (used_p == (struct RANGELIST *) 0) {
		return;
	}

	count_verses(used_p->next);
	Maxverses++;
	FREE(used_p);
}


/* Return pointer to the SSV that contains the default timeunit information
 * for verses being defined. Make sure if there are multiple staffs
 * being defined, they all have the same default timeunit. If there is only
 * one voice on the staff, use that for default. If there are 2 voices, if the
 * place is PL_ABOVE use voice 1, otherwise use voice 2. */

struct SSV *
get_lyr_dflt_timeunit_ssv()

{
	struct RANGELIST *staffrange_p;	/* to index through list of staffs */
	struct SSV *ssv_p;		/* containing relevent timeunit */
	struct SSV *ref_ssv_p;		/* to ensure consistency */
	RATIONAL timeunit;		/* timeunit for current staff */
	RATIONAL reference_timeunit;	/* the timeunit found on previous
					 * staff, for comparison to make sure
					 * they are the same */
	int staff;


	reference_timeunit = Zero;
	/* if all else fails, use Score value */
	ssv_p = ref_ssv_p = &Score;

	/* go down the list of staffs, get timeunit for each */
	for (staffrange_p = Staffrange_p;
					staffrange_p != (struct RANGELIST *) 0;
					staffrange_p = staffrange_p->next) {

		for (staff = staffrange_p->begin; staff <= staffrange_p->end;
						staff++) {

			/* get appropriate default timeunit value */
			if (svpath(staff, VSCHEME)->vscheme == V_1 ||
						Place == PL_ABOVE) {
				ssv_p = vvpath(staff, 1, TIMEUNIT);
			}
			else {
				ssv_p = vvpath(staff, 2, TIMEUNIT);
			}
			timeunit = ssv_p->timeunit;

			/* check for consistency */
			if ( NE(reference_timeunit, Zero) ) {
				if ( NE(timeunit, reference_timeunit) ||
						timelists_equal(
						ssv_p->timelist_p,
						ref_ssv_p->timelist_p) == NO) {
					/*  have a mis-match. Give error message,
					 * and return the first timeunit we got.
					 * No reason to check any more, because
					 * all that may happen is that we'll
				 	 * print lots more error messages for
					 * the same problem. */
					lyr_error(Curr_filename, yylineno,
					"timeunit value must be the same for all lyric staffs being defined on the same input line");
					return(ssv_p);
				}
			}
			else {
				/* first time through. Now we have a timeunit
				 * to use for default */
				reference_timeunit = timeunit;
				ref_ssv_p = ssv_p;
			}
		}
	}

	/* Used to pfatal if couldn't find default value. However, that can
	 * happen if user specifies an invalid staff number, so that isn't
	 * a program bug, but a user error. The user error would already have
	 * been flagged, so no reason to complain at all here */

	return(ssv_p);
}


/* user wants us to derive the lyrics time values from corresponding music
 * time values. Find the proper list of GRPSYLs to copy time values from,
 * and make a copy of that list, with field set properly for lyrics. */

struct GRPSYL *
derive_lyrtime()

{
	struct MAINLL *mll_p;		/* to find STAFF to derive from */
	struct GRPSYL *new_list_p;	/* the list of derived time values */
	struct GRPSYL *gs_p;		/* to walk through new_list_p */
	struct GRPSYL *ref_gs_p;	/* to walk through the reference list--
					 * the list we are deriving from */
	struct GRPSYL *nextgs_p;	/* for saving next of what to delete */
	int staff, vindex;		/* which list of GRPSYLs to clone */
	int place;			/* PL_* value */
	int extendlyrics;		/* Value of relevent parameter */
	int eindex;			/* Need_extender[] index */


	/* Make sure any old instance Need_extender gets zapped,
	 * in case something goes wrong and we don't get around to creating
	 * a new one.
	 */
	clear_need_extender();

	/* If there are no staffs associated with these lyrics, then nothing
	 * to do here. We should have already printed an error elsewhere */
	if (Staffrange_p == (struct RANGELIST *) 0) {
		return (struct GRPSYL *) 0;
	}

	/* find the proper music GRPSYL from which to derive time values */
	staff = leadstaff(&place);

	/* If user specified a staff/voice to derive from, use that to
	 * override what we would use. */
	if (Using_staff > 0) {
		if (rangecheck(Using_staff, MINSTAFFS, Score.staffs, "using staff") == YES) {
			staff = Using_staff;
		}
	}
	for (mll_p = Mainlltc_p; mll_p != (struct MAINLL *) 0;
						mll_p = mll_p->prev) {

		/* if back up all the way to a bar, user hasn't defined the
		 * right staff's music input yet */
		if (mll_p->str == S_BAR) {
			/* pretend we fell off top of list and break out.
			 * That way we only need one copy of error message code */
			mll_p = (struct MAINLL *) 0;
			break;
		}
		if (mll_p->str == S_STAFF && mll_p->u.staff_p->staffno == staff) {
			/* found the right staff data */
			break;
		}
	}

	if (mll_p == (struct MAINLL *) 0) {
		lyr_error(Curr_filename, yylineno,
			"can't derive lyrics time values: staff %d music not defined yet", staff);
		return (struct GRPSYL *) 0;
	}

	if (Using_voice > 0) {
		/* User told us which voice to use. */
		if (rangecheck(Using_voice, MINVOICES, MAXVOICES, "using voice") == NO) {
			return (struct GRPSYL *) 0;
		}

		if (vscheme_voices(svpath(staff, VSCHEME)->vscheme) < Using_voice) {
			lyr_error(Curr_filename, yylineno,
				"can't derive lyrics time values: staff %d does not have a voice %d", staff, Using_voice);
			return (struct GRPSYL *) 0;
		}

		vindex = Using_voice - 1;
		if (mll_p->u.staff_p->groups_p[vindex] == 0) {
			lyr_error(Curr_filename, yylineno,
				"can't derive lyrics time values: staff %d voice %d music not defined yet", staff, Using_voice);
			return (struct GRPSYL *) 0;
		}
	}
	else {
		/* User wants us to figure out which voice to use.
		 * Usually we use voice 1 of first staff specified.
		 * exceptions are if voice 1 is invisible or nonexistent
		 * or if explicitly below, and there is a visible voice 2,
		 * in which case voice 2 is used. */
		vindex = 0;
		if ((vvpath(staff, 1, VISIBLE)->visible == NO ||
				mll_p->u.staff_p->groups_p[0] == (struct GRPSYL *) 0
				|| place == PL_BELOW)
				&& mll_p->u.staff_p->groups_p[1] != (struct GRPSYL *) 0
				&& vvpath(staff, 2, VISIBLE)->visible == YES) {
			vindex = 1;
		}
		/* One more exception. If both voices are invisible,
		 * but we would have used voice 2 if it was visible,
		 * use voice 2. Otherwise if times would normally be
		 * derived from voice 2, but the whole staff is invisible,
		 * we could derive the wrong values. */
		if (vvpath(staff, 1, VISIBLE)->visible == NO &&
				vvpath(staff, 2, VISIBLE)->visible == NO &&
				mll_p->u.staff_p->groups_p[1] != (struct GRPSYL *) 0 &&
				place == PL_BELOW) {
			vindex = 1;
		}
	
		if (mll_p->u.staff_p->groups_p[vindex] == (struct GRPSYL *) 0) {
			lyr_error(Curr_filename, yylineno,
				"can't derive lyrics time values: staff %d voice %d music not defined yet", staff, vindex + 1);
			return (struct GRPSYL *) 0;
		}
	}

	new_list_p = clone_gs_list(mll_p->u.staff_p->groups_p[vindex], NO);

	/* See if we should deduce where underscore should be added. */
	extendlyrics = vvpath(staff, vindex + 1, EXTENDLYRICS)->extendlyrics;

	/* Grace notes don't count in the time derivation, so remove
	 * them from the cloned list. Count up the number of nongrace,
	 * which is how big to make the Need_extender array. */
	for (gs_p = new_list_p; gs_p != (struct GRPSYL *) 0; gs_p = nextgs_p) {
		nextgs_p = gs_p->next;
		if (gs_p->grpvalue == GV_ZERO) {
			struct GRPSYL *to_free_p;
			to_free_p = gs_p;
			if (gs_p->prev == 0) {
				new_list_p = gs_p->next;
			}
			else {
				gs_p->prev->next = gs_p->next;
			}
			/* Note that there should always be a 'next' after
			 * a grace note, so this 'if' shouldn't really be
			 * necessary, but it is here as defensive code. */
			if (gs_p->next != 0) {
				gs_p->next->prev = gs_p->prev;
			}
			FREE(to_free_p);
		}
		else if (extendlyrics) {
			Num_need_extender++;
		}
	}

	/* Allocate space for an array to tell where to add underscores */
	if (extendlyrics) {
		MALLOCA(int, Need_extender, Num_need_extender);
		for (eindex = 0; eindex < Num_need_extender; eindex++) {
			Need_extender[eindex] = NO;
		}
	}

	/* go through cloned list, changing type to lyrics and changing rests
	 * in the music to spaces for the lyrics. Also deal with ties and slurs,
	 * since notes that are tied or slurred to will be treated like a
	 * space, since there is probably only a single syllable wanted. */
	for (eindex = 0, gs_p = new_list_p,
			ref_gs_p = mll_p->u.staff_p->groups_p[vindex];
			gs_p != (struct GRPSYL *) 0;
			gs_p = gs_p->next, ref_gs_p = ref_gs_p->next, eindex++) {
		/* Skip graces in reference list. (There aren't any in
		 * the new list since we removed them above.) */
		while (ref_gs_p->grpvalue == GV_ZERO) {
			ref_gs_p = ref_gs_p->next;
			if (ref_gs_p == 0) {
				/* should be impossible to get here */
				pfatal("unexpected null ref_gs_p in derive_lyrtime");
			}
		}

		gs_p->grpsyl = GS_SYLLABLE;
		if (ref_gs_p->grpcont == GC_REST) {
			gs_p->grpcont = GC_SPACE;
		}
		else if (ref_gs_p->grpcont == GC_NOTES && ref_gs_p->nnotes > 0) {
			/* if the top note of the chord is tied or slurred,
			 * or the entire chord is tied,
			 * make the following lyrics GRPSYL, if any, a space. */
			if ((ref_gs_p->notelist[0].tie == YES ||
					ref_gs_p->tie == YES ||
					ref_gs_p->notelist[0].nslurto > 0)) {
				if (gs_p->next != 0) {
					gs_p->next->grpcont = GC_SPACE;
				}
				if (extendlyrics == YES) {
					Need_extender[eindex] = YES;
				}
			}
		}
	}

	/* One more detail: if the first note was tied-to or slurred-to from
	 * the previous measure, we have to change that to a space. To do that,
	 * we call prevgrpsyl, but it expects the staffno and vno to be filled
	 * in on the GRPSYL passed to it, and we're so early in parsing that
	 * that hasn't happened yet. So first have to patch that up. Later on,
	 * all the GRPSYLs will get the staffno and vno filled in, but it won't
	 * hurt anything that we've already done this one here; they will
	 * just be overwritten with the same values. */
	ref_gs_p = mll_p->u.staff_p->groups_p[vindex];
	ref_gs_p->staffno = mll_p->u.staff_p->staffno;
	ref_gs_p->vno = vindex + 1;
	if ((gs_p = prevgrpsyl(ref_gs_p, &mll_p)) != (struct GRPSYL *) 0) {
		if (gs_p->grpcont == GC_NOTES && gs_p->nnotes > 0) {
			if (gs_p->notelist[0].tie == YES ||
					gs_p->tie == YES ||
					gs_p->notelist[0].nslurto > 0) {
				new_list_p->grpcont = GC_SPACE;
			}
		}
	}

	/* Remember which staff/voice we used for derivation, so we can
	 * give a better error message if the syllables don't match up */
	/* Note that if the user specified "using S V" we would 
	 * already have the info in those variables, but since the Derive*
	 * variables were historically used to distinguish between whether
	 * the user specified time values or not, it seems best to keep
	 * them separate rather than overload them. */
	Derive_staff = staff;
	Derive_voice = vindex + 1;

	return (new_list_p);
}


/* If lyrics time is explicitly set by user rather than derived,
 * this should be called to mark that fact. */

void
not_derived()
{
	if (Using_staff > 0 && Doing_MIDI == NO) {
		l_warning(Curr_filename, yylineno,
			"'using' staff/voice ignored because time values were specified");
	}
	Derive_staff = Derive_voice = NOT_DERIVED;
	clear_need_extender();
}

/* Error message for when number of syllable doesn't match number of time
 * values. If the time values were derived, we tell user what they were
 * derived from, because that might be helpful.
 */

static void
lyr_mismatch(which)

char *which;	/* "few" or "many" */

{
	if (Derive_staff == NOT_DERIVED) {
		lyr_error(Curr_filename, yylineno,
				"too %s syllables in string", which);
	}
	else {
		lyr_error(Curr_filename, yylineno,
				"too %s syllables in lyrics string (Note: time values were derived from staff %d voice %d)",
				which, Derive_staff, Derive_voice);
	}
}


/* Free up any old instance of extender list, so we can't accidentally
 * reuse it.
 */

static void
clear_need_extender()
{
	if (Need_extender != 0) {
		FREE(Need_extender);
		Need_extender = 0;
	}
	Num_need_extender = 0;
}


/* If being run for generating a MIDI file, we don't really care about lyrics.
 * So we treat lyrics errors as just warnings when in MIDI mode. 
 * Note that possibly some day we might put lyrics in the MIDI file
 * (wish list item #308), in which case we would begin to care.
 */

/*VARARGS3*/
static void
#ifdef __STDC__
lyr_error(char *filename, int lineno, char * format, ...)
#else
lyr_error(filename, lineno, format, va_alist)

char *filename;
int lineno;
char *format;
va_dcl
#endif

{
	va_list args;
	/* Passing args from one varargs function to another can be tricky.
	 * So we do the formating here with vsprintf, and pass the
	 * formatted result. We know we are only called for error messages
	 * of limited size, either a fixed string, or where there is a %d
	 * we know it can't be very many digits long, or where there is
	 * a %s it gets passed a fixed string of known (quite short) length,
	 * so we don't have to worry about overflow that could result in
	 * core dumps or security exploits,
	 * as long as we make it plenty longer than the longest message,
	 * which is only about 100 bytes.
	 */
	char buffer[512];


#ifdef __STDC__
       va_start(args, format);
#else
       va_start(args);
#endif
	vsprintf(buffer, format, args);
	va_end(args);

	if (Doing_MIDI == YES) {
		l_warning(filename, lineno, "%s", buffer);
	}
	else {
		l_yyerror(filename, lineno, "%s", buffer);
	}
}
