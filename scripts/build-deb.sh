#!/bin/sh

set -e
set -x

mkdir -p ./debian/out
for arch in ${ARCH:-arm32v7 arm32v6 arm64v8 amd64} ; do
    if test ${arch} = arm32v6 ; then
        CXXFLAGS="-g -O2 -ffile-prefix-map=/src=. -fstack-protector-strong -Wformat -Werror=format-security -march=armv6+fp -marm"
        BASE_IMAGE=arm32v7
    else
        CXXFLAGS="-g -O2 -ffile-prefix-map=/src=. -fstack-protector-strong -Wformat -Werror=format-security"
        BASE_IMAGE=${arch}
    fi

    docker build --build-arg "ARCH=${arch}" --build-arg "CXXFLAGS=${CXXFLAGS}" \
        --build-arg "BASE_IMAGE=${BASE_IMAGE}" \
        -f scripts/Dockerfile.debian -t localhost/genie-builder:debian-${arch} .
    docker run --rm -v ./debian/out:/out --entrypoint bash --security-opt label=disable \
        localhost/genie-builder:debian-${arch} -c 'cp /genie-client* /out'
    if test ${arch} = arm32v7 ; then
        rename armhf armv7 debian/out/*
    fi
done
