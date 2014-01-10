include $(top_srcdir)/common.mk

sysconf_in_files = tlm.conf.in
sysconf_DATA = $(sysconf_in_files:.conf.in=.conf)

tlm.conf:tlm.conf.in Makefile
	$(AM_V_GEN) sed -e "s|\@TLM_PLUGINS_DIR\@|$(pluginsdir)|" \
                    -e "s|\@TLM_ACCOUNTS_PLUGIN\@|default|" $< > $@
