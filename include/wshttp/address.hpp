#pragma once

#include "types.hpp"

#include <variant>

namespace wshttp
{
    static constexpr auto URI_FIELDS{8};
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
        friend class url_parser;

        uri() = default;

      private:
        static enum { _scheme, _userinfo, _host, _port, _pathname, _query, _fragment, _href } UF;

        explicit uri(
            const std::string_view& _s,
            const std::string_view& _u,
            const std::string_view& _h,
            const std::string_view& _p,
            const std::string_view& _pn,
            const std::string_view& _q,
            const std::string_view& _f,
            const std::string_view& _hr);

      public:
        std::array<std::string, URI_FIELDS> _fields{};

        template <concepts::string_view_compatible T>
        static uri parse(T u)
        {
            return uri::parse(std::string{reinterpret_cast<const char*>(u.data()), u.size()});
        }

        static uri parse(std::string u);

        std::string_view scheme() const { return _fields[_scheme]; }
        std::string_view userinfo() const { return _fields[_userinfo]; }
        std::string_view host() const { return _fields[_host]; }
        std::string_view port() const { return _fields[_port]; }
        std::string_view path() const { return _fields[_pathname]; }
        std::string_view query() const { return _fields[_query]; }
        std::string_view fragment() const { return _fields[_fragment]; }
        std::string_view href() const { return _fields[_href]; }

        void print_contents() const;

        bool empty() const { return _fields.empty(); }

        explicit operator bool() const { return !empty(); }

        auto operator<=>(const uri& u) { return _fields <=> u._fields; }
        bool operator==(const uri& u) { return (*this <=> u) == 0; }
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

        explicit operator in_addr() const;

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
        constexpr ip_address() = default;
        explicit ip_address(struct sockaddr* in);

        template <concepts::ip_type T>
        constexpr explicit ip_address(T v4, uint16_t p) : _ip{v4}, _port{p}
        {}

        ip_address(const ip_address& a) { _copy_internals(a); }
        ip_address(ip_address& a) { _copy_internals(a); }

        ip_address& operator=(ip_address& a)
        {
            _copy_internals(a);
            return *this;
        }
        ip_address& operator=(const ip_address& a)
        {
            _copy_internals(a);
            return *this;
        }

      private:
        ip_v _ip;        // host order
        uint16_t _port;  // host order

        // internal getters w/ no safety checking
        ipv4& _ipv4() { return std::get<ipv4>(_ip); }
        const ipv4& _ipv4() const { return std::get<ipv4>(_ip); }
        ipv6& _ipv6() { return std::get<ipv6>(_ip); }
        const ipv6& _ipv6() const { return std::get<ipv6>(_ip); }

        bool _is_v4{!_ip.index()};

        void _copy_internals(const ip_address& a)
        {
            using ip_t = decltype(a._ip);
            _ip = ip_t{a._ip};
            _port = a._port;
            _is_v4 = a._is_v4;
        }

        friend struct std::hash<ip_address>;

      public:
        void set_port(uint16_t p) { _port = p; }

        uint16_t port() { return _port; }

        bool is_ipv4() const { return _is_v4; }

        std::string to_string() const;

        auto operator<=>(const ip_address& a) const { return std::tie(_ip, _port) <=> std::tie(a._ip, a._port); }
        bool operator==(const ip_address& a) const { return (*this <=> a) == 0; }
    };

    struct path
    {
        path() = default;

        path(const ip_address& local, const ip_address& remote) : _local{local}, _remote{remote} {}
        path(const path& p) : path{p._local, p._remote} {}

        path& operator=(const path& p)
        {
            _local = p._local;
            _remote = p._remote;
            return *this;
        }

        ip_address _local;
        ip_address _remote;

        ip_address local() { return _local; }
        const ip_address& local() const { return _local; }
        ip_address remote() { return _remote; }
        const ip_address& remote() const { return _remote; }

        std::string to_string() const;
    };

    // TODO: unify address type sinto this main container
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
    template <>
    struct hash<wshttp::ipv4>
    {
        size_t operator()(const wshttp::ipv4& v4) const noexcept { return hash<uint32_t>{}(v4.addr); }
    };

    template <>
    struct hash<wshttp::ipv6>
    {
        size_t operator()(const wshttp::ipv6& v6) const noexcept
        {
            size_t h{};
            for (const auto& v : v6.addr)
                h ^= hash<uint16_t>{}(v) + wshttp::inverse_golden_ratio + (h << 7) + (h >> 3);
            return h;
        }
    };

    template <>
    struct hash<wshttp::ip_address>
    {
        size_t operator()(const wshttp::ip_address& ip) const noexcept
        {
            using ip_t = decltype(ip._ip);
            return hash<ip_t>{}(ip._ip);
        }
    };

    template <>
    struct hash<wshttp::uri>
    {
        size_t operator()(const wshttp::uri& u) const noexcept
        {
            return hash<string_view>{}(u.href());  // TODO: hash to href
        }
    };
}  //  namespace std
