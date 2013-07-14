#!/bin/bash
# reconfigure.sh - This script is run from sudo if the user is offered the
#    option to try configuring the node again.  It makes sense to use a shell
#    script for this because it'll make it easy to add other stuff later if
#    necessary.

# Copyright (C) 2013 Project Byzantium
# By: The Doctor [412/724/301/703][ZS]

# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or any later version.

# Delete the captive portal pid file just in case one exists.
rm -f /var/run/captive_portal.*

# Run the autoconfiguration daemon again.
/usr/local/sbin/byzantium_configd.py

# Fin.
exit 0
