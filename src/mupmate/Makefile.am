bin_PROGRAMS = mupmate
BUILT_SOURCES = template.C
EXTRA_DIST = MupMate.icns.b64 MupMateDoc.icns.b64
if ! OSX
  EXTRA_DIST += apple.mm
endif
PIXMAPS = mup16.xpm mup32.xpm mup48.xpm
mup_pixmap_dir = $(datadir)/pixmaps/$(PACKAGE)
mup_pixmap__DATA = mup32.xpm

mupmate_SOURCES = Config.C Config.H Edit.C Edit.H File.C File.H \
	globals.H Help.C Help.H Main.C Main.H \
	Preferences.C Preferences.H Run.C Run.H template.C utils.C utils.H \
	../include/defines.h $(PIXMAPS) resource.h mupmate.rc
if OSX
  mupmate_SOURCES += apple.mm
endif
AM_CXXFLAGS = -I../include $(FLTK_CXXFLAGS) @EXTRA_CFLAGS@ $(optflags)

mupmate_LDFLAGS =
mupmate_LDADD = $(FLTK_LDFLAGS)
# Add appropriate options for Windows and OSX
if WINDOWS
   mupmate_LDFLAGS += -Wl,--subsystem,windows
   mupmate_LDADD += icon.o -lcomdlg32 -lgdi32 -luuid -lole32 -lws2_32
endif
if OSX
   mupmate_LDFLAGS += -framework Cocoa -L/usr/X11/lib -L/opt/X11/lib
   mupmate_LDADD += -lX11
endif
if LINUX
   mupmate_LDADD += -lXext -lX11 -lXpm -lXft -lXinerama
endif
mupmate_LDADD += -lm

template.C: ../../tools/mupmate/mktemplateC ../../mup-input/examples/template.mup ../../LICENSE
	../../tools/mupmate/mktemplateC ../../LICENSE ../../mup-input/examples/template.mup > template.C

if WINDOWS
icon.o:	mupicons.ico mupmate.rc resource.h
	$(WINDRES) -i mupmate.rc -o icon.o

mupicons.ico: $(PIXMAPS)
if HAVE_PAMTOWINICON
	$(XPMTOPPM) mup16.xpm > mup16.ppm
	$(SED) -e "s/const //" mup32.xpm | $(XPMTOPPM) > mup32.ppm
	$(XPMTOPPM) mup48.xpm > mup48.ppm
	cat mup16.ppm mup32.ppm mup48.ppm | $(PAMTOWINICON) > mupicons.ico
else
if HAVE_PPMTOWINICON
	$(XPMTOPPM) mup16.xpm | $(SED) -e "3s/255/16/" > mup16.ppm
	$(PPMTOPGM) mup16.ppm > mup16.pgm 2> /dev/null
	$(SED) -e "s/const //" mup32.xpm | $(SED) -e "3s/255/16/" | $(XPMTOPPM) > mup32.ppm
	$(PPMTOPGM) mup32.ppm > mup32.pgm 2> /dev/null
	$(XPMTOPPM) mup48.xpm | $(SED) -e "3s/255/16/" | > mup48.ppm
	$(PPMTOPGM) mup48.ppm > mup48.pgm 2> /dev/null
	$(PPMTOWINICON) -andpgms mup16.ppm mup16.pgm mup32.ppm mup32.pgm mup48.ppm mup48.pgm > mupicons.ico
endif
endif
endif
