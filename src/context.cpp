#include "context.hpp"

#include "internal.hpp"

namespace wshttp
{
    static constexpr auto KEY_FILE_PLACEHOLDER = ""sv;
    static constexpr auto CERT_FILE_PLACEHOLDER = ""sv;

    std::unique_ptr<IOContext> IOContext::make(const std::string& keyfile, const std::string& certfile)
    {
        try
        {
            return std::unique_ptr<IOContext>{new IOContext{keyfile, certfile}};
        }
        catch (const std::exception& e)
        {
            log->critical("IOContext exception: {}", e.what());
            throw;  // TODO: this is OK for now
        }
    }

    IOContext::IOContext(const std::string& keyfile, const std::string& certfile)
        : _keyfile{keyfile}, _certfile{certfile}
    {
        if (_keyfile.empty() or _certfile.empty())
            throw std::invalid_argument{"Empty paths"};
    }

    std::unique_ptr<app_context> app_context::make(IOContext::IO _d, const fs::path& _key, const fs::path& _cert)
    {
        return std::unique_ptr<app_context>{new app_context{_d, _key, _cert}};
    }

    app_context::app_context(IOContext::IO _d, const fs::path& _key, const fs::path& _cert) : _dir{_d}
    {
        _dir ? _init_outbound(_key.c_str(), _cert.c_str()) : _init_inbound(_key.c_str(), _cert.c_str());
    }

    void app_context::_init_inbound(const char* _keyfile, const char* _certfile)
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
        if (SSL_CTX_use_PrivateKey_file(ssl, KEY_FILE_PLACEHOLDER.data(), SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error{"Failed to read private key file!"};

        // if (SSL_CTX_use_certificate_chain_file(ssl, _certfile) != 1)
        if (SSL_CTX_use_certificate_chain_file(ssl, CERT_FILE_PLACEHOLDER.data()) != 1)
            throw std::runtime_error{"Failed to read certificate file!"};

        SSL_CTX_set_alpn_select_cb(ssl, callbacks::server_select_alpn_proto_cb, NULL);

        _ctx.reset(ssl);
        (void)_keyfile;
        (void)_certfile;
    }

    void app_context::_init_outbound(const char* _keyfile, const char* _certfile)
    {
        (void)_keyfile;
        (void)_certfile;
    }
}  //  namespace wshttp
