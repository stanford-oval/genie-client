#!/usr/bin/env bash

: ${LIGHT:=0}

set -ex

if test "$IP_ADDRESS" = "adb" ; then
    if [ ${LIGHT} -eq 0 ]; then
        adb shell "rm -fr /opt/genie ; mkdir -p /opt/genie/lib/gio/modules /opt/genie/lib/gstreamer-1.0 /opt/genie/assets"
        adb push out/lib /opt/genie
        adb push out/assets /opt/genie
        adb push out/pulseaudio /opt/genie/pulseaudio
    fi
    adb push scripts/launch.sh /opt/duer/dcslaunch.sh
    test -f out/config.ini && adb push out/config.ini /opt/genie/config.ini
    adb push scripts/asoundrc /opt/genie/.asoundrc
    adb push scripts/pulse-system.pa /opt/genie/.system.pa
    adb push out/genie /opt/genie/genie
else
    if [ ${LIGHT} -eq 0 ]; then
        ssh root@$IP_ADDRESS "rm -fr /opt/genie"
        sleep 1
        ssh root@$IP_ADDRESS "mkdir -p /opt/genie"
        scp -r out/lib root@$IP_ADDRESS:/opt/genie/
        scp -r out/assets root@$IP_ADDRESS:/opt/genie/
        scp out/pulseaudio root@$IP_ADDRESS:/opt/genie/
    fi
    scp scripts/launch.sh root@$IP_ADDRESS:/opt/duer/dcslaunch.sh
    test -f out/config.ini && scp out/config.ini root@$IP_ADDRESS:/opt/genie/
    scp scripts/asoundrc root@$IP_ADDRESS:/opt/genie/.asoundrc
    scp scripts/pulse-system.pa root@$IP_ADDRESS:/opt/genie/.system.pa
    scp out/genie root@$IP_ADDRESS:/opt/genie/
fi
