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
        log->info("Inbound connection established!");
        l.create_inbound_session(ip_address{addr}, fd);
    }

    void listen_callbacks::error_cb(struct evconnlistener * /* evconn */, void *user_arg)
    {
        auto &l = *static_cast<listener *>(user_arg);
        log->warn("evconnlistener error");
        l.close_all();
    }

    listener::~listener()
    {
        log->debug("Closing listener on port: {}", _port);
    }

    void listener::create_inbound_session(ip_address remote, evutil_socket_t fd)
    {
        return _ep.call_get([&]() {
            auto [it, b] = _sessions.emplace(remote, nullptr);

            if (not b)
            {
                log->critical("Connection from {} already exists! Rejecting new inbound...", remote);
                _sessions.erase(it);
                return;
            }

            it->second = session::make(*this, std::move(remote), fd);

            if (not it->second)
            {
                log->critical("Failed to make inbound session for remote: {}", it->first);
                _sessions.erase(it);
            }
        });
    }

    void listener::close_all()
    {
        return _ep.call_get([&]() {
            log->info("listener (port:{}) closing all sessions...", _port);
            _sessions.clear();
        });
    }

    void listener::_init_internals()
    {
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
            deleters::evconnlistener_d);

        if (not _tcp)
        {
            auto err = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
            throw std::runtime_error{"TCP listener construction is fucked: {}"_format(err)};
        }
        // evconnlistener_set_error_cb(_tcp.get(), evconnlistener_errorcb errorcb)
    }
}  //  namespace wshttp
