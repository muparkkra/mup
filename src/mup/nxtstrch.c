/*
 Copyright (c) 1995-2024  by Arkkra Enterprises.
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

/* This file contains functions related to walking through strings,
 * to get each character in turn, along with any font/size changes
 * and horizontal/vertical motions that should be done
 * before printing that character.
 */

#include "defines.h"
#include "structs.h"
#include "globals.h"


/* when entering pile, adjust size by PILE_A / PILE_B and when
 * exiting pile, adjust by the recipocal of that. */
#define PILE_A 	65
#define PILE_B	100


/* This structure contains information about one line in a set of lines
 * that are piled on top of one another. This might be used for figured
 * bass or superscript/subscript or things like that. */
struct PILED_LINE {
	char *text;		/* points to start of piled line */
	short align_offset;	/* where the alignment is, relative to text */
	short length;		/* bytes in this line */
	short last_char_offset;	/* where the last character is */
	short last_digit_offset;/* where the last digit is */
	float width_left_of_align;	/* width for this line */
	float width_right_of_align;
	struct PILED_LINE *next;/* the next line in the pile */
};

/* This keep track of an entire piled section of a string */
struct PILED {
	float curr_x, curr_y;	/* where we are now, relative to pile start */
	struct PILED_LINE *lines_p;	/* list of lines in pile */
	struct PILED_LINE *curr_line_p;	/* which piled line we're processing */
	short curr_offset;		/* offset into curr_line_p->text */
	float width_left_of_align;	/* width of the entire pile */
	float width_right_of_align;
	short orig_font;		/* font coming into the pile */
	short orig_size;		/* size coming into the pile */
	short new_size;			/* size at beginning of pile */
	char *pile_end;			/* last character of pile */
};

/* One of these structs is passed to get_piled() with the current
 * font and size filled in. If we are in piled mode, it will return 1 and
 * will have filled in all the field. Otherwise it will return 0.
 */

struct PILE_INFO {
	char	font;
	char	size;
	char	ch;	/* the next character in the pile, or \0 if
			 * this only indicates motion */
	double	hor;	/* how much to adjust before printing ch */
	double	vert;	/* how much to adjust before printing ch */
};

/* When a backspace is returned, this keeps track of its width, to
 * potentially be returned via backsp_width() */
static int Backspace_dist;

/* This is all the info about the piled section we are currently processing.
 * We only deal with one pile at a time, so a single static structure
 * meets our needs. */
static struct PILED Pile_info;

static void begin_pile P((struct PILE_INFO *pile_info_p));
static char *prep_pile P((char *string, struct PILE_INFO *pile_info_p));
static int get_piled P((struct PILE_INFO *pile_info_p));
static double vert_distance P((int vert_arg, int font, int size));


/* When a STR_PILE is encountered, this should be called to gather all
 * the information we might need about how to align everything in the pile.
 * Then other functions like get_piled() can be called to retrieve info.
 * The passed-in pile_info_p contains the font and size at the beginning
 * of the pile, and on return will contain the new size and the vertical
 * and horizontal offsets before the first character of the pile.
 * Return value points to the last character that was processed from string.
 */

static char *
prep_pile(string, pile_info_p)

char *string;		/* place in string where piling begins */
struct PILE_INFO *pile_info_p;	/* passed in font and size, returns size and vert */

{
	struct PILED_LINE **piled_line_p_p;	/* where to link on next line */
	struct PILED_LINE *line_p;		/* the current line of pile */
	char *s;				/* to walk through string */
	int alignments = 0;			/* how many align points found */
	int lines = 0;				/* how many lines in pile */
	float width_contribution;		/* width of current char */
	int i;					/* index though a line */
	int orig_i;				/* value of i when current
						 * character began (some
						 * things take up multiple
						 * bytes and we need to know
						 * where it began. */
	int font, size;
	int eff_muschar_size;			/* adjusted for being in pile */

	/* initialize */
	Pile_info.width_left_of_align = Pile_info.width_right_of_align = 0.0;
	Pile_info.lines_p = (struct PILED_LINE *) 0;
	Pile_info.curr_line_p = (struct PILED_LINE *) 0;
	Pile_info.curr_offset = 0;
	piled_line_p_p = &(Pile_info.lines_p);
	line_p = *piled_line_p_p;

	Pile_info.orig_font = font = pile_info_p->font;

