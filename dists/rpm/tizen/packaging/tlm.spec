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
Version: 1.0.0
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
%{summary} files


%package devel
Summary:    Development files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}


%description devel
%{summary} files


%package doc
Summary:    Documentation files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}


%description doc
%{summary} files


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
install -m 755 -d %{buildroot}%{_unitdir}
install -m 644 data/tlm.service %{buildroot}%{_unitdir}
install -m 755 -d %{buildroot}%{_sysconfdir}/pam.d
install -m 644 data/tlm-login %{buildroot}%{_sysconfdir}/pam.d/
install -m 644 data/tlm-default-login %{buildroot}%{_sysconfdir}/pam.d/
install -m 644 data/tlm-system-login %{buildroot}%{_sysconfdir}/pam.d/
install -m 755 -d %{buildroot}%{_sysconfdir}/session.d
%if "%{profile}" == "common"
install -m 644 data/tizen-common/etc/tlm.conf %{buildroot}%{_sysconfdir}
install -m 755 data/tizen-common/etc/session.d/* %{buildroot}%{_sysconfdir}/session.d/
%endif
%if "%{profile}" == "ivi"
install -m 644 data/tizen-ivi/etc/tlm.conf %{buildroot}%{_sysconfdir}
install -m 755 data/tizen-ivi/etc/session.d/* %{buildroot}%{_sysconfdir}/session.d/
%endif


%post
/sbin/ldconfig
/usr/bin/systemctl enable tlm
/usr/bin/systemctl daemon-reload


%postun
/sbin/ldconfig
/usr/bin/systemctl disable tlm
/usr/bin/systemctl daemon-reload


%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
%license COPYING
%doc AUTHORS NEWS README
%{_bindir}/%{name}
%{_bindir}/%{name}-sessiond
%{_bindir}/%{name}-client
%{_libdir}/lib%{name}*.so.*
%{_libdir}/%{name}/plugins/*.so*
%{_unitdir}/tlm.service
%config(noreplace) %{_sysconfdir}/tlm.conf
%config %{_sysconfdir}/pam.d/tlm-login
%config %{_sysconfdir}/pam.d/tlm-default-login
%config %{_sysconfdir}/pam.d/tlm-system-login
%config(noreplace) %{_sysconfdir}/session.d/*


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
