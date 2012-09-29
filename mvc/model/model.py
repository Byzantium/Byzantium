'''model.py - Abstraction of the system to Byzantium systems
	All models must adhere to this interface.
'''

import abc

class Model(object):
	'''Model Metaclass'''
	__metaclass__ = abc.ABCMeta
	@abc.abstractmethod
	def get(self, **kwargs):
		'''get
			retrive one or more
			return: value requested or None if not found
			'''
		pass

	@abc.abstractmethod
	def set(self, **kwargs):
		'''set
			Create/Update
			return Boolean (True if success, False if failed)
			'''
		pass

	@abc.abstractmethod
	def del(self, **kwargs):
		'''del
			remove/delete entry
			return Boolean (True if success, False if failed)
			'''
		pass

	@abc.abstractmethod
	def save(self, sink=None):
		'''save
			save to sink
			where sink is a place to store data in some fashion
			return Boolean (True if success, False if failed)
			'''
		pass

	@abc.abstractmethod
	def load(self, source=None):
		'''load
			load from source
			where source is a source of data of some kind
			return Boolean (True if success, False if failed)
			'''
		pass

	@abc.abstractmethod
	def sync(self, target=None):
		'''sync
			syncronize self._config with target
			'''
		pass

