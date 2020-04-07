
%{

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

/* This file contains the lexer for the Mup music publication program.
 * It recognizes and returns all the tokens needed by the yacc parser. */

#include <errno.h>
#include "rational.h"
#include "ytab.h"
#include "defines.h"
#include "structs.h"
#include "globals.h"

/* "If Types" -- the different kinds of statements
 * that are ended with an endif */
#define IT_IFDEF	(0)
#define IT_IFNDEF	(1)
#define IT_IF		(2)


/* In order to reduce the number of terminal symbols so yacc wouldn't blow
 * up, we mapped several similar tokens to the same return code. The
 * yylval variable is then used to say which specific one of the set
 * was actually in the input. Bison doesn't have the number of tokens
 * restriction, but grouping related tokens is still useful. */
extern YYSTYPE yylval;


#define YY_INPUT(buf,result,max_size) \
	{ \
		int c = getc(yyin); \
		yylineno += Lineno_increment; \
		if (c == '\n') { \
			Lineno_increment = not_in_mac(1); \
		} \
		else { \
			Lineno_increment = 0; \
		} \
		if (c == EOF || c == 0) { \
			 result = YY_NULL; \
		} else { \
			buf[0] = c; \
			result = 1; \
		} \
	} \

#define YY_USER_ACTION \
	if (yydebug) { \
		 yydebugtoken(); \
	 }
/* list of Flex buffers for includes/macros */
struct Lexbuffer {
	YY_BUFFER_STATE	buff;
	struct Lexbuffer *next;
};
static struct Lexbuffer *Lexlist;

/* This struct is used to keep a stack of the line numbers of
 * in-progress if/ifdef/ifndef statements, for better error messages,
 * if there is no matching endif. */
struct IF_LINE {
	int linenum;		/* line where the if* began */
	char *inputfile;	/* which file it was in */
	int if_type;		/* IT_* value */
	struct IF_LINE *prev;	/* for stack */
};
static struct IF_LINE *If_linenum_stack;

/* We add a newline at end of input, to avoid cryptic error message
 * if user didn't end the file with a newline. */
static YY_BUFFER_STATE Eof_newline = 0;

/* When doing expressions for coordinates, we return special tokens for
 * arithmetic operators */
static int Expr_mode = NO;

/* The compressed form of an "if" clause is stored here,
 * then passed to a mini-parser. */
unsigned char Ifclause_buff[1024];
int If_length;			/* how much of Ifclause_buffer is used */
static void add_ifclause P((int token));
static void addnum_ifclause P((int value));
static void addnumstr_ifclause P((char *numtoken));
extern void set_ifclause_buffer P((unsigned char * buffer, int length));
extern int ifparse P((void));
static void bad_input P((char *where));
static struct Lexbuffer *add_lexbuff P((void));

int yylineno;


static int If_count = 0;	/* how many if, ifdef, and ifndef constructs in progress */
static int If_errs;		/* to try to deduce missing 'then' */
static int Escapedquotes = 0;
static int Raw_string = NO;	/* YES if doing raw string (not expanding
				 * any non-ASCII into Mup escapes) */

static int get_a_param P((char *macname));
static void skipwhite P((void));
static void to_eol P((void));
static int skip2else P((void));
static int oddbs P((void));
static void embedquotes P((void));
static void xpand_non_ascii P((void));
static void ignore P((void));
static void yydebugtoken P((void));
static void push_if_line P((int if_type));
static void pop_if_line P((void));
static char *if_type_name P((int if_type));
%}


%array

%x IF_CLAUSE STRING

%%

rom		{ yylval.intval = FONT_TR; return(T_FONT); }
bold		{ yylval.intval = FONT_TB; return(T_FONT); }
boldital	{ yylval.intval = FONT_TX; return(T_FONT); }
ital		{ yylval.intval = FONT_TI; return(T_FONT); }

times		{ yylval.intval = BASE_TIMES; return(T_FFAMILY); }
avantgarde	{ yylval.intval = BASE_AVANTGARDE; return(T_FFAMILY); }
courier		{ yylval.intval = BASE_COURIER; return(T_FFAMILY); }
helvetica	{ yylval.intval = BASE_HELVETICA; return(T_FFAMILY); }
bookman		{ yylval.intval = BASE_BOOKMAN; return(T_FFAMILY); }
newcentury	{ yylval.intval = BASE_NEWCENTURY; return(T_FFAMILY); }
palatino	{ yylval.intval = BASE_PALATINO; return(T_FFAMILY); }

include[ \t]+\"[^"\r\n]+\"	{
			/* set up include using filename with quotes stripped */
			yytext[yyleng - 1] = '\0';
			includefile(strchr(yytext, '"') + 1);
		}

file		{ return(T_FILE); }
fontfile[ \t]+\"[^"\r\n]+\"	{
			yytext[yyleng - 1] = '\0';
			parse_font_file(strchr(yytext, '"') + 1);
		}

dim		|
diminished	{ yylval.intval = DIMINISHED; return(T_XPOS_INT); }
aug		|
augmented	{ yylval.intval = AUGMENTED; return(T_XPOS_INT); }
per		|
perfect		{ yylval.intval = PERFECT; return(T_XPOS_INT); }
min		|
minor		{ yylval.intval = MINOR; return(T_XPOS_INT); }
maj		|
major		{ yylval.intval = MAJOR; return(T_XPOS_INT); }

title		return(T_TITLE);

score		return(T_SCORE);
staff		return(T_STAFF);
voice		return(T_VOICE);
header		{ yylval.intval = C_HEADER; return(T_BLOCKHEAD); }
footer		{ yylval.intval = C_FOOTER; return(T_BLOCKHEAD); }
header2		{ yylval.intval = C_HEAD2; return(T_BLOCKHEAD); }
footer2		{ yylval.intval = C_FOOT2; return(T_BLOCKHEAD); }
top		{ yylval.intval = C_TOP; return(T_BLOCKHEAD); }
bottom		{ yylval.intval = C_BOT; return(T_BLOCKHEAD); }
top2		{ yylval.intval = C_TOP2; return(T_BLOCKHEAD); }
bottom2		{ yylval.intval = C_BOT2; return(T_BLOCKHEAD); }
block		{ yylval.intval = C_BLOCK; return(T_BLOCKHEAD); }
grids		return(T_GRIDS);
music		return(T_MUSIC);
headshapes	return(T_HEADSHAPES);
symbol		return(T_SYMBOL);
cents		return(T_CENTS);
accidentals	return(T_ACCIDENTALS);
control		return(T_CONTROL);
saveparms	return(T_SAVEPARMS);
restoreparms	return(T_RESTOREPARMS);

afterprolog	{ yylval.intval = PU_AFTERPROLOG; return(T_PSHOOKLOC); }
beforetrailer	{ yylval.intval = PU_BEFORETRAILER; return(T_PSHOOKLOC); }
atpagebegin	{ yylval.intval = PU_ATPAGEBEGIN; return(T_PSHOOKLOC); }
atpageend	{ yylval.intval = PU_ATPAGEEND; return(T_PSHOOKLOC); }
atscorebegin	{ yylval.intval = PU_ATSCOREBEGIN; return(T_PSHOOKLOC); }
atscoreend	{ yylval.intval = PU_ATSCOREEND; return(T_PSHOOKLOC); }

[0-9]+		return(T_NUMBER);

=		return(T_EQUAL);
;		return(T_SEMICOLON);
\r		|
\r\n		|
\n		return(T_NEWLINE);
:		return(T_COLON);
","		return(T_COMMA);
"["		return(T_LBRACKET);
"]"		return(T_RBRACKET);
"{"		return(T_LBRACE);
"}"		return(T_RBRACE);
"("		return(T_LPAREN);
")"		return(T_RPAREN);
"!"		return(T_EXCLAM);

\"		{ BEGIN STRING; yymore(); }
<STRING>[^"]*\"	{
			/* string: handle embedded backslashed quotes */
			if (yytext[yyleng-2] == '\\' && oddbs() == YES) {
				Escapedquotes++;
				yymore();
			}
			else {
				embedquotes();
				BEGIN 0;
				xpand_non_ascii();
				if (yyleng > YYLMAX) {
					l_ufatal(Curr_filename, yylineno,
							"text string too long");
				}
				return(T_STRING);
			}
		}

"-"		{
			if (Expr_mode == YES) {
				yylval.intval = OP_SUB;
				return(T_ADDSUB_OP);
			}
			else {
				return(T_DASH);
			}
		}
"+"		{
			if (Expr_mode == YES) {
				yylval.intval = OP_ADD;
				return(T_ADDSUB_OP);
			}
			else {
				return(T_PLUS);
			}
		}
"%"		{ yylval.intval = OP_MOD; return(T_MULDIV_OP); }

