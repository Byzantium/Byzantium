#!/bin/bash

# Project Byzantium: captive-portal.sh
# This script does the heavy lifting of IP tables manipulation under the
# captive portal's hood.  It should only be used by the captive portal daemon.

# Written by Sitwon and The Doctor.
# License: GPLv3

IPTABLES=/usr/sbin/iptables

# Set up the choice tree of options that can be passed to this script.
case "$1" in
    'initialize')
        # $2: IP address of the client interface.  Assumes final octet is .1.

        # Initialize the IP tables ruleset by creating a new chain for captive
        # portal users.
        $IPTABLES -N internet -t mangle

        # Convert the IP address of the client interface into a netblock.
        CLIENTNET=`echo $2 | sed 's/1$/0\/24/'`

        # Exempt traffic which does not originate from the client network.
        $IPTABLES -t mangle -A PREROUTING -p tcp ! -s $CLIENTNET -j RETURN
        $IPTABLES -t mangle -A PREROUTING -p udp ! -s $CLIENTNET -j RETURN

        # Traffic not exempted by the above rules gets kicked to the captive
        # portal chain.  When a use clicks through a rule is inserted above
        # this one that matches them with a RETURN.
        $IPTABLES -t mangle -A PREROUTING -j internet

        # Traffic not coming from an accepted user gets marked 99.
        $IPTABLES -t mangle -A internet -j MARK --set-mark 99

        # $2 is actually the IP address of the client interface, so let's make
        # it a bit more clear.
        CLIENTIP=$2

        # Traffic which has been marked 99 and is headed for 80/TCP or 443/TCP
        # should be redirected to the captive portal web server.
        $IPTABLES -t nat -A PREROUTING -m mark --mark 99 -p tcp --dport 80 \
	    -j DNAT --to-destination $CLIENTIP:31337
        $IPTABLES -t nat -A PREROUTING -m mark --mark 99 -p tcp --dport 443 \
	    -j DNAT --to-destination $CLIENTIP:31338

        # All other traffic which is marked 99 is just dropped
        $IPTABLES -t filter -A FORWARD -m mark --mark 99 -j DROP

        # Allow incoming traffic that is headed for the local node.
        $IPTABLES -t filter -A INPUT -p tcp --dport 53 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 80 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 443 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 9001 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 31337 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 31338 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p udp --dport 53 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p udp --dport 67 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p udp --dport 6696 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p icmp -j ACCEPT

        # But reject anything else coming from unrecognized users.
        $IPTABLES -t filter -A INPUT -m mark --mark 99 -j DROP

	exit 0
        ;;
    'add')
        # $2: IP address of client.
        CLIENT=$2

        # Isolate the MAC address of the client in question.
        CLIENTMAC=`arp -n | grep ':' | grep $CLIENT | awk '{print $3}'`

        # Add the MAC address of the client to the whitelist, so it'll be able
        # to access the mesh even if its IP address changes.
        $IPTABLES -t mangle -A internet -m mac --mac-source \
            $CLIENTMAC -j RETURN

	exit 0
        ;;
    'remove')
        # $2: IP address of client.
        CLIENT=$2

        # Isolate the MAC address of the client in question.
        CLIENTMAC=`arp -n | grep ':' | grep $CLIENT | awk '{print $3}'`

        # Delete the MAC address of the client from the whitelist.
        $IPTABLES -t mangle -D internet -m mac --mac-source \
            $CLIENTMAC -j RETURN

	exit 0
        ;;
    'purge')
        # Purge all of the IP tables rules.
        $IPTABLES -F
        $IPTABLES -X
        $IPTABLES -t nat -F
        $IPTABLES -t nat -X
        $IPTABLES -t mangle -F
        $IPTABLES -t mangle -X
        $IPTABLES -t filter -F
        $IPTABLES -t filter -X

	exit 0
        ;;
    'list')
	# Display the currently running IP tables ruleset.
	$IPTABLES --list -n
	$IPTABLES --list -t nat -n
	$IPTABLES --list -t mangle -n
	$IPTABLES --list -t filter -n

	exit 0
	;;
    *)
        echo "USAGE: $0 {initialize <IP> <interface>|add <IP> <interface>|remove <IP> <interface>|purge|list}"
        exit 0
    esac
