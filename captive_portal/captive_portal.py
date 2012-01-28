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
#    3: Bad IP tables commands during initialization.
#    4: Bad parameters passed to IP tables during initialization.

# v0.1 - Initial release.
# v0.2 - Added a --test option that doesn't actually do anything to the system
#        the daemon's running on, it just prints what the command would be.
#        Makes debugging easier and helps with testing. (Github ticket #87)

# TODO:
# - Write a 404 handler that redirects everything to /index.html.<lang>

# Modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
import os
import sys
import getopt
import subprocess
from subprocess import call

# Global variables.
filedir = '/srv/captiveportal'
configdir = '/etc/captiveportal'
appconfig = os.path.join(configdir,'captiveportal.conf')
cachedir = '/tmp/portalcache'

# Command line arguments to the server.
debug = False
test = False
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

    # whitelist(): Takes the form input from /index.html.*, adds the IP address
    # of the client to IP tables, and then flips the browser to the node's
    # frontpage.  Takes one argument, a value for the variable 'accepted'.
    # Returns an HTML page with an HTTP refresh as its sole content to the
    # client.
    def whitelist(self, accepted=None):
        # Extract the client's IP address from the client headers.
        clientip = cherrypy.request.headers['Remote-Addr']
        if debug:
            print "DEBUG: Client's IP address: %s" % clientip

        # Set up the command string to add the client to the IP tables ruleset.
        addclient = ['/usr/local/sbin/captive-portal.sh', 'add', clientip,
                   interface]
        if test:
            print "Command that would be executed:"
            print str(addclient)
        else:
            iptables = subprocess.call(addclient)

        # Assemble some HTML to redirect the client to the node's frontpage.
        redirect = """<html><head><meta http-equiv="refresh" content="0; url=https://""" + address + """/" /></head> <body></body> </html>"""

        if debug:
            print "DEBUG: Generated HTML refresh is:"
            print redirect

        # Fire the redirect at the client.
        return redirect
    whitelist.exposed = True

# Helper methods used by the core code.
# usage: Prints online help.  Takes no args, returns nothing.
def usage():
    print
    print "This daemon implements the captive portal functionality of Byzantium Linux."
    print "Specifically, it acts as the front end to IP tables and automates the addition"
    print "of mesh clients to the whitelist."
    print
    print "\t-h / --help: Display online help."
    print "\t-i / --interface: The name of the interface the daemon listens on."
    print "\t-a / --address: The IP address of the interface the daemon listens on."
    print "\t-p / --port: Port to listen on.  Defaults to 31337/TCP."
    print "\t-d / --debug: Enable debugging mode."
    print "\t-t / --test: Disables actually doing anything, it just prints what would"
    print "\tbe done.  Used for testing commands without altering the test system."
    print

# Core code.
# Acquire the command line args.
# h - Display online help.
# i: - Interface to listen on.
# a: - Address to listen on so we can figure out the network info later.
# p: - Port to listen on.  Defaults to 31337.
# d - Debugging mode.
# t - Test mode.
shortopts = 'hi:a:p:d:t'
longopts = ['help', 'interface=', 'address=', 'port=', 'debug', 'test']
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

    # User turns on test mode, which prints the commands but doesn't actually
    # run them.
    if opt in ('-t', '--test'):
        test = True
        print "Command testing mode on."

# If some arguments are missing, ABEND.
if not interface:
    print "ERROR: Missing command line argument."
    exit(2)
if not address:
    print "ERROR: Missing command line argument."
    exit(2)

# Set up the location the templates will be served out of.
templatelookup = TemplateLookup(directories=[filedir],
                 module_directory=cachedir, collection_size=100)

# Attach the captive portal object to the URL tree.
root = CaptivePortal()

# Mount the object for the root of the URL tree, which happens to be the
# system status page.  Use the application config file to set it up.
if debug:
    print "DEBUG: Mounting web app in %s to /." % appconfig
cherrypy.tree.mount(root, "/", appconfig)

# Configure a few things about the web server so we don't have to fuss
# with an extra config file, namely, the port and IP address to listen on.
cherrypy.server.socket_port = port
cherrypy.server.socket_host = address

# Initialize the IP tables ruleset for the node.
initialize_iptables = ['/usr/local/sbin/captive-portal.sh', 'initialize',
                       address, interface]
if test:
    print "Command that would be executed:"
    print str(initialize_iptables)
else:
    iptables = subprocess.call(initialize_iptables)

# Now do some error checking in case IP tables went pear-shaped.  This appears
# oddly specific, but /usr/sbin/iptables treats these two kinds of errors
# differently and that makes a difference during troubleshooting.
if iptables == 1:
    print "ERROR: Unknown IP tables error during firewall initialization."
    print "Packet filters NOT configured.  Examine the rules in captive-portal.sh."
    exit(3)

if iptables == 2:
    print "ERROR: Invalid or incorrect options passed to iptables in captive-portal.sh"
    print "Packet filters NOT configured.  Examine the rules in captive-portal.sh."
    print "Parameters passed to captive-portal.sh: initialize, %s, %s" % address, interface
    exit(4)

# Start the web server.
if debug:
    print "DEBUG: Starting web server."
cherrypy.engine.start()

# Insert opening anthem from Blaster Master here.
# Fin.
