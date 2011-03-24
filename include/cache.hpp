#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <ctime>

#include <boost/format.hpp>
#include <boost/noncopyable.hpp>
#include <boost/assign.hpp>
#include <boost/function.hpp>

#include <libmemcached/memcached.h>
#include <libmemcached/util/pool.h>

#include <log4cxx/logger.h>

namespace yandex { namespace memcached {
    typedef std::vector<std::string> cache_vector_t;
    typedef std::map<std::string, std::string> cache_map_t;
    
    typedef boost::function<memcached_return_t
            (memcached_st*, const char*, size_t, const char*, size_t, time_t, uint32_t)> store_fn_t;

    struct Config {
        public:
            struct {
                uint32_t size;
                bool blocking;
            } pool;

            struct {
                uint32_t threshold;
            } compression;

            Config() {
                // Default pool
                pool.size = 5;
                pool.blocking = false;

                // Disable compression
                compression.threshold = std::numeric_limits<uint32_t>::max();
            }
    };
    
    class Client: private boost::noncopyable {
        public:
            explicit Client(const std::vector<std::string>& servers, bool routing = true);
            ~Client();

            void configure(const std::map<std::string, uint64_t>& config);

            std::string get(const std::string& key);
            cache_map_t get_multi(const cache_vector_t& keys);
            
            inline bool set(const std::string& key, const std::string& value, time_t expire = 0) {
                return store(memcached_set, key, value, expire);
            }

            inline void set_multi(cache_map_t& cache_map, time_t expire = 0) {
                store(memcached_set, cache_map, expire);
            }

            inline bool add(const std::string& key, const std::string& value, time_t expire = 0) {
                return store(memcached_add, key, value, expire);
            }

            inline void add_multi(cache_map_t& cache_map, time_t expire = 0) {
                store(memcached_add, cache_map, expire);
            }

            inline bool replace(const std::string& key, const std::string& value, time_t expire = 0) {
                return store(memcached_replace, key, value, expire);
            }

            inline void replace_multi(cache_map_t& cache_map, time_t expire = 0) {
                store(memcached_replace, cache_map, expire);
            }
            
            bool remove(const std::string& key);
            void remove_multi(cache_vector_t& cache_vector);
            void flush();

            template<typename K>
            inline std::string compose_key(const std::string& prefix, const K key) const {
                std::ostringstream result;

                result << prefix << ':' << key;
                return result.str();
            }
       
        private:
            bool store(store_fn_t store_fn, const std::string& key, const std::string& value, time_t expire);
            void store(store_fn_t store_fn, cache_map_t& cache_map, time_t expire);

            boost::format error(const char* function, const memcached_st* connection,
                memcached_return_t code, const std::string& key = "") const;

            memcached_pool_st* m_pool;
            log4cxx::LoggerPtr m_log;
            Config m_config;
    };
}}
