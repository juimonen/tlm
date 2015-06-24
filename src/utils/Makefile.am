## Process this file with automake to produce Makefile.in

bin_PROGRAMS = tlm-seatconf

tlm_seatconf_SOURCES = tlm-seatconf.c
tlm_seatconf_CFLAGS = \
    -I$(top_srcdir)/src \
    $(TLM_INCLUDES) \
    $(TLM_CFLAGS) \
    $(DEPS_CFLAGS) \
    -DTLM_SYSCONF_DIR='"$(sysconfdir)"'
tlm_seatconf_LDADD = \
    $(GLIB_LIBS) \
    $(DEPS_LIBS)

CLEANFILES = *.gcno *.gcda
