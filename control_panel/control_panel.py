#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

""""Main code for running the control panel app.

This application runs in the background and implements a (relatively) simple
HTTP server using CherryPy (http://www.cherrypy.org/) that controls the
major functions of the Byzantium node, such as configuring, starting, and
stopping the mesh networking subsystem.  CherryPy is hardcoded to listen on
the loopback interface (127.0.0.1) on port 8080/TCP unless told otherwise.
For security reasons I see no reason to change this; if you want to admin a
Byzantium node remotely you'll have to use SSH port forwarding.

v0.2  - Split the network traffic graphs from the system status report.
v0.1  - Initial release.
"""

from status import Status
from mako.lookup import TemplateLookup
import cherrypy
import argparse
import logging
import os

from networktraffic import NetworkTraffic
from networkconfiguration import NetworkConfiguration
from meshconfiguration import MeshConfiguration
from services import Services
from gateways import Gateways


def parse_args():
    """Argument parser."""
    parser = argparse.ArgumentParser(conflict_handler='resolve',
                                     description="This daemon implements the "
                                     "control panel functionality of Byzantium "
                                     "Linux.")
    parser.add_argument("--cachedir", action="store",
                        default="/tmp/controlcache")
    parser.add_argument("--configdir", action="store",
                        default="/etc/controlpanel")
    parser.add_argument("-d", "--debug", action="store_true", default=False,
                        help="Enable debugging mode.")
    parser.add_argument("--filedir", action="store",
                        default="/srv/controlpanel")
    parser.add_argument("-t", "--test", action="store_true", default=False,
                        help="Disables actually doing anything, it just prints "
                        "what would be done.  Used for testing commands "
                        "without altering the test system.")
    return parser.parse_args()


def check_args(args):
    """Audits the args."""
    if args.debug:
        print "Control panel debugging mode is on."
    if args.test:
        print "Control panel functional testing mode is on."
        # Configure for running in the current working directory.  This will
        # always be Byzantium/control_panel/.
        print("TEST: Referencing files from current working directory for "
              "testing.")
        args.filedir = 'srv/controlpanel'
        args.configdir = 'etc/controlpanel'
    return args


def main():
    """Yel olde main function."""
    args = check_args(parse_args())
    if args.debug or args.test:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.ERROR)
    globalconfig = os.path.join(args.configdir,'controlpanelGlobal.conf')
    if args.test:
        appconfig = os.path.join(args.configdir,'controlpanel_test.conf')
    else:
        appconfig = os.path.join(args.configdir,'controlpanel.conf')

    # Set up the location the templates will be served out of.
    templatelookup = TemplateLookup(directories=[args.filedir],
                                    module_directory=args.cachedir,
                                    collection_size=100)

    # Read in the name and location of the appserver's global config file.
    cherrypy.config.update(globalconfig)

    # Allocate the objects representing the URL tree.
    root = Status(templatelookup, args.test, args.filedir)

    # Allocate objects for all of the control panel's main features.
    root.traffic = NetworkTraffic(args.filedir, templatelookup)
    root.network = NetworkConfiguration(templatelookup, args.test)
    root.mesh = MeshConfiguration(templatelookup, args.test)
    root.services = Services(templatelookup, args.test)
    root.gateways = Gateways(templatelookup, args.test)

    # Mount the object for the root of the URL tree, which happens to be the
    # system status page.  Use the application config file to set it up.
    logging.debug("Mounting Status() object as webapp root.")
    cherrypy.tree.mount(root, "/", appconfig)

    # Start the web server.
    if args.debug:
        logging.debug("Starting CherryPy.")
    cherrypy.engine.start()
    cherrypy.engine.block()

if __name__ == "__main__":
    main()

