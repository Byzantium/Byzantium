#!/bin/bash

useradd -g 99 -u 1001 -M -N -s /bin/bash -d /opt/etherpad-lite etherpad
passwd -l etherpad
groupadd -g 1001 ngircd
useradd -g 1001 -u 1002 -M -N -s /bin/false -d /var/empty ngircd
passwd -l ngircd
mkdir -p /var/run/ngircd
chown ngircd.ngircd /var/run/ngircd
cp ~guest/Byzantium/control_panel/*.py /usr/local/sbin
chmod 744 /usr/local/sbin/*.py
mkdir -p /srv/controlpanel/graphs
cp -rv ~guest/Byzantium/control_panel/srv/controlpanel/* /srv/controlpanel
mkdir /etc/controlpanel
cp ~guest/Byzantium/control_panel/etc/controlpanel/* /etc/controlpanel
mkdir -p /var/db/controlpanel
cp -rv ~guest/Byzantium/control_panel/var/db/controlpanel/* /var/db/controlpanel
cd ~guest/Byzantium/scripts
cp 11-media-by-label-auto-mount.rules /etc/udev/rules.d/
cp rc.local rc.mysqld rc.ssl /etc/rc.d
cp traffic_stats.sh /usr/local/bin
cd ../control_panel
cp etc/rc.d/rc3.d/rc.byzantium /etc/rc.d/rc3.d
cp etc/ssl/openssl.cnf /etc/ssl
cd ../porteus
cp qwebirc/rc.qwebirc /etc/rc.d
cp qwebirc/config.py /opt/qwebirc
cp -rv apache/etc/httpd/* /etc/httpd
cp babel/babeld.conf /etc
cp dnsmasq/dnsmasq.conf /etc
cp etherpad-lite/rc.etherpad-lite /etc/rc.d
cp -rv ifplugd/etc/ifplugd/* /etc/ifplugd
cp mysql/my.cnf /etc
cp ngircd/ngircd.conf /etc
cp ngircd/rc.ngircd /etc/rc.d
cp php/etc/httpd/php.ini /etc/httpd
chmod +x \
  /etc/rc.d/rc.dnsmasq \
  /etc/rc.d/rc.ifplugd \
  /etc/rc.d/rc.httpd \
  /etc/rc.d/rc.local \
  /etc/rc.d/rc.mysqld \
  /etc/rc.d/rc.cups \
  /etc/rc.d/rc.gpm \

mysql_install_db --user=mysql
/etc/rc.d/rc.mysqld start
mysqladmin -u root password "NEW PASSWORD HERE"
/usr/bin/mysql_secure_installation <<EOF
NEW PASSWORD HERE
n
y
y
y
y
EOF

mysqladmin -u root -p'NEW PASSWORD HERE' create etherpad
mysqladmin -u root -p'NEW PASSWORD HERE' create statusnet
mysql -h localhost -u root -p'NEW PASSWORD HERE' <<EOF
use etherpad
source etherpad.sql
use statusnet
source statusnet.sql
grant all on statusnet.* to 'statusnet'@'localhost' identified by 'password';
grant all on etherpad.* to 'etherpad'@'localhost' identified by 'password';
flush privileges;
EOF

