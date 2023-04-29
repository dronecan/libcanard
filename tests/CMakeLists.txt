#
# Copyright (c) 2016-2017 UAVCAN Team
#

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

add_executable(${PROJECT_NAME}_tests
    # common_test.h
    test_crc.cpp
    test_float16.cpp
    test_init.cpp
    # test_memory_allocator.cpp
    test_rxerr.cpp
    # test_scalar_encoding.cpp
)

set(CANARD_CXX_FLAGS "${CANARD_CXX_FLAGS} -DCANARD_INTERNAL=''")
set(CANARD_C_FLAGS   "${CANARD_C_FLAGS}   -DCANARD_INTERNAL=''")

# add source properties
set_target_properties(${PROJECT_NAME}_tests PROPERTIES COMPILE_FLAGS "${CANARD_CXX_FLAGS}")
set_source_files_properties(canard_tgt PROPERTIES COMPILE_FLAGS "${CANARD_C_FLAGS}")

target_link_libraries(${PROJECT_NAME}_tests PRIVATE GTest::gtest_main canard_tgt canard_private_tgt pthread)
if (CANARD_LINK_FLAGS)
    set_target_properties(test_canard PROPERTIES LINK_FLAGS "${CANARD_LINK_FLAGS}")
endif()

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)


# TODO get demo to build standalone
# add_executable(${PROJECT_NAME}_demo demo.c)
# target_link_libraries(${PROJECT_NAME}_tests PRIVATE GTest::gtest_main canard_tgt)

# target_compile_definitions(demo
#                            PUBLIC GIT_HASH=0x${GIT_HASH})
