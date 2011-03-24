#include "python.hpp"

namespace yandex { namespace memcached { namespace python {
    template<class T1, class T2>
    std::pair<T1, T2> tuple_to_pair<T1, T2>::operator()(const tuple& arg) {
        extract<T1> first(arg[0]);
        extract<T2> second(arg[1]);

        if(first.check() && second.check()) {
            return std::pair<T1, T2>(first, second);
        } else {
            throw std::invalid_argument("unconvertible type");
        }
    }

    str ClientWrapper::get(const str& key) const {    
        std::string result, k = extract<std::string>(key);

        {
            scoped_gil_unlocker scoped;
            result = m_client->get(k);
        }

        return str(result);
    } 
    
    dict ClientWrapper::get_multi(const list& keys) const {
        cache_map_t cache_map;
        stl_input_iterator<std::string> begin(keys), end;
        cache_vector_t cache_vector(begin, end);

        {
            scoped_gil_unlocker scoped;
            cache_map = m_client->get_multi(cache_vector);
        }

        dict results;
        for(cache_map_t::const_iterator it = cache_map.begin(); it != cache_map.end(); ++it) {
            results.setdefault(it->first, it->second);
        }

        return results;
    }

    bool ClientWrapper::store(store_fn_t store_fn, const str& key, const str& value, time_t expire) {
        std::string k = extract<std::string>(key);
        std::string v = extract<std::string>(value);
        
        {
            scoped_gil_unlocker scoped;
            return store_fn(m_client, k, v, expire);
        }
    }

    dict ClientWrapper::store(bulk_store_fn_t store_fn, const dict& items, time_t expire) {
        cache_map_t cache_map(dict_to_map<cache_map_t::key_type, cache_map_t::mapped_type>(items));
        
        {
            scoped_gil_unlocker scoped;
            store_fn(m_client, cache_map, expire);
        }

        dict results;
        for(cache_map_t::const_iterator it = cache_map.begin(); it != cache_map.end(); ++it) {
            results.setdefault(it->first, it->second);
        }

        return results;
    }

    bool ClientWrapper::remove(const str& key) {
        std::string k = extract<std::string>(key);

        {
            scoped_gil_unlocker scoped; 
            return m_client->remove(k);
        }
    }

    list ClientWrapper::remove_multi(const list& keys) {
        stl_input_iterator<std::string> begin(keys), end;
        cache_vector_t cache_vector(begin, end);
    
        {
            scoped_gil_unlocker scoped;
            m_client->remove_multi(cache_vector);
        }

        list results;
        for(cache_vector_t::const_iterator it = cache_vector.begin(); it != cache_vector.end(); ++it) {
            results.append(*it);
        }

        return results;
    }

    BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(set_overloads, set, 2, 3)
    BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(set_multi_overloads, set_multi, 1, 2)
    BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(add_overloads, add, 2, 3)
    BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(add_multi_overloads, add_multi, 1, 2)
    BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(replace_overloads, replace, 2, 3)
    BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(replace_multi_overloads, replace_multi, 1, 2)

    BOOST_PYTHON_MODULE(_memcached) {
        char* logging_config = getenv("MEMCACHED_LOGGING_CONFIG");
        
        if(!logging_config) {
            // If we don't have any config, supress all logging
            log4cxx::helpers::LogLog::setQuietMode(true);
        } else {
            log4cxx::xml::DOMConfigurator::configure(logging_config);
        }
        
        class_<ClientWrapper>("Client", "The Cache Client",
            init<const list&>(
                "Initializes with a list of servers",
                args("servers")))

            .def(init<const list&, bool>(
                "Initializes with a list of servers and an option to disable smartrouting",
                args("servers", "routing")))

            .def("configure", &ClientWrapper::configure,
                "Sets various memcached behavior options",
                args("self", "config"))
            
            .def("get", &ClientWrapper::get,
                "Fetches a single value from the cache",
                args("self", "key"))
            
            .def("get_multi", &ClientWrapper::get_multi,
                "Fetches multiple values from  the cache",
                args("self", "keys"))
            
            .def("set", &ClientWrapper::set,
                set_overloads("Stores the value with specified key to the cache",
                args("key", "value", "expire")))
            
            .def("set_multi", &ClientWrapper::set_multi,
                set_multi_overloads("Stores multiple items to the cache",
                args("items", "expire")))
            
            .def("add", &ClientWrapper::add,
                add_overloads("Stores the value with specified key to the cache if its not there yet",
                args("key", "value", "expire")))
            
            .def("add_multi", &ClientWrapper::add_multi,
                add_multi_overloads("Stores multiple items to the cache if they are not there yet",
                args("items", "expire")))
            
            .def("replace", &ClientWrapper::replace,
                replace_overloads("Replaces the value with specified key to the cache if it's there",
                args("key", "value", "expire")))
            
            .def("replace_multi", &ClientWrapper::replace_multi,
                replace_multi_overloads("Stores multiple items to the cache if they're there",
                args("items", "expire")))
            
            .def("delete", &ClientWrapper::remove,
                "Invalidates the specified key",
                args("self", "key"))
            
            .def("delete_multi", &ClientWrapper::remove_multi,
                "Invalidates a set of keys",
                args("self", "keys"))

            .def("flush_all", &ClientWrapper::flush,
                "Invalidates everything",
                args("self"));
    }
}}} // namespace Yandex::Memcached::Python
