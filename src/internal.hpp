#pragma once

#include "encoding.hpp"
// #include "format.hpp"
#include "request.hpp"

extern "C" {
#include <openssl/types.h>
}

#include <ada.h>

namespace wshttp
{
    using namespace wshttp::literals;

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

        inline ip_address get_connection_address(evhttp_connection* conn)
        {
            return ip_address{evhttp_connection_get_addr(conn)};
        }

        inline ip_address get_request_address(evhttp_request* req)
        {
            return get_connection_address(evhttp_request_get_connection(req));
        }

        inline evutil_socket_t get_request_fd(evhttp_request* req)
        {
            return bufferevent_getfd(evhttp_connection_get_bufferevent(evhttp_request_get_connection(req)));
        }

        inline constexpr auto get_method_string(METHOD m)
        {
            switch (m)
            {
                case METHOD::GET:
                    return "GET"sv;
                case METHOD::POST:
                    return "POST"sv;
                case METHOD::HEAD:
                    return "HEAD"sv;
                case METHOD::PUT:
                    return "PUT"sv;
                case METHOD::DELETE:
                    return "DELETE"sv;
                case METHOD::UNSUPPORTED:
                default:
                    return "UNSUPPORTED"sv;
            }
        }

        inline METHOD get_request_method(evhttp_request* req)
        {
            static constexpr uint8_t BITMASK{0b00001111};
            return METHOD{std::bit_width(
                    static_cast<uint8_t>((std::to_underlying(evhttp_request_get_command(req)) & BITMASK)))};
        }

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
        static void gen_cb(struct evhttp_request* req, void* user_arg);

        static bufferevent* bev_cb(struct event_base* ev, void* user_arg);

        static int newreq_cb(struct evhttp_request* req, void* user_arg);

        static void ws_cb(struct evhttp_request* req, void* user_arg);

        static void close_cb(struct evhttp_connection* conn, void* user_arg);

        static int error_cb(
                struct evhttp_request* req, struct evbuffer* buffer, int error, const char* reason, void* user_arg);
    };

    struct ws_callbacks
    {
        static void msg_cb(
                struct evws_connection* evws, int type, const unsigned char* data, size_t len, void* user_arg);

        static void close_cb(struct evws_connection* evws, void* user_arg);
    };

}  // namespace wshttp
