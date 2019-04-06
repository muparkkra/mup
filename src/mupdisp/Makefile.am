bin_PROGRAMS = mupdisp
BUILT_SOURCES =	help.bm waitmsg.bm
mupdisp_SOURCES = at386.c do_cmd.c genfile.c init.c mupdisp.c xterm.c \
	 dos.c linvga.c mupdisp.h dispttyp.h help.bm waitmsg.bm
AM_CFLAGS = @LIBVGA@ @EXTRA_CFLAGS@ $(optflags)
AM_LDFLAGS = -lm
if OSX
  AM_LDFLAGS += -framework Carbon -L/usr/X11/lib -lX11
else
  AM_LDFLAGS += -lX11
endif

EXTRA_DIST = help waitmsg

help.bm:	../../tools/mupdisp/ps2bm help
	$(GROFF) help | ../../tools/mupdisp/ps2bm Help > help.bm

waitmsg.bm:	../../tools/mupdisp/ps2bm waitmsg
	$(GROFF) waitmsg | ../../tools/mupdisp/ps2bm Waitmsg > waitmsg.bm
