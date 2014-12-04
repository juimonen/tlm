# enable debug features such as control environment variables
# WARNING! do not use for production builds as it will break security
%define debug_build 0
%define efl 0

%if %{debug_build} == 1
%define extra_config_options1 --enable-gtk-doc --enable-debug
%endif

%if %{efl} == 1
%define extra_config_options1 --enable-examples
%endif


Name: tlm
Summary: Login manager for Tizen
Version: 1.0.1
Release: 0
Group: System/Service
License: LGPL-2.1+
Source: %{name}-%{version}.tar.gz
URL: https://github.com/01org/tlm
Source1001: %{name}.manifest
Requires(post): /sbin/ldconfig, systemd
Requires(postun): /sbin/ldconfig, systemd
Requires: gumd
Requires: libsystemd
Requires: tlm-config
BuildRequires: pkgconfig(glib-2.0) >= 2.30
BuildRequires: pkgconfig(gobject-2.0)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(gio-unix-2.0)
BuildRequires: pkgconfig(gmodule-2.0)
BuildRequires: pkgconfig(libgum)
BuildRequires: pam-devel
%if %{debug_build} == 1
BuildRequires: gtk-doc
%endif
%if %{efl} == 1
BuildRequires: pkgconfig(elementary)
%endif

%description
Login manager daemon for Tizen.


%package devel
Summary:    Development files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
%{summary} package.


%package doc
Summary:    Documentation files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description doc
%{summary} package.


%if "%{profile}" != "ivi"

%package config-common
Summary:    Configuration files for common-profile
Group:      System/Service
Requires:   %{name} = %{version}-%{release}
Provides:   tlm-config

%description config-common
Tizen Login Manager configuration files for common-profile.

%else

%package config-ivi-singleseat
Summary:    Configuration files for ivi-profile with single seat
Group:	    System/Service
Requires:   %{name} = %{version}-%{release}
Provides:   tlm-config
Conflicts:  tlm-config-ivi-multiseat, tlm-config-ivi-vtc1010

%description config-ivi-singleseat
Generic Tizen Login Manager configuration files for ivi-profile with
single seat.


%package config-ivi-multiseat
Summary:    Configuration files for ivi-profile with multi seat
Group:	    System/Service
Requires:   %{name} = %{version}-%{release}
Provides:   tlm-config
Conflicts:  tlm-config-ivi-singleseat, tlm-config-ivi-vtc1010

%description config-ivi-multiseat
Generic Tizen Login Manager configuration files for ivi-profile with
multi seat.


%package config-ivi-vtc1010
Summary:    Configuration files for ivi-profile on VTC-1010
Group:      System/Service
Requires:   %{name} = %{version}-%{release}
Provides:   tlm-config
Conflicts:  tlm-config-ivi-singleseat, tlm-config-ivi-multiseat

%description config-ivi-vtc1010
Tizen Login Manager configuration files for ivi-profile on VTC-1010 hardware.

%endif


%prep
%setup -q -n %{name}-%{version}
cp %{SOURCE1001} .


