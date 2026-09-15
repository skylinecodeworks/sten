Name:           sten
Version:        @VERSION@
Release:        1%{?dist}
Summary:        ScatterBit steganography for images
License:        MIT
URL:            https://github.com/skylinecodeworks/sten
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make

%description
sten hides a message inside an image by modifying its least significant
bytes using a pseudo-random ScatterBit walk, so the payload is spread over
the whole carrier. The modified image keeps its format and stays visually
unchanged. Written in pure C with no external libraries.

Supported carrier formats: BMP, PNG, GIF, JPEG (COM segment), PNM/PAM,
TGA, TIFF and ICO.

%prep
%setup -q

%build
make %{?_smp_mflags}

%check
make test

%install
make install DESTDIR=%{buildroot} PREFIX=/usr

%files
%license LICENSE
%doc README.md
%{_bindir}/sten
%{_mandir}/man1/sten.1*

%changelog
* Tue Sep 15 2026 Skyline Code Works <skylinecodeworks@users.noreply.github.com> - @VERSION@-1
- Initial release
