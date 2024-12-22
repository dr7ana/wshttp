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

    static constexpr auto default_sslopts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) | SSL_OP_NO_SSLv2 |
                                            SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
                                            SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;

    static constexpr auto MIN_TLS_VERSION = /* TLS1_2_VERSION */ 0x0303;
    static constexpr auto MAX_TLS_VERSION = /* TLS1_3_VERSION */ 0x0304;

    // "Intermediate capability" cipherlist for TLS 1.2-1.3
    // https://wiki.mozilla.org/Security/Server_Side_TLS
    static constexpr auto DEFAULT_TLS_CIPHERS =
            "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-"
            "GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-"
            "AES256-GCM-SHA384"sv;

    static constexpr auto SESSION_ID_ROOT = "wshttp-"_usp;

    static std::array<uint8_t, 7> sid_base{
            enc::bit_cast<uint8_t>('w'),
            enc::bit_cast<uint8_t>('s'),
            enc::bit_cast<uint8_t>('h'),
            enc::bit_cast<uint8_t>('t'),
            enc::bit_cast<uint8_t>('p'),
            enc::bit_cast<uint8_t>('-'),
            uint8_t{}};

    static std::array<uint8_t, 7> _sid_base{
            {enc::bit_cast<uint8_t>('w'),
             enc::bit_cast<uint8_t>('s'),
             enc::bit_cast<uint8_t>('h'),
             enc::bit_cast<uint8_t>('t'),
             enc::bit_cast<uint8_t>('p'),
             enc::bit_cast<uint8_t>('-'),
             uint8_t{}}};

    static constexpr auto next_sid_ = []() -> uspan {
        static std::unique_ptr<uint8_t> sid;

        if (not sid)
            sid = std::make_unique<uint8_t>();

        std::span<unsigned char> writeable{_sid_base};
        writeable[7] = ++(*sid);

        return sid_base;
    };

    // static constexpr auto next_sid = []() -> std::array<uint8_t, 7> {
    //     static std::unique_ptr<uint8_t> sid;
    //     if (not sid)
    //         sid = std::make_unique<uint8_t>();

    //     return {enc::bit_cast<uint8_t>('w'),
    //             enc::bit_cast<uint8_t>('s'),
    //             enc::bit_cast<uint8_t>('h'),
    //             enc::bit_cast<uint8_t>('t'),
    //             enc::bit_cast<uint8_t>('p'),
    //             enc::bit_cast<uint8_t>('-'),
    //             ++(*sid)};
    // };

    void app_context::_set_sslopts(bool outbound)
    {
        auto* ctx = outbound ? _o.get() : _i.get();
        assert(ctx != nullptr);

        SSL_CTX_set_options(
                ctx,
                outbound ? default_sslopts
                         : default_sslopts | SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_TICKET |
                                   SSL_OP_CIPHER_SERVER_PREFERENCE);

        SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
        SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS);

        if (SSL_CTX_set_min_proto_version(ctx, MIN_TLS_VERSION) != 1 ||
            SSL_CTX_set_max_proto_version(ctx, MAX_TLS_VERSION) != 1)
            throw std::runtime_error{"Failed to set SSL CTX min/max TLS protocols!"};

        if (SSL_CTX_set_cipher_list(ctx, DEFAULT_TLS_CIPHERS.data()) != 1)
            throw std::runtime_error{"Failed to set SSL CTX TLS ciphers!"};

        auto session_id = next_sid_();
        SSL_CTX_set_session_id_context(ctx, session_id.data(), session_id.size());
        SSL_CTX_set_session_cache_mode(ctx, outbound ? SSL_SESS_CACHE_CLIENT : SSL_SESS_CACHE_SERVER);

        log->critical("current session id: {}", uspan{session_id});
    }

    void app_context::_init_inbound()
    {
        log->debug("Creating inbound context using system certs...");
        _i.reset(SSL_CTX_new(TLS_server_method()));

        if (not _i)
            throw std::runtime_error{"Failed to create SSL context: {}"_format(detail::current_error())};

        _set_sslopts(false);

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

        _set_sslopts(false);

        if (SSL_CTX_use_PrivateKey_file(_i.get(), _keyfile, SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error{"Failed to read private key file!"};

        if (SSL_CTX_use_certificate_chain_file(_i.get(), _certfile) != 1)
            throw std::runtime_error{"Failed to read certificate chain file!"};

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

        _set_sslopts(true);

        // abort handshake if cert verifiation fails
        SSL_CTX_set_verify(_o.get(), SSL_VERIFY_PEER, nullptr);

        // use default system certificate store for verification
        if (SSL_CTX_set_default_verify_paths(_o.get()) != 1)
            throw std::runtime_error{
                    "Call to SSL_CTX_set_default_verify_paths failed: {}"_format(detail::current_error())};
    }

    void app_context::_init_outbound(const char* _keyfile, const char* _certfile)
    {
        log->debug("Creating outbound context using user key/cert...");
        _o.reset(SSL_CTX_new(TLS_client_method()));

        if (not _o)
            throw std::runtime_error{"Failed to create SSL context: {}"_format(detail::current_error())};

        _set_sslopts(true);

        if (SSL_CTX_use_PrivateKey_file(_o.get(), _keyfile, SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error{"Failed to read private key file!"};

        if (SSL_CTX_use_certificate_file(_o.get(), _certfile, SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error{"Failed to read certificate file!"};

        if (SSL_CTX_check_private_key(_o.get()) != 1)
            throw std::runtime_error{"Failed to check private key!"};

        SSL_CTX_set_verify(_o.get(), SSL_VERIFY_PEER, nullptr);
    }
}  //  namespace wshttp
