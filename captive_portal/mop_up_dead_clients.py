#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :
# mop_up_dead_clients.py - Daemon that pairs with the captive portal to remove
#    IP tables rules for idle clients so they don't overflow the kernel.
# By: Haxwithaxe
# Copyright (C) 2013 Project Byzantium
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.


# Modules
import sys
import os
import time
import subprocess

# Global variables.
# Defaults are set here but they can be overridden on the command line.
CACHEFILE = '/tmp/captive_portal-mopup.cache'
STASHTO = 'ram' # options are 'ram','disk'
MAXIDLESEC = 18000 # max idle time in seconds (18000s == 5hr)
CHECKEVERY = 1800.0 # check every CHECKEVERY seconds for idle clients (1800s == 30min)
IPTABLESCMD = ['/usr/sbin/iptables','-t','mangle','-L','internet','-n','-v']
USAGE = '''[(-c|--cache) <cache file>]\n\t[(-s|--stashto) <disk|ram>]\n\t[(-m|--maxidle) <time before idle client expires in seconds>]\n\t[(-i|--checkinterval) <time between each check for idle clients in\n\t\tseconds>]'''

# List of clients the daemon knows about.
clients={}

# _stash(): Writes the cache of known clients' information (documented below)
#           to a JSON file on disk.  Takes one argument, a dict containing a
#           client's information.  Returns nothing.  Only does something if the
#           stashtype is 'disk'.
'''@param	info	dict of mac:{'ip':string,'mac':string,'metric':int,'lastChange':int} (lastChange in unix timestamp)'''
def _stash(info):
    if STASHTO == 'disk':
        fobj = open(CACHEFILE,'w')
        fobj.write(json.dumps(info))
        fobj.close()

# _get_stash(): Tries to load the database of the node's clients from disk if
#               it exists.  Returns a dict containing the list of clients
#               associated with the node.
'''@return		False if empty or not found, else dict of mac:{'ip':string,'mac':string,'metric':int,'lastChange':int} (lastChange in unix timestamp)'''
def _get_stash():
    try:
        fobj = open(CACHEFILE,'r')
        fstr = fobj.read()
        fobj.close()
        if fstr:
            try:
                return json.loads()
            except ValueError as ve:
                print('Reading Cache File: Cache File Likely Empty '+str(ve))
    except IOError as ioe:
        print('Reading Cache File: Cache File Not Found '+str(ioe))
    return False

# _die(): Top-level error handler for the daemon.  Prints the usage info and
#         terminates.
def _die():
    print "USAGE: %s" % sys.argv[0], USAGE
    sys.exit(1)

# _scrub_dead(): Calls captive-portal.sh to remove an IP tables rule for a mesh
#                client that no longer exists.  Takes the MAC address of the
#                client as an arg, returns nothing.
'''@param	mac	string representing the mac address of a client to be removed'''
def _scrub_dead(mac):
    del clients[mac]
    subprocess.call(['/usr/local/sbin/captive-portal.sh', 'remove', mac])

# read_metrics(): Updates the client cache with the number of packets logged
#                 per client.  Takes no args.  Returns a dict containing the
#                 MAC and the current packet count.
'''@return	list of dict of {'ip':string,'mac':string,'metric':int}'''
def read_metrics():
    metrics = []
    packetcounts = get_packetcounts()
    for mac, pc in packetcounts.items():
        metrics += [{'mac':mac, 'metric':pc}]
    return metrics

