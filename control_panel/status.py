# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

# status.py - Implements the status screen of the Byzantium control panel.
#    Relies on a few other things running under the hood that are independent
#    of the control panel.  By default, this comprises the /index.html part of
#    the web app.

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# Import modules.
import logging
import os
import os.path
import sqlite3
import subprocess

# Import control panel modules.
# from control_panel import *
from networktraffic import NetworkTraffic
from networkconfiguration import NetworkConfiguration
from meshconfiguration import MeshConfiguration
from services import Services
from gateways import Gateways


# Query the node's uptime (in seconds) from the OS.
def get_uptime(injected_open=open):
    # Open /proc/uptime.
    try:
        uptime = injected_open("/proc/uptime", "r")
    except IOError:
        # Can't find file
        return False

    system_uptime = uptime.readline()

    # Separate the uptime from the idle time.
    node_uptime = system_uptime.split()[0]

    # Cleanup.
    uptime.close()

    # Return the system uptime (in seconds).
    return node_uptime


# Queries the OS to get the system load stats.
def get_load(injected_open=open):
    # Open /proc/loadavg.
    try:
        loadavg = injected_open("/proc/loadavg", "r")
    except IOError:
        return False

    loadstring = loadavg.readline()
    # Extract the load averages from the string.
    averages = loadstring.split(' ')
    loadavg.close()
    # check to avoid errors
    if len(averages) < 3:
        print('WARNING: /proc/loadavg is not formatted as expected')
        return False

    # Return the load average values.
    return (averages[:3])


# Queries the OS to get the system memory usage stats.
def get_memory(injected_open=open):
    memtotal = 0
    memused = 0
    # Open /proc/meminfo.
    try:
        meminfo = injected_open("/proc/meminfo", "r")
    except IOError:
        return False

    # Read in the contents of that virtual file.  Put them into a dictionary
    # to make it easy to pick out what we want.  If this can't be done,
    # return nothing and let the default values handle it.
    for line in meminfo:
        # Homoginize the data.
        line = line.strip().lower()
        # Figure out how much RAM and swap are in use right now
        try:
            if line.startswith('memtotal'):
                memtotal = int(line.split()[1])
            elif line.startswith('memfree'):
                memfree = int(line.split()[1])
            # break out early
            if bool(memtotal) and bool(memused):
                break
        except KeyError as ex:
            print(ex)
            print('WARNING: /proc/meminfo is not formatted as expected')
            return False
    memused = memtotal - memfree

    # Return total RAM, RAM used, total swap space, swap space used.
    return (memtotal, memused)


def get_ip_address(interface):
    ip_address = ''
    command = ['/sbin/ifconfig', interface]
    output = subprocess.Popen(command, stdout=subprocess.PIPE).stdout
    configuration = output.readlines()
    logging.debug("Output of ifconfig:")
    logging.debug(configuration)

    # Parse the output of ifconfig.
    for line in configuration:
        if 'inet addr' in line:
            line = line.strip()
            ip_address = line.split(' ')[1].split(':')[1]
            logging.debug("IP address is %s", ip_address)
    return ip_address

