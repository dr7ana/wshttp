#include "endpoint.hpp"

// #include "dns.hpp"
#include "internal.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    caller_id_t endpoint::next_client_id = 0;

    endpoint::~endpoint()
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

    void endpoint::test_parse_method(std::string url)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        return _loop->call_get([&]() {
            if (parser->read(std::move(url)))
            {
                log->critical("Parser successfully read input: {}", parser->href_sv());
                parser->print_aggregates();
            }
        });
    }

    void endpoint::close_listener(uint16_t p)
    {
        return _loop->call_get([&]() {
            if (_listeners.erase(p))
            {
                log->info("Endpoint closed listener on port: {}", p);
            }
            else
                log->warn("Endpoint failed to find listener (port: {}) to close!", p);
        });
    }

    void endpoint::shutdown_endpoint()
    {
        log->debug("{} called...", __PRETTY_FUNCTION__);

        std::promise<void> p;
        auto f = p.get_future();

        _loop->call([&]() {
            // clear all mappings here
            for (auto& [_, l] : _listeners)
                l->close_all();
            p.set_value();
        });

        f.get();
    }

    SSL_CTX* endpoint::inbound_ctx()
    {
        return _ctx->I();
    }

    SSL_CTX* endpoint::outbound_ctx()
    {
        return _ctx->O();
    }

    void endpoint::handle_ep_opt(std::shared_ptr<ssl_creds> c)
    {
        log->info("New endpoint configured with SSL credentials");
        _ctx = app_context::make(std::move(c));
    }
}  //  namespace wshttp
