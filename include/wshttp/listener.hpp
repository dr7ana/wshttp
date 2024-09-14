#pragma once

#include "address.hpp"
#include "types.hpp"

namespace wshttp
{
    class app_context;
    class endpoint;
    class session;

    class listener
    {
        friend class session;
        friend class endpoint;
        friend class event_loop;
        friend struct listen_callbacks;

        template <typename... Opt>
        explicit listener(endpoint& e, uint16_t p, Opt&&... opts) : _ep{e}, _port{p}
        {
            _ctx = std::make_shared<app_context>(IO::INBOUND, std::forward<Opt>(opts)...);
            _init_internals();
        }

      public:
        listener() = delete;

        ~listener();

      private:
        endpoint& _ep;
        const uint16_t _port;
        ip_address _local;
        int _fd;

        tcp_listener _tcp;
        std::shared_ptr<app_context> _ctx;

        // key: remote address, value: session ptr
        std::unordered_map<ip_address, std::shared_ptr<session>> _sessions;

        void _init_internals();

      protected:
        SSL* new_ssl();

        void close_all();

        void close_session(ip_address remote);

      public:
        void create_inbound_session(ip_address remote, evutil_socket_t fd);
    };
}  //  namespace wshttp
