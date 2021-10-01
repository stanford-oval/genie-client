# NOTE  I defaulted `ARCH` to the xiaodu architecture because it was how I was
#       able to run VSCode in the container -- couldn't get the IDE to provide
#       the right build arg to bring up an `arm32v7` container.
ARG ARCH=arm32v7/
ARG STATIC=0
FROM ${ARCH}debian:stretch-slim
ARG ARCH

# baseline deps
RUN apt-get update && \
    apt-get -y --allow-unauthenticated install build-essential pkg-config \
        ninja-build git nano wget libasound2-dev libglib2.0-dev \
        libjson-glib-dev libsoup2.4-dev libevdev-dev libgstreamer1.0-dev \
        python3 python3-pip flex bison libmount-dev libffi-dev libsemanage-dev \
        libogg-dev libvorbis-dev libmpg123-dev libspeex-dev libspeexdsp-dev \
        sound-theme-freedesktop gdb gdbserver libtdb-dev libsndfile-dev check \
        libwebrtc-audio-processing-dev libglib2.0-0-dbg libstdc++6-6-dbg \
        zlib1g-dev libncurses5-dev libgdbm-dev libnss3-dev libssl-dev \
        libreadline-dev libsqlite3-dev libbz2-dev

# Extra dev stuff (VSCode dev containers)
# ============================================================================

# cli needs Python 3.8+
RUN cd /tmp \
    && wget https://www.python.org/ftp/python/3.9.7/Python-3.9.7.tgz \
    && tar -xf Python-3.9.7.tgz \
    && cd Python-3.9.7 \
    && ./configure \
    && make -j 8 \
    && make altinstall \
    && cd /tmp \
    && rm ./Python-3.9.7.tgz \
    && rm -rf ./Python-3.9.7

# and the cli deps need Rust (?!?) to install. We want it for `spotifyd` anyways
RUN curl https://sh.rustup.rs -sSf | bash -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

RUN pip3 install meson==0.54.0
RUN mkdir -p /src/scripts /out
WORKDIR /src

# CLI dev deps (for VSCode dev container)
COPY cli/requirements.txt /src/cli/requirements.txt
RUN python3.9 -m pip install -r /src/cli/requirements.txt

# Repo
# ============================================================================

# custom build of pulseaudio
COPY scripts/build-pulse.sh /src/scripts/build-pulse.sh
RUN ./scripts/build-pulse.sh

# custom build of gstreamer
COPY scripts/gst /src/scripts/gst
RUN ./scripts/gst/build.sh

# assets
COPY assets /src/assets
COPY scripts/get-assets.sh /src/scripts
RUN ./scripts/get-assets.sh ${ARCH}

# our actual build
COPY . /src/
RUN ./scripts/build.sh
