#include "request.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

namespace wshttp
{
    static constexpr size_t MAX_URI_LEN{4096};

    static constexpr auto http_scheme = "http"sv;
    static constexpr auto https_scheme = "https"sv;

    uri_t::uri_t(const char* input, size_t inputlen)
    {
        if (inputlen <= MAX_URI_LEN)
            throw std::invalid_argument{"uri length must be <= 4096 (given:{})"_format(inputlen)};

        evuri = evhttp_uri_parse(input);

        if (not evuri)
            throw std::invalid_argument{"evhttp failed to parse uri input: {}"_format(input)};

        if (auto s = scheme(); s == https_scheme)
            _scheme = SCHEME::HTTPS;
        else if (s == http_scheme)
            _scheme = SCHEME::HTTP;
        else
            throw std::invalid_argument{"uri scheme must be one of HTTP/HTTPS (given:{})"_format(input)};
    }

    uri_t::~uri_t()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        if (evuri)
            evhttp_uri_free(evuri);
    }

    std::string_view uri_t::scheme() const
    {
        return evhttp_uri_get_scheme(evuri);
    }

    http_request::http_request(evhttp_request* r, const char* host) :
            req{r}, buffer{evhttp_request_get_output_headers(req)}
    {
        check_rv(evhttp_add_header(buffer, hdr::host, host), "add host hdr", 1);
        check_rv(evhttp_add_header(buffer, hdr::conn, hdr::close), "add conn close hdr", 1);
    }
}  // namespace wshttp
