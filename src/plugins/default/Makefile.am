include $(top_srcdir)/common.mk
plugins_LTLIBRARIES = libtlm-plugin-default.la

libtlm_plugin_default_la_SOURCES = \
	tlm-account-plugin-default.h \
	tlm-account-plugin-default.c \
	tlm-auth-plugin-default.h \
    tlm-auth-plugin-default.c

libtlm_plugin_default_la_CFLAGS = \
	-I$(abs_top_builddir)/src \
	-DG_LOG_DOMAIN=\"TLM_PLUGIN_DEFAULT\" \
	$(GLIB_CFLAGS)

libtlm_plugin_default_la_LDFLAGS = -avoid-version

libtlm_plugin_default_la_LIBADD = \
	$(abs_top_builddir)/src/libtlm-plugin.la \
	$(GLIB_LIBS)
