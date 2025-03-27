#pragma once

#include "request.hpp"
#include "types.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    namespace deleters
    {
        struct _bufferevent
        {
            inline void operator()(::bufferevent* b) const { bufferevent_free(b); }
        };

        struct _evconn
        {
            inline void operator()(::evhttp_connection* c) const { evhttp_connection_free(c); }
        };
    }  // namespace deleters

    using bufferevent_ptr = std::unique_ptr<::bufferevent, deleters::_bufferevent>;

    using evconn_ptr = std::unique_ptr<::evhttp_connection, deleters::_evconn>;

    struct inbound_request
    {
        friend class listener;

        explicit inbound_request(/* listener& l, */ ip_address remote, evutil_socket_t sock);

        ~inbound_request();

      private:
        // listener& _l;

        path _path;

        evutil_socket_t _fd{-1};

        // request handlers
        void recv_unsupported(struct evhttp_request* req);
        void recv_get(struct evhttp_request* req);
        void recv_post(struct evhttp_request* req);
        void recv_head(struct evhttp_request* req);
        void recv_put(struct evhttp_request* req);
        void recv_delete(struct evhttp_request* req);

        using request_handler = void (inbound_request::*)(struct evhttp_request* req);

        std::array<request_handler, 6> handlers{
                &inbound_request::recv_unsupported,
                &inbound_request::recv_get,
                &inbound_request::recv_post,
                &inbound_request::recv_head,
                &inbound_request::recv_put,
                &inbound_request::recv_delete};

      protected:
        void recv_request(struct evhttp_request* req);

      public:
        //
    };

    class outbound_session final : public socket_interface
    {
        friend class endpoint;
        friend struct outbound_callbacks;
        friend struct outbound_ptr_hash;
        friend struct outbound_ptr_comp;

      public:
        explicit outbound_session(endpoint& e, ev_uri u);

        outbound_session() = delete;

        ~outbound_session();

      private:
        endpoint& _ep;
        evconn_ptr _evconn;

        // uri _uri;
        ev_uri _evuri;

        const bool _use_tls{true};

        SSL* new_ssl() override;

        bufferevent* new_bev() override;

        void close() override;

        bool make_request_base();

        void recv_response(struct evhttp_request* req);

      protected:
        void initiate_request(METHOD method);

        std::optional<http_request> make_request(METHOD method);

      public:
        auto operator<=>(const outbound_session& o) const { return _evuri <=> o._evuri; }
        bool operator==(const outbound_session& o) const { return (*this <=> o) == 0; }
        bool operator==(const ev_uri& u) const { return _evuri == u; }
    };

    struct outbound_ptr_comp
    {
        using is_transparent = void;

        bool operator()(const std::shared_ptr<outbound_session>& lhs, const std::shared_ptr<outbound_session>& rhs)
                const noexcept
        {
            return *lhs == *rhs;
        }

        bool operator()(const std::shared_ptr<outbound_session>& lhs, const ev_uri& rhs) const noexcept
        {
            return *lhs == rhs;
        }

        bool operator()(const ev_uri& lhs, const std::shared_ptr<outbound_session>& rhs) const noexcept
        {
            return *rhs == lhs;
        }
    };

    struct outbound_ptr_hash
    {
        using is_transparent = void;
        using transparent_key_eq = outbound_ptr_comp;

        size_t operator()(const std::shared_ptr<outbound_session>& o) const noexcept
        {
            return std::hash<ev_uri>{}(o->_evuri);
        }

        size_t operator()(const ev_uri& u) const noexcept { return std::hash<ev_uri>{}(u); }
    };

    // Holds shared pointers to outbound_session objects, which can be searched using uri's as transparent keys
    using outbound_ptr_set = std::
            unordered_set<std::shared_ptr<outbound_session>, outbound_ptr_hash, outbound_ptr_hash::transparent_key_eq>;

}  // namespace wshttp
