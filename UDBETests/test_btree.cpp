/**
 * @file test_btree.cpp
 * @brief Unit tests for the B-Tree (MultiIndex) implementation
 */

#include <gtest/gtest.h>
#include "udb.h"
#include <filesystem>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <random>

using namespace udb;

namespace {
    // Test fixture for B-Tree tests
    class BTreeTest : public ::testing::Test {
    protected:
        std::string testFile;

        void SetUp() override {
            testFile = "test_btree_" + std::to_string(std::rand()) + ".ndx";
            // Clean up any existing test file
            std::filesystem::remove(testFile);
        }

        void TearDown() override {
            // Clean up test file
            std::filesystem::remove(testFile);
        }
    };

    //=========================================================================
    // Construction Tests
    //=========================================================================

    TEST_F(BTreeTest, CreateNewIndexFile) {
        MultiIndex index(testFile, 1);
        EXPECT_EQ(index.getNumIndexes(), 1);
        EXPECT_FALSE(index.hasError());
    }

    TEST_F(BTreeTest, CreateMultipleIndexes) {
        MultiIndex index(testFile, 5);
        EXPECT_EQ(index.getNumIndexes(), 5);
        EXPECT_FALSE(index.hasError());
    }

    TEST_F(BTreeTest, InitializeIndex) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);
        
        EXPECT_EQ(index.getKeyType(), KeyType::STRING);
        EXPECT_EQ(index.getKeySize(), 50);
        EXPECT_TRUE(index.canDelete());
        EXPECT_FALSE(index.isUnique());
        EXPECT_FALSE(index.hasError());
    }

    TEST_F(BTreeTest, OpenExistingFile) {
        // Create and close
        {
            MultiIndex index(testFile, 2);
            index.setActiveIndex(1);
            index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);
            index.append("TestKey", 100);
        }

        // Open existing
        {
            MultiIndex index(testFile);
            EXPECT_EQ(index.getNumIndexes(), 2);
            EXPECT_FALSE(index.hasError());
            
            index.setActiveIndex(1);
            int64_t pos = index.find("TestKey");
            EXPECT_EQ(pos, 100);
        }
    }

    //=========================================================================
    // Append Tests
    //=========================================================================

    TEST_F(BTreeTest, AppendSingleKey) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        bool result = index.append("Hello", 42);
        EXPECT_TRUE(result);
        EXPECT_FALSE(index.hasError());

        int64_t pos = index.find("Hello");
        EXPECT_EQ(pos, 42);
    }

    TEST_F(BTreeTest, AppendMultipleKeys) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        std::vector<std::string> keys = {"Apple", "Banana", "Cherry", "Date", "Elderberry"};
        
        for (size_t i = 0; i < keys.size(); ++i) {
            EXPECT_TRUE(index.append(keys[i].c_str(), static_cast<int64_t>(i + 1)));
        }

        // Verify all keys can be found
        for (size_t i = 0; i < keys.size(); ++i) {
            int64_t pos = index.find(keys[i].c_str());
            EXPECT_EQ(pos, static_cast<int64_t>(i + 1));
        }
    }

    TEST_F(BTreeTest, AppendIntegerKeys) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::LONG_INT, sizeof(int32_t), IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        for (int32_t i = 1; i <= 100; ++i) {
            EXPECT_TRUE(index.append(&i, i * 10));
        }

        for (int32_t i = 1; i <= 100; ++i) {
            int64_t pos = index.find(&i);
            EXPECT_EQ(pos, i * 10);
        }
    }

    TEST_F(BTreeTest, AppendCausesSplit) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        // Small node size to force splits
        index.initIndex(KeyType::STRING, 20, IndexAttribute::ALLOW_DELETE, 3, 10, 20);

        // Insert enough keys to cause multiple splits
        for (int i = 1; i <= 50; ++i) {
            std::string key = "Key" + std::to_string(i);
            EXPECT_TRUE(index.append(key.c_str(), i));
        }

        // Verify all keys can be found
        for (int i = 1; i <= 50; ++i) {
            std::string key = "Key" + std::to_string(i);
            int64_t pos = index.find(key.c_str());
            EXPECT_EQ(pos, i);
        }
    }

    //=========================================================================
    // Unique Key Tests
    //=========================================================================

    TEST_F(BTreeTest, UniqueKeyRejectsDuplicates) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::UNIQUE, 5, 50, 100);

        EXPECT_TRUE(index.append("UniqueKey", 100));
        EXPECT_FALSE(index.append("UniqueKey", 200)); // Should fail - duplicate
        
        // Original should still be there
        int64_t pos = index.find("UniqueKey");
        EXPECT_EQ(pos, 100);
    }

    TEST_F(BTreeTest, NonUniqueyAllowsDuplicates) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        EXPECT_TRUE(index.append("DuplicateKey", 100));
        EXPECT_TRUE(index.append("DuplicateKey", 200)); // Should succeed
        EXPECT_TRUE(index.append("DuplicateKey", 300)); // Should succeed
    }

    TEST_F(BTreeTest, UniqueWithDelete) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, 
            static_cast<IndexAttribute>(static_cast<uint16_t>(IndexAttribute::UNIQUE) | 
                                        static_cast<uint16_t>(IndexAttribute::ALLOW_DELETE)), 
            5, 50, 100);

        EXPECT_TRUE(index.append("Key", 100));
        EXPECT_FALSE(index.append("Key", 200)); // Duplicate rejected
        
        EXPECT_TRUE(index.deleteKey("Key"));
        
        EXPECT_TRUE(index.append("Key", 300)); // Now should succeed
        EXPECT_EQ(index.find("Key"), 300);
    }

    //=========================================================================
    // Navigation Tests
    //=========================================================================

    TEST_F(BTreeTest, NavigateFirstNext) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        index.append("C", 3);
        index.append("A", 1);
        index.append("B", 2);

        char key[51] = {0};
        int64_t pos = index.getFirst(key);
        EXPECT_EQ(pos, 1);
        EXPECT_STREQ(key, "A");

        pos = index.getNext(key);
        EXPECT_EQ(pos, 2);
        EXPECT_STREQ(key, "B");

        pos = index.getNext(key);
        EXPECT_EQ(pos, 3);
        EXPECT_STREQ(key, "C");

        pos = index.getNext(key);
        EXPECT_EQ(pos, -1); // EOF
    }

    TEST_F(BTreeTest, NavigatePrevious) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        index.append("A", 1);
        index.append("B", 2);
        index.append("C", 3);

        // Go to last key first
        index.find("C");
        
        char key[51] = {0};
        int64_t pos = index.getPrev(key);
        EXPECT_EQ(pos, 2);
        EXPECT_STREQ(key, "B");

        pos = index.getPrev(key);
        EXPECT_EQ(pos, 1);
        EXPECT_STREQ(key, "A");
    }

    TEST_F(BTreeTest, EOFAndBOFFlags) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        index.append("A", 1);
        index.append("B", 2);

        index.getFirst(nullptr);
        EXPECT_TRUE(index.isBOF());
        EXPECT_FALSE(index.isEOF());

        index.getNext(nullptr);
        EXPECT_FALSE(index.isBOF());
        EXPECT_TRUE(index.isEOF());
    }

    TEST_F(BTreeTest, NavigateFullSequence) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        // Insert in random order
        std::vector<std::string> keys = {"D", "B", "F", "A", "C", "E"};
        for (size_t i = 0; i < keys.size(); ++i) {
            index.append(keys[i].c_str(), static_cast<int64_t>(i + 1));
        }

        // Navigate forward
        char key[51];
        std::vector<std::string> forward;
        int64_t pos = index.getFirst(key);
        while (pos != -1 && !index.isEOF()) {
            forward.push_back(key);
            pos = index.getNext(key);
        }
        forward.push_back(key); // Get last one

        // Should be sorted
        std::vector<std::string> expected = {"A", "B", "C", "D", "E", "F"};
        EXPECT_EQ(forward, expected);
    }

    //=========================================================================
    // Delete Tests
    //=========================================================================

    TEST_F(BTreeTest, DeleteKey) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        index.append("A", 1);
        index.append("B", 2);
        index.append("C", 3);

        EXPECT_TRUE(index.deleteKey("B"));
        
        EXPECT_NE(index.find("A"), -1);
        EXPECT_EQ(index.find("B"), -1);
        EXPECT_NE(index.find("C"), -1);
    }

    TEST_F(BTreeTest, DeleteCurrent) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        index.append("A", 1);
        index.append("B", 2);
        index.append("C", 3);

        index.find("B");
        int64_t deletedPos = index.deleteCurrent();
        EXPECT_EQ(deletedPos, 2);

        EXPECT_EQ(index.find("B"), -1);
    }

    TEST_F(BTreeTest, DeleteNonExistent) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        index.append("A", 1);

        EXPECT_FALSE(index.deleteKey("NotFound"));
    }

    TEST_F(BTreeTest, DeleteAllKeys) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 20, IndexAttribute::ALLOW_DELETE, 3, 10, 20);

        // Insert keys
        for (int i = 1; i <= 20; ++i) {
            std::string key = "Key" + std::to_string(i);
            index.append(key.c_str(), i);
        }

        // Delete all
        for (int i = 1; i <= 20; ++i) {
            std::string key = "Key" + std::to_string(i);
            EXPECT_TRUE(index.deleteKey(key.c_str()));
        }

        // Verify empty
        EXPECT_EQ(index.getFirst(nullptr), -1);
    }

    TEST_F(BTreeTest, DeleteAndReinsert) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        index.append("TestKey", 100);
        EXPECT_TRUE(index.deleteKey("TestKey"));
        EXPECT_EQ(index.find("TestKey"), -1);

        index.append("TestKey", 200);
        EXPECT_EQ(index.find("TestKey"), 200);
    }

    //=========================================================================
    // Multi-Index Tests
    //=========================================================================

    TEST_F(BTreeTest, MultipleIndexesSameFile) {
        MultiIndex index(testFile, 3);
        
        // Initialize different indexes with different configurations
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);
        
        index.setActiveIndex(2);
        index.initIndex(KeyType::LONG_INT, sizeof(int32_t), IndexAttribute::ALLOW_DELETE, 7, 50, 100);
        
        index.setActiveIndex(3);
        index.initIndex(KeyType::STRING, 100, IndexAttribute::UNIQUE, 9, 50, 100);

        // Add data to each index
        index.setActiveIndex(1);
        index.append("StringKey1", 100);

        index.setActiveIndex(2);
        int32_t intKey = 42;
        index.append(&intKey, 200);

        index.setActiveIndex(3);
        index.append("UniqueKey", 300);

        // Verify each index has its own data
        index.setActiveIndex(1);
        EXPECT_EQ(index.find("StringKey1"), 100);
        EXPECT_EQ(index.getKeyType(), KeyType::STRING);

        index.setActiveIndex(2);
        EXPECT_EQ(index.find(&intKey), 200);
        EXPECT_EQ(index.getKeyType(), KeyType::LONG_INT);

        index.setActiveIndex(3);
        EXPECT_EQ(index.find("UniqueKey"), 300);
        EXPECT_TRUE(index.isUnique());
    }

    TEST_F(BTreeTest, SwitchingIndexes) {
        MultiIndex index(testFile, 2);
        
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);
        
        index.setActiveIndex(2);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        // Add to index 1
        index.setActiveIndex(1);
        index.append("IndexOne", 100);

        // Add to index 2
        index.setActiveIndex(2);
        index.append("IndexTwo", 200);

        // Verify isolation
        index.setActiveIndex(1);
        EXPECT_NE(index.find("IndexOne"), -1);
        EXPECT_EQ(index.find("IndexTwo"), -1);

        index.setActiveIndex(2);
        EXPECT_EQ(index.find("IndexOne"), -1);
        EXPECT_NE(index.find("IndexTwo"), -1);
    }

    //=========================================================================
    // Thread Safety Tests
    //=========================================================================

    TEST_F(BTreeTest, ConcurrentReads) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        // Populate with data
        for (int i = 1; i <= 100; ++i) {
            std::string key = "Key" + std::to_string(i);
            index.append(key.c_str(), i);
        }

        std::atomic<int> successCount{0};
        std::vector<std::thread> threads;

        // Launch multiple reader threads
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&index, &successCount, t]() {
                for (int i = 1; i <= 100; ++i) {
                    std::string key = "Key" + std::to_string(i);
                    if (index.find(key.c_str()) == i) {
                        ++successCount;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        EXPECT_EQ(successCount.load(), 400); // 4 threads * 100 reads each
    }

    TEST_F(BTreeTest, ConcurrentWrites) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        std::atomic<int> writeCount{0};
        std::vector<std::thread> threads;

        // Launch multiple writer threads
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&index, &writeCount, t]() {
                for (int i = 0; i < 25; ++i) {
                    std::string key = "Thread" + std::to_string(t) + "_Key" + std::to_string(i);
                    if (index.append(key.c_str(), t * 1000 + i)) {
                        ++writeCount;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        EXPECT_EQ(writeCount.load(), 100); // 4 threads * 25 writes each
    }

    //=========================================================================
    // Edge Case Tests
    //=========================================================================

    TEST_F(BTreeTest, EmptyIndex) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        EXPECT_EQ(index.getFirst(nullptr), -1);
        EXPECT_EQ(index.find("NotFound"), -1);
    }

    TEST_F(BTreeTest, SingleKey) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        index.append("OnlyKey", 42);

        char key[51] = {0};
        EXPECT_EQ(index.getFirst(key), 42);
        EXPECT_STREQ(key, "OnlyKey");
        EXPECT_TRUE(index.isBOF());
        EXPECT_TRUE(index.isEOF());
    }

    TEST_F(BTreeTest, LargeDataset) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 20, IndexAttribute::ALLOW_DELETE, 5, 100, 200);

        const int count = 1000;
        for (int i = 0; i < count; ++i) {
            std::string key = std::to_string(i);
            EXPECT_TRUE(index.append(key.c_str(), i));
        }

        // Verify random samples
        for (int i = 0; i < 100; ++i) {
            int key = std::rand() % count;
            std::string keyStr = std::to_string(key);
            EXPECT_EQ(index.find(keyStr.c_str()), key);
        }
    }

    TEST_F(BTreeTest, EmptyStringKey) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        EXPECT_TRUE(index.append("", 100));
        EXPECT_EQ(index.find(""), 100);
    }

    TEST_F(BTreeTest, MaxLengthStringKey) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        std::string maxKey(49, 'X'); // Max length for 50-byte key with null terminator
        EXPECT_TRUE(index.append(maxKey.c_str(), 100));
        EXPECT_EQ(index.find(maxKey.c_str()), 100);
    }

    TEST_F(BTreeTest, ReverseOrderInsert) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::STRING, 20, IndexAttribute::ALLOW_DELETE, 3, 10, 20);

        // Insert in reverse order
        for (int i = 100; i >= 1; --i) {
            std::string key = std::to_string(i);
            // Pad to ensure proper string ordering
            while (key.length() < 3) key = "0" + key;
            EXPECT_TRUE(index.append(key.c_str(), i));
        }

        // Navigate forward should be in sorted order
        char key[51];
        int64_t pos = index.getFirst(key);
        int expected = 1;
        while (pos != -1 && !index.isEOF()) {
            EXPECT_EQ(pos, expected);
            expected++;
            pos = index.getNext(key);
        }
    }

    TEST_F(BTreeTest, RandomOrderInsert) {
        MultiIndex index(testFile, 1);
        index.setActiveIndex(1);
        index.initIndex(KeyType::LONG_INT, sizeof(int32_t), IndexAttribute::ALLOW_DELETE, 5, 50, 100);

        std::vector<int32_t> keys;
        for (int32_t i = 1; i <= 100; ++i) {
            keys.push_back(i);
        }

        // Shuffle
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(keys.begin(), keys.end(), g);

        // Insert in random order
        for (int32_t key : keys) {
            EXPECT_TRUE(index.append(&key, key * 10));
        }

        // Verify all exist
        for (int32_t key : keys) {
            EXPECT_EQ(index.find(&key), key * 10);
        }

        // Verify sorted order navigation
        char keyBuf[sizeof(int32_t)];
        int64_t pos = index.getFirst(keyBuf);
        int32_t expected = 1;
        while (pos != -1 && !index.isEOF()) {
            EXPECT_EQ(pos, expected * 10);
            expected++;
            pos = index.getNext(keyBuf);
        }
    }

    //=========================================================================
    // Persistence Tests
    //=========================================================================

    TEST_F(BTreeTest, PersistAcrossReopen) {
        // Create index and add data
        {
            MultiIndex index(testFile, 1);
            index.setActiveIndex(1);
            index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

            for (int i = 1; i <= 50; ++i) {
                std::string key = "Persist" + std::to_string(i);
                index.append(key.c_str(), i * 100);
            }
        }

        // Reopen and verify
        {
            MultiIndex index(testFile);
            index.setActiveIndex(1);

            for (int i = 1; i <= 50; ++i) {
                std::string key = "Persist" + std::to_string(i);
                EXPECT_EQ(index.find(key.c_str()), i * 100);
            }
        }
    }

    TEST_F(BTreeTest, PersistAfterDelete) {
        // Create, add, delete
        {
            MultiIndex index(testFile, 1);
            index.setActiveIndex(1);
            index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 50, 100);

            index.append("Keep1", 100);
            index.append("Delete1", 200);
            index.append("Keep2", 300);
            index.append("Delete2", 400);

            index.deleteKey("Delete1");
            index.deleteKey("Delete2");
        }

        // Reopen and verify
        {
            MultiIndex index(testFile);
            index.setActiveIndex(1);

            EXPECT_EQ(index.find("Keep1"), 100);
            EXPECT_EQ(index.find("Keep2"), 300);
            EXPECT_EQ(index.find("Delete1"), -1);
            EXPECT_EQ(index.find("Delete2"), -1);
        }
    }

} // anonymous namespace
