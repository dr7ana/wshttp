#include "dns.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

namespace wshttp
{
    using req_tuple = std::pair<dns::server*, dns::request_id>;

    namespace deleters
    {
        struct _evdns
        {
            inline void operator()(::evdns_base* e) const { ::evdns_base_free(e, 1); }
        };

        struct _evdns_port
        {
            inline void operator()(::evdns_server_port* e) const { ::evdns_close_server_port(e); }
        };
    }  // namespace deleters

    namespace detail
    {
        static constexpr std::string_view translate_dns_req_class(int t)
        {
            switch (t)
            {
                case EVDNS_CLASS_INET:
                    return "CLASS-INET"sv;
                default:
                    return "CLASS-UNKNOWN"sv;
            }
        }

        static constexpr std::string_view translate_req_type(int t)
        {
            switch (t)
            {
                case EVDNS_TYPE_A:
                    return "IPV4(A)-REQUEST"sv;
                case EVDNS_TYPE_CNAME:
                    return "CNAME-REQUEST"sv;
                case EVDNS_TYPE_PTR:
                    return "PTR-REQUEST"sv;
                case EVDNS_TYPE_AAAA:
                    return "IPV6(AAAA)-REQUEST"sv;
                case EVDNS_TYPE_SOA:
                    return "SOA-REQUEST"sv;
                default:
                    return "UNKNOWN-REQUEST"sv;
            }
        }

        static void print_dns_req(struct evdns_server_request* req)
        {
            auto msg = "\n----- INCOMING REQUEST -----\nFlags: {}\nNum questions: {}\n"_format(
                    req->flags, req->nquestions ? req->nquestions : 0);

            if (req->nquestions)
            {
                for (int i = 0; i < req->nquestions; ++i)
                {
                    auto* q = req->questions[i];
                    msg += "Question #{}:\nName: {} -- Type: {} -- Class: {}"_format(
                            i + 1,
                            q->name,
                            translate_req_type(q->type),
                            translate_dns_req_class(q->dns_question_class));
                }
            }

            unlog::critical("{}", msg);
        }

        static dns::server* _get_dns(void* user_arg)
        {
            return static_cast<dns::server*>(user_arg);
        }

        static dns::dns_request* _get_dnsreq(void* user_arg)
        {
            return static_cast<dns::dns_request*>(user_arg);
        }

    }  // namespace detail

    void dns_callbacks::gai_cb(int /* err */, struct evutil_addrinfo* /* ai */, void* user_arg)
    {
        detail::_get_dnsreq(user_arg)->hook();
    }

    namespace dns
    {
        std::unique_ptr<server> server::make(wshttp::endpoint& e)
        {
            return std::unique_ptr<server>{new server{e}};
        }

        server::server(wshttp::endpoint& e) : _ep{e}
        {
            _evdns = _ep.template shared_ptr<evdns_base>(
                    evdns_base_new(_ep._loop->loop().get(), EVDNS_BASE_INITIALIZE_NAMESERVERS), deleters::_evdns{});

            evdns_set_log_fn([](int is_warning, const char* msg) {
                if (is_warning)
                    unlog::critical("{}", msg);
                else
                    unlog::debug("{}", msg);
            });

            // apparently this option ensures request addresses are not weirdly capitalized
            evdns_base_set_option(_evdns.get(), "randomize-case:", "0");
        }

        void server::_close_request(request_id req_id)
        {
            unlog::trace("{} called", __PRETTY_FUNCTION__);

            _ep.loop()->call_soon([this, req_id]() mutable {
                if (auto it = _requests.find(req_id); it != _requests.end())
                {
                    _requests.erase(it);
                    unlog::debug("Closed dns request id:{}", req_id);
                }
                else
                    unlog::warn("Could not find dns request (id:{}) for closure!", req_id);
            });
        }

        void server::_dns_request_init(dns_request_cb hook, domain_host host)
        {
            unlog::trace("{} called", __PRETTY_FUNCTION__);

            auto [it, _] = _requests.emplace(dns_request::make(++next_request_id, std::move(hook)));

            hook = [this, cb = std::move(hook), req_id = next_request_id]() mutable {
                cb();
                _close_request(req_id);
            };

            unlog::trace("Initiating dns request id:{}", next_request_id);

            evutil_addrinfo hints{};

            hints.ai_family = AF_UNSPEC;
            hints.ai_protocol = IPPROTO_TCP;
            hints.ai_flags = EVUTIL_AI_CANONNAME;

            if (not evdns_getaddrinfo(
                        _evdns.get(), host.host_cstr(), nullptr, &hints, dns_callbacks::gai_cb, it->get()))
            {
                //
            }
        }

        void server::gai_request_result(int err, struct evutil_addrinfo* ai)
        {
            if (err)
            {
                //
            }

            (void)ai;
            // (void)req_id;
        }

    }  // namespace dns
}  //  namespace wshttp
