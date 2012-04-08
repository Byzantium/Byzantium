#!/bin/sh

config() {
  NEW="$1"
  OLD="$(dirname $NEW)/$(basename $NEW .new)"
  # If there's no config file by that name, mv it over:
  if [ ! -r $OLD ]; then
    mv $NEW $OLD
  elif [ "$(cat $OLD | md5sum)" = "$(cat $NEW | md5sum)" ]; then # toss the redundant copy
    rm $NEW
  fi
  # Otherwise, we leave the .new copy for the admin to consider...
}

# Keep same perms on rc.murmur.new:
if [ -e etc/rc.d/rc.murmur ]; then
  cp -a etc/rc.d/rc.murmur etc/rc.d/rc.murmur.new.incoming
  cat etc/rc.d/rc.murmur.new > etc/rc.d/rc.murmur.new.incoming
  mv etc/rc.d/rc.murmur.new.incoming etc/rc.d/rc.murmur.new
else
  # Install executable otherwise - irrelevant unless user starts in rc.local
  chmod 0755 etc/rc.d/rc.murmur.new
fi

config etc/rc.d/rc.murmur.new
config etc/murmur.ini.new
