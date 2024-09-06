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
        friend struct listener_callbacks;
        friend class session;

        template <typename... Opt>
        explicit listener(endpoint& e, uint16_t p, Opt&&... opts) : _ep{e}, _port{p}
        {
            _ctx = std::make_shared<app_context>(IO::INBOUND, std::forward<Opt>(opts)...);
            _init_internals();
        }

      public:
        listener() = delete;

        ~listener() = default;

        template <typename... Opt>
        static std::shared_ptr<listener> make(endpoint& e, uint16_t _port, Opt&&... opts)
        {
            return std::shared_ptr<listener>{new listener{e, _port, std::forward<Opt>(opts)...}};
        }

      private:
        endpoint& _ep;
        const uint16_t _port;

        tcp_listener _tcp;
        std::shared_ptr<app_context> _ctx;

        // key: remote address, value: session ptr
        std::unordered_map<ip_address, std::shared_ptr<session>> _sessions;

        void _init_internals();

      public:
        void create_inbound_session(ip_address remote, evutil_socket_t fd);
    };
}  //  namespace wshttp
