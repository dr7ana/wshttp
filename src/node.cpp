#include "node.hpp"

#include "context.hpp"
#include "endpoint.hpp"
#include "internal.hpp"
#include "session.hpp"

namespace wshttp
{
    node::~node()
    {
        log->debug("Closing outbound node to host: {}", _uri.host());
    }

    void node::close_session(std::string host)
    {
        assert(_ep.in_event_loop());
        _ep.call([&]() {
            if (_sessions.erase(host))
                log->info("Listener closed session to remote: {}", host);
            else
                log->warn("Listener failed to find session (remote: {}) to close!", host);
        });
    }

    void node::_init_internals()
    {
        assert(_ep.in_event_loop());

        if (_local.has_value())
        {
            _fd = socket(_local->is_ipv4() ? AF_INET : AF_INET6, SOCK_STREAM, 0);

            if (auto rv = evutil_make_socket_nonblocking(_fd); rv < 0)
                throw std::runtime_error{
                    "Failed to non-block outbound node socket: {}"_format(detail::current_error())};

            if (_fd < 0)
                throw std::runtime_error{
                    "Could not create socket for outbound node: {}"_format(detail::current_error())};

            if (_local->is_ipv4())
            {
                sockaddr_in _in4;
                _in4.sin_family = AF_INET;
                _in4.sin_addr = *_local;
                _in4.sin_port = enc::host_to_big(_local->port());

                if (auto rv = bind(_fd, reinterpret_cast<sockaddr*>(&_in4), sizeof(sockaddr)); rv < 0)
                    throw std::runtime_error{"Outbound node failed to bind socket: {}"_format(detail::current_error())};
            }
            else
            {
                sockaddr_in6 _in6;
                _in6.sin6_family = AF_INET6;
                _in6.sin6_addr = *_local;
                _in6.sin6_port = enc::host_to_big(_local->port());

                if (auto rv = bind(_fd, reinterpret_cast<sockaddr*>(&_in6), sizeof(sockaddr)); rv < 0)
                    throw std::runtime_error{"Outbound node failed to bind socket: {}"_format(detail::current_error())};
            }

            log->info("Outbound node successfully bound socket: {}", *_local);
        }
    }

    void node::handle_nd_opts(ip_address local)
    {
        log->trace("Outbound node configured to connect from local address: {}", local);
        _local = local;
    }

    void node::create_outbound_session()
    {
        assert(_ep.in_event_loop());
        log->debug("Creating outbound session (host: {})", _uri.host());
        _ep.call_get([&]() {
            auto [it, b] = _sessions.emplace(_uri.host(), nullptr);

            if (not b)
            {
                log->critical("Connection to host {} already exists! ...", _uri.host());
                _sessions.erase(it);
                return;
            }

            it->second = _ep.template make_shared<outbound_session>(*this, _fd, _local);

            if (not it->second)
            {
                log->critical("Failed to make outbound session to host: {}", it->first);
                _sessions.erase(it);
            }
        });
    }

    SSL* node::new_ssl()
    {
        assert(_ep.in_event_loop());
        return _ep.call_get([&]() {
            SSL* _ssl = SSL_new(_ep.outbound_ctx());

            if (!_ssl)
                throw std::runtime_error{"Failed to create SSL/TLS for inbound: {}"_format(detail::current_error())};

            return _ssl;
        });
    }

}  //  namespace wshttp
