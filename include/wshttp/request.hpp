#pragma once

#include "address.hpp"
#include "listener.hpp"
#include "ssl.hpp"
#include "utils.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

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
        inbound_request(listener& l, ip_address remote, evhttp_connection* c);

      private:
        listener& _l;

        bufferevent_ptr _bev;

        evutil_socket_t _fd{-1};

        path _path;

      public:
        //
    };
}  //  namespace wshttp
