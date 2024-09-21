#pragma once

#include "address.hpp"
#include "request.hpp"
#include "types.hpp"

namespace wshttp
{
    class inbound_session;

    class stream
    {
        friend class event_loop;
        friend class inbound_session;
        friend struct session_callbacks;

        stream(inbound_session& s, const session_ptr& _s, int32_t id = 0);

        // No copy, no move; always hold in shared_ptr using static ::make()
        stream(const stream&) = delete;
        stream& operator=(const stream&) = delete;
        stream(stream&&) = delete;
        stream& operator=(stream&&) = delete;

      public:
        ~stream() = default;

      private:
        inbound_session& _s;
        session_ptr _session;

        int32_t _id;
        int _fd{-1};
        std::array<int, 2> _pipes{};

        uri _req;

        int recv_header(req::headers hdr);

        int recv_request();

        int send_error();

        int send_response(req::headers hdr);

      public:
        int fd() const { return _fd; }
    };
    namespace deleters
    {
        inline constexpr auto stream_d = [](stream* s) {
            if (s->fd() == -1)
                close(s->fd());
        };
    }

}  //  namespace wshttp
