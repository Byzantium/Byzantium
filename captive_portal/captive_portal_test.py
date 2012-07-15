#!/usr/bin/env python

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# captive_portal_test.py

import unittest
import captive_portal

class CaptivePortalDetectorTest(unittest.TestCase):
    
    def setUp(self):
        self.detector = captive_portal.CaptivePortalDetector()
        captive_portal.interface = "test"

    def test_index(self):
        self.assertEqual("You shouldn't be seeing this, either.", self.detector.index())

if __name__ == '__main__':
    unittest.main()
