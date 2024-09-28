#pragma once

#include "utils.hpp"

#include <concepts>

namespace wshttp
{
    struct ipv4;
    struct ipv6;

    class session_base;

    namespace concepts
    {
        template <typename Char>
        concept basic_char =
            sizeof(Char) == 1 && !std::same_as<Char, bool> && (std::integral<Char> || std::same_as<Char, std::byte>);

        template <typename R, typename T>
        concept const_contiguous_range_t = std::ranges::contiguous_range<const R>
            && std::same_as<std::remove_cvref_t<T>, std::ranges::range_value_t<const R>>;

        template <typename T>
        concept string_view_compatible =
            std::convertible_to<T, std::string_view> || std::convertible_to<T, std::basic_string_view<unsigned char>>
            || std::convertible_to<T, std::basic_string_view<std::byte>>;

        // Types can opt-in to being fmt-formattable by ensuring they have a ::to_string() method defined
        template <typename T>
        concept to_string_formattable = T::to_string_formattable && requires(T a) {
            {
                a.to_string()
            } -> std::convertible_to<std::string_view>;
        };

        template <typename ip_t>
        concept ip_type = std::same_as<ip_t, ipv4> || std::same_as<ip_t, ipv6>;

        template <typename T>
        concept session_type = std::derived_from<T, session_base>;

        template <typename T>
        concept endian_swappable_type =
            std::integral<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

        template <typename T>
        concept nghttp2_session_type = std::same_as<T, nghttp2_session>;

        template <typename T>
        concept nghttp2_nv_type = std::same_as<T, nghttp2_nv>;

        template <typename T>
        concept nghttp2_settings_type = std::same_as<T, nghttp2_settings_entry>;

        template <typename T>
        concept dns_base = std::same_as<T, ::evdns_base>;

    }  //  namespace concepts

}  //  namespace wshttp
