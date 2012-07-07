#!/bin/sh

# Byzantium Linux top level build script.
# by: Sitwon
# This shell script, when executed inside of a Porteus build machine, will
# result in the generation of the file 000-byzantium.xzm.

# Bail on errors
set -e

FAKE_ROOT=${FAKE_ROOT:-/tmp/fakeroot}
BUILD_HOME=${BUILD_HOME:-/home/guest}
OUTPUT=${OUTPUT:-/tmp}
LANGUAGE_ROOT=${LANGUAGE_ROOT:-/tmp/languageroot}

# clean $FAKE_ROOT if true
CLEAN_FAKE_ROOT=true

# clean $LANGUAGE_ROOT if true
CLEAN_LANGUAGE_ROOT=true

# set true to run http_placeholder.sh to add the web service placeholder pages
HTTP_PLACEHOLDER=true

if_def(){
# if $1 is defined include $2 inline (execute it's contents in the
# current scope)
if [ -n $1 ] ;then
	. $2
fi
}

is_user(){
	####### check if user exists: function for build machines
	## @param $1 user[:group]
	## @param $2 UID[:GID]
	## @return 0|1 where 0 is 
	#################################################
	# check for user and if it does not exist create it with the UID passed to this function
	user=${1//[\.\:]*/} #username
	uid=${2//[\.\:]*/} #uid
	grptmp=${1//*[\.\:]/} #temporary value for getting the groupname
	gidtmp=${2//*[\.\:]/} #temporary value for getting the gid
	group=${grptmp//$user/} #groupname or ''
	gid=${gidtmp//$uid/} #gid or ''
	if [ $group ] ;then
		if [[ `grep -e "^${group}:" /etc/group` == '' ]] ;then
			echo "creating group ${group} with gid ${gid}"
			groupadd -g $gid $group
		fi
		if [[ `grep -e "^${user}:" /etc/passwd` == '' ]] ;then
			echo "creating user ${user} with uid ${uid} and gid ${gid}"
			useradd -u ${uid} -g $gid $user
		fi
	elif [[ `grep -e "^${user}:" /etc/passwd` == '' ]] ;then
		echo "creating user ${user} with uid ${uid}"
		useradd -u ${uid} ${user}
	fi
}

safe_chown(){
	is_user $1 $2
	chown ${1} ${*:3}
}

# Create the fakeroots.
cd $BUILD_HOME/Byzantium
echo "Deleting and recreating the fakeroots..."
if $CLEAN_FAKE_ROOT ;then
    read -p "Rebuilding fakeroot, okay? [press enter to continue]" rebuild
    rm -rf ${FAKE_ROOT}
    mkdir -p ${FAKE_ROOT}
    if $CLEAN_LANGUAGE_ROOT ;then
        rm -rf ${LANGUAGE_ROOT}
         mkdir -p ${LANGUAGE_ROOT}
    fi
fi

# Test to see if the Byzantium SVN repository has been checked out into the
# home directory of the guest user.  ABEND if it's not.
    if [ ! -d $BUILD_HOME/byzantium ]; then
        echo "ERROR: Byzantium SVN repository not found in $BUILD_HOME."
        exit 1
    fi

# Unpack all of the .xzm packages into the fakeroots to populate it with the
# libraries and executables under the hood of Byzantium.
    for i in `cat required_packages.txt` ; do
        echo "Now installing $i to ${FAKE_ROOT}..."
        xzm2dir $BUILD_HOME/byzantium/$i ${FAKE_ROOT}
        echo "Done."
    done
    for i in `cat languages.txt` ; do
        echo "Now installing $i to ${LANGUAGE_ROOT}..."
        xzm2dir $BUILD_HOME/byzantium/$i ${LANGUAGE_ROOT}
        echo "Done."
    done

# The thing about symlinks is that they're absolute.  When you're building in
# a fakeroot this breaks things.  So, we have to set up the web server's
# content directories manually.
    echo "Deleting bad symlinks to httpd directories."
    rm ${FAKE_ROOT}/srv/httpd
    rm ${FAKE_ROOT}/srv/www

else
    echo "Skipping fakeroot rebuild."
fi # end if $CLEAN_FAKE_ROOT

echo "Creating database and web server content directories."
mkdir -p ${FAKE_ROOT}/srv/httpd/htdocs
if_def $HTTP_PLACEHOLDER ${BUILD_HOME}/Byzantium/http_placeholder.sh # conditionally run the script at arg 2
mkdir -p ${FAKE_ROOT}/srv/httpd/cgi-bin
mkdir -p ${FAKE_ROOT}/srv/httpd/databases
cd ${FAKE_ROOT}/srv
ln -s httpd www || echo -n

# We should build a controlpanel module to obviate these steps.
echo "Creating directories for the traffic graphs."
mkdir -p ${FAKE_ROOT}/srv/controlpanel/graphs

echo "Copying control panel's HTML templates into place."
cp -rv $BUILD_HOME/Byzantium/control_panel/srv/controlpanel/* ${FAKE_ROOT}/srv/controlpanel

echo "Installing control panel config files."
mkdir -p ${FAKE_ROOT}/etc/controlpanel
cp $BUILD_HOME/Byzantium/control_panel/etc/controlpanel/* ${FAKE_ROOT}/etc/controlpanel

echo "Installing control panel's SQLite databases and schemas."
mkdir -p ${FAKE_ROOT}/var/db/controlpanel
cp -rv $BUILD_HOME/Byzantium/control_panel/var/db/controlpanel/* ${FAKE_ROOT}/var/db/controlpanel

# Create the xdg directory tree and populate it with our wicd disabler.
echo "Creating desktop environment autostart directories."
mkdir -p ${FAKE_ROOT}/etc/xdg/autostart/
mkdir -p ${FAKE_ROOT}/usr/share/autostart/
cp -rv $BUILD_HOME/Byzantium/porteus/etc/xdg/autostart/wicd-tray.desktop ${FAKE_ROOT}/etc/xdg/autostart
cp -rv $BUILD_HOME/Byzantium/porteus/usr/share/autostart/wicd-tray.desktop ${FAKE_ROOT}/usr/share/autostart

# Install our custom udev automount rules.
echo "Installing udev media-by-label rule patch."
cd $BUILD_HOME/Byzantium/scripts
mkdir -p ${FAKE_ROOT}/etc/udev/rules.d
cp 11-media-by-label-auto-mount.rules ${FAKE_ROOT}/etc/udev/rules.d

# Could these be placed in a module?
echo "Installing custom initscripts."
cp rc.local rc.mysqld rc.ssl rc.setup_mysql rc.inet1 rc.M ${FAKE_ROOT}/etc/rc.d
chmod +x ${FAKE_ROOT}/etc/rc.d/rc.*

# Set up mDNS service descriptor repository.
mkdir -p ${FAKE_ROOT}/etc/avahi/inactive

# Configure libnss to reference mDNS for resolution in addition to DNS.
cp ${FAKE_ROOT}/etc/nsswitch.conf-mdns ${FAKE_ROOT}/etc/nsswitch.conf

# This stuff probably belongs in the controlpanel package.
echo "Installing rrdtool shell script."
cp traffic_stats.sh ${FAKE_ROOT}/usr/local/bin

echo "Installing the control panel."
cd ../control_panel
mkdir -p ${FAKE_ROOT}/usr/local/sbin
cp *.py *.sh ${FAKE_ROOT}/usr/local/sbin
cp etc/rc.d/rc.byzantium ${FAKE_ROOT}/etc/rc.d/

echo "Installing OpenSSL configuration file."
mkdir -p ${FAKE_ROOT}/etc/ssl
cp etc/ssl/openssl.cnf ${FAKE_ROOT}/etc/ssl

# Install the CGI-BIN script that implements the service directory the users
# see.
echo "Installing the service directory."
cd ../service_directory
cp index.html ${FAKE_ROOT}/srv/httpd/htdocs
cp -r services.py _services.py _utils.py tmpl ${FAKE_ROOT}/srv/httpd/cgi-bin
chmod 0755 ${FAKE_ROOT}/srv/httpd/cgi-bin/services.py
mkdir -p ${FAKE_ROOT}/opt/byzantium/avahi/
cp avahiclient.sh avahiclient.py _utils.py ${FAKE_ROOT}/opt/byzantium/avahi/
chmod -R 0755 ${FAKE_ROOT}/opt/byzantium/avahi/
cp rc.avahiclient ${FAKE_ROOT}/etc/rc.d/
chmod 0755 ${FAKE_ROOT}/etc/rc.d/rc.avahiclient

# Add the custom Firefox configuration.
echo "Installing Mozilla configs for the guest user."
cd ../porteus
mkdir -p ${FAKE_ROOT}/home/guest/.mozilla/firefox/c3pp43bg.default
cp home/guest/.mozilla/firefox/c3pp43bg.default/prefs.js ${FAKE_ROOT}/home/guest/.mozilla/firefox/c3pp43bg.default

# Why aren't these in their modules?
echo "Installing custom configuration files and initscripts for services."
cp -rv apache/etc/httpd/* ${FAKE_ROOT}/etc/httpd
cp babel/babeld.conf ${FAKE_ROOT}/etc
cp dnsmasq/dnsmasq.conf ${FAKE_ROOT}/etc
cp etherpad-lite/rc.etherpad-lite ${FAKE_ROOT}/etc/rc.d
cp etherpad-lite/settings.json ${FAKE_ROOT}/opt/etherpad-lite
cp etherpad-lite/etherpad-lite.service ${FAKE_ROOT}/etc/avahi/inactive
cp sudo/etc/sudoers ${FAKE_ROOT}/etc
chown root:root ${FAKE_ROOT}/etc/sudoers
chmod 0440 ${FAKE_ROOT}/etc/sudoers
cp avahi/etc/avahi/avahi-daemon.conf ${FAKE_ROOT}/etc/avahi
cp etc/profile ${FAKE_ROOT}/etc

# Install our custom avahi-dnsconfd.action script.
cp avahi/etc/avahi/avahi-dnsconfd.action ${FAKE_ROOT}/etc/avahi

# Install our custom dhcpcd event script.
mkdir -p ${FAKE_ROOT}/lib/dhcpcd/dhcpcd-hooks
cp dhcpcd/lib/dhcpcd/dhcpcd-hooks/*.conf ${FAKE_ROOT}/lib/dhcpcd/dhcpcd-hooks
chmod 0444 ${FAKE_ROOT}/lib/dhcpcd/dhcpcd-hooks/*.conf

# Add the custom passwd and group files.
echo "Installing custom system configuration files."
cp etc/passwd ${FAKE_ROOT}/etc
cp etc/shadow ${FAKE_ROOT}/etc
cp etc/hosts ${FAKE_ROOT}/etc
cp etc/HOSTNAME ${FAKE_ROOT}/etc
cp etc/inittab ${FAKE_ROOT}/etc
cp etc/group ${FAKE_ROOT}/etc
chown root:root ${FAKE_ROOT}/etc/passwd ${FAKE_ROOT}/etc/shadow ${FAKE_ROOT}/etc/hosts ${FAKE_ROOT}/etc/HOSTNAME ${FAKE_ROOT}/etc/inittab ${FAKE_ROOT}/etc/group
chmod 0600 ${FAKE_ROOT}/etc/shadow

# These belong in modules!
echo "Installing config files for MySQL, ngircd, and PHP."
cp mysql/my.cnf ${FAKE_ROOT}/etc
cp ngircd/ngircd.conf ${FAKE_ROOT}/etc
cp ngircd/ngircd.service ${FAKE_ROOT}/etc/avahi/inactive
cp ngircd/rc.ngircd ${FAKE_ROOT}/etc/rc.d
cp php/etc/httpd/php.ini ${FAKE_ROOT}/etc/httpd

# This should be a module
echo "Installing qwebirc configuration file and initscript."
mkdir -p ${FAKE_ROOT}/opt/qwebirc
cp qwebirc/config.py ${FAKE_ROOT}/opt/qwebirc
cp qwebirc/rc.qwebirc ${FAKE_ROOT}/etc/rc.d
cp qwebirc/qwebirc.service ${FAKE_ROOT}/etc/avahi/inactive

# Install the database files.
echo "Installing database files."
cd ..
cp databases/* ${FAKE_ROOT}/srv/httpd/databases

# Add our custom desktop stuff.
echo "Customizing desktop for guest user."
mkdir -p ${FAKE_ROOT}/home/guest/Desktop
cp porteus/home/guest/Desktop/Control\ Panel.desktop ${FAKE_ROOT}/home/guest/Desktop
mkdir -p ${FAKE_ROOT}/usr/share/pixmaps/porteus
cp byzantium-icon.png ${FAKE_ROOT}/usr/share/pixmaps/porteus

# Create the runtime directory for ngircd because its package doesn't.
echo "Setting up directories for ngircd."
mkdir -p ${FAKE_ROOT}/var/run/ngircd
safe_chown ngircd:root 1002:0 ${FAKE_ROOT}/var/run/ngircd
chmod 0750 ${FAKE_ROOT}/var/run/ngircd

# Install the captive portal daemon.
echo "Installing captive portal."
mkdir -p ${FAKE_ROOT}/srv/captiveportal
mkdir -p ${FAKE_ROOT}/etc/captiveportal
cd $BUILD_HOME/Byzantium/captive_portal
cp captive_portal.py ${FAKE_ROOT}/usr/local/sbin
cp captive-portal.sh ${FAKE_ROOT}/usr/local/sbin
cp mop_up_dead_clients.py ${FAKE_ROOT}/usr/local/sbin
cp fake_dns.py ${FAKE_ROOT}/usr/local/sbin
cp etc/captiveportal/captiveportal.conf ${FAKE_ROOT}/etc/captiveportal/
cp srv/captiveportal/* ${FAKE_ROOT}/srv/captiveportal/

# Directory ownership sanity for ~guest.
echo "Confirming ownership of guest user's home directory."
safe_chown guest:guest 1000:1000 -R ${FAKE_ROOT}/home/guest

# Slap branding into module. This needs to be done more cleanly.
echo  "Installing branding stuff."
cp -dr $BUILD_HOME/Byzantium/branding/* ${FAKE_ROOT}/

# Build the Byzantium module.
echo "Building 000-byzantium.xzm.  Sit back and enjoy the ride."
dir2xzm ${FAKE_ROOT} ${OUTPUT}/000-byzantium.xzm

# Build the language module.
echo "Building 001-languages.xzm."
dir2xzm ${LANGUAGE_ROOT} ${OUTPUT}/001-language.xzm

# "Hey, Bishop - do the thing with the knife!"
