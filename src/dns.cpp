#include "dns.hpp"

#include "endpoint.hpp"
#include "internal.hpp"
#include "types.hpp"

namespace wshttp
{
    void dns_callbacks::server_cb(struct evdns_server_request* req, void* user_data)
    {
        auto* server = reinterpret_cast<wshttp::dns::server*>(user_data);
        assert(server);

        detail::print_dns_req(req);

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

    namespace dns
    {
        std::unique_ptr<server> server::make(wshttp::endpoint& e)
        {
            return std::unique_ptr<server>{new server{e}};
        }

        server::server(wshttp::endpoint& e) : _ep{e}
        {
            _evdns = _ep.template shared_ptr<evdns_base>(
                evdns_base_new(_ep._loop->loop().get(), EVDNS_BASE_NAMESERVERS_NO_DEFAULT), deleters::_evdns{});

            evdns_set_log_fn([](int is_warning, const char* msg) {
                if (is_warning)
                    log->critical("{}", msg);
                else
                    log->debug("{}", msg);
            });

            // apparently this option ensures request addresses are not weirdly capitalized
            evdns_base_set_option(_evdns.get(), "randomize-case:", "0");
        }

        void server::initialize()
        {
            _ep.call_get([&]() {
                sockaddr_in _bind;

                _udp_sock = socket(PF_INET, SOCK_DGRAM, 0);

                if (_udp_sock < 0)
                    throw std::runtime_error{"UDP socket is fucked"};

                if (auto rv = evutil_make_socket_nonblocking(_udp_sock); rv < 0)
                    throw std::runtime_error{"Failed to non-block UDP socket: {}"_format(detail::current_error())};

                _bind.sin_family = AF_INET;
                _bind.sin_addr.s_addr = INADDR_ANY;
                _bind.sin_port = enc::host_to_big(defaults::DNS_PORT);

                if (auto rv = bind(_udp_sock, reinterpret_cast<sockaddr*>(&_bind), sizeof(sockaddr)); rv < 0)
                    throw std::runtime_error{"DNS server failed to bind UDP port: {}"_format(detail::current_error())};

                _udp_bind = _ep.template shared_ptr<evdns_server_port>(
                    evdns_add_server_port_with_base(
                        _ep._loop->loop().get(), _udp_sock, 0, dns_callbacks::server_cb, this),
                    deleters::_evdns_port{});

                if (not _udp_bind)
                    throw std::runtime_error{"DNS server failed to add UDP port: {}"_format(detail::current_error())};

                log->debug("DNS server successfully configured UDP socket!");

                _tcp_listener = _ep.template shared_ptr<struct evconnlistener>(
                    evconnlistener_new_bind(
                        _ep._loop->loop().get(),
                        nullptr,
                        nullptr,
                        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE,
                        -1,
                        reinterpret_cast<sockaddr*>(&_bind),
                        sizeof(sockaddr)),
                    deleters::_evconnlistener{});

                if (not _tcp_listener)
                {
                    auto err = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
                    throw std::runtime_error{"TCP listener construction is fucked: {}"_format(err)};
                }

                // we don't need to hold on to this one since we pass LEV_OPT_CLOSE_ON_FREE in creating the
                // evconnlistener, which closes the underlying socket when the listener is freed
                auto* _tcp_bind = evdns_add_server_port_with_listener(
                    _ep._loop->loop().get(), _tcp_listener.get(), 0, dns_callbacks::server_cb, this);

                if (not _tcp_bind)
                    throw std::runtime_error{"DNS server failed to bind server TCP port!"};

                log->debug("DNS server successfully configured TCP socket!");

                register_nameserver(defaults::DNS_PORT);
            });
        }

        void server::register_nameserver(uint16_t port)
        {
            auto ns_ip = detail::localhost_ip(port);
            evdns_base_nameserver_ip_add(_evdns.get(), ns_ip.c_str());
            log->info("Server successfully registered local nameserver ip: {}", ns_ip);
        }

        int server::main_lookup(struct evdns_server_request* req, struct evdns_server_question* q)
        {
            log->critical("DNS server received {} req for: {}", detail::translate_req_type(q->type), q->name);

            auto name = std::string_view{q->name};

            const char* res;
            auto is_v6 = q->type == EVDNS_TYPE_AAAA;

            /**
                TODO: add lookup logic in body
            */

            return is_v6 ? evdns_server_request_add_aaaa_reply(req, q->name, 1, res, 10)
                         : evdns_server_request_add_a_reply(req, q->name, 1, res, 10);
        }
    }  // namespace dns
}  //  namespace wshttp
