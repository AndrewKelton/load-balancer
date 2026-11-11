
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    test_parse_request.cpp
    test_server_health_status_to_string.cpp
)

target_link_libraries(unit_tests
    PRIVATE
        lb_core
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)