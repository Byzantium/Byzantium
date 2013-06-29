#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :
''' services.py
A module that reads the database of services running on the node and those found via avahi (mdns) and spits them out for use elsewhere.
'''
# Copyright (C) 2013 Project Byzantium
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.


__author__ = 'haxwithaxe (me at haxwithaxe dot net)'

__license__ = 'GPL v3'

import _utils
import sqlite3

# grab shared config
conf = _utils.Config()
logging = _utils.get_logging()

def get_local_services_list():
    '''Get the list of services running on this node from the databases.'''
    # Define the location of the service database.
    servicedb = conf.servicedb
    service_list = []

    # Set up a connection to the database.
    logging.debug("DEBUG: Opening service database.")
    connection = sqlite3.connect(servicedb)
    cursor = connection.cursor()

    # Pull a list of running web apps on the node.
    logging.debug("DEBUG: Getting list of running webapps from database.")
    cursor.execute("SELECT name FROM webapps WHERE status='active';")
    results = cursor.fetchall()
    for service in results:
        service_list += [{'name':service[0],'path':'/%s' % service[0],'description':''}]

    # Pull a list of daemons running on the node. This means that most of the web apps users will access will be displayed.
    logging.debug("DEBUG: Getting list of running servers from database.")
    cursor.execute("SELECT name FROM daemons WHERE status='active' AND showtouser='yes';")
    results = cursor.fetchall()
    for service in results:
        logging.debug("DEBUG: Value of service: %s" % str(service))
        if service[0] in conf.service_info:
            path = conf.service_info[service[0]]
        else:
            path = '/%s/' % service[0]
        service_list += [{'name':service[0],'path':path,'description':''}]

        # Clean up after ourselves.
        logging.debug("DEBUG: Closing service database.")
        cursor.close()
    return service_list

def get_remote_services_list():
    '''Get list of services advertised by Byzantium nodes found by avahi.'''
    import re
    service_list = []
    srvcdict = file2json(conf.services_cache)
    if not srvcdict: return service_list
    for name, vals in srvcdict.items():
        if re.search('\.__byz__\._[tucdp]{3}',name):
            description = ''
            path = vals['host']
            if vals['port']: path += str(vals['port'])
            if vals['text'] not in ('','\x00'):
                for entry in vals['text'].split('\n'):
                    key,val = (entry+'=').split('=')
                    v = list(val)
                    v.pop(-1)
                    val = ''.join(v)
                    if key == conf.uri_post_port_string_key:
                        path += val
                    elif key == conf.service_description_key:
                        description += val
            name = re.sub('\.__byz__\._[udtcp]{3}.*','',name)
            service_list = [{'name':name,'path':path,'description':description}]
    return service_list

def get_services_list():
    local_srvc = get_local_services_list()
    remote_srvc = get_remote_services_list()
    return local_srvc + remote_srvc

if __name__ == '__main__':
    logging.debug(get_services_list())
