# state.py - Abstraction layer for storing different types of state in any kind
#     of backend store

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3
class Mesh(Model):

    def __init__(self, interface=None, protocol=None, enabled=None, persistance=None, testing=False):
        self.interface = interface
        self.protocol = protocol
        self.enabled = enabled
        super(Mesh, self).__init__('meshes', persistance, testing)
    
    # probably going to want some method to update_babeld in here


