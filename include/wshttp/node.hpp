#pragma once

#include "types.hpp"

namespace wshttp
{
    struct uri;
    class endpoint;
    class app_context;

    class node
    {
        template <typename... Opt>
        explicit node(endpoint& e, Opt&&... opts) : _ep{e}
        {
            _ctx = std::make_shared<app_context>(IO::OUTBOUND, std::forward<Opt>(opts)...);
            _init_internals();
        }

      public:
        node() = delete;

        ~node() = default;

        template <typename... Opt>
        static std::shared_ptr<node> make(endpoint& e, Opt&&... opts)
        {
            return std::shared_ptr<node>{new node{e, std::forward<Opt>(opts)...}};
        }

      private:
        endpoint& _ep;
        uint16_t _port;

        std::shared_ptr<app_context> _ctx;

        void _init_internals();
    };
}  //  namespace wshttp
