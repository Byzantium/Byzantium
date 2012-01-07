#!/bin/bash

# I've hard-coded the IP address and network address of the client network in this example.
# Those should probably be parameterized so that they can be configured by the control-panel system.

IPTABLES=/usr/sbin/iptables

# Create a new chain for captive portal users
$IPTABLES -N internet -t mangle

# These two rules exempt traffic which does not originate from the client network
$IPTABLES -t mangle -A PREROUTING -p tcp ! -s 192.168.21.0/24 -j RETURN
$IPTABLES -t mangle -A PREROUTING -p udp ! -s 192.168.21.0/24 -j RETURN

# Traffic not exempted by above rules are kicked to the captive portal chain
$IPTABLES -t mangle -A PREROUTING -j internet

# This would be used to pre-load the captive portal chain with accepted users
#awk 'BEGIN { FS="\t"; } { system("$IPTABLES -t mangle -A internet -m mac--mac-source "$4" -j RETURN"); }' /var/lib/users

# Traffic not coming from an accepted user gets marked 99
$IPTABLES -t mangle -A internet -j MARK --set-mark 99
# When a user is accepted a rule is inserted above this one to match them and RETURN

# Traffic which has been marked 99 and is headed for TCP/80 should be redirected to the localhost (captive portal web server)
$IPTABLES -t nat -A PREROUTING -m mark --mark 99 -p tcp --dport 80 -j DNAT --to-destination 192.168.21.1
# All other traffic which is marked 99 is just dropped
$IPTABLES -t filter -A FORWARD -m mark --mark 99 -j DROP

# Allow incomming traffic that is headed for our captive portal web server or local DNS server
$IPTABLES -t filter -A INPUT -p tcp --dport 80 -j ACCEPT
$IPTABLES -t filter -A INPUT -p udp --dport 53 -j ACCEPT
# But reject anything else that is comming from an unrecognized user
$IPTABLES -t filter -A INPUT -m mark --mark 99 -j DROP

