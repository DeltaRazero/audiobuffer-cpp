include(${AUDIOBUFFER_REPO_DIR}/.cpm/CPM.cmake)

# ******************************************************************************

CPMAddPackage(NAME googletest
  GITHUB_REPOSITORY
    google/googletest
  GIT_TAG
    v1.17.x
  OPTIONS
    "INSTALL_GTEST OFF"
    # For Windows: prevent GTest from overriding the compiler/linker settings.
    "gtest_force_shared_crt"
)

# ******************************************************************************

file(GLOB SOURCE_FILES
  ./src/values/scale_test.cpp
)

# Define the unit tests executable.
add_executable(unit_tests
  ${SOURCE_FILES}
)

add_compile_definitions(
  NUMIO_ENABLE_EXCEPTIONS
  audiobuffer__enable_nonstd_unsigned
)
add_compile_options(
  -O3
  -pedantic
  -Wall
  -Wextra
  -Werror
  -Wno-unused-command-line-argument
)

find_package(OpenMP REQUIRED)

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
