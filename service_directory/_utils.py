
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

class config:
	''' Make me read from a file '''
	def __init__(self):
		self.services_cache = '/tmp/byz_services.json'
		self.service_template = '/etc/byzantium/services/avahi/template.service'
		self.services_store_dir = '/etc/avahi/inactive'
		self.services_live_dir = '/etc/avahi/services'


