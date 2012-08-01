# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

# gateways.py - Implements the network gateway configuration subsystem of the
#    Byzantium control panel.

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# TODO:
# - In Gateways.activate(), add some code before running iptables to make sure
#   that NAT rules don't exist already.
#   iptables -t nat -n -L | grep MASQUERADE
# - Add code to configure encryption on wireless gateways.
# - Make it possible to specify IP configuration information on a wireless
#   uplink.
# - Add code to update_network_interfaces() to delete interfaces from the
#   database if they don't exist anymore.

import logging
import subprocess
import time

import _utils
import networkconfiguration
import models.mesh
import models.state
import models.wired_network
import models.wireless_network


def audit_procnetdev(procnetdev):
    if procnetdev:
        logging.debug("Successfully opened /proc/net/dev.")
    else:
        # Note: This means that we use the contents of the database.
        logging.debug("Warning: Unable to open /proc/net/dev.")
        return False

    # Smoke test by trying to read the first two lines from the pseudofile
    # (which comprises the column headers.  If this fails, just return
    # because we're falling back on the existing contents of the database.
    headers = procnetdev.readline()
    headers = procnetdev.readline()
    if not headers:
        logging.debug("Smoke test of /proc/net/dev read failed.")
        procnetdev.close()
        return False
    return True
    
    
def build_interfaces(interfaces, procnetdev):
    for line in procnetdev:
        interface = line.split()[0]
        interface = interface.strip()
        interface = interface.strip(':')
        interfaces.append(interface)
    return interfaces


def check_for_wired_interface(interface, persistance):
    results, _ = persistance.exists('wired', {'interface': interface})
    if not results:
        logging.debug("Interface %s isn't a known wired interface.  Checking wireless interfaces...",
                      interface)
        return ''
    else:
        logging.debug("Interface %s is a known wired interface.", interface)
        return 'wired'


def check_for_wireless_interface(interface, persistance):
    results, _ = persistance.exists('wireless', {'mesh_interface': interface})
    # If it's not in there, either, figure out which table it
    # has to go in.
    if not results:
        logging.debug("%s isn't a known wireless interface, either.  Figuring out where it has to go...", 
                      interface)
    else:
        logging.debug("%s is a known wireless interface.", interface)
        return 'wireless'


def check_wireless_table(interface):
    table = False
    procnetwireless = open('/proc/net/wireless', 'r')
    procnetwireless.readline()
    procnetwireless.readline()
    for line in procnetwireless:
        if interface in line:
            logging.debug("Goes in wireless table.")
            table = True
    procnetwireless.close()
    return table


