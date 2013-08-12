Name:           yumapro-eval
Version:        13.04
Release:        1%{?dist}
Summary:        Professional YANG-based Unified Modular Automation Tools

Group:          Development/Tools
License:        YumaWorks
URL:            http://www.yumaworks.com/
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
YumaPro is a YANG-based NETCONF-over-SSH client and server
development toolkit.  The netconfd server includes an automated
central NETCONF protocol stack, based directly on YANG modules.
The yangcli client supports single sessions over SSH with some
script support.  The yangdump and yangdiff development tools are also
included, to compile and process YANG modules.

Requires: ncurses
Requires: libxml2
Requires: libssh2

%define debug_package %{nil}

%define myrel 1

%define mycflags PACKAGE_BUILD=1 EVAL=1 PRODUCTION=1 RELEASE=%{myrel}

%define mydocdir /usr/share/doc

# main package rules

%prep
%setup -q

%build
cd libtecla
./configure --prefix=$RPM_BUILD_ROOT 
cd ..
%ifarch x86_64
make LIB64=1 %{mycflags}
%else
make %{mycflags}
%endif

%install
rm -rf $RPM_BUILD_ROOT
%ifarch x86_64
make %{mycflags} install LDFLAGS+=--build-id LIB64=1 %{mycflags} DESTDIR=$RPM_BUILD_ROOT
%else
make %{mycflags} install LDFLAGS+=--build-id %{mycflags} DESTDIR=$RPM_BUILD_ROOT
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/yangcli-pro
%{_sbindir}/netconfd-pro
%{_sbindir}/netconf-subsystem-pro
%{_sysconfdir}/yumapro/netconfd-pro-sample.conf
%{_sysconfdir}/yumapro/yangcli-pro-sample.conf
%{mydocdir}/yumapro/yumapro-legal-notices.pdf
%{mydocdir}/yumapro/yumapro-eval-license.pdf
%{mydocdir}/yumapro/AUTHORS
%{mydocdir}/yumapro/README
%{mydocdir}/yumapro/pdf/
%{mydocdir}/yumapro/html/
%{_mandir}/man1/netconfd-pro.1.gz
%{_mandir}/man1/netconf-subsystem-pro.1.gz
%{_mandir}/man1/yangcli-pro.1.gz
%{_datadir}/yumapro/modules/*
%{_libdir}/libyumapro_ncx.so*
%{_libdir}/libyumapro_agt.so*
%{_libdir}/yumapro/
%{_datadir}/yumapro/src/

%post
echo "YumaPro evaluation version installed."
echo "Check the documentation in the %{mydocdir}/yumapro directory."

%include yumapro-changelog

