#include "address.hpp"

#include "internal.hpp"

namespace wshttp
{
    namespace detail
    {
        static void parse_addr(int af, void* dest, const std::string& from)
        {
            auto rv = inet_pton(af, from.c_str(), dest);

            if (rv == 0)  // inet_pton returns this on invalid input
                throw std::invalid_argument{"Unable to parse IP address!"};
            if (rv < 0)
                throw std::system_error{errno, std::system_category()};
        }
    }  // namespace detail

    uri::uri(
            const std::string_view& s,
            const std::string_view& u,
            const std::string_view& h,
            const std::string_view& p,
            const std::string_view& pn,
            const std::string_view& q,
            const std::string_view& f,
            const std::string_view& hr) :
            _fields{std::string{s},
                    std::string{u},
                    std::string{h},
                    std::string{p},
                    std::string{pn},
                    std::string{q},
                    std::string{f},
                    std::string{hr}}
    {
        if (auto s = scheme(); !s.empty() && (s != HTTP_S and s != HTTPS_S))
            throw std::invalid_argument{"uri must use protocol scheme HTTP or HTTPS (given: {})"_format(scheme())};

        if (_fields[_port].empty())
            _p = use_tls() ? 443 : 80;
        else
            _p = std::stoi(_fields[_port]);
    }

    uri uri::populate(struct evhttp_request* r)
    {
        (void)r;

        // return parser->read(std::string{evhttp_request_get_uri(r)}) ? parser->extract() : uri{};
        return {};
    }

    uri uri::parse(const char* c, size_t s)
    {
        return parser->read(std::string{c, s}) ? parser->extract() : uri{};
    }

    domain_host uri::host_url() const
    {
        return domain_host{host()};
    }

    const char* uri::host_cstr() const
    {
        return _fields[_host].c_str();
    }

    const char* uri::path_cstr() const
    {
        return _fields[_pathname].c_str();
    }

    std::string uri::to_string() const
    {
        return _fields[_href];
    }

    ipv4::ipv4(const std::string& str)
    {
        detail::parse_addr(AF_INET, &addr, str);
        enc::big_to_host_inplace(addr);
    }

    in_addr ipv4::to_inaddr() const
    {
        in_addr a;
        a.s_addr = enc::host_to_big(addr);
        return a;
    }

    bool ipv4::is_anyaddr() const
    {
        return *this == ipv4_anyaddr;
    }

    std::string ipv4::to_string() const
    {
        char buf[INET_ADDRSTRLEN] = {};
        uint32_t net = enc::host_to_big(addr);
        inet_ntop(AF_INET, &net, buf, sizeof(buf));

        return "{}"_format(buf);
    }

    ipv6::ipv6(const std::string& str)
    {
        detail::parse_addr(AF_INET6, &addr, str);
        for (int i = 0; i < 8; ++i)
            enc::big_to_host_inplace(addr[i]);
    }

    in6_addr ipv6::to_in6addr() const
    {
        in6_addr ret{};
        std::ranges::copy(addr, ret.s6_addr16);
        for (int i = 0; i < 8; ++i)
            enc::host_to_big_inplace(ret.s6_addr16[i]);

        return ret;
    }

    std::string ipv6::to_string() const
    {
        char buf[INET6_ADDRSTRLEN] = {};

        std::array<uint16_t, 8> temp{};
        std::memcpy(&temp, &addr, sizeof(addr));

        for (int i = 0; i < 8; ++i)
            enc::host_to_big_inplace(temp[i]);

        inet_ntop(AF_INET6, &temp, buf, sizeof(buf));

        return "{}"_format(buf);
    }

    ip_address ip_address::from_socket(int fd)
    {
        sockaddr bind{};
        socklen_t len = sizeof(bind);

        if (getsockname(fd, &bind, &len) < 0)
            throw std::runtime_error{
                    "Failed to get local socket address for socket on fd {}: {}"_format(fd, detail::current_error())};

        return ip_address{&bind};
    }

    bool ip_address::is_anyaddr() const
    {
        return (_is_v4 ? _ipv4().is_anyaddr() : _ipv6().is_anyaddr()) and not _port;
    }

    std::string ip_address::to_string() const
    {
        return "{}:{}"_format(_is_v4 ? _ipv4().to_string() : _ipv6().to_string(), _port);
    }

    std::string path::to_string() const
    {
        return "[ local:{} | remote:{}]"_format(_local, _remote);
    }
}  //  namespace wshttp
