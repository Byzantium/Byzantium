# state.py - Abstraction layer for storing different types of state in any kind
#     of backend store

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3


import abc
import sqlite3


def Error(Exception):
    pass


def _sanitize(tainted):
    copy = tainted.copy()
    if 'persistance' in copy:
        copy.pop('persistance')
    return copy


def get_matching(pattern, objects):
    """Take a dict and find all objects in list with matching values.
    
    Args:
        pattern: dict, a dict to match off
        objects: list, list of objects to check against

    Returns:
        A list of matching objects

    Raises:
        Error: if the keys of the pattern are not valid object attrs
    """
    contenders = []
    keys = set(pattern.keys())
    vals = set(pattern.values())
    for obj in objects:
        if not set(obj.__dict__.keys()) >= keys:
            raise Error('Bad pattern passed.')
        if set(obj.__dict__.values()) >= vals:
            contenders.append(obj)
    return contenders


class State(object):
    """Metaclass State object."""
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def initialize(self, name, prototype):
        """Set up any initial state necessary.
        
        Args:
            name: str, the name of the state you are storing
            prototype: instance, a simple instance of the class who's attributes
                make up the state
        """
        return

    @abc.abstractmethod
    def create(self, kind, item):
        """Create a new state entry.
        
        Args:
            kind: str, the kind of state you are creating
            item: instance, the instance who's attributes you are adding
        """
        return

    @abc.abstractmethod
    def list(self, kind):
        """Get a list of state objects.
        
        Args:
            kind: str, the kind of state you are trying to get a listing of
        
        Returns:
            A list of appropriate objects
        """
        pass

    @abc.abstractmethod
    def replace(self, kind, old, new):
        """Replace an old state entry with a new one.
        
        Args:
            kind: str, the kind of state you are trying to update
            old: instance, the instance who's attributes you will be replacing
            new: instance, the instance who's attributes will replace the old attributes
        """
        pass

