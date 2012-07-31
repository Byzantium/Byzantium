# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

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
import subprocess

import models.daemon
import models.state
import models.webapp


class Error(Exception):
    pass


def generate_rows(results, kind):
    # Set up the opening tag of the table.
    row = '<tr>'

    # Roll through the list returned by the SQL query.
    for result in results:
        # Set up the first cell in the row, the name of the webapp.
        if result.status == 'active':
            # White on green means that it's active.
            row += "<td style='background-color:green; color:white;' >%s</td>" % result.name
        else:
            # White on red means that it's not active.
            row += "<td style='background-color:red; color:white;' >%s</td>" % result.name

        # Set up the second cell in the row, the toggle that will either
        # turn the web app off or on.
        if result.status == 'active':
            # Give the option to deactivate the app.
            row += "<td><button type='submit' name='%s' value='%s' style='background-color:red; color:white;' >Deactivate</button></td>" % (kind, result.name)
        else:
            # Give the option to activate the app.
            row += "<td><button type='submit' name='%s' value='%s' style='background-color:green; color:white;' >Activate</button></td>" % (kind, result.name)

        # Set the closing tag of the row.
        row += "</tr>\n"

    # Add that row to the buffer of HTML for the webapp table.
    return row


# Classes.
# Allows the user to configure to configure mesh networking on wireless network
# interfaces.
class Services(object):
    
    def __init__(self, templatelookup, test):
        self.templatelookup = templatelookup
        self.test = test

        # Static class attributes.
        self.pid = '/var/run/httpd/httpd.pid'

        # These attributes will be used as scratch variables to keep from running
        # the same SQL queries over and over again.
        self.app = ''
        self.status = ''
        self.init_script = ''
        
        if self.test:
            self.service_state = models.state.ServiceState('var/db/controlpanel/services.sqlite')
        else:
            self.service_state = models.state.ServiceState('/var/db/controlpanel/services.sqlite')

    # Pretends to be index.html.
    def index(self):
        # Set up the strings that will hold the HTML for the tables on this
        # page.
        webapps = ''
        systemservices = ''

        results = self.service_state.list('webapps', models.webapp.WebApp)
        if not results:
            # Display an error page that says that something went wrong.
            error = "<p>ERROR: Something went wrong in database %s, table webapps.  SELECT query failed.</p>" % self.service_state.db_path
        else:
            webapps = generate_rows(results, 'app')

        results = self.service_state.list('daemons', models.daemon.Daemon)
        if not results:
            # Display an error page that says that something went wrong.
            error = "<p>ERROR: Something went wrong in database %s, table daemons.  SELECT query failed.</p>" % self.service_state.db_path
        else:
            systemservices = generate_rows(results, 'service')

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

    def _fetch_webapp(self, name):
        results = self.service_state.list('webapps', models.webapp.WebApp, {'name': name})
        if len(results) == 0:
            raise Error("Found no webapps with name: %s" % name)
        elif len(results) > 1:
            raise Error("Found too many webapps with name: %s" % name)
        else:
            return results[0]
            
    def _fetch_daemon(self, name):
        results = self.service_state.list('daemons', models.daemon.Daemon, {'name': name})
        if len(results) == 0:
            raise Error("Found no daemons with name: %s" % name)
        elif len(results) > 1:
            raise Error("Found too many daemons with name: %s" % name)
        else:
            return results[0]

    # Handler for changing the state of a web app.  This method is only called
    # when the user wants to toggle the state of the app, so it looks in the
    # configuration database and switches 'enabled' to 'disabled' or vice versa
    # depending on what it finds.
    def webapps(self, app=None):
        warning = ''
        try:
            result = self._fetch_webapp(app)
            # Save the name of the app in a class attribute to save effort later.
            self.app = app

            status = result.status

            # Save the status of the app in another class attribute for later.
            self.status = status

            # Determine what to do.
            if status == 'active':
                action = 'deactivate'
                warning = 'This will deactivate the application!'
            else:
                action = 'activate'
                warning = 'This will activate the application!'
        except Error as ex:
            warning = str(ex)
        
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

        try:
            result = self._fetch_webapp(self.app)
            result.status = status
            
        except Error as ex:
            error = str(ex)

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

        warning = ''
        try:
            result = self._fetch_daemon(self.app)
            status = result.status
            init_script = result.init_script

            # Save the status of the app and the initscript in class attributes for
            # later use.
            self.status = status
            self.init_script = init_script

            # Figure out what to do.
            if status == 'active':
                action = 'deactivate'
                warning = 'This will deactivate the application!'
            else:
                action = 'activate'
                warning = 'This will activate the application!'
        except Error as ex:
            warning = str(ex)
        
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
        error = ''
        try:
            result = self._fetch_daemon(self.app)
            self.init_script = result.init_script

            if action == 'activate':
                status = 'active'
            else:
                status = 'disabled'

            # Construct the command line ahead of time to make the code a bit
            # simpler in the long run.
            init_script = '/etc/rc.d/' + self.init_script
            if self.status == 'active':
                if self.test:
                    logging.debug('Would run "%s stop" here.', init_script)
                else:
                    subprocess.Popen([init_script, 'stop'])
            else:
                if self.test:
                    logging.debug('Would run "%s start" here.', init_script)
                else:
                    subprocess.Popen([init_script, 'start'])

            # Update the status of the service in the database.
            result.status = status
        except Error as ex:
            error = str(ex)
        
        # Render the HTML page and send it to the browser.
        try:
            page = self.templatelookup.get_template("/services/toggled.html")
            return page.render(title = "Byzantium Node Services",
                               purpose_of_page = "Service toggled.",
                               app = self.app, action = action, error = error)
        except:
            return exceptions.html_error_template().render()
    toggle_service.exposed = True
