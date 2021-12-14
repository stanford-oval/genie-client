#!/bin/bash -ex

apt-get update

apt-get -y install \
    debhelper devscripts wget \
    pkg-config meson ninja-build libasound2-dev libglib2.0-dev libjson-glib-dev \
    libsoup2.4-dev libpulse-dev libevdev-dev libgstreamer1.0-dev sound-theme-freedesktop \
    libwebrtc-audio-processing-dev libspeex-dev libspeexdsp-dev

cd /src
git clean -fdx
./scripts/get-assets.sh armv6

rm -fr /genie-client*
debuild -us -uc

cp /genie-client* /out
