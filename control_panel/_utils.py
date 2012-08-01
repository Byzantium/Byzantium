from mako.exceptions import RichTraceback

import logging
import sqlite3
import models.wireless_network

def debug(message,level = '1'):
    import os
    _debug = ('BYZ_DEBUG' in os.environ)
    if _debug and os.environ['BYZ_DEBUG'] >= level:
        print(repr(message))

def file2str(file_name, mode = 'r'):
    fileobj = open(file_name,mode)
    filestr = fileobj.read()
    fileobj.close()
    return filestr

def str2file(string,file_name,mode = 'w'):
    fileobj = open(file_name,mode)
    fileobj.write(string)
    fileobj.close()

class Config(object):
    ''' Make me read from a file '''
    def __init__(self):
        self.services_cache = '/tmp/byz_services.json'
        self.service_template = '/etc/byzantium/services/avahi/template.service'
        self.services_store_dir = '/etc/avahi/inactive'
        self.services_live_dir = '/etc/avahi/services'

def execute_query(db, query, template=None):
    """docstring for execute_query"""
    connection = sqlite3.connect(db)
    cursor = connection.cursor()
    if template:
        cursor.execute(query, template)
    else:
        cursor.execute(query)
    return connection, cursor

def check_for_configured_interface(persistance, interface, channel, essid):
    """docstring for check_for_configured_interface"""
    warning = ""
    # If a network interface is marked as configured in the database, pull
    # its settings and insert them into the page rather than displaying the
    # defaults.
    results = persistance.list('wireless', models.wireless_network.WirelessNetwork, {'mesh_interface': interface})
    if results and (results[0].enabled == 'yes'):
        channel = result[0].channel
        essid = result[0].essid
        warning = '<p>WARNING: This interface is already configured!  Changing it now will break the local mesh!  You can hit cancel now without changing anything!</p>'
    return (channel, essid, warning)

def set_confdbs(test):
    if test:
        # self.netconfdb = '/home/drwho/network.sqlite'
        netconfdb = 'var/db/controlpanel/network.sqlite'
        logging.debug("Location of netconfdb: %s", netconfdb)
        # self.meshconfdb = '/home/drwho/mesh.sqlite'
        meshconfdb = 'var/db/controlpanel/mesh.sqlite'
        logging.debug("Location of meshconfdb: %s", meshconfdb)
    else:
        netconfdb = '/var/db/controlpanel/network.sqlite'
        meshconfdb = '/var/db/controlpanel/mesh.sqlite'
    return netconfdb, meshconfdb

def set_wireless_db_entry(persistance, old, new):
    """docstring for set_wireless_db_entry"""
    # Commit the interface's configuration to the database.
    persistance.replace(old, new)

def output_error_data():
    traceback = RichTraceback()
    for filename, lineno, function, line in traceback.traceback:
        print '\n'
        print ('Error in file %s\n\tline %s\n\tfunction %s') % ((filename, lineno, function))
        print ('Execution died on line %s\n') % (line)
        print ('%s: %s') % ((str(traceback.error.__class__.__name__), traceback.error))
