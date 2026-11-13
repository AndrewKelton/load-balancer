#include <gtest/gtest.h>

#include "strategies/least_connections.hpp"
#include "utils.hpp"

#include <thread>
#include <vector>
#include <set>
#include <optional>

#define TEST_NUM_SERVERS 3

TEST(LeastConnectionsDistribution, LeastConnectionsDistributesCorrectlyServersHealthy) {

    LeastConnections least_connections;

    std::vector<server_info_t> servers;
    servers = std::vector<server_info_t>(TEST_NUM_SERVERS);
    for(int i = 0; i < TEST_NUM_SERVERS; ++i) {
        servers[i].port = 9000 + i;
        servers[i].status.store(ServerHealthStatus::OK);
    }

    std::set<size_t> tried;
    for (int i = 0; i < 6; ++i) {
        // least connections starts at offset 1
        EXPECT_EQ(least_connections.get_server_index(servers, tried), (i + 1) % TEST_NUM_SERVERS);
    }
}

TEST(LeastConnectionsDistribution, LeastConnectionsDistributesNoServerHealthy) {

    LeastConnections least_connections;

    std::vector<server_info_t> servers;
    servers = std::vector<server_info_t>(TEST_NUM_SERVERS);
    for(int i = 0; i < TEST_NUM_SERVERS; ++i) {
        servers[i].port = 9000 + i;
    }

    std::set<size_t> tried;
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(least_connections.get_server_index(servers, tried), std::nullopt);
    }
}