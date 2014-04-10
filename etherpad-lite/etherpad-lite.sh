#!/bin/bash

# etherpad-lite.sh - Tiny shell script that starts Etherpad-Lite.
# By: The Doctor

# Make sure we are where we need to be.
cd /opt/etherpad-lite

# Start Etherpad-Lite.
cd etherpad-lite
bin/run.sh &

# Fin.
exit 0
