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
        explicit outbound_session(endpoint& e, uri u);

        outbound_session() = delete;

        ~outbound_session();

      private:
        endpoint& _ep;
        evconn_ptr _evconn;

        uri _uri;

        const bool _use_tls{true};

        SSL* new_ssl() override;

        bufferevent* new_bev() override;

        void close() override;

        bool make_request_base();

      protected:
        void initiate_request();

        void make_request();

        void recv_response(struct evhttp_request* req);

      public:
        auto operator<=>(const outbound_session& o) const { return _uri <=> o._uri; }
        bool operator==(const outbound_session& o) const { return (*this <=> o) == 0; }
        bool operator==(const uri& u) const { return _uri == u; }
    };

    struct outbound_ptr_comp
    {
        using is_transparent = void;

        bool operator()(const std::shared_ptr<outbound_session>& lhs, const std::shared_ptr<outbound_session>& rhs)
                const noexcept
        {
            return *lhs == *rhs;
        }

        bool operator()(const std::shared_ptr<outbound_session>& lhs, const uri& rhs) const noexcept
        {
            return *lhs == rhs;
        }

        bool operator()(const uri& lhs, const std::shared_ptr<outbound_session>& rhs) const noexcept
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
            return std::hash<uri>{}(o->_uri);
        }

        size_t operator()(const uri& u) const noexcept { return std::hash<uri>{}(u); }
    };

    // Holds shared pointers to outbound_session objects, which can be searched using uri's as transparent keys
    using outbound_ptr_set = std::
            unordered_set<std::shared_ptr<outbound_session>, outbound_ptr_hash, outbound_ptr_hash::transparent_key_eq>;

}  // namespace wshttp
