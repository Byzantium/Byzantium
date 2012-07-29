# state.py - Abstraction layer for storing different types of state in any kind
#     of backend store

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3                
class WiredNetwork(Model):
    
    def __init__(self, interface=None, gateway=None, enabled=None, persistance=None, testing=False):
        self._interface = interface
        self._gateway = gateway
        self._enabled = enabled
        super(WiredNetwork, self).__init__('wired', persistance, testing)
        
    # probably going to want something like the activate/set_ip/tcip functionality here


    @property
    def interface(self):
        """I'm the 'interface' property."""
        return self._interface

    @interface.setter
    def interface(self, value):
        self._interface = value
        self.replace(interface=value)
        
    @property
    def gateway(self):
        """I'm the 'gateway' property."""
        return self._gateway

    @gateway.setter
    def gateway(self, value):
        self._gateway = value
        self.replace(gateway=value)
            
    @property
    def enabled(self):
        """I'm the 'enabled' property."""
        return self._enabled

    @enabled.setter
    def enabled(self, value):
        self._enabled = value
        self.replace(enabled=value)
                
                
                