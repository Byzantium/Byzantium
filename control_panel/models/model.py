# model.py - generic model

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# model.py - model abstraction


class Model(object):
    ''' Comment here'''

    def __init__(self, kind=None, persistance=None, testing=False):
        self.kind = kind
        self.persistance = persistance
        if not testing:
            results, _ = self.persistance.get(self.kind, self._trimmed())
            if not results:
                self.persistance.create(self._trimmed())

    def _trimmed(self):
        out = {}
        for k, v in self.__dict__.copy().iteritems():
            if k.startswith('_'):
              out[k[1:]] = v
            elif k == 'kind':
              out[k] = v
        return out

    def find(self, attrs):
        results, _ = self.persistance.get(self.kind, attrs)
        return results
    
    def list(self):
        return self.persistance.list(self.kind, self.__class__)
    
    def replace(self, **kwargs):
        old = self._trimmed()
        new = self._trimmed()
        for k, v in kwargs.iteritems():
            if k not in ('kind', 'persistance'):
                new[k] = v
        self.persistance.replace(old, new)

