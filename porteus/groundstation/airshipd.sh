#!/bin/bash

# stationd.sh - Tiny shell script that starts stationd.
# By: The Doctor

# Make sure we are where we need to be.
cd /opt/groundstation

# Source the custom Python environment.
. env/bin/activate

# Start stationd in a loop to ensure that it keeps running.
cd groundstation
./airshipd 2> /dev/null

# Fin.
exit 0
