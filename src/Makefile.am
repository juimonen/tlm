NULL=
bin_PROGRAMS=tlm

tlm_SOURCES = \
	tlm-utils.h \
	tlm-utils.c \
   	tlm-log.h \
    tlm-log.c \
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
    -I$(abs_top_builddir) \
    -I$(abs_top_srcdir)/src \
    $(TLM_CFLAGS) \
	-DG_LOG_DOMAIN=\"TLM\"
    $(NULL)


tlm_LDADD = \
	$(TLM_LIBS) \
	-lpam \
	$(NULL)
