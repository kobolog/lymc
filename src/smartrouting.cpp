#include "smartrouting.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <cstring>

#include <boost/lambda/bind.hpp>

namespace yandex { namespace helpers { namespace smartrouting {
    Endpoint::Endpoint(const std::string& hostname) {
        struct addrinfo *result, hints;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = 0;
        hints.ai_flags = AI_PASSIVE;
        hints.ai_protocol = 0;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;

        if (getaddrinfo(hostname.c_str(), NULL, &hints, &result) == 0) {
            for (addrinfo* it = result; it != NULL; it = it->ai_next) {
                addresses.push_back(Address(reinterpret_cast<sockaddr_in*>(it->ai_addr)->sin_addr));
            }
            
            freeaddrinfo(result);
        } else {
            throw Exception(std::string("Can't resolve hostname: ") + hostname);
        }
    }

    Interface::Interface(const std::string& name_, sockaddr address_, sockaddr mask_):
        name(name_),
        address(((sockaddr_in*)&address_)->sin_addr),
        mask(((sockaddr_in*)&mask_)->sin_addr) {}

    Interfaces::Interfaces() {
        char buf[1024 * 100];
        ifconf ifc;
        ifreq *ifr;

        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;
        
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
            throw Exception("Can't ioctl SIOCGIFCONF");
        
        int ifaces_count = ifc.ifc_len / sizeof(ifreq);
        ifr = ifc.ifc_req;

        for (int i = 0; i < ifaces_count; i++) {
            ifreq request;
            strcpy(request.ifr_name, ifr[i].ifr_name);
            ioctl(s, SIOCGIFNETMASK, &request);
            interfaces.push_back(Interface(ifr[i].ifr_name, ifr[i].ifr_addr, request.ifr_netmask));
        }

        shutdown(s, SHUT_RDWR);
    }

    namespace {
        bool is_same_subnet_address(const Interface& interface, const Address& address) {
            return (interface.address.address & interface.mask.address) == (address.address & interface.mask.address);
        }

        bool is_same_subnet_interface(const Interface& interface, const Endpoint& endpoint){
            Endpoint::const_iterator it = std::find_if(
                    endpoint.addresses.begin(), 
                    endpoint.addresses.end(),
                    bind(&is_same_subnet_address, interface, boost::lambda::_1));

            return it != endpoint.addresses.end();
        }

        bool is_same_subnet_impl(Interfaces::const_iterator begin, Interfaces::const_iterator end, const Endpoint& endpoint) {
            Interfaces::const_iterator it = std::find_if(
                    begin,
                    end,
                    bind(&is_same_subnet_interface, boost::lambda::_1, endpoint));
            
            return it != end;
        }
    }
    
    bool is_same_subnet(const std::string& hostname) {
        Interfaces& interfaces = theInterfaces::Instance();
        
        return is_same_subnet_impl(
                interfaces.interfaces.begin(),
                interfaces.interfaces.end(),
                hostname);
    }
}}} // namespace yandex::helpers::smartrouting
