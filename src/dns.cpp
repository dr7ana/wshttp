#include "dns.hpp"

#include "client.hpp"
#include "format.hpp"
#include "types.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    static void server_cb(struct evdns_server_request* req, void* user_data)
    {
        auto* server = reinterpret_cast<wshttp::dns::Server*>(user_data);
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

    namespace dns
    {
        std::unique_ptr<Server> Server::make(wshttp::Client& c)
        {
            return std::unique_ptr<Server>{new Server{c}};
        }

        Server::Server(wshttp::Client& c) : _client{c}
        {
            _dns = _client.shared_ptr<evdns_base>(
                evdns_base_new(_client._loop->loop().get(), EVDNS_BASE_NAMESERVERS_NO_DEFAULT), wshttp::evdns_deleter);

            evdns_set_log_fn([](int is_warning, const char* msg) {
                if (is_warning)
                    log->critical("{}", msg);
                else
                    log->debug("{}", msg);
            });

            // apparently this option ensures request addresses are not weirdly capitalized
            evdns_base_set_option(_dns.get(), "randomize-case:", "0");
        }

        void Server::initialize()
        {
            sockaddr_in _bind;

            _udp_sock = socket(PF_INET, SOCK_DGRAM, 0);

            if (_udp_sock < 0)
                throw std::runtime_error{"UDP socket is fucked"};

            if (auto rv = evutil_make_socket_nonblocking(_udp_sock); rv < 0)
                throw std::runtime_error{"Failed to non-block UDP socket"};

            _bind.sin_family = AF_INET;
            _bind.sin_addr.s_addr = INADDR_ANY;
            _bind.sin_port = htons(defaults::DNS_PORT);

            if (auto rv = bind(_udp_sock, reinterpret_cast<sockaddr*>(&_bind), sizeof(sockaddr)); rv < 0)
                throw std::runtime_error{"DNS server failed to bind UDP port!"};

            _udp_bind = _client.shared_ptr<evdns_server_port>(
                evdns_add_server_port_with_base(_client._loop->loop().get(), _udp_sock, 0, server_cb, this),
                evdns_port_deleter);

            if (not _udp_bind)
                throw std::runtime_error{"DNS server failed to add UDP port!"};

            log->debug("DNS server successfully configured UDP socket!");

            _tcp_listener = _client.shared_ptr<struct evconnlistener>(
                evconnlistener_new_bind(
                    _client._loop->loop().get(),
                    nullptr,
                    nullptr,
                    LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE,
                    -1,
                    reinterpret_cast<sockaddr*>(&_bind),
                    sizeof(sockaddr)),
                evconnlistener_deleter);

            if (not _tcp_listener)
            {
                auto err = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
                throw std::runtime_error{"TCP listener construction is fucked: {}"_format(err)};
            }

            // we don't need to hold on to this one since we pass LEV_OPT_CLOSE_ON_FREE in creating the
            // evconnlistener, which closes the underlying socket when the listener is freed
            auto* _tcp_bind = evdns_add_server_port_with_listener(
                _client._loop->loop().get(), _tcp_listener.get(), 0, server_cb, this);

            if (not _tcp_bind)
                throw std::runtime_error{"DNS server failed to bind server TCP port!"};

            log->info("DNS server successfully configured TCP socket!");

            register_nameserver(defaults::DNS_PORT);
        }

        void Server::register_nameserver(uint16_t port)
        {
            auto ns_ip = detail::localhost_ip(port);
            evdns_base_nameserver_ip_add(_dns.get(), ns_ip.c_str());
            log->info("Server successfully registered local nameserver ip: {}", ns_ip);
        }

        int Server::main_lookup(struct evdns_server_request* req, struct evdns_server_question* q)
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
