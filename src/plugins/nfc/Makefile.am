include $(top_srcdir)/common.mk
plugins_LTLIBRARIES = libtlm-plugin-nfc.la

libtlm_plugin_nfc_la_SOURCES = \
	tlm-auth-plugin-nfc.h \
	tlm-auth-plugin-nfc.c

libtlm_plugin_nfc_la_CFLAGS = \
	-I$(abs_top_srcdir)/src/common \
	-DG_LOG_DOMAIN=\"TLM_PLUGIN_NFC\" \
	$(LIBTLM_NFC_CFLAGS) \
	$(GLIB_CFLAGS)

libtlm_plugin_nfc_la_LDFLAGS = -avoid-version

libtlm_plugin_nfc_la_LIBADD = \
	$(abs_top_builddir)/src/common/libtlm-plugin.la \
	$(LIBTLM_NFC_LIBS) \
	$(GLIB_LIBS)

all-local: slink

slink:
	ln -sf $(abs_top_builddir)/src/plugins/nfc/.libs/libtlm-plugin-nfc.so ../.libs/libtlm-plugin-nfc.so
