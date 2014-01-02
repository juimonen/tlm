include $(top_srcdir)/common.mk
NULL=
SUBDIRS = . plugins
bin_PROGRAMS = tlm
lib_LTLIBRARIES = libtlm-plugin.la
BUILT_SOURCES = 

libtlm_plugin_la_includedir = $(includedir)/tlm
libtlm_plugin_la_include_HEADERS = \
	tlm-account-plugin.h \
	tlm-auth-plugin.h \
	$(NULL)

libtlm_plugin_la_SOURCES = \
	$(BUILT_SOURCES) \
	tlm-account-plugin.c \
	tlm-auth-plugin.c \
	$(NULL)

libtlm_plugin_la_CFLAGS = \
	-I$(abs_top_builddir) \
    -I$(abs_top_srcdir)/src \
	-DG_LOG_DOMAIN=\"TLM_PLUGIN\" \
	$(GLIB_CFLAGS) \
	$(NULL)

libtlm_plugin_la_LIBADD = \
	$(GLIB_LIBS) \
	$(NULL)

tlm_SOURCES = \
	tlm-utils.h \
	tlm-utils.c \
   	tlm-log.h \
    tlm-log.c \
	tlm-auth-session.h \
	tlm-auth-session.c \
    tlm-session.h \
    tlm-session.c \
	tlm-seat.h \
	tlm-seat.c \
	tlm-manager.h \
	tlm-manager.c \
    tlm-config.h \
    tlm-config.c \
	tlm-main.c \
	$(NULL)

tlm_CFLAGS = \
    -I$(abs_top_builddir) \
    -I$(abs_top_srcdir)/src \
	-D_GNU_SOURCE \
	-DG_LOG_DOMAIN=\"TLM\" \
	-DTLM_PLUGINS_DIR='"$(pluginsdir)"' \
    -DTLM_SYSCONF_DIR='"$(sysconfdir)"' \
    $(TLM_CFLAGS) \
    $(NULL)


tlm_LDADD = \
	$(TLM_LIBS) \
	$(abs_top_builddir)/src/libtlm-plugin.la \
	-lpam \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
