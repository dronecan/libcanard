#
# Copyright (c) 2016-2017 UAVCAN Team
#

cmake_minimum_required (VERSION 3.5)
project(libcanard)

# Libcanard
include_directories(..)
include(CTest)
# include_directories(../drivers/socketcan)

# Compiler configuration - supporting only Clang and GCC
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c99   -Wall -Wextra -Werror -pedantic")
# turn on debug symbols by default
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if (CANARD_ENABLE_CANFD)
    add_definitions(-DCANARD_ENABLE_CANFD=1)
endif()

if (CANARD_ENABLE_DEADLINE)
    add_definitions(-DCANARD_ENABLE_DEADLINE=1)
endif()

if (CANARD_MULTI_IFACE)
    add_definitions(-DCANARD_MULTI_IFACE=1)
endif()

# check arguments for 32-bit build
if (CMAKE_32BIT)
     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
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
set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -Woverloaded-virtual -Wsign-promo -Wold-style-cast")
# We allow the following warnings for compatibility with the C codebase:
set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -Wno-error=old-style-cast -Wno-error=zero-as-null-pointer-constant")
# add coverage flags
if (CANARD_ENABLE_COVERAGE)
    set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CANARD_C_FLAGS "${CANARD_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CANARD_LINK_FLAGS "${CANARD_LINK_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

# Expose internal API for unit testing
set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -DCANARD_INTERNAL=''")
set(CANARD_C_FLAGS   "${CANARD_C_FLAGS}   -DCANARD_INTERNAL=''")

add_subdirectory(googletest)

enable_testing()

include_directories(googletest/googletest/include)

# Unit tests
file(GLOB tests_src
     RELATIVE "${CMAKE_SOURCE_DIR}"
     "*.cpp")

message(STATUS "Unit test source files: ${tests_src}")

# add source properties
set_source_files_properties(${tests_src} PROPERTIES COMPILE_FLAGS "${CANARD_CXX_FLAGS}")
set_source_files_properties(../canard.c PROPERTIES COMPILE_FLAGS "${CANARD_C_FLAGS}")

add_executable(test_canard ${tests_src} ../canard.c)
target_link_libraries(test_canard GTest::gtest_main)
if (CANARD_LINK_FLAGS)
    set_target_properties(test_canard PROPERTIES LINK_FLAGS "${CANARD_LINK_FLAGS}")
endif()
include(GoogleTest)
gtest_discover_tests(test_canard)

if (CANARD_ENABLE_COVERAGE)
    add_custom_target(coverage
                        COMMAND lcov --directory . --capture --output-file coverage.info
                        COMMAND lcov --remove coverage.info '/usr/*' '*gtest*' '*tests/*' --output-file coverage.info
                        COMMAND lcov --list coverage.info
                        COMMAND genhtml coverage.info --output-directory coverage
                        COMMENT echo "Coverage report generated at ${CMAKE_BINARY_DIR}/coverage/index.html"
                        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    add_custom_command(TARGET coverage POST_BUILD COMMAND ;)
endif()
