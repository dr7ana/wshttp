#pragma once

#include "encoding.hpp"
#include "format.hpp"
#include "types.hpp"

#include <variant>

namespace wshttp
{
    static constexpr size_t URI_FIELDS{8};
    static constexpr uint16_t HTTPS_PORT{443};
    static constexpr auto HTTPS_SCHEME = "https:"sv;
    static constexpr auto HTTP_SCHEME = "http:"sv;

    struct domain_host;

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

        static uri parse(const char* c, size_t s);

      public:
        std::array<std::string, URI_FIELDS> _fields{};

        static uri populate(struct evhttp_request* r);

        template <const_span_convertible T>
        static uri parse(T u)
        {
            return uri::parse(std::string{reinterpret_cast<const char*>(u.data()), u.size()});
        }

        static uri parse(std::string_view u) { return uri::parse(u.data(), u.size()); }

        std::string_view scheme() const { return _fields[_scheme]; }
        std::string_view userinfo() const { return _fields[_userinfo]; }
        std::string_view host() const { return _fields[_host]; }
        std::string_view port() const { return _fields[_port]; }
        std::string_view path() const { return _fields[_pathname]; }
        std::string_view query() const { return _fields[_query]; }
        std::string_view fragment() const { return _fields[_fragment]; }
        std::string_view href() const { return _fields[_href]; }

        domain_host host_url() const;

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;

        bool empty() const { return _fields.empty(); }

        explicit operator bool() const { return !empty(); }

