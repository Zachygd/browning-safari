%global debug_package %{nil}
%global _docdir_fmt %{name}

Name:           browning-safari
Version:        1.02
Release:        1%{?dist}
Summary:        Native Linux port of The Browning African Safari (1998)
License:        LicenseRef-Proprietary
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc
BuildRequires:  pkgconfig(sdl2)
Recommends:     (kdialog or zenity)

%description
A native SDL2 port of The Browning African Safari (Oquirrh Productions /
Multimedia Holding LC, 1998), rebuilt from the original Windows program.

No game data is included. On first start the game copies its data from the
original CD or from a SAFARI.iso image into ~/.local/share/browning-safari.

%prep
%autosetup

%build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=%{_prefix} \
      -DCMAKE_C_FLAGS="%{optflags}" -DCMAKE_EXE_LINKER_FLAGS="%{?build_ldflags}"
cmake --build build %{?_smp_mflags}

%install
DESTDIR=%{buildroot} cmake --install build
rm -f %{buildroot}%{_docdir}/%{name}/README.md

%files
%doc README.md
%{_bindir}/browning-safari
%{_datadir}/applications/browning-safari.desktop
%{_datadir}/icons/hicolor/scalable/apps/browning-safari.svg
%{_datadir}/metainfo/browning-safari.metainfo.xml

%changelog
* Mon Oct 12 2026 Zach - 1.02-1
- Native SDL2 port of version 1.02
