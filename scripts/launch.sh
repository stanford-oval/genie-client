#!/bin/bash
cd /opt/genie
export TZ=UTC
export HOME=/opt/genie
if [ -z "$(pidof ntpd)" ]; then
	/usr/bin/start_ntp.sh &
fi
amixer -D hw:audiocodec cset name='LINEOUT volume' 3
amixer -D hw:audiocodec cset name='MIC1 gain volume' 6
amixer -D hw:audiocodec cset name='MIC2 gain volume' 6

export GIO_EXTRA_MODULES=/opt/genie/lib/gio/modules
export LD_LIBRARY_PATH=/opt/genie/lib/pulseaudio:/opt/genie/lib/:/lib:/usr/lib
export GST_REGISTRY_UPDATE=no
export GST_PLUGIN_PATH=/opt/genie/lib/gstreamer-1.0
export GST_PLUGIN_SCANNER=/opt/genie/lib/gstreamer-1.0/gst-plugin-scanner
export XDG_CONFIG_HOME=/tmp/.config

mkdir -p /tmp/.config
grep -qe '^backend=pulse' config.ini
if [ $? -eq 0 ]; then
	./pulseaudio --start -v -F /opt/genie/.system.pa -p /opt/genie/lib/pulseaudio --exit-idle-time=-1 --log-target=file:/tmp/pa.log
fi

RET=1
while [ $RET -ne 0 ]; do
	if test "$1" = "--gdb" ; then
		./gdbserver 0.0.0.0:${GDB_PORT:-1234} ./genie
	else
		./genie >>/tmp/genie.log 2>&1
	fi
	RET=$?
done

exit $RET
