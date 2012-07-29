# daemon.py - daemon model


# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3


import model


class Daemon(Model):

    def __init__(self, name=None, showtouser=None, port=None, initscript=None, status=None, persistance=None, testing=False):
        self.name = name
        self.status = status
        self.showtouser = showtouser
        self.port = port
        self.initscript = initscript
        super(Daemon, self).__init__('daemons', persistance, testing)
        
    # probably going to want something like toggle_service in here


