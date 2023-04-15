#
# Copyright (c) 2016-2017 UAVCAN Team
#

cmake_minimum_required (VERSION 3.5)
project(libcanard)

# Libcanard
include_directories(..)
# include_directories(../drivers/socketcan)

# Compiler configuration - supporting only Clang and GCC
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c99   -Wall -Wextra -Werror -pedantic")

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
include(GoogleTest)
gtest_discover_tests(test_canard)
