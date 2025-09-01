#pragma once

#include "opts.hpp"
#include "request.hpp"

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

    struct session_base
    {
        //
    };

    struct inbound_session
    {
        friend class listener;

        explicit inbound_session(
                /* listener& l, */ ip_address remote,
                evutil_socket_t sock,
                std::optional<inbound_opts> opts = std::nullopt);

        ~inbound_session();

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

        using request_handler_direct = void (inbound_session::*)(struct evhttp_request* req);

        std::array<request_handler_direct, 10> handlers{
                &inbound_session::recv_unsupported,
                &inbound_session::recv_get,
                &inbound_session::recv_post,
                &inbound_session::recv_head,
                &inbound_session::recv_put,
                &inbound_session::recv_delete,
                &inbound_session::recv_options,
                &inbound_session::recv_trace,
                &inbound_session::recv_connect,
                &inbound_session::recv_patch};

      protected:
        void recv_request(struct evhttp_request* req);

      public:
        //
    };

    /** TODO:
            - interface base class for outbound_session to be derived from
            - pure virtual methods to be exposed publicly
     */
    struct session_interface
    {
        //
    };

    class outbound_session final : public socket_interface
    {
      protected:
        explicit outbound_session(endpoint& ep, uri_ptr u, std::optional<session_opts> opts = std::nullopt);

      public:
        // static std::shared_ptr<outbound_session> make_outbound(endpoint& e, uri u);

        outbound_session() = delete;

        ~outbound_session();

      private:
        endpoint& _ep;

        // initialize in this order
        // uri _uri;

        content_type _default_type{content_type::WILDCARD};

        request_data_cb _hook;

        evconn_ptr _evconn;

        uri_ptr _uri;

        std::atomic<request_id_t> _next_request_id{};

        // request_ptr_set _request_que;

        request_ptr_list _request_que;

        std::unordered_map<request_id_t, request_ptr_list::iterator> _request_table;

        void populate_opts(session_opts opts);

        SSL* new_ssl() override;

        void close() override;

      protected:
        bufferevent* new_bev(bool ssl = true) override;

        request_data_cb make_req_data_caller();

        request_data_cb make_req_data_caller(request_data_cb cb);

        // Called internally
        void initiate_request(METHOD method, uri_ptr req_uri, std::optional<request_opts> opts = std::nullopt);

        // void initiate_request(uri_ptr u, METHOD method);

        // void recv_request(struct evhttp_request* req);

        void close_request(request_id_t id);

      public:
        // Called externally
        void request(METHOD method, std::optional<request_opts> opts = std::nullopt);
        void request(METHOD method, std::string_view path, std::optional<request_opts> opts = std::nullopt);

        auto operator<=>(const outbound_session& o) const { return _uri->host_domain() <=> o._uri->host_domain(); }
        bool operator==(const outbound_session& o) const { return (*this <=> o) == 0; }

        friend class endpoint;
        friend struct http_request;
        friend struct outbound_callbacks;
        friend class event_loop;
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
