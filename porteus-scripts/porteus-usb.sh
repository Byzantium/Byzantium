#!/bin/bash
# Jayflod's script to format USB with 2 partitions and install porteus on it
# Modifications by fanthom

# Switch to root
if [ `whoami` != "root" ]; then
    echo "Please enter your root password below"
    su - -c "/opt/porteus-scripts/porteus-usb.sh" || sleep 1
    exit
fi

# Variables
c='\e[36m'
r='\e[31m'
e=`tput sgr0`

# Set exit on error
if ! command; then echo "command failed"; exit 1; fi

# Welcome message
clear
echo -e "${c}Welcome to Porteus2usb installer by jayflood (modified by fanthom).
This tool will let you:
- set two partitions on a usb stick: the first one will be formatted with the FAT32 filesystem and the second one with any linux filesystem (useful for saving changes on it)
- copy Porteus to the partition formatted with a linux filesystem
- make the pendrive bootable with porteus."$e | fmt -w 80
echo -e "${r}Be aware that the size of the pendrive must be bigger than the existing Porteus installation."$e | fmt -w 80
echo -e "${c}Dbus will be disabled while running this script.

Press enter to continue or ctrl+c to exit"$e
read
echo

# Get path to porteus data
CDVD=`grep "Porteus data found in" -A 1 /mnt/live/var/log/livedbg | tail -n1 | rev | cut -d/ -f2- | rev`
CDPORTEUS=/mnt/live$CDVD

# Check presence of porteus data
if [ -d $CDPORTEUS/boot -a -d $CDPORTEUS/porteus ]; then
echo -e "${c}Porteus data found in $CDPORTEUS folder"$e
else
echo -e "${c}Couldn't find porteus data (/boot and /porteus folders) in $CDPORTEUS directory. press enter to exit"$e | fmt -w 80
read
exit 1
fi

# Disable Dbus daemon
/etc/rc.d/rc.messagebus stop > /dev/null 2>&1

echo -e "${c}Please plug in your USB stick and press enter when ready."$e
read
sleep 3

# Check if device is removable - if not exit
CHKREM=($(dmesg | tail -n3 | grep removable | awk '{print $5}' | sed -e s/.$// | sed -e s/^.//))
echo
if [ "$CHKREM" = "" ]; then
echo -e "${r}Error: Did not find any REMOVABLE device!!  Press enter to exit."$e; read; exit
fi

# Get path to removable device
CHKPRT=($(cat /proc/partitions | tail -n1 | awk '{print$4}'))

# Mount removable device
# Check if device is mounted and mount
if mount | grep -q /mnt/$CHKPRT; then
echo -e "${c}Pendrive mounted"$e
else
echo -e "${c}Mounting pendrive"$e
mkdir /mnt/$CHKPRT > /dev/null 2>&1
mount /dev/$CHKPRT /mnt/$CHKPRT
fi

# Set device paths in /dev and /mnt style
DEVPATH=/dev/$CHKPRT
MNTPATH=/mnt/$CHKPRT
BASEPATH=$(echo $DEVPATH | rev); BASEPATH=$(echo ${BASEPATH:1} | rev); # E.g takes the 1 from sda1 giving /dev/sda
BASEPATH2=$(echo $MNTPATH | rev); BASEPATH2=$(echo ${BASEPATH2:1} | rev); # Gives same but /mnt/sda

# Get USB size in Mb
USBMB=($(df -h $MNTPATH | grep dev | awk '{print$2}'))

# Make sure the removable device is big enough for porteus
USBSIZE=($(fdisk -l /dev/"$CHKREM"1 | grep MB | awk '{print$3}'))
PORTEUSSIZE=($(du -m $CDPORTEUS/porteus | tail -n1 | awk '{print$1}'))
if [ $USBSIZE -lt $PORTEUSSIZE ]; then
echo -e "${c}Sorry, not enough room on your USB stick for Porteus.  Press enter to exit."$e; read; exit
fi

# Give message that process will now begin
echo
echo -e "${c}We will now format the USB device with 2 partitions. The first one will be a FAT32 /boot partition.
The second one will be a linux filesystem (better than FAT for storing changes) for /porteus.  This partition will use up the remainder of the device.
Press enter to continue."$e | fmt -w 80
read
echo

# Define the size for boot partition
echo -e "${c}Please provide the size for the FAT32 boot partition, in megabytes."$e
echo -e "${r}This cannot be less than 32M, otherwise the system will be unbootable!!"$e
echo -e "${c}example: 64M"$e
read size
echo

# Define the linux filesystem for second partition
echo -e "${c}Please choose the linux filesystem type for the second partition.
Currently, the following are supported: ext2, ext3, ext4, xfs, and reiserfs.

Example: xfs"$e | fmt -w 80
read fs
echo

if [ "$fs" = "xfs" ]; then
fs="xfs -f"
fi

if [ "$fs" = "reiserfs" ]; then
fs="$fs -q"
fi

# Last chance to exit :)
echo -e "${r}WARNING: you will lose ALL information on the device. Do you want to continue? Answer y/n"$e | fmt -w 80
read ans
echo
if [ "$ans" != "y" ]; then exit; fi

# unmount removabe device
umount $MNTPATH

echo -e "${c}Organizing partitions now."$e

# Begin formatting
fdisk $BASEPATH << EOF1
d
1
n
p
1

+$size
n
p
2


t
1
b
t
2
83
w
q
EOF1

sleep 1
echo -e "${r}Formatting partitions now. DO NOT REMOVE USB DEVICE!"$e
sleep 2

# Format partitions
mkfs.$fs $BASEPATH'2'
mkdosfs -F 32 $BASEPATH'1'

echo -e "${c}Almost done ...."$e
sleep 1

# Ask user to remove USB and replug
echo -e "${c}Please remove your USB device ... and then plug it back in. 
Press enter when done."$e
read
sleep 4

# Mount partitions
mount /dev/$CHKREM'1' $BASEPATH2'1'
mkdir /mnt/$CHKREM'2' > /dev/null 2>&1
mount /dev/$CHKREM'2' $BASEPATH2'2'

echo -e "${c}Copying Porteus data now ... please wait."$e

#Copy boot directory
cp -r $CDPORTEUS/boot $BASEPATH2'1'/

echo -e "${r}Almost done ... DO NOT remove USB device."$e
sleep 2

# Copy porteus directories
cp -r $CDPORTEUS/porteus $BASEPATH2'2'/
sleep 1

# Run scipt to make USB bootable
cd $BASEPATH2'1'/boot/
./lin_start_here.sh

# Start disabled messagebus
/etc/rc.d/rc.messagebus start > /dev/null 2>&1

# Give option to reboot now or later
echo -e "${c}Your USB device should now be able to boot into Porteus.  Please ensure your BIOS is set to boot from USB.
You will need to restart Porteus now without the CD in. Would you like to reboot Porteus now? Answer y/n"$e | fmt -w 80
read reboot

if [ "$reboot" != "y" ]; then exit
else
shutdown -r now
fi
exit