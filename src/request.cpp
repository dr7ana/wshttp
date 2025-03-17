#include "request.hpp"

#include "internal.hpp"

namespace wshttp
{
    inbound_request::inbound_request(listener& l, ip_address remote, evutil_socket_t sock) :
            _l{l}, _path{ip_address{}, std::move(remote)}, _fd{sock}
    {
        log->debug("Inbound request has fd: {}", _fd);

        int val = 1;
        if (setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0)
            throw std::runtime_error{
                    "Failed to set TCP_NODELAY on inbound request socket: {}"_format(detail::current_error())};

        _path._local = ip_address::from_socket(_fd);
        log->info("Successfully configured inbound request; path: {}", _path);
    }

    // int inbound_request::recv_initial(struct evhttp_request* req)
    // {
    //     log->trace("{} called", __PRETTY_FUNCTION__);

    //     auto* evconn = evhttp_request_get_connection(req);
    //     _bev.reset(evhttp_connection_get_bufferevent(evconn));

    //     if (not _bev)
    //     {
    //         log->warn("Failed to query bufferevent from inbound HTTP request from {}!", _path.remote());
    //         _l.close_session(_path.remote());
    //         return -1;
    //     }

    //     _uri = uri::populate(req);

    //     if (not _uri)
    //     {
    //         log->warn("Failed to parse URI for inbound HTTP request from {}!", _path.remote());
    //         _l.close_session(_path.remote());
    //         return -1;
    //     }

    //     return 0;
    // }
}  // namespace wshttp
