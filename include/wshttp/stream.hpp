#pragma once

#include "address.hpp"
#include "request.hpp"
#include "utils.hpp"

namespace wshttp
{
    class session;

    class stream
    {
        friend class event_loop;
        friend class session;

        stream(session& s, int32_t id = 0);

        // No copy, no move; always hold in shared_ptr using static ::make()
        stream(const stream&) = delete;
        stream& operator=(const stream&) = delete;
        stream(stream&&) = delete;
        stream& operator=(stream&&) = delete;

      public:
        ~stream() = default;

      private:
        session& _s;
        int32_t _id;
        int _fd{-1};
        uri _req;

        int recv_header(req::headers hdr);

        int recv_request();

      public:
        //
    };
}  //  namespace wshttp
