#include "loop.hpp"

#include "internal.hpp"
// #include "utils.hpp"

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

    Loop::Loop(std::shared_ptr<::event_base> loop_ptr, std::thread::id thread_id)
        : ev_loop{std::move(loop_ptr)}, loop_thread_id{thread_id}
    {
        assert(ev_loop);
        log->trace("Beginning event loop creation with pre-existing ev loop thread");

        setup_job_waker();

        running.store(true);
    }

    static std::vector<std::string_view> get_ev_methods()
    {
        std::vector<std::string_view> ev_methods_avail;
        for (const char** methods = event_get_supported_methods(); methods && *methods; methods++)
            ev_methods_avail.emplace_back(*methods);
        return ev_methods_avail;
    }

    Loop::Loop()
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

        log->debug(
            "Starting libevent {}; available backends: {}", event_get_version(), fmt::join(ev_methods_avail, ", "));

        std::unique_ptr<event_config, decltype(&event_config_free)> ev_conf{event_config_new(), event_config_free};
        event_config_set_flag(ev_conf.get(), EVENT_BASE_FLAG_PRECISE_TIMER);
        event_config_set_flag(ev_conf.get(), EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST);

        ev_loop = std::shared_ptr<event_base>{event_base_new_with_config(ev_conf.get()), event_base_free};

        log->info("Started libevent loop with backend {}", event_base_get_method(ev_loop.get()));

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

    Loop::~Loop()
    {
        log->info("Shutting down loop...");

        if (loop_thread)
            event_base_loopbreak(ev_loop.get());

        if (loop_thread and loop_thread->joinable())
            loop_thread->join();

        log->info("Loop shutdown complete");

#ifdef _WIN32
        WSACleanup();
#endif
    }

    void Loop::call_soon(std::function<void(void)> f)
    {
        {
            std::lock_guard lock{job_queue_mutex};
            job_queue.emplace(std::move(f));
            log->trace("Event loop now has {} jobs queued", job_queue.size());
        }

        event_active(job_waker.get(), 0, 0);
    }

    void Loop::shutdown(bool immediate)
    {
        log->info("Shutting down loop...");

        if (loop_thread)
            immediate ? event_base_loopbreak(ev_loop.get()) : event_base_loopexit(ev_loop.get(), nullptr);

        if (loop_thread and loop_thread->joinable())
            loop_thread->join();

        log->info("Loop shutdown complete");
    }

    void Loop::setup_job_waker()
    {
        job_waker.reset(event_new(
            ev_loop.get(),
            -1,
            0,
            [](evutil_socket_t, short, void* self) {
                log->trace("processing job queue");
                static_cast<Loop*>(self)->process_job_queue();
            },
            this));
        assert(job_waker);
    }

    void Loop::process_job_queue()
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
