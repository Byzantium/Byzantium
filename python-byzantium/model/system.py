# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

'''
    system.py - *Example* Abstraction of a hardware system to Byzantium systems.
    System models must not hold values unless absolutely necessery. Holding on to values that can be read from the system is likely to cause errors and unintended behavior of the system when discrepencies between the OS and Byzantium occur.
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
    '''NonByzantium System Interface Model (ie OS or Hardware)'''
    def get(self, *agrs,**kwargs):
        '''get read the value or values of the given item or items within the system
            return: value requested or None if not found
            '''
        return _is_ipforward()

    def set(self, **kwargs):
        '''set a given value or values or act upon the system
            return Boolean (True if success, False if failed)
            '''
        if 'crap' in kwargs:
            _do_randomcrap(kwargs['crap'])
        return True

    def remove(self, *args, **kwargs):
        '''remove or unset a given value or values
            return Boolean (True if success, False if failed)
            '''
        _do_randomcrap('crap')
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

