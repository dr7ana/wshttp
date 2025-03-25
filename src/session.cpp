#include "session.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

#include <iostream>

namespace wshttp
{
    namespace detail
    {
        outbound_session* _get_outbound(void* user_arg)
        {
            return static_cast<outbound_session*>(user_arg);
        }
    }  // namespace detail

    void outbound_callbacks::req_done_cb(struct evhttp_request* req, void* user_arg)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_outbound(user_arg)->recv_response(req);
    }

    void outbound_callbacks::req_error_cb(evhttp_request_error ec, void* /* user_arg */)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        log->critical("HTTP request error: {}", detail::evreq_err_str(ec));
    }

    inbound_request::inbound_request(/* listener& l, */ ip_address remote, evutil_socket_t sock) :
            // _l{l},
            _path{ip_address{}, std::move(remote)}, _fd{sock}
    {
        log->debug("Inbound request has fd: {}", _fd);

        int val = 1;
        if (setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0)
            throw std::runtime_error{
                    "Failed to set TCP_NODELAY on inbound request socket: {}"_format(detail::current_error())};

        _path._local = ip_address::from_socket(_fd);
        log->info("Successfully configured inbound request; path: {}", _path);
    }

    inbound_request::~inbound_request()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
    }

    void inbound_request::recv_request(struct evhttp_request* req)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        auto method = detail::get_request_method(req);

        switch (method)
        {
            case METHOD::UNSUPPORTED:
                log->warn(
                        "Received unsupported HTTP request (code:{})",
                        std::to_underlying(evhttp_request_get_command(req)));
                return evhttp_send_error(req, HTTP_BADMETHOD, nullptr);
            case METHOD::GET:
            case METHOD::POST:
            case METHOD::HEAD:
            case METHOD::PUT:
            case METHOD::DELETE:
                log->info("Received {} HTTP request from {}", detail::get_method_string(method), _path.remote());
                break;
        }

        evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    outbound_session::outbound_session(endpoint& e, uri u) : _ep{e}, _uri{std::move(u)}, _use_tls{_uri.use_tls()}
    {
        log->debug("Outbound session (remote: {}) created", _uri.host());
    }

    outbound_session::~outbound_session()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
    }

    bool outbound_session::make_request_base()
    {
        // bufferevent is freed by evhttp
        auto* bev = new_bev();

        if (not bev)
        {
            log->critical("Outbound session (remote: {}) failed to create new bufferevent!", _uri.host());
            return false;
        }

        _evconn.reset(
                evhttp_connection_base_bufferevent_new(_ep.ev_base(), nullptr, bev, _uri.host_cstr(), _uri.port()));

        if (not _evconn)
        {
            log->critical("Outbound session (remote: {}) failed to create new evhttp_connection!", _uri.host());
            return false;
        }

        return true;
    }

    void outbound_session::initiate_request()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
    }

    void outbound_session::make_request(METHOD method)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        if (not make_request_base())
            return close();

        evhttp_request* req = evhttp_request_new(outbound_callbacks::req_done_cb, this);

        if (not req)
        {
            log->warn("Failed to make new evhttp_request!");
            return close();
        };

        evhttp_request_set_error_cb(req, outbound_callbacks::req_error_cb);

        auto* buffer = evhttp_request_get_output_headers(req);

        check_rv(evhttp_add_header(buffer, hdr::host, _uri.host_cstr()), "add host hdr", 0);
        check_rv(evhttp_add_header(buffer, hdr::conn, hdr::close), "add conn close hdr", 0);

        auto evmethod = detail::get_method_cmd_type(method);

        if (evhttp_make_request(_evconn.get(), req, evmethod, _uri.path_cstr()) != 0)
        {
            log->warn("Failed to dispatch evhttp_request!");
            return close();
        }

        log->info("Successfully dispatched new evhttp_request for: {}", _uri.href());
    }

    void outbound_session::recv_response(struct evhttp_request* req)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        if (!req || !evhttp_request_get_response_code(req))
        {
            log->critical(
                    "http request failure:\n\topenssl:{}\n\tevutil:{}\n",
                    detail::current_error(),
                    evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
            return close();
        }

        int code{evhttp_request_get_response_code(req)};
        std::string_view line{evhttp_request_get_response_code_line(req)};

        auto* evbuffer = evhttp_request_get_input_buffer(req);
        int nread = evbuffer_get_length(evbuffer);

        // std::fwrite(evbuffer_pullup(evbuffer, nread), nread, 1, stderr);
        evbuffer_drain(evbuffer, nread);

        log->info(
                "Request (remote:{}) received response:[ payload:{}B | code:{} | line: {} ]",
                _uri.host(),
                nread,
                code,
                line);
        close();
    }

    SSL* outbound_session::new_ssl()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        SSL* _ssl = SSL_new(_ep.outbound_ctx());

        if (!_ssl)
            throw std::runtime_error{"Failed to create SSL/TLS: {}"_format(detail::current_error())};

        log->trace("Created SSL/TLS...");

        check_rv(SSL_set1_host(_ssl, _uri.host_cstr()), "Outbound SSL set server hostname");

        SSL_set_tlsext_host_name(_ssl, _uri.host_cstr());

        return _ssl;
    }

    static constexpr auto bev_flags{BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_THREADSAFE};

    bufferevent* outbound_session::new_bev()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        bufferevent* bev = nullptr;

        if (_use_tls)
        {
            bev = bufferevent_openssl_socket_new(_ep.ev_base(), -1, new_ssl(), BUFFEREVENT_SSL_CONNECTING, bev_flags);

            bufferevent_ssl_set_flags(bev, BUFFEREVENT_SSL_DIRTY_SHUTDOWN);
        }
        else
            bev = bufferevent_socket_new(_ep.ev_base(), -1, bev_flags);

        return bev;
    }

    void outbound_session::close()
    {
        log->debug("Outbound session signalling endpoint to close listener...");

        _ep.loop()->call_soon([wep = _ep.weak_from_this(), u = _uri]() mutable {
            if (auto ep = wep.lock())
                ep->close_outbound(std::move(u));
            else
                log->warn("Endpoint closed before outbound session (remote: {}) could be closed", u.host());
        });
    }
}  // namespace wshttp
