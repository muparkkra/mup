AM_CFLAGS = -I../include @EXTRA_CFLAGS@ $(optflags)
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
mup_LDADD = ../../lib/librational.a -lm

# The cd ../.. and using paths to src/mup is so the generated #line directives
# have src/mup in the paths, so that find-debuginfo.sh can find them
# when building a debuginfo RPM. 
lex.c:	lex.l
	(cd ../.. ; $(LEX) -o src/mup/lex.c src/mup/lex.l)

ifgram.c: ifgram.y
	(cd ../.. ; $(BISON) -p if -o src/mup/ifgram.c src/mup/ifgram.y)

ytab.c ../include/ytab.h: gram.y
	(cd ../.. ; $(BISON) -b y -d -o src/mup/ytab.c src/mup/gram.y)
	mv ytab.h ../include/ytab.h

fontdata.c:	prolog.ps ../../tools/mup/getfontinfo.ps ../../LICENSE
	echo "/*" > fontdata.c
	$(CAT) ../../LICENSE >> fontdata.c
	echo " */" >> fontdata.c
	$(GS) -sDEVICE=nullpage -sOutputFile=/dev/null -dQUIET - < ../../tools/mup/getfontinfo.ps | $(SED) -e "/Warning:/d" >> fontdata.c
	$(CAT) charnames fontinit >> fontdata.c
	rm charnames fontinit

prolog.c:	prolog.ps ../../tools/mup/mkprolog
	../../tools/mup/mkprolog < prolog.ps > prolog.c

musfont.c prolog.ps:	prolog.ps.in ../include/muschar.h ../../tools/mup/bbox 
	../../tools/mup/bbox
