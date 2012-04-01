#!/usr/bin/env python

import sys
import os
import time
import subprocess

CACHEFILE = '/tmp/captive_portal-mopup.cache'
STASHTO = 'ram' # options are 'ram','disk'
MAXIDLESEC = 18000 # max idle time in seconds (18000s == 5hr) 
CHECKEVERY = 1800 # check every CHECKEVERY seconds for idle clients (1800s == 30min)
USAGE = '''usage: %s [(-c|--cache) <cache file>] [(-s|--stashto) <disk|ram>] [(-m|--maxidle) <time before idle client expires in seconds>] [(-i|--checkinterval) <time between each check for idle clients in seconds>]'''

clients={}


'''@param	info	dict of mac:{'ip':string,'mac':string,'metric':int,'lastChange':int} (lastChange in unix timestamp)'''
def _stash(info):
	if STASHTO == 'disk':
		fobj = open(CACHEFILE,'w')
		fobj.write(json.dumps(info))
		fobj.close()

'''@return		False if empty or not found, else dict of mac:{'ip':string,'mac':string,'metric':int,'lastChange':int} (lastChange in unix timestamp)'''
def _get_stash():
	try:
		fobj = open(CACHEFILE,'r')
		fstr = fobj.read()
		fobj.close()
		if len(fstr) > 0:
			try:
				return json.loads()
			except ValueError as ve:
				print('Reading Cache File: Cache File Likely Empty '+str(ve))
	except IOError as ioe:
		print('Reading Cache File: Cache File Not Found '+str(ioe))
	return False

def _die():
	print(USAGE % sys.argv[0])
	sys.exit(1)

'''@param	mac	string representing the mac address of a client to be removed'''
def _scrub_dead(mac):
	pass

'''@return	list of dict of {'ip':string,'mac':string,'metric':int}'''
def read_metrics():
	return []

def bring_out_your_dead(metrics):
	global clients
	if len(clients) < 1:
		for c in metrics:
			c['lastChanged'] = int(time.time())
			clients[c['mac']] = c
	else:
		for c in metrics:
			if c['mac'] in clients: # If we know this client then check if it's dead
				if clients[c['mac']]['metric'] != c['metric']: # if change in metric then update time of last change
					clients[c['mac']]['lastChanged'] = int(time.time())
				elif (int(time.time()) - clients[c['mac']]['lastChanged']) > MAXIDLESEC: # else if the time since the last change is too long remove the client from the system
					_scrub_dead(c['mac'])
			else: # Else add client
				c['lastChanged'] = int(time.time())
				clients[c['mac']] = c
	_stash(clients) # stash the client list someplace

'''call this if this is used as a module'''
def mop_up():
	metrics = read_metrics()
	bring_out_your_dead(metrics)

'''this is run if this is used as a script'''
def main(args):
	if len(args) > 1:
		global CACHEFILE
		global STASHTO
		global MAXIDLESEC 
		global CHECKEVERY
		try:
			if '-c' in args: CACHEFILE = args[args.index('-c')+1]
			if '--cache' in args: CACHEFILE = args[args.index('--cache')+1]
			if '-s' in args: STASHTO = args[args.index('-s')+1]
			if '--stashto' in args: STASHTO = args[args.index('--stashto')+1]
			if '-m' in args: MAXIDLESEC = args[args.index('-m')+1]
			if '--maxidle' in args: MAXIDLESEC = args[args.index('--maxidle')+1]
			if '-i' in args: CHECKEVERY = args[args.index('-i')+1]
			if '--checkinterval' in args: CHECKEVERY = args[args.index('--checkinterval')+1]
		except IndexError as ie:
			_die(USAGE % args[0])
		
	while True:
		mop_up()
		time.sleep(CHECKEVERY)

if __name__ == '__main__':
	main(sys.argv)
