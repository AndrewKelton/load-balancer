#include <gtest/gtest.h>

#include "utils.hpp"

#include <vector>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <limits>

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

TEST(ToSizeT, ParsesValidNumber) {
    EXPECT_EQ(to_size_t("123"), 123);
}

TEST(ToSizeT, ReturnsNulloptOnEmptyString) {
    EXPECT_EQ(to_size_t(""), std::nullopt);
}

TEST(ToSizeT, ReturnsNulloptOnNegativeNumber) {
    EXPECT_EQ(to_size_t("-123"), std::nullopt);
}

// TEST(ToSizeT, ReturnsNulloptOnOverflow) {
//     EXPECT_EQ(to_size_t(std::to_string(std::numeric_limits<std::size_t>::max())), std::nullopt);
// }

TEST(ToSizeT, ReturnsNulloptOnTrailingGarbage) {
    EXPECT_EQ(to_size_t("123 .\t"), std::nullopt);
}

TEST(ToInt, ParsesNegativeNumber) {
    EXPECT_EQ(to_int("-123"), -123);
}

TEST(Utils, ContentLengthIsFound) {
    
    std::vector<std::string_view> content_split = split("Content-Length: 23", ":");
    size_t content_length = to_size_t(trim(content_split[1])).value();

    EXPECT_EQ(content_length, 23);
}

/* Need to add this test*/
// TEST(ConvertBinaryIpToString, ValidConversion) {
// 
// }