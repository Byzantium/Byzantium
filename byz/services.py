#!/usr/bin/env python

# services.py
# A CGI-BIN script that reads the database of services running on the node
# and outputs them to a web browser in the form of a dynamically generated
# HTML page.  This script was written over lunch at CarolinaCon 8 for a demo
# which Murphy handily took over at the last minute.

# By: Haxwithaxe (me at haxwithaxe dot net)

# Import Python modules.
import os

# Global variables.
service_list_file = '/etc/byzantium/services.csv'

# Define methods.
# file2str(): Helper method that reads a CSV file in /etc/byzantium containing
#    a static list of services and reads them into a string.  Takes two args,
#    a filename and an access mode (defaults to 'read'), returns a string
#    containing the contents of a CSV file.
def file2str(fname, mode='r'):
    fobj = open(fname,mode)
    fstr = fobj.read()
    fobj.close()
    return fstr

# get_services(): Takes the list of services the node knows about and turns
#    them into a list of hyperlinks so the user can click on one to visit it.
#    Takes no arguments.  Returns a string containing HTML code.
def get_services():
    output = ''
    fstr = file2str(service_list_file)
    for srv in fstr.strip().split('\n'):
        port, name, desc = srv.strip().split(',',2)
        output += '<a href="'+os.environ['SERVER_PROTOCOL'].split('/')[0]+'://'+os.environ['SERVER_NAME']+':'+port+'">'+name+'</a> '+desc+'<br/>'
    return output

# Core code.
# Generate the HTML header to return to the client.
print('Content-type: text/html\n\n')
print('<html><head><title>Mesh Services</title><head/><body>')

# Print the list of services the node knows about.
print(get_services())

# Print the HTML footer that gets sent to the client by the webserver.
print('</body></html>')

# Fin.
