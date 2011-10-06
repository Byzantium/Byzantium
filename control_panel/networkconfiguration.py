# networkconfiguration.py - Implements the network interface configuration
#    subsystem of the Byzantium control panel.

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# TODO:
# - While we're at it, refactor that method into separate methods that configure
#   just the mesh and just the client interfaces.
# - Figure out what columns in the network configuration database to index.
#   It's doubtful that a Byzantium node would have more than three interfaces
#   (not counting lo) but it's wise to plan for the future.
# - Validate the wireless channel after it's entered by the user to make sure
#   that it's not fractional or invalid.  Remember that the US has 1-12, the
#   European Union has 1-13, and Japan has 1-14.
# - Find a way to prune network interfaces that have vanished.
# - In NetworkConfiguration.make_hosts(), add code to display an error message
#   on the control panel if the /etc/hosts.mesh file can't be created.
# - Add code to .set_ip() to verify that the wireless settings took the way
#   they're supposed to.
# - Write an initscript for dnsmasq so that I don't have to send a signal
#   directly to the process.
# - Add a check to NetworkConfiguration.tcpip() to see if the mesh interface is
#   already configured, and if it is display an alternative HTML template that
#   shows the existing settings and gives a chance to abort.
# - Rework network/done.html so that it makes a little more sense.

# Import external modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
from mako.exceptions import RichTraceback
import os
import os.path
import sqlite3
import subprocess
from subprocess import call
import random
import signal
import time

# Import core control panel modules.
from control_panel import *

