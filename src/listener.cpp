#include "listener.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

namespace wshttp
{
    namespace detail
    {
        static listener* _get_listener(void* user_arg)
        {
            return static_cast<listener*>(user_arg);
        }
    }  // namespace detail

    void listen_callbacks::accept_cb(
            struct evconnlistener* /* evconn */,
            evutil_socket_t fd,
            struct sockaddr* addr,
            int /* addrlen */,
            void* user_arg)
    {
        return detail::_get_listener(user_arg)->create_session(ip_address{addr}, fd);
    }

    void listen_callbacks::gen_cb(struct evhttp_request* req, void* user_arg)
    {
        (void)req;
        (void)user_arg;
    }

    void listen_callbacks::ws_cb(struct evhttp_request* req, void* user_arg)
    {
        (void)req;
        (void)user_arg;
    }

    bufferevent* listen_callbacks::bev_cb(struct event_base* /* ev */, void* user_arg)
    {
        return detail::_get_listener(user_arg)->new_bev();
    }

    int listen_callbacks::newreq_cb(struct evhttp_request* req, void* user_arg)
    {
        return detail::_get_listener(user_arg)->accept_request(req);
    }

    void listen_callbacks::error_cb(struct evconnlistener* /* evconn */, void* user_arg)
    {
        return detail::_get_listener(user_arg)->close_listener();
    }

    static constexpr auto default_ipv4_anyaddr = "0.0.0.0"sv;

    listener::listener(endpoint& e, uint16_t p) : _ep{e}, _local{ipv4_anyaddr, p}
    {
        assert(_ep.in_event_loop());

        _evh.reset(_ep.make_evhttp());
        evhttp_set_gencb(_evh.get(), listen_callbacks::gen_cb, this);
        evhttp_set_cb(_evh.get(), "/ws", listen_callbacks::ws_cb, this);
        evhttp_set_bevcb(_evh.get(), listen_callbacks::bev_cb, this);

        evhttp_bound_socket* handle = evhttp_bind_socket_with_handle(_evh.get(), default_ipv4_anyaddr.data(), p);

        if (!handle)
            throw std::runtime_error{"Failed to bind evhttp socket to {}:{}"_format(default_ipv4_anyaddr, p)};

        _fd = evhttp_bound_socket_get_fd(handle);
        log->debug("evhttp listener has fd: {}", _fd);

        sockaddr _laddr{};
        socklen_t len;

        if (getsockname(_fd, &_laddr, &len) < 0)
            throw std::runtime_error{"Failed to get local socket address for evhttp listener on port {}: {}"_format(
                    p, detail::current_error())};

        _local = ip_address{&_laddr};

        log->info("evhttp listener deployed on local bind: {}", _local);
    }

    int listener::accept_request(struct evhttp_request* req)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        evhttp_connection* evconn = evhttp_request_get_connection(req);
        ip_address remote{evhttp_connection_get_addr(evconn)};

        auto [it, b] = _sessions.try_emplace(remote, nullptr);

        if (not b)
        {
            log->critical("Connection from {} already exists! Rejecting new inbound...", remote);
            return -1;
        }

        it->second = _ep.template make_shared<inbound_request>(*this, std::move(remote), evconn);

        if (not it->second)
        {
            log->critical("Failed to make inbound request for remote: {}", it->first);
            _sessions.erase(it);
            return -1;
        }

        return 0;
    }

    bufferevent* listener::new_bev()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        return bufferevent_openssl_socket_new(
                _ep.ev_base(),
                -1,
                new_ssl(),
                BUFFEREVENT_SSL_ACCEPTING,
                BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_THREADSAFE);
    }

    // void listener::_init_internals()
    // {
    //     assert(_ep.in_event_loop());

    //     _evh.reset(_ep.make_evhttp());
    //     evhttp_set_gencb(_evh.get(), listen_callbacks::gen_cb, this);
    //     evhttp_set_cb(_evh.get(), "/ws", listen_callbacks::ws_cb, this);
    //     evhttp_set_bevcb(_evh.get(), listen_callbacks::bev_cb, this);

    //     sockaddr_in addr{};
    //     addr.sin_family = AF_INET;
    //     addr.sin_addr.s_addr = INADDR_ANY;
    //     addr.sin_port = enc::host_to_big(_local.port());

    //     _tcp = _ep.template shared_ptr<struct evconnlistener>(
    //             evconnlistener_new_bind(
    //                     _ep._loop->loop().get(),
    //                     listen_callbacks::accept_cb,
    //                     this,
    //                     LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE,
    //                     -1,
    //                     reinterpret_cast<sockaddr*>(&addr),
    //                     sizeof(sockaddr)),
    //             deleters::_evconnlistener{});

    //     if (not _tcp)
    //     {
    //         auto err = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
    //         throw std::runtime_error{"TCP listener construction failed: {}"_format(err)};
    //     }

    //     evconnlistener_set_error_cb(_tcp.get(), listen_callbacks::error_cb);

    //     evhttp_bound_socket* handle = evhttp_bind_listener(_evh.get(), _tcp.get());

    //     if (!handle)
    //         throw std::runtime_error{
    //                 "Failed to bind evhttp socket to {}:{}"_format(default_ipv4_anyaddr, _local.port())};

    //     _fd = evhttp_bound_socket_get_fd(handle);
    //     log->debug("evhttp listener has fd: {}", _fd);

    //     sockaddr _laddr{};
    //     socklen_t len;

    //     if (getsockname(_fd, &_laddr, &len) < 0)
    //         throw std::runtime_error{"Failed to get local socket address for tcp listener on port {}: {}"_format(
    //                 _local.port(), detail::current_error())};

    //     _local = ip_address{&_laddr};

    //     log->info("TCP listener deployed on local bind: {}", _local);
    // }

    listener::~listener()
    {
        log->debug("Closing listener on port: {}", _local.port());
    }

    void listener::create_session(ip_address remote, evutil_socket_t fd)
    {
        assert(_ep.in_event_loop());
        log->info("Inbound connection established (fd: {}, remote: {})", fd, remote);

        // auto [it, b] = _sessions.emplace(remote, nullptr);

        // if (not b)
        // {
        //     log->critical("Connection from {} already exists! Rejecting new inbound...", remote);
        //     _sessions.erase(it);
        //     return;
        // }

        // it->second = _ep.template make_shared<inbound_session>(*this, std::move(remote), fd);

        // if (not it->second)
        // {
        //     log->critical("Failed to make inbound session for remote: {}", it->first);
        //     _sessions.erase(it);
        // }
    }

    void listener::close_all()
    {
        assert(_ep.in_event_loop());
        _ep.loop()->call([&]() {
            log->info("listener (port:{}) closing all sessions...", _local.port());
            // _sessions.clear();
        });
    }

    void listener::close_listener()
    {
        log->warn("Evconnlistener error; signalling endpoint to close listener...");

        _ep.loop()->call_soon([wep = _ep.weak_from_this(), p = _local.port()]() mutable {
            if (auto ep = wep.lock())
                ep->close_listener(p);
            else
                log->warn("Endpoint closed before outbound session could be closed");
        });
    }

    void listener::close_session(ip_address /* remote */)
    {
        _ep.loop()->call_soon([&]() {
            // if (_sessions.erase(remote))
            //     log->info("Listener closed session to remote: {}", remote);
            // else
            //     log->warn("Listener failed to find session (remote: {}) to close!", remote);
        });
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