cue		{ yylval.intval= CUE; return(T_CUE); }
grace		return(T_GRACE);
xnote		return(T_XNOTE);
diam		return(T_DIAM);
ho		return(T_HO);
hs		return(T_HS);
tie			{ yylval.intval = L_NORMAL; return(T_TIE); }
dotted[ \t]*tie		{ yylval.intval = L_DOTTED; return(T_TIE); }
dashed[ \t]*tie		{ yylval.intval = L_DASHED; return(T_TIE); }
slur			{ yylval.intval = L_NORMAL; return(T_SLUR); }
dotted[ \t]*slur	{ yylval.intval = L_DOTTED; return(T_SLUR); }
dashed[ \t]*slur	{ yylval.intval = L_DASHED; return(T_SLUR); }
with		return(T_WITH);
above		{ yylval.intval = PL_ABOVE; return(T_PLACE); }
below		{ yylval.intval = PL_BELOW; return(T_PLACE); }
between		{ yylval.intval = PL_BETWEEN; return(T_PLACE); }
all		return(T_ALL);
up		return(T_UP);
down		return(T_DOWN);
any		return(T_ANY);
len		return(T_LEN);
slash		return(T_SLASHMARK);
alt		return(T_ALT);
ph		return(T_PH);
eph		return(T_EPH);
bm		return(T_BM);
esbm		return(T_ESBM);
ebm		return(T_EBM);
abm		return(T_ABM);
eabm		return(T_EABM);
slope		return(T_SLOPE);
pad		{ yylval.intval = PAD; return(T_PAD); }

"^"		return(T_HAT);
"~"			{ yylval.intval = L_NORMAL; return(T_TILDE); }
dotted[ \t]*"~"		{ yylval.intval = L_DOTTED; return(T_TILDE); }
dashed[ \t]*"~"		{ yylval.intval = L_DASHED; return(T_TILDE); }
"?"		return(T_QUESTION);

"."		return(T_DOT);
1" "*"/"" "*2		{ yylval.intval = 2; return(T_MULTIWHOLE); }
1" "*"/"" "*4		{ yylval.intval = 4; return(T_MULTIWHOLE); }
1" "*"/"" "*8		{ yylval.intval = 8; return(T_MULTIWHOLE); }

m		return(T_LET_M);
r		return(T_LET_R);
s		return(T_LET_S);
u		return(T_LET_U);
[a-g]		return(T_PITCH);
"#"		return(T_SHARP);
"&"		return(T_AMPERSAND);
n		return(T_LET_N);
x		return(T_LET_X);
[h-lopqtvwyz]	return(T_LETTER);	/* all letters not used for something special */
rpt		return(T_RPT);
"_"[A-Za-z0-9_]+	{
			if (strcmp(yytext, "_staff") == 0) {
				return(T_ARRAYVAR);
			}
			else {
				/* multi-character location variable names */
				return(T_LVAR);
			}
		}
"&&"		return(T_DBLFLAT);

lyrics		return(T_LYRICS);
using		return(T_USING);

newscore	{ yylval.intval = NO; return(T_SCOREFEED); }
newpage		{ yylval.intval = YES; return(T_SCOREFEED); }
auto		{ return(T_AUTO); }

multirest	return(T_MULTIREST);

bar		{ yylval.intval = SINGLEBAR; return(T_BARTYPE); }
dblbar		{ yylval.intval = DOUBLEBAR; return(T_BARTYPE); }
repeatstart	{ yylval.intval = REPEATSTART; return(T_BARTYPE); }
repeatend	{ yylval.intval = REPEATEND; return(T_BARTYPE); }
repeatboth	{ yylval.intval = REPEATBOTH; return(T_BARTYPE); }
invisbar	{ yylval.intval = INVISBAR; return(T_BARTYPE); }
endbar		{ yylval.intval = ENDBAR; return(T_BARTYPE); }
restart		{ yylval.intval = RESTART; return(T_BARTYPE); }

reh		|
rehearsal	return(T_REHEARSAL);
num		return(T_NUM);
let		return(T_LET);
mnum		return(T_MNUM);

ending		return(T_ENDING);
endending	{ yylval.intval = EE_DEFAULT; return(T_ENDENDING); }
openendending	{ yylval.intval = EE_OPEN; return(T_ENDENDING); }
closedendending	{ yylval.intval = EE_CLOSED; return(T_ENDENDING); }

hidechanges	return(T_HIDECHANGES);

wide		{ yylval.intval = L_WIDE; return(T_LINETYPE); }
medium		{ yylval.intval = L_MEDIUM; return(T_LINETYPE); }
wavy		{ yylval.intval = L_WAVY; return(T_LINETYPE); }
dotted		{ yylval.intval = L_DOTTED; return(T_LINETYPE); }
dashed		{ yylval.intval = L_DASHED; return(T_LINETYPE); }

line		return(T_LINE);
to		return(T_TO);
curve		return(T_CURVE);
bulge		return(T_BULGE);

left		{ yylval.intval = J_LEFT; return(T_JUSTIFYTYPE); }
right		{ yylval.intval = J_RIGHT; return(T_JUSTIFYTYPE); }
center		{ yylval.intval = J_CENTER; return(T_JUSTIFYTYPE); }
print		{ yylval.intval = J_NONE; return(T_PRINTTYPE); }
postscript	{ return(T_POSTSCRIPT); }
ragged		{ yylval.intval = J_RAGPARA; return(T_PARATYPE); }
justified	{ yylval.intval = J_JUSTPARA; return(T_PARATYPE); }
paragraph	return(T_PARAGRAPH);
nl		return(T_NL); 


sin		{ yylval.intval = OP_SIN; return(T_1ARG_FUNC); }
cos		{ yylval.intval = OP_COS; return(T_1ARG_FUNC); }
tan		{ yylval.intval = OP_TAN; return(T_1ARG_FUNC); }
asin		{ yylval.intval = OP_ASIN; return(T_1ARG_FUNC); }
acos		{ yylval.intval = OP_ACOS; return(T_1ARG_FUNC); }
atan		{ yylval.intval = OP_ATAN; return(T_1ARG_FUNC); }
sqrt		{ yylval.intval = OP_SQRT; return(T_1ARG_FUNC); }
atan2		{ yylval.intval = OP_ATAN2; return(T_2ARG_FUNC); }
hypot		{ yylval.intval = OP_HYPOT; return(T_2ARG_FUNC); }

"/"		{
			if (Expr_mode == YES) {
				yylval.intval = OP_DIV;
				return(T_MULDIV_OP);
			}
			else {
				return(T_SLASH);
			}
		}
cut		return(T_CUT);
common		return(T_COMMON);

