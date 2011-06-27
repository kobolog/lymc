# coding: utf-8

from django.core.cache.backends.memcached import BaseMemcachedCache
import lymc

class YandexMemcachedCache(BaseMemcachedCache):
    def __init__(self, server, params):
        super(YandexMemcachedCache, self).__init__(server, params,
                                                   library=lymc,
                                                   value_not_found_exception=NotImplementedError)

    @property
    def _cache(self):
        if self.client:
            return client

        client = self._lib.Client(self._servers)

        if self._options:
            client.configure(self._options)

        self.client = client

        return client

    def incr(self, key, delta=1, version=None):
        raise NotImplementedError

    def decr(self, key, delta=1, version=None):
        raise NotImplementedError

    def close(self, **kwargs):
        # No need to disconnect, connections are not persistent
        pass
