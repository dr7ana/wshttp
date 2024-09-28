#pragma once

#include "concepts.hpp"

#include <cstdint>
#include <span>

namespace wshttp
{
    template <typename T = const char, size_t N = std::dynamic_extent>
        requires std::is_const_v<T> && std::equality_comparable<T>
    class cspan : public std::span<T, N>
    {
      public:
        using std::span<T, N>::span;

        std::string to_string() const { return {reinterpret_cast<const char*>(this->data(), this->size())}; }
        static constexpr bool to_string_formattable = true;
    };

    using uspan = cspan<const unsigned char>;
    using bspan = cspan<const std::byte>;

    namespace concepts
    {
        template <typename T>
        concept cspan_compatible =
            std::convertible_to<T, cspan<const char>> || std::convertible_to<T, uspan> || std::convertible_to<T, bspan>;

    }  //  namespace concepts

    template <typename T, size_t N, concepts::const_contiguous_range_t<T> R>
    bool operator==(cspan<T, N> lhs, const R& rhs)
    {
        return std::ranges::equal(lhs, rhs);
    }

    template <typename T, size_t N, concepts::const_contiguous_range_t<T> R>
    auto operator<=>(cspan<T, N> lhs, const R& rhs)
    {
        return std::lexicographical_compare_three_way(
            lhs.begin(), lhs.end(), std::ranges::begin(rhs), std::ranges::end(rhs));
    }

    namespace detail
    {
        template <wshttp::concepts::basic_char T, size_t N>
        struct literal
        {
            consteval literal(const char (&s)[N])
            {
                for (size_t i = 0; i < N; ++i)
                    arr[i] = static_cast<T>(s[i]);
            }

            std::array<T, N> arr;
            using size = std::integral_constant<size_t, N>;

            consteval cspan<const T, N> span() const { return {arr}; }
        };

        template <size_t N>
        struct sp_literal : literal<char, N>
        {
            consteval sp_literal(const char (&s)[N]) : literal<char, N>{s} {}
        };

        template <size_t N>
        struct bsp_literal : literal<std::byte, N>
        {
            consteval bsp_literal(const char (&s)[N]) : literal<std::byte, N>{s} {}
        };

        template <size_t N>
        struct usp_literal : literal<unsigned char, N>
        {
            consteval usp_literal(const char (&s)[N]) : literal<unsigned char, N>{s} {}
        };

    }  // namespace detail

    namespace literals
    {
        template <detail::sp_literal CStr>
        constexpr cspan<const char, decltype(CStr)::size::value> operator""_sp()
        {
            return CStr.span();
        }

        template <detail::usp_literal UStr>
        constexpr cspan<const unsigned char, decltype(UStr)::size::value> operator""_usp()
        {
            return UStr.span();
        }

        template <detail::bsp_literal BStr>
        constexpr cspan<const std::byte, decltype(BStr)::size::value> operator""_bsp()
        {
            return BStr.span();
        }
    }  // namespace literals

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

    enum class IO { INBOUND, OUTBOUND };

    namespace req
    {
        enum class FIELD { method, scheme, authority, path, status };
        enum class STATUS { _200, _404 };
    }  // namespace req

    namespace defaults
    {
        using namespace wshttp::literals;

        inline constexpr uint16_t DNS_PORT{4400};

        inline constexpr auto ALPN = "h2"_usp;
    }  // namespace defaults

}  //  namespace wshttp
