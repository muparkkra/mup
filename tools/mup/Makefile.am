AM_CFLAGS = -I../../src/include
noinst_PROGRAMS = mkprolog genhdr bbox smadi
smadi_SOURCES = smadi.c ../../src/include/allocdebug.h
mkprolog_SOURCES = mkprolog.c
genhdr_SOURCES = genhdr.c
BUILT_SOURCES = ../../src/include/muschar.h ../../src/include/extchar.h ../../src/include/ssvused.h
CLEANFILES = ../../src/include/muschar.h ../../src/include/extchar.h ../../src/include/ssvused.h
nodist_bbox_SOURCES = ../../src/include/muschar.h
bbox_SOURCES = bbox.c ../../src/include/defines.h
EXTRA_DIST = getfontinfo.ps

# Possibly it would seem more logical to build extchar.h and ssvused.h
# from src/include rather than from this directory,
# but muschar.h is needed here, so probably has to stay here,
# and then we may as well do the others here too, for consistency.
../../src/include/muschar.h: ../../src/include/muschar.h.in genhdr
	./genhdr char ../../src/include/muschar.h.in ../../src/include/muschar.h

../../src/include/extchar.h: ../../src/include/extchar.h.in genhdr
	./genhdr char ../../src/include/extchar.h.in ../../src/include/extchar.h

../../src/include/ssvused.h: ../../src/include/ssvused.h.in genhdr
	./genhdr used ../../src/include/ssvused.h.in ../../src/include/ssvused.h
