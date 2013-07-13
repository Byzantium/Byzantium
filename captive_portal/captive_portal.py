#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :
# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
__license__ = 'GPL v3'

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
#      - Added a second listener for HTTPS connections.

# TODO:

# Modules.
import cherrypy
from cherrypy.process.plugins import PIDFile
from mako.lookup import TemplateLookup

import argparse
import fcntl
import logging
import os
import socket
import struct
import subprocess

# Need this for the 404 method.
def get_ip_address(interface):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    return socket.inet_ntoa(fcntl.ioctl(sock.fileno(), 0x8915,
                            struct.pack('256s', interface[:15]))[20:24])

# The CaptivePortalDetector class implements a fix for an undocumented bit of
# fail in Apple iOS.  iProducts attempt to access a particular file hidden in
# the Apple Computer website.  If it can't find it, iOS forces the user to try
# to log into the local captive portal (even if it doesn't support that
# functionality).  This breaks things for users of iOS, unless we fix it in
# the captive portal.
class CaptivePortalDetector(object):
    # index(): Pretends to be /library/test and /library/test/index.html.
    def index(self):
        return("You shouldn't be seeing this, either.")
    index.exposed = True

    # success_html(): Pretends to be http://apple.com/library/test/success.html.
    def success_html(self):
        logging.debug("iOS device detected.")
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
    success_html.exposed = True

# Dummy class that has to exist to create a durectory URI hierarchy.
class Library(object):

    logging.debug("Instantiating Library() dummy object.")
    test = CaptivePortalDetector()

    # index(): Pretends to be /library and /library/index.html.
    def index(self):
        return("You shouldn't be seeing this.")
    index.exposed = True


# The CaptivePortal class implements the actual captive portal stuff - the
# HTML front-end and the IP tables interface.
class CaptivePortal(object):

    def __init__(self, args):
        self.args = args

        logging.debug("Mounting Library() from CaptivePortal().")
        self.library = Library()

    # index(): Pretends to be / and /index.html.
    def index(self):
        # Identify the primary language the client's web browser supports.
        try:
            clientlang = cherrypy.request.headers['Accept-Language']
            clientlang = clientlang.split(',')[0].lower()
        except:
            logging.debug("Client language not found.  Defaulting to en-us.")
            clientlang = 'en-us'
        logging.debug("Current browser language: %s", clientlang)

        # Piece together the filename of the /index.html file to return based
        # on the primary language.
        indexhtml = "index.html." + clientlang
        templatelookup = build_templatelookup(self.args)
        try:
            page = templatelookup.get_template(indexhtml)
        except:
            page = templatelookup.get_template('index.html.en-us')
            logging.debug("Unable to find HTML template for language %s!", clientlang)
            logging.debug("\tDefaulting to /srv/captiveportal/index.html.en-us.")
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
        logging.debug("Client's IP address: %s", clientip)

        # Set up the command string to add the client to the IP tables ruleset.
        addclient = ['/usr/local/sbin/captive-portal.sh', 'add', clientip]
        if self.args.test:
            logging.debug("Command that would be executed:\n%s", addclient)
        else:
            subprocess.call(addclient)

        # Assemble some HTML to redirect the client to the node's frontpage.
        redirect = """
                   <html>
                   <head>
                   <meta http-equiv="refresh" content="0; url=http://""" + self.args.address + """/index.html" />
                   </head>
                   <body>
                   </body>
                   </html>"""

        logging.debug("Generated HTML refresh is:")
        logging.debug(redirect)

        # Fire the redirect at the client.
        return redirect
    whitelist.exposed = True

    # error_page_404(): Registered with CherryPy as the default handler for
    # HTTP 404 errors (file or resource not found).  Takes four arguments
    # (required by CherryPy), returns some HTML generated at runtime that
    # redirects the client to http://<IP address>/, where it'll be caught by
    # CaptivePortal.index().  I wish there was an easier way to do this (like
    # calling self.index() directly) but the stable's fresh out of ponies.
    # We don't use any of the arguments passed to this method so I reference
    # them in debug mode.
    def error_page_404(status, message, traceback, version):
        # Extract the client's IP address from the client headers.
        clientip = cherrypy.request.headers['Remote-Addr']
        logging.debug("Client's IP address: %s", clientip)
        logging.debug("Value of status is: %s", status)
        logging.debug("Value of message is: %s", message)
        logging.debug("Value of traceback is: %s", traceback)
        logging.debug("Value of version is: %s", version)

        # Assemble some HTML to redirect the client to the captive portal's
        # /index.html-* page.
        #
        # We are using wlan0:1 here for the address - this may or may not be
        # right, but since we can't get at self.args.address it's the best we
        # can do.
        redirect = """<html><head><meta http-equiv="refresh" content="0; url=http://""" + get_ip_address("wlan0:1") + """/" /></head> <body></body> </html>"""

        logging.debug("Generated HTML refresh is:")
        logging.debug(redirect)
        logging.debug("Redirecting client to /.")

        # Fire the redirect at the client.
        return redirect
    cherrypy.config.update({'error_page.404':error_page_404})


