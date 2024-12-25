#pragma once

#include "types.hpp"

#define OPENSSL_API_COMPAT 30000

extern "C" {
#include <openssl/conf.h>
#include <openssl/decoder.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/types.h>
}

namespace wshttp
{
    class endpoint;

    namespace deleters
    {
        struct _ssl_ctx
        {
            inline void operator()(SSL_CTX* s) const { SSL_CTX_free(s); }
        };

        struct _ssl
        {
            inline void operator()(SSL* s) const { SSL_shutdown(s); }
        };
    }  // namespace deleters

    using ssl_ptr = std::unique_ptr<::SSL, deleters::_ssl>;
    using ssl_ctx_ptr = std::unique_ptr<::SSL_CTX, deleters::_ssl_ctx>;

    namespace auth
    {
        enum class ALPN { NONE, UNVALIDATED, VALIDATED };
    }  //  namespace auth

    struct ssl_creds
    {
      private:
        explicit ssl_creds(const std::string_view& keyfile, const std::string_view& certfile) :
                _keyfile{keyfile}, _certfile{certfile}
        {
            if (_keyfile.empty() or _certfile.empty())
                throw std::invalid_argument{"Empty paths"};
        }

      public:
        static std::shared_ptr<ssl_creds> make(const std::string_view& keyfile, const std::string_view& certfile)
        {
            return std::shared_ptr<ssl_creds>{new ssl_creds{keyfile, certfile}};
        }

        const fs::path _keyfile;
        const fs::path _certfile;
    };

    class app_context
    {
        friend struct ctx_callbacks;
        friend class endpoint;
        friend class node;
        friend class listener;

        app_context(std::shared_ptr<ssl_creds> c) : _creds{c} { _init(); }

      public:
        static std::shared_ptr<app_context> make(std::shared_ptr<ssl_creds> c)
        {
            return std::shared_ptr<app_context>{new app_context{c}};
        }

        SSL_CTX* I() { return _i.get(); }
        const SSL_CTX* I() const { return _i.get(); }

        SSL_CTX* O() { return _o.get(); }
        const SSL_CTX* O() const { return _o.get(); }

      private:
        std::shared_ptr<ssl_creds> _creds;

        ssl_ctx_ptr _i;
        ssl_ctx_ptr _o;

        void _init();

        void _set_sslopts(bool outbound);

        void _init_inbound();
        void _init_outbound();
    };
}  //  namespace wshttp

namespace std
{
    template <>
    struct hash<wshttp::app_context>
    {
        size_t operator()(const wshttp::app_context& c) const noexcept
        {
            // TODO:
            (void)c;
            return {};
        }
    };
}  //  namespace std
