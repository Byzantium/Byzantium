# state.py - Abstraction layer for storing different types of state in any kind
#     of backend store

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

class WirelessNetwork(Model):

    def __init__(self, client_interface=None, mesh_interface=None, gateway=None, enabled=None, channel=None, essid=None, persistance=None, testing=False):
        self.client_interface = client_interface
        self.mesh_interface = mesh_interface
        self.gateway = gateway
        self.enabled = enabled
        self.channel = channel
        self.essid = essid
        super(WirelessNetwork, self).__init__('wireless', persistance, testing)
        
    # probably going to want something like the activate/set_ip/tcip functionality here

