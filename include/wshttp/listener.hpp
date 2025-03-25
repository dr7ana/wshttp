#pragma once

#include "address.hpp"
#include "loop.hpp"
#include "ssl.hpp"

extern "C" {
#include <event2/listener.h>
}

namespace wshttp
{
    class socket_interface
    {
      protected:
        virtual SSL* new_ssl() = 0;
        virtual bufferevent* new_bev() = 0;
        virtual void close() = 0;
    };

    class app_context;
    class endpoint;
    struct inbound_request;
    struct ws_session_base;

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

    using tcp_listener = std::unique_ptr<evconnlistener, deleters::_evconnlistener>;
    using evhttp_ptr = std::unique_ptr<::evhttp, deleters::_evhttp>;

    class listener final : public socket_interface
    {
        friend struct inbound_request;
        friend struct ws_session_base;
        friend class endpoint;
        friend class event_loop;
        friend struct listen_callbacks;

        explicit listener(endpoint& e, ip_address bind);
        explicit listener(endpoint& e, uint16_t p) : listener{e, ip_address{p}} {}

      public:
        listener() = delete;

        ~listener();

      private:
        endpoint& _ep;
        ip_address _local{};
        int _fd{-1};

        tcp_listener _tcp;

        evhttp_ptr _evh;

        // key: remote address, value: session ptr
        std::unordered_map<ip_address, std::shared_ptr<inbound_request>> _requests;

        // TODO: unify requests and ws sessions with base class to use the same map
        std::unordered_map<ip_address, std::shared_ptr<ws_session_base>> _sessions;

        void _init_internals();

      protected:
        int recv_request(struct evhttp_request* req);

        void handle_request(struct evhttp_request* req);

        int request_error(struct evhttp_request* req, int error, const char* reason);

        void ws_request(struct evhttp_request* req);

        bufferevent* new_bev() override;

        SSL* new_ssl() override;

        void close_all();

        void close() override;

        void close_request(ip_address remote);

        void close_ws(ip_address remote);
    };
}  //  namespace wshttp
