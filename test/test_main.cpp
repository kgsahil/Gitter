#include <gtest/gtest.h>

/**
 * @brief Main entry point for Gitter unit tests
 * 
 * All test files are automatically registered with GoogleTest.
 * Run with: ./gitter_tests
 * 
 * Or with CMake CTest: ctest --output-on-failure
 */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

