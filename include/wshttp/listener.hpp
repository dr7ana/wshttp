#pragma once

#include "address.hpp"
#include "context.hpp"

namespace wshttp
{
    class app_context;
    class endpoint;
    class inbound_session;

    class listener
    {
        friend class inbound_session;
        friend class endpoint;
        friend class event_loop;
        friend struct listen_callbacks;

        explicit listener(endpoint& e, uint16_t p) : _ep{e}, _local{p} { _init_internals(); }

      public:
        listener() = delete;

        ~listener();

      private:
        endpoint& _ep;
        ip_address _local{};
        int _fd{-1};

        tcp_listener _tcp;

        // key: remote address, value: session ptr
        std::unordered_map<ip_address, std::shared_ptr<inbound_session>> _sessions;

        void _init_internals();

      protected:
        SSL* new_ssl();

        void close_all();

        void close_listener();

        void close_session(ip_address remote);

        void create_inbound_session(ip_address remote, evutil_socket_t fd);
    };
}  //  namespace wshttp
