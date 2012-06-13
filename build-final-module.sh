#!/bin/sh

# Byzantium Linux top level build script.
# by: Sitwon
# This shell script, when executed inside of a Porteus build machine, will
# result in the generation of the file 000-byzantium.xzm.

# Bail on errors
set -e

BUILD_HOME=${BUILD_HOME:-/home/guest}
# Create the fakeroot.
cd $BUILD_HOME/Byzantium
echo "Deleting and recreating the fakeroot..."
rm -rf /tmp/fakeroot
mkdir -p /tmp/fakeroot

# Test to see if the Byzantium SVN repository has been checked out into the
# home directory of the guest user.  ABEND if it's not.
if [ ! -d $BUILD_HOME/byzantium ]; then
    echo "ERROR: Byzantium SVN package repository not found in ~/guest."
    exit 1
    fi

# Unpack all of the .xzm packages into the fakeroot to populate it with the
# libraries and executables under the hood of Byzantium.
for i in `cat required_packages.txt` ; do
    echo "Now installing $i to /tmp/fakeroot..."
    xzm2dir $BUILD_HOME/byzantium/$i /tmp/fakeroot
    echo "Done."
    done

# The thing about symlinks is that they're absolute.  When you're building in
# a fakeroot this breaks things.  So, we have to set up the web server's
# content directories manually.
echo "Deleting bad symlinks to httpd directories."
rm /tmp/fakeroot/srv/httpd
rm /tmp/fakeroot/srv/www

echo "Creating database and web server content directories."
mkdir -p /tmp/fakeroot/srv/httpd/htdocs
mkdir -p /tmp/fakeroot/srv/httpd/cgi-bin
mkdir -p /tmp/fakeroot/srv/httpd/databases
cd /tmp/fakeroot/srv
ln -s httpd www

# We should build a controlpanel module to obviate these steps.
echo "Creating directories for the traffic graphs."
mkdir -p /tmp/fakeroot/srv/controlpanel/graphs

