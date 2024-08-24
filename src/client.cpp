#include "client.hpp"

#include "format.hpp"

namespace wshttp
{
    caller_id_t Client::next_client_id = 0;

    std::unique_ptr<Client> Client::make()
    {
        return std::unique_ptr<Client>{new Client{}};
    }

    std::unique_ptr<Client> Client::make(std::shared_ptr<Loop> ev_loop)
    {
        return std::unique_ptr<Client>{new Client{std::move(ev_loop)}};
    }

    Client::Client() : _loop{Loop::make()}, client_id{++next_client_id}
    {
        log->trace("Creating client with new event loop!");
    }

    Client::Client(std::shared_ptr<Loop> ev_loop) : _loop{std::move(ev_loop)}, client_id{++next_client_id}
    {
        log->trace("Creating client with pre-existing event loop!");
    }

    Client::~Client()
    {
        log->debug("Shutting down client...");

        if (not _close_immediately)
            teardown_client();

        // clear all mappings here
        if (_loop.use_count() == 1)
            _loop->stop_thread(_close_immediately);

        _loop->stop_tickers(client_id);

        log->info("Client shutdown complete!");
    }

    std::unique_ptr<Client> Client::create_linked_client()
    {
        return Client::make(_loop);
    }

    void Client::teardown_client()
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
