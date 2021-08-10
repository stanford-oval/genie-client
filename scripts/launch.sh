#!/bin/bash
cd /opt/genie
export GIO_EXTRA_MODULES=lib/gio/modules
export LD_LIBRARY_PATH=lib/
export GST_REGISTRY_UPDATE=no
export GST_PLUGIN_PATH=/opt/genie/lib/gstreamer-1.0
export GST_PLUGIN_SCANNER=/opt/genie/lib/gstreamer-1.0/gst-plugin-scanner
./genie
