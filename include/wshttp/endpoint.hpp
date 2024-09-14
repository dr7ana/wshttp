#pragma once

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

    template <typename... Opt>
    static constexpr void check_for_ssl_creds()
    {
        if constexpr ((std::is_same_v<std::shared_ptr<ssl_creds>, std::remove_cvref_t<Opt>> || ...))
        {
            static_assert(
                (0 + ... + std::is_convertible_v<std::remove_cvref_t<Opt>, std::shared_ptr<ssl_creds>>) == 1,
                "Node listen/connect require exactly one std::shared_ptr<ssl_creds> argument");
        }
    }

    class endpoint final
    {
        friend class session;
        friend class stream;
        friend class listener;
        friend class dns::server;

      public:
        endpoint();
        explicit endpoint(std::shared_ptr<event_loop> ev_loop);
        endpoint(const endpoint& c) : endpoint{c._loop} {}

        endpoint& operator=(endpoint) = delete;
        endpoint& operator=(endpoint&&) = delete;

        [[nodiscard]] static std::shared_ptr<endpoint> make();
        [[nodiscard]] static std::shared_ptr<endpoint> make(std::shared_ptr<event_loop> ev_loop);

        ~endpoint();

        [[nodiscard]] std::shared_ptr<endpoint> create_linked_node();

      private:
        std::shared_ptr<event_loop> _loop;

        std::shared_ptr<dns::server> _dns;

        const caller_id_t client_id;
        static caller_id_t next_client_id;

        // local listeners managing inbound https connections
        std::unordered_map<uint16_t, std::shared_ptr<listener>> _listeners;
        // local nodes manging outbound https connections
        std::unordered_map<std::string, std::shared_ptr<node>> _nodes;

        std::atomic<bool> _close_immediately{false};

      public:
        template <typename... Opt>
        bool listen(uint16_t port, Opt&&... opts)
        {
            check_for_ssl_creds<Opt...>();

            return call_get([&]() {
                auto [itr, b] = _listeners.try_emplace(port, nullptr);

                if (not b)
                    throw std::invalid_argument{
                        "Cannot create tcp-listener at port {} -- listener already exists!"_format(port)};

                itr->second = make_shared<listener>(*this, port, std::forward<Opt>(opts)...);

                if (not itr->second)
                    throw std::runtime_error{"TCP listener construction is fucked"};

                log->critical("Endpoint deployed tcp-listener on port {}", port);
                return true;
            });
        }

        template <typename... Opt>
        bool connect(std::string url, Opt&&... opts)
        {
            check_for_ssl_creds<Opt...>();

            return call_get([&]() {
                auto _uri = uri::parse(url);
                if (not _uri)
                    throw std::invalid_argument{"Failed to parse input url: {}"_format(url)};

                auto [itr, b] = _nodes.try_emplace(std::string{_uri.host()}, nullptr);

                if (not b)
                    throw std::invalid_argument{
                        "Cannot create outbound node for input {} -- node already exists!"_format(url)};

                // TODO: add uri to node ctor
                itr->second = make_shared<node>(*this, std::forward<Opt>(opts)...);

                if (not itr->second)
                    throw std::runtime_error{"Node construction is fucked"};

                log->critical("Endpoint deployed node for outbound to uri [{}]", url);
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

        template <typename... Opt>
        std::shared_ptr<node> make_node(endpoint& e, Opt&&... opts)
        {
            return _loop->template make_shared<node>(e, std::forward<Opt>(opts)...);
        }

        bool in_event_loop() const { return _loop->in_event_loop(); }

        void shutdown_node();
    };
}  //  namespace wshttp
