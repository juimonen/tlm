include $(top_srcdir)/common.mk
SUBDIRS = dbus

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
	tlm-log.h \
	tlm-log.c \
	tlm-error.h \
	tlm-error.c \
	tlm-config.h \
	tlm-config.c \
	tlm-config-general.h \
	$(NULL)

libtlm_plugin_la_CFLAGS = \
	-I$(abs_top_srcdir)/src \
	-DG_LOG_DOMAIN=\"TLM_PLUGIN\" \
	-DTLM_PLUGINS_DIR='"$(pluginsdir)"' \
	-DTLM_SYSCONF_DIR='"$(sysconfdir)"' \
	$(TLM_CFLAGS) \
	$(NULL)

libtlm_plugin_la_LIBADD = \
	$(TLM_LIBS) \
	$(NULL)

