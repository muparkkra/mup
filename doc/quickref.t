.\"pl 11.0i
.\"ll 8.5i
.de Ph
.sp 1.5
.if \\n%>1 \{
.ev 1
.ce
.ps 9
.ft R
- \\n% -
.ev
\}
.sp
..
.wh 0 Ph
.nf
.na
.po -0.5i
.ll +0.4i
.lt +0.4i
.vs +5
.ps +1
.ce
.ft B
Mup Version 6.8 Statement Summary
.sp
\fIcontext\fR
\fIstaffs voices & staffs voices\fB:\fI chord \fB; \fI....\fR
\fB[\fIstaffs voices & staffs voices\fB; \fIstaffs voices\fB] :\fI chord \fB; \fI....\fR
\fBlyrics \fIplace staff \fBusing \fIstaff voice\fB:\fI time_valuelist \fB[\fI verses \fB] "\fItext\fB"; \fI...\fR
\fIlinetype bartype \fBpad \fInum ending_mark \fBrehearsal \fIfontfamily font \fB(\fIsize\fB) \fIrehearsal_mark \fBmnum=\fInum \fB=\fItag \fBhidechanges\fR
\fIparameter\fB=\fIvalue
\fBmultirest \fInum\fR
\fBnewscore leftmargin=\fInum\fB rightmargin=\fInum\fR 
\fBnewpage leftmargin=\fInum\fB rightmargin=\fInum\fR
\fIfontfamily font \fB(\fIsize\fB) \fImodifier place staffs \fBdist\fI num \fB! \fBalign \fInum \fB: \fIbeat \fB"\fItext\fB" til \fItil_value\fB; \fI....
\fBmussym (\fIsize\fR) \fIplace staffs \fB: \fIbeat \fB"\fImus_symbol\fB" til \fItil_value\fB; \fI....\fR
\fIlinetype \fBphrase \fIplace staffs \fB: \fIbeat \fBtil \fItil_value\fB; \fI....\fR
\fBoctave \fIplace staffs \fB: \fIbeat \fB"\fItext\fB" til \fItil_value\fB; \fI....\fR
\fBpedal \fIplace staffs \fB: \fIbeat \fB* ; \fI....\fR
\fIcres_mark place staffs \fB: \fIbeat \fBtil \fItil_value\fB; \fI....\fR
\fBmidi \fIstaffs voices \fB:\fI beat \fB"\fImidi_keyword\fB=\fIvalue\fB" til \fItil_value\fB; \fI....\fR 
\fBroll \fIdirection staffnum voicenum \fBto \fIstaffnum voicenum \fB:\fI num \fB; \fI....\fR
\fIprintcmd location \fB"\fItext\fB"\fR
\fBpostscript \fIpostscript_location \fBwith \fItag \fB= \fItag \fBfile "\fItext\fB"\fR
\fBtitle \fIfontfamily font \fB(\fIsize\fB) "\fItext\fB" "\fItext\fB" "\fItext\fB"\fR
\fIjustifytype \fBparagraph \fIfontfamily font \fB(\fIsize\fB) "\fItext\fB"
\fIlinetype \fBline\fI location \fBto\fI location\fR \fBwith \fIfontfamily font \fB(\fIsize\fB) "\fItext\fB"\fR
\fIlinetype \fBcurve\fI location \fBto\fI location \fBto\fI location ....\fR
\fIlinetype \fBcurve\fI location \fBto\fI location \fBbulge\fI num ....\fR
\fBdefine\fI MACRO_NAME\fB(\fImac_params\fB) \fImacro_definition \fB@\fR
\fBifdef \fIMACRO_NAME ..... \fBelse\fI ..... \fBendif\fR
\fBifndef \fIMACRO_NAME ..... \fBelse\fI ..... \fBendif\fR
\fBundef \fIMACRO_NAME\fR
\fBexit\fR
\fBsavemacros "\fIname\fB"\fR
\fBrestoremacros "\fIname\fB"\fR
\fBif \fIexpression ..... \fBelse\fI ..... \fBendif\fR
\fBinclude "\fIfilename\fB"\fR
\fBfontfile "\fIfilename\fB"\fR
\fB// \fIcomment\(emarbitrary text that will be ignored.
.bp
.vs -4
.ps -1
.ce
\fBValues used in Mup statements\fR
.sp
.ta 1.2i 2.6i 4.0i 5.4i
\fIaccidental	\fB#\fR, \fB&\fR, \fBx\fR, \fB&&\fR, \fBn\fR or \fB{"\fIacc_name\fB"}
\fIacc_spec	\fRaccidental symbol name in quotes followed by \fIpitch_spec\fR
\fIbartype	\fBbar\fR, \fBdblbar\fR, \fBrepeatstart\fR, \fBrepeatend\fR, \fBrepeatboth\fR, \fBendbar\fR, \fBinvisbar\fR, or \fBrestart\fR
\fIbeat	\fRnumber from 0.0 to time signature denominator plus 1.0, optionally followed
	by how many grace notes to back up from there, as a negative number in parentheses,
	and an offset as a signed number in square brackets
