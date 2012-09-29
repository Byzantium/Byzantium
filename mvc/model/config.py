# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

'''
    config.py - Class that defines the configuration model - an object that
                contains a snapshot of the system's configuration and status
                at time T.
'''

import os
import time

# The Model() class is the template which the confiugration model object has
# to adhere to.
import model

# This stuff is for testing of the new model only.
import json

# End of stuff for testing only.

class Config(model.Model):
    def __init__(self, source):
        self._source = source
        self._config = {}
        self.__mtime = time.time()

    def _mod(self):
        self.__mtime = time.time()

    def _get_key(self, key):
        if key in self._config:
            return self._config[key]
        else:
            return None

    def get(self, *args, **kwargs):
        return_dict = {}
        if len(kwargs) == 1:
            return self._get_key(kwargs['key'])
        elif len(args) == 1:
            return self._get_key(args[0])
        elif len(kwargs) < 1 and len(args) < 1:
            return self._config
        elif len(args) > 0:
            for key in args:
                return_dict[key] = self._get_key(key)
        elif len(kwargs) > 0:
            for key, val in kwargs:
                if key == 'key':
                    key = val
                    val = kwargs['value']
                    del kwargs['key']
                    del kwargs['value']
                value = _get_key(key)
                if type(value) == dict and val in value:
                    return_dict[key] = value[val]
                else:
                    return_dict[key] = value
        return return_dict

    def _set_key_value(self, key, value):
        self._config[key] = value
        return True

    def set(self, **kwargs):
        return_dict = {}
        if len(kwargs) == 1:
            return self._set_key_value(kwargs.keys()[0], kwargs.values()[0])
        else:
            for key, val in kwargs.items():
                return_dict[key] = self._set_key_value(key, val)
        self._mod()
        return return_dict

    def remove(self, *args):
        for key in args:
            if key in self._config:
                del self._config[key]
                self._mod()
        return True

    def save(self):
        sink_file = open(self._source, 'w')
        json.dump(self._config, sink_file)
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

