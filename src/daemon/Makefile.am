include $(top_srcdir)/common.mk
SUBDIRS = dbus

bin_PROGRAMS = tlm
BUILT_SOURCES =

tlm_SOURCES = \
	tlm-types.h \
	tlm-session-remote.h \
	tlm-session-remote.c \
	tlm-seat.h \
	tlm-seat.c \
	tlm-dbus-observer.h \
	tlm-dbus-observer.c \
	tlm-manager.h \
	tlm-manager.c \
	tlm-main.c \
	$(NULL)

tlm_CFLAGS = \
	-I$(abs_top_builddir) \
	-I$(abs_top_builddir)/src \
	-I$(abs_top_srcdir)/src \
	-I$(abs_top_srcdir)/src/common \
	-DG_LOG_DOMAIN=\"TLM\" \
	-DTLM_BIN_DIR='"$(bindir)"' \
	-DTLM_PLUGINS_DIR='"$(pluginsdir)"' \
	$(TLM_CFLAGS) \
	$(NULL)


tlm_LDADD = \
	$(TLM_LIBS) \
	$(abs_top_builddir)/src/common/libtlm-common.la \
  $(abs_top_builddir)/src/daemon/dbus/libtlm-dbus.la \
	$(NULL)

EXTRA_DIST = \
      tlm.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tlm.pc

CLEANFILES = $(BUILT_SOURCES)
