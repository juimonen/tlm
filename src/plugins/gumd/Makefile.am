include $(top_srcdir)/common.mk
plugins_LTLIBRARIES = libgumd.la

libgumd_la_SOURCES = \
	tlm-plugin-gumd.h \
	tlm-plugin-gumd.c

libgumd_la_CFLAGS = \
	-I$(abs_top_builddir)/src \
	-DG_LOG_DOMAIN=\"TLM_PLUGIN_GUMD\" \
	$(LIBGUM_CFLAGS)

libgumd_la_LDFLAGS = -avoid-version

libgumd_la_LIBADD = \
	$(abs_top_builddir)/src/libtlm-plugin.la \
    $(LIBGUM_LIBS)
