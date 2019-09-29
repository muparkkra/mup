# Makefile for Mup (and auxiliary programs mupdisp, mkmupfnt, and mupmate).
# This is not an optimal makefile; instead it tries to be very simple,
# and easy to understand, so it can be easily modified, if needed.

# On most Linux/Unix type systems, just doing
#	make install
# as root may work.
# (You really only need to be root to copy the products into
# the system directories. If you change DESTDIR
# to point to some other writeable area, you wouldn't need to be root.)

# For Apple Mac OS X, see the notes for what to change (CFLAGS and X_LIBS)

# If you only want the Mup program itself, you can do
#	make src/mup/mup
# to compile it, or
# 	make install-mup
# to compile and install it.

# The other programs are optional:

# mupdisp: runs Mup and then runs GhostScript on the result.
#   You can run Mup directly, and use gv, GSview, ghostview or any other
#   PostScript viewer on the Mup output, as an alternative to mupdisp.
#   To compile mupdisp:
#	make src/mupdisp/mupdisp
#   To compile and install mupdisp:
#	make install-mupdisp

# mkmupfnt: is only needed if you want to use fonts beyond the basic
#   standard PostScript fonts, so most people don't need this.
#   To compile mkmupfnt:
#	make src/mkmupfnt/mkmupfnt
#   To compile and install mkmupfnt:
#	make install-mkmupfnt

# mupmate: is a graphical user interface front end for Mup.
#   It is not needed if you intend to only use Mup via command line interface.
#   To compile mupmate:
#	make src/mupmate/mupmate
#   To compile and install mupmate:
#	make install-mupmate

# The mupdisp and mupmate programs require X libraries and headers (www.x.org).
# The mupmate program requires FLTK 1.x libraries and headers (www.fltk.org).

# If you want mupdisp to support Linux console mode, make sure you have
# the svgalib package installed, then find the two commented-out lines below
# related to Linux console support, and uncomment them.

# If you are building on a system that does not support make,
# you can look at what this makefile does for how to build.
# Mup itself only needs a C compiler and the standard math library.
# So you could just cd to the src/mup directory and run your C compiler on
# all the .c files in directory plus lib/rational.c,
# pull in headers from ../include, and link with the math library.
# Typically this would be done from command line something like this:
#	cd src/mup
#	cc -I../include *.c ../../lib/rational.c -lm
# You can then copy the resulting mup executable to somewhere in your PATH:
#	cp mup /usr/bin/mup
# or perhaps:
#	cp mup $HOME/bin/mup

# Lines you might conceivably want to change have comments by them,
# explaining how you might want to change them.
# Any line without any comment by it is very unlikely to ever need changing.

#-----------------------------------------------------------------------

# These define where to put the Mup products.
# You could change this to /usr/local or your own space if you like.
# A .deb package build sets DESTDIR; in other environments,
# it will likely be empty.
PREFIX = $(DESTDIR)/usr
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
ICONDIR = $(PREFIX)/share/pixmaps/mup
LIBDIR = $(PREFIX)/lib/mup
DOCDIR = $(PREFIX)/share/doc/mup

# This is the name of your C compiler.
# cc would be another common choice.
CCOMPILER = gcc

# This is the name of your C++ compiler.
# CC would be another common choice. It is only used for mupmate
CPPCOMPILER = g++

# -O option turns on optimization for most C compilers.
# You can add other options, if you like, as appropriate for your C compiler.
# Another common addition would be -g to get debugging information.
# For Mac OS X, you should probably add
#	-Dunix
# and if you want universal binaries, add
#	-arch i386 -arch ppc
# and if you want backward compatibility to older versions, add something like
#	-mmacosx-version-min=10.1
# Or in other words:
# CFLAGS = -O -Dunix -arch i386 -arch ppc -mmacosx-version-min=10.1
# You can set optflags in your environment or via arguments to make
# to add C compiler options without needing to edit this makefile.
CFLAGS = -O $(optflags)

# You can change this if your X libraries and headers are somewhere else
# (like /usr/X11R6).
X_LOCATION = /usr/X11

# If you installed fltk somewhere other than $(X_LOCATION)/lib
# set this to find them, as in
# FLTK_LIB_LOCATION = -L/usr/local/lib
FLTK_LIB_LOCATION =

# The X libraries to link with Mupmate.
# Depending on how fltk was compiled, you may be able to omit
# -lXext, -Xft, and -lXinerama
X_LIBS = -lXext -lX11 -lXpm -lXft -lXinerama
# On Mac OS X replace that with
# X_LIBS = -framework Carbon

# Default is to use a 1.3 version of FLTK. If you want to use a 1.1 version
# instead, uncomment the next line. (Note: using version 2 is not supported.)
#FLTK_VERSION=FLTK_1_1

# The FLTK header files are under $(FLTK_INCLUDE)/FL
FLTK_INCLUDE = $(X_LOCATION)/include

# You can use fltk_jpeg or jpeg library, whichever you have,
# or depending on how fltk was compiled, you may not need it at all
JPEGLIB = -ljpeg

