" Vim syntax file
" Language:	Mup music notation language
" Maintainer:	Arkkra Enterprises <support@arkkra.com>
" URL:		http://www.arkkra.com/ftp/pub/unix/user.pgms/mup.vim
" Last Change:  22 June 2022 ( Mup 7.0 )

" Quit when a syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

syn case match

syn match Number "[0-9]\.*"
syn match Number "1/[24]"
syn match Operator "[-+,=:&%()#xn!./*<>]"
syn match Operator "[][{}]"
syn match Function "[~^?]"
syn match String "\".*\""
syn match Identifier "[a-z]"
syn match Comment "//.*$"
syn match Macro "[A-Z][A-Z0-9_]*"
syn match Define "@"
syn match Type "[\\/]n]" "n[\\/]"

syn keyword Operator sin cos tan asin acos atan hypot sqrt
syn keyword Operator round floor ceiling string
highlight MupContext ctermfg=Yellow ctermbg=Blue guifg=Yellow guibg=Blue
syn keyword MupContext header header2 footer footer2 top top2 bottom bottom2
syn keyword MupContext block music grids score voice staff headshapes symbol
syn keyword MupContext keymap accidentals control
" Note that shapes is also used as a parameter name
syn keyword MupContext shapes

syn keyword Type dim diminished aug augmented per perfect min minor maj major
syn keyword Type wide medium wavy
syn keyword Type tab boxed circled plain pedstar barred grouped
syn keyword Type othertext inches cm
syn keyword Type chord analysis figbass dyn
syn keyword Type above below between all lyrics multirest ending
syn keyword Type bass bass8 8bass subbass treble 8treble treble8 alto soprano mezzosoprano tenor baritone drum
syn keyword Type cue grace xnote dotted dashed
syn keyword Type blank diam isostri norm pie rect righttri semicirc slash allslash x allx
syn keyword Type 4n 2n 1n dblwhole altdblwhole filldiamond diamond dwhdiamond
syn keyword Type dbl quad
syn keyword Type fillisostriangle isostriangle dwhisostriangle
syn keyword Type fillpiewedge piewedge dwhpiewedge
syn keyword Type fillrectangle rectangle dwhrectangle
syn keyword Type fillrighttriangle righttriangle dwhrighttriangle
syn keyword Type fillurighttriangle urighttriangle dwhurighttriangle
syn keyword Type fillsemicircle semicircle dwhsemicircle
syn keyword Type fillslashhead slashhead dwhslashhead blankhead
syn keyword Type hs
syn keyword Type nooverlap shareone overlap stepsapart bymeas cut common
syn keyword Type letter legal flsa halfleter
syn keyword Type portrait landscape
syn keyword Type equal pythagorean meantone cents

syn keyword Function midi rom ital bold boldital
syn keyword Function title left center right print nl paragraph ragged justified
syn keyword Function newscore newpage
syn keyword Function samescorebegin samescoreend samepagebegin samepageend
syn keyword Function reh rehearsal num let mnum exit
syn keyword Function ending endending openendending closedendending hidechanges
syn keyword Function line curve to bulge
syn keyword Function ho tie slur up down len slash alt bm ebm esbm pad
syn keyword Function postscript bbox ystemoffset
syn keyword Operator afterprolog beforetrailer atpagebegin atpageend atscorebegin atscoreend file

syn keyword Identifier times avantgarde courier helvetica bookman newcentury palatino

syn keyword Include include fontfile

syn keyword Define ifdef ifndef if then else endif define undef defined eval
syn keyword Define savemacros restoremacros

syn keyword Delimiter bar invisbar dblbar endbar repeatstart repeatend repeatboth restart
syn keyword Delimiter ";"

syn keyword Statement pedal mussym phrase til octave roll
syn keyword Statement staffs vscheme defoct lyricssize sylposition vcombine
syn keyword Statement size staffsep scorepad staffpad chorddist dist dyndist
syn keyword Statement division panelsperpage  gridfret restcombine restsymmult firstpage
syn keyword Statement leftpage rightpage
syn keyword Statement scoresep stafflines ontheline warn numbermrpt printmultnum
syn keyword Statement midlinestemfloat numbermultrpt defaultphraseside
syn keyword Statement gridswhereused gridsatend mingridheight tabwhitebox timeunit
syn keyword Statement topmargin bottommargin botmargin leftmargin rightmargin units
syn keyword Statement packfact packexp staffscale gridscale scale beamslope slope tupletslope
syn keyword Statement transpose addtranspose lyricsalign pageheight pagewidth pagesize
syn keyword Statement endingstyle rehstyle pedstyle release
syn keyword Statement brace bracket barstyle subbarstyle aboveorder beloworder betweenorder
syn keyword Statement key time beamstyle visible whereused measnum cancelkey label label2
syn keyword Statement measnumfont measnumfontfamily measnumsize
syn keyword Statement font fontfamily lyricsfont lyricsfontfamily extendlyrics clef alignped
syn keyword Statement noteheads stemlen stemshorten swingunit
syn keyword Statement lyricsdist maxscores useaccs carryaccs measnumstyle
syn keyword Statement align
syn keyword Statement alignlabels chordtranslation printedtime
syn keyword Statement emptymeas withfont withfontfamily withsize
syn keyword Statement slashesbetween bracketrepeats repeatdots alignrests
syn keyword Statement indentrestart flipmargin minalignscale
syn keyword Statement defaultkeymap endingkeymap labelkeymap lyricskeymap
syn keyword Statement printkeymap rehearsalkeymap textkeymap withkeymap
syn keyword Statement a4freq tuning acctable
syn keyword Statement leftspace noteinputdir
syn keyword Statement saveparms restoreparms
syn keyword Statement any using

let b:current_syntax = "mup"
