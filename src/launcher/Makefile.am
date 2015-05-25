## Process this file with automake to produce Makefile.in

SUBDIRS = dbus

bin_PROGRAMS = tlm-launcher

tlm_launcher_SOURCES = \
	tlm-dbus-launcher-observer.c \
	tlm-dbus-launcher-observer.h \
	tlm-launcher.c

tlm_launcher_CFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-DBINDIR='"$(bindir)"' \
	-DG_LOG_DOMAIN=\"TLM_LAUNCHER\" \
	$(GLIB_CFLAGS) \
	$(DEPS_CFLAGS)

tlm_launcher_LDADD = \
	$(abs_top_builddir)/src/common/libtlm-common.la \
	$(abs_top_builddir)/src/launcher/dbus/libtlm-launcher-dbus.la \
	$(GLIB_LIBS) \
	$(DEPS_LIBS)

CLEANFILES = *.gcno *.gcda
