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

# Remove -fcommon after package updated to a new enough version.
# Used for allowing uninitialized global variables in a common block.
%make_build CFLAGS="-fcommon $RPM_OPT_FLAGS"

%install
%make_install
rm -rf %{buildroot}%{_docdir}/dirmngr
rm -rf %{buildroot}%{_datadir}/info
rm -rf %{buildroot}%{_datadir}/man

%find_lang dirmngr

%files -f dirmngr.lang
%defattr(-,root,root,-)
%license COPYING
%{_bindir}/*
