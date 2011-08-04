# meshconfiguration.py - Lets the user configure and manipulate mesh-enabled
#    wireless network interfaces.  Wired interfaces (Ethernet) are reserved for
#    use as net.gateways and fall under a different web app.

# For the time being this class is designed to operate with the Babel protocol
# (http://www.pps.jussieu.fr/~jch/software/babel/).  It would have to be
# rewritten to support a different (or more) protocols.

# TODO:
# - Make network interfaces that don't exist anymore go away.
# - Detect when an interface is already routing and instead offer the ability
#   to remove it from the mesh.  Do that in the MeshConfiguration.index()
#   method.
# - Switch out the development configuration databases for the production
#   databases.

# Import external modules.
import cherrypy
from mako.template import Template
from mako.lookup import TemplateLookup
from mako.exceptions import RichTraceback
import sqlite3

# Import core control panel modules.
from control_panel import *

# Classes.
# Allows the user to configure to configure mesh networking on wireless network
# interfaces.
class MeshConfiguration(object):
    # Class constants.
    babeld = '/usr/sbin/babeld'

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
    index.exposed = True

