# services.py - Lets the user start and stop web applications and daemons
#    running on their Byzantium node.

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# TODO:
# - List the initscripts in a config file to make them easier to edit?
# - Come up with a method for determining whether or not a system service was
#   successfully deactivated.  Not all services have initscripts, and of those
#   that do, not all of them put PID files in /var/run/<nameofapp>.pid.

# Import external modules.
from mako import exceptions

import logging
import sqlite3
import subprocess

import _utils


# Classes.
# Allows the user to configure to configure mesh networking on wireless network
# interfaces.
class Services(object):
    
    def __init__(self, templatelookup, test):
        self.templatelookup = templatelookup
        self.test = test
    
        # Database used to store states of services and webapps.
        
        if self.test:
            self.servicedb = 'var/db/controlpanel/services.sqlite'
        else:
            self.servicedb = '/var/db/controlpanel/services.sqlite'
            # self.servicedb = '/home/drwho/services.sqlite'

        # Static class attributes.
        self.pid = '/var/run/httpd/httpd.pid'

        # These attributes will be used as scratch variables to keep from running
        # the same SQL queries over and over again.
        self.app = ''
        self.status = ''
        self.initscript = ''

    def generate_rows(self, results, kind):
        # Set up the opening tag of the table.
        row = '<tr>'

        # Roll through the list returned by the SQL query.
        for (name, status) in results:
            # Set up the first cell in the row, the name of the webapp.
            if status == 'active':
                # White on green means that it's active.
                row += "<td style='background-color:green; color:white;' >%s</td>" % name
            else:
                # White on red means that it's not active.
                row += "<td style='background-color:red; color:white;' >%s</td>" % name

            # Set up the second cell in the row, the toggle that will either
            # turn the web app off or on.
            if status == 'active':
                # Give the option to deactivate the app.
                row += "<td><button type='submit' name='%s' value='%s' style='background-color:red; color:white;' >Deactivate</button></td>" % (kind, name)
            else:
                # Give the option to activate the app.
                row += "<td><button type='submit' name='%s' value='%s' style='background-color:green; color:white;' >Activate</button></td>" % (kind, name)

            # Set the closing tag of the row.
            row += "</tr>\n"

        # Add that row to the buffer of HTML for the webapp table.
        return row

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
            error = "<p>ERROR: Something went wrong in database %s, table webapps.  SELECT query failed.</p>" % self.servicedb
        else:
            webapps = self.generate_rows(results, 'app')

        # Do the same thing for system services.
        cursor.execute("SELECT name, status FROM daemons;")
        results = cursor.fetchall()
        if not results:
            # Display an error page that says that something went wrong.
            error = "<p>ERROR: Something went wrong in database %s, table daemons.  SELECT query failed.</p>" % self.servicedb
        else:
            systemservices = self.generate_rows(results, 'service')

        # Gracefully detach the system services database.
        cursor.close()

        # Render the HTML page.
        try:
            page = self.templatelookup.get_template("/services/index.html")
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

        query = "SELECT name, status FROM webapps WHERE name=?;"
        template = (self.app, )
        _, cursor = _utils.execute_query(self.servicedb, query, template=template)
        result = cursor.fetchall()
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
            page = self.templatelookup.get_template("/services/webapp.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = (action + " service"),
                               app = app, action = action, warning = warning)
        except:
            return exceptions.html_error_template().render()
    webapps.exposed = True

    # The method that updates the services.sqlite database to flag a given web
    # application as accessible to mesh users or not.  Takes one argument, the
    # name of the app.
    def toggle_webapp(self, action=None):
        # Set up a generic error catching variable for this page.
        error = ''

        if action == 'activate':
            status = 'active'
            action = 'activated'
        else:
            status = 'disabled'
            action = 'deactivated'

        query = "UPDATE webapps SET status=? WHERE name=?;"
        template = template = (status, self.app, )
        database, cursor = _utils.execute_query(self.servicedb, query, template=template)
        database.commit()
        cursor.close()

        # Render the HTML page and send it to the browser.
        try:
            page = self.templatelookup.get_template("/services/toggled.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = "Service toggled.",
                               error = error, app = self.app,
                               action = action)
        except:
            return exceptions.html_error_template().render()
    toggle_webapp.exposed = True

    # Handler for changing the state of a system service.  This method is also
    # only called when the user wants to toggle the state of the app, so it
    # looks in the configuration database and switches 'enabled' to 'disabled'
    # or vice versa depending on what it finds.
    def services(self, service=None):
        # Save the name of the app in a class attribute to save effort later.
        self.app = service

        query = "SELECT name, status, initscript FROM daemons WHERE name=?;"
        template = (service, )
        _, cursor = _utils.execute_query(self.servicedb, query, template=template)
        result = cursor.fetchall()
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
            page = self.templatelookup.get_template("/services/services.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = (action + " service"),
                               action = action, app = service, warning = warning)
        except:
            return exceptions.html_error_template().render()
    services.exposed = True

    # The method that does the actual work of running initscripts located in
    # /etc/rc.d and starting or stopping system services.  Takes one argument,
    # the name of the app.  This should never be called from anywhere other than
    # Services.services().
    def toggle_service(self, action=None):
        # Set up an error handling variable just in case.
        error = ''
        query = "SELECT name, initscript FROM daemons WHERE name=?;"
        template = template = (self.app, )
        database, cursor = _utils.execute_query(self.servicedb, query, template=template)
        results = cursor.fetchall()
        self.initscript = results[0][1]

        if action == 'activate':
            status = 'active'
        else:
            status = 'disabled'

        # Construct the command line ahead of time to make the code a bit
        # simpler in the long run.
        initscript = '/etc/rc.d/' + self.initscript
        if self.status == 'active':
            if self.test:
                logging.debug('Would run "%s stop" here.' % initscript)
            else:
                subprocess.Popen([initscript, 'stop'])
        else:
            if self.test:
                logging.debug('Would run "%s start" here.' % initscript)
            else:
                subprocess.Popen([initscript, 'start'])

        # Update the status of the service in the database.
        template = (status, self.app, )
        cursor.execute("UPDATE daemons SET status=? WHERE name=?;", template)
        database.commit()
        cursor.close()

        # Render the HTML page and send it to the browser.
        try:
            page = self.templatelookup.get_template("/services/toggled.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = "Service toggled.",
                               app = self.app, action = action, error = error)
        except:
            return exceptions.html_error_template().render()
    toggle_service.exposed = True
