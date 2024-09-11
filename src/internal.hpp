#pragma once

#include "encoding.hpp"
#include "format.hpp"

namespace wshttp
{
    using namespace wshttp::literals;
    class stream;

    namespace detail
    {
        inline void parse_addr(int af, void *dest, const std::string &from)
        {
            auto rv = inet_pton(af, from.c_str(), dest);

            if (rv == 0)  // inet_pton returns this on invalid input
                throw std::invalid_argument{"Unable to parse IP address!"};
            if (rv < 0)
                throw std::system_error{errno, std::system_category()};
        }

        inline constexpr std::string_view translate_dns_req_class(int t)
        {
            switch (t)
            {
                case EVDNS_CLASS_INET:
                    return "CLASS-INET"sv;
                default:
                    return "CLASS-UNKNOWN"sv;
            }
        }

        inline void print_dns_req(struct evdns_server_request *req)
        {
            auto msg = "\n----- INCOMING REQUEST -----\nFlags:{}\nNum questions:{}\n"_format(
                req->flags, req->nquestions ? req->nquestions : 0);

            if (req->nquestions)
            {
                for (int i = 0; i < req->nquestions; ++i)
                {
                    auto *q = req->questions[i];
                    msg += "Question #{}\nName: {} -- Type: {} -- Class: {}"_format(
                        i + 1, q->name, translate_req_type(q->type), translate_dns_req_class(q->dns_question_class));
                }
            }

            log->critical("{}", msg);
        }

        // static wshttp::stream *_get_stream(struct nghttp2_session *s, int32_t id)
        // {
        //     return static_cast<wshttp::stream *>(nghttp2_session_get_stream_user_data(s, id));
        // }
    }  // namespace detail

    struct loop_callbacks
    {
        static void exec_oneshot(evutil_socket_t fd, short, void *user_arg);
        static void exec_iterative(evutil_socket_t fd, short, void *user_arg);
    };

    struct ctx_callbacks
    {
        static int server_select_alpn_proto_cb(
            SSL *,
            const unsigned char **out,
            unsigned char *outlen,
            const unsigned char *in,
            unsigned int inlen,
            void *arg);
    };

    struct dns_callbacks
    {
        static void server_cb(struct evdns_server_request *req, void *user_data);
    };

    struct listen_callbacks
    {
        static void accept_cb(
            struct evconnlistener *evconn, evutil_socket_t fd, struct sockaddr *addr, int addrlen, void *user_arg);
        static void error_cb(struct evconnlistener *evconn, void *user_arg);
    };

    struct session_callbacks
    {
        static void event_cb(struct bufferevent *bev, short events, void *user_arg);
        static void read_cb(struct bufferevent *bev, void *user_arg);
        static void write_cb(struct bufferevent *bev, void *user_arg);

        static nghttp2_ssize send_callback(
            nghttp2_session *session, const uint8_t *data, size_t length, int flags, void *user_arg);
        static int on_frame_recv_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_arg);
        static int on_stream_close_callback(
            nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_arg);
        static int on_header_callback(
            nghttp2_session *session,
            const nghttp2_frame *frame,
            const uint8_t *name,
            size_t namelen,
            const uint8_t *value,
            size_t valuelen,
            uint8_t flags,
            void *user_arg);
        static int on_begin_headers_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_arg);
    };

    struct stream_callbacks
    {
        static ssize_t file_read_callback(
            nghttp2_session *session,
            int32_t stream_id,
            uint8_t *buf,
            size_t length,
            uint32_t *data_flags,
            nghttp2_data_source *source,
            void *user_data);
    };

}  // namespace wshttp
