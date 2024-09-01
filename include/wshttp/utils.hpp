#pragma once

extern "C"
{
#include <arpa/inet.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/dns.h>
#include <event2/dns_struct.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>
#include <nghttp2/nghttp2.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/types.h>
}

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <queue>
#include <unordered_map>

namespace wshttp
{
    using namespace std::literals;
    namespace fs = std::filesystem;

    using bstring = std::basic_string<std::byte>;
    using ustring = std::basic_string<unsigned char>;
    using bstring_view = std::basic_string_view<std::byte>;
    using ustring_view = std::basic_string_view<unsigned char>;

    using tcp_listener = std::shared_ptr<evconnlistener>;

    inline constexpr auto localhost = "127.0.0.1"sv;

    inline constexpr auto event_deleter = [](::event* e) {
        if (e)
            ::event_free(e);
    };

    inline constexpr auto session_deleter = [](nghttp2_session* s) {
        if (s)
            nghttp2_session_del(s);
    };

    inline constexpr auto evdns_deleter = [](::evdns_base* e) {
        if (e)
            ::evdns_base_free(e, 1);
    };

    inline constexpr auto evdns_port_deleter = [](::evdns_server_port* e) {
        if (e)
            evdns_close_server_port(e);
    };

    inline constexpr auto evconnlistener_deleter = [](::evconnlistener* e) {
        if (e)
            evconnlistener_free(e);
    };

    inline constexpr auto ssl_deleter = [](::SSL_CTX* s) {
        if (s)
            SSL_CTX_free(s);
    };

    namespace detail
    {
        template <size_t N>
        struct bsv_literal
        {
            consteval bsv_literal(const char (&s)[N])
            {
                for (size_t i = 0; i < N; i++)
                    str[i] = static_cast<std::byte>(s[i]);
            }
            std::byte str[N];  // we keep the null on the end, in case you pass .data() to a C func
            using size = std::integral_constant<size_t, N - 1>;
        };

        template <size_t N>
        struct usv_literal
        {
            consteval usv_literal(const char (&s)[N])
            {
                for (size_t i = 0; i < N; i++)
                    str[i] = static_cast<unsigned char>(s[i]);
            }
            unsigned char str[N];
            using size = std::integral_constant<size_t, N - 1>;
        };

        std::chrono::steady_clock::time_point get_time();

        std::string translate_req_type(int t);

        void print_req(struct evdns_server_request* req);

        std::string translate_req_class(int t);

        std::string localhost_ip(uint16_t port);

        template <std::integral T>
        constexpr bool increment_will_overflow(T val)
        {
            return std::numeric_limits<T>::max() == val;
        }

    }  // namespace detail

    inline ustring operator""_us(const char* str, size_t len) noexcept
    {
        return {reinterpret_cast<const unsigned char*>(str), len};
    }

    template <detail::usv_literal UStr>
    constexpr ustring_view operator""_usv()
    {
        return {UStr.str, decltype(UStr)::size::value};
    }

    template <detail::bsv_literal BStr>
    constexpr bstring_view operator""_bsv()
    {
        return {BStr.str, decltype(BStr)::size::value};
    }

    inline bstring operator""_bs(const char* str, size_t len) noexcept
    {
        return {reinterpret_cast<const std::byte*>(str), len};
    }

}  //  namespace wshttp
