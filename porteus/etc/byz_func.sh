#!/bin/bash

# for use as dot import to provide common functions within byzantium

# define Bins
iptables=/usr/sbin/iptables
olsrd=/usr/sbin/olsrd #?
avahi=/stuff #fixme

# define Config files
olsrd_conf=/etc/olsrd.conf
resolv_conf=/etc/resolv.conf # note here
resolv_conf_gateway=${resolv_conf}.gateway # note here
resolv_conf_avahi=${resolv_conf}.avahi # note here

# Test if this node is a gateway node
# @return boolean (as bool as it gets in bash)
am_gateway() {
	#insert magic here
	return 1
}

# Set up NAT rules.
# @param 1 client interface
# @param 2 mesh interface
start_nat(){
	client=$1
	mesh=$2
	$iptables -t nat -A POSTROUTING -o $client -j MASQUERADE
	$iptables -A FORWARD -i $client -o $mesh -m state --state ESTABLISHED,RELATED -j ACCEPT
	$iptables -A FORWARD -i $mesh -o $1 -j ACCEPT
}

# Remove NAT rules.
# @param 1 client interface
# @param 2 mesh interface
stop_nat() {
	client=$1
	mesh=$2
	$iptables -t nat -D POSTROUTING -o $client -j MASQUERADE
	$iptables -D FORWARD -i $client -o $mesh -m state --state ESTABLISHED,RELATED -j ACCEPT
	$iptables -D FORWARD -i $mesh -o $1 -j ACCEPT
}

# Add "a couple" of DNSes to the resolv.conf file.
# @param 1 a path to a resolv.conf file
populate_a_resov_conf(){
	cat - > $1 <<-EOF
nameserver 8.8.4.4
nameserver 4.2.2.2
nameserver 208.67.220.220
nameserver 8.8.8.8
nameserver 4.2.2.1
nameserver 208.67.222.222
nameserver 4.2.2.4
EOF
}

# Figure out what network interface olsrd is listening on.
get_olsrd_iface(){
	echo -n `ps ax | grep [o]lsrd | awk '{print $NF}'` >> /dev/stdout	#fixme: does this work?
}

# Get the PID of olsrd.
get_olsrd_pid(){
	echo `ps ax | grep [o]lsrd | awk '{print $1}'` >> /dev/stdout	#fixme: does this work?
}

# Add a gateway route.
# @param 1 destination netmask
# @param 2 destination ip
olsrd_set_a_gateway(){
    echo "Hna4 {$dest_net $dest_ip}" >> $olsrd_conf
}

# Add gateway to the internet to olsrd
olsrd_set_global_gateway(){
	olsrd_set_a_gateway 0.0.0.0 0.0.0.0
}

