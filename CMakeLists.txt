cmake_minimum_required(VERSION 3.14)
set(AUDIOBUFFER_REPO_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ******************************************************************************

# CPM dependency manager.
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
  # ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  ${AUDIOBUFFER_REPO_DIR}/.cpm/CPM.cmake
)
include(${AUDIOBUFFER_REPO_DIR}/.cpm/CPM.cmake)

# ******************************************************************************

# Project declaration.
project(
  audiobuffer
  VERSION 0.2.0
  DESCRIPTION "STL-like containers for handling audio data."
  LANGUAGES CXX
)

# Header-only (interface) library.
add_library(${PROJECT_NAME} INTERFACE)
# Create an alias so the library can be used internally as audiobuffer::audiobuffer
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# ******************************************************************************

# Include directories.
target_include_directories(${PROJECT_NAME} INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# C++ standard and requirements.
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

CPMAddPackage("gh:DeltaRazero/numio-cpp@1.4.0")
target_link_libraries(${PROJECT_NAME} INTERFACE numio::numio)

# ******************************************************************************

# Installation targets (for find_package()).
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Testing framework.
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  enable_testing()
  add_subdirectory(test)
endif()
