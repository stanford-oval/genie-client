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

mkdir -p assets assets/computer
cp submodules/porcupine/resources/keyword_files/${LIB_DIST}/computer_${LIB_DIST}.ppn assets/computer/keyword.ppn
cp submodules/porcupine/lib/${LIB_PATH}/libpv_porcupine.so assets/libpv_porcupine.so
cp submodules/porcupine/lib/common/porcupine_params.pv assets/porcupine_params.pv

cp /usr/share/sounds/freedesktop/stereo/message-new-instant.oga assets/match.oga
cp /usr/share/sounds/freedesktop/stereo/dialog-warning.oga assets/no-match.oga
cp /usr/share/sounds/freedesktop/stereo/alarm-clock-elapsed.oga assets/alarm-clock-elapsed.oga

exit 0