# Classes.
# This class allows the user to turn a configured network interface on their
# node into a gateway from the mesh to another network (usually the global Net).
class Gateways(object):

    def __init__(self, templatelookup, test):
        self.templatelookup = templatelookup
        self.test = test

        netconfdb, meshconfdb = _utils.set_confdbs(self.test)
        self.network_state = models.state.NetworkState(netconfdb)
        self.mesh_state = models.state.MeshState(meshconfdb)

        # Configuration information for the network device chosen by the user to
        # act as the uplink.
        self.interface = ''
        self.channel = 0
        self.essid = ''

        # Used for sanity checking user input.
        self.frequency = 0
        
        # Class attributes which make up a network interface.  By default they are
        # blank, but will be populated from the network.sqlite database if the
        # user picks an already-configured interface.
        self.mesh_interface = ''
        self.mesh_ip = ''
        self.client_interface = ''
        self.client_ip = ''

        # Set the netmasks aside so everything doesn't run together.
        self.mesh_netmask = '255.255.0.0'
        self.client_netmask = '255.255.255.0'

        # Attributes for flat files that this object maintains for the client side
        # of the network subsystem.
        self.hosts_file = '/etc/hosts.mesh'
        self.dnsmasq_include_file = '/etc/dnsmasq.conf.include'

    # Pretends to be index.html.
    def index(self):
        ethernet_buttons = ""
        wireless_buttons = ""

        # To find any new network interfaces, rescan the network interfaces on
        # the node.
        self.update_network_interfaces()

        results = self.network_state.list('wired', models.wired_network.WiredNetwork, {'gateway': 'no'})
        if results:
            for interface in results:
                ethernet_buttons = ethernet_buttons + "<td><input type='submit' name='interface' value='" + interface.interface + "' /></td>\n"

        # Generate a list of wireless interfaces on the node that are not
        # enabled but are known.  As before, each button gets is own button
        # in a table.
        results = self.network_state.list('wireless', models.wireless_network.WirelessNetwork, {'gateway': 'no'})
        if results:
            for interface in results:
                wireless_buttons = wireless_buttons + "<td><input type='submit' name='interface' value='" + interface.interface + "' /></td>\n"

        # Render the HTML page.
        try:
            page = self.templatelookup.get_template("/gateways/index.html")
            return page.render(title = "Network Gateway",
                               purpose_of_page = "Configure Network Gateway",
                               ethernet_buttons = ethernet_buttons,
                               wireless_buttons = wireless_buttons)
        except:
            _utils.output_error_data()
    index.exposed = True

    # Utility method to update the list of all network interfaces on a node.
    # New ones detected are added to the network.sqlite database.  Takes no
    # arguments; returns nothing (but alters the database).
    def update_network_interfaces(self):
        logging.debug("Entered Gateways.update_network_interfaces().")

        # Open the kernel's canonical list of network interfaces.
        procnetdev = open("/proc/net/dev", "r")
        if not audit_procnetdev(procnetdev):
            return

        # Begin parsing the contents of /proc/net/dev to extract the names of
        # the interfaces.
        interfaces = []
        if self.test:
            logging.debug("Pretending to harvest /proc/net/dev for network interfaces.  Actually using the contents of %s and loopback.", self.network_state.db_path)
            return
        else:
            interfaces = build_interfaces(interfaces, procnetdev)

            # Walk through the list of interfaces just generated and see if
            # each one is already in the database.  If it's not, add it.
            for interface in interfaces:

                # See if it's in the table of wired interfaces.
                found = check_for_wired_interface(interface, self.network_state)

                # If it's not in the wired table, check the wireless table.
                if not found:
                    found = check_for_wireless_interface(interface, self.network_state)

                # If it still hasn't been found, figure out where it has to go.
                if not found:
                    logging.debug("Interface %s really is new.  Figuring out where it should go.", interface)

                    # Look in /proc/net/wireless.  If it's in there, it
                    # goes in the wireless table.  Otherwise it goes in
                    # the wired table.
                    if check_wireless_table(interface):
                        models.wireless_network.WirelessNetwork(client_interface=interface + ':1', mesh_interface=interface, gateway='no',
                                     enabled='no', channel=0, essid='', persistance=self.network_state)
                    else:
                        logging.debug("Goes in wired table.")
                        models.wired_network.WiredNetwork(interface=interface, gateway='no', enabled='no',
                                     persistance=self.network_state)

        logging.debug("Leaving Gateways.enumerate_network_interfaces().")

    # Implements step two of the wired gateway configuration process: turning
    # the gateway on.  This method assumes that whichever Ethernet interface
    # chosen is already configured via DHCP through ifplugd.
    def tcpip(self, interface=None, essid=None, channel=None):
        logging.debug("Entered Gateways.tcpip().")

        # Define this variable in case wireless configuration information is
        # passed into this method.
        iwconfigs = ''

        # Test to see if the interface argument has been passed.  If it hasn't
        # then this method is being called from Gateways.wireless(), so
        # populate it from the class attribute variable.
        if interface is None:
            interface = self.interface

        # If an ESSID and channel were passed to this method, store them in
        # class attributes.
        if essid:
            self.essid = essid
            iwconfigs = '<p>Wireless network configuration:</p>\n'
            iwconfigs = iwconfigs + '<p>ESSID: ' + essid + '</p>\n'
        if channel:
            self.channel = channel
            iwconfigs = iwconfigs + '<p>Channel: ' + channel + '</p>\n'

        # Run the "Are you sure?" page through the template interpeter.
        try:
            page = self.templatelookup.get_template("/gateways/confirm.html")
            return page.render(title = "Enable gateway?",
                               purpose_of_page = "Confirm gateway mode.",
                               interface = interface, iwconfigs = iwconfigs)
        except:
            _utils.output_error_data()
    tcpip.exposed = True

    # Allows the user to enter the ESSID and wireless channel of the wireless
    # network interface that will act as an uplink to another Network for the
    # mesh.  Takes as an argument the value of the 'interface' variable passed
    # from the form on /gateways/index.html.
    def wireless(self, interface=None):
        # Store the name of the interface in question in a class attribute for
        # use later.
        self.interface = interface

        # Set up variables to hold the ESSID and channel of the wireless
        # uplink.
        channel = 0
        essid = ''

        channel, essid, warning = _utils.check_for_configured_interface(self.network_state, interface, channel, essid)

        # The forms in the HTML template do everything here, as well.  This
        # method only accepts input for use later.
        try:
            page = self.templatelookup.get_template("/gateways/wireless.html")
            return page.render(title = "Configure wireless uplink.",
                           purpose_of_page = "Set wireless uplink parameters.",
                           warning = warning, interface = interface,
                           channel = channel, essid = essid)
        except:
            _utils.output_error_data()
    wireless.exposed = True

    def _get_mesh_interfaces(self, interface):
        results = self.mesh_state.list('meshes', models.mesh.Mesh, {'enabled': 'yes', 'protocol': 'babel'})
        return [result.interface for result in results]
        
    def _update_netconfdb(self, interface):
        
        results = self.network_state.list('wired', WiredNetwork, {'interface': interface})
        if results:
            for result in results:
                result.gateway = 'yes'
        # Otherwise, it's a wireless interface.
        else:
            self.network_state.replace({'kind': 'wireless', 'mesh_interface': interface},
                                       {'kind': 'wireless', 'mesh_interface': interface, 'gateway': 'yes'})

    # Method that does the deed of turning an interface into a gateway.  This
    def activate(self, interface=None):
        logging.debug("Entered Gateways.activate().")

        # Test to see if wireless configuration attributes are set, and if they
        # are, use iwconfig to set up the interface.
        if self.essid:
            command = ['/sbin/iwconfig', interface, 'essid', self.essid]
            logging.debug("Setting ESSID to %s.", self.essid)
            if self.test:
                logging.debug("Command to set ESSID:\n%s", ' '.join(command))
            else:
                subprocess.Popen(command)
        if self.channel:
            command = ['/sbin/iwconfig', interface, 'channel', self.channel]
            logging.debug("Setting channel %s.", self.channel)
            if self.test:
                logging.debug("Command to set channel:\n%s", ' '.join(command))
            else:
                subprocess.Popen(command)

        # If we have to configure layers 1 and 2, then it's a safe bet that we
        # should use DHCP to set up layer 3.  This is wrapped in a shell script
        # because of a timing conflict between the time dhcpcd starts, the
        # time dhcpcd gets IP configuration information (or not) and when
        # avahi-daemon is bounced.
        command = ['/usr/local/sbin/gateway.sh', interface]
        logging.debug("Preparing to configure interface %s.", interface)
        if self.test:
            logging.debug("Pretending to run gateway.sh on interface %s.", interface)
            logging.debug("Command that would be run:\n%s", ' '.join(command))
        else:
            subprocess.Popen(command)

        # See what value was returned by the script.

        # Set up a list of mesh interfaces for which babeld is already running.
        #
        # NOTE: the interfaces variable doesn't seem to ever get used anywhere :/
        #
        interfaces = self._get_mesh_interfaces(interface)

        # Update the network configuration database to reflect the fact that
        # the interface is now a gateway.  Search the table of Ethernet
        # interfaces first.
        self._update_netconfdb(interface)

        # Display the confirmation of the operation to the user.
        try:
            page = self.templatelookup.get_template("/gateways/done.html")
            return page.render(title = "Enable gateway?",
                               purpose_of_page = "Confirm gateway mode.",
                               interface = interface)
        except:
            _utils.output_error_data()
    activate.exposed = True

    # Configure the network interface.
    def set_ip(self):
        # If we've made it this far, the user's decided to (re)configure a
        # network interface.  Full steam ahead, damn the torpedoes!

        # First, take the wireless NIC offline so its mode can be changed.
        command = ['/sbin/ifconfig', self.mesh_interface, 'down']
        output = subprocess.Popen(command)
        time.sleep(5)

        # Wrap this whole process in a loop to ensure that stubborn wireless
        # interfaces are configured reliably.  The wireless NIC has to make it
        # all the way through one iteration of the loop without errors before
        # we can go on.
        while True:
            # Set the mode, ESSID and channel.
            command = ['/sbin/iwconfig', self.mesh_interface, 'mode ad-hoc']
            output = subprocess.Popen(command)
            command = ['/sbin/iwconfig', self.mesh_interface, 'essid', self.essid]
            output = subprocess.Popen(command)
            command = ['/sbin/iwconfig', self.mesh_interface, 'channel',  self.channel]
            output = subprocess.Popen(command)

            # Run iwconfig again and capture the current wireless configuration.
            command = ['/sbin/iwconfig', self.mesh_interface]
            output = subprocess.Popen(command).stdout
            configuration = output.readlines()

            # Test the interface by going through the captured text to see if
            # it's in ad-hoc mode.  If it's not, put it in ad-hoc mode and go
            # back to the top of the loop to try again.
            for line in configuration:
                if 'Mode' in line:
                    line = line.strip()
                    mode = line.split(' ')[0].split(':')[1]
                    if mode != 'Ad-Hoc':
                        continue

            # Test the ESSID to see if it's been set properly.
            for line in configuration:
                if 'ESSID' in line:
                    line = line.strip()
                    essid = line.split(' ')[-1].split(':')[1]
                    if essid != self.essid:
                        continue

            # Check the wireless channel to see if it's been set properly.
            for line in configuration:
                if 'Frequency' in line:
                    line = line.strip()
                    frequency = line.split(' ')[2].split(':')[1]
                    if frequency != self.frequency:
                        continue

            # "Victory is mine!"
            # --Stewie, _Family Guy_
            break

        # Call ifconfig and set up the network configuration information.
        command = ['/sbin/ifconfig', self.mesh_interface, self.mesh_ip,
                   'netmask', self.mesh_netmask, 'up']
        output = subprocess.Popen(command)
        time.sleep(5)

        # Add the client interface.
        command = ['/sbin/ifconfig', self.client_interface, self.client_ip, 'up']
        output = subprocess.Popen(command)

        template = ('yes', self.channel, self.essid, self.mesh_interface, self.client_interface, self.mesh_interface)
        new = dict(enabled='yes, channel=self.channel, essid=self.essid, mesh_interface=self.mesh_interface, client_interface=self.client_interface)
        old = dict(mesh_interface=self.mesh_interface)
        _utils.set_wireless_db_entry(self.network_state, old, new)

        # Send this information to the methods that write the /etc/hosts and
        # dnsmasq config files.
        networkconfiguration.make_hosts(self.hosts_file, self.test, starting_ip=self.client_ip)
        networkconfiguration.configure_dnsmasq(self.dnsmasq_include_file, self.test, starting_ip=self.client_ip)

        # Render and display the page.
        try:
            page = self.templatelookup.get_template("/network/done.html")
            return page.render(title = "Network interface configured.",
                               purpose_of_page = "Configured!",
                               interface = self.mesh_interface,
                               ip_address = self.mesh_ip,
                               netmask = self.mesh_netmask,
                               client_ip = self.client_ip,
                               client_netmask = self.client_netmask)
        except:
            _utils.output_error_data()
    set_ip.exposed = True

