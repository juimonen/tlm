include $(top_srcdir)/tests/test_common.mk

TESTS = daemontest
TESTS_ENVIRONMENT += \
    TLM_BIN_DIR=$(top_builddir)/src/daemon/.libs \
    TLM_CONF_FILE=$(top_builddir)/tests/tlm-test.conf \
    TLM_PLUGINS_DIR=$(top_builddir)/src/plugins/.libs

VALGRIND_TESTS_DISABLE=

check_PROGRAMS = daemontest
include $(top_srcdir)/tests/valgrind_common.mk

daemontest_SOURCES = daemon-test.c

daemontest_CFLAGS = \
    -I$(abs_top_srcdir)/src \
    $(TLM_CFLAGS) \
    $(CHECK_CFLAGS) \
    -U G_LOG_DOMAIN \
    -DG_LOG_DOMAIN=\"tlm-test-daemon\"

daemontest_LDADD = \
    $(TLM_LIBS) \
    $(CHECK_LIBS) \
    $(abs_top_builddir)/src/daemon/tlm-tlm-utils.o \
    $(abs_top_builddir)/src/daemon/dbus/libtlm-dbus.la \
    $(abs_top_builddir)/src/common/dbus/libtlm-dbus-glue.la

CLEANFILES = *.gcno *.gcda
