#include "listener.hpp"

#include "endpoint.hpp"
#include "internal.hpp"
#include "request.hpp"
#include "ws.hpp"

namespace wshttp {
    namespace detail {
        static listener* _get_listener(void* user_arg) {
            return static_cast<listener*>(user_arg);
        }

        struct request_complete_ctx {
            listener* l;
            request_id_t id;
        };
    }  // namespace detail

    void listen_callbacks::gen_cb(struct evhttp_request* req, void* user_arg) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->handle_request(req);
    }

    bufferevent* listen_callbacks::bev_cb(struct event_base* /* ev */, void* user_arg) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->new_bev();
    }

    int listen_callbacks::newreq_cb(struct evhttp_request* req, void* user_arg) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->recv_request(req);
    }

    void listen_callbacks::ws_cb(struct evhttp_request* req, void* user_arg) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->ws_request(req);
    }

    void listen_callbacks::req_complete_cb(struct evhttp_request* /* req */, void* user_arg) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        auto* ctx = static_cast<detail::request_complete_ctx*>(user_arg);
        if (!ctx) {
            unlog::warn("Request completion callback missing context");
            return;
        }
        ctx->l->request_complete(ctx->id);
        delete ctx;
    }

    int listen_callbacks::error_cb(
            struct evhttp_request* req, struct evbuffer* /* buffer */, int error, const char* reason, void* user_arg) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->request_error(req, error, reason);
    }

    listener::listener(endpoint& e, ip_address bind, std::optional<inbound_opts> opts) :
            _ep{e}, _local{std::move(bind)}, _iopts{std::move(opts)} {
        _init_internals();
    }

    void listener::_init_internals() {
        assert(_ep.in_event_loop());

        sockaddr saddr{};

        if (_local.is_ipv4()) {
            auto* in = reinterpret_cast<sockaddr_in*>(&saddr);
            in->sin_family = AF_INET;
            in->sin_addr = _local;  // operator in_addr()
            in->sin_port = enc::host_to_big(_local.port());
        }
        else {
            auto* in6 = reinterpret_cast<sockaddr_in6*>(&saddr);
            in6->sin6_family = AF_INET6;
            in6->sin6_addr = _local;  // operator in6_addr()
            in6->sin6_port = enc::host_to_big(_local.port());
        }

        _tcp.reset(evconnlistener_new_bind(
                _ep.ev_base(),
                nullptr,
                this,
                LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE,
                -1,
                reinterpret_cast<sockaddr*>(&saddr),
                sizeof(sockaddr)));

        if (not _tcp) {
            throw std::runtime_error{"TCP listener construction is fucked: {}"_format(
                    evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()))};
        }

        _evh.reset(_ep.make_evhttp());
        evhttp_set_gencb(_evh.get(), listen_callbacks::gen_cb, this);
        evhttp_set_cb(_evh.get(), "/ws", listen_callbacks::ws_cb, this);
        evhttp_set_bevcb(_evh.get(), listen_callbacks::bev_cb, this);
        evhttp_set_newreqcb(_evh.get(), listen_callbacks::newreq_cb, this);
        evhttp_set_errorcb(_evh.get(), listen_callbacks::error_cb, this);
        evhttp_set_allowed_methods(_evh.get(), default_evhttp_flags);

        evhttp_bound_socket* handle = evhttp_bind_listener(_evh.get(), _tcp.get());

        if (!handle) {
            throw std::runtime_error{"Failed to bind evhttp listener to {}"_format(_local)};
        }

        _fd = evhttp_bound_socket_get_fd(handle);
        unlog::debug("evhttp listener has fd: {}", _fd);

        int val = 1;
        if (setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0) {
            throw std::runtime_error{
                    "Failed to set TCP_NODELAY on inbound listener socket: {}"_format(detail::current_error())};
        }

        _local = ip_address::from_socket(_fd);

        unlog::info("evhttp listener deployed on local bind: {}", _local);

        _register_handlers();
    }

    void listener::_register_handlers() {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        const auto default_handler = [this](std::shared_ptr<http_request> req) mutable {
            _ep.loop()->call([req]() mutable {
                unlog::warn("No specific or generic handlers provided for {}", req->to_string());
                evhttp_send_error(*req, HTTP_BADMETHOD, nullptr);
            });
        };

        if (!_iopts) {
            request_handlers.fill(default_handler);
            return;
        }

        for (uint8_t i = 0; i < request_handlers.size(); ++i) {
            auto m = METHOD{i};

            if (auto it = _iopts->handlers.find(m); it != _iopts->handlers.end()) {
                request_handlers[i] = [this, mcb = it->second](std::shared_ptr<http_request> req) mutable {
                    _ep.loop()->call([req, cb = mcb]() mutable { return cb(req); });
                };
            }
            else if (_iopts->generic_handler) {
                request_handlers[i] = [this,
                                       mcb = *_iopts->generic_handler](std::shared_ptr<http_request> req) mutable {
                    _ep.loop()->call([req, cb = mcb]() mutable { return cb(req); });
                };
            }
            else {
                request_handlers[i] = default_handler;
            }
        }
    }

    int listener::recv_request(struct evhttp_request* req) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        if (!req)
            return -1;

        return 0;
    }

    void listener::handle_request(struct evhttp_request* req) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        if (!req)
            return;

        auto id = ++_next_request_id;
        auto [it, b] = _requests.try_emplace(id, nullptr);
        if (!b) {
            unlog::warn("Inbound request for id:{} already tracked; refusing duplicate", id);
            evhttp_send_error(req, HTTP_INTERNAL, nullptr);
            return;
        }

        it->second = http_request::construct_inbound(req, id);
        if (!it->second) {
            unlog::warn("Failed to construct inbound request (id:{})", id);
            _requests.erase(it);
            evhttp_send_error(req, HTTP_INTERNAL, nullptr);
            return;
        }

        auto& hreq = it->second;

        evhttp_request_set_on_complete_cb(
                req, listen_callbacks::req_complete_cb, new detail::request_complete_ctx{this, id});

        const auto method = hreq->method();

        // invoke handler
        request_handlers[std::to_underlying(method)](hreq);
    }

    int listener::request_error(struct evhttp_request* req, int error, const char* reason) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        auto remote = detail::get_request_address(req);
        unlog::warn("Received error (code:{}) for inbound (remote:{}): {}", error, remote, reason);

        return -1;
    }

    void listener::request_complete(request_id_t id) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        if (_requests.erase(id)) {
            _ep._completed_inbounds += 1;
            unlog::debug("Inbound request completed and released");
        }
        else {
            unlog::warn("Inbound request completion callback could not find request to release");
        }
    }

    void listener::ws_request(struct evhttp_request* req) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        auto remote = detail::get_request_address(req);

        auto [it, b] = _sessions.try_emplace(remote, nullptr);

        if (not b) {
            unlog::info("Inbound WS session from already exists from remote: {}", remote);
            return evhttp_send_error(req, HTTP_INTERNAL, nullptr);
        }

        try {
            it->second = _ep.template make_shared<ws_session_base>(*this, std::move(remote), req);
        } catch (const std::exception& e) {
            unlog::warn("Exception: {}", e.what());
            return evhttp_send_error(req, HTTP_INTERNAL, nullptr);
        }

        if (not it->second) {
            unlog::critical("Failed to make inbound WS session for remote: {}", it->first);
            return evhttp_send_error(req, HTTP_INTERNAL, nullptr);
        }

        unlog::debug("Successfully created inbound WS session for remote: {}", it->first);
    }

    bufferevent* listener::new_bev(bool /* ssl */) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        auto bev = bufferevent_openssl_socket_new(
                _ep.ev_base(), -1, new_ssl(), BUFFEREVENT_SSL_ACCEPTING, default_bev_flags);

        bufferevent_ssl_set_flags(bev, BUFFEREVENT_SSL_DIRTY_SHUTDOWN);
        return bev;
    }

    listener::~listener() {
        unlog::debug("Closing listener on port: {}", _local.port());
    }

    void listener::close_all() {
        assert(_ep.in_event_loop());
        unlog::info("listener (port:{}) closing all sessions...", _local.port());

        _requests.clear();
        _sessions.clear();
    }

    void listener::close() {
        unlog::warn("Evconnlistener error; signalling endpoint to close listener...");

        _ep.loop()->call_soon([wep = _ep.weak_from_this(), p = _local.port()]() mutable {
            if (auto ep = wep.lock())
                ep->close_listener(p);
            else
                unlog::warn("Endpoint closed before outbound session could be closed");
        });
    }

    void listener::close_ws(ip_address remote) {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        if (_sessions.erase(remote)) {
            unlog::info("Listener closed session to remote: {}", remote);
            _ep._completed_inbounds += 1;
        }
        else
            unlog::warn("Listener failed to find session (remote: {}) to close!", remote);
    }

    SSL* listener::new_ssl() {
        assert(_ep.in_event_loop());
        SSL* _ssl = SSL_new(_ep.inbound_ctx());

        if (!_ssl)
            throw std::runtime_error{"Failed to create SSL/TLS: {}"_format(detail::current_error())};

        unlog::trace("Created SSL/TLS...");
        return _ssl;
    }

}  //  namespace wshttp
