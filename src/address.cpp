#include "address.hpp"

#include "internal.hpp"
#include "parser.hpp"

namespace wshttp
{
    uri uri::parse(std::string url)
    {
        return parser->read(std::move(url)) ? parser->extract() : uri{};
    }

    void uri::print_contents() const
    {
        log->info("scheme:{}", _fields[_scheme]);
        log->info("userinfo:{}", _fields[_userinfo]);
        log->info("host:{}", _fields[_host]);
        log->info("port:{}", _fields[_port]);
        log->info("pathname:{}", _fields[_pathname]);
        log->info("query:{}", _fields[_query]);
        log->info("fragment:{}", _fields[_fragment]);
    }

    ipv4::ipv4(struct in_addr* a)
    {
        std::memmove(&addr, &a->s_addr, sizeof(a->s_addr));
        enc::big_to_host_inplace(addr);
    }

    ipv4::ipv4(const std::string& str)
    {
        detail::parse_addr(AF_INET, &addr, str);
        enc::big_to_host_inplace(addr);
    }

    ipv4::operator in_addr() const
    {
        in_addr a;
        a.s_addr = enc::host_to_big(addr);
        return a;
    }

    std::string ipv4::to_string() const
    {
        char buf[INET_ADDRSTRLEN] = {};
        uint32_t net = enc::host_to_big(addr);
        inet_ntop(AF_INET, &net, buf, sizeof(buf));

        return "{}"_format(buf);
    }

    ipv6::ipv6(const struct in6_addr* a)
    {
        std::memmove(&addr, &a->s6_addr, sizeof(a->s6_addr));
        for (int i = 0; i < 8; ++i)
            enc::big_to_host_inplace(addr[i]);
    }

    ipv6::ipv6(const std::string& str)
    {
        detail::parse_addr(AF_INET6, &addr, str);
        for (int i = 0; i < 8; ++i)
            enc::big_to_host_inplace(addr[i]);
    }

    in6_addr ipv6::to_in6() const
    {
        in6_addr ret;
        std::memmove(&ret.s6_addr, &addr, sizeof(ret.s6_addr));
        for (int i = 0; i < 8; ++i)
            enc::big_to_host_inplace(ret.s6_addr[i]);

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

    ip_address::ip_address(struct sockaddr* in)
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

    std::string path::to_string() const
    {
        return "[local={}, remote={}]"_format(_local, _remote);
    }
}  //  namespace wshttp