staffs		{ yylval.intval = NUMSTAFF; return(T_NUMVAR); }
vscheme		{ yylval.intval = VSCHEME; return(T_VVAR); }
vcombine	{ yylval.intval = VCOMBINE; return(T_VCOMBINE); }
nooverlap	{ yylval.intval = VC_NOOVERLAP; return(T_VCOMBVAL); }
shareone	{ yylval.intval = VC_SHAREONE; return(T_VCOMBVAL); }
overlap		{ yylval.intval = VC_OVERLAP; return(T_VCOMBVAL); }
restsonly	{ yylval.intval = VC_RESTSONLY; return(T_VCOMBVAL); }
stepsapart	{ yylval.intval = VC_STEPSAPART; return(T_VCOMBVAL); }
defoct		{ yylval.intval = DEFOCT; return(T_NUMVAR); }
lyricssize	{ yylval.intval = LYRICSSIZE; return(T_NUMVAR); }
sylposition	{ yylval.intval = SYLPOSITION; return(T_NUMVAR); }
size		{ yylval.intval = SIZE; return(T_NUMVAR); }
staffsep	{ yylval.intval = MINSTSEP; return(T_FNUMVAR); }
scorepad	{ yylval.intval = MINSCPAD; return(T_SCOREPAD); }
staffpad	{ yylval.intval = STAFFPAD; return(T_FNUMVAR); }
chorddist	{ yylval.intval = CHORDDIST; return(T_FNUMVAR); }
dist		{ yylval.intval = DIST; return(T_FNUMVAR); }
dyndist		{ yylval.intval = DYNDIST; return(T_FNUMVAR); }
lyricsdist	{ yylval.intval = LYRICSDIST; return(T_FNUMVAR); }
align		{ return(T_ALIGN); }
division	{ yylval.intval = DIVISION; return(T_NUMVAR); }
release		{ yylval.intval = RELEASE; return(T_NUMVAR); }
panelsperpage	{ yylval.intval = PANELSPERPAGE; return(T_NUMVAR); }
maxscores	{ yylval.intval = MAXSCORES; return(T_NUMVAR); }
maxmeasures	{ yylval.intval = MAXMEASURES; return(T_NUMVAR); }
gridfret	{ yylval.intval = GRIDFRET; return(T_NUMVAR); }
mingridheight	{ yylval.intval = MINGRIDHEIGHT; return(T_NUMVAR); }
restcombine	{ yylval.intval = RESTCOMBINE; return(T_NUMVAR); }
firstpage	{ yylval.intval = FIRSTPAGE; return(T_FIRSTPAGE); }
leftpage	{ yylval.intval = PGSIDE_LEFT; return(T_PAGESIDE); }
rightpage	{ yylval.intval = PGSIDE_RIGHT; return(T_PAGESIDE); }
flipmargins	{ yylval.intval = FLIPMARGINS; return(T_YESNOVAR); }
scoresep	{ yylval.intval = MINSCSEP; return(T_SCORESEP); }
stafflines	{ yylval.intval = STAFFLINES; return(T_STAFFLINES); }
ontheline	{ yylval.intval = ONTHELINE; return(T_YESNOVAR); }
indentrestart	{ yylval.intval = INDENTRESTART; return(T_YESNOVAR); }
warn		{ yylval.intval = WARN; return(T_YESNOVAR); }
numbermrpt	{ yylval.intval = NUMBERMRPT; return(T_YESNOVAR); }
bracketrepeats	{ yylval.intval = BRACKETREPEATS; return(T_YESNOVAR); }
repeatdots	{ yylval.intval = REPEATDOTS; return(T_REPEATDOTS); }
standard	{ return(T_STANDARD); }
alignrests	{ yylval.intval = ALIGNRESTS; return(T_YESNOVAR); }
alignped	{ yylval.intval = ALIGNPED; return(T_YESNOVAR); }
printmultnum	{ yylval.intval = PRINTMULTNUM; return(T_YESNOVAR); }
restsymmult	{ yylval.intval = RESTSYMMULT; return(T_YESNOVAR); }
gridswhereused	{ yylval.intval = GRIDSWHEREUSED; return(T_YESNOVAR); }
gridsatend	{ yylval.intval = GRIDSATEND; return(T_YESNOVAR); }
tab		return(T_TAB);
tabwhitebox	{ yylval.intval = TABWHITEBOX; return(T_YESNOVAR); }
timeunit	{ yylval.intval = TIMEUNIT; return(T_TIMEUNIT); }
swingunit	{ yylval.intval = SWINGUNIT; return(T_SWINGUNIT); }
a4freq		{ yylval.intval = A4FREQ; return(T_FNUMVAR); }
topmargin	{ yylval.intval = TOPMARGIN; return(T_FNUMVAR); }
botmargin	{ yylval.intval = BOTMARGIN; return(T_FNUMVAR); }
bottommargin	{ yylval.intval = BOTMARGIN; return(T_FNUMVAR); }
leftmargin	{ yylval.intval = LEFTMARGIN; return(T_FNUMVAR); }
rightmargin	{ yylval.intval = RIGHTMARGIN; return(T_FNUMVAR); }
packfact	{ yylval.intval = PACKFACT; return(T_FNUMVAR); }
packexp		{ yylval.intval = PACKEXP; return(T_FNUMVAR); }
staffscale	{ yylval.intval = STAFFSCALE; return(T_FNUMVAR); }
gridscale	{ yylval.intval = GRIDSCALE; return(T_FNUMVAR); }
minalignscale	{ yylval.intval = MINALIGNSCALE; return(T_FNUMVAR); }
scale		{ yylval.intval = SCALE_FACTOR; return(T_FNUMVAR); }
stemlen		{ yylval.intval = STEMLEN; return(T_FNUMVAR); }
stemshorten	{ yylval.intval = BEAMSHORT; return(T_SHORTEN); }
leftspace	{ yylval.intval = LEFTSPACE; return(T_2FNUMVAR); }
noteinputdir	{ yylval.intval = NOTEINPUTDIR; return(T_INPUTDIR); }
beamslope	{ yylval.intval = BEAMSLOPE; return(T_2FNUMVAR); }
transpose	{ yylval.intval = TRANSPOSITION; return(T_TRANSPOSE); }
addtranspose	{ yylval.intval = ADDTRANSPOSITION; return(T_TRANSPOSE); }
chordtranslation { yylval.intval = CHORDTRANSLATION ; return(T_CHORDTRANSLATION); }
alignlabels	{ yylval.intval = ALIGNLABELS; return(T_ALIGNLABELS); }
notes		{ yylval.intval = TR_NOTES; return(T_TRANSUSAGE); }
chords		{ yylval.intval = TR_CHORDS; return(T_TRANSUSAGE); }
lyricsalign	{ yylval.intval = LYRICSALIGN; return(T_FNUMVAR); }
extendlyrics	{ yylval.intval = EXTENDLYRICS; return(T_YESNOVAR); }
pageheight	{ yylval.intval = PAGEHEIGHT; return(T_FNUMVAR); }
pagewidth	{ yylval.intval = PAGEWIDTH; return(T_FNUMVAR); }
pagesize	{ return(T_PSVAR); }
letter		{ yylval.intval = PS_LETTER; return(T_PAGESIZE); }
legal		{ yylval.intval = PS_LEGAL; return(T_PAGESIZE); }
flsa		{ yylval.intval = PS_FLSA; return(T_PAGESIZE); }
halfletter	{ yylval.intval = PS_HALFLETTER; return(T_PAGESIZE); }
portrait	{ yylval.intval = O_PORTRAIT; return(T_ORIENTATION); }
landscape	{ yylval.intval = O_LANDSCAPE; return(T_ORIENTATION); }
units		{ yylval.intval = UNITS; return(T_UNITS); }
cm		{ yylval.intval = CM; return(T_UNITTYPE); }
inches		{ yylval.intval = INCHES; return(T_UNITTYPE); }

endingstyle	{ yylval.intval = ENDINGSTYLE; return(T_ENDSTYLE); }
rehstyle	{ yylval.intval = REHSTYLE; return(T_REH_STYLE); }
boxed		{ yylval.intval = RS_BOXED; return(T_REHTYPE); }
circled		{ yylval.intval = RS_CIRCLED; return(T_REHTYPE); }
plain		{ yylval.intval = RS_PLAIN; return(T_REHTYPE); }
pedstyle	{ yylval.intval = PEDSTYLE; return(T_PEDSTYLE); }
pedstar		return(T_PEDSTAR);
barred		{ yylval.intval = ENDING_BARRED; return(T_ENDTYPE); }
grouped		{ yylval.intval = ENDING_GROUPED; return(T_ENDTYPE); }

brace		{ yylval.intval = BRACELIST; return(T_RANGELISTVAR); }
bracket		{ yylval.intval = BRACKLIST; return(T_RANGELISTVAR); }
barstyle	{ yylval.intval = BARSTLIST; return(T_BARSTLISTVAR); }
subbarstyle	{ yylval.intval = SUBBARSTYLE; return(T_SUBBARSTYLE); }
middle		{ return(T_MIDDLE); }
aboveorder	{ yylval.intval = PL_ABOVE; return(T_ORDER); }
beloworder	{ yylval.intval = PL_BELOW; return(T_ORDER); }
betweenorder	{ yylval.intval = PL_BETWEEN; return(T_ORDER); }
othertext	return(T_OTHERTEXT);
key		{ yylval.intval = SHARPS; return(T_KEY); }
useaccs		{ yylval.intval = USEACCS; return(T_USEACCS); }
carryaccs	{ yylval.intval = CARRYACCS; return(T_YESNOVAR); }
none		{ yylval.intval = UA_Y_NONE; return(T_NONE); }
noneremuser	{ yylval.intval = UA_Y_NONEREMUSER; return(T_NONE); }
nonnatremuser	{ yylval.intval = UA_Y_NONNATREMUSER; return(T_NONE); }
nonnat		{ return(T_NONNAT); }
time		{ yylval.intval = TIME; return(T_TIME); }
printedtime	{ yylval.intval = PRINTEDTIME; return(T_PRINTEDTIME); }
beamstyle	{ yylval.intval = BEAMSTLIST; return(T_RATNUMLISTVAR); }
visible		{ yylval.intval = VISIBLE; return(T_VISVAR); }
whereused	{ return(T_WHEREUSED); }
emptymeas	{ yylval.intval = EMPTYMEAS; return(T_STRVAR); }
measnum		{ yylval.intval = MEASNUM; return(T_MEASNUM); }
every		{ return(T_EVERY); }
slashesbetween	{ yylval.intval = SLASHESBETWEEN; return(T_YESNOVAR); }
cancelkey	{ yylval.intval = CANCELKEY; return(T_YESNOVAR); }
label		{ yylval.intval = LABEL; return(T_STRVAR); }
label2		{ yylval.intval = LABEL2; return(T_STRVAR); }
noteheads	{ yylval.intval = NOTEHEADS; return(T_STRVAR); }
acctable	{ yylval.intval = ACCTABLE; return(T_OPTSTRVAR); }
tuning		{ yylval.intval = TUNING; return(T_TUNINGVAR); }
equal		{ yylval.intval = TU_EQUAL; return(T_TUNINGTYPE); }
pythagorean	{ yylval.intval = TU_PYTHAGOREAN; return(T_TUNINGTYPE); }
meantone	{ yylval.intval = TU_MEANTONE; return(T_TUNINGTYPE); }

