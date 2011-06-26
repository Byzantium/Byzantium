#!/usr/bin/env python

from networkmanager.networkmanager import NetworkManager

# Connect to the NetworkManager daemon.
nm = NetworkManager()

# Enumerate devices managed by NetworkManager.
for d in nm.GetDevices():
    print "Interface: %s" % d['Interface']
    print "IPv4 Address: %s" % d['Ip4Address']
    print "Interface state: %s" % d['State']
    print "Interface state: %d" % d['State']
    print "Device Type: %s" % d['DeviceType']
    print "Device Type: %d" % d['DeviceType']
    print

