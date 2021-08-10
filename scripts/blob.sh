#!/bin/bash

: ${ARCH:="amd64"}

SEARCH_ARCH=""
case ${ARCH} in
	amd64)
		SEARCH_ARCH="x86_64-linux-gnu"
	;;
	arm32v7)
		SEARCH_ARCH="arm-linux-gnueabihf"
	;;
	arm64v8)
		SEARCH_ARCH="aarch64-linux-gnu"
	;;
	*)
		echo "Unsupported arch: ${ARCH}"
		exit 1
	;;
esac

DESTDIR="/out/lib"
export LD_LIBRARY_PATH="/usr/local/lib/${SEARCH_ARCH}"

deps=""
blacklistDeps="libicudata.* libasound.* libxml2.*"

findDeps() {
	local src="${1}"
	echo "finding deps for $src"

	ldd "$src"
	for a in $(ldd ${src} | grep -E "=> (/|build/)" | awk '{print $3}'); do
		if [ -z "${a}" ]; then continue; fi

		isBL=0
		for bl in ${blacklistDeps}; do
			echo "${a}" | grep -qe "${bl}"
			if [ ${?} -eq 0 ]; then
				echo "blacklist dependency ${a}"
				isBL=1
			fi
		done

		if [ $isBL -eq 0 ]; then
			echo "${deps}" | grep -qe "${a}"
			if [ ${?} -eq 1 ]; then
				deps+="${a} "
				findDeps "${a}"
			fi
		fi
	done
}

findDeps build/src/genie

mkdir -p ${DESTDIR} ${DESTDIR}/gio/modules ${DESTDIR}/gstreamer-1.0

plugins="coreelements alsa playback wavparse ogg mpg123 typefindfunctions soup vorbis audioconvert audioresample"
for p in ${plugins} ; do
	cp -p /usr/local/lib/${SEARCH_ARCH}/gstreamer-1.0/libgst${p}.so ${DESTDIR}/gstreamer-1.0/
	findDeps /usr/local/lib/${SEARCH_ARCH}/gstreamer-1.0/libgst${p}.so
done
cp -p /usr/local/libexec/gstreamer-1.0/gst-plugin-scanner ${DESTDIR}/gstreamer-1.0/

findDeps /usr/lib/${SEARCH_ARCH}/gio/modules/libgiognutls.so
cp -p /usr/lib/${SEARCH_ARCH}/gio/modules/libgiognutls.so ${DESTDIR}/gio/modules/

deps=$(echo $deps | sort | uniq)

echo "copying deps ..."
for a in ${deps}; do
	echo "copying ${a}"
	cp -p ${a} ${DESTDIR}/
done
chmod +x ${DESTDIR}/*

mkdir -p /out/src
cp -p build/src/genie /out/src/

exit 0
