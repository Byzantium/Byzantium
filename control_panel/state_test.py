#!/usr/bin/env python

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# state_test.py

from flexmock import flexmock  # http://has207.github.com/flexmock
import re
import state
import sqlite3
import unittest


class Simple(object):
    
    def __init__(self, attr1='it is text', attr2=3, persistance=None):
        self.attr1 = attr1
        self.attr2 = attr2
        self.kind = 'attrs'
        self.persistance = persistance


class DBBackedStateTest(unittest.TestCase):

    def setUp(self):
        self.connection = flexmock()
        flexmock(sqlite3).should_receive('connect').and_return(self.connection)
        self.state = state.DBBackedState('/no/real/path')
    
    def test_create_initialization_fragment_from_prototype(self):
        daemon = {'attr1': 'it is text', 'attr2': 3}
        result_str, result_vals = self.state._create_initialization_fragment_from_prototype(daemon)
        expected_str = '%s TEXT, %s NUMERIC'
        expected_vals = ('attr1', 'attr2')
        self.assertEqual(expected_str, result_str)
        self.assertEqual(expected_vals, result_vals)
        
    def test_create_query_fragment_from_item(self):
        daemon = {'attr1': 'it is text', 'attr2': 3}
        result_str, result_vals = self.state._create_query_fragment_from_item(daemon)
        expected_str = '?,?'
        expected_vals = ('it is text', 3)
        self.assertEqual(expected_str, result_str)
        self.assertEqual(expected_vals, result_vals)
        
    def test_create_update_query_fragment_from_item(self):
        daemon = {'attr1': 'it is text', 'attr2': 3}
        result_str, result_vals = self.state._create_update_query_fragment_from_item(daemon)
        # Can't count on any particular implementation spitting out the same order, so...
        self.assertEqual(1, len(re.findall('AND', result_str)))
        for attr in ['attr1=?', 'attr2=?']:
            self.assertTrue(attr in result_str)
        expected_vals = ('it is text', 3)
        self.assertEqual(sorted(expected_vals), sorted(result_vals))
        
    def test_create_update_setting_fragment_from_item(self):
        daemon = {'attr1': 'it is text', 'attr2': 3}
        result_str, result_vals = self.state._create_update_setting_fragment_from_item(daemon)
        # Can't count on any particular implementation spitting out the same order, so...
        self.assertEqual(1, len(re.findall(',', result_str)))
        for attr in ['attr1=?', 'attr2=?']:
            self.assertTrue(attr in result_str)
        expected_vals = ('it is text', 3)
        self.assertEqual(sorted(expected_vals), sorted(result_vals))
        
    def test_initialize(self):
        proto = {'attr1': '', 'attr2': 0, 'kind': 'attrs'}
        expected = 'CREATE TABLE IF NOT EXISTS attrs (attr1 TEXT, attr2 NUMERIC);'
        flexmock(self.state).should_receive('_execute_and_commit').with_args(expected).once
        self.state.initialize(proto)
        
    def test_create(self):
        proto = {'attr1': 'str', 'attr2': 1, 'kind': 'attrs'}
        expected = "INSERT OR REPLACE INTO attrs VALUES (?,?);"
        flexmock(self.state).should_receive('_execute_and_commit').with_args(expected, ('str', 1)).once
        self.state.create(proto)
        
    def test_list(self):
        one_dict = {'attr1': '', 'attr2': 0, 'kind': 'attrs'}
        two_dict = {'attr1': 'str', 'attr2': 1, 'kind': 'attrs'}
        one_obj = Simple(attr1='', attr2=0)
        two_obj = Simple(attr1='str', attr2=0)
        mock_cursor = flexmock()
        mock_cursor.description = [('attr1',), ('attr2',)]
        mock_cursor.should_receive('execute').once
        mock_cursor.should_receive('fetchall').once.and_return([('', 0), ('str', 1)])
        self.connection.should_receive('cursor').once.and_return(mock_cursor)
        returns = self.state.list('attrs', Simple)
        self.assertTrue(set(one_dict.values()) <= set(returns[0].__dict__.values()))
        self.assertTrue(set(two_dict.values()) <= set(returns[1].__dict__.values()))
    
    def test_replace(self):
        one_dict = {'attr1': '', 'attr2': 0, 'kind': 'attrs'}
        two_dict = {'attr1': 'str', 'attr2': 1, 'kind': 'attrs'}
        expected = 'UPDATE OR REPLACE attrs SET attr2=?,attr1=? WHERE attr2=? AND attr1=?;'
        flexmock(self.state).should_receive('_execute_and_commit').with_args(expected, (1, 'str', 0, '')).once
        self.state.replace(one_dict, two_dict)
        
        

if __name__ == '__main__':
    unittest.main()