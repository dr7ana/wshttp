#pragma once

#include "types.hpp"

namespace wshttp
{
    class Stream;

    class Session
    {
        // No copy, no move; always hold in shared_ptr using static ::make()
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

      public:
        Session();

        static std::shared_ptr<Session> make();

        ~Session();

      protected:
        std::unique_ptr<nghttp2_session, decltype(session_deleter)> _session;

        std::unordered_map<uint32_t, Stream> _streams;

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