# Classes.
# This class allows the user to configure the network interfaces of their node.
# Note that this does not configure mesh functionality.
class NetworkConfiguration(object):
    # Location of the network.sqlite database, which holds the configuration
    # of every network interface in the node.
    netconfdb = '/var/db/controlpanel/network.sqlite'
    #netconfdb = '/home/drwho/network.sqlite'

    # Class attributes which make up a network interface.  By default they are
    # blank, but will be populated from the network.sqlite database if the
    # user picks an already-configured interface.
    mesh_interface = ''
    mesh_ip = ''
    client_interface = ''
    client_ip = ''
    channel = ''
    essid = ''
    ethernet_interface = ''
    ethernet_ip = ''

    # Set the netmasks aside so everything doesn't run together.
    mesh_netmask = '255.255.255.255'
    client_netmask = '255.255.255.0'

    # Attributes for flat files that this object maintains for the client side
    # of the network subsystem.
    hosts_file = '/etc/hosts.mesh'
    dnsmasq_include_file = '/etc/dnsmasq.conf.include'

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

        # Build tables containing the interfaces extant.  At the same time,
        # search the network configuration databases for interfaces that are
        # already configured and give them a different color.  If they're up
        # and running give them yet another color.
        connection = sqlite3.connect(self.netconfdb)
        cursor = connection.cursor()
        wireless_buttons = ""
        ethernet_buttons = ""

        # Start with wireless interfaces.
        for i in wireless:
            cursor.execute("SELECT mesh_interface, configured, enabled FROM wireless WHERE mesh_interface=?", (i, ))
            result = cursor.fetchall()

            # If the interface is not found in database, add it.
            if not len(result):
                # client_netmask, client_ip, client_interface, enabled, channel,
                # configured, essid, mesh_interface, mesh_ip, mesh_netmask
                template = ('0.0.0.0', '0.0.0.0', (i + ':1'), 'no', '0', 'no', '', i, '0.0.0.0', '0.0.0.0', )
                cursor.execute("INSERT INTO wireless VALUES (?,?,?,?,?,?,?,?,?,?);", template)
                connection.commit()
                wireless_buttons = wireless_buttons + "<input type='submit' name='interface' value='" + i + "' />\n"
                continue

            # If the interface has been configured, check to see if it's
            # running.  If it is, use a CSS hack to make it a different color.
            if (result[0][1] == "yes") and (result[0][2] == "yes"):
                wireless_buttons = wireless_buttons + "<input type='submit' name='interface' value='" + i + "' style='background-color:green' />\n"
                continue

            # If it is there test to see if it's been configured or not.  If it
            # has, use a CSS hack to make its button a different color.
            if result[0][1] == "yes":
                wireless_buttons = wireless_buttons + "<input type='submit' name='interface' value='" + i + "' style='background-color:red' />\n"
                continue

            # If all else fails, just add the button without any extra
            # decoration.
            wireless_buttons = wireless_buttons + "<input type='submit' name='interface' value='" + i + "' />\n"

        # Wired interfaces.
        for i in ethernet:
            cursor.execute("SELECT interface, configured, enabled FROM wired WHERE interface=?", (i, ))
            result = cursor.fetchall()

            # If the interface is not found in database, add it.
            if not len(result):
                # enabled, configured, gateway, interface, ipaddress, netmask
                template = ('no', 'no', 'no', i, '0.0.0.0', '0.0.0.0', )
                cursor.execute("INSERT INTO wired VALUES (?,?,?,?,?,?);", template)
                connection.commit()
                ethernet_buttons = ethernet_buttons + "<input type='submit' name='interface' value='" + i + "'/>\n"
                continue

            # If the interface has been configured, check to see if it's
            # running.  If it has, use a CSS hack to make it a different color.
            if (result[0][1] == "yes") and (result[0][2] == "yes"):
                ethernet_buttons = ethernet_buttons + "<input type='submit' name='interface' value='" + i + "' style='background-color:green' />\n"
                continue

            # If it is found test to see if it's been configured or not.  If it
            # has, use a CSS hack to make its button a different color.
            if result[0][1] == "yes":
                ethernet_buttons = ethernet_buttons + "<input type='submit' name='interface' value='" + i + "' style='background-color:red' />\n"
                continue

            # If all else fails, just add the button without any extra
            # decoration.
            ethernet_buttons = ethernet_buttons + "<input type='submit' name='interface' value='" + i + "' />\n"

        # Render the HTML page.
        cursor.close()
        try:
            page = templatelookup.get_template("/network/index.html")
            return page.render(title = "Byzantium Node Network Interfaces",
                               purpose_of_page = "Configure Network Interfaces",
                               wireless_buttons = wireless_buttons,
                               ethernet_buttons = ethernet_buttons)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    index.exposed = True

    # Used to reset this class' attributes to a known state.
    def reinitialize_attributes(self):
        self.mesh_interface = ''
        self.client_interface = ''
        self.channel = ''
        self.essid = ''
        self.mesh_ip = ''
        self.client_ip = ''

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
        # object's attribute set and then generate the name of the client
        # interface.
        self.mesh_interface = interface
        self.client_interface = interface + ':1'

        # Default settings for /network/wireless.html page.
        channel = 3
        essid = 'Byzantium'

        # If a network interface is marked as configured in the database, pull
        # its settings and insert them into the page rather than displaying the
        # defaults.
        connection = sqlite3.connect(self.netconfdb)
        cursor = connection.cursor()
        template = (interface, )
        cursor.execute("SELECT configured, channel, essid FROM wireless WHERE mesh_interface=?;", template)
        result = cursor.fetchall()
        if result and (result[0][0] == 'yes'):
            channel = result[0][1]
            essid = result[0][2]
        connection.close()
        
        # The forms in the HTML template do everything here, as well.  This
        # method only accepts input for use later.
        try:
            page = templatelookup.get_template("/network/wireless.html")
            return page.render(title = "Configure wireless for Byzantium node.",
                           purpose_of_page = "Set wireless network parameters.",
                           interface = self.mesh_interface, channel = channel,
                           essid = essid)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    wireless.exposed = True

    # Implements step two of the interface configuration process: selecting
    # IP address blocks for the mesh and client interfaces.  Draws upon class
    # attributes where they exist but pseudorandomly chooses values where it
    # needs to.
    def tcpip(self, essid=None, channel=None):
        # Store the ESSID and wireless channel in the class' attribute set if
        # they were passed as args.
        if essid:
            self.essid = essid
        if channel:
            self.channel = channel

        # MOOF MOOF MOOF: Check the database to see if the mesh interface is
        # already configured, and display an alternate page with the current
        # settings and a chance to abort if it is!  Don't forget to check to
        # see if the interface is online at the same time to keep from borking
        # the local mesh!

        # Initialize the Python environment's randomizer.
        random.seed()

        # To run arping, the interface has to be up.
        # Note that arping returns '2' if the interface isn't online!
        command = '/sbin/ifconfig ' + self.mesh_interface + ' up'
        output = os.popen(command)
        
        # Sleep five seconds to give the hardware a chance to catch up.
        time.sleep(5)
       
        print "DEBUG: Output of ifconfig is: %s" % output 
        print "DEBUG: Interface %s is now active." % self.mesh_interface

        # First pick an IP address for the mesh interface on the node.
        # Go into a loop in which pseudorandom IP addresses are chosen and
        # tested to see if they have been taken already or not.  Loop until we
        # have a winner.
        ip_in_use = 1
        while ip_in_use:
            # Pick a random IP address in 192.168/16.
            addr = '192.168.'
            addr = addr + str(random.randint(0, 254)) + '.'
            addr = addr + str(random.randint(0, 254))

            print "DEBUG: Probing for routing IP %s." % addr
            
            # Run arping to see if any node in range has claimed that IP address
            # and capture the return code.
            # Argument breakdown:
            # -c 5: Send 5 packets
            # -D: Detect specified address.  Return 1 if found, 0 if not,
            # -f: Stop after the first positive response.
            # -I Network interface to use.  Mandatory.
            arping = ['/sbin/arping', '-c 5', '-D', '-f', '-q', '-I',
                      self.mesh_interface, addr]
            ip_in_use = subprocess.call(arping)
            
            print "DEBUG: arping returned %d." % ip_in_use

            # arping returns 1 if the IP is in use, 0 if it's not.
            if not ip_in_use:
                self.mesh_ip = addr
                break

        # Next pick a distinct IP address for the client interface and its
        # netblock.
        ip_in_use = 1
        while ip_in_use:
            # Pick a random IP address in a 10/24.
            addr = '10.'
            addr = addr + str(random.randint(0, 254)) + '.'
            addr = addr + str(random.randint(0, 254)) + '.1'

            print "DEBUG: Probing for mesh IP %s." % addr
            
            # Run arping to see if any mesh node in range has claimed that IP
            # address and capture the return code.
            # Argument breakdown:
            # -c 5: Send 5 packets
            # -D: Detect specified address.  Return 1 if found, 0 if not,
            # -f: Stop after the first positive response.
            # -I Network interface to use.  Mandatory.
            arping = ['/sbin/arping', '-c 5', '-D', '-f', '-q', '-I',
                      self.mesh_interface, addr]
            ip_in_use = subprocess.call(arping)

            print "DEBUG: arping returned %d." % ip_in_use

            # arping returns 1 if the IP is in use, 0 if it's not.
            if not ip_in_use:
                self.client_ip = addr
                break

        # Deactivate the interface again.
        command = '/sbin/ifconfig ' + self.mesh_interface + ' down'
        output = os.popen(command)
        
        print "DEBUG: Output of ifconfig is: %s" % output 
        print "DEBUG: Deactivating interface %s." % self.mesh_interface

        # Store this information in the SQLite network configuration database.
        connection = sqlite3.connect(self.netconfdb)
        cursor = connection.cursor()
        template = ('yes', self.essid, self.channel, self.mesh_ip, self.mesh_netmask, self.client_interface, self.client_ip, self.client_netmask, self.mesh_interface, )
        cursor.execute("UPDATE wireless SET configured=?, essid=?, channel=?, mesh_ip=?, mesh_netmask=?, client_interface=?, client_ip=?, client_netmask=? WHERE mesh_interface=?;", template)
        connection.commit()
        connection.close()

        # Send this information to the methods that write the /etc/hosts and
        # dnsmasq config files.
        self.make_hosts(self.client_ip)
        self.configure_dnsmasq(self.client_ip)

        # Run the "Are you sure?" page through the template interpeter.
        try:
            page = templatelookup.get_template("/network/confirm.html")
            return page.render(title = "Confirm network address for interface.",
                               purpose_of_page = "Confirm IP configuration.",
                               interface = self.mesh_interface,
                               mesh_ip = self.mesh_ip,
                               mesh_netmask = self.mesh_netmask,
                               client_ip = self.client_ip,
                               client_netmask = self.client_netmask)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    tcpip.exposed = True

    # Method that generates an /etc/hosts.mesh file for the node for dnsmasq.
    # Takes three args, the first and last IP address of the netblock.  Returns
    # nothing.
    def make_hosts(self, starting_ip=None):
        # See if the /etc/hosts.mesh backup file exists.  If it does, delete it.
        old_hosts_file = self.hosts_file + '.bak'
        if os.path.exists(old_hosts_file):
            os.remove(old_hosts_file)

        # Back up the old hosts.mesh file.
        if os.path.exists(self.hosts_file):
            os.rename(self.hosts_file, old_hosts_file)

        # We can make a few assumptions given only the starting IP address of
        # the client IP block.  Each node only has a /24 netblock for clients,
        # so we only have to generate 254 entries for that file (.2-254).
        # First, split the last octet off of the IP address passed to this
        # method.
        (octet_one, octet_two, octet_three, octet_four) = starting_ip.split('.')
        prefix = octet_one + '.' + octet_two + '.' + octet_three + '.'

        # Generate the contents of the new hosts.mesh file.
        hosts = open(self.hosts_file, "w")
        for i in range(2, 255):
            line = prefix + str(i) + '\tclient-' + prefix + str(i) + '.byzantium.mesh\n'
            hosts.write(line)
        hosts.close()

        # Test for successful generation.
        if not os.path.exists(self.hosts_file):
            # Set an error message and put the old file back.
            # MOOF MOOF MOOF - Error message goes here.
            os.rename(old_hosts_file, self.hosts_file)
        return

    # Generates an /etc/dnsmasq.conf.include file for the node.  Takes two
    # args, the starting IP address.
    def configure_dnsmasq(self, starting_ip=None):
        # First, split the last octet off of the IP address passed into this
        # method.
        (octet_one, octet_two, octet_three, octet_four) = starting_ip.split('.')
        prefix = octet_one + '.' + octet_two + '.' + octet_three + '.'
        start = prefix + str('2')
        end = prefix + str('254')

        # Use that to generate the line for the config file.
        # dhcp-range=<starting IP>,<ending IP>,<length of lease>
        directive = 'dhcp-range=' + start + ',' + end + ',5m\n'

        # If an include file already exists, move it out of the way.
        oldfile = self.dnsmasq_include_file + '.bak'
        if os.path.exists(oldfile):
            os.remove(oldfile)

        # Back up the old dnsmasq.conf.include file.
        if os.path.exists(self.dnsmasq_include_file):
            os.rename(self.dnsmasq_include_file, oldfile)

        # Generate the new include file.
        file = open(self.dnsmasq_include_file, 'w')
        file.write(directive)
        file.close()

        # Test for success.  If so, HUP dnsmasq.  If not, move the old file
        # back into place and throw an error.
        if os.path.exists(self.dnsmasq_include_file):
            file = open('/var/run/dnsmasq.pid', 'r')
            pid = file.readline()
            file.close()

            # If no dnsmasq PID was found, start the server.
            if not pid:
                output = subprocess.Popen(['/usr/sbin/dnsmasq'])
            else:
                os.kill(int(pid), signal.SIGHUP)

        else:
            # Set an error message and put the old file back.
            # MOOF MOOF MOOF - Error message goes here.
            os.rename(oldfile, self.dnsmasq_include_file)
            return

    # Configure the network interface.
    def set_ip(self):
        # if the network interface in question is a wireless interface (the
        # wireless class attributes won't be set under any other circumstances),
        # put the interface into ad-hoc mode.
        if self.essid:
            # First, take the wireless NIC offline so its mode can be changed.
            command = '/sbin/ifconfig ' + self.mesh_interface + ' down'
            output = os.popen(command)

            # Set the mode, ESSID and channel.
            command = '/sbin/iwconfig ' + self.mesh_interface + ' mode ad-hoc'
            output = os.popen(command)
            command = '/sbin/iwconfig ' + self.mesh_interface + ' essid ' + self.essid
            output = os.popen(command)
            command = '/sbin/iwconfig ' + self.mesh_interface + ' channel ' + self.channel
            output = os.popen(command)

        # Call ifconfig and set up the network configuration information.
        command = '/sbin/ifconfig ' + self.mesh_interface + ' ' + self.mesh_ip
        command = command + ' netmask ' + self.mesh_netmask
        output = os.popen(command)
        command = '/sbin/ifconfig ' + self.mesh_interface + ' up'
        output = os.popen(command)
        time.sleep(5)

        # Add the client interface.
        command = '/sbin/ifconfig ' + self.client_interface + ' ' + self.client_ip + ' up'
        output = os.popen(command)

        # Commit the interface's configuration to the database.
        connection = sqlite3.connect(self.netconfdb)
        cursor = connection.cursor()

        # Because wireless and wired interfaces are in separate tables, we need
        # different queries to update the tables.  Start with wireless.
        if self.essid:
            template = ('yes', self.channel, 'yes', self.essid, self.mesh_interface, self.mesh_ip, self.mesh_netmask, self.client_interface, self.client_ip, self.client_netmask, self.mesh_interface, )
            cursor.execute("UPDATE wireless SET enabled=?, channel=?, configured=?, essid=?, mesh_interface=?, mesh_ip=?, mesh_netmask=?, client_interface=?, client_ip=?, client_netmask=? WHERE mesh_interface=?;", template)

        # Update query for wired interfaces.
        else:
            template = ('yes', 'yes', 'no', self.interface, self.ip_address,
                        self.netmask, self.interface, )
            cursor.execute("UPDATE wired SET enabled=?, configured=?, gateway=?, interface=?, ipaddress=?, netmask=? WHERE interface=?;", template)

        # SQLite commands common to both configuration tables.
        connection.commit()
        cursor.close()

        # Render and display the page.
        try:
            page = templatelookup.get_template("/network/done.html")
            return page.render(title = "Network interface configured.",
                               purpose_of_page = "Configured!",
                               interface = self.mesh_interface,
                               ip_address = self.mesh_ip,
                               netmask = self.mesh_netmask)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    set_ip.exposed = True
