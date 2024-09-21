#pragma once

#include "address.hpp"
#include "context.hpp"
#include "listener.hpp"
#include "request.hpp"

namespace wshttp
{
    class stream;
    class endpoint;

    class session
    {
        friend class stream;
        friend class listener;
        friend struct session_callbacks;

      public:
        session(IO dir, listener& l, ip_address remote, evutil_socket_t fd);

        session() = delete;

        // No copy, no move; always hold in shared_ptr using static ::make()
        session(const session&) = delete;
        session& operator=(const session&) = delete;
        session(session&&) = delete;
        session& operator=(session&&) = delete;

        ~session();

        static std::shared_ptr<session> make(IO dir, listener& l, ip_address remote, evutil_socket_t fd);

        bool is_inbound() const { return _dir == IO::INBOUND; }
        bool is_outbound() const { return _dir == IO::OUTBOUND; }

      private:
        IO _dir;

        listener& _lst;
        endpoint& _ep;

        evutil_socket_t _fd;

        path _path;

        ssl_ptr _ssl;
        bufferevent_ptr _bev;

        session_ptr _session;
        std::unordered_map<uint32_t, std::shared_ptr<stream>> _streams;

        void _init_internals();

        void config_send_initial();

        void initialize_session();

        void initialize_client_session();

        void send_server_initial();

        void send_session_data();

        void read_session_data();

        void write_session_data();

        nghttp2_ssize send_hook(ustring_view data);

        int stream_close_hook(int32_t stream_id, uint32_t error_code = 0);

        int begin_headers_hook(int32_t stream_id);

        std::shared_ptr<stream> make_stream(int32_t stream_id);

        void close_session();

      public:
        const ip_address& local() const { return _path.local(); }
        const ip_address& remote() const { return _path.remote(); }
        const path& path() const { return _path; }

        template <concepts::nghttp2_session_type T>
        operator const T*() const
        {
            return _session.get();
        }

        template <concepts::nghttp2_session_type T>
        operator T*()
        {
            return _session.get();
        }
    };
}  //  namespace wshttp
