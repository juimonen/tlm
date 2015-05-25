NULL=

noinst_LTLIBRARIES = libtlm-launcher-dbus.la

libtlm_launcher_dbus_la_CPPFLAGS = \
    -I$(top_srcdir)/src \
    -I$(top_builddir)/src \
    $(TLM_CFLAGS) \
    -DG_LOG_DOMAIN=\"TLM-LAUNCHER\" \
    $(NULL)

libtlm_launcher_dbus_la_LIBADD = \
    $(top_builddir)/src/common/dbus/libtlm-dbus-glue.la \
    $(TLM_LIBS) \
    $(NULL)

libtlm_launcher_dbus_la_SOURCES = \
   tlm-dbus-launcher-adapter.c \
   tlm-dbus-launcher-adapter.h \
   $(NULL)

clean-local :
	rm -f *~