keymap		{ return(T_KEYMAP); }
defaultkeymap	{ yylval.intval = DEFAULTKEYMAP; return(T_KEYMAPVAR); }
endingkeymap	{ yylval.intval = ENDINGKEYMAP; return(T_KEYMAPVAR); }
labelkeymap	{ yylval.intval = LABELKEYMAP; return(T_KEYMAPVAR); }
lyricskeymap	{ yylval.intval = LYRICSKEYMAP; return(T_KEYMAPVAR); }
printkeymap	{ yylval.intval = PRINTKEYMAP; return(T_KEYMAPVAR); }
rehearsalkeymap	{ yylval.intval = REHEARSALKEYMAP; return(T_KEYMAPVAR); }
textkeymap	{ yylval.intval = TEXTKEYMAP; return(T_KEYMAPVAR); }
withkeymap	{ yylval.intval = WITHKEYMAP; return(T_KEYMAPVAR); }

font		{ yylval.intval = FONT; return(T_FONTVAR); }
fontfamily	{ yylval.intval = FONTFAMILY; return(T_FAMILY); }
lyricsfont	{ yylval.intval = LYRICSFONT; return(T_FONTVAR); }
lyricsfontfamily { yylval.intval = LYRICSFAMILY; return(T_FAMILY); }
measnumsize	{ yylval.intval = MEASNUMSIZE; return(T_NUMVAR); }
measnumfont	{ yylval.intval = MEASNUMFONT; return(T_FONTVAR); }
measnumfontfamily	{ yylval.intval = MEASNUMFAMILY; return(T_FAMILY); }
measnumstyle	{ yylval.intval = MEASNUMSTYLE; return(T_REH_STYLE); }
withsize	{ yylval.intval = WITHSIZE; return(T_NUMVAR); }
withfont	{ yylval.intval = WITHFONT; return(T_FONTVAR); }
withfontfamily	{ yylval.intval = WITHFAMILY; return(T_FAMILY); }
clef		{ yylval.intval = CLEF; return(T_CLEFVAR); }
unset		{ return(T_UNSET); }

8treble		{ yylval.intval = TREBLE_8A; return(T_CLEF); }
treble		{ yylval.intval = TREBLE; return(T_CLEF); }
treble8		{ yylval.intval = TREBLE_8; return(T_CLEF); }
frenchviolin	{ yylval.intval = FRENCHVIOLIN; return(T_CLEF); }
soprano		{ yylval.intval = SOPRANO; return(T_CLEF); }
mezzosoprano	{ yylval.intval = MEZZOSOPRANO; return(T_CLEF); }
alto		{ yylval.intval = ALTO; return(T_CLEF); }
tenor		{ yylval.intval = TENOR; return(T_CLEF); }
baritone	{ yylval.intval = BARITONE; return(T_CLEF); }
8bass		{ yylval.intval = BASS_8A; return(T_CLEF); }
bass		{ yylval.intval = BASS; return(T_CLEF); }
bass8		{ yylval.intval = BASS_8; return(T_CLEF); }
subbass		{ yylval.intval = SUBBASS; return(T_CLEF); }
drum		{ return(T_DRUM); }

">"		return(T_R_ANGLE);
"<"		{ yylval.intval = L_NORMAL; return(T_L_ANGLE); }
"<<"		{ return(T_L_DBLANGLE); }
">>"		{ return(T_R_DBLANGLE); }
dotted[ \t]*"<"	{ yylval.intval = L_DOTTED; return(T_L_ANGLE); }
dashed[ \t]*"<"	{ yylval.intval = L_DASHED; return(T_L_ANGLE); }
\\n		{ yylval.intval = IN_DOWNWARD; return(T_NOWHERE_SLIDE); }
"/n"		{
			if (Expr_mode == YES) {
				yyless(1);
				yylval.intval = OP_DIV;
				return(T_MULDIV_OP);
			}
			else {
				yylval.intval = IN_UPWARD;
				return(T_NOWHERE_SLIDE);
			}
		}
"n/"		{
			if (Expr_mode == YES) {
				yyless(1);
				return(T_LET_N);
			}
			else {
				yylval.intval = OUT_UPWARD;
				return(T_NOWHERE_SLIDE);
			}
		}
n\\		{ yylval.intval = OUT_DOWNWARD; return(T_NOWHERE_SLIDE); }

pedal		return(T_PEDAL);
mussym		return(T_MUSSYM);
chord		{ yylval.intval = TM_CHORD; return(T_MODIFIER); }
analysis	{ yylval.intval = TM_ANALYSIS; return(T_MODIFIER); }
figbass		{ yylval.intval = TM_FIGBASS; return(T_MODIFIER); }
dyn		{ yylval.intval = TM_DYN; return(T_MODIFIER); }
phrase		return(T_PHRASE);
til		return(T_TIL);
octave		return(T_OCTAVE);
roll		return(T_ROLL);
midi		return(T_MIDI);

bbox		{ return(T_BBOX); }
ystemoffset	{ return(T_STEMOFFSET); }

"*"		{
			if (Expr_mode == YES) {
 				yylval.intval = OP_MUL;
				return(T_MULDIV_OP);
			}
			else {
				return(T_STAR);
			}
		}

"//"		to_eol();

if			{
				If_length = 0;
				If_errs = 0;
				BEGIN IF_CLAUSE;
				push_if_line(IT_IF);
			}
<IF_CLAUSE>then		{
				/* End of 'if' clause'. Set back to the
				 * normal lexer. Call the mini-paser on
				 * the compressed form of the clause we
				 * have just gotten
				 */
				BEGIN 0;
				set_ifclause_buffer(Ifclause_buff, If_length);
				if (ifparse() == YES) {
					If_count++;
				}
				else {
					/* skip ifdef-ed part */
					if (skip2else() == YES) {
						/* found an else */
						If_count++;
					}
					/* if found an endif instead,
					 * we just continue */
				}
			}
<IF_CLAUSE>"&&"		{ add_ifclause('a'); }
<IF_CLAUSE>"||"		{ add_ifclause('o'); }
<IF_CLAUSE>[!()+-/*%<>~&|^:?]	{ add_ifclause(yytext[0]); }
<IF_CLAUSE>"<<"		{ add_ifclause('l'); }
<IF_CLAUSE>">>"		{ add_ifclause('r'); }
<IF_CLAUSE>"<="		{ add_ifclause('L'); }
<IF_CLAUSE>">="		{ add_ifclause('G'); }
<IF_CLAUSE>"=="		{ add_ifclause('E'); }
<IF_CLAUSE>"!="		{ add_ifclause('N'); }
<IF_CLAUSE>0x[0-9A-Fa-f]+	|
<IF_CLAUSE>[0-9]+	{ addnumstr_ifclause(yytext); }
<IF_CLAUSE>defined[ \t\n]*"("?[ \t\n]*[A-Z][A-Z_0-9]*[ \t\n]*")"?	{
				char * macname;

				if (strchr(yytext, '(') != 0) {
					if (yytext[yyleng-1] != ')') {
						yyerror("missing ')' at end of target of 'defined'");
					}
				}
				else {
					if (yytext[yyleng-1] == ')') {
						unput(')');
					}
				}
				macname = yytext + 7 + strspn(yytext + 7, "( \t\n");
				strtok(macname, " \t\n)");
				add_ifclause(is_defined(macname, YES) ? 'T' : 'F');
			}

<IF_CLAUSE>[A-Z][A-Z_0-9]*	{
					if (is_defined(yytext, YES) != 0) {
						call_macro(yytext);
					}
					else {
						addnumstr_ifclause("0");
					}
				}
<IF_CLAUSE>[ \t\r?\n]	;
<IF_CLAUSE>\\[ \t]*\r?\n	{
			/* ignore backslashed newlines;
			 * allow white space after backslash */
			ignore();
		}
<IF_CLAUSE>endif	{
				yyerror("Missing 'then' for 'if' clause");
				BEGIN 0;
			}
<IF_CLAUSE>.		{
				bad_input("'if' clause");
				/* User forgetting or misspelling 'then' can
				 * quickly cause hundreds of errors,
				 * so bail out if we suspect that.
				 * The actual number of errors to bail out on
				 * is arbitrary. */
				if (++If_errs > 20) {
					ufatal("Too many errors in 'if' clause. Maybe missing 'then'?");
				}
			}

define[ \t]+[A-Z][A-Z_0-9]*"("?		define_macro(yytext + 7);

undef[ \t]+[A-Z][A-Z_0-9]*			undef_macro(yytext + 6);

