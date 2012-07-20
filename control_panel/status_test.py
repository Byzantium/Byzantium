#!/usr/bin/env python

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# captive_portal_test.py

from flexmock import flexmock  # http://has207.github.com/flexmock
import unittest
import status
import sys


class StatusHelpersTest(unittest.TestCase):

    def _raise_ioerror(self, x, y):
        raise IOError()

    def test_get_uptime_returns_false_on_ioerror(self):
        self.assertFalse(status.get_uptime(injected_open=self._raise_ioerror))

    def test_get_uptime_succeeds(self):
        mock = flexmock()
        mock.should_receive('readline').once.and_return('16544251.37 3317189.25\n')
        mock.should_receive('close').once
        self.assertEqual('16544251.37', status.get_uptime(injected_open=lambda x, y: mock))

    def test_get_load_returns_false_on_ioerror(self):
        self.assertFalse(status.get_load(injected_open=self._raise_ioerror))

    def test_get_load_returns_false_with_bad_formatting(self):
        mock = flexmock()
        mock.should_receive('readline').once.and_return('12345 6789\n')
        mock.should_receive('close').once
        self.assertFalse(status.get_load(injected_open=lambda x, y: mock))

    def test_get_load_succeeds(self):
        mock = flexmock()
        mock.should_receive('readline').once.and_return('0.00 0.01 0.05 1/231 11054\n')
        mock.should_receive('close').once
        expected = ['0.00', '0.01', '0.05']
        self.assertEqual(expected, status.get_load(injected_open=lambda x, y: mock))

    def test_get_memory_returns_false_on_ioerror(self):
        self.assertFalse(status.get_load(injected_open=self._raise_ioerror))

    def test_get_memory_succeds(self):
        mem = ['MemTotal:         509424 kB',
               'MemFree:           60232 kB',
               'Buffers:           98136 kB']
        expected = (509424, 449192)
        self.assertEqual(expected, status.get_memory(injected_open=lambda x, y: mem))
        

if __name__ == '__main__':
    unittest.main()
