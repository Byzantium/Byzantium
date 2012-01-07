#!/usr/bin/env python

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# captive_portal.py
# This application implements a little web server that mesh clients will be
# redirected to if they haven't been added to the whitelist in IP tables.  When
# the user clicks a button a callback will be triggered that runs iptables and
# adds the IP and MAC addresses to the whitelist so that it won't run into the
# captive portal anymore.

# The fiddly configuration-type stuff is passed on the command line to avoid
# having to manage a config file with the control panel.

# Codes passed to exit().  They'll be of use to the control panel later.
#    0: Normal termination.
#    1: Insufficient CLI args.
#    2: Bad CLI args.

# v0.1 - Initial release.

# Modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
import os
import sys
import getopt

# Global variables.
filedir = '/srv/captiveportal'
configdir = '/etc/captiveportal'
appconfig = os.path.join(configdir,'captiveportal.conf')
cachedir = '/tmp/portalcache'

# Command line arguments.
debug = False
interface = ''
address = ''
port = ''

# The CaptivePortal class implements the actual captive portal stuff - the
# HTML front-end and the IP tables interface.
class CaptivePortal(object):
    # index(): Pretends to be / and /index.html.
    def index(self):
        # Identify the primary language the client's web browser supports.
        clientlang = cherrypy.request.headers['Accept-Language']
        clientlang = clientlang.split(',')[0]
        if debug:
            print "DEBUG: Current browser language: %s" % clientlang

        # Piece together the filename of the /index.html file to return based
        # on the primary language.
        indexhtml = "index.html." + clientlang
        page = templatelookup.get_template(indexhtml)
        return page.render()
    index.exposed = True

    # whitelist(): Takes the form input from /index.html/*, adds the IP address
    # of the client to IP tables, and then flips the browser to the node's
    # frontpage.  Takes one argument, a value for the variable 'accepted'.
    # Returns an HTML page with an HTTP refresh as its sole content to the
    # client.
    def whitelist(self, accepted=None):

    whitlelist.exposed = True

    # This will be the catch-all URI for captive portal.  Everything will
    # fall back to CaptivePortal.index().
    # MOOF MOOF MOOF
    #def default (self, *args):
    #    self.index()
    #default.exposed = True

# Helper methods used by the core code.
# usage: Prints online help.  Takes no args, returns nothing.
def usage():
    print "This daemon implements the captive portal functionality of Byzantium Linux."
    print "Specifically, it acts as the front end to IP tables and automates the addition"
    print "of mesh clients to the whitelist."
    print "\t-h / --help: Display online help."
    print "\t-i / --interface: The name of the interface this daemon listens on."
    print "\t-a / --address: The IP address of the interface this daemon listens on."
    print "\t-p / --port: Port to listen on.  Defaults to 31337/TCP."
    print "\t-d / --debug: Enable debugging mode."

# Core code.
# Acquire the command line args.
# h - Display online help.
# i: - Interface to listen on.
# a: - Address to listen on so we can figure out the network info later.
# p: - Port to listen on.  Defaults to 31337.
# d - Debugging mode.  This is new code.
shortopts = 'hi:a:p:d'
longopts = ['help', 'interface=', 'address=', 'port=', 'debug']
try:
    (opts, args) = getopt.getopt(sys.argv[1:], shortopts, longopts)
except get.GetoptError:
    print "ERROR: Bad command line argument."
    usage()
    exit(2)

# Parse the command line args.
for opt, arg in opts:
    # Is the user asking for help?
    if opt in ('-h', '--help'):
        usage()
        exit(1)

    # User specifying the network interface to listen on.  This will be
    # more helpful in bookkeeping than anything else.
    if opt in ('-i', '--interface'):
        interface = arg.rstrip()

    # User specifying the IP address to listen on.  This is more useful
    # for network math than anything else.
    if opt in ('-a', '--address'):
        address = arg.rstrip()

    # User specifies the port to listen on.  This has a default.
    if opt in ('-p', '--port'):
        port = arg.rstrip()
    else:
        port = 31337

    # User turns on debugging mode.
    if opt in ('-d', '--debug'):
        debug = True
        print "Debugging mode on."

# If some arguments are missing, ABEND.
if not interface:
    print "ERROR: Missing command line argument."
    exit(2)
if not address:
    print "ERROR: Missing command line argument."
    exit(2)

# Set up the location the templates will be served out of.
templatelookup = TemplateLookup(directories=[filedir],
                module_directory=cachedir, collection_size=50)

# Attach the captive portal object to the URL tree.
root = CaptivePortal()

# Mount the object for the root of the URL tree, which happens to be the
# system status page.  Use the application config file to set it up.
if debug:
    print "Mounting web app in %s to /." % appconfig
cherrypy.tree.mount(root, "/", appconfig)

# Configure a few things about the web server so we don't have to fuss
# with an extra config file, namely, the port and IP address to listen on.
cherrypy.server.socket_port = port
cherrypy.server.socket_host = address

# Start the web server.
if debug:
    print "Starting web server."
cherrypy.engine.start()

# Insert opening anthem from Blaster Master here.
# Fin.
