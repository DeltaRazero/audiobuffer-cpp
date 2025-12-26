include(FetchContent)

# Download GoogleTest.
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)

# For Windows: prevent GTest from overriding the compiler/linker settings.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Define the test executable.
add_executable(unit_tests
  dummy_test.cpp
)

# Link to the library and GTest.
# GTest provides 'gtest_main' which includes a default main() function.
target_link_libraries(unit_tests
  PRIVATE
    audiobuffer::audiobuffer
    gtest_main
)

# Allow CTest to discover the tests automatically.
include(GoogleTest)
gtest_discover_tests(unit_tests)
