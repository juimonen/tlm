TESTS_ENVIRONMENT = \
	G_MESSAGES_DEBUG="all" \
    LD_LIBRARY_PATH="$(top_builddir)/src/common/.libs:$(top_builddir)/src/common/dbus/.libs:$(top_builddir)/src/daemon/.libs:$(top_builddir)/src/daemon/dbus/.libs:$(top_builddir)/src/plugins/.libs:/usr/local/lib/"