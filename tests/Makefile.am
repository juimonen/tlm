if ENABLE_TESTS

TESTS = configtest
TESTS_ENVIRONMENT=TLM_CONF_FILE=$(top_builddir)/tests/test.conf

check_PROGRAMS = configtest
configtest_SOURCES = config.c 

configtest_CFLAGS = \
	$(TLM_CFLAGS) $(CHECK_CFLAGS) \
	-I$(top_builddir) \
	-I$(top_builddir)/src/daemon

configtest_LDADD = \
	$(TLM_LIBS) \
	$(CHECK_LIBS) \
	$(top_builddir)/src/daemon/tlm-tlm-config.o

endif
