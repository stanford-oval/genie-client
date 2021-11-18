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

export LD_LIBRARY_PATH="/usr/local/lib/${SEARCH_ARCH}"

deps=""
# blocklistDeps="libicudata.* libasound.* libxml2.* libpthread.so.0 libc.so.6 libm.so.6 librt.so.1 libdl.so.2 libresolv.so.2"
blocklistDeps="libicudata.* libasound.* libxml2.*"

findDeps() {
	local src="${1}"
	echo "finding deps for $src"

	ldd "$src"
	for a in $(ldd ${src} | grep -E "=> (/|build/)" | awk '{print $3}'); do
		if [ -z "${a}" ]; then continue; fi

		isBL=0
		for bl in ${blocklistDeps}; do
			echo "${a}" | grep -qe "${bl}"
			if [ ${?} -eq 0 ]; then
				echo "blocklist dependency ${a}"
				isBL=1
				break
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

findDeps build/src/genie-client

DESTDIR="/out/lib"
rm -rf "${DESTDIR}"
mkdir -p ${DESTDIR} ${DESTDIR}/gio/modules ${DESTDIR}/gstreamer-1.0 ${DESTDIR}/pulseaudio

plugins="coreelements autodetect alsa pulseaudio playback wavparse audioparsers ogg mpg123 id3demux icydemux typefindfunctions soup vorbis volume audioconvert audioresample"
for p in ${plugins} ; do
	deps+="/usr/local/lib/${SEARCH_ARCH}/gstreamer-1.0/libgst${p}.so "
	findDeps /usr/local/lib/${SEARCH_ARCH}/gstreamer-1.0/libgst${p}.so
done
cp -p /usr/local/libexec/gstreamer-1.0/gst-plugin-scanner ${DESTDIR}/gstreamer-1.0/
strip ${DESTDIR}/gstreamer-1.0/gst-plugin-scanner

deps+="/usr/lib/${SEARCH_ARCH}/gio/modules/libgiognutls.so "
findDeps /usr/lib/${SEARCH_ARCH}/gio/modules/libgiognutls.so

findDeps /usr/local/bin/pulseaudio
pulse_modules="libalsa-util libprotocol-native module-native-protocol-unix module-alsa-sink module-alsa-source module-null-sink module-always-sink module-null-sink module-echo-cancel module-role-ducking"
for p in ${pulse_modules} ; do
	cp -p /usr/local/lib/${SEARCH_ARCH}/pulse-15.0/modules/${p}.so ${DESTDIR}/pulseaudio
	strip ${DESTDIR}/pulseaudio/${p}.so
	findDeps /usr/local/lib/${SEARCH_ARCH}/pulse-15.0/modules/${p}.so
done

deps=$(echo $deps | sort | uniq)

echo "copying deps ..."
rm -fr /out/debug/.build-id
cp -rp /usr/lib/debug/.build-id /out/debug/.build-id
for a in ${deps}; do
	dest=$(echo "${a}" | sed -E -e "s|/usr/local/lib/${SEARCH_ARCH}/pulse-15.0/modules|${DESTDIR}/pulseaudio|g" -e  "s|(/usr(/local)?)?/lib/${SEARCH_ARCH}|${DESTDIR}|g")
	echo "copying ${a} into ${dest}"
	build_id=$(file -L "${a}" | grep -E -o 'BuildID\[sha1\]=[a-f0-9]+' | cut -d'=' -f2)
	if test -n "$build_id" ; then
		first_two=$(echo $build_id | grep -E -o '^..')
		rest=$(echo $build_id | sed -E 's/^..//g')
		mkdir -p /out/debug/.build-id/$first_two
		if test -f /out/debug/.build-id/$first_two/$rest.debug ; then
			echo "debugging information already present"
		else
			echo "saving debug information into /out/debug/.build-id/$first_two/$rest.debug"
			objcopy --only-keep-debug "${a}" /out/debug/.build-id/$first_two/$rest.debug
		fi
	else
		echo "missing build id information"
	fi

	cp -p -T ${a} ${dest}
	strip ${dest}
done

cp /lib/arm-linux-gnueabihf/libnss_* /out/lib/

cp -p /usr/local/bin/pulseaudio /out/
strip /out/pulseaudio

cp -p /usr/local/bin/{parec,paplay,pacmd,pactl} /out/
strip /out/{parec,paplay,pacmd,pactl}

cp -p /usr/bin/gdbserver /out/
strip /out/gdbserver

cp -p build/src/genie-client /out/

mkdir -p /out/assets
cp -p assets/* /out/assets/

exit 0