	/* Adjust size for entering the pile */
	size = Pile_info.new_size = pile_size(pile_info_p->size, NO);

	/* Walk through the piled things, skipping past the initial STR_PILE,
	 * this time just to find the line boundaries and alignment offsets. */
	for (s = string + 1; *s != '\0'; s++) {

		/* if end piling, jump out */
		int curr_char;	
		curr_char = *s & 0xff;
		if ( curr_char == STR_PILE || curr_char == STR_BOX_END
						|| curr_char == STR_CIR_END) {
			if (line_p != (struct PILED_LINE *) 0) {
				line_p->length = s - line_p->text;
			}
			line_p = (struct PILED_LINE *) 0;
			s++;
			break;
		}

		/* If we don't have a current PILED_LINE struct,
		 * get one, and initialize it. */
		if (line_p == (struct PILED_LINE *) 0) {
			MALLOC(PILED_LINE, *piled_line_p_p, 1);
			line_p = *piled_line_p_p;
			line_p->text = s;
			line_p->align_offset = -1;
			line_p->last_digit_offset = -1;
			line_p->last_char_offset = -1;
			line_p->length = -1;
			line_p->width_left_of_align = 0.0;
			line_p->width_right_of_align = 0.0;
			line_p->next = (struct PILED_LINE *) 0;
			lines++;
		}

		switch (*s & 0xff) {

		case STR_C_ALIGN:
		case STR_L_ALIGN:
			/* Point the offset at the following character,
			 * because for C_ALIGN that character will straddle
			 * the alignment point, and we can easily make
			 * L_ALIGN work with that convention too. */
			line_p->align_offset = s - line_p->text + 1;
			alignments++;
			break;

		case '\n':
			/* if newline, finish this line by saving its length,
			 * then prepare for next line by moving the place at
			 * which to add a line to the current line's 'next' */
			line_p->length = s - line_p->text;
			piled_line_p_p = &(line_p->next);
			line_p = (struct PILED_LINE *) 0;
			break;

		case STR_FONT:
		case STR_SIZE:
		case STR_BACKSPACE:
		case STR_VERTICAL:
			/* need to skip past its argument byte */
			s++;
			break;

		case STR_MUS_CHAR:
		case STR_MUS_CHAR2:
		case STR_USERDEF1:
			/* Mark as last character so far, in case we need
			 * to use that as default alignment */
			line_p->last_char_offset = s - line_p->text;
			/* need to skip past its 2 argument bytes */
			s += 2;
			break;

		case STR_PAGENUM:
		case STR_NUMPAGES:
			/* We actually don't allow this, but can get to this
			 * code in error cases. Skip past argument. */
			s++;
			break;

		case STR_KEYMAP:
			pfatal("prep_pile called before keymap processing");
			/*NOTREACHED*/
			break;

		default:
			/* Mark as last character so far, in case we need
			 * to use that as default alignment */
			line_p->last_char_offset = s - line_p->text;

			/* remember location of last digit, in case we
			 * need to use as default aligment */
			if ( isdigit(*s) ) {
				line_p->last_digit_offset = s - line_p->text;
			}
			break;
		}
	}

	if (*s == '\0' && line_p != (struct PILED_LINE *) 0) {
		line_p->length = s - line_p->text;
	}

	/* if there are lines with unspecified alignment points, find them */
	if (alignments < lines) {
		for (line_p = Pile_info.lines_p;
				line_p != (struct PILED_LINE *) 0;
				line_p = line_p->next) {
			/* If no user-specified alignment point in this line,
			 * use last digit if any, else last char. If there was
			 * no last char (empty line, maybe to get subscript
			 * without superscript, for example) this will end
			 * up with an offset of -1. */
			if (line_p->align_offset < 0) {
				line_p->align_offset =
				    ((line_p->last_digit_offset >= 0)
				    ? line_p->last_digit_offset
				    : line_p->last_char_offset);
			}
		}
	}

