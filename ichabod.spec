
%define name ichabod

Summary: Ichabod Server
Name: %{name}
Version: %{expand:%%(cat version.h | awk '{print $3;}' | tr -d \")}
Release: 1
License: Spec file is LGPL, binary rpm is gratis but non-distributable
Group: Applications/System
BuildRoot: %{_topdir}/BUILD/%{name}-%{version}-%{release}
Source: %{name}-%{version}.tar.gz

%description
Image rasterization and javasript evaluation

%prep
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
cd $RPM_BUILD_ROOT
cp %{expand:%%(pwd)}/ichabod ./usr/bin/

%build

%install

%clean

%files
%defattr(-,root,root)
%doc

/usr/bin/ichabod

%changelog
