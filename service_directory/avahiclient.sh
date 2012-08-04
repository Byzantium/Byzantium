#!/bin/bash

pid_file=$1
pid=$$

echo -n $pid > $pid_file

while true ;do
    su -c '/usr/bin/env python /opt/byzantium/avahi/avahi_scraper.py' avahi
    sleep 60
done

exit 1
