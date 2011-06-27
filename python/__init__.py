# coding: utf-8

from _memcached import Client as ClientBase

try:
    import cPickle as pickle
except ImportError:
    import pickle
    

class Client(ClientBase):
    # Helper methods
    def _pickled(self, arg):
        if not self.autopickling:
            return arg

        if isinstance(arg, basestring):
            return arg
        else:
            return pickle.dumps(arg)

    def _unpickled(self, arg):
        if not self.autopickling:
            return arg

        try:
            return pickle.loads(arg)
        except:
            return arg
    
    # Basic initialization
    def __init__(self, servers):
        self.autopickling = True

        if isinstance(servers, basestring):
            servers = [servers]

        super(Client, self).__init__(servers)
    
    def configure(self, config):
        try:
            self.autopickling = config.pop('autopickling')
        except KeyError:
            pass

        super(Client, self).configure(config)

    # pylibmc-like interface
    def get(self, key, default = None):
        value = super(Client, self).get(str(key))
        
        if not value:
            return default
        
        return self._unpickled(value)

    def set(self, key, value, expire = 0):
        return super(Client, self).set(str(key), self._pickled(value), long(expire))

    def add(self, key, value, expire = 0):
        return super(Client, self).add(str(key), self._pickled(value), long(expire))

    def replace(self, key, value, expire = 0):
        return super(Client, self).replace(str(key), self._pickled(value), long(expire))

    def delete(self, key):
        return super(Client, self).delete(str(key))
    
    def get_multi(self, keys):
        items = super(Client, self).get_multi([str(key) for key in keys])
        return dict((k, self._unpickled(v)) for k, v in items.iteritems())

    def set_multi(self, items, expire = 0):
        items = dict((str(k), self._pickled(v)) for k, v in items.iteritems())
        return super(Client, self).set_multi(items, long(expire))
    
    def add_multi(self, items, expire = 0):
        items = dict((str(k), self._pickled(v)) for k, v in items.iteritems())
        return super(Client, self).add_multi(items, long(expire))

    def replace_multi(self, items, expire = 0):
        items = dict((str(k), self._pickled(v)) for k, v in items.iteritems())
        return super(Client, self).replace_multi(items, long(expire))

    def delete_multi(self, keys):
        return super(Client, self).delete_multi([str(key) for key in keys])

    # dict-like interface
    def __getitem__(self, key):
        value = self.get(key)
        
        if value is None:
            raise KeyError

        return value

    def __setitem__(self, key, value):
        if not self.set(key, value):
            raise ValueError

    def __delitem__(self, key):
        if not self.delete(key):
            raise KeyError

    def __contains__(self, key):
        return self.get(key) is not None

    def pop(self, key, default = None):
        try:
            value = self[key]
        except KeyError:
            if default is not None:
                return default
            else:
                raise

        self.delete(key)
        return value

    def setdefault(self, key, default):
        try:
            value = self[key]
        except KeyError:
            self[key] = value = default

        return value

    def clear(self):
        return self.flush_all()

    update = set_multi


class ClientPool(object):
    def __init__(self, groups):
        self.groups = {}
        
        for group, servers in groups.iteritems():
            client = Client(list(servers))
            self.groups[group] = client

        name, self.closest = max(self.groups.iteritems(),
            key = lambda item: item[1].locality())

    def configure(self, config):
        [group.configure(config) for group in self.groups.itervalues()]
        
    def __getattr__(self, name):
        return getattr(self.closest, name)

    def __getitem__(self, key):
        return self.closest[key]

    def __setitem__(self, key, value):
        self.closest.set(key, value)

    def __delitem__(self, key):
        self.closest.delete(key)

    def __contains__(self, key):
        return self.closest.get(key) is not None

    def mass_invalidate(self, target):
        if isinstance(target, (str, unicode)):
            [group.delete(target) for group in self.groups.itervalues()]
        else:
            [group.delete_multi(target) for group in self.groups.itervalues()]


__all__ = [Client, ClientPool]
