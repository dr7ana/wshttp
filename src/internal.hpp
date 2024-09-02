#pragma once

#include "encoding.hpp"
#include "format.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

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
    }  // namespace detail

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
        static void event_cb(struct bufferevent *bev, short events, void *ptr);
        static void accept_cb(
            struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int addrlen, void *arg);
    };

}  // namespace wshttp
