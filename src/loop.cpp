#include "loop.hpp"

#include "internal.hpp"

namespace wshttp
{
    static void setup_libevent_logging()
    {
        event_set_log_callback([](int severity, const char* msg) {
            switch (severity)
            {
                case _EVENT_LOG_ERR:
                    log->error("{}", msg);
                    break;
                case _EVENT_LOG_WARN:
                    log->warn("{}", msg);
                    break;
                case _EVENT_LOG_MSG:
                    log->info("{}", msg);
                    break;
                case _EVENT_LOG_DEBUG:
                    log->debug("{}", msg);
                    break;
            }
            std::abort();
        });
    }

    /** Static casting to `decltype(timeval::tv_{sec,usec})` makes sure that;
        - on linux
            .tv_sec is type __time_t
            .tv_usec is type __suseconds_t
        - on OSX    (https://developer.apple.com/documentation/kernel/timeval)
            .tv_sec is type __darwin_time_t
                - this is an annoying typedef of `time_t`
            .tv_usec is type __darwin_suseconds_t
                - this is an equally annoying typedef for `suseconds_t`
        Alas, yet again another mac idiosyncrasy...
     */
    timeval loop_time_to_timeval(std::chrono::microseconds t)
    {
        return timeval{
            .tv_sec = static_cast<decltype(timeval::tv_sec)>(t / 1s),
            .tv_usec = static_cast<decltype(timeval::tv_usec)>((t % 1s) / 1us)};
    }

    void loop_callbacks::exec_iterative(int /* fd */, short /* what */, void* user_arg)
    {
        try
        {
            auto* self = reinterpret_cast<ev_watcher*>(user_arg);
            if (not self->f)
            {
                log->critical("Ticker does not have a callback to execute!");
                return;
            }
            if (not self->is_running())
            {
                log->critical("Ticker attempting to execute finished event!");
                return;
            }
            // execute callback
            self->fire();
        }
        catch (const std::exception& e)
        {
            log->critical("EventTicker caught exception: {}", e.what());
        }
    }

    void loop_callbacks::exec_oneshot(int /* fd */, short /* what */, void* user_arg)
    {
        try
        {
            auto* self = reinterpret_cast<ev_watcher*>(user_arg);
            if (not self->f)
            {
                log->critical("Ticker does not have a callback to execute!");
                return;
            }
            // execute callback
            self->f();
        }
        catch (const std::exception& e)
        {
            log->critical("Ticker caught exception: {}", e.what());
        }
    }

    bool ev_watcher::start()
    {
        if (_is_running)
            return false;

        if (event_add(ev.get(), &interval) != 0)
        {
            log->critical("EventHandler failed to start repeating event!");
            return false;
        }

        _is_running = true;

        return true;
    }

    bool ev_watcher::stop()
    {
        if (not _is_running)
            return false;

        if (event_del(ev.get()) != 0)
        {
            log->critical("EventHandler failed to pause repeating event!");
            return false;
        }

        _is_running = false;

        return true;
    }

    void ev_watcher::fire()
    {
        f();
        event_del(ev.get());
        event_add(ev.get(), &interval);
    }

    void ev_watcher::init_event(
        const loop_ptr& _loop,
        std::chrono::microseconds _t,
        std::function<void()> task,
        bool one_off,
        bool start_immediately,
        bool fixed_interval)
    {
        f = (one_off or not fixed_interval) ? std::move(task) : [this, func = std::move(task)]() mutable {
            func();
            event_del(ev.get());
            event_add(ev.get(), &interval);
        };

        interval = loop_time_to_timeval(_t);

        ev.reset(event_new(
            _loop.get(),
            -1,
            0,
            [](evutil_socket_t, short, void* s) {
                try
                {
                    auto* self = reinterpret_cast<ev_watcher*>(s);
                    if (not self->f)
                    {
                        log->critical("Ticker does not have a callback to execute!");
                        return;
                    }
                    // execute callback
                    self->f();
                }
                catch (const std::exception& e)
                {
                    log->critical("Ticker caught exception: {}", e.what());
                }
            },
            this));

        if ((one_off or start_immediately) and not start())
            log->critical("Failed to immediately start one-off event!");
    }

    ev_watcher::~ev_watcher()
    {
        ev.reset();
        f = nullptr;
    }

    static std::vector<std::string_view> get_ev_methods()
    {
        std::vector<std::string_view> ev_methods_avail;
        for (const char** methods = event_get_supported_methods(); methods && *methods; methods++)
            ev_methods_avail.emplace_back(*methods);
        return ev_methods_avail;
    }

