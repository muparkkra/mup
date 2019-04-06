AM_CFLAGS = -I../include @EXTRA_CFLAGS@ $(optflags)
AM_LDFLAGS = -lm
bin_PROGRAMS = mup
BUILT_SOURCES = ../include/muschar.h ../include/extchar.h ../include/ssvused.h ../include/ytab.h lex.c ifgram.c ytab.c fontdata.c prolog.c musfont.c
EXTRA_DIST = lex.l gram.y ifgram.y prolog.ps.in

mup_SOURCES = abshorz.c absvert.c assign.c beaming.c beamstem.c brac.c \
	charinfo.c check.c debug.c ../include/defines.h  \
	errors.c ../include/extchar.h \
	font.c fontdata.c globals.c ../include/globals.h \
	grpsyl.c ifgram.c keymap.c \
	lex.c locvar.c lyrics.c macros.c main.c mainlist.c map.c \
	midi.c midigrad.c miditune.c midiutil.c \
	mkchords.c ../include/muschar.h musfont.c \
	nxtstrch.c parstssv.c parstuff.c \
	phrase.c plutils.c print.c prntdata.c prntmisc.c prnttab.c \
	prolog.c range.c relvert.c restsyl.c roll.c \
	setgrps.c setnotes.c ssv.c ../include/ssvused.h ../include/structs.h \
	stuff.c symtbl.c tie.c trantab.c trnspose.c \
	undrscre.c utils.c ytab.c
mup_LDADD = ../../lib/librational.a

lex.c:	lex.l
	$(LEX) -t lex.l > lex.c

ifgram.c: ifgram.y
	$(BISON) -p if -o ifgram.c ifgram.y

ytab.c ../include/ytab.h: gram.y
	$(BISON) -b y -d -o ytab.c gram.y
	mv ytab.h ../include/ytab.h

fontdata.c:	prolog.ps ../../tools/mup/getfontinfo.ps ../../LICENSE
	echo "/*" > fontdata.c
	$(CAT) ../../LICENSE >> fontdata.c
	echo " */" >> fontdata.c
	$(GS) -sDEVICE=nullpage -sOutputFile=/dev/null -dQUIET - < ../../tools/mup/getfontinfo.ps >> fontdata.c
	$(CAT) charnames fontinit >> fontdata.c
	rm charnames fontinit

prolog.c:	prolog.ps ../../tools/mup/mkprolog
	../../tools/mup/mkprolog < prolog.ps > prolog.c

musfont.c prolog.ps:	prolog.ps.in ../include/muschar.h ../../tools/mup/bbox 
	../../tools/mup/bbox
