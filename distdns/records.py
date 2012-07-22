# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

import sqlite3

class Database:

    def __init__(self, dbfile):

        self.database = sqlite3.connect(dbfile) # can be :memory: to run db in memory

        self.db = self.database.cursor()

        if not os.path.isfile(dbfile):

            self.db.execute('create table hosts (ip text, name text, mac text, type text, ttl integer, ts integer)')

        self.database.commit()

    def add(self, vals):

        # check for record

        # do magic to remove old dups

        self.db.execute("insert into hosts values ('%s', '%s', '%s')" % (vals['ip'], vals['name'], vals['mac'], vals['type']) )

        self.database.commit()

    def del(self, cond):

      self.execute("delete from hosts where %s" % cond )

        self.database.commit()

    def check(self, name, rectype = 'A'):

        records = []

        rectype = rectype.upper()

        self.db.execute("select * from hosts where name = '%s' and type = '%s'" % (name, rectype) )

        for i in self.db:

            records.append(i)

        return records

    def close(self):

        self.database.close()
