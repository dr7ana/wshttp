#pragma once

#include "address.hpp"
#include "context.hpp"

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
        explicit listener(endpoint& e, uint16_t p, Opt&&... opts) : _ep{e}, _local{p}
        {
            if constexpr (sizeof...(opts))
                handle_lst_opt(std::forward<Opt>(opts)...);
            _init_internals();
        }

        template <typename... Opt>
        explicit listener(endpoint& e, uri _u, Opt&&... opts) : _ep{e}
        {
            if constexpr (sizeof...(opts))
                handle_lst_opt(std::forward<Opt>(opts)...);
            _init_internals();
            create_outbound_session(std::move(_u));
        }

      public:
        listener() = delete;

        ~listener();

      private:
        endpoint& _ep;
        ip_address _local{};
        int _fd{-1};

        std::shared_ptr<app_context> _ctx;

        tcp_listener _tcp;

        // key: remote address, value: session ptr
        std::unordered_map<ip_address, std::shared_ptr<session>> _sessions;

        void _init_internals();

        void handle_lst_opt(std::shared_ptr<ssl_creds> c);

      protected:
        // SSL* new_ssl(IO dir);
        SSL* new_ssl();

        void close_all();

        void close_listener();

        void close_session(ip_address remote);

        void create_outbound_session(uri _u);

        void create_inbound_session(ip_address remote, evutil_socket_t fd);
    };
}  //  namespace wshttp
