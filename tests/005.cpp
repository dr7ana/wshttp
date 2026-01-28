#include "utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace wshttp::test {
    TEST_CASE("005: Request opts body and content type", "[005][request][opts]") {
        SECTION("String body uses content type") {
            request_opts opts{content_type::JSON, "payload"sv};

            REQUIRE(opts.media_type);
            CHECK(*opts.media_type == content_type::JSON);
            REQUIRE(opts.body);
            CHECK(opts.body->size() == 7);
            CHECK(std::string{opts.body->begin(), opts.body->end()} == "payload");
        }

        SECTION("Vector body is preserved") {
            std::vector<char> buf{'a', 'b', 'c'};
            request_opts opts{std::move(buf)};

            REQUIRE(opts.body);
            CHECK(opts.body->size() == 3);
            CHECK((*opts.body)[0] == 'a');
            CHECK((*opts.body)[1] == 'b');
            CHECK((*opts.body)[2] == 'c');
        }

        SECTION("Callback and body can coexist") {
            bool invoked = false;
            request_opts opts{
                    content_type::PLAIN, request_data_cb{[&invoked](std::vector<char>) { invoked = true; }}, "ok"sv};

            REQUIRE(opts.media_type);
            CHECK(*opts.media_type == content_type::PLAIN);
            CHECK(opts.data_cb.has_value());
            REQUIRE(opts.body);
            CHECK(opts.body->size() == 2);
            CHECK(std::string{opts.body->begin(), opts.body->end()} == "ok");
            CHECK_FALSE(invoked);
        }

        SECTION("Accept and content type are independent") {
            request_opts opts{content_type::JSON, accept_type{content_type::HTML}, user_agent{"ua/1.0"}};

            REQUIRE(opts.media_type);
            CHECK(*opts.media_type == content_type::JSON);
            REQUIRE(opts.accept);
            CHECK(*opts.accept == content_type::HTML);
            REQUIRE(opts.ua);
            CHECK(*opts.ua == "ua/1.0");
        }

        SECTION("Accept without content type does not set media_type") {
            request_opts opts{accept_type{content_type::PLAIN}};

            CHECK_FALSE(opts.media_type.has_value());
            REQUIRE(opts.accept);
            CHECK(*opts.accept == content_type::PLAIN);
        }
    }

    TEST_CASE("005: Session opts connection settings", "[005][session][opts]") {
        using namespace std::chrono_literals;

        session_opts opts{
                content_type::XML,
                accept_type{content_type::JSON},
                user_agent{"wshttp-test/1.0"},
                connection_timeout{5s},
                retry_count{3},
                ip_family::IPV4};

        REQUIRE(opts.media_type);
        CHECK(*opts.media_type == content_type::XML);
        REQUIRE(opts.accept);
        CHECK(*opts.accept == content_type::JSON);
        REQUIRE(opts.ua);
        CHECK(*opts.ua == "wshttp-test/1.0");
        REQUIRE(opts.timeout);
        CHECK(opts.timeout->count() == 5);
        REQUIRE(opts.retries);
        CHECK(*opts.retries == 3);
        REQUIRE(opts.family);
        CHECK(*opts.family == ip_family::IPV4);
    }

    TEST_CASE("005: Invalid session opts do not block session creation", "[005][session][opts]") {
        using namespace std::chrono_literals;

        auto creds = ssl_creds::make();
        auto ep = endpoint::make(creds);
        REQUIRE(ep);

        session_opts opts{connection_timeout{std::chrono::seconds{-5}}, retry_count{-2}, static_cast<ip_family>(255)};

        auto session = ep->initiate_session("https://example.com", std::move(opts));
        REQUIRE(session);
    }
}  // namespace wshttp::test
