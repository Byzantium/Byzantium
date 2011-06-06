#!/bin/bash

# Network traffic stats generator.
# By: The Doctor (drwho at virtadpt dot net)
# PGP: 0x807B17C1 / 7960 1CDC 85C9 0B63 8D9F  DD89 3BD8 FF2B 807B 17C1

# Starts when a Byzantium node is booted, runs in background every sixty seconds.
# Polls network stats from all active network interfaces and puts them into
# rrdtool databases.  Also regenerates graphs every minute.

# Calls to rrdtool shamelessly taken from rrd_traffic.pl by Martin Pot.
# http://martybugs.net/linux/rrdtool/traffic.cgi
# I can't give him enough credit for this, I've been trying to learn RRDtool
# for six years and I still can't figure it out.

# Requires: rrdtool

# Version 1.0

# TODO:
#	- Add code to delete rrdtool databases and graphs for network interfaces
#	  that go away (say, someone unplugs a USB wifi device from the
#	  computer).

# Variables.
INTERFACES=""
DATABASES="/tmp/stats_databases"
GRAPHS="/srv/controlpanel/graphs"

# Core code.
# Create the directory to store the rrdtool databases in.
if [ ! -d "$DATABASES" ]; then
	mkdir -p $DATABASES
	fi

# Ensure that the destination directory for the traffic graphs exists.
if [ ! -d "$GRAPHS" ]; then
	mkdir -p $GRAPHS
	fi

# Set up the loop that updates everything once a minute.
while true; do
	# Pick out all of the network interfaces on the box.
	for i in `ifconfig | grep '^[a-z]' | grep -v '^lo' | awk '{print $1}'`;do
		INTERFACES="$INTERFACES $i"
		done

	# Create an RRDtool database for every interface if one does not exist
	# yet.
	for i in $INTERFACES; do
		if [ ! -e $DATABASES/$i.rrd ]; then
			rrdtool create "$DATABASES/$i.rrd" -s 300 \
	                DS:in:DERIVE:600:0:12500000 \
			DS:out:DERIVE:600:0:12500000 \
        	        RRA:AVERAGE:0.5:1:576 \
			RRA:AVERAGE:0.5:6:672 \
                	RRA:AVERAGE:0.5:24:732 \
			RRA:AVERAGE:0.5:144:1460
			fi
		done

	# Extract the bytes sent and received by each network interface.
	for i in $INTERFACES; do
		TRAFFIC=`ifconfig "$i" | grep bytes | awk '{print $2, $6}' | sed 's/bytes://g'`
		IN=`echo $TRAFFIC | awk '{print $1}'`
		OUT=`echo $TRAFFIC | awk '{print $2}'`

		# Update the appropriate RRDtool database with the IN and OUT
		# stats.
		rrdtool update "$DATABASES/$i.rrd" -t "in:out" "N:$IN:$OUT"
		done

	# Update the graphs for the status page.
	for i in $INTERFACES; do
		# Generate time_t values for the start and end of the graphs.
		START=`date -d "1 hour ago" +%s`
		END=`date +%s`

		# Build the graph
		rrdtool graph "$GRAPHS/$i.png" \
		-s $START -e $END \
		-t "Traffic on $i." \
		--lazy -h 80 -w 600 -l 0 -a PNG -v "bytes/minute" \
		DEF:in=$DATABASES/$i.rrd:in:AVERAGE \
		DEF:out=$DATABASES/$i.rrd:out:AVERAGE \
		CDEF:"out_neg=out,-1,*" \
		AREA:in#32CD32:Incoming \
		LINE1:in#336600 \
		GPRINT:in:MAX:"  Max\\: %5.1lf %s" \
		GPRINT:in:AVERAGE:" Avg\\: %5.1lf %S" \
		GPRINT:in:LAST:" Current\\: %5.1lf %Sbytes/sec\\n" \
		AREA:out_neg#4169E1:Outgoing \
		LINE1:out_neg#0033CC \
		GPRINT:out:MAX:"  Max\\: %5.1lf %S" \
		GPRINT:out:AVERAGE:" Avg\\: %5.1lf %S" \
		GPRINT:out:LAST:" Current\\: %5.1lf %Sbytes/sec" \
		HRULE:0#000000 1>/dev/null
		done

	# Bottom of the loop.

	echo "Sleeping for 60 seconds..."

	sleep 60
	done

# End.
