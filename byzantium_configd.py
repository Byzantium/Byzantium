#!/usr/bin/python
# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

# Copyright (C) 2013 Project Byzantium
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or any later version.

# byzantium_configd.py - A (relatively) simple daemon that automatically
# configures wireless interfaces on a Byzantium node.  No user interaction is
# required.  Much of this code was taken from the control panel, it's just
# been assembled into a more compact form.  The default network settings are
# the same ones used by Commotion Wireless, so this means that our respective
# projects are now seamlessly interoperable.  For the record, we tested this
# in Red Hook in November of 2012 and it works.

# This utility is less of a hack, but it's far from perfect.

# By: The Doctor [412/724/301/703] [ZS|Media] <drwho at virtadpt dot net>
# License: GPLv3

# Imports
import os
import os.path
import random
import re
import subprocess
import sys
import time

# Layer 1 and 2 defaults.  Note that ESSIDs are for logical separation of
# wireless networks and not signal separation.  It's the BSSIDs that count.
channel = '5'
frequency = '2.432'
bssid = '02:CA:FF:EE:BA:BE'
essid = 'Byzantium'

# Layer 3 defaults.
mesh_netmask = '255.255.0.0'
client_netmask = '255.255.255.0'
commotion_network = '5.0.0.0'
commotion_netmask = '255.0.0.0'

# Paths to generated configuration files.
hostsmesh = '/etc/hosts.mesh'
dnsmasq_include_file = '/etc/dnsmasq.conf.include'

# Global variables.
wireless = []
mesh_ip = ''
client_ip = ''

# Initialize the randomizer.
random.seed()

# Enumerate all network interfaces and pick out only the wireless ones.
# Ignore the rest.
interfaces = os.listdir('/sys/class/net')

# Remove the loopback interface because that's our failure case.
if 'lo' in interfaces:
    interfaces.remove('lo')
if not interfaces:
    print "ERROR: No wireless interfaces found.  Terminating."
    sys.exit(1)

# For each network interface's pseudofile in /sys, test to see if a
# subdirectory 'wireless/' exists.  Use this to sort the list of
# interfaces into wired and wireless.
for i in interfaces:
    if os.path.isdir("/sys/class/net/%s/wireless" % i):
        wireless.append(i)

