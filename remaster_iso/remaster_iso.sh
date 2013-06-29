#!/bin/bash
# remaster_iso.sh - Shell script that takes a Porteus Linux .iso image and
#	turns it into a Byzantium Linux .iso image by unpacking, modifying, and
#	rebuilding it.
# Copyright (C) 2013 Project Byzantium
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.

# NOTE: This script is intended for developers only.  If you don't plan on
#	hacking your .iso image you don't need this utility!

# Project Byzantium: http://project-byzantium.org/

# By: The Doctor <drwho at virtadpt dot net>

# Command line arguments:
#    $1: Location of Porteus Linux .iso image.
#    $2: Location Byzantium Linux .iso image will be created in.
#    $3: Path to 000-byzantium.xzm module.

# Count the command line args.  If there aren't any, terminate.
if [ $# -lt 3 ]; then
    echo "ERROR: Insufficient command line arguments given to $0."
    echo "USAGE: $0 /path/to/porteus.iso /path/to/create/byzantium.iso /path/to/000-byzantium.xzm"
    exit 1
    fi

# Make sure this script is being run as root.  If it's not, ABEND.
if [ `id -u` -gt 0 ]; then
    echo "ERROR: You must be root to run this script."
    exit 1
    fi

# This script exists in the directory Byzantium/remaster_iso/.  Thus, we know
# where we are already, and we know where our towel is as well.  Or in this
# case, our custom files.

# Store the current working directory in a variable so we can return to it
# later.
HOMEDIR=`pwd`

# Mount the Porteus disk image to an unused loop device.
echo "Mounting $1 on a loopback device..."
LOOP=`losetup -f`
losetup $LOOP $1
PORTEUS_MOUNTPOINT=`mktemp -d`
mount $LOOP $PORTEUS_MOUNTPOINT

# Unpack the Porteus .iso image.
echo "Unpacking the Porteus image..."
PORTEUS_DIR=`mktemp -d`
cp -rv $PORTEUS_MOUNTPOINT/* $PORTEUS_DIR

# Copy the Byzantium Linux files into the unpacked .iso image.
echo "Installing Byzantium files..."
cp byzantium.jpg $PORTEUS_DIR/boot/
cp porteus.cfg $PORTEUS_DIR/boot/
cp $3 $PORTEUS_DIR/porteus/modules

# Add the MD5 hash of 000-byzantium.xzm to md5sums.txt.  The way the line is
# generated is a little awkward because the path the module has to be specified
# in a particular way.
HASH=`md5sum $3 | awk '{print $1}'`
MD5SUM_LINE=`echo $HASH ./modules/000-byzantium.xzm`
echo $MD5SUM_LINE >> $PORTEUS_DIR/porteus/md5sums.txt

# Make the Byzantium Linux .iso image.
echo "Building Byzantium Linux .iso image..."
cd $PORTEUS_DIR/porteus/
./make_iso.sh $2

# Test to see if the .iso image was created successfully.
if [ -f $2 ]; then
    echo "Byzantium Linux .iso image successfully created at $2."
else
    echo "FAILURE: Byzantium Linux .iso image NOT created!"
fi

# Clean up after ourselves.
echo "Cleaning up..."
cd $HOMEDIR
umount $PORTEUS_MOUNTPOINT
losetup -d $LOOP
rm -rf $PORTEUS_MOUNTPOINT $PORTEUS_DIR
echo "Done."

exit 0

