#pragma once

#include "listener.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    enum class METHOD : int { UNSUPPORTED = 0, GET = 1, POST = 2, HEAD = 3, PUT = 4, DELETE = 5 };

    namespace deleters
    {
        struct _bufferevent
        {
            inline void operator()(::bufferevent* b) const
            {
                if (b)
                    bufferevent_free(b);
            }
        };
    }  // namespace deleters

    using bufferevent_ptr = std::unique_ptr<::bufferevent, deleters::_bufferevent>;

    struct inbound_request
    {
        friend class listener;

        explicit inbound_request(listener& l, ip_address remote, evutil_socket_t sock);

      private:
        listener& _l;

        uri _uri;

        path _path;

        evutil_socket_t _fd{-1};

      protected:
        // int recv_initial(struct evhttp_request* req);

      public:
        //
    };
}  //  namespace wshttp
