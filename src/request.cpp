#include "request.hpp"

#include "internal.hpp"

namespace wshttp
{
    inbound_request::inbound_request(listener& l, ip_address remote, evhttp_connection* c) :
            _l{l}, _bev{evhttp_connection_get_bufferevent(c)}, _path{ip_address{}, std::move(remote)}
    {
        _fd = bufferevent_getfd(_bev.get());

        log->debug("Inbound request has fd: {}", _fd);

        sockaddr _laddr{};
        socklen_t len;

        if (getsockname(_fd, &_laddr, &len) < 0)
            throw std::runtime_error{"Failed to get local socket address for incoming (remote: {}): {}, {}"_format(
                    _path.remote(), detail::current_error(), errno)};

        _path._local = ip_address{&_laddr};

        log->info("Successfully configured inbound request; path: {}", _path);
    }
}  // namespace wshttp
