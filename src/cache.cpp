#include "cache.hpp"
#include "wrap.hpp"
#include "compression.hpp"
#include "smartrouting.hpp"

#include "boost/lambda/bind.hpp"
#include "boost/algorithm/string/split.hpp"
#include "boost/algorithm/string/classification.hpp"

namespace yandex { namespace memcached {
    using namespace std;
    using namespace log4cxx;
    using namespace yandex::helpers;
    using namespace boost::lambda;

    Client::Client(const vector<string>& servers, bool routing):
        m_pool(NULL),
        m_log(Logger::getLogger("ru.yandex.memcached")),
        m_config()
    {
        LOG4CXX_INFO(m_log, "initializing");
        LOG4CXX_INFO(m_log, "routing: " << (routing ? "on" : "off"));
        
        memcached_return_t rc;
        wrap<memcached_st> memcached(NULL, memcached_free);
        wrap<memcached_server_list_st> server_list(NULL, memcached_server_list_free);
        
        // Basic initializations
        srand(time(NULL));
        lzo_init();

        // Creating the initial memcached structure, which will be
        // converted to the pool later on
        memcached = memcached_create(NULL);
        if(!memcached.valid()) {
            LOG4CXX_FATAL(m_log, "failed to initialize libmemcached!");
            return;
        }

        // Parsing the server list
        vector<string> host;
        bool local = false;

        for(vector<string>::const_iterator it = servers.begin(); it != servers.end(); ++it) {
            host.clear();
            boost::split(host, *it, boost::is_any_of(":"));
                
            try {
                local = !routing || smartrouting::is_same_subnet(host[0]);
            } catch(const smartrouting::Exception& e) {
                LOG4CXX_WARN(m_log, boost::format("skipping unroutable host %1%: %2%") % host[0] % e.what());
                continue;
            }

            if(local) {
                LOG4CXX_INFO(m_log, boost::format("configuring server %1%") % host[0]);
                
                server_list = memcached_server_list_append(
                    server_list.release(),
                    host[0].c_str(),
                    host.size() == 2 ? atoi(host[1].c_str()) : 11211,
                    &rc);
                
                LOG4CXX_ASSERT(m_log, rc == MEMCACHED_SUCCESS, 
                    boost::format("failed to initialize server %1%: %2%") % host[0] % memcached_strerror(*memcached, rc));
            }
        }

        // Pushing it into the library
        if(memcached_server_list_count(*server_list) > 0) {
            rc = memcached_server_push(*memcached, *server_list);
            if(rc != MEMCACHED_SUCCESS) {
                LOG4CXX_FATAL(m_log, boost::format("failed to initialize the server list: %1%!") %
                    memcached_strerror(*memcached, rc));
                return;
            }
        } else {
            LOG4CXX_FATAL(m_log, "server list is empty!");
            return;
        }

        // Creating the default pool
        m_pool = memcached_pool_create(memcached.release(), m_config.pool.size / 2, m_config.pool.size);
    }

    Client::~Client() {
        if(m_pool) {
            memcached_st* memcached = memcached_pool_destroy(m_pool);
            memcached_free(memcached);
        }
        
        LOG4CXX_INFO(m_log, "shutting down");
    }
    
    void Client::configure(const map<string, uint64_t>& config) {
        if(!m_pool) {
            LOG4CXX_ERROR(m_log, "cannot configure an empty pool");
            return;
        }
        
        memcached_return_t rc;
        map<string, memcached_behavior> behaviors = boost::assign::map_list_of
            ("no-block", MEMCACHED_BEHAVIOR_NO_BLOCK)
            ("cache-lookups", MEMCACHED_BEHAVIOR_CACHE_LOOKUPS)
            ("binary-protocol", MEMCACHED_BEHAVIOR_BINARY_PROTOCOL)
            ("consistent-hashing", MEMCACHED_BEHAVIOR_KETAMA)
            ("tcp-nodelay", MEMCACHED_BEHAVIOR_TCP_NODELAY)
            ("tcp-keepalive", MEMCACHED_BEHAVIOR_TCP_KEEPALIVE)
            ("tcp-keepalive-timeout", MEMCACHED_BEHAVIOR_TCP_KEEPIDLE)
            ("server-failure-limit", MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT)
            ("server-poll-timeout", MEMCACHED_BEHAVIOR_POLL_TIMEOUT)
            ("server-connect-timeout", MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT)
            ("server-retry-timeout", MEMCACHED_BEHAVIOR_RETRY_TIMEOUT);

        for(map<string, uint64_t>::const_iterator it = config.begin(); it != config.end(); ++it) {
            LOG4CXX_INFO(m_log, boost::format("setting %1% to %2%") % it->first % it->second);
            
            if(behaviors.find(it->first) != behaviors.end()) {
                rc = memcached_pool_behavior_set(m_pool, behaviors.find(it->first)->second, it->second);
                LOG4CXX_ASSERT(m_log, rc == MEMCACHED_SUCCESS, boost::format("failed to set %1% to %2%: %3%") %
                    it->first % it->second % memcached_strerror(NULL, rc));
            } else if(it->first == "pool-size") {
                m_config.pool.size = it->second;
                // TODO: Need a scoped lock here to avoid a rare race condition
                memcached_st* memcached = memcached_pool_destroy(m_pool);
                m_pool = memcached_pool_create(memcached, m_config.pool.size / 2, m_config.pool.size);
            } else if(it->first == "pool-blocking") {
                m_config.pool.blocking = it->second;
            } else if(it->first == "compression-threshold") {
                m_config.compression.threshold = it->second;
            } else {
                LOG4CXX_WARN(m_log, boost::format("skipping unknown option %1%") % it->first);
            }
        }
    }