ifn?def[ \t]+[A-Z][A-Z_0-9]*	{
					int result;

					/* do ifdef or ifndef as appropriate */
					if (yytext[2] == 'n') {
						push_if_line(IT_IFNDEF);
						result = ! is_defined(yytext + 7
							+ strspn(yytext + 7, " \t"), NO);
					}
					else {
						push_if_line(IT_IFDEF);
						result = is_defined(yytext + 6
							+ strspn(yytext + 6, " \t"), NO);
					}

					if (result) {
						/* do stuff till else or endif */
						If_count++;
					}
					else {
						/* skip ifdef-ed part */
						if (skip2else() == YES) {
							/* found an else */
							If_count++;
						}
						/* if found an endif instead,
						 * we just continue */
					}
				}

else				{
					if (If_count < 1) {
						yyerror("else without ifdef");
					}
					else {
						If_count--;
						if (skip2else() == YES) {
							yyerror("else without ifdef");
						}
					}
				}

endif				{
					if (If_count < 1) {
						yyerror("endif without ifdef");
					}
					else {
						If_count--;
						pop_if_line();
					}
				}

[A-Z][A-Z_0-9]*		call_macro(yytext);
"`"?[A-Z][A-Z_0-9]*([ 	]*##[ 	]*[A-Z][A-Z_0-9]*)+"`"?	{
							macro_concat(yytext);
						}
"`"[ 	]*[A-Z][A-Z_0-9]*[ 	]*"`"	{
				if (isspace(yytext[1]) ||
						isspace(yytext[yyleng-2])) {
					yyerror("spaces not allowed inside ` ` marks");
				}
				else {
					/* We only need the leading ` to tell
					 * it is a quoted macro parm,
					 * and it's easier to deal with later
					 * if there is no trailing `
					 * so strip off the trailing. */
					yytext[yyleng - 1] = '\0';
					call_macro(yytext);
				}
			}

savemacros[ \t\r\n]*\"[^"]*\"	{
			yytext[yyleng - 1] = '\0';
			mac_saveto(strchr(yytext, '"') + 1);
		}
restoremacros[ \t\r\n]*\"[^"]*\"	{
			yytext[yyleng - 1] = '\0';
			mac_restorefrom(strchr(yytext, '"') + 1);
		}

"|||-.->>"	{
			/* This pattern has to be kept in sync with the
			 * value of EM_begin_marker in grpsyl.c */
			return(T_EM_BEGIN);
		}
"<<-.-|||"	{
			/* This pattern has to be kept in sync with the
			 * value of EM_end_marker in grpsyl.c */
			return(T_EM_END);
		}

"'"+		{ yylval.intval = strlen(yytext); return(T_TICKS); }

\\[ \t]*\r?\n	{
			/* ignore backslashed newlines;
			 * allow white space after backslash */
			ignore();
		}

\032			{
				/* in case we get control-Z from DOS */
				ignore();
			}

[ 	]		{
				/* ignore extra white space */
				ignore();
			}
.		{
			bad_input("input");
		}

exit		{
			/* User says they want out now! So if they happen
			 * to be inside an unclosed 'if' clause, we want to
			 * ignore that error. So clear the 'if' info */
			while (If_linenum_stack != 0) {
				pop_if_line();
			}
			If_count = 0;
			/* Tell parser we are all done lexing */
			return(0);
		}

<<EOF>>		{
			/* In case user used an editor or other tool to
			 * create the input file which may leave the final
			 * line without a newline, add a newline to the
			 * input. If there was already a newline, an extra
			 * one should never hurt. Adding it can prevent
			 * cryptic message about error at token '' */
			if (Eof_newline == 0) {
				Eof_newline = yy_scan_string("\n");
			}
			else {
				yy_delete_buffer(Eof_newline);
				return(0);
			}
		}


%%


/* comments go to end of line. This can be done with a simple
 * lex pattern, but when taken with all the other patterns, lex blows
 * up because of too many right contexts. So we do this special case
 * with this function.
 */

static void
to_eol()

{
	int inp;

	/* The rather unusual use of comma operator is so we can get one
	 * copy of the input characters as type int (so the compiler won't
	 * complain about the compare to EOF) and another copy cast to
	 * type char (so the compiler won't complain about the assignment
	 * into yytext array). We can't use
	 *   yytext[yyleng] = (char) inp = input()
	 * because then the compiler objects to using a cast on an lvalue.
	 */
	while ( (inp = input()), (yytext[yyleng] = (char) inp) != '\n'
					&& inp != 0 && inp != EOF) {
		if (++yyleng >= YYLMAX - 1) {
			/* Too long. Print what we have if using -C option.
			 * In any case discard what we have and start over,
			 * since we are skipping anyway. */
			if (Preproc == YES && Ppcomments == YES) {
				yytext[yyleng] = '\0';
				printf("%s", yytext);
			}
			yyleng = 0;
		}
		if (yytext[yyleng-1] == '\r') {
			/* If the \r is by itself, treat like a newline */
			int c;
			if ((c = input()) == '\n') {
				yytext[--yyleng] = '\n';
				break;
			}
			else if (c != 0) {
				unput(c);
			}
		}
	}

	if (inp ==  0 || inp == EOF) {
		return;
	}
	else {
		if (yyleng > 0 && yytext[yyleng - 1] == '\\') {
			/* if backslashed newline, replace with real newline
			 * and continue */
			yytext[--yyleng] = '\n';
			to_eol();
		}
		/* handle DOS style backslashed \r\n */
		else if (yyleng > 1 && yytext[yyleng - 1] == '\r'
						&& yytext[yyleng - 2] == '\\') {
			yytext[yyleng - 2] = '\r';
			yytext[--yyleng] = '\n';
			to_eol();
		}

		/* put newline back onto input stream */
		unput('\n');
	}

	yytext[yyleng] = '\0';

	if (Preproc == YES && Ppcomments == YES) {
		printf("%s", yytext);
	}
}


/* Save macro text in specified file.
 * @ is used to mark end of macro definition.
 * Returns YES if there appeared to be any references to
 * macro parameters to be quoted.
 * That used to be used to try to optimize away making quoted copies when
 * they weren't necessary, but it could skip when it shouldn't, so that is
 * no longer done, but the return is kept for now, in case it becomes useful
 * again later.
 */

int
save_macro(file)

FILE *file;	/* put macro text into this file */

{
	int c;				/* character read from macro */
	int num_backslashes = 0;
	int has_quoted_parameters = NO;
	int first = YES;


	while ((c = input()) != 0 && c != EOF) {
		if (first == YES) {
			first = NO;
			if (c == ' ' || c == '\t') {
				/* This is just to separate the macro name
				 * from the value, so ignore it. */
				continue;
			}
		}
		if (c == '@') {
			/* if had a backslash before it, use real @ */
			if (num_backslashes == 1) {
				putc(c, file);
				num_backslashes = 0;
			}
			else {	
				/* reached end of macro definition */
				return(has_quoted_parameters);
			}
		}
		else {
			if (c == '\\') {
				num_backslashes++;
				if (num_backslashes == 2) {
					putc(c, file);
					num_backslashes = 0;
				}
			}
			else {
				if (num_backslashes == 1) {
					putc('\\', file);
				}
				putc(c, file);
				num_backslashes = 0;
				if (c == '`') {
					has_quoted_parameters = YES;
				}
			}
		}
	}

	yyerror("unterminated macro");
	return(has_quoted_parameters);
}


/* Collect the parameter names on the definition of a macro with parameters.
 * Note that num_args might be 0 (macro defined as X()). */

void
get_parameters(macname)

char *macname;		/* name of macro being defined */

{
	while (get_a_param(macname) == YES) {
		;
	}
}

static int
get_a_param(macname)

char *macname;

{
	int c;		/* input character */
	int index;	/* into name buffer */


	/* skip leading white space */
	skipwhite();

	/* collect macro-like parameter names (upper case, digits, or
	 * underscore, starting with upper case), delimited by comma
	 * and ended by close parenthesis. */
	while ((c = input()) != '\0' && c != EOF) {
		if (isupper(c)) {
			yytext[0] = c;
			index = 1;
			while ((c = input()) != '\0' && c != EOF) {
				if (isupper(c) || isdigit(c) || c == '_') {
					yytext[index++] = c;
				}
				else  {
					/* found end of parameter name */
					unput(c);
					yytext[index] = '\0';
					add_parameter(macname, yytext);

					/* skip white space */
					skipwhite();

					/* next better be , or ) */
					if ((c = input()) != ',' && c != ')') {
						l_yyerror(Curr_filename, yylineno,
							"unexpected character '%c' in parameter list for macro %s",
							c, macname);
						return(NO);
					}
					if (c == ')') {
						/* return to input so we can
						 * read on next call to know
						 * we are at end of list */
						unput(c);
					}
					return(YES);
				}
			}
		}
		else if (c == ')') {
			/* end of parameter list */
			return(NO);
		}
		else {
			l_yyerror(Curr_filename, yylineno,
				"illegal macro parameter name starting at character '%c'", c);
			return(NO);
		}
	}
	yyerror("unexpected end-of-file in macro parameter list");
	return(NO);
}