# You can use fltk_png or png library, whichever you have,
# or depending on how fltk was compiled, you may not need it at all
PNGLIB = -lpng

# You can use fltk_z or z library, whichever you have,
# or depending on how fltk was compiled, you may not need it at all
ZLIB = -lz

# Options to pass to man command.
# On Mac OS X, replace with -t
MAN_OPTIONS = -l -Tps

#-----------------------------------------------------------------------

MUP_SRC =  \
	src/mup/abshorz.c \
	src/mup/absvert.c \
	src/mup/assign.c \
	src/mup/beaming.c \
	src/mup/beamstem.c \
	src/mup/brac.c \
	src/mup/charinfo.c \
	src/mup/check.c \
	src/mup/debug.c \
	src/mup/errors.c \
	src/mup/font.c \
	src/mup/fontdata.c \
	src/mup/globals.c \
	src/mup/grpsyl.c \
	src/mup/ifgram.c \
	src/mup/keymap.c \
	src/mup/lex.c \
	src/mup/locvar.c \
	src/mup/lyrics.c \
	src/mup/macros.c \
	src/mup/main.c \
	src/mup/mainlist.c \
	src/mup/map.c \
	src/mup/midi.c \
	src/mup/midigrad.c \
	src/mup/miditune.c \
	src/mup/midiutil.c \
	src/mup/mkchords.c \
	src/mup/musfont.c \
	src/mup/nxtstrch.c \
	src/mup/parstssv.c \
	src/mup/parstuff.c \
	src/mup/phrase.c \
	src/mup/plutils.c \
	src/mup/print.c \
	src/mup/prntdata.c \
	src/mup/prntmisc.c \
	src/mup/prnttab.c \
	src/mup/prolog.c \
	src/mup/range.c \
	lib/rational.c \
	src/mup/relvert.c \
	src/mup/restsyl.c \
	src/mup/roll.c \
	src/mup/setgrps.c \
	src/mup/setnotes.c \
	src/mup/ssv.c \
	src/mup/stuff.c \
	src/mup/symtbl.c \
	src/mup/tie.c \
	src/mup/trantab.c \
	src/mup/trnspose.c \
	src/mup/undrscre.c \
	src/mup/utils.c \
	src/mup/ytab.c

MUP_HDRS = \
	src/include/defines.h \
	src/include/extchar.h \
	src/include/globals.h \
	src/include/muschar.h \
	src/include/rational.h \
	src/include/ssvused.h \
	src/include/structs.h \
	src/include/ytab.h

MUPDISP_SRC = \
	src/mupdisp/at386.c \
	src/mupdisp/do_cmd.c \
	src/mupdisp/dos.c \
	src/mupdisp/genfile.c \
	src/mupdisp/init.c \
	src/mupdisp/linvga.c \
	src/mupdisp/mupdisp.c \
	src/mupdisp/xterm.c

MUPDISP_HDRS = src/mupdisp/dispttyp.h src/mupdisp/mupdisp.h

MUPDISP_BITMAPS = src/mupdisp/help.bm src/mupdisp/waitmsg.bm

MKMUPFNT_SRC = src/mkmupfnt/mkmupfnt.c

MUPMATE_SRC = \
	src/mupmate/Config.C \
	src/mupmate/Edit.C \
	src/mupmate/File.C \
	src/mupmate/Help.C \
	src/mupmate/Main.C \
	src/mupmate/Preferences.C \
	src/mupmate/Run.C \
	src/mupmate/utils.C \
	src/mupmate/template.C

MUPMATE_HDRS = \
	src/include/defines.h \
	src/mupmate/Config.H \
	src/mupmate/Edit.H \
	src/mupmate/File.H \
	src/mupmate/Help.H \
	src/mupmate/Main.H \
	src/mupmate/Preferences.H \
	src/mupmate/Run.H \
	src/mupmate/utils.H \
	src/mupmate/globals.H \
	src/mupmate/resource.h

MUPMATE_OTHER_FILES = \
	src/mupmate/mup32.xpm

MUP_LIB_FILES = \
	mup-input/includes/cyrillic \
	mup-input/includes/gen_midi \
	mup-input/includes/grids \
	mup-input/includes/grids2 \
	mup-input/includes/guitar \
	mup-input/includes/helmholtz_accs \
	tools/mup.vim \
	mup-input/includes/quarterstep_accs \
	mup-input/includes/tabstems

#---------------------------------------------------------------

all: src/mup/mup src/mupdisp/mupdisp src/mkmupfnt/mkmupfnt src/mupmate/mupmate docs

src/mup/mup: $(MUP_HDRS) $(MUP_SRC)
	$(CCOMPILER) -Isrc/include $(CFLAGS) -o $@ $(MUP_SRC) -lm

src/mupdisp/mupdisp: $(MUPDISP_HDRS) $(MUPDISP_BITMAPS) $(MUPDISP_SRC)
	$(CCOMPILER) $(CFLAGS) -L$(X_LOCATION)/lib -o $@ -DNO_VGA_LIB $(MUPDISP_SRC) -lX11
	# For Linux console mode support, comment out the previous line
	# and uncomment the following line
	# $(CCOMPILER) $(CFLAGS) -L$(X_LOCATION)/lib -o $@ $(MUPDISP_SRC) -lvga -lX11 -lm

