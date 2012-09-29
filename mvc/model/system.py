# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

'''
    system.py - *Example* Abstraction of a hardware system to Byzantium systems.
'''
import model

def _is_ipforward():
    from sh import cat
    fval = cat("/proc/sys/net/ipv4/ip_forward")
    if fval == '0': return False
    return True

def _do_randomcrap(crap):
    from sh import echo
    echo(str(crap))

class System(model.Model):
    '''Hardware Interface Model'''
    def get(self, *agrs,**kwargs):
        '''get the value or values of the given item or items within the system
            return: value requested or None if not found
            '''
        return _is_ipforward()

    def set(self, **kwargs):
        '''set a given value or values within the system
            return Boolean (True if success, False if failed)
            '''
        if 'crap' in kwargs:
            _do_randomcrap(kwargs['crap'])
        return True

    def remove(self, **kwargs):
        '''remove unset a given value or values
            return Boolean (True if success, False if failed)
            '''
        return True

    def save(self, sink=None):
        '''save is a NoOp'''
        return True

    def load(self, source=None):
        '''load is a NoOp'''
        return True

    def sync(self, target=None):
        '''sync is a NoOp'''
        return True