    string Client::get(const string& key) {
        memcached_return_t rc;
        string result;
        wrap<char*> value(NULL, free);
        size_t value_length;
        uint32_t inflated_length;
        wrap<memcached_st> connection(
            m_pool ? memcached_pool_pop(m_pool, m_config.pool.blocking, &rc) : NULL,
            bind(memcached_pool_push, m_pool, _1));
        decompressor<lzo> inflate;

        if(!connection.valid()) {
            return result;
        }

        if(key.empty()) {
            return result;
        }

        value = memcached_get(*connection, key.data(), key.length(),
            &value_length, &inflated_length, &rc);

        if(rc != MEMCACHED_SUCCESS) {
            LOG4CXX_ASSERT(m_log, rc == MEMCACHED_NOTFOUND,
                error(__func__, *connection, rc, key));
            return result;
        }

        if(inflated_length) {
            if(inflate(*value, value_length, inflated_length)) {
                result.assign(inflate.data(), inflate.length());
            } else {
                LOG4CXX_ERROR(m_log, boost::format("failed to decompress the value for key %1%") % key);
            }
        } else {
            result.assign(*value, value_length);
        }

        return result;
    }

    cache_map_t Client::get_multi(const cache_vector_t& keys) {
        memcached_return_t rc;
        cache_map_t result;
        wrap<memcached_st> connection(
            m_pool ? memcached_pool_pop(m_pool, m_config.pool.blocking, &rc) : NULL,
            bind(memcached_pool_push, m_pool, _1));
        decompressor<lzo> inflate;
        
        if(!connection.valid()) {
            return result;
        }
        
        // Converting the key vector to a char pointer vector
        std::vector<char*> key_values;
        std::vector<size_t> key_sizes;
        key_values.reserve(keys.size());
        key_sizes.reserve(keys.size());

        for(cache_vector_t::const_iterator it = keys.begin(); it != keys.end(); ++it) {
            if(it->empty()) {
                continue;
            }

            key_values.push_back(const_cast<char*>(it->data()));
            key_sizes.push_back(it->length());
        }

        // Querying
        rc = memcached_mget(*connection, &key_values[0], &key_sizes[0], key_values.size());
        if(rc != MEMCACHED_SUCCESS) {
            LOG4CXX_ASSERT(m_log, rc == MEMCACHED_NOTFOUND, error(__func__, *connection, rc))
            return result;
        }

        // Fetching
        wrap<memcached_result_st> ret(NULL, memcached_result_free); 
        string k, v;

        for(;;) {
            ret = memcached_fetch_result(*connection, ret.release(), &rc);
        
            // So, according to the manual, we continue fetching until we get MEMCACHED_END,
            // but in practice, we have to stop when we get anything except MEMCACHED_SUCCESS
            // OR when we get invalid result pointer. This is how it's done in memcached_fetch()
            if(rc != MEMCACHED_SUCCESS || !ret.valid()) {
                LOG4CXX_ASSERT(m_log, rc == MEMCACHED_END, error(__func__, *connection, rc));
                break;
            }

            // Getting the key
            k.assign(memcached_result_key_value(*ret), memcached_result_key_length(*ret));

            // Decompressing the value, if needed
            if(memcached_result_flags(*ret)) {
                if(inflate(memcached_result_value(*ret), memcached_result_length(*ret), memcached_result_flags(*ret))) {
                    v.assign(inflate.data(), inflate.length());
                } else {
                    LOG4CXX_ERROR(m_log, boost::format("failed to decompress the value for key %1%") % k);
                    continue;
                }
            } else {
                v.assign(memcached_result_value(*ret), memcached_result_length(*ret));
            }
            
            result.insert(make_pair(k, v));
        }

        return result;
    }