	/* Find left and right of alignment for each line and the
	 * widest overall for the pile. */
	for (line_p = Pile_info.lines_p;
					line_p != (struct PILED_LINE *) 0;
					line_p = line_p->next) {

		/* Some items take more than one byte. In that case,
		 * the index 'i' in this 'for' loop will get more added to it
		 * than just the increment at the end of each loop.
		 * When this happens, orig_i keeps track of where the
		 * multi-byte item began. */
		for (i = 0; i < line_p->length; i++) {
			
			orig_i = i;
			width_contribution = 0.0;

			switch (line_p->text[i] & 0xff) {

			case STR_MUS_CHAR:
			case STR_MUS_CHAR2:
			case STR_USERDEF1:
				eff_muschar_size = adj_size(line_p->text[i+1], 
				(double) size / (double) Pile_info.orig_size,
				0, 0);
				/* get its width and skip past its args */
				width_contribution += width(
						str2mfont(line_p->text[i] & 0xff),
						eff_muschar_size,
						line_p->text[i+2]);
				i += 2;
				break;

			case STR_FONT:
				/* Remember the font, may need for getting
				 * widths of future characters */
				font = line_p->text[++i];
				break;

			case STR_SIZE:
				/* remember the size, may need for getting
				 * widths of future characters */
				size = line_p->text[++i];
				break;

			case STR_C_ALIGN:
			case STR_L_ALIGN:
			case STR_SLASH:
				/* irrelevant for width, no args */
				break;

			case STR_VERTICAL:
				/* irrelevant for width, 1 byte arg to skip */
				i++;
				break;

			case STR_BACKSPACE:
				width_contribution -= ((line_p->text[++i] *
					size) / DFLT_SIZE) / BACKSP_FACTOR;
				break;

			case STR_BOX:
			case STR_BOX_END:
			case STR_CIR:
			case STR_CIR_END:
				/* not relevant here */
				break;

			case STR_PAGENUM:
			case STR_NUMPAGES:
				/* we don't really know how wide this will
				 * end up being, so just use the width of
				 * the following byte (% or #).
				 */
				width_contribution = width(font, size,
                                                line_p->text[++i]);
				break;
			default:
				if ( IS_STR_COMMAND(line_p->text[i]) & 0xff) {
					pfatal("unexpected STR_ value 0x%x in prep_pile()",
						line_p->text[i] & 0xff);
				}

				/* ordinary character, look up its width */
				width_contribution = width(font, size,
						line_p->text[i]);
				break;
			}

			/* Figure out which side of alignment.
			 * Anything before align_offset goes on left,
			 * while anything after goes on the right.
			 * If we're right at the alignment offset,
			 * and the alignment type is STR_C_ALIGN,
			 * split the width. Otherwise goes on right. */
			if (orig_i < line_p->align_offset) {
				line_p->width_left_of_align
					+= width_contribution;
			}
			else if (orig_i == line_p->align_offset &&
					(line_p->text[orig_i - 1] & 0xff)
					!= STR_L_ALIGN) {
				/* put half on each side */
				width_contribution /= 2.0;
				line_p->width_left_of_align
					+= width_contribution;
				line_p->width_right_of_align
					+= width_contribution;
			}
			else {
				line_p->width_right_of_align
					+= width_contribution;
			}
		}

		/* find widest line */
		if (line_p->width_left_of_align
					> Pile_info.width_left_of_align) {
			Pile_info.width_left_of_align
						= line_p->width_left_of_align;
		}
		if (line_p->width_right_of_align
					> Pile_info.width_right_of_align) {
			Pile_info.width_right_of_align
						= line_p->width_right_of_align;
		}
	}

	/* start at first character of first line of pile */
	begin_pile(pile_info_p);

	Pile_info.pile_end = s;
	return(s);
}


/* fill in the fields of pile_info_p for starting at the beginning of
 * the current pile. */

static void
begin_pile(pile_info_p)

struct PILE_INFO *pile_info_p;

{
	/* begin at first line of pile */
	Pile_info.curr_line_p = Pile_info.lines_p;
	Pile_info.curr_offset = -1;

	/* if user ended a string with a pile indicator (which is a bit
	 * silly, but could happen...) get out now,
	 * to avoid dereferencing a line that doesn't exist. */
	if (Pile_info.curr_line_p == (struct PILED_LINE *) 0) {
		return;
	}

	/* fill in rest of returned data */
	pile_info_p->hor = Pile_info.curr_x = Pile_info.width_left_of_align
				- Pile_info.lines_p->width_left_of_align;
	/* Adjust vertical by half the unpiled line height, but less a little
	 * (there is some white space padding, usually piles are numbers
	 * without descenders, and they are smaller anyway, so looks better
	 * to jam things together a bit more than normal). */
	pile_info_p->vert = Pile_info.curr_y
		= fontheight(Pile_info.orig_font, Pile_info.orig_size) / 2.0 
		- POINT * Pile_info.new_size / DFLT_SIZE;
	pile_info_p->size = Pile_info.new_size;

