#pragma once

#include "address.hpp"
#include "request.hpp"
#include "session.hpp"

namespace wshttp
{
    enum class IO { INBOUND, OUTBOUND };

    class stream_base
    {
        friend class event_loop;
        friend class inbound_session;
        friend class outbound_node;
        friend struct session_callbacks;

      public:
        virtual ~stream_base() = default;

      protected:
        stream_base(session_base& b, const session_ptr& s, IO d, int32_t id = 0) : _s{b}, _session{s}, _dir{d}, _id{id}
        {}

        // No copy, no move; always hold in shared_ptr using static ::make()
        stream_base(const stream_base&) = delete;
        stream_base& operator=(const stream_base&) = delete;
        stream_base(stream_base&&) = delete;
        stream_base& operator=(stream_base&&) = delete;

        session_base& _s;
        session_ptr _session;

        IO _dir;

        int32_t _id;
        int _fd{-1};
        std::array<int, 2> _pipes{};
    };

    class stream
    {
        friend class event_loop;
        friend class inbound_session;
        friend class outbound_node;
        friend struct session_callbacks;

        stream(inbound_session& s, const session_ptr& _s, int32_t id = 0);

        stream(outbound_node& s, const session_ptr& _s, int32_t id = 0);

        // No copy, no move; always hold in shared_ptr using static ::make()
        stream(const stream&) = delete;
        stream& operator=(const stream&) = delete;
        stream(stream&&) = delete;
        stream& operator=(stream&&) = delete;

      public:
        ~stream() = default;

      private:
        session_base& _s;
        session_ptr _session;

        IO dir;

        int32_t _id;
        int _fd{-1};
        std::array<int, 2> _pipes{};

        uri _req;

        int recv_path_header(uspan path);

        int recv_header(req::headers hdr);

        int recv_frame();

        int submit_stream_rst(uint32_t ec);

        int send_error();

        int send_response(req::headers hdr);

        struct deleter
        {
            inline void operator()(stream* s) const
            {
                if (s and s->fd() != -1)
                    ::close(s->fd());
            }
        };

      public:
        int fd() const { return _fd; }
    };
}  //  namespace wshttp
