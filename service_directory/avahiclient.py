import select
import json
import sys
import os
import pybonjour
import _utils

conf = _utils.Config()
logging = _utils.get_logging()

SERVICE_TYPE = '__byz__._tcp'

timeout  = 5
resolved = []

def update_services_cache(service,action = 'add'):
	logging.debug(service)
	logging.debug('updating cache')
	if os.path.exists(conf.services_cache):
		services = json.loads(_utils.file2str(conf.services_cache))
	else:
		services = {}

	if action.lower() == 'add':
		services.update(service)
		logging.debug('Service added')
	elif action.lower() == 'del' and service in services:
		del services[service]
		logging.debug('Service removed')
	_utils.str2file(json.dumps(services),conf.services_cache)

def resolve_callback(sdRef, flags, interfaceIndex, errorCode, fullname, hosttarget, port, txtRecord):
	if errorCode == pybonjour.kDNSServiceErr_NoError:
		logging.debug('adding')
		update_services_cache({fullname:{'host':hosttarget,'port':port,'text':txtRecord}})
		resolved.append(True)

def browse_callback(sdRef, flags, interfaceIndex, errorCode, serviceName, regtype, replyDomain):
	logging.debug(serviceName)
	if errorCode != pybonjour.kDNSServiceErr_NoError:
		return

	if not (flags & pybonjour.kDNSServiceFlagsAdd):
		update_services_cache(serviceName+'.'+regtype+replyDomain,'del')
		return

	resolve_sdRef = pybonjour.DNSServiceResolve(0, interfaceIndex, serviceName, regtype, replyDomain, resolve_callback)

	try:
		while not resolved:
			ready = select.select([resolve_sdRef], [], [], timeout)
			if resolve_sdRef not in ready[0]:
				logging.debug('Resolve timed out',1)
				break
			pybonjour.DNSServiceProcessResult(resolve_sdRef)
		else:
			resolved.pop()
	finally:
		resolve_sdRef.close()

def run():
	browse_sdRef = pybonjour.DNSServiceBrowse(regtype = SERVICE_TYPE, callBack = browse_callback)
	try:
		try:
			while True:
				ready = select.select([browse_sdRef], [], [])
				if browse_sdRef in ready[0]:
					pybonjour.DNSServiceProcessResult(browse_sdRef)
		except KeyboardInterrupt:
			pass
	finally:
		browse_sdRef.close()

if __name__ == '__main__':
	run()
