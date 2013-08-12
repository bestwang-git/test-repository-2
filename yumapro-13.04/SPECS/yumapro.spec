Name:           yumapro
Version:        13.04
Release:        1%{?dist}
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

%description
YumaPro is a YANG-based multi-protocol client and server SDK.
This package contains a client, server, and YANG compiler tools.

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_sbindir}/*
%{_mandir}/man1/*
%{_libdir}/*
%{_sysconfdir}/yumapro/*
%{mydocdir}/yumapro/*
%{_includedir}/yumapro/
%{_datadir}/yumapro/*

%package server
Summary:  Professional YANG-based Unified Modular Automation Tools (Server)

%description server
YumaPro is a YANG-based multi-protocol client and server
development toolkit.  The netconfd-pro server includes an automated
central NETCONF protocol stack, based directly on YANG modules.

%files server
%defattr(-,root,root,-)
%{_bindir}/yangdump-pro
%{_bindir}/yangdiff-pro
%{_bindir}/yp-shell
%{_bindir}/make_sil_dir_pro
%{_sbindir}/netconfd-pro
%{_sbindir}/netconf-subsystem-pro
%{_sbindir}/yang-api
%{_includedir}/yumapro/
%{_datadir}/yumapro/util/
%{_datadir}/yumapro/src/
%{_sysconfdir}/yumapro/yangdiff-pro-sample.conf
%{_sysconfdir}/yumapro/yangdump-pro-sample.conf
%{_sysconfdir}/yumapro/netconfd-pro-sample.conf
%{mydocdir}/yumapro/yumapro-legal-notices.pdf
%{mydocdir}/yumapro/yumapro-support-agreement.pdf
%{mydocdir}/yumapro/yumapro-server-binary-license.pdf
%{mydocdir}/yumapro/yumapro-server-source-license.pdf
%{mydocdir}/yumapro/AUTHORS
%{mydocdir}/yumapro/README
%{mydocdir}/yumapro/index.html
%{mydocdir}/yumapro/pdf/yumapro-quickstart-guide.pdf
%{mydocdir}/yumapro/pdf/yumapro-user-cmn-manual.pdf
%{mydocdir}/yumapro/html/quickstart/
%{mydocdir}/yumapro/html/user-cmn/
%{mydocdir}/yumapro/pdf/yumapro-dev-manual.pdf
%{mydocdir}/yumapro/pdf/yumapro-installation-guide.pdf
%{mydocdir}/yumapro/pdf/yumapro-netconfd-manual.pdf
%{mydocdir}/yumapro/pdf/yumapro-yangdump-manual.pdf
%{mydocdir}/yumapro/pdf/yumapro-yangdiff-manual.pdf
%{mydocdir}/yumapro/html/dev/
%{mydocdir}/yumapro/html/install/
%{mydocdir}/yumapro/html/yangdump/
%{mydocdir}/yumapro/html/yangdiff/
%{mydocdir}/yumapro/html/netconfd/
%{_mandir}/man1/yangdiff-pro.1.gz
%{_mandir}/man1/yangdump-pro.1.gz
%{_mandir}/man1/netconfd-pro.1.gz
%{_mandir}/man1/netconf-subsystem-pro.1.gz
%{_mandir}/man1/yang-api.1.gz
%{_mandir}/man1/yp-shell.1.gz
%{_mandir}/man1/make_sil_dir_pro.1.gz
%{_libdir}/libyumapro_agt.so*
%{_libdir}/libyumapro_ncx.so*
%{_libdir}/yumapro/
%{_datadir}/yumapro/modules/*

%package client
Summary: Professional YANG-based Unified Modular Automation Tools (Client)
%description client
YumaPro is a YANG-based multi-protocol client and server
development toolkit.  The yangcli-pro client supports multiple
sessions over SSH with script and test-suite support.


%files client
%defattr(-,root,root,-)
%{_bindir}/yangcli-pro
%{_sysconfdir}/yumapro/yangcli-pro-sample.conf
%{mydocdir}/yumapro/yumapro-legal-notices.pdf
%{mydocdir}/yumapro/yumapro-support-agreement.pdf
%{mydocdir}/yumapro/yumapro-client-site-license.pdf
%{mydocdir}/yumapro/yumapro-client-user-license.pdf
%{mydocdir}/yumapro/AUTHORS
%{mydocdir}/yumapro/README
%{mydocdir}/yumapro/index.html
%{_mandir}/man1/yangcli-pro.1.gz
%{_datadir}/yumapro/modules/*
%{mydocdir}/yumapro/pdf/yumapro-quickstart-guide.pdf
%{mydocdir}/yumapro/pdf/yumapro-user-cmn-manual.pdf
%{mydocdir}/yumapro/pdf/yumapro-installation-guide.pdf
%{mydocdir}/yumapro/pdf/yumapro-yangcli-manual.pdf
%{mydocdir}/yumapro/html/quickstart/
%{mydocdir}/yumapro/html/user-cmn/
%{mydocdir}/yumapro/html/install/
%{mydocdir}/yumapro/html/yangcli/

%include yumapro-changelog

