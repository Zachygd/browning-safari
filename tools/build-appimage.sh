#!/bin/bash
# Build Browning_African_Safari-<version>-x86_64.AppImage in out/.
#
# Needs: cmake, gcc, curl, and the X11/Wayland/audio development headers SDL2
# builds against (Fedora: sudo dnf builddep SDL2). The AppImage tools and the
# SDL2 source are downloaded into out/ the first time.
#
# SDL2 is built here with its X11, Wayland, PipeWire, PulseAudio and ALSA
# backends loaded at run time, so the AppImage carries just libSDL2 and uses
# the host's display and audio libraries. glibc also comes from the host, so
# build on the oldest distro you want to support.
set -euo pipefail
top=$(cd "$(dirname "$0")/.." && pwd)
out=$top/out
tools=$out/tools
appdir=$out/AppDir
version=$(sed -n 's/^Version:[[:space:]]*//p' "$top/packaging/browning-safari.spec")
release=$(sed -n 's/^Release:[[:space:]]*\([0-9]*\).*/\1/p' "$top/packaging/browning-safari.spec")

mkdir -p "$tools"
fetch() {
    [ -x "$tools/$2" ] && return 0
    echo ">> downloading $2"
    curl -fsSL -o "$tools/$2" "$1"
    chmod +x "$tools/$2"
}
fetch https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage linuxdeploy
fetch https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage appimagetool
# run the tools without FUSE (works in containers and on systems without libfuse2)
export APPIMAGE_EXTRACT_AND_RUN=1

sdl_ver=2.32.10
sdl=$out/sdl2-$sdl_ver
if [ ! -f "$sdl/lib/libSDL2-2.0.so.0" ]; then
    echo ">> building SDL2 $sdl_ver"
    curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-$sdl_ver/SDL2-$sdl_ver.tar.gz" | tar xz -C "$out"
    cmake -S "$out/SDL2-$sdl_ver" -B "$out/sdl2-build" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$sdl" -DSDL_STATIC=OFF -DSDL_TEST=OFF \
        -DSDL_X11_SHARED=ON -DSDL_WAYLAND_SHARED=ON -DSDL_PULSEAUDIO_SHARED=ON \
        -DSDL_PIPEWIRE_SHARED=ON -DSDL_ALSA_SHARED=ON -DSDL_LIBDECOR_SHARED=ON \
        -DSDL_SNDIO=OFF -DSDL_JACK=OFF >/dev/null
    cmake --build "$out/sdl2-build" -j"$(nproc)" >/dev/null
    cmake --install "$out/sdl2-build" >/dev/null
fi

echo ">> building the game"
rm -rf "$out/build-appimage" "$appdir"
cmake -S "$top" -B "$out/build-appimage" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_PREFIX_PATH="$sdl" >/dev/null
cmake --build "$out/build-appimage" -j"$(nproc)" >/dev/null
DESTDIR=$appdir cmake --install "$out/build-appimage" >/dev/null

echo ">> bundling libraries"
LD_LIBRARY_PATH="$sdl/lib" "$tools/linuxdeploy" --appdir "$appdir" \
    --desktop-file "$appdir/usr/share/applications/browning-safari.desktop" \
    --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/browning-safari.svg" \
    --executable "$appdir/usr/bin/browning-safari" >/dev/null

echo ">> packing"
name="Browning_African_Safari-$version-$release-x86_64.AppImage"
ARCH=x86_64 "$tools/appimagetool" --no-appstream "$appdir" "$out/$name" >/dev/null
echo ">> $out/$name"
