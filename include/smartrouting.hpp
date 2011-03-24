#ifndef YANDEX_SMART_ROUTE_HPP
#define YANDEX_SMART_ROUTE_HPP

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdexcept>
#include <vector>

#include <loki/Singleton.h>

namespace yandex { namespace helpers { namespace smartrouting {
    class Exception: public std::runtime_error {
        public:
            Exception(const std::string& what) : std::runtime_error(what) {}
    };

    struct Address {
        unsigned long address;
        std::string name;

        Address(in_addr address_):
            address(address_.s_addr),
            name(inet_ntoa(address_)) {}
    };

    struct Endpoint {
        typedef std::vector<Address> address_vector_t;
        typedef address_vector_t::const_iterator const_iterator;
        
        address_vector_t addresses;
        
        Endpoint(const std::string& hostname);
    };

    struct Interface {
        std::string name;
        Address address, mask;

        Interface(const std::string& name_, sockaddr address_, sockaddr mask_);
    };

    struct Interfaces {
        typedef std::vector<Interface> interface_vector_t;
        typedef interface_vector_t::const_iterator const_iterator;

        interface_vector_t interfaces;
        
        Interfaces();
    };

    typedef Loki::SingletonHolder
    <
        Interfaces,
        Loki::CreateUsingNew,
        Loki::DefaultLifetime,
        Loki::ClassLevelLockable
    >
    theInterfaces;

    bool is_same_subnet(const std::string& hostname);
}}}
#endif
