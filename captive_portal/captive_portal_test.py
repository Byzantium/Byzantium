#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :
# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# Copyright (C) 2013 Project Byzantium
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.

# captive_portal_test.py

import flexmock  # http://has207.github.com/flexmock
import unittest
import captive_portal


class CaptivePortalDetectorTest(unittest.TestCase):
    
    def setUp(self):
        self.detector = captive_portal.CaptivePortalDetector()

    def test_index(self):
        self.assertEqual("You shouldn't be seeing this, either.", self.detector.index())

if __name__ == '__main__':
    unittest.main()
