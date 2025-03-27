#pragma once

// #include "encoding.hpp"
// #include "format.hpp"
#include "endpoint.hpp"
#include "parser.hpp"

extern "C" {
#include <openssl/types.h>
}

namespace wshttp
{
    using namespace wshttp::literals;

    static constexpr auto HTTPS_S = "https:"sv;
    static constexpr auto HTTP_S = "http:"sv;
    static constexpr auto WS_S = "http:"sv;
    static constexpr auto WSS_S = "http:"sv;

    // global parser
    extern std::shared_ptr<url_parser> parser;

    static int check_rv(int rv, std::string_view action, int expected = 1)
    {
        if (rv == expected)
            return rv;

        std::error_code ec{errno, std::system_category()};

        log->error(
                "Error code {} ({}) returned during {} (expected:{}, returned:{})",
                ec.value(),
                ec.message(),
                action,
                expected,
                rv);
        throw std::system_error{ec};
    }

    namespace detail
    {
        const char* current_error();

        inline constexpr auto evreq_err_str(evhttp_request_error ec)
        {
            switch (ec)
            {
                case EVREQ_HTTP_TIMEOUT:
                    return "REQUEST TIMEOUT"sv;
                case EVREQ_HTTP_EOF:
                    return "EOF REACHED"sv;
                case EVREQ_HTTP_INVALID_HEADER:
                    return "INVALID HEADER"sv;
                case EVREQ_HTTP_BUFFER_ERROR:
                    return "R/W BUFFER ERROR"sv;
                case EVREQ_HTTP_REQUEST_CANCEL:
                    return "REQUEST CANCELLED"sv;
                case EVREQ_HTTP_DATA_TOO_LONG:
                    return "DATA TOO LONG"sv;
                default:
                    return "UNKNOWN ERR"sv;
            }
        }

        void setup_ssl_library();

        inline ip_address get_connection_address(evhttp_connection* conn)
        {
            return ip_address{evhttp_connection_get_addr(conn)};
        }

        inline ip_address get_request_address(evhttp_request* req)
        {
            return get_connection_address(evhttp_request_get_connection(req));
        }

        inline bufferevent* get_request_bev(evhttp_request* req)
        {
            return evhttp_connection_get_bufferevent(evhttp_request_get_connection(req));
        }

        inline evutil_socket_t get_request_fd(evhttp_request* req)
        {
            return bufferevent_getfd(get_request_bev(req));
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
            static constexpr uint8_t BITMASK{0b00011111};
            return METHOD{static_cast<uint8_t>(
                    std::bit_width(static_cast<uint8_t>(evhttp_request_get_command(req) & BITMASK)))};
            // static_cast<uint8_t>((std::to_underlying(evhttp_request_get_command(req)) & BITMASK)))};
        }

        inline evhttp_cmd_type get_method_cmd_type(METHOD m)
        {
            switch (m)
            {
                case METHOD::GET:
                    return EVHTTP_REQ_GET;
                case METHOD::POST:
                    return EVHTTP_REQ_POST;
                case METHOD::HEAD:
                    return EVHTTP_REQ_HEAD;
                case METHOD::PUT:
                    return EVHTTP_REQ_PUT;
                case METHOD::DELETE:
                    return EVHTTP_REQ_DELETE;
                case METHOD::UNSUPPORTED:
                    [[unlikely]] [[fallthrough]];
                default:
                    [[unlikely]] throw std::runtime_error{"Cannot create evhttp request for unsupported method!"};
            }
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

        static int ssl_cert_verify_cb(X509_STORE_CTX* x509_ctx, void* user_arg);
    };

    struct dns_callbacks
    {
        static void gai_cb(int err, struct evutil_addrinfo* ai, void* user_arg);
    };

    struct outbound_callbacks
    {
        static void req_done_cb(struct evhttp_request* req, void* user_arg);

        static void req_error_cb(evhttp_request_error ec, void* user_arg);
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

    struct request_callbacks
    {
        static void req_error_cb(evhttp_request_error ec, void* user_arg);
    };

    struct ws_callbacks
    {
        static void msg_cb(
                struct evws_connection* evws, int type, const unsigned char* data, size_t len, void* user_arg);

        static void close_cb(struct evws_connection* evws, void* user_arg);
    };

}  // namespace wshttp
