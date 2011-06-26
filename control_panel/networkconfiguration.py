# Import modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
import os

from control_panel import *

# Classes.
# This class allows the user to configure the network interfaces of their node.
# Note that this does not configure mesh functionality.
class NetworkConfiguration(object):
    # Pretends to be index.html.
    def index(self):
        # Get a list of all network interfaces on the node (sans loopback).
        interfaces = self.enumerate_network_interfaces()

        # Split the network interface list into two other lists, one for Ethernet
        # and one for wireless.
        ethernet = []
        wireless = []
        for i in interfaces:
            if i.startswith('eth'):
                ethernet.append(i)
            else:
                wireless.append(i)

        # Build tables containing the interfaces extant.
        wireless_interfaces = ""
        ethernet_interfaces = ""
        for i in wireless:
            wireless_interfaces = wireless_interfaces + "<td>" + i + "</td>"
        for i in ethernet:
            ethernet_interfaces = ethernet_interfaces + "<td>" + i + "</td>"

        # Render the HTML page.
        page = templatelookup.get_template("/network/index.html")
        return page.render(title = "Byzantium Node Network Interfaces",
                           wireless_interfaces = wireless_interfaces,
                           ethernet_interfaces = ethernet_interfaces)
    index.exposed = True

    # Helper method to enumerate all of the network interfaces on a node.
    def enumerate_network_interfaces(self):
        interfaces = []

        # Enumerate network interfaces.
        procnetdev = open("/proc/net/dev", "r")

        # Smoke test by trying to read the first two lines from the pseudofile
        # (which comprises the column headers.  If this fails, make it default
        # to 'lo' (loopback).
        headers = procnetdev.readline()
        headers = procnetdev.readline()
        if not headers:
            procnetdev.close()
            return 'lo'

        # Begin parsing the contents of /proc/net/dev and extracting the names of
        # the interfaces.
        for line in procnetdev:
            interface = line.split()[0]
            interface = interface.strip()
            interface = interface.strip(':')
            interfaces.append(interface)

        # Remove the loopback interface because that's our failure case.
        interfaces.remove('lo')

        return interfaces
