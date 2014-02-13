include $(top_srcdir)/tests/test_common.mk

TESTS = configtest
TESTS_ENVIRONMENT +=TLM_CONF_FILE=$(abs_top_srcdir)/tests/config/test.conf

check_PROGRAMS = configtest
configtest_SOURCES = config.c

configtest_CFLAGS = \
	$(TLM_CFLAGS) $(CHECK_CFLAGS) \
	-I$(abs_top_srcdir)/src/common

configtest_LDADD = \
	$(TLM_LIBS) \
	$(CHECK_LIBS) \
	$(abs_top_builddir)/src/common/libtlm_plugin_la-tlm-config.lo

EXTRA_DIST = test.conf
