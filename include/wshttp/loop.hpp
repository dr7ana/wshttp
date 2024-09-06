#pragma once

#include "types.hpp"

#include <atomic>
#include <cstdint>
#include <future>
#include <list>
#include <memory>
#include <thread>

namespace wshttp
{
    using Job = std::function<void()>;
    using loop_ptr = std::shared_ptr<::event_base>;
    using caller_id_t = uint16_t;

    class event_loop;

    struct ev_watcher
    {
        friend class event_loop;
        friend struct loop_callbacks;

      private:
        std::atomic<bool> _is_running{false};
        event_ptr ev;
        timeval interval;
        std::function<void()> f;

        void init_event(
            const loop_ptr& _loop, std::chrono::microseconds _t, std::function<void()> task, bool one_off = false);

        ev_watcher() = default;

        void fire();

      public:
        ~ev_watcher();

        bool is_running() const { return _is_running; }

        /** Starts the repeating event on the given interval on Ticker creation
            Returns:
                - true: event successfully started
                - false: event is already running, or failed to start the event
         */
        bool start();

        /** Stops the repeating event managed by Ticker
            Returns:
                - true: event successfully stopped
                - false: event is already stopped, or failed to stop the event
         */
        bool stop();
    };

    class event_loop final
    {
        friend class endpoint;
        event_loop();

        event_loop(const event_loop&) = delete;
        event_loop(event_loop&&) = delete;
        event_loop& operator=(event_loop&&) = delete;
        event_loop& operator=(event_loop) = delete;

      public:
        [[nodiscard]] static std::shared_ptr<event_loop> make();

        ~event_loop();

      private:
        std::atomic<bool> running{false};
        std::shared_ptr<::event_base> ev_loop;
        std::optional<std::thread> loop_thread;
        std::thread::id loop_thread_id;

        event_ptr job_waker;
        std::queue<Job> job_queue;
        std::mutex job_queue_mutex;

        std::unordered_map<caller_id_t, std::list<std::weak_ptr<ev_watcher>>> tickers;

      public:
        const std::shared_ptr<::event_base>& loop() const { return ev_loop; }

        template <typename Callable>
        void call(Callable&& f)
        {
            if (in_event_loop())
            {
                f();
            }
            else
            {
                call_soon(std::forward<Callable>(f));
            }
        }

        template <typename Callable, typename Ret = decltype(std::declval<Callable>()())>
        Ret call_get(Callable&& f)
        {
            if (in_event_loop())
            {
                return f();
            }

            std::promise<Ret> prom;
            auto fut = prom.get_future();

            call_soon([&f, &prom] {
                try
                {
                    if constexpr (!std::is_void_v<Ret>)
                        prom.set_value(f());
                    else
                    {
                        f();
                        prom.set_value();
                    }
                }
                catch (...)
                {
                    prom.set_exception(std::current_exception());
                }
            });

            return fut.get();
        }

        /** This overload of `call_every` will begin an indefinitely repeating object tied to the lifetime of `caller`.
            Prior to executing each iteration, the weak_ptr will be checked to ensure the calling object lifetime has
            persisted up to that point.
        */
        template <typename Callable>
        void call_every(std::chrono::microseconds interval, std::weak_ptr<void> caller, Callable&& f)
        {
            _call_every(interval, std::move(caller), std::forward<Callable>(f), event_loop::loop_id);
        }

        /** This overload of `call_every` will return an EventHandler object from which the application can start and
           stop the repeated event. It is NOT tied to the lifetime of the caller via a weak_ptr. If the application
           wants to defer start until explicitly calling EventHandler::start(), `start_immediately` should take a false
           boolean.
        */
        template <typename Callable>
        [[nodiscard]] std::shared_ptr<ev_watcher> call_every(std::chrono::microseconds interval, Callable&& f)
        {
            return _call_every(interval, std::forward<Callable>(f), event_loop::loop_id);
        }

        template <std::invocable Callable>
        void call_later(std::chrono::microseconds delay, Callable hook)
        {
            if (in_event_loop())
            {
                add_oneshot_event(delay, std::move(hook));
            }
            else
            {
                call_soon([this, func = std::move(hook), target_time = detail::get_time() + delay]() mutable {
                    auto now = detail::get_time();

                    if (now >= target_time)
                        func();
                    else
                        add_oneshot_event(
                            std::chrono::duration_cast<std::chrono::microseconds>(target_time - now), std::move(func));
                });
            }
        }

        template <std::invocable Callable>
        void call_soon(Callable f)
        {
            {
                std::lock_guard lock{job_queue_mutex};
                job_queue.emplace(std::move(f));
            }

            event_active(job_waker.get(), 0, 0);
        }

      private:
        template <std::invocable Callable>
        void add_oneshot_event(std::chrono::microseconds delay, Callable hook)
        {
            auto handler = make_handler(event_loop::loop_id);
            auto& h = *handler;

            h.init_event(
                loop(),
                delay,
                [hndlr = std::move(handler), func = std::move(hook)]() mutable {
                    auto h = std::move(hndlr);
                    func();
                    h.reset();
                },
                true);
        }

        void clear_old_tickers();

        std::shared_ptr<ev_watcher> make_handler(caller_id_t _id);

        bool in_event_loop() const { return std::this_thread::get_id() == loop_thread_id; }

        static constexpr caller_id_t loop_id{0};

        // Returns a pointer deleter that defers the actual destruction call to this network
        // object's event loop.
        template <typename T>
        auto loop_deleter()
        {
            return [this](T* ptr) {
                call([ptr] { delete ptr; });
            };
        }

        // Returns a pointer deleter that defers invocation of a custom deleter to the event loop
        template <typename T, std::invocable<T*> Callable>
        auto wrapped_deleter(Callable f)
        {
            return [this, func = std::move(f)](T* ptr) mutable {
                return call_get([f = std::move(func), ptr]() { return f(ptr); });
            };
        }

        // Similar in concept to std::make_shared<T>, but it creates the shared pointer with a
        // custom deleter that dispatches actual object destruction to the network's event loop for
        // thread safety.
        template <typename T, typename... Args>
        std::shared_ptr<T> make_shared(Args&&... args)
        {
            auto* ptr = new T{std::forward<Args>(args)...};
            return std::shared_ptr<T>{ptr, loop_deleter<T>()};
        }

        // Similar to the above make_shared, but instead of forwarding arguments for the
        // construction of the object, it creates the shared_ptr from the already created object ptr
        // and wraps the object's deleter in a wrapped_deleter
        template <typename T, typename Callable>
        std::shared_ptr<T> shared_ptr(T* obj, Callable&& deleter)
        {
            return std::shared_ptr<T>(obj, wrapped_deleter<T>(std::forward<Callable>(deleter)));
        }

        // private method invoked in Network destructor by final Network to close shared_ptr
        void stop_thread(bool immediate = true);

        void stop_tickers(caller_id_t _id);

        template <typename Callable>
        [[nodiscard]] std::shared_ptr<ev_watcher> _call_every(
            std::chrono::microseconds interval, Callable&& f, caller_id_t _id)
        {
            auto h = make_handler(_id);

            h->init_event(loop(), interval, std::forward<Callable>(f));

            return h;
        }

        void setup_job_waker();

        void process_job_queue();
    };
}  // namespace wshttp
