#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mup], [7.0], [support@arkkra.com])
AM_INIT_AUTOMAKE([-Wall -Werror])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR
AM_PROG_LEX
AC_PROG_GREP
AC_PROG_SED
AC_PROG_MKDIR_P

AC_PATH_PROGS(GS, [gs gs386], [none])
test "$GS" == "none" && AC_MSG_ERROR([required ghostscript program 'gs' is missing])
AC_PATH_PROG(GROFF, [groff], [none])
test "$GROFF" == "none" && AC_MSG_ERROR([required program 'groff' is missing])
AC_PATH_PROG(BISON, [bison], [none])
test "$BISON" == "none" && AC_MSG_ERROR([required program 'bison' is missing])
AC_PATH_PROG(PPMTOGIF, [ppmtogif], [none])
test "$PPMTOGIF" == "none" && AC_MSG_ERROR([required program 'ppmtogif' is missing])
AC_PATH_PROG(EXPR, [expr], [none])
test "$EXPR" == "none" && AC_MSG_ERROR([required program 'expr' is missing])
AC_PATH_PROG(SORT, [sort], [none])
test "$SORT" == "none" && AC_MSG_ERROR([required program 'sort' is missing])
AC_PATH_PROG(CAT, [cat], [none])
test "$CAT" == "none" && AC_MSG_ERROR([required program 'cat' is missing])
AC_PATH_PROG(EGREP, [egrep], [none])
test "$EGREP" == "none" && AC_MSG_ERROR([required program 'egrep' is missing])

# Need to know if on Windows to install mupprnt.bat rather than shell script
AC_CANONICAL_HOST
build_windows=no
build_mac=no
build_linux=no
case "${host_os}" in
*mingw*) build_windows=yes ;;
*darwin*) build_mac=yes ;;
*linux*) build_linux=yes ;;
esac
AM_CONDITIONAL([WINDOWS], [test "$build_windows" = "yes"])
AM_CONDITIONAL([OSX], [test "$build_mac" = "yes"])
AM_CONDITIONAL([LINUX], [test "$build_linux" = "yes"])

# Check for option to not build mupdisp
AC_ARG_ENABLE(mupdisp,[  --disable-mupdisp  Do not build mupdisp],,)
AM_CONDITIONAL([BUILD_MUPDISP], [test x$enable_mupdisp != xno])

# Check for option to not build mupmate
AC_ARG_ENABLE(mupmate,[  --disable-mupmate  Do not build mupmate],,)
AM_CONDITIONAL([BUILD_MUPMATE], [test x$enable_mupmate != xno])

# Programs for Windows icons for Mupmate
AC_PATH_PROG(PAMTOWINICON, pamtowinicon, none, $PATH)
AM_CONDITIONAL([HAVE_PAMTOWINICON],[test "$PAMTOWINICON" != "none"])
AC_PATH_PROG(PPMTOWINICON, ppmtowinicon, none, $PATH)
AM_CONDITIONAL([HAVE_PPMTOWINICON],[test "$PPMTOWINICON" != "none"])

# Mac needs objective-C for one Mupmate file
test "$build_mac" == "yes" && AC_PROG_OBJCXX