/* Read input, skipping any white space. White space is space, tab, return,
 * backslashed return, newline, or backslashed newline.
 * Backslashed anything else will leave the backslash in the input
 * without the thing that followed it.
 */

 
static void
skipwhite()

{
	int c;

	while ((c = input()) != '\0' && c != EOF) {
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			continue;
		}

		if (c == '\\') {
			if ((c = input()) == '\n' || c == '\r' ) {
				/* backslashed newline/return is white space */
				continue;
			}
			else {
				unput('\\');
				return;
			}
		}
		/* non-space. Stuff back into input and return */
		unput(c);
		return;
	}
}



/* collect arguments to a call to a macro with parameters */

int
get_mac_arguments(macname, num_args)

char *macname;		/* name of the macro, to use in error messages */
int num_args;		/* number of arguments expected */

{
	int c;		/* input character */
	char *argbuff;	/* for storing argument */
	int n;		/* number of arguments collected so far */


	/* first skip any white space before the opening parenthesis */
	skipwhite();

	/* next thing better be open parenthesis */
	if ((c = input()) != '(') {

		l_yyerror(Curr_filename, yylineno,
			"macro %s has parameters, but has no '(' on call",
			macname);

		/* Trying to parse anything more in this file might be
		 * almost hopeless. But we'll try just skipping to end of
		 * the line and returning failure and hope for the best. */
		while (((c = input()) != '\0') && (c != EOF) &&
						(c != '\n') && (c != '\r')) {
			;
		}
		return(NO);
	}

	/* init to no argument collected. Space will be malloc-ed when needed */
	argbuff = (char *) 0;
	n = 0;

	/* Now collect arguments. Read and process input
	 * one character at a time */
	while ((c = input()) != '\0' && c != EOF) {
		switch (c) {
		case '\\':
			/* something that has been escaped with a backslash.
			 * Get the next character and do appropriate escape. */
			switch (c = input()) {
			default:
				/* some unexpected backslashed thing */
				l_warning(Curr_filename, yylineno,
					"dubious escape \\%c in argument %d of macro %s",
					c, n+1, macname);
				/* fall through to use the literal, to be
				 * sort of consistent with how C handles
				 * such dubious escapes */
				/*FALLTHRU*/

			case '"':	/* could be a quote inside a string */
			case ',':	/* would normally mean end of argument */
			case ')':	/* would normally mean end of argument list */
			case '\\':	/* user wants a real backslash */
				/* escaped to allow literal, so copy the
				 * character without backslash to buffer */
				argbuff = add2argbuff(argbuff, c);
				break;

			case '\n':
				/* user wanted to put a newline in the input
				 * to make it look better, but doesn't really
				 * want a newline here in the argument text,
				 * so just skip over this character */
				break;

			case '\r':
				/* If by itself, treat like newline.
				 * If immediately followed by a newline,
				 * treat the pair like newline.
				 */
				if ((c = input()) != '\n') {
					/* Followed by something other than
					 * newline, so push back into input.
					 */
					if (c != EOF) {
						unput(c);
					}
				}
				break;
			case '\0':
				l_yyerror(Curr_filename, yylineno,
					"unexpected end of file in macro %s argument %d",
					macname, n + 1);
				return(NO);
			}
			break;

		case ',':
			/* This marks the end of an argument.
			 * Process this argument, then reset for next one */
			if (++n <= num_args) {
				set_parm_value(macname, argbuff, n);
			}
			else {
				/* discard this one. It's extra. But wait
				 * to print error until we find the ),
				 * so if they have lots of extras we only
				 * print one message */
				if (argbuff != (char *) 0) {
					FREE(argbuff);
				}
			}

			/* reset for next argument */
			argbuff = (char *) 0;
			break;

		case ')':
			/* This marks end of arguments. Save the last one
			 * collected and signal success. The case of
			 * num_args == 0 has to be handled specially.
			 */
			if (n == 0 && argbuff == (char *) 0 && num_args == 0) {
				return(YES); /* no args to save */
			}
			if (++n == num_args) {
				/* good. got exactly the right number of
				 * arguments. Save the last one and
				 * declare victory */
				set_parm_value(macname, argbuff, n);
				return(YES);
			}
			else {
				l_yyerror(Curr_filename, yylineno,
					"too %s arguments to macro %s (expected %d, got %d)",
					((n < num_args) ? "few" : "many"),
					macname, num_args, n);
			}
			return(NO);

		default:
			/* ordinary character. Save it away */
			argbuff = add2argbuff(argbuff, c);
			break;
		}
	}

	l_yyerror(Curr_filename, yylineno, "unexpected end of file in call to %s macro",
		macname);
	return(NO);
}


/* skip input until find an else or endif. If find else, return YES, if find
 * endif or end of file, return NO */
/* It probably would be possible to get lex to do this pattern matching with
 * appropriate start conditions, but it seems like too much trouble to figure
 * that out. Just need to look for 2 simple patterns, so do simple state
 * machine */

typedef enum { STATE_NONE, STATE_E, STATE_EL, STATE_ELS, STATE_EN,
		STATE_END, STATE_ENDI, STATE_I, STATE_IF, STATE_IFD,
		STATE_IFDE } STATE;

static int
skip2else()

{
	int c;				/* char read from input */
	STATE state = STATE_NONE;
	int escaped;		/* true if got odd number of backslashes */
	int saved_ppcomments;	/* saved value of Ppcomments */
	int is_ndef = NO;	/* For the most part, we treat ifdef and
				 * ifndef the same here, but this tells
				 * us which it actually is */


	/* When skipping stuff in if/else, we don't want anything to come
	 * out when using -E -C. Since this function can call to_eol(),
	 * we have to convince it -C isn't on even if it is. */
	saved_ppcomments = Ppcomments;
	Ppcomments = NO;

	/* read characters. Based on current character and state, go into
	 * new state if appropriate. */
	while ((c = input()) != 0 && c != EOF) {

		switch (c) {

		case 'e':
			if (state == STATE_ELS) {
				YY_FLUSH_BUFFER;
				Ppcomments = saved_ppcomments;
				return(YES);
			}
			else if (state == STATE_IFD) {
				state = STATE_IFDE;
			}
			else {
				state = STATE_E;
			}
			break;

		case 'l':
			state = (state == STATE_E ? STATE_EL : STATE_NONE);
			break;

		case 's':
			state = (state == STATE_EL ? STATE_ELS : STATE_NONE);
			break;

		case 'n':
			if (state == STATE_E) {
				state = STATE_EN;
			}
			else if (state == STATE_IF) {
				/* For ifndef, mostly treat this like ifdef.
				 * But we do note that it actually is ifndef,
				 * so that if we get another n, we wouldn't
				 * accept ifnndef, and so we can know what
				 * type to tell push_if_line().
				 */
				if (is_ndef == YES) {
					/* this is a syntax error by user */
					state = STATE_NONE;
				}
				else {
					is_ndef = YES;
				}
			}
			else {
				state = STATE_NONE;
			}
			break;

		case 'd':
			if (state == STATE_EN) {
				state = STATE_END;
			}
			else if (state == STATE_IF) {
				state = STATE_IFD;
			}
			else {
				state = STATE_NONE;
			}
			break;

		case 'i':
			state = (state == STATE_END ? STATE_ENDI : STATE_I);
			break;

		case 'f':
			if (state == STATE_ENDI) {
				YY_FLUSH_BUFFER;
				Ppcomments = saved_ppcomments;
				pop_if_line();
				return(NO);
			}
			else if (state == STATE_I) {
				state = STATE_IF;
				is_ndef = NO; /* not yet anyway... */
			}
			else if (state == STATE_IFDE) {
				push_if_line(is_ndef == YES ? IT_IFNDEF : IT_IFDEF);
				if (skip2else() == YES) {
					if (skip2else() == YES) {
						yyerror("else without ifdef");
					}
				}
			}
			else {
				state = STATE_NONE;
			}
			break;

		case '/':
			if ((c = input()) == '/') {
				/* Skip past comments, in case they happen to
				 * contain 'else' or 'endif'
				 * that could confuse us. */
				to_eol();
			}
			else if (c != 0 && c != EOF) {
				unput(c);
			}
			state = STATE_NONE;
			break;

		case '"':
			/* Skip strings, in case they happen to contain
			 * 'else' or 'endif' that could confuse us. */
			escaped = 0;
			while ((c = input()) != 0 && c != EOF) {
				if (c == '"' && ! escaped) {
					break;
				}
				if (c == '\\') {
					escaped ^= 1;
				}
				else {
					escaped = 0;
				}
			}
			state = STATE_NONE;
			break;
		default:
			if (state == STATE_IF) {
				push_if_line(IT_IF);
				/* skip to else or endif */
				if (skip2else() == YES) {
					/* if was an else, need to continue
					 * to the endif */
					if (skip2else() == YES) {
						yyerror("else without ifdef");
					}
				}
			}
			state = STATE_NONE;
			break;
		}
	}

	/* Hit end of file. When chk_ifdef is called, it will find
	 * we are in this state and print an error. */
	return(NO);
}