class DBBackedState(State):
    """A State object who's backend store is an SQLite3 DB.
    
    Note: To deal with threading silliness between SQLite3 and CherryPy each
          action needs open and close its own connection to the db :/
    
    Attributes:
        db_path: str, the path to the SQLite3 DB file
        connection: sqlite3.connection, a connection object for working with the
            db
        kind_to_class: dict, a mapping of string 'kinds' to the proper classes to
            return them with
    """

    def __init__(self, db_path):
        super(DBBackedState, self).__init__()
        self.db_path = db_path

    def _execute_and_commit(self, query, values=None):
        self.connection = sqlite3.connect(self.db_path)
        cursor = self.connection.cursor()
        if values:
            cursor.execute(query, values)
        else:
            cursor.execute(query)
        self.connection.commit()
        self.connection.close()

    def _create_initialization_fragment_from_prototype(self, prototype):
        """Take an instance of a class and make a table based on its attributes.
        
        Args:
            prototype: dict, dict of attributes we will
                name columns with
        
        Returns:
            A string with '%s NUMERIC/TEXT' entries to be used and the tuple of
                column names
        """
        query = []
        columns = _sanitize(prototype).keys()
        columns.sort()
        for column in columns:
            if type(prototype[column]) == int:
                query.append('%s NUMERIC,')
            else:
                query.append('%s TEXT,')
        return ' '.join(query)[0:-1], tuple(columns)

    def _create_query_fragment_from_item(self, item):
        """Take an item and build a query fragment from its attributes.
        
        Args:
            item: instance, an instance of a class who's attributes we will query
        
        Returns:
            A string of '?,' entries and a tuple of values to be used in the query
        """
        query = []
        values = []
        attrs = _sanitize(item).keys()
        attrs.sort()
        for attr in attrs:
            query.append('?')
            values.append(item[attr])
        return ','.join(query), tuple(values)

    def _create_update_query_fragment_from_item(self, item):
        """Take an item and build a query template for an update.
        
        Args:
            item: instance, an instance of a class who's attributes we will query
        
        Returns:
            A string of 'attribute=?' entries and a tuple of values to be used in
                the query
        """
        update = []
        values = []
        for k, v in item.iteritems():
            if k not in ('kind', 'persistance'):
                update.append('%s=?' % k)
                values.append(v)
        return ' AND '.join(update), tuple(values)

    def _create_update_setting_fragment_from_item(self, item):
        """Take an item and build a setting template for an update.
        
        Args:
            item: dict, a dict who's attributes we will use
        
        Returns:
            A string of 'attribute=?' entries and a tuple of values to be used in
                the setting command
        """
        update = []
        values = []
        for k, v in item.iteritems():
            if k not in ('kind', 'persistance'):
                update.append('%s=?' % k)
                values.append(v)
        return ','.join(update), tuple(values )

    def initialize(self, prototype):
        """Create a table based on a prototype dict.
        
        Args:
            prototype: dict, a dict to base column names off
        """
        proto_copy = prototype.copy()
        kind = proto_copy.pop('kind')
        frag, columns = self._create_initialization_fragment_from_prototype(proto_copy)
        to_execute = 'CREATE TABLE IF NOT EXISTS %s (%s);' % (kind, frag % columns)
        self._execute_and_commit(to_execute)

    def create(self, item):
        """Insert or replace an entry in the backend.
        
        Args:
            item: dict, a dict who's attributes we'll insert/replace
        """
        item_copy = item.copy()
        kind = item_copy.pop('kind')
        frag, values = self._create_query_fragment_from_item(item_copy)
        to_execute = 'INSERT OR REPLACE INTO %s VALUES (%s);' % (kind, frag)
        self._execute_and_commit(to_execute, values)

    def get(self, kind, attrs):
        """Find all matching entries that match a dict of attrs.
        
        Args:
            kind: str, the kind of thing being searched for
            attrs: dict, a dict of attributes
          
         Returns:
             A list of matching entries and the cursor.description
        """
        self.connection = sqlite3.connect(self.db_path)
        cursor = self.connection.cursor()
        template, vals = self._create_update_query_fragment_from_item(attrs)
        to_execute = 'SELECT * FROM %s WHERE %s' % (kind, template)
        cursor.execute(to_execute, vals)
        results = cursor.fetchall()
        desc = cursor.description
        self.connection.close()
        return results, desc

    def exists(self, kind, attrs):
        '''Determine if any entries match a dict of attrs.
        
        Args:
            kind: str, the kind of thing being searched for
            attrs: dict, a dict of attributes
          
         Returns:
             Boolean indicating existence of records. True if records exist, False if none are found.
        '''
        if len(self.get(kind,attrs)[0]) > 0:
            return True
        return False

    def list(self, kind, klass, attrs=None):
        """List all entries for a given kind, returning them as klass objects.
        
        Args:
            kind: str, the kind of entry we are looking for
            klass: class, the class type we should build out of each result
            attrs: dict, dict of attrs specifically being checked for
          
         Returns:
             a list of klass objects based on results from the backend call
        """
        self.connection = sqlite3.connect(self.db_path)
        cursor = self.connection.cursor()
        if attrs:
            results, description = self.get(kind, attrs)
            col_name_list = [desc[0] for desc in description]
        else:
            to_execute = 'SELECT * FROM %s;' % kind
            cursor.execute(to_execute)
            # Not the most efficient way of doing things, but the db will always
            # be small enough that it won't matter
            results = cursor.fetchall()
            # We need to know what the attribute names are of the class we are
            # building
            col_name_list = [desc[0] for desc in cursor.description]
        objects = []
        for result in results:
            attrs = {}
            for i, v in enumerate(result):
                attrs[col_name_list[i]] = v
            attrs['persistance'] = self
            obj = klass(**attrs)
            objects.append(obj)
        self.connection.close()
        return objects

    def replace(self, old, new):
        """Replace an old entry with a new one.
        
        Args:
            old: dict, a dict of the old data (what we search for)
            new: dict, a dict of the replacement data
        """
        old_copy = old.copy()
        new_copy = new.copy()
        kind = old_copy.pop('kind')
        query_frag, query_values = self._create_update_query_fragment_from_item(old_copy)
        setting_frag, setting_values = self._create_update_setting_fragment_from_item(new_copy)
        # Again, not the most efficient, but it works in all cases and doesn't
        # need any fancy logic
        to_execute = 'UPDATE OR REPLACE %s SET %s WHERE %s;' % (kind, setting_frag, query_frag)
        self._execute_and_commit(to_execute, setting_values + query_values)

    def remove(self, kind, record=None, record_index=None, record_index_value=None):
        query_frag = query_values = None
        if record:
            if self.exists(record):
                query_frag, query_values = self._create_update_query_fragment_from_item(record)
            else:
                return False
        elif record_index and record_index_value:
            query_frag = '%s=?' % record_index
            query_values = tuple(record_index_value)
        if query_frag and query_values:
            to_execute = 'DELETE FROM %s WHERE %s' % (kind, query_frag)
            self._execute_and_commit(to_execute, query_values)

class NetworkState(DBBackedState):

    def __init__(self, db_path):
        super(NetworkState, self).__init__(db_path)
        wired = {'interface': '', 'gateway': '', 'enabled': '', 'kind': 'wired'}
        wireless = {'client_interface': '', 'mesh_interface': '',
                    'gateway': '', 'enabled': '', 'channel': 0, 'essid': '',
                    'kind': 'wireless'}
        self.initialize(wired)
        self.initialize(wireless)


class MeshState(DBBackedState):
    
    def __init__(self, db_path):
        super(MeshState, self).__init__(db_path)
        meshes = {'interface': '', 'protocol': '', 'enabled': '',
                  'kind': 'meshes'}
        self.initialize(meshes)


class ServiceState(DBBackedState):

    def __init__(self, db_path):
        super(ServiceState, self).__init__(db_path)
        daemons = {'h_name': '', 'desc':'', 'path':'', 'port': 0, 'status': 0,
                'kind':'daemons', 'show_to_user': '', 'init_script': ''}
        webapps = {'h_name': '', 'desc':'', 'path':'', 'port': 0, 'status': 0,
                'kind': 'webapps'}
        remote = {'h_name':'', 'desc':'', 'path':'', 'port':0, 'status':0,
                'kind':'remote', 'txt_record':'', 'full_name':'', 'host':''}
        self.initialize(daemons)
        self.initialize(webapps)
        self.initialize(remote)