    std::shared_ptr<event_loop> event_loop::make()
    {
        return std::shared_ptr<event_loop>{new event_loop{}};
    }

    event_loop::event_loop()
    {
        log->trace("Beginning loop context creation with new ev loop thread");

#ifdef _WIN32
        {
            WSADATA ignored;
            if (int err = WSAStartup(MAKEWORD(2, 2), &ignored); err != 0)
            {
                log->critical("WSAStartup failed to initialize the windows socket layer ({:x})", err);
                throw std::runtime_error{"Unable to initialize windows socket layer"};
            }
        }
#endif

        if (static bool once = false; !once)
        {
            once = true;
            setup_libevent_logging();

            // Older versions of libevent do not like having this called multiple times
#ifdef _WIN32
            evthread_use_windows_threads();
#else
            evthread_use_pthreads();
#endif
        }

        static std::vector<std::string_view> ev_methods_avail = get_ev_methods();

        log->trace(
            "Starting libevent {}; available backends: {}", event_get_version(), fmt::join(ev_methods_avail, ", "));

        std::unique_ptr<event_config, decltype(&event_config_free)> ev_conf{event_config_new(), event_config_free};
        event_config_set_flag(ev_conf.get(), EVENT_BASE_FLAG_PRECISE_TIMER);
        event_config_set_flag(ev_conf.get(), EVENT_BASE_FLAG_NO_CACHE_TIME);
        event_config_set_flag(ev_conf.get(), EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST);

        ev_loop = std::shared_ptr<event_base>{event_base_new_with_config(ev_conf.get()), event_base_free};

        log->debug("Started libevent loop with backend {}", event_base_get_method(ev_loop.get()));

        setup_job_waker();

        std::promise<void> p;

        loop_thread.emplace([this, &p]() mutable {
            log->debug("Starting event loop run");
            p.set_value();
            event_base_loop(ev_loop.get(), EVLOOP_NO_EXIT_ON_EMPTY);
            log->debug("Event loop run returned, thread finished");
        });

        loop_thread_id = loop_thread->get_id();
        p.get_future().get();

        running.store(true);
        log->info("loop is started");
    }

    event_loop::~event_loop()
    {
        log->info("Shutting down loop...");

        stop_thread();

        for (auto& [id, list] : tickers)
        {
            std::for_each(list.begin(), list.end(), [](auto& t) {
                if (auto tick = t.lock())
                {
                    tick->f = nullptr;
                    tick->stop();
                }
            });
        }

        log->info("Loop shutdown complete");

#ifdef _WIN32
        WSACleanup();
#endif
    }

    void event_loop::stop_thread(bool immediate)
    {
        log->debug("Stopping loop thread...");
        if (loop_thread)
            immediate ? event_base_loopbreak(ev_loop.get()) : event_base_loopexit(ev_loop.get(), nullptr);

        if (loop_thread and loop_thread->joinable())
            loop_thread->join();
    }

    void event_loop::clear_old_tickers()
    {
        for (auto& [id, list] : tickers)
        {
            for (auto itr = list.begin(); itr != list.end();)
            {
                if (itr->expired())
                    itr = list.erase(itr);
                else
                    ++itr;
            }
        }
    }

    std::shared_ptr<ev_watcher> event_loop::make_handler(caller_id_t _id)
    {
        clear_old_tickers();
        auto t = make_shared<ev_watcher>();
        tickers[_id].push_back(t);
        return t;
    }

    void event_loop::stop_tickers(caller_id_t id)
    {
        if (auto it = tickers.find(id); it != tickers.end())
        {
            for (auto& t : it->second)
            {
                if (auto tick = t.lock())
                {
                    tick->f = nullptr;
                    tick->stop();
                }
            }
        }
    }

    void event_loop::setup_job_waker()
    {
        job_waker.reset(event_new(
            ev_loop.get(),
            -1,
            0,
            [](evutil_socket_t, short, void* self) {
                log->trace("processing job queue");
                static_cast<event_loop*>(self)->process_job_queue();
            },
            this));
        assert(job_waker);
    }

    void event_loop::process_job_queue()
    {
        log->trace("Event loop processing job queue");
        assert(in_event_loop());

        decltype(job_queue) swapped_queue;

        {
            std::lock_guard<std::mutex> lock{job_queue_mutex};
            job_queue.swap(swapped_queue);
        }

        while (not swapped_queue.empty())
        {
            auto job = swapped_queue.front();
            swapped_queue.pop();
            job();
        }
    }

}  //  namespace wshttp
