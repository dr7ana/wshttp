#include "request.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

namespace wshttp
{
    void request_callbacks::req_error_cb(evhttp_request_error ec, void* /* user_arg */)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        log->critical("HTTP request error: {}", detail::evreq_err_str(ec));
    }

    http_request::http_request(evhttp_request* r, const char* host, METHOD m) :
            req{r}, buffer{evhttp_request_get_output_headers(req)}, method{m}
    {
        // TODO: fully encapsulate request production w/ new constructor
        if (not req)
            throw std::runtime_error{"Failed to make new evhttp_request!"};

        evhttp_request_set_error_cb(req, request_callbacks::req_error_cb);

        check_rv(evhttp_add_header(buffer, hdr::host, host), "add host hdr", 0);
        check_rv(evhttp_add_header(buffer, hdr::conn, hdr::close), "add conn close hdr", 0);
    }

}  // namespace wshttp
