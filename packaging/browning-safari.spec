%global debug_package %{nil}
%global _docdir_fmt %{name}

Name:           browning-safari
Version:        1.02
Release:        3%{?dist}
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
* Sat Oct 17 2026 Zach - 1.02-3
- Fix a crash at the airport while the trophy fees were being counted
- The guide's remarks after a shot now match what happened (the original
  played the next clip in the list, so kills and misses got the wrong lines)
- Shots are checked against the scenery at the place you're looking at,
  not the start of the panorama, so bushes no longer block clear shots
- Sound functions check their arguments
* Tue Oct 13 2026 Zach - 1.02-2
- Closing the window now quits immediately from any screen (saving first)
- Fix sounds that could hang the game waiting for the wrong channel
- Enter/Esc close one-button message boxes; Esc reliably skips speech
- Lower CPU use
* Mon Oct 12 2026 Zach - 1.02-1
- Native SDL2 port of version 1.02
