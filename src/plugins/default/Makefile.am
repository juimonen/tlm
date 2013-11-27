include $(top_srcdir)/common.mk
plugins_LTLIBRARIES = libdefault.la

libdefault_la_SOURCES = \
	tlm-plugin-default.h \
	tlm-plugin-default.c

libdefault_la_CFLAGS = \
	-I$(abs_top_builddir)/src \
	-DG_LOG_DOMAIN=\"TLM_PLUGIN_DEFAULT\" \
	$(GLIB_CFLAGS)

libdefault_la_LDFLAGS = -avoid-version

libdefault_la_LIBADD = \
	$(abs_top_builddir)/src/libtlm-plugin.la \
	$(GLIB_LIBS)
