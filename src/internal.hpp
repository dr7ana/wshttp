#pragma once

#include "format.hpp"
#include "utils.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    struct callbacks
    {
        static int server_select_alpn_proto_cb(
            SSL *,
            const unsigned char **out,
            unsigned char *outlen,
            const unsigned char *in,
            unsigned int inlen,
            void *arg);

        static void dns_server_cb(struct evdns_server_request *req, void *user_data);
    };

}  // namespace wshttp
