'''config.py - 
'''
import os
import time
from . import model

# start for tesing only
import json

# end for testing only

class Config(model.Model):
	def __init__(self, source):
		self._source = source
		self._config = None
		self.__mtime = time.time()
	
	def _mod(self):
		self.__mtime = time.time()

	def get(self, *args, **kwargs):
		return_dict = {}
		if 'key' in kwargs:
			return _get_key(kwargs['key'])
		else:
			for key in args:
				return_dict[key] = _get_key(key)
		self._mod()
		return return_dict

	def set(self, **kwargs):
		return_dict = {}
		if len(kwargs) == 1:
			return _set_key_value(kwargs.keys()[0], kwargs.vals()[0])
		else:
			for key, val in kwargs.items():
				return_dict[key] = _set_key_value(key, val)
		self._mod()
		return return_dict

	def del(self, *args):
		self._mod()
		return True

	def save(self):
		sink_file = open(self._source, 'w')
		json.dump(sink_file, self._config)
		sink_file.close()
		self._mod()
		return True

	def _load(self):
		source_file = open(self._source, 'r')
		config = json.load(source_file)
		source_file.close()
		return config

	def load(self):
		self._config = self._load()
		self._mod()
		return True

	def sync(self):
		source = self._load()
		mtime = os.stat(self._source).st_mtime
		if mtime > self.__mtime:
			self._config.update(source)
		else:
			source.update(self._config)
			self._config = source
		self._mod()
		return True
