%{
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

/* troff to HTML converter.
 * Handles .H  .P   .AL   .DL   .BL   .LI   .LE   .bp   .br   .SK
 * font changes (limited), nroff comments,
 * conversion of  \(em  \(ga \(bu \&  \e  & < > 
 * Converts .TS - .TE tbl blocks to HTML tables,
 * but does only limited troff->HTML conversion of text inside table,
 * so be careful for now!!
 *
 * HTML marks can be put in input if they are at the beginning of a line,
 * and contain only upper case letters and /
 *
 * To have alternate blocks--one for troff output and one for HTML output--
 * put the troff stuff inside .Hi  -- .He  and put the HTML inside .ig  --  ..
 *
 * A   .Hr  reference     creates an HTML <A HREF> construct. The
 * reference is the URL and the next line of input is taken as the
 * link name to go before the </A>.
 * A   .Hm  name	creates an HTML <A NAME> construct. The name is
 * the anchor name and the next line of input is taken as the anchor text
 * A   .Hh name		create an HTML <A NAME> construct inside
 * the <Hx>..</Hx> created by the .H that is expected to be on the next line.
 *
 * .Hd  filename    causes output to be diverted to filename
 * .Ha  filename    is similar but opens the filename in append mode
 *
 * .Ht title		use title on next diversion
 * .Hn			don't put trailer on current diversion when closing
 * .Hi			ignore everything till a line containing only .He
 * .Nf			go into "no fill" mode (add <BR> at input newlines)
 *
 * Extension for Mup uguide: handes .Ex and .Ee by creating <PRE>
 * section for the text and a <IMG> for a .gif version of the PostScript file.
 * Replaces .pI with contents of param_index file
 * (This tool must be run on the output of ugexi)
 * Handles parameter macros: .bP  .Na  .De  .Va  .Df  .Cn  .eX  .eS  .Sa  .eP
 * as well as .Te and its variants.
 * Handles command line option formatting macros: .Co  .Mo  .Op
 * Builds an index from .Ix entries.
 *
 * Other troff macros are discarded.
 */

#include <string.h>
#include <unistd.h>
#include <ctype.h>

#ifndef NLSTATE
#define NLSTATE yy_set_bol(1)
#endif

struct Index_item {
	char *filename;		/* File the index entry should point to */
	char *tagname;		/* most recent #name inside the file, if any */
	char *title;		/* Title of the file */
	char *param;		/* parameter name, if a parameter ref */
	struct Index_item *next;	/* linked list */
};
/* Index items are identified by a troff string whose name begin consists
 * of a lower case letter and an upper case letter */
struct Index_item * Index[26][26];

#define FNT_ROM  0
#define FNT_ITAL 1
#define FNT_BOLD 2
#define FNT_BOLDITAL 3
#define FNT_CW   4

int nofill = 0;
int paragraph = 0;	/* non-zero if inside a paragraph */
int headlevel;		/* current heading level */
int i;
int font, prevfont;	/* current and previous font */
int list[32];		/* to track stack of lists, whether UL  or OL */
int lt;			/* index into list array */
char *mode;		/* "w" or "a" for fopen */
int trailer = 0;	/* non-zero if we are to put HTML trailer on file */
int doing_table_row;	/* true if inside a </TR> */

char *prevfile;		/* name of previous file, for adding link to it */
char *currfile;		/* name of the current file */
long nextlink_offset;	/* where to put "next page" link info */
int doing_name;		/* true if doing A NAME rather than A HREF */
int param_name;		/* true if looking for parameter name after a .Na */
char *curr_param;	/* parameter being described currently */
char * table_of_contents = "&nbsp;&nbsp;&nbsp;&nbsp;<A HREF=\"index.html\">Table of Contents</A>&nbsp;&nbsp;&nbsp;&nbsp;<A HREF=\"mupindex.html\">Index</A>";
char * next_page = "&nbsp;&nbsp;&nbsp;&nbsp;<A HREF=\"%s\">next page --&gt;</A>\n</P>\n";

char title[YYLMAX];	/* text for <TITLE> */
char *curr_title;	/* malloced copy of current title, for index */
char *index_filename;	/* malloced copy of current file, for index */
char *curr_tagname;	/* malloced copy of current #name, for index */
int do_headname;	/* If to add an A NAME to the next heading */
void ignore(void);	/* to throw away stuff between .Hi   and  .He */
int get_a_line(char *buff);
void add_param_index();
void chg2ital();
void chg2bold();
void chg2boldital();
void chg2rom();
void chg2cw();
void chg2prev();
void expand();
void index_reference(char *);
void index_entry();
%}

