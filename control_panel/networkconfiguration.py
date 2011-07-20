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
    # Class attributes which make up a network interface.  By default they are
    # blank, but will be populated from the network.sqlite database if the
    # user picks an already-configured interface.
    interface = ''
    channel = ''
    essid = ''
    ip_address = ''
    netmask = ''

    # Pretends to be index.html.
    def index(self):
        # Reinitialize this class' attributes in case the user wants to
        # reconfigure an interface.  It'll be used to set the default values
        # of the HTML fields.
        self.reinitialize_attributes()

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

    # Used to reset this class' attributes to a known state.
    def reinitialize_attributes(self):
        self.interface = ''
        self.channel = ''
        self.essid = ''
        self.ip_address = ''
        self.netmask = ''

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

    # Allows the user to enter the ESSID and wireless channel of their node.
    # Takes as an argument the value of the 'interface' variable defined in
    # the form on /network/index.html.
    def wireless(self, interface=None):
        # Store the name of the network interface chosen by the user in the
        # object's attribute set.
        self.interface = interface

        # The forms in the HTML template do everything here, as well.  This
        # method only accepts input for use later.
        page = templatelookup.get_template("/network/wireless.html")
        return page.render(title = "Configure wireless for Byzantium node.",
                           purpose_of_page = "Set wireless network parameters.",
                           interface = self.interface)
    wireless.exposed = True

    # Implements step one of the interface configuration process: picking an
    # IP address.
    def tcpip(self, essid=None, channel=None):
        # Store the ESSID and wireless channel in the class' attribute set if
        # they were passed as args.
        if essid:
            self.essid = essid
        if channel:
            self.channel = channel

        # The forms in the HTML template do everything here.  This is probably
        # not the right way to do this, but it can always be fixed later.
        page = templatelookup.get_template("/network/tcpip.html")
        return page.render(title = "Set IP address for Byzantium node.",
                           purpose_of_page = "Set IP address on interface.",
                           interface = self.interface, essid = self.essid,
                           channel = self.channel)
    tcpip.exposed = True

    # Method turns the user's input from /network/step1.html into an IP address
    # and netmask.  **ip_info is used to pass the values that are then broken
    # up and assembled correctly.
    def enter_ip(self, **ip_info):
        # Split ip_info into an IP address and a netmask.  This is ugly, but
        # you can't pass multiple keyword argument lists to a method.  Then
        # store them in the class' attribute set.
        self.ip_address = ip_info['octet_one'] + "." 
        self.ip_address = self.ip_address + ip_info['octet_two'] + "."
        self.ip_address = self.ip_address + ip_info['octet_three'] + "."
        self.ip_address = self.ip_address + ip_info['octet_four']

        self.netmask = ip_info['netmask_one'] + "."
        self.netmask = self.netmask + ip_info['netmask_two'] + "."
        self.netmask = self.netmask + ip_info['netmask_three'] + "."
        self.netmask = self.netmask + ip_info['netmask_four']

        # Run the "Are you sure?" page through the Mako template interpeter.
        try:
            page = templatelookup.get_template("/network/confirm.html")
            return page.render(title = "Confirm network address for interface.",
                               purpose_of_page = "Confirm IP address.",
                               interface = self.interface,
                               ip_address = self.ip_address,
                               netmask = self.netmask)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    enter_ip.exposed = True

    # If this method is called, randomly choose an RFC 1918 IP address to
    # apply to the network interface.
    def random_ip(self):
        # Choose the RFC 1918 network the interface will be configured for.
        import random
        random.seed()
        rfc1918 = random.randint(1, 3)
        if rfc1918 == 1:
            ip_address = '10.'
            ip_address = ip_address + str(random.randint(1, 254)) + '.'
            ip_address = ip_address + str(random.randint(1, 254)) + '.'
            ip_address = ip_address + str(random.randint(1, 254))
            netmask = '255.0.0.0'
        elif rfc1918 == 2:
            ip_address = '172.16.'
            ip_address = ip_address + str(random.randint(1, 254)) + '.'
            ip_address = ip_address + str(random.randint(1, 254))
            netmask = '255.255.0.0'
        else:
            ip_address = '192.168.'
            ip_address = ip_address + str(random.randint(1, 254)) + '.'
            ip_address = ip_address + str(random.randint(1, 254))
            netmask = '255.255.255.0'

        # Store the randomly generated network configuration information in
        # the class' attribute set.
        self.ip_address = ip_address
        self.netmask = netmask

        # Run the "Are you sure?" page through the Mako template interpeter.
        try:
            page = templatelookup.get_template("/network/confirm.html")
            return page.render(title = "Confirm network address for interface.",
                               purpose_of_page = "Confirm IP address.",
                               interface = self.interface,
                               ip_address = self.ip_address,
                               netmask = self.netmask)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    random_ip.exposed = True

    # Configure the network interface.
    def set_ip(self):
        # Turn the network interface up.  Without that, nothing else will work
        # reliably.
        command = '/sbin/ifconfig ' + self.interface + ' up'
        output = os.popen(command)

        # If they're defined, set the wireless channel and ESSID.
        if self.essid:
            command = '/sbin/iwconfig '+self.interface+' essid '+self.essid
            output = os.popen(command)
        if self.channel:
            command = '/sbin/iwconfig ' + self.interface + ' channel'
            command = command + self.essid
            output = os.popen(command)

        # Call ifconfig and pass the network configuration information.
        command = '/sbin/ifconfig ' + self.interface + ' ' + self.ip_address
        command = command + ' netmask ' + self.netmask
        output = os.popen(command)

        try:
            page = templatelookup.get_template("/network/done.html")
            return page.render(title = "Network interface configured.",
                               purpose_of_page = "Configured!",
                               interface = self.interface,
                               ip_address = self.ip_address,
                               netmask = self.netmask)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    set_ip.exposed = True
