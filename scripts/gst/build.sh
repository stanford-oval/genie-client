#!/bin/bash -ex
: ${STATIC:=0}
: ${GST_TAG:="1.18.4"}

mkdir -p build/deps-src
cd build/deps-src

if [ ! -d gst-build ]; then
	git clone https://github.com/GStreamer/gst-build
fi
cd gst-build
git checkout ${GST_TAG}

rm -rf build
mkdir -p build/
PARAMS=""
if [ ${STATIC} -eq 1 ]; then
	PARAMS="--default-library=static"
fi
#	-Dgst-plugins-base:vorbis=enabled \
meson ${PARAMS} --prefix=/usr/local -Dintrospection=disabled -Dauto_features=disabled -Dgood=enabled \
	-Dgst-plugins-base:alsa=enabled \
	-Dgst-plugins-base:ogg=enabled \
	-Dgst-plugins-base:playback=enabled \
	-Dgst-plugins-base:typefind=enabled \
	-Dgst-plugins-base:volume=enabled \
	-Dgst-plugins-base:vorbis=enabled \
	-Dgst-plugins-base:audioconvert=enabled \
	-Dgst-plugins-base:audioresample=enabled \
	-Dgst-plugins-good:autodetect=enabled \
	-Dgst-plugins-good:wavparse=enabled \
	-Dgst-plugins-good:mpg123=enabled \
	-Dgst-plugins-good:soup=enabled \
	-Dgst-plugins-good:pulse=enabled \
	build

# patch for souphttpsrc
cd subprojects/gst-plugins-good
patch -p1 < "../../../../../scripts/gst/gst-plugin-souphttpsrc-postdata.patch"
cd ../../

ninja -C build
ninja -C build install
