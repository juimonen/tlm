SUPPRESSIONS = $(top_srcdir)/test/valgrind.supp

%.valgrind: %
	@$(TESTS_ENVIRONMENT) \
	CK_FORK=no \
	CK_DEFAULT_TIMEOUT=120 \
	G_SLICE=always-malloc \
	$(LIBTOOL) --mode=execute \
	valgrind -q \
	$(foreach s,$(SUPPRESSIONS),--suppressions=$(s)) \
	--tool=memcheck --leak-check=full --trace-children=yes \
	--leak-resolution=high --num-callers=20 \
	--error-exitcode=1 \
	./$*

valgrind: $(check_PROGRAMS)
	for t in $(filter-out $(VALGRIND_TESTS_DISABLE),$(check_PROGRAMS)); do \
		$(MAKE) $$t.valgrind; \
	done;


