#!/bin/bash

# Project Byzantium: captive-portal.sh
# This script does the heavy lifting of IP tables manipulation under the
# captive portal's hood.  It should only be used by the captive portal daemon.

# Written by Sitwon and The Doctor.
# Copyright (C) 2013 Project Byzantium
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.

IPTABLES=/usr/sbin/iptables
ARP=/sbin/arp

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
        $IPTABLES -t mangle -A PREROUTING -p all ! -s $CLIENTNET -j RETURN

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
        $IPTABLES -t nat -A PREROUTING -m mark --mark 99 -p udp --dport 53 \
            -j DNAT --to-destination $CLIENTIP:31339

        # HTTP replies come from the same port the requests were received by.
        # Rewrite the outbound packets to appear to come from the appropriate
        # ports.
        $IPTABLES -t nat -A POSTROUTING -d $CLIENTNET -p tcp --sport 31337 \
            -j SNAT --to-source :80
        $IPTABLES -t nat -A POSTROUTING -d $CLIENTNET -p tcp --sport 31338 \
            -j SNAT --to-source :443

        # Replies from fake_dns.py come from the same port because they're
        # UDP.  Rewrite the packet headers so it loos like it's from port
        # 53/udp.
        $IPTABLES -t nat -A POSTROUTING -d $CLIENTNET -p udp --sport 31339 \
            -j SNAT --to-source :53

        # All other traffic which is marked 99 is just dropped
        $IPTABLES -t filter -A FORWARD -m mark --mark 99 -j DROP

        # Allow incoming traffic that is headed for the local node.
        $IPTABLES -t filter -A INPUT -p tcp --dport 53 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 80 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 443 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 1248 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 6667 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 8089 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 9001 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 9005 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 9006 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 9090 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 31337 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p tcp --dport 31338 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p udp --dport 53 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p udp --dport 67 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p udp --dport 698 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p udp --dport 5353 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p udp --dport 31339 -j ACCEPT
        $IPTABLES -t filter -A INPUT -p icmp -j ACCEPT

        # But reject anything else coming from unrecognized users.
        $IPTABLES -t filter -A INPUT -m mark --mark 99 -j DROP

        exit 0
        ;;
    'add')
        # $2: IP address of client.
        CLIENT=$2

        # Isolate the MAC address of the client in question.
        CLIENTMAC=`$ARP -n | grep ':' | grep $CLIENT | awk '{print $3}'`

        # Add the MAC address of the client to the whitelist, so it'll be able
        # to access the mesh even if its IP address changes.
        $IPTABLES -t mangle -I internet -m mac --mac-source \
            $CLIENTMAC -j RETURN

        exit 0
        ;;
    'remove')
        # $2: IP address of client.
        CLIENT=$2

        # Isolate the MAC address of the client in question.
        CLIENTMAC=`$ARP -n | grep ':' | grep $CLIENT | awk '{print $3}'`

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
