include $(top_srcdir)/common.mk
plugins_LTLIBRARIES = libtlm-plugin-gumd.la

libtlm_plugin_gumd_la_SOURCES = \
	tlm-plugin-gumd.h \
	tlm-plugin-gumd.c

libtlm_plugin_gumd_la_CFLAGS = \
	-I$(abs_top_srcdir)/src/common \
	-DG_LOG_DOMAIN=\"TLM_PLUGIN_GUMD\" \
	$(LIBGUM_CFLAGS)

libtlm_plugin_gumd_la_LDFLAGS = -avoid-version

libtlm_plugin_gumd_la_LIBADD = \
	$(abs_top_builddir)/src/common/libtlm-common.la \
	$(LIBGUM_LIBS)

all-local: slink

slink:
	ln -sf $(abs_top_builddir)/src/plugins/gumd/.libs/libtlm-plugin-gumd.so ../.libs/libtlm-plugin-gumd.so
