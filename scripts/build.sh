#!/bin/bash -ex
: ${CROSS:=""}
: ${STATIC:=0}

EXTRA=""
if [ ${STATIC} -eq 1 ]; then
	EXTRA="-Dstatic=true"
	#export LDFLAGS=-static-libstdc++
fi

mkdir -p build
if [ -n "${CROSS}" ]; then
	meson . build --cross-file scripts/cross-${CROSS}.txt
else
	meson . build ${EXTRA}
fi

ninja -C build clean
ninja -C build

ls -l build/src/genie-client

exit 0
