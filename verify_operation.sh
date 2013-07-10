#!/bin/bash
# verify_operation.sh - This script runs as the guest user when the desktop
#     starts up.  It does two things: First, it tests to see if network
#     functionality has been configured.  Depending on what it finds it starts
#     Firefox with one of two HTML files, one for success and one for failure.

# Copyright (C) 2013 Project Byzantium
# By: The Doctor [412/724/301/703][ZS]

# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or any later version.

# If the .desktop file for re-running the configuration daemon exists on the
# desktop, delete it.
rm -f ~/Desktop/retry.desktop

# Set the global pass/fail flag.
SUCCESS="true"

# Build a list of all of the wireless interfaces on the node.  If there aren't
# any, then none of the kernel drivers worked.
WIRELESS=""
for i in /sys/class/net/*; do
    if [ -d $i/wireless ]; then
        j=`echo $i | sed 's/\/sys\/class\/net\///'`
        WIRELESS="$WIRELESS $j"
        fi
    done
if [ ! $WIRELESS ]; then
    SUCCESS=""
    fi

# Test to see if olsrd started.
OLSRD_PID=`pgrep olsrd`
if [ $? -gt 0 ]; then
    SUCCESS=""
    fi

# Test to see if the captive portal started.
CP_PID=`pgrep -f captive_portal`
if [ $? -gt 0 ]; then
    SUCCESS=""
    fi

# Test to see if the dead client reaper started.
MUDC_PID=`pgrep -f mop_up_dead_clients`
if [ $? -gt 0 ]; then
    SUCCESS=""
    fi

# Test to see if the fake DNS server started.
FAKEDNS_PID=`pgrep -f fake_dns`
if [ $? -gt 0 ]; then
    SUCCESS=""
    fi

# Depending on whether or not everything started up properly, run Firefox with
# one of two HTML files as arguments.
if [ "$SUCCESS" == "true" ]; then
    firefox ~/.passfail/success.html &
else
    firefox ~/.passfail/failure.html &

    # Copy the .desktop file for re-running the configuration daemon into
    # position.
    cp ~/retry.desktop ~/Desktop/retry.desktop
fi

# End.
