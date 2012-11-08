#!/usr/bin/python

# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

# byzantium_configd.py - A (relatively) simple daemon that automatically
# configures wireless interfaces on a Byzantium node.  No user interaction is
# required.
# This is a hack, but it's also an emergency.

# Imports
import logging
import os
import os.path
import random
import re
import subprocess
import sys
import time

# Layer 1 and 2 defaults.
channel = '5'
frequency = 2.432
bssid = '02:CA:FF:EE:BA:BE'
essid = 'Sandy'

# Layer 3 defaults.
mesh_netmask = '255.255.0.0'
client_netmask = '255.255.255.0'
commotion_network = '5.0.0.0'
commotion_netmask = '255.0.0.0'

# Paths to generated configuration files.
hostsmesh = '/etc/hosts.mesh'
hostsmeshbak = '/etc/hosts.mesh.bak'
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
    logging.debug("No wireless interfaces found.  Terminating.")
    sys.exit(1)

# For each network interface's pseudofile in /sys, test to see if a
# subdirectory 'wireless/' exists.  Use this to sort the list of
# interfaces into wired and wireless.
for i in interfaces:
    logging.debug("Adding network interface %s.", i)
    if os.path.isdir("/sys/class/net/%s/wireless" % i):
        wireless.append(i)

# Find unused IP addresses to configure this node's interfaces with.
for interface in wireless:
    # Set wireless parameters on the interface.  Do this by going into a loop
    # that tests the configuration for correctness and starts the procedure
    # over if it didn't take the first time.
    break_flag = False
    while True:
        logging.debug("Configuring wireless settings...")
        command = ['/sbin/iwconfig', interface, 'mode', 'ad-hoc']
        subprocess.Popen(command)
        time.sleep(1)
        command = ['/sbin/iwconfig', interface, 'essid', essid]
        subprocess.Popen(command)
        time.sleep(1)
        command = ['/sbin/iwconfig', interface, 'ap', bssid]
        subprocess.Popen(command)
        time.sleep(1)
        command = ['/sbin/iwconfig', interface, 'channel', channel]
        subprocess.Popen(command)
        time.sleep(1)

        # Capture the interface's current settings.
        command = ['/sbin/iwconfig', interface]
        configuration = ''
        output = subprocess.Popen(command, stdout=subprocess.PIPE).stdout
        configuration = output.readlines()

        # Test the interface's current configuration.  Go back to the top of
        # the loop and try again if it's not what we expect.
        for line in configuration:
            if re.search("Mode|ESSID|Cell|Frequency", line):
                line = line.split(' ')
            else:
                continue

            # Ad-hoc mode?
            if 'Mode' in line:
                mode = line[0].split(':')[1]
                if mode != 'Ad-Hoc':
                    break_flag = True
                    break

            # Correct ESSID?
            if 'ESSID' in line:
                ESSID = line[-1].split(':')[1]
                if ESSID != essid:
                    break_flag = True
                    break

            # Correct BSSID?
            if 'Cell' in line:
                BSSID = line[-1]
                if BSSID != bssid:
                    break_flag = True
                    break

            # Correct frequency (because iwconfig doesn't report channels)?
            if 'Frequency' in line:
                FREQUENCY = line[2].split(':')[1]
                if FREQUENCY != frequency:
                    break_flag = True
                    break

        # "Victory is mine!"
        #     --Stewie, _Family Guy_
        if not(break_flag):
            logging.debug("Wireless configured.")
            break

    # Turn up the interface.
    command = ['/sbin/ifconfig', interface, 'up']
    subprocess.Popen(command)
    time.sleep(5)

    # Start with the mesh interface.
    ip_in_use = 1
    while ip_in_use:
        # Generate a pseudorandom IP address for the mesh interface.
        addr = '192.168.'
        addr = addr + str(random.randint(0, 254)) + '.'
        addr = addr + str(random.randint(1, 254))

        # Use arping to see if anyone's claimed it.
        arping = ['/sbin/arping', '-c 5', '-D', '-f', '-q', '-I', interface,
                  addr]
        logging.debug("Finding an IP for mesh interface...")
        ip_in_use = subprocess.call(arping)

        # If the IP isn't in use, ip_in_use==0 so we bounce out of the loop.
        # We lose nothing by saving the address anyway.
        mesh_ip = addr
    logging.debug("Mesh address: %s" % mesh_ip)

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
        logging.debug("Finding an IP for client interface...")
        ip_in_use = subprocess.call(arping)

        # If the IP isn't in use, ip_in_use==0 so we bounce out of the loop.
        # We lose nothing by saving the address anyway.
        client_ip = addr
    logging.debug("Client address: %s" % client_ip)

    # Configure the mesh interface.
    logging.debug("Configuring mesh interface...")
    command = ['/sbin/ifconfig', interface, mesh_ip, 'netmask', mesh_netmask,
               'up']
    subprocess.Popen(command)
    time.sleep(5)

    # Configure the client interface.
    logging.debug("Configuring client interface...")
    client_interface = interface + ':1'
    command = ['/sbin/ifconfig', client_interface, client_ip, 'up']
    subprocess.Popen(command)
    time.sleep(5)

    # Add a route for any Commotion nodes nearby.
    logging.debug("Adding Commotion route...")
    command = ['/sbin/route', 'add', '-net', commotion_network, 'netmask',
               commotion_netmask, 'dev', interface]

    # Start the captive portal daemon on that interface.
    captive_portal_daemon = ['/usr/local/sbin/captive_portal.py', '-i',
                             interface, '-a', client_ip]
    captive_portal_return = 0
    captive_portal_return = subprocess.Popen(captive_portal_daemon)
    time.sleep(5)

# Build a string which can be used as a template for an /etc/hosts style file.
(octet_one, octet_two, octet_three) = client_ip.split('.')
prefix = octet_one + '.' + octet_two + '.' + octet_three + '.'

# Make an /etc/hosts.mesh file, which will be used by dnsmasq to resolve its
# mesh clients.
hosts = open(hosts_file, "w")
line = prefix + str('1') + '\tbyzantium.byzantium.mesh\n'
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
subprocess.Popen(['/etc/rc.d/rc.dnsmasq', 'restart'])

# Start olsrd.
olsrd_command = ['/etc/rc.d/rc.olsrd', 'start']
subprocess.Popen(olsrd_command)
time.sleep(5)

# Fin.
sys.exit(0)
