#!/bin/bash
#
# rc.M		This file is executed by init(8) when the system is being
#		initialized for one of the "multi user" run levels (i.e.
#		levels 1 through 6).  It usually does mounting of file
#		systems et al.
#
# Version:	@(#)/etc/rc.d/rc.M	2.23	Wed Feb 26 19:20:58 PST 2003
#
# Author:	Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
#		Heavily modified by Patrick Volkerding <volkerdi@slackware.com>
#
# Author:   Tomas M. <http://slax.linux-live.org>
#		- change root password, autostart gui, reset terminal
#
# Modified for Porteus by fanthom

# Tell the viewers what's going to happen.
echo "Going multiuser..."

# Update various cache's if extra modules are found
if grep -qv base/00 /mnt/live/tmp/modules; then
    echo "[1m""Additional modules have been found - updating system cache...""[0m"
    # Update shared library links:
    /sbin/ldconfig &
    # Update the X font indexes:
    /usr/bin/fc-cache -f 2>/dev/null &
    # Update GTK+/gdk/pango cache's:
    /usr/bin/update-pango-querymodules 2>/dev/null &
    /usr/bin/update-gtk-immodules 2>/dev/null &
    /usr/bin/update-gdk-pixbuf-loaders 2>/dev/null &
    # Update mime database:
    /usr/bin/update-mime-database /usr/share/mime >/dev/null 2>&1 &
    # Update desktop database:
    /usr/bin/update-desktop-database 2>/dev/null &
fi

# Screen blanks after 15 minutes idle time, and powers down in one hour
# if the kernel supports APM or ACPI power management:
/bin/setterm -blank 15 -powersave powerdown -powerdown 60 > /dev/null 2>&1 &

# Save the contents of 'dmesg':
/bin/dmesg -s 65536 > /var/log/dmesg &

# Start the system logger.
[ -x /etc/rc.d/rc.syslog ] && sh /etc/rc.d/rc.syslog start >/dev/null &

# Initialize the networking hardware.
[ -x /etc/rc.d/rc.inet1 ] && . /etc/rc.d/rc.inet1

# Set the hostname.
/bin/hostname $(cat /etc/HOSTNAME | cut -f1 -d .)

# Look for additional USB/SCSI/IEEE1394/etc devices on multiple LUNs:
[ -x /etc/rc.d/rc.scanluns ] && sh /etc/rc.d/rc.scanluns &

# Start networking daemons:
[ -x /etc/rc.d/rc.inet2 ] && . /etc/rc.d/rc.inet2

# Start the Control Script for automounter:
[ -x /etc/rc.d/rc.autofs ] && sh /etc/rc.d/rc.autofs start &

# Start the Network Time Protocol daemon:
[ -x /etc/rc.d/rc.ntpd ] && sh /etc/rc.d/rc.ntpd start &

# Start ACPI daemon.
[ -x /etc/rc.d/rc.acpid ] && sh /etc/rc.d/rc.acpid start &

# Start D-Bus:
[ -x /etc/rc.d/rc.messagebus ] && . /etc/rc.d/rc.messagebus start

# Start console-kit-daemon:
[ -x /etc/rc.d/rc.consolekit ] && sh /etc/rc.d/rc.consolekit start &

# Start HAL:
[ -x /etc/rc.d/rc.hald ] && . /etc/rc.d/rc.hald start

# Start Bluetooth:
[ -x /etc/rc.d/rc.bluetooth ] && sh /etc/rc.d/rc.bluetooth start &

# Start wicd:
#[ -x /etc/rc.d/rc.wicd ] && sh /etc/rc.d/rc.wicd start &

# Start dnsmasq, a simple DHCP/DNS server:
#[ -x /etc/rc.d/rc.dnsmasq ] && sh /etc/rc.d/rc.dnsmasq start &

# Start snmpd:
#[ -x /etc/rc.d/rc.snmpd ] && sh /etc/rc.d/rc.snmpd start &

# Start the print spooling system.  This will usually be LPRng (lpd) or CUPS.
#if [ -x /etc/rc.d/rc.cups ]; then
#    # Start CUPS:
#    sh /etc/rc.d/rc.cups start >/dev/null 2>&1 &
#elif [ -x /etc/rc.d/rc.lprng ]; then
#    # Start LPRng (lpd):
#    . /etc/rc.d/rc.lprng start
#fi

# Start netatalk. (a file/print server for Macs using Appletalk)
#[ -x /etc/rc.d/rc.atalk ] && sh /etc/rc.d/rc.atalk &

# Start smartd, which monitors the status of S.M.A.R.T. compatible
# hard drives and reports any problems.  Note some devices (which aren't
# smart, I guess ;) will hang if probed by smartd, so it's commented out
# by default.
# [ -x /usr/sbin/smartd ] && /usr/sbin/smartd

# If we're using udev, make /dev/cdrom and any other optical drive symlinks
# if some udev rule hasn't made them already:
if ! egrep -qo " nohotplug( |\$)" /proc/cmdline; then
    [ -x /lib/udev/rc.optical-symlinks -a -x /etc/rc.d/rc.udev ] && sh /lib/udev/rc.optical-symlinks &
fi

# Monitor the UPS with genpowerd.
# To use this, uncomment this section and edit your settings in
# /etc/genpowerd.conf (serial device, UPS type, etc).  For more information,
# see "man genpowerd" or the extensive documentation in the
# /usr/doc/genpower-*/ directory.
# You'll also need to configure a similar block in /etc/rc.d/rc.6 if you want
# support for stopping the UPS's inverter after the machine halts.
# [ -x /sbin/genpowerd ] && { echo "Starting genpowerd daemon..."; /sbin/genpowerd; }

