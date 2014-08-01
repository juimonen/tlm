include $(top_srcdir)/common.mk

SUBDIRS=
NULL=

noinst_LTLIBRARIES = libtlm-session-daemon.la

libtlm_session_daemon_la_CPPFLAGS = \
    -I$(top_builddir) \
    -I$(top_srcdir)/src \
    -I$(top_srcdir)/include \
    -I$(top_builddir)/src \
    -DG_LOG_DOMAIN=\"TLM_SESSIOND\" \
    $(TLM_CFLAGS)

libtlm_session_daemon_la_LIBADD =    \
        $(top_builddir)/src/common/libtlm-common.la \
        $(top_builddir)/src/common/dbus/libtlm-dbus-glue.la \
        -lpam \
        $(TLM_LIBS)

libtlm_session_daemon_la_SOURCES = \
   tlm-auth-session.h \
   tlm-auth-session.c \
   tlm-session.h \
   tlm-session.c \
   tlm-session-daemon.h \
   tlm-session-daemon.c

bin_PROGRAMS = tlm-sessiond

tlm_sessiond_SOURCES = \
    main.c \
    $(NULL)

tlm_sessiond_CFLAGS = \
    -I$(top_builddir) \
    -I$(top_srcdir)/include/ \
    -I$(top_srcdir)/src/ \
    $(TLM_CFLAGS) \
    -DG_LOG_DOMAIN=\"TLM_SESSIOND\" \
    $(NULL)

tlm_sessiond_LDADD = \
    libtlm-session-daemon.la \
    $(TLM_LIBS) \
    $(NULL)
