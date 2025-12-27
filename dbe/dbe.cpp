/**
 * @file test_main.cpp
 * @brief Test application for the UDB library
 *
 * This program provides an interactive console interface for testing
 * the B-Tree index functionality, similar to the original TEST.CPP.
 *
 * @author Digixoil
 * @version 2.0.0
 */

#include "udb.h"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <filesystem>

using namespace udb;

/**
 * @brief Print application header
 */
void printHeader()
{
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "  " << getLibraryName() << " v" << getVersion() << "\n";
    std::cout << "  Interactive Test Application\n";
    std::cout << "============================================================\n\n";
}

/**
 * @brief Print available commands
 */
void printHelp()
{
    std::cout << "\nAvailable commands:\n";
    std::cout << "  A <key>     - Append a new key\n";
    std::cout << "  D <key>     - Delete a key\n";
    std::cout << "  F <key>     - Find a key\n";
    std::cout << "  N <count>   - Fill with N sequential keys\n";
    std::cout << "  L           - List all keys\n";
    std::cout << "  .           - Go to first key\n";
    std::cout << "  +           - Go to next key\n";
    std::cout << "  -           - Go to previous key\n";
    std::cout << "  T           - Show current key\n";
    std::cout << "  R           - Remove current key\n";
    std::cout << "  C <num>     - Change active index (1-5)\n";
    std::cout << "  S           - Show statistics\n";
    std::cout << "  H           - Show this help\n";
    std::cout << "  X           - Exit\n\n";
}

/**
 * @brief Interactive command processor
 */