src/mkmupfnt/mkmupfnt: $(MKMUPFNT_SRC)
	$(CCOMPILER) $(CFLAGS) -o $@ $(MKMUPFNT_SRC)

src/mupmate/mupmate: $(MUPMATE_SRC) $(MUPMATE_HDRS) $(MUPMATE_OTHER_FILES)
	$(CPPCOMPILER) $(CFLAGS) -o $@ $(MUPMATE_SRC) \
	$(FLTK_VERSION) -I$(FLTK_INCLUDE) -Isrc/include -L$(X_LOCATION)/lib \
	$(FLTK_LIB_LOCATION) -lfltk -lfltk_images $(X_LIBS) \
	$(JPEGLIB) $(PNGLIB) $(ZLIB) -lm


docs: doc/manpages/mup.ps doc/manpages/mkmupfnt.ps doc/manpages/mupmate.ps doc/manpages/mupprnt.ps doc/manpages/mupdisp.ps

doc/manpages/mup.ps: doc/manpages/mup.1
	man $(MAN_OPTIONS) doc/manpages/mup.1 > doc/manpages/mup.ps

doc/manpages/mupmate.ps: doc/manpages/mupmate.1
	man $(MAN_OPTIONS) doc/manpages/mupmate.1 > doc/manpages/mupmate.ps

doc/manpages/mkmupfnt.ps: doc/manpages/mkmupfnt.1
	man $(MAN_OPTIONS) doc/manpages/mkmupfnt.1 > doc/manpages/mkmupfnt.ps

doc/manpages/mupprnt.ps: doc/manpages/mupprnt.1
	man $(MAN_OPTIONS) doc/manpages/mupprnt.1 > doc/manpages/mupprnt.ps

doc/manpages/mupdisp.ps: doc/manpages/mupdisp.1
	man $(MAN_OPTIONS) doc/manpages/mupdisp.1 > doc/manpages/mupdisp.ps

install:	install-mup install-mkmupfnt install-mupdisp install-mupmate install-mupdocs install-mupincludes install-mupprnt

install-mup:	src/mup/mup
	mkdir -p $(BINDIR)
	cp src/mup/mup $(BINDIR)/mup

install-mkmupfnt:	src/mkmupfnt/mkmupfnt
	mkdir -p $(BINDIR)
	cp src/mkmupfnt/mkmupfnt $(BINDIR)/mkmupfnt

install-mupdisp:	src/mupdisp/mupdisp
	mkdir -p $(BINDIR)
	cp src/mupdisp/mupdisp $(BINDIR)/mupdisp

install-mupmate:	src/mupmate/mupmate
	mkdir -p $(BINDIR) $(ICONDIR)
	cp src/mupmate/mupmate $(BINDIR)/mupmate
	# For Linux console mode support, uncomment the following line
	# chown root $(BINDIR)/mupdisp ; chmod 4755 $(BINDIR)/mupdisp
	cp $(MUPMATE_OTHER_FILES) $(ICONDIR)

install-mupprnt: src/mupprnt/mupprnt
	cp src/mupprnt/mupprnt $(BINDIR)/mupprnt
	chmod +x $(BINDIR)/mupprnt

install-mupdocs: LICENSE doc/uguide.ps doc/quickref.ps \
	doc/manpages/mup.ps doc/manpages/mupmate.ps \
	doc/manpages/mkmupfnt.ps doc/manpages/mupprnt.ps doc/manpages/mupdisp.ps \
	mup-input/examples/template.mup mup-input/examples/template.mup
	mkdir -p $(MANDIR) $(DOCDIR)/uguide
	cp doc/manpages/*.1 $(MANDIR)
	cp doc/htmldocs/*.html $(DOCDIR)
	cp doc/uguide/* $(DOCDIR)/uguide
	cp LICENSE $(DOCDIR)/license.txt
	cp doc/uguide.ps $(DOCDIR)/uguide.ps
	cp doc/quickref.ps $(DOCDIR)/quickref.ps
	cp mup-input/examples/template.mup $(DOCDIR)/template.mup
	cp mup-input/examples/sample.mup $(DOCDIR)/sample.mup
	cp doc/manpages/mup.ps $(DOCDIR)/mup.ps
	cp doc/manpages/mupmate.ps $(DOCDIR)/mupmate.ps
	cp doc/manpages/mkmupfnt.ps $(DOCDIR)/mkmupfnt.ps
	cp doc/manpages/mupprnt.ps $(DOCDIR)/mupprnt.ps
	cp doc/manpages/mupdisp.ps $(DOCDIR)/mupdisp.ps

install-mupincludes:
	mkdir -p $(LIBDIR)
	cp $(MUP_LIB_FILES) $(LIBDIR)


clean:
	rm -f src/mup/*.o src/mupdisp/*.o src/mkmupfnt/*.o src/mupmate/*.o

clobber:	clean
	rm -f src/mup/mup src/mupdisp/mupdisp src/mkmupfnt/mkmupfnt src/mupmate/mupmate
