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
#    5: Daemon already running on this network interface.

# v0.1 - Initial release.
# v0.2 - Added a --test option that doesn't actually do anything to the system
#        the daemon's running on, it just prints what the command would be.
#        Makes debugging easier and helps with testing. (Github ticket #87)
#      - Added a 404 error handler that redirects the client to / on the same
#        port that the captive portal daemon is listening on. (Github
#        ticket #85)
#      - Figured out how to make CherryPy respond to the usual signals (i.e.,
#        SIGTERM) and call a cleanup function to take care of things before
#        terminating.  It took a couple of hours of hacking but I finally found
#        something that worked.
# v0.3 - Added code to implement an Apple iOS captive portal detector.
#      - Added code that starts the idle client reaper daemon that Haxwithaxe
#        wrote.

# TODO:

# Modules.
import cherrypy
from cherrypy import _cperror
from cherrypy.process.plugins import PIDFile
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
pidfile = ''

# Command line arguments to the server.
debug = False
test = False
interface = ''
address = ''
port = ''

# The CaptivePortalDetector class implements a fix for an undocumented bit of
# fail in Apple iOS.  iProducts attempt to access a particular file hidden in
# the Apple Computer website.  If it can't find it, iOS forces the user to try
# to log into the local captive portal (even if it doesn't support that
# functionality).  This breaks things for users of iOS, unless we fix it in
# the captive portal.
class CaptivePortalDetector(object):
    if debug:
        print "DEBUG: Instantiating captive portal detector object."

    # __init__(): Method that runs when the CaptivePortalDetector() object is
    #             instantiated.
    def __init__(self):
        if debug:
            print "DEBUG: running hacked __init__() method of CaptivePortalDetector()."
        setattr(self, 'success.html', self.success)

    # index(): Pretends to be /library/test and /library/test/index.html.
    def index(self):
        return("You shouldn't be seeing this, either.")
    index.exposed = True

    # success(): Pretends to be http://apple.com/library/test/success.html.
    def success(self):
        if debug:
            print "DEBUG: iOS device detected."
        success = '''
                <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2//EN">
                <HTML>
                <HEAD>
                        <TITLE>Success</TITLE>
                </HEAD>
                <BODY>
                        Success
                </BODY>
                </HTML>
                '''
        return success
    success.exposed = True

# Dummy class that has to exist to create a durectory URI hierarchy.
class Library(object):
    if debug:
        print "DEBUG: Instantiating Library() dummy object."
    test = CaptivePortalDetector()

    # index(): Pretends to be /library and /library/index.html.
    def index(self):
        return("You shouldn't be seeing this.")
    index.exposed = True

# The CaptivePortal class implements the actual captive portal stuff - the
# HTML front-end and the IP tables interface.
class CaptivePortal(object):
    if debug:
        print "DEBUG: Mounting Library() from CaptivePortal()."
    library = Library()

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
        addclient = ['/usr/local/sbin/captive-portal.sh', 'add', clientip]
        if test:
            print "Command that would be executed:"
            print str(addclient)
        else:
            iptables = subprocess.call(addclient)

        # Assemble some HTML to redirect the client to the node's frontpage.
        redirect = """<html><head><meta http-equiv="refresh" content="0; url=https://""" + address + """/" /></head><body></body></html>"""

        if debug:
            print "DEBUG: Generated HTML refresh is:"
            print redirect

        # Fire the redirect at the client.
        return redirect
    whitelist.exposed = True

    # error_page_404(): Registered with CherryPy as the default handler for
    # HTTP 404 errors (file or resource not found).  Takes four arguments (this
    # is required by CherryPy), returns some HTML generated at runtime that
    # redirects the client to http://<IP address>/, where it'll be caught by
    # CaptivePortal.index().  I wish there was an easier way to do this (like
    # calling self.index() directly) but the stable's fresh out of ponies.
    # We don't use any of the arguments passed to this method so I reference
    # a few of them in debug mode.
    def error_page_404(status, message, traceback, version):
        # Extract the client's IP address from the client headers.
        clientip = cherrypy.request.headers['Remote-Addr']
        if debug:
            print "DEBUG: Client's IP address: %s" % clientip
            print "DEBUG: Value of status is: %s" % status
            print "DEBUG: Value of message is: %s" % message

        # Assemble some HTML to redirect the client to the captive portal's
        # /index.html-* page.
        redirect = """<html><head><meta http-equiv="refresh" content="0; url=http://""" + address + """/" /></head> <body></body> </html>"""

        if debug:
            print "DEBUG: Generated HTML refresh is:"
            print redirect
            print "DEBUG: Redirecting client to /."

        # Fire the redirect at the client.
        return redirect
    cherrypy.config.update({'error_page.404':error_page_404})

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
# Set up the command line arguments.
# h - Display online help.
# i: - Interface to listen on.
# a: - Address to listen on so we can figure out the network info later.
# p: - Port to listen on.  Defaults to 31337.
# d - Debugging mode.
# t - Test mode.
shortopts = 'hi:a:p:dt'
longopts = ['help', 'interface=', 'address=', 'port=', 'debug', 'test']
try:
    (opts, args) = getopt.getopt(sys.argv[1:], shortopts, longopts)
except getopt.GetoptError:
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
    print "ERROR: Missing command line argument 'interface'."
    exit(2)

# Create the filename for this instance's PID file.
if test:
    pidfile = '/tmp/captive_portal.'
else:
    pidfile = '/var/run/captive_portal.'
pidfile = pidfile + interface
if debug:
    print "DEBUG: Name of PID file is: %s" % pidfile

# If a PID file already exists for this network interface, ABEND.
if os.path.exists(pidfile):
    print "ERROR: A pidfile already exists for network interface %s." % pidfile
    print "ERROR: Is a daemon already running on this interface?"
    exit(5)

# Write the PID file of this instance to the PID file.
if debug:
    print "DEBUG: Creating pidfile for network interface %s." % str(interface)
    print "DEBUG: PID of process is %s." % str(os.getpid())
pid = PIDFile(cherrypy.engine, pidfile)
pid.subscribe()

# Configure a few things about the web server so we don't have to fuss
# with an extra config file, namely, the port and IP address to listen on.
cherrypy.config.update({'server.socket_host':'0.0.0.0', })
cherrypy.config.update({'server.socket_port':port, })

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

# Initialize the IP tables ruleset for the node.
initialize_iptables = ['/usr/local/sbin/captive-portal.sh', 'initialize',
                       address, interface]
iptables = 0
if test:
    print "Command that would be executed:"
    print str(initialize_iptables)
else:
    iptables = subprocess.call(initialize_iptables)

# Start up the idle client reaper daemon.
idle_client_reaper = ['/usr/local/sbin/mop_up_dead_clients.py', '-m', '600',
                      '-i', '60']
reaper = 0
if test:
    print "Idle client monitor command that would be executed:"
    print str(idle_client_reaper)
else:
    reaper = subprocess.Popen(idle_client_reaper)
if reaper:
    print "ERROR: mop_up_dead_clients.py did not start."

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
cherrypy.engine.block()

# [Insert opening anthem from Blaster Master here.]
# Fin.
