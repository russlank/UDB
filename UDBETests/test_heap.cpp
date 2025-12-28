/**
 * @file test_heap.cpp
 * @brief Unit tests for the HeapFile implementation
 */

#include <gtest/gtest.h>
#include "udb.h"
#include <filesystem>
#include <string>
#include <vector>
#include <thread>
#include <random>

using namespace udb;

namespace {
    // Test fixture for HeapFile tests
    class HeapTest : public ::testing::Test {
    protected:
        std::string testFile;

        void SetUp() override {
            testFile = "test_heap_" + std::to_string(std::rand()) + ".heap";
            std::filesystem::remove(testFile);
        }

        void TearDown() override {
            std::filesystem::remove(testFile);
        }
    };

    //=========================================================================
    // Construction Tests
    //=========================================================================

    TEST_F(HeapTest, CreateNewHeapFile) {
        HeapFile heap(testFile, 100);
        EXPECT_TRUE(heap.isOpen());
        EXPECT_FALSE(heap.hasError());
        EXPECT_EQ(heap.getHolesTableSize(), 100);
    }

    TEST_F(HeapTest, CreateWithDifferentTableSizes) {
        {
            HeapFile heap10(testFile, 10);
            EXPECT_EQ(heap10.getHolesTableSize(), 10);
        }
        std::filesystem::remove(testFile);
        {
            HeapFile heap500(testFile, 500);
            EXPECT_EQ(heap500.getHolesTableSize(), 500);
        }
    }

    TEST_F(HeapTest, OpenExistingHeapFile) {
        // Create and close
        {
            HeapFile heap(testFile, 75);
            int64_t pos = heap.allocateSpace(100);
            EXPECT_GE(pos, 0);
        }

        // Open existing
        {
            HeapFile heap(testFile);
            EXPECT_TRUE(heap.isOpen());
            EXPECT_EQ(heap.getHolesTableSize(), 75);
        }
    }

    //=========================================================================
    // Allocation Tests
    //=========================================================================

    TEST_F(HeapTest, AllocateSimple) {
        HeapFile heap(testFile, 100);

        int64_t pos1 = heap.allocateSpace(100);
        int64_t pos2 = heap.allocateSpace(200);
        int64_t pos3 = heap.allocateSpace(50);

        EXPECT_GE(pos1, 0);
        EXPECT_GE(pos2, 0);
        EXPECT_GE(pos3, 0);
        EXPECT_NE(pos1, pos2);
        EXPECT_NE(pos2, pos3);
        EXPECT_NE(pos1, pos3);
    }

    TEST_F(HeapTest, AllocateAndWrite) {
        HeapFile heap(testFile, 100);

        struct TestRecord {
            int32_t id;
            char name[20];
            double value;
        };

        TestRecord writeRec = {42, "TestRecord", 3.14159};
        int64_t pos = heap.allocateSpace(sizeof(TestRecord));
        heap.write(&writeRec, sizeof(TestRecord), pos);

        TestRecord readRec = {};
        heap.read(&readRec, sizeof(TestRecord), pos);

        EXPECT_EQ(readRec.id, writeRec.id);
        EXPECT_STREQ(readRec.name, writeRec.name);
        EXPECT_DOUBLE_EQ(readRec.value, writeRec.value);
    }

    TEST_F(HeapTest, AllocateMultipleRecords) {
        HeapFile heap(testFile, 100);

        std::vector<int64_t> positions;
        for (int i = 0; i < 100; ++i) {
            int64_t pos = heap.allocateSpace(64);
            positions.push_back(pos);
            
            int32_t data = i;
            heap.write(&data, sizeof(data), pos);
        }

        // Verify all records
        for (int i = 0; i < 100; ++i) {
            int32_t data;
            heap.read(&data, sizeof(data), positions[i]);
            EXPECT_EQ(data, i);
        }
    }

    //=========================================================================
    // Free Space Tests
    //=========================================================================

    TEST_F(HeapTest, FreeAndReallocate) {
        HeapFile heap(testFile, 100);

        // Allocate several blocks
        int64_t pos1 = heap.allocateSpace(100);
        int64_t pos2 = heap.allocateSpace(200);
        int64_t pos3 = heap.allocateSpace(100);

        // Free the middle block
        heap.freeSpace(pos2, 200);

        // Allocate a block that fits in the hole
        int64_t pos4 = heap.allocateSpace(150);

        // Should reuse the freed space (pos2)
        EXPECT_EQ(pos4, pos2);
    }

    TEST_F(HeapTest, FreeAndReallocateExact) {
        HeapFile heap(testFile, 100);

        int64_t pos1 = heap.allocateSpace(100);
        heap.freeSpace(pos1, 100);

        // Exact fit should reuse the hole
        int64_t pos2 = heap.allocateSpace(100);
        EXPECT_EQ(pos2, pos1);
    }

