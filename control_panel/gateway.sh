#!/bin/bash

# gateway.sh: Does the heavy lifting of configuring a gateway interface.
#	      Hopefully this'll be temporary until we get a better network
#	      configuration system in place.

# By: The Doctor <drwho at virtadpt dot net>
# For: Project Byzantium (http://project-byzantium.org/)

# Takes one argument, the name of a network interface.  Returns 0 if
# successful, non-zero if not.

# Set up global variables.
DHCPCD=/usr/sbin/dhcpcd
IPTABLES=/usr/sbin/iptables
AVAHI=/usr/sbin/avahi-daemon

# Make sure that at least one command line argument has been passed to this
# script.  ABEND if not.
if [ $# -lt 1 ]; then
    echo "ERROR: Insufficient command line arguments given to $0."
    echo "USAGE: $0 [interface name]"
    exit 1
    fi

# Move the name of the network interface into a more sane variable.
INTERFACE=$1

# Run dhcpcd on the gateway interface, but don't let it fork() into the
# background until it gets configuration information.  By default, dhcpcd tries
# for 30 seconds.
$DHCPCD -w $INTERFACE

# Test to see if dhcpcd was successful.  dhcpcd returns 1 when it times out or
# zero if it was successful.
if [ $? -gt 0 ]; then
    echo "ERROR: dhcpcd timed out."
    exit 1
    fi

# Run the iptables utility to set up NAT on the interface.
$IPTABLES -t nat -A POSTROUTING -o $INTERFACE -j MASQUERADE

# Tell avahi-daemon to re-read /etc/resolv.conf and announce the IP addresses
# of the DNSes in there across the mesh.
$AVAHI -r

# Fin.
exit 0

