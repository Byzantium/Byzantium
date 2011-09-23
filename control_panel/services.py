# services.py - Lets the user start and stop web applications and daemons
#    running on their Byzantium node.

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# TODO:
# - Make it so that the toggle buttons in Services.index() don't display the name
#   of the service again, but 'enable' or 'disable' as appropriate.
# - In Services.toggle_webapps(), if there is an error give the user the option
#   to un-do the last change, forcibly kill Apache (just in case), clear out the
#   PID file, and start Apache to get it back into a consistent state.  This
#   should go into an error handler method (Services.apache_fixer()) with its
#   own HTML file.

# Import external modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
from mako import exceptions
import sqlite3
import os
import subprocess
import shutil

# Import core control panel modules.
from control_panel import *

# Classes.
# Allows the user to configure to configure mesh networking on wireless network
# interfaces.
class Services(object):
    # Database used to store states of services and webapps.
    #servicedb = '/var/db/controlpanel/services.sqlite'
    servicedb = '/home/drwho/services.sqlite'

    # Static class attributes.
    enabled_configs = '/etc/httpd/enabled_apps'
    disabled_configs = '/etc/httpd/disabled_apps'
    pid = '/var/run/httpd/httpd.pid'

    # These attributes will be used as scratch variables to keep from running the
    # same SQL queries over and over again.
    app = ''
    status = ''
    initscript = ''

    # Pretends to be index.html.
    def index(self):
        # Set up the strings that will hold the HTML for the tables on this
        # page.
        webapps = ''
        systemservices = ''

        # Set up access to the system services database.  We're going to need
        # to read successive lines from it to build the HTML tables.
        error = ''
        connection = sqlite3.connect(self.servicedb)
        cursor = connection.cursor()

        # Use the contents of the services.webapps table to build an HTML table
        # of buttons that are either go/no-go indicators.  It's a bit 
        # complicated, so I'll break it into smaller pieces.
        cursor.execute("SELECT name, status FROM webapps;")
        results = cursor.fetchall()
        if not results:
            # Display an error page that says that something went wrong.
            error = "<p>ERROR: Something went wrong in database " + this.servicedb + ", table webapps.  SELECT query failed.</p>"
        else:
            # Set up the opening tag of the table.
            webapp_row = '<tr>'

            # Roll through the list returned by the SQL query.
            for (name, status) in results:
                # Set up the first cell in the row, the name of the webapp.
                if status == 'active':
                    # White on green means that it's active.
                    webapp_row = webapp_row + "<td style='background-color:green; color:white;' >" + name + "</td>"
                else:
                    # White on red means that it's not active.
                    webapp_row = webapp_row + "<td style='background-color:red; color:white;' >" + name + "</td>"

                # Set up the second cell in the row, the toggle that will either
                # turn the web app off or on.
                if status == 'active':
                    # Give the option to deactivate the app.
                    webapp_row = webapp_row + "<td><input type='submit' name='app' value='" + name + "' style='background-color:red; color:white;' title='deactivate' ></td>"
                else:
                    # Give the option to activate the app.
                    webapp_row = webapp_row + "<td><input type='submit' name='app' value='" + name + "' style='background-color:green; color:white;' title='activate' ></td>"

                # Set the closing tag of the row.
                webapp_row = webapp_row + "</tr>\n"

            # Add that row to the buffer of HTML for the webapp table.
            webapps = webapps + webapp_row

        # Do the same thing for system services.
        cursor.execute("SELECT name, status FROM daemons;")
        results = cursor.fetchall()
        if not results:
            # Display an error page that says that something went wrong.
            error = "<p>ERROR: Something went wrong in database " + this.servicedb + ", table daemons.  SELECT query failed.</p>"
        else:
            # Set up the opening tag of the table.
            services_row = '<tr>'

            # Roll through the list returned by the SQL query.
            for (name, status) in results:
                # Set up the first cell in the row, the name of the webapp.
                if status == 'active':
                    # White on green means that it's active.
                    services_row = services_row + "<td style='background-color:green; color:white;' >" + name + "</td>"
                else:
                    # White on red means that it's not active.
                    services_row = services_row + "<td style='background-color:red; color:white;' >" + name + "</td>"

                # Set up the second cell in the row, the toggle that will either
                # turn the web app off or on.
                if status == 'active':
                    # Give the option to deactivate the app.
                    services_row = services_row + "<td><input type='submit' name='service' value='" + name + "' style='background-color:red; color:white;' title='deactivate' ></td>"
                else:
                    # Give the option to activate the app.
                    services_row = services_row + "<td><input type='submit' name='service' value='" + name + "' style='background-color:green; color:white;' title='activate' ></td>"

                # Set the closing tag of the row.
                services_row = services_row + "</tr>\n"

            # Add that row to the buffer of HTML for the webapp table.
            systemservices = systemservices + services_row

        # Gracefully detach the system services database.
        cursor.close()

        # Render the HTML page.
        try:
            page = templatelookup.get_template("/services/index.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = "Manipulate services",
                               error = error, webapps = webapps,
                               systemservices = systemservices)
        except:
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
    def webapps(self, app=None):
        # Save the name of the app in a class attribute to save effort later.
        self.app = app

        # Set up a connection to the services.sqlite database.
        database = sqlite3.connect(self.servicedb)
        cursor = database.cursor()

        # Search the webapps table of the services.sqlite database for the name
        # of the app that was passed to this method.  Note the status attached
        # to the name.
        template = (self.app, )
        cursor.execute("SELECT name, status FROM webapps WHERE name=?;", template)
        result = cursor.fetchall()
        name = result[0][0]
        status = result[0][1]

        # Save the status of the app in another class attribute for later.
        self.status = status

        # Determine what to do.
        if status == 'active':
            action = 'deactivate'
            warning = 'This will deactivate the application!'
        else:
            action = 'activate'
            warning = 'This will activate the application!'

        # Close the connection to the database.
        cursor.close()

        # Display to the user the page that asks them if they really want to
        # shut down that app.
        try:
            page = templatelookup.get_template("/services/webapp.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = (action + " service"),
                               app = app, action = action, warning = warning)
        except:
            return exceptions.html_error_template().render()
    webapps.exposed = True

    # The method that does the actual heavy lifting of moving an Apache sub-config
    # file into or out of /etc/httpd/conf/conf.d.  Takes one argument, the name
    # of the app.  This should never be called from anywhere other than
    # Services.webapps().
    def toggle_webapp(self, app=None):
        # Set up a connection to the services.sqlite database.
        database = sqlite3.connect(self.servicedb)
        cursor = database.cursor()

        if self.status == 'active':
            # Copy the Apache sub-config file into the right location.
            shutil.copyfile((self.disabled_configs + '/' + app + '.conf'),
                            (self.enabled_configs + '/' + app + '.conf'))
            status = self.status
            action = 'activated'
        else:
            # Delete the Apache sub-config file from the enable_apps/ directory.
            if os.path.exists():
                os.remove(self.enabled_configs + '/' + app + '.conf')
            status = 'disabled'
            action = 'deactivated'

        # Restart Apache.
        output = subprocess.Popen(['/etc/rc.d/rc.httpd', 'graceful'])

        # Make sure Apache came back up, and if it did update the database.
        if os.path.exists(self.pid):
            error = ''
            template = (status, self.app, )
            cursor.execute("UPDATE webapps SET status=? WHERE name=?;", template)
        else:
            error = "<p>WARNING: It doesn't look like Apache came back up.  Something went wrong!</p>"

        # Detach the system services database.
        cursor.close()

        # Render the HTML page and send it to the browser.
        try:
            page = templatelookup.get_template("/services/toggled.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = "Service toggled.", app = app,
                               action = action, error = error)
        except:
            return exceptions.html_error_template().render()
    toggle_webapp.exposed = True

    # Handler for changing the state of a system service.  This method is also
    # only called when the user wants to toggle the state of the app, so it
    # looks in the configuration database and switches 'enabled' to 'disabled'
    # or vice versa depending on what it finds.
    def services(self, service=None):
        print "DEBUG: Entered Services.services()."

        # Save the name of the app in a class attribute to save effort later.
        self.app = service

        # Set up a connection to the services.sqlite database.
        database = sqlite3.connect(self.servicedb)
        cursor = database.cursor()

        # Search the daemons table of the services.sqlite database for the name
        # of the app passed to this method.  Note the status and name of the
        # initscript attached to the name.
        template = (service, )
        cursor.execute("SELECT name, status, initscript FROM daemons WHERE name=?;", template)
        result = cursor.fetchall()
        name = result[0][0]
        status = result[0][1]
        initscript = result[0][2]

        # Save the status of the app and the initscript in class attributes for
        # later use.
        self.status = status
        self.initscript = initscript

        # Figure out what to do.
        if status == 'active':
            action = 'deactivate'
            warning = 'This will deactivate the application!'
        else:
            action = 'activate'
            warning = 'This will activate the application!'

        # Close the connection to the database.
        cursor.close()

        # Display to the user the page that asks them if they really want to
        # shut down that app.
        try:
            page = templatelookup.get_template("/services/services.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = (action + " service"),
                               action = action, app = service, warning = warning)
        except:
            return exceptions.html_error_template().render()
    services.exposed = True
