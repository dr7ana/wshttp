#include "listener.hpp"

#include "endpoint.hpp"
#include "internal.hpp"
#include "ws.hpp"

namespace wshttp
{
    namespace detail
    {
        static listener* _get_listener(void* user_arg)
        {
            return static_cast<listener*>(user_arg);
        }
    }  // namespace detail

    void listen_callbacks::gen_cb(struct evhttp_request* req, void* user_arg)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->handle_request(req);
    }

    bufferevent* listen_callbacks::bev_cb(struct event_base* /* ev */, void* user_arg)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->new_bev();
    }

    int listen_callbacks::newreq_cb(struct evhttp_request* req, void* user_arg)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->recv_request(req);
    }

    void listen_callbacks::ws_cb(struct evhttp_request* req, void* user_arg)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->ws_request(req);
    }

    void listen_callbacks::close_cb(struct evhttp_connection* conn, void* user_arg)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->close_request(detail::get_connection_address(conn));
    }

    int listen_callbacks::error_cb(
            struct evhttp_request* req, struct evbuffer* /* buffer */, int error, const char* reason, void* user_arg)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);
        return detail::_get_listener(user_arg)->request_error(req, error, reason);
    }

    listener::listener(endpoint& e, ip_address bind) : _ep{e}, _local{std::move(bind)}
    {
        _init_internals();
    }

    void listener::_init_internals()
    {
        assert(_ep.in_event_loop());

        sockaddr saddr{};

        if (_local.is_ipv4())
        {
            auto* in = reinterpret_cast<sockaddr_in*>(&saddr);
            in->sin_family = AF_INET;
            in->sin_addr = _local;  // operator in_addr()
            in->sin_port = enc::host_to_big(_local.port());
        }
        else
        {
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

        if (not _tcp)
            throw std::runtime_error{"TCP listener construction is fucked: {}"_format(
                    evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()))};

        _evh.reset(_ep.make_evhttp());
        evhttp_set_gencb(_evh.get(), listen_callbacks::gen_cb, this);
        evhttp_set_cb(_evh.get(), "/ws", listen_callbacks::ws_cb, this);
        evhttp_set_bevcb(_evh.get(), listen_callbacks::bev_cb, this);
        evhttp_set_newreqcb(_evh.get(), listen_callbacks::newreq_cb, this);
        evhttp_set_errorcb(_evh.get(), listen_callbacks::error_cb, this);
        evhttp_set_allowed_methods(_evh.get(), default_evhttp_flags);

        evhttp_bound_socket* handle = evhttp_bind_listener(_evh.get(), _tcp.get());

        if (!handle)
            throw std::runtime_error{"Failed to bind evhttp listener to {}"_format(_local)};

        _fd = evhttp_bound_socket_get_fd(handle);
        log->debug("evhttp listener has fd: {}", _fd);

        _local = ip_address::from_socket(_fd);

        log->info("evhttp listener deployed on local bind: {}", _local);
    }

    int listener::recv_request(struct evhttp_request* req)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        auto remote = detail::get_request_address(req);

        auto [it, b] = _requests.try_emplace(remote, nullptr);

        if (not b)
        {
            log->info("Closing inbound request from remote: {}", remote);
            return -1;
        }

        try
        {
            it->second = _ep.template make_shared<inbound_session>(
                    /* *this,  */ std::move(remote), detail::get_request_fd(req));
        }
        catch (const std::exception& e)
        {
            log->warn("Exception: {}", e.what());
            return -1;
        }

        if (not it->second)
        {
            log->critical("Failed to make inbound request for remote: {}", it->first);
            return -1;
        }

        evhttp_connection_set_closecb(evhttp_request_get_connection(req), listen_callbacks::close_cb, this);

        log->debug("Successfully created inbound request for remote: {}", it->first);

        return 0;
    }

    void listener::handle_request(struct evhttp_request* req)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        auto remote = detail::get_request_address(req);

        if (auto it = _requests.find(remote); it != _requests.end())
            return it->second->recv_request(req);

        log->warn(
                "Received HTTP request (type:{}) from unknown remote: {}",
                detail::get_method_string(detail::get_request_method(req)),
                remote);
        evhttp_send_error(req, HTTP_FORBIDDEN, nullptr);
    }

    int listener::request_error(struct evhttp_request* req, int error, const char* reason)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        auto remote = detail::get_request_address(req);
        log->warn("Received error (code:{}) for inbound (remote:{}): {}", error, remote, reason);

        return -1;
    }

    void listener::ws_request(struct evhttp_request* req)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        auto remote = detail::get_request_address(req);

        auto [it, b] = _sessions.try_emplace(remote, nullptr);

        if (not b)
        {
            log->info("Inbound WS session from already exists from remote: {}", remote);
            return evhttp_send_error(req, HTTP_INTERNAL, nullptr);
        }

        try
        {
            it->second = _ep.template make_shared<ws_session_base>(*this, std::move(remote), req);
        }
        catch (const std::exception& e)
        {
            log->warn("Exception: {}", e.what());
            return evhttp_send_error(req, HTTP_INTERNAL, nullptr);
        }

        if (not it->second)
        {
            log->critical("Failed to make inbound WS session for remote: {}", it->first);
            return evhttp_send_error(req, HTTP_INTERNAL, nullptr);
        }

        log->debug("Successfully created inbound WS session for remote: {}", it->first);
    }

    bufferevent* listener::new_bev(bool /* ssl */)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        auto bev = bufferevent_openssl_socket_new(
                _ep.ev_base(), -1, new_ssl(), BUFFEREVENT_SSL_ACCEPTING, default_bev_flags);

        bufferevent_ssl_set_flags(bev, BUFFEREVENT_SSL_DIRTY_SHUTDOWN);
        return bev;
    }

    listener::~listener()
    {
        log->debug("Closing listener on port: {}", _local.port());
    }

    void listener::close_all()
    {
        assert(_ep.in_event_loop());
        log->info("listener (port:{}) closing all sessions...", _local.port());

        _requests.clear();
        _sessions.clear();
    }

    void listener::close()
    {
        log->warn("Evconnlistener error; signalling endpoint to close listener...");

        _ep.loop()->call_soon([wep = _ep.weak_from_this(), p = _local.port()]() mutable {
            if (auto ep = wep.lock())
                ep->close_listener(p);
            else
                log->warn("Endpoint closed before outbound session could be closed");
        });
    }

    void listener::close_request(ip_address remote)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        if (_requests.erase(remote))
        {
            log->info("Listener closed session to remote: {}", remote);
            _ep._completed_inbounds += 1;
        }
        else
            log->warn("Listener failed to find session (remote: {}) to close!", remote);
    }

    void listener::close_ws(ip_address remote)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        if (_sessions.erase(remote))
        {
            log->info("Listener closed session to remote: {}", remote);
            _ep._completed_inbounds += 1;
        }
        else
            log->warn("Listener failed to find session (remote: {}) to close!", remote);
    }

    SSL* listener::new_ssl()
    {
        assert(_ep.in_event_loop());
        SSL* _ssl = SSL_new(_ep.inbound_ctx());

        if (!_ssl)
            throw std::runtime_error{"Failed to create SSL/TLS: {}"_format(detail::current_error())};

        log->trace("Created SSL/TLS...");
        return _ssl;
    }

}  //  namespace wshttp
