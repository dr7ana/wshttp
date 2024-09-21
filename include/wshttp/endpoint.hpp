#pragma once

#include "dns.hpp"
#include "format.hpp"
#include "listener.hpp"
#include "loop.hpp"
#include "node.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    struct ssl_creds;

    namespace dns
    {
        class server;
    }

    class endpoint final
    {
        friend class inbound_session;
        friend class outbound_session;
        friend class session_base;
        friend class node;
        friend class listener;
        friend class stream;
        friend class event_loop;
        friend class dns::server;

        template <typename... Opt>
        explicit endpoint(std::shared_ptr<event_loop> ev_loop, Opt&&... opts)
            : _loop{std::move(ev_loop)},
              _dns{_loop->template make_shared<dns::server>(*this)},
              client_id{++next_client_id}
        {
            require_ssl_creds<Opt...>();

            if constexpr (sizeof...(opts))
                handle_ep_opt(std::forward<Opt>(opts)...);

            _dns->initialize();
            log->trace("Client endpoint created with initialized event loop!");
        }

      public:
        endpoint& operator=(endpoint) = delete;
        endpoint& operator=(endpoint&&) = delete;

        template <typename... Opt>
        [[nodiscard]] static std::shared_ptr<endpoint> make(Opt&&... args)
        {
            return endpoint::make(event_loop::make(), std::forward<Opt>(args)...);
        }

        template <typename... Opt>
        [[nodiscard]] static std::shared_ptr<endpoint> make(std::shared_ptr<event_loop> ev_loop, Opt&&... args)
        {
            return ev_loop->template make_shared<endpoint>(std::move(ev_loop), std::forward<Opt>(args)...);
        }

        ~endpoint();

      private:
        std::shared_ptr<event_loop> _loop;

        std::shared_ptr<dns::server> _dns;

        std::shared_ptr<app_context> _ctx;

        const caller_id_t client_id;
        static caller_id_t next_client_id;

        // local listeners managing inbound https connections
        std::unordered_map<uint16_t, std::shared_ptr<listener>> _listeners;

        // local nodes managing outbound https connections
        std::unordered_map<std::string, std::shared_ptr<node>> _nodes;

        std::atomic<bool> _close_immediately{false};

      public:
        bool listen(uint16_t port)
        {
            return call_get([&]() {
                auto [itr, b] = _listeners.try_emplace(port, nullptr);

                if (not b)
                    throw std::invalid_argument{
                        "Cannot create tcp-listener at port {} -- listener already exists!"_format(port)};

                itr->second = make_shared<listener>(*this, port);

                if (not itr->second)
                    throw std::runtime_error{"TCP listener construction failed!"};

                return true;
            });
        }

        template <typename... Opt>
        bool connect(std::string_view url, Opt&&... opts)
        {
            return call_get([&]() {
                auto _uri = uri::parse(url);
                if (not _uri)
                    throw std::invalid_argument{"Failed to parse input url: {}"_format(url)};

                auto [itr, b] = _nodes.try_emplace(std::string{_uri.host()}, nullptr);

                if (not b)
                    throw std::invalid_argument{
                        "Cannot create outbound node for input: {} -- node already exists!"_format(url)};

                itr->second = make_shared<node>(*this, std::move(_uri), std::forward<Opt>(opts)...);

                if (not itr->second)
                    throw std::runtime_error{"Node construction is fucked"};

                return true;
            });
        }

        void test_parse_method(std::string url);

        template <typename Callable>
        void call(Callable&& f)
        {
            _loop->call(std::forward<Callable>(f));
        }

        template <typename Callable, typename Ret = decltype(std::declval<Callable>()())>
        Ret call_get(Callable&& f)
        {
            return _loop->call_get(std::forward<Callable>(f));
        }

        void call_soon(std::function<void(void)> f) { _loop->call_soon(std::move(f)); }

        template <typename Callable>
        [[nodiscard]] std::shared_ptr<ev_watcher> call_every(std::chrono::microseconds interval, Callable&& f)
        {
            return _loop->_call_every(interval, std::forward<Callable>(f), client_id);
        }

        template <typename Callable>
        void call_later(std::chrono::microseconds delay, Callable&& hook)
        {
            _loop->call_later(delay, std::forward<Callable>(hook));
        }

        void set_shutdown_immediate(bool b = true) { _close_immediately = b; }

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

        void close_listener(uint16_t p);

        bool in_event_loop() const { return _loop->in_event_loop(); }

        void shutdown_endpoint();

        SSL_CTX* inbound_ctx();

        SSL_CTX* outbound_ctx();

      private:
        void handle_ep_opt(std::shared_ptr<ssl_creds> c);

        template <typename... Opt>
        static constexpr void require_ssl_creds()
        {
            static_assert(
                (0 + ... + std::is_convertible_v<std::remove_cvref_t<Opt>, std::shared_ptr<ssl_creds>>) == 1,
                "Endpoint construction requires exactly one std::shared_ptr<ssl_creds> argument");
        }
    };
}  //  namespace wshttp
