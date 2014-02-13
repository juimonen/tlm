if ENABLE_TESTS
SUBDIRS = config daemon
else
SUBDIRS =

check-local:
	@echo "ERROR: tests are enabled only if ./configure is run with --enable-tests"
	@exit 1
endif

VALGRIND_TESTS_DISABLE =
valgrind: $(SUBDIRS)
	for t in $(filter-out $(VALGRIND_TESTS_DISABLE),$(SUBDIRS)); do \
		cd $$t; $(MAKE) valgrind; cd ..;\
	done;

EXTRA_DIST = \
    valgrind.supp \
    valgrind_common.mk \
    tlm-test.conf.in \
    test_common.mk