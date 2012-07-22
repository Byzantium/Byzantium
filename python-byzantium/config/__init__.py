# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

__all__ = ['ini']

import ini
import json

CONFIG_INTERFACE_PATH = '/etc/byzantium/config/interfaces'
CONFIG_DIR = '/etc/byzantium/config'

class Config(object):
	def __init__(self):
		self.all_config = {}
		self.config_interfaces = {}

	def __call__(self):
		return self.all_config

	def load_interfaces(self):
		self.config_interfaces = {}
		for inf in os.listdir(CONFIG_INTERFACE_PATH):
			if not inf.startswith('.'):
				inffile = open(os.path.join(CONFIG_INTERFACE_PATH,inf),'r')
				config_interfaces.update(json.load(inffile))
				inffile.close()

	def load_config(self):
		self.all_config = {}
		for system,inf in self.config_interfaces.items():
			if inf['config-type'].lower() == 'ini':
				ini_file = ini(os.path.join(CONFIG_DIR,system))
				for key,opts in inf['keys'].items():
					if 'default-value' not in opts: opts['default-value'] = ''
					if 'value-type' not in opts: opts['value-type'] = ''
					self.all_config[key] = ini_file.get(key,opts['value-type'],opts['default-value'])
			elif inf['config-type'].lower() == 'json':
				config_file = open(os.path.join(CONFIG_DIR,system),'r')
				self.all_config[key] = josn.load(config_file)
				config_file.close()
			# Add other config interfaces here

	def load(self):
		load_interfaces
		load_config

	def reload(self):
		load