%option yylineno
%option array

%Start EXAMPLE EX1 HEAD HEAD1 REF REFTEXT TMAC DIVERT TITLE NL
%Start TABLETOP TABLEBODY TABLEBLOCK
%Start INDEX
%Start HEADREF

%%


^".Ex".*\n		{ printf("<BR><PRE>\n"); }

^".\\\"".*\n	;

^".Ee"" "*	{
			printf("</PRE><BR>\n");
			BEGIN EXAMPLE;
		}

<EXAMPLE>mugex.*\./ps	{
				printf("<IMG SRC=\"%sgif\" ALT=\"Picture of Mup output\"><BR>\n", yytext);
				BEGIN EX1;
			}

<EXAMPLE>\n		BEGIN 0;

<EX1>ps.*\n	BEGIN 0;

^".bP"		printf("<HR>");
^".Na"		{ printf("<H2>"); param_name = 1; curr_param = 0; }
^".De"		printf("</H2><P>");
^".Va"		printf("</P>\n<P><B>Value: </B>");
^".Df"		printf("</P>\n<P><B>Default value: </B>");
^".Cn"		printf("</P>\n<P><B>Context: </B>");
^".Te"		printf("</P>\n<P><B>Takes effect: </B>");
^".Nm"		printf("</P>\n<P><B>Takes effect: </B>at next music context");
^".Ni"		printf("</P>\n<P><B>Takes effect: </B>at next music context, or immediately if mid-measure");
^".Im"		printf("</P>\n<P><B>Takes effect: </B>immediately");
^".Ns"		printf("</P>\n<P><B>Takes effect: </B>at start of next score");
^".Pm"		printf("</P>\n<P><B>Takes effect: </B>immediately for purposes of checking for interactions with other parameters, at next music context for printing");
^".Ps"		printf("</P>\n<P><B>Takes effect: </B>immediately for purposes of checking for interactions with other parameters, at next score for printing");
^".Oo"		printf("</P>\n<P><B>Takes effect: </B>applies to entire input, and cannot be changed after music input");
^".Mn"		printf("</P>\n<P><B>Takes effect: </B>at next music context if &quot;every N&quot;, else start of next score");
^".Bw"		printf("</P>\n<P><B>Takes effect: </B>between the current score and the next score");
^".eX"		printf("</P>\n<P><B>Example:</B><BR>");
^".eS"		printf("</P>\n<P><B>Examples:</B><BR>");
^".Sa"		printf("</P>\n<P><B>Related parameters: </B>");
^".eP"		{ printf("</P>"); paragraph = param_name = 0; curr_param = 0; }
^".pI"		add_param_index();

^".Hr"" "*	{ printf("<A HREF=\""); BEGIN REF; doing_name = 0; }
^".Hm"" "*	{ printf("<A NAME=\""); BEGIN REF; doing_name = 1; }
^".Hh"" "*	{ BEGIN HEADREF; }
^".Nf"		{ nofill = 1; }

^".Co"		printf("<HR>\n<CODE>Command line:&nbsp;</CODE>");
^".Mo"		printf("<BR>\n<CODE>Mupmate:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</CODE>");
^".Op"		printf("<BR>&nbsp;&nbsp;<BR>\n");

^".Ix"[ ]*[a-z][A-Z]	index_reference(yytext + yyleng - 2);
^INDEX\n		BEGIN INDEX;
<INDEX>^.*\\\*"("[a-z][A-Z]	index_entry();
<INDEX>^".TC"		index_entry();
<INDEX>.		|
<INDEX>\n		;

<REF>.*/\n	{
			printf("%s\">", yytext);
			BEGIN REFTEXT;
			if (doing_name) {
				curr_tagname = strdup(yytext);
			}
		}

<REFTEXT>\n..*/\n	{
			i = ((yytext[1] == '\\' && yytext[2] == '&') ? 3 : 1);
			printf("%s</A>", yytext + i);
			if (param_name && doing_name) {
				curr_param = strdup(yytext + i);
			}
			BEGIN 0;
		}

