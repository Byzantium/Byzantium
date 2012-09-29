# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

import copy
import config
import json
import os
import unittest

config_file_name = 'config-example.json'
files = [config_file_name]

class BaseConfigTestCase(unittest.TestCase):
	def setUp(self):
		self.files = []
		self.file_name = config_file_name
		self.files.append(self.file_name)
		self.conf = config.Config(self.file_name)
		self.single_dict_key = 'test'
		self.single_dict_val = '123'
		self.fake_config_frag = {'fake': True}
		self.single_dict = {self.single_dict_key: self.single_dict_val}
		self.multi_dict = {'test':'321', 'test0':'123', 'test1':'hello', 'test2':'world'}
		self.fake_config = copy.deepcopy(self.multi_dict)
		self.fake_config.update(self.fake_config_frag)

	def tearDown(self):
		self.cleanup()

	def cleanup(self):
		for f in self.files:
			print(f)
			if os.path.exists(f):
				os.remove(f)

	def readRawConfig(self):
		f = open(self.file_name, 'r')
		d = json.load(f)
		return d

	def writeFakeConfig(self):
		f = open(self.file_name, 'w')
		json.dump(self.fake_config, f)
		f.close()

class SetSingleValueTestCases(BaseConfigTestCase):
	def runTest(self):
		'''set single value'''
		set_one_ret_val = self.conf.set(**self.single_dict)
		assert self.single_dict_val == self.conf.get(self.single_dict_key), 'set failed to set single value'

class GetSingleValueTestCases(BaseConfigTestCase):
	def runTest(self):
		'''get single value'''
		self.conf.set(**self.single_dict)
		get_ret_val = self.conf.get(self.single_dict_key)
		assert get_ret_val == self.single_dict_val, 'get failed to load single value'

class SaveTestCases(BaseConfigTestCase):
	def runTest(self):
		'''test saving config'''
		self.conf.set(**self.single_dict)
		save_ret_val = self.conf.save()
		raw_config = self.readRawConfig()
		assert self.single_dict == raw_config, 'save failed to save the config'

class LoadTestCases(BaseConfigTestCase):
	def runTest(self):
		'''test loading config'''
		self.writeFakeConfig()
		self.conf.load()
		assert self.fake_config == self.conf.get(), 'load failed to load the config'

class SetGetMultipleTestCase(BaseConfigTestCase):
	def runTest(self):
		set_multi_ret_val = self.conf.set(**self.multi_dict)
		get_multi_ret_val = self.conf.get(*(self.multi_dict.keys()))
		assert get_multi_ret_val == self.multi_dict, 'save failed to save the config (or get failed to get it)'

class SyncTestCase(BaseConfigTestCase):
	def runTest(self):
		'''sync with source file with before and after printouts'''
		self.writeFakeConfig()
		before = copy.deepcopy(self.conf.get())
		ret_val = self.conf.sync()
		after = copy.deepcopy(self.conf.get())
		expected_after = self.fake_config
		assert expected_after == after, 'sync failed to sync the config file and the config'

class RemoveTestCase(BaseConfigTestCase):
	def runTest(self):
		'''remove value (fake)'''
		removed = self.single_dict_key
		self.conf.set(**self.multi_dict)
		before = copy.deepcopy(self.conf.get())
		ret_val = self.conf.remove(removed)
		after = copy.deepcopy(self.conf.get())
		del before[removed]
		assert before == after, 'remove failed to remove the specified key'

if __name__ == '__main__':
	unittest.main()
