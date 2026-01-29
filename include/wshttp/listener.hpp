#pragma once

#include "address.hpp"
#include "opts.hpp"
#include "ssl.hpp"

#include <uneventful/loop.hpp>

extern "C" {
#include <event2/listener.h>
}

namespace wshttp {
    class socket_interface {
      protected:
        virtual SSL* new_ssl() = 0;
        virtual bufferevent* new_bev(bool) = 0;
        virtual void close() = 0;
    };

    class app_context;
    class endpoint;
    struct http_request;
    struct ws_session_base;

    namespace deleters {
        struct _evconnlistener {
            inline void operator()(::evconnlistener* e) const { ::evconnlistener_free(e); }
        };

        struct _evhttp {
            inline void operator()(::evhttp* e) const { return evhttp_free(e); }
        };
    }  // namespace deleters

    using tcp_listener = std::unique_ptr<evconnlistener, deleters::_evconnlistener>;
    using evhttp_ptr = std::unique_ptr<::evhttp, deleters::_evhttp>;

    using request_handler_hook = std::function<void(std::shared_ptr<http_request>)>;

    class listener final : public socket_interface {
        friend struct ws_session_base;
        friend class endpoint;
        friend class un::event::event_loop;
        friend struct listen_callbacks;

        explicit listener(endpoint& e, ip_address bind, std::optional<inbound_opts> opts = std::nullopt);
        explicit listener(endpoint& e, uint16_t p, std::optional<inbound_opts> opts = std::nullopt) :
                listener{e, ip_address{p}, std::move(opts)} {}

      public:
        listener() = delete;

        ~listener();

      private:
        endpoint& _ep;
        ip_address _local{};
        int _fd{-1};

        tcp_listener _tcp;

        evhttp_ptr _evh;

        std::optional<inbound_opts> _iopts;

        std::atomic<request_id_t> _next_request_id{};
        std::unordered_map<request_id_t, std::shared_ptr<http_request>> _requests;

        // TODO: unify requests and ws sessions with base class to use the same map
        std::unordered_map<ip_address, std::shared_ptr<ws_session_base>> _sessions;

        std::array<request_handler_hook, 10> request_handlers{};

        void _init_internals();

        void _register_handlers();

      protected:
        int recv_request(struct evhttp_request* req);

        void handle_request(struct evhttp_request* req);

        int request_error(struct evhttp_request* req, int error, const char* reason);

        void request_complete(request_id_t id);

        void ws_request(struct evhttp_request* req);

        bufferevent* new_bev(bool ssl = true) override;

        SSL* new_ssl() override;

        void close_all();

        void close() override;

        void close_ws(ip_address remote);
    };
}  //  namespace wshttp
