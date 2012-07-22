import os
import json
import logging

if os.environ['BYZ_DEBUG']:
	logging.basicConfig(level=logging.DEBUG)
else:
	logging.basicConfig(level=logging.ERROR)

def get_logging():
	return logging

def file2str(file_name, mode = 'r'):
	if not os.path.exists(file_name):
		logging.debug('File not found: '+file_name)
		return ''
	fileobj = open(file_name,mode)
	filestr = fileobj.read()
	fileobj.close()
	return filestr

def file2json(file_name, mode = 'r'):
	filestr = file2str(file_name, mode)
	try:
		return_value = json.loads(filestr)
	except ValueError as val_e:
		logging.debug(val_e)
		return_value = None
	return return_value

def str2file(string,file_name,mode = 'w'):
	fileobj = open(file_name,mode)
	fileobj.write(string)
	fileobj.close()

def json2file(obj, file_name, mode = 'w'):
	try:
		string = json.dumps(obj)
		str2file(string, file_name, mode)
		return True
	except TypeError as type_e:
		logging.debug(type_e)
		return False

class Config(object):
	''' Make me read from a file and/or environment'''
	def __init__(self):
		self.services_cache = '/tmp/byz_services.json'
		self.service_template = '/etc/byzantium/services/avahi/template.service'
		self.services_store_dir = '/etc/avahi/inactive'
		self.services_live_dir = '/etc/avahi/services'
		self.servicedb = '/var/db/controlpanel/services.sqlite'
		self.no_services_msg = 'No services found in the network. Please try again in a little while.'
		self.no_internet_msg = '<span class="sad-face">This mesh network is probably not connected to the internet.</span>'
		self.no_internet_msg = '<span class="winning">This mesh network is probably connected to the internet.</span>'
		self.uri_post_port_string_key = 'appendtourl'
		self.service_description_key = 'description'
		self.service_info = {'chat':'/chat/?channels=byzantium'}

