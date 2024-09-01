#include "utils.hpp"

#include <catch2/catch_test_macros.hpp>

namespace wshttp::test
{
    TEST_CASE("001: Types", "[001][types]")
    {
        auto v4_base = ipv4(10, 0, 0, 0);
        auto v4_base_from_str = ipv4("10.0.0.0"s);

        CHECK(v4_base == v4_base_from_str);
        CHECK(v4_base.to_string() == "10.0.0.0"s);
        CHECK(v4_base_from_str.to_string() == "10.0.0.0"s);

        auto a_v6_base = ipv6(0x2001, 0xdb8, 0, 0, 0, 0, 0, 0);
        auto a_v6_base_from_str = ipv6("2001:db8::"s);

        CHECK(a_v6_base == a_v6_base_from_str);
        CHECK(a_v6_base.to_string() == "2001:db8::"s);
        CHECK(a_v6_base_from_str.to_string() == "2001:db8::"s);
    }

}  // namespace wshttp::test
