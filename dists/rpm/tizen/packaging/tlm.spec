# enable debug features such as control environment variables
# WARNING! do not use for production builds as it will break security
%define debug_build 0

Name: tlm
Summary: Login manager for Tizen
Version: 0.0.2
Release: 7
Group: System/Service
License: LGPL-2.1+
Source: %{name}-%{version}.tar.gz
URL: https://github.com/01org/tlm
Source1001: %{name}.manifest
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: gumd
BuildRequires: pkgconfig(glib-2.0) >= 2.30
BuildRequires: pkgconfig(gobject-2.0)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(gio-unix-2.0)
BuildRequires: pkgconfig(gmodule-2.0)
BuildRequires: pkgconfig(libgum)
BuildRequires: pkgconfig(elementary)
BuildRequires: pam-devel

%description
%{summary}.


%package devel
Summary:    Development files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}


%description devel
%{summary}.


%package doc
Summary:    Documentation files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}


%description doc
%{summary}.


%prep
%setup -q -n %{name}-%{version}
cp %{SOURCE1001} .


%build
%if %{debug_build} == 1
%configure --enable-gum --enable-gtk-doc --enable-examples --enable-debug
%else
%configure --enable-gum --enable-examples
%endif
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
%make_install
install -m 755 -d %{buildroot}%{_libdir}/systemd/system
install -m 644 data/tlm.service %{buildroot}%{_libdir}/systemd/system/
install -m 755 -d %{buildroot}%{_sysconfdir}/pam.d
install -m 644 data/tlm-login %{buildroot}%{_sysconfdir}/pam.d/


%post
/sbin/ldconfig


%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
%doc AUTHORS COPYING INSTALL NEWS README
%{_bindir}/%{name}
%{_libdir}/lib%{name}*.so.*
%{_libdir}/%{name}/plugins/*.so*
%{_libdir}/systemd/system/tlm.service
%config(noreplace) %{_sysconfdir}/tlm.conf
%config %{_sysconfdir}/pam.d/tlm-login


%files devel
%defattr(-,root,root,-)
%{_includedir}/%{name}/*.h
%{_libdir}/lib%{name}*.so
%{_libdir}/pkgconfig/%{name}.pc
%{_bindir}/tlm-ui


%files doc
%defattr(-,root,root,-)
%{_datadir}/gtk-doc/html/tlm/*
