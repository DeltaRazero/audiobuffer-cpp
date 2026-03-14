include(FetchContent)

# Download GoogleTest.
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)

# For Windows: prevent GTest from overriding the compiler/linker settings.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB SOURCE_FILES
  # ./**/*.cpp
  ./values/scale_test.cpp
)

set(CMAKE_CXX_FLAGS "-O3")

# Define the test executable.
add_executable(unit_tests
  ${SOURCE_FILES}
)

find_package(OpenMP)

# Link to the library and GTest.
# GTest provides 'gtest_main' which includes a default main() function.
target_link_libraries(unit_tests
  PRIVATE
    OpenMP::OpenMP_CXX
    audiobuffer::audiobuffer
    gtest_main
)

# Allow CTest to discover the tests automatically.
include(GoogleTest)
gtest_discover_tests(unit_tests)
