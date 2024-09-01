#include "endpoint.hpp"

#include "context.hpp"
#include "dns.hpp"
#include "encoding.hpp"
#include "format.hpp"
#include "internal.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    caller_id_t Endpoint::next_client_id = 0;

    std::shared_ptr<Endpoint> Endpoint::make()
    {
        return std::shared_ptr<Endpoint>{new Endpoint{}};
    }

    std::shared_ptr<Endpoint> Endpoint::make(std::shared_ptr<Loop> ev_loop)
    {
        return std::shared_ptr<Endpoint>{new Endpoint{std::move(ev_loop)}};
    }

    Endpoint::Endpoint()
        : _loop{Loop::make()}, _dns{_loop->template make_shared<dns::Server>(*this)}, client_id{++next_client_id}
    {
        log->trace("Creating client with new event loop!");
        _dns->initialize();
    }

    Endpoint::Endpoint(std::shared_ptr<Loop> ev_loop)
        : _loop{std::move(ev_loop)}, _dns{_loop->template make_shared<dns::Server>(*this)}, client_id{++next_client_id}
    {
        log->trace("Creating client with pre-existing event loop!");
        _dns->initialize();
    }

    Endpoint::~Endpoint()
    {
        log->debug("Shutting down client...");

        if (not _close_immediately)
            shutdown_endpoint();

        _listeners.clear();

        // clear all mappings here
        if (_loop.use_count() == 1)
            _loop->stop_thread(_close_immediately);

        _loop->stop_tickers(client_id);

        log->info("Client shutdown complete!");
    }

    std::shared_ptr<Endpoint> Endpoint::create_linked_endpoint()
    {
        return Endpoint::make(_loop);
    }

    bool Endpoint::_listen(uint16_t port)
    {
        return _loop->call_get([this, port]() {
            auto [itr, b] = _listeners.try_emplace(port, nullptr);

            if (not b)
            {
                log->critical("Cannot create tcp-listener at port {} -- listener already exists!", port);
                return false;
            }

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = enc::host_to_big(port);

            itr->second = shared_ptr<struct evconnlistener>(
                evconnlistener_new_bind(
                    _loop->loop().get(),
                    nullptr,
                    nullptr,
                    LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE,
                    -1,
                    reinterpret_cast<sockaddr*>(&addr),
                    sizeof(sockaddr)),
                evconnlistener_deleter);

            if (not itr->second)
            {
                auto err = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
                throw std::runtime_error{"TCP listener construction is fucked: {}"_format(err)};
            }

            log->critical("Endpoint deployed tcp-listener on port {}", port);
            return true;
        });
    }

    void Endpoint::test_parse_method(std::string url)
    {
        return _loop->call_get([&]() mutable {
            if (parser->read(url))
            {
                log->critical("Parser successfully read input: {}", url);
                parser->print_aggregates();
            }
        });
    }

    void Endpoint::shutdown_endpoint()
    {
        log->debug("{} called...", __PRETTY_FUNCTION__);

        std::promise<void> p;
        auto f = p.get_future();

        _loop->call([&]() mutable {
            // clear all mappings here
            p.set_value();
        });

        f.get();
    }
}  //  namespace wshttp
