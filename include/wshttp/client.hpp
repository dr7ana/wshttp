#pragma once

#include "loop.hpp"

namespace wshttp::dns
{
    class Server;
}

namespace wshttp
{
    class Client final
    {
        friend class dns::Server;

        Client();
        explicit Client(std::shared_ptr<Loop> ev_loop);
        Client(const Client& c) : Client{c._loop}
        {}

        Client& operator=(Client) = delete;
        Client& operator=(Client&&) = delete;

      public:
        static std::unique_ptr<Client> make();
        static std::unique_ptr<Client> make(std::shared_ptr<Loop> ev_loop);

        ~Client();

        [[nodiscard]] std::unique_ptr<Client> create_linked_client();

      private:
        std::shared_ptr<Loop> _loop;
        std::atomic<bool> _close_immediately{false};

        const caller_id_t client_id;
        static caller_id_t next_client_id;

      public:
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

        void call_soon(std::function<void(void)> f)
        {
            _loop->call_soon(std::move(f));
        }

        template <typename Callable>
        void call_every(std::chrono::microseconds interval, std::weak_ptr<void> caller, Callable&& f)
        {
            _loop->_call_every(interval, std::move(caller), std::forward<Callable>(f), client_id);
        }

        template <typename Callable>
        std::shared_ptr<Ticker> call_every(
            std::chrono::microseconds interval, Callable&& f, bool start_immediately = true)
        {
            return _loop->_call_every(interval, std::forward<Callable>(f), client_id, start_immediately);
        }

        template <typename Callable>
        void call_later(std::chrono::microseconds delay, Callable&& hook)
        {
            _loop->call_later(delay, std::forward<Callable>(hook));
        }

        void set_shutdown_immediate(bool b = true)
        {
            _close_immediately = b;
        }

      private:
        template <typename T, typename Callable>
        std::shared_ptr<T> shared_ptr(T* obj, Callable&& deleter)
        {
            return _loop->shared_ptr<T>(obj, std::forward<Callable>(deleter));
        }

        template <typename T, typename... Args>
        std::shared_ptr<T> make_shared(Args&&... args)
        {
            return _loop->make_shared<T>(std::forward<Args>(args)...);
        }

        bool in_event_loop() const
        {
            return _loop->in_event_loop();
        }

        void teardown_client();
    };
}  //  namespace wshttp
