# services.py - Lets the user start and stop web applications and daemons
#    running on their Byzantium node.

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# TODO:

# Import external modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
from mako.exceptions import RichTraceback
import sqlite3
import os
import time
import subprocess

from mako import exceptions

# Import core control panel modules.
from control_panel import *

# Classes.
# Allows the user to configure to configure mesh networking on wireless network
# interfaces.
class Services(object):
    # Database used to store states of services and webapps.
    #servicedb = '/var/db/controlpanel/services.sqlite'
    servicedb = '/home/drwho/services.sqlite'

    # Class attributes.  By default they are blank but will be populated from
    # the services.sqlite database.

    # Pretends to be index.html.
    def index(self):
        # Set up the strings that will hold the HTML for the tables on this
        # page.
        webapps = ""
        systemservices = ""

        # Set up access to the system services database.  We're going to need
        # to read successive lines from it to build the HTML tables.
        error = ''
        connection = sqlite3.connect(self.servicedb)
        cursor = connection.cursor()

        # Use the contents of the services.webapps table to build an HTML
        # table of buttons that are either go/no-go indicators.  It's
        # complicated, so I'll break it down into smaller pieces.
        cursor.execute("SELECT name, status FROM webapps;")
        results = cursor.fetchall()
        if not results:
            # Display an error page that says that something went wrong.
            error = "<p>ERROR: Something went wrong in database " + this.servicedb + ", table webapps.  SELECT query failed.</p>"
        else:
            # Roll through the list returned by the SQL query.
            for (name, status) in results:
                webapp_row = ''

                # Set up the first cell in the row, the name of the webapp.
                if status == 'active':
                    # White on green means that it's active.
                    webapp_row = webapp_row + "<td style='background-color:green; color:white;' >" + name + "</td>"
                else:
                    # White on red means that it's not active.
                    webapp_row = webapp_row + "<td style='background-color:red; color:white;' >" + name + "</td>"

                # Set up the second cell in the row, the toggle that will
                # either turn the web app off, or turn it on.
                if status == 'active':
                    # Give the option to deactivate the app.
                    webapp_row = webapp_row + "<td><input type='submit' name='app' value='" + name + "' style='background-color:red; color:white;' ></td>"
                else:
                    # Give the option to activate the app.
                    webapp_row = webapp_row + "<td><input type='submit' name='app' value='" + name + "' style='background-color:green; color:white;' ></td>"

                # Finish off the row in that table.
                webapp_row = webapp_row + "\n"

                # Add that row to the buffer of HTML for the webapp table.
                webapps = webapps + webapp_row

        # Do the same thing for system services, only call this.services().

        # Gracefully detach the system services database.
        cursor.close()

        print "DEBUG: " + webapps

        # Render the HTML page.
        try:
            page = templatelookup.get_template("/services/index.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = "Manipulate services",
                               error = error, webapps = webapps)
                               #systemservices = systemservices)
        except:
            #traceback = RichTraceback()
            #for (filename, lineno, function, line) in traceback.traceback:
            #    print "\n"
            #    print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
            #    print "Execution died on line %s\n" % line
            #    print "%s: %s" % (str(traceback.error.__class__.__name__),
            #        traceback.error)

            # Holy crap, this is a better exception analysis method than the
            # one above, because it actually prints useful information to the
            # web browser, rather than forcing you to figure it out from stderr.
            # I might have to start using this more.
            return exceptions.html_error_template().render()
    index.exposed = True

    # Handler for changing the state of a web app.  This method is only called
    # when the user wants to toggle the state of the app, so it looks in the
    # configuration database and switches 'enabled' to 'disabled' or vice versa
    # depending on what it finds.
    #def webapps(self, app=None):

    #webapps.exposed = True


    # Handler for changing the state of a system service.  This method is also
    # only called when the user wants to toggle the state of the app, so it
    # looks in the configuration database and switches 'enabled' to 'disabled'
    # or vice versa depending on what it finds.
    #def services(self, service=None):

    #services.exposed = True