# Do extra check if user wants to build mupmate
AS_IF([test "x$enable_mupmate" != xno],
	[
	# Check for fltk
	# Allow user to specify where to get fltk from using --fltk-prefix
	AC_ARG_WITH(fltk-prefix,[  --with-fltk-prefix=PREFIX  Prefix where FLTK is installed (default /usr)],
  	fltk_prefix="$withval", fltk_prefix="/usr")
	# Find fltk-config program
	PATH="$fltk_prefix/bin:$PATH"
	AC_PATH_PROG(FLTK_CONFIG, fltk-config, none, $PATH)
	test "$FLTK_CONFIG" == "none" && AC_MSG_ERROR([required program 'fltk-config' is missing])
	# Make sure it is the 1.x API, not 2.x
	fltk_api_version="`$FLTK_CONFIG --api-version`"
	fltk_api_major=`echo $fltk_api_version | sed -e 's/\..*//'`
	test "$fltk_api_major" != "1" && AC_MSG_ERROR([fltk api version 1.x required, not $fltk_api_major])
	# Set variables for Mupmate compile to use
	AC_SUBST([FLTK_CXXFLAGS], [`$FLTK_CONFIG --cxxflags`])
	# Red Hat/CentOS 7 version of fltk-config doesn't include -lfltk when using --use_image --ldflags, so fix that
	fltk_ldflags="`$FLTK_CONFIG --use-images --ldflags`"
	if test "$fltk_ldflags" == "-lfltk_images"
	then
		AC_SUBST([FLTK_LDFLAGS], ["-lfltk -lfltk_images"])
	else
		AC_SUBST([FLTK_LDFLAGS], [`$FLTK_CONFIG --use-images --ldflags`])
	fi
	test "$build_mac" == "yes" && AC_SUBST([EXTRA_CFLAGS], [-Dunix])
	# Check that libraries that Mupmate needs exist
	AC_CHECK_LIB([fltk], [fl_alphasort], [ ])
	AC_CHECK_LIB([fltk_images], [main], [ ])
	AC_CHECK_LIB([X11],[main], [ ])
	AC_CHECK_LIB([Xext], [XdbeQueryExtension], [ ])
	AC_CHECK_LIB([Xft], [XftDrawCreate], [ ])
	AC_CHECK_LIB([Xpm], [XpmCreatePixmapFromData], [ ])
	AC_CHECK_LIB([Xinerama], [XineramaIsActive], [ ])
	AC_CHECK_LIB([jpeg], [jpeg_CreateCompress], [ ])
	AC_CHECK_LIB([m], [exp], [ ])
	AC_CHECK_LIB([png], [png_read_info], [ ])
	AC_CHECK_LIB([z], [gzgets], [ ])

	# Programs for Windows icons for Mupmate
	test "$build_windows" == "yes" && test "$PAMTOWINICON" == "none" && test "$PPMWINICON" == "none" && AC_MSG_ERROR([Need either pamtowinicon or ppmtowinicon])
	AC_PATH_PROG(XPMTOPPM, xpmtoppm, none, $PATH)
	test "$build_windows" == "yes" && test "$XPMTOPPM" == "none" && AC_MSG_ERROR([Need xpmtoppm])
	AC_PATH_PROG(PPMTOPGM, ppmtopgm, none, $PATH)
	test "$build_windows" == "yes" && test "$PPMTOPGM" == "none" && test "$PAMTOWINICON" == "none" && AC_MSG_ERROR([Need ppmtopgm])
	AC_PATH_PROG(WINDRES, i386-mingw32msvc-windres, none, $PATH)
	test "$build_windows" == "yes" && test "$WINDRES" == "none" && AC_MSG_ERROR([Need i386-mingw32msvc-windres])
	]
)

AC_CONFIG_HEADERS([config.h])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h stdint.h stdlib.h string.h unistd.h])

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([access alarm atan cos getcwd isascii isspace memcmp memmove memset munmap pow putenv select setenv sin sqrt strcasecmp strchr strcmp strcpy strcspn strdup strerror strncasecmp strncpy strpbrk strrchr strspn strstr strtol tan unlink])
test "$build_linux" == "yes" && AC_CHECK_FUNCS([kill sleep waitpid])

AS_IF([test "x$enable_mupdisp" != xno],
	[
	# Check if libvga.a is available for mupdisp
	AC_CHECK_FILE([/usr/lib/libvga.a],[use_vga=yes],)
	AC_CHECK_FILE([/usr/lib64/libvga.a],[use_vga=yes],)
	test "$use_vga" != "yes" && AC_SUBST([LIBVGA],[-DNO_VGA_LIB])
	]
)


AC_CONFIG_FILES([
 Makefile
 doc/Makefile
 doc/manpages/Makefile
 doc/htmldocs/Makefile
 lib/Makefile
 mup-input/Makefile
 mup-input/examples/Makefile
 mup-input/includes/Makefile
 mup-input/testfiles/Makefile
 mup-input/testfiles/test-midi/Makefile
 packaging/Makefile
 src/Makefile
 src/include/Makefile
 src/mup/Makefile
 src/mkmupfnt/Makefile
 src/mupprnt/Makefile
 tools/Makefile
 tools/mup/Makefile
 tools/doc/Makefile
 tools/test/Makefile
])
AS_IF([test "x$enable_mupdisp" != xno],
	[ AC_CONFIG_FILES([
	src/mupdisp/Makefile
	tools/mupdisp/Makefile
	])
])
AS_IF([test "x$enable_mupmate" != xno],
	[ AC_CONFIG_FILES([
	src/mupmate/Makefile
	tools/mupmate/Makefile
	])
])

AC_OUTPUT
