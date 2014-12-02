ACLOCAL_AMFLAGS = -I m4

# configure flags used in by distcheck
AM_DISTCHECK_CONFIGURE_FLAGS = \
    --enable-distcheck \
    --enable-debug

SUBDIRS= src data docs

if ENABLE_GTK_DOC
AM_DISTCHECK_CONFIGURE_FLAGS += --enable-gtk-doc
endif


if ENABLE_EXAMPLES
AM_DISTCHECK_CONFIGURE_FLAGS += --enable-examples
SUBDIRS += examples
endif

if ENABLE_TESTS
AM_DISTCHECK_CONFIGURE_FLAGS += --enable-tests
SUBDIRS += tests
endif

EXTRA_DIST = dists tools autogen.sh

valgrind:
	cd tests; make valgrind

lcov: check
	@rm -rf lcov-report
	@lcov --no-external -c --directory src/ --output-file cov.output
	@lcov -r cov.output common/dbus/\* daemon/dbus/\*.c --output-file lcov.output
	@genhtml lcov.output --output-directory lcov-report
	@rm lcov.output cov.output
	@echo "Coverage report is in file://$(abs_srcdir)/lcov-report/index.html"
