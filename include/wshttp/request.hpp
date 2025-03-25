#pragma once

#include "listener.hpp"

namespace wshttp
{
    enum class SCHEME : uint8_t { HTTP = 0, HTTPS = 1 };

    enum class METHOD : uint8_t { UNSUPPORTED = 0, GET = 1, POST = 2, HEAD = 3, PUT = 4, DELETE = 5 };

    template <typename T>
    concept supported_method = std::is_same_v<T, METHOD> && requires(T a) { std::to_underlying(a) > 0; };

    struct uri_t
    {
        uri_t() = delete;

        uri_t(std::string_view input) : uri_t{input.data(), input.size()} {}

        template <const_span_convertible T>
        uri_t(T input) : uri_t{reinterpret_cast<const char*>(input.data()), input.size()}
        {}

        ~uri_t();

      private:
        uri_t(const char* input, size_t inputlen);

        evhttp_uri* evuri;

        SCHEME _scheme;

      public:
        std::string_view scheme() const;
    };

    struct hdr
    {
        static constexpr auto* host = "Host";
        static constexpr auto* conn = "Connection";
        static constexpr auto* close = "close";
    };

    // wrapper for am evhttp_request
    struct http_request
    {
        evhttp_request* req = nullptr;

        http_request(evhttp_request* r, const char* host);

      protected:
        evkeyvalq* buffer = nullptr;

      public:
        // void make();
        //
    };
}  //  namespace wshttp
