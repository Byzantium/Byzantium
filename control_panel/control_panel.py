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

# Global variables.
filedir = '/srv/controlpanel'
configdir = '/etc/controlpanel'
globalconfig = os.path.join(configdir,'controlpanelGlobal.conf')
appconfig = os.path.join(configdir,'controlpanel.conf')
cachedir = '/tmp/controlcache'

# Core code.
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
    cherrypy.tree.mount(root, "/", appconfig)

    # Start the web server.
    cherrypy.engine.start()
    cherrypy.engine.block()

if __name__=="__main__":
    main()