\fIchord	\fB<<\fIparm_contxt parameter=value\fB>> [\fIchord_style\fB] \fItime_value pitch(es) note_attributes inter-chord_attr\fR
	Tuplets specified by   \fB{ \fIchord \fB; \fI.... \fB} \fIside num tupstyle, time_value \fB;\fR
	On tablature staff, the \fIpitch\fR is: \fIstring fret \fB"\fIbend\fB"\fR
\fIbend	\fBfull\fR, or \fInum\fR and/or a fraction as \fInum\fB/\fInum\fR
\fIchord_style	\fRone or more (semicolon-separated) from the following:\fR
	\fR\fBgrace\fR, \fBcue\fR, \fBxnote\fR, \fBdiam\fR, \fBwith \fIwithlist\fR, \fBpad \fInum\fR, \fBslash \fInum\fR, \fIstemdir\fR, \fBlen \fInum\fR, \fBdist\fR \fInum\fR,
	\fBho \fInum\fR, \fBhs "\fIheadshape\fB"\fR, or \fB=\fItag\fR
	\fRPutting \fB...\fR after the \fB]\fR will repeat the \fIchord_style\fR until \fB[-]\fR or end of measure.
\fIclef	\fBtreble\fR, \fBsoprano\fR, \fBmezzosoprano\fR, \fBalto\fR, \fB8bass   \fR(these have default octave of 4)\fR
	\fBtreble8\fR, \fBtenor\fR, \fBbaritone\fR, \fBbass  \fR(these have default octave of 3)\fR
	\fBfrenchviolin\fR, \fB8treble\fR (these have default octave of 5)
	\fBbass8\fR, \fBsubbass\fR   (these have default octave of 2)
\fIcontrol_command	\fBsaveparms "\fIname\fB" \fRor \fBrestoreparms "\fIname\fB"\fR
\fIcontext\fR	One of the following:
	\fIparm_context\fR	followed by list of parameters
	\fBacctable "\fIacc_table_name\fB"	\fRfollowed by \fIacc_spec\fR
	\fBcontrol	\fRfollowed by \fIcontrol_commands\fR
	\fBmusic\fR	followed by commands for printing music
	\fIheadfoot_context\fR	followed by commands for printing text
	\fBblock\fR	followed by \fBparagraph\fP commands
	\fBgrids\fR	followed by pairs of strings: name and frets
	\fBheadshapes\fR	followed by pairs of strings: name and list of 4 note head characters
	\fBkeymap "\fIkeymap_name\fB"	\fRfollowed by pairs of strings: pattern and replacement
	\fBsymbol\fR	followed by
		    \fBbbox=\fInum\fB,\fInum\fB,\fInum\fB,\fInum\fR
		    \fBpostscript="\fItext\fB"
		    \fIystemoffset=\fInum\fB,\fInum\fR
\fIcres_mark	\fB<\fR (crescendo) or \fB>\fR (decrescendo)
\fIdirection	\fBup\fR or \fBdown\fR
\fIending_mark	\fBending "\fItext\fB" \fR or \fBendending\fR or \fBopenendending\fR or \fBclosedendending\fR
\fIexpression	\fRcan contain \fBdefined()  ()  +  -  ~  !  *  /  %  ^  &  |  <<  >>  <  >  <=  >=  ==  !=  &&  ||  ?:\fR
\fIfilename	\fRpath to a file on your system; uses MUPPATH to find, if not in current directory
	and not absolute path
