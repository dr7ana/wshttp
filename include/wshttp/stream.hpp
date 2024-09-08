#pragma once

#include "utils.hpp"
#include "request.hpp"

namespace wshttp
{
    class session;

    class stream
    {
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
        std::vector<char> _uri;
        int32_t _id;
        int _fd;

        int recv_header(req::headers hdr);

        int recv_request();

      public:
        //
    };
}  //  namespace wshttp
