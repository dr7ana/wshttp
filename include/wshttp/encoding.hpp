#pragma once

#include <bit>
#include <concepts>
#include <cstdint>

#if defined(__GNUC__) || defined(__clang__)
#define BSWAP16 __builtin_bswap16
#define BSWAP32 __builtin_bswap32
#define BSWAP64 __builtin_bswap64
#define BUILTIN_BYTESWAP_IS_CONSTEXPR

#elif defined(__linux__)
extern "C" {
#include <byteswap.h>
}
#define BSWAP16 bswap_16
#define BSWAP32 bswap_32
#define BSWAP64 bswap_64
#elif defined(_MSC_VER)
#include <cstdlib>
#define BSWAP16 _byteswap_ushort
#define BSWAP32 _byteswap_ulong
#define BSWAP64 _byteswap_uint64
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

    namespace detail
    {
        template <std::unsigned_integral T>
        [[nodiscard]] inline constexpr T byteswap_fallback(T x) noexcept
        {
            if constexpr (sizeof(T) == 2)
                return (x >> 8) | ((x & 0xff) << 8);
            else if constexpr (sizeof(T) == 4)
                return ((x & 0xff000000u) >> 24)  // (comments for formatting)
                     | ((x & 0x00ff0000u) >> 8)   //
                     | ((x & 0x0000ff00u) << 8)   //
                     | ((x & 0x000000ffu) << 24);
            else if constexpr (sizeof(T) == 8)
                return ((x & 0xff00000000000000ull) >> 56)  // (comments for formatting)
                     | ((x & 0x00ff000000000000ull) >> 40)  //
                     | ((x & 0x0000ff0000000000ull) >> 24)  //
                     | ((x & 0x000000ff00000000ull) >> 8)   //
                     | ((x & 0x00000000ff000000ull) << 8)   //
                     | ((x & 0x0000000000ff0000ull) << 24)  //
                     | ((x & 0x000000000000ff00ull) << 40)  //
                     | ((x & 0x00000000000000ffull) << 56);
            else
            {
                static_assert(sizeof(T) == 1);
                return x;
            }
        }
    }  // namespace detail

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
#ifndef BSWAP64
        val = bit_cast<T>(detail::byteswap_fallback(bit_cast<std::make_unsigned_t<T>>(val)));
#else
#ifndef BUILTIN_BYTESWAP_IS_CONSTEXPR
        if (std::is_constant_evaluated())
            val = bit_cast<T>(detail::byteswap_fallback(bit_cast<std::make_unsigned_t<T>>(val)));
#endif
        if constexpr (sizeof(T) == 2)
            val = bit_cast<T>(BSWAP16(bit_cast<uint16_t>(val)));
        else if constexpr (sizeof(T) == 4)
            val = bit_cast<T>(BSWAP32(bit_cast<uint32_t>(val)));
        else if constexpr (sizeof(T) == 8)
            val = bit_cast<T>(BSWAP64(bit_cast<uint64_t>(val)));
#endif
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