def parse_args():
    parser = argparse.ArgumentParser(conflict_handler='resolve', description="This daemon implements the captive "
                                     "portal functionality of Byzantium Linux. pecifically, it acts as the front end "
                                     "to IP tables and automates the addition of mesh clients to the whitelist.")
    parser.add_argument("-a", "--address", action="store",
                        help="The IP address of the interface the daemon listens on.")
    parser.add_argument("--appconfig", action="store", default="/etc/captiveportal/captiveportal.conf")
    parser.add_argument("--cachedir", action="store", default="/tmp/portalcache")
    parser.add_argument("-c", "--certificate", action="store", default="/etc/httpd/server.crt",
                        help="Path to an SSL certificate. (Defaults to /etc/httpd/server.crt)")
    parser.add_argument("--configdir", action="store", default="/etc/captiveportal")
    parser.add_argument("-d", "--debug", action="store_true", default=False, help="Enable debugging mode.")
    parser.add_argument("--filedir", action="store", default="/srv/captiveportal")
    parser.add_argument("-i", "--interface", action="store", required=True,
                        help="The name of the interface the daemon listens on.")
    parser.add_argument("-k", "--key", action="store", default="/etc/httpd/server.key",
                        help="Path to an SSL private key file. (Defaults to /etc/httpd/server.key)")
    parser.add_argument("--pidfile", action="store")
    parser.add_argument("-p", "--port", action="store", default=31337, type=int,
                        help="Port to listen on.  Defaults to 31337/TCP.")
    parser.add_argument("-s", "--sslport", action="store", default=31338, type=int,
                        help="Port to listen for HTTPS connections on. (Defaults to HTTP port +1.")
    parser.add_argument("-t", "--test", action="store_true", default=False,
                        help="Disables actually doing anything, it just prints what would be done.  Used for testing "
                        "commands without altering the test system.")
    return parser.parse_args()


def check_args(args):
    if not args.port == 31337 and args.sslport == 31338:
        args.sslport = args.port + 1
        logging.debug("Setting ssl port to %d/TCP", args.sslport)

    if not os.path.exists(args.certificate):
        if not args.test:
            logging.error("Specified SSL cert not found: %s", args.certificate)
            exit(2)
    else:
        logging.debug("Using SSL cert at: %s", args.certificate)

    if not os.path.exists(args.key):
        if not args.test:
            logging.error("Specified SSL private key not found: %s", args.key)
            exit(2)
    else:
        logging.debug("Using SSL private key at: %s", args.key)

    if not args.configdir == "/etc/captiveportal" and args.appconfig == "/etc/captiveportal/captiveportal.conf":
        args.appconfig = "%s/captiveportal.conf" % args.configdir

    if args.debug:
        print "Captive portal debugging mode is on."

    if args.test:
        print "Captive portal functional testing mode is on."

    return args


def create_pidfile(args):
    # Create the filename for this instance's PID file.
    if not args.pidfile:
        if args.test:
            args.pidfile = '/tmp/captive_portal.'
        else:
            args.pidfile = '/var/run/captive_portal.'
    full_pidfile = args.pidfile + args.interface
    logging.debug("Name of PID file is: %s", full_pidfile)

    # If a PID file already exists for this network interface, ABEND.
    if os.path.exists(full_pidfile):
        logging.error("A pidfile already exists for network interface %s.", full_pidfile)
        logging.error("Is a daemon already running on this interface?")
        exit(5)

    # Write the PID file of this instance to the PID file.
    logging.debug("Creating pidfile for network interface %s.", str(args.interface))
    logging.debug("PID of process is %s.", str(os.getpid()))
    pid = PIDFile(cherrypy.engine, full_pidfile)
    pid.subscribe()


