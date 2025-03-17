#pragma once

#include "address.hpp"
#include "loop.hpp"
#include "ssl.hpp"

extern "C" {
#include <event2/listener.h>
}

namespace wshttp
{
    class local_node
    {
      protected:
        ip_address _local{};

        virtual void _init_internals() = 0;

      public:
        local_node(ip_address l) : _local{std::move(l)} {}

        virtual bufferevent* new_nev() = 0;
        virtual void close() = 0;
    };

    class app_context;
    class endpoint;
    struct inbound_request;

    namespace deleters
    {
        struct _evconnlistener
        {
            inline void operator()(::evconnlistener* e) const { ::evconnlistener_free(e); }
        };

        struct _evhttp
        {
            inline void operator()(::evhttp* e) const { return evhttp_free(e); }
        };
    }  // namespace deleters

    using tcp_listener = std::shared_ptr<evconnlistener>;
    using evhttp_ptr = std::unique_ptr<::evhttp, deleters::_evhttp>;

    class listener
    {
        friend class inbound_session;
        friend class endpoint;
        friend class event_loop;
        friend struct listen_callbacks;

        explicit listener(endpoint& e, uint16_t p);

      public:
        listener() = delete;

        ~listener();

      private:
        endpoint& _ep;
        ip_address _local{};
        int _fd{-1};

        // tcp_listener _tcp;
        evhttp_ptr _evh;

        // key: remote address, value: session ptr
        std::unordered_map<ip_address, std::shared_ptr<inbound_request>> _sessions;

        // void _init_internals();

      protected:
        int accept_request(struct evhttp_request* req);

        bufferevent* new_bev();

        SSL* new_ssl();

        void close_all();

        void close_listener();

        void close_session(ip_address remote);

        void create_session(ip_address remote, evutil_socket_t fd);
    };
}  //  namespace wshttp
