#pragma once

#include "utils.hpp"

#include <bit>

#if defined(_MSC_VER) && (!defined(__clang__) || defined(__c2__))
#include <cstdlib>

#elif !(defined(__clang__) || defined(__GNUC__))
#if defined(__linux__)
extern "C"
{
#include <byteswap.h>
}  // extern "C"

#else
#include <algorithm>

#endif
#endif
namespace wshttp::enc
{
#ifdef __cpp_lib_bit_cast
    using std::bit_cast;
#else
    template <class To, class From>
        requires(
                sizeof(To) == sizeof(From) && std::is_trivially_copyable<To>::value &&
                std::is_trivially_copyable<From>::value && std::is_trivially_constructible<To>::value &&
                std::is_trivially_constructible<From>::value)
    inline constexpr To bit_cast(const From& from)
    {
        To storage{};
        std::construct_at(std::launder(&storage), from);
        return storage;
    }
#endif

#if defined(_MSC_VER) && (!defined(__clang__) || defined(__c2__))
#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)

#elif defined(__clang__) || defined(__GNUC__)
#define bswap_16(x) __builtin_bswap16(x)
#define bswap_32(x) __builtin_bswap32(x)
#define bswap_64(x) __builtin_bswap64(x)

#elif !defined(__linux__)
    template <std::integral T>
        requires std::has_unique_object_representations_v<T>
    inline constexpr T byteswap(T val) noexcept
    {
        auto value = bit_cast<std::array<std::byte, sizeof(T)>>(val);
        std::ranges::reverse(value);
        return bit_cast<T>(value);
    }

#define bswap_16(x) byteswap<uint16_t>(x)
#define bswap_32(x) byteswap<uint32_t>(x)
#define bswap_64(x) byteswap<uint64_t>(x)

#endif

    template <typename Char>
    concept basic_char =
            sizeof(Char) == 1 && !std::same_as<Char, bool> && (std::integral<Char> || std::same_as<Char, std::byte>);

    template <typename T>
    concept endian_swappable_type =
            std::integral<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

    inline constexpr bool big_endian = std::endian::native == std::endian::big;
    inline constexpr bool little_endian = !big_endian;

    template <endian_swappable_type T>
    constexpr void byteswap_inplace(T& val)
    {
        if constexpr (sizeof(T) == 2)
            val = bit_cast<T>(bswap_16(bit_cast<uint16_t>(val)));
        else if constexpr (sizeof(T) == 4)
            val = bit_cast<T>(bswap_32(bit_cast<uint32_t>(val)));
        else if constexpr (sizeof(T) == 8)
            val = bit_cast<T>(bswap_64(bit_cast<uint64_t>(val)));
    }

    // Host-order integer -> big-endian integer
    template <endian_swappable_type T>
    constexpr void host_to_big_inplace(T& val)
    {
        if constexpr (!big_endian)
            byteswap_inplace(val);
    }

    // Host-order integer -> little-endian integer
    template <endian_swappable_type T>
    constexpr void host_to_little_inplace(T& val)
    {
        if constexpr (!little_endian)
            byteswap_inplace(val);
    }

    // Big-endian integer -> host-order integer
    template <endian_swappable_type T>
    constexpr void big_to_host_inplace(T& val)
    {
        if constexpr (!big_endian)
            byteswap_inplace(val);
    }

    // Little-endian integer -> host-order integer
    template <endian_swappable_type T>
    constexpr void little_to_host_inplace(T& val)
    {
        if constexpr (!little_endian)
            byteswap_inplace(val);
    }

    // Host-order integer -> big-endian integer
    template <endian_swappable_type T>
    constexpr T host_to_big(T val)
    {
        host_to_big_inplace(val);
        return val;
    }

    // Host-order integer -> little-endian integer
    template <endian_swappable_type T>
    constexpr T host_to_little(T val)
    {
        host_to_little_inplace(val);
        return val;
    }

    // Big-endian integer -> host-order integer
    template <endian_swappable_type T>
    constexpr T big_to_host(T val)
    {
        big_to_host_inplace(val);
        return val;
    }

    // Little-endian integer -> host-order integer
    template <endian_swappable_type T>
    constexpr T little_to_host(T val)
    {
        little_to_host_inplace(val);
        return val;
    }

}  // namespace wshttp::enc
