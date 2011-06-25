#!/bin/bash

BABELD=${BABELD:-"babeld"}
AHCPD=${AHCPD:-"ahcpd"}

WDEV=${1:-"wlan0"}
ESSID=${2:-"hacdc-babel"}
CHAN=${3:-"9"}

kill_nm() {
	#Fedora
	/etc/init.d/NetworkManager stop >/dev/null 2>&1 || true

	#Ubuntu
	stop network-manager >/dev/null 2>&1 || true

	/sbin/iptables -F
}

connect_to_adhoc() {
	/sbin/ifconfig "${WDEV}" down
	/sbin/iwconfig "${WDEV}" mode ad-hoc essid "${ESSID}" channel "${CHAN}"
	/sbin/ifconfig "${WDEV}" up
}

start_babel() {
	"$BABELD" -D -g 33123 "${WDEV}"
}

start_ahcpd() {
	"$AHCPD" -D "${WDEV}"
}

main() {
	kill_nm
	connect_to_adhoc
	start_babel
	start_ahcpd
}

main