# The Status class implements the system status report page that makes up
# /index.html.
class Status(object):

    def __init__(self, templatelookup, test, filedir):
        self.templatelookup = templatelookup
        self.test = test
        # Allocate objects for all of the control panel's main features.
        self.traffic = NetworkTraffic(filedir, templatelookup)
        self.network = NetworkConfiguration(templatelookup, test)
        self.mesh = MeshConfiguration(templatelookup, test)
        self.services = Services(templatelookup, test)
        self.gateways = Gateways(templatelookup, test)

        # Location of the network.sqlite database, which holds the configuration
        # of every network interface in the node.
        if test:
            # self.netconfdb = '/home/drwho/network.sqlite'
            self.netconfdb = 'var/db/controlpanel/network.sqlite'
            logging.debug("Location of NetworkConfiguration.netconfdb: %s", self.netconfdb)
        else:
            self.netconfdb = '/var/db/controlpanel/network.sqlite'

    # Pretends to be index.html.
    def index(self):
        logging.debug("Entered Status.index().")
        
        # Get the node's uptime from the OS.
        uptime = get_uptime() or 0

        # Convert the uptime in seconds into something human readable.
        (minutes, seconds) = divmod(float(uptime), 60)
        (hours, minutes) = divmod(minutes, 60)
        uptime = "%i hours, %i minutes, %i seconds" % (hours, minutes, seconds)
        logging.debug("System uptime: %s", uptime)

        # Get the amount of RAM in and in use by the system.
        ram, ram_used = get_memory()
        logging.debug("Total RAM: %s", ram)
        logging.debug("RAM in use: %s", ram_used)

        # For the purposes of debugging, test to see if the network
        # configuration database file exists and print a tell to the console.
        logging.debug("Checking for existence of network configuration database.")
        if os.path.exists(self.netconfdb):
            logging.debug("Network configuration database %s found.", self.netconfdb)
        else:
            logging.debug("DEBUG: Network configuration database %s NOT found!", self.netconfdb)

        # Pull a list of the mesh interfaces on this system out of the network
        # configuration database.  If none are found, report none.
        mesh_interfaces = ''
        ip_address = ''

        connection = sqlite3.connect(self.netconfdb)
        cursor = connection.cursor()
        query = "SELECT mesh_interface, essid, channel FROM wireless;"
        cursor.execute(query)
        result = cursor.fetchall()

        if not result:
            # Fields:
            #    interface, IP, ESSID, channel
            mesh_interfaces = "<tr><td>n/a</td>\n<td>n/a</td>\n<td>n/a</td>\n<td>n/a</td></tr>\n"
        else:
            for (mesh_interface, essid, channel) in result:
                # Test to see if any of the variables retrieved from the
                # database are empty, and if they are set them to obviously
                # non-good but also non-null values.
                if not mesh_interface:
                    logging.debug("Value of mesh_interface is empty.")
                    mesh_interface = ' '
                if not essid:
                    logging.debug("Value of ESSID is empty.")
                    essid = ' '
                if not channel:
                    logging.debug("Value of channel is empty.")
                    channel = 0

                # For every mesh interface found in the database, get its
                # current IP address with ifconfig.
                command = ['/sbin/ifconfig', mesh_interface]
                if self.test:
                    print "TEST: Status.index() command to pull the configuration of a mesh interface:"
                    print '/sbin/ifconfig' + mesh_interface
                else:
                    logging.debug("Running ifconfig to collect configuration of interface %s.", mesh_interface)

                    ip_address = get_ip_address(mesh_interface)

                # Assemble the HTML for the status page using the mesh
                # interface configuration data.
                mesh_interfaces = mesh_interfaces + "<tr><td>" + mesh_interface + "</td>\n<td>" + ip_address + "</td>\n<td>" + essid + "</td>\n<td>" + str(channel) + "</td></tr>\n"

        # Pull a list of the client interfaces on this system.  If none are
        # found, report none.
        client_interfaces = ''
        ip_address = ''
        number_of_clients = 0

        query = "SELECT client_interface FROM wireless;"
        cursor.execute(query)
        result = cursor.fetchall()

        if not result:
            # Fields:
            #    interface, IP, active clients
            client_interfaces = "<tr><td>n/a</td>\n<td>n/a</td>\n<td>0</td></tr>\n"
        else:
            for client_interface in result:
                # For every client interface found, run ifconfig and pull
                # its configuration information.
                if self.test:
                    print "TEST: Status.index() command to pull the configuration of a client interface:"
                    print '/sbin/ifconfig' + client_interface[0]
                else:
                    logging.debug("Running ifconfig to collect configuration of interface %s.", client_interface)
                    ip_address = get_ip_address(client_interface[0])

                # For each client interface, count the number of rows in its
                # associated arp table to count the number of clients currently
                # associated.  Note that one has to be subtracted from the
                # count of rows to account for the line of column headers.
                command = ['/sbin/arp', '-n', '-i', client_interface[0]]
                if self.test:
                    print "TEST: Status.index() command to dump the ARP table of interface %s: " % client_interface
                    print command
                else:
                    logging.debug("Running arp to dump the ARP table of client interface %s.", client_interface)
                    output = subprocess.Popen(command, stdout=subprocess.PIPE).stdout
                    arp_table = output.readlines()
                    logging.debug("Contents of ARP table:")
                    logging.debug(arp_table)

                    # Count the number of clients associated with the client
                    # interface by analyzing the ARP table.
                    number_of_clients = len(arp_table) - 1
                    logging.debug("Number of associated clients: %i", number_of_clients)

                # Assemble the HTML for the status page using the mesh
                # interface configuration data.
                client_interfaces = client_interfaces + "<tr><td>" + client_interface[0] + "</td>\n<td>" + ip_address + "</td>\n<td>" + str(number_of_clients) + "</td></tr>\n"

        # Render the HTML page and return to the client.
        cursor.close()
        page = self.templatelookup.get_template("index.html")
        return page.render(ram_used = ram_used, ram = ram, uptime = uptime,
                           mesh_interfaces = mesh_interfaces,
                           client_interfaces = client_interfaces,
                           title = "Byzantium Mesh Node Status",
                           purpose_of_page = "System Status")
    index.exposed = True





