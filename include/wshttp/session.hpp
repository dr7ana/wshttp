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
    }  // namespace deleters

    using bufferevent_ptr = std::unique_ptr<::bufferevent, deleters::_bufferevent>;

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
        void recv_options(struct evhttp_request* req);
        void recv_trace(struct evhttp_request* req);
        void recv_connect(struct evhttp_request* req);
        void recv_patch(struct evhttp_request* req);

        using request_handler = void (inbound_request::*)(struct evhttp_request* req);

        std::array<request_handler, 10> handlers{
                &inbound_request::recv_unsupported,
                &inbound_request::recv_get,
                &inbound_request::recv_post,
                &inbound_request::recv_head,
                &inbound_request::recv_put,
                &inbound_request::recv_delete,
                &inbound_request::recv_options,
                &inbound_request::recv_trace,
                &inbound_request::recv_connect,
                &inbound_request::recv_patch};

      protected:
        void recv_request(struct evhttp_request* req);

      public:
        //
    };

    class outbound_session final : public socket_interface
    {
      public:
        // static std::shared_ptr<outbound_session> make_outbound(endpoint& e, uri u);

        explicit outbound_session(endpoint& ep, domain_host remote);

        outbound_session() = delete;

        ~outbound_session();

      private:
        endpoint& _ep;

        // initialize in this order
        // uri _uri;

        domain_host _remote;

        std::atomic<request_id_t> _next_request_id{};

        using request_ptr_que = std::list<http_req_ptr>;

        request_ptr_que _request_que;

        std::unordered_map<request_id_t, request_ptr_que::iterator> _request_table;

        SSL* new_ssl() override;

        void close() override;

      protected:
        bufferevent* new_bev(bool ssl = true) override;

        void initiate_request(uri u, METHOD method);

        void close_request(request_id_t id);

      public:
        auto operator<=>(const outbound_session& o) const { return _remote <=> o._remote; }
        bool operator==(const outbound_session& o) const { return (*this <=> o) == 0; }

        friend class endpoint;
        friend struct http_request;
    };

    struct outbound_ptr_comp
    {
        using is_transparent = void;

        // bool operator()(const std::shared_ptr<outbound_session>& lhs, const std::shared_ptr<outbound_session>& rhs)
        //         const noexcept
        // {
        //     return *lhs == *rhs;
        // }

        // bool operator()(const std::shared_ptr<outbound_session>& lhs, const uri& rhs) const noexcept
        // {
        //     return *lhs == rhs;
        // }

        // bool operator()(const uri& lhs, const std::shared_ptr<outbound_session>& rhs) const noexcept
        // {
        //     return *rhs == lhs;
        // }
    };

    struct outbound_ptr_hash
    {
        using is_transparent = void;
        using transparent_key_eq = outbound_ptr_comp;

        // size_t operator()(const std::shared_ptr<outbound_session>& o) const noexcept
        // {
        //     return std::hash<uri>{}(o->_uri);
        // }

        // size_t operator()(const uri& u) const noexcept { return std::hash<uri>{}(u); }
    };

    // Holds shared pointers to outbound_session objects, which can be searched using uri's as transparent keys
    // using outbound_ptr_set = std::
    //         unordered_set<std::shared_ptr<outbound_session>, outbound_ptr_hash,
    //         outbound_ptr_hash::transparent_key_eq>;

}  // namespace wshttp
