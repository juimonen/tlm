NULL=
INTERFACES_DIR = $(top_builddir)/src/common/dbus/interfaces
DBUS_BUILT_SOURCES = \
    tlm-dbus-login-gen.c \
    tlm-dbus-login-gen.h \
    $(NULL)

DBUS_BUILT_DOCS = \
    tlm-dbus-login-doc-gen-org.tizen.Tlm.Login.xml \
    $(NULL)

DBUS_INTERFACE_PREFIX="org.tizen.Tlm."
tlm-dbus-login-gen.c tlm-dbus-login-gen.h : $(INTERFACES_DIR)/org.tizen.Tlm.Login.xml
	gdbus-codegen                                       \
       --interface-prefix $(DBUS_INTERFACE_PREFIX)      \
       --c-namespace TlmDbus                       \
       --generate-c-code  tlm-dbus-login-gen     \
       --generate-docbook tlm-dbus-login-doc-gen \
       $<

noinst_LTLIBRARIES = libtlm-dbus-glue.la

libtlm_dbus_glue_la_CPPFLAGS = \
    $(TLM_CFLAGS) \
    $(NULL)

libtlm_dbus_glue_la_LIBADD = \
    $(TLM_LIBS) \
    $(NULL)

libtlm_dbus_glue_la_SOURCES =  \
   $(DBUS_BUILT_SOURCES) \
   $(NULL)

EXTRA_DIST = interfaces

clean-local :
	rm -f *~ $(DBUS_BUILT_SOURCES) $(DBUS_BUILT_DOCS)

