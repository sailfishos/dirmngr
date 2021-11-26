Name:       dirmngr
Summary:    GnuPG utility to manage certificates
Version:    1.0.3
Release:    1
License:    GPLv2
URL:        ftp://ftp.gnupg.org/gcrypt/dirmngr
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(libgcrypt)
BuildRequires:  gettext
BuildRequires:  libassuan-devel >= 1.0.4
BuildRequires:  libgpg-error-devel
BuildRequires:  libksba-devel
BuildRequires:  pth-devel

%description
A module that handles the Certificate Revocation Lists (CRLs)

%prep
%setup -q -n %{name}-%{version}/%{name}-1.0.3

%build
%configure --disable-static --disable-doc
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%make_install
rm -rf %{buildroot}%{_docdir}/dirmngr
rm -rf %{buildroot}%{_datadir}/info
rm -rf %{buildroot}%{_datadir}/man

%find_lang dirmngr

%files -f dirmngr.lang
%defattr(-,root,root,-)
%license COPYING
%{_bindir}/*
