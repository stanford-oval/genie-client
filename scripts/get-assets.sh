#!/bin/bash -ex

if [ -z "${1}" ]; then
    ARCH=$(uname -m)
else
    ARCH="${1}"
fi

if [ "${ARCH}" == "amd64" ] || [ "${ARCH}" == "amd64/" ] || [ "${ARCH}" == "x86_64" ]; then
    LIB_PATH="linux/x86_64"
elif [ "${ARCH}" == "armhf" ] || [ "${ARCH}" == "arm32v7/" ] || [ "${ARCH}" == "armv7l" ] || [ "${ARCH}" == "armv7" ]; then
    LIB_PATH="raspberry-pi/cortex-a72"
elif [ "${ARCH}" == "arm64" ] || [ "${ARCH}" == "arm64v8/" ] || [ "${ARCH}" == "aarch64" ]; then
    LIB_PATH="raspberry-pi/cortex-a72-aarch64"
else
    echo "Unsupported: ${ARCH}"
    exit 1
fi

LIB_DIST=$(echo ${LIB_PATH} | cut -d'/' -f1)
KEYWORD="computer"
DESTDIR="assets/"

mkdir -p ${DESTDIR}
wget https://github.com/Picovoice/porcupine/raw/v1.9/resources/keyword_files/${LIB_DIST}/${KEYWORD}_${LIB_DIST}.ppn -O ${DESTDIR}/keyword.ppn
wget https://github.com/Picovoice/porcupine/raw/v1.9/lib/${LIB_PATH}/libpv_porcupine.so -O ${DESTDIR}/libpv_porcupine.so
wget https://github.com/Picovoice/porcupine/raw/v1.9/lib/common/porcupine_params.pv -O ${DESTDIR}/porcupine_params.pv

# audio (vorbis vs raw)
#ffmpeg -y -i /usr/share/sounds/freedesktop/stereo/message-new-instant.oga -ar 22050 -ac 1 ${DESTDIR}/match.wav
#ffmpeg -y -i /usr/share/sounds/freedesktop/stereo/dialog-warning.oga -ar 22050 -ac 1 ${DESTDIR}/no-match.wav
cp /usr/share/sounds/freedesktop/stereo/message-new-instant.oga ${DESTDIR}/match.oga
cp /usr/share/sounds/freedesktop/stereo/dialog-warning.oga ${DESTDIR}/no-match.oga
cp /usr/share/sounds/freedesktop/stereo/alarm-clock-elapsed.oga ${DESTDIR}/alarm-clock-elapsed.oga

exit 0
