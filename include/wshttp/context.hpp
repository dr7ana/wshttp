#pragma once

#include "utils.hpp"

namespace wshttp
{
    using ssl_ptr = std::unique_ptr<::SSL_CTX, decltype(ssl_deleter)>;

    class app_context;

    class IOContext
    {
        explicit IOContext(const std::string& keyfile, const std::string& certfile);

      public:
        IOContext() = delete;

        static std::unique_ptr<IOContext> make(const std::string& keyfile, const std::string& certfile);

        enum IO { I, O };

      private:
        const fs::path _keyfile;
        const fs::path _certfile;

        std::unique_ptr<app_context> _inbounds;

        std::unique_ptr<app_context> _o;
    };

    class app_context
    {
        friend class IOContext;

        explicit app_context(IOContext::IO _d, const fs::path& _key, const fs::path& _cert);

      public:
        app_context() = delete;

        static std::unique_ptr<app_context> make(IOContext::IO _d, const fs::path& _key, const fs::path& _cert);

        ~app_context() = default;

      private:
        const IOContext::IO _dir;
        ssl_ptr _ctx;

        void _init_inbound(const char* _keyfile, const char* _certfile);
        void _init_outbound(const char* _keyfile, const char* _certfile);
    };

}  //  namespace wshttp