\fIfont	\fBrom\fR, \fBital\fR, \fBbold\fR, or \fBboldital\fR
\fIfontfamily	\fBavantgarde\fR, \fBbookman\fR, \fBcourier\fR, \fBhelvetica\fR, \fBnewcentury\fR, \fBpalatino\fB, or \fBtimes\fR
\fIfret	\fRnumber from 0 to 99
\fIheadfoot_context	\fBheader, footer, header2, footer2, top, bottom, top2, \fRor \fBbottom2\fR
\fIhex_data	\fReven number of hexadecimal digits\fR
\fIinter-chord_attr	\fItieslur_style \fBtie\fR \fIdirection\fR, \fItieslur_style \fBslur\fR \fIdirection\fR, \fBbm\fR, \fBbm with staff \fIside\fR, \fBebm\fR, \fBesbm\fR,
	\fBabm\fP, \fBeabm\fP, \fBslope\fR \fInum\fR, \fBph \fI side\fR, \fBeph\fR, or \fBalt \fInum\fR
\fIinterval	\fBperfect\fR, \fBmajor\fR, \fBminor\fR, \fBaugmented\fR, or \fBdiminished\fR
\fIjustifytype	\fBjustified\fR, or \fBragged\fR
\fIlinetype	\fBmedium\fR, \fBwide\fR, \fBdashed\fR, or \fBdotted\fR (if omitted, \fIlinetype\fR is narrow)
\fIlocation	\fB(\fIx_expr, y_expr \fB)\fR can contain \fItag\fR, \fIsteps\fR, \fBtime \fInum\fR, \fBsqrt\fR, \fBsin\fR, \fBcos\fR, \fBtan\fR, \fBasin\fR, \fBacos\fR, \fBatan\fR, \fBatan2\fR, \fBhypot
\fImac_params	\fRcomma-separated list of parameter names, same naming rules as \fIMACRO_NAME\fR
\fIMACRO_NAME	\fRupper case letters, digits, and/or underscores, beginning with a letter
\fImacro_definition	\fRarbitrary text that will be used wherever \fIMACRO_NAME\fR appears in input
.bp
\fImidi_keyword	\fBtempo=\fInum\fR	\fBprogram=\fInum\fR	\fBparameter=\fInum\fB,\fInum\fR	\fBport=\fInum\fR
	\fBonvelocity=\fInum\fR	\fBoffvelocity=\fInum\fR	\fBchannel=\fInum\fR	\fBchanpressure=\fInum\fR
	\fBseqnum=\fInum\fR	\fBtext=\fItext\fR	\fBcopyright=\fItext\fR	\fBname=\fItext\fR
	\fBinstrument=\fInum\fR	\fBmarker=\fItext\fR	\fBcue=\fItext\fR	\fBhex=\fIhex_data\fR
\fImodifier	\fBchord\fR, \fBanalysis\fR, \fBfigbass\fR, or omitted
\fImus_symbol	\fRmusical symbol; see chart on page 8 for complete list.
\fInote_attributes	\fRone or more of the following: \fItieslur_style \fB~ \fIdirection \fR(tie), \fItieslur_style \fB< >\fR \fIdirection) (slur),
	\fB? \fR(small), \fB^\fIpitch\fR (bend), \fBhs "\fIheadshape\fB"\fR, or \fB=\fItag\fR
\fInum	\fRsome number; valid values depend on where it appears
\fIoctave	\fRnumber from \fB0\fR to \fB9\fR or one or more \fB+\fR or \fB-\fR signs
\fIparameter	\fRSee parameter table on page 5 for complete list
\fIparm_context	\fBscore\fR or \fBstaff \fInum(s) \fRor \fBvoice \fIstaffnum(s) voicenum(s)\fR
\fIpitch	\fRletter from \fBa\fR to \fBg\fR, optionally followed by \fIaccidental\fR and/or \fIoctave\fR,
	or \fBr\fR (rest), \fBs\fR (space), \fBus\fR (uncollapseable space), or \fBrpt\fR (repeat)
\fIpitch_adjustment	\fB+\fInum \fBcents\fR
	or \fB-\fInum \fBcents\fR
	or \fInum \fB/ \fInum\fR
	or \fInum\fR
\fIpitch_spec\fR	\fBall \fIpitch_adjustment\fR
	or one or more instances of a letter a-g followed by a \fIpitch_adjustment\fR
