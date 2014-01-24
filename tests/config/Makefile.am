include $(top_srcdir)/tests/test_common.mk

TESTS = configtest
TESTS_ENVIRONMENT +=TLM_CONF_FILE=$(top_builddir)/tests/config/test.conf

check_PROGRAMS = configtest
configtest_SOURCES = config.c

configtest_CFLAGS = \
	$(TLM_CFLAGS) $(CHECK_CFLAGS) \
	-I$(top_builddir) \
	-I$(top_builddir)/src/common

configtest_LDADD = \
	$(TLM_LIBS) \
	$(CHECK_LIBS) \
	$(top_builddir)/src/common/libtlm_plugin_la-tlm-config.lo

