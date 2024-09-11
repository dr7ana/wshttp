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
        inline constexpr auto event_d = [](::event* e) {
            if (e)
                ::event_free(e);
        };

        inline constexpr auto session_d = [](nghttp2_session* s) {
            if (s)
                nghttp2_session_del(s);
        };

        inline constexpr auto evdns_d = [](::evdns_base* e) {
            if (e)
                ::evdns_base_free(e, 1);
        };

        inline constexpr auto evdns_port_d = [](::evdns_server_port* e) {
            if (e)
                evdns_close_server_port(e);
        };

        inline constexpr auto evconnlistener_d = [](::evconnlistener* e) {
            if (e)
                evconnlistener_free(e);
        };

        inline constexpr auto ssl_ctx_d = [](::SSL_CTX* s) {
            if (s)
                SSL_CTX_free(s);
        };

        inline constexpr auto ssl_d = [](::SSL* s) {
            if (s)
                SSL_shutdown(s);
        };

        inline constexpr auto bufferevent_d = [](::bufferevent* b) {
            if (b)
                bufferevent_free(b);
        };
    }  //  namespace deleters

    using tcp_listener = std::shared_ptr<evconnlistener>;

    using session_ptr = std::shared_ptr<::nghttp2_session>;

    using ssl_ptr = std::unique_ptr<::SSL, decltype(deleters::ssl_d)>;
    using ssl_ctx_ptr = std::unique_ptr<::SSL_CTX, decltype(deleters::ssl_ctx_d)>;

    using event_ptr = std::unique_ptr<::event, decltype(deleters::event_d)>;

    using bufferevent_ptr = std::unique_ptr<::bufferevent, decltype(deleters::bufferevent_d)>;

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
