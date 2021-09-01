#!/bin/bash
: ${BLOB:=0}
: ${SH:=0}
: ${STATIC:=0}
: ${LIGHT:=0}

: ${ARCH:="amd64"}
if [ -n "${1}" ]; then
	ARCH=$1
fi

case ${ARCH} in
	amd64)
	;;
	armhf)
		ARCH="arm32v7"
	;;
	arm64)
		ARCH="arm64v8"
	;;
	*)
		echo "Unsupported arch: ${1}"
		exit 1
	;;
esac

set -e
set -x

# docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

docker build \
	--build-arg ARCH=${ARCH}/ \
	--build-arg STATIC=${STATIC} \
	-t genie-builder:${ARCH} \
	-f scripts/Dockerfile \
	.

if [ ${SH} -eq 1 ]; then
	docker run --rm -it -v $(pwd):/out --security-opt label=disable genie-builder:${ARCH} /bin/bash
	exit ${?}
fi

# copy the artifacts to the output directory
if [ ${BLOB} -eq 1 ]; then
	docker run --rm -v $(pwd)/build:/out --security-opt label=disable -e ARCH=${ARCH} genie-builder:${ARCH} /src/scripts/blob.sh
	exit ${?}
fi

if [ ${LIGHT} -eq 1 ]; then
	docker run --rm -v $(pwd)/build:/out --security-opt label=disable -e ARCH=${ARCH} genie-builder:${ARCH} /src/scripts/binonly.sh
	exit ${?}
fi

exit 0
