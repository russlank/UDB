/**
 * @file test_main.cpp
 * @brief Main entry point for UDB unit tests using Google Test
 */

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
