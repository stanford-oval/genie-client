#!/bin/sh

set -e
set -x

DEBIAN_RELEASE=bullseye

# normal debian build

outdir=./debian/out/debian/${DEBIAN_RELEASE}
mkdir -p ${outdir}
for arch in ${ARCH:-arm32v7 arm64v8 amd64} ; do
    BASE_IMAGE=${arch}/debian:${DEBIAN_RELEASE}-slim
    docker build --build-arg "ARCH=${arch}" --build-arg "BASE_IMAGE=${BASE_IMAGE}" \
        -f scripts/Dockerfile.debian -t localhost/genie-builder:debian-${arch} .

    docker run --rm -v ${outdir}:/out --entrypoint bash --security-opt label=disable \
        localhost/genie-builder:debian-${arch} -c 'cp /genie-client* /out'
done
(cd ${outdir} ; dpkg-scanpackages -m . /dev/null > Packages)
gzip -k ${outdir}/Packages

# raspios build
RASPIOS_VERSION=${2021-10-30}

outdir=./debian/out/raspios/${DEBIAN_RELEASE}
mkdir -p ${outdir}
if ! test -f debian/out/raspios-${DEBIAN_RELEASE}.img ; then
    wget https://downloads.raspberrypi.org/raspios_lite_armhf/images/raspios_lite_armhf-2021-11-08/${RASPIOS_VERSION}-raspios-${DEBIAN_RELEASE}-armhf-lite.zip
    unzip ${RASPIOS_VERSION}-raspios-${DEBIAN_RELEASE}-armhf-lite.zip
    mv ${RASPIOS_VERSION}-raspios-${DEBIAN_RELEASE}-armhf-lite.img debian/out/raspios-${DEBIAN_RELEASE}.img
    rm -f ${RASPIOS_VERSION}-raspios-${DEBIAN_RELEASE}-armhf-lite.zip
    qemu-img resize -f raw debian/out/raspios-${DEBIAN_RELEASE}.img 10G
    parted debian/out/raspios-${DEBIAN_RELEASE}.img rm 1 resizepart 2 11G set 2 boot on
fi
builddir=`mktemp -d`
sudo systemd-nspawn --image debian/out/raspios-${DEBIAN_RELEASE}.img \
    --overlay $(pwd):${builddir}:/src --bind $(realpath ${outdir}):/out \
    /src/scripts/build-raspios.sh
(cd ${outdir} ; dpkg-scanpackages -m . /dev/null > Packages)
gzip -k ${outdir}/Packages
