# daemon.py - daemon model


# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3


import model


class Daemon(Model):

    def __init__(self, name=None, show_to_user=None, port=None, init_script=None, status=None, persistance=None, testing=False):
        self._name = name
        self._status = status
        self._show_to_user = show_to_user
        self._port = port
        self._init_script = init_script
        super(Daemon, self).__init__('daemons', persistance, testing)
        
    # probably going to want something like toggle_service in here

    @propery
    def name(self):
        return self._name

    @name.setter
    def name(self,value):
        self.replace('name',value)

    @propery
    def status(self):
        return self._status

    @status.setter
    def status(self,value):
        self.replace('status',value)

    @propery
    def show_to_user(self):
        return self._show_to_user

    @show_to_user.setter
    def show_to_user(self,value):
        self.replace('show_to_user',value)

    @propery
    def port(self):
        return self._port

    @port.setter
    def port(self,value):
        self.replace('port',value)

    @propery
    def init_script(self):
        return self._init_script

    @init_script.setter
    def init_script(self,value):
        self.replace('init_script',value)

