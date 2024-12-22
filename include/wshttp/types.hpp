#pragma once

#include "encoding.hpp"

#include <cstdint>
#include <span>

namespace wshttp
{
    inline namespace types
    {
        template <enc::basic_char T = char, size_t N = std::dynamic_extent>
        using const_span = std::span<const T, N>;
    }  // namespace types

    using cspan = const_span<char>;
    using uspan = const_span<unsigned char>;
    using bspan = const_span<std::byte>;

    template <typename T>
    concept const_span_type = std::same_as<T, cspan> || std::same_as<T, uspan> || std::same_as<T, bspan>;

    template <typename T>
    concept const_span_convertible =
            std::convertible_to<T, cspan> || std::convertible_to<T, uspan> || std::convertible_to<T, bspan>;

    template <typename R, typename T>
    concept const_contiguous_range_t = std::ranges::contiguous_range<const R> &&
                                       std::same_as<std::remove_cvref_t<T>, std::ranges::range_value_t<const R>>;

    template <typename T, size_t N, const_contiguous_range_t<T> R>
    bool operator==(const_span<T, N> lhs, const R& rhs)
    {
        return std::ranges::equal(lhs, rhs);
    }

    template <typename T, size_t N, const_contiguous_range_t<T> R>
    auto operator<=>(const_span<T, N> lhs, const R& rhs)
    {
        return std::lexicographical_compare_three_way(
                lhs.begin(), lhs.end(), std::ranges::begin(rhs), std::ranges::end(rhs));
    }

    namespace detail
    {
        template <wshttp::enc::basic_char T, size_t N>
        struct span_literal
        {
            consteval span_literal(const char (&s)[N])
            {
                for (size_t i = 0; i < N; ++i)
                    arr[i] = enc::bit_cast<T>(s[i]);
            }

            std::array<T, N> arr;
            using size = std::integral_constant<size_t, N>;

            consteval const_span<const T, N> span() const { return {arr}; }
        };

        template <size_t N>
        struct sp_literal : span_literal<char, N>
        {
            consteval sp_literal(const char (&s)[N]) : span_literal<char, N>{s} {}
        };

        template <size_t N>
        struct bsp_literal : span_literal<std::byte, N>
        {
            consteval bsp_literal(const char (&s)[N]) : span_literal<std::byte, N>{s} {}
        };

        template <size_t N>
        struct usp_literal : span_literal<unsigned char, N>
        {
            consteval usp_literal(const char (&s)[N]) : span_literal<unsigned char, N>{s} {}
        };

    }  // namespace detail

    inline namespace literals
    {
        template <detail::sp_literal CStr>
        constexpr const_span<char, decltype(CStr)::size::value> operator""_sp()
        {
            return CStr.span();
        }

        template <detail::usp_literal UStr>
        constexpr const_span<unsigned char, decltype(UStr)::size::value> operator""_usp()
        {
            return UStr.span();
        }

        template <detail::bsp_literal BStr>
        constexpr const_span<std::byte, decltype(BStr)::size::value> operator""_bsp()
        {
            return BStr.span();
        }
    }  // namespace literals

    namespace deleters
    {
        struct _event
        {
            inline void operator()(::event* e) const { ::event_free(e); }
        };

        struct _session
        {
            inline void operator()(nghttp2_session* s) const { nghttp2_session_del(s); }
        };

        struct _evdns
        {
            inline void operator()(::evdns_base* e) const { ::evdns_base_free(e, 1); }
        };

        struct _evdns_port
        {
            inline void operator()(::evdns_server_port* e) const { ::evdns_close_server_port(e); }
        };

        struct _bufferevent
        {
            inline void operator()(::bufferevent* b) const { bufferevent_free(b); }
        };

        struct _evconnlistener
        {
            inline void operator()(::evconnlistener* e) const { ::evconnlistener_free(e); }
        };

        struct _evhttp
        {
            inline void operator()(::evhttp* e) { ::evhttp_free(e); }
        };

        struct _ssl_ctx
        {
            inline void operator()(SSL_CTX* s) const { SSL_CTX_free(s); }
        };

        struct _ssl
        {
            inline void operator()(SSL* s) const { SSL_shutdown(s); }
        };

    }  //  namespace deleters

    using evhttp_ptr = std::unique_ptr<::evhttp, deleters::_evhttp>;

    using tcp_listener = std::shared_ptr<evconnlistener>;

    using session_ptr = std::shared_ptr<::nghttp2_session>;

    // using evhttp_bind = std::unique_ptr<evhttp_bound_socket>;

    using ssl_ptr = std::unique_ptr<::SSL, deleters::_ssl>;
    using ssl_ctx_ptr = std::unique_ptr<::SSL_CTX, deleters::_ssl_ctx>;

    using event_ptr = std::unique_ptr<::event, deleters::_event>;

    using bufferevent_ptr = std::unique_ptr<::bufferevent, deleters::_bufferevent>;

    enum class IO { INBOUND, OUTBOUND };

    namespace req
    {
        enum class FIELD { method, scheme, authority, path, status };
        enum class CODE { _200, _404 };
    }  // namespace req

    namespace defaults
    {
        using namespace wshttp::literals;

        inline constexpr uint16_t DNS_PORT{4400};

        inline constexpr auto ALPN = "h2"_usp;
    }  // namespace defaults

}  //  namespace wshttp
