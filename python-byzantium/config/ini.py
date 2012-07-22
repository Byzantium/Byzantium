import ConfigParser

class ini(object):
	def __init__(self, file_name):
		self.config = ConfigParser.ConfigParser()
		self.file_name = file_name

	def get(self,key,value_type,default_value):
		section,key = (key+'.').split('.')[:2]
		try:
			self.config.read(self.file_name)
		except ConfigParser.ParsingError as cppe:
			logging.error(repr(cppe))
		return 'Malformed configuration file: %s' % self.file_name
		try:
			value = self.config.get(section,key)
		except ConfigParser.NoOptionError as cpnoe:
			logging.debug(repr(cpnoe))
			value = default_value
		return value

