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
        concept SessionWrapper = std::is_same_v<T, nghttp2_session>;
    }  // namespace concepts

    namespace detail
    {}
}  //  namespace wshttp
