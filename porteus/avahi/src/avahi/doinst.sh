config() {
  NEW="$1"
  OLD="$(dirname $NEW)/$(basename $NEW .new)"
  # If there's no config file by that name, mv it over:
  if [ ! -r $OLD ]; then
    mv $NEW $OLD
  elif [ "$(cat $OLD | md5sum)" = "$(cat $NEW | md5sum)" ]; then
    # toss the redundant copy
    rm $NEW
  fi
  # Otherwise, we leave the .new copy for the admin to consider...
}

preserve_perms() {
  NEW="$1"
  OLD="$(dirname $NEW)/$(basename $NEW .new)"
  if [ -e $OLD ]; then
    cp -a $OLD ${NEW}.incoming
    cat $NEW > ${NEW}.incoming
    mv ${NEW}.incoming $NEW
  fi
  config $NEW
}


preserve_perms etc/rc.d/rc.avahidaemon.new
preserve_perms etc/rc.d/rc.avahidnsconfd.new
preserve_perms etc/avahi/avahi-daemon.conf.new
preserve_perms etc/dbus-1/system.d/avahi-dbus.conf.new

if [ -x /usr/bin/update-desktop-database ]; then
  /usr/bin/update-desktop-database -q usr/share/applications >/dev/null 2>&1
fi

# Reload messagebus service
if [ -x etc/rc.d/rc.messagebus ]; then
  chroot . /etc/rc.d/rc.messagebus reload
fi

