#pragma once

#include "utils.hpp"

#include <bit>

#if defined(__clang__) || defined(__GNUC__)
#define bswap_16(x) __builtin_bswap16(x)
#define bswap_32(x) __builtin_bswap32(x)
#define bswap_64(x) __builtin_bswap64(x)
#elif defined(__linux__)
extern "C"
{
#include <byteswap.h>
}
#endif

namespace wshttp
{
    namespace concepts
    {
        template <typename T>
        concept endian_swappable =
            std::integral<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);
    }  // namespace concepts

    namespace enc
    {
        inline constexpr bool big_endian = std::endian::native == std::endian::big;

        template <concepts::endian_swappable T>
        void byteswap_inplace(T& val)
        {
            if constexpr (sizeof(T) == 2)
                val = static_cast<T>(bswap_16(static_cast<uint16_t>(val)));
            else if constexpr (sizeof(T) == 4)
                val = static_cast<T>(bswap_32(static_cast<uint32_t>(val)));
            else if constexpr (sizeof(T) == 8)
                val = static_cast<T>(bswap_64(static_cast<uint64_t>(val)));
        }

        // Host-order integer -> big-endian integer
        template <concepts::endian_swappable T>
        void host_to_big_inplace(T& val)
        {
            if constexpr (!big_endian)
                byteswap_inplace(val);
        }

        // Big-endian integer -> host-order integer
        template <concepts::endian_swappable T>
        void big_to_host_inplace(T& val)
        {
            if constexpr (!big_endian)
                byteswap_inplace(val);
        }

        // Host-order integer -> big-endian integer
        template <concepts::endian_swappable T>
        T host_to_big(T val)
        {
            host_to_big_inplace(val);
            return val;
        }

        // Big-endian integer -> host-order integer
        template <concepts::endian_swappable T>
        T big_to_host(T val)
        {
            big_to_host_inplace(val);
            return val;
        }
    }  // namespace enc

}  //  namespace wshttp
