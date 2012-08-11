#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

"""Utility functions used throughout the codebase."""

from mako.exceptions import RichTraceback

import logging
import models.wireless_network


def file2str(file_name, mode = 'r'):
    """Reads in a file and returns it as a string.

    Args:
        file_name: str, Path to the file
        mode: str, Mode to open with (defaults to 'r')

    Returns:
        A string with the contents of the file.
    """
    fileobj = open(file_name, mode)
    filestr = fileobj.read()
    fileobj.close()
    return filestr


def str2file(string, file_name, mode = 'w'):
    """Writes a string to a file.

    Args:
        string: str, the string to be written
        file_name: str, the path of the file where contents will be written
        mode: str, the mode to open the file (defaults to 'w')
    """
    fileobj = open(file_name, mode)
    fileobj.write(string)
    fileobj.close()


class Config(object):
    """Make me read from a file."""

    def __init__(self):
        self.services_cache = '/tmp/byz_services.json'
        self.service_template = ('/etc/byzantium/services/avahi/'
                                 'template.service')
        self.services_store_dir = '/etc/avahi/inactive'
        self.services_live_dir = '/etc/avahi/services'


def check_for_configured_interface(persistance, interface, channel, essid):
    """Checking persistant storage for a configured interface.

    Args:
        persistance: obj, The State object to access storage
        interface: str, the interface to look for
        channel: str, channel setting
        essid: str, essid setting

    Returns:
        A tuple of the updated channel, essid, and warning arguments """
    warning = ""

    # If a network interface is marked as configured in the database, pull
    # its settings and insert them into the page rather than displaying the
    # defaults.
    results = persistance.list('wireless',
                               models.wireless_network.WirelessNetwork,
                               {'mesh_interface': interface})
    if results and (results[0].enabled == 'yes'):
        channel = results[0].channel
        essid = results[0].essid
        warning = ('<p>WARNING: This interface is already configured! '
                   'Changing it now will break the local mesh!  You can hit '
                   'cancel now without changing anything!</p>')
    return (channel, essid, warning)


def set_confdbs(test):
    """Determine the proper paths to net and mesh conf db files.

    Args:
        test: bool, If we are running in the test environment

    Returns:
        A the path to the netconfdb and meshconfdb files"""
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


def output_error_data():
    """Outputs usefule error information to stdout."""
    traceback = RichTraceback()
    for filename, lineno, function, line in traceback.traceback:
        print '\n'
        print ('Error in file %s\n\tline %s\n\tfunction %s' %
               (filename, lineno, function))
        print 'Execution died on line %s\n' % line
        print '%s: %s' % (str(traceback.error.__class__.__name__),
                          traceback.error)
