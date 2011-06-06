#!/bin/bash

# Network traffic stats generator.
# By: The Doctor (drwho at virtadpt dot net)
# PGP: 0x807B17C1 / 7960 1CDC 85C9 0B63 8D9F  DD89 3BD8 FF2B 807B 17C1

# Starts when a Byzantium node is booted, runs in background every sixty seconds.
# Polls network stats from all active network interfaces and puts them into
# rrdtool databases.  Also regenerates graphs every minute.

# Calls to rrdtool shamelessly taken from rrd_traffic.pl by Martin Pot.
# http://martybugs.net/linux/rrdtool/traffic.cgi

# Requires: rrdtool

# Version 1.0

# TODO:
#	- Make this run in a loop on a 60 second delay.
#	- Add code to delete rrdtool databases and graphs for network interfaces
#	  that go away (say, someone unplugs a USB wifi device from the
#	  computer).

# Variables.
INTERFACES = ""
DATABASES = "/tmp/stats_databases"

# Core code.
# Create the directory to store the rrdtool databases in.
if [ ! -d "$DATABASES" ]; then
	mkdir -p $DATABASES
	fi

# Pick out all of the network interfaces on the box.
INTERFACES=`ifconfig | grep '^[a-z]' | grep -v '^lo' | awk '{print $1}'`

# Create an RRDtool database for every interface if one does not exist yet.
for i in $INTERFACES; do
	if [ ! -f "$i" ]; then
		rrdtool create $DATABASES/$i.rrd -s 300 \
                DS:in:DERIVE:600:0:12500000 \
                DS:out:DERIVE:600:0:12500000 \
                RRA:AVERAGE:0.5:1:576 \
                RRA:AVERAGE:0.5:6:672 \
                RRA:AVERAGE:0.5:24:732 \
                RRA:AVERAGE:0.5:144:1460 \
		fi
	done

# Extract the bytes sent and received by each network interface.
for i in $INTERFACES; do
	TRAFFIC = `ifconfig $i| grep bytes | awk '{print $2, $6}' | sed 's/bytes://g'`
	IN = `echo $TRAFFIC | awk '{print $3}'`
	OUT = `echo $TRAFFIC | awk '{print $4}'`

	# Update the appropriate RRDtool database with the IN and OUT stats.
	rrdtool update $DATABASES/$i.rrd -t in:out N:"$IN":"$OUT"
	done

# End.