# get_packetcounts(): Calls /usr/sbin/iptables to display the list of rules
#                     that correspond to mesh clients, in particular the number
#                     of packets sent or received by the client at time t.
#                     Returns a dict consisting of <MAC address>:<packet count>
#                     pairs.
def get_packetcounts():
    # Run iptables to dump a list of clients currently connected to this node.
    packetcounts = {}
    p = subprocess.Popen(IPTABLESCMD)
    p.wait()
    stdout, stderr = p.communicate()

    # If nothing was returned from iptables, return an empty list.
    if not stdout:
        return packetcounts

    # Roll through the captured output from iptables to pick out the packet
    # counts on a per client basis.
    for line in stdout.split('\n')[2:]:
        # If the line's empty, just exit this method so it doesn't error out
        # later.
        if not line:
            break
        larr = line.strip().split()

        # If the string's contents after being cleaned up are null, skip this
        # iteration.
        if not larr:
            continue

        # If the line contains a MAC address take it apart to extract the
        # MAC address and the packet count associated with it.
        if 'MAC' in larr:
            pcount = int(larr[0].strip() or 0)
            if len(larr) >= larr.index('MAC')+1:
                mac = larr[larr.index('MAC')+1]
                packetcounts[mac] = pcount

    # Return the hash to the calling method.
    return packetcounts

# bring_out_your_dead(): Method that carries out the task of checking to see
#                        which clients have been active and which haven't.
#                        This method is also responsible for calling the
#                        methods that remove a client's IP tables rule and
#                        maintain the internal database of clients.
def bring_out_your_dead(metrics):
    global clients

    # If the clients dict isn't populated, go through the dict of clients and
    # associate the current time (in time_t format) with their packet count.
    if not clients:
        for c in metrics:
            c['lastChanged'] = int(time.time())
            clients[c['mac']] = c
    else:
        for c in metrics:
            # Test every client we know about to see if it's been active or
            # not.
            if c['mac'] in clients:
                # If the number of packets recieved has changed, then update
                # its last known-alive time.
                if clients[c['mac']]['metric'] != c['metric']:
                    clients[c['mac']]['lastChanged'] = int(time.time())

                # If the client hasn't been alive for longer than MAXIDLESEC,
                # remove its rule from IP tables.  It'll have to reassociate.
                elif (int(time.time()) - clients[c['mac']]['lastChanged']) > MAXIDLESEC:
                    _scrub_dead(c['mac'])
            else:
                # Else, add the client.
                c['lastChanged'] = int(time.time())
                clients[c['mac']] = c
    # Update the cache of clients.
    _stash(clients)
    print(metrics,clients)

# mop_up(): Wrapper method that calls all of the methods that do the heavy
#           lifting in sequence.  Supposed to run when this code is imported
#           into other code as a module.  Takes no args.  Returns nothing.
'''call this if this is used as a module'''
def mop_up():
    metrics = read_metrics()
    print(metrics)
    bring_out_your_dead(metrics)

# If running this code as a separate process, main() gets called.
'''this is run if this is used as a script'''
def main(args):
    if args:
        global CACHEFILE
        global STASHTO
        global MAXIDLESEC
        global CHECKEVERY
        try:
            if '-c' in args:
                CACHEFILE = args[args.index('-c')+1]
            if '--cache' in args:
                CACHEFILE = args[args.index('--cache')+1]
            if '-s' in args:
                STASHTO = args[args.index('-s')+1]
            if '--stashto' in args:
                STASHTO = args[args.index('--stashto')+1]
            if '-m' in args:
                MAXIDLESEC = args[args.index('-m')+1]
            if '--maxidle' in args:
                MAXIDLESEC = args[args.index('--maxidle')+1]
            if '-i' in args:
                CHECKEVERY = float(args[args.index('-i')+1])
            if '--checkinterval' in args:
                CHECKEVERY = float(args[args.index('--checkinterval')+1])
            if '--help' in args:
                print "USAGE: %s" % sys.argv[0], USAGE
                sys.exit(1)
        except IndexError as ie:
            _die(USAGE % args[0])

        # Go to sleep for a period of time equal to three delay intervals to
        # give the node a chance to have some clients associate with it.
        # Otherwise this daemon will immediately try to build a list of
        # associated clients, not find any, and crash.
        time.sleep(CHECKEVERY * 3)

        # Go into a loop of mopping up and sleeping endlessly.
        while True:
            mop_up()
            time.sleep(CHECKEVERY)

if __name__ == '__main__':
    main(sys.argv)

# Fin.
