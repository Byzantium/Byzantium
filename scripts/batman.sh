#!/bin/sh
# batman.sh - Shell script that simplifies some common operations of the
#	BATMAN-advanced mesh networking utility by wrapping them in an
#	initscript-like interface.

# Requires:
#	- BATMAN-adv: http://www.open-mesh.org/wiki/batman-adv
#	- root access

# AUTHORS:
#	The Doctor [412/724/301/703] <drwho at virtadpt dot net>
#	0x807B17C1 / 7960 1CDC 85C9 0B63 8D9F  DD89 3BD8 FF2B 807B 17C1

# v1.0	- Initial release.

# TODO:
#	- Figure out how to make this work for other distros.
#	- Modularize the start/stop code to clean it up?

# Variables

# Core code.
# Here's where the heavy lifting happens - this parses the arguments passed to
# script and triggers what has to be triggered.
case "$1" in
	# See if the batman-adv.ko module has been inserted.  If it has, error
	# out.  If not, insmod it.
	'insmod')
		INSMOD=`lsmod | grep batman`
		if [ -n "$INSMOD" ]; then
			echo "NOTE: Kernel module batman-adv.ko is already installed."
			exit 1
		else
			modprobe batman-adv
		fi
		;;
	# Remove the kernel module.  This should effectively down the /bat[0-9]*/
	# interfaces.
	'rmmod')
		INSMOD=`lsmod | grep batman`
		if [ -z "$INSMOD" ]; then
			echo "NOTE: Kernel module batman-adv.ko was already removed."
			exit 1
		else
			rmmod batman_adv
		fi
		;;
	# Show the status of all of the BATMAN-adv interfaces.
	'status')
		BATS=`ifconfig -a | grep '^bat' | awk '{print $1}'`
		for i in BATS; do
			ifconfig $i
			done
		;;
	'start')
		# Arguments to this function:
		#	$2: network interface
		#	$3: ESSID
		#	$4: channel

		# See if any network interfaces have been passed as extra args.
		# If not, ABEND.
		if [ -z "$2" ]; then
			echo "ERROR: Insufficient arguments to start mesh interface."
			echo "USAGE: $0 $1 <network interface> <essid> <channel>"
			exit 1
			fi

		# See if NetworkManager is running.  If it is, kill it.
		echo "Terminating NetworkManager."

		# Redhat and its derivatives.
		if [ -f /etc/redhat* ]; then
			/etc/init.d/NetworkManager stop
			fi

		# Debian and its derivatives.
		if [ -f /etc/debian* ]; then
			stop network-manager
			fi

		# Arch Linux.
		if [ -f /etc/arch-release ]; then
			/etc/rc.d/networkmanager stop
			fi

		# Disable IPtables.
		iptables -F

		# Configure ad-hoc mode for wireless interface.  Blow away the
		# old IP configuration while we're at it.
		ifconfig $2 down
		iwconfig $2 mode ad-hoc essid $3 channel $4
		ifconfig $2 0.0.0.0 up

		# Configure BATMAN-advanced interface.
		batctl if add $2
		ifconfig $2 mtu 1527
		STATUS=`cat /sys/class/net/$2/batman_adv/iface_status`
		if [ "$STATUS" != "active" ]; then
			echo "ERROR: Not a BATMAN interface.  Are you sure that the module's installed?"
			echo "You'll have to do it manually, I'm afraid."
			exit 1
			fi
		BAT=`cat /sys/class/net/$2/batman_adv/mesh_iface`
		ifconfig $BAT up
		;;
	'stop')
		# Arguments to this function:
		#	$2: network interface
		#	$3: ESSID
		#	$4: channel

		# See if any network interfaces have been passed as extra args.
		# If not, ABEND.
		if [ -z "$2" ]; then
			echo "ERROR: Insufficient arguments to start mesh interface."
			echo "USAGE: $0 $1 <network interface> <essid> <channel>"
			exit 1
			fi

		# Determine if a /bat?/ interface or another interface have been
		# passed as an argument.  That determines how we go about things.
		# grep returns 0 if found, 1 if not.
		grep 'bat' "$2"
		if [ "$?" == 0 ]; then
			INT=$2
		else
			INT=`cat /sys/class/net/$2/batman_adv/mesh_iface`
		fi

		Take down the network interface (and associated BATMAN interface).
		ifconfig $INT down
		...
		;;
	*)
		echo "USAGE: $0 {insmod|rmmod|status|start|...}"
		exit 0
	esac

# End of script.
