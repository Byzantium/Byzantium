# Import modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
from mako.exceptions import RichTraceback
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

        # Split the network interface list into two other lists, one for
        # Ethernet and one for wireless.
        ethernet = []
        wireless = []
        for i in interfaces:
            if i.startswith('eth'):
                ethernet.append(i)
            else:
                wireless.append(i)

        # Build tables containing the interfaces extant.
        wireless_buttons = ""
        ethernet_buttons = ""
        for i in wireless:
            wireless_buttons = wireless_buttons + "<input type='submit' name='interface' value='" + i + "' />\n"

        for i in ethernet:
            ethernet_buttons = ethernet_buttons + "<input type='submit' name='interface' value='" + i + "'/>\n"

        # Render the HTML page.
        page = templatelookup.get_template("/network/index.html")
        return page.render(title = "Byzantium Node Network Interfaces",
                           purpose_of_page = "Configure Network Interfaces",
                           wireless_buttons = wireless_buttons,
                           ethernet_buttons = ethernet_buttons)
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

        # Begin parsing the contents of /proc/net/dev and extracting the names
        # of the interfaces.
        for line in procnetdev:
            interface = line.split()[0]
            interface = interface.strip()
            interface = interface.strip(':')
            interfaces.append(interface)

        # Remove the loopback interface because that's our failure case.
        interfaces.remove('lo')
        return interfaces

    # Implements step one of the interface configuration process: picking an
    # IP address.
    def step1(self, interface=None):
        # The forms in the HTML template do everything here.  This is probably
        # not the right way to do this, but it can always be fixed later.
        page = templatelookup.get_template("/network/step1.html")
        return page.render(title = "Set IP address for Byzantium node.",
                           purpose_of_page = "Set IP address on interface.",
                           interface = interface)
    step1.exposed = True

    # Method turns the user's input from /network/step1.html into an IP address
    # and netmask.  **ip_info is used to pass the values that are then broken
    # up and assembled correctly.
    def enter_ip(self, interface=None, **ip_info):
        # Split ip_info into an IP address and a netmask.  This is ugly, but
        # you can't pass multiple keyword argument lists to a method.
        ip_address = ip_info['octet_one'] + "." + ip_info['octet_two'] + "."
        ip_address = ip_address + ip_info['octet_three'] + "."
        ip_address = ip_address + ip_info['octet_four']

        netmask = ip_info['netmask_one'] + "." + ip_info['netmask_two'] + "."
        netmask = netmask + ip_info['netmask_three'] + "."
        netmask = netmask + ip_info['netmask_four']

        try:
            page = templatelookup.get_template("/network/step2.html")
            return page.render(title = "Confirm IP address for network interface.",
                               purpose_of_page = "Confirm IP address.",
                               interface = interface,
                               ip_address = ip_address, netmask = netmask)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    enter_ip.exposed = True

    # Call ifconfig to set the IP address and netmask on the network interface.
    def set_ip(self, interface=None, ip_address=None, netmask=None):
        # Call ifconfig and pass the network configuration information.
        command = '/sbin/ifconfig ' + interface + ip_address + 'netmask'
        command = command + netmask + 'up'
        output = os.popen(command)

        try:
            page = templatelookup.get_template("/network/done.html")
            return page.render(title = "Network interface configured.",
                               purpose_of_page = "Configured!",
                               interface = interface, ip_address = ip_address,
                               netmask = netmask, output = output.read())
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    set_ip.exposed = True
