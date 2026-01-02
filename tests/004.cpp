#include "utils.hpp"

#include <catch2/catch_test_macros.hpp>

namespace wshttp::test {
    TEST_CASE("004: Endpoint session creation", "[004][endpoint][session]") {
        auto creds = ssl_creds::make();
        auto ep = endpoint::make(creds);
        REQUIRE(ep);

        SECTION("Valid session creation") {
            auto session = ep->initiate_session("https://example.com");
            REQUIRE(session);
        }

        SECTION("Duplicate session creation is rejected") {
            auto session = ep->initiate_session("https://example.com");
            REQUIRE(session);

            auto duplicate = ep->initiate_session("https://example.com");
            CHECK_FALSE(duplicate);
        }

        SECTION("Invalid session creation fails") {
            auto session = ep->initiate_session("not a url");
            CHECK_FALSE(session);
        }
    }

    TEST_CASE("004: Endpoint request validation", "[004][endpoint][request]") {
        auto creds = ssl_creds::make();
        auto ep = endpoint::make(creds);
        REQUIRE(ep);

        CHECK_FALSE(ep->request("not a url", METHOD::GET));
    }
}  // namespace wshttp::test