    TEST_F(HeapTest, FreeMultipleHoles) {
        HeapFile heap(testFile, 100);

        std::vector<int64_t> positions;
        for (int i = 0; i < 10; ++i) {
            positions.push_back(heap.allocateSpace(50));
        }

        // Free every other block
        for (int i = 0; i < 10; i += 2) {
            heap.freeSpace(positions[i], 50);
        }

        // Allocate new blocks - should reuse holes
        for (int i = 0; i < 5; ++i) {
            int64_t pos = heap.allocateSpace(50);
            EXPECT_GE(pos, 0);
        }
    }

    //=========================================================================
    // Integration with Index Tests
    //=========================================================================

    TEST_F(HeapTest, HeapWithBTreeIndex) {
        std::string indexFile = testFile + ".ndx";
        
        {
            HeapFile heap(testFile, 100);
            MultiIndex index(indexFile, 1);
            index.setActiveIndex(1);
            index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

            // Store records with keys
            struct Record {
                int32_t id;
                char name[32];
            };

            Record rec1 = {1, "Alice"};
            Record rec2 = {2, "Bob"};
            Record rec3 = {3, "Charlie"};

            int64_t pos1 = heap.allocateSpace(sizeof(Record));
            heap.write(&rec1, sizeof(Record), pos1);
            index.append(rec1.name, pos1);

            int64_t pos2 = heap.allocateSpace(sizeof(Record));
            heap.write(&rec2, sizeof(Record), pos2);
            index.append(rec2.name, pos2);

            int64_t pos3 = heap.allocateSpace(sizeof(Record));
            heap.write(&rec3, sizeof(Record), pos3);
            index.append(rec3.name, pos3);

            // Find and verify
            int64_t foundPos = index.find("Bob");
            EXPECT_EQ(foundPos, pos2);

            Record foundRec;
            heap.read(&foundRec, sizeof(Record), foundPos);
            EXPECT_EQ(foundRec.id, 2);
        }

        std::filesystem::remove(indexFile);
    }

    //=========================================================================
    // Thread Safety Tests
    //=========================================================================

    TEST_F(HeapTest, ConcurrentAllocations) {
        HeapFile heap(testFile, 100);

        std::vector<std::thread> threads;
        std::vector<int64_t> positions(40, -1);
        std::atomic<int> successCount{0};

        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&heap, &positions, &successCount, t]() {
                for (int i = 0; i < 10; ++i) {
                    int64_t pos = heap.allocateSpace(100);
                    if (pos >= 0) {
                        positions[t * 10 + i] = pos;
                        ++successCount;
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(successCount.load(), 40);

        // Verify all positions are unique
        std::sort(positions.begin(), positions.end());
        auto last = std::unique(positions.begin(), positions.end());
        EXPECT_EQ(std::distance(positions.begin(), last), 40);
    }

    TEST_F(HeapTest, ConcurrentWriteRead) {
        HeapFile heap(testFile, 100);

        // Pre-allocate positions
        std::vector<int64_t> positions;
        for (int i = 0; i < 100; ++i) {
            positions.push_back(heap.allocateSpace(sizeof(int32_t)));
        }

        // Write from multiple threads
        std::vector<std::thread> writers;
        for (int t = 0; t < 4; ++t) {
            writers.emplace_back([&heap, &positions, t]() {
                for (int i = t * 25; i < (t + 1) * 25; ++i) {
                    int32_t data = i;
                    heap.write(&data, sizeof(data), positions[i]);
                }
            });
        }

        for (auto& thread : writers) {
            thread.join();
        }

        // Read and verify
        for (int i = 0; i < 100; ++i) {
            int32_t data;
            heap.read(&data, sizeof(data), positions[i]);
            EXPECT_EQ(data, i);
        }
    }

    //=========================================================================
    // Edge Cases
    //=========================================================================

    TEST_F(HeapTest, ZeroSizeAllocation) {
        HeapFile heap(testFile, 100);
        int64_t pos = heap.allocateSpace(0);
        EXPECT_GE(pos, 0); // Should still return valid position
    }

    TEST_F(HeapTest, LargeAllocation) {
        HeapFile heap(testFile, 100);
        
        // Allocate 1MB
        int64_t pos = heap.allocateSpace(1024 * 1024);
        EXPECT_GE(pos, 0);

        // Write and verify
        std::vector<uint8_t> data(1024 * 1024, 0xAB);
        heap.write(data.data(), data.size(), pos);

        std::vector<uint8_t> readData(1024 * 1024);
        heap.read(readData.data(), readData.size(), pos);
        EXPECT_EQ(data, readData);
    }

    TEST_F(HeapTest, ManySmallHoles) {
        HeapFile heap(testFile, 10); // Small holes table

        // Create many allocations
        std::vector<int64_t> positions;
        for (int i = 0; i < 50; ++i) {
            positions.push_back(heap.allocateSpace(16));
        }

        // Free all - should require multiple holes tables
        for (auto pos : positions) {
            heap.freeSpace(pos, 16);
        }

        // Reallocate - should reuse holes
        for (int i = 0; i < 50; ++i) {
            int64_t pos = heap.allocateSpace(16);
            EXPECT_GE(pos, 0);
        }
    }

} // anonymous namespace
