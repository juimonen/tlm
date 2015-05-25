NULL=
INTERFACES_DIR = $(top_builddir)/src/common/dbus/interfaces
DBUS_BUILT_SOURCES = \
    tlm-dbus-login-gen.c \
    tlm-dbus-login-gen.h \
    tlm-dbus-session-gen.c \
    tlm-dbus-session-gen.h \
    tlm-dbus-launcher-gen.c \
    tlm-dbus-launcher-gen.h \
    $(NULL)
BUILT_SOURCES = $(DBUS_BUILT_SOURCES)

DBUS_BUILT_DOCS = \
    tlm-dbus-login-doc-gen-org.O1.Tlm.Login.xml \
    tlm-dbus-session-doc-gen-org.O1.Tlm.Session.xml \
    tlm-dbus-launcher-doc-gen-org.O1.Tlm.Launcher.xml \
    $(NULL)

DBUS_INTERFACE_PREFIX="org.O1.Tlm."
tlm-dbus-login-gen.c tlm-dbus-login-gen.h : $(INTERFACES_DIR)/org.O1.Tlm.Login.xml
	gdbus-codegen                                       \
       --interface-prefix $(DBUS_INTERFACE_PREFIX)      \
       --c-namespace TlmDbus                       \
       --generate-c-code  tlm-dbus-login-gen     \
       --generate-docbook tlm-dbus-login-doc-gen \
       $<

tlm-dbus-session-gen.c tlm-dbus-session-gen.h : $(INTERFACES_DIR)/org.O1.Tlm.Session.xml
	gdbus-codegen                                       \
       --interface-prefix $(DBUS_INTERFACE_PREFIX)      \
       --c-namespace TlmDbus                       \
       --generate-c-code  tlm-dbus-session-gen     \
       --generate-docbook tlm-dbus-session-doc-gen \
       $<

tlm-dbus-launcher-gen.c tlm-dbus-launcher-gen.h : $(INTERFACES_DIR)/org.O1.Tlm.Launcher.xml
	gdbus-codegen                                       \
       --interface-prefix $(DBUS_INTERFACE_PREFIX)      \
       --c-namespace TlmDbus                       \
       --generate-c-code  tlm-dbus-launcher-gen     \
       --generate-docbook tlm-dbus-launcher-doc-gen \
       $<

noinst_LTLIBRARIES = libtlm-dbus-glue.la

libtlm_dbus_glue_la_CPPFLAGS = \
    -I$(top_srcdir)/src \
    -I$(top_builddir)/src \
    $(TLM_CFLAGS) \
    $(NULL)

libtlm_dbus_glue_la_LIBADD = \
    $(TLM_LIBS) \
    $(NULL)

nodist_libtlm_dbus_glue_la_SOURCES =  \
   $(DBUS_BUILT_SOURCES) \
   $(NULL)

libtlm_dbus_glue_la_SOURCES =  \
   tlm-dbus-server-interface.c \
   tlm-dbus-server-interface.h \
   tlm-dbus-server-p2p.c \
   tlm-dbus-server-p2p.h \
   tlm-dbus.h \
   tlm-dbus-utils.c \
   tlm-dbus-utils.h \
   $(NULL)

EXTRA_DIST = interfaces tlm-dbus-login-doc-gen-org.O1.Tlm.Login.xml \
  tlm-dbus-session-doc-gen-org.O1.Tlm.Session.xml \
  tlm-dbus-launcher-doc-gen-org.O1.Tlm.Launcher.xml

all-local: copy_xml_doc

mostlyclean-local:
	rm -rf $(abs_top_builddir)/docs/tlm-dbus-login-doc-gen-org.O1.Tlm.Login.xml
	rm -rf $(abs_top_builddir)/docs/tlm-dbus-session-doc-gen-org.O1.Tlm.Session.xml
	rm -rf $(abs_top_builddir)/docs/tlm-dbus-launcher-doc-gen-org.O1.Tlm.Launcher.xml

copy_xml_doc: tlm-dbus-login-gen.c tlm-dbus-session-gen.c tlm-dbus-launcher-gen.c
	cp -f $(abs_top_builddir)/src/common/dbus/tlm-dbus-login-doc-gen-org.O1.Tlm.Login.xml $(abs_top_builddir)/docs/tlm-dbus-login-doc-gen-org.O1.Tlm.Login.xml
	chmod +w $(abs_top_builddir)/docs/tlm-dbus-login-doc-gen-org.O1.Tlm.Login.xml
	cp -f $(abs_top_builddir)/src/common/dbus/tlm-dbus-session-doc-gen-org.O1.Tlm.Session.xml $(abs_top_builddir)/docs/tlm-dbus-session-doc-gen-org.O1.Tlm.Session.xml
	chmod +w $(abs_top_builddir)/docs/tlm-dbus-session-doc-gen-org.O1.Tlm.Session.xml
	cp -f $(abs_top_builddir)/src/common/dbus/tlm-dbus-launcher-doc-gen-org.O1.Tlm.Launcher.xml $(abs_top_builddir)/docs/tlm-dbus-launcher-doc-gen-org.O1.Tlm.Launcher.xml
	chmod +w $(abs_top_builddir)/docs/tlm-dbus-launcher-doc-gen-org.O1.Tlm.Launcher.xml

clean-local:
	rm -f *~ $(DBUS_BUILT_SOURCES) $(DBUS_BUILT_DOCS)
