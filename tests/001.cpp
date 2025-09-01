#include "utils.hpp"

#include <catch2/catch_test_macros.hpp>

namespace wshttp::test
{
    TEST_CASE("001: Endian Flipping", "[001][endian]")
    {
        SECTION("Native <-> little endian")
        {
            constexpr uint8_t constexpr_u8 = 0x01;
            constexpr uint16_t constexpr_u16 = 0x0123;
            constexpr uint32_t constexpr_u32 = 0x01234567;
            constexpr uint64_t constexpr_u64 = 0x0123456789abcdef;

            constexpr uint8_t constexpr_u8_little = enc::host_to_little(constexpr_u8);
            constexpr uint16_t constexpr_u16_little = enc::host_to_little(constexpr_u16);
            constexpr uint32_t constexpr_u32_little = enc::host_to_little(constexpr_u32);
            constexpr uint64_t constexpr_u64_little = enc::host_to_little(constexpr_u64);

            uint8_t u8 = 0x01;
            uint16_t u16 = 0x0123;
            uint32_t u32 = 0x01234567;
            uint64_t u64 = 0x0123456789abcdef;

            constexpr uint8_t u8_little = 0x01;
            constexpr uint16_t u16_little = enc::little_endian ? 0x0123 : 0x2301;
            constexpr uint32_t u32_little = enc::little_endian ? 0x01234567 : 0x67452301;
            constexpr uint64_t u64_little = enc::little_endian ? 0x0123456789abcdef : 0xefcdab8967452301;

            CHECK(enc::host_to_little(u8) == constexpr_u8_little);
            CHECK(enc::host_to_little(u16) == constexpr_u16_little);
            CHECK(enc::host_to_little(u32) == constexpr_u32_little);
            CHECK(enc::host_to_little(u64) == constexpr_u64_little);

            CHECK(constexpr_u8_little == u8_little);
            CHECK(constexpr_u16_little == u16_little);
            CHECK(constexpr_u32_little == u32_little);
            CHECK(constexpr_u64_little == u64_little);

            REQUIRE(u8 == 0x01);
            REQUIRE(u16 == 0x0123);
            REQUIRE(u32 == 0x01234567);
            REQUIRE(u64 == 0x0123456789abcdef);

            enc::host_to_little_inplace(u8);
            enc::host_to_little_inplace(u16);
            enc::host_to_little_inplace(u32);
            enc::host_to_little_inplace(u64);

            CHECK(u8 == u8_little);
            CHECK(u16 == u16_little);
            CHECK(u32 == u32_little);
            CHECK(u64 == u64_little);

            CHECK(enc::little_to_host(u8) == 0x01);
            CHECK(enc::little_to_host(u16) == 0x0123);
            CHECK(enc::little_to_host(u32) == 0x01234567);
            CHECK(enc::little_to_host(u64) == 0x0123456789abcdef);

            enc::little_to_host_inplace(u8);
            enc::little_to_host_inplace(u16);
            enc::little_to_host_inplace(u32);
            enc::little_to_host_inplace(u64);

            CHECK(u8 == 0x01);
            CHECK(u16 == 0x0123);
            CHECK(u32 == 0x01234567);
            CHECK(u64 == 0x0123456789abcdef);
        }

        SECTION("Native <-> big endian")
        {
            constexpr uint8_t constexpr_u8 = 0x01;
            constexpr uint16_t constexpr_u16 = 0x0123;
            constexpr uint32_t constexpr_u32 = 0x01234567;
            constexpr uint64_t constexpr_u64 = 0x0123456789abcdef;

            constexpr uint8_t constexpr_u8_big = enc::host_to_big(constexpr_u8);
            constexpr uint16_t constexpr_u16_big = enc::host_to_big(constexpr_u16);
            constexpr uint32_t constexpr_u32_big = enc::host_to_big(constexpr_u32);
            constexpr uint64_t constexpr_u64_big = enc::host_to_big(constexpr_u64);

            uint8_t u8 = 0x01;
            uint16_t u16 = 0x0123;
            uint32_t u32 = 0x01234567;
            uint64_t u64 = 0x0123456789abcdef;

            constexpr uint8_t u8_big = 0x01;
            constexpr uint16_t u16_big = enc::big_endian ? 0x0123 : 0x2301;
            constexpr uint32_t u32_big = enc::big_endian ? 0x01234567 : 0x67452301;
            constexpr uint64_t u64_big = enc::big_endian ? 0x0123456789abcdef : 0xefcdab8967452301;

            CHECK(enc::host_to_big(u8) == constexpr_u8_big);
            CHECK(enc::host_to_big(u16) == constexpr_u16_big);
            CHECK(enc::host_to_big(u32) == constexpr_u32_big);
            CHECK(enc::host_to_big(u64) == constexpr_u64_big);

            CHECK(constexpr_u8_big == u8_big);
            CHECK(constexpr_u16_big == u16_big);
            CHECK(constexpr_u32_big == u32_big);
            CHECK(constexpr_u64_big == u64_big);

            REQUIRE(u8 == 0x01);
            REQUIRE(u16 == 0x0123);
            REQUIRE(u32 == 0x01234567);
            REQUIRE(u64 == 0x0123456789abcdef);

            enc::host_to_big_inplace(u8);
            enc::host_to_big_inplace(u16);
            enc::host_to_big_inplace(u32);
            enc::host_to_big_inplace(u64);

            CHECK(u8 == u8_big);
            CHECK(u16 == u16_big);
            CHECK(u32 == u32_big);
            CHECK(u64 == u64_big);

            CHECK(enc::big_to_host(u8) == 0x01);
            CHECK(enc::big_to_host(u16) == 0x0123);
            CHECK(enc::big_to_host(u32) == 0x01234567);
            CHECK(enc::big_to_host(u64) == 0x0123456789abcdef);

            enc::big_to_host_inplace(u8);
            enc::big_to_host_inplace(u16);
            enc::big_to_host_inplace(u32);
            enc::big_to_host_inplace(u64);

            CHECK(u8 == 0x01);
            CHECK(u16 == 0x0123);
            CHECK(u32 == 0x01234567);
            CHECK(u64 == 0x0123456789abcdef);
        }
    }

