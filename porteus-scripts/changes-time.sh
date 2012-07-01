#!/bin/bash
# script by fanthom

# Switch to root
if [ `whoami` != "root" ]; then
    echo "Please enter your root password below"
    su - -c "/opt/porteus-scripts/changes-time.sh"
    exit
fi

# Variables
c='\e[36m'
r='\e[31m'
e=`tput sgr0`
changes=/mnt/live/memory/changes
tmp=/tmp/work$$
path=/root/changes$$

clear
echo -e ${c}"Enter time interval or press enter for keeping default value (3 minutes):"$e
read int
[ ! $int = "" ] && time=$int || time=3

# Creating tmp dir
mkdir $tmp

# Locating and copying newly created/modified files except of /dev, /home, /mnt, /root and /tmp
echo -e ${r}"Following folders will be excluded by default: /dev, /home, /mnt, /root, /tmp"$e
file=`find $changes -mmin -$time | egrep -v "$changes/dev|$changes/home|$changes/mnt|$changes/root|$changes/tmp"`
for x in $file; do cp -P --parents $x $tmp 2> /dev/null; done

# Removing all files which are recreated by Porteus during boot anyway and may cause troubles during activation "on the fly"
rm $tmp/$changes/etc/ld.so.cache $tmp/$changes/lib/modules/2.6.*/* $tmp/$changes/usr/share/applications/mimeinfo.cache 2>/dev/null
rm -r $tmp/$changes/usr/share/mime 2>/dev/null

# Moving all stuff to the final destination
mv $tmp/$changes $path

echo -e ${c}"all files created/modified in last $time minutes were copied to $path folder"$e
