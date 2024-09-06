#pragma once

#include "address.hpp"
#include "context.hpp"
#include "listener.hpp"

namespace wshttp
{
    class stream;
    class endpoint;

    class session
    {
        friend class listener;

      public:
        session(listener& l, ip_address remote, evutil_socket_t fd);

        session() = delete;

        // No copy, no move; always hold in shared_ptr using static ::make()
        session(const session&) = delete;
        session& operator=(const session&) = delete;
        session(session&&) = delete;
        session& operator=(session&&) = delete;

        ~session() = default;

        static std::shared_ptr<session> make(listener& l, ip_address remote, evutil_socket_t fd);

      private:
        endpoint& _ep;

        ip_address _local;
        ip_address _remote;

        ssl_ptr _ssl;
        bufferevent_ptr _bev;

        session_ptr _session;
        std::unordered_map<uint32_t, stream> _streams;

      public:
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
