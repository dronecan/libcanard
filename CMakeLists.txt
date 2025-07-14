#
# Copyright (c) 2016-2017 UAVCAN Team
#

cmake_minimum_required (VERSION 3.22.1)
project(Canard)

set(Canard_VERSION_MAJOR 0)
set(Canard_VERSION_MINOR 2)
set(Canard_VERSION_PATCH 0)
set(Canard_VERSION ${Canard_VERSION_MAJOR}.${Canard_VERSION_MINOR}.${Canard_VERSION_PATCH})

# check arguments for 32-bit build
option(CMAKE_32BIT "Enable 32 bit build" OFF)

option(CANARD_ENABLE_CANFD "Enable CAN Flexible Data-Rate" OFF)
if (${CANARD_ENABLE_CANFD})
    add_definitions(-DCANARD_ENABLE_CANFD=1)
endif()

option(CANARD_ENABLE_DEADLINE "Enable Deadline" OFF)
if (${CANARD_ENABLE_DEADLINE})
    add_definitions(-DCANARD_ENABLE_DEADLINE=1)
endif()

option(CANARD_MULTI_IFACE "Enable Multi-Interface" OFF)
if (${CANARD_MULTI_IFACE})
    add_definitions(-DCANARD_MULTI_IFACE=1)
endif()

# Compiler configuration - supporting only Clang and GCC
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c11   -Wall -Wextra -Werror -pedantic")
# turn on debug symbols by default
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if (CMAKE_32BIT)
     message(STATUS "Building for 32-bit")
     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
else()
      message(STATUS "Building for 64-bit")
endif()

# C warnings
set(CANARD_C_FLAGS "${CANARD_C_FLAGS} -Wdouble-promotion -Wswitch-enum -Wfloat-equal -Wundef")
set(CANARD_C_FLAGS "${CANARD_C_FLAGS} -Wconversion -Wtype-limits")
set(CANARD_C_FLAGS "${CANARD_C_FLAGS} -Wsign-conversion -Wcast-align -Wmissing-declarations")

# C++ warnings
set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -Wdouble-promotion -Wswitch-enum -Wfloat-equal -Wundef")
set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -Wconversion -Wsign-promo")
set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -Wsign-conversion -Wcast-align -Wmissing-declarations")
set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -Wtype-limits -Wzero-as-null-pointer-constant -Wnon-virtual-dtor")
set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -Woverloaded-virtual -Wsign-promo")
# We allow the following warnings for compatibility with the C codebase:
set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -Wno-error=old-style-cast -Wno-error=zero-as-null-pointer-constant")
# add coverage flags
option(CANARD_ENABLE_COVERAGE "Enable Code Coverage" OFF)
if (${CANARD_ENABLE_COVERAGE})
    set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CANARD_C_FLAGS "${CANARD_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CANARD_LINK_FLAGS "${CANARD_LINK_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

add_library(canard_tgt canard.c canard.h)
target_compile_options(canard_tgt PUBLIC -Wall -g)
target_include_directories(canard_tgt
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
set_target_properties(canard_tgt PROPERTIES
  VERSION ${Canard_VERSION}
)

add_library(canard_private_tgt canard_internals.h canard.c)
target_compile_options(canard_private_tgt PUBLIC
  -Wall
  -g)
target_link_libraries(canard_private_tgt PUBLIC canard_tgt)

include(CTest)
if(${BUILD_TESTING})
  add_definitions(-DCANARD_INTERNAL=)

  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.13.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  set(BUILD_GTEST ON CACHE BOOL "" FORCE)
  # disable GMOCK
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)

  add_subdirectory(tests)
  add_subdirectory(canard/tests)
  if (CANARD_ENABLE_COVERAGE)
    add_custom_target(coverage
                      COMMAND lcov --directory . --capture --output-file coverage.info --exclude '/usr/*' --exclude '*gtest*' --exclude '*dsdlc_generated/*' --exclude '*tests/*'
                      COMMAND lcov --list coverage.info
                      COMMAND genhtml coverage.info --output-directory coverage
                      COMMENT echo "Coverage report generated at ${CMAKE_BINARY_DIR}/coverage/index.html"
                      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    add_custom_command(TARGET coverage POST_BUILD COMMAND ;)
  endif()
endif()

# Install targets
install(
  TARGETS canard_tgt
  EXPORT ${PROJECT_NAME}Targets
  DESTINATION lib
)

install(
  EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  DESTINATION lib/cmake/${PROJECT_NAME}
)

# Install header
install(
  FILES canard.h
  DESTINATION include
)

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

include(CMakePackageConfigHelpers)
# generate the config file that includes the exports
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "lib/cmake/${PROJECT_NAME}"
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION "${Canard_VERSION}"
  COMPATIBILITY AnyNewerVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  DESTINATION lib/cmake/${PROJECT_NAME}
)

export(EXPORT ${PROJECT_NAME}Targets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake"
)
