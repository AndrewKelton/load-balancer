#include <gtest/gtest.h>

#include "utils.hpp"

TEST(ParseRequest, ParsesMethodPathAndVersion) {

    // Arrange
    std::string raw = 
        "GET /health HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";
    
    // Act
    request_t request = parse_request(raw);

    // Assert
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.path_uri, "/health");
    EXPECT_EQ(request.version, "HTTP/1.1");
    ASSERT_EQ(request.headers.count("Host"), 1u);
    EXPECT_EQ(request.headers.at("Host"), "localhost");
}   
