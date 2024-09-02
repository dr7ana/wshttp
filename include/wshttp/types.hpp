#pragma once

#include "utils.hpp"

#include <cstdint>

namespace wshttp
{
    namespace defaults
    {
        static constexpr uint16_t DNS_PORT{4400};
    }  // namespace defaults

    namespace concepts
    {
        template <typename T>
        concept endian_swappable_type =
            std::integral<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

        template <typename T>
        concept nghttp2_session_type = std::same_as<T, nghttp2_session>;

        template <typename T>
        concept nghttp2_nv_type = std::same_as<T, nghttp2_nv>;
    }  // namespace concepts

    enum class IO { INBOUND, OUTBOUND };

    namespace req
    {
        enum class FIELD { method, scheme, authority, path, status };
    }

    namespace detail
    {}
}  //  namespace wshttp