%build
%if %{debug_build} == 1
./autogen.sh
%endif
%reconfigure --enable-gum \
             %{?extra_config_options1:%extra_config_options1} \
             %{?extra_config_options2:%extra_config_options2}
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
%make_install
rm -f %{buildroot}%{_sysconfdir}/tlm.conf
install -m 755 -d %{buildroot}%{_unitdir}
install -m 644 data/tlm.service %{buildroot}%{_unitdir}
install -m 755 -d %{buildroot}%{_sysconfdir}/pam.d
install -m 644 data/tlm-login %{buildroot}%{_sysconfdir}/pam.d/
install -m 644 data/tlm-default-login %{buildroot}%{_sysconfdir}/pam.d/
install -m 644 data/tlm-system-login %{buildroot}%{_sysconfdir}/pam.d/
install -m 755 -d %{buildroot}%{_sysconfdir}/session.d
install -m 755 -d %{buildroot}%{_sysconfdir}/xdg/weston
%if "%{profile}" == "ivi"
install -m 644 data/tizen-ivi/etc/tlm*.conf %{buildroot}%{_sysconfdir}
install -m 755 data/tizen-ivi/etc/session.d/* %{buildroot}%{_sysconfdir}/session.d/
install -m 644 data/tizen-ivi/weston-*.ini %{buildroot}%{_sysconfdir}/xdg/weston/
install -m 755 -d %{buildroot}%{_sysconfdir}/udev/rules.d
install -m 644 data/tizen-ivi/10-multiseat-vtc1010.rules %{buildroot}%{_sysconfdir}/udev/rules.d/
%else
install -m 644 data/tizen-common/etc/tlm.conf %{buildroot}%{_sysconfdir}
install -m 755 data/tizen-common/etc/session.d/* %{buildroot}%{_sysconfdir}/session.d/
%endif


%post
/sbin/ldconfig
/usr/bin/systemctl enable tlm
/usr/bin/systemctl daemon-reload


%preun
if [ $1 == 0 ]; then
	/usr/bin/systemctl disable tlm
	/usr/bin/systemctl daemon-reload
fi

%postun -p /sbin/ldconfig


%if "%{profile}" == "ivi"

%post config-ivi-singleseat
if [ ! -e /etc/tlm.conf ] || [ -h /etc/tlm.conf ]; then
ln -s -f /etc/tlm-singleseat.conf /etc/tlm.conf
fi

%postun config-ivi-singleseat
if [ -h /etc/tlm.conf ]; then
rm -f /etc/tlm.conf
fi


%post config-ivi-multiseat
if [ ! -e /etc/tlm.conf ] || [ -h /etc/tlm.conf ]; then
ln -s -f /etc/tlm-multiseat.conf /etc/tlm.conf
fi

%postun config-ivi-multiseat
if [ -h /etc/tlm.conf ]; then
rm -f /etc/tlm.conf
fi


%post config-ivi-vtc1010
if [ ! -e /etc/tlm.conf ] || [ -h /etc/tlm.conf ]; then
ln -s -f /etc/tlm-vtc1010.conf /etc/tlm.conf
fi

%postun config-ivi-vtc1010
if [ -h /etc/tlm.conf ]; then
rm -f /etc/tlm.conf
fi

%endif


%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
%license COPYING
%doc AUTHORS NEWS README
%{_bindir}/%{name}
%{_bindir}/%{name}-sessiond
%{_bindir}/%{name}-client
%{_bindir}/%{name}-weston-launch
%{_libdir}/lib%{name}*.so.*
%{_libdir}/%{name}/plugins/*.so*
%{_unitdir}/tlm.service
%config %{_sysconfdir}/pam.d/tlm-login
%config %{_sysconfdir}/pam.d/tlm-default-login
%config %{_sysconfdir}/pam.d/tlm-system-login


%files devel
%defattr(-,root,root,-)
%{_includedir}/%{name}/*.h
%{_libdir}/lib%{name}*.so
%{_libdir}/pkgconfig/%{name}.pc
%if %{efl} == 1
%{_bindir}/tlm-ui
%endif


%files doc
%defattr(-,root,root,-)
%{_datadir}/gtk-doc/html/tlm/*


%if "%{profile}" != "ivi"

%files config-common
%defattr(-,root,root,-)
%manifest %{name}.manifest
%config(noreplace) %{_sysconfdir}/tlm.conf
%config(noreplace) %{_sysconfdir}/session.d/*

%else

%files config-ivi-singleseat
%defattr(-,root,root,-)
%manifest %{name}.manifest
%config(noreplace) %{_sysconfdir}/tlm-singleseat.conf
%config(noreplace) %{_sysconfdir}/session.d/genivi-session-singleseat
%config(noreplace) %{_sysconfdir}/session.d/user-session
%config(noreplace) %{_sysconfdir}/session.d/user-session.modello
%config(noreplace) %{_sysconfdir}/xdg/weston/weston-genivi.ini
%config(noreplace) %{_sysconfdir}/xdg/weston/weston-user.ini


%files config-ivi-multiseat
%defattr(-,root,root,-)
%manifest %{name}.manifest
%config(noreplace) %{_sysconfdir}/tlm-multiseat.conf
%config(noreplace) %{_sysconfdir}/session.d/genivi-session-multiseat
%config(noreplace) %{_sysconfdir}/session.d/user-session
%config(noreplace) %{_sysconfdir}/session.d/user-session.modello
%config(noreplace) %{_sysconfdir}/xdg/weston/weston-genivi.ini
%config(noreplace) %{_sysconfdir}/xdg/weston/weston-user.ini


%files config-ivi-vtc1010
%defattr(-,root,root,-)
%manifest %{name}.manifest
%config(noreplace) %{_sysconfdir}/tlm-vtc1010.conf
%config(noreplace) %{_sysconfdir}/session.d/genivi-session-vtc1010
%config(noreplace) %{_sysconfdir}/session.d/user-session
%config(noreplace) %{_sysconfdir}/session.d/user-session.modello
%config(noreplace) %{_sysconfdir}/xdg/weston/weston-genivi-vtc1010.ini
%config(noreplace) %{_sysconfdir}/xdg/weston/weston-user.ini
%config(noreplace) %{_sysconfdir}/udev/rules.d/*

%endif

