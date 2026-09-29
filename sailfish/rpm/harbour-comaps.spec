Name:       harbour-comaps
Summary:    Offline maps and navigation
Version:    0.1
Release:    1
License:    ASL 2.0
URL:        https://codeberg.org/comaps/comaps
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Positioning)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(glesv2)
BuildRequires:  pkgconfig(zlib)
BuildRequires:  python3-base
BuildRequires:  desktop-file-utils
BuildRequires:  cmake >= 3.22

# Everything CoMaps needs beyond the platform is linked statically from 3party/, into a private
# libcomaps.so that other packages must neither see nor depend on.
%define debug_package %{nil}
%define __provides_exclude_from ^%{_datadir}/.*$
%define __requires_exclude ^libcomaps.*$

%description
CoMaps: offline maps, search and navigation, built on OpenStreetMap data.

%prep
%setup -q -n %{name}-%{version}

%build
# The cmake macro defaults to shared libraries, but the CoMaps libraries depend on each other
# in cycles, which only static libraries allow. Keep macros and apostrophes out of comments
# here: rpm expands the former and sfdk rewrites this section.
%cmake -DBUILD_SAILFISH=ON -DSKIP_TESTS=ON -DSKIP_TOOLS=ON -DSKIP_QT_GUI=ON -DUSE_CCACHE=OFF \
       -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
%make_build

%install
# Only the app: the bundled 3party libraries have install rules of their own.
DESTDIR=%{buildroot} cmake -P sailfish/cmake_install.cmake
desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications \
   %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