<HEADREF>.*	{
			BEGIN 0;
			curr_tagname = strdup(yytext);
			do_headname = 1;
		}

^".Hd"" "*	{ mode = "w"; BEGIN DIVERT; }
^".Ha"" "*	{ mode = "a"; BEGIN DIVERT; }

^".Hn".*/\n	{ trailer = 0; }

<DIVERT>.*/\n	{
			if (trailer) {
				if (paragraph == 1) {
					printf("</P>\n");
					paragraph = 0;
				}
				printf("<HR><P>\n");
				if (prevfile != 0) {
					printf("&nbsp;&nbsp;&nbsp;<A HREF=\"%s\">&lt;-- previous page</A>", prevfile);
					free(prevfile);
				}
				if (currfile != 0 && strcmp(currfile, "index.html") != 0) {
					printf("%s", table_of_contents);
				}
				printf(next_page, yytext);
				prevfile = currfile;
				currfile = strdup(yytext);
				printf("</BODY></HTML>\n");
				if (nextlink_offset != 0) {
					fseek(yyout, nextlink_offset, SEEK_SET);
					printf(next_page, yytext);
				}
			}
			trailer = 1;
			if ((yyout = freopen(yytext, "w", yyout)) == (FILE *) 0) {
				fprintf(stderr, "failed to open %s\n", yytext);
				exit(1);
			}
			BEGIN NL;
			if (*mode == 'w') {
				printf ("<HTML>\n");
				if (title[0]) {
					printf("<HEAD><TITLE>\n%s\n</TITLE></HEAD>\n", title);
				}
				printf("<BODY>\n");
				nextlink_offset = 0;
				if (prevfile != 0) {
					int s;

					printf("<P>\n&nbsp;&nbsp;&nbsp;<A HREF=\"%s\">&lt;-- previous page</A>\n\n", prevfile);
					printf("%s", table_of_contents);
					/* Print enough blanks to allow for
					 * a "next page" link. We don't know
					 * what to put there yet, so will fill
					 * it in later. The last page will
					 * not have any next, so the blanks
					 * will remain. Any half-sane HTML
					 * renderer will ignore
					 * the extra white space, since that's
					 * what the standard says it should do.
					 */
					nextlink_offset = ftell(yyout);
					for (s = 0; s < strlen(next_page) + 20; s++) {
						putc(' ', yyout);
					}
				}
				paragraph = 0;
			}
			/* save info for use in index */
			curr_title = strdup(title);
			index_filename = strdup(yytext);
			curr_tagname = "";
			title[0] = '\0';
			BEGIN 0;
		}

^".Ht"" "*	BEGIN TITLE;
<TITLE>.*/\n	{
			/* remove any quotes from the title */
			if (yytext[0] == '"') {
				strcpy(title, yytext + 1);
				if (yytext[yyleng-1] == '"') {
					title[yyleng-2] = '\0';
				}
			}
			else {
				strcpy(title, yytext);
			}
			BEGIN NL;
		}


<NL>\n		BEGIN 0;

^".Hi"		{ ignore(); NLSTATE; }

^".P"\n		{
			if (paragraph == 1) {
				printf("</P>\n");
			}
			printf("<P>\n");
			paragraph = 1;
		}

^".H"		{
			if (paragraph == 1) {
				printf("</P>\n");
			}
			paragraph = 0;
			BEGIN HEAD;
		}

<HEAD>" "*[0-7]" "*		{
			headlevel = atoi(yytext);
			printf("<H%d>\n", headlevel);
			BEGIN HEAD1;
		}

<HEAD1>.*\n	{
			if (do_headname && curr_tagname != 0 && curr_tagname[0] != '\0') {
				printf("<A NAME=\"%s\">", curr_tagname);
			}
			for (i = 0; i < yyleng; i++) {
				if (yytext[i] == '"' && yytext[i-1] != '\\') {
					continue;
				}
				else {
					putchar(yytext[i]);
				}
			}
			if (do_headname && curr_tagname != 0 && curr_tagname[0] != '\0') {
				printf("</A>");
			}
			do_headname = 0;
			printf("</H%d>\n", headlevel);
			BEGIN 0;
		}