	pile_info_p->ch = '\0';
}


/* Get the next character in piled segment, along with any horizontal
 * and/or vertical motion that should be done before printing that
 * character. Info is placed into the struct passed in. If the returned
 * ->ch is '\0', that means this call only returned motion information.
 * Returns NO at end of pile, otherwise YES.
 */

static int
get_piled(pile_info_p)

struct PILE_INFO *pile_info_p;

{
	char *text = 0;		/* Piled text to process. The initialization
				 * is just to avoid bogus "used before set"
				 * warning. It will always get set to a real
				 * string before actually being used. */
	float backdist;		/* width of backspace */
	int mfont;		/* FONT_MUSIC*  */


	/* Move on to next character. */
	++(Pile_info.curr_offset);

	if (Pile_info.curr_line_p != (struct PILED_LINE *) 0) {

		/* Check if we've gotten to end of pile. We are if we're
		 * past the last character of the last line of the pile */
		if (Pile_info.curr_line_p->next == (struct PILED_LINE *) 0 &&
				Pile_info.curr_offset
				>= Pile_info.curr_line_p->length) {
			/* yes, at end of pile */
			Pile_info.curr_line_p = (struct PILED_LINE *) 0;
		}
		else {
			text = Pile_info.curr_line_p->text;
		}
	}

	/* check for end of pile */
	if (Pile_info.curr_line_p == (struct PILED_LINE *) 0) {

		/* Adjust vertical and horizontal to past pile.
		 * Horizontal adjust is the total width of the
		 * pile minus where we are currently. */
		pile_info_p->hor = (Pile_info.width_left_of_align
					+ Pile_info.width_right_of_align)
					- Pile_info.curr_x;
		/* Vertical adjust is the negative of the current
		 * y, since that gets us back to zero relative to
		 * where we started. */
		pile_info_p->vert = - (Pile_info.curr_y);

		/* Restore size for leaving the pile. */
		pile_info_p->size = pile_size(pile_info_p->size, YES);

		/* mark as not being an actual character, just motion */
		pile_info_p->ch = '\0';

		return(NO);
	}

	/* handle newline -- go to next line of pile */
	if (text[Pile_info.curr_offset] == '\n') {
		/* prepare for next line, if any */
		Pile_info.curr_line_p = Pile_info.curr_line_p->next;
		Pile_info.curr_offset = -1;

		/* Adjust vertical and horizontal to start next line of pile.
		 * For horizontal, go back to beginning of pile,
		 * then add the difference between this line's
		 * left of align and the pile's. */
		if (Pile_info.curr_line_p != (struct PILED_LINE *) 0) {
			pile_info_p->hor = - Pile_info.curr_x;
			Pile_info.curr_x = Pile_info.width_left_of_align
				- Pile_info.curr_line_p->width_left_of_align;
			pile_info_p->hor += Pile_info.curr_x;

			/* vertical is adjusted by height of font, less
			 * a little to jam together to look better. */
			pile_info_p->vert = - fontheight(pile_info_p->font,
				pile_info_p->size)
				+ POINT * pile_info_p->size / DFLT_SIZE;
			Pile_info.curr_y += pile_info_p->vert;
		}
		else {
			pile_info_p->vert = pile_info_p->hor = 0.0;
		}

		/* only motion */
		pile_info_p->ch = '\0';

	}

