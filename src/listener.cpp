#include "listener.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

namespace wshttp
{
    void listen_callbacks::event_cb(struct bufferevent * /* bev */, short /* events */, void * /* user_arg*/)
    {
        //
    }

    void listen_callbacks::accept_cb(
        struct evconnlistener * /* evconn */,
        evutil_socket_t fd,
        struct sockaddr *addr,
        int /* addrlen */,
        void *user_arg)
    {
        ip_address remote{addr};
        sockaddr _local{};
        socklen_t len;

        if (getsockname(fd, &_local, &len) < 0)
            throw std::runtime_error{"Failed to get local socket address for incoming (remote: {})"_format(remote)};

        ip_address local{&_local};

        auto &listener = *static_cast<Listener *>(user_arg);
        log->info("Inbound connection established!");

        listener.create_inbound_session(std::move(local), std::move(remote));
    }

    void Listener::create_inbound_session(ip_address local, ip_address remote)
    {
        (void)local;
        (void)remote;
        return _ep.call_get([]() {});
    }

    void Listener::_init_internals()
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
            evconnlistener_deleter);

        if (not _tcp)
        {
            auto err = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
            throw std::runtime_error{"TCP listener construction is fucked: {}"_format(err)};
        }
    }
}  //  namespace wshttp