def update_cherrypy_config(port):
    # Configure a few things about the web server so we don't have to fuss
    # with an extra config file, namely, the port and IP address to listen on.
    cherrypy.config.update({'server.socket_host':'0.0.0.0', })
    cherrypy.config.update({'server.socket_port':port, })


def start_ssl_listener(args):
    # Set up an SSL listener running in parallel.
    ssl_listener = cherrypy._cpserver.Server()
    ssl_listener.socket_host = '0.0.0.0'
    ssl_listener.socket_port = args.sslport
    ssl_listener.ssl_certificate = args.certificate
    ssl_listener.ssl_private_key = args.key
    ssl_listener.subscribe()


def build_templatelookup(args):
    # Set up the location the templates will be served out of.
    return TemplateLookup(directories=[args.filedir], module_directory=args.cachedir, collection_size=100)


def setup_url_tree(args):
    # Attach the captive portal object to the URL tree.
    root = CaptivePortal(args)

    # Mount the object for the root of the URL tree, which happens to be the
    # system status page.  Use the application config file to set it up.
    logging.debug("Mounting web app in %s to /.", args.appconfig)
    cherrypy.tree.mount(root, "/", args.appconfig)


def setup_iptables(args):
    # Initialize the IP tables ruleset for the node.
    initialize_iptables = ['/usr/local/sbin/captive-portal.sh', 'initialize',
                           args.address, args.interface]
    iptables = 0
    if args.test:
        logging.debug("Command that would be executed:\n%s", ' '.join(initialize_iptables))
    else:
        iptables = subprocess.call(initialize_iptables)
    return iptables


def setup_reaper(test):
    # Start up the idle client reaper daemon.
    idle_client_reaper = ['/usr/local/sbin/mop_up_dead_clients.py', '-m', '600',
                          '-i', '60']
    reaper = 0
    if test:
        logging.debug("Idle client monitor command that would be executed:\n%s", ' '.join(idle_client_reaper))
    else:
        logging.debug("Starting mop_up_dead_clients.py.")
        reaper = subprocess.Popen(idle_client_reaper)
    if not reaper:
        logging.error("mop_up_dead_clients.py did not start.")


def setup_hijacker(args):
    # Start the fake DNS server that hijacks every resolution request with the
    # IP address of the client interface.
    dns_hijacker = ['/usr/local/sbin/fake_dns.py', args.address]
    hijacker = 0
    if args.test:
        logging.debug("Command that would start the fake DNS server:\n%s", ' '.join(dns_hijacker))
    else:
        logging.debug("Starting fake_dns.py.")
        hijacker = subprocess.Popen(dns_hijacker)
    if not hijacker:
        logging.error("fake_dns.py did not start.")


def check_ip_tables(iptables, args):
    # Now do some error checking in case IP tables went pear-shaped.  This appears
    # oddly specific, but /usr/sbin/iptables treats these two kinds of errors
    # differently and that makes a difference during troubleshooting.
    if iptables == 1:
        logging.error("Unknown IP tables error during firewall initialization.")
        logging.error("Packet filters NOT configured.  Examine the rules in captive-portal.sh.")
        exit(3)

    if iptables == 2:
        logging.error("Invalid or incorrect options passed to iptables in captive-portal.sh")
        logging.error("Packet filters NOT configured.  Examine the rules in captive-portal.sh.")
        logging.error("Parameters passed to captive-portal.sh: initialize, %s, %s" % args.address, args.interface)
        exit(4)


def start_web_server():
    # Start the web server.
    logging.debug("Starting web server.")
    cherrypy.engine.start()
    cherrypy.engine.block()
    # [Insert opening anthem from Blaster Master here.]
    # Fin.


def main():
    args = check_args(parse_args())
    if args.debug:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.ERROR)
    create_pidfile(args)
    update_cherrypy_config(args.port)
    start_ssl_listener(args)
    setup_url_tree(args)
    iptables = setup_iptables(args)
    setup_reaper(args.test)
    setup_hijacker(args)
    check_ip_tables(iptables, args)
    start_web_server()


if __name__ == "__main__":
    main()
