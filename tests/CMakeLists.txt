# Fetch GoogleTest for the unit-test executable.
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
)

# GoogleTest-based tests
add_executable(unit_tests
  ${TEST_SOURCES}
)

target_link_libraries(unit_tests
    PRIVATE
    ${PROJECT_NAME}
        GTest::gtest_main
)

gtest_discover_tests(unit_tests)

# Standalone test programs
# add_subdirectory(integration)

