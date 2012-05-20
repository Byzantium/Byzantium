#!/bin/sh

# Byzantium Linux top level build script.
# by: Sitwon
# This shell script, when executed inside of a Porteus build machine, will
# result in the generation of the file 000-byzantium.xzm.

# Bail on errors
set -e

# Create the fakeroot.
cd ~guest/byzantium
echo "Deleting and recreating the fakeroot..."
rm -rf /tmp/fakeroot
mkdir -p /tmp/fakeroot

# Test to see if the Byzantium SVN repository has been checked out into the
# home directory of the guest user.  ABEND if it's not.
if [ ! -d /home/guest/byzantium ]; then
    echo "ERROR: Byzantium SVN package repository not found in ~/guest."
    exit 1
    fi

# Unpack all of the .xzm packages into the fakeroot to populate it with the
# libraries and executables under the hood of Byzantium.
for i in `cat required_packages.txt` ; do
    echo "Now installing $i to /tmp/fakeroot..."
    xzm2dir /home/guest/byzantium/$i /tmp/fakeroot
    echo "Done."
    done

# Should these steps be necessary? Maybe it should be fixed in the module.
rm /tmp/fakeroot/srv/httpd
rm /tmp/fakeroot/srv/www
mkdir -p /tmp/fakeroot/srv/httpd/databases
cd /tmp/fakeroot/srv
ln -s httpd www

# We should build a controlpanel module to obviate these steps.
mkdir -p /tmp/fakeroot/srv/controlpanel/graphs
cp -rv ~guest/Byzantium/control_panel/srv/controlpanel/* /tmp/fakeroot/srv/controlpanel
mkdir -p /tmp/fakeroot/etc/controlpanel
cp ~guest/Byzantium/control_panel/etc/controlpanel/* /tmp/fakeroot/etc/controlpanel
mkdir -p /tmp/fakeroot/var/db/controlpanel
cp -rv ~guest/Byzantium/control_panel/var/db/controlpanel/* /tmp/fakeroot/var/db/controlpanel

# We need to upgrade the version of Wicd to fix a bug, this is our own fix.
# Note that when Poteus Linux updates to the latest stable version of wicd
# this will become obsolete.
mkdir -p /tmp/fakeroot/usr/share/wicd/cli
cp ~guest/Byzantium/porteus/wicd/usr/share/wicd/cli/wicd-cli.py /tmp/fakeroot/usr/share/wicd/cli/

# Install our custom udev automount rules.
cd ~guest/Byzantium/scripts
mkdir -p /tmp/fakeroot/etc/udev/rules.d
cp 11-media-by-label-auto-mount.rules /tmp/fakeroot/etc/udev/rules.d

# Could these be placed in a module?
cp rc.local rc.mysqld rc.ssl rc.setup_mysql /tmp/fakeroot/etc/rc.d

# Do we really want to enable _everything_?
chmod +x /tmp/fakeroot/etc/rc.d/rc.*

# This stuff probably belongs in the controlpanel package.
cp traffic_stats.sh /tmp/fakeroot/usr/local/bin
cd ../control_panel
mkdir -p /tmp/fakeroot/usr/local/sbin
cp *.py /tmp/fakeroot/usr/local/sbin
cp etc/rc.d/rc.byzantium /tmp/fakeroot/etc/rc.d/
mkdir -p /tmp/fakeroot/etc/ssl
cp etc/ssl/openssl.cnf /tmp/fakeroot/etc/ssl

# Add the custom Firefox configuration.
cd ../porteus
mkdir -p /tmp/fakeroot/home/guest/.mozilla/firefox/c3pp43bg.default
cp home/guest/.mozilla/firefox/c3pp43bg.default/prefs.js /tmp/fakeroot/home/guest/.mozilla/firefox/c3pp43bg.default

# Why aren't these in their modules?
cp -rv apache/etc/httpd/* /tmp/fakeroot/etc/httpd
cp babel/babeld.conf /tmp/fakeroot/etc
cp dnsmasq/dnsmasq.conf /tmp/fakeroot/etc
cp etherpad-lite/rc.etherpad-lite /tmp/fakeroot/etc/rc.d
cp -rv ifplugd/etc/ifplugd/* /tmp/fakeroot/etc/ifplugd
cp sudo/etc/sudoers /tmp/fakeroot/etc
chown root:root /tmp/fakeroot/etc/sudoers
chmod 0440 /tmp/fakeroot/etc/sudoers

# Add the custom passwd files.
cp etc/passwd /tmp/fakeroot/etc
cp etc/shadow /tmp/fakeroot/etc
cp etc/hosts /tmp/fakeroot/etc
cp etc/HOSTNAME /tmp/fakeroot/etc
cp etc/inittab /tmp/fakeroot/etc
chown root:root /tmp/fakeroot/etc/passwd /tmp/fakeroot/etc/shadow /tmp/fakeroot/etc/hosts /tmp/fakeroot/etc/HOSTNAME /tmp/fakeroot/etc/inittab
chmod 0600 /tmp/fakeroot/etc/shadow

# These belong in modules!
cp mysql/my.cnf /tmp/fakeroot/etc
cp ngircd/ngircd.conf /tmp/fakeroot/etc
cp ngircd/rc.ngircd /tmp/fakeroot/etc/rc.d
cp php/etc/httpd/php.ini /tmp/fakeroot/etc/httpd

# This should be a module
mkdir -p /tmp/fakeroot/opt/qwebirc
cp qwebirc/config.py /tmp/fakeroot/opt/qwebirc
cp qwebirc/rc.qwebirc /tmp/fakeroot/etc/rc.d

# Install the database files.
cd ..
cp databases/* /tmp/fakeroot/srv/httpd/databases

# Add our custom desktop stuff.
mkdir -p /tmp/fakeroot/home/guest/Desktop
cp porteus/home/guest/Desktop/Control\ Panel.desktop /tmp/fakeroot/home/guest/Desktop
mkdir -p /tmp/fakeroot/usr/share/pixmaps/porteus
cp byzantium-icon.png /tmp/fakeroot/usr/share/pixmaps/porteus

# Create the runtime directory for ngircd because its package doesn't.
mkdir /tmp/fakeroot/var/run/ngircd
chown ngircd.root /tmp/fakeroot/var/run/ngircd
chmod 0750 /tmp/fakeroot/var/run/ngircd

# Install the captive portal daemon.
mkdir -p /tmp/fakeroot/srv/captiveportal
mkdir -p /tmp/fakeroot/etc/captiveportal
cd ~guest/Byzantium/captive_portal
cp captive_portal.py /tmp/fakeroot/usr/local/sbin
cp captive-portal.sh /tmp/fakeroot/usr/local/sbin
cp mop_up_dead_clients.py/tmp/fakeroot/usr/local/sbin
cp etc/captiveportal/captiveportal.conf /tmp/fakeroot/etc/captiveportal/
cp srv/captiveportal/* /tmp/fakeroot/srv/captiveportal/

# Directory ownership sanity for ~guest.
chown -R guest:guest /tmp/fakeroot/home/guest

# Build the Byzantium module.
dir2xzm /tmp/fakeroot /tmp/000-byzantium.xzm

