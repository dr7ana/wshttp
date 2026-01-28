#include "session.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

#include <sys/socket.h>

#include <iostream>

namespace wshttp {
    namespace detail {
        outbound_session* _get_outbound(void* user_arg) {
            return static_cast<outbound_session*>(user_arg);
        }
    }  // namespace detail

    // void outbound_callbacks::req_done_cb(struct evhttp_request* req, void* user_arg)
    // {
    //     return detail::_get_outbound(user_arg)->recv_request(req);
    // }

    inbound_session::inbound_session(
            /* listener& l, */ ip_address remote, evutil_socket_t sock, std::optional<inbound_opts> opts) :
            // _l{l},
            _path{ip_address{}, std::move(remote)}, _fd{sock} {
        unlog::debug("Inbound request has fd: {}", _fd);
        (void)opts;

        int val = 1;
        if (setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0)
            throw std::runtime_error{
                    "Failed to set TCP_NODELAY on inbound request socket: {}"_format(detail::current_error())};

        _path._local = ip_address::from_socket(_fd);
        unlog::info("Successfully configured inbound request; path: {}", _path);
    }

    inbound_session::~inbound_session() {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
    }

    void inbound_session::recv_request(struct evhttp_request* req) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        METHOD method = detail::get_request_method(req);

        std::invoke(handlers[std::to_underlying(method)], this, req);

        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_session::recv_unsupported(struct evhttp_request* req) {
        unlog::warn("Received unsupported HTTP request (code:{})", std::to_underlying(evhttp_request_get_command(req)));
        evhttp_send_error(req, HTTP_BADMETHOD, nullptr);
    }

