#pragma once

#include "listener.hpp"

extern "C" {
#include <event2/ws.h>
}

namespace wshttp
{
    namespace deleters
    {
        struct _evws
        {
            inline void operator()(::evws_connection* e) const { evws_connection_free(e); }
        };
    }  // namespace deleters

    using evws_ptr = std::unique_ptr<::evws_connection, deleters::_evws>;

    struct ws_session_base
    {
        friend class listener;
        friend struct ws_callbacks;

        explicit ws_session_base(listener& l, ip_address remote, evhttp_request* req);

      protected:
        listener& _l;

        evws_ptr _ws;

        path _path;

        evutil_socket_t _fd{-1};

        void recv_msg(int type, uspan data);

        void close_session();

      public:
        //
    };
}  // namespace wshttp