\fIplace	\fBabove\fR, \fBbelow\fR, or \fBbetween\fR. If \fBbetween\fR, \fIstaffs\fR must be of the form \fInum\fB&\fInum\fR
\fIprintcmd	\fBleft\fR, \fBright\fR, \fBcenter\fR, or \fBprint\fR
\fIpostscript_location	location \fRor \fBafterprolog\fR, \fBbeforetrailer\fR, \fBatpagebegin\fR, \fBatpageend\fR, \fBatscorebegin\fR, or \fBatscoreend\fR
\fIrehearsal_mark	\fBlet\fR, \fBnum\fR, \fBmnum\fR, or \fB"\fItext\fB"\fR
\fIside	\fBabove\fR or \fBbelow\fR
\fIsize	\fRnumber from 1 to 100 (points)
\fIstaffnum	\fRnumber from 1 to 40
\fIstaffs	\fRnumber from 1 to 40, or comma-separated list of dash-separated ranges
\fIstemdir	\fBup\fR or \fBdown\fR
\fIstemlen	\fRa number, in stepsizes
\fIsteps	\fRa number of stepsizes, where a stepsize is 1/2 the distance between two staff lines
\fIstring	\fRin tablature chord: \fIpitch accidental\fR followed by zero or more \fB'\fR marks
\fItab_string	\fRlist of tablature strings. Each includes a \fIpitch\fR, optional \fB'\fR marks, and optional \fIoctave.\fR
\fItag	\fRa lower case letter, or an underscore followed by lower case letters, digits, and/or underscores
	\fREach has 6 associated values: \fBx\fR, \fBy\fR, \fBn\fR, \fBs\fR, \fBe\fR, and \fBw\fR
	\fRPredefined tag names are \fB_win\fR, \fB_page\fR, \fB_score\fR, \fB_staff.\fIN\fR and \fB_cur\fR
\fItext	\fRarbitrary text; use \fB\e"\fR to include quotes, \fB\ef(\fIfontfamily font\fB)\fR to change font, \fB\es(\fInum\fB)\fR to change size,
	\fB\e(\fIxx\fB)\fR to include special characters, \fB\ev(\fInum\fB)\fR for vertical motion, \fB\e:\fR to toggle piling, \fB\e|\fP or \fB\e^\fR for alignment
\fItime_value	\fB1/8\fR, \fB1/4\fR, \fB1/2\fR, \fB1\fR, \fB2\fR, \fB4\fR, \fB8\fR, \fB16\fR, \fB32\fR, \fB64\fR, \fB128\fR, or \fB256\fR with optional dots, \fB+\fR or \fB-\fR; or \fBm\fR (measure)
\fItieslur_style	\fBdotted\fR, \fBdashed\fR, or omitted
\fItil_value	num \fRor\fI num\fBm+\fInum\fR, plus optional \fB(-\fInum\fB)\fR and/or signed \fIsteps\fR in square brackets
\fItuplet	\fRSee \fIchord\fR
\fItupstyle\fP	\fBy\fR (always print bracket), \fBn\fR (no number or bracket), \fBnum\fR (number only), or omitted (default)
\fIvalue	\fRSee information about the item to the left of the = for valid values
\fIverses	\fRnumber or comma-separated list of dash-separated ranges
\fIvoicenum	\fB1\fR, \fB2\fR, or \fB3\fR
\fIvoices	comma-separated list of dash-separated ranges of \fIvoicenum\fRs
\fIwithlist	\fRcomma-separated list of the following: \fB. \fR(stacatto), \fB- \fR(legato), \fB^ \fR(accent), \fB> \fR(accent), or \fB"\fItext\fB"\fR
\fIX	\fBx\fR, \fBw\fR, or \fBe\fR  (x, west, or east)
\fIY	\fBy\fR, \fBn\fR, or \fBs\fR  (y, north, or south)
.bp
.vs +1
.ce
\fBFont/size changes in text strings\fR
.sp
To change font inside a quoted text string, use \fB\ef(\fIfontfamily font\fB)\fR or \fB\ef(\fIfont_abbr\fB)\fR from the table:
.TS
c c s s s s s s
l c c c c c c c.
\fIfont\fR	\fIfontfamily\fR 
	avantgarde	bookman	courier	helvetica	newcentury	palatino	times
