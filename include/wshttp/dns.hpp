#pragma once

#include "loop.hpp"

namespace wshttp
{
    class Client;
}

namespace wshttp::dns
{
    class Server final
    {
        friend class wshttp::Client;

        Server() = delete;
        explicit Server(wshttp::Client& c);

      public:
        static std::unique_ptr<Server> make(wshttp::Client& c);

        ~Server();

        int main_lookup(struct evdns_server_request* req, struct evdns_server_question* q);

      private:
        wshttp::Client& _client;

        std::shared_ptr<evdns_base> _dns;
        std::shared_ptr<evdns_server_port> _udp_bind;
        evutil_socket_t _udp_sock;
        std::shared_ptr<evconnlistener> _tcp_listener;

        void initialize();

        void register_nameserver(uint16_t port);
    };
}  //  namespace wshttp::dns
