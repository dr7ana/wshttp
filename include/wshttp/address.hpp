#pragma once

#include "encoding.hpp"

#include <variant>

namespace wshttp
{
    static constexpr auto URI_FIELDS{7};
    static constexpr uint16_t HTTPS_PORT{443};
    static constexpr auto HTTPS_SCHEME = "https:"sv;

    struct ipv4;
    struct ipv6;

    namespace concepts
    {
        template <typename ip_t>
        concept ip_type = std::same_as<ip_t, ipv4> || std::same_as<ip_t, ipv6>;
    }  // namespace concepts

    using ip_v = std::variant<ipv4, ipv6>;

    struct uri
    {
      private:
        static enum { _scheme, _userinfo, _host, _port, _pathname, _query, _fragment } UF;

      public:
        std::array<const char*, URI_FIELDS> _fields{};

        static uri parse(std::string uri);

        constexpr std::string_view scheme() { return _fields[_scheme]; }
        constexpr std::string_view userinfo() { return _fields[_userinfo]; }
        constexpr std::string_view host() { return _fields[_host]; }
        constexpr std::string_view port() { return _fields[_port]; }
        constexpr std::string_view path() { return _fields[_pathname]; }
        constexpr std::string_view query() { return _fields[_query]; }
        constexpr std::string_view fragment() { return _fields[_fragment]; }
    };

    struct ipv4
    {
        // host order
        uint32_t addr;

        constexpr ipv4() = default;

        explicit ipv4(struct in_addr* a);

        explicit ipv4(const std::string& str);

        constexpr ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
            : addr{uint32_t{a} << 24 | uint32_t{b} << 16 | uint32_t{c} << 8 | uint32_t{d}}
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
        explicit ipv6(const struct in6_addr* a);

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

    struct ip_address
    {
        explicit ip_address(struct sockaddr* in)
        {
            if (in->sa_family == AF_INET)
            {
                auto& in4 = *reinterpret_cast<sockaddr_in*>(in);
                _ip = ipv4{&in4.sin_addr};
                _port = in4.sin_port;
            }
            else if (in->sa_family == AF_INET6)
            {
                auto& in6 = *reinterpret_cast<sockaddr_in6*>(in);
                _ip = ipv6{&in6.sin6_addr};
                _port = in6.sin6_port;
            }
            else
                throw std::runtime_error{"Failed to understand incoming address sa_family"};

            enc::big_to_host_inplace(_port);
        }

        template <concepts::ip_type T>
        explicit ip_address(T v4, uint16_t p) : _ip{v4}, _port{p}
        {}

      private:
        ip_v _ip;        // host order
        uint16_t _port;  // host order

        // internal getters w/ no safety checking
        ipv4& _ipv4() { return std::get<ipv4>(_ip); }
        const ipv4& _ipv4() const { return std::get<ipv4>(_ip); }
        ipv6& _ipv6() { return std::get<ipv6>(_ip); }
        const ipv6& _ipv6() const { return std::get<ipv6>(_ip); }

        const bool _is_v4{!_ip.index()};

      public:
        void set_port(uint16_t p) { _port = p; }

        uint16_t port() { return _port; }

        bool is_ipv4() const { return _is_v4; }

        std::string to_string() const { return _is_v4 ? _ipv4().to_string() : _ipv6().to_string(); }
    };

    struct address
    {
      private:
        // sockaddr_storage _sockaddr{};
        // uint16_t _port{};

      public:
        //
    };
}  //  namespace wshttp

namespace std
{
    // template <>
    // struct hash<wshttp::uri>
    // {
    //     size_t operator()(const wshttp::uri& u) const noexcept
    //     {
    //         size_t h{};

    //         return h;
    //     }
    // };
}  //  namespace std
