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

    void inbound_request::recv_options(struct evhttp_request* req)
    {
        log->info("Received OPTIONS HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_request::recv_trace(struct evhttp_request* req)
    {
        log->info("Received TRACE HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_request::recv_connect(struct evhttp_request* req)
    {
        log->info("Received CONNECT HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_request::recv_patch(struct evhttp_request* req)
    {
        log->info("Received PATCH HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    outbound_session::outbound_session(endpoint& ep, domain_host remote) : _ep{ep}, _remote{std::move(remote)}
    {
        log->debug("Outbound session (remote: {}) created", _remote.host());
    }

    outbound_session::~outbound_session()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
    }

    void outbound_session::initiate_request(uri u, METHOD method)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        _request_que.emplace_front(http_request::construct(*this, ++_next_request_id, std::move(u), method));

        auto [it, b] = _request_table.emplace(_next_request_id, _request_que.begin());

        if (not *it->second)
        {
            log->warn("Outbound session (remote: {}) failed to create http request!");
            _request_que.erase(it->second);
            _request_table.erase(it);
        }
        else
            log->info("Successfully dispatched new evhttp_request for {}", _remote.host());
    }

    void outbound_session::close_request(request_id_t id)
    {
        assert(_ep.in_event_loop());
        log->trace("{} called", __PRETTY_FUNCTION__);
        _ep.loop()->call_soon([this, id = id]() mutable {
            if (auto it = _request_table.find(id); it != _request_table.end())
            {
                _request_que.erase(it->second);
                log->debug("Deleted completeed request (id:{})", id);
            }
            else if (_request_que.remove_if([id](auto& r) { return r->_request_id == id; }))
                log->info("Could not lookup request (id:{}); deleted directly from request que", id);
            else
                log->warn("Failed to find completed request id:{} in lookup table and request que!", id);
        });
    }

    SSL* outbound_session::new_ssl()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        SSL* _ssl = SSL_new(_ep.outbound_ctx());

        if (!_ssl)
            throw std::runtime_error{"Failed to create SSL/TLS: {}"_format(detail::current_error())};

        log->trace("Created SSL/TLS...");

        const auto* h = _remote.host_cstr();

        check_rv(SSL_set1_host(_ssl, h), "Outbound SSL set server hostname");

        SSL_set_tlsext_host_name(_ssl, h);

        return _ssl;
    }

    static constexpr auto bev_flags{BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_THREADSAFE};

    bufferevent* outbound_session::new_bev(bool ssl)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        bufferevent* bev = nullptr;

        if (ssl)
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

        _ep.loop()->call_soon([wep = _ep.weak_from_this(), remote = _remote]() mutable {
            if (auto ep = wep.lock())
                ep->close_outbound(remote);
            else
                log->warn("Endpoint closed before outbound session (remote: {}) could be closed", remote.host());
        });
    }
}  // namespace wshttp
