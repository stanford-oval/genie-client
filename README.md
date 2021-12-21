# Genie Light Client

This is a light-weight client for the [Genie Web API](https://wiki.genie.stanford.edu/api-references/web-almond) implemented using C++.

It is designed for embedded clients with limited flash storage and RAM.

Genie is a research project led by Prof. Monica Lam, from Stanford University.
You can find more information at <https://oval.cs.stanford.edu>.

## Developer Setup

You can make a local build for development a standard Linux distro using [`meson`](https://mesonbuild.com) and [`ninja`](https://ninja-build.org/).

### Step 1: Package Dependencies

Install the dependencies from your package manager. On Fedora, this is:
```bash
sudo dnf -y install meson gcc-c++ 'pkgconfig(alsa)' 'pkgconfig(glib-2.0)' 'pkgconfig(libsoup-2.4)' \
  'pkgconfig(json-glib-1.0)' 'pkgconfig(libevdev)' 'pkgconfig(gstreamer-1.0)' 'pkgconfig(speex)' 'pkgconfig(speexdsp)' \
  'pkgconfig(webrtc-audio-processing)' gstreamer1-plugins-base gstreamer1-plugins-good cmake 
```

### Step 2: Picovoice

Run:
```
./scripts/get-assets.sh
```

Review the Picovoice SDK Licence before continuing.

### Step 3: Build

Run:
```bash
meson ./build/
ninja -C ./build/
```

The compiled binary is located in ./build/src/genie-client

To use in a normal Linux installation, the binary should be installed with
```bash
ninja -C ./build/ install
```

### Step 4: Configure

The default config.ini is optimized for use in a general purpose Linux device (such as a laptop or Raspberry Pi), against
a cloud instance of Genie.

While Genie is running, limited configuration is possible by connecting to the client web interface, hosted on port 8000.
The web interface allows to login to the cloud instance, or connect to a local Genie server.

Advanced customization to target a specific hardward is possible by editing the file config.ini
in the same directory as where the client is run. The config.ini in the repository is an example and shows the default
values.
