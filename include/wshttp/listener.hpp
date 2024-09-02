#pragma once

#include "address.hpp"
#include "types.hpp"

namespace wshttp
{
    class Endpoint;
    class app_context;

    class Listener
    {
        template <typename... Opt>
        explicit Listener(Endpoint& e, uint16_t p, Opt&&... opts) : _ep{e}, _port{p}
        {
            _ctx = std::make_shared<app_context>(IO::INBOUND, std::forward<Opt>(opts)...);
            _init_internals();
        }

      public:
        Listener() = delete;

        ~Listener() = default;

        template <typename... Opt>
        static std::shared_ptr<Listener> make(Endpoint& e, uint16_t _port, Opt&&... opts)
        {
            return std::shared_ptr<Listener>{new Listener{e, _port, std::forward<Opt>(opts)...}};
        }

      private:
        Endpoint& _ep;
        const uint16_t _port;

        tcp_listener _tcp;
        std::shared_ptr<app_context> _ctx;

        void _init_internals();

      public:
        void create_inbound_session(ip_address local, ip_address remote);
    };
}  //  namespace wshttp
