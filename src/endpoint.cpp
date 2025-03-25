#include "endpoint.hpp"

// #include "dns.hpp"
#include "internal.hpp"
#include "request.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    caller_id_t endpoint::next_caller_id = 0;

    void endpoint::_init_internals()
    {
        _stats = _loop->call_every(10s, [this]() { _print_stats(); });
        assert(_stats);

        if (_stats)
            log->debug("Endpoint stats ticker started!");
        else
            log->warn("Endpoint failed to start stats ticker!");

        log->trace("Client endpoint created with initialized event loop!");
    }

    bool endpoint::_listen(ip_address addr)
    {
        return _loop->call_get([&]() {
            auto [itr, b] = _listeners.try_emplace(addr, nullptr);

            if (not b)
                throw std::invalid_argument{
                        "Cannot create tcp-listener at bind: {} -- listener already exists!"_format(addr)};

            itr->second = make_shared<listener>(*this, std::move(addr));

            if (not itr->second)
                throw std::runtime_error{"TCP listener construction failed!"};

            return true;
        });
    }

    bool endpoint::_request(std::string_view uri, METHOD method)
    {
        return _loop->call_get([&]() {
            auto _uri = uri::parse(uri);
            if (not _uri)
                throw std::invalid_argument{"Failed to parse input url: {}"_format(uri)};

            auto host = _uri.host_url();

            auto& outbound_set = _outbounds[host];

            if (outbound_set.contains(_uri))
            {
                log->warn("Outbound session already exists for uri: {}!", _uri);
                return false;
            }

            auto [itr, b] = outbound_set.emplace(make_shared<outbound_session>(*this, std::move(_uri)));

            if (not b)
                throw std::invalid_argument{"Outbound session already exists for uri: {}!"_format(uri)};

            (*itr)->make_request(method);

            return true;
        });
    }

    void endpoint::_print_stats()
    {
        auto n_listeners = _listeners.size();
        auto n_remotes = _outbounds.size();
        auto n_inbounds = _completed_inbounds.load();
        auto n_outbounds = _completed_outbounds.load();

        log->info(
                "Endpoint:[ listeners:{} | remote domains:{} | completed:[ inbound:{} | outbound:{} ] ]",
                n_listeners,
                n_remotes,
                n_inbounds,
                n_outbounds);
    }

    endpoint::~endpoint()
    {
        log->debug("Shutting down client...");

        if (not _close_immediately)
            shutdown_endpoint();

        _listeners.clear();

        // clear all mappings here
        if (_loop.use_count() == 1)
            _loop->stop_thread(_close_immediately);

        _loop->stop_tickers(caller_id);

        log->info("Client shutdown complete!");
    }

    void endpoint::test_parse_method(std::string url)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        if (auto u = uri::parse(url))
        {
            auto h = u.host_url();
            log->critical("Domain host ({}) parsed uri: {}", h, u);
        }

        evhttp_uri* evuri = evhttp_uri_parse(url.c_str());

        std::string_view scheme, host, path;

        scheme = evhttp_uri_get_scheme(evuri);
        host = evhttp_uri_get_host(evuri);
        int port = evhttp_uri_get_port(evuri);
        path = evhttp_uri_get_path(evuri);

        if (port == -1)
        {
            port = (scheme == "https") ? 443 : 80;
            log->debug("setting uri port to {}", port);
            evhttp_uri_set_port(evuri, port);
        }

        auto msg = "\n\tscheme:{}\n\thost:{}\n\tport:{}\n\tpath:{}"_format(scheme, host, port, path);

        log->critical("parsed evhttp uri data: {}", msg);
        evhttp_uri_free(evuri);
    }

    void endpoint::close_listener(ip_address b)
    {
        assert(in_event_loop());
        if (_listeners.erase(b))
        {
            log->info("Endpoint closed listener on bind: {}", b);
        }
        else
            log->warn("Endpoint failed to find listener (bind: {}) to close!", b);
    }

    void endpoint::close_outbound(uri u)
    {
        assert(in_event_loop());

        if (auto ita = _outbounds.find(u.host_url()); ita != _outbounds.end())
        {
            if (auto itb = ita->second.find(u); itb != ita->second.end())
            {
                ita->second.erase(itb);
                ++_completed_outbounds;

                log->info("Endpoint closed outbound request to remote domain (host: {}): {}", u.host(), u);

                if (ita->second.empty())
                {
                    log->debug("All outbounds completed for remote domain host: {}", u.host());
                    _outbounds.erase(ita);
                }
                return;
            }
            log->warn("Outbound set to remote domain (host: {}) already cleared finished request: {}", u.host(), u);
        }
        else
            log->warn("Endpoint failed to find any outbounds to remote domain (host: {}): {}", u.host(), u);
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

    struct evhttp* endpoint::make_evhttp()
    {
        evhttp* e = evhttp_new(_loop->ev_loop.get());

        if (!e)
            throw std::runtime_error{"Failed to create evhttp event base!"};

        log->trace("Created evhttp event base...");
        return e;
    }

    void endpoint::handle_ep_opt(std::shared_ptr<ssl_creds> c)
    {
        log->info("New endpoint configured with SSL credentials");
        _ctx = app_context::make(std::move(c));
    }
}  //  namespace wshttp
