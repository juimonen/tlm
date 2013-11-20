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
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

# Checks for libraries.
PKG_CHECK_MODULES([TLM], [glib-2.0 gio-2.0 gio-unix-2.0])
AC_SUBST(TLM_CFLAGS)
AC_SUBST(TLM_LIBS)

TLM_CFLAGS="$TLM_CFLAGS -Wall -Werror"

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_OUTPUT([
Makefile
src/Makefile
src/plugins/Makefile
])

