#!/bin/sh

# This is a script to create the PostScript to make a picture of each music
# character, for including in the Mup User's Guide and Quick Reference.
# It expects to have Mup's muschar.h file as its stdin.
# $1 needs to be the path to prolog.ps

cat - > muschar.ps <<!
%!PS-Adobe-1.0
%%BoundingBox: Calculate later
/boxheight 60 def
/boxwidth 47 def
/nameheight 10 def
/tmpstring 50 string def
/flagsep 1.6 300 mul def

/Times-Roman findfont nameheight 2 sub scalefont setfont
/prm {
	/yoff exch def
	/xoff exch def
	/sym exch def
	/name sym tmpstring cvs def
	/realboxheight boxheight extra add def

	save
	xoff yoff translate

	% draw box
	newpath
	0 0 moveto
	boxwidth 0 lineto
	boxwidth realboxheight lineto
	0 realboxheight lineto
	closepath
	stroke

	% print name of music character
	gsave
	nameheight 4 sub
	realboxheight name stringwidth pop sub 2 div 4 add moveto
	55 rotate
	0 0.25 0.75 setrgbcolor
	name show
	grestore
	% display the music character
	boxwidth 2 div nameheight add 2 sub
	realboxheight nameheight sub 2 div 2 add
	1.0 sym cvx exec
	restore
} def

/offset 2 def
/col 0 def
/row 11 def
/extra 0 def
/extraoffset 0 def

% go to next display slot on the page and show one music character
/showone {
	col boxwidth mul offset add row boxheight mul
		extra sub extraoffset sub prm
	/col col 1 add def
	% go to next column when current one is full
	col 12 ge { /col 0 def /row row 1 sub def } if
	% A few rows have to be extra tall, others shorter.
	% The extra is how much to adjust height from the norm.
	% To get each extraoffset, add the extra from the previous.
	% Rows are numbered from the number, starting from 1
	row 7 eq { /extra -15 def } if
	row 6 eq { /extraoffset -15 def /extra -5 def } if
	row 5 eq { /extraoffset -20 def /extra 35 def } if
	row 4 eq { /extraoffset 15 def /extra 0 def } if
	row 3 eq { /extraoffset 15 def /extra 0 def } if
} def

!

# get the list of music characters from the header file
grep "#define C" - | sed -e "s/#define C_//" -e 's/	.*$//' | dd conv=lcase 2>/dev/null  | sed -e 's,^,/,' -e 's/$/ showone/' >> muschar.ps

# add in PostScript trailer
echo showpage >> muschar.ps

# set BoundingBox
echo quit | cat $1 muschar.ps - | gs -sDEVICE=bbox -dNOPAUSE - >/dev/null 2> mcbboxinfo
bbox=$(head -1 mcbboxinfo)
sed -i -e "/BoundingBox/s/.*/$bbox/" muschar.ps
rm mcbboxinfo
