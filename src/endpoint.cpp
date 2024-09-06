#include "endpoint.hpp"

#include "dns.hpp"
#include "internal.hpp"
#include "parser.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    caller_id_t endpoint::next_client_id = 0;

    std::shared_ptr<endpoint> endpoint::make()
    {
        return std::shared_ptr<endpoint>{new endpoint{}};
    }

    std::shared_ptr<endpoint> endpoint::make(std::shared_ptr<event_loop> ev_loop)
    {
        return std::shared_ptr<endpoint>{new endpoint{std::move(ev_loop)}};
    }

    endpoint::endpoint()
        : _loop{event_loop::make()}, _dns{_loop->template make_shared<dns::server>(*this)}, client_id{++next_client_id}
    {
        log->trace("Creating client with new event loop!");
        _dns->initialize();
    }

    endpoint::endpoint(std::shared_ptr<event_loop> ev_loop)
        : _loop{std::move(ev_loop)}, _dns{_loop->template make_shared<dns::server>(*this)}, client_id{++next_client_id}
    {
        log->trace("Creating client with pre-existing event loop!");
        _dns->initialize();
    }

    endpoint::~endpoint()
    {
        log->debug("Shutting down client...");

        if (not _close_immediately)
            shutdown_node();

        _listeners.clear();

        // clear all mappings here
        if (_loop.use_count() == 1)
            _loop->stop_thread(_close_immediately);

        _loop->stop_tickers(client_id);

        log->info("Client shutdown complete!");
    }

    std::shared_ptr<endpoint> endpoint::create_linked_node()
    {
        return endpoint::make(_loop);
    }

    void endpoint::test_parse_method(std::string url)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        return _loop->call_get([&]() mutable {
            if (parser->read(std::move(url)))
            {
                log->critical("Parser successfully read input: {}", parser->href_sv());
                parser->print_aggregates();
            }
        });
    }

    void endpoint::shutdown_node()
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
