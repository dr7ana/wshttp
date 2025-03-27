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

        METHOD method = detail::get_request_method(req);

        std::invoke(handlers[std::to_underlying(method)], this, req);

        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_request::recv_unsupported(struct evhttp_request* req)
    {
        log->warn("Received unsupported HTTP request (code:{})", std::to_underlying(evhttp_request_get_command(req)));
        evhttp_send_error(req, HTTP_BADMETHOD, nullptr);
    }

    void inbound_request::recv_get(struct evhttp_request* req)
    {
        log->info("Received GET HTTP request from {}", _path.remote());
        evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_request::recv_post(struct evhttp_request* req)
    {
        log->info("Received POST HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_request::recv_head(struct evhttp_request* req)
    {
        log->info("Received HEAD HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_request::recv_put(struct evhttp_request* req)
    {
        log->info("Received PUT HTTP request from {}", _path.remote());

        evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_request::recv_delete(struct evhttp_request* req)
    {
        log->info("Received DELETE HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    outbound_session::outbound_session(endpoint& e, ev_uri u) : _ep{e}, _evuri{std::move(u)}, _use_tls{_evuri.use_tls()}
    {
        log->debug("Outbound session (remote: {}) created", _evuri.hview());
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
            log->critical("Outbound session (remote: {}) failed to create new bufferevent!", _evuri.hview());
            return false;
        }

        _evconn.reset(evhttp_connection_base_bufferevent_new(
                _ep.ev_base(), nullptr, bev, _evuri.host().c_str(), _evuri.port()));

        if (not _evconn)
        {
            log->critical("Outbound session (remote: {}) failed to create new evhttp_connection!", _evuri.hview());
            return false;
        }

        return true;
    }

    std::optional<http_request> outbound_session::make_request(METHOD method)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        auto* bev = new_bev();

        if (not bev)
        {
            log->critical("Outbound session (remote: {}) failed to create new bufferevent!", _evuri.hview());
            return std::nullopt;
        }

        const auto* h = _evuri.host().c_str();

        _evconn.reset(evhttp_connection_base_bufferevent_new(_ep.ev_base(), nullptr, bev, h, _evuri.port()));

        if (not _evconn)
        {
            log->critical("Outbound session (remote: {}) failed to create new evhttp_connection!", _evuri.hview());
            return std::nullopt;
        }

        std::optional<http_request> newreq = std::nullopt;

        try
        {
            newreq = http_request{evhttp_request_new(outbound_callbacks::req_done_cb, this), h, method};
        }
        catch (const std::exception& e)
        {
            log->critical("http_request construction exception: {}", e.what());
        }

        return newreq;
    }

    void outbound_session::initiate_request(METHOD method)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        // if (not make_request_base())
        //     return close();

        auto request = make_request(method);

        if (not request)
        {
            log->warn("Failed to make new evhttp_request!");
            return close();
        }

        if (evhttp_make_request(
                    _evconn.get(), *request, detail::get_method_cmd_type(method), _evuri.pathquery().c_str()) != 0)
        {
            log->warn("Failed to dispatch evhttp_request!");
            return close();
        }

        log->info("Successfully dispatched new evhttp_request for {}", _evuri.to_string());
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
                _evuri.hview(),
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

        const auto* h = _evuri.host().c_str();

        check_rv(SSL_set1_host(_ssl, h), "Outbound SSL set server hostname");

        SSL_set_tlsext_host_name(_ssl, h);

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

        _ep.loop()->call_soon([wep = _ep.weak_from_this(), u = std::move(_evuri)]() mutable {
            if (auto ep = wep.lock())
                ep->close_outbound(std::move(u));
            else
                log->warn("Endpoint closed before outbound session (remote: {}) could be closed", u.host());
        });
    }
}  // namespace wshttp
