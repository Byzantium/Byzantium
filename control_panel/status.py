# status.py - Implements the status screen of the Byzantium control panel.
#    Relies on a few other things running under the hood that are independent
#    of the control panel.  By default, this comprises the /index.html part of
#    the web app.

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# Import modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
import os

# Import control panel modules.
from control_panel import *
from networktraffic import NetworkTraffic
from networkconfiguration import NetworkConfiguration
from meshconfiguration import MeshConfiguration

templatelookup = TemplateLookup(directories=[filedir],
                 module_directory=cachedir, collection_size=100)

# The Status class implements the system status report page that makes up
# /index.html.
class Status(object):
    traffic = NetworkTraffic()
    network = NetworkConfiguration()
    mesh = MeshConfiguration()

    # Pretends to be index.html.
    def index(self):
        # Set the variables that'll eventually be displayed to the user to
        # known values.  If nothing else, we'll know if something is going wrong
        # someplace if they don't change.
        uptime = 0
        one_minute_load = 0
        five_minute_load = 0
        fifteen_minute_load = 0
        ram = 0
        ram_used = 0
        swap = 0
        swap_used = 0

        # Get the node's uptime from the OS.
        sysuptime = self.get_uptime()
        if sysuptime: uptime = sysuptime

        # Convert the uptime in seconds into something human readable.
        (minutes, seconds) = divmod(float(uptime), 60)
        (hours, minutes) = divmod(minutes, 60)
        uptime = "%i hours, %i minutes, %i seconds" % (hours, minutes, seconds)

        # Get the one, five, and ten minute system load averages from the OS.
        sysload = self.get_load()
        if sysload: (one_minute_load,five_minute_load,fifteen_minute_load) = sysload

        # Get the amount of RAM and swap used by the system.
        sysmem = self.get_memory()
        if sysmem: (ram, ram_used, swap, swap_used) = sysmem

        page = templatelookup.get_template("index.html")
        return page.render(uptime = uptime, one_minute_load = one_minute_load,
                           five_minute_load = five_minute_load,
                           fifteen_minute_load = fifteen_minute_load,
                           ram = ram, ram_used = ram_used,
                           swap = swap, swap_used = swap_used,
                           title = "Byzantium Node Control Panel",
                           purpose_of_page = "System Status")
    index.exposed = True

    # Query the node's uptime (in seconds) from the OS.
    def get_uptime(self):
        # Open /proc/uptime.
        uptime = open("/proc/uptime", "r")

        # Read the first vlaue from the file.  If it can't be opened, return
        # nothing and let the default values take care of it.
        system_uptime = uptime.readline()
        if not system_uptime:
            uptime.close()
            return False

        # Separate the uptime from the idle time.
        node_uptime = system_uptime.split()[0]

        # Cleanup.
        uptime.close()

        # Return the system uptime (in seconds).
        return node_uptime

    # Queries the OS to get the system load stats.
    def get_load(self):
        # Open /proc/loadavg.
        loadavg = open("/proc/loadavg", "r")

        # Read first three values from /proc/loadavg.  If it can't be opened,
        # return nothing and let the default values take care of it.
        loadstring = loadavg.readline()
        if not loadstring:
            loadavg.close()
            return False

        # Extract the load averages from the string.
        averages = loadstring.split()
        loadavg.close()
        # check to avoid errors
        if len(averages) < 3:
           print('WARNING: /proc/loadavg is not formatted as expected')
           return False

        # Return the load average values.
        return (averages[:3])

    # Queries the OS to get the system memory usage stats.
    def get_memory(self):
        # Open /proc/meminfo.
        meminfo = open("/proc/meminfo", "r")

        # Read in the contents of that virtual file.  Put them into a dictionary
        # to make it easy to pick out what we want.  If this can't be done,
        # return nothing and let the default values handle it.
        for line in meminfo:
           # homoginize the data
           line = line.strip().lower()
           # Figure out how much RAM and swap are in use right now
           try:
              if line.startswith('memtotal'):
                 memtotal = line.split()[1]
              elif line.startswith('memfree'):
                 memfree = line.split()[1]
              elif line.startswith('swaptotal'):
                 swaptotal = line.split()[1]
              elif line.startswith('swapfree'):
                 swapfree = line.split()[1]
           except KeyError as e:
              print(e)
              print('WARNING: /proc/meminfo is not formatted as expected')
              return False
        memused = int(memtotal) - int(memfree)
        swapused = int(swaptotal) - int(swapfree)

        # Return total RAM, RAM used, total swap space, swap space used.
        return (memtotal, memused, memtotal, swapused)

