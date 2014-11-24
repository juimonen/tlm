#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([tlm], [1.0.1], [])
AC_CONFIG_SRCDIR([src/daemon/tlm-main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 silent-rules subdir-objects tar-pax -Wno-portability])

#libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

#gtk-doc
GTK_DOC_CHECK([1.18],[--flavour no-tmpl])

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

# Check for libtlm-nfc
PKG_CHECK_MODULES([LIBTLM_NFC], [libtlm-nfc], [have_libtlm_nfc=yes], [have_libtlm_nfc=no])
AC_SUBST(LIBTLM_NFC_CFLAGS)
AC_SUBST(LIBTLM_NFC_LIBS)
AM_CONDITIONAL(HAVE_LIBTLM_NFC, [test x$have_libtlm_nfc = xyes])

AC_CHECK_HEADERS([security/pam_appl.h],,[AC_MSG_ERROR("pam-devel is required")])
AC_CHECK_HEADERS([security/pam_misc.h],,[AC_MSG_ERROR("pam-misc is required")])

TLM_CFLAGS="$GLIB_CFLAGS $GIO_CFLAGS $GMODULE_CFLAGS -D_POSIX_C_SOURCE=\"200809L\" -D_GNU_SOURCE -D_REENTRANT -D_THREAD_SAFE -Wall -Werror"
TLM_LIBS="$GLIB_LIBS $GIO_LIBS $GMODULE_LIBS"
AC_SUBST(TLM_CFLAGS)
AC_SUBST(TLM_LIBS)

# Enable debug
AC_ARG_ENABLE(debug, [ --enable-debug enable debug features],  
                     [enable_debug=$enable_debug], [enable_debug="no"])
if test "x$enable_debug" = "xyes" ; then
    AC_DEFINE(ENABLE_DEBUG, [1], [Enable debug features])
    AC_ARG_ENABLE(tests, [ --enable-tests build unit tests],
                  [enable_tests=$enable_tests], [enable_tests="no"])
    if test "x$enable_tests" = "xyes" ; then
        AC_DEFINE(ENABLE_TESTS, [1], [Build unit tests], [enable_tests=yes])
        PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])
    fi
else
    AC_MSG_WARN("Tests can't be enabled without debug features")
    enable_tests=no
fi
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)
AM_CONDITIONAL(ENABLE_TESTS, [test "x$enable_tests" = "xyes"])

# Enable examples
AC_ARG_ENABLE(examples, [ --enable-examples enable examples],
                     [enable_examples=$enable_examples], [enable_examples="no"])
AM_CONDITIONAL(ENABLE_EXAMPLES, [test x$enable_examples = xyes])

if test "x$enable_examples" = "xyes" ; then
    PKG_CHECK_MODULES([ELEMENTARY], [elementary])
    AC_SUBST(ELEMENTARY_CFLAGS)
    AC_SUBST(ELEMENTARY_LIBS)
fi

# Define socket path
AC_ARG_ENABLE(sockets-path,
          [  --enable-sockets-path=path  enable dbus sockets path at location'
           "path" instead of default "/var/run/tlm"],
          [enable_sockets_path=$enableval],
          [enable_sockets_path="/var/run/tlm"])
AC_DEFINE_UNQUOTED(TLM_DBUS_SOCKET_PATH, ["$enable_sockets_path"],
         [Path for dbus sockets])
AC_DEFINE_UNQUOTED(TLM_DBUS_ROOT_SOCKET_ADDRESS,
         ["unix:path=$enable_sockets_path/dbus-sock"], [Address for dbus socket
         accessed by root only])

# Enable gum
PKG_CHECK_MODULES([LIBGUM], [libgum], [have_libgum=yes], [have_libgum=no])
AC_ARG_ENABLE(gum, [  --enable-gum build for gumd plugin], ,
            [enable_gum=$have_libgum])
if test "x$enable_gum" = "xyes" && test "x$have_libgum" = "xno" ; then
    AC_MSG_ERROR("libgum is required.")
fi
AC_SUBST(LIBGUM_CFLAGS)
AC_SUBST(LIBGUM_LIBS)
AM_CONDITIONAL(HAVE_LIBGUM, [test x$have_libgum = xyes])

if test "x$enable_gum" = "xyes" ; then
    AC_SUBST(ACCOUNT_PLUGIN_NAME, [gumd])
else
    AC_SUBST(ACCOUNT_PLUGIN_NAME, [default])
fi

AC_CONFIG_FILES([
Makefile
docs/Makefile
src/Makefile
src/common/Makefile
src/common/dbus/Makefile
src/daemon/Makefile
src/daemon/dbus/Makefile
src/daemon/tlm.pc
src/sessiond/Makefile
src/utils/Makefile
src/plugins/Makefile
src/plugins/default/Makefile
src/plugins/gumd/Makefile
src/plugins/nfc/Makefile
data/Makefile
data/tlm.conf
tests/Makefile
tests/config/Makefile
tests/daemon/Makefile
tests/tlm-test.conf
examples/Makefile
])

AC_OUTPUT

echo ""
echo "Enabled debug features : "$enable_debug
echo "Building tests         : "$enable_tests
echo "Enabled Gumd           : "$enable_gum $have_libgum
echo "Enabled NFC            : "$have_libtlm_nfc
echo "Enabled examples       : "$enable_examples
echo ""
