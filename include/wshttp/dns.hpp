#pragma once

#include "loop.hpp"

namespace wshttp
{
    class endpoint;
}

namespace wshttp::dns
{
    class server final
    {
        friend class wshttp::endpoint;

      public:
        server() = delete;
        explicit server(wshttp::endpoint& e);

        [[nodiscard]] static std::unique_ptr<server> make(wshttp::endpoint& e);

        ~server() = default;

        int main_lookup(struct evdns_server_request* req, struct evdns_server_question* q);

      private:
        wshttp::endpoint& _ep;

        tcp_listener _tcp_listener;
        std::shared_ptr<evdns_server_port> _udp_bind;
        evutil_socket_t _udp_sock;
        std::shared_ptr<evdns_base> _evdns;

        const std::shared_ptr<evdns_base>& dns() const { return _evdns; }

        void initialize();

        void register_nameserver(uint16_t port);
    };
}  //  namespace wshttp::dns
