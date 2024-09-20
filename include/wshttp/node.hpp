#pragma once

#include "address.hpp"

namespace wshttp
{
    struct uri;
    class endpoint;
    class app_context;

    class node_old
    {
        friend class session;
        friend class endpoint;

        template <typename... Opt>
        explicit node_old(endpoint& e, uri _u, Opt&&... opts) : _ep{e}, _uri{std::move(_u)}
        {
            _ctx = std::make_shared<app_context>(IO::OUTBOUND, std::forward<Opt>(opts)...);
            _init_internals();
        }

      public:
        node_old() = delete;

        ~node_old() = default;

        template <typename... Opt>
        static std::shared_ptr<node_old> make(endpoint& e, uri _u, Opt&&... opts)
        {
            return std::shared_ptr<node_old>{new node_old{e, std::move(_u), std::forward<Opt>(opts)...}};
        }

        std::string_view href() const { return _uri.href(); }

      private:
        endpoint& _ep;
        uint16_t _port;
        ip_address _local;
        int _fd;

        uri _uri;

        bufferevent_ptr _bev;
        std::shared_ptr<app_context> _ctx;

        void _init_internals();

      protected:
        SSL* new_ssl();
    };
}  //  namespace wshttp

namespace std
{
    template <>
    struct hash<wshttp::node_old>
    {
        size_t operator()(const wshttp::node_old& n) const noexcept { return hash<string_view>{}(n.href()); };
    };
}  // namespace std
