#pragma once

#include "listener.hpp"
#include "opts.hpp"

#include <list>

namespace wshttp {
    namespace deleters {
        struct _evconn {
            inline void operator()(::evhttp_connection* c) const { evhttp_connection_free(c); }
        };

        struct _evreq {
            inline void operator()(::evhttp_request* r) const { evhttp_request_free(r); }
        };
    }  // namespace deleters

    struct http_request;
    class outbound_session;

    using http_req_ptr = std::unique_ptr<http_request>;

    using evconn_ptr = std::unique_ptr<::evhttp_connection, deleters::_evconn>;

    using evreq_ptr = std::unique_ptr<evhttp_request, deleters::_evreq>;
    using request_ptr_list = std::list<http_req_ptr>;

    /**
    - case insensitive; lowercase preferred (RFC 9110)
    evhttp_set_default_content_type

    Some media types
    - "* / *" (no spaces, in documentation by alpaca)
    - "application/json"
    - "application/misc"
    - "application/octet-stream" (arbitary binary data)
    - "application/x-www-form-urlencoded"
    - "application/xml"
        - "application/zip"
        - "application/zstd"
        - "application/ *" (no space)
        - "image/gif"
        - "image/jpeg"
        - "image/png"
    - "text/css"
    - "text/csv"
    - "text/event-stream"
    - "text/html"
        + "; charset=utf-8"
        + "; charset=ISO-8859-1"
    - "text/plain"
        - "text/ *" (no space)

    Request-only headers:
    - "Accept"
        - accepted media types
    - "Host"
        - internal only

    Header fields:
    - "Connection"
        - "keep-alive", "close"
            - client needs to confirm server eechos keep-alive
            - libevent calls close if needed
                - if client expects it, no action needed
                    if not, should probably log early close?
        - "Upgrade"
    - "Content-Type"
        - libevent default: "text/html; charset=ISO-8859-1"
        - accepted media types
    - "Content-Length"
        - added by libevent if absent
    - "User-Agent"
    - "X-Request-ID"
        - must be held onto for alpaca for support requests
     */

    /** User facing request flags
        - close: do not persist connection

        User facing header options:
        - media type
        - user-agent string

        User facing body options:
        - media type

     */

    // wrapper for am evhttp_request
    struct http_request {
        http_request() = delete;

        static http_req_ptr construct(
                outbound_session& s,
                request_id_t id,
                uri_ptr u,
                METHOD m,
                std::optional<request_opts> opts = std::nullopt);

        static std::shared_ptr<http_request> construct_inbound(struct evhttp_request* req, request_id_t id);

        ~http_request();

      protected:
        // explicit http_request(outbound_session& s, request_id_t id, uri_ptr u, METHOD m);

        explicit http_request(
                outbound_session& s,
                request_id_t id,
                uri_ptr u,
                METHOD m,
                std::optional<request_opts> opts = std::nullopt);

        explicit http_request(request_id_t id, struct evhttp_request* req);

      private:
        const request_id_t _request_id;

        // TODO: make this a ptr to a socket_interface for the http_request base
        // Keep in order, hook made by generator
        outbound_session* _session{nullptr};
        request_data_cb _hook;  // TODO: outbound only

        evreq_ptr _req;
        // uri _uri;
        uri_ptr _uri;

        METHOD _method;
        content_type _type;
        content_type _accept;
        std::optional<std::string> _user_agent;
        std::optional<std::vector<char>> _request_body;
        std::optional<std::vector<char>> _response_body;
        int _response_code{};
        std::string _response_line{};

        std::atomic<bool> _close_session_on_complete{false};

        void populate_opts();

        void populate_opts(request_opts opts);

        void signal_close(bool close_session = false);

        void set_close_on_complete();

      public:
        void test_method();

        request_id_t request_id() const { return _request_id; }

        METHOD method() const { return _method; }

        std::string_view request_body() const {
            if (_request_body)
                return std::string_view{_request_body->data(), _request_body->size()};
            return {};
        }

        std::string_view response_body() const {
            if (_response_body)
                return std::string_view{_response_body->data(), _response_body->size()};
            return {};
        }

        int response_code() const { return _response_code; }

        std::string_view response_line() const { return _response_line; }

        void request_recv(struct evhttp_request* req);

        std::string to_string() const;

        static constexpr bool to_string_formattable = true;

        auto operator<=>(const http_request& req) const {
            return std::tie(_request_id, *_uri) <=> std::tie(req._request_id, *req._uri);
        }
        bool operator==(const http_request& req) const { return (*this <=> req) == 0; }
        bool operator==(evhttp_request* req) const { return _req.get() == req; }

        template <typename T, typename U = std::remove_cv_t<T>>
            requires std::same_as<U, evhttp_request>
        operator T*() {
            return _req.get();
        }

        friend class outbound_session;
        friend struct request_ptr_hash;
    };

    inline auto hash_evreq_ptr = [](evhttp_request* r) noexcept -> size_t {
        return reinterpret_cast<std::uintptr_t>(r);
    };

    struct request_ptr_comp {
        using is_transparent = void;

        bool operator()(const http_req_ptr& lhs, const http_req_ptr& rhs) const noexcept { return *lhs == *rhs; }

        bool operator()(const http_req_ptr& lhs, evhttp_request* rhs) const noexcept { return *lhs == rhs; }

        bool operator()(evhttp_request* lhs, const http_req_ptr& rhs) const noexcept { return lhs == *rhs; }
    };

    struct request_ptr_hash {
        using is_transparent = void;
        using transparent_key_eq = request_ptr_comp;

        size_t operator()(const http_req_ptr& r) const noexcept { return hash_evreq_ptr(r->_req.get()); }

        size_t operator()(evhttp_request* r) const noexcept { return hash_evreq_ptr(r); }
    };

    // Holds unique pointers to http request objects; owning endpoints can search based on evhttp_request pointer hash
    using request_ptr_set = std::unordered_set<http_req_ptr, request_ptr_hash, request_ptr_hash::transparent_key_eq>;

}  //  namespace wshttp

namespace std {
    // template <>
    // struct hash<wshttp::http_req_ptr>
    // {
    //     size_t operator()(const wshttp::http_req_ptr& r) const noexcept { return wshttp::hash_evreq_ptr(r->_req); }
    // };
}  // namespace std
