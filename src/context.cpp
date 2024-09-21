#include "context.hpp"

#include "internal.hpp"

namespace wshttp
{
    int ctx_callbacks::server_select_alpn_proto_cb(
        SSL*,
        const unsigned char** out,
        unsigned char* outlen,
        const unsigned char* in,
        unsigned int inlen,
        void* /* user_arg */)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        if (nghttp2_select_alpn(out, outlen, in, inlen) != 1)
        {
            log->critical("Failed to select ALPN proto!");
            return SSL_TLSEXT_ERR_NOACK;
        }

        return SSL_TLSEXT_ERR_OK;
    }

    void app_context::_init()
    {
        if (_creds)
        {
            _init_inbound(_creds->_keyfile.c_str(), _creds->_certfile.c_str());
            _init_outbound(_creds->_keyfile.c_str(), _creds->_certfile.c_str());
        }
        else
        {
            _init_inbound();
            _init_outbound();
        }
    }

    void app_context::_init_inbound()
    {
        log->debug("Creating inbound context using system certs...");
        _i.reset(SSL_CTX_new(TLS_server_method()));

        if (not _i)
            throw std::runtime_error{"Failed to create SSL context: {}"_format(detail::current_error())};

        X509_STORE* storage = SSL_CTX_get_cert_store(_i.get());

        if (X509_STORE_set_default_paths(storage) != 1)
            throw std::runtime_error{"Call to X509_STORE_set_default_paths failed: {}"_format(detail::current_error())};

        SSL_CTX_set_verify(_i.get(), SSL_VERIFY_PEER, nullptr);
        // SSL_CTX_set_cert_verify_callback(_ctx.get(), nullptr, nullptr);
    }

    void app_context::_init_inbound(const char* _keyfile, const char* _certfile)
    {
        log->debug("Creating inbound context using user key/cert...");
        _i.reset(SSL_CTX_new(TLS_server_method()));

        if (not _i)
            throw std::runtime_error{"Failed to create SSL context: {}"_format(detail::current_error())};

        SSL_CTX_set_options(
            _i.get(),
            SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION
                | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION | SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_TICKET
                | SSL_OP_CIPHER_SERVER_PREFERENCE);

        if (SSL_CTX_use_PrivateKey_file(_i.get(), _keyfile, SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error{"Failed to read private key file!"};

        if (SSL_CTX_use_certificate_file(_i.get(), _certfile, SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error{"Failed to read certificate file!"};

        if (SSL_CTX_check_private_key(_i.get()) != 1)
            throw std::runtime_error{"Failed to check private key!"};

        SSL_CTX_set_alpn_select_cb(_i.get(), ctx_callbacks::server_select_alpn_proto_cb, this);
    }

    void app_context::_init_outbound()
    {
        log->debug("Creating outbound context using system certs...");
        _o.reset(SSL_CTX_new(TLS_client_method()));

        if (not _o)
            throw std::runtime_error{"Failed to create SSL context: {}"_format(detail::current_error())};

        SSL_CTX_set_options(
            _o.get(),
            SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION
                | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION | SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_TICKET);

        // for client outbounds w/ no keys
        if (SSL_CTX_set_default_verify_paths(_o.get()) != 1)
            throw std::runtime_error{
                "Call to SSL_CTX_set_default_verify_paths failed: {}"_format(detail::current_error())};

        SSL_CTX_set_verify(_o.get(), SSL_VERIFY_PEER, nullptr);
    }

    void app_context::_init_outbound(const char* _keyfile, const char* _certfile)
    {
        log->debug("Creating outbound context using user key/cert...");
        _o.reset(SSL_CTX_new(TLS_client_method()));

        if (not _o)
            throw std::runtime_error{"Failed to create SSL context: {}"_format(detail::current_error())};

        SSL_CTX_set_options(
            _o.get(),
            SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION
                | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION | SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_TICKET);

        if (SSL_CTX_use_PrivateKey_file(_o.get(), _keyfile, SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error{"Failed to read private key file!"};

        if (SSL_CTX_use_certificate_file(_o.get(), _certfile, SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error{"Failed to read certificate file!"};

        if (SSL_CTX_check_private_key(_o.get()) != 1)
            throw std::runtime_error{"Failed to check private key!"};

        SSL_CTX_set_verify(_o.get(), SSL_VERIFY_PEER, nullptr);
    }
}  //  namespace wshttp
