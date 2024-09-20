#include "listener.hpp"

#include "endpoint.hpp"
#include "internal.hpp"
#include "session.hpp"

namespace wshttp
{
    void listen_callbacks::accept_cb(
        struct evconnlistener * /* evconn */,
        evutil_socket_t fd,
        struct sockaddr *addr,
        int /* addrlen */,
        void *user_arg)
    {
        auto &l = *static_cast<listener *>(user_arg);
        auto remote = ip_address{addr};
        log->info("Inbound connection established (remote: {})", remote);
        l.create_inbound_session(std::move(remote), fd);
    }

    void listen_callbacks::error_cb(struct evconnlistener * /* evconn */, void *user_arg)
    {
        auto &l = *static_cast<listener *>(user_arg);
        log->warn("Evconnlistener error; closing listener...");
        return l._ep.close_listener(l._port);
    }

    listener::~listener()
    {
        log->debug("Closing listener on port: {}", _port);
        close_all();
    }

    void listener::handle_lst_opt(std::shared_ptr<ssl_creds> c)
    {
        log->debug("Listener creating I/O context...");
        io_ctx = app_context::make_pair(c);
    }

    void listener::create_inbound_session(ip_address remote, evutil_socket_t fd)
    {
        assert(_ep.in_event_loop());
        _ep.call_get([&]() {
            auto [it, b] = _sessions.emplace(remote, nullptr);

            if (not b)
            {
                log->critical("Connection from {} already exists! Rejecting new inbound...", remote);
                _sessions.erase(it);
                return;
            }

            it->second = _ep.template make_shared<session>(IO::INBOUND, *this, std::move(remote), fd);

            if (not it->second)
            {
                log->critical("Failed to make inbound session for remote: {}", it->first);
                _sessions.erase(it);
            }
        });
    }

    void listener::close_all()
    {
        assert(_ep.in_event_loop());
        _ep.call([&]() {
            log->info("listener (port:{}) closing all sessions...", _port);
            _sessions.clear();
        });
    }

    void listener::close_session(ip_address remote)
    {
        assert(_ep.in_event_loop());
        _ep.call([&]() {
            if (_sessions.erase(remote))
                log->info("Listener closed session to remote: {}", remote);
            else
                log->warn("Listener failed to find session (remote: {}) to close!", remote);
        });
    }

    void listener::_init_internals()
    {
        assert(_ep.in_event_loop());
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = enc::host_to_big(_port);

        _tcp = _ep.template shared_ptr<struct evconnlistener>(
            evconnlistener_new_bind(
                _ep._loop->loop().get(),
                listen_callbacks::accept_cb,
                this,
                LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE,
                -1,
                reinterpret_cast<sockaddr *>(&addr),
                sizeof(sockaddr)),
            deleters::_evconnlistener{});

        if (not _tcp)
        {
            auto err = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
            throw std::runtime_error{"TCP listener construction is fucked: {}"_format(err)};
        }

        evconnlistener_set_error_cb(_tcp.get(), listen_callbacks::error_cb);

        _fd = evconnlistener_get_fd(_tcp.get());
        log->debug("TCP listener has fd: {}", _fd);

        sockaddr _laddr{};
        socklen_t len;

        if (getsockname(_fd, &_laddr, &len) < 0)
            throw std::runtime_error{"Failed to get local socket address for tcp listener on port {}: {}"_format(
                _port, detail::current_error())};

        _local = ip_address{&_laddr};
        log->info("TCP listener has local bind: {}", _local);
    }

    SSL *listener::new_ssl(IO dir)
    {
        assert(_ep.in_event_loop());
        return _ep.call_get([&]() {
            SSL *_ssl;
            auto is_inbound = dir == IO::INBOUND;

            auto &ctx = is_inbound ? io_ctx.first : io_ctx.second;
            _ssl = SSL_new(ctx->_ctx.get());

            if (!_ssl)
                throw std::runtime_error{"Failed to create SSL/TLS for {}bound: {}"_format(
                    is_inbound ? "in" : "out", detail::current_error())};

            log->trace("Created SSL/TLS for {}bound...", is_inbound ? "in" : "out");

            return _ssl;
        });
    }

}  //  namespace wshttp
