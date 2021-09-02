#!/usr/bin/env bash

: ${LIGHT:=0}

set -ex

if test "$IP_ADDRESS" = "adb" ; then
    if [ ${LIGHT} -eq 0 ]; then
        adb shell "rm -fr /opt/genie ; mkdir -p /opt/genie/lib/gio/modules /opt/genie/lib/gstreamer-1.0 /opt/genie/assets"
        adb push build/lib /opt/genie
        adb push build/assets /opt/genie
    fi
    adb push scripts/launch.sh /opt/duer/dcslaunch.sh
    test -f build/config.ini && adb push build/config.ini /opt/genie/config.ini
    adb push build/src/genie /opt/genie/genie
else
    if [ ${LIGHT} -eq 0 ]; then
        ssh root@$IP_ADDRESS "rm -fr /opt/genie"
        sleep 1
        ssh root@$IP_ADDRESS "mkdir -p /opt/genie"
        scp -r build/lib root@$IP_ADDRESS:/opt/genie/
        scp -r build/assets root@$IP_ADDRESS:/opt/genie/
    fi
    scp scripts/launch.sh root@$IP_ADDRESS:/opt/duer/dcslaunch.sh
    test -f build/config.ini && scp build/config.ini root@$IP_ADDRESS:/opt/genie/
    scp build/src/genie root@$IP_ADDRESS:/opt/genie/
fi
