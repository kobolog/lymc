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
    def __init__(self, servers, routing = True):
        self.autopickling = True

        if isinstance(servers, basestring):
            servers = [servers]

        super(Client, self).__init__(servers, routing)
    
    def configure(self, config = {}, **kwargs):
        if kwargs:
            config.update(kwargs)
        
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


__all__ = [Client]
