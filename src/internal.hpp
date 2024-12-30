#pragma once

#include "encoding.hpp"
#include "format.hpp"

extern "C" {
#include <openssl/types.h>
}

#include <ada.h>

namespace wshttp
{
    using namespace wshttp::literals;
    class stream;

    using url_result = ada::result<ada::url_aggregator>;

    struct uri;

    class url_parser
    {
        std::unique_ptr<std::string> _data{};

        std::unique_ptr<url_result> _res;

        url_parser() = default;

      public:
        static std::shared_ptr<url_parser> make();

        bool read(std::string input);

        void print_aggregates();

        uri extract();

        url_result& url();

        std::string href_str();
        std::string_view href_sv();

      private:
        url_result& _url();  // does no safety checking
        bool _parse();
        void _reset();
    };

    // global parser
    extern std::shared_ptr<url_parser> parser;

    namespace detail
    {
        const char* current_error();

        void setup_ssl_library();
    }  // namespace detail

    struct loop_callbacks
    {
        static void exec_oneshot(evutil_socket_t fd, short, void* user_arg);

        static void exec_iterative(evutil_socket_t fd, short, void* user_arg);
    };

    struct ctx_callbacks
    {
        static int server_select_alpn_proto_cb(
                SSL*,
                const unsigned char** out,
                unsigned char* outlen,
                const unsigned char* in,
                unsigned int inlen,
                void* arg);
    };

    struct dns_callbacks
    {
        static void server_cb(struct evdns_server_request* req, void* user_arg);
    };

    struct listen_callbacks
    {
        static void accept_cb(
                struct evconnlistener* evconn, evutil_socket_t fd, struct sockaddr* addr, int addrlen, void* user_arg);

        static void error_cb(struct evconnlistener* evconn, void* user_arg);
    };

    struct session_callbacks
    {
        static void event_cb(struct bufferevent* bev, short events, void* user_arg);

        static void read_cb(struct bufferevent* bev, void* user_arg);

        static void write_cb(struct bufferevent* bev, void* user_arg);

        static nghttp2_ssize send_callback(
                nghttp2_session* session, const uint8_t* data, size_t length, int flags, void* user_arg);

        // static int on_frame_send_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_arg);

        // static int on_data_chunk_recv_callback(
        //         nghttp2_session* session,
        //         uint8_t flags,
        //         int32_t stream_id,
        //         const uint8_t* data,
        //         size_t len,
        //         void* user_arg);

        static int on_frame_recv_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_arg);

        static int on_stream_close_callback(
                nghttp2_session* session, int32_t stream_id, uint32_t error_code, void* user_arg);

        static int on_header_callback(
                nghttp2_session* session,
                const nghttp2_frame* frame,
                const uint8_t* name,
                size_t namelen,
                const uint8_t* value,
                size_t valuelen,
                uint8_t flags,
                void* user_arg);

        static int on_begin_headers_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_arg);
    };

    struct stream_callbacks
    {
        static ssize_t file_read_callback(
                nghttp2_session* session,
                int32_t stream_id,
                uint8_t* buf,
                size_t length,
                uint32_t* data_flags,
                nghttp2_data_source* source,
                void* user_arg);
    };

    template <size_t N>
    struct datum
    {
      private:
        std::array<uint8_t, N> buf{};

        datum(const uint8_t* data, size_t sz) { write(data, sz); }

      public:
        datum() = default;

        template <enc::basic_char T>
        datum(const_span<T> data) : datum{reinterpret_cast<const uint8_t*>(data.data()), data.size()}
        {}

        datum(const datum& other) : datum{other.buf.data(), other.buf.size()} {}

        datum& operator=(const datum& other)
        {
            buf = other.buf;
            return *this;
        }

        inline void write(const uint8_t* data, size_t sz)
        {
            if (sz != N)
                throw std::invalid_argument{"Datum size must be {}"_format(N)};

            std::memcpy(buf.data(), data, sz);
        }

        template <enc::basic_char T = uint8_t>
        const_span<T> span() const
        {
            return {reinterpret_cast<const T*>(buf.data()), buf.size()};
        }

        explicit operator bool() const { return !buf.empty(); }

        bool operator<=>(const datum& other) const { return buf <=> other.buf; }
    };

}  // namespace wshttp