^".TS"/\n		{ BEGIN TABLETOP; }
<TABLETOP>^.*;		;
<TABLETOP>^[|lcs ]*	;
<TABLETOP>\n		;
<TABLETOP>^.*"."/\n	{
				printf("<TABLE BORDER=4>\n");
				doing_table_row = 0;
				BEGIN TABLEBODY;
			}
<TABLEBODY>^"_"/\n	;
<TABLEBODY>^".ig"/\n	;
<TABLEBODY>^".."/\n	;
<TABLEBODY>^".Ix".*/\n	 index_reference(yytext + yyleng - 2);
<TABLEBODY>[^\t\n]*	{
				if ( ! doing_table_row) {
					printf("<TR>\n");
					doing_table_row = 1;
				}
				printf("<TD>");
				if (yytext[0] == 'T' && yytext[1] == '{') {
					BEGIN TABLEBLOCK;
				}
				else {
					expand(yytext);
					printf("</TD>");
				}
			}
<TABLEBODY>\t\t		{ printf("<TD></TD>"); yyless(1); }
<TABLEBLOCK>^"T}"	{ printf("</TD>"); BEGIN TABLEBODY; }
<TABLEBLOCK>[^\t\n]*	{ expand(yytext); }
<TABLEBODY>\n		{
				if (doing_table_row) {
					printf("\n</TR>\n");
					doing_table_row = 0;
				}
			}
<TABLEBODY>^".TE"/\n	{
				BEGIN 0;
				printf("</TABLE>\n");
			}

\\"(em"		printf("--");

\\"(ga"		printf("`");

\\&		;

\\e		printf("\\");

"&"		printf("&amp;");
"<"		printf("&lt;");
">"		printf("&gt;");
"\""		printf("&quot;");
"&#"[0-9]+";"	ECHO;	/* if &#nn; don't translate the &, use as is */

^"<"[A-Z/]+[^>]*">"	ECHO;

^".I"\n		|
^".ft"" "*I\n	|
\\fI		{ chg2ital(); }

^".B"\n		|
^".ft"" "*B\n	|
\\fB		{ chg2bold(); }

^".BI"\n		|
^".ft"" "*BI\n	|
\\"f(BI"	{ chg2boldital(); }

\\f"(NX"	{ chg2boldital(); }
\\f"(NR"	{ chg2rom(); }

^".ft"" "*CW\n	|
\\f"(CW"	{ chg2cw(); }

^".R"\n		|
^".ft"" "*R\n	|
\\fR		{
			chg2rom();
		}

^".ft"" "*P	|
\\fP		{
			chg2prev();
		}

^".br"\n	{ printf("<BR>\n"); }

^".bp"\n	|
^".SK"\n	{ printf("<HR>\n"); }

^".AL".*\n	{ printf("<OL>\n"); list[lt++] = 0; }

^".BL".*\n	|
^".DL".*\n	{ printf("<UL>\n"); list[lt++] = 1; }

^".LI".*\n	{ printf("<LI>\n"); }

^".LE".*\n	{
			if (lt == 0) {
				fprintf(stderr, "LE with no begin: line %d\n", yylineno);
			}
			else {
				switch(list[--lt]) {
				case 0:
					printf("</OL>\n");
					break;
				case 1:
					printf("</UL>\n");
					break;
				}
			}
		}

^\.		BEGIN TMAC;

<TMAC>.*\n	BEGIN 0;

\n		{ if (nofill) printf("\n<BR>"); ECHO; }

%%

int
main()
{
	yyin = stdin;
	yyout = stdout;
	yylex();

	/* clean up */
	if (paragraph == 1) {
		printf("</P>\n");
	}
	if (trailer) {
		if (prevfile != 0) {
			printf("<HR>\n<P>\n&nbsp;&nbsp;&nbsp;<A HREF=\"%s\">&lt;-- previous page</A>\n", prevfile);
			printf("%s", table_of_contents);
			printf("</P>\n");
		}
		printf("</BODY></HTML>\n");
	}
	exit(0);
}


/* read and discard lines until line containing .He */


void
ignore()
{
	char buff[YYLMAX];
	int processIx = 1;


	while (get_a_line(buff)) {
		if (strcmp(buff, ".He\n") == 0) {
			return;
		}
		/* Still need to process index references, unless
		 * we see special flag to not do so */
		if (strncmp(buff, ".Nx", 3) == 0) {
			processIx = 0;	
		}

		if (processIx && strncmp(buff, ".Ix", 3) == 0) {
			buff[strlen(buff)-1] = '\0';

			index_reference(buff + strlen(buff) - 2);
		}
	}
}

