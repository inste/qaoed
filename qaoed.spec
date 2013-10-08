Summary: Mulithreaded ATA over Ethernet storage target
Name: qaoed
Version: 1
URL: http://pi.nxs.se/~wowie/qaoed.tgz
Release: 2
License: GPL
Group: Storage/Filesystems
Source0: %{name}.tgz
Source1: %{name}
Buildroot: /tmp/%{name}-%{version}-buildroot
Prefix: /usr

%define debug_package %{nil}

%description
Qaoed is a multithreaded ATA over Ethernet storage target that is easy to
use and yet higly configurable. Without any argument qaoed will try to
read the configuration file /etc/qaoed.conf.

%prep 
%setup -n %{name}

%__make 

%pre

%post
/sbin/chkconfig --add %{name}

%preun
if [ "$1" = 0 ]
then
   /etc/init.d/%{name} stop
   /sbin/chkconfig --del %{name}
fi

%postun

%install
%__rm -rf $RPM_BUILD_ROOT
%__mkdir -p $RPM_BUILD_ROOT/usr/sbin
%__install -m 750 qaoed $RPM_BUILD_ROOT/usr/sbin

%__mkdir -p $RPM_BUILD_ROOT/etc/init.d
%__install -m 750 %{SOURCE1} $RPM_BUILD_ROOT/etc/init.d

%files
%defattr(755,root,root)
%{_sbindir}/*
/etc/init.d/%{name}

%clean
%__rm -rf $RPM_BUILD_ROOT

%changelog
* Wed Mar 07 2007 Bernard Li <bernard@vanhpc.org>
- Genesis
