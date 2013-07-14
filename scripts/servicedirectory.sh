#!/bin/bash

# servicedirectory.sh - Tiny shell script that starts the dynamic service
#	directory.
# By: The Doctor [412/724/301/703][ZS]

# Make sure we are where we need to be.
cd /opt/byzantium

# Start the service directory daemon.
python service_index/services.py 2>/dev/null &

# Fin.
exit 0