/* read a line of input. Return 0 on EOF, else 1 */

int
get_a_line(char *buff)
{
	int i;

	for (i = 0; i < YYLMAX; i++) {
		if ((buff[i] = input()) == '\n') {
			buff[++i] = '\0';
			return(1);
		}
		if (buff[i] == 0) {
			return(0);
		}
	}
	return(1);
}

/* write contents of param_index to output */

void
add_param_index()
{
	FILE *file;
	char *param_file_name = "param_index";
	char buff[BUFSIZ];

	if ((file = fopen(param_file_name, "r")) == 0) {
		fprintf(stderr, "warning: can't open %s file\n", param_file_name);
		return;
	}
	while (fgets(buff, sizeof(buff), file)) {
		printf("%s", buff);
	}
	fclose(file);
}

void
endfont()
{
	if (font == FNT_ITAL)
		printf("</I>");
	else if (font == FNT_BOLD)
		printf("</B>");
	else if (font == FNT_CW)
		printf("</TT>");
	else if (font == FNT_BOLDITAL)
		printf("</I></B>");
}

void
chg2ital()
{
	endfont();
	printf("<I>");
	prevfont = font;
	font = FNT_ITAL;
}


void
chg2bold()
{
	endfont();
	printf("<B>");
	prevfont = font;
	font = FNT_BOLD;
}

void
chg2boldital()
{
	endfont();
	printf("<B><I>");
	prevfont = font;
	font = FNT_BOLDITAL;
}

void
chg2cw()
{
	endfont();
	printf("<TT>");
	prevfont = font;
	font = FNT_CW;
}

void
chg2rom()
{
	prevfont = font;
	endfont();
	font = FNT_ROM;
}

void
chg2prev()
{
	endfont();
	font = prevfont;
}


void
expand(char *text)
{
	for (  ; *text != '\0'; text++) {
		if (*text == '\\' && *(text+1) == 'f') {
			switch( *(text+2) ) {
			case 'B':
				chg2bold();
				break;
			case 'I':
				chg2ital();
				break;
			case 'R':
				chg2rom();
				break;
			case 'P':
				chg2prev();
				break;
			case '(':
				if ( *(text+3) == 'C' && *(text+4) == 'W') {
					chg2cw();
				}
				else if ( *(text+3) == 'N' && *(text+4) == 'X') {
					chg2bold();
				}
				else if ( *(text+3) == 'N' && *(text+4) == 'R') {
					chg2rom();
				}
				else if ( *(text+3) == 'B' && *(text+4) == 'I') {
					chg2boldital();
				}
				else {
					fprintf(stderr, "line %d: unknown 2-letter font name %c%c\n", yylineno, text[3], text[4]);
				}
				text += 2;
				break;
			default:
				fprintf(stderr, "unknown font change\n");
			}
			text += 2;
		}
		else if (*text == '\\' && *(text+1) == 's') {
			/* ignore size changes */
			text += 3;
		}
		else if (*text == '\\' && *(text+1) == '&') {
			text++;
			continue;
		}
		else if (*text == '<') {
			printf("&lt;");
		}
		else if (*text == '>') {
			printf("&gt;");
		}
		else if (*text == '&') {
			printf("&amp;");
		}
		else if (*text == '"') {
			printf("&quot;");
		}
		else if (strncmp(text, "\\(bu", 4) == 0) {
			printf("X");
			text += 3;
		}
		else {
			printf("%c", *text);
		}
	}
}

/* Handle a .Ix macro that generates a reference for the index */

