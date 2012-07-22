
def debug(message,level = '1'):
    import os
    _debug = ('BYZ_DEBUG' in os.environ)
    if _debug and os.environ['BYZ_DEBUG'] >= level:
        print(repr(message))


def file2str(file_name, mode = 'r'):
    fileobj = open(file_name,mode)
    filestr = fileobj.read()
    fileobj.close()
    return filestr


def str2file(string,file_name,mode = 'w'):
    fileobj = open(file_name,mode)
    fileobj.write(string)
    fileobj.close()


class Config(object):
    ''' Make me read from a file '''
    def __init__(self):
        self.services_cache = '/tmp/byz_services.json'
        self.service_template = '/etc/byzantium/services/avahi/template.service'
        self.services_store_dir = '/etc/avahi/inactive'
        self.services_live_dir = '/etc/avahi/services'


def check_for_configured_interface(netconfdb, interface, channel, essid):
    """docstring for check_for_configured_interface"""
    warning = ""

    # If a network interface is marked as configured in the database, pull
    # its settings and insert them into the page rather than displaying the
    # defaults.
    connection = sqlite3.connect(netconfdb)
    cursor = connection.cursor()
    template = (interface, )
    cursor.execute("SELECT enabled, channel, essid FROM wireless WHERE mesh_interface=?;", template)
    result = cursor.fetchall()
    if result and (result[0][0] == 'yes'):
        channel = result[0][1]
        essid = result[0][2]
        warning = '<p>WARNING: This interface is already configured!  Changing it now will break the local mesh!  You can hit cancel now without changing anything!</p>'
    connection.close()
    return (channel, essid, warning)