#!/bin/bash

# This script runs the unit tests for the library.
# build native bit version
cmake -DCMAKE_32BIT=0 -S . -B build && cmake --build build || exit 1
# run tests
pushd build/
ctest || exit 1
popd
# build 32 bit version
cmake -DCMAKE_32BIT=1 -S . -B build32 && cmake --build build32 || exit 1
# run tests
pushd build32/
# exit with error code if tests failed
ctest || exit 1
popd