    bool Client::store(store_fn_t store_fn, const string& key, const string& value, time_t expire) {
        if(key.empty() || value.empty()) {
            return false;
        }

        cache_map_t cache_map = boost::assign::map_list_of(key, value);
        store(store_fn, cache_map, expire);

        return cache_map.empty();
    }

    void Client::store(store_fn_t store_fn, cache_map_t& cache_map, time_t expire) {
        memcached_return_t rc;
        wrap<memcached_st> connection(
            m_pool ? memcached_pool_pop(m_pool, m_config.pool.blocking, &rc) : NULL,
            bind(memcached_pool_push, m_pool, _1));
        compressor<lzo> deflate;

        if(!connection.valid()) {
            return;
        }

        cache_map_t::iterator it = cache_map.begin();
        bool compressed;

        while(it != cache_map.end()) {
            if(it->first.empty() || it->second.empty()) {
                ++it;
                continue;
            }

            compressed = (it->second.length() > m_config.compression.threshold) ?
                deflate(it->second.data(), it->second.length()) : false;
                            
            // Ternary abomination
            rc = store_fn(*connection, it->first.data(), it->first.length(),
                    compressed ? deflate.data() : it->second.data(),
                    compressed ? deflate.length() : it->second.length(),
                    expire ? expire : rand() % 60 + 120,
                    static_cast<uint64_t>(compressed ? it->second.length() : 0));

            if(rc == MEMCACHED_SUCCESS) {
                cache_map.erase(it++);
            } else {
                LOG4CXX_ERROR(m_log, error(__func__, *connection, rc, it->first));
                ++it;
            }
        }
    }

    bool Client::remove(const string& key) {
        if(key.empty()) {
            return false;
        }
        
        cache_vector_t cache_vector = boost::assign::list_of(key);
        remove_multi(cache_vector);
    
        return cache_vector.empty();
    }

    void Client::remove_multi(cache_vector_t& cache_vector) {
        memcached_return_t rc;
        wrap<memcached_st> connection(
            m_pool ? memcached_pool_pop(m_pool, m_config.pool.blocking, &rc) : NULL,
            bind(memcached_pool_push, m_pool, _1));

        if(!connection.valid()) {
            return;
        }

        cache_vector_t::iterator it = cache_vector.begin();
        
        while(it != cache_vector.end()) {
            if(it->empty()) {
                ++it;
            }

            rc = memcached_delete(*connection, it->data(), it->length(), static_cast<time_t>(0));
            
            if(rc == MEMCACHED_SUCCESS) {
                it = cache_vector.erase(it);
            } else {
                LOG4CXX_ASSERT(m_log, rc == MEMCACHED_NOTFOUND,
                    error(__func__, *connection, rc, *it));
                ++it;
            }
        }
    }

    void Client::flush() {
        memcached_return_t rc;
        wrap<memcached_st> connection(
            m_pool ? memcached_pool_pop(m_pool, m_config.pool.blocking, &rc) : NULL,
            bind(memcached_pool_push, m_pool, _1));

        if(!connection.valid()) {
            return;
        }

        rc = memcached_flush(*connection, static_cast<time_t>(0));

        LOG4CXX_ASSERT(m_log, rc == MEMCACHED_SUCCESS,
            error(__func__, *connection, rc));
    }

    boost::format Client::error(const char* function, const memcached_st* connection, memcached_return_t code, const string& key) const {
        memcached_return_t rc;
        memcached_server_instance_st server = NULL;

        if(code == MEMCACHED_SERVER_MARKED_DEAD) {
            server = memcached_server_get_last_disconnect(connection);
        } else if (!key.empty()) {
            server = memcached_server_by_key(connection, key.data(), key.length(), &rc);
        }

        if(server) {
            return boost::format("%1% failed for key %2%: %3% on %4%") %
                function %
                key %
                memcached_strerror(const_cast<memcached_st*>(connection), code) %
                server->hostname;
        } else {
            return boost::format("%1% failed: %2%") %
                function %
                memcached_strerror(const_cast<memcached_st*>(connection), code);
        }
    }
}}

