#pragma once

#include "utils.hpp"

namespace wshttp
{
    template <typename T>
    concept SessionWrapper = std::is_same_v<T, nghttp2_session>;

    class Stream;

    class Session
    {
        Session();
        // No copy, no move; always hold in shared_ptr using static ::make()
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

      public:
        static std::shared_ptr<Session> make();

      private:
        std::unique_ptr<nghttp2_session, decltype(session_deleter)> _session;

        // HTTP streams mapped to remote uri
        // TODO: make URI struct
        std::unordered_map<std::string, Stream> _streams;

      public:
        template <SessionWrapper T>
        operator const T*() const
        {
            return _session.get();
        }

        template <SessionWrapper T>
        operator T*()
        {
            return _session.get();
        }

        ~Session();
    };
}  //  namespace wshttp