    TEST_CASE("001: Address Types", "[001][address][types]")
    {
        auto v4_str = "10.0.0.0"s;

        auto v4_base = ipv4(10, 0, 0, 0);
        auto v4_base_from_str = ipv4("10.0.0.0"s);

        CHECK(v4_base == v4_base_from_str);
        CHECK(v4_base.to_string() == "10.0.0.0"s);
        CHECK(v4_base_from_str.to_string() == "10.0.0.0"s);

        auto v6_base = ipv6(0x2001, 0xdb8, 0, 0, 0, 0, 0, 0);
        auto v6_base_from_str = ipv6("2001:db8::"s);

        CHECK(v6_base == v6_base_from_str);
        CHECK(v6_base.to_string() == "2001:db8::"s);
        CHECK(v6_base_from_str.to_string() == "2001:db8::"s);

        sockaddr_in in{};
        in.sin_addr = v4_base.to_inaddr();
        auto v4_from_saddr = ipv4{&in};
        CHECK(v4_base == v4_from_saddr);

        sockaddr_in6 in6{};
        in6.sin6_addr = v6_base.to_in6addr();
        auto v6_from_saddr = ipv6{&in6};

        CHECK(v6_base.to_string() == v6_from_saddr.to_string());
        CHECK(v6_base == v6_from_saddr);

        constexpr ipv4 ipv4_anyaddr(0, 0, 0, 0);
        constexpr ipv4 ipv4_not_anyaddr(192, 168, 1, 1);

        CHECK(ipv4_anyaddr.is_anyaddr());
        CHECK(!ipv4_not_anyaddr.is_anyaddr());

        ip_address v4_anyaddr{ipv4_anyaddr, 0};
        ip_address v4_not_anyaddr{ipv4_not_anyaddr, 0};

        CHECK(v4_anyaddr.is_anyaddr());
        CHECK(!v4_not_anyaddr.is_anyaddr());
    }

    TEST_CASE("001: Endpoint Creation", "[001][endoing]")
    {
        auto creds = ssl_creds::make();

        SECTION("Endpoint owns its event loop")
        {
            auto ep = endpoint::make(creds);
            REQUIRE(ep);
        }
        // SECTION("Application owns event loop")
        // {
        //     auto loop = event_loop::make();
        //     REQUIRE(loop);

        //     auto ep = endpoint::make(loop, creds);
        //     REQUIRE(ep);
        // }
    }
}  // namespace wshttp::test
