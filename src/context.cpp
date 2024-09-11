#include "context.hpp"

#include "internal.hpp"

namespace wshttp
{
    int ctx_callbacks::server_select_alpn_proto_cb(
        SSL *, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg)
    {
        (void)arg;  // user argument
        if (nghttp2_select_alpn(out, outlen, in, inlen) != 1)
        {
            log->critical("Failed to select ALPN proto!");
            return SSL_TLSEXT_ERR_NOACK;
        }

        return SSL_TLSEXT_ERR_OK;
    }

    void app_context::handle_io_opt(std::shared_ptr<ssl_creds> c)
    {
        log->debug("app_context emplacing ssl creds...");
        _creds = std::move(c);
    }

    void app_context::_init()
    {
        switch (_dir)
        {
            case IO::INBOUND:
                log->info("Creating inbound context...");
                return _init_outbound(_creds->_keyfile.c_str(), _creds->_certfile.c_str());
            case IO::OUTBOUND:
                log->info("Creating outbound context...");
                return _init_inbound(_creds->_keyfile.c_str(), _creds->_certfile.c_str());
        }
    }

    void app_context::_init_inbound(const char *_keyfile, const char *_certfile)
    {
        auto ssl = SSL_CTX_new(TLS_server_method());

        if (not ssl)
            throw std::runtime_error{
                "Failed to create SSL Context: {}"_format(ERR_error_string(ERR_get_error(), NULL))};

        SSL_CTX_set_options(
            ssl,
            SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION
                | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

        if (SSL_CTX_set1_curves_list(ssl, "P-256") != 1)
            throw std::runtime_error{
                "Failed to set SSLL curves list: {}"_format(ERR_error_string(ERR_get_error(), NULL))};

        // if (SSL_CTX_use_PrivateKey_file(ssl, _keyfile, SSL_FILETYPE_PEM) != 1)
        if (SSL_CTX_use_PrivateKey_file(ssl, _keyfile, SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error{"Failed to read private key file!"};

        // if (SSL_CTX_use_certificate_chain_file(ssl, _certfile) != 1)
        if (SSL_CTX_use_certificate_chain_file(ssl, _certfile) != 1)
            throw std::runtime_error{"Failed to read certificate file!"};

        SSL_CTX_set_alpn_select_cb(ssl, ctx_callbacks::server_select_alpn_proto_cb, NULL);

        _ctx.reset(ssl);
    }

    void app_context::_init_outbound(const char *_keyfile, const char *_certfile)
    {
        (void)_keyfile;
        (void)_certfile;
    }
}  //  namespace wshttp
