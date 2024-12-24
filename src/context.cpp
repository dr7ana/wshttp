#include "context.hpp"

#include "internal.hpp"

namespace wshttp
{
    static constexpr auto default_sslopts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) | SSL_OP_NO_SSLv2 |
                                            SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
                                            SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;

    static constexpr auto MIN_TLS_VERSION = /* TLS1_2_VERSION */ 0x0303;
    static constexpr auto MAX_TLS_VERSION = /* TLS1_3_VERSION */ 0x0304;

    // "Intermediate capability" cipherlist for TLS 1.2-1.3
    // https://wiki.mozilla.org/Security/Server_Side_TLS
    static constexpr auto TLS_CIPHERS =
            "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-"
            "GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-"
            "AES256-GCM-SHA384"sv;

    static constexpr auto HTTP2_ALPNS = "\x2h25h2-165h2-14"_usp;
    static constexpr auto H2_ALPN = HTTP2_ALPNS.first<3>();
    static constexpr auto H2_16_ALPN = HTTP2_ALPNS.subspan<3, 6>();
    static constexpr auto H2_14_ALPN = HTTP2_ALPNS.subspan<9, 6>();
    static constexpr std::array<uspan, 3> H2_ALPN_ARR{H2_ALPN, H2_16_ALPN, H2_14_ALPN};

    static auto next_sid = []() -> std::array<uint8_t, 2> {
        static std::unique_ptr<uint16_t> counter;
        if (not counter)
            counter = std::make_unique<uint16_t>();

        ++(*counter);
        return {static_cast<unsigned char>(*counter >> 8), static_cast<unsigned char>(*counter)};
    };

    static int check_rv(int rv, std::string_view action, int expected = 1)
    {
        std::optional<std::error_code> ec;

        if (rv != expected)
            ec.emplace(errno, std::system_category());

        if (ec)
        {
            log->error("Error code {} ({}) returned during {}", ec->value(), ec->message(), action);
            throw std::system_error{*ec};
        }

        return rv;
    }

    int ctx_callbacks::server_select_alpn_proto_cb(
            SSL*,
            const unsigned char** out,
            unsigned char* outlen,
            const unsigned char* in,
            unsigned int inlen,
            void* /* user_arg */)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        for (auto& a : H2_ALPN_ARR)
        {
            for (auto curr = in; curr != in + inlen; curr += *curr + 1)
            {
                if (a == uspan{curr, curr + *curr})
                {
                    *out = curr + 1;
                    *outlen = *curr;
                    log->debug("Client list matched local alpn proto: {}", a);
                    return SSL_TLSEXT_ERR_OK;
                }
            }
        }

        log->info("Failed to select ALPN proto!");
        return SSL_TLSEXT_ERR_NOACK;
    }

    void app_context::_init()
    {
        _init_inbound();
        _init_outbound();
    }

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

        check_rv(
                SSL_CTX_set_min_proto_version(ctx, MIN_TLS_VERSION) &&
                        SSL_CTX_set_max_proto_version(ctx, MAX_TLS_VERSION),
                "Set TLS min/max proto");

        check_rv(SSL_CTX_set_cipher_list(ctx, TLS_CIPHERS.data()), "Set TLS ciphers");

        auto session_id = next_sid();
        SSL_CTX_set_session_id_context(ctx, session_id.data(), session_id.size());
        SSL_CTX_set_session_cache_mode(ctx, outbound ? SSL_SESS_CACHE_CLIENT : SSL_SESS_CACHE_SERVER);
    }

    void app_context::_init_inbound()
    {
        log->debug("Creating inbound context...");
        _i.reset(SSL_CTX_new(TLS_server_method()));

        if (not _i)
            throw std::runtime_error{"Failed to create SSL context: {}"_format(detail::current_error())};

        auto* ctx = _i.get();

        _set_sslopts(false);

        if (_creds)
        {
            log->debug("Configuring inbound context using user key/cert...");

            check_rv(
                    SSL_CTX_use_PrivateKey_file(ctx, _creds->_keyfile.c_str(), SSL_FILETYPE_PEM),
                    "SSL CTX read private key file");
            check_rv(
                    SSL_CTX_use_certificate_chain_file(ctx, _creds->_certfile.c_str()), "SSL CTX read cert chain file");
            check_rv(SSL_CTX_check_private_key(ctx), "SSL CTX check private key");
        }
        else
        {
            log->debug("Configuring inbound context using system certs...");

            X509_STORE* storage = SSL_CTX_get_cert_store(ctx);
            check_rv(X509_STORE_set_default_paths(storage), "Set x509 store default paths");
        }

        SSL_CTX_set_alpn_select_cb(ctx, ctx_callbacks::server_select_alpn_proto_cb, this);
    }

    void app_context::_init_outbound()
    {
        log->debug("Creating outbound context...");
        _o.reset(SSL_CTX_new(TLS_client_method()));

        if (not _o)
            throw std::runtime_error{"Failed to create SSL context: {}"_format(detail::current_error())};

        auto* ctx = _o.get();

        _set_sslopts(true);

        if (_creds)
        {
            log->debug("Configuring outbound context using user key/cert...");
            check_rv(
                    SSL_CTX_use_PrivateKey_file(ctx, _creds->_keyfile.c_str(), SSL_FILETYPE_PEM),
                    "SSL CTX read private key file");
            check_rv(
                    SSL_CTX_use_certificate_file(ctx, _creds->_certfile.c_str(), SSL_FILETYPE_PEM),
                    "SSL CTX read cert file");
        }
        else
        {
            log->debug("Configuring outbound context using system certs...");
            // use default system certificate store for verification
            check_rv(SSL_CTX_set_default_verify_paths(ctx), "SSL CTX set default verify paths");
        }

        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_alpn_protos(ctx, HTTP2_ALPNS.data(), HTTP2_ALPNS.size());
    }
}  //  namespace wshttp
