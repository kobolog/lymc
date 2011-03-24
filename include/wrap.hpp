#include <boost/function.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits.hpp>
#include <algorithm>

namespace yandex { namespace helpers {
    template<typename T> struct wrap {
        private:
            typedef typename boost::mpl::if_<
                boost::is_pointer<T>,
                T,
                typename boost::add_pointer<T>::type
            >::type ptr_t;

            typedef boost::function<void (ptr_t object)> destructor_t;

        public:
            wrap(ptr_t object, destructor_t destructor):
                m_object(object),
                m_destructor(destructor) {}

            ~wrap() {
                destroy();
            }

            inline void operator=(ptr_t object) {
                destroy();
                m_object = object;
            }

            inline void operator=(wrap<T>& other) {
                destroy();
                m_object = other.release();
            } 

            inline ptr_t operator*() {
                return m_object;
            }

            inline const ptr_t operator*() const {
                return m_object;
            }

            inline bool valid() const {
                return (m_object != NULL);
            }

            inline ptr_t release() {
                ptr_t tmp = NULL;
                std::swap(tmp, m_object);
                return tmp;
            }

        private:
            void destroy() {
                if(m_object) {
                    m_destructor(m_object);
                }
            }

            ptr_t m_object;
            destructor_t m_destructor;
    };
}}
    