    void inbound_session::recv_get(struct evhttp_request* req) {
        unlog::info("Received GET HTTP request from {}", _path.remote());
        evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_session::recv_post(struct evhttp_request* req) {
        unlog::info("Received POST HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_session::recv_head(struct evhttp_request* req) {
        unlog::info("Received HEAD HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_session::recv_put(struct evhttp_request* req) {
        unlog::info("Received PUT HTTP request from {}", _path.remote());

        evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_session::recv_delete(struct evhttp_request* req) {
        unlog::info("Received DELETE HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_session::recv_options(struct evhttp_request* req) {
        unlog::info("Received OPTIONS HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_session::recv_trace(struct evhttp_request* req) {
        unlog::info("Received TRACE HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_session::recv_connect(struct evhttp_request* req) {
        unlog::info("Received CONNECT HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    void inbound_session::recv_patch(struct evhttp_request* req) {
        unlog::info("Received PATCH HTTP request from {}", _path.remote());
        return recv_get(req);
        // evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
    }

    outbound_session::outbound_session(endpoint& ep, uri_ptr u, std::optional<session_opts> opts) :
            _ep{ep}, _uri{std::move(u)} {
        unlog::debug("Outbound session (remote: {}) created", _uri->hview());

        auto* bev = new_bev(_uri->use_tls());

        if (not bev)
            throw std::runtime_error{
                    "Outbound session (remote: {}) failed to create new bufferevent!"_format(_uri->hview())};

        _evconn.reset(evhttp_connection_base_bufferevent_new(
                bufferevent_get_base(bev), nullptr, bev, _uri->host().c_str(), _uri->port()));

        if (not _evconn)
            throw std::runtime_error{
                    "Outbound session(remote: {}) failed to create new evhttp_connection!"_format(_uri->hview())};

        if (opts)
            populate_opts(std::move(*opts));
    }

    void outbound_session::populate_opts(session_opts opts) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        if (opts.data_cb)
            _hook = std::move(*opts.data_cb);
        if (opts.media_type)
            _default_type = *opts.media_type;
        if (opts.accept)
            _default_accept = *opts.accept;
        else
            _default_accept = _default_type;
        if (opts.ua)
            _default_user_agent.swap(opts.ua);
        if (opts.timeout) {
            if (opts.timeout->count() < 0) {
                unlog::warn("Ignoring invalid connection timeout: {}s", opts.timeout->count());
            }
            else {
                _timeout = *opts.timeout;
                evhttp_connection_set_timeout(_evconn.get(), static_cast<int>(_timeout->count()));
            }
        }
        if (opts.retries) {
            if (*opts.retries < -1) {
                unlog::warn("Ignoring invalid retry count: {}", *opts.retries);
            }
            else {
                _retries = *opts.retries;
                evhttp_connection_set_retries(_evconn.get(), *_retries);
            }
        }
        if (opts.family) {
            switch (*opts.family) {
                case ip_family::IPV4:
                    _family = *opts.family;
                    evhttp_connection_set_family(_evconn.get(), AF_INET);
                    break;
                case ip_family::IPV6:
                    _family = *opts.family;
                    evhttp_connection_set_family(_evconn.get(), AF_INET6);
                    break;
                case ip_family::ANY:
                    _family = *opts.family;
                    evhttp_connection_set_family(_evconn.get(), AF_UNSPEC);
                    break;
                default:
                    unlog::warn("Ignoring invalid IP family hint: {}", std::to_underlying(*opts.family));
                    break;
            }
        }
    }

    outbound_session::~outbound_session() {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
    }

    void outbound_session::initiate_request(METHOD method, uri_ptr req_uri, std::optional<request_opts> opts) {
        _ep.loop()->call([&]() {
            unlog::trace("{} called", __PRETTY_FUNCTION__);

            auto& req = _request_que.emplace_front(
                    http_request::construct(*this, ++_next_request_id, std::move(req_uri), method, std::move(opts)));

            if (not req) {
                unlog::warn("Outbound session (remote: {}) failed to create http request!", _uri->hview());
                _request_que.erase(_request_que.begin());
            }

            auto [it, b] = _request_table.emplace(_next_request_id, _request_que.begin());

            if (!b) [[unlikely]] {
                unlog::critical("ERROR: REQUEST ID COLLISION");
                req->set_close_on_complete();
            }
            else
                unlog::info("Successfully dispatched new evhttp_request for {}", _uri->hview());
        });
    }

    void outbound_session::request(METHOD method, std::optional<request_opts> opts) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        initiate_request(method, _uri, std::move(opts));
    }

    void outbound_session::request(METHOD method, std::string_view path, std::optional<request_opts> opts) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        auto new_uri = uri::make(path, _uri->base());
        if (!new_uri) {
            unlog::warn("Failed to parse request path {} for remote {}", path, _uri->hview());
            return;
        }
        initiate_request(method, std::move(new_uri), std::move(opts));
    }

    // void outbound_session::recv_request(struct evhttp_request* req)
    // {
    //     assert(_ep.in_event_loop());
    //     unlog::trace("{} called", __PRETTY_FUNCTION__);

    //     if (auto it = _request_que.find(req); it != _request_que.end())
    //         (*it)->request_recv(req);
    // }

    void outbound_session::close_request(request_id_t id) {
        assert(_ep.in_event_loop());
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        _ep.loop()->call_soon([this, id]() mutable {
            if (auto it = _request_table.find(id); it != _request_table.end()) {
                _request_que.erase(it->second);
                _request_table.erase(it);
                unlog::debug("Deleted completed request (id:{})", id);
            }
            else
                unlog::warn("Failed to find completed request id:{} in lookup table and request que!", id);
        });
    }

    SSL* outbound_session::new_ssl() {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        SSL* _ssl = SSL_new(_ep.outbound_ctx());

        if (!_ssl)
            throw std::runtime_error{"Failed to create SSL/TLS: {}"_format(detail::current_error())};

        unlog::trace("Created SSL/TLS...");

        const auto* h = _uri->host().c_str();

        check_rv(SSL_set1_host(_ssl, h), "Outbound SSL set server hostname");

        SSL_set_tlsext_host_name(_ssl, h);

        return _ssl;
    }

    bufferevent* outbound_session::new_bev(bool ssl) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        bufferevent* bev = nullptr;

        if (ssl) {
            bev = bufferevent_openssl_socket_new(
                    _ep.ev_base(), -1, new_ssl(), BUFFEREVENT_SSL_CONNECTING, default_bev_flags);

            bufferevent_ssl_set_flags(bev, BUFFEREVENT_SSL_DIRTY_SHUTDOWN);
        }
        else
            bev = bufferevent_socket_new(_ep.ev_base(), -1, default_bev_flags);

        return bev;
    }

    request_data_cb outbound_session::make_req_data_caller() {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        if (_hook)
            return [this](std::vector<char> buf) mutable -> void {
                _ep.loop()->call([&]() { _hook(std::move(buf)); });
            };
        else
            return nullptr;
    }

    request_data_cb outbound_session::make_req_data_caller(request_data_cb cb) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        return [this, cb = std::move(cb)](std::vector<char> buf) mutable -> void {
            _ep.loop()->call([&]() { cb(std::move(buf)); });
        };
    }

    void outbound_session::close() {
        unlog::debug("Outbound (remote: {}) signalling endpoint to close session...", _uri->hview());

        _ep.loop()->call_soon([wep = _ep.weak_from_this(), remote = _uri->host_domain()]() mutable {
            if (auto ep = wep.lock())
                ep->close_outbound(remote);
            else
                unlog::warn("Endpoint closed before outbound session (remote: {}) could be closed", remote.host());
        });
    }
}  // namespace wshttp
