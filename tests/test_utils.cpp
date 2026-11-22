#include <gtest/gtest.h>

#include "utils.hpp"

#include <vector>
#include <string_view>

TEST(UtilsParseRequesr, ParsesMethodPathAndVersion) {

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

TEST(Utils, ContentLengthIsFound) {
    
    std::vector<std::string_view> content_split = split("Content-Length: 23", ":");
    size_t content_length = to_size_t(trim(content_split[1])).value();

    EXPECT_EQ(content_length, 23);
}