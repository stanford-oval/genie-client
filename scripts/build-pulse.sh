#!/bin/bash
: ${PULSE_TAG:="v15.0"}

mkdir -p build/deps-src
cd build/deps-src

if [ ! -d pulseaudio ]; then
	git clone https://gitlab.freedesktop.org/pulseaudio/pulseaudio.git
fi

cd pulseaudio
git checkout ${PULSE_TAG}

rm -rf build

meson -Ddoxygen=false -Dman=false build

ninja -C build
ninja -C build install
