#include "request.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

namespace wshttp
{
    struct hdr_fields
    {
        static constexpr auto* accept = "Accept";
        static constexpr auto* close = "close";
        static constexpr auto* conn = "Connection";
        static constexpr auto* host = "Host";
    };

    static constexpr auto content_type_string(content_type t)
    {
        switch (t)
        {
            default:
            case content_type::WILDCARD:
                return "*/*"sv;
            case content_type::APP_WC:
                return "application/*"sv;
            case content_type::JSON:
                return "application/json"sv;
            case content_type::MISC:
                return "application/misc"sv;
            case content_type::U8STREAM:
                return "application/octet-stream"sv;
            case content_type::XURL:
                return "application/x-www-form-urlencoded"sv;
            case content_type::XML:
                return "application/xml"sv;
            case content_type::TEXT_WC:
                return "text/*"sv;
            case content_type::CSS:
                return "text/css"sv;
            case content_type::STREAM:
                return "text/event-stream"sv;
            case content_type::HTML:
                return "text/html"sv;
            case content_type::PLAIN:
                return "text/plain"sv;
        }
    }

    namespace detail
    {
        http_request* _get_request(void* user_arg)
        {
            return static_cast<http_request*>(user_arg);
        }
    }  // namespace detail

    size_t hash_reqptr(evhttp_request* req)
    {
        return reinterpret_cast<std::uintptr_t>(req);
    }

    void request_callbacks::req_done_cb(struct evhttp_request* req, void* user_arg)
    {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        unlog::critical("ptr hash: {}", hash_reqptr(req));
        return detail::_get_request(user_arg)->request_recv(req);
    }

    void request_callbacks::req_error_cb(evhttp_request_error ec, void* user_arg)
    {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        unlog::info("HTTP request error: {}", detail::evreq_err_str(ec));
        return detail::_get_request(user_arg)->test_method();
    }

    http_request::http_request(
            outbound_session& s, request_id_t id, uri_ptr u, METHOD m, std::optional<request_opts> opts) :
            _request_id{id}, _session{s}, _uri{std::move(u)}, _method{m}
    {
        _req.reset(evhttp_request_new(request_callbacks::req_done_cb, this));
        // _req.reset(_session.new_req());

        if (not _req)
            throw std::runtime_error{"Outbound request failed to make new evhttp_request object!"};

        if (opts)
            populate_opts(std::move(*opts));
        else
            populate_opts();

        unlog::critical("ptr hash: {}", hash_reqptr(_req.get()));

        evhttp_request_own(_req.get());
        assert(evhttp_request_is_owned(_req.get()));

        evhttp_request_set_error_cb(_req.get(), request_callbacks::req_error_cb);

        auto* evbuffer = evhttp_request_get_output_headers(_req.get());

        check_rv(evhttp_add_header(evbuffer, hdr_fields::host, _uri->host().c_str()), "add host hdr", 0);
        check_rv(
                evhttp_add_header(evbuffer, hdr_fields::accept, content_type_string(content_type::JSON).data()),
                "add json hdr",
                0);

        if (_close_session_on_complete)
            check_rv(evhttp_add_header(evbuffer, hdr_fields::conn, hdr_fields::close), "add conn close hdr", 0);

        if (evhttp_make_request(
                    _session._evconn.get(),
                    _req.get(),
                    detail::get_method_cmd_type(_method),
                    _uri->pathquery().c_str()) != 0)
            throw std::runtime_error{"Failed to dispatch evhttp_request!"};
    }

    http_request::~http_request()
    {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
    }

    void http_request::populate_opts()
    {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        _type = _session._default_type;
        _hook = _session.make_req_data_caller();
    }

    void http_request::populate_opts(request_opts opts)
    {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        if (opts.media_type)
            _type = *opts.media_type;
        else
            _type = _session._default_type;

        if (opts.data_cb)
            _hook = _session.make_req_data_caller(std::move(*opts.data_cb));
        else
            _hook = _session.make_req_data_caller();

        if (opts.flags & std::to_underlying(hdr_flags::CLOSE))
        {
            unlog::critical("GOOD");
            _close_session_on_complete = true;
        }
    }

    http_req_ptr http_request::construct(
            outbound_session& s, request_id_t id, uri_ptr u, METHOD m, std::optional<request_opts> opts)
    {
        http_req_ptr ret = nullptr;

        try
        {
            ret = std::unique_ptr<http_request>(new http_request{s, id, std::move(u), m, std::move(opts)});
        }
        catch (const std::exception& e)
        {
            unlog::critical("http_request construction exception: {}", e.what());
        }

        return ret;
    }

    void http_request::signal_close(bool close_session)
    {
        if (close_session)
        {
            unlog::debug("Signalling outbound termination of remote session");
            _session.close();
        }
        else
        {
            unlog::debug("Signalling outbound session to close completed request (id:{})", _request_id);
            _session.close_request(_request_id);
        }
    }

    void http_request::set_close_on_complete()
    {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        _close_session_on_complete = true;
    }

    void http_request::test_method()
    {
        unlog::critical("inner ptr hash: {}", hash_reqptr(_req.get()));
    }

    void http_request::request_recv(struct evhttp_request* req)
    {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        if (!req || !evhttp_request_get_response_code(req))
        {
            unlog::info("closing http request id:{}", _request_id);
            return signal_close();
        }

        if (!_req || !evhttp_request_get_response_code(_req.get()))
        {
            unlog::critical("ERROR: inner http req ptr is gone? Shutting down session");
            return signal_close(true);
        }

        unlog::critical("function ptr hash: {}", hash_reqptr(req));
        unlog::critical("inner ptr hash: {}", hash_reqptr(_req.get()));

        int code1{evhttp_request_get_response_code(req)};
        std::string_view line1{evhttp_request_get_response_code_line(req)};

        int nread_comp = evbuffer_get_length(evhttp_request_get_input_buffer(req));

        int code{evhttp_request_get_response_code(_req.get())};
        std::string_view line{evhttp_request_get_response_code_line(_req.get())};

        auto* evbuffer = evhttp_request_get_input_buffer(_req.get());

        int nread = evbuffer_get_length(evbuffer);

        unlog::debug("bufsize={}, code={}, line={}", nread == nread_comp, code == code1, line == line1);

        unlog::info(
                "Request (remote:{}) received response:[ payload:{}B | code:{} | line: {} ]",
                _uri->hview(),
                nread,
                code,
                line);

        std::vector<char> buf(nread);

        if (evbuffer_remove(evbuffer, buf.data(), nread) < 0)
        {
            unlog::critical("Buffer error: failed to remove request data from evbuffer!");
            return signal_close(true);
        }

        if (buf.back() != '\n')
            buf.push_back('\n');

        unlog::debug("{}B read from request response body (id:{})", nread, _request_id);

        if (_hook)
            _hook(std::move(buf));

        signal_close(_close_session_on_complete);
    }
}  // namespace wshttp
