#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/assign.hpp>
#include "cache.hpp"

#include <log4cxx/helpers/loglog.h>
#include <log4cxx/xml/domconfigurator.h>

namespace yandex { namespace memcached { namespace python {
    using namespace boost::python;

    template<class T1, class T2>
    struct tuple_to_pair:
        public std::unary_function<tuple, std::pair<T1, T2> >
    {
        public:
            typedef tuple argument_type;
            typedef std::pair<T1, T2> result_type;

            result_type operator()(const argument_type& arg);
    };

    template<class Container>
    struct map_insert_iterator:
        public std::iterator<std::output_iterator_tag, typename Container::value_type>
    {
        public:
            typedef Container container_type;

            explicit map_insert_iterator(Container& x):
                container(x) {}

            map_insert_iterator<Container>& operator*() { return *this; }
            map_insert_iterator<Container>& operator++() { return *this; }
            map_insert_iterator<Container>& operator=(typename Container::const_reference value) {
                container.insert(value);
                return *this;
            }

        protected:
            Container& container;
    };

    template<class Container>
    inline map_insert_iterator<Container> map_inserter(Container& container) {
        return map_insert_iterator<Container>(container);
    }

    template<class T1, class T2>
    inline std::map<T1, T2> dict_to_map(const dict& d) {
        std::map<T1, T2> result;

        stl_input_iterator<tuple> begin(d.iteritems()), end;
        std::transform(begin, end, map_inserter(result),
            boost::bind(tuple_to_pair<T1, T2>(), _1));
    
        return result;
    }

    struct scoped_gil_unlocker: private boost::noncopyable {
        public:
            inline scoped_gil_unlocker():
                m_state(PyEval_SaveThread()) {}
            
            inline ~scoped_gil_unlocker() {
                PyEval_RestoreThread(m_state);
            }

        private:
            PyThreadState* m_state;
    };

    struct scoped_gil_locker: private boost::noncopyable {
        public:
            inline scoped_gil_locker():
                m_state(PyGILState_Ensure()) {}
            
            inline ~scoped_gil_locker() { 
                PyGILState_Release(m_state); 
            }

        private:
            PyGILState_STATE m_state;
    };

    class ClientWrapper {
        public:
            typedef boost::function<bool (Client*, const std::string&, const std::string&, time_t)> store_fn_t;
            typedef boost::function<void (Client*, cache_map_t&, time_t)> bulk_store_fn_t;

            ClientWrapper(const list& servers):
                m_client(NULL)
            {
                stl_input_iterator<std::string> begin(servers), end;
                m_client = new Client(std::vector<std::string>(begin, end));
            }

            ~ClientWrapper() {
                delete m_client;
            }

            inline void configure(const dict& config) {
                std::map<std::string, uint64_t> cfg_map(dict_to_map<std::string, uint64_t>(config));

                {
                    scoped_gil_locker lock;
                    m_client->configure(cfg_map);
                }
            }

            double locality() {
                return m_client->locality();
            }
            
            str get(const str& key) const;
            dict get_multi(const list& keys) const;

            inline bool set(const str& key, const str& value, time_t expire = 0) {
                return store(&Client::set, key, value, expire);
            }

            inline dict set_multi(const dict& items, time_t expire = 0) {
                return store(&Client::set_multi, items, expire);
            }
            
            inline bool add(const str& key, const str& value, time_t expire = 0) {
                return store(&Client::add, key, value, expire);
            }

            inline dict add_multi(const dict& items, time_t expire = 0) {
                return store(&Client::add_multi, items, expire);
            }
            
            inline bool replace(const str& key, const str& value, time_t expire = 0) {
                return store(&Client::replace, key, value, expire);
            }

            inline dict replace_multi(const dict& items, time_t expire = 0) {
                return store(&Client::replace_multi, items, expire);
            }
            
            bool remove(const str& key);
            list remove_multi(const list& keys);
            
            inline void flush() {
                m_client->flush();
            }

            list get_stats() const;

        private:
            bool store(store_fn_t store_fn, const str& key, const str& value, time_t expire);
            dict store(bulk_store_fn_t store_fn, const dict& items, time_t expire);

            Client* m_client;
    };
}}} // namespace Yandex::Memcached::Python
