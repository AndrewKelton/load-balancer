#include <gtest/gtest.h>

#include "load_balancer.hpp"

TEST(ServerHealth) {

    EXPECT_EQ(load_balancer::server_health_status_to_string(ServerHealthStatus::OK), "OK");
    EXPECT_EQ(load_balancer::server_health_status_to_string(ServerHealthStatus::DEGRADED), "DEGRADED");
    EXPECT_EQ(load_balancer::server_health_status_to_string(ServerHealthStatus::FAILED), "FAILED");
}   