        auto operator<=>(const uri& u) const { return _fields <=> u._fields; }
        bool operator==(const uri& u) const { return (*this <=> u) == 0; }
    };

    struct domain_host final
    {
        friend struct uri;

        domain_host() = delete;

      protected:
        domain_host(std::string_view u) : _host{u} {}

        std::string _host{};

      public:
        std::string_view host() const { return _host; }

        auto operator<=>(const domain_host& d) const { return _host <=> d._host; }
        bool operator==(const domain_host& d) const { return (*this <=> d) == 0; }

        std::string to_string() const { return _host; }
        static constexpr bool to_string_formattable = true;
    };

    struct ipv4
    {
        // host order
        uint32_t addr;

        constexpr ipv4() = default;

        explicit constexpr ipv4(const struct sockaddr_in* in) : addr{std::move(in->sin_addr.s_addr)}
        {
            enc::big_to_host_inplace(addr);
        }

        explicit ipv4(const std::string& str);

        constexpr ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) :
                addr{uint32_t{a} << 24 | uint32_t{b} << 16 | uint32_t{c} << 8 | uint32_t{d}}
        {}

        in_addr to_inaddr() const;

        bool is_anyaddr() const;

        constexpr auto operator<=>(const ipv4& a) const { return addr <=> a.addr; }

        constexpr bool operator==(const ipv4& a) const { return (addr <=> a.addr) == 0; }

        constexpr bool operator==(const in_addr& a) const { return (addr <=> a.s_addr) == 0; }

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

    struct ipv6
    {
        std::array<uint16_t, 8> addr{};

        constexpr ipv6() = default;

        explicit constexpr ipv6(const struct sockaddr_in6* in6)
        {
            std::ranges::move(in6->sin6_addr.s6_addr16, addr.begin());
            for (int i = 0; i < 8; ++i)
                enc::big_to_host_inplace(addr[i]);
        }

        explicit ipv6(const std::string& str);

        explicit constexpr ipv6(
                uint16_t a,
                uint16_t b = 0x0000,
                uint16_t c = 0x0000,
                uint16_t d = 0x0000,
                uint16_t e = 0x0000,
                uint16_t f = 0x0000,
                uint16_t g = 0x0000,
                uint16_t h = 0x0000) :
                addr{a, b, c, d, e, f, g, h}
        {}

        in6_addr to_in6addr() const;

        constexpr bool is_anyaddr() const;

        constexpr auto operator<=>(const ipv6& a) const { return addr <=> a.addr; }

        constexpr bool operator==(const ipv6& a) const { return (addr <=> a.addr) == 0; }

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

    inline constexpr ipv4 ipv4_anyaddr(0, 0, 0, 0);

    inline constexpr ipv6 ipv6_anyaddr(0, 0, 0, 0, 0, 0, 0, 0);

    constexpr bool ipv6::is_anyaddr() const
    {
        return *this == ipv6_anyaddr;
    }

    template <typename ip_t>
    concept ip_type = std::same_as<ip_t, ipv4> || std::same_as<ip_t, ipv6>;

    using ip_v = std::variant<ipv4, ipv6>;

    struct ip_address
    {
        constexpr ip_address(uint16_t p = 0) : _ip{ipv4_anyaddr}, _port{p}, _is_v4{true} {}

        explicit ip_address(const struct sockaddr* in)
        {
            if (in->sa_family == AF_INET)
            {
                auto* in4 = reinterpret_cast<const sockaddr_in*>(in);
                _ip = ipv4{in4};
                _port = enc::big_to_host(in4->sin_port);
                _is_v4 = true;
            }
            else if (in->sa_family == AF_INET6)
            {
                auto* in6 = reinterpret_cast<const sockaddr_in6*>(in);
                _ip = ipv6{in6};
                _port = enc::big_to_host(in6->sin6_port);
                _is_v4 = false;
            }
            else
                throw std::runtime_error{"Failed to understand incoming address sa_family: {}"_format(in->sa_family)};
        }

        template <ip_type T>
        constexpr explicit ip_address(T ip, uint16_t p) : _ip{ip}, _port{p}, _is_v4{!_ip.index()}
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

        static ip_address from_socket(int fd);

      private:
        ip_v _ip;        // host order
        uint16_t _port;  // host order

        // internal getters w/ no safety checking
        ipv4& _ipv4() { return std::get<ipv4>(_ip); }
        const ipv4& _ipv4() const { return std::get<ipv4>(_ip); }
        ipv6& _ipv6() { return std::get<ipv6>(_ip); }
        const ipv6& _ipv6() const { return std::get<ipv6>(_ip); }

        bool _is_v4{true};

        void _copy_internals(const ip_address& a)
        {
            using ip_t = decltype(a._ip);
            _ip = ip_t{a._ip};
            _port = a._port;
            _is_v4 = a._is_v4;
        }

        friend struct std::hash<ip_address>;

      public:
        bool is_anyaddr() const;

        void set_port(uint16_t p) { _port = p; }

        uint16_t port() { return _port; }

        bool is_ipv4() const { return _is_v4; }

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;

        auto operator<=>(const ip_address& a) const { return std::tie(_ip, _port) <=> std::tie(a._ip, a._port); }
        bool operator==(const ip_address& a) const { return (*this <=> a) == 0; }

        operator in_addr() const { return _ipv4().to_inaddr(); }

        operator in6_addr() const { return _ipv6().to_in6addr(); }
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
        static constexpr bool to_string_formattable = true;
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
                h ^= hash<uint16_t>{}(v) + wshttp::inverse_golden_ratio + (h << 6) + (h >> 2);
            return h;
        }
    };

    template <>
    struct hash<wshttp::ip_address>
    {
        size_t operator()(const wshttp::ip_address& ip) const noexcept
        {
            size_t h{};

            if (auto maybe_v4 = std::get_if<wshttp::ipv4>(&ip._ip))
                h = hash<wshttp::ipv4>{}(*maybe_v4);
            else
                h = hash<wshttp::ipv6>{}(std::get<wshttp::ipv6>(ip._ip));

            h ^= hash<decltype(ip._port)>{}(ip._port) + wshttp::inverse_golden_ratio + (h << 7) + (h >> 3);
            return h;
        }
    };

    template <>
    struct hash<wshttp::uri>
    {
        size_t operator()(const wshttp::uri& u) const noexcept { return hash<string_view>{}(u.href()); }
    };

    template <>
    struct hash<wshttp::domain_host>
    {
        size_t operator()(const wshttp::domain_host& u) const noexcept { return hash<string_view>{}(u.host()); }
    };
}  //  namespace std
