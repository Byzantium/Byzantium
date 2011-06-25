#!/bin/bash

BABELD=${BABELD:-"babeld"}
AHCPD=${AHCPD:-"ahcpd"}

#WDEV=${1:-"wlan0"}

kill_nm() {
	#Fedora
	/etc/init.d/NetworkManager stop >/dev/null 2>&1 || true

	#Ubuntu
	stop network-manager >/dev/null 2>&1 || true

	iptables -F
}

connect_to_adhoc() {
	/sbin/ifconfig "${WDEV}" down
	/sbin/iwconfig "${WDEV}" mode ad-hoc essid "${ESSID}" channel "${CHAN}"
	/sbin/ifconfig "${WDEV}" up
}

start_babel() {
	"$BABELD" -D \
		-g 33123 \
		-C "redistribute local if ${WDEV}" \
		-C "redistribute local deny" \
		-C "redistribute metric 128" \
		"${WDEV}"
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

WDEV=""

while [ $# -gt 0 ]
do
	case "$1" in
		"-e")
			ESSID="$2"
			shift 1
			;;
		"-c")
			CHAN="$2"
			shift 1
			;;
		*)
			WDEV="$1"
			;;
	esac
	shift 1
done

ESSID=${2:-"hacdc-babel"}
CHAN=${3:-"9"}

if [ "x$WDEV" = "x" ]
then
	echo "Usage: ./babel.sh [-e ESSID] [-c CHANNEL] INTERFACE" >&2
	exit 1
fi

main

