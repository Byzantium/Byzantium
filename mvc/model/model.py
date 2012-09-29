# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

'''model.py - Abstraction of the system to Byzantium systems
    All models must adhere to this interface.
'''

# New at this? Check this out: http://www.doughellmann.com/PyMOTW/abc/
import abc

class Model(object):
    '''Model Metaclass'''
    __metaclass__ = abc.ABCMeta
    @abc.abstractmethod
    def get(self, **kwargs):
        '''get
            Retrive one or more configuration settings for that aspect of the
            node's configuration.
            return: value requested or None if not found
            '''
        pass

    @abc.abstractmethod
    def set(self, **kwargs):
        '''set
            Create/Update a configuration setting for an aspect of the node's
            configuration.
            return Boolean (True if success, False if failed)
            '''
        pass

    @abc.abstractmethod
    def remove(self, **kwargs):
        '''del
            Remove/delete a configuration entry for an aspect of the node.
            return Boolean (True if success, False if failed)
            '''
        pass

    @abc.abstractmethod
    def save(self, sink=None):
        '''save
            Save settings to a data sink, where the data sink is a place to
            store data in some fashion.
            return Boolean (True if success, False if failed)
            '''
        pass

    @abc.abstractmethod
    def load(self, source=None):
        '''load
            Load configuration data from a data source, where the source is a
            store of data of some kind.
            return Boolean (True if success, False if failed)
            '''
        pass

    @abc.abstractmethod
    def sync(self, target=None):
        '''sync
            Synchronize self._config (the control panel's snapshot of the
            system state) with the target (the actual system state at time T
            beneath the control panel).
            '''
        pass