_
\fRrom\f(CW	AR	BR	CR	HR	NR	PR	TR
\fBbold\f(CW	AB	BB	CB	HB	NB	PB	TB
\fIital\f(CW	AI	BI	CI	HI	NI	PI	TI
\f(BIboldital\f(CW	AX	BX	CX	HX	NX	PX	TX
.TE
\fB\ef(previous) \fRor \fB\ef(PV) \fRchanges to previous font.
\fB\ef(rom) \fRor \fB\ef(R) \fRchanges to roman in current family.
\fB\ef(ital) \fRor \fB\ef(I) \fRchanges to ital in current family.
\fB\ef(bold) \fRor \fB\ef(I) \fRchanges to bold in current family.
\fB\ef(boldital) \fRor \fB\ef(X) \fRchanges to boldital in current family.
\fB\es(\fInum\fB) \fRchanges to point size \fInum\fR, 1 to 100.
\fB\es(+\fInum\fB) \fRincreases the size by \fInum\fR while \fB\es(-\fInum\fB) \fRdecreases it by \fInum\fR points.
\fB\es(PV)\fR or \fB\es(previous)\fP reverts to previous size.
.sp
.ce
\fBMup command line arguments\fR
.sp
.ta 2.0i
\fB-c \fInum	\fRcombine \fInum\fR or more measures of rest into multirest\fR
\fB-C\fR	used with -E, keep comments rather than discarding them
\fB-d \fInum	\fRturn on debugging level \fInum\fR
\fB-D \fIMACRO\fB=\fIdef	\fRdefine \fIMACRO\fR as \fIdef\fR
\fB-e \fIerrfile	\fRput error output into \fIerrfile\fR
\fB-E\fR	just expand macros and "include" files and write result to standard output
\fB-f \fIoutfile	\fRput output into \fIoutfile\fR
\fB-F\fR	put output into file, deriving output file name from input file name
\fB-l\fR	print the Mup license and exit
\fB-m \fImidifile	\fRgenerate MIDI output into \fImidifile\fR
\fB-M	\fRgenerate MIDI output, derive file name\fR
\fB-o \fIpagelist	\fRonly print pages in \fIpagelist\fR, list of numbers or ranges, optional \fBodd\fP or \fBeven\fP, or \fBreversed\fP
\fB-p \fInum	\fRstart numbering pages at \fInum\fR
\fB-q	\fRquiet mode; omit version and copyright notice on startup
\fB-s \fIstafflist	\fRprint only the staffs listed in \fIstafflist\fR; add \fBv\fIN\fR to restrict to voice \fIN\fR
\fB-v	\fRprint version number and exit
\fB-x \fIM\fB,\fIN\fR	extract measures \fIM\fR through \fIN\fR, negative relative to end, 0 for pickup
.sp
.ce
\fBMupdisp commands\fR
.sp
.ta 3.5i
\fInum\fR<Enter>	go to page \fInum\fR
\fB+\fR or <space> or <cntl-E> or <cntl-F>	forward 1/8"
\fBf\fR or <Enter> or <cntl-D> or <cntl-N>	forward 1"
\fB-\fR or <backspace> or <cntl-Y> or <cntl-B>	backward 1/8"
\fBb\fR or <cntrl-U> or <cntl-P> or <cntl-B>	backward 1"
\fBh\fR or \fB?\fR	help
\fBm\fR	toggle between full and partial page modes
\fBn\fR	next page
\fBp\fR	previous page
\fBq\fR or \fBZZ\fR	quit
\fBr\fR	repaint the screen
.vs -1
.bp
.ce
\fBMup Parameters\fR
.ll +0.5i
.sp 2
.ps -2
.vs +2
.TS
allbox;
c c c c c c c
lB c c c c l l.
Parameter	Score	Staff  	Voice	Hd/Ft	Valid Values	Default
_
a4freq	\(bu				100.0 to 1000.0	440.0
aboveorder	\(bu	\(bu			\fBmussym,octave,dyn&othertext&chord,lyrics,ending,reh	\fRas listed
acctable	\(bu				\fB"\fItable_name\fB"	""
addtranspose	\(bu	\(bu			\fBup\fR or \fBdown\fR, followed by \fIinterval\fR and \fInum\fR, then optional \fBnotes\fP or \fBchords\fR	up perfect 1
alignlabels	\(bu				\fBcenter\fR, or \fBleft\fR, or \fBright\fR	right
alignped	\(bu	\(bu			\fBy\fR or \fBn\fR	y
alignrests	\(bu	\(bu	\(bu		\fBy\fR or \fBn\fR	n
barstyle	\(bu				optional \fBbetween\fP comma-separated lists of staffs or dash-separated ranges, or \fBall\fP	
beamslope	\(bu	\(bu	\(bu		0.0 to 1.0, 0.0 to 45.0	0.7,20.0
beamstyle	\(bu	\(bu	\(bu		comma-separated list of \fItime_values\fR that add up to a measure	
beloworder	\(bu	\(bu			\fBmussym,octave,dyn&othertext&chord,lyrics,pedal	\fRas listed
betweenorder	\(bu	\(bu			\fBmussym,dyn&othertext&chord,lyrics	\fRas listed
bottommargin	\(bu				0.0 to pageheight minus 0.5 inches	0.5 inches
brace	\(bu				comma-separated list of staffs or dash-separated ranges	
bracket	\(bu				comma-separated list of staffs or dash-separated ranges	
bracketrepeats	\(bu				\fBy\fR or \fBn\fR	n
cancelkey	\(bu	\(bu			\fBy\fR or \fBn\fR	n
carryaccs	\(bu	\(bu			\fBy\fR or \fBn\fR	y
chorddist	\(bu	\(bu			0.0 to 50.0 (stepsizes)	3.0
chordtranslation	\(bu	\(bu			nothing or "German" or string like "do re mi fa sol la si"	
clef	\(bu	\(bu			T{
\s-2\fBtreble\fB,\fBtreble8\fR,\fB8treble\fR,\fBfrenchviolin\fR,\fBsoprano\fR,\fBmezzosoprano\fR,\fBalto\fR,\fBtenor\fR,\fBbaritone\fR,\fBbass\fR,\fB8bass\fR,\fBbass8\fR,\fBsubbass\fR\s+2
T}	treble
cue	\(bu	\(bu	\(bu		\fBy\fR or \fBn\fR	n
defaultkeymap	\(bu	\(bu			\fB"\fIkeymap_name\fB"	\fR""
defoct	\(bu	\(bu	\(bu		0 to 9	based on clef
dist	\(bu	\(bu			0.0 to 50.0 (stepsizes)	2.0
division	\(bu				MIDI division, 1 to 1536 (ticks per quarter note)	192
dyndist	\(bu	\(bu			0.0 to 50.0 (stepsizes)	2.0
emptymeas	\(bu	\(bu	\(bu		string containing music input	"ms;"
endingkeymap	\(bu	\(bu			\fB"\fIkeymap_name\fB"	\fR""
endingstyle	\(bu				\fBtop\fR, \fBbarred\fR, or \fBgrouped\fR	top
extendlyrics	\(bu	\(bu	\(bu		\fBy\fR or \fBn\fR	n
firstpage	\(bu				1 to 5000	1
flipmargins	\(bu				\fBy\fR or \fBn\fR	n
font	\(bu	\(bu		\(bu	\fBrom\fR, \fBbold\fR, \fBital\fR, or \fBboldital\fR	rom
fontfamily	\(bu	\(bu		\(bu	T{
\fBavantgarde\fR, \fBbookman\fR, \fBcourier\fR, \fBhelvetica\fR, \fBnewcentury\fR, \fBpalatino\fR, or \fBtimes\fR
T}	times
gridfret	\(bu	\(bu			2 to 99	4
gridsatend	\(bu	\(bu			\fBy\fR or \fBn\fP	n
gridswhereused	\(bu	\(bu			\fBy\fR or \fBn\fP	n
gridscale	\(bu	\(bu			0.1 to 10.0	1.0
.TE
.TS
allbox;
c c c c c c c
lB cw(0.3i) cw(0.3i) cw(0.3i) cw(0.3i) lw(3.5i) l.
Parameter	Score	Staff  	Voice	Hd/Ft	Valid Values	Default
_
indentrestart	\(bu				\fBy\fR or \fBn\fP	n
key	\(bu	\(bu			T{
\fIpitch\fR \fBmajor\fR or \fBminor\fR; or 0 to 7 \fB#\fR or \fB&\fR and optional \fBmajor\fR or \fBminor\fR
T}	c major
label2	\(bu	\(bu			\fB"\fItext\fB"\fR	""
label	\(bu	\(bu			\fB"\fItext\fB"\fR	"            "
labelkeymap	\(bu	\(bu			\fB"\fIkeymap_name\fB"	\fR""
leftmargin	\(bu				0.0 to pagewidth minus 0.5 inches	0.5 inches
leftspace	\(bu				0.0 to 0.5, 0.0 to 100.0	0.15, 5.0
lyricsalign	\(bu	\(bu			0.0 to 1.0	0.25
lyricsdist	\(bu	\(bu			0.0 to 50.0 (stepsizes)	2.0
lyricsfont	\(bu	\(bu			\fBrom\fR, \fBbold\fR, \fBital\fR, or \fBboldital\fR	rom
lyricsfontfamily	\(bu	\(bu			T{
\fBavantgarde\fR, \fBbookman\fR, \fBcourier\fR, \fBhelvetica\fR, \fBnewcentury\fR, \fBpalatino\fR, or \fBtimes\fR
T}	times
lyricskeymap	\(bu	\(bu			\fB"\fIkeymap_name\fB"	\fR""
lyricssize	\(bu	\(bu			1 to 100 (points)	12
maxmeasures	\(bu				1 to 1000	1000
maxscores	\(bu				1 to 1000	1000
measnum	\(bu				\fBy\fR, \fBn\fR, or \fBevery \fIN\fR	n
measnumfont	\(bu				\fBrom\fR, \fBbold\fR, \fBital\fR, or \fBboldital\fR	rom
measnumfontfamily	\(bu				T{
\fBavantgarde\fR, \fBbookman\fR, \fBcourier\fR, \fBhelvetica\fR, \fBnewcentury\fR, \fBpalatino\fR, or \fBtimes\fR
T}	times
measnumsize	\(bu				1 to 100 (points)	11
measnumstyle	\(bu				\fBboxed\fR, \fBcircled\fR, or \fBplain\fR	plain
minalignscale	\(bu	\(bu			0.1 to 1.0	0.667
mingridheight	\(bu	\(bu			2 to 99	4
noteheads	\(bu	\(bu	\(bu		string containing 1 or 7 headshape names	"norm"
noteinputdir	\(bu	\(bu	\(bu		\fBup\fP, \fBdown\fR, or \fBany\fP	any
numbermrpt	\(bu	\(bu			\fBy\fR or \fBn\fR	y
ontheline	\(bu	\(bu	\(bu		\fBy\fR or \fBn\fR	y
packexp	\(bu				0.0 to 1.0	0.8
packfact	\(bu				0.0 to 10.0	1.0
pad	\(bu	\(bu	\(bu		-5.0 to 50.0	0.0
pageheight	\(bu				2.0 to 24.0 inches or 5.0 to 61.0 cm	11.0 inches
pagesize	\(bu				\fBletter, legal, flsa, halfletter, a4, a5, a6\fR then optional \fBportrait\fR or \fBlandscape\fR	letter
pagewidth	\(bu				2.0 to 24.0 inches or 5.0 to 61.0 cm	8.5 inches
panelsperpage	\(bu				1 or 2	1
pedstyle	\(bu	\(bu			\fBline\fR, \fBpedstar\fR, or \fBalt pedstar\fR	line
printedtime	\(bu	\(bu			times signature or one or two strings	
printkeymap	\(bu			\(bu	\fB"\fIkeymap_name\fB"	\fR""
printmultnum	\(bu	\(bu			\fBy\fR or \fBn\fR	y
rehearsalkeymap	\(bu	\(bu			\fB"\fIkeymap_name\fB"	\fR""
rehstyle	\(bu	\(bu			\fBboxed\fR, \fBcircled\fR, or \fBplain\fR	boxed
.TE
.TS
allbox;
c c c c c c c
lB cw(0.3i) cw(0.3i) cw(0.3i) cw(0.3i) lw(3.5i) l.
Parameter	Score	Staff  	Voice	Hd/Ft	Valid Values	Default
_
release	\(bu	\(bu	\(bu		0 to 500 (milliseconds)	20
repeatdots	\(bu	\(bu			\fBstandard\fR or \fBall\fR	standard
restcombine	\(bu				2 to 1000	not set
restsymmult	\(bu	\(bu			\fBy\fR or \fBn\fR	n
rightmargin	\(bu				0.0 to pagewidth minus 0.5 inches	0.5 inches
scale	\(bu				0.1 to 10.0	1.0
scorepad	\(bu				\fInum\fR or \fInum\fB,\fInum\fR, negative pageheight to pageheight (stepsizes)	2.0,2.0
scoresep	\(bu				\fInum\fR or \fInum\fB,\fInum\fR, 6.0 to pageheight (stepsizes)	12.0,20.0
size	\(bu	\(bu		\(bu	1 to 100	12
slashesbetween	\(bu				\fBy\fR or \fBn\fR	n
stafflines	\(bu	\(bu			\fB1\fR, \fB1n\fR, \fB5\fR, \fB5n\fR, \fB5 drum\fR, \fB1 drum\fR, or \fBtab (\fItab_strings\fB)\fR optionally followed by \fBy\fR or \fBn\fR	5
staffpad	\(bu	\(bu			negative pageheight to pageheight (stepsizes)	0.0
staffs	\(bu				1 to 40	1
staffscale	\(bu	\(bu			0.1 to 10.0	1.0
staffsep	\(bu	\(bu			6.0 to pageheight (stepsizes)	10.0
stemlen	\(bu	\(bu	\(bu		0.0 to 100.0 (stepsizes)	7.0
stemshorten	\(bu	\(bu	\(bu		0.0 to 2.0, 0.0 to 7.0, -4 to 50, -4 to 50 (stepsizes)	1.0,2.0,1,6
subbarstyle	\(bu				\fIlinetype bartype appearance staff_list \fBtime\fI counts\fP
swingunit	\(bu	\(bu	\(bu		time value or nothing	nothing
sylposition	\(bu	\(bu			-100 to 100 (points)	-5
tabwhitebox	\(bu	\(bu	\(bu		\fBy\fR or \fBn\fR	n
textkeymap	\(bu	\(bu			\fB"\fIkeymap_name\fB"	\fR""
time	\(bu				\fBcommon\fR, \fBcut\fR or \fIN\fB/\fID\fR where \fIN\fR is 1 to 99 and \fID\fR is \fB1\fR, \fB2\fR, \fB4\fR, \fB8\fR, \fB16\fR, \fB32\fR, or \fB64\fR	4/4
timeunit	\(bu	\(bu	\(bu		\fItime_value\fR (can include dots)	\fID\fR of time sig
topmargin	\(bu				0.0 to pageheight minus 0.5 inches	0.5 inches
transpose	\(bu	\(bu			\fBup\fR or \fBdown\fR, followed by \fIinterval\fR and \fInum\fR, then optional \fBnotes\fR or \fBchords\fR	up perfect 1
tuning	\(bu				\fBequal\fR, \fBpythagorean\fR, or \fBmeantone\fR	equal
units	\(bu				\fBinches\fR or \fBcm\fR	inches
useaccs	\(bu	\(bu			\fBn\fR, \fBy none\fR, \fBy all\fR, \fBy nonnat\fR, \fBy noneremuser\fR, \fBy nonnatremuser\fR	n
visible	\(bu	\(bu	\(bu		\fBy\fR, \fBn\fR, or \fBwhereused\fR	y
vcombine	\(bu	\(bu			voice list plus \fBnooverlap\fR, \fBshareone\fR, \fBstepsapart\fR, \fBoverlap\fR, \fBrestsonly\fR, or nothing	nothing
vscheme	\(bu	\(bu			\fB1\fR, \fB2o\fR, \fB2f\fR, \fB3o\fR, or \fB3f\fR	1
warn	\(bu				\fBy\fR or \fBn\fR	y
withfont	\(bu	\(bu	\(bu		\fBrom\fR, \fBbold\fR, \fBital\fR, or \fBboldital\fR	rom
withfontfamily	\(bu	\(bu	\(bu		T{
\fBavantgarde\fR, \fBbookman\fR, \fBcourier\fR, \fBhelvetica\fR, \fBnewcentury\fR, \fBpalatino\fR, or \fBtimes\fR
T}	times
withkeymap	\(bu	\(bu			\fB"\fIkeymap_name\fB"	\fR""
withsize	\(bu	\(bu	\(bu		1 to 100	12
.TE
.bp
.po +0.5i
Music symbols can be used in text strings by using \fB\e(\fIsymbol_name\fB)\fR. Prefix with \fBsm\fR for smaller version.
.ie \n(.g \{
.mk Px
.sp -1
.if \n(.P .PSPIC -L muschar.ps
.po -0.5i
.ps +1
.sp 0.5
.ce 5 \}
.el \{
.sp
.if \n(.P \!x X PI:\n(.o:\n(.i:\n(.l:\n(.t:muschar.ps:9.0,6.2,0.1,0:l:
.po -0.5i
.sp 7.75i
.ps +2
.ce 3 \}
\f(NXArkkra Enterprises\f(NR
http://www.arkkra.com
support@arkkra.com
