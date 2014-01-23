include $(top_srcdir)/common.mk

bin_PROGRAMS = tlm
BUILT_SOURCES =

tlm_SOURCES = \
	tlm-utils.h \
	tlm-utils.c \
	tlm-auth-session.h \
	tlm-auth-session.c \
	tlm-session.h \
	tlm-session.c \
	tlm-seat.h \
	tlm-seat.c \
	tlm-manager.h \
	tlm-manager.c \
	tlm-main.c \
	$(NULL)

tlm_CFLAGS = \
	-I$(abs_top_builddir) \
	-I$(abs_top_srcdir)/src \
	-I$(abs_top_srcdir)/src/common \
	-D_GNU_SOURCE \
	-DG_LOG_DOMAIN=\"TLM\" \
	$(TLM_CFLAGS) \
	$(NULL)


tlm_LDADD = \
	$(TLM_LIBS) \
	$(abs_top_builddir)/src/common/libtlm-plugin.la \
	-lpam \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
