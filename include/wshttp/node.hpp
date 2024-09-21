#pragma once

#include "address.hpp"

namespace wshttp
{
    struct uri;
    class endpoint;
    class outbound_session;
    class app_context;

    class node
    {
        friend class outbound_session;
        friend class endpoint;

        template <typename... Opt>
        explicit node(endpoint& e, uri _u, Opt&&... opts) : _ep{e}, _uri{std::move(_u)}
        {
            if (sizeof...(opts))
                handle_nd_opts(std::forward<Opt>(opts)...);
            _init_internals();
            create_outbound_session();
        }

      public:
        node() = delete;

        ~node();

        std::string_view href() const { return _uri.href(); }

      private:
        endpoint& _ep;
        std::optional<ip_address> _local;
        int _fd{-1};

        uri _uri;

        // key: host, value: session ptr
        std::unordered_map<std::string, std::shared_ptr<outbound_session>> _sessions;

        void _init_internals();

        void handle_nd_opts(ip_address local);

      protected:
        SSL* new_ssl();

        void close_session(std::string host);

        void create_outbound_session();
    };
}  //  namespace wshttp

namespace std
{
    template <>
    struct hash<wshttp::node>
    {
        size_t operator()(const wshttp::node& n) const noexcept { return hash<string_view>{}(n.href()); };
    };
}  // namespace std
