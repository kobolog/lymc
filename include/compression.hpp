#include <lzo/lzo1x.h>

namespace yandex { namespace helpers {
    enum algorithm {
        lzo
    };

    template<algorithm> struct compressor;
    template<algorithm> struct decompressor;

    template<> struct compressor<lzo> {
        public:
            compressor():
                m_buffer(NULL),
                m_buffer_length(0),
                m_result_length(0) {}

            ~compressor() {
                free(m_buffer);
            }
                    
            bool operator()(const char* data, size_t data_length) {
                // /usr/share/doc/liblzo2-dev/LZO.FAQ.gz
                // Worst case expansion calculation
                lzo_uint expansion = data_length + (data_length / 16) + 64 + 3;

                if(expansion > m_buffer_length) {
                    m_buffer_length = expansion;
                    m_buffer = static_cast<lzo_bytep>(realloc(m_buffer, m_buffer_length));
                }

                lzo1x_1_compress(reinterpret_cast<const lzo_bytep>(data), data_length, m_buffer,
                    &m_result_length, &m_workmem);

                return (m_result_length < data_length);
            }

            inline const char* data() const {
                return reinterpret_cast<const char*>(m_buffer);
            }

            inline size_t length() const {
                return m_result_length;
            }

        private:
            lzo_byte *m_buffer, m_workmem[LZO1X_MEM_COMPRESS];
            lzo_uint m_buffer_length, m_result_length;
    };

    template<> struct decompressor<lzo> {
        public:
            decompressor():
                m_buffer(NULL),
                m_buffer_length(0),
                m_result_length(0) {}

            ~decompressor() {
                free(m_buffer);
            }

            bool operator()(const char* data, size_t data_length, size_t expansion_length) {
                if(expansion_length > m_buffer_length) {
                    m_buffer_length = expansion_length;
                    m_buffer = static_cast<lzo_bytep>(realloc(m_buffer, m_buffer_length));
                }

                m_result_length = m_buffer_length;

                int ret = lzo1x_decompress_safe(reinterpret_cast<const lzo_bytep>(data), data_length,
                    m_buffer, &m_result_length, NULL);

                return (ret == LZO_E_OK);
            }

            inline const char* data() const {
                return reinterpret_cast<const char*>(m_buffer);
            }

            inline size_t length() const {
                return m_result_length;
            }

        private:
            lzo_byte *m_buffer;
            lzo_uint m_buffer_length, m_result_length;
    };
}}
