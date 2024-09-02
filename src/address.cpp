#include "address.hpp"

#include "encoding.hpp"
#include "internal.hpp"
#include "parser.hpp"

namespace wshttp
{
    uri uri::parse(std::string url)
    {
        return parser->read(std::move(url)) ? parser->extract() : uri{};
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
}  //  namespace wshttp
