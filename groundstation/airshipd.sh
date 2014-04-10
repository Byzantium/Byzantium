#!/bin/bash

# stationd.sh - Tiny shell script that starts stationd.
# By: The Doctor

flags=""

# Make sure we are where we need to be.
cd /opt/groundstation

# Source the custom Python environment.
. env/bin/activate

[ -n "$APID" ] &&
    flags="$flags --pidfile $APID"

# Start stationd in a loop to ensure that it keeps running.
cd groundstation
./airshipd $flags 2> /dev/null

# Fin.
exit 0
