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
#		- change root password, 'autoexec=' cheatcode
#
# Modified for Porteus by fanthom

# Set linefeed mode to avoid staircase-effect:
stty onlcr

# Tell the viewers what's going to happen:
echo "[1m""Going multiuser...""[0m"

# Start the system logger:
#[ -x /etc/rc.d/rc.syslog ] && sh /etc/rc.d/rc.syslog start >/dev/null &

# Bind magic folders:
if [ -e /etc/magic_folders/folders.cfg ]; then
    if ! egrep -qo " nomagic( |\$)" /proc/cmdline; then
	echo "[1m""Binding magic folders...""[0m"
	num=`grep -c / /etc/magic_folders/folders.cfg`; x=1
	while [ $x -le $num ]; do
	    # Source:
	    src=`sed -n "$x"p /etc/magic_folders/folders.cfg | cut -d " " -f1`
	    # UUID to sdXY translation:
	    uuid=`echo $src | cut -d/ -f3`; dev=`blkid -U $uuid | cut -d/ -f3`; src=`echo $src | sed s/$uuid/$dev/g`
	    # Destination:
	    dest=`sed -n "$x"p /etc/magic_folders/folders.cfg | cut -d " " -f2`
	    if [ -f $src ]; then
		# fsck for .dat containers:
		draw() { echo "[1;33m""---------------------------------------------------------""[0m"; }
		fsck_dat() { echo "performing filesystem check on $1"
		fs=`blkid $1 | egrep -o ' TYPE=[^ ]+' | cut -b8-10`
		if [ $fs = xfs ]; then
		    echo "detected xfs - performing fsck at mount time"
		elif [ $fs = ext ]; then
		    draw; e2fsck -C 0 -p $1; wait; draw
		elif [ $fs = rei ]; then
		    draw; reiserfsck -p $1; wait; draw
		else
		    echo "detected unsupported fs - skipping fsck"
		fi }
		# Add free loop device for .dat file:
		x=`losetup -a | tail -n1 | cut -d: -f1 | sed s^/dev/loop^^`; let y=x+1
		[ -b /dev/loop$y ] || mknod /dev/loop$y b 7 $y
		echo "mount -o loop $src $dest"
		if file $src | grep -qo LUKS; then
		    losetup /dev/loop$y $src
		    cryptsetup luksOpen /dev/loop$y magic$y
		    fsck_dat /dev/mapper/magic$y
		    mount /dev/mapper/magic$y $dest
		else
		    fsck_dat $src
		    mount -o loop $src $dest
		fi
	    else
		echo "mount -o bind $src $dest"
		mount -o bind $src $dest
	    fi
	let x=x+1
	done
    # Restart syslog when magic folders are used on /var folder:
    grep -qo ' /var' /etc/magic_folders/folders.cfg && [ -x /etc/rc.d/rc.syslog ] && sh /etc/rc.d/rc.syslog restart >/dev/null &
    fi
fi

# Initialize the networking hardware:
#[ -x /etc/rc.d/rc.inet1 ] && . /etc/rc.d/rc.inet1 &

# Set the hostname:
hostname $(cat /etc/HOSTNAME | cut -f1 -d .) &

# Start ACPI daemon:
[ -x /etc/rc.d/rc.acpid ] && sh /etc/rc.d/rc.acpid start &

# Start console-kit-daemon:
[ -x /etc/rc.d/rc.consolekit ] && sh /etc/rc.d/rc.consolekit start &

# Start D-Bus:
[ -x /etc/rc.d/rc.messagebus ] && . /etc/rc.d/rc.messagebus start

# Start NetworkManager/Wicd:
#if [ -x /etc/rc.d/rc.networkmanager ]; then
#    sh /etc/rc.d/rc.networkmanager start &
#elif [ -x /etc/rc.d/rc.wicd ]; then
#    sh /etc/rc.d/rc.wicd start &
#fi

