# meshconfiguration.py - Lets the user configure and manipulate mesh-enabled
#    wireless network interfaces.  Wired interfaces (Ethernet) are reserved for
#    use as net.gateways and fall under a different web app.

# For the time being this class is designed to operate with the Babel protocol
# (http://www.pps.jussieu.fr/~jch/software/babel/).  It would have to be
# rewritten to support a different (or more) protocols.

# TODO:
# - Test this more!
# - Make network interfaces that don't exist anymore go away.
# - Detect when an interface is already routing and instead offer the ability
#   to remove it from the mesh.  Do that in the MeshConfiguration.index()
#   method.
# - Switch out the development configuration databases for the production
#   databases.
# - Refactor code to split PID getting into a helper method.

# Import external modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
from mako.exceptions import RichTraceback
import sqlite3
import os
import time
import subprocess

# Import core control panel modules.
from control_panel import *

# Classes.
# Allows the user to configure to configure mesh networking on wireless network
# interfaces.
class MeshConfiguration(object):
    # Class constants.
    babeld = '/usr/bin/babeld'
    babeld_pid = '/var/run/babeld.pid'
    babeld_timeout = 3

    #netconfdb = '/var/db/network.sqlite'
    netconfdb = '/home/drwho/network.sqlite'

    #meshconfdb = '/var/db/mesh.sqlite'
    meshconfdb = '/home/drwho/mesh.sqlite'

    # Class attributes which apply to a network interface.  By default they
    # are blank but will be populated from the mesh.sqlite database if the
    # user picks an interface that's already been set up.
    interface = ''
    protocol = ''
    enabled = ''
    pid = ''

    # Pretends to be index.html.
    def index(self):
        # This is technically irrelevant because the class' attributes are blank
        # when instantiated, but it's useful for setting up the HTML fields.
        self.reinitialize_attributes()

        # Populate the database of mesh interfaces using the network interface
        # database as a template.  Start by pulling a list of interfaces out of
        # the network configuration database.
        error = ''
        netconfconn = sqlite3.connect(self.netconfdb)
        netconfcursor = netconfconn.cursor()
        interfaces = []
        netconfcursor.execute("SELECT interface, enabled FROM wireless;")
        results = netconfcursor.fetchall()
        if not results:
            # Display an error page which says that no wireless interfaces have
            # been configured yet.
            error = "<p>ERROR: No wireless network interfaces have been configured yet.  <a href='/networkconfiguration'>You need to do that first!</a></p>"
        else:
            # Open a connection to the mesh configuration database.
            meshconfconn = sqlite3.connect(self.meshconfdb)
            meshconfcursor = meshconfconn.cursor()

            # Walk through the list of results.
            interfaces = ''
            for i in results:
                if i[1] == 'yes':
                    # See if the interface is already in the mesh configuration
                    # database, and if it's not insert it.
                    template = (i[0], )
                    meshconfcursor.execute("SELECT interface FROM meshes WHERE interface=?;", template)
                    interface_found = meshconfcursor.fetchall()
                    if not interface_found:
                        template = ('no', i[0], 'babel', )
                        meshconfcursor.execute("INSERT INTO meshes VALUES (?, ?, ?);", template)
                        meshconfconn.commit()

                    # This is a network interface that's ready to configure, so
                    # add it to the HTML template as a button.
                    interfaces = interfaces + "<input type='submit' name='interface' value='" + i[0] + "' />\n"

                else:
                    # This interface isn't ready yet but it's in the database,
                    # so add it to the template as an unclickable button.
                    # While it might not be a good idea to put unusable buttons
                    # into the page, it would tell the user that the interfaces
                    # were detected and are usable.
                    interfaces = interfaces + "<input type='submit' name='junk' value='" + i[0] + "' style='background-color:grey;' />\n"

        # Clean up our connections to the configuration databases.
        meshconfcursor.close()
        netconfcursor.close()

        # Render the HTML page.
        try:
            page = templatelookup.get_template("/mesh/index.html")
            return page.render(title = "Byzantium Node Mesh Configuration",
                               purpose_of_page = "Configure Mesh Interfaces",
                               error = error, interfaces = interfaces)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    index.exposed = True

    # Reinitialize the attributes of an instance of this class to a known
    # state.
    def reinitialize_attributes(self):
        self.interface = ''
        self.protocol = ''
        self.enabled = ''
        self.pid = ''

    # Allows the user to add a wireless interface to the mesh.  Assumes that
    # the interface is already configured (we wouldn't get this far if it
    # wasn't.
    def addtomesh(self, interface=None):
        # Store the name of the network interface and whether or not it's
        # enabled in the object's attributes.  Right now only the Babel
        # protocol is supported, so that's hardcoded for the moment (but it
        # could change in later releases).
        self.interface = interface
        self.protocol = 'babel'
        self.enabled = 'no'

        # Render the HTML page.
        try:
            page = templatelookup.get_template("/mesh/addtomesh.html")
            return page.render(title = "Byzantium Node Mesh Configuration",
                               purpose_of_page = "Enable Mesh Interfaces",
                               interface = self.interface,
                               protocol = self.protocol)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    addtomesh.exposed = True

    # Runs babeld to turn self.interface into a mesh interface.
    def enable(self):
        # Set up the error and successful output messages.
        error = ''
        output = ''

        # Set up a default set of command line options for babeld.  Some of
        # these are redundant but are present in case an older version of
        # babeld is used on the node.  See entries in the following file for
        # 30 January 2011 (release of babeld v1.1.0) to understand why.
        # http://www.pps.jussieu.fr/~jch/software/babel/CHANGES.text
        common_babeld_opts = ['-m ff02:0:0:0:0:0:1:6', '-p 6697', '-D']

        # Create a set of unique command line options for babeld.  Right now,
        # this variable is empty but it might be used in the future.  Maybe
        # it'll be populated from a config file or something.
        unique_babeld_opts = []

        # Set up a list of mesh interfaces for which babeld is already running.
        interfaces = []
        connection = sqlite3.connect(self.meshconfdb)
        cursor = connection.cursor()
        cursor.execute("SELECT interface, enabled, protocol FROM meshes WHERE enabled='yes' AND protocol='babel';")
        results = cursor.fetchall()
        for i in results:
            interfaces.append(i[0][0])

        # By definition, if we're in this method the new interface hasn't been
        # added yet.
        interfaces.append(self.interface)

        # Assemble the invocation of babeld.
        babeld_command = []
        babeld_command.append(self.babeld)
        babeld_command = babeld_command + common_babeld_opts
        babeld_command = babeld_command + unique_babeld_opts + interfaces

        # Test to see if babeld is running.  If it is, it's routing for at
        # least one interface, in which case we add the one the user just
        # picked to the list because we'll have to restart babeld.  Otherwise,
        # we just start babeld.
        pid = ''
        if os.path.exists(self.babeld_pid):
            pidfile = open(self.babeld_pid, 'r')
            pid = pidfile.readline()
            pidfile.close()
        if pid:
            os.kill(int(pid), signal.SIGTERM)
            time.sleep(self.babeld_timeout)
        process = subprocess.Popen(babeld_command)
        time.sleep(self.babeld_timeout)

        # Get the PID of babeld, then test to see if that pid exists and
        # corresponds to a running babeld process.  If there is no match,
        # babeld isn't running.
        pid = ''
        if os.path.exists(self.babeld_pid):
            pidfile = open(self.babeld_pid, 'r')
            pid = pidfile.readline()
            pidfile.close()
        if pid:
            procdir = '/proc/' + pid
            if not os.path.isdir(procdir):
                error = "ERROR: babeld is not running!  Did it crash after startup?"
            else:
                output = babeld + " has been successfully started with PID " + pid + "."
                # Update the mesh configuration database to take into account
                # the presence of the new interface.
                template = ('yes', self.interface, )
                cursor.execute("UPDATE meshes SET enabled=? WHERE interface=?;", template)
                cursor.commit()
        cursor.close()

        # Render the HTML page.
        try:
            page = templatelookup.get_template("/mesh/enabled.html")
            return page.render(title = "Byzantium Node Mesh Configuration",
                               purpose_of_page = "Mesh Interface Enabled",
                               protocol = self.protocol,
                               interface = self.interface,
                               error = error, output = output)
        except:
            traceback = RichTraceback()
            for (filename, lineno, function, line) in traceback.traceback:
                print "\n"
                print "Error in file %s\n\tline %s\n\tfunction %s" % (filename, lineno, function)
                print "Execution died on line %s\n" % line
                print "%s: %s" % (str(traceback.error.__class__.__name__),
                    traceback.error)
    enable.exposed = True
