# NOTE  I defaulted `ARCH` to the xiaodu architecture because it was how I was
#       able to run VSCode in the container -- couldn't get the IDE to provide
#       the right build arg to bring up an `arm32v7` container.
ARG ARCH=arm32v7/
ARG STATIC=0
FROM ${ARCH}debian:stretch-slim
ARG ARCH

# baseline deps
RUN apt-get update && \
    apt-get -y --allow-unauthenticated install build-essential pkg-config ninja-build git nano wget \
        libasound2-dev libglib2.0-dev libjson-glib-dev libsoup2.4-dev libevdev-dev libgstreamer1.0-dev \
        python3-pip flex bison libmount-dev libffi-dev libsemanage-dev libpulse-dev libogg-dev libvorbis-dev libmpg123-dev \
        libspeex-dev libspeexdsp-dev sound-theme-freedesktop

RUN pip3 install meson==0.54.0
RUN mkdir -p /src/scripts /out
WORKDIR /src

# custom build of gstreamer
COPY scripts/gst /src/scripts/gst
RUN ./scripts/gst/build.sh

# custom build of webrtc (only vad)
COPY scripts/webrtcvad /src/scripts/webrtcvad
RUN ./scripts/webrtcvad/build.sh

# assets
COPY assets /src/assets
COPY scripts/get-assets.sh /src/scripts
RUN ./scripts/get-assets.sh ${ARCH}

# our actual build
COPY . /src/
RUN ./scripts/build.sh
