#include "dns.hpp"
#include "internal.hpp"

namespace wshttp
{
    int callbacks::server_select_alpn_proto_cb(
        SSL *, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg)
    {
        (void)arg;  // user argument
        if (nghttp2_select_alpn(out, outlen, in, inlen) != 1)
        {
            log->critical("Failed to select ALPN proto!");
            return SSL_TLSEXT_ERR_NOACK;
        }

        return SSL_TLSEXT_ERR_OK;
    }

    void callbacks::dns_server_cb(struct evdns_server_request *req, void *user_data)
    {
        auto *server = reinterpret_cast<wshttp::dns::Server *>(user_data);
        assert(server);

        detail::print_req(req);

        int r;
        auto n_reqs = req ? req->nquestions : 0;

        for (int i = 0; i < n_reqs; ++i)
        {
            auto q = req->questions[i];

            switch (q->type)
            {
                case EVDNS_TYPE_A:
                case EVDNS_TYPE_AAAA:
                case EVDNS_TYPE_CNAME:
                    r = server->main_lookup(req, q);
                    break;
                case EVDNS_TYPE_PTR:
                case EVDNS_TYPE_SOA:
                default:
                    log->critical("Server received invalid request type: {}", detail::translate_req_type(q->type));
                    break;
            };
        }

        r = evdns_server_request_respond(req, 0);

        if (r < 0)
        {
            log->critical("Server failed to respond to request... dropping like its hot");
            evdns_server_request_drop(req);
        }
        else
            log->info("Server successfully responded to request!");
    };

}  //  namespace wshttp
