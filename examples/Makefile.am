## Process this file with automake to produce Makefile.in

bin_PROGRAMS = tlm-ui

tlm_ui_SOURCES = tlm-ui.c
tlm_ui_CPPFLAGS = \
    -I$(top_srcdir)/src \
    $(GLIB_CFLAGS) \
    $(GIO_CFLAGS) \
    $(GMODULE_CFLAGS) \
    $(ELEMENTARY_CFLAGS) \
    $(DEPS_CFLAGS)
	
tlm_ui_LDADD = \
    $(GLIB_LIBS) \
    $(GIO_LIBS) \
    $(GMODULE_LIBS) \
    $(ELEMENTARY_LIBS) \
    $(DEPS_LIBS) \
    $(top_builddir)/src/common/.libs/libtlm_common_la-tlm-utils.o \
    $(top_builddir)/src/daemon/dbus/libtlm-dbus.la \
    $(top_builddir)/src/common/dbus/libtlm-dbus-glue.la

CLEANFILES = *.gcno *.gcda
