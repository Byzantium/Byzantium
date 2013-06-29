#!/bin/bash
# Copyright (C) 2013 Project Byzantium
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.


pid_file=$1
pid=$$

echo -n $pid > $pid_file

while true ;do
    su -c '/usr/bin/env python /opt/byzantium/avahi/avahiclient.py' avahi
    sleep 60
done

exit 1
