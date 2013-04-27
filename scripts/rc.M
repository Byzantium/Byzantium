#!/bin/bash
#
# rc.M		This file is executed by init(8) when the system is being
#		initialized for one of the "multi user" run levels (i.e.
#		levels 1 through 6).

# Bind magic folders:
if [ -e /etc/magic_folders/folders.cfg ]; then
    if ! egrep -qo " nomagic( |\$)" /proc/cmdline; then
	echo "Binding magic folders:"
	num=`grep -c / /etc/magic_folders/folders.cfg`; x=1
	while [ $x -le $num ]; do
	    # Source:
	    src=`sed -n "$x"p /etc/magic_folders/folders.cfg | cut -d" " -f1`
	    # UUID to sdXY translation:
	    uuid=`echo $src | cut -d/ -f3`; dev=`blkid -U $uuid | cut -d/ -f3`; src=`echo $src | sed s/$uuid/$dev/g`
	    # Destination:
	    dest=`sed -n "$x"p /etc/magic_folders/folders.cfg | cut -d" " -f2`
	    if [ -f $src ]; then
		# fsck for magic .dat containers:
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
		echo "mount -o loop $src $dest"
		if file $src | grep -qo LUKS; then
		    x=`losetup -a | tail -n1 | cut -d: -f1 | sed s^/dev/loop^^`; let y=x+1
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

# Initialize loopback interface:
ifconfig lo 127.0.0.1 2>/dev/null &
route add -net 127.0.0.0 netmask 255.0.0.0 lo 2>/dev/null &

# Initialize the networking hardware:
#[ -x /etc/rc.d/rc.inet1 ] && . /etc/rc.d/rc.inet1 &

# Start ACPI daemon:
[ -x /etc/rc.d/rc.acpid ] && /usr/sbin/acpid -n &

# Start NetworkManager:
#[ -x /etc/rc.d/rc.networkmanager ] && /usr/sbin/NetworkManager &

# Determine display manager and autologin requested user:
login=`egrep -o " login=[^ ]+*" /proc/cmdline | cut -dn -f2`
dm() { echo $1 > /mnt/live/tmp/dm; }
if egrep -qo " lxde( |\$)" /proc/cmdline; then
    if [ -x /usr/sbin/lxdm ]; then
        sed -i -r s^session=/.*$^session=/usr/bin/startlxde^g /etc/lxdm/lxdm.conf 2>/dev/null
        ln -sf /etc/X11/xinit/xinitrc.lxde /etc/X11/xinit/xinitrc
        dm /usr/sbin/lxdm
        [ "$login" ] && sed -i -r s/^.*autologin=.*$/autologin$login/g /etc/lxdm/lxdm.conf || sed -i -r s/^.*autologin=.*$/autologin=guest/g /etc/lxdm/lxdm.conf
    else
        dm xdm
    fi
elif egrep -qo " xfce( |\$)" /proc/cmdline; then
    if [ -x /usr/sbin/lxdm ]; then
        sed -i -r s^session=/.*$^session=/usr/bin/startxfce4^g /etc/lxdm/lxdm.conf 2>/dev/null
        ln -sf /etc/X11/xinit/xinitrc.xfce /etc/X11/xinit/xinitrc
        dm /usr/sbin/lxdm
        [ "$login" ] && sed -i -r s/^.*autologin=.*$/autologin$login/g /etc/lxdm/lxdm.conf || sed -i -r s/^.*autologin=.*$/autologin=guest/g /etc/lxdm/lxdm.conf
    else
        dm xdm
    fi
else
    if test -x /usr/bin/kdm; then
        ln -sf /etc/X11/xinit/xinitrc.kde /etc/X11/xinit/xinitrc
        dm /usr/bin/kdm
        [ "$login" ] && sed --follow-symlinks -i -r s/^.*AutoLoginUser=.*$/AutoLoginUser$login/g /usr/share/config/kdm/kdmrc || sed --follow-symlinks -i -r s/^.*AutoLoginUser=.*$/AutoLoginUser=guest/g /usr/share/config/kdm/kdmrc
    elif test -x /usr/sbin/gdm; then
        ln -sf /etc/X11/xinit/xinitrc.razorqt /etc/X11/xinit/xinitrc
        dm /usr/sbin/gdm
        [ "$login" ] && sed -i -r s/^.*AutomaticLogin=.*$/AutomaticLogin$login/g /etc/X11/gdm/custom.conf || sed -i -r s/^.*AutomaticLogin=.*$/AutomaticLogin=guest/g /etc/X11/gdm/custom.conf
    else
        dm xdm
    fi
fi

# Set correct permissions in the system:
chmod 1777 /dev/shm /tmp /var/tmp 2>/dev/null
chmod 700 /root

# Run SystemV init scripts for this runlevel:
[ -x /etc/rc.d/rc.sysvinit ] && . /etc/rc.d/rc.sysvinit

# Start the local setup procedure:
[ -x /etc/rc.d/rc.local ] && . /etc/rc.d/rc.local

# Start other services in the background:
sh /etc/rc.d/rc.services >/dev/null 2>&1 &

# If autoexec= boot parameter is present, automatically start the command given.
# If you need to execute several commands, separate them using ';'
# If you need to use spaces in your command, use ~ instead
# If you wish to reboot/poweroff after the command's execution is finished,
# simply use autoexec=yourCommand;poweroff for example.
AUTOCMD=`egrep -o " autoexec=[^ ]+" /proc/cmdline | cut -d= -f2 | tr "~" " "`
[ -z "$AUTOCMD" ] || { echo "Starting autoexec commands"; su - -c "$AUTOCMD"; }

# All done.