void
index_reference(char * ref)
{
	int i1, i2;			/* indexes into Index[][] */
	struct Index_item * entry_p;	/* new entry to save */
	struct Index_item ** last_p_p;	/* last entry currently on list
					 * for the index to be added to */


	i1 = ref[0] - 'a';
	i2 = ref[1] - 'A';

	for (last_p_p = &(Index[i1][i2]); *last_p_p != 0;
				last_p_p = &((*last_p_p)->next)) {
		if (strcmp(currfile, (*last_p_p)->filename) == 0 &&
				strcmp(curr_tagname, (*last_p_p)->tagname) == 0) {
			/* This file#name already referenced for the index item */
			return;
		}
	}
	entry_p = (struct Index_item *) malloc(sizeof(struct Index_item));
	*last_p_p = entry_p;
	entry_p->filename = index_filename;
	entry_p->title = curr_title;
	if (param_name && curr_param != 0
				&& strcmp(index_filename, "param.html") == 0) {
		entry_p->param = curr_param;
	}
	else {
		entry_p->param = 0;
	}
	entry_p->tagname = curr_tagname;
	entry_p->next = 0;
}


/* Generate an index entry */

void
index_entry()
{
	static FILE * indexfile = 0;
	static int letter;	/* for index of the index */
	int i1, i2;		/* indexes into Index[][] */
	char * text;		/*  What the user sees as index topic */
	struct Index_item * entry_p;	/* an index item */

	if (indexfile == 0) {
		indexfile = fopen("mupindex.html", "w");
		(void) fprintf(indexfile, "<HTML><HEAD><TITLE>Mup User's Guide Index</TITLE></HEAD>\n");
		(void) fprintf(indexfile, "<BODY><H1>Mup User's Guide Index</H1>\n");
		(void) fprintf(indexfile, "<P><A HREF=\"index.html\">Table of Contents</A></P>\n");
		(void) fprintf(indexfile, "<P>\n");
		/* Mup currently does not have entries for Y or Z */
		for (letter = (int) 'A'; letter <= (int) 'X'; letter++) {
			(void) fprintf(indexfile, "<A HREF=\"#%c\">%c</A>\n",
							letter, letter);
		}
		/* Initialize which index letter we are on */
		letter = 'A' - 1;
	}

	/* The .TC entry ends the index */
	if (strncmp(yytext, ".TC", 3) == 0) {
		(void) fprintf(indexfile, "</UL></P>\n<P><A HREF=\"index.html\">Table of Contents</A></P>\n</BODY></HTML>\n");
		(void) fclose(indexfile);
		return;
	}

	/* This somewhat depends on the fact that we have no gaps with
	 * letters with no index value. This is currently true of the Mup
	 * index: it has all but y and z. There are a few entries in the
	 * index that start with _ for the special tags, so skip those.
	 */
	if (isalpha(yytext[0]) && letter != toupper(yytext[0])) {
		if (letter >= (int) 'A') {
			(void) fprintf(indexfile, "</UL>\n");
		}
		letter = toupper(yytext[0]);
		(void) fprintf(indexfile, "</P><P><A NAME=\"%c\"><B>%c</B></A><UL>\n",
						letter, letter);
	}

	/* Separate the index entry text from the troff reference string.
	 * Each entry is in the form:
	 *	something of interest^A\*(xX
	 * where ^A means control-A, and xX is a troff string name.
	 */
	text = strtok(yytext, "\01");
	i1 = yytext[yyleng-2] - 'a';
	i2 = yytext[yyleng-1] - 'A';

	if (Index[i1][i2] == 0) {
		/* No actual entries. May be a few that don't appear in
		 * the HTML version. */
		/* There is one expected thing missing--the reference
		 * to Arkkra in the Conclusion, which we don't put in
		 * the html version currently. Otherwise, print warning. */
		if ((i1 + 'a') != 'b' || (i2 + 'A') != 'M') {
			fprintf(stderr, "No entry for index %c%c\n", i1 + 'a', i2 + 'A');
		}
		return;
	}

	(void) fprintf(indexfile, "<LI>%s<BR>\n<UL>", text);
	for (entry_p = Index[i1][i2]; entry_p != 0; entry_p = entry_p->next) {
		(void) fprintf(indexfile, "<LI><A HREF=\"%s", entry_p->filename);
		if (entry_p->tagname[0] != '\0') {
			(void) fprintf(indexfile, "#%s", entry_p->tagname);
		}
		if (entry_p->param != 0) {
			(void) fprintf(indexfile, "\">%s parameter</A>\n", entry_p->param);
		}
		else {
			(void) fprintf(indexfile, "\">%s</A>\n", entry_p->title);
		}
	} 
	(void) fprintf(indexfile, "</UL>\n");
}

int
yywrap()
{
	return(1);
}
