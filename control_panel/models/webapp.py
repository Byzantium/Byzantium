# webapp.py - webapp model

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

class WebApp(Model):

    def __init__(self, name=None, status=None, persistance=None, testing=False):
        self.name = name
        self.status = status
        super(WebApp, self).__init__('webapps', persistance, testing)
        
    # probably going to want something like toggle_service in here