echo "Copying control panel's HTML templates into place."
cp -rv $BUILD_HOME/Byzantium/control_panel/srv/controlpanel/* /tmp/fakeroot/srv/controlpanel

echo "Installing control panel config files."
mkdir -p /tmp/fakeroot/etc/controlpanel
cp $BUILD_HOME/Byzantium/control_panel/etc/controlpanel/* /tmp/fakeroot/etc/controlpanel

echo "Installing control panel's SQLite databases and schemas."
mkdir -p /tmp/fakeroot/var/db/controlpanel
cp -rv $BUILD_HOME/Byzantium/control_panel/var/db/controlpanel/* /tmp/fakeroot/var/db/controlpanel

# We need to upgrade the version of Wicd to fix a bug, this is our own fix.
# Note that when Poteus Linux updates to the latest stable version of wicd
# this will become obsolete.
echo "Installing wicd-cli patch."
mkdir -p /tmp/fakeroot/usr/share/wicd/cli
cp $BUILD_HOME/Byzantium/porteus/wicd/usr/share/wicd/cli/wicd-cli.py /tmp/fakeroot/usr/share/wicd/cli/

# Install our custom udev automount rules.
echo "Installing udev media-by-label rule patch."
cd $BUILD_HOME/Byzantium/scripts
mkdir -p /tmp/fakeroot/etc/udev/rules.d
cp 11-media-by-label-auto-mount.rules /tmp/fakeroot/etc/udev/rules.d

# Could these be placed in a module?
echo "Installing custom initscripts."
cp rc.local rc.mysqld rc.ssl rc.setup_mysql rc.inet1 /tmp/fakeroot/etc/rc.d
chmod +x /tmp/fakeroot/etc/rc.d/rc.*

# Set up mDNS service descriptor repository.
mkdir -p /tmp/fakeroot/etc/avahi/inactive

# This stuff probably belongs in the controlpanel package.
echo "Installing rrdtool shell script."
cp traffic_stats.sh /tmp/fakeroot/usr/local/bin

echo "Installing the control panel."
cd ../control_panel
mkdir -p /tmp/fakeroot/usr/local/sbin
cp *.py /tmp/fakeroot/usr/local/sbin
cp etc/rc.d/rc.byzantium /tmp/fakeroot/etc/rc.d/

echo "Installing OpenSSL configuration file."
mkdir -p /tmp/fakeroot/etc/ssl
cp etc/ssl/openssl.cnf /tmp/fakeroot/etc/ssl

# Install the CGI-BIN script that implements the service directory the users
# see.
echo "Installing the service directory."
cd ../service_directory
cp index.html /tmp/fakeroot/srv/httpd/htdocs
cp services.py /tmp/fakeroot/srv/httpd/cgi-bin
chmod 0755 /tmp/fakeroot/srv/httpd/cgi-bin/services.py

# Add the custom Firefox configuration.
echo "Installing Mozilla configs for the guest user."
cd ../porteus
mkdir -p /tmp/fakeroot/home/guest/.mozilla/firefox/c3pp43bg.default
cp home/guest/.mozilla/firefox/c3pp43bg.default/prefs.js /tmp/fakeroot/home/guest/.mozilla/firefox/c3pp43bg.default

# Why aren't these in their modules?
echo "Installing custom configuration files and initscripts for services."
cp -rv apache/etc/httpd/* /tmp/fakeroot/etc/httpd
cp babel/babeld.conf /tmp/fakeroot/etc
cp dnsmasq/dnsmasq.conf /tmp/fakeroot/etc
cp etherpad-lite/rc.etherpad-lite /tmp/fakeroot/etc/rc.d
cp etherpad-lite/settings.json /tmp/fakeroot/opt/etherpad-lite
cp etherpad-lite/etherpad-lite.service /tmp/fakeroot/etc/avahi/inactive
cp sudo/etc/sudoers /tmp/fakeroot/etc
chown root:root /tmp/fakeroot/etc/sudoers
chmod 0440 /tmp/fakeroot/etc/sudoers

# Add the custom passwd and group files.
echo "Installing custom system configuration files."
cp etc/passwd /tmp/fakeroot/etc
cp etc/shadow /tmp/fakeroot/etc
cp etc/hosts /tmp/fakeroot/etc
cp etc/HOSTNAME /tmp/fakeroot/etc
cp etc/inittab /tmp/fakeroot/etc
cp etc/group /tmp/fakeroot/etc
chown root:root /tmp/fakeroot/etc/passwd /tmp/fakeroot/etc/shadow /tmp/fakeroot/etc/hosts /tmp/fakeroot/etc/HOSTNAME /tmp/fakeroot/etc/inittab /tmp/fakeroot/etc/group
chmod 0600 /tmp/fakeroot/etc/shadow

# These belong in modules!
echo "Installing config files for MySQL, ngircd, and PHP."
cp mysql/my.cnf /tmp/fakeroot/etc
cp ngircd/ngircd.conf /tmp/fakeroot/etc
cp ngircd/ngircd.service /tmp/fakeroot/etc/avahi/inactive
cp ngircd/rc.ngircd /tmp/fakeroot/etc/rc.d
cp php/etc/httpd/php.ini /tmp/fakeroot/etc/httpd

# This should be a module
echo "Installing qwebirc configuration file and initscript."
mkdir -p /tmp/fakeroot/opt/qwebirc
cp qwebirc/config.py /tmp/fakeroot/opt/qwebirc
cp qwebirc/rc.qwebirc /tmp/fakeroot/etc/rc.d
cp qwebirc/qwebirc.service /tmp/fakeroot/etc/avahi/inactive

# Install the database files.
echo "Installing database files."
cd ..
cp databases/* /tmp/fakeroot/srv/httpd/databases

# Add our custom desktop stuff.
echo "Customizing desktop for guest user."
mkdir -p /tmp/fakeroot/home/guest/Desktop
cp porteus/home/guest/Desktop/Control\ Panel.desktop /tmp/fakeroot/home/guest/Desktop
mkdir -p /tmp/fakeroot/usr/share/pixmaps/porteus
cp byzantium-icon.png /tmp/fakeroot/usr/share/pixmaps/porteus

# Create the runtime directory for ngircd because its package doesn't.
echo "Setting up directories for ngircd."
mkdir /tmp/fakeroot/var/run/ngircd
chown ngircd.root /tmp/fakeroot/var/run/ngircd
chmod 0750 /tmp/fakeroot/var/run/ngircd

# Install the captive portal daemon.
echo "Installing captive portal."
mkdir -p /tmp/fakeroot/srv/captiveportal
mkdir -p /tmp/fakeroot/etc/captiveportal
cd $BUILD_HOME/Byzantium/captive_portal
cp captive_portal.py /tmp/fakeroot/usr/local/sbin
cp captive-portal.sh /tmp/fakeroot/usr/local/sbin
cp mop_up_dead_clients.py /tmp/fakeroot/usr/local/sbin
cp fake_dns.py /tmp/fakeroot/usr/local/sbin
cp etc/captiveportal/captiveportal.conf /tmp/fakeroot/etc/captiveportal/
cp srv/captiveportal/* /tmp/fakeroot/srv/captiveportal/

# Directory ownership sanity for ~guest.
echo "Confirming ownership of guest user's home directory."
chown -R guest:guest /tmp/fakeroot/home/guest

# Build the Byzantium module.
echo "Building 000-byzantium.xzm.  Sit back and enjoy the ride."
dir2xzm /tmp/fakeroot /tmp/000-byzantium.xzm

# "Hey, Bishop - do the thing with the knife!"
