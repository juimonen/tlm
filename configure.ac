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


# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([
Makefile
src/Makefile
src/plugins/Makefile
src/plugins/default/Makefile
])

AC_OUTPUT

