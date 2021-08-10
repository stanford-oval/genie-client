# Genie Light Client

This is a light-weight client for the [Genie Web API](https://wiki.almond.stanford.edu/api-references/web-almond) implemented using C++.

It is designed for embedded clients with limited flash storage and RAM.

Genie is a research project led by Prof. Monica Lam, from Stanford University.

You can find more information at <https://almond.stanford.edu>.

## Developer Setup

You can make a local build for development a standard Linux distro using [`meson`](https://mesonbuild.com) and [`ninja`](https://ninja-build.org/).

### Step 1: Package Dependencies

Install the dependencies from your package manager. On Fedora, this is:
```bash
sudo dnf -y install meson gcc-c++ 'pkgconfig(alsa)' 'pkgconfig(glib-2.0)' 'pkgconfig(libsoup-2.4)' 'pkgconfig(json-glib-1.0)' 'pkgconfig(libevdev)' 'pkgconfig(gstreamer-1.0)' gstreamer1-plugins-base gstreamer1-plugins-good
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

The compiled binary is located in ./build/src/genie

### Step 4: Configure

The default config.ini is optimized for a specific hardward. To customize for development use, edit config.ini and set:

- `sink`: set this to the GStreamer element to use for audio output; on a normal Linux installation, this should be commented out
- `input`: set this to the ALSA input device to use for input; use `arecord -L` to list available devices. This must be a real ALSA device, it cannot be `pulse` or `pipewire`
- `url` + `accessToken`: set this to the URL and access token of the Almond instance to access
