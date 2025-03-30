#pragma once

#include "listener.hpp"

#include <bitset>

namespace wshttp
{
    enum class METHOD : uint8_t {
        UNSUPPORTED = 0,
        GET = 1,
        POST = 2,
        HEAD = 3,
        PUT = 4,
        DELETE = 5,
        OPTIONS = 6,
        TRACE = 7,
        CONNECT = 8,
        PATCH = 9
    };

    template <typename T>
    concept supported_method = std::is_same_v<T, METHOD> && requires(T a) { std::to_underlying(a) > 0; };

    struct hdr
    {
        static constexpr auto* host = "Host";
        static constexpr auto* conn = "Connection";
        static constexpr auto* close = "close";
    };

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
    enum class hdr_flags : uint8_t {};

    namespace deleters
    {
        struct _evconn
        {
            inline void operator()(::evhttp_connection* c) const { evhttp_connection_free(c); }
        };

        struct _evreq
        {
            inline void operator()(::evhttp_request* r) const
            {
                if (r and evhttp_request_is_owned(r))
                    evhttp_request_free(r);
            }
        };
    }  // namespace deleters

    struct http_request;
    class outbound_session;

    using http_req_ptr = std::unique_ptr<http_request>;
    using evreq_ptr = std::unique_ptr<evhttp_request, deleters::_evreq>;
    using evconn_ptr = std::unique_ptr<::evhttp_connection, deleters::_evconn>;

    using request_id_t = size_t;

    // wrapper for am evhttp_request
    struct http_request
    {
        http_request() = delete;

        static http_req_ptr construct(outbound_session& s, request_id_t id, uri u, METHOD m);

      protected:
        explicit http_request(outbound_session& s, request_id_t id, uri u, METHOD m);

      private:
        const request_id_t _request_id;

        outbound_session& _session;

        evreq_ptr _req;
        evkeyvalq* _buffer = nullptr;

        // new fields
        evconn_ptr _evconn;
        uri _uri;

        METHOD _method;

        void signal_close(bool close_session = false);

      public:
        void recv_response(struct evhttp_request* req);

        auto operator<=>(const http_request& req)
        {
            return std::tie(_request_id, _uri) <=> std::tie(req._request_id, req._uri);
        }
        bool operator==(const http_request& req) { return (*this <=> req) == 0; }

        template <typename T, typename U = std::remove_cv_t<T>>
            requires std::same_as<U, evhttp_request>
        operator T*()
        {
            return _req;
        }

        friend class outbound_session;
    };
}  //  namespace wshttp
