#!/bin/bash -e

export DESTDIR="$(pwd)/build/deps"

mkdir -p build/deps-src
cd build/deps-src

if [ ! -d json-glib ]; then
	git clone https://gitlab.gnome.org/GNOME/json-glib
fi
cd json-glib
git checkout 1.4.4
meson --default-library=static --prefix=/usr build .
ninja -C build install
cd ..

if [ ! -d alsa-lib ]; then
	git clone https://github.com/alsa-project/alsa-lib
fi
cd alsa-lib
git checkout v1.1.4
./gitcompile --enable-shared=no --enable-static=yes --prefix=/usr
make install
cd ..

if [ ! -d alsa-plugins ]; then
	git clone https://github.com/alsa-project/alsa-plugins
fi
cd alsa-plugins
git checkout v1.1.4
PKG_CONFIG_PATH=/opt/usr/lib/pkgconfig/ ./gitcompile --enable-pulseaudio --enable-shared=no --enable-static=yes --prefix=/usr
make install
cd ..

exit 0
