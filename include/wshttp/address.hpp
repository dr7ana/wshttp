#pragma once

#include "encoding.hpp"
#include "format.hpp"
#include "parser.hpp"
#include "types.hpp"

#include <variant>

/** TODO: **post http_request unification**
        URI as a concept isn't needed at the http_request level. Requests are many:1 within sessions, which themselves
    are 1:1 with host:port (dmain_host) w.r.t. the TLS session. Requests are then 1:1 with "{path}+{query}", and can
    be decomposed as such.

        - Take URI out of shared_ptr
        - When passing to an http_request object, decompose into:
            - domain_host
                - Encapsulates the concept of TLS host (host:port)
                - Add ip_address to object if IP address is given to invocation
                - For subsequent requests on established sessions, the domain_host
                    half can be discarded
            - path_query
                - Holds the metadata unique to each http request
                - On subsequent requests, only this portion is needed
*/

namespace wshttp
{
    struct domain_host;

    enum class SCHEME : uint8_t { UNSUPPORTED = 0, HTTPS = 1, HTTP = 2, WSS = 3, WS = 4 };

    template <typename T>
    concept supported_scheme = std::is_same_v<T, SCHEME> && requires(T a) { std::to_underlying(a) > 0; };

    namespace deleters
    {
        struct _evuri
        {
            inline void operator()(::evhttp_uri* u) const { evhttp_uri_free(u); }
        };
    }  // namespace deleters

    struct uri;

    using uri_ptr = std::shared_ptr<uri>;

    using evuri_ptr = std::shared_ptr<::evhttp_uri>;

    struct uri
    {
        uri() = delete;

        static uri_ptr make(std::string_view input, const url_result_ptr& base = nullptr);

        uri(std::string_view input, const url_result_ptr& base = nullptr);

        // TODO: ctors/ops
        // uri(uri&& u);

        ~uri();

      private:
        url_result_ptr _url;

        evuri_ptr _evuri;

        // TODO: make _host into a domain_host object
        // need null-terminated c-strings for SSL and libevent
        std::string _host;
        std::string _pathquery;

        SCHEME _scheme;
        int _port;
        bool _use_tls;

        void _populate_internals();

        // internal getter
        ada::url_aggregator& url() { return _url->value(); }
        const ada::url_aggregator& url() const { return _url->value(); }

      public:
        // public getter to construct a new uri using a parsed base url
        const url_result_ptr& base() { return _url; }

        domain_host host_domain() const;

        void set_path(std::string_view path);

        const std::string& host() const { return _host; }
        const std::string& pathquery() const { return _pathquery; }

        std::string_view scheme() const;

        int port() const { return _port; }

        bool use_tls() const { return _use_tls; }

        std::string_view hview() const { return _host; }

        std::string_view view() const;

        auto operator<=>(const uri& u) const { return view() <=> u.view(); }
        bool operator==(const uri& u) const { return (*this <=> u) == 0; }

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

    struct domain_host final
    {
        domain_host() = delete;

      protected:
        domain_host(std::string_view u, int p) : _host{u}, _port{p} {}

        const std::string _host{};
        const int _port{};

      public:
        std::string_view host() const { return _host; }
        const char* host_cstr() const { return _host.c_str(); }
        int port() const { return _port; }

        auto operator<=>(const domain_host& d) const { return std::tie(_host, _port) <=> std::tie(d._host, d._port); }
        bool operator==(const domain_host& d) const { return (*this <=> d) == 0; }

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;

        friend struct uri;
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

        constexpr bool is_anyaddr() const;

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

    inline constexpr ipv4 ipv4_anyaddr{0, 0, 0, 0};

    inline constexpr ipv6 ipv6_anyaddr{0, 0, 0, 0, 0, 0, 0, 0};

    constexpr bool ipv4::is_anyaddr() const
    {
        return *this == ipv4_anyaddr;
    }

    constexpr bool ipv6::is_anyaddr() const
    {
        return *this == ipv6_anyaddr;
    }

    template <typename ip_t>
    concept ip_type = std::same_as<ip_t, ipv4> || std::same_as<ip_t, ipv6>;

    using ip_v = std::variant<ipv4, ipv6>;

    struct ip_address
    {
        constexpr ip_address(uint16_t p = 0) : _ip{ipv4_anyaddr}, _port{p}, _is_v4{true}, _is_anyaddr(true) {}

        explicit ip_address(const struct sockaddr* in);

        explicit constexpr ip_address(ip_v ip, uint16_t port) : _ip{ip}, _port{port}, _is_v4{!_ip.index()}
        {
            if (std::holds_alternative<ipv4>(_ip))
                _is_anyaddr = _ipv4().is_anyaddr();
            else
                _is_anyaddr = _ipv6().is_anyaddr();
        }

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
        constexpr ipv4& _ipv4() { return std::get<ipv4>(_ip); }
        constexpr const ipv4& _ipv4() const { return std::get<ipv4>(_ip); }
        constexpr ipv6& _ipv6() { return std::get<ipv6>(_ip); }
        constexpr const ipv6& _ipv6() const { return std::get<ipv6>(_ip); }

        bool _is_v4{true};
        bool _is_anyaddr{true};

        void _copy_internals(const ip_address& a)
        {
            using ip_t = decltype(a._ip);
            _ip = ip_t{a._ip};
            _port = a._port;
            _is_v4 = a._is_v4;
            _is_anyaddr = a._is_anyaddr;
        }

        friend struct std::hash<ip_address>;

      public:
        bool is_anyaddr() const { return _is_anyaddr; }

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
        size_t operator()(const wshttp::uri& u) const noexcept { return hash<string_view>{}(u.view()); }
    };

    template <>
    struct hash<wshttp::domain_host>
    {
        size_t operator()(const wshttp::domain_host& u) const noexcept
        {
            auto h = hash<string_view>{}(u.host());
            h ^= hash<int>{}(u.port()) + wshttp::inverse_golden_ratio + (h << 7) + (h >> 3);
            return h;
        }
    };
}  //  namespace std
