cmake_minimum_required (VERSION 3.5)
project(Canard)

set(INTERFACE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/interface)
set(MODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)


# execute process to generate libcanard headers using dronecan_dsdlc/dronecan_dsdlc.py
execute_process(COMMAND python3 dronecan_dsdlc/dronecan_dsdlc.py
    -O ${CMAKE_CURRENT_BINARY_DIR}/dsdlc_generated
    ${MODULES_DIR}/DSDL/uavcan
    ${MODULES_DIR}/DSDL/dronecan
    ${MODULES_DIR}/DSDL/ardupilot
    ${MODULES_DIR}/DSDL/com
    )

# generate dsdl messages
include_directories(${CMAKE_CURRENT_BINARY_DIR}/dsdlc_generated/include)

# add local directory to include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/modules/libcanard/)

# add libcanard source files
file(GLOB LIBCANARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/modules/libcanard/*.c)
list(APPEND SRC_FILES ${LIBCANARD_SOURCES})

# glob all generated dsdlc files
file(GLOB DSDL_GENERATED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/dsdlc_generated/src/*.c*)

# append DSDL_GENERATED_SOURCES_C to SRC_FILES
list(APPEND SRC_FILES ${DSDL_GENERATED_SOURCES})

add_compile_options(-Wall -m32 -g)
add_link_options(-m32)

add_subdirectory(googletest)

# disable GMOCK
set(BUILD_GMOCK OFF BOOL FORCE)

add_subdirectory(tests)
