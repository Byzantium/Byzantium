#!/usr/bin/env python

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# control_panel.py
# This application runs in the background and implements a (relatively) simple
# HTTP server using CherryPy (http://www.cherrypy.org/) that controls the
# major functions of the Byzantium node, such as configuring, starting, and
# stopping the mesh networking subsystem.  CherryPy is hardcoded to listen on
# the loopback interface (127.0.0.1) on port 8080/TCP unless told otherwise.
# For security reasons I see no reason to change this; if you want to admin a
# Byzantium node remotely you'll have to use SSH port forwarding.

# v0.2	- Split the network traffic graphs from the system status report.
# v0.1	- Initial release.

# Import modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
import os
import sys
import getopt

# Global variables.
cachedir = '/tmp/controlcache'

# Mode control flags.
debug = False
test = False

# Core code.
# Set up the command line flags passed to this application to determine what
# mode (if any) the control panel should be in.
# d - debugging mode
# t - test mode
shortopts = 'dt'
longopts = ['debug', 'test']
try:
    (opts, args) = getopt.getopt(sys.argv[1:], shortopts, longopts)
except getopt.GetoptError:
    print "ERROR: Bad command line argument."
    print "ARGS: --debug/-d | --test/-t"
    print "These command line arguments may be combined."
    exit(1)

# Parse the command line arguments and set global variables as appropriate.
for opt, arg in opts:
    if opt in ('-d', '--debug'):
        debug = True
        print "Control panel debugging mode is on."
    if opt in ('-t', '--test'):
        test = True
        print "Control panel functional testing mode is on."

# Configure the CLI-dependent global variables used by the rest of the control
# panel.
if test:
    # Configure for running in the current working directory.  This will
    # always be Byzantium/control_panel/.
    print "TEST: Referencing files from current working directory for testing."
    filedir = 'srv/controlpanel'
    configdir = 'etc/controlpanel'
else:
    # Configure for production.
    filedir = '/srv/controlpanel'
    configdir = '/etc/controlpanel'
globalconfig = os.path.join(configdir,'controlpanelGlobal.conf')
appconfig = os.path.join(configdir,'controlpanel.conf')

# Set up the location the templates will be served out of.
templatelookup = TemplateLookup(directories=[filedir],
                 module_directory=cachedir, collection_size=100)

def main():
    # Read in the name and location of the appserver's global config file.
    cherrypy.config.update(globalconfig)

    # Allocate the objects representing the URL tree.
    from status import Status
    root = Status()

    # Mount the object for the root of the URL tree, which happens to be the
    # system status page.  Use the application config file to set it up.
    if debug:
        print "DEBUG: Mounting Status() object as webapp root."
    cherrypy.tree.mount(root, "/", appconfig)

    # Start the web server.
    if debug:
        print "DEBUG: Starting CherryPy."
    cherrypy.engine.start()
    cherrypy.engine.block()

if __name__=="__main__":
    main()

