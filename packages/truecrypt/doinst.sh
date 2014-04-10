# Update the desktop database:
if [ -x usr/bin/update-desktop-database ]; then
  chroot . /usr/bin/update-desktop-database /usr/share/applications > /dev/null 2>&1
fi

