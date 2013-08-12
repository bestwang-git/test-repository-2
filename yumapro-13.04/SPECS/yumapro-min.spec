Name:           yumapro
Version:        13.04
Release:        1
Summary:        Professional YANG-based Unified Modular Automation Tools

Group:          Development/Tools
License:        YumaWorks
URL:            http://www.yumaworks.com/
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
YumaPro is a YANG-based multi-protocol client and server
development toolkit.

BuildRequires: ncurses-devel
BuildRequires: libxml2-devel
BuildRequires: libssh2-devel

%define debug_package %{nil}

%define myrel 1

%define mycflags PACKAGE_BUILD=1 WITH_CLI=1 WITH_YANGAPI=1 PRODUCTION=1 RELEASE=%{myrel}

%define mydocdir /usr/share/doc

# project build rules

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
make DOC=1 %{mycflags}

%install
rm -rf $RPM_BUILD_ROOT
%ifarch x86_64
make %{mycflags} install LDFLAGS+=--build-id LIB64=1 %{mycflags} DESTDIR=$RPM_BUILD_ROOT
%else
make %{mycflags} install LDFLAGS+=--build-id %{mycflags} DESTDIR=$RPM_BUILD_ROOT
%endif

%clean
rm -rf $RPM_BUILD_ROOT


%package server
Summary:  Professional YANG-based Unified Modular Automation Tools (Server)

%description server
YumaPro is a YANG-based multi-protocol client and server
development toolkit.  The netconfd-pro server includes an automated
central NETCONF protocol stack, based directly on YANG modules.

Requires: yumapro-modules
Requires: libyumapro-ncx

%files server
%defattr(-,root,root,-)
%{_bindir}/yangdump-pro
%{_bindir}/yangdiff-pro
%{_sbindir}/netconfd-pro
%{_sbindir}/netconf-subsystem-pro
%{_sysconfdir}/yumapro/yangdiff-pro-sample.conf
%{_sysconfdir}/yumapro/yangdump-pro-sample.conf
%{_sysconfdir}/yumapro/netconfd-pro-sample.conf
%{mydocdir}/yumapro/yumapro-legal-notices.pdf
%{mydocdir}/yumapro/yumapro-license.pdf
%{mydocdir}/yumapro/AUTHORS
%{mydocdir}/yumapro/README
%{mydocdir}/yumapro/YUMA-COPYRIGHT
%{_mandir}/man1/yangdiff-pro.1.gz
%{_mandir}/man1/yangdump-pro.1.gz
%{_mandir}/man1/netconfd-pro.1.gz
%{_mandir}/man1/netconf-subsystem-pro.1.gz
%{_libdir}/libyumapro_agt.so*
%{_libdir}/yumapro/


%package client
Summary:  Professional YANG-based Unified Modular Automation Tools (Client)

%description client
YumaPro is a YANG-based multi-protocol client and server
development toolkit.  The yangcli-pro client supports multiple
sessions over SSH with script and test-suite support.

Requires: yumapro-modules
Requires: libyumapro-ncx

%files client
%defattr(-,root,root,-)
%{_bindir}/yangcli-pro
%{_sysconfdir}/yumapro/yangcli-pro-sample.conf
%{mydocdir}/yumapro/yumapro-legal-notices.pdf
%{mydocdir}/yumapro/yumapro-license.pdf
%{mydocdir}/yumapro/AUTHORS
%{mydocdir}/yumapro/README
%{mydocdir}/yumapro/YUMA-COPYRIGHT
%{_mandir}/man1/yangcli-pro.1.gz

%package -n libyumapro-ncx
Summary:  Professional YANG-based Unified Modular Automation Tools (NCX Library)

%description -n libyumapro-ncx
YumaPro is a YANG-based multi-protocol client and server
development toolkit.  This package contains the NCX library.

%files -n libyumapro-ncx
%defattr(-,root,root,-)
%{_libdir}/libyumapro_ncx.so*
%{mydocdir}/yumapro/yumapro-legal-notices.pdf
%{mydocdir}/yumapro/yumapro-license.pdf
%{mydocdir}/yumapro/AUTHORS
%{mydocdir}/yumapro/README
%{mydocdir}/yumapro/YUMA-COPYRIGHT

%include yumapro-changelog

