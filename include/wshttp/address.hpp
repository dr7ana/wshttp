#pragma once

#include "utils.hpp"

namespace wshttp
{
    struct ipv4
    {
        // host order
        uint32_t addr;

        constexpr ipv4() = default;

        explicit ipv4(const std::string& str);

        constexpr ipv4(uint32_t a) : addr{a} {}
        constexpr ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
            : ipv4{uint32_t{a} << 24 | uint32_t{b} << 16 | uint32_t{c} << 8 | uint32_t{d}}
        {}

        std::string to_string() const;

        static constexpr bool to_string_formattable = true;

        explicit operator in_addr() const
        {
            in_addr a;
            a.s_addr = htonl(addr);
            return a;
        }

        constexpr auto operator<=>(const ipv4& a) const { return addr <=> a.addr; }

        constexpr bool operator==(const ipv4& a) const { return (addr <=> a.addr) == 0; }

        constexpr bool operator==(const in_addr& a) const { return (addr <=> a.s_addr) == 0; }
    };

    struct ipv6
    {
        std::array<uint16_t, 8> addr{};

        constexpr ipv6() = default;

        // Network order in6_addr constructor
        ipv6(const struct in6_addr* a);

        explicit ipv6(const std::string& str);

        explicit constexpr ipv6(
            uint16_t a,
            uint16_t b = 0x0000,
            uint16_t c = 0x0000,
            uint16_t d = 0x0000,
            uint16_t e = 0x0000,
            uint16_t f = 0x0000,
            uint16_t g = 0x0000,
            uint16_t h = 0x0000)
            : addr{a, b, c, d, e, f, g, h}
        {}

        in6_addr to_in6() const;

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;

        constexpr auto operator<=>(const ipv6& a) const { return addr <=> a.addr; }

        constexpr bool operator==(const ipv6& a) const { return (addr <=> a.addr) == 0; }
    };
}  //  namespace wshttp
