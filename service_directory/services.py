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
print('<html><head><title>Mesh Services</title><head/>\n<body>\n')
print 'Services running on this node:\n<ul>\n'

# Get the list of services running on this node from the database.  Start by
# setting up a connection to the database.
if debug:
    print "DEBUG: Opening service database."
connection = sqlite3.connect(servicedb)
cursor = connection.cursor()

# Pull a list of running web apps on the node.
if debug:
    print "DEBUG: Getting list of running webapps from database."
cursor.execute("SELECT name FROM webapps WHERE status='active';")
results = cursor.fetchall()
if results:
    for service in results:
        line = '<li><a href="'
        if test:
            line = line + 'https://localhost/'
	else:
            # This isn't the most elegant way to detect whether or not HTTPS
            # was used to contact the server but it's either that or parse
            # URIs.
            if 'HTTPS' in os.environ:
                line = line + 'https://'
            else:
                line = line + 'http://'

        # Populate the location-aware bit of the hyperlink.
        line = line + os.environ['SERVER_NAME'] +  '/'
        line = line + service[0] + '/">' + service[0]
        line = line + '</a></li>\n'
        print line

# Now pull a list of daemons running on the node.  This means that most of the
# web apps users will access will be displayed.
if debug:
    print "DEBUG: Getting list of running servers from database."
cursor.execute("SELECT name,showtouser FROM daemons WHERE status='active';")
results = cursor.fetchall()
if results:
    for service in results:
        if debug:
            print "DEBUG: Value of service: %s" % service

        # Test to see if the daemon is one that can be shown to the user.  If
        # it's not, skip to the next iteration.
        if service[1] == 'no':
            if debug:
                print "DEBUG: This daemon won't be shown to the user."
            continue

        # Begin setting up a hyperlink that the user will see.
        line = '<li><a href="'
        if test:
            line = line + 'https://localhost/'
	else:
            # This isn't the most elegant way to detect whether or not HTTPS
            # was used to contact the server but it's either that or parse
            # URIs.
            if 'HTTPS' in os.environ:
                line = line + 'https://'
            else:
                line = line + 'http://'

        # Populate the location-aware bit of the hyperlink.
        line = line + os.environ['SERVER_NAME'] +  '/'
        line = line + service[0] + '/">' + service[0]
        line = line + '</a></li>\n'

        # Display the line to the user.
        print line

# Print the HTML footer that gets sent to the client by the webserver.
print('</ul>\n</body></html>')

# Clean up after ourselves.
if debug:
    print "DEBUG: Closing service database."
cursor.close()

# Fin.
