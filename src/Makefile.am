include $(top_srcdir)/common.mk
NULL=
SUBDIRS = plugins
bin_PROGRAMS = tlm
lib_LTLIBRARIES = libtlm-plugin.la
BUILT_SOURCES = \
    tlm-plugin-types.h \
	tlm-plugin-types.c

tlm-plugin-types.h: tlm-enum.h.template tlm-plugin.h
	$(GLIB_MKENUMS) --template tlm-enum.h.template \
                    --fhead "#ifndef _TLM_PLUGIN_TYPES_H\n#define _TLM_PLUGIN_TYPES_H_\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS" \
                    --identifier-prefix Tlm \
                    --ftail "\nG_END_DECLS\n\n#endif\n" \
                    tlm-plugin.h > $@

tlm-plugin-types.c: tlm-enum.c.template tlm-plugin.h tlm-plugin-types.h
	$(GLIB_MKENUMS) --template tlm-enum.c.template \
                    --fhead "#include \"tlm-plugin-types.h\"\n" \
                    --identifier-prefix Tlm \
                    tlm-plugin.h > $@

libtlm_plugin_la_includedir = $(includedir)/tlm
libtlm_plugin_la_include_HEADERS = \
	tlm-plugin.h \
	$(NULL)

libtlm_plugin_la_SOURCES = \
	$(BUILT_SOURCES) \
	tlm-plugin.c \
	$(NULL)

libtlm_plugin_la_CFLAGS = \
	-I$(abs_top_builddir) \
    -I$(abs_top_srcdir)/src \
	-DG_LOG_DOMAIN=\"TLM_PLUGIN\" \
	$(TLM_CFLAGS) \
	$(NULL)

libtlm_plugin_la_LIBADD = \
	$(TLM_LIBS) \
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
	tlm-main.c \
	$(NULL)

tlm_CFLAGS = \
    -I$(abs_top_builddir) \
    -I$(abs_top_srcdir)/src \
	-DG_LOG_DOMAIN=\"TLM\" \
	-DTLM_PLUGINS_DIR='"$(pluginsdir)"' \
    $(TLM_CFLAGS) \
    $(NULL)


tlm_LDADD = \
	$(TLM_LIBS) \
	$(abs_top_builddir)/src/libtlm-plugin.la \
	-lpam \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
