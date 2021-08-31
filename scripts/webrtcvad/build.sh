#!/bin/bash -e

mkdir -p build/deps-src
cd build/deps-src

if [ ! -d webrtc ]; then
	git clone https://webrtc.googlesource.com/src webrtc
fi
cd webrtc

if [ ! -d abseil-cpp ]; then
	git clone https://github.com/abseil/abseil-cpp
fi

cp ../../../scripts/webrtcvad/meson.build .
cp ../../../scripts/webrtcvad/webrtcvad.pc.in .

meson --prefix=/usr/local build

ninja -C build
sudo ninja -C build install
