#pragma once

#include "types.hpp"

namespace wshttp
{
    struct uri;
    class Endpoint;
    class app_context;

    class Node
    {
        template <typename... Opt>
        explicit Node(Endpoint& e, Opt&&... opts) : _ep{e}
        {
            _ctx = std::make_shared<app_context>(IO::OUTBOUND, std::forward<Opt>(opts)...);
            _init_internals();
        }

      public:
        Node() = delete;

        ~Node() = default;

        template <typename... Opt>
        static std::shared_ptr<Node> make(Endpoint& e, Opt&&... opts)
        {
            return std::shared_ptr<Node>{new Node{e, std::forward<Opt>(opts)...}};
        }

      private:
        Endpoint& _ep;
        uint16_t _port;

        std::shared_ptr<app_context> _ctx;

        void _init_internals();
    };
}  //  namespace wshttp
