#include "utils.hpp"

#include <catch2/catch_test_macros.hpp>

namespace wshttp::test {
    namespace {
        inline constexpr auto kMinTlsVersion = 0x0303;  // TLS 1.2
        inline constexpr auto kMaxTlsVersion = 0x0304;  // TLS 1.3
    }  // namespace

    TEST_CASE("003: SSL credentials", "[003][ssl]") {
        auto creds = ssl_creds::make();
        REQUIRE(creds);

        auto ctx = app_context::make(creds);
        REQUIRE(ctx);

        SECTION("Contexts are allocated") {
            CHECK(ctx->I() != nullptr);
            CHECK(ctx->O() != nullptr);
        }

        SECTION("Protocol versions are constrained") {
            CHECK(SSL_CTX_get_min_proto_version(ctx->I()) == kMinTlsVersion);
            CHECK(SSL_CTX_get_max_proto_version(ctx->I()) == kMaxTlsVersion);
            CHECK(SSL_CTX_get_min_proto_version(ctx->O()) == kMinTlsVersion);
            CHECK(SSL_CTX_get_max_proto_version(ctx->O()) == kMaxTlsVersion);
        }

        SECTION("Verify mode differs for inbound vs outbound") {
            CHECK((SSL_CTX_get_verify_mode(ctx->I()) & SSL_VERIFY_PEER) == 0);
            CHECK((SSL_CTX_get_verify_mode(ctx->O()) & SSL_VERIFY_PEER) != 0);
        }

        SECTION("TLS hardening options are enabled") {
            auto inbound_opts = SSL_CTX_get_options(ctx->I());
            auto outbound_opts = SSL_CTX_get_options(ctx->O());

            CHECK((inbound_opts & SSL_OP_NO_SSLv3) != 0);
            CHECK((inbound_opts & SSL_OP_NO_TLSv1_1) != 0);
            CHECK((inbound_opts & SSL_OP_NO_COMPRESSION) != 0);

            CHECK((outbound_opts & SSL_OP_NO_SSLv3) != 0);
            CHECK((outbound_opts & SSL_OP_NO_TLSv1_1) != 0);
            CHECK((outbound_opts & SSL_OP_NO_COMPRESSION) != 0);
        }

        SECTION("Cipher configuration is present") {
            const auto* inbound_ciphers = SSL_CTX_get_ciphers(ctx->I());
            const auto* outbound_ciphers = SSL_CTX_get_ciphers(ctx->O());

            CHECK(inbound_ciphers != nullptr);
            CHECK(outbound_ciphers != nullptr);
        }
    }
}  // namespace wshttp::test
