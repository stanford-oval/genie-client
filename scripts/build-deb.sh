#!/bin/sh

set -e
set -x

mkdir -p ./debian/out
for arch in ${ARCH:-arm32v7 arm64v8 amd64} ; do
    docker build --build-arg ARCH=${arch} -f scripts/Dockerfile.debian -t localhost/genie-builder:debian-${arch} .
    docker run --rm -v ./debian/out:/out --entrypoint bash --security-opt label=disable \
        localhost/genie-builder:debian-${arch} -c 'cp /genie-client* /out'
done
