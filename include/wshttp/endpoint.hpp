#pragma once

#include "address.hpp"
#include "dns.hpp"
#include "loop.hpp"
#include "session.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    struct ssl_creds;
    class listener;

    namespace dns
    {
        class server;
    }

    class endpoint final : public std::enable_shared_from_this<endpoint>
    {
        template <typename... Opt>
            requires detail::require_one_of_is<std::shared_ptr<ssl_creds>, Opt...>
        explicit endpoint(std::shared_ptr<event_loop> ev_loop, Opt&&... opts) :
                _loop{std::move(ev_loop)},
                _dns{_loop->template make_shared<dns::server>(*this)},
                caller_id{++next_caller_id}
        {
            if constexpr (sizeof...(opts))
                handle_ep_opt(std::forward<Opt>(opts)...);

            _init_internals();
        }

        template <typename... Opt>
            requires detail::require_one_of_is<std::shared_ptr<ssl_creds>, Opt...>
        explicit endpoint(Opt&&... opts) :
                _loop{event_loop::make()},
                _dns{_loop->template make_shared<dns::server>(*this)},
                caller_id{++next_caller_id}
        {
            if constexpr (sizeof...(opts))
                handle_ep_opt(std::forward<Opt>(opts)...);

            _init_internals();
        }

      public:
        endpoint& operator=(endpoint) = delete;
        endpoint& operator=(endpoint&&) = delete;

        template <typename... Opt>
        [[nodiscard]] static std::shared_ptr<endpoint> make(Opt&&... args)
        {
            return std::shared_ptr<endpoint>(new endpoint{std::forward<Opt>(args)...});
        }

        template <typename... Opt>
        [[nodiscard]] static std::shared_ptr<endpoint> make(std::shared_ptr<event_loop> ev_loop, Opt&&... args)
        {
            return std::shared_ptr<endpoint>(new endpoint{std::move(ev_loop), std::forward<Opt>(args)...});
        }

        ~endpoint();

      private:
        std::shared_ptr<event_loop> _loop;
        std::shared_ptr<dns::server> _dns;
        std::shared_ptr<app_context> _ctx;

        std::shared_ptr<ev_watcher> _stats;

        const caller_id_t caller_id;
        static caller_id_t next_caller_id;

        // local listeners managing inbound https connections
        std::unordered_map<ip_address, std::shared_ptr<listener>> _listeners{};

        // sessions managing outbound https connections
        std::unordered_map<domain_host, std::shared_ptr<outbound_session>> _outbound_sessions{};

        // debug counters
        std::atomic<uint64_t> _completed_inbounds{};
        std::atomic<uint64_t> _completed_outbounds{};

        std::atomic<bool> _close_immediately{false};

        void _init_internals();

        void _print_stats();

        bool _listen(ip_address addr, std::optional<inbound_opts> opts = std::nullopt);

        bool _request(std::string_view uri, METHOD method, std::optional<session_opts> opts = std::nullopt);

        // bool _request(std::string_view uri, METHOD method);

      public:
        bool listen(ip_v ip, uint16_t port, std::optional<inbound_opts> opts = std::nullopt)
        {
            return _listen(ip_address{ip, port}, std::move(opts));
        }

        bool listen(uint16_t port, std::optional<inbound_opts> opts = std::nullopt)
        {
            return _listen(ip_address{port}, std::move(opts));
        }

        // template <supported_method M, typename... Arg>
        // bool request(std::string_view uri, M method, Arg... args)
        // {
        //     if constexpr (sizeof...(args))
        //         ((void)handle_ep_opt(std::forward<Arg>(args)), ...);

        //     return _request(uri, method);
        // }

        std::shared_ptr<outbound_session> initiate_session(
                std::string_view uri, std::optional<session_opts> opts = std::nullopt);

        bool request(std::string_view uri, METHOD method, std::optional<session_opts> opts = std::nullopt)
        {
            return _request(uri, method, std::move(opts));
        }

        const std::shared_ptr<event_loop>& loop() { return _loop; }

        void set_shutdown_immediate(bool b = true) { _close_immediately = b; }

        bool in_event_loop() const { return _loop->in_event_loop(); }

      protected:
        template <typename T, typename Callable>
        std::shared_ptr<T> shared_ptr(T* obj, Callable&& deleter)
        {
            return _loop->template shared_ptr<T>(obj, std::forward<Callable>(deleter));
        }

        template <typename T, typename... Args>
        std::shared_ptr<T> make_shared(Args&&... args)
        {
            return _loop->template make_shared<T>(std::forward<Args>(args)...);
        }

        void close_listener(ip_address b);

        void close_listener(uint16_t p) { return close_listener(ip_address{p}); }

        void close_outbound(const domain_host& remote);

        void shutdown_endpoint();

        SSL_CTX* inbound_ctx();

        SSL_CTX* outbound_ctx();

        struct event_base* ev_base() { return loop()->loop().get(); }

        struct evdns_base* dns_base() { return *_dns; }

        struct evhttp* make_evhttp();

      private:
        void handle_ep_opt(std::shared_ptr<ssl_creds> c);

        template <typename... Opt>
        static constexpr void require_ssl_creds()
        {
            static_assert(
                    (0 + ... + std::is_same_v<std::remove_cvref_t<Opt>, std::shared_ptr<ssl_creds>>) == 1,
                    "Endpoint construction requires exactly one std::shared_ptr<ssl_creds> "
                    "argument");
        }

        friend class outbound_session;
        friend struct ws_session_base;
        friend class listener;
        friend class stream;
        friend class event_loop;
        friend class dns::server;
    };
}  //  namespace wshttp
