#!/bin/sh

: ${HOSTAPD_TPL="/etc/conf/hostapd.conf"}
: ${HOSTAPD_CONF="/data/hostap/hostapd.conf"}
: ${UDHCPD_TPL:=""}
: ${UDHCPD_CONF:="/etc/conf/udhcpd.conf"}

function help() {
	echo "$0 <start/stop> <wlan-if> [ssid (only for start)]"
	exit 1
}

function config_hostapd() {
	ssid=$1
	cat ${HOSTAPD_TPL} | sed "s|ssid=.*|ssid=${ssid}|" > ${HOSTAPD_CONF}
	return 0
}

function start_ap_emb() {
	local ssid=$1
	local i=$2
	config_hostapd $ssid
	killall wpa_supplicant
	sleep 1
	killall hostapd udhcpc
	killall -9 dhcpcd udhcpd
	ifconfig $i down
	ifconfig $i 192.168.2.1 netmask 255.255.255.0
	ifconfig $i up
	hostapd ${HOSTAPD_CONF} &
	udhcpd -S ${UDHCPD_CONF}
	return 0
}

function stop_ap_emb() {
	killall -9 udhcpd hostapd
	return 0
}

function start_ap_deb() {
	config_hostapd $1
	systemctl stop hostapd
	systemctl stop udhcpd
	systemctl stop wpa_supplicant
	systemctl start hostapd
	systemctl start udhcpd
	return 0
}

function stop_ap_deb() {
	systemctl stop hostapd
	systemctl stop udhcpd
	return 0
}

cmd="$1"
wlan_if="$2"
ssid="$3"
if [ -z "$cmd" ]; then
	help
fi

platform="emb"
if [ -f /etc/lsb-release ]; then
	platform="deb"
fi

if [ "$platform" = "deb" ]; then
	if [ "$cmd" = "start" ]; then
		start_ap_deb $1 $2
	elif [ "$cmd" = "stop" ]; then
		stop_ap_deb $1
	else
		help
	fi
elif [ "$platform" = "emb" ]; then
	if [ "$cmd" = "start" ]; then
		start_ap_emb $1 $2
	elif [ "$cmd" = "stop" ]; then
		stop_ap_emb $1
	else
		help
	fi
fi

exit 0
