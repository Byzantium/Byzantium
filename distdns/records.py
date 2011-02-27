import sqlite3

class Database:

	def __init__(self, dbfile):

		self.database = sqlite3.connect(dbfile) # can be :memory: to run db in memory

		self.db = self.database.cursor()

		if not os.path.isfile(dbfile):

			self.db.execute('create table hosts (ip text, name text, mac text)')

		self.database.commit()

	def addrecord(self, vals):

		# check for record

		# do magic to remove old dups

		self.db.execute("insert into hosts values ('%s', '%s', '%s')" % (vals[ip], vals[name], vals[mac]) )

		self.database.commit()

	def delrecord(self, cond):

      self.execute("delete from hosts where %s" % cond )

		self.database.commit()

	def checkrecord(self, name, rectype = 'A'):

		records = []

		rectype = rectype.upper()

		self.db.execute("select * from hosts where name = '%s' and type = '%s'" % (name, rectype) )

		for i in self.db:

			records.append(i)

		return records

	def close(self):

		self.database.close()
