# webapp.py - webapp model

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3


import model


class WebApp(model.Model):

    def __init__(self, name=None, status=None, persistance=None, testing=False):
        self._name = name
        self._status = status
        super(WebApp, self).__init__('webapps', persistance, testing)
        
    # probably going to want something like toggle_service in here

    @property
    def name(self):
        """I'm the 'name' property."""
        return self._name

    @name.setter
    def name(self, value):
        self.replace(name=value)
        self._name = value
        
    @property
    def status(self):
        """I'm the 'status' property."""
        return self._status

    @status.setter
    def status(self, value):
        self.replace(status=value)
        self._status = value
