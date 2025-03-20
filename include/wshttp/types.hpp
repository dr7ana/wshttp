#pragma once

#include "encoding.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstdint>
#include <span>

namespace wshttp
{
    inline namespace types
    {
        inline namespace span
        {
            template <enc::basic_char T = char, size_t N = std::dynamic_extent>
            using const_span = std::span<const T, N>;
        }
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

    inline namespace operators
    {
        inline namespace span
        {
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
        }  // namespace span
    }      // namespace operators

    namespace detail
    {
        template <enc::basic_char T, size_t N>
        struct span_literal
        {
            consteval span_literal(const char (&s)[N])
            {
                for (size_t i = 0; i < N; ++i)
                    arr[i] = enc::bit_cast<T>(s[i]);
            }

            static constexpr size_t SIZE{N - 1};
            T arr[N];

            constexpr auto span() const { return const_span<T, SIZE>{arr, SIZE}; }
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
        constexpr auto operator""_sp()
        {
            return CStr.span();
        }

        template <detail::usp_literal UStr>
        constexpr auto operator""_usp()
        {
            return UStr.span();
        }

        template <detail::bsp_literal BStr>
        constexpr auto operator""_bsp()
        {
            return BStr.span();
        }
    }  // namespace literals

    namespace defaults
    {
        using namespace wshttp::literals;

        inline constexpr uint16_t DNS_PORT{4400};

        inline constexpr auto ALPN = "h2"_usp;
    }  // namespace defaults

}  //  namespace wshttp
