#!/usr/bin/env python

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium

# control_panel.py
# This application runs in the background and implements a (relatively) simple
# HTTP server using CherryPy (http://www.cherrypy.org/) that controls the
# major functions of the Byzantium node, such as configuring, starting, and
# stopping the mesh networking subsystem.  CherryPy is hardcoded to listen on
# the loopback interface (127.0.0.1) on port 8080/TCP unless told otherwise.
# For security reasons I see no reason to change this; if you want to admin a
# Byzantium node remotely you'll have to use SSH port forwarding.

# Uses the Mako templating system (http://www.makotemplates.org/).

# Developers:
# The Doctor [412/724/301/703] (http://drwho.virtadpt.net) (drwho@virtadpt.net)
# PGP: 0x807B17C1 / 7960 1CDC 85C9 0B63 8D9F  DD89 3BD8 FF2B 807B 17C1

# By default, this app can be accessed at http://localhost:8080/

# URL mappings:
#	/ == /index.html == system status
#	/traffic == /traffic/index.html == network traffic report

# v0.2	- Split the network traffic graphs from the system status report.
# v0.1	- Initial release.

# Import modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
import os

# Global variables.
filedir = '/srv/controlpanel'
globalconfig = '/etc/controlpanel/controlpanelGlobal.conf'
appconfig = '/etc/controlpanel/controlpanel.conf'
cachedir = '/tmp/controlcache'

# Classes.
# This class implements the network traffic status report page.
class NetworkTraffic(object):
    # Pretends to be index.html.
    def index(self):
        # Enumerate the list of PNG files in the graphs/ directory and generate
        # a sequence of IMG SRCs to insert into the HTML template.
        graphdir = "/srv/controlpanel/graphs"
        images = os.listdir(graphdir)

        # Pack the string of IMG SRCs into a string.
        graphs = ""
        for image in images:
            graphs = graphs + '<img src="/graphs/' + image + '" width="75%"' + 'height="75%" alt="' + image + '" /><br />'

        page = templatelookup.get_template("/traffic/index.html")
        return page.render(graphs = graphs,
                           title = "Byzantium Network Traffic Report",
                           purpose_of_page = "Traffic Graphs")
    index.exposed = True

# The Status class implements the system status report page that makes up
# /index.html.
class Status(object):
    traffic = NetworkTraffic()

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
        uptime = self.get_uptime()

        # Convert the uptime in seconds into something human readable.
        (minutes, seconds) = divmod(float(uptime), 60)
        (hours, minutes) = divmod(minutes, 60)
        uptime = "%i hours, %i minutes, %i seconds" % (hours, minutes, seconds)

        # Get the one, five, and ten minute system load averages from the OS.
        (one_minute_load,five_minute_load,fifteen_minute_load) = self.get_load()

        # Get the amount of RAM and swap used by the system.
        (ram, ram_used, swap, swap_used) = self.get_memory()

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
            return

        # Separate the uptime from the idle time.
        (node_uptime, node_idle_time) = system_uptime.split()

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
            return

        # Extract the load averages from the string.
        averages = loadstring.split()
        loadavg.close()

        # Return the load average values.
        return (averages[0], averages[1], averages[2])

    # Queries the OS to get the system memory usage stats.
    def get_memory(self):
        # Set up the dictionary that will hold the system's memory stats.
        memstats = {}

        # Open /proc/meminfo.
        meminfo = open("/proc/meminfo", "r")

        # Read in the contents of that virtual file.  Put them into a dictionary
        # to make it easy to pick out what we want.  If this can't be done,
        # return nothing and let the default values handle it.
        for line in meminfo:
            (label, value, junk) = line.split()
            memstats[label] = value

        # Clean up after ourselves.
        meminfo.close()

        # Figure out how much RAM and swap are in use right now.
        memused = int(memstats['MemTotal:']) - int(memstats['MemFree:'])
        swapused = int(memstats['SwapTotal:']) - int(memstats['SwapFree:'])

        # Return total RAM, RAM used, total swap space, swap space used.
        return (memstats['MemTotal:'], memused, memstats['SwapTotal:'], swapused)

# Core code.
# Set up the location the templates will be served out of.
templatelookup = TemplateLookup(directories=[filedir],
                 module_directory=cachedir, collection_size=100)

# Read in the name and location of the appserver's global config file.
cherrypy.config.update(globalconfig)

# Allocate the objects representing the URL tree.
root = Status()

# Mount the object for the root of the URL tree, which happens to be the system
# status page.  Use the application config file to set it up.
cherrypy.tree.mount(root, "/", appconfig)

# Start the web server.
cherrypy.engine.start()

# End.