	else {
		pile_info_p->hor = pile_info_p->vert = 0.0;

		switch (text[Pile_info.curr_offset] & 0xff) {

		case STR_MUS_CHAR:
		case STR_MUS_CHAR2:
		case STR_USERDEF1:
			mfont = str2mfont( text[Pile_info.curr_offset] & 0xff);
			pile_info_p->font = mfont;
			pile_info_p->size = adj_size(
				text[Pile_info.curr_offset+1],
				(double) pile_info_p->size / (double) Pile_info.orig_size,
				0, 0);
			pile_info_p->ch = text[Pile_info.curr_offset+2];
			Pile_info.curr_x += width(mfont,
					pile_info_p->size, pile_info_p->ch);
			Pile_info.curr_offset += 2;
			break;

		case STR_FONT:
			(Pile_info.curr_offset)++;
			pile_info_p->font = text[Pile_info.curr_offset];
			pile_info_p->ch = '\0';
			break;

		case STR_SIZE:
			(Pile_info.curr_offset)++;
			pile_info_p->size = text[Pile_info.curr_offset];
			pile_info_p->ch = '\0';
			break;

		case STR_SLASH:
			pile_info_p->ch = (char) STR_SLASH;
			break;

		case STR_C_ALIGN:
		case STR_L_ALIGN:
			pile_info_p->ch = '\0';
			break;

		case STR_VERTICAL:
			(Pile_info.curr_offset)++;
			pile_info_p->vert = vert_distance(
				DECODE_VERT(text[Pile_info.curr_offset]),
				pile_info_p->font, pile_info_p->size);
			pile_info_p->ch = '\0';
			break;

		case STR_BACKSPACE:
			(Pile_info.curr_offset)++;
			Backspace_dist = text[Pile_info.curr_offset];
			backdist = backsp_width(pile_info_p->size);
			pile_info_p->hor -= backdist;
			Pile_info.curr_x -= backdist;
			pile_info_p->ch = '\0';
			break;

		case STR_PAGENUM:
		case STR_NUMPAGES:
			/* These aren't really allowed in piles,
			 * but we can hit this code under some error cases,
			 * so we recognize them and do something reasonable.
			 */
			(Pile_info.curr_offset)++;
			pile_info_p->ch = text[Pile_info.curr_offset];
			break;
		
		default:
			if ( IS_STR_COMMAND(text[Pile_info.curr_offset]) & 0xff) {
				pfatal("unexpected STR_ value 0x%x in get_piled()",
					text[Pile_info.curr_offset] & 0xff);
			}

			/* ordinary character */
			Pile_info.curr_x += width(pile_info_p->font,
					pile_info_p->size,
					text[Pile_info.curr_offset]);
			pile_info_p->ch = text[Pile_info.curr_offset];
			break;
		}
	}

	return(YES);
}


/* given a pointer into a string, return the code for the next
 * character in the string. This will be either the ASCII code
 * or the music symbol code. This function will also update the
 * pointer into the string and the font and size if appropriate.
 * If there happens to be a music character in the middle of a string,
 * the font (and maybe size) will temporarily be changed for the music
 * font. This function keeps track of that.
 *
 * Restrictions: This function must be used by calling it on the beginning
 * of a string, then to walk through that string. You don't have to go all the
 * way to the end of a given string, but once you switch to a new string
 * (from which you must begin at its beginning) you cannot go back to the
 * middle of some other string that you had been working on before.
 *
 * This function calls the more general function which can also update
 * vertical motion and alignment information. It is retained for backward
 * compatibility for the places that don't care about those things.
 */

int
next_str_char(str_p_p, font_p, size_p)

char **str_p_p;	/* address of pointer into string. Will be incremented
		 * to beyond current character on return (this might
		 * be several bytes into the string if current char
		 * is a special string command */
int *font_p;	/* pointer to font. Will be updated to new font if
		 * appropriate */
int *size_p;	/* pointer to size. Will be updated to new size if
		 * appropriate */

{
	/* place to hold things we don't care about from general function */
	double dummy_vertical, dummy_horizontal;
	int dummy_in_pile;
	int dummy_textfont;

	return(nxt_str_char(str_p_p, font_p, size_p, &dummy_textfont,
		&dummy_vertical, &dummy_horizontal, &dummy_in_pile, NO));
}


/* returns the next character in string, along with any font/size changes
 * and horizontal/vertical motions that should happen before that character.
 */

int
nxt_str_char(str_p_p, font_p, size_p, textfont_p, vertical_p, horizontal_p, in_pile_p, slash)

char **str_p_p;
int *font_p;
int *size_p;
int *textfont_p;	/* font disregarding music characters */
double *vertical_p;	/* if non-zero, do vertical motion */
double *horizontal_p;	/* if non-zero, do horizontal motion */
int *in_pile_p;		/* will be set to YES if inside a pile */
int slash;		/* if YES, return any STR_SLASH's found,
			 * else ignore them */

