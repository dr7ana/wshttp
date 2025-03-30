#include "request.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

namespace wshttp
{

    namespace detail
    {
        http_request* _get_request(void* user_arg)
        {
            return static_cast<http_request*>(user_arg);
        }
    }  // namespace detail

    void request_callbacks::req_done_cb(struct evhttp_request* req, void* user_arg)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_request(user_arg)->recv_response(req);
    }

    void request_callbacks::req_error_cb(evhttp_request_error ec, void* /* user_arg */)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        log->critical("HTTP request error: {}", detail::evreq_err_str(ec));
    }

    http_request::http_request(outbound_session& s, request_id_t id, uri u, METHOD m) :
            _request_id{id}, _session{s}, _uri{std::move(u)}, _method{m}
    {
        auto* bev = s.new_bev();

        if (not bev)
            throw std::runtime_error{
                    "Outbound request (remote: {}) failed to create new bufferevent!"_format(u.hview())};

        auto* h = _uri.host().c_str();

        _evconn.reset(evhttp_connection_base_bufferevent_new(bufferevent_get_base(bev), nullptr, bev, h, _uri.port()));

        if (not _evconn)
            throw std::runtime_error{
                    "Outbound request (remote: {}) failed to create new evhttp_connection!"_format(_uri.hview())};

        _req.reset(evhttp_request_new(request_callbacks::req_done_cb, this));

        if (not _req)
            throw std::runtime_error{"Outbound request failed to make new evhttp_request object!"};

        evhttp_request_own(_req.get());
        assert(evhttp_request_is_owned(_req.get()));

        evhttp_request_set_error_cb(_req.get(), request_callbacks::req_error_cb);

        _buffer = evhttp_request_get_output_headers(_req.get());

        check_rv(evhttp_add_header(_buffer, hdr::host, h), "add host hdr", 0);
        check_rv(evhttp_add_header(_buffer, hdr::conn, hdr::close), "add conn close hdr", 0);

        if (evhttp_make_request(
                    _evconn.get(), _req.get(), detail::get_method_cmd_type(_method), _uri.pathquery().c_str()) != 0)
            throw std::runtime_error{"Failed to dispatch evhttp_request!"};
    }

    http_req_ptr http_request::construct(outbound_session& s, request_id_t id, uri u, METHOD m)
    {
        http_req_ptr ret = nullptr;

        try
        {
            ret = std::unique_ptr<http_request>(new http_request{s, id, std::move(u), m});
        }
        catch (const std::exception& e)
        {
            log->critical("http_request construction exception: {}", e.what());
        }

        return ret;
    }

    void http_request::signal_close(bool close_session)
    {
        if (close_session)
        {
            log->debug("Signalling outbound termination of remote session");
            _session.close();
        }
        else
        {
            log->debug("Signalling outbound session to close completed request (id:{})", _request_id);
            _session.close_request(_request_id);
        }
    }

    void http_request::recv_response(struct evhttp_request* req)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        if (!req || !evhttp_request_get_response_code(req))
        {
            log->info("closing http request id:{}", _request_id);
            return signal_close();
        }

        int code{evhttp_request_get_response_code(req)};
        std::string_view line{evhttp_request_get_response_code_line(req)};

        auto* evbuffer = evhttp_request_get_input_buffer(req);
        int nread = evbuffer_get_length(evbuffer);

        // std::fwrite(evbuffer_pullup(evbuffer, nread), nread, 1, stderr);
        evbuffer_drain(evbuffer, nread);

        log->info(
                "Request (remote:{}) received response:[ payload:{}B | code:{} | line: {} ]",
                _uri.hview(),
                nread,
                code,
                line);

        /** If close flag set by user or returned from server, close
         */

        // close();
    }

    // http_request::http_request(evhttp_request* r, const char* host, METHOD m) :
    //         req{r}, buffer{evhttp_request_get_output_headers(req)}, _method{m}
    // {
    //     // TODO: fully encapsulate request production w/ new constructor
    //     if (not req)
    //         throw std::runtime_error{"Failed to make new evhttp_request!"};

    //     evhttp_request_set_error_cb(req, request_callbacks::req_error_cb);

    //     check_rv(evhttp_add_header(buffer, hdr::host, host), "add host hdr", 0);
    //     check_rv(evhttp_add_header(buffer, hdr::conn, hdr::close), "add conn close hdr", 0);
    // }

}  // namespace wshttp
