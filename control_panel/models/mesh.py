# state.py - Abstraction layer for storing different types of state in any kind
#     of backend store

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3
class Mesh(Model):

    def __init__(self, interface=None, protocol=None, enabled=None, persistance=None, testing=False):
        self._interface = interface
        self._protocol = protocol
        self._enabled = enabled
        super(Mesh, self).__init__('meshes', persistance, testing)
    
    # probably going to want some method to update_babeld in here


    @property
    def interface(self):
        """I'm the 'interface' property."""
        return self._interface

    @interface.setter
    def interface(self, value):
        self._interface = value
        
    @property
    def protocol(self):
        """I'm the 'protocol' property."""
        return self._protocol

    @protocol.setter
    def protocol(self, value):
        self._protocol = value
            
    @property
    def enabled(self):
        """I'm the 'enabled' property."""
        return self._enabled

    @enabled.setter
    def enabled(self, value):
        self._enabled = value