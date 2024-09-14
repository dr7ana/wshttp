#pragma once

#include "utils.hpp"

#include <cstdint>

namespace wshttp
{
    namespace defaults
    {
        inline constexpr uint16_t DNS_PORT{4400};

        inline constexpr auto ALPN = "h2"_usv;

    }  // namespace defaults

    namespace deleters
    {
        struct _event
        {
            inline void operator()(::event* e) const { ::event_free(e); };
        };

        struct _session
        {
            inline void operator()(nghttp2_session* s) const { nghttp2_session_del(s); };
        };

        struct _evdns
        {
            inline void operator()(::evdns_base* e) const { ::evdns_base_free(e, 1); };
        };

        struct _evdns_port
        {
            inline void operator()(::evdns_server_port* e) const { ::evdns_close_server_port(e); };
        };

        struct _bufferevent
        {
            inline void operator()(::bufferevent* b) { bufferevent_free(b); };
        };

        struct _evconnlistener
        {
            inline void operator()(::evconnlistener* e) const { ::evconnlistener_free(e); };
        };

        struct _ssl_ctx
        {
            inline void operator()(SSL_CTX* s) const { SSL_CTX_free(s); };
        };

        struct _ssl
        {
            inline void operator()(SSL* s) const { SSL_shutdown(s); };
        };

    }  //  namespace deleters

    using tcp_listener = std::shared_ptr<evconnlistener>;

    using session_ptr = std::shared_ptr<::nghttp2_session>;

    using ssl_ptr = std::unique_ptr<::SSL, deleters::_ssl>;
    using ssl_ctx_ptr = std::unique_ptr<::SSL_CTX, deleters::_ssl_ctx>;

    using event_ptr = std::unique_ptr<::event, deleters::_event>;

    using bufferevent_ptr = std::unique_ptr<::bufferevent, deleters::_bufferevent>;

    namespace concepts
    {
        template <typename T>
        concept string_view_compatible =
            std::convertible_to<T, std::string_view> || std::convertible_to<T, std::basic_string_view<unsigned char>>
            || std::convertible_to<T, std::basic_string_view<std::byte>>;

        template <typename T>
        concept endian_swappable_type =
            std::integral<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

        template <typename T>
        concept nghttp2_session_type = std::same_as<T, nghttp2_session>;

        template <typename T>
        concept nghttp2_nv_type = std::same_as<T, nghttp2_nv>;

        template <typename T>
        concept nghttp2_settings_type = std::same_as<T, nghttp2_settings_entry>;
    }  // namespace concepts

    enum class IO { INBOUND, OUTBOUND };

    namespace req
    {
        enum class FIELD { method, scheme, authority, path, status };
        enum class STATUS { _200, _404 };
    }  //  namespace req

    namespace detail
    {}
}  //  namespace wshttp
