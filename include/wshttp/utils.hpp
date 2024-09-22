#pragma once

extern "C"
{
#include <arpa/inet.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/bufferevent_struct.h>
#include <event2/dns.h>
#include <event2/dns_struct.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>
#include <netinet/tcp.h>
#include <nghttp2/nghttp2.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/types.h>
}

#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
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

    inline constexpr auto localhost = "127.0.0.1"sv;

    inline constexpr size_t inverse_golden_ratio = sizeof(size_t) >= 8 ? 0x9e37'79b9'7f4a'7c15 : 0x9e37'79b9;

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

        std::string_view translate_req_type(int t);

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
