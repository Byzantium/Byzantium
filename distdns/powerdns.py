import sys

msg = '%s\t%s\t%s\t%s\t%s\t%s\n' # type	qname	qclass	qtype	id	remote-ip-address

def output(data):

	sys.stdout.write(data)

def input():

	return sys.stdin.readline()

class PDNS:

	def __init__(self)

		self.gothelo = False

		while True:

			line = input()

			if self.gothelo and not line in (None, ''):

				output(self.handleinput(line))

			elif line == 'HELO\t1\n':

				output('OK\t\n')

				self.gothelo = True

			else:

				return 1

	def handleinput(line):

		line = line.split('\t')

		if line[0] == 'Q':

			return self.lookup(line)

		elif line[0] == 'AXFR':

			return self.axfr(line)

		elif line[0] == 'PING':

			pass

		elif line[0] == 'DATA':

			self.store(line)

		elif line[0] == 'END':

			pass

		elif line[0] == 'FAIL':

			pass

		else:

			return 'FAIL\t\n'

	def store(self,line):

		pass

	def lookup(self,line):

		pass

	def axfr(self,line):

		pass
