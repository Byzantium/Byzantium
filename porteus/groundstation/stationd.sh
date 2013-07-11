#!/bin/bash

# stationd.sh - Tiny shell script that starts stationd.
# By: The Doctor

# This sets an environment variable for stationd which allows it to broadcast
# on the mesh because we don't have a 0.0.0.0 route by default.
export BROADCAST_ADDRESS="192.168.255.255"

# Make sure we are where we need to be.
cd /opt/groundstation

# Source the custom Python environment.
. env/bin/activate

# Start stationd in a loop to ensure that it keeps running.
cd groundstation
./stationd 2> /dev/null

# Fin.
exit 0
