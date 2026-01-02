#include "utils.hpp"

#include <catch2/catch_test_macros.hpp>

namespace wshttp::test {
    TEST_CASE("002: URI parsing", "[002][uri]") {
        SECTION("HTTPS default port and path") {
            auto parsed = uri::make("https://example.com/path?x=1");
            REQUIRE(parsed);

            CHECK(parsed->scheme() == "https"sv);
            CHECK(parsed->host() == "example.com"sv);
            CHECK(parsed->port() == 443);
            CHECK(parsed->pathquery() == "/path?x=1"sv);
        }

        SECTION("HTTP default port") {
            auto parsed = uri::make("http://example.com");
            REQUIRE(parsed);

            CHECK(parsed->scheme() == "http"sv);
            CHECK(parsed->port() == 80);
        }

        SECTION("Path updates") {
            auto parsed = uri::make("https://example.com/initial");
            REQUIRE(parsed);

            parsed->set_path("/next");
            CHECK(parsed->pathquery() == "/next"sv);
        }

        SECTION("Complex query strings") {
            auto parsed = uri::make("https://example.com/search?q=hello%20world&lang=en&sort=asc");
            REQUIRE(parsed);

            CHECK(parsed->pathquery() == "/search?q=hello%20world&lang=en&sort=asc"sv);
        }

        SECTION("Query with encoded delimiters") {
            auto parsed = uri::make("https://example.com/api?tags=a%2Cb%2Cc&filter=name%3Dtest%26status%3Dactive");
            REQUIRE(parsed);

            CHECK(parsed->pathquery() == "/api?tags=a%2Cb%2Cc&filter=name%3Dtest%26status%3Dactive"sv);
        }

        SECTION("Query with empty values") {
            auto parsed = uri::make("https://example.com/lookup?empty=&flag&name=test");
            REQUIRE(parsed);

            CHECK(parsed->pathquery() == "/lookup?empty=&flag&name=test"sv);
        }

        SECTION("Unsupported scheme fails") {
            auto parsed = uri::make("ftp://example.com");
            CHECK_FALSE(parsed);
        }
    }
}  // namespace wshttp::test
