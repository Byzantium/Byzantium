# state.py - Abstraction layer for storing different types of state in any kind
#     of backend store

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3


import model


class WirelessNetwork(model.Model):

    def __init__(self, client_interface=None, mesh_interface=None, gateway=None,
                 enabled=None, channel=None, essid=None, persistance=None,
                 testing=False):
        self._client_interface = client_interface
        self._mesh_interface = mesh_interface
        self._gateway = gateway
        self._enabled = enabled
        self._channel = channel
        self._essid = essid
        super(WirelessNetwork, self).__init__('wireless', persistance, testing)
        
    @property
    def client_interface(self):
        """I'm the 'client_interface' property."""
        return self._client_interface

    @client_interface.setter
    def client_interface(self, value):
        self.replace(client_interface=value)
        self._client_interface = value
        
    @property
    def mesh_interface(self):
        """I'm the 'mesh_interface' property."""
        return self._mesh_interface

    @mesh_interface.setter
    def mesh_interface(self, value):
        self.replace(mesh_interface=value)
        self._mesh_interface = value
            
    @property
    def gateway(self):
        """I'm the 'gateway' property."""
        return self._gateway

    @gateway.setter
    def gateway(self, value):
        self.replace(gateway=value)
        self._gateway = value
        
    @property
    def enabled(self):
        """I'm the 'enabled' property."""
        return self._enabled

    @enables.setter
    def enabled(self, value):
        self.replace(enabled=value)
        self._enabled = value
        
    @property
    def channel(self):
        """I'm the 'channel' property."""
        return self._channel

    @channel.setter
    def channel(self, value):
        self.replace(channel=value)
        self._channel = value
                        
    @property
    def essid(self):
        """I'm the 'essid' property."""
        return self._essid

    @essid.setter
    def essid(self, value):
        self.replace(essid=value)
        self._essid = value
        
    # probably going to want something like the activate/set_ip/tcip functionality here


