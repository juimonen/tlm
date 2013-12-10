#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([tlm], [0.0.1], [])
AC_CONFIG_SRCDIR([src/tlm-main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 silent-rules subdir-objects tar-pax -Wno-portability])

#libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# Checks for programs.
AC_PROG_CC
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums, [$PATH])

# Checks for libraries.
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.30])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES([GIO], gio-2.0 gio-unix-2.0)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

PKG_CHECK_MODULES([GMODULE], [gmodule-2.0])
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

TLM_CFLAGS="$GLIB_CFLAGS $GIO_CFLAGS $GMODULE_CFLAGS -Wall -Werror"
TLM_LIBS="$GLIB_LIBS $GIO_LIBS $GMODULE_LIBS"
AC_SUBST(TLM_CFLAGS)
AC_SUBST(TLM_LIBS)

# Enable gum
PKG_CHECK_MODULES([LIBGUM], [libgum], [have_libgum=yes], [have_libgum=no])
AC_ARG_ENABLE(gum, [  --enable-gum build for gumd plugin], ,
            [enable_gum=$have_libgum])
if test "x$enable_gum" = "xyes" && test "x$have_libgum" = "xno" ; then
    AC_MSG_ERROR("libgum is required.")
fi
AC_SUBST(LIBGUM_CFLAGS)
AC_SUBST(LIBGUM_LIBS)
AM_CONDITIONAL(HAVE_LIBGUM, [test x$enable_gum = xyes])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([
Makefile
src/Makefile
src/plugins/Makefile
src/plugins/default/Makefile
])

AM_COND_IF([HAVE_LIBGUM],
           AC_CONFIG_FILES([src/plugins/gumd/Makefile]))

AC_OUTPUT

