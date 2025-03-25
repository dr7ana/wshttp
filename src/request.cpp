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

        if (auto s = evhttp_uri_get_scheme(evuri); !s)
            throw std::invalid_argument{"evhttp failed to parse uri scheme (given:{})"_format(input)};
        else
        {
            std::string_view sv{s};
            if (sv == https_scheme)
                _scheme = SCHEME::HTTPS;
            else if (sv == http_scheme)
                _scheme = SCHEME::HTTP;
            else
                throw std::invalid_argument{"uri scheme must be one of HTTP/HTTPS (given:{})"_format(sv)};
        }
    }

    uri_t::~uri_t()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        if (evuri)
            evhttp_uri_free(evuri);
    }

    std::string_view uri_t::scheme() const
    {
        switch (_scheme)
        {
            case SCHEME::HTTP:
                return http_scheme;
            case SCHEME::HTTPS:
                return https_scheme;
            default:
                [[unlikely]] return "ERROR"sv;
        }
    }

    http_request::http_request(evhttp_request* r, const char* host) :
            req{r}, buffer{evhttp_request_get_output_headers(req)}
    {
        check_rv(evhttp_add_header(buffer, hdr::host, host), "add host hdr", 1);
        check_rv(evhttp_add_header(buffer, hdr::conn, hdr::close), "add conn close hdr", 1);
    }
}  // namespace wshttp