{
	static char *prev_str_p;	/* most recent value of *str_p_p */
	static int textfont;		/* current font, ignoring music chars */
	static int textsize;		/* current size, ignoring music chars */
	static int pile_mode;		/* if inside a pile */
	int code;
	struct PILE_INFO pile_info;

	/* initialize to assume no vertical motion or alignment at this char */
	*horizontal_p = *vertical_p = 0.0;

	if (*str_p_p == (char *) 0) {
		return(0);
	}

	for ( ; ; ) {

		if (prev_str_p != *str_p_p) {
			/* must be starting on a different string. Our
			 * remembered text font/size are no longer valid */
			textfont = *font_p;
			*textfont_p = textfont;
			textsize = *size_p;
			pile_mode = NO;
		}

		if (pile_mode == YES) {
			/* call the special function for piled to get next */
			pile_info.font = textfont;
			pile_info.size = textsize;
			pile_mode = get_piled(&pile_info);

			/* save what it returned */
			*font_p = pile_info.font;
			*size_p = pile_info.size;
			if (pile_info.font != FONT_MUSIC) {
				textfont = pile_info.font;
				*textfont_p = textfont;
				textsize = pile_info.size;
			}

			/* Add motion to any accumulated so far */
			 *vertical_p += pile_info.vert;
			 *horizontal_p += pile_info.hor;

			if (pile_info.ch == '\0') {
				/* only motion this time, keep going */
				continue;
			}
			else if ( (pile_info.ch & 0xff) == STR_SLASH &&
						slash == NO) {
				/* caller doesn't care about slashes */
				continue;
			}
			else {
				*in_pile_p = pile_mode;
				return (pile_info.ch);
			}
		}
		*in_pile_p = NO;

		if (**str_p_p == '\0') {
			/* end of string */
			*font_p = textfont;
			*size_p = textsize;
			return(0);
		}

		if ( ! IS_STR_COMMAND(**str_p_p)) {
			/* just an ordinary character. Return it after updating
			 * pointer to point past it */
			code = **str_p_p & 0xff;
			++(*str_p_p);
			prev_str_p = *str_p_p;

			/* if last chacter was music character, put back real				 * font and size */
			if (IS_MUSIC_FONT(*font_p)) {
				*font_p = textfont;
				*size_p = textsize;
			}
			return(code);
		}

		/* special command. see which one */
		switch ( (unsigned) (**str_p_p & 0xff) ) {

		case STR_FONT:
			/* change font to specified font and point past it */
			*font_p = *++(*str_p_p);
			++(*str_p_p);
			textfont = *font_p;
			*textfont_p = textfont;
			break;

		case STR_SIZE:
			/* change size to specified size and point past it */
			*size_p = *++(*str_p_p);
			++(*str_p_p);
			textsize = *size_p;
			break;

		case STR_PAGENUM:
			/* Page number and number of pages are strange cases.
			 * We don't necessarily know at this point
			 * how many digits long they will be. So we punt:
			 * just return % or #. For height, ascent, and descent,
			 * the % or # will be the same as any number
			 * of digits (or at least close enough, we hope).
			 * That means the only problem is when we are getting
			 * the width of a string or actually printing
			 * it, so we'll deal with those as special cases */
			*str_p_p += 2;
			prev_str_p = *str_p_p;
			return('%');
		case STR_NUMPAGES:
			*str_p_p += 2;
			prev_str_p = *str_p_p;
			return('#');


		case STR_MUS_CHAR:
		case STR_MUS_CHAR2:
		case STR_USERDEF1:
			/* music character */
			*font_p = str2mfont( (unsigned) (**str_p_p & 0xff) );
			*size_p = *++(*str_p_p);
			code = (*++(*str_p_p)) & 0xff;
			++(*str_p_p);
			prev_str_p = *str_p_p;
			return(code);

		case STR_BACKSPACE:
			Backspace_dist = (*++(*str_p_p)) & 0xff;
			++(*str_p_p);
			prev_str_p = *str_p_p;
			*font_p = textfont;
			*size_p = textsize;
			return('\b');

		case STR_PRE:
		case STR_U_PRE:
		case STR_PRE_END:
		case STR_PST:
		case STR_U_PST:
		case STR_PST_END:
		case STR_BOX:
		case STR_BOX_END:
		case STR_CIR:
		case STR_CIR_END:
			++(*str_p_p);
			break;

		case STR_UNDER_END:
			*str_p_p = *str_p_p + 4;
			break;

		case STR_VERTICAL:
			*vertical_p += vert_distance(
					DECODE_VERT( *++(*str_p_p) ),
					textfont, textsize);
			++(*str_p_p);
			break;

		case STR_C_ALIGN:
		case STR_L_ALIGN:
			++(*str_p_p);
			break;

		case STR_PILE:
			pile_info.font = textfont;
			pile_info.size = textsize;
			*str_p_p = prep_pile(*str_p_p, &pile_info);
			*vertical_p += pile_info.vert;
			*horizontal_p += pile_info.hor;
			*size_p = pile_info.size;
			textsize = *size_p;
			pile_mode = YES;
			break;

		case STR_SLASH:
			++(*str_p_p);
			prev_str_p = *str_p_p;
			if (slash == YES) {
				return(STR_SLASH);
			}
			break;

		case STR_TAG:
			/* We normally shouldn't be here. The only time that
			 * "should" happen is if the user tried to use a tag
			 * in a no-lyrics string, and we're just forging ahead.
			 * So if we're going to be bailing out eventually,
			 * just not quite yet, we can just skip by the tag.
			 */
			if (Errorcount > 0) {
				while (**str_p_p != '\0' &&
						((**str_p_p) & 0xff)
						!= (STR_END_TAG & 0xff)) {
					++(*str_p_p);
				}
				if ( ((**str_p_p) & 0xff) == (STR_END_TAG & 0xff)) {
					++(*str_p_p);
				}
			}
			else {
				pfatal("unexpected STR_TAG in nxt_str_ch()");
			}
			break;

		default:
			pfatal("unexpected STR_ (%x) in nxt_str_ch()",
					**str_p_p & 0xff);
			/*NOTREACHED*/
			break;
		}
		prev_str_p = *str_p_p;
	}
}


