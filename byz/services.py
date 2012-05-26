#!/usr/bin/env python

# services.py
# A CGI-BIN script that reads the database of services running on the node
# and outputs them to a web browser in the form of a dynamically generated
# HTML page.  This script was written over lunch at CarolinaCon 8 for a demo
# which Murphy handily took over at the last minute.

# By: Haxwithaxe (me at haxwithaxe dot net)

# Import Python modules.
import os
import sys
import getopt
import sqlite3

# Global variables.
debug = False
test = False
output = ''

# Define the location of the service database.  This may be redefined later
# depending upon whether or not command line args are passed to this script.
servicedb = '/var/db/controlpanel/services.sqlite'

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
        print "Service directory debugging mode is on."
    if opt in ('-t', '--test'):
        test = True
        servicedb = '/home/drwho/services.sqlite'
        print "Service directory functional testing mode is on."

# Display some basic debugging information so we know what we're looking at.
if debug:
    print "DEBUG: Services database referenced: %s" % servicedb

# Generate the HTML header to return to the client.
print('Content-type: text/html\n\n')
print('<html><head><title>Mesh Services</title><head/>\n<body>\n<ul>\n')

# Get the list of services running on this node from the database.  Start by
# setting up a connection to the database.
if debug:
    print "DEBUG: Opening service database."
connection = sqlite3.connect(servicedb)
cursor = connection.cursor()

# Pull a list of running web apps on the node.
if debug:
    print "DEBUG: Getting list of running webapps from database."
cursor.execute("SELECT name,location FROM webapps WHERE status='active';")
results = cursor.fetchall()
if results:
    for service in results:
        line = '<li><a href="'
        if test:
            line = line + 'https://localhost'
        else:
            line = line + os.environ['SERVER_PROTOCOL'] + '://'
            line = line + os.environ['SERVER_NAME']

        # Detect and remove explicit references to port 80 or 443 because
        # they'll hose the links.  Trying to throw HTTPS at port 80 doesn't
        # work.
        servicename = ''
        if '80/' in service[1]:
            servicename = service[1].strip('80')
        elif '443/' in service[1]:
            servicename = service[1].strip('443')
        else:
            servicename = ':/' + service[1]
        line = line + servicename + '">' + service[0]
        line = line + '</a></li>\n'
        print line

# Generate a list of daemons running on the node.
if debug:
    print "DEBUG: Getting list of running system services from database."
cursor.execute("SELECT name,port FROM daemons WHERE status='active';")
results = cursor.fetchall()
if results:
    for service in results:
        line = '<li><a href="'
        if test:
            line = line + 'http://localhost'
        else:
            line = line + os.environ['SERVER_PROTOCOL'] + '://'
            line = line + os.environ['SERVER_NAME']

        # Detect and remove explicit references to port 80 or 443 because
        # they'll hose the links.  Trying to throw HTTPS at port 80 doesn't
        # work.
        servicename = ':' + str(service[1]) + '/'
        line = line + servicename + '">' + service[0]
        line = line + '</a></li>\n'
        print line

# Print the HTML footer that gets sent to the client by the webserver.
print('</ul>\n</body></html>')

# Clean up after ourselves.
if debug:
    print "DEBUG: Closing service database."
cursor.close()

# Fin.