/* at the very end, verify all if, ifdef, and ifndef clauses are closed */

void
chk_ifdefs()

{
	if (If_count != 0 && If_linenum_stack == 0) {
		/* This means the If_count code that existed prior
		 * to Mup version 6.3 thinks we have a missing endif,
		 * but the code added in 6.3 that tries to keep
		 * track of starting line number to give a better error
		 * message does not. So we should never hit this code unless
		 * there is a bug in the newer code, but if we ever do get here,
		 * we fall back to the old less helpful message in that case.
		 */
		yyerror("missing endif");
		return;
	}
	/* Report on any as yet unclosed if* statements */
	while (If_linenum_stack != 0) {
		l_yyerror(Curr_filename, yylineno,
			"No endif for %s begun on line %d of %s",
			if_type_name(If_linenum_stack->if_type),
			If_linenum_stack->linenum,
			If_linenum_stack->inputfile);
		pop_if_line();
	}
}


/* check if there is an odd number of backslashes starting before the final
 * double quote. If so, return YES--the last backslash applies to the double
 * quote. Otherwise return NO, because the backslash was itself backslashed
 * which means user wants to end string with a backslash */

static int
oddbs()

{
	int i;
	int bscount = 0;	/* count of backslashes */

	for (i = yyleng - 2; i >= 0; i--) {
		if (yytext[i] == '\\') {
			bscount++;
		}
		else {
			break;
		}
	}
	return ((bscount & 1) ? YES: NO);
}


/* Flex has the (documented) limitation that you can't change the value
 * of yyleng in the same rule action as you use yymore. So when there is
 * an escaped double quote mark inside a string, we can't remove it and
 * also use yymore in the same rule. So we collect the whole string,
 * including their embedded quote marks and their backslashes,
 * then call this function which moves the characters back to get rid of
 * the backslashes, and adjusts yyleng appropriately.
 */

static void
embedquotes()

{
	int i;
	int moveback;

	if (Escapedquotes < 1) {
		/* No escaped quote found, so nothing to do here */
		return;
	}

	moveback = 0;
	for (i = 1; i < yyleng; i++) {
		/* if there is a quote inside the string,
		 * it must have been escaped */
		if (yytext[i] == '"' && yytext[i-1] == '\\' && i < yyleng - 1) {
			moveback++;
		}
		/* If we have come across one or more embedded quotes,
		 * move bytes to the left one position for each quote,
		 * which gets rid of the backslashes. */
		if (moveback > 0) {
			yytext[i-moveback] = yytext[i];
		}
	}
	yytext[i-moveback] = '\0';
	yyleng -= Escapedquotes;
	Escapedquotes = 0;
		
}


/* Mapping of Latin-1 non-ASCII characters to their Mup names. If Mup
 * doesn't support the value, or the value is undefined in Latin-1,
 * value in this table is 0. */
static char *latin1_to_mup_name[] = {
	/* 128 */	0,
	/* 129 */	0,
	/* 130 */	0,
	/* 131 */	0,
	/* 132 */	0,
	/* 133 */	0,
	/* 134 */	0,
	/* 135 */	0,
	/* 136 */	0,
	/* 137 */	0,
	/* 138 */	0,
	/* 139 */	0,
	/* 140 */	0,
	/* 141 */	0,
	/* 142 */	0,
	/* 143 */	0,
	/* 144 */	0,
	/* 145 */	0,
	/* 146 */	0,
	/* 147 */	0,
	/* 148 */	0,
	/* 149 */	0,
	/* 150 */	0,
	/* 151 */	0,
	/* 152 */	0,
	/* 153 */	0,
	/* 154 */	0,
	/* 155 */	0,
	/* 156 */	0,
	/* 157 */	0,
	/* 158 */	0,
	/* 159 */	0,
	/* 160 */	"space",
	/* 161 */	"exclamdown",
	/* 162 */	"cent",
	/* 163 */	"sterling",
	/* 164 */	"currency",
	/* 165 */	"yen",
	/* 166 */	"brokenbar",
	/* 167 */	"section",
	/* 168 */	"dieresis",
	/* 169 */	"copyright",
	/* 170 */	"ordfeminine",
	/* 171 */	"<<",
	/* 172 */	"logicalnot",
	/* 173 */	"endash",
	/* 174 */	"registered",
	/* 175 */	"macron",
	/* 176 */	"degree",
	/* 177 */	"plusminus",
	/* 178 */	"twosuperior",
	/* 179 */	"threesuperior",
	/* 180 */	"acute",
	/* 181 */	"mu",
	/* 182 */	"paragraph",
	/* 183 */	"dotaccent",
	/* 184 */	"cedilla",
	/* 185 */	"onesuperior",
	/* 186 */	"ordmasculine",
	/* 187 */	">>",
	/* 188 */	"onequarter",
	/* 189 */	"onehalf",
	/* 190 */	"threequarters",
	/* 191 */	"questiondown",
	/* 192 */	"A`",
	/* 193 */	"A'",
	/* 194 */	"A^",
	/* 195 */	"A~",
	/* 196 */	"A:",
	/* 197 */	"Ao",
	/* 198 */	"AE",
	/* 199 */	"C,",
	/* 200 */	"E`",
	/* 201 */	"E'",
	/* 202 */	"E^",
	/* 203 */	"E:",
	/* 204 */	"I`",
	/* 205 */	"I'",
	/* 206 */	"I^",
	/* 207 */	"I:",
	/* 208 */	"Eth",
	/* 209 */	"N~",
	/* 210 */	"O`",
	/* 211 */	"O'",
	/* 212 */	"O^",
	/* 213 */	"O~",
	/* 214 */	"O:",
	/* 215 */	"multiply",
	/* 216 */	"Oslash",
	/* 217 */	"U`",
	/* 218 */	"U'",
	/* 219 */	"U^",
	/* 220 */	"U:",
	/* 221 */	"Y'",
	/* 222 */	"Thorn",
	/* 223 */	"germandbls",
	/* 224 */	"a`",
	/* 225 */	"a'",
	/* 226 */	"a^",
	/* 227 */	"a~",
	/* 228 */	"a:",
	/* 229 */	"ao",
	/* 230 */	"ae",
	/* 231 */	"c,",
	/* 232 */	"e`",
	/* 233 */	"e'",
	/* 234 */	"e^",
	/* 235 */	"e:",
	/* 236 */	"i`",
	/* 237 */	"i'",
	/* 238 */	"i^",
	/* 239 */	"i:",
	/* 240 */	"eth",
	/* 241 */	"n~",
	/* 242 */	"o`",
	/* 243 */	"o'",
	/* 244 */	"o^",
	/* 245 */	"o~",
	/* 246 */	"o:",
	/* 247 */	"divide",
	/* 248 */	"o/",
	/* 249 */	"u`",
	/* 250 */	"u'",
	/* 251 */	"u^",
	/* 252 */	"u:",
	/* 253 */	"y'",
	/* 254 */	"thorn",
	/* 255 */	"y:"
};

/* This function goes through the string currently in yytext, and if it finds
 * any non-ASCII characters that can be transformed into the non-ASCII
 * characters that Mup supports through the \(....) escape mechanism,
 * it replaces them with the appropriate escape strings.
 * This functionality probably logically belongs in fix_string(),
 * but for historical reasons, that function is not allowed
 * to increase the length of a string,
 * so it's easier to do this step separately.
 */