void runCommander()
{
    std::unique_ptr<MultiIndex> index;
    std::string command;
    std::string parameter;
    int64_t counter = 1;
    const std::string indexFile = "test_index.ndx";

    std::cout << "Enter 'C' to create new index or 'O' to open existing: ";
    std::getline(std::cin, command);

    try {
        if (!command.empty() && (command[0] == 'c' || command[0] == 'C')) {
            // Remove existing file
            std::filesystem::remove(indexFile);

            // Create new index file with 5 indexes
            index = std::make_unique<MultiIndex>(indexFile, 5);

            // Initialize each index with different configurations
            for (int i = 1; i <= 5; ++i) {
                index->setActiveIndex(i);
                uint16_t maxItems = 3 + (i * 2);  // 5, 7, 9, 11, 13
                index->initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE,
                    maxItems, 50, 150);
            }

            index->setActiveIndex(1);
            std::cout << "Created new index file with 5 indexes.\n";
        }
        else {
            // Open existing file
            index = std::make_unique<MultiIndex>(indexFile);
            std::cout << "Opened existing index file.\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return;
    }

    printHelp();

    while (true) {
        std::cout << "\n[Index " << index->getActiveIndex()
            << ", Error: " << static_cast<int>(index->getError()) << "] > ";

        std::getline(std::cin, command);
        if (command.empty()) continue;

        char cmd = std::toupper(command[0]);

        // Extract parameter if present
        parameter.clear();
        if (command.length() > 2) {
            parameter = command.substr(2);
        }

        try {
            switch (cmd) {
            case 'H':
                printHelp();
                break;

            case 'L': {
                // List all keys
                char key[51] = { 0 };
                int64_t dataPos = index->getFirst(key);

                if (dataPos != -1 && !index->isEOF()) {
                    std::cout << "\nKeys: ";
                    int count = 0;
                    do {
                        std::cout << key;
                        dataPos = index->getNext(key);
                        if (dataPos != -1 && !index->isEOF()) {
                            std::cout << ", ";
                        }
                        ++count;
                        if (count % 10 == 0) {
                            std::cout << "\n      ";
                        }
                    } while (dataPos != -1 && !index->isEOF());
                    std::cout << "\n(" << count << " keys total)\n";
                }
                else {
                    std::cout << "\nIndex is empty.\n";
                }
                break;
            }

            case 'C': {
                // Change active index
                int num = std::stoi(parameter);
                if (num >= 1 && num <= 5) {
                    index->setActiveIndex(num);
                    std::cout << "Switched to index " << num << "\n";
                }
                else {
                    std::cout << "Invalid index number. Use 1-5.\n";
                }
                break;
            }

            case 'R': {
                // Remove current
                int64_t dataPos = index->deleteCurrent();
                if (dataPos != -1) {
                    std::cout << "Removed current key (data pos was: " << dataPos << ")\n";
                }
                else {
                    std::cout << "No current key to remove.\n";
                }
                break;
            }

            case 'N': {
                // Fill with N keys
                int64_t num = std::stoll(parameter);

                auto start = std::chrono::high_resolution_clock::now();

                for (int64_t i = 1; i <= num; ++i) {
                    std::string key = std::to_string(i);
                    index->append(key.c_str(), i);

                    if (i % 1000 == 0) {
                        std::cout << "\r" << i << " keys added..." << std::flush;
                    }
                }

                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

                std::cout << "\rAdded " << num << " keys in "
                    << duration.count() << " ms ("
                    << std::fixed << std::setprecision(0)
                    << (num * 1000.0 / duration.count()) << " keys/sec)\n";
                break;
            }

            case 'A': {
                // Append key
                if (parameter.empty()) {
                    std::cout << "Usage: A <key>\n";
                    break;
                }
                if (index->append(parameter.c_str(), counter)) {
                    std::cout << "Added key '" << parameter << "' at position " << counter << "\n";
                    ++counter;
                }
                else {
                    std::cout << "Failed to add key.\n";
                }
                break;
            }

            case 'D': {
                // Delete key
                if (parameter.empty()) {
                    std::cout << "Usage: D <key>\n";
                    break;
                }
                if (index->deleteKey(parameter.c_str())) {
                    std::cout << "Deleted key '" << parameter << "'\n";
                }
                else {
                    std::cout << "Key not found or delete failed.\n";
                }
                break;
            }

            case 'F': {
                // Find key
                if (parameter.empty()) {
                    std::cout << "Usage: F <key>\n";
                    break;
                }
                int64_t dataPos = index->find(parameter.c_str());
                if (dataPos != -1) {
                    std::cout << "Found key '" << parameter << "' at data position " << dataPos << "\n";
                }
                else {
                    std::cout << "Key '" << parameter << "' not found.\n";
                }
                break;
            }

            case 'T': {
                // Current key
                char key[51] = { 0 };
                int64_t dataPos = index->getCurrent(key);
                if (dataPos != -1) {
                    std::cout << "Current: '" << key << "' at position " << dataPos << "\n";
                }
                else {
                    std::cout << "No current key.\n";
                }
                break;
            }

            case '.': {
                // First key
                char key[51] = { 0 };
                int64_t dataPos = index->getFirst(key);
                if (dataPos != -1) {
                    std::cout << "First: '" << key << "' at position " << dataPos << "\n";
                }
                else {
                    std::cout << "Index is empty.\n";
                }
                break;
            }

            case '+': {
                // Next key
                if (index->isEOF()) {
                    std::cout << "At end of index.\n";
                }
                else {
                    char key[51] = { 0 };
                    int64_t dataPos = index->getNext(key);
                    if (dataPos != -1) {
                        std::cout << "Next: '" << key << "' at position " << dataPos << "\n";
                    }
                    else {
                        std::cout << "No more keys.\n";
                    }
                }
                break;
            }

            case '-': {
                // Previous key
                if (index->isBOF()) {
                    std::cout << "At beginning of index.\n";
                }
                else {
                    char key[51] = { 0 };
                    int64_t dataPos = index->getPrev(key);
                    if (dataPos != -1) {
                        std::cout << "Previous: '" << key << "' at position " << dataPos << "\n";
                    }
                    else {
                        std::cout << "No previous key.\n";
                    }
                }
                break;
            }

            case 'S': {
                // Statistics
                std::cout << "\n=== Index Statistics ===\n";
                std::cout << "Library: " << getLibraryName() << " v" << getVersion() << "\n";
                std::cout << "File: " << indexFile << "\n";
                std::cout << "Number of indexes: " << index->getNumIndexes() << "\n";
                std::cout << "Active index: " << index->getActiveIndex() << "\n";
                std::cout << "Key type: " << static_cast<int>(index->getKeyType()) << " (STRING=5)\n";
                std::cout << "Key size: " << index->getKeySize() << " bytes\n";
                std::cout << "Delete allowed: " << (index->canDelete() ? "Yes" : "No") << "\n";
                std::cout << "Unique keys: " << (index->isUnique() ? "Yes" : "No") << "\n";
                std::cout << "EOF: " << (index->isEOF() ? "Yes" : "No") << "\n";
                std::cout << "BOF: " << (index->isBOF() ? "Yes" : "No") << "\n";
                break;
            }

            case 'X':
                std::cout << "Exiting...\n";
                return;

            default:
                std::cout << "Unknown command. Type 'H' for help.\n";
                break;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
}

int main()
{
    printHeader();

    try {
        runCommander();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
