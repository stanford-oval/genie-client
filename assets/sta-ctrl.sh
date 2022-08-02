#!/bin/sh

function help() {
	echo "$0 <start/stop>"
	exit 1
}

cmd="$1"
if [ -z "$cmd" ]; then
	help
fi

platform="emb"
if [ -f /etc/lsb-release ]; then
	platform="deb"
fi

if [ "$cmd" = "start" ] || [ "$cmd" = "stop" ] || [ "$cmd" = "restart" ]; then
	if [ "$platform" = "deb" ]; then
		systemctl $cmd wpa_supplicant
	elif [ "$platform" = "emb" ]; then
		/etc/init.d/S42wifi $cmd
	fi
else
	help
fi

exit 0
