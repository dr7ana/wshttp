#pragma once

extern "C"
{
#include <event2/event.h>
#include <event2/thread.h>
}

#include <memory>
#include <optional>
#include <queue>

namespace wshttp
{
    struct event_deleter final
    {
        void operator()(::event* e) const;
    };

    using event_ptr = std::unique_ptr<::event, event_deleter>;
}  //  namespace wshttp