# Turn on process accounting.  To enable process accounting, make sure the
# option for BSD process accounting is enabled in your kernel, and then
# create the file /var/log/pacct (touch /var/log/pacct).  By default, process
# accounting is not enabled (since /var/log/pacct does not exist).  This is
# because the log file can get VERY large.
[ -x /sbin/accton -a -r /var/log/pacct ] && chmod 640 /var/log/pacct && /sbin/accton /var/log/pacct &

# Start crond (Dillon's crond):
# If you want cron to actually log activity to /var/log/cron, then change
# -l notice to -l info to increase the logging level.
/usr/sbin/crond -l notice > /dev/null 2>&1 &

# Start atd (manages jobs scheduled with 'at'):
[ -x /usr/sbin/atd ] && /usr/sbin/atd -b 15 -l 1 &

# Slackware-Mini-Quota-HOWTO:
# To really activate quotas, you'll need to add 'usrquota' and/or 'grpquota' to
# the appropriate partitions as listed in /etc/fstab.  Here's an example:
#
# /dev/hda2      /home      ext3     defaults,usrquota      1   1
#
# You'll then need to setup initial quota files at the top of the partitions
# to support quota, like this:
# touch /home/aquota.user /home/aquota.group
# chmod 600 /home/aquota.user /home/aquota.group
#
# Then, reboot to activate the system.
# To edit user quotas, use 'edquota'.  See 'man edquota'.  Also, the
# official Quota Mini-HOWTO has lots of useful information.  That can be found
# here:  /usr/doc/Linux-HOWTOs/Quota

# Check quotas and then turn quota system on:
#if grep -q quota /etc/fstab; then
#    for quotafs in $(awk '/quota/ {print $2}' /etc/fstab) ; do rm -f $quotafs/{a,}quota.{group,user}.new; done
#    [ -x /sbin/quotacheck ] && echo "Checking filesystem quotas:  /sbin/quotacheck -avugm" && /sbin/quotacheck -avugm
#    [ -x /sbin/quotaon ] && echo "Activating filesystem quotas:  /sbin/quotaon -avug" && /sbin/quotaon -avug
#fi

# Start the SASL authentication server.  This provides SASL
# authentication services for sendmail:
#[ -x /etc/rc.d/rc.saslauthd ] && sh /etc/rc.d/rc.saslauthd start &

# Start the sendmail daemon:
#[ -x /etc/rc.d/rc.sendmail ] && sh /etc/rc.d/rc.sendmail start &

# Load ALSA (sound) defaults:
[ -x /etc/rc.d/rc.alsa ] && sh /etc/rc.d/rc.alsa &

# Load a custom screen font if the user has an rc.font script.
[ -x /etc/rc.d/rc.font ] && sh /etc/rc.d/rc.font &

# Load a custom keymap if the user has an rc.keymap script.
[ -x /etc/rc.d/rc.keymap ] && sh /etc/rc.d/rc.keymap &

# Start the MySQL database:
#[ -x /etc/rc.d/rc.mysqld ] && sh /etc/rc.d/rc.mysqld start &

# Start Apache web server:
#[ -x /etc/rc.d/rc.httpd ] && sh /etc/rc.d/rc.httpd start &

# Start OpenLDAP:
#[ -x /etc/rc.d/rc.openldap ] && sh /etc/rc.d/rc.openldap start &

# Start Samba (a file/print server for Win95/NT machines).
# Samba can be started in /etc/inetd.conf instead.
#[ -x /etc/rc.d/rc.samba ] && sh /etc/rc.d/rc.samba start &

# Start the GPM mouse server:
#[ -x /etc/rc.d/rc.gpm ] && sh /etc/rc.d/rc.gpm start &

# Run SystemV init scripts for this runlevel.
[ -x /etc/rc.d/rc.sysvinit ] && . /etc/rc.d/rc.sysvinit

# Start the local setup procedure.
[ -x /etc/rc.d/rc.local ] && . /etc/rc.d/rc.local

## rc.slax script part
# change password
NEWROOTPASS=`egrep -o " passwd=[^ ]+" /proc/cmdline | cut -d= -f2`
[ "$NEWROOTPASS" = "ask" ] && { echo -ne "\nEnter new password for root: "; read -s NEWROOTPASS; }
[ "$NEWROOTPASS" = "" ] || echo "root:$NEWROOTPASS" | /usr/sbin/chpasswd

# switch to root account
if egrep -qo " toroot( |\$)" /proc/cmdline; then
    sed -i /usr/share/config/kdm/kdmrc -e s/AutoLoginUser=guest/AutoLoginUser=root/g 2>/dev/null
    sed -i /etc/lxdm/lxdm.conf -e s/autologin=guest/autologin=root/g 2>/dev/null
else
    sed -i /usr/share/config/kdm/kdmrc -e s/AutoLoginUser=root/AutoLoginUser=guest/g 2>/dev/null
    sed -i /etc/lxdm/lxdm.conf -e s/autologin=root/autologin=guest/g 2>/dev/null
fi

# if autoexec= boot parameter is present, automatically start the command given.
# If you need to execute several commands, separate them using ';'
# If you need to use spaces in your command, use ~ instead
# If you wish to reboot/poweroff after the command's execution is finished,
# simply use autoexec=yourCommand;poweroff for example.
AUTOCMD=`egrep -o " autoexec=[^ ]+" /proc/cmdline | cut -d= -f2 | tr "~" " "`
[ "$AUTOCMD" = "" ] || { echo "starting autoexec commands"; su --login -c "$AUTOCMD"; }

# All done.
