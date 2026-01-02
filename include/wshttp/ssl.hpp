#pragma once

#define OPENSSL_API_COMPAT 30000

#include "types.hpp"

extern "C" {
#include <openssl/conf.h>
#include <openssl/decoder.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/types.h>
#include <openssl/x509.h>
}

#include <variant>

namespace wshttp {
    class endpoint;

    namespace deleters {
        struct _ssl_ctx {
            inline void operator()(::SSL_CTX* s) const { SSL_CTX_free(s); }
        };

        struct _ssl {
            inline void operator()(::SSL* s) const { SSL_shutdown(s); }
        };
        struct _evp {
            inline void operator()(::EVP_PKEY* p) const { return ::EVP_PKEY_free(p); }
        };

        struct _x509 {
            inline void operator()(::X509* x) const { return ::X509_free(x); }
        };
    }  // namespace deleters

    using evp_pkey_ptr = std::unique_ptr<::EVP_PKEY, deleters::_evp>;
    using x509_ptr = std::unique_ptr<::X509, deleters::_x509>;
    using ssl_ptr = std::unique_ptr<::SSL, deleters::_ssl>;
    using ssl_ctx_ptr = std::unique_ptr<::SSL_CTX, deleters::_ssl_ctx>;

    struct ssl_creds;
    class app_context;

    struct x509_cert_keypair {
        friend struct ssl_creds;

      protected:
        evp_pkey_ptr pk;
        x509_ptr x;

        void _init_internals();

        std::pair<::X509*, ::EVP_PKEY*> cert_keypair() { return {x.get(), pk.get()}; }

      public:
        x509_cert_keypair();
    };

    struct cert_pk_file_pair {
        friend struct ssl_creds;

        cert_pk_file_pair() = delete;

      protected:
        explicit cert_pk_file_pair(const std::string_view& keyfile, const std::string_view& certfile);

        fs::path key;
        fs::path cert;

        std::pair<fs::path, fs::path> cert_keypair() { return {cert, key}; }

      public:
        //
    };

    // first: certfile, second: keyfile
    using ssl_cert_store_v = std::variant<cert_pk_file_pair, x509_cert_keypair>;

    struct ssl_creds {
        friend class app_context;

      private:
        ssl_cert_store_v storage;
        const size_t variant_index{storage.index()};

        explicit ssl_creds(const std::string_view& keyfile, const std::string_view& certfile);

        ssl_creds() : storage{x509_cert_keypair{}} {}

        void configure_ssl_ctx(SSL_CTX* inbound, SSL_CTX* outbound);

      public:
        ~ssl_creds() = default;

        static std::shared_ptr<ssl_creds> make() { return std::shared_ptr<ssl_creds>{new ssl_creds{}}; }

        static std::shared_ptr<ssl_creds> make(const std::string_view& keyfile, const std::string_view& certfile) {
            return std::shared_ptr<ssl_creds>{new ssl_creds{keyfile, certfile}};
        }
    };

    class app_context {
        friend struct ctx_callbacks;
        friend class endpoint;
        friend class node;
        friend class listener;

        app_context(std::shared_ptr<ssl_creds> c);

      public:
        static std::shared_ptr<app_context> make(std::shared_ptr<ssl_creds> c) {
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

        void _init_inbound();
        void _init_outbound();
    };
}  //  namespace wshttp

namespace std {
    template <>
    struct hash<wshttp::app_context> {
        size_t operator()(const wshttp::app_context& c) const noexcept {
            // TODO:
            (void)c;
            return {};
        }
    };
}  //  namespace std