# Change password:
NEWROOTPASS=`egrep -o " passwd=[^ ]+" /proc/cmdline | cut -d= -f2`
[ "$NEWROOTPASS" = "ask" ] && { echo -ne "\nEnter new password for root: "; read -s NEWROOTPASS; }
[ "$NEWROOTPASS" = "" ] || echo "root:$NEWROOTPASS" | chpasswd

# Disable GUI autologin:
if egrep -qo " noautologin( |\$)" /proc/cmdline; then
    sed --follow-symlinks -i s/AutoLoginEnable=true/AutoLoginEnable=false/g /usr/share/config/kdm/kdmrc 2>/dev/null
    sed -i 's/^autologin=/#autologin=/g' /etc/lxdm/lxdm.conf 2>/dev/null
else
    sed --follow-symlinks -i s/AutoLoginEnable=false/AutoLoginEnable=true/g /usr/share/config/kdm/kdmrc 2>/dev/null
    sed -i 's/^#autologin=/autologin=/g' /etc/lxdm/lxdm.conf 2>/dev/null
    # Switch to root account:
    if egrep -qo " toroot( |\$)" /proc/cmdline; then
        sed --follow-symlinks -i s/AutoLoginUser=guest/AutoLoginUser=root/g /usr/share/config/kdm/kdmrc 2>/dev/null
        sed -i s/autologin=guest/autologin=root/g /etc/lxdm/lxdm.conf 2>/dev/null
    else
        sed --follow-symlinks -i s/AutoLoginUser=root/AutoLoginUser=guest/g /usr/share/config/kdm/kdmrc 2>/dev/null
        sed -i s/autologin=root/autologin=guest/g /etc/lxdm/lxdm.conf 2>/dev/null
    fi
fi

# Let's check if user used 'lxde/xfce' cheatcode to determine display manager:
dm() { echo $1 > /mnt/live/tmp/dm; }
if egrep -qo " lxde( |\$)" /proc/cmdline; then
    if [ -x /usr/sbin/lxdm ]; then
	sed -i s^session=/usr/bin/startxfce4^session=/usr/bin/startlxde^g /etc/lxdm/lxdm.conf 2>/dev/null
        ln -sf /etc/X11/xinit/xinitrc.lxde /etc/X11/xinit/xinitrc
        dm /usr/sbin/lxdm
    else
        dm xdm
    fi
elif egrep -qo " xfce( |\$)" /proc/cmdline; then
    if [ -x /usr/sbin/lxdm ]; then
	sed -i s^session=/usr/bin/startlxde^session=/usr/bin/startxfce4^g /etc/lxdm/lxdm.conf 2>/dev/null
        ln -sf /etc/X11/xinit/xinitrc.xfce /etc/X11/xinit/xinitrc
        dm /usr/sbin/lxdm
    else
        dm xdm
    fi
else
    if test -x /usr/bin/kdm; then
        ln -sf /etc/X11/xinit/xinitrc.kde /etc/X11/xinit/xinitrc
        dm /usr/bin/kdm
    else
        dm xdm
    fi
fi

# Set correct permissions in the system:
chmod 1777 /dev/shm /tmp /var/tmp
chmod 700 /root

# Run SystemV init scripts for this runlevel:
[ -x /etc/rc.d/rc.sysvinit ] && . /etc/rc.d/rc.sysvinit

# Start the local setup procedure:
[ -x /etc/rc.d/rc.local ] && . /etc/rc.d/rc.local

# If autoexec= boot parameter is present, automatically start the command given.
# If you need to execute several commands, separate them using ';'
# If you need to use spaces in your command, use ~ instead
# If you wish to reboot/poweroff after the command's execution is finished,
# simply use autoexec=yourCommand;poweroff for example.
AUTOCMD=`egrep -o " autoexec=[^ ]+" /proc/cmdline | cut -d= -f2 | tr "~" " "`
[ "$AUTOCMD" = "" ] || { echo "Starting autoexec commands"; su --login -c "$AUTOCMD"; }

# Start other services in the background:
echo "[1m""Starting other services in the background""[0m"
sh /etc/rc.d/rc.services >/dev/null 2>&1 &

# All done.