static void
xpand_non_ascii()
{
	int index;			/* walk through yytext */
	int start;			/* index of first non-ascii */
	int value;			/* value of a character */
	char *replacement;		/* Mup's name for the non_ascii */
	static char *tmp_buff = 0;	/* for expanded version */
	int tmp_index;			/* index into tmp_buff */

	if (Raw_string == YES) {
		/* Collecting a string for something like raw PostScript,
		 * that shouldn't be converted. */
		return;
	}

	/* see if any non-ascii */
	for (index = 0; index < yyleng; index++) {
		if (yytext[index] & 0x80) {
			break;
		}
	}

	if (index == yyleng) {
		/* no non-ascii in current string, so leave it as is */
		return;
	}

	/* If we don't have a buffer yet, allocate it. If an input file
	 * has no non-ascii in it at all, we'll never get here, and no
	 * memory will be wasted on a buffer we don't need. If we do get
	 * here, chances are there will be lots more non-ascii, so we
	 * keep the buffer around, rather than constantly doing malloc/free. */
	if (tmp_buff == 0) {
		MALLOCA(char, tmp_buff, YYLMAX);
	}

	/* only need to rewrite the part of the string
	 * beyond the first non-ascii, so remember where we are now */
	start = index;

	/* walk through, converting non-ascii to their Mup names */
	for (tmp_index = 0; index < yyleng; index++) {

		if (yytext[index] & 0x80) {
			/* Look up replacement. The replacement table
			 * only contains entries for 128-255,
			 * so adjust the value of the non-ascii character
			 * to get the appropriate array offset. */
			value = ((int)(yytext[index])) & 0xff;
			replacement = latin1_to_mup_name[value - 128];

			if (replacement == (char *) 0) {
				/* Apparently user is able to input a
				 * character that Mup doesn't support... */
				l_yyerror(Curr_filename, yylineno,
					"unsupported non-ASCII character (0x%x) in string",
					yytext[index] & 0xff);
			}
			else {
				(void) sprintf(tmp_buff + tmp_index, "\\(%s)",
								replacement);
				tmp_index += strlen(replacement) + 3;
			}
		}
		else {
			/* normal ASCII character; copy as is */
			tmp_buff[tmp_index++] = yytext[index];
		}
	}

	/* Now we can copy the expanded string back into yytext and update
	 * yyleng so that all later code will think the user typed in the
	 * expanded version themselves. */
	(void) strncpy(yytext + start, tmp_buff, tmp_index);
	yyleng = start + tmp_index;
	yytext[yyleng] = '\0';
}


/* push a character back into input. This allows another C file to
 * access the unput macro */

void
pushback(c)

int c;

{
	unput(c);
}


/* this function will do nothing unless the "preprocessor only" flag is on,
 * in which case it will print the current token
 */

static void
ignore()

{
	if (Preproc == YES) {
		printf("%s", yytext);
	}
}


/* When collecting raw PostScript, we don't want to convert non-ASCII to
 * Mup escapes. These functions turn the raw processing on and off. */

void
begin_raw()
{
	Raw_string = YES;
}

void
end_raw()
{
	Raw_string = NO;
}


/* Create a new flex buffer for the given input file and switch to it */

void
new_lexbuff(file)

FILE * file;

{
	struct Lexbuffer *newbuff;

	newbuff = add_lexbuff();
	newbuff->buff = yy_create_buffer(file, YY_BUF_SIZE);
	yy_switch_to_buffer(newbuff->buff);
}

/* Common function that pushes an entry on the Lexbuffer stack and
 * returns it. It can be used for either a file or string buffer. */

static struct Lexbuffer *
add_lexbuff()

{
	struct Lexbuffer *newbuff;

	if (Lexlist == 0) {
		MALLOC(Lexbuffer, newbuff, 1);
		newbuff->next = 0;
		newbuff->buff = YY_CURRENT_BUFFER;
		Lexlist = newbuff;
	}

	MALLOC(Lexbuffer, newbuff, 1);
	newbuff->next = Lexlist;
	Lexlist = newbuff;
	return(newbuff);
}

/* Switch back to previous flex buffer on the stack */

void
del_lexbuff()

{
	struct Lexbuffer *nextbuff;

	if (Lexlist == 0) {
		pfatal("attempt to pop from empty lex buffer stack");
	}
	nextbuff = Lexlist->next;
	yy_delete_buffer(Lexlist->buff);
	FREE(Lexlist);
	Lexlist = nextbuff;
	yy_switch_to_buffer(nextbuff->buff);
}


void
new_lexstrbuff(buff, len)

char *buff;
int len;

{
	struct Lexbuffer *newbuff;

	/* Add the terminators */
	buff[len-1] = YY_END_OF_BUFFER_CHAR;
	buff[len-2] = YY_END_OF_BUFFER_CHAR;
	newbuff = add_lexbuff();
	newbuff->buff = yy_scan_buffer(buff, len);
}


/* The contents of an "if clause" is stored in compressed form in
 * Ifclause_buff, and eventually passed to a mini-parser. This function
 * add tokens to the buffer. Compressed format:
 *	input		compressed token
 *    -------------------------------------
 *	(		(
 *	)		)
 *	!		!
 *	~		~
 *	*		*
 *	/		/
 *	%		%
 *	+		+
 *	-		-
 *	<<		l
 *	>>		r
 *	<		<
 *	>		>
 *	<=		L
 *	>=		G
 *	==		E
 *	!=		N
 *	&		&
 *	^		^
 *	|		|
 *	&&		a
 *	||		o
 *	?		?
 *	:		:
 *	defined(X)	T or F depending on whether X is defined or not
 *      literal number	# followed by 4 bytes in MSB order
 *      macro name 	# followed by macro's value as 4 bytes in MSB order
 */

static void
add_ifclause(token)

int token;

{
	if (If_length >= sizeof(Ifclause_buff)) {
		ufatal("Sorry, 'if' clause is too complex");
	}
	Ifclause_buff[If_length++] = (unsigned char) (token & 0xff);
}

/* We store numbers in the if-clause compressed form as '#' followed by
 * 4 bytes in MSB order.
 */


static void
addnum_ifclause(value)

int value;

{
	int shift;

	add_ifclause('#');
	for (shift = 24; shift >= 0; shift -= 8) {
		add_ifclause((value >> shift) & 0xff);
	}
}


/* Add number in ASCII. Convert to value and add */

static void
addnumstr_ifclause(numtoken)

char *numtoken;

{
	int value;
	char *end_p;

#ifdef ERANGE
	errno = 0;
#endif
	value = strtol(numtoken, &end_p, 0);
	if (*end_p != '\0') {
		/* Lexer shouldn't allow unparsable number,
		 * so if strtol didn't get to end, must be too big */
		yyerror("bad number (probably out of range)");
	}
#ifdef ERANGE
	/* Additional range check assuming strtol sets this properly
	 * on all systems. */
	else if (errno == ERANGE) {
		yyerror("number is out of range");
	}
#endif

	addnum_ifclause(value);
}


/* Give error message for invalid input character, as actual character
 * if printable, otherwise as hex value. */

static void
bad_input(where)

char *where;		/* describes where the bad input was found */

{
	if (isgraph(yytext[0])) {
		l_yyerror(Curr_filename, yylineno,
			"invalid character in %s: '%c'",
			where, yytext[0]);
	}
	else {
		l_yyerror(Curr_filename, yylineno,
			"invalid character in %s: 0x%x",
			where, (int) yytext[0]);
	}
}


/* Print the current token if yydebug is set */

static void
yydebugtoken()

{
	int i;		/* index through yytext */
	int ch;		/* the current character in yytext */


	fprintf(stderr, "Token read is \"");
	for (i = 0; i < yyleng; i++) {
		ch = yytext[i];

		if (ch >= 32 && ch < 127) {
			if (ch == '"' || ch == '\\') {
				putc('\\', stderr);
			}
			putc(ch, stderr);
			continue;
		}
		putc('\\', stderr);
		switch (ch) {
		case '\r':
			putc('c', stderr);
			break; 
		case '\n':
			putc('n', stderr);
			break; 
		case '\t':
			putc('t', stderr);
			break;
		default:
			fprintf(stderr, "x%02x", ch);
			break;
		}
	}                                            
	fprintf(stderr, "\"\n");
}


/* When doing arithmetic expresssions for coordinates, we return special
 * tokens for arithmetic operators, to ensure the precedence rules for those
 * don't mess up the other uses of the symbols when used in other contexts.
 * So the parser calls this function to turn the special mode on and off.
 */

void
set_lex_mode(doing_expression)

int doing_expression;	/* YES or NO */

{
	Expr_mode = doing_expression;
}


/* Add a line number to the stack of line numbers of in-progress
 * if/ifdef/ifndef statements. */

static void
push_if_line(if_type)

int if_type;	/* IT_* value */

{
	struct IF_LINE *new_if_line;

	MALLOC(IF_LINE, new_if_line, 1);
	new_if_line->linenum = yylineno;
	new_if_line->inputfile = Curr_filename;
	new_if_line->if_type = if_type;
	new_if_line->prev = If_linenum_stack;
	If_linenum_stack = new_if_line;
}


/* Remove a line number from the stack of line numbers of in-progress
 * if/ifdef/ifndef statements. */

static void
pop_if_line()

{
	struct IF_LINE *last_if_line;

	if (If_linenum_stack == 0) {
		pfatal("pop_if_line called with empty stack");
	}
	last_if_line = If_linenum_stack;
	If_linenum_stack = last_if_line->prev;
	FREE(last_if_line);
}


/* Translate an 'if' type "enum" to its name */

static char *
if_type_name(if_type)

int if_type;	/* IT_* value */

{
	switch (if_type) {
	case IT_IFDEF:	return("ifdef");
	case IT_IFNDEF:	return("ifndef");
	case IT_IF:	return("if");
	/* The default should never be hit, but if it is, we use the
	 * vague and generic "if" */
	default:	return("if");
	}
}
