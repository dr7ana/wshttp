#pragma once

#include "loop.hpp"

namespace wshttp
{
    class Endpoint;
}

namespace wshttp::dns
{
    class Server final
    {
        friend class wshttp::Endpoint;

      public:
        Server() = delete;
        explicit Server(wshttp::Endpoint& e);

        [[nodiscard]] static std::unique_ptr<Server> make(wshttp::Endpoint& e);

        ~Server() = default;

        int main_lookup(struct evdns_server_request* req, struct evdns_server_question* q);

      private:
        wshttp::Endpoint& _ep;

        std::shared_ptr<evconnlistener> _tcp_listener;
        std::shared_ptr<evdns_server_port> _udp_bind;
        evutil_socket_t _udp_sock;
        std::shared_ptr<evdns_base> _evdns;

        const std::shared_ptr<evdns_base>& dns() const { return _evdns; }

        void initialize();

        void register_nameserver(uint16_t port);
    };
}  //  namespace wshttp::dns
