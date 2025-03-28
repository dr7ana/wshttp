#include "address.hpp"

#include "internal.hpp"
// #include "parser.hpp"

namespace wshttp
{
    namespace detail
    {
        static void parse_addr(int af, void* dest, const std::string& from)
        {
            auto rv = inet_pton(af, from.c_str(), dest);

            if (rv == 0)
                throw std::invalid_argument{"Unable to parse IP address!"};
            if (rv < 0)
                throw std::system_error{errno, std::system_category()};
        }
    }  // namespace detail

    static constexpr size_t MAX_URI_LEN{4096};

    static constexpr auto tls_port = "443"sv;
    static constexpr auto notls_port = "80"sv;

    // scheme strings + colon delimited for comparison w/ ada
    static constexpr auto http_scheme = "http"sv, http_scheme_d = "http:"sv;
    static constexpr auto https_scheme = "https"sv, https_scheme_d = "https:"sv;
    static constexpr auto ws_scheme = "ws"sv, ws_scheme_d = "ws:"sv;
    static constexpr auto wss_scheme = "wss"sv, wss_scheme_d = "wss:"sv;

    static constexpr auto str_to_scheme(std::string_view s)
    {
        if (s == https_scheme_d)
            return SCHEME::HTTPS;
        else if (s == http_scheme_d)
            return SCHEME::HTTP;
        else if (s == wss_scheme_d)
            return SCHEME::WSS;
        else if (s == ws_scheme_d)
            return SCHEME::WS;
        else
            return SCHEME::UNSUPPORTED;
    }

    static constexpr auto scheme_to_str(SCHEME s)
    {
        switch (s)
        {
            case SCHEME::HTTP:
                return http_scheme;
            case SCHEME::HTTPS:
                return https_scheme;
            case SCHEME::WS:
                return ws_scheme;
            case SCHEME::WSS:
                return wss_scheme;
            default:
                [[unlikely]] return "UNSUPPORTED"sv;
        }
    }

    ev_uri::ev_uri(std::string_view input, const url_result_ptr& base)
    {
        if (input.size() >= MAX_URI_LEN)
            throw std::invalid_argument{"uri length must be <= 4096 (given:{})"_format(input.size())};

        // parser throws on error
        _url = parser->parse(input, base);

        if (!_url)
            throw std::invalid_argument{"uri length must be <= 4096 (given:{})"_format(input.size())};

        _populate_internals();
        // _evuri.reset(evhttp_uri_parse(input));

        // if (not _evuri)
        //     throw std::invalid_argument{"evhttp failed to parse uri input: {}"_format(input)};

        // if (auto s = evhttp_uri_get_scheme(_evuri.get()); !s)
        //     throw std::invalid_argument{"evhttp failed to parse uri scheme (given:{})"_format(input)};

        log->info("parsed url: {}", url().get_href());
    }

    void ev_uri::_populate_internals()
    {
        auto s = url().get_protocol();

        if (s.empty())
            throw std::invalid_argument{
                    "uri must use protocol schemes HTTP/S or WS/S (input: {})"_format(url().get_href())};

        _scheme = str_to_scheme(s);
        if (_scheme == SCHEME::UNSUPPORTED)
            throw std::invalid_argument{"uri must use protocol schemes HTTP/S or WS/S (given: {})"_format(s)};

        if (!s.empty())
        {

            if (s == https_scheme_d)
                _scheme = SCHEME::HTTPS;
            else if (s == http_scheme_d)
                _scheme = SCHEME::HTTP;
            else if (s == wss_scheme_d)
                _scheme = SCHEME::WSS;
            else if (s == ws_scheme_d)
                _scheme = SCHEME::WS;
            else
                throw std::invalid_argument{"uri must use protocol schemes HTTP/S or WS/S (given: {})"_format(s)};
        }
        else
            throw std::invalid_argument{"uri must use protocol schemes HTTP/S or WS/S (given: empty protocol)"};

        _use_tls = s.ends_with('s');

        auto h = url().get_host();
        if (h.empty())
            throw std::invalid_argument{"uri cannot have empty host field!"};

        _host += h;
        _pathquery += url().get_pathname();
        _pathquery += url().get_search();

        log->trace("uri pathquery: {}", _pathquery);

        auto p = url().get_port();

        if (p.empty())
        {
            if (_use_tls)
            {
                url().set_port(tls_port);
                _port = 443;
            }
            else
            {
                url().set_port(notls_port);
                _port = 80;
            }
        }
        else
            _port = std::atoi(p.data());
    }

    ev_uri::~ev_uri()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
    }

    domain_host ev_uri::host_domain() const
    {
        return domain_host{url().get_host()};
    }

    std::string_view ev_uri::scheme() const
    {
        switch (_scheme)
        {
            case SCHEME::HTTP:
                return http_scheme;
            case SCHEME::HTTPS:
                return https_scheme;
            case SCHEME::WS:
                return ws_scheme;
            case SCHEME::WSS:
                return wss_scheme;
            default:
                [[unlikely]] return "ERROR"sv;
        }
    }

    std::string_view ev_uri::view() const
    {
        return url().get_href();
    }

    std::string ev_uri::to_string() const
    {
        return "uri:[ {} ]"_format(url().get_href());
    }

    ipv4::ipv4(const std::string& str)
    {
        detail::parse_addr(AF_INET, &addr, str);
        enc::big_to_host_inplace(addr);
    }

    in_addr ipv4::to_inaddr() const
    {
        in_addr a{};
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

        std::array<uint16_t, 8> temp{addr};

        for (int i = 0; i < 8; ++i)
            enc::host_to_big_inplace(temp[i]);

        inet_ntop(AF_INET6, &temp, buf, sizeof(buf));

        return "{}"_format(buf);
    }

    ip_address::ip_address(const struct sockaddr* in)
    {
        if (in->sa_family == AF_INET)
        {
            auto* in4 = reinterpret_cast<const sockaddr_in*>(in);
            _ip = ipv4{in4}, _port = enc::big_to_host(in4->sin_port);
            _is_anyaddr = _ipv4().is_anyaddr();
            _is_v4 = true;
        }
        else if (in->sa_family == AF_INET6)
        {
            auto* in6 = reinterpret_cast<const sockaddr_in6*>(in);
            _ip = ipv6{in6}, _port = enc::big_to_host(in6->sin6_port);
            _is_anyaddr = _ipv6().is_anyaddr();
            _is_v4 = false;
        }
        else
            throw std::runtime_error{"Failed to understand incoming address sa_family: {}"_format(in->sa_family)};
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

    std::string ip_address::to_string() const
    {
        return "{}:{}"_format(_is_v4 ? _ipv4().to_string() : _ipv6().to_string(), _port);
    }

    std::string path::to_string() const
    {
        return "[ local:{} | remote:{} ]"_format(_local, _remote);
    }
}  //  namespace wshttp