# Find unused IP addresses to configure this node's interfaces with.
if len(wireless):
    interface = wireless[0]
    print "Attempting to configure interface %s." % interface

    # Turn down the interface.
    command = ['/sbin/ifconfig', interface, 'down']
    subprocess.Popen(command)
    time.sleep(5)

    # Set wireless parameters on the interface.  Do this by going into a loop
    # that tests the configuration for correctness and starts the procedure
    # over if it didn't take the first time.
    # We wait a few seconds between iwconfig operations because some wireless
    # chipsets are pokey (coughAtheroscough) and silently reset themselves if
    # you try to configure them too rapidly, meaning that they drop out of
    # ad-hoc mode.
    success = False
    for try_num in range(3):
        print "Attempting to configure the wireless interface. Try:", try_num
        # Configure the wireless chipset.
        command = ['/sbin/iwconfig', interface, 'mode', 'ad-hoc']
        subprocess.Popen(command)
        time.sleep(5)
        command = ['/sbin/iwconfig', interface, 'essid', essid]
        subprocess.Popen(command)
        time.sleep(5)
        command = ['/sbin/iwconfig', interface, 'ap', bssid]
        subprocess.Popen(command)
        time.sleep(5)
        command = ['/sbin/iwconfig', interface, 'channel', channel]
        subprocess.Popen(command)
        time.sleep(5)

        # Capture the interface's current settings.
        command = ['/sbin/iwconfig', interface]
        configuration = ''
        output = subprocess.Popen(command, stdout=subprocess.PIPE).stdout
        configuration = output.readlines()

        # Test the interface's current configuration.  Go back to the top of
        # the configuration loop and try again if it's not what we expect.
        Mode = False
        Essid = False
        Bssid = False
        Frequency = False
        for line in configuration:
            # Ad-hoc mode?
            match = re.search('Mode:([\w-]+)', line)
            if match and match.group(1) == 'Ad-Hoc':
                print "Mode is correct."
                Mode = True

            # Correct ESSID?
            match = re.search('ESSID:"([\w]+)"', line)
            if match and match.group(1) == essid:
                print "ESSID is correct."
                Essid = True

            # Correct BSSID?
            match = re.search('Cell: (([\dA-F][\dA-F]:){5}[\dA-F][\dA-F])', line)
            if match and match.group(1) == bssid:
                print "BSSID is correct."
                Bssid = True

            # Correct frequency (because iwconfig doesn't report channels)?
            match = re.search('Frequency:([\d.]+)', line)
            if match and match.group(1) == frequency:
                print "Channel is correct."
                Frequency = True

        # "Victory is mine!"
        #     --Stewie, _Family Guy_
        if Mode and Essid and Bssid and Frequency:
            success = True
            break
        else:
            print "Failed to setup the interface properly. Retrying..."
    if not success:
        sys.exit(1)

    # Turn up the interface.
    command = ['/sbin/ifconfig', interface, 'up']
    subprocess.Popen(command)
    time.sleep(5)

    # Start with the mesh interface.
    ip_in_use = 1
    while ip_in_use:
        # Generate a pseudorandom IP address for the mesh interface.
        addr = '192.168.'
        addr = addr + str(random.randint(0, 255)) + '.'
        addr = addr + str(random.randint(1, 254))

        # Use arping to see if anyone's claimed it.
        arping = ['/sbin/arping', '-c 5', '-D', '-f', '-q', '-I', interface,
                  addr]
        ip_in_use = subprocess.call(arping)

        # If the IP isn't in use, ip_in_use == 0 so we bounce out of the loop.
        # We lose nothing by saving the address anyway.
        mesh_ip = addr
    print "Mesh interface address: %s" % mesh_ip

    # Now configure the client interface.
    ip_in_use = 1
    while ip_in_use:
        # Generate a pseudorandom IP address for the client interface.
        addr = '10.'
        addr = addr + str(random.randint(0, 254)) + '.'
        addr = addr + str(random.randint(0, 254)) + '.1'

        # Use arping to see if anyone's claimed it.
        arping = ['/sbin/arping', '-c 5', '-D', '-f', '-q', '-I', interface,
                  addr]
        ip_in_use = subprocess.call(arping)

        # If the IP isn't in use, ip_in_use==0 so we bounce out of the loop.
        # We lose nothing by saving the address anyway.
        client_ip = addr
    print "Client interface address: %s" % client_ip

    # Configure the mesh interface.
    command = ['/sbin/ifconfig', interface, mesh_ip, 'netmask', mesh_netmask,
               'up']
    subprocess.Popen(command)
    time.sleep(5)
    print "Mesh interface %s configured." % interface

    # Configure the client interface.
    client_interface = interface + ':1'
    command = ['/sbin/ifconfig', client_interface, client_ip, 'up']
    subprocess.Popen(command)
    time.sleep(5)
    print "Client interface %s configured." % client_interface

    # Add a route for any Commotion nodes nearby.
    print "Adding Commotion route..."
    command = ['/sbin/route', 'add', '-net', commotion_network, 'netmask',
               commotion_netmask, 'dev', interface]
    commotion_route_return = subprocess.Popen(command)

    # Start the captive portal daemon on that interface.
    captive_portal_daemon = ['/usr/local/sbin/captive_portal.py', '-i',
                             interface, '-a', client_ip]
    captive_portal_return = 0
    captive_portal_return = subprocess.Popen(captive_portal_daemon)
    time.sleep(5)
    print "Started captive portal daemon."
else:
    # There is no wireless interface.  Don't even bother continuing.
    print "ERROR: I wasn't able to find a wireless interface to configure.  ABENDing."
    sys.exit(1)

# Build a string which can be used as a template for an /etc/hosts style file.
(octet_one, octet_two, octet_three, _) = client_ip.split('.')
prefix = octet_one + '.' + octet_two + '.' + octet_three + '.'

# Make an /etc/hosts.mesh file, which will be used by dnsmasq to resolve its
# mesh clients.
hosts = open(hostsmesh, "w")
line = prefix + str('1') + '\tbyzantium.mesh\n'
hosts.write(line)
for i in range(2, 254):
    line = prefix + str(i) + '\tclient-' + prefix + str(i) + '.byzantium.mesh\n'
    hosts.write(line)
hosts.close()

# Generate an /etc/dnsmasq.conf.include file.
(octet_one, octet_two, octet_three, _) = client_ip.split('.')
prefix = octet_one + '.' + octet_two + '.' + octet_three + '.'
start = prefix + str('2')
end = prefix + str('254')
dhcp_range = 'dhcp-range=' + start + ',' + end + ',5m\n'
include_file = open(dnsmasq_include_file, 'w')
include_file.write(dhcp_range)
include_file.close()

# Start dnsmasq.
print "Starting dnsmasq."
subprocess.Popen(['/etc/rc.d/rc.dnsmasq', 'restart'])

# Start olsrd.
olsrd_command = ['/usr/sbin/olsrd', '-i']

#for i in wireless:

if len(wireless):
    olsrd_command.append(wireless[0])

print "Starting routing daemon."
subprocess.Popen(olsrd_command)
time.sleep(5)

# Fin.
sys.exit(0)
