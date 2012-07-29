# state.py - Abstraction layer for storing different types of state in any kind
#     of backend store

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3                
class WiredNetwork(Model):
    
    def __init__(self, interface=None, gateway=None, enabled=None, persistance=None, testing=False):
        self.interface = interface
        self.gateway = gateway
        self.enabled = enabled
        super(WiredNetwork, self).__init__('wired', persistance, testing)
        
    # probably going to want something like the activate/set_ip/tcip functionality here


