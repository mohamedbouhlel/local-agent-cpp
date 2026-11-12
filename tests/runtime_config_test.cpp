#include "../src/runtime_config.hpp"
#include "test_support.hpp"

TEST(agent, positive_seconds_config_accepts_only_complete_positive_integers)
{
    const auto valid = agent::config::parse_positive_seconds("20");
    CHECK(valid.has_value());
    if (valid) CHECK_EQ(*valid, 20L);

    CHECK(!agent::config::parse_positive_seconds("20seconds").has_value());
    CHECK(!agent::config::parse_positive_seconds("20s").has_value());
    CHECK(!agent::config::parse_positive_seconds("20 ").has_value());
    CHECK(!agent::config::parse_positive_seconds(" 20").has_value());
    CHECK(!agent::config::parse_positive_seconds("").has_value());
    CHECK(!agent::config::parse_positive_seconds("0").has_value());
    CHECK(!agent::config::parse_positive_seconds("-1").has_value());
    CHECK(!agent::config::parse_positive_seconds("999999999999999999999999999999999999").has_value());
}
