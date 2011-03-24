#include <boost/python.hpp>
#include "smartrouting.hpp"

namespace yandex { namespace helpers { namespace smartrouting {
    using namespace boost::python;
        
    BOOST_PYTHON_MODULE(_smartrouting) {
        def("is_same_subnet", is_same_subnet);
    }
}}}