/* Given an argument to STR_VERTICAL and a font and size,
 * return the distance in inches to move vertically. */

static double
vert_distance(vert_arg, font, size)

int vert_arg;
int font;
int size;

{
	double ratio;

	/* first convert to ratio relative to font height */
	if (vert_arg >= 0) {
		ratio = (double) vert_arg / (double) MAXVERTICAL;
	}
	else {
		ratio = (double) vert_arg / (double) (-MINVERTICAL);
	}
	return(ratio * fontheight(font, size));
}


/* return width of most recent backspace returned by next_str_char() */

double
backsp_width(size)

int size;	/* adjust to this point size */

{
	return ( (double) Backspace_dist * ((double) size / (double) DFLT_SIZE)
					/ BACKSP_FACTOR);
}


/* Return width of current pile */

double
pile_width()

{
	return(Pile_info.width_left_of_align + Pile_info.width_right_of_align);
}


/* adjust size when entering/leaving a pile. Called with current size and
 * whether we are entering or leaving. It returns the new size. */

int
pile_size(size, in_pile)

int size;	/* current size */
int in_pile;	/* if YES, we are currently in a pile and are about to leave
		 * it; if NO, we are about to enter the pile */

{
	int new_size;

	if (in_pile == NO) {
		/* entering pile. Save current size, and return new size */
		Pile_info.orig_size = size;
		/* constrain new size to be at least MINSIZE */
		Pile_info.new_size = (int)(Score.pilescale * (double) size);
		if (Pile_info.new_size < MINSIZE) {
			Pile_info.new_size = MINSIZE;
		}
		return(Pile_info.new_size);
	}
	else {
		/* leaving pile */
		/* Restore size. If we're still at the size at the beginning
		 * of the pile, use the orginal value before the pile to avoid
		 * any possible roundoff errors going back and forth,
		 * otherwise adjust by inverse of the factor used when
		 * entering the pile. */
		if (size == Pile_info.new_size) {
			return(Pile_info.orig_size);
		}
		else {
			new_size = (int) ((double) size / Score.pilescale);
			if (new_size > MAXSIZE) {
				/* constrain to no larger than MAXSIZE */
				new_size = MAXSIZE;
			}
			return(new_size);
		}
	}
}


/* given where a pile begins and the font/size at that point, return the
 * width from the beginning to the pile to the alignment point of the pile. */

double
align_distance(string, font, size)

char *string;	/* pile starts here */
int font;
int size;

{
	struct PILE_INFO pile_info;

	pile_info.font = font;
	pile_info.size = size;
	(void) prep_pile(string, &pile_info);
	return(Pile_info.width_left_of_align);
}
