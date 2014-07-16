NULL=

noinst_LTLIBRARIES = libtlm-dbus.la

libtlm_dbus_la_CPPFLAGS = \
    -I$(top_srcdir)/src \
    -I$(top_builddir)/src \
    $(TLM_CFLAGS) \
    -DG_LOG_DOMAIN=\"TLM\" \
    $(NULL)

libtlm_dbus_la_LIBADD = \
    $(top_builddir)/src/common/dbus/libtlm-dbus-glue.la \
    $(TLM_LIBS) \
    $(NULL)

libtlm_dbus_la_SOURCES = \
   tlm-dbus-server-interface.c \
   tlm-dbus-server-interface.h \
   tlm-dbus-server-p2p.c \
   tlm-dbus-server-p2p.h \
   tlm-dbus-login-adapter.c \
   tlm-dbus-login-adapter.h \
   $(NULL)

clean-local :
	rm -f *~

