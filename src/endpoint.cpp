#include "endpoint.hpp"

#include "dns.hpp"
#include "format.hpp"

namespace wshttp
{
    caller_id_t Endpoint::next_client_id = 0;

    std::unique_ptr<Endpoint> Endpoint::make()
    {
        return std::unique_ptr<Endpoint>{new Endpoint{}};
    }

    std::unique_ptr<Endpoint> Endpoint::make(std::shared_ptr<Loop> ev_loop)
    {
        return std::unique_ptr<Endpoint>{new Endpoint{std::move(ev_loop)}};
    }

    Endpoint::Endpoint() : _loop{Loop::make()}, _dns{dns::Server::make(*this)}, client_id{++next_client_id}
    {
        log->trace("Creating client with new event loop!");
        _dns->initialize();
    }

    Endpoint::Endpoint(std::shared_ptr<Loop> ev_loop)
        : _loop{std::move(ev_loop)}, _dns{dns::Server::make(*this)}, client_id{++next_client_id}
    {
        log->trace("Creating client with pre-existing event loop!");
        _dns->initialize();
    }

    Endpoint::~Endpoint()
    {
        log->debug("Shutting down client...");

        if (not _close_immediately)
            shutdown_endpoint();

        // clear all mappings here
        if (_loop.use_count() == 1)
            _loop->stop_thread(_close_immediately);

        _loop->stop_tickers(client_id);

        log->info("Client shutdown complete!");
    }

    std::unique_ptr<Endpoint> Endpoint::create_linked_client()
    {
        return Endpoint::make(_loop);
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
