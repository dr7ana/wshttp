#include "session.hpp"

#include "endpoint.hpp"
#include "internal.hpp"
#include "stream.hpp"

extern "C"
{
#include <event2/bufferevent_ssl.h>
}

namespace wshttp
{
    std::shared_ptr<session> session::make(listener& l, ip_address remote, evutil_socket_t fd)
    {
        return l._ep.template make_shared<session>(l, std::move(remote), fd);
    }

    session::session(listener& l, ip_address remote, evutil_socket_t fd) : _ep{l._ep}, _remote{std::move(remote)}
    {
        _ssl.reset(SSL_new(l._ctx->ctx().get()));

        if (not _ssl)
            throw std::runtime_error{
                "Failed to create SSL/TLS for inbound: {}"_format(ERR_error_string(ERR_get_error(), NULL))};

        _bev.reset(bufferevent_openssl_socket_new(
            _ep._loop->loop().get(),
            fd,
            _ssl.get(),
            BUFFEREVENT_SSL_ACCEPTING,
            BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_THREADSAFE));

        if (not _bev)
            throw std::runtime_error{"Failed to create bufferevent socket for inbound TLS session: {}"_format(
                ERR_error_string(ERR_get_error(), NULL))};

        bufferevent_enable(_bev.get(), EV_READ | EV_WRITE);

        sockaddr _laddr{};
        socklen_t len;

        if (getsockname(fd, &_laddr, &len) < 0)
            throw std::runtime_error{"Failed to get local socket address for incoming (remote: {})"_format(remote)};

        _local = ip_address{&_laddr};
    }
}  //  namespace wshttp
