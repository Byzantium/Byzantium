#!/usr/bin/env python

import ConfigParser

class config:
	def __init__(self, config_file):
		conf = ConfigParser.ConfigParser()
		conf.read(config_file)

	def __call__(self):
		return self.to_dict()
	
	def to_dict(self)
