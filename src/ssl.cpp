#include "ssl.hpp"

#include "internal.hpp"

namespace wshttp
{
    namespace detail
    {
        const char* current_error()
        {
            return ERR_error_string(ERR_get_error(), nullptr);
        }

        void setup_ssl_library()
        {
            OPENSSL_init_ssl(0, NULL);
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
            OpenSSL_add_all_ciphers();
        }
    }  // namespace detail

    static constexpr auto default_sslopts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) | SSL_OP_NO_SSLv2 |
                                            SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_COMPRESSION |
                                            SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;

    static constexpr auto MIN_TLS_VERSION = /* TLS1_2_VERSION */ 0x0303;
    static constexpr auto MAX_TLS_VERSION = /* TLS1_3_VERSION */ 0x0304;

    // "Intermediate capability" cipherlist for TLS 1.2-1.3
    // https://wiki.mozilla.org/Security/Server_Side_TLS
    static constexpr auto TLS1_3_CIPHERS =
            "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256"sv;
    static constexpr auto TLS1_2_CIPHERS =
            "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-"
            "GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-"
            "AES256-GCM-SHA384"sv;

    static constexpr auto HTTP2_ALPNS = "\x2h25h2-165h2-14"_usp;
    static constexpr auto H2_ALPN = HTTP2_ALPNS.first<3>();
    static constexpr auto H2_16_ALPN = HTTP2_ALPNS.subspan<3, 6>();
    static constexpr auto H2_14_ALPN = HTTP2_ALPNS.subspan<9, 6>();
    static constexpr std::array<uspan, 3> H2_ALPNS{H2_ALPN, H2_16_ALPN, H2_14_ALPN};

    static auto next_sid = []() -> std::array<uint8_t, 2> {
        static std::unique_ptr<uint16_t> counter;
        if (not counter)
            counter = std::make_unique<uint16_t>();

        ++(*counter);
        return {static_cast<unsigned char>(*counter >> 8), static_cast<unsigned char>(*counter)};
    };

    static void set_sslopts(::SSL_CTX* ctx, bool outbound)
    {
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

        check_rv(SSL_CTX_set_cipher_list(ctx, TLS1_2_CIPHERS.data()), "Set TLSv1.2 ciphers");
        check_rv(SSL_CTX_set_ciphersuites(ctx, TLS1_3_CIPHERS.data()), "Set TLSv1.3 ciphers");

        auto session_id = next_sid();
        SSL_CTX_set_session_id_context(ctx, session_id.data(), session_id.size());
        SSL_CTX_set_session_cache_mode(ctx, outbound ? SSL_SESS_CACHE_CLIENT : SSL_SESS_CACHE_SERVER);
    }

    int ctx_callbacks::server_select_alpn_proto_cb(
            SSL*,
            const unsigned char** out,
            unsigned char* outlen,
            const unsigned char* in,
            unsigned int inlen,
            void* /* user_arg */)
    {
        unlog::trace("{} called", __PRETTY_FUNCTION__);

        for (const auto& a : H2_ALPNS)
        {
            unlog::trace("Seeking ALPN: {}", a);
            for (auto *curr = in, *end = in + inlen; curr + a.size() <= end; curr += *curr + 1)
            {
                if (uspan input{curr, curr + *curr + 1}; a == input)
                {
                    *out = curr + 1;
                    *outlen = *curr;
                    unlog::debug("Client alpn ({}) matched local alpn proto: {}", input, a);
                    return SSL_TLSEXT_ERR_OK;
                }
                else
                    unlog::trace("Did not find match for client alpn: {}", input);
            }
        }

        unlog::warn("Failed to select ALPN proto!");
        return SSL_TLSEXT_ERR_NOACK;
    }

    static constexpr size_t RSA_KEYSIZE{4096};
    static constexpr auto CERT_LIFETIME{31536000L};
    static constexpr auto CERT_COUNTRY = "US"_usp;
    static constexpr auto CERT_ORG = "wshttp"_usp;
    static constexpr auto CERT_CN = "localhost"_usp;

    x509_cert_keypair::x509_cert_keypair()
    {
        _init_internals();
    }

    void x509_cert_keypair::_init_internals()
    {
        pk.reset(EVP_RSA_gen(RSA_KEYSIZE));

        if (!pk)
            throw std::runtime_error{"Failed to create RSA keypair object: {}"_format(detail::current_error())};

        x.reset(X509_new());

        if (!x)
            throw std::runtime_error{"Failed to create X509 cert object: {}"_format(detail::current_error())};

        X509_set_version(x.get(), NID_X509);
        ASN1_INTEGER_set(X509_get_serialNumber(x.get()), 1);

        // one year lifespan
        X509_gmtime_adj(X509_getm_notBefore(x.get()), 0);
        X509_gmtime_adj(X509_getm_notAfter(x.get()), CERT_LIFETIME);

        X509_set_pubkey(x.get(), pk.get());

        X509_NAME* n = X509_get_subject_name(x.get());

        // country code
        X509_NAME_add_entry_by_txt(n, "C", MBSTRING_ASC, CERT_COUNTRY.data(), -1, -1, 0);
        X509_NAME_add_entry_by_txt(n, "O", MBSTRING_ASC, CERT_ORG.data(), -1, -1, 0);
        X509_NAME_add_entry_by_txt(n, "CN", MBSTRING_ASC, CERT_CN.data(), -1, -1, 0);

        // copy subject name to issuer to self-sign certificate
        X509_set_issuer_name(x.get(), n);

        if (!X509_sign(x.get(), pk.get(), EVP_sha512()))
            throw std::runtime_error{"Failed to sign X509 certificate: {}"_format(detail::current_error())};

        unlog::info("Self-signed X509 certificate created!");
    }

    cert_pk_file_pair::cert_pk_file_pair(const std::string_view& keyfile, const std::string_view& certfile) :
            key{keyfile}, cert{certfile}
    {
        if (key.is_relative())
            key = fs::absolute(key);
        if (cert.is_relative())
            cert = fs::absolute(cert);
    }

    ssl_creds::ssl_creds(const std::string_view& keyfile, const std::string_view& certfile) :
            storage{cert_pk_file_pair{keyfile, certfile}}
    {}

    void ssl_creds::configure_ssl_ctx(SSL_CTX* inbound, SSL_CTX* outbound)
    {
        if (storage.index())
        {
            // index != 0 -> variant holds generated x509_cert_keypair
            auto [cert, pk] = std::get<x509_cert_keypair>(storage).cert_keypair();
            assert(cert != nullptr && pk != nullptr);

            unlog::debug("Configuring inbound SSL context using generated self-signed X509...");

            check_rv(SSL_CTX_use_certificate(inbound, cert), "Inbound SSL CTX use X509 certificate");
            check_rv(SSL_CTX_use_PrivateKey(inbound, pk), "Inbound SSL CTX use EVP_PKEY private key");

            unlog::trace("Configuring outbound SSL context using generated self-signed X509...");

            check_rv(SSL_CTX_use_PrivateKey(outbound, pk), "Outbound SSL CTX use EVP_PKEY private key");
            check_rv(SSL_CTX_use_certificate(outbound, cert), "Outbound SSL CTX use X509 certificate");
        }
        else
        {
            // index == 0 -> variant holds user-provided cert/key file paths
            assert(storage.index() == 0);
            const auto& [certfile, keyfile] = std::get<cert_pk_file_pair>(storage).cert_keypair();

            unlog::debug("Configuring inbound SSL context using user-provided key/cert...");

            check_rv(
                    SSL_CTX_use_certificate_chain_file(inbound, certfile.c_str()),
                    "Inbound SSL CTX read cert chain file");
            check_rv(
                    SSL_CTX_use_PrivateKey_file(inbound, keyfile.c_str(), SSL_FILETYPE_PEM),
                    "Inbound SSL CTX read private key file");
            check_rv(SSL_CTX_check_private_key(inbound), "Inbound SSL CTX check private key");

            unlog::trace("Configuring outbound context using user-provided key/cert...");

            check_rv(
                    SSL_CTX_use_PrivateKey_file(outbound, keyfile.c_str(), SSL_FILETYPE_PEM),
                    "Outbound SSL CTX read private key file");
            check_rv(
                    SSL_CTX_use_certificate_file(outbound, certfile.c_str(), SSL_FILETYPE_PEM),
                    "Outbound SSL CTX read cert file");
            check_rv(SSL_CTX_load_verify_file(outbound, certfile.c_str()), "Outbound SSL CTX load verify file");
        }
    }

    app_context::app_context(std::shared_ptr<ssl_creds> c) :
            _creds{c}, _i{SSL_CTX_new(TLS_server_method())}, _o{SSL_CTX_new(TLS_client_method())}
    {
        if (!_i)
            throw std::runtime_error{"Failed to create inbound SSL context: {}"_format(detail::current_error())};
        if (!_o)
            throw std::runtime_error{"Failed to create outbound SSL context: {}"_format(detail::current_error())};

        _creds->configure_ssl_ctx(_i.get(), _o.get());

        _init_inbound();
        _init_outbound();
    }

    void app_context::_init_inbound()
    {
        auto* ctx = _i.get();
        assert(ctx);

        set_sslopts(ctx, false);

        // SSL_CTX_set_alpn_select_cb(ctx, ctx_callbacks::server_select_alpn_proto_cb, this);
    }

    void app_context::_init_outbound()
    {
        auto* ctx = _o.get();
        assert(ctx);

        set_sslopts(ctx, true);

        // initialize default system certificate store for verification
        check_rv(SSL_CTX_set_default_verify_paths(ctx), "SSL CTX set default verify paths");

        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_alpn_protos(ctx, HTTP2_ALPNS.data(), HTTP2_ALPNS.size());
    }
}  //  namespace wshttp
