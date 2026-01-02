#include "endpoint.hpp"

// #include "dns.hpp"
#include "internal.hpp"
#include "request.hpp"

namespace wshttp {
    using namespace unlog::literals;

    caller_id_t endpoint::next_caller_id = 0;

    void endpoint::_init_internals() {
        _stats = _loop->call_every(10s, [this]() { _print_stats(); });
        assert(_stats);

        if (_stats)
            unlog::debug("Endpoint stats ticker started!");
        else
            unlog::warn("Endpoint failed to start stats ticker!");

        unlog::trace("Client endpoint created with initialized event loop!");
    }

    bool endpoint::_listen(ip_address addr, std::optional<inbound_opts> opts) {
        (void)opts;
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

    std::shared_ptr<outbound_session> endpoint::initiate_session(std::string_view u, std::optional<session_opts> opts) {
        return _loop->call_get([&]() -> std::shared_ptr<outbound_session> {
            auto _uri = uri::make(u);
            if (not _uri) {
                unlog::warn("Outbound session must be initiated to a valid remote host (given: {})", u);
                return nullptr;
            }

            auto [it, b] = _outbound_sessions.try_emplace(_uri->host_domain(), nullptr);

            if (b) {
                unlog::error("Constructing outbound session to new remote domain: {}", _uri->hview());
                it->second = make_shared<outbound_session>(*this, std::move(_uri), std::move(opts));
            }
            else {
                unlog::error("Outbound session already exists for remote domain: {}!", _uri->hview());
                return nullptr;
            }

            return it->second;
        });
    }

    bool endpoint::_request(std::string_view u, METHOD method, std::optional<session_opts> opts) {
        // TODO: make a :call(...)
        return _loop->call_get([&]() {
            auto new_session = initiate_session(u, opts);

            if (!new_session) {
                unlog::warn("Failed to construct outbound session");
                return false;
            }

            new_session->request(method);
            return true;
        });
    }

    void endpoint::_print_stats() {
        auto n_listeners = _listeners.size();
        auto n_remotes = _outbound_sessions.size();
        auto n_inbounds = _completed_inbounds.load();
        auto n_outbounds = _completed_outbounds.load();

        unlog::info(
                "Endpoint:[ listeners:{} | remote domains:{} | completed:[ inbound:{} | outbound:{} ] ]",
                n_listeners,
                n_remotes,
                n_inbounds,
                n_outbounds);
    }

    endpoint::~endpoint() {
        unlog::debug("Shutting down client...");

        if (not _close_immediately)
            shutdown_endpoint();

        _listeners.clear();
        _outbound_sessions.clear();

        // clear all mappings here
        if (_loop.use_count() == 1)
            _loop->stop_thread(_close_immediately);

        _loop->stop_tickers(caller_id);

        unlog::info("Client shutdown complete!");
    }

    void endpoint::close_listener(ip_address b) {
        assert(in_event_loop());
        unlog::trace("{} called", __PRETTY_FUNCTION__);
        if (_listeners.erase(b))
            unlog::info("Endpoint closed listener on bind: {}", b);
        else
            unlog::warn("Endpoint failed to find listener (bind: {}) to close!", b);
    }

    void endpoint::close_outbound(const domain_host& remote) {
        assert(in_event_loop());

        if (auto it = _outbound_sessions.find(remote); it != _outbound_sessions.end()) {
            _outbound_sessions.erase(it);
            ++_completed_outbounds;
            unlog::info("Endpoint closed outbound session to remote: {}", remote.host());
        }
        else
            unlog::warn("Endpoint failed to find any outbound sessions to remote: {}", remote.host());
    }

    void endpoint::shutdown_endpoint() {
        unlog::debug("{} called...", __PRETTY_FUNCTION__);

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

    SSL_CTX* endpoint::inbound_ctx() {
        return _ctx->I();
    }

    SSL_CTX* endpoint::outbound_ctx() {
        return _ctx->O();
    }

    struct evhttp* endpoint::make_evhttp() {
        evhttp* e = evhttp_new(_loop->ev_loop.get());

        if (!e)
            throw std::runtime_error{"Failed to create evhttp event base!"};

        unlog::trace("Created evhttp event base...");
        return e;
    }

    void endpoint::handle_ep_opt(std::shared_ptr<ssl_creds> c) {
        unlog::info("New endpoint configured with SSL credentials");
        _ctx = app_context::make(std::move(c));
    }
}  //  namespace wshttp